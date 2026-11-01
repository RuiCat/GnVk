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

// ═══════════════════════════════════════════════════════════
//  单个光源 GPU 数据结构 (与着色器 LightUBO 对齐)
//  sizeof = 80 bytes, 需要 16 字节对齐
// ═══════════════════════════════════════════════════════════
struct alignas(16) sLIGHT_GPU {
    float color[4]; // RGB + intensity
    float position[4]; // xyz + type (0=dir, 1=point, 2=spot)
    float direction[4]; // xyz + _pad
    float attenuation[4]; // constant, linear, quadratic, range
    float coneAngles[4]; // innerCos, outerCos, _pad, _pad
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
};
static_assert(sizeof(sLIGHT_UBO) == 976, "sLIGHT_UBO 必须为 976 字节");

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

    // 添加点光源
    int AddPoint(const GNg::sVEC3 &pos, float range,
                 const GNg::sVEC3 &color, float intensity = 1.0f);

    // 添加聚光灯 (enableShadow=false 可关闭阴影)
    int AddSpot(const GNg::sVEC3 &pos, const GNg::sVEC3 &dir,
                float range, float innerConeRad, float outerConeRad,
                const GNg::sVEC3 &color, float intensity = 1.0f,
                bool enableShadow = true);

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

} // namespace GN3d
#endif // _GN_3DLIGHT_H_
