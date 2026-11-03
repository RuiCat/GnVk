// tests/cornellBox/cornellCapture.cpp — 引擎 swapchain 两阶段截图（C2 对拍用）
//
// 为什么两阶段：swapchain 镜像在帧中仍被「正在录制」的帧命令缓冲引用，
// 若在帧中另起命令缓冲直接读回，读到的是上一帧/未定义内容（实测品红条纹）。
// 正确做法：在 composite 之后、HUD overlay 之前，把「swapchain→staging」的拷贝
// 录进当前帧命令缓冲；下一帧帧首（引擎已 wait inFlightFence）再把 staging 写 PNG。
#include "cornellCapture.h"

#include <GN.h>

#include <png.h>

#include <csetjmp>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

namespace Cornell {

namespace {

bool makeDir(const char *path) {
#ifdef _WIN32
    return _mkdir(path) == 0;
#else
    return mkdir(path, 0755) == 0;
#endif
}

// 自己写 PNG（不走 GN_SavePngFile）：后者在 swapchain 读回时曾产出 0 字节文件。
bool writeRgbPng(const char *path, const uint8_t *rgba, int width, int height) {
    FILE *file = std::fopen(path, "wb");
    if (!file) {
        std::fprintf(stderr, "[cornellCapture] 无法创建 %s\n", path);
        return false;
    }
    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if (!png) {
        std::fclose(file);
        return false;
    }
    png_infop info = png_create_info_struct(png);
    if (!info) {
        png_destroy_write_struct(&png, nullptr);
        std::fclose(file);
        return false;
    }
    if (setjmp(png_jmpbuf(png))) {
        png_destroy_write_struct(&png, &info);
        std::fclose(file);
        return false;
    }
    png_init_io(png, file);
    png_set_IHDR(png, info, static_cast<png_uint_32>(width), static_cast<png_uint_32>(height),
                 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
                 PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png, info);

    std::vector<png_byte> row(static_cast<size_t>(width) * 3u);
    for (int y = 0; y < height; ++y) {
        const uint8_t *src = rgba + static_cast<size_t>(y) * width * 4u;
        for (int x = 0; x < width; ++x) {
            row[static_cast<size_t>(x) * 3u + 0] = src[x * 4 + 0];
            row[static_cast<size_t>(x) * 3u + 1] = src[x * 4 + 1];
            row[static_cast<size_t>(x) * 3u + 2] = src[x * 4 + 2];
        }
        png_write_row(png, row.data());
    }
    png_write_end(png, nullptr);
    png_destroy_write_struct(&png, &info);
    std::fclose(file);
    return true;
}

struct sPendingCapture {
    bool aPending;
    VkBuffer aBuffer;
    VkDeviceMemory aMemory;
    uint32_t aWidth;
    uint32_t aHeight;
    std::string aPath;
};

sPendingCapture s_pending = {false, VK_NULL_HANDLE, VK_NULL_HANDLE, 0u, 0u, {}};

uint32_t findHostVisibleMemory(VkPhysicalDevice physical, uint32_t typeBits) {
    VkPhysicalDeviceMemoryProperties props;
    vkGetPhysicalDeviceMemoryProperties(physical, &props);
    for (uint32_t i = 0; i < props.memoryTypeCount; ++i) {
        const VkMemoryPropertyFlags flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        if ((typeBits & (1u << i)) && (props.memoryTypes[i].propertyFlags & flags) == flags) {
            return i;
        }
    }
    return UINT32_MAX;
}

bool createStagingBuffer(VkDeviceSize size, VkBuffer *buffer, VkDeviceMemory *memory) {
    VkBufferCreateInfo bci = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bci.size = size;
    bci.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(GNg::GN_Vk.device, &bci, nullptr, buffer) != VK_SUCCESS) return false;
    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(GNg::GN_Vk.device, *buffer, &req);
    const uint32_t type = findHostVisibleMemory(GNg::GN_Vk.physicalDevice, req.memoryTypeBits);
    if (type == UINT32_MAX) return false;
    VkMemoryAllocateInfo mai = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    mai.allocationSize = req.size;
    mai.memoryTypeIndex = type;
    if (vkAllocateMemory(GNg::GN_Vk.device, &mai, nullptr, memory) != VK_SUCCESS) return false;
    return vkBindBufferMemory(GNg::GN_Vk.device, *buffer, *memory, 0) == VK_SUCCESS;
}

void destroyPending() {
    if (s_pending.aBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(GNg::GN_Vk.device, s_pending.aBuffer, nullptr);
    }
    if (s_pending.aMemory != VK_NULL_HANDLE) {
        vkFreeMemory(GNg::GN_Vk.device, s_pending.aMemory, nullptr);
    }
    s_pending = sPendingCapture{false, VK_NULL_HANDLE, VK_NULL_HANDLE, 0u, 0u, {}};
}

} // namespace

void ensureCaptureDir() {
    makeDir(".local");
    makeDir(".local/cornellbox");
}

bool requestSwapchainCapture(const char *path) {
    if (!path || s_pending.aPending || !GNg::GN_Vk.initialized) return false;
    const uint32_t width = GNg::GN_Vk.swapchainExtent.width;
    const uint32_t height = GNg::GN_Vk.swapchainExtent.height;
    if (width == 0u || height == 0u) return false;
    const VkDeviceSize size = static_cast<VkDeviceSize>(width) * height * 4u;
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    if (!createStagingBuffer(size, &buffer, &memory)) return false;

    VkCommandBuffer cb = GNg::GN_Vk.commandBuffers[GNg::GN_Vk.currentFrame];
    VkImage image = GNg::GN_Vk.swapchainImages[GNg::GN_Vk.currentFrame];
    // composite pass 结束时 swapchain 已是 PRESENT_SRC；HUD overlay pass 也从它开始。
    VkImageMemoryBarrier toTransfer = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    toTransfer.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    toTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTransfer.image = image;
    toTransfer.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    toTransfer.subresourceRange.levelCount = 1;
    toTransfer.subresourceRange.layerCount = 1;
    toTransfer.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    toTransfer.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1,
                         &toTransfer);

    VkBufferImageCopy region = {};
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.layerCount = 1;
    region.imageExtent = {width, height, 1};
    vkCmdCopyImageToBuffer(cb, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buffer, 1, &region);

    VkImageMemoryBarrier toPresent = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    toPresent.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    toPresent.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    toPresent.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toPresent.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toPresent.image = image;
    toPresent.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    toPresent.subresourceRange.levelCount = 1;
    toPresent.subresourceRange.layerCount = 1;
    toPresent.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    toPresent.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0,
                         nullptr, 1, &toPresent);

    s_pending.aPending = true;
    s_pending.aBuffer = buffer;
    s_pending.aMemory = memory;
    s_pending.aWidth = width;
    s_pending.aHeight = height;
    s_pending.aPath = path;
    return true;
}

bool finishPendingCapture() {
    if (!s_pending.aPending) return false;
    const VkDeviceSize size =
        static_cast<VkDeviceSize>(s_pending.aWidth) * s_pending.aHeight * 4u;
    void *mapped = nullptr;
    if (vkMapMemory(GNg::GN_Vk.device, s_pending.aMemory, 0, size, 0, &mapped) != VK_SUCCESS) {
        destroyPending();
        return false;
    }
    std::vector<uint8_t> rgba(static_cast<size_t>(size));
    std::memcpy(rgba.data(), mapped, static_cast<size_t>(size));
    vkUnmapMemory(GNg::GN_Vk.device, s_pending.aMemory);

    const VkFormat format = GNg::GN_Vk.swapchainFormat;
    if (format == VK_FORMAT_B8G8R8A8_UNORM || format == VK_FORMAT_B8G8R8A8_SRGB) {
        for (uint32_t i = 0; i < s_pending.aWidth * s_pending.aHeight; ++i) {
            uint8_t tmp = rgba[i * 4 + 0];
            rgba[i * 4 + 0] = rgba[i * 4 + 2];
            rgba[i * 4 + 2] = tmp;
        }
    }

    const bool ok = writeRgbPng(s_pending.aPath.c_str(), rgba.data(),
                                static_cast<int>(s_pending.aWidth),
                                static_cast<int>(s_pending.aHeight));
    if (ok) {
        std::printf("[cornellBox] 引擎截图: %s\n", s_pending.aPath.c_str());
    }
    destroyPending();
    return ok;
}

bool hasPendingCapture() {
    return s_pending.aPending;
}

} // namespace Cornell
