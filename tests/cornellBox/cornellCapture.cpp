// tests/cornellBox/cornellCapture.cpp — 引擎 swapchain 两阶段截图（C2 对拍用）
//
// 为什么两阶段：swapchain 镜像在帧中仍被「正在录制」的帧命令缓冲引用，
// 若在帧中另起命令缓冲直接读回，读到的是上一帧/未定义内容（实测品红条纹）。
// 正确做法：在 composite 之后、HUD overlay 之前，把「swapchain→staging」的拷贝
// 录进当前帧命令缓冲；下一帧帧首（引擎已 wait inFlightFence）再把 staging 写 PNG。
#include "cornellCapture.h"

#include <GN.h>

#include <png.h>

#include <cmath>
#include <cstdint>
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
    bool aHdr;    // RGBA16F（ACES+gamma）
    bool aNormal; // RGBA16F（法线编码，原样 RGB，不 tonemap）
    bool aAlbedo; // RGBA8_SRGB 原样
    bool aSwap;   // swapchain RGBA8，需 BGR 交换
    VkBuffer aBuffer;
    VkDeviceMemory aMemory;
    uint32_t aWidth;
    uint32_t aHeight;
    std::string aPath;
};

sPendingCapture s_pending = {false, false, false, false, false, VK_NULL_HANDLE,
                               VK_NULL_HANDLE, 0u, 0u, {}};

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


// 半精度 float → float（诊断 HDR dump 用）。
float halfToFloat(uint16_t h) {
    const uint32_t sign = static_cast<uint32_t>(h & 0x8000u) << 16;
    uint32_t exp = (h >> 10) & 0x1Fu;
    uint32_t mant = h & 0x3FFu;
    uint32_t bits;
    if (exp == 0) {
        if (mant == 0) {
            bits = sign;
        } else {
            exp = 1;
            while ((mant & 0x400u) == 0) {
                mant <<= 1;
                --exp;
            }
            mant &= 0x3FFu;
            bits = sign | ((exp + 112u) << 23) | (mant << 13);
        }
    } else if (exp == 0x1Fu) {
        bits = sign | 0x7F800000u | (mant << 13);
    } else {
        bits = sign | ((exp + 112u) << 23) | (mant << 13);
    }
    float f;
    std::memcpy(&f, &bits, sizeof(f));
    return f;
}

// 与 PT/合成一致的 ACES fit + gamma 2.2。
float tonemapAcesGamma(float v) {
    v = (v * (2.51f * v + 0.03f)) / (v * (2.43f * v + 0.59f) + 0.14f);
    if (v < 0.0f) v = 0.0f;
    if (v > 1.0f) v = 1.0f;
    return std::pow(v, 1.0f / 2.2f);
}

void destroyPending() {
    if (s_pending.aBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(GNg::GN_Vk.device, s_pending.aBuffer, nullptr);
    }
    if (s_pending.aMemory != VK_NULL_HANDLE) {
        vkFreeMemory(GNg::GN_Vk.device, s_pending.aMemory, nullptr);
    }
    s_pending = sPendingCapture{false, false, false, false, false, VK_NULL_HANDLE,
                                 VK_NULL_HANDLE, 0u, 0u, {}};
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
    s_pending.aSwap = true;
    s_pending.aBuffer = buffer;
    s_pending.aMemory = memory;
    s_pending.aWidth = width;
    s_pending.aHeight = height;
    s_pending.aPath = path;
    return true;
}


bool requestHdrCapture(const char *path) {
    if (!path || s_pending.aPending || !GNg::GN_Vk.initialized) return false;
    if (GNg::GN_Vk.hdrImage == VK_NULL_HANDLE) return false;
    const uint32_t width = GNg::GN_Vk.swapchainExtent.width;
    const uint32_t height = GNg::GN_Vk.swapchainExtent.height;
    if (width == 0u || height == 0u) return false;
    const VkDeviceSize size = static_cast<VkDeviceSize>(width) * height * 8u;
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    if (!createStagingBuffer(size, &buffer, &memory)) return false;

    VkCommandBuffer cb = GNg::GN_Vk.commandBuffers[GNg::GN_Vk.currentFrame];
    VkImage image = GNg::GN_Vk.hdrImage;
    // HDR 在 composite 后被采样（finalLayout SHADER_READ_ONLY），这里转 TRANSFER_SRC 拷贝。
    VkImageMemoryBarrier toTransfer = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    toTransfer.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    toTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTransfer.image = image;
    toTransfer.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    toTransfer.subresourceRange.levelCount = 1;
    toTransfer.subresourceRange.layerCount = 1;
    toTransfer.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    toTransfer.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1,
                         &toTransfer);
    VkBufferImageCopy region = {};
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.layerCount = 1;
    region.imageExtent = {width, height, 1};
    vkCmdCopyImageToBuffer(cb, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buffer, 1, &region);
    VkImageMemoryBarrier toRead = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    toRead.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    toRead.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    toRead.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toRead.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toRead.image = image;
    toRead.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    toRead.subresourceRange.levelCount = 1;
    toRead.subresourceRange.layerCount = 1;
    toRead.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    toRead.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1,
                         &toRead);

    s_pending.aPending = true;
    s_pending.aHdr = true;
    s_pending.aBuffer = buffer;
    s_pending.aMemory = memory;
    s_pending.aWidth = width;
    s_pending.aHeight = height;
    s_pending.aPath = path;
    return true;
}

// 直抓某个 SHADER_READ_ONLY 布局的 RGBA8 图（albedo 附件等），原样写 PNG，不经 tonemap。
static bool requestReadonlyImageCapture(const char *path, VkImage image) {
    if (!path || s_pending.aPending || !GNg::GN_Vk.initialized || image == VK_NULL_HANDLE) {
        return false;
    }
    const uint32_t width = GNg::GN_Vk.swapchainExtent.width;
    const uint32_t height = GNg::GN_Vk.swapchainExtent.height;
    if (width == 0u || height == 0u) return false;
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    if (!createStagingBuffer(static_cast<VkDeviceSize>(width) * height * 4u, &buffer, &memory)) {
        return false;
    }
    VkCommandBuffer cb = GNg::GN_Vk.commandBuffers[GNg::GN_Vk.currentFrame];
    VkImageMemoryBarrier toT = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    toT.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    toT.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    toT.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toT.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toT.image = image;
    toT.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    toT.subresourceRange.levelCount = 1;
    toT.subresourceRange.layerCount = 1;
    toT.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    toT.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toT);
    VkBufferImageCopy region = {};
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.layerCount = 1;
    region.imageExtent = {width, height, 1};
    vkCmdCopyImageToBuffer(cb, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buffer, 1, &region);
    VkImageMemoryBarrier toR = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    toR.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    toR.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    toR.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toR.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toR.image = image;
    toR.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    toR.subresourceRange.levelCount = 1;
    toR.subresourceRange.layerCount = 1;
    toR.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    toR.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1,
                         &toR);
    s_pending.aPending = true;
    s_pending.aAlbedo = true;
    s_pending.aBuffer = buffer;
    s_pending.aMemory = memory;
    s_pending.aWidth = width;
    s_pending.aHeight = height;
    s_pending.aPath = path;
    return true;
}

bool requestAlbedoCapture(const char *path) {
    return requestReadonlyImageCapture(path, GNg::GN_Vk.gAlbedoMetalImage);
}

// 直抓 RGBA16F 的 G-Buffer 法线附件（复用 requestHdrCapture 的拷贝逻辑，但 8 字节/像素）。
bool requestNormalCapture(const char *path) {
    if (!path || s_pending.aPending || !GNg::GN_Vk.initialized) return false;
    if (GNg::GN_Vk.gNormalRoughImage == VK_NULL_HANDLE) return false;
    const uint32_t width = GNg::GN_Vk.swapchainExtent.width;
    const uint32_t height = GNg::GN_Vk.swapchainExtent.height;
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    if (!createStagingBuffer(static_cast<VkDeviceSize>(width) * height * 8u, &buffer, &memory)) {
        return false;
    }
    VkCommandBuffer cb = GNg::GN_Vk.commandBuffers[GNg::GN_Vk.currentFrame];
    VkImage image = GNg::GN_Vk.gNormalRoughImage;
    VkImageMemoryBarrier toT = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    toT.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    toT.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    toT.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toT.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toT.image = image;
    toT.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    toT.subresourceRange.levelCount = 1;
    toT.subresourceRange.layerCount = 1;
    toT.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    toT.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toT);
    VkBufferImageCopy region = {};
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.layerCount = 1;
    region.imageExtent = {width, height, 1};
    vkCmdCopyImageToBuffer(cb, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buffer, 1, &region);
    VkImageMemoryBarrier toR = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    toR.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    toR.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    toR.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toR.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toR.image = image;
    toR.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    toR.subresourceRange.levelCount = 1;
    toR.subresourceRange.layerCount = 1;
    toR.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    toR.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1,
                         &toR);
    s_pending.aPending = true;
    s_pending.aNormal = true;
    s_pending.aBuffer = buffer;
    s_pending.aMemory = memory;
    s_pending.aWidth = width;
    s_pending.aHeight = height;
    s_pending.aPath = path;
    return true;
}

bool finishPendingCapture() {
    if (!s_pending.aPending) return false;
    const uint32_t w = s_pending.aWidth;
    const uint32_t h = s_pending.aHeight;
    const VkDeviceSize bytesPerPixel = (s_pending.aHdr || s_pending.aNormal) ? 8u : 4u;
    const VkDeviceSize size = static_cast<VkDeviceSize>(w) * h * bytesPerPixel;
    void *mapped = nullptr;
    if (vkMapMemory(GNg::GN_Vk.device, s_pending.aMemory, 0, size, 0, &mapped) != VK_SUCCESS) {
        destroyPending();
        return false;
    }
    std::vector<uint8_t> raw(static_cast<size_t>(size));
    std::memcpy(raw.data(), mapped, static_cast<size_t>(size));
    vkUnmapMemory(GNg::GN_Vk.device, s_pending.aMemory);

    std::vector<uint8_t> rgba(static_cast<size_t>(w) * h * 4u);
    if (s_pending.aAlbedo) {
        std::memcpy(rgba.data(), raw.data(), raw.size());
    } else if (s_pending.aNormal) {
        const uint16_t *halfs = reinterpret_cast<const uint16_t *>(raw.data());
        for (uint32_t i = 0; i < w * h; ++i) {
            rgba[i * 4 + 0] = static_cast<uint8_t>(std::min(1.0f, std::max(0.0f, halfToFloat(halfs[i * 4 + 0]))) * 255.0f + 0.5f);
            rgba[i * 4 + 1] = static_cast<uint8_t>(std::min(1.0f, std::max(0.0f, halfToFloat(halfs[i * 4 + 1]))) * 255.0f + 0.5f);
            rgba[i * 4 + 2] = static_cast<uint8_t>(std::min(1.0f, std::max(0.0f, halfToFloat(halfs[i * 4 + 2]))) * 255.0f + 0.5f);
            rgba[i * 4 + 3] = 255u;
        }
    } else if (s_pending.aHdr) {
        const uint16_t *halfs = reinterpret_cast<const uint16_t *>(raw.data());
        for (uint32_t i = 0; i < w * h; ++i) {
            const float r = tonemapAcesGamma(halfToFloat(halfs[i * 4 + 0]));
            const float g = tonemapAcesGamma(halfToFloat(halfs[i * 4 + 1]));
            const float b = tonemapAcesGamma(halfToFloat(halfs[i * 4 + 2]));
            rgba[i * 4 + 0] = static_cast<uint8_t>(r * 255.0f + 0.5f);
            rgba[i * 4 + 1] = static_cast<uint8_t>(g * 255.0f + 0.5f);
            rgba[i * 4 + 2] = static_cast<uint8_t>(b * 255.0f + 0.5f);
            rgba[i * 4 + 3] = 255u;
        }
    } else {
        std::memcpy(rgba.data(), raw.data(), raw.size());
        if (!s_pending.aSwap) return false;
        const VkFormat format = GNg::GN_Vk.swapchainFormat;
        if (format == VK_FORMAT_B8G8R8A8_UNORM || format == VK_FORMAT_B8G8R8A8_SRGB) {
            for (uint32_t i = 0; i < w * h; ++i) {
                uint8_t tmp = rgba[i * 4 + 0];
                rgba[i * 4 + 0] = rgba[i * 4 + 2];
                rgba[i * 4 + 2] = tmp;
            }
        }
    }

    const bool ok = writeRgbPng(s_pending.aPath.c_str(), rgba.data(), static_cast<int>(w),
                                static_cast<int>(h));
    if (ok) {
        std::printf("[cornellBox] %s: %s\n",
                    s_pending.aAlbedo   ? "albedo 截图"
                    : (s_pending.aNormal ? "normal 截图" : (s_pending.aHdr ? "HDR 截图" : "引擎截图")),
                    s_pending.aPath.c_str());
    }
    destroyPending();
    return ok;
}

bool hasPendingCapture() {
    return s_pending.aPending;
}

} // namespace Cornell
