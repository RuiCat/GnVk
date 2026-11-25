#pragma once
#ifndef GN_IMOSCENE_ACTIONS_H
#define GN_IMOSCENE_ACTIONS_H
// tests/imoScene/imoSceneActions.h — imoScene 的动作编号与键位绑定表（P0）。
//
// 与 mmdViewer 一样：改键只改绑定表，相机逻辑只读动作。
// 复用引擎便利动作里的"重置视角 / 方向键转视角 / 滚轮缩放"，拖动类动作另编。
#include "GN.h"
#include "GN_InputMap.h"

namespace imoScene {

enum eIMO_ACT : GN::eACTION {
    eIMO_ACT_DRAG_ROTATE = GN::INPUT_ACT_USER_FIRST, // 鼠标左/右键拖动旋转
    eIMO_ACT_DRAG_PAN, // 鼠标中键拖动平移
    eIMO_ACT_MOVE_FRONT, // W：沿相机朝向前进
    eIMO_ACT_MOVE_BACK, // S
    eIMO_ACT_MOVE_LEFT, // A
    eIMO_ACT_MOVE_RIGHT, // D
    eIMO_ACT_JUMP, // Space：仅着地生效
    eIMO_ACT_SPRINT, // Shift：走 aSprintSpeed
    eIMO_ACT_OUTFIT_CAT, // 1：猫耳/铃铛/尾巴显隐
    eIMO_ACT_OUTFIT_HAT, // 2：帽子（毛线帽/女巫帽）
    eIMO_ACT_OUTFIT_MASK, // 3：口罩
    eIMO_ACT_OUTFIT_RIBBON, // 4：丝带/领结/项圈
    eIMO_ACT_OUTFIT_HAIR, // 5：循环切换发型变体（Mesh_Hair*）
    eIMO_ACT_OUTFIT_BODY, // 6：循环切换身体/服装变体（Mesh_Body*）
    eIMO_ACT_OUTFIT_RESET, // 0：全部显示
    eIMO_ACT_COUNT
};

// 唯一的输入映射实例（定义在 imoSceneActions.cpp）；每帧由 cameraUpdate 驱动。
extern GN::cINPUT_MAP gInput;

// 绑定整张表（重复调用不会产生重复键）。
void bindImoSceneActions(GN::cINPUT_MAP &map);

} // namespace imoScene
#endif // GN_IMOSCENE_ACTIONS_H