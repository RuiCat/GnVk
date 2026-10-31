#version 450
// ╔══════════════════════════════════════════════════════════╗
// ║  GN_mini — SSAO 片段着色器（G2b，世界空间）              ║
// ║  深度 + invViewProj 重建世界位置 → 16 点半球核 → R8      ║
// ╚══════════════════════════════════════════════════════════╝
// 契约（docs/GI_PLAN.md §4.2.1）：位置/法线均为世界空间；
// UBO 400 B：viewProj@0 / invViewProj@64 / samples[16]@128 / params@384。

layout(location = 0) in vec2 fragUV;
layout(location = 0) out float outAO;

layout(binding = 0, set = 0) uniform SSAO_UBO {
    mat4 viewProj;
    mat4 invViewProj;
    vec4 samples[16]; // xyz = 半球样本方向（+z 半球），w = padding
    vec4 params;      // x=半径(m) y=偏置 z=幂 w=启用位(0=关)
} ssao;

layout(binding = 1, set = 0) uniform sampler2D gNormalTex; // RGBA16F：世界空间法线
layout(binding = 2, set = 0) uniform sampler2D gDepthTex;  // D32：Vulkan NDC z [0,1]

vec3 worldPosFromDepth(float depth, vec2 uv) {
    vec4 clip = vec4(uv * 2.0 - 1.0, depth, 1.0);
    vec4 world = ssao.invViewProj * clip;
    return world.xyz / world.w;
}

void main() {
    float depth = texture(gDepthTex, fragUV).r;
    if (depth >= 0.9999) {
        outAO = 1.0;
        return;
    }
    vec3 P = worldPosFromDepth(depth, fragUV);
    vec3 N = normalize(texture(gNormalTex, fragUV).rgb * 2.0 - 1.0);

    // 逐像素随机切向（固定哈希，无时间噪声），打散带状伪影。
    float angle = fract(sin(dot(fragUV, vec2(12.9898, 78.233))) * 43758.5453) * 6.2831853;
    vec3 randomVec = vec3(cos(angle), sin(angle), 0.0);
    vec3 tangent = randomVec - N * dot(randomVec, N);
    if (dot(tangent, tangent) < 1e-6) {
        tangent = abs(N.y) < 0.99 ? cross(N, vec3(0.0, 1.0, 0.0)) : cross(N, vec3(1.0, 0.0, 0.0));
    }
    tangent = normalize(tangent);
    vec3 bitangent = cross(N, tangent);
    mat3 tbn = mat3(tangent, bitangent, N);

    const int SAMPLE_COUNT = 16;
    float occlusion = 0.0;
    for (int i = 0; i < SAMPLE_COUNT; ++i) {
        vec3 samplePos = P + tbn * ssao.samples[i].xyz * ssao.params.x;
        vec4 clip = ssao.viewProj * vec4(samplePos, 1.0);
        if (clip.w <= 1e-6) continue;
        vec3 ndc = clip.xyz / clip.w;
        vec2 uv = ndc.xy * 0.5 + 0.5;
        if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) continue;
        float storedDepth = texture(gDepthTex, uv).r;
        // NDC z 越小越靠近相机：已存表面在采样点前方 ⇒ 采样点被遮挡。
        if (storedDepth < ndc.z - ssao.params.y) {
            vec3 storedPos = worldPosFromDepth(storedDepth, uv);
            float distance = length(P - storedPos);
            // 距离超出半径的遮挡视为前景物体，不计入（range check）。
            float rangeCheck = smoothstep(0.0, 1.0, ssao.params.x / max(distance, 1e-4));
            occlusion += rangeCheck;
        }
    }
    float ao = 1.0 - clamp(occlusion / float(SAMPLE_COUNT), 0.0, 1.0);
    outAO = pow(clamp(ao, 0.0, 1.0), max(ssao.params.z, 1e-3));
}
