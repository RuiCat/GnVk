// GN_Spirv.h — SPIR-V 模块的最小结构校验（CPU，不依赖 Vulkan/反射）。
#ifndef _GN_SPIRV_H_
#define _GN_SPIRV_H_

#include <cstddef>
#include <cstdint>

namespace GN3d {

const uint32_t kSpirvMagic = 0x07230203u;

// 校验：魔数、4 字节对齐、≥20 字节 header、版本 1.0–1.6、bound>0；失败写 reason（可为 null）。
bool validateSpirvModule(const void *data, size_t size, char *reason, size_t reasonSize);

} // namespace GN3d
#endif // _GN_SPIRV_H_
