#version 450
// ╔══════════════════════════════════════════════════════════╗
// ║  GN_mini — P1c 材质实例片元着色器                       ║
// ║  固定 set 0：binding 0 = 材质 UBO，binding 1 = 反照率纹理 ║
// ╚══════════════════════════════════════════════════════════╝
layout(binding = 0, set = 0) uniform MaterialUBO {
    vec4 baseColor;
} material;

layout(binding = 1, set = 0) uniform sampler2D albedoTex;

layout(location = 0) in vec2 fragUV;
layout(location = 0) out vec4 outColor;

void main() {
    outColor = material.baseColor * texture(albedoTex, fragUV);
}
