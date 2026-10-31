// tests/mmdViewer/mmdViewerCamera.cpp — MMD 查看器：相机与视角控制（C5：接线到 GN3d::cCAMERA）
//
// 职责：持有唯一的相机实例，默认正面视角重置，鼠标/键盘输入汇总为 sCAMERA_INPUT 后
//       交给 cCAMERA::ApplyInput，并提供相机基向量 / 注视点 / 距离的推导。
// 模式：F 键自由飞行 ↔ 轨道；轨道下滚轮缩放、中键拖动平移；R 重置默认视角（自由模式）。
// 不涉及场景绘制、天气与叠层（那些在各自模块里）。

#include "mmdViewerShared.h"

#include <cmath>

namespace {

const float DEFAULT_VIEW_PITCH = 0.12f;     // 默认正面视角的俯角（相机高于模型中心）
const float MOUSE_LOOK_SPEED = 0.005f;      // 鼠标灵敏度（弧度/像素）
const float KEY_LOOK_SPEED = 1.6f;          // 方向键转视角速度（弧度/秒）
const float MOVE_SPEED_FACTOR = 0.9f;       // 平移速度 = max(1, 模型高) × 该系数（m/s）
const float DEFAULT_DISTANCE_FACTOR = 1.9f; // 默认观察距离 = 模型高度 × 该系数
const float ORBIT_ZOOM_STEP = 0.6f;         // 滚轮每一格的缩放增量
const float ORBIT_PAN_SPEED = 0.0015f;      // 中键平移：每像素 = max(1, 距离) × 该系数（米）

// 鼠标拖拽求增量用的上一帧光标位置。不依赖引擎的 MouseM 时序：
// 每帧自己用光标绝对位置做差，按键松开/首次按下时重新锚定，避免用陈旧增量。
int gLastMouseX = 0;
int gLastMouseY = 0;
bool gLastMouseValid = false;
// 中键平移单独一套锚点：与左/右键转视角可同时按住而不互相污染。
int gLastPanX = 0;
int gLastPanY = 0;
bool gLastPanValid = false;

// 自由 → 轨道：以当前视线方向取焦点（位置 + 前向 × 距离），反算 yaw/pitch 后交给 SetOrbit。
// 这样切换瞬间 position/rotation 连续（相机仍朝同一方向看同一个点）。
void enterOrbitFromFree() {
    float distance = cameraDistance();
    if (!(distance > 1.0e-3f)) {
        distance = (gHeight > 1.0f ? gHeight : 1.0f) * DEFAULT_DISTANCE_FACTOR;
    }
    const GNg::sVEC3 position = gCamera.GetCoordV();
    const GNg::sVEC3 forward = gCamera.Forward();
    const GNg::sVEC3 focus = {position.x + forward.x * distance,
                              position.y + forward.y * distance,
                              position.z + forward.z * distance};
    const GNg::sVEC3 offset = {position.x - focus.x, position.y - focus.y, position.z - focus.z};
    const float yaw = std::atan2(offset.x, offset.z);
    const float pitch = std::asin(clampValue(offset.y / distance, -1.0f, 1.0f));
    gCamera.SetOrbit(focus, distance, yaw, pitch);
}

} // namespace

// 全查看器唯一的相机实例（定义在此，声明见 mmdViewerShared.h）。
GN3d::cCAMERA gCamera;

float clampValue(float value, float low, float high) {
    if (value < low) {
        return low;
    }
    return value > high ? high : value;
}

// 相机位置（世界空间）。
GNg::sVEC3 cameraPosition() {
    return gCamera.GetCoordV();
}

// 相机前向 / 右向 / 上向：由相机持有的朝向四元数旋转局部基向量得到。
void cameraBasisVectors(GNg::sVEC3 &forward, GNg::sVEC3 &right, GNg::sVEC3 &up) {
    forward = gCamera.Forward();
    right = gCamera.Right();
    up = gCamera.Up();
}

// 相机注视点：沿前向 1 m 处（LookAt 的 center）。
GNg::sVEC3 cameraTarget() {
    const GNg::sVEC3 position = gCamera.GetCoordV();
    const GNg::sVEC3 forward = gCamera.Forward();
    const GNg::sVEC3 target = {position.x + forward.x, position.y + forward.y,
                               position.z + forward.z};
    return target;
}

// 相机到模型中心的距离：远平面、降水回收盒、溅射半径共用。
float cameraDistance() {
    const GNg::sVEC3 position = gCamera.GetCoordV();
    GNg::sVEC3 offset;
    GNg::Vec3Sub(&offset, &position, &gTarget);
    return std::sqrt(offset.x * offset.x + offset.y * offset.y + offset.z * offset.z);
}

// 相机基向量：forward 指向注视点，right/up 供降水 billboard 展开使用。
// 这里刻意按世界 +Y 重建（与旧实现一致）：billboard 不要 roll，轨道/自由模式都不引入 roll。
void cameraBasis(const GNg::sVEC3 &camera, GNg::sVEC3 &right, GNg::sVEC3 &camUp) {
    GNg::sVEC3 forward;
    const GNg::sVEC3 target = cameraTarget();
    GNg::Vec3Sub(&forward, &target, &camera);
    GNg::Vec3Normalize(&forward, &forward);
    const GNg::sVEC3 worldUp = {0.0f, 1.0f, 0.0f};
    GNg::Vec3Cross(&right, &forward, &worldUp);
    GNg::Vec3Normalize(&right, &right);
    GNg::Vec3Cross(&camUp, &right, &forward);
}

// 回到默认正面视角（统一坐标系 v2：资产正面 +Z）：相机放在模型 +Z 侧、略高于中心，
// yaw = 0 看向 -Z，俯角取正值（轨道 pitch 高 = 相机在上往下看）。
// 默认仍是自由飞行模式（WASD 可用），F 才进轨道；轨道参数已就位，F 切换沿用同一视角。
void resetCamera() {
    gCamera.SetOrbit(gTarget, gHeight * DEFAULT_DISTANCE_FACTOR, 0.0f, DEFAULT_VIEW_PITCH);
    gCamera.aMode = GN3d::eCAMERA_MODE_FREE;
    gLastMouseValid = false;
    gLastPanValid = false;
}

// 每帧相机输入：只汇总 sCAMERA_INPUT，具体运动全部由 cCAMERA::ApplyInput 执行。
// 顺序与拆分前一致：F 切换模式 → 平移/键盘转视角/鼠标拖拽 → 轨道缩放/平移。
void updateCameraInput(float dt) {
    // F 键：自由飞行 ↔ 轨道（与 1-6/G/H/O/P/C/V/R 不冲突）。
    if (GN::Key1[GK_F]) {
        if (gCamera.aMode == GN3d::eCAMERA_MODE_ORBIT) {
            gCamera.aMode = GN3d::eCAMERA_MODE_FREE; // 出轨道：保留当前 position/rotation
        } else {
            enterOrbitFromFree();
        }
    }
    const bool orbitMode = (gCamera.aMode == GN3d::eCAMERA_MODE_ORBIT);

    GN3d::sCAMERA_INPUT input;

    // 平移：W/S 前后、A/D 左右、Q/E 升降（沿相机自身轴），Shift 加速。
    if (GN::Key[GK_W]) input.aMoveFront += 1.0f;
    if (GN::Key[GK_S]) input.aMoveFront -= 1.0f;
    if (GN::Key[GK_A]) input.aMoveLeft += 1.0f;
    if (GN::Key[GK_D]) input.aMoveLeft -= 1.0f;
    if (GN::Key[GK_Q]) input.aMoveUp -= 1.0f;
    if (GN::Key[GK_E]) input.aMoveUp += 1.0f;
    input.aBoost = (GN::Key[GK_SHIFT] != 0);

    // 键盘转视角：方向键 / J L（鼠标不可用时始终可用的一条路径），增量 = rad/s × dt。
    const float lookStep = KEY_LOOK_SPEED * dt;
    float lookYaw = 0.0f;
    float lookPitch = 0.0f;
    if (GN::Key[GK_LEFT] || GN::Key[GK_J]) lookYaw -= lookStep;
    if (GN::Key[GK_RIGHT] || GN::Key[GK_L]) lookYaw += lookStep;
    if (GN::Key[GK_UP]) lookPitch += lookStep;
    if (GN::Key[GK_DOWN]) lookPitch -= lookStep;

    // 鼠标拖拽转视角（按住左键或右键；光标绝对位置求增量，松开/首次按下重新锚定）。
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
            // 屏幕 y 向下：鼠标上移 (dy<0) 对应抬头。
            lookYaw += static_cast<float>(dx) * MOUSE_LOOK_SPEED;
            lookPitch += -static_cast<float>(dy) * MOUSE_LOOK_SPEED;
        }
    }
    input.aLookYaw = lookYaw;
    input.aLookPitch = lookPitch;

    // 轨道模式专属：滚轮缩放 + 中键拖动平移（自由模式保持旧行为：滚轮/中键无作用）。
    if (orbitMode) {
        if (GN::Mouse1[GM_TOP]) input.aZoom += ORBIT_ZOOM_STEP;
        if (GN::Mouse1[GM_BOTTOM]) input.aZoom -= ORBIT_ZOOM_STEP;
        if (GNg::ViewCur) {
            const int mx = GNg::ViewCur->mouseMX;
            const int my = GNg::ViewCur->mouseMY;
            const bool panning = (GN::Mouse[GM_CENTRE] != 0);
            if (!panning) {
                gLastPanValid = false;
                gLastPanX = mx;
                gLastPanY = my;
            } else if (!gLastPanValid) {
                gLastPanValid = true;
                gLastPanX = mx;
                gLastPanY = my;
            } else {
                const int dx = mx - gLastPanX;
                const int dy = my - gLastPanY;
                gLastPanX = mx;
                gLastPanY = my;
                const float panScale =
                    (cameraDistance() > 1.0f ? cameraDistance() : 1.0f) * ORBIT_PAN_SPEED;
                input.aPanRight += static_cast<float>(dx) * panScale;
                input.aPanUp += -static_cast<float>(dy) * panScale;
            }
        }
        // 轨道下把视角增量取反：向右拖 = 视角右转 = 相机绕焦点左移，和自由飞行手感一致。
        input.aLookYaw = -input.aLookYaw;
        input.aLookPitch = -input.aLookPitch;
    }

    const float moveSpeed =
        (gHeight > 1.0f ? gHeight : 1.0f) * MOVE_SPEED_FACTOR; // m/s，Shift ×3 由 ApplyInput 处理
    gCamera.ApplyInput(input, dt, moveSpeed, 1.0f);
}
