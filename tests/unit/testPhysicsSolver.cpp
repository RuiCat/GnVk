// testPhysicsSolver.cpp — 顺序冲量求解器单测（纯 CPU；手工构造 sRIGID_BODY + sCONTACT_MANIFOLD，
// 不依赖窄相 CollideBodies / MakeSphereBody，也不依赖 GN_Physics.cpp 的辅助函数）。
//
// 数值口径：线性容差 1e-4~1e-3；所有刚体惯量按球体 I = 2/5 m r² 手填世界逆惯量。
// 隔离编译链缺 GN.h 依赖目录，这里与 GN_PhysicsSolver.cpp 相同的最小前置定义。
#include "_SysDef/GN_SysDef.h"

typedef unsigned char uint8;
typedef signed char int8;

#ifndef GT_ANGLE_UNIT
#define GT_ANGLE_UNIT 0.01745329252f
#define GT_ANGLE_180 3.1415926536f
#define GT_ANGLE_360 6.2831853072f
#endif

#ifndef GN_FLOAT_MAX
#define GN_FLOAT_MAX 600000000.0f
#endif

#ifndef GN_DotAgToRadian
#define GN_DotAgToRadian(ag) (ag <= -1.0f ? GT_ANGLE_180 : (ag >= 1.0f ? 0.0f : acosf(ag)))
#endif

#include "GN_Physics.h"
#include "testFramework.h"

#include <cmath>
#include <cstring>

using namespace GN3d;

namespace {

const float kDt = 1.0f / 60.0f;

GNg::sVEC3 vec(float x, float y, float z) {
    GNg::sVEC3 v;
    v.x = x;
    v.y = y;
    v.z = z;
    return v;
}

GNg::sVEC3 vecSub(const GNg::sVEC3 &a, const GNg::sVEC3 &b) {
    return vec(a.x - b.x, a.y - b.y, a.z - b.z);
}

float vecDot(const GNg::sVEC3 &a, const GNg::sVEC3 &b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

GNg::sMAT44 zeroMatrix() {
    GNg::sMAT44 m;
    m.Init(false);
    return m;
}

GNg::sMAT44 diagonalMatrix(float diagonal) {
    GNg::sMAT44 m = zeroMatrix();
    m.m[0] = diagonal;
    m.m[5] = diagonal;
    m.m[10] = diagonal;
    return m;
}

// 动态球：质量 mass，惯量按实心球 2/5 m r² 手算世界逆惯量（不调用 BodyUpdateWorldState）。
sRIGID_BODY makeDynamicSphere(const GNg::sVEC3 &pos, float radius, float mass) {
    sRIGID_BODY body;
    body.aType = eBODY_DYNAMIC;
    body.aShape = eSHAPE_SPHERE;
    body.aMass = mass;
    body.aInvMass = 1.0f / mass;
    body.aPos = pos;
    body.aRot = GNg::sQUAT(0.0f, 0.0f, 0.0f, 1.0f);
    body.aLinVel = vec(0.0f, 0.0f, 0.0f);
    body.aAngVel = vec(0.0f, 0.0f, 0.0f);
    const float inverseInertia = 2.5f / (mass * radius * radius);
    body.aInvInertiaLocal = diagonalMatrix(inverseInertia);
    body.aInvInertiaWorld = diagonalMatrix(inverseInertia);
    body.aFriction = 0.5f;
    body.aRestitution = 0.0f;
    body.aLinearDamping = 0.0f;
    body.aAngularDamping = 0.0f;
    body.aRadius = radius;
    body.aHalfExtent = vec(0.0f, 0.0f, 0.0f);
    body.aSleeping = false;
    body.aSleepTime = 0.0f;
    body.aWorldAABB = sAABB();
    return body;
}

// 静态盒：无限质量，速度/位置绝不允许被求解器改动。
sRIGID_BODY makeStaticBox(const GNg::sVEC3 &pos, const GNg::sVEC3 &halfExtent) {
    sRIGID_BODY body;
    body.aType = eBODY_STATIC;
    body.aShape = eSHAPE_BOX;
    body.aMass = 0.0f;
    body.aInvMass = 0.0f;
    body.aPos = pos;
    body.aRot = GNg::sQUAT(0.0f, 0.0f, 0.0f, 1.0f);
    body.aLinVel = vec(0.0f, 0.0f, 0.0f);
    body.aAngVel = vec(0.0f, 0.0f, 0.0f);
    body.aInvInertiaLocal = zeroMatrix();
    body.aInvInertiaWorld = zeroMatrix();
    body.aFriction = 0.5f;
    body.aRestitution = 0.0f;
    body.aLinearDamping = 0.0f;
    body.aAngularDamping = 0.0f;
    body.aRadius = 0.0f;
    body.aHalfExtent = halfExtent;
    body.aSleeping = false;
    body.aSleepTime = 0.0f;
    body.aWorldAABB = sAABB();
    return body;
}

// 运动学体：速度由调用方给定，无限质量。
sRIGID_BODY makeKinematicSphere(const GNg::sVEC3 &pos, float radius) {
    sRIGID_BODY body = makeDynamicSphere(pos, radius, 1.0f);
    body.aType = eBODY_KINEMATIC;
    body.aInvMass = 0.0f;
    body.aInvInertiaLocal = zeroMatrix();
    body.aInvInertiaWorld = zeroMatrix();
    return body;
}

// 单点流形约束；仅填 aPoints[0]，其余接触槽清零累计冲量。
sCONTACT_CONSTRAINT makeConstraint(sRIGID_BODY *a, sRIGID_BODY *b, const GNg::sVEC3 &normal,
                                   const GNg::sVEC3 &point, float penetration) {
    sCONTACT_CONSTRAINT constraint;
    constraint.aBodyA = a;
    constraint.aBodyB = b;
    constraint.aManifold.aNormal = normal;
    constraint.aManifold.aPoints[0].aPoint = point;
    constraint.aManifold.aPoints[0].aPenetration = penetration;
    constraint.aManifold.aCount = 1;
    for(int i = 0; i < 4; ++i) {
        constraint.aAccNormal[i] = 0.0f;
        constraint.aAccTangent[i] = 0.0f;
    }
    return constraint;
}

bool vecEquals(const GNg::sVEC3 &a, const GNg::sVEC3 &b) {
    return a.x == b.x && a.y == b.y && a.z == b.z;
}

float normalRelativeSpeed(const sRIGID_BODY &a, const sRIGID_BODY &b, const GNg::sVEC3 &normal) {
    return vecDot(vecSub(b.aLinVel, a.aLinVel), normal);
}

} // namespace

// 两动态球正心对撞：法向相对速度不再闭合（>=0），总动量守恒。
TEST_CASE(testTwoDynamicSpheresHeadOn) {
    sRIGID_BODY a = makeDynamicSphere(vec(0.0f, 0.0f, 0.0f), 1.0f, 1.0f);
    sRIGID_BODY b = makeDynamicSphere(vec(2.0f, 0.0f, 0.0f), 1.0f, 1.0f);
    a.aLinVel = vec(1.0f, 0.0f, 0.0f);
    b.aLinVel = vec(-1.0f, 0.0f, 0.0f);
    const GNg::sVEC3 normal = vec(1.0f, 0.0f, 0.0f);
    sCONTACT_CONSTRAINT constraint = makeConstraint(&a, &b, normal, vec(1.0f, 0.0f, 0.0f), 0.0f);

    SolveVelocities(&constraint, 1, kDt, 10);

    CHECK(normalRelativeSpeed(a, b, normal) >= -1e-5f);
    CHECK_NEAR(normalRelativeSpeed(a, b, normal), 0.0f, 1e-3);
    CHECK_NEAR(a.aLinVel.x + b.aLinVel.x, 0.0f, 1e-4);
    CHECK_NEAR(a.aLinVel.x, 0.0f, 1e-4);
    CHECK_NEAR(b.aLinVel.x, 0.0f, 1e-4);
}

// e=0：接触后分离速度≈0。
TEST_CASE(testRestitutionZero) {
    sRIGID_BODY a = makeDynamicSphere(vec(0.0f, 0.0f, 0.0f), 1.0f, 1.0f);
    sRIGID_BODY b = makeDynamicSphere(vec(2.0f, 0.0f, 0.0f), 1.0f, 1.0f);
    a.aLinVel = vec(1.0f, 0.0f, 0.0f);
    b.aLinVel = vec(-1.0f, 0.0f, 0.0f);
    a.aRestitution = 0.0f;
    b.aRestitution = 0.0f;
    const GNg::sVEC3 normal = vec(1.0f, 0.0f, 0.0f);
    sCONTACT_CONSTRAINT constraint = makeConstraint(&a, &b, normal, vec(1.0f, 0.0f, 0.0f), 0.0f);

    SolveVelocities(&constraint, 1, kDt, 10);

    CHECK_NEAR(std::fabs(normalRelativeSpeed(a, b, normal)), 0.0f, 1e-3);
}

// e=1：相对法向速度反向且大小≈接近速度（两体取 max，两侧都设为 1）。
TEST_CASE(testRestitutionOne) {
    sRIGID_BODY a = makeDynamicSphere(vec(0.0f, 0.0f, 0.0f), 1.0f, 1.0f);
    sRIGID_BODY b = makeDynamicSphere(vec(2.0f, 0.0f, 0.0f), 1.0f, 1.0f);
    a.aLinVel = vec(1.0f, 0.0f, 0.0f);
    b.aLinVel = vec(-1.0f, 0.0f, 0.0f);
    a.aRestitution = 1.0f;
    b.aRestitution = 1.0f;
    const GNg::sVEC3 normal = vec(1.0f, 0.0f, 0.0f);
    sCONTACT_CONSTRAINT constraint = makeConstraint(&a, &b, normal, vec(1.0f, 0.0f, 0.0f), 0.0f);

    SolveVelocities(&constraint, 1, kDt, 10);

    // 接近速度 2 m/s，弹性碰撞后相对法向速度应为 +2 m/s。
    CHECK_NEAR(normalRelativeSpeed(a, b, normal), 2.0f, 1e-3);
    CHECK_NEAR(a.aLinVel.x, -1.0f, 1e-3);
    CHECK_NEAR(b.aLinVel.x, 1.0f, 1e-3);
}

// 静态盒：动态球被修正，静态体的速度/位置/朝向完全不变（速度求解与位置修正都不许动）。
TEST_CASE(testStaticBodyImmune) {
    sRIGID_BODY a = makeDynamicSphere(vec(0.0f, 0.0f, 0.0f), 1.0f, 1.0f);
    sRIGID_BODY b = makeStaticBox(vec(2.0f, 0.0f, 0.0f), vec(1.0f, 1.0f, 1.0f));
    a.aLinVel = vec(1.0f, 0.0f, 0.0f);
    const GNg::sVEC3 staticPositionBefore = b.aPos;
    const GNg::sQUAT staticRotationBefore = b.aRot;
    const GNg::sVEC3 normal = vec(1.0f, 0.0f, 0.0f);
    sCONTACT_CONSTRAINT constraint = makeConstraint(&a, &b, normal, vec(1.0f, 0.0f, 0.0f), 0.05f);

    SolveVelocities(&constraint, 1, kDt, 10);
    CHECK_NEAR(a.aLinVel.x, 0.0f, 1e-3);

    SolvePositions(&constraint, 1, 10);

    CHECK(vecEquals(b.aPos, staticPositionBefore));
    CHECK(b.aRot.x == staticRotationBefore.x && b.aRot.y == staticRotationBefore.y &&
          b.aRot.z == staticRotationBefore.z && b.aRot.w == staticRotationBefore.w);
    CHECK(vecEquals(b.aLinVel, vec(0.0f, 0.0f, 0.0f)));
    CHECK(vecEquals(b.aAngVel, vec(0.0f, 0.0f, 0.0f)));
    CHECK(a.aPos.x < 0.0f); // 动态球被位置修正沿 -n 推开（A 在 B 左侧）
}

// 运动学体：自身速度不被修改，且把速度推给动态体。
TEST_CASE(testKinematicPushesDynamic) {
    sRIGID_BODY a = makeDynamicSphere(vec(0.0f, 0.0f, 0.0f), 1.0f, 1.0f);
    sRIGID_BODY b = makeKinematicSphere(vec(2.0f, 0.0f, 0.0f), 1.0f);
    b.aLinVel = vec(-2.0f, 0.0f, 0.0f);
    const GNg::sVEC3 normal = vec(1.0f, 0.0f, 0.0f);
    sCONTACT_CONSTRAINT constraint = makeConstraint(&a, &b, normal, vec(1.0f, 0.0f, 0.0f), 0.0f);

    SolveVelocities(&constraint, 1, kDt, 10);

    CHECK(vecEquals(b.aLinVel, vec(-2.0f, 0.0f, 0.0f)));
    CHECK_NEAR(a.aLinVel.x, -2.0f, 1e-3);
    CHECK_NEAR(a.aLinVel.y, 0.0f, 1e-6);
    CHECK_NEAR(a.aLinVel.z, 0.0f, 1e-6);
}

// 库仑摩擦 mu=0.5：斜向速度的切向分量下降，累计切向冲量被 mu*jn 钳制。
TEST_CASE(testFrictionCone) {
    sRIGID_BODY a = makeDynamicSphere(vec(0.0f, 0.0f, 0.0f), 0.5f, 1.0f);
    sRIGID_BODY b = makeStaticBox(vec(0.0f, 0.5f, 0.0f), vec(0.5f, 0.5f, 0.5f));
    a.aLinVel = vec(1.0f, 1.0f, 0.0f);
    a.aFriction = 0.5f;
    b.aFriction = 0.5f;
    const GNg::sVEC3 normal = vec(0.0f, 1.0f, 0.0f);
    sCONTACT_CONSTRAINT constraint = makeConstraint(&a, &b, normal, vec(0.0f, 0.25f, 0.0f), 0.0f);

    SolveVelocities(&constraint, 1, kDt, 10);

    CHECK_NEAR(a.aLinVel.y, 0.0f, 1e-4); // 法向被消掉
    CHECK_NEAR(a.aLinVel.x, 0.5f, 1e-3); // 切向 1 -> 0.5（mu*jn = 0.5）
    CHECK(std::fabs(constraint.aAccTangent[0]) <= 0.5f * constraint.aAccNormal[0] + 1e-5f);
    CHECK_NEAR(constraint.aAccNormal[0], 1.0f, 1e-4);
    CHECK_NEAR(constraint.aAccTangent[0], -0.5f, 1e-4);
}

// 无摩擦 mu=0：法向仍被求解，切向速度完全不变。
TEST_CASE(testFrictionless) {
    sRIGID_BODY a = makeDynamicSphere(vec(0.0f, 0.0f, 0.0f), 0.5f, 1.0f);
    sRIGID_BODY b = makeStaticBox(vec(0.0f, 0.5f, 0.0f), vec(0.5f, 0.5f, 0.5f));
    a.aLinVel = vec(1.0f, 1.0f, 0.0f);
    a.aFriction = 0.0f;
    b.aFriction = 0.0f;
    const GNg::sVEC3 normal = vec(0.0f, 1.0f, 0.0f);
    sCONTACT_CONSTRAINT constraint = makeConstraint(&a, &b, normal, vec(0.0f, 0.25f, 0.0f), 0.0f);

    SolveVelocities(&constraint, 1, kDt, 10);

    CHECK_NEAR(a.aLinVel.y, 0.0f, 1e-4);
    CHECK_NEAR(a.aLinVel.x, 1.0f, 1e-6);
    CHECK_NEAR(constraint.aAccTangent[0], 0.0f, 1e-6);
}

// 偏心接触：接触点到质心的力臂不平行于法线 => 引入非零角速度。
TEST_CASE(testEccentricContactAngular) {
    sRIGID_BODY a = makeDynamicSphere(vec(0.0f, 0.0f, 0.0f), 0.5f, 1.0f);
    sRIGID_BODY b = makeStaticBox(vec(0.0f, 0.5f, 0.0f), vec(0.5f, 0.5f, 0.5f));
    a.aLinVel = vec(0.0f, 1.0f, 0.0f);
    const GNg::sVEC3 normal = vec(0.0f, 1.0f, 0.0f);
    sCONTACT_CONSTRAINT constraint = makeConstraint(&a, &b, normal, vec(0.4f, 0.5f, 0.0f), 0.0f);

    SolveVelocities(&constraint, 1, kDt, 10);

    CHECK(std::fabs(a.aAngVel.z) > 1e-4f);
    CHECK_NEAR(a.aAngVel.x, 0.0f, 1e-6);
    CHECK_NEAR(a.aAngVel.y, 0.0f, 1e-6);
}

// 正心接触：力臂平行于法线 => 角速度保持精确零。
TEST_CASE(testCenterContactNoAngular) {
    sRIGID_BODY a = makeDynamicSphere(vec(0.0f, 0.0f, 0.0f), 0.5f, 1.0f);
    sRIGID_BODY b = makeStaticBox(vec(1.0f, 0.0f, 0.0f), vec(0.5f, 0.5f, 0.5f));
    a.aLinVel = vec(1.0f, 0.0f, 0.0f);
    const GNg::sVEC3 normal = vec(1.0f, 0.0f, 0.0f);
    sCONTACT_CONSTRAINT constraint = makeConstraint(&a, &b, normal, vec(0.5f, 0.0f, 0.0f), 0.0f);

    SolveVelocities(&constraint, 1, kDt, 10);

    CHECK(a.aAngVel.x == 0.0f && a.aAngVel.y == 0.0f && a.aAngVel.z == 0.0f);
    CHECK_NEAR(a.aLinVel.x, 0.0f, 1e-3);
}

// 位置修正：穿透 0.05 的两球被推开、速度 bitwise 不变。
TEST_CASE(testPositionCorrection) {
    sRIGID_BODY a = makeDynamicSphere(vec(0.0f, 0.0f, 0.0f), 1.0f, 1.0f);
    sRIGID_BODY b = makeDynamicSphere(vec(1.95f, 0.0f, 0.0f), 1.0f, 1.0f);
    a.aLinVel = vec(0.5f, 0.0f, 0.0f);
    b.aLinVel = vec(-0.5f, 0.0f, 0.0f);
    const GNg::sVEC3 aVelocityBefore = a.aLinVel;
    const GNg::sVEC3 bVelocityBefore = b.aLinVel;
    const GNg::sVEC3 normal = vec(1.0f, 0.0f, 0.0f);
    sCONTACT_CONSTRAINT constraint =
        makeConstraint(&a, &b, normal, vec(0.975f, 0.0f, 0.0f), 0.05f);

    SolvePositions(&constraint, 1, 10);

    CHECK(b.aPos.x - a.aPos.x > 1.95f + 1e-3f);
    CHECK(std::memcmp(&a.aLinVel, &aVelocityBefore, sizeof(GNg::sVEC3)) == 0);
    CHECK(std::memcmp(&b.aLinVel, &bVelocityBefore, sizeof(GNg::sVEC3)) == 0);
    CHECK(a.aAngVel.x == 0.0f && a.aAngVel.y == 0.0f && a.aAngVel.z == 0.0f);
}

// 穿透小于 0.005 容差：位置不变。
TEST_CASE(testPositionBelowTolerance) {
    sRIGID_BODY a = makeDynamicSphere(vec(0.0f, 0.0f, 0.0f), 1.0f, 1.0f);
    sRIGID_BODY b = makeDynamicSphere(vec(1.997f, 0.0f, 0.0f), 1.0f, 1.0f);
    const GNg::sVEC3 aPositionBefore = a.aPos;
    const GNg::sVEC3 bPositionBefore = b.aPos;
    const GNg::sVEC3 normal = vec(1.0f, 0.0f, 0.0f);
    sCONTACT_CONSTRAINT constraint =
        makeConstraint(&a, &b, normal, vec(0.9985f, 0.0f, 0.0f), 0.003f);

    SolvePositions(&constraint, 1, 10);

    CHECK(vecEquals(a.aPos, aPositionBefore));
    CHECK(vecEquals(b.aPos, bPositionBefore));
}

// 确定性：同一输入两次求解（两份独立刚体）结果 bitwise 相等。
TEST_CASE(testDeterministicRepeat) {
    sRIGID_BODY a1 = makeDynamicSphere(vec(0.0f, 0.0f, 0.0f), 0.5f, 2.0f);
    sRIGID_BODY b1 = makeStaticBox(vec(0.0f, 0.5f, 0.0f), vec(0.5f, 0.5f, 0.5f));
    a1.aLinVel = vec(1.0f, 1.0f, 0.0f);
    a1.aAngVel = vec(0.0f, 0.0f, 0.5f);
    sCONTACT_CONSTRAINT c1 =
        makeConstraint(&a1, &b1, vec(0.0f, 1.0f, 0.0f), vec(0.4f, 0.5f, 0.0f), 0.02f);

    sRIGID_BODY a2 = a1;
    sRIGID_BODY b2 = b1;
    sCONTACT_CONSTRAINT c2 =
        makeConstraint(&a2, &b2, vec(0.0f, 1.0f, 0.0f), vec(0.4f, 0.5f, 0.0f), 0.02f);

    SolveVelocities(&c1, 1, kDt, 12);
    SolvePositions(&c1, 1, 6);
    SolveVelocities(&c2, 1, kDt, 12);
    SolvePositions(&c2, 1, 6);

    CHECK(std::memcmp(&a1.aLinVel, &a2.aLinVel, sizeof(GNg::sVEC3)) == 0);
    CHECK(std::memcmp(&a1.aAngVel, &a2.aAngVel, sizeof(GNg::sVEC3)) == 0);
    CHECK(std::memcmp(&a1.aPos, &a2.aPos, sizeof(GNg::sVEC3)) == 0);
    CHECK(a1.aRot.x == a2.aRot.x && a1.aRot.y == a2.aRot.y && a1.aRot.z == a2.aRot.z &&
          a1.aRot.w == a2.aRot.w);
    CHECK(c1.aAccNormal[0] == c2.aAccNormal[0] && c1.aAccTangent[0] == c2.aAccTangent[0]);
}

// 退化输入：空指针/零数量/静态-静态/零有效质量/iterations=0 都不得崩溃或写坏数据。
TEST_CASE(testDegenerateInputs) {
    SolveVelocities(0, 0, kDt, 10);
    SolvePositions(0, 0, 10);

    sRIGID_BODY staticA = makeStaticBox(vec(0.0f, 0.0f, 0.0f), vec(1.0f, 1.0f, 1.0f));
    sRIGID_BODY staticB = makeStaticBox(vec(1.0f, 0.0f, 0.0f), vec(1.0f, 1.0f, 1.0f));
    sCONTACT_CONSTRAINT staticPair = makeConstraint(
        &staticA, &staticB, vec(1.0f, 0.0f, 0.0f), vec(0.5f, 0.0f, 0.0f), 0.05f);
    SolveVelocities(&staticPair, 1, kDt, 10);
    SolvePositions(&staticPair, 1, 10);
    CHECK(vecEquals(staticA.aPos, vec(0.0f, 0.0f, 0.0f)));
    CHECK(vecEquals(staticB.aPos, vec(1.0f, 0.0f, 0.0f)));
    CHECK(vecEquals(staticA.aLinVel, vec(0.0f, 0.0f, 0.0f)));

    // 动态类型但 aInvMass == 0（退化）：视为无限质量，不得除零/产生 NaN。
    sRIGID_BODY zeroMass = makeDynamicSphere(vec(0.0f, 0.0f, 0.0f), 1.0f, 1.0f);
    zeroMass.aInvMass = 0.0f;
    zeroMass.aInvInertiaLocal = zeroMatrix();
    zeroMass.aInvInertiaWorld = zeroMatrix();
    sCONTACT_CONSTRAINT zeroConstraint = makeConstraint(
        &zeroMass, &staticB, vec(1.0f, 0.0f, 0.0f), vec(0.5f, 0.0f, 0.0f), 0.05f);
    SolveVelocities(&zeroConstraint, 1, kDt, 10);
    SolvePositions(&zeroConstraint, 1, 10);
    CHECK(vecEquals(zeroMass.aPos, vec(0.0f, 0.0f, 0.0f)));
    CHECK(zeroMass.aPos.x == zeroMass.aPos.x && zeroMass.aLinVel.x == zeroMass.aLinVel.x);

    // 空刚体指针、零数量、iterations = 0。
    sCONTACT_CONSTRAINT nullPair = makeConstraint(0, &staticB, vec(1.0f, 0.0f, 0.0f),
                                                  vec(0.5f, 0.0f, 0.0f), 0.05f);
    SolveVelocities(&nullPair, 1, kDt, 10);
    SolvePositions(&nullPair, 1, 10);
    sCONTACT_CONSTRAINT array[1] = {staticPair};
    SolveVelocities(array, 0, kDt, 10);
    SolvePositions(array, 0, 10);
    SolveVelocities(&array[0], 1, kDt, 0);
    SolvePositions(&array[0], 1, 0);
    CHECK(vecEquals(staticB.aPos, vec(1.0f, 0.0f, 0.0f)));
}
