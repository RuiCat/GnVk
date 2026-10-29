// gn3d_cloth.vert — P9-a GPU 布料绘制的顶点着色器：顶点 pulling 读 SSBO，法线取相邻顶点叉积。
//
// set 0：binding 0 = 布料顶点 SSBO（只读）。
// push constant：viewProj(64) + color(16) + params(16, x = width, y = height)。
// 颜色 = albedo * (0.25 + 0.75 * max(dot(n, normalize(vec3(0,0,1))), 0))（与契约一致）。
#version 450

struct ClothVertex {
    vec4 posPinned; // xyz = pos, w = pinned
    vec4 prevPad; // xyz = prev, w = 0
};

layout(set = 0, binding = 0) readonly buffer ClothVertices {
    ClothVertex vertices[];
} cloth;

layout(push_constant) uniform ClothDrawPC {
    mat4 viewProj;
    vec4 color;
    vec4 params; // x = width, y = height
} pc;

layout(location = 0) out vec3 fragColor;

void main() {
    uint width = uint(pc.params.x);
    uint height = uint(pc.params.y);
    uint index = uint(gl_VertexIndex);
    uint column = index % width;
    uint row = index / width;

    uint left = (column > 0u) ? index - 1u : index;
    uint right = (column + 1u < width) ? index + 1u : index;
    uint down = (row > 0u) ? index - width : index;
    uint up = (row + 1u < height) ? index + width : index;

    vec3 tangentX = cloth.vertices[right].posPinned.xyz - cloth.vertices[left].posPinned.xyz;
    vec3 tangentY = cloth.vertices[up].posPinned.xyz - cloth.vertices[down].posPinned.xyz;
    vec3 normal = normalize(cross(tangentX, tangentY));
    if(any(isnan(normal))) normal = vec3(0.0, 0.0, 1.0);

    float lambert = max(dot(normal, normalize(vec3(0.0, 0.0, 1.0))), 0.0);
    fragColor = pc.color.rgb * (0.25 + 0.75 * lambert);
    gl_Position = pc.viewProj * vec4(cloth.vertices[index].posPinned.xyz, 1.0);
}
