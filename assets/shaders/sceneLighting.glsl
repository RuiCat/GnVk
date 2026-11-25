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

// 契约 §8：i=0..count-1 由近到远做包含测试；返回第一个命中级，都不命中返回 -1（无阴影）。
//
// **Vulkan NDC 的口径：x/y ∈ [-1,1]，只有 z ∈ [0,1]**（见 Mat44OrthoVK 的实现）。
// 这里曾经把 x/y 也写成 [0,1] —— 于是每个级联只有"Light 空间右上象限"那 1/4 的片元
// 能选中级联，阴影只在那块区域出现（表现为"场景里只有一小片有影子"）。
// 更隐蔽的是：下方的 ShadowCalculation 用的是 uv = ndc.xy * 0.5 + 0.5（x/y ∈ [-1,1] 口径），
// 两处口径不一致，所以不是"完全没阴影"而是"阴影缺了 3/4"，很难一眼看出来。
int selectCascade(vec3 worldPos) {
    int cascadeCount = int(clamp(lightData.shadowParams.y, 1.0, 4.0));
    for (int i = 0; i < cascadeCount; ++i) {
        vec4 clip = lightData.shadowLightVP[i] * vec4(worldPos, 1.0);
        if (clip.w <= 0.0) continue;
        vec3 ndc = clip.xyz / clip.w;
        if (ndc.x >= -1.0 && ndc.x <= 1.0 &&
            ndc.y >= -1.0 && ndc.y <= 1.0 &&
            ndc.z >= 0.0 && ndc.z <= 1.0) {
            return i;
        }
    }
    return -1;
}

// ═══════════════════════════════════════════════════════════
//  B2：点光/聚光 cube shadow
// ═══════════════════════════════════════════════════════════
// 传统做法是用 `texture(samplerCubeShadow, vec4(dir, refZ))` 让硬件比较。本项目已有的
// CSM 走的是"手动比较 + PCSS"，为了口径统一，这里也手动比较（sampler 关闭 compare）。
//
// 为什么 PCF 的偏移要在**方向空间**做而不是 uv 空间：cube 的 6 个面用的是不同的 2D 投影，
// 在某一面的 uv 上偏移到面外会跳变。做法是把方向扰动后重新采样 —— 代价是多几次 normalize。
// ═══════════════════════════════════════════════════════════
//  B2b：点光/聚光 cube shadow 的 PCSS（接触硬化）
// ═══════════════════════════════════════════════════════════
// 之前是"固定 5 点 PCF + 常数软度"。问题是：软度写死 ⇒ 遮挡物紧贴接收面时阴影也是糊的，
// 而离得远时又不够糊。PCSS 的三段式正好解决这一点：
//
//   1) 阻塞物搜索：在"光源可见锥"里采样，求平均遮挡深度 blocker；
//      一个都没遮挡 ⇒ 直接返回 1.0（全亮），这是最省的早退路径。
//   2) 半影宽度：penumbra = (refZ - blocker) * lightRadius / blocker
//      —— 遮挡物离接收面越近，(refZ - blocker) 越小，半影越窄（**接触硬化**）。
//   3) 在半影半径上做 PCF。
//
// 与 CSM 的 PCSS 的差别：那边在 2D 纹理的 uv 空间偏移，这边在**方向空间**偏移
// （cube 的 6 个面是不同投影，uv 偏移会跨面跳变）。方向偏移角 = 世界宽度 / 距离。
//
// lightRadius 来自 L.coneAngles.z（见 GN_3Dlight.h 的说明，复用未使用的 pad 字段）。
// ═══════════════════════════════════════════════════════════
//  编译期守卫：cube shadow 只在声明了 samplerCubeArray 的着色器里编译
// ═══════════════════════════════════════════════════════════
// 本文件是**公共片段**，被 gn3d_pbr.frag 与 gn3d_deferred_light.frag 同时 include。
// 后者不用 cube shadow，也就没声明 cubeShadowSampler —— 不加守卫会报
// "cubeShadowSampler: undeclared identifier"（实测）。
// 需要的一方在 include 之前 `#define GN_HAS_CUBE_SHADOW_ARRAY 1`。
#ifdef GN_HAS_CUBE_SHADOW_ARRAY

// 距离（米）→ NDC 深度（0..1）。**这一步漏掉过一次**：
// cube shadow 图里存的是 NDC 深度，而这里算出的是世界距离；直接比较会让几乎所有片元
// 都被判成"被遮挡"（实测整片地面暗了 25%，正好是"那一盏灯完全没贡献"）。
// 透视投影的深度关系：z_ndc = far/(far-near) * (1 - near/dist)
float cubeDepthToNdc(float dist, float nearZ, float farZ) {
    float n = max(nearZ, 1e-4);
    float f = max(farZ, n + 1e-3);
    float d = max(dist, n);
    return clamp(f / (f - n) * (1.0 - n / d), 0.0, 1.0);
}

float cubeShadowFactor(vec3 worldPos, vec3 lightPos, vec3 N, float lightRadius,
                       float nearZ, float farZ, int layer) {
    // near/far 未设置时（<=0）无法把米转成 NDC —— **保守返回全亮**，
    // 而不是拿两个不同单位的值硬比（那会把整片场景压黑）。
    if(!(farZ > nearZ) || nearZ <= 0.0) return 1.0;
    // layer < 0：这盏灯没有 cube shadow（超过 4 盏灯的阴影上限，或调用方没渲染）。
    // 同样保守返回全亮，而不是去采样别的灯的层 —— 那会把别人的阴影搬过来。
    if(layer < 0) return 1.0;

    vec3 toFrag = worldPos - lightPos; // 从光源指向片元（正好是 cube 的采样方向）
    float dist = length(toFrag);
    if(dist < 1e-4) return 1.0;

    // 法线偏移：沿法线抬高一点再比较，避免自阴影痤疮
    vec3 biasedPos = worldPos + N * max(dist * 0.0025, 0.002);
    float refDist = length(biasedPos - lightPos);
    // 纹理里存的是 NDC 深度，比较前必须换算到同一单位
    float refZ = cubeDepthToNdc(refDist, nearZ, farZ);
    vec3 baseDir = normalize(biasedPos - lightPos);

    // 方向空间的一组正交基（用于在锥内扰动采样方向）
    vec3 up = abs(baseDir.y) < 0.95 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
    vec3 tangent = normalize(cross(up, baseDir));
    vec3 bitangent = cross(baseDir, tangent);

    // 光源半径不能为 0（否则没有任何半影）：给一个极小值保证"硬阴影"也至少 1 texel
    float radius = max(lightRadius, 0.002);

    // ── 1) 阻塞物搜索 ──
    //
    // 搜索锥的**上限由 cube 分辨率决定**，不是由物理半影决定。这点踩过坑：
    // 第一版直接用"光源张角 × 1.6"，火塘场景里算出来 0.3 rad ≈ 17° ——
    // 而 1024 面的 cube 是 1024/90° ≈ 11 texel/度，17° 相当于 190 texel 的搜索半径，
    // 采到的全是邻面噪声与**远处**遮挡物（结果是右侧 5 m 外的矮墙也被误判成有阴影，
    // 整片地面暗了 27%）。
    //
    // 合理上限 ≈ 搜索 12 texel：12 / (1024/90) ≈ 1.05° ≈ 0.018 rad。
    // 但纯 12 texel 又表达不了"石头紧贴光源"那种大半影，所以取一个折中上限 0.10 rad
    // （≈ 5.7°，约 65 texel）—— 既不误采远处遮挡，也能表达中等宽度的半影。
    const float kMaxSearchAngle = 0.10;
    float searchAngle = clamp(radius / max(dist, 0.05), 0.0, kMaxSearchAngle);
    const int kBlockerSamples = 8;
    float blockerSum = 0.0;
    float blockerCount = 0.0;
    for(int i = 0; i < kBlockerSamples; ++i) {
        // 8 个方向均匀铺在锥内（避免用 poissonDisk 那样的大表：cube 侧对噪点更敏感）
        float angle = float(i) * 0.78539816339; // 2pi / 8
        float radial = (i == 0) ? 0.0 : 1.0; // 第 0 个取中心，其余铺满
        vec3 sampleDir = normalize(baseDir + (tangent * cos(angle) * radial +
                                             bitangent * sin(angle) * radial) * searchAngle);
        // cube ARRAY 采样：第四个分量是层号（哪盏灯）
        float occluder = textureLod(cubeShadowSampler, vec4(sampleDir, float(layer)), 0.0).r;
        // border=白(1.0) ⇒ 图外自然不算遮挡
        if(occluder < refZ - 1e-4) {
            blockerSum += occluder;
            blockerCount += 1.0;
        }
    }
    if(blockerCount < 0.5) return 1.0; // 锥内无遮挡：全亮（最重要的早退）
    float blockerNdc = blockerSum / blockerCount;

    // ── 2) 半影宽度（世界单位 → 方向角）──
    // 需要**世界距离**而不是 NDC：把 blocker 的 NDC 深度反解回距离。
    // 由 z = f/(f-n) * (1 - n/d) 反解：d = n / (1 - z*(f-n)/f)
    float k = (farZ - nearZ) / farZ;
    float denom = 1.0 - blockerNdc * k;
    float blockerDist = denom > 1e-4 ? nearZ / denom : farZ;
    // 相似三角形：penumbraWorld / lightRadius = (refDist - blockerDist) / blockerDist
    float penumbraWorld = (refDist - blockerDist) * radius / max(blockerDist, 1e-4);
    // 转成方向角，钳到 [1 texel, 搜索锥] —— **上界必须与搜索锥一致**：
    // PCF 采到搜索范围之外的东西等于在采样没有经过 blocker 验证的区域。
    float penumbraAngle = clamp(penumbraWorld / max(refDist, 1e-3), 0.0015, kMaxSearchAngle);

    // ── 3) 在半影半径上做 PCF ──
    const int kPcfSamples = 8;
    float lit = 0.0;
    for(int i = 0; i < kPcfSamples; ++i) {
        float angle = float(i) * 0.78539816339;
        float radial = (i == 0) ? 0.0 : 1.0;
        vec3 sampleDir = normalize(baseDir + (tangent * cos(angle) * radial +
                                             bitangent * sin(angle) * radial) * penumbraAngle);
        float occluder = textureLod(cubeShadowSampler, vec4(sampleDir, float(layer)), 0.0).r;
        lit += (refZ <= occluder + 1e-4) ? 1.0 : 0.0;
    }
    return lit / float(kPcfSamples);
}

#endif // GN_HAS_CUBE_SHADOW_ARRAY

// 原始深度比较：receiver <= occluder 视为受光（layer = 选中的 CSM 级）。
float shadowCompare(vec2 uv, float layer, float receiver) {
    float occluder = textureLod(shadowSampler, vec3(uv, layer), 0.0).r;
    return receiver <= occluder ? 1.0 : 0.0;
}

#endif // SCENE_LIGHTING_GLSL
