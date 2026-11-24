// src/graphics/GN_ShadowCascade.h — L3c CSM（CPU）：级联分割、相机视锥角点、
// 光源空间正交拟合与 texel snapping，输出每级 lightViewProj / splits / texelWorld。
//
// 职责：纯 CPU 数学，不创建也不读写任何 Vulkan 资源；调用方把结果写进 sLIGHT_UBO
// （cLIGHT::SetShadowLightVP / SetShadowCascades）并逐级提交 shadow pass。
// 契约：docs/L4_L3_CONTRACT.md §8（冻结）；分割公式与输出字段名以契约与
// GN_3Dlight.h::sLIGHT_UBO 为准（shadowLightVP/cascadeSplits/shadowTexelWorld）。
#ifndef GN_SHADOW_CASCADE_H
#define GN_SHADOW_CASCADE_H

#include <GN.h>         // 先定义 GN_API / 模块宏：GN_2Dmath.h 依赖它（同 weather.h 的写法）
#include "GN_3Dmath.h"  // sMAT44 / sVEC3
#include "GN_Spatial.h" // sAABB

namespace GN3d {

class cLIGHT; // 前向声明（fitShadowCascades 可选写光源）

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

// ── 阴影远平面口径（契约 §8，L3c）────────────────────────────
// far = max(拟合盒对角, 相机到盒最远角距离 + 0.5)，下限 1 m。
//
// 为什么需要这个函数：这个口径原先在两个 demo 里各写了一份完全相同的 22 行循环，
// 改一处必忘另一处。抽到引擎后只有一份定义。
//
// 为什么不只取盒对角：相机站在场景外面时，只取对角会让各级切片全落在
// 相机与场景之间——阴影整片消失（那个 bug 曾经真的出现过）。
float computeShadowFar(const sAABB &sceneBox, const GNg::sVEC3 &cameraPos);


// ── 一步完成：策略 + 拟合 + 写进 sLIGHT_UBO ────────────────
// 这是 demo 里真正重复的那段“策略”：
//   1. 拟合盒可选外扩并抬到地面以上；
//   2. far 用 computeShadowFar；
//   3. 级联数钳到引擎实际分配的层数；
//   4. buildShadowCascades；
//   5. 结果写进 cLIGHT（SetShadowLightVP / SetShadowCascades）。
// 调用方只需持有结果用于逐级提交 shadow pass。
struct sSHADOW_FIT_DESC {
    GNg::sMAT44 aCameraView;      // 世界→视图
    GNg::sVEC3 aCameraPos;
    float aFovYRad;
    float aAspect;
    GNg::sVEC3 aLightDirection;   // 光线传播方向（指向场景）
    sAABB aCasterBox;             // 投影体（角色/模型）的 AABB
    float aCatchMargin;           // 承接区外扩（m）；0 = 不外扩
    bool aClampToGround;          // true: 拟合盒 aMin.y 抬到 0（地面建在 y=0 时用）
    uint32_t aCascadeCount;
    uint32_t aShadowMapSize;
    float aSplitLambda;
    bool aTexelSnapping;
    uint32_t aEngineCascadeLayers; // 引擎实际分配的阴影层数（GN_VkShadowCascadeCount）

    sSHADOW_FIT_DESC();
};

// 成功返回 true，并把结果写进 *out 与 *light（light 可为 nullptr，此时只算不写）。
// aCasterBox 无效 / 参数非法 / 拟合失败均返回 false（*out 不被修改）。
bool fitShadowCascades(const sSHADOW_FIT_DESC &desc, cLIGHT *light,
                       sSHADOW_CASCADE_RESULT *out);

// ── 一帧的级联结果（示例/实体层持有的那份全局量）────────────────────
// 为什么要有它：两个示例原先各自在 .cpp 里摊开 7 个全局量
// （4 个光矩阵 + splits + texelWorld + far + count + valid），外加一个把
// sSHADOW_CASCADE_RESULT 逐字段搬进去的循环——两份逐字相同。收成一个结构后，
// 搬运用例里的 `state = result` 一句话就够，"哪些字段忘了搬"这种 bug 不再可能。
struct sSHADOW_STATE {
    GNg::sMAT44 aLightViewProj[SHADOW_CASCADE_MAX]; // 每级 lightVP（col-major）
    float aSplits[SHADOW_CASCADE_MAX];              // 每级视距（m）
    float aTexelWorld[SHADOW_CASCADE_MAX];          // 每级 texel 世界尺寸（m/texel）
    uint32_t aCount;                                // 实际级数（0 = 无效）
    uint32_t aMapSize;                              // 阴影图边长（texel）
    float aFar;                                     // 拟合用的 far（诊断/叠层显示）
    bool aValid;                                    // 最近一次拟合是否成功

    sSHADOW_STATE();
    void Clear(); // 全清零（aValid = false）
};

// 拟合盒：模型世界 AABB 的 x/z 各外扩 margin，下沿抬到 groundY（只抬不压）。
//
// 为什么是 x/z 而不是全向：阴影承接区是「模型四周的地面」——竖直方向外扩只会把
// 空白天空拉进拟合盒，让 texel 变粗。mmdViewer 原先在 .cpp 里写的正是这个策略，
// 现在只有一份。
sAABB expandShadowCasterBox(const sAABB &modelBox, float margin, float groundY);

// 一步：拟合级联（内部调 fitShadowCascades）+ 填 sSHADOW_STATE（含 far 回填）。
// 失败时 *out 被 Clear()（aValid = false），不保留上一帧的脏数据——
// 否则"这一帧没拟合成功却继续用上一帧的光矩阵"会画出错位阴影。
bool fitShadowState(const sSHADOW_FIT_DESC &desc, cLIGHT *light, sSHADOW_STATE *out);

} // namespace GN3d
#endif // GN_SHADOW_CASCADE_H
