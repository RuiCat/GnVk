#version 450
// ╔══════════════════════════════════════════════════════════╗
// ║  GN_mini — 天空盒片段着色器                              ║
// ║  大气 sky-view LUT / 解析式天空 + 太阳（云由 L5 体积云接管） ║
// ╚══════════════════════════════════════════════════════════╝
//
// 与 C++ 的 GNg::VkSkyParams 逐字段对应（UBO 共 144 B，前 64 B 是顶点阶段用的 viewProj）。
// L4：atmosphereParams.x >= 0.5 时基色改为采样 sky-view LUT（契约 §5），否则保持旧解析渐变。

#define PI 3.14159265359

layout(location = 0) in vec3 fragWorldDir;
layout(location = 0) out vec4 outColor;

layout(binding = 0, set = 0) uniform SkyboxUBO {
    mat4 viewProj;          // 64 B（片元阶段不用，仅为对齐 C++ 结构）
    vec4 sunDirection;      // xyz = 指向太阳的单位方向, w = 太阳强度
    vec4 horizonColor;      // rgb = 地平线颜色, w = 云覆盖度 0..1
    vec4 zenithColor;       // rgb = 天顶颜色, w = 云锐度 0..1
    vec4 timeParams;        // x = 时间(秒), y/z = 风(x,z), w = 曝光（旧路径用）
    vec4 atmosphereParams;  // x=atmosphereMix(0=旧解析/1=LUT), y=辐亮度倍率, z/w=保留
} skybox;

// L4 大气 LUT（契约 §4：天空盒 set0 binding 2/3）。
// 透射率 LUT 在 skybox v1 不采样（保留给后续太阳透射率/环境一致性），sky-view 是天空基色来源。
layout(binding = 2, set = 0) uniform sampler2D transmittanceLut;
layout(binding = 3, set = 0) uniform sampler2D skyViewLut;

// 契约 §5：世界方向 → sky-view LUT uv（与 CPU skyViewUv / computeSkyViewLut 同一参数化）。
// u = wrap(az - azSun, -PI, PI)/(2PI) + 0.5，v = 1 - clamp(dir.y, 0, 1)（LUT 只覆盖上半球）。
vec2 skyViewUv(vec3 dir, vec3 sunDir) {
    float azSun = atan(sunDir.x, sunDir.z);
    float az = atan(dir.x, dir.z);
    float delta = az - azSun;
    delta -= 2.0 * PI * floor(delta / (2.0 * PI) + 0.5); // 环绕到 [-PI, PI)
    float u = delta / (2.0 * PI) + 0.5;
    float v = 1.0 - clamp(dir.y, 0.0, 1.0);
    return vec2(u, v);
}

void main() {
    vec3 dir = normalize(fragWorldDir);
    vec3 sunDir = normalize(skybox.sunDirection.xyz);
    float sunIntensity = skybox.sunDirection.w;
    float atmosphereMix = clamp(skybox.atmosphereParams.x, 0.0, 1.0);
    bool useAtmosphere = atmosphereMix >= 0.5;

    // ── 天空基色：sky-view LUT（大气）或 地平线 → 天顶（旧解析）──
    vec3 sky;
    if (useAtmosphere) {
        vec3 lut = textureLod(skyViewLut, skyViewUv(dir, sunDir), 0.0).rgb
                 * max(skybox.atmosphereParams.y, 0.0);
        // 防黑兜底：LUT 未就绪/绑定异常时采样可能为 0，此时回落到解析渐变（宁可近似也不黑屏）。
        if (max(max(lut.r, lut.g), lut.b) < 1.0e-4) {
            float up = clamp(dir.y, 0.0, 1.0);
            lut = mix(skybox.horizonColor.rgb, skybox.zenithColor.rgb, pow(up, 0.55));
        }
        // 地平线以下不得纯黑：LUT 只覆盖上半球，回落 horizonColor（契约 §5）。
        sky = dir.y < 0.0 ? skybox.horizonColor.rgb : lut;
    } else {
        float upAmount = clamp(dir.y, 0.0, 1.0);
        float t = pow(upAmount, 0.55);
        sky = mix(skybox.horizonColor.rgb, skybox.zenithColor.rgb, t);
    }

    // ── 阴天：天空整体压灰 + 太阳隐没 ────────────────
    // 天气系统在阴雨时会把 sunIntensity 调低（CLEAR 1.0 / CLOUDY 0.55 / RAIN 0.30 / STORM 0.15、
    // SNOW 0.40），这里用它当"阴天程度"的代理：越高越接近地平线→天顶的灰色渐变，太阳圆盘按平方
    // 衰减到看不见。否则即使云盖铺满，画面里仍然挂着一个亮太阳，读起来还是晴天。
    // 1.4 倍是标定：SNOW(0.40) 要走到 ~0.84，不能只灰一半。
    float overcast = clamp((1.0 - sunIntensity) * 1.4, 0.0, 1.0);
    {
        float upAmount = clamp(dir.y, 0.0, 1.0);
        vec3 flatSky = mix(skybox.horizonColor.rgb, skybox.zenithColor.rgb, pow(upAmount, 0.55));
        sky = mix(sky, flatSky * 1.15, 0.85 * overcast);
    }

    // ── 太阳：圆盘 + 光晕（解析式保留，v1 不乘太阳透射率）──
    float sunDot = max(dot(dir, sunDir), 0.0);
    float sunDisc = smoothstep(0.9994, 0.99985, sunDot);
    float sunGlow = pow(sunDot, 32.0) * 0.25 + pow(sunDot, 256.0) * 0.8;
    vec3 sunColor = vec3(1.0, 0.88, 0.70);
    // 平方衰减：sunIntensity 0.3 时圆盘只剩 9%，被云盖遮住后不会"透出一个白点"。
    sky += sunColor * (sunDisc * 2.2 + sunGlow) * sunIntensity * sunIntensity;

    // ── 云已由 L5 体积云（gn3d_cloud_vol）接管：本 shader 不再绘制 FBM 云 ──

    // ── 地平线雾霭 / 地平线以下压暗 ────────────────
    float haze = smoothstep(0.18, -0.02, dir.y) * 0.55;
    sky = mix(sky, skybox.horizonColor.rgb, haze);
    vec3 below = mix(vec3(0.25, 0.26, 0.28), skybox.horizonColor.rgb,
                     smoothstep(-0.5, 0.0, dir.y));
    sky = mix(below, sky, smoothstep(-0.02, 0.02, dir.y));

    // ── 曝光 ───────────────────────────────────────
    // 契约 §0：LUT 大气路径输出线性 HDR 辐亮度，曝光由 composite 统一施加，这里不乘；
    // 契约 §5：旧解析路径逐行不动（保留原曝光烘焙）。
    if (!useAtmosphere) sky *= skybox.timeParams.w;
    outColor = vec4(sky, 1.0);
}
