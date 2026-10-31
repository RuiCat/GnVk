// GN_3DcameraControl.cpp — sCAMERA_INPUT → 移动/转视角（自由飞行与轨道的输入适配，不读全局输入）。
#include <GN.h>
#include "GN_3Dcamera.h"

#include <cmath>

namespace GN3d {

namespace {

bool finiteInput(const sCAMERA_INPUT &input) {
    return std::isfinite(input.aMoveFront) && std::isfinite(input.aMoveLeft) &&
           std::isfinite(input.aMoveUp) && std::isfinite(input.aLookYaw) &&
           std::isfinite(input.aLookPitch) && std::isfinite(input.aZoom) &&
           std::isfinite(input.aPanRight) && std::isfinite(input.aPanUp);
}

} // namespace

void cCAMERA::ApplyInput(const sCAMERA_INPUT &input, float dt, float moveSpeed, float lookSpeed) {
    if(!(dt > 0.0f) || !std::isfinite(dt)) return;
    if(dt > 0.1f) dt = 0.1f;
    if(!finiteInput(input)) return; // 含 NaN/Inf 整帧忽略
    if(!std::isfinite(moveSpeed) || !std::isfinite(lookSpeed)) return;

    if(aMode == eCAMERA_MODE_ORBIT) {
        OrbitRotate(input.aLookYaw * lookSpeed, input.aLookPitch * lookSpeed);
        OrbitZoom(input.aZoom);
        if(input.aPanRight != 0.0f || input.aPanUp != 0.0f)
            OrbitPan(input.aPanRight, input.aPanUp);
        return;
    }

    const float boost = input.aBoost ? 3.0f : 1.0f;
    const float step = moveSpeed * dt * boost; // 位移 = 速度 × dt（Shift ×3）
    if(input.aMoveFront != 0.0f || input.aMoveLeft != 0.0f || input.aMoveUp != 0.0f) {
        SelfMove(input.aMoveFront * step, input.aMoveLeft * step, input.aMoveUp * step);
    }
    if(input.aLookYaw != 0.0f) SelfRotLR(input.aLookYaw * lookSpeed);
    if(input.aLookPitch != 0.0f) SelfRotTB(input.aLookPitch * lookSpeed);
    // 非轨道模式下滚轮不改镜头参数：near/far/fov 仍由调用方显式控制
}

} // namespace GN3d
