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
bool cameraBuildViewProj(float aspect, GNg::sMAT44 &viewProjection);

// 相机世界位置（绘制/排序用）。
GNg::sVEC3 cameraPosition();

} // namespace imoScene
#endif // GN_IMOSCENE_CAMERA_H
