// testGameClock.cpp — 游戏时钟（P0）纯 CPU 单测。
// 覆盖 docs/ENTITY_CONTROLLERS_PLAN.md §5.1 P0 的验收口径：
//   60Hz 物理 + 144Hz 渲染的累加器余量、maxSubSteps 限幅、timeScale=0 冻结、
//   Alpha() 单调且 ∈[0,1)、dt 为 0/负/NaN 不崩。
#include "testFramework.h"

#include <GN.h>
#include "GN_GameClock.h"

#include <cmath>

using namespace GN;

namespace {

const float STEP_60 = 1.0f / 60.0f;
const float STEP_144 = 1.0f / 144.0f;

bool alphaInRange(float a) { return a >= 0.0f && a < 1.0f; }

} // namespace

TEST_CASE(gameClockDefaults) {
    cGAME_CLOCK clock;
    CHECK_NEAR(clock.FixedStepSize(), STEP_60, 1e-7);
    CHECK_EQ(clock.MaxSubSteps(), 4u);
    CHECK_EQ(clock.FixedSteps(), 0u);
    CHECK_NEAR(clock.Alpha(), 0.0f, 1e-7);
    CHECK_NEAR(clock.DeltaTime(), 0.0f, 1e-7);
    CHECK_NEAR(clock.TimeScale(), 1.0f, 1e-7);
    CHECK(!clock.Paused());
    CHECK(alphaInRange(clock.Alpha()));
}

// 144Hz 渲染驱动 60Hz 物理：跑满 1 秒真实时间，应得约 60 步，且余量始终不足一步。
TEST_CASE(gameClockAccumulates60HzFrom144Hz) {
    cGAME_CLOCK clock;
    uint32_t steps = 0;
    for (int i = 0; i < 144; ++i) {
        clock.Tick(STEP_144);
        CHECK(alphaInRange(clock.Alpha()));
        steps += clock.FixedSteps();
    }
    CHECK(steps >= 59u);
    CHECK(steps <= 61u);
    CHECK_NEAR(clock.TotalTime(), 1.0f, 1e-4);
}

// 掉帧：一次 10 秒的 dt 也只能跑 maxSubSteps 步，剩下的整步丢弃（不追赶）。
TEST_CASE(gameClockMaxSubStepsClamp) {
    cGAME_CLOCK clock;
    clock.Tick(10.0f);
    CHECK_EQ(clock.FixedSteps(), 4u);
    CHECK(alphaInRange(clock.Alpha()));
    // 限幅后余量必须 < 一步，否则下一帧还会继续追赶
    CHECK(clock.Alpha() < 1.0f);
    clock.Tick(STEP_60);
    CHECK(clock.FixedSteps() <= 4u);
}

TEST_CASE(gameClockTimeScaleZeroFreezes) {
    cGAME_CLOCK clock;
    CHECK(clock.SetTimeScale(0.0f));
    clock.Tick(0.5f);
    CHECK_EQ(clock.FixedSteps(), 0u);
    CHECK_NEAR(clock.Alpha(), 0.0f, 1e-7);
    CHECK_NEAR(clock.DeltaTime(), 0.0f, 1e-7);
    CHECK_NEAR(clock.GameTime(), 0.0f, 1e-7);
    CHECK_NEAR(clock.TotalTime(), 0.5f, 1e-6); // 真实时间照走（UI 计时用）
    CHECK(clock.Paused());
}

TEST_CASE(gameClockTimeScaleHalf) {
    cGAME_CLOCK clock;
    CHECK(clock.SetTimeScale(0.5f));
    clock.Tick(0.1f);
    CHECK_NEAR(clock.DeltaTime(), 0.05f, 1e-6);
    CHECK_NEAR(clock.GameTime(), 0.05f, 1e-6);
    CHECK_NEAR(clock.TotalTime(), 0.1f, 1e-6);
}

// Alpha 在同一周期内单调不减；回落只可能发生在跑了子步之后（真实回绕）。
TEST_CASE(gameClockAlphaMonotonicWithinCycle) {
    cGAME_CLOCK clock;
    float prev = clock.Alpha();
    for (int i = 0; i < 500; ++i) {
        clock.Tick(STEP_144);
        const float now = clock.Alpha();
        CHECK(alphaInRange(now));
        if (now < prev) {
            CHECK(clock.FixedSteps() > 0u);
        }
        prev = now;
    }
}

TEST_CASE(gameClockIgnoresInvalidDt) {
    cGAME_CLOCK clock;
    const float nanValue = std::nanf("");
    const float infValue = HUGE_VALF;
    clock.Tick(-1.0f);
    clock.Tick(0.0f);
    clock.Tick(nanValue);
    clock.Tick(infValue);
    CHECK_EQ(clock.FixedSteps(), 0u);
    CHECK(std::isfinite(clock.TotalTime()));
    CHECK(std::isfinite(clock.Alpha()));
    CHECK(alphaInRange(clock.Alpha()));
}

TEST_CASE(gameClockRejectsInvalidFixedStep) {
    cGAME_CLOCK clock;
    CHECK(!clock.SetFixedStep(0.0f, 4));
    CHECK(!clock.SetFixedStep(-STEP_60, 4));
    CHECK(!clock.SetFixedStep(std::nanf(""), 4));
    CHECK(!clock.SetFixedStep(STEP_60, 0));
    CHECK_NEAR(clock.FixedStepSize(), STEP_60, 1e-7); // 状态不变
    CHECK_EQ(clock.MaxSubSteps(), 4u);

    CHECK(clock.SetFixedStep(1.0f / 120.0f, 8));
    CHECK_NEAR(clock.FixedStepSize(), 1.0f / 120.0f, 1e-7);
    CHECK_EQ(clock.MaxSubSteps(), 8u);
}

TEST_CASE(gameClockRejectsInvalidTimeScale) {
    cGAME_CLOCK clock;
    CHECK(!clock.SetTimeScale(-0.5f));
    CHECK(!clock.SetTimeScale(std::nanf("")));
    CHECK_NEAR(clock.TimeScale(), 1.0f, 1e-7);
    CHECK(!clock.Paused());
}

TEST_CASE(gameClockResetKeepsConfig) {
    cGAME_CLOCK clock;
    clock.SetFixedStep(1.0f / 120.0f, 2);
    clock.SetTimeScale(0.25f);
    clock.Tick(0.5f);
    clock.Reset();
    CHECK_EQ(clock.FixedSteps(), 0u);
    CHECK_NEAR(clock.Alpha(), 0.0f, 1e-7);
    CHECK_NEAR(clock.TotalTime(), 0.0f, 1e-7);
    CHECK_NEAR(clock.GameTime(), 0.0f, 1e-7);
    CHECK_NEAR(clock.FixedStepSize(), 1.0f / 120.0f, 1e-7);
    CHECK_EQ(clock.MaxSubSteps(), 2u);
    CHECK_NEAR(clock.TimeScale(), 0.25f, 1e-7);
}

// 中途把步长改小：旧余量不能变成"一次跑很多步"的隐患。
TEST_CASE(gameClockShrinkStepKeepsRemainderBelowOneStep) {
    cGAME_CLOCK clock;
    clock.Tick(0.02f); // 1/60 下 1 步 + 余 0.00333
    CHECK(clock.SetFixedStep(1.0f / 240.0f, 8));
    CHECK(alphaInRange(clock.Alpha()));
    clock.Tick(1.0f / 240.0f);
    CHECK(clock.FixedSteps() <= 8u);
    CHECK(alphaInRange(clock.Alpha()));
}
