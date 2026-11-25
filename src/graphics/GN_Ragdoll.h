#pragma once
#ifndef _GN_RAGDOLL_H_
#define _GN_RAGDOLL_H_
// ╔══════════════════════════════════════════════════════════╗
// ║  GN_mini — 布娃娃（C3b，纯 CPU）                          ║
// ╚══════════════════════════════════════════════════════════╝
//
// 解决的问题：角色"死亡"或"被击飞"时，播放一段预制动画显得假 —— 倒地位置、
// 山坡坡度、撞到什么东西都不影响结果。布娃娃让身体由物理驱动。
//
// ═══ 设计：每根骨头一个刚体（默认球体）+ distance 关节 ═══
//
// 骨架（sSKELETON）给的是"关节树 + 绑定姿势"，布娃娃要的是"刚体 + 约束"。
// 映射方式：
//   * 每条**骨**（关节到其父的连线）⇒ 1 个刚体（质心在骨中点，沿骨方向）
//   * 相邻两骨 ⇒ 1 个 **distance 关节**，锚点在共享关节处
//
// 为什么默认**球体**而不是胶囊：胶囊又细又长，绕长轴的转动惯量 I ≈ m·r²/2 太小，
// 接触冲量产生的角速度会被放大几十倍（实测 56 rad/s、动能 220 → 2189 的数值爆炸）。
// 球体把半径放大到骨长的一半，惯量涨 7 倍以上，接触力矩不再失控。胶囊模式保留，
// 但只在"确实需要细长外观"时用，且要配合 ClampAngularSpeeds()。详见 sRAGDOLL_DESC。
//
// 为什么用 distance 而不是 hinge/fixed：
//   * fixed 太硬 —— 布娃娃应该软塌塌的。
//   * hinge 需要指定"哪个轴弯曲"，而 MMD/GLB 的骨朝向各式各样，自动化容易搞错。
//   * distance 只约束"锚点重合"，正是布娃娃最核心的一条：**骨长不变、位置连着**。
//     旋转完全自由 ⇒ 自然下垂、自然扭曲，视觉上最像"软掉的尸体"。
//
// 代价：没有关节角限制（膝盖能反向弯）。这是刻意的取舍 ——
// 引擎目前没有"带角限制的球窝关节"，硬加会引入一个大得多的求解器改动。
//
// ═══ 自碰撞 ═══
// 同一个布娃娃的所有刚体用同一个非零 aNoCollideGroup ⇒ **内部不互撞**。
// 必须如此：十几节刚体彼此紧挨，互撞会持续互相推挤、抖成一团；
// 而且形状本来就由关节约束维持，不需要自身碰撞。

#include "GN_3Danim.h"
#include "GN_3Dmath.h"
#include "GN_3Dskeleton.h"
#include "GN_Physics.h"

#include <cstdint>
#include <vector>

namespace GN3d {

struct sRAGDOLL_DESC {
    // 每根骨用**球体**还是**胶囊**。
    //
    // 默认球体：胶囊又细又长（半径 0.06、长 0.4），绕长轴的转动惯量小到
    // I ≈ m·r²/2，接触冲量产生的角速度被放大几十倍 —— 实测落地瞬间
    // 角速度冲到 56 rad/s、动能从 220 跳到 2189（数值爆炸）。
    // 球体把半径放大到"骨长的一半"，惯量涨 7 倍以上，接触力矩不再失控。
    // 视觉上球链也更接近"软塌塌的布娃娃"的经典表现。
    bool aUseSpheres;
    float aBoneRadius; // **最小半径**。球体模式取 max(aBoneRadius, 骨长/2)；胶囊模式直接用

    // 注意：aBoneRadius 不能太小。实测 0.06 时最短那根骨（0.126 m，球半径 0.063）
    // 会**穿过地面掉下去**（穿地 100 m，动能涨到 750）：单帧位移 0.08 m 已经接近
    // 球直径 0.126 m，再加上关节位置修正会把末端骨推过地面 —— 任何一层都可能漏掉。
    // 0.09（略大于单帧位移）就能稳住。默认值取 0.09。
    float aMassScale; // 质量 = 骨长 × aMassScale（默认 8 kg/m）
    float aJointStiffness; // 预留：关节软硬度（当前未用于 distance 约束）
    uint32_t aNoCollideGroup; // 非 0 时同组互不碰撞（默认自动分配）

    sRAGDOLL_DESC();
};

// 一根骨对应的刚体（与骨架的"子关节索引"一一对应）
struct sRAGDOLL_BONE {
    uint32_t aJoint; // 该骨的"子端"关节索引（骨 = 父关节 → 该关节）
    uint32_t aBody; // 物理世界里的句柄
    float aLength; // 静止长度（米）
    GNg::sVEC3 aLocalDir; // 骨方向（在刚体局部空间，恒为 +Y 或 -Y 的近似）
};

class cRAGDOLL {
  public:
    cRAGDOLL();
    ~cRAGDOLL();

    // 按骨架 + 绑定姿势建立布娃娃，刚体加入 world。
    // `localPose` 是当前姿势（用它决定初始位置/朝向；通常传绑定姿势或"死亡瞬间"的姿势）。
    // 成功返回 true；骨架为空/关节数 < 2 返回 false。
    bool Build(cPHYSICS_WORLD *world, const sSKELETON &skeleton,
               const std::vector<sANIM_TRANSFORM> &localPose, const sRAGDOLL_DESC &desc);

    // 销毁：把刚体/关节从 world 里移除（world 必须仍然有效）。
    void Destroy();

    bool IsBuilt() const { return !aBones.empty(); }
    uint32_t BoneCount() const { return (uint32_t)aBones.size(); }
    const sRAGDOLL_BONE &BoneAt(uint32_t index) const { return aBones[index]; }

    // 从物理世界读回刚体位姿，写进 `outLocal`（与骨架等长）。
    // 用途：把布娃娃的姿态喂给蒙皮（computePalette）。
    //
    // 口径：每根骨给出它**子端关节**的世界位置与朝向；再按骨架层级
    // （parentWorld^-1 * world）换算成局部变换。
    // 没有被任何骨覆盖的关节（根关节、没有子骨的末端）保持原样。
    //
    // 必须在 Build 之后调用（内部要用骨架的父索引）。骨架被销毁后不可调用。
    bool SyncPose(std::vector<sANIM_TRANSFORM> &outLocal) const;

    // 加一个"冲击"：给所有刚体施加瞬间速度（用于"被击飞"）。
    void ApplyImpulse(const GNg::sVEC3 &impulse);

    // 把角速度钳到合理上限（默认在 Step 之后调用；见实现里的说明）。
    void ClampAngularSpeeds();

    // 所有刚体都睡着（或速度极小）⇒ 布娃娃已静止。
    bool IsAtRest() const;

    // 总动能（J）。用于验收"无外力时能量不增"。
    float KineticEnergy() const;

    // 关节锚点最大误差（米）。用于验收"约束收敛"。
    float MaxJointError() const;

    // 当前所有刚体位置的世界 AABB（用于把相机对准布娃娃）。
    void WorldBounds(GNg::sVEC3 &outMin, GNg::sVEC3 &outMax) const;

  private:
    cPHYSICS_WORLD *aWorld;
    std::vector<sRAGDOLL_BONE> aBones;
    std::vector<uint32_t> aJoints; // 物理世界里的关节句柄
    std::vector<std::pair<uint32_t, uint32_t>> aBoneJoints; // (子骨索引, 父关节索引)
    uint32_t aNoCollideGroup;
    uint32_t aRootBody; // 根骨对应的刚体（用于 ApplyImpulse 的兜底）

    // Build 时拷贝的父索引（SyncPose 需要；骨架可能在使用期间被销毁）
    std::vector<int32_t> aParentOf;
};

} // namespace GN3d
#endif // _GN_RAGDOLL_H_
