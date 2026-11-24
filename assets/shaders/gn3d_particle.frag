// gn3d_particle.frag — P9 GPU 粒子片元：直接输出顶点插值颜色，混合状态由管线负责。
#version 450

layout(location = 0) in vec4 vColor;
layout(location = 1) in vec2 vCorner;
layout(location = 0) out vec4 outColor;

void main() {
    // 径向衰减：把 billboard 从"方块"变成"软圆点"。没有这一步，火焰是一堆可见的方格。
    // 中心 length=0 ⇒ falloff=1（不改变既有断言的口径：中心像素仍是原色全不透明）；
    // 边缘 length=1 ⇒ falloff=0（平滑收边）。
    float r = length(vCorner);
    float falloff = 1.0 - smoothstep(0.35, 1.0, r);
    outColor = vec4(vColor.rgb, vColor.a * falloff);
}
