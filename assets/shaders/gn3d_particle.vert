// gn3d_particle.vert — P9 GPU 粒子实例化 billboard：每实例 6 顶点，顶点 pulling 读 SSBO。
//
// 契约：
//   * 无顶点缓冲 / 无索引缓冲：quad 角点由 gl_VertexIndex 取常量数组，粒子数据由
//     gl_InstanceIndex 直接索引 SSBO（set 0 binding 1）。
//   * 死槽（misc.z < 0.5）放到裁剪空间外（z > w），不产生任何片元。
//   * 颜色 = mix(colorStart, colorEnd, age/life)，由片元着色器做 alpha 混合。
#version 450

layout(set = 0, binding = 0) uniform ParticleDrawUBO {
    mat4 viewProj;
    vec4 cameraRight;
    vec4 cameraUp;
    vec4 colorStart;
    vec4 colorEnd;
    vec4 params; // x = size, y = count, z = time, w = 保留
} u;

struct ParticleGPU {
    vec4 posAge; // xyz = 位置, w = age
    vec4 velLife; // xyz = 速度, w = life
    vec4 color;
    vec4 misc; // x = size, y = seed, z = alive, w = pad
};

layout(set = 0, binding = 1) readonly buffer ParticleBuffer {
    ParticleGPU particles[];
};

layout(location = 0) out vec4 vColor;

// 两个三角形覆盖 [-1,1]²（双面绘制，绕序无关）。
const vec2 kCorners[6] = vec2[6](
    vec2(-1.0, -1.0), vec2(1.0, -1.0), vec2(1.0, 1.0),
    vec2(-1.0, -1.0), vec2(1.0, 1.0), vec2(-1.0, 1.0));

void main() {
    ParticleGPU p = particles[gl_InstanceIndex];
    if (p.misc.z < 0.5) {
        gl_Position = vec4(0.0, 0.0, 2.0, 1.0);
        vColor = vec4(0.0);
        return;
    }
    vec2 corner = kCorners[gl_VertexIndex];
    vec3 world = p.posAge.xyz +
                 (u.cameraRight.xyz * corner.x + u.cameraUp.xyz * corner.y) * u.params.x;
    gl_Position = u.viewProj * vec4(world, 1.0);
    float t = clamp(p.posAge.w / max(p.velLife.w, 1e-4), 0.0, 1.0);
    vColor = mix(u.colorStart, u.colorEnd, t);
}
