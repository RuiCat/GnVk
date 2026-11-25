#pragma once
#ifndef _GN_ANIM_BLEND_H_
#define _GN_ANIM_BLEND_H_
// ╔══════════════════════════════════════════════════════════╗
// ║  GN_mini — 混合树与动画层遮罩（D3，纯 CPU）               ║
// ╚══════════════════════════════════════════════════════════╝
//
// 为什么需要它：`cANIM_PLAYER` 只能播一段 clip（或两段 crossFade）。但真实游戏里
// 角色移动几乎从不"整段切"：
//   * 从走到跑是**连续**的（速度 2.7 m/s 时既不是走也不是跑）；
//   * 上半身挥剑时下半身还在跑（这两段动画必须**同时**播）。
// 这两件事分别对应"混合空间"与"动画层"，就是本文件的两个类。
//
// 与 `cANIM_PLAYER` 的分工：PLAYER 负责"一段 clip 的时间轴 + crossFade"，
// 本文件负责"多段 clip 按权重合成"。两者都能喂 `sSKELETON::computePalette`。

#include "GN_3Dmath.h"
#include "GN_3Danim.h"

#include <cstdint>
#include <vector>

namespace GN3d {

// ═══════════════════════════════════════════════════════════
//  1D 混合空间的一个采样点
// ═══════════════════════════════════════════════════════════
struct sANIM_BLEND_POINT {
    float aThreshold; // 该 clip 对应的参数值（如速度 m/s），阈值必须唯一
    const sANIM_CLIP *aClip; // 非拥有
    sANIM_BLEND_POINT();
};

// ═══════════════════════════════════════════════════════════
//  1D 混合空间（混合树的最简形式：一条轴 + 若干采样点）
// ═══════════════════════════════════════════════════════════
// 语义（每条都有用例）：
//
//   * **权重只在相邻两点间线性插值**，端点外的参数**钳到端点**（不外推）。
//     外推会让 speed=10 时把 run 放大成怪异的高速抖动 —— 钳制是刻意的。
//
//   * **相位同步**：所有 clip 共享一个归一化相位 `phase ∈ [0,1)`，
//     各自换算成自己的时间 `phase * duration`。
//     没有它，walk(1.0s/循环) 与 run(0.6s/循环) 混在一起时相位会各走各的，
//     混出的腿是"两条腿各走各的"（视觉上非常明显）。这是所有引擎的标准做法。
//
//   * **`Sample` 按"有该节点通道的 clip"重新归一化**。一段 walk 可能没有手臂通道
//     而 run 有 —— 此时直接用 run 的值，而不是把缺失当成"单位变换"参与插值
//     （后者会把手臂吸到原点）。
class cANIM_BLEND_1D {
  public:
    cANIM_BLEND_1D();

    void Clear();
    // 追加采样点。clip 为空 / 阈值与已有点重复 → 返回 false 且不改变状态。
    bool AddPoint(float threshold, const sANIM_CLIP *clip);
    uint32_t PointCount() const;
    const sANIM_BLEND_POINT *PointAt(uint32_t index) const; // 越界返回 nullptr

    // 当前参数（由调用方每帧设，通常就是角色的水平速度）
    void SetParameter(float parameter);
    float Parameter() const;

    // 权重：out 与采样点是**同序**，和为 1（无点时 out 清空）。
    // 参数落在两端之外时只有一个 1，其余 0。
    void EvaluateWeights(float parameter, std::vector<float> &out) const;

    // ── 时间轴 ──
    void Play(bool loop = true); // 重置相位到 0
    void Update(float dt); // 推进相位（按 dt / Duration()）；duration 为 0 时不动
    float Phase() const;
    void SetPhase(float phase); // 钳到 [0,1)
    bool IsLooping() const;
    // 所有 clip 的最大 duration（相位换算的基准）。无 clip 返回 0。
    float Duration() const;

    // 采样一个节点：把有通道的 clip 按权重混合。
    // 没有任何 clip 覆盖该节点 → 返回 false 且不修改 out。
    bool Sample(uint32_t node, sANIM_TRANSFORM &out) const;

  private:
    // 每个 clip 各自的采样时间 = phase * clip->aDuration
    float timeOf(const sANIM_CLIP *clip) const;

    std::vector<sANIM_BLEND_POINT> aPoints;
    float aParameter;
    float aPhase;
    bool aLoop;
};

// ═══════════════════════════════════════════════════════════
//  动画层（带关节遮罩）
// ═══════════════════════════════════════════════════════════
// 遮罩语义：`aMask[joint]` 是该层对关节 joint 的影响系数 [0,1]。
//   * 空遮罩 = 对所有关节生效（等价于全 1）；
//   * 0 = 完全不影响该关节（下半身的腿）；
//   * 0.5 = 半叠加（常用于"上半身动作减弱"）。
struct sANIM_LAYER {
    cANIM_BLEND_1D *aSource; // 非拥有
    float aWeight; // 层总权重 [0,1]
    std::vector<float> aMask; // 每关节遮罩；空 = 全 1

    sANIM_LAYER();
    // 该层对 joint 的最终权重 = aWeight * mask(joint)，钳到 [0,1]。
    float WeightAt(uint32_t joint) const;
};

// ═══════════════════════════════════════════════════════════
//  层栈（基础层 + 若干覆盖层）
// ═══════════════════════════════════════════════════════════
// 合成顺序：从基础层开始，逐层做 `result = lerp(result, layerSample, w)`，
// 其中 `w = layer.WeightAt(node)`。这就是标准的 override blending。
//
// 关键设计：**上层对某关节没有采样时该关节保持不变**（不是回 T-pose）。
// 否则"上半身攻击层"会让没被攻击动画覆盖的关节瞬间跳到绑定姿势。
class cANIM_LAYER_STACK {
  public:
    cANIM_LAYER_STACK();

    void Clear(); // 清空所有层（含基础层）
    // 设置基础层（先清空）。source 为空返回 false。
    bool SetBase(cANIM_BLEND_1D *source);
    // 追加覆盖层（非拥有）。mask 为空指针 / maskCount==0 表示"全关节生效"。
    // 返回该层的索引（基础层是 0，第一个覆盖层是 1）；失败返回 0。
    uint32_t AddLayer(cANIM_BLEND_1D *source, float weight, const float *mask = 0,
                      uint32_t maskCount = 0);

    uint32_t LayerCount() const; // 含基础层
    const sANIM_LAYER *LayerAt(uint32_t index) const; // 越界返回 nullptr
    sANIM_LAYER *LayerAt(uint32_t index);

    // 推进所有层的时间轴（各层可以是不同速度/不同 clip 集）。
    void Update(float dt);

    // 合成：从基础层开始逐层覆盖。没有任何层覆盖该节点 → false。
    bool Sample(uint32_t node, sANIM_TRANSFORM &out) const;

  private:
    std::vector<sANIM_LAYER> aLayers; // aLayers[0] 是基础层
};

} // namespace GN3d
#endif // _GN_ANIM_BLEND_H_
