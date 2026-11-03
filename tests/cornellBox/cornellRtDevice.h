// tests/cornellBox/cornellRtDevice.h — 独立 ray-query Vulkan 设备（不依赖引擎）
//
// 引擎的 VkDevice 没开 VK_KHR_acceleration_structure / VK_KHR_ray_query，
// 因此硬件光追预览用一个独立实例/设备（同一物理 GPU），渲染结果仍回读到 CPU 再交给 sIMAGE。
#ifndef CORNELL_RT_DEVICE_H
#define CORNELL_RT_DEVICE_H

#include <vulkan/vulkan.h>

#include <cstdint>

namespace Cornell {

struct sRayQueryDevice {
    VkInstance aInstance;
    VkPhysicalDevice aPhysical;
    VkDevice aDevice;
    VkQueue aQueue;
    uint32_t aQueueFamily;
};

// 创建带 ray-query 能力的无窗口设备；设备不支持时返回 false（调用方回退 compute/CPU）。
bool createRayQueryDevice(sRayQueryDevice *out);

// 销毁 createRayQueryDevice 创建的资源；重复调用安全。
void destroyRayQueryDevice(sRayQueryDevice *dev);

} // namespace Cornell
#endif // CORNELL_RT_DEVICE_H
