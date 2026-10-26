// ╔══════════════════════════════════════════════════════════╗
// ║  GN_mini — GLFW + Vulkan 平台层                       ║
// ║  完整的键盘 / 鼠标 / 滚轮 / 文字输入控制               ║
// ╚══════════════════════════════════════════════════════════╝

#include <GN.h>
#include <GN_Vk.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

GLFWwindow *GT_MainWindow = nullptr;

// ═══════════════════════════════════════════════════════════
//  静态状态：追踪鼠标光标位置与逐帧累积增量
// ═══════════════════════════════════════════════════════════
static double GT_LastCursorX = 0.0;
static double GT_LastCursorY = 0.0;
static double GT_AccumDX = 0.0; // 本帧累积的水平增量
static double GT_AccumDY = 0.0; // 本帧累积的垂直增量
static bool GT_FirstCursor = true; // 首帧初始化标记

// ═══════════════════════════════════════════════════════════
//  IME 文字输入缓冲 (引擎通过 GF_ReSetInputState 消费者)
// ═══════════════════════════════════════════════════════════
static char GT_ImeBuffer[256] = {0};
static uint8 GT_ImeBufLen = 0;

// 引擎每帧调用此函数取走缓存的文字输入
static void GT_GetImeChar(char *ime_str, uint8 *ime_len) {
    if(GT_ImeBufLen > 0) {
        memcpy(ime_str, GT_ImeBuffer, GT_ImeBufLen);
        *ime_len = GT_ImeBufLen;
        GT_ImeBufLen = 0;
    } else {
        *ime_len = 0;
    }
}

// ═══════════════════════════════════════════════════════════
//  GLFW → GN 键码映射 (覆盖完整的键盘)
// ═══════════════════════════════════════════════════════════
static int GLFW2GN(int glfwKey) {
    // ── 字母 A-Z：GLFW 65-90 → GN GK_A(51)+offset ──
    if(glfwKey >= 65 && glfwKey <= 90) return 51 + (glfwKey - 65);

    // ── 主键盘数字 0-9：GLFW 48-57 → GN GK_0(41)+offset ──
    if(glfwKey >= 48 && glfwKey <= 57) return 41 + (glfwKey - 48);

    // ── 功能键 F1-F25：GLFW 290-314 → GN GK_F1(98) 起 ──
    if(glfwKey >= 290 && glfwKey <= 314) return 98 + (glfwKey - 290);

    // ── 小键盘 0-9：GLFW 320-329 → GN GK_NUMPAD0(81)+offset ──
    if(glfwKey >= 320 && glfwKey <= 329) return 81 + (glfwKey - 320);

    // ── 符号键 + 特殊键 ──
    switch(glfwKey) {
        // 导航/编辑
        case 256: return 20; // ESCAPE
        case 257: return 7; // ENTER
        case 258: return 5; // TAB
        case 259: return 4; // BACKSPACE
        case 260: return 38; // INSERT
        case 261: return 39; // DELETE
        case 268: return 29; // HOME
        case 269: return 28; // END
        case 266: return 26; // PAGE_UP
        case 267:
            return 27; // PAGE_DOWN

        // 方向键
        case 262: return 32; // RIGHT
        case 263: return 30; // LEFT
        case 264: return 33; // DOWN
        case 265:
            return 31; // UP

        // 空格
        case 32:
            return 25; // SPACE

        // 修饰键
        case 340: return 130; // LEFT_SHIFT  → GK_LSHIFT
        case 344: return 131; // RIGHT_SHIFT → GK_RSHIFT
        case 341: return 132; // LEFT_CTRL   → GK_LCONTROL
        case 345: return 133; // RIGHT_CTRL  → GK_RCONTROL
        case 342: return 134; // LEFT_ALT    → GK_LMENU
        case 346: return 135; // RIGHT_ALT   → GK_RMENU
        case 343: return 77; // LEFT_SUPER  → GK_LWIN
        case 347: return 78; // RIGHT_SUPER → GK_RWIN
        case 348:
            return 10; // MENU

        // 锁定键
        case 280: return 12; // CAPS_LOCK
        case 281: return 123; // SCROLL_LOCK
        case 282:
            return 122; // NUM_LOCK

        // 打印/暂停
        case 283: return 35; // PRINT_SCREEN
        case 284:
            return 11; // PAUSE

        // US 键盘符号 (基于 US 布局)
        case 44: return 138; // COMMA
        case 46: return 140; // PERIOD
        case 45: return 139; // MINUS
        case 61: return 124; // EQUAL (GK_OEM_NEC_EQUAL)
        case 59: return 136; // SEMICOLON (GK_OEM_1)
        case 39: return 146; // APOSTROPHE (GK_OEM_7)
        case 47: return 141; // SLASH (GK_OEM_2)
        case 96: return 142; // GRAVE_ACCENT (GK_OEM_3)
        case 91: return 143; // LEFT_BRACKET (GK_OEM_4)
        case 92: return 144; // BACKSLASH (GK_OEM_5)
        case 93:
            return 145; // RIGHT_BRACKET (GK_OEM_6)

        // 小键盘特殊键
        case 330: return 96; // KP_DECIMAL  → GK_DECIMAL
        case 331: return 97; // KP_DIVIDE   → GK_DIVIDE
        case 332: return 92; // KP_MULTIPLY → GK_MULTIPLY
        case 333: return 95; // KP_SUBTRACT → GK_SUBTRACT
        case 334: return 93; // KP_ADD      → GK_ADD
        case 335: return 91; // KP_ENTER    → GK_NUM_ENTER
        case 336: return 124; // KP_EQUAL    → GK_OEM_NEC_EQUAL

        default: return -1; // 未识别的键，不处理
    }
}

// ═══════════════════════════════════════════════════════════
//  键盘回调：通过 GF_SetKeyState 完整驱动引擎状态机
// ═══════════════════════════════════════════════════════════
static void KeyCallback(GLFWwindow *, int key, int, int action, int) {
    int gnKey = GLFW2GN(key);
    if(gnKey >= 0 && gnKey < GN_MAX_KEY_NUM) {
        GF_SetKeyState(gnKey, (action == GLFW_PRESS || action == GLFW_REPEAT));
    }
}

// ═══════════════════════════════════════════════════════════
//  鼠标按钮回调：通过 GF_SetMouseState 完整驱动引擎状态机
//  (修正 GLFW 按钮编号 → GN 鼠标键位映射)
// ═══════════════════════════════════════════════════════════
static void MouseButtonCallback(GLFWwindow *, int btn, int action, int) {
    int mk = -1;
    switch(btn) {
        case GLFW_MOUSE_BUTTON_LEFT: mk = GM_LEFT; break; // GLFW 0 → GN 0
        case GLFW_MOUSE_BUTTON_RIGHT: mk = GM_RIGHT; break; // GLFW 1 → GN 2
        case GLFW_MOUSE_BUTTON_MIDDLE: mk = GM_CENTRE; break; // GLFW 2 → GN 1
        default: mk = (btn < GN_MAX_MK_NUM) ? btn : -1; break;
    }
    if(mk >= 0 && mk < GN_MAX_MK_NUM) {
        GF_SetMouseState((uint8)mk, action == GLFW_PRESS,
                         (int)GT_LastCursorX, (int)GT_LastCursorY);
    }
}

// ═══════════════════════════════════════════════════════════
//  鼠标移动回调：逐帧累积增量
//  ★ 检测异常大跳变(>200px) → 自动重置基准, 防止模式切换时突变
// ═══════════════════════════════════════════════════════════
static void CursorPosCallback(GLFWwindow *, double x, double y) {
    if(GT_FirstCursor) {
        GT_LastCursorX = x;
        GT_LastCursorY = y;
        GT_FirstCursor = false;
        return;
    }
    double dx = x - GT_LastCursorX;
    double dy = y - GT_LastCursorY;
    // 异常大跳变: 光标模式切换 / GLFW 虚拟位置重置 → 重新锚定
    if(dx > 200.0 || dx < -200.0 || dy > 200.0 || dy < -200.0) {
        GT_LastCursorX = x;
        GT_LastCursorY = y;
        return; // 丢弃此帧的异常增量
    }
    GT_AccumDX += dx;
    GT_AccumDY += dy;
    GT_LastCursorX = x;
    GT_LastCursorY = y;
}

// ═══════════════════════════════════════════════════════════
//  滚轮回调 (新增)：映射到 GM_TOP / GM_BOTTOM
//  引擎会在下一帧通过 GF_ReSetInputState 自动清除单帧状态
// ═══════════════════════════════════════════════════════════
static void ScrollCallback(GLFWwindow *, double /*xoffset*/, double yoffset) {
    if(yoffset > 0.0) {
        GF_SetMouseState(GM_TOP, true, (int)GT_LastCursorX, (int)GT_LastCursorY);
    } else if(yoffset < 0.0) {
        GF_SetMouseState(GM_BOTTOM, true, (int)GT_LastCursorX, (int)GT_LastCursorY);
    }
}

// ═══════════════════════════════════════════════════════════
//  文字输入回调 (新增)：UTF-32 → UTF-8 转换后缓存
//  引擎在 GF_ReSetInputState 中通过 GT_GetImeChar 取走
// ═══════════════════════════════════════════════════════════
static void CharCallback(GLFWwindow *, unsigned int codepoint) {
    if(GT_ImeBufLen >= 252) return; // 防止溢出

    if(codepoint < 0x80) {
        GT_ImeBuffer[GT_ImeBufLen++] = (char)codepoint;
    } else if(codepoint < 0x800) {
        if(GT_ImeBufLen + 1 >= 255) return;
        GT_ImeBuffer[GT_ImeBufLen++] = (char)(0xC0 | (codepoint >> 6));
        GT_ImeBuffer[GT_ImeBufLen++] = (char)(0x80 | (codepoint & 0x3F));
    } else if(codepoint < 0x10000) {
        if(GT_ImeBufLen + 2 >= 255) return;
        GT_ImeBuffer[GT_ImeBufLen++] = (char)(0xE0 | (codepoint >> 12));
        GT_ImeBuffer[GT_ImeBufLen++] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
        GT_ImeBuffer[GT_ImeBufLen++] = (char)(0x80 | (codepoint & 0x3F));
    } else if(codepoint < 0x110000) {
        if(GT_ImeBufLen + 3 >= 255) return;
        GT_ImeBuffer[GT_ImeBufLen++] = (char)(0xF0 | (codepoint >> 18));
        GT_ImeBuffer[GT_ImeBufLen++] = (char)(0x80 | ((codepoint >> 12) & 0x3F));
        GT_ImeBuffer[GT_ImeBufLen++] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
        GT_ImeBuffer[GT_ImeBufLen++] = (char)(0x80 | (codepoint & 0x3F));
    }
}

// ═══════════════════════════════════════════════════════════
//  平台 API 实现
// ═══════════════════════════════════════════════════════════

GN_MAIN_API void GT_AppArgsSet(int *argc, char **argv) {
    (void)argc;
    (void)argv;
    if(!glfwInit()) {
        fprintf(stderr, "GLFW init failed\n");
        exit(1);
    }
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
}

GN_MAIN_API void GN_InitWindow(int **hwnd, const char *caption, short vw, short vh, bool fs, bool zoom, bool cursor) {
    fprintf(stderr, "[GN] GN_InitWindow: creating %dx%d window \"%s\"\n", vw, vh, caption ? caption : "");
    GT_MainWindow = glfwCreateWindow(vw, vh, caption ? caption : "GN_mini", nullptr, nullptr);
    fprintf(stderr, "[GN] Window created: %p\n", (void *)GT_MainWindow);
    if(hwnd) *hwnd = (int *)GT_MainWindow;
}

GN_MAIN_API void GT_InitAllAndRun(int *hwnd, short vw, short vh, short cb,
                                  short bl, short bt, short br, short bb, bool fs, bool zoom, bool db, uint8 ms) {
    fprintf(stderr, "[GN] GT_InitAllAndRun: vw=%d vh=%d win=%p\n", vw, vh, (void *)GT_MainWindow);
    if(!GT_MainWindow) {
        GT_MainWindow = glfwCreateWindow(vw, vh, "GN_mini", nullptr, nullptr);
        fprintf(stderr, "[GN] Created fallback window: %p\n", (void *)GT_MainWindow);
        if(!GT_MainWindow) return;
    }

    // 注册全套输入回调
    glfwSetKeyCallback(GT_MainWindow, KeyCallback);
    glfwSetMouseButtonCallback(GT_MainWindow, MouseButtonCallback);
    glfwSetCursorPosCallback(GT_MainWindow, CursorPosCallback);
    glfwSetScrollCallback(GT_MainWindow, ScrollCallback);
    glfwSetCharCallback(GT_MainWindow, CharCallback);

    GNg::GN_VkInit(GT_MainWindow, vw, vh);

    // 初始化输入子系统 (TouchZ/TouchX/TouchY/IO 映射键)
    GN_InitInput();

    // 标记图形设备已可用，防止 GF_FrameCycleBeginProc 重复创建窗口
    GT_SystemInited |= (1 << 2);
    GF_FrameCycleBeginProc();

    // ── 主循环 ──
    while(!glfwWindowShouldClose(GT_MainWindow)) {
        // ★ 通过引擎标准 API 重置每帧输入状态
        //    这会正确清零 Key0/Key1/Key11/Mouse0/Mouse1/Mouse2/MouseQ/MouseI
        //    同时处理 MouseM/MouseMKS/KeyDown/TouchM/触摸Z 状态
        //    以及输入录制回放、IME 文字取走、延误鼠标消化
        GF_ReSetInputState(GT_GetImeChar);

        glfwPollEvents();

        // ★ 每帧一次性提交累积的鼠标增量 (避免多事件覆盖 mouseMSX)
        if(GT_AccumDX != 0.0 || GT_AccumDY != 0.0) {
            unsigned short ks = 0;
            if(GN::Mouse[GM_LEFT]) ks |= GMM_LEFT;
            if(GN::Mouse[GM_CENTRE]) ks |= GMM_CENTRE;
            if(GN::Mouse[GM_RIGHT]) ks |= GMM_RIGHT;
            GF_SetMouseMoveState(ks,
                                 (int)(GN::MouseMX + GT_AccumDX),
                                 (int)(GN::MouseMY + GT_AccumDY));
            GT_AccumDX = 0.0;
            GT_AccumDY = 0.0;
        }

        static unsigned int fc = 0;
        unsigned int t = fc * 16;
        GF_FrameCycleProc1(t);
        GF_FrameCycleProc2(t);
        fc++;
    }

    GF_FrameCycleEndProc();
    GNg::GN_VkShutdown();
    glfwDestroyWindow(GT_MainWindow);
    GT_MainWindow = nullptr;
}

// ── 兼容旧 API 桩 ──
GN_MAIN_API void GN_GameLoop(bool) {}
GN_MAIN_API void GN_InitGraphics(void *hwnd, short vw, short vh, short cb, short, short, short, short, bool, bool, bool, uint8) {
    GNg::GN_VkInit(GT_MainWindow, vw, vh);
}
GN_MAIN_API void GN_UnInitGraphics() {
    GNg::GN_VkShutdown();
}
GN_MAIN_API void GN_GameBreakLoop() {
    glfwSetWindowShouldClose(GT_MainWindow, GLFW_TRUE);
}
GN_MAIN_API void GN_ScreenUpData() {}
GN_MAIN_API void GN_SetAppKey(long key) {
    (void)key;
}
GN_MAIN_API void GN_UnInitWindow(void *hwnd) {
    (void)hwnd;
}
GN_MAIN_API void GN_SetFullScreen(bool fs) {
    (void)fs;
}
GN_MAIN_API bool GN_GetFullScreen() {
    return false;
}
GN_MAIN_API bool GN_Has3DAccelerate() {
    return true;
}
GN_MAIN_API char *const GN_GetInterfaceInfoRender() {
    return (char *)"Vulkan";
}
GN_MAIN_API void (*GN_GameBreakLoopProc)(void) = nullptr;
