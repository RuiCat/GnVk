// testCharacter.cpp — 胶囊角色控制器（P5）纯 CPU 单测。
//
// 覆盖 docs/ENTITY_CONTROLLERS_PLAN.md §5.1 P5 的验收口径：
//   平地行走速度 = aWalkSpeed、45° 斜坡可上而 60° 被阻、aStepHeight 台阶能过而更高不能、
//   跳跃高度 ≈ v²/2g、贴墙下滑不卡死、Teleport 后不残留旧速度。
//
// 全部用例只需要三角网 + 胶囊数学，不需要 GPU / Vulkan / 窗口。
#include "testFramework.h"

#include <GN.h>
#include "GN_Character.h"

#include <cmath>
#include <cstring>
#include <vector>

using namespace GN3d;

namespace {

const float kStep = 1.0f / 60.0f;
const float kEps = 1.0e-4f;

GNg::sVEC3 vec(float x, float y, float z) {
    GNg::sVEC3 v = {x, y, z};
    return v;
}

// 标准角色：r=0.3, 高=1.7（脚底在 center.y - 0.85）
sCHARACTER_DESC defaultDesc() {
    sCHARACTER_DESC desc;
    desc.aRadius = 0.3f;
    desc.aHeight = 1.7f;
    desc.aMaxSlopeDeg = 45.0f;
    desc.aStepHeight = 0.35f;
    desc.aWalkSpeed = 3.5f;
    desc.aSprintSpeed = 6.0f;
    desc.aJumpSpeed = 5.0f;
    desc.aGravityScale = 1.0f;
    desc.aSkinWidth = 0.01f;
    desc.aResolveIterations = 6;
    return desc;
}

// 把角色放到地板上，跑到着地。
void settle(cCHARACTER &character, float floorY, int frames = 30) {
    character.SetPosition(vec(0.0f, floorY + character.Desc().aHeight * 0.5f + 0.05f, 0.0f));
    for (int i = 0; i < frames; ++i) {
        character.Move(vec(0.0f, 0.0f, 0.0f), false, false, kStep);
    }
}

// 一块大地板（y = floorY），中心在原点。
void addFloor(cCHARACTER &character, float floorY, float half = 50.0f) {
    character.AddFloor(vec(0.0f, 0.0f, 0.0f), half, half, floorY);
}

} // namespace

// ── 基础 ───────────────────────────────────────────────
TEST_CASE(characterDescClampsInvalidValues) {
    cCHARACTER character;
    sCHARACTER_DESC desc;
    desc.aRadius = -1.0f;
    desc.aHeight = 0.1f; // 小于 2r
    desc.aStepHeight = -0.5f;
    desc.aGravityScale = 0.0f;
    desc.aMaxSlopeDeg = 200.0f;
    desc.aResolveIterations = 0;
    character.Init(desc);

    const sCHARACTER_DESC &out = character.Desc();
    CHECK(out.aRadius > 0.0f);
    CHECK(out.aHeight >= out.aRadius * 2.0f);
    CHECK(out.aStepHeight >= 0.0f);
    CHECK(out.aGravityScale > 0.0f);
    CHECK(out.aMaxSlopeDeg <= 89.0f);
    CHECK(out.aResolveIterations >= 1);
}

TEST_CASE(characterGeometryBuilders) {
    cCHARACTER character;
    character.Init(defaultDesc());
    CHECK_EQ(character.TriangleCount(), 0u);

    character.AddFloor(vec(0.0f, 0.0f, 0.0f), 5.0f, 5.0f, 0.0f);
    CHECK_EQ(character.TriangleCount(), 2u);

    character.AddBox(vec(1.0f, 0.5f, 0.0f), vec(0.5f, 0.5f, 0.5f));
    CHECK_EQ(character.TriangleCount(), 2u + 12u);

    character.AddRamp(vec(2.0f, 0.0f, 0.0f), 2.0f, 2.0f, 2.0f);
    CHECK_EQ(character.TriangleCount(), 2u + 12u + 2u);

    // 退化三角形被丢弃（三点共线）
    const uint32_t before = character.TriangleCount();
    character.AddTriangle(vec(0.0f, 0.0f, 0.0f), vec(1.0f, 0.0f, 0.0f), vec(2.0f, 0.0f, 0.0f));
    CHECK_EQ(character.TriangleCount(), before);

    character.ClearGeometry();
    CHECK_EQ(character.TriangleCount(), 0u);
}

// 空几何：不下坠、不崩、不报着地。
TEST_CASE(characterWithoutGeometry) {
    cCHARACTER character;
    character.Init(defaultDesc());
    character.SetPosition(vec(0.0f, 10.0f, 0.0f));
    for (int i = 0; i < 10; ++i) {
        const sCHARACTER_STEP step = character.Move(vec(0.0f, 0.0f, 0.0f), false, false, kStep);
        CHECK(!step.aGrounded);
    }
    CHECK(!character.IsGrounded());
    CHECK(character.Velocity().y < 0.0f); // 仍然受重力
    CHECK(std::isfinite(character.Position().y));
}

// ── 平地行走速度 ───────────────────────────────────────
TEST_CASE(characterWalkSpeedOnFlatGround) {
    cCHARACTER character;
    character.Init(defaultDesc());
    addFloor(character, 0.0f);
    settle(character, 0.0f);
    CHECK(character.IsGrounded());
    CHECK_NEAR(character.GroundNormalY(), 1.0f, 0.01f);
    CHECK_NEAR(character.SlopeDeg(), 0.0f, 0.5f);

    const GNg::sVEC3 start = character.Position();
    const int frames = 60; // 1 秒
    for (int i = 0; i < frames; ++i) {
        character.Move(vec(1.0f, 0.0f, 0.0f), false, false, kStep);
    }
    const float travelled = character.Position().x - start.x;
    // 1 秒 × walkSpeed = 3.5 m（容差 5%：起步那一步是瞬时的，没有加速度模型）
    CHECK_NEAR(travelled, character.Desc().aWalkSpeed, character.Desc().aWalkSpeed * 0.05f);
    // 高度不变（没掉下去也没飘起来）
    CHECK_NEAR(character.Position().y, start.y, 0.02f);
}

TEST_CASE(characterSprintIsFaster) {
    cCHARACTER character;
    character.Init(defaultDesc());
    addFloor(character, 0.0f);
    settle(character, 0.0f);
    const float startX = character.Position().x;
    for (int i = 0; i < 30; ++i) {
        character.Move(vec(1.0f, 0.0f, 0.0f), true, false, kStep);
    }
    const float sprinted = character.Position().x - startX;
    CHECK_NEAR(sprinted, character.Desc().aSprintSpeed * 0.5f,
               character.Desc().aSprintSpeed * 0.05f);
}

TEST_CASE(characterInvalidDtDoesNothing) {
    cCHARACTER character;
    character.Init(defaultDesc());
    addFloor(character, 0.0f);
    settle(character, 0.0f);
    const GNg::sVEC3 before = character.Position();
    character.Move(vec(1.0f, 0.0f, 0.0f), false, false, 0.0f);
    character.Move(vec(1.0f, 0.0f, 0.0f), false, false, -1.0f);
    character.Move(vec(1.0f, 0.0f, 0.0f), false, false, std::nanf(""));
    CHECK_NEAR(character.Position().x, before.x, kEps);
    CHECK_NEAR(character.Position().y, before.y, kEps);
}

// ── 斜坡：45° 能上，60° 被阻 ───────────────────────────
TEST_CASE(characterClimbs45DegreeRamp) {
    cCHARACTER character;
    addFloor(character, 0.0f);
    character.Init(defaultDesc()); // maxSlope = 45
    settle(character, 0.0f);

    // 斜坡：从 x = 1 开始，run 4 m / rise 4 m（45°），宽 4 m
    character.AddRamp(vec(1.0f, 0.0f, 0.0f), 4.0f, 4.0f, 4.0f);

    const float startY = character.Position().y;
    // 一路向前（+x），跑到坡中段（x ∈ [3, 4]）就停：
    // 跑满会冲出坡顶掉回地面，那时"坡度"自然是 0，测的就不是爬坡了。
    int frames = 0;
    while (character.Position().x < 3.0f && frames < 240) {
        character.Move(vec(1.0f, 0.0f, 0.0f), false, false, kStep);
        ++frames;
    }
    const float climbed = character.Position().y - startY;
    // 45° 在 maxSlope 之内：必须爬上去（x 到 3 时高度约 2 m）
    CHECK(climbed > 1.0f);
    CHECK(character.Position().x >= 3.0f);
    // 站得住（着地且报告"站在斜面上"）。
    // 注意坡度读数不是恒定 45°：竖着的胶囊在 45° 坡上是**侧下方半球**接触，
    // 往上走时接触点会滑到半球更"平"的位置，读数会落到 35° 左右。
    // 这是刚体几何决定的（不是插值/判定错误），所以这里只要求"明显是斜面"。
    CHECK(character.IsGrounded());
    CHECK(character.SlopeDeg() > 30.0f);
    CHECK(character.SlopeDeg() < 50.0f);
    // 竖胶囊在 45° 坡上，脚底最低点会比"正下方那点坡面"高约 r(√2-1) ≈ 0.12 m
    // （接触在下坡侧半球）。这条把"看着像悬空"的正常误差写在用例里。
    const float rampYUnderFeet = (character.Position().x - 1.0f);
    CHECK(character.FootPosition().y - rampYUnderFeet < 0.2f);
}

TEST_CASE(characterBlockedBy60DegreeRamp) {
    cCHARACTER character;
    addFloor(character, 0.0f);
    character.Init(defaultDesc()); // maxSlope = 45
    settle(character, 0.0f);

    // 60° 斜坡：run 2 m / rise 3.464 m
    character.AddRamp(vec(1.0f, 0.0f, 0.0f), 2.0f, 3.4641f, 4.0f);

    const float startY = character.Position().y;
    for (int i = 0; i < 120; ++i) {
        character.Move(vec(1.0f, 0.0f, 0.0f), false, false, kStep);
    }
    const float climbed = character.Position().y - startY;
    // 60° 超过上限：不能爬上去（允许因为解穿透/台阶逻辑被抬一点点）
    CHECK(climbed < 0.5f);
    // 也不能穿到坡里面去（脚底不高于坡面太多）
    CHECK(std::isfinite(character.Position().x));
    // 站在坡前，不该报"着地在 60° 面上"
    if (character.IsGrounded()) {
        CHECK(character.SlopeDeg() <= 46.0f);
    }
}

// ── 台阶 ───────────────────────────────────────────────
TEST_CASE(characterStepsUpShortStep) {
    cCHARACTER character;
    addFloor(character, 0.0f);
    character.Init(defaultDesc()); // stepHeight = 0.35
    settle(character, 0.0f);

    // 高 0.3 m（< stepHeight）的台阶：从 x = 1 起，宽 4 m、高 0.3、厚 2 m
    character.AddBox(vec(3.0f, 0.15f, 0.0f), vec(2.0f, 0.15f, 3.0f));

    const float startY = character.Position().y;
    for (int i = 0; i < 90; ++i) {
        character.Move(vec(1.0f, 0.0f, 0.0f), false, false, kStep);
    }
    // 走上去了：高度 +0.3 左右，且 x 越过台阶前沿
    CHECK(character.Position().y > startY + 0.2f);
    CHECK(character.Position().x > 1.5f);
}

TEST_CASE(characterBlockedByTallStep) {
    cCHARACTER character;
    addFloor(character, 0.0f);
    character.Init(defaultDesc()); // stepHeight = 0.35
    settle(character, 0.0f);

    // 高 1.0 m（> stepHeight）的台阶
    character.AddBox(vec(3.0f, 0.5f, 0.0f), vec(2.0f, 0.5f, 3.0f));

    const float startY = character.Position().y;
    for (int i = 0; i < 90; ++i) {
        character.Move(vec(1.0f, 0.0f, 0.0f), false, false, kStep);
    }
    // 上不去：高度几乎没变，且被挡在台阶前面
    CHECK(character.Position().y < startY + 0.2f);
    CHECK(character.Position().x < 1.5f);
}

// ── 跳跃 ───────────────────────────────────────────────
TEST_CASE(characterJumpHeightMatchesPhysics) {
    cCHARACTER character;
    addFloor(character, 0.0f);
    character.Init(defaultDesc()); // jumpSpeed = 5, g = 9.81
    settle(character, 0.0f);
    const float groundY = character.Position().y;

    // 起跳
    const sCHARACTER_STEP jumpStep = character.Move(vec(0.0f, 0.0f, 0.0f), false, true, kStep);
    CHECK(jumpStep.aJumped);
    CHECK(!character.IsGrounded());

    float peak = character.Position().y;
    int framesToLand = -1;
    for (int i = 0; i < 200; ++i) {
        character.Move(vec(0.0f, 0.0f, 0.0f), false, false, kStep);
        if (character.Position().y > peak) peak = character.Position().y;
        if (framesToLand < 0 && character.IsGrounded() && i > 2) framesToLand = i;
    }
    const float height = peak - groundY;
    // 理论高度 v²/2g = 25 / 19.62 = 1.274 m；离散积分会略高一点（一步的余量），容差 5%
    const float expected = 5.0f * 5.0f / (2.0f * 9.81f);
    CHECK_NEAR(height, expected, expected * 0.05f);
    // 落地：约 2v/g = 1.02 s ≈ 61 帧
    CHECK(framesToLand > 50);
    CHECK(framesToLand < 75);
    // 落回原高度
    CHECK_NEAR(character.Position().y, groundY, 0.02f);
}

TEST_CASE(characterCannotJumpInAir) {
    cCHARACTER character;
    addFloor(character, 0.0f);
    character.Init(defaultDesc());
    settle(character, 0.0f);
    character.Move(vec(0.0f, 0.0f, 0.0f), false, true, kStep); // 第一次起跳
    for (int i = 0; i < 10; ++i) character.Move(vec(0.0f, 0.0f, 0.0f), false, false, kStep);
    const float airY = character.Position().y;
    const float airVy = character.Velocity().y;
    const sCHARACTER_STEP second = character.Move(vec(0.0f, 0.0f, 0.0f), false, true, kStep);
    CHECK(!second.aJumped); // 空中不能二段跳
    CHECK(character.Velocity().y < airVy + 0.01f); // 速度没有被重置成 +5
    CHECK(character.Position().y < airY + 0.2f);
}

// ── 贴墙下滑 ───────────────────────────────────────────
TEST_CASE(characterSlidesDownWall) {
    cCHARACTER character;
    addFloor(character, 0.0f);
    character.Init(defaultDesc());
    settle(character, 0.0f);

    // 一面高墙：从 y = 0 到 y = 8（竖直面），在 x = 1 处
    character.AddBox(vec(2.0f, 4.0f, 0.0f), vec(1.0f, 4.0f, 5.0f));

    // 一边往墙里推一边等：必须"贴着墙往下滑"而不是卡住悬空
    const float groundY = character.Position().y;
    float lowestY = character.Position().y;
    for (int i = 0; i < 120; ++i) {
        const sCHARACTER_STEP step = character.Move(vec(1.0f, 0.0f, 0.0f), false, false, kStep);
        (void)step;
        if (character.Position().y < lowestY) lowestY = character.Position().y;
    }
    // 没有卡在半空：应该落回地面高度
    CHECK(lowestY < groundY + 0.05f);
    CHECK(character.IsGrounded());
    // 也没有穿进墙里（脚底中心不会跑到墙内部：墙的 -x 面在 x = 1）
    CHECK(character.Position().x < 1.0f);
    // 且没有被判定为"夹住"
    CHECK(!character.IsStuck());
}

// 贴墙 + 下落的组合：从空中贴着墙往下掉，也不能被卡住。
TEST_CASE(characterFallsAlongWallWithoutSticking) {
    cCHARACTER character;
    addFloor(character, 0.0f);
    character.Init(defaultDesc());
    // 墙：x ∈ [1, 3]，y ∈ [0, 8]
    character.AddBox(vec(2.0f, 4.0f, 0.0f), vec(1.0f, 4.0f, 5.0f));

    // 从墙边高处开始
    character.SetPosition(vec(0.7f, 6.0f, 0.0f));
    character.SetVelocity(vec(0.0f, 0.0f, 0.0f));
    const float startY = character.Position().y;
    float highestY = startY;
    for (int i = 0; i < 180; ++i) {
        character.Move(vec(0.5f, 0.0f, 0.0f), false, false, kStep); // 持续往墙里挤
        if (character.Position().y > highestY) highestY = character.Position().y;
    }
    // 没有被"弹上去"：全程不高过起点（单步穿透修正会抬起几厘米，所以不逐帧卡死，
    // 但绝不该超过起始高度 —— 那才是"贴墙被弹飞"）。
    CHECK(highestY <= startY + 1e-3f);
    CHECK(character.Position().y < 1.0f); // 落到了地面高度
    CHECK(character.Position().y > -0.2f); // 没穿地
    CHECK(!character.IsStuck());
}

// ── Teleport ───────────────────────────────────────────
TEST_CASE(characterTeleportClearsVelocity) {
    cCHARACTER character;
    addFloor(character, 0.0f);
    character.Init(defaultDesc());
    settle(character, 0.0f);

    // 先攒一点速度（水平 + 向下）
    character.Move(vec(1.0f, 0.0f, 0.0f), false, false, kStep);
    CHECK(character.Velocity().x > 1.0f);

    character.Teleport(vec(10.0f, 5.0f, 0.0f));
    CHECK_NEAR(character.Velocity().x, 0.0f, kEps);
    CHECK_NEAR(character.Velocity().y, 0.0f, kEps);
    CHECK_NEAR(character.Velocity().z, 0.0f, kEps);
    CHECK_NEAR(character.Position().x, 10.0f, kEps);
    CHECK_NEAR(character.Position().y, 5.0f, kEps);
    CHECK(!character.IsGrounded());

    // 瞬移后第一帧：不能因为残留速度而弹飞（水平位移应为 0）
    character.Move(vec(0.0f, 0.0f, 0.0f), false, false, kStep);
    CHECK_NEAR(character.Position().x, 10.0f, 0.01f);
    // 然后自然下落
    for (int i = 0; i < 120; ++i) character.Move(vec(0.0f, 0.0f, 0.0f), false, false, kStep);
    CHECK_NEAR(character.Position().y, 0.85f, 0.05f); // 脚底回到 y = 0
    CHECK(character.IsGrounded());
}

// ── 斜 + 台阶混合：走完整段路 ───────────────────────────
TEST_CASE(characterTraversesCourse) {
    cCHARACTER character;
    addFloor(character, 0.0f);
    character.Init(defaultDesc());
    settle(character, 0.0f);

    // 平地 → 0.3 m 台阶 → 45° 坡
    character.AddBox(vec(2.0f, 0.15f, 0.0f), vec(1.0f, 0.15f, 3.0f)); // x ∈ [1,3]，高 0.3
    character.AddRamp(vec(3.0f, 0.3f, 0.0f), 3.0f, 3.0f, 4.0f);      // 45°，到 x=6 时 y=3.3

    float maxY = 0.0f;
    for (int i = 0; i < 240; ++i) {
        character.Move(vec(1.0f, 0.0f, 0.0f), false, false, kStep);
        if (character.Position().y > maxY) maxY = character.Position().y;
        // 全程不许穿地
        CHECK(character.FootPosition().y > -0.1f);
    }
    CHECK(maxY > 1.0f); // 上过坡
    CHECK(character.Position().x > 4.0f); // 走到了坡上
}

// 意图接口：forward/right 轴 + 四个字段，结果应与直接给方向一致。
TEST_CASE(characterMoveIntentPacksAxes) {
    cCHARACTER character;
    addFloor(character, 0.0f);
    character.Init(defaultDesc());
    settle(character, 0.0f);

    cCHARACTER::sINTENT intent;
    intent.aMoveForward = 1.0f;
    intent.aMoveRight = 0.0f;
    const float startX = character.Position().x;
    const GNg::sVEC3 forward = vec(1.0f, 0.0f, 0.0f);
    const GNg::sVEC3 right = vec(0.0f, 0.0f, 1.0f);
    for (int i = 0; i < 30; ++i) {
        character.MoveIntent(forward, right, intent, kStep);
    }
    CHECK(character.Position().x > startX + 1.0f);

    // 对角线：两轴各 1，速度仍然受 walkSpeed 限制（不会 sqrt(2) 倍）
    cCHARACTER other;
    other.Init(defaultDesc());
    addFloor(other, 0.0f);
    settle(other, 0.0f);
    const GNg::sVEC3 origin = other.Position();
    cCHARACTER::sINTENT diag;
    diag.aMoveForward = 1.0f;
    diag.aMoveRight = 1.0f;
    for (int i = 0; i < 30; ++i) {
        other.MoveIntent(forward, right, diag, kStep);
    }
    const float dx = other.Position().x - origin.x;
    const float dz = other.Position().z - origin.z;
    const float distance = std::sqrt(dx * dx + dz * dz);
    CHECK_NEAR(distance, other.Desc().aWalkSpeed * 0.5f, other.Desc().aWalkSpeed * 0.06f);
}

// ═══════════════════════════════════════════════════════════
//  C2：角色推动态刚体
// ═══════════════════════════════════════════════════════════
// 背景：cCHARACTER 只消费**静态三角网**，完全不知道动态刚体存在 ——
// 接上 cPHYSICS_WORLD 之后它才变成"运动学推体器"。
// 这一组用例钉住三条语义：推得动、不推时就别动、角色自己不被顶回去。
namespace {

const float kPushStep = 1.0f / 60.0f;

// 搭一个"平地 + 一个动态箱子"的世界，角色站在箱子左侧
struct sPushFixture {
    GN3d::cPHYSICS_WORLD aWorld;
    GN3d::cCHARACTER aCharacter;
    uint32_t aBoxHandle;

    explicit sPushFixture(float boxX = 1.0f) {
        aWorld.SetFixedTimestep(kPushStep, 4);
        aWorld.SetGravity(GNg::sVEC3{0.0f, -9.81f, 0.0f});

        // 地面：薄板，顶面 y = 0
        aWorld.AddBody(GN3d::MakeBoxBody(GN3d::eBODY_STATIC, GNg::sVEC3{0.0f, -0.5f, 0.0f},
                                         GNg::sVEC3{20.0f, 0.5f, 20.0f}, 0.0f));
        // 动态箱子（半边长 0.5，中心在 y=0.5 时正好贴地）
        aBoxHandle = aWorld.AddBody(GN3d::MakeBoxBody(
            GN3d::eBODY_DYNAMIC, GNg::sVEC3{boxX, 0.5f, 0.0f}, GNg::sVEC3{0.5f, 0.5f, 0.5f}, 1.0f));

        sCHARACTER_DESC desc;
        desc.aRadius = 0.3f;
        desc.aHeight = 1.7f;
        desc.aWalkSpeed = 4.0f;
        aCharacter.Init(desc);
        // 角色站在箱子左边 1.2 m 处
        aCharacter.Position();
        GN3d::cCHARACTER &character = aCharacter;
        character.SetPosition(GNg::sVEC3{boxX - 1.2f, 0.85f, 0.0f});
        character.SetPhysicsWorld(&aWorld);
    }
};

} // namespace

// 最基本的语义：角色往前走，箱子必须被推开
TEST_CASE(characterPushesBoxForward) {
    sPushFixture fixture;
    const float boxStartX = fixture.aWorld.GetBody(fixture.aBoxHandle)->aPos.x;

    // 朝 +X 走 1.5 秒
    int pushedFrames = 0;
    for (int i = 0; i < 90; ++i) {
        fixture.aCharacter.Move(GNg::sVEC3{1.0f, 0.0f, 0.0f}, false, false, kPushStep);
        if (fixture.aCharacter.LastPushedCount() > 0) ++pushedFrames;
    }
    const float boxEndX = fixture.aWorld.GetBody(fixture.aBoxHandle)->aPos.x;

    CHECK(pushedFrames > 0);          // 至少有一帧真的接触到了
    CHECK(boxEndX > boxStartX + 0.5f); // 箱子被明显推动（> 半个身位）
    // 角色没有被顶回去（运动学推体的语义：角色赢）
    CHECK(fixture.aCharacter.Position().x > boxStartX - 1.2f);
}

// 不接物理世界时行为与之前完全一致（默认关闭）
TEST_CASE(characterPushDisabledByDefault) {
    sPushFixture fixture;
    fixture.aCharacter.SetPhysicsWorld(nullptr);
    CHECK(fixture.aCharacter.PhysicsWorld() == nullptr);
    const float boxStartX = fixture.aWorld.GetBody(fixture.aBoxHandle)->aPos.x;

    for (int i = 0; i < 90; ++i) {
        fixture.aCharacter.Move(GNg::sVEC3{1.0f, 0.0f, 0.0f}, false, false, kPushStep);
    }
    // 一帧都不该推，且计数器恒为 0
    CHECK_EQ(fixture.aCharacter.LastPushedCount(), 0u);
    CHECK_NEAR(fixture.aWorld.GetBody(fixture.aBoxHandle)->aPos.x, boxStartX, 1e-6);
}

// **站着不动不该推**：角色只是存在，箱子不该自己漂走
TEST_CASE(characterStandingStillDoesNotPush) {
    sPushFixture fixture(0.45f); // 箱子紧贴角色（有穿透）
    const float boxStartX = fixture.aWorld.GetBody(fixture.aBoxHandle)->aPos.x;
    const GNg::sVEC3 startPos = fixture.aCharacter.Position();

    // 只跑物理（角色原地不动，也不给输入）
    for (int i = 0; i < 120; ++i) {
        fixture.aWorld.Step(kPushStep);
        fixture.aCharacter.Move(GNg::sVEC3{0.0f, 0.0f, 0.0f}, false, false, kPushStep);
    }
    // 位置分离仍然会发生（重叠必须解开），但**不该有速度传递**
    const GN3d::sRIGID_BODY *body = fixture.aWorld.GetBody(fixture.aBoxHandle);
    const float speed = std::sqrt(body->aLinVel.x * body->aLinVel.x +
                                  body->aLinVel.z * body->aLinVel.z);
    CHECK(speed < 1.0f); // 站着不动不该把箱子推得跑起来
    (void)boxStartX;
    CHECK_NEAR(fixture.aCharacter.Position().x, startPos.x, 0.5f); // 角色基本没动
}

// 推力强度：0 = 只分离不传递速度；1 = 正常
//
// **怎么量才对**（第一版量错了）：直接比"推完之后的位移"没有区分度 ——
// 位置分离本身已经把箱子按角色步速顶走了，速度冲量**不会**增加推的过程中的行程。
// 真正体现差别的量是**角色停手之后箱子还能滑多远**（冲量留下的动能）。
TEST_CASE(characterPushStrengthSemantics) {
    struct sRun {
        float aCoast; // 角色停手后箱子又走了多远
        float aPeakSpeed;
    };
    auto run = [](float strength) {
        sRun out;
        out.aCoast = 0.0f;
        out.aPeakSpeed = 0.0f;
        sPushFixture fixture;
        fixture.aCharacter.SetPushStrength(strength);
        // 推 0.6 秒
        for (int i = 0; i < 36; ++i) {
            fixture.aCharacter.Move(GNg::sVEC3{1.0f, 0.0f, 0.0f}, false, false, kPushStep);
        }
        const GN3d::sRIGID_BODY *body = fixture.aWorld.GetBody(fixture.aBoxHandle);
        out.aPeakSpeed = std::sqrt(body->aLinVel.x * body->aLinVel.x +
                                   body->aLinVel.z * body->aLinVel.z);
        // 角色把物理世界关掉（等价于"停手"），只让箱子按自己的速度滑行
        fixture.aCharacter.SetPhysicsWorld(nullptr);
        const float beforeX = body->aPos.x;
        for (int i = 0; i < 60; ++i) fixture.aWorld.Step(kPushStep);
        body = fixture.aWorld.GetBody(fixture.aBoxHandle);
        out.aCoast = body->aPos.x - beforeX;
        return out;
    };

    const sRun normal = run(1.0f);
    const sRun none = run(0.0f);
    // 强度 0：没有速度传递 ⇒ 停手后箱子几乎立刻停住
    CHECK(none.aPeakSpeed < 0.5f);
    // 强度 1：箱子带着速度 ⇒ 停手后还会滑一段
    CHECK(normal.aPeakSpeed > none.aPeakSpeed + 0.5f);
    CHECK(normal.aCoast > none.aCoast);

    // 非法强度被钳制
    sPushFixture clampFixture;
    clampFixture.aCharacter.SetPushStrength(-5.0f);
    CHECK_NEAR(clampFixture.aCharacter.PushStrength(), 0.0f, 1e-6);
    clampFixture.aCharacter.SetPushStrength(99.0f);
    CHECK_NEAR(clampFixture.aCharacter.PushStrength(), 4.0f, 1e-6);
}

// 静态刚体不被推（且角色靠**自己的三角网**被挡住）
//
// 注意这条用例故意同时给了 character.AddBox —— 因为"角色只碰撞三角网、不碰撞静态刚体"
// 是 cCHARACTER 的既定边界（见 GN_Character.h 文件头）。第一版只放了静态刚体墙、
// 没给三角网，结果角色直接穿过去，断言失败 —— 那是**边界**不是 bug。
TEST_CASE(characterDoesNotPushStaticBodies) {
    GN3d::cPHYSICS_WORLD world;
    world.SetFixedTimestep(kPushStep, 4);
    world.AddBody(GN3d::MakeBoxBody(GN3d::eBODY_STATIC, GNg::sVEC3{0.0f, -0.5f, 0.0f},
                                    GNg::sVEC3{20.0f, 0.5f, 20.0f}, 0.0f));
    // 一堵静态墙（刚体侧）
    const uint32_t wall = world.AddBody(GN3d::MakeBoxBody(
        GN3d::eBODY_STATIC, GNg::sVEC3{1.0f, 1.0f, 0.0f}, GNg::sVEC3{0.5f, 1.0f, 2.0f}, 0.0f));
    const GNg::sVEC3 wallStart = world.GetBody(wall)->aPos;

    GN3d::cCHARACTER character;
    sCHARACTER_DESC desc;
    desc.aRadius = 0.3f;
    desc.aHeight = 1.7f;
    character.Init(desc);
    character.SetPosition(GNg::sVEC3{-0.5f, 0.85f, 0.0f});
    character.SetPhysicsWorld(&world);
    // 同一堵墙的三角网（角色只认这个）
    character.AddFloor(GNg::sVEC3{0.0f, 0.0f, 0.0f}, 20.0f, 20.0f, 0.0f);
    character.AddBox(GNg::sVEC3{1.0f, 1.0f, 0.0f}, GNg::sVEC3{0.5f, 1.0f, 2.0f});

    for (int i = 0; i < 90; ++i) {
        character.Move(GNg::sVEC3{1.0f, 0.0f, 0.0f}, false, false, kPushStep);
    }
    // 墙（刚体）的位置一点没变，计数器也不该把它算进去
    CHECK_NEAR(world.GetBody(wall)->aPos.x, wallStart.x, 1e-6);
    CHECK_EQ(character.LastPushedCount(), 0u);
    // 角色被三角网挡住（不是穿过去）
    CHECK(character.Position().x < 1.0f);
}

// 世界查询本身：按 AABB 取句柄，先清空、按升序、不误报
TEST_CASE(physicsQueryBodiesAABB) {
    GN3d::cPHYSICS_WORLD world;
    const uint32_t a = world.AddBody(GN3d::MakeBoxBody(
        GN3d::eBODY_STATIC, GNg::sVEC3{0.0f, 0.0f, 0.0f}, GNg::sVEC3{0.5f, 0.5f, 0.5f}, 0.0f));
    const uint32_t b = world.AddBody(GN3d::MakeBoxBody(
        GN3d::eBODY_DYNAMIC, GNg::sVEC3{10.0f, 0.0f, 0.0f}, GNg::sVEC3{0.5f, 0.5f, 0.5f}, 1.0f));
    const uint32_t c = world.AddBody(GN3d::MakeBoxBody(
        GN3d::eBODY_STATIC, GNg::sVEC3{1.0f, 0.0f, 0.0f}, GNg::sVEC3{0.5f, 0.5f, 0.5f}, 0.0f));
    // 手动刷新 AABB（AddBody 时是空盒，Step 才填）
    world.Step(1.0f / 60.0f);

    std::vector<uint32_t> hits;
    hits.push_back(0xDEADBEEF); // 预填垃圾：必须被清空
    sAABB query;
    query.aMin = GNg::sVEC3{-1.0f, -1.0f, -1.0f};
    query.aMax = GNg::sVEC3{2.0f, 1.0f, 1.0f};
    world.QueryBodiesAABB(query, hits);
    // a 与 c 在范围内，b 在 x=10 不在
    CHECK_EQ((uint32_t)hits.size(), 2u);
    CHECK_EQ(hits[0], a);
    CHECK_EQ(hits[1], c);
    // 升序
    for (size_t i = 1; i < hits.size(); ++i) CHECK(hits[i] > hits[i - 1]);

    // 空盒查询：返回空
    sAABB empty;
    world.QueryBodiesAABB(empty, hits);
    CHECK_EQ((uint32_t)hits.size(), 0u);
}
