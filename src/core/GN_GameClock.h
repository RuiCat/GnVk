#pragma once
#ifndef _GN_GAMECLOCK_H_
#define _GN_GAMECLOCK_H_
// GN_GameClock.h — 游戏时钟契约：固定步长累加器 + 渲染插值因子 + 时间缩放。
// 纯 CPU：不读输入、不碰渲染、不依赖平台；只负责把"每帧真实间隔"换算成
// "本帧该跑几次固定步"与"渲染插值因子"，让物理/AI/动画状态机拿到同一个 dt。
//
// 约定每帧三段（顺序不要换）：
//   FixedUpdate：跑 FixedSteps() 次，步长 FixedStepSize()（物理/AI/状态机）
//   Update     ：跑一次，用 DeltaTime()（输入采样/相机跟随）
//   Render     ：用 Alpha() 做快照插值
#include <cstdint>

namespace GN {

// 默认固定步长 1/60 s，最多 4 个子步（与 cPHYSICS_WORLD 内部同参）
const float GAME_CLOCK_DEFAULT_STEP = 1.0f / 60.0f;
const uint32_t GAME_CLOCK_DEFAULT_MAX_SUBSTEPS = 4;

class cGAME_CLOCK {
  public:
    cGAME_CLOCK();

    // 清累加器/计时/本帧结果；不动 step、maxSubSteps、timeScale
    void Reset();

    // 固定步长与子步上限。step 必须是有限正数、maxSubSteps 必须 > 0，
    // 否则返回 false 且状态不变。
    bool SetFixedStep(float step, uint32_t maxSubSteps = GAME_CLOCK_DEFAULT_MAX_SUBSTEPS);

    // 时间缩放：0 = 冻结；负值/NaN 非法，返回 false 且状态不变。
    bool SetTimeScale(float scale);

    // 每帧一次：传真实帧间隔（秒）。0/负/NaN/Inf 都按 0 处理，不崩。
    void Tick(float realDt);

    uint32_t FixedSteps() const; // 本帧该跑的 FixedUpdate 次数（已被 maxSubSteps 限幅）
    float FixedStepSize() const; // 固定步长（秒）
    float Alpha() const; // 渲染插值因子，恒在 [0,1)
    float DeltaTime() const; // 本帧参与模拟的时间 = realDt × timeScale（已清洗）
    float TotalTime() const; // 真实累计时间（不受 timeScale 影响，给 UI/统计用）
    float GameTime() const; // 受 timeScale 影响的累计游戏时间
    bool Paused() const; // timeScale <= 0
    float TimeScale() const;
    uint32_t MaxSubSteps() const;

  private:
    float aFixedStep; // 固定步长（秒）
    float aTimeScale; // 时间缩放
    uint32_t aMaxSubSteps; // 单帧子步上限
    float aAccumulator; // 不足一步的余量（秒）
    uint32_t aSteps; // 本帧 FixedSteps()
    float aAlpha; // 本帧插值因子
    float aDelta; // 本帧参与模拟的时间
    float aTotalTime; // 真实累计
    float aGameTime; // 缩放后累计
    bool aPaused; // 冻结标记
};

}; // end namespace GN
#endif //_GN_GAMECLOCK_H_
