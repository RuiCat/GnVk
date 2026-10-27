// GN_PhysicsCcd.cpp — 连续碰撞（CCD v1）：防高速刚体一步穿过静态/运动学几何。
//
// 做法（近似，契约见 GN_Physics.h 的 setContinuousCollision 注释）：
//   * 只用「上一位置 → 本步目标位置」的线段，对每个静态/运动学刚体的世界 AABB 做扫掠；
//     目标 AABB 按**本刚体包围半径**（球=半径、盒=半尺寸对角线长、胶囊=圆柱半长+半径）扩展，
//     因此对盒/胶囊的棱角是保守近似（可能提前停下，不会漏判）。
//   * 命中最早 TOI 时把刚体中心钳在扩展 AABB 表面后退 kCcdSlop，并清零**进入方向**的法向速度；
//     切向速度保留，恢复系数不参与 CCD（避免一次钳位注入能量）。
//   * v1 边界：只对静态/运动学目标扫掠（运动学目标的步内自身运动不扫掠）；只处理动态且醒着的刚体；
//     被钳位后本步不再继续剩余位移（下一子步由接触求解接手）。
#include <GN.h>
#include "GN_Physics.h"

#include <cfloat>
#include <cmath>

namespace GN3d {

namespace {

const float kCcdSlop = 1e-3f; // 钳位后退量（m），避免下一子步正好贴面产生重复命中
const float kCcdMinMotion = 1e-9f; // 位移平方下限

float sweepRadius(const sRIGID_BODY &body) {
    if(body.aShape == eSHAPE_SPHERE) return body.aRadius;
    if(body.aShape == eSHAPE_BOX) {
        return sqrtf(body.aHalfExtent.x * body.aHalfExtent.x +
                     body.aHalfExtent.y * body.aHalfExtent.y +
                     body.aHalfExtent.z * body.aHalfExtent.z);
    }
    if(body.aShape == eSHAPE_CAPSULE) return body.aHalfExtent.y + body.aRadius;
    return 0.0f;
}

// 点沿 delta 扫过 AABB：返回进入参数 t ∈ (0,1) 与进入面外法线。起点在盒内/未进入返回 false。
bool sweepPointAgainstAabb(const GNg::sVEC3 &origin, const GNg::sVEC3 &delta, const sAABB &box,
                           float &outT, GNg::sVEC3 &outNormal) {
    const float o[3] = {origin.x, origin.y, origin.z};
    const float d[3] = {delta.x, delta.y, delta.z};
    const float lo[3] = {box.aMin.x, box.aMin.y, box.aMin.z};
    const float hi[3] = {box.aMax.x, box.aMax.y, box.aMax.z};
    float tEnter = -FLT_MAX;
    float tExit = FLT_MAX;
    int axis = -1;
    for(int i = 0; i < 3; ++i) {
        if(std::fabs(d[i]) < 1e-12f) {
            if(o[i] < lo[i] || o[i] > hi[i]) return false;
            continue;
        }
        float t1 = (lo[i] - o[i]) / d[i];
        float t2 = (hi[i] - o[i]) / d[i];
        if(t1 > t2) {
            const float swapValue = t1;
            t1 = t2;
            t2 = swapValue;
        }
        if(t1 > tEnter) {
            tEnter = t1;
            axis = i;
        }
        if(t2 < tExit) tExit = t2;
        if(tEnter > tExit) return false;
    }
    // axis < 0：位移退化为点；tEnter <= 0：起点已在扩展盒内（交给接触求解，不在此处理）。
    if(axis < 0 || tEnter <= 0.0f || tEnter >= 1.0f) return false;
    outT = tEnter;
    const float sign = (d[axis] > 0.0f) ? -1.0f : 1.0f; // 前进方向 → 进入面外向法线取反
    outNormal.x = (axis == 0) ? sign : 0.0f;
    outNormal.y = (axis == 1) ? sign : 0.0f;
    outNormal.z = (axis == 2) ? sign : 0.0f;
    return true;
}

} // namespace

void cPHYSICS_WORLD::setContinuousCollision(bool enabled) {
    aContinuousCollision = enabled;
}

void cPHYSICS_WORLD::resolveContinuousCollisions() {
    const size_t bodyCount = aBodies.size();
    if(aPreviousPositions.size() < bodyCount) aPreviousPositions.resize(bodyCount);
    for(size_t i = 0; i < bodyCount; ++i) {
        if(!aUsed[i]) continue;
        sRIGID_BODY &body = aBodies[i];
        if(body.aType != eBODY_DYNAMIC || body.aSleeping) continue;
        const GNg::sVEC3 previous = aPreviousPositions[i];
        GNg::sVEC3 delta;
        delta.x = body.aPos.x - previous.x;
        delta.y = body.aPos.y - previous.y;
        delta.z = body.aPos.z - previous.z;
        if(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z < kCcdMinMotion) continue;
        const float radius = sweepRadius(body);
        if(!(radius > 0.0f)) continue;

        float bestT = 1.0f;
        GNg::sVEC3 bestNormal;
        bestNormal.x = 0.0f;
        bestNormal.y = 0.0f;
        bestNormal.z = 0.0f;
        bool hit = false;
        for(size_t other = 0; other < bodyCount; ++other) {
            if(other == i || !aUsed[other]) continue;
            const sRIGID_BODY &target = aBodies[other];
            if(target.aType == eBODY_DYNAMIC) continue; // v1：只对静态/运动学目标扫掠
            sAABB expanded = target.aWorldAABB;
            expanded.aMin.x -= radius;
            expanded.aMin.y -= radius;
            expanded.aMin.z -= radius;
            expanded.aMax.x += radius;
            expanded.aMax.y += radius;
            expanded.aMax.z += radius;
            float tEnter = 0.0f;
            GNg::sVEC3 normal;
            if(!sweepPointAgainstAabb(previous, delta, expanded, tEnter, normal)) continue;
            if(tEnter < bestT) {
                bestT = tEnter;
                bestNormal = normal;
                hit = true;
            }
        }
        if(!hit) continue;

        body.aPos.x = previous.x + delta.x * bestT - bestNormal.x * kCcdSlop;
        body.aPos.y = previous.y + delta.y * bestT - bestNormal.y * kCcdSlop;
        body.aPos.z = previous.z + delta.z * bestT - bestNormal.z * kCcdSlop;
        const float normalSpeed = body.aLinVel.x * bestNormal.x + body.aLinVel.y * bestNormal.y +
                                  body.aLinVel.z * bestNormal.z;
        if(normalSpeed < 0.0f) { // 只消去进入速度，离开方向不干预
            body.aLinVel.x -= bestNormal.x * normalSpeed;
            body.aLinVel.y -= bestNormal.y * normalSpeed;
            body.aLinVel.z -= bestNormal.z * normalSpeed;
        }
        BodyUpdateWorldState(body);
        const uint32_t treeSlot = aTreeSlotOfBody[i];
        if(treeSlot != INVALID_HANDLE) aBroadphase.Update(treeSlot, body.aWorldAABB);
    }
}

} // namespace GN3d
