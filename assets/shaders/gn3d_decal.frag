#version 450
// gn3d_decal.frag — P11-S6b 地面贴花片段着色器：软圆斑 / 溅射环。
//
//   * fragKind ≈ 1（SPLASH_RING）：环状 alpha（内圈透明、环带亮、外缘淡出）；
//   * 其它（SPOT）：中心亮、边缘 smoothstep 淡出的软圆斑。
// alpha 基础值来自顶点色（已乘寿命渐隐），alpha 混合在管线固定状态里。

layout(location = 0) in vec2 fragUv;
layout(location = 1) in vec4 fragColor;
layout(location = 2) in float fragKind;
layout(location = 0) out vec4 outColor;

layout(push_constant) uniform PushConstants {
    mat4 viewProj;
    vec4 params;
} pc;

void main() {
    const float radius = length(fragUv - vec2(0.5)) * 2.0;
    float alpha = fragColor.a;

    if(fragKind > 0.5) {
        // 溅射环：内圈空、环带亮、外缘快速淡出。
        const float ring = smoothstep(0.35, 0.80, radius) * (1.0 - smoothstep(0.86, 1.0, radius));
        alpha *= ring;
    } else {
        // 水渍：中心亮，边缘柔化。
        alpha *= 1.0 - smoothstep(0.45, 1.0, radius);
    }

    if(alpha <= 0.003) {
        discard;
    }
    outColor = vec4(fragColor.rgb, alpha);
}
