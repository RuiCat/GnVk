// GN_PhysicsJoint.cpp — 关节约束：距离 / 铰链 / 固定（持久数据、世界构建、速度求解与位置修正）。
//
// 做法：
//   * 距离：沿两锚点连线消去相对速度（+ Baumgarte 误差项），位置修正只沿该轴推拉。
//   * 铰链：锚点重合（3 个平移行）+ 两局部轴对齐（2 个垂直轴向的旋转行），绕轴旋转自由。
//   * 固定：锚点重合（3 行）+ 相对朝向锁定（3 个旋转行）。
//   * 静态 / 运动学 / 睡着的动态刚体在关节里按无限质量处理（aMovable=false）。
//   * v1 不做关节 warm starting；两端都不可动的关节不构建。
#include <GN.h>
#include "GN_Physics.h"

#include <cmath>
#include <cfloat>

namespace GN3d {

namespace {

const float kJointBeta = 0.2f; // Baumgarte 位置误差系数（速度求解的偏置项）
const float kJointPositionFactor = 0.2f; // 位置修正系数
const float kJointEpsilon = 1e-9f;
const float kJointMinMass = 1e-12f;

GNg::sVEC3 makeVec3(float x, float y, float z) {
    GNg::sVEC3 v = {x, y, z};
    return v;
}

GNg::sVEC3 addVec(const GNg::sVEC3 &a, const GNg::sVEC3 &b) {
    return makeVec3(a.x + b.x, a.y + b.y, a.z + b.z);
}

GNg::sVEC3 subVec(const GNg::sVEC3 &a, const GNg::sVEC3 &b) {
    return makeVec3(a.x - b.x, a.y - b.y, a.z - b.z);
}

GNg::sVEC3 scaleVec(const GNg::sVEC3 &v, float s) {
    return makeVec3(v.x * s, v.y * s, v.z * s);
}

float dotVec(const GNg::sVEC3 &a, const GNg::sVEC3 &b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

GNg::sVEC3 crossVec(const GNg::sVEC3 &a, const GNg::sVEC3 &b) {
    return makeVec3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}

float lengthVec(const GNg::sVEC3 &v) {
    return sqrtf(dotVec(v, v));
}

float lengthSqVec(const GNg::sVEC3 &v) {
    return dotVec(v, v);
}

// 归一化；长度过小时返回 false（调用方跳过该约束）。
bool normalizeVec(const GNg::sVEC3 &v, GNg::sVEC3 &out) {
    const float lengthSq = lengthSqVec(v);
    if(lengthSq < kJointEpsilon) return false;
    const float inverseLength = 1.0f / sqrtf(lengthSq);
    out = scaleVec(v, inverseLength);
    return true;
}

GNg::sVEC3 rotateVec(const GNg::sQUAT &rotation, const GNg::sVEC3 &v) {
    GNg::sVEC3 out;
    GNg::QuatRotateVec3(&out, &v, &rotation);
    return out;
}

// 世界逆惯量乘向量（列主序 m[col*4+row]）。
GNg::sVEC3 mulInvInertia(const GNg::sMAT44 &invInertia, const GNg::sVEC3 &v) {
    return makeVec3(invInertia.Elem(0, 0) * v.x + invInertia.Elem(1, 0) * v.y +
                        invInertia.Elem(2, 0) * v.z,
                    invInertia.Elem(0, 1) * v.x + invInertia.Elem(1, 1) * v.y +
                        invInertia.Elem(2, 1) * v.z,
                    invInertia.Elem(0, 2) * v.x + invInertia.Elem(1, 2) * v.y +
                        invInertia.Elem(2, 2) * v.z);
}

GNg::sVEC3 worldAnchor(const sRIGID_BODY &body, const GNg::sVEC3 &localAnchor) {
    return addVec(body.aPos, rotateVec(body.aRot, localAnchor));
}

GNg::sVEC3 pointVelocity(const sRIGID_BODY &body, const GNg::sVEC3 &r) {
    return addVec(body.aLinVel, crossVec(body.aAngVel, r));
}

// 轴 axis 上的线性有效质量（含两体角向项）。
float linearEffectiveMass(const sRIGID_BODY *a, bool movableA, const sRIGID_BODY *b, bool movableB,
                          const GNg::sVEC3 &rA, const GNg::sVEC3 &rB, const GNg::sVEC3 &axis) {
    float mass = 0.0f;
    if(movableA) {
        mass += a->aInvMass + dotVec(crossVec(mulInvInertia(a->aInvInertiaWorld, crossVec(rA, axis)), rA),
                                     axis);
    }
    if(movableB) {
        mass += b->aInvMass + dotVec(crossVec(mulInvInertia(b->aInvInertiaWorld, crossVec(rB, axis)), rB),
                                     axis);
    }
    return mass;
}

// 轴 axis 上的角向有效质量。
float angularEffectiveMass(const sRIGID_BODY *a, bool movableA, const sRIGID_BODY *b, bool movableB,
                           const GNg::sVEC3 &axis) {
    float mass = 0.0f;
    if(movableA) mass += dotVec(axis, mulInvInertia(a->aInvInertiaWorld, axis));
    if(movableB) mass += dotVec(axis, mulInvInertia(b->aInvInertiaWorld, axis));
    return mass;
}

void applyImpulse(sRIGID_BODY *body, bool movable, const GNg::sVEC3 &r, const GNg::sVEC3 &impulse) {
    if(!movable) return;
    body->aLinVel = addVec(body->aLinVel, scaleVec(impulse, body->aInvMass));
    body->aAngVel = addVec(body->aAngVel, mulInvInertia(body->aInvInertiaWorld, crossVec(r, impulse)));
}

void applyAngularImpulse(sRIGID_BODY *body, bool movable, const GNg::sVEC3 &impulse) {
    if(!movable) return;
    body->aAngVel = addVec(body->aAngVel, mulInvInertia(body->aInvInertiaWorld, impulse));
}

// 小角度四元数旋转：q' = normalize(q + 0.5 * (Δθ, 0) * q)。
void rotateBody(sRIGID_BODY &body, const GNg::sVEC3 &deltaTheta) {
    if(lengthSqVec(deltaTheta) < kJointEpsilon) return;
    const GNg::sQUAT delta(deltaTheta.x, deltaTheta.y, deltaTheta.z, 0.0f);
    body.aRot = body.aRot + (delta * body.aRot) * 0.5f;
    GNg::QuatNormalize(&body.aRot, &body.aRot);
}

// 相对朝向误差（world 旋转向量，A 的朝向为参考）：qErr = qA^-1 * qB，取最短弧。
GNg::sVEC3 orientationError(const sRIGID_BODY &a, const sRIGID_BODY &b) {
    GNg::sQUAT inverseA;
    GNg::QuatInverse(&inverseA, &a.aRot);
    GNg::sQUAT error = inverseA * b.aRot;
    if(error.w < 0.0f) {
        error.x = -error.x;
        error.y = -error.y;
        error.z = -error.z;
        error.w = -error.w;
    }
    const GNg::sVEC3 local = makeVec3(2.0f * error.x, 2.0f * error.y, 2.0f * error.z);
    return rotateVec(a.aRot, local);
}

// 与 axis 垂直的两个正交单位轴（用于铰链的 2 个角向行）。
void perpendicularAxes(const GNg::sVEC3 &axis, GNg::sVEC3 &outU, GNg::sVEC3 &outV) {
    const GNg::sVEC3 reference = (std::fabs(axis.x) < 0.9f) ? makeVec3(1.0f, 0.0f, 0.0f)
                                                           : makeVec3(0.0f, 1.0f, 0.0f);
    GNg::sVEC3 u;
    normalizeVec(crossVec(reference, axis), u);
    outU = u;
    normalizeVec(crossVec(axis, u), outV);
}

// 铰链轴对齐误差（world 旋转向量，去掉绕轴分量）。
GNg::sVEC3 hingeAlignmentError(const sJOINT_CONSTRAINT &joint) {
    GNg::sVEC3 axisA;
    GNg::sVEC3 axisB;
    if(!normalizeVec(rotateVec(joint.aBodyA->aRot, joint.aLocalAxisA), axisA)) return makeVec3(0.0f, 0.0f, 0.0f);
    if(!normalizeVec(rotateVec(joint.aBodyB->aRot, joint.aLocalAxisB), axisB)) return makeVec3(0.0f, 0.0f, 0.0f);
    GNg::sVEC3 error = crossVec(axisA, axisB);
    return subVec(error, scaleVec(axisA, dotVec(error, axisA)));
}

// 位置伪冲量：把锚点误差沿 axis 分摊给 A(+)/B(-)（含角向旋转），返回是否移动。
void correctAnchorError(sJOINT_CONSTRAINT &joint, const GNg::sVEC3 &axis, float errorMagnitude,
                        float factor) {
    sRIGID_BODY *a = joint.aBodyA;
    sRIGID_BODY *b = joint.aBodyB;
    const GNg::sVEC3 anchorA = worldAnchor(*a, joint.aLocalAnchorA);
    const GNg::sVEC3 anchorB = worldAnchor(*b, joint.aLocalAnchorB);
    const GNg::sVEC3 rA = subVec(anchorA, a->aPos);
    const GNg::sVEC3 rB = subVec(anchorB, b->aPos);
    const float feasibleMass =
        linearEffectiveMass(a, joint.aMovableA, b, joint.aMovableB, rA, rB, axis);
    if(feasibleMass <= kJointMinMass) return;
    const float impulse = factor * errorMagnitude / feasibleMass;
    const GNg::sVEC3 pseudo = scaleVec(axis, impulse);
    if(joint.aMovableA) {
        a->aPos = addVec(a->aPos, scaleVec(pseudo, a->aInvMass));
        rotateBody(*a, mulInvInertia(a->aInvInertiaWorld, crossVec(rA, pseudo)));
    }
    if(joint.aMovableB) {
        b->aPos = subVec(b->aPos, scaleVec(pseudo, b->aInvMass));
        rotateBody(*b, scaleVec(mulInvInertia(b->aInvInertiaWorld, crossVec(rB, pseudo)), -1.0f));
    }
}

// 角向误差修正：A 转 +e*wA、B 转 -e*wB（w 按逆惯量标量分配）。
void correctAngularError(sJOINT_CONSTRAINT &joint, const GNg::sVEC3 &error, float factor) {
    const float inertiaA = joint.aMovableA ? (joint.aBodyA->aInvInertiaWorld.Elem(0, 0) +
                                              joint.aBodyA->aInvInertiaWorld.Elem(1, 1) +
                                              joint.aBodyA->aInvInertiaWorld.Elem(2, 2))
                                           : 0.0f;
    const float inertiaB = joint.aMovableB ? (joint.aBodyB->aInvInertiaWorld.Elem(0, 0) +
                                              joint.aBodyB->aInvInertiaWorld.Elem(1, 1) +
                                              joint.aBodyB->aInvInertiaWorld.Elem(2, 2))
                                           : 0.0f;
    const float total = inertiaA + inertiaB;
    if(total <= kJointMinMass) return;
    const GNg::sVEC3 step = scaleVec(error, factor);
    if(joint.aMovableA) rotateBody(*joint.aBodyA, scaleVec(step, inertiaA / total));
    if(joint.aMovableB) rotateBody(*joint.aBodyB, scaleVec(step, -inertiaB / total));
}

// 3 个世界轴的线性锚点重合行（速度层）。
void solveAnchorVelocity(sJOINT_CONSTRAINT &joint, float dt) {
    sRIGID_BODY *a = joint.aBodyA;
    sRIGID_BODY *b = joint.aBodyB;
    const GNg::sVEC3 anchorA = worldAnchor(*a, joint.aLocalAnchorA);
    const GNg::sVEC3 anchorB = worldAnchor(*b, joint.aLocalAnchorB);
    const GNg::sVEC3 rA = subVec(anchorA, a->aPos);
    const GNg::sVEC3 rB = subVec(anchorB, b->aPos);
    const GNg::sVEC3 error = subVec(anchorB, anchorA);
    const GNg::sVEC3 axes[3] = {makeVec3(1.0f, 0.0f, 0.0f), makeVec3(0.0f, 1.0f, 0.0f),
                                makeVec3(0.0f, 0.0f, 1.0f)};
    for(int i = 0; i < 3; ++i) {
        const GNg::sVEC3 &axis = axes[i];
        const float mass = linearEffectiveMass(a, joint.aMovableA, b, joint.aMovableB, rA, rB, axis);
        if(mass <= kJointMinMass) continue;
        const GNg::sVEC3 relative = subVec(pointVelocity(*b, rB), pointVelocity(*a, rA));
        const float bias = kJointBeta / dt * dotVec(error, axis);
        const float impulse = -(dotVec(relative, axis) + bias) / mass;
        applyImpulse(a, joint.aMovableA, rA, scaleVec(axis, -impulse));
        applyImpulse(b, joint.aMovableB, rB, scaleVec(axis, impulse));
    }
}

// 3 个角向行（固定关节）：锁死相对角速度。
void solveFixedAngularVelocity(sJOINT_CONSTRAINT &joint, float dt) {
    sRIGID_BODY *a = joint.aBodyA;
    sRIGID_BODY *b = joint.aBodyB;
    const GNg::sVEC3 error = orientationError(*a, *b);
    const GNg::sVEC3 axes[3] = {makeVec3(1.0f, 0.0f, 0.0f), makeVec3(0.0f, 1.0f, 0.0f),
                                makeVec3(0.0f, 0.0f, 1.0f)};
    for(int i = 0; i < 3; ++i) {
        const GNg::sVEC3 &axis = axes[i];
        const float mass = angularEffectiveMass(a, joint.aMovableA, b, joint.aMovableB, axis);
        if(mass <= kJointMinMass) continue;
        const GNg::sVEC3 relative = subVec(b->aAngVel, a->aAngVel);
        const float bias = kJointBeta / dt * dotVec(error, axis);
        const float impulse = -(dotVec(relative, axis) + bias) / mass;
        applyAngularImpulse(a, joint.aMovableA, scaleVec(axis, -impulse));
        applyAngularImpulse(b, joint.aMovableB, scaleVec(axis, impulse));
    }
}

// 2 个角向行（铰链）：消去绕轴旋转之外的相对角速度并拉齐两轴。
void solveHingeAngularVelocity(sJOINT_CONSTRAINT &joint, float dt) {
    sRIGID_BODY *a = joint.aBodyA;
    sRIGID_BODY *b = joint.aBodyB;
    const GNg::sVEC3 error = hingeAlignmentError(joint);
    GNg::sVEC3 axisA;
    if(!normalizeVec(rotateVec(a->aRot, joint.aLocalAxisA), axisA)) return;
    GNg::sVEC3 u;
    GNg::sVEC3 v;
    perpendicularAxes(axisA, u, v);
    const GNg::sVEC3 axes[2] = {u, v};
    for(int i = 0; i < 2; ++i) {
        const GNg::sVEC3 &axis = axes[i];
        const float mass = angularEffectiveMass(a, joint.aMovableA, b, joint.aMovableB, axis);
        if(mass <= kJointMinMass) continue;
        const GNg::sVEC3 relative = subVec(b->aAngVel, a->aAngVel);
        const float bias = kJointBeta / dt * dotVec(error, axis);
        const float impulse = -(dotVec(relative, axis) + bias) / mass;
        applyAngularImpulse(a, joint.aMovableA, scaleVec(axis, -impulse));
        applyAngularImpulse(b, joint.aMovableB, scaleVec(axis, impulse));
    }
}

} // namespace

// ═══════════════════════════════════════════════════════════
//  持久关节的增删（cPHYSICS_WORLD 成员）
// ═══════════════════════════════════════════════════════════

uint32_t cPHYSICS_WORLD::AddJoint(const sJOINT_DESC &desc) {
    if(desc.aType != eJOINT_DISTANCE && desc.aType != eJOINT_HINGE && desc.aType != eJOINT_FIXED) {
        return INVALID_HANDLE;
    }
    if(desc.aBodyA == desc.aBodyB) return INVALID_HANDLE;
    const sRIGID_BODY *a = GetBody(desc.aBodyA);
    const sRIGID_BODY *b = GetBody(desc.aBodyB);
    if(a == 0 || b == 0) return INVALID_HANDLE;
    // 两端都不可动（静态/运动学）：关节没有意义，直接拒绝。
    if(a->aType != eBODY_DYNAMIC && b->aType != eBODY_DYNAMIC) return INVALID_HANDLE;

    sJOINT joint;
    joint.aType = desc.aType;
    joint.aBodyA = desc.aBodyA;
    joint.aBodyB = desc.aBodyB;
    joint.aLocalAnchorA = desc.aLocalAnchorA;
    joint.aLocalAnchorB = desc.aLocalAnchorB;
    joint.aLocalAxisA = makeVec3(0.0f, 1.0f, 0.0f);
    joint.aLocalAxisB = makeVec3(0.0f, 1.0f, 0.0f);
    if(desc.aType == eJOINT_HINGE) {
        GNg::sVEC3 axisA;
        GNg::sVEC3 axisB;
        if(normalizeVec(desc.aLocalAxisA, axisA)) joint.aLocalAxisA = axisA;
        if(normalizeVec(desc.aLocalAxisB, axisB)) joint.aLocalAxisB = axisB;
    }
    if(desc.aType == eJOINT_DISTANCE && desc.aDistance >= 0.0f) {
        joint.aRestLength = desc.aDistance;
    } else {
        const GNg::sVEC3 anchorA = worldAnchor(*a, joint.aLocalAnchorA);
        const GNg::sVEC3 anchorB = worldAnchor(*b, joint.aLocalAnchorB);
        joint.aRestLength = lengthVec(subVec(anchorB, anchorA));
    }

    uint32_t handle;
    if(!aFreeJointHandles.empty()) {
        handle = aFreeJointHandles.back();
        aFreeJointHandles.pop_back();
        aJoints[handle] = joint;
    } else {
        handle = (uint32_t)aJoints.size();
        aJoints.push_back(joint);
        aJointUsed.push_back(0);
    }
    aJointUsed[handle] = 1;
    ++aJointCount;
    return handle;
}

bool cPHYSICS_WORLD::RemoveJoint(uint32_t handle) {
    if(handle >= aJoints.size() || !aJointUsed[handle]) return false;
    aJointUsed[handle] = 0;
    aFreeJointHandles.push_back(handle);
    if(aJointCount > 0) --aJointCount;
    return true;
}

void cPHYSICS_WORLD::resetJoints() {
    aJoints.clear();
    aJointUsed.clear();
    aFreeJointHandles.clear();
    aJointConstraints.clear();
    aJointCount = 0;
}

// RemoveBody 时删除引用该刚体的关节（句柄随 RemoveJoint 回收）。
void cPHYSICS_WORLD::removeJointsOfBody(uint32_t handle) {
    for(size_t i = 0; i < aJoints.size(); ++i) {
        if(!aJointUsed[i]) continue;
        if(aJoints[i].aBodyA == handle || aJoints[i].aBodyB == handle) RemoveJoint((uint32_t)i);
    }
}

// 每子步构建运行时关节：解析刚体指针，计算世界锚点/轴与可动性。
void cPHYSICS_WORLD::buildJoints() {
    aJointConstraints.clear();
    if(aJointCount == 0) return;
    aJointConstraints.reserve(aJointCount);
    for(size_t i = 0; i < aJoints.size(); ++i) {
        if(!aJointUsed[i]) continue;
        const sJOINT &joint = aJoints[i];
        sRIGID_BODY *a = GetBody(joint.aBodyA);
        sRIGID_BODY *b = GetBody(joint.aBodyB);
        if(a == 0 || b == 0) continue;
        const bool movableA = (a->aType == eBODY_DYNAMIC) && !a->aSleeping;
        const bool movableB = (b->aType == eBODY_DYNAMIC) && !b->aSleeping;
        if(!movableA && !movableB) continue; // 两端都不可动：无需求解

        sJOINT_CONSTRAINT constraint;
        constraint.aType = joint.aType;
        constraint.aBodyA = a;
        constraint.aBodyB = b;
        constraint.aLocalAnchorA = joint.aLocalAnchorA;
        constraint.aLocalAnchorB = joint.aLocalAnchorB;
        constraint.aLocalAxisA = joint.aLocalAxisA;
        constraint.aLocalAxisB = joint.aLocalAxisB;
        constraint.aRestLength = joint.aRestLength;
        constraint.aMovableA = movableA;
        constraint.aMovableB = movableB;
        aJointConstraints.push_back(constraint);
    }
}

// ═══════════════════════════════════════════════════════════
//  求解器（无世界依赖，可单测）
// ═══════════════════════════════════════════════════════════

void solveJointVelocities(sJOINT_CONSTRAINT *joints, uint32_t count, float dt, uint32_t iterations) {
    if(joints == 0 || count == 0 || iterations == 0 || !(dt > 0.0f)) return;
    for(uint32_t iteration = 0; iteration < iterations; ++iteration) {
        for(uint32_t ci = 0; ci < count; ++ci) {
            sJOINT_CONSTRAINT &joint = joints[ci];
            if(joint.aBodyA == 0 || joint.aBodyB == 0) continue;
            if(!joint.aMovableA && !joint.aMovableB) continue;
            if(joint.aType == eJOINT_DISTANCE) {
                sRIGID_BODY *a = joint.aBodyA;
                sRIGID_BODY *b = joint.aBodyB;
                const GNg::sVEC3 anchorA = worldAnchor(*a, joint.aLocalAnchorA);
                const GNg::sVEC3 anchorB = worldAnchor(*b, joint.aLocalAnchorB);
                GNg::sVEC3 axis;
                if(!normalizeVec(subVec(anchorB, anchorA), axis)) continue;
                const GNg::sVEC3 rA = subVec(anchorA, a->aPos);
                const GNg::sVEC3 rB = subVec(anchorB, b->aPos);
                const float mass =
                    linearEffectiveMass(a, joint.aMovableA, b, joint.aMovableB, rA, rB, axis);
                if(mass <= kJointMinMass) continue;
                const float error = lengthVec(subVec(anchorB, anchorA)) - joint.aRestLength;
                const GNg::sVEC3 relative = subVec(pointVelocity(*b, rB), pointVelocity(*a, rA));
                const float impulse = -(dotVec(relative, axis) + kJointBeta / dt * error) / mass;
                applyImpulse(a, joint.aMovableA, rA, scaleVec(axis, -impulse));
                applyImpulse(b, joint.aMovableB, rB, scaleVec(axis, impulse));
            } else {
                solveAnchorVelocity(joint, dt);
                if(joint.aType == eJOINT_HINGE) {
                    solveHingeAngularVelocity(joint, dt);
                } else {
                    solveFixedAngularVelocity(joint, dt);
                }
            }
        }
    }
}

void solveJointPositions(sJOINT_CONSTRAINT *joints, uint32_t count, uint32_t iterations) {
    if(joints == 0 || count == 0 || iterations == 0) return;
    for(uint32_t iteration = 0; iteration < iterations; ++iteration) {
        for(uint32_t ci = 0; ci < count; ++ci) {
            sJOINT_CONSTRAINT &joint = joints[ci];
            if(joint.aBodyA == 0 || joint.aBodyB == 0) continue;
            if(!joint.aMovableA && !joint.aMovableB) continue;
            if(joint.aType == eJOINT_DISTANCE) {
                const GNg::sVEC3 anchorA = worldAnchor(*joint.aBodyA, joint.aLocalAnchorA);
                const GNg::sVEC3 anchorB = worldAnchor(*joint.aBodyB, joint.aLocalAnchorB);
                const GNg::sVEC3 delta = subVec(anchorB, anchorA);
                GNg::sVEC3 axis;
                if(!normalizeVec(delta, axis)) continue;
                const float error = lengthVec(delta) - joint.aRestLength;
                correctAnchorError(joint, axis, error, kJointPositionFactor);
            } else {
                for(int i = 0; i < 3; ++i) {
                    const GNg::sVEC3 axes[3] = {makeVec3(1.0f, 0.0f, 0.0f), makeVec3(0.0f, 1.0f, 0.0f),
                                                makeVec3(0.0f, 0.0f, 1.0f)};
                    const GNg::sVEC3 anchorA = worldAnchor(*joint.aBodyA, joint.aLocalAnchorA);
                    const GNg::sVEC3 anchorB = worldAnchor(*joint.aBodyB, joint.aLocalAnchorB);
                    const float error = dotVec(subVec(anchorB, anchorA), axes[i]);
                    correctAnchorError(joint, axes[i], error, kJointPositionFactor);
                }
                if(joint.aType == eJOINT_HINGE) {
                    const GNg::sVEC3 error = hingeAlignmentError(joint);
                    correctAngularError(joint, error, kJointPositionFactor);
                } else {
                    const GNg::sVEC3 error = orientationError(*joint.aBodyA, *joint.aBodyB);
                    correctAngularError(joint, error, kJointPositionFactor);
                }
            }
        }
    }
}

} // namespace GN3d
