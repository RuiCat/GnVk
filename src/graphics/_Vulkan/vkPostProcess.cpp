// vkPostProcess.cpp — L1 后处理 UBO：POST 参数上传与 composite 描述符 binding 2 绑定。
//
// 契约：POST UBO 32 B（GN3d::sPOST_UBO / gn3d_composite.frag 的 POST_UBO），
// 由 GN_VkSetPost / GN_VkSetExposure / GN_VkSetTonemapMode 写入；composite pass 采样。
// 缓冲不存在时所有 setter 静默忽略（初始化早于 GN_Vk.initialized，不检查该标志）。
#include <GN.h>
#include "GN_Vk.h"
#include "vkInternal.h"

#include <cstring>

namespace GNg {

namespace {

VkBuffer s_postBuffer = VK_NULL_HANDLE;
VkDeviceMemory s_postMemory = VK_NULL_HANDLE;
void *s_postMapped = 0;
GN3d::sPOST_UBO s_postUbo = GN3d::makeDefaultPostUbo();

void writePostUbo() {
    if(!s_postMapped) return;
    std::memcpy(s_postMapped, &s_postUbo, sizeof(s_postUbo));
}

void bindPostDescriptor() {
    if(GN_Vk.compositeDescriptorSet == VK_NULL_HANDLE || s_postBuffer == VK_NULL_HANDLE) return;
    VkDescriptorBufferInfo dbi = {s_postBuffer, 0, sizeof(s_postUbo)};
    VkWriteDescriptorSet w = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    w.dstSet = GN_Vk.compositeDescriptorSet;
    w.dstBinding = 2;
    w.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    w.descriptorCount = 1;
    w.pBufferInfo = &dbi;
    vkUpdateDescriptorSets(GN_Vk.device, 1, &w, 0, 0);
}

} // namespace

void init3DPost() {
    if(s_postBuffer != VK_NULL_HANDLE) return;
    if(!GN_Vk.device) return; // 早于 GN_Vk.initialized 置位
    CreateBuf(sizeof(s_postUbo), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
              s_postBuffer, s_postMemory);
    if(s_postBuffer == VK_NULL_HANDLE || s_postMemory == VK_NULL_HANDLE) {
        destroy3DPost();
        return;
    }
    if(vkMapMemory(GN_Vk.device, s_postMemory, 0, VK_WHOLE_SIZE, 0, &s_postMapped) != VK_SUCCESS) {
        s_postMapped = 0;
        destroy3DPost();
        return;
    }
    writePostUbo();
    bindPostDescriptor();
}

void destroy3DPost() {
    if(s_postMapped && GN_Vk.device) {
        vkUnmapMemory(GN_Vk.device, s_postMemory);
    }
    s_postMapped = 0;
    if(s_postMemory && GN_Vk.device) {
        vkFreeMemory(GN_Vk.device, s_postMemory, 0);
    }
    s_postMemory = VK_NULL_HANDLE;
    if(s_postBuffer && GN_Vk.device) {
        vkDestroyBuffer(GN_Vk.device, s_postBuffer, 0);
    }
    s_postBuffer = VK_NULL_HANDLE;
}

bool GN_VkSetPost(const GN3d::sPOST_UBO &ubo) {
    GN3d::sPOST_UBO fixed;
    const bool valid = GN3d::validatePostUbo(ubo, &fixed);
    s_postUbo = fixed;
    writePostUbo();
    return valid;
}

bool isPostReady() {
    return s_postBuffer != VK_NULL_HANDLE && s_postMapped != 0;
}

bool GN_VkGetPost(GN3d::sPOST_UBO *out) {
    if(!out) return false;
    *out = s_postUbo;
    return s_postBuffer != VK_NULL_HANDLE;
}

void GN_VkSetExposure(float ev) {
    if(!(ev > -10.0f)) ev = -10.0f; // 同时处理 NaN
    if(ev > 10.0f) ev = 10.0f;
    s_postUbo.ev = ev;
    s_postUbo.exposure = GN3d::exposureScaleFromEv(ev);
    writePostUbo();
}

void GN_VkSetTonemapMode(int32_t mode) {
    if(mode < GN3d::eTONEMAP_NONE || mode > GN3d::eTONEMAP_ACES) {
        mode = GN3d::eTONEMAP_ACES;
    }
    s_postUbo.tonemapMode = mode;
    writePostUbo();
}

void GN_VkSetFxaa(bool enabled) {
    s_postUbo.fxaa = enabled ? 1.0f : 0.0f;
    writePostUbo();
}

} // namespace GNg
