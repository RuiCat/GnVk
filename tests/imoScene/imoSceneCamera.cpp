// tests/imoScene/imoSceneCamera.cpp — 轨道相机：状态、鼠标/键盘输入与 view-projection。
//
// 职责：唯一的 cCAMERA 实例、默认取景参数与每帧输入映射（不涉及模型与绘制）。
// 视角约定：yaw=0 时相机在焦点 +Z 侧看向 -Z（角色正面 +Z，与 glTF 资产正面一致）。

#include "imoSceneCamera.h"

#include <cmath>

namespace imoScene {

namespace {

// 默认取景：距离 = 模型高度 × 该系数，俯角略高于模型中心。
const float DEFAULT_DISTANCE_FACTOR = 2.2f;
const float DEFAULT_PITCH = 0.10f;
// 鼠标旋转灵敏度（弧度/像素）与键盘旋转速度（弧度/秒）。
const float MOUSE_ROTATE_SPEED = 0.005f;
const float KEY_ROTATE_SPEED = 1.2f;
// 滚轮每一格的缩放增量（OrbitZoom 按 exp(-delta·0.15) 缩放距离）。
const float WHEEL_ZOOM_STEP = 0.6f;
// 中键平移：每像素位移 = 距离 × 该系数。
const float PAN_SPEED = 0.0012f;
// 默认镜头：45° 垂直视场，近/远平面覆盖 0.05–300 m。
const float DEFAULT_FOV = 45.0f;
const float DEFAULT_NEAR = 0.05f;
const float DEFAULT_FAR = 300.0f;

GN3d::cCAMERA gCamera;
GNg::sVEC3 gInitialFocus;
float gInitialHeight = 2.0f;
// 拖拽锚点：每帧用光标绝对位置求增量，松开/首次按下时重新锚定（避免陈旧增量）。
int gLastMouseX = 0;
int gLastMouseY = 0;
bool gLastMouseValid = false;
int gLastPanX = 0;
int gLastPanY = 0;
bool gLastPanValid = false;

} // namespace

void cameraInit(const GNg::sVEC3 &focus, float height) {
    gInitialFocus = focus;
    gInitialHeight = (height > 0.5f) ? height : 0.5f;
    gCamera.Init();
    gCamera.SetFov(DEFAULT_FOV);
    gCamera.SetNF(DEFAULT_NEAR, DEFAULT_FAR);
    cameraReset();
}

void cameraReset() {
    gCamera.SetOrbit(gInitialFocus, gInitialHeight * DEFAULT_DISTANCE_FACTOR, 0.0f, DEFAULT_PITCH);
    gLastMouseValid = false;
    gLastPanValid = false;
}

void cameraUpdate(float deltaSeconds) {
    if (GN::Key1[GK_R]) {
        cameraReset();
    }
    // 方向键 / J L 键盘旋转（鼠标不可用时的兜底路径）。
    float keyYaw = 0.0f;
    float keyPitch = 0.0f;
    if (GN::Key[GK_LEFT] || GN::Key[GK_J]) keyYaw -= KEY_ROTATE_SPEED * deltaSeconds;
    if (GN::Key[GK_RIGHT] || GN::Key[GK_L]) keyYaw += KEY_ROTATE_SPEED * deltaSeconds;
    if (GN::Key[GK_UP]) keyPitch += KEY_ROTATE_SPEED * deltaSeconds;
    if (GN::Key[GK_DOWN]) keyPitch -= KEY_ROTATE_SPEED * deltaSeconds;
    if (keyYaw != 0.0f || keyPitch != 0.0f) {
        gCamera.OrbitRotate(-keyYaw, keyPitch);
    }
    // 滚轮：上滚拉近、下滚拉远。
    if (GN::Mouse1[GM_TOP]) {
        gCamera.OrbitZoom(WHEEL_ZOOM_STEP);
    }
    if (GN::Mouse1[GM_BOTTOM]) {
        gCamera.OrbitZoom(-WHEEL_ZOOM_STEP);
    }
    if (!GNg::ViewCur) {
        return;
    }
    const int mouseX = GNg::ViewCur->mouseMX;
    const int mouseY = GNg::ViewCur->mouseMY;

    // 左/右键拖动：绕焦点旋转（向右拖 = 视角右转 = 相机左移）。
    const bool rotating = (GN::Mouse[GM_LEFT] != 0) || (GN::Mouse[GM_RIGHT] != 0);
    if (!rotating) {
        gLastMouseValid = false;
        gLastMouseX = mouseX;
        gLastMouseY = mouseY;
    } else if (!gLastMouseValid) {
        gLastMouseValid = true;
        gLastMouseX = mouseX;
        gLastMouseY = mouseY;
    } else {
        const int dx = mouseX - gLastMouseX;
        const int dy = mouseY - gLastMouseY;
        gLastMouseX = mouseX;
        gLastMouseY = mouseY;
        gCamera.OrbitRotate(-static_cast<float>(dx) * MOUSE_ROTATE_SPEED,
                            static_cast<float>(dy) * MOUSE_ROTATE_SPEED);
    }

    // 中键拖动：沿相机右/上轴平移焦点。
    const bool panning = (GN::Mouse[GM_CENTRE] != 0);
    if (!panning) {
        gLastPanValid = false;
        gLastPanX = mouseX;
        gLastPanY = mouseY;
    } else if (!gLastPanValid) {
        gLastPanValid = true;
        gLastPanX = mouseX;
        gLastPanY = mouseY;
    } else {
        const int dx = mouseX - gLastPanX;
        const int dy = mouseY - gLastPanY;
        gLastPanX = mouseX;
        gLastPanY = mouseY;
        const float scale = gInitialHeight * DEFAULT_DISTANCE_FACTOR * PAN_SPEED;
        gCamera.OrbitPan(-static_cast<float>(dx) * scale, static_cast<float>(dy) * scale);
    }
}

bool cameraBuildViewProj(float aspect, GNg::sMAT44 &viewProjection) {
    if (!(aspect > 0.0f) || !std::isfinite(aspect)) {
        return false;
    }
    GNg::sMAT44 view;
    GNg::sMAT44 projection;
    return gCamera.BuildViewProj(aspect, &view, &projection, &viewProjection);
}

GNg::sVEC3 cameraPosition() {
    return gCamera.GetCoordV();
}

} // namespace imoScene
