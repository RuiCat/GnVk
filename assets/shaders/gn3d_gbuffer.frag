#version 450
// ╔══════════════════════════════════════════════════════════╗
// ║  GN_mini — G-Buffer 片段着色器 (延迟渲染 MRT)           ║
// ║  将几何信息写入 3 个颜色附件 + 深度附件                  ║
// ║  附件0: Normal(RGB,N*0.5+0.5) + Roughness(A)            ║
// ║  附件1: Albedo(RGB) + Metallic(A)                       ║
// ║  附件2: AO(R)                                           ║
// ╚══════════════════════════════════════════════════════════╝

// ── 顶点着色器输出 (插值后) ────────────────────────────
layout(location = 0) in vec3 fragWorldPos;   // 世界空间位置
layout(location = 1) in vec3 fragNormal;     // 世界空间法线 (顶点)
layout(location = 2) in vec2 fragUV;         // 纹理坐标
layout(location = 3) in mat3 fragTBN;        // TBN 矩阵 (法线贴图用)

// ── 输入: UBO + 纹理 ──────────────────────────────────
layout(binding = 0, set = 0) uniform SceneUBO {
    mat4 viewProj;        // 摄像机 VP 矩阵
    vec4 cameraPos;       // 摄像机世界位置
    vec4 ambientLight;    // 环境光 (RGB + 强度)
} scene;

layout(binding = 1, set = 0) uniform sampler2D texSampler;      // Albedo 纹理
layout(binding = 3, set = 0) uniform sampler2D normalSampler;   // 法线贴图

// ── Push Constant: 模型矩阵 + 材质参数 ───────────────
layout(push_constant) uniform ModelPC {
    mat4 model;           // 模型矩阵 (世界变换)
    vec4 diffuse;         // PBR: albedo RGB
    vec4 specular;        // PBR: x=roughness, y=metallic, z=ao
} pc;

// ── MRT 输出 (3 个颜色附件) ──────────────────────────
layout(location = 0) out vec4 gNormalRough; // RGB=编码法线, A=粗糙度
layout(location = 1) out vec4 gAlbedoMetal; // RGB=反射率,   A=金属度
layout(location = 2) out vec4 gAo;          // R=环境遮蔽,   GBA=未使用

void main() {
    // ── 法线计算 (法线贴图优先, 否则用插值法线) ────
    vec3 N = normalize(fragNormal);
    vec3 texN = texture(normalSampler, fragUV).rgb;
    if (length(texN) > 0.01) {
        // 法线贴图: [0,1]→[-1,1] 解码, TBN 变换到世界空间
        texN = normalize(texN * 2.0 - 1.0);
        N = normalize(fragTBN * texN);
    }

    // ── 材质参数 (从 Push Constant 获取) ─────────────
    vec3 albedo    = texture(texSampler, fragUV).rgb * pc.diffuse.rgb;
    float roughness = clamp(pc.specular.x, 0.04, 1.0); // 粗糙度下限 0.04 防除零
    float metallic  = clamp(pc.specular.y, 0.0,  1.0);
    float ao        = clamp(pc.specular.z, 0.0,  1.0);

    // ── 写入 G-Buffer ───────────────────────────────
    // 法线: 从 [-1,1] 编码到 [0,1] (N*0.5+0.5)
    // 粗糙度: 直接存储 (延迟着色器读取)
    gNormalRough = vec4(N * 0.5 + 0.5, roughness);

    // Albedo: 反射率 (无光照颜色)
    // Metallic: 金属度 (0=介质, 1=纯金属)
    gAlbedoMetal = vec4(albedo, metallic);

    // AO: 环境遮蔽 (1=无遮蔽, 0=全遮蔽)
    gAo = vec4(ao, 0.0, 0.0, 1.0);
}
