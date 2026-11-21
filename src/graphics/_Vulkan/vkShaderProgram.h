// vkShaderProgram.h — 程序注册表：元数据登记 + 层入口 SPIR-V 绑定/封口（P2a/P5 CPU 骨架）。
// 纯 CPU：不创建 Vulkan 管线；入口与管线绑定留到 P2b。
#ifndef _GN_VK_SHADER_PROGRAM_H_
#define _GN_VK_SHADER_PROGRAM_H_

#include <cstddef>
#include <cstdint>

#include "GN_RenderPipeline.h"

namespace GN3d {

// 注册状态：空槽 / 元数据已登记（P2a）/ 入口与 SPIR-V 已就绪（P2b 之后）。
enum ePROGRAM_REGISTER_STATE {
    ePROG_STATE_EMPTY = 0,
    ePROG_STATE_DECLARED = 1,
    ePROG_STATE_READY = 2
};

// 登记程序元数据（名字/层掩码/顶点格式/蒙皮/标志/状态）。名字 ≤63 且唯一；失败写 reason。
bool RegisterShaderProgramMeta(const char *name, uint32_t layerMask, eVERTEX_FORMAT vertexFormat,
                               eSKIN_DATA skinData, uint32_t flags, uint32_t state,
                               char *reason, size_t reasonSize);
const sSHADER_PROGRAM *FindShaderProgram(const char *name);
uint32_t ShaderProgramCount();
// 注册内建程序 gn.pbr / gn.depth（幂等）。
bool RegisterBuiltinShaderPrograms(char *reason, size_t reasonSize);
// 清空注册表（单元测试/热重载用；不碰 Vulkan）。
void ResetShaderPrograms();
const char *ProgramStateName(uint32_t state);
// 绑定某层入口的 SPIR-V（校验通过才写入；失败时注册表不变）。layer 必须在程序层掩码内。
bool BindProgramEntry(const char *name, eRENDER_LAYER layer, const void *vertSpv, uint32_t vertSize,
                      const void *fragSpv, uint32_t fragSize, uint32_t pushConstantBytes,
                      char *reason, size_t reasonSize);
// 封口：用当前条目做完整 ValidateShaderProgramDesc；成功才转 READY，失败保持原状态。
bool SealShaderProgram(const char *name, char *reason, size_t reasonSize);
// 当前状态（未登记返回 0/empty）。
uint32_t ShaderProgramState(const char *name);

} // namespace GN3d
#endif // _GN_VK_SHADER_PROGRAM_H_
