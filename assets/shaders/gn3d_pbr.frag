#version 450
// ╔══════════════════════════════════════════════════════════╗
// ║  GN_mini — PBR 片段着色器                                ║
// ║  Cook-Torrance BRDF + GGX + 多光源 + 法线贴图 + PCSS/CSM 阴影 ║
// ╚══════════════════════════════════════════════════════════╝

#define MAX_LIGHTS 8
#extension GL_GOOGLE_include_directive : require
#define PI 3.14159265359
#include "brdfLighting.glsl"
// PCSS 采样数：12 次 blocker 搜索 + 8 次 PCF。待真机 A/B；噪点明显时升到 12/12。
#define PCSS_BLOCKER_SAMPLES 12
#define PCSS_PCF_SAMPLES 8

layout(location = 0) in vec3 fragWorldPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragUV;
layout(location = 3) in mat3 fragTBN;

layout(binding = 0, set = 0) uniform SceneUBO {
    mat4 viewProj;
    vec4 cameraPos;
    vec4 ambientLight;
} scene;

layout(binding = 1, set = 0) uniform sampler2D texSampler;
layout(binding = 3, set = 0) uniform sampler2D normalSampler;  // 法线贴图
// L3c：阴影图改为 2D 数组（0..3 层 = CSM 级，契约 §4）；PCSS 仍手动比较原始深度。
layout(binding = 4, set = 0) uniform sampler2DArray shadowSampler;
// B2：点光/聚光的 cube shadow。与上面的 2D array 是两套资源：
// 4 是方向光的 CSM 级联，12 是点光/聚光的立方图。
//
// **B2 扩展：改成 CUBE ARRAY**，每盏灯占一层（一个完整立方图）。
// 之前是单张 samplerCube —— 一帧里只能给一盏灯投影，后渲染的会覆盖前面的，
// 多灯场景里只有最后一盏有阴影。
layout(binding = 12, set = 0) uniform samplerCubeArray cubeShadowSampler;
layout(binding = 5, set = 0) uniform samplerCube skyboxCubemap;     // 天空盒 IBL

// P11-S3：雾（独立 Weather UBO，绑定 6；density == 0 时无效果）
// L4：扩为 4 个 vec4（64 B，契约 §3.1）；前两个 vec4 是旧指数雾 fallback 字段。
layout(binding = 6, set = 0) uniform FogUBO {
    vec4 fogColor;
    vec4 fogParams;    // x=density(1/m), y=heightFalloff(1/m), z=baseHeight(m), w=maxOpacity
    vec4 atmosphere;   // xyz=太阳方向（单位、指向太阳）, w=太阳强度（C++ aAtmosphere）
    vec4 atmosphere2;  // x=地面半径(m), y=大气顶半径(m), z=相机高度(m), w=空气透视开关0/1（C++ aAtmosphere2）
} fog;

// ── G1b：GI UBO（binding 7）+ 预滤波环境 cubemap（8）+ BRDF LUT（9，RG16F）──
layout(binding = 7, set = 0) uniform GIUBO {
    vec4 indirect;   // x=漫反射倍率 y=镜面倍率 z=AO 强度 w=SSAO 开关
    vec4 aoParams;
    vec4 envParams;  // x=环境旋转 y=预滤波 mip 数
    vec4 sh[9];      // SH9 辐照度系数（rgb + padding）
    mat4 invViewProj;
} gi;
layout(binding = 8, set = 0) uniform samplerCube envPrefilteredCube;
layout(binding = 9, set = 0) uniform sampler2D brdfLutTex;
// L4 大气 LUT（契约 §4：PBR set0 binding 10/11）；空气透视只用透射率 LUT，
// sky-view LUT 留给后续天空/IBL 一致性复用（v1 前向光照路径不采样）。
layout(binding = 10, set = 0) uniform sampler2D transmittanceLut;
layout(binding = 11, set = 0) uniform sampler2D skyViewLut;

// 环境图只有天空（下半球偏暗/为空）。朝上的大平面（地面）反射方向落入下半球时，
// 会把环境图下半球的暗色投到地面，而且这块暗区随视角滑动（看起来像跟随相机的影子）。
// 对朝上表面把反射方向抬到地平线以上，避开下半球；模型表面保持原反射。
vec3 iblReflection(vec3 V, vec3 N) {
    vec3 R = reflect(-V, N);
    if (N.y > 0.7 && R.y < 0.02) {
        R.y = 0.02;
        R = normalize(R);
    }
    return R;
}

struct Light {
    vec4 color;
    vec4 position;
    vec4 direction;
    vec4 attenuation;
    vec4 coneAngles;
};

layout(binding = 2, set = 0) uniform LightUBO {
    Light lights[MAX_LIGHTS];
    vec4 ambientLight;
    float lightCount;
    float _pad0, _pad1, _pad2;
    // L3c：CSM 数据（契约 §3.2，std140 偏移 672 / 1696 / 1712 / 1728）。
    mat4 shadowLightVP[4];  // 每级光源 VP（列主序）
    vec4 shadowParams;      // x=阴影图分辨率, y=级数(1..4), z/w=0
    vec4 cascadeSplits;     // 各级视距分割（米，升序；选级用包含测试）
    vec4 shadowTexelWorld;  // 各级 texel 世界尺寸（米/texel，法线偏移用）
    // B2 扩展：多光源 cube shadow。每项 .x=near, .y=far, .z=层索引(0..3，<0=无)，.w 保留。
    // **顺序必须与 C++ 的 sLIGHT_UBO::cubeShadowRange 完全一致**（std140 下 vec4[4] 是 64 B）。
    vec4 cubeShadowRange[4];
} lightData;

// PBR 模式下 push_constant 重映射:
// specular.x = roughness, specular.y = metallic, specular.z = ao, specular.w = alphaCutoff
layout(push_constant) uniform ModelPC {
    mat4 model;
    vec4 diffuse;       // albedo
    vec4 specular;      // .x=roughness, .y=metallic, .z=ao, .w=alphaCutoff（0=不裁）
    vec4 emissive;      // .rgb=自发光颜色（线性）, .a=强度倍率
} pc;

// 本文件用了 cube shadow（点光/聚光）⇒ 打开 sceneLighting.glsl 里的那段守卫。
// 必须在 include **之前**定义（未定义的 shader 不编译 cubeShadowFactor）。
#define GN_HAS_CUBE_SHADOW_ARRAY 1
// 公共光照片段（B5d-S25）：本文件引用 SceneUBO/FogUBO/GIUBO/LightUBO 与
// transmittanceLut/shadowSampler，必须在上述声明之后 include（纯数学见 brdfLighting.glsl）。
#include "sceneLighting.glsl"

layout(location = 0) out vec4 outColor;

// ═══════════════════════════════════════════════════════════
//  PBR 函数
// ═══════════════════════════════════════════════════════════

// GGX/Trowbridge-Reitz 法线分布函数
// Schlick-GGX 几何遮蔽函数
// Smith 联合遮蔽-阴影函数
// Fresnel-Schlick
// ═══════════════════════════════════════════════════════════
//  Poisson 圆盘采样点 (16 点, 单位圆盘内分布)
//  用于 PCF 软阴影 — 替代固定 3×3 网格
// ═══════════════════════════════════════════════════════════
const vec2 poissonDisk[16] = vec2[](
    vec2(-0.94201624, -0.39906216),
    vec2( 0.94558609, -0.76890725),
    vec2(-0.094184101, -0.92938870),
    vec2( 0.34495938,  0.29387760),
    vec2(-0.91588581,  0.45771432),
    vec2(-0.81544232, -0.87912464),
    vec2(-0.38277543,  0.27676845),
    vec2( 0.97484398,  0.75648379),
    vec2( 0.44323325, -0.97511554),
    vec2( 0.53742981, -0.47373420),
    vec2(-0.26496911, -0.41893023),
    vec2( 0.79197514,  0.19090188),
    vec2(-0.24188840,  0.99706507),
    vec2(-0.81409955,  0.91437590),
    vec2( 0.19984126,  0.78641367),
    vec2( 0.14383161, -0.14100790)
);

// ═══════════════════════════════════════════════════════════
//  阴影映射（L3c：CSM 选级 + L3b PCSS 接触硬化软阴影；sampler2DArray 手动深度比较）
// ═══════════════════════════════════════════════════════════
// blocker search / PCF 半径参数为经验值（按 2048² 阴影图），待桌面标定。

const float PCSS_SEARCH_TEXELS = 12.0;
const float PCSS_DEPTH_TO_TEXELS = 400.0;
const float PCSS_MIN_TEXELS = 1.0;
const float PCSS_MAX_TEXELS = 24.0;
// L3c 法线偏移：按选中级的 texel 世界尺寸平移世界位置（契约 §8；2 texel 为经验值，待桌面标定）。
const float SHADOW_NORMAL_OFFSET_TEXELS = 2.0;

float ShadowCalculation(vec4 fragPosLightSpace, float layer, vec3 N, vec3 L) {
    // 透视除法 → Vulkan NDC：x/y ∈ [-1,1]、z ∈ [0,1]（契约 §8；OrthoVK 深度不重映射）。
    vec3 ndc = fragPosLightSpace.xyz / max(fragPosLightSpace.w, 0.0001);
    vec2 uv = ndc.xy * 0.5 + 0.5;

    // 超出阴影贴图范围 → 无阴影
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0 ||
        ndc.z < 0.0 || ndc.z > 1.0) {
        return 1.0;
    }

    // 偏移避免阴影痤疮（法线偏移在调用点按 shadowTexelWorld[cascade] 做，这里保留深度偏移）
    float bias = max(0.002 * (1.0 - dot(N, L)), 0.0005);
    float smSize = max(lightData.shadowParams.x, 1.0);
    float texel = 1.0 / smSize;
    float receiver = ndc.z - bias;

    // 随机旋转 (基于片元世界坐标, 避免带状伪影)
    float angle = fract(sin(dot(fragWorldPos.xy, vec2(12.9898, 78.233))) * 43758.5453) * 2.0 * PI;
    float cosA = cos(angle);
    float sinA = sin(angle);

    // ── 1) 阻塞物搜索：平均遮挡深度 ──
    float blockerSum = 0.0;
    float blockerCount = 0.0;
    for (int i = 0; i < PCSS_BLOCKER_SAMPLES; i++) {
        vec2 diskOffset = vec2(
            poissonDisk[i].x * cosA - poissonDisk[i].y * sinA,
            poissonDisk[i].x * sinA + poissonDisk[i].y * cosA
        );
        float occluder = textureLod(shadowSampler,
                                    vec3(uv + diskOffset * texel * PCSS_SEARCH_TEXELS, layer),
                                    0.0).r;
        if (occluder < receiver) {
            blockerSum += occluder;
            blockerCount += 1.0;
        }
    }
    if (blockerCount < 0.5) return 1.0; // 半影内无遮挡
    float blocker = blockerSum / blockerCount;

    // ── 2) 半影半径 ∝ 遮挡物-接收面距离；3) 该半径上做 PCF ──
    float filterSize = clamp((receiver - blocker) * PCSS_DEPTH_TO_TEXELS,
                             PCSS_MIN_TEXELS, PCSS_MAX_TEXELS);
    float shadow = 0.0;
    for (int i = 0; i < PCSS_PCF_SAMPLES; i++) {
        vec2 diskOffset = vec2(
            poissonDisk[i].x * cosA - poissonDisk[i].y * sinA,
            poissonDisk[i].x * sinA + poissonDisk[i].y * cosA
        );
        shadow += shadowCompare(uv + diskOffset * texel * filterSize, layer, receiver);
    }
    return shadow / float(PCSS_PCF_SAMPLES);
}

// 计算单个光源的 PBR 贡献 (含阴影)
// shadowType (direction.w):
//   0.0 = 无阴影 (点光源默认)
//   1.0 = 2D shadow map (方向光/聚光灯)
//   2.0 = cube shadow map (点光源 — 预留)
vec3 CalcPBRLight(Light L, vec3 N, vec3 V, vec3 albedo, float roughness, float metallic, vec3 F0,
                  int lightSlot) {
    vec3 lightColor = L.color.rgb * L.color.a;
    vec3 Ldir;
    float attenuation = 1.0;
    float shadowType = L.direction.w;  // 0=none, 1=2D, 2=cube(reserved)
    
    if (L.position.w < 0.5) {
        // 方向光
        Ldir = normalize(-L.direction.xyz);
    } else if (L.position.w < 1.5) {
        // 点光源
        vec3 toLight = L.position.xyz - fragWorldPos;
        float dist = length(toLight);
        if (L.attenuation.w > 0.0 && dist > L.attenuation.w) return vec3(0.0);
        Ldir = toLight / max(dist, 0.0001);
        attenuation = 1.0 / (L.attenuation.x + L.attenuation.y * dist + L.attenuation.z * dist * dist);
    } else {
        // 聚光灯
        vec3 toLight = L.position.xyz - fragWorldPos;
        float dist = length(toLight);
        if (L.attenuation.w > 0.0 && dist > L.attenuation.w) return vec3(0.0);
        Ldir = toLight / max(dist, 0.0001);
        float theta = dot(-Ldir, normalize(L.direction.xyz));
        float epsilon = L.coneAngles.x - L.coneAngles.y;
        float spot = clamp((theta - L.coneAngles.y) / max(epsilon, 0.0001), 0.0, 1.0);
        attenuation = spot / (L.attenuation.x + L.attenuation.y * dist + L.attenuation.z * dist * dist);
    }

    float NdotL = max(dot(N, Ldir), 0.0);
    if (NdotL <= 0.0) return vec3(0.0);

    // 阴影计算 (基于 shadowType)
    float shadow = 1.0;
    if (shadowType > 0.5 && shadowType < 1.5) {
        // ── CSM（契约 §8）：先按近到远包含测试选级，再按该级 texel 世界尺寸做法线偏移 ──
        int cascade = selectCascade(fragWorldPos);
        if (cascade >= 0) {
            float texelWorld = lightData.shadowTexelWorld[cascade];
            vec3 shadowWorldPos = fragWorldPos + N * (texelWorld * SHADOW_NORMAL_OFFSET_TEXELS);
            vec4 fragPosLight = lightData.shadowLightVP[cascade] * vec4(shadowWorldPos, 1.0);
            shadow = ShadowCalculation(fragPosLight, float(cascade), N, Ldir);
        }
    }
    // B2：点光/聚光走 cube shadow。shadowType == 2 表示"这盏灯有 cube shadow"。
    // B2 扩展：**每盏灯有自己的一层与自己的深度范围**（lightData.cubeShadowRange[slot]）。
    // 之前是全局 shadowParams.z/w + 单张 cube map ⇒ 多灯里只有最后一盏对得上。
    else if (shadowType > 1.5 && shadowType < 2.5) {
        // B2b：PCSS。半影宽度由**光源物理半径**（L.coneAngles.z）与遮挡物-接收面距离决定，
        // 不再是一个写死的软度常数 —— 遮挡物贴近接收面时边缘自然变锐（接触硬化）。
        // 半径 0 表示调用方没设：给 0.1 m（约等于一个灯泡的灯丝尺度）。
        float lightRadius = L.coneAngles.z;
        if (lightRadius <= 0.0) lightRadius = 0.1;
        // .x = near, .y = far, .z = 层索引（<0 表示这盏灯没有可用的 cube shadow）
        vec4 cubeInfo = lightData.cubeShadowRange[lightSlot];
        shadow = cubeShadowFactor(fragWorldPos, L.position.xyz, N, lightRadius,
                                  cubeInfo.x, cubeInfo.y, int(cubeInfo.z));
    }
    
    vec3 H = normalize(V + Ldir);
    vec3 radiance = lightColor * attenuation * shadow;
    
    // Cook-Torrance BRDF
    float NDF = DistributionGGX(N, H, roughness);
    float G   = GeometrySmith(N, V, Ldir, roughness);
    vec3  F   = FresnelSchlick(max(dot(H, V), 0.0), F0);
    
    vec3 numerator    = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, Ldir), 0.0) + 0.0001;
    vec3 specularBRDF = numerator / denominator;
    
    vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);
    
    return (kD * albedo / PI + specularBRDF) * radiance * NdotL;
}

// ═══════════════════════════════════════════════════════════
//  主函数
// ═══════════════════════════════════════════════════════════

void main() {
    // 所有带 mip 的纹理采样放在 discard 之前：discard 会制造非一致控制流，
    // 之后的隐式 LOD 计算是未定义的（alpha cutout 边缘会闪烁）。
    vec4 albedoSample = texture(texSampler, fragUV);
    vec3 texNormal = texture(normalSampler, fragUV).rgb;
    float alphaCutoff = pc.specular.w;
    float alpha = albedoSample.a * pc.diffuse.a;
    if (alphaCutoff > 0.0 && alpha < alphaCutoff) discard;

    // 默认 1×1 白图不是合法切线空间法线（解码后 normalize(1,1,1) 偏 45°），按无贴图处理。
    vec3 N = normalize(fragNormal);
    if (!all(greaterThan(texNormal, vec3(0.999))) && length(texNormal) > 0.01) {
        // 从 [0,1] 映射到 [-1,1]
        texNormal = normalize(texNormal * 2.0 - 1.0);
        N = normalize(fragTBN * texNormal);
    }

    vec3 V = normalize(scene.cameraPos.xyz - fragWorldPos);
    float NdotV = max(dot(N, V), 0.0);

    // 材质参数（albedo 已在 discard 前采好，避免重复采样）
    vec3 albedo = albedoSample.rgb * pc.diffuse.rgb;
    float roughness = clamp(pc.specular.x, 0.04, 1.0);
    float metallic  = clamp(pc.specular.y, 0.0, 1.0);
    float ao        = clamp(pc.specular.z, 0.0, 1.0);
    
    // 基础反射率: 非金属 ~0.04, 纯金属 = albedo
    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    
    // 对每个光源计算 PBR (含阴影)
    vec3 Lo = vec3(0.0);
    int count = int(lightData.lightCount);
    for (int i = 0; i < count && i < MAX_LIGHTS; i++) {
        Lo += CalcPBRLight(lightData.lights[i], N, V, albedo, roughness, metallic, F0, i);
    }
    
    // ═══════════════════════════════════════════════════════
    //  Skybox-based IBL (基于天空盒 Cubemap 的全局照明)
    //  G1b：GI 倍率 > 0 走 split-sum + SH9；为 0（默认）保持接线前观感
    // ═══════════════════════════════════════════════════════
    vec3 ambient;
    if (gi.indirect.x + gi.indirect.y <= 0.0) {
        vec3 ambientColor = scene.ambientLight.rgb * lightData.ambientLight.rgb;
        // 采样天空盒: 法线方向 (漫反射) + 反射方向 (镜面反射)
        vec3 skyDiffuse  = texture(skyboxCubemap, N).rgb;
        vec3 R = iblReflection(V, N);
        vec3 skySpecular = texture(skyboxCubemap, R).rgb;
        // Fresnel → 镜面比例；漫反射 IBL 用 F0 做能量分配（不随视角在掠射角归零），
        // 否则地面在掠射角会整片变暗，形成随相机滑动的暗带。
        vec3 F = FresnelSchlick(NdotV, F0);
        vec3 kD = (vec3(1.0) - F0) * (1.0 - metallic);
        // ─ 漫反射 IBL: 天空漫反射 × albedo, 混合分析环境光作为保底
        vec3 diffuseIBL = kD * albedo * mix(ambientColor, skyDiffuse, 0.7) * ao;
        // ─ 镜面反射 IBL: 天空反射 × Fresnel(非F0!) × 粗糙度调制
        float specularOcclusion = 1.0 - roughness * 0.7;
        vec3 specularIBL = skySpecular * F * specularOcclusion * 0.5;
        ambient = diffuseIBL + specularIBL;
    } else {
        // ─ split-sum IBL：SH9 漫反射 + 预滤波 cubemap × BRDF LUT 镜面 ─
        vec3 F = FresnelSchlick(NdotV, F0);
        // 漫反射求和的能量项用 F0 近似（常见 IBL 做法）：用 NdotV 的 Fresnel 会让
        // 掠射角 kD→0，地面出现随视角/相机位置滑动的暗带。
        vec3 kD = (vec3(1.0) - F0) * (1.0 - metallic);
        // evalSH9 返回 irradiance（已含余弦卷积 A_l=π）；Lambert 漫反射 BRDF 的 1/π 必须在此补上，
        // 否则环境项比直射光（CalcPBRLight 内已 /π）亮 π 倍。
        vec3 diffuseIBL = kD * albedo * evalSH9(N) * 0.31830988618 * gi.indirect.x * ao;
        vec3 R = iblReflection(V, N);
        float maxMip = max(gi.envParams.y - 1.0, 0.0);
        vec3 prefiltered = textureLod(envPrefilteredCube, R, roughness * maxMip).rgb;
        vec2 ab = texture(brdfLutTex, vec2(NdotV, roughness)).rg;
        vec3 specularIBL = prefiltered * (F0 * ab.x + ab.y) * gi.indirect.y;
        ambient = diffuseIBL + specularIBL;
    }
    
    vec3 color = ambient + Lo;

    // 自发光（L1c）：不参与光照计算，直接加到出射辐射上。
    // * 乘 .a 是强度倍率，默认 0 ⇒ 与加入本字段之前逐位一致（既有画面不受影响）；
    // * 加在雾之前：自发光表面同样被大气/天气雾遮蔽，物理上更对；
    // * 数值可以是 HDR（>1），Bloom 在 composite 阶段按阈值抓取。
    color += pc.emissive.rgb * pc.emissive.a;
    
    // L1：着色器输出线性 HDR；曝光/色调映射/Bloom 统一在 gn3d_composite.frag 做一次。
    
    // 注：输出为线性 HDR；sRGB/非 sRGB 交换链的色彩编码统一由 composite 处理。
    
    // 输出 alpha：BLEND 管线（srcAlpha/oneMinusSrcAlpha）靠它做透明；不透明/MASK
    // 管线忽略 alpha 通道，行为不变。雾只混 rgb，不把透明片元的不透明度拉满。
    float outAlpha = alpha;
    // L4/L5：空气透视（AP）只负责远处的物理大气辉光；天气雾（fogParams.x / maxOpacity）
    // 必须独立叠加——否则 FOG/STORM 下 fog.density 被完全绕过：AP 的散射系数是物理量级
    // （~1e-5 /m），1 km 视距只有 ~1e-2 的贡献，等于"雾没有效果"。
    vec3 baseColor = color;
    if (fog.atmosphere2.w > 0.5 && fog.atmosphere.w > 0.0) {
        baseColor = applyAerialPerspective(color, fragWorldPos);
    }
    vec3 fogged = mix(fog.fogColor.rgb, baseColor, 1.0 - fogAmountAt(fragWorldPos));
    outColor = vec4(fogged, outAlpha);
}
