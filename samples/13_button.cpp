// Test 13: Button Test (TODO)
#include "common/test_utils.h"
extern "C" {
GN_CALL void AppInit() {}
GN_CALL void AppUnInit() {}
GN_CALL void AppDisplay2D(unsigned int nTime) {
    (void)nTime;
    ClearScreen(0xff111133);
    GN_RunTextOut("Button Test - TODO", 0, Vcx()-60, Vcy()-20, 0,0, TEST_WHITE, GNg::eBLEND_NORMAL);
    GN_RunTextOut("coming soon...", 0, Vcx()-50, Vcy()+10, 0,0, TEST_YELLOW, GNg::eBLEND_NORMAL);
    DrawFPS();
}
GN_CALL void AppDisplay3D(unsigned int nTime) { (void)nTime; }
GN_CALL void AppRun(unsigned int nTime) { (void)nTime; }
}
