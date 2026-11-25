// testHiZ.cpp — Hi-Z 遮挡剔除 + Overdraw 测量（B4，纯 CPU）。
//
// 验收重点只有一条：**保守性**。
// 遮挡剔除最严重的错误是"误剔"（把可见物体剔掉 ⇒ 画面上东西凭空消失），
// 而"漏剔"只是没省下性能。所以所有用例的第一判据是：
//   若 cHIZ 说"被遮挡" ⇒ 暴力算法必须也认为"完全被遮挡"。
#include "testFramework.h"

#include <GN.h>
#include "GN_HiZ.h"

#include <cmath>
#include <cstdio>
#include <vector>

using namespace GN3d;

namespace {

// 造一个正交投影的 viewProj（世界 → NDC），方便手推屏幕坐标。
// 世界 x∈[-W/2,W/2] → NDC x∈[-1,1]；y 同理；z∈[near,far] → [0,1]。
GNg::sMAT44 makeOrtho(float halfX, float halfY, float nearZ, float farZ) {
    GNg::sMAT44 m;
    for (int i = 0; i < 16; ++i) m.m[i] = 0.0f;
    m.m[0] = 1.0f / halfX;
    m.m[5] = -1.0f / halfY; // Vulkan NDC y 朝下
    m.m[10] = 1.0f / (farZ - nearZ);
    m.m[14] = -nearZ / (farZ - nearZ);
    m.m[15] = 1.0f;
    return m;
}

// 暴力参考：物体的屏幕矩形内所有像素的 max 深度是否都 < 物体的最近深度。
// 口径与 cHIZ 一致（用 8 角点矩形），但**逐像素**而不是取 mip。
// 返回 true 表示"完全被遮挡"。
bool bruteOccluded(const GNg::sMAT44 &viewProj, const float *depth, uint32_t w, uint32_t h,
                   const sAABB &box) {
    float minX = 1e30f, maxX = -1e30f, minY = 1e30f, maxY = -1e30f, nearZ = 1e30f;
    for (int c = 0; c < 8; ++c) {
        const GNg::sVEC3 corner{(c & 1) ? box.aMax.x : box.aMin.x,
                                (c & 2) ? box.aMax.y : box.aMin.y,
                                (c & 4) ? box.aMax.z : box.aMin.z};
        const float x = viewProj.m[0] * corner.x + viewProj.m[4] * corner.y +
                        viewProj.m[8] * corner.z + viewProj.m[12];
        const float y = viewProj.m[1] * corner.x + viewProj.m[5] * corner.y +
                        viewProj.m[9] * corner.z + viewProj.m[13];
        const float z = viewProj.m[2] * corner.x + viewProj.m[6] * corner.y +
                        viewProj.m[10] * corner.z + viewProj.m[14];
        const float wc = viewProj.m[3] * corner.x + viewProj.m[7] * corner.y +
                         viewProj.m[11] * corner.z + viewProj.m[15];
        if (!(wc > 1e-6f)) return false;
        const float nx = x / wc, ny = y / wc, nz = z / wc;
        if (nz < 0.0f) return false;
        if (nx < minX) minX = nx;
        if (ny < minY) minY = ny;
        if (nz < nearZ) nearZ = nz;
        if (nx > maxX) maxX = nx;
        if (ny > maxY) maxY = ny;
    }
    float px0 = (minX * 0.5f + 0.5f) * (float)w;
    float px1 = (maxX * 0.5f + 0.5f) * (float)w;
    float py0 = (minY * 0.5f + 0.5f) * (float)h;
    float py1 = (maxY * 0.5f + 0.5f) * (float)h;
    if (px0 < 0) px0 = 0;
    if (py0 < 0) py0 = 0;
    if (px1 > (float)w) px1 = (float)w;
    if (py1 > (float)h) py1 = (float)h;
    if (px1 <= px0 || py1 <= py0) return false;

    const int x0 = (int)std::floor(px0);
    const int x1 = (int)std::ceil(px1);
    const int y0 = (int)std::floor(py0);
    const int y1 = (int)std::ceil(py1);
    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            if (x < 0 || y < 0 || x >= (int)w || y >= (int)h) return false; // 出屏：不敢判
            const float d = depth[(size_t)y * (size_t)w + (size_t)x];
            if (!(d < nearZ - 1e-5f)) return false; // 有一个像素不更近 ⇒ 不算全遮挡
        }
    }
    return true;
}

// 一张"近处一大块遮挡物 + 远处背景"的深度图
std::vector<float> makeDepthWithOccluder(uint32_t w, uint32_t h, float occluderDepth,
                                         int ox0, int oy0, int ox1, int oy1) {
    std::vector<float> d((size_t)w * (size_t)h, 1.0f); // 1.0 = 最远 = 没有几何
    for (int y = oy0; y < oy1; ++y) {
        for (int x = ox0; x < ox1; ++x) {
            if (x < 0 || y < 0 || x >= (int)w || y >= (int)h) continue;
            d[(size_t)y * (size_t)w + (size_t)x] = occluderDepth;
        }
    }
    return d;
}

} // namespace

// ── 1. 金字塔构建 ────────────────────────────────────
TEST_CASE(hizBuildsPyramidWithMax) {
    const uint32_t w = 8, h = 8;
    std::vector<float> depth((size_t)w * h, 0.5f);
    depth[0] = 0.9f; // 一个角落更深
    cHIZ hiz;
    hiz.Build(&depth[0], w, h);
    CHECK(hiz.IsBuilt());
    // 8×8 → 4×4 → 2×2 → 1×1，共 4 级
    CHECK_EQ(hiz.LevelCount(), 4u);
    CHECK_EQ(hiz.LevelWidth(0), 8u);
    CHECK_EQ(hiz.LevelWidth(1), 4u);
    CHECK_EQ(hiz.LevelWidth(2), 2u);
    CHECK_EQ(hiz.LevelWidth(3), 1u);
    // 取 max：那个 0.9 会一路传到最顶层
    CHECK_NEAR(hiz.DepthAt(3, 0, 0), 0.9f, 1e-6);
    CHECK_NEAR(hiz.DepthAt(0, 0, 0), 0.9f, 1e-6);
    CHECK_NEAR(hiz.DepthAt(0, 1, 0), 0.5f, 1e-6);
}

TEST_CASE(hizHandlesNonPowerOfTwo) {
    const uint32_t w = 5, h = 3;
    std::vector<float> depth((size_t)w * h, 0.25f);
    cHIZ hiz;
    hiz.Build(&depth[0], w, h);
    CHECK(hiz.IsBuilt());
    CHECK_EQ(hiz.LevelWidth(0), 5u);
    CHECK_EQ(hiz.LevelWidth(1), 3u); // ceil(5/2)
    CHECK_EQ(hiz.LevelHeight(1), 2u); // ceil(3/2)
    CHECK_EQ(hiz.LevelWidth(2), 2u);
    CHECK_EQ(hiz.LevelWidth(3), 1u);
}

TEST_CASE(hizRejectsInvalidInput) {
    cHIZ hiz;
    hiz.Build(nullptr, 8, 8);
    CHECK(!hiz.IsBuilt());
    hiz.Build(nullptr, 0, 0);
    CHECK(!hiz.IsBuilt());
    float d[4] = {0.1f, 0.2f, 0.3f, 0.4f};
    hiz.Build(d, 0, 2);
    CHECK(!hiz.IsBuilt());
    // 越界访问安全
    CHECK_NEAR(hiz.DepthAt(0, 0, 0), 1.0f, 1e-6);
    CHECK_NEAR(hiz.DepthAt(99, 0, 0), 1.0f, 1e-6);
}

// ── 2. 遮挡判定：被完全遮挡 ───────────────────────────
TEST_CASE(hizDetectsFullyOccludedBox) {
    const uint32_t w = 64, h = 64;
    // 整屏覆盖一层深 0.5 的遮挡物
    std::vector<float> depth = makeDepthWithOccluder(w, h, 0.5f, 0, 0, (int)w, (int)h);
    cHIZ hiz;
    hiz.Build(&depth[0], w, h);

    const GNg::sMAT44 vp = makeOrtho(10.0f, 10.0f, 1.0f, 100.0f);
    // 一个在 z = 50（NDC z = (50-1)/99 ≈ 0.495）附近的小盒子 ⇒ 比遮挡物近 ⇒ 可见
    sAABB visible;
    visible.aMin = GNg::sVEC3{-1.0f, -1.0f, 50.0f};
    visible.aMax = GNg::sVEC3{1.0f, 1.0f, 52.0f};
    CHECK(!hiz.IsOccluded(vp, w, h, visible));

    // 一个在 z = 90（NDC z ≈ 0.899）的盒子 ⇒ 比 0.5 更远 ⇒ 被遮挡
    sAABB hidden;
    hidden.aMin = GNg::sVEC3{-1.0f, -1.0f, 90.0f};
    hidden.aMax = GNg::sVEC3{1.0f, 1.0f, 92.0f};
    CHECK(hiz.IsOccluded(vp, w, h, hidden));
}

TEST_CASE(hizDoesNotOccludeWhenPartiallyVisible) {
    const uint32_t w = 64, h = 64;
    // 遮挡物只覆盖左半边
    std::vector<float> depth = makeDepthWithOccluder(w, h, 0.5f, 0, 0, (int)w / 2, (int)h);
    cHIZ hiz;
    hiz.Build(&depth[0], w, h);

    const GNg::sMAT44 vp = makeOrtho(10.0f, 10.0f, 1.0f, 100.0f);
    // 盒子横跨整屏 ⇒ 右半边可见 ⇒ **不能**判遮挡
    sAABB box;
    box.aMin = GNg::sVEC3{-5.0f, -5.0f, 90.0f};
    box.aMax = GNg::sVEC3{5.0f, 5.0f, 92.0f};
    CHECK(!hiz.IsOccluded(vp, w, h, box));
    // 对照：暴力算法也认为不完全遮挡
    CHECK(!bruteOccluded(vp, &depth[0], w, h, box));
}

// ── 3. 保守性对拍（核心）────────────────────────────
TEST_CASE(hizNeverOccludesWhatBruteSaysVisible) {
    const uint32_t w = 48, h = 32;
    const GNg::sMAT44 vp = makeOrtho(8.0f, 6.0f, 1.0f, 50.0f);

    // 用规则网格的遮挡物（模拟一堵有缝隙的墙）
    std::vector<float> depth((size_t)w * h, 1.0f);
    for (uint32_t y = 4; y < h - 4; y += 6) {
        for (uint32_t x = 4; x < w - 4; x += 6) {
            for (uint32_t dy = 0; dy < 4; ++dy) {
                for (uint32_t dx = 0; dx < 4; ++dx) {
                    depth[(size_t)(y + dy) * w + (x + dx)] = 0.4f;
                }
            }
        }
    }
    cHIZ hiz;
    hiz.Build(&depth[0], w, h);

    // 扫一批不同位置/大小的盒子，逐例对拍
    uint32_t occludedCount = 0;
    uint32_t checked = 0;
    for (int zi = 0; zi < 6; ++zi) {
        for (int xi = 0; xi < 7; ++xi) {
            for (int yi = 0; yi < 5; ++yi) {
                const float z = 10.0f + (float)zi * 6.0f;
                const float cx = -6.0f + (float)xi * 2.0f;
                const float cy = -4.0f + (float)yi * 2.0f;
                sAABB box;
                box.aMin = GNg::sVEC3{cx - 0.8f, cy - 0.8f, z};
                box.aMax = GNg::sVEC3{cx + 0.8f, cy + 0.8f, z + 1.2f};
                const bool hizSays = hiz.IsOccluded(vp, w, h, box);
                const bool bruteSays = bruteOccluded(vp, &depth[0], w, h, box);
                ++checked;
                // **核心不变量**：Hi-Z 说遮挡 ⇒ 暴力必须也说遮挡
                if (hizSays) {
                    CHECK(bruteSays);
                    ++occludedCount;
                }
            }
        }
    }
    std::fprintf(stderr, "[hiz] 对拍(有缝隙墙) %u 个盒子，其中 %u 个被判遮挡\n", checked,
                 occludedCount);
    CHECK(checked > 100u);
    // 这组用例**故意**是"有缝隙的墙"，所以 occludedCount 可能是 0 ——
    // 它只验证"不误剔"。真正验证"该剔的时候确实剔"是下一条用例。
}

// **这一条才验证"遮挡判定真的生效"**。
//
// 上面那条用"有缝隙的墙"，结果 0 个被判遮挡 —— 断言虽然通过，但**没跑到**遮挡分支，
// 是典型的"空真"（vacuous truth）。测试必须证明它真的覆盖到目标行为。
TEST_CASE(hizOccludesSolidWallAndAgreesWithBrute) {
    const uint32_t w = 48, h = 32;
    const GNg::sMAT44 vp = makeOrtho(8.0f, 6.0f, 1.0f, 50.0f);
    // **整屏**覆盖一层深 0.4 的遮挡物
    std::vector<float> depth((size_t)w * h, 0.4f);
    cHIZ hiz;
    hiz.Build(&depth[0], w, h);

    uint32_t occludedCount = 0;
    uint32_t checked = 0;
    uint32_t visibleCount = 0;
    for (int zi = 0; zi < 6; ++zi) {
        for (int xi = 0; xi < 7; ++xi) {
            const float z = 5.0f + (float)zi * 7.0f;
            const float cx = -6.0f + (float)xi * 2.0f;
            sAABB box;
            box.aMin = GNg::sVEC3{cx - 0.8f, -2.0f, z};
            box.aMax = GNg::sVEC3{cx + 0.8f, 2.0f, z + 1.2f};
            const bool hizSays = hiz.IsOccluded(vp, w, h, box);
            const bool bruteSays = bruteOccluded(vp, &depth[0], w, h, box);
            ++checked;
            if (hizSays) {
                // Hi-Z 说遮挡 ⇒ 暴力必须也说遮挡（核心不变量）
                CHECK(bruteSays);
                ++occludedCount;
            } else {
                ++visibleCount;
            }
        }
    }
    std::fprintf(stderr, "[hiz] 对拍(实心墙) %u 个盒子：%u 个判遮挡、%u 个判可见\n", checked,
                 occludedCount, visibleCount);
    // **必须真的剔掉一些**，否则这条用例又是空真
    CHECK(occludedCount > 10u);
    // 也必须有可见的（说明不是无脑全剔）
    CHECK(visibleCount > 0u);
}

// ── 4. 边界：未建树/空盒/出屏 ─────────────────────────
TEST_CASE(hizSafeOnEdgeCases) {
    const GNg::sMAT44 vp = makeOrtho(10.0f, 10.0f, 1.0f, 100.0f);
    cHIZ hiz;
    sAABB box;
    box.aMin = GNg::sVEC3{-1, -1, 10};
    box.aMax = GNg::sVEC3{1, 1, 12};
    // 未建树 ⇒ 一律不遮挡
    CHECK(!hiz.IsOccluded(vp, 64, 64, box));

    const uint32_t w = 16, h = 16;
    std::vector<float> depth((size_t)w * h, 0.9f);
    hiz.Build(&depth[0], w, h);
    // 视口为 0 ⇒ 不遮挡
    CHECK(!hiz.IsOccluded(vp, 0, 0, box));
    // 空盒 ⇒ 不遮挡
    sAABB empty;
    CHECK(!hiz.IsOccluded(vp, w, h, empty));
    // 完全在屏幕外（远高于视口）⇒ 不遮挡（交给视锥剔除）
    sAABB outside;
    outside.aMin = GNg::sVEC3{-100, -100, 10};
    outside.aMax = GNg::sVEC3{-90, -90, 12};
    CHECK(!hiz.IsOccluded(vp, w, h, outside));
    // 跨越近平面（z < near 的部分）⇒ 不遮挡（保守）
    sAABB nearCross;
    nearCross.aMin = GNg::sVEC3{-1, -1, -5.0f};
    nearCross.aMax = GNg::sVEC3{1, 1, 5.0f};
    CHECK(!hiz.IsOccluded(vp, w, h, nearCross));
}

// ── 5. Overdraw 测量 ────────────────────────────────
TEST_CASE(overdrawMeasuresSingleLayer) {
    const GNg::sMAT44 vp = makeOrtho(10.0f, 10.0f, 1.0f, 100.0f);
    // 一个覆盖全屏的盒子
    sAABB box;
    box.aMin = GNg::sVEC3{-10, -10, 50};
    box.aMax = GNg::sVEC3{10, 10, 52};
    const sOVERDRAW_STATS st = measureOverdraw(vp, 32, 32, &box, 1);
    CHECK_EQ(st.aObjects, 1u);
    CHECK_NEAR(st.OverdrawRatio(), 1.0f, 0.15f); // 单层 ≈ 1.0
}

TEST_CASE(overdrawDoublesWithTwoOverlappingBoxes) {
    const GNg::sMAT44 vp = makeOrtho(10.0f, 10.0f, 1.0f, 100.0f);
    sAABB boxes[2];
    boxes[0].aMin = GNg::sVEC3{-10, -10, 50};
    boxes[0].aMax = GNg::sVEC3{10, 10, 52};
    boxes[1].aMin = GNg::sVEC3{-10, -10, 60};
    boxes[1].aMax = GNg::sVEC3{10, 10, 62};
    const sOVERDRAW_STATS st = measureOverdraw(vp, 32, 32, boxes, 2);
    CHECK_EQ(st.aObjects, 2u);
    CHECK_NEAR(st.OverdrawRatio(), 2.0f, 0.2f); // 两层 ≈ 2.0
}

TEST_CASE(overdrawZeroForEmptyInput) {
    const GNg::sMAT44 vp = makeOrtho(10.0f, 10.0f, 1.0f, 100.0f);
    sAABB box;
    box.aMin = GNg::sVEC3{-1, -1, 50};
    box.aMax = GNg::sVEC3{1, 1, 52};
    CHECK_NEAR(measureOverdraw(vp, 32, 32, nullptr, 0).OverdrawRatio(), 0.0f, 1e-6);
    CHECK_NEAR(measureOverdraw(vp, 0, 0, &box, 1).OverdrawRatio(), 0.0f, 1e-6);
    // 超大视口：直接返回全 0（不打爆内存）
    CHECK_NEAR(measureOverdraw(vp, 8192, 8192, &box, 1).OverdrawRatio(), 0.0f, 1e-6);
}

// 不重叠的盒子：overdraw 仍 ≈ 1
TEST_CASE(overdrawOneForDisjointBoxes) {
    const GNg::sMAT44 vp = makeOrtho(10.0f, 10.0f, 1.0f, 100.0f);
    sAABB boxes[2];
    boxes[0].aMin = GNg::sVEC3{-9, -9, 50};
    boxes[0].aMax = GNg::sVEC3{-4, -4, 52};
    boxes[1].aMin = GNg::sVEC3{4, 4, 50};
    boxes[1].aMax = GNg::sVEC3{9, 9, 52};
    const sOVERDRAW_STATS st = measureOverdraw(vp, 64, 64, boxes, 2);
    CHECK_EQ(st.aObjects, 2u);
    CHECK_NEAR(st.OverdrawRatio(), 1.0f, 0.1f);
}
