#version 450
// gn3d_decal.vert — P11-S6b 地面贴花顶点着色器：顶点是 CPU 展开的世界空间水平四边形。
// push constant 80 B：viewProj(64) + params(16)：x = alphaScale。

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inUv;
layout(location = 2) in vec4 inColor;
layout(location = 3) in float inKind;

layout(push_constant) uniform PushConstants {
    mat4 viewProj;
    vec4 params;
} pc;

layout(location = 0) out vec2 fragUv;
layout(location = 1) out vec4 fragColor;
layout(location = 2) out float fragKind;

void main() {
    gl_Position = pc.viewProj * vec4(inPosition, 1.0);
    fragUv = inUv;
    fragColor = inColor;
    fragKind = inKind;
}
