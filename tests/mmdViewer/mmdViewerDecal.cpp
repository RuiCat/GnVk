// tests/mmdViewer/mmdViewerDecal.cpp — MMD 查看器：地面 / 贴花 / 调试线框
//
// 职责：地面与脚下面片的绘制、雨滴溅射贴花池的创建/更新/绘制、
//       世界坐标轴与模型世界 AABB 线框（G 键）。
//       不涉及相机、天气状态机与叠层文字（那些在各自模块里）。

#include "mmdViewerShared.h"
#include "GN_Physics.h" // sDEBUG_LINE（P8 调试线框）

#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

namespace {

// P11-S6b：地面贴花（雨滴溅射的水渍/环）。
GN3d::cDECAL_SYSTEM gDecals;
std::vector<GN3d::sDECAL_RENDER_ITEM> gDecalItems;
bool gDecalReady = false;
float gSplashAccumulator = 0.0f;
uint32_t gSplashRngState = 0x9E3779B9u;

} // namespace

// P11-S6d：包围盒 + 坐标轴可视化（G 键切换）。
bool gShowGizmos = true;
// 分系统开关（排查用）：J 关/开降水粒子，K 关/开地面溅射贴花。
bool gDecalEnabled = true;

// 地面高度取模型世界 AABB 的最低点（模型站在地面上）。
float groundHeight() {
    if (gModel.aWorldAABB.IsValid()) {
        return gModel.aWorldAABB.aMin.y;
    }
    return 0.0f;
}

namespace {

// 固定种子 xorshift32：同一天气序列的溅射位置可重复。
float splashRandom() {
    gSplashRngState ^= gSplashRngState << 13;
    gSplashRngState ^= gSplashRngState >> 17;
    gSplashRngState ^= gSplashRngState << 5;
    return (float)(gSplashRngState & 0xFFFFFFu) / 16777216.0f;
}

} // namespace

void createDecals() {
    if (!gDecals.create(512)) {
        std::fprintf(stderr, "[mmdViewer] 贴花池创建失败：雨天地面不会有水渍\n");
        return;
    }
    gDecalItems.resize(gDecals.capacity());
    gDecalReady = true;
}

// 地面：一块 400 m 的正方形，走 **PBR 管线**（`GN_VkDraw3DPBR`）。gpuCheck 的
// ground.placement 验证过这条路径；绕序 0,2,1 / 0,3,2 是 CCW（法线 +Y），
// 与 U2b 后 PBR 的 frontFace=CW 匹配（CCW 内容 + 投影 Y 翻转 ⇒ 帧缓冲 CW）。
void drawGround() {
    const float y = groundHeight() - 0.02f;
    const float half = 200.0f;
    const float xs[4] = {-half, half, half, -half};
    const float zs[4] = {-half, -half, half, half};
    float vertices[4 * 11];
    std::memset(vertices, 0, sizeof(vertices));
    for (int i = 0; i < 4; ++i) {
        float *v = vertices + i * 11;
        v[0] = xs[i];
        v[1] = y;
        v[2] = zs[i];
        // 法线 +Y：地面几何绕序是 CCW（0,2,1 → 法线 +Y）；旧的 -Y 是 PBR 负高度视口
        // 时期的镜像补丁，已在 U2 统一为正高度视口 + CCW 世界后废弃。
        v[3] = 0.0f;
        v[4] = 1.0f;
        v[5] = 0.0f;
        v[6] = 1.0f; // 切线 +X（布局：pos0-2 / normal3-5 / tangent6-8 / uv9-10）
        v[9] = (xs[i] + half) / (2.0f * half);
        v[10] = (zs[i] + half) / (2.0f * half);
    }
    // 地面走 PBR 管线；0,2,1 / 0,3,2 已是 CCW（法线 +Y），配合 U2b 的 PBR frontFace=CW。
    const uint32_t indices[6] = {0, 2, 1, 0, 3, 2};
    GNg::sMAT44 identity;
    GNg::Mat44Identity(&identity);
    const GNg::sMAT44 *model = &identity;
    GNg::GN_VkDraw3DPBR(vertices, 4, indices, 6, model, 0.45f, 0.46f, 0.42f, 0.95f, 0.0f, 1.0f);
}

// 脚下面片的半边长：绘制、包围盒与坐标叠层共用同一口径，避免三处各算一套。
float footQuadHalf() {
    return (gHeight > 1.0f ? gHeight : 1.0f) * 0.35f;
}

// 脚下一块普通四边形（走**前向管线**，和角色同一条路径），用来把「地面」和「普通场景面片」
// 分开看：它在脚下 ⇒ 前向管线与相机矩阵是对的，跑到头顶的只是只走 PBR 管线的地面；
// 它也在头顶 ⇒ 问题才在相机/矩阵。颜色用高饱和红，便于和灰色地面对照。
void drawFootQuad() {
    const float y = groundHeight() + 0.02f;
    const float half = footQuadHalf();
    const float xs[4] = {gTarget.x - half, gTarget.x + half, gTarget.x + half, gTarget.x - half};
    const float zs[4] = {gTarget.z - half, gTarget.z - half, gTarget.z + half, gTarget.z + half};
    float vertices[4 * 11];
    std::memset(vertices, 0, sizeof(vertices));
    for (int i = 0; i < 4; ++i) {
        float *v = vertices + i * 11;
        v[0] = xs[i];
        v[1] = y;
        v[2] = zs[i];
        v[3] = 0.0f; // 法线 +Y：前向管线是标准竖直约定
        v[4] = 1.0f;
        v[5] = 0.0f;
        v[6] = 1.0f; // 切线 +X（布局：pos0-2 / normal3-5 / tangent6-8 / uv9-10）
        v[9] = (i == 1 || i == 2) ? 1.0f : 0.0f;
        v[10] = (i >= 2) ? 1.0f : 0.0f;
    }
    // 与地面相同的绕序：正/反面判定在「PBR 负高度视口 + CW」和「前向正高度视口 + CCW」下等价。
    const uint32_t indices[6] = {0, 2, 1, 0, 3, 2};
    GNg::sMAT44 identity;
    GNg::Mat44Identity(&identity);
    GNg::GN_VkDraw3D(vertices, 4, indices, 6, &identity, 1.0f, 0.2f, 0.15f, 0.25f, 0.25f, 0.25f,
                     8.0f, nullptr);
}

namespace {

// 雨滴溅射：按雨强在角色周围的地面生成水渍 / 溅射环（单帧上限 64 条，避免爆发卡顿）。
void spawnSplashes(float dt) {
    if (!gDecalReady) {
        return;
    }
    const GN3d::WeatherOutput out = gWeather.output();
    const float groundY = groundHeight() + 0.02f;
    const float distance = cameraDistance() > 1.0f ? cameraDistance() : 1.0f;
    const float radius = distance * 0.6f + 4.0f;
    gSplashAccumulator += 220.0f * out.precipitation.rain * dt;
    uint32_t budget = 64;
    while (gSplashAccumulator >= 1.0f && budget > 0) {
        gSplashAccumulator -= 1.0f;
        --budget;
        const float angle = splashRandom() * 6.2831853f;
        const float dist = radius * std::sqrt(splashRandom());
        GN3d::sDECAL decal;
        std::memset(&decal, 0, sizeof(decal));
        decal.aPosition = {gTarget.x + std::cos(angle) * dist, groundY,
                           gTarget.z + std::sin(angle) * dist};
        decal.aSize = 0.25f + 0.45f * splashRandom();
        decal.aColor = GNg::sVEC4(0.78f, 0.84f, 0.95f, 0.55f);
        decal.aLife = 0.5f + 0.6f * splashRandom();
        decal.aKind = (splashRandom() < 0.35f) ? (uint32_t)GN3d::DecalKind::SPLASH_RING
                                                : (uint32_t)GN3d::DecalKind::SPOT;
        gDecals.spawn(decal);
    }
}

} // namespace

void updateDecals(float dt) {
    if (!gDecalReady) {
        return;
    }
    spawnSplashes(dt);
    gDecals.update(dt);
}

void drawDecals(const GNg::sMAT44 &viewProjection) {
    if (!gDecalEnabled || !gDecalReady || gDecalItems.empty()) {
        return;
    }
    const uint32_t count = GN3d::gatherDecalRenderItems(gDecals, gDecalItems.data(),
                                                        (uint32_t)gDecalItems.size());
    if (count == 0) {
        return;
    }
    GNg::sDECAL_DRAW_PARAMS params;
    params.aViewProj = viewProjection;
    params.aItems = gDecalItems.data();
    params.aCount = count;
    params.aAlphaScale = 1.0f;
    GNg::GN_VkDrawDecals(params);
}

namespace {

// 世界坐标轴：+X 红、+Y 绿、+Z 蓝（长度随模型高度缩放），画在原点。
void drawAxes() {
    const float len = (gHeight > 1.0f ? gHeight : 1.0f) * 0.6f;
    const GNg::sVEC3 origin = {0.0f, 0.0f, 0.0f};
    GN3d::sDEBUG_LINE axis[1];
    axis[0].aStart = origin;
    axis[0].aEnd = {len, 0.0f, 0.0f};
    GNg::GN_VkDrawDebugLines(axis, 1, 0xFFFF0000); // +X 红
    axis[0].aEnd = {0.0f, len, 0.0f};
    GNg::GN_VkDrawDebugLines(axis, 1, 0xFF00FF00); // +Y 绿
    axis[0].aEnd = {0.0f, 0.0f, len};
    GNg::GN_VkDrawDebugLines(axis, 1, 0xFF0000FF); // +Z 蓝
}

// 模型世界 AABB 的 12 条棱（黄色线框）；无效盒子不画。
void drawModelAabb() {
    if (!gModel.aWorldAABB.IsValid()) {
        return;
    }
    const GNg::sVEC3 &lo = gModel.aWorldAABB.aMin;
    const GNg::sVEC3 &hi = gModel.aWorldAABB.aMax;
    const GNg::sVEC3 corners[8] = {
        {lo.x, lo.y, lo.z}, {hi.x, lo.y, lo.z}, {hi.x, hi.y, lo.z}, {lo.x, hi.y, lo.z},
        {lo.x, lo.y, hi.z}, {hi.x, lo.y, hi.z}, {hi.x, hi.y, hi.z}, {lo.x, hi.y, hi.z}};
    const uint32_t edges[12][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6},
                                   {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
    GN3d::sDEBUG_LINE lines[12];
    for (int i = 0; i < 12; ++i) {
        lines[i].aStart = corners[edges[i][0]];
        lines[i].aEnd = corners[edges[i][1]];
    }
    GNg::GN_VkDrawDebugLines(lines, 12, 0xFFFFFF00); // 黄色 AABB
}

} // namespace

void drawGizmos() {
    if (!gShowGizmos) {
        return;
    }
    drawAxes();
    drawModelAabb();
}
