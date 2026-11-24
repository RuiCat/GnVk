// GN_AnimStateMachine.cpp — 动画状态机实现（P3）。
//
// 三条实现口径（都有对应用例，见 tests/unit/testAnimStateMachine.cpp）：
//   1. 转换评估与时间推进是两件事：dt 为 0/负/NaN 时时间不动，但触发器照样生效
//      （暂停时"按一下攻击"不该丢）。
//   2. 一次 Update 内最多级联 ANIM_MAX_CHAIN 次：A→B→A 这种瞬时环不会把一帧吃掉。
//   3. 事件按"归一化时间跨越"判定，loop 时处理回绕；crossFade 期间只有淡入方产出事件。

#include "GN_AnimStateMachine.h"

#include <cmath>

namespace GN3d {

// ═══════════════════════════════════════════════════════════
//  sANIM_TRANSITION / sANIM_EVENT
// ═══════════════════════════════════════════════════════════
sANIM_TRANSITION::sANIM_TRANSITION()
    : aTarget(cANIM_STATE_MACHINE::INVALID_STATE), aParam(0), aCompare(eANIM_CMP_NONE),
      aThreshold(0.0f), aTrigger(-1), aFadeDuration(0.0f), aPriority(0) {}

sANIM_EVENT::sANIM_EVENT() : aNormalizedTime(0.0f), aId(0) {}

// ═══════════════════════════════════════════════════════════
//  cANIM_STATE_MACHINE
// ═══════════════════════════════════════════════════════════
cANIM_STATE_MACHINE::cANIM_STATE_MACHINE()
    : aInitial(INVALID_STATE), aCurrent(INVALID_STATE), aPrevious(INVALID_STATE),
      aStarted(false), aChanged(false), aPlayer(nullptr), aChainDepth(0), aPrevNormalized(0.0f) {}

void cANIM_STATE_MACHINE::Clear() {
    aClips.clear();
    aStates.clear();
    aPlayer = nullptr;
    aInitial = INVALID_STATE;
    aCurrent = INVALID_STATE;
    aPrevious = INVALID_STATE;
    aStarted = false;
    aChanged = false;
    aChainDepth = 0;
    aPrevNormalized = 0.0f;
    aFired.clear();
}

// ── clip 表 ──
bool cANIM_STATE_MACHINE::RegisterClip(const char *name, const sANIM_CLIP *clip) {
    if (name == nullptr || name[0] == '\0' || clip == nullptr) return false;
    for (size_t i = 0; i < aClips.size(); ++i) {
        if (aClips[i].first == name) {
            aClips[i].second = clip; // 同名覆盖（重新导入资产时用）
            return true;
        }
    }
    aClips.push_back(std::make_pair(std::string(name), clip));
    return true;
}

const sANIM_CLIP *cANIM_STATE_MACHINE::FindClip(const char *name) const {
    if (name == nullptr) return nullptr;
    for (size_t i = 0; i < aClips.size(); ++i) {
        if (aClips[i].first == name) return aClips[i].second;
    }
    return nullptr;
}

uint32_t cANIM_STATE_MACHINE::ClipCount() const { return (uint32_t)aClips.size(); }

const char *cANIM_STATE_MACHINE::ClipName(uint32_t index) const {
    if (index >= aClips.size()) return "";
    return aClips[index].first.c_str();
}

// ── 状态 ──
bool cANIM_STATE_MACHINE::validState(uint16_t state) const {
    return state < aStates.size();
}

uint16_t cANIM_STATE_MACHINE::AddState(const char *name, const char *clipName, bool loop,
                                       float speed) {
    if (name == nullptr || name[0] == '\0') return INVALID_STATE;
    if (FindState(name) != INVALID_STATE) return INVALID_STATE; // 重名：拒绝而不是覆盖（转换表会乱）
    const sANIM_CLIP *clip = FindClip(clipName);
    if (clip == nullptr) return INVALID_STATE; // clip 查不到：不留半建成状态

    sSTATE state;
    state.aName = name;
    state.aClipName = clipName;
    state.aClip = clip;
    state.aLoop = loop;
    state.aSpeed = (speed > 0.0f) ? speed : 0.0f; // 负/NaN 按 0（冻结），与播放器一致
    aStates.push_back(state);
    return (uint16_t)(aStates.size() - 1);
}

uint16_t cANIM_STATE_MACHINE::FindState(const char *name) const {
    if (name == nullptr) return INVALID_STATE;
    for (size_t i = 0; i < aStates.size(); ++i) {
        if (aStates[i].aName == name) return (uint16_t)i;
    }
    return INVALID_STATE;
}

uint32_t cANIM_STATE_MACHINE::StateCount() const { return (uint32_t)aStates.size(); }

const char *cANIM_STATE_MACHINE::StateName(uint16_t state) const {
    if (!validState(state)) return "";
    return aStates[state].aName.c_str();
}

// ── 转换与事件 ──
bool cANIM_STATE_MACHINE::AddTransition(uint16_t from, const sANIM_TRANSITION &transition) {
    if (!validState(from) || !validState(transition.aTarget)) return false;
    if (transition.aParam >= ANIM_PARAM_MAX) return false;
    if (transition.aTrigger >= (int32_t)ANIM_PARAM_MAX) return false;
    if (!std::isfinite(transition.aThreshold)) return false;
    aStates[from].aTransitions.push_back(transition);
    return true;
}

bool cANIM_STATE_MACHINE::AddTransition(uint16_t from, uint16_t target, eANIM_PARAM param,
                                        eANIM_COMPARE compare, float threshold,
                                        float fadeDuration, uint32_t priority) {
    sANIM_TRANSITION transition;
    transition.aTarget = target;
    transition.aParam = param;
    transition.aCompare = compare;
    transition.aThreshold = threshold;
    transition.aTrigger = -1;
    transition.aFadeDuration = fadeDuration;
    transition.aPriority = priority;
    return AddTransition(from, transition);
}

bool cANIM_STATE_MACHINE::AddTransitionOnTrigger(uint16_t from, uint16_t target,
                                                 eANIM_PARAM trigger, float fadeDuration,
                                                 uint32_t priority) {
    sANIM_TRANSITION transition;
    transition.aTarget = target;
    transition.aParam = trigger;
    transition.aCompare = eANIM_CMP_NONE;
    transition.aThreshold = 0.0f;
    transition.aTrigger = (int32_t)trigger;
    transition.aFadeDuration = fadeDuration;
    transition.aPriority = priority;
    return AddTransition(from, transition);
}

uint32_t cANIM_STATE_MACHINE::TransitionCount(uint16_t from) const {
    if (!validState(from)) return 0;
    return (uint32_t)aStates[from].aTransitions.size();
}

bool cANIM_STATE_MACHINE::AddEvent(uint16_t state, float normalizedTime, uint16_t eventId) {
    if (!validState(state)) return false;
    if (!std::isfinite(normalizedTime)) return false;
    sANIM_EVENT event;
    event.aNormalizedTime = normalizedTime;
    event.aId = eventId;
    aStates[state].aEvents.push_back(event);
    return true;
}

uint32_t cANIM_STATE_MACHINE::EventCount(uint16_t state) const {
    if (!validState(state)) return 0;
    return (uint32_t)aStates[state].aEvents.size();
}

// ── 运行 ──
bool cANIM_STATE_MACHINE::SetInitial(uint16_t state) {
    if (!validState(state)) return false;
    aInitial = state;
    return true;
}

uint16_t cANIM_STATE_MACHINE::InitialState() const { return aInitial; }

bool cANIM_STATE_MACHINE::Started() const { return aStarted; }

bool cANIM_STATE_MACHINE::Start(cANIM_PLAYER &player) {
    if (!validState(aInitial)) return false;
    aPlayer = &player;
    aStarted = true;
    aChanged = false;
    aPrevious = INVALID_STATE;
    aChainDepth = 0;
    aFired.clear();
    enterState(aInitial, 0.0f, player);
    return true;
}

void cANIM_STATE_MACHINE::Stop() {
    if (aPlayer != nullptr) aPlayer->stop();
    aPlayer = nullptr;
    aStarted = false;
    aCurrent = INVALID_STATE;
    aPrevious = INVALID_STATE;
    aChanged = false;
    aFired.clear();
    aPrevNormalized = 0.0f;
}

cANIM_PLAYER *cANIM_STATE_MACHINE::Player() const { return aPlayer; }

void cANIM_STATE_MACHINE::enterState(uint16_t state, float fadeDuration, cANIM_PLAYER &player) {
    const sSTATE &target = aStates[state];
    if (fadeDuration > 0.0f && aCurrent != INVALID_STATE) {
        player.crossFade(target.aClip, fadeDuration, target.aLoop);
    } else {
        player.play(target.aClip, target.aLoop, target.aSpeed);
    }
    aPrevious = aCurrent;
    aCurrent = state;
    aChanged = true;
    aPrevNormalized = 0.0f;
    // 归一化时间 <= 0 的事件在进入状态时立刻触发（"动作开始"这类）
    const std::vector<sANIM_EVENT> &events = target.aEvents;
    for (size_t i = 0; i < events.size(); ++i) {
        if (events[i].aNormalizedTime <= 0.0f && aFired.size() < ANIM_MAX_FIRED_EVENTS) {
            aFired.push_back(events[i].aId);
        }
    }
}

bool cANIM_STATE_MACHINE::compare(float value, eANIM_COMPARE kind, float threshold) {
    switch (kind) {
    case eANIM_CMP_LT: return value < threshold;
    case eANIM_CMP_LE: return value <= threshold;
    case eANIM_CMP_GT: return value > threshold;
    case eANIM_CMP_GE: return value >= threshold;
    case eANIM_CMP_EQ: return value == threshold;
    case eANIM_CMP_NE: return value != threshold;
    case eANIM_CMP_NONE:
    default: return true; // 不看数值
    }
}

uint16_t cANIM_STATE_MACHINE::evaluate(uint16_t from, const sANIM_PARAMS &params) const {
    const std::vector<sANIM_TRANSITION> &transitions = aStates[from].aTransitions;
    uint16_t best = INVALID_STATE;
    uint32_t bestPriority = 0;
    for (size_t i = 0; i < transitions.size(); ++i) {
        const sANIM_TRANSITION &transition = transitions[i];
        // 触发器条件：必须是"本帧被触发"（不是"按住"）
        if (transition.aTrigger >= 0 && !params.Triggered((eANIM_PARAM)transition.aTrigger)) {
            continue;
        }
        if (transition.aCompare != eANIM_CMP_NONE &&
            !compare(params.Get(transition.aParam), transition.aCompare, transition.aThreshold)) {
            continue;
        }
        // 优先级最大者胜；并列取先注册（严格大于才替换）
        if (best == INVALID_STATE || transition.aPriority > bestPriority) {
            best = transition.aTarget;
            bestPriority = transition.aPriority;
        }
    }
    return best;
}

void cANIM_STATE_MACHINE::collectEvents(uint16_t state, float prevNormalized,
                                        float currNormalized, bool looped) {
    if (!validState(state)) return;
    const std::vector<sANIM_EVENT> &events = aStates[state].aEvents;
    for (size_t i = 0; i < events.size(); ++i) {
        const float at = events[i].aNormalizedTime;
        if (!(at > 0.0f)) continue; // <= 0 的已在 enterState 里触发
        bool crossed = false;
        if (looped) {
            // 回绕：跨过 1.0 时命中 (prev, 1] 与 [0, curr] 两段
            crossed = (at > prevNormalized) || (at <= currNormalized);
        } else {
            crossed = (at > prevNormalized && at <= currNormalized);
        }
        if (crossed && aFired.size() < ANIM_MAX_FIRED_EVENTS) aFired.push_back(events[i].aId);
    }
}

bool cANIM_STATE_MACHINE::Update(float dt, sANIM_PARAMS &params) {
    aFired.clear();
    aChanged = false;
    aChainDepth = 0;
    if (!aStarted || aPlayer == nullptr || !validState(aCurrent)) return false;
    cANIM_PLAYER &player = *aPlayer;

    // dt 非法按 0：时间不推进，但转换照常评估（暂停时触发器仍生效）
    if (!(dt > 0.0f) || !std::isfinite(dt)) dt = 0.0f;

    // 1) 评估转换（级联上限）
    for (uint32_t chain = 0; chain < ANIM_MAX_CHAIN; ++chain) {
        const uint16_t next = evaluate(aCurrent, params);
        if (next == INVALID_STATE || next == aCurrent) break;
        const sANIM_TRANSITION *matched = nullptr;
        const std::vector<sANIM_TRANSITION> &transitions = aStates[aCurrent].aTransitions;
        uint32_t bestPriority = 0;
        for (size_t i = 0; i < transitions.size(); ++i) {
            if (transitions[i].aTarget != next) continue;
            if (matched == nullptr || transitions[i].aPriority > bestPriority) {
                matched = &transitions[i];
                bestPriority = transitions[i].aPriority;
            }
        }
        const float fade = matched ? matched->aFadeDuration : 0.0f;
        enterState(next, fade, player);
        ++aChainDepth;
    }

    // 2) 推进时间轴
    const float before = player.time();
    player.update(dt);

    // 3) 事件：按归一化时间跨越判定
    if (validState(aCurrent)) {
        const sANIM_CLIP *clip = aStates[aCurrent].aClip;
        const float duration = clip ? clip->aDuration : 0.0f;
        if (duration > 0.0f) {
            const float curr = player.time() / duration;
            const bool looped = (player.time() < before); // loop 回绕
            collectEvents(aCurrent, aPrevNormalized, curr, looped);
            aPrevNormalized = curr;
        } else {
            aPrevNormalized = 0.0f;
        }
    }

    // 4) 触发器是"本帧"语义：消费完就清
    for (uint16_t i = 0; i < ANIM_PARAM_MAX; ++i) params.aTrigger[i] = false;
    return aChanged;
}

uint16_t cANIM_STATE_MACHINE::CurrentState() const { return aCurrent; }

uint16_t cANIM_STATE_MACHINE::PreviousState() const { return aPrevious; }

bool cANIM_STATE_MACHINE::StateChanged() const { return aChanged; }

bool cANIM_STATE_MACHINE::InTransition() const {
    if (aPlayer == nullptr) return false;
    return aPlayer->blendWeight() < 1.0f;
}

float cANIM_STATE_MACHINE::BlendWeight() const {
    if (aPlayer == nullptr) return 1.0f;
    return aPlayer->blendWeight();
}

bool cANIM_STATE_MACHINE::Finished() const {
    if (aPlayer == nullptr || !validState(aCurrent)) return false;
    const sANIM_CLIP *clip = aStates[aCurrent].aClip;
    if (clip == nullptr || aStates[aCurrent].aLoop) return false;
    return aPlayer->time() >= clip->aDuration && !InTransition();
}

uint32_t cANIM_STATE_MACHINE::LastChainDepth() const { return aChainDepth; }

const cANIM_STATE_MACHINE::sSTATE *cANIM_STATE_MACHINE::currentStatePtr() const {
    if (!validState(aCurrent)) return nullptr;
    return &aStates[aCurrent];
}

uint32_t cANIM_STATE_MACHINE::FiredEventCount() const { return (uint32_t)aFired.size(); }

uint16_t cANIM_STATE_MACHINE::FiredEventId(uint32_t index) const {
    if (index >= aFired.size()) return 0;
    return aFired[index];
}

bool cANIM_STATE_MACHINE::ConsumeEvent(uint16_t eventId) {
    for (size_t i = 0; i < aFired.size(); ++i) {
        if (aFired[i] == eventId) {
            aFired.erase(aFired.begin() + (long)i);
            return true;
        }
    }
    return false;
}

const char *cANIM_STATE_MACHINE::CurrentClipName() const {
    if (!validState(aCurrent)) return "";
    return aStates[aCurrent].aClipName.c_str();
}

} // namespace GN3d
