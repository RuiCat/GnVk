#version 450
// ╔══════════════════════════════════════════════════════════╗
// ║  GN_mini — 合成着色器 (HDR → Swapchain)                ║
// ║  曝光 + 色调映射 (NONE/Reinhard/ACES) + Bloom + FXAA    ║
// ╚══════════════════════════════════════════════════════════╝
//
// L1：所有 3D pass 输出线性 HDR（R16G16B16A16_SFLOAT），本 pass 是唯一的
// 曝光/色调映射落点；POST UBO 与 src/graphics/postParams.h 的 sPOST_UBO 逐字段一致。
// 公式与 postParams.cpp 的 CPU 参考实现必须同步（单测锁 CPU 侧）。
// L2a：FXAA（Lottes 2009）在 tonemap 之后的 LDR 域做单 pass 抗锯齿；
// FXAA 的邻域样本只含 HDR+曝光（不含 Bloom），边缘检测忽略泛光晕影。

layout(location = 0) in vec2 fragUV;

layout(binding = 1, set = 0) uniform sampler2D hdrSampler;

layout(binding = 2, set = 0) uniform POST_UBO {
    float exposure;        // 线性曝光倍率（= 2^EV）
    float bloomIntensity;  // Bloom 叠加强度（0 = 关）
    float bloomThreshold;  // Bloom 亮度阈值
    float bloomKnee;       // 阈值软膝盖
    float ev;              // 曝光 EV（仅记录）
    int   tonemapMode;     // 0=NONE / 1=Reinhard / 2=ACES
    float fxaa;            // 0=关 / 1=开（L2a）
    float pad1;
} post;

layout(binding = 3, set = 0) uniform sampler2D bloomTex; // L1d：Bloom mip 链 level0（已曝光/阈值/上采样）

layout(location = 0) out vec4 outColor;

// 与 postParams.cpp tonemapChannel 同式；mode 非法时按 NONE 处理。
vec3 tonemapColor(vec3 color, int mode) {
    if(mode == 2) {
        // Narkowicz 2015 ACES 单曲线 fit
        const float a = 2.51;
        const float b = 0.03;
        const float c = 2.43;
        const float d = 0.59;
        const float e = 0.14;
        return clamp((color * (a * color + b)) / (color * (c * color + d) + e), 0.0, 1.0);
    }
    if(mode == 1) {
        return color / (color + vec3(1.0));
    }
    return clamp(color, 0.0, 1.0);
}

float lumaOf(vec3 color) {
    return dot(color, vec3(0.299, 0.587, 0.114));
}

// FXAA 邻域样本：线性 HDR × 曝光 → tonemap，与中心同色彩域。
vec3 tonemappedSample(vec2 uv, float exposure) {
    vec3 hdr = texture(hdrSampler, uv).rgb * exposure;
    return tonemapColor(hdr, post.tonemapMode);
}

// FXAA 3.11 风格的 compact 实现（Lottes 2009）：对角 luma 求边缘方向 → 沿方向混合。
vec3 applyFxaa(vec2 uv, vec2 texel, vec3 centerColor, float exposure) {
    const float lumaM = lumaOf(centerColor);
    const float lumaNW = lumaOf(tonemappedSample(uv + vec2(-1.0, -1.0) * texel, exposure));
    const float lumaNE = lumaOf(tonemappedSample(uv + vec2( 1.0, -1.0) * texel, exposure));
    const float lumaSW = lumaOf(tonemappedSample(uv + vec2(-1.0,  1.0) * texel, exposure));
    const float lumaSE = lumaOf(tonemappedSample(uv + vec2( 1.0,  1.0) * texel, exposure));

    const float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
    const float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));
    if(lumaMax - lumaMin < max(0.0312, lumaMax * 0.125)) {
        return centerColor; // 平坦区不处理
    }

    vec2 dir;
    dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
    dir.y =  ((lumaNW + lumaSW) - (lumaNE + lumaSE));
    const float dirReduce = max((lumaNW + lumaNE + lumaSW + lumaSE) * 0.03125, 0.0078125);
    const float rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);
    dir = clamp(dir * rcpDirMin, vec2(-8.0), vec2(8.0)) * texel;

    const vec3 rgbA = 0.5 * (tonemappedSample(uv + dir * (1.0 / 3.0 - 0.5), exposure) +
                             tonemappedSample(uv + dir * (2.0 / 3.0 - 0.5), exposure));
    const vec3 rgbB = rgbA * 0.5 + 0.25 * (tonemappedSample(uv + dir * -0.5, exposure) +
                                           tonemappedSample(uv + dir * 0.5, exposure));
    const float lumaB = lumaOf(rgbB);
    if(lumaB < lumaMin || lumaB > lumaMax) {
        return rgbA;
    }
    return rgbB;
}

void main() {
    const float exposure = max(post.exposure, 0.0);
    vec3 exposed = texture(hdrSampler, fragUV).rgb * exposure;
    // Bloom 链已在首级乘曝光并做软阈值，这里只按强度叠加。
    exposed += texture(bloomTex, fragUV).rgb * max(post.bloomIntensity, 0.0);
    vec3 color = tonemapColor(exposed, post.tonemapMode);

    if(post.fxaa > 0.5) {
        const vec2 texelSize = vec2(1.0) / vec2(textureSize(hdrSampler, 0));
        color = applyFxaa(fragUV, texelSize, color, exposure);
    }

    // swapchain 为 SRGB 格式：硬件自动编码 Gamma，这里只输出线性值。
    outColor = vec4(color, 1.0);
}
