#pragma once
#ifndef GN_MMD_ENCODING_H
#define GN_MMD_ENCODING_H
// ╔══════════════════════════════════════════════════════════╗
// ║  GN_mini — MMD 文本编码转换                             ║
// ╚══════════════════════════════════════════════════════════╝
//
// PMD 的文本是 Shift-JIS，PMX 按文件头 encoding 字段存 UTF-16LE 或 UTF-8。
// 解析结果统一转成 UTF-8，调用方无需再关心编码；转换失败时原样返回字节
// （不抛异常、不崩溃），以保证损坏文件仍能读出其余结构。
#include <cstddef>
#include <cstdint>
#include <string>

namespace mmd {

// Shift-JIS → UTF-8（依次尝试 SHIFT-JIS / CP932 / SHIFT_JIS）。
std::string shiftJisToUtf8(const uint8_t *bytes, size_t size);

// UTF-16LE → UTF-8（含代理对；孤立代理项替换为 U+FFFD）。
std::string utf16LeToUtf8(const uint8_t *bytes, size_t size);

} // namespace mmd
#endif // GN_MMD_ENCODING_H
