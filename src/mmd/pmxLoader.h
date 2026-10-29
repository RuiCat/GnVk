#pragma once
#ifndef GN_MMD_PMX_LOADER_H
#define GN_MMD_PMX_LOADER_H
// PMX 模型加载入口：从文件或内存流解析，失败时写 error。
#include "pmxModel.h"

#include <cstddef>
#include <cstdint>
#include <string>

namespace mmd {

// 从内存流解析 PMX；成功返回 true，失败返回 false 并写 error。
bool loadPmxStream(const uint8_t *data, size_t size, PmxModel &model, std::string *error = nullptr);

// 从文件解析 PMX；成功返回 true，失败返回 false 并写 error。
bool loadPmxFile(const std::string &path, PmxModel &model, std::string *error = nullptr);

} // namespace mmd
#endif // GN_MMD_PMX_LOADER_H
