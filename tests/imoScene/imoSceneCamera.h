#pragma once
#ifndef GN_IMOSCENE_CAMERA_H
#define GN_IMOSCENE_CAMERA_H
// tests/imoScene/imoSceneCamera.h — 轨道相机与输入（重置 / 拖动旋转 / 滚轮缩放 / 中键平移）。
//
// 边界：只维护一个 GN3d::cCAMERA 与输入状态，不持有模型、不做绘制。
#include "GN.h"
#include "GN_3Dcamera.h"

namespace imoScene {

// 以模型中心与高度初始化相机（默认正面 +Z 视角，距离 = 高度 × 2.2）。
void cameraInit(const GNg::sVEC3 &focus, float height);

// 回到初始视角（R 键与启动时共用）。
void cameraReset();

// 每帧输入：R 重置、左/右键拖动旋转、滚轮缩放、中键平移、方向键旋转。
void cameraUpdate(float deltaSeconds);

// 取本帧 view-projection（aspect 非法或矩阵退化返回 false，调用方跳过本帧）。
// viewOut 非空时同时输出视图矩阵（阴影级联拟合要用）。
bool cameraBuildViewProj(float aspect, GNg::sMAT44 &viewProjection, GNg::sMAT44 *viewOut = 0);

// 垂直视场角（弧度）：阴影拟合需要与相机一致的投影参数。
float cameraFovYRadians();

// 相机世界位置（绘制/排序用）。
GNg::sVEC3 cameraPosition();

// 角色跟随：只移动轨道焦点（保留 yaw/pitch/距离），每帧在 playerUpdate 里调。
void cameraFollow(const GNg::sVEC3 &focus);

// 当前轨道 yaw（弧度）：playerUpdate 用它把 WASD 输入转成世界方向。
float cameraYaw();

} // namespace imoScene
#endif // GN_IMOSCENE_CAMERA_H
