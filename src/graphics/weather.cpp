// 天气状态机实现：预设表、限幅、平滑过渡、渲染参数派生。纯 CPU，无 Vulkan 依赖。
#include "weather.h"

#include <cmath>

namespace GN3d {

namespace {

const float WEATHER_MAX_TRANSITION = 60.0f;
const float WEATHER_MAX_WIND = 50.0f;

// 晴天的基准色；阴天/雨雪按 overcast 比例向灰色混合。
const GNg::sVEC3 CLEAR_HORIZON = {0.68f, 0.78f, 0.92f};
const GNg::sVEC3 CLEAR_ZENITH = {0.16f, 0.32f, 0.62f};
const GNg::sVEC3 OVERCAST_HORIZON = {0.55f, 0.57f, 0.60f};
const GNg::sVEC3 OVERCAST_ZENITH = {0.38f, 0.41f, 0.46f};
const GNg::sVEC3 FOG_GRAY = {0.62f, 0.64f, 0.67f};
// 默认太阳方向（指向太阳）：右上前方，与常见美术布光一致。
const GNg::sVEC3 SUN_DIRECTION = {0.45f, 0.78f, 0.44f};

GNg::sVEC3 makeVec3(float x, float y, float z) {
    GNg::sVEC3 v = {x, y, z};
    return v;
}

GNg::sVEC3 lerpVec(const GNg::sVEC3 &from, const GNg::sVEC3 &to, float t) {
    return makeVec3(from.x + (to.x - from.x) * t, from.y + (to.y - from.y) * t,
                    from.z + (to.z - from.z) * t);
}

GNg::sVEC3 scaleVec(const GNg::sVEC3 &v, float s) {
    return makeVec3(v.x * s, v.y * s, v.z * s);
}

float lerpFloat(float from, float to, float t) { return from + (to - from) * t; }

float clamp01(float value) {
    if (!(value >= 0.0f)) { // 同时挡住 NaN
        return 0.0f;
    }
    return value > 1.0f ? 1.0f : value;
}

float clampWind(float value) {
    if (!std::isfinite(value)) {
        return 0.0f;
    }
    if (value < -WEATHER_MAX_WIND) {
        return -WEATHER_MAX_WIND;
    }
    return value > WEATHER_MAX_WIND ? WEATHER_MAX_WIND : value;
}

// smoothstep：过渡两端速度为 0，避免天气切换时突变。
float smoothStep(float t) {
    if (t <= 0.0f) {
        return 0.0f;
    }
    if (t >= 1.0f) {
        return 1.0f;
    }
    return t * t * (3.0f - 2.0f * t);
}

WeatherState makeState(float rain, float snow, float fog, float cloud, float sharp, float windX,
                       float windZ, float sun) {
    WeatherState state;
    state.rain = rain;
    state.snow = snow;
    state.fog = fog;
    state.cloudCoverage = cloud;
    state.cloudSharpness = sharp;
    state.windX = windX;
    state.windZ = windZ;
    state.sunIntensity = sun;
    return state;
}

} // namespace

WeatherState weatherPresetState(WeatherPreset preset) {
    switch (preset) {
    case WeatherPreset::CLOUDY:
        return makeState(0.0f, 0.0f, 0.20f, 0.75f, 0.40f, 2.0f, 0.5f, 0.55f);
    case WeatherPreset::RAIN:
        return makeState(0.80f, 0.0f, 0.35f, 0.90f, 0.35f, 6.0f, 1.0f, 0.30f);
    case WeatherPreset::SNOW:
        return makeState(0.0f, 0.80f, 0.30f, 0.85f, 0.50f, 2.0f, -0.5f, 0.40f);
    case WeatherPreset::FOG:
        return makeState(0.0f, 0.0f, 0.90f, 0.50f, 0.60f, 0.0f, 0.0f, 0.50f);
    case WeatherPreset::STORM:
        return makeState(1.0f, 0.0f, 0.55f, 1.00f, 0.30f, 14.0f, 2.0f, 0.15f);
    case WeatherPreset::CLEAR:
    default:
        return makeState(0.0f, 0.0f, 0.05f, 0.15f, 0.60f, 0.5f, 0.0f, 1.0f);
    }
}

WeatherSystem::WeatherSystem()
    : aCurrent(weatherPresetState(WeatherPreset::CLEAR)),
      aTarget(aCurrent),
      aStart(aCurrent),
      aDuration(0.0f),
      aElapsed(0.0f),
      aTime(0.0f) {}

WeatherState WeatherSystem::clampState(const WeatherState &state) {
    WeatherState clamped;
    clamped.rain = clamp01(state.rain);
    clamped.snow = clamp01(state.snow);
    clamped.fog = clamp01(state.fog);
    clamped.cloudCoverage = clamp01(state.cloudCoverage);
    clamped.cloudSharpness = clamp01(state.cloudSharpness);
    clamped.windX = clampWind(state.windX);
    clamped.windZ = clampWind(state.windZ);
    clamped.sunIntensity = clamp01(state.sunIntensity);
    return clamped;
}

void WeatherSystem::startTransition(const WeatherState &target, float transitionSeconds) {
    aStart = aCurrent;
    aTarget = clampState(target);
    if (!std::isfinite(transitionSeconds) || transitionSeconds <= 0.0f) {
        aDuration = 0.0f;
    } else {
        aDuration = transitionSeconds > WEATHER_MAX_TRANSITION ? WEATHER_MAX_TRANSITION : transitionSeconds;
    }
    aElapsed = 0.0f;
    if (aDuration == 0.0f) {
        aCurrent = aTarget;
    }
}

void WeatherSystem::setPreset(WeatherPreset preset, float transitionSeconds) {
    startTransition(weatherPresetState(preset), transitionSeconds);
}

void WeatherSystem::setState(const WeatherState &state, float transitionSeconds) {
    startTransition(state, transitionSeconds);
}

void WeatherSystem::reset(WeatherPreset preset) {
    aCurrent = clampState(weatherPresetState(preset));
    aTarget = aCurrent;
    aStart = aCurrent;
    aDuration = 0.0f;
    aElapsed = 0.0f;
    aTime = 0.0f;
}

void WeatherSystem::update(float dt) {
    if (!(dt > 0.0f) || !std::isfinite(dt)) {
        return;
    }
    aTime += dt;
    if (aDuration <= 0.0f) {
        return;
    }
    aElapsed += dt;
    if (aElapsed >= aDuration) {
        aElapsed = aDuration;
        aCurrent = aTarget;
        return;
    }
    const float t = smoothStep(aElapsed / aDuration);
    aCurrent.rain = lerpFloat(aStart.rain, aTarget.rain, t);
    aCurrent.snow = lerpFloat(aStart.snow, aTarget.snow, t);
    aCurrent.fog = lerpFloat(aStart.fog, aTarget.fog, t);
    aCurrent.cloudCoverage = lerpFloat(aStart.cloudCoverage, aTarget.cloudCoverage, t);
    aCurrent.cloudSharpness = lerpFloat(aStart.cloudSharpness, aTarget.cloudSharpness, t);
    aCurrent.windX = lerpFloat(aStart.windX, aTarget.windX, t);
    aCurrent.windZ = lerpFloat(aStart.windZ, aTarget.windZ, t);
    aCurrent.sunIntensity = lerpFloat(aStart.sunIntensity, aTarget.sunIntensity, t);
}

WeatherOutput WeatherSystem::output() const {
    WeatherOutput out;
    const float overcast = clamp01(aCurrent.cloudCoverage * 0.7f +
                                   (aCurrent.rain > aCurrent.snow ? aCurrent.rain : aCurrent.snow) * 0.6f);
    const float wet = aCurrent.rain * 0.25f + aCurrent.snow * 0.15f;

    out.sky.sunDirection = SUN_DIRECTION;
    out.sky.sunIntensity = aCurrent.sunIntensity;
    out.sky.cloudCoverage = aCurrent.cloudCoverage;
    out.sky.cloudSharpness = aCurrent.cloudSharpness;
    out.sky.timeSeconds = aTime;
    out.sky.horizonColor = lerpVec(CLEAR_HORIZON, OVERCAST_HORIZON, overcast * 0.85f);
    out.sky.zenithColor = lerpVec(CLEAR_ZENITH, OVERCAST_ZENITH, overcast * 0.85f);
    out.sky.horizonColor = scaleVec(out.sky.horizonColor, 1.0f - wet);
    out.sky.zenithColor = scaleVec(out.sky.zenithColor, 1.0f - wet * 0.8f);

    out.fog.color = lerpVec(out.sky.horizonColor, FOG_GRAY, aCurrent.fog * 0.6f);
    out.fog.density = 0.0005f + aCurrent.fog * 0.0295f;
    out.fog.heightFalloff = 0.15f;
    out.fog.baseHeight = 0.0f;

    out.precipitation.rain = aCurrent.rain;
    out.precipitation.snow = aCurrent.snow;
    out.precipitation.wind = makeVec3(aCurrent.windX, 0.0f, aCurrent.windZ);
    out.precipitation.rainRate = 2600.0f * aCurrent.rain;
    out.precipitation.snowRate = 900.0f * aCurrent.snow;
    out.precipitation.rainMaxParticles = static_cast<uint32_t>(7000.0f * aCurrent.rain + 0.5f);
    out.precipitation.snowMaxParticles = static_cast<uint32_t>(4500.0f * aCurrent.snow + 0.5f);

    out.ambientScale = clamp01(1.0f - 0.35f * aCurrent.cloudCoverage -
                               0.25f * (aCurrent.rain > aCurrent.snow ? aCurrent.rain : aCurrent.snow) -
                               0.20f * aCurrent.fog);
    if (out.ambientScale < 0.15f) {
        out.ambientScale = 0.15f;
    }
    return out;
}

} // namespace GN3d
