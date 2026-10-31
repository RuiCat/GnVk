// giParams.cpp — GI CPU 基座：UBO 契约、SSAO 核、SH9、BRDF LUT、G-Buffer 打包。
// 纯 CPU 实现，不依赖 Vulkan/引擎数学类型；契约与 docs/GI_PLAN.md §4.3/§5.2/§5.4 对齐。
#include "giParams.h"

#include <cmath>
#include <cstring>

namespace GN3d {
namespace {

constexpr float PI = 3.14159265358979323846f;
constexpr float TWO_PI = 2.0f * PI;
constexpr float HALF_PI = 0.5f * PI;

// 实球谐基归一化常数（§4.1：SH9 漫反射）。
constexpr float SH_BASIS_C0 = 0.2820947918f;
constexpr float SH_BASIS_C1 = 0.4886025119f;
constexpr float SH_BASIS_C2_0 = 1.0925484306f;
constexpr float SH_BASIS_C2_1 = 0.3153915653f;
constexpr float SH_BASIS_C2_2 = 0.5462742153f;

// 余弦卷积系数 A_l（Ramamoorthi & Hanrahan 2001）。
constexpr float SH_COSINE_A0 = PI;
constexpr float SH_COSINE_A1 = TWO_PI / 3.0f;
constexpr float SH_COSINE_A2 = PI / 4.0f;

float clamp01(float value) {
    if (!std::isfinite(value)) return 0.0f;
    if (value < 0.0f) return 0.0f;
    if (value > 1.0f) return 1.0f;
    return value;
}

float signNotZero(float value) { return value >= 0.0f ? 1.0f : -1.0f; }

bool normalize3(const float in[3], float out[3]) {
    if (!in || !out) return false;
    const float x = in[0];
    const float y = in[1];
    const float z = in[2];
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) return false;
    const float len2 = x * x + y * y + z * z;
    if (!(len2 > 1e-24f)) return false;
    const float invLen = 1.0f / std::sqrt(len2);
    out[0] = x * invLen;
    out[1] = y * invLen;
    out[2] = z * invLen;
    return true;
}

// 实球谐基 Y_lm（标准归一化，方向 y 向上；顺序 0..8 = 00,1-1,10,11,2-2,2-1,20,21,22）。
void shBasis9(const float n[3], float outBasis[SH9_COEFF_COUNT]) {
    const float x = n[0];
    const float y = n[1];
    const float z = n[2];
    outBasis[0] = SH_BASIS_C0;
    outBasis[1] = SH_BASIS_C1 * y;
    outBasis[2] = SH_BASIS_C1 * z;
    outBasis[3] = SH_BASIS_C1 * x;
    outBasis[4] = SH_BASIS_C2_0 * x * y;
    outBasis[5] = SH_BASIS_C2_0 * y * z;
    outBasis[6] = SH_BASIS_C2_1 * (3.0f * z * z - 1.0f);
    outBasis[7] = SH_BASIS_C2_0 * x * z;
    outBasis[8] = SH_BASIS_C2_2 * (x * x - y * y);
}

int shBandOf(int coefficientIndex) {
    if (coefficientIndex == 0) return 0;
    if (coefficientIndex < 4) return 1;
    return 2;
}

// Halton 序列（第 index 项，base 进制），用于 SSAO 核的低差异采样。
float halton(std::uint32_t index, std::uint32_t base) {
    float result = 0.0f;
    float fraction = 1.0f;
    while (index > 0) {
        fraction /= static_cast<float>(base);
        result += fraction * static_cast<float>(index % base);
        index /= base;
    }
    return result;
}

// splitmix64：确定性伪随机源（固定 seed 可复现），只用于切向旋转。
std::uint64_t splitmix64(std::uint64_t* state) {
    std::uint64_t z = (*state += 0x9E3779B97F4A7C15ull);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

float random01(std::uint64_t* state) {
    return static_cast<float>(splitmix64(state) >> 40) * (1.0f / 16777216.0f);
}

float radicalInverseVdC(std::uint32_t bits) {
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return static_cast<float>(bits) * 2.3283064365386963e-10f;
}

void hammersley2d(std::uint32_t index, std::uint32_t count, float outXi[2]) {
    outXi[0] = static_cast<float>(index) / static_cast<float>(count);
    outXi[1] = radicalInverseVdC(index);
}

void importanceSampleGgx(const float xi[2], float roughness, float outH[3]) {
    const float a = roughness * roughness;
    const float a2 = a * a;
    const float phi = TWO_PI * xi[0];
    const float cosTheta = std::sqrt((1.0f - xi[1]) / (1.0f + (a2 - 1.0f) * xi[1]));
    const float sinTheta = std::sqrt(1.0f > cosTheta * cosTheta ? 1.0f - cosTheta * cosTheta : 0.0f);
    outH[0] = sinTheta * std::cos(phi);
    outH[1] = sinTheta * std::sin(phi);
    outH[2] = cosTheta;
}

float geometrySchlickGgx(float ndotV, float roughness) {
    const float k = (roughness * roughness) * 0.5f;
    return ndotV / (ndotV * (1.0f - k) + k);
}

// split-sum 的第二项：GGX 重要性采样积分（Karis 2013 / UE4 实现），64 样本。
void integrateBrdf(float ndotV, float roughness, float outAB[2]) {
    const float vx = std::sqrt(1.0f > ndotV * ndotV ? 1.0f - ndotV * ndotV : 0.0f);
    const float v[3] = {vx, 0.0f, ndotV};
    float accumA = 0.0f;
    float accumB = 0.0f;
    for (int i = 0; i < BRDF_LUT_SAMPLES; ++i) {
        float xi[2];
        hammersley2d(static_cast<std::uint32_t>(i), static_cast<std::uint32_t>(BRDF_LUT_SAMPLES), xi);
        float h[3];
        importanceSampleGgx(xi, roughness, h);
        const float vdotH = v[0] * h[0] + v[2] * h[2];
        const float l[3] = {2.0f * vdotH * h[0] - v[0], 2.0f * vdotH * h[1] - v[1],
                            2.0f * vdotH * h[2] - v[2]};
        const float ndotL = l[2];
        const float ndotH = h[2];
        if (ndotL <= 0.0f || ndotV <= 1e-6f || ndotH <= 1e-6f) continue;
        const float geometry = geometrySchlickGgx(ndotV, roughness) *
                               geometrySchlickGgx(ndotL, roughness);
        const float gVis = geometry * vdotH / (ndotH * ndotV);
        const float fc = std::pow(1.0f - vdotH, 5.0f);
        accumA += (1.0f - fc) * gVis;
        accumB += fc * gVis;
    }
    const float invCount = 1.0f / static_cast<float>(BRDF_LUT_SAMPLES);
    outAB[0] = accumA * invCount;
    outAB[1] = accumB * invCount;
}

std::uint8_t quantizeUnorm8(float value) {
    const float clamped = clamp01(value);
    return static_cast<std::uint8_t>(clamped * 255.0f + 0.5f);
}

bool isFiniteArray(const float* values, int count) {
    for (int i = 0; i < count; ++i) {
        if (!std::isfinite(values[i])) return false;
    }
    return true;
}

void setIdentityMatrix(float outMatrix[16]) {
    for (int i = 0; i < 16; ++i) outMatrix[i] = 0.0f;
    outMatrix[0] = 1.0f;
    outMatrix[5] = 1.0f;
    outMatrix[10] = 1.0f;
    outMatrix[15] = 1.0f;
}

} // namespace

sGI_UBO makeDefaultGiUbo() {
    sGI_UBO ubo = {};
    ubo.indirect[0] = 1.0f;  // 漫反射倍率
    ubo.indirect[1] = 1.0f;  // 镜面倍率
    ubo.indirect[2] = 1.0f;  // AO 强度
    ubo.indirect[3] = 0.0f;  // SSAO 默认关
    ubo.aoParams[0] = 0.5f;  // 半径 0.5 m
    ubo.aoParams[1] = 0.025f; // 偏移
    ubo.aoParams[2] = 1.0f;  // 幂
    ubo.aoParams[3] = 1.0f;  // 模糊半径
    ubo.envParams[0] = 0.0f; // 环境旋转
    ubo.envParams[1] = 5.0f; // 预滤波 mip 数
    ubo.envParams[2] = 0.0f; // 曝光补偿
    ubo.envParams[3] = 0.0f; // 保留
    // 中性常数环境 radiance=1：SH00 = sqrt(4π)，evalSh9 得 irradiance=π。
    ubo.sh[0] = std::sqrt(4.0f * PI);
    ubo.sh[1] = std::sqrt(4.0f * PI);
    ubo.sh[2] = std::sqrt(4.0f * PI);
    setIdentityMatrix(ubo.invViewProj);
    return ubo;
}

bool validateGiUbo(const sGI_UBO& ubo, sGI_UBO* corrected) {
    bool valid = true;
    sGI_UBO fixed = ubo;

    for (int c = 0; c < 3; ++c) {
        if (!std::isfinite(fixed.indirect[c])) {
            valid = false;
            fixed.indirect[c] = 1.0f;
        } else if (fixed.indirect[c] < 0.0f) {
            valid = false;
            fixed.indirect[c] = 0.0f;
        }
    }
    if (!std::isfinite(fixed.indirect[3]) || (fixed.indirect[3] != 0.0f && fixed.indirect[3] != 1.0f)) {
        valid = false;
        fixed.indirect[3] = fixed.indirect[3] > 0.5f ? 1.0f : 0.0f;
    }

    const float aoFallback[4] = {0.5f, 0.025f, 1.0f, 1.0f};
    for (int c = 0; c < 4; ++c) {
        if (!std::isfinite(fixed.aoParams[c])) {
            valid = false;
            fixed.aoParams[c] = aoFallback[c];
        } else if (fixed.aoParams[c] < 0.0f) {
            valid = false;
            fixed.aoParams[c] = 0.0f;
        }
    }
    if (fixed.aoParams[2] <= 0.0f) {
        valid = false;
        fixed.aoParams[2] = aoFallback[2];
    }

    if (!std::isfinite(fixed.envParams[0])) {
        valid = false;
        fixed.envParams[0] = 0.0f;
    }
    if (!std::isfinite(fixed.envParams[1]) || fixed.envParams[1] < 0.0f) {
        valid = false;
        fixed.envParams[1] = 5.0f;
    } else if (fixed.envParams[1] > 16.0f) {
        valid = false;
        fixed.envParams[1] = 16.0f;
    }
    if (!std::isfinite(fixed.envParams[2])) {
        valid = false;
        fixed.envParams[2] = 0.0f;
    }
    if (!std::isfinite(fixed.envParams[3])) {
        valid = false;
        fixed.envParams[3] = 0.0f;
    }

    for (int i = 0; i < SH9_FLOAT_COUNT; ++i) {
        if (!std::isfinite(fixed.sh[i])) {
            valid = false;
            fixed.sh[i] = 0.0f;
        }
    }
    if (!isFiniteArray(fixed.invViewProj, 16)) {
        valid = false;
        setIdentityMatrix(fixed.invViewProj);
    }

    if (corrected) *corrected = fixed;
    return valid;
}

bool packGiUbo(const sGI_UBO& ubo, void* outBytes) {
    if (!outBytes) return false;
    std::memcpy(outBytes, &ubo, sizeof(ubo));
    return true;
}

// G2b：SSAO UBO 默认值与打包（400 B 契约）。
sSSAO_UBO makeDefaultSsaoUbo() {
    sSSAO_UBO ubo = {};
    setIdentityMatrix(ubo.viewProj);
    setIdentityMatrix(ubo.invViewProj);
    generateSsaoKernel(ubo.samples, 0u);
    ubo.params[0] = 0.5f;   // 半径 (m)
    ubo.params[1] = 0.025f; // 偏置
    ubo.params[2] = 1.0f;   // 幂
    ubo.params[3] = 0.0f;   // 启用位：默认关（观感/开销不变）
    return ubo;
}

bool packSsaoUbo(const sSSAO_UBO& ubo, void* outBytes) {
    if (!outBytes) return false;
    std::memcpy(outBytes, &ubo, sizeof(ubo));
    return true;
}

// G4：SSGI UBO 默认值与打包（160 B 契约）。
sSSGI_UBO makeDefaultSsgiUbo() {
    sSSGI_UBO ubo = {};
    setIdentityMatrix(ubo.viewProj);
    setIdentityMatrix(ubo.invViewProj);
    ubo.params[0] = 0.0f;  // 强度：默认关（延迟 shader 逐像素等同现状）
    ubo.params[1] = 8.0f;  // 步数
    ubo.params[2] = 0.5f;  // 厚度
    ubo.params[3] = 20.0f; // 最大距离 (m)
    // screen 由运行时每帧填；默认 0 表示未填。
    return ubo;
}

bool packSsgiUbo(const sSSGI_UBO& ubo, void* outBytes) {
    if (!outBytes) return false;
    std::memcpy(outBytes, &ubo, sizeof(ubo));
    return true;
}

// G5b：探针 UBO 默认值与打包（48 B 契约）。
sPROBE_UBO makeDefaultProbeUbo() {
    sPROBE_UBO ubo = {};
    ubo.origin[0] = 0.0f;
    ubo.origin[1] = 0.0f;
    ubo.origin[2] = 0.0f;
    ubo.spacingCounts[0] = 2.0f; // spacing (m)
    ubo.spacingCounts[1] = 8.0f; // countX
    ubo.spacingCounts[2] = 4.0f; // countY
    ubo.spacingCounts[3] = 8.0f; // countZ
    ubo.weight[0] = 0.0f;        // 默认关
    return ubo;
}

bool packProbeUbo(const sPROBE_UBO& ubo, void* outBytes) {
    if (!outBytes) return false;
    std::memcpy(outBytes, &ubo, sizeof(ubo));
    return true;
}

void generateSsaoKernel(float kernel[SSAO_KERNEL_SIZE][4], std::uint32_t seed) {
    if (!kernel) return;
    std::uint64_t rngState = static_cast<std::uint64_t>(seed) * 0x9E3779B97F4A7C15ull + 1ull;
    const float rotation = TWO_PI * random01(&rngState);
    const float cosRotation = std::cos(rotation);
    const float sinRotation = std::sin(rotation);
    for (int i = 0; i < SSAO_KERNEL_SIZE; ++i) {
        const std::uint32_t index = static_cast<std::uint32_t>(i + 1);
        const float u = halton(index, 2u);
        const float v = halton(index, 3u);
        const float radius = std::sqrt(u);
        const float theta = TWO_PI * v + rotation;
        const float x = radius * std::cos(theta);
        const float y = radius * std::sin(theta);
        const float z = std::sqrt(1.0f > u ? 1.0f - u : 0.0f);
        const float t = static_cast<float>(i + 1) / static_cast<float>(SSAO_KERNEL_SIZE);
        const float length = 0.1f + 0.9f * t * t;
        kernel[i][0] = (x * cosRotation - y * sinRotation) * length;
        kernel[i][1] = (x * sinRotation + y * cosRotation) * length;
        kernel[i][2] = z * length;
        kernel[i][3] = 1.0f;
    }
}

bool shProjectEquirect(const float* envRgb, int width, int height, float sh[SH9_FLOAT_COUNT]) {
    if (!sh) return false;
    for (int i = 0; i < SH9_FLOAT_COUNT; ++i) sh[i] = 0.0f;
    if (!envRgb || width < 2 || height < 2) return false;

    const float deltaTheta = PI / static_cast<float>(height);
    const float deltaPhi = TWO_PI / static_cast<float>(width);
    for (int y = 0; y < height; ++y) {
        const float theta = (static_cast<float>(y) + 0.5f) * deltaTheta;
        const float sinTheta = std::sin(theta);
        const float cosTheta = std::cos(theta);
        const float solidAngle = sinTheta * deltaTheta * deltaPhi;
        for (int x = 0; x < width; ++x) {
            const float phi = (static_cast<float>(x) + 0.5f) * deltaPhi;
            const float direction[3] = {sinTheta * std::cos(phi), cosTheta, sinTheta * std::sin(phi)};
            float basis[SH9_COEFF_COUNT];
            shBasis9(direction, basis);
            const float* src = envRgb + (static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
                                         static_cast<std::size_t>(x)) * 3u;
            if (!std::isfinite(src[0]) || !std::isfinite(src[1]) || !std::isfinite(src[2])) continue;
            for (int lm = 0; lm < SH9_COEFF_COUNT; ++lm) {
                const float weight = basis[lm] * solidAngle;
                sh[lm * 4 + 0] += src[0] * weight;
                sh[lm * 4 + 1] += src[1] * weight;
                sh[lm * 4 + 2] += src[2] * weight;
            }
        }
    }
    return true;
}

void evalSh9(const float sh[SH9_FLOAT_COUNT], const float normal[3], float outIrradiance[3]) {
    if (!outIrradiance) return;
    outIrradiance[0] = 0.0f;
    outIrradiance[1] = 0.0f;
    outIrradiance[2] = 0.0f;
    if (!sh || !normal) return;
    float direction[3];
    if (!normalize3(normal, direction)) return;
    float basis[SH9_COEFF_COUNT];
    shBasis9(direction, basis);
    for (int lm = 0; lm < SH9_COEFF_COUNT; ++lm) {
        const int band = shBandOf(lm);
        const float cosineCoefficient =
            band == 0 ? SH_COSINE_A0 : (band == 1 ? SH_COSINE_A1 : SH_COSINE_A2);
        const float weight = cosineCoefficient * basis[lm];
        for (int c = 0; c < 3; ++c) {
            const float coefficient = sh[lm * 4 + c];
            if (std::isfinite(coefficient)) outIrradiance[c] += weight * coefficient;
        }
    }
}

bool generateBrdfLut(float* rgOut, int width, int height) {
    if (!rgOut || width < 1 || height < 1) return false;
    for (int y = 0; y < height; ++y) {
        const float roughness = (static_cast<float>(y) + 0.5f) / static_cast<float>(height);
        for (int x = 0; x < width; ++x) {
            const float ndotV = (static_cast<float>(x) + 0.5f) / static_cast<float>(width);
            float ab[2];
            integrateBrdf(ndotV > 1e-4f ? ndotV : 1e-4f, roughness, ab);
            float* dst = rgOut + (static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
                                  static_cast<std::size_t>(x)) * 2u;
            dst[0] = ab[0];
            dst[1] = ab[1];
        }
    }
    return true;
}

bool evalBrdfLut(const float* lut, int width, int height, float ndotV, float roughness,
                 float outRG[2]) {
    if (!outRG) return false;
    outRG[0] = 0.0f;
    outRG[1] = 0.0f;
    if (!lut || width < 1 || height < 1) return false;

    const float u = clamp01(std::isfinite(ndotV) ? ndotV : 0.0f);
    const float v = clamp01(std::isfinite(roughness) ? roughness : 0.0f);
    const float pixelX = u * static_cast<float>(width) - 0.5f;
    const float pixelY = v * static_cast<float>(height) - 0.5f;
    const float floorX = std::floor(pixelX);
    const float floorY = std::floor(pixelY);
    const int x0 = static_cast<int>(floorX);
    const int y0 = static_cast<int>(floorY);
    const float tx = pixelX - floorX;
    const float ty = pixelY - floorY;
    const int x1 = x0 + 1 < width ? x0 + 1 : width - 1;
    const int y1 = y0 + 1 < height ? y0 + 1 : height - 1;
    const int cx0 = x0 < 0 ? 0 : (x0 >= width ? width - 1 : x0);
    const int cy0 = y0 < 0 ? 0 : (y0 >= height ? height - 1 : y0);

    const float* p00 = lut + (static_cast<std::size_t>(cy0) * static_cast<std::size_t>(width) +
                              static_cast<std::size_t>(cx0)) * 2u;
    const float* p10 = lut + (static_cast<std::size_t>(cy0) * static_cast<std::size_t>(width) +
                              static_cast<std::size_t>(x1)) * 2u;
    const float* p01 = lut + (static_cast<std::size_t>(y1) * static_cast<std::size_t>(width) +
                              static_cast<std::size_t>(cx0)) * 2u;
    const float* p11 = lut + (static_cast<std::size_t>(y1) * static_cast<std::size_t>(width) +
                              static_cast<std::size_t>(x1)) * 2u;
    for (int c = 0; c < 2; ++c) {
        const float bottom = p00[c] * (1.0f - tx) + p10[c] * tx;
        const float top = p01[c] * (1.0f - tx) + p11[c] * tx;
        outRG[c] = bottom * (1.0f - ty) + top * ty;
    }
    return true;
}

float srgbEncode(float linearValue) {
    if (!std::isfinite(linearValue) || linearValue <= 0.0f) return 0.0f;
    if (linearValue >= 1.0f) return 1.0f;
    if (linearValue <= 0.0031308f) return linearValue * 12.92f;
    return 1.055f * std::pow(linearValue, 1.0f / 2.4f) - 0.055f;
}

float srgbDecode(float encodedValue) {
    if (!std::isfinite(encodedValue) || encodedValue <= 0.0f) return 0.0f;
    if (encodedValue >= 1.0f) return 1.0f;
    if (encodedValue <= 0.04045f) return encodedValue / 12.92f;
    return std::pow((encodedValue + 0.055f) / 1.055f, 2.4f);
}

void packAlbedoRgba8(const float rgb[3], float alpha, std::uint8_t outRgba[4]) {
    if (!outRgba) return;
    outRgba[0] = quantizeUnorm8(srgbEncode(rgb ? rgb[0] : 0.0f));
    outRgba[1] = quantizeUnorm8(srgbEncode(rgb ? rgb[1] : 0.0f));
    outRgba[2] = quantizeUnorm8(srgbEncode(rgb ? rgb[2] : 0.0f));
    outRgba[3] = quantizeUnorm8(alpha);
}

void unpackAlbedoRgba8(const std::uint8_t rgba[4], float outRgb[3], float* outAlpha) {
    if (outRgb) {
        outRgb[0] = srgbDecode(rgba ? static_cast<float>(rgba[0]) / 255.0f : 0.0f);
        outRgb[1] = srgbDecode(rgba ? static_cast<float>(rgba[1]) / 255.0f : 0.0f);
        outRgb[2] = srgbDecode(rgba ? static_cast<float>(rgba[2]) / 255.0f : 0.0f);
    }
    if (outAlpha) *outAlpha = rgba ? static_cast<float>(rgba[3]) / 255.0f : 0.0f;
}

std::uint32_t packGbufferMaterial(float ao, float reflectance, std::uint32_t materialId,
                                  float alpha) {
    const std::uint32_t id = materialId > 255u ? 255u : materialId;
    return static_cast<std::uint32_t>(quantizeUnorm8(ao)) |
           (static_cast<std::uint32_t>(quantizeUnorm8(reflectance)) << 8u) |
           (id << 16u) | (static_cast<std::uint32_t>(quantizeUnorm8(alpha)) << 24u);
}

void unpackGbufferMaterial(std::uint32_t packed, float* outAo, float* outReflectance,
                           std::uint32_t* outMaterialId, float* outAlpha) {
    if (outAo) *outAo = static_cast<float>(packed & 0xFFu) / 255.0f;
    if (outReflectance) *outReflectance = static_cast<float>((packed >> 8u) & 0xFFu) / 255.0f;
    if (outMaterialId) *outMaterialId = (packed >> 16u) & 0xFFu;
    if (outAlpha) *outAlpha = static_cast<float>((packed >> 24u) & 0xFFu) / 255.0f;
}

void octEncodeNormal(const float normal[3], float outOct[2]) {
    if (!outOct) return;
    outOct[0] = 0.5f;
    outOct[1] = 0.5f;
    if (!normal) return;
    const float x = normal[0];
    const float y = normal[1];
    const float z = normal[2];
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) return;
    const float l1 = std::fabs(x) + std::fabs(y) + std::fabs(z);
    if (!(l1 > 1e-12f)) return;
    const float nx = x / l1;
    const float ny = y / l1;
    const float nz = z / l1;
    float ox = nx;
    float oy = ny;
    if (nz < 0.0f) {
        ox = (1.0f - std::fabs(ny)) * signNotZero(nx);
        oy = (1.0f - std::fabs(nx)) * signNotZero(ny);
    }
    outOct[0] = clamp01(ox * 0.5f + 0.5f);
    outOct[1] = clamp01(oy * 0.5f + 0.5f);
}

bool octDecodeNormal(const float oct[2], float outNormal[3]) {
    if (!outNormal) return false;
    outNormal[0] = 0.0f;
    outNormal[1] = 0.0f;
    outNormal[2] = 1.0f;
    if (!oct || !std::isfinite(oct[0]) || !std::isfinite(oct[1])) return false;
    const float fx = oct[0] * 2.0f - 1.0f;
    const float fy = oct[1] * 2.0f - 1.0f;
    const float fz = 1.0f - std::fabs(fx) - std::fabs(fy);
    float nx = fx;
    float ny = fy;
    float nz = fz;
    if (nz < 0.0f) {
        const float oldX = nx;
        nx = (1.0f - std::fabs(ny)) * signNotZero(oldX);
        ny = (1.0f - std::fabs(oldX)) * signNotZero(ny);
    }
    const float length = std::sqrt(nx * nx + ny * ny + nz * nz);
    if (!(length > 1e-12f)) return false;
    outNormal[0] = nx / length;
    outNormal[1] = ny / length;
    outNormal[2] = nz / length;
    return true;
}

void octEncodeSnorm16(const float normal[3], std::int16_t outOct[2]) {
    if (!outOct) return;
    float encoded[2];
    octEncodeNormal(normal, encoded);
    for (int i = 0; i < 2; ++i) {
        const float scaled = clamp01(encoded[i]) * 2.0f - 1.0f;
        float quantized = scaled * 32767.0f;
        quantized = quantized >= 0.0f ? quantized + 0.5f : quantized - 0.5f;
        if (quantized > 32767.0f) quantized = 32767.0f;
        if (quantized < -32767.0f) quantized = -32767.0f;
        outOct[i] = static_cast<std::int16_t>(quantized);
    }
}

bool octDecodeSnorm16(const std::int16_t oct[2], float outNormal[3]) {
    if (!oct) {
        if (outNormal) {
            outNormal[0] = 0.0f;
            outNormal[1] = 0.0f;
            outNormal[2] = 1.0f;
        }
        return false;
    }
    const float encoded[2] = {static_cast<float>(oct[0]) / 32767.0f * 0.5f + 0.5f,
                              static_cast<float>(oct[1]) / 32767.0f * 0.5f + 0.5f};
    return octDecodeNormal(encoded, outNormal);
}

bool alphaTestPasses(float alpha, float cutoff) {
    if (std::isnan(cutoff) || cutoff <= 0.0f) return true;
    if (std::isnan(alpha)) return false;
    return alpha >= cutoff;
}

namespace {

void cross3(const float a[3], const float b[3], float out[3]) {
    out[0] = a[1] * b[2] - a[2] * b[1];
    out[1] = a[2] * b[0] - a[0] * b[2];
    out[2] = a[0] * b[1] - a[1] * b[0];
}

void buildTangentBasis(const float n[3], float outTangent[3], float outBitangent[3]) {
    const float up[3] = {0.0f, 0.0f, 1.0f};
    const float fallback[3] = {1.0f, 0.0f, 0.0f};
    const float* reference = std::fabs(n[2]) > 0.999f ? fallback : up;
    cross3(reference, n, outTangent);
    if (!normalize3(outTangent, outTangent)) {
        outTangent[0] = 1.0f;
        outTangent[1] = 0.0f;
        outTangent[2] = 0.0f;
    }
    cross3(n, outTangent, outBitangent);
}

bool directionToCube(const float direction[3], int* outFace, float* outU, float* outV) {
    if (!direction || !outFace || !outU || !outV) return false;
    const float x = direction[0];
    const float y = direction[1];
    const float z = direction[2];
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) return false;
    const float ax = std::fabs(x);
    const float ay = std::fabs(y);
    const float az = std::fabs(z);
    if (ax >= ay && ax >= az) {
        if (!(ax > 1e-12f)) return false;
        *outFace = x >= 0.0f ? 0 : 1;
        *outU = -z / x;
        *outV = (x >= 0.0f ? -y : y) / x;
    } else if (ay >= az) {
        if (!(ay > 1e-12f)) return false;
        *outFace = y >= 0.0f ? 2 : 3;
        *outU = (y >= 0.0f ? x : -x) / y;
        *outV = z / y;
    } else {
        if (!(az > 1e-12f)) return false;
        *outFace = z >= 0.0f ? 4 : 5;
        *outU = x / z;
        *outV = (z >= 0.0f ? -y : y) / z;
    }
    return true;
}

bool sampleCubemapNearest(const float* envFaces, int faceSize, int channels,
                          const float direction[3], float outRgb[3]) {
    if (!envFaces || !outRgb || faceSize < 1 || (channels != 3 && channels != 4)) return false;
    int face = 0;
    float u = 0.0f;
    float v = 0.0f;
    if (!directionToCube(direction, &face, &u, &v)) return false;
    int px = static_cast<int>(std::floor((u * 0.5f + 0.5f) * static_cast<float>(faceSize)));
    int py = static_cast<int>(std::floor((v * 0.5f + 0.5f) * static_cast<float>(faceSize)));
    px = px < 0 ? 0 : (px >= faceSize ? faceSize - 1 : px);
    py = py < 0 ? 0 : (py >= faceSize ? faceSize - 1 : py);
    const float* texel =
        envFaces + ((static_cast<std::size_t>(face) * faceSize + py) * faceSize + px) * channels;
    outRgb[0] = texel[0];
    outRgb[1] = texel[1];
    outRgb[2] = texel[2];
    return true;
}

// 对方向 n 做一次 GGX 预滤波（N=V=R 近似，64 样本，NdotL 加权）。
void prefilterCubemapDirection(const float* envFaces, int faceSize, int channels,
                               const float n[3], float roughness, float outRgb[3]) {
    float accum[3] = {0.0f, 0.0f, 0.0f};
    float weight = 0.0f;
    float tangent[3];
    float bitangent[3];
    buildTangentBasis(n, tangent, bitangent);
    for (int i = 0; i < CUBEMAP_PREFILTER_SAMPLES; ++i) {
        float xi[2];
        hammersley2d(static_cast<std::uint32_t>(i),
                     static_cast<std::uint32_t>(CUBEMAP_PREFILTER_SAMPLES), xi);
        float localH[3];
        importanceSampleGgx(xi, roughness, localH);
        const float h[3] = {tangent[0] * localH[0] + bitangent[0] * localH[1] + n[0] * localH[2],
                            tangent[1] * localH[0] + bitangent[1] * localH[1] + n[1] * localH[2],
                            tangent[2] * localH[0] + bitangent[2] * localH[1] + n[2] * localH[2]};
        const float vdotH = n[0] * h[0] + n[1] * h[1] + n[2] * h[2];
        const float l[3] = {2.0f * vdotH * h[0] - n[0], 2.0f * vdotH * h[1] - n[1],
                            2.0f * vdotH * h[2] - n[2]};
        const float ndotL = n[0] * l[0] + n[1] * l[1] + n[2] * l[2];
        if (ndotL <= 0.0f) continue;
        float rgb[3];
        if (!sampleCubemapNearest(envFaces, faceSize, channels, l, rgb)) continue;
        accum[0] += rgb[0] * ndotL;
        accum[1] += rgb[1] * ndotL;
        accum[2] += rgb[2] * ndotL;
        weight += ndotL;
    }
    if (weight > 1e-8f) {
        const float invWeight = 1.0f / weight;
        outRgb[0] = accum[0] * invWeight;
        outRgb[1] = accum[1] * invWeight;
        outRgb[2] = accum[2] * invWeight;
    } else if (!sampleCubemapNearest(envFaces, faceSize, channels, n, outRgb)) {
        outRgb[0] = 0.0f;
        outRgb[1] = 0.0f;
        outRgb[2] = 0.0f;
    }
}

} // namespace

int cubemapMaxMipCount(int faceSize) {
    if (faceSize < 1) return 0;
    int levels = 1;
    while (faceSize > 1) {
        faceSize >>= 1;
        ++levels;
    }
    return levels;
}

std::size_t prefilterCubemapFloatCount(int faceSize, int mipCount) {
    const int maxMips = cubemapMaxMipCount(faceSize);
    if (maxMips == 0 || mipCount < 1 || mipCount > maxMips) return 0;
    std::size_t total = 0;
    for (int mip = 0; mip < mipCount; ++mip) {
        int levelSize = faceSize >> mip;
        if (levelSize < 1) levelSize = 1;
        total += 6u * static_cast<std::size_t>(levelSize) * static_cast<std::size_t>(levelSize) * 4u;
    }
    return total;
}

std::size_t prefilterCubemapMipOffset(int faceSize, int mipCount, int mip) {
    const int maxMips = cubemapMaxMipCount(faceSize);
    if (maxMips == 0 || mipCount < 1 || mipCount > maxMips || mip < 0 || mip >= mipCount) return 0;
    std::size_t offset = 0;
    for (int level = 0; level < mip; ++level) {
        int levelSize = faceSize >> level;
        if (levelSize < 1) levelSize = 1;
        offset += 6u * static_cast<std::size_t>(levelSize) * static_cast<std::size_t>(levelSize) * 4u;
    }
    return offset;
}

void cubemapFaceDirection(int face, float u, float v, float outDir[3]) {
    if (!outDir) return;
    float direction[3] = {0.0f, 0.0f, 1.0f};
    switch (face) {
    case 0:
        direction[0] = 1.0f;
        direction[1] = -v;
        direction[2] = -u;
        break;
    case 1:
        direction[0] = -1.0f;
        direction[1] = -v;
        direction[2] = u;
        break;
    case 2:
        direction[0] = u;
        direction[1] = 1.0f;
        direction[2] = v;
        break;
    case 3:
        direction[0] = u;
        direction[1] = -1.0f;
        direction[2] = -v;
        break;
    case 4:
        direction[0] = u;
        direction[1] = -v;
        direction[2] = 1.0f;
        break;
    case 5:
        direction[0] = -u;
        direction[1] = -v;
        direction[2] = -1.0f;
        break;
    default:
        break;
    }
    if (!normalize3(direction, outDir)) {
        outDir[0] = 0.0f;
        outDir[1] = 0.0f;
        outDir[2] = 1.0f;
    }
}

bool prefilterCubemap(const float* envFaces, int faceSize, int inputChannels, int mipCount,
                      float* outRgba) {
    const int maxMips = cubemapMaxMipCount(faceSize);
    if (!envFaces || !outRgba || faceSize < 1 || (inputChannels != 3 && inputChannels != 4) ||
        mipCount < 1 || mipCount > maxMips) {
        return false;
    }
    const std::size_t faceTexels =
        static_cast<std::size_t>(faceSize) * static_cast<std::size_t>(faceSize);
    for (std::size_t face = 0; face < 6u; ++face) {
        for (std::size_t texel = 0; texel < faceTexels; ++texel) {
            const std::size_t srcIndex = (face * faceTexels + texel) * inputChannels;
            const std::size_t dstIndex = (face * faceTexels + texel) * 4u;
            outRgba[dstIndex + 0] = envFaces[srcIndex + 0];
            outRgba[dstIndex + 1] = envFaces[srcIndex + 1];
            outRgba[dstIndex + 2] = envFaces[srcIndex + 2];
            outRgba[dstIndex + 3] = inputChannels == 4 ? envFaces[srcIndex + 3] : 1.0f;
        }
    }
    for (int mip = 1; mip < mipCount; ++mip) {
        int levelSize = faceSize >> mip;
        if (levelSize < 1) levelSize = 1;
        const float roughness = static_cast<float>(mip) / static_cast<float>(mipCount - 1);
        const std::size_t levelOffset = prefilterCubemapMipOffset(faceSize, mipCount, mip);
        const std::size_t levelFaceTexels =
            static_cast<std::size_t>(levelSize) * static_cast<std::size_t>(levelSize);
        for (std::size_t face = 0; face < 6u; ++face) {
            for (int y = 0; y < levelSize; ++y) {
                const float v = 2.0f * (static_cast<float>(y) + 0.5f) /
                                    static_cast<float>(levelSize) - 1.0f;
                for (int x = 0; x < levelSize; ++x) {
                    const float u = 2.0f * (static_cast<float>(x) + 0.5f) /
                                        static_cast<float>(levelSize) - 1.0f;
                    float direction[3];
                    cubemapFaceDirection(static_cast<int>(face), u, v, direction);
                    float rgb[3];
                    prefilterCubemapDirection(envFaces, faceSize, inputChannels, direction,
                                              roughness, rgb);
                    const std::size_t dstIndex =
                        levelOffset + (face * levelFaceTexels +
                                       static_cast<std::size_t>(y) * levelSize + x) * 4u;
                    outRgba[dstIndex + 0] = rgb[0];
                    outRgba[dstIndex + 1] = rgb[1];
                    outRgba[dstIndex + 2] = rgb[2];
                    outRgba[dstIndex + 3] = 1.0f;
                }
            }
        }
    }
    return true;
}

bool shProjectCubemap(const float* envFaces, int faceSize, int inputChannels,
                      float sh[SH9_FLOAT_COUNT]) {
    if (!sh) return false;
    for (int i = 0; i < SH9_FLOAT_COUNT; ++i) sh[i] = 0.0f;
    if (!envFaces || faceSize < 1 || (inputChannels != 3 && inputChannels != 4)) return false;
    const float texelAngle = (2.0f / static_cast<float>(faceSize)) *
                             (2.0f / static_cast<float>(faceSize));
    for (int face = 0; face < 6; ++face) {
        for (int y = 0; y < faceSize; ++y) {
            const float v = 2.0f * (static_cast<float>(y) + 0.5f) /
                                static_cast<float>(faceSize) - 1.0f;
            for (int x = 0; x < faceSize; ++x) {
                const float u = 2.0f * (static_cast<float>(x) + 0.5f) /
                                    static_cast<float>(faceSize) - 1.0f;
                float direction[3];
                cubemapFaceDirection(face, u, v, direction);
                float basis[SH9_COEFF_COUNT];
                shBasis9(direction, basis);
                const float denominator = 1.0f + u * u + v * v;
                const float solidAngle = texelAngle / (denominator * std::sqrt(denominator));
                const float* texel =
                    envFaces + ((static_cast<std::size_t>(face) * faceSize + y) * faceSize + x) *
                                   inputChannels;
                if (!std::isfinite(texel[0]) || !std::isfinite(texel[1]) ||
                    !std::isfinite(texel[2])) {
                    continue;
                }
                for (int lm = 0; lm < SH9_COEFF_COUNT; ++lm) {
                    const float weight = basis[lm] * solidAngle;
                    sh[lm * 4 + 0] += texel[0] * weight;
                    sh[lm * 4 + 1] += texel[1] * weight;
                    sh[lm * 4 + 2] += texel[2] * weight;
                }
            }
        }
    }
    return true;
}

} // namespace GN3d
