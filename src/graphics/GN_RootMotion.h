#pragma once
#ifndef _GN_ROOT_MOTION_H_
#define _GN_ROOT_MOTION_H_
// ╔══════════════════════════════════════════════════════════╗
// ║  GN_mini — Root Motion（C3，纯 CPU）                     ║
// ╚══════════════════════════════════════════════════════════╝
//
// 解决的问题：没有 root motion 时，角色的位移是**代码算出来的**
// （`wishDir * walkSpeed * dt`），动画只是"看起来在走"。
// 后果是脚一定会滑：动画里每步迈 0.7 m、而代码每秒推 2.6 m，
// 两个数字独立漂移，任何一格速度档都对不齐。
//
// root motion 反过来：**位移来自动画本身**。每帧采样根关节的平移，
// 算出"这一帧动画让角色走多远"，把那一段位移交给 `cCHARACTER` 去碰撞求解。
// 于是脚与地面天然同步（动画师调好的步幅就是实际步幅）。
//
// 本文件的三个职责：
//   1. `sROOT_MOTION_EXTRACTOR` —— 从两帧的根关节变换里**提取**位移与转角；
//   2. 阈值/钳制 —— 丢弃"瞬移型"跳变（动画里换片段时常见），避免角色被弹飞；
//   3. 与 `cCHARACTER` 的桥 —— 把"本帧该走多远"换成一次 `Move` 调用。
//
// 与 `cANIM_BLEND_1D` 的关系：混合树的输出就是这里要提取的根变换。
// 用 `cANIM_LAYER_STACK::Sample(rootJoint, ...)` 拿到本帧的根姿势即可。

#include "GN_3Dmath.h"
#include "GN_3Danim.h"

#include <cstdint>

namespace GN3d {

// ═══════════════════════════════════════════════════════════
//  一帧的 root motion
// ═══════════════════════════════════════════════════════════
struct sROOT_MOTION {
    GNg::sVEC3 aDeltaPosition; // 本帧的世界位移（**已**包含朝向旋转）
    float aDeltaYaw; // 本帧绕 Y 的转角（弧度；右手系，逆时针为正）
    bool aValid; // 是否提取到了有效数据

    sROOT_MOTION();
};

// ═══════════════════════════════════════════════════════════
//  提取器
// ═══════════════════════════════════════════════════════════
// 用法（每帧一次，在动画 Update 之后、角色 Move 之前）：
//
//   sROOT_MOTION motion;
//   if (extractor.Extract(rootTransform, &motion)) {
//       character.ApplyRootMotion(motion.aDeltaPosition, dt);
//       character.RotateYaw(motion.aDeltaYaw);
//   }
//
// 口径（每条都有用例）：
//
//   * **位移在原地坐标系里提取，再按"当前朝向"旋转到世界**。
//     动画的根关节位移通常是"朝角色前方走"的值；角色转了 90° 后，
//     同一个动画应当把角色往新朝向前方推。所以 `SetFacingYaw` 必须由调用方同步。
//
//   * **首帧只记录、不产生位移**。否则播第一帧时角色会从动画起点"跳"到根关节位置
//     （很多 clip 的根关节在第 0 帧并不在原点）。
//
//   * **超过 `aMaxStep` 的位移视为瞬移，整帧丢弃**。切换 clip / 循环回绕时
//     根关节可能出现大幅跳变；让它进物理会把角色弹飞。
//
//   * **转角同样钳制到 ±`aMaxYawStep`**，理由同上。
class cROOT_MOTION_EXTRACTOR {
  public:
    cROOT_MOTION_EXTRACTOR();

    // 清零历史（换 clip / 传送后必须调，否则下一帧会把"传送"当成位移）。
    void Reset();

    // 朝向（弧度）。提取时用它把局部位移转到世界。
    void SetFacingYaw(float yawRad);
    float FacingYaw() const;

    // 单帧最大位移（米）与最大转角（弧度）。超过视为瞬移并丢弃该帧。
    // 默认 2.0 m / 4.0 rad（约 230°）—— 一帧走出 2 m 只可能是跳变。
    void SetLimits(float maxStep, float maxYawStep);
    float MaxStep() const;
    float MaxYawStep() const;

    // ── 循环回绕（**循环型 clip 必须设**）──
    // 走路/跑步 clip 通常做成"根关节从 0 走到 strideLength，然后回绕到 0"。
    // 处理不当会有两种错误：
    //   * 丢掉回绕那一帧 → 角色每循环一次就"少走一步"，累计成明显变慢
    //     （实测 4 秒只走 0.03 m）；
    //   * 当成正常位移 → 回绕那一帧根关节瞬移 -strideLength，角色被倒退一步。
    // 正确做法：告诉提取器 strideLength；检测到"位移 ≈ -strideLength"时补上
    // strideLength（等价于"这一圈走完了，继续往前走"）。
    //
    // strideLength <= 0 表示关闭（默认关闭：原地动画不该补偿）。
    void SetLoopStride(float strideLength);
    float LoopStride() const;
    // 累计检测到多少次循环回绕（诊断/验收）。
    uint32_t WrapCount() const;

    // 提取。root 是本帧根关节的局部变换（来自混合树 / clip 采样）。
    // 返回 false 表示"这一帧不该产生位移"：
    //   * 首帧（只记录）；
    //   * 位移/转角超过上限（瞬移，已丢弃并重新记录）；
    //   * 输入非有限值。
    // 返回 true 时 out 填好，且内部历史已前移。
    bool Extract(const sANIM_TRANSFORM &root, sROOT_MOTION *out);

    // 只记录不产生位移（用于"换了 clip 但想保留历史"的场合，少见）。
    void Prime(const sANIM_TRANSFORM &root);

    bool HasHistory() const;

  private:
    GNg::sVEC3 aLastPosition;
    GNg::sQUAT aLastRotation;
    float aFacingYaw;
    float aMaxStep;
    float aMaxYawStep;
    float aLoopStride; // 0 = 不做循环补偿
    uint32_t aWrapCount;
    bool aHasHistory;
};

// ═══════════════════════════════════════════════════════════
//  工具：四元数 → 绕 Y 的偏航角
// ═══════════════════════════════════════════════════════════
// 只取绕 Y 的分量（root motion 通常只关心偏航；俯仰/翻滚由骨骼自身表现）。
// 返回 [-pi, pi]。
float quatYaw(const GNg::sQUAT &q);

// 把局部位移按偏航角旋转到世界（y 分量原样保留）。
GNg::sVEC3 rotateYaw(const GNg::sVEC3 &v, float yawRad);

} // namespace GN3d
#endif // _GN_ROOT_MOTION_H_
