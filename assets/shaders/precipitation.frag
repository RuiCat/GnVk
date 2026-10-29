#version 450
// precipitation.frag — P11-S5 降水片段着色器：按形状模式做软边 alpha。
//
//   * streakScale > 1.5（雨）：细芯 + 头部亮、尾部淡的雨丝（沿速度方向取向，有风会斜）；
//   * 否则（雪）：以 uv 中心为圆心的径向软斑。
// 这样不会出现"实心长方形面片"。alpha 混合在管线固定状态里；这里只调 alpha。

layout(location = 0) in vec2 fragUv;
layout(location = 1) in vec4 fragColor;
layout(location = 2) in float fragNearFade;
layout(location = 0) out vec4 outColor;

layout(push_constant) uniform PushConstants {
    mat4 viewProj;
    vec4 params; // x = streakScale, y = alphaScale
} pc;

void main() {
    const float streak = pc.params.x;
    float alpha = fragColor.a;

    if(streak > 1.5) {
        // 雨丝：横向保留中间约 45% 的亮芯（太窄会在中远景变成亚像素、整条消失）；
        // 纵向两端对称收边，不做"一头亮一头暗"的渐变。
        const float dx = abs(fragUv.x - 0.5) * 2.0;
        const float core = 1.0 - smoothstep(0.45, 1.0, dx);
        const float ends = smoothstep(0.0, 0.12, fragUv.y) * (1.0 - smoothstep(0.88, 1.0, fragUv.y));
        alpha *= core * ends * fragNearFade;
    } else {
        // 雪团：径向软斑（中心 0.45 半径内全亮，边缘平滑消失）。
        const float radius = length(fragUv - vec2(0.5)) * 2.0;
        alpha *= (1.0 - smoothstep(0.45, 1.0, radius)) * fragNearFade;
    }

    if(alpha <= 0.003) {
        discard;
    }
    outColor = vec4(fragColor.rgb, alpha);
}
