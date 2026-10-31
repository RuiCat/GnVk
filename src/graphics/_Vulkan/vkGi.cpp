// G1b：把 G1a 产出的 GI 资源写入三个管线的描述符槽位（前向 4 / PBR 7 / 延迟 9），
// 并额外绑定预滤波环境 cubemap 与 BRDF LUT（PBR 8/9、延迟 10/11）。
// 启动时把 GI 倍率置 0（关闭），着色器走旧 IBL 分支，保证默认观感与接线前一致。
#include <GN.h>
#include "GN_Vk.h"
#include "vkInternal.h"
#include "giParams.h"

#include <cstdio>

namespace GNg {

// 把 GI 倍率置 0（GI 关闭），保留 G1a 写入的 SH9 与 envParams。
static void disableGiByDefault() {
    if(GN_Vk.giUBOMemory == VK_NULL_HANDLE) return;
    void *mapped = nullptr;
    vkMapMemory(GN_Vk.device, GN_Vk.giUBOMemory, 0, sizeof(GN3d::sGI_UBO), 0, &mapped);
    if(mapped) {
        GN3d::sGI_UBO *ubo = (GN3d::sGI_UBO *)mapped;
        ubo->indirect[0] = 0.0f; // 漫反射倍率
        ubo->indirect[1] = 0.0f; // 镜面倍率
        vkUnmapMemory(GN_Vk.device, GN_Vk.giUBOMemory);
    }
}

// 三个管线共用的 GI UBO 写入；set 为空则跳过。
static void writeGiUbo(VkDescriptorSet set, uint32_t binding) {
    if(set == VK_NULL_HANDLE || GN_Vk.giUBOBuffer == VK_NULL_HANDLE) return;
    VkDescriptorBufferInfo dbi = {GN_Vk.giUBOBuffer, 0, sizeof(GN3d::sGI_UBO)};
    VkWriteDescriptorSet w = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    w.dstSet = set;
    w.dstBinding = binding;
    w.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    w.descriptorCount = 1;
    w.pBufferInfo = &dbi;
    vkUpdateDescriptorSets(GN_Vk.device, 1, &w, 0, 0);
}

// 预滤波 cubemap + BRDF LUT 两个采样器槽位。
static void writeGiEnvironment(VkDescriptorSet set, uint32_t cubeBinding, uint32_t lutBinding) {
    if(set == VK_NULL_HANDLE) return;
    VkDescriptorImageInfo cube = {GN_Vk.envPrefilteredSampler, GN_Vk.envPrefilteredView,
                                  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    VkDescriptorImageInfo lut = {GN_Vk.brdfLutSampler, GN_Vk.brdfLutView,
                                 VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    VkWriteDescriptorSet w[2] = {{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET},
                                 {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET}};
    w[0].dstSet = set;
    w[0].dstBinding = cubeBinding;
    w[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    w[0].descriptorCount = 1;
    w[0].pImageInfo = &cube;
    w[1].dstSet = set;
    w[1].dstBinding = lutBinding;
    w[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    w[1].descriptorCount = 1;
    w[1].pImageInfo = &lut;
    vkUpdateDescriptorSets(GN_Vk.device, 2, w, 0, 0);
}

// GN_VkInit3D 末尾调用：此时三套描述符布局/描述符集与 G1a 资源都已存在。
void init3DGiBindings() {
    if(GN_Vk.giUBOBuffer == VK_NULL_HANDLE) {
        fprintf(stderr, "[GN_Vk] 警告: GI UBO 不可用，GI 槽位未接线（G1b）\n");
        return;
    }
    disableGiByDefault();
    // 前向 binding 4（着色器暂未使用，按 §3.7 冻结槽位预留）。
    writeGiUbo(GN_Vk.descriptorSet3D, 4);
    // PBR binding 7=GI UBO、8=预滤波 cubemap、9=BRDF LUT。
    writeGiUbo(GN_Vk.descriptorSetPBR, 7);
    writeGiEnvironment(GN_Vk.descriptorSetPBR, 8, 9);
    // 延迟 binding 9=GI UBO、10=预滤波 cubemap、11=BRDF LUT（8 仍是天气雾 UBO）。
    writeGiUbo(GN_Vk.deferredLightDescriptorSet, 9);
    writeGiEnvironment(GN_Vk.deferredLightDescriptorSet, 10, 11);
    if(GN_Vk.envPrefilteredView == VK_NULL_HANDLE || GN_Vk.brdfLutView == VK_NULL_HANDLE) {
        fprintf(stderr, "[GN_Vk] 警告: 预滤波 cubemap/BRDF LUT 不可用，GI 环境采样槽位未写全（G1b）\n");
    }
}

// 开启/关闭 GI IBL：只改 GI UBO 的 indirect[0]/[1]（漫反射/镜面倍率），
// 其余字段（SH9/envParams/invViewProj）保持当前值；GI UBO 不存在时静默返回。
void GN_VkEnableGI(bool enable) {
    if(GN_Vk.giUBOMemory == VK_NULL_HANDLE) return;
    void *mapped = nullptr;
    vkMapMemory(GN_Vk.device, GN_Vk.giUBOMemory, 0, sizeof(GN3d::sGI_UBO), 0, &mapped);
    if(mapped) {
        GN3d::sGI_UBO *ubo = (GN3d::sGI_UBO *)mapped;
        ubo->indirect[0] = enable ? 1.0f : 0.0f; // 漫反射倍率
        ubo->indirect[1] = enable ? 1.0f : 0.0f; // 镜面倍率
        vkUnmapMemory(GN_Vk.device, GN_Vk.giUBOMemory);
    }
}

} // namespace GNg
