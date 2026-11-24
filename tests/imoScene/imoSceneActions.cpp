// tests/imoScene/imoSceneActions.cpp — imoScene 键位绑定表（唯一的键位来源）。
//
// 与拆分前逐键等价：R 重置、方向键/J L 转视角、滚轮缩放、左/右键拖动旋转、中键平移。

#include "imoSceneActions.h"

namespace imoScene {

GN::cINPUT_MAP gInput;

void bindImoSceneActions(GN::cINPUT_MAP &map) {
    map.Bind(GN::eACT_RESET_VIEW, GK_R);
    map.Bind(GN::eACT_LOOK_LEFT, GK_LEFT);
    map.Bind(GN::eACT_LOOK_LEFT, GK_J);
    map.Bind(GN::eACT_LOOK_RIGHT, GK_RIGHT);
    map.Bind(GN::eACT_LOOK_RIGHT, GK_L);
    map.Bind(GN::eACT_LOOK_UP, GK_UP);
    map.Bind(GN::eACT_LOOK_DOWN, GK_DOWN);
    map.Bind(GN::eACT_ZOOM_IN, GN::INPUT_MOUSE_BASE + GM_TOP);
    map.Bind(GN::eACT_ZOOM_OUT, GN::INPUT_MOUSE_BASE + GM_BOTTOM);

    map.Bind(eIMO_ACT_DRAG_ROTATE, GN::INPUT_MOUSE_BASE + GM_LEFT);
    map.Bind(eIMO_ACT_DRAG_ROTATE, GN::INPUT_MOUSE_BASE + GM_RIGHT);
    map.Bind(eIMO_ACT_DRAG_PAN, GN::INPUT_MOUSE_BASE + GM_CENTRE);
}

} // namespace imoScene
