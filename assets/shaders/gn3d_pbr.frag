#version 450
// ╔══════════════════════════════════════════════════════════╗
// ║  GN_mini — PBR 片段着色器                                ║
// ║  Cook-Torrance BRDF + GGX + 多光源 + 法线贴图 + PCF阴影  ║
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
layout(binding = 4, set = 0) uniform sampler2DShadow shadowSampler; // 阴影贴图
layout(binding = 5, set = 0) uniform samplerCube skyboxCubemap;     // 天空盒 IBL

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
    mat4 shadowLightVP;     // 光源 VP 矩阵 (用于阴影映射)
    float shadowMapSize;    // Shadow Map 分辨率
    float _pad3, _pad4, _pad5;
} lightData;

// PBR 模式下 push_constant 重映射:
// specular.x = roughness, specular.y = metallic, specular.z = ao
layout(push_constant) uniform ModelPC {
    mat4 model;
    vec4 diffuse;       // albedo
    vec4 specular;      // .x=roughness, .y=metallic, .z=ao
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
//  阴影映射 (Poisson 圆盘 PCF 软阴影)
// ═══════════════════════════════════════════════════════════

float ShadowCalculation(vec4 fragPosLightSpace, vec3 N, vec3 L) {
    // 透视除法 → NDC
    vec3 projCoords = fragPosLightSpace.xyz / max(fragPosLightSpace.w, 0.0001);
    
    // NDC [-1,1] → UV [0,1]
    projCoords = projCoords * 0.5 + 0.5;
    
    // 超出阴影贴图范围 → 无阴影
    if (projCoords.x < 0.0 || projCoords.x > 1.0 ||
        projCoords.y < 0.0 || projCoords.y > 1.0 ||
        projCoords.z < 0.0) {
        return 1.0;
    }
    
    // 偏移避免阴影痤疮 (基于法线偏移)
    float bias = max(0.002 * (1.0 - dot(N, L)), 0.0005);
    float smSize = max(lightData.shadowMapSize, 1.0);
    vec2 texelSize = vec2(1.0 / smSize);
    
    // ── Poisson 圆盘 PCF ──
    // 随机旋转 (基于片元世界坐标, 避免带状伪影)
    float angle = fract(sin(dot(fragWorldPos.xy, vec2(12.9898, 78.233))) * 43758.5453) * 2.0 * PI;
    float cosA = cos(angle);
    float sinA = sin(angle);
    
    float shadow = 0.0;
    float filterSize = 1.5;  // 可调: 值越大阴影越软
    
    for (int i = 0; i < SHADOW_PCF_SAMPLES; i++) {
        // 旋转 Poisson 采样点
        vec2 diskOffset = vec2(
            poissonDisk[i].x * cosA - poissonDisk[i].y * sinA,
            poissonDisk[i].x * sinA + poissonDisk[i].y * cosA
        );
        vec3 pcfCoord = vec3(projCoords.xy + diskOffset * texelSize * filterSize,
                             projCoords.z - bias);
        shadow += texture(shadowSampler, pcfCoord);
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
        // ── 2D shadow map (方向光/聚光灯) ──
        vec4 fragPosLight = lightData.shadowLightVP * vec4(fragWorldPos, 1.0);
        shadow = ShadowCalculation(fragPosLight, N, Ldir);
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
    //  替代纯分析近似, 使用实际天空颜色
    // ═══════════════════════════════════════════════════════
    vec3 ambientColor = scene.ambientLight.rgb * lightData.ambientLight.rgb;

    // 采样天空盒: 法线方向 (漫反射) + 反射方向 (镜面反射)
    vec3 skyDiffuse  = texture(skyboxCubemap, N).rgb;
    vec3 R = reflect(-V, N);
    vec3 skySpecular = texture(skyboxCubemap, R).rgb;

    // Fresnel → 漫反射/镜面反射比例
    vec3 F = FresnelSchlick(NdotV, F0);
    vec3 kS = F;
    vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);

    // ─ 漫反射 IBL: 天空漫反射 × albedo, 混合分析环境光作为保底
    vec3 diffuseIBL = kD * albedo * mix(ambientColor, skyDiffuse, 0.7) * ao;

    // ─ 镜面反射 IBL: 天空反射 × Fresnel(非F0!) × 粗糙度调制
    float specularOcclusion = 1.0 - roughness * 0.7;
    vec3 specularIBL = skySpecular * F * specularOcclusion * 0.5;

    vec3 ambient = diffuseIBL + specularIBL;
    
    vec3 color = ambient + Lo;
    
    // Reinhard 色调映射 (HDR→LDR)
    color = color / (color + vec3(1.0));
    
    // 注: swapchain 为 SRGB 格式, 硬件自动 Gamma 编码, 无需手动 pow(1/2.2)
    
    outColor = vec4(color, 1.0);
}
