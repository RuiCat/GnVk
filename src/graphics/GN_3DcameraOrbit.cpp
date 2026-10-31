// GN_3DcameraOrbit.cpp — 轨道状态机：由 focus/角度/距离重解位姿，旋转/缩放/平移与手动摆位反算。
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

} // namespace

void cCAMERA::detachFollowAndOrbit() {
    aMode = eCAMERA_MODE_FREE;
    aOrbitValid = false;
    aTarget = sCAMERA_TARGET();
    aSmoothValid = false;
    aFollowVelocity = GNg::sVEC3{0.0f, 0.0f, 0.0f};
}

void cCAMERA::applyOrbit() {
    if(!aOrbitValid) return;

    const float cosPitch = cosf(aOrbitPitch);
    GNg::sVEC3 offset = {sinf(aOrbitYaw) * cosPitch, sinf(aOrbitPitch),
                         cosf(aOrbitYaw) * cosPitch};
    aPosition.x = aOrbitFocus.x + offset.x * aOrbitDistance;
    aPosition.y = aOrbitFocus.y + offset.y * aOrbitDistance;
    aPosition.z = aOrbitFocus.z + offset.z * aOrbitDistance;

    GNg::sVEC3 dir;
    GNg::Vec3Sub(&dir, &aOrbitFocus, &aPosition);
    if(GNg::Vec3Length(&dir) > 1.0e-5f) {
        aRotation = makeLookRotation(dir, WORLD_UP); // 无 roll
    }
}

void cCAMERA::syncOrbitFromPosition() {
    if(!aOrbitValid) return;

    GNg::sVEC3 offset;
    GNg::Vec3Sub(&offset, &aPosition, &aOrbitFocus);
    const float distance = GNg::Vec3Length(&offset);
    aOrbitDistance = clampValue(distance, CAMERA_MIN_DISTANCE, CAMERA_MAX_DISTANCE);
    if(distance > 1.0e-5f) {
        aOrbitPitch = clampValue(asinf(clampValue(offset.y / distance, -1.0f, 1.0f)),
                                 -CAMERA_MAX_PITCH, CAMERA_MAX_PITCH);
        aOrbitYaw = atan2f(offset.x, offset.z);
    }
    // focus == position 时反算退化：保留上一帧角度，不产生 NaN
    applyOrbit();
}

void cCAMERA::SetOrbit(const GNg::sVEC3 &focus, float distance, float yawRad, float pitchRad) {
    if(!finiteVec3(focus) || !std::isfinite(distance) || !std::isfinite(yawRad) ||
       !std::isfinite(pitchRad))
        return;

    aMode = eCAMERA_MODE_ORBIT;
    aOrbitValid = true;
    aOrbitFocus = focus;
    aOrbitDistance = clampValue(distance, CAMERA_MIN_DISTANCE, CAMERA_MAX_DISTANCE);
    aOrbitYaw = yawRad;
    aOrbitPitch = clampValue(pitchRad, -CAMERA_MAX_PITCH, CAMERA_MAX_PITCH);
    applyOrbit();
}

void cCAMERA::OrbitRotate(float yawDelta, float pitchDelta) {
    if(!aOrbitValid || !std::isfinite(yawDelta) || !std::isfinite(pitchDelta)) return;
    aOrbitYaw += yawDelta;
    aOrbitPitch = clampValue(aOrbitPitch + pitchDelta, -CAMERA_MAX_PITCH, CAMERA_MAX_PITCH);
    applyOrbit();
}

void cCAMERA::OrbitZoom(float delta) {
    if(!aOrbitValid || !std::isfinite(delta)) return;
    float distance = aOrbitDistance * expf(-delta * 0.15f);
    if(!std::isfinite(distance)) distance = (delta < 0.0f) ? CAMERA_MAX_DISTANCE : CAMERA_MIN_DISTANCE;
    aOrbitDistance = clampValue(distance, CAMERA_MIN_DISTANCE, CAMERA_MAX_DISTANCE);
    applyOrbit();
}

void cCAMERA::OrbitPan(float right, float up) {
    if(!aOrbitValid || !std::isfinite(right) || !std::isfinite(up)) return;
    if(right == 0.0f && up == 0.0f) return;

    const GNg::sVEC3 rightAxis = Right();
    const GNg::sVEC3 upAxis = Up();
    aOrbitFocus.x += rightAxis.x * right + upAxis.x * up;
    aOrbitFocus.y += rightAxis.y * right + upAxis.y * up;
    aOrbitFocus.z += rightAxis.z * right + upAxis.z * up;
    applyOrbit();
}

} // namespace GN3d
