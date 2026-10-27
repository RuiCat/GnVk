// GN_3Dcloth.cpp — 布料 XPBD：规则网格、距离约束（结构/剪切/弯曲）、球体碰撞、法线重建。
//
// 参考实现范围（v1，纯 CPU）：
//   * 子步内：v += (gravity + wind) * h → x += v * h → XPBD 距离约束（8 次迭代）→ 球碰撞 → v = (x - xPrev)/h。
//   * 柔度只在距离约束里生效（alpha = compliance / h²），固定点 invMass = 0。
//   * 不做阻尼/摩擦/自碰撞；GPU 上传与绘制留 P8/P9。
#include <GN.h>
#include "GN_3Dcloth.h"

#include <cmath>

namespace GN3d {

namespace {

const int kSolveIterations = 8;
const float kMaxDt = 0.1f;
const int kMaxSubsteps = 16;
const float kClothEpsilon = 1e-9f;

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

float lengthVec(const GNg::sVEC3 &v) {
    return sqrtf(lengthSqVec(v));
}

bool finiteVec(const GNg::sVEC3 &v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

} // namespace

cCLOTH::cCLOTH() {
    aDesc.aNx = 0;
    aDesc.aNy = 0;
    aDesc.aSpacing = 0.0f;
    aDesc.aMass = 0.0f;
    aDesc.aCompliance = 0.0f;
    aDesc.aSubsteps = 1;
    aDesc.aGravity = makeVec3(0.0f, -9.81f, 0.0f);
    aDesc.aWind = makeVec3(0.0f, 0.0f, 0.0f);
    aDesc.aPinCorners = false;
}

cCLOTH::~cCLOTH() {}

bool cCLOTH::create(const sCLOTH_DESC &desc) {
    if(desc.aNx < 2 || desc.aNy < 2) return false;
    if(!(desc.aSpacing > 0.0f) || !std::isfinite(desc.aSpacing)) return false;
    if(!(desc.aMass > 0.0f) || !std::isfinite(desc.aMass)) return false;
    if(!(desc.aCompliance >= 0.0f) || !std::isfinite(desc.aCompliance)) return false;
    if(!finiteVec(desc.aGravity) || !finiteVec(desc.aWind)) return false;

    aDesc = desc;
    if(aDesc.aSubsteps < 1) aDesc.aSubsteps = 1;
    if(aDesc.aSubsteps > kMaxSubsteps) aDesc.aSubsteps = kMaxSubsteps;
    aColliders.clear();
    buildGrid();
    buildConstraints();
    aNormals.assign(aPositions.size(), makeVec3(0.0f, 0.0f, 1.0f));
    aLambdas.assign(aConstraints.size(), 0.0f);
    return true;
}

void cCLOTH::buildGrid() {
    const uint32_t count = (uint32_t)(aDesc.aNx * aDesc.aNy);
    aPositions.resize(count);
    aPrevious.resize(count);
    aVelocities.assign(count, makeVec3(0.0f, 0.0f, 0.0f));
    aInvMass.assign(count, 1.0f / aDesc.aMass);
    for(int y = 0; y < aDesc.aNy; ++y) {
        for(int x = 0; x < aDesc.aNx; ++x) {
            aPositions[(size_t)y * aDesc.aNx + x] =
                makeVec3((float)x * aDesc.aSpacing, (float)y * aDesc.aSpacing, 0.0f);
        }
    }
    if(aDesc.aPinCorners) {
        aInvMass[0] = 0.0f;
        aInvMass[(size_t)aDesc.aNx - 1u] = 0.0f;
        aInvMass[(size_t)(aDesc.aNy - 1) * aDesc.aNx] = 0.0f;
        aInvMass[count - 1u] = 0.0f;
    }
    aPrevious = aPositions;

    aIndices.clear();
    aIndices.reserve((size_t)(aDesc.aNx - 1) * (aDesc.aNy - 1) * 6u);
    for(int y = 0; y < aDesc.aNy - 1; ++y) {
        for(int x = 0; x < aDesc.aNx - 1; ++x) {
            const uint32_t i0 = (uint32_t)y * aDesc.aNx + x;
            const uint32_t i1 = i0 + 1u;
            const uint32_t i2 = i0 + (uint32_t)aDesc.aNx;
            const uint32_t i3 = i2 + 1u;
            aIndices.push_back(i0);
            aIndices.push_back(i1);
            aIndices.push_back(i2);
            aIndices.push_back(i1);
            aIndices.push_back(i3);
            aIndices.push_back(i2);
        }
    }
}

void cCLOTH::buildConstraints() {
    aConstraints.clear();
    const int nx = aDesc.aNx;
    const int ny = aDesc.aNy;
    auto addDistance = [this](uint32_t a, uint32_t b) {
        if(a == b) return;
        sDISTANCE constraint;
        constraint.aVertexA = a;
        constraint.aVertexB = b;
        constraint.aRestLength = lengthVec(subVec(aPositions[b], aPositions[a]));
        aConstraints.push_back(constraint);
    };
    for(int y = 0; y < ny; ++y) {
        for(int x = 0; x < nx; ++x) {
            const uint32_t index = (uint32_t)y * nx + x;
            if(x + 1 < nx) addDistance(index, index + 1u);
            if(y + 1 < ny) addDistance(index, index + (uint32_t)nx);
            if(x + 1 < nx && y + 1 < ny) addDistance(index, index + (uint32_t)nx + 1u);
            if(x > 0 && y + 1 < ny) addDistance(index, index + (uint32_t)nx - 1u);
            if(x + 2 < nx) addDistance(index, index + 2u);
            if(y + 2 < ny) addDistance(index, index + (uint32_t)nx * 2u);
        }
    }
}

void cCLOTH::reset() {
    if(aPositions.empty()) return;
    buildGrid();
    aNormals.assign(aPositions.size(), makeVec3(0.0f, 0.0f, 1.0f));
    aLambdas.assign(aConstraints.size(), 0.0f);
}

void cCLOTH::step(float dt) {
    if(aPositions.empty() || !(dt > 0.0f)) return;
    if(dt > kMaxDt) dt = kMaxDt;
    const int substeps = aDesc.aSubsteps > 0 ? aDesc.aSubsteps : 1;
    const float h = dt / (float)substeps;
    for(int substep = 0; substep < substeps; ++substep) {
        aPrevious = aPositions;
        integrateVelocities(h);
        for(size_t i = 0; i < aPositions.size(); ++i) {
            if(aInvMass[i] <= 0.0f) continue;
            aPositions[i] = addVec(aPositions[i], scaleVec(aVelocities[i], h));
        }
        for(size_t i = 0; i < aLambdas.size(); ++i) aLambdas[i] = 0.0f;
        for(int iteration = 0; iteration < kSolveIterations; ++iteration) solveConstraints(h);
        solveColliders();
        updateVelocities(h);
    }
    updateNormals();
}

void cCLOTH::integrateVelocities(float step) {
    const GNg::sVEC3 acceleration = addVec(aDesc.aGravity, aDesc.aWind);
    for(size_t i = 0; i < aPositions.size(); ++i) {
        if(aInvMass[i] <= 0.0f) {
            aVelocities[i] = makeVec3(0.0f, 0.0f, 0.0f);
            continue;
        }
        aVelocities[i] = addVec(aVelocities[i], scaleVec(acceleration, step));
    }
}

void cCLOTH::solveConstraints(float step) {
    const float alphaTilde = aDesc.aCompliance / (step * step);
    for(size_t c = 0; c < aConstraints.size(); ++c) {
        const sDISTANCE &constraint = aConstraints[c];
        const float wA = aInvMass[constraint.aVertexA];
        const float wB = aInvMass[constraint.aVertexB];
        const float wSum = wA + wB;
        if(wSum <= 0.0f) continue;
        GNg::sVEC3 delta = subVec(aPositions[constraint.aVertexA], aPositions[constraint.aVertexB]);
        const float length = lengthVec(delta);
        if(length < kClothEpsilon) continue;
        const GNg::sVEC3 gradient = scaleVec(delta, 1.0f / length);
        const float error = length - constraint.aRestLength;
        const float denominator = wSum + alphaTilde;
        if(denominator < kClothEpsilon) continue;
        const float deltaLambda = (-error - alphaTilde * aLambdas[c]) / denominator;
        aLambdas[c] += deltaLambda;
        aPositions[constraint.aVertexA] =
            addVec(aPositions[constraint.aVertexA], scaleVec(gradient, deltaLambda * wA));
        aPositions[constraint.aVertexB] =
            addVec(aPositions[constraint.aVertexB], scaleVec(gradient, -deltaLambda * wB));
    }
}

void cCLOTH::solveColliders() {
    for(size_t i = 0; i < aPositions.size(); ++i) {
        if(aInvMass[i] <= 0.0f) continue;
        for(size_t c = 0; c < aColliders.size(); ++c) {
            const sCLOTH_COLLIDER &collider = aColliders[c];
            const GNg::sVEC3 delta = subVec(aPositions[i], collider.aCenter);
            const float distance = lengthVec(delta);
            if(distance >= collider.aRadius) continue;
            if(distance < kClothEpsilon) {
                aPositions[i] = addVec(collider.aCenter, makeVec3(0.0f, collider.aRadius, 0.0f));
            } else {
                aPositions[i] = addVec(collider.aCenter, scaleVec(delta, collider.aRadius / distance));
            }
        }
    }
}

void cCLOTH::updateVelocities(float step) {
    for(size_t i = 0; i < aPositions.size(); ++i) {
        if(aInvMass[i] <= 0.0f) {
            aVelocities[i] = makeVec3(0.0f, 0.0f, 0.0f);
            continue;
        }
        aVelocities[i] = scaleVec(subVec(aPositions[i], aPrevious[i]), 1.0f / step);
    }
}

void cCLOTH::updateNormals() {
    for(size_t i = 0; i < aNormals.size(); ++i) aNormals[i] = makeVec3(0.0f, 0.0f, 0.0f);
    for(size_t t = 0; t + 2 < aIndices.size(); t += 3) {
        const uint32_t i0 = aIndices[t];
        const uint32_t i1 = aIndices[t + 1u];
        const uint32_t i2 = aIndices[t + 2u];
        const GNg::sVEC3 normal = crossVec(subVec(aPositions[i1], aPositions[i0]),
                                           subVec(aPositions[i2], aPositions[i0]));
        aNormals[i0] = addVec(aNormals[i0], normal);
        aNormals[i1] = addVec(aNormals[i1], normal);
        aNormals[i2] = addVec(aNormals[i2], normal);
    }
    for(size_t i = 0; i < aNormals.size(); ++i) {
        const float length = lengthVec(aNormals[i]);
        aNormals[i] = (length > kClothEpsilon) ? scaleVec(aNormals[i], 1.0f / length)
                                                : makeVec3(0.0f, 0.0f, 1.0f);
    }
}

bool cCLOTH::addSphereCollider(const GNg::sVEC3 &center, float radius) {
    if(!(radius > 0.0f) || !std::isfinite(radius)) return false;
    if(!finiteVec(center)) return false;
    sCLOTH_COLLIDER collider;
    collider.aCenter = center;
    collider.aRadius = radius;
    aColliders.push_back(collider);
    return true;
}

} // namespace GN3d
