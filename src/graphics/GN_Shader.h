#pragma once
#ifndef _GN_SHADER_H_
#define _GN_SHADER_H_
// ╔══════════════════════════════════════════════════════════╗
// ║  GN_mini — 着色器（SPIR-V）加载与模块缓存                 ║
// ║  文件读取/内容哈希与 Vulkan 模块创建分离，前者可无 GPU 单测  ║
// ╚══════════════════════════════════════════════════════════╝
//
// 契约（冻结）：
//   * 模块以 **SPIR-V 内容哈希** 为键，同一份字节只创建一个 VkShaderModule；
//   * 模块生命周期归 cSHADER_CACHE 所有，调用方不得自行销毁；
//   * 读取失败时 LoadSpirvFile 返回 false 并保持 out 为空，不抛异常；**0 字节文件视为读取失败**；
//   * 缓存析构即 Release：必须在 VkDevice 销毁**之前**析构缓存或显式 Release，
//     否则会对已销毁设备调 vkDestroyShaderModule；
//   * 模块以 64 位内容哈希为键命中，不做二次字节比较（碰撞为理论风险，见 P1b 备忘）。

#include <vulkan/vulkan.h>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace GN3d {

// 读取 .spv 文件到 out（成功返回 true；失败返回 false 且 out 被清空）
bool LoadSpirvFile(const char *path, std::vector<char> &out);

// SPIR-V 字节内容哈希（FNV-1a 64，按字节；用于模块与管线缓存键）
uint64_t HashSpirv(const void *data, size_t bytes);

// SPIR-V 魔数校验（0x07230203 小端）
bool IsSpirv(const void *data, size_t bytes);

// ═══════════════════════════════════════════════════════════
//  cSHADER_CACHE — 同一 SPIR-V 内容只创建一个模块
// ═══════════════════════════════════════════════════════════
class cSHADER_CACHE {
  public:
    cSHADER_CACHE();
    ~cSHADER_CACHE();

    void Init(VkDevice device);
    void Release(); // 销毁全部模块；可重复调用

    // 命中则复用；成功返回模块，失败返回 VK_NULL_HANDLE
    VkShaderModule GetOrCreate(const void *spirv, size_t bytes, bool *created = 0);

    uint32_t GetCount() const { return (uint32_t)aModules.size(); }
    uint64_t GetHitCount() const { return aHits; }
    uint64_t GetMissCount() const { return aMisses; }

  private:
    struct sMODULE {
        uint64_t aKey;
        VkShaderModule aModule;
    };

    std::vector<sMODULE> aModules;
    VkDevice aDevice;
    uint64_t aHits;
    uint64_t aMisses;
};

} // namespace GN3d
#endif // _GN_SHADER_H_
