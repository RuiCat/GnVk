// testProbeRelocation.cpp — CPU probe relocation：合法性、空场景、单面贴墙、逐轴 clamp。
#include <GN.h>
#include "GN_3DprobeRelocation.h"
#include "testFramework.h"

#include <cmath>
#include <vector>

namespace {
using namespace GN3d;

sPROBE_RELOCATION_DESC makeDesc(const float *depth, uint32_t probes, uint32_t res) {
    sPROBE_RELOCATION_DESC desc;
    desc.aDepthMoments = depth;
    desc.aProbeCount = probes;
    desc.aDepthRes = res;
    desc.aSpacing = 0.228f;
    desc.aThreshold = 0.5f;
    desc.aMaxOffset = 0.5f * desc.aSpacing;
    return desc;
}

} // namespace

TEST_CASE(probeRelocationRejectsInvalid) {
    float out[4] = {9, 9, 9, 9};
    sPROBE_RELOCATION_DESC desc = makeDesc(0, 1, 4);
    CHECK(!computeProbeRelocationOffsets(desc, out));
    CHECK(!computeProbeRelocationOffsets(desc, 0));
    desc.aDepthMoments = out;
    desc.aProbeCount = 0;
    CHECK(!computeProbeRelocationOffsets(desc, out));
    desc.aProbeCount = 1;
    desc.aDepthRes = 0;
    CHECK(!computeProbeRelocationOffsets(desc, out));
}

TEST_CASE(probeRelocationNoShortHitsKeepsZero) {
    const uint32_t probes = 2, res = 4;
    std::vector<float> depth(probes * 6 * res * res * 2, 1.0e4f);
    std::vector<float> out(probes * 4, 9.0f);
    sPROBE_RELOCATION_DESC desc = makeDesc(depth.data(), probes, res);
    CHECK(computeProbeRelocationOffsets(desc, out.data()));
    for (float v : out) CHECK_NEAR(v, 0.0f, 1e-6);
}

TEST_CASE(probeRelocationWallPushesAwayAndClamps) {
    const uint32_t probes = 1, res = 4;
    std::vector<float> depth(probes * 6 * res * res * 2, 1.0e4f);
    // face 0（+X 面）全部 texel 极近：探针贴墙，应沿 -X 推开。
    for (uint32_t y = 0; y < res; ++y) {
        for (uint32_t x = 0; x < res; ++x) {
            const uint32_t idx = ((0u * 6u + 0u) * res + y) * res + x;
            depth[idx * 2u] = 0.01f;
        }
    }
    std::vector<float> out(probes * 4, 0.0f);
    sPROBE_RELOCATION_DESC desc = makeDesc(depth.data(), probes, res);
    CHECK(computeProbeRelocationOffsets(desc, out.data()));
    CHECK(out[0] < -0.01f); // -X
    CHECK_NEAR(out[1], 0.0f, 1e-5);
    CHECK_NEAR(out[2], 0.0f, 1e-5);
    CHECK_NEAR(out[3], 1.0f, 1e-6);
    CHECK(std::fabs(out[0]) <= desc.aMaxOffset + 1e-6f);
}


TEST_CASE(probeRelocationOffsetScalesWithScore) {
    // 单个短命中 texel：strength = (1/96)/0.5，偏移长度 = maxOffset * strength。
    const uint32_t probes = 1, res = 4;
    std::vector<float> depth(probes * 6 * res * res * 2, 1.0e4f);
    const uint32_t idx = 0; // face 0, texel (0,0)
    depth[idx * 2u] = 0.01f;
    std::vector<float> out(probes * 4, 0.0f);
    sPROBE_RELOCATION_DESC desc = makeDesc(depth.data(), probes, res);
    CHECK(computeProbeRelocationOffsets(desc, out.data()));
    const float totalTexels = 6.0f * (float)(res * res);
    const float expectedLen = desc.aMaxOffset * ((1.0f / totalTexels) / 0.5f);
    const float len = std::sqrt(out[0] * out[0] + out[1] * out[1] + out[2] * out[2]);
    CHECK_NEAR(len, expectedLen, 1e-5);
    CHECK_NEAR(out[3], 1.0f, 1e-6);
    // 短命中在 +X 面，推开方向必须离开该面（三个分量均为负）。
    CHECK(out[0] < 0.0f);
    CHECK(out[1] < 0.0f);
    CHECK(out[2] < 0.0f);
}

TEST_CASE(probeRelocationAllFacesTightStillClamps) {
    const uint32_t probes = 1, res = 4;
    std::vector<float> depth(probes * 6 * res * res * 2, 0.01f);
    std::vector<float> out(probes * 4, 0.0f);
    sPROBE_RELOCATION_DESC desc = makeDesc(depth.data(), probes, res);
    CHECK(computeProbeRelocationOffsets(desc, out.data()));
    CHECK(std::isfinite(out[0]) && std::isfinite(out[1]) && std::isfinite(out[2]));
    CHECK(std::fabs(out[0]) <= desc.aMaxOffset + 1e-6f);
    CHECK(std::fabs(out[1]) <= desc.aMaxOffset + 1e-6f);
    CHECK(std::fabs(out[2]) <= desc.aMaxOffset + 1e-6f);
    CHECK_NEAR(out[3], 1.0f, 1e-6);
}
