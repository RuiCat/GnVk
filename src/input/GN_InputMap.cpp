// GN_InputMap.cpp — 动作映射实现。
// 语义（每条都有对应用例，见 tests/unit/testInputMap.cpp）：
//   1. 键位与语义分离：游戏只认 eACTION，改键只改 Bind；
//   2. 边沿（Pressed/Released）只在 Update 里算一次，不在每个使用点重复；
//   3. 按下边沿 = 按住态由松到按，或引擎本帧的单次键（Key1/Mouse1）——
//      后者覆盖"一帧内按下又松开"与滚轮这类瞬时脉冲；
//   4. 一个动作可绑多个键，任一键按下即动作成立（键盘 + 鼠标混绑即可）。

#include "GN.h"
#include "GN_InputMap.h"

#include <cmath>

namespace GN {

cINPUT_MAP::cINPUT_MAP()
    : aClock(0.0f), aDoubleTapTime(0.35f), aEnabled(true) {
    for (uint16_t act = 0; act < INPUT_ACTION_MAX; ++act) {
        aBindCount[act] = 0;
        aDown[act] = 0;
        aPressed[act] = 0;
        aReleased[act] = 0;
        aDoubleTap[act] = 0;
        aLastPressTime[act] = -1.0f;
        for (int i = 0; i < INPUT_MAX_BIND_PER_ACTION; ++i) {
            aBind[act][i] = INPUT_KEY_NONE;
        }
    }
    for (int k = 0; k < GN_MAX_KEY_NUM; ++k) {
        aKeyState[k].aHold = false;
        aKeyState[k].aPressed = false;
        aKeyState[k].aReleased = false;
        aKeyState[k].aPrev = false;
    }
    for (int m = 0; m < GN_MAX_MK_NUM; ++m) {
        aMouseState[m].aHold = false;
        aMouseState[m].aPressed = false;
        aMouseState[m].aReleased = false;
        aMouseState[m].aPrev = false;
    }
}

bool cINPUT_MAP::ValidAction(eACTION action) {
    return action > eACT_NONE && action < INPUT_ACTION_MAX;
}

bool cINPUT_MAP::ValidKey(uint16_t key) {
    if (key < GN_MAX_KEY_NUM) {
        return true; // 键盘
    }
    if (key >= INPUT_MOUSE_BASE && key < INPUT_MOUSE_BASE + GN_MAX_MK_NUM) {
        return true; // 鼠标
    }
    return false;
}

void cINPUT_MAP::Clear() {
    for (uint16_t act = 0; act < INPUT_ACTION_MAX; ++act) {
        aBindCount[act] = 0;
        aDown[act] = 0;
        aPressed[act] = 0;
        aReleased[act] = 0;
        aDoubleTap[act] = 0;
        aLastPressTime[act] = -1.0f;
    }
}

bool cINPUT_MAP::Bind(eACTION action, uint16_t key) {
    if (!ValidAction(action) || !ValidKey(key)) {
        return false;
    }
    for (int i = 0; i < aBindCount[action]; ++i) {
        if (aBind[action][i] == key) {
            return true; // 重复绑定：幂等
        }
    }
    if (aBindCount[action] >= INPUT_MAX_BIND_PER_ACTION) {
        return false; // 槽满
    }
    aBind[action][aBindCount[action]] = key;
    ++aBindCount[action];
    return true;
}

bool cINPUT_MAP::Unbind(eACTION action, uint16_t key) {
    if (!ValidAction(action)) {
        return false;
    }
    if (key == INPUT_KEY_NONE) {
        const bool had = (aBindCount[action] > 0);
        aBindCount[action] = 0;
        aDown[action] = aPressed[action] = aReleased[action] = aDoubleTap[action] = 0;
        aLastPressTime[action] = -1.0f;
        return had;
    }
    for (int i = 0; i < aBindCount[action]; ++i) {
        if (aBind[action][i] != key) {
            continue;
        }
        for (int j = i; j + 1 < aBindCount[action]; ++j) {
            aBind[action][j] = aBind[action][j + 1];
        }
        --aBindCount[action];
        aBind[action][aBindCount[action]] = INPUT_KEY_NONE;
        aDown[action] = aPressed[action] = aReleased[action] = aDoubleTap[action] = 0;
        aLastPressTime[action] = -1.0f;
        return true;
    }
    return false;
}

void cINPUT_MAP::LoadDefaultWASD() {
    Bind(eACT_MOVE_FRONT, GK_W);
    Bind(eACT_MOVE_BACK, GK_S);
    Bind(eACT_MOVE_LEFT, GK_A);
    Bind(eACT_MOVE_RIGHT, GK_D);
    Bind(eACT_MOVE_UP, GK_E);
    Bind(eACT_MOVE_DOWN, GK_Q);
    Bind(eACT_BOOST, GK_SHIFT);
    Bind(eACT_LOOK_LEFT, GK_LEFT);
    Bind(eACT_LOOK_LEFT, GK_J);
    Bind(eACT_LOOK_RIGHT, GK_RIGHT);
    Bind(eACT_LOOK_RIGHT, GK_L);
    Bind(eACT_LOOK_UP, GK_UP);
    Bind(eACT_LOOK_DOWN, GK_DOWN);
    Bind(eACT_ZOOM_IN, INPUT_MOUSE_BASE + GM_TOP);
    Bind(eACT_ZOOM_OUT, INPUT_MOUSE_BASE + GM_BOTTOM);
    Bind(eACT_ORBIT_TOGGLE, GK_F);
    Bind(eACT_RESET_VIEW, GK_R);
}

void cINPUT_MAP::SampleKeys() {
    // 键盘：按住看 Key，按下态取"由松到按"或引擎本帧单次键，松开态取"由按到松"或 Key0。
    for (int k = 0; k < GN_MAX_KEY_NUM; ++k) {
        sKEY_STATE &st = aKeyState[k];
        const bool hold = (GN::Key[k] != 0);
        st.aHold = hold;
        st.aPressed = (hold && !st.aPrev) || (GN::Key1[k] != 0);
        st.aReleased = (!hold && st.aPrev) || (GN::Key0[k] != 0);
        st.aPrev = hold;
    }
    // 鼠标：与键盘同一套边沿语义。滚轮（GM_TOP/GM_BOTTOM）只有 Mouse1 脉冲，没有"按住"态。
    for (int m = 0; m < GN_MAX_MK_NUM; ++m) {
        sKEY_STATE &st = aMouseState[m];
        const bool hold = (GN::Mouse[m] != 0);
        st.aHold = hold;
        st.aPressed = (hold && !st.aPrev) || (GN::Mouse1[m] != 0);
        st.aReleased = (!hold && st.aPrev) || (GN::Mouse0[m] != 0);
        st.aPrev = hold;
    }
}

void cINPUT_MAP::Update(float dt) {
    if (!(dt > 0.0f) || !std::isfinite(dt)) {
        dt = 0.0f;
    }
    aClock += dt;
    SampleKeys();

    for (uint16_t act = 1; act < INPUT_ACTION_MAX; ++act) {
        bool down = false;
        bool pressed = false;
        bool released = false;
        const int count = aBindCount[act];
        for (int i = 0; i < count; ++i) {
            const uint16_t key = aBind[act][i];
            const sKEY_STATE &st = (key >= INPUT_MOUSE_BASE)
                                       ? aMouseState[key - INPUT_MOUSE_BASE]
                                       : aKeyState[key];
            down = down || st.aHold;
            pressed = pressed || st.aPressed;
            released = released || st.aReleased;
        }
        aDown[act] = down ? 1 : 0;
        aPressed[act] = pressed ? 1 : 0;
        aReleased[act] = released ? 1 : 0;

        aDoubleTap[act] = 0;
        if (pressed) {
            const float last = aLastPressTime[act];
            if (aDoubleTapTime > 0.0f && last >= 0.0f && (aClock - last) <= aDoubleTapTime) {
                aDoubleTap[act] = 1;
                aLastPressTime[act] = -1.0f; // 消费掉，三连击不会算两次双击
            } else {
                aLastPressTime[act] = aClock;
            }
        }
    }
}

bool cINPUT_MAP::Down(eACTION action) const {
    if (!aEnabled || !ValidAction(action)) {
        return false;
    }
    return aDown[action] != 0;
}

bool cINPUT_MAP::Pressed(eACTION action) const {
    if (!aEnabled || !ValidAction(action)) {
        return false;
    }
    return aPressed[action] != 0;
}

bool cINPUT_MAP::Released(eACTION action) const {
    if (!aEnabled || !ValidAction(action)) {
        return false;
    }
    return aReleased[action] != 0;
}

bool cINPUT_MAP::DoubleTapped(eACTION action) const {
    if (!aEnabled || !ValidAction(action)) {
        return false;
    }
    return aDoubleTap[action] != 0;
}

float cINPUT_MAP::Axis1D(eACTION neg, eACTION pos) const {
    const bool p = Down(pos);
    const bool n = Down(neg);
    if (p == n) {
        return 0.0f; // 都没按 或 对向同时按住（抵消）
    }
    return p ? 1.0f : -1.0f;
}

GNg::sVEC2 cINPUT_MAP::Axis2D(eACTION negX, eACTION posX, eACTION negY,
                              eACTION posY) const {
    return GNg::sVEC2(Axis1D(negX, posX), Axis1D(negY, posY));
}

GN3d::sCAMERA_INPUT cINPUT_MAP::CameraInput(float lookScale) const {
    GN3d::sCAMERA_INPUT input;
    input.aMoveFront = Axis1D(eACT_MOVE_BACK, eACT_MOVE_FRONT);
    input.aMoveLeft = Axis1D(eACT_MOVE_RIGHT, eACT_MOVE_LEFT);
    input.aMoveUp = Axis1D(eACT_MOVE_DOWN, eACT_MOVE_UP);
    input.aBoost = Down(eACT_BOOST);
    input.aLookYaw = Axis1D(eACT_LOOK_LEFT, eACT_LOOK_RIGHT) * lookScale;
    input.aLookPitch = Axis1D(eACT_LOOK_DOWN, eACT_LOOK_UP) * lookScale;
    if (Pressed(eACT_ZOOM_IN)) {
        input.aZoom += 1.0f;
    }
    if (Pressed(eACT_ZOOM_OUT)) {
        input.aZoom -= 1.0f;
    }
    return input;
}

void cINPUT_MAP::SetEnabled(bool on) { aEnabled = on; }

bool cINPUT_MAP::Enabled() const { return aEnabled; }

void cINPUT_MAP::SetDoubleTapTime(float sec) {
    if (!std::isfinite(sec) || sec < 0.0f) {
        return;
    }
    aDoubleTapTime = sec;
}

float cINPUT_MAP::DoubleTapTime() const { return aDoubleTapTime; }

int cINPUT_MAP::BindCount(eACTION action) const {
    if (!ValidAction(action)) {
        return 0;
    }
    return aBindCount[action];
}

uint16_t cINPUT_MAP::KeyOf(eACTION action, int index) const {
    if (!ValidAction(action) || index < 0 || index >= aBindCount[action]) {
        return INPUT_KEY_NONE;
    }
    return aBind[action][index];
}

}; // end namespace GN
