// testWeather.cpp — 天气状态机：预设表、平滑过渡、限幅、派生参数、确定性与时间累计。
#include "testFramework.h"

#include "weather.h"

#include <cmath>
#include <limits>

using namespace GN3d;

TEST_CASE(weatherDefaultIsClear) {
    WeatherSystem weather;
    const WeatherState &state = weather.state();
    CHECK_NEAR(state.rain, 0.0f, 1e-6);
    CHECK_NEAR(state.snow, 0.0f, 1e-6);
    CHECK(!weather.isTransitioning());

    const WeatherOutput out = weather.output();
    CHECK(out.sky.cloudCoverage < 0.5f);
    CHECK(out.sky.sunIntensity > 0.5f);
    CHECK(out.precipitation.rainMaxParticles == 0u);
    CHECK(out.ambientScale > 0.5f);
}

TEST_CASE(weatherPresetTable) {
    const WeatherState rain = weatherPresetState(WeatherPreset::RAIN);
    CHECK_NEAR(rain.rain, 0.8f, 1e-6);
    CHECK(rain.fog > 0.2f);
    CHECK(rain.cloudCoverage > 0.8f);

    const WeatherState snow = weatherPresetState(WeatherPreset::SNOW);
    CHECK_NEAR(snow.rain, 0.0f, 1e-6);
    CHECK_NEAR(snow.snow, 0.8f, 1e-6);

    const WeatherState storm = weatherPresetState(WeatherPreset::STORM);
    CHECK_NEAR(storm.rain, 1.0f, 1e-6);
    CHECK(storm.windX > 10.0f);
    CHECK(storm.sunIntensity < 0.2f);
}

TEST_CASE(weatherTransitionIsSmoothAndCompletes) {
    WeatherSystem weather;
    weather.setPreset(WeatherPreset::RAIN, 3.0f);
    CHECK(weather.isTransitioning());

    weather.update(1.5f);
    CHECK(weather.isTransitioning());
    CHECK(weather.state().rain > 0.0f);
    CHECK(weather.state().rain < 0.8f);
    CHECK_NEAR(weather.state().rain, 0.4f, 1e-4); // smoothstep(0.5) = 0.5

    weather.update(1.5f);
    CHECK(!weather.isTransitioning());
    CHECK_NEAR(weather.state().rain, 0.8f, 1e-6);
    CHECK_NEAR(weather.state().fog, 0.35f, 1e-6);
}

TEST_CASE(weatherImmediateAndReset) {
    WeatherSystem weather;
    weather.setPreset(WeatherPreset::SNOW, 0.0f);
    CHECK(!weather.isTransitioning());
    CHECK_NEAR(weather.state().snow, 0.8f, 1e-6);

    weather.setPreset(WeatherPreset::STORM, 5.0f);
    weather.update(0.5f);
    weather.reset(WeatherPreset::FOG);
    CHECK(!weather.isTransitioning());
    CHECK_NEAR(weather.state().fog, 0.9f, 1e-6);
    CHECK_NEAR(weather.timeSeconds(), 0.0f, 1e-6);
}

TEST_CASE(weatherStateIsClamped) {
    WeatherState state;
    state.rain = 5.0f;
    state.snow = -2.0f;
    state.fog = 3.0f;
    state.cloudCoverage = -1.0f;
    state.cloudSharpness = 2.0f;
    state.windX = 1000.0f;
    state.windZ = -1000.0f;
    state.sunIntensity = 4.0f;

    WeatherSystem weather;
    weather.setState(state, 0.0f);
    CHECK_NEAR(weather.state().rain, 1.0f, 1e-6);
    CHECK_NEAR(weather.state().snow, 0.0f, 1e-6);
    CHECK_NEAR(weather.state().fog, 1.0f, 1e-6);
    CHECK_NEAR(weather.state().cloudCoverage, 0.0f, 1e-6);
    CHECK_NEAR(weather.state().cloudSharpness, 1.0f, 1e-6);
    CHECK_NEAR(weather.state().windX, 50.0f, 1e-6);
    CHECK_NEAR(weather.state().windZ, -50.0f, 1e-6);
    CHECK_NEAR(weather.state().sunIntensity, 1.0f, 1e-6);
}

TEST_CASE(weatherUpdateIgnoresInvalidDtAndAcceptsLargeDt) {
    WeatherSystem weather;
    weather.setPreset(WeatherPreset::RAIN, 3.0f);

    weather.update(std::numeric_limits<float>::quiet_NaN());
    weather.update(-1.0f);
    CHECK_NEAR(weather.timeSeconds(), 0.0f, 1e-6);

    weather.update(10.0f); // 跨过 3s 终点 → 直接完成
    CHECK(!weather.isTransitioning());
    CHECK_NEAR(weather.state().rain, 0.8f, 1e-6);
    CHECK_NEAR(weather.timeSeconds(), 10.0f, 1e-6);
}

TEST_CASE(weatherDerivedParamsFollowState) {
    WeatherSystem clear;
    clear.reset(WeatherPreset::CLEAR);

    WeatherSystem fog;
    fog.reset(WeatherPreset::FOG);
    const WeatherOutput fogOut = fog.output();
    CHECK(fogOut.fog.density > clear.output().fog.density);
    CHECK(fogOut.ambientScale < clear.output().ambientScale);

    WeatherSystem rain;
    rain.reset(WeatherPreset::RAIN);
    const WeatherOutput rainOut = rain.output();
    CHECK(rainOut.precipitation.rainRate > 0.0f);
    CHECK(rainOut.precipitation.rainMaxParticles > 0u);
    CHECK(rainOut.precipitation.snowRate == 0.0f);
    CHECK_NEAR(rainOut.precipitation.wind.x, 6.0f, 1e-6);
    CHECK_NEAR(rainOut.precipitation.wind.z, 1.0f, 1e-6);
    CHECK(rainOut.fog.density > clear.output().fog.density);

    WeatherSystem snow;
    snow.reset(WeatherPreset::SNOW);
    CHECK(snow.output().precipitation.snowRate > 0.0f);
    CHECK(snow.output().precipitation.rainRate == 0.0f);
}

TEST_CASE(weatherDeterministicForSameDtSequence) {
    WeatherSystem a;
    WeatherSystem b;
    a.setPreset(WeatherPreset::STORM, 4.0f);
    b.setPreset(WeatherPreset::STORM, 4.0f);
    const float steps[5] = {0.016f, 0.033f, 0.1f, 0.25f, 0.5f};
    for (int i = 0; i < 5; ++i) {
        a.update(steps[i]);
        b.update(steps[i]);
    }
    CHECK_NEAR(a.state().rain, b.state().rain, 0.0f);
    CHECK_NEAR(a.state().cloudCoverage, b.state().cloudCoverage, 0.0f);
    CHECK_NEAR(a.output().fog.density, b.output().fog.density, 0.0f);
    CHECK_NEAR(a.timeSeconds(), b.timeSeconds(), 0.0f);
}
