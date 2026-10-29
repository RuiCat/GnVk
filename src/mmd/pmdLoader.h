#pragma once
#ifndef GN_MMD_PMD_LOADER_H
#define GN_MMD_PMD_LOADER_H
// PMD 模型加载入口：从文件或内存流解析，失败时写 error 且不修改 model 之外的资源。
#include "pmdModel.h"

#include <cstddef>
#include <cstdint>
#include <string>

namespace mmd {

// 从内存流解析 PMD；成功返回 true，失败返回 false 并写 error。
bool loadPmdStream(const uint8_t *data, size_t size, PmdModel &model, std::string *error = nullptr);

// 从文件解析 PMD；成功返回 true，失败返回 false 并写 error。
bool loadPmdFile(const std::string &path, PmdModel &model, std::string *error = nullptr);

} // namespace mmd
#endif // GN_MMD_PMD_LOADER_H
