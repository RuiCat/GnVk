// testGi.cpp — GI CPU 基座单测：UBO 契约/校验、SSAO 核、SH9 能量、BRDF LUT 单调、
// G-Buffer 打包 roundtrip、octahedral 法线、alpha cutoff 与非法输入。
#include "testFramework.h"

#include "giParams.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>

using namespace GN3d;

namespace {

const float TEST_PI = 3.14159265358979323846f;

// 确定性 LCG，避免测试依赖 <random> 实现差异。
std::uint32_t g_rngState = 0x12345678u;

float nextRandom01() {
    g_rngState = g_rngState * 1664525u + 1013904223u;
    return static_cast<float>(g_rngState >> 8) * (1.0f / 16777216.0f);
}

float kernelLength(const float sample[4]) {
    return std::sqrt(sample[0] * sample[0] + sample[1] * sample[1] + sample[2] * sample[2]);
}

void normalizeTestVector(float v[3]) {
    const float length = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if (length > 1e-12f) {
        v[0] /= length;
        v[1] /= length;
        v[2] /= length;
    }
}

// 合成一阶梯度环境：L(dir) = 0.5 + 0.5*dir.y（SH9 可精确表示）。
float gradientEnvironment(const float direction[3]) {
    return 0.5f + 0.5f * direction[1];
}

std::vector<float> makeGradientCubemap(int faceSize, int channels) {
    std::vector<float> env(static_cast<std::size_t>(6 * faceSize * faceSize * channels), 0.0f);
    for (int face = 0; face < 6; ++face) {
        for (int y = 0; y < faceSize; ++y) {
            const float v =
                2.0f * (static_cast<float>(y) + 0.5f) / static_cast<float>(faceSize) - 1.0f;
            for (int x = 0; x < faceSize; ++x) {
                const float u =
                    2.0f * (static_cast<float>(x) + 0.5f) / static_cast<float>(faceSize) - 1.0f;
                float direction[3];
                cubemapFaceDirection(face, u, v, direction);
                const float value = gradientEnvironment(direction);
                const std::size_t index =
                    ((static_cast<std::size_t>(face) * faceSize + y) * faceSize + x) * channels;
                env[index] = value;
                env[index + 1] = value;
                env[index + 2] = value;
                if (channels == 4) env[index + 3] = 0.75f;
            }
        }
    }
    return env;
}

std::vector<float> makeGradientEquirect(int width, int height, int channels) {
    std::vector<float> env(static_cast<std::size_t>(width * height * channels), 0.0f);
    for (int y = 0; y < height; ++y) {
        const float theta =
            (static_cast<float>(y) + 0.5f) * TEST_PI / static_cast<float>(height);
        const float sinTheta = std::sin(theta);
        const float cosTheta = std::cos(theta);
        for (int x = 0; x < width; ++x) {
            const float phi = (static_cast<float>(x) + 0.5f) * 2.0f * TEST_PI /
                              static_cast<float>(width);
            const float direction[3] = {sinTheta * std::cos(phi), cosTheta,
                                        sinTheta * std::sin(phi)};
            const float value = gradientEnvironment(direction);
            const std::size_t index = (static_cast<std::size_t>(y) * width + x) * channels;
            env[index] = value;
            env[index + 1] = value;
            env[index + 2] = value;
            if (channels == 4) env[index + 3] = 1.0f;
        }
    }
    return env;
}

// 输出 mip 的 RGB 亮度方差（6 面全部纹素，double 累加）。
double mipLuminanceVariance(const float* mip, int levelSize, int channels) {
    const std::size_t texelCount =
        static_cast<std::size_t>(6) * static_cast<std::size_t>(levelSize) *
        static_cast<std::size_t>(levelSize);
    double sum = 0.0;
    double sumSquared = 0.0;
    for (std::size_t i = 0; i < texelCount; ++i) {
        const std::size_t index = i * static_cast<std::size_t>(channels);
        const double luminance =
            (static_cast<double>(mip[index]) + static_cast<double>(mip[index + 1]) +
             static_cast<double>(mip[index + 2])) /
            3.0;
        sum += luminance;
        sumSquared += luminance * luminance;
    }
    const double mean = sum / static_cast<double>(texelCount);
    return sumSquared / static_cast<double>(texelCount) - mean * mean;
}

} // namespace

TEST_CASE(giUboLayoutIsFrozen) {
    CHECK_EQ(sizeof(sGI_UBO), static_cast<std::size_t>(256));
    CHECK_EQ(offsetof(sGI_UBO, indirect), static_cast<std::size_t>(0));
    CHECK_EQ(offsetof(sGI_UBO, aoParams), static_cast<std::size_t>(16));
    CHECK_EQ(offsetof(sGI_UBO, envParams), static_cast<std::size_t>(32));
    CHECK_EQ(offsetof(sGI_UBO, sh), static_cast<std::size_t>(48));
    CHECK_EQ(offsetof(sGI_UBO, invViewProj), static_cast<std::size_t>(192));
    CHECK_EQ(sizeof(sGI_UBO::sh), static_cast<std::size_t>(36 * sizeof(float)));
    CHECK_EQ(sizeof(sGI_UBO::invViewProj), static_cast<std::size_t>(16 * sizeof(float)));
}

TEST_CASE(giUboDefaultsAreNeutral) {
    const sGI_UBO ubo = makeDefaultGiUbo();
    CHECK_NEAR(ubo.indirect[0], 1.0f, 1e-6);
    CHECK_NEAR(ubo.indirect[1], 1.0f, 1e-6);
    CHECK_NEAR(ubo.indirect[2], 1.0f, 1e-6);
    CHECK_NEAR(ubo.indirect[3], 0.0f, 1e-6); // SSAO 默认关
    CHECK_NEAR(ubo.aoParams[0], 0.5f, 1e-6);
    CHECK_NEAR(ubo.aoParams[1], 0.025f, 1e-6);
    CHECK_NEAR(ubo.aoParams[2], 1.0f, 1e-6);
    CHECK_NEAR(ubo.aoParams[3], 1.0f, 1e-6);
    CHECK_NEAR(ubo.envParams[1], 5.0f, 1e-6);
    CHECK_NEAR(ubo.sh[0], std::sqrt(4.0f * TEST_PI), 1e-5);
    CHECK_NEAR(ubo.sh[1], std::sqrt(4.0f * TEST_PI), 1e-5);
    CHECK_NEAR(ubo.sh[2], std::sqrt(4.0f * TEST_PI), 1e-5);
    CHECK_NEAR(ubo.sh[3], 0.0f, 1e-6);
    CHECK_NEAR(ubo.invViewProj[0], 1.0f, 1e-6);
    CHECK_NEAR(ubo.invViewProj[5], 1.0f, 1e-6);
    CHECK_NEAR(ubo.invViewProj[10], 1.0f, 1e-6);
    CHECK_NEAR(ubo.invViewProj[15], 1.0f, 1e-6);
    CHECK_NEAR(ubo.invViewProj[1], 0.0f, 1e-6);
    sGI_UBO corrected = {};
    CHECK(validateGiUbo(ubo, &corrected));
}

TEST_CASE(giUboValidationRejectsInvalid) {
    const sGI_UBO base = makeDefaultGiUbo();
    sGI_UBO fixed = {};

    sGI_UBO changed = base;
    changed.indirect[0] = 2.5f;
    changed.indirect[3] = 1.0f;
    CHECK(validateGiUbo(changed, &fixed));
    CHECK_NEAR(fixed.indirect[0], 2.5f, 1e-6);
    CHECK_NEAR(fixed.indirect[3], 1.0f, 1e-6);

    sGI_UBO bad = base;
    bad.indirect[0] = -1.0f;
    CHECK(!validateGiUbo(bad, &fixed));
    CHECK_NEAR(fixed.indirect[0], 0.0f, 1e-6);

    bad = base;
    bad.indirect[1] = std::numeric_limits<float>::quiet_NaN();
    CHECK(!validateGiUbo(bad, &fixed));
    CHECK(std::isfinite(fixed.indirect[1]));

    bad = base;
    bad.indirect[3] = 0.5f;
    CHECK(!validateGiUbo(bad, &fixed));
    CHECK(fixed.indirect[3] == 0.0f || fixed.indirect[3] == 1.0f);

    bad = base;
    bad.aoParams[0] = -2.0f;
    CHECK(!validateGiUbo(bad, &fixed));
    CHECK_NEAR(fixed.aoParams[0], 0.0f, 1e-6);

    bad = base;
    bad.aoParams[2] = 0.0f;
    CHECK(!validateGiUbo(bad, &fixed));
    CHECK(fixed.aoParams[2] > 0.0f);

    bad = base;
    bad.envParams[1] = -3.0f;
    CHECK(!validateGiUbo(bad, &fixed));
    CHECK(fixed.envParams[1] >= 0.0f);

    bad = base;
    bad.envParams[1] = 99.0f;
    CHECK(!validateGiUbo(bad, &fixed));
    CHECK(fixed.envParams[1] <= 16.0f);

    bad = base;
    bad.sh[5] = std::numeric_limits<float>::infinity();
    CHECK(!validateGiUbo(bad, &fixed));
    CHECK(std::isfinite(fixed.sh[5]));

    bad = base;
    bad.invViewProj[3] = std::numeric_limits<float>::quiet_NaN();
    CHECK(!validateGiUbo(bad, &fixed));
    CHECK(std::isfinite(fixed.invViewProj[3]));
    CHECK_NEAR(fixed.invViewProj[0], 1.0f, 1e-6);
}

TEST_CASE(giUboPackRoundtrip) {
    const sGI_UBO ubo = makeDefaultGiUbo();
    unsigned char bytes[256];
    std::memset(bytes, 0xCD, sizeof(bytes));
    CHECK(packGiUbo(ubo, bytes));
    CHECK(std::memcmp(bytes, &ubo, sizeof(ubo)) == 0);
    CHECK(!packGiUbo(ubo, nullptr));
}

TEST_CASE(ssaoKernelIsDeterministic) {
    float kernelA[SSAO_KERNEL_SIZE][4];
    float kernelB[SSAO_KERNEL_SIZE][4];
    float kernelC[SSAO_KERNEL_SIZE][4];
    generateSsaoKernel(kernelA, 12345u);
    generateSsaoKernel(kernelB, 12345u);
    generateSsaoKernel(kernelC, 999u);
    CHECK(std::memcmp(kernelA, kernelB, sizeof(kernelA)) == 0);
    CHECK(std::memcmp(kernelA, kernelC, sizeof(kernelA)) != 0);
}

TEST_CASE(ssaoKernelDistributionIsSane) {
    float kernel[SSAO_KERNEL_SIZE][4];
    generateSsaoKernel(kernel, 7u);
    float previousLength = 0.0f;
    float lengthSum = 0.0f;
    for (int i = 0; i < SSAO_KERNEL_SIZE; ++i) {
        CHECK(std::isfinite(kernel[i][0]));
        CHECK(std::isfinite(kernel[i][1]));
        CHECK(std::isfinite(kernel[i][2]));
        CHECK(kernel[i][2] > 0.0f); // 半球：切空间 z 恒正
        const float length = kernelLength(kernel[i]);
        CHECK(length > 0.099f);
        CHECK(length <= 1.001f);
        CHECK(length >= previousLength - 1e-6f); // 0.1..1 递增分布
        previousLength = length;
        lengthSum += length;
        CHECK_NEAR(kernel[i][3], 1.0f, 1e-6);
    }
    const float meanLength = lengthSum / static_cast<float>(SSAO_KERNEL_SIZE);
    CHECK(meanLength > 0.35f);
    CHECK(meanLength < 0.5f);
}

TEST_CASE(shConstantEnvironmentIsEnergyConserving) {
    const int width = 64;
    const int height = 32;
    std::vector<float> environment(static_cast<std::size_t>(width * height * 3), 0.5f);
    float sh[SH9_FLOAT_COUNT];
    CHECK(shProjectEquirect(environment.data(), width, height, sh));
    CHECK_NEAR(sh[0], 0.5f * std::sqrt(4.0f * TEST_PI), 2e-3);
    for (int lm = 0; lm < SH9_COEFF_COUNT; ++lm) {
        CHECK_NEAR(sh[lm * 4 + 3], 0.0f, 1e-6); // padding 保持 0
    }

    const float normals[6][3] = {{0.0f, 0.0f, 1.0f},  {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f},
                                 {0.0f, 0.0f, -1.0f}, {-1.0f, 0.0f, 0.0f}, {0.577f, 0.577f, 0.577f}};
    for (int i = 0; i < 6; ++i) {
        float irradiance[3];
        evalSh9(sh, normals[i], irradiance);
        for (int c = 0; c < 3; ++c) {
            // 常数 radiance L=0.5 的球面 irradiance = πL。
            CHECK_NEAR(irradiance[c], TEST_PI * 0.5f, 5e-3);
        }
    }
}

TEST_CASE(shDirectionalEnvironmentHasGradient) {
    const int width = 32;
    const int height = 16;
    std::vector<float> environment(static_cast<std::size_t>(width * height * 3), 0.0f);
    for (int y = 0; y < height; ++y) {
        const float cosTheta = std::cos((static_cast<float>(y) + 0.5f) * TEST_PI /
                                        static_cast<float>(height));
        const float value = 0.5f + 0.5f * cosTheta; // 平滑线性梯度，SH9 可精确表示
        for (int x = 0; x < width; ++x) {
            float* texel = environment.data() + (static_cast<std::size_t>(y) * width + x) * 3;
            texel[0] = value;
            texel[1] = value;
            texel[2] = value;
        }
    }
    float sh[SH9_FLOAT_COUNT];
    CHECK(shProjectEquirect(environment.data(), width, height, sh));
    float irradianceUp[3];
    float irradianceDown[3];
    const float up[3] = {0.0f, 1.0f, 0.0f};
    const float down[3] = {0.0f, -1.0f, 0.0f};
    evalSh9(sh, up, irradianceUp);
    evalSh9(sh, down, irradianceDown);
    for (int c = 0; c < 3; ++c) {
        // L(y)=0.5+0.5y 的解析 irradiance：E(+Y)=5π/6，E(-Y)=π/6。
        CHECK_NEAR(irradianceUp[c], 5.0f * TEST_PI / 6.0f, 0.02f);
        CHECK_NEAR(irradianceDown[c], TEST_PI / 6.0f, 0.02f);
    }
}

TEST_CASE(shProjectHandlesInvalidInput) {
    float sh[SH9_FLOAT_COUNT];
    for (int i = 0; i < SH9_FLOAT_COUNT; ++i) sh[i] = 1.0f;
    CHECK(!shProjectEquirect(nullptr, 64, 32, sh));
    for (int i = 0; i < SH9_FLOAT_COUNT; ++i) CHECK_NEAR(sh[i], 0.0f, 1e-6);

    std::vector<float> environment(3 * 3 * 3, 1.0f);
    CHECK(!shProjectEquirect(environment.data(), 1, 1, sh));

    float output[3] = {1.0f, 1.0f, 1.0f};
    evalSh9(nullptr, nullptr, output);
    CHECK_NEAR(output[0], 0.0f, 1e-6);
    CHECK_NEAR(output[1], 0.0f, 1e-6);
    CHECK_NEAR(output[2], 0.0f, 1e-6);

    const float zeroNormal[3] = {0.0f, 0.0f, 0.0f};
    output[0] = 1.0f;
    evalSh9(sh, zeroNormal, output);
    CHECK_NEAR(output[0], 0.0f, 1e-6);

    const float nanNormal[3] = {std::numeric_limits<float>::quiet_NaN(), 0.0f, 1.0f};
    output[0] = 1.0f;
    evalSh9(sh, nanNormal, output);
    CHECK_NEAR(output[0], 0.0f, 1e-6);
}

TEST_CASE(brdfLutHasSaneDomain) {
    const int width = BRDF_LUT_SIZE;
    const int height = BRDF_LUT_SIZE;
    std::vector<float> lut(static_cast<std::size_t>(width * height * 2));
    CHECK(generateBrdfLut(lut.data(), width, height));
    for (std::size_t i = 0; i < lut.size(); ++i) {
        CHECK(std::isfinite(lut[i]));
        CHECK(lut[i] >= 0.0f);
        CHECK(lut[i] <= 1.001f);
    }
    for (std::size_t i = 0; i < lut.size(); i += 2) {
        CHECK(lut[i] + lut[i + 1] <= 1.001f);
    }

    float rg[2];
    CHECK(evalBrdfLut(lut.data(), width, height, 0.99f, 0.01f, rg));
    CHECK(rg[0] > 0.95f); // 近法向、近光滑：scale 接近 1
    CHECK(rg[1] < 0.02f); // bias 接近 0
    CHECK(!generateBrdfLut(nullptr, width, height));
    CHECK(!generateBrdfLut(lut.data(), 0, height));
}

TEST_CASE(brdfLutIsMonotone) {
    const int width = BRDF_LUT_SIZE;
    const int height = BRDF_LUT_SIZE;
    std::vector<float> lut(static_cast<std::size_t>(width * height * 2));
    CHECK(generateBrdfLut(lut.data(), width, height));

    const float ndotVs[3] = {0.3f, 0.5f, 0.8f};
    for (int n = 0; n < 3; ++n) {
        const int column = static_cast<int>(ndotVs[n] * static_cast<float>(width));
        float previousA = lut[static_cast<std::size_t>(column) * 2];
        float previousSum = previousA + lut[static_cast<std::size_t>(column) * 2 + 1];
        for (int y = 1; y < height; ++y) {
            const std::size_t index = (static_cast<std::size_t>(y) * width + column) * 2;
            const float a = lut[index];
            const float sum = a + lut[index + 1];
            // 非掠射角（NdotV≥0.3）：roughness 增大时 scale（A）与 A+B 单调不增。
            // 掠射角（NdotV≈0.1）的 split-sum 曲线本身非单调，不在断言范围内。
            CHECK(a <= previousA + 2e-3f);
            CHECK(sum <= previousSum + 2e-3f);
            previousA = a;
            previousSum = sum;
        }
    }

    // 固定 roughness 时 NdotV 的变化方向：低粗糙度整体上升，高粗糙度整体下降
    // （中段允许局部波动，故只断言端点趋势）。
    const int lowRow = static_cast<int>(0.1f * static_cast<float>(height));
    const float lowFirst = lut[(static_cast<std::size_t>(lowRow) * width) * 2];
    const float lowLast = lut[(static_cast<std::size_t>(lowRow) * width + width - 1) * 2];
    CHECK(lowLast > lowFirst + 0.5f);

    const int highRow = static_cast<int>(0.9f * static_cast<float>(height));
    const float highFirst = lut[(static_cast<std::size_t>(highRow) * width) * 2];
    const float highLast = lut[(static_cast<std::size_t>(highRow) * width + width - 1) * 2];
    CHECK(highLast < highFirst - 0.1f);
}

TEST_CASE(brdfLutSamplingWorks) {
    const int width = 16;
    const int height = 8;
    std::vector<float> lut(static_cast<std::size_t>(width * height * 2));
    CHECK(generateBrdfLut(lut.data(), width, height));
    const int sampleX = 3;
    const int sampleY = 2;
    const float ndotV = (static_cast<float>(sampleX) + 0.5f) / static_cast<float>(width);
    const float roughness = (static_cast<float>(sampleY) + 0.5f) / static_cast<float>(height);
    float rg[2];
    CHECK(evalBrdfLut(lut.data(), width, height, ndotV, roughness, rg));
    const std::size_t texel = (static_cast<std::size_t>(sampleY) * width + sampleX) * 2;
    CHECK_NEAR(rg[0], lut[texel], 1e-6);
    CHECK_NEAR(rg[1], lut[texel + 1], 1e-6);
    CHECK(evalBrdfLut(lut.data(), width, height, -1.0f, 2.0f, rg));
    const std::size_t cornerTexel =
        (static_cast<std::size_t>(height - 1) * width) * 2; // uv 钳制到 (0,1) → 左下角
    CHECK_NEAR(rg[0], lut[cornerTexel], 1e-6);
    CHECK_NEAR(rg[1], lut[cornerTexel + 1], 1e-6);
    CHECK(!evalBrdfLut(nullptr, width, height, 0.5f, 0.5f, rg));
}

TEST_CASE(albedoSrgbRoundtrip) {
    const float linearValues[5] = {0.0f, 0.0031308f, 0.04f, 0.18f, 0.5f};
    for (int i = 0; i < 5; ++i) {
        const float encoded = srgbEncode(linearValues[i]);
        CHECK_NEAR(srgbDecode(encoded), linearValues[i], 1e-5);
    }
    CHECK_NEAR(srgbEncode(0.0f), 0.0f, 1e-6);
    CHECK_NEAR(srgbEncode(1.0f), 1.0f, 1e-6);
    CHECK_NEAR(srgbEncode(-1.0f), 0.0f, 1e-6);
    CHECK_NEAR(srgbEncode(std::numeric_limits<float>::quiet_NaN()), 0.0f, 1e-6);
    CHECK_NEAR(srgbDecode(1.0f), 1.0f, 1e-6);
    CHECK_NEAR(srgbDecode(-1.0f), 0.0f, 1e-6);
    CHECK_NEAR(srgbDecode(std::numeric_limits<float>::quiet_NaN()), 0.0f, 1e-6);

    const float rgb[3] = {0.0f, 0.18f, 0.5f};
    std::uint8_t rgba[4];
    packAlbedoRgba8(rgb, 0.25f, rgba);
    float decoded[3];
    float alpha = 0.0f;
    unpackAlbedoRgba8(rgba, decoded, &alpha);
    for (int c = 0; c < 3; ++c) CHECK_NEAR(decoded[c], rgb[c], 2.5f / 255.0f);
    CHECK_NEAR(alpha, 0.25f, 2.5f / 255.0f);

    const float outOfRange[3] = {2.0f, -1.0f, std::numeric_limits<float>::quiet_NaN()};
    std::uint8_t clamped[4];
    packAlbedoRgba8(outOfRange, std::numeric_limits<float>::quiet_NaN(), clamped);
    CHECK_EQ(clamped[0], static_cast<std::uint8_t>(255));
    CHECK_EQ(clamped[1], static_cast<std::uint8_t>(0));
    CHECK_EQ(clamped[2], static_cast<std::uint8_t>(0));
    CHECK_EQ(clamped[3], static_cast<std::uint8_t>(0));
}

TEST_CASE(gbufferMaterialBitfieldRoundtrip) {
    const std::uint32_t packed = packGbufferMaterial(0.5f, 0.04f, 7u, 0.25f);
    CHECK_EQ(packed & 0xFFu, 128u);          // 0.5*255+0.5
    CHECK_EQ((packed >> 8u) & 0xFFu, 10u);   // 0.04*255
    CHECK_EQ((packed >> 16u) & 0xFFu, 7u);   // 材质 ID
    CHECK_EQ((packed >> 24u) & 0xFFu, 64u);  // 0.25*255

    float ao = 0.0f;
    float reflectance = 0.0f;
    float alpha = 0.0f;
    std::uint32_t materialId = 0u;
    unpackGbufferMaterial(packed, &ao, &reflectance, &materialId, &alpha);
    CHECK_NEAR(ao, 0.5f, 2.5f / 255.0f);
    CHECK_NEAR(reflectance, 0.04f, 2.5f / 255.0f);
    CHECK_EQ(materialId, 7u);
    CHECK_NEAR(alpha, 0.25f, 2.5f / 255.0f);
    unpackGbufferMaterial(packed, nullptr, nullptr, nullptr, nullptr);

    const std::uint32_t clamped = packGbufferMaterial(
        2.0f, -1.0f, 300u, std::numeric_limits<float>::quiet_NaN());
    CHECK_EQ(clamped & 0xFFu, 255u);
    CHECK_EQ((clamped >> 8u) & 0xFFu, 0u);
    CHECK_EQ((clamped >> 16u) & 0xFFu, 255u);
    CHECK_EQ((clamped >> 24u) & 0xFFu, 0u);
}

TEST_CASE(octahedralNormalRoundtrip) {
    const float directions[6][3] = {{0.0f, 0.0f, 1.0f},  {0.0f, 0.0f, -1.0f},
                                    {1.0f, 0.0f, 0.0f},  {-1.0f, 0.0f, 0.0f},
                                    {0.0f, 1.0f, 0.0f},  {0.577f, 0.577f, 0.577f}};
    float maxFloatError = 0.0f;
    float maxSnormError = 0.0f;
    for (int i = 0; i < 6; ++i) {
        float oct[2];
        octEncodeNormal(directions[i], oct);
        CHECK(oct[0] >= 0.0f && oct[0] <= 1.0f);
        CHECK(oct[1] >= 0.0f && oct[1] <= 1.0f);
        float decoded[3];
        CHECK(octDecodeNormal(oct, decoded));
        float normalized[3] = {directions[i][0], directions[i][1], directions[i][2]};
        normalizeTestVector(normalized);
        for (int c = 0; c < 3; ++c) {
            const float error = std::fabs(decoded[c] - normalized[c]);
            if (error > maxFloatError) maxFloatError = error;
        }
    }

    for (int i = 0; i < 200; ++i) {
        float direction[3] = {nextRandom01() * 2.0f - 1.0f, nextRandom01() * 2.0f - 1.0f,
                              nextRandom01() * 2.0f - 1.0f};
        normalizeTestVector(direction);
        float oct[2];
        octEncodeNormal(direction, oct);
        float decoded[3];
        CHECK(octDecodeNormal(oct, decoded));
        for (int c = 0; c < 3; ++c) {
            const float error = std::fabs(decoded[c] - direction[c]);
            if (error > maxFloatError) maxFloatError = error;
        }

        std::int16_t quantized[2];
        octEncodeSnorm16(direction, quantized);
        float decodedQuantized[3];
        CHECK(octDecodeSnorm16(quantized, decodedQuantized));
        for (int c = 0; c < 3; ++c) {
            const float error = std::fabs(decodedQuantized[c] - direction[c]);
            if (error > maxSnormError) maxSnormError = error;
        }
    }
    CHECK(maxFloatError <= 1e-3f);
    CHECK(maxSnormError <= 1e-3f);

    const float zeroNormal[3] = {0.0f, 0.0f, 0.0f};
    float oct[2];
    octEncodeNormal(zeroNormal, oct);
    CHECK_NEAR(oct[0], 0.5f, 1e-6);
    CHECK_NEAR(oct[1], 0.5f, 1e-6);
    float decoded[3];
    CHECK(octDecodeNormal(oct, decoded));
    CHECK_NEAR(decoded[0], 0.0f, 1e-6);
    CHECK_NEAR(decoded[1], 0.0f, 1e-6);
    CHECK_NEAR(decoded[2], 1.0f, 1e-6);

    const float invalid[2] = {std::numeric_limits<float>::quiet_NaN(), 0.0f};
    CHECK(!octDecodeNormal(invalid, decoded));
    CHECK(!octDecodeSnorm16(nullptr, decoded));
}

TEST_CASE(alphaCutoffBoundaries) {
    CHECK(alphaTestPasses(0.0f, 0.0f));    // cutoff=0 不裁剪
    CHECK(alphaTestPasses(0.0f, -1.0f));   // 负 cutoff 不裁剪
    CHECK(!alphaTestPasses(0.0f, 0.5f));
    CHECK(alphaTestPasses(0.5f, 0.5f));    // 边界通过（shader 用 a < cutoff 才 discard）
    CHECK(!alphaTestPasses(0.4999f, 0.5f));
    CHECK(alphaTestPasses(0.5001f, 0.5f));
    CHECK(alphaTestPasses(1.0f, 1.0f));
    CHECK(!alphaTestPasses(0.999f, 1.0f));
    CHECK(alphaTestPasses(0.2f, std::numeric_limits<float>::quiet_NaN()));
    CHECK(!alphaTestPasses(std::numeric_limits<float>::quiet_NaN(), 0.5f));
    CHECK(!alphaTestPasses(0.5f, 2.0f)); // 非法 cutoff > 1：全部裁掉
}

TEST_CASE(cubemapPrefilterConstantEnvIsConstant) {
    const int size = 16;
    const int mips = 4;
    std::vector<float> env(static_cast<std::size_t>(6 * size * size * 3), 0.5f);
    CHECK_EQ(cubemapMaxMipCount(size), 5);

    const std::size_t total = prefilterCubemapFloatCount(size, mips);
    CHECK(total > 0u);
    CHECK_EQ(prefilterCubemapMipOffset(size, mips, 0), static_cast<std::size_t>(0));
    CHECK_EQ(prefilterCubemapMipOffset(size, mips, 1),
             static_cast<std::size_t>(6 * size * size * 4));

    std::vector<float> out(total, -1.0f);
    CHECK(prefilterCubemap(env.data(), size, 3, mips, out.data()));
    for (int mip = 0; mip < mips; ++mip) {
        const int levelSize = size >> mip;
        const std::size_t offset = prefilterCubemapMipOffset(size, mips, mip);
        const std::size_t texelCount =
            static_cast<std::size_t>(6) * static_cast<std::size_t>(levelSize) *
            static_cast<std::size_t>(levelSize);
        for (std::size_t i = 0; i < texelCount; ++i) {
            const std::size_t index = offset + i * 4u;
            // 常数环境：各级 mip 的 RGB 必须仍为常数（GGX 权重归一化无偏）。
            CHECK_NEAR(out[index + 0], 0.5f, 1e-5);
            CHECK_NEAR(out[index + 1], 0.5f, 1e-5);
            CHECK_NEAR(out[index + 2], 0.5f, 1e-5);
            CHECK_NEAR(out[index + 3], 1.0f, 1e-6);
        }
    }
}

TEST_CASE(cubemapPrefilterSmoothsGradient) {
    const int size = 32;
    const int mips = 4;
    const int channels = 3;
    const std::vector<float> env = makeGradientCubemap(size, channels);
    std::vector<float> out(prefilterCubemapFloatCount(size, mips));
    CHECK(prefilterCubemap(env.data(), size, channels, mips, out.data()));

    // mip0 = 输入原图（RGB 逐位拷贝，A=1）。
    for (std::size_t texel = 0; texel < static_cast<std::size_t>(6) * size * size; ++texel) {
        const std::size_t src = texel * channels;
        const std::size_t dst = texel * 4u;
        CHECK_EQ(out[dst + 0], env[src + 0]);
        CHECK_EQ(out[dst + 1], env[src + 1]);
        CHECK_EQ(out[dst + 2], env[src + 2]);
        CHECK_NEAR(out[dst + 3], 1.0f, 1e-6);
    }

    // 方向梯度下，mip 级越高越平滑：方差单调不增，且末级明显低于 mip0。
    const double firstVariance = mipLuminanceVariance(out.data(), size, 4);
    CHECK(firstVariance > 0.01);
    double previousVariance = firstVariance;
    for (int mip = 1; mip < mips; ++mip) {
        const int levelSize = size >> mip;
        const std::size_t offset = prefilterCubemapMipOffset(size, mips, mip);
        const double variance = mipLuminanceVariance(out.data() + offset, levelSize, 4);
        CHECK(variance <= previousVariance + 1e-4);
        previousVariance = variance;
    }
    CHECK(previousVariance < 0.7 * firstVariance);
}

TEST_CASE(cubemapShMatchesEquirectSh) {
    // 常数环境：cubemap SH 求值 = πL。
    const int constantSize = 16;
    std::vector<float> constantEnv(static_cast<std::size_t>(6 * constantSize * constantSize * 3),
                                   0.5f);
    float shConstant[SH9_FLOAT_COUNT];
    CHECK(shProjectCubemap(constantEnv.data(), constantSize, 3, shConstant));
    const float normals[5][3] = {{0.0f, 1.0f, 0.0f},  {0.0f, -1.0f, 0.0f}, {1.0f, 0.0f, 0.0f},
                                 {0.0f, 0.0f, 1.0f},  {0.577f, 0.577f, 0.577f}};
    for (int i = 0; i < 5; ++i) {
        float irradiance[3];
        evalSh9(shConstant, normals[i], irradiance);
        for (int c = 0; c < 3; ++c) CHECK_NEAR(irradiance[c], TEST_PI * 0.5f, 0.01f);
    }

    // 同一梯度环境：cubemap SH 与 equirect SH 求值一致，且符合解析值。
    const int equirectWidth = 64;
    const int equirectHeight = 32;
    const std::vector<float> equirectEnv = makeGradientEquirect(equirectWidth, equirectHeight, 3);
    const std::vector<float> cubemapEnv = makeGradientCubemap(32, 3);
    float shEquirect[SH9_FLOAT_COUNT];
    float shCubemap[SH9_FLOAT_COUNT];
    CHECK(shProjectEquirect(equirectEnv.data(), equirectWidth, equirectHeight, shEquirect));
    CHECK(shProjectCubemap(cubemapEnv.data(), 32, 3, shCubemap));
    for (int lm = 0; lm < SH9_COEFF_COUNT; ++lm) {
        CHECK_NEAR(shCubemap[lm * 4 + 3], 0.0f, 1e-6);
    }
    for (int i = 0; i < 5; ++i) {
        float irradianceEquirect[3];
        float irradianceCubemap[3];
        evalSh9(shEquirect, normals[i], irradianceEquirect);
        evalSh9(shCubemap, normals[i], irradianceCubemap);
        for (int c = 0; c < 3; ++c) {
            CHECK_NEAR(irradianceCubemap[c], irradianceEquirect[c], 0.01f);
        }
    }
    float irradianceUp[3];
    float irradianceDown[3];
    const float up[3] = {0.0f, 1.0f, 0.0f};
    const float down[3] = {0.0f, -1.0f, 0.0f};
    evalSh9(shCubemap, up, irradianceUp);
    evalSh9(shCubemap, down, irradianceDown);
    for (int c = 0; c < 3; ++c) {
        CHECK_NEAR(irradianceUp[c], 5.0f * TEST_PI / 6.0f, 0.02f);
        CHECK_NEAR(irradianceDown[c], TEST_PI / 6.0f, 0.02f);
    }
}

TEST_CASE(cubemapPrefilterIsDeterministic) {
    const int size = 16;
    const int mips = 3;
    const int channels = 4; // 覆盖 RGBA 输入
    const std::vector<float> env = makeGradientCubemap(size, channels);
    const std::size_t total = prefilterCubemapFloatCount(size, mips);
    std::vector<float> outputA(total);
    std::vector<float> outputB(total);
    CHECK(prefilterCubemap(env.data(), size, channels, mips, outputA.data()));
    CHECK(prefilterCubemap(env.data(), size, channels, mips, outputB.data()));
    CHECK(std::memcmp(outputA.data(), outputB.data(), total * sizeof(float)) == 0);

    // mip0 保留输入 alpha，mip>0 的 A 固定为 1。
    const std::size_t mip0Texels = static_cast<std::size_t>(6) * size * size;
    for (std::size_t i = 0; i < mip0Texels; ++i) {
        CHECK_NEAR(outputA[i * 4u + 3], 0.75f, 1e-6);
    }
    const std::size_t mip1Offset = prefilterCubemapMipOffset(size, mips, 1);
    const std::size_t mip1Texels = static_cast<std::size_t>(6) * (size / 2) * (size / 2);
    for (std::size_t i = 0; i < mip1Texels; ++i) {
        CHECK_NEAR(outputA[mip1Offset + i * 4u + 3], 1.0f, 1e-6);
    }

    float shA[SH9_FLOAT_COUNT];
    float shB[SH9_FLOAT_COUNT];
    CHECK(shProjectCubemap(env.data(), size, channels, shA));
    CHECK(shProjectCubemap(env.data(), size, channels, shB));
    CHECK(std::memcmp(shA, shB, sizeof(shA)) == 0);
}

TEST_CASE(cubemapPrefilterRejectsInvalidInput) {
    CHECK_EQ(cubemapMaxMipCount(0), 0);
    CHECK_EQ(cubemapMaxMipCount(1), 1);
    CHECK_EQ(cubemapMaxMipCount(32), 6);
    CHECK_EQ(prefilterCubemapFloatCount(0, 1), static_cast<std::size_t>(0));
    CHECK_EQ(prefilterCubemapFloatCount(32, 0), static_cast<std::size_t>(0));
    CHECK_EQ(prefilterCubemapFloatCount(32, 7), static_cast<std::size_t>(0));

    const int size = 8;
    const int mips = 2;
    const int channels = 3;
    std::vector<float> env(static_cast<std::size_t>(6 * size * size * channels), 0.5f);
    const std::size_t total = prefilterCubemapFloatCount(size, mips);
    std::vector<float> guard(total, 12345.0f);
    const std::vector<float> expected = guard;

    CHECK(!prefilterCubemap(nullptr, size, channels, mips, guard.data()));
    CHECK(!prefilterCubemap(env.data(), 0, channels, mips, guard.data()));
    CHECK(!prefilterCubemap(env.data(), size, 2, mips, guard.data()));
    CHECK(!prefilterCubemap(env.data(), size, 5, mips, guard.data()));
    CHECK(!prefilterCubemap(env.data(), size, channels, 0, guard.data()));
    CHECK(!prefilterCubemap(env.data(), size, channels, cubemapMaxMipCount(size) + 1,
                            guard.data()));
    CHECK(!prefilterCubemap(env.data(), size, channels, mips, nullptr));
    CHECK(std::memcmp(guard.data(), expected.data(), total * sizeof(float)) == 0);

    float sh[SH9_FLOAT_COUNT];
    for (int i = 0; i < SH9_FLOAT_COUNT; ++i) sh[i] = 1.0f;
    CHECK(!shProjectCubemap(nullptr, size, channels, sh));
    for (int i = 0; i < SH9_FLOAT_COUNT; ++i) CHECK_NEAR(sh[i], 0.0f, 1e-6);

    for (int i = 0; i < SH9_FLOAT_COUNT; ++i) sh[i] = 1.0f;
    CHECK(!shProjectCubemap(env.data(), 0, channels, sh));
    for (int i = 0; i < SH9_FLOAT_COUNT; ++i) CHECK_NEAR(sh[i], 0.0f, 1e-6);

    for (int i = 0; i < SH9_FLOAT_COUNT; ++i) sh[i] = 1.0f;
    CHECK(!shProjectCubemap(env.data(), size, 2, sh));
    for (int i = 0; i < SH9_FLOAT_COUNT; ++i) CHECK_NEAR(sh[i], 0.0f, 1e-6);
}

// ── G2b：SSAO UBO 契约（400 B，偏移冻结）──────────────────────────────
TEST_CASE(ssaoUboSizeAndLayout) {
    CHECK_EQ(sizeof(sSSAO_UBO), static_cast<std::size_t>(400));
    CHECK_EQ(offsetof(sSSAO_UBO, viewProj), static_cast<std::size_t>(0));
    CHECK_EQ(offsetof(sSSAO_UBO, invViewProj), static_cast<std::size_t>(64));
    CHECK_EQ(offsetof(sSSAO_UBO, samples), static_cast<std::size_t>(128));
    CHECK_EQ(offsetof(sSSAO_UBO, params), static_cast<std::size_t>(384));
}

TEST_CASE(ssaoUboDefaults) {
    const sSSAO_UBO ubo = makeDefaultSsaoUbo();
    for (int i = 0; i < 16; ++i) {
        CHECK_NEAR(ubo.viewProj[i], (i % 5 == 0) ? 1.0f : 0.0f, 1e-6);
        CHECK_NEAR(ubo.invViewProj[i], (i % 5 == 0) ? 1.0f : 0.0f, 1e-6);
    }
    CHECK_NEAR(ubo.params[0], 0.5f, 1e-6);
    CHECK_NEAR(ubo.params[1], 0.025f, 1e-6);
    CHECK_NEAR(ubo.params[2], 1.0f, 1e-6);
    CHECK_NEAR(ubo.params[3], 0.0f, 1e-6); // 默认关
}

TEST_CASE(ssaoUboKernelIsDeterministic) {
    const sSSAO_UBO a = makeDefaultSsaoUbo();
    const sSSAO_UBO b = makeDefaultSsaoUbo();
    CHECK(std::memcmp(&a, &b, sizeof(a)) == 0);
    for (int i = 0; i < SSAO_KERNEL_SIZE; ++i) {
        const float length = std::sqrt(a.samples[i][0] * a.samples[i][0] +
                                       a.samples[i][1] * a.samples[i][1] +
                                       a.samples[i][2] * a.samples[i][2]);
        CHECK(length >= 0.1f - 1e-5f);
        CHECK(length <= 1.0f + 1e-5f);
        CHECK(a.samples[i][2] >= 0.0f); // 半球朝 +z（shader 侧再旋转到法线半球）
        CHECK_NEAR(a.samples[i][3], 1.0f, 1e-6);
    }
}

TEST_CASE(ssaoUboPackRoundtrip) {
    const sSSAO_UBO ubo = makeDefaultSsaoUbo();
    unsigned char bytes[400] = {};
    CHECK(packSsaoUbo(ubo, bytes));
    CHECK(std::memcmp(bytes, &ubo, sizeof(ubo)) == 0);
}

TEST_CASE(ssaoUboPackRejectsNull) {
    const sSSAO_UBO ubo = makeDefaultSsaoUbo();
    CHECK(!packSsaoUbo(ubo, nullptr));
}

TEST_CASE(ssaoUboDefaultIsDisabled) {
    const sSSAO_UBO ubo = makeDefaultSsaoUbo();
    CHECK_NEAR(ubo.params[3], 0.0f, 1e-6);
}

// ── G4：SSGI UBO 契约（160 B）────────────────────────────────────────
TEST_CASE(ssgiUboSizeAndLayout) {
    CHECK_EQ(sizeof(sSSGI_UBO), static_cast<std::size_t>(160));
    CHECK_EQ(offsetof(sSSGI_UBO, viewProj), static_cast<std::size_t>(0));
    CHECK_EQ(offsetof(sSSGI_UBO, invViewProj), static_cast<std::size_t>(64));
    CHECK_EQ(offsetof(sSSGI_UBO, params), static_cast<std::size_t>(128));
    CHECK_EQ(offsetof(sSSGI_UBO, screen), static_cast<std::size_t>(144));
}

TEST_CASE(ssgiUboDefaults) {
    const sSSGI_UBO ubo = makeDefaultSsgiUbo();
    for (int i = 0; i < 16; ++i) {
        CHECK_NEAR(ubo.viewProj[i], (i % 5 == 0) ? 1.0f : 0.0f, 1e-6);
        CHECK_NEAR(ubo.invViewProj[i], (i % 5 == 0) ? 1.0f : 0.0f, 1e-6);
    }
    CHECK_NEAR(ubo.params[0], 0.0f, 1e-6);  // 强度：默认关
    CHECK_NEAR(ubo.params[1], 8.0f, 1e-6);  // 步数
    CHECK_NEAR(ubo.params[2], 0.5f, 1e-6);  // 厚度
    CHECK_NEAR(ubo.params[3], 20.0f, 1e-6); // 最大距离
    for (int i = 0; i < 4; ++i) CHECK_NEAR(ubo.screen[i], 0.0f, 1e-6);
}

TEST_CASE(ssgiUboPackRoundtrip) {
    const sSSGI_UBO ubo = makeDefaultSsgiUbo();
    unsigned char bytes[160] = {};
    CHECK(packSsgiUbo(ubo, bytes));
    CHECK(std::memcmp(bytes, &ubo, sizeof(ubo)) == 0);
}

TEST_CASE(ssgiUboPackRejectsNull) {
    const sSSGI_UBO ubo = makeDefaultSsgiUbo();
    CHECK(!packSsgiUbo(ubo, nullptr));
}

TEST_CASE(ssgiUboDefaultIsDisabled) {
    const sSSGI_UBO ubo = makeDefaultSsgiUbo();
    CHECK_NEAR(ubo.params[0], 0.0f, 1e-6);
}

// ── G5b：探针 UBO 契约（48 B）────────────────────────────────────────
TEST_CASE(probeUboSizeAndLayout) {
    CHECK_EQ(sizeof(sPROBE_UBO), static_cast<std::size_t>(48));
    CHECK_EQ(offsetof(sPROBE_UBO, origin), static_cast<std::size_t>(0));
    CHECK_EQ(offsetof(sPROBE_UBO, spacingCounts), static_cast<std::size_t>(16));
    CHECK_EQ(offsetof(sPROBE_UBO, weight), static_cast<std::size_t>(32));
}

TEST_CASE(probeUboDefaults) {
    const sPROBE_UBO ubo = makeDefaultProbeUbo();
    CHECK_NEAR(ubo.origin[0], 0.0f, 1e-6);
    CHECK_NEAR(ubo.origin[1], 0.0f, 1e-6);
    CHECK_NEAR(ubo.origin[2], 0.0f, 1e-6);
    CHECK_NEAR(ubo.spacingCounts[0], 2.0f, 1e-6);
    CHECK_NEAR(ubo.spacingCounts[1], 8.0f, 1e-6);
    CHECK_NEAR(ubo.spacingCounts[2], 4.0f, 1e-6);
    CHECK_NEAR(ubo.spacingCounts[3], 8.0f, 1e-6);
    CHECK_NEAR(ubo.weight[0], 0.0f, 1e-6);
}

TEST_CASE(probeUboPackRoundtrip) {
    sPROBE_UBO ubo = makeDefaultProbeUbo();
    ubo.weight[0] = 0.75f;
    unsigned char bytes[48] = {};
    CHECK(packProbeUbo(ubo, bytes));
    CHECK(std::memcmp(bytes, &ubo, sizeof(ubo)) == 0);
}

TEST_CASE(probeUboPackRejectsNull) {
    const sPROBE_UBO ubo = makeDefaultProbeUbo();
    CHECK(!packProbeUbo(ubo, nullptr));
}

TEST_CASE(probeUboDefaultIsDisabled) {
    const sPROBE_UBO ubo = makeDefaultProbeUbo();
    CHECK_NEAR(ubo.weight[0], 0.0f, 1e-6);
}

