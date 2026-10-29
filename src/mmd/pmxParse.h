#pragma once
#ifndef GN_MMD_PMX_PARSE_H
#define GN_MMD_PMX_PARSE_H
// PMX 解析内部接口：跨 pmxLoader.cpp / pmxMorph.cpp / pmxSoftBody.cpp 复用的读取原语。
#include "mmdByteReader.h"
#include "pmxModel.h"

#include <string>

namespace mmd {
namespace detail {

// 读取文件头指定宽度的有符号索引；255/65535 等哨兵值返回 -1。宽度非法返回 false。
bool readPmxIndex(ByteReader &reader, uint8_t size, int32_t &value);

// 按 PMX encoding 读取带长度前缀的字符串（0 = UTF-16LE，其它 = UTF-8）。
bool readPmxString(ByteReader &reader, uint8_t encoding, std::string &value);

bool readFloat3(ByteReader &reader, float out[3]);
bool readFloat4(ByteReader &reader, float out[4]);

// 解析一个完整变形（名称 + 类型 + 偏移量数组）。
bool parsePmxMorph(ByteReader &reader, const PmxSetting &setting, PmxMorph &morph);

// 解析一个软体（PMX 2.1）。
bool parsePmxSoftBody(ByteReader &reader, const PmxSetting &setting, PmxSoftBody &body);

} // namespace detail
} // namespace mmd
#endif // GN_MMD_PMX_PARSE_H
