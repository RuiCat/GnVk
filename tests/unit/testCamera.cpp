// testCamera.cpp — 3D 摄像机（C0–C4）纯 CPU 单测：逐条覆盖 docs/CAMERA_PLAN.md §5.2 的 22 条用例。
#include "testFramework.h"

#include <GN.h>
#include "GN_3Dcamera.h"

#include <cmath>

using namespace GN3d;

namespace {

GNg::sVEC3 makeVec(float x, float y, float z) {
    const GNg::sVEC3 v = {x, y, z};
    return v;
}

GNg::sVEC3 vecSub(const GNg::sVEC3 &a, const GNg::sVEC3 &b) {
    return makeVec(a.x - b.x, a.y - b.y, a.z - b.z);
}

float vecLength(const GNg::sVEC3 &v) {
    return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

float vecDot(const GNg::sVEC3 &a, const GNg::sVEC3 &b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

GNg::sVEC3 vecCross(const GNg::sVEC3 &a, const GNg::sVEC3 &b) {
    return makeVec(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}

float quatLength(const GNg::sQUAT &q) {
    return std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
}

// 用相机的 viewProj 把世界点投到 NDC（VK 深度 0..1）
GNg::sVEC3 viewProjNdc(const cCAMERA &camera, float aspect, const GNg::sVEC3 &point) {
    GNg::sMAT44 view, proj, viewProj;
    camera.BuildViewProj(aspect, &view, &proj, &viewProj);
    const GNg::sVEC4 clip(point, 1.0f);
    GNg::sVEC4 out;
    GNg::Mat44TransformV4(&out, &clip, &viewProj);
    return makeVec(out.x / out.w, out.y / out.w, out.z / out.w);
}

} // namespace

// 用例 1：默认值
TEST_CASE(cameraInitDefaults) {
    cCAMERA camera;
    CHECK_NEAR(camera.aFovY, 45.0f, 1e-6f);
    CHECK_NEAR(camera.aNear, 0.1f, 1e-6f);
    CHECK_NEAR(camera.aFar, 1000.0f, 1e-6f);
    CHECK_NEAR(camera.aOrthoH, 0.0f, 1e-6f);
    CHECK(camera.aMode == eCAMERA_MODE_FREE);
    CHECK(!camera.IsOrthoMode());
    CHECK_NEAR(quatLength(camera.aRotation), 1.0f, 1e-6f);
    CHECK_NEAR(camera.Forward().z, -1.0f, 1e-6f);

    sCAMERA_DESC desc;
    cCAMERA fromDesc;
    fromDesc.Init(&desc); // 空描述等价于默认值
    CHECK_NEAR(fromDesc.aFovY, 45.0f, 1e-6f);
}

// 用例 2：镜头参数校验（非法返回 false 且不改状态；正交与透视互切）
TEST_CASE(cameraLensValidate) {
    cCAMERA camera;
    const float fovBefore = camera.aFovY;
    const float nearBefore = camera.aNear;
    const float farBefore = camera.aFar;

    CHECK(!camera.SetFov(0.0f));
    CHECK(!camera.SetFov(-1.0f));
    CHECK(!camera.SetFov(200.0f));
    CHECK_NEAR(camera.aFovY, fovBefore, 1e-6f);

    CHECK(!camera.SetNF(10.0f, 1.0f));
    CHECK_NEAR(camera.aNear, nearBefore, 1e-6f);
    CHECK_NEAR(camera.aFar, farBefore, 1e-6f);

    CHECK(camera.SetOrthoH(7.0f));
    CHECK(camera.IsOrthoMode());
    CHECK_NEAR(camera.aOrthoH, 7.0f, 1e-6f);

    CHECK(camera.SetFov(45.0f)); // GN 语义：设置 FOV 回透视
    CHECK(!camera.IsOrthoMode());
    CHECK_NEAR(camera.aOrthoH, 0.0f, 1e-6f);
}

// 用例 3：相机在 (0,0,5) 看原点 → 原点投到 NDC 中心
TEST_CASE(cameraBuildViewProjCenter) {
    cCAMERA camera;
    const GNg::sVEC3 position = makeVec(0.0f, 0.0f, 5.0f);
    camera.SetWorldPos(&position);

    GNg::sMAT44 view, proj, viewProj;
    CHECK(camera.BuildViewProj(1.6f, &view, &proj, &viewProj));
    CHECK(!camera.BuildViewProj(0.0f, &view, &proj, &viewProj)); // aspect 非法

    const GNg::sVEC3 ndc = viewProjNdc(camera, 1.6f, makeVec(0.0f, 0.0f, 0.0f));
    CHECK_NEAR(ndc.x, 0.0f, 1e-5f);
    CHECK_NEAR(ndc.y, 0.0f, 1e-5f);
    CHECK(ndc.z > 0.0f && ndc.z < 1.0f);
}

// 用例 4：近/远平面点深度落在 VK 区间两端
TEST_CASE(cameraBuildViewProjDepth) {
    cCAMERA camera;
    const GNg::sVEC3 nearPoint = makeVec(0.0f, 0.0f, -camera.aNear);
    const GNg::sVEC3 farPoint = makeVec(0.0f, 0.0f, -camera.aFar);

    const GNg::sVEC3 nearNdc = viewProjNdc(camera, 1.0f, nearPoint);
    const GNg::sVEC3 farNdc = viewProjNdc(camera, 1.0f, farPoint);
    CHECK_NEAR(nearNdc.z, 0.0f, 1e-4f);
    CHECK_NEAR(farNdc.z, 1.0f, 1e-4f);
}

// 用例 5：正交投影半宽与 NDC 的对应关系
TEST_CASE(cameraBuildOrtho) {
    cCAMERA camera;
    CHECK(camera.SetOrthoH(10.0f)); // halfH = 5，aspect = 2 → halfW = 10

    const GNg::sVEC3 halfWidthNdc = viewProjNdc(camera, 2.0f, makeVec(5.0f, 0.0f, -10.0f));
    CHECK_NEAR(halfWidthNdc.x, 0.5f, 1e-5f);

    const GNg::sVEC3 edgeNdc = viewProjNdc(camera, 2.0f, makeVec(10.0f, 0.0f, -10.0f));
    CHECK_NEAR(edgeNdc.x, 1.0f, 1e-5f);
}

// 用例 6：FovToOrthoH 与往返一致
TEST_CASE(cameraFovToOrthoH) {
    CHECK_NEAR(cCAMERA::FovToOrthoH(90.0f, 5.0f), 10.0f, 1e-5f);

    const float distance = 3.0f;
    const float orthoH = cCAMERA::FovToOrthoH(60.0f, distance);
    const float backToFov = 2.0f * std::atan(orthoH / (2.0f * distance)) * CAMERA_RAD_TO_DEG;
    CHECK_NEAR(backToFov, 60.0f, 1e-4f);
}

// 用例 7：SelfMove 沿自身轴（默认朝 -Z）
TEST_CASE(cameraSelfMoveLocal) {
    cCAMERA frontCamera;
    frontCamera.SelfMove(1.0f, 0.0f, 0.0f);
    CHECK_NEAR(frontCamera.GetCoordV().z, -1.0f, 1e-6f);

    cCAMERA leftCamera;
    leftCamera.SelfMove(0.0f, 1.0f, 0.0f);
    CHECK_NEAR(leftCamera.GetCoordV().x, -1.0f, 1e-6f);

    cCAMERA upCamera;
    upCamera.SelfMove(0.0f, 0.0f, 1.0f);
    CHECK_NEAR(upCamera.GetCoordV().y, 1.0f, 1e-6f);
}

// 用例 8：SelfRotLR 右转 / SelfRotTB 抬头（本仓库符号约定）
TEST_CASE(cameraSelfRotYawPitch) {
    cCAMERA yawCamera;
    yawCamera.SelfRotLR(90.0f * CAMERA_DEG_TO_RAD);
    CHECK(yawCamera.Forward().x > 0.99f);
    CHECK_NEAR(yawCamera.Forward().z, 0.0f, 1e-4f);

    cCAMERA pitchCamera;
    pitchCamera.SelfRotTB(30.0f * CAMERA_DEG_TO_RAD);
    CHECK(pitchCamera.Forward().y > 0.4f);
    CHECK_NEAR(pitchCamera.Forward().x, 0.0f, 1e-5f);
}

// 用例 9：连续抬头不越过 ±83°，四元数仍单位化
TEST_CASE(cameraPitchClamp) {
    cCAMERA camera;
    for(int i = 0; i < 10; ++i) camera.SelfRotTB(30.0f * CAMERA_DEG_TO_RAD);

    CHECK(camera.Forward().y <= std::sin(83.0f * CAMERA_DEG_TO_RAD) + 1e-4f);
    CHECK(camera.Forward().y > 0.5f); // 确实抬过，只是被钳住
    CHECK_NEAR(quatLength(camera.aRotation), 1.0f, 1e-6f);
}

// 用例 10：ToMove 是世界坐标位移，不受朝向影响
TEST_CASE(cameraToMoveWorld) {
    cCAMERA camera;
    camera.SelfRotLR(90.0f * CAMERA_DEG_TO_RAD);
    camera.ToMove(1.0f, 0.0f, 0.0f);
    CHECK_NEAR(camera.GetCoordV().x, 1.0f, 1e-6f);
    CHECK_NEAR(camera.GetCoordV().z, 0.0f, 1e-6f);
}

// 用例 11：绕任意轴旋转后正交基仍正交、行列式 ≈ 1；零长度轴不旋转
TEST_CASE(cameraSelfRotAxis) {
    cCAMERA camera;
    const GNg::sVEC3 axis = makeVec(1.0f, 1.0f, 0.0f);
    camera.SelfRotAxis(0.7f, &axis);

    const GNg::sVEC3 forward = camera.Forward();
    const GNg::sVEC3 right = camera.Right();
    const GNg::sVEC3 up = camera.Up();
    CHECK_NEAR(vecLength(forward), 1.0f, 1e-5f);
    CHECK_NEAR(vecLength(right), 1.0f, 1e-5f);
    CHECK_NEAR(vecLength(up), 1.0f, 1e-5f);
    CHECK_NEAR(vecDot(forward, right), 0.0f, 1e-5f);
    CHECK_NEAR(vecDot(forward, up), 0.0f, 1e-5f);
    CHECK_NEAR(vecDot(right, up), 0.0f, 1e-5f);

    const GNg::sVEC3 back = makeVec(-forward.x, -forward.y, -forward.z);
    CHECK_NEAR(vecDot(vecCross(right, up), back), 1.0f, 1e-5f); // 右手基行列式

    const GNg::sVEC3 zeroAxis = makeVec(0.0f, 0.0f, 0.0f);
    const GNg::sQUAT before = camera.aRotation;
    camera.SelfRotAxis(1.0f, &zeroAxis);
    CHECK_NEAR(quatLength(camera.aRotation), 1.0f, 1e-6f);
    CHECK_NEAR(before.Dot(camera.aRotation), 1.0f, 1e-6f);
}

// 用例 12：默认轨道（yaw=0,pitch=0）→ 位置在焦点 +Z 侧，朝 -Z 看
TEST_CASE(cameraOrbitPosition) {
    cCAMERA camera;
    const GNg::sVEC3 focus = makeVec(0.0f, 0.0f, 0.0f);
    camera.SetOrbit(focus, 10.0f, 0.0f, 0.0f);

    CHECK_NEAR(camera.GetCoordV().x, 0.0f, 1e-5f);
    CHECK_NEAR(camera.GetCoordV().y, 0.0f, 1e-5f);
    CHECK_NEAR(camera.GetCoordV().z, 10.0f, 1e-5f);
    CHECK_NEAR(camera.Forward().z, -1.0f, 1e-5f);
}

// 用例 13：缩放距离钳制在 [0.05, 1e5]，focus 不变
TEST_CASE(cameraOrbitZoomClamp) {
    cCAMERA camera;
    const GNg::sVEC3 focus = makeVec(0.0f, 0.0f, 0.0f);
    camera.SetOrbit(focus, 10.0f, 0.0f, 0.0f);

    camera.OrbitZoom(1000.0f);
    CHECK_NEAR(vecLength(camera.GetCoordV()), CAMERA_MIN_DISTANCE, 1e-5f);
    CHECK_NEAR(camera.GetCoordV().z, CAMERA_MIN_DISTANCE, 0.05f);
    camera.OrbitZoom(-1000.0f);
    CHECK_NEAR(vecLength(camera.GetCoordV()), CAMERA_MAX_DISTANCE, 1.0f);
    CHECK_NEAR(camera.GetCoordV().x, 0.0f, 1e-3f); // focus 不变 → 偏移方向仍沿 +Z
}

// 用例 14：平移改 focus（沿相机 right/up），距离不变
TEST_CASE(cameraOrbitPan) {
    cCAMERA camera;
    const GNg::sVEC3 focus = makeVec(0.0f, 0.0f, 0.0f);
    camera.SetOrbit(focus, 10.0f, 0.0f, 0.0f);
    camera.OrbitPan(1.0f, 1.0f);

    const GNg::sVEC3 pannedFocus = makeVec(1.0f, 1.0f, 0.0f);
    CHECK_NEAR(camera.GetCoordV().x, pannedFocus.x, 1e-5f);
    CHECK_NEAR(camera.GetCoordV().y, pannedFocus.y, 1e-5f);
    CHECK_NEAR(camera.GetCoordV().z, pannedFocus.z + 10.0f, 1e-5f);
    CHECK_NEAR(vecLength(vecSub(camera.GetCoordV(), pannedFocus)), 10.0f, 1e-5f);
}

// 用例 15：目标瞬移后 1.5 s 内收敛到 offset 相对位置
TEST_CASE(cameraFollowConverge) {
    cCAMERA camera;
    const GNg::sVEC3 position = makeVec(0.0f, 0.0f, 5.0f);
    camera.SetWorldPos(&position);

    sCAMERA_TARGET target;
    target.aValid = true;
    target.aPosition = makeVec(0.0f, 0.0f, 0.0f);
    camera.SetTarget(target, 0.15f);
    CHECK(camera.HasTarget());
    CHECK(camera.aMode == eCAMERA_MODE_FOLLOW);

    target.aPosition = makeVec(10.0f, 0.0f, 0.0f);
    camera.SetTarget(target, 0.15f);

    const GNg::sVEC3 goal = makeVec(10.0f, 0.0f, 5.0f);
    const float dt = 1.0f / 60.0f;
    for(int i = 0; i < 90; ++i) camera.Update(dt);
    CHECK(vecLength(vecSub(camera.GetCoordV(), goal)) < 1e-3f);
}

// 用例 16：收敛过程距离单调不增（无过冲），末速度≈0
TEST_CASE(cameraFollowNoOvershoot) {
    cCAMERA camera;
    const GNg::sVEC3 position = makeVec(0.0f, 0.0f, 5.0f);
    camera.SetWorldPos(&position);
    sCAMERA_TARGET target;
    target.aValid = true;
    target.aPosition = makeVec(0.0f, 0.0f, 0.0f);
    camera.SetTarget(target, 0.15f);
    target.aPosition = makeVec(10.0f, 0.0f, 0.0f);
    camera.SetTarget(target, 0.15f);

    const GNg::sVEC3 goal = makeVec(10.0f, 0.0f, 5.0f);
    const float dt = 1.0f / 60.0f;
    float previousDistance = vecLength(vecSub(camera.GetCoordV(), goal));
    float previousX = camera.GetCoordV().x;
    bool monotonic = true;
    for(int i = 0; i < 180; ++i) {
        camera.Update(dt);
        const float distance = vecLength(vecSub(camera.GetCoordV(), goal));
        if(distance > previousDistance + 1e-6f || camera.GetCoordV().x < previousX - 1e-6f)
            monotonic = false;
        previousDistance = distance;
        previousX = camera.GetCoordV().x;
    }
    CHECK(monotonic);
    CHECK(previousDistance < 1e-5f);

    const GNg::sVEC3 settled = camera.GetCoordV();
    camera.Update(dt);
    CHECK(vecLength(vecSub(camera.GetCoordV(), settled)) < 1e-6f); // 速度已清零
}

// 用例 17：目标 aValid=false 后 Update 不改位置/朝向
TEST_CASE(cameraFollowInvalidTarget) {
    cCAMERA camera;
    const GNg::sVEC3 position = makeVec(0.0f, 0.0f, 5.0f);
    camera.SetWorldPos(&position);
    sCAMERA_TARGET target;
    target.aValid = true;
    target.aPosition = makeVec(0.0f, 0.0f, 0.0f);
    camera.SetTarget(target);
    camera.Update(0.1f);

    const GNg::sVEC3 positionBefore = camera.GetCoordV();
    const GNg::sQUAT rotationBefore = camera.aRotation;
    sCAMERA_TARGET invalidTarget; // aValid = false
    camera.SetTarget(invalidTarget);
    for(int i = 0; i < 30; ++i) camera.Update(1.0f / 60.0f);
    CHECK(vecLength(vecSub(camera.GetCoordV(), positionBefore)) < 1e-6f);
    CHECK_NEAR(std::fabs(rotationBefore.Dot(camera.aRotation)), 1.0f, 1e-6f);
}

// 用例 18：跟随中调整偏移后自动平衡（up 无 roll）
TEST_CASE(cameraFollowAutoBalance) {
    cCAMERA camera;
    const GNg::sVEC3 position = makeVec(0.0f, 1.0f, 10.0f);
    camera.SetWorldPos(&position);
    sCAMERA_TARGET target;
    target.aValid = true;
    target.aPosition = makeVec(0.0f, 0.0f, 0.0f);
    camera.SetTarget(target, 0.15f, true, false);
    for(int i = 0; i < 30; ++i) camera.Update(1.0f / 60.0f);

    camera.SelfMove(0.0f, 5.0f, 0.0f); // 跟随中调偏移，朝向需要重新对准
    for(int i = 0; i < 120; ++i) camera.Update(1.0f / 60.0f);
    CHECK(camera.Up().y > 0.99f); // 无 roll
    CHECK_NEAR(camera.Right().y, 0.0f, 1e-3f);
}

// 用例 19：八个观看方位的前向/up 与定义表一致
TEST_CASE(cameraLookDirectionModes) {
    struct sLOOK_EXPECT {
        eCAMERA_LOOK_MODE aMode;
        GNg::sVEC3 aForward;
        GNg::sVEC3 aUp;
    };
    const sLOOK_EXPECT table[] = {
        {eCAMERA_LOOK_TOP, {0.0f, -1.0f, 0.0f}, {0.0f, 0.0f, -1.0f}},
        {eCAMERA_LOOK_FRONT, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f, 0.0f}},
        {eCAMERA_LOOK_LEFT, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
        {eCAMERA_LOOK_BOTTOM, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
        {eCAMERA_LOOK_BACK, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}},
        {eCAMERA_LOOK_RIGHT, {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
        {eCAMERA_LOOK_DOWN, {0.0f, -1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
        {eCAMERA_LOOK_SWAP, {0.0f, 0.0f, -1.0f}, {0.0f, -1.0f, 0.0f}},
    };

    for(size_t i = 0; i < sizeof(table) / sizeof(table[0]); ++i) {
        cCAMERA camera;
        SetCameraLookDirection(camera, table[i].aMode);
        CHECK_NEAR(vecDot(camera.Forward(), table[i].aForward), 1.0f, 1e-4f);
        CHECK_NEAR(vecDot(camera.Up(), table[i].aUp), 1.0f, 1e-4f);
        CHECK_NEAR(vecLength(camera.GetCoordV()), 0.0f, 1e-6f); // 放到原点
    }
}

// 用例 20：半范围 (1,1,1)、fov=90、aspect=1、margin=1 → 距离≈2，范围恰好填满
TEST_CASE(cameraLookRangeFit) {
    cCAMERA camera;
    CHECK(camera.SetFov(90.0f));
    const GNg::sVEC3 center = makeVec(0.0f, 0.0f, 0.0f);
    const GNg::sVEC3 halfExtent = makeVec(1.0f, 1.0f, 1.0f);

    const float distance = SetCameraLookRange(camera, center, halfExtent, 1.0f,
                                              eCAMERA_LOOK_FRONT);
    CHECK_NEAR(distance, 2.0f, 1e-4f);
    CHECK_NEAR(camera.GetCoordV().z, 2.0f, 1e-4f);

    float maxAbsNdc = 0.0f;
    for(int sx = -1; sx <= 1; sx += 2) {
        for(int sy = -1; sy <= 1; sy += 2) {
            for(int sz = -1; sz <= 1; sz += 2) {
                const GNg::sVEC3 corner =
                    makeVec((float)sx * halfExtent.x, (float)sy * halfExtent.y,
                            (float)sz * halfExtent.z);
                const GNg::sVEC3 ndc = viewProjNdc(camera, 1.0f, corner);
                CHECK(std::fabs(ndc.x) <= 1.0f + 1e-4f);
                CHECK(std::fabs(ndc.y) <= 1.0f + 1e-4f);
                if(std::fabs(ndc.x) > maxAbsNdc) maxAbsNdc = std::fabs(ndc.x);
                if(std::fabs(ndc.y) > maxAbsNdc) maxAbsNdc = std::fabs(ndc.y);
            }
        }
    }
    CHECK_NEAR(maxAbsNdc, 1.0f, 1e-4f);
}

// 用例 21：按 AABB 取景后 8 个角点都落在 NDC [-1,1]
TEST_CASE(cameraLookObjFit) {
    cCAMERA camera;
    sCAMERA_TARGET target;
    target.aValid = true;
    target.aPosition = makeVec(0.0f, 0.0f, 0.0f);
    target.aBounds = sAABB(makeVec(-1.0f, -1.0f, -1.0f), makeVec(1.0f, 1.0f, 1.0f));

    const float distance = SetCameraLookObj(camera, target);
    CHECK(distance > 1.0f);

    for(int sx = -1; sx <= 1; sx += 2) {
        for(int sy = -1; sy <= 1; sy += 2) {
            for(int sz = -1; sz <= 1; sz += 2) {
                const GNg::sVEC3 corner = makeVec((float)sx, (float)sy, (float)sz);
                const GNg::sVEC3 ndc = viewProjNdc(camera, 1.0f, corner);
                CHECK(std::fabs(ndc.x) <= 1.0f + 1e-3f);
                CHECK(std::fabs(ndc.y) <= 1.0f + 1e-3f);
                CHECK(ndc.z >= 0.0f && ndc.z <= 1.0f);
            }
        }
    }
}

// 用例 22：X↔Y 互逆、对角关系 tanD² = tanX² + tanY²
TEST_CASE(cameraFovConvert) {
    const float aspect = 2.0f;
    const float fovY = 45.0f;
    const float fovX = cCAMERA::FovConvert(eCAMERA_FOV_X, eCAMERA_FOV_Y, fovY, aspect);
    CHECK(fovX > fovY);
    CHECK_NEAR(cCAMERA::FovConvert(eCAMERA_FOV_Y, eCAMERA_FOV_X, fovX, aspect), fovY, 1e-4f);

    const float fovDiagonal = cCAMERA::FovConvert(eCAMERA_FOV_XY, eCAMERA_FOV_Y, fovY, aspect);
    const float tanY = std::tan(fovY * 0.5f * CAMERA_DEG_TO_RAD);
    const float tanX = tanY * aspect;
    const float expectedDiagonal =
        2.0f * std::atan(std::sqrt(tanX * tanX + tanY * tanY)) * CAMERA_RAD_TO_DEG;
    CHECK_NEAR(fovDiagonal, expectedDiagonal, 1e-4f);
    CHECK_NEAR(cCAMERA::FovConvert(eCAMERA_FOV_XY, eCAMERA_FOV_X, fovX, aspect), expectedDiagonal,
               1e-4f);
}
