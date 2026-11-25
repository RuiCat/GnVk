// GN_HiZ.cpp — Hi-Z 遮挡剔除 + Overdraw 测量（B4，纯 CPU）。
//
// 边界与算法见头文件。这里强调四处实现要点：
//   1. 金字塔取 **max** 深度（不是 min）：遮挡判定必须是保守的。
//   2. 屏幕矩形必须由 8 个角点算，不能只用中心 ± 半径 —— 旋转后的 AABB
//      在屏幕上的投影不是对称的。
//   3. 任何"不确定"都返回"没被遮挡"（宁可多画）。
//   4. 选 mip 级要选"能完整覆盖矩形的最小级"，否则会读到矩形外的深度。

#include <GN.h>
#include "GN_HiZ.h"

#include <cmath>

namespace GN3d {

namespace {

uint32_t ceilHalf(uint32_t v) { return (v + 1u) / 2u; }

// 世界点 → NDC。返回 false 表示 w<=0（在相机背后）。
bool projectToNdc(const GNg::sMAT44 &m, const GNg::sVEC3 &p, GNg::sVEC3 &outNdc) {
    const float x = m.m[0] * p.x + m.m[4] * p.y + m.m[8] * p.z + m.m[12];
    const float y = m.m[1] * p.x + m.m[5] * p.y + m.m[9] * p.z + m.m[13];
    const float z = m.m[2] * p.x + m.m[6] * p.y + m.m[10] * p.z + m.m[14];
    const float w = m.m[3] * p.x + m.m[7] * p.y + m.m[11] * p.z + m.m[15];
    if(!(w > 1.0e-6f)) return false;
    outNdc.x = x / w;
    outNdc.y = y / w;
    outNdc.z = z / w;
    return true;
}

} // namespace

// ═══════════════════════════════════════════════════════════
//  sOVERDRAW_STATS
// ═══════════════════════════════════════════════════════════
sOVERDRAW_STATS::sOVERDRAW_STATS()
    : aFragments(0), aCoveredPixels(0), aObjects(0), aWidth(0), aHeight(0) {}

float sOVERDRAW_STATS::OverdrawRatio() const {
    if(aCoveredPixels == 0) return 0.0f;
    return (float)((double)aFragments / (double)aCoveredPixels);
}

sOVERDRAW_STATS measureOverdraw(const GNg::sMAT44 &viewProj, uint32_t width, uint32_t height,
                                const sAABB *boxes, uint32_t count) {
    sOVERDRAW_STATS stats;
    stats.aWidth = width;
    stats.aHeight = height;
    if(width == 0 || height == 0 || boxes == nullptr || count == 0) return stats;
    // 上限 16M 像素（4096×4096）：诊断工具不该为了跑一次把内存打爆
    const uint64_t pixels = (uint64_t)width * (uint64_t)height;
    if(pixels > 16777216ull) return stats;

    std::vector<uint16_t> cover((size_t)pixels, 0u);

    for(uint32_t i = 0; i < count; ++i) {
        const sAABB &box = boxes[i];
        if(box.aMin.x > box.aMax.x) continue; // 空盒

        // 8 角点投影 → 屏幕矩形（含最近/最远 NDC 深度）
        float minX = 1e30f;
        float maxX = -1e30f;
        float minY = 1e30f;
        float maxY = -1e30f;
        float minZ = 1e30f;
        bool any = false;
        bool anyBehind = false;
        for(int c = 0; c < 8; ++c) {
            const GNg::sVEC3 corner{(c & 1) ? box.aMax.x : box.aMin.x,
                                    (c & 2) ? box.aMax.y : box.aMin.y,
                                    (c & 4) ? box.aMax.z : box.aMin.z};
            GNg::sVEC3 ndc;
            if(!projectToNdc(viewProj, corner, ndc)) {
                anyBehind = true;
                continue;
            }
            if(ndc.x < minX) minX = ndc.x;
            if(ndc.y < minY) minY = ndc.y;
            if(ndc.z < minZ) minZ = ndc.z;
            if(ndc.x > maxX) maxX = ndc.x;
            if(ndc.y > maxY) maxY = ndc.y;
            any = true;
        }
        if(!any || anyBehind) continue; // 有角点在背后：矩形不可靠，跳过（保守）

        // NDC → 像素（y 不翻转，见 gizmoRayFromScreen 的口径说明）
        int x0 = (int)std::floor((minX * 0.5f + 0.5f) * (float)width);
        int x1 = (int)std::ceil((maxX * 0.5f + 0.5f) * (float)width);
        int y0 = (int)std::floor((minY * 0.5f + 0.5f) * (float)height);
        int y1 = (int)std::ceil((maxY * 0.5f + 0.5f) * (float)height);
        if(x1 < 0 || y1 < 0 || x0 >= (int)width || y0 >= (int)height) continue;
        if(x0 < 0) x0 = 0;
        if(y0 < 0) y0 = 0;
        if(x1 > (int)width) x1 = (int)width;
        if(y1 > (int)height) y1 = (int)height;
        if(x1 <= x0 || y1 <= y0) continue;

        ++stats.aObjects;
        for(int y = y0; y < y1; ++y) {
            for(int x = x0; x < x1; ++x) {
                uint16_t &c = cover[(size_t)y * (size_t)width + (size_t)x];
                if(c < 0xFFFFu) ++c;
            }
        }
    }

    for(size_t i = 0; i < cover.size(); ++i) {
        if(cover[i] == 0) continue;
        ++stats.aCoveredPixels;
        stats.aFragments += (uint64_t)cover[i];
    }
    return stats;
}

// ═══════════════════════════════════════════════════════════
//  cHIZ
// ═══════════════════════════════════════════════════════════
cHIZ::cHIZ() : aLastLevel(0) {}

void cHIZ::Clear() {
    aLevels.clear();
    aLastLevel = 0;
}

void cHIZ::Build(const float *depth, uint32_t width, uint32_t height) {
    Clear();
    if(depth == nullptr || width == 0 || height == 0) return;

    sLevel base;
    base.aWidth = width;
    base.aHeight = height;
    base.aDepths.assign(depth, depth + (size_t)width * (size_t)height);
    aLevels.push_back(base);

    // 逐级取 2×2 的 **max**（保守：只有整块最远点都更近才算遮挡）
    while(aLevels.back().aWidth > 1u || aLevels.back().aHeight > 1u) {
        const sLevel &prev = aLevels.back();
        sLevel next;
        next.aWidth = ceilHalf(prev.aWidth);
        next.aHeight = ceilHalf(prev.aHeight);
        next.aDepths.resize((size_t)next.aWidth * (size_t)next.aHeight);
        for(uint32_t y = 0; y < next.aHeight; ++y) {
            for(uint32_t x = 0; x < next.aWidth; ++x) {
                float m = 0.0f;
                for(uint32_t dy = 0; dy < 2; ++dy) {
                    const uint32_t sy = y * 2u + dy;
                    if(sy >= prev.aHeight) continue;
                    for(uint32_t dx = 0; dx < 2; ++dx) {
                        const uint32_t sx = x * 2u + dx;
                        if(sx >= prev.aWidth) continue;
                        const float d = prev.aDepths[(size_t)sy * (size_t)prev.aWidth + (size_t)sx];
                        if(d > m) m = d;
                    }
                }
                next.aDepths[(size_t)y * (size_t)next.aWidth + (size_t)x] = m;
            }
        }
        aLevels.push_back(next);
        if(aLevels.size() > 16u) break; // 防御：尺寸异常时不无限增长
    }
}

uint32_t cHIZ::LevelWidth(uint32_t level) const {
    if(level >= aLevels.size()) return 0;
    return aLevels[level].aWidth;
}

uint32_t cHIZ::LevelHeight(uint32_t level) const {
    if(level >= aLevels.size()) return 0;
    return aLevels[level].aHeight;
}

float cHIZ::DepthAt(uint32_t level, uint32_t x, uint32_t y) const {
    if(level >= aLevels.size()) return 1.0f;
    const sLevel &lv = aLevels[level];
    if(x >= lv.aWidth || y >= lv.aHeight) return 1.0f;
    return lv.aDepths[(size_t)y * (size_t)lv.aWidth + (size_t)x];
}

bool cHIZ::IsOccluded(const GNg::sMAT44 &viewProj, uint32_t viewportW, uint32_t viewportH,
                      const sAABB &box) const {
    aLastLevel = 0;
    if(aLevels.empty()) return false;
    if(viewportW == 0 || viewportH == 0) return false;
    if(box.aMin.x > box.aMax.x) return false; // 空盒

    // ── 8 角点 → NDC 矩形 + 最近深度 ──
    float minX = 1e30f;
    float maxX = -1e30f;
    float minY = 1e30f;
    float maxY = -1e30f;
    float nearZ = 1e30f;
    for(int c = 0; c < 8; ++c) {
        const GNg::sVEC3 corner{(c & 1) ? box.aMax.x : box.aMin.x,
                                (c & 2) ? box.aMax.y : box.aMin.y,
                                (c & 4) ? box.aMax.z : box.aMin.z};
        GNg::sVEC3 ndc;
        // 任一角点在背后 ⇒ 矩形不可靠，直接判"没被遮挡"（保守）
        if(!projectToNdc(viewProj, corner, ndc)) return false;
        // 跨越近平面（NDC z < 0）同理：投影会把它们拉到屏幕外，矩形失去意义
        if(ndc.z < 0.0f) return false;
        if(ndc.x < minX) minX = ndc.x;
        if(ndc.y < minY) minY = ndc.y;
        if(ndc.z < nearZ) nearZ = ndc.z;
        if(ndc.x > maxX) maxX = ndc.x;
        if(ndc.y > maxY) maxY = ndc.y;
    }
    // 完全在 NDC 之外 ⇒ 不可见（不是"被遮挡"，但调用方可以据此跳过）
    if(maxX < -1.0f || minX > 1.0f || maxY < -1.0f || minY > 1.0f) return false;

    // ── NDC 矩形 → 第 0 级像素矩形 ──
    const float w0 = (float)aLevels[0].aWidth;
    const float h0 = (float)aLevels[0].aHeight;
    float px0 = (minX * 0.5f + 0.5f) * w0;
    float px1 = (maxX * 0.5f + 0.5f) * w0;
    float py0 = (minY * 0.5f + 0.5f) * h0;
    float py1 = (maxY * 0.5f + 0.5f) * h0;
    if(px0 < 0.0f) px0 = 0.0f;
    if(py0 < 0.0f) py0 = 0.0f;
    if(px1 > w0) px1 = w0;
    if(py1 > h0) py1 = h0;
    if(px1 <= px0 || py1 <= py0) return false;

    // ── 选"能完整覆盖矩形的最小 mip 级" ──
    //
    // 级 k 的 texel 边长（第 0 级像素数）= 2^k。要覆盖 [px0,px1) × [py0,py1)，
    // 需要覆盖它的 texel 数与矩形像素跨度的关系：
    //   覆盖起点 texel = floor(px0 / 2^k)，终点 texel = ceil(px1 / 2^k)
    // 选最小的 k 使 texel 跨度 == 1 且起点==终点（即矩形落在单 texel 内）。
    // 若到了最顶层仍不满足 ⇒ 矩形太大，返回 false（不做测试）。
    uint32_t level = 0;
    for(uint32_t k = 0; k + 1 < aLevels.size(); ++k) {
        const uint32_t cell = 1u << k;
        const int tx0 = (int)std::floor(px0 / (float)cell);
        const int tx1 = (int)std::ceil(px1 / (float)cell);
        const int ty0 = (int)std::floor(py0 / (float)cell);
        const int ty1 = (int)std::ceil(py1 / (float)cell);
        if(tx1 - tx0 <= 1 && ty1 - ty0 <= 1) {
            level = k;
            break;
        }
        level = k + 1; // 没覆盖住就继续往粗走
    }
    // 落到最粗一级仍覆盖不住 ⇒ 放弃（保守）
    {
        const uint32_t cell = (level < 16u) ? (1u << level) : (1u << 15);
        const int tx0 = (int)std::floor(px0 / (float)cell);
        const int tx1 = (int)std::ceil(px1 / (float)cell);
        const int ty0 = (int)std::floor(py0 / (float)cell);
        const int ty1 = (int)std::ceil(py1 / (float)cell);
        if(tx1 - tx0 > 1 || ty1 - ty0 > 1) return false;
    }
    if(level >= aLevels.size()) level = (uint32_t)aLevels.size() - 1u;
    aLastLevel = level;

    const uint32_t cell = 1u << level;
    const uint32_t tx = (uint32_t)(px0 / (float)cell);
    const uint32_t ty = (uint32_t)(py0 / (float)cell);

    // 该 texel 的 max 深度**严格小于**物体最近深度 ⇒ 整块区域都在物体前面 ⇒ 被遮挡。
    // 留 1e-5 的余量，避免浮点相等时的抖动。
    const float occluder = DepthAt(level, tx, ty);
    return occluder < nearZ - 1.0e-5f;
}

} // namespace GN3d
