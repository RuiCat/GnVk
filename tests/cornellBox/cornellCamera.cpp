// tests/cornellBox/cornellCamera.cpp — Cornell Box 自由相机
//
// 职责：持有一个 cCAMERA，把键鼠输入汇总为 sCAMERA_INPUT（WASD/QE + 鼠标/方向键），
//       提供 4 个可复现的 canonical 位姿与自由/轨道切换。不涉及场景绘制与 GI。
#include "cornellShared.h"

#include <cmath>

namespace Cornell {

namespace {

const float MOUSE_LOOK_SPEED = 0.004f; // 弧度/像素
const float KEY_LOOK_SPEED = 1.4f;     // 弧度/秒
const float MOVE_SPEED = 2.5f;         // 米/秒
const float DEFAULT_FOV = 45.0f;

int gLastMouseX = 0;
int gLastMouseY = 0;
bool gLastMouseValid = false;

float clampValue(float value, float low, float high) {
    if (value < low) return low;
    return value > high ? high : value;
}

// 自由 → 轨道：以当前视线前方 3 m 为焦点，反算 yaw/pitch，切换瞬间不跳变。
void enterOrbitFromFree() {
    const GNg::sVEC3 position = gCamera.GetCoordV();
    const GNg::sVEC3 forward = gCamera.Forward();
    const float distance = 3.0f;
    const GNg::sVEC3 focus = {position.x + forward.x * distance,
                              position.y + forward.y * distance,
                              position.z + forward.z * distance};
    const GNg::sVEC3 offset = {position.x - focus.x, position.y - focus.y,
                               position.z - focus.z};
    const float yaw = std::atan2(offset.x, offset.z);
    const float pitch = std::asin(clampValue(offset.y / distance, -1.0f, 1.0f));
    gCamera.SetOrbit(focus, distance, yaw, pitch);
}

} // namespace

GN3d::cCAMERA gCamera;

void initCamera() {
    gCamera.Init(nullptr);
    gCamera.SetFov(DEFAULT_FOV);
    gCamera.SetNF(0.02f, 100.0f);
    resetCamera(0);
}

void resetCamera(int poseIndex) {
    const int index = (poseIndex >= 0 && poseIndex < 4) ? poseIndex : 0;
    const sPose &pose = CORNELL_POSES[index];
    const GNg::sVEC3 position = {pose.aPosition.x, pose.aPosition.y, pose.aPosition.z};
    const GNg::sVEC3 focus = {pose.aFocus.x, pose.aFocus.y, pose.aFocus.z};

    const GNg::sVEC3 offset = {position.x - focus.x, position.y - focus.y,
                               position.z - focus.z};
    const float distance = std::sqrt(offset.x * offset.x + offset.y * offset.y +
                                     offset.z * offset.z);
    const float yaw = std::atan2(offset.x, offset.z);
    const float pitch = std::asin(clampValue(offset.y / distance, -1.0f, 1.0f));
    gCamera.SetOrbit(focus, distance, yaw, pitch);
    gCamera.aMode = GN3d::eCAMERA_MODE_FREE; // 位姿就位后回到自由飞行
    gLastMouseValid = false;
}

void updateCamera(float dt) {
    if (GN::Key1[GK_F]) {
        if (gCamera.aMode == GN3d::eCAMERA_MODE_ORBIT) {
            gCamera.aMode = GN3d::eCAMERA_MODE_FREE;
        } else {
            enterOrbitFromFree();
        }
    }

    GN3d::sCAMERA_INPUT input;
    if (GN::Key[GK_W]) input.aMoveFront += 1.0f;
    if (GN::Key[GK_S]) input.aMoveFront -= 1.0f;
    if (GN::Key[GK_A]) input.aMoveLeft += 1.0f;
    if (GN::Key[GK_D]) input.aMoveLeft -= 1.0f;
    if (GN::Key[GK_Q]) input.aMoveUp -= 1.0f;
    if (GN::Key[GK_E]) input.aMoveUp += 1.0f;
    input.aBoost = (GN::Key[GK_SHIFT] != 0);

    const float lookStep = KEY_LOOK_SPEED * dt;
    if (GN::Key[GK_LEFT] || GN::Key[GK_J]) input.aLookYaw -= lookStep;
    if (GN::Key[GK_RIGHT] || GN::Key[GK_L]) input.aLookYaw += lookStep;
    if (GN::Key[GK_UP]) input.aLookPitch += lookStep;
    if (GN::Key[GK_DOWN]) input.aLookPitch -= lookStep;

    if (GNg::ViewCur) {
        const int mx = GNg::ViewCur->mouseMX;
        const int my = GNg::ViewCur->mouseMY;
        const bool dragging = (GN::Mouse[GM_LEFT] != 0) || (GN::Mouse[GM_RIGHT] != 0);
        if (!dragging) {
            gLastMouseValid = false;
            gLastMouseX = mx;
            gLastMouseY = my;
        } else if (!gLastMouseValid) {
            gLastMouseValid = true;
            gLastMouseX = mx;
            gLastMouseY = my;
        } else {
            const int dx = mx - gLastMouseX;
            const int dy = my - gLastMouseY;
            gLastMouseX = mx;
            gLastMouseY = my;
            input.aLookYaw += static_cast<float>(dx) * MOUSE_LOOK_SPEED;
            input.aLookPitch += -static_cast<float>(dy) * MOUSE_LOOK_SPEED;
        }
    }

    if (gCamera.aMode == GN3d::eCAMERA_MODE_ORBIT) {
        input.aLookYaw = -input.aLookYaw;
        input.aLookPitch = -input.aLookPitch;
    }
    gCamera.ApplyInput(input, dt, MOVE_SPEED, 1.0f);
    gCamera.Update(dt);
}

} // namespace Cornell
