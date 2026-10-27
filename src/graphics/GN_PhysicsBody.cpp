// GN_PhysicsBody.cpp — 刚体辅助：球 / 盒 / 胶囊的构造、惯量、世界 AABB 与逆惯量。
//
// 胶囊语义见只读契约 GN_Physics.h：沿局部 Y 轴、质心在中点、aHalfExtent.y = 圆柱段半长、aRadius = 半径。
// 胶囊惯量用"圆柱近似"：沿轴 Iy = 0.5 m r²，垂直轴 Ix = Iz = m (3 r² + (2h)²) / 12；
// 未计入两端半球的质量分布（真实胶囊的垂直轴惯量略小），仅用于 v1 的稳定求解。
#include <GN.h>
#include "GN_Physics.h"

#include <cmath>

namespace GN3d {

namespace {

inline GNg::sVEC3 MakeVec3(float x, float y, float z) {
    GNg::sVEC3 v = {x, y, z};
    return v;
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

} // namespace

// ═══════════════════════════════════════════════════════════
//  构造
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

sRIGID_BODY MakeCapsuleBody(eBODY_TYPE type, const GNg::sVEC3 &pos, float radius, float halfHeight,
                            float mass) {
    sRIGID_BODY body;
    body.aType = type;
    body.aShape = eSHAPE_CAPSULE;
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
    body.aHalfExtent = MakeVec3(0.0f, halfHeight, 0.0f);
    body.aSleeping = false;
    body.aSleepTime = 0.0f;
    body.aWorldAABB = sAABB();
    BodyUpdateInertia(body);
    BodyUpdateWorldState(body);
    return body;
}

// ═══════════════════════════════════════════════════════════
//  惯量
// ═══════════════════════════════════════════════════════════

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
    } else if(body.aShape == eSHAPE_CAPSULE) {
        const float radius = body.aRadius;
        const float halfHeight = body.aHalfExtent.y;
        if(!std::isfinite(radius) || !(radius > 0.0f) || !std::isfinite(halfHeight) || !(halfHeight >= 0.0f)) {
            return; // 半径非法 / 圆柱半长非法：降级为无限质量
        }
        invIy = 1.0f / (0.5f * mass * radius * radius); // 沿轴：I = 1/2 m r²
        const float perpendicular = mass * (3.0f * radius * radius + 4.0f * halfHeight * halfHeight) / 12.0f;
        invIx = invIz = 1.0f / perpendicular; // 垂直轴：圆柱近似 m(3r²+(2h)²)/12
    } else {
        return;
    }
    if(!std::isfinite(invIx) || !std::isfinite(invIy) || !std::isfinite(invIz)) return; // 参数极端：仍按无限质量
    body.aInvMass = 1.0f / mass;
    SetDiagonalMatrix(body.aInvInertiaLocal, invIx, invIy, invIz);
}

// ═══════════════════════════════════════════════════════════
//  世界状态
// ═══════════════════════════════════════════════════════════

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
    } else if(body.aShape == eSHAPE_CAPSULE) {
        // 胶囊世界 AABB = 两端球心（pos ± R·(0,h,0)）各扩 radius。
        GNg::sMAT44 rotation;
        GNg::QuatToMatrix44(&rotation, &body.aRot);
        const float radius = SafeHalfExtent(body.aRadius);
        const float halfHeight = (std::isfinite(body.aHalfExtent.y) && body.aHalfExtent.y > 0.0f)
                                     ? body.aHalfExtent.y
                                     : 0.0f;
        const float axisX = rotation.Elem(1, 0); // 局部 Y 轴的世界方向 = 旋转矩阵第 2 列
        const float axisY = rotation.Elem(1, 1);
        const float axisZ = rotation.Elem(1, 2);
        const float extentX = std::fabs(axisX) * halfHeight + radius;
        const float extentY = std::fabs(axisY) * halfHeight + radius;
        const float extentZ = std::fabs(axisZ) * halfHeight + radius;
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

} // namespace GN3d
