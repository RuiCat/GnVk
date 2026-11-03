// tests/cornellBox/cornellPng.h — RGBA8 缓冲写 RGB PNG（CPU/GPU 自检共用）
//
// 职责：只做「RGBA8 → RGB PNG」这一件事；cornellCpuRtProbe 与 cornellGpuRtProbe
// 共用，避免两份几乎相同的 libpng 样板代码。不依赖引擎。
#ifndef CORNELL_PNG_H
#define CORNELL_PNG_H

#include <png.h>

#include <csetjmp>
#include <cstdio>
#include <vector>

namespace Cornell {

// 把 rgba（width*height*4）写成 RGB PNG；成功返回 true。
inline bool writeRgbPng(const char *path, const std::vector<unsigned char> &rgba, int width,
                        int height) {
    FILE *file = std::fopen(path, "wb");
    if (!file) return false;
    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if (!png) {
        std::fclose(file);
        return false;
    }
    png_infop info = png_create_info_struct(png);
    if (!info) {
        png_destroy_write_struct(&png, nullptr);
        std::fclose(file);
        return false;
    }
    if (setjmp(png_jmpbuf(png))) {
        png_destroy_write_struct(&png, &info);
        std::fclose(file);
        return false;
    }
    png_init_io(png, file);
    png_set_IHDR(png, info, width, height, 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png, info);
    std::vector<png_byte> row(static_cast<size_t>(width) * 3u);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const size_t src = (static_cast<size_t>(y) * width + x) * 4u;
            row[static_cast<size_t>(x) * 3u + 0] = rgba[src + 0];
            row[static_cast<size_t>(x) * 3u + 1] = rgba[src + 1];
            row[static_cast<size_t>(x) * 3u + 2] = rgba[src + 2];
        }
        png_write_row(png, row.data());
    }
    png_write_end(png, nullptr);
    png_destroy_write_struct(&png, &info);
    std::fclose(file);
    return true;
}

} // namespace Cornell
#endif // CORNELL_PNG_H
