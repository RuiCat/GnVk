// testAnimBlend.cpp — 混合树（1D 混合空间）与动画层遮罩（D3），纯 CPU。
//
// 这一组用例围绕三条语义：
//   1. 权重：只在相邻点间插值、端点外钳制、和为 1；
//   2. 相位同步：各 clip 用自己的 duration 换算时间（walk/run 混合不"两腿各走各的"）；
//   3. 遮罩："上层没覆盖的关节保持不变"，而不是回 T-pose。
#include "testFramework.h"

#include <GN.h>
#include "GN_AnimBlend.h"

#include <cmath>
#include <cstring>
#include <vector>

using namespace GN3d;

namespace {

// 造一段单节点、单通道（平移 X）的 clip。
// 关键帧：t=0 → x=fromX，t=duration → x=toX（LINEAR）。
sANIM_CLIP *makeMoveClip(uint32_t node, float duration, float fromX, float toX) {
    sANIM_CLIP *clip = new sANIM_CLIP();
    sANIM_CHANNEL channel;
    channel.aNode = node;
    channel.aPath = eANIM_TRANSLATION;
    channel.aInterp = eANIM_LINEAR;
    channel.aTimes.push_back(0.0f);
    channel.aTimes.push_back(duration);
    channel.aVecValues.push_back(GNg::sVEC3{fromX, 0.0f, 0.0f});
    channel.aVecValues.push_back(GNg::sVEC3{toX, 0.0f, 0.0f});
    clip->aChannels.push_back(channel);
    clip->updateDuration();
    return clip;
}

// 造一段"旋转 90°"的 clip，用于验证 slerp 路径
sANIM_CLIP *makeRotateClip(uint32_t node, float duration) {
    sANIM_CLIP *clip = new sANIM_CLIP();
    sANIM_CHANNEL channel;
    channel.aNode = node;
    channel.aPath = eANIM_ROTATION;
    channel.aInterp = eANIM_LINEAR;
    channel.aTimes.push_back(0.0f);
    channel.aTimes.push_back(duration);
    GNg::sQUAT identity = {};
    identity.w = 1.0f;
    GNg::sQUAT turned = {};
    // 绕 Y 轴 90°：w=cos45, y=sin45
    turned.w = std::cos(0.7853981f);
    turned.y = std::sin(0.7853981f);
    channel.aQuatValues.push_back(identity);
    channel.aQuatValues.push_back(turned);
    clip->aChannels.push_back(channel);
    clip->updateDuration();
    return clip;
}

// 给已有 clip 追加一个"常量平移"通道（用于搭多关节场景）
void addTranslateChannel(sANIM_CLIP *clip, uint32_t node, const GNg::sVEC3 &from,
                         const GNg::sVEC3 &to) {
    sANIM_CHANNEL channel;
    channel.aNode = node;
    channel.aPath = eANIM_TRANSLATION;
    channel.aInterp = eANIM_LINEAR;
    channel.aTimes.push_back(0.0f);
    channel.aTimes.push_back(1.0f);
    channel.aVecValues.push_back(from);
    channel.aVecValues.push_back(to);
    clip->aChannels.push_back(channel);
    clip->updateDuration();
}

void freeClips(std::vector<sANIM_CLIP *> &clips) {
    for (size_t i = 0; i < clips.size(); ++i) delete clips[i];
    clips.clear();
}

} // namespace

// ── 权重计算 ───────────────────────────────────────────
TEST_CASE(blendWeightsLinearBetweenPoints) {
    std::vector<sANIM_CLIP *> clips;
    clips.push_back(makeMoveClip(0, 1.0f, 0.0f, 1.0f)); // walk @ 1.5
    clips.push_back(makeMoveClip(0, 0.8f, 0.0f, 2.0f)); // run  @ 4.5

    cANIM_BLEND_1D space;
    CHECK(space.AddPoint(1.5f, clips[0]));
    CHECK(space.AddPoint(4.5f, clips[1]));
    CHECK_EQ(space.PointCount(), 2u);

    std::vector<float> w;

    // 端点：单点权重 1
    space.EvaluateWeights(1.5f, w);
    CHECK_EQ((uint32_t)w.size(), 2u);
    CHECK_NEAR(w[0], 1.0f, 1e-6);
    CHECK_NEAR(w[1], 0.0f, 1e-6);

    // 中点：各 0.5
    space.EvaluateWeights(3.0f, w);
    CHECK_NEAR(w[0], 0.5f, 1e-6);
    CHECK_NEAR(w[1], 0.5f, 1e-6);

    // 1/4 处
    space.EvaluateWeights(2.25f, w);
    CHECK_NEAR(w[0], 0.75f, 1e-6);
    CHECK_NEAR(w[1], 0.25f, 1e-6);

    // **端点外钳制**（不外推）
    space.EvaluateWeights(0.0f, w);
    CHECK_NEAR(w[0], 1.0f, 1e-6);
    space.EvaluateWeights(99.0f, w);
    CHECK_NEAR(w[1], 1.0f, 1e-6);

    // 权重和恒为 1
    for (float p = 0.0f; p <= 6.0f; p += 0.37f) {
        space.EvaluateWeights(p, w);
        float sum = 0.0f;
        for (size_t i = 0; i < w.size(); ++i) sum += w[i];
        CHECK_NEAR(sum, 1.0f, 1e-5);
    }
    freeClips(clips);
}

TEST_CASE(blendWeightsRejectBadPoints) {
    std::vector<sANIM_CLIP *> clips;
    clips.push_back(makeMoveClip(0, 1.0f, 0.0f, 1.0f));
    cANIM_BLEND_1D space;
    CHECK(space.AddPoint(1.0f, clips[0]));
    // 重复阈值：拒绝（否则权重插值出现 0/0）
    CHECK(!space.AddPoint(1.0f, clips[0]));
    CHECK_EQ(space.PointCount(), 1u);
    // 空 clip：拒绝
    CHECK(!space.AddPoint(2.0f, nullptr));
    CHECK_EQ(space.PointCount(), 1u);
    // 非有限阈值：拒绝
    CHECK(!space.AddPoint(std::nanf(""), clips[0]));
    CHECK_EQ(space.PointCount(), 1u);
    freeClips(clips);
}

// 采样点必须按阈值有序（乱序加入也要正确）
TEST_CASE(blendPointsSortedByThreshold) {
    std::vector<sANIM_CLIP *> clips;
    clips.push_back(makeMoveClip(0, 1.0f, 0.0f, 1.0f));
    clips.push_back(makeMoveClip(0, 1.0f, 0.0f, 2.0f));
    clips.push_back(makeMoveClip(0, 1.0f, 0.0f, 3.0f));
    cANIM_BLEND_1D space;
    CHECK(space.AddPoint(5.0f, clips[2])); // 乱序加入
    CHECK(space.AddPoint(1.0f, clips[0]));
    CHECK(space.AddPoint(3.0f, clips[1]));
    CHECK_EQ(space.PointCount(), 3u);
    CHECK_NEAR(space.PointAt(0)->aThreshold, 1.0f, 1e-6);
    CHECK_NEAR(space.PointAt(1)->aThreshold, 3.0f, 1e-6);
    CHECK_NEAR(space.PointAt(2)->aThreshold, 5.0f, 1e-6);
    CHECK(space.PointAt(3) == nullptr); // 越界

    // 中间段插值：2.0 落在 [1,3] 的中点
    std::vector<float> w;
    space.EvaluateWeights(2.0f, w);
    CHECK_NEAR(w[0], 0.5f, 1e-6);
    CHECK_NEAR(w[1], 0.5f, 1e-6);
    CHECK_NEAR(w[2], 0.0f, 1e-6);
    freeClips(clips);
}

// ── 相位同步：这是混合树最关键的一条 ──────────────────
// walk(1.0s 循环) 与 run(0.4s 循环) 混在相位 0.5 时，两者都应处于各自循环的中点。
TEST_CASE(blendPhaseSyncUsesPerClipDuration) {
    std::vector<sANIM_CLIP *> clips;
    // walk: 0→1 over 1.0s ⇒ 相位 0.5 时 x=0.5
    clips.push_back(makeMoveClip(0, 1.0f, 0.0f, 1.0f));
    // run:  0→4 over 0.4s ⇒ 相位 0.5 时 x=2.0
    clips.push_back(makeMoveClip(0, 0.4f, 0.0f, 4.0f));

    cANIM_BLEND_1D space;
    space.AddPoint(0.0f, clips[0]);
    space.AddPoint(10.0f, clips[1]);
    CHECK_NEAR(space.Duration(), 1.0f, 1e-6); // 取最大 duration

    space.Play(true);
    space.SetPhase(0.5f);
    sANIM_TRANSFORM out;
    // 参数 0 ⇒ 纯 walk
    space.SetParameter(0.0f);
    CHECK(space.Sample(0, out));
    CHECK_NEAR(out.aTranslation.x, 0.5f, 1e-5); // walk 相位 0.5

    // 参数 10 ⇒ 纯 run
    space.SetParameter(10.0f);
    CHECK(space.Sample(0, out));
    CHECK_NEAR(out.aTranslation.x, 2.0f, 1e-5); // run 相位 0.5（用 0.4s 换算）

    // 混合：x = 0.5*0.5 + 0.5*2.0 = 1.25
    space.SetParameter(5.0f);
    CHECK(space.Sample(0, out));
    CHECK_NEAR(out.aTranslation.x, 1.25f, 1e-5);
    freeClips(clips);
}

TEST_CASE(blendPhaseAdvancesAndLoops) {
    std::vector<sANIM_CLIP *> clips;
    clips.push_back(makeMoveClip(0, 2.0f, 0.0f, 1.0f)); // duration 2.0
    cANIM_BLEND_1D space;
    space.AddPoint(0.0f, clips[0]);
    space.Play(true);
    CHECK_NEAR(space.Phase(), 0.0f, 1e-6);

    space.Update(0.5f); // 0.5/2.0 = 0.25
    CHECK_NEAR(space.Phase(), 0.25f, 1e-5);
    space.Update(0.5f);
    CHECK_NEAR(space.Phase(), 0.5f, 1e-5);

    // 回绕：再推进 1.5s ⇒ 相位 1.25 ⇒ 0.25
    space.Update(1.5f);
    CHECK_NEAR(space.Phase(), 0.25f, 1e-5);

    // 一次推进很多圈（掉帧）：仍落在 [0,1)
    space.Update(20.0f); // 20/2 = 10 圈
    CHECK(space.Phase() >= 0.0f);
    CHECK(space.Phase() < 1.0f);

    // 非 loop：钳到 1
    space.Play(false);
    space.SetPhase(0.0f);
    space.Update(5.0f);
    CHECK_NEAR(space.Phase(), 1.0f, 1e-5);

    // 非法 dt 不动
    const float before = space.Phase();
    space.Update(-1.0f);
    CHECK_NEAR(space.Phase(), before, 1e-6);
    space.Update(std::nanf(""));
    CHECK_NEAR(space.Phase(), before, 1e-6);
    freeClips(clips);
}

TEST_CASE(blendSetPhaseClamps) {
    std::vector<sANIM_CLIP *> clips;
    clips.push_back(makeMoveClip(0, 1.0f, 0.0f, 1.0f));
    cANIM_BLEND_1D space;
    space.AddPoint(0.0f, clips[0]);
    space.Play(true);
    space.SetPhase(-3.0f);
    CHECK_NEAR(space.Phase(), 0.0f, 1e-6);
    space.SetPhase(1.7f); // loop：回绕到 0.7
    CHECK_NEAR(space.Phase(), 0.7f, 1e-5);
    space.Play(false);
    space.SetPhase(1.7f); // 非 loop：钳到 1
    CHECK_NEAR(space.Phase(), 1.0f, 1e-6);
    freeClips(clips);
}

// ── 采样：按"有该节点通道的 clip"重新归一化 ──
TEST_CASE(blendSampleRenormalizesOverCoveringClips) {
    std::vector<sANIM_CLIP *> clips;
    // clip0 只覆盖 node 0
    clips.push_back(makeMoveClip(0, 1.0f, 10.0f, 10.0f));
    // clip1 覆盖 node 0 与 node 1
    sANIM_CLIP *both = makeMoveClip(0, 1.0f, 20.0f, 20.0f);
    sANIM_CHANNEL arm;
    arm.aNode = 1;
    arm.aPath = eANIM_TRANSLATION;
    arm.aInterp = eANIM_LINEAR;
    arm.aTimes.push_back(0.0f);
    arm.aTimes.push_back(1.0f);
    arm.aVecValues.push_back(GNg::sVEC3{0.0f, 5.0f, 0.0f});
    arm.aVecValues.push_back(GNg::sVEC3{0.0f, 5.0f, 0.0f});
    both->aChannels.push_back(arm);
    both->updateDuration();
    clips.push_back(both);

    cANIM_BLEND_1D space;
    space.AddPoint(0.0f, clips[0]);
    space.AddPoint(1.0f, clips[1]);
    space.Play(true);
    space.SetPhase(0.1f);

    // node 0：两段都有 → 按权重混合（参数 0.5 ⇒ 各半 ⇒ 15）
    space.SetParameter(0.5f);
    sANIM_TRANSFORM out;
    CHECK(space.Sample(0, out));
    CHECK_NEAR(out.aTranslation.x, 15.0f, 1e-4);

    // node 1：**只有 clip1 有** → 直接取 clip1，不被 clip0 的"缺失"拖到 0
    CHECK(space.Sample(1, out));
    CHECK_NEAR(out.aTranslation.y, 5.0f, 1e-4);

    // node 2：谁都没有 → false
    CHECK(!space.Sample(2, out));
    freeClips(clips);
}

TEST_CASE(blendSampleEmptyAndSinglePoint) {
    cANIM_BLEND_1D empty;
    sANIM_TRANSFORM out;
    CHECK(!empty.Sample(0, out));
    CHECK_EQ(empty.Duration() < 0.001f, true);

    std::vector<sANIM_CLIP *> clips;
    clips.push_back(makeMoveClip(0, 1.0f, 0.0f, 8.0f));
    cANIM_BLEND_1D single;
    single.AddPoint(0.0f, clips[0]);
    single.Play(true);
    single.SetPhase(0.5f);
    CHECK(single.Sample(0, out));
    CHECK_NEAR(out.aTranslation.x, 4.0f, 1e-5); // 单点退化为普通播放
    freeClips(clips);
}

// 旋转必须走 slerp（不能线性插值四元数）
TEST_CASE(blendRotationUsesSlerp) {
    std::vector<sANIM_CLIP *> clips;
    clips.push_back(makeRotateClip(0, 1.0f));
    cANIM_BLEND_1D space;
    space.AddPoint(0.0f, clips[0]);
    space.Play(true);
    space.SetPhase(0.5f); // 中途：应为 45°
    sANIM_TRANSFORM out;
    CHECK(space.Sample(0, out));
    // 四元数应是单位四元数（slerp 保持归一化）
    const float len = std::sqrt(out.aRotation.x * out.aRotation.x +
                                out.aRotation.y * out.aRotation.y +
                                out.aRotation.z * out.aRotation.z +
                                out.aRotation.w * out.aRotation.w);
    CHECK_NEAR(len, 1.0f, 1e-4);
    // 45° 绕 Y：w = cos(22.5°) ≈ 0.9239
    CHECK_NEAR(out.aRotation.w, 0.92388f, 1e-3);
    freeClips(clips);
}

// ── 动画层遮罩 ─────────────────────────────────────────
TEST_CASE(layerMaskAffectsOnlyMaskedJoints) {
    std::vector<sANIM_CLIP *> clips;
    // **基础层必须也覆盖关节 1**，否则 Sample(1) 一开始就 false，
    // 测不到"遮罩 0 时保持基础值"这件事（第一版就是这么写错的）。
    sANIM_CLIP *baseClip = makeMoveClip(0, 1.0f, 0.0f, 10.0f);
    addTranslateChannel(baseClip, 1, GNg::sVEC3{0.0f, 7.0f, 0.0f}, GNg::sVEC3{0.0f, 7.0f, 0.0f});
    clips.push_back(baseClip);
    sANIM_CLIP *upperClip = makeMoveClip(0, 1.0f, 100.0f, 110.0f);
    addTranslateChannel(upperClip, 1, GNg::sVEC3{0.0f, 99.0f, 0.0f}, GNg::sVEC3{0.0f, 99.0f, 0.0f});
    clips.push_back(upperClip);

    cANIM_BLEND_1D base;
    base.AddPoint(0.0f, clips[0]);
    base.Play(true);
    base.SetPhase(0.5f); // node0 x = 5, node1 y = 7

    cANIM_BLEND_1D upper;
    upper.AddPoint(0.0f, clips[1]);
    upper.Play(true);
    upper.SetPhase(0.5f); // node0 x = 105, node1 y = 99

    cANIM_LAYER_STACK stack;
    CHECK(stack.SetBase(&base));
    CHECK_EQ(stack.LayerCount(), 1u);

    // 遮罩：关节 0 完全生效、关节 1 完全不生效
    const float mask[2] = {1.0f, 0.0f};
    const uint32_t index = stack.AddLayer(&upper, 1.0f, mask, 2);
    CHECK_EQ(index, 1u);
    CHECK_EQ(stack.LayerCount(), 2u);

    sANIM_TRANSFORM out;
    // 关节 0：遮罩 1 ⇒ 完全被上层覆盖 ⇒ 105
    CHECK(stack.Sample(0, out));
    CHECK_NEAR(out.aTranslation.x, 105.0f, 1e-3);
    // 关节 1：遮罩 0 ⇒ 上层不生效，保持基础层 ⇒ 7（而不是上层的 99）
    CHECK(stack.Sample(1, out));
    CHECK_NEAR(out.aTranslation.y, 7.0f, 1e-3);
    freeClips(clips);
}

// **核心语义**：上层没覆盖的关节必须保持不变，不能回 T-pose
TEST_CASE(layerUncoveredJointsKeepBase) {
    std::vector<sANIM_CLIP *> clips;
    // 基础层：覆盖 node 0 与 node 1
    sANIM_CLIP *baseClip = makeMoveClip(0, 1.0f, 0.0f, 10.0f);
    sANIM_CHANNEL leg;
    leg.aNode = 1;
    leg.aPath = eANIM_TRANSLATION;
    leg.aInterp = eANIM_LINEAR;
    leg.aTimes.push_back(0.0f);
    leg.aTimes.push_back(1.0f);
    leg.aVecValues.push_back(GNg::sVEC3{0.0f, 7.0f, 0.0f});
    leg.aVecValues.push_back(GNg::sVEC3{0.0f, 7.0f, 0.0f});
    baseClip->aChannels.push_back(leg);
    baseClip->updateDuration();
    clips.push_back(baseClip);

    // 上层：只覆盖 node 0（模拟"上半身攻击动画只有手臂/脊柱通道"）
    clips.push_back(makeMoveClip(0, 1.0f, 100.0f, 110.0f));

    cANIM_BLEND_1D base;
    base.AddPoint(0.0f, clips[0]);
    base.Play(true);
    base.SetPhase(0.0f); // node0: x=0, node1: y=7

    cANIM_BLEND_1D upper;
    upper.AddPoint(0.0f, clips[1]);
    upper.Play(true);
    upper.SetPhase(0.0f);

    cANIM_LAYER_STACK stack;
    stack.SetBase(&base);
    // 无遮罩（全关节生效）
    stack.AddLayer(&upper, 1.0f);

    sANIM_TRANSFORM out;
    // node 0：被上层覆盖 ⇒ 100
    CHECK(stack.Sample(0, out));
    CHECK_NEAR(out.aTranslation.x, 100.0f, 1e-3);

    // node 1：**上层没有这个节点的通道** ⇒ 必须保持基础层的 y=7，
    // 而不是被上层"没有"影响成 0（那会让腿瞬间跳到原点）
    CHECK(stack.Sample(1, out));
    CHECK_NEAR(out.aTranslation.y, 7.0f, 1e-3);

    // node 2：谁都没有 ⇒ false
    CHECK(!stack.Sample(2, out));
    freeClips(clips);
}

TEST_CASE(layerWeightBlendsPartial) {
    std::vector<sANIM_CLIP *> clips;
    clips.push_back(makeMoveClip(0, 1.0f, 0.0f, 10.0f));  // 基础：x=0
    clips.push_back(makeMoveClip(0, 1.0f, 100.0f, 100.0f)); // 上层：x=100

    cANIM_BLEND_1D base;
    base.AddPoint(0.0f, clips[0]);
    base.Play(true);
    base.SetPhase(0.0f); // x=0

    cANIM_BLEND_1D upper;
    upper.AddPoint(0.0f, clips[1]);
    upper.Play(true);
    upper.SetPhase(0.0f); // x=100

    cANIM_LAYER_STACK stack;
    stack.SetBase(&base);
    stack.AddLayer(&upper, 0.25f); // 25% 权重

    sANIM_TRANSFORM out;
    CHECK(stack.Sample(0, out));
    // lerp(0, 100, 0.25) = 25
    CHECK_NEAR(out.aTranslation.x, 25.0f, 1e-3);

    // 权重 0 ⇒ 完全不生效
    stack.LayerAt(1)->aWeight = 0.0f;
    CHECK(stack.Sample(0, out));
    CHECK_NEAR(out.aTranslation.x, 0.0f, 1e-4);

    // 权重 1 ⇒ 完全覆盖
    stack.LayerAt(1)->aWeight = 1.0f;
    CHECK(stack.Sample(0, out));
    CHECK_NEAR(out.aTranslation.x, 100.0f, 1e-3);
    freeClips(clips);
}

TEST_CASE(layerStackRequiresBase) {
    std::vector<sANIM_CLIP *> clips;
    clips.push_back(makeMoveClip(0, 1.0f, 0.0f, 1.0f));
    cANIM_BLEND_1D base;
    base.AddPoint(0.0f, clips[0]);

    cANIM_LAYER_STACK stack;
    // 没有基础层时不能加覆盖层
    CHECK_EQ(stack.AddLayer(&base, 1.0f), 0u);
    CHECK_EQ(stack.LayerCount(), 0u);
    // 空 source 不能当基础层
    CHECK(!stack.SetBase(nullptr));
    CHECK_EQ(stack.LayerCount(), 0u);
    // 正常设置
    CHECK(stack.SetBase(&base));
    CHECK_EQ(stack.LayerCount(), 1u);
    // SetBase 会清空已有层
    stack.AddLayer(&base, 0.5f);
    CHECK_EQ(stack.LayerCount(), 2u);
    stack.SetBase(&base);
    CHECK_EQ(stack.LayerCount(), 1u);
    // 空 source 不能加层
    CHECK_EQ(stack.AddLayer(nullptr, 1.0f), 0u);
    // Clear
    stack.Clear();
    CHECK_EQ(stack.LayerCount(), 0u);
    CHECK(stack.LayerAt(0) == nullptr);
    freeClips(clips);
}

TEST_CASE(layerUpdateAdvancesAllSources) {
    std::vector<sANIM_CLIP *> clips;
    clips.push_back(makeMoveClip(0, 2.0f, 0.0f, 1.0f)); // duration 2
    clips.push_back(makeMoveClip(0, 4.0f, 0.0f, 1.0f)); // duration 4

    cANIM_BLEND_1D base;
    base.AddPoint(0.0f, clips[0]);
    base.Play(true);
    cANIM_BLEND_1D upper;
    upper.AddPoint(0.0f, clips[1]);
    upper.Play(true);

    cANIM_LAYER_STACK stack;
    stack.SetBase(&base);
    stack.AddLayer(&upper, 1.0f);
    stack.Update(1.0f); // base: 1/2=0.5, upper: 1/4=0.25

    CHECK_NEAR(base.Phase(), 0.5f, 1e-5);
    CHECK_NEAR(upper.Phase(), 0.25f, 1e-5);
    freeClips(clips);
}

// 遮罩越界：超出遮罩长度的关节不受该层影响（保守，不是"全 1"）
TEST_CASE(layerMaskOutOfRangeIsIgnored) {
    std::vector<sANIM_CLIP *> clips;
    clips.push_back(makeMoveClip(0, 1.0f, 0.0f, 10.0f));
    clips.push_back(makeMoveClip(0, 1.0f, 100.0f, 100.0f));

    cANIM_BLEND_1D base;
    base.AddPoint(0.0f, clips[0]);
    base.Play(true);
    base.SetPhase(0.0f);
    cANIM_BLEND_1D upper;
    upper.AddPoint(0.0f, clips[1]);
    upper.Play(true);
    upper.SetPhase(0.0f);

    cANIM_LAYER_STACK stack;
    stack.SetBase(&base);
    const float mask[1] = {1.0f}; // 只声明了 1 个关节
    stack.AddLayer(&upper, 1.0f, mask, 1);

    const sANIM_LAYER *layer = stack.LayerAt(1);
    CHECK(layer != nullptr);
    CHECK_NEAR(layer->WeightAt(0), 1.0f, 1e-6);
    CHECK_NEAR(layer->WeightAt(1), 0.0f, 1e-6); // 越界 ⇒ 0
    CHECK_NEAR(layer->WeightAt(99), 0.0f, 1e-6);
    freeClips(clips);
}

// 端到端：模拟"下半身走路 + 上半身攻击"
// 这是这两个类组合起来的真实用途。
TEST_CASE(layerEndToEndWalkAndAttack) {
    // 关节约定：0 = 腿，1 = 手臂
    std::vector<sANIM_CLIP *> clips;
    // 走路：腿 x 0→2，手臂不参与
    sANIM_CLIP *walk = makeMoveClip(0, 1.0f, 0.0f, 2.0f);
    clips.push_back(walk);
    // 攻击：手臂 y 0→3
    sANIM_CLIP *attack = new sANIM_CLIP();
    sANIM_CHANNEL arm;
    arm.aNode = 1;
    arm.aPath = eANIM_TRANSLATION;
    arm.aInterp = eANIM_LINEAR;
    arm.aTimes.push_back(0.0f);
    arm.aTimes.push_back(1.0f);
    arm.aVecValues.push_back(GNg::sVEC3{0.0f, 0.0f, 0.0f});
    arm.aVecValues.push_back(GNg::sVEC3{0.0f, 3.0f, 0.0f});
    attack->aChannels.push_back(arm);
    attack->updateDuration();
    clips.push_back(attack);

    cANIM_BLEND_1D locomotion; // 下半身
    locomotion.AddPoint(0.0f, walk);
    locomotion.Play(true);
    locomotion.SetPhase(0.5f); // 腿 x = 1

    cANIM_BLEND_1D action; // 上半身
    action.AddPoint(0.0f, attack);
    action.Play(true);
    action.SetPhase(0.5f); // 手臂 y = 1.5

    cANIM_LAYER_STACK stack;
    stack.SetBase(&locomotion);
    const float upperBodyMask[2] = {0.0f, 1.0f}; // 只作用于手臂
    stack.AddLayer(&action, 1.0f, upperBodyMask, 2);

    sANIM_TRANSFORM out;
    // 腿：基础层照常走（x=1），不被攻击层影响
    CHECK(stack.Sample(0, out));
    CHECK_NEAR(out.aTranslation.x, 1.0f, 1e-4);

    // 手臂：基础层（walk）没有手臂通道，攻击层接管
    CHECK(stack.Sample(1, out));
    CHECK_NEAR(out.aTranslation.y, 1.5f, 1e-4);
    freeClips(clips);
}

// ═══════════════════════════════════════════════════════════
//  接入 cANIM_CONTROLLER（引擎侧接线）
// ═══════════════════════════════════════════════════════════
// 这一组验证"混合树能被实体直接用"：clip 走 RegisterClip，混合空间按名字建，
// Update/Sample 自动切到层栈。
TEST_CASE(controllerLayeredBlendEndToEnd) {
    std::vector<sANIM_CLIP *> clips;
    clips.push_back(makeMoveClip(0, 1.0f, 0.0f, 1.0f));  // walk: x 0→1
    clips.push_back(makeMoveClip(0, 1.0f, 0.0f, 4.0f));  // run:  x 0→4

    cENTITY entity;
    CHECK(entity.aAnim.RegisterClip("walk", clips[0]));
    CHECK(entity.aAnim.RegisterClip("run", clips[1]));

    // 建混合空间 + 两个点
    CHECK(entity.aAnim.AddBlendSpace("locomotion"));
    CHECK(!entity.aAnim.AddBlendSpace("locomotion")); // 重名
    CHECK(entity.aAnim.AddBlendPoint("locomotion", 1.5f, "walk"));
    CHECK(entity.aAnim.AddBlendPoint("locomotion", 4.5f, "run"));
    // clip 不存在 ⇒ 拒绝
    CHECK(!entity.aAnim.AddBlendPoint("locomotion", 9.0f, "nosuchclip"));
    // 阈值重复 ⇒ 拒绝
    CHECK(!entity.aAnim.AddBlendPoint("locomotion", 1.5f, "run"));

    CHECK(!entity.aAnim.LayeredActive()); // 还没设基础层
    CHECK(entity.aAnim.SetBaseBlendSpace("locomotion"));
    CHECK(entity.aAnim.LayeredActive());
    CHECK_EQ(entity.aAnim.LayerStack().LayerCount(), 1u);

    // 参数 3.0（中点）⇒ 权重各半
    CHECK(entity.aAnim.SetBlendParameter("locomotion", 3.0f));
    entity.aAnim.Update(0.5f); // 相位 0.5

    sANIM_TRANSFORM out;
    CHECK(entity.aAnim.Sample(0, out));
    // walk 相位 0.5 ⇒ x=0.5；run 相位 0.5 ⇒ x=2.0；各半 ⇒ 1.25
    CHECK_NEAR(out.aTranslation.x, 1.25f, 1e-4);

    // 参数拉到纯 run
    entity.aAnim.SetBlendParameter("locomotion", 4.5f);
    CHECK(entity.aAnim.Sample(0, out));
    CHECK_NEAR(out.aTranslation.x, 2.0f, 1e-4);

    // StopLayered 回到单播放器（未播放 ⇒ Sample false）
    entity.aAnim.StopLayered();
    CHECK(!entity.aAnim.LayeredActive());
    CHECK_EQ(entity.aAnim.LayerStack().LayerCount(), 0u);
    CHECK(!entity.aAnim.Sample(0, out));
    freeClips(clips);
}

TEST_CASE(controllerLayerRequiresBaseAndValidSpace) {
    std::vector<sANIM_CLIP *> clips;
    clips.push_back(makeMoveClip(0, 1.0f, 0.0f, 1.0f));
    cENTITY entity;
    entity.aAnim.RegisterClip("a", clips[0]);

    // 没有基础层时不能加覆盖层
    CHECK(!entity.aAnim.AddBlendLayer("x", 1.0f));
    // 空空间不能当基础层
    CHECK(entity.aAnim.AddBlendSpace("empty"));
    CHECK(!entity.aAnim.SetBaseBlendSpace("empty"));
    // 不存在的空间
    CHECK(!entity.aAnim.SetBaseBlendSpace("nosuch"));
    CHECK(!entity.aAnim.SetBlendParameter("nosuch", 1.0f));
    // 空名字
    CHECK(!entity.aAnim.AddBlendSpace(""));
    CHECK(!entity.aAnim.AddBlendSpace(nullptr));

    // 正常路径
    CHECK(entity.aAnim.AddBlendSpace("base"));
    CHECK(entity.aAnim.AddBlendPoint("base", 0.0f, "a"));
    CHECK(entity.aAnim.SetBaseBlendSpace("base"));

    CHECK(entity.aAnim.AddBlendSpace("upper"));
    CHECK(entity.aAnim.AddBlendPoint("upper", 0.0f, "a"));
    const float mask[1] = {0.0f};
    // 遮罩全 0：层加上了但不影响任何关节
    CHECK(entity.aAnim.AddBlendLayer("upper", 1.0f, mask, 1));
    CHECK_EQ(entity.aAnim.LayerStack().LayerCount(), 2u);
    freeClips(clips);
}

// 混合树接管后，状态机与单播放器都让位
TEST_CASE(controllerLayeredTakesPriority) {
    std::vector<sANIM_CLIP *> clips;
    clips.push_back(makeMoveClip(0, 1.0f, 0.0f, 10.0f)); // 播放器用
    clips.push_back(makeMoveClip(0, 1.0f, 0.0f, 99.0f)); // 混合树用
    cENTITY entity;
    entity.aAnim.RegisterClip("player", clips[0]);
    entity.aAnim.RegisterClip("blend", clips[1]);

    // 先让单播放器跑起来
    CHECK(entity.aAnim.Play("player", true, 1.0f));
    entity.aAnim.Update(0.5f);
    sANIM_TRANSFORM out;
    CHECK(entity.aAnim.Sample(0, out));
    CHECK_NEAR(out.aTranslation.x, 5.0f, 1e-3);

    // 启用混合树：立刻接管
    entity.aAnim.AddBlendSpace("s");
    entity.aAnim.AddBlendPoint("s", 0.0f, "blend");
    CHECK(entity.aAnim.SetBaseBlendSpace("s"));
    CHECK(!entity.aAnim.Playing()); // 播放器被停掉
    entity.aAnim.Update(0.5f);
    CHECK(entity.aAnim.Sample(0, out));
    CHECK_NEAR(out.aTranslation.x, 49.5f, 1e-3); // 混合树的值（99 * 0.5）
    freeClips(clips);
}
