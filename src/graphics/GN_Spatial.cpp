// GN_Spatial.cpp — AABB、射线-包围盒求交与六面视锥的纯 CPU 实现（不依赖 Vulkan）。
//
// 语义遵循 GN_Spatial.h 的冻结契约：
//   * 视锥平面按列主序行提取（clip = M * v），片内判定 n·p + d >= 0，深度按 Vulkan NDC [0,1]；
//   * 射线 t 为进入距离，起点在盒内为 0，t > maxT 视为未命中；
//   * 空盒与零体积盒不崩溃，查询自然返回未命中。
#include "GN.h"
#include "GN_Spatial.h"

#include <cfloat>
#include <cmath>

namespace GN3d {

namespace {

// 射线与盒的进入距离：invDir 分量为 ±inf 表示该轴与盒面平行，只在起点落到面外时判未命中。
bool SlabEntryByInvDir(const GNg::sVEC3 &origin, const GNg::sVEC3 &invDir, const sAABB &box,
                       float maxT, float &outT) {
    if(!box.IsValid() || maxT < 0.0f) return false;

    const float originAxis[3] = {origin.x, origin.y, origin.z};
    const float invAxis[3] = {invDir.x, invDir.y, invDir.z};
    const float minAxis[3] = {box.aMin.x, box.aMin.y, box.aMin.z};
    const float maxAxis[3] = {box.aMax.x, box.aMax.y, box.aMax.z};

    float tEnter = -FLT_MAX;
    float tExit = FLT_MAX;
    for(int axis = 0; axis < 3; ++axis) {
        if(std::isinf(invAxis[axis])) {
            if(originAxis[axis] < minAxis[axis] || originAxis[axis] > maxAxis[axis]) return false;
            continue;
        }
        float t1 = (minAxis[axis] - originAxis[axis]) * invAxis[axis];
        float t2 = (maxAxis[axis] - originAxis[axis]) * invAxis[axis];
        if(t1 > t2) {
            const float swapValue = t1;
            t1 = t2;
            t2 = swapValue;
        }
        if(t1 > tEnter) tEnter = t1;
        if(t2 < tExit) tExit = t2;
        if(tEnter > tExit) return false;
    }
    if(tExit < 0.0f) return false; // 盒整体在射线背后
    if(tEnter < 0.0f) tEnter = 0.0f; // 起点在盒内（含贴面）
    if(tEnter > maxT) return false;
    outT = tEnter;
    return true;
}

// 由视锥矩阵的一行取出平面系数（列主序：Elem(col, row)）。
GNg::sVEC4 RowPlane(const GNg::sMAT44 &m, int row) {
    GNg::sVEC4 plane;
    plane.x = m.Elem(0, row);
    plane.y = m.Elem(1, row);
    plane.z = m.Elem(2, row);
    plane.w = m.Elem(3, row);
    return plane;
}

// 两个平面系数按 sign 做线性组合：w 行 + sign * row 行。
GNg::sVEC4 CombinePlane(const GNg::sVEC4 &wRow, const GNg::sVEC4 &row, float sign) {
    GNg::sVEC4 plane;
    plane.x = wRow.x + sign * row.x;
    plane.y = wRow.y + sign * row.y;
    plane.z = wRow.z + sign * row.z;
    plane.w = wRow.w + sign * row.w;
    return plane;
}

} // namespace

// ═══════════════════════════════════════════════════════════
//  sAABB
// ═══════════════════════════════════════════════════════════

sAABB::sAABB() {
    Reset();
}

sAABB::sAABB(const GNg::sVEC3 &minValue, const GNg::sVEC3 &maxValue) {
    aMin = minValue;
    aMax = maxValue;
}

void sAABB::Reset() {
    aMin.x = aMin.y = aMin.z = FLT_MAX;
    aMax.x = aMax.y = aMax.z = -FLT_MAX;
}

bool sAABB::IsValid() const {
    return aMin.x <= aMax.x && aMin.y <= aMax.y && aMin.z <= aMax.z;
}

void sAABB::Expand(const GNg::sVEC3 &point) {
    if(point.x < aMin.x) aMin.x = point.x;
    if(point.y < aMin.y) aMin.y = point.y;
    if(point.z < aMin.z) aMin.z = point.z;
    if(point.x > aMax.x) aMax.x = point.x;
    if(point.y > aMax.y) aMax.y = point.y;
    if(point.z > aMax.z) aMax.z = point.z;
}

void sAABB::Expand(const sAABB &box) {
    // 空盒的 min/max 是 ±FLT_MAX，逐点扩张后自然成为“无操作”，故并集语义对空盒也成立。
    Expand(box.aMin);
    Expand(box.aMax);
}

void sAABB::Expand(float pad) {
    aMin.x -= pad;
    aMin.y -= pad;
    aMin.z -= pad;
    aMax.x += pad;
    aMax.y += pad;
    aMax.z += pad;
}

GNg::sVEC3 sAABB::Center() const {
    GNg::sVEC3 center;
    center.x = (aMin.x + aMax.x) * 0.5f;
    center.y = (aMin.y + aMax.y) * 0.5f;
    center.z = (aMin.z + aMax.z) * 0.5f;
    return center;
}

GNg::sVEC3 sAABB::Extent() const {
    GNg::sVEC3 extent;
    extent.x = (aMax.x - aMin.x) * 0.5f;
    extent.y = (aMax.y - aMin.y) * 0.5f;
    extent.z = (aMax.z - aMin.z) * 0.5f;
    return extent;
}

float sAABB::SurfaceArea() const {
    if(!IsValid()) return 0.0f;
    const float dx = aMax.x - aMin.x;
    const float dy = aMax.y - aMin.y;
    const float dz = aMax.z - aMin.z;
    return 2.0f * (dx * dy + dy * dz + dz * dx);
}

float sAABB::Volume() const {
    if(!IsValid()) return 0.0f;
    return (aMax.x - aMin.x) * (aMax.y - aMin.y) * (aMax.z - aMin.z);
}

int sAABB::LongestAxis() const {
    if(!IsValid()) return 0;
    const float dx = aMax.x - aMin.x;
    const float dy = aMax.y - aMin.y;
    const float dz = aMax.z - aMin.z;
    if(dx >= dy && dx >= dz) return 0;
    if(dy >= dz) return 1;
    return 2;
}

bool sAABB::Overlaps(const sAABB &box) const {
    if(!IsValid() || !box.IsValid()) return false;
    return aMin.x <= box.aMax.x && aMax.x >= box.aMin.x &&
           aMin.y <= box.aMax.y && aMax.y >= box.aMin.y &&
           aMin.z <= box.aMax.z && aMax.z >= box.aMin.z;
}

bool sAABB::Contains(const GNg::sVEC3 &point) const {
    if(!IsValid()) return false;
    return point.x >= aMin.x && point.x <= aMax.x &&
           point.y >= aMin.y && point.y <= aMax.y &&
           point.z >= aMin.z && point.z <= aMax.z;
}

bool sAABB::Contains(const sAABB &box) const {
    if(!IsValid() || !box.IsValid()) return false;
    return box.aMin.x >= aMin.x && box.aMax.x <= aMax.x &&
           box.aMin.y >= aMin.y && box.aMax.y <= aMax.y &&
           box.aMin.z >= aMin.z && box.aMax.z <= aMax.z;
}

sAABB sAABB::FromCenterExtent(const GNg::sVEC3 &center, const GNg::sVEC3 &extent) {
    sAABB box;
    box.aMin.x = center.x - extent.x;
    box.aMin.y = center.y - extent.y;
    box.aMin.z = center.z - extent.z;
    box.aMax.x = center.x + extent.x;
    box.aMax.y = center.y + extent.y;
    box.aMax.z = center.z + extent.z;
    return box;
}

// ═══════════════════════════════════════════════════════════
//  sRAY_HIT
// ═══════════════════════════════════════════════════════════

sRAY_HIT::sRAY_HIT() {
    Reset();
}

void sRAY_HIT::Reset() {
    aHit = false;
    aT = 0.0f;
    aPoint.x = aPoint.y = aPoint.z = 0.0f;
    aIndex = 0xFFFFFFFFu;
}

// ═══════════════════════════════════════════════════════════
//  sFRUSTUM
// ═══════════════════════════════════════════════════════════

sFRUSTUM::sFRUSTUM() {
    for(int i = 0; i < 6; ++i) {
        aPlanes[i].Init(0.0f, 0.0f, 0.0f, 0.0f);
    }
}

sFRUSTUM sFRUSTUM::FromViewProj(const GNg::sMAT44 &viewProj) {
    // Gribb-Hartmann：片内即 -w <= x,y <= w 且 0 <= z <= w（Vulkan NDC）。
    const GNg::sVEC4 wRow = RowPlane(viewProj, 3);
    const GNg::sVEC4 xRow = RowPlane(viewProj, 0);
    const GNg::sVEC4 yRow = RowPlane(viewProj, 1);
    const GNg::sVEC4 zRow = RowPlane(viewProj, 2);

    sFRUSTUM frustum;
    frustum.aPlanes[0] = CombinePlane(wRow, xRow, 1.0f); // 左：w + x >= 0
    frustum.aPlanes[1] = CombinePlane(wRow, xRow, -1.0f); // 右：w - x >= 0
    frustum.aPlanes[2] = CombinePlane(wRow, yRow, 1.0f); // 下：w + y >= 0
    frustum.aPlanes[3] = CombinePlane(wRow, yRow, -1.0f); // 上：w - y >= 0
    frustum.aPlanes[4] = zRow; // 近：z >= 0
    frustum.aPlanes[5] = CombinePlane(wRow, zRow, -1.0f); // 远：w - z >= 0
    frustum.Normalize();
    return frustum;
}

bool sFRUSTUM::ContainsPoint(const GNg::sVEC3 &point) const {
    for(int i = 0; i < 6; ++i) {
        const GNg::sVEC4 &plane = aPlanes[i];
        if(plane.x * point.x + plane.y * point.y + plane.z * point.z + plane.w < 0.0f) return false;
    }
    return true;
}

bool sFRUSTUM::IntersectsAABB(const sAABB &box) const {
    if(!box.IsValid()) return false;
    for(int i = 0; i < 6; ++i) {
        const GNg::sVEC4 &plane = aPlanes[i];
        // 取沿法线最远的角点（p-vertex）：它在外侧则整个盒都在该面外侧。
        const float px = plane.x >= 0.0f ? box.aMax.x : box.aMin.x;
        const float py = plane.y >= 0.0f ? box.aMax.y : box.aMin.y;
        const float pz = plane.z >= 0.0f ? box.aMax.z : box.aMin.z;
        if(plane.x * px + plane.y * py + plane.z * pz + plane.w < 0.0f) return false;
    }
    return true;
}

void sFRUSTUM::Normalize() {
    for(int i = 0; i < 6; ++i) {
        GNg::sVEC4 &plane = aPlanes[i];
        const float length = sqrtf(plane.x * plane.x + plane.y * plane.y + plane.z * plane.z);
        if(length <= 1e-12f) continue;
        const float invLength = 1.0f / length;
        plane.x *= invLength;
        plane.y *= invLength;
        plane.z *= invLength;
        plane.w *= invLength;
    }
}

// ═══════════════════════════════════════════════════════════
//  射线与包围盒
// ═══════════════════════════════════════════════════════════

bool RayIntersectAABB(const GNg::sVEC3 &origin, const GNg::sVEC3 &dir, const sAABB &box,
                      float maxT, float &outT) {
    GNg::sVEC3 invDir;
    invDir.x = 1.0f / dir.x; // dir 分量为 0 时得到 ±inf，SlabEntryByInvDir 按平行轴处理
    invDir.y = 1.0f / dir.y;
    invDir.z = 1.0f / dir.z;
    return SlabEntryByInvDir(origin, invDir, box, maxT, outT);
}

bool RayIntersectAABBInside(const GNg::sVEC3 &origin, const GNg::sVEC3 &dir, const sAABB &box,
                            float maxT, float &outT, bool *originInside) {
    if(originInside) *originInside = box.Contains(origin);
    return RayIntersectAABB(origin, dir, box, maxT, outT);
}

} // namespace GN3d
