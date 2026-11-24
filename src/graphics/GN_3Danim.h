#pragma once
#ifndef _GN_3DANIM_H_
#define _GN_3DANIM_H_
// ╔══════════════════════════════════════════════════════════╗
// ║  GN_mini — 3D 动画（clip 采样 / 混合 / 简单状态机，CPU）  ║
// ╚══════════════════════════════════════════════════════════╝
//
// 契约：
//   * clip = 若干通道（节点 + 路径 T/R/S + 关键帧时间/值）；时间轴严格递增，采样时钳在 [0, duration]。
//   * 插值：STEP（取前一关键帧）与 LINEAR（T/S 线性、R 用最短弧 slerp）；CUBICSPLINE 未实现（P3 v1 边界）。
//   * 未被任何通道覆盖的节点：`sANIM_CLIP::sample` 返回 false、不修改 out。
//   * `cANIM_PLAYER`：play / crossFade / update / sample；crossFade 期间按线性权重混合两段 clip，
//     某段 clip 没有该节点通道时直接采用另一段的结果；loop 时按 duration 回绕，非 loop 到末尾停住。
//   * 时间轴由调用方驱动（`update(dt)`），本层不依赖 glTF/骨骼/渲染（P2 导入、P8 GPU 蒙皮）。
#include "GN_3Dmath.h"

#include <cstdint>
#include <vector>

namespace GN3d {

enum eANIM_PATH {
    eANIM_TRANSLATION = 0,
    eANIM_ROTATION = 1,
    eANIM_SCALE = 2
};

enum eANIM_INTERP {
    eANIM_STEP = 0,
    eANIM_LINEAR = 1 // CUBICSPLINE 未实现
};

struct sANIM_CHANNEL {
    uint32_t aNode;
    eANIM_PATH aPath;
    eANIM_INTERP aInterp;
    std::vector<float> aTimes; // 严格递增
    std::vector<GNg::sVEC3> aVecValues; // T/S 的关键帧值
    std::vector<GNg::sQUAT> aQuatValues; // R 的关键帧值
};

// ── 状态机的外部输入（P3）──────────────────────────────────
// 放在本头而不是状态机头里：`cENTITY` 按值持有参数包，而 GN_Entity.h 不能包含
// GN_AnimStateMachine.h（会形成 GN.h ⇄ GN_Entity.h 的循环包含）。参数类型本身也只
// 依赖动画层，放这里在语义上是对的。
//
// 参数编号是 uint16_t 而非枚举：引擎不预设游戏有哪些参数（否则引擎要随每个游戏改）。
typedef uint16_t eANIM_PARAM;
const uint16_t ANIM_PARAM_MAX = 64; // 0 保留为"无参数"

struct sANIM_PARAMS {
    float aValue[ANIM_PARAM_MAX];  // 数值参数（未设置 = 0）
    bool aTrigger[ANIM_PARAM_MAX]; // 触发器（"本帧"语义；状态机每帧末清空）

    sANIM_PARAMS();
    void Reset(); // 数值与触发器全清（切场景时用）
    void Set(eANIM_PARAM id, float value);
    float Get(eANIM_PARAM id) const;
    void Trigger(eANIM_PARAM id); // 置位（同一帧重复置位无副作用）
    bool Triggered(eANIM_PARAM id) const;
};

struct sANIM_TRANSFORM {
    GNg::sVEC3 aTranslation;
    GNg::sQUAT aRotation;
    GNg::sVEC3 aScale;
};

struct sANIM_CLIP {
    std::vector<sANIM_CHANNEL> aChannels;
    float aDuration; // 最大关键帧时间（updateDuration 刷新）

    // 采样节点 node 在 time 处的 T/R/S；节点无通道返回 false 且不修改 out。
    bool sample(uint32_t node, float time, sANIM_TRANSFORM &out) const;
    void updateDuration(); // 用所有通道的最大关键帧时间刷新 aDuration
};

class cANIM_PLAYER {
  public:
    cANIM_PLAYER();

    void play(const sANIM_CLIP *clip, bool loop, float speed = 1.0f);
    void crossFade(const sANIM_CLIP *clip, float duration, bool loop = true); // duration<=0 等价 play
    void stop();
    void update(float dt);

    bool sample(uint32_t node, sANIM_TRANSFORM &out) const;
    bool isPlaying() const;
    float time() const { return aTime; }
    float blendWeight() const { return aFading ? aFadeWeight : 1.0f; } // 当前 clip 权重

  private:
    void advance(float &time, const sANIM_CLIP *clip, bool loop, float dt);
    void finishFade();

    const sANIM_CLIP *aClip;
    const sANIM_CLIP *aNextClip;
    float aTime;
    float aNextTime;
    float aSpeed;
    bool aLoop;
    bool aNextLoop;
    bool aFading;
    float aFadeElapsed;
    float aFadeDuration;
    float aFadeWeight; // 当前 clip 的权重（下一 clip = 1 - weight）
};

} // namespace GN3d
#endif // _GN_3DANIM_H_
