#pragma once
#ifndef GN_IMOSCENE_PLAYER_H
#define GN_IMOSCENE_PLAYER_H
// tests/imoScene/imoScenePlayer.h — P0/P5 游戏侧接线：动作映射 → 固定步长 → 胶囊角色 → 相机跟随。
//
// 边界：只把「输入意图」变成「角色运动」和「相机跟随」，不碰模型加载与绘制。
// 输入采样（gInput.Update）由 cameraUpdate 负责，因此 playerUpdate 必须排在它之后。
#include "GN.h"

namespace imoScene {

// 初始化胶囊角色（startCenter = 胶囊中心）与碰撞几何（地面 + 房间四周墙）。
void playerInit(const GNg::sVEC3 &startCenter, float characterHeight);

// 每帧一次：固定步长推进角色，最后把相机焦点移到角色胸口。
void playerUpdate(float deltaSeconds);

// 释放碰撞几何（AppUnInit 调一次）。
void playerShutdown();

// 角色脚底世界坐标（写模型变换与贴地阴影用）。
GNg::sVEC3 playerFootPosition();

// 当前是否着地（HUD/调试用）。
bool playerGrounded();

} // namespace imoScene
#endif