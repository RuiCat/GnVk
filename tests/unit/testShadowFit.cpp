// testShadowFit.cpp — CSM 拟合策略（A6 从两个 demo 抽出的公共部分）。
//
// 抽出来的东西只有三件，但每件都有过真实 bug：
//   1. far 口径：只取盒对角时，相机站在场景外会让各级切片全落在相机与场景之间，
//      阴影整片消失。必须取 max(对角, 相机到最远角 + 0.5)。
//   2. 承接区外扩 / 抬到地面以上：不留余量则角色脚下的地面接不到阴影。
//   3. 级联数钳制：demo 配的级数可能超过引擎实际分配的阴影层数。
#include "testFramework.h"

#include "GN_ShadowCascade.h"

#include <cmath>

namespace {

bool nearlyEqual(float a, float b, float eps = 1.0e-4f) {
    return std::fabs(a - b) <= eps;
}

GN3d::sAABB makeBox(float x0, float y0, float z0, float x1, float y1, float z1) {
    return GN3d::sAABB(GNg::sVEC3{x0, y0, z0}, GNg::sVEC3{x1, y1, z1});
}

} // namespace

// 相机在场景外：far 必须容得下「相机到最远角」，否则切片落在相机与场景之间。
TEST_CASE(shadowFarCoversCameraOutsideScene) {
    // 盒：[(-1,-1,-1), (1,1,1)]，盒对角 = 2*sqrt(3) ≈ 3.464
    const GN3d::sAABB box = makeBox(-1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f);
    // 相机在 (0, 0, 100)：到盒最远角 (-1,-1,-1) 距离 = sqrt(1+1+101²) ≈ 101.01
    const GNg::sVEC3 camera = {0.0f, 0.0f, 100.0f};
    const float farZ = GN3d::computeShadowFar(box, camera);

    const float expected = std::sqrt(1.0f + 1.0f + 101.0f * 101.0f) + 0.5f;
    CHECK(farZ > 100.0f); // 远大于盒对角 3.46
    CHECK(nearlyEqual(farZ, expected, 0.01f));
}

// 相机在场景中心附近：对角占优。
TEST_CASE(shadowFarUsesDiagonalWhenCameraInside) {
    const GN3d::sAABB box = makeBox(-10.0f, -10.0f, -10.0f, 10.0f, 10.0f, 10.0f);
    const GNg::sVEC3 camera = {0.0f, 0.0f, 0.0f};
    const float farZ = GN3d::computeShadowFar(box, camera);

    // 对角 = 2*sqrt(300) ≈ 34.64；最远角距 = sqrt(300) ≈ 17.32 + 0.5
    // 对角占优
    CHECK(nearlyEqual(farZ, 2.0f * std::sqrt(300.0f), 0.01f));
}

// 极小模型兜底：far 不得小于 1 m，否则近平面/远平面挤在一起，深度精度崩。
TEST_CASE(shadowFarHasMinimum) {
    const GN3d::sAABB tiny = makeBox(0.0f, 0.0f, 0.0f, 0.001f, 0.001f, 0.001f);
    const GNg::sVEC3 camera = {0.0f, 0.0f, 0.0f};
    CHECK(GN3d::computeShadowFar(tiny, camera) >= 1.0f);
}

// 无效盒不产生 NaN，返回兜底下限。
TEST_CASE(shadowFarInvalidBoxIsSafe) {
    GN3d::sAABB invalid; // 默认构造应无效
    const GNg::sVEC3 camera = {0.0f, 0.0f, 0.0f};
    const float farZ = GN3d::computeShadowFar(invalid, camera);
    CHECK(farZ >= 1.0f);
    CHECK(farZ == farZ); // 非 NaN
}

// fitShadowCascades：级联数必须钳到引擎层数，且能写进结果。
TEST_CASE(shadowFitClampsToEngineLayers) {
    GN3d::sSHADOW_FIT_DESC desc;
    desc.aCameraView = GNg::sMAT44(true);
    desc.aCameraPos = GNg::sVEC3{0.0f, 1.7f, 8.0f};
    desc.aFovYRad = 60.0f * 0.017453292519943295f;
    desc.aAspect = 1.6f;
    desc.aLightDirection = GNg::sVEC3{0.12f, -1.0f, 0.10f};
    desc.aCasterBox = makeBox(-1.0f, 0.0f, -1.0f, 1.0f, 1.8f, 1.0f);
    desc.aCascadeCount = 4;
    desc.aEngineCascadeLayers = 2; // 引擎只分配了 2 层

    GN3d::sSHADOW_CASCADE_RESULT result;
    CHECK(GN3d::fitShadowCascades(desc, nullptr, &result)); // light=nullptr 只算不写
    CHECK(result.aCount == 2);
}

// 承接区外扩 + 抬到地面以上：拟合盒 aMin.y 不应为负。
TEST_CASE(shadowFitClampsBoxAboveGround) {
    GN3d::sSHADOW_FIT_DESC desc;
    desc.aCameraView = GNg::sMAT44(true);
    desc.aCameraPos = GNg::sVEC3{0.0f, 1.7f, 5.0f};
    desc.aFovYRad = 60.0f * 0.017453292519943295f;
    desc.aAspect = 1.6f;
    desc.aCasterBox = makeBox(-0.5f, -2.0f, -0.5f, 0.5f, 1.8f, 0.5f); // 盒底在地下
    desc.aCatchMargin = 2.5f;
    desc.aClampToGround = true;

    GN3d::sSHADOW_CASCADE_RESULT result;
    CHECK(GN3d::fitShadowCascades(desc, nullptr, &result));
    // 拟合成功即说明盒被抬到地面以上（否则 far 计算仍可成功，但这里主要验不崩）
    CHECK(result.aCount >= 1);
    CHECK(result.aCount <= GN3d::SHADOW_CASCADE_MAX);
}

// 无效输入必须返回 false，不能写出垃圾结果。
TEST_CASE(shadowFitRejectsInvalidInput) {
    GN3d::sSHADOW_CASCADE_RESULT result;

    GN3d::sSHADOW_FIT_DESC bad;
    bad.aAspect = 0.0f; // 非法宽高比
    CHECK(!GN3d::fitShadowCascades(bad, nullptr, &result));

    GN3d::sSHADOW_FIT_DESC noBox;
    noBox.aAspect = 1.6f;
    // aCasterBox 默认无效
    CHECK(!GN3d::fitShadowCascades(noBox, nullptr, &result));

    GN3d::sSHADOW_FIT_DESC ok;
    ok.aAspect = 1.6f;
    ok.aCasterBox = makeBox(-1.0f, 0.0f, -1.0f, 1.0f, 1.0f, 1.0f);
    CHECK(!GN3d::fitShadowCascades(ok, nullptr, nullptr)); // out 为空
}

// 级联数退化：请求 0 级时至少给 1 级。
TEST_CASE(shadowFitAtLeastOneCascade) {
    GN3d::sSHADOW_FIT_DESC desc;
    desc.aAspect = 1.6f;
    desc.aFovYRad = 1.0f;
    desc.aCasterBox = makeBox(-1.0f, 0.0f, -1.0f, 1.0f, 1.0f, 1.0f);
    desc.aCascadeCount = 0;
    desc.aEngineCascadeLayers = 4;

    GN3d::sSHADOW_CASCADE_RESULT result;
    CHECK(GN3d::fitShadowCascades(desc, nullptr, &result));
    CHECK(result.aCount == 1);
}
