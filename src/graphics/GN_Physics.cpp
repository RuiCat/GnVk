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

bool RayBody(const GNg::sVEC3 &origin, const GNg::sVEC3 &dir, const sRIGID_BODY &body, float maxT,
             float &outT) {
    if(body.aShape == eSHAPE_SPHERE) {
        return RaySphere(origin, dir, body.aPos, body.aRadius, maxT, outT);
    }
    if(body.aShape == eSHAPE_BOX) {
        return RayBox(origin, dir, body, maxT, outT);
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
    if(body.aShape != eSHAPE_SPHERE && body.aShape != eSHAPE_BOX) return INVALID_HANDLE;
    if(body.aShape == eSHAPE_SPHERE && !(body.aRadius > 0.0f)) return INVALID_HANDLE;
    if(body.aShape == eSHAPE_BOX) {
        if(!(body.aHalfExtent.x > 0.0f) || !(body.aHalfExtent.y > 0.0f) ||
           !(body.aHalfExtent.z > 0.0f)) {
            return INVALID_HANDLE;
        }
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
    aAccumulator = 0.0f;
    aBodyCount = 0;
    aLastContactCount = 0;
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
        SolveVelocities(&aConstraints[0], (uint32_t)aConstraints.size(), dt, kVelocityIterations);
        SolvePositions(&aConstraints[0], (uint32_t)aConstraints.size(), kPositionIterations);
    }
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
        for(int k = 0; k < 4; ++k) {
            constraint.aAccNormal[k] = 0.0f;
            constraint.aAccTangent[k] = 0.0f;
        }
        aConstraints.push_back(constraint);
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
