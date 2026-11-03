// tests/cornellBox/cornellRtDevice.cpp — 创建带 VK_KHR_ray_query 的独立 Vulkan 设备
//
// 只做「选物理设备 + 校验扩展/特性 + 建设备 + 取图形队列」；实例请求 Vulkan 1.2
// （bufferDeviceAddress 为 core，AS/rayQuery 走 KHR 扩展）。无窗口、无 surface。
#include "cornellRtDevice.h"

#include <cstdio>
#include <cstring>
#include <vector>

namespace Cornell {

namespace {

bool hasDeviceExtension(VkPhysicalDevice physical, const char *name) {
    uint32_t count = 0;
    if (vkEnumerateDeviceExtensionProperties(physical, nullptr, &count, nullptr) != VK_SUCCESS) {
        return false;
    }
    std::vector<VkExtensionProperties> props(count);
    vkEnumerateDeviceExtensionProperties(physical, nullptr, &count, props.data());
    for (uint32_t i = 0; i < count; ++i) {
        if (std::strcmp(props[i].extensionName, name) == 0) return true;
    }
    return false;
}

uint32_t findGraphicsFamily(VkPhysicalDevice physical) {
    uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical, &count, nullptr);
    std::vector<VkQueueFamilyProperties> queues(count);
    vkGetPhysicalDeviceQueueFamilyProperties(physical, &count, queues.data());
    for (uint32_t i = 0; i < count; ++i) {
        if (queues[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) return i;
    }
    return UINT32_MAX;
}

bool queryRayQueryFeatures(VkPhysicalDevice physical,
                           VkPhysicalDeviceAccelerationStructureFeaturesKHR *asFeatures,
                           VkPhysicalDeviceRayQueryFeaturesKHR *rqFeatures,
                           VkPhysicalDeviceBufferDeviceAddressFeatures *bdaFeatures) {
    VkPhysicalDeviceFeatures2 features = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
    *asFeatures = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR};
    *rqFeatures = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR};
    *bdaFeatures = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES};
    asFeatures->pNext = rqFeatures;
    rqFeatures->pNext = bdaFeatures;
    features.pNext = asFeatures;
    vkGetPhysicalDeviceFeatures2(physical, &features);
    return asFeatures->accelerationStructure == VK_TRUE && rqFeatures->rayQuery == VK_TRUE &&
           bdaFeatures->bufferDeviceAddress == VK_TRUE;
}

} // namespace

bool createRayQueryDevice(sRayQueryDevice *out) {
    if (!out) return false;
    *out = sRayQueryDevice{VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, 0};
    VkApplicationInfo app = {VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app.pApplicationName = "cornellHwRt";
    app.apiVersion = VK_API_VERSION_1_2;
    VkInstanceCreateInfo ici = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    ici.pApplicationInfo = &app;
    if (vkCreateInstance(&ici, nullptr, &out->aInstance) != VK_SUCCESS) {
        std::fprintf(stderr, "[cornellRtDevice] 无法创建 Vulkan 1.2 实例\n");
        return false;
    }

    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(out->aInstance, &deviceCount, nullptr);
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(out->aInstance, &deviceCount, devices.data());
    for (uint32_t i = 0; i < deviceCount; ++i) {
        const VkPhysicalDevice physical = devices[i];
        const uint32_t family = findGraphicsFamily(physical);
        if (family == UINT32_MAX) continue;
        if (!hasDeviceExtension(physical, VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME) ||
            !hasDeviceExtension(physical, VK_KHR_RAY_QUERY_EXTENSION_NAME)) {
            continue;
        }
        VkPhysicalDeviceAccelerationStructureFeaturesKHR asFeatures;
        VkPhysicalDeviceRayQueryFeaturesKHR rqFeatures;
        VkPhysicalDeviceBufferDeviceAddressFeatures bdaFeatures;
        if (!queryRayQueryFeatures(physical, &asFeatures, &rqFeatures, &bdaFeatures)) continue;

        std::vector<const char *> extensions = {
            VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
            VK_KHR_RAY_QUERY_EXTENSION_NAME,
        };
        if (hasDeviceExtension(physical, VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME)) {
            extensions.push_back(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
        }
        if (hasDeviceExtension(physical, VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME)) {
            extensions.push_back(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
        }
        asFeatures.accelerationStructure = VK_TRUE;
        rqFeatures.rayQuery = VK_TRUE;
        bdaFeatures.bufferDeviceAddress = VK_TRUE;

        const float priority = 1.0f;
        VkDeviceQueueCreateInfo qci = {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
        qci.queueFamilyIndex = family;
        qci.queueCount = 1;
        qci.pQueuePriorities = &priority;
        VkDeviceCreateInfo dci = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
        dci.queueCreateInfoCount = 1;
        dci.pQueueCreateInfos = &qci;
        dci.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        dci.ppEnabledExtensionNames = extensions.data();
        dci.pNext = &asFeatures;
        if (vkCreateDevice(physical, &dci, nullptr, &out->aDevice) == VK_SUCCESS) {
            out->aPhysical = physical;
            out->aQueueFamily = family;
            vkGetDeviceQueue(out->aDevice, family, 0, &out->aQueue);
            return true;
        }
    }
    std::fprintf(stderr, "[cornellRtDevice] 无支持 ray-query 的图形设备\n");
    destroyRayQueryDevice(out);
    return false;
}

void destroyRayQueryDevice(sRayQueryDevice *dev) {
    if (!dev) return;
    if (dev->aDevice != VK_NULL_HANDLE) vkDestroyDevice(dev->aDevice, nullptr);
    if (dev->aInstance != VK_NULL_HANDLE) vkDestroyInstance(dev->aInstance, nullptr);
    *dev = sRayQueryDevice{VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, 0};
}

} // namespace Cornell
