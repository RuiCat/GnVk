#version 450
// precipitation.vert — P11-S5 降水顶点着色器：顶点已在 CPU 端展开成世界空间 billboard，
// 这里只做 viewProj 变换（push constant 80 B）并把 uv / 颜色传给片元。
// uv.x = 侧向 [0,1]，uv.y = 沿速度方向（0 = 头部，1 = 尾部）。

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inUv;
layout(location = 2) in vec4 inColor;

layout(push_constant) uniform PushConstants {
    mat4 viewProj;
    vec4 params; // x = streakScale, y = alphaScale
} pc;

layout(location = 0) out vec2 fragUv;
layout(location = 1) out vec4 fragColor;
layout(location = 2) out float fragNearFade;

void main() {
    gl_Position = pc.viewProj * vec4(inPosition, 1.0);
    fragUv = inUv;
    fragColor = inColor;
    // gl_Position.w = 视空间深度。贴着相机的粒子会被投影放大成几百像素的白条
    // （1 m 雨丝在 4 m 处就有约 350 px），所以 1.5..6 m 内淡出，只保留中远景的雨幕。
    fragNearFade = smoothstep(1.5, 6.0, gl_Position.w);
}
