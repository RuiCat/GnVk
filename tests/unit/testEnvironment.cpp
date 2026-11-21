// testEnvironment.cpp — 环境光照预设的 CPU 测试：室内均匀 SH9、参数钳制、SKY 不覆盖烘焙。
#include <GN.h>
#include "GN_Environment.h"
#include "testFramework.h"

#include <cmath>
#include <cstring>

namespace {

bool nearlyEqual(float a, float b) { return std::fabs(a - b) <= 1e-5f; }

} // namespace

TEST_CASE(environmentIndoorUniformSh9) {
    const float ambient = 0.35f;
    GN3d::sGI_UBO ubo;
    CHECK(GN3d::buildEnvironmentGi(GN3d::eENV_LIGHTING_INDOOR_UNIFORM, ambient, &ubo));
    const float constant = ambient * std::sqrt(4.0f * 3.14159265358979323846f);
    CHECK(nearlyEqual(ubo.sh[0], constant));
    CHECK(nearlyEqual(ubo.sh[1], constant * 0.97f));
    CHECK(nearlyEqual(ubo.sh[2], constant * 0.92f));
    CHECK(nearlyEqual(ubo.sh[3], 0.0f));
    for(int index = 4; index < GN3d::SH9_FLOAT_COUNT; ++index) CHECK(nearlyEqual(ubo.sh[index], 0.0f));
    CHECK(nearlyEqual(ubo.indirect[0], 1.0f));
    CHECK(nearlyEqual(ubo.indirect[1], 0.25f));
    CHECK(nearlyEqual(ubo.indirect[2], 1.0f));
    CHECK(nearlyEqual(ubo.indirect[3], 0.0f));
}

TEST_CASE(environmentIndoorScaleClamps) {
    GN3d::sGI_UBO ubo;
    CHECK(GN3d::buildEnvironmentGi(GN3d::eENV_LIGHTING_INDOOR_UNIFORM, -1.0f, &ubo));
    CHECK(nearlyEqual(ubo.sh[0], 0.0f));
    CHECK(GN3d::buildEnvironmentGi(GN3d::eENV_LIGHTING_INDOOR_UNIFORM, 1.0e9f, &ubo));
    const float clamped = 4.0f * std::sqrt(4.0f * 3.14159265358979323846f);
    CHECK(nearlyEqual(ubo.sh[0], clamped));
    float outScale = -1.0f;
    CHECK(GN3d::validateEnvironmentLighting(GN3d::eENV_LIGHTING_INDOOR_UNIFORM, 1.0e9f, &outScale, 0, 0));
    CHECK(nearlyEqual(outScale, 4.0f));
}

TEST_CASE(environmentSkyKeepsBaked) {
    GN3d::sGI_UBO ubo;
    CHECK(!GN3d::buildEnvironmentGi(GN3d::eENV_LIGHTING_SKY_IBL, 0.0f, &ubo));
    float outScale = 7.0f;
    CHECK(GN3d::validateEnvironmentLighting(GN3d::eENV_LIGHTING_SKY_IBL, 0.0f, &outScale, 0, 0));
    CHECK(nearlyEqual(outScale, 0.0f));
}

TEST_CASE(environmentRejectsUnknownMode) {
    char reason[128];
    GN3d::sGI_UBO ubo;
    CHECK(!GN3d::buildEnvironmentGi((GN3d::eENV_LIGHTING)7, 1.0f, &ubo));
    CHECK(!GN3d::validateEnvironmentLighting((GN3d::eENV_LIGHTING)7, 1.0f, 0, reason, sizeof(reason)));
    CHECK(reason[0] != '\0');
    CHECK(!GN3d::buildEnvironmentGi(GN3d::eENV_LIGHTING_INDOOR_UNIFORM, 1.0f, 0));
    float nanScale = std::nanf("");
    float outScale = -1.0f;
    CHECK(GN3d::validateEnvironmentLighting(GN3d::eENV_LIGHTING_INDOOR_UNIFORM, nanScale, &outScale, 0, 0));
    CHECK(nearlyEqual(outScale, 0.0f));
}
