// tests/cornellBox/cornellCapture.cpp — 引擎 swapchain 读回并保存 PNG
//
// 职责：F12 截图（供 cornellCompare 与 PT 对拍）。读回的是已 composite 的 swapchain
// （tonemap 后），因此与 PT 的 ACES+PNG 输出处于同一显示空间；HUD 在截图时不应绘制。
#include "cornellCapture.h"

#include <GN.h>

#include <png.h>

#include <csetjmp>
#include <cstdio>
#include <vector>

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

namespace Cornell {

namespace {

bool makeDir(const char *path) {
#ifdef _WIN32
    return _mkdir(path) == 0;
#else
    return mkdir(path, 0755) == 0;
#endif
}

// 自己写 PNG（不走 GN_SavePngFile）：后者在 swapchain 读回时曾产出 0 字节文件，
// 这里用 libpng 直接写 RGB，失败时能给出明确原因。
bool writeRgbPng(const char *path, const uint8_t *rgba, int width, int height) {
    FILE *file = std::fopen(path, "wb");
    if (!file) {
        std::fprintf(stderr, "[cornellCapture] 无法创建 %s\n", path);
        return false;
    }
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
    png_set_IHDR(png, info, static_cast<png_uint_32>(width), static_cast<png_uint_32>(height),
                 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
                 PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png, info);

    std::vector<png_byte> row(static_cast<size_t>(width) * 3u);
    for (int y = 0; y < height; ++y) {
        const uint8_t *src = rgba + static_cast<size_t>(y) * width * 4u;
        for (int x = 0; x < width; ++x) {
            row[static_cast<size_t>(x) * 3u + 0] = src[x * 4 + 0];
            row[static_cast<size_t>(x) * 3u + 1] = src[x * 4 + 1];
            row[static_cast<size_t>(x) * 3u + 2] = src[x * 4 + 2];
        }
        png_write_row(png, row.data());
    }
    png_write_end(png, nullptr);
    png_destroy_write_struct(&png, &info);
    std::fclose(file);
    return true;
}

} // namespace

void ensureCaptureDir() {
    makeDir(".local");
    makeDir(".local/cornellbox");
}

bool captureSwapchainPng(const char *path) {
    if (!path) return false;
    const uint32_t width = GNg::GN_Vk.swapchainExtent.width;
    const uint32_t height = GNg::GN_Vk.swapchainExtent.height;
    if (width == 0 || height == 0) {
        std::fprintf(stderr, "[cornellCapture] swapchain 尺寸为 0\n");
        return false;
    }

    int size = 0;
    uint8_t *data = GNg::GN_VkReadBack(0, 0, static_cast<int>(width), static_cast<int>(height),
                                       &size);
    if (!data) {
        std::fprintf(stderr, "[cornellCapture] GN_VkReadBack 返回空（swapchain 布局/时机？）\n");
        return false;
    }
    if (size < static_cast<int>(width * height * 4u)) {
        std::fprintf(stderr, "[cornellCapture] 读回字节数异常: %d < %u\n", size,
                     width * height * 4u);
        GNg::GN_VkReadBackFree(data);
        return false;
    }

    const VkFormat format = GNg::GN_Vk.swapchainFormat;
    if (format == VK_FORMAT_B8G8R8A8_UNORM || format == VK_FORMAT_B8G8R8A8_SRGB) {
        for (uint32_t i = 0; i < width * height; ++i) {
            uint8_t tmp = data[i * 4 + 0];
            data[i * 4 + 0] = data[i * 4 + 2];
            data[i * 4 + 2] = tmp;
        }
    }

    const bool ok = writeRgbPng(path, data, static_cast<int>(width), static_cast<int>(height));
    GNg::GN_VkReadBackFree(data);
    return ok;
}

} // namespace Cornell
