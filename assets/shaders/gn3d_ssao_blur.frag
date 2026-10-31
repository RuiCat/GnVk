#version 450
// ╔══════════════════════════════════════════════════════════╗
// ║  GN_mini — SSAO 4×4 双边模糊（G2b）                      ║
// ║  以深度/法线差加权，避免把前景/背景的 AO 糊到一起         ║
// ╚══════════════════════════════════════════════════════════╝

layout(location = 0) in vec2 fragUV;
layout(location = 0) out float outAO;

layout(binding = 0, set = 0) uniform SSAO_UBO {
    mat4 viewProj;
    mat4 invViewProj;
    vec4 samples[16];
    vec4 params; // x=半径 y=偏置 z=幂 w=启用位
} ssao;

layout(binding = 1, set = 0) uniform sampler2D rawAOTex;   // R8：SSAO 原始输出
layout(binding = 2, set = 0) uniform sampler2D gDepthTex;  // D32
layout(binding = 3, set = 0) uniform sampler2D gNormalTex; // RGBA16F：世界空间法线

void main() {
    vec2 texel = 1.0 / vec2(textureSize(rawAOTex, 0));
    float centerDepth = texture(gDepthTex, fragUV).r;
    vec3 centerNormal = normalize(texture(gNormalTex, fragUV).rgb * 2.0 - 1.0);

    float sum = 0.0;
    float weightSum = 0.0;
    // 固定 4×4 邻域（每个 texel 一个样本），深度差与法线夹角共同降权。
    for (int y = -2; y <= 1; ++y) {
        for (int x = -2; x <= 1; ++x) {
            vec2 uv = fragUV + vec2(float(x) + 0.5, float(y) + 0.5) * texel;
            float sampleDepth = texture(gDepthTex, uv).r;
            vec3 sampleNormal = normalize(texture(gNormalTex, uv).rgb * 2.0 - 1.0);
            float ao = texture(rawAOTex, uv).r;
            // 深度权重：相对差越大权重越小（1e3 为经验尺度，未在 GPU 上标定）。
            float depthWeight = 1.0 / (1.0 + abs(sampleDepth - centerDepth) * 1000.0);
            float normalWeight = 0.5 + 0.5 * max(dot(sampleNormal, centerNormal), 0.0);
            float weight = depthWeight * normalWeight;
            sum += ao * weight;
            weightSum += weight;
        }
    }
    outAO = weightSum > 1e-6 ? sum / weightSum : texture(rawAOTex, fragUV).r;
}
