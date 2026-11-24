// GN_GameClock.cpp — 固定步长时钟实现。
// 设计要点（每条都有对应用例，见 tests/unit/testGameClock.cpp）：
//   1. 累加器只保留"不足一步"的余量，整步交给 FixedSteps()；
//   2. 单帧最多跑 maxSubSteps 步，多出来的整步直接丢弃（掉帧后不做螺旋追赶）；
//   3. Alpha() 恒在 [0,1)，可直接拿去插值，不会因浮点误差越界；
//   4. 非法 dt（0/负/NaN/Inf）一律当 0，时间轴不倒退也不出现 NaN。

#include "GN_GameClock.h"

#include <cmath>

namespace GN {

cGAME_CLOCK::cGAME_CLOCK()
    : aFixedStep(GAME_CLOCK_DEFAULT_STEP), aTimeScale(1.0f),
      aMaxSubSteps(GAME_CLOCK_DEFAULT_MAX_SUBSTEPS), aAccumulator(0.0f), aSteps(0),
      aAlpha(0.0f), aDelta(0.0f), aTotalTime(0.0f), aGameTime(0.0f), aPaused(false) {}

void cGAME_CLOCK::Reset() {
    aAccumulator = 0.0f;
    aSteps = 0;
    aAlpha = 0.0f;
    aDelta = 0.0f;
    aTotalTime = 0.0f;
    aGameTime = 0.0f;
    aPaused = (aTimeScale <= 0.0f);
}

bool cGAME_CLOCK::SetFixedStep(float step, uint32_t maxSubSteps) {
    if (!(step > 0.0f) || !std::isfinite(step)) {
        return false;
    }
    if (maxSubSteps == 0) {
        return false;
    }
    aFixedStep = step;
    aMaxSubSteps = maxSubSteps;
    // 步长变小时旧余量可能已经超过一步，按新步长折算一次，避免下一帧一口气跑很多步。
    if (aAccumulator >= aFixedStep) {
        aAccumulator = std::fmod(aAccumulator, aFixedStep);
    }
    aAlpha = aAccumulator / aFixedStep;
    if (!(aAlpha >= 0.0f) || aAlpha >= 1.0f) {
        aAlpha = 0.0f;
    }
    return true;
}

bool cGAME_CLOCK::SetTimeScale(float scale) {
    if (!std::isfinite(scale) || scale < 0.0f) {
        return false;
    }
    aTimeScale = scale;
    aPaused = (scale <= 0.0f);
    return true;
}

void cGAME_CLOCK::Tick(float realDt) {
    if (!(realDt > 0.0f) || !std::isfinite(realDt)) {
        realDt = 0.0f;
    }
    aDelta = realDt * aTimeScale;
    if (!(aDelta > 0.0f) || !std::isfinite(aDelta)) {
        aDelta = 0.0f; // timeScale = 0 或乘法溢出
    }
    aTotalTime += realDt;
    aGameTime += aDelta;
    aAccumulator += aDelta;

    aSteps = 0;
    while (aAccumulator >= aFixedStep && aSteps < aMaxSubSteps) {
        aAccumulator -= aFixedStep;
        ++aSteps;
    }
    // 限幅：本帧的子步已经跑满，剩下的整步丢弃（只留不足一步的余量）。
    // 否则一次 10 s 的卡顿会让之后每帧都在追赶，越追越慢。
    if (aAccumulator >= aFixedStep) {
        aAccumulator = std::fmod(aAccumulator, aFixedStep);
    }
    aAlpha = aAccumulator / aFixedStep;
    if (!(aAlpha >= 0.0f) || aAlpha >= 1.0f) {
        aAlpha = 0.0f;
    }
    aPaused = (aTimeScale <= 0.0f);
}

uint32_t cGAME_CLOCK::FixedSteps() const { return aSteps; }

float cGAME_CLOCK::FixedStepSize() const { return aFixedStep; }

float cGAME_CLOCK::Alpha() const { return aAlpha; }

float cGAME_CLOCK::DeltaTime() const { return aDelta; }

float cGAME_CLOCK::TotalTime() const { return aTotalTime; }

float cGAME_CLOCK::GameTime() const { return aGameTime; }

bool cGAME_CLOCK::Paused() const { return aPaused; }

float cGAME_CLOCK::TimeScale() const { return aTimeScale; }

uint32_t cGAME_CLOCK::MaxSubSteps() const { return aMaxSubSteps; }

}; // end namespace GN
