// tests/cornellBox/cornellCompare.cpp — 两张 PNG 的误差对比（PT vs 引擎截图）
//
// 用法：make cornellCompare
//       ./build/cornellCompare --ref=pt.png --test=engine.png [--out=diff.png]
// 输出：RMSE / MAE / 最大误差（0..255）与逐通道均值偏差；可选绝对误差热力图。
#include <png.h>

#include <cmath>
#include <csetjmp>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {

struct sImage {
    int aWidth;
    int aHeight;
    std::vector<unsigned char> aRgb;
    sImage() : aWidth(0), aHeight(0) {}
};

bool loadPngRgb(const char *path, sImage *out) {
    FILE *file = std::fopen(path, "rb");
    if (!file) return false;
    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if (!png) {
        std::fclose(file);
        return false;
    }
    png_infop info = png_create_info_struct(png);
    if (!info) {
        png_destroy_read_struct(&png, nullptr, nullptr);
        std::fclose(file);
        return false;
    }
    if (setjmp(png_jmpbuf(png))) {
        png_destroy_read_struct(&png, &info, nullptr);
        std::fclose(file);
        return false;
    }
    png_init_io(png, file);
    png_read_info(png, info);
    const int width = static_cast<int>(png_get_image_width(png, info));
    const int height = static_cast<int>(png_get_image_height(png, info));
    const int colorType = png_get_color_type(png, info);
    const int bitDepth = png_get_bit_depth(png, info);
    if (bitDepth == 16) png_set_strip_16(png);
    if (colorType == PNG_COLOR_TYPE_PALETTE) png_set_palette_to_rgb(png);
    if (colorType == PNG_COLOR_TYPE_GRAY && bitDepth < 8) png_set_expand_gray_1_2_4_to_8(png);
    if (png_get_valid(png, info, PNG_INFO_tRNS)) png_set_tRNS_to_alpha(png);
    if (colorType == PNG_COLOR_TYPE_GRAY || colorType == PNG_COLOR_TYPE_GRAY_ALPHA) {
        png_set_gray_to_rgb(png);
    }
    png_set_strip_alpha(png);
    png_read_update_info(png, info);
    const size_t rowBytes = png_get_rowbytes(png, info);
    std::vector<unsigned char> bytes(rowBytes * static_cast<size_t>(height));
    std::vector<png_bytep> rows(static_cast<size_t>(height));
    for (int y = 0; y < height; ++y) rows[y] = bytes.data() + rowBytes * static_cast<size_t>(y);
    png_read_image(png, rows.data());
    png_destroy_read_struct(&png, &info, nullptr);
    std::fclose(file);
    if (rowBytes < static_cast<size_t>(width) * 3u) return false;
    out->aWidth = width;
    out->aHeight = height;
    out->aRgb.resize(static_cast<size_t>(width) * height * 3u);
    for (int y = 0; y < height; ++y) {
        const unsigned char *src = bytes.data() + rowBytes * static_cast<size_t>(y);
        std::memcpy(out->aRgb.data() + static_cast<size_t>(y) * width * 3u, src,
                    static_cast<size_t>(width) * 3u);
    }
    return true;
}

bool savePngRgb(const char *path, const std::vector<unsigned char> &rgb, int width, int height) {
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
    png_set_IHDR(png, info, static_cast<png_uint_32>(width), static_cast<png_uint_32>(height),
                 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
                 PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png, info);
    std::vector<png_byte> row(static_cast<size_t>(width) * 3u);
    for (int y = 0; y < height; ++y) {
        std::memcpy(row.data(), rgb.data() + static_cast<size_t>(y) * width * 3u,
                    static_cast<size_t>(width) * 3u);
        png_write_row(png, row.data());
    }
    png_write_end(png, nullptr);
    png_destroy_write_struct(&png, &info);
    std::fclose(file);
    return true;
}

bool parseArg(const char *arg, const char *name, const char **value) {
    const size_t length = std::strlen(name);
    if (std::strncmp(arg, name, length) != 0) return false;
    *value = arg + length;
    return true;
}

} // namespace

int main(int argc, char **argv) {
    const char *refPath = nullptr;
    const char *testPath = nullptr;
    const char *diffPath = nullptr;
    for (int i = 1; i < argc; ++i) {
        const char *value = nullptr;
        if (parseArg(argv[i], "--ref=", &value)) {
            refPath = value;
        } else if (parseArg(argv[i], "--test=", &value)) {
            testPath = value;
        } else if (parseArg(argv[i], "--out=", &value)) {
            diffPath = value;
        } else {
            std::fprintf(stderr, "[cornellCompare] 未知参数: %s\n", argv[i]);
            return 2;
        }
    }
    if (!refPath || !testPath) {
        std::fprintf(stderr,
                     "用法: cornellCompare --ref=pt.png --test=engine.png [--out=diff.png]\n");
        return 2;
    }

    sImage ref;
    sImage test;
    if (!loadPngRgb(refPath, &ref) || !loadPngRgb(testPath, &test)) {
        std::fprintf(stderr, "[cornellCompare] 读取 PNG 失败\n");
        return 1;
    }
    if (ref.aWidth != test.aWidth || ref.aHeight != test.aHeight) {
        std::fprintf(stderr, "[cornellCompare] 尺寸不一致: %dx%d vs %dx%d\n", ref.aWidth,
                     ref.aHeight, test.aWidth, test.aHeight);
        return 1;
    }

    const size_t pixelCount = static_cast<size_t>(ref.aWidth) * ref.aHeight;
    double sumSq = 0.0;
    double sumAbs = 0.0;
    double sumSigned[3] = {0.0, 0.0, 0.0};
    int maxError = 0;
    std::vector<unsigned char> diff(pixelCount * 3u);
    for (size_t i = 0; i < pixelCount; ++i) {
        for (int c = 0; c < 3; ++c) {
            const int a = ref.aRgb[i * 3u + c];
            const int b = test.aRgb[i * 3u + c];
            const int d = a - b;
            const int ad = d < 0 ? -d : d;
            sumSq += static_cast<double>(d) * d;
            sumAbs += ad;
            sumSigned[c] += d;
            if (ad > maxError) maxError = ad;
            const int heat = ad > 255 ? 255 : ad;
            diff[i * 3u + c] = static_cast<unsigned char>(heat > 32 ? heat : 0);
        }
    }
    const double count = static_cast<double>(pixelCount) * 3.0;
    const double rmse = std::sqrt(sumSq / count);
    const double mae = sumAbs / count;
    std::printf("[cornellCompare] %dx%d\n", ref.aWidth, ref.aHeight);
    std::printf("  RMSE=%.3f  MAE=%.3f  max=%d (0..255)\n", rmse, mae, maxError);
    std::printf("  signed mean R=%.3f G=%.3f B=%.3f (ref - test)\n",
                sumSigned[0] / pixelCount, sumSigned[1] / pixelCount,
                sumSigned[2] / pixelCount);

    if (diffPath && !savePngRgb(diffPath, diff, ref.aWidth, ref.aHeight)) {
        std::fprintf(stderr, "[cornellCompare] 写误差图失败: %s\n", diffPath);
        return 1;
    }
    return 0;
}
