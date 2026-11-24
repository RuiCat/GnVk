#pragma once
#ifndef _GN_ANIMSTATEMACHINE_H_
#define _GN_ANIMSTATEMACHINE_H_
// ╔══════════════════════════════════════════════════════════╗
// ║  GN_mini — 动画状态机（P3：参数 → 状态转换 + 事件，CPU）  ║
// ╚══════════════════════════════════════════════════════════╝
//
// 为什么需要它：`cANIM_PLAYER` 只有 play/crossFade，**"现在该播哪段"要调用方自己判断**。
// 做游戏时这段判断会散落在每个角色、每个动作里（走得快就加速、落地就切、受击就打断…），
// 是本仓库"最缺且最容易写歪"的一层。这里把它收成一张表。
//
// 契约：
//   * 参数类型 `eANIM_PARAM` / `sANIM_PARAMS` 定义在 `GN_3Danim.h`（GN_Entity 按值持有
//     参数包，而那一层不能包含本头，见那边的注释）。触发器是"本帧按下"语义，
//     每次 `Update` 结束自动清空。
//   * 状态 = 状态名 + clip 名 + loop/speed + 一组转换 + 一组事件。clip 名在**同一个** clip
//     表里查（表归状态机所有，控制器的 RegisterClip 直接转发到它，不允许存在两份）。
//   * 转换条件 = 参数比较（可选）+ 触发器（可选）。同一帧多条满足时取 `aPriority` 最大者，
//     并列取先注册者 —— 这条是为了确定性：否则注册顺序变化会让行为悄悄变。
//   * 一次 `Update` 内最多级联 `ANIM_MAX_CHAIN` 次转换（防止 A→B→A 这种瞬时环把帧吃光）。
//   * 状态机在 `Start(player)` 时**记住**播放器并在其后的 `Update(dt, params)` 里驱动它。
//     这样"手工播放"与"状态驱动"不可能各跑一份时间轴（那是很难查的双时间轴 bug），
//     并且 InTransition/BlendWeight/Finished 这些查询拿得到真实播放器状态。
//   * 事件：按"归一化时间跨越"触发（loop 时处理回绕）。crossFade 期间**只有淡入方**
//     产生事件，淡出方剩余事件被丢弃 —— 这是边界，写在用例里而不是靠猜。
//   * dt 为 0/负/NaN/Inf 一律按 0：时间轴不推进，但转换照常评估（暂停时触发器仍然生效）。
//   * 本层不碰渲染、不读全局输入、不含 Vulkan 依赖，可纯 CPU 单测。
#include <GN.h> // GN_API 等基础宏（GN_3Dmath 依赖）
#include "GN_3Dmath.h"
#include "GN_3Danim.h" // sANIM_PARAMS / eANIM_PARAM / sANIM_TRANSFORM / sANIM_CLIP

#include <cstdint>
#include <string>
#include <vector>

namespace GN3d {

// 单次 Update 的级联转换上限（防止 A→B→A 瞬时环把一帧吃光）；
// 单帧最多上报多少个事件（超出丢弃，不做动态分配）。
const uint32_t ANIM_MAX_CHAIN = 4;
const uint32_t ANIM_MAX_FIRED_EVENTS = 16;

// ═══════════════════════════════════════════════════════════
//  转换与事件
// ═══════════════════════════════════════════════════════════
enum eANIM_COMPARE {
    eANIM_CMP_NONE = 0, // 不看数值参数（只用触发器）
    eANIM_CMP_LT,       // value <  threshold
    eANIM_CMP_LE,       // value <= threshold
    eANIM_CMP_GT,       // value >  threshold
    eANIM_CMP_GE,       // value >= threshold
    eANIM_CMP_EQ,       // value == threshold（浮点等值，仅用于 0/1 这类离散参数）
    eANIM_CMP_NE        // value != threshold
};

struct sANIM_TRANSITION {
    uint16_t aTarget;       // 目标状态编号
    eANIM_PARAM aParam;     // 参与比较的参数（eANIM_CMP_NONE 时忽略）
    eANIM_COMPARE aCompare; // 比较方式
    float aThreshold;       // 阈值
    int32_t aTrigger;       // >= 0：还要求该参数本帧被触发；-1 = 不要求
    float aFadeDuration;    // > 0：crossFade；0：直接切换（play）
    uint32_t aPriority;     // 同帧多条满足时取最大；并列取先注册

    sANIM_TRANSITION();
};

struct sANIM_EVENT {
    float aNormalizedTime; // 0..1（按 clip 时长归一化；<=0 在进入状态时触发一次）
    uint16_t aId;          // 游戏自定义事件编号（脚步/命中窗口/结束…）

    sANIM_EVENT();
};

// ═══════════════════════════════════════════════════════════
//  cANIM_STATE_MACHINE
// ═══════════════════════════════════════════════════════════
class cANIM_STATE_MACHINE {
  public:
    static const uint16_t INVALID_STATE = 0xFFFF;

    cANIM_STATE_MACHINE();

    void Clear(); // clip 表 / 状态 / 转换 / 当前状态全部清空

    // ── clip 表（非拥有指针；同名覆盖）──
    bool RegisterClip(const char *name, const sANIM_CLIP *clip);
    const sANIM_CLIP *FindClip(const char *name) const;
    uint32_t ClipCount() const;
    const char *ClipName(uint32_t index) const; // 越界返回 ""

    // ── 状态 ──
    // 名字为空 / clip 名查不到 → 返回 INVALID_STATE（状态不会半建成）。
    uint16_t AddState(const char *name, const char *clipName, bool loop = true,
                      float speed = 1.0f);
    uint16_t FindState(const char *name) const;
    uint32_t StateCount() const;
    const char *StateName(uint16_t state) const; // 越界返回 ""

    // 转换：from/target 都必须已存在，否则返回 false（不留半条规则）。
    bool AddTransition(uint16_t from, const sANIM_TRANSITION &transition);
    // 便捷版：只按参数比较
    bool AddTransition(uint16_t from, uint16_t target, eANIM_PARAM param, eANIM_COMPARE compare,
                       float threshold, float fadeDuration = 0.2f, uint32_t priority = 0);
    // 便捷版：只按触发器
    bool AddTransitionOnTrigger(uint16_t from, uint16_t target, eANIM_PARAM trigger,
                                float fadeDuration = 0.2f, uint32_t priority = 0);
    uint32_t TransitionCount(uint16_t from) const;

    bool AddEvent(uint16_t state, float normalizedTime, uint16_t eventId);
    uint32_t EventCount(uint16_t state) const;

    // ── 运行 ──
    bool SetInitial(uint16_t state);
    uint16_t InitialState() const;
    bool Started() const;
    // 进初始状态（重置播放器到该状态的 clip 起点）并记住播放器。没有初始状态返回 false。
    bool Start(cANIM_PLAYER &player);
    void Stop();

    // 每帧一次（必须先 Start）。dt 非法按 0；转换照常评估。返回是否发生了状态切换。
    bool Update(float dt, sANIM_PARAMS &params);

    // 播放器（未 Start 返回 nullptr）。查询类接口都读它，不存在第二份时间轴。
    cANIM_PLAYER *Player() const;

    uint16_t CurrentState() const;
    uint16_t PreviousState() const; // 上一次切换前的状态（未切换过 = INVALID_STATE）
    bool StateChanged() const;      // 本帧是否切换过
    uint32_t LastChainDepth() const; // 上一次 Update 实际级联了几次（诊断）
    bool InTransition() const;      // 播放器是否在 crossFade 中
    float BlendWeight() const;      // 当前状态权重（crossFade 时 < 1）
    bool Finished() const;          // 非 loop 状态已播到末尾

    // 本帧事件
    uint32_t FiredEventCount() const;
    uint16_t FiredEventId(uint32_t index) const;
    // 取出并清除某个事件（"只处理一次"）；不存在返回 false。
    bool ConsumeEvent(uint16_t eventId);

    // 当前 clip 名/时长（诊断用）
    const char *CurrentClipName() const;

  private:
    struct sSTATE {
        std::string aName;
        const sANIM_CLIP *aClip;
        std::string aClipName;
        bool aLoop;
        float aSpeed;
        std::vector<sANIM_TRANSITION> aTransitions;
        std::vector<sANIM_EVENT> aEvents;
    };

    bool validState(uint16_t state) const;
    // 评估 from 的所有转换，返回目标状态（无满足项 = INVALID_STATE）。
    uint16_t evaluate(uint16_t from, const sANIM_PARAMS &params) const;
    static bool compare(float value, eANIM_COMPARE compare, float threshold);
    void enterState(uint16_t state, float fadeDuration, cANIM_PLAYER &player);
    void collectEvents(uint16_t state, float prevNormalized, float currNormalized, bool looped);
    const sSTATE *currentStatePtr() const;

    std::vector<std::pair<std::string, const sANIM_CLIP *> > aClips;
    std::vector<sSTATE> aStates;
    uint16_t aInitial;
    uint16_t aCurrent;
    uint16_t aPrevious;
    bool aStarted;
    bool aChanged;
    cANIM_PLAYER *aPlayer; // Start() 时记住，之后所有驱动/查询都走它
    uint32_t aChainDepth; // 上一次 Update 里实际走了几级（诊断）
    float aPrevNormalized; // 当前状态上一帧的归一化时间（事件跨越判定）
    std::vector<uint16_t> aFired;
};

} // namespace GN3d
#endif // _GN_ANIMSTATEMACHINE_H_
