// testHdrSave.cpp — SaveHdrFile 格式判定回归（2026-11-26 缺陷修复）。
// 旧写法 (CR_RGB || CR_RGBA) 是枚举常量求或，恒真 ⇒ ≤4 字节的非 RGB/RGBA 格式
// （灰度等）从不转换，PackRGBE0 按 3 字节/像素读 1 字节/像素的行（越界读 + 错色）。
//
// 编解码口径：PackRGBE0 字节直存、指数 128；GT_UnpkRGBE 乘 ldexp(1,-8)=1/256。
// 所以往返是精确的：decoded = src / 256。w≥8 才走 RLE 读写路径。
#include "testFramework.h"

#include <GN.h>

#include <cmath>
#include <cstdio>

namespace {

void checkGrayRoundTrip(GNg::COLOR_FORMAT fmt, const uint8 *src, int w, int h,
                        const char *path) {
    CHECK(GNg::SaveHdrFile((char *)path, (uint8 *)src, fmt, w, h, false, 1));
    GNg::COLOR_FORMAT outFmt = GNg::CR_DEFAULT_FORMAT;
    int rw = 0, rh = 0;
    uint8 *data = GNg::LoadHdrFileData((char *)path, &outFmt, &rw, &rh);
    CHECK(data != 0);
    if(!data) return;
    CHECK_EQ(rw, w);
    CHECK_EQ(rh, h);
    const float *rgb = (const float *)data;
    for(int i = 0; i < w * h; ++i) {
        const float expect = (float)src[i * (fmt == GNg::CR_RGBA ? 4 : 1)] / 256.0f;
        for(int c = 0; c < 3; ++c) {
            CHECK(std::fabs(rgb[i * 3 + c] - expect) < 1.0e-6f);
        }
    }
    delete[] (float *)data;
    std::remove(path);
}

} // namespace

TEST_CASE(hdrSaveLuminanceRoundTrip) {
    uint8 src[16];
    for(int i = 0; i < 16; ++i) src[i] = (uint8)(i * 17); // 0..255 灰阶
    checkGrayRoundTrip(GNg::CR_LUMINANCE, src, 8, 2, "build/test-hdr-luma.hdr");
}

TEST_CASE(hdrSaveRgbaPassthroughRoundTrip) {
    uint8 src[32];
    for(int i = 0; i < 8; ++i) {
        src[i * 4 + 0] = (uint8)(i * 31);
        src[i * 4 + 1] = (uint8)(i * 17);
        src[i * 4 + 2] = (uint8)(255 - i * 31);
        src[i * 4 + 3] = 255;
    }
    // RGBA 直通：R/G/B 各自保留（上面循环只断言灰度，这里单独验三通道）
    char path[] = "build/test-hdr-rgba.hdr";
    CHECK(GNg::SaveHdrFile(path, src, GNg::CR_RGBA, 8, 1, false, 1));
    GNg::COLOR_FORMAT outFmt = GNg::CR_DEFAULT_FORMAT;
    int rw = 0, rh = 0;
    uint8 *data = GNg::LoadHdrFileData(path, &outFmt, &rw, &rh);
    CHECK(data != 0);
    if(!data) return;
    const float *rgb = (const float *)data;
    for(int i = 0; i < 8; ++i) {
        for(int c = 0; c < 3; ++c) {
            const float expect = (float)src[i * 4 + c] / 256.0f;
            CHECK(std::fabs(rgb[i * 3 + c] - expect) < 1.0e-6f);
        }
    }
    delete[] (float *)data;
    std::remove(path);
}
