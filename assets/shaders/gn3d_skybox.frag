#version 450
// ╔══════════════════════════════════════════════════════════╗
// ║  GN_mini — 天空盒片段着色器                              ║
// ║  解析式天空 + 太阳 + FBM 云（混合模式·远景层）           ║
// ╚══════════════════════════════════════════════════════════╝
//
// 与 C++ 的 GNg::VkSkyParams 逐字段对应（UBO 共 128 B，前 64 B 是顶点阶段用的 viewProj）。

layout(location = 0) in vec3 fragWorldDir;
layout(location = 0) out vec4 outColor;

layout(binding = 0, set = 0) uniform SkyboxUBO {
    mat4 viewProj;        // 64 B（片元阶段不用，仅为对齐 C++ 结构）
    vec4 sunDirection;    // xyz = 指向太阳的单位方向, w = 太阳强度
    vec4 horizonColor;    // rgb = 地平线颜色, w = 云覆盖度 0..1
    vec4 zenithColor;     // rgb = 天顶颜色, w = 云锐度 0..1
    vec4 timeParams;      // x = 时间(秒), y/z = 风(x,z), w = 曝光
} skybox;

float hash21(vec2 p) {
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}

float valueNoise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    float a = hash21(i);
    float b = hash21(i + vec2(1.0, 0.0));
    float c = hash21(i + vec2(0.0, 1.0));
    float d = hash21(i + vec2(1.0, 1.0));
    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

float fbm(vec2 p) {
    float value = 0.0;
    float amplitude = 0.5;
    for (int i = 0; i < 4; ++i) {
        value += amplitude * valueNoise(p);
        p = p * 2.03 + vec2(7.1, 3.7);
        amplitude *= 0.5;
    }
    return value;
}

void main() {
    vec3 dir = normalize(fragWorldDir);
    vec3 sunDir = normalize(skybox.sunDirection.xyz);
    float sunIntensity = skybox.sunDirection.w;

    // ── 天空基色：地平线 → 天顶 ─────────────────────
    float upAmount = clamp(dir.y, 0.0, 1.0);
    float t = pow(upAmount, 0.55);
    vec3 sky = mix(skybox.horizonColor.rgb, skybox.zenithColor.rgb, t);

    // ── 太阳：圆盘 + 光晕 ───────────────────────────
    float sunDot = max(dot(dir, sunDir), 0.0);
    float sunDisc = smoothstep(0.9994, 0.99985, sunDot);
    float sunGlow = pow(sunDot, 32.0) * 0.25 + pow(sunDot, 256.0) * 0.8;
    vec3 sunColor = vec3(1.0, 0.88, 0.70);
    sky += sunColor * (sunDisc * 2.2 + sunGlow) * sunIntensity;

    // ── 云：方向投影到云平面 + 两层 FBM ────────────
    float coverage = clamp(skybox.horizonColor.w, 0.0, 1.0);
    float sharpness = clamp(skybox.zenithColor.w, 0.0, 1.0);
    vec2 wind = vec2(skybox.timeParams.y, skybox.timeParams.z) * skybox.timeParams.x * 0.004;
    vec2 cloudUv = dir.xz / max(dir.y, 0.08);
    float n = fbm(cloudUv * 0.55 + wind) * 0.65 +
              fbm(cloudUv * 1.15 - wind * 1.7) * 0.35;
    float threshold = mix(0.92, 0.30, coverage);
    float soft = mix(0.10, 0.02, sharpness);
    float cloud = smoothstep(threshold, threshold + soft, n);
    cloud *= smoothstep(0.02, 0.20, dir.y);

    vec3 cloudColor = mix(vec3(0.42, 0.44, 0.48), vec3(1.0, 0.98, 0.94),
                          0.35 + 0.65 * sunDot);
    cloudColor *= mix(0.55, 1.0, sunIntensity);
    sky = mix(sky, cloudColor, cloud);

    // ── 地平线雾霭 / 地平线以下压暗 ────────────────
    float haze = smoothstep(0.18, -0.02, dir.y) * 0.55;
    sky = mix(sky, skybox.horizonColor.rgb, haze);
    vec3 below = mix(vec3(0.25, 0.26, 0.28), skybox.horizonColor.rgb,
                     smoothstep(-0.5, 0.0, dir.y));
    sky = mix(below, sky, smoothstep(-0.02, 0.02, dir.y));

    // ── 曝光 ───────────────────────────────────────
    sky *= skybox.timeParams.w;
    outColor = vec4(sky, 1.0);
}
