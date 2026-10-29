#pragma once
#ifndef GN_MMD_FILE_H
#define GN_MMD_FILE_H
// MMD 文件读取：把整个文件读入内存后交给流式解析器。
#include <cstdint>
#include <string>
#include <vector>

namespace mmd {

// 读取整个文件；失败时写 error 并返回 false。
bool readFileBytes(const std::string &path, std::vector<uint8_t> &bytes, std::string *error);

} // namespace mmd
#endif // GN_MMD_FILE_H
