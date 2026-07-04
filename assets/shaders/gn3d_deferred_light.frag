#version 450
// ╔══════════════════════════════════════════════════════════╗
// ║  GN_mini — 延迟光照片段着色器 (PBR Cook-Torrance BRDF)  ║
// ║  读取 4 个 G-Buffer 纹理 → 逐光源 PBR + Skybox IBL     ║
// ║  GGX 法线分布 + Smith 几何遮蔽 + Fresnel-Schlick         ║
// ║  Poisson 16点 PCF 软阴影 + Reinhard 色调映射             ║
// ╚══════════════════════════════════════════════════════════╝

#define MAX_LIGHTS 8         // 最大光源数 (与 LightUBO 对齐)
#define PI 3.14159265359     // π

layout(location = 0) in vec2 fragUV;    // 屏幕空间 UV (全屏三角形)

// ── G-Buffer 输入纹理 ────────────────────────────────
layout(binding = 1) uniform sampler2D gNormalRoughTex; // RGB=编码法线(N*0.5+0.5), A=粗糙度
layout(binding = 2) uniform sampler2D gAlbedoMetalTex; // RGB=反射率, A=金属度
layout(binding = 3) uniform sampler2D gAOTex;          // R=环境遮蔽(AO)
layout(binding = 4) uniform sampler2D gDepthTex;       // R=深度 (Vulkan NDC [0,1])

// ── 阴影 + 天空盒 ────────────────────────────────────
layout(binding = 5) uniform sampler2DShadow shadowSampler;  // 阴影比较采样器
layout(binding = 6) uniform samplerCube  skyboxCubemap;     // 天空盒 Cubemap

// ── 光源数据结构 ─────────────────────────────────────
struct Light {
    vec4 color;         // RGB + intensity
    vec4 position;      // xyz + type (0=方向光, 1=点光, 2=聚光)
    vec4 direction;     // xyz + shadowType (0=无阴影, 1=2D, 2=Cube预留)
    vec4 attenuation;   // constant, linear, quadratic, range
    vec4 coneAngles;    // innerCos, outerCos (聚光灯用)
};

// ── Scene UBO ────────────────────────────────────────
layout(binding = 0) uniform SceneUBO {
    mat4 vp;            // 摄像机 VP 矩阵 (用于深度→世界位置重建)
    vec4 cp;            // 摄像机世界位置 (cameraPos)
    vec4 al;            // 场景环境光 (ambientLight)
} scene;

// ── Light UBO ────────────────────────────────────────
layout(binding = 7) uniform LightUBO {
    Light lights[MAX_LIGHTS];    // 8 个光源 (640B)
    vec4 ambientLight;           // 光源环境光
    float lightCount;            // 实际激活光源数
    float _p0, _p1, _p2;        // padding
    mat4 shadowVP;               // 光源 VP 矩阵 (阴影映射用)
    float smSize;                // Shadow Map 分辨率
    float _p3, _p4, _p5;        // padding
} ld;

layout(location = 0) out vec4 outColor;

// ═══════════════════════════════════════════════════════════
//  Poisson 圆盘采样点 (16 点, 用于 PCF 软阴影)
// ═══════════════════════════════════════════════════════════
const vec2 pd[16] = vec2[](
    vec2(-0.942, -0.399), vec2( 0.946, -0.769), vec2(-0.094, -0.929),
    vec2( 0.345,  0.294), vec2(-0.916,  0.458), vec2(-0.815, -0.879),
    vec2(-0.383,  0.277), vec2( 0.975,  0.756), vec2( 0.443, -0.975),
    vec2( 0.537, -0.474), vec2(-0.265, -0.419), vec2( 0.792,  0.191),
    vec2(-0.242,  0.997), vec2(-0.814,  0.914), vec2( 0.200,  0.786),
    vec2( 0.144, -0.141)
);

// ═══════════════════════════════════════════════════════════
//  PCF 软阴影计算
// ═══════════════════════════════════════════════════════════
float pcf(vec4 fls, vec3 N, vec3 L, vec3 wp) {
    vec3 p = fls.xyz / max(fls.w, 0.0001);
    p = p * 0.5 + 0.5;                        // NDC [-1,1] → UV [0,1]

    if (any(lessThan(p, vec3(0.0))) || any(greaterThan(p, vec3(1.0))))
        return 1.0;

    float bias = max(0.002 * (1.0 - dot(N, L)), 0.0005);
    float sz   = max(ld.smSize, 1.0);
    float ts   = 1.0 / sz;

    float angle = fract(sin(dot(wp.xy, vec2(12.9898, 78.233))) * 43758.5453)
                * 2.0 * PI;
    float ca = cos(angle), sa = sin(angle);

    float shadow = 0.0;
    for (int i = 0; i < 16; i++) {
        vec2 o = vec2(pd[i].x * ca - pd[i].y * sa,
                      pd[i].x * sa + pd[i].y * ca);
        shadow += texture(shadowSampler,
                          vec3(p.xy + o * ts * 1.5,
                               p.z - bias));
    }
    return shadow / 16.0;
}

// ═══════════════════════════════════════════════════════════
//  深度 → 世界位置重建
// ═══════════════════════════════════════════════════════════
vec3 wpFromDepth(float d, vec2 uv) {
    vec4 clip = vec4(uv * 2.0 - 1.0, d, 1.0);
    vec4 view = inverse(scene.vp) * clip;
    return view.xyz / view.w;
}

// ═══════════════════════════════════════════════════════════
//  PBR Cook-Torrance BRDF 函数
// ═══════════════════════════════════════════════════════════

// GGX/Trowbridge-Reitz 法线分布函数
float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float denom = NdotH2 * (a2 - 1.0) + 1.0;
    return a2 / (PI * denom * denom);
}

// Schlick-GGX 几何遮蔽函数
float GeometrySchlickGGX(float NdotV, float roughness) {
    float k = (roughness + 1.0) * (roughness + 1.0) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

// Smith 联合遮蔽-阴影函数
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    return GeometrySchlickGGX(max(dot(N, V), 0.0), roughness)
         * GeometrySchlickGGX(max(dot(N, L), 0.0), roughness);
}

// Fresnel-Schlick
vec3 FresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// ═══════════════════════════════════════════════════════════
//  单光源 PBR 贡献计算 (Cook-Torrance BRDF)
// ═══════════════════════════════════════════════════════════
vec3 CalcPBRLightDeferred(Light L, vec3 N, vec3 V, vec3 wp,
                           vec3 albedo, float roughness, float metallic) {
    vec3 lc = L.color.rgb * L.color.a;            // 光源颜色 × 强度
    if (lc.r + lc.g + lc.b < 0.001) return vec3(0.0);

    // ── 光照方向 + 衰减 ──────────────────────────
    vec3 Ldir;
    float att = 1.0;
    float st  = L.direction.w;

    if (L.position.w < 0.5) {
        Ldir = normalize(-L.direction.xyz);
    } else if (L.position.w < 1.5) {
        vec3 t = L.position.xyz - wp;
        float d = length(t);
        Ldir = t / max(d, 0.0001);
        att = 1.0 / (L.attenuation.x + L.attenuation.y * d
                   + L.attenuation.z * d * d);
    } else {
        vec3 t = L.position.xyz - wp;
        float d = length(t);
        Ldir = t / max(d, 0.0001);
        float ct = dot(-Ldir, normalize(L.direction.xyz));
        float e  = L.coneAngles.x - L.coneAngles.y;
        float spot = clamp((ct - L.coneAngles.y) / max(e, 0.0001), 0.0, 1.0);
        att = spot / (L.attenuation.x + L.attenuation.y * d
                    + L.attenuation.z * d * d);
    }

    // ── 阴影计算 ──────────────────────────────────
    float sh = 1.0;
    if (st > 0.5 && st < 1.5) {
        vec4 fls = ld.shadowVP * vec4(wp, 1.0);
        sh = pcf(fls, N, Ldir, wp);
    }

    vec3 radiance = lc * att * sh;
    vec3 H = normalize(V + Ldir);

    // ── 材质参数 ───────────────────────────────────
    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    // ── Cook-Torrance BRDF ────────────────────────
    float NDF = DistributionGGX(N, H, roughness);
    float G   = GeometrySmith(N, V, Ldir, roughness);
    vec3  F   = FresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3 numerator    = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, Ldir), 0.0) + 0.0001;
    vec3 specularBRDF = numerator / denominator;

    vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);
    float NdotL = max(dot(N, Ldir), 0.0);

    // ── 包裹漫反射 (柔化明暗交界线) ───────────────
    float wrap = (NdotL + 0.3) / 1.3;

    // kD·albedo/π (Lambertian) × wrap + specular (纯 PBR, 不加 wrap)
    return kD * albedo / PI * radiance * wrap + specularBRDF * radiance * NdotL;
}

// ═══════════════════════════════════════════════════════════
//  主函数
// ═══════════════════════════════════════════════════════════
void main() {
    // ── 读取 G-Buffer ────────────────────────────────
    vec4 gNR = texture(gNormalRoughTex, fragUV);
    vec4 gAM = texture(gAlbedoMetalTex,  fragUV);
    vec4 gAo = texture(gAOTex,           fragUV);
    float depth = texture(gDepthTex, fragUV).r;

    // ── 天空: depth ≈ 1.0 → 直接采样天空盒 ──────────
    if (depth >= 0.9999) {
        vec3 skyDir = normalize(wpFromDepth(depth, fragUV) - scene.cp.xyz);
        outColor = vec4(texture(skyboxCubemap, skyDir).rgb, 1.0);
        return;
    }

    // ── 解码 G-Buffer 数据 ───────────────────────────
    vec3 albedo   = gAM.rgb;
    vec3 N        = normalize(gNR.rgb * 2.0 - 1.0);
    float roughness = clamp(gNR.a, 0.04, 1.0);        // 使用粗糙度!
    float metallic  = clamp(gAM.a, 0.0, 1.0);          // 使用金属度!
    float ao        = gAo.r;

    // ── 重建世界位置 + 视线方向 ─────────────────────
    vec3 wp = wpFromDepth(depth, fragUV);
    vec3 V  = normalize(scene.cp.xyz - wp);

    // ═══════════════════════════════════════════════════════
    //  PBR 直接光照 (Cook-Torrance BRDF, 逐光源)
    // ═══════════════════════════════════════════════════════
    vec3 Lo = vec3(0.0);
    int cnt = int(ld.lightCount);

    for (int i = 0; i < cnt && i < MAX_LIGHTS; i++) {
        Lo += CalcPBRLightDeferred(ld.lights[i], N, V, wp,
                                    albedo, roughness, metallic);
    }

    // ═══════════════════════════════════════════════════════
    //  环境光 + IBL
    // ═══════════════════════════════════════════════════════
    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    float NdotV = max(dot(N, V), 0.0);

    // 分析环境光
    vec3 amb = scene.al.rgb * ld.ambientLight.rgb * 3.0;

    // Skybox IBL
    vec3 skyDiffuse  = texture(skyboxCubemap, N).rgb;
    vec3 R = reflect(-V, N);
    vec3 skySpecular = texture(skyboxCubemap, R).rgb;

    // Fresnel → 漫反射/镜面反射比例
    vec3 F = FresnelSchlick(NdotV, F0);
    vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);

    // 漫反射 IBL: 天空漫反射 × albedo, 混合分析环境光
    vec3 diffuseIBL = kD * albedo * mix(amb, skyDiffuse, 0.5) * ao;

    // 镜面反射 IBL: 天空反射 × Fresnel × 粗糙度调制
    float specularOcclusion = 1.0 - roughness * 0.7;
    vec3 specularIBL = skySpecular * F * specularOcclusion * 0.5;

    vec3 ambient = diffuseIBL + specularIBL;
    vec3 color = ambient + Lo;

    // ── Reinhard 色调映射 (HDR→LDR) ─────────────────
    color = color / (color + vec3(1.0));

    outColor = vec4(color, 1.0);
}
