// testAtmosphere.cpp — L4a 大气 CPU 基座单测：参数校验、光学深度/透射率、
// 大气顶求交、LUT 生成与双线性采样（与 atmosphereParams.h 契约一致）。
#include "testFramework.h"

#include "atmosphereParams.h"

#include <cmath>
#include <cstring>
#include <limits>
#include <vector>

using namespace GN3d;

namespace {

bool finite3(const float v[3]) {
    return std::isfinite(v[0]) && std::isfinite(v[1]) && std::isfinite(v[2]);
}

} // namespace

TEST_CASE(atmosphereDefaultParams) {
    const sATMOSPHERE_PARAMS p = makeDefaultAtmosphereParams();
    CHECK(validateAtmosphereParams(p));
    CHECK(p.groundRadius > 6.0e6f);
    CHECK(p.atmosphereRadius > p.groundRadius);
    CHECK(p.rayleighScaleHeight > 0.0f);
    CHECK(p.mieScaleHeight > 0.0f);
    CHECK(p.mieAnisotropy > -1.0f && p.mieAnisotropy < 1.0f);
    CHECK(p.rayleighScattering[2] > p.rayleighScattering[0]); // 蓝光散射更强
}

TEST_CASE(atmosphereValidateClamps) {
    sATMOSPHERE_PARAMS p = makeDefaultAtmosphereParams();
    sATMOSPHERE_PARAMS fixed;
    CHECK(validateAtmosphereParams(p, &fixed));

    p.rayleighScattering[0] = std::numeric_limits<float>::quiet_NaN();
    CHECK(!validateAtmosphereParams(p, &fixed));
    CHECK(std::isfinite(fixed.rayleighScattering[0]));

    p = makeDefaultAtmosphereParams();
    p.atmosphereRadius = p.groundRadius; // 必须 > 地面
    CHECK(!validateAtmosphereParams(p, &fixed));
    CHECK(fixed.atmosphereRadius > fixed.groundRadius);

    p = makeDefaultAtmosphereParams();
    p.mieAnisotropy = 2.0f;
    CHECK(!validateAtmosphereParams(p, &fixed));
    CHECK(fixed.mieAnisotropy < 0.9991f);

    p = makeDefaultAtmosphereParams();
    p.mieScattering = -1.0f;
    CHECK(!validateAtmosphereParams(p, &fixed));
    CHECK(fixed.mieScattering >= 0.0f);
}

TEST_CASE(atmosphereTransmittanceZenith) {
    const sATMOSPHERE_PARAMS p = makeDefaultAtmosphereParams();
    const float origin[3] = {0.0f, p.groundRadius, 0.0f};
    const float up[3] = {0.0f, 1.0f, 0.0f};
    float t[3];
    CHECK(computeTransmittance(p, origin, up, t));
    CHECK(finite3(t));
    CHECK(t[0] > 0.0f && t[0] <= 1.0f);
    CHECK(t[1] > 0.0f && t[1] <= 1.0f);
    CHECK(t[2] > 0.0f && t[2] <= 1.0f);
    // Rayleigh：蓝光衰减 > 红光（天顶仍偏蓝但蓝光透射更低）。
    CHECK(t[2] < t[0]);
}

TEST_CASE(atmosphereTransmittanceGroundBlocked) {
    const sATMOSPHERE_PARAMS p = makeDefaultAtmosphereParams();
    const float origin[3] = {0.0f, p.groundRadius + 1000.0f, 0.0f};
    const float down[3] = {0.0f, -1.0f, 0.0f};
    float t[3];
    CHECK(computeTransmittance(p, origin, down, t));
    CHECK(t[0] == 0.0f && t[1] == 0.0f && t[2] == 0.0f);
}

TEST_CASE(atmosphereTransmittanceAltitudeMonotonic) {
    const sATMOSPHERE_PARAMS p = makeDefaultAtmosphereParams();
    const float up[3] = {0.0f, 1.0f, 0.0f};
    const float lowOrigin[3] = {0.0f, p.groundRadius, 0.0f};
    const float highOrigin[3] = {0.0f, p.groundRadius + 60000.0f, 0.0f};
    float low[3];
    float high[3];
    CHECK(computeTransmittance(p, lowOrigin, up, low));
    CHECK(computeTransmittance(p, highOrigin, up, high));
    // 高空空气更少 → 透射率更高。
    CHECK(high[0] > low[0]);
    CHECK(high[2] >= low[2]);
}

TEST_CASE(atmosphereIntersect) {
    const sATMOSPHERE_PARAMS p = makeDefaultAtmosphereParams();
    const float origin[3] = {0.0f, p.groundRadius + 1000.0f, 0.0f};
    const float up[3] = {0.0f, 1.0f, 0.0f};
    const float down[3] = {0.0f, -1.0f, 0.0f};
    bool hitsGround = false;
    const float tUp = intersectAtmosphere(p, origin, up, &hitsGround);
    CHECK(tUp > 0.0f);
    CHECK(!hitsGround);
    const float tDown = intersectAtmosphere(p, origin, down, &hitsGround);
    CHECK(tDown > 0.0f);
    CHECK(hitsGround);
}

TEST_CASE(atmosphereTransmittanceLut) {
    const sATMOSPHERE_PARAMS p = makeDefaultAtmosphereParams();
    const int width = 32;
    const int height = 16;
    std::vector<float> lut((size_t)width * (size_t)height * 4u, 0.0f);
    CHECK(computeTransmittanceLut(p, width, height, lut.data()));
    for(size_t i = 0; i < (size_t)width * (size_t)height; ++i) {
        CHECK(std::isfinite(lut[i * 4 + 0]));
        CHECK(std::isfinite(lut[i * 4 + 1]));
        CHECK(std::isfinite(lut[i * 4 + 2]));
        CHECK(lut[i * 4 + 0] >= 0.0f && lut[i * 4 + 0] <= 1.0f);
        CHECK(lut[i * 4 + 3] == 1.0f);
    }
    // 天顶、大气顶：几乎全透射；地平线：蓝光透射 < 红光。
    float zenith[3];
    float horizon[3];
    CHECK(evalTransmittanceLut(lut.data(), width, height, 1.0f, 1.0f, zenith));
    CHECK(evalTransmittanceLut(lut.data(), width, height, 0.0f, 0.0f, horizon));
    CHECK(zenith[0] > 0.9f);
    CHECK(horizon[2] < horizon[0]);

    // 越界输入被钳制且不产生 NaN。
    float clamped[3];
    CHECK(evalTransmittanceLut(lut.data(), width, height, -5.0f, 9.0f, clamped));
    CHECK(finite3(clamped));

    // 空指针/非法尺寸被拒绝。
    CHECK(!computeTransmittanceLut(p, 0, height, lut.data()));
    CHECK(!computeTransmittanceLut(p, width, height, nullptr));
    CHECK(!evalTransmittanceLut(nullptr, width, height, 0.5f, 0.5f, clamped));
}

TEST_CASE(atmosphereSkyRadiance) {
    const sATMOSPHERE_PARAMS p = makeDefaultAtmosphereParams();
    const int tw = 32;
    const int th = 16;
    std::vector<float> trans((size_t)tw * (size_t)th * 4u, 0.0f);
    CHECK(computeTransmittanceLut(p, tw, th, trans.data()));

    // 太阳高度角约 45°。
    const float sun[3] = {0.5f, 0.70710678f, 0.5f};
    const float towardSun[3] = {0.5f, 0.70710678f, 0.5f};
    const float awayFromSun[3] = {-0.5f, 0.70710678f, -0.5f};
    const float down[3] = {0.0f, -1.0f, 0.0f};
    float sunView[3];
    float awayView[3];
    float groundView[3];
    CHECK(computeSkyRadiance(p, trans.data(), tw, th, towardSun, sun, sunView));
    CHECK(computeSkyRadiance(p, trans.data(), tw, th, awayFromSun, sun, awayView));
    CHECK(computeSkyRadiance(p, trans.data(), tw, th, down, sun, groundView));
    CHECK(finite3(sunView) && finite3(awayView));
    CHECK(sunView[0] >= 0.0f && sunView[1] >= 0.0f && sunView[2] >= 0.0f);
    CHECK(sunView[0] > awayView[0]); // 朝阳方向更亮（单次散射）
    CHECK(groundView[0] == 0.0f && groundView[1] == 0.0f && groundView[2] == 0.0f);
    CHECK(!computeSkyRadiance(p, nullptr, tw, th, towardSun, sun, sunView));
}

TEST_CASE(atmosphereSkyViewLut) {
    const sATMOSPHERE_PARAMS p = makeDefaultAtmosphereParams();
    const int tw = 32;
    const int th = 16;
    std::vector<float> trans((size_t)tw * (size_t)th * 4u, 0.0f);
    CHECK(computeTransmittanceLut(p, tw, th, trans.data()));

    const int sw = 32;
    const int sh = 18;
    std::vector<float> sky((size_t)sw * (size_t)sh * 4u, 0.0f);
    const float sun[3] = {0.5f, 0.70710678f, 0.5f};
    CHECK(computeSkyViewLut(p, trans.data(), tw, th, sun, sw, sh, sky.data()));

    for(size_t i = 0; i < (size_t)sw * (size_t)sh; ++i) {
        CHECK(std::isfinite(sky[i * 4 + 0]));
        CHECK(sky[i * 4 + 0] >= 0.0f);
        CHECK(sky[i * 4 + 3] == 1.0f);
    }

    float sunDirValue[3];
    float awayDirValue[3];
    const float away[3] = {-0.5f, 0.70710678f, -0.5f};
    CHECK(evalSkyViewLut(sky.data(), sw, sh, sun, sun, sunDirValue));
    CHECK(evalSkyViewLut(sky.data(), sw, sh, away, sun, awayDirValue));
    CHECK(finite3(sunDirValue) && finite3(awayDirValue));
    CHECK(sunDirValue[0] >= awayDirValue[0]);

    float clamped[3];
    CHECK(evalSkyViewLut(sky.data(), sw, sh, away, sun, clamped));
    CHECK(finite3(clamped));
    CHECK(!computeSkyViewLut(p, trans.data(), tw, th, sun, 0, sh, sky.data()));
    CHECK(!evalSkyViewLut(nullptr, sw, sh, sun, sun, clamped));
}

TEST_CASE(atmosphereWeatherMapping) {
    const sATMOSPHERE_PARAMS base = makeDefaultAtmosphereParams();
    const sATMOSPHERE_WEATHER_INPUT clear = {0.0f, 0.0f, 1.0f};
    const sATMOSPHERE_PARAMS clearMapped = mapWeatherToAtmosphere(base, clear);
    CHECK(std::fabs(clearMapped.mieScattering - base.mieScattering) < 1e-12f);
    CHECK(std::fabs(clearMapped.sunIntensity - base.sunIntensity) < 1e-6f);

    const sATMOSPHERE_WEATHER_INPUT cloudy = {0.8f, 0.0f, 1.0f};
    const sATMOSPHERE_PARAMS cloudyMapped = mapWeatherToAtmosphere(base, cloudy);
    CHECK(cloudyMapped.mieScattering > base.mieScattering);
    CHECK(cloudyMapped.sunIntensity < base.sunIntensity);
    CHECK(validateAtmosphereParams(cloudyMapped));

    const sATMOSPHERE_WEATHER_INPUT foggy = {0.0f, 0.6f, 2.0f};
    const sATMOSPHERE_PARAMS foggyMapped = mapWeatherToAtmosphere(base, foggy);
    CHECK(foggyMapped.mieScattering > cloudyMapped.mieScattering);

    const sATMOSPHERE_WEATHER_INPUT invalid = {-3.0f, 9.0f, -1.0f};
    const sATMOSPHERE_PARAMS invalidMapped = mapWeatherToAtmosphere(base, invalid);
    CHECK(validateAtmosphereParams(invalidMapped));
    CHECK(invalidMapped.sunIntensity >= 0.0f);
}
