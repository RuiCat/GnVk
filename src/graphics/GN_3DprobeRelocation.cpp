// GN_3DprobeRelocation.cpp — 从 depth moments 估计 probe relocation 偏移。
// 纯 CPU；方向映射复用 giParams::cubemapFaceDirection，保证与烘焙/着色器一致。
#include "GN_3DprobeRelocation.h"

#include "giParams.h"

#include <cmath>
#include <cstring>

namespace GN3d {
namespace {

float length3(float x, float y, float z) { return std::sqrt(x * x + y * y + z * z); }

} // namespace

bool computeProbeRelocationOffsets(const sPROBE_RELOCATION_DESC &desc, float *outOffsets) {
    if (!desc.aDepthMoments || !outOffsets || desc.aProbeCount == 0u || desc.aDepthRes == 0u ||
        !(desc.aSpacing > 0.0f) || !(desc.aThreshold > 0.0f) || !(desc.aMaxOffset > 0.0f)) {
        return false;
    }
    const uint32_t texelsPerFace = desc.aDepthRes * desc.aDepthRes;
    const float threshold = desc.aThreshold * desc.aSpacing;
    const uint32_t floatsPerProbe = desc.aProbeCount * 4u;
    std::memset(outOffsets, 0, sizeof(float) * floatsPerProbe);

    for (uint32_t probe = 0; probe < desc.aProbeCount; ++probe) {
        float sumX = 0.0f, sumY = 0.0f, sumZ = 0.0f;
        float minMean = 1.0e30f;
        int minFace = 0;
        float minU = 0.0f, minV = 0.0f;
        uint32_t shortHits = 0;
        const uint32_t totalTexels = 6u * texelsPerFace;
        for (uint32_t face = 0; face < 6u; ++face) {
            for (uint32_t y = 0; y < desc.aDepthRes; ++y) {
                for (uint32_t x = 0; x < desc.aDepthRes; ++x) {
                    const uint32_t idx =
                        (((probe * 6u + face) * desc.aDepthRes + y) * desc.aDepthRes + x) * 2u;
                    const float mean = desc.aDepthMoments[idx];
                    if (!(mean < threshold)) continue;
                    const float u = (static_cast<float>(x) + 0.5f) /
                                        static_cast<float>(desc.aDepthRes) * 2.0f - 1.0f;
                    const float v = (static_cast<float>(y) + 0.5f) /
                                        static_cast<float>(desc.aDepthRes) * 2.0f - 1.0f;
                    float dir[3] = {0.0f, 0.0f, 1.0f};
                    cubemapFaceDirection(static_cast<int>(face), u, v, dir);
                    const float weight = threshold - mean;
                    sumX += dir[0] * weight;
                    sumY += dir[1] * weight;
                    sumZ += dir[2] * weight;
                    ++shortHits;
                    if (mean < minMean) {
                        minMean = mean;
                        minFace = static_cast<int>(face);
                        minU = u;
                        minV = v;
                    }
                }
            }
        }
        if (shortHits == 0u) continue;

        float awayX = 0.0f, awayY = 0.0f, awayZ = 0.0f;
        const float sumLen = length3(sumX, sumY, sumZ);
        if (sumLen > 1.0e-6f) {
            awayX = -sumX / sumLen;
            awayY = -sumY / sumLen;
            awayZ = -sumZ / sumLen;
        } else {
            // 各方向权重抵消（探针在球体中心等）：用最近命中面的反方向兜底。
            float dir[3] = {0.0f, 0.0f, 1.0f};
            cubemapFaceDirection(minFace, minU, minV, dir);
            awayX = -dir[0];
            awayY = -dir[1];
            awayZ = -dir[2];
        }

        const float score = static_cast<float>(shortHits) / static_cast<float>(totalTexels);
        float strength = score / 0.5f;
        if (strength > 1.0f) strength = 1.0f;
        float offsetX = awayX * desc.aMaxOffset * strength;
        float offsetY = awayY * desc.aMaxOffset * strength;
        float offsetZ = awayZ * desc.aMaxOffset * strength;
        if (offsetX > desc.aMaxOffset) offsetX = desc.aMaxOffset;
        if (offsetX < -desc.aMaxOffset) offsetX = -desc.aMaxOffset;
        if (offsetY > desc.aMaxOffset) offsetY = desc.aMaxOffset;
        if (offsetY < -desc.aMaxOffset) offsetY = -desc.aMaxOffset;
        if (offsetZ > desc.aMaxOffset) offsetZ = desc.aMaxOffset;
        if (offsetZ < -desc.aMaxOffset) offsetZ = -desc.aMaxOffset;

        float *out = outOffsets + probe * 4u;
        out[0] = offsetX;
        out[1] = offsetY;
        out[2] = offsetZ;
        out[3] = 1.0f;
    }
    return true;
}

} // namespace GN3d
