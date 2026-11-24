// tests/mmdViewer/mmdViewerActions.h — 查看器的动作编号与键位绑定表（P0：键位与语义分离）
//
// 改键只改这里；游戏逻辑只认动作编号。移动/转视角/加速/滚轮/重置复用引擎自带的
// 便利动作（cINPUT_MAP::LoadDefaultWASD），查看器独有的开关另编一套。
#ifndef MMD_VIEWER_ACTIONS_H
#define MMD_VIEWER_ACTIONS_H

#include "GN.h"
#include "GN_InputMap.h"

// 从 INPUT_ACT_USER_FIRST 起编号，避免和引擎便利动作（1..15）冲突。
enum eMMD_ACT : GN::eACTION {
    eMMD_ACT_WEATHER_CLEAR = GN::INPUT_ACT_USER_FIRST,
    eMMD_ACT_WEATHER_CLOUDY,
    eMMD_ACT_WEATHER_RAIN,
    eMMD_ACT_WEATHER_SNOW,
    eMMD_ACT_WEATHER_FOG,
    eMMD_ACT_WEATHER_STORM,
    eMMD_ACT_TOGGLE_GIZMOS, // G：坐标轴 + 包围盒
    eMMD_ACT_TOGGLE_COORDINFO, // H：左上角坐标叠层
    eMMD_ACT_TOGGLE_PRECIP, // O：降水粒子
    eMMD_ACT_TOGGLE_DECAL, // P：地面溅射贴花
    eMMD_ACT_TOGGLE_PARTICLEBOX, // V：粒子四边形包围盒
    eMMD_ACT_TOGGLE_TONE, // T：FXAA
    eMMD_ACT_TOGGLE_SHADOW, // K：方向光阴影
    eMMD_ACT_TOGGLE_ATMOSPHERE, // B：空气透视
    eMMD_ACT_TOGGLE_CLOUD, // X：立方体云
    eMMD_ACT_EXPOSURE_DOWN, // 9：EV −0.25
    eMMD_ACT_EXPOSURE_UP, // 0：EV +0.25
    eMMD_ACT_BLOOM_DOWN, // 7：Bloom −0.05
    eMMD_ACT_BLOOM_UP, // 8：Bloom +0.05
    eMMD_ACT_DRAG_LOOK, // 鼠标左/右键拖动转视角
    eMMD_ACT_DRAG_PAN, // 鼠标中键拖动平移（仅轨道模式）
    eMMD_ACT_COUNT // 哨兵（不是动作）
};

// 绑定整张表（含引擎便利动作）；重复调用不会产生重复键。
void bindMmdViewerActions(GN::cINPUT_MAP &map);

#endif // MMD_VIEWER_ACTIONS_H
