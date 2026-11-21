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

// 光追预览的最近一帧计时与累积状态（CPU 计时，不含 GPU timestamp）。
struct sGpuRtStats {
    double aSubmitMs;            // 最近一次 vkQueueSubmit 的 CPU 耗时
    double aWaitMs;              // 最近一次等待上一帧 fence 的耗时
    double aReadbackMs;          // 最近一次 memcpy 回读的耗时
    unsigned int aAccumSamples;  // 已提交的累积样本数（目标见 CORNELL_RT_TARGET）
    unsigned int aCompletedSerial; // 已完成回读的帧序号（每回读一帧 +1）
    int aLastSamples;            // 最近一帧提交的每像素样本数
};

const sGpuRtStats &gpuRtStats();

// 相机/光照相对上次提交是否变化（用于选择移动/静止的样本数）。
bool gpuRtCameraMoved(const float eye[3], const float fwd[3], const float right[3],
                      const float up[3], float fovYDeg, int lightMode);

// 渲染当前相机视角到 RGBA8（width*height*4）。相机/光照语义与 renderCpuRt 一致；
// sampleCount 为每像素路径数（8 = 与 CPU 参考同 seed 逐像素对照）。
// 同步路径：提交后立即等待并回读，返回的一定是本帧结果；供一次性自检使用。
bool renderGpuRt(int width, int height, const float eye[3], const float fwd[3],
                 const float right[3], const float up[3], float fovYDeg, int lightMode,
                 int sampleCount, std::vector<unsigned char> *outRgba);

// 交互路径的两步（顺序必须是 collect → 上传纹理 → submit，才能让 GPU 计算与
// 上一帧的上传/绘制重叠）：
//   gpuRtCollect：等待并回读上一帧（首帧可能为空），不提交新帧；
//   gpuRtSubmit：提交本帧；若已收敛则只更新状态、不 dispatch。
bool gpuRtCollect(std::vector<unsigned char> *outRgba);
bool gpuRtSubmit(int width, int height, const float eye[3], const float fwd[3],
                 const float right[3], const float up[3], float fovYDeg, int lightMode,
                 int sampleCount);

// 上传 SH9 探针（cx*cy*cz 个，每个 36 float）与 48B probe UBO；weight=0 关闭采样。
// 必须在 initGpuRt 之后、开始提交帧之前调用。
bool gpuRtUploadProbes(const float *sh9, const float *depthCubemap, const float origin[3],
                       float spacing, unsigned int cx, unsigned int cy, unsigned int cz,
                       unsigned int depthRes, float weight, float normalBias = 0.0f,
                       bool relocation = false);
// B3c-2 异步探针更新：每帧只更新 budget 个探针（0 = 每帧全量，供 parity 校验），
// samples = 每 texel 的 radiance 样本数，hysteresis = temporal 下限（0.9 → alpha 下限 0.1）。
// 必须先 gpuRtUploadProbes；失败返回 false 且保持同步（静态资产）路径不变。
bool gpuRtEnableProbeAsync(unsigned int budget, unsigned int samples, float hysteresis);
// 异步更新是否已启用。
bool gpuRtProbeAsyncActive();
// 把前 count 个探针标为 inactive（class=2），供“跳过 inactive”验收；需先 gpuRtEnableProbeAsync。
bool gpuRtMarkInactiveProbes(unsigned int count);
// 启用 GPU 端 6 轴 probe relocation（B3b-full）：与异步更新同批 dispatch，读刚更新的 depth。
// 需先 gpuRtEnableProbeAsync + gpuRtUploadProbes；关闭时 upload 期 CPU v1-lite 仍可用。
bool gpuRtEnableProbeRelocationGpu(bool enabled);
// 回读偏移（4*N float：xyz=世界偏移，w=有效标记）。
bool gpuRtReadbackRelocation(std::vector<float> *outOffsets);
// 回读探针 SH9（36*N float）与状态（4*N uint：age/class/score/flags）；内部先等待在飞帧。
bool gpuRtReadbackProbes(std::vector<float> *outSh, std::vector<unsigned int> *outState);

} // namespace Cornell
#endif // CORNELL_GPU_RT_H
