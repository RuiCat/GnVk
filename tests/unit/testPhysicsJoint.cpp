// testPhysicsJoint.cpp — 关节约束：距离 / 铰链 / 固定、非法输入与确定性。
#include "testFramework.h"

#include <GN.h>
#include "GN_Physics.h"

#include <cmath>
#include <cstring>

using namespace GN3d;

namespace {

const float kDt = 1.0f / 60.0f;

sJOINT_DESC makeJointDesc(eJOINT_TYPE type, uint32_t bodyA, uint32_t bodyB) {
    sJOINT_DESC desc;
    desc.aType = type;
    desc.aBodyA = bodyA;
    desc.aBodyB = bodyB;
    desc.aLocalAnchorA = GNg::sVEC3{0.0f, 0.0f, 0.0f};
    desc.aLocalAnchorB = GNg::sVEC3{0.0f, 0.0f, 0.0f};
    desc.aLocalAxisA = GNg::sVEC3{0.0f, 1.0f, 0.0f};
    desc.aLocalAxisB = GNg::sVEC3{0.0f, 1.0f, 0.0f};
    desc.aDistance = -1.0f;
    return desc;
}

GNg::sVEC3 worldAnchorOf(const sRIGID_BODY &body, const GNg::sVEC3 &localAnchor) {
    GNg::sVEC3 rotated;
    GNg::QuatRotateVec3(&rotated, &localAnchor, &body.aRot);
    GNg::sVEC3 out;
    out.x = body.aPos.x + rotated.x;
    out.y = body.aPos.y + rotated.y;
    out.z = body.aPos.z + rotated.z;
    return out;
}

float vecLength(const GNg::sVEC3 &v) {
    return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
}

GNg::sVEC3 vecSub(const GNg::sVEC3 &a, const GNg::sVEC3 &b) {
    GNg::sVEC3 out;
    out.x = a.x - b.x;
    out.y = a.y - b.y;
    out.z = a.z - b.z;
    return out;
}

bool allFinite(const GNg::sVEC3 &v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

} // namespace

// 非法输入：无效句柄、同体、两端都不可动、无效类型都拒绝；RemoveBody 连带删除关节。
TEST_CASE(jointValidationAndBodyRemoval) {
    cPHYSICS_WORLD world;
    const uint32_t staticA = world.AddBody(
        MakeBoxBody(eBODY_STATIC, GNg::sVEC3{0.0f, 0.0f, 0.0f}, GNg::sVEC3{0.5f, 0.5f, 0.5f}, 0.0f));
    const uint32_t staticB = world.AddBody(
        MakeBoxBody(eBODY_STATIC, GNg::sVEC3{2.0f, 0.0f, 0.0f}, GNg::sVEC3{0.5f, 0.5f, 0.5f}, 0.0f));
    const uint32_t dynamicA = world.AddBody(
        MakeBoxBody(eBODY_DYNAMIC, GNg::sVEC3{1.0f, 0.0f, 0.0f}, GNg::sVEC3{0.5f, 0.5f, 0.5f}, 1.0f));
    const uint32_t dynamicB = world.AddBody(
        MakeBoxBody(eBODY_DYNAMIC, GNg::sVEC3{3.0f, 0.0f, 0.0f}, GNg::sVEC3{0.5f, 0.5f, 0.5f}, 1.0f));
    CHECK(staticA != cPHYSICS_WORLD::INVALID_HANDLE);
    CHECK(staticB != cPHYSICS_WORLD::INVALID_HANDLE);
    CHECK(dynamicA != cPHYSICS_WORLD::INVALID_HANDLE);
    CHECK(dynamicB != cPHYSICS_WORLD::INVALID_HANDLE);

    sJOINT_DESC desc = makeJointDesc(eJOINT_DISTANCE, dynamicA, dynamicB);
    desc.aDistance = 2.0f;
    const uint32_t joint = world.AddJoint(desc);
    CHECK(joint != cPHYSICS_WORLD::INVALID_HANDLE);
    CHECK_EQ(world.GetJointCount(), 1u);

    sJOINT_DESC invalidHandles = makeJointDesc(eJOINT_DISTANCE, 9999u, dynamicA);
    CHECK(world.AddJoint(invalidHandles) == cPHYSICS_WORLD::INVALID_HANDLE);
    sJOINT_DESC sameBody = makeJointDesc(eJOINT_DISTANCE, dynamicA, dynamicA);
    CHECK(world.AddJoint(sameBody) == cPHYSICS_WORLD::INVALID_HANDLE);
    sJOINT_DESC bothStatic = makeJointDesc(eJOINT_FIXED, staticA, staticB);
    CHECK(world.AddJoint(bothStatic) == cPHYSICS_WORLD::INVALID_HANDLE);
    sJOINT_DESC badType = makeJointDesc(eJOINT_DISTANCE, dynamicA, dynamicB);
    badType.aType = (eJOINT_TYPE)99;
    CHECK(world.AddJoint(badType) == cPHYSICS_WORLD::INVALID_HANDLE);
    CHECK(world.RemoveJoint(9999u) == false);
    CHECK_EQ(world.GetJointCount(), 1u);

    CHECK(world.RemoveBody(dynamicA));
    CHECK_EQ(world.GetJointCount(), 0u); // 引用该刚体的关节被连带删除
}

// 距离关节：两球被推开后仍保持静止长度（含角向有效质量的位置修正）。
TEST_CASE(jointDistanceHoldsLength) {
    cPHYSICS_WORLD world;
    world.SetGravity(GNg::sVEC3{0.0f, 0.0f, 0.0f});
    const uint32_t handleA =
        world.AddBody(MakeSphereBody(eBODY_DYNAMIC, GNg::sVEC3{0.0f, 0.0f, 0.0f}, 0.5f, 1.0f));
    const uint32_t handleB =
        world.AddBody(MakeSphereBody(eBODY_DYNAMIC, GNg::sVEC3{2.0f, 0.0f, 0.0f}, 0.5f, 1.0f));
    sJOINT_DESC desc = makeJointDesc(eJOINT_DISTANCE, handleA, handleB);
    desc.aDistance = 2.0f;
    const uint32_t joint = world.AddJoint(desc);
    CHECK(joint != cPHYSICS_WORLD::INVALID_HANDLE);

    world.GetBody(handleA)->aLinVel.x = -1.0f;
    world.GetBody(handleB)->aLinVel.x = 1.0f;
    world.WakeBody(handleA);
    world.WakeBody(handleB);
    for(int i = 0; i < 120; ++i) world.Step(kDt);

    const sRIGID_BODY *a = world.GetBody(handleA);
    const sRIGID_BODY *b = world.GetBody(handleB);
    CHECK(a != 0 && b != 0);
    if(!a || !b) return;
    CHECK(allFinite(a->aPos));
    CHECK(allFinite(b->aPos));
    CHECK_NEAR(vecLength(vecSub(b->aPos, a->aPos)), 2.0f, 0.05f);
    CHECK(std::fabs(a->aLinVel.x) < 0.05f); // 拉开速度被约束吸收
    CHECK(std::fabs(b->aLinVel.x) < 0.05f);
}

// 固定关节：给 B 线速度 + 角速度后，两体相对位置与相对朝向仍被锁定。
TEST_CASE(jointFixedKeepsRelativePose) {
    cPHYSICS_WORLD world;
    world.SetGravity(GNg::sVEC3{0.0f, 0.0f, 0.0f});
    const uint32_t handleA = world.AddBody(
        MakeBoxBody(eBODY_DYNAMIC, GNg::sVEC3{0.0f, 0.0f, 0.0f}, GNg::sVEC3{0.5f, 0.5f, 0.5f}, 1.0f));
    const uint32_t handleB = world.AddBody(
        MakeBoxBody(eBODY_DYNAMIC, GNg::sVEC3{1.0f, 0.0f, 0.0f}, GNg::sVEC3{0.5f, 0.5f, 0.5f}, 1.0f));
    sJOINT_DESC desc = makeJointDesc(eJOINT_FIXED, handleA, handleB);
    // 两锚点在初始位姿下都落在 (0,0,0)：B 的局部锚点 = (-1,0,0)（B 无旋转）。
    desc.aLocalAnchorB = GNg::sVEC3{-1.0f, 0.0f, 0.0f};
    const uint32_t joint = world.AddJoint(desc);
    CHECK(joint != cPHYSICS_WORLD::INVALID_HANDLE);

    world.GetBody(handleB)->aLinVel.x = 1.0f;
    world.GetBody(handleB)->aLinVel.y = 1.0f;
    world.GetBody(handleB)->aAngVel.x = 0.5f;
    world.GetBody(handleB)->aAngVel.y = 1.0f;
    world.GetBody(handleB)->aAngVel.z = 0.5f;
    world.WakeBody(handleB);
    for(int i = 0; i < 120; ++i) world.Step(kDt);

    const sRIGID_BODY *a = world.GetBody(handleA);
    const sRIGID_BODY *b = world.GetBody(handleB);
    CHECK(a != 0 && b != 0);
    if(!a || !b) return;
    // 自由刚体对会带着初始角动量整体旋转 ⇒ 相对位姿要在 A 的局部系里判（世界系里会跟着转）。
    GNg::sQUAT inverseA;
    GNg::QuatInverse(&inverseA, &a->aRot);
    const GNg::sVEC3 offset = vecSub(b->aPos, a->aPos);
    GNg::sVEC3 localOffset;
    GNg::QuatRotateVec3(&localOffset, &offset, &inverseA);
    CHECK_NEAR(localOffset.x, 1.0f, 0.05f);
    CHECK_NEAR(localOffset.y, 0.0f, 0.05f);
    CHECK_NEAR(localOffset.z, 0.0f, 0.05f);

    const GNg::sQUAT relativeRotation = inverseA * b->aRot;
    CHECK_NEAR(std::fabs(relativeRotation.w), 1.0f, 0.02f); // 相对朝向≈单位
}

// 铰链：绕局部轴旋转自由（角速度保留、实际转动），锚点与轴向仍被约束。
TEST_CASE(jointHingeAllowsAxialRotation) {
    cPHYSICS_WORLD world;
    world.SetGravity(GNg::sVEC3{0.0f, 0.0f, 0.0f});
    const uint32_t handleA = world.AddBody(
        MakeBoxBody(eBODY_STATIC, GNg::sVEC3{0.0f, 0.0f, 0.0f}, GNg::sVEC3{0.5f, 0.5f, 0.5f}, 0.0f));
    const uint32_t handleB = world.AddBody(
        MakeBoxBody(eBODY_DYNAMIC, GNg::sVEC3{0.0f, 1.0f, 0.0f}, GNg::sVEC3{0.5f, 0.5f, 0.5f}, 1.0f));
    sJOINT_DESC desc = makeJointDesc(eJOINT_HINGE, handleA, handleB);
    desc.aLocalAnchorA = GNg::sVEC3{0.0f, 1.0f, 0.0f}; // A 的世界锚点 = (0,1,0)
    desc.aLocalAnchorB = GNg::sVEC3{0.0f, 0.0f, 0.0f}; // B 的世界锚点 = (0,1,0)
    const uint32_t joint = world.AddJoint(desc);
    CHECK(joint != cPHYSICS_WORLD::INVALID_HANDLE);

    world.GetBody(handleB)->aAngVel.y = 2.0f;
    world.WakeBody(handleB);
    for(int i = 0; i < 120; ++i) world.Step(kDt);

    const sRIGID_BODY *a = world.GetBody(handleA);
    const sRIGID_BODY *b = world.GetBody(handleB);
    CHECK(a != 0 && b != 0);
    if(!a || !b) return;
    const GNg::sVEC3 anchorA = worldAnchorOf(*a, desc.aLocalAnchorA);
    const GNg::sVEC3 anchorB = worldAnchorOf(*b, desc.aLocalAnchorB);
    CHECK_NEAR(vecLength(vecSub(anchorA, anchorB)), 0.0f, 0.02f);

    GNg::sVEC3 axisA;
    GNg::sVEC3 axisB;
    GNg::QuatRotateVec3(&axisA, &desc.aLocalAxisA, &a->aRot);
    GNg::QuatRotateVec3(&axisB, &desc.aLocalAxisB, &b->aRot);
    const float alignment = axisA.x * axisB.x + axisA.y * axisB.y + axisA.z * axisB.z;
    CHECK(alignment > 0.99f);
    CHECK(std::fabs(b->aAngVel.y - 2.0f) < 0.5f); // 绕轴自由度未被消去
    CHECK(std::fabs(b->aRot.y) > 0.1f); // 确实绕 Y 转了（初始为单位四元数）
    CHECK(std::fabs(b->aRot.x) < 0.1f);
    CHECK(std::fabs(b->aRot.z) < 0.1f);
}

// 铰链：垂直于轴方向的角速度会被消去。
TEST_CASE(jointHingeResistsPerpendicularRotation) {
    cPHYSICS_WORLD world;
    world.SetGravity(GNg::sVEC3{0.0f, 0.0f, 0.0f});
    const uint32_t handleA = world.AddBody(
        MakeBoxBody(eBODY_STATIC, GNg::sVEC3{0.0f, 0.0f, 0.0f}, GNg::sVEC3{0.5f, 0.5f, 0.5f}, 0.0f));
    const uint32_t handleB = world.AddBody(
        MakeBoxBody(eBODY_DYNAMIC, GNg::sVEC3{0.0f, 1.0f, 0.0f}, GNg::sVEC3{0.5f, 0.5f, 0.5f}, 1.0f));
    sJOINT_DESC desc = makeJointDesc(eJOINT_HINGE, handleA, handleB);
    desc.aLocalAnchorA = GNg::sVEC3{0.0f, 1.0f, 0.0f};
    desc.aLocalAnchorB = GNg::sVEC3{0.0f, 0.0f, 0.0f};
    CHECK(world.AddJoint(desc) != cPHYSICS_WORLD::INVALID_HANDLE);

    world.GetBody(handleB)->aAngVel.x = 2.0f;
    world.WakeBody(handleB);
    for(int i = 0; i < 120; ++i) world.Step(kDt);

    const sRIGID_BODY *b = world.GetBody(handleB);
    CHECK(b != 0);
    if(!b) return;
    CHECK(std::fabs(b->aAngVel.x) < 0.2f); // 垂直轴的角速度被约束消去
    CHECK(std::fabs(b->aRot.x) < 0.2f);
}

// 确定性：同一初态两次构建/求解结果逐位相同。
TEST_CASE(jointDeterministicRepeat) {
    cPHYSICS_WORLD first;
    cPHYSICS_WORLD second;
    cPHYSICS_WORLD *worlds[2] = {&first, &second};
    uint32_t handles[2][2];
    for(int i = 0; i < 2; ++i) {
        worlds[i]->SetGravity(GNg::sVEC3{0.0f, -9.81f, 0.0f});
        handles[i][0] = worlds[i]->AddBody(MakeBoxBody(
            eBODY_DYNAMIC, GNg::sVEC3{0.0f, 1.0f, 0.0f}, GNg::sVEC3{0.3f, 0.3f, 0.3f}, 1.0f));
        handles[i][1] = worlds[i]->AddBody(MakeBoxBody(
            eBODY_DYNAMIC, GNg::sVEC3{1.0f, 1.0f, 0.0f}, GNg::sVEC3{0.3f, 0.3f, 0.3f}, 1.0f));
        worlds[i]->AddJoint(makeJointDesc(eJOINT_FIXED, handles[i][0], handles[i][1]));
        worlds[i]->GetBody(handles[i][1])->aLinVel.z = 0.5f;
        worlds[i]->WakeBody(handles[i][1]);
    }
    for(int step = 0; step < 60; ++step) {
        first.Step(kDt);
        second.Step(kDt);
    }
    for(int i = 0; i < 2; ++i) {
        const sRIGID_BODY *a = first.GetBody(handles[0][i]);
        const sRIGID_BODY *b = second.GetBody(handles[1][i]);
        CHECK(a != 0 && b != 0);
        if(!a || !b) continue;
        CHECK(std::memcmp(&a->aPos, &b->aPos, sizeof(GNg::sVEC3)) == 0);
        CHECK(std::memcmp(&a->aLinVel, &b->aLinVel, sizeof(GNg::sVEC3)) == 0);
        CHECK(a->aRot.x == b->aRot.x && a->aRot.y == b->aRot.y && a->aRot.z == b->aRot.z &&
              a->aRot.w == b->aRot.w);
    }
}
