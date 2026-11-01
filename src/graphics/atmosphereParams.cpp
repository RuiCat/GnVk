// atmosphereParams.cpp — L4a 大气 CPU 基座：Hillaire 2020 参数、光学深度积分、透射率 LUT。
// 公式与 docs/LIGHTING_PLAN.md §4.6 配套；GPU 接线前先用本层单测锁数值行为。
#include "atmosphereParams.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace GN3d {

namespace {

float clampFinite(float value, float fallback, float lo, float hi) {
    if(!std::isfinite(value)) value = fallback;
    if(value < lo) value = lo;
    if(value > hi) value = hi;
    return value;
}

void normalize3(const float in[3], float out[3]) {
    const float len = std::sqrt(in[0] * in[0] + in[1] * in[1] + in[2] * in[2]);
    if(!(len > 1e-12f)) {
        out[0] = out[1] = out[2] = 0.0f;
        return;
    }
    out[0] = in[0] / len;
    out[1] = in[1] / len;
    out[2] = in[2] / len;
}

} // namespace

sATMOSPHERE_PARAMS makeDefaultAtmosphereParams() {
    sATMOSPHERE_PARAMS p;
    std::memset(&p, 0, sizeof(p));
    p.groundRadius = 6360000.0f;
    p.atmosphereRadius = 6420000.0f;
    p.rayleighScattering[0] = 5.802e-6f;
    p.rayleighScattering[1] = 13.558e-6f;
    p.rayleighScattering[2] = 33.100e-6f;
    p.rayleighScaleHeight = 8000.0f;
    p.mieScattering = 3.996e-6f;
    p.mieAbsorption = 4.400e-6f;
    p.mieScaleHeight = 1200.0f;
    p.mieAnisotropy = 0.8f;
    p.ozoneAbsorption[0] = 0.650e-6f;
    p.ozoneAbsorption[1] = 1.881e-6f;
    p.ozoneAbsorption[2] = 0.085e-6f;
    p.ozoneCenterHeight = 25000.0f;
    p.ozoneWidth = 15000.0f;
    p.sunIntensity = 20.0f;
    return p;
}

bool validateAtmosphereParams(const sATMOSPHERE_PARAMS& params, sATMOSPHERE_PARAMS* corrected) {
    sATMOSPHERE_PARAMS fixed = params;
    bool valid = true;

    fixed.groundRadius = clampFinite(params.groundRadius, 6360000.0f, 1.0f, 1.0e9f);
    fixed.atmosphereRadius = clampFinite(params.atmosphereRadius, 6420000.0f, 1.0f, 1.0e9f);
    if(!(fixed.atmosphereRadius > fixed.groundRadius + 1.0f)) {
        fixed.atmosphereRadius = fixed.groundRadius + 1.0f;
    }
    for(int c = 0; c < 3; ++c) {
        fixed.rayleighScattering[c] =
            clampFinite(params.rayleighScattering[c], 0.0f, 0.0f, 1.0e-2f);
        fixed.ozoneAbsorption[c] =
            clampFinite(params.ozoneAbsorption[c], 0.0f, 0.0f, 1.0e-2f);
    }
    fixed.rayleighScaleHeight = clampFinite(params.rayleighScaleHeight, 8000.0f, 1.0f, 1.0e6f);
    fixed.mieScattering = clampFinite(params.mieScattering, 0.0f, 0.0f, 1.0e-2f);
    fixed.mieAbsorption = clampFinite(params.mieAbsorption, 0.0f, 0.0f, 1.0e-2f);
    fixed.mieScaleHeight = clampFinite(params.mieScaleHeight, 1200.0f, 1.0f, 1.0e6f);
    fixed.mieAnisotropy = clampFinite(params.mieAnisotropy, 0.8f, -0.999f, 0.999f);
    fixed.ozoneCenterHeight = clampFinite(params.ozoneCenterHeight, 25000.0f, 0.0f, 1.0e6f);
    fixed.ozoneWidth = clampFinite(params.ozoneWidth, 15000.0f, 0.0f, 1.0e6f);
    fixed.sunIntensity = clampFinite(params.sunIntensity, 20.0f, 0.0f, 1.0e4f);

    if(!std::isfinite(params.groundRadius) || params.groundRadius != fixed.groundRadius) valid = false;
    if(!std::isfinite(params.atmosphereRadius) || params.atmosphereRadius != fixed.atmosphereRadius) valid = false;
    for(int c = 0; c < 3; ++c) {
        if(!std::isfinite(params.rayleighScattering[c]) ||
           params.rayleighScattering[c] != fixed.rayleighScattering[c]) {
            valid = false;
        }
        if(!std::isfinite(params.ozoneAbsorption[c]) ||
           params.ozoneAbsorption[c] != fixed.ozoneAbsorption[c]) {
            valid = false;
        }
    }
    if(!std::isfinite(params.rayleighScaleHeight) ||
       params.rayleighScaleHeight != fixed.rayleighScaleHeight) valid = false;
    if(!std::isfinite(params.mieScattering) || params.mieScattering != fixed.mieScattering) valid = false;
    if(!std::isfinite(params.mieAbsorption) || params.mieAbsorption != fixed.mieAbsorption) valid = false;
    if(!std::isfinite(params.mieScaleHeight) || params.mieScaleHeight != fixed.mieScaleHeight) valid = false;
    if(!std::isfinite(params.mieAnisotropy) || params.mieAnisotropy != fixed.mieAnisotropy) valid = false;
    if(!std::isfinite(params.ozoneCenterHeight) ||
       params.ozoneCenterHeight != fixed.ozoneCenterHeight) valid = false;
    if(!std::isfinite(params.ozoneWidth) || params.ozoneWidth != fixed.ozoneWidth) valid = false;
    if(!std::isfinite(params.sunIntensity) || params.sunIntensity != fixed.sunIntensity) valid = false;

    if(corrected) *corrected = fixed;
    return valid;
}

float intersectAtmosphere(const sATMOSPHERE_PARAMS& params, const float origin[3],
                          const float dir[3], bool* hitsGround) {
    if(hitsGround) *hitsGround = false;
    float d[3];
    normalize3(dir, d);
    const float r = std::sqrt(origin[0] * origin[0] + origin[1] * origin[1] + origin[2] * origin[2]);
    const float mu = (origin[0] * d[0] + origin[1] * d[1] + origin[2] * d[2]) / std::fmax(r, 1e-6f);

    const float topDisc = r * r * (mu * mu - 1.0f) + params.atmosphereRadius * params.atmosphereRadius;
    if(topDisc < 0.0f) return -1.0f;
    const float tTop = -r * mu + std::sqrt(topDisc);
    if(tTop <= 0.0f) return -1.0f;

    if(hitsGround && mu < 0.0f) {
        const float groundDisc = r * r * (mu * mu - 1.0f) + params.groundRadius * params.groundRadius;
        if(groundDisc >= 0.0f) {
            const float tGround = -r * mu - std::sqrt(groundDisc);
            if(tGround > 0.0f && tGround < tTop) *hitsGround = true;
        }
    }
    return tTop;
}

bool computeTransmittance(const sATMOSPHERE_PARAMS& params, const float origin[3],
                          const float dir[3], float outTransmittance[3]) {
    outTransmittance[0] = outTransmittance[1] = outTransmittance[2] = 0.0f;
    if(!origin || !dir) return false;
    sATMOSPHERE_PARAMS p;
    validateAtmosphereParams(params, &p); // 用钳制值继续积分；合法性由调用方先查
    float d[3];
    normalize3(dir, d);
    const float r = std::sqrt(origin[0] * origin[0] + origin[1] * origin[1] + origin[2] * origin[2]);
    if(!(r > 0.0f)) return false;
    const float mu = (origin[0] * d[0] + origin[1] * d[1] + origin[2] * d[2]) / r;

    bool hitsGround = false;
    const float tTop = intersectAtmosphere(p, origin, d, &hitsGround);
    if(tTop <= 0.0f) { // 在真空/大气外，视为全透射
        outTransmittance[0] = outTransmittance[1] = outTransmittance[2] = 1.0f;
        return true;
    }
    if(hitsGround) {
        // 视线被行星挡住（地面/夜面）→ 透射率 0（本层不做地面反弹）。
        return true;
    }

    const float step = tTop / (float)ATMOSPHERE_TRANSMITTANCE_STEPS;
    float odRayleigh = 0.0f;
    float odMie = 0.0f;
    float odOzone = 0.0f;
    for(int i = 0; i < ATMOSPHERE_TRANSMITTANCE_STEPS; ++i) {
        const float t = ((float)i + 0.5f) * step;
        const float px = origin[0] + d[0] * t;
        const float py = origin[1] + d[1] * t;
        const float pz = origin[2] + d[2] * t;
        const float height = std::sqrt(px * px + py * py + pz * pz) - p.groundRadius;
        const float clampedHeight = std::fmax(height, 0.0f);
        const float dR = std::exp(-clampedHeight / p.rayleighScaleHeight);
        const float dM = std::exp(-clampedHeight / p.mieScaleHeight);
        const float dO = p.ozoneWidth > 1e-6f
                             ? std::fmax(0.0f,
                                         1.0f - std::fabs(clampedHeight - p.ozoneCenterHeight) /
                                                    p.ozoneWidth)
                             : 0.0f;
        odRayleigh += dR * step;
        odMie += dM * step;
        odOzone += dO * step;
    }

    const float mieExtinction = p.mieScattering + p.mieAbsorption;
    for(int c = 0; c < 3; ++c) {
        const float opticalDepth =
            p.rayleighScattering[c] * odRayleigh + mieExtinction * odMie +
            p.ozoneAbsorption[c] * odOzone;
        outTransmittance[c] = std::exp(-opticalDepth);
    }
    return true;
}

bool computeTransmittanceLut(const sATMOSPHERE_PARAMS& params, int width, int height,
                             float* rgba) {
    if(!rgba || width <= 0 || height <= 0) return false;
    sATMOSPHERE_PARAMS p;
    validateAtmosphereParams(params, &p);

    for(int y = 0; y < height; ++y) {
        const float v = ((float)y + 0.5f) / (float)height; // 0=地面，1=大气顶
        const float r = p.groundRadius + v * (p.atmosphereRadius - p.groundRadius);
        for(int x = 0; x < width; ++x) {
            const float u = ((float)x + 0.5f) / (float)width; // 0=向下，1=天顶
            const float mu = -1.0f + 2.0f * u;
            const float sinTheta = std::sqrt(std::fmax(0.0f, 1.0f - mu * mu));
            const float origin[3] = {0.0f, r, 0.0f};
            const float dir[3] = {sinTheta, mu, 0.0f};
            float t[3];
            computeTransmittance(p, origin, dir, t);
            float* texel = rgba + ((size_t)y * (size_t)width + (size_t)x) * 4u;
            texel[0] = t[0];
            texel[1] = t[1];
            texel[2] = t[2];
            texel[3] = 1.0f;
        }
    }
    return true;
}

bool evalTransmittanceLut(const float* rgba, int width, int height, float height01, float mu,
                          float outTransmittance[3]) {
    outTransmittance[0] = outTransmittance[1] = outTransmittance[2] = 0.0f;
    if(!rgba || width <= 0 || height <= 0) return false;
    if(!std::isfinite(height01) || !std::isfinite(mu)) return false;

    // 与 computeTransmittanceLut 相同的线性参数化：mu∈[-1,1]、height01∈[0,1]。
    const float u = (mu + 1.0f) * 0.5f;
    const float v = height01;
    const float fx = std::fmax(0.0f, std::fmin(1.0f, u)) * (float)width - 0.5f;
    const float fy = std::fmax(0.0f, std::fmin(1.0f, v)) * (float)height - 0.5f;
    const int x0 = (int)std::floor(fx);
    const int y0 = (int)std::floor(fy);
    const float tx = fx - (float)x0;
    const float ty = fy - (float)y0;
    for(int c = 0; c < 3; ++c) {
        float samples[4];
        for(int s = 0; s < 4; ++s) {
            const int sx = std::max(0, std::min(width - 1, x0 + (s & 1)));
            const int sy = std::max(0, std::min(height - 1, y0 + ((s >> 1) & 1)));
            samples[s] = rgba[((size_t)sy * (size_t)width + (size_t)sx) * 4u + (size_t)c];
        }
        const float top = samples[0] * (1.0f - tx) + samples[1] * tx;
        const float bottom = samples[2] * (1.0f - tx) + samples[3] * tx;
        outTransmittance[c] = top * (1.0f - ty) + bottom * ty;
    }
    return true;
}

// ── L4b：单次散射天空（CPU 参考；GPU 接线前用测试锁数值）──────────────────

namespace {

const float PI = 3.14159265358979323846f;

float rayleighPhase(float mu) {
    return (3.0f / (16.0f * PI)) * (1.0f + mu * mu);
}

// Henyey-Greenstein 近似（Mie 相位）。
float miePhase(float mu, float g) {
    const float g2 = g * g;
    const float denom = std::fmax(1.0f + g2 - 2.0f * g * mu, 1e-6f);
    return (1.0f - g2) / (4.0f * PI * denom * std::sqrt(denom));
}

float dot3(const float a[3], const float b[3]) {
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

// 世界方向 → sky-view LUT uv（u=相对太阳方位角，v=天顶角；与生成端一致）。
void skyViewUv(const float viewDir[3], const float sunDir[3], float* u, float* v) {
    float d[3];
    float s[3];
    normalize3(viewDir, d);
    normalize3(sunDir, s);
    const float azView = std::atan2(d[0], d[2]);
    const float azSun = std::atan2(s[0], s[2]);
    float delta = azView - azSun;
    while(delta > PI) delta -= 2.0f * PI;
    while(delta < -PI) delta += 2.0f * PI;
    *u = (delta / PI + 1.0f) * 0.5f;
    const float mu = std::fmax(0.0f, std::fmin(1.0f, d[1]));
    *v = 1.0f - mu;
}

} // namespace

bool computeSkyRadiance(const sATMOSPHERE_PARAMS& params, const float* transmittanceLut,
                        int lutWidth, int lutHeight, const float viewDir[3],
                        const float sunDir[3], float outRgb[3]) {
    outRgb[0] = outRgb[1] = outRgb[2] = 0.0f;
    if(!transmittanceLut || !viewDir || !sunDir) return false;
    sATMOSPHERE_PARAMS p;
    validateAtmosphereParams(params, &p);

    float v[3];
    float s[3];
    normalize3(viewDir, v);
    normalize3(sunDir, s);
    const float origin[3] = {0.0f, p.groundRadius + 1.0f, 0.0f}; // 地面上 1 m
    bool hitsGround = false;
    const float tTop = intersectAtmosphere(p, origin, v, &hitsGround);
    if(tTop <= 0.0f) return true; // 无大气
    if(hitsGround) return true;   // 地面遮挡：天空 LUT 只覆盖上半球

    const float mieExtinction = p.mieScattering + p.mieAbsorption;
    const float muSun = dot3(v, s);
    const float phaseR = rayleighPhase(muSun);
    const float phaseM = miePhase(muSun, p.mieAnisotropy);
    const float step = tTop / (float)ATMOSPHERE_SKYVIEW_STEPS;

    float viewTransmittance[3] = {1.0f, 1.0f, 1.0f};
    float radiance[3] = {0.0f, 0.0f, 0.0f};
    for(int i = 0; i < ATMOSPHERE_SKYVIEW_STEPS; ++i) {
        const float t = ((float)i + 0.5f) * step;
        const float px = origin[0] + v[0] * t;
        const float py = origin[1] + v[1] * t;
        const float pz = origin[2] + v[2] * t;
        const float r = std::sqrt(px * px + py * py + pz * pz);
        const float height = std::fmax(r - p.groundRadius, 0.0f);
        const float dR = std::exp(-height / p.rayleighScaleHeight);
        const float dM = std::exp(-height / p.mieScaleHeight);
        const float dO = p.ozoneWidth > 1e-6f
                             ? std::fmax(0.0f,
                                         1.0f - std::fabs(height - p.ozoneCenterHeight) / p.ozoneWidth)
                             : 0.0f;

        // 太阳可见性：从该点向太阳做透射率查表。
        const float sunMu = (px * s[0] + py * s[1] + pz * s[2]) / std::fmax(r, 1e-6f);
        const float height01 =
            std::fmax(0.0f, std::fmin(1.0f, height / (p.atmosphereRadius - p.groundRadius)));
        float sunTransmittance[3];
        evalTransmittanceLut(transmittanceLut, lutWidth, lutHeight, height01, sunMu,
                             sunTransmittance);

        for(int c = 0; c < 3; ++c) {
            const float scatter =
                p.rayleighScattering[c] * dR * phaseR + p.mieScattering * dM * phaseM;
            radiance[c] += viewTransmittance[c] * scatter * sunTransmittance[c] *
                           p.sunIntensity * step;
            const float extinction = p.rayleighScattering[c] * dR + mieExtinction * dM +
                                     p.ozoneAbsorption[c] * dO;
            viewTransmittance[c] *= std::exp(-extinction * step);
        }
    }
    outRgb[0] = radiance[0];
    outRgb[1] = radiance[1];
    outRgb[2] = radiance[2];
    return true;
}

bool computeSkyViewLut(const sATMOSPHERE_PARAMS& params, const float* transmittanceLut,
                       int lutWidth, int lutHeight, const float sunDir[3], int width,
                       int height, float* rgba) {
    if(!rgba || !transmittanceLut || !sunDir || width <= 0 || height <= 0) return false;
    float s[3];
    normalize3(sunDir, s);
    const float azSun = std::atan2(s[0], s[2]);

    for(int y = 0; y < height; ++y) {
        const float v = ((float)y + 0.5f) / (float)height; // 0=天顶，1=地平线
        const float mu = 1.0f - v;
        const float sinTheta = std::sqrt(std::fmax(0.0f, 1.0f - mu * mu));
        for(int x = 0; x < width; ++x) {
            const float u = ((float)x + 0.5f) / (float)width;
            const float delta = (u * 2.0f - 1.0f) * PI;
            const float azimuth = azSun + delta;
            const float dir[3] = {sinTheta * std::sin(azimuth), mu, sinTheta * std::cos(azimuth)};
            float rgb[3];
            computeSkyRadiance(params, transmittanceLut, lutWidth, lutHeight, dir, s, rgb);
            float* texel = rgba + ((size_t)y * (size_t)width + (size_t)x) * 4u;
            texel[0] = rgb[0];
            texel[1] = rgb[1];
            texel[2] = rgb[2];
            texel[3] = 1.0f;
        }
    }
    return true;
}

bool evalSkyViewLut(const float* rgba, int width, int height, const float viewDir[3],
                    const float sunDir[3], float outRgb[3]) {
    outRgb[0] = outRgb[1] = outRgb[2] = 0.0f;
    if(!rgba || !viewDir || !sunDir || width <= 0 || height <= 0) return false;
    float u = 0.0f;
    float v = 0.0f;
    skyViewUv(viewDir, sunDir, &u, &v);
    const float fx = std::fmax(0.0f, std::fmin(1.0f, u)) * (float)width - 0.5f;
    const float fy = std::fmax(0.0f, std::fmin(1.0f, v)) * (float)height - 0.5f;
    const int x0 = (int)std::floor(fx);
    const int y0 = (int)std::floor(fy);
    const float tx = fx - (float)x0;
    const float ty = fy - (float)y0;
    for(int c = 0; c < 3; ++c) {
        float samples[4];
        for(int i = 0; i < 4; ++i) {
            const int sx = std::max(0, std::min(width - 1, x0 + (i & 1)));
            const int sy = std::max(0, std::min(height - 1, y0 + ((i >> 1) & 1)));
            samples[i] = rgba[((size_t)sy * (size_t)width + (size_t)sx) * 4u + (size_t)c];
        }
        const float top = samples[0] * (1.0f - tx) + samples[1] * tx;
        const float bottom = samples[2] * (1.0f - tx) + samples[3] * tx;
        outRgb[c] = top * (1.0f - ty) + bottom * ty;
    }
    return true;
}

sATMOSPHERE_PARAMS mapWeatherToAtmosphere(const sATMOSPHERE_PARAMS& base,
                                          const sATMOSPHERE_WEATHER_INPUT& weather) {
    sATMOSPHERE_PARAMS p = base;
    const float cloud = clampFinite(weather.cloudCoverage, 0.0f, 0.0f, 1.0f);
    const float fog = clampFinite(weather.fogDensity, 0.0f, 0.0f, 1.0f);
    const float sunScale = clampFinite(weather.sunIntensityScale, 1.0f, 0.0f, 4.0f);
    // 云/雾增加 Mie 散射与吸收（近似“浑浊度”）；太阳强度按云/雾衰减。
    const float mieScale = 1.0f + 2.5f * cloud + 4.0f * fog;
    p.mieScattering = base.mieScattering * mieScale;
    p.mieAbsorption = base.mieAbsorption * mieScale;
    const float sunAttenuation = std::fmax(0.0f, 1.0f - 0.55f * cloud - 0.25f * fog);
    p.sunIntensity = base.sunIntensity * sunScale * sunAttenuation;

    sATMOSPHERE_PARAMS clamped;
    validateAtmosphereParams(p, &clamped);
    return clamped;
}

} // namespace GN3d
