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
//   * 外力：v += (aGravity + aWind) * h（风当作加速度，不做面积/法线投影 —— 见下方边界）。
//   * 碰撞：球 / 胶囊 / 静态三角网三种（`addSphereCollider` / `addCapsuleCollider` /
//     `addMeshCollider`）。顶点被推到表面外，并按 `aFriction` 衰减切向速度。
//   * 所有输出指针在下一次 Create/Reset 前有效；未 Create 时返回 0。
//
// ═══ 碰撞的边界（先说清楚，避免按想象使用）═══
//
//   * **点碰撞，不是面碰撞**：每个布料顶点当作半径 = 0 的点去测穿透。
//     所以薄片（比如一张纸）可以被布料的顶点"漏穿"；真实布料是连续曲面。
//     补偿办法是 `aSpacing` 取小 + 子步多，而不是指望这里做连续碰撞。
//   * **单向**：布料不会推动碰撞体（碰撞体是无限质量的静态几何）。
//     所以它适合"布挂在角色身上 / 落在桌面上"，不适合"布压弯一根绳子"。
//   * **三角网是静态的**：`addMeshCollider` 复制一份顶点，之后碰撞体动了布料不会跟着变。
//     需要跟着动的物体请用胶囊（`addCapsuleCollider` 每次重建即可，代价很小）。
//   * **法线朝外**：三角网的法线由三角绕序决定（右手、CCW 为正面）。绕序反了，
//     布料会被推到三角形背面 —— 表现是"布从地面掉下去"。这是最常见的接线错误。
//   * **摩擦是速度衰减，不是静摩擦**：接触时把切向速度乘以 (1 - aFriction)。
//     所以 aFriction=1 时布料不会沿切向滑动，但仍会因重力法向分量而"贴住"。
//     真正的静摩擦（需要区分粘着/滑动）不在这里做。
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
    // 切向摩擦系数（0..1）。接触时切向速度乘以 (1 - aFriction)。
    // 默认 **0**（保持 v1 的"无摩擦"行为，老代码与老测试不受影响）；
    // 实际场景建议 0.2~0.5 —— 否则布料落在球/桌面上会一直往下滑。
    float aFriction;
};

// 碰撞体种类。三角网碰撞体内部持有顶点副本（不与调用方共享所有权）。
enum eCLOTH_COLLIDER_KIND {
    eCLOTH_COLLIDER_SPHERE = 0,
    eCLOTH_COLLIDER_CAPSULE,
    eCLOTH_COLLIDER_MESH,
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
    void setFriction(float friction); // 钳到 [0,1]
    float friction() const { return aDesc.aFriction; }

    bool addSphereCollider(const GNg::sVEC3 &center, float radius); // 半径 <= 0 返回 false
    // 胶囊：线段 from→to 加半径。from == to 等价于球。半径 <= 0 返回 false。
    bool addCapsuleCollider(const GNg::sVEC3 &from, const GNg::sVEC3 &to, float radius);
    // 静态三角网：vertices = 每顶点 3 个 float 的扁平数组（世界坐标），
    // indices 每 3 个一组。索引越界/退化三角形会被跳过；一个有效三角形都没有则返回 false。
    // **法线方向由绕序决定（CCW 为正面）**，绕反了布料会被推到背面。
    bool addMeshCollider(const float *vertices, uint32_t vertexCount, const uint32_t *indices,
                         uint32_t indexCount);
    void clearColliders();
    uint32_t colliderCount() const { return (uint32_t)aColliders.size(); }
    // 各类型的数量（诊断用：确认"我加的三角网到底进去了没有"）。
    uint32_t meshColliderTriangleCount() const;

  private:
    struct sDISTANCE {
        uint32_t aVertexA;
        uint32_t aVertexB;
        float aRestLength;
    };

    struct sCOLLIDER {
        eCLOTH_COLLIDER_KIND aKind;
        GNg::sVEC3 aCenter; // 球心
        GNg::sVEC3 aFrom; // 胶囊端点
        GNg::sVEC3 aTo;
        float aRadius;
        uint32_t aMesh; // 三角网下标（aMeshes 内）

        sCOLLIDER();
    };

    // 三角网 + 均匀网格加速（布料顶点数上万时不能对三角形两两暴力）。
    struct sMESH_COLLIDER {
        std::vector<GNg::sVEC3> aVertices;
        std::vector<uint32_t> aIndices; // 已过滤掉越界/退化，每 3 个一组
        GNg::sVEC3 aMin;
        GNg::sVEC3 aMax;
        float aCellSize; // 单格边长（>= 最大查询半径，保证查 3×3×3 邻域不漏）
        int aDim[3];
        std::vector<std::vector<uint32_t>> aCells; // 三角下标

        sMESH_COLLIDER();
        int cellIndex(int x, int y, int z) const;
        void cellCoord(const GNg::sVEC3 &point, int out[3]) const;
    };

    void buildGrid();
    void buildConstraints();
    void integrateVelocities(float step);
    void solveConstraints(float step);
    void solveColliders();
    void updateVelocities(float step);
    void updateNormals();
    void resolveContactResponse();
    // 单点对单个碰撞体：返回 true 表示发生了推送，outNormal 写入接触法线（朝外）。
    bool pushOutOfCollider(const sCOLLIDER &collider, GNg::sVEC3 &position, GNg::sVEC3 &outNormal);
    bool pushOutOfMesh(const sMESH_COLLIDER &mesh, GNg::sVEC3 &position, GNg::sVEC3 &outNormal);

    sCLOTH_DESC aDesc;
    std::vector<GNg::sVEC3> aPositions;
    std::vector<GNg::sVEC3> aPrevious;
    std::vector<GNg::sVEC3> aVelocities;
    std::vector<GNg::sVEC3> aNormals;
    std::vector<uint32_t> aIndices;
    std::vector<float> aInvMass;
    std::vector<sDISTANCE> aConstraints;
    std::vector<float> aLambdas; // XPBD 每子步重置
    std::vector<sCOLLIDER> aColliders;
    std::vector<sMESH_COLLIDER> aMeshes;
    // 每个顶点本子步累积的接触法线（摩擦用，求解前清零）。
    std::vector<GNg::sVEC3> aContactNormals;
    std::vector<uint8_t> aContacted;
};

} // namespace GN3d
#endif // _GN_3DCLOTH_H_
