#version 450
// ╔══════════════════════════════════════════════════════════╗
// ║  GN_mini — 合成着色器 (HDR + Bloom → Swapchain)        ║
// ║  Reinhard 色调映射 + 简易泛光 + Gamma 编码              ║
// ╚══════════════════════════════════════════════════════════╝

layout(location = 0) in vec2 fragUV;

layout(binding = 1, set = 0) uniform sampler2D hdrSampler;

layout(location = 0) out vec4 outColor;

// ═══════════════════════════════════════════════════════════
//  简易 Bloom: 从邻近像素提取亮部叠加
//  不需要额外的模糊 Pass — 适合轻量引擎
// ═══════════════════════════════════════════════════════════

vec3 SimpleBloom(sampler2D hdrTex, vec2 uv, vec2 texelSize) {
    const float threshold = 0.8;
    const float intensity = 0.3;
    
    // 采样中心 + 4 个对角邻近像素 → 近似模糊
    vec3 c0 = texture(hdrTex, uv).rgb;
    vec3 c1 = texture(hdrTex, uv + vec2( 1.5,  1.5) * texelSize).rgb;
    vec3 c2 = texture(hdrTex, uv + vec2(-1.5,  1.5) * texelSize).rgb;
    vec3 c3 = texture(hdrTex, uv + vec2(-1.5, -1.5) * texelSize).rgb;
    vec3 c4 = texture(hdrTex, uv + vec2( 1.5, -1.5) * texelSize).rgb;
    
    vec3 bloom = (c1 + c2 + c3 + c4) * 0.25;
    
    // 只保留亮度超过阈值的部分
    float luminance = dot(bloom, vec3(0.2126, 0.7152, 0.0722));
    bloom *= smoothstep(threshold, threshold + 0.3, luminance);
    
    return c0 + bloom * intensity;
}

void main() {
    vec2 texelSize = vec2(1.0) / vec2(textureSize(hdrSampler, 0));
    vec3 hdrColor = SimpleBloom(hdrSampler, fragUV, texelSize);
    
    // ── Reinhard 色调映射 ──
    vec3 mapped = hdrColor / (hdrColor + vec3(1.0));
    
    // ── 注: swapchain SRGB 自动编码 Gamma ──
    outColor = vec4(mapped, 1.0);
}
