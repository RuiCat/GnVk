// tests/cornellBox/cornellVkUtil.h — Cornell 光追/计算模块共用的 Vulkan 缓冲小工具
//
// 只做「选内存类型 + 建缓冲 + 可选映射 + 销毁」；被 cornellGpuRt.cpp 与
// cornellRtAs.cpp 共用，避免两份几乎相同的样板代码。
#ifndef CORNELL_VK_UTIL_H
#define CORNELL_VK_UTIL_H

#include <vulkan/vulkan.h>

#include <cstdint>

namespace Cornell {

// 一个可映射/不可映射的 Vulkan 缓冲及其内存。
struct sVkBuffer {
    VkBuffer aBuffer;
    VkDeviceMemory aMemory;
    void *aMapped;
};

inline uint32_t findMemoryType(VkPhysicalDevice physical, uint32_t typeBits,
                               VkMemoryPropertyFlags required) {
    VkPhysicalDeviceMemoryProperties props;
    vkGetPhysicalDeviceMemoryProperties(physical, &props);
    for (uint32_t i = 0; i < props.memoryTypeCount; ++i) {
        if ((typeBits & (1u << i)) &&
            (props.memoryTypes[i].propertyFlags & required) == required) {
            return i;
        }
    }
    return UINT32_MAX;
}

inline bool createVkBuffer(VkDevice device, VkPhysicalDevice physical, VkDeviceSize size,
                           VkBufferUsageFlags usage, VkMemoryPropertyFlags required,
                           bool mapMemory, sVkBuffer *out) {
    if (!out) return false;
    *out = sVkBuffer{VK_NULL_HANDLE, VK_NULL_HANDLE, nullptr};
    VkBufferCreateInfo bci = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bci.size = size;
    bci.usage = usage;
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(device, &bci, nullptr, &out->aBuffer) != VK_SUCCESS) return false;
    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(device, out->aBuffer, &req);
    const uint32_t type = findMemoryType(physical, req.memoryTypeBits, required);
    if (type == UINT32_MAX) return false;
    VkMemoryAllocateInfo mai = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    mai.allocationSize = req.size;
    mai.memoryTypeIndex = type;
    if (vkAllocateMemory(device, &mai, nullptr, &out->aMemory) != VK_SUCCESS) return false;
    if (vkBindBufferMemory(device, out->aBuffer, out->aMemory, 0) != VK_SUCCESS) return false;
    if (mapMemory && vkMapMemory(device, out->aMemory, 0, size, 0, &out->aMapped) != VK_SUCCESS) {
        return false;
    }
    return true;
}

inline void destroyVkBuffer(VkDevice device, sVkBuffer *buffer) {
    if (!buffer) return;
    if (buffer->aMapped) {
        vkUnmapMemory(device, buffer->aMemory);
        buffer->aMapped = nullptr;
    }
    if (buffer->aBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, buffer->aBuffer, nullptr);
        buffer->aBuffer = VK_NULL_HANDLE;
    }
    if (buffer->aMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, buffer->aMemory, nullptr);
        buffer->aMemory = VK_NULL_HANDLE;
    }
}

} // namespace Cornell
#endif // CORNELL_VK_UTIL_H
