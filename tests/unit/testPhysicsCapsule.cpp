// testPhysicsCapsule.cpp — 胶囊窄相单测：球-胶囊、胶囊-胶囊、盒-胶囊，加上胶囊构造/惯量/AABB/退化。
// 全部为固定算例（深度、法线、接触点可手工核对）；法线一律 A→B。
#include "testFramework.h"

#include <GN.h>
#include "GN_Physics.h"

#include <cmath>
#include <cstdint>

using namespace GN3d;

namespace {

const float kPi = 3.14159265358979f;

GNg::sVEC3 V(float x, float y, float z) {
    GNg::sVEC3 v = {x, y, z};
    return v;
}

float VecLength(const GNg::sVEC3 &v) {
    return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
}

bool IsUnit(const GNg::sVEC3 &v) {
    return std::fabs(VecLength(v) - 1.0f) < 1e-4f;
}

bool IsFiniteVec(const GNg::sVEC3 &v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

bool ManifoldSane(const sCONTACT_MANIFOLD &manifold) {
    if(manifold.aCount < 1 || manifold.aCount > 4) return false;
    if(!IsUnit(manifold.aNormal)) return false;
    for(uint32_t i = 0; i < manifold.aCount; ++i) {
        if(!IsFiniteVec(manifold.aPoints[i].aPoint)) return false;
        if(!(manifold.aPoints[i].aPenetration >= 0.0f)) return false;
    }
    return true;
}

GNg::sQUAT RotateAxisZ(float radians) {
    GNg::sQUAT rotation;
    const GNg::sVEC3 axis = V(0.0f, 0.0f, 1.0f);
    GNg::QuatFromAxisAngle(&rotation, &axis, radians);
    return rotation;
}

// 顶部在 y=0 的静态地面盒。
sRIGID_BODY MakeGround() {
    return MakeBoxBody(eBODY_STATIC, V(0.0f, -0.25f, 0.0f), V(2.0f, 0.25f, 2.0f), 0.0f);
}

void UpdateWorld(sRIGID_BODY &body) {
    BodyUpdateWorldState(body);
}

} // namespace

TEST_CASE(capsuleInertiaAndAabb) {
    // r=0.5, h=1, m=2：Iy = 0.5*2*0.25 = 0.25 ⇒ 逆 4；
    // 垂直轴 I = m(3r²+(2h)²)/12 = 2*(0.75+4)/12 = 0.7916667 ⇒ 逆 1.2631579。
    sRIGID_BODY capsule = MakeCapsuleBody(eBODY_DYNAMIC, V(1.0f, 2.0f, 3.0f), 0.5f, 1.0f, 2.0f);
    CHECK_NEAR(capsule.aInvMass, 0.5f, 1e-6f);
    CHECK_NEAR(capsule.aInvInertiaLocal.Elem(1, 1), 4.0f, 1e-4f); // 沿 Y（轴）
    CHECK_NEAR(capsule.aInvInertiaLocal.Elem(0, 0), 1.2631579f, 1e-4f);
    CHECK_NEAR(capsule.aInvInertiaLocal.Elem(2, 2), 1.2631579f, 1e-4f);
    // 竖直：AABB = 端球心 (1,1,3)/(1,3,3) 各扩 0.5。
    CHECK_NEAR(capsule.aWorldAABB.aMin.x, 0.5f, 1e-6f);
    CHECK_NEAR(capsule.aWorldAABB.aMin.y, 0.5f, 1e-6f);
    CHECK_NEAR(capsule.aWorldAABB.aMax.y, 3.5f, 1e-6f);

    // 绕 Z 转 90°：轴变 -X，AABB 半尺寸 = (h+r, r, r) = (1.5, 0.5, 0.5)。
    sRIGID_BODY lieDown = MakeCapsuleBody(eBODY_DYNAMIC, V(0.0f, 0.0f, 0.0f), 0.5f, 1.0f, 2.0f);
    lieDown.aRot = RotateAxisZ(kPi * 0.5f);
    UpdateWorld(lieDown);
    CHECK_NEAR(lieDown.aWorldAABB.Extent().x, 1.5f, 1e-4f);
    CHECK_NEAR(lieDown.aWorldAABB.Extent().y, 0.5f, 1e-4f);
    CHECK_NEAR(lieDown.aWorldAABB.Extent().z, 0.5f, 1e-4f);

    // halfHeight = 0 时退化为球：垂直轴 I = m*3r²/12 = m r²/4 = 0.125 ⇒ 逆 8。
    sRIGID_BODY sphereLike = MakeCapsuleBody(eBODY_DYNAMIC, V(0.0f, 0.0f, 0.0f), 0.5f, 0.0f, 2.0f);
    CHECK_NEAR(sphereLike.aInvInertiaLocal.Elem(0, 0), 8.0f, 1e-4f);
    CHECK_NEAR(sphereLike.aInvInertiaLocal.Elem(1, 1), 4.0f, 1e-4f);

    // 非 dynamic：无限质量、逆惯量全零。
    sRIGID_BODY staticCapsule = MakeCapsuleBody(eBODY_STATIC, V(0.0f, 0.0f, 0.0f), 0.5f, 1.0f, 5.0f);
    CHECK_NEAR(staticCapsule.aInvMass, 0.0f, 0.0f);
    for(int column = 0; column < 4; ++column) {
        for(int row = 0; row < 4; ++row) {
            CHECK_NEAR(staticCapsule.aInvInertiaLocal.Elem(column, row), 0.0f, 0.0f);
            CHECK_NEAR(staticCapsule.aInvInertiaWorld.Elem(column, row), 0.0f, 0.0f);
        }
    }
}

TEST_CASE(capsuleVerticalOnBoxFace) {
    sRIGID_BODY ground = MakeGround();
    // 胶囊竖直、下端球心正好落在盒顶面 y=0 ⇒ 穿透 = 半径 = 0.5。
    sRIGID_BODY capsule = MakeCapsuleBody(eBODY_DYNAMIC, V(0.0f, 1.0f, 0.0f), 0.5f, 1.0f, 1.0f);
    UpdateWorld(ground);
    UpdateWorld(capsule);

    sCONTACT_MANIFOLD manifold;
    CHECK(CollideBodies(ground, capsule, manifold)); // A 盒 → B 胶囊：法线 +Y
    CHECK(ManifoldSane(manifold));
    CHECK_EQ(manifold.aCount, 1u);
    CHECK_NEAR(manifold.aNormal.y, 1.0f, 1e-4f);
    CHECK_NEAR(manifold.aPoints[0].aPenetration, 0.5f, 1e-4f);
    CHECK_NEAR(manifold.aPoints[0].aPoint.y, 0.0f, 1e-4f);

    // A/B 交换：法线反号、深度不变。
    sCONTACT_MANIFOLD swapped;
    CHECK(CollideBodies(capsule, ground, swapped));
    CHECK_NEAR(swapped.aNormal.y, -1.0f, 1e-4f);
    CHECK_NEAR(swapped.aPoints[0].aPenetration, 0.5f, 1e-4f);
}

TEST_CASE(capsuleLyingOnBoxFace) {
    sRIGID_BODY ground = MakeGround();
    // 侧躺（轴沿 X），轴心离顶面 0.4 ⇒ 穿透 = 0.5 - 0.4 = 0.1。
    sRIGID_BODY capsule = MakeCapsuleBody(eBODY_DYNAMIC, V(0.0f, 0.4f, 0.0f), 0.5f, 1.0f, 1.0f);
    capsule.aRot = RotateAxisZ(kPi * 0.5f);
    UpdateWorld(ground);
    UpdateWorld(capsule);
    sCONTACT_MANIFOLD manifold;
    CHECK(CollideBodies(ground, capsule, manifold));
    CHECK(ManifoldSane(manifold));
    CHECK_NEAR(manifold.aNormal.x, 0.0f, 1e-4f);
    CHECK_NEAR(manifold.aNormal.y, 1.0f, 1e-4f);
    // 躺平是面接触：必须给出多点流形，且点沿胶囊轴（X）分布。
    CHECK(manifold.aCount >= 2);
    float minimumX = manifold.aPoints[0].aPoint.x;
    float maximumX = minimumX;
    for(uint32_t i = 0; i < manifold.aCount; ++i) {
        CHECK_NEAR(manifold.aPoints[i].aPoint.y, 0.0f, 1e-4f);
        CHECK_NEAR(manifold.aPoints[i].aPenetration, 0.1f, 1e-4f);
        if(manifold.aPoints[i].aPoint.x < minimumX) minimumX = manifold.aPoints[i].aPoint.x;
        if(manifold.aPoints[i].aPoint.x > maximumX) maximumX = manifold.aPoints[i].aPoint.x;
    }
    CHECK(maximumX - minimumX > 0.5f);
}

TEST_CASE(capsuleLyingTouchingManifold) {
    // Lead 的最小复现：躺平胶囊（轴沿 X）刚好贴在盒面 y=0 上（穿透=0）。
    sRIGID_BODY ground = MakeBoxBody(eBODY_STATIC, V(0.0f, -0.25f, 0.0f), V(10.0f, 0.25f, 10.0f), 0.0f);
    sRIGID_BODY capsule = MakeCapsuleBody(eBODY_DYNAMIC, V(0.0f, 0.5f, 0.0f), 0.5f, 1.0f, 1.0f);
    capsule.aRot = RotateAxisZ(kPi * 0.5f);
    UpdateWorld(ground);
    UpdateWorld(capsule);

    sCONTACT_MANIFOLD manifold;
    CHECK(CollideBodies(capsule, ground, manifold)); // A 胶囊 → B 盒：法线 -Y
    CHECK(ManifoldSane(manifold));
    CHECK_NEAR(manifold.aNormal.y, -1.0f, 1e-4f);
    CHECK(manifold.aCount >= 2);
    float minimumX = manifold.aPoints[0].aPoint.x;
    float maximumX = minimumX;
    for(uint32_t i = 0; i < manifold.aCount; ++i) {
        CHECK_NEAR(manifold.aPoints[i].aPoint.y, 0.0f, 1e-4f);
        CHECK_NEAR(manifold.aPoints[i].aPenetration, 0.0f, 1e-4f);
        if(manifold.aPoints[i].aPoint.x < minimumX) minimumX = manifold.aPoints[i].aPoint.x;
        if(manifold.aPoints[i].aPoint.x > maximumX) maximumX = manifold.aPoints[i].aPoint.x;
    }
    CHECK(maximumX - minimumX > 0.5f); // 接触点覆盖胶囊轴（半长 1，跨度应接近 2）
}

TEST_CASE(capsuleTilted45) {
    sRIGID_BODY ground = MakeGround();
    // 绕 Z 转 45°：下端球心 (0.7071, 0.2, 0) ⇒ 离顶面 0.2，穿透 = 0.5 - 0.2 = 0.3。
    sRIGID_BODY capsule = MakeCapsuleBody(eBODY_DYNAMIC, V(0.0f, 0.90710678f, 0.0f), 0.5f, 1.0f, 1.0f);
    capsule.aRot = RotateAxisZ(45.0f * kPi / 180.0f);
    UpdateWorld(ground);
    UpdateWorld(capsule);
    sCONTACT_MANIFOLD manifold;
    CHECK(CollideBodies(ground, capsule, manifold));
    CHECK(ManifoldSane(manifold));
    CHECK_NEAR(manifold.aNormal.y, 1.0f, 1e-3f);
    CHECK_NEAR(manifold.aPoints[0].aPenetration, 0.3f, 1e-3f);
    CHECK_NEAR(manifold.aPoints[0].aPoint.y, 0.0f, 1e-3f);
    CHECK_NEAR(manifold.aPoints[0].aPoint.x, 0.70710678f, 1e-3f);
}

TEST_CASE(capsuleSphereSideAndEnd) {
    sRIGID_BODY capsule = MakeCapsuleBody(eBODY_STATIC, V(0.0f, 0.0f, 0.0f), 0.5f, 1.0f, 0.0f);
    UpdateWorld(capsule);

    // 侧面：球心 (0.7,0.3,0)、r=0.4，轴最近点 (0,0.3,0) ⇒ 距离 0.7、穿透 0.2、法线 -X。
    sRIGID_BODY side = MakeSphereBody(eBODY_DYNAMIC, V(0.7f, 0.3f, 0.0f), 0.4f, 1.0f);
    UpdateWorld(side);
    sCONTACT_MANIFOLD manifold;
    CHECK(CollideBodies(side, capsule, manifold));
    CHECK(ManifoldSane(manifold));
    CHECK_NEAR(manifold.aNormal.x, -1.0f, 1e-4f);
    CHECK_NEAR(manifold.aPoints[0].aPenetration, 0.2f, 1e-4f);
    CHECK_NEAR(manifold.aPoints[0].aPoint.x, 0.4f, 1e-4f);

    // 端面：球心 (0,1.7,0)，轴最近点 (0,1,0) ⇒ 距离 0.7、穿透 0.2、法线 -Y。
    sRIGID_BODY end = MakeSphereBody(eBODY_DYNAMIC, V(0.0f, 1.7f, 0.0f), 0.4f, 1.0f);
    UpdateWorld(end);
    CHECK(CollideBodies(end, capsule, manifold));
    CHECK_NEAR(manifold.aNormal.y, -1.0f, 1e-4f);
    CHECK_NEAR(manifold.aPoints[0].aPenetration, 0.2f, 1e-4f);

    // 交换顺序：法线反号。
    sCONTACT_MANIFOLD swapped;
    CHECK(CollideBodies(capsule, side, swapped));
    CHECK_NEAR(swapped.aNormal.x, 1.0f, 1e-4f);
}

TEST_CASE(capsuleCapsuleParallel) {
    sRIGID_BODY a = MakeCapsuleBody(eBODY_STATIC, V(0.0f, 0.0f, 0.0f), 0.5f, 1.0f, 0.0f);
    sRIGID_BODY b = MakeCapsuleBody(eBODY_STATIC, V(0.6f, 0.2f, 0.0f), 0.5f, 1.0f, 0.0f);
    UpdateWorld(a);
    UpdateWorld(b);
    sCONTACT_MANIFOLD manifold;
    CHECK(CollideBodies(a, b, manifold));
    CHECK(ManifoldSane(manifold));
    CHECK_NEAR(manifold.aNormal.x, 1.0f, 1e-4f);
    CHECK_NEAR(manifold.aPoints[0].aPenetration, 0.4f, 1e-4f); // 1.0 - 0.6
    sCONTACT_MANIFOLD swapped;
    CHECK(CollideBodies(b, a, swapped));
    CHECK_NEAR(swapped.aNormal.x, -1.0f, 1e-4f);
    CHECK_NEAR(swapped.aPoints[0].aPenetration, 0.4f, 1e-4f);
}

TEST_CASE(capsuleCapsuleCross) {
    sRIGID_BODY a = MakeCapsuleBody(eBODY_STATIC, V(0.0f, 0.0f, 0.0f), 0.5f, 1.0f, 0.0f);
    sRIGID_BODY b = MakeCapsuleBody(eBODY_STATIC, V(0.0f, 0.0f, 0.3f), 0.5f, 1.0f, 0.0f);
    b.aRot = RotateAxisZ(kPi * 0.5f); // 轴沿 -X，与 A 的轴在 y=0 平面相交
    UpdateWorld(a);
    UpdateWorld(b);
    sCONTACT_MANIFOLD manifold;
    CHECK(CollideBodies(a, b, manifold));
    CHECK(ManifoldSane(manifold));
    CHECK_NEAR(manifold.aNormal.z, 1.0f, 1e-4f);
    CHECK_NEAR(manifold.aPoints[0].aPenetration, 0.7f, 1e-4f); // 1.0 - 0.3
    CHECK_NEAR(manifold.aPoints[0].aPoint.z, 0.15f, 1e-4f); // 最近的球心对中点
}

TEST_CASE(capsuleCapsuleEndToEnd) {
    sRIGID_BODY a = MakeCapsuleBody(eBODY_STATIC, V(0.0f, 0.0f, 0.0f), 0.5f, 1.0f, 0.0f);
    sRIGID_BODY b = MakeCapsuleBody(eBODY_STATIC, V(0.0f, 2.8f, 0.0f), 0.5f, 1.0f, 0.0f);
    UpdateWorld(a);
    UpdateWorld(b);
    sCONTACT_MANIFOLD manifold;
    CHECK(CollideBodies(a, b, manifold));
    CHECK(ManifoldSane(manifold));
    CHECK_NEAR(manifold.aNormal.y, 1.0f, 1e-4f);
    CHECK_NEAR(manifold.aPoints[0].aPenetration, 0.2f, 1e-4f); // 1.0 - 0.8
}

TEST_CASE(capsuleBoxCorner) {
    sRIGID_BODY box = MakeBoxBody(eBODY_STATIC, V(0.0f, 0.0f, 0.0f), V(1.0f, 1.0f, 1.0f), 0.0f);
    // 下端球心 (1.25,1.25,0)，离盒角 (1,1,0) 距离 0.353553 ⇒ 穿透 = 0.5 - 0.353553。
    sRIGID_BODY capsule = MakeCapsuleBody(eBODY_DYNAMIC, V(1.25f, 2.25f, 0.0f), 0.5f, 1.0f, 1.0f);
    UpdateWorld(box);
    UpdateWorld(capsule);
    sCONTACT_MANIFOLD manifold;
    CHECK(CollideBodies(box, capsule, manifold));
    CHECK(ManifoldSane(manifold));
    CHECK_NEAR(manifold.aNormal.x, 0.70710678f, 1e-3f);
    CHECK_NEAR(manifold.aNormal.y, 0.70710678f, 1e-3f);
    CHECK_NEAR(manifold.aPoints[0].aPenetration, 0.14644661f, 1e-3f);
    CHECK_NEAR(manifold.aPoints[0].aPoint.x, 1.0f, 1e-3f);
    CHECK_NEAR(manifold.aPoints[0].aPoint.y, 1.0f, 1e-3f);
}

TEST_CASE(capsuleBoxSymmetric) {
    sRIGID_BODY ground = MakeGround();
    sRIGID_BODY capsule = MakeCapsuleBody(eBODY_DYNAMIC, V(0.0f, 0.4f, 0.0f), 0.5f, 1.0f, 1.0f);
    capsule.aRot = RotateAxisZ(kPi * 0.5f);
    UpdateWorld(ground);
    UpdateWorld(capsule);
    sCONTACT_MANIFOLD forward;
    sCONTACT_MANIFOLD backward;
    CHECK(CollideBodies(ground, capsule, forward));
    CHECK(CollideBodies(capsule, ground, backward));
    CHECK_NEAR(forward.aNormal.x, -backward.aNormal.x, 1e-5f);
    CHECK_NEAR(forward.aNormal.y, -backward.aNormal.y, 1e-5f);
    CHECK_NEAR(forward.aNormal.z, -backward.aNormal.z, 1e-5f);
    CHECK_NEAR(forward.aPoints[0].aPenetration, backward.aPoints[0].aPenetration, 1e-4f);
}

TEST_CASE(capsuleDegenerate) {
    sRIGID_BODY box = MakeBoxBody(eBODY_STATIC, V(0.0f, 0.0f, 0.0f), V(1.0f, 1.0f, 1.0f), 0.0f);
    UpdateWorld(box);

    // halfHeight=0 的胶囊与同参数球完全等价（A 为胶囊/球，B 为盒）。
    sRIGID_BODY zeroHeight = MakeCapsuleBody(eBODY_DYNAMIC, V(1.3f, 0.0f, 0.0f), 0.5f, 0.0f, 1.0f);
    sRIGID_BODY sphere = MakeSphereBody(eBODY_DYNAMIC, V(1.3f, 0.0f, 0.0f), 0.5f, 1.0f);
    UpdateWorld(zeroHeight);
    UpdateWorld(sphere);
    sCONTACT_MANIFOLD capsuleManifold;
    sCONTACT_MANIFOLD sphereManifold;
    CHECK(CollideBodies(zeroHeight, box, capsuleManifold));
    CHECK(CollideBodies(sphere, box, sphereManifold));
    CHECK_EQ(capsuleManifold.aCount, sphereManifold.aCount);
    CHECK_NEAR(capsuleManifold.aNormal.x, sphereManifold.aNormal.x, 1e-5f);
    CHECK_NEAR(capsuleManifold.aPoints[0].aPenetration, sphereManifold.aPoints[0].aPenetration, 1e-5f);
    CHECK_NEAR(capsuleManifold.aPoints[0].aPoint.x, sphereManifold.aPoints[0].aPoint.x, 1e-5f);

    // halfHeight=0 的胶囊-胶囊与球-球等价。
    sRIGID_BODY capsuleA = MakeCapsuleBody(eBODY_STATIC, V(0.0f, 0.0f, 0.0f), 0.5f, 0.0f, 0.0f);
    sRIGID_BODY capsuleB = MakeCapsuleBody(eBODY_STATIC, V(0.8f, 0.0f, 0.0f), 0.5f, 0.0f, 0.0f);
    sRIGID_BODY sphereA = MakeSphereBody(eBODY_STATIC, V(0.0f, 0.0f, 0.0f), 0.5f, 0.0f);
    sRIGID_BODY sphereB = MakeSphereBody(eBODY_STATIC, V(0.8f, 0.0f, 0.0f), 0.5f, 0.0f);
    UpdateWorld(capsuleA);
    UpdateWorld(capsuleB);
    UpdateWorld(sphereA);
    UpdateWorld(sphereB);
    CHECK(CollideBodies(capsuleA, capsuleB, capsuleManifold));
    CHECK(CollideBodies(sphereA, sphereB, sphereManifold));
    CHECK_NEAR(capsuleManifold.aNormal.x, sphereManifold.aNormal.x, 1e-5f);
    CHECK_NEAR(capsuleManifold.aPoints[0].aPenetration, sphereManifold.aPoints[0].aPenetration, 1e-5f);

    // radius 非法：惯量降级为无限质量；碰撞返回 false 且不崩溃。
    sRIGID_BODY zeroRadius = MakeCapsuleBody(eBODY_DYNAMIC, V(0.0f, 0.0f, 0.0f), 0.0f, 1.0f, 2.0f);
    CHECK_NEAR(zeroRadius.aInvMass, 0.0f, 0.0f);
    CHECK_NEAR(zeroRadius.aInvInertiaLocal.Elem(1, 1), 0.0f, 0.0f);
    CHECK(!CollideBodies(zeroRadius, box, capsuleManifold));
    CHECK_EQ(capsuleManifold.aCount, 0u);

    // halfHeight 为负：同样降级（AABB 只按半径），碰撞返回 false 不崩溃。
    sRIGID_BODY negativeHeight = MakeCapsuleBody(eBODY_DYNAMIC, V(0.0f, 0.0f, 0.0f), 0.5f, -1.0f, 2.0f);
    CHECK_NEAR(negativeHeight.aInvMass, 0.0f, 0.0f);
    CHECK_NEAR(negativeHeight.aWorldAABB.Extent().y, 0.5f, 1e-6f);
    CHECK(!CollideBodies(negativeHeight, box, capsuleManifold));

    // 重合轴（共线重叠）：不崩溃，返回合法流形或 false。
    sRIGID_BODY overlapA = MakeCapsuleBody(eBODY_STATIC, V(0.0f, 0.0f, 0.0f), 0.5f, 1.0f, 0.0f);
    sRIGID_BODY overlapB = MakeCapsuleBody(eBODY_STATIC, V(0.0f, 0.5f, 0.0f), 0.5f, 1.0f, 0.0f);
    UpdateWorld(overlapA);
    UpdateWorld(overlapB);
    const bool overlap = CollideBodies(overlapA, overlapB, capsuleManifold);
    if(overlap) {
        CHECK(ManifoldSane(capsuleManifold));
    } else {
        CHECK_EQ(capsuleManifold.aCount, 0u);
    }
}
