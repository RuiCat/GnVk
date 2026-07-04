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

void main() {
    // ── 世界空间变换 ─────────────────────────────────
    vec4 worldPos = pc.model * vec4(inPos, 1.0);
    gl_Position = scene.viewProj * worldPos;     // 裁剪空间
    fragWorldPos = worldPos.xyz;

    // ── 世界空间法线 (mat3 变换, 正确处理非均匀缩放) ──
    vec3 N = normalize(mat3(pc.model) * inNormal);
    fragNormal = N;

    // ── TBN 矩阵 (Gram-Schmidt 正交化) ───────────────
    // 用于片元着色器中法线贴图的切线空间→世界空间变换
    vec3 T = normalize(mat3(pc.model) * inTangent);
    T = normalize(T - dot(T, N) * N);   // 确保 T ⊥ N
    vec3 B = cross(N, T);               // 副法线
    fragTBN = mat3(T, B, N);            // 列主序: [T, B, N]

    fragUV = inUV;
}
