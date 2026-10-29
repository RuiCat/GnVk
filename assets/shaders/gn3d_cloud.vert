#version 450
// ╔══════════════════════════════════════════════════════════╗
// ║  GN_mini — 中景 billboard 云层顶点着色器（三层云盘）      ║
// ║  无顶点缓冲：gl_VertexIndex 展开 tile 四边形              ║
// ╚══════════════════════════════════════════════════════════╝
//
// 与 C++ 的 sCLOUD_UBO（160 B，set 0 binding 0）逐字段对应：
//   viewProj     世界 → 裁剪（列主序）
//   cameraPos    xyz = 相机世界位置, w = nearFade（m，0 = 关闭）
//   windTime     x/y = 风 (x,z), z = 时间（秒）, w 未用
//   params0      x = tileSize, y = tilesPerAxis, z = coverage, w = sharpness
//   params1      x = opacity, y = layerHeight（底层）, z = layerCount, w = layerSpacing
//   cameraRight  xyz = 相机右向的水平投影（单位，CPU 取 viewProj 第 0 行 = P00·相机右向）
//   cameraUp     xyz = 世界 +Y（云层法线）
//
// 分层：同一 draw 里 tiles²×layerCount 个四边形；layer = quadIndex / tiles²，
// 第 L 层高度 = layerHeight - L*layerSpacing，tile 尺寸 ×(1+0.15L)、风速 ×(1+0.35L)，
// 片元按层给不同的 coverage 偏置与明暗 ⇒ 云层有层叠感而不是一整片灰雾。
// 布局：每层是 y = 层高度的水平面，面内两轴 = cameraRight 的水平投影与 cross(+Y, right)；
// tile 格点在同一正交基上铺开 ⇒ 相邻四边形共边，无接缝。

layout(location = 0) out vec3 fragWorldPos;
layout(location = 1) out float fragLayer;

layout(binding = 0, set = 0) uniform CloudUBO {
    mat4 viewProj;
    vec4 cameraPos;
    vec4 windTime;
    vec4 params0;
    vec4 params1;
    vec4 cameraRight;
    vec4 cameraUp;
} cloud;

// 每片 6 顶点 = 2 三角形：角点顺序查表 {0,1,2, 0,2,3}。
const int kCornerIndex[6] = int[6](0, 1, 2, 0, 2, 3);
const int kCornerX[4] = int[4](0, 1, 1, 0);
const int kCornerY[4] = int[4](0, 0, 1, 1);

void main() {
    const uint tilesPerAxis = uint(max(cloud.params0.y, 1.0));
    const uint quadsPerLayer = tilesPerAxis * tilesPerAxis;
    const uint vertexIndex = uint(gl_VertexIndex);
    const uint quadIndex = vertexIndex / 6u;
    const uint corner = uint(kCornerIndex[vertexIndex - quadIndex * 6u]);

    const uint layer = quadIndex / quadsPerLayer;
    const uint layerQuad = quadIndex - layer * quadsPerLayer;
    const float layerFloat = float(layer);
    const float tileSize = cloud.params0.x * (1.0 + 0.15 * layerFloat);
    const float layerHeight = cloud.params1.y - layerFloat * cloud.params1.w;
    const float windScale = 1.0 + 0.35 * layerFloat;

    const uint tileX = layerQuad - (layerQuad / tilesPerAxis) * tilesPerAxis;
    const uint tileZ = layerQuad / tilesPerAxis;

    // 面内正交基：right 已由 CPU 归一化到水平面；forward = cross(+Y, right) 仍是水平单位向量。
    vec2 rightXZ = cloud.cameraRight.xz;
    vec2 forwardXZ = normalize(cross(cloud.cameraUp.xyz, vec3(rightXZ.x, 0.0, rightXZ.y)).xz);

    // 网格原点按相机 xz 取模对齐（每层用自己的 tile 尺寸），再随 wind × time 漂移；
    // -(tiles/2) 让整片云层以相机为中心。
    vec2 anchor = floor(cloud.cameraPos.xz / tileSize + 0.5) * tileSize;
    vec2 tileOffset = (vec2(float(tileX), float(tileZ)) + 0.5 - float(tilesPerAxis) * 0.5) * tileSize;
    vec2 centerXZ = anchor + rightXZ * tileOffset.x + forwardXZ * tileOffset.y +
                    cloud.windTime.xy * (cloud.windTime.z * windScale);

    vec2 cornerOffset = (vec2(float(kCornerX[corner]), float(kCornerY[corner])) - 0.5) * tileSize;
    vec2 offsetXZ = rightXZ * cornerOffset.x + forwardXZ * cornerOffset.y;
    vec3 worldPos = vec3(centerXZ.x + offsetXZ.x, layerHeight, centerXZ.y + offsetXZ.y);

    fragWorldPos = worldPos;
    fragLayer = layerFloat;
    gl_Position = cloud.viewProj * vec4(worldPos, 1.0);
}
