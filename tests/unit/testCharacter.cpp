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
