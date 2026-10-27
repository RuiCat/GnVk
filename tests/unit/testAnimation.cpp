// testAnimation.cpp — 动画 clip 采样（STEP/LINEAR/slerp）、未覆盖节点、播放器 loop/crossFade/非循环停住。
#include "testFramework.h"

#include <GN.h>
#include "GN_3Danim.h"

#include <cmath>
#include <vector>

using namespace GN3d;

namespace {

sANIM_CHANNEL makeTranslationChannel(uint32_t node, const std::vector<float> &times,
                                     const std::vector<GNg::sVEC3> &values, eANIM_INTERP interp) {
    sANIM_CHANNEL channel;
    channel.aNode = node;
    channel.aPath = eANIM_TRANSLATION;
    channel.aInterp = interp;
    channel.aTimes = times;
    channel.aVecValues = values;
    return channel;
}

sANIM_CHANNEL makeStepScaleChannel(uint32_t node) {
    sANIM_CHANNEL channel;
    channel.aNode = node;
    channel.aPath = eANIM_SCALE;
    channel.aInterp = eANIM_STEP;
    channel.aTimes.push_back(0.0f);
    channel.aTimes.push_back(1.0f);
    channel.aVecValues.push_back(GNg::sVEC3{1.0f, 1.0f, 1.0f});
    channel.aVecValues.push_back(GNg::sVEC3{2.0f, 2.0f, 2.0f});
    return channel;
}

} // namespace

// clip 采样：LINEAR 平移中点、STEP 取前一帧、duration 由关键帧刷新。
TEST_CASE(animClipStepAndLinear) {
    sANIM_CLIP clip;
    clip.aChannels.push_back(makeTranslationChannel(
        0, {0.0f, 1.0f}, {GNg::sVEC3{0.0f, 0.0f, 0.0f}, GNg::sVEC3{10.0f, 0.0f, 0.0f}}, eANIM_LINEAR));
    clip.aChannels.push_back(makeStepScaleChannel(1));
    clip.updateDuration();
    CHECK_NEAR(clip.aDuration, 1.0f, 1e-6f);

    sANIM_TRANSFORM transform;
    CHECK(clip.sample(0, 0.5f, transform));
    CHECK_NEAR(transform.aTranslation.x, 5.0f, 1e-4f);
    CHECK_NEAR(transform.aRotation.w, 1.0f, 1e-5f); // 未覆盖分量保持默认
    CHECK_NEAR(transform.aScale.x, 1.0f, 1e-5f);

    CHECK(clip.sample(1, 0.75f, transform));
    CHECK_NEAR(transform.aScale.x, 1.0f, 1e-5f); // STEP：t=0.75 仍取前一帧
    CHECK_NEAR(transform.aTranslation.x, 0.0f, 1e-5f); // 节点 1 无平移通道 ⇒ 默认
}

// 旋转：identity → 90°(Y)，t=0.5 用最短弧 slerp 得到 45°。
TEST_CASE(animRotationSlerp) {
    sANIM_CHANNEL channel;
    channel.aNode = 0;
    channel.aPath = eANIM_ROTATION;
    channel.aInterp = eANIM_LINEAR;
    channel.aTimes = {0.0f, 1.0f};
    channel.aQuatValues.push_back(GNg::sQUAT());
    GNg::sVEC3 yAxis{0.0f, 1.0f, 0.0f};
    GNg::sQUAT ninety;
    GNg::QuatFromAxisAngle(&ninety, &yAxis, 1.5707963f);
    channel.aQuatValues.push_back(ninety);

    sANIM_CLIP clip;
    clip.aChannels.push_back(channel);
    clip.updateDuration();
    sANIM_TRANSFORM transform;
    CHECK(clip.sample(0, 0.5f, transform));
    CHECK_NEAR(transform.aRotation.y, 0.3826834f, 1e-4f); // sin(22.5°)
    CHECK_NEAR(transform.aRotation.w, 0.9238795f, 1e-4f); // cos(22.5°)
}

// 未覆盖节点：sample 返回 false 且不修改 out。
TEST_CASE(animUnanimatedNodeUntouched) {
    sANIM_CLIP clip;
    clip.aChannels.push_back(makeTranslationChannel(
        0, {0.0f, 1.0f}, {GNg::sVEC3{0.0f, 0.0f, 0.0f}, GNg::sVEC3{1.0f, 0.0f, 0.0f}}, eANIM_LINEAR));
    clip.updateDuration();
    sANIM_TRANSFORM transform;
    transform.aTranslation = GNg::sVEC3{7.0f, 8.0f, 9.0f};
    CHECK(!clip.sample(5, 0.5f, transform));
    CHECK_NEAR(transform.aTranslation.x, 7.0f, 1e-6f);
    CHECK_NEAR(transform.aTranslation.y, 8.0f, 1e-6f);
}

// 播放器 loop：时间按 duration 回绕。
TEST_CASE(animPlayerLoopWraps) {
    sANIM_CLIP clip;
    clip.aChannels.push_back(makeTranslationChannel(
        0, {0.0f, 1.0f}, {GNg::sVEC3{0.0f, 0.0f, 0.0f}, GNg::sVEC3{10.0f, 0.0f, 0.0f}}, eANIM_LINEAR));
    clip.updateDuration();

    cANIM_PLAYER player;
    player.play(&clip, true, 1.0f);
    player.update(1.25f);
    CHECK_NEAR(player.time(), 0.25f, 1e-5f);
    CHECK(player.isPlaying());
    sANIM_TRANSFORM transform;
    CHECK(player.sample(0, transform));
    CHECK_NEAR(transform.aTranslation.x, 2.5f, 1e-4f);
}

// crossFade：两段常量平移 clip，权重线性过渡后切到下一段。
TEST_CASE(animPlayerCrossFade) {
    sANIM_CLIP from;
    from.aChannels.push_back(makeTranslationChannel(
        0, {0.0f, 1.0f}, {GNg::sVEC3{0.0f, 0.0f, 0.0f}, GNg::sVEC3{0.0f, 0.0f, 0.0f}}, eANIM_LINEAR));
    from.updateDuration();
    sANIM_CLIP to;
    to.aChannels.push_back(makeTranslationChannel(
        0, {0.0f, 1.0f}, {GNg::sVEC3{10.0f, 0.0f, 0.0f}, GNg::sVEC3{10.0f, 0.0f, 0.0f}}, eANIM_LINEAR));
    to.updateDuration();

    cANIM_PLAYER player;
    player.play(&from, true, 1.0f);
    player.crossFade(&to, 1.0f, true);
    player.update(0.5f);
    CHECK_NEAR(player.blendWeight(), 0.5f, 1e-4f);
    sANIM_TRANSFORM transform;
    CHECK(player.sample(0, transform));
    CHECK_NEAR(transform.aTranslation.x, 5.0f, 1e-3f);
    player.update(0.6f); // 淡入完成 ⇒ 只剩 to
    CHECK_NEAR(player.blendWeight(), 1.0f, 1e-5f);
    CHECK(player.sample(0, transform));
    CHECK_NEAR(transform.aTranslation.x, 10.0f, 1e-4f);
}

// 非循环：到末尾停住，isPlaying=false，采样停在最后一帧。
TEST_CASE(animPlayerNonLoopStops) {
    sANIM_CLIP clip;
    clip.aChannels.push_back(makeTranslationChannel(
        0, {0.0f, 1.0f}, {GNg::sVEC3{0.0f, 0.0f, 0.0f}, GNg::sVEC3{10.0f, 0.0f, 0.0f}}, eANIM_LINEAR));
    clip.updateDuration();

    cANIM_PLAYER player;
    player.play(&clip, false, 1.0f);
    player.update(2.0f);
    CHECK_NEAR(player.time(), 1.0f, 1e-5f);
    CHECK(!player.isPlaying());
    sANIM_TRANSFORM transform;
    CHECK(player.sample(0, transform));
    CHECK_NEAR(transform.aTranslation.x, 10.0f, 1e-4f);
}
