// GN_3DcameraFollow.cpp — 目标快照/跟随、临界阻尼平滑、自动对准与 Update(dt) 推进。
#include <GN.h>
#include "GN_3Dcamera.h"

#include <cmath>

namespace GN3d {

namespace {

const GNg::sVEC3 WORLD_UP = {0.0f, 1.0f, 0.0f};

float clampValue(float value, float low, float high) {
    if(value < low) return low;
    if(value > high) return high;
    return value;
}

bool finiteVec3(const GNg::sVEC3 &v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

// f_ratio → 平滑时间：clamp((1 - ratio) × 0.25, 0.02, 0.5) 秒（文档 §2.6）
float smoothTimeForRatio(float ratio) {
    return clampValue((1.0f - ratio) * 0.25f, 0.02f, 0.5f);
}

// Thomas Lowe 临界阻尼平滑（单个轴，无过冲）
float smoothDampAxis(float current, float target, float &velocity, float smoothTime, float dt) {
    const float omega = 2.0f / smoothTime;
    const float x = omega * dt;
    const float expFactor = 1.0f / (1.0f + x + 0.48f * x * x + 0.235f * x * x * x);
    const float change = current - target;
    const float temp = (velocity + omega * change) * dt;
    velocity = (velocity - omega * temp) * expFactor;
    return target + (change + temp) * expFactor;
}

} // namespace

void cCAMERA::SetTarget(const sCAMERA_TARGET &target, float followRatio, bool autoBalanced,
                        bool autoCorrectsRot) {
    if(std::isfinite(followRatio)) aFollowRatio = clampValue(followRatio, 0.0f, 1.0f);
    aFollowBalanced = autoBalanced;
    aFollowCorrectsRot = autoCorrectsRot;

    if(!target.aValid) {
        ClearTarget(); // aValid=false 等价于无目标
        return;
    }
    if(!finiteVec3(target.aPosition)) return; // 非法快照整帧忽略

    const bool continuing = aTarget.aValid;
    aTarget = target;

    if(aMode == eCAMERA_MODE_ORBIT) {
        // 轨道模式下目标只绑定焦点；继续沿用本帧平滑状态，避免每帧快照造成瞬移
        if(!continuing || !aSmoothValid) {
            aSmoothedTarget = aOrbitValid ? aOrbitFocus : aTarget.aPosition;
            aFollowVelocity = GNg::sVEC3{0.0f, 0.0f, 0.0f};
            aSmoothValid = true;
        }
        return;
    }

    if(aMode == eCAMERA_MODE_FOLLOW) {
        // 已跟随：保留 offset 与平滑状态，新位置由 Update 平滑追上
        if(!continuing || !aSmoothValid) {
            aFollowOffset = GNg::sVEC3{aPosition.x - aTarget.aPosition.x,
                                       aPosition.y - aTarget.aPosition.y,
                                       aPosition.z - aTarget.aPosition.z};
            aSmoothedTarget = aTarget.aPosition;
            aFollowVelocity = GNg::sVEC3{0.0f, 0.0f, 0.0f};
            aSmoothValid = true;
        }
        return;
    }

    // FREE：首次设置目标即进入跟随（GN 的 SetFollowOBJ / SetTarget 语义）
    aMode = eCAMERA_MODE_FOLLOW;
    aFollowOffset = GNg::sVEC3{aPosition.x - aTarget.aPosition.x,
                               aPosition.y - aTarget.aPosition.y,
                               aPosition.z - aTarget.aPosition.z};
    aSmoothedTarget = aTarget.aPosition;
    aFollowVelocity = GNg::sVEC3{0.0f, 0.0f, 0.0f};
    aSmoothValid = true;
}

void cCAMERA::ClearTarget() {
    aTarget = sCAMERA_TARGET();
    aFollowVelocity = GNg::sVEC3{0.0f, 0.0f, 0.0f};
    aSmoothValid = false;
    if(aMode == eCAMERA_MODE_FOLLOW) aMode = eCAMERA_MODE_FREE; // 取消目标回自由镜头
}

bool cCAMERA::HasTarget() const {
    return aTarget.aValid;
}

void cCAMERA::smoothTargetPosition(float dt) {
    const float smoothTime = smoothTimeForRatio(aFollowRatio);

    const float nx = smoothDampAxis(aSmoothedTarget.x, aTarget.aPosition.x, aFollowVelocity.x,
                                    smoothTime, dt);
    const float ny = smoothDampAxis(aSmoothedTarget.y, aTarget.aPosition.y, aFollowVelocity.y,
                                    smoothTime, dt);
    const float nz = smoothDampAxis(aSmoothedTarget.z, aTarget.aPosition.z, aFollowVelocity.z,
                                    smoothTime, dt);
    aSmoothedTarget = GNg::sVEC3{nx, ny, nz};

    const float speed = sqrtf(aFollowVelocity.x * aFollowVelocity.x +
                              aFollowVelocity.y * aFollowVelocity.y +
                              aFollowVelocity.z * aFollowVelocity.z);
    const bool nearPosition = fabsf(nx - aTarget.aPosition.x) < 1.0e-4f &&
                              fabsf(ny - aTarget.aPosition.y) < 1.0e-4f &&
                              fabsf(nz - aTarget.aPosition.z) < 1.0e-4f;
    if(nearPosition && speed < 1.0e-4f) {
        aSmoothedTarget = aTarget.aPosition; // 收敛后贴死，防尾抖
        aFollowVelocity = GNg::sVEC3{0.0f, 0.0f, 0.0f};
    }
}

void cCAMERA::Update(float dt) {
    if(!(dt > 0.0f) || !std::isfinite(dt)) return;
    if(dt > 0.1f) dt = 0.1f; // dt 尖峰钳制

    GNg::QuatNormalize(&aRotation, &aRotation);

    if(aMode == eCAMERA_MODE_ORBIT) {
        if(aOrbitValid && aTarget.aValid && aSmoothValid && finiteVec3(aTarget.aPosition)) {
            smoothTargetPosition(dt);
            aOrbitFocus = aSmoothedTarget;
            applyOrbit();
        }
        return;
    }

    if(aMode != eCAMERA_MODE_FOLLOW || !aTarget.aValid || !aSmoothValid) return;
    if(!finiteVec3(aTarget.aPosition)) return;

    smoothTargetPosition(dt);
    aPosition = GNg::sVEC3{aSmoothedTarget.x + aFollowOffset.x,
                           aSmoothedTarget.y + aFollowOffset.y,
                           aSmoothedTarget.z + aFollowOffset.z};

    // 朝向：看向平滑后的目标；balanced 用世界 +Y 重建（无 roll）
    GNg::sVEC3 dir;
    GNg::Vec3Sub(&dir, &aSmoothedTarget, &aPosition);
    if(GNg::Vec3Length(&dir) > 1.0e-5f) {
        const GNg::sVEC3 upHint = aFollowBalanced ? WORLD_UP : Up();
        const GNg::sQUAT desired = makeLookRotation(dir, upHint);
        if(aFollowCorrectsRot) {
            aRotation = desired; // 每帧直接重建
        } else {
            const float t = 1.0f - expf(-dt / smoothTimeForRatio(aFollowRatio));
            GNg::sQUAT blended;
            GNg::QuatSlerp(&blended, &aRotation, &desired, t);
            GNg::QuatNormalize(&blended, &blended);
            aRotation = blended;
        }
    }
    GNg::QuatNormalize(&aRotation, &aRotation);
}

} // namespace GN3d
