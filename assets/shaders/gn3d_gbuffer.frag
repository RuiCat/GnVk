#version 450
// ╔══════════════════════════════════════════════════════════╗
// ║  GN_mini — G-Buffer 片段着色器 (延迟渲染 MRT)           ║
// ║  将几何信息写入 3 个颜色附件 + 深度附件                  ║
// ║  G2a 打包（16 B/px）：                                    ║
// ║  附件0 RGBA16F: Normal(RGB,N*0.5+0.5) + Roughness(A)     ║
// ║  附件1 RGBA8_SRGB: Albedo(线性写入,硬件编码) + Metallic(A)║
// ║  附件2 RGBA8_UNORM: R=AO G=反射率 B=材质ID A=alpha       ║
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
    vec4 albedoSample = texture(texSampler, fragUV);
    vec3 albedo = albedoSample.rgb * pc.diffuse.rgb;
    float roughness = clamp(pc.specular.x, 0.04, 1.0); // 粗糙度下限 0.04 防除零
    float metallic  = clamp(pc.specular.y, 0.0,  1.0);
    float ao        = clamp(pc.specular.z, 0.0,  1.0);
    // G2a：alpha 通道打包进附件 2 的 A。alphaCutoff 预留于 pc.specular.w（0=不裁）；
    // 当前 C++ 侧恒写 0，因此默认路径不会 discard；材质 alpha 模式接线留给 G0b。
    float alpha = albedoSample.a * pc.diffuse.a;
    float alphaCutoff = clamp(pc.specular.w, 0.0, 1.0);
    if (alpha < alphaCutoff) discard;

    // ── 写入 G-Buffer ───────────────────────────────
    // 附件 0：法线从 [-1,1] 编码到 [0,1] (N*0.5+0.5) + 粗糙度（RGBA16F）
    gNormalRough = vec4(N * 0.5 + 0.5, roughness);

    // 附件 1：Albedo 线性值（RGBA8_SRGB 由硬件自动 sRGB 编码）+ Metallic(A)
    gAlbedoMetal = vec4(albedo, metallic);

    // 附件 2：R=AO、G=反射率（暂用介电 F0 0.04，材质通路待 G2b/G4）、B=材质ID、A=alpha
    gAo = vec4(ao, 0.04, 0.0, alpha);
}
