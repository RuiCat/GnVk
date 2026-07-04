// Test 04: FreeType 字体渲染 (中文/英文/Emoji)
#include "common/test_utils.h"

GNg::cTEXT_ART FontText;

extern "C" {
GN_CALL void AppInit() {
    FontText.Init(nullptr);
    FontText.SetFreeTypeFont(18);
}

GN_CALL void AppUnInit() {
    FontText.Release();
}

GN_CALL void AppDisplay2D(unsigned int nTime) {
    (void)nTime;
    ClearScreen(0xff111122);

    GN_RunTextOut("FreeType Font Test", 0, Vcx()-70, 20, 0,0, TEST_WHITE, GNg::eBLEND_NORMAL);

    int y = 60;

    // 英文
    FontText.OutText("Hello Vulkan! GN_mini Engine v2026", 0, 20, y, 0,0, TEST_YELLOW, GNg::eBLEND_NORMAL);
    y += 30;

    // 中文
    FontText.OutText("你好，Vulkan！轻量级游戏引擎", 0, 20, y, 0,0, TEST_GREEN, GNg::eBLEND_NORMAL);
    y += 30;

    // 混排
    FontText.OutText("中文 English 混排 Mix Test 12345", 0, 20, y, 0,0, TEST_CYAN, GNg::eBLEND_NORMAL);
    y += 30;

    // 不同颜色
    FontText.OutText("Red Green Blue Yellow White", 0, 20, y, 0,0, TEST_RED, GNg::eBLEND_NORMAL);
    y += 30;
    FontText.OutText("ABCDEFGHIJKLMNOPQRSTUVWXYZ", 0, 20, y, 0,0, 0xff88ff88, GNg::eBLEND_NORMAL);
    y += 30;
    FontText.OutText("abcdefghijklmnopqrstuvwxyz", 0, 20, y, 0,0, 0xff8888ff, GNg::eBLEND_NORMAL);

    // 显示字体信息
    char info[128];
    sprintf(info, "Font Size: 18px  |  Renderer: FreeType");
    GN_RunTextOut(info, 0, 20, Vh()-40, 0,0, TEST_GRAY, GNg::eBLEND_NORMAL);

    DrawFPS();
}

GN_CALL void AppDisplay3D(unsigned int nTime) { (void)nTime; }
GN_CALL void AppRun(unsigned int nTime) { (void)nTime; }
}
