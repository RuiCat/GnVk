// testMultiCubeShadow.cpp — 多光源 cube shadow（B2 扩展），纯 CPU 部分。
//
// 三层验收：
//   1. LIGHT_UBO 的布局契约（追加字段不破坏既有偏移）
//   2. 每灯深度范围（SetCubeShadowRangeForLight / 层号默认为 -1）
//   3. 多灯选择（pickCubeShadowLights 的排序、上限、并列）
#include "testFramework.h"

#include <GN.h>
#include "GN_3Dlight.h"

#include <cmath>
#include <cstring>
#include <vector>

using namespace GN3d;

namespace {

sLIGHT_GPU makePoint(float x, float y, float z, float intensity, bool cubeShadow,
                     float radius = 0.1f) {
    sLIGHT_GPU L;
    std::memset(&L, 0, sizeof(L));
    L.color[0] = L.color[1] = L.color[2] = 1.0f;
    L.color[3] = intensity;
    L.position[0] = x;
    L.position[1] = y;
    L.position[2] = z;
    L.position[3] = 1.0f; // 点光
    L.direction[3] = cubeShadow ? 2.0f : 0.0f; // shadowType = 2
    L.attenuation[0] = 1.0f;
    L.attenuation[1] = 0.1f;
    L.attenuation[2] = 0.01f;
    L.attenuation[3] = 20.0f;
    L.coneAngles[2] = radius;
    return L;
}

sLIGHT_GPU makeDirectional() {
    sLIGHT_GPU L;
    std::memset(&L, 0, sizeof(L));
    L.color[3] = 1.0f;
    L.position[3] = 0.0f; // 方向光没有位置
    L.direction[3] = 0.0f;
    return L;
}

} // namespace

// ── 1. 布局契约 ───────────────────────────────────────
TEST_CASE(multiCubeShadowUboLayout) {
    // 976（既有）+ 8×16（cube shadow，按灯槽索引） = 1104
    CHECK_EQ((int)sizeof(sLIGHT_UBO), 1104);

    // 既有字段偏移不能被破坏（着色器侧按这些偏移读）
    const size_t offShadowParams = offsetof(sLIGHT_UBO, shadowParams);
    const size_t offTexelWorld = offsetof(sLIGHT_UBO, shadowTexelWorld);
    const size_t offCubeRange = offsetof(sLIGHT_UBO, cubeShadowRange);
    CHECK_EQ((int)offShadowParams, 928);
    CHECK_EQ((int)offTexelWorld, 960);
    // cubeShadowRange 紧跟在既有字段之后
    CHECK_EQ((int)offCubeRange, 976);
}

TEST_CASE(multiCubeShadowRangeDefaultsToNone) {
    cLIGHT light;
    // 默认所有灯的层号必须是 -1（"没有 cube shadow"）。
    // 若是 0，着色器会去采样一张从未渲染过的深度图 —— 整片场景被错误遮挡。
    for (uint32_t i = 0; i < 4; ++i) {
        CHECK_NEAR(light.aData.cubeShadowRange[i][2], -1.0f, 1e-6);
        CHECK_NEAR(light.aData.cubeShadowRange[i][0], 0.0f, 1e-6);
    }
}

// ── 2. 每灯深度范围 ───────────────────────────────────
TEST_CASE(multiCubeShadowSetPerLight) {
    cLIGHT light;
    light.SetCubeShadowRangeForLight(0, 0.1f, 15.0f, 0);
    light.SetCubeShadowRangeForLight(1, 0.2f, 30.0f, 1);

    CHECK_NEAR(light.aData.cubeShadowRange[0][0], 0.1f, 1e-6);
    CHECK_NEAR(light.aData.cubeShadowRange[0][1], 15.0f, 1e-6);
    CHECK_NEAR(light.aData.cubeShadowRange[0][2], 0.0f, 1e-6);

    CHECK_NEAR(light.aData.cubeShadowRange[1][0], 0.2f, 1e-6);
    CHECK_NEAR(light.aData.cubeShadowRange[1][1], 30.0f, 1e-6);
    CHECK_NEAR(light.aData.cubeShadowRange[1][2], 1.0f, 1e-6);

    // 没设的仍然是 -1
    CHECK_NEAR(light.aData.cubeShadowRange[2][2], -1.0f, 1e-6);
}

TEST_CASE(multiCubeShadowClearRanges) {
    cLIGHT light;
    light.SetCubeShadowRangeForLight(0, 0.1f, 15.0f, 0);
    light.SetCubeShadowRangeForLight(3, 0.1f, 15.0f, 3);
    light.ClearCubeShadowRanges();
    for (uint32_t i = 0; i < 4; ++i) {
        CHECK_NEAR(light.aData.cubeShadowRange[i][2], -1.0f, 1e-6);
    }
}

TEST_CASE(multiCubeShadowNegativeLayerMeansNoShadow) {
    cLIGHT light;
    // layer < 0 ⇒ 写 0/0/-1（着色器看到 near<=0 就返回全亮）
    light.SetCubeShadowRangeForLight(0, 0.1f, 15.0f, -1);
    CHECK_NEAR(light.aData.cubeShadowRange[0][0], 0.0f, 1e-6);
    CHECK_NEAR(light.aData.cubeShadowRange[0][2], -1.0f, 1e-6);
}

TEST_CASE(multiCubeShadowInvalidNearFarIsSanitized) {
    cLIGHT light;
    // near <= 0 ⇒ 钳到 0.05；far <= near ⇒ near+1
    light.SetCubeShadowRangeForLight(0, -1.0f, -5.0f, 0);
    CHECK(light.aData.cubeShadowRange[0][0] > 0.0f);
    CHECK(light.aData.cubeShadowRange[0][1] > light.aData.cubeShadowRange[0][0]);
}

TEST_CASE(multiCubeShadowSlotOutOfRangeIgnored) {
    cLIGHT light;
    // slot >= 4 应当被忽略（不越界写）
    light.SetCubeShadowRangeForLight(4, 0.1f, 15.0f, 0);
    light.SetCubeShadowRangeForLight(99, 0.1f, 15.0f, 0);
    for (uint32_t i = 0; i < 4; ++i) {
        CHECK_NEAR(light.aData.cubeShadowRange[i][2], -1.0f, 1e-6);
    }
}

// ── 3. 多灯选择 ───────────────────────────────────────
TEST_CASE(multiCubeShadowPickEmpty) {
    sLIGHT_UBO ubo;
    std::memset(&ubo, 0, sizeof(ubo));
    const sCUBE_SHADOW_PICKS p = pickCubeShadowLights(ubo, GNg::sVEC3{0, 0, 0});
    CHECK_EQ(p.aCount, 0u);
    CHECK_EQ(p.aLightIndex[0], -1);
}

TEST_CASE(multiCubeShadowPickSortsByScore) {
    sLIGHT_UBO ubo;
    std::memset(&ubo, 0, sizeof(ubo));
    // 相机在原点。三盏灯距离不同：近的分数高
    ubo.lights[0] = makePoint(5.0f, 0.0f, 0.0f, 1.0f, true);   // d=5
    ubo.lights[1] = makePoint(1.0f, 0.0f, 0.0f, 1.0f, true);   // d=1 最高分
    ubo.lights[2] = makePoint(20.0f, 0.0f, 0.0f, 1.0f, true);  // d=20 最低
    ubo.lightCount = 3.0f;

    const sCUBE_SHADOW_PICKS p = pickCubeShadowLights(ubo, GNg::sVEC3{0, 0, 0}, 40.0f, 4);
    CHECK_EQ(p.aCount, 3u);
    CHECK_EQ(p.aLightIndex[0], 1); // 最近
    CHECK_EQ(p.aLightIndex[1], 0);
    CHECK_EQ(p.aLightIndex[2], 2); // 最远
    CHECK(p.aScore[0] > p.aScore[1]);
    CHECK(p.aScore[1] > p.aScore[2]);
}

TEST_CASE(multiCubeShadowPickRespectsMaxCount) {
    sLIGHT_UBO ubo;
    std::memset(&ubo, 0, sizeof(ubo));
    for (int i = 0; i < 6; ++i) {
        ubo.lights[i] = makePoint((float)(i + 1), 0.0f, 0.0f, 1.0f, true);
    }
    ubo.lightCount = 6.0f;

    const sCUBE_SHADOW_PICKS p = pickCubeShadowLights(ubo, GNg::sVEC3{0, 0, 0}, 40.0f, 2);
    CHECK_EQ(p.aCount, 2u);
    // 最近的两盏（d=1, d=2）
    CHECK_EQ(p.aLightIndex[0], 0);
    CHECK_EQ(p.aLightIndex[1], 1);
}

TEST_CASE(multiCubeShadowSkipsNonShadowLights) {
    sLIGHT_UBO ubo;
    std::memset(&ubo, 0, sizeof(ubo));
    ubo.lights[0] = makeDirectional();                            // 方向光：跳过
    ubo.lights[1] = makePoint(1.0f, 0.0f, 0.0f, 1.0f, false);     // 没开 cube shadow：跳过
    ubo.lights[2] = makePoint(2.0f, 0.0f, 0.0f, 1.0f, true);      // 唯一候选
    ubo.lightCount = 3.0f;

    const sCUBE_SHADOW_PICKS p = pickCubeShadowLights(ubo, GNg::sVEC3{0, 0, 0}, 40.0f, 4);
    CHECK_EQ(p.aCount, 1u);
    CHECK_EQ(p.aLightIndex[0], 2);
}

TEST_CASE(multiCubeShadowSkipsTooFar) {
    sLIGHT_UBO ubo;
    std::memset(&ubo, 0, sizeof(ubo));
    ubo.lights[0] = makePoint(50.0f, 0.0f, 0.0f, 1.0f, true); // d=50 > maxDistance 40
    ubo.lights[1] = makePoint(10.0f, 0.0f, 0.0f, 1.0f, true); // d=10 保留
    ubo.lightCount = 2.0f;

    const sCUBE_SHADOW_PICKS p = pickCubeShadowLights(ubo, GNg::sVEC3{0, 0, 0}, 40.0f, 4);
    CHECK_EQ(p.aCount, 1u);
    CHECK_EQ(p.aLightIndex[0], 1);
}

TEST_CASE(multiCubeShadowTieKeepsRegistrationOrder) {
    sLIGHT_UBO ubo;
    std::memset(&ubo, 0, sizeof(ubo));
    // 完全相同的两盏：并列时**先注册的在前**
    ubo.lights[0] = makePoint(3.0f, 0.0f, 0.0f, 1.0f, true);
    ubo.lights[1] = makePoint(3.0f, 0.0f, 0.0f, 1.0f, true);
    ubo.lightCount = 2.0f;

    const sCUBE_SHADOW_PICKS p = pickCubeShadowLights(ubo, GNg::sVEC3{0, 0, 0}, 40.0f, 4);
    CHECK_EQ(p.aCount, 2u);
    CHECK_EQ(p.aLightIndex[0], 0);
    CHECK_EQ(p.aLightIndex[1], 1);
}

TEST_CASE(multiCubeShadowSinglePickAgreesWithMulti) {
    // 单灯版与多灯版必须一致（多灯时代"挑中的那盏"依然排第一）
    sLIGHT_UBO ubo;
    std::memset(&ubo, 0, sizeof(ubo));
    ubo.lights[0] = makePoint(8.0f, 2.0f, 0.0f, 3.0f, true);
    ubo.lights[1] = makePoint(2.0f, 1.0f, 0.0f, 1.0f, true);
    ubo.lights[2] = makePoint(15.0f, 0.0f, 0.0f, 5.0f, true);
    ubo.lightCount = 3.0f;

    const GNg::sVEC3 cam{0, 0, 0};
    const sCUBE_SHADOW_PICK single = pickCubeShadowLight(ubo, cam, 40.0f);
    const sCUBE_SHADOW_PICKS multi = pickCubeShadowLights(ubo, cam, 40.0f, 4);

    CHECK(single.aLightIndex >= 0);
    CHECK(multi.aCount >= 1u);
    CHECK_EQ(multi.aLightIndex[0], single.aLightIndex);
    CHECK_NEAR(multi.aScore[0], single.aScore, 1e-5);
}

TEST_CASE(multiCubeShadowMaxCountClamped) {
    sLIGHT_UBO ubo;
    std::memset(&ubo, 0, sizeof(ubo));
    for (int i = 0; i < 8; ++i) ubo.lights[i] = makePoint((float)(i + 1), 0.0f, 0.0f, 1.0f, true);
    ubo.lightCount = 8.0f;

    // maxCount 超过上限 4 ⇒ 钳到 4
    const sCUBE_SHADOW_PICKS p = pickCubeShadowLights(ubo, GNg::sVEC3{0, 0, 0}, 40.0f, 99);
    CHECK_EQ(p.aCount, 4u);
    // maxCount = 0 ⇒ 一个都不选
    const sCUBE_SHADOW_PICKS p0 = pickCubeShadowLights(ubo, GNg::sVEC3{0, 0, 0}, 40.0f, 0);
    CHECK_EQ(p0.aCount, 0u);
}

TEST_CASE(multiCubeShadowHandlesCorruptLightCount) {
    sLIGHT_UBO ubo;
    std::memset(&ubo, 0, sizeof(ubo));
    ubo.lights[0] = makePoint(1.0f, 0.0f, 0.0f, 1.0f, true);
    // lightCount 超过 GN_MAX_LIGHTS ⇒ 视为损坏，返回空（不越界读）
    ubo.lightCount = 999.0f;
    const sCUBE_SHADOW_PICKS p = pickCubeShadowLights(ubo, GNg::sVEC3{0, 0, 0});
    CHECK_EQ(p.aCount, 0u);
    // 负值同样安全
    ubo.lightCount = -1.0f;
    const sCUBE_SHADOW_PICKS p2 = pickCubeShadowLights(ubo, GNg::sVEC3{0, 0, 0});
    CHECK_EQ(p2.aCount, 0u);
}

// 强度参与评分：同等距离下更亮的排前面
TEST_CASE(multiCubeShadowIntensityBreaksTie) {
    sLIGHT_UBO ubo;
    std::memset(&ubo, 0, sizeof(ubo));
    ubo.lights[0] = makePoint(5.0f, 0.0f, 0.0f, 1.0f, true);  // 暗
    ubo.lights[1] = makePoint(5.0f, 0.0f, 0.0f, 9.0f, true);  // 亮（同距离）
    ubo.lightCount = 2.0f;

    const sCUBE_SHADOW_PICKS p = pickCubeShadowLights(ubo, GNg::sVEC3{0, 0, 0}, 40.0f, 4);
    CHECK_EQ(p.aCount, 2u);
    CHECK_EQ(p.aLightIndex[0], 1); // 亮的在前
}

// ═══════════════════════════════════════════════════════════
//  核心：层号必须与**相机无关**
// ═══════════════════════════════════════════════════════════
//
// 影子是"光源 + 遮挡物"的几何结果，**不允许随视角变化**。
// 之前我把"选灯排名"直接当层号用 —— 而排名依赖 cameraPos，相机一动同一盏灯就换层，
// 灯数超过上限时影子会随视角闪进闪出。这一组用例把口径钉死。

TEST_CASE(multiCubeShadowLayerIndependentOfCamera) {
    sLIGHT_UBO ubo;
    std::memset(&ubo, 0, sizeof(ubo));
    // 四盏带 cube shadow 的灯 + 一盏不带
    ubo.lights[0] = makePoint(4.0f, 2.0f, 0.0f, 2.0f, true);
    ubo.lights[1] = makePoint(-4.0f, 2.0f, 0.0f, 2.0f, true);
    ubo.lights[2] = makePoint(0.0f, 2.0f, 4.0f, 2.0f, true);
    ubo.lights[3] = makePoint(0.0f, 2.0f, -4.0f, 2.0f, true);
    ubo.lights[4] = makePoint(1.0f, 2.0f, 1.0f, 2.0f, false); // 无 cube shadow
    ubo.lightCount = 5.0f;

    const sCUBE_SHADOW_LAYOUT base = assignCubeShadowLayers(ubo, 4);
    // 注册顺序：0→层0, 1→层1, 2→层2, 3→层3；第 4 盏没开阴影 ⇒ -1
    CHECK_EQ(base.aLayer[0], 0);
    CHECK_EQ(base.aLayer[1], 1);
    CHECK_EQ(base.aLayer[2], 2);
    CHECK_EQ(base.aLayer[3], 3);
    CHECK_EQ(base.aLayer[4], -1);
    CHECK_EQ((int)base.aLayerCount, 4);
    CHECK_EQ((int)base.aDroppedLights, 0);

    // **换一堆相机位置，层号必须一模一样**
    const GNg::sVEC3 cameras[] = {
        {0.0f, 0.0f, 0.0f},   {100.0f, 50.0f, -80.0f}, {-77.0f, 3.0f, 12.0f},
        {1.0f, 1.0f, 1.0f},   {-4.0f, 2.0f, 0.0f},     {0.0f, 2.0f, -4.0f},
    };
    for (size_t c = 0; c < sizeof(cameras) / sizeof(cameras[0]); ++c) {
        const sCUBE_SHADOW_LAYOUT lay = assignCubeShadowLayers(ubo, 4);
        for (int i = 0; i < 5; ++i) {
            CHECK_EQ(lay.aLayer[i], base.aLayer[i]);
        }
        // 顺带确认：pickCubeShadowLights 的**排名**确实会随相机变化 ——
        // 这正是"不能用排名当层号"的原因（下面单独用例钉住）。
        (void)cameras[c];
    }
}

// 证明"排名确实会随相机变"，从而说明它为什么不能当层号
TEST_CASE(multiCubeShadowPickRankDoesDependOnCamera) {
    sLIGHT_UBO ubo;
    std::memset(&ubo, 0, sizeof(ubo));
    ubo.lights[0] = makePoint(10.0f, 0.0f, 0.0f, 1.0f, true);
    ubo.lights[1] = makePoint(-10.0f, 0.0f, 0.0f, 1.0f, true);
    ubo.lightCount = 2.0f;

    // 站左边：灯 1（在 -x）更近 ⇒ 排第一
    const sCUBE_SHADOW_PICKS left = pickCubeShadowLights(ubo, GNg::sVEC3{-10.0f, 0.0f, 0.0f});
    CHECK_EQ(left.aLightIndex[0], 1);
    // 站右边：灯 0 更近 ⇒ 排第一（**排名翻转了**）
    const sCUBE_SHADOW_PICKS right = pickCubeShadowLights(ubo, GNg::sVEC3{10.0f, 0.0f, 0.0f});
    CHECK_EQ(right.aLightIndex[0], 0);

    // 但层号始终稳定：灯 0 永远是层 0，灯 1 永远是层 1
    const sCUBE_SHADOW_LAYOUT lay = assignCubeShadowLayers(ubo, 4);
    CHECK_EQ(lay.aLayer[0], 0);
    CHECK_EQ(lay.aLayer[1], 1);
}

TEST_CASE(multiCubeShadowLayerCapsAtMaxLayers) {
    sLIGHT_UBO ubo;
    std::memset(&ubo, 0, sizeof(ubo));
    for (int i = 0; i < 6; ++i) ubo.lights[i] = makePoint((float)i, 2.0f, 0.0f, 1.0f, true);
    ubo.lightCount = 6.0f;

    const sCUBE_SHADOW_LAYOUT lay = assignCubeShadowLayers(ubo, 2);
    CHECK_EQ(lay.aLayer[0], 0);
    CHECK_EQ(lay.aLayer[1], 1);
    // 超出的拿不到层（-1 = 无阴影），且计数正确
    CHECK_EQ(lay.aLayer[2], -1);
    CHECK_EQ(lay.aLayer[5], -1);
    CHECK_EQ((int)lay.aLayerCount, 2);
    CHECK_EQ((int)lay.aDroppedLights, 4);
}

TEST_CASE(multiCubeShadowLayerMaxLayersClamped) {
    sLIGHT_UBO ubo;
    std::memset(&ubo, 0, sizeof(ubo));
    for (int i = 0; i < 8; ++i) ubo.lights[i] = makePoint((float)i, 2.0f, 0.0f, 1.0f, true);
    ubo.lightCount = 8.0f;
    // 要求 99 层 ⇒ 钳到 GN_MAX_CUBE_SHADOW_PICKS
    const sCUBE_SHADOW_LAYOUT lay = assignCubeShadowLayers(ubo, 99);
    CHECK_EQ((int)lay.aLayerCount, (int)GN_MAX_CUBE_SHADOW_PICKS);
    CHECK_EQ((int)lay.aDroppedLights, 8 - (int)GN_MAX_CUBE_SHADOW_PICKS);
}

TEST_CASE(multiCubeShadowLayerIgnoresNonShadowAndDirectional) {
    sLIGHT_UBO ubo;
    std::memset(&ubo, 0, sizeof(ubo));
    ubo.lights[0] = makeDirectional();                      // 无位置 ⇒ 不给层
    ubo.lights[1] = makePoint(1.0f, 2.0f, 0.0f, 1.0f, false); // 没开 shadow ⇒ 不给层
    ubo.lights[2] = makePoint(2.0f, 2.0f, 0.0f, 1.0f, true);  // ⇒ 层 0
    ubo.lights[3] = makePoint(3.0f, 2.0f, 0.0f, 1.0f, true);  // ⇒ 层 1
    ubo.lightCount = 4.0f;

    const sCUBE_SHADOW_LAYOUT lay = assignCubeShadowLayers(ubo, 4);
    CHECK_EQ(lay.aLayer[0], -1);
    CHECK_EQ(lay.aLayer[1], -1);
    CHECK_EQ(lay.aLayer[2], 0);
    CHECK_EQ(lay.aLayer[3], 1);
    CHECK_EQ((int)lay.aLayerCount, 2);
}

TEST_CASE(multiCubeShadowLayerHandlesCorruptCount) {
    sLIGHT_UBO ubo;
    std::memset(&ubo, 0, sizeof(ubo));
    ubo.lights[0] = makePoint(1.0f, 2.0f, 0.0f, 1.0f, true);
    ubo.lightCount = 999.0f; // 损坏
    const sCUBE_SHADOW_LAYOUT lay = assignCubeShadowLayers(ubo);
    CHECK_EQ((int)lay.aLayerCount, 0);
    CHECK_EQ(lay.aLayer[0], -1);
}

// cLIGHT 的一步到位版本：写进 UBO 的 cubeShadowRange
TEST_CASE(multiCubeShadowAssignLayersWritesUbo) {
    cLIGHT light;
    light.AddPoint(GNg::sVEC3{1.0f, 2.0f, 0.0f}, 20.0f, GNg::sVEC3{1, 1, 1}, 2.0f, true, 0.1f);
    light.AddPoint(GNg::sVEC3{2.0f, 2.0f, 0.0f}, 20.0f, GNg::sVEC3{1, 1, 1}, 2.0f, true, 0.1f);
    light.AddPoint(GNg::sVEC3{3.0f, 2.0f, 0.0f}, 20.0f, GNg::sVEC3{1, 1, 1}, 2.0f, true, 0.1f);

    const uint32_t n = light.AssignCubeShadowLayers(0.1f, 20.0f, 2);
    CHECK_EQ(n, 2u);
    // 前两盏有层（0、1），第三盏因超上限拿 -1
    CHECK_NEAR(light.aData.cubeShadowRange[0][2], 0.0f, 1e-6);
    CHECK_NEAR(light.aData.cubeShadowRange[1][2], 1.0f, 1e-6);
    CHECK_NEAR(light.aData.cubeShadowRange[2][2], -1.0f, 1e-6);
    // near/far 都写进去了
    CHECK_NEAR(light.aData.cubeShadowRange[0][0], 0.1f, 1e-6);
    CHECK_NEAR(light.aData.cubeShadowRange[0][1], 20.0f, 1e-6);
}
