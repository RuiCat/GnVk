// src/graphics/_Vulkan/vkShadowCascade.h — L3c CSM（CPU）：级联分割、相机视锥角点、
// 光源空间正交拟合与 texel snapping，输出每级 lightViewProj / splits / texelWorld。
//
// 职责：纯 CPU 数学，不创建也不读写任何 Vulkan 资源；调用方把结果写进 sLIGHT_UBO
// （cLIGHT::SetShadowLightVP / SetShadowCascades）并逐级提交 shadow pass。
// 契约：docs/L4_L3_CONTRACT.md §8（冻结）；分割公式与输出字段名以契约与
// GN_3Dlight.h::sLIGHT_UBO 为准（shadowLightVP/cascadeSplits/shadowTexelWorld）。
#ifndef VK_SHADOW_CASCADE_H
#define VK_SHADOW_CASCADE_H

#include <GN.h>         // 先定义 GN_API / 模块宏：GN_2Dmath.h 依赖它（同 weather.h 的写法）
#include "GN_3Dmath.h"  // sMAT44 / sVEC3
#include "GN_Spatial.h" // sAABB

namespace GN3d {

// 最大阴影级数（与 GN_3Dlight.h 的 GN_MAX_SHADOW_CASCADES 一致）
const uint32_t SHADOW_CASCADE_MAX = 4;

// 级联拟合输入。aCascadeCount / aShadowMapSize / aSplitLambda 越界时被钳制。
struct sSHADOW_CASCADE_DESC {
    GNg::sMAT44 aCameraView;    // 世界→视图（RH，-Z 前；须可逆）
    float aFovYRad;             // 垂直视场角（弧度，>0）
    float aAspect;              // 宽/高（>0）
    float aNear;                // 第 0 级近平面（m，>0；契约 §8：0.1）
    float aFar;                 // 最后一级远平面（m，>aNear）
    GNg::sVEC3 aLightDirection; // 光线传播方向（指向场景、单位；零向量回退 (0,-1,0)）
    sAABB aSceneAABB;           // 场景世界 AABB（无效 ⇒ buildShadowCascades 返回 false）
    uint32_t aCascadeCount;     // 1..4（默认 4；1 = 退化为单级）
    uint32_t aShadowMapSize;    // 每级阴影图边长（texel，默认 2048）
    float aSplitLambda;         // 对数/均匀混合权重 0..1（0=纯对数、1=纯均匀；默认 0.5 = 各半）
    bool aTexelSnapping;        // L3a texel snapping 开关（默认 true；仅供对照/单测）

    sSHADOW_CASCADE_DESC();
};

// 级联拟合输出；未使用的级保持全 0（与 sLIGHT_UBO 的「未用级填 0」注释一致）。
struct sSHADOW_CASCADE_RESULT {
    GNg::sMAT44 aLightViewProj[SHADOW_CASCADE_MAX]; // 每级 proj × lightView（列主序）
    float aSplits[SHADOW_CASCADE_MAX];              // 各级视距（m，升序）
    float aTexelWorld[SHADOW_CASCADE_MAX];          // 各级 texel 世界尺寸（m/texel）
    uint32_t aCount;                                // 实际级数（= 钳制后的级数）
    uint32_t aShadowMapSize;                        // 钳制后的阴影图边长（texel）

    sSHADOW_CASCADE_RESULT();
};

// 固定分割（契约 §8）：
//   split[i] = lerp(near*(far/near)^t, near+(far-near)*t, lambda)，t=(i+1)/count。
// count 钳到 1..SHADOW_CASCADE_MAX、lambda 钳到 0..1；
// near/far 非法（near<=0 或 far<=near）或 outSplits 为空时输出全 0。
void computeCascadeSplits(float nearZ, float farZ, uint32_t count, float lambda,
                          float outSplits[SHADOW_CASCADE_MAX]);

// 拟合 4 级 CSM 矩阵；成功返回 true。
// 参数非法（out 为空 / 视图矩阵不可逆或非有限 / AABB 无效 / fov/aspect/near/far 非法）
// 返回 false 且不改动 *out；某级矩阵出现非有限值时该级保持全 0（选级不会命中，不产生垃圾数据）。
bool buildShadowCascades(const sSHADOW_CASCADE_DESC &desc, sSHADOW_CASCADE_RESULT *out);

} // namespace GN3d
#endif // VK_SHADOW_CASCADE_H
