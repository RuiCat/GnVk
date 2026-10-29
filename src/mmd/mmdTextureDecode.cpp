// TGA / BMP 解码实现：输出顶部行在前的 RGBA8，边界全部按文件长度校验。
#include "mmdTextureDecode.h"

#include "mmdFile.h"

#include <algorithm>

namespace mmd {

namespace {

uint16_t readLe16(const uint8_t *data) {
    return static_cast<uint16_t>(data[0] | (data[1] << 8));
}

uint32_t readLe32(const uint8_t *data) {
    return static_cast<uint32_t>(data[0]) | (static_cast<uint32_t>(data[1]) << 8) |
           (static_cast<uint32_t>(data[2]) << 16) | (static_cast<uint32_t>(data[3]) << 24);
}

bool fail(std::string *error, const char *message) {
    if (error) {
        *error = message;
    }
    return false;
}

void flipRows(std::vector<uint8_t> &pixels, int width, int height) {
    const size_t rowBytes = static_cast<size_t>(width) * 4;
    std::vector<uint8_t> row(rowBytes);
    for (int y = 0; y < height / 2; ++y) {
        uint8_t *top = &pixels[static_cast<size_t>(y) * rowBytes];
        uint8_t *bottom = &pixels[static_cast<size_t>(height - 1 - y) * rowBytes];
        std::copy(top, top + rowBytes, row.begin());
        std::copy(bottom, bottom + rowBytes, top);
        std::copy(row.begin(), row.end(), bottom);
    }
}

void writeBgraPixel(uint8_t *rgba, const uint8_t *pixel, bool hasAlpha) {
    rgba[0] = pixel[2];
    rgba[1] = pixel[1];
    rgba[2] = pixel[0];
    rgba[3] = hasAlpha ? pixel[3] : 255;
}

bool decodeTga(const std::vector<uint8_t> &file, std::vector<uint8_t> &rgba, int &width, int &height,
               std::string *error) {
    if (file.size() < 18) {
        return fail(error, "TGA 头部截断");
    }
    const uint8_t *header = file.data();
    const uint8_t idLength = header[0];
    const uint8_t colorMapType = header[1];
    const uint8_t imageType = header[2];
    const uint16_t colorMapLength = readLe16(header + 5);
    const uint8_t colorMapDepth = header[7];
    width = readLe16(header + 12);
    height = readLe16(header + 14);
    const uint8_t bitsPerPixel = header[16];
    const uint8_t descriptor = header[17];
    if (colorMapType != 0 || (imageType != 2 && imageType != 10) ||
        (bitsPerPixel != 24 && bitsPerPixel != 32) || width <= 0 || height <= 0) {
        return fail(error, "不支持的 TGA 变体（仅 2/10、24/32 位）");
    }

    size_t offset = 18 + idLength;
    if (colorMapLength > 0) {
        offset += static_cast<size_t>(colorMapLength) * ((colorMapDepth + 7) / 8);
    }
    if (offset > file.size()) {
        return fail(error, "TGA 数据偏移越界");
    }

    const size_t pixelCount = static_cast<size_t>(width) * static_cast<size_t>(height);
    const size_t bytesPerPixel = bitsPerPixel / 8;
    const bool hasAlpha = bitsPerPixel == 32;
    std::vector<uint8_t> pixels(pixelCount * 4);

    if (imageType == 2) {
        if (offset + pixelCount * bytesPerPixel > file.size()) {
            return fail(error, "TGA 像素数据截断");
        }
        for (size_t i = 0; i < pixelCount; ++i) {
            writeBgraPixel(&pixels[i * 4], file.data() + offset + i * bytesPerPixel, hasAlpha);
        }
    } else {
        size_t written = 0;
        while (written < pixelCount) {
            if (offset >= file.size()) {
                return fail(error, "TGA RLE 数据截断");
            }
            const uint8_t packet = file[offset++];
            size_t count = static_cast<size_t>(packet & 0x7F) + 1;
            if (packet & 0x80) {
                if (offset + bytesPerPixel > file.size()) {
                    return fail(error, "TGA RLE 像素截断");
                }
                const uint8_t *pixel = file.data() + offset;
                offset += bytesPerPixel;
                for (size_t i = 0; i < count && written < pixelCount; ++i) {
                    writeBgraPixel(&pixels[written++ * 4], pixel, hasAlpha);
                }
            } else {
                if (offset + count * bytesPerPixel > file.size()) {
                    return fail(error, "TGA RLE 原始像素截断");
                }
                for (size_t i = 0; i < count && written < pixelCount; ++i) {
                    writeBgraPixel(&pixels[written++ * 4], file.data() + offset + i * bytesPerPixel,
                                   hasAlpha);
                }
                offset += count * bytesPerPixel;
            }
        }
    }

    // descriptor bit5 = 图像原点在左上角；未设置时文件是自底向上的。
    if ((descriptor & 0x20) == 0) {
        flipRows(pixels, width, height);
    }
    rgba.swap(pixels);
    return true;
}

bool decodeBmp(const std::vector<uint8_t> &file, std::vector<uint8_t> &rgba, int &width, int &height,
               std::string *error) {
    if (file.size() < 54) {
        return fail(error, "BMP 头部截断");
    }
    if (file[0] != 'B' || file[1] != 'M') {
        return fail(error, "BMP 魔数不匹配");
    }
    const uint32_t dataOffset = readLe32(file.data() + 10);
    const uint32_t headerSize = readLe32(file.data() + 14);
    const int32_t rawWidth = static_cast<int32_t>(readLe32(file.data() + 18));
    const int32_t rawHeight = static_cast<int32_t>(readLe32(file.data() + 22));
    const uint16_t bitsPerPixel = readLe16(file.data() + 28);
    const uint32_t compression = readLe32(file.data() + 30);
    if (headerSize < 40 || rawWidth <= 0 || rawHeight == 0 || compression != 0 ||
        (bitsPerPixel != 24 && bitsPerPixel != 32)) {
        return fail(error, "不支持的 BMP 变体（仅 BI_RGB 24/32 位）");
    }
    width = rawWidth;
    height = rawHeight < 0 ? -rawHeight : rawHeight;
    const bool topDown = rawHeight < 0;
    const size_t bytesPerPixel = bitsPerPixel / 8;
    const size_t stride = ((static_cast<size_t>(width) * bitsPerPixel + 31) / 32) * 4;
    if (dataOffset > file.size() ||
        dataOffset + stride * static_cast<size_t>(height - 1) +
                static_cast<size_t>(width) * bytesPerPixel >
            file.size()) {
        return fail(error, "BMP 像素数据截断");
    }

    std::vector<uint8_t> pixels(static_cast<size_t>(width) * static_cast<size_t>(height) * 4);
    for (int y = 0; y < height; ++y) {
        const int sourceRow = topDown ? y : (height - 1 - y);
        const uint8_t *source = file.data() + dataOffset + static_cast<size_t>(sourceRow) * stride;
        uint8_t *target = &pixels[static_cast<size_t>(y) * static_cast<size_t>(width) * 4];
        for (int x = 0; x < width; ++x) {
            // BI_RGB 的 32 位 alpha 字节属于保留位，按不透明处理（MMD 贴图常用）。
            writeBgraPixel(target + static_cast<size_t>(x) * 4, source + static_cast<size_t>(x) * bytesPerPixel,
                           false);
        }
    }
    rgba.swap(pixels);
    return true;
}

} // namespace

bool decodeTgaOrBmp(const std::string &path, std::vector<uint8_t> &rgba, int &width, int &height,
                    std::string *error) {
    std::vector<uint8_t> file;
    if (!readFileBytes(path, file, error)) {
        return false;
    }
    if (file.size() >= 2 && file[0] == 'B' && file[1] == 'M') {
        return decodeBmp(file, rgba, width, height, error);
    }
    return decodeTga(file, rgba, width, height, error);
}

} // namespace mmd
