// GN_3Dcloth.cpp — 布料 XPBD：规则网格、距离约束（结构/剪切/弯曲）、三种碰撞体、法线重建。
//
// 子步内：v += (gravity + wind) * h → x += v * h → XPBD 距离约束（8 次迭代）
//         → 碰撞推离（球/胶囊/三角网）→ 摩擦衰减 → v = (x - xPrev)/h。
// 柔度只在距离约束里生效（alpha = compliance / h²），固定点 invMass = 0。
//
// 边界（不是"没做完"，是刻意的取舍，详见头文件）：
//   * 点对几何的**离散**碰撞，不做连续碰撞 —— 薄壁可以被顶点漏穿；
//   * 碰撞体是静的，布料不反推它们；
//   * 摩擦是**速度衰减**，不是静摩擦；
//   * 不做自碰撞（布自己撞自己需要哈希网格 + 互相推开，是另一套东西）。
// GPU 上传与绘制留 P8/P9。
//
// 三角网碰撞体为什么要加速结构：布料顶点上万、场景三角网上万，
// 两两暴力是 1e8 量级的距离计算，每帧跑不起。这里用**均匀网格**把三角形分桶，
// 每个顶点只查自己所在格及 3×3×3 邻域。格边长取"不小于查询半径"，
// 于是"邻域"这一步是充分的（而不是启发式的）。
#include <GN.h>
#include "GN_3Dcloth.h"

#include "GN_Physics.h" // querySphereTriangle（点对三角形，避免两处各写一遍）

#include <cmath>

namespace GN3d {

namespace {

const int kSolveIterations = 8;
const float kMaxDt = 0.1f;
const int kMaxSubsteps = 16;
const float kClothEpsilon = 1e-9f;
// 三角网网格：单轴最多分这么多格（防止超大场景分配出几百万个空桶）。
const int kMeshGridMaxDim = 48;
// 三角网接触容差（与物理模块的 kMeshSlop 同量级）。
const float kMeshSlop = 1e-4f;
// 判定"点的投影是否落在三角形 footprint 内"的容差（10 * kMeshSlop）。
// 取绝对值而不是按三角形尺寸缩放：布料的顶点间距通常在毫米~厘米量级，
// 1e-3 米已经比它小两个数量级，不会把"真在内部"的点误判成"在边上"。
const float kFootprintToleranceSq = 1.0e-6f; // (1e-3)^2

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

bool normalizeVec(const GNg::sVEC3 &v, GNg::sVEC3 &out) {
    const float length = lengthVec(v);
    if(length <= kClothEpsilon) return false;
    out = scaleVec(v, 1.0f / length);
    return true;
}

// 取一个与 axis 垂直的单位向量。用于"顶点正好落在胶囊轴线上"的退化情形：
// 此时沿轴推会把顶点推穿胶囊，只能选一个垂直方向。
// 做法是拿一个与 axis 最不平行的坐标轴做叉积（选绝对值最小的分量，数值最稳）。
bool perpendicularTo(const GNg::sVEC3 &axis, GNg::sVEC3 &out) {
    GNg::sVEC3 unit;
    if(!normalizeVec(axis, unit)) return false;
    GNg::sVEC3 reference;
    const float ax = fabsf(unit.x), ay = fabsf(unit.y), az = fabsf(unit.z);
    if(ax <= ay && ax <= az) {
        reference = makeVec3(1.0f, 0.0f, 0.0f);
    } else if(ay <= az) {
        reference = makeVec3(0.0f, 1.0f, 0.0f);
    } else {
        reference = makeVec3(0.0f, 0.0f, 1.0f);
    }
    GNg::sVEC3 perpendicular = crossVec(unit, reference);
    return normalizeVec(perpendicular, out);
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
    aOrigin = makeVec3(0.0f, 0.0f, 0.0f);
    aDesc.aPinMode = eCLOTH_PIN_NONE;
    aDesc.aFriction = 0.0f; // 默认保持 v1 的"无摩擦"行为
}

// ═══════════════════════════════════════════════════════════
//  碰撞体结构
// ═══════════════════════════════════════════════════════════

cCLOTH::sCOLLIDER::sCOLLIDER()
    : aKind(eCLOTH_COLLIDER_SPHERE), aCenter(makeVec3(0.0f, 0.0f, 0.0f)),
      aFrom(makeVec3(0.0f, 0.0f, 0.0f)), aTo(makeVec3(0.0f, 0.0f, 0.0f)), aRadius(0.0f), aMesh(0) {}

cCLOTH::sMESH_COLLIDER::sMESH_COLLIDER()
    : aMin(makeVec3(0.0f, 0.0f, 0.0f)), aMax(makeVec3(0.0f, 0.0f, 0.0f)), aCellSize(1.0f) {
    aDim[0] = aDim[1] = aDim[2] = 1;
}

int cCLOTH::sMESH_COLLIDER::cellIndex(int x, int y, int z) const {
    if(x < 0 || y < 0 || z < 0) return -1;
    if(x >= aDim[0] || y >= aDim[1] || z >= aDim[2]) return -1;
    return (z * aDim[1] + y) * aDim[0] + x;
}

void cCLOTH::sMESH_COLLIDER::cellCoord(const GNg::sVEC3 &point, int out[3]) const {
    const float inverse = 1.0f / aCellSize;
    const float local[3] = {(point.x - aMin.x) * inverse, (point.y - aMin.y) * inverse,
                            (point.z - aMin.z) * inverse};
    for(int axis = 0; axis < 3; ++axis) {
        int cell = (int)floorf(local[axis]);
        // 贴边（点在 max 上）会算出 dim，钳回最后一格；极端越界也钳住，
        // 让"查询"永远有一个合法格（真正的越界判定交给距离测试）。
        if(cell < 0) cell = 0;
        if(cell >= aDim[axis]) cell = aDim[axis] - 1;
        out[axis] = cell;
    }
}

cCLOTH::~cCLOTH() {}

bool cCLOTH::create(const sCLOTH_DESC &desc) {
    if(desc.aNx < 2 || desc.aNy < 2) return false;
    // nx*ny 按 int 相乘：封顶防溢出；4096²=16M 顶点已远超实用布料规模。
    if(desc.aNx > 4096 || desc.aNy > 4096) return false;
    if(!(desc.aSpacing > 0.0f) || !std::isfinite(desc.aSpacing)) return false;
    if(!(desc.aMass > 0.0f) || !std::isfinite(desc.aMass)) return false;
    if(!(desc.aCompliance >= 0.0f) || !std::isfinite(desc.aCompliance)) return false;
    if(!finiteVec(desc.aGravity) || !finiteVec(desc.aWind)) return false;
    if(!(desc.aFriction >= 0.0f) || !std::isfinite(desc.aFriction)) return false;

    aDesc = desc;
    if(aDesc.aSubsteps < 1) aDesc.aSubsteps = 1;
    if(aDesc.aSubsteps > kMaxSubsteps) aDesc.aSubsteps = kMaxSubsteps;
    if(aDesc.aFriction > 1.0f) aDesc.aFriction = 1.0f;
    aColliders.clear();
    aMeshes.clear();
    buildGrid();
    buildConstraints();
    aNormals.assign(aPositions.size(), makeVec3(0.0f, 0.0f, 1.0f));
    aLambdas.assign(aConstraints.size(), 0.0f);
    aContactNormals.assign(aPositions.size(), makeVec3(0.0f, 0.0f, 0.0f));
    aContacted.assign(aPositions.size(), 0);
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
                makeVec3(aOrigin.x + (float)x * aDesc.aSpacing,
                         aOrigin.y + (float)y * aDesc.aSpacing, aOrigin.z);
        }
    }
    if(aDesc.aPinMode == eCLOTH_PIN_CORNERS) {
        aInvMass[0] = 0.0f;
        aInvMass[(size_t)aDesc.aNx - 1u] = 0.0f;
        aInvMass[(size_t)(aDesc.aNy - 1) * aDesc.aNx] = 0.0f;
        aInvMass[count - 1u] = 0.0f;
    } else if(aDesc.aPinMode == eCLOTH_PIN_TOP_EDGE) {
        // 顶边 = y 最大的那一排（索引 y = aNy-1）
        const size_t row = (size_t)(aDesc.aNy - 1) * (size_t)aDesc.aNx;
        for(int x = 0; x < aDesc.aNx; ++x) aInvMass[row + (size_t)x] = 0.0f;
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

void cCLOTH::place(const GNg::sVEC3 &offset) {
    if(!finiteVec(offset)) return;
    aOrigin = offset;
    if(aPositions.empty()) return; // 还没 create：只记下位置，buildGrid 时生效
    reset(); // reset 会按新的 aOrigin 重建网格并清零速度
}

void cCLOTH::reset() {
    if(aPositions.empty()) return;
    buildGrid();
    aNormals.assign(aPositions.size(), makeVec3(0.0f, 0.0f, 1.0f));
    aLambdas.assign(aConstraints.size(), 0.0f);
    aContactNormals.assign(aPositions.size(), makeVec3(0.0f, 0.0f, 0.0f));
    aContacted.assign(aPositions.size(), 0);
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
        for(size_t i = 0; i < aContacted.size(); ++i) aContacted[i] = 0;
        for(int iteration = 0; iteration < kSolveIterations; ++iteration) solveConstraints(h);
        solveColliders();
        updateVelocities(h);
        // 接触响应必须在速度更新**之后**：这一步的速度才是本子步的真实速度。
        // 即使不设摩擦也要调用 —— 见 resolveContactResponse 的说明（零恢复系数）。
        resolveContactResponse();
    }
    updateNormals();
}

// 接触响应：**零恢复系数 + 切向摩擦**。
//
// 为什么必须丢掉法向分量（而不是保留它）：
// 碰撞求解是把顶点"推到表面外"，于是 `v = (x - xPrev) / h` 会得到一个**人为的巨大外向速度**
// —— 顶点越深，推得越远，速度越大。保留它就会反复弹跳、能量不断累积。
// 实测：一块布落在板上、加一点风，300 步后横向位移跑到 **54 万米**（发散）。
//
// 正确做法就是"不弹"：法向速度清零，只留切向并按摩擦衰减。
// 这也是 restitution=0 的定义 —— 布料不该弹起来。
void cCLOTH::resolveContactResponse() {
    const float keep = 1.0f - aDesc.aFriction;
    for(size_t i = 0; i < aPositions.size(); ++i) {
        if(aContacted[i] == 0 || aInvMass[i] <= 0.0f) continue;
        GNg::sVEC3 normal;
        if(!normalizeVec(aContactNormals[i], normal)) continue;
        const GNg::sVEC3 &velocity = aVelocities[i];
        const float alongNormal = dotVec(velocity, normal);
        const GNg::sVEC3 tangential = subVec(velocity, scaleVec(normal, alongNormal));
        aVelocities[i] = scaleVec(tangential, keep); // 法向分量丢弃（不弹），切向衰减
    }
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
    if(aColliders.empty()) return;
    for(size_t i = 0; i < aPositions.size(); ++i) {
        if(aInvMass[i] <= 0.0f) continue;
        // 一个顶点可能同时挨着好几个碰撞体（比如胶囊 + 地面）：
        // 逐个推离，并把接触法线累加（摩擦只需要一个平均方向）。
        GNg::sVEC3 accumulated = makeVec3(0.0f, 0.0f, 0.0f);
        bool touched = false;
        for(size_t c = 0; c < aColliders.size(); ++c) {
            GNg::sVEC3 normal;
            if(!pushOutOfCollider(aColliders[c], aPositions[i], normal)) continue;
            accumulated = addVec(accumulated, normal);
            touched = true;
        }
        if(touched) {
            aContacted[i] = 1;
            aContactNormals[i] = accumulated;
        }
    }
}

// 圆点对球/胶囊/三角网的推离。返回 true 表示"在内部，已经推出来了"。
bool cCLOTH::pushOutOfCollider(const sCOLLIDER &collider, GNg::sVEC3 &position,
                               GNg::sVEC3 &outNormal) {
    switch(collider.aKind) {
    case eCLOTH_COLLIDER_SPHERE: {
        const GNg::sVEC3 delta = subVec(position, collider.aCenter);
        const float distance = lengthVec(delta);
        if(distance >= collider.aRadius) return false;
        if(distance < kClothEpsilon) {
            // 正好在球心：没有可用的方向，取一个固定方向（+Y）而不是除零。
            outNormal = makeVec3(0.0f, 1.0f, 0.0f);
        } else {
            outNormal = scaleVec(delta, 1.0f / distance);
        }
        position = addVec(collider.aCenter, scaleVec(outNormal, collider.aRadius));
        return true;
    }
    case eCLOTH_COLLIDER_CAPSULE: {
        // 到线段的最近点：胶囊 = "线段上每一点的球"的并集。
        const GNg::sVEC3 axis = subVec(collider.aTo, collider.aFrom);
        const float axisLengthSq = lengthSqVec(axis);
        float t = 0.0f;
        if(axisLengthSq > kClothEpsilon) {
            t = dotVec(subVec(position, collider.aFrom), axis) / axisLengthSq;
            if(t < 0.0f) t = 0.0f;
            if(t > 1.0f) t = 1.0f;
        }
        const GNg::sVEC3 closest = addVec(collider.aFrom, scaleVec(axis, t));
        const GNg::sVEC3 delta = subVec(position, closest);
        const float distance = lengthVec(delta);
        if(distance >= collider.aRadius) return false;
        if(distance < kClothEpsilon) {
            // 顶点正好在轴线上：沿"垂直于轴"的方向推出去，避免沿轴推（那会把人推穿）。
            GNg::sVEC3 perpendicular;
            if(!perpendicularTo(axis, perpendicular)) perpendicular = makeVec3(0.0f, 1.0f, 0.0f);
            outNormal = perpendicular;
        } else {
            outNormal = scaleVec(delta, 1.0f / distance);
        }
        position = addVec(closest, scaleVec(outNormal, collider.aRadius));
        return true;
    }
    case eCLOTH_COLLIDER_MESH: {
        if(collider.aMesh >= aMeshes.size()) return false;
        return pushOutOfMesh(aMeshes[collider.aMesh], position, outNormal);
    }
    }
    return false;
}

// 点对三角网：查所在格 + 3×3×3 邻域，取**穿透最深**的那个三角形。
// 取最深而不是第一个命中的原因：布料可能同时压着两个三角形（网格接缝处），
// 随手取第一个会让顶点在两个三角形之间来回弹。
bool cCLOTH::pushOutOfMesh(const sMESH_COLLIDER &mesh, GNg::sVEC3 &position,
                           GNg::sVEC3 &outNormal) {
    if(mesh.aIndices.size() < 3u) return false;
    int base[3];
    mesh.cellCoord(position, base);
    float bestPenetration = 0.0f;
    GNg::sVEC3 bestNormal = makeVec3(0.0f, 0.0f, 0.0f);
    GNg::sVEC3 bestTarget = position;
    // 邻域查重：同一个三角形可能落在多个格里，用"上次检查的三角形"去重不够可靠，
    // 所以这里允许重复检查（代价是少量重复距离计算，换来零状态）。
    for(int dz = -1; dz <= 1; ++dz) {
        for(int dy = -1; dy <= 1; ++dy) {
            for(int dx = -1; dx <= 1; ++dx) {
                const int cell = mesh.cellIndex(base[0] + dx, base[1] + dy, base[2] + dz);
                if(cell < 0) continue;
                const std::vector<uint32_t> &triangles = mesh.aCells[(size_t)cell];
                for(size_t t = 0; t < triangles.size(); ++t) {
                    // **这里是三角形下标，不是顶点下标**：必须先经 aIndices 取三个顶点索引。
                    // 第一版写成了 aVertices[triangle * 3]，等于把"第几个三角形"当成"第几个顶点"，
                    // 于是三角形的三个角被读成完全无关的三个顶点 —— 算出来的法线是 -Y，
                    // 布料被推到板子下面去（表现为"布穿地掉下去"，而不是"没检测到"）。
                    const uint32_t triangle = triangles[t];
                    const uint32_t i0 = mesh.aIndices[(size_t)triangle * 3u];
                    const uint32_t i1 = mesh.aIndices[(size_t)triangle * 3u + 1u];
                    const uint32_t i2 = mesh.aIndices[(size_t)triangle * 3u + 2u];
                    const GNg::sVEC3 &a = mesh.aVertices[i0];
                    const GNg::sVEC3 &b = mesh.aVertices[i1];
                    const GNg::sVEC3 &c = mesh.aVertices[i2];
                    // 点对三角形：要的是"在哪一侧、穿了多深"，所以用 queryPointTriangle。
                    // 不能用 radius=0 的 contact 查询：它内部按 (radius + 容差) 提前返回，
                    // 而半径 0 时那个阈值只有 1e-4，实际上永远测不到穿透
                    // —— 这正是第一版布料碰撞"看起来没穿地、其实一次都没碰上"的原因。
                    GNg::sVEC3 closest;
                    GNg::sVEC3 normal;
                    if(!queryPointTriangle(position, a, b, c, &closest, &normal)) continue;
                    const float signedDistance = dotVec(subVec(position, closest), normal);
                    if(signedDistance >= 0.0f) continue; // 在正向侧：没穿进去
                    // **只处理投影落在三角形内部的情况**。
                    // `closestPointOnTriangle` 对"footprint 之外"的点返回的是**边上**的最近点；
                    // 若把它也当穿透，就会把远处的顶点横向吸到三角形边缘 ——
                    // 实测：布料滑出地板边缘后被反复拽回，横向速度越积越大，
                    // 240 步后摊成 4 万米的"平面"。单向碰撞体的正确语义是：
                    // 点在 footprint 之外 ⇒ 根本没碰到这块三角形，让它继续掉。
                    const GNg::sVEC3 projected = subVec(position, scaleVec(normal, signedDistance));
                    if(lengthSqVec(subVec(projected, closest)) > kFootprintToleranceSq) continue;
                    const float penetration = -signedDistance;
                    if(penetration <= bestPenetration) continue;
                    bestPenetration = penetration;
                    bestNormal = normal;
                    bestTarget = addVec(closest, scaleVec(normal, kMeshSlop));
                }
            }
        }
    }
    if(bestPenetration <= 0.0f) return false;
    position = bestTarget;
    outNormal = bestNormal;
    return true;
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

void cCLOTH::setFriction(float friction) {
    if(!std::isfinite(friction)) return;
    if(friction < 0.0f) friction = 0.0f;
    if(friction > 1.0f) friction = 1.0f;
    aDesc.aFriction = friction;
}

bool cCLOTH::addSphereCollider(const GNg::sVEC3 &center, float radius) {
    if(!(radius > 0.0f) || !std::isfinite(radius)) return false;
    if(!finiteVec(center)) return false;
    sCOLLIDER collider;
    collider.aKind = eCLOTH_COLLIDER_SPHERE;
    collider.aCenter = center;
    collider.aRadius = radius;
    aColliders.push_back(collider);
    return true;
}

bool cCLOTH::addCapsuleCollider(const GNg::sVEC3 &from, const GNg::sVEC3 &to, float radius) {
    if(!(radius > 0.0f) || !std::isfinite(radius)) return false;
    if(!finiteVec(from) || !finiteVec(to)) return false;
    sCOLLIDER collider;
    collider.aKind = eCLOTH_COLLIDER_CAPSULE;
    collider.aFrom = from;
    collider.aTo = to;
    collider.aRadius = radius;
    // 端点重合时退化成一个球：记下中心走球分支，更快也更稳。
    if(lengthSqVec(subVec(to, from)) <= kClothEpsilon) {
        collider.aKind = eCLOTH_COLLIDER_SPHERE;
        collider.aCenter = from;
    }
    aColliders.push_back(collider);
    return true;
}

bool cCLOTH::addMeshCollider(const float *vertices, uint32_t vertexCount, const uint32_t *indices,
                             uint32_t indexCount) {
    if(vertices == 0 || indices == 0 || vertexCount < 3u || indexCount < 3u) return false;
    if((indexCount % 3u) != 0u) return false;
    for(uint32_t i = 0; i < vertexCount; ++i) {
        if(!finiteVec(makeVec3(vertices[i * 3u], vertices[i * 3u + 1u], vertices[i * 3u + 2u])))
            return false;
    }

    sMESH_COLLIDER mesh;
    mesh.aVertices.resize(vertexCount);
    for(uint32_t i = 0; i < vertexCount; ++i) {
        mesh.aVertices[i] = makeVec3(vertices[i * 3u], vertices[i * 3u + 1u], vertices[i * 3u + 2u]);
    }
    // 过滤越界索引与退化三角形：它们没有法线，留着只会白白参与距离计算。
    mesh.aIndices.reserve(indexCount);
    for(uint32_t t = 0; t + 2u < indexCount; t += 3u) {
        const uint32_t i0 = indices[t];
        const uint32_t i1 = indices[t + 1u];
        const uint32_t i2 = indices[t + 2u];
        if(i0 >= vertexCount || i1 >= vertexCount || i2 >= vertexCount) continue;
        if(i0 == i1 || i1 == i2 || i0 == i2) continue;
        const GNg::sVEC3 normal = crossVec(subVec(mesh.aVertices[i1], mesh.aVertices[i0]),
                                           subVec(mesh.aVertices[i2], mesh.aVertices[i0]));
        if(lengthSqVec(normal) <= kClothEpsilon) continue; // 三点共线
        mesh.aIndices.push_back(i0);
        mesh.aIndices.push_back(i1);
        mesh.aIndices.push_back(i2);
    }
    if(mesh.aIndices.size() < 3u) return false;

    // ── 包围盒 ──
    mesh.aMin = mesh.aVertices[0];
    mesh.aMax = mesh.aVertices[0];
    for(size_t i = 1; i < mesh.aVertices.size(); ++i) {
        const GNg::sVEC3 &p = mesh.aVertices[i];
        if(p.x < mesh.aMin.x) mesh.aMin.x = p.x;
        if(p.y < mesh.aMin.y) mesh.aMin.y = p.y;
        if(p.z < mesh.aMin.z) mesh.aMin.z = p.z;
        if(p.x > mesh.aMax.x) mesh.aMax.x = p.x;
        if(p.y > mesh.aMax.y) mesh.aMax.y = p.y;
        if(p.z > mesh.aMax.z) mesh.aMax.z = p.z;
    }

    // ── 均匀网格 ──
    // 格边长取"最长轴 / 32"，但**不小于布料顶点间距** —— 这样"查 3×3×3 邻域"是充分的，
    // 而不是碰运气的启发式（邻域半径 >= 一个格，所以任意可能接触的三角形都在邻域内）。
    float extent[3] = {mesh.aMax.x - mesh.aMin.x, mesh.aMax.y - mesh.aMin.y,
                       mesh.aMax.z - mesh.aMin.z};
    float longest = extent[0];
    if(extent[1] > longest) longest = extent[1];
    if(extent[2] > longest) longest = extent[2];
    float cellSize = longest / 32.0f;
    const float queryRadius = (aDesc.aSpacing > 0.0f) ? aDesc.aSpacing : 0.0f;
    if(cellSize < queryRadius) cellSize = queryRadius;
    if(!(cellSize > kClothEpsilon)) cellSize = 1.0f;
    mesh.aCellSize = cellSize;
    for(int axis = 0; axis < 3; ++axis) {
        int dim = (int)ceilf(extent[axis] / cellSize);
        if(dim < 1) dim = 1;
        if(dim > kMeshGridMaxDim) dim = kMeshGridMaxDim;
        mesh.aDim[axis] = dim;
    }
    const size_t cellCount = (size_t)mesh.aDim[0] * mesh.aDim[1] * mesh.aDim[2];
    mesh.aCells.assign(cellCount, std::vector<uint32_t>());

    // 三角形按覆盖的格登记。大三角形会跨很多格 —— 均匀网格的固有代价，
    // 但布料碰撞面对的通常是地面/桌面这类小三角形密布的几何，可接受。
    for(size_t t = 0; t < mesh.aIndices.size() / 3u; ++t) {
        GNg::sVEC3 triMin = mesh.aVertices[mesh.aIndices[t * 3u]];
        GNg::sVEC3 triMax = triMin;
        for(int corner = 1; corner < 3; ++corner) {
            const GNg::sVEC3 &p = mesh.aVertices[mesh.aIndices[t * 3u + (size_t)corner]];
            if(p.x < triMin.x) triMin.x = p.x;
            if(p.y < triMin.y) triMin.y = p.y;
            if(p.z < triMin.z) triMin.z = p.z;
            if(p.x > triMax.x) triMax.x = p.x;
            if(p.y > triMax.y) triMax.y = p.y;
            if(p.z > triMax.z) triMax.z = p.z;
        }
        int low[3];
        int high[3];
        mesh.cellCoord(triMin, low);
        mesh.cellCoord(triMax, high);
        for(int z = low[2]; z <= high[2]; ++z) {
            for(int y = low[1]; y <= high[1]; ++y) {
                for(int x = low[0]; x <= high[0]; ++x) {
                    const int cell = mesh.cellIndex(x, y, z);
                    if(cell < 0) continue;
                    mesh.aCells[(size_t)cell].push_back((uint32_t)t);
                }
            }
        }
    }

    aMeshes.push_back(mesh);
    sCOLLIDER collider;
    collider.aKind = eCLOTH_COLLIDER_MESH;
    collider.aMesh = (uint32_t)(aMeshes.size() - 1u);
    aColliders.push_back(collider);
    return true;
}

void cCLOTH::clearColliders() {
    aColliders.clear();
    aMeshes.clear();
}

uint32_t cCLOTH::meshColliderTriangleCount() const {
    uint32_t count = 0;
    for(size_t i = 0; i < aMeshes.size(); ++i) {
        count += (uint32_t)(aMeshes[i].aIndices.size() / 3u);
    }
    return count;
}

} // namespace GN3d
