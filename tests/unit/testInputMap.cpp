// testInputMap.cpp — 动作映射（P0）纯 CPU 单测。
// 覆盖 docs/ENTITY_CONTROLLERS_PLAN.md §5.1 P0 的验收口径：
//   多键绑同一动作、边沿只在首帧为真、Unbind 后失效、Axis1D 对向抵消、
//   未绑定动作恒 false、Update(dt) 为 0/负/NaN 不崩。
#include "testFramework.h"

#include <GN.h>
#include "GN_InputMap.h"

#include <cmath>

using namespace GN;

namespace {

enum {
    eA_JUMP = INPUT_ACT_USER_FIRST,
    eA_FORWARD,
    eA_BACK,
    eA_FIRE,
    eA_CAM_LEFT,
    eA_CAM_RIGHT
};

// 引擎输入是全局数组：每个用例开头清干净，避免互相污染。
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

// 引擎每帧会把 Key0/Key1（Mouse0/Mouse1）这类"本帧事件"数组清零，
// 这里在 Update 之后清掉，模拟真实帧边界（否则边沿会在测试里一直为真）。
void clearEdge(uint16_t key) {
    if (key >= INPUT_MOUSE_BASE) {
        GN::Mouse0[key - INPUT_MOUSE_BASE] = false;
        GN::Mouse1[key - INPUT_MOUSE_BASE] = false;
    } else {
        GN::Key0[key] = false;
        GN::Key1[key] = false;
    }
}

// 一帧"按下"（引擎同时给按住态与本帧单次键）
void framePress(cINPUT_MAP &map, uint16_t key, float dt = 1.0f / 60.0f) {
    if (key >= INPUT_MOUSE_BASE) {
        GN::Mouse[key - INPUT_MOUSE_BASE] = true;
        GN::Mouse1[key - INPUT_MOUSE_BASE] = true;
    } else {
        GN::Key[key] = true;
        GN::Key1[key] = true;
    }
    map.Update(dt);
    clearEdge(key);
}

// 一帧"按住不放"（引擎本帧不给单次键）
void frameHold(cINPUT_MAP &map, uint16_t key, float dt = 1.0f / 60.0f) {
    if (key >= INPUT_MOUSE_BASE) {
        GN::Mouse[key - INPUT_MOUSE_BASE] = true;
    } else {
        GN::Key[key] = true;
    }
    map.Update(dt);
    clearEdge(key);
}

// 一帧"松开"（按住态归零，引擎给松开键）
void frameRelease(cINPUT_MAP &map, uint16_t key, float dt = 1.0f / 60.0f) {
    if (key >= INPUT_MOUSE_BASE) {
        GN::Mouse[key - INPUT_MOUSE_BASE] = false;
        GN::Mouse0[key - INPUT_MOUSE_BASE] = true;
    } else {
        GN::Key[key] = false;
        GN::Key0[key] = true;
    }
    map.Update(dt);
    clearEdge(key);
}

void frameIdle(cINPUT_MAP &map, float dt = 1.0f / 60.0f) { map.Update(dt); }

} // namespace

TEST_CASE(inputMapBindAndUnbind) {
    resetEngineInput();
    cINPUT_MAP map;
    CHECK(map.Bind(eA_JUMP, GK_SPACE));
    CHECK_EQ(map.BindCount(eA_JUMP), 1);
    CHECK_EQ(map.KeyOf(eA_JUMP, 0), (uint16_t)GK_SPACE);
    CHECK(map.Bind(eA_JUMP, GK_SPACE)); // 重复绑定幂等
    CHECK_EQ(map.BindCount(eA_JUMP), 1);
    CHECK(map.Bind(eA_JUMP, GK_RETURN)); // 多键绑同一动作
    CHECK_EQ(map.BindCount(eA_JUMP), 2);

    CHECK(!map.Unbind(eA_JUMP, GK_TAB)); // 没绑过
    CHECK(map.Unbind(eA_JUMP, GK_SPACE)); // 解掉一个
    CHECK_EQ(map.BindCount(eA_JUMP), 1);
    CHECK(map.Unbind(eA_JUMP)); // 省略 key = 全解
    CHECK_EQ(map.BindCount(eA_JUMP), 0);
    CHECK(!map.Down(eA_JUMP));
}

TEST_CASE(inputMapRejectsInvalidBind) {
    resetEngineInput();
    cINPUT_MAP map;
    CHECK(!map.Bind(eACT_NONE, GK_SPACE)); // 0 是"无动作"
    CHECK(!map.Bind(INPUT_ACTION_MAX, GK_SPACE)); // 越界
    CHECK(!map.Bind(eA_JUMP, (uint16_t)(GN_MAX_KEY_NUM + 7))); // 键位空洞
    CHECK(!map.Bind(eA_JUMP, (uint16_t)(INPUT_MOUSE_BASE + GN_MAX_MK_NUM))); // 鼠标越界

    // 槽位上限
    CHECK(map.Bind(eA_JUMP, GK_A));
    CHECK(map.Bind(eA_JUMP, GK_B));
    CHECK(map.Bind(eA_JUMP, GK_C));
    CHECK(map.Bind(eA_JUMP, GK_D));
    CHECK(!map.Bind(eA_JUMP, GK_E));
    CHECK_EQ(map.BindCount(eA_JUMP), INPUT_MAX_BIND_PER_ACTION);
}

// 一个动作绑键盘 + 鼠标：任一按下即成立。
TEST_CASE(inputMapMultipleKeysSameAction) {
    resetEngineInput();
    cINPUT_MAP map;
    CHECK(map.Bind(eA_FIRE, GK_CONTROL));
    CHECK(map.Bind(eA_FIRE, INPUT_MOUSE_BASE + GM_LEFT));

    framePress(map, GK_CONTROL);
    CHECK(map.Down(eA_FIRE));
    CHECK(map.Pressed(eA_FIRE));
    frameRelease(map, GK_CONTROL);
    CHECK(!map.Down(eA_FIRE));

    framePress(map, INPUT_MOUSE_BASE + GM_LEFT);
    CHECK(map.Down(eA_FIRE));
    CHECK(map.Pressed(eA_FIRE));
}

// 边沿只在按下后的第一帧为真；第二帧只剩"按住"。
TEST_CASE(inputMapEdgeOnlyOnFirstFrame) {
    resetEngineInput();
    cINPUT_MAP map;
    map.Bind(eA_JUMP, GK_SPACE);

    framePress(map, GK_SPACE);
    CHECK(map.Pressed(eA_JUMP));
    CHECK(map.Down(eA_JUMP));
    CHECK(!map.Released(eA_JUMP));

    frameHold(map, GK_SPACE);
    CHECK(!map.Pressed(eA_JUMP));
    CHECK(map.Down(eA_JUMP));

    frameRelease(map, GK_SPACE);
    CHECK(!map.Pressed(eA_JUMP));
    CHECK(!map.Down(eA_JUMP));
    CHECK(map.Released(eA_JUMP));

    frameIdle(map);
    CHECK(!map.Released(eA_JUMP)); // 松开边沿同样只有一帧
}

// 一帧内按下又松开（Key1 有脉冲、Key 已归零）也要算一次按下 —— 否则快速点按会丢。
TEST_CASE(inputMapTapWithinOneFrame) {
    resetEngineInput();
    cINPUT_MAP map;
    map.Bind(eA_JUMP, GK_SPACE);
    GN::Key[GK_SPACE] = false;
    GN::Key1[GK_SPACE] = true;
    map.Update(1.0f / 60.0f);
    CHECK(map.Pressed(eA_JUMP));
    CHECK(!map.Down(eA_JUMP));
    GN::Key1[GK_SPACE] = false;
    map.Update(1.0f / 60.0f);
    CHECK(!map.Pressed(eA_JUMP));
}

TEST_CASE(inputMapUnbindDisablesAction) {
    resetEngineInput();
    cINPUT_MAP map;
    map.Bind(eA_JUMP, GK_SPACE);
    framePress(map, GK_SPACE);
    CHECK(map.Down(eA_JUMP));
    map.Unbind(eA_JUMP);
    frameHold(map, GK_SPACE);
    CHECK(!map.Down(eA_JUMP));
    CHECK(!map.Pressed(eA_JUMP));
}

TEST_CASE(inputMapAxis1DOppositeCancel) {
    resetEngineInput();
    cINPUT_MAP map;
    map.Bind(eA_FORWARD, GK_W);
    map.Bind(eA_BACK, GK_S);

    framePress(map, GK_W);
    CHECK_NEAR(map.Axis1D(eA_BACK, eA_FORWARD), 1.0f, 1e-6);
    frameRelease(map, GK_W);

    framePress(map, GK_S);
    CHECK_NEAR(map.Axis1D(eA_BACK, eA_FORWARD), -1.0f, 1e-6);

    frameHold(map, GK_W); // 对向同时按住 → 抵消
    CHECK_NEAR(map.Axis1D(eA_BACK, eA_FORWARD), 0.0f, 1e-6);
}

TEST_CASE(inputMapAxis2DPacks) {
    resetEngineInput();
    cINPUT_MAP map;
    map.Bind(eA_CAM_LEFT, GK_LEFT);
    map.Bind(eA_CAM_RIGHT, GK_RIGHT);
    map.Bind(eA_FORWARD, GK_W);
    map.Bind(eA_BACK, GK_S);

    GN::Key[GK_RIGHT] = true;
    GN::Key1[GK_RIGHT] = true;
    map.Update(1.0f / 60.0f);
    const GNg::sVEC2 axis = map.Axis2D(eA_CAM_LEFT, eA_CAM_RIGHT, eA_BACK, eA_FORWARD);
    CHECK_NEAR(axis.x, 1.0f, 1e-6);
    CHECK_NEAR(axis.y, 0.0f, 1e-6);
}

TEST_CASE(inputMapUnboundActionAlwaysFalse) {
    resetEngineInput();
    cINPUT_MAP map;
    GN::Key[GK_W] = true;
    GN::Key1[GK_W] = true;
    map.Update(1.0f / 60.0f);
    CHECK(!map.Down(eA_FORWARD));
    CHECK(!map.Pressed(eA_FORWARD));
    CHECK(!map.Released(eA_FORWARD));
    CHECK(!map.DoubleTapped(eA_FORWARD));
    CHECK_NEAR(map.Axis1D(eA_BACK, eA_FORWARD), 0.0f, 1e-6);
    // 非法动作编号也不崩
    CHECK(!map.Down(eACT_NONE));
    CHECK(!map.Down((eACTION)9999));
    CHECK(!map.Pressed((eACTION)9999));
}

TEST_CASE(inputMapUpdateInvalidDtDoesNotCrash) {
    resetEngineInput();
    cINPUT_MAP map;
    map.Bind(eA_JUMP, GK_SPACE);
    framePress(map, GK_SPACE, 0.0f);
    CHECK(map.Pressed(eA_JUMP)); // 边沿与 dt 无关
    GN::Key[GK_SPACE] = false;
    GN::Key1[GK_SPACE] = false;
    map.Update(-1.0f);
    map.Update(std::nanf(""));
    map.Update(HUGE_VALF);
    CHECK(!map.Down(eA_JUMP));
    CHECK(!map.Pressed(eA_JUMP));
}

// 滚轮只有 Mouse1 脉冲，没有"按住"态。
TEST_CASE(inputMapWheelPulse) {
    resetEngineInput();
    cINPUT_MAP map;
    map.Bind(eACT_ZOOM_IN, INPUT_MOUSE_BASE + GM_TOP);
    GN::Mouse1[GM_TOP] = true;
    map.Update(1.0f / 60.0f);
    CHECK(map.Pressed(eACT_ZOOM_IN));
    CHECK(!map.Down(eACT_ZOOM_IN));
    GN::Mouse1[GM_TOP] = false;
    map.Update(1.0f / 60.0f);
    CHECK(!map.Pressed(eACT_ZOOM_IN));
}

TEST_CASE(inputMapDoubleTap) {
    resetEngineInput();
    cINPUT_MAP map;
    map.Bind(eA_JUMP, GK_SPACE);
    map.SetDoubleTapTime(0.3f);

    framePress(map, GK_SPACE, 0.05f);
    CHECK(!map.DoubleTapped(eA_JUMP));
    frameRelease(map, GK_SPACE, 0.05f);
    framePress(map, GK_SPACE, 0.05f); // 距上次按下 0.1 s < 0.3 s
    CHECK(map.DoubleTapped(eA_JUMP));
    frameRelease(map, GK_SPACE, 0.05f);
    framePress(map, GK_SPACE, 0.05f); // 第三下不再算双击
    CHECK(!map.DoubleTapped(eA_JUMP));
}

TEST_CASE(inputMapSeparatedPressIsNotDoubleTap) {
    resetEngineInput();
    cINPUT_MAP map;
    map.Bind(eA_JUMP, GK_SPACE);
    map.SetDoubleTapTime(0.3f);
    framePress(map, GK_SPACE, 0.05f);
    frameRelease(map, GK_SPACE, 0.5f); // 超过窗口
    framePress(map, GK_SPACE, 0.05f);
    CHECK(!map.DoubleTapped(eA_JUMP));
}

TEST_CASE(inputMapDisabledBlocksQueries) {
    resetEngineInput();
    cINPUT_MAP map;
    map.Bind(eA_JUMP, GK_SPACE);
    framePress(map, GK_SPACE);
    CHECK(map.Down(eA_JUMP));
    map.SetEnabled(false);
    CHECK(!map.Enabled());
    CHECK(!map.Down(eA_JUMP));
    CHECK(!map.Pressed(eA_JUMP));
    map.SetEnabled(true);
    frameHold(map, GK_SPACE);
    CHECK(map.Down(eA_JUMP));
}

TEST_CASE(inputMapClearResetsBindings) {
    resetEngineInput();
    cINPUT_MAP map;
    map.Bind(eA_JUMP, GK_SPACE);
    map.Bind(eA_FIRE, INPUT_MOUSE_BASE + GM_LEFT);
    framePress(map, GK_SPACE);
    map.Clear();
    CHECK_EQ(map.BindCount(eA_JUMP), 0);
    CHECK_EQ(map.BindCount(eA_FIRE), 0);
    frameHold(map, GK_SPACE);
    CHECK(!map.Down(eA_JUMP));
}

TEST_CASE(inputMapLoadDefaultWASD) {
    resetEngineInput();
    cINPUT_MAP map;
    map.LoadDefaultWASD();
    CHECK(map.BindCount(eACT_MOVE_FRONT) >= 1);
    CHECK(map.BindCount(eACT_MOVE_BACK) >= 1);
    CHECK(map.BindCount(eACT_MOVE_LEFT) >= 1);
    CHECK(map.BindCount(eACT_MOVE_RIGHT) >= 1);
    CHECK(map.BindCount(eACT_BOOST) >= 1);
    CHECK(map.BindCount(eACT_LOOK_LEFT) >= 2); // 方向键 + J
    CHECK(map.BindCount(eACT_LOOK_RIGHT) >= 2); // 方向键 + L
    CHECK(map.BindCount(eACT_ZOOM_IN) >= 1);
    CHECK(map.BindCount(eACT_ZOOM_OUT) >= 1);
}

TEST_CASE(inputMapCameraInputPacking) {
    resetEngineInput();
    cINPUT_MAP map;
    map.LoadDefaultWASD();
    GN::Key[GK_W] = true;
    GN::Key1[GK_W] = true;
    GN::Key[GK_D] = true;
    GN::Key[GK_SHIFT] = true;
    GN::Key[GK_RIGHT] = true;
    GN::Mouse1[GM_TOP] = true;
    map.Update(1.0f / 60.0f);

    const GN3d::sCAMERA_INPUT input = map.CameraInput(0.1f);
    CHECK_NEAR(input.aMoveFront, 1.0f, 1e-6);
    CHECK_NEAR(input.aMoveLeft, -1.0f, 1e-6);
    CHECK_NEAR(input.aMoveUp, 0.0f, 1e-6);
    CHECK(input.aBoost);
    CHECK_NEAR(input.aLookYaw, 0.1f, 1e-6);
    CHECK_NEAR(input.aZoom, 1.0f, 1e-6);
}
