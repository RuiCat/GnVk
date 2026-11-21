// GN_Environment.cpp — 环境光照预设的 CPU 实现（室内均匀 SH9 / 天空 IBL 选择）。
#include "GN_Environment.h"

#include <cmath>
#include <cstdio>

namespace GN3d {
namespace {

const float kAmbientMin = 0.0f;
const float kAmbientMax = 4.0f;

void setReason(char *reason, size_t reasonSize, const char *message) {
    if(reason && reasonSize) snprintf(reason, reasonSize, "%s", message);
}

} // namespace

bool validateEnvironmentLighting(eENV_LIGHTING mode, float ambientScale, float *outScale,
                                 char *reason, size_t reasonSize) {
    if(reason && reasonSize) reason[0] = 0;
    if(mode != eENV_LIGHTING_SKY_IBL && mode != eENV_LIGHTING_INDOOR_UNIFORM) {
        setReason(reason, reasonSize, "unknown environment lighting mode");
        return false;
    }
    float scale = ambientScale;
    if(!std::isfinite(scale)) scale = kAmbientMin;
    if(scale < kAmbientMin) scale = kAmbientMin;
    if(scale > kAmbientMax) scale = kAmbientMax;
    if(outScale) *outScale = scale;
    return true;
}

bool buildEnvironmentGi(eENV_LIGHTING mode, float ambientScale, sGI_UBO *outUbo) {
    if(!outUbo) return false;
    float scale = 0.0f;
    if(!validateEnvironmentLighting(mode, ambientScale, &scale, 0, 0)) return false;
    if(mode == eENV_LIGHTING_SKY_IBL) return false;
    sGI_UBO ubo = makeDefaultGiUbo();
    // 均匀环境辐亮度 L 的 SH00 = L*sqrt(4pi)；其余系数清零，避免默认 UBO 的方向性偏色。
    const float constant = scale * std::sqrt(4.0f * 3.14159265358979323846f);
    ubo.sh[0] = constant * 1.00f;
    ubo.sh[1] = constant * 0.97f;
    ubo.sh[2] = constant * 0.92f;
    ubo.sh[3] = 0.0f;
    for(int index = 4; index < SH9_FLOAT_COUNT; ++index) ubo.sh[index] = 0.0f;
    ubo.indirect[0] = 1.0f;
    ubo.indirect[1] = 0.25f;
    ubo.indirect[2] = 1.0f;
    ubo.indirect[3] = 0.0f;
    *outUbo = ubo;
    return true;
}

} // namespace GN3d
