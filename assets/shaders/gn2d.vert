#version 450
// GN_mini 2D 顶点着色器
// 输入: 屏幕坐标(x,y) + 纹理UV + 顶点颜色RGBA
// 输出: 变换后的裁剪坐标 + 传递UV和颜色给片段着色器

layout(location=0) in vec2 inPos;
layout(location=1) in vec2 inUV;
layout(location=2) in vec4 inColor;

layout(binding=0, set=0) uniform UBO {
    mat4 proj;
} ubo;

layout(location=0) out vec2 fragUV;
layout(location=1) out vec4 fragColor;

void main() {
    gl_Position = ubo.proj * vec4(inPos, 0.0, 1.0);
    fragUV = inUV;
    fragColor = inColor;
}
