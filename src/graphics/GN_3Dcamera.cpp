// GN_3Dcamera.cpp — 相机生命周期、镜头参数校验、状态读写、基向量与视图/投影构建。
#include <GN.h>
#include "GN_3Dcamera.h"

#include <cmath>

namespace GN3d {

namespace {

float clampValue(float value, float low, float high) {
    if(value < low) return low;
    if(value > high) return high;
    return value;
}

bool finiteVec3(const GNg::sVEC3 &v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

bool validMode(eCAMERA_MODE mode) {
    return mode == eCAMERA_MODE_FREE || mode == eCAMERA_MODE_ORBIT || mode == eCAMERA_MODE_FOLLOW;
}

} // namespace

sCAMERA_TARGET::sCAMERA_TARGET() : aValid(false) {
    aPosition.x = 0.0f;
    aPosition.y = 0.0f;
    aPosition.z = 0.0f;
}

sCAMERA_DESC::sCAMERA_DESC()
    : aFovYDeg(45.0f), aNear(0.1f), aFar(1000.0f), aOrthoH(0.0f), aMode(eCAMERA_MODE_FREE),
      aFollowRatio(0.15f) {
    aPosition.x = 0.0f;
    aPosition.y = 0.0f;
    aPosition.z = 0.0f;
    aRotation = GNg::sQUAT(0.0f, 0.0f, 0.0f, 1.0f);
}

sCAMERA_INPUT::sCAMERA_INPUT()
    : aMoveFront(0.0f), aMoveLeft(0.0f), aMoveUp(0.0f), aLookYaw(0.0f), aLookPitch(0.0f),
      aZoom(0.0f), aPanRight(0.0f), aPanUp(0.0f), aBoost(false) {}

cCAMERA::cCAMERA() {
    Reset();
}

void cCAMERA::Reset() {
    aPosition = GNg::sVEC3{0.0f, 0.0f, 0.0f};
    aRotation = GNg::sQUAT(0.0f, 0.0f, 0.0f, 1.0f);
    aFovY = 45.0f;
    aNear = 0.1f;
    aFar = 1000.0f;
    aOrthoH = 0.0f;
    aMode = eCAMERA_MODE_FREE;

    aOrbitValid = false;
    aOrbitFocus = GNg::sVEC3{0.0f, 0.0f, 0.0f};
    aOrbitDistance = 1.0f;
    aOrbitYaw = 0.0f;
    aOrbitPitch = 0.0f;

    aTarget = sCAMERA_TARGET();
    aFollowOffset = GNg::sVEC3{0.0f, 0.0f, 0.0f};
    aFollowVelocity = GNg::sVEC3{0.0f, 0.0f, 0.0f};
    aSmoothedTarget = GNg::sVEC3{0.0f, 0.0f, 0.0f};
    aFollowRatio = 0.15f;
    aFollowBalanced = true;
    aFollowCorrectsRot = false;
    aSmoothValid = false;
}

void cCAMERA::Init(const sCAMERA_DESC *desc) {
    Reset();
    if(!desc) return;

    if(finiteVec3(desc->aPosition)) aPosition = desc->aPosition;
    aRotation = desc->aRotation;
    GNg::QuatNormalize(&aRotation, &aRotation);

    SetFov(desc->aFovYDeg); // 非法保留默认 45 并退出正交
    SetNF(desc->aNear, desc->aFar); // 非法整对保留默认
    if(std::isfinite(desc->aOrthoH) && desc->aOrthoH > 0.0f) aOrthoH = desc->aOrthoH;
    if(validMode(desc->aMode)) aMode = desc->aMode;
    if(std::isfinite(desc->aFollowRatio))
        aFollowRatio = clampValue(desc->aFollowRatio, 0.0f, 1.0f);
}

bool cCAMERA::SetFov(float deg) {
    if(!std::isfinite(deg) || !(deg > 0.0f) || !(deg < 180.0f)) return false;
    aFovY = deg;
    aOrthoH = 0.0f; // GN 语义：设置 FOV 即回透视
    return true;
}

bool cCAMERA::SetOrthoH(float h) {
    if(!std::isfinite(h) || h < 0.0f) return false;
    aOrthoH = h; // 0 = 回透视
    return true;
}

bool cCAMERA::SetNear(float n) {
    if(!std::isfinite(n) || !(n > 0.0f) || !(n < aFar)) return false;
    aNear = n;
    return true;
}

bool cCAMERA::SetFar(float f) {
    if(!std::isfinite(f) || !(f > 0.0f) || !(f > aNear)) return false;
    aFar = f;
    return true;
}

bool cCAMERA::SetNF(float n, float f) {
    if(!std::isfinite(n) || !std::isfinite(f) || !(n > 0.0f) || !(f > n)) return false;
    aNear = n;
    aFar = f;
    return true;
}

bool cCAMERA::IsOrthoMode() const {
    return aOrthoH > 0.0f;
}

GNg::sVEC3 cCAMERA::Forward() const {
    const GNg::sVEC3 local = {0.0f, 0.0f, -1.0f};
    GNg::sVEC3 out;
    GNg::QuatRotateVec3(&out, &local, &aRotation);
    return out;
}

GNg::sVEC3 cCAMERA::Right() const {
    const GNg::sVEC3 local = {1.0f, 0.0f, 0.0f};
    GNg::sVEC3 out;
    GNg::QuatRotateVec3(&out, &local, &aRotation);
    return out;
}

GNg::sVEC3 cCAMERA::Up() const {
    const GNg::sVEC3 local = {0.0f, 1.0f, 0.0f};
    GNg::sVEC3 out;
    GNg::QuatRotateVec3(&out, &local, &aRotation);
    return out;
}

bool cCAMERA::BuildViewProj(float aspect, GNg::sMAT44 *viewOut, GNg::sMAT44 *projOut,
                            GNg::sMAT44 *viewProjOut) const {
    if(!std::isfinite(aspect) || !(aspect > 0.0f)) return false;
    if(!(aNear > 0.0f) || !(aFar > aNear)) return false;

    const GNg::sVEC3 forward = Forward();
    GNg::sVEC3 up = Up();
    // 退化保护：视线与相机 up 平行时 LookAt 的叉积会塌成零向量
    const float alignment = forward.x * up.x + forward.y * up.y + forward.z * up.z;
    if(alignment > 0.9999f || alignment < -0.9999f) {
        up = GNg::sVEC3{0.0f, 1.0f, 0.0f};
        if(forward.y > 0.9999f || forward.y < -0.9999f) up = GNg::sVEC3{0.0f, 0.0f, 1.0f};
    }

    const GNg::sVEC3 eye = aPosition;
    const GNg::sVEC3 center = {eye.x + forward.x, eye.y + forward.y, eye.z + forward.z};
    GNg::sMAT44 view;
    GNg::Mat44LookAtRH(&view, &eye, &center, &up);

    GNg::sMAT44 proj;
    if(aOrthoH > 0.0f) {
        const float halfH = aOrthoH * 0.5f;
        const float halfW = halfH * aspect;
        GNg::Mat44OrthoVK(&proj, -halfW, halfW, -halfH, halfH, aNear, aFar);
    } else {
        GNg::Mat44PerspectiveVK(&proj, aFovY * CAMERA_DEG_TO_RAD, aspect, aNear, aFar);
    }

    if(viewOut) *viewOut = view;
    if(projOut) *projOut = proj;
    if(viewProjOut) GNg::Mat44Mul(viewProjOut, &proj, &view);
    return true;
}

} // namespace GN3d
