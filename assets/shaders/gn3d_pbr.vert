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
    // 自发光（RGB + 强度）。顶点阶段用不到，但 push constant 块必须与片元阶段逐字段一致。
    vec4 emissive;
} pc;

layout(location = 0) out vec3 fragWorldPos;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec2 fragUV;
layout(location = 3) out mat3 fragTBN;

// 非均匀缩放必须用逆转置矩阵变换法线；det≈0 时退回单位矩阵，避免 inverse(0) 产生 NaN。
mat3 normalMatrix(mat4 model) {
    mat3 linear = mat3(model);
    float det = determinant(linear);
    return abs(det) > 1.0e-8 ? transpose(inverse(linear)) : mat3(1.0);
}

void main() {
    vec4 worldPos = pc.model * vec4(inPos, 1.0);
    gl_Position = scene.viewProj * worldPos;
    fragWorldPos = worldPos.xyz;

    // 世界空间法线（逆转置）
    vec3 N = normalMatrix(pc.model) * inNormal;
    if (dot(N, N) < 1.0e-12) N = vec3(0.0, 0.0, 1.0);
    N = normalize(N);
    fragNormal = N;

    // 世界空间切线：先投影再检查退化；零切线 / 切线∥法线都要有回退。
    vec3 T = mat3(pc.model) * inTangent;
    T -= N * dot(T, N);
    if (dot(T, T) < 1.0e-8) {
        vec3 axis = abs(N.y) < 0.9 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
        T = cross(axis, N);
    }
    T = normalize(T);
    vec3 B = cross(N, T);
    fragTBN = mat3(T, B, N);

    fragUV = inUV;
}
