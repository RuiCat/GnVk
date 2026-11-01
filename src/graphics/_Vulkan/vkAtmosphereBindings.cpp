// vkAtmosphereBindings.cpp — L4：把大气 LUT 视图/采样器写入需要采样它们的描述符集。
//
// 为什么单独一个函数：LUT 在 GN_VkInit3D 中（init3DAtmosphereLut）才就绪，而 PBR/延迟/天空/
// 环境烘焙的描述符集在各自 init 阶段创建；本函数在所有相关 init 之后调用一次，统一补写
// 2/3（天空、环境烘焙）、10/11（PBR）、24/25（延迟）四组槽位。契约见 docs/L4_L3_CONTRACT.md §4。
#include <GN.h>
#include "GN_Vk.h"
#include "vkInternal.h"

namespace GNg {

namespace {

void writeLutPair(VkDescriptorSet set, uint32_t firstBinding, const VkDescriptorImageInfo *pair) {
    if(set == VK_NULL_HANDLE) return;
    VkWriteDescriptorSet w[2] = {{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET},
                                 {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET}};
    for(int i = 0; i < 2; ++i) {
        w[i].dstSet = set;
        w[i].dstBinding = firstBinding + (uint32_t)i;
        w[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        w[i].descriptorCount = 1;
        w[i].pImageInfo = &pair[i];
    }
    vkUpdateDescriptorSets(GN_Vk.device, 2, w, 0, 0);
}

} // namespace

void wireAtmosphereDescriptors() {
    VkImageView transView = VK_NULL_HANDLE;
    VkImageView skyView = VK_NULL_HANDLE;
    VkSampler transSampler = VK_NULL_HANDLE;
    VkSampler skySampler = VK_NULL_HANDLE;
    if(!GN_VkGetAtmosphereLutViews(&transView, &skyView, &transSampler, &skySampler)) {
        fprintf(stderr, "[GN_Vk] 警告: 大气 LUT 未就绪，天空/空气透视描述符跳过（L4）\n");
        return;
    }
    VkDescriptorImageInfo pair[2] = {
        {transSampler, transView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
        {skySampler, skyView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL}};

    // 天空盒 set0：2/3
    writeLutPair(GN_Vk.skyboxDescriptorSet, 2, pair);
    // PBR set0：10/11
    writeLutPair(GN_Vk.descriptorSetPBR, 10, pair);
    // 延迟光照 set0：24/25
    writeLutPair(GN_Vk.deferredLightDescriptorSet, 24, pair);
}

} // namespace GNg
