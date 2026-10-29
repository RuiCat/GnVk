// gn3d_particle.frag — P9 GPU 粒子片元：直接输出顶点插值颜色，混合状态由管线负责。
#version 450

layout(location = 0) in vec4 vColor;
layout(location = 0) out vec4 outColor;

void main() {
    outColor = vColor;
}
