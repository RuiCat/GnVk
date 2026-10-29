// GN_3Dtexture.cpp — glTF 纹理图片解码：PNG（libpng）/ JPEG（libjpeg，可选）/ data URI。
//
// 所有解码器都按“不可信输入”处理：先校验尺寸上限（16384×16384），再用 setjmp 捕获
// libpng/libjpeg 的错误退出，确保损坏/截断的图片返回 false 而不是崩溃或读越界。
#include <GN.h>
#include "GN_3Dtexture.h"

#include <csetjmp>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <png.h>

#ifdef GN_HAS_JPEG
#include <jpeglib.h>
#endif

namespace GN3d {

namespace {

const png_uint_32 kMaxTextureDimension = 16384;

struct sPNG_READER {
    const uint8_t *aData;
    size_t aSize;
    size_t aOffset;
};

void pngReadCallback(png_structp png, png_bytep out, png_size_t count) {
    sPNG_READER *reader = (sPNG_READER *)png_get_io_ptr(png);
    if(reader == 0 || reader->aOffset + count > reader->aSize) {
        png_error(png, "PNG truncated");
        return;
    }
    std::memcpy(out, reader->aData + reader->aOffset, count);
    reader->aOffset += count;
}

// 把任意 PNG 颜色类型展开为 RGBA8（与 2D 的 ImageLoad 同一套转换，保证两条路径同源）。
void configurePngTransforms(png_structp png, png_infop info) {
    const int colorType = png_get_color_type(png, info);
    if(colorType == PNG_COLOR_TYPE_RGB) png_set_filler(png, 0xFF, PNG_FILLER_AFTER);
    if(colorType == PNG_COLOR_TYPE_PALETTE) png_set_palette_to_rgb(png);
    if(colorType == PNG_COLOR_TYPE_GRAY && png_get_bit_depth(png, info) < 8) {
        png_set_expand_gray_1_2_4_to_8(png);
    }
    if(png_get_valid(png, info, PNG_INFO_tRNS)) png_set_tRNS_to_alpha(png);
    if(colorType == PNG_COLOR_TYPE_GRAY || colorType == PNG_COLOR_TYPE_GRAY_ALPHA) {
        png_set_gray_to_rgb(png);
    }
    if(png_get_bit_depth(png, info) == 16) png_set_strip_16(png);
}

bool base64Value(char c) {
    if(c >= 'A' && c <= 'Z') return true;
    if(c >= 'a' && c <= 'z') return true;
    if(c >= '0' && c <= '9') return true;
    return c == '+' || c == '/';
}

int base64Index(char c) {
    if(c >= 'A' && c <= 'Z') return c - 'A';
    if(c >= 'a' && c <= 'z') return c - 'a' + 26;
    if(c >= '0' && c <= '9') return c - '0' + 52;
    if(c == '+') return 62;
    return 63; // '/'
}

} // namespace

sTEXTURE_IMAGE::sTEXTURE_IMAGE() : aWidth(0), aHeight(0) {
}

sTEXTURE_SAMPLER::sTEXTURE_SAMPLER() : aWrapS(10497), aWrapT(10497), aMagFilter(0), aMinFilter(0) {
}

eTEXTURE_FORMAT sniffTextureFormat(const void *data, size_t size) {
    if(data == 0 || size < 4) return eTEX_FMT_UNKNOWN;
    const uint8_t *bytes = (const uint8_t *)data;
    static const uint8_t pngMagic[8] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    if(size >= 8 && std::memcmp(bytes, pngMagic, 8) == 0) return eTEX_FMT_PNG;
    if(bytes[0] == 0xFF && bytes[1] == 0xD8 && bytes[2] == 0xFF) return eTEX_FMT_JPEG;
    return eTEX_FMT_UNKNOWN;
}

bool decodePngMemory(const void *data, size_t size, sTEXTURE_IMAGE &out) {
    if(data == 0 || size < 8) return false;

    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
    if(png == 0) return false;
    png_infop info = png_create_info_struct(png);
    if(info == 0) {
        png_destroy_read_struct(&png, 0, 0);
        return false;
    }

    sPNG_READER reader = {(const uint8_t *)data, size, 0};
    png_bytep *rows = 0;
    std::vector<uint8_t> pixels;
    if(setjmp(png_jmpbuf(png))) {
        delete[] rows;
        png_destroy_read_struct(&png, &info, 0);
        return false;
    }

    png_set_read_fn(png, &reader, pngReadCallback);
    png_read_info(png, info);
    png_uint_32 width = 0, height = 0;
    int bitDepth = 0, colorType = 0;
    png_get_IHDR(png, info, &width, &height, &bitDepth, &colorType, 0, 0, 0);
    if(width == 0 || height == 0 || width > kMaxTextureDimension || height > kMaxTextureDimension) {
        png_destroy_read_struct(&png, &info, 0);
        return false;
    }

    configurePngTransforms(png, info);
    png_read_update_info(png, info);
    if(png_get_rowbytes(png, info) != (size_t)width * 4u) {
        png_destroy_read_struct(&png, &info, 0);
        return false;
    }

    pixels.resize((size_t)width * (size_t)height * 4u);
    rows = new png_bytep[height];
    for(png_uint_32 y = 0; y < height; ++y) {
        rows[y] = pixels.data() + (size_t)y * (size_t)width * 4u;
    }
    png_read_image(png, rows);
    png_destroy_read_struct(&png, &info, 0);
    delete[] rows;

    out.aWidth = width;
    out.aHeight = height;
    out.aPixels.swap(pixels);
    return true;
}

bool decodeJpegMemory(const void *data, size_t size, sTEXTURE_IMAGE &out) {
#ifdef GN_HAS_JPEG
    if(data == 0 || size < 4) return false;

    struct sJPEG_ERROR {
        jpeg_error_mgr aBase;
        jmp_buf aJump;
    };
    struct sJPEG_ERROR error;
    jpeg_decompress_struct cinfo;
    std::memset(&cinfo, 0, sizeof(cinfo));
    cinfo.err = jpeg_std_error(&error.aBase);
    error.aBase.error_exit = [](j_common_ptr info) {
        sJPEG_ERROR *self = (sJPEG_ERROR *)info->err;
        longjmp(self->aJump, 1);
    };
    if(setjmp(error.aJump)) {
        jpeg_destroy_decompress(&cinfo);
        return false;
    }

    jpeg_create_decompress(&cinfo);
    jpeg_mem_src(&cinfo, (const unsigned char *)data, (unsigned long)size);
    jpeg_read_header(&cinfo, TRUE);
    if(cinfo.image_width == 0 || cinfo.image_height == 0 ||
       cinfo.image_width > kMaxTextureDimension || cinfo.image_height > kMaxTextureDimension) {
        jpeg_destroy_decompress(&cinfo);
        return false;
    }
    cinfo.out_color_space = JCS_RGB;
    jpeg_start_decompress(&cinfo);

    const uint32_t width = (uint32_t)cinfo.output_width;
    const uint32_t height = (uint32_t)cinfo.output_height;
    std::vector<uint8_t> pixels((size_t)width * (size_t)height * 4u);
    std::vector<uint8_t> row((size_t)width * 3u);
    while(cinfo.output_scanline < cinfo.output_height) {
        JSAMPROW rowPointer = row.data();
        jpeg_read_scanlines(&cinfo, &rowPointer, 1);
        const uint32_t y = (uint32_t)cinfo.output_scanline - 1u;
        uint8_t *dst = pixels.data() + (size_t)y * (size_t)width * 4u;
        for(uint32_t x = 0; x < width; ++x) {
            dst[x * 4u + 0u] = row[(size_t)x * 3u + 0u];
            dst[x * 4u + 1u] = row[(size_t)x * 3u + 1u];
            dst[x * 4u + 2u] = row[(size_t)x * 3u + 2u];
            dst[x * 4u + 3u] = 0xFF;
        }
    }
    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);

    out.aWidth = width;
    out.aHeight = height;
    out.aPixels.swap(pixels);
    return true;
#else
    (void)data;
    (void)size;
    (void)out;
    return false;
#endif
}

bool decodeDataUriBase64(const char *uri, std::vector<uint8_t> &out) {
    if(uri == 0 || std::strncmp(uri, "data:", 5) != 0) return false;
    const char *comma = std::strchr(uri, ',');
    if(comma == 0) return false;

    bool isBase64 = false;
    for(const char *p = uri + 5; p < comma; ++p) {
        if(std::strncmp(p, ";base64", 7) == 0) {
            isBase64 = true;
            break;
        }
    }
    if(!isBase64) return false;

    std::vector<uint8_t> decoded;
    uint32_t accumulator = 0;
    uint32_t bits = 0;
    for(const char *p = comma + 1; *p != 0; ++p) {
        const char c = *p;
        if(c == '=') break;
        if(c == ' ' || c == '\n' || c == '\r' || c == '\t') continue;
        if(!base64Value(c)) return false;
        accumulator = (accumulator << 6) | (uint32_t)base64Index(c);
        bits += 6;
        if(bits >= 8) {
            bits -= 8;
            decoded.push_back((uint8_t)((accumulator >> bits) & 0xFFu));
        }
    }
    if(decoded.empty()) return false;
    out.swap(decoded);
    return true;
}

bool decodeAssetTexture(const sASSET_TEXTURE &texture, sTEXTURE_IMAGE &out) {
    std::vector<uint8_t> fromUri;
    const uint8_t *encoded = 0;
    size_t encodedSize = 0;

    if(!texture.aImageBytes.empty()) {
        encoded = texture.aImageBytes.data();
        encodedSize = texture.aImageBytes.size();
    } else if(!texture.aUri.empty()) {
        if(!decodeDataUriBase64(texture.aUri.c_str(), fromUri)) return false;
        encoded = fromUri.data();
        encodedSize = fromUri.size();
    } else {
        return false;
    }

    eTEXTURE_FORMAT format = sniffTextureFormat(encoded, encodedSize);
    if(format == eTEX_FMT_UNKNOWN) {
        if(texture.aMime.find("png") != std::string::npos) {
            format = eTEX_FMT_PNG;
        } else if(texture.aMime.find("jpeg") != std::string::npos ||
                  texture.aMime.find("jpg") != std::string::npos) {
            format = eTEX_FMT_JPEG;
        }
    }

    if(format == eTEX_FMT_PNG) return decodePngMemory(encoded, encodedSize, out);
    if(format == eTEX_FMT_JPEG) return decodeJpegMemory(encoded, encodedSize, out);
    return false;
}

sTEXTURE_SAMPLER samplerFromAssetTexture(const sASSET_TEXTURE &texture) {
    sTEXTURE_SAMPLER sampler;
    sampler.aWrapS = (texture.aWrapS == 0) ? 10497 : texture.aWrapS;
    sampler.aWrapT = (texture.aWrapT == 0) ? 10497 : texture.aWrapT;
    sampler.aMagFilter = texture.aMagFilter;
    sampler.aMinFilter = texture.aMinFilter;
    return sampler;
}

} // namespace GN3d
