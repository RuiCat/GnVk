#pragma once
// GN_mini 测试程序公共工具

#include <GN.h>

// ── 初始化 (在 AppInit 中调用) ────────────────
inline void TestInit() {}

// ── 颜色常量 ──────────────────────────────
#define TEST_RED    0xffff4444
#define TEST_GREEN  0xff44ff44
#define TEST_BLUE   0xff4444ff
#define TEST_WHITE  0xffffffff
#define TEST_BLACK  0xff000000
#define TEST_GRAY   0xff777777
#define TEST_YELLOW 0xffffff44
#define TEST_CYAN   0xff44ffff

// ── 视口辅助 ──────────────────────────────
inline int Vw() { return GNg::ViewCur->ortho_right  - GNg::ViewCur->ortho_left; }
inline int Vh() { return GNg::ViewCur->ortho_bottom - GNg::ViewCur->ortho_top; }
inline int Vcx(){ return (Vw() / 2); }
inline int Vcy(){ return (Vh() / 2); }

// ── 简单清屏 ──────────────────────────────
inline void ClearScreen(unsigned int color = TEST_GRAY) {
    GNg::ImageDraw(0, 0, 0, Vw(), Vh(), color);
}

// ── FPS 显示 (使用 cTEXT_ART) ──────────────
inline void DrawFPS(GNg::cTEXT_ART &font, int x = 4, int y = 4) {
    static char buf[32];
    static unsigned int lastFps = 0;
    unsigned int fps = GN::TimeGetRunFpsCount();
    if(fps != lastFps) {
        lastFps = fps;
        sprintf(buf, "FPS:%d", fps % 60);
    }
    font.OutText(buf, 0, x, y, 0, 0, TEST_YELLOW);
}

// ── 简单断言 (运行日志) ────────────────────
#define TEST_ASSERT(cond, msg) do { \
    if(!(cond)) GN_Log("[FAIL] %s:%d %s\n", __FILE__, __LINE__, msg); \
} while(0)
