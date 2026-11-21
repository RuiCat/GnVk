#version 450
// ╔══════════════════════════════════════════════════════════╗
// ║  GN_mini — 延迟光照片段着色器 (PBR Cook-Torrance BRDF)  ║
// ║  读取 4 个 G-Buffer 纹理 → 逐光源 PBR + Skybox IBL     ║
// ║  GGX 法线分布 + Smith 几何遮蔽 + Fresnel-Schlick         ║
// ║  PCSS 软阴影 + CSM 选级（输出线性 HDR，tonemap 在 composite）║
// ╚══════════════════════════════════════════════════════════╝

#define MAX_LIGHTS 8         // 最大光源数 (与 LightUBO 对齐)
#extension GL_GOOGLE_include_directive : require
#define PI 3.14159265359     // π
#include "brdfLighting.glsl"

layout(location = 0) in vec2 fragUV;    // 屏幕空间 UV (全屏三角形)

// ── G-Buffer 输入纹理 ────────────────────────────────
// G2a：附件 1 为 RGBA8_SRGB（采样时硬件自动 sRGB→linear）；附件 2 为 RGBA8_UNORM。
layout(binding = 1) uniform sampler2D gNormalRoughTex; // RGBA16F: RGB=编码法线(N*0.5+0.5), A=粗糙度
layout(binding = 2) uniform sampler2D gAlbedoMetalTex; // RGBA8_SRGB: RGB=albedo(线性), A=金属度
layout(binding = 3) uniform sampler2D gAOTex;          // RGBA8_UNORM: R=AO G=反射率 B=材质ID A=alpha
layout(binding = 4) uniform sampler2D gDepthTex;       // D32 深度 (Vulkan NDC [0,1])

// ── 阴影 + 天空盒 ────────────────────────────────────
// L3c：阴影图改为 2D 数组（0..3 层 = CSM 级，契约 §4）；PCSS 仍手动比较原始深度。
layout(binding = 5) uniform sampler2DArray shadowSampler;
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
    mat4 viewProj;      // 摄像机 VP 矩阵 (用于深度→世界位置重建)
    vec4 cameraPos;     // 摄像机世界位置 (cameraPos)
    vec4 ambientLight;  // 场景环境光 (ambientLight)
} scene;

// ── Light UBO ────────────────────────────────────────
layout(binding = 7) uniform LightUBO {
    Light lights[MAX_LIGHTS];    // 8 个光源 (640B)
    vec4 ambientLight;           // 光源环境光
    float lightCount;            // 实际激活光源数
    float _pad0, _pad1, _pad2;   // padding
    // L3c：CSM 数据（契约 §3.2，std140 偏移 672 / 928 / 944 / 960，总长 976 B）。
    mat4 shadowLightVP[4];       // 每级光源 VP（列主序）
    vec4 shadowParams;           // x=阴影图分辨率, y=级数(1..4), z/w=0
    vec4 cascadeSplits;          // 各级视距分割（米，升序；选级用包含测试）
    vec4 shadowTexelWorld;       // 各级 texel 世界尺寸（米/texel，法线偏移用）
} lightData;

// P11-S3：雾（独立 Weather UBO，绑定 8；density == 0 时无效果）
// L4：扩为 4 个 vec4（64 B，契约 §3.1）；前两个 vec4 是旧指数雾 fallback 字段。
layout(binding = 8) uniform FogUBO {
    vec4 fogColor;
    vec4 fogParams;  // x=density(1/m), y=heightFalloff(1/m), z=baseHeight(m), w=maxOpacity
    vec4 atmosphere;   // xyz=太阳方向（单位、指向太阳）, w=太阳强度（C++ aAtmosphere）
    vec4 atmosphere2;  // x=地面半径(m), y=大气顶半径(m), z=相机高度(m), w=空气透视开关0/1（C++ aAtmosphere2）
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
// L4 大气 LUT（契约 §4：延迟 set0 binding 24/25，GI 槽位不动）；空气透视只用透射率 LUT，
// sky-view LUT 留给后续天空/IBL 一致性复用（v1 延迟光照路径不采样）。
layout(binding = 24) uniform sampler2D transmittanceLut;
layout(binding = 25) uniform sampler2D skyViewLut;

// 公共光照片段（B5d-S25）：本文件引用 SceneUBO/FogUBO/GIUBO/LightUBO 与
// transmittanceLut/shadowSampler，必须在上述声明之后 include（纯数学见 brdfLighting.glsl）。
#include "sceneLighting.glsl"

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
//  L3c：CSM 选级 + L3b PCSS 软阴影（blocker search + 半影半径 PCF；
//  sampler2DArray 手动深度比较，layer = 选中级）
// ═══════════════════════════════════════════════════════════
const float PCSS_SEARCH_TEXELS = 12.0;
const float PCSS_DEPTH_TO_TEXELS = 400.0;
const float PCSS_MIN_TEXELS = 1.0;
const float PCSS_MAX_TEXELS = 24.0;
// PCSS 采样数：12 blocker + 8 PCF（原 16+16）。待真机 A/B；噪点明显时升到 12/12。
const int PCSS_BLOCKER_SAMPLES = 12;
const int PCSS_PCF_SAMPLES = 8;
// L3c 法线偏移：按选中级的 texel 世界尺寸平移世界位置（契约 §8；2 texel 为经验值，待桌面标定）。
const float SHADOW_NORMAL_OFFSET_TEXELS = 2.0;

float pcf(vec4 fls, float layer, vec3 N, vec3 L, vec3 wp) {
    // Vulkan NDC：x/y ∈ [-1,1] → UV [0,1]；z ∈ [0,1] 直接作深度（契约 §8，不重映射）。
    vec3 ndc = fls.xyz / max(fls.w, 0.0001);
    vec2 uv = ndc.xy * 0.5 + 0.5;

    if (any(lessThan(uv, vec2(0.0))) || any(greaterThan(uv, vec2(1.0))) ||
        ndc.z < 0.0 || ndc.z > 1.0)
        return 1.0;

    float bias = max(0.002 * (1.0 - dot(N, L)), 0.0005);
    float sz   = max(lightData.shadowParams.x, 1.0);
    float ts   = 1.0 / sz;
    float receiver = ndc.z - bias;

    float angle = fract(sin(dot(wp.xy, vec2(12.9898, 78.233))) * 43758.5453)
                * 2.0 * PI;
    float ca = cos(angle), sa = sin(angle);

    // 1) 阻塞物搜索
    float blockerSum = 0.0;
    float blockerCount = 0.0;
    for (int i = 0; i < PCSS_BLOCKER_SAMPLES; i++) {
        vec2 o = vec2(pd[i].x * ca - pd[i].y * sa,
                      pd[i].x * sa + pd[i].y * ca);
        float occluder = textureLod(shadowSampler,
                                    vec3(uv + o * ts * PCSS_SEARCH_TEXELS, layer), 0.0).r;
        if (occluder < receiver) {
            blockerSum += occluder;
            blockerCount += 1.0;
        }
    }
    if (blockerCount < 0.5) return 1.0;

    // 2) 半影半径 ∝ 遮挡物-接收面深度差；3) 该半径做 PCF
    float blocker = blockerSum / blockerCount;
    float filterSize = clamp((receiver - blocker) * PCSS_DEPTH_TO_TEXELS,
                             PCSS_MIN_TEXELS, PCSS_MAX_TEXELS);
    float shadow = 0.0;
    for (int i = 0; i < PCSS_PCF_SAMPLES; i++) {
        vec2 o = vec2(pd[i].x * ca - pd[i].y * sa,
                      pd[i].x * sa + pd[i].y * ca);
        shadow += shadowCompare(uv + o * ts * filterSize, layer, receiver);
    }
    return shadow / float(PCSS_PCF_SAMPLES);
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

// 屏幕空间反射（v1，仅金属/低粗糙度）：沿反射方向步进，用深度重建判断命中。
// 未命中返回 false，调用方回退到预滤波环境 cubemap。
bool traceSsr(vec3 wp, vec3 N, vec3 V, out vec3 outColor) {
    outColor = vec3(0.0);
    vec3 R = reflect(-V, N);
    vec3 origin = wp + N * 0.02;
    // 0.08 m 步长 / 64 步 ≈ 5.1 m：保证不会跨过墙面（原 0.30 m 步长会直接跳过）。
    for (int i = 0; i < 64; ++i) {
        float t = 0.05 + float(i) * 0.08;
        vec3 p = origin + R * t;
        vec4 clip = scene.viewProj * vec4(p, 1.0);
        if (clip.w <= 1e-6) break;
        vec3 ndc = clip.xyz / clip.w;
        if (ndc.z < 0.0 || ndc.z > 1.0) break;
        vec2 uv = ndc.xy * 0.5 + 0.5;
        if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) break;
        // 屏幕空间步进里不能用隐式 LOD（循环有 break/continue，非一致控制流）。
        float d = textureLod(gDepthTex, uv, 0.0).r;
        if (d >= 0.9999) continue;
        vec3 sp = wpFromDepth(d, uv);
        float thickness = 0.03 + t * 0.01;
        if (length(sp - p) <= thickness) {
            vec3 hn = normalize(textureLod(gNormalRoughTex, uv, 0.0).rgb * 2.0 - 1.0);
            if (dot(hn, -R) <= 0.0) continue; // 只接受朝向反射射线的表面
            vec3 ha = textureLod(gAlbedoMetalTex, uv, 0.0).rgb;
            // evalSH9 返回 irradiance；Lambertian 出射 radiance 需要 /π。
            outColor = ha * evalSH9(hn) / PI;
            return true;
        }
    }
    return false;
}

// ═══════════════════════════════════════════════════════════
//  PBR Cook-Torrance BRDF 函数
// ═══════════════════════════════════════════════════════════

// GGX/Trowbridge-Reitz 法线分布函数
// Schlick-GGX 几何遮蔽函数
// Smith 联合遮蔽-阴影函数
// Fresnel-Schlick
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
        if (L.attenuation.w > 0.0 && d > L.attenuation.w) return vec3(0.0);
        Ldir = t / max(d, 0.0001);
        att = 1.0 / (L.attenuation.x + L.attenuation.y * d
                   + L.attenuation.z * d * d);
    } else {
        vec3 t = L.position.xyz - wp;
        float d = length(t);
        if (L.attenuation.w > 0.0 && d > L.attenuation.w) return vec3(0.0);
        Ldir = t / max(d, 0.0001);
        float ct = dot(-Ldir, normalize(L.direction.xyz));
        float e  = L.coneAngles.x - L.coneAngles.y;
        float spot = clamp((ct - L.coneAngles.y) / max(e, 0.0001), 0.0, 1.0);
        att = spot / (L.attenuation.x + L.attenuation.y * d
                    + L.attenuation.z * d * d);
    }

    // 注意：延迟路径使用 wrap diffuse（NdotL=0 时仍有 0.3/1.3 的柔和项），
    // 不能在这里对 NdotL<=0 直接早退，否则会改变背光面的观感；只保留点/聚光
    // range 早退。
    float NdotL = max(dot(N, Ldir), 0.0);

    // ── 阴影计算（契约 §8：选级 + 层采样 + 按级 texel 世界尺寸做法线偏移）──
    float sh = 1.0;
    if (st > 0.5 && st < 1.5) {
        int cascade = selectCascade(wp);
        if (cascade >= 0) {
            float texelWorld = lightData.shadowTexelWorld[cascade];
            vec3 shadowWorldPos = wp + N * (texelWorld * SHADOW_NORMAL_OFFSET_TEXELS);
            vec4 fls = lightData.shadowLightVP[cascade] * vec4(shadowWorldPos, 1.0);
            sh = pcf(fls, float(cascade), N, Ldir, wp);
        }
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
        vec3 skyDir = normalize(wpFromDepth(depth, fragUV) - scene.cameraPos.xyz);
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
    vec3 V  = normalize(scene.cameraPos.xyz - wp);

    // ═══════════════════════════════════════════════════════
    //  PBR 直接光照 (Cook-Torrance BRDF, 逐光源)
    // ═══════════════════════════════════════════════════════
    vec3 Lo = vec3(0.0);
    int cnt = int(lightData.lightCount);

    for (int i = 0; i < cnt && i < MAX_LIGHTS; i++) {
        Lo += CalcPBRLightDeferred(lightData.lights[i], N, V, wp,
                                    albedo, roughness, metallic);
    }

    // ═══════════════════════════════════════════════════════
    //  环境光 + IBL
    //  G1b：GI 倍率 > 0 走 split-sum + SH9；为 0（默认）保持接线前观感
    // ═══════════════════════════════════════════════════════
    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    float NdotV = max(dot(N, V), 0.0);

    vec3 ambient;
    vec3 specularIBL = vec3(0.0);
    if (gi.indirect.x + gi.indirect.y <= 0.0) {
        // 分析环境光
        vec3 amb = scene.ambientLight.rgb * lightData.ambientLight.rgb * 3.0;
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
        specularIBL = skySpecular * F * specularOcclusion * 0.5;
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
        // evalSH9 / 探针 / SSGI 都按 irradiance 约定（含 A_l=π 余弦卷积）；Lambert 漫反射 BRDF 需
        // 除以 π，否则环境项比直射光（CalcPBRLightDeferred 内已 /π）亮 π 倍。
        vec3 envDiffuse = kD * albedo * ambientEnv * 0.31830988618 * gi.indirect.x * ao;
        // G4：SSGI 与 SH9 同尺度（SSGI shader 输出辐照度等价值）；w=0 时 mix 退化为 envDiffuse。
        vec3 ssgiRadiance = texture(ssgiTex, fragUV).rgb;
        vec3 ssgiDiffuse = kD * albedo * ssgiRadiance * 0.31830988618 * gi.indirect.x * ao;
        float ssgiWeight = clamp(gi.envParams.w, 0.0, 1.0);
        vec3 diffuseIBL = mix(envDiffuse, ssgiDiffuse, ssgiWeight);
        vec3 R = reflect(-V, N);
        float maxMip = max(gi.envParams.y - 1.0, 0.0);
        vec3 prefiltered = textureLod(envPrefilteredCube, R, roughness * maxMip).rgb;
        vec2 ab = texture(brdfLutTex, vec2(NdotV, roughness)).rg;
        specularIBL = prefiltered * (F0 * ab.x + ab.y) * gi.indirect.y;
        ambient = diffuseIBL + specularIBL;
    }
    // SSR（v1）：只有金属且足够光滑才值得 64 步屏幕空间步进；粗糙金属回退 IBL。
    if (metallic > 0.05 && roughness < 0.6) {
        vec3 ssrColor = vec3(0.0);
        if (traceSsr(wp, N, V, ssrColor)) {
            float ssrWeight = clamp((1.0 - roughness) * metallic, 0.0, 1.0);
            ambient = ambient - specularIBL + mix(specularIBL, ssrColor, ssrWeight);
        }
    }
    vec3 color = ambient + Lo;

    // L1：输出线性 HDR；曝光/色调映射/Bloom 由 gn3d_composite.frag 统一处理。
    // L4/L5：空气透视（AP）只负责远处的物理大气辉光；天气雾（fogParams.x / maxOpacity）
    // 必须独立叠加——否则 FOG/STORM 下 fog.density 被完全绕过（AP 在 1 km 视距只有 ~1e-2 贡献）。
    vec3 baseColor = color;
    if (fog.atmosphere2.w > 0.5 && fog.atmosphere.w > 0.0) {
        baseColor = applyAerialPerspective(color, wp);
    }
    vec3 fogged = mix(fog.fogColor.rgb, baseColor, 1.0 - fogAmountAt(wp));
    outColor = vec4(fogged, 1.0);
}
