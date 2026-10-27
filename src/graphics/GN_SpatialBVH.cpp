// GN_SpatialBVH.cpp — 静态 BVH 的构建与视锥/包围盒/射线查询实现。
//
// 构建：按最长轴中位数切分，叶子上限 4 个盒；查询结果始终为 Build 时的原始盒序号，
// 且保证升序。射线查询取最近命中，同 t 取索引较小者（契约见 GN_Spatial.h）。
#include "GN.h"
#include "GN_Spatial.h"

#include <algorithm>
#include <cfloat>
#include <cmath>

namespace GN3d {

namespace {

// 叶子最多容纳的盒数：控制树高与叶子内暴力测试的平衡。
const uint32_t kLeafSize = 4;

// 盒中心点（BuildRange 按中心的中位数切分）。
GNg::sVEC3 BoxCenter(const sAABB &box) {
    return box.Center();
}

// 按指定轴比较两个盒的中心，用于中位数切分。
struct CentroidLess {
    const std::vector<sAABB> *boxes;
    int axis;

    bool operator()(uint32_t lhs, uint32_t rhs) const {
        const GNg::sVEC3 leftCenter = BoxCenter((*boxes)[lhs]);
        const GNg::sVEC3 rightCenter = BoxCenter((*boxes)[rhs]);
        const float leftValue = axis == 0 ? leftCenter.x : (axis == 1 ? leftCenter.y : leftCenter.z);
        const float rightValue = axis == 0 ? rightCenter.x : (axis == 1 ? rightCenter.y : rightCenter.z);
        return leftValue < rightValue;
    }
};

// 私有 RaycastNode 只持有 invDir，这里还原方向以便复用 RayIntersectAABB：
// 平行轴（invDir 为 ±inf）还原为 0，表示该轴无位移。
GNg::sVEC3 RecoverDirection(const GNg::sVEC3 &invDir) {
    GNg::sVEC3 dir;
    dir.x = std::isinf(invDir.x) ? 0.0f : 1.0f / invDir.x;
    dir.y = std::isinf(invDir.y) ? 0.0f : 1.0f / invDir.y;
    dir.z = std::isinf(invDir.z) ? 0.0f : 1.0f / invDir.z;
    return dir;
}

} // namespace

cBVH::cBVH() : aRoot(INVALID_NODE) {
}

cBVH::~cBVH() {
}

void cBVH::Build(const sAABB *boxes, uint32_t count) {
    Clear();
    if(boxes == 0 || count == 0) return;

    aBoxes.assign(boxes, boxes + count);
    aOrder.resize(count);
    for(uint32_t i = 0; i < count; ++i) aOrder[i] = i;

    aNodes.reserve(count * 2);
    aRoot = BuildRange(0, count);
}

void cBVH::Clear() {
    aNodes.clear();
    aBoxes.clear();
    aOrder.clear();
    aRoot = INVALID_NODE;
}

uint32_t cBVH::BuildRange(uint32_t first, uint32_t count) {
    const uint32_t nodeIndex = (uint32_t)aNodes.size();
    aNodes.push_back(sNODE());
    aNodes[nodeIndex].aLeft = INVALID_NODE;
    aNodes[nodeIndex].aRight = INVALID_NODE;
    aNodes[nodeIndex].aFirst = first;
    aNodes[nodeIndex].aCount = 0;

    sAABB bounds;
    sAABB centroidBounds;
    for(uint32_t i = first; i < first + count; ++i) {
        const uint32_t boxIndex = aOrder[i];
        bounds.Expand(aBoxes[boxIndex]);
        centroidBounds.Expand(BoxCenter(aBoxes[boxIndex]));
    }
    aNodes[nodeIndex].aBox = bounds;

    if(count <= kLeafSize) {
        aNodes[nodeIndex].aFirst = first;
        aNodes[nodeIndex].aCount = count;
        return nodeIndex;
    }

    const int axis = centroidBounds.LongestAxis();
    CentroidLess less;
    less.boxes = &aBoxes;
    less.axis = axis;
    std::sort(aOrder.begin() + first, aOrder.begin() + first + count, less);

    const uint32_t mid = count / 2;
    const uint32_t left = BuildRange(first, mid);
    const uint32_t right = BuildRange(first + mid, count - mid);
    aNodes[nodeIndex].aLeft = left;
    aNodes[nodeIndex].aRight = right;
    return nodeIndex;
}

void cBVH::QueryFrustum(const sFRUSTUM &frustum, std::vector<uint32_t> &out) const {
    out.clear();
    if(aRoot == INVALID_NODE || aNodes.empty()) return;
    QueryNodeFrustum(aRoot, frustum, out);
    std::sort(out.begin(), out.end());
}

void cBVH::QueryAABB(const sAABB &box, std::vector<uint32_t> &out) const {
    out.clear();
    if(aRoot == INVALID_NODE || aNodes.empty() || !box.IsValid()) return;
    QueryNodeAABB(aRoot, box, out);
    std::sort(out.begin(), out.end());
}

void cBVH::QueryNodeAABB(uint32_t node, const sAABB &box, std::vector<uint32_t> &out) const {
    if(node >= aNodes.size()) return;
    if(!aNodes[node].aBox.Overlaps(box)) return;

    if(aNodes[node].IsLeaf()) {
        const uint32_t first = aNodes[node].aFirst;
        const uint32_t count = aNodes[node].aCount;
        for(uint32_t i = first; i < first + count; ++i) {
            const uint32_t boxIndex = aOrder[i];
            if(aBoxes[boxIndex].Overlaps(box)) out.push_back(boxIndex);
        }
        return;
    }
    QueryNodeAABB(aNodes[node].aLeft, box, out);
    QueryNodeAABB(aNodes[node].aRight, box, out);
}

void cBVH::QueryNodeFrustum(uint32_t node, const sFRUSTUM &frustum,
                            std::vector<uint32_t> &out) const {
    if(node >= aNodes.size()) return;
    if(!frustum.IntersectsAABB(aNodes[node].aBox)) return;

    if(aNodes[node].IsLeaf()) {
        const uint32_t first = aNodes[node].aFirst;
        const uint32_t count = aNodes[node].aCount;
        for(uint32_t i = first; i < first + count; ++i) {
            const uint32_t boxIndex = aOrder[i];
            if(frustum.IntersectsAABB(aBoxes[boxIndex])) out.push_back(boxIndex);
        }
        return;
    }
    QueryNodeFrustum(aNodes[node].aLeft, frustum, out);
    QueryNodeFrustum(aNodes[node].aRight, frustum, out);
}

bool cBVH::Raycast(const GNg::sVEC3 &origin, const GNg::sVEC3 &dir, float maxT,
                   sRAY_HIT &hit) const {
    hit.Reset();
    if(aRoot == INVALID_NODE || aNodes.empty()) return false;

    GNg::sVEC3 invDir;
    invDir.x = 1.0f / dir.x;
    invDir.y = 1.0f / dir.y;
    invDir.z = 1.0f / dir.z;

    sRAY_HIT best;
    best.aT = FLT_MAX;
    bool originInside = false;
    RaycastNode(aRoot, origin, invDir, &originInside, maxT, best);
    if(!best.aHit) return false;

    best.aPoint.x = origin.x + dir.x * best.aT;
    best.aPoint.y = origin.y + dir.y * best.aT;
    best.aPoint.z = origin.z + dir.z * best.aT;
    hit = best;
    return true;
}

void cBVH::RaycastNode(uint32_t node, const GNg::sVEC3 &origin, const GNg::sVEC3 &invDir,
                       bool *insideOut, float maxT, sRAY_HIT &hit) const {
    if(node >= aNodes.size()) return;

    const GNg::sVEC3 dir = RecoverDirection(invDir);
    float entryT = 0.0f;
    if(!RayIntersectAABB(origin, dir, aNodes[node].aBox, maxT, entryT)) return;

    // insideOut 表示“起点落在当前节点盒内”，仅供上层诊断；命中裁剪仍以 entryT 为准。
    if(insideOut) *insideOut = entryT <= 0.0f;

    // 更近的命中才需要继续；相等时继续，以便同 t 取索引较小者。
    if(hit.aHit && entryT > hit.aT + 1e-6f) return;

    if(aNodes[node].IsLeaf()) {
        const uint32_t first = aNodes[node].aFirst;
        const uint32_t count = aNodes[node].aCount;
        for(uint32_t i = first; i < first + count; ++i) {
            const uint32_t boxIndex = aOrder[i];
            float t = 0.0f;
            if(!RayIntersectAABB(origin, dir, aBoxes[boxIndex], maxT, t)) continue;
            const bool better = !hit.aHit || t < hit.aT - 1e-5f ||
                                (std::fabs(t - hit.aT) <= 1e-5f && boxIndex < hit.aIndex);
            if(better) {
                hit.aHit = true;
                hit.aT = t;
                hit.aIndex = boxIndex;
            }
        }
        return;
    }
    RaycastNode(aNodes[node].aLeft, origin, invDir, insideOut, maxT, hit);
    RaycastNode(aNodes[node].aRight, origin, invDir, insideOut, maxT, hit);
}

} // namespace GN3d
