#version 450
// ╔══════════════════════════════════════════════════════════╗
// ║  GN_mini — G-Buffer 四通道调试视图（G0b）                ║
// ║  0=NormalRough、1=AlbedoMetal、2=AO/材质、3=线性化深度   ║
// ╚══════════════════════════════════════════════════════════╝
// 复用 deferredLightDescriptorSet 的布局：0=SceneUBO、1=NormalRough、2=AlbedoMetal、
// 3=AO/材质、4=Depth、9=GI UBO（仅取 invViewProj 用于深度线性化）。

layout(location = 0) in vec2 fragUV;
layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform SceneUBO {
    mat4 vp;
    vec4 cp; // 摄像机世界位置
    vec4 al;
} scene;
layout(binding = 1) uniform sampler2D gNormalRoughTex; // RGBA16F
layout(binding = 2) uniform sampler2D gAlbedoMetalTex; // RGBA8_SRGB（采样自动 linear）
layout(binding = 3) uniform sampler2D gAOTex;          // RGBA8_UNORM
layout(binding = 4) uniform sampler2D gDepthTex;       // D32
layout(binding = 9) uniform GIUBO {
    vec4 indirect;
    vec4 aoParams;
    vec4 envParams;
    vec4 sh[9];
    mat4 invViewProj;
} gi;

layout(push_constant) uniform DebugPC {
    int channel; // 0..3（调用方已钳制）
} pc;

vec3 worldPosFromDepth(float depth, vec2 uv) {
    vec4 clip = vec4(uv * 2.0 - 1.0, depth, 1.0);
    vec4 world = gi.invViewProj * clip;
    return world.xyz / world.w;
}

void main() {
    int channel = clamp(pc.channel, 0, 3);
    vec3 color = vec3(0.0);
    if (channel == 0) {
        // 法线已按 N*0.5+0.5 编码，直接作为 RGB 显示。
        color = texture(gNormalRoughTex, fragUV).rgb;
    } else if (channel == 1) {
        // albedo 线性值（RGBA8_SRGB 采样时硬件已解码）；调试图不做 gamma。
        color = texture(gAlbedoMetalTex, fragUV).rgb;
    } else if (channel == 2) {
        // R=AO、G=反射率、B=材质ID/255。
        color = texture(gAOTex, fragUV).rgb;
    } else {
        // 线性化伪彩：深度 → 世界位置 → 与相机的世界距离 → 伪彩渐变。
        float depth = texture(gDepthTex, fragUV).r;
        if (depth >= 0.9999) {
            color = vec3(0.0);
        } else {
            vec3 wp = worldPosFromDepth(depth, fragUV);
            float distance = length(wp - scene.cp.xyz);
            float t = clamp(distance / 50.0, 0.0, 1.0);
            color = vec3(t, 1.0 - abs(t - 0.5) * 2.0, 1.0 - t);
        }
    }
    outColor = vec4(color, 1.0);
}
