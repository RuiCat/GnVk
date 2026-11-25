// testLight.cpp — 3D 光源系统契约：sLIGHT_UBO 与着色器 std140 对齐、cLIGHT 增删改、CSM 配置。
//
// 这些断言把「光照系统的 CPU 侧」和「着色器 LightUBO 的字段偏移」钉在一起：
// gn3d.frag(binding2) / gn3d_pbr.frag(binding2) / gn3d_deferred_light.frag(binding7)
// 都按下面这些偏移读数据，任何一处漂移都会让光照静默错位（不报错、画面只是暗/错色）。
#include "testFramework.h"

#include <GN.h>
#include "GN_3Dlight.h"

#include <cmath>
#include <cstddef>
#include <cstring>

using namespace GN3d;
using namespace GNg;

// ── 布局契约：与 gn3d_pbr.frag / gn3d_deferred_light.frag 的 LightUBO 逐字段一致 ──
TEST_CASE(lightUboLayoutMatchesShader) {
    CHECK_EQ(sizeof(sLIGHT_GPU), 80u);
    // B2 扩展：976（既有契约）+ 8×16（每**灯槽**一份 near/far/layer） = 1104。
    // **已有字段的偏移一个都不能变** —— 下面逐条断言。
    CHECK_EQ(sizeof(sLIGHT_UBO), 1104u);

    // 8 光源 × 80 B = 640
    CHECK_EQ(offsetof(sLIGHT_UBO, lights), 0u);
    CHECK_EQ(offsetof(sLIGHT_UBO, ambientLight), 640u);      // vec4
    CHECK_EQ(offsetof(sLIGHT_UBO, lightCount), 656u);        // float + 12B padding
    CHECK_EQ(offsetof(sLIGHT_UBO, shadowLightVP), 672u);     // mat4[4] = 256 B
    CHECK_EQ(offsetof(sLIGHT_UBO, shadowParams), 928u);      // vec4
    CHECK_EQ(offsetof(sLIGHT_UBO, cascadeSplits), 944u);     // vec4
    CHECK_EQ(offsetof(sLIGHT_UBO, shadowTexelWorld), 960u);  // vec4
    CHECK_EQ(offsetof(sLIGHT_UBO, cubeShadowRange), 976u);   // vec4[4]（B2 扩展追加）

    CHECK_EQ(GN_MAX_LIGHTS, 8);
    CHECK_EQ(GN_MAX_SHADOW_CASCADES, 4u);
}

TEST_CASE(lightDefaultState) {
    cLIGHT light;
    CHECK_EQ(light.aCount, 0u);
    CHECK_NEAR(light.aData.lightCount, 0.0f, 1e-6);
    // 默认环境光与阴影图尺寸（CSM 默认单级，级数由查看器显式覆盖）
    CHECK_NEAR(light.aData.ambientLight[3], 1.0f, 1e-6);
    CHECK_NEAR(light.aData.shadowParams[0], 2048.0f, 1e-6);
    CHECK_NEAR(light.aData.shadowParams[1], 1.0f, 1e-6);
}

TEST_CASE(lightAddDirectional) {
    cLIGHT light;
    const sVEC3 dir = {0.0f, -1.0f, 0.0f};
    const sVEC3 color = {1.0f, 0.5f, 0.25f};
    const int idx = light.AddDirectional(dir, color, 2.0f, true);
    CHECK_EQ(idx, 0);
    CHECK_EQ(light.aCount, 1u);
    CHECK_NEAR(light.aData.lightCount, 1.0f, 1e-6);

    const sLIGHT_GPU &L = light.aData.lights[0];
    CHECK_NEAR(L.position[3], (float)eLIGHT_DIRECTIONAL, 1e-6); // 类型 = 0
    CHECK_NEAR(L.direction[3], 1.0f, 1e-6);                     // shadowType = 1（开阴影）
    CHECK_NEAR(L.direction[1], -1.0f, 1e-4);                    // 方向已归一化
    CHECK_NEAR(L.color[3], 2.0f, 1e-6);                         // 强度在 .a
    CHECK_NEAR(L.attenuation[0], 1.0f, 1e-6);                   // 方向光无衰减

    // 关闭阴影时 shadowType 归 0，着色器就不会进 CSM 分支
    const int idx2 = light.AddDirectional(dir, color, 1.0f, false);
    CHECK_EQ(idx2, 1);
    CHECK_NEAR(light.aData.lights[1].direction[3], 0.0f, 1e-6);
}

TEST_CASE(lightAddPointHasRange) {
    cLIGHT light;
    const sVEC3 pos = {0.0f, 0.0f, 1.0f};
    const sVEC3 color = {1.0f, 1.0f, 1.0f};
    const int idx = light.AddPoint(pos, 5.0f, color, 1.0f);
    CHECK_EQ(idx, 0);
    const sLIGHT_GPU &L = light.aData.lights[0];
    CHECK_NEAR(L.position[3], (float)eLIGHT_POINT, 1e-6);
    CHECK_NEAR(L.position[2], 1.0f, 1e-6);
    CHECK_NEAR(L.attenuation[3], 5.0f, 1e-6); // range 在 .w（着色器用它做距离早退）
    CHECK(L.attenuation[1] > 0.0f);           // 线性项非零
    CHECK(L.attenuation[2] > 0.0f);           // 二次项非零
}

TEST_CASE(lightAddSpotConeAngles) {
    cLIGHT light;
    const sVEC3 pos = {0.0f, 2.0f, 0.0f};
    const sVEC3 dir = {0.0f, -1.0f, 0.0f};
    const sVEC3 color = {1.0f, 1.0f, 1.0f};
    const int idx = light.AddSpot(pos, dir, 10.0f, 0.3f, 0.6f, color, 1.0f, false);
    CHECK_EQ(idx, 0);
    const sLIGHT_GPU &L = light.aData.lights[0];
    CHECK_NEAR(L.position[3], (float)eLIGHT_SPOT, 1e-6);
    CHECK_NEAR(L.coneAngles[0], std::cos(0.3f), 1e-5); // 内锥余弦
    CHECK_NEAR(L.coneAngles[1], std::cos(0.6f), 1e-5); // 外锥余弦
    // 内锥必须比外锥更窄（余弦更大），着色器按 (cos-outer)/(inner-outer) 插值
    CHECK(L.coneAngles[0] > L.coneAngles[1]);
}

TEST_CASE(lightOverflowAndRemove) {
    cLIGHT light;
    const sVEC3 pos = {0.0f, 0.0f, 1.0f};
    const sVEC3 color = {1.0f, 1.0f, 1.0f};
    for(int i = 0; i < GN_MAX_LIGHTS; ++i) {
        CHECK_EQ(light.AddPoint(pos, 5.0f, color, (float)(i + 1)), i);
    }
    CHECK_EQ(light.AddPoint(pos, 5.0f, color, 1.0f), -1); // 第 9 个被拒
    CHECK_EQ(light.aCount, (uint32_t)GN_MAX_LIGHTS);

    // 删除中间一个：后面的前移，计数与 lightCount 同步
    light.Remove(0);
    CHECK_EQ(light.aCount, (uint32_t)(GN_MAX_LIGHTS - 1));
    CHECK_NEAR(light.aData.lightCount, (float)(GN_MAX_LIGHTS - 1), 1e-6);
    CHECK_NEAR(light.aData.lights[0].color[3], 2.0f, 1e-6); // 原来的 2 号光前移到 0

    // 越界删除是空操作
    light.Remove(-1);
    light.Remove((int)light.aCount);
    CHECK_EQ(light.aCount, (uint32_t)(GN_MAX_LIGHTS - 1));
}

TEST_CASE(lightShadowCascadeConfig) {
    cLIGHT light;
    const float splits[4] = {5.0f, 12.0f, 28.0f, 60.0f};
    const float texels[4] = {0.002f, 0.006f, 0.014f, 0.030f};
    light.SetShadowCascades(4, splits, texels);
    CHECK_NEAR(light.aData.shadowParams[1], 4.0f, 1e-6);
    for (int i = 0; i < 4; ++i) {
        CHECK_NEAR(light.aData.cascadeSplits[i], splits[i], 1e-6);
        CHECK_NEAR(light.aData.shadowTexelWorld[i], texels[i], 1e-6);
    }

    // 级数被夹到 [1, GN_MAX_SHADOW_CASCADES]，未用级的 split/texel 必须清 0
    light.SetShadowCascades(0, splits, texels);
    CHECK_NEAR(light.aData.shadowParams[1], 1.0f, 1e-6);
    CHECK_NEAR(light.aData.cascadeSplits[1], 0.0f, 1e-6);
    light.SetShadowCascades(99, splits, texels);
    CHECK_NEAR(light.aData.shadowParams[1], (float)GN_MAX_SHADOW_CASCADES, 1e-6);

    light.SetShadowMapSize(4096.0f);
    CHECK_NEAR(light.aData.shadowParams[0], 4096.0f, 1e-6);
}

TEST_CASE(lightShadowLightVPPerCascade) {
    cLIGHT light;
    // 每级写一个可区分的矩阵，验证不互相覆盖（旧实现只用一个槽，4 级全用同一矩阵）
    for (int c = 0; c < 4; ++c) {
        sMAT44 m;
        std::memset(&m, 0, sizeof(m));
        for (int i = 0; i < 16; ++i) m.m[i] = (float)(c * 100 + i);
        light.SetShadowLightVP(m, (uint32_t)c);
    }
    for (int c = 0; c < 4; ++c) {
        for (int i = 0; i < 16; ++i) {
            CHECK_NEAR(light.aData.shadowLightVP[c][i], (float)(c * 100 + i), 1e-6);
        }
    }
    // 越界级号被夹到最后一级，而不是越界写
    sMAT44 m;
    std::memset(&m, 0, sizeof(m));
    m.m[0] = -1.0f;
    light.SetShadowLightVP(m, 99);
    CHECK_NEAR(light.aData.shadowLightVP[GN_MAX_SHADOW_CASCADES - 1][0], -1.0f, 1e-6);
}

TEST_CASE(lightClearResets) {
    cLIGHT light;
    const sVEC3 pos = {0.0f, 0.0f, 1.0f};
    const sVEC3 color = {1.0f, 1.0f, 1.0f};
    light.AddPoint(pos, 5.0f, color, 1.0f);
    light.SetAmbient(color, 3.0f);
    light.Clear();
    CHECK_EQ(light.aCount, 0u);
    CHECK_NEAR(light.aData.lightCount, 0.0f, 1e-6);
    CHECK_NEAR(light.aData.ambientLight[3], 1.0f, 1e-6);
    CHECK_NEAR(light.aData.shadowParams[0], 2048.0f, 1e-6);
}

TEST_CASE(lightAmbientAndIntensitySetters) {
    cLIGHT light;
    const sVEC3 pos = {0.0f, 0.0f, 1.0f};
    const sVEC3 color = {1.0f, 0.0f, 0.0f};
    light.AddPoint(pos, 5.0f, color, 1.0f);

    light.SetIntensity(0, 4.0f);
    CHECK_NEAR(light.aData.lights[0].color[3], 4.0f, 1e-6);

    light.SetColor(0, sVEC3{0.0f, 1.0f, 0.0f});
    CHECK_NEAR(light.aData.lights[0].color[1], 1.0f, 1e-6);

    light.SetPosition(0, sVEC3{3.0f, 0.0f, 0.0f});
    CHECK_NEAR(light.aData.lights[0].position[0], 3.0f, 1e-6);

    light.SetAmbient(sVEC3{0.5f, 0.5f, 0.5f}, 2.0f);
    CHECK_NEAR(light.aData.ambientLight[0], 0.5f, 1e-6);
    CHECK_NEAR(light.aData.ambientLight[3], 2.0f, 1e-6);

    // 越界 setter 是空操作（不写坏别的光源）
    light.SetIntensity(-1, 9.0f);
    light.SetIntensity(5, 9.0f);
    CHECK_NEAR(light.aData.lights[0].color[3], 4.0f, 1e-6);
}
