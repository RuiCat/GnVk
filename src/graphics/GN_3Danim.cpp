// GN_3Danim.cpp — 动画 clip 采样（STEP/LINEAR）与 cANIM_PLAYER（play/crossFade/loop）。
//
// 采样约定：
//   * 时间先钳到通道首/末关键帧；STEP 取前一帧，LINEAR 在相邻帧间插值（旋转用 QuatSlerp 最短弧）。
//   * 节点首次命中通道时把 out 重置为默认（T=0、R=单位、S=1），未覆盖的分量保持默认。
#include <GN.h>
#include "GN_3Danim.h"

#include <cmath>

namespace GN3d {

namespace {

GNg::sVEC3 makeVec3(float x, float y, float z) {
    GNg::sVEC3 v = {x, y, z};
    return v;
}

GNg::sVEC3 lerpVec(const GNg::sVEC3 &a, const GNg::sVEC3 &b, float t) {
    return makeVec3(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t);
}

void defaultTransform(sANIM_TRANSFORM &out) {
    out.aTranslation = makeVec3(0.0f, 0.0f, 0.0f);
    out.aRotation = GNg::sQUAT();
    out.aScale = makeVec3(1.0f, 1.0f, 1.0f);
}

// 在通道上采样一个分量；关键帧不足/值数量不匹配返回 false。
bool sampleChannel(const sANIM_CHANNEL &channel, float time, sANIM_TRANSFORM &out) {
    const size_t keyCount = channel.aTimes.size();
    if(keyCount == 0) return false;
    const bool isRotation = (channel.aPath == eANIM_ROTATION);
    if(isRotation) {
        if(channel.aQuatValues.size() < keyCount) return false;
    } else if(channel.aVecValues.size() < keyCount) {
        return false;
    }

    float clamped = time;
    if(clamped < channel.aTimes[0]) clamped = channel.aTimes[0];
    if(clamped > channel.aTimes[keyCount - 1u]) clamped = channel.aTimes[keyCount - 1u];

    size_t index = 0;
    if(keyCount == 1u) {
        index = 0;
    } else {
        while(index + 2u < keyCount && channel.aTimes[index + 1u] <= clamped) ++index;
    }
    float alpha = 0.0f;
    if(keyCount > 1u && channel.aTimes[index + 1u] > channel.aTimes[index]) {
        alpha = (clamped - channel.aTimes[index]) / (channel.aTimes[index + 1u] - channel.aTimes[index]);
    }
    if(alpha < 0.0f) alpha = 0.0f;
    if(alpha > 1.0f) alpha = 1.0f;
    if(channel.aInterp == eANIM_STEP) alpha = 0.0f;

    if(isRotation) {
        GNg::sQUAT rotation;
        if(keyCount == 1u || alpha <= 0.0f) {
            rotation = channel.aQuatValues[index];
        } else {
            GNg::QuatSlerp(&rotation, &channel.aQuatValues[index], &channel.aQuatValues[index + 1u], alpha);
        }
        out.aRotation = rotation;
    } else {
        const GNg::sVEC3 &from = channel.aVecValues[index];
        const GNg::sVEC3 value = (keyCount == 1u) ? from
                                                  : lerpVec(from, channel.aVecValues[index + 1u], alpha);
        if(channel.aPath == eANIM_TRANSLATION) {
            out.aTranslation = value;
        } else {
            out.aScale = value;
        }
    }
    return true;
}

} // namespace

void sANIM_CLIP::updateDuration() {
    aDuration = 0.0f;
    for(size_t c = 0; c < aChannels.size(); ++c) {
        const sANIM_CHANNEL &channel = aChannels[c];
        if(channel.aTimes.empty()) continue;
        const float last = channel.aTimes.back();
        if(last > aDuration) aDuration = last;
    }
}

bool sANIM_CLIP::sample(uint32_t node, float time, sANIM_TRANSFORM &out) const {
    bool found = false;
    for(size_t c = 0; c < aChannels.size(); ++c) {
        const sANIM_CHANNEL &channel = aChannels[c];
        if(channel.aNode != node) continue;
        if(!found) {
            defaultTransform(out); // 首次命中：未覆盖的分量保持默认
            found = true;
        }
        sampleChannel(channel, time, out);
    }
    return found;
}

cANIM_PLAYER::cANIM_PLAYER()
    : aClip(0), aNextClip(0), aTime(0.0f), aNextTime(0.0f), aSpeed(1.0f), aLoop(false),
      aNextLoop(false), aFading(false), aFadeElapsed(0.0f), aFadeDuration(0.0f), aFadeWeight(1.0f) {}

void cANIM_PLAYER::play(const sANIM_CLIP *clip, bool loop, float speed) {
    aClip = clip;
    aTime = 0.0f;
    aLoop = loop;
    aSpeed = (speed > 0.0f) ? speed : 0.0f;
    aNextClip = 0;
    aNextLoop = false;
    aNextTime = 0.0f;
    aFading = false;
    aFadeElapsed = 0.0f;
    aFadeDuration = 0.0f;
    aFadeWeight = 1.0f;
}

void cANIM_PLAYER::crossFade(const sANIM_CLIP *clip, float duration, bool loop) {
    if(aClip == 0 || duration <= 0.0f) {
        play(clip, loop, aSpeed > 0.0f ? aSpeed : 1.0f);
        return;
    }
    aNextClip = clip;
    aNextTime = 0.0f;
    aNextLoop = loop;
    aFading = true;
    aFadeElapsed = 0.0f;
    aFadeDuration = duration;
    aFadeWeight = 1.0f;
}

void cANIM_PLAYER::stop() {
    aClip = 0;
    aNextClip = 0;
    aFading = false;
    aTime = 0.0f;
    aNextTime = 0.0f;
    aFadeWeight = 1.0f;
}

void cANIM_PLAYER::advance(float &time, const sANIM_CLIP *clip, bool loop, float delta) {
    if(clip == 0 || !(delta > 0.0f)) return;
    const float duration = clip->aDuration;
    if(!(duration > 0.0f)) {
        time = 0.0f;
        return;
    }
    time += delta;
    if(loop) {
        while(time >= duration) time -= duration;
    } else if(time > duration) {
        time = duration;
    }
}

void cANIM_PLAYER::update(float dt) {
    if(aClip == 0 || !(dt > 0.0f)) return;
    advance(aTime, aClip, aLoop, dt * aSpeed);
    if(!aFading || aNextClip == 0) return;
    aFadeElapsed += dt;
    if(aFadeDuration > 0.0f) {
        float progress = aFadeElapsed / aFadeDuration;
        if(progress > 1.0f) progress = 1.0f;
        aFadeWeight = 1.0f - progress;
    }
    advance(aNextTime, aNextClip, aNextLoop, dt * aSpeed);
    if(aFadeElapsed >= aFadeDuration) finishFade();
}

void cANIM_PLAYER::finishFade() {
    aClip = aNextClip;
    aTime = aNextTime;
    aLoop = aNextLoop;
    aNextClip = 0;
    aNextTime = 0.0f;
    aFading = false;
    aFadeElapsed = 0.0f;
    aFadeDuration = 0.0f;
    aFadeWeight = 1.0f;
}

bool cANIM_PLAYER::sample(uint32_t node, sANIM_TRANSFORM &out) const {
    if(aClip == 0) return false;
    sANIM_TRANSFORM current;
    const bool hasCurrent = aClip->sample(node, aTime, current);
    if(!aFading || aNextClip == 0) {
        if(hasCurrent) out = current;
        return hasCurrent;
    }

    sANIM_TRANSFORM next;
    const bool hasNext = aNextClip->sample(node, aNextTime, next);
    if(hasCurrent && hasNext) {
        const float nextWeight = 1.0f - aFadeWeight;
        out.aTranslation = lerpVec(current.aTranslation, next.aTranslation, nextWeight);
        out.aScale = lerpVec(current.aScale, next.aScale, nextWeight);
        GNg::QuatSlerp(&out.aRotation, &current.aRotation, &next.aRotation, nextWeight);
        return true;
    }
    if(hasCurrent) {
        out = current;
        return true;
    }
    if(hasNext) {
        out = next;
        return true;
    }
    return false;
}

bool cANIM_PLAYER::isPlaying() const {
    if(aClip == 0) return false;
    if(aLoop) return true;
    return aTime < aClip->aDuration;
}

} // namespace GN3d
