// Test 05: Input Test (TODO)
#include "common/test_utils.h"
GNg::cTEXT_ART InfoFont;
extern "C" {
GN_CALL void AppInit() { InfoFont.Init(nullptr); InfoFont.SetFreeTypeFont(16); }
GN_CALL void AppUnInit() { InfoFont.Release(); }
GN_CALL void AppDisplay2D(unsigned int nTime) {
    (void)nTime; ClearScreen(0xff111133);
    InfoFont.OutText("Input Test - TODO", 0, Vcx()-60, Vcy()-20, 0,0, TEST_WHITE);
    InfoFont.OutText("coming soon...", 0, Vcx()-50, Vcy()+10, 0,0, TEST_YELLOW);
    DrawFPS(InfoFont);
}
GN_CALL void AppDisplay3D(unsigned int nTime) { (void)nTime; }
GN_CALL void AppRun(unsigned int nTime) { (void)nTime; }
}
