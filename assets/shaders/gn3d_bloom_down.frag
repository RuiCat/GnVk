#version 450
// bloom 下采样：13-tap（Jimenez 2014, COD:AW）+ 可选首级亮度阈值。
// push threshold < 0 表示“已是曝光/阈值后的亮部”，不再重复乘曝光与阈值。
layout(location = 0) in vec2 fragUV;
layout(binding = 1, set = 0) uniform sampler2D srcTex;
layout(location = 0) out vec4 outColor;

layout(push_constant) uniform BloomPC {
    float threshold; // <0 = 仅下采样
    float knee;
    float exposure;
    float pad;
} pc;

vec3 downsample13(vec2 uv, vec2 texel) {
    vec3 a = texture(srcTex, uv + texel * vec2(-2.0, -2.0)).rgb;
    vec3 b = texture(srcTex, uv + texel * vec2( 0.0, -2.0)).rgb;
    vec3 c = texture(srcTex, uv + texel * vec2( 2.0, -2.0)).rgb;
    vec3 d = texture(srcTex, uv + texel * vec2(-1.0, -1.0)).rgb;
    vec3 e = texture(srcTex, uv + texel * vec2( 1.0, -1.0)).rgb;
    vec3 f = texture(srcTex, uv + texel * vec2(-2.0,  0.0)).rgb;
    vec3 g = texture(srcTex, uv).rgb;
    vec3 h = texture(srcTex, uv + texel * vec2( 2.0,  0.0)).rgb;
    vec3 i = texture(srcTex, uv + texel * vec2(-1.0,  1.0)).rgb;
    vec3 j = texture(srcTex, uv + texel * vec2( 1.0,  1.0)).rgb;
    vec3 k = texture(srcTex, uv + texel * vec2(-2.0,  2.0)).rgb;
    vec3 l = texture(srcTex, uv + texel * vec2( 0.0,  2.0)).rgb;
    vec3 m = texture(srcTex, uv + texel * vec2( 2.0,  2.0)).rgb;

    vec3 result = (a + c + k + m) * 0.03125;
    result += (b + f + h + l) * 0.0625;
    result += (d + e + i + j) * 0.125;
    result += g * 0.125;
    return result;
}

void main() {
    vec2 texel = 1.0 / vec2(textureSize(srcTex, 0));
    vec3 color = downsample13(fragUV, texel);
    if(pc.threshold >= 0.0) {
        color *= max(pc.exposure, 0.0);
        float luma = dot(color, vec3(0.2126, 0.7152, 0.0722));
        float weight = smoothstep(pc.threshold, pc.threshold + max(pc.knee, 0.001), luma);
        color *= weight;
    }
    outColor = vec4(color, 1.0);
}
