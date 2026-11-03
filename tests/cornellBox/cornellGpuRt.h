// tests/cornellBox/cornellGpuRt.h — GPU compute 光追预览的宿主契约
//
// 职责：把 cornellCpuRt 的解析体路径追踪搬到 compute shader；场景由
// buildCornellGeometry(false) 打包成 SSBO（避免两套几何/材质漂移），
// 宿主只负责建管线、传相机参数、回读 RGBA8 像素。
//
// 设备上下文由调用方注入：cornellBox 传引擎的 GN_Vk；cornellGpuRtProbe
// 自建无窗口设备，因此本模块不依赖引擎（不 include GN.h）。
#ifndef CORNELL_GPU_RT_H
#define CORNELL_GPU_RT_H

#include <vulkan/vulkan.h>

#include <vector>

namespace Cornell {

// 调用方提供的 Vulkan 设备上下文（逻辑设备 + 物理设备 + 图形/计算队列）。
struct sGpuRtContext {
    VkDevice aDevice;
    VkPhysicalDevice aPhysicalDevice;
    VkQueue aQueue;
    uint32_t aQueueFamily;
};

// 创建 compute 管线、场景/参数/输出缓冲（width×height 固定，重设尺寸需先 shutdown）。
// 失败返回 false，并已回滚本次分配；调用方可回退到 CPU RT。
bool initGpuRt(const sGpuRtContext &ctx, int width, int height);

// 释放 initGpuRt 创建的全部 Vulkan 资源；重复调用安全。
void shutdownGpuRt();

// 管线是否已就绪。
bool isGpuRtReady();

// 渲染当前相机视角到 RGBA8（width*height*4）。相机/光照语义与 renderCpuRt 一致；
// sampleCount 为每像素路径数（8 = 与 CPU 参考同 seed 逐像素对照）。
bool renderGpuRt(int width, int height, const float eye[3], const float fwd[3],
                 const float right[3], const float up[3], float fovYDeg, int lightMode,
                 int sampleCount, std::vector<unsigned char> *outRgba);

} // namespace Cornell
#endif // CORNELL_GPU_RT_H
