// vkBindingLayout.h — canonical 描述符绑定表（set0 场景 / set1 材质 / set2 蒙皮）与阴影层例外的纯 CPU 描述。
// 不引用 Vulkan 类型：数值镜像 VkDescriptorType / VkShaderStageFlags；P2b 创建 VkDescriptorSetLayout 时按本表映射。
// 契约见 docs/MULTILAYER_RENDER_PIPELINE_PLAN.md §3.4。
#ifndef _GN_VK_BINDING_LAYOUT_H_
#define _GN_VK_BINDING_LAYOUT_H_

#include <cstddef>
#include <cstdint>

namespace GN3d {

// 布局版本（与 GN_RenderPipeline.h 的 kBindingLayoutVersion 同源；P2b 用于布局缓存键）。
const uint32_t kVkBindingLayoutVersion = 1;
// 阴影层专用 binding（canonical set0 内的 per-cascade lightVP 槽位）。
const uint32_t kVkShadowBinding = 12;

enum eVK_BINDING_SET {
    eVK_SET_SCENE = 0,
    eVK_SET_MATERIAL = 1,
    eVK_SET_SKIN = 2,
    eVK_SET_COUNT = 3
};

// 镜像 VkDescriptorType 的 32 位数值（避免公共头依赖 vulkan.h）。
enum eVK_BINDING_DESC {
    eVK_DESC_UNIFORM_BUFFER = 6,
    eVK_DESC_STORAGE_BUFFER = 7,
    eVK_DESC_COMBINED_IMAGE_SAMPLER = 100
};

// 镜像 VkShaderStageFlagBits（VERTEX=0x1，FRAGMENT=0x10）。
enum eVK_BINDING_STAGE { eVK_STAGE_VERTEX = 0x1u, eVK_STAGE_FRAGMENT = 0x10u };

struct sVK_BINDING {
    uint32_t aSet;           // eVK_BINDING_SET
    uint32_t aBinding;       // set 内 binding 号（canonical 表内连续）
    uint32_t aDescriptorType; // eVK_BINDING_DESC（VkDescriptorType 数值）
    uint32_t aArrayCount;    // 1，CSM 阴影数组为 4
    uint32_t aStageFlags;    // eVK_BINDING_STAGE 位或
};

// canonical set0（13 槽）。返回静态表指针；countOut 可为 null。
const sVK_BINDING *CanonicalSceneBindings(uint32_t *countOut);
// canonical set1（8 槽：0=材质 UBO，1..7=纹理）。
const sVK_BINDING *CanonicalMaterialBindings(uint32_t *countOut);
// canonical set2 的单槽（skinData：eSKIN_NONE→null，eSKIN_UBO/eSKIN_SSBO→对应表）。
const sVK_BINDING *CanonicalSkinBinding(uint32_t skinData);
// 阴影层例外：set0 只含 binding12（每 cascade 一个 set）。
const sVK_BINDING *ShadowBindings(uint32_t *countOut);

// 某套布局的 binding 数（skinData 只影响 set2；eSKIN_NONE 时 set2 为 0）。
uint32_t CanonicalSetBindingCount(eVK_BINDING_SET set, uint32_t skinData);
const char *BindingSetName(eVK_BINDING_SET set);
// 表自检：数量/连续性/类型/集合归属/阴影例外与 set0 binding12 一致。
bool ValidateCanonicalBindingTable(char *reason, size_t reasonSize);

} // namespace GN3d
#endif // _GN_VK_BINDING_LAYOUT_H_
