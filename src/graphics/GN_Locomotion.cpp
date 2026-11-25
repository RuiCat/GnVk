// GN_Locomotion.cpp — 选段 + 转向（纯 CPU）。实现细节见头文件。

#include "GN_Locomotion.h"

#include <cmath>

namespace GN3d {

namespace {

const float kPi = 3.14159265358979f;
const float kTwoPi = 6.28318530717959f;
// 判定"这个方向能不能算出朝向"的长度下限（平方）。1 mm 级已经远小于任何有意义的移动。
const float kMinDirectionLengthSq = 1.0e-6f;
// 速度低于它时改用输入意图（与 imoScene 调过的手感一致）。
const float kVelocityBiasSpeed = 0.25f;

float horizontalSpeed(const GNg::sVEC3 &vector) {
    return std::sqrt(vector.x * vector.x + vector.z * vector.z);
}

bool finiteVec(const GNg::sVEC3 &v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

} // namespace

sLOCOMOTION_DESC::sLOCOMOTION_DESC()
    : aIdleSpeed(0.4f), aRunSpeed(4.2f), aWalkRef(3.0f), aRunRef(5.5f), aTurnRate(14.0f),
      aMinPlayRate(0.55f), aMaxPlayRate(1.8f) {}

bool sLOCOMOTION_DESC::IsValid() const {
    if(!std::isfinite(aIdleSpeed) || !std::isfinite(aRunSpeed)) return false;
    if(!std::isfinite(aWalkRef) || !std::isfinite(aRunRef)) return false;
    if(!std::isfinite(aTurnRate) || !std::isfinite(aMinPlayRate) || !std::isfinite(aMaxPlayRate))
        return false;
    if(aIdleSpeed < 0.0f || aRunSpeed <= aIdleSpeed) return false;
    if(!(aWalkRef > 0.0f) || !(aRunRef > 0.0f)) return false;
    if(!(aMinPlayRate > 0.0f) || aMaxPlayRate < aMinPlayRate) return false;
    return true;
}

eLOCOMOTION_CLIP selectLocomotionClip(float speed, const sLOCOMOTION_DESC &desc) {
    if(!std::isfinite(speed) || speed <= desc.aIdleSpeed) return eLOCOMOTION_IDLE;
    if(speed <= desc.aRunSpeed) return eLOCOMOTION_WALK;
    return eLOCOMOTION_RUN;
}

float locomotionPlayRate(float speed, eLOCOMOTION_CLIP clip, const sLOCOMOTION_DESC &desc) {
    if(clip == eLOCOMOTION_IDLE) return 1.0f;
    if(!std::isfinite(speed) || speed <= 0.0f) return desc.aMinPlayRate;
    const float reference = (clip == eLOCOMOTION_RUN) ? desc.aRunRef : desc.aWalkRef;
    if(!(reference > 0.0f)) return 1.0f; // 参数没填好：退回原速而不是除零
    float rate = speed / reference;
    if(rate < desc.aMinPlayRate) rate = desc.aMinPlayRate;
    if(rate > desc.aMaxPlayRate) rate = desc.aMaxPlayRate;
    return rate;
}

const char *locomotionClipName(eLOCOMOTION_CLIP clip) {
    switch(clip) {
    case eLOCOMOTION_IDLE:
        return "idle";
    case eLOCOMOTION_WALK:
        return "walk";
    case eLOCOMOTION_RUN:
        return "run";
    default:
        return "?";
    }
}

float wrapRadians(float angle) {
    if(!std::isfinite(angle)) return 0.0f;
    // 结果是 [-pi, pi)（左闭右开）。
    // 用 fmod 而不是 while 循环：绕很多圈时 while 会变成 O(圈数)，
    // 而 fmod 是常数时间（这个函数每帧每角色都调，值得）。
    angle = std::fmod(angle + kPi, kTwoPi);
    if(angle < 0.0f) angle += kTwoPi;
    return angle - kPi;
}

bool yawFromDirection(float dirX, float dirZ, float *outYaw) {
    if(outYaw == 0 || !std::isfinite(dirX) || !std::isfinite(dirZ)) return false;
    if(dirX * dirX + dirZ * dirZ <= kMinDirectionLengthSq) return false;
    *outYaw = std::atan2(dirX, dirZ); // 模型正前方 = +Z
    return true;
}

float approachYaw(float currentYaw, float targetYaw, float turnRate, float dt) {
    if(!std::isfinite(currentYaw)) return std::isfinite(targetYaw) ? targetYaw : 0.0f;
    if(!std::isfinite(targetYaw)) return currentYaw;
    if(!(turnRate > 0.0f) || !(dt > 0.0f)) return currentYaw;
    const float delta = wrapRadians(targetYaw - currentYaw);
    // blend ∈ [0, 1)：指数衰减，永远靠近但不会越过目标。
    const float blend = 1.0f - std::exp(-turnRate * dt);
    return wrapRadians(currentYaw + delta * blend);
}

sFACING_STATE::sFACING_STATE() : aYaw(0.0f) {}

void updateFacing(sFACING_STATE &state, const GNg::sVEC3 &velocity, const GNg::sVEC3 &wish,
                  float dt, const sLOCOMOTION_DESC &desc) {
    if(!finiteVec(velocity) || !finiteVec(wish)) return;
    float targetYaw = state.aYaw;
    const float speed = horizontalSpeed(velocity);
    if(speed > kVelocityBiasSpeed && yawFromDirection(velocity.x, velocity.z, &targetYaw)) {
        // 走起来之后按实际移动方向转身（被挡住时会朝着"实际能走的方向"）
    } else if(yawFromDirection(wish.x, wish.z, &targetYaw)) {
        // 刚起步：用输入意图，保证一按键就转身
    } else {
        return; // 既没速度也没意图：保持当前朝向
    }
    state.aYaw = approachYaw(state.aYaw, targetYaw, desc.aTurnRate, dt);
}

} // namespace GN3d
