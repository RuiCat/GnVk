#pragma once
#ifndef GN_MMD_TEXTURE_DECODE_H
#define GN_MMD_TEXTURE_DECODE_H
// ╔══════════════════════════════════════════════════════════╗
// ║  GN_mini — MMD 贴图格式解码（TGA / BMP → RGBA8）        ║
// ╚══════════════════════════════════════════════════════════╝
//
// 引擎的 ImageLoad 只认 `<name>0000.png` 图集，而 MMD 模型贴图多为 .tga/.bmp。
// 本模块只做纯 CPU 解码：输出第一行 = 图像顶部的 RGBA8 字节，供 sIMAGE::DataFill 直接上传。
// 支持范围：TGA 2(未压缩)/10(RLE) 24/32 位；BMP BI_RGB 24/32 位（含负高度自顶向下）。
#include <cstdint>
#include <string>
#include <vector>

namespace mmd {

// 按文件魔数识别 TGA / BMP 并解码；成功时填 rgba（宽*高*4）与尺寸。
bool decodeTgaOrBmp(const std::string &path, std::vector<uint8_t> &rgba, int &width, int &height,
                    std::string *error = nullptr);

} // namespace mmd
#endif // GN_MMD_TEXTURE_DECODE_H
