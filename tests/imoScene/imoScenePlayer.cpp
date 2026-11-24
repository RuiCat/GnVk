// tests/imoScene/imoScenePlayer.cpp — 胶囊角色 + 固定步长 + 相机跟随（P0/P5 接线）。
//
// 一帧顺序：cameraUpdate(dt)（采样输入/转视角）→ playerUpdate(dt)（固定步长跑角色）。
// 角色走 "wishDir + sprint + jump" 的意图接口，不直接读键位（键位只在 imoSceneActions）。
#include "imoScenePlayer.h"

#include "GN_Character.h"
#include "GN_GameClock.h"
#include "imoSceneActions.h"
#include "imoSceneCamera.h"

#include <cmath>

namespace imoScene {

namespace {

// 房间碰撞只取"地面 + 四面墙"：家具不参与碰撞，避免角色被隐形盒卡住。
// 数值来自 room.glb 的 AABB（x[-13.06, 8.37] z[-8.49, 5.62]），墙半厚 0.3 m。
const float kRoomWestX = -13.3f;
const float kRoomEastX = 8.6f;
const float kRoomCenterZ = -1.4f;
const float kRoomHalfZ = 7.5f;
const float kRoomNorthZ = -8.7f;
const float kRoomSouthZ = 5.9f;
const float kRoomCenterX = -2.3f;
const float kRoomHalfX = 10.9f;
const float kWallHalfY = 1.5f;

// 固定步长与物理同参（1/60、最多 4 子步），渲染插值因子暂未使用。
const float kFixedStep = 1.0f / 60.0f;
const uint32_t kMaxSubSteps = 4;

GN3d::cCHARACTER gCharacter;
GN::cGAME_CLOCK gClock;
float gCharacterHeight = 1.7f;
GNg::sVEC3 gSmoothedFoot;
bool gSmoothedValid = false;
bool gReady = false;

GNg::sVEC3 makeVec(float x, float y, float z) {
    GNg::sVEC3 v;
    v.x = x;
    v.y = y;
    v.z = z;
    return v;
}

// 房间四周墙：只挡水平移动，挡住后角色贴墙滑（cCHARACTER 内部解穿透）。
void buildRoomCollision() {
    gCharacter.AddFloor(makeVec(0.0f, 0.0f, 0.0f), 14.0f, 9.0f, 0.0f);
    gCharacter.AddBox(makeVec(kRoomWestX, kWallHalfY, kRoomCenterZ),
                      makeVec(0.3f, kWallHalfY, kRoomHalfZ));
    gCharacter.AddBox(makeVec(kRoomEastX, kWallHalfY, kRoomCenterZ),
                      makeVec(0.3f, kWallHalfY, kRoomHalfZ));
    gCharacter.AddBox(makeVec(kRoomCenterX, kWallHalfY, kRoomNorthZ),
                      makeVec(kRoomHalfX, kWallHalfY, 0.3f));
    gCharacter.AddBox(makeVec(kRoomCenterX, kWallHalfY, kRoomSouthZ),
                      makeVec(kRoomHalfX, kWallHalfY, 0.3f));
}

} // namespace

void playerInit(const GNg::sVEC3 &startCenter, float characterHeight) {
    GN3d::sCHARACTER_DESC desc;
    desc.aRadius = 0.3f;
    desc.aHeight = (characterHeight > 0.5f) ? characterHeight : 1.7f;
    gCharacterHeight = desc.aHeight;
    gSmoothedValid = false;
    desc.aMaxSlopeDeg = 45.0f;
    desc.aStepHeight = 0.35f;
    desc.aWalkSpeed = 3.0f;
    desc.aSprintSpeed = 5.5f;
    desc.aJumpSpeed = 5.0f;
    gCharacter.Init(desc);
    buildRoomCollision();
    gCharacter.SetPosition(startCenter);
    gCharacter.SetVelocity(makeVec(0.0f, 0.0f, 0.0f));
    gClock.SetFixedStep(kFixedStep, kMaxSubSteps);
    gReady = true;
}

void playerUpdate(float deltaSeconds) {
    if (!gReady) {
        return;
    }
    gClock.Tick(deltaSeconds);

    // 相机 yaw → 世界前/右轴（约定：yaw=0 时相机在 +Z 看向 -Z）。
    const float yaw = cameraYaw();
    const float forwardX = -std::sin(yaw);
    const float forwardZ = -std::cos(yaw);
    const float rightX = std::cos(yaw);
    const float rightZ = -std::sin(yaw);

    const float axisForward = gInput.Axis1D(eIMO_ACT_MOVE_BACK, eIMO_ACT_MOVE_FRONT);
    const float axisRight = gInput.Axis1D(eIMO_ACT_MOVE_LEFT, eIMO_ACT_MOVE_RIGHT);
    float wishX = forwardX * axisForward + rightX * axisRight;
    float wishZ = forwardZ * axisForward + rightZ * axisRight;
    const float length = std::sqrt(wishX * wishX + wishZ * wishZ);
    if (length > 1.0f) {
        wishX /= length;
        wishZ /= length;
    }

    const bool sprint = gInput.Down(eIMO_ACT_SPRINT);
    const bool jumpRequested = gInput.Pressed(eIMO_ACT_JUMP);
    const GNg::sVEC3 wish = makeVec(wishX, 0.0f, wishZ);
    for (uint32_t step = 0; step < gClock.FixedSteps(); ++step) {
        // 起跳只在第一个子步消费一次，避免同一帧连跳。
        gCharacter.Move(wish, sprint, jumpRequested && step == 0, gClock.FixedStepSize());
    }

    // 渲染位置做指数平滑：cCHARACTER 每步"重力下沉 → 地面吸附"会让 y 抖 ~8 mm
    // （实测 centerY 在 0.8137–0.8218 之间逐帧交替）。模型和相机都用平滑后的脚底，
    // 否则角色自身会抖、阴影拟合包围盒也会逐帧跳（表现为贴图/阴影整体偏移）。
    const GNg::sVEC3 rawFoot = gCharacter.FootPosition();
    if (!gSmoothedValid) {
        gSmoothedFoot = rawFoot;
        gSmoothedValid = true;
    } else {
        const float blend = 1.0f - std::exp(-14.0f * (deltaSeconds > 0.0f ? deltaSeconds : 0.0f));
        gSmoothedFoot.x += (rawFoot.x - gSmoothedFoot.x) * blend;
        gSmoothedFoot.z += (rawFoot.z - gSmoothedFoot.z) * blend;
        // 竖直方向单独用更慢的平滑（~1/6 s）：引擎着地时每步"下沉/吸附"造成 ~8 mm
        // 的逐帧抖动，60 Hz 下被衰减到 0.1 mm 以下；但最终会精确收敛到地面高度
        // （死区方案会停在离地 2 cm，脚看起来是悬空的）。
        const float blendY = 1.0f - std::exp(-6.0f * (deltaSeconds > 0.0f ? deltaSeconds : 0.0f));
        gSmoothedFoot.y += (rawFoot.y - gSmoothedFoot.y) * blendY;
    }
    cameraFollow(makeVec(gSmoothedFoot.x, gSmoothedFoot.y + gCharacterHeight * 0.5f + 0.15f,
                         gSmoothedFoot.z));

    // 临时诊断（IMO_SCENE_DEBUG_PLAYER=1）：记录每帧角色/焦点高度，定位"上下震动"。
    const char *debugPlayer = std::getenv("IMO_SCENE_DEBUG_PLAYER");
    if (debugPlayer != 0 && debugPlayer[0] == '1') {
        std::fprintf(stderr, "[PLAYER] dt=%.4f footY=%.6f smoothY=%.6f steps=%u\n",
                     deltaSeconds, rawFoot.y, gSmoothedFoot.y, gClock.FixedSteps());
    }
}

void playerShutdown() {
    gCharacter.ClearGeometry();
    gReady = false;
}

GNg::sVEC3 playerFootPosition() {
    // 渲染用平滑位置：没初始化过（首帧前）就退回原始脚底。
    return gSmoothedValid ? gSmoothedFoot : gCharacter.FootPosition();
}

bool playerGrounded() {
    return gCharacter.IsGrounded();
}

} // namespace imoScene