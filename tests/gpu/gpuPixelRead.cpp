// GPU 像素回读实现：只做图像布局转换 + 拷贝，不修改渲染结果。
#include "gpuPixelRead.h"

#include <GN.h>
#include "GN_Vk.h"

#include <cmath>
#include <cstring>

namespace GpuCheck {
namespace {

uint32_t FormatBytes(VkFormat format) {
    switch (format) {
    case VK_FORMAT_R16G16B16A16_SFLOAT: return 8;
    case VK_FORMAT_R8G8B8A8_UNORM:
    case VK_FORMAT_R8G8B8A8_SRGB:
    case VK_FORMAT_B8G8R8A8_UNORM:
    case VK_FORMAT_B8G8R8A8_SRGB: return 4;
    default: return 0;
    }
}

uint32_t FindMemoryTypeIndex(uint32_t typeBits, VkMemoryPropertyFlags props) {
    VkPhysicalDeviceMemoryProperties mp;
    vkGetPhysicalDeviceMemoryProperties(GNg::GN_Vk.physicalDevice, &mp);
    for(uint32_t i = 0; i < mp.memoryTypeCount; ++i) {
        if((typeBits & (1u << i)) && (mp.memoryTypes[i].propertyFlags & props) == props) return i;
    }
    return 0;
}

bool CreateStaging(VkDeviceSize size, VkBuffer *buffer, VkDeviceMemory *memory) {
    VkBufferCreateInfo bi = {};
    bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bi.size = size;
    bi.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if(vkCreateBuffer(GNg::GN_Vk.device, &bi, 0, buffer) != VK_SUCCESS) return false;
    VkMemoryRequirements mr;
    vkGetBufferMemoryRequirements(GNg::GN_Vk.device, *buffer, &mr);
    VkMemoryAllocateInfo ai = {};
    ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    ai.allocationSize = mr.size;
    ai.memoryTypeIndex = FindMemoryTypeIndex(mr.memoryTypeBits,
                                             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                             VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if(vkAllocateMemory(GNg::GN_Vk.device, &ai, 0, memory) != VK_SUCCESS) return false;
    vkBindBufferMemory(GNg::GN_Vk.device, *buffer, *memory, 0);
    return true;
}

void RecordBarrier(VkCommandBuffer cb, VkImage image, VkImageAspectFlags aspect,
                   VkImageLayout oldLayout, VkImageLayout newLayout) {
    VkImageMemoryBarrier b = {};
    b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    b.oldLayout = oldLayout;
    b.newLayout = newLayout;
    b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.image = image;
    b.subresourceRange.aspectMask = aspect;
    b.subresourceRange.levelCount = 1;
    b.subresourceRange.layerCount = 1;
    b.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    b.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0, 0, 0, 0, 0, 1, &b);
}

} // namespace

uint32_t SwapchainBytesPerPixel() {
    switch(GNg::GN_Vk.swapchainFormat) {
    case VK_FORMAT_R16G16B16A16_SFLOAT: return 8;
    case VK_FORMAT_B8G8R8A8_UNORM:
    case VK_FORMAT_B8G8R8A8_SRGB:
    case VK_FORMAT_R8G8B8A8_UNORM:
    case VK_FORMAT_R8G8B8A8_SRGB: return 4;
    default: return 0;
    }
}

sFLOAT_PIXEL SampleSwapchain(const std::vector<uint8_t> &pixels, int width, int x, int y) {
    sFLOAT_PIXEL out = {0.0f, 0.0f, 0.0f, 0.0f};
    const uint32_t bytes = SwapchainBytesPerPixel();
    if(bytes == 0 || width <= 0 || x < 0 || y < 0) return out;
    const size_t offset = ((size_t)y * (size_t)width + (size_t)x) * bytes;
    if(offset + bytes > pixels.size()) return out;

    if(bytes == 8) {
        uint16_t words[4] = {0, 0, 0, 0};
        std::memcpy(words, pixels.data() + offset, sizeof(words));
        out.r = HalfToFloat(words[0]);
        out.g = HalfToFloat(words[1]);
        out.b = HalfToFloat(words[2]);
        out.a = HalfToFloat(words[3]);
        return out;
    }

    const bool bgra = GNg::GN_Vk.swapchainFormat == VK_FORMAT_B8G8R8A8_UNORM ||
                      GNg::GN_Vk.swapchainFormat == VK_FORMAT_B8G8R8A8_SRGB;
    const bool srgb = GNg::GN_Vk.swapchainFormat == VK_FORMAT_B8G8R8A8_SRGB ||
                      GNg::GN_Vk.swapchainFormat == VK_FORMAT_R8G8B8A8_SRGB;
    const uint8_t c0 = pixels[offset + 0u];
    const uint8_t c1 = pixels[offset + 1u];
    const uint8_t c2 = pixels[offset + 2u];
    const uint8_t c3 = pixels[offset + 3u];
    const uint8_t rb = bgra ? c2 : c0;
    const uint8_t gb = c1;
    const uint8_t bb = bgra ? c0 : c2;
    const auto decode = [srgb](uint8_t value) -> float {
        const float c = (float)value / 255.0f;
        return srgb && c > 0.04045f ? std::pow((c + 0.055f) / 1.055f, 2.4f) : c;
    };
    out.r = decode(rb);
    out.g = decode(gb);
    out.b = decode(bb);
    out.a = (float)c3 / 255.0f;
    return out;
}

bool ReadSwapchain(int x, int y, int width, int height, std::vector<uint8_t> &out) {
    const uint32_t bytes = SwapchainBytesPerPixel();
    if(bytes == 0) return false;
    const uint32_t sw = GNg::GN_Vk.swapchainExtent.width;
    const uint32_t sh = GNg::GN_Vk.swapchainExtent.height;
    if(x < 0) {
        width += x;
        x = 0;
    }
    if(y < 0) {
        height += y;
        y = 0;
    }
    if(x + width > (int)sw) width = (int)sw - x;
    if(y + height > (int)sh) height = (int)sh - y;
    if(width <= 0 || height <= 0) return false;

    // 引擎 GN_VkReadBack 固定按 4 字节/像素分配，RGBA16F swapchain 会被截断；
    // 这里按实际格式自行拷贝。
    const VkImage image = GNg::GN_Vk.swapchainImages[GNg::GN_Vk.currentFrame];
    const VkDeviceSize size = (VkDeviceSize)width * (VkDeviceSize)height * bytes;
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    if(!CreateStaging(size, &buffer, &memory)) return false;

    VkCommandBufferAllocateInfo ai = {};
    ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    ai.commandPool = GNg::GN_Vk.commandPool;
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = 1;
    VkCommandBuffer cb = VK_NULL_HANDLE;
    if(vkAllocateCommandBuffers(GNg::GN_Vk.device, &ai, &cb) != VK_SUCCESS) return false;

    VkCommandBufferBeginInfo bi = {};
    bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cb, &bi);
    RecordBarrier(cb, image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                  VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    VkBufferImageCopy region = {};
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.layerCount = 1;
    region.imageOffset.x = x;
    region.imageOffset.y = y;
    region.imageExtent = {(uint32_t)width, (uint32_t)height, 1};
    vkCmdCopyImageToBuffer(cb, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buffer, 1, &region);
    RecordBarrier(cb, image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                  VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    vkEndCommandBuffer(cb);

    VkSubmitInfo si = {};
    si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cb;
    vkQueueSubmit(GNg::GN_Vk.graphicsQueue, 1, &si, VK_NULL_HANDLE);
    vkQueueWaitIdle(GNg::GN_Vk.graphicsQueue);

    void *mapped = 0;
    out.resize((size_t)size);
    if(vkMapMemory(GNg::GN_Vk.device, memory, 0, size, 0, &mapped) != VK_SUCCESS) {
        out.clear();
    } else {
        std::memcpy(out.data(), mapped, (size_t)size);
        vkUnmapMemory(GNg::GN_Vk.device, memory);
    }
    vkFreeCommandBuffers(GNg::GN_Vk.device, GNg::GN_Vk.commandPool, 1, &cb);
    vkDestroyBuffer(GNg::GN_Vk.device, buffer, 0);
    vkFreeMemory(GNg::GN_Vk.device, memory, 0);
    return !out.empty();
}

bool ReadColorImage(VkImage image, VkFormat format, VkImageLayout currentLayout,
                    uint32_t width, uint32_t height, std::vector<uint8_t> &out) {
    const uint32_t bytes = FormatBytes(format);
    if(!image || bytes == 0 || width == 0 || height == 0) return false;
    const VkDeviceSize size = (VkDeviceSize)width * height * bytes;
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    if(!CreateStaging(size, &buffer, &memory)) return false;

    VkCommandBufferAllocateInfo ai = {};
    ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    ai.commandPool = GNg::GN_Vk.commandPool;
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = 1;
    VkCommandBuffer cb = VK_NULL_HANDLE;
    if(vkAllocateCommandBuffers(GNg::GN_Vk.device, &ai, &cb) != VK_SUCCESS) return false;

    VkCommandBufferBeginInfo bi = {};
    bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cb, &bi);
    RecordBarrier(cb, image, VK_IMAGE_ASPECT_COLOR_BIT, currentLayout,
                  VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    VkBufferImageCopy region = {};
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.layerCount = 1;
    region.imageExtent = {width, height, 1};
    vkCmdCopyImageToBuffer(cb, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buffer, 1, &region);
    RecordBarrier(cb, image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                  currentLayout);
    vkEndCommandBuffer(cb);

    VkSubmitInfo si = {};
    si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cb;
    vkQueueSubmit(GNg::GN_Vk.graphicsQueue, 1, &si, VK_NULL_HANDLE);
    vkQueueWaitIdle(GNg::GN_Vk.graphicsQueue);

    void *mapped = 0;
    out.resize((size_t)size);
    if(vkMapMemory(GNg::GN_Vk.device, memory, 0, size, 0, &mapped) == VK_SUCCESS) {
        std::memcpy(out.data(), mapped, (size_t)size);
        vkUnmapMemory(GNg::GN_Vk.device, memory);
    }
    vkFreeCommandBuffers(GNg::GN_Vk.device, GNg::GN_Vk.commandPool, 1, &cb);
    vkDestroyBuffer(GNg::GN_Vk.device, buffer, 0);
    vkFreeMemory(GNg::GN_Vk.device, memory, 0);
    return true;
}

bool ReadDepthImage(VkImage image, VkImageLayout currentLayout, uint32_t width, uint32_t height,
                    std::vector<float> &out) {
    if(!image || width == 0 || height == 0) return false;
    const VkDeviceSize size = (VkDeviceSize)width * height * sizeof(float);
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    if(!CreateStaging(size, &buffer, &memory)) return false;

    VkCommandBufferAllocateInfo ai = {};
    ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    ai.commandPool = GNg::GN_Vk.commandPool;
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = 1;
    VkCommandBuffer cb = VK_NULL_HANDLE;
    if(vkAllocateCommandBuffers(GNg::GN_Vk.device, &ai, &cb) != VK_SUCCESS) return false;

    VkCommandBufferBeginInfo bi = {};
    bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cb, &bi);
    RecordBarrier(cb, image, VK_IMAGE_ASPECT_DEPTH_BIT, currentLayout,
                  VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    VkBufferImageCopy region = {};
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    region.imageSubresource.layerCount = 1;
    region.imageExtent = {width, height, 1};
    vkCmdCopyImageToBuffer(cb, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buffer, 1, &region);
    RecordBarrier(cb, image, VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                  currentLayout);
    vkEndCommandBuffer(cb);

    VkSubmitInfo si = {};
    si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cb;
    vkQueueSubmit(GNg::GN_Vk.graphicsQueue, 1, &si, VK_NULL_HANDLE);
    vkQueueWaitIdle(GNg::GN_Vk.graphicsQueue);

    void *mapped = 0;
    out.resize((size_t)width * height);
    if(vkMapMemory(GNg::GN_Vk.device, memory, 0, size, 0, &mapped) == VK_SUCCESS) {
        std::memcpy(out.data(), mapped, (size_t)size);
        vkUnmapMemory(GNg::GN_Vk.device, memory);
    }
    vkFreeCommandBuffers(GNg::GN_Vk.device, GNg::GN_Vk.commandPool, 1, &cb);
    vkDestroyBuffer(GNg::GN_Vk.device, buffer, 0);
    vkFreeMemory(GNg::GN_Vk.device, memory, 0);
    return true;
}

float HalfToFloat(uint16_t half) {
    const uint32_t sign = (half >> 15) & 0x1u;
    const uint32_t exponent = (half >> 10) & 0x1Fu;
    const uint32_t mantissa = half & 0x3FFu;
    float value = 0.0f;
    if(exponent == 0) {
        value = (float)mantissa / 1024.0f * 0.00006103515625f;
    } else if(exponent == 31) {
        value = mantissa ? 0.0f : 65504.0f;
    } else {
        const int exp = (int)exponent - 15;
        value = std::ldexp(1.0f + (float)mantissa / 1024.0f, exp);
    }
    return sign ? -value : value;
}

} // namespace GpuCheck
