#pragma once
#ifndef GN_WEATHER_H
#define GN_WEATHER_H
// ╔══════════════════════════════════════════════════════════╗
// ║  GN_mini — 天气状态机（纯 CPU，可单测）                  ║
// ╚══════════════════════════════════════════════════════════╝
//
// 职责：把「天气预设」变成可平滑过渡的标量状态，并派生渲染层消费的参数
// （天空、雾、降水、环境光）。本层**不**碰 Vulkan、不分配 GPU 资源、不依赖时间源；
// 调用方每帧传入 dt（钳制后）即可。所有输入都做限幅，输出只依赖当前状态与累计时间，
// 同参数 + 同 dt 序列结果逐位可重复。
#include <GN.h> // 先定义 GN_API / 模块宏，GN_3Dmath.h 依赖它
#include "GN_3Dmath.h"

#include <cstdint>

namespace GN3d {

enum class WeatherPreset { CLEAR = 0, CLOUDY, RAIN, SNOW, FOG, STORM };

// 状态标量：强度均为 0..1，风为世界水平速度（m/s）。
struct WeatherState {
    float rain = 0.0f;
    float snow = 0.0f;
    float fog = 0.0f;
    float cloudCoverage = 0.15f;
    float cloudSharpness = 0.6f;
    float windX = 0.0f;
    float windZ = 0.0f;
    float sunIntensity = 1.0f;
};

// 天空着色器参数（方向均为单位向量，方向指向太阳）。
struct SkyParams {
    GNg::sVEC3 sunDirection;
    GNg::sVEC3 horizonColor;
    GNg::sVEC3 zenithColor;
    float sunIntensity = 1.0f;
    float cloudCoverage = 0.0f;
    float cloudSharpness = 0.5f;
    float timeSeconds = 0.0f;
};

// 雾参数：距离 × 高度指数雾（具体公式见 docs/SKY_WEATHER_PLAN.md）。
struct FogParams {
    GNg::sVEC3 color;
    float density = 0.0f; // 基准密度（1/m）
    float heightFalloff = 0.15f; // 高度衰减（1/m）
    float baseHeight = 0.0f; // 基准高度（m）
};

// 降水参数：强度 0..1，由渲染层映射成粒子数/发射率。
struct PrecipitationParams {
    float rain = 0.0f;
    float snow = 0.0f;
    GNg::sVEC3 wind;
    float rainRate = 0.0f; // 粒子/秒
    float snowRate = 0.0f;
    uint32_t rainMaxParticles = 0;
    uint32_t snowMaxParticles = 0;
};

struct WeatherOutput {
    SkyParams sky;
    FogParams fog;
    PrecipitationParams precipitation;
    float ambientScale = 1.0f; // 环境光/太阳强度的整体缩放
};

// 预设对应的一次性目标状态（供 UI/测试复用）。
WeatherState weatherPresetState(WeatherPreset preset);

// 天气状态机：current 向 target 平滑过渡；过渡中途再次设置目标时从当前值重新起算。
class WeatherSystem {
  public:
    WeatherSystem();

    // 设置目标；transitionSeconds <= 0 立即生效，否则钳到 [0, 60] 秒。
    void setPreset(WeatherPreset preset, float transitionSeconds = 3.0f);
    void setState(const WeatherState &state, float transitionSeconds = 3.0f);

    // 立即切到预设（不经过渡），并清零累计时间。
    void reset(WeatherPreset preset);

    // 推进过渡与时间；dt 非有限值/<=0 忽略，大 dt 直接推进（跨过终点即完成过渡）。
    void update(float dt);

    bool isTransitioning() const { return aElapsed < aDuration; }
    const WeatherState &state() const { return aCurrent; }
    const WeatherState &target() const { return aTarget; }
    float timeSeconds() const { return aTime; }
    WeatherOutput output() const;

  private:
    void startTransition(const WeatherState &target, float transitionSeconds);
    static WeatherState clampState(const WeatherState &state);

    WeatherState aCurrent;
    WeatherState aTarget;
    WeatherState aStart; // 本次过渡的起点
    float aDuration;
    float aElapsed;
    float aTime;
};

} // namespace GN3d
#endif // GN_WEATHER_H
