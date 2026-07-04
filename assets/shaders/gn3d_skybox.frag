#version 450
// ╔══════════════════════════════════════════════════════════╗
// ║  GN_mini — 天空盒片段着色器                              ║
// ║  采样 cubemap 天空纹理                                   ║
// ╚══════════════════════════════════════════════════════════╝

layout(location = 0) in vec3 fragViewDir;

layout(binding = 1, set = 0) uniform samplerCube skyboxCubemap;

layout(location = 0) out vec4 outColor;

void main() {
    // 采样 cubemap (视线方向)
    vec3 skyColor = texture(skyboxCubemap, fragViewDir).rgb;
    outColor = vec4(skyColor, 1.0);
}
