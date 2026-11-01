#version 450
// bloom 上采样：9-tap tent 滤波，管线用 ONE/ONE 加法混合叠回上一级。
layout(location = 0) in vec2 fragUV;
layout(binding = 1, set = 0) uniform sampler2D srcTex;
layout(location = 0) out vec4 outColor;

vec3 upsample9(vec2 uv, vec2 texel) {
    vec3 result = texture(srcTex, uv).rgb * 4.0;
    result += texture(srcTex, uv + vec2(-texel.x, 0.0)).rgb * 2.0;
    result += texture(srcTex, uv + vec2( texel.x, 0.0)).rgb * 2.0;
    result += texture(srcTex, uv + vec2(0.0, -texel.y)).rgb * 2.0;
    result += texture(srcTex, uv + vec2(0.0,  texel.y)).rgb * 2.0;
    result += texture(srcTex, uv + vec2(-texel.x, -texel.y)).rgb;
    result += texture(srcTex, uv + vec2( texel.x, -texel.y)).rgb;
    result += texture(srcTex, uv + vec2(-texel.x,  texel.y)).rgb;
    result += texture(srcTex, uv + vec2( texel.x,  texel.y)).rgb;
    return result / 16.0;
}

void main() {
    vec2 texel = 1.0 / vec2(textureSize(srcTex, 0));
    outColor = vec4(upsample9(fragUV, texel), 1.0);
}
