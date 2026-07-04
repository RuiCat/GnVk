#version 450
// ╔══════════════════════════════════════════════════════════╗
// ║  GN_mini — 阴影映射顶点着色器                            ║
// ║  从光源视角渲染深度                                      ║
// ╚══════════════════════════════════════════════════════════╝

layout(location = 0) in vec3 inPos;

layout(binding = 0, set = 0) uniform ShadowUBO {
    mat4 lightViewProj;
} shadow;

layout(push_constant) uniform ModelPC {
    mat4 model;
} pc;

void main() {
    gl_Position = shadow.lightViewProj * pc.model * vec4(inPos, 1.0);
}
