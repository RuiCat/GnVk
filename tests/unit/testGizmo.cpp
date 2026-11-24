// testGizmo.cpp — 编辑器基础控件（网格 / 坐标轴 / Gizmo / 包围盒 / 线框 / 样条）纯 CPU 单测。
//
// 这一层的价值全在"能逐条断言"：网格线对不对、拾取命中哪根轴、样条过不过点、
// 线框去重后边数对不对。这些都不需要 Vulkan，也不需要看画面。
#include "testFramework.h"

#include <GN.h>
#include "GN_Gizmo.h"

#include <cmath>
#include <cstring>
#include <vector>

using namespace GN3d;

namespace {

GNg::sVEC3 vec(float x, float y, float z) {
    GNg::sVEC3 v = {x, y, z};
    return v;
}

// 把 Y 轴转到 (0,1,0) → Z 轴转到别处的单位四元数（绕 Y 转 90°）
GNg::sQUAT rotY90() {
    const float half = 0.7853981633974483f; // pi/4
    return GNg::sQUAT(0.0f, std::sin(half), 0.0f, std::cos(half));
}

} // namespace

// ── 网格 ───────────────────────────────────────────────
TEST_CASE(gizmoGridLineCount) {
    sGRID_DESC desc;
    desc.aHalfCells = 10;
    desc.aCellSize = 1.0f;
    // 每个方向 2*10+1 = 21 条，两个方向共 42
    CHECK_EQ(gridLineCount(desc), 42u);

    std::vector<sEDIT_LINE> lines;
    buildGridLines(desc, lines);
    CHECK_EQ((uint32_t)lines.size(), gridLineCount(desc));

    // 非法参数：不产生线
    sGRID_DESC bad;
    bad.aCellSize = 0.0f;
    CHECK_EQ(gridLineCount(bad), 0u);
    std::vector<sEDIT_LINE> empty;
    buildGridLines(bad, empty);
    CHECK_EQ((uint32_t)empty.size(), 0u);
}

TEST_CASE(gizmoGridUsesAxisColorsAtOrigin) {
    sGRID_DESC desc;
    desc.aHalfCells = 4;
    desc.aCellSize = 2.0f;
    desc.aDrawAxes = true;
    std::vector<sEDIT_LINE> lines;
    buildGridLines(desc, lines);

    uint32_t axisX = 0;
    uint32_t axisZ = 0;
    uint32_t other = 0;
    for (size_t i = 0; i < lines.size(); ++i) {
        if (lines[i].aColor == gizmoColor::kAxisX) ++axisX;
        else if (lines[i].aColor == gizmoColor::kAxisZ) ++axisZ;
        else ++other;
    }
    // 中心两条线各 1 条，且必须是轴语义色（否则原点处的轴会被网格盖掉）
    CHECK_EQ(axisX, 1u);
    CHECK_EQ(axisZ, 1u);
    CHECK_EQ(other, gridLineCount(desc) - 2u);
}

TEST_CASE(gizmoGridExtentMatchesCells) {
    sGRID_DESC desc;
    desc.aHalfCells = 3;
    desc.aCellSize = 2.5f;
    desc.aFade = false;
    std::vector<sEDIT_LINE> lines;
    buildGridLines(desc, lines);
    // 网格覆盖 ±(3 * 2.5) = ±7.5
    float minCoord = 1.0e9f;
    float maxCoord = -1.0e9f;
    for (size_t i = 0; i < lines.size(); ++i) {
        const float values[6] = {lines[i].aStart.x, lines[i].aStart.z, lines[i].aEnd.x,
                                 lines[i].aEnd.z, lines[i].aStart.y, lines[i].aEnd.y};
        if (values[0] < minCoord) minCoord = values[0];
        if (values[2] > maxCoord) maxCoord = values[2];
        if (values[1] < minCoord) minCoord = values[1];
        if (values[3] > maxCoord) maxCoord = values[3];
        // 网格必须在 y = 0 平面上（水平）
        CHECK_NEAR(values[4], 0.0f, 1e-6);
        CHECK_NEAR(values[5], 0.0f, 1e-6);
    }
    CHECK_NEAR(minCoord, -7.5f, 1e-5);
    CHECK_NEAR(maxCoord, 7.5f, 1e-5);
}

// ── 坐标轴 ─────────────────────────────────────────────
TEST_CASE(gizmoAxesLines) {
    std::vector<sEDIT_LINE> lines;
    buildAxesLines(vec(0.0f, 1.0f, 0.0f), 5.0f, lines);
    // 每根轴：1 条主线 + 4 条箭头 = 5，三根共 15
    CHECK_EQ((uint32_t)lines.size(), 15u);
    // 颜色分配：各 5 条
    uint32_t counts[3] = {0, 0, 0};
    for (size_t i = 0; i < lines.size(); ++i) {
        if (lines[i].aColor == gizmoColor::kAxisX) ++counts[0];
        else if (lines[i].aColor == gizmoColor::kAxisY) ++counts[1];
        else if (lines[i].aColor == gizmoColor::kAxisZ) ++counts[2];
    }
    CHECK_EQ(counts[0], 5u);
    CHECK_EQ(counts[1], 5u);
    CHECK_EQ(counts[2], 5u);
    // 长度非法：不产生线
    std::vector<sEDIT_LINE> bad;
    buildAxesLines(vec(0.0f, 0.0f, 0.0f), 0.0f, bad);
    CHECK_EQ((uint32_t)bad.size(), 0u);
}

// ── 包围盒 ─────────────────────────────────────────────
TEST_CASE(gizmoBoxLines) {
    sAABB box;
    box.aMin = vec(-1.0f, -2.0f, -3.0f);
    box.aMax = vec(1.0f, 2.0f, 3.0f);
    std::vector<sEDIT_LINE> lines;
    buildBoxLines(box, gizmoColor::kBoundingBox, lines);
    CHECK_EQ((uint32_t)lines.size(), 12u);

    // 12 条棱的每个端点都必须落在盒的角上
    for (size_t i = 0; i < lines.size(); ++i) {
        const GNg::sVEC3 points[2] = {lines[i].aStart, lines[i].aEnd};
        for (int k = 0; k < 2; ++k) {
            bool onCorner = false;
            for (int axis = 0; axis < 3; ++axis) {
                const float value = (axis == 0) ? points[k].x : ((axis == 1) ? points[k].y : points[k].z);
                const float lo = (axis == 0) ? box.aMin.x : ((axis == 1) ? box.aMin.y : box.aMin.z);
                const float hi = (axis == 0) ? box.aMax.x : ((axis == 1) ? box.aMax.y : box.aMax.z);
                if (std::fabs(value - lo) > 1e-5f && std::fabs(value - hi) > 1e-5f) onCorner = false;
                (void)onCorner;
            }
            // 更直接的检查：每个坐标必须等于 min 或 max
            CHECK(std::fabs(std::fabs(points[k].x) - 1.0f) < 1e-5f);
            CHECK(std::fabs(std::fabs(points[k].y) - 2.0f) < 1e-5f);
            CHECK(std::fabs(std::fabs(points[k].z) - 3.0f) < 1e-5f);
        }
    }

    // 空盒：不产生线
    sAABB empty;
    std::vector<sEDIT_LINE> none;
    buildBoxLines(empty, gizmoColor::kBoundingBox, none);
    CHECK_EQ((uint32_t)none.size(), 0u);
}

TEST_CASE(gizmoBoxCornerMarks) {
    sAABB box;
    box.aMin = vec(0.0f, 0.0f, 0.0f);
    box.aMax = vec(2.0f, 2.0f, 2.0f);
    std::vector<sEDIT_LINE> lines;
    buildBoxCornerMarks(box, 0.2f, gizmoColor::kBoundingBox, lines);
    CHECK_EQ((uint32_t)lines.size(), 24u); // 8 角 × 3
    // 每段长度 = extent * tickFraction = 1 * 0.2
    for (size_t i = 0; i < lines.size(); ++i) {
        const GNg::sVEC3 d = vec(lines[i].aEnd.x - lines[i].aStart.x,
                                 lines[i].aEnd.y - lines[i].aStart.y,
                                 lines[i].aEnd.z - lines[i].aStart.z);
        const float len = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
        CHECK_NEAR(len, 0.2f, 1e-5);
    }
}

// ── 线框 ───────────────────────────────────────────────
// 一个正方形的两条三角形共享一条对角线：去重后应当是 5 条边（4 边 + 1 对角线），
// 而不是 6 条（不去重会把对角线画两次）。
TEST_CASE(gizmoWireframeDeduplicatesSharedEdges) {
    // 顶点布局：11 floats/顶点，只读前 3
    float vertices[4 * 11];
    std::memset(vertices, 0, sizeof(vertices));
    const float positions[4][3] = {{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 1.0f},
                                   {0.0f, 0.0f, 1.0f}};
    for (int i = 0; i < 4; ++i) {
        vertices[i * 11 + 0] = positions[i][0];
        vertices[i * 11 + 1] = positions[i][1];
        vertices[i * 11 + 2] = positions[i][2];
    }
    const uint32_t indices[6] = {0, 1, 2, 0, 2, 3};

    std::vector<sEDIT_LINE> lines;
    buildIndexedWireframeLines(vertices, 4, indices, 6, nullptr, gizmoColor::kWireframe, lines);
    CHECK_EQ((uint32_t)lines.size(), 5u); // 4 条外边 + 1 条共享对角线

    // 带变换：所有点应被平移
    GNg::sMAT44 translate;
    GNg::Mat44Identity(&translate);
    translate.m[12] = 10.0f;
    std::vector<sEDIT_LINE> moved;
    buildIndexedWireframeLines(vertices, 4, indices, 6, &translate, gizmoColor::kWireframe, moved);
    CHECK_EQ((uint32_t)moved.size(), 5u);
    for (size_t i = 0; i < moved.size(); ++i) {
        CHECK(moved[i].aStart.x >= 10.0f - 1e-5f);
        CHECK(moved[i].aEnd.x >= 10.0f - 1e-5f);
    }

    // 空输入：不产生线
    std::vector<sEDIT_LINE> none;
    buildIndexedWireframeLines(nullptr, 4, indices, 6, nullptr, 0xFFFFFFFFu, none);
    CHECK_EQ((uint32_t)none.size(), 0u);
}

// ── 样条 ───────────────────────────────────────────────
TEST_CASE(gizmoSplinePassesThroughControlPoints) {
    const GNg::sVEC3 points[4] = {vec(0.0f, 0.0f, 0.0f), vec(1.0f, 2.0f, 0.0f),
                                  vec(2.0f, -1.0f, 1.0f), vec(3.0f, 0.5f, 0.0f)};
    sSPLINE_DESC desc; // Catmull-Rom，开放
    desc.aSamplesPerSegment = 8;

    // 端点必须是首尾控制点
    const GNg::sVEC3 start = evaluateSpline(points, 4, desc, 0.0f);
    const GNg::sVEC3 end = evaluateSpline(points, 4, desc, 1.0f);
    CHECK_NEAR(start.x, points[0].x, 1e-5);
    CHECK_NEAR(start.y, points[0].y, 1e-5);
    CHECK_NEAR(end.x, points[3].x, 1e-5);
    CHECK_NEAR(end.y, points[3].y, 1e-5);
    CHECK_NEAR(end.z, points[3].z, 1e-5);

    // 每个控制点都必须出现在采样序列里（"过点"是 Catmull-Rom 的定义性质）
    std::vector<GNg::sVEC3> samples;
    sampleSpline(points, 4, desc, samples);
    CHECK_EQ((uint32_t)samples.size(), 3u * 8u + 1u);
    for (int p = 0; p < 4; ++p) {
        float best = 1.0e9f;
        for (size_t s = 0; s < samples.size(); ++s) {
            const float dx = samples[s].x - points[p].x;
            const float dy = samples[s].y - points[p].y;
            const float dz = samples[s].z - points[p].z;
            const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
            if (distance < best) best = distance;
        }
        CHECK_NEAR(best, 0.0f, 1e-5);
    }
}

TEST_CASE(gizmoSplineDegenerateCases) {
    sSPLINE_DESC desc;
    // 0 点：原点
    const GNg::sVEC3 zero = evaluateSpline(nullptr, 0, desc, 0.5f);
    CHECK_NEAR(zero.x, 0.0f, 1e-6);
    // 1 点：恒等于该点
    const GNg::sVEC3 single[1] = {vec(3.0f, 4.0f, 5.0f)};
    const GNg::sVEC3 one = evaluateSpline(single, 1, desc, 0.3f);
    CHECK_NEAR(one.x, 3.0f, 1e-6);
    CHECK_NEAR(one.y, 4.0f, 1e-6);
    // 2 点：退化成直线，中点 = 两端平均
    const GNg::sVEC3 two[2] = {vec(0.0f, 0.0f, 0.0f), vec(2.0f, 4.0f, 6.0f)};
    const GNg::sVEC3 mid = evaluateSpline(two, 2, desc, 0.5f);
    CHECK_NEAR(mid.x, 1.0f, 1e-5);
    CHECK_NEAR(mid.y, 2.0f, 1e-5);
    CHECK_NEAR(mid.z, 3.0f, 1e-5);
    // t 越界：钳到 [0,1]
    const GNg::sVEC3 clamped = evaluateSpline(two, 2, desc, 5.0f);
    CHECK_NEAR(clamped.x, 2.0f, 1e-5);
}

TEST_CASE(gizmoSplineClosedLoops) {
    const GNg::sVEC3 square[4] = {vec(0.0f, 0.0f, 0.0f), vec(1.0f, 0.0f, 0.0f),
                                  vec(1.0f, 0.0f, 1.0f), vec(0.0f, 0.0f, 1.0f)};
    sSPLINE_DESC desc;
    desc.aClosed = true;
    desc.aSamplesPerSegment = 8;
    // 闭环：t=0 与 t=1 必须是同一点
    const GNg::sVEC3 start = evaluateSpline(square, 4, desc, 0.0f);
    const GNg::sVEC3 end = evaluateSpline(square, 4, desc, 1.0f);
    CHECK_NEAR(start.x, end.x, 1e-5);
    CHECK_NEAR(start.z, end.z, 1e-5);
    // 段数 = count（闭环多一段回原点）
    std::vector<GNg::sVEC3> samples;
    sampleSpline(square, 4, desc, samples);
    CHECK_EQ((uint32_t)samples.size(), 4u * 8u + 1u);
}

TEST_CASE(gizmoSplineIsContinuous) {
    // 相邻采样点的位移必须连续变化（C1 曲线的判据之一：没有突跳）
    const GNg::sVEC3 points[5] = {vec(0.0f, 0.0f, 0.0f), vec(1.0f, 1.0f, 0.0f),
                                  vec(2.0f, 0.0f, 1.0f), vec(3.0f, 2.0f, 0.0f),
                                  vec(4.0f, 0.0f, 1.0f)};
    sSPLINE_DESC desc;
    desc.aSamplesPerSegment = 12;
    std::vector<GNg::sVEC3> samples;
    sampleSpline(points, 5, desc, samples);

    float previousStep = -1.0f;
    float maxJump = 0.0f;
    for (size_t i = 1; i < samples.size(); ++i) {
        const float dx = samples[i].x - samples[i - 1].x;
        const float dy = samples[i].y - samples[i - 1].y;
        const float dz = samples[i].z - samples[i - 1].z;
        const float step = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (previousStep > 0.0f) {
            const float jump = std::fabs(step - previousStep);
            if (jump > maxJump) maxJump = jump;
        }
        previousStep = step;
    }
    // 相邻步长的变化必须远小于步长本身（折线会有大跳变）
    CHECK(maxJump < 0.25f);
}

TEST_CASE(gizmoSplineBezierAndLinear) {
    const GNg::sVEC3 points[3] = {vec(0.0f, 0.0f, 0.0f), vec(1.0f, 3.0f, 0.0f),
                                  vec(2.0f, 0.0f, 0.0f)};
    // 线性模式：中点必须在两点连线上
    sSPLINE_DESC linear;
    linear.aMode = eSPLINE_LINEAR;
    const GNg::sVEC3 mid = evaluateSpline(points, 3, linear, 0.25f); // 第一段中点
    CHECK_NEAR(mid.x, 0.5f, 1e-5);
    CHECK_NEAR(mid.y, 1.5f, 1e-5);

    // 贝塞尔模式：端点同样必须过控制点
    sSPLINE_DESC bezier;
    bezier.aMode = eSPLINE_BEZIER;
    const GNg::sVEC3 start = evaluateSpline(points, 3, bezier, 0.0f);
    const GNg::sVEC3 end = evaluateSpline(points, 3, bezier, 1.0f);
    CHECK_NEAR(start.x, 0.0f, 1e-5);
    CHECK_NEAR(end.x, 2.0f, 1e-5);
    // 中间必然离开直线（贝塞尔向内收）
    const GNg::sVEC3 bezierMid = evaluateSpline(points, 3, bezier, 0.5f);
    CHECK(bezierMid.y > 0.0f);
}

TEST_CASE(gizmoSplineHandles) {
    const GNg::sVEC3 points[3] = {vec(0.0f, 0.0f, 0.0f), vec(1.0f, 1.0f, 0.0f),
                                  vec(2.0f, 0.0f, 0.0f)};
    std::vector<sEDIT_LINE> lines;
    buildSplineHandles(points, 3, 0.05f, true, lines);
    // 每个控制点一个立方体（12 段）= 36，加 3 段虚线多边形（每段 3 条）
    CHECK_EQ((uint32_t)lines.size(), 36u + 3u * 3u);
    // 不画多边形：只有立方体
    std::vector<sEDIT_LINE> noPolygon;
    buildSplineHandles(points, 3, 0.05f, false, noPolygon);
    CHECK_EQ((uint32_t)noPolygon.size(), 36u);
}

// ── Gizmo 几何 ─────────────────────────────────────────
TEST_CASE(gizmoTranslateGeometry) {
    sGIZMO_DESC desc;
    desc.aMode = eGIZMO_TRANSLATE;
    desc.aOrigin = vec(1.0f, 2.0f, 3.0f);
    desc.aSize = 2.0f;
    std::vector<sEDIT_LINE> lines;
    buildGizmoLines(desc, lines);
    // 每轴：1 轴线 + 4 箭头 = 5，三轴 15；加中心方块 12 = 27
    CHECK_EQ((uint32_t)lines.size(), 27u);
    // 三轴的轴线起点都必须是 origin
    int axisLines = 0;
    for (size_t i = 0; i < lines.size(); ++i) {
        const bool fromOrigin = std::fabs(lines[i].aStart.x - 1.0f) < 1e-5f &&
                                std::fabs(lines[i].aStart.y - 2.0f) < 1e-5f &&
                                std::fabs(lines[i].aStart.z - 3.0f) < 1e-5f;
        if (fromOrigin && (lines[i].aColor == gizmoColor::kAxisX ||
                           lines[i].aColor == gizmoColor::kAxisY ||
                           lines[i].aColor == gizmoColor::kAxisZ)) {
            ++axisLines;
        }
    }
    CHECK_EQ(axisLines, 3);
}

TEST_CASE(gizmoRotateGeometry) {
    sGIZMO_DESC desc;
    desc.aMode = eGIZMO_ROTATE;
    desc.aOrigin = vec(0.0f, 0.0f, 0.0f);
    desc.aSize = 1.0f;
    std::vector<sEDIT_LINE> lines;
    buildGizmoLines(desc, lines);
    // 每环 48 段，三环 = 144；旋转模式不画中心方块
    CHECK_EQ((uint32_t)lines.size(), 144u);

    // 每个顶点到轴心的距离都必须等于环半径 0.86
    const float radius = 0.86f;
    for (size_t i = 0; i < lines.size(); ++i) {
        const GNg::sVEC3 points[2] = {lines[i].aStart, lines[i].aEnd};
        for (int k = 0; k < 2; ++k) {
            const float length = std::sqrt(points[k].x * points[k].x +
                                           points[k].y * points[k].y +
                                           points[k].z * points[k].z);
            CHECK_NEAR(length, radius, 1e-3);
        }
    }
}

TEST_CASE(gizmoScaleGeometry) {
    sGIZMO_DESC desc;
    desc.aMode = eGIZMO_SCALE;
    desc.aSize = 1.0f;
    std::vector<sEDIT_LINE> lines;
    buildGizmoLines(desc, lines);
    // 每轴：1 轴线 + 末端方块 12 = 13，三轴 39；加中心方块 12 = 51
    CHECK_EQ((uint32_t)lines.size(), 51u);
}

TEST_CASE(gizmoHighlightRecolorsAxis) {
    sGIZMO_DESC desc;
    desc.aMode = eGIZMO_TRANSLATE;
    desc.aSize = 1.0f;
    desc.aHighlight = eGIZMO_AXIS_Y;
    std::vector<sEDIT_LINE> lines;
    buildGizmoLines(desc, lines);
    uint32_t highlight = 0;
    uint32_t axisY = 0;
    for (size_t i = 0; i < lines.size(); ++i) {
        if (lines[i].aColor == gizmoColor::kHighlight) ++highlight;
        if (lines[i].aColor == gizmoColor::kAxisY) ++axisY;
    }
    CHECK_EQ(highlight, 5u); // Y 轴的 5 条全变成高亮色
    CHECK_EQ(axisY, 0u); // 原色消失
}

TEST_CASE(gizmoLocalSpaceUsesBasis) {
    sGIZMO_DESC desc;
    desc.aLocalSpace = true;
    gizmoBasisFromRotation(rotY90(), &desc.aBasis);
    // 绕 Y 转 90°：局部 X 轴 → 世界 -Z（或 +Z，取决于四元数朝向），
    // 只断言"不是世界 X 轴"这一个性质：局部空间真的生效了
    const GNg::sVEC3 localX = gizmoAxisVector(desc, eGIZMO_AXIS_X);
    CHECK(std::fabs(std::fabs(localX.z) - 1.0f) < 1e-4f);
    CHECK(std::fabs(localX.x) < 1e-4f);
    // 局部 Y 轴在世界里仍是 Y（绕 Y 转不改变 Y）
    const GNg::sVEC3 localY = gizmoAxisVector(desc, eGIZMO_AXIS_Y);
    CHECK_NEAR(localY.y, 1.0f, 1e-4);

    // 全局空间：一定是世界轴
    sGIZMO_DESC world;
    const GNg::sVEC3 worldX = gizmoAxisVector(world, eGIZMO_AXIS_X);
    CHECK_NEAR(worldX.x, 1.0f, 1e-6);
    CHECK_NEAR(worldX.y, 0.0f, 1e-6);
}

TEST_CASE(gizmoScreenSizeScalesWithDistance) {
    // 距离翻倍 ⇒ 尺寸翻倍（屏幕上大小不变）
    const float near = gizmoScreenSize(10.0f, 0.8f, 0.15f);
    const float far = gizmoScreenSize(20.0f, 0.8f, 0.15f);
    CHECK_NEAR(far, near * 2.0f, 1e-4);
    // 非法输入有兜底
    CHECK_NEAR(gizmoScreenSize(0.0f, 0.8f, 0.15f), 1.0f, 1e-6);
    CHECK_NEAR(gizmoScreenSize(10.0f, 0.0f, 0.15f), 1.0f, 1e-6);
}

// ── 拾取 ───────────────────────────────────────────────
TEST_CASE(gizmoPickTranslateAxis) {
    sGIZMO_DESC desc;
    desc.aMode = eGIZMO_TRANSLATE;
    desc.aOrigin = vec(0.0f, 0.0f, 0.0f);
    desc.aSize = 1.0f;

    // 从 +Z 方向看向原点，射线瞄准 +X 轴中点 (0.7, 0, 0)
    {
        const GNg::sVEC3 rayOrigin = vec(0.7f, 0.0f, 5.0f);
        const GNg::sVEC3 rayDir = vec(0.0f, 0.0f, -1.0f);
        CHECK_EQ((int)pickGizmo(desc, rayOrigin, rayDir, 0.1f), (int)eGIZMO_AXIS_X);
    }
    // 瞄准 +Y 轴
    {
        const GNg::sVEC3 rayOrigin = vec(0.0f, 0.7f, 5.0f);
        const GNg::sVEC3 rayDir = vec(0.0f, 0.0f, -1.0f);
        CHECK_EQ((int)pickGizmo(desc, rayOrigin, rayDir, 0.1f), (int)eGIZMO_AXIS_Y);
    }
    // 瞄准 +Z 轴：从侧面（+X）看
    {
        const GNg::sVEC3 rayOrigin = vec(5.0f, 0.0f, 0.7f);
        const GNg::sVEC3 rayDir = vec(-1.0f, 0.0f, 0.0f);
        CHECK_EQ((int)pickGizmo(desc, rayOrigin, rayDir, 0.1f), (int)eGIZMO_AXIS_Z);
    }
    // 瞄偏：什么都不命中
    {
        const GNg::sVEC3 rayOrigin = vec(3.0f, 3.0f, 5.0f);
        const GNg::sVEC3 rayDir = vec(0.0f, 0.0f, -1.0f);
        CHECK_EQ((int)pickGizmo(desc, rayOrigin, rayDir, 0.1f), (int)eGIZMO_AXIS_NONE);
    }
    // 中心方块
    {
        const GNg::sVEC3 rayOrigin = vec(0.0f, 0.0f, 5.0f);
        const GNg::sVEC3 rayDir = vec(0.0f, 0.0f, -1.0f);
        CHECK_EQ((int)pickGizmo(desc, rayOrigin, rayDir, 0.1f) >= 0, 1);
    }
}

TEST_CASE(gizmoPickRotateRing) {
    sGIZMO_DESC desc;
    desc.aMode = eGIZMO_ROTATE;
    desc.aSize = 1.0f;
    const float radius = 0.86f;

    // Z 环在 XY 平面上：从 +Z 看向 (radius, 0, 0)
    {
        const GNg::sVEC3 rayOrigin = vec(radius, 0.0f, 5.0f);
        const GNg::sVEC3 rayDir = vec(0.0f, 0.0f, -1.0f);
        CHECK_EQ((int)pickGizmo(desc, rayOrigin, rayDir, 0.12f), (int)eGIZMO_AXIS_Z);
    }
    // Y 环在 XZ 平面上：从 +Y 看向 (radius, 0, 0) —— 这里 X 环与 Y 环都经过该点，
    // 断言"命中了某一个"而不指定具体哪根（这是设计上的歧义区）
    {
        const GNg::sVEC3 rayOrigin = vec(radius, 5.0f, 0.0f);
        const GNg::sVEC3 rayDir = vec(0.0f, -1.0f, 0.0f);
        const eGIZMO_AXIS hit = pickGizmo(desc, rayOrigin, rayDir, 0.12f);
        CHECK(hit != eGIZMO_AXIS_NONE);
    }
    // 瞄准环外（半径 3 倍处）：不命中
    {
        const GNg::sVEC3 rayOrigin = vec(radius * 3.0f, 0.0f, 5.0f);
        const GNg::sVEC3 rayDir = vec(0.0f, 0.0f, -1.0f);
        CHECK_EQ((int)pickGizmo(desc, rayOrigin, rayDir, 0.12f), (int)eGIZMO_AXIS_NONE);
    }
}

// ── 拖拽 ───────────────────────────────────────────────
TEST_CASE(gizmoDragTranslateAlongAxis) {
    sGIZMO_DESC desc;
    desc.aSize = 1.0f;
    // 沿 +X 拖：射线从 (0,0,5) 对准 (1.5,0,0) ⇒ 投影到 X 轴应是 1.5
    const float projection = gizmoDragTranslate(desc, eGIZMO_AXIS_X, vec(0.0f, 0.0f, 5.0f),
                                                vec(1.5f, 0.0f, -5.0f));
    CHECK_NEAR(projection, 1.5f, 1e-3);
    // 同一根轴、不同起点 ⇒ 差值就是位移量
    const float moved = gizmoDragTranslate(desc, eGIZMO_AXIS_X, vec(0.0f, 0.0f, 5.0f),
                                          vec(2.75f, 0.0f, -5.0f));
    CHECK_NEAR(moved - projection, 1.25f, 1e-3);
    // 非法轴：0
    CHECK_NEAR(gizmoDragTranslate(desc, eGIZMO_AXIS_NONE, vec(0.0f, 0.0f, 5.0f),
                                  vec(1.0f, 0.0f, -1.0f)),
               0.0f, 1e-6);
}

TEST_CASE(gizmoDragRotateAroundAxis) {
    sGIZMO_DESC desc;
    desc.aSize = 1.0f;
    // 绕 Z 轴：起点在 (1,0,0)，当前在 (0,1,0) ⇒ 角度 +90°
    const float delta = gizmoDragRotate(desc, eGIZMO_AXIS_Z, vec(1.0f, 0.0f, 0.0f),
                                        vec(0.0f, 1.0f, 5.0f), vec(0.0f, 0.0f, -1.0f));
    CHECK_NEAR(std::fabs(delta), 1.5707963f, 0.02f);

    // 回到起点 ⇒ 角度 ~0（不是 2π）
    const float zero = gizmoDragRotate(desc, eGIZMO_AXIS_Z, vec(1.0f, 0.0f, 0.0f),
                                       vec(1.0f, 0.0f, 5.0f), vec(0.0f, 0.0f, -1.0f));
    CHECK_NEAR(zero, 0.0f, 0.02f);
}

TEST_CASE(gizmoDragScaleRatio) {
    sGIZMO_DESC desc;
    desc.aSize = 1.0f;
    // 起始投影 1.0，当前 2.0 ⇒ 倍率 2
    const float ratio = gizmoDragScale(desc, eGIZMO_AXIS_X, vec(0.0f, 0.0f, 5.0f),
                                       vec(2.0f, 0.0f, -5.0f), 1.0f);
    CHECK_NEAR(ratio, 2.0f, 1e-3);
    // 起始投影为 0：返回 1（不缩放，而不是无穷大）
    CHECK_NEAR(gizmoDragScale(desc, eGIZMO_AXIS_X, vec(0.0f, 0.0f, 5.0f),
                              vec(2.0f, 0.0f, -5.0f), 0.0f),
               1.0f, 1e-6);
    // 负投影（拖到轴反方向）：倍率钳到正数，不翻负
    const float clamped = gizmoDragScale(desc, eGIZMO_AXIS_X, vec(0.0f, 0.0f, 5.0f),
                                         vec(-2.0f, 0.0f, -5.0f), 1.0f);
    CHECK(clamped > 0.0f);
}
