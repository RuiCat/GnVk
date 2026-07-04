#version 450
// ╔══════════════════════════════════════════════════════════╗
// ║  GN_mini — PBR 顶点着色器                               ║
// ║  MVP 变换 + TBN 矩阵 + 世界空间位置/法线                 ║
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
    vec4 diffuse;       // albedo RGB + _pad
    vec4 specular;      // roughness, metallic, ao, _pad  (PBR模式下重映射)
} pc;

layout(location = 0) out vec3 fragWorldPos;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec2 fragUV;
layout(location = 3) out mat3 fragTBN;

void main() {
    vec4 worldPos = pc.model * vec4(inPos, 1.0);
    gl_Position = scene.viewProj * worldPos;
    fragWorldPos = worldPos.xyz;
    
    // 世界空间法线
    vec3 N = normalize(mat3(pc.model) * inNormal);
    fragNormal = N;
    
    // 世界空间切线 (gram-schmidt 正交化)
    vec3 T = normalize(mat3(pc.model) * inTangent);
    T = normalize(T - dot(T, N) * N);
    vec3 B = cross(N, T);
    fragTBN = mat3(T, B, N);
    
    fragUV = inUV;
}
