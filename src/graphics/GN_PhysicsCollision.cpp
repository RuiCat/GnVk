// GN_PhysicsCollision.cpp — 窄相碰撞与刚体状态辅助：球/盒初始化、惯量、世界 AABB/逆惯量，
//   以及球-球、球-盒、盒-盒（OBB SAT + 面裁剪流形）接触流形；法线一律 A→B、穿透深度 >= 0。
//
// 契约与语义见只读的 GN_Physics.h；本文件不依赖世界 / 宽相 / 求解器，可独立单测。
// 退化策略：零半径、零/负半尺寸、非有限参数、同心球一律返回 false（不产生无意义法线）；
//   轴对齐盒的平行面是正常接触（必须返回流形），只有"完全重合的盒"依赖枚举顺序取轴、仍然退化为深度最小的轴。
#include <GN.h>
#include "GN_Physics.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdint>

namespace GN3d {

namespace {

const float kContactSlop = 1e-6f; // 重叠 >= -slop 视为接触（避免浮点误差吞掉刚好相切）
const float kAxisEpsilon = 1e-6f; // 叉积轴长度下限：低于它视为平行轴，不参与 SAT
const float kCoincidentEpsilon = 1e-12f; // 球心重合判定（距离平方）

inline GNg::sVEC3 MakeVec3(float x, float y, float z) {
    GNg::sVEC3 v = {x, y, z};
    return v;
}

inline GNg::sVEC3 Add(const GNg::sVEC3 &a, const GNg::sVEC3 &b) {
    return MakeVec3(a.x + b.x, a.y + b.y, a.z + b.z);
}

inline GNg::sVEC3 Sub(const GNg::sVEC3 &a, const GNg::sVEC3 &b) {
    return MakeVec3(a.x - b.x, a.y - b.y, a.z - b.z);
}

inline GNg::sVEC3 Scale(const GNg::sVEC3 &v, float s) {
    return MakeVec3(v.x * s, v.y * s, v.z * s);
}

inline GNg::sVEC3 Neg(const GNg::sVEC3 &v) {
    return MakeVec3(-v.x, -v.y, -v.z);
}

inline float Dot(const GNg::sVEC3 &a, const GNg::sVEC3 &b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline GNg::sVEC3 Cross(const GNg::sVEC3 &a, const GNg::sVEC3 &b) {
    return MakeVec3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}

inline float LengthSq(const GNg::sVEC3 &v) {
    return Dot(v, v);
}

inline float Clamp(float value, float low, float high) {
    return value < low ? low : (value > high ? high : value);
}

inline bool IsFiniteVec3(const GNg::sVEC3 &v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

inline float SafeHalfExtent(float value) {
    return (std::isfinite(value) && value > 0.0f) ? value : 0.0f;
}

inline void SetZeroMatrix(GNg::sMAT44 &matrix) {
    matrix = GNg::sMAT44(false);
}

inline void SetDiagonalMatrix(GNg::sMAT44 &matrix, float x, float y, float z) {
    SetZeroMatrix(matrix);
    matrix.m[0] = x;
    matrix.m[5] = y;
    matrix.m[10] = z;
    matrix.m[15] = 1.0f;
}

// 盒的三条世界局部轴（列向量），由四元数旋转矩阵取出。
struct sBoxAxes {
    GNg::sVEC3 aAxis[3];
};

void GetBoxAxes(const sRIGID_BODY &box, sBoxAxes &out) {
    GNg::sMAT44 rotation;
    GNg::QuatToMatrix44(&rotation, &box.aRot);
    for(int i = 0; i < 3; ++i) {
        out.aAxis[i] = MakeVec3(rotation.Elem(i, 0), rotation.Elem(i, 1), rotation.Elem(i, 2));
    }
}

void GetHalfExtents(const sRIGID_BODY &box, float half[3]) {
    half[0] = std::fabs(box.aHalfExtent.x);
    half[1] = std::fabs(box.aHalfExtent.y);
    half[2] = std::fabs(box.aHalfExtent.z);
}

bool IsCollidable(const sRIGID_BODY &body) {
    if(!IsFiniteVec3(body.aPos)) return false;
    if(!std::isfinite(body.aRot.x) || !std::isfinite(body.aRot.y) || !std::isfinite(body.aRot.z) ||
       !std::isfinite(body.aRot.w)) {
        return false;
    }
    if(body.aShape == eSHAPE_SPHERE) return std::isfinite(body.aRadius) && body.aRadius > 0.0f;
    if(body.aShape == eSHAPE_BOX) {
        const GNg::sVEC3 &half = body.aHalfExtent;
        return std::isfinite(half.x) && half.x > 0.0f && std::isfinite(half.y) && half.y > 0.0f &&
               std::isfinite(half.z) && half.z > 0.0f;
    }
    return false;
}

// ═══════════════════════════════════════════════════════════
//  窄相实现（static）
// ═══════════════════════════════════════════════════════════

bool CollideSphereSphere(const sRIGID_BODY &a, const sRIGID_BODY &b, sCONTACT_MANIFOLD &out) {
    const GNg::sVEC3 delta = Sub(b.aPos, a.aPos);
    const float distanceSq = LengthSq(delta);
    const float radiusSum = a.aRadius + b.aRadius;
    if(distanceSq <= kCoincidentEpsilon) return false; // 同心：法线不确定，按退化处理
    const float distance = sqrtf(distanceSq);
    if(distance > radiusSum + kContactSlop) return false;
    const GNg::sVEC3 normal = Scale(delta, 1.0f / distance); // A → B
    float penetration = radiusSum - distance;
    if(!(penetration >= 0.0f)) penetration = 0.0f;
    out.aNormal = normal;
    out.aCount = 1;
    out.aPoints[0].aPoint = Add(a.aPos, Scale(normal, a.aRadius - penetration * 0.5f));
    out.aPoints[0].aPenetration = penetration;
    return true;
}

bool CollideSphereBox(const sRIGID_BODY &sphere, const sRIGID_BODY &box, sCONTACT_MANIFOLD &out) {
    sBoxAxes axes;
    GetBoxAxes(box, axes);
    float half[3];
    GetHalfExtents(box, half);
    const GNg::sVEC3 relative = Sub(sphere.aPos, box.aPos);
    float local[3] = {Dot(relative, axes.aAxis[0]), Dot(relative, axes.aAxis[1]),
                      Dot(relative, axes.aAxis[2])};
    GNg::sVEC3 boxToSphere = MakeVec3(0.0f, 0.0f, 0.0f);
    GNg::sVEC3 closest = box.aPos;
    for(int i = 0; i < 3; ++i) {
        const float clamped = Clamp(local[i], -half[i], half[i]);
        boxToSphere = Add(boxToSphere, Scale(axes.aAxis[i], local[i] - clamped));
        closest = Add(closest, Scale(axes.aAxis[i], clamped));
    }

    GNg::sVEC3 normal;
    float penetration;
    GNg::sVEC3 point;
    const float distanceSq = LengthSq(boxToSphere);
    if(distanceSq > kCoincidentEpsilon) {
        // 球心在盒外：法线由球心指向盒上最近点（A 球 → B 盒）。
        const float distance = sqrtf(distanceSq);
        if(distance > sphere.aRadius + kContactSlop) return false;
        normal = Neg(Scale(boxToSphere, 1.0f / distance));
        penetration = sphere.aRadius - distance;
        point = closest;
    } else {
        // 球心在盒内/表面上：取最近面，把球沿该面法线推出（法线取推出方向的反向，仍为 A→B）。
        int faceAxis = 0;
        float minFace = half[0] - std::fabs(local[0]);
        for(int i = 1; i < 3; ++i) {
            const float face = half[i] - std::fabs(local[i]);
            if(face < minFace) {
                minFace = face;
                faceAxis = i;
            }
        }
        const float outwardSign = local[faceAxis] >= 0.0f ? 1.0f : -1.0f;
        normal = Neg(Scale(axes.aAxis[faceAxis], outwardSign));
        penetration = sphere.aRadius + minFace;
        point = box.aPos;
        for(int i = 0; i < 3; ++i) {
            const float value = (i == faceAxis) ? outwardSign * half[i] : local[i];
            point = Add(point, Scale(axes.aAxis[i], value));
        }
    }
    if(!(penetration >= 0.0f)) penetration = 0.0f;
    out.aNormal = normal;
    out.aCount = 1;
    out.aPoints[0].aPoint = point;
    out.aPoints[0].aPenetration = penetration;
    return true;
}

// 盒在单位轴 axis 上的投影半径。
float ProjectRadius(const sRIGID_BODY &box, const sBoxAxes &axes, const GNg::sVEC3 &axis) {
    float half[3];
    GetHalfExtents(box, half);
    return std::fabs(Dot(axis, axes.aAxis[0])) * half[0] + std::fabs(Dot(axis, axes.aAxis[1])) * half[1] +
           std::fabs(Dot(axis, axes.aAxis[2])) * half[2];
}

// 保留 dot(planeNormal, p) <= planeOffset 的部分（Sutherland-Hodgman 单平面裁剪）。
int ClipPolygonToPlane(const GNg::sVEC3 *input, int inputCount, const GNg::sVEC3 &planeNormal,
                       float planeOffset, GNg::sVEC3 *output) {
    if(inputCount <= 0) return 0;
    int outputCount = 0;
    for(int i = 0; i < inputCount; ++i) {
        const GNg::sVEC3 &current = input[i];
        const GNg::sVEC3 &previous = input[(i + inputCount - 1) % inputCount];
        const float currentDistance = Dot(planeNormal, current) - planeOffset;
        const float previousDistance = Dot(planeNormal, previous) - planeOffset;
        const bool currentInside = currentDistance <= 0.0f;
        const bool previousInside = previousDistance <= 0.0f;
        if(currentInside) {
            if(!previousInside) {
                const float t = previousDistance / (previousDistance - currentDistance);
                output[outputCount++] = Add(previous, Scale(Sub(current, previous), t));
            }
            output[outputCount++] = current;
        } else if(previousInside) {
            const float t = previousDistance / (previousDistance - currentDistance);
            output[outputCount++] = Add(previous, Scale(Sub(current, previous), t));
        }
    }
    return outputCount;
}

struct sManifoldPoint {
    GNg::sVEC3 aPoint;
    float aPenetration;
};

// 面-面流形：取入射面上与参考面相对的那张面，裁掉参考面侧面以外的部分，最多 4 个接触点。
uint32_t BuildFaceManifold(const sRIGID_BODY &reference, const sBoxAxes &referenceAxes,
                           const sRIGID_BODY &incident, const sBoxAxes &incidentAxes, int referenceAxis,
                           float referenceSign, const GNg::sVEC3 &referenceNormal,
                           const GNg::sVEC3 &manifoldNormal, sCONTACT_MANIFOLD &out) {
    float referenceHalf[3];
    float incidentHalf[3];
    GetHalfExtents(reference, referenceHalf);
    GetHalfExtents(incident, incidentHalf);
    const int lateralJ = (referenceAxis + 1) % 3;
    const int lateralK = (referenceAxis + 2) % 3;
    const GNg::sVEC3 faceCenter =
        Add(reference.aPos, Scale(referenceAxes.aAxis[referenceAxis], referenceSign * referenceHalf[referenceAxis]));

    int incidentAxis = 0;
    float bestDot = Dot(incidentAxes.aAxis[0], referenceNormal);
    for(int i = 1; i < 3; ++i) {
        const float dot = Dot(incidentAxes.aAxis[i], referenceNormal);
        if(dot > bestDot) {
            bestDot = dot;
            incidentAxis = i;
        }
    }
    const int incidentJ = (incidentAxis + 1) % 3;
    const int incidentK = (incidentAxis + 2) % 3;
    const GNg::sVEC3 incidentCenter =
        Sub(incident.aPos, Scale(incidentAxes.aAxis[incidentAxis], incidentHalf[incidentAxis]));

    GNg::sVEC3 polygon[10];
    const GNg::sVEC3 offsetJ = Scale(incidentAxes.aAxis[incidentJ], incidentHalf[incidentJ]);
    const GNg::sVEC3 offsetK = Scale(incidentAxes.aAxis[incidentK], incidentHalf[incidentK]);
    polygon[0] = Add(Add(incidentCenter, offsetJ), offsetK);
    polygon[1] = Add(Sub(incidentCenter, offsetJ), offsetK);
    polygon[2] = Sub(Sub(incidentCenter, offsetJ), offsetK);
    polygon[3] = Sub(Add(incidentCenter, offsetJ), offsetK);

    GNg::sVEC3 buffer[10];
    const GNg::sVEC3 axisJ = referenceAxes.aAxis[lateralJ];
    const GNg::sVEC3 axisK = referenceAxes.aAxis[lateralK];
    int count = ClipPolygonToPlane(polygon, 4, axisJ, Dot(axisJ, reference.aPos) + referenceHalf[lateralJ], buffer);
    count = ClipPolygonToPlane(buffer, count, Neg(axisJ), Dot(Neg(axisJ), reference.aPos) + referenceHalf[lateralJ],
                               polygon);
    count = ClipPolygonToPlane(polygon, count, axisK, Dot(axisK, reference.aPos) + referenceHalf[lateralK], buffer);
    count = ClipPolygonToPlane(buffer, count, Neg(axisK), Dot(Neg(axisK), reference.aPos) + referenceHalf[lateralK],
                               polygon);

    sManifoldPoint points[10];
    for(int i = 0; i < count; ++i) {
        float penetration = Dot(Sub(faceCenter, polygon[i]), referenceNormal);
        if(!(penetration >= 0.0f)) penetration = 0.0f;
        points[i].aPoint = polygon[i];
        points[i].aPenetration = penetration;
    }
    if(count == 0) { // 数值退化时保留一个保底接触点，避免"报告相交却没有点"
        points[0].aPoint = incidentCenter;
        points[0].aPenetration = 0.0f;
        count = 1;
    }
    std::sort(points, points + count, [](const sManifoldPoint &left, const sManifoldPoint &right) {
        return left.aPenetration > right.aPenetration;
    });
    out.aNormal = manifoldNormal;
    out.aCount = count > 4 ? 4u : (uint32_t)count;
    for(uint32_t i = 0; i < out.aCount; ++i) {
        out.aPoints[i].aPoint = points[i].aPoint;
        out.aPoints[i].aPenetration = points[i].aPenetration;
    }
    return out.aCount > 0;
}

struct sSatCandidate {
    float aDepth;
    GNg::sVEC3 aNormal; // A → B
    int aKind; // 0 = A 的面轴，1 = B 的面轴，2 = 棱-棱
    int aAxis; // 面轴索引 / 棱-棱时 A 的棱方向轴索引
    int aSecondaryAxis; // 棱-棱时 B 的棱方向轴索引
};

bool CollideBoxBox(const sRIGID_BODY &a, const sRIGID_BODY &b, sCONTACT_MANIFOLD &out) {
    sBoxAxes axesA;
    sBoxAxes axesB;
    GetBoxAxes(a, axesA);
    GetBoxAxes(b, axesB);
    const GNg::sVEC3 delta = Sub(b.aPos, a.aPos);

    sSatCandidate best;
    best.aDepth = FLT_MAX;
    best.aNormal = MakeVec3(0.0f, 0.0f, 0.0f);
    best.aKind = -1;
    best.aAxis = 0;
    best.aSecondaryAxis = 0;
    bool found = false;

    // 15 根候选轴：A 的 3 面轴、B 的 3 面轴、9 条棱叉积轴；返回 false 表示已找到分离轴。
    auto considerAxis = [&](const GNg::sVEC3 &rawAxis, int kind, int axisIndex,
                            int secondaryAxis) -> bool {
        const float lengthSq = LengthSq(rawAxis);
        if(!(lengthSq > kAxisEpsilon * kAxisEpsilon)) return true; // 平行棱：叉积轴退化，跳过
        const GNg::sVEC3 axis = Scale(rawAxis, 1.0f / sqrtf(lengthSq));
        const float radiusA = ProjectRadius(a, axesA, axis);
        const float radiusB = ProjectRadius(b, axesB, axis);
        const float centerDistance = Dot(axis, delta);
        const float depth = radiusA + radiusB - std::fabs(centerDistance);
        if(depth < -kContactSlop) return false;
        if(!found || depth < best.aDepth - kAxisEpsilon) {
            found = true;
            best.aDepth = depth;
            best.aNormal = centerDistance >= 0.0f ? axis : Neg(axis);
            best.aKind = kind;
            best.aAxis = axisIndex;
            best.aSecondaryAxis = secondaryAxis;
        }
        return true;
    };

    for(int i = 0; i < 3; ++i) {
        if(!considerAxis(axesA.aAxis[i], 0, i, 0)) return false;
    }
    for(int i = 0; i < 3; ++i) {
        if(!considerAxis(axesB.aAxis[i], 1, i, 0)) return false;
    }
    for(int i = 0; i < 3; ++i) {
        for(int j = 0; j < 3; ++j) {
            if(!considerAxis(Cross(axesA.aAxis[i], axesB.aAxis[j]), 2, i, j)) return false;
        }
    }
    if(!found) return false;

    if(best.aKind == 0) {
        const float sign = Dot(best.aNormal, axesA.aAxis[best.aAxis]) >= 0.0f ? 1.0f : -1.0f;
        return BuildFaceManifold(a, axesA, b, axesB, best.aAxis, sign, best.aNormal, best.aNormal, out);
    }
    if(best.aKind == 1) {
        const GNg::sVEC3 referenceNormal = Neg(best.aNormal);
        const float sign = Dot(referenceNormal, axesB.aAxis[best.aAxis]) >= 0.0f ? 1.0f : -1.0f;
        return BuildFaceManifold(b, axesB, a, axesA, best.aAxis, sign, referenceNormal, best.aNormal, out);
    }
    // 棱-棱：两条支撑棱是有限线段，取两线段最近点连线的中点作为世界接触点。
    out.aNormal = best.aNormal;
    out.aCount = 1;
    float halfA[3];
    float halfB[3];
    GetHalfExtents(a, halfA);
    GetHalfExtents(b, halfB);
    const GNg::sVEC3 directionA = axesA.aAxis[best.aAxis];
    const GNg::sVEC3 directionB = axesB.aAxis[best.aSecondaryAxis];
    GNg::sVEC3 edgeA = a.aPos;
    GNg::sVEC3 edgeB = b.aPos;
    for(int i = 0; i < 3; ++i) {
        if(i != best.aAxis) {
            edgeA = Add(edgeA, Scale(axesA.aAxis[i], Dot(best.aNormal, axesA.aAxis[i]) >= 0.0f ? halfA[i] : -halfA[i]));
        }
        if(i != best.aSecondaryAxis) {
            edgeB = Add(edgeB, Scale(axesB.aAxis[i], Dot(best.aNormal, axesB.aAxis[i]) <= 0.0f ? halfB[i] : -halfB[i]));
        }
    }
    // 线段最近点：先按无限直线求参数，再各自夹到 [-half, half]，两次交替投影收敛。
    const GNg::sVEC3 between = Sub(edgeA, edgeB);
    const float alignment = Dot(directionA, directionB);
    float offsetA = 0.0f;
    float offsetB = 0.0f;
    const float denominator = 1.0f - alignment * alignment;
    if(denominator > kAxisEpsilon) {
        offsetA = (alignment * Dot(directionB, between) - Dot(directionA, between)) / denominator;
        offsetB = (Dot(directionB, between) - alignment * Dot(directionA, between)) / denominator;
    }
    offsetA = Clamp(offsetA, -halfA[best.aAxis], halfA[best.aAxis]);
    offsetB = Clamp(offsetB, -halfB[best.aSecondaryAxis], halfB[best.aSecondaryAxis]);
    for(int i = 0; i < 2; ++i) {
        offsetB = Dot(directionB, Sub(Add(edgeA, Scale(directionA, offsetA)), edgeB));
        offsetB = Clamp(offsetB, -halfB[best.aSecondaryAxis], halfB[best.aSecondaryAxis]);
        offsetA = Dot(directionA, Sub(Add(edgeB, Scale(directionB, offsetB)), edgeA));
        offsetA = Clamp(offsetA, -halfA[best.aAxis], halfA[best.aAxis]);
    }
    out.aPoints[0].aPoint = Scale(Add(Add(edgeA, Scale(directionA, offsetA)), Add(edgeB, Scale(directionB, offsetB))),
                                  0.5f);
    out.aPoints[0].aPenetration = best.aDepth > 0.0f ? best.aDepth : 0.0f;
    return true;
}

} // namespace

// ═══════════════════════════════════════════════════════════
//  刚体辅助
// ═══════════════════════════════════════════════════════════

sRIGID_BODY MakeSphereBody(eBODY_TYPE type, const GNg::sVEC3 &pos, float radius, float mass) {
    sRIGID_BODY body;
    body.aType = type;
    body.aShape = eSHAPE_SPHERE;
    body.aMass = mass;
    body.aInvMass = 0.0f;
    body.aPos = pos;
    body.aRot = GNg::sQUAT(); // 单位四元数
    body.aLinVel = MakeVec3(0.0f, 0.0f, 0.0f);
    body.aAngVel = MakeVec3(0.0f, 0.0f, 0.0f);
    SetZeroMatrix(body.aInvInertiaLocal);
    SetZeroMatrix(body.aInvInertiaWorld);
    body.aFriction = 0.5f;
    body.aRestitution = 0.0f;
    body.aLinearDamping = 0.02f;
    body.aAngularDamping = 0.05f;
    body.aRadius = radius;
    body.aHalfExtent = MakeVec3(0.0f, 0.0f, 0.0f);
    body.aSleeping = false;
    body.aSleepTime = 0.0f;
    body.aWorldAABB = sAABB();
    BodyUpdateInertia(body);
    BodyUpdateWorldState(body);
    return body;
}

sRIGID_BODY MakeBoxBody(eBODY_TYPE type, const GNg::sVEC3 &pos, const GNg::sVEC3 &halfExtent, float mass) {
    sRIGID_BODY body;
    body.aType = type;
    body.aShape = eSHAPE_BOX;
    body.aMass = mass;
    body.aInvMass = 0.0f;
    body.aPos = pos;
    body.aRot = GNg::sQUAT(); // 单位四元数
    body.aLinVel = MakeVec3(0.0f, 0.0f, 0.0f);
    body.aAngVel = MakeVec3(0.0f, 0.0f, 0.0f);
    SetZeroMatrix(body.aInvInertiaLocal);
    SetZeroMatrix(body.aInvInertiaWorld);
    body.aFriction = 0.5f;
    body.aRestitution = 0.0f;
    body.aLinearDamping = 0.02f;
    body.aAngularDamping = 0.05f;
    body.aRadius = 0.0f;
    body.aHalfExtent = halfExtent;
    body.aSleeping = false;
    body.aSleepTime = 0.0f;
    body.aWorldAABB = sAABB();
    BodyUpdateInertia(body);
    BodyUpdateWorldState(body);
    return body;
}

void BodyUpdateInertia(sRIGID_BODY &body) {
    body.aInvMass = 0.0f;
    SetZeroMatrix(body.aInvInertiaLocal);
    if(body.aType != eBODY_DYNAMIC) return; // 静态/运动学：无限质量
    const float mass = body.aMass;
    if(!std::isfinite(mass) || !(mass > 0.0f)) return; // 非法质量：降级为无限质量

    float invIx;
    float invIy;
    float invIz;
    if(body.aShape == eSHAPE_SPHERE) {
        const float radius = body.aRadius;
        if(!std::isfinite(radius) || !(radius > 0.0f)) return;
        invIx = invIy = invIz = 1.0f / (0.4f * mass * radius * radius); // I = 2/5 m r²
    } else if(body.aShape == eSHAPE_BOX) {
        const GNg::sVEC3 &half = body.aHalfExtent;
        if(!std::isfinite(half.x) || !std::isfinite(half.y) || !std::isfinite(half.z) || !(half.x > 0.0f) ||
           !(half.y > 0.0f) || !(half.z > 0.0f)) {
            return;
        }
        const float massThird = mass / 3.0f;
        invIx = 1.0f / (massThird * (half.y * half.y + half.z * half.z));
        invIy = 1.0f / (massThird * (half.x * half.x + half.z * half.z));
        invIz = 1.0f / (massThird * (half.x * half.x + half.y * half.y));
    } else {
        return;
    }
    if(!std::isfinite(invIx) || !std::isfinite(invIy) || !std::isfinite(invIz)) return; // 参数极端：仍按无限质量
    body.aInvMass = 1.0f / mass;
    SetDiagonalMatrix(body.aInvInertiaLocal, invIx, invIy, invIz);
}

void BodyUpdateWorldState(sRIGID_BODY &body) {
    if(body.aShape == eSHAPE_SPHERE) {
        const float radius = SafeHalfExtent(body.aRadius);
        body.aWorldAABB.aMin = MakeVec3(body.aPos.x - radius, body.aPos.y - radius, body.aPos.z - radius);
        body.aWorldAABB.aMax = MakeVec3(body.aPos.x + radius, body.aPos.y + radius, body.aPos.z + radius);
    } else if(body.aShape == eSHAPE_BOX) {
        sBoxAxes axes;
        GetBoxAxes(body, axes);
        const float halfX = SafeHalfExtent(body.aHalfExtent.x);
        const float halfY = SafeHalfExtent(body.aHalfExtent.y);
        const float halfZ = SafeHalfExtent(body.aHalfExtent.z);
        // OBB 的世界 AABB 轴向半径 = |R| * halfExtent（不能只取中心 ± 半尺寸）。
        const float extentX = std::fabs(axes.aAxis[0].x) * halfX + std::fabs(axes.aAxis[1].x) * halfY +
                              std::fabs(axes.aAxis[2].x) * halfZ;
        const float extentY = std::fabs(axes.aAxis[0].y) * halfX + std::fabs(axes.aAxis[1].y) * halfY +
                              std::fabs(axes.aAxis[2].y) * halfZ;
        const float extentZ = std::fabs(axes.aAxis[0].z) * halfX + std::fabs(axes.aAxis[1].z) * halfY +
                              std::fabs(axes.aAxis[2].z) * halfZ;
        body.aWorldAABB.aMin = MakeVec3(body.aPos.x - extentX, body.aPos.y - extentY, body.aPos.z - extentZ);
        body.aWorldAABB.aMax = MakeVec3(body.aPos.x + extentX, body.aPos.y + extentY, body.aPos.z + extentZ);
    } else {
        body.aWorldAABB.aMin = body.aPos; // 未知形状：退化为点盒
        body.aWorldAABB.aMax = body.aPos;
    }

    if(body.aType != eBODY_DYNAMIC) {
        SetZeroMatrix(body.aInvInertiaWorld);
        return;
    }
    GNg::sMAT44 rotation;
    GNg::sMAT44 rotationT;
    GNg::sMAT44 temp;
    GNg::QuatToMatrix44(&rotation, &body.aRot);
    GNg::Mat44Transpose(&rotationT, &rotation);
    GNg::Mat44Mul(&temp, &rotation, &body.aInvInertiaLocal);
    GNg::Mat44Mul(&body.aInvInertiaWorld, &temp, &rotationT);
}

// ═══════════════════════════════════════════════════════════
//  窄相碰撞入口
// ═══════════════════════════════════════════════════════════

bool CollideBodies(const sRIGID_BODY &a, const sRIGID_BODY &b, sCONTACT_MANIFOLD &out) {
    out = sCONTACT_MANIFOLD();
    if(!IsCollidable(a) || !IsCollidable(b)) return false;
    if(a.aShape == eSHAPE_SPHERE && b.aShape == eSHAPE_SPHERE) return CollideSphereSphere(a, b, out);
    if(a.aShape == eSHAPE_SPHERE && b.aShape == eSHAPE_BOX) return CollideSphereBox(a, b, out);
    if(a.aShape == eSHAPE_BOX && b.aShape == eSHAPE_SPHERE) {
        sCONTACT_MANIFOLD flipped;
        if(!CollideSphereBox(b, a, flipped)) return false;
        out.aNormal = Neg(flipped.aNormal);
        out.aCount = flipped.aCount;
        for(uint32_t i = 0; i < flipped.aCount; ++i) out.aPoints[i] = flipped.aPoints[i];
        return true;
    }
    return CollideBoxBox(a, b, out);
}

} // namespace GN3d
