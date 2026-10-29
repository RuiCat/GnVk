// GN_3Dtexture.h — glTF 纹理图片解码（CPU，PNG/JPEG）与采样参数提取。
//
// 契约（P1c 纹理 v1）：
//   * 只做“压缩字节 → RGBA8 像素”，不碰 Vulkan；解出的像素为逐行 top-down、每像素 4 字节。
//   * 来源优先级：sASSET_TEXTURE::aImageBytes（bufferView / 外部文件）→ aUri 的 data URI。
//     data URI 只接受 `;base64,` 载荷；外部文件 uri 在导入期已按 .gltf 目录读入字节。
//   * PNG 走 libpng；JPEG 仅当构建带 GN_HAS_JPEG（pkg-config libjpeg 可用）时可用，否则返回 false。
//   * 所有解码失败都返回 false 且不修改 out；损坏输入不得崩溃（libpng/libjpeg 错误走 longjmp 回收）。
#pragma once
#ifndef _GN_3DTEXTURE_H_
#define _GN_3DTEXTURE_H_

#include "GN_3Dasset.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace GN3d {

// 解出的图片：RGBA8、逐行 top-down；aWidth/aHeight 为 0 表示未解码。
struct sTEXTURE_IMAGE {
    uint32_t aWidth;
    uint32_t aHeight;
    std::vector<uint8_t> aPixels;

    sTEXTURE_IMAGE();
};

// glTF sampler 的原始枚举（供 Vulkan 层映射；0/缺省保持原值，映射时给默认）。
struct sTEXTURE_SAMPLER {
    int32_t aWrapS;
    int32_t aWrapT;
    int32_t aMagFilter;
    int32_t aMinFilter;

    sTEXTURE_SAMPLER();
};

enum eTEXTURE_FORMAT {
    eTEX_FMT_UNKNOWN = 0,
    eTEX_FMT_PNG = 1,
    eTEX_FMT_JPEG = 2
};

// 按文件头识别 PNG/JPEG（不依赖扩展名或 mime）。
eTEXTURE_FORMAT sniffTextureFormat(const void *data, size_t size);

// 从内存字节解码 PNG 到 RGBA8；损坏/截断输入返回 false。
bool decodePngMemory(const void *data, size_t size, sTEXTURE_IMAGE &out);

// 从内存字节解码 JPEG 到 RGBA8；未启用 GN_HAS_JPEG 时恒为 false。
bool decodeJpegMemory(const void *data, size_t size, sTEXTURE_IMAGE &out);

// 解出 `data:<mime>;base64,<payload>` 的二进制载荷；非 base64 data URI 返回 false。
bool decodeDataUriBase64(const char *uri, std::vector<uint8_t> &out);

// 按 sASSET_TEXTURE 的来源解码（字节优先，其次 data URI）；格式按文件头、再按 mime 判定。
bool decodeAssetTexture(const sASSET_TEXTURE &texture, sTEXTURE_IMAGE &out);

// 直接取出 sampler 的 wrap/filter 原始枚举（缺省值与导入层一致：wrap=10497，filter=0）。
sTEXTURE_SAMPLER samplerFromAssetTexture(const sASSET_TEXTURE &texture);

} // namespace GN3d
#endif // _GN_3DTEXTURE_H_
