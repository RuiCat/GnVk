// tests/cornellBox/cornellGpuRt.h — GPU compute 光追预览的宿主契约
//
// 职责：Cornell 光追预览的后端装置——两种后端共用同一套 dispatch/累积/回读：
//   * eGPU_RT_ANALYTIC：compute shader 做解析求交（7 个四边形/球）；
//   * eGPU_RT_RAY_QUERY：compute shader 用 VK_KHR_ray_query 遍历 TLAS（BVH）。
// 场景均由 buildCornellGeometry(false) 提供（SSBO / 三角化后建 AS），避免两套几何漂移。
//
// 设备上下文由调用方注入：cornellBox 传引擎的 GN_Vk；cornellGpuRtProbe
// 自建无窗口设备，因此本模块不依赖引擎（不 include GN.h）。
#ifndef CORNELL_GPU_RT_H
#define CORNELL_GPU_RT_H

#include <vulkan/vulkan.h>

#include <vector>

namespace Cornell {

// 光追后端：解析求交（无特殊扩展）或硬件 ray query（设备需 AS/ray_query 扩展）。
enum eGpuRtBackend {
    eGPU_RT_ANALYTIC = 0,
    eGPU_RT_RAY_QUERY = 1,
};

// 调用方提供的 Vulkan 设备上下文（逻辑设备 + 物理设备 + 图形/计算队列）。
struct sGpuRtContext {
    VkDevice aDevice;
    VkPhysicalDevice aPhysicalDevice;
    VkQueue aQueue;
    uint32_t aQueueFamily;
};

// 创建 compute 管线、场景/参数/输出缓冲（width×height 固定，重设尺寸需先 shutdown）。
// backend 选择解析求交或 ray query；后者要求 ctx.aDevice 已启 AS/ray_query 扩展。
// 失败返回 false，并已回滚本次分配；调用方可回退 CPU/解析路径。
bool initGpuRt(const sGpuRtContext &ctx, int width, int height,
               eGpuRtBackend backend = eGPU_RT_ANALYTIC);

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
