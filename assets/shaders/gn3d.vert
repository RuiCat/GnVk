#version 450
// ╔══════════════════════════════════════════════════════════╗
// ║  GN_mini — Blinn-Phong 顶点着色器                       ║
// ║  MVP 变换 + 世界空间位置/法线                            ║
// ╚══════════════════════════════════════════════════════════╝
layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inTangent;
layout(location = 3) in vec2 inUV;

layout(binding = 0, set = 0) uniform SceneUBO {
    mat4 viewProj;
    vec4 cameraPos;
    vec4 ambientLight;
} scene;

layout(push_constant) uniform ModelPC {
    mat4 model;
    vec4 diffuse;
    vec4 specular;
} pc;

layout(location = 0) out vec3 fragWorldPos;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec2 fragUV;

void main() {
    vec4 worldPos = pc.model * vec4(inPos, 1.0);
    gl_Position = scene.viewProj * worldPos;
    fragWorldPos = worldPos.xyz;

    // 世界空间法线 — 用 model 矩阵 3×3 部分变换 (正确处理非均匀缩放)
    fragNormal = normalize(mat3(pc.model) * inNormal);

    fragUV = inUV;
}
