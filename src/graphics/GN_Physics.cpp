// GN_Physics.cpp — 物理世界：固定步长循环、宽相（动态 AABB 树）、配对、求解调度、休眠与射线。
//
// 每步顺序：速度积分（重力/阻尼）→ 构建接触（宽相 + 窄相）→ 速度求解 → 位置修正 → 位置积分 → 休眠更新。
// 契约与语义见 GN_Physics.h；本文件不依赖 Vulkan。
#include <GN.h>
#include "GN_Physics.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <vector>

namespace GN3d {

namespace {

const float kSleepLinearSpeed = 0.05f; // m/s
const float kSleepAngularSpeed = 0.08f; // rad/s
const float kSleepDelaySeconds = 0.5f;
const float kMaxFrameDt = 0.25f;
const float kMaxRayT = 1e30f;
const uint32_t kVelocityIterations = 20;
const uint32_t kPositionIterations = 4;
const float kWarmStartMatchDistance = 0.05f; // 接触点跨步匹配半径（m）

bool IsDynamicBody(const sRIGID_BODY &body) {
    return body.aType == eBODY_DYNAMIC;
}

float SpeedSq(const GNg::sVEC3 &v) {
    return v.x * v.x + v.y * v.y + v.z * v.z;
}

// 世界逆惯量乘向量：(I⁻¹ * v)_i = Σ_j M[i][j] v_j = Σ_j Elem(j, i) v_j（列主序）。
GNg::sVEC3 MulInvInertia(const GNg::sMAT44 &invInertia, const GNg::sVEC3 &v) {
    GNg::sVEC3 out;
    out.x = invInertia.Elem(0, 0) * v.x + invInertia.Elem(1, 0) * v.y + invInertia.Elem(2, 0) * v.z;
    out.y = invInertia.Elem(0, 1) * v.x + invInertia.Elem(1, 1) * v.y + invInertia.Elem(2, 1) * v.z;
    out.z = invInertia.Elem(0, 2) * v.x + invInertia.Elem(1, 2) * v.y + invInertia.Elem(2, 2) * v.z;
    return out;
}

// 射线与球：返回进入距离（起点在球内为 0）。
bool RaySphere(const GNg::sVEC3 &origin, const GNg::sVEC3 &dir, const GNg::sVEC3 &center,
               float radius, float maxT, float &outT) {
    const float ox = origin.x - center.x;
    const float oy = origin.y - center.y;
    const float oz = origin.z - center.z;
    const float b = ox * dir.x + oy * dir.y + oz * dir.z;
    const float c = ox * ox + oy * oy + oz * oz - radius * radius;
    if(c <= 0.0f) {
        outT = 0.0f; // 起点在球内
        return maxT >= 0.0f;
    }
    if(b > 0.0f) return false; // 球在射线背后
    const float disc = b * b - c;
    if(disc < 0.0f) return false;
    float t = -b - sqrtf(disc);
    if(t < 0.0f) t = 0.0f;
    if(t > maxT) return false;
    outT = t;
    return true;
}

// 射线与盒：先把射线变到盒局部空间，再做 slab 测试。
bool RayBox(const GNg::sVEC3 &origin, const GNg::sVEC3 &dir, const sRIGID_BODY &box, float maxT,
            float &outT) {
    GNg::sQUAT invRot;
    GNg::QuatInverse(&invRot, &box.aRot);
    GNg::sVEC3 localOrigin;
    GNg::sVEC3 rel;
    rel.x = origin.x - box.aPos.x;
    rel.y = origin.y - box.aPos.y;
    rel.z = origin.z - box.aPos.z;
    GNg::QuatRotateVec3(&localOrigin, &rel, &invRot);
    GNg::sVEC3 localDir;
    GNg::QuatRotateVec3(&localDir, &dir, &invRot);

    float tEnter = -FLT_MAX;
    float tExit = FLT_MAX;
    const float originAxis[3] = {localOrigin.x, localOrigin.y, localOrigin.z};
    const float dirAxis[3] = {localDir.x, localDir.y, localDir.z};
    const float halfAxis[3] = {box.aHalfExtent.x, box.aHalfExtent.y, box.aHalfExtent.z};
    for(int axis = 0; axis < 3; ++axis) {
        if(std::fabs(dirAxis[axis]) < 1e-12f) {
            if(originAxis[axis] < -halfAxis[axis] || originAxis[axis] > halfAxis[axis]) return false;
            continue;
        }
        float t1 = (-halfAxis[axis] - originAxis[axis]) / dirAxis[axis];
        float t2 = (halfAxis[axis] - originAxis[axis]) / dirAxis[axis];
        if(t1 > t2) {
            const float swapValue = t1;
            t1 = t2;
            t2 = swapValue;
        }
        if(t1 > tEnter) tEnter = t1;
        if(t2 < tExit) tExit = t2;
        if(tEnter > tExit) return false;
    }
    if(tExit < 0.0f) return false;
    if(tEnter < 0.0f) tEnter = 0.0f;
    if(tEnter > maxT) return false;
    outT = tEnter;
    return true;
}

// 胶囊两端球心（局部 Y 轴线段绕朝向旋转后平移到世界）
void CapsuleEndpoints(const sRIGID_BODY &body, GNg::sVEC3 &outA, GNg::sVEC3 &outB) {
    GNg::sVEC3 up{0.0f, 1.0f, 0.0f};
    GNg::sVEC3 axis;
    GNg::QuatRotateVec3(&axis, &up, &body.aRot);
    const float halfHeight = body.aHalfExtent.y;
    outA.x = body.aPos.x - axis.x * halfHeight;
    outA.y = body.aPos.y - axis.y * halfHeight;
    outA.z = body.aPos.z - axis.z * halfHeight;
    outB.x = body.aPos.x + axis.x * halfHeight;
    outB.y = body.aPos.y + axis.y * halfHeight;
    outB.z = body.aPos.z + axis.z * halfHeight;
}

// 射线-胶囊：沿轴采样若干球做求交（近似；17 个采样点在 r=0.5、h=1 时误差 <1mm，
// 契约未要求解析解，先按近似实现并在此注明）。
bool RayCapsule(const GNg::sVEC3 &origin, const GNg::sVEC3 &dir, const sRIGID_BODY &body,
                float maxT, float &outT) {
    GNg::sVEC3 a;
    GNg::sVEC3 b;
    CapsuleEndpoints(body, a, b);
    const int kSamples = 16;
    bool hit = false;
    float bestT = FLT_MAX;
    for(int i = 0; i <= kSamples; ++i) {
        const float u = (float)i / (float)kSamples;
        GNg::sVEC3 center;
        center.x = a.x + (b.x - a.x) * u;
        center.y = a.y + (b.y - a.y) * u;
        center.z = a.z + (b.z - a.z) * u;
        float t = 0.0f;
        if(!RaySphere(origin, dir, center, body.aRadius, maxT, t)) continue;
        if(t < bestT) {
            bestT = t;
            hit = true;
        }
    }
    if(!hit) return false;
    outT = bestT;
    return true;
}

bool RayBody(const GNg::sVEC3 &origin, const GNg::sVEC3 &dir, const sRIGID_BODY &body, float maxT,
             float &outT) {
    if(body.aShape == eSHAPE_SPHERE) {
        return RaySphere(origin, dir, body.aPos, body.aRadius, maxT, outT);
    }
    if(body.aShape == eSHAPE_BOX) {
        return RayBox(origin, dir, body, maxT, outT);
    }
    if(body.aShape == eSHAPE_CAPSULE) {
        return RayCapsule(origin, dir, body, maxT, outT);
    }
    return false;
}

} // namespace

// 类内 static const 成员被按引用使用（如 vector::push_back）时需要类外定义
const uint32_t cPHYSICS_WORLD::INVALID_HANDLE;

// ═══════════════════════════════════════════════════════════
//  cPHYSICS_WORLD
// ═══════════════════════════════════════════════════════════

cPHYSICS_WORLD::cPHYSICS_WORLD()
    : aGravity{0.0f, -9.81f, 0.0f}, aFixedStep(1.0f / 60.0f), aAccumulator(0.0f),
      aMaxSubSteps(4), aBodyCount(0), aLastContactCount(0) {}

cPHYSICS_WORLD::~cPHYSICS_WORLD() {
    Clear();
}

void cPHYSICS_WORLD::SetGravity(const GNg::sVEC3 &gravity) {
    aGravity = gravity;
}

void cPHYSICS_WORLD::SetFixedTimestep(float step, uint32_t maxSubSteps) {
    if(step > 0.0f) aFixedStep = step;
    aMaxSubSteps = maxSubSteps > 0 ? maxSubSteps : 1;
    aAccumulator = 0.0f;
}

uint32_t cPHYSICS_WORLD::AddBody(const sRIGID_BODY &body) {
    if(body.aType != eBODY_STATIC && body.aType != eBODY_DYNAMIC && body.aType != eBODY_KINEMATIC) {
        return INVALID_HANDLE;
    }
    if(body.aShape != eSHAPE_SPHERE && body.aShape != eSHAPE_BOX &&
       body.aShape != eSHAPE_CAPSULE) return INVALID_HANDLE;
    if(body.aShape == eSHAPE_SPHERE && !(body.aRadius > 0.0f)) return INVALID_HANDLE;
    if(body.aShape == eSHAPE_BOX) {
        if(!(body.aHalfExtent.x > 0.0f) || !(body.aHalfExtent.y > 0.0f) ||
           !(body.aHalfExtent.z > 0.0f)) {
            return INVALID_HANDLE;
        }
    }
    if(body.aShape == eSHAPE_CAPSULE) {
        if(!(body.aRadius > 0.0f) || !(body.aHalfExtent.y >= 0.0f)) return INVALID_HANDLE;
    }
    if(body.aType == eBODY_DYNAMIC && !(body.aMass > 0.0f)) return INVALID_HANDLE;

    uint32_t handle;
    if(!aFreeHandles.empty()) {
        handle = aFreeHandles.back();
        aFreeHandles.pop_back();
        aBodies[handle] = body;
    } else {
        handle = (uint32_t)aBodies.size();
        aBodies.push_back(body);
        aUsed.push_back(0);
        aTreeSlotOfBody.push_back(INVALID_HANDLE);
    }
    aUsed[handle] = 1;
    aBodies[handle].aSleepTime = 0.0f;
    if(!IsDynamicBody(aBodies[handle])) aBodies[handle].aSleeping = false;
    BodyUpdateInertia(aBodies[handle]);
    BodyUpdateWorldState(aBodies[handle]);

    const uint32_t treeSlot = aBroadphase.Insert(aBodies[handle].aWorldAABB, 0);
    if(treeSlot == cDYNAMIC_AABB_TREE::INVALID_HANDLE) {
        aUsed[handle] = 0;
        aFreeHandles.push_back(handle);
        return INVALID_HANDLE;
    }
    aTreeSlotOfBody[handle] = treeSlot;
    if(aBodyOfTreeSlot.size() <= treeSlot) aBodyOfTreeSlot.resize(treeSlot + 1, INVALID_HANDLE);
    aBodyOfTreeSlot[treeSlot] = handle;
    ++aBodyCount;
    return handle;
}

bool cPHYSICS_WORLD::RemoveBody(uint32_t handle) {
    if(handle >= aBodies.size() || !aUsed[handle]) return false;
    const uint32_t treeSlot = aTreeSlotOfBody[handle];
    if(treeSlot != INVALID_HANDLE) {
        aBroadphase.Remove(treeSlot);
        if(treeSlot < aBodyOfTreeSlot.size()) aBodyOfTreeSlot[treeSlot] = INVALID_HANDLE;
        aTreeSlotOfBody[handle] = INVALID_HANDLE;
    }
    aUsed[handle] = 0;
    aBodies[handle].aSleeping = true;
    aFreeHandles.push_back(handle);
    aWarmContacts.clear(); // 句柄可能被复用，旧接触冲量不再对应当前刚体
    if(aBodyCount > 0) --aBodyCount;
    return true;
}

sRIGID_BODY *cPHYSICS_WORLD::GetBody(uint32_t handle) {
    if(handle >= aBodies.size() || !aUsed[handle]) return 0;
    return &aBodies[handle];
}

const sRIGID_BODY *cPHYSICS_WORLD::GetBody(uint32_t handle) const {
    if(handle >= aBodies.size() || !aUsed[handle]) return 0;
    return &aBodies[handle];
}

void cPHYSICS_WORLD::Clear() {
    aBodies.clear();
    aUsed.clear();
    aFreeHandles.clear();
    aTreeSlotOfBody.clear();
    aBodyOfTreeSlot.clear();
    aBroadphase.Clear();
    aConstraints.clear();
    aConstraintBodyLo.clear();
    aConstraintBodyHi.clear();
    aWarmContacts.clear();
    aAccumulator = 0.0f;
    aBodyCount = 0;
    aLastContactCount = 0;
}

// 用上一步的接触冲量预填本步约束：按刚体对匹配，再按接触点世界位置就近匹配（阈值 5cm）。
// 找不到对应点就保持 0（该点冷启动）；刚体对无缓存时不预填。
void cPHYSICS_WORLD::seedWarmStart(uint32_t bodyLo, uint32_t bodyHi, sCONTACT_CONSTRAINT &constraint) {
    const uint32_t pointCount = constraint.aManifold.aCount > 4u ? 4u : constraint.aManifold.aCount;
    for(uint32_t pi = 0; pi < 4; ++pi) {
        constraint.aAccNormal[pi] = 0.0f;
        constraint.aAccTangent[pi] = 0.0f;
    }
    for(size_t entry = 0; entry < aWarmContacts.size(); ++entry) {
        const sWARM_CONTACT &cached = aWarmContacts[entry];
        if(cached.aBodyLo != bodyLo || cached.aBodyHi != bodyHi) continue;
        for(uint32_t pi = 0; pi < pointCount; ++pi) {
            const GNg::sVEC3 &point = constraint.aManifold.aPoints[pi].aPoint;
            float bestDistanceSq = kWarmStartMatchDistance * kWarmStartMatchDistance;
            uint32_t matchIndex = 0xFFFFFFFFu;
            for(uint32_t ci = 0; ci < cached.aPointCount; ++ci) {
                const float dx = point.x - cached.aPoints[ci].x;
                const float dy = point.y - cached.aPoints[ci].y;
                const float dz = point.z - cached.aPoints[ci].z;
                const float distanceSq = dx * dx + dy * dy + dz * dz;
                if(distanceSq <= bestDistanceSq) {
                    bestDistanceSq = distanceSq;
                    matchIndex = ci;
                }
            }
            if(matchIndex == 0xFFFFFFFFu) continue;
            constraint.aAccNormal[pi] = cached.aAccNormal[matchIndex];
            constraint.aAccTangent[pi] = cached.aAccTangent[matchIndex];
        }
        return; // 每对刚体最多一条缓存
    }
}

// 把本子步求解后的累计冲量写入缓存，供下一子步 warm start。
void cPHYSICS_WORLD::cacheContacts() {
    aWarmContacts.clear();
    if(aConstraints.empty()) return;
    aWarmContacts.reserve(aConstraints.size());
    for(size_t i = 0; i < aConstraints.size(); ++i) {
        const sCONTACT_CONSTRAINT &constraint = aConstraints[i];
        sWARM_CONTACT entry;
        entry.aBodyLo = aConstraintBodyLo[i];
        entry.aBodyHi = aConstraintBodyHi[i];
        entry.aPointCount = constraint.aManifold.aCount > 4u ? 4u : constraint.aManifold.aCount;
        for(uint32_t pi = 0; pi < 4; ++pi) {
            if(pi < entry.aPointCount) {
                entry.aPoints[pi] = constraint.aManifold.aPoints[pi].aPoint;
            } else {
                entry.aPoints[pi].x = 0.0f;
                entry.aPoints[pi].y = 0.0f;
                entry.aPoints[pi].z = 0.0f;
            }
            entry.aAccNormal[pi] = constraint.aAccNormal[pi];
            entry.aAccTangent[pi] = constraint.aAccTangent[pi];
        }
        aWarmContacts.push_back(entry);
    }
}

void cPHYSICS_WORLD::WakeBody(uint32_t handle) {
    if(handle >= aBodies.size() || !aUsed[handle]) return;
    if(aBodies[handle].aType != eBODY_DYNAMIC) return;
    aBodies[handle].aSleeping = false;
    aBodies[handle].aSleepTime = 0.0f;
}

uint32_t cPHYSICS_WORLD::GetAwakeCount() const {
    uint32_t awake = 0;
    for(size_t i = 0; i < aBodies.size(); ++i) {
        if(aUsed[i] && IsDynamicBody(aBodies[i]) && !aBodies[i].aSleeping) ++awake;
    }
    return awake;
}

void cPHYSICS_WORLD::Step(float dt) {
    if(!(dt > 0.0f)) return; // NaN / 0 / 负数都不推进
    if(dt > kMaxFrameDt) dt = kMaxFrameDt;
    aAccumulator += dt;
    uint32_t steps = 0;
    while(aAccumulator >= aFixedStep && steps < aMaxSubSteps) {
        StepOnce(aFixedStep);
        aAccumulator -= aFixedStep;
        ++steps;
    }
    // 子步用尽仍有积压：丢弃余量，避免"死亡螺旋"
    if(steps >= aMaxSubSteps && aAccumulator > aFixedStep) aAccumulator = 0.0f;
}

void cPHYSICS_WORLD::StepOnce(float dt) {
    IntegrateVelocities(dt);
    BuildContacts();
    if(!aConstraints.empty()) {
        SolveVelocities(&aConstraints[0], (uint32_t)aConstraints.size(), dt, kVelocityIterations, true);
        SolvePositions(&aConstraints[0], (uint32_t)aConstraints.size(), kPositionIterations);
    }
    cacheContacts(); // 本子步的累计冲量留给下一子步 warm start
    IntegratePositions(dt);
    UpdateSleep(dt);
    aLastContactCount = (uint32_t)aConstraints.size();
}

void cPHYSICS_WORLD::IntegrateVelocities(float dt) {
    for(size_t i = 0; i < aBodies.size(); ++i) {
        if(!aUsed[i]) continue;
        sRIGID_BODY &body = aBodies[i];
        if(body.aType == eBODY_STATIC) {
            body.aLinVel.x = body.aLinVel.y = body.aLinVel.z = 0.0f;
            body.aAngVel.x = body.aAngVel.y = body.aAngVel.z = 0.0f;
            continue;
        }
        if(body.aType == eBODY_KINEMATIC) continue; // 速度由调用方给定
        if(body.aSleeping) continue;
        body.aLinVel.x += aGravity.x * dt;
        body.aLinVel.y += aGravity.y * dt;
        body.aLinVel.z += aGravity.z * dt;
        const float linDamp = 1.0f / (1.0f + body.aLinearDamping * dt);
        const float angDamp = 1.0f / (1.0f + body.aAngularDamping * dt);
        body.aLinVel.x *= linDamp;
        body.aLinVel.y *= linDamp;
        body.aLinVel.z *= linDamp;
        body.aAngVel.x *= angDamp;
        body.aAngVel.y *= angDamp;
        body.aAngVel.z *= angDamp;
    }
}

void cPHYSICS_WORLD::BuildContacts() {
    aConstraints.clear();
    aConstraintBodyLo.clear();
    aConstraintBodyHi.clear();

    std::vector<uint32_t> candidates;
    std::vector<uint64_t> pairs;
    for(size_t i = 0; i < aBodies.size(); ++i) {
        if(!aUsed[i]) continue;
        const sRIGID_BODY &body = aBodies[i];
        if(!IsDynamicBody(body) || body.aSleeping) continue;
        candidates.clear();
        aBroadphase.QueryHandles(body.aWorldAABB, candidates);
        for(size_t c = 0; c < candidates.size(); ++c) {
            const uint32_t slot = candidates[c];
            if(slot >= aBodyOfTreeSlot.size()) continue;
            const uint32_t other = aBodyOfTreeSlot[slot];
            if(other == INVALID_HANDLE || other == (uint32_t)i) continue;
            if(other >= aBodies.size() || !aUsed[other]) continue;
            const uint32_t lo = (uint32_t)i < other ? (uint32_t)i : other;
            const uint32_t hi = (uint32_t)i < other ? other : (uint32_t)i;
            pairs.push_back(((uint64_t)lo << 32) | (uint64_t)hi);
        }
    }
    std::sort(pairs.begin(), pairs.end());
    pairs.erase(std::unique(pairs.begin(), pairs.end()), pairs.end());

    const float wakeSpeed = 2.0f * kSleepLinearSpeed;
    for(size_t p = 0; p < pairs.size(); ++p) {
        const uint32_t lo = (uint32_t)(pairs[p] >> 32);
        const uint32_t hi = (uint32_t)(pairs[p] & 0xFFFFFFFFu);
        sRIGID_BODY &A = aBodies[lo];
        sRIGID_BODY &B = aBodies[hi];
        if(!IsDynamicBody(A) && !IsDynamicBody(B)) continue;

        // 睡眠体被"正在运动的另一体"撞到 ⇒ 唤醒
        const bool aMoving = IsDynamicBody(A) ? !A.aSleeping && SpeedSq(A.aLinVel) > wakeSpeed * wakeSpeed
                             : (A.aType == eBODY_KINEMATIC && SpeedSq(A.aLinVel) > wakeSpeed * wakeSpeed);
        const bool bMoving = IsDynamicBody(B) ? !B.aSleeping && SpeedSq(B.aLinVel) > wakeSpeed * wakeSpeed
                             : (B.aType == eBODY_KINEMATIC && SpeedSq(B.aLinVel) > wakeSpeed * wakeSpeed);
        if(IsDynamicBody(A) && A.aSleeping && bMoving) WakeBody(lo);
        if(IsDynamicBody(B) && B.aSleeping && aMoving) WakeBody(hi);
        // 仍处于睡眠（或静态）的刚体不参与求解，避免被邻居"拖着动"
        if(IsDynamicBody(A) && A.aSleeping) continue;
        if(IsDynamicBody(B) && B.aSleeping) continue;

        sCONTACT_MANIFOLD manifold;
        if(!CollideBodies(A, B, manifold) || manifold.aCount == 0) continue;

        sCONTACT_CONSTRAINT constraint;
        constraint.aBodyA = &A;
        constraint.aBodyB = &B;
        constraint.aManifold = manifold;
        seedWarmStart(lo, hi, constraint); // 从上一子步的累计冲量预填（无匹配则清零）
        aConstraints.push_back(constraint);
        aConstraintBodyLo.push_back(lo);
        aConstraintBodyHi.push_back(hi);
    }
}

void cPHYSICS_WORLD::IntegratePositions(float dt) {
    for(size_t i = 0; i < aBodies.size(); ++i) {
        if(!aUsed[i]) continue;
        sRIGID_BODY &body = aBodies[i];
        if(body.aType == eBODY_STATIC) {
            BodyUpdateWorldState(body);
            const uint32_t staticSlot = aTreeSlotOfBody[i];
            if(staticSlot != INVALID_HANDLE) aBroadphase.Update(staticSlot, body.aWorldAABB);
            continue;
        }
        if(IsDynamicBody(body) && body.aSleeping) {
            BodyUpdateWorldState(body);
            const uint32_t sleepingSlot = aTreeSlotOfBody[i];
            if(sleepingSlot != INVALID_HANDLE) aBroadphase.Update(sleepingSlot, body.aWorldAABB);
            continue;
        }
        // 动态（醒着）与运动学刚体都按速度积分位置
        body.aPos.x += body.aLinVel.x * dt;
        body.aPos.y += body.aLinVel.y * dt;
        body.aPos.z += body.aLinVel.z * dt;
        GNg::sQUAT omega(body.aAngVel.x, body.aAngVel.y, body.aAngVel.z, 0.0f);
        GNg::sQUAT delta = omega * body.aRot; // dq/dt = 0.5 * ω * q
        body.aRot.x += 0.5f * delta.x * dt;
        body.aRot.y += 0.5f * delta.y * dt;
        body.aRot.z += 0.5f * delta.z * dt;
        body.aRot.w += 0.5f * delta.w * dt;
        GNg::QuatNormalize(&body.aRot, &body.aRot);
        BodyUpdateWorldState(body);
        const uint32_t treeSlot = aTreeSlotOfBody[i];
        if(treeSlot != INVALID_HANDLE) aBroadphase.Update(treeSlot, body.aWorldAABB);
    }
}

void cPHYSICS_WORLD::UpdateSleep(float dt) {
    const float linLimit = kSleepLinearSpeed * kSleepLinearSpeed;
    const float angLimit = kSleepAngularSpeed * kSleepAngularSpeed;
    for(size_t i = 0; i < aBodies.size(); ++i) {
        if(!aUsed[i]) continue;
        sRIGID_BODY &body = aBodies[i];
        if(!IsDynamicBody(body) || body.aSleeping) continue;
        if(SpeedSq(body.aLinVel) < linLimit && SpeedSq(body.aAngVel) < angLimit) {
            body.aSleepTime += dt;
            if(body.aSleepTime >= kSleepDelaySeconds) {
                body.aSleeping = true;
                body.aLinVel.x = body.aLinVel.y = body.aLinVel.z = 0.0f;
                body.aAngVel.x = body.aAngVel.y = body.aAngVel.z = 0.0f;
            }
        } else {
            body.aSleepTime = 0.0f;
        }
    }
}

bool cPHYSICS_WORLD::Raycast(const GNg::sVEC3 &origin, const GNg::sVEC3 &dir, float maxT,
                             sRAY_HIT &hit) const {
    hit.Reset();
    if(maxT < 0.0f) return false;
    if(maxT > kMaxRayT) maxT = kMaxRayT;

    uint32_t bestHandle = INVALID_HANDLE;
    float bestT = FLT_MAX;
    for(size_t i = 0; i < aBodies.size(); ++i) {
        if(!aUsed[i]) continue;
        float t = 0.0f;
        if(!RayBody(origin, dir, aBodies[i], maxT, t)) continue;
        const bool better = t < bestT - 1e-6f ||
                            (std::fabs(t - bestT) <= 1e-6f && (uint32_t)i < bestHandle);
        if(better) {
            bestT = t;
            bestHandle = (uint32_t)i;
        }
    }
    if(bestHandle == INVALID_HANDLE) return false;

    hit.aHit = true;
    hit.aT = bestT;
    hit.aIndex = bestHandle;
    hit.aPoint.x = origin.x + dir.x * bestT;
    hit.aPoint.y = origin.y + dir.y * bestT;
    hit.aPoint.z = origin.z + dir.z * bestT;
    return true;
}

} // namespace GN3d
