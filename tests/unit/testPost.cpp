// testPost.cpp — L1 后处理 CPU 契约单测：POST UBO 默认值/校验/打包、
// 曝光 EV 换算、Reinhard/ACES 参考曲线（与 gn3d_composite.frag 同式）。
#include "testFramework.h"

#include "postParams.h"

#include <cmath>
#include <cstring>
#include <limits>

using namespace GN3d;

namespace {

const float kEps = 1.0e-5f;

bool nearly(float a, float b) {
    return std::fabs(a - b) <= kEps;
}

} // namespace

TEST_CASE(postDefaultValues) {
    const sPOST_UBO ubo = makeDefaultPostUbo();
    CHECK_EQ(ubo.tonemapMode, (int32_t)eTONEMAP_ACES);
    CHECK(nearly(ubo.exposure, 1.0f));
    CHECK(nearly(ubo.bloomIntensity, 0.15f));
    CHECK(nearly(ubo.bloomThreshold, 1.0f));
    CHECK(nearly(ubo.bloomKnee, 0.5f));
    CHECK(nearly(ubo.ev, 0.0f));
    CHECK(nearly(ubo.fxaa, 0.0f));
    CHECK(nearly(ubo.pad1, 0.0f));
    CHECK_EQ(sizeof(sPOST_UBO), (size_t)32);
}

TEST_CASE(postLegacyValues) {
    // 回归档：线性直通 + 关 Bloom，等价于改造前的 LDR 输出。
    const sPOST_UBO ubo = makeLegacyPostUbo();
    CHECK_EQ(ubo.tonemapMode, (int32_t)eTONEMAP_NONE);
    CHECK(nearly(ubo.exposure, 1.0f));
    CHECK(nearly(ubo.bloomIntensity, 0.0f));
    CHECK(validatePostUbo(ubo));
}

TEST_CASE(postValidateClamps) {
    sPOST_UBO ubo = makeDefaultPostUbo();
    sPOST_UBO fixed;
    CHECK(validatePostUbo(ubo, &fixed));

    ubo.exposure = std::numeric_limits<float>::quiet_NaN();
    CHECK(!validatePostUbo(ubo, &fixed));
    CHECK(nearly(fixed.exposure, 1.0f));

    ubo = makeDefaultPostUbo();
    ubo.exposure = 100.0f;
    CHECK(!validatePostUbo(ubo, &fixed));
    CHECK(nearly(fixed.exposure, 64.0f));

    ubo = makeDefaultPostUbo();
    ubo.bloomIntensity = -2.0f;
    CHECK(!validatePostUbo(ubo, &fixed));
    CHECK(nearly(fixed.bloomIntensity, 0.0f));

    ubo = makeDefaultPostUbo();
    ubo.tonemapMode = 99;
    CHECK(!validatePostUbo(ubo, &fixed));
    CHECK_EQ(fixed.tonemapMode, (int32_t)eTONEMAP_ACES);

    ubo = makeDefaultPostUbo();
    ubo.ev = -100.0f;
    CHECK(!validatePostUbo(ubo, &fixed));
    CHECK(nearly(fixed.ev, -10.0f));

    ubo = makeDefaultPostUbo();
    ubo.fxaa = 2.0f;
    CHECK(!validatePostUbo(ubo, &fixed));
    CHECK(nearly(fixed.fxaa, 1.0f));
}

TEST_CASE(postExposureScale) {
    CHECK(nearly(exposureScaleFromEv(0.0f), 1.0f));
    CHECK(nearly(exposureScaleFromEv(1.0f), 2.0f));
    CHECK(nearly(exposureScaleFromEv(-1.0f), 0.5f));
    CHECK(nearly(exposureScaleFromEv(10.0f), 1024.0f));
    CHECK(nearly(exposureScaleFromEv(100.0f), 1024.0f));   // EV 钳制
    CHECK(nearly(exposureScaleFromEv(-100.0f), 1.0f / 1024.0f));
}

TEST_CASE(postReinhardCurve) {
    CHECK(nearly(tonemapReinhard(0.0f), 0.0f));
    CHECK(nearly(tonemapReinhard(1.0f), 0.5f));
    CHECK(nearly(tonemapReinhard(9.0f), 0.9f));
    CHECK(nearly(tonemapReinhard(-3.0f), 0.0f)); // 负值按 0
    float previous = -1.0f;
    for(int i = 0; i <= 20; ++i) {
        const float value = tonemapReinhard((float)i * 0.5f);
        CHECK(value >= previous);
        CHECK(value >= 0.0f && value < 1.0f);
        previous = value;
    }
}

TEST_CASE(postAcesCurve) {
    // Narkowicz fit：f(1) = 2.54 / 3.16，f(10) 已接近 1 并被 clamp。
    CHECK(nearly(tonemapAces(0.0f), 0.0f));
    CHECK(nearly(tonemapAces(1.0f), 2.54f / 3.16f));
    CHECK(nearly(tonemapAces(10.0f), 1.0f));
    CHECK(nearly(tonemapAces(-1.0f), 0.0f));
    float previous = -1.0f;
    for(int i = 0; i <= 20; ++i) {
        const float value = tonemapAces((float)i * 0.5f);
        CHECK(value >= previous);
        CHECK(value >= 0.0f && value <= 1.0f);
        previous = value;
    }
}

TEST_CASE(postChannelModes) {
    // 曝光先乘、再套曲线。
    CHECK(nearly(tonemapChannel(eTONEMAP_NONE, 0.5f, 2.0f), 1.0f));
    CHECK(nearly(tonemapChannel(eTONEMAP_NONE, 2.0f, 1.0f), 1.0f));       // clamp
    CHECK(nearly(tonemapChannel(eTONEMAP_REINHARD, 1.0f, 1.0f), 0.5f));
    CHECK(nearly(tonemapChannel(eTONEMAP_REINHARD, 0.5f, 2.0f), 0.5f));  // 曝光 2× 后同值
    CHECK(nearly(tonemapChannel(eTONEMAP_ACES, 1.0f, 1.0f), 2.54f / 3.16f));
    // 非法 mode 按 NONE（clamp）处理。
    CHECK(nearly(tonemapChannel(99, 0.25f, 1.0f), 0.25f));
    CHECK(nearly(tonemapChannel(eTONEMAP_ACES, 1.0f, 0.0f), 0.0f));      // 曝光 0 → 黑
}

TEST_CASE(postPackRoundtrip) {
    sPOST_UBO ubo = makeDefaultPostUbo();
    ubo.exposure = 1.5f;
    ubo.tonemapMode = eTONEMAP_REINHARD;
    unsigned char bytes[sizeof(sPOST_UBO)];
    std::memset(bytes, 0xAA, sizeof(bytes));
    CHECK(packPostUbo(ubo, bytes));
    CHECK(std::memcmp(bytes, &ubo, sizeof(ubo)) == 0);
    CHECK(!packPostUbo(ubo, nullptr));
}
