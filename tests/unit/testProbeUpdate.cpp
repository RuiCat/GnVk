// testProbeUpdate.cpp — DDGI 异步更新 CPU 基座：范围环绕、budget、temporal alpha、SH9 混合。
#include <GN.h>
#include "GN_3DprobeUpdate.h"
#include "testFramework.h"

#include <cmath>

namespace {
using namespace GN3d;
}

TEST_CASE(probeUpdateSchedulerRangesWrap) {
    sPROBE_UPDATE_SCHEDULER s;
    initProbeUpdateScheduler(&s, 10, 4);
    sPROBE_UPDATE_RANGE r = nextProbeUpdateRange(&s);
    CHECK_EQ(r.aStart, 0u);
    CHECK_EQ(r.aCount, 4u);
    r = nextProbeUpdateRange(&s);
    CHECK_EQ(r.aStart, 4u);
    CHECK_EQ(r.aCount, 4u);
    r = nextProbeUpdateRange(&s);
    CHECK_EQ(r.aStart, 8u);
    CHECK_EQ(r.aCount, 2u); // 到尾部截断
    r = nextProbeUpdateRange(&s);
    CHECK_EQ(r.aStart, 0u); // 环绕
    CHECK_EQ(r.aCount, 4u);
}

TEST_CASE(probeUpdateSchedulerInvalidAndZeroBudget) {
    CHECK(nextProbeUpdateRange(0).aCount == 0u);
    sPROBE_UPDATE_SCHEDULER s;
    initProbeUpdateScheduler(&s, 0, 0);
    CHECK(nextProbeUpdateRange(&s).aCount == 0u);

    initProbeUpdateScheduler(&s, 3, 0); // budget 0 按 1
    sPROBE_UPDATE_RANGE r = nextProbeUpdateRange(&s);
    CHECK_EQ(r.aStart, 0u);
    CHECK_EQ(r.aCount, 1u);
}

TEST_CASE(probeUpdateAlphaHysteresis) {
    CHECK_NEAR(probeUpdateAlpha(0, 0.9f), 1.0f, 1e-6);
    CHECK_NEAR(probeUpdateAlpha(9, 0.9f), 0.1f, 1e-6); // 1/(9+1)
    CHECK_NEAR(probeUpdateAlpha(1000, 0.9f), 0.1f, 1e-6); // floor
    CHECK_NEAR(probeUpdateAlpha(0, 2.0f), 1.0f, 1e-6); // h 钳制 1
    CHECK_NEAR(probeUpdateAlpha(0, -1.0f), 1.0f, 1e-6);
}



TEST_CASE(probeUpdateSchedulerBudgetLargerThanCount) {
    using namespace GN3d;
    sPROBE_UPDATE_SCHEDULER s;
    initProbeUpdateScheduler(&s, 10, 64); // budget > probeCount
    sPROBE_UPDATE_RANGE r = nextProbeUpdateRange(&s);
    CHECK_EQ(r.aStart, 0u);
    CHECK_EQ(r.aCount, 10u);
    r = nextProbeUpdateRange(&s);
    CHECK_EQ(r.aStart, 0u);
    CHECK_EQ(r.aCount, 10u);
}

TEST_CASE(probeUpdateAlphaIsMonotonicInAge) {
    using namespace GN3d;
    // age 越大 alpha 越小（收敛越慢），且不低于 1-hysteresis。
    float prev = probeUpdateAlpha(0, 0.9f);
    for (unsigned int age = 1; age < 32u; ++age) {
        const float cur = probeUpdateAlpha(age, 0.9f);
        CHECK(cur <= prev + 1.0e-6f);
        CHECK(cur >= 0.1f - 1.0e-6f);
        prev = cur;
    }
    // hysteresis=0 时 floorAlpha=1，alpha 恒为 1（不做时间混合）。
    CHECK_NEAR(probeUpdateAlpha(31, 0.0f), 1.0f, 1e-6);
    // hysteresis=0.5 时 age 足够大、alpha 收敛到 0.5 的下限。
    CHECK_NEAR(probeUpdateAlpha(31, 0.5f), 0.5f, 1e-6);
}

TEST_CASE(probeUpdateUboLayout) {
    using namespace GN3d;
    sPROBE_UPDATE_UBO ubo;
    initProbeUpdateUbo(&ubo);
    CHECK_NEAR(ubo.aWeight[1], 4.0f, 1e-6); // depthRes
    CHECK_NEAR(ubo.aState[0], 4.0f, 1e-6);  // spp
    ubo.aOrigin[0] = 0.12f;
    ubo.aSpacingCounts[0] = 0.228f;
    ubo.aWeight[0] = 1.0f;
    ubo.aRelocation[1] = 0.5f;
    ubo.aUpdate[0] = 7.0f;
    ubo.aUpdate[1] = 64.0f;
    ubo.aState[2] = 0.001f;
    float packed[24] = {0.0f};
    CHECK(packProbeUpdateUbo(&ubo, packed));
    CHECK_NEAR(packed[0], 0.12f, 1e-6);   // vec4 0 = origin
    CHECK_NEAR(packed[4], 0.228f, 1e-6);  // vec4 1 = spacingCounts
    CHECK_NEAR(packed[8], 1.0f, 1e-6);    // vec4 2 = weight
    CHECK_NEAR(packed[13], 0.5f, 1e-6);   // vec4 3 = relocation.y
    CHECK_NEAR(packed[16], 7.0f, 1e-6);   // vec4 4 = update.x
    CHECK_NEAR(packed[17], 64.0f, 1e-6);  // vec4 4 = update.y
    CHECK_NEAR(packed[22], 0.001f, 1e-6); // vec4 5 = state.z
    CHECK(!packProbeUpdateUbo(0, packed));
    CHECK(!packProbeUpdateUbo(&ubo, 0));
}

TEST_CASE(probeUpdateStateAgeAndReset) {
    using namespace GN3d;
    CHECK_EQ(advanceProbeAge(0, 10), 1u);
    CHECK_EQ(advanceProbeAge(9, 10), 10u);
    CHECK_EQ(advanceProbeAge(10, 10), 10u);
    CHECK_EQ(advanceProbeAge(0, 0), 1u); // 非法 maxAge 钳到 1
    CHECK_EQ(advanceProbeAge(5, 3), 3u);
    sPROBE_STATE states[3];
    for (int i = 0; i < 3; ++i) {
        states[i].aAge = 7u;
        states[i].aClass = ePROBE_CLASS_INACTIVE;
        states[i].aScoreMilli = 42u;
        states[i].aFlags = 1u;
    }
    resetProbeStates(states, 3);
    for (int i = 0; i < 3; ++i) {
        CHECK_EQ(states[i].aAge, 0u);
        CHECK_EQ(states[i].aClass, static_cast<uint32_t>(ePROBE_CLASS_ACTIVE));
        CHECK_EQ(states[i].aScoreMilli, 0u);
        CHECK_EQ(states[i].aFlags, 0u);
    }
    resetProbeStates(0, 3); // 空指针不崩
}

TEST_CASE(probeUpdateBlendProbeSh) {
    float oldSh[36], sampleSh[36], outSh[36];
    for (int i = 0; i < 36; ++i) {
        oldSh[i] = 1.0f;
        sampleSh[i] = 3.0f;
        outSh[i] = 0.0f;
    }
    blendProbeSh(oldSh, sampleSh, 0.0f, outSh);
    CHECK_NEAR(outSh[0], 1.0f, 1e-6);
    blendProbeSh(oldSh, sampleSh, 1.0f, outSh);
    CHECK_NEAR(outSh[35], 3.0f, 1e-6);
    blendProbeSh(oldSh, sampleSh, 0.25f, outSh);
    CHECK_NEAR(outSh[0], 1.5f, 1e-6);
    blendProbeSh(oldSh, sampleSh, -1.0f, outSh); // 钳制
    CHECK_NEAR(outSh[0], 1.0f, 1e-6);
    blendProbeSh(0, sampleSh, 0.5f, outSh); // 空输入不崩
}
