// 程序化图像生成单测：矩形渐变与棋盘格（纯内存，无 GPU）。
#include "testFramework.h"
#include <GN.h>

TEST_CASE(imageGenRectGradient) {
    uint8 pixels[2] = {0, 0};
    GNg::GenRectDataToMem(pixels, 2, 1, 0.0f, 0.0f, 1.0f, 0.0f);
    CHECK_EQ((int)pixels[0], 255);
    CHECK_EQ((int)pixels[1], 0);
}

TEST_CASE(imageGenCheckerboard) {
    uint8 pixels[16] = {0};
    GNg::GenCheckerboardDataToMem(pixels, 2, 2, 2, 100);

    CHECK_EQ((int)pixels[0], 100);
    CHECK_EQ((int)pixels[1], 100);
    CHECK_EQ((int)pixels[2], 255);
    CHECK_EQ((int)pixels[3], 255);
    CHECK_EQ((int)pixels[8], 255);
    CHECK_EQ((int)pixels[9], 255);
    CHECK_EQ((int)pixels[10], 100);
    CHECK_EQ((int)pixels[11], 100);
}
