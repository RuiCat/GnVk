// gn3d_cloth.frag — P9-a GPU 布料的片段着色器：直接输出顶点着色器算好的 Lambert 颜色。
#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 0) out vec4 outColor;

void main() {
    outColor = vec4(fragColor, 1.0);
}
