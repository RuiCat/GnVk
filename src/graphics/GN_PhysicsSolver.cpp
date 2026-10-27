// GN_PhysicsSolver.cpp — 顺序冲量速度求解与位置修正（纯 CPU，无世界/窄相依赖）。
//
// 契约（GN_Physics.h 已冻结，只读）：
//   * 约束里的刚体指针只在本次 Step 内有效；求解器不拥有刚体。
//   * 静态 / 运动学刚体（aInvMass == 0）视为无限质量：速度与位置绝不被修改。
//   * SolveVelocities 只改线速度 / 角速度；SolvePositions 只改位置 / 朝向。
//   * 法线 aNormal 由 A 指向 B；穿透深度 >= 0；调用方负责每步更新世界逆惯量。
//
// 本文件固定的取法（报告与头注释一致）：
//   * 恢复系数：e = max(aRestitutionA, aRestitutionB)；仅当接触点初始法向接近速度
//     低于 -0.5 m/s（kRestThreshold）时施加，目标分离速度 = e * 接近速度。
//   * 摩擦：mu = sqrt(aFrictionA * aFrictionB)，库仑锥按接触点累计冲量钳制
//     |jt| <= mu * jn（jn 为同一接触点的累计法向冲量）。
//   * 位置修正：穿透超出 0.005 m 容差的部分乘 0.2，按有效质量分配，迭代 iterations 次。
//   * warm starting：调用方预填 aAccNormal/aAccTangent 时（warmStart=true），求解器先把这些
//     累计冲量沿当前法线/切线施加到刚体速度，再以该累计值为起点迭代；否则起始清零。
// 隔离编译链（任务指定）只提供 -Isrc/core、-Isrc/graphics 等目录，缺 GN.h 依赖的
// src/file 等目录，无法引入整个 GN.h；这里补齐 GN_2Dmath.h 实际依赖的最小前置定义。
// 整库构建中这些类型/宏分别来自 GN.h 与 GN_Base.h（重复 typedef 在 C++ 中合法，值一致）。
#include "_SysDef/GN_SysDef.h"

typedef unsigned char uint8;
typedef signed char int8;

#ifndef GT_ANGLE_UNIT
#define GT_ANGLE_UNIT 0.01745329252f
#define GT_ANGLE_180 3.1415926536f
#define GT_ANGLE_360 6.2831853072f
#endif

#ifndef GN_FLOAT_MAX
#define GN_FLOAT_MAX 600000000.0f
#endif

#ifndef GN_DotAgToRadian
#define GN_DotAgToRadian(ag) (ag <= -1.0f ? GT_ANGLE_180 : (ag >= 1.0f ? 0.0f : acosf(ag)))
#endif

#include "GN_Physics.h"

#include <cmath>
#include <vector>

namespace GN3d {

namespace {

const float kRestThreshold = 0.5f;
const float kPositionTolerance = 0.005f;
const float kPositionFactor = 0.2f;
const float kMinEffectiveMass = 1e-12f;

inline GNg::sVEC3 makeVec3(float x, float y, float z) {
    GNg::sVEC3 v;
    v.x = x;
    v.y = y;
    v.z = z;
    return v;
}

inline GNg::sVEC3 vec3Add(const GNg::sVEC3 &a, const GNg::sVEC3 &b) {
    return makeVec3(a.x + b.x, a.y + b.y, a.z + b.z);
}

inline GNg::sVEC3 vec3Sub(const GNg::sVEC3 &a, const GNg::sVEC3 &b) {
    return makeVec3(a.x - b.x, a.y - b.y, a.z - b.z);
}

inline GNg::sVEC3 vec3Scale(const GNg::sVEC3 &v, float s) {
    return makeVec3(v.x * s, v.y * s, v.z * s);
}

inline float vec3Dot(const GNg::sVEC3 &a, const GNg::sVEC3 &b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline float vec3LengthSq(const GNg::sVEC3 &v) {
    return vec3Dot(v, v);
}

inline GNg::sVEC3 vec3Cross(const GNg::sVEC3 &a, const GNg::sVEC3 &b) {
    return makeVec3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}

// 世界逆惯量（3x3 部分）乘向量：列主序 m[col*4+row]。
inline GNg::sVEC3 mat44ApplyRotation(const GNg::sMAT44 &m, const GNg::sVEC3 &v) {
    return makeVec3(m.m[0] * v.x + m.m[4] * v.y + m.m[8] * v.z,
                    m.m[1] * v.x + m.m[5] * v.y + m.m[9] * v.z,
                    m.m[2] * v.x + m.m[6] * v.y + m.m[10] * v.z);
}

inline GNg::sQUAT quatNormalized(const GNg::sQUAT &q) {
    const float len = sqrtf(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
    if(len < 1e-8f) return GNg::sQUAT(0.0f, 0.0f, 0.0f, 1.0f);
    const float inv = 1.0f / len;
    return GNg::sQUAT(q.x * inv, q.y * inv, q.z * inv, q.w * inv);
}

// 动态刚体才可被推动；aInvMass == 0 的刚体视为无限质量（契约）。
inline bool bodyMovable(const sRIGID_BODY *body) {
    return body != 0 && body->aType == eBODY_DYNAMIC && body->aInvMass > 0.0f;
}

inline GNg::sVEC3 pointVelocity(const sRIGID_BODY &body, const GNg::sVEC3 &r) {
    return vec3Add(body.aLinVel, vec3Cross(body.aAngVel, r));
}

inline GNg::sVEC3 relativeVelocityAt(const sRIGID_BODY &a, const sRIGID_BODY &b, const GNg::sVEC3 &ra,
                                     const GNg::sVEC3 &rb) {
    return vec3Sub(pointVelocity(b, rb), pointVelocity(a, ra));
}

// 轴 axis 上的角向有效质量项：axis · ((I^-1 (r × axis)) × r)。
inline float angularMassTerm(const GNg::sMAT44 &invInertiaWorld, const GNg::sVEC3 &r,
                             const GNg::sVEC3 &axis) {
    const GNg::sVEC3 lever = vec3Cross(r, axis);
    const GNg::sVEC3 angular = mat44ApplyRotation(invInertiaWorld, lever);
    return vec3Dot(vec3Cross(angular, r), axis);
}

inline float effectiveMass(const sRIGID_BODY *a, const sRIGID_BODY *b, bool movableA, bool movableB,
                           const GNg::sVEC3 &ra, const GNg::sVEC3 &rb, const GNg::sVEC3 &axis) {
    float k = 0.0f;
    if(movableA) k += a->aInvMass + angularMassTerm(a->aInvInertiaWorld, ra, axis);
    if(movableB) k += b->aInvMass + angularMassTerm(b->aInvInertiaWorld, rb, axis);
    return k;
}

// 把冲量 J 施加到 B、-J 施加到 A（只动可移动刚体）。
inline void applyPairImpulse(sRIGID_BODY *a, sRIGID_BODY *b, bool movableA, bool movableB,
                             const GNg::sVEC3 &ra, const GNg::sVEC3 &rb, const GNg::sVEC3 &impulse) {
    if(movableA) {
        a->aLinVel = vec3Sub(a->aLinVel, vec3Scale(impulse, a->aInvMass));
        a->aAngVel = vec3Sub(a->aAngVel, mat44ApplyRotation(a->aInvInertiaWorld, vec3Cross(ra, impulse)));
    }
    if(movableB) {
        b->aLinVel = vec3Add(b->aLinVel, vec3Scale(impulse, b->aInvMass));
        b->aAngVel = vec3Add(b->aAngVel, mat44ApplyRotation(b->aInvInertiaWorld, vec3Cross(rb, impulse)));
    }
}

// 摩擦切线：优先取初始相对速度的切向分量，退化时取与法线正交的任意稳定轴。
inline GNg::sVEC3 tangentFromRelative(const GNg::sVEC3 &normal, const GNg::sVEC3 &relVel) {
    const GNg::sVEC3 tangential = vec3Sub(relVel, vec3Scale(normal, vec3Dot(relVel, normal)));
    if(vec3LengthSq(tangential) > 1e-12f) {
        const float inv = 1.0f / sqrtf(vec3LengthSq(tangential));
        return vec3Scale(tangential, inv);
    }
    const GNg::sVEC3 axis = (fabsf(normal.x) < 0.9f) ? makeVec3(1.0f, 0.0f, 0.0f)
                                                     : makeVec3(0.0f, 1.0f, 0.0f);
    const GNg::sVEC3 tangent = vec3Cross(axis, normal);
    const float inv = 1.0f / sqrtf(vec3LengthSq(tangent));
    return vec3Scale(tangent, inv);
}

// 小角度四元数积分：q' = normalize(q + 0.5 * (dTheta, 0) * q)。
inline void rotateBodyByDelta(sRIGID_BODY &body, const GNg::sVEC3 &deltaTheta) {
    const GNg::sQUAT delta(deltaTheta.x, deltaTheta.y, deltaTheta.z, 0.0f);
    body.aRot = quatNormalized(body.aRot + (delta * body.aRot) * 0.5f);
}

// 单个约束在一次求解内的预计算数据（位置在速度求解期间不变）。
struct sVelocityCache {
    GNg::sVEC3 aRA[4];
    GNg::sVEC3 aRB[4];
    GNg::sVEC3 aTangent[4];
    float aNormalMass[4];
    float aTangentMass[4];
    float aRestBias[4];
    float aFriction;
    uint32_t aPointCount;
};

inline uint32_t clampPointCount(uint32_t count) {
    return (count > 4u) ? 4u : count;
}

} // namespace

void SolveVelocities(sCONTACT_CONSTRAINT *constraints, uint32_t count, float dt, uint32_t iterations,
                     bool warmStart) {
    (void)dt; // 顺序冲量速度求解不直接使用步长（重力/积分由世界负责）。
    if(constraints == 0 || count == 0 || iterations == 0) return;

    std::vector<sVelocityCache> caches(count);
    for(uint32_t ci = 0; ci < count; ++ci) {
        sCONTACT_CONSTRAINT &constraint = constraints[ci];
        sVelocityCache &cache = caches[ci];
        cache.aPointCount = 0;
        cache.aFriction = 0.0f;

        sRIGID_BODY *a = constraint.aBodyA;
        sRIGID_BODY *b = constraint.aBodyB;
        if(a == 0 || b == 0) continue;
        const bool movableA = bodyMovable(a);
        const bool movableB = bodyMovable(b);
        if(!movableA && !movableB) continue; // 静态-静态 / 运动学-运动学：无需求解

        const GNg::sVEC3 normal = constraint.aManifold.aNormal;
        if(vec3LengthSq(normal) < kMinEffectiveMass) continue;
        const uint32_t pointCount = clampPointCount(constraint.aManifold.aCount);
        if(pointCount == 0) continue;

        const float frictionA = (a->aFriction > 0.0f) ? a->aFriction : 0.0f;
        const float frictionB = (b->aFriction > 0.0f) ? b->aFriction : 0.0f;
        const float restitutionA = (a->aRestitution > 0.0f) ? a->aRestitution : 0.0f;
        const float restitutionB = (b->aRestitution > 0.0f) ? b->aRestitution : 0.0f;
        const float restitution = (restitutionA > restitutionB) ? restitutionA : restitutionB;
        cache.aFriction = sqrtf(frictionA * frictionB);

        for(uint32_t pi = 0; pi < pointCount; ++pi) {
            const GNg::sVEC3 &point = constraint.aManifold.aPoints[pi].aPoint;
            cache.aRA[pi] = vec3Sub(point, a->aPos);
            cache.aRB[pi] = vec3Sub(point, b->aPos);
            cache.aNormalMass[pi] =
                effectiveMass(a, b, movableA, movableB, cache.aRA[pi], cache.aRB[pi], normal);
            const GNg::sVEC3 relVel =
                relativeVelocityAt(*a, *b, cache.aRA[pi], cache.aRB[pi]);
            cache.aTangent[pi] = tangentFromRelative(normal, relVel);
            cache.aTangentMass[pi] = effectiveMass(a, b, movableA, movableB, cache.aRA[pi],
                                                   cache.aRB[pi], cache.aTangent[pi]);
            const float normalSpeed = vec3Dot(relVel, normal);
            cache.aRestBias[pi] = (normalSpeed < -kRestThreshold) ? (-restitution * normalSpeed) : 0.0f;
            if(!warmStart) {
                constraint.aAccNormal[pi] = 0.0f;
                constraint.aAccTangent[pi] = 0.0f;
            }
        }
        cache.aPointCount = pointCount;
    }

    // warm start：沿本步的法线/切线重放上一步累计冲量（重放后迭代从该累计值继续）。
    if(warmStart) {
        for(uint32_t ci = 0; ci < count; ++ci) {
            sCONTACT_CONSTRAINT &constraint = constraints[ci];
            sVelocityCache &cache = caches[ci];
            if(cache.aPointCount == 0) continue;
            sRIGID_BODY *a = constraint.aBodyA;
            sRIGID_BODY *b = constraint.aBodyB;
            const bool movableA = bodyMovable(a);
            const bool movableB = bodyMovable(b);
            if(!movableA && !movableB) continue;
            const GNg::sVEC3 normal = constraint.aManifold.aNormal;
            for(uint32_t pi = 0; pi < cache.aPointCount; ++pi) {
                const float accNormal = constraint.aAccNormal[pi];
                const float accTangent = constraint.aAccTangent[pi];
                if(accNormal == 0.0f && accTangent == 0.0f) continue;
                const GNg::sVEC3 impulse = vec3Add(vec3Scale(normal, accNormal),
                                                   vec3Scale(cache.aTangent[pi], accTangent));
                applyPairImpulse(a, b, movableA, movableB, cache.aRA[pi], cache.aRB[pi], impulse);
            }
        }
    }

    for(uint32_t it = 0; it < iterations; ++it) {
        for(uint32_t ci = 0; ci < count; ++ci) {
            sCONTACT_CONSTRAINT &constraint = constraints[ci];
            sVelocityCache &cache = caches[ci];
            if(cache.aPointCount == 0) continue;
            sRIGID_BODY *a = constraint.aBodyA;
            sRIGID_BODY *b = constraint.aBodyB;
            const bool movableA = bodyMovable(a);
            const bool movableB = bodyMovable(b);
            const GNg::sVEC3 normal = constraint.aManifold.aNormal;

            for(uint32_t pi = 0; pi < cache.aPointCount; ++pi) {
                const float normalMass = cache.aNormalMass[pi];
                if(normalMass <= kMinEffectiveMass) continue;

                const GNg::sVEC3 &ra = cache.aRA[pi];
                const GNg::sVEC3 &rb = cache.aRB[pi];

                GNg::sVEC3 relVel = relativeVelocityAt(*a, *b, ra, rb);
                const float normalSpeed = vec3Dot(relVel, normal);
                const float normalImpulseDelta = (cache.aRestBias[pi] - normalSpeed) / normalMass;
                const float oldNormal = constraint.aAccNormal[pi];
                float newNormal = oldNormal + normalImpulseDelta;
                if(newNormal < 0.0f) newNormal = 0.0f;
                constraint.aAccNormal[pi] = newNormal;
                const float appliedNormal = newNormal - oldNormal;
                if(appliedNormal != 0.0f) {
                    applyPairImpulse(a, b, movableA, movableB, ra, rb,
                                     vec3Scale(normal, appliedNormal));
                }

                const float tangentMass = cache.aTangentMass[pi];
                if(tangentMass <= kMinEffectiveMass) continue;
                const GNg::sVEC3 &tangent = cache.aTangent[pi];
                relVel = relativeVelocityAt(*a, *b, ra, rb);
                const float tangentSpeed = vec3Dot(relVel, tangent);
                const float tangentImpulseDelta = -tangentSpeed / tangentMass;
                const float maxFriction = cache.aFriction * constraint.aAccNormal[pi];
                const float oldTangent = constraint.aAccTangent[pi];
                float newTangent = oldTangent + tangentImpulseDelta;
                if(newTangent > maxFriction) newTangent = maxFriction;
                if(newTangent < -maxFriction) newTangent = -maxFriction;
                constraint.aAccTangent[pi] = newTangent;
                const float appliedTangent = newTangent - oldTangent;
                if(appliedTangent != 0.0f) {
                    applyPairImpulse(a, b, movableA, movableB, ra, rb,
                                     vec3Scale(tangent, appliedTangent));
                }
            }
        }
    }
}

void SolvePositions(sCONTACT_CONSTRAINT *constraints, uint32_t count, uint32_t iterations) {
    if(constraints == 0 || count == 0 || iterations == 0) return;

    for(uint32_t ci = 0; ci < count; ++ci) {
        sCONTACT_CONSTRAINT &constraint = constraints[ci];
        sRIGID_BODY *a = constraint.aBodyA;
        sRIGID_BODY *b = constraint.aBodyB;
        if(a == 0 || b == 0) continue;
        const bool movableA = bodyMovable(a);
        const bool movableB = bodyMovable(b);
        if(!movableA && !movableB) continue;

        const GNg::sVEC3 normal = constraint.aManifold.aNormal;
        if(vec3LengthSq(normal) < kMinEffectiveMass) continue;
        const uint32_t pointCount = clampPointCount(constraint.aManifold.aCount);
        if(pointCount == 0) continue;
        // 位置修正总量 ≈ kPositionFactor × 穿透深度：按「接触点数 × 迭代次数」均摊。
        // 否则同一个穿透量被每个点、每次迭代重复推开（4 点 × 4 次 ≈ 3.2 倍），堆叠会被顶散。
        const float spread = 1.0f / ((float)pointCount * (float)iterations);

        for(uint32_t it = 0; it < iterations; ++it) {
            for(uint32_t pi = 0; pi < pointCount; ++pi) {
                const sCONTACT_POINT &contact = constraint.aManifold.aPoints[pi];
                const float correction = contact.aPenetration - kPositionTolerance;
                if(correction <= 0.0f) continue;

                const GNg::sVEC3 ra = vec3Sub(contact.aPoint, a->aPos);
                const GNg::sVEC3 rb = vec3Sub(contact.aPoint, b->aPos);
                const float mass = effectiveMass(a, b, movableA, movableB, ra, rb, normal);
                if(mass <= kMinEffectiveMass) continue;
                const float positionImpulse = kPositionFactor * correction / mass * spread;

                if(movableA) {
                    a->aPos = vec3Sub(a->aPos, vec3Scale(normal, positionImpulse * a->aInvMass));
                    const GNg::sVEC3 deltaTheta =
                        mat44ApplyRotation(a->aInvInertiaWorld, vec3Cross(ra, vec3Scale(normal, -positionImpulse)));
                    rotateBodyByDelta(*a, deltaTheta);
                }
                if(movableB) {
                    b->aPos = vec3Add(b->aPos, vec3Scale(normal, positionImpulse * b->aInvMass));
                    const GNg::sVEC3 deltaTheta =
                        mat44ApplyRotation(b->aInvInertiaWorld, vec3Cross(rb, vec3Scale(normal, positionImpulse)));
                    rotateBodyByDelta(*b, deltaTheta);
                }
            }
        }
    }
}

} // namespace GN3d
