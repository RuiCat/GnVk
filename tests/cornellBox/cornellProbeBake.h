// cornellProbeBake.h — Cornell 场景的 SH9 光照探针 CPU 烘焙
#ifndef CORNELL_PROBE_BAKE_H
#define CORNELL_PROBE_BAKE_H

#include <GN.h>

#include <cstdint>
#include <vector>

namespace Cornell {

struct sProbeBakeResult {
    GNg::sVEC3 aOrigin;
    float aSpacing;
    uint32_t aCounts[3];
    std::vector<float> aSh9;   // probes * 36 floats（raw radiance SH）
    std::vector<float> aDepth; // probes * 6 * aDepthRes² * 2（mean, mean²）
    uint32_t aDepthRes;
};

// 静态场景/单光照模式；faceSize=8, samples=8 时约 2.8M paths。
bool bakeProbeGrid(int lightMode, int faceSize, int samples, sProbeBakeResult *out);

} // namespace Cornell
#endif // CORNELL_PROBE_BAKE_H
