#version 450
// GN_mini — 调试线框片段着色器（P8）：直接输出顶点颜色（0xAARRGGBB 已在 CPU 端展开为 RGBA）。

layout(location = 0) in vec4 fragColor;
layout(location = 0) out vec4 outColor;

void main() {
    outColor = fragColor;
}
