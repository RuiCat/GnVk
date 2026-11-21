#version 450
// ╔══════════════════════════════════════════════════════════╗
// ║  GN_mini — G-Buffer 顶点着色器                          ║
// ║  世界空间变换 + TBN 矩阵生成                             ║
// ╚══════════════════════════════════════════════════════════╝

// ── 顶点属性 (VkVertex3D 格式: 11 floats) ────────────
layout(location = 0) in vec3 inPos;       // 模型空间位置
layout(location = 1) in vec3 inNormal;    // 模型空间法线
layout(location = 2) in vec3 inTangent;   // 模型空间切线
layout(location = 3) in vec2 inUV;        // 纹理坐标

// ── 场景 UBO (每帧更新) ──────────────────────────────
layout(binding = 0, set = 0) uniform SceneUBO {
    mat4 viewProj;        // 摄像机 VP 矩阵 (列主序)
    vec4 cameraPos;       // 摄像机世界位置 (w=padding)
    vec4 ambientLight;    // 环境光
} scene;

// ── Push Constant: 模型矩阵 ───────────────────────────
layout(push_constant) uniform ModelPC {
    mat4 model;           // 模型矩阵 (世界变换)
    vec4 diffuse;         // 漫反射颜色 (暂不使用)
    vec4 specular;        // 镜面参数 (暂不使用)
} pc;

// ── 输出到片元着色器 ─────────────────────────────────
layout(location = 0) out vec3 fragWorldPos;   // 世界空间位置
layout(location = 1) out vec3 fragNormal;     // 世界空间法线
layout(location = 2) out vec2 fragUV;         // 纹理坐标
layout(location = 3) out mat3 fragTBN;        // TBN 矩阵 (切线空间→世界空间)

// 非均匀缩放必须用逆转置矩阵变换法线；det≈0 时退回单位矩阵，避免 inverse(0) 产生 NaN。
mat3 normalMatrix(mat4 model) {
    mat3 linear = mat3(model);
    float det = determinant(linear);
    return abs(det) > 1.0e-8 ? transpose(inverse(linear)) : mat3(1.0);
}

void main() {
    // ── 世界空间变换 ─────────────────────────────────
    vec4 worldPos = pc.model * vec4(inPos, 1.0);
    gl_Position = scene.viewProj * worldPos;     // 裁剪空间
    fragWorldPos = worldPos.xyz;

    // ── 世界空间法线（逆转置，正确处理非均匀缩放）────
    vec3 N = normalMatrix(pc.model) * inNormal;
    if (dot(N, N) < 1.0e-12) N = vec3(0.0, 0.0, 1.0);
    N = normalize(N);
    fragNormal = N;

    // ── TBN 矩阵 (Gram-Schmidt 正交化) ───────────────
    // 用于片元着色器中法线贴图的切线空间→世界空间变换。
    // 注意：回退必须放在“投影之后”。原始切线与 N 平行时，投影结果同样为零，
    // 只检查原始切线（旧实现）仍会在 normalize(0) 处产生 NaN。
    vec3 T = mat3(pc.model) * inTangent;
    T -= N * dot(T, N);
    if (dot(T, T) < 1.0e-8) {
        vec3 axis = abs(N.y) < 0.9 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
        T = cross(axis, N);
    }
    T = normalize(T);
    vec3 B = cross(N, T);               // 副法线
    fragTBN = mat3(T, B, N);            // 列主序: [T, B, N]

    fragUV = inUV;
}
