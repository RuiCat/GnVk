// GN_3DprobeUpdate.cpp — 异步更新调度与 SH9 混合；纯 CPU，可无 GPU 单测。
#include "GN_3DprobeUpdate.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace GN3d {

void initProbeUpdateScheduler(sPROBE_UPDATE_SCHEDULER *scheduler, uint32_t probeCount,
                              uint32_t budget) {
    if (!scheduler) return;
    scheduler->aCursor = 0;
    scheduler->aBudget = budget == 0u ? 1u : budget;
    scheduler->aProbeCount = probeCount;
}

sPROBE_UPDATE_RANGE nextProbeUpdateRange(sPROBE_UPDATE_SCHEDULER *scheduler) {
    sPROBE_UPDATE_RANGE range = {0u, 0u};
    if (!scheduler || scheduler->aProbeCount == 0u) return range;
    const uint32_t budget = scheduler->aBudget == 0u ? 1u : scheduler->aBudget;
    const uint32_t start = scheduler->aCursor % scheduler->aProbeCount;
    range.aStart = start;
    range.aCount = std::min(budget, scheduler->aProbeCount - start);
    scheduler->aCursor = (start + range.aCount) % scheduler->aProbeCount;
    return range;
}

float probeUpdateAlpha(uint32_t age, float hysteresis) {
    float h = hysteresis;
    if (!(h >= 0.0f)) h = 0.0f;
    if (h > 1.0f) h = 1.0f;
    const float floorAlpha = 1.0f - h;
    const float ageAlpha = 1.0f / (static_cast<float>(age) + 1.0f);
    float alpha = std::max(floorAlpha, ageAlpha);
    if (alpha < 0.0f) alpha = 0.0f;
    if (alpha > 1.0f) alpha = 1.0f;
    return alpha;
}

void initProbeUpdateUbo(sPROBE_UPDATE_UBO *ubo) {
    if (!ubo) return;
    std::memset(ubo, 0, sizeof(*ubo));
    ubo->aWeight[1] = 4.0f;    // depthRes 与 GPV4 资产一致
    ubo->aState[0] = 4.0f;     // spp
    ubo->aState[1] = 65535.0f; // maxAge
    ubo->aState[2] = 0.001f;   // epsilon
}

bool packProbeUpdateUbo(const sPROBE_UPDATE_UBO *ubo, float out[24]) {
    if (!ubo || !out) return false;
    std::memcpy(out + 0, ubo->aOrigin, sizeof(ubo->aOrigin));
    std::memcpy(out + 4, ubo->aSpacingCounts, sizeof(ubo->aSpacingCounts));
    std::memcpy(out + 8, ubo->aWeight, sizeof(ubo->aWeight));
    std::memcpy(out + 12, ubo->aRelocation, sizeof(ubo->aRelocation));
    std::memcpy(out + 16, ubo->aUpdate, sizeof(ubo->aUpdate));
    std::memcpy(out + 20, ubo->aState, sizeof(ubo->aState));
    return true;
}

uint32_t advanceProbeAge(uint32_t age, uint32_t maxAge) {
    const uint32_t cap = maxAge == 0u ? 1u : maxAge;
    return age >= cap ? cap : age + 1u;
}

void resetProbeStates(sPROBE_STATE *states, uint32_t probeCount) {
    if (!states) return;
    std::memset(states, 0, sizeof(sPROBE_STATE) * static_cast<size_t>(probeCount));
}

void blendProbeSh(const float oldSh[36], const float sampleSh[36], float alpha, float outSh[36]) {
    if (!oldSh || !sampleSh || !outSh) return;
    float a = alpha;
    if (!(a >= 0.0f)) a = 0.0f;
    if (a > 1.0f) a = 1.0f;
    for (int i = 0; i < 36; ++i) {
        outSh[i] = oldSh[i] * (1.0f - a) + sampleSh[i] * a;
        if (!std::isfinite(outSh[i])) outSh[i] = oldSh[i];
    }
}

} // namespace GN3d
