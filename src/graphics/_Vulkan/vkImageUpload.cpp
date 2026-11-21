// vkImageUpload.cpp — sIMAGE::DataFill 的持久上传缓存
//
// 职责：复用一条 host-visible staging buffer、一个专用 command buffer 与 fence，
// 避免每次 DataFill 都 CreateBuf / AllocateCommandBuffers / CreateFence / Destroy。
// 仍保留 submit+wait（同步语义不变），只去掉驱动对象分配开销。
#include <GN.h>
#include "GN_Vk.h"
#include "vkInternal.h"

#include <cstring>

namespace GNg {

namespace {

VkBuffer s_staging = VK_NULL_HANDLE;
VkDeviceMemory s_stagingMem = VK_NULL_HANDLE;
VkDeviceSize s_stagingSize = 0;
unsigned char *s_stagingMapped = nullptr;
VkCommandPool s_pool = VK_NULL_HANDLE;
VkCommandBuffer s_cmd = VK_NULL_HANDLE;
VkFence s_fence = VK_NULL_HANDLE;

bool ensurePersistentObjects() {
    if (s_pool == VK_NULL_HANDLE) {
        VkCommandPoolCreateInfo cpci = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        cpci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        cpci.queueFamilyIndex = GN_Vk.graphicsFamily;
        if (vkCreateCommandPool(GN_Vk.device, &cpci, nullptr, &s_pool) != VK_SUCCESS) {
            s_pool = VK_NULL_HANDLE;
            return false;
        }
    }
    if (s_cmd == VK_NULL_HANDLE) {
        VkCommandBufferAllocateInfo ai = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        ai.commandPool = s_pool;
        ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        ai.commandBufferCount = 1;
        if (vkAllocateCommandBuffers(GN_Vk.device, &ai, &s_cmd) != VK_SUCCESS) {
            s_cmd = VK_NULL_HANDLE;
            return false;
        }
    }
    if (s_fence == VK_NULL_HANDLE) {
        VkFenceCreateInfo fci = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        if (vkCreateFence(GN_Vk.device, &fci, nullptr, &s_fence) != VK_SUCCESS) {
            s_fence = VK_NULL_HANDLE;
            return false;
        }
    }
    return true;
}

bool ensureStaging(VkDeviceSize bytes) {
    if (s_staging != VK_NULL_HANDLE && s_stagingSize >= bytes) return true;
    if (s_stagingMapped) {
        vkUnmapMemory(GN_Vk.device, s_stagingMem);
        s_stagingMapped = nullptr;
    }
    if (s_staging != VK_NULL_HANDLE) {
        vkDestroyBuffer(GN_Vk.device, s_staging, nullptr);
        s_staging = VK_NULL_HANDLE;
    }
    if (s_stagingMem != VK_NULL_HANDLE) {
        vkFreeMemory(GN_Vk.device, s_stagingMem, nullptr);
        s_stagingMem = VK_NULL_HANDLE;
    }
    s_stagingSize = 0;
    CreateBuf(bytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
              s_staging, s_stagingMem, true);
    if (s_staging == VK_NULL_HANDLE || s_stagingMem == VK_NULL_HANDLE) return false;
    void *mapped = nullptr;
    if (vkMapMemory(GN_Vk.device, s_stagingMem, 0, bytes, 0, &mapped) != VK_SUCCESS) {
        return false;
    }
    s_stagingMapped = static_cast<unsigned char *>(mapped);
    s_stagingSize = bytes;
    return true;
}

} // namespace

unsigned char *BeginImageUpload(VkDeviceSize bytes) {
    if (!GN_Vk.initialized || bytes == 0) return nullptr;
    if (!ensurePersistentObjects() || !ensureStaging(bytes)) return nullptr;
    return s_stagingMapped;
}

bool EndImageUpload(VkImage image, uint32_t width, uint32_t height, int32_t offsetX,
                    int32_t offsetY, bool fromUndefined) {
    if (!GN_Vk.initialized || image == VK_NULL_HANDLE || s_cmd == VK_NULL_HANDLE ||
        s_fence == VK_NULL_HANDLE || s_staging == VK_NULL_HANDLE) {
        return false;
    }
    if (vkResetCommandBuffer(s_cmd, 0) != VK_SUCCESS) return false;
    VkCommandBufferBeginInfo bi = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (vkBeginCommandBuffer(s_cmd, &bi) != VK_SUCCESS) return false;

    VkImageMemoryBarrier toDst = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    toDst.oldLayout = fromUndefined ? VK_IMAGE_LAYOUT_UNDEFINED
                                    : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    toDst.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toDst.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toDst.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toDst.image = image;
    toDst.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    toDst.subresourceRange.levelCount = 1;
    toDst.subresourceRange.layerCount = 1;
    toDst.srcAccessMask = fromUndefined ? 0 : VK_ACCESS_SHADER_READ_BIT;
    toDst.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(s_cmd,
                         fromUndefined ? VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT
                                       : VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toDst);

    VkBufferImageCopy region = {};
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {offsetX, offsetY, 0};
    region.imageExtent = {width, height, 1};
    vkCmdCopyBufferToImage(s_cmd, s_staging, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                           &region);

    VkImageMemoryBarrier toShader = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    toShader.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toShader.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    toShader.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toShader.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toShader.image = image;
    toShader.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    toShader.subresourceRange.levelCount = 1;
    toShader.subresourceRange.layerCount = 1;
    toShader.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    toShader.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(s_cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1,
                         &toShader);
    if (vkEndCommandBuffer(s_cmd) != VK_SUCCESS) return false;

    VkSubmitInfo si = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
    si.commandBufferCount = 1;
    si.pCommandBuffers = &s_cmd;
    if (vkResetFences(GN_Vk.device, 1, &s_fence) != VK_SUCCESS) return false;
    if (vkQueueSubmit(GN_Vk.graphicsQueue, 1, &si, s_fence) != VK_SUCCESS) return false;
    return vkWaitForFences(GN_Vk.device, 1, &s_fence, VK_TRUE, UINT64_MAX) == VK_SUCCESS;
}

void DestroyImageUploadCache() {
    if (!GN_Vk.initialized) return;
    if (s_stagingMapped) {
        vkUnmapMemory(GN_Vk.device, s_stagingMem);
        s_stagingMapped = nullptr;
    }
    if (s_fence != VK_NULL_HANDLE) {
        vkDestroyFence(GN_Vk.device, s_fence, nullptr);
        s_fence = VK_NULL_HANDLE;
    }
    if (s_pool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(GN_Vk.device, s_pool, nullptr);
        s_pool = VK_NULL_HANDLE;
        s_cmd = VK_NULL_HANDLE;
    }
    if (s_staging != VK_NULL_HANDLE) {
        vkDestroyBuffer(GN_Vk.device, s_staging, nullptr);
        s_staging = VK_NULL_HANDLE;
    }
    if (s_stagingMem != VK_NULL_HANDLE) {
        vkFreeMemory(GN_Vk.device, s_stagingMem, nullptr);
        s_stagingMem = VK_NULL_HANDLE;
    }
    s_stagingSize = 0;
}

} // namespace GNg
