#ifndef _GN_MAIN_H_
#define _GN_MAIN_H_

// ╔══════════════════════════════════════════════════════════╗
// ║  GN_mini — GLFW + Vulkan 平台层                        ║
// ║  替代旧的 GLUT + OpenGL 实现                            ║
// ╚══════════════════════════════════════════════════════════╝

#ifdef GN_MAIN_USING_SOURCE_CODE
#define GN_MAIN_API
#define GN_MAIN_CALL
#elif defined GN_MAIN_USING_LIB_EXPORTS
#ifdef _WIN32_
#define GN_MAIN_API __declspec(dllexport)
#define GN_MAIN_CALL __declspec(dllimport)
#else
#define GN_MAIN_API extern
#define GN_MAIN_CALL extern
#endif
#else
#ifdef _WIN32_
#define GN_MAIN_API __declspec(dllimport)
#define GN_MAIN_CALL __declspec(dllexport)
#else
#define GN_MAIN_API
#define GN_MAIN_CALL
#endif
#endif

// ── GLFW + Vulkan 的 GN_MAIN 宏 ───────────────────────────
#include <GLFW/glfw3.h>

#define GN_MAIN \
    void GT_GLFW_main() {

#define GN_EXIT                       \
    }                                 \
    int main(int argc, char **argv) { \
        GT_AppArgsSet(&argc, argv);   \
        GT_GLFW_main();               \
        return 0;                     \
    }

GN_MAIN_API void GT_AppArgsSet(int *argc, char **argv);

// 窗口尺寸
extern GN_MAIN_API GLFWwindow *GT_MainWindow;

// ── 兼容旧 API (替代 GLUT 版本中的函数) ──────────────────
GN_MAIN_API void GN_InitWindow(int **hwnd = 0, const char *caption = 0, short vw = 800, short vh = 600, bool fs = false, bool zoom = true, bool cursor = true);
GN_MAIN_API void GN_UnInitWindow(void *hwnd);
GN_MAIN_API void GN_InitGraphics(void *hwnd = 0, short vw = 800, short vh = 600, short cb = 0, short bl = 0, short bt = 0, short br = 0, short bb = 0, bool fs = false, bool zoom = true, bool db = false, uint8 ms = 0);
GN_MAIN_API void GN_UnInitGraphics();
GN_MAIN_API void GN_GameLoop(bool b);
GN_MAIN_API void GN_GameBreakLoop();
GN_MAIN_API void GN_ScreenUpData();
GN_MAIN_API void GN_SetAppKey(long key);
extern GN_MAIN_API void (*GN_GameBreakLoopProc)(void);
GN_MAIN_API void RenderFinish(bool only3d = false);
GN_MAIN_API bool GN_Has3DAccelerate();
GN_MAIN_API char *const GN_GetInterfaceInfoRender();
GN_MAIN_API void GN_SetFullScreen(bool fs);
GN_MAIN_API bool GN_GetFullScreen();
GN_MAIN_API void GT_InitAllAndRun(int *hwnd, short vw, short vh, short cb,
                                  short bl, short bt, short br, short bb, bool fs, bool zoom, bool db, uint8 ms);

inline void GN_SetCursorPos(int x, int y, bool raw = false) {
    if(GT_MainWindow) glfwSetCursorPos(GT_MainWindow, (double)x, (double)y);
    (void)raw;
}
inline void GN_SetWindowPos(int x, int y, int w, int h) {
    if(GT_MainWindow) {
        if(w > 0 && h > 0) glfwSetWindowSize(GT_MainWindow, w, h);
        if(x != 0 || y != 0) glfwSetWindowPos(GT_MainWindow, x, y);
        // 注意: 不在pass内调用GN_VkResize (会销毁swapchain导致崩溃)
        // 窗口resize由GLFW回调自动触发swapchain重建
    }
}
inline void GN_SetImeFouscePos(int x, int y) {
    (void)x;
    (void)y;
}
inline void *GN_GetSoftKeyboardRange() {
    static int r[4] = {0};
    return r;
}

#endif // _GN_MAIN_H_

namespace GN {
inline unsigned char GetTouchMaxNum() {
    return 1;
}
} // namespace GN

inline void GT_ScissorRectSet(int x, int y, int w, int h) {}
inline void GT_ScissorEnable(bool en) {}
inline void *GT_CopyToDataFromFrameBuffer(int *sz = 0, short vx = 0, short vy = 0, short vw = 0, short vh = 0, bool ct = false) {
    (void)vx;
    (void)vy;
    (void)vw;
    (void)vh;
    (void)ct;
    return nullptr;
}
inline void GT_UnInitG2D() {}
inline void GT_ReSetRenderView2D(short vx = 0, short vy = 0, short vw = 0, short vh = 0, short bl = 0, short bt = 0) {
    (void)vx;
    (void)vy;
    (void)vw;
    (void)vh;
}

inline void GT_ReSetRenderState2D() {}

inline void GT_InitG2D(void *hwnd = 0, short vw = 0, short vh = 0, short cb = 0) {}

inline void *GT_CopyToImageFromFrameBuffer(GNg::sIMAGE *img, short vx = 0, short vy = 0, short vw = 0, short vh = 0, int fmt = 0) {
    return nullptr;
}
