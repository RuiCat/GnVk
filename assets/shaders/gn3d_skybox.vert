#version 450
// ╔══════════════════════════════════════════════════════════╗
// ║  GN_mini — 天空盒顶点着色器 (单位立方体)                ║
// ║  立方体顶点 = 世界空间方向 → 直接用于 cubemap 采样       ║
// ╚══════════════════════════════════════════════════════════╝

layout(location = 0) out vec3 fragWorldDir;

layout(binding = 0, set = 0) uniform SkyboxUBO {
    mat4 viewProj;  // VP 矩阵 (平移=0, 天空盒始终跟随摄像机在远平面)
} skybox;

// 单位立方体 36 顶点, 世界空间方向 (Vulkan cubemap 约定)
// +X=右, +Y=上, +Z=前
const vec3 cube[36] = vec3[](
    vec3( 1,  1,  1), vec3( 1, -1,  1), vec3( 1, -1, -1),
    vec3( 1,  1,  1), vec3( 1, -1, -1), vec3( 1,  1, -1),
    vec3(-1, -1,  1), vec3(-1,  1,  1), vec3(-1,  1, -1),
    vec3(-1, -1,  1), vec3(-1,  1, -1), vec3(-1, -1, -1),
    vec3(-1,  1,  1), vec3( 1,  1,  1), vec3( 1,  1, -1),
    vec3(-1,  1,  1), vec3( 1,  1, -1), vec3(-1,  1, -1),
    vec3( 1, -1,  1), vec3(-1, -1,  1), vec3(-1, -1, -1),
    vec3( 1, -1,  1), vec3(-1, -1, -1), vec3( 1, -1, -1),
    vec3(-1,  1,  1), vec3(-1, -1,  1), vec3( 1, -1,  1),
    vec3(-1,  1,  1), vec3( 1, -1,  1), vec3( 1,  1,  1),
    vec3( 1, -1, -1), vec3(-1, -1, -1), vec3(-1,  1, -1),
    vec3( 1, -1, -1), vec3(-1,  1, -1), vec3( 1,  1, -1)
);

void main() {
    vec3 pos = cube[gl_VertexIndex];
    // VP×(pos,0) → 平移分量归零, 天空盒跟随摄像机
    vec4 clipPos = skybox.viewProj * vec4(pos, 0.0);
    gl_Position = clipPos.xyww;  // z=w → 始终在远平面
    fragWorldDir = pos;          // 世界空间方向
}
