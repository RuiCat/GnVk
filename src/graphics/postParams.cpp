// postParams.cpp — L1 后处理 CPU 契约：POST UBO 默认值/校验/打包与色调映射参考实现。
// 公式必须与 assets/shaders/gn3d_composite.frag 保持一致（改一处必须同步另一处与单测）。
#include "postParams.h"

#include <cmath>
#include <cstring>

namespace GN3d {

namespace {

float clampFloat(float value, float lo, float hi) {
    if(!(value > lo)) return lo; // 同时处理 NaN
    if(value > hi) return hi;
    return value;
}

float finiteOr(float value, float fallback) {
    return std::isfinite(value) ? value : fallback;
}

} // namespace

sPOST_UBO makeDefaultPostUbo() {
    sPOST_UBO ubo;
    std::memset(&ubo, 0, sizeof(ubo));
    ubo.exposure = 1.0f;
    // L1d：mip 链 Bloom 比 4 邻域近似能量更集中；阈值抬到 1.0（线性 HDR）避免白色衣物整片泛光。
    ubo.bloomIntensity = 0.15f;
    ubo.bloomThreshold = 1.0f;
    ubo.bloomKnee = 0.5f;
    ubo.ev = 0.0f;
    ubo.tonemapMode = eTONEMAP_ACES;
    return ubo;
}

sPOST_UBO makeLegacyPostUbo() {
    sPOST_UBO ubo;
    std::memset(&ubo, 0, sizeof(ubo));
    ubo.exposure = 1.0f;
    ubo.bloomIntensity = 0.0f;
    ubo.bloomThreshold = 0.8f;
    ubo.bloomKnee = 0.3f;
    ubo.ev = 0.0f;
    ubo.tonemapMode = eTONEMAP_NONE;
    return ubo;
}

bool validatePostUbo(const sPOST_UBO& ubo, sPOST_UBO* corrected) {
    sPOST_UBO fixed = ubo;
    bool valid = true;

    fixed.exposure = clampFloat(finiteOr(ubo.exposure, 1.0f), 0.0f, 64.0f);
    fixed.bloomIntensity = clampFloat(finiteOr(ubo.bloomIntensity, 0.3f), 0.0f, 10.0f);
    fixed.bloomThreshold = clampFloat(finiteOr(ubo.bloomThreshold, 0.8f), 0.0f, 10.0f);
    fixed.bloomKnee = clampFloat(finiteOr(ubo.bloomKnee, 0.3f), 0.0f, 10.0f);
    fixed.ev = clampFloat(finiteOr(ubo.ev, 0.0f), -10.0f, 10.0f);
    fixed.fxaa = clampFloat(finiteOr(ubo.fxaa, 0.0f), 0.0f, 1.0f);
    fixed.pad1 = 0.0f;

    if(ubo.tonemapMode < eTONEMAP_NONE || ubo.tonemapMode > eTONEMAP_ACES) {
        fixed.tonemapMode = eTONEMAP_ACES;
    } else {
        fixed.tonemapMode = ubo.tonemapMode;
    }

    if(!std::isfinite(ubo.exposure) || ubo.exposure != fixed.exposure) valid = false;
    if(!std::isfinite(ubo.bloomIntensity) || ubo.bloomIntensity != fixed.bloomIntensity) valid = false;
    if(!std::isfinite(ubo.bloomThreshold) || ubo.bloomThreshold != fixed.bloomThreshold) valid = false;
    if(!std::isfinite(ubo.bloomKnee) || ubo.bloomKnee != fixed.bloomKnee) valid = false;
    if(!std::isfinite(ubo.ev) || ubo.ev != fixed.ev) valid = false;
    if(!std::isfinite(ubo.fxaa) || ubo.fxaa != fixed.fxaa) valid = false;
    if(ubo.tonemapMode != fixed.tonemapMode) valid = false;

    if(corrected) *corrected = fixed;
    return valid;
}

bool packPostUbo(const sPOST_UBO& ubo, void* outBytes) {
    if(!outBytes) return false;
    std::memcpy(outBytes, &ubo, sizeof(ubo));
    return true;
}

float exposureScaleFromEv(float ev) {
    return std::pow(2.0f, clampFloat(finiteOr(ev, 0.0f), -10.0f, 10.0f));
}

float tonemapReinhard(float linear) {
    const float x = clampFloat(linear, 0.0f, 1.0e6f);
    return x / (1.0f + x);
}

float tonemapAces(float linear) {
    // Narkowicz 2015 "ACES Filmic Tone Mapping Curve" 单曲线 fit（与 shader 同式）。
    const float x = clampFloat(linear, 0.0f, 1.0e6f);
    const float a = 2.51f;
    const float b = 0.03f;
    const float c = 2.43f;
    const float d = 0.59f;
    const float e = 0.14f;
    return clampFloat((x * (a * x + b)) / (x * (c * x + d) + e), 0.0f, 1.0f);
}

float tonemapChannel(int32_t mode, float linear, float exposure) {
    const float value = clampFloat(linear, 0.0f, 1.0e6f) * clampFloat(finiteOr(exposure, 1.0f), 0.0f, 64.0f);
    if(mode == eTONEMAP_REINHARD) return tonemapReinhard(value);
    if(mode == eTONEMAP_ACES) return tonemapAces(value);
    return clampFloat(value, 0.0f, 1.0f);
}

} // namespace GN3d
