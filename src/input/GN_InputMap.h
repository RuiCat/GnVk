#pragma once
#ifndef _GN_INPUTMAP_H_
#define _GN_INPUTMAP_H_
// GN_InputMap.h — 动作映射契约：把「键位」和「游戏语义」分开。
// 现状痛点：游戏代码直接读 GN::Key1[GK_JUMP]，改键要翻遍逻辑；"本帧刚按下"的边沿
// 要在每个使用点自己存上一帧快照。本类把这两件事收拢到一处。
//
// 用法（每帧一次，且必须在任何动作查询之前）：
//   gInput.Update(dt);                       // 采样 + 算边沿
//   if (gInput.Pressed(eACT_JUMP)) Jump();   // 边沿只在 Update 后的这一帧为真
//   float fwd = gInput.Axis1D(eACT_MOVE_BACK, eACT_MOVE_FRONT);
//
// 依赖：**必须先包含 GN.h**——GN_Input.h 用到 GN 的基础类型（uint8/P<>），单独包含本头编不过。
// 与 GN_InputAct.h / GN_RunRec.h 等旧头同样的前置条件。
#include "GN_Input.h" // GK_*/GM_* 与 GN_MAX_KEY_NUM/GN_MAX_MK_NUM
#include "GN_2Dmath.h" // GNg::sVEC2
#include "GN_3Dcamera.h" // GN3d::sCAMERA_INPUT

#include <cstdint>

namespace GN {

// 动作编号：uint16_t 而不是枚举——引擎不预设游戏有哪些动作（否则引擎要随每个游戏改）。
typedef uint16_t eACTION;

const uint16_t INPUT_KEY_NONE = 0xFFFF; // Unbind 的"全部解绑"
const uint16_t INPUT_MOUSE_BASE = 0x200; // 统一键位空间里鼠标键的基址：INPUT_MOUSE_BASE + GM_*
const int INPUT_MAX_BIND_PER_ACTION = 4; // 一个动作最多绑几个键（键盘 + 鼠标）
const uint16_t INPUT_ACTION_MAX = 256; // 动作编号上限（0 保留为"无动作"）
const uint16_t INPUT_ACT_USER_FIRST = 64; // 游戏自定义动作请从这里往上编

// 引擎自带的便利动作（只有 LoadDefaultWASD/CameraInput 用；游戏可无视）
enum eACTION_DEFAULT : uint16_t {
    eACT_NONE = 0,
    eACT_MOVE_FRONT = 1,
    eACT_MOVE_BACK = 2,
    eACT_MOVE_LEFT = 3,
    eACT_MOVE_RIGHT = 4,
    eACT_MOVE_UP = 5,
    eACT_MOVE_DOWN = 6,
    eACT_BOOST = 7,
    eACT_LOOK_LEFT = 8,
    eACT_LOOK_RIGHT = 9,
    eACT_LOOK_UP = 10,
    eACT_LOOK_DOWN = 11,
    eACT_ZOOM_IN = 12,
    eACT_ZOOM_OUT = 13,
    eACT_ORBIT_TOGGLE = 14,
    eACT_RESET_VIEW = 15
};

class cINPUT_MAP {
  public:
    cINPUT_MAP();

    // 清空全部绑定与本帧状态（不动 enabled/doubleTapTime）
    void Clear();

    // 绑定：action 与 key 都必须在合法区间内、且该动作的绑定槽没满；返回是否成功。
    // key：键盘用 GK_*，鼠标用 INPUT_MOUSE_BASE + GM_*。
    bool Bind(eACTION action, uint16_t key);
    // 解绑：key 省略表示解绑该动作的全部键；返回是否真的删掉了绑定。
    bool Unbind(eACTION action, uint16_t key = INPUT_KEY_NONE);
    // 便利函数：WASD + QE 升降 + Shift 加速 + 方向键/J L 转视角 + 滚轮缩放（不强制使用）
    void LoadDefaultWASD();

    // 每帧开头调一次：采样全局输入并算边沿。重复调用会让边沿丢帧，不要在一帧里调两次。
    void Update(float dt);

    bool Down(eACTION action) const; // 持续按住
    bool Pressed(eACTION action) const; // 本帧刚按下（边沿）
    bool Released(eACTION action) const; // 本帧刚松开（边沿）
    bool DoubleTapped(eACTION action) const; // 双击间隔内再次按下
    float Axis1D(eACTION neg, eACTION pos) const; // -1/0/+1（对向同时按住 = 0）
    GNg::sVEC2 Axis2D(eACTION negX, eACTION posX, eACTION negY, eACTION posY) const;

    // 把方向/视角/滚轮动作打包成相机意图。lookScale = 本次调用要用的弧度增量
    // （例如 转视角速度 rad/s × dt）；滚轮每格 aZoom = ±1（调用方再乘自己的步长）。
    GN3d::sCAMERA_INPUT CameraInput(float lookScale = 1.0f) const;

    // 全局屏蔽（UI 抢输入）：屏蔽后所有查询恒 false，但 Update 仍照常采样。
    void SetEnabled(bool on);
    bool Enabled() const;

    // 双击判定窗口（秒）。<=0 视为关闭双击判定。
    void SetDoubleTapTime(float sec);
    float DoubleTapTime() const;

    // 调试/测试用：该动作绑了几个键、第 index 个键是什么
    int BindCount(eACTION action) const;
    uint16_t KeyOf(eACTION action, int index) const;

  private:
    // 键位 → 本帧三态（采样结果，动作查询只是它的聚合）
    struct sKEY_STATE {
        bool aHold; // 引擎"按住"（GN::Key / GN::Mouse）
        bool aPressed; // 本帧按下边沿
        bool aReleased; // 本帧松开边沿
        bool aPrev; // 上一帧的"按住"（算边沿用）
    };
    static bool ValidAction(eACTION action);
    static bool ValidKey(uint16_t key);
    void SampleKeys();

    sKEY_STATE aKeyState[GN_MAX_KEY_NUM]; // 键盘
    sKEY_STATE aMouseState[GN_MAX_MK_NUM]; // 鼠标
    uint16_t aBind[INPUT_ACTION_MAX][INPUT_MAX_BIND_PER_ACTION]; // 动作 → 键位表
    uint8_t aBindCount[INPUT_ACTION_MAX]; // 每个动作已绑几个
    uint8_t aDown[INPUT_ACTION_MAX]; // 聚合后的按住
    uint8_t aPressed[INPUT_ACTION_MAX]; // 聚合后的按下边沿
    uint8_t aReleased[INPUT_ACTION_MAX]; // 聚合后的松开边沿
    uint8_t aDoubleTap[INPUT_ACTION_MAX]; // 聚合后的双击
    float aLastPressTime[INPUT_ACTION_MAX]; // 上次按下时刻（双击判定）
    float aClock; // Update 累计时间
    float aDoubleTapTime; // 双击窗口（秒）
    bool aEnabled; // 全局屏蔽
};

}; // end namespace GN
#endif //_GN_INPUTMAP_H_
