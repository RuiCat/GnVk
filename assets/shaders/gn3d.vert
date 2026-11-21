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
layout(location = 3) out float fragWorldY;

// 非均匀缩放必须用逆转置矩阵变换法线；det≈0 时退回单位矩阵。
mat3 normalMatrix(mat4 model) {
    mat3 linear = mat3(model);
    float det = determinant(linear);
    return abs(det) > 1.0e-8 ? transpose(inverse(linear)) : mat3(1.0);
}

void main() {
    vec4 worldPos = pc.model * vec4(inPos, 1.0);
    gl_Position = scene.viewProj * worldPos;
    fragWorldPos = worldPos.xyz;
    fragWorldY = worldPos.y;

    vec3 n = normalMatrix(pc.model) * inNormal;
    if (dot(n, n) < 1.0e-12) n = vec3(0.0, 0.0, 1.0);
    fragNormal = normalize(n);
    fragUV = inUV;
}
