// gn3d_skinned_ssbo.frag — P9-b SSBO 调色板蒙皮的片段着色器（与 P2 的 gn3d_skinned.frag 等价）。
//
// albedo × 单方向 Lambert 近似；颜色走 push constant 的 vec4 color。
#version 450

layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec2 fragUv;
layout(location = 2) in vec3 fragWorldPos;

layout(push_constant) uniform ModelPC {
    mat4 model;
    vec4 color;
} pc;

layout(location = 0) out vec4 outColor;

void main() {
    vec3 normal = normalize(fragNormal);
    vec3 lightDirection = normalize(vec3(0.45, 0.78, 0.44));
    float lambert = max(dot(normal, lightDirection), 0.0);
    outColor = vec4(pc.color.rgb * (0.35 + 0.65 * lambert), 1.0);
}
