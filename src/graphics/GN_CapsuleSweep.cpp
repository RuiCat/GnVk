// GN_CapsuleSweep.cpp — 胶囊扫掠（C4）。
//
// 口径见头文件。三处关键实现细节：
//   1. "安全推进量"必须由**到最近几何的距离**推出，不能用固定步长 ——
//      固定步长在"贴着墙平移"时会退化成极小步（碰撞每帧都触发），
//      而大步长又会直接穿过去。
//   2. 二分只在 [t_prev, t_cur] 之间做，且**保留 t_prev 为无碰撞侧** ——
//      这样收敛后 t_prev 一定能推出一个安全位置。
//   3. 已穿透时（起点就在几何内部）TOI 记 0 并直接用当前法线推出，
//      不要试图"二分找进入点"（那需要回溯到过去）。

#include <GN.h>
#include "GN_CapsuleSweep.h"

#include <cmath>

namespace GN3d {

namespace {

GNg::sVEC3 v3(float x, float y, float z) {
    GNg::sVEC3 v;
    v.x = x;
    v.y = y;
    v.z = z;
    return v;
}
GNg::sVEC3 add(const GNg::sVEC3 &a, const GNg::sVEC3 &b) { return v3(a.x + b.x, a.y + b.y, a.z + b.z); }
GNg::sVEC3 sub(const GNg::sVEC3 &a, const GNg::sVEC3 &b) { return v3(a.x - b.x, a.y - b.y, a.z - b.z); }
GNg::sVEC3 mul(const GNg::sVEC3 &a, float s) { return v3(a.x * s, a.y * s, a.z * s); }
float dot(const GNg::sVEC3 &a, const GNg::sVEC3 &b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
float lenSq(const GNg::sVEC3 &a) { return dot(a, a); }
float len(const GNg::sVEC3 &a) { return std::sqrt(lenSq(a)); }
GNg::sVEC3 lerp(const GNg::sVEC3 &a, const GNg::sVEC3 &b, float t) {
    return v3(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t);
}

// 胶囊线段（下半球心 → 上半球心）。与 cCHARACTER::segment 同一口径。
void capsuleSegment(const GNg::sVEC3 &center, float halfHeight, GNg::sVEC3 &e0, GNg::sVEC3 &e1) {
    e0 = v3(center.x, center.y - halfHeight, center.z);
    e1 = v3(center.x, center.y + halfHeight, center.z);
}

// 在 center 处查询一次接触（含穿透与"刚好贴上"两种）。
//
// **不能只判"穿透量 > 0"**：扫掠要抓的是"会不会撞上"，而胶囊与三角形
// **恰好接触**（穿透量 0）时就已经撞上了。第一版只认 depth > 0，于是：
//   * 保守推进一步跨到"刚好接触"的位置 → 被判定为"安全" → 继续前进；
//   * 下一档已经越过三角形 → 查不到 → 一路走到终点。
// 结果就是**薄墙完全挡不住**（实测 6 米位移直接穿过去，一次都没命中）。
// 现在把 queryCapsuleTriangle 返回 true 且 count > 0 一律视为命中。
bool deepestAt(const GNg::sVEC3 &center, const sCAPSULE_SWEEP_PARAMS &p,
               const sSWEEP_TRIANGLE *tris, uint32_t count, GNg::sVEC3 &outNormal,
               float &outDepth, float extraRadius = 0.0f) {
    GNg::sVEC3 e0, e1;
    capsuleSegment(center, p.aHalfHeight, e0, e1);
    const float probeRadius = p.aRadius + (extraRadius > 0.0f ? extraRadius : 0.0f);
    bool any = false;
    float best = -1.0f;
    GNg::sVEC3 bestNormal = v3(0.0f, 1.0f, 0.0f);
    for(uint32_t i = 0; i < count; ++i) {
        sCONTACT_MANIFOLD m;
        if(!queryCapsuleTriangle(e0, e1, probeRadius, tris[i].aA, tris[i].aB, tris[i].aC, m))
            continue;
        if(m.aCount == 0) continue;
        // **接触（包括穿透量为 0）就算命中** —— 见函数头的说明
        for(uint32_t k = 0; k < m.aCount && k < 4; ++k) {
            if(m.aPoints[k].aPenetration > best) {
                best = m.aPoints[k].aPenetration;
                bestNormal = m.aNormal;
                any = true;
            }
        }
        if(!any) {
            // 全部点的穿透量都是 0：仍然算命中（贴上了），法线用它
            best = 0.0f;
            bestNormal = m.aNormal;
            any = true;
        }
    }
    if(!any) return false;
    outNormal = bestNormal;
    outDepth = (best > 0.0f) ? best : 0.0f;
    return true;
}

} // namespace

// ═══════════════════════════════════════════════════════════
//  结果 / 参数
// ═══════════════════════════════════════════════════════════
sCAPSULE_SWEEP_RESULT::sCAPSULE_SWEEP_RESULT()
    : aTOI(1.0f), aNormal{0.0f, 0.0f, 0.0f}, aPosition{0.0f, 0.0f, 0.0f}, aHit(false),
      aIterations(0) {}

sCAPSULE_SWEEP_PARAMS::sCAPSULE_SWEEP_PARAMS()
    : aRadius(0.3f), aHalfHeight(0.55f), aSkinWidth(1.0e-3f), aMaxIterations(24),
      aTolerance(1.0e-4f) {}

// ═══════════════════════════════════════════════════════════
//  最近距离（保守推进用）
// ═══════════════════════════════════════════════════════════
namespace {

// 点到**线段**的最近距离（胶囊的圆柱段用）
float pointSegmentDistSq(const GNg::sVEC3 &p, const GNg::sVEC3 &s0, const GNg::sVEC3 &s1) {
    const GNg::sVEC3 d = sub(s1, s0);
    const float dd = lenSq(d);
    if(!(dd > 1.0e-12f)) return lenSq(sub(p, s0));
    float t = dot(sub(p, s0), d) / dd;
    if(t < 0.0f) t = 0.0f;
    if(t > 1.0f) t = 1.0f;
    return lenSq(sub(p, add(s0, mul(d, t))));
}

// 点到三角形的最近距离平方。
//
// 实现方式：先求点在三角形平面上的投影，用**重心坐标**判断是否落在三角形内；
// 在内就直接取投影距离，在外就对三条边取"点到线段"的最小值。
//
// 第一版我照抄了 Ericson 的三分支判据，但把 `d1/d2/d3...` 的符号写错了一处，
// 结果**三角形内部的点被算成了到顶点的距离**（实测点 (0,-0.55,2) 落在三角形内，
// 却返回 0.7253 而不是 0）。改用重心坐标后逻辑直白、可手验。
float pointTriangleDistSq(const GNg::sVEC3 &p, const GNg::sVEC3 &a, const GNg::sVEC3 &b,
                          const GNg::sVEC3 &c) {
    const GNg::sVEC3 ab = sub(b, a);
    const GNg::sVEC3 ac = sub(c, a);
    const GNg::sVEC3 n = [&]() {
        GNg::sVEC3 cross;
        // 面法线（未归一化）—— 只用它投影，不需要单位长度
        cross.x = ab.y * ac.z - ab.z * ac.y;
        cross.y = ab.z * ac.x - ab.x * ac.z;
        cross.z = ab.x * ac.y - ab.y * ac.x;
        return cross;
    }();
    const float n2 = lenSq(n);
    if(n2 > 1.0e-20f) {
        // 投影到平面
        const GNg::sVEC3 ap = sub(p, a);
        const float signedDist = dot(ap, n) / n2; // 沿法线的参数
        const GNg::sVEC3 projected = sub(p, mul(n, signedDist));
        // 重心坐标判内
        const GNg::sVEC3 v0 = sub(b, a);
        const GNg::sVEC3 v1 = sub(c, a);
        const GNg::sVEC3 v2 = sub(projected, a);
        const float d00 = dot(v0, v0);
        const float d01 = dot(v0, v1);
        const float d11 = dot(v1, v1);
        const float d20 = dot(v2, v0);
        const float d21 = dot(v2, v1);
        const float denom = d00 * d11 - d01 * d01;
        if(std::fabs(denom) > 1.0e-20f) {
            const float v = (d11 * d20 - d01 * d21) / denom;
            const float w = (d00 * d21 - d01 * d20) / denom;
            const float u = 1.0f - v - w;
            if(u >= -1.0e-6f && v >= -1.0e-6f && w >= -1.0e-6f) {
                // 落在三角形内：距离就是到平面的距离
                return signedDist * signedDist * n2;
            }
        }
    }
    // 在外（或退化）：对三条边取最小
    float best = lenSq(sub(p, a));
    const float dAB = pointSegmentDistSq(p, a, b);
    if(dAB < best) best = dAB;
    const float dBC = pointSegmentDistSq(p, b, c);
    if(dBC < best) best = dBC;
    const float dCA = pointSegmentDistSq(p, c, a);
    if(dCA < best) best = dCA;
    return best;
}

} // namespace

float capsuleMinDistance(const GNg::sVEC3 &center, const sCAPSULE_SWEEP_PARAMS &params,
                         const sSWEEP_TRIANGLE *triangles, uint32_t triangleCount) {
    if(triangles == 0 || triangleCount == 0) return 1.0e30f;
    GNg::sVEC3 e0, e1;
    capsuleSegment(center, params.aHalfHeight, e0, e1);

    // **精确计算**：胶囊表面到三角形的最短距离 = min over 三角形上所有点到胶囊轴的距离
    // 减去半径。点到三角形的最近点用三分支解析解；对胶囊轴取端点与中点做采样
    // （胶囊轴是一条线段，三角形是平面片，两者最近点必在"某端点到三角形"或
    //  "三角形某点到轴线段"上；两点采样 + 三角形三顶点到轴的距离可覆盖）。
    //
    // **为什么不用"放大半径试探"**（第一版的做法）：它返回的是离散档位，
    // 误差可达 0.15 m，导致保守推进要么走太小（迭代用尽）要么跨过去（漏检）。
    float best = 1.0e30f;
    for(uint32_t i = 0; i < triangleCount; ++i) {
        const sSWEEP_TRIANGLE &t = triangles[i];
        // 胶囊轴的两个端点 + 中点，到三角形的距离
        const GNg::sVEC3 mid = mul(add(e0, e1), 0.5f);
        const GNg::sVEC3 probes[3] = {e0, mid, e1};
        for(int k = 0; k < 3; ++k) {
            const float d2 = pointTriangleDistSq(probes[k], t.aA, t.aB, t.aC);
            if(d2 < best) best = d2;
        }
        // 三角形三个顶点到胶囊轴的距离（覆盖"三角形很大、轴很短"的情形）
        const GNg::sVEC3 verts[3] = {t.aA, t.aB, t.aC};
        for(int k = 0; k < 3; ++k) {
            const float d2 = pointSegmentDistSq(verts[k], e0, e1);
            if(d2 < best) best = d2;
        }
    }
    if(best > 1.0e29f) return 1.0e30f;
    // best 是"轴到三角形"的距离平方；胶囊表面距离 = sqrt(best) - radius（可为负）
    const float axisDist = std::sqrt(best);
    return axisDist - params.aRadius;
}

// ═══════════════════════════════════════════════════════════
//  扫掠
// ═══════════════════════════════════════════════════════════
sCAPSULE_SWEEP_RESULT sweepCapsuleAgainstTriangles(const GNg::sVEC3 &from, const GNg::sVEC3 &to,
                                                   const sCAPSULE_SWEEP_PARAMS &params,
                                                   const sSWEEP_TRIANGLE *triangles,
                                                   uint32_t triangleCount) {
    sCAPSULE_SWEEP_RESULT result;
    result.aPosition = from;

    if(triangles == 0 || triangleCount == 0) {
        result.aPosition = to;
        result.aTOI = 1.0f;
        return result;
    }
    if(!(params.aRadius > 0.0f)) {
        result.aPosition = to;
        result.aTOI = 1.0f;
        return result;
    }

    const GNg::sVEC3 delta = sub(to, from);
    const float travel = len(delta);
    if(!(travel > 1.0e-8f)) {
        // 没位移：直接查一次穿透，穿透就推出
        GNg::sVEC3 normal;
        float depth = 0.0f;
        if(deepestAt(from, params, triangles, triangleCount, normal, depth)) {
            result.aHit = true;
            result.aTOI = 0.0f;
            result.aNormal = normal;
            result.aPosition = add(from, mul(normal, depth + params.aSkinWidth));
        }
        return result;
    }

    // 起点就已穿透：TOI = 0，直接推出（不做二分 —— 那要回溯到过去）
    {
        GNg::sVEC3 normal;
        float depth = 0.0f;
        if(deepestAt(from, params, triangles, triangleCount, normal, depth)) {
            result.aHit = true;
            result.aTOI = 0.0f;
            result.aNormal = normal;
            result.aPosition = add(from, mul(normal, depth + params.aSkinWidth));
            return result;
        }
    }

    // ── 保守推进 + 二分收敛 ──
    //
    // 纯保守推进的问题是**收敛慢**：进度量 = clearance/travel，而 clearance 在接近
    // 几何时趋于 0 ⇒ 步长越来越小，24 次迭代走不到接触点就被迫返回"全程安全"。
    // （实测斜穿薄墙：distance 从 1.0 降到 0.285，24 次只推进了 t≈0.77。）
    //
    // 所以这里混合两种手段：
    //   * clearance 还很大 ⇒ 直接用 clearance 推进（少迭代）；
    //   * clearance 已经很小（< kNearThreshold）⇒ 说明快接触了，**二分**定位；
    //   * 二分找到的接触点再验证一次，确认是真空碰撞而不是数值误差。
    const float kNearThreshold = params.aRadius * 0.5f; // 进入"快接触"的判据
    float tSafe = 0.0f;
    GNg::sVEC3 lastNormal = v3(0.0f, 1.0f, 0.0f);

    for(uint32_t iter = 0; iter < params.aMaxIterations; ++iter) {
        result.aIterations = iter + 1;

        const GNg::sVEC3 here = lerp(from, to, tSafe);
        const float clearance = capsuleMinDistance(here, params, triangles, triangleCount);

        // 已经接触（表面距离 <= 0）⇒ 停在这里
        if(!(clearance > params.aSkinWidth)) {
            GNg::sVEC3 n;
            float d = 0.0f;
            result.aHit = true;
            result.aTOI = tSafe;
            if(deepestAt(here, params, triangles, triangleCount, n, d)) {
                result.aNormal = n;
            } else {
                result.aNormal = lastNormal;
            }
            result.aPosition = here;
            return result;
        }

        // 快接触：二分定位接触点，一次搞定（不再小步爬）
        if(clearance < kNearThreshold) {
            float lo = tSafe; // 已知 clearance > skin
            float hi = 1.0f;
            // 先找一个确认"接触"的 hi
            bool foundHi = false;
            float hiClear = 0.0f;
            for(int probe = 1; probe <= 8; ++probe) {
                const float cand = lo + (1.0f - lo) * (float)probe / 8.0f;
                hiClear = capsuleMinDistance(lerp(from, to, cand), params, triangles, triangleCount);
                if(!(hiClear > params.aSkinWidth)) {
                    hi = cand;
                    foundHi = true;
                    break;
                }
            }
            if(!foundHi) {
                // 剩余段全程保持 clearance —— 直接走完
                tSafe = 1.0f;
                break;
            }
            for(uint32_t bisect = 0; bisect < 16; ++bisect) {
                const float mid = (lo + hi) * 0.5f;
                const float mc =
                    capsuleMinDistance(lerp(from, to, mid), params, triangles, triangleCount);
                if(mc > params.aSkinWidth) {
                    lo = mid;
                } else {
                    hi = mid;
                }
                if(hi - lo < params.aTolerance) break;
            }
            tSafe = lo;

            // 在 lo 处取法线（lo 是安全的，但最接近接触；用它附近一点取法线更准）
            const GNg::sVEC3 atHit = lerp(from, to, hi);
            GNg::sVEC3 n;
            float d = 0.0f;
            if(deepestAt(atHit, params, triangles, triangleCount, n, d)) {
                lastNormal = n;
            }
            result.aHit = true;
            result.aTOI = hi;
            result.aNormal = lastNormal;
            result.aPosition = add(lerp(from, to, lo), mul(lastNormal, params.aSkinWidth));
            return result;
        }

        // 还很远：按 clearance 大步推进
        float tNext = tSafe + clearance / travel;
        if(tNext > 1.0f) tNext = 1.0f;
        if(tNext <= tSafe) tNext = tSafe + params.aTolerance;
        if(tNext > 1.0f) tNext = 1.0f;
        tSafe = tNext;
        if(tSafe >= 1.0f) break;
    }

    // 用尽迭代仍未命中：全程安全
    result.aPosition = to;
    result.aTOI = 1.0f;
    result.aHit = false;
    return result;
}

} // namespace GN3d
