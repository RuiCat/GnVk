// tests/unit/testShadowCascade.cpp — L3c CSM CPU 单测：分割单调/级数钳制/矩阵有限且不退化/
// texel snapping 不改变投影中心覆盖（契约 docs/L4_L3_CONTRACT.md §8）。
#include "testFramework.h"

#include "vkShadowCascade.h"

#include <cmath>

using namespace GN3d;

namespace {

// 相机在原点、看向 -Z 的视图矩阵（与查看器 BuildViewProj 的视图部分同构）。
GNg::sMAT44 makeTestCameraView() {
    GNg::sMAT44 view;
    const GNg::sVEC3 eye = {0.0f, 0.0f, 0.0f};
    const GNg::sVEC3 target = {0.0f, 0.0f, -1.0f};
    const GNg::sVEC3 up = {0.0f, 1.0f, 0.0f};
    GNg::Mat44LookAtRH(&view, &eye, &target, &up);
    return view;
}

// 默认描述：60° 视场、near=0.1、far=50、场景 AABB 在相机前方 3 m（完全落在第 0 级切片内：
// 第 0 级分割 ≈ 6.52 m，z=3.5 处视锥半宽 ≈ 2.02 m > 0.5 m）。
sSHADOW_CASCADE_DESC makeTestDesc() {
    sSHADOW_CASCADE_DESC desc;
    desc.aCameraView = makeTestCameraView();
    desc.aFovYRad = 60.0f * 3.14159265358979323846f / 180.0f;
    desc.aAspect = 1.0f;
    desc.aNear = 0.1f;
    desc.aFar = 50.0f;
    desc.aLightDirection = GNg::sVEC3{0.3f, -0.9f, 0.2f};
    desc.aSceneAABB = sAABB(GNg::sVEC3{-0.5f, -0.5f, -3.5f}, GNg::sVEC3{0.5f, 0.5f, -2.5f});
    desc.aCascadeCount = SHADOW_CASCADE_MAX;
    desc.aShadowMapSize = 2048;
    desc.aSplitLambda = 0.5f;
    desc.aTexelSnapping = true;
    return desc;
}

// 世界点 → 该级阴影图 UV（正交矩阵 w=1，仍按通用齐次除法写）。
bool projectToShadowUv(const GNg::sMAT44 &viewProjection, const GNg::sVEC3 &point,
                       float *outU, float *outV, float *outDepth) {
    const float *m = viewProjection.m;
    const float x = m[0] * point.x + m[4] * point.y + m[8] * point.z + m[12];
    const float y = m[1] * point.x + m[5] * point.y + m[9] * point.z + m[13];
    const float z = m[2] * point.x + m[6] * point.y + m[10] * point.z + m[14];
    const float w = m[3] * point.x + m[7] * point.y + m[11] * point.z + m[15];
    if (!(std::fabs(w) > 1.0e-9f)) {
        return false;
    }
    *outU = (x / w) * 0.5f + 0.5f;
    *outV = (y / w) * 0.5f + 0.5f;
    *outDepth = z / w;
    return true;
}

bool matrixIsFinite(const GNg::sMAT44 &matrix) {
    for (int i = 0; i < 16; ++i) {
        if (!std::isfinite(matrix.m[i])) {
            return false;
        }
    }
    return true;
}

float determinant3(const GNg::sMAT44 &matrix) {
    const float *m = matrix.m;
    const float a = m[0], b = m[4], c = m[8];
    const float d = m[1], e = m[5], f = m[9];
    const float g = m[2], h = m[6], i = m[10];
    return a * (e * i - f * h) - b * (d * i - f * g) + c * (d * h - e * g);
}

bool matrixIsZero(const GNg::sMAT44 &matrix) {
    for (int i = 0; i < 16; ++i) {
        if (matrix.m[i] != 0.0f) {
            return false;
        }
    }
    return true;
}

} // namespace

TEST_CASE(shadowCascadeSplitsMonotonic) {
    float splits[SHADOW_CASCADE_MAX] = {0.0f, 0.0f, 0.0f, 0.0f};
    computeCascadeSplits(0.1f, 50.0f, 4, 0.5f, splits);
    CHECK(splits[0] > 0.1f);
    for (uint32_t i = 1; i < 4; ++i) {
        CHECK(splits[i] > splits[i - 1]); // 严格升序
    }
    CHECK_NEAR(splits[3], 50.0f, 1.0e-4f); // 最后一级落在远平面

    // lambda 是对「均匀」的权重：0 ⇒ 纯对数；1 ⇒ 纯均匀（契约的 lerp 两端）
    float logarithmic[SHADOW_CASCADE_MAX] = {0.0f, 0.0f, 0.0f, 0.0f};
    computeCascadeSplits(0.1f, 50.0f, 4, 0.0f, logarithmic);
    CHECK_NEAR(logarithmic[0], 0.1f * std::pow(500.0f, 0.25f), 1.0e-5f);
    float uniform[SHADOW_CASCADE_MAX] = {0.0f, 0.0f, 0.0f, 0.0f};
    computeCascadeSplits(0.1f, 50.0f, 4, 1.0f, uniform);
    CHECK_NEAR(uniform[0], 0.1f + 49.9f * 0.25f, 1.0e-4f);

    // 单级 ⇒ 只有一级且等于远平面；未用槽位为 0
    float single[SHADOW_CASCADE_MAX] = {7.0f, 7.0f, 7.0f, 7.0f};
    computeCascadeSplits(0.1f, 50.0f, 1, 0.5f, single);
    CHECK_NEAR(single[0], 50.0f, 1.0e-4f);
    CHECK(single[1] == 0.0f);
}

TEST_CASE(shadowCascadeCountClamp) {
    sSHADOW_CASCADE_DESC desc = makeTestDesc();
    sSHADOW_CASCADE_RESULT result;

    desc.aCascadeCount = 0; // 钳到 1：只剩一级，其余槽位保持全 0
    CHECK(buildShadowCascades(desc, &result));
    CHECK_EQ(result.aCount, 1u);
    CHECK_NEAR(result.aSplits[0], 50.0f, 1.0e-4f);
    CHECK(result.aTexelWorld[0] > 0.0f);
    CHECK(!matrixIsZero(result.aLightViewProj[0]));
    CHECK(matrixIsZero(result.aLightViewProj[1]));
    CHECK(matrixIsZero(result.aLightViewProj[3]));
    CHECK(result.aSplits[1] == 0.0f);
    CHECK(result.aTexelWorld[1] == 0.0f);

    desc.aCascadeCount = 9; // 钳到 4：四级都产出非零矩阵与正 texel
    CHECK(buildShadowCascades(desc, &result));
    CHECK_EQ(result.aCount, 4u);
    for (uint32_t i = 0; i < 4; ++i) {
        CHECK(result.aSplits[i] > 0.0f);
        CHECK(result.aTexelWorld[i] > 0.0f);
        CHECK(!matrixIsZero(result.aLightViewProj[i]));
    }
}

TEST_CASE(shadowCascadeMatricesFiniteAndNonDegenerate) {
    const sSHADOW_CASCADE_DESC desc = makeTestDesc();
    sSHADOW_CASCADE_RESULT result;
    CHECK(buildShadowCascades(desc, &result));
    CHECK_EQ(result.aShadowMapSize, 2048u);
    for (uint32_t i = 0; i < result.aCount; ++i) {
        const GNg::sMAT44 &matrix = result.aLightViewProj[i];
        CHECK(matrixIsFinite(matrix));
        // 正交投影：w 行 (0,0,0,1)，上左 3×3 行列式非 0（不可退化成平面）
        CHECK_NEAR(matrix.m[3], 0.0f, 1.0e-6f);
        CHECK_NEAR(matrix.m[7], 0.0f, 1.0e-6f);
        CHECK_NEAR(matrix.m[11], 0.0f, 1.0e-6f);
        CHECK_NEAR(matrix.m[15], 1.0f, 1.0e-6f);
        CHECK(std::fabs(determinant3(matrix)) > 1.0e-12f);
    }
}

TEST_CASE(shadowCascadeCoversSceneAabb) {
    const sSHADOW_CASCADE_DESC desc = makeTestDesc();
    sSHADOW_CASCADE_RESULT result;
    CHECK(buildShadowCascades(desc, &result));

    // 场景 AABB 的 8 角点必须落在覆盖它的那一级的 [0,1] UV 内（1 texel 容差：
    // snapping 会把盒中心平移 ≤0.5 texel，边缘因此最多外溢 0.5 texel）。
    const float tolerance = 2.0f / static_cast<float>(result.aShadowMapSize);
    const GNg::sVEC3 &lo = desc.aSceneAABB.aMin;
    const GNg::sVEC3 &hi = desc.aSceneAABB.aMax;
    uint32_t coveredCascades = 0;
    for (uint32_t cascade = 0; cascade < result.aCount; ++cascade) {
        bool allInside = true;
        for (uint32_t corner = 0; corner < 8; ++corner) {
            const GNg::sVEC3 point = {(corner & 1u) ? hi.x : lo.x, (corner & 2u) ? hi.y : lo.y,
                                      (corner & 4u) ? hi.z : lo.z};
            float u = 0.0f;
            float v = 0.0f;
            float depth = 0.0f;
            if (!projectToShadowUv(result.aLightViewProj[cascade], point, &u, &v, &depth)) {
                allInside = false;
                break;
            }
            if (u < -tolerance || u > 1.0f + tolerance || v < -tolerance ||
                v > 1.0f + tolerance) {
                allInside = false;
            }
            // 契约 §8 选级也要求 z ∈ [0,1]（正交深度：近端 0、远端 1）。
            if (!(depth >= -1.0e-3f && depth <= 1.0f + 1.0e-3f)) {
                allInside = false;
            }
        }
        if (allInside) {
            ++coveredCascades;
        }
    }
    CHECK(coveredCascades >= 1u);
}

TEST_CASE(shadowCascadeTexelSnappingKeepsCenterCovered) {
    sSHADOW_CASCADE_DESC desc = makeTestDesc();
    sSHADOW_CASCADE_RESULT snapped;
    CHECK(buildShadowCascades(desc, &snapped));

    // 场景 AABB 完全落在第 0 级切片内 ⇒ 拟合盒 = 场景盒，其中心点应被投影到图心
    // 不超过 0.5 texel 的位置（texel snapping 只允许整 texel 的平移）。
    const GNg::sVEC3 sceneCenter = desc.aSceneAABB.Center();
    float u = 0.0f;
    float v = 0.0f;
    float depth = 0.0f;
    CHECK(projectToShadowUv(snapped.aLightViewProj[0], sceneCenter, &u, &v, &depth));
    CHECK(std::fabs(u - 0.5f) * static_cast<float>(snapped.aShadowMapSize) <= 0.5f + 1.0e-3f);
    CHECK(std::fabs(v - 0.5f) * static_cast<float>(snapped.aShadowMapSize) <= 0.5f + 1.0e-3f);

    // 关闭 snapping ⇒ 中心严格落在图心
    sSHADOW_CASCADE_DESC unsnappedDesc = desc;
    unsnappedDesc.aTexelSnapping = false;
    sSHADOW_CASCADE_RESULT unsnapped;
    CHECK(buildShadowCascades(unsnappedDesc, &unsnapped));
    CHECK(projectToShadowUv(unsnapped.aLightViewProj[0], sceneCenter, &u, &v, &depth));
    CHECK_NEAR(u, 0.5f, 1.0e-4f);
    CHECK_NEAR(v, 0.5f, 1.0e-4f);
    // 分割与 texel 尺寸不受 snapping 开关影响
    CHECK_NEAR(snapped.aSplits[0], unsnapped.aSplits[0], 1.0e-6f);
    CHECK_NEAR(snapped.aTexelWorld[0], unsnapped.aTexelWorld[0], 1.0e-9f);
}

TEST_CASE(shadowCascadeTexelSnappingQuantizesTranslation) {
    // 场景 AABB 取很大 ⇒ 拟合盒 = 该级视锥盒（与包围盒求交不改变它），
    // 相机做纯平移时视锥盒等大平移，光空间中心随之平移 ⇒ 可精确验证 snapping 的量化行为。
    sSHADOW_CASCADE_DESC desc = makeTestDesc();
    desc.aSceneAABB = sAABB(GNg::sVEC3{-1000.0f, -1000.0f, -1000.0f},
                            GNg::sVEC3{1000.0f, 1000.0f, 1000.0f});
    sSHADOW_CASCADE_RESULT base;
    CHECK(buildShadowCascades(desc, &base));
    const float texel = base.aTexelWorld[0];
    CHECK(texel > 0.0f);

    // 探针世界点（固定不动）与其在平移前后的 UV 变化量。
    const GNg::sVEC3 probe = {0.0f, 0.0f, -3.0f};
    const float nudgeTexels = 0.3f;
    sSHADOW_CASCADE_DESC moved = desc;
    const float shift = nudgeTexels * texel; // 沿世界 +x 平移；投影后是 texel 的某个倍数分量
    GNg::sMAT44 translate;
    GNg::Mat44Translate(&translate, -shift, 0.0f, 0.0f);
    GNg::Mat44Mul(&moved.aCameraView, &desc.aCameraView, &translate);

    sSHADOW_CASCADE_RESULT shifted;
    CHECK(buildShadowCascades(moved, &shifted));
    CHECK_NEAR(shifted.aTexelWorld[0], texel, 1.0e-9f);

    // 世界点相对平移的固定探针：取相机平移后的“原世界点 + 平移量”，UV 变化量应被量化成整数 texel。
    float u0 = 0.0f, v0 = 0.0f, d0 = 0.0f;
    CHECK(projectToShadowUv(base.aLightViewProj[0], probe, &u0, &v0, &d0));
    float u1 = 0.0f, v1 = 0.0f, d1 = 0.0f;
    CHECK(projectToShadowUv(shifted.aLightViewProj[0], probe, &u1, &v1, &d1));
    const float deltaTexels = (u1 - u0) * static_cast<float>(base.aShadowMapSize);
    const float nearestInteger = std::floor(deltaTexels + 0.5f);
    CHECK(std::fabs(deltaTexels - nearestInteger) < 1.0e-2f);

    // 对照：关闭 snapping 时同一平移量的 UV 变化是连续值（不可能恰好总是整数 texel）
    sSHADOW_CASCADE_DESC movedUnsnapped = moved;
    movedUnsnapped.aTexelSnapping = false;
    sSHADOW_CASCADE_RESULT shiftedUnsnapped;
    CHECK(buildShadowCascades(movedUnsnapped, &shiftedUnsnapped));
    float u2 = 0.0f, v2 = 0.0f, d2 = 0.0f;
    CHECK(projectToShadowUv(shiftedUnsnapped.aLightViewProj[0], probe, &u2, &v2, &d2));
    const float rawDeltaTexels = (u2 - u0) * static_cast<float>(base.aShadowMapSize);
    CHECK(std::fabs(rawDeltaTexels) > 1.0e-3f); // 确有平移
}

TEST_CASE(shadowCascadeRejectsInvalidInput) {
    sSHADOW_CASCADE_DESC desc = makeTestDesc();
    sSHADOW_CASCADE_RESULT result;
    CHECK(buildShadowCascades(desc, nullptr) == false);

    sSHADOW_CASCADE_DESC invalidAabb = desc;
    invalidAabb.aSceneAABB.Reset();
    CHECK(buildShadowCascades(invalidAabb, &result) == false);

    sSHADOW_CASCADE_DESC invalidRange = desc;
    invalidRange.aFar = invalidRange.aNear;
    CHECK(buildShadowCascades(invalidRange, &result) == false);

    sSHADOW_CASCADE_DESC invalidFov = desc;
    invalidFov.aFovYRad = 0.0f;
    CHECK(buildShadowCascades(invalidFov, &result) == false);

    sSHADOW_CASCADE_DESC invalidAspect = desc;
    invalidAspect.aAspect = -1.0f;
    CHECK(buildShadowCascades(invalidAspect, &result) == false);

    // 零光方向走回退顶光，不产生 NaN（矩阵仍有限）
    sSHADOW_CASCADE_DESC zeroLight = desc;
    zeroLight.aLightDirection = GNg::sVEC3{0.0f, 0.0f, 0.0f};
    CHECK(buildShadowCascades(zeroLight, &result));
    CHECK(matrixIsFinite(result.aLightViewProj[0]));
}
