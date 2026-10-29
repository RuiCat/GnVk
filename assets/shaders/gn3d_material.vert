#version 450
// ╔══════════════════════════════════════════════════════════╗
// ║  GN_mini — P1c 材质实例顶点着色器                       ║
// ║  model（64B push constant）直接变换顶点 → 裁剪空间      ║
// ╚══════════════════════════════════════════════════════════╝
// 材质管线没有场景 UBO（set 0 固定为材质 UBO + 纹理），顶点位置按
// 调用方约定已是 NDC/世界坐标，只由 64B model 矩阵完成最后变换。
layout(location = 0) in vec3 inPos;
layout(location = 1) in vec2 inUV;

layout(push_constant) uniform MaterialPC {
    mat4 model;
} pc;

layout(location = 0) out vec2 fragUV;

void main() {
    gl_Position = pc.model * vec4(inPos, 1.0);
    fragUV = inUV;
}
