#pragma once
#ifndef _GN_LOCOMOTION_H_
#define _GN_LOCOMOTION_H_
// ╔══════════════════════════════════════════════════════════╗
// ║  GN_mini — 移动表现：选段 + 转向（纯 CPU，无 Vulkan）    ║
// ╚══════════════════════════════════════════════════════════╝
//
// 解决的问题：任何第三人称控制器都要做同样两件事，而这两件事的**阈值和折算关系**
// 一直是硬编码在演示程序里的：
//
//   * 按水平速度选「待机 / 走 / 跑」——阈值 0.4 / 4.2 m/s；
//   * 播放速率跟着速度走（walk 对应 3.0、run 对应 5.5 m/s），否则脚会打滑；
//   * 朝向按移动方向平滑转过去（指数衰减，帧率无关）。
//
// 这些数字一旦散落在各处，就会出现"角色 A 走路脚打滑、角色 B 不打滑"这种
// 没人说得清的问题 —— 因为两处的参考速度取了不同的值。
//
// 这一层只做纯计算：不读输入、不碰模型、不碰渲染。
//
// ═══ 朝向约定 ═══
// 模型正前方取 **+Z**（`forward = (sin yaw, 0, cos yaw)`），因此
// `yaw = atan2(dir.x, dir.z)`。这与 `GN_Space.h` 的"+Z 资产正面"一致。
// 如果你的模型正面是 -Z，不要在调用处偷偷加 pi —— 关掉它造成的偏移会让
// "输入方向"与"看起来的方向"差 180°，调试起来像在抓鬼。
//
// ═══ 为什么播放速率要钳制 ═══
// 速度趋近 0 时 `speed / walkSpeed` 也趋近 0，动画会停住 —— 但角色还在缓慢移动，
// 看起来就是"贴图静止、整个人在滑"。所以有一个下限（默认 0.55）；
// 上限（默认 1.8）防止超速时腿甩成电风扇。

#include <GN.h> // 必须先于本组头：GN_API 等基础宏
#include "GN_3Dmath.h"

namespace GN3d {

enum eLOCOMOTION_CLIP {
    eLOCOMOTION_IDLE = 0,
    eLOCOMOTION_WALK,
    eLOCOMOTION_RUN,
    eLOCOMOTION_CLIP_COUNT
};

// 选段与转向用到的全部参数。默认值取自 imoScene 调过的手感，可直接用。
struct sLOCOMOTION_DESC {
    float aIdleSpeed; // 水平速度 < 它 → 待机（默认 0.4）
    float aRunSpeed; // 水平速度 > 它 → 跑（默认 4.2）
    float aWalkRef; // 走路的参考速度，用于折算播放速率（默认 3.0）
    float aRunRef; // 跑的参考速度（默认 5.5）
    float aTurnRate; // 转向速度（rad/s，默认 14）
    float aMinPlayRate; // 播放速率下限（默认 0.55）
    float aMaxPlayRate; // 播放速率上限（默认 1.8）

    sLOCOMOTION_DESC();
    // 参数是否自洽：阈值非负且 idle < run、参考速度 > 0、上下限 0 < min <= max。
    // 不自洽时 selectLocomotionClip 仍会给出结果（退化处理），但应当先修正参数。
    bool IsValid() const;
};

// 按水平速度选段。speed <= aIdleSpeed → IDLE；<= aRunSpeed → WALK；否则 RUN。
// speed 非有限（NaN/Inf）时按 IDLE 处理 —— 宁可站着，也不要让 NaN 决定播哪段。
eLOCOMOTION_CLIP selectLocomotionClip(float speed, const sLOCOMOTION_DESC &desc);

// 播放速率：IDLE 恒为 1；WALK 用 speed / aWalkRef；RUN 用 speed / aRunRef。
// 结果钳在 [aMinPlayRate, aMaxPlayRate]。
float locomotionPlayRate(float speed, eLOCOMOTION_CLIP clip, const sLOCOMOTION_DESC &desc);

// 段的可读名（日志/HUD 用）。
const char *locomotionClipName(eLOCOMOTION_CLIP clip);

// 把角度折到 **[-pi, pi)**。用于"转向时不要绕远路"。
// 注意区间是左闭右开：+pi 会被折成 -pi（同一个方向，但用 == 比较时不一样）。
float wrapRadians(float angle);

// 由水平方向求 yaw；方向长度接近 0 时返回 false（此时没有"朝向"可言）。
bool yawFromDirection(float dirX, float dirZ, float *outYaw);

// 朝目标 yaw 平滑靠近，返回新的 yaw。
// 用 `blend = 1 - exp(-turnRate * dt)` 做指数衰减：
//   * 帧率无关（把 dt 拆成两半跑两次，结果 ≈ 跑一次）；
//   * **永不越过目标**（blend ∈ [0,1)），所以不会左右抖动。
// `turnRate <= 0` 或 `dt <= 0` 时原样返回 currentYaw。
float approachYaw(float currentYaw, float targetYaw, float turnRate, float dt);

// 朝向状态（调用方持有）。
struct sFACING_STATE {
    float aYaw; // 当前朝向（弧度）

    sFACING_STATE();
};

// 一步更新朝向：优先用**实际速度方向**（走起来以后就该朝实际移动方向），
// 速度太小时退回到**输入意图**（刚起步也能立刻转身，不必等速度积累）。
// 两者都太小则保持不变。
//
// 之所以要这个"速度优先、意图兜底"的顺序：
//   * 只用意图 → 被墙挡住时角色会朝着墙"转身"；
//   * 只用速度 → 刚按键那一瞬间速度为 0，角色先原地不动一小会儿才转身，手感发粘。
void updateFacing(sFACING_STATE &state, const GNg::sVEC3 &velocity, const GNg::sVEC3 &wish,
                  float dt, const sLOCOMOTION_DESC &desc);

} // namespace GN3d
#endif // _GN_LOCOMOTION_H_
