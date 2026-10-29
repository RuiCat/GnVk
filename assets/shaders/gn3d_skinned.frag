#version 450
// ╔══════════════════════════════════════════════════════════╗
// ║  GN_mini — P2 蒙皮片段着色器                             ║
// ║  albedo × 单方向 Lambert 近似（不引光照 UBO）            ║
// ╚══════════════════════════════════════════════════════════╝
//
// 颜色由 push constant 传入（GN_VkDraw3DSkinned 的 r/g/b），只做 albedo × Lambert，
// 与契约一致：不采样纹理、不读光照 UBO，GPU 验收靠颜色与几何边界判定。

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
