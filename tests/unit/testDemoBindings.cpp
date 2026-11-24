// testDemoBindings.cpp — 两个示例的键位绑定表与拆分前逐键等价（P0 验收）。
//
// 迁移前：mmdViewer 与 imoScene 共 42 处直接读 GN::Key/GN::Key1/GN::Mouse/GN::Mouse1。
// 迁移后键位只出现在 mmdViewerActions.cpp / imoSceneActions.cpp 两张表里，本用例把每一条
// 绑定都按"按下该键 → 该动作成立"走一遍，保证换表不换行为。
#include "testFramework.h"

#include <GN.h>
#include "GN_InputMap.h"
#include "mmdViewerActions.h"
#include "imoSceneActions.h"

namespace {

void resetEngineInput() {
    for (int i = 0; i < GN_MAX_KEY_NUM; ++i) {
        GN::Key[i] = false;
        GN::Key0[i] = false;
        GN::Key1[i] = false;
        GN::Key11[i] = false;
    }
    GN::KeyDown = false;
    for (int m = 0; m < GN_MAX_MK_NUM; ++m) {
        GN::Mouse[m] = false;
        GN::Mouse0[m] = false;
        GN::Mouse1[m] = false;
        GN::Mouse2[m] = false;
    }
}

bool hasKey(const GN::cINPUT_MAP &map, GN::eACTION action, uint16_t key) {
    for (int i = 0; i < map.BindCount(action); ++i) {
        if (map.KeyOf(action, i) == key) {
            return true;
        }
    }
    return false;
}

// 模拟"本帧按下 key 并 Update"，返回该帧动作是否成立（按下与按住都查）。
struct sPressResult {
    bool aDown;
    bool aPressed;
};

// 滚轮（GM_TOP/GM_BOTTOM）在引擎里只有 Mouse1 脉冲，没有"按住"态——照实模拟。
bool isWheelKey(uint16_t key) {
    return key == GN::INPUT_MOUSE_BASE + GM_TOP || key == GN::INPUT_MOUSE_BASE + GM_BOTTOM;
}

sPressResult pressAndQuery(GN::cINPUT_MAP &map, uint16_t key, GN::eACTION action) {
    resetEngineInput();
    if (key >= GN::INPUT_MOUSE_BASE) {
        if (!isWheelKey(key)) {
            GN::Mouse[key - GN::INPUT_MOUSE_BASE] = true;
        }
        GN::Mouse1[key - GN::INPUT_MOUSE_BASE] = true;
    } else {
        GN::Key[key] = true;
        GN::Key1[key] = true;
    }
    map.Update(1.0f / 60.0f);
    sPressResult result;
    result.aDown = map.Down(action);
    result.aPressed = map.Pressed(action);
    return result;
}

} // namespace

// 查看器：每条绑定的键都能触发对应动作（含"两个键绑同一动作"的转视角组合键）。
TEST_CASE(demoBindingsMmdViewerEachKeyFires) {
    resetEngineInput();
    GN::cINPUT_MAP map;
    bindMmdViewerActions(map);

    // 天气 1–6
    const GN::eACTION weather[6] = {eMMD_ACT_WEATHER_CLEAR, eMMD_ACT_WEATHER_CLOUDY,
                                    eMMD_ACT_WEATHER_RAIN, eMMD_ACT_WEATHER_SNOW,
                                    eMMD_ACT_WEATHER_FOG, eMMD_ACT_WEATHER_STORM};
    const uint16_t weatherKeys[6] = {GK_1, GK_2, GK_3, GK_4, GK_5, GK_6};
    for (int i = 0; i < 6; ++i) {
        CHECK(hasKey(map, weather[i], weatherKeys[i]));
        const sPressResult r = pressAndQuery(map, weatherKeys[i], weather[i]);
        CHECK(r.aDown);
        CHECK(r.aPressed);
        // 不能串到别的天气
        for (int j = 0; j < 6; ++j) {
            if (j != i) {
                CHECK(!map.Down(weather[j]));
            }
        }
    }

    // 调试/渲染开关 G H O P V T K B X
    struct sToggleCase {
        GN::eACTION action;
        uint16_t key;
    };
    const sToggleCase toggles[] = {
        {eMMD_ACT_TOGGLE_GIZMOS, GK_G},       {eMMD_ACT_TOGGLE_COORDINFO, GK_H},
        {eMMD_ACT_TOGGLE_PRECIP, GK_O},       {eMMD_ACT_TOGGLE_DECAL, GK_P},
        {eMMD_ACT_TOGGLE_PARTICLEBOX, GK_V},  {eMMD_ACT_TOGGLE_TONE, GK_T},
        {eMMD_ACT_TOGGLE_SHADOW, GK_K},       {eMMD_ACT_TOGGLE_ATMOSPHERE, GK_B},
        {eMMD_ACT_TOGGLE_CLOUD, GK_X}};
    for (size_t i = 0; i < sizeof(toggles) / sizeof(toggles[0]); ++i) {
        CHECK(hasKey(map, toggles[i].action, toggles[i].key));
        const sPressResult r = pressAndQuery(map, toggles[i].key, toggles[i].action);
        CHECK(r.aPressed);
        CHECK(r.aDown);
    }

    // 曝光 9/0、Bloom 7/8
    CHECK(hasKey(map, eMMD_ACT_EXPOSURE_DOWN, GK_9));
    CHECK(hasKey(map, eMMD_ACT_EXPOSURE_UP, GK_0));
    CHECK(hasKey(map, eMMD_ACT_BLOOM_DOWN, GK_7));
    CHECK(hasKey(map, eMMD_ACT_BLOOM_UP, GK_8));
    CHECK(pressAndQuery(map, GK_9, eMMD_ACT_EXPOSURE_DOWN).aPressed);
    CHECK(!pressAndQuery(map, GK_9, eMMD_ACT_EXPOSURE_UP).aPressed);
    CHECK(!pressAndQuery(map, GK_0, eMMD_ACT_EXPOSURE_DOWN).aPressed);
    CHECK(pressAndQuery(map, GK_0, eMMD_ACT_EXPOSURE_UP).aPressed);
    CHECK(pressAndQuery(map, GK_7, eMMD_ACT_BLOOM_DOWN).aPressed);
    CHECK(pressAndQuery(map, GK_8, eMMD_ACT_BLOOM_UP).aPressed);
}

// 查看器相机：WASD/QE/Shift/方向键+J L/滚轮/F/R 的绑定一个不少。
TEST_CASE(demoBindingsMmdViewerCameraKeys) {
    resetEngineInput();
    GN::cINPUT_MAP map;
    bindMmdViewerActions(map);

    struct sMoveCase {
        GN::eACTION action;
        uint16_t key;
    };
    const sMoveCase moves[] = {
        {GN::eACT_MOVE_FRONT, GK_W}, {GN::eACT_MOVE_BACK, GK_S},
        {GN::eACT_MOVE_LEFT, GK_A},  {GN::eACT_MOVE_RIGHT, GK_D},
        {GN::eACT_MOVE_UP, GK_E},    {GN::eACT_MOVE_DOWN, GK_Q},
        {GN::eACT_BOOST, GK_SHIFT},  {GN::eACT_ORBIT_TOGGLE, GK_F},
        {GN::eACT_RESET_VIEW, GK_R}};
    for (size_t i = 0; i < sizeof(moves) / sizeof(moves[0]); ++i) {
        CHECK(hasKey(map, moves[i].action, moves[i].key));
        CHECK(pressAndQuery(map, moves[i].key, moves[i].action).aDown);
    }

    // 转视角：方向键与 J/L 都要绑上（鼠标不可用时的兜底路径）。
    CHECK(hasKey(map, GN::eACT_LOOK_LEFT, GK_LEFT));
    CHECK(hasKey(map, GN::eACT_LOOK_LEFT, GK_J));
    CHECK(hasKey(map, GN::eACT_LOOK_RIGHT, GK_RIGHT));
    CHECK(hasKey(map, GN::eACT_LOOK_RIGHT, GK_L));
    CHECK(hasKey(map, GN::eACT_LOOK_UP, GK_UP));
    CHECK(hasKey(map, GN::eACT_LOOK_DOWN, GK_DOWN));
    CHECK(pressAndQuery(map, GK_J, GN::eACT_LOOK_LEFT).aDown);
    CHECK(pressAndQuery(map, GK_L, GN::eACT_LOOK_RIGHT).aDown);

    // 滚轮：只有单次脉冲，没有按住态。
    const uint16_t zoomIn = GN::INPUT_MOUSE_BASE + GM_TOP;
    const uint16_t zoomOut = GN::INPUT_MOUSE_BASE + GM_BOTTOM;
    CHECK(hasKey(map, GN::eACT_ZOOM_IN, zoomIn));
    CHECK(hasKey(map, GN::eACT_ZOOM_OUT, zoomOut));
    const sPressResult in = pressAndQuery(map, zoomIn, GN::eACT_ZOOM_IN);
    CHECK(in.aPressed);
    CHECK(!in.aDown);

    // 鼠标拖动：左+右键都算转视角，中键是平移。
    CHECK(hasKey(map, eMMD_ACT_DRAG_LOOK, GN::INPUT_MOUSE_BASE + GM_LEFT));
    CHECK(hasKey(map, eMMD_ACT_DRAG_LOOK, GN::INPUT_MOUSE_BASE + GM_RIGHT));
    CHECK(hasKey(map, eMMD_ACT_DRAG_PAN, GN::INPUT_MOUSE_BASE + GM_CENTRE));
    CHECK(pressAndQuery(map, GN::INPUT_MOUSE_BASE + GM_RIGHT, eMMD_ACT_DRAG_LOOK).aDown);
    CHECK(pressAndQuery(map, GN::INPUT_MOUSE_BASE + GM_CENTRE, eMMD_ACT_DRAG_PAN).aDown);
}

// imoScene：R / 方向键+J L / 滚轮 / 左+右拖动 / 中键平移。
TEST_CASE(demoBindingsImoSceneKeys) {
    resetEngineInput();
    GN::cINPUT_MAP map;
    imoScene::bindImoSceneActions(map);

    CHECK(hasKey(map, GN::eACT_RESET_VIEW, GK_R));
    CHECK(pressAndQuery(map, GK_R, GN::eACT_RESET_VIEW).aPressed);

    CHECK(hasKey(map, GN::eACT_LOOK_LEFT, GK_LEFT));
    CHECK(hasKey(map, GN::eACT_LOOK_LEFT, GK_J));
    CHECK(hasKey(map, GN::eACT_LOOK_RIGHT, GK_RIGHT));
    CHECK(hasKey(map, GN::eACT_LOOK_RIGHT, GK_L));
    CHECK(hasKey(map, GN::eACT_LOOK_UP, GK_UP));
    CHECK(hasKey(map, GN::eACT_LOOK_DOWN, GK_DOWN));
    CHECK(pressAndQuery(map, GK_UP, GN::eACT_LOOK_UP).aDown);

    const uint16_t zoomIn = GN::INPUT_MOUSE_BASE + GM_TOP;
    const uint16_t zoomOut = GN::INPUT_MOUSE_BASE + GM_BOTTOM;
    CHECK(hasKey(map, GN::eACT_ZOOM_IN, zoomIn));
    CHECK(hasKey(map, GN::eACT_ZOOM_OUT, zoomOut));
    CHECK(pressAndQuery(map, zoomIn, GN::eACT_ZOOM_IN).aPressed);

    CHECK(hasKey(map, imoScene::eIMO_ACT_DRAG_ROTATE, GN::INPUT_MOUSE_BASE + GM_LEFT));
    CHECK(hasKey(map, imoScene::eIMO_ACT_DRAG_ROTATE, GN::INPUT_MOUSE_BASE + GM_RIGHT));
    CHECK(hasKey(map, imoScene::eIMO_ACT_DRAG_PAN, GN::INPUT_MOUSE_BASE + GM_CENTRE));
    CHECK(pressAndQuery(map, GN::INPUT_MOUSE_BASE + GM_LEFT, imoScene::eIMO_ACT_DRAG_ROTATE).aDown);
    CHECK(pressAndQuery(map, GN::INPUT_MOUSE_BASE + GM_CENTRE, imoScene::eIMO_ACT_DRAG_PAN).aDown);
}

// 两张表重复绑定不产生重复键（bind 会被调多次也不能让边沿行为漂移）。
TEST_CASE(demoBindingsAreIdempotent) {
    resetEngineInput();
    GN::cINPUT_MAP map;
    bindMmdViewerActions(map);
    const int firstLook = map.BindCount(GN::eACT_LOOK_LEFT);
    const int firstToggle = map.BindCount(eMMD_ACT_TOGGLE_GIZMOS);
    bindMmdViewerActions(map);
    CHECK_EQ(map.BindCount(GN::eACT_LOOK_LEFT), firstLook);
    CHECK_EQ(map.BindCount(eMMD_ACT_TOGGLE_GIZMOS), firstToggle);

    GN::cINPUT_MAP imoMap;
    imoScene::bindImoSceneActions(imoMap);
    const int imoRotate = imoMap.BindCount(imoScene::eIMO_ACT_DRAG_ROTATE);
    imoScene::bindImoSceneActions(imoMap);
    CHECK_EQ(imoMap.BindCount(imoScene::eIMO_ACT_DRAG_ROTATE), imoRotate);
    CHECK_EQ(imoMap.BindCount(imoScene::eIMO_ACT_DRAG_ROTATE), 2); // 左键 + 右键
}
