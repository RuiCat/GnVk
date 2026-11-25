#pragma once
#ifndef _GN_3DLIGHT_H_
#define _GN_3DLIGHT_H_
// ╔══════════════════════════════════════════════════════════╗
// ║  GN_mini — 3D 光源系统                                  ║
// ║  方向光 / 点光源 / 聚光灯 + GPU UBO 布局                 ║
// ╚══════════════════════════════════════════════════════════╝

#include "GN_3Dmath.h"

namespace GN3d {

// ═══════════════════════════════════════════════════════════
//  光源类型
// ═══════════════════════════════════════════════════════════
enum eLIGHT_TYPE {
    eLIGHT_DIRECTIONAL = 0,
    eLIGHT_POINT = 1,
    eLIGHT_SPOT = 2,
};

#define GN_MAX_LIGHTS 8 // 着色器中的最大光源数

// cube shadow 最多几层（= 最多几盏灯能同时有 cube shadow）。
// 必须在使用它的类/结构**之前**定义。
#define GN_MAX_CUBE_SHADOW_PICKS 4

// ═══════════════════════════════════════════════════════════
//  单个光源 GPU 数据结构 (与着色器 LightUBO 对齐)
//  sizeof = 80 bytes, 需要 16 字节对齐
// ═══════════════════════════════════════════════════════════
struct alignas(16) sLIGHT_GPU {
    float color[4]; // RGB + intensity
    float position[4]; // xyz + type (0=dir, 1=point, 2=spot)
    float direction[4]; // xyz + _pad
    float attenuation[4]; // constant, linear, quadratic, range
    // [0]=innerCos [1]=outerCos（聚光用）；[2]=**光源物理半径**（点光/聚光，PCSS 半影用）；
    // [3]=保留。复用 [2] 而不是新增字段：新增会改动 sLIGHT_UBO 的布局契约（672 B 冻结槽位），
    // 而 [2]/[3] 自始未使用 —— 用它零成本。
    float coneAngles[4];
};

// 最大阴影级联数（L3c CSM；固定 4 级，cascadeCount=1 时退化为单张阴影图）
#define GN_MAX_SHADOW_CASCADES 4

// ═══════════════════════════════════════════════════════════
//  光源 UBO (整个传给着色器)
//  sizeof = 976 bytes (含 4 级 CSM 数据；与 gn3d_pbr.frag /
//  gn3d_deferred_light.frag / gn3d.frag 的 LightUBO 逐字段一致)
//  672 + shadowLightVP(4×64=256) + shadowParams(16) + cascadeSplits(16) + shadowTexelWorld(16)
// ═══════════════════════════════════════════════════════════
struct alignas(16) sLIGHT_UBO {
    sLIGHT_GPU lights[GN_MAX_LIGHTS];
    float ambientLight[4]; // RGB + intensity
    float lightCount; // 实际激活的光源数
    float _pad[3];
    // ── 阴影映射数据 (PBR/延迟管线使用；L3c：最多 4 级 CSM) ──
    float shadowLightVP[GN_MAX_SHADOW_CASCADES][16]; // 每级光源 VP (列主序；未用级填 0)
    float shadowParams[4]; // x=Shadow Map 分辨率, y=级数(1..4), z/w=0
    float cascadeSplits[4]; // 各级视距分割 (米，升序；选级用包含测试，此处供日志/验收)
    float shadowTexelWorld[4]; // 各级 texel 世界尺寸 (米/texel，法线偏移用)

    // ── 多光源 cube shadow（B2 扩展）──
    // **按灯槽索引（GN_MAX_LIGHTS = 8）**，不是按层索引。
    // 每项：.x = near, .y = far, .z = 层索引(0..3 或 -1=无)，.w 保留。
    //
    // 为什么是 8 项而不是 4 项（这里踩过一次）：层号只有 4 个，但**灯**有 8 盏。
    // 若只留 4 项，槽 4..7 就写不进去，保留 memset 的 0 —— 而 0 是**有效**层号，
    // 着色器会以为"这盏灯有 shadow、层号 0"，去采样一张从没渲染过的深度图，
    // 整片场景被错误遮挡。（这正是"层号默认必须 -1"那条坑的另一个入口。）
    //
    // **在末尾追加**而不是改已有字段偏移：前面 976 B 的布局是既有的着色器契约，
    // 改动会让所有引用它的着色器静默错位。追加只影响 sizeof。
    float cubeShadowRange[GN_MAX_LIGHTS][4];
};
static_assert(sizeof(sLIGHT_UBO) == 1104,
              "sLIGHT_UBO = 976（既有契约）+ 8×16（cube shadow 每灯 near/far/layer）");

// ═══════════════════════════════════════════════════════════
//  cLIGHT — 光源管理器
// ═══════════════════════════════════════════════════════════
class cLIGHT {
  public:
    sLIGHT_UBO aData; // GPU 数据
    uint32_t aCount; // 当前光源数
    bool aDirty; // 需要重新上传

    cLIGHT();
    void Clear();

    // 添加方向光 (enableShadow=false 可创建无影方向光, 节省性能)
    int AddDirectional(const GNg::sVEC3 &dir, const GNg::sVEC3 &color,
                       float intensity = 1.0f, bool enableShadow = true);

    // 添加点光源。enableShadow = true 时写入 cube shadow（**只有一盏能生效**：
    // 引擎只有一张 cube map，后写入的会覆盖前面的；见 GN_VkSetCubeShadowLight）。
    // lightRadius：光源的物理半径（米），决定点光阴影的半影宽度（PCSS）。
    // 0 = 用默认 0.1（近似"细灯丝"）；火焰/灯泡这类有体积的光源给 0.15~0.4。
    int AddPoint(const GNg::sVEC3 &pos, float range,
                 const GNg::sVEC3 &color, float intensity = 1.0f, bool enableShadow = false,
                 float lightRadius = 0.1f);

    // 添加聚光灯 (enableShadow=false 可关闭阴影)
    int AddSpot(const GNg::sVEC3 &pos, const GNg::sVEC3 &dir,
                float range, float innerConeRad, float outerConeRad,
                const GNg::sVEC3 &color, float intensity = 1.0f,
                bool enableShadow = false, // 同上：点光/聚光共用一张 cube map
                float lightRadius = 0.1f);

    // ── cube shadow 的深度范围（nearZ/farZ）──
    // **必须与 GN_VkCubeShadowBegin 传的值一致**。原因：cube shadow 图里存的是
    // **NDC 深度（0..1）**，而着色器里算出的是世界距离（米）—— 两者要比较就得先换算，
    // 换算需要 near/far。不设的话着色器没法把米转成 NDC，只能保守地返回"全亮"。
    // 写入 LIGHT_UBO 的 shadowParams.z/w（这两个位置此前未使用）。
    // B2 扩展：**多盏灯**各自的 cube shadow 深度范围。
    // lightSlot = 该灯在 UBO lights[] 里的下标；layer = 它在 cube array 里的层号。
    // 旧接口 SetCubeShadowRange 保留（等价于对 layer 0 的灯设置）。
    void SetCubeShadowRangeForLight(uint32_t lightSlot, float nearZ, float farZ, int32_t layer);
    void ClearCubeShadowRanges();
    // 一步到位：按**注册顺序**（与相机无关）给所有开 cube shadow 的灯分配层号并写入
    // cubeShadowRange。返回分配到的层数。
    //
    // 这是推荐用法 —— 手写"层号 = 选灯排名"会让影子随视角闪动（见 assignCubeShadowLayers）。
    uint32_t AssignCubeShadowLayers(float nearZ, float farZ,
                                    uint32_t maxLayers = GN_MAX_CUBE_SHADOW_PICKS);
    void SetCubeShadowRange(float nearZ, float farZ);
    float CubeShadowNear() const;
    float CubeShadowFar() const;

    // 光源物理半径（PCSS 半影宽度用）。越界返回 0。
    float GetLightRadius(int index) const;
    void SetLightRadius(int index, float radius); // 钳到 >= 0

    // 修改/获取光源
    void SetColor(int index, const GNg::sVEC3 &color);
    void SetIntensity(int index, float intensity);
    void SetPosition(int index, const GNg::sVEC3 &pos);
    void SetDirection(int index, const GNg::sVEC3 &dir);
    void Remove(int index);
    void SetAmbient(const GNg::sVEC3 &color, float intensity = 1.0f);

    // 阴影映射数据（L3c：级联索引 0..3；越界钳制）
    void SetShadowLightVP(const GNg::sMAT44 &lightVP, uint32_t cascade = 0);
    void SetShadowMapSize(float size);
    // 写入级数/分割距离/每级 texel 世界尺寸（splits 与 texelWorld 指针可为空）
    void SetShadowCascades(uint32_t count, const float *splits, const float *texelWorld);

    // 获取 GPU 就绪的数据指针
    const sLIGHT_UBO *GetUBO() const { return &aData; }
};

// ═══════════════════════════════════════════════════════════
//  便捷函数: 创建常见光照配置
// ═══════════════════════════════════════════════════════════

// 标准三点光照 (Key + Fill + Back)
cLIGHT *CreateThreePointLighting(
    const GNg::sVEC3 &targetPos = GNg::sVEC3{0, 0, 0});

// 简单户外光照 (方向光 + 环境光)
cLIGHT *CreateOutdoorLighting();

// ═══════════════════════════════════════════════════════════
//  B2c：多光源 cube shadow 的**选灯**
// ═══════════════════════════════════════════════════════════
// 引擎只有一张 cube map（6 面 × 1024² ≈ 25 MB + 6 遍场景渲染），所以"多盏点光都有阴影"
// 在当前结构下不成立。可行做法是**每帧挑一盏最重要的**渲染，其余用无阴影版本 ——
// 这也是多数实时引擎在中低配置下的策略。
//
// 评分口径（每条都有用例）：
//   * 只考虑 `shadowType == 2`（开了 cube shadow）的点光/聚光；
//   * 主要项是"**离相机多近**"：靠近相机的光源，投出的阴影最容易被看到；
//   * 次项是"强度"：一盏在画面外的弱光不该抢走阴影预算；
//   * 距离超过 maxDistance 的直接排除（远处光源的阴影在屏幕上只有几个像素）。
struct sCUBE_SHADOW_PICK {
    int aLightIndex; // -1 = 没有可用的候选
    float aScore; // 越大越重要（诊断/日志用）
    sCUBE_SHADOW_PICK();
};

// 从 LIGHT_UBO 里挑一盏该渲 cube shadow 的灯。maxDistance <= 0 用默认 40 m。
sCUBE_SHADOW_PICK pickCubeShadowLight(const sLIGHT_UBO &light, const GNg::sVEC3 &cameraPos,
                                      float maxDistance = 40.0f);

// ═══════════════════════════════════════════════════════════
//  多光源 cube shadow 的选灯（B2 扩展）
// ═══════════════════════════════════════════════════════════

struct sCUBE_SHADOW_PICKS {
    int aLightIndex[GN_MAX_CUBE_SHADOW_PICKS]; // 按评分**从高到低**排；-1 表示空位
    float aScore[GN_MAX_CUBE_SHADOW_PICKS];
    uint32_t aCount; // 实际选中几盏（0..4）

    sCUBE_SHADOW_PICKS();
};

// 挑出最多 maxCount 盏该渲 cube shadow 的灯，按评分降序。
//
// 与单灯版**共用同一套评分**（离相机近 + 强度高 + 距离上限），只是改成取前 N 名 ——
// 这样"单灯时代挑中的那一盏"在多灯时代依然是第一名，行为可预测。
// 并列时**先注册的在前**（与单灯版一致）。
//
// **注意：返回数组的下标是"评分排名"，不是层号，不要直接拿它当层号用。**
// 层号必须用 `assignCubeShadowLayers`（它只看注册顺序，与相机无关）。
//
// 为什么：本函数依赖 cameraPos，相机一动排名就变。若把排名当层号，
// 同一盏灯在相邻两帧会落到不同的层 —— 灯数超过上限（或 maxCount < 4）时，
// 影子就会随着视角**闪进闪出**。影子只该由"光源 + 遮挡物"决定，与相机无关。
// 本函数的用途是"在灯很多时选出最该有阴影的几盏"，选完之后**仍要**用
// 稳定的层号去渲染。
sCUBE_SHADOW_PICKS pickCubeShadowLights(const sLIGHT_UBO &light, const GNg::sVEC3 &cameraPos,
                                        float maxDistance = 40.0f,
                                        uint32_t maxCount = GN_MAX_CUBE_SHADOW_PICKS);



// ═══════════════════════════════════════════════════════════
//  层号分配（**与相机无关**）
// ═══════════════════════════════════════════════════════════
// 影子是"光源 + 遮挡物"的几何结果，**不允许依赖相机**。
// 所以层号只按光源在 UBO 里的**注册顺序**分配：第 1 盏开 cube shadow 的灯 → 层 0，
// 第 2 盏 → 层 1 …… 超过 maxLayers 的灯拿不到层（-1 = 无阴影，保守返回全亮）。
//
// 这个函数是纯函数（不碰 cLIGHT），便于单测直接钉住"换个相机也得到同样的层号"。
struct sCUBE_SHADOW_LAYOUT {
    int32_t aLayer[GN_MAX_LIGHTS]; // 每盏灯的层号；-1 = 无 cube shadow
    uint32_t aLayerCount; // 实际分配了几层（0..maxLayers）
    uint32_t aDroppedLights; // 因为是第 maxLayers+1 盏之后而没分到层的灯数

    sCUBE_SHADOW_LAYOUT();
};

// 按注册顺序给"开了 cube shadow 的点光/聚光"分配层号。
// maxLayers 超过 GN_MAX_CUBE_SHADOW_PICKS 时会被钳制（cube array 只有那么多层）。
sCUBE_SHADOW_LAYOUT assignCubeShadowLayers(const sLIGHT_UBO &light,
                                          uint32_t maxLayers = GN_MAX_CUBE_SHADOW_PICKS);

} // namespace GN3d
#endif // _GN_3DLIGHT_H_
