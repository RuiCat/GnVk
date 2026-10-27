// GN_PhysicsMesh.cpp — 静态三角网碰撞（v1：球 / 胶囊对三角网，BVH 宽相）。
//
// 边界：
//   * 输入网格按 VkVertex3D 布局（每顶点 11 floats），只取前 3 个 float 作为位置；网格必须已是世界坐标。
//   * 三角形用 BVH（`cBVH`）按包围盒宽相筛选，再逐个做窄相。
//   * 球：球心到三角形最近点；胶囊：线段上到三角形最近点（距离函数沿线段凸，三分法求全局最小）。
//   * 盒对三角网 v1 未实现（直接跳过）；三角网接触不参与 warm start（每次冷启动求解）。
//   * 接触的 A 端是一个不在 aBodies 里的静态代理刚体（invMass/invInertia 全 0），因此求解器无需改。
#include <GN.h>
#include "GN_Physics.h"
#include "GN_3Dmodel.h"

#include <cmath>
#include <cstdint>
#include <vector>

namespace GN3d {

namespace {

const float kMeshSlop = 1e-6f;
const float kMeshEpsilon = 1e-12f;
const int kCapsuleSegmentIterations = 40;

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

float lengthSqVec(const GNg::sVEC3 &v) {
    return dotVec(v, v);
}

bool normalizeVec(const GNg::sVEC3 &v, GNg::sVEC3 &out) {
    const float lengthSq = lengthSqVec(v);
    if(lengthSq < kMeshEpsilon) return false;
    out = scaleVec(v, 1.0f / sqrtf(lengthSq));
    return true;
}

// 三角形面法线；退化三角形返回 false。
bool triangleNormal(const GNg::sVEC3 &a, const GNg::sVEC3 &b, const GNg::sVEC3 &c, GNg::sVEC3 &out) {
    return normalizeVec(crossVec(subVec(b, a), subVec(c, a)), out);
}

// 点到三角形最近点（Ericson 区域判定，覆盖顶点/边/面内部）。
GNg::sVEC3 closestPointOnTriangle(const GNg::sVEC3 &point, const GNg::sVEC3 &a, const GNg::sVEC3 &b,
                                  const GNg::sVEC3 &c) {
    const GNg::sVEC3 ab = subVec(b, a);
    const GNg::sVEC3 ac = subVec(c, a);
    const GNg::sVEC3 ap = subVec(point, a);
    const float d1 = dotVec(ab, ap);
    const float d2 = dotVec(ac, ap);
    if(d1 <= 0.0f && d2 <= 0.0f) return a;

    const GNg::sVEC3 bp = subVec(point, b);
    const float d3 = dotVec(ab, bp);
    const float d4 = dotVec(ac, bp);
    if(d3 >= 0.0f && d4 <= d3) return b;

    const float vc = d1 * d4 - d3 * d2;
    if(vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f) return addVec(a, scaleVec(ab, d1 / (d1 - d3)));

    const GNg::sVEC3 cp = subVec(point, c);
    const float d5 = dotVec(ab, cp);
    const float d6 = dotVec(ac, cp);
    if(d6 >= 0.0f && d5 <= d6) return c;

    const float vb = d5 * d2 - d1 * d6;
    if(vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f) return addVec(a, scaleVec(ac, d2 / (d2 - d6)));

    const float va = d3 * d6 - d5 * d4;
    if(va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f) {
        return addVec(b, scaleVec(subVec(c, b), (d4 - d3) / ((d4 - d3) + (d5 - d6))));
    }

    const float denominator = 1.0f / (va + vb + vc);
    return addVec(a, addVec(scaleVec(ab, vb * denominator), scaleVec(ac, vc * denominator)));
}

float distanceSqPointTriangle(const GNg::sVEC3 &point, const GNg::sVEC3 &a, const GNg::sVEC3 &b,
                              const GNg::sVEC3 &c) {
    return lengthSqVec(subVec(point, closestPointOnTriangle(point, a, b, c)));
}

// 球（center/radius）与三角形接触；法线由三角形指向球心（A=三角网 → B=球）。
bool sphereTriangleContact(const GNg::sVEC3 &center, float radius, const GNg::sVEC3 &a,
                           const GNg::sVEC3 &b, const GNg::sVEC3 &c, sCONTACT_MANIFOLD &out) {
    const GNg::sVEC3 closest = closestPointOnTriangle(center, a, b, c);
    const GNg::sVEC3 delta = subVec(center, closest);
    const float distanceSq = lengthSqVec(delta);
    if(distanceSq > (radius + kMeshSlop) * (radius + kMeshSlop)) return false;
    GNg::sVEC3 normal;
    if(!normalizeVec(delta, normal)) {
        if(!triangleNormal(a, b, c, normal)) return false; // 球心落在三角形平面上：取面法线
    }
    const float distance = sqrtf(distanceSq);
    float penetration = radius - distance;
    if(!(penetration >= 0.0f)) penetration = 0.0f;
    out.aNormal = normal;
    out.aCount = 1;
    out.aPoints[0].aPoint = closest;
    out.aPoints[0].aPenetration = penetration;
    return true;
}

// 胶囊（线段 e0→e1 + 半径）与三角形接触；在线段上求到三角形距离的全局最小点。
bool capsuleTriangleContact(const GNg::sVEC3 &e0, const GNg::sVEC3 &e1, float radius,
                            const GNg::sVEC3 &a, const GNg::sVEC3 &b, const GNg::sVEC3 &c,
                            sCONTACT_MANIFOLD &out) {
    float low = 0.0f;
    float high = 1.0f;
    for(int iteration = 0; iteration < kCapsuleSegmentIterations; ++iteration) {
        const float first = low + (high - low) / 3.0f;
        const float second = high - (high - low) / 3.0f;
        const GNg::sVEC3 pointFirst = addVec(e0, scaleVec(subVec(e1, e0), first));
        const GNg::sVEC3 pointSecond = addVec(e0, scaleVec(subVec(e1, e0), second));
        if(distanceSqPointTriangle(pointFirst, a, b, c) <
           distanceSqPointTriangle(pointSecond, a, b, c)) {
            high = second;
        } else {
            low = first;
        }
    }
    const GNg::sVEC3 segmentPoint = addVec(e0, scaleVec(subVec(e1, e0), 0.5f * (low + high)));
    return sphereTriangleContact(segmentPoint, radius, a, b, c, out);
}

// 静态代理刚体：不在 aBodies 中，只作为三角网接触的 A 端（无限质量）。
void initMeshProxy(sRIGID_BODY &body) {
    body = sRIGID_BODY();
    body.aType = eBODY_STATIC;
    body.aShape = eSHAPE_BOX;
    body.aMass = 0.0f;
    body.aInvMass = 0.0f;
    body.aPos = makeVec3(0.0f, 0.0f, 0.0f);
    body.aRot = GNg::sQUAT();
    body.aLinVel = makeVec3(0.0f, 0.0f, 0.0f);
    body.aAngVel = makeVec3(0.0f, 0.0f, 0.0f);
    body.aInvInertiaLocal.Init(false);
    body.aInvInertiaWorld.Init(false);
    body.aFriction = 0.5f;
    body.aRestitution = 0.0f;
    body.aLinearDamping = 0.0f;
    body.aAngularDamping = 0.0f;
    body.aRadius = 0.0f;
    body.aHalfExtent = makeVec3(0.0f, 0.0f, 0.0f);
    body.aSleeping = true;
    body.aSleepTime = 0.0f;
}

} // namespace

void cPHYSICS_WORLD::resetMesh() {
    initMeshProxy(aMeshProxyBody);
    aMeshVertices.clear();
    aMeshTriangleBounds.clear();
    aMeshBvh.Clear();
}

void cPHYSICS_WORLD::AddStaticMesh(const sMESH &mesh) {
    if(mesh.aVertices == 0 || mesh.aIndices == 0) return;
    const uint32_t triangleCount = mesh.aIndexCount / 3u;
    for(uint32_t triangle = 0; triangle < triangleCount; ++triangle) {
        const uint32_t i0 = mesh.aIndices[triangle * 3u];
        const uint32_t i1 = mesh.aIndices[triangle * 3u + 1u];
        const uint32_t i2 = mesh.aIndices[triangle * 3u + 2u];
        if(i0 >= mesh.aVertexCount || i1 >= mesh.aVertexCount || i2 >= mesh.aVertexCount) continue;
        const float *p0 = mesh.aVertices + i0 * 11u;
        const float *p1 = mesh.aVertices + i1 * 11u;
        const float *p2 = mesh.aVertices + i2 * 11u;
        const GNg::sVEC3 a = makeVec3(p0[0], p0[1], p0[2]);
        const GNg::sVEC3 b = makeVec3(p1[0], p1[1], p1[2]);
        const GNg::sVEC3 c = makeVec3(p2[0], p2[1], p2[2]);
        GNg::sVEC3 normal;
        if(!triangleNormal(a, b, c, normal)) continue; // 退化三角形
        sAABB bounds;
        bounds.aMin.x = (a.x < b.x) ? (a.x < c.x ? a.x : c.x) : (b.x < c.x ? b.x : c.x);
        bounds.aMin.y = (a.y < b.y) ? (a.y < c.y ? a.y : c.y) : (b.y < c.y ? b.y : c.y);
        bounds.aMin.z = (a.z < b.z) ? (a.z < c.z ? a.z : c.z) : (b.z < c.z ? b.z : c.z);
        bounds.aMax.x = (a.x > b.x) ? (a.x > c.x ? a.x : c.x) : (b.x > c.x ? b.x : c.x);
        bounds.aMax.y = (a.y > b.y) ? (a.y > c.y ? a.y : c.y) : (b.y > c.y ? b.y : c.y);
        bounds.aMax.z = (a.z > b.z) ? (a.z > c.z ? a.z : c.z) : (b.z > c.z ? b.z : c.z);
        aMeshVertices.push_back(a);
        aMeshVertices.push_back(b);
        aMeshVertices.push_back(c);
        aMeshTriangleBounds.push_back(bounds);
    }
    aMeshBvh.Clear();
    if(!aMeshTriangleBounds.empty()) {
        aMeshBvh.Build(&aMeshTriangleBounds[0], (uint32_t)aMeshTriangleBounds.size());
    }
}

// 宽相：动态刚体的世界 AABB 查 BVH；窄相：球/胶囊对三角形。
void cPHYSICS_WORLD::buildMeshContacts() {
    if(aMeshTriangleBounds.empty()) return;
    std::vector<uint32_t> candidates;
    for(size_t i = 0; i < aBodies.size(); ++i) {
        if(!aUsed[i]) continue;
        sRIGID_BODY &body = aBodies[i];
        if(body.aType != eBODY_DYNAMIC || body.aSleeping) continue;
        if(body.aShape != eSHAPE_SPHERE && body.aShape != eSHAPE_CAPSULE) continue; // v1
        candidates.clear();
        aMeshBvh.QueryAABB(body.aWorldAABB, candidates);
        for(size_t candidate = 0; candidate < candidates.size(); ++candidate) {
            const uint32_t triangle = candidates[candidate];
            if(triangle * 3u + 2u >= aMeshVertices.size()) continue;
            const GNg::sVEC3 &a = aMeshVertices[triangle * 3u];
            const GNg::sVEC3 &b = aMeshVertices[triangle * 3u + 1u];
            const GNg::sVEC3 &c = aMeshVertices[triangle * 3u + 2u];
            sCONTACT_MANIFOLD manifold = sCONTACT_MANIFOLD(); // 值初始化，窄相失败时也不会读到未初始化字段
            bool hit = false;
            if(body.aShape == eSHAPE_SPHERE) {
                hit = sphereTriangleContact(body.aPos, body.aRadius, a, b, c, manifold);
            } else {
                GNg::sVEC3 up = makeVec3(0.0f, 1.0f, 0.0f);
                GNg::sVEC3 axis;
                GNg::QuatRotateVec3(&axis, &up, &body.aRot);
                const float halfHeight = body.aHalfExtent.y > 0.0f ? body.aHalfExtent.y : 0.0f;
                const GNg::sVEC3 e0 = subVec(body.aPos, scaleVec(axis, halfHeight));
                const GNg::sVEC3 e1 = addVec(body.aPos, scaleVec(axis, halfHeight));
                hit = capsuleTriangleContact(e0, e1, body.aRadius, a, b, c, manifold);
            }
            if(!hit) continue;
            sCONTACT_CONSTRAINT constraint;
            constraint.aBodyA = &aMeshProxyBody;
            constraint.aBodyB = &body;
            constraint.aManifold = manifold;
            for(int k = 0; k < 4; ++k) {
                constraint.aAccNormal[k] = 0.0f;
                constraint.aAccTangent[k] = 0.0f;
            }
            aConstraints.push_back(constraint);
            aConstraintBodyLo.push_back(INVALID_HANDLE); // 三角网接触不缓存 warm start
            aConstraintBodyHi.push_back(INVALID_HANDLE);
        }
    }
}

} // namespace GN3d
