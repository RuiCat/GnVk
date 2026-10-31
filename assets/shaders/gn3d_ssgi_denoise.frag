#version 450
// ╔══════════════════════════════════════════════════════════╗
// ║  GN_mini — SSGI 单帧 5×5 双边去噪（半分辨率，G4）        ║
// ╚══════════════════════════════════════════════════════════╝

layout(location = 0) in vec2 fragUV;
layout(location = 0) out vec4 outColor;

layout(binding = 0, set = 0) uniform sampler2D rawSsgiTex; // RGBA16F
layout(binding = 1, set = 0) uniform sampler2D gDepthTex;  // D32（全分辨率）
layout(binding = 2, set = 0) uniform sampler2D gNormalTex; // RGBA16F（全分辨率）

void main() {
    vec2 texel = 1.0 / vec2(textureSize(rawSsgiTex, 0));
    float centerDepth = texture(gDepthTex, fragUV).r;
    vec3 centerNormal = normalize(texture(gNormalTex, fragUV).rgb * 2.0 - 1.0);
    vec3 sum = vec3(0.0);
    float weightSum = 0.0;
    // 5×5 单帧双边：深度差 + 法线夹角降权（与 SSAO blur 同思路）。
    for (int y = -2; y <= 2; ++y) {
        for (int x = -2; x <= 2; ++x) {
            vec2 uv = fragUV + vec2(float(x), float(y)) * texel;
            float sampleDepth = texture(gDepthTex, uv).r;
            vec3 sampleNormal = normalize(texture(gNormalTex, uv).rgb * 2.0 - 1.0);
            vec3 sampleRadiance = texture(rawSsgiTex, uv).rgb;
            float depthWeight = 1.0 / (1.0 + abs(sampleDepth - centerDepth) * 1000.0);
            float normalWeight = 0.5 + 0.5 * max(dot(sampleNormal, centerNormal), 0.0);
            // 距离权重：中心权重大，远处权重略降。
            float spatialWeight = 1.0 / (1.0 + 0.15 * float(x * x + y * y));
            float weight = depthWeight * normalWeight * spatialWeight;
            sum += sampleRadiance * weight;
            weightSum += weight;
        }
    }
    outColor = vec4(weightSum > 1e-6 ? sum / weightSum : texture(rawSsgiTex, fragUV).rgb, 1.0);
}
