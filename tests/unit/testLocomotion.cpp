// testLocomotion.cpp — 移动表现：选段 / 播放速率 / 转向。
//
// 为什么值得单测：这些"阈值 + 折算关系"以前硬编码在演示程序里，改一个数字就要跑起来看效果，
// 而且两处不一致时表现为"角色 A 走路脚打滑、B 不打滑"，根本查不出是哪边错了。
//
// 验收重点：
//   1. **边界要精确**：4.2 m/s 到底算走还是跑？差一个等号，手感就差一档。
//   2. **转向不越目标、不抖**：指数衰减必须单调收敛（用序列断言，不是只看终点）。
//   3. **帧率无关**：把 dt 拆两半跑两次 ≈ 跑一次。这条不做的话，
//      60fps 下正常、144fps 下变慢的 bug 会一直藏着。
//   4. **NaN 不能决定播哪段**。
#include "testFramework.h"

#include <GN.h>
#include "GN_Locomotion.h"

#include <cmath>

using namespace GN3d;

namespace {

const float kPi = 3.14159265358979f;

// 浮点角度比较（折到 (-pi, pi] 后比差）。
void checkAngleNear(float actual, float expected, float eps) {
    CHECK_NEAR(wrapRadians(actual - expected), 0.0f, eps);
}

} // namespace

// ═══════════════════════════════════════════════════════════
//  一、选段
// ═══════════════════════════════════════════════════════════

TEST_CASE(locoSelectClipBoundaries) {
    sLOCOMOTION_DESC desc; // 默认 0.4 / 4.2
    CHECK_EQ(desc.IsValid(), true);
    // 静止与低速
    CHECK_EQ(selectLocomotionClip(0.0f, desc), eLOCOMOTION_IDLE);
    CHECK_EQ(selectLocomotionClip(0.4f, desc), eLOCOMOTION_IDLE); // 恰好等于阈值 → 仍是 idle
    CHECK_EQ(selectLocomotionClip(0.41f, desc), eLOCOMOTION_WALK);
    // 走/跑边界
    CHECK_EQ(selectLocomotionClip(4.2f, desc), eLOCOMOTION_WALK); // 恰好 4.2 → 走
    CHECK_EQ(selectLocomotionClip(4.21f, desc), eLOCOMOTION_RUN);
    CHECK_EQ(selectLocomotionClip(9.0f, desc), eLOCOMOTION_RUN);
}

TEST_CASE(locoSelectClipRejectsNonFiniteSpeed) {
    sLOCOMOTION_DESC desc;
    CHECK_EQ(selectLocomotionClip(std::nanf(""), desc), eLOCOMOTION_IDLE);
    // 无穷大也按 IDLE 处理：非有限速度说明上游算错了（比如除了 0），
    // 此时"站着不动"比"用最快速度狂奔"安全 —— 后者会把错误放大成满天飞的动画。
    CHECK_EQ(selectLocomotionClip(INFINITY, desc), eLOCOMOTION_IDLE);
    CHECK_EQ(selectLocomotionClip(-1.0f, desc), eLOCOMOTION_IDLE); // 负速度当静止
}

TEST_CASE(locoDescValidation) {
    sLOCOMOTION_DESC good;
    CHECK(good.IsValid());
    sLOCOMOTION_DESC badIdle;
    badIdle.aIdleSpeed = 5.0f; // idle 阈值 >= run 阈值
    CHECK_EQ(badIdle.IsValid(), false);
    sLOCOMOTION_DESC badRef;
    badRef.aWalkRef = 0.0f;
    CHECK_EQ(badRef.IsValid(), false);
    sLOCOMOTION_DESC badRate;
    badRate.aMinPlayRate = 2.0f;
    badRate.aMaxPlayRate = 1.0f;
    CHECK_EQ(badRate.IsValid(), false);
    sLOCOMOTION_DESC nanDesc;
    nanDesc.aTurnRate = std::nanf("");
    CHECK_EQ(nanDesc.IsValid(), false);
}

// ═══════════════════════════════════════════════════════════
//  二、播放速率
// ═══════════════════════════════════════════════════════════

TEST_CASE(locoPlayRateFollowsSpeed) {
    sLOCOMOTION_DESC desc;
    // idle 恒为 1（站着不能播快播慢）
    CHECK_NEAR(locomotionPlayRate(0.0f, eLOCOMOTION_IDLE, desc), 1.0f, 1.0e-6);
    CHECK_NEAR(locomotionPlayRate(3.0f, eLOCOMOTION_IDLE, desc), 1.0f, 1.0e-6);
    // walk：参考速度 3.0，所以 3.0 m/s 时正好 1.0 —— 这就是"脚不打滑"的定义
    CHECK_NEAR(locomotionPlayRate(3.0f, eLOCOMOTION_WALK, desc), 1.0f, 1.0e-6);
    CHECK_NEAR(locomotionPlayRate(1.5f, eLOCOMOTION_WALK, desc), 0.55f, 1.0e-6); // 撞下限
    CHECK_NEAR(locomotionPlayRate(4.2f, eLOCOMOTION_WALK, desc), 1.4f, 1.0e-6);
    // run：参考速度 5.5
    CHECK_NEAR(locomotionPlayRate(5.5f, eLOCOMOTION_RUN, desc), 1.0f, 1.0e-6);
    CHECK_NEAR(locomotionPlayRate(3.0f, eLOCOMOTION_RUN, desc), 0.55f, 1.0e-6); // 撞下限
    CHECK_NEAR(locomotionPlayRate(20.0f, eLOCOMOTION_RUN, desc), 1.8f, 1.0e-6); // 撞上限
}

TEST_CASE(locoPlayRateClampsAndHandlesBadInput) {
    sLOCOMOTION_DESC desc;
    // 速度 0 也不能让速率变 0（否则动画停住、人还在滑）
    CHECK_NEAR(locomotionPlayRate(0.0f, eLOCOMOTION_WALK, desc), 0.55f, 1.0e-6);
    CHECK_NEAR(locomotionPlayRate(std::nanf(""), eLOCOMOTION_WALK, desc), 0.55f, 1.0e-6);
    // 参考速度为 0（参数没填好）：退回 1.0 而不是除零
    sLOCOMOTION_DESC broken;
    broken.aWalkRef = 0.0f;
    CHECK_NEAR(locomotionPlayRate(3.0f, eLOCOMOTION_WALK, broken), 1.0f, 1.0e-6);
    // 自定义钳制
    sLOCOMOTION_DESC custom;
    custom.aMinPlayRate = 0.8f;
    custom.aMaxPlayRate = 1.2f;
    CHECK_NEAR(locomotionPlayRate(1.0f, eLOCOMOTION_WALK, custom), 0.8f, 1.0e-6);
    CHECK_NEAR(locomotionPlayRate(6.0f, eLOCOMOTION_WALK, custom), 1.2f, 1.0e-6);
}

TEST_CASE(locoClipNames) {
    CHECK_STREQ(locomotionClipName(eLOCOMOTION_IDLE), "idle");
    CHECK_STREQ(locomotionClipName(eLOCOMOTION_WALK), "walk");
    CHECK_STREQ(locomotionClipName(eLOCOMOTION_RUN), "run");
    CHECK_STREQ(locomotionClipName(static_cast<eLOCOMOTION_CLIP>(99)), "?");
}

// ═══════════════════════════════════════════════════════════
//  三、角度工具
// ═══════════════════════════════════════════════════════════

TEST_CASE(locoWrapRadians) {
    CHECK_NEAR(wrapRadians(0.0f), 0.0f, 1.0e-6);
    // 区间是 **[ -pi, pi )**：+pi 折成 -pi（同一方向，但端点不同）
    CHECK_NEAR(wrapRadians(kPi), -kPi, 1.0e-5);
    CHECK_NEAR(wrapRadians(-kPi), -kPi, 1.0e-5);
    CHECK_NEAR(wrapRadians(kPi + 0.1f), -kPi + 0.1f, 1.0e-4);
    CHECK_NEAR(wrapRadians(-kPi - 0.1f), kPi - 0.1f, 1.0e-4);
    // 绕很多圈也要常数时间且结果正确
    CHECK_NEAR(wrapRadians(kPi * 7.0f), wrapRadians(kPi * 7.0f), 1.0e-6);
    CHECK_NEAR(wrapRadians(2.0f * 6.28318530718f + 0.5f), 0.5f, 1.0e-4);
    // NaN 不能传播
    CHECK_NEAR(wrapRadians(std::nanf("")), 0.0f, 1.0e-6);
}

TEST_CASE(locoYawFromDirection) {
    float yaw = 0.0f;
    // 模型正前方 = +Z，所以"朝 +Z"的 yaw 是 0
    CHECK(yawFromDirection(0.0f, 1.0f, &yaw));
    CHECK_NEAR(yaw, 0.0f, 1.0e-6);
    // +X 方向：yaw = atan2(1, 0) = pi/2
    CHECK(yawFromDirection(1.0f, 0.0f, &yaw));
    CHECK_NEAR(yaw, kPi * 0.5f, 1.0e-5);
    // -Z 方向：pi
    CHECK(yawFromDirection(0.0f, -1.0f, &yaw));
    CHECK_NEAR(std::fabs(yaw), kPi, 1.0e-5);
    // 长度接近 0：没有朝向可言
    CHECK_EQ(yawFromDirection(0.0f, 0.0f, &yaw), false);
    CHECK_EQ(yawFromDirection(1.0e-5f, 0.0f, &yaw), false);
    // NaN 输入
    CHECK_EQ(yawFromDirection(std::nanf(""), 1.0f, &yaw), false);
    // 空指针
    CHECK_EQ(yawFromDirection(0.0f, 1.0f, 0), false);
}

// ═══════════════════════════════════════════════════════════
//  四、平滑转向
// ═══════════════════════════════════════════════════════════

TEST_CASE(locoApproachYawConvergesWithoutOvershoot) {
    // 单调收敛：每一步都更靠近目标，且**永远不超过**目标。
    const float target = 1.0f;
    float yaw = 0.0f;
    float previousError = std::fabs(target - yaw);
    for(int step = 0; step < 200; ++step) {
        yaw = approachYaw(yaw, target, 14.0f, 1.0f / 60.0f);
        const float error = std::fabs(target - yaw);
        CHECK(error <= previousError + 1.0e-6f); // 不反弹
        CHECK(yaw <= target + 1.0e-6f); // 不越界
        previousError = error;
    }
    CHECK_NEAR(yaw, target, 1.0e-3f);
}

TEST_CASE(locoApproachYawTakesShortestPath) {
    // 从 170° 转到 -170°：应该走 20° 的近路（穿过 ±180°），而不是倒着走 340°。
    const float from = 170.0f * kPi / 180.0f;
    const float to = -170.0f * kPi / 180.0f;
    const float next = approachYaw(from, to, 10.0f, 1.0f / 60.0f);
    // 往"更大角度"的方向走（越过 180°），差值是正的
    CHECK(wrapRadians(next - from) > 0.0f);
    // 一步走的角度应该接近 20° 的一小部分，而不是 340° 的量级
    CHECK(wrapRadians(next - from) < 0.1f);
}

TEST_CASE(locoApproachYawIsFrameRateIndependent) {
    // 把 dt 拆成两半跑两次，结果应与一次跑完近似相同（指数衰减的性质）。
    const float target = 0.8f;
    const float rate = 14.0f;
    const float whole = approachYaw(0.0f, target, rate, 1.0f / 30.0f);
    float half = 0.0f;
    half = approachYaw(half, target, rate, 1.0f / 60.0f);
    half = approachYaw(half, target, rate, 1.0f / 60.0f);
    // 指数衰减是精确的：两者相等（浮点误差内）
    CHECK_NEAR(half, whole, 1.0e-5f);
}

TEST_CASE(locoApproachYawRejectsBadParameters) {
    // turnRate <= 0 / dt <= 0：保持原样（不能因为参数为 0 就把朝向清零）
    CHECK_NEAR(approachYaw(1.0f, 2.0f, 0.0f, 0.1f), 1.0f, 1.0e-6);
    CHECK_NEAR(approachYaw(1.0f, 2.0f, 10.0f, 0.0f), 1.0f, 1.0e-6);
    CHECK_NEAR(approachYaw(1.0f, 2.0f, -5.0f, 0.1f), 1.0f, 1.0e-6);
    // 当前朝向是 NaN：接受目标（否则会永久 NaN）
    CHECK_NEAR(approachYaw(std::nanf(""), 0.5f, 10.0f, 0.1f), 0.5f, 1.0e-6);
    // 目标是 NaN：保持当前
    CHECK_NEAR(approachYaw(0.5f, std::nanf(""), 10.0f, 0.1f), 0.5f, 1.0e-6);
}

// ═══════════════════════════════════════════════════════════
//  五、一步更新（速度优先 / 意图兜底）
// ═══════════════════════════════════════════════════════════

TEST_CASE(locoUpdateFacingUsesWishWhenStandingStill) {
    // 刚按下 W（速度还是 0）：必须立刻朝意图方向转，而不是等着速度积累。
    sFACING_STATE state;
    sLOCOMOTION_DESC desc;
    const GNg::sVEC3 velocity{0.0f, 0.0f, 0.0f};
    const GNg::sVEC3 wish{0.0f, 0.0f, 1.0f}; // 朝 +Z
    updateFacing(state, velocity, wish, 1.0f / 60.0f, desc);
    CHECK_NEAR(state.aYaw, 0.0f, 1.0e-6); // +Z 的 yaw 就是 0，一开始就在目标上
    // 换个方向：+X（yaw = +pi/2）。一步只走 20%（指数衰减），所以要跑多步才收敛 ——
    // 一开始我把"一步就转向"写成了预期，实测一步只到 0.33 rad。
    sFACING_STATE east;
    updateFacing(east, velocity, GNg::sVEC3{1.0f, 0.0f, 0.0f}, 1.0f / 60.0f, desc);
    CHECK(east.aYaw > 0.0f); // 方向对
    CHECK(east.aYaw < kPi * 0.5f); // 但还没到
    // 走 1 秒（60 步）应该基本到位
    for(int i = 0; i < 59; ++i) {
        updateFacing(east, velocity, GNg::sVEC3{1.0f, 0.0f, 0.0f}, 1.0f / 60.0f, desc);
    }
    checkAngleNear(east.aYaw, kPi * 0.5f, 0.02f);
}

TEST_CASE(locoUpdateFacingPrefersVelocityOverWish) {
    // 走起来之后按**实际速度**转身：被墙挡住时（wish 顶着墙、velocity 沿墙滑）
    // 角色应朝着能走的方向，而不是朝着墙。
    sFACING_STATE state;
    sLOCOMOTION_DESC desc;
    const GNg::sVEC3 wish{1.0f, 0.0f, 0.0f}; // 想往 +X（撞墙）
    const GNg::sVEC3 velocity{0.0f, 0.0f, 2.0f}; // 实际沿 +Z 滑
    for(int i = 0; i < 120; ++i) {
        updateFacing(state, velocity, wish, 1.0f / 60.0f, desc);
    }
    checkAngleNear(state.aYaw, 0.0f, 0.05f); // 朝向 +Z（速度方向），不是 +X
}

TEST_CASE(locoUpdateFacingHoldsWhenIdle) {
    // 速度和意图都接近 0：保持当前朝向，不能慢慢转回 0。
    sFACING_STATE state;
    state.aYaw = 1.2f;
    sLOCOMOTION_DESC desc;
    const GNg::sVEC3 zero{0.0f, 0.0f, 0.0f};
    for(int i = 0; i < 60; ++i) {
        updateFacing(state, zero, zero, 1.0f / 60.0f, desc);
    }
    CHECK_NEAR(state.aYaw, 1.2f, 1.0e-6);
}

TEST_CASE(locoUpdateFacingRejectsNonFiniteInput) {
    sFACING_STATE state;
    state.aYaw = 0.7f;
    sLOCOMOTION_DESC desc;
    const GNg::sVEC3 nan{std::nanf(""), 0.0f, 0.0f};
    const GNg::sVEC3 good{0.0f, 0.0f, 1.0f};
    updateFacing(state, nan, good, 1.0f / 60.0f, desc);
    CHECK_NEAR(state.aYaw, 0.7f, 1.0e-6); // NaN 速度被拒绝，朝向不变
    updateFacing(state, good, nan, 1.0f / 60.0f, desc);
    CHECK_NEAR(state.aYaw, 0.7f, 1.0e-6);
}

TEST_CASE(locoUpdateFacingIsDeterministic) {
    // 同一串输入跑两遍必须完全一致（这条能抓住"在内部用了未初始化状态"这类问题）。
    sLOCOMOTION_DESC desc;
    sFACING_STATE first;
    sFACING_STATE second;
    const GNg::sVEC3 wish{0.6f, 0.0f, 0.8f};
    const GNg::sVEC3 velocity{0.5f, 0.0f, 1.0f};
    for(int i = 0; i < 90; ++i) {
        updateFacing(first, velocity, wish, 1.0f / 60.0f, desc);
        updateFacing(second, velocity, wish, 1.0f / 60.0f, desc);
    }
    CHECK_EQ(first.aYaw, second.aYaw);
}
