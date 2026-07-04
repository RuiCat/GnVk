// Test 01: Hello Vulkan — 窗口+文字+矩形
#include "common/test_utils.h"

extern "C" {
GN_CALL void AppInit() {}
GN_CALL void AppUnInit() {}

GN_CALL void AppDisplay2D(unsigned int nTime) {
    (void)nTime;
    ClearScreen(0xff222233);

    // 标题
    GN_RunTextOut("Hello Vulkan! GN_mini Engine", 0,
        Vcx() - 130, Vcy() - 60, 0,0, TEST_WHITE, GNg::eBLEND_NORMAL);

    // 彩色矩形
    GNg::ImageDraw(0, Vcx()-120, Vcy()-20, 60,40, TEST_RED);
    GNg::ImageDraw(0, Vcx()-50,  Vcy()-30, 60,50, TEST_GREEN);
    GNg::ImageDraw(0, Vcx()+20,  Vcy()-40, 60,60, TEST_BLUE);
    GNg::ImageDraw(0, Vcx()+90,  Vcy()-30, 60,50, TEST_YELLOW, GNg::eBLEND_NORMAL);

    // 边框
    GNg::LineFrame2dDraw(Vcx()-125, Vcy()-45, 260, 70, 1, TEST_WHITE, GNg::eBLEND_NORMAL);

    // FPS
    DrawFPS();
}

GN_CALL void AppDisplay3D(unsigned int nTime) { (void)nTime; }
GN_CALL void AppRun(unsigned int nTime) { (void)nTime; }
}
