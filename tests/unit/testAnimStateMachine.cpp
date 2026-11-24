// testAnimStateMachine.cpp — 动画状态机（P3）纯 CPU 单测。
//
// 覆盖 docs/ENTITY_CONTROLLERS_PLAN.md §5.1 P3 的验收口径：
//   转换规则、crossFade 期间权重、缺通道降级、非法状态名、dt 为 0/负/超大；
// 另外用它把 fixtures 两段 clip（旋转 0° / 90°）跑通「状态切换 → 关节世界矩阵变化」。
#include "testFramework.h"

#include <GN.h>
#include "GN_AnimStateMachine.h"
#include "GN_Entity.h"
#include "fixtures.h"

#include <cmath>
#include <vector>

using namespace GN3d;

namespace {

// 测试用参数编号（游戏侧自定，引擎不预设）
enum {
    eP_SPEED = 1,
    eP_GROUNDED = 2,
    eP_ATTACK = 3,
    eP_DEAD = 4
};

// 一段"只有旋转通道"的 clip。**注意**：`sANIM_CLIP::sample` 在节点有任意通道时会把
// 未覆盖的分量重置为默认值（平移 0、缩放 1），所以只用它测旋转是对的，
// 但要测"关节位置也跟着变"就必须同时给平移通道（见 makeRotationClipAt）。
// 这条行为本身有专门的用例钉住：animClipRotationOnlyResetsTranslation。
// 一段只有单通道的旋转 clip：节点 node 绕 Z 轴 fromDeg → toDeg，时长 duration。
sANIM_CLIP makeRotationClip(uint32_t node, float fromDeg, float toDeg, float duration) {
    sANIM_CLIP clip;
    clip.aDuration = duration;
    sANIM_CHANNEL channel;
    channel.aNode = node;
    channel.aPath = eANIM_ROTATION;
    channel.aInterp = eANIM_LINEAR;
    channel.aTimes.push_back(0.0f);
    channel.aTimes.push_back(duration);
    const float fromHalf = fromDeg * 0.017453292519943f * 0.5f;
    const float toHalf = toDeg * 0.017453292519943f * 0.5f;
    channel.aQuatValues.push_back(GNg::sQUAT{0.0f, 0.0f, std::sin(fromHalf), std::cos(fromHalf)});
    channel.aQuatValues.push_back(GNg::sQUAT{0.0f, 0.0f, std::sin(toHalf), std::cos(toHalf)});
    clip.aChannels.push_back(channel);
    clip.updateDuration();
    return clip;
}

// 带平移通道的版本：节点 node 固定在 (0, bindY, 0) 并绕 Z 转。真实资产里被动画驱动的
// 骨骼几乎都同时带 T 与 R 通道，用它构造的姿势才等价于"真骨骼"。
sANIM_CLIP makeRotationClipAt(uint32_t node, float fromDeg, float toDeg, float duration,
                              float bindY) {
    sANIM_CLIP clip = makeRotationClip(node, fromDeg, toDeg, duration);
    sANIM_CHANNEL translation;
    translation.aNode = node;
    translation.aPath = eANIM_TRANSLATION;
    translation.aInterp = eANIM_LINEAR;
    translation.aTimes.push_back(0.0f);
    translation.aTimes.push_back(duration);
    translation.aVecValues.push_back(GNg::sVEC3{0.0f, bindY, 0.0f});
    translation.aVecValues.push_back(GNg::sVEC3{0.0f, bindY, 0.0f});
    clip.aChannels.push_back(translation);
    clip.updateDuration();
    return clip;
}

// 建一个"站立/行走/攻击/死亡"四状态的典型表（后面多条用例复用）。
struct sRIG {
    cANIM_STATE_MACHINE aMachine;
    cANIM_PLAYER aPlayer;
    sANIM_PARAMS aParams;
    uint16_t aIdle;
    uint16_t aWalk;
    uint16_t aAttack;
    uint16_t aDead;
    sANIM_CLIP aIdleClip;
    sANIM_CLIP aWalkClip;
    sANIM_CLIP aAttackClip;
    sANIM_CLIP aDeadClip;
};

void buildRig(sRIG &rig) {
    rig.aIdleClip = makeRotationClip(0, 0.0f, 0.0f, 1.0f);
    rig.aWalkClip = makeRotationClip(0, 0.0f, 90.0f, 1.0f);
    rig.aAttackClip = makeRotationClip(0, 0.0f, 45.0f, 0.5f);
    rig.aDeadClip = makeRotationClip(0, 0.0f, -90.0f, 1.0f);

    CHECK(rig.aMachine.RegisterClip("idle", &rig.aIdleClip));
    CHECK(rig.aMachine.RegisterClip("walk", &rig.aWalkClip));
    CHECK(rig.aMachine.RegisterClip("attack", &rig.aAttackClip));
    CHECK(rig.aMachine.RegisterClip("dead", &rig.aDeadClip));

    rig.aIdle = rig.aMachine.AddState("idle", "idle", true, 1.0f);
    rig.aWalk = rig.aMachine.AddState("walk", "walk", true, 1.0f);
    rig.aAttack = rig.aMachine.AddState("attack", "attack", false, 1.0f);
    rig.aDead = rig.aMachine.AddState("dead", "dead", false, 1.0f);
    CHECK(rig.aIdle != cANIM_STATE_MACHINE::INVALID_STATE);
    CHECK(rig.aWalk != cANIM_STATE_MACHINE::INVALID_STATE);
    CHECK(rig.aAttack != cANIM_STATE_MACHINE::INVALID_STATE);
    CHECK(rig.aDead != cANIM_STATE_MACHINE::INVALID_STATE);

    // idle ↔ walk 按速度；攻击/死亡按触发器（优先级更高，会抢占速度转换）
    CHECK(rig.aMachine.AddTransition(rig.aIdle, rig.aWalk, eP_SPEED, eANIM_CMP_GT, 0.1f, 0.2f));
    CHECK(rig.aMachine.AddTransition(rig.aWalk, rig.aIdle, eP_SPEED, eANIM_CMP_LE, 0.1f, 0.2f));
    CHECK(rig.aMachine.AddTransitionOnTrigger(rig.aIdle, rig.aAttack, eP_ATTACK, 0.1f, 10));
    CHECK(rig.aMachine.AddTransitionOnTrigger(rig.aWalk, rig.aAttack, eP_ATTACK, 0.1f, 10));
    CHECK(rig.aMachine.AddTransition(rig.aIdle, rig.aDead, eP_DEAD, eANIM_CMP_GT, 0.5f, 0.0f, 100));
    CHECK(rig.aMachine.AddTransition(rig.aWalk, rig.aDead, eP_DEAD, eANIM_CMP_GT, 0.5f, 0.0f, 100));

    CHECK(rig.aMachine.SetInitial(rig.aIdle));
}

} // namespace

// ── 参数包 ──────────────────────────────────────────────
TEST_CASE(animParamsBasics) {
    sANIM_PARAMS params;
    CHECK_NEAR(params.Get(eP_SPEED), 0.0f, 1e-9);
    CHECK(!params.Triggered(eP_ATTACK));

    params.Set(eP_SPEED, 3.5f);
    CHECK_NEAR(params.Get(eP_SPEED), 3.5f, 1e-6);
    params.Set(eP_SPEED, std::nanf("")); // NaN 不污染参数表
    CHECK_NEAR(params.Get(eP_SPEED), 0.0f, 1e-9);

    params.Trigger(eP_ATTACK);
    params.Trigger(eP_ATTACK); // 重复置位无副作用
    CHECK(params.Triggered(eP_ATTACK));

    // 越界编号全部静默无效（不崩、不越界写）
    params.Set((eANIM_PARAM)ANIM_PARAM_MAX, 1.0f);
    params.Trigger((eANIM_PARAM)9999);
    CHECK_NEAR(params.Get((eANIM_PARAM)ANIM_PARAM_MAX), 0.0f, 1e-9);
    CHECK(!params.Triggered((eANIM_PARAM)9999));

    params.Reset();
    CHECK(!params.Triggered(eP_ATTACK));
    CHECK_NEAR(params.Get(eP_SPEED), 0.0f, 1e-9);
}

// ── 建表：非法输入全部拒绝，且不留半建成状态 ──────────────
TEST_CASE(animStateMachineRejectsBadDefinitions) {
    cANIM_STATE_MACHINE machine;
    const sANIM_CLIP clip = makeRotationClip(0, 0.0f, 10.0f, 1.0f);

    CHECK(!machine.RegisterClip("", &clip));
    CHECK(!machine.RegisterClip("clip", nullptr));
    CHECK(machine.RegisterClip("clip", &clip));
    CHECK(machine.RegisterClip("clip", &clip)); // 同名覆盖
    CHECK_EQ(machine.ClipCount(), 1u);

    // 状态名空 / clip 不存在 / 重名：全部返回 INVALID_STATE 且状态数不变
    CHECK(machine.AddState("", "clip") == cANIM_STATE_MACHINE::INVALID_STATE);
    CHECK(machine.AddState("run", "nope") == cANIM_STATE_MACHINE::INVALID_STATE);
    CHECK_EQ(machine.StateCount(), 0u);

    const uint16_t idle = machine.AddState("idle", "clip");
    CHECK(idle != cANIM_STATE_MACHINE::INVALID_STATE);
    CHECK(machine.AddState("idle", "clip") == cANIM_STATE_MACHINE::INVALID_STATE); // 重名
    CHECK_EQ(machine.StateCount(), 1u);
    CHECK_STREQ(machine.StateName(idle), "idle");
    CHECK_STREQ(machine.StateName(99), "");

    // 转换：端点必须存在；参数/触发器编号必须合法
    CHECK(!machine.AddTransition(idle, 99, eP_SPEED, eANIM_CMP_GT, 0.0f));
    CHECK(!machine.AddTransition(99, idle, eP_SPEED, eANIM_CMP_GT, 0.0f));
    CHECK(!machine.AddTransition(idle, idle, (eANIM_PARAM)ANIM_PARAM_MAX, eANIM_CMP_GT, 0.0f));
    CHECK_EQ(machine.TransitionCount(idle), 0u); // 半条规则都没有留下
    CHECK(machine.AddTransition(idle, idle, eP_SPEED, eANIM_CMP_GT, 0.0f));
    CHECK_EQ(machine.TransitionCount(idle), 1u);

    // 事件
    CHECK(machine.AddEvent(idle, 0.5f, 7));
    CHECK(!machine.AddEvent(99, 0.5f, 7));
    CHECK(!machine.AddEvent(idle, std::nanf(""), 7));
    CHECK_EQ(machine.EventCount(idle), 1u);
}

// ── 启动 / 转换规则 ─────────────────────────────────────
TEST_CASE(animStateMachineStartAndParamTransition) {
    sRIG rig;
    buildRig(rig);

    CHECK(!rig.aMachine.Started());
    CHECK(rig.aMachine.Start(rig.aPlayer));
    CHECK(rig.aMachine.Started());
    CHECK_EQ(rig.aMachine.CurrentState(), rig.aIdle);
    CHECK_EQ(rig.aMachine.PreviousState(), cANIM_STATE_MACHINE::INVALID_STATE);
    CHECK_STREQ(rig.aMachine.CurrentClipName(), "idle");
    CHECK(rig.aMachine.Player() == &rig.aPlayer); // 记住的是同一个播放器（不存在第二份时间轴）

    // 速度 0：不切
    rig.aParams.Set(eP_SPEED, 0.0f);
    rig.aMachine.Update(0.1f, rig.aParams);
    CHECK_EQ(rig.aMachine.CurrentState(), rig.aIdle);

    // 速度 > 0.1：idle → walk（crossFade 0.2s）
    rig.aParams.Set(eP_SPEED, 2.0f);
    CHECK(rig.aMachine.Update(0.1f, rig.aParams));
    CHECK_EQ(rig.aMachine.CurrentState(), rig.aWalk);
    CHECK_EQ(rig.aMachine.PreviousState(), rig.aIdle);
    CHECK(rig.aMachine.StateChanged());
    CHECK(rig.aMachine.InTransition());
    CHECK(rig.aMachine.BlendWeight() < 1.0f);

    // 回到 idle
    rig.aParams.Set(eP_SPEED, 0.0f);
    CHECK(rig.aMachine.Update(0.1f, rig.aParams));
    CHECK_EQ(rig.aMachine.CurrentState(), rig.aIdle);

    // 没有满足的转换：本帧不切换
    CHECK(!rig.aMachine.Update(0.1f, rig.aParams));
    CHECK(!rig.aMachine.StateChanged());
}

// 触发器只在"本帧"有效：Update 后自动清空，按住不放不会反复触发。
TEST_CASE(animStateMachineTriggerIsOneShot) {
    sRIG rig;
    buildRig(rig);
    CHECK(rig.aMachine.Start(rig.aPlayer));

    rig.aParams.Trigger(eP_ATTACK);
    CHECK(rig.aMachine.Update(0.016f, rig.aParams));
    CHECK_EQ(rig.aMachine.CurrentState(), rig.aAttack);
    CHECK(!rig.aParams.Triggered(eP_ATTACK)); // Update 结束即清

    // 从 attack 没有回 idle 的规则：停在 attack，直到播完（非 loop）
    for (int i = 0; i < 40; ++i) rig.aMachine.Update(1.0f / 60.0f, rig.aParams);
    CHECK_EQ(rig.aMachine.CurrentState(), rig.aAttack);
    CHECK(rig.aMachine.Finished());
}

// 优先级：死亡(100) 抢占攻击(10)，同一帧两者都触发时走死亡。
TEST_CASE(animStateMachinePriorityWins) {
    sRIG rig;
    buildRig(rig);
    CHECK(rig.aMachine.Start(rig.aPlayer));

    rig.aParams.Trigger(eP_ATTACK);
    rig.aParams.Set(eP_DEAD, 1.0f);
    rig.aMachine.Update(1.0f / 60.0f, rig.aParams);
    CHECK_EQ(rig.aMachine.CurrentState(), rig.aDead); // 高优先级胜
    CHECK_EQ(rig.aMachine.PreviousState(), rig.aIdle);
}

// 并列优先级取"先注册"：行为不能依赖注册顺序之外的东西。
TEST_CASE(animStateMachineTieTakesFirstRegistered) {
    cANIM_STATE_MACHINE machine;
    const sANIM_CLIP clipA = makeRotationClip(0, 0.0f, 10.0f, 1.0f);
    const sANIM_CLIP clipB = makeRotationClip(0, 0.0f, 20.0f, 1.0f);
    machine.RegisterClip("a", &clipA);
    machine.RegisterClip("b", &clipB);
    const uint16_t start = machine.AddState("start", "a");
    const uint16_t first = machine.AddState("first", "a");
    const uint16_t second = machine.AddState("second", "b");
    machine.AddTransitionOnTrigger(start, second, eP_ATTACK, 0.0f, 5); // 先注册 second
    machine.AddTransitionOnTrigger(start, first, eP_ATTACK, 0.0f, 5);  // 后注册 first
    CHECK(machine.SetInitial(start));

    cANIM_PLAYER player;
    CHECK(machine.Start(player));
    sANIM_PARAMS params;
    params.Trigger(eP_ATTACK);
    machine.Update(0.016f, params);
    CHECK_EQ(machine.CurrentState(), second);
}

// 级联上限：A→B→A 的瞬时环不会把一帧吃掉。
TEST_CASE(animStateMachineChainLimit) {
    cANIM_STATE_MACHINE machine;
    const sANIM_CLIP clip = makeRotationClip(0, 0.0f, 10.0f, 1.0f);
    machine.RegisterClip("c", &clip);
    const uint16_t a = machine.AddState("a", "c");
    const uint16_t b = machine.AddState("b", "c");
    // 两条无条件互相转换（a→b, b→a）：没有级联上限时会无限循环
    machine.AddTransition(a, b, 0, eANIM_CMP_NONE, 0.0f, 0.0f);
    machine.AddTransition(b, a, 0, eANIM_CMP_NONE, 0.0f, 0.0f);
    CHECK(machine.SetInitial(a));

    cANIM_PLAYER player;
    CHECK(machine.Start(player));
    sANIM_PARAMS params;
    machine.Update(0.016f, params); // 能返回就说明没有死循环
    CHECK(machine.LastChainDepth() <= ANIM_MAX_CHAIN);
}

// ── dt 边界 ────────────────────────────────────────────
TEST_CASE(animStateMachineInvalidDt) {
    sRIG rig;
    buildRig(rig);
    CHECK(rig.aMachine.Start(rig.aPlayer));

    rig.aMachine.Update(0.5f, rig.aParams);
    const float timeBefore = rig.aPlayer.time();

    rig.aMachine.Update(0.0f, rig.aParams);
    rig.aMachine.Update(-1.0f, rig.aParams);
    rig.aMachine.Update(std::nanf(""), rig.aParams);
    rig.aMachine.Update(HUGE_VALF, rig.aParams);
    CHECK_NEAR(rig.aPlayer.time(), timeBefore, 1e-5);

    // 暂停时触发器仍然生效（"按一下攻击"不该因为暂停而丢）
    rig.aParams.Trigger(eP_ATTACK);
    CHECK(rig.aMachine.Update(0.0f, rig.aParams));
    CHECK_EQ(rig.aMachine.CurrentState(), rig.aAttack);

    // 超大 dt：时间不会 NaN/Inf（loop clip 回绕到 [0, duration)）
    rig.aMachine.Update(1.0e6f, rig.aParams);
    CHECK(std::isfinite(rig.aPlayer.time()));
    CHECK(rig.aPlayer.time() >= 0.0f);
    CHECK(rig.aPlayer.time() <= 1.0f);
}

// ── 事件 ───────────────────────────────────────────────
TEST_CASE(animStateMachineEvents) {
    cANIM_STATE_MACHINE machine;
    const sANIM_CLIP clip = makeRotationClip(0, 0.0f, 90.0f, 1.0f);
    machine.RegisterClip("swing", &clip);
    const uint16_t swing = machine.AddState("swing", "swing", true);
    machine.AddEvent(swing, 0.0f, 100); // 进入即触发
    machine.AddEvent(swing, 0.5f, 101); // 一半时
    machine.AddEvent(swing, 0.9f, 102);
    CHECK(machine.SetInitial(swing));

    cANIM_PLAYER player;
    CHECK(machine.Start(player));
    sANIM_PARAMS params;

    // 进入状态时 <=0 的事件已触发
    CHECK_EQ(machine.FiredEventCount(), 1u);
    CHECK_EQ(machine.FiredEventId(0), 100u);
    CHECK(machine.ConsumeEvent(100));
    CHECK(!machine.ConsumeEvent(100)); // 只能消费一次

    // 推进到 0.6：应命中 101（0.5）但不到 102（0.9）
    machine.Update(0.6f, params);
    CHECK(machine.ConsumeEvent(101));
    CHECK(!machine.ConsumeEvent(102));

    // 再推进 0.4 到 1.0：命中 102
    machine.Update(0.4f, params);
    CHECK(machine.ConsumeEvent(102));

    // loop 回绕：跨过 0.5 应再次触发 101
    machine.Update(0.6f, params); // 1.0 → 0.6（回绕）
    CHECK(machine.ConsumeEvent(101));
}

// ── crossFade 期间的采样权重 ────────────────────────────
// 两段 clip 分别把关节转到 0° / 90°；淡入淡出中间采样值必须落在两者之间，
// 淡入结束后与目标 clip 单独播放完全一致。
TEST_CASE(animStateMachineCrossFadeWeight) {
    const sANIM_CLIP clip0 = makeRotationClip(0, 0.0f, 0.0f, 1.0f); // 恒 0°
    const sANIM_CLIP clip90 = makeRotationClip(0, 90.0f, 90.0f, 1.0f); // 恒 90°

    cANIM_STATE_MACHINE machine;
    machine.RegisterClip("a", &clip0);
    machine.RegisterClip("b", &clip90);
    const uint16_t stateA = machine.AddState("a", "a", true);
    const uint16_t stateB = machine.AddState("b", "b", true);
    machine.AddTransitionOnTrigger(stateA, stateB, eP_ATTACK, 0.4f); // 0.4s 淡入
    machine.SetInitial(stateA);

    cANIM_PLAYER player;
    CHECK(machine.Start(player));
    sANIM_PARAMS params;

    // 淡入前：完全是 A（0°）
    sANIM_TRANSFORM transform;
    CHECK(player.sample(0, transform));
    CHECK_NEAR(transform.aRotation.z, 0.0f, 1e-5);

    params.Trigger(eP_ATTACK);
    machine.Update(0.1f, params);
    CHECK_EQ(machine.CurrentState(), stateB);
    CHECK(machine.InTransition());
    CHECK(machine.BlendWeight() < 1.0f && machine.BlendWeight() > 0.0f);

    // 淡入中途：旋转角的 sin(z) 必须严格在两者之间（0 < z < sin45°）
    CHECK(player.sample(0, transform));
    const float midZ = transform.aRotation.z;
    CHECK(midZ > 1e-3f);
    CHECK(midZ < std::sin(0.785398163f));

    // 淡入结束：完全等于 B（90°），且不再处于过渡
    machine.Update(0.5f, params);
    CHECK(!machine.InTransition());
    CHECK_NEAR(machine.BlendWeight(), 1.0f, 1e-5);
    CHECK(player.sample(0, transform));
    CHECK_NEAR(transform.aRotation.z, std::sin(0.785398163f), 1e-4);
}

// ── 缺通道降级 ─────────────────────────────────────────
// 目标 clip 只有 node0 的通道，node1 没有：crossFade 期间 node1 必须报"没有通道"
// 且**不修改**输出（交给上层保留绑定姿势），而不是写一个零变换进去。
TEST_CASE(animStateMachineMissingChannelFallsBack) {
    const sANIM_CLIP clipA = makeRotationClip(0, 0.0f, 30.0f, 1.0f);
    const sANIM_CLIP clipB = makeRotationClip(1, 0.0f, 60.0f, 1.0f); // 只有 node1

    cANIM_STATE_MACHINE machine;
    machine.RegisterClip("a", &clipA);
    machine.RegisterClip("b", &clipB);
    const uint16_t stateA = machine.AddState("a", "a", true);
    const uint16_t stateB = machine.AddState("b", "b", true);
    machine.AddTransitionOnTrigger(stateA, stateB, eP_ATTACK, 0.2f);
    machine.SetInitial(stateA);

    cANIM_PLAYER player;
    CHECK(machine.Start(player));
    sANIM_PARAMS params;

    // node1 在 A 里没有通道 → sample 返回 false
    sANIM_TRANSFORM untouched;
    untouched.aTranslation = GNg::sVEC3{7.0f, 8.0f, 9.0f};
    CHECK(!player.sample(1, untouched));
    CHECK_NEAR(untouched.aTranslation.x, 7.0f, 1e-6); // 没被写脏

    params.Trigger(eP_ATTACK);
    machine.Update(0.05f, params); // 淡入中（0.2s）
    CHECK(machine.InTransition());
    // 淡入中：node0 只有 A 有通道 → 仍返回 A 的值（不因 B 缺失而失败）
    sANIM_TRANSFORM node0;
    CHECK(player.sample(0, node0));
    CHECK(std::fabs(node0.aRotation.z) > 1e-6f);

    // 淡入结束：node0 变成"没有通道"
    machine.Update(0.3f, params);
    CHECK(!machine.InTransition());
    CHECK(!player.sample(0, node0));
}

// 既有行为（钉住，避免以后被"顺手改掉"）：节点只要**有任意通道**，`sample` 就会把
// 它未覆盖的分量重置为默认值（平移 0 / 缩放 1）。所以"只写旋转的 clip"会把关节挪到原点。
// 真实资产被驱动的骨骼都带 T+R 通道；写夹具时要记得这一点。
TEST_CASE(animClipRotationOnlyResetsTranslation) {
    const sANIM_CLIP rotationOnly = makeRotationClip(0, 30.0f, 30.0f, 1.0f);
    sANIM_TRANSFORM transform;
    CHECK(rotationOnly.sample(0, 0.5f, transform));
    CHECK_NEAR(transform.aTranslation.x, 0.0f, 1e-6);
    CHECK_NEAR(transform.aTranslation.y, 0.0f, 1e-6); // 平移被重置为 0
    CHECK_NEAR(transform.aScale.y, 1.0f, 1e-6);       // 缩放取默认 1
    CHECK(std::fabs(transform.aRotation.z) > 1e-3f);  // 旋转生效

    const sANIM_CLIP withTranslation = makeRotationClipAt(0, 30.0f, 30.0f, 1.0f, 2.0f);
    CHECK(withTranslation.sample(0, 0.5f, transform));
    CHECK_NEAR(transform.aTranslation.y, 2.0f, 1e-6); // 有平移通道 → 保住
}

// ── 与实体层接通：状态切换 → 调色板/关节世界矩阵变化 ──
TEST_CASE(entityAnimSwitch) {
    cENTITY entity;
    const sSKELETON skeleton = fixture::buildBoneChain(3);
    entity.aSkeleton.SetSkeleton(&skeleton);

    // fixtures 造两段 clip：关节 1 固定在 (0,1,0)（= 它的绑定平移）并转 0° / 90°。
    // 必须带平移通道，否则 sample 会把关节平移重置成 0（见 helper 上的注释）。
    const sANIM_CLIP clip0 = makeRotationClipAt(1, 0.0f, 0.0f, 1.0f, fixture::BONE_LENGTH);
    const sANIM_CLIP clip90 = makeRotationClipAt(1, 90.0f, 90.0f, 1.0f, fixture::BONE_LENGTH);
    CHECK(entity.aAnim.RegisterClip("pose0", &clip0));
    CHECK(entity.aAnim.RegisterClip("pose90", &clip90));
    // RegisterClip 会转发到状态机的 clip 表：只有一份
    CHECK(entity.aAnim.StateMachine().FindClip("pose0") == &clip0);
    CHECK_EQ(entity.aAnim.StateMachine().ClipCount(), entity.aAnim.ClipCount());

    const uint16_t state0 = entity.aAnim.StateMachine().AddState("pose0", "pose0", true);
    const uint16_t state90 = entity.aAnim.StateMachine().AddState("pose90", "pose90", true);
    CHECK(entity.aAnim.StateMachine().AddTransitionOnTrigger(state0, state90, eP_ATTACK, 0.0f));
    CHECK(entity.aAnim.StateMachine().SetInitial(state0));
    CHECK(entity.aAnim.StartStateMachine());
    CHECK(entity.aAnim.StateMachineActive());

    // 跑几帧到 pose0 稳定，记录**子**关节（node 2）的世界位置。
    // 注意：关节 1 自身绕 Z 转不会移动它自己的原点（旋转只影响子节点）——
    // 这条在 P1 已经踩过一次，这里特意用子关节验证，避免"旋转=位移"的错觉。
    for (int i = 0; i < 10; ++i) entity.Update(1.0f / 60.0f);
    CHECK(entity.aSkeleton.PaletteValid());
    const GNg::sVEC3 beforeTip = entity.aSkeleton.JointWorldPosition(2);
    CHECK_NEAR(beforeTip.x, 0.0f, 1e-3); // 绑定姿势：关节 2 在 (0, 3, 0)

    // 触发切换 → pose90
    entity.aParams.Trigger(eP_ATTACK);
    entity.Update(1.0f / 60.0f);
    CHECK_STREQ(entity.aAnim.Current(), "pose90");
    for (int i = 0; i < 10; ++i) entity.Update(1.0f / 60.0f);

    // 关节 1 绕 Z 转 90°：子关节世界位置 (0,3,0) → (-1,2,0)
    const GNg::sVEC3 afterTip = entity.aSkeleton.JointWorldPosition(2);
    CHECK(afterTip.x < beforeTip.x - 0.5f); // x 明显变负
    CHECK(afterTip.y < beforeTip.y - 0.5f); // y 明显变小
    CHECK_NEAR(afterTip.x, -1.0f, 0.1f);
    CHECK_NEAR(afterTip.y, 2.0f, 0.1f);

    // 蒙皮顶点也跟着动（fixtures 立方体上半绑在关节 1 上）
    std::vector<GNg::sVEC3> skinned;
    std::vector<GNg::sVEC3> normals(fixture::buildSkinnedCube(1.0f).size());
    const std::vector<GN3d::sSKINNED_VERTEX> cube = fixture::buildSkinnedCube(1.0f);
    skinned.assign(cube.size(), GNg::sVEC3{0.0f, 0.0f, 0.0f});
    CHECK(skinVertices(cube.data(), (uint32_t)cube.size(), entity.aSkeleton.Palette().data(),
                       (uint32_t)entity.aSkeleton.Palette().size(), skinned.data(),
                       normals.data()));
    bool upperMoved = false;
    for (size_t i = 0; i < cube.size(); ++i) {
        if (cube[i].aPosition.y <= 0.0f) continue;
        const float dx = skinned[i].x - cube[i].aPosition.x;
        const float dz = skinned[i].z - cube[i].aPosition.z;
        if (std::sqrt(dx * dx + dz * dz) > 0.3f) upperMoved = true;
    }
    CHECK(upperMoved);

    // 调色板也跟着变了（关节 1 的旋转部分不再是单位阵）
    const GNg::sMAT44 &palette1 = entity.aSkeleton.Palette()[1];
    CHECK(std::fabs(palette1.m[1]) > 0.5f || std::fabs(palette1.m[4]) > 0.5f);
}

// 手工播放与状态机混用：Stop() 必须同时退出状态驱动，不能"停下了又在播"。
TEST_CASE(entityAnimManualAndMachineCoexist) {
    cENTITY entity;
    const sANIM_CLIP clip0 = makeRotationClip(0, 0.0f, 0.0f, 1.0f);
    CHECK(entity.aAnim.RegisterClip("pose0", &clip0));
    const uint16_t state = entity.aAnim.StateMachine().AddState("pose0", "pose0", true);
    CHECK(entity.aAnim.StateMachine().SetInitial(state));
    CHECK(entity.aAnim.StartStateMachine());
    CHECK(entity.aAnim.StateMachineActive());

    entity.aAnim.Stop();
    CHECK(!entity.aAnim.StateMachineActive());
    CHECK(!entity.aAnim.Playing());
    CHECK(entity.aAnim.Current()[0] == '\0');

    // 手工播放仍然可用（同一张 clip 表）
    CHECK(entity.aAnim.Play("pose0", true, 1.0f));
    CHECK(entity.aAnim.Playing());
    CHECK_STREQ(entity.aAnim.Current(), "pose0");
}

// 没有初始状态 / clip 缺失：Start 失败但不崩。
TEST_CASE(entityAnimMachineStartRequiresInitial) {
    cANIM_STATE_MACHINE machine;
    cANIM_PLAYER player;
    CHECK(!machine.Start(player)); // 没有初始状态
    CHECK(!machine.Started());

    sANIM_PARAMS params;
    CHECK(!machine.Update(0.016f, params)); // 没 Start 也不行
    CHECK(machine.CurrentState() == cANIM_STATE_MACHINE::INVALID_STATE);

    // 清空后 AddTransition 一律失败
    machine.Clear();
    CHECK_EQ(machine.ClipCount(), 0u);
    CHECK_EQ(machine.StateCount(), 0u);
    CHECK(!machine.AddTransition(0, 1, eP_SPEED, eANIM_CMP_GT, 0.0f));
}
