// GN_Gizmo.cpp — 编辑器基础控件的纯 CPU 几何/数学实现。
//
// 四条口径（每条都有用例，见 tests/unit/testGizmo.cpp）：
//   1. **网格线不重复**：每个方向的线只生成一次，中心两条线用轴语义色而不是普通网格色
//      （否则原点处的轴线会被网格线盖掉，看起来"轴缺了一段"）。
//   2. **线框去重**：三角形共享边只画一次。直接画三条边会让密集网格出现"某些边更粗"，
//      因为两条重合线在同一位置画两遍、alpha 混合后更亮。
//   3. **Gizmo 尺寸与屏幕成比例**：控件长度随相机距离缩放，否则远处看不见、近处塞满屏幕
//      （`gizmoScreenSize`）。
//   4. **拾取按"最近的轴"**：射线到轴线的距离小于阈值才算命中；多根轴同时命中时取距离
//      最小的那根 —— 否则在原点附近拖拽会随机选中轴。

#include <GN.h>
#include "GN_Gizmo.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace GN3d {

namespace {

const float kEps = 1.0e-6f;
const float kPi = 3.14159265358979323846f;

GNg::sVEC3 vec(float x, float y, float z) {
    GNg::sVEC3 v = {x, y, z};
    return v;
}
GNg::sVEC3 add(const GNg::sVEC3 &a, const GNg::sVEC3 &b) {
    return vec(a.x + b.x, a.y + b.y, a.z + b.z);
}
GNg::sVEC3 sub(const GNg::sVEC3 &a, const GNg::sVEC3 &b) {
    return vec(a.x - b.x, a.y - b.y, a.z - b.z);
}
GNg::sVEC3 mul(const GNg::sVEC3 &v, float s) {
    return vec(v.x * s, v.y * s, v.z * s);
}
float dot(const GNg::sVEC3 &a, const GNg::sVEC3 &b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}
GNg::sVEC3 cross(const GNg::sVEC3 &a, const GNg::sVEC3 &b) {
    return vec(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}
float lengthSq(const GNg::sVEC3 &v) {
    return dot(v, v);
}
bool normalize(GNg::sVEC3 &v) {
    const float sq = lengthSq(v);
    if (!(sq > 1.0e-12f)) return false;
    const float inv = 1.0f / std::sqrt(sq);
    v.x *= inv;
    v.y *= inv;
    v.z *= inv;
    return true;
}
uint32_t axisColor(int axis) {
    if (axis == 0) return gizmoColor::kAxisX;
    if (axis == 1) return gizmoColor::kAxisY;
    return gizmoColor::kAxisZ;
}

// 与 normal 不平行的任意单位向量（用于构造平面上的一组基）。
// 选"最不平行的坐标轴"而不是固定用 X：法线接近 X 时叉积会塌成零向量。
GNg::sVEC3 anyPerpendicular(const GNg::sVEC3 &normal) {
    const float ax = std::fabs(normal.x);
    const float ay = std::fabs(normal.y);
    const float az = std::fabs(normal.z);
    GNg::sVEC3 helper = (ax <= ay && ax <= az) ? vec(1.0f, 0.0f, 0.0f)
                                              : ((ay <= az) ? vec(0.0f, 1.0f, 0.0f) : vec(0.0f, 0.0f, 1.0f));
    GNg::sVEC3 out = cross(normal, helper);
    if (!normalize(out)) out = vec(1.0f, 0.0f, 0.0f);
    return out;
}

// 射线与直线（origin + dir*s）的最近点参数 s。返回 false 表示平行（无解）。
bool projectRayOnLine(const GNg::sVEC3 &lineOrigin, const GNg::sVEC3 &lineDir,
                      const GNg::sVEC3 &rayOrigin, const GNg::sVEC3 &rayDir, float &outS) {
    const GNg::sVEC3 w0 = sub(lineOrigin, rayOrigin);
    const float B = dot(lineDir, rayDir); // lineDir 已归一化，rayDir 也归一化
    const float D = dot(lineDir, w0);
    const float E = dot(rayDir, w0);
    const float denom = 1.0f - B * B; // A = C = 1
    if (std::fabs(denom) < kEps) return false; // 平行
    outS = (B * E - D) / denom;
    return true;
}

// 点到线段的距离（用于拾取：轴线段 = origin → origin + axis*size）。
float distanceRayToSegment(const GNg::sVEC3 &rayOrigin, const GNg::sVEC3 &rayDir,
                           const GNg::sVEC3 &segStart, const GNg::sVEC3 &segEnd) {
    GNg::sVEC3 segDir = sub(segEnd, segStart);
    const float segLength = std::sqrt(lengthSq(segDir));
    if (!(segLength > kEps)) return std::sqrt(lengthSq(sub(segStart, rayOrigin)));
    segDir = mul(segDir, 1.0f / segLength);

    float s = 0.0f;
    float t = 0.0f;
    const GNg::sVEC3 w0 = sub(segStart, rayOrigin);
    const float B = dot(segDir, rayDir);
    const float D = dot(segDir, w0);
    const float E = dot(rayDir, w0);
    const float denom = 1.0f - B * B;
    if (std::fabs(denom) < kEps) {
        s = 0.0f; // 平行：退化到线段起点
        t = E; // rayDir 归一化 ⇒ t 就是沿射线的距离
    } else {
        // 解 minimize |P(s) - Q(t)|²（P 在线段上、Q 在射线上）：
        //   ∂/∂s = 0 ⇒ s + D - tB = 0
        //   ∂/∂t = 0 ⇒ t - E - sB = 0 ⇒ t = E + sB
        // 第一版把 t 写成 (sB - E)，符号反了 ⇒ t 恒为负、被钳成 0，
        // 于是"射线上的点"永远是相机位置、距离恒等于原始距离 —— 拾取全部失效。
        s = (B * E - D) / denom;
        t = E + s * B;
    }
    // 钳到线段内
    if (s < 0.0f) s = 0.0f;
    if (s > segLength) s = segLength;
    if (t < 0.0f) t = 0.0f; // 射线不能往后退

    const GNg::sVEC3 onSegment = add(segStart, mul(segDir, s));
    const GNg::sVEC3 onRay = add(rayOrigin, mul(rayDir, t));
    return std::sqrt(lengthSq(sub(onSegment, onRay)));
}

void addLine(std::vector<sEDIT_LINE> &out, const GNg::sVEC3 &a, const GNg::sVEC3 &b,
             uint32_t color) {
    out.push_back(sEDIT_LINE(a, b, color));
}

// 一个以 center 为中心、半边长 half 的小立方体（12 条棱）。
void addCubeWire(std::vector<sEDIT_LINE> &out, const GNg::sVEC3 &center, float half,
                 uint32_t color) {
    const GNg::sVEC3 c[8] = {
        vec(center.x - half, center.y - half, center.z - half),
        vec(center.x + half, center.y - half, center.z - half),
        vec(center.x + half, center.y - half, center.z + half),
        vec(center.x - half, center.y - half, center.z + half),
        vec(center.x - half, center.y + half, center.z - half),
        vec(center.x + half, center.y + half, center.z - half),
        vec(center.x + half, center.y + half, center.z + half),
        vec(center.x - half, center.y + half, center.z + half)};
    static const int kEdges[12][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6},
                                      {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
    for (int i = 0; i < 12; ++i) addLine(out, c[kEdges[i][0]], c[kEdges[i][1]], color);
}

} // namespace

// ═══════════════════════════════════════════════════════════
//  sEDIT_LINE
// ═══════════════════════════════════════════════════════════
sEDIT_LINE::sEDIT_LINE() : aStart{0.0f, 0.0f, 0.0f}, aEnd{0.0f, 0.0f, 0.0f}, aColor(0xFFFFFFFFu) {}

sEDIT_LINE::sEDIT_LINE(const GNg::sVEC3 &start, const GNg::sVEC3 &end, uint32_t color)
    : aStart(start), aEnd(end), aColor(color) {}

// ═══════════════════════════════════════════════════════════
//  网格
// ═══════════════════════════════════════════════════════════
sGRID_DESC::sGRID_DESC()
    : aCenter{0.0f, 0.0f, 0.0f}, aCellSize(1.0f), aHalfCells(10u), aMajorEvery(5u),
      aDrawAxes(true), aFade(true) {}

uint32_t gridLineCount(const sGRID_DESC &desc) {
    if (!(desc.aCellSize > 0.0f)) return 0;
    const uint32_t perDirection = desc.aHalfCells * 2u + 1u;
    return perDirection * 2u; // 沿线方向 / 沿 Z 方向
}

void buildGridLines(const sGRID_DESC &desc, std::vector<sEDIT_LINE> &out) {
    if (!(desc.aCellSize > 0.0f) || desc.aHalfCells == 0u) return;
    const int32_t half = (int32_t)desc.aHalfCells;
    const float cell = desc.aCellSize;
    const float extent = (float)half * cell;

    for (int32_t i = -half; i <= half; ++i) {
        const float offset = (float)i * cell;
        const int32_t absIndex = i < 0 ? -i : i;

        // 颜色：中心线（i == 0）走轴语义色；否则按主线/次线/外圈分级
        uint32_t colorAlongX = 0;
        if (desc.aDrawAxes && i == 0) {
            colorAlongX = gizmoColor::kAxisZ; // 沿 X 延伸的线在 z=0 上 ⇒ 它就是 Z 轴所在位置
        } else {
            const bool major = (desc.aMajorEvery > 0u) && ((uint32_t)absIndex % desc.aMajorEvery == 0u);
            if (desc.aFade && absIndex >= half - 1) colorAlongX = gizmoColor::kGridFade;
            else colorAlongX = major ? gizmoColor::kGridMajor : gizmoColor::kGridMinor;
        }
        uint32_t colorAlongZ = 0;
        if (desc.aDrawAxes && i == 0) {
            colorAlongZ = gizmoColor::kAxisX;
        } else {
            const bool major = (desc.aMajorEvery > 0u) && ((uint32_t)absIndex % desc.aMajorEvery == 0u);
            if (desc.aFade && absIndex >= half - 1) colorAlongZ = gizmoColor::kGridFade;
            else colorAlongZ = major ? gizmoColor::kGridMajor : gizmoColor::kGridMinor;
        }

        // 沿 X 的线（固定 z = offset）
        addLine(out, vec(desc.aCenter.x - extent, desc.aCenter.y, desc.aCenter.z + offset),
                vec(desc.aCenter.x + extent, desc.aCenter.y, desc.aCenter.z + offset), colorAlongX);
        // 沿 Z 的线（固定 x = offset）
        addLine(out, vec(desc.aCenter.x + offset, desc.aCenter.y, desc.aCenter.z - extent),
                vec(desc.aCenter.x + offset, desc.aCenter.y, desc.aCenter.z + extent), colorAlongZ);
    }
}

// ═══════════════════════════════════════════════════════════
//  坐标轴
// ═══════════════════════════════════════════════════════════
void buildAxesLines(const GNg::sVEC3 &origin, float length, std::vector<sEDIT_LINE> &out) {
    if (!(length > 0.0f)) return;
    const GNg::sVEC3 axes[3] = {vec(1.0f, 0.0f, 0.0f), vec(0.0f, 1.0f, 0.0f),
                                vec(0.0f, 0.0f, 1.0f)};
    const float headLength = length * 0.14f;
    const float headRadius = length * 0.045f;
    for (int axis = 0; axis < 3; ++axis) {
        const uint32_t color = axisColor(axis);
        const GNg::sVEC3 tip = add(origin, mul(axes[axis], length));
        addLine(out, origin, tip, color);
        // 四棱锥箭头：4 条从尖端回到基座圆上的线
        const GNg::sVEC3 u = anyPerpendicular(axes[axis]);
        const GNg::sVEC3 v = cross(axes[axis], u);
        const GNg::sVEC3 baseCenter = add(origin, mul(axes[axis], length - headLength));
        for (int corner = 0; corner < 4; ++corner) {
            const float angle = (float)corner * kPi * 0.5f;
            const GNg::sVEC3 ringPoint =
                add(baseCenter, add(mul(u, std::cos(angle) * headRadius),
                                    mul(v, std::sin(angle) * headRadius)));
            addLine(out, tip, ringPoint, color);
        }
    }
}

// ═══════════════════════════════════════════════════════════
//  包围盒 / 线框
// ═══════════════════════════════════════════════════════════
void buildBoxLines(const sAABB &box, uint32_t color, std::vector<sEDIT_LINE> &out,
                   float padding) {
    if (!box.IsValid()) return;
    sAABB expanded = box;
    if (padding > 0.0f) expanded.Expand(padding);
    const GNg::sVEC3 &lo = expanded.aMin;
    const GNg::sVEC3 &hi = expanded.aMax;
    const GNg::sVEC3 c[8] = {vec(lo.x, lo.y, lo.z), vec(hi.x, lo.y, lo.z), vec(hi.x, lo.y, hi.z),
                             vec(lo.x, lo.y, hi.z), vec(lo.x, hi.y, lo.z), vec(hi.x, hi.y, lo.z),
                             vec(hi.x, hi.y, hi.z), vec(lo.x, hi.y, hi.z)};
    static const int kEdges[12][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6},
                                      {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
    for (int i = 0; i < 12; ++i) addLine(out, c[kEdges[i][0]], c[kEdges[i][1]], color);
}

void buildBoxCornerMarks(const sAABB &box, float tickFraction, uint32_t color,
                         std::vector<sEDIT_LINE> &out, float padding) {
    if (!box.IsValid()) return;
    if (!(tickFraction > 0.0f)) tickFraction = 0.15f;
    sAABB expanded = box;
    if (padding > 0.0f) expanded.Expand(padding);
    const GNg::sVEC3 extent = expanded.Extent();
    const GNg::sVEC3 lo = expanded.aMin;
    const GNg::sVEC3 hi = expanded.aMax;
    const float tx = extent.x * tickFraction;
    const float ty = extent.y * tickFraction;
    const float tz = extent.z * tickFraction;
    // 8 个角，每个角沿三条棱向内画一小段（一共 24 段）
    for (int corner = 0; corner < 8; ++corner) {
        const bool highX = (corner & 1) != 0;
        const bool highY = (corner & 2) != 0;
        const bool highZ = (corner & 4) != 0;
        const GNg::sVEC3 p = vec(highX ? hi.x : lo.x, highY ? hi.y : lo.y, highZ ? hi.z : lo.z);
        const float dx = highX ? -tx : tx;
        const float dy = highY ? -ty : ty;
        const float dz = highZ ? -tz : tz;
        addLine(out, p, vec(p.x + dx, p.y, p.z), color);
        addLine(out, p, vec(p.x, p.y + dy, p.z), color);
        addLine(out, p, vec(p.x, p.y, p.z + dz), color);
    }
}

float suggestedBoxPadding(const sAABB &box, float cameraDistance, float screenHeight,
                          float fovYRad, float minPixels) {
    if (minPixels <= 0.0f) minPixels = 2.0f;
    float byPixels = 0.0f;
    if (cameraDistance > 0.0f && screenHeight > 0.0f && fovYRad > 0.0f) {
        const float worldPerPixel = 2.0f * cameraDistance * std::tan(fovYRad * 0.5f) / screenHeight;
        byPixels = minPixels * worldPerPixel;
    }
    // 兜底：即使相机参数不合理，也保证有一个与物体尺度相关的最小外扩
    float bySize = 0.02f;
    if (box.IsValid()) {
        const GNg::sVEC3 extent = box.Extent();
        bySize = 0.02f * std::sqrt(extent.x * extent.x + extent.y * extent.y + extent.z * extent.z);
    }
    return byPixels > bySize ? byPixels : bySize;
}

void buildIndexedWireframeLines(const float *vertices, uint32_t vertexCount,
                                const uint32_t *indices, uint32_t indexCount,
                                const GNg::sMAT44 *transform, uint32_t color,
                                std::vector<sEDIT_LINE> &out) {
    if (vertices == 0 || indices == 0) return;
    const uint32_t triangleCount = indexCount / 3u;
    if (triangleCount == 0u) return;

    // 去重键：把 (min,max) 顶点序号打包成 64 位。共享边只画一次，
    // 否则同一位置画两遍、alpha 叠加后那些边会显得更亮（密集网格尤其明显）。
    std::vector<uint64_t> edges;
    edges.reserve((size_t)triangleCount * 3u);
    for (uint32_t triangle = 0; triangle < triangleCount; ++triangle) {
        const uint32_t i0 = indices[triangle * 3u];
        const uint32_t i1 = indices[triangle * 3u + 1u];
        const uint32_t i2 = indices[triangle * 3u + 2u];
        if (i0 >= vertexCount || i1 >= vertexCount || i2 >= vertexCount) continue;
        const uint32_t triple[3][2] = {{i0, i1}, {i1, i2}, {i2, i0}};
        for (int e = 0; e < 3; ++e) {
            uint32_t a = triple[e][0];
            uint32_t b = triple[e][1];
            if (a == b) continue; // 退化边
            if (a > b) std::swap(a, b);
            edges.push_back(((uint64_t)a << 32) | (uint64_t)b);
        }
    }
    std::sort(edges.begin(), edges.end());
    edges.erase(std::unique(edges.begin(), edges.end()), edges.end());

    for (size_t i = 0; i < edges.size(); ++i) {
        const uint32_t a = (uint32_t)(edges[i] >> 32);
        const uint32_t b = (uint32_t)(edges[i] & 0xFFFFFFFFu);
        const float *pa = vertices + (size_t)a * 11u;
        const float *pb = vertices + (size_t)b * 11u;
        GNg::sVEC3 va = vec(pa[0], pa[1], pa[2]);
        GNg::sVEC3 vb = vec(pb[0], pb[1], pb[2]);
        if (transform != 0) {
            GNg::sVEC3 ta;
            GNg::sVEC3 tb;
            GNg::Mat44TransformCoord(&ta, &va, transform);
            GNg::Mat44TransformCoord(&tb, &vb, transform);
            va = ta;
            vb = tb;
        }
        addLine(out, va, vb, color);
    }
}

// ═══════════════════════════════════════════════════════════
//  样条
// ═══════════════════════════════════════════════════════════
sSPLINE_DESC::sSPLINE_DESC()
    : aMode(eSPLINE_CATMULL_ROM), aClosed(false), aSamplesPerSegment(16u), aTension(0.5f) {}

namespace {

// Catmull-Rom（非均匀张力的简化版）：p1 到 p2 之间，切线由 p0/p3 决定。
GNg::sVEC3 catmullRom(const GNg::sVEC3 &p0, const GNg::sVEC3 &p1, const GNg::sVEC3 &p2,
                      const GNg::sVEC3 &p3, float t, float tension) {
    const float t2 = t * t;
    const float t3 = t2 * t;
    // 张力 0.5 = 标准 Catmull-Rom；调小更"尖锐"（趋向折线），调大更"松"
    const float s = (1.0f - tension) * 2.0f * 0.5f;
    const GNg::sVEC3 m1 = mul(sub(p2, p0), s);
    const GNg::sVEC3 m2 = mul(sub(p3, p1), s);
    const float h00 = 2.0f * t3 - 3.0f * t2 + 1.0f;
    const float h10 = t3 - 2.0f * t2 + t;
    const float h01 = -2.0f * t3 + 3.0f * t2;
    const float h11 = t3 - t2;
    return add(add(mul(p1, h00), mul(m1, h10)), add(mul(p2, h01), mul(m2, h11)));
}

GNg::sVEC3 bezier3(const GNg::sVEC3 &p0, const GNg::sVEC3 &p1, const GNg::sVEC3 &p2,
                   const GNg::sVEC3 &p3, float t) {
    const float u = 1.0f - t;
    const float b0 = u * u * u;
    const float b1 = 3.0f * u * u * t;
    const float b2 = 3.0f * u * t * t;
    const float b3 = t * t * t;
    return add(add(mul(p0, b0), mul(p1, b1)), add(mul(p2, b2), mul(p3, b3)));
}

uint32_t effectiveCount(uint32_t count, bool closed) {
    if (count < 2u) return 0u;
    if (closed && count < 3u) return count; // 两个点闭环 = 一条来回的直线，没意义，按开放处理
    return count;
}

// 取第 index 个控制点，越界时按 open/closed 规则环绕或钳制。
const GNg::sVEC3 &controlPoint(const GNg::sVEC3 *points, uint32_t count, int32_t index,
                               bool closed) {
    const int32_t n = (int32_t)count;
    if (closed) {
        int32_t wrapped = index % n;
        if (wrapped < 0) wrapped += n;
        return points[wrapped];
    }
    if (index < 0) return points[0];
    if (index >= n) return points[n - 1];
    return points[index];
}

} // namespace

GNg::sVEC3 evaluateSpline(const GNg::sVEC3 *points, uint32_t count, const sSPLINE_DESC &desc,
                          float t) {
    if (points == 0) return vec(0.0f, 0.0f, 0.0f);
    const bool closed = desc.aClosed && count >= 3u;
    const uint32_t used = effectiveCount(count, desc.aClosed);
    if (used == 0u) return points[0];
    if (used == 1u) return points[0];
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;

    const uint32_t segments = closed ? used : (used - 1u);
    if (segments == 0u) return points[0];

    // t 映射到"第几段 + 段内进度"
    float scaled = t * (float)segments;
    uint32_t segment = (uint32_t)scaled;
    if (segment >= segments) segment = segments - 1u; // t == 1 落在最后一段终点
    const float local = scaled - (float)segment;

    if (desc.aMode == eSPLINE_LINEAR) {
        const GNg::sVEC3 &a = points[segment];
        const GNg::sVEC3 &b = (segment + 1u < used) ? points[segment + 1u] : points[0];
        return add(a, mul(sub(b, a), local));
    }
    const int32_t i1 = (int32_t)segment;
    const int32_t i2 = i1 + 1;
    if (desc.aMode == eSPLINE_BEZIER) {
        // 每段用 (p[i1], p[i1]+c, p[i2]-c, p[i2]) 三次贝塞尔，c 沿控制多边形方向。
        // 这是"把控制点当路径点"的常见做法：曲线仍然过点，形状可控。
        const GNg::sVEC3 &p0 = points[segment];
        const GNg::sVEC3 &p3 = (segment + 1u < used) ? points[segment + 1u] : points[0];
        const GNg::sVEC3 &prev = controlPoint(points, used, i1 - 1, closed);
        const GNg::sVEC3 &next = controlPoint(points, used, i2 + 1, closed);
        const float k = 0.28f;
        const GNg::sVEC3 c1 = add(p0, mul(sub(p3, prev), k));
        const GNg::sVEC3 c2 = sub(p3, mul(sub(next, p0), k));
        return bezier3(p0, c1, c2, p3, local);
    }
    const GNg::sVEC3 &p0 = controlPoint(points, used, i1 - 1, closed);
    const GNg::sVEC3 &p1 = controlPoint(points, used, i1, closed);
    const GNg::sVEC3 &p2 = controlPoint(points, used, i2, closed);
    const GNg::sVEC3 &p3 = controlPoint(points, used, i2 + 1, closed);
    return catmullRom(p0, p1, p2, p3, local, desc.aTension);
}

void sampleSpline(const GNg::sVEC3 *points, uint32_t count, const sSPLINE_DESC &desc,
                  std::vector<GNg::sVEC3> &outPoints) {
    outPoints.clear();
    const bool closed = desc.aClosed && count >= 3u;
    const uint32_t used = effectiveCount(count, desc.aClosed);
    if (used < 2u) {
        if (used == 1u) outPoints.push_back(points[0]);
        return;
    }
    uint32_t samples = desc.aSamplesPerSegment;
    if (samples < 2u) samples = 2u;
    const uint32_t segments = closed ? used : (used - 1u);
    const uint32_t total = segments * samples;
    outPoints.reserve((size_t)total + 1u);
    for (uint32_t i = 0; i <= total; ++i) {
        const float t = (float)i / (float)total;
        outPoints.push_back(evaluateSpline(points, count, desc, t));
    }
}

void buildSplineLines(const GNg::sVEC3 *points, uint32_t count, const sSPLINE_DESC &desc,
                      uint32_t color, std::vector<sEDIT_LINE> &out) {
    std::vector<GNg::sVEC3> samples;
    sampleSpline(points, count, desc, samples);
    for (size_t i = 1; i < samples.size(); ++i) addLine(out, samples[i - 1u], samples[i], color);
}

void buildSplineHandles(const GNg::sVEC3 *points, uint32_t count, float markerSize,
                        bool drawPolygon, std::vector<sEDIT_LINE> &out) {
    if (points == 0 || count == 0u) return;
    if (!(markerSize > 0.0f)) markerSize = 0.05f;
    const uint32_t color = gizmoColor::kSpline;
    const uint32_t polygonColor = gizmoColor::kSplineHandle;
    for (uint32_t i = 0; i < count; ++i) {
        addCubeWire(out, points[i], markerSize, color);
        if (!drawPolygon) continue;
        const uint32_t next = (i + 1u) % count;
        if (i + 1u == count && count < 3u) break; // 两个点不画闭环
        // 虚线：把这段分成 6 份，只画偶数份 ⇒ 控制多边形不会和曲线抢注意力
        const GNg::sVEC3 &a = points[i];
        const GNg::sVEC3 &b = points[next];
        const int kDashes = 6;
        for (int d = 0; d < kDashes; d += 2) {
            const float t0 = (float)d / (float)kDashes;
            const float t1 = (float)(d + 1) / (float)kDashes;
            addLine(out, add(a, mul(sub(b, a), t0)), add(a, mul(sub(b, a), t1)), polygonColor);
        }
    }
}

// ═══════════════════════════════════════════════════════════
//  Gizmo
// ═══════════════════════════════════════════════════════════
sGIZMO_DESC::sGIZMO_DESC()
    : aMode(eGIZMO_TRANSLATE), aOrigin{0.0f, 0.0f, 0.0f}, aLocalSpace(false), aBasis(),
      aSize(1.0f), aHighlight(eGIZMO_AXIS_NONE) {
    GNg::Mat44Identity(&aBasis);
}

float gizmoScreenSize(float cameraDistance, float fovYRad, float screenFraction) {
    if (!(cameraDistance > 0.0f) || !(fovYRad > 0.0f)) return 1.0f;
    if (!(screenFraction > 0.0f)) screenFraction = 0.15f;
    const float viewHeight = 2.0f * cameraDistance * std::tan(fovYRad * 0.5f);
    return viewHeight * screenFraction;
}

void gizmoBasisFromRotation(const GNg::sQUAT &rotation, GNg::sMAT44 *outBasis) {
    if (outBasis == 0) return;
    GNg::QuatToMatrix44(outBasis, &rotation);
    // 去掉平移列（QuatToMatrix44 本来就只写旋转，这里显式写一遍以防将来改实现）
    outBasis->m[12] = 0.0f;
    outBasis->m[13] = 0.0f;
    outBasis->m[14] = 0.0f;
    outBasis->m[15] = 1.0f;
}

GNg::sVEC3 gizmoAxisVector(const sGIZMO_DESC &desc, eGIZMO_AXIS axis) {
    if (axis < 0 || axis > 2) return vec(0.0f, 0.0f, 0.0f);
    const int index = (int)axis;
    if (!desc.aLocalSpace) {
        return vec(index == 0 ? 1.0f : 0.0f, index == 1 ? 1.0f : 0.0f, index == 2 ? 1.0f : 0.0f);
    }
    // 局部空间：取基矩阵的第 index 列（前三个分量），归一化
    GNg::sVEC3 v = vec(desc.aBasis.m[index * 4 + 0], desc.aBasis.m[index * 4 + 1],
                       desc.aBasis.m[index * 4 + 2]);
    if (!normalize(v)) {
        return vec(index == 0 ? 1.0f : 0.0f, index == 1 ? 1.0f : 0.0f, index == 2 ? 1.0f : 0.0f);
    }
    return v;
}

void buildGizmoLines(const sGIZMO_DESC &desc, std::vector<sEDIT_LINE> &out) {
    const float size = desc.aSize > 0.0f ? desc.aSize : 1.0f;
    const bool rotateMode = (desc.aMode == eGIZMO_ROTATE);

    for (int axis = 0; axis < 3; ++axis) {
        const eGIZMO_AXIS axisEnum = (eGIZMO_AXIS)axis;
        const GNg::sVEC3 dir = gizmoAxisVector(desc, axisEnum);
        const uint32_t color = (desc.aHighlight == axisEnum) ? gizmoColor::kHighlight
                                                            : axisColor(axis);
        if (rotateMode) {
            // 环所在平面的法线 = 该轴；环半径略小于 size，避免三环在轴上互相穿插
            const float radius = size * 0.86f;
            const GNg::sVEC3 u = anyPerpendicular(dir);
            const GNg::sVEC3 v = cross(dir, u);
            const int kSegments = 48;
            GNg::sVEC3 previous = add(desc.aOrigin, mul(u, radius));
            for (int s = 1; s <= kSegments; ++s) {
                const float angle = (float)s * 6.2831853f / (float)kSegments;
                const GNg::sVEC3 point =
                    add(desc.aOrigin, add(mul(u, std::cos(angle) * radius),
                                          mul(v, std::sin(angle) * radius)));
                addLine(out, previous, point, color);
                previous = point;
            }
            continue;
        }

        // 平移 / 缩放：轴线 + 末端形状
        const GNg::sVEC3 tip = add(desc.aOrigin, mul(dir, size));
        addLine(out, desc.aOrigin, tip, color);
        if (desc.aMode == eGIZMO_TRANSLATE) {
            // 箭头：4 条线从尖端回到基座
            const float headLength = size * 0.20f;
            const float headRadius = size * 0.055f;
            const GNg::sVEC3 u = anyPerpendicular(dir);
            const GNg::sVEC3 v = cross(dir, u);
            const GNg::sVEC3 baseCenter = add(desc.aOrigin, mul(dir, size - headLength));
            for (int corner = 0; corner < 4; ++corner) {
                const float angle = (float)corner * kPi * 0.5f;
                const GNg::sVEC3 ringPoint =
                    add(baseCenter, add(mul(u, std::cos(angle) * headRadius),
                                        mul(v, std::sin(angle) * headRadius)));
                addLine(out, tip, ringPoint, color);
            }
        } else {
            addCubeWire(out, tip, size * 0.06f, color); // 缩放：末端小方块
        }
    }

    // 中心自由拖拽标记（平移/缩放模式）。旋转模式不画，否则会盖住三环交汇点。
    if (!rotateMode) {
        const uint32_t centerColor = (desc.aHighlight == eGIZMO_AXIS_XYZ)
                                         ? gizmoColor::kHighlight
                                         : gizmoColor::kGridMajor;
        addCubeWire(out, desc.aOrigin, size * 0.045f, centerColor);
    }
}

eGIZMO_AXIS pickGizmo(const sGIZMO_DESC &desc, const GNg::sVEC3 &rayOrigin,
                      const GNg::sVEC3 &rayDir, float pickRadius) {
    const float size = desc.aSize > 0.0f ? desc.aSize : 1.0f;
    if (!(pickRadius > 0.0f)) pickRadius = size * 0.10f;
    GNg::sVEC3 dir = rayDir;
    if (!normalize(dir)) return eGIZMO_AXIS_NONE;

    if (desc.aMode == eGIZMO_ROTATE) {
        // 环拾取：射线与环平面求交，交点到轴心距离落在环半径附近即命中。
        const float radius = size * 0.86f;
        eGIZMO_AXIS best = eGIZMO_AXIS_NONE;
        float bestDistance = 1.0e30f;
        for (int axis = 0; axis < 3; ++axis) {
            const GNg::sVEC3 normal = gizmoAxisVector(desc, (eGIZMO_AXIS)axis);
            const float denom = dot(dir, normal);
            if (std::fabs(denom) < kEps) continue; // 射线与平面平行
            const float t = dot(sub(desc.aOrigin, rayOrigin), normal) / denom;
            if (t <= 0.0f) continue; // 交点在相机背后
            const GNg::sVEC3 point = add(rayOrigin, mul(dir, t));
            const float radial = std::sqrt(lengthSq(sub(point, desc.aOrigin)));
            const float error = std::fabs(radial - radius);
            if (error <= pickRadius && error < bestDistance) {
                bestDistance = error;
                best = (eGIZMO_AXIS)axis;
            }
        }
        return best;
    }

    // 平移/缩放：轴线段拾取，取最近的
    eGIZMO_AXIS best = eGIZMO_AXIS_NONE;
    float bestDistance = 1.0e30f;
    for (int axis = 0; axis < 3; ++axis) {
        const GNg::sVEC3 dirAxis = gizmoAxisVector(desc, (eGIZMO_AXIS)axis);
        const GNg::sVEC3 tip = add(desc.aOrigin, mul(dirAxis, size));
        const float distance = distanceRayToSegment(rayOrigin, dir, desc.aOrigin, tip);
        if (distance <= pickRadius && distance < bestDistance) {
            bestDistance = distance;
            best = (eGIZMO_AXIS)axis;
        }
    }
    // 中心方块：射线到轴心点的距离（方块半径的 1.6 倍作为容差）
    {
        float t = dot(sub(desc.aOrigin, rayOrigin), dir);
        if (t < 0.0f) t = 0.0f;
        const GNg::sVEC3 onRay = add(rayOrigin, mul(dir, t));
        const float distance = std::sqrt(lengthSq(sub(onRay, desc.aOrigin)));
        const float centerRadius = size * 0.045f * 2.4f;
        if (distance <= centerRadius && distance < bestDistance) {
            return eGIZMO_AXIS_XYZ;
        }
    }
    return best;
}

float gizmoDragTranslate(const sGIZMO_DESC &desc, eGIZMO_AXIS dragAxis,
                         const GNg::sVEC3 &rayOrigin, const GNg::sVEC3 &rayDir) {
    if (dragAxis < 0 || dragAxis > 2) return 0.0f;
    GNg::sVEC3 dir = rayDir;
    if (!normalize(dir)) return 0.0f;
    const GNg::sVEC3 axisDir = gizmoAxisVector(desc, dragAxis);
    float s = 0.0f;
    if (!projectRayOnLine(desc.aOrigin, axisDir, rayOrigin, dir, s)) return 0.0f;
    return s;
}

float gizmoDragRotate(const sGIZMO_DESC &desc, eGIZMO_AXIS dragAxis,
                      const GNg::sVEC3 &startPlanePoint, const GNg::sVEC3 &rayOrigin,
                      const GNg::sVEC3 &rayDir) {
    if (dragAxis < 0 || dragAxis > 2) return 0.0f;
    GNg::sVEC3 dir = rayDir;
    if (!normalize(dir)) return 0.0f;
    const GNg::sVEC3 normal = gizmoAxisVector(desc, dragAxis);
    const float denom = dot(dir, normal);
    if (std::fabs(denom) < kEps) return 0.0f;
    const float t = dot(sub(desc.aOrigin, rayOrigin), normal) / denom;
    if (t <= 0.0f) return 0.0f;
    const GNg::sVEC3 current = sub(add(rayOrigin, mul(dir, t)), desc.aOrigin);
    const GNg::sVEC3 start = sub(startPlanePoint, desc.aOrigin);

    // 平面上的一组基（与拾取用同一套构造，角度才自洽）
    const GNg::sVEC3 u = anyPerpendicular(normal);
    const GNg::sVEC3 v = cross(normal, u);
    const float startAngle = std::atan2(dot(start, v), dot(start, u));
    const float currentAngle = std::atan2(dot(current, v), dot(current, u));
    float delta = currentAngle - startAngle;
    // 归一到 (-pi, pi]：手转一圈应该回到 0 而不是累加到 2pi
    while (delta > kPi) delta -= 2.0f * kPi;
    while (delta <= -kPi) delta += 2.0f * kPi;
    return delta;
}

float gizmoDragScale(const sGIZMO_DESC &desc, eGIZMO_AXIS dragAxis,
                     const GNg::sVEC3 &rayOrigin, const GNg::sVEC3 &rayDir,
                     float startProjection) {
    if (!(std::fabs(startProjection) > 1.0e-4f)) return 1.0f;
    const float current = gizmoDragTranslate(desc, dragAxis, rayOrigin, rayDir);
    float ratio = current / startProjection;
    if (!(ratio > 0.005f)) ratio = 0.005f; // 不允许翻负/归零
    return ratio;
}

} // namespace GN3d
