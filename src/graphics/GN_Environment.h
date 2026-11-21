// GN_Environment.h — 环境光照预设的纯 CPU 契约：室内均匀 SH9 与天空 IBL 的引擎侧选择。
// 不依赖 Vulkan；buildEnvironmentGi 只生成 sGI_UBO，GPU 写入由 GN_VkSetEnvironmentLighting 完成。
#ifndef _GN_ENVIRONMENT_H_
#define _GN_ENVIRONMENT_H_

#include "giParams.h"

#include <cstddef>

namespace GN3d {

// 环境光照来源：SKY_IBL=烘焙天空/IBL（默认），INDOOR_UNIFORM=均匀暖灰 SH9（室内）。
enum eENV_LIGHTING {
    eENV_LIGHTING_SKY_IBL = 0,
    eENV_LIGHTING_INDOOR_UNIFORM = 1
};

// 校验预设参数；ambientScale 非有限时按 0，钳制到 [0,4]，钳制结果写入 outScale（可为 null）。
bool validateEnvironmentLighting(eENV_LIGHTING mode, float ambientScale, float *outScale,
                                 char *reason, size_t reasonSize);
// 生成预设的 GI UBO：INDOOR 返回 true 并写 outUbo；SKY 返回 false（调用方保留烘焙 UBO）。
bool buildEnvironmentGi(eENV_LIGHTING mode, float ambientScale, sGI_UBO *outUbo);

} // namespace GN3d
#endif // _GN_ENVIRONMENT_H_
