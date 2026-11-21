// brdfLighting.glsl — pbr/deferred 两条光照路径共用的纯数学 GLSL 片段（B5d-S25）
//
// 内容全部是参数化的纯函数（BRDF 三项 + 相位函数 + 臭氧密度），不引用任何 UBO/采样器；
// 调用方必须先定义 PI 再 #include 本文件（两条 frag 都在 #version 后 #define PI）。
// 抽出的目的：消除 gn3d_pbr.frag 与 gn3d_deferred_light.frag 的逐字重复，防止常量漂移。
#ifndef BRDF_LIGHTING_GLSL
#define BRDF_LIGHTING_GLSL

const float OZONE_CENTER_HEIGHT = 25000.0; // 臭氧帐篷中心（m）
const float OZONE_WIDTH = 15000.0;         // 臭氧帐篷半宽（m）

// Rayleigh 相位：(3/16π)(1 + mu²)。
float rayleighPhase(float mu) {
    return (3.0 / (16.0 * PI)) * (1.0 + mu * mu);
}

// Henyey-Greenstein 近似（Mie 相位；与 atmosphereParams.cpp miePhase 同式）。
float miePhase(float mu, float g) {
    float g2 = g * g;
    float denom = max(1.0 + g2 - 2.0 * g * mu, 1e-6);
    return (1.0 - g2) / (4.0 * PI * denom * sqrt(denom));
}

// 臭氧帐篷密度（与 computeTransmittance 的 dO 同式）。
float ozoneDensity(float height) {
    return max(0.0, 1.0 - abs(height - OZONE_CENTER_HEIGHT) / OZONE_WIDTH);
}

float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float denom = NdotH2 * (a2 - 1.0) + 1.0;
    return a2 / (PI * denom * denom);
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float k = (roughness + 1.0) * (roughness + 1.0) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    return GeometrySchlickGGX(max(dot(N, V), 0.0), roughness)
         * GeometrySchlickGGX(max(dot(N, L), 0.0), roughness);
}

vec3 FresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

#endif // BRDF_LIGHTING_GLSL