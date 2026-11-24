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
    eIMO_ACT_COUNT
};

// 唯一的输入映射实例（定义在 imoSceneActions.cpp）；每帧由 cameraUpdate 驱动。
extern GN::cINPUT_MAP gInput;

// 绑定整张表（重复调用不会产生重复键）。
void bindImoSceneActions(GN::cINPUT_MAP &map);

} // namespace imoScene
#endif // GN_IMOSCENE_ACTIONS_H
