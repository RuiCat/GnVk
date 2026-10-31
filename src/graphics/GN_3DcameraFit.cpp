// GN_3DcameraFit.cpp — 八方位观看、按范围/对象自动取景，以及 FOV 参考方向换算。
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

// 方位 → (从相机指向中心的单位方向, 相机 up 参考)
void lookAxisOf(eCAMERA_LOOK_MODE mode, GNg::sVEC3 *dir, GNg::sVEC3 *up) {
    *dir = GNg::sVEC3{0.0f, 0.0f, -1.0f};
    *up = GNg::sVEC3{0.0f, 1.0f, 0.0f};
    switch(mode) {
    case eCAMERA_LOOK_FRONT: // 相机在 +Z 侧
        break;
    case eCAMERA_LOOK_BACK:
        *dir = GNg::sVEC3{0.0f, 0.0f, 1.0f};
        break;
    case eCAMERA_LOOK_LEFT: // 相机在 -X 侧，朝 +X 看
        *dir = GNg::sVEC3{1.0f, 0.0f, 0.0f};
        break;
    case eCAMERA_LOOK_RIGHT:
        *dir = GNg::sVEC3{-1.0f, 0.0f, 0.0f};
        break;
    case eCAMERA_LOOK_TOP: // 俯视，屏幕上方为 -Z
        *dir = GNg::sVEC3{0.0f, -1.0f, 0.0f};
        *up = GNg::sVEC3{0.0f, 0.0f, -1.0f};
        break;
    case eCAMERA_LOOK_DOWN: // 俯视，屏幕上方为 +Z
        *dir = GNg::sVEC3{0.0f, -1.0f, 0.0f};
        *up = GNg::sVEC3{0.0f, 0.0f, 1.0f};
        break;
    case eCAMERA_LOOK_BOTTOM:
        *dir = GNg::sVEC3{0.0f, 1.0f, 0.0f};
        *up = GNg::sVEC3{0.0f, 0.0f, 1.0f};
        break;
    case eCAMERA_LOOK_SWAP: // 正视但上下颠倒
        *up = GNg::sVEC3{0.0f, -1.0f, 0.0f};
        break;
    default:
        break;
    }
}

} // namespace

void SetCameraLookDirection(cCAMERA &camera, eCAMERA_LOOK_MODE mode, const GNg::sVEC3 *focus) {
    GNg::sVEC3 dir, up;
    lookAxisOf(mode, &dir, &up);

    GNg::sVEC3 center = {0.0f, 0.0f, 0.0f};
    if(focus) {
        if(!finiteVec3(*focus)) return;
        center = *focus;
    }

    camera.detachFollowAndOrbit();
    camera.aPosition = center; // GN 语义：放到原点（此处为焦点）并按方位看
    camera.aRotation = cCAMERA::makeLookRotation(dir, up);
    GNg::QuatNormalize(&camera.aRotation, &camera.aRotation);
}

float SetCameraLookRange(cCAMERA &camera, const GNg::sVEC3 &center, const GNg::sVEC3 &halfExtent,
                         float margin, eCAMERA_LOOK_MODE mode) {
    if(!finiteVec3(center) || !finiteVec3(halfExtent)) return 0.0f;
    if(!std::isfinite(margin) || !(margin > 0.0f)) return 0.0f;

    GNg::sVEC3 dir, up;
    lookAxisOf(mode, &dir, &up);

    const float tanHalfY = tanf(camera.aFovY * 0.5f * CAMERA_DEG_TO_RAD);
    if(!(tanHalfY > 1.0e-6f)) return 0.0f;

    // GN 签名没有 aspect：v1 按方形画幅（横向半角同纵向）估算，见文档 §4.5
    const float hx = fabsf(halfExtent.x);
    const float hy = fabsf(halfExtent.y);
    const float halfDepth = fabsf(halfExtent.x * dir.x + halfExtent.y * dir.y +
                                  halfExtent.z * dir.z);
    const float required = (hx > hy ? hx : hy) / tanHalfY * margin + halfDepth;
    const float distance = required > 0.0f ? required : 1.0f; // 空盒按点处理：d = 1

    const GNg::sVEC3 position = {center.x - dir.x * distance, center.y - dir.y * distance,
                                 center.z - dir.z * distance};

    const bool keepOrbit = camera.aOrbitValid && camera.aMode == eCAMERA_MODE_ORBIT;
    camera.detachFollowAndOrbit();
    if(keepOrbit) {
        const GNg::sVEC3 offset = {-dir.x * distance, -dir.y * distance, -dir.z * distance};
        const float yaw = atan2f(offset.x, offset.z);
        const float pitch = asinf(clampValue(offset.y / distance, -1.0f, 1.0f));
        camera.SetOrbit(center, distance, yaw, pitch);
        camera.aRotation = cCAMERA::makeLookRotation(dir, up); // 保留请求的 up（TOP/BOTTOM 等）
    } else {
        camera.aPosition = position;
        camera.aRotation = cCAMERA::makeLookRotation(dir, up);
        GNg::QuatNormalize(&camera.aRotation, &camera.aRotation);
    }

    if(camera.aOrthoH > 0.0f) camera.aOrthoH = 2.0f * distance * tanHalfY; // 正交同步截取高度
    return distance;
}

float SetCameraLookObj(cCAMERA &camera, const sCAMERA_TARGET &target, float margin,
                       eCAMERA_LOOK_MODE mode) {
    if(!target.aValid || !finiteVec3(target.aPosition)) return 0.0f;

    GNg::sVEC3 center = target.aPosition;
    GNg::sVEC3 halfExtent = {0.0f, 0.0f, 0.0f};
    if(target.aBounds.IsValid()) {
        center = target.aBounds.Center();
        halfExtent = target.aBounds.Extent();
    }
    return SetCameraLookRange(camera, center, halfExtent, margin, mode);
}

float cCAMERA::FovToOrthoH(float fovDeg, float targetDistance) {
    if(!std::isfinite(fovDeg) || !(fovDeg > 0.0f) || !(fovDeg < 180.0f)) return 0.0f;
    if(!std::isfinite(targetDistance) || targetDistance < 0.0f) return 0.0f;
    return 2.0f * targetDistance * tanf(fovDeg * 0.5f * CAMERA_DEG_TO_RAD);
}

float cCAMERA::FovConvert(eCAMERA_FOV_TYPE dst, eCAMERA_FOV_TYPE src, float fovDeg,
                          float whRatio) {
    if(!std::isfinite(fovDeg) || !(fovDeg > 0.0f) || !(fovDeg < 180.0f)) return 0.0f;
    if(!std::isfinite(whRatio) || !(whRatio > 0.0f)) return 0.0f;
    if(dst != eCAMERA_FOV_X && dst != eCAMERA_FOV_Y && dst != eCAMERA_FOV_XY) return 0.0f;

    const float tanHalf = tanf(fovDeg * 0.5f * CAMERA_DEG_TO_RAD);
    float tanY = 0.0f;
    float tanX = 0.0f;
    if(src == eCAMERA_FOV_Y) {
        tanY = tanHalf;
        tanX = tanHalf * whRatio;
    } else if(src == eCAMERA_FOV_X) {
        tanX = tanHalf;
        tanY = tanHalf / whRatio;
    } else if(src == eCAMERA_FOV_XY) {
        // 对角线视角：tanD² = tanX² + tanY²，且 tanX = tanY × aspect
        tanY = tanHalf / sqrtf(1.0f + whRatio * whRatio);
        tanX = tanY * whRatio;
    } else {
        return 0.0f;
    }

    float outTan = tanY;
    if(dst == eCAMERA_FOV_X) outTan = tanX;
    else if(dst == eCAMERA_FOV_XY) outTan = sqrtf(tanX * tanX + tanY * tanY);
    return 2.0f * atanf(outTan) * CAMERA_RAD_TO_DEG;
}

} // namespace GN3d
