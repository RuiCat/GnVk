// GN_3DcameraMove.cpp — GN 移动原语、世界绝对摆位与由朝向构造注视四元数。
#include <GN.h>
#include "GN_3Dcamera.h"

#include <cmath>

namespace GN3d {

namespace {

const GNg::sVEC3 WORLD_UP = {0.0f, 1.0f, 0.0f};
const GNg::sVEC3 LOCAL_FORWARD = {0.0f, 0.0f, -1.0f};

float clampValue(float value, float low, float high) {
    if(value < low) return low;
    if(value > high) return high;
    return value;
}

bool finiteVec3(const GNg::sVEC3 &v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

// 列主序旋转矩阵 → 四元数（三轴旋转矩阵的 Shepperd 分支）
GNg::sQUAT quatFromRotationMatrix(const GNg::sMAT44 &m) {
    const float m00 = m.m[0], m10 = m.m[1], m20 = m.m[2];
    const float m01 = m.m[4], m11 = m.m[5], m21 = m.m[6];
    const float m02 = m.m[8], m12 = m.m[9], m22 = m.m[10];
    GNg::sQUAT q;
    const float trace = m00 + m11 + m22;
    if(trace > 0.0f) {
        const float s = sqrtf(trace + 1.0f) * 2.0f;
        q.w = 0.25f * s;
        q.x = (m21 - m12) / s;
        q.y = (m02 - m20) / s;
        q.z = (m10 - m01) / s;
    } else if(m00 > m11 && m00 > m22) {
        const float s = sqrtf(1.0f + m00 - m11 - m22) * 2.0f;
        q.w = (m21 - m12) / s;
        q.x = 0.25f * s;
        q.y = (m01 + m10) / s;
        q.z = (m02 + m20) / s;
    } else if(m11 > m22) {
        const float s = sqrtf(1.0f + m11 - m00 - m22) * 2.0f;
        q.w = (m02 - m20) / s;
        q.x = (m01 + m10) / s;
        q.y = 0.25f * s;
        q.z = (m12 + m21) / s;
    } else {
        const float s = sqrtf(1.0f + m22 - m00 - m11) * 2.0f;
        q.w = (m10 - m01) / s;
        q.x = (m02 + m20) / s;
        q.y = (m12 + m21) / s;
        q.z = 0.25f * s;
    }
    GNg::QuatNormalize(&q, &q);
    return q;
}

} // namespace

// 由「相机→目标」方向与 up 参考构造朝向：局部 -Z 朝 dir，局部 +Y 尽量贴 upHint。
GNg::sQUAT cCAMERA::makeLookRotation(const GNg::sVEC3 &dir, const GNg::sVEC3 &upHint) {
    GNg::sVEC3 f;
    GNg::Vec3Normalize(&f, &dir);

    GNg::sVEC3 up = upHint;
    GNg::Vec3Normalize(&up, &up);
    if(GNg::Vec3Length(&up) < 0.5f) up = WORLD_UP;

    GNg::sVEC3 right;
    GNg::Vec3Cross(&right, &f, &up);
    if(GNg::Vec3Length(&right) < 1.0e-4f) {
        // 视线与 upHint 平行：换一个参考轴避免退化
        GNg::sVEC3 alt = {0.0f, 0.0f, 1.0f};
        if(fabsf(GNg::Vec3Dot(&f, &alt)) > 0.9f) alt = GNg::sVEC3{0.0f, 1.0f, 0.0f};
        GNg::Vec3Cross(&right, &f, &alt);
    }
    GNg::Vec3Normalize(&right, &right);

    GNg::sVEC3 upPerp;
    GNg::Vec3Cross(&upPerp, &right, &f);

    // 列：right / up / -forward（相机的局部 X/Y/Z 在世界中的方向）
    const GNg::sMAT44 basis(right.x, right.y, right.z, 0.0f,
                            upPerp.x, upPerp.y, upPerp.z, 0.0f,
                            -f.x, -f.y, -f.z, 0.0f,
                            0.0f, 0.0f, 0.0f, 1.0f);
    return quatFromRotationMatrix(basis);
}

void cCAMERA::SelfMove(float front, float left, float up) {
    if(!std::isfinite(front) || !std::isfinite(left) || !std::isfinite(up)) return;
    if(front == 0.0f && left == 0.0f && up == 0.0f) return;

    const GNg::sVEC3 f = Forward();
    const GNg::sVEC3 r = Right();
    const GNg::sVEC3 u = Up();
    const GNg::sVEC3 delta = {f.x * front - r.x * left + u.x * up,
                              f.y * front - r.y * left + u.y * up,
                              f.z * front - r.z * left + u.z * up};

    if(aMode == eCAMERA_MODE_ORBIT) {
        if(!aOrbitValid) return;
        aOrbitFocus.x += delta.x;
        aOrbitFocus.y += delta.y;
        aOrbitFocus.z += delta.z;
        applyOrbit();
        return;
    }

    aPosition.x += delta.x;
    aPosition.y += delta.y;
    aPosition.z += delta.z;
    if(aMode == eCAMERA_MODE_FOLLOW && aSmoothValid) {
        // 跟随中调偏移：保持相对位置，下一帧 Update 重新拼回 position
        aFollowOffset = GNg::sVEC3{aPosition.x - aSmoothedTarget.x,
                                   aPosition.y - aSmoothedTarget.y,
                                   aPosition.z - aSmoothedTarget.z};
    }
}

void cCAMERA::SelfRotLR(float angleRad) {
    if(!std::isfinite(angleRad) || angleRad == 0.0f) return;
    if(aMode == eCAMERA_MODE_ORBIT) {
        OrbitRotate(-angleRad, 0.0f); // 轨道：绕目标右转 = yaw 减小
        return;
    }
    GNg::sQUAT delta;
    GNg::QuatFromAxisAngle(&delta, &WORLD_UP, -angleRad); // 右转取负角
    aRotation = delta * aRotation;
    GNg::QuatNormalize(&aRotation, &aRotation);
}

void cCAMERA::SelfRotTB(float angleRad) {
    if(!std::isfinite(angleRad) || angleRad == 0.0f) return;
    if(aMode == eCAMERA_MODE_ORBIT) {
        OrbitRotate(0.0f, -angleRad); // 轨道：抬头 = pitch 减小（相机下移看向目标）
        return;
    }
    const GNg::sVEC3 right = Right();
    GNg::sQUAT delta;
    GNg::QuatFromAxisAngle(&delta, &right, angleRad);
    GNg::sQUAT candidate = delta * aRotation;
    GNg::QuatNormalize(&candidate, &candidate);

    GNg::sVEC3 candidateForward;
    GNg::QuatRotateVec3(&candidateForward, &LOCAL_FORWARD, &candidate);
    const float pitch = asinf(clampValue(-candidateForward.y, -1.0f, 1.0f));
    if(fabsf(pitch) > CAMERA_MAX_PITCH + 1.0e-4f) return; // 越界丢弃该帧垂直分量
    aRotation = candidate;
}

void cCAMERA::SelfRotCW(float angleRad) {
    if(!std::isfinite(angleRad) || angleRad == 0.0f) return;
    if(aMode == eCAMERA_MODE_ORBIT) return; // 轨道朝向由 focus 推导，无 roll
    const GNg::sVEC3 forward = Forward();
    GNg::sQUAT delta;
    GNg::QuatFromAxisAngle(&delta, &forward, angleRad);
    aRotation = delta * aRotation;
    GNg::QuatNormalize(&aRotation, &aRotation);
}

void cCAMERA::SelfRotAxis(float angleRad, const GNg::sVEC3 *axis) {
    if(!axis || !std::isfinite(angleRad)) return;
    if(!finiteVec3(*axis)) return;
    if(GNg::Vec3Length(axis) < 1.0e-6f) return; // 零长度轴：不旋转
    if(aMode == eCAMERA_MODE_ORBIT) return; // 保持 look-at 不变量

    GNg::sVEC3 normalized = *axis;
    GNg::Vec3Normalize(&normalized, &normalized);
    GNg::sQUAT delta;
    GNg::QuatFromAxisAngle(&delta, &normalized, angleRad);
    aRotation = delta * aRotation;
    GNg::QuatNormalize(&aRotation, &aRotation);
}

void cCAMERA::ToMove(float x, float y, float z) {
    if(!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) return;

    if(aMode == eCAMERA_MODE_ORBIT) {
        if(!aOrbitValid) return;
        aOrbitFocus.x += x;
        aOrbitFocus.y += y;
        aOrbitFocus.z += z;
        applyOrbit();
        return;
    }

    aPosition.x += x;
    aPosition.y += y;
    aPosition.z += z;
    if(aMode == eCAMERA_MODE_FOLLOW && aSmoothValid) {
        aFollowOffset = GNg::sVEC3{aPosition.x - aSmoothedTarget.x,
                                   aPosition.y - aSmoothedTarget.y,
                                   aPosition.z - aSmoothedTarget.z};
    }
}

void cCAMERA::SetWorldPos(const GNg::sVEC3 *pos) {
    if(!pos || !finiteVec3(*pos)) return;

    if(aMode == eCAMERA_MODE_ORBIT) {
        aPosition = *pos;
        syncOrbitFromPosition(); // 轨道不变量：position 仍由 focus + 角度 + 距离推导
        return;
    }

    aPosition = *pos;
    if(aMode == eCAMERA_MODE_FOLLOW && aSmoothValid) {
        aFollowOffset = GNg::sVEC3{aPosition.x - aSmoothedTarget.x,
                                   aPosition.y - aSmoothedTarget.y,
                                   aPosition.z - aSmoothedTarget.z};
    }
}

void cCAMERA::GetWorldPos(GNg::sVEC3 *out) const {
    if(!out) return;
    *out = aPosition;
}

GNg::sVEC3 cCAMERA::GetCoordV() const {
    return aPosition;
}

} // namespace GN3d
