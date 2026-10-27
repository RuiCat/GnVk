// GN_PhysicsCollisionCapsule.cpp — 胶囊窄相：球-胶囊、胶囊-胶囊、盒-胶囊。
//
// 胶囊 = 沿局部 Y 轴、半长 aHalfExtent.y 的线段，外扩半径 aRadius（质心在线段中点）。
// 球-胶囊 / 胶囊-胶囊都归约为"两组球心"的球-球接触；盒-胶囊用线段与 OBB 的交替投影求最近点对
//（面接触一次迭代即精确，角/棱收敛到 1e-5 级）。法线一律 A→B、穿透 >= 0、接触点世界坐标。
#include <GN.h>
#include "GN_Physics.h"

#include <cmath>
#include <cstdint>

namespace GN3d {

namespace {

const float kCapsuleSlop = 1e-6f; // 重叠 >= -slop 视为接触
const float kCapsuleEpsilon = 1e-12f; // 几何退化阈值（长度平方）
const float kParallelThreshold = 1e-6f; // 线段近似平行的 sin²θ 阈值
const int kSegmentBoxIterations = 48; // 三分法迭代数：(2/3)^48 ≈ 4e-9，远小于 1e-3 容差

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

// 胶囊轴线段（世界坐标）：端点 = 质心 ± 局部 Y 轴 * 圆柱段半长；aAxis 为单位轴方向。
void GetCapsuleSegment(const sRIGID_BODY &capsule, GNg::sVEC3 &pointA, GNg::sVEC3 &pointB,
                       GNg::sVEC3 &axis) {
    GNg::sMAT44 rotation;
    GNg::QuatToMatrix44(&rotation, &capsule.aRot);
    axis = MakeVec3(rotation.Elem(1, 0), rotation.Elem(1, 1), rotation.Elem(1, 2)); // 局部 Y 轴
    const float halfHeight = (std::isfinite(capsule.aHalfExtent.y) && capsule.aHalfExtent.y > 0.0f)
                                 ? capsule.aHalfExtent.y
                                 : 0.0f;
    pointA = Sub(capsule.aPos, Scale(axis, halfHeight));
    pointB = Add(capsule.aPos, Scale(axis, halfHeight));
}

// 点到线段的最近点（线段退化为点时返回该点）。
GNg::sVEC3 ClosestPointOnSegment(const GNg::sVEC3 &point, const GNg::sVEC3 &segmentA,
                                 const GNg::sVEC3 &segmentB) {
    const GNg::sVEC3 direction = Sub(segmentB, segmentA);
    const float lengthSq = LengthSq(direction);
    if(lengthSq <= kCapsuleEpsilon) return segmentA;
    const float t = Clamp(Dot(Sub(point, segmentA), direction) / lengthSq, 0.0f, 1.0f);
    return Add(segmentA, Scale(direction, t));
}

// 两条线段的最近点对（Ericson 教科书算法；平行/退化时退化为端点投影，不产生 NaN）。
void ClosestPointsOnSegments(const GNg::sVEC3 &a0, const GNg::sVEC3 &a1, const GNg::sVEC3 &b0,
                             const GNg::sVEC3 &b1, GNg::sVEC3 &outA, GNg::sVEC3 &outB) {
    const GNg::sVEC3 directionA = Sub(a1, a0);
    const GNg::sVEC3 directionB = Sub(b1, b0);
    const GNg::sVEC3 betweenStart = Sub(a0, b0);
    const float lengthA = LengthSq(directionA);
    const float lengthB = LengthSq(directionB);
    float onA = 0.0f;
    float onB = 0.0f;
    if(lengthA <= kCapsuleEpsilon && lengthB <= kCapsuleEpsilon) {
        onA = onB = 0.0f;
    } else if(lengthA <= kCapsuleEpsilon) {
        onB = Clamp(Dot(directionB, betweenStart) / lengthB, 0.0f, 1.0f);
    } else {
        const float projectionA = Dot(directionA, betweenStart);
        if(lengthB <= kCapsuleEpsilon) {
            onA = Clamp(-projectionA / lengthA, 0.0f, 1.0f);
        } else {
            const float alignment = Dot(directionA, directionB);
            const float denominator = lengthA * lengthB - alignment * alignment;
            if(denominator > kParallelThreshold * lengthA * lengthB) {
                onA = Clamp((alignment * Dot(directionB, betweenStart) - projectionA * lengthB) / denominator, 0.0f,
                            1.0f);
            }
            onB = (alignment * onA + Dot(directionB, betweenStart)) / lengthB;
            if(onB < 0.0f) {
                onB = 0.0f;
                onA = Clamp(-projectionA / lengthA, 0.0f, 1.0f);
            } else if(onB > 1.0f) {
                onB = 1.0f;
                onA = Clamp((alignment - projectionA) / lengthA, 0.0f, 1.0f);
            }
        }
    }
    outA = Add(a0, Scale(directionA, onA));
    outB = Add(b0, Scale(directionB, onB));
}

// 点到 OBB 的最近点（盒局部空间夹取后回世界）。
GNg::sVEC3 ClosestPointOnBox(const sRIGID_BODY &box, const GNg::sVEC3 &point) {
    GNg::sMAT44 rotation;
    GNg::QuatToMatrix44(&rotation, &box.aRot);
    const float half[3] = {std::fabs(box.aHalfExtent.x), std::fabs(box.aHalfExtent.y),
                           std::fabs(box.aHalfExtent.z)};
    const GNg::sVEC3 relative = Sub(point, box.aPos);
    GNg::sVEC3 closest = box.aPos;
    for(int i = 0; i < 3; ++i) {
        const GNg::sVEC3 axis = MakeVec3(rotation.Elem(i, 0), rotation.Elem(i, 1), rotation.Elem(i, 2));
        closest = Add(closest, Scale(axis, Clamp(Dot(relative, axis), -half[i], half[i])));
    }
    return closest;
}

// 点到 OBB 的距离平方。
float DistanceSqToBox(const sRIGID_BODY &box, const GNg::sVEC3 &point) {
    return LengthSq(Sub(point, ClosestPointOnBox(box, point)));
}

// 盒上最近点的特征分类：返回贴边轴数（1 = 面内点，>=2 = 棱/角点；0 = 盒内点），
// 并给出面轴索引与该面的外法线（boundCount != 1 时 outNormal 无意义）。
int ClassifyBoxFeature(const sRIGID_BODY &box, const GNg::sVEC3 &point, int &faceAxis,
                       GNg::sVEC3 &outNormal) {
    GNg::sMAT44 rotation;
    GNg::QuatToMatrix44(&rotation, &box.aRot);
    const float half[3] = {std::fabs(box.aHalfExtent.x), std::fabs(box.aHalfExtent.y),
                           std::fabs(box.aHalfExtent.z)};
    const GNg::sVEC3 relative = Sub(point, box.aPos);
    int boundCount = 0;
    faceAxis = -1;
    outNormal = MakeVec3(0.0f, 0.0f, 0.0f);
    for(int i = 0; i < 3; ++i) {
        const GNg::sVEC3 axis = MakeVec3(rotation.Elem(i, 0), rotation.Elem(i, 1), rotation.Elem(i, 2));
        const float local = Dot(relative, axis);
        if(std::fabs(local) >= half[i] - 1e-4f) {
            ++boundCount;
            faceAxis = i;
            outNormal = Scale(axis, local >= 0.0f ? 1.0f : -1.0f);
        }
    }
    return boundCount;
}

// 线段到 OBB 的最近点：距离平方沿线段参数是凸函数，用三分法求全局最小值
//（避免交替投影在线段近乎平行盒面/棱时收敛过慢导致漏判浅接触）。
GNg::sVEC3 ClosestPointOnSegmentToBox(const sRIGID_BODY &box, const GNg::sVEC3 &segmentA,
                                      const GNg::sVEC3 &segmentB) {
    const GNg::sVEC3 direction = Sub(segmentB, segmentA);
    float low = 0.0f;
    float high = 1.0f;
    for(int iteration = 0; iteration < kSegmentBoxIterations; ++iteration) {
        const float first = low + (high - low) / 3.0f;
        const float second = high - (high - low) / 3.0f;
        const float firstDistance = DistanceSqToBox(box, Add(segmentA, Scale(direction, first)));
        const float secondDistance = DistanceSqToBox(box, Add(segmentA, Scale(direction, second)));
        if(firstDistance < secondDistance) {
            high = second;
        } else {
            low = first;
        }
    }
    return Add(segmentA, Scale(direction, 0.5f * (low + high)));
}

// 胶囊在盒内时：胶囊轴点取最近面，沿该面外法线推出（法线 A=盒 → B=胶囊 取外法线）。
void DeepCapsuleBox(const sRIGID_BODY &box, const GNg::sVEC3 &axisPoint, const sRIGID_BODY &capsule,
                    sCONTACT_MANIFOLD &out) {
    GNg::sMAT44 rotation;
    GNg::QuatToMatrix44(&rotation, &box.aRot);
    const float half[3] = {std::fabs(box.aHalfExtent.x), std::fabs(box.aHalfExtent.y),
                           std::fabs(box.aHalfExtent.z)};
    const GNg::sVEC3 relative = Sub(axisPoint, box.aPos);
    float local[3];
    GNg::sVEC3 axes[3];
    for(int i = 0; i < 3; ++i) {
        axes[i] = MakeVec3(rotation.Elem(i, 0), rotation.Elem(i, 1), rotation.Elem(i, 2));
        local[i] = Dot(relative, axes[i]);
    }
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
    float penetration = capsule.aRadius + minFace;
    if(!(penetration >= 0.0f)) penetration = 0.0f;
    out.aNormal = Scale(axes[faceAxis], outwardSign); // A 盒 → B 胶囊：指向胶囊被推出的方向
    out.aCount = 1;
    GNg::sVEC3 point = box.aPos;
    for(int i = 0; i < 3; ++i) {
        const float value = (i == faceAxis) ? outwardSign * half[i] : local[i];
        point = Add(point, Scale(axes[i], value));
    }
    out.aPoints[0].aPoint = point;
    out.aPoints[0].aPenetration = penetration;
}

} // namespace

// ═══════════════════════════════════════════════════════════
//  球-胶囊
// ═══════════════════════════════════════════════════════════

bool CollideSphereCapsule(const sRIGID_BODY &sphere, const sRIGID_BODY &capsule, sCONTACT_MANIFOLD &out) {
    GNg::sVEC3 segmentA;
    GNg::sVEC3 segmentB;
    GNg::sVEC3 axis;
    GetCapsuleSegment(capsule, segmentA, segmentB, axis);
    const GNg::sVEC3 closest = ClosestPointOnSegment(sphere.aPos, segmentA, segmentB);
    const GNg::sVEC3 delta = Sub(closest, sphere.aPos);
    const float distanceSq = LengthSq(delta);
    const float radiusSum = sphere.aRadius + capsule.aRadius;
    if(distanceSq <= kCapsuleEpsilon) return false; // 球心落在胶囊轴线上：法线不确定，按退化处理
    const float distance = sqrtf(distanceSq);
    if(distance > radiusSum + kCapsuleSlop) return false;
    float penetration = radiusSum - distance;
    if(!(penetration >= 0.0f)) penetration = 0.0f;
    const GNg::sVEC3 normal = Scale(delta, 1.0f / distance); // A 球 → B 胶囊
    out.aNormal = normal;
    out.aCount = 1;
    out.aPoints[0].aPoint = Add(sphere.aPos, Scale(normal, sphere.aRadius - penetration * 0.5f));
    out.aPoints[0].aPenetration = penetration;
    return true;
}

// ═══════════════════════════════════════════════════════════
//  胶囊-胶囊
// ═══════════════════════════════════════════════════════════

bool CollideCapsuleCapsule(const sRIGID_BODY &a, const sRIGID_BODY &b, sCONTACT_MANIFOLD &out) {
    GNg::sVEC3 a0;
    GNg::sVEC3 a1;
    GNg::sVEC3 axisA;
    GNg::sVEC3 b0;
    GNg::sVEC3 b1;
    GNg::sVEC3 axisB;
    GetCapsuleSegment(a, a0, a1, axisA);
    GetCapsuleSegment(b, b0, b1, axisB);
    GNg::sVEC3 closestA;
    GNg::sVEC3 closestB;
    ClosestPointsOnSegments(a0, a1, b0, b1, closestA, closestB);
    const GNg::sVEC3 delta = Sub(closestB, closestA);
    const float distanceSq = LengthSq(delta);
    const float radiusSum = a.aRadius + b.aRadius;
    GNg::sVEC3 normal;
    float penetration;
    GNg::sVEC3 contact;
    if(distanceSq > kCapsuleEpsilon) {
        const float distance = sqrtf(distanceSq);
        if(distance > radiusSum + kCapsuleSlop) return false;
        normal = Scale(delta, 1.0f / distance); // A → B
        penetration = radiusSum - distance;
        contact = Add(closestA, Scale(normal, a.aRadius - penetration * 0.5f));
    } else {
        // 共线重叠/同心：最近点重合，法线不唯一。取两线段中点连线，仍退化则取垂直于轴 A 的确定方向。
        const GNg::sVEC3 middleA = Scale(Add(a0, a1), 0.5f);
        const GNg::sVEC3 middleB = Scale(Add(b0, b1), 0.5f);
        const GNg::sVEC3 middleDelta = Sub(middleB, middleA);
        if(LengthSq(middleDelta) > kCapsuleEpsilon) {
            normal = Scale(middleDelta, 1.0f / sqrtf(LengthSq(middleDelta)));
        } else {
            const GNg::sVEC3 reference = std::fabs(axisA.y) < 0.9f ? MakeVec3(0.0f, 1.0f, 0.0f) : MakeVec3(1.0f, 0.0f, 0.0f);
            normal = Cross(axisA, reference);
            if(LengthSq(normal) <= kCapsuleEpsilon) normal = MakeVec3(1.0f, 0.0f, 0.0f);
            else normal = Scale(normal, 1.0f / sqrtf(LengthSq(normal)));
        }
        penetration = radiusSum; // 轴线重叠：按半径和给保守分离量（退化情形只保证不崩溃、深度非负）
        contact = Scale(Add(middleA, middleB), 0.5f);
    }
    if(!(penetration >= 0.0f)) penetration = 0.0f;
    out.aNormal = normal;
    out.aCount = 1;
    out.aPoints[0].aPoint = contact;
    out.aPoints[0].aPenetration = penetration;
    return true;
}

// ═══════════════════════════════════════════════════════════
//  盒-胶囊
// ═══════════════════════════════════════════════════════════

bool CollideBoxCapsule(const sRIGID_BODY &box, const sRIGID_BODY &capsule, sCONTACT_MANIFOLD &out) {
    GNg::sVEC3 segmentA;
    GNg::sVEC3 segmentB;
    GNg::sVEC3 axis;
    GetCapsuleSegment(capsule, segmentA, segmentB, axis);
    // 线段-盒最近点：三分法求全局最近点（面/角/棱都收敛到 1e-6 级）。
    const GNg::sVEC3 onSegment = ClosestPointOnSegmentToBox(box, segmentA, segmentB);
    const GNg::sVEC3 onBox = ClosestPointOnBox(box, onSegment);

    const GNg::sVEC3 delta = Sub(onSegment, onBox); // 盒表面 → 胶囊轴
    const float distanceSq = LengthSq(delta);
    int faceAxis = -1;
    GNg::sVEC3 faceNormal = MakeVec3(0.0f, 0.0f, 0.0f);
    const int boundCount = ClassifyBoxFeature(box, onBox, faceAxis, faceNormal);
    const bool faceContact = (boundCount == 1);
    GNg::sVEC3 normal;
    float penetration;
    if(distanceSq <= kCapsuleEpsilon) {
        if(!faceContact) {
            // 胶囊轴点落在盒内/棱角上：取最近面把胶囊推出（单点）。
            DeepCapsuleBox(box, onSegment, capsule, out);
            return true;
        }
        normal = faceNormal; // 轴点贴在盒面上：A 盒 → B 胶囊
        penetration = capsule.aRadius;
    } else {
        const float distance = sqrtf(distanceSq);
        if(distance > capsule.aRadius + kCapsuleSlop) return false;
        normal = Scale(delta, 1.0f / distance); // A 盒 → B 胶囊
        penetration = capsule.aRadius - distance;
        if(!(penetration >= 0.0f)) penetration = 0.0f;
    }

    // 面接触：沿胶囊轴取 4 个采样点，各自投影到同一个盒面，形成与盒-盒同级的流形；
    // 端部接触（只有轴端在半径内）与棱/角接触（最近点在棱/角上）自然只剩 1 点。
    if(faceContact) {
        const GNg::sVEC3 segmentDirection = Sub(segmentB, segmentA);
        uint32_t count = 0;
        for(int sampleIndex = 0; sampleIndex < 4; ++sampleIndex) {
            const float t = (float)sampleIndex / 3.0f;
            const GNg::sVEC3 sample = Add(segmentA, Scale(segmentDirection, t));
            const GNg::sVEC3 sampleOnBox = ClosestPointOnBox(box, sample);
            int sampleFaceAxis = -1;
            GNg::sVEC3 sampleFaceNormal;
            if(ClassifyBoxFeature(box, sampleOnBox, sampleFaceAxis, sampleFaceNormal) != 1 ||
               sampleFaceAxis != faceAxis) {
                continue; // 该采样点投影到盒棱/角或落入盒内：不参与面流形
            }
            const float sampleDistance = sqrtf(LengthSq(Sub(sample, sampleOnBox)));
            if(sampleDistance > capsule.aRadius + kCapsuleSlop) continue;
            float samplePenetration = capsule.aRadius - sampleDistance;
            if(!(samplePenetration >= 0.0f)) samplePenetration = 0.0f;
            bool duplicate = false; // 短胶囊（含 halfHeight=0）采样点可能重合：去重后仍与球等价
            for(uint32_t i = 0; i < count; ++i) {
                if(LengthSq(Sub(sampleOnBox, out.aPoints[i].aPoint)) <= 1e-8f) {
                    duplicate = true;
                    break;
                }
            }
            if(duplicate) continue;
            out.aPoints[count].aPoint = sampleOnBox;
            out.aPoints[count].aPenetration = samplePenetration;
            ++count;
        }
        if(count > 0) {
            out.aNormal = normal;
            out.aCount = count;
            return true;
        }
    }
    out.aNormal = normal;
    out.aCount = 1;
    out.aPoints[0].aPoint = onBox;
    out.aPoints[0].aPenetration = penetration;
    return true;
}

} // namespace GN3d
