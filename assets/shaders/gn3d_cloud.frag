#version 450
// ╔══════════════════════════════════════════════════════════╗
// ║  GN_mini — 中景 billboard 云层片段着色器（三层云盘）      ║
// ║  高频 FBM 程序化 alpha + coverage 截断 + 层间明暗 + 近淡出 ║
// ╚══════════════════════════════════════════════════════════╝
//
// alpha = smoothstep(threshold-soft, threshold+soft, n) * opacity * 近处淡出
// n = 5 层 value noise FBM；threshold = 1 - (coverage - 0.10*layer)；
// soft 由 sharpness 控制（越大越锐，避免糊成一片灰）。
// 每层颜色略有差异（高层更亮、低层更暗），并对噪声梯度做一点方向光 ⇒ 云团有体积感。

layout(location = 0) in vec3 fragWorldPos;
layout(location = 1) in float fragLayer;
layout(location = 0) out vec4 outColor;

layout(binding = 0, set = 0) uniform CloudUBO {
    mat4 viewProj;
    vec4 cameraPos;
    vec4 windTime;
    vec4 params0;
    vec4 params1;
    vec4 cameraRight;
    vec4 cameraUp;
} cloud;

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

// 5 层 value noise：大团块 + 中频 + 边缘细节；归一化到 0..1。
float fbm(vec2 p) {
    float value = 0.0;
    float amplitude = 0.5;
    float norm = 0.0;
    for(int i = 0; i < 5; ++i) {
        value += amplitude * valueNoise(p);
        norm += amplitude;
        p = p * 2.07 + vec2(7.1, 3.7);
        amplitude *= 0.5;
    }
    return value / max(norm, 0.0001);
}

void main() {
    const float tileSize = max(cloud.params0.x, 1.0);
    const float sharpness = clamp(cloud.params0.w, 0.0, 1.0);
    const float opacity = clamp(cloud.params1.x, 0.0, 1.0);
    const float layer = fragLayer;

    // 每层略降 coverage（高层更稀），让三层能透出层次而不是叠成一块。
    const float coverage = clamp(cloud.params0.z - 0.10 * layer, 0.02, 1.0);

    // 世界 xz 取样：频率比"每 tile 一次"高得多，云团边缘清晰；时间项让云缓慢翻滚。
    const float scale = 3.4 / max(tileSize * 0.35, 8.0);
    vec2 noiseP = fragWorldPos.xz * scale + vec2(cloud.windTime.z * 0.02 * (1.0 + 0.3 * layer));
    float n = fbm(noiseP) * 0.7 + fbm(noiseP * 2.4 + vec2(11.0, 5.0)) * 0.3;

    const float soft = mix(0.14, 0.012, sharpness);
    const float threshold = 1.0 - coverage;
    float alpha = smoothstep(threshold - soft, threshold + soft, n) * opacity;

    // 近处淡出：dist < nearFade 时 alpha 线性降到 0，避免云层糊在镜头上。
    const float nearFade = cloud.cameraPos.w;
    if(nearFade > 0.0) {
        float dist = distance(fragWorldPos, cloud.cameraPos.xyz);
        alpha *= 1.0 - clamp(nearFade / max(dist, 0.001), 0.0, 1.0);
    }
    if(alpha <= 0.002) {
        discard;
    }

    // 噪声梯度近似方向光：云团上侧亮、下侧暗；高层整体更亮。
    float dx = fbm(noiseP + vec2(0.08, 0.0)) - n;
    float dz = fbm(noiseP + vec2(0.0, 0.08)) - n;
    float shade = clamp(0.62 + 1.8 * (dx + dz), 0.28, 1.0);
    vec3 dark = vec3(0.52, 0.56, 0.64);
    vec3 bright = vec3(1.0, 0.99, 0.96);
    vec3 color = mix(dark, bright, clamp(shade + 0.12 * layer, 0.0, 1.0));
    outColor = vec4(color, alpha);
}
