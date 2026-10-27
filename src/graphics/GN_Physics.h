#pragma once
#ifndef _GN_PHYSICS_H_
#define _GN_PHYSICS_H_
// ╔══════════════════════════════════════════════════════════╗
// ║  GN_mini — 3D 刚体物理（自研最小实现，C++11，无新依赖）    ║
// ║  v1 范围：球 / 盒 / 胶囊，静态 / 动态 / 运动学，顺序冲量 + 位置修正 ║
// ╚══════════════════════════════════════════════════════════╝
//
// 契约（冻结）：
//   * 碰撞法线约定：`CollideBodies(A, B, out)` 的 `out.aNormal` **由 A 指向 B**，
//     穿透深度 >= 0（=0 表示仅接触）；A/B 的世界状态必须已由 `BodyUpdateWorldState` 更新。
//   * 形状（v1）：球 / 盒 / 胶囊；胶囊沿局部 Y 轴，质心在中点，aHalfExtent.y 为圆柱段半长。
//   * 刚体质量：`aType != eBODY_DYNAMIC` 时 `aInvMass = 0`、`aInvInertiaLocal/World` 全零；
//     运动学（kinematic）刚体的速度由调用方设置，世界按速度积分其位姿（也可直接改位姿），
//     物理只推动态刚体。
//   * 世界坐标：右手系、Y 向上；角度用 sQUAT；惯量张量用 sMAT44（列主序，局部对角）。
//   * 世界槽位：`AddBody` 返回的句柄在 `RemoveBody` 前保持稳定；`GetBody` 返回的指针
//     **仅在下一次 AddBody/RemoveBody 之前有效**（内部 vector 可能扩容）。
//   * 步长：`Step(dt)` 内部按固定步长累加（默认 1/60，最多 4 子步），dt 会被钳在 0.25s；
//     同一初始状态 + 同一 dt 序列 ⇒ 结果可重复。
//   * 休眠：动态刚体连续 0.5s 速度低于阈值即入睡；被运动中的刚体接触或调用方改速度即唤醒。
//   * 射线：`Raycast` 的 dir **必须归一化**；返回最近命中的刚体句柄（同 t 取较小句柄）。
//   * 空世界、无效句柄、零质量/零体积、重复 Remove 都不得崩溃。

#include "GN_3Dmath.h"
#include "GN_Spatial.h"
#include <vector>
#include <cstdint>

namespace GN3d {

enum eBODY_TYPE {
    eBODY_STATIC = 0, // 无限质量，不动
    eBODY_DYNAMIC = 1, // 受力/重力/碰撞
    eBODY_KINEMATIC = 2 // 无限质量，速度由调用方给定，推动态物体
};

enum eSHAPE_TYPE {
    eSHAPE_SPHERE = 0,
    eSHAPE_BOX = 1,
    eSHAPE_CAPSULE = 2 // 沿局部 Y 轴的胶囊：线段半长 aHalfExtent.y + 半径 aRadius
};

// ═══════════════════════════════════════════════════════════
//  sRIGID_BODY — 单个刚体
// ═══════════════════════════════════════════════════════════
struct sRIGID_BODY {
    eBODY_TYPE aType;
    eSHAPE_TYPE aShape;

    float aMass; // 千克；dynamic 必须 > 0
    float aInvMass; // 0 表示无限质量

    GNg::sVEC3 aPos; // 质心世界位置
    GNg::sQUAT aRot; // 世界朝向（单位四元数）
    GNg::sVEC3 aLinVel; // 线速度 m/s
    GNg::sVEC3 aAngVel; // 角速度 rad/s

    GNg::sMAT44 aInvInertiaLocal; // 局部逆惯量（对角）
    GNg::sMAT44 aInvInertiaWorld; // 世界逆惯量（每步由朝向算出）

    float aFriction; // 0..1（默认 0.5）
    float aRestitution; // 0..1（默认 0 = 不弹）
    float aLinearDamping; // 每步速度衰减系数（默认 0.02）
    float aAngularDamping; // 默认 0.05

    float aRadius; // 球/胶囊半径（eSHAPE_SPHERE / eSHAPE_CAPSULE，> 0）
    // 盒半尺寸（eSHAPE_BOX，各分量 > 0）；胶囊只用 aHalfExtent.y 作为圆柱段半长（>= 0），
    // 胶囊总半高 = aHalfExtent.y + aRadius，质心在胶囊中点。
    GNg::sVEC3 aHalfExtent;

    bool aSleeping;
    float aSleepTime; // 连续低速累计秒数
    sAABB aWorldAABB; // 每步更新的世界包围盒（broadphase）
};

// ═══════════════════════════════════════════════════════════
//  接触
// ═══════════════════════════════════════════════════════════
struct sCONTACT_POINT {
    GNg::sVEC3 aPoint; // 世界坐标接触点
    float aPenetration; // 穿透深度（>=0）
};

struct sCONTACT_MANIFOLD {
    GNg::sVEC3 aNormal; // 单位法线，**A → B**
    sCONTACT_POINT aPoints[4];
    uint32_t aCount; // 0..4
};

// 一条待求解的接触约束（求解器不拥有刚体；指针只在本次 Step 内有效）
struct sCONTACT_CONSTRAINT {
    sRIGID_BODY *aBodyA;
    sRIGID_BODY *aBodyB;
    sCONTACT_MANIFOLD aManifold;
    // 累计冲量：warm start 时由世界预填上一步的值，求解器沿新法线/切线重放后继续迭代；
    // 冷启动（warmStart=false）由求解器清零。
    float aAccNormal[4]; // 法向累计冲量
    float aAccTangent[4]; // 切向累计冲量
};

// ═══════════════════════════════════════════════════════════
//  刚体辅助
// ═══════════════════════════════════════════════════════════
sRIGID_BODY MakeSphereBody(eBODY_TYPE type, const GNg::sVEC3 &pos, float radius, float mass = 1.0f);
sRIGID_BODY MakeBoxBody(eBODY_TYPE type, const GNg::sVEC3 &pos, const GNg::sVEC3 &halfExtent,
                        float mass = 1.0f);
// 胶囊：沿局部 Y 轴，圆柱段半长 halfHeight，两端半球半径 radius
sRIGID_BODY MakeCapsuleBody(eBODY_TYPE type, const GNg::sVEC3 &pos, float radius, float halfHeight,
                            float mass = 1.0f);

// 由 shape + mass 计算 aInvMass / aInvInertiaLocal（会按 shape 校验参数）
void BodyUpdateInertia(sRIGID_BODY &body);
// 由 aPos/aRot 计算 aWorldAABB 与 aInvInertiaWorld（每步调用）
void BodyUpdateWorldState(sRIGID_BODY &body);

// ═══════════════════════════════════════════════════════════
//  窄相碰撞（无世界/宽相依赖，可单测）
// ═══════════════════════════════════════════════════════════
bool CollideBodies(const sRIGID_BODY &a, const sRIGID_BODY &b, sCONTACT_MANIFOLD &out);

// ═══════════════════════════════════════════════════════════
//  求解器（无世界依赖，可单测）
// ═══════════════════════════════════════════════════════════
// 顺序冲量：法向 + 摩擦，iterations 次迭代；不改位置。
// warmStart=true 表示调用方已把上一步的累计冲量预填到 aAccNormal/aAccTangent：
// 求解器先沿当前法线/切线施加这些冲量，再以该累计值继续迭代（不清零）。
void SolveVelocities(sCONTACT_CONSTRAINT *constraints, uint32_t count, float dt, uint32_t iterations,
                     bool warmStart = false);
// 位置修正（Baumgarte/伪速度式，只推位置，不改速度）
void SolvePositions(sCONTACT_CONSTRAINT *constraints, uint32_t count, uint32_t iterations);

// ═══════════════════════════════════════════════════════════
//  调试绘制（CPU 线段收集，渲染器消费；P8 接入 vkDebugDraw）
// ═══════════════════════════════════════════════════════════
struct sDEBUG_LINE {
    GNg::sVEC3 aStart;
    GNg::sVEC3 aEnd;
};

struct sDEBUG_DRAW {
    std::vector<sDEBUG_LINE> aLines;

    void clear() { aLines.clear(); }
    void addLine(const GNg::sVEC3 &start, const GNg::sVEC3 &end) {
        sDEBUG_LINE line;
        line.aStart = start;
        line.aEnd = end;
        aLines.push_back(line);
    }
};

// ═══════════════════════════════════════════════════════════
//  cPHYSICS_WORLD — 物理世界
// ═══════════════════════════════════════════════════════════
class cPHYSICS_WORLD {
  public:
    static const uint32_t INVALID_HANDLE = 0xFFFFFFFFu;

    cPHYSICS_WORLD();
    ~cPHYSICS_WORLD();

    void SetGravity(const GNg::sVEC3 &gravity); // 默认 (0,-9.81,0)
    void SetFixedTimestep(float step, uint32_t maxSubSteps); // 默认 1/60，4

    uint32_t AddBody(const sRIGID_BODY &body); // 返回句柄；失败返回 INVALID_HANDLE
    bool RemoveBody(uint32_t handle); // 无效句柄返回 false（不崩溃）
    sRIGID_BODY *GetBody(uint32_t handle);
    const sRIGID_BODY *GetBody(uint32_t handle) const;
    uint32_t GetBodyCount() const { return aBodyCount; }
    void Clear();

    void Step(float dt); // 固定步长 + 累加器

    // 最近命中的刚体（同 t 取较小句柄）；dir 必须归一化
    bool Raycast(const GNg::sVEC3 &origin, const GNg::sVEC3 &dir, float maxT, sRAY_HIT &hit) const;

    uint32_t GetContactCount() const { return aLastContactCount; } // 诊断
    uint32_t GetAwakeCount() const;

    // 唤醒指定刚体（无效句柄无操作）。调用方直接改速度后应调用它。
    void WakeBody(uint32_t handle);

    // 连续碰撞（CCD v1）：默认关闭。开启后每个子步用「上一位置 → 目标位置」的线段扫掠
    // 静态/运动学刚体的世界 AABB（按本刚体包围半径扩展），在最早 TOI 处钳住并清零进入方向的
    // 法向速度。近似边界见 GN_PhysicsCcd.cpp 文件头（保守扩展、不扫掠运动学目标的步内运动、
    // 不施加恢复系数、只处理动态且醒着的刚体）。
    void setContinuousCollision(bool enabled);
    bool getContinuousCollision() const { return aContinuousCollision; }

    // 物理调试线框：所有刚体的形状线框 + 上一步接触点/法线。draw 会先被清空。
    // 线数量口径：球 = 3×24，盒 = 12，胶囊 = 2×24 + 4；每个接触点额外 4 条（十字 + 法线）。
    void DebugDraw(sDEBUG_DRAW &draw) const;

  private:
    // 上一步的接触冲量缓存（warm start）：按刚体对 + 接触点位置匹配到本步的新流形。
    struct sWARM_CONTACT {
        uint32_t aBodyLo;
        uint32_t aBodyHi;
        uint32_t aPointCount;
        GNg::sVEC3 aPoints[4];
        float aAccNormal[4];
        float aAccTangent[4];
    };

    void StepOnce(float fixedDt);
    void IntegrateVelocities(float fixedDt);
    void IntegratePositions(float fixedDt);
    void BuildContacts();
    void UpdateSleep(float fixedDt);
    void seedWarmStart(uint32_t bodyLo, uint32_t bodyHi, sCONTACT_CONSTRAINT &constraint);
    void cacheContacts();
    void resolveContinuousCollisions();

    std::vector<sRIGID_BODY> aBodies;
    std::vector<uint8_t> aUsed;
    std::vector<uint32_t> aFreeHandles;
    std::vector<uint32_t> aTreeSlotOfBody; // 句柄 → broadphase 槽位
    std::vector<uint32_t> aBodyOfTreeSlot; // broadphase 槽位 → 句柄
    cDYNAMIC_AABB_TREE aBroadphase;
    std::vector<sCONTACT_CONSTRAINT> aConstraints;
    std::vector<uint32_t> aConstraintBodyLo; // 与 aConstraints 同索引的刚体对（小/大句柄）
    std::vector<uint32_t> aConstraintBodyHi;
    std::vector<sWARM_CONTACT> aWarmContacts; // 上一步接触冲量（本步 warm start 的输入）
    std::vector<GNg::sVEC3> aPreviousPositions; // 本子步积分前的质心位置（CCD 扫掠起点）
    bool aContinuousCollision; // CCD v1 开关（默认 false）
    GNg::sVEC3 aGravity;
    float aFixedStep;
    float aAccumulator;
    uint32_t aMaxSubSteps;
    uint32_t aBodyCount;
    uint32_t aLastContactCount;
};

} // namespace GN3d
#endif // _GN_PHYSICS_H_
