#version 450
// GN_mini 2D 片段着色器
// 采样纹理 × 顶点颜色 → 输出 + Alpha 混合

layout(location=0) in vec2 fragUV;
layout(location=1) in vec4 fragColor;

layout(binding=1, set=0) uniform sampler2D texSampler;

layout(location=0) out vec4 outColor;

void main() {
    outColor = texture(texSampler, fragUV) * fragColor;
}
