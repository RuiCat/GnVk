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
// G2a：附件 1 为 RGBA8_SRGB（采样时硬件自动 sRGB→linear）；附件 2 为 RGBA8_UNORM。
layout(binding = 1) uniform sampler2D gNormalRoughTex; // RGBA16F: RGB=编码法线(N*0.5+0.5), A=粗糙度
layout(binding = 2) uniform sampler2D gAlbedoMetalTex; // RGBA8_SRGB: RGB=albedo(线性), A=金属度
layout(binding = 3) uniform sampler2D gAOTex;          // RGBA8_UNORM: R=AO G=反射率 B=材质ID A=alpha
layout(binding = 4) uniform sampler2D gDepthTex;       // D32 深度 (Vulkan NDC [0,1])

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

// P11-S3：雾（独立 Weather UBO，绑定 8；density == 0 时无效果）
layout(binding = 8) uniform FogUBO {
    vec4 fogColor;
    vec4 fogParams;  // x=density(1/m), y=heightFalloff(1/m), z=baseHeight(m), w=maxOpacity
} fog;

// ── G1b：GI UBO（binding 9）+ 预滤波环境 cubemap（10）+ BRDF LUT（11，RG16F）──
layout(binding = 9) uniform GIUBO {
    vec4 indirect;   // x=漫反射倍率 y=镜面倍率 z=AO 强度 w=SSAO 开关
    vec4 aoParams;
    vec4 envParams;  // x=环境旋转 y=预滤波 mip 数
    vec4 sh[9];      // SH9 辐照度系数（rgb + padding）
    mat4 invViewProj;
} gi;
layout(binding = 10) uniform samplerCube envPrefilteredCube;
layout(binding = 11) uniform sampler2D brdfLutTex;
// G2b：blurAO（R8）；GI UBO 的 indirect.w = 1 时才参与合成。
layout(binding = 12) uniform sampler2D ssaoTex;
// G4：半分辨率去噪 SSGI（RGBA16F）；GI UBO 的 envParams.w=0 时不参与（逐像素等同现状）。
layout(binding = 13) uniform sampler2D ssgiTex;
// G5b-2：探针 UBO（48 B）+ 9 张 SH9 探针 3D 纹理（RGBA16F 8×4×8，各存 1 个 vec4 系数）。
layout(binding = 14) uniform PROBE_UBO {
    vec4 origin;        // xyz = 网格原点
    vec4 spacingCounts; // x=spacing, y=countX, z=countY, w=countZ
    vec4 weight;        // x=权重（0=关）
} probe;
layout(binding = 15) uniform sampler3D probeT0;
layout(binding = 16) uniform sampler3D probeT1;
layout(binding = 17) uniform sampler3D probeT2;
layout(binding = 18) uniform sampler3D probeT3;
layout(binding = 19) uniform sampler3D probeT4;
layout(binding = 20) uniform sampler3D probeT5;
layout(binding = 21) uniform sampler3D probeT6;
layout(binding = 22) uniform sampler3D probeT7;
layout(binding = 23) uniform sampler3D probeT8;

// 与 giParams::evalSh9 一致：y 向上世界空间，余弦卷积 A_l = π, 2π/3, π/4。
vec3 evalSH9(vec3 n) {
    float b0 = 0.2820947918;
    float b1 = 0.4886025119 * n.y;
    float b2 = 0.4886025119 * n.z;
    float b3 = 0.4886025119 * n.x;
    float b4 = 1.0925484306 * n.x * n.y;
    float b5 = 1.0925484306 * n.y * n.z;
    float b6 = 0.3153915653 * (3.0 * n.z * n.z - 1.0);
    float b7 = 1.0925484306 * n.x * n.z;
    float b8 = 0.5462742153 * (n.x * n.x - n.y * n.y);
    vec3 irr = (PI * b0) * gi.sh[0].rgb;
    irr += (2.0 * PI / 3.0) * (b1 * gi.sh[1].rgb + b2 * gi.sh[2].rgb + b3 * gi.sh[3].rgb);
    irr += (PI / 4.0) * (b4 * gi.sh[4].rgb + b5 * gi.sh[5].rgb + b6 * gi.sh[6].rgb
                         + b7 * gi.sh[7].rgb + b8 * gi.sh[8].rgb);
    return max(irr, vec3(0.0));
}

// G5b-2：三线性采样 9 张探针纹理（硬件 3D 线性 + CLAMP），再按 SH9 基函数求辐照度。
// 世界→格坐标 → texel 中心归一 (gridCoord+0.5)/counts；weight=0 时调用点不进入本函数。
vec3 sampleProbeSh(vec3 wp, vec3 n) {
    vec3 counts = max(probe.spacingCounts.yzw, vec3(1.0));
    vec3 grid = (wp - probe.origin.xyz) / max(probe.spacingCounts.x, 1e-6);
    grid = clamp(grid, vec3(0.0), counts - vec3(1.0));
    vec3 uvw = (grid + vec3(0.5)) / counts;
    vec4 c0 = texture(probeT0, uvw);
    vec4 c1 = texture(probeT1, uvw);
    vec4 c2 = texture(probeT2, uvw);
    vec4 c3 = texture(probeT3, uvw);
    vec4 c4 = texture(probeT4, uvw);
    vec4 c5 = texture(probeT5, uvw);
    vec4 c6 = texture(probeT6, uvw);
    vec4 c7 = texture(probeT7, uvw);
    vec4 c8 = texture(probeT8, uvw);
    float b0 = 0.2820947918;
    float b1 = 0.4886025119 * n.y;
    float b2 = 0.4886025119 * n.z;
    float b3 = 0.4886025119 * n.x;
    float b4 = 1.0925484306 * n.x * n.y;
    float b5 = 1.0925484306 * n.y * n.z;
    float b6 = 0.3153915653 * (3.0 * n.z * n.z - 1.0);
    float b7 = 1.0925484306 * n.x * n.z;
    float b8 = 0.5462742153 * (n.x * n.x - n.y * n.y);
    vec3 irr = (PI * b0) * c0.rgb;
    irr += (2.0 * PI / 3.0) * (b1 * c1.rgb + b2 * c2.rgb + b3 * c3.rgb);
    irr += (PI / 4.0) * (b4 * c4.rgb + b5 * c5.rgb + b6 * c6.rgb + b7 * c7.rgb + b8 * c8.rgb);
    return max(irr, vec3(0.0));
}

float fogAmountAt(vec3 worldPosition) {
    float distanceToCamera = length(worldPosition - scene.cp.xyz);
    float heightScale = exp(-fog.fogParams.y * max(worldPosition.y - fog.fogParams.z, 0.0));
    float amount = 1.0 - exp(-fog.fogParams.x * heightScale * distanceToCamera);
    return clamp(amount, 0.0, fog.fogParams.w);
}

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
    // G2b：invViewProj 由 CPU 每帧写入 GI UBO 偏移 192（不再每像素 inverse）。
    vec4 clip = vec4(uv * 2.0 - 1.0, d, 1.0);
    vec4 view = gi.invViewProj * clip;
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
    // G2b：materialAO 来自附件 2 的 .r；SSAO 开启（indirect.w=1）时与 blurAO 相乘。
    float materialAO = gAo.r;
    float ssao = (gi.indirect.w > 0.5) ? texture(ssaoTex, fragUV).r : 1.0;
    float ao = materialAO * mix(1.0, ssao, clamp(gi.indirect.w, 0.0, 1.0));

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
    //  G1b：GI 倍率 > 0 走 split-sum + SH9；为 0（默认）保持接线前观感
    // ═══════════════════════════════════════════════════════
    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    float NdotV = max(dot(N, V), 0.0);

    vec3 ambient;
    if (gi.indirect.x + gi.indirect.y <= 0.0) {
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
        ambient = diffuseIBL + specularIBL;
    } else {
        // ─ split-sum IBL：SH9 漫反射 + 预滤波 cubemap × BRDF LUT 镜面 ─
        vec3 F = FresnelSchlick(NdotV, F0);
        vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);
        // G5b-2：probe weight=0 时走原 evalSH9（逐像素等同现状，不采样探针纹理）。
        vec3 ambientEnv = evalSH9(N);
        if (probe.weight.x > 0.0) {
            ambientEnv = mix(ambientEnv, sampleProbeSh(wp, N), clamp(probe.weight.x, 0.0, 1.0));
        }
        vec3 envDiffuse = kD * albedo * ambientEnv * gi.indirect.x * ao;
        // G4：SSGI 与 SH9 同尺度（SSGI shader 输出辐照度等价值）；w=0 时 mix 退化为 envDiffuse。
        vec3 ssgiRadiance = texture(ssgiTex, fragUV).rgb;
        vec3 ssgiDiffuse = kD * albedo * ssgiRadiance * gi.indirect.x * ao;
        float ssgiWeight = clamp(gi.envParams.w, 0.0, 1.0);
        vec3 diffuseIBL = mix(envDiffuse, ssgiDiffuse, ssgiWeight);
        vec3 R = reflect(-V, N);
        float maxMip = max(gi.envParams.y - 1.0, 0.0);
        vec3 prefiltered = textureLod(envPrefilteredCube, R, roughness * maxMip).rgb;
        vec2 ab = texture(brdfLutTex, vec2(NdotV, roughness)).rg;
        vec3 specularIBL = prefiltered * (F0 * ab.x + ab.y) * gi.indirect.y;
        ambient = diffuseIBL + specularIBL;
    }
    vec3 color = ambient + Lo;

    // ── Reinhard 色调映射 (HDR→LDR) ─────────────────
    color = color / (color + vec3(1.0));

    outColor = vec4(mix(fog.fogColor.rgb, color, 1.0 - fogAmountAt(wp)), 1.0);
}
