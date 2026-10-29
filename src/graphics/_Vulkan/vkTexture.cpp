// vkTexture.cpp — 把 RGBA8 像素上传为可采样的 VkImage，并按 glTF sampler 参数建 VkSampler。
//
// 契约（P1c 纹理 v1）：
//   * 输入为 GN3d::sTEXTURE_IMAGE 解出的 RGBA8（逐行 top-down）；格式固定 R8G8B8A8_SRGB，
//     glTF 的 baseColor 贴图按 sRGB 解释，采样后得到线性值。
//   * 一次性命令缓冲完成 staging → 布局转换 → 拷贝 → SHADER_READ_ONLY_OPTIMAL，提交后等队列空闲；
//     只在资产加载期调用，不进入每帧热路径。
//   * 采样器映射 glTF 枚举：wrap 10497→REPEAT / 33071→CLAMP_TO_EDGE / 33648→MIRRORED_REPEAT；
//     filter 9728→NEAREST，其余（含 mipmap 变体）→ LINEAR（v1 不生成 mipmap）。
//   * 失败时释放已建资源并返回 false；GN_VkDestroyTexture 可重复调用。
#include <GN.h>
#include "GN_Vk.h"
#include "GN_3Dtexture.h"
#include "vkInternal.h"

#include <cstring>

namespace GNg {

namespace {

VkSamplerAddressMode toAddressMode(int32_t wrap) {
    if(wrap == 33071) return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    if(wrap == 33648) return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    return VK_SAMPLER_ADDRESS_MODE_REPEAT;
}

VkFilter toFilter(int32_t filter) {
    if(filter == 9728 || filter == 9984 || filter == 9986) return VK_FILTER_NEAREST;
    return VK_FILTER_LINEAR;
}

// 一条命令完成：未定义布局 → TRANSFER_DST → 拷贝 → SHADER_READ_ONLY。
bool uploadPixels(VkImage image, const unsigned char *pixels, uint32_t width, uint32_t height) {
    const VkDeviceSize byteSize = (VkDeviceSize)width * (VkDeviceSize)height * 4u;
    VkBuffer staging = VK_NULL_HANDLE;
    VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
    CreateBuf(byteSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging,
              stagingMemory);
    if(staging == VK_NULL_HANDLE || stagingMemory == VK_NULL_HANDLE) return false;

    void *mapped = 0;
    if(vkMapMemory(GN_Vk.device, stagingMemory, 0, byteSize, 0, &mapped) != VK_SUCCESS ||
       mapped == 0) {
        vkDestroyBuffer(GN_Vk.device, staging, 0);
        vkFreeMemory(GN_Vk.device, stagingMemory, 0);
        return false;
    }
    std::memcpy(mapped, pixels, (size_t)byteSize);
    vkUnmapMemory(GN_Vk.device, stagingMemory);

    VkCommandBufferAllocateInfo allocInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    allocInfo.commandPool = GN_Vk.commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;
    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    if(vkAllocateCommandBuffers(GN_Vk.device, &allocInfo, &commandBuffer) != VK_SUCCESS) {
        vkDestroyBuffer(GN_Vk.device, staging, 0);
        vkFreeMemory(GN_Vk.device, stagingMemory, 0);
        return false;
    }

    VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    VkImageMemoryBarrier barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, 0, 0, 0, 1, &barrier);

    VkBufferImageCopy region = {};
    region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.imageExtent = {width, height, 1};
    vkCmdCopyBufferToImage(commandBuffer, staging, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                           &region);

    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, 0, 0, 0, 1, &barrier);
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    const bool submitted = vkQueueSubmit(GN_Vk.graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE) ==
                           VK_SUCCESS;
    if(submitted) vkQueueWaitIdle(GN_Vk.graphicsQueue);
    vkFreeCommandBuffers(GN_Vk.device, GN_Vk.commandPool, 1, &commandBuffer);
    vkDestroyBuffer(GN_Vk.device, staging, 0);
    vkFreeMemory(GN_Vk.device, stagingMemory, 0);
    return submitted;
}

} // namespace

sTEXTURE_GPU::sTEXTURE_GPU()
    : aImage(VK_NULL_HANDLE), aMemory(VK_NULL_HANDLE), aView(VK_NULL_HANDLE),
      aSampler(VK_NULL_HANDLE), aWidth(0), aHeight(0) {
}

bool GN_VkCreateTextureRGBA8(const unsigned char *pixels, uint32_t width, uint32_t height,
                             const GN3d::sTEXTURE_SAMPLER &sampler, sTEXTURE_GPU &out) {
    if(!GN_Vk.initialized || pixels == 0 || width == 0 || height == 0) return false;
    if(out.aImage != VK_NULL_HANDLE) return false; // 已占用：要求调用方先 Destroy

    VkImageCreateInfo imageInfo = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent = {width, height, 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if(vkCreateImage(GN_Vk.device, &imageInfo, 0, &out.aImage) != VK_SUCCESS) {
        out.aImage = VK_NULL_HANDLE;
        return false;
    }

    VkMemoryRequirements requirements;
    vkGetImageMemoryRequirements(GN_Vk.device, out.aImage, &requirements);
    VkMemoryAllocateInfo memoryInfo = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    memoryInfo.allocationSize = requirements.size;
    memoryInfo.memoryTypeIndex = FindMemType(requirements.memoryTypeBits,
                                             VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if(vkAllocateMemory(GN_Vk.device, &memoryInfo, 0, &out.aMemory) != VK_SUCCESS) {
        GN_VkDestroyTexture(out);
        return false;
    }
    if(vkBindImageMemory(GN_Vk.device, out.aImage, out.aMemory, 0) != VK_SUCCESS) {
        GN_VkDestroyTexture(out);
        return false;
    }
    if(!uploadPixels(out.aImage, pixels, width, height)) {
        GN_VkDestroyTexture(out);
        return false;
    }

    VkImageViewCreateInfo viewInfo = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    viewInfo.image = out.aImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
    viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    if(vkCreateImageView(GN_Vk.device, &viewInfo, 0, &out.aView) != VK_SUCCESS) {
        GN_VkDestroyTexture(out);
        return false;
    }

    VkSamplerCreateInfo samplerInfo = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    samplerInfo.magFilter = toFilter(sampler.aMagFilter);
    samplerInfo.minFilter = toFilter(sampler.aMinFilter);
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.addressModeU = toAddressMode(sampler.aWrapS);
    samplerInfo.addressModeV = toAddressMode(sampler.aWrapT);
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f; // v1 只有一级，禁止采样 mip
    if(vkCreateSampler(GN_Vk.device, &samplerInfo, 0, &out.aSampler) != VK_SUCCESS) {
        GN_VkDestroyTexture(out);
        return false;
    }

    out.aWidth = width;
    out.aHeight = height;
    return true;
}

void GN_VkDestroyTexture(sTEXTURE_GPU &texture) {
    if(GN_Vk.device != VK_NULL_HANDLE) {
        if(texture.aSampler != VK_NULL_HANDLE) {
            vkDestroySampler(GN_Vk.device, texture.aSampler, 0);
        }
        if(texture.aView != VK_NULL_HANDLE) {
            vkDestroyImageView(GN_Vk.device, texture.aView, 0);
        }
        if(texture.aImage != VK_NULL_HANDLE) {
            vkDestroyImage(GN_Vk.device, texture.aImage, 0);
        }
        if(texture.aMemory != VK_NULL_HANDLE) {
            vkFreeMemory(GN_Vk.device, texture.aMemory, 0);
        }
    }
    texture.aSampler = VK_NULL_HANDLE;
    texture.aView = VK_NULL_HANDLE;
    texture.aImage = VK_NULL_HANDLE;
    texture.aMemory = VK_NULL_HANDLE;
    texture.aWidth = 0;
    texture.aHeight = 0;
}

} // namespace GNg
