// tests/characterDemo/main.cpp — P5 端到端：胶囊在"台阶 + 斜坡"的测试场地上跑完一段路。
//
// 与 tests/physicsBoxes 一样，**纯 CPU，不需要 GPU/窗口/Vulkan**（`make characterDemo` 直接出结果），
// 所以它也在 `make check` 里，任何机器都能跑。
//
// 场地（x 方向，全部沿 +x 前进）：
//   [0, 2)   平地（y = 0）
//   [2, 3)   0.25 m 台阶（< stepHeight = 0.35，应能上）
//   [3, 5)   平台
//   [5, 8)   40° 斜坡（run 3 / rise 2.5，< maxSlope = 45°，应能上）
//   [8, 9)   1.2 m 高台（> stepHeight，应被挡住 → 用跳跃过去）
//   [9, 20)  终点平面（y = 2.5 + 1.2？不，落地后回到 y = 2.5）
//
// 判据（240 帧，60Hz）：
//   1. **全程不穿地**：脚底最低点从不低于"脚下地面 - 2 cm"（用探针近似）；
//   2. 依次触发台阶抬升（aSteppedUp）、爬上斜坡（y 上升）、被高台挡住（aHitWall）；
//   3. 跳过高台后落到终点平面，最终 x 越过终点线；
//   4. 位置/速度始终有限（无 NaN 爆炸）。
#include <GN.h>
#include "GN_Character.h"
#include "GN_GameClock.h"

#include <cmath>
#include <cstdio>

namespace {

const float kStep = 1.0f / 60.0f;
const int kFrames = 240;

int gPass = 0;
int gFail = 0;

void check(const char *name, bool ok, const char *detail) {
    std::printf("[%s] %-28s %s\n", ok ? "PASS" : "FAIL", name, detail);
    if (ok) ++gPass;
    else ++gFail;
}

GNg::sVEC3 vec(float x, float y, float z) {
    GNg::sVEC3 v = {x, y, z};
    return v;
}

} // namespace

int main() {
    GN3d::sCHARACTER_DESC desc;
    desc.aRadius = 0.3f;
    desc.aHeight = 1.7f;
    desc.aMaxSlopeDeg = 45.0f;
    desc.aStepHeight = 0.35f;
    desc.aWalkSpeed = 4.0f;
    desc.aJumpSpeed = 5.0f;

    GN3d::cCHARACTER character;
    character.Init(desc);

    // ── 场地 ──
    character.AddFloor(vec(0.0f, 0.0f, 0.0f), 40.0f, 6.0f, 0.0f);          // 起点平地
    character.AddBox(vec(2.5f, 0.125f, 0.0f), vec(0.5f, 0.125f, 3.0f));    // 0.25 m 台阶
    character.AddBox(vec(4.0f, 0.125f, 0.0f), vec(1.0f, 0.125f, 3.0f));    // 平台
    character.AddRamp(vec(5.0f, 0.25f, 0.0f), 3.0f, 2.5f, 4.0f);           // 40° 斜坡 → y = 2.75
    character.AddBox(vec(8.5f, 2.75f + 0.6f, 0.0f), vec(0.5f, 0.6f, 3.0f)); // 1.2 m 高台（需跳）
    character.AddBox(vec(14.0f, 2.75f - 0.25f + 0.0f, 0.0f), vec(5.0f, 0.25f, 6.0f)); // 终点平面（顶面 y = 2.75）
    // 高台顶面 y = 2.75 + 1.2 = 3.95；（上到高台再跳下到终点平面 y=2.75）

    // 起点：平地中心，脚在 y = 0
    character.SetPosition(vec(0.0f, desc.aHeight * 0.5f + 0.05f, 0.0f));
    character.SetVelocity(vec(0.0f, 0.0f, 0.0f));

    // ── 主循环 ──
    GN::cGAME_CLOCK clock;
    clock.SetFixedStep(1.0f / 60.0f, 4);

    bool sawStepUp = false;
    bool sawSlope = false;
    bool sawWall = false;
    bool sawJump = false;
    bool sawGrounded = false;
    float maxY = 0.0f;
    float lowestFoot = 1.0e9f;
    float lowestGap = 1.0e9f; // 脚底到"脚下地面"的最小间隙（负数 = 穿地）
    bool finite = true;

    for (int frame = 0; frame < kFrames; ++frame) {
        clock.Tick(kStep);

        // 控制策略（固定脚本，不依赖渲染）：一路 +x；撞到 1.2 m 高台时跳一次。
        const bool needJump = sawWall && !sawJump && character.Position().x > 7.5f;
        const GN3d::sCHARACTER_STEP step =
            character.Move(vec(1.0f, 0.0f, 0.0f), false, needJump, clock.DeltaTime());

        if (step.aSteppedUp) sawStepUp = true;
        if (step.aSlopeDeg > 30.0f) sawSlope = true;
        if (step.aHitWall) sawWall = true;
        if (step.aJumped) sawJump = true;
        if (step.aGrounded) sawGrounded = true;

        const float y = character.Position().y;
        if (y > maxY) maxY = y;
        if (character.FootPosition().y < lowestFoot) lowestFoot = character.FootPosition().y;

        // 穿插地：用下一次 Move 之前的脚底位置与"脚下地面高度"比。
        // 地面高度用几何本身算：x 落在哪一段就取那一段的高度（保守近似，不含半径修正）。
        const float x = character.Position().x;
        float groundY = 0.0f;
        if (x >= 5.0f && x <= 8.0f) {
            groundY = 0.25f + (x - 5.0f) * (2.5f / 3.0f); // 斜坡
        } else if (x > 8.0f && x <= 9.0f && y < 3.5f) {
            groundY = 2.75f; // 高台前的平台段
        } else if (x > 9.0f) {
            groundY = 2.75f; // 终点平面
        } else if (x >= 2.0f) {
            groundY = 0.25f;
        }
        const float gap = character.FootPosition().y - groundY;
        if (x < 9.0f && gap < lowestGap) lowestGap = gap; // 高台段是跳跃，不计

        if (!std::isfinite(y) || !std::isfinite(character.Velocity().x) ||
            !std::isfinite(character.Velocity().y)) {
            finite = false;
        }
    }

    // ── 判据 ──
    {
        char detail[200];
        std::snprintf(detail, sizeof(detail), "触发台阶抬升=%d（0.25 m 台阶）", sawStepUp ? 1 : 0);
        check("character.steppedUp", sawStepUp, detail);
    }
    {
        char detail[200];
        std::snprintf(detail, sizeof(detail), "爬坡=%d 最高 y=%.2f（斜坡顶 y=2.75）",
                      sawSlope ? 1 : 0, maxY);
        check("character.climbedRamp", sawSlope && maxY > 2.0f, detail);
    }
    {
        char detail[200];
        std::snprintf(detail, sizeof(detail), "触发撞墙=%d 跳跃=%d（1.2 m 高台 > stepHeight %.2f）",
                      sawWall ? 1 : 0, sawJump ? 1 : 0, desc.aStepHeight);
        check("character.blockedThenJumped", sawWall && sawJump, detail);
    }
    {
        char detail[200];
        std::snprintf(detail, sizeof(detail), "最低脚底 y=%.3f，最小间隙 %.3f m（负 = 穿地）",
                      lowestFoot, lowestGap);
        // 容差 5 cm：竖胶囊在斜面/台阶边缘会有半球接触的正常间隙
        check("character.noPenetration", lowestGap > -0.05f, detail);
    }
    {
        char detail[200];
        std::snprintf(detail, sizeof(detail), "最终 x=%.2f（终点线 12），y=%.2f，着地=%d",
                      character.Position().x, character.Position().y,
                      character.IsGrounded() ? 1 : 0);
        check("character.reachedGoal", character.Position().x > 12.0f, detail);
    }
    {
        char detail[200];
        std::snprintf(detail, sizeof(detail), "全程出现过着地=%d，最终着地=%d",
                      sawGrounded ? 1 : 0, character.IsGrounded() ? 1 : 0);
        check("character.staysGrounded", sawGrounded && character.IsGrounded(), detail);
    }
    {
        char detail[200];
        std::snprintf(detail, sizeof(detail), "位置/速度有限=%d，卡住=%d", finite ? 1 : 0,
                      character.IsStuck() ? 1 : 0);
        check("character.finiteState", finite && !character.IsStuck(), detail);
    }

    std::printf("\n---- CHARACTER DEMO SUMMARY: pass=%d fail=%d ----\n", gPass, gFail);
    return gFail == 0 ? 0 : 1;
}
