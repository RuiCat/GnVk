// 颜色与像素格式单测：sCOLOR 通道、格式查询与 RGBA/RGB565 转换。
#include "testFramework.h"
#include <GN.h>

TEST_CASE(colorChannelsFromUint) {
    GNg::sCOLOR color(0xFF112233u);
    CHECK_EQ(color.COLOR, 0xFF112233u);
    CHECK_EQ((int)color.BGRA.a, 0xFF);
    CHECK_EQ((int)color.BGRA.r, 0x11);
    CHECK_EQ((int)color.BGRA.g, 0x22);
    CHECK_EQ((int)color.BGRA.b, 0x33);
}

TEST_CASE(colorChannelsFromBytes) {
    GNg::sCOLOR color((unsigned char)255, (unsigned char)128,
                      (unsigned char)0, (unsigned char)200);
    CHECK_EQ((int)color.BGRA.r, 255);
    CHECK_EQ((int)color.BGRA.g, 128);
    CHECK_EQ((int)color.BGRA.b, 0);
    CHECK_EQ((int)color.BGRA.a, 200);
}

TEST_CASE(color4fFromBytes) {
    GNg::sCOLOR4F color((unsigned char)255, (unsigned char)128,
                        (unsigned char)0, (unsigned char)255);
    CHECK_NEAR(color.r, 1.0f, 1e-6f);
    CHECK_NEAR(color.g, 128.0f / 255.0f, 1e-6f);
    CHECK_NEAR(color.b, 0.0f, 1e-6f);
    CHECK_NEAR(color.a, 1.0f, 1e-6f);
}

TEST_CASE(formatHasAlpha) {
    CHECK(GNg::ImgFmtHasAlpha(GNg::CR_RGBA));
    CHECK(GNg::ImgFmtHasAlpha(GNg::CR_LUMINANCE_ALPHA));
    CHECK(!GNg::ImgFmtHasAlpha(GNg::CR_RGB));
    CHECK(!GNg::ImgFmtHasAlpha(GNg::CR_RGB565));
}

TEST_CASE(formatByteNum) {
    CHECK_EQ(GNg::ImgFmtGetByteNum(GNg::CR_RGBA), 4);
    CHECK_EQ(GNg::ImgFmtGetByteNum(GNg::CR_RGB), 3);
    CHECK_EQ(GNg::ImgFmtGetByteNum(GNg::CR_RGB565), 2);
    CHECK_EQ(GNg::ImgFmtGetByteNum(GNg::CR_LUMINANCE), 1);
    CHECK_EQ(GNg::ImgFmtGetByteNum(GNg::CR_ALPHA), 1);
}

TEST_CASE(formatFromBitNum) {
    CHECK_EQ(GNg::ImgFmtGetFormat(32), GNg::CR_RGBA);
    CHECK_EQ(GNg::ImgFmtGetFormat(24), GNg::CR_RGB);
    CHECK_EQ(GNg::ImgFmtGetFormat(16), GNg::CR_RGB565);
    CHECK_EQ(GNg::ImgFmtGetFormat(8), GNg::CR_PALETTE);
}

TEST_CASE(formatRgbaToRgb565White) {
    uint8 rgba[4] = {255, 255, 255, 255};
    unsigned short rgb565 = 0;
    GNg::ImgFmtConvertTs((uint8 *)&rgb565, GNg::CR_RGB565, rgba, GNg::CR_RGBA, 1, false);
    CHECK_EQ(rgb565, 0xFFFF);
}

TEST_CASE(formatRgb565ToRgbaWhite) {
    unsigned short rgb565 = 0xFFFF;
    uint8 rgba[4] = {0, 0, 0, 0};
    GNg::ImgFmtConvertTs(rgba, GNg::CR_RGBA, (const uint8 *)&rgb565, GNg::CR_RGB565, 1, false);
    CHECK_EQ((int)rgba[0], 255);
    CHECK_EQ((int)rgba[1], 255);
    CHECK_EQ((int)rgba[2], 255);
    CHECK_EQ((int)rgba[3], 255);
}

TEST_CASE(formatRgbaToRgbaCopy) {
    uint8 source[4] = {1, 2, 3, 4};
    uint8 dest[4] = {0, 0, 0, 0};
    GNg::ImgFmtConvertTs(dest, GNg::CR_RGBA, source, GNg::CR_RGBA, 1, false);
    CHECK_EQ((int)dest[0], 1);
    CHECK_EQ((int)dest[1], 2);
    CHECK_EQ((int)dest[2], 3);
    CHECK_EQ((int)dest[3], 4);
}

TEST_CASE(formatRgbToRgba) {
    uint8 source[3] = {10, 20, 30};
    uint8 dest[4] = {0, 0, 0, 0};
    GNg::ImgFmtConvertTs(dest, GNg::CR_RGBA, source, GNg::CR_RGB, 1, false);
    CHECK_EQ((int)dest[0], 10);
    CHECK_EQ((int)dest[1], 20);
    CHECK_EQ((int)dest[2], 30);
    CHECK_EQ((int)dest[3], 255);
}

TEST_CASE(formatIsLuminance) {
    CHECK(GNg::ImgFmtIsLuminance(GNg::CR_LUMINANCE));
    CHECK(GNg::ImgFmtIsLuminance(GNg::CR_LUMINANCE_ALPHA));
    CHECK(!GNg::ImgFmtIsLuminance(GNg::CR_RGB));
}

TEST_CASE(formatByteNumWide) {
    CHECK_EQ(GNg::ImgFmtGetByteNum(GNg::CR_RGB16), 12);
    CHECK_EQ(GNg::ImgFmtGetByteNum(GNg::CR_RGBA16), 16);
    CHECK_EQ(GNg::ImgFmtGetByteNum(GNg::CR_DEFAULT_FORMAT), 4);
}
