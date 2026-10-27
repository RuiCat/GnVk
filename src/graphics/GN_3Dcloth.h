#pragma once
#ifndef _GN_3DCLOTH_H_
#define _GN_3DCLOTH_H_
// ╔══════════════════════════════════════════════════════════╗
// ║  GN_mini — 布料（XPBD，CPU 模拟；GPU 上传/绘制留 P8/P9）  ║
// ╚══════════════════════════════════════════════════════════╝
//
// 契约：
//   * 布料是一张 (nx × ny) 的规则网格，初始位于 XY 平面（z = 0），顶点 (i,j) 在
//     (i*spacing, j*spacing, 0)；三角形索引 (nx-1)*(ny-1)*2 个、每三角形 3 个索引，绕序一致。
//   * `aPinCorners=true` 时固定四个角（invMass = 0）：这些顶点在 Step 后位置不变。
//   * XPBD 距离约束：结构（横/竖）、剪切（两条对角线）、弯曲（间隔 2 的横/竖）；
//     `aCompliance` 为柔度（0 = 刚性），内部固定 8 次约束迭代、`aSubsteps` 个子步。
//   * 外力：v += (aGravity + aWind) * h（v1 把风当作加速度，不做面积/法线投影）。
//   * 碰撞：只支持球体（`addSphereCollider`），把进入球内的顶点推到球面；不处理摩擦。
//   * 所有输出指针在下一次 Create/Reset 前有效；未 Create 时返回 0。
#include "GN_3Dmath.h"

#include <cstdint>
#include <vector>

namespace GN3d {

struct sCLOTH_DESC {
    int aNx; // 顶点列数（>= 2）
    int aNy; // 顶点行数（>= 2）
    float aSpacing; // 顶点间距（> 0）
    float aMass; // 每个顶点质量（> 0；固定点不参与）
    float aCompliance; // XPBD 柔度（>= 0；0 = 刚性）
    int aSubsteps; // 子步数（>= 1；内部钳到 1..16）
    GNg::sVEC3 aGravity; // 默认 (0,-9.81,0)
    GNg::sVEC3 aWind; // 默认 (0,0,0)
    bool aPinCorners; // 固定四个角
};

struct sCLOTH_COLLIDER {
    GNg::sVEC3 aCenter;
    float aRadius;
};

class cCLOTH {
  public:
    cCLOTH();
    ~cCLOTH();

    bool create(const sCLOTH_DESC &desc); // 失败返回 false（参数非法），不修改既有布料
    void reset(); // 回到初始网格与零速度（保留碰撞球）
    bool isCreated() const { return !aPositions.empty(); }
    void step(float dt); // 固定子步积分（dt 钳在 0.1s 内）

    uint32_t vertexCount() const { return (uint32_t)aPositions.size(); }
    uint32_t triangleCount() const { return (uint32_t)aIndices.size() / 3u; }
    const GNg::sVEC3 *positions() const { return aPositions.empty() ? 0 : &aPositions[0]; }
    const GNg::sVEC3 *normals() const { return aNormals.empty() ? 0 : &aNormals[0]; }
    const uint32_t *indices() const { return aIndices.empty() ? 0 : &aIndices[0]; }

    void setWind(const GNg::sVEC3 &wind) { aDesc.aWind = wind; }
    bool addSphereCollider(const GNg::sVEC3 &center, float radius); // 半径 <= 0 返回 false
    void clearColliders() { aColliders.clear(); }
    uint32_t colliderCount() const { return (uint32_t)aColliders.size(); }

  private:
    struct sDISTANCE {
        uint32_t aVertexA;
        uint32_t aVertexB;
        float aRestLength;
    };

    void buildGrid();
    void buildConstraints();
    void integrateVelocities(float step);
    void solveConstraints(float step);
    void solveColliders();
    void updateVelocities(float step);
    void updateNormals();

    sCLOTH_DESC aDesc;
    std::vector<GNg::sVEC3> aPositions;
    std::vector<GNg::sVEC3> aPrevious;
    std::vector<GNg::sVEC3> aVelocities;
    std::vector<GNg::sVEC3> aNormals;
    std::vector<uint32_t> aIndices;
    std::vector<float> aInvMass;
    std::vector<sDISTANCE> aConstraints;
    std::vector<float> aLambdas; // XPBD 每子步重置
    std::vector<sCLOTH_COLLIDER> aColliders;
};

} // namespace GN3d
#endif // _GN_3DCLOTH_H_
