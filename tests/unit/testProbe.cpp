// testProbe.cpp — SH9 探针网格 CPU 基座单测：Init 校验、三线性采样、Recenter 搬移、
// 时间滞后混合、非法输入与确定性。
#include "testFramework.h"

#include <GN.h>

#include "GN_3Dprobe.h"

#include <cmath>
#include <cstring>
#include <limits>
#include <vector>

namespace {

void fillSh(float sh[9][4], float value) {
    for (int i = 0; i < 9; ++i) {
        for (int c = 0; c < 4; ++c) sh[i][c] = value;
    }
}

} // namespace

TEST_CASE(probeDefaultDescAndInit) {
    GN3d::sPROBE_GRID_DESC desc;
    CHECK_NEAR(desc.aSpacing, 2.0f, 1e-6);
    CHECK_EQ(desc.aCountX, 8u);
    CHECK_EQ(desc.aCountY, 4u);
    CHECK_EQ(desc.aCountZ, 8u);
    CHECK_NEAR(desc.aOrigin.x, 0.0f, 1e-6);

    GN3d::cPROBE_GRID grid;
    CHECK(!grid.IsValid());
    CHECK(grid.Init(desc));
    CHECK(grid.IsValid());
    CHECK_EQ(grid.GetProbeCount(), 256u);
    CHECK_EQ(grid.GetCountX(), 8u);
    CHECK_EQ(grid.GetCountY(), 4u);
    CHECK_EQ(grid.GetCountZ(), 8u);
    CHECK_NEAR(grid.GetSpacing(), 2.0f, 1e-6);
}

TEST_CASE(probeRejectsInvalidDesc) {
    GN3d::cPROBE_GRID grid;
    GN3d::sPROBE_GRID_DESC desc;

    desc.aCountX = 0;
    CHECK(!grid.Init(desc));
    CHECK(!grid.IsValid());

    desc = GN3d::sPROBE_GRID_DESC();
    desc.aSpacing = 0.0f;
    CHECK(!grid.Init(desc));

    desc.aSpacing = -1.0f;
    CHECK(!grid.Init(desc));

    desc.aSpacing = std::numeric_limits<float>::quiet_NaN();
    CHECK(!grid.Init(desc));

    desc = GN3d::sPROBE_GRID_DESC();
    desc.aCountX = 16;
    desc.aCountY = 16;
    desc.aCountZ = 16; // 4096 > 1024
    CHECK(!grid.Init(desc));
    CHECK_EQ(grid.GetProbeCount(), 0u);

    desc = GN3d::sPROBE_GRID_DESC();
    desc.aCountX = 8;
    desc.aCountY = 4;
    desc.aCountZ = 32; // 1024 = 上限，允许
    CHECK(grid.Init(desc));
    CHECK_EQ(grid.GetProbeCount(), 1024u);
}

TEST_CASE(probeSetBoundsAndInvalidIndex) {
    GN3d::sPROBE_GRID_DESC desc;
    desc.aCountX = 4;
    desc.aCountY = 2;
    desc.aCountZ = 2;
    GN3d::cPROBE_GRID grid;
    CHECK(grid.Init(desc));

    float sh[9][4];
    fillSh(sh, 0.5f);
    CHECK(grid.SetProbeSh(0, sh));
    CHECK(!grid.SetProbeSh(grid.GetProbeCount(), sh));
    CHECK(!grid.SetProbeSh(0, nullptr));

    float out[9][4];
    CHECK(grid.SampleSh(GNg::sVEC3{0.0f, 0.0f, 0.0f}, out));
    for (int i = 0; i < 9; ++i) {
        for (int c = 0; c < 4; ++c) CHECK_NEAR(out[i][c], 0.5f, 1e-6);
    }
}

TEST_CASE(probeConstantGridSamplesConstant) {
    GN3d::sPROBE_GRID_DESC desc;
    desc.aOrigin = {1.0f, -2.0f, 0.5f};
    GN3d::cPROBE_GRID grid;
    CHECK(grid.Init(desc));
    float sh[9][4];
    fillSh(sh, 0.25f);
    for (uint32_t i = 0; i < grid.GetProbeCount(); ++i) CHECK(grid.SetProbeSh(i, sh));

    const GNg::sVEC3 samples[4] = {{1.0f, -2.0f, 0.5f},
                                    {3.0f, 0.0f, 2.5f},
                                    {-100.0f, 100.0f, -100.0f},
                                    {100.0f, -100.0f, 100.0f}};
    for (int s = 0; s < 4; ++s) {
        float out[9][4];
        CHECK(grid.SampleSh(samples[s], out));
        for (int i = 0; i < 9; ++i) {
            for (int c = 0; c < 4; ++c) CHECK_NEAR(out[i][c], 0.25f, 1e-6);
        }
    }
}

TEST_CASE(probeTrilinearAxisMidpoint) {
    GN3d::sPROBE_GRID_DESC desc;
    desc.aOrigin = {0.0f, 0.0f, 0.0f};
    desc.aSpacing = 2.0f;
    desc.aCountX = 2;
    desc.aCountY = 2;
    desc.aCountZ = 2;
    GN3d::cPROBE_GRID grid;
    CHECK(grid.Init(desc));
    float zero[9][4];
    float one[9][4];
    fillSh(zero, 0.0f);
    fillSh(one, 1.0f);
    for (uint32_t i = 0; i < grid.GetProbeCount(); ++i) CHECK(grid.SetProbeSh(i, zero));
    // 索引 (1,0,0) = x 方向第 2 个探针。
    CHECK(grid.SetProbeSh(1, one));

    float out[9][4];
    CHECK(grid.SampleSh(GNg::sVEC3{1.0f, 0.0f, 0.0f}, out)); // x 轴中点
    for (int i = 0; i < 9; ++i) {
        for (int c = 0; c < 4; ++c) CHECK_NEAR(out[i][c], 0.5f, 1e-6);
    }
}

TEST_CASE(probeCornerExactAndClamp) {
    GN3d::sPROBE_GRID_DESC desc;
    desc.aSpacing = 1.0f;
    desc.aCountX = 4;
    desc.aCountY = 2;
    desc.aCountZ = 3;
    GN3d::cPROBE_GRID grid;
    CHECK(grid.Init(desc));
    float sh[9][4];
    fillSh(sh, 0.75f);
    // 索引 (2,1,2) = (2 * 2 + 1) * 4 + 2 = 22。
    CHECK(grid.SetProbeSh(22, sh));

    float out[9][4];
    // 该探针的世界坐标 = origin + (2,1,2) * spacing。
    CHECK(grid.SampleSh(GNg::sVEC3{2.0f, 1.0f, 2.0f}, out));
    for (int i = 0; i < 9; ++i) {
        for (int c = 0; c < 4; ++c) CHECK_NEAR(out[i][c], 0.75f, 1e-6);
    }
    // 远超边界：夹紧到最大角 (3,1,2)，其它探针为 0。
    CHECK(grid.SampleSh(GNg::sVEC3{100.0f, 100.0f, 100.0f}, out));
    for (int i = 0; i < 9; ++i) {
        for (int c = 0; c < 4; ++c) CHECK_NEAR(out[i][c], 0.0f, 1e-6);
    }
}

TEST_CASE(probeRecenterThreshold) {
    GN3d::cPROBE_GRID grid;
    CHECK(grid.Init(GN3d::sPROBE_GRID_DESC()));
    const GNg::sVEC3 origin0 = grid.GetOrigin();

    CHECK(!grid.Recenter(GNg::sVEC3{3.9f, 0.0f, 0.0f})); // < 2×spacing：不搬
    CHECK_NEAR(grid.GetOrigin().x, origin0.x, 1e-6);

    CHECK(grid.Recenter(GNg::sVEC3{4.1f, 0.0f, 0.0f})); // >= 2×spacing：搬
    CHECK_EQ(grid.GetProbeCount(), 256u);
    CHECK_NEAR(grid.GetOrigin().x, -2.0f, 1e-5);
    CHECK_NEAR(grid.GetOrigin().y, -2.0f, 1e-5);
    CHECK_NEAR(grid.GetOrigin().z, -6.0f, 1e-5);
}

TEST_CASE(probeRecenterPreservesSh) {
    GN3d::cPROBE_GRID grid;
    CHECK(grid.Init(GN3d::sPROBE_GRID_DESC()));
    float sh[9][4];
    for (uint32_t i = 0; i < grid.GetProbeCount(); ++i) {
        fillSh(sh, static_cast<float>(i % 17) * 0.1f);
        CHECK(grid.SetProbeSh(i, sh));
    }
    const GNg::sVEC3 samplePos = {3.0f, 0.0f, 0.0f};
    float before[9][4];
    CHECK(grid.SampleSh(samplePos, before));
    CHECK(grid.Recenter(GNg::sVEC3{4.1f, 0.0f, 0.0f}));
    float after[9][4];
    CHECK(grid.SampleSh(samplePos, after));
    for (int i = 0; i < 9; ++i) {
        for (int c = 0; c < 4; ++c) CHECK_NEAR(after[i][c], before[i][c], 1e-6);
    }
}

TEST_CASE(probeBlendMonotonic) {
    GN3d::cPROBE_GRID grid;
    CHECK(grid.Init(GN3d::sPROBE_GRID_DESC()));
    float target[9][4];
    fillSh(target, 1.0f);
    CHECK(grid.BlendProbeSh(0, target, 0.5f));
    float out[9][4];
    CHECK(grid.SampleSh(grid.GetOrigin(), out));
    const float first = out[0][0];
    CHECK_NEAR(first, 0.5f, 1e-6);
    CHECK(grid.BlendProbeSh(0, target, 0.5f));
    CHECK(grid.SampleSh(grid.GetOrigin(), out));
    CHECK(out[0][0] > first);
    CHECK_NEAR(out[0][0], 0.75f, 1e-6);
    CHECK(!grid.BlendProbeSh(0, target, std::numeric_limits<float>::quiet_NaN()));
    CHECK(grid.BlendProbeSh(0, target, 1.0f));
    CHECK(grid.SampleSh(grid.GetOrigin(), out));
    CHECK_NEAR(out[0][0], 1.0f, 1e-6);
}

TEST_CASE(probeInvalidInputsAndDeterminism) {
    GN3d::cPROBE_GRID grid;
    float out[9][4];
    CHECK(!grid.SampleSh(GNg::sVEC3{0.0f, 0.0f, 0.0f}, out)); // 未 Init
    CHECK(!grid.Recenter(GNg::sVEC3{10.0f, 0.0f, 0.0f}));
    CHECK(grid.Init(GN3d::sPROBE_GRID_DESC()));

    float sh[9][4];
    fillSh(sh, 0.3f);
    for (uint32_t i = 0; i < grid.GetProbeCount(); ++i) CHECK(grid.SetProbeSh(i, sh));
    CHECK(!grid.SampleSh(GNg::sVEC3{0.0f, 0.0f, 0.0f}, nullptr));
    CHECK(!grid.BlendProbeSh(0, nullptr, 0.5f));

    float a[9][4];
    float b[9][4];
    CHECK(grid.SampleSh(GNg::sVEC3{1.0f, 2.0f, 3.0f}, a));
    CHECK(grid.SampleSh(GNg::sVEC3{1.0f, 2.0f, 3.0f}, b));
    CHECK(std::memcmp(a, b, sizeof(a)) == 0);

    grid.Reset();
    CHECK(!grid.IsValid());
    CHECK_EQ(grid.GetProbeCount(), 0u);
    CHECK(!grid.SampleSh(GNg::sVEC3{0.0f, 0.0f, 0.0f}, out));
}

// ── G5b-1（裁决 A）：9 张 8×4×8 RGBA16F 纹理打包 ────────────────────
TEST_CASE(probePackConstantTextures) {
    GN3d::cPROBE_GRID grid;
    CHECK(grid.Init(GN3d::sPROBE_GRID_DESC()));
    float sh[9][4];
    fillSh(sh, 0.25f);
    for (uint32_t i = 0; i < grid.GetProbeCount(); ++i) CHECK(grid.SetProbeSh(i, sh));

    const size_t total = 9u * 8u * 4u * 8u * 4u; // 9216
    std::vector<float> out(total, -1.0f);
    CHECK(GN3d::packProbeTextures(grid, out.data()));
    // 9 张纹理各抽 4 个 texel 检查常数。
    for (uint32_t k = 0; k < 9u; ++k) {
        for (uint32_t z = 0; z < 8u; z += 3u) {
            for (uint32_t y = 0; y < 4u; y += 3u) {
                for (uint32_t x = 0; x < 8u; x += 3u) {
                    const size_t base = ((((size_t)k * 8u + z) * 4u + y) * 8u + x) * 4u;
                    for (uint32_t c = 0; c < 4u; ++c) CHECK_NEAR(out[base + c], 0.25f, 1e-6);
                }
            }
        }
    }
    CHECK_NEAR(out[0], 0.25f, 1e-6);
    CHECK_NEAR(out[total - 1], 0.25f, 1e-6);
}

TEST_CASE(probePackSingleProbeOnlyChangesItsTexels) {
    GN3d::cPROBE_GRID grid;
    CHECK(grid.Init(GN3d::sPROBE_GRID_DESC()));
    float zero[9][4];
    float one[9][4];
    fillSh(zero, 0.0f);
    fillSh(one, 1.0f);
    for (uint32_t i = 0; i < grid.GetProbeCount(); ++i) CHECK(grid.SetProbeSh(i, zero));
    // 探针 (x=2,y=1,z=3) 的线性索引 = (3*4+1)*8+2 = 106。
    CHECK(grid.SetProbeSh(106, one));

    std::vector<float> out(9u * 8u * 4u * 8u * 4u, 0.0f);
    CHECK(GN3d::packProbeTextures(grid, out.data()));
    for (uint32_t k = 0; k < 9u; ++k) {
        const size_t changed = ((((size_t)k * 8u + 3u) * 4u + 1u) * 8u + 2u) * 4u;
        const size_t other = ((((size_t)k * 8u + 0u) * 4u + 0u) * 8u + 0u) * 4u;
        for (uint32_t c = 0; c < 4u; ++c) CHECK_NEAR(out[changed + c], 1.0f, 1e-6);
        for (uint32_t c = 0; c < 4u; ++c) CHECK_NEAR(out[other + c], 0.0f, 1e-6);
    }
}

TEST_CASE(probePackRoundtripLossless) {
    GN3d::cPROBE_GRID grid;
    CHECK(grid.Init(GN3d::sPROBE_GRID_DESC()));
    const uint32_t count = grid.GetProbeCount();
    for (uint32_t i = 0; i < count; ++i) {
        float sh[9][4];
        for (int k = 0; k < 9; ++k) {
            for (int c = 0; c < 4; ++c) {
                sh[k][c] = static_cast<float>(i * 36u + static_cast<uint32_t>(k) * 4u +
                                              static_cast<uint32_t>(c)) *
                           0.001f;
            }
        }
        CHECK(grid.SetProbeSh(i, sh));
    }
    std::vector<float> out(static_cast<size_t>(count) * 36u, 0.0f);
    CHECK(GN3d::packProbeTextures(grid, out.data()));
    for (uint32_t i = 0; i < count; ++i) {
        const uint32_t x = i % 8u;
        const uint32_t y = (i / 8u) % 4u;
        const uint32_t z = i / 32u;
        float restored[36];
        CHECK(grid.GetProbeSh(i, restored));
        for (uint32_t k = 0; k < 9u; ++k) {
            const size_t base = ((((size_t)k * 8u + z) * 4u + y) * 8u + x) * 4u;
            for (uint32_t c = 0; c < 4u; ++c) {
                CHECK_NEAR(out[base + c], restored[k * 4u + c], 1e-9);
            }
        }
    }
}

TEST_CASE(probePackInvalidInputs) {
    std::vector<float> out(9u * 8u * 4u * 8u * 4u, -123.0f);
    GN3d::cPROBE_GRID grid; // 未 Init
    CHECK(!GN3d::packProbeTextures(grid, out.data()));
    CHECK_NEAR(out[0], -123.0f, 1e-6); // 失败不写

    CHECK(grid.Init(GN3d::sPROBE_GRID_DESC()));
    CHECK(!GN3d::packProbeTextures(grid, nullptr));
    float sh36[36];
    CHECK(!grid.GetProbeSh(grid.GetProbeCount(), sh36));
    CHECK(!grid.GetProbeSh(0, nullptr));
    CHECK(grid.GetProbeSh(0, sh36));
}
