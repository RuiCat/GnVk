// Test 02: 纹理加载与绘制 (PNG 单帧)
#include "common/test_utils.h"

GNg::sIMAGE TexImg;

extern "C" {
GN_CALL void AppInit() {
    // 加载测试纹理 (需 assets/ui/ 目录下的引擎默认纹理)
    TexImg.InitToData(GN_SysDir("ui/bubbleB"), 1);
}

GN_CALL void AppUnInit() {
    TexImg.Release();
}

GN_CALL void AppDisplay2D(unsigned int nTime) {
    (void)nTime;
    ClearScreen(0xff111122);

    // 标题
    GN_RunTextOut("Texture2D Test", 0, Vcx()-60, 20, 0,0, TEST_WHITE, GNg::eBLEND_NORMAL);

    int x = Vcx() - 60, y = Vcy() - 40;
    int w = 120, h = 80;

    // 正常绘制
    GNg::ImageDraw(&TexImg, x, y, w, h, TEST_WHITE, GNg::eBLEND_NORMAL);

    // 半透明
    GNg::ImageDraw(&TexImg, x-80, y-20, w/2, h/2, 0x88ffffff);

    // 高亮叠加
    GNg::ImageDraw(&TexImg, x+100, y+10, w, h, 0xffff4444, 0, GNg::eBLEND_BRIGHT);

    // 边框
    GNg::LineFrame2dDraw(x, y, w, h, 1, TEST_WHITE, GNg::eBLEND_NORMAL);

    DrawFPS();
}

GN_CALL void AppDisplay3D(unsigned int nTime) { (void)nTime; }
GN_CALL void AppRun(unsigned int nTime) { (void)nTime; }
}
