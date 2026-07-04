#version 450
// ╔══════════════════════════════════════════════════════════╗
// ║  GN_mini — 合成顶点着色器 (全屏三角形)                 ║
// ╚══════════════════════════════════════════════════════════╝

layout(location = 0) out vec2 fragUV;

void main() {
    // 全屏三角形: 3 顶点覆盖整个屏幕, 无需缓冲
    // gl_VertexIndex: 0→(-1,-1), 1→(3,-1), 2→(-1,3)
    fragUV = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(fragUV * 2.0 - 1.0, 0.0, 1.0);
}
