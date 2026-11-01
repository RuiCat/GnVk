#version 450
// ╔══════════════════════════════════════════════════════════╗
// ║  GN_mini — PBR 片段着色器                                ║
// ║  Cook-Torrance BRDF + GGX + 多光源 + 法线贴图 + PCSS/CSM 阴影 ║
// ╚══════════════════════════════════════════════════════════╝

#define MAX_LIGHTS 8
#define PI 3.14159265359
#define SHADOW_PCF_SAMPLES 16

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

float fogAmountAt(vec3 worldPosition) {
    float distanceToCamera = length(worldPosition - scene.cameraPos.xyz);
    float heightScale = exp(-fog.fogParams.y * max(worldPosition.y - fog.fogParams.z, 0.0));
    float amount = 1.0 - exp(-fog.fogParams.x * heightScale * distanceToCamera);
    return clamp(amount, 0.0, fog.fogParams.w);
}

// ═══════════════════════════════════════════════════════════
//  L4：空气透视（契约 §6；8 步单次散射解析积分，非 froxel/LUT）
//  常量逐值抄自 atmosphereParams.cpp::makeDefaultAtmosphereParams（契约 §7）。
// ═══════════════════════════════════════════════════════════
const vec3 RAYLEIGH_SCATTERING = vec3(5.802e-6, 13.558e-6, 33.100e-6); // σR（1/m）
const float RAYLEIGH_SCALE_HEIGHT = 8000.0;  // Hr（m）
const float MIE_SCATTERING = 3.996e-6;       // σM 散射（1/m）
const float MIE_ABSORPTION = 4.400e-6;       // σM 吸收（1/m）
const float MIE_EXTINCTION = MIE_SCATTERING + MIE_ABSORPTION; // 消光 = 散射 + 吸收
const float MIE_SCALE_HEIGHT = 1200.0;       // Hm（m）
const float MIE_ANISOTROPY = 0.8;            // HG 各向异性 g
const vec3 OZONE_ABSORPTION = vec3(0.650e-6, 1.881e-6, 0.085e-6); // σO（1/m）
const float OZONE_CENTER_HEIGHT = 25000.0;   // 臭氧帐篷中心（m）
const float OZONE_WIDTH = 15000.0;           // 臭氧帐篷半宽（m）
const int AERIAL_STEPS = 8;                  // 单次散射积分步数（契约 §6）

// Rayleigh 相位：(3/16π)(1 + mu²)。
float rayleighPhase(float mu) {
    return (3.0 / (16.0 * PI)) * (1.0 + mu * mu);
}

// Henyey-Greenstein 近似（Mie 相位；与 atmosphereParams.cpp miePhase 同式）。
float miePhase(float mu, float g) {
    float g2 = g * g;
    float denom = max(1.0 + g2 - 2.0 * g * mu, 1e-6);
    return (1.0 - g2) / (4.0 * PI * denom * sqrt(denom));
}

// 臭氧帐篷密度（与 computeTransmittance 的 dO 同式）。
float ozoneDensity(float height) {
    return max(0.0, 1.0 - abs(height - OZONE_CENTER_HEIGHT) / OZONE_WIDTH);
}

// 透射率 LUT 查表（契约 §6：u = mu*0.5+0.5、v = h/(Rt-Rg)；与 evalTransmittanceLut 同一参数化）。
vec3 sampleSunTransmittance(vec3 p, vec3 planetCenter, float groundRadius,
                            float atmosphereRadius, vec3 sunDir) {
    vec3 up = normalize(p - planetCenter);
    float height = max(length(p - planetCenter) - groundRadius, 0.0);
    float mu = dot(up, sunDir);
    float height01 = clamp(height / max(atmosphereRadius - groundRadius, 1.0), 0.0, 1.0);
    return textureLod(transmittanceLut, vec2(mu * 0.5 + 0.5, height01), 0.0).rgb;
}

// 契约 §6：沿相机→片元射线做 8 步单次散射积分，替换指数雾。
// 返回 litColor * T_view + S * sunIntensity，并按 aParams[3]（maxOpacity）限制最终混合比例。
vec3 applyAerialPerspective(vec3 litColor, vec3 worldPos) {
    vec3 cameraPos = scene.cameraPos.xyz;
    vec3 delta = worldPos - cameraPos;
    float tMax = length(delta);
    if (tMax < 1e-4) return litColor;
    vec3 viewDir = delta / tMax;

    // 行星中心：世界 y-up；用 aAtmosphere2.z（相机离地高度）反推地面平面
    // y = cameraPos.y - cameraHeight，使相机高度恰为 CPU 给的值。groundHeight=0 时
    // C.y = -Rg，与契约 §6 一致；地面不在 y=0 时也不会把高度积分整体偏移。
    float groundRadius = max(fog.atmosphere2.x, 1.0);
    float atmosphereRadius = max(fog.atmosphere2.y, groundRadius + 1.0);
    float cameraHeight = max(fog.atmosphere2.z, 0.0);
    vec3 planetCenter = vec3(0.0, cameraPos.y - cameraHeight - groundRadius, 0.0);
    vec3 sunDir = normalize(fog.atmosphere.xyz);
    float sunIntensity = max(fog.atmosphere.w, 0.0);

    float muView = dot(viewDir, sunDir);
    float phaseR = rayleighPhase(muView);
    float phaseM = miePhase(muView, MIE_ANISOTROPY);

    float stepLen = tMax / float(AERIAL_STEPS);
    vec3 viewTransmittance = vec3(1.0);
    vec3 inscatter = vec3(0.0);
    for (int i = 0; i < AERIAL_STEPS; ++i) {
        vec3 p = cameraPos + viewDir * ((float(i) + 0.5) * stepLen);
        float height = max(length(p - planetCenter) - groundRadius, 0.0);
        float dR = exp(-height / RAYLEIGH_SCALE_HEIGHT);
        float dM = exp(-height / MIE_SCALE_HEIGHT);
        float dO = ozoneDensity(height);
        // 太阳可见性：该点沿太阳方向的透射率 LUT。
        vec3 sunT = sampleSunTransmittance(p, planetCenter, groundRadius, atmosphereRadius, sunDir);
        // 单次散射累加（Rayleigh + Mie 相位；与 computeSkyRadiance 同式）。
        vec3 scatter = RAYLEIGH_SCATTERING * (dR * phaseR) + vec3(MIE_SCATTERING * dM * phaseM);
        inscatter += viewTransmittance * scatter * sunT * stepLen;
        // 光学深度递增：σR*dR + σM_ext*dM + σO*hOzone(h)（契约 §6/§7）。
        vec3 extinction = RAYLEIGH_SCATTERING * dR + vec3(MIE_EXTINCTION * dM)
                        + OZONE_ABSORPTION * dO;
        viewTransmittance *= exp(-extinction * stepLen);
    }
    viewTransmittance = clamp(viewTransmittance, 0.0, 1.0);
    vec3 aerialColor = litColor * viewTransmittance + inscatter * sunIntensity; // 契约 §6

    // 契约 §6：aParams[3] 对空气透视结果的作用 —— 最终混合比例不超过 maxOpacity
    // （cap=1 时等价于物理结果；cap<1 时整体按比例退化为原色，便于与旧指数雾对照）。
    float opacity = clamp(1.0 - min(viewTransmittance.r,
                            min(viewTransmittance.g, viewTransmittance.b)), 0.0, 1.0);
    float cap = clamp(fog.fogParams.w, 0.0, 1.0);
    float blend = (opacity > 1e-5) ? min(1.0, cap / opacity) : 1.0;
    return mix(litColor, aerialColor, blend);
}

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
} lightData;

// PBR 模式下 push_constant 重映射:
// specular.x = roughness, specular.y = metallic, specular.z = ao, specular.w = alphaCutoff
layout(push_constant) uniform ModelPC {
    mat4 model;
    vec4 diffuse;       // albedo
    vec4 specular;      // .x=roughness, .y=metallic, .z=ao, .w=alphaCutoff（0=不裁）
} pc;

layout(location = 0) out vec4 outColor;

// ═══════════════════════════════════════════════════════════
//  PBR 函数
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

// 契约 §8：i=0..count-1 由近到远做包含测试（Vulkan NDC x/y/z ∈ [0,1]）；
// 返回第一个命中级，都不命中返回 -1（无阴影）。
int selectCascade(vec3 worldPos) {
    int cascadeCount = int(clamp(lightData.shadowParams.y, 1.0, 4.0));
    for (int i = 0; i < cascadeCount; ++i) {
        vec4 clip = lightData.shadowLightVP[i] * vec4(worldPos, 1.0);
        if (clip.w <= 0.0) continue;
        vec3 ndc = clip.xyz / clip.w;
        if (ndc.x >= 0.0 && ndc.x <= 1.0 &&
            ndc.y >= 0.0 && ndc.y <= 1.0 &&
            ndc.z >= 0.0 && ndc.z <= 1.0) {
            return i;
        }
    }
    return -1;
}

// 原始深度比较：receiver <= occluder 视为受光（layer = 选中的 CSM 级）。
float shadowCompare(vec2 uv, float layer, float receiver) {
    float occluder = textureLod(shadowSampler, vec3(uv, layer), 0.0).r;
    return receiver <= occluder ? 1.0 : 0.0;
}

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
    for (int i = 0; i < SHADOW_PCF_SAMPLES; i++) {
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
    for (int i = 0; i < SHADOW_PCF_SAMPLES; i++) {
        vec2 diskOffset = vec2(
            poissonDisk[i].x * cosA - poissonDisk[i].y * sinA,
            poissonDisk[i].x * sinA + poissonDisk[i].y * cosA
        );
        shadow += shadowCompare(uv + diskOffset * texel * filterSize, layer, receiver);
    }
    return shadow / float(SHADOW_PCF_SAMPLES);
}

// 计算单个光源的 PBR 贡献 (含阴影)
// shadowType (direction.w):
//   0.0 = 无阴影 (点光源默认)
//   1.0 = 2D shadow map (方向光/聚光灯)
//   2.0 = cube shadow map (点光源 — 预留)
vec3 CalcPBRLight(Light L, vec3 N, vec3 V, vec3 albedo, float roughness, float metallic, vec3 F0) {
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
        Ldir = toLight / max(dist, 0.0001);
        attenuation = 1.0 / (L.attenuation.x + L.attenuation.y * dist + L.attenuation.z * dist * dist);
    } else {
        // 聚光灯
        vec3 toLight = L.position.xyz - fragWorldPos;
        float dist = length(toLight);
        Ldir = toLight / max(dist, 0.0001);
        float theta = dot(-Ldir, normalize(L.direction.xyz));
        float epsilon = L.coneAngles.x - L.coneAngles.y;
        float spot = clamp((theta - L.coneAngles.y) / max(epsilon, 0.0001), 0.0, 1.0);
        attenuation = spot / (L.attenuation.x + L.attenuation.y * dist + L.attenuation.z * dist * dist);
    }
    
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
    // shadowType >= 1.5: cube shadow map (预留, 需要额外采样器)
    
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
    float NdotL = max(dot(N, Ldir), 0.0);
    
    return (kD * albedo / PI + specularBRDF) * radiance * NdotL;
}

// ═══════════════════════════════════════════════════════════
//  主函数
// ═══════════════════════════════════════════════════════════

void main() {
    // 材质 alpha test（MASK）：cutoff=0 时不裁，与接线前行为一致。
    // 放在最前面，被裁掉的片元不进入法线/光照/阴影采样。
    float alphaCutoff = pc.specular.w;
    if (alphaCutoff > 0.0) {
        float alpha = texture(texSampler, fragUV).a * pc.diffuse.a;
        if (alpha < alphaCutoff) discard;
    }

    // 法线贴图采样
    vec3 N;
    vec3 texNormal = texture(normalSampler, fragUV).rgb;
    if (length(texNormal) > 0.01) {
        // 从 [0,1] 映射到 [-1,1]
        texNormal = normalize(texNormal * 2.0 - 1.0);
        N = normalize(fragTBN * texNormal);
    } else {
        N = normalize(fragNormal);
    }
    
    vec3 V = normalize(scene.cameraPos.xyz - fragWorldPos);
    float NdotV = max(dot(N, V), 0.0);
    
    // 材质参数
    vec3 albedo = texture(texSampler, fragUV).rgb * pc.diffuse.rgb;
    float roughness = clamp(pc.specular.x, 0.04, 1.0);
    float metallic  = clamp(pc.specular.y, 0.0, 1.0);
    float ao        = clamp(pc.specular.z, 0.0, 1.0);
    
    // 基础反射率: 非金属 ~0.04, 纯金属 = albedo
    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    
    // 对每个光源计算 PBR (含阴影)
    vec3 Lo = vec3(0.0);
    int count = int(lightData.lightCount);
    for (int i = 0; i < count && i < MAX_LIGHTS; i++) {
        Lo += CalcPBRLight(lightData.lights[i], N, V, albedo, roughness, metallic, F0);
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
    
    // L1：着色器输出线性 HDR；曝光/色调映射/Bloom 统一在 gn3d_composite.frag 做一次。
    
    // 注: swapchain 为 SRGB 格式, 硬件自动 Gamma 编码, 无需手动 pow(1/2.2)
    
    // 输出 alpha：BLEND 管线（srcAlpha/oneMinusSrcAlpha）靠它做透明；不透明/MASK
    // 管线忽略 alpha 通道，行为不变。雾只混 rgb，不把透明片元的不透明度拉满。
    float outAlpha = texture(texSampler, fragUV).a * pc.diffuse.a;
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
