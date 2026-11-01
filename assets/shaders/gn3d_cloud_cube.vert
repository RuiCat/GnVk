#version 450
// gn3d_cloud_cube.vert — 云块包围盒：展开立方体并**反转绕序**，让背面朝相机。
//
// 语义：方块 = 云的**包围盒**，不是云的形状。片元只负责拿到"这条视线穿出盒子的点"，
// 真正的云由 fragment 在盒内 raymarch 噪声密度得到（见 gn3d_cloud_cube.frag）。
//
// 反转绕序 + 管线 CULL_BACK ⇒ 只画盒子背面（远侧表面）：每个被盒子覆盖的像素恰好一个片元，
// 片元位置即射线与盒子的远交点，片元再用射线与盒子求交算出近交点，在 [near, far] 内步进。
//
// 云盖（阴天整片云层）：半边长取**负值**作为标记 —— 片元只步进"云底→云顶"的薄板，
// 不会在覆盖全天空的大盒子里空跑 20 步。
layout(location = 0) out vec3 vWorldPos; // 盒子远侧表面点
layout(location = 1) out vec3 vBoxMin;
layout(location = 2) out vec3 vBoxMax;
layout(location = 3) out vec3 vColor;
layout(location = 4) out float vAlpha;
layout(location = 5) out float vViewDist;
layout(location = 6) out float vIsDeck; // 1 = 云盖，0 = 普通云块

layout(binding = 0, set = 0) uniform CubeUBO {
    mat4 viewProj;
    vec4 aCamera;   // xyz = 相机世界位置，w = 时间(秒)
    vec4 aSun;      // xyz = 指向太阳的单位向量，w = 太阳强度
    vec4 aAmbient;  // rgb = 天空环境色
    vec4 aFog;      // rgb = 雾色，w = 雾密度(1/m)
    vec4 aCount;    // x = 实例数，y = 全场景云底高度(m)，z = 阴雨程度
    vec4 aInstances[128];    // xyz = 中心，w = 半边长（负值 = 云盖）
    vec4 aInstanceTint[128]; // rgb = 色调，w = 密度倍率
} u;

const vec3 kFaceNormal[6] = vec3[6](vec3(0.0, 1.0, 0.0), vec3(0.0, -1.0, 0.0),
                                    vec3(1.0, 0.0, 0.0), vec3(-1.0, 0.0, 0.0),
                                    vec3(0.0, 0.0, 1.0), vec3(0.0, 0.0, -1.0));
const vec3 kFaceTangent[6] = vec3[6](vec3(1.0, 0.0, 0.0), vec3(1.0, 0.0, 0.0),
                                     vec3(0.0, 0.0, 1.0), vec3(0.0, 0.0, 1.0),
                                     vec3(1.0, 0.0, 0.0), vec3(1.0, 0.0, 0.0));
const vec2 kQuadCorner[4] = vec2[4](vec2(-1.0, -1.0), vec2(1.0, -1.0),
                                    vec2(1.0, 1.0), vec2(-1.0, 1.0));

// 反转绕序：正面朝外是 CCW，这里要画背面 ⇒ 三角形顺序取 0,2,1 / 0,3,2。
const int kReversedCorner[6] = int[6](0, 2, 1, 0, 3, 2);

void main() {
    const int face = gl_VertexIndex / 6;
    const int corner = gl_VertexIndex % 6;
    const vec4 instance = u.aInstances[gl_InstanceIndex];
    const vec4 tint = u.aInstanceTint[gl_InstanceIndex];

    const float signedHalf = instance.w;
    const float halfSize = max(abs(signedHalf), 0.0);
    const vec3 normal = kFaceNormal[face];
    const vec3 tangent = kFaceTangent[face];
    const vec3 bitangent = cross(normal, tangent);
    const vec2 offset = kQuadCorner[kReversedCorner[corner]];
    const vec3 position =
        instance.xyz + (normal + offset.x * tangent + offset.y * bitangent) * halfSize;

    vWorldPos = position;
    vBoxMin = instance.xyz - vec3(halfSize);
    vBoxMax = instance.xyz + vec3(halfSize);
    vColor = tint.rgb;
    vAlpha = tint.w;
    vViewDist = length(position - u.aCamera.xyz);
    vIsDeck = signedHalf < 0.0 ? 1.0 : 0.0;
    gl_Position = u.viewProj * vec4(position, 1.0);
}
