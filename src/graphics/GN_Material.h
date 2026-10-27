#pragma once
#ifndef _GN_MATERIAL_H_
#define _GN_MATERIAL_H_
// ╔══════════════════════════════════════════════════════════╗
// ║  GN_mini — GPU 材质实例（描述/校验/去重键；CPU 侧）       ║
// ╚══════════════════════════════════════════════════════════╝
//
// 目标（P1c）：把 `GN3d::sMATERIAL`（CPU 光照属性）与 GPU 资源绑定（管线 + 描述符集 + UBO/纹理）分离。
//
// 契约：
//   * 一个材质实例 = 管线缓存键 + 一组描述符绑定（binding/种类/阶段/资源指针）。
//   * 绑定按 `aBinding` 严格递增且不重复；`aResource` 必须非空（P8 的渲染器把它解释为 VkBuffer/纹理句柄）。
//   * `aPipelineKey` 由 `cPIPELINE_CACHE` 的键提供（0 视为未指定 ⇒ 非法）。
//   * 本层只做 CPU 侧描述与去重；VkDescriptorSet 创建/写入在 P8（`_Vulkan/vkMaterial.cpp`）实现，
//     句柄槽 `aDescriptorSet` 由渲染器填充，本层不碰 Vulkan 类型。
#include <cstdint>
#include <vector>

namespace GN3d {

enum eMATERIAL_BINDING_KIND {
    eMAT_BIND_UNIFORM_BUFFER = 0, // 对应 VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
    eMAT_BIND_COMBINED_IMAGE_SAMPLER = 1 // 对应 VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
};

struct sMATERIAL_BINDING {
    uint32_t aBinding; // 描述符集内的 binding 编号（严格递增）
    eMATERIAL_BINDING_KIND aKind;
    uint32_t aStageFlags; // VkShaderStageFlags 的位掩码（本层不解释具体位）
    const void *aResource; // 资源指针（VkBuffer / 纹理对象；本层只比较地址）
};

struct sMATERIAL_DESC {
    uint64_t aPipelineKey; // 管线缓存键（0 = 未指定，非法）
    sMATERIAL_BINDING aBindings[8];
    uint32_t aBindingCount; // 0..8
};

// CPU 侧材质实例：GPU 句柄由 P8 填充。
struct sMATERIAL_RES {
    uint64_t aKey;
    sMATERIAL_DESC aDesc;
    void *aDescriptorSet; // VkDescriptorSet（P8 之前始终为空）
};

// 校验：管线键非 0、绑定数 <=8、binding 严格递增且不重复、资源非空。
bool ValidateMaterialDesc(const sMATERIAL_DESC &desc);
// 64 位内容键（FNV-1a；包含管线键、binding 顺序、种类、阶段与资源地址）。
uint64_t ComputeMaterialKey(const sMATERIAL_DESC &desc);
// 逐字段比较（资源按地址比较）；非法描述返回 false。
bool MaterialDescEqual(const sMATERIAL_DESC &a, const sMATERIAL_DESC &b);

// CPU 去重表：同一描述（键相同且逐字段相等）返回同一句柄；不创建 GPU 资源。
class cMATERIAL_CACHE {
  public:
    static const uint32_t INVALID_HANDLE = 0xFFFFFFFFu;

    uint32_t GetOrCreate(const sMATERIAL_DESC &desc); // 失败返回 INVALID_HANDLE
    const sMATERIAL_RES *Get(uint32_t handle) const;
    uint32_t GetCount() const { return (uint32_t)aMaterials.size(); }
    void Release(); // 清空表（不释放 GPU 资源；P8 由渲染器负责）

  private:
    std::vector<sMATERIAL_RES> aMaterials;
};

} // namespace GN3d
#endif // _GN_MATERIAL_H_
