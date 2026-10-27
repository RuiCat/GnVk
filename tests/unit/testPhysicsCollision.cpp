// testPhysicsCollision.cpp — 窄相碰撞单测：球-球 / 球-盒 / 盒-盒流形、惯量、世界 AABB/逆惯量、
//   退化输入与 A/B 交换对称性；全部为固定算例，便于手工核对深度与法线。
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

// 点是否落在世界 AABB 内（粗判接触点是否在两盒重叠区，允许 eps 的浮点边界误差）。
bool WithinAabb(const sAABB &box, const GNg::sVEC3 &point, float eps) {
    return point.x >= box.aMin.x - eps && point.x <= box.aMax.x + eps && point.y >= box.aMin.y - eps &&
           point.y <= box.aMax.y + eps && point.z >= box.aMin.z - eps && point.z <= box.aMax.z + eps;
}

GNg::sQUAT RotateAxisY(float radians) {
    GNg::sQUAT rotation;
    const GNg::sVEC3 axis = V(0.0f, 1.0f, 0.0f);
    GNg::QuatFromAxisAngle(&rotation, &axis, radians);
    return rotation;
}

GNg::sQUAT RotateAxisZ(float radians) {
    GNg::sQUAT rotation;
    const GNg::sVEC3 axis = V(0.0f, 0.0f, 1.0f);
    GNg::QuatFromAxisAngle(&rotation, &axis, radians);
    return rotation;
}

GNg::sQUAT RotateAxisX(float radians) {
    GNg::sQUAT rotation;
    const GNg::sVEC3 axis = V(1.0f, 0.0f, 0.0f);
    GNg::QuatFromAxisAngle(&rotation, &axis, radians);
    return rotation;
}

// 校验流形的基本契约：法线单位长、点数 1..4、每点穿透 >= 0 且为世界坐标有限值。
bool ManifoldSane(const sCONTACT_MANIFOLD &manifold) {
    if(manifold.aCount < 1 || manifold.aCount > 4) return false;
    if(!IsUnit(manifold.aNormal)) return false;
    for(uint32_t i = 0; i < manifold.aCount; ++i) {
        if(!IsFiniteVec(manifold.aPoints[i].aPoint)) return false;
        if(!(manifold.aPoints[i].aPenetration >= 0.0f)) return false;
        if(!std::isfinite(manifold.aPoints[i].aPenetration)) return false;
    }
    return true;
}

void UpdateWorld(sRIGID_BODY &body) {
    BodyUpdateWorldState(body);
}

} // namespace

TEST_CASE(collisionSphereSphere) {
    sRIGID_BODY a = MakeSphereBody(eBODY_DYNAMIC, V(0.0f, 0.0f, 0.0f), 1.0f, 1.0f);
    sRIGID_BODY b = MakeSphereBody(eBODY_DYNAMIC, V(1.5f, 0.0f, 0.0f), 1.0f, 1.0f);
    UpdateWorld(a);
    UpdateWorld(b);
    sCONTACT_MANIFOLD manifold;
    CHECK(CollideBodies(a, b, manifold));
    CHECK(ManifoldSane(manifold));
    CHECK_EQ(manifold.aCount, 1u);
    CHECK_NEAR(manifold.aNormal.x, 1.0f, 1e-4f); // A → B：+x
    CHECK_NEAR(manifold.aNormal.y, 0.0f, 1e-4f);
    CHECK_NEAR(manifold.aNormal.z, 0.0f, 1e-4f);
    CHECK_NEAR(manifold.aPoints[0].aPenetration, 0.5f, 1e-4f); // (1+1) - 1.5
    CHECK_NEAR(manifold.aPoints[0].aPoint.x, 0.75f, 1e-4f); // a.pos + n*(r - pen/2)

    // 刚好相切：距离 = 半径和 ⇒ 深度 ≈ 0，但仍算接触。
    sRIGID_BODY touching = MakeSphereBody(eBODY_DYNAMIC, V(2.0f, 0.0f, 0.0f), 1.0f, 1.0f);
    UpdateWorld(touching);
    CHECK(CollideBodies(a, touching, manifold));
    CHECK_NEAR(manifold.aPoints[0].aPenetration, 0.0f, 1e-4f);

    // 分离：无接触，输出被清空。
    sRIGID_BODY apart = MakeSphereBody(eBODY_DYNAMIC, V(5.0f, 0.0f, 0.0f), 1.0f, 1.0f);
    UpdateWorld(apart);
    CHECK(!CollideBodies(a, apart, manifold));
    CHECK_EQ(manifold.aCount, 0u);
    CHECK_NEAR(VecLength(manifold.aNormal), 0.0f, 1e-6f);
}

TEST_CASE(collisionSphereBoxFaceAndCorner) {
    sRIGID_BODY box = MakeBoxBody(eBODY_STATIC, V(0.0f, 0.0f, 0.0f), V(1.0f, 1.0f, 1.0f), 0.0f);
    UpdateWorld(box);

    // 固定算例 1：球心 (1.5,0,0) 半径 1，最近点 (1,0,0)，距离 0.5 ⇒ 深度 = 1 - 0.5 = 0.5。
    sRIGID_BODY face = MakeSphereBody(eBODY_DYNAMIC, V(1.5f, 0.0f, 0.0f), 1.0f, 1.0f);
    UpdateWorld(face);
    sCONTACT_MANIFOLD manifold;
    CHECK(CollideBodies(face, box, manifold));
    CHECK(ManifoldSane(manifold));
    CHECK_NEAR(manifold.aNormal.x, -1.0f, 1e-4f); // A 球 → B 盒
    CHECK_NEAR(manifold.aPoints[0].aPenetration, 0.5f, 1e-4f);
    CHECK_NEAR(manifold.aPoints[0].aPoint.x, 1.0f, 1e-4f);
    CHECK_NEAR(manifold.aPoints[0].aPoint.y, 0.0f, 1e-4f);

    // 固定算例 2：球在角上，最近点为角 (1,1,0)，距离 √0.5，深度 = 1 - 0.7071 = 0.2929。
    sRIGID_BODY corner = MakeSphereBody(eBODY_DYNAMIC, V(1.5f, 1.5f, 0.0f), 1.0f, 1.0f);
    UpdateWorld(corner);
    CHECK(CollideBodies(corner, box, manifold));
    CHECK(ManifoldSane(manifold));
    CHECK_NEAR(manifold.aNormal.x, -0.70710678f, 1e-4f);
    CHECK_NEAR(manifold.aNormal.y, -0.70710678f, 1e-4f);
    CHECK_NEAR(manifold.aPoints[0].aPenetration, 0.29289322f, 1e-4f);
    CHECK_NEAR(manifold.aPoints[0].aPoint.x, 1.0f, 1e-4f);
    CHECK_NEAR(manifold.aPoints[0].aPoint.y, 1.0f, 1e-4f);

    // 固定算例 3：球心在盒内且靠近 +x 面，最近面距离 0.1，深度 = 0.5 + 0.1 = 0.6。
    sRIGID_BODY inside = MakeSphereBody(eBODY_DYNAMIC, V(0.9f, 0.0f, 0.0f), 0.5f, 1.0f);
    UpdateWorld(inside);
    CHECK(CollideBodies(inside, box, manifold));
    CHECK(ManifoldSane(manifold));
    CHECK_NEAR(manifold.aNormal.x, -1.0f, 1e-4f);
    CHECK_NEAR(manifold.aPoints[0].aPenetration, 0.6f, 1e-4f);
    CHECK_NEAR(manifold.aPoints[0].aPoint.x, 1.0f, 1e-4f);

    // 连续性：球心刚穿出盒面（0.9 → 1.1）时法线方向不变，只是深度从 0.6 变到 0.4。
    sRIGID_BODY justOutside = MakeSphereBody(eBODY_DYNAMIC, V(1.1f, 0.0f, 0.0f), 0.5f, 1.0f);
    UpdateWorld(justOutside);
    CHECK(CollideBodies(justOutside, box, manifold));
    CHECK_NEAR(manifold.aNormal.x, -1.0f, 1e-4f);
    CHECK_NEAR(manifold.aPoints[0].aPenetration, 0.4f, 1e-4f);

    // 分离：球在面外 1.5 个半径处（中心 2.6，半径 0.5，盒面 1）⇒ 无接触。
    sRIGID_BODY apart = MakeSphereBody(eBODY_DYNAMIC, V(2.6f, 0.0f, 0.0f), 0.5f, 1.0f);
    UpdateWorld(apart);
    CHECK(!CollideBodies(apart, box, manifold));
    CHECK_EQ(manifold.aCount, 0u);
}

TEST_CASE(collisionSphereBoxSwapped) {
    sRIGID_BODY box = MakeBoxBody(eBODY_STATIC, V(0.0f, 0.0f, 0.0f), V(1.0f, 1.0f, 1.0f), 0.0f);
    sRIGID_BODY sphere = MakeSphereBody(eBODY_DYNAMIC, V(1.5f, 0.0f, 0.0f), 1.0f, 1.0f);
    UpdateWorld(box);
    UpdateWorld(sphere);

    sCONTACT_MANIFOLD manifold;
    CHECK(CollideBodies(box, sphere, manifold)); // A 盒 → B 球：法线 +x
    CHECK(ManifoldSane(manifold));
    CHECK_NEAR(manifold.aNormal.x, 1.0f, 1e-4f);
    CHECK_NEAR(manifold.aPoints[0].aPenetration, 0.5f, 1e-4f);
    CHECK_NEAR(manifold.aPoints[0].aPoint.x, 1.0f, 1e-4f);
}

TEST_CASE(collisionBoxBoxAxisAligned) {
    sRIGID_BODY a = MakeBoxBody(eBODY_STATIC, V(0.0f, 0.0f, 0.0f), V(1.0f, 1.0f, 1.0f), 0.0f);
    sRIGID_BODY b = MakeBoxBody(eBODY_STATIC, V(1.5f, 0.0f, 0.0f), V(1.0f, 1.0f, 1.0f), 0.0f);
    UpdateWorld(a);
    UpdateWorld(b);
    sCONTACT_MANIFOLD manifold;
    CHECK(CollideBodies(a, b, manifold));
    CHECK(ManifoldSane(manifold));
    CHECK(manifold.aCount >= 1 && manifold.aCount <= 4);
    // 最短轴 = x，重叠 = 1 + 1 - 1.5 = 0.5，法线 A → B = +x。
    CHECK_NEAR(manifold.aNormal.x, 1.0f, 1e-4f);
    CHECK_NEAR(manifold.aNormal.y, 0.0f, 1e-4f);
    CHECK_NEAR(manifold.aNormal.z, 0.0f, 1e-4f);
    CHECK_NEAR(manifold.aPoints[0].aPenetration, 0.5f, 1e-4f);
    for(uint32_t i = 0; i < manifold.aCount; ++i) {
        CHECK(WithinAabb(a.aWorldAABB, manifold.aPoints[i].aPoint, 1e-4f));
        CHECK(WithinAabb(b.aWorldAABB, manifold.aPoints[i].aPoint, 1e-4f));
        CHECK_NEAR(manifold.aPoints[i].aPenetration, 0.5f, 1e-4f);
    }
}

TEST_CASE(collisionBoxBoxRotated) {
    sRIGID_BODY a = MakeBoxBody(eBODY_STATIC, V(0.0f, 0.0f, 0.0f), V(1.0f, 1.0f, 1.0f), 0.0f);
    sRIGID_BODY b = MakeBoxBody(eBODY_STATIC, V(0.9f, 0.0f, 0.0f), V(0.5f, 0.5f, 0.5f), 0.0f);
    b.aRot = RotateAxisY(45.0f * kPi / 180.0f);
    UpdateWorld(a);
    UpdateWorld(b);

    // 绕 Y 转 45° 后世界 AABB 的 x/z 半径 = 0.5*(cos45 + sin45) = 0.7071 > 0.5。
    CHECK_NEAR(b.aWorldAABB.Extent().x, 0.70710678f, 1e-4f);
    CHECK(b.aWorldAABB.Extent().x > 0.5f);

    sCONTACT_MANIFOLD manifold;
    CHECK(CollideBodies(a, b, manifold));
    CHECK(ManifoldSane(manifold));
    // A 的 ±x 面轴重叠 0.5 + 0.7071 - 0.9 = 0.3071？不：0.5 是 B 半尺寸，投影后 0.7071。
    // 实际重叠 = 1 + 0.70710678 - 0.9 = 0.80710678。
    CHECK_NEAR(manifold.aNormal.x, 1.0f, 1e-4f);
    CHECK_NEAR(manifold.aNormal.y, 0.0f, 1e-4f);
    CHECK_NEAR(manifold.aNormal.z, 0.0f, 1e-4f);
    CHECK_NEAR(manifold.aPoints[0].aPenetration, 0.80710678f, 1e-3f);
    for(uint32_t i = 0; i < manifold.aCount; ++i) {
        CHECK(WithinAabb(a.aWorldAABB, manifold.aPoints[i].aPoint, 1e-4f));
        CHECK(WithinAabb(b.aWorldAABB, manifold.aPoints[i].aPoint, 1e-4f));
    }
}

TEST_CASE(collisionBoxBoxSeparatedAndTilted) {
    sRIGID_BODY a = MakeBoxBody(eBODY_STATIC, V(0.0f, 0.0f, 0.0f), V(1.0f, 1.0f, 1.0f), 0.0f);
    sRIGID_BODY far = MakeBoxBody(eBODY_STATIC, V(5.0f, 0.0f, 0.0f), V(1.0f, 1.0f, 1.0f), 0.0f);
    UpdateWorld(a);
    UpdateWorld(far);
    sCONTACT_MANIFOLD manifold;
    CHECK(!CollideBodies(a, far, manifold));
    CHECK_EQ(manifold.aCount, 0u);

    // 倾斜盒（同时绕 Z 转 30°）：只校验流形合法、穿透 >= 0、接触点有限，不锁死内部选轴分支。
    sRIGID_BODY tilted = MakeBoxBody(eBODY_STATIC, V(1.2f, 0.2f, 0.1f), V(0.6f, 0.5f, 0.4f), 0.0f);
    tilted.aRot = RotateAxisZ(30.0f * kPi / 180.0f);
    UpdateWorld(tilted);
    CHECK(CollideBodies(a, tilted, manifold));
    CHECK(ManifoldSane(manifold));

    // 棱-棱构型：两盒各绕多轴旋转、棱接近垂直且不共面 ⇒ SAT 最小轴来自棱叉积；接触点须落在两盒内。
    sRIGID_BODY edgeA = MakeBoxBody(eBODY_STATIC, V(0.0f, 0.0f, 0.0f), V(1.0f, 0.7f, 0.3f), 0.0f);
    edgeA.aRot = RotateAxisY(17.0f * kPi / 180.0f) * RotateAxisX(39.0f * kPi / 180.0f);
    sRIGID_BODY edgeB = MakeBoxBody(eBODY_STATIC, V(1.1f, 0.35f, 0.15f), V(0.8f, 0.5f, 0.2f), 0.0f);
    edgeB.aRot = RotateAxisX(39.0f * kPi / 180.0f) * edgeA.aRot;
    UpdateWorld(edgeA);
    UpdateWorld(edgeB);
    CHECK(CollideBodies(edgeA, edgeB, manifold));
    CHECK(ManifoldSane(manifold));
    for(uint32_t i = 0; i < manifold.aCount; ++i) {
        CHECK(WithinAabb(edgeA.aWorldAABB, manifold.aPoints[i].aPoint, 1e-4f));
        CHECK(WithinAabb(edgeB.aWorldAABB, manifold.aPoints[i].aPoint, 1e-4f));
    }
}

TEST_CASE(collisionInertia) {
    // 球：I = 2/5 m r² = 0.4 * 2 * 0.25 = 0.2 ⇒ 逆惯量 5。
    sRIGID_BODY sphere = MakeSphereBody(eBODY_DYNAMIC, V(0.0f, 0.0f, 0.0f), 0.5f, 2.0f);
    CHECK_NEAR(sphere.aInvMass, 0.5f, 1e-6f);
    CHECK_NEAR(sphere.aInvInertiaLocal.Elem(0, 0), 5.0f, 1e-4f);
    CHECK_NEAR(sphere.aInvInertiaLocal.Elem(1, 1), 5.0f, 1e-4f);
    CHECK_NEAR(sphere.aInvInertiaLocal.Elem(2, 2), 5.0f, 1e-4f);

    // 盒：m=3、half=(0.5,1,1.5) ⇒ Ix=3.25, Iy=2.5, Iz=1.25。
    sRIGID_BODY box = MakeBoxBody(eBODY_DYNAMIC, V(0.0f, 0.0f, 0.0f), V(0.5f, 1.0f, 1.5f), 3.0f);
    CHECK_NEAR(box.aInvMass, 1.0f / 3.0f, 1e-6f);
    CHECK_NEAR(box.aInvInertiaLocal.Elem(0, 0), 1.0f / 3.25f, 1e-4f);
    CHECK_NEAR(box.aInvInertiaLocal.Elem(1, 1), 1.0f / 2.5f, 1e-4f);
    CHECK_NEAR(box.aInvInertiaLocal.Elem(2, 2), 1.0f / 1.25f, 1e-4f);
    CHECK_NEAR(box.aInvInertiaLocal.Elem(0, 1), 0.0f, 1e-6f);
    CHECK_NEAR(box.aInvInertiaLocal.Elem(2, 1), 0.0f, 1e-6f);

    // 非 dynamic：无限质量，逆惯量全零。
    sRIGID_BODY staticSphere = MakeSphereBody(eBODY_STATIC, V(0.0f, 0.0f, 0.0f), 1.0f, 5.0f);
    sRIGID_BODY kinematicBox = MakeBoxBody(eBODY_KINEMATIC, V(0.0f, 0.0f, 0.0f), V(1.0f, 1.0f, 1.0f), 5.0f);
    CHECK_NEAR(staticSphere.aInvMass, 0.0f, 0.0f);
    CHECK_NEAR(kinematicBox.aInvMass, 0.0f, 0.0f);
    for(int column = 0; column < 4; ++column) {
        for(int row = 0; row < 4; ++row) {
            CHECK_NEAR(staticSphere.aInvInertiaLocal.Elem(column, row), 0.0f, 0.0f);
            CHECK_NEAR(kinematicBox.aInvInertiaLocal.Elem(column, row), 0.0f, 0.0f);
        }
    }
    // 非 dynamic 即使调用 UpdateWorldState，世界逆惯量也保持全零。
    UpdateWorld(kinematicBox);
    for(int column = 0; column < 4; ++column) {
        for(int row = 0; row < 4; ++row) {
            CHECK_NEAR(kinematicBox.aInvInertiaWorld.Elem(column, row), 0.0f, 0.0f);
        }
    }
}

TEST_CASE(collisionWorldState) {
    // 球：AABB = 圆心 ± r。
    sRIGID_BODY sphere = MakeSphereBody(eBODY_DYNAMIC, V(1.0f, 2.0f, 3.0f), 0.5f, 1.0f);
    CHECK_NEAR(sphere.aWorldAABB.aMin.x, 0.5f, 1e-6f);
    CHECK_NEAR(sphere.aWorldAABB.aMin.y, 1.5f, 1e-6f);
    CHECK_NEAR(sphere.aWorldAABB.aMin.z, 2.5f, 1e-6f);
    CHECK_NEAR(sphere.aWorldAABB.aMax.x, 1.5f, 1e-6f);
    CHECK_NEAR(sphere.aWorldAABB.aMax.y, 2.5f, 1e-6f);
    CHECK_NEAR(sphere.aWorldAABB.aMax.z, 3.5f, 1e-6f);

    // 未旋转盒：AABB = 中心 ± 半尺寸。
    sRIGID_BODY axisAligned = MakeBoxBody(eBODY_STATIC, V(0.0f, 0.0f, 0.0f), V(1.0f, 0.5f, 2.0f), 0.0f);
    UpdateWorld(axisAligned);
    CHECK_NEAR(axisAligned.aWorldAABB.Extent().x, 1.0f, 1e-6f);
    CHECK_NEAR(axisAligned.aWorldAABB.Extent().z, 2.0f, 1e-6f);

    // 绕 Y 转 45°：x/z 半径 = cos45*1 + sin45*2 = 2.1213203，y 保持不变。
    sRIGID_BODY rotated = MakeBoxBody(eBODY_STATIC, V(0.0f, 0.0f, 0.0f), V(1.0f, 0.5f, 2.0f), 0.0f);
    rotated.aRot = RotateAxisY(45.0f * kPi / 180.0f);
    UpdateWorld(rotated);
    CHECK_NEAR(rotated.aWorldAABB.Extent().x, 2.12132034f, 1e-4f);
    CHECK_NEAR(rotated.aWorldAABB.Extent().y, 0.5f, 1e-4f);
    CHECK_NEAR(rotated.aWorldAABB.Extent().z, 2.12132034f, 1e-4f);
    CHECK(rotated.aWorldAABB.Extent().x > axisAligned.aWorldAABB.Extent().x);

    // 与 8 个角点的包围盒一致（不能只取中心 ± 半尺寸）。
    GNg::sVEC3 cornerMin = V(1e30f, 1e30f, 1e30f);
    GNg::sVEC3 cornerMax = V(-1e30f, -1e30f, -1e30f);
    const float signs[2] = {-1.0f, 1.0f};
    for(int sx = 0; sx < 2; ++sx) {
        for(int sy = 0; sy < 2; ++sy) {
            for(int sz = 0; sz < 2; ++sz) {
                const GNg::sVEC3 local = V(signs[sx] * 1.0f, signs[sy] * 0.5f, signs[sz] * 2.0f);
                GNg::sVEC3 world;
                GNg::QuatRotateVec3(&world, &local, &rotated.aRot);
                const float xs = world.x;
                const float ys = world.y;
                const float zs = world.z;
                if(xs < cornerMin.x) cornerMin.x = xs;
                if(ys < cornerMin.y) cornerMin.y = ys;
                if(zs < cornerMin.z) cornerMin.z = zs;
                if(xs > cornerMax.x) cornerMax.x = xs;
                if(ys > cornerMax.y) cornerMax.y = ys;
                if(zs > cornerMax.z) cornerMax.z = zs;
            }
        }
    }
    CHECK_NEAR(rotated.aWorldAABB.aMin.x, cornerMin.x, 1e-4f);
    CHECK_NEAR(rotated.aWorldAABB.aMin.y, cornerMin.y, 1e-4f);
    CHECK_NEAR(rotated.aWorldAABB.aMin.z, cornerMin.z, 1e-4f);
    CHECK_NEAR(rotated.aWorldAABB.aMax.x, cornerMax.x, 1e-4f);
    CHECK_NEAR(rotated.aWorldAABB.aMax.y, cornerMax.y, 1e-4f);
    CHECK_NEAR(rotated.aWorldAABB.aMax.z, cornerMax.z, 1e-4f);

    // 世界逆惯量 = R * invI_local * Rᵀ（绕 Y 45°，局部对角 0.3076923/0.4/0.8）。
    sRIGID_BODY inertiaBox = MakeBoxBody(eBODY_DYNAMIC, V(0.0f, 0.0f, 0.0f), V(0.5f, 1.0f, 1.5f), 3.0f);
    inertiaBox.aRot = RotateAxisY(45.0f * kPi / 180.0f);
    UpdateWorld(inertiaBox);
    const float expectedDiag = 0.5f * (1.0f / 3.25f + 1.0f / 1.25f); // 0.5538462
    const float expectedOff = 0.5f * (1.0f / 1.25f - 1.0f / 3.25f); // 0.2461538
    CHECK_NEAR(inertiaBox.aInvInertiaWorld.Elem(0, 0), expectedDiag, 1e-4f);
    CHECK_NEAR(inertiaBox.aInvInertiaWorld.Elem(1, 1), 0.4f, 1e-4f);
    CHECK_NEAR(inertiaBox.aInvInertiaWorld.Elem(2, 2), expectedDiag, 1e-4f);
    CHECK_NEAR(inertiaBox.aInvInertiaWorld.Elem(0, 2), expectedOff, 1e-4f);
    CHECK_NEAR(inertiaBox.aInvInertiaWorld.Elem(2, 0), expectedOff, 1e-4f);
    CHECK_NEAR(inertiaBox.aInvInertiaWorld.Elem(0, 1), 0.0f, 1e-5f);
    CHECK_NEAR(inertiaBox.aInvInertiaWorld.Elem(1, 0), 0.0f, 1e-5f);
}

TEST_CASE(collisionDegenerate) {
    sCONTACT_MANIFOLD manifold;

    // 零半径球 / 零半尺寸盒：不崩溃，返回无接触。
    sRIGID_BODY validSphere = MakeSphereBody(eBODY_DYNAMIC, V(0.0f, 0.0f, 0.0f), 1.0f, 1.0f);
    sRIGID_BODY zeroSphere = MakeSphereBody(eBODY_DYNAMIC, V(0.5f, 0.0f, 0.0f), 0.0f, 1.0f);
    UpdateWorld(validSphere);
    UpdateWorld(zeroSphere);
    CHECK(!CollideBodies(validSphere, zeroSphere, manifold));
    CHECK_EQ(manifold.aCount, 0u);

    sRIGID_BODY validBox = MakeBoxBody(eBODY_STATIC, V(0.0f, 0.0f, 0.0f), V(1.0f, 1.0f, 1.0f), 0.0f);
    sRIGID_BODY flatBox = MakeBoxBody(eBODY_STATIC, V(0.5f, 0.0f, 0.0f), V(0.0f, 1.0f, 1.0f), 0.0f);
    UpdateWorld(validBox);
    UpdateWorld(flatBox);
    CHECK(!CollideBodies(validBox, flatBox, manifold));
    CHECK(!CollideBodies(flatBox, validBox, manifold));
    CHECK_EQ(manifold.aCount, 0u);

    // 同心球：法线不确定，按退化返回 false。
    sRIGID_BODY twin = MakeSphereBody(eBODY_DYNAMIC, V(0.0f, 0.0f, 0.0f), 1.0f, 1.0f);
    UpdateWorld(twin);
    CHECK(!CollideBodies(validSphere, twin, manifold));
    CHECK_EQ(manifold.aCount, 0u);

    // 非有限参数：不崩溃，返回 false。
    sRIGID_BODY nanSphere = MakeSphereBody(eBODY_DYNAMIC, V(std::nanf(""), 0.0f, 0.0f), 1.0f, 1.0f);
    CHECK(!CollideBodies(validSphere, nanSphere, manifold));

    // 质量 0 / 负质量 / 零半径：惯量走"无限质量"降级，全零且不崩溃。
    sRIGID_BODY zeroMass = MakeSphereBody(eBODY_DYNAMIC, V(0.0f, 0.0f, 0.0f), 1.0f, 0.0f);
    sRIGID_BODY negativeMass = MakeBoxBody(eBODY_DYNAMIC, V(0.0f, 0.0f, 0.0f), V(1.0f, 1.0f, 1.0f), -2.0f);
    BodyUpdateInertia(zeroMass);
    BodyUpdateInertia(negativeMass);
    CHECK_NEAR(zeroMass.aInvMass, 0.0f, 0.0f);
    CHECK_NEAR(negativeMass.aInvMass, 0.0f, 0.0f);
    CHECK_NEAR(zeroMass.aInvInertiaLocal.Elem(0, 0), 0.0f, 0.0f);
    CHECK_NEAR(negativeMass.aInvInertiaLocal.Elem(1, 1), 0.0f, 0.0f);
}

TEST_CASE(collisionSymmetry) {
    sCONTACT_MANIFOLD forward;
    sCONTACT_MANIFOLD backward;

    // 球-球。
    sRIGID_BODY sphereA = MakeSphereBody(eBODY_DYNAMIC, V(0.0f, 0.0f, 0.0f), 1.0f, 1.0f);
    sRIGID_BODY sphereB = MakeSphereBody(eBODY_DYNAMIC, V(1.5f, 0.2f, -0.3f), 0.8f, 1.0f);
    UpdateWorld(sphereA);
    UpdateWorld(sphereB);
    CHECK(CollideBodies(sphereA, sphereB, forward));
    CHECK(CollideBodies(sphereB, sphereA, backward));
    CHECK_NEAR(forward.aNormal.x, -backward.aNormal.x, 1e-5f);
    CHECK_NEAR(forward.aNormal.y, -backward.aNormal.y, 1e-5f);
    CHECK_NEAR(forward.aNormal.z, -backward.aNormal.z, 1e-5f);
    CHECK_NEAR(forward.aPoints[0].aPenetration, backward.aPoints[0].aPenetration, 1e-5f);

    // 球-盒（两种 A/B 顺序）。
    sRIGID_BODY sphere = MakeSphereBody(eBODY_DYNAMIC, V(1.4f, 0.2f, 0.3f), 0.6f, 1.0f);
    sRIGID_BODY box = MakeBoxBody(eBODY_STATIC, V(0.0f, 0.0f, 0.0f), V(1.0f, 1.0f, 1.0f), 0.0f);
    UpdateWorld(sphere);
    UpdateWorld(box);
    CHECK(CollideBodies(sphere, box, forward));
    CHECK(CollideBodies(box, sphere, backward));
    CHECK_NEAR(forward.aNormal.x, -backward.aNormal.x, 1e-5f);
    CHECK_NEAR(forward.aNormal.y, -backward.aNormal.y, 1e-5f);
    CHECK_NEAR(forward.aNormal.z, -backward.aNormal.z, 1e-5f);
    CHECK_NEAR(forward.aPoints[0].aPenetration, backward.aPoints[0].aPenetration, 1e-4f);

    // 盒-盒（最短轴唯一，交换后法线反号、深度一致）。
    sRIGID_BODY boxA = MakeBoxBody(eBODY_STATIC, V(0.0f, 0.0f, 0.0f), V(1.0f, 0.8f, 0.6f), 0.0f);
    sRIGID_BODY boxB = MakeBoxBody(eBODY_STATIC, V(1.5f, 0.2f, 0.1f), V(0.7f, 0.5f, 0.9f), 0.0f);
    boxB.aRot = RotateAxisY(20.0f * kPi / 180.0f);
    UpdateWorld(boxA);
    UpdateWorld(boxB);
    CHECK(CollideBodies(boxA, boxB, forward));
    CHECK(CollideBodies(boxB, boxA, backward));
    CHECK(ManifoldSane(forward));
    CHECK(ManifoldSane(backward));
    CHECK_NEAR(forward.aNormal.x, -backward.aNormal.x, 1e-5f);
    CHECK_NEAR(forward.aNormal.y, -backward.aNormal.y, 1e-5f);
    CHECK_NEAR(forward.aNormal.z, -backward.aNormal.z, 1e-5f);
    CHECK_NEAR(forward.aPoints[0].aPenetration, backward.aPoints[0].aPenetration, 1e-4f);
}
