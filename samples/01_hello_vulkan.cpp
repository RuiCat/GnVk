// Test 01: Hello Vulkan — 窗口+文字+矩形
#include "common/test_utils.h"

GNg::cTEXT_ART TitleFont, InfoFont;

extern "C" {
GN_CALL void AppInit() {
    TitleFont.SetFreeTypeFont(24);
    InfoFont.SetFreeTypeFont(16);
}

GN_CALL void AppUnInit() {
    TitleFont.Release();
    InfoFont.Release();
}

GN_CALL void AppDisplay2D(unsigned int nTime) {
    (void)nTime;
    ClearScreen(0xff222233);

    TitleFont.OutText("Hello Vulkan!", 0, Vcx()-70, Vcy()-60, 0,0, TEST_WHITE);
    InfoFont.OutText("GN_mini Engine v2026", 0, Vcx()-80, Vcy()-30, 0,0, TEST_YELLOW);

    GNg::ImageDraw(0, Vcx()-120, Vcy()+20, 60,40, TEST_RED);
    GNg::ImageDraw(0, Vcx()-50,  Vcy()+10, 55,50, TEST_GREEN);
    GNg::ImageDraw(0, Vcx()+15,  Vcy(),   55,60, TEST_BLUE);
    GNg::ImageDraw(0, Vcx()+80,  Vcy()+10, 55,50, TEST_CYAN);
    GNg::LineFrame2dDraw(Vcx()-125, Vcy()-10, 265, 80, 1, TEST_WHITE);

    DrawFPS(InfoFont);
}

GN_CALL void AppDisplay3D(unsigned int nTime) { (void)nTime; }
GN_CALL void AppRun(unsigned int nTime) { (void)nTime; }
}
