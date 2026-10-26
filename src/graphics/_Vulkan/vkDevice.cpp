// Vulkan 设备级辅助：SPIR-V 读取、错误检查、内存类型、缓冲与视图创建。
#include <GN.h>
#include "GN_Vk.h"
#include "vkInternal.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <vector>

namespace GNg {

GN_VK_INTERNAL std::vector<uint32_t> LoadSPIRVFile(const char *path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if(!f.is_open()) return {};
    size_t sz = f.tellg();
    f.seekg(0);
    std::vector<uint32_t> spv(sz / sizeof(uint32_t));
    f.read((char *)spv.data(), sz);
    return spv;
}

void VkCheck(VkResult err, const char *msg) {
    if(err == VK_SUCCESS) return;

    const char *errStr = "UNKNOWN";
    switch(err) {
        case VK_NOT_READY: errStr = "NOT_READY"; break;
        case VK_TIMEOUT: errStr = "TIMEOUT"; break;
        case VK_EVENT_SET: errStr = "EVENT_SET"; break;
        case VK_EVENT_RESET: errStr = "EVENT_RESET"; break;
        case VK_INCOMPLETE: errStr = "INCOMPLETE"; break;
        case VK_ERROR_OUT_OF_HOST_MEMORY: errStr = "OUT_OF_HOST_MEMORY"; break;
        case VK_ERROR_OUT_OF_DEVICE_MEMORY: errStr = "OUT_OF_DEVICE_MEMORY"; break;
        case VK_ERROR_INITIALIZATION_FAILED: errStr = "INITIALIZATION_FAILED"; break;
        case VK_ERROR_DEVICE_LOST: errStr = "DEVICE_LOST"; break;
        case VK_ERROR_MEMORY_MAP_FAILED: errStr = "MEMORY_MAP_FAILED"; break;
        case VK_ERROR_LAYER_NOT_PRESENT: errStr = "LAYER_NOT_PRESENT"; break;
        case VK_ERROR_EXTENSION_NOT_PRESENT: errStr = "EXTENSION_NOT_PRESENT"; break;
        case VK_ERROR_FEATURE_NOT_PRESENT: errStr = "FEATURE_NOT_PRESENT"; break;
        case VK_ERROR_INCOMPATIBLE_DRIVER: errStr = "INCOMPATIBLE_DRIVER"; break;
        case VK_ERROR_TOO_MANY_OBJECTS: errStr = "TOO_MANY_OBJECTS"; break;
        case VK_ERROR_FORMAT_NOT_SUPPORTED: errStr = "FORMAT_NOT_SUPPORTED"; break;
        case VK_ERROR_FRAGMENTED_POOL: errStr = "FRAGMENTED_POOL"; break;
        case VK_ERROR_SURFACE_LOST_KHR: errStr = "SURFACE_LOST"; break;
        case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: errStr = "NATIVE_WINDOW_IN_USE"; break;
        case VK_SUBOPTIMAL_KHR: errStr = "SUBOPTIMAL"; break;
        case VK_ERROR_OUT_OF_DATE_KHR: errStr = "OUT_OF_DATE"; break;
        default: break;
    }
    fprintf(stderr, "[GN_Vk] ERROR %s: %s (code=%d)\n",
            msg ? msg : "(no message)", errStr, (int)err);
}

GN_VK_INTERNAL uint32_t FindMemType(uint32_t f, VkMemoryPropertyFlags p) {
    VkPhysicalDeviceMemoryProperties mp;
    vkGetPhysicalDeviceMemoryProperties(GN_Vk.physicalDevice, &mp);
    for(uint32_t i = 0; i < mp.memoryTypeCount; i++)
        if((f & (1 << i)) && (mp.memoryTypes[i].propertyFlags & p) == p) return i;
    return 0;
}

GN_VK_INTERNAL void CreateBuf(VkDeviceSize sz, VkBufferUsageFlags u, VkMemoryPropertyFlags p, VkBuffer &b, VkDeviceMemory &m) {
    VkBufferCreateInfo ci = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    ci.size = sz;
    ci.usage = u;
    ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VkResult res = vkCreateBuffer(GN_Vk.device, &ci, 0, &b);
    VkCheck(res, "vkCreateBuffer in CreateBuf");
    if(res != VK_SUCCESS) return;
    VkMemoryRequirements r;
    vkGetBufferMemoryRequirements(GN_Vk.device, b, &r);
    VkMemoryAllocateInfo ai = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    ai.allocationSize = r.size;
    ai.memoryTypeIndex = FindMemType(r.memoryTypeBits, p);
    res = vkAllocateMemory(GN_Vk.device, &ai, 0, &m);
    VkCheck(res, "vkAllocateMemory in CreateBuf");
    if(res != VK_SUCCESS) return;
    res = vkBindBufferMemory(GN_Vk.device, b, m, 0);
    VkCheck(res, "vkBindBufferMemory in CreateBuf");
}

GN_VK_INTERNAL VkImageView MakeImageView(VkImage img, VkFormat fmt) {
    VkImageViewCreateInfo ci = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    ci.image = img;
    ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    ci.format = fmt;
    ci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    ci.subresourceRange.levelCount = 1;
    ci.subresourceRange.layerCount = 1;
    VkImageView v;
    VkResult res = vkCreateImageView(GN_Vk.device, &ci, 0, &v);
    VkCheck(res, "vkCreateImageView in MakeImageView");
    return v;
}

} // namespace GNg
