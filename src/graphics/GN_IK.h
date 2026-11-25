#pragma once
#ifndef _GN_IK_H_
#define _GN_IK_H_
// ╔══════════════════════════════════════════════════════════╗
// ║  GN_mini — 两骨 IK（C3b，纯 CPU）                        ║
// ╚══════════════════════════════════════════════════════════╝
//
// 解决的问题：动画是"离线做好的"，落地时脚常常插进地面或悬空（尤其在不平的地面上）。
// IK 把"脚应该在的位置"作为目标，反解出大腿/小腿的旋转。
//
// 为什么是**两骨**：大腿 + 小腿（圆点 = 髋、膝、踝）是唯一能用解析解稳定求解的链。
// 三骨以上要么加约束（超定），要么迭代，那是另一套东西。
//
// 算法：余弦定理 + 旋转对齐。给定骨长 L1/L2 与目标距离 d：
//   * d > L1+L2  ⇒ 伸不直（目标不可达），钳到 L1+L2 并把 chain 拉向目标方向；
//   * d < |L1-L2| ⇒ 太近（目标在"腋下"），钳到 |L1-L2|；
//   * 中间段用余弦定理算膝角：cos(knee) = (L1²+L2²-d²)/(2·L1·L2)。
//
// **极向量（pole）**：两骨 IK 有一个自由度没被约束 —— 膝盖可以绕"髋→踝"那条轴
// 转一整圈。极点决定膝盖朝哪边弯（人的膝盖朝前）。没有它，膝会随机翻到后面。
//
// 与 `cANIM_LAYER_STACK` 的关系：IK 在**动画采样之后**作用，产出的是"覆盖后的
// 关节旋转"。它不修改 clip，只改这一帧的输出。

#include "GN_3Dmath.h"
#include "GN_3Danim.h" // sANIM_TRANSFORM（IK 读的是动画输出的姿势）
#include "GN_3Dskeleton.h" // sSKELETON / sJOINT

#include <cstdint>
#include <vector>

namespace GN3d {

// ═══════════════════════════════════════════════════════════
//  两骨链的输入 / 输出
// ═══════════════════════════════════════════════════════════
struct sIK_CHAIN {
    GNg::sVEC3 aRootPos; // 髋（链的起点，由父级决定，IK 不改它）
    GNg::sVEC3 aJointPos; // 膝（当前值，IK 会重算）
    GNg::sVEC3 aEndPos; // 踝（当前值，IK 会重算）
    GNg::sVEC3 aTarget; // 希望踝到达的位置
    GNg::sVEC3 aPole; // 极向量（膝朝哪边弯；通常传"角色前方"）
    float aWeight; // 0 = 完全不生效（动画原样），1 = 完全IK

    sIK_CHAIN();
};

struct sIK_RESULT {
    GNg::sVEC3 aJointPos; // 解出的膝位置
    GNg::sVEC3 aEndPos; // 解出的踝位置（可达时 == 目标）
    bool aReachable; // 目标是否可达（false 表示被钳到骨长范围内）

    sIK_RESULT();
};

// 解一条两骨链。boneA 从 root 到 joint、boneB 从 joint 到 end；
// 骨长由**当前姿势**推出来（不另外传参，避免与骨架不一致）。
//
// 返回解的膝/踝位置。`aWeight` 会是插值权重：0 时原样返回当前 joint/end。
sIK_RESULT solveTwoBoneIK(const sIK_CHAIN &chain);

// ═══════════════════════════════════════════════════════════
//  四元数工具（IK 需要，引擎里此前没有乘法）
// ═══════════════════════════════════════════════════════════
// 语法糖包装（实现直接调 GNg::QuatMul，保证只有一份实现）
// 从向量 from 转到 to 的最短弧旋转。两者都为零/共线时返回单位四元数（安全）。
GNg::sQUAT quatFromTo(const GNg::sVEC3 &from, const GNg::sVEC3 &to);

// ═══════════════════════════════════════════════════════════
//  骨架级的 IK 求解器
// ═══════════════════════════════════════════════════════════
// 用法（每帧，在动画采样之后、computePalette 之前）：
//
//   cIK_SOLVER ik;
//   ik.SetSkeleton(&skeleton);
//   sIK_TARGET t;
//   sIK_CHAIN chain;
//   if (ik.Build(targetJoint, targetPos, pole, weight, chain)) {
//       // chain 给出了正确的膝/踝位置；再用 solveTwoBoneIK 或直接写回
//   }
//
// 这里只做"从层级姿势里取出这条链"，反解本身仍是 `solveTwoBoneIK` 一个纯函数 ——
// 拆开的好处是：纯函数可以单测到每个边界，层级部分只是搬运。
class cIK_SOLVER {
  public:
    cIK_SOLVER();

    void SetSkeleton(struct sSKELETON *skeleton); // 仅用于取父子关系；可为空
    const struct sSKELETON *Skeleton() const;

    // 从 local 姿势算出世界位置（不改姿势，只读）。out 与 local 等长。
    bool ComputeWorldPositions(const std::vector<GN3d::sANIM_TRANSFORM> &local,
                               std::vector<GNg::sVEC3> &outWorldPos) const;

    // 从层级里取出"以 endJoint 为末端的两骨链"。
    // 需要的关节：endJoint 的父（膝）与祖父（髋）必须存在。
    bool BuildChain(const std::vector<GN3d::sANIM_TRANSFORM> &local, uint32_t endJoint,
                    const GNg::sVEC3 &targetWorld, const GNg::sVEC3 &poleWorld, float weight,
                    sIK_CHAIN &out) const;

  private:
    struct sSKELETON *aSkeleton; // 非拥有
};

} // namespace GN3d
#endif // _GN_IK_H_
