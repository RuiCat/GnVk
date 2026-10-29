#version 450
// GN_mini — 调试线框顶点着色器（P8）：世界坐标经场景 UBO 的 viewProj 变换，颜色来自顶点属性。

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec4 inColor;

layout(binding = 0, set = 0) uniform SceneUBO {
    mat4 viewProj;
    vec4 cameraPos;
    vec4 ambientLight;
} scene;

layout(location = 0) out vec4 fragColor;

void main() {
    gl_Position = scene.viewProj * vec4(inPosition, 1.0);
    fragColor = inColor;
}
