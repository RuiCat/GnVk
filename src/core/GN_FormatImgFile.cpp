// GN_FormatImgFile.cpp — 图像文件读写：BMP/TGA/PNG/HDR 保存与加载、RGBE 打包、图像格式判定（原 GN_Format.cpp 拆分）。
#include "GN.h"
#include "GN_FormatInternal.h"
#ifdef _GN_FORMAT_H_
#if defined _WIN32_ || defined _WIN32_WCE
#ifndef DONT_INCLUDE_IMAGELIBS
#define DONT_INCLUDE_IMAGELIBS
#endif
#if _MSC_VER < 1600
#pragma comment(lib, "png.lib")
#else
#pragma comment(lib, "legacy_stdio_definitions.lib")
#pragma comment(lib, "png.lib")
#if _MSC_VER >= 1900
extern "C" FILE *__cdecl __iob_func(unsigned i) {
    return __acrt_iob_func(i);
}
#endif
#endif
#endif
#include <libpng/png.h>
// 2026-11-26 B4 move-only 拆分：编解码片段在 *.inc，包含顺序即原文件顺序，不要调整。
#include "fmtBmp.inc"
#include "fmtTga.inc"
#include "fmtPng.inc"
#include "fmtHdr.inc"
#include "fmtPixelUtil.inc"
#endif //_GN_FORMAT_H_
