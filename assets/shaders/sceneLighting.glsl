// sceneLighting.glsl — pbr/deferred 两条光照路径共用的 UBO 依赖光照片段（B5d-S25）
//
// 内含 evalSH9 / fogAmountAt / 空气透视（含 sampleSunTransmittance）/
// CSM 选级 selectCascade / 原始深度比较 shadowCompare。
// 这些函数直接引用 SceneUBO(scene) / FogUBO(fog) / GIUBO(gi) / LightUBO(lightData)
// 与 transmittanceLut / shadowSampler 采样器，因此调用方必须在声明这些 UBO 与采样器之后
// #include 本文件（纯数学部分见 brdfLighting.glsl，它在 #version 之后即可 include）。
// 依赖 brdfLighting.glsl 的 rayleighPhase / miePhase / ozoneDensity 与调用方定义的 PI。
#ifndef SCENE_LIGHTING_GLSL
#define SCENE_LIGHTING_GLSL

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
const int AERIAL_STEPS = 8;                  // 单次散射积分步数（契约 §6）

// Rayleigh 相位：(3/16π)(1 + mu²)。
// Henyey-Greenstein 近似（Mie 相位；与 atmosphereParams.cpp miePhase 同式）。
// 臭氧帐篷密度（与 computeTransmittance 的 dO 同式）。
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

#endif // SCENE_LIGHTING_GLSL
