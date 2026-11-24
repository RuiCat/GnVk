// tests/mmdViewer/mmdViewerActions.cpp — 查看器键位绑定表（唯一的键位来源）。
//
// 与拆分前逐键等价（迁移前分布在 mmdViewer.cpp 的 AppRun 与 mmdViewerCamera.cpp）：
//   1–6 天气、G/H/O/P/V 调试开关、T/K/B/X 渲染开关、9/0 曝光、7/8 Bloom、
//   F 自由↔轨道、R 回默认视角、WASD+QE 平移、Shift 加速、方向键/J L 转视角、滚轮缩放。

#include "mmdViewerActions.h"

void bindMmdViewerActions(GN::cINPUT_MAP &map) {
    map.LoadDefaultWASD(); // W/S/A/D、Q/E、Shift、方向键 + J/L、滚轮、F、R

    map.Bind(eMMD_ACT_WEATHER_CLEAR, GK_1);
    map.Bind(eMMD_ACT_WEATHER_CLOUDY, GK_2);
    map.Bind(eMMD_ACT_WEATHER_RAIN, GK_3);
    map.Bind(eMMD_ACT_WEATHER_SNOW, GK_4);
    map.Bind(eMMD_ACT_WEATHER_FOG, GK_5);
    map.Bind(eMMD_ACT_WEATHER_STORM, GK_6);

    map.Bind(eMMD_ACT_TOGGLE_GIZMOS, GK_G);
    map.Bind(eMMD_ACT_TOGGLE_COORDINFO, GK_H);
    map.Bind(eMMD_ACT_TOGGLE_PRECIP, GK_O);
    map.Bind(eMMD_ACT_TOGGLE_DECAL, GK_P);
    map.Bind(eMMD_ACT_TOGGLE_PARTICLEBOX, GK_V);
    map.Bind(eMMD_ACT_TOGGLE_TONE, GK_T);
    map.Bind(eMMD_ACT_TOGGLE_SHADOW, GK_K);
    map.Bind(eMMD_ACT_TOGGLE_ATMOSPHERE, GK_B);
    map.Bind(eMMD_ACT_TOGGLE_CLOUD, GK_X);

    map.Bind(eMMD_ACT_EXPOSURE_DOWN, GK_9);
    map.Bind(eMMD_ACT_EXPOSURE_UP, GK_0);
    map.Bind(eMMD_ACT_BLOOM_DOWN, GK_7);
    map.Bind(eMMD_ACT_BLOOM_UP, GK_8);

    map.Bind(eMMD_ACT_DRAG_LOOK, GN::INPUT_MOUSE_BASE + GM_LEFT);
    map.Bind(eMMD_ACT_DRAG_LOOK, GN::INPUT_MOUSE_BASE + GM_RIGHT);
    map.Bind(eMMD_ACT_DRAG_PAN, GN::INPUT_MOUSE_BASE + GM_CENTRE);
}
