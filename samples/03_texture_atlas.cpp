// Test 03: 9宫格纹理 + 图集子区域
#include "common/test_utils.h"

GNg::sIMAGE BubbleImg;
GNg::cTEXT_ART InfoFont;

extern "C" {
GN_CALL void AppInit() {
    InfoFont.SetFreeTypeFont(16);
    BubbleImg.InitToData(GN_SysDir("ui/bubbleB"), 9, GNg::CR_ALPHA);
}

GN_CALL void AppUnInit() {
    InfoFont.Release();
    BubbleImg.Release();
}

GN_CALL void AppDisplay2D(unsigned int nTime) {
    (void)nTime;
    ClearScreen(0xff111122);

    InfoFont.OutText("Texture Atlas & 9-Slice", 0, Vcx()-80, 20, 0,0, TEST_WHITE);

    GNg::ImageDraw9Square(&BubbleImg, Vcx()-100, Vcy()-80, 200, 40, 0x88ffffff);
    GNg::ImageDraw9Square(&BubbleImg, Vcx()-50,  Vcy()-20, 100, 80, 0x88ff4444);
    GNg::ImageDraw9Square(&BubbleImg, Vcx()-80,  Vcy()+60, 160, 30, 0x8844ff44);

    for(int i = 0; i < 9; i++) {
        int dx = 30 + (i % 3) * 50;
        int dy = 100 + (i / 3) * 50;
        GNg::ImageDraw(&BubbleImg, dx, dy, 40, 40, TEST_WHITE, (unsigned short)i);
    }

    DrawFPS(InfoFont);
}

GN_CALL void AppDisplay3D(unsigned int nTime) { (void)nTime; }
GN_CALL void AppRun(unsigned int nTime) { (void)nTime; }
}
