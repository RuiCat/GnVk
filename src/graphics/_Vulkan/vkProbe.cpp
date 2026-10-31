// G5b-2：SH9 探针 9 张 3D 纹理（RGBA16F 8×4×8）+ probe UBO 48 B + 延迟描述符 14..23。
// 启动时把当前环境 SH9（GI UBO 的 sh）填满所有探针；weight=0（默认）时 shader 不采样探针。
#include <GN.h>
#include "GN_Vk.h"
#include "vkInternal.h"
#include "giParams.h"
#include "GN_3Dprobe.h"

#include <cstdio>
#include <cstring>
#include <vector>

namespace GNg {
namespace {

const int PROBE_TEXTURE_COUNT = 9;
const uint32_t PROBE_COUNT_X = 8;
const uint32_t PROBE_COUNT_Y = 4;
const uint32_t PROBE_COUNT_Z = 8;
const uint32_t PROBE_TEXELS = PROBE_COUNT_X * PROBE_COUNT_Y * PROBE_COUNT_Z; // 256
const uint32_t PROBE_FLOATS_PER_IMAGE = PROBE_TEXELS * 4u;                   // 1024

GN3d::cPROBE_GRID sProbeGrid; // 256 探针 × 36 float（固定尺寸，不随 resize）

// 一次性命令：录制 → 提交 → 等 fence。
bool submitOnce(VkCommandBuffer cb) {
    VkResult endResult = vkEndCommandBuffer(cb);
    VkFence fence = VK_NULL_HANDLE;
    VkFenceCreateInfo fci = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    VkResult fenceResult = vkCreateFence(GN_Vk.device, &fci, 0, &fence);
    VkResult submitResult = VK_ERROR_UNKNOWN;
    if(fenceResult == VK_SUCCESS) {
        VkSubmitInfo si = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
        si.commandBufferCount = 1;
        si.pCommandBuffers = &cb;
        submitResult = vkQueueSubmit(GN_Vk.graphicsQueue, 1, &si, fence);
        if(submitResult == VK_SUCCESS) vkWaitForFences(GN_Vk.device, 1, &fence, VK_TRUE, UINT64_MAX);
        vkDestroyFence(GN_Vk.device, fence, 0);
    }
    vkFreeCommandBuffers(GN_Vk.device, GN_Vk.commandPool, 1, &cb);
    if(endResult != VK_SUCCESS || fenceResult != VK_SUCCESS || submitResult != VK_SUCCESS) {
        fprintf(stderr, "[GN_Vk] 探针上传命令失败（end=%d fence=%d submit=%d）\n",
                (int)endResult, (int)fenceResult, (int)submitResult);
        return false;
    }
    return true;
}

VkCommandBuffer beginOnce() {
    VkCommandBuffer cb = VK_NULL_HANDLE;
    VkCommandBufferAllocateInfo ai = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandPool = GN_Vk.commandPool;
    ai.commandBufferCount = 1;
    vkAllocateCommandBuffers(GN_Vk.device, &ai, &cb);
    VkCommandBufferBeginInfo bi = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cb, &bi);
    return cb;
}

// 读当前环境 SH9（GI UBO 偏移 offsetof(sGI_UBO, sh)=48，36 float）；无 GI UBO 时清零。
void readEnvironmentSh(float sh9[9][4]) {
    std::memset(sh9, 0, sizeof(float) * 36);
    if(GN_Vk.giUBOMemory == VK_NULL_HANDLE) return;
    void *mapped = nullptr;
    vkMapMemory(GN_Vk.device, GN_Vk.giUBOMemory, 0, sizeof(GN3d::sGI_UBO), 0, &mapped);
    if(mapped) {
        std::memcpy(sh9, (const char *)mapped + offsetof(GN3d::sGI_UBO, sh), sizeof(float) * 36);
        vkUnmapMemory(GN_Vk.device, GN_Vk.giUBOMemory);
    }
}

bool createProbeImages() {
    for(int k = 0; k < PROBE_TEXTURE_COUNT; ++k) {
        VkImageCreateInfo ii = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        ii.imageType = VK_IMAGE_TYPE_3D;
        ii.extent = {PROBE_COUNT_X, PROBE_COUNT_Y, PROBE_COUNT_Z};
        ii.mipLevels = 1;
        ii.arrayLayers = 1;
        ii.format = VK_FORMAT_R16G16B16A16_SFLOAT;
        ii.tiling = VK_IMAGE_TILING_OPTIMAL;
        ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        ii.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        ii.samples = VK_SAMPLE_COUNT_1_BIT;
        ii.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if(vkCreateImage(GN_Vk.device, &ii, 0, &GN_Vk.probeImages[k]) != VK_SUCCESS) return false;
        VkMemoryRequirements mr;
        vkGetImageMemoryRequirements(GN_Vk.device, GN_Vk.probeImages[k], &mr);
        VkMemoryAllocateInfo mai = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        mai.allocationSize = mr.size;
        mai.memoryTypeIndex = FindMemType(mr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if(vkAllocateMemory(GN_Vk.device, &mai, 0, &GN_Vk.probeMemories[k]) != VK_SUCCESS) {
            return false;
        }
        vkBindImageMemory(GN_Vk.device, GN_Vk.probeImages[k], GN_Vk.probeMemories[k], 0);
        VkImageViewCreateInfo vi = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        vi.image = GN_Vk.probeImages[k];
        vi.viewType = VK_IMAGE_VIEW_TYPE_3D;
        vi.format = VK_FORMAT_R16G16B16A16_SFLOAT;
        vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        vi.subresourceRange.levelCount = 1;
        vi.subresourceRange.layerCount = 1;
        if(vkCreateImageView(GN_Vk.device, &vi, 0, &GN_Vk.probeViews[k]) != VK_SUCCESS) {
            return false;
        }
    }
    return true;
}

// 整网格上传：9 张纹理 × 1024 floats（=256 texels × 4 half）。布局来自 packProbeTextures。
bool uploadProbeTextures() {
    std::vector<float> packed(static_cast<size_t>(PROBE_TEXTURE_COUNT) * PROBE_FLOATS_PER_IMAGE,
                              0.0f);
    if(!GN3d::packProbeTextures(sProbeGrid, packed.data())) return false;
    std::vector<unsigned short> halves(packed.size());
    for(size_t i = 0; i < packed.size(); ++i) halves[i] = GN_ValueFloatToHalf(packed[i]);
    const VkDeviceSize bytes = (VkDeviceSize)halves.size() * 2;
    VkBuffer staging = VK_NULL_HANDLE;
    VkDeviceMemory stagingMem = VK_NULL_HANDLE;
    CreateBuf(bytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging,
              stagingMem);
    if(staging == VK_NULL_HANDLE) return false;
    void *mapped = nullptr;
    vkMapMemory(GN_Vk.device, stagingMem, 0, bytes, 0, &mapped);
    std::memcpy(mapped, halves.data(), (size_t)bytes);
    vkUnmapMemory(GN_Vk.device, stagingMem);

    VkCommandBuffer cb = beginOnce();
    for(int k = 0; k < PROBE_TEXTURE_COUNT; ++k) {
        VkImageMemoryBarrier b = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        b.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        b.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.image = GN_Vk.probeImages[k];
        b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        b.subresourceRange.levelCount = 1;
        b.subresourceRange.layerCount = 1;
        b.srcAccessMask = 0;
        b.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             0, 0, 0, 0, 0, 1, &b);
        VkBufferImageCopy region = {};
        region.bufferOffset = (VkDeviceSize)k * PROBE_FLOATS_PER_IMAGE * 2u;
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.layerCount = 1;
        region.imageExtent = {PROBE_COUNT_X, PROBE_COUNT_Y, PROBE_COUNT_Z};
        vkCmdCopyBufferToImage(cb, staging, GN_Vk.probeImages[k],
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
        b.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        b.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        b.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, 0, 0, 0, 1, &b);
    }
    const bool ok = submitOnce(cb);
    vkDestroyBuffer(GN_Vk.device, staging, 0);
    vkFreeMemory(GN_Vk.device, stagingMem, 0);
    return ok;
}

bool createProbeSampler() {
    VkSamplerCreateInfo sci = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    sci.magFilter = VK_FILTER_LINEAR;
    sci.minFilter = VK_FILTER_LINEAR;
    sci.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sci.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    return vkCreateSampler(GN_Vk.device, &sci, 0, &GN_Vk.probeSampler) == VK_SUCCESS;
}

bool createProbeUbo() {
    CreateBuf(sizeof(GN3d::sPROBE_UBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
              GN_Vk.probeUBOBuffer, GN_Vk.probeUBOMemory);
    if(GN_Vk.probeUBOBuffer == VK_NULL_HANDLE) return false;
    GN3d::sPROBE_UBO ubo = GN3d::makeDefaultProbeUbo();
    ubo.origin[0] = 0.0f;
    ubo.origin[1] = 0.0f;
    ubo.origin[2] = 0.0f;
    ubo.spacingCounts[0] = 2.0f;
    ubo.spacingCounts[1] = (float)PROBE_COUNT_X;
    ubo.spacingCounts[2] = (float)PROBE_COUNT_Y;
    ubo.spacingCounts[3] = (float)PROBE_COUNT_Z;
    ubo.weight[0] = 0.0f; // 默认关
    void *mapped = nullptr;
    vkMapMemory(GN_Vk.device, GN_Vk.probeUBOMemory, 0, sizeof(ubo), 0, &mapped);
    std::memcpy(mapped, &ubo, sizeof(ubo));
    vkUnmapMemory(GN_Vk.device, GN_Vk.probeUBOMemory);
    return true;
}

// 延迟描述符：14=probe UBO、15..23=9 张 3D 纹理。
void writeProbeDescriptors() {
    if(GN_Vk.deferredLightDescriptorSet == VK_NULL_HANDLE) return;
    VkDescriptorBufferInfo ubo = {GN_Vk.probeUBOBuffer, 0, sizeof(GN3d::sPROBE_UBO)};
    VkWriteDescriptorSet w = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    w.dstSet = GN_Vk.deferredLightDescriptorSet;
    w.dstBinding = 14;
    w.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    w.descriptorCount = 1;
    w.pBufferInfo = &ubo;
    vkUpdateDescriptorSets(GN_Vk.device, 1, &w, 0, 0);
    VkDescriptorImageInfo infos[9];
    VkWriteDescriptorSet writes[9] = {};
    for(int k = 0; k < 9; ++k) {
        infos[k].sampler = GN_Vk.probeSampler;
        infos[k].imageView = GN_Vk.probeViews[k];
        infos[k].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        writes[k].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[k].dstSet = GN_Vk.deferredLightDescriptorSet;
        writes[k].dstBinding = (uint32_t)(15 + k);
        writes[k].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[k].descriptorCount = 1;
        writes[k].pImageInfo = &infos[k];
    }
    vkUpdateDescriptorSets(GN_Vk.device, 9, writes, 0, 0);
}

} // namespace

void init3DProbes() {
    if(GN_Vk.probeImages[0] != VK_NULL_HANDLE) return; // 幂等
    GN3d::sPROBE_GRID_DESC desc;
    desc.aOrigin = {0.0f, 0.0f, 0.0f};
    desc.aSpacing = 2.0f;
    desc.aCountX = PROBE_COUNT_X;
    desc.aCountY = PROBE_COUNT_Y;
    desc.aCountZ = PROBE_COUNT_Z;
    if(!sProbeGrid.Init(desc)) return;
    float sh9[9][4];
    readEnvironmentSh(sh9);
    for(uint32_t i = 0; i < sProbeGrid.GetProbeCount(); ++i) {
        if(!sProbeGrid.SetProbeSh(i, sh9)) return;
    }
    if(!createProbeSampler() || !createProbeUbo() || !createProbeImages()) {
        fprintf(stderr, "[GN_Vk] 警告: 探针资源初始化失败（G5b-2）\n");
        return;
    }
    if(!uploadProbeTextures()) {
        fprintf(stderr, "[GN_Vk] 警告: 探针纹理上传失败（G5b-2）\n");
        return;
    }
    writeProbeDescriptors();
}

void destroy3DProbes() {
    for(int k = 0; k < 9; ++k) {
        if(GN_Vk.probeViews[k]) vkDestroyImageView(GN_Vk.device, GN_Vk.probeViews[k], 0);
        if(GN_Vk.probeImages[k]) vkDestroyImage(GN_Vk.device, GN_Vk.probeImages[k], 0);
        if(GN_Vk.probeMemories[k]) vkFreeMemory(GN_Vk.device, GN_Vk.probeMemories[k], 0);
        GN_Vk.probeViews[k] = VK_NULL_HANDLE;
        GN_Vk.probeImages[k] = VK_NULL_HANDLE;
        GN_Vk.probeMemories[k] = VK_NULL_HANDLE;
    }
    if(GN_Vk.probeSampler) vkDestroySampler(GN_Vk.device, GN_Vk.probeSampler, 0);
    if(GN_Vk.probeUBOBuffer) vkDestroyBuffer(GN_Vk.device, GN_Vk.probeUBOBuffer, 0);
    if(GN_Vk.probeUBOMemory) vkFreeMemory(GN_Vk.device, GN_Vk.probeUBOMemory, 0);
    GN_Vk.probeSampler = VK_NULL_HANDLE;
    GN_Vk.probeUBOBuffer = VK_NULL_HANDLE;
    GN_Vk.probeUBOMemory = VK_NULL_HANDLE;
}

// 读取当前探针权重（0=关）。
static float probeWeight() {
    if(GN_Vk.probeUBOMemory == VK_NULL_HANDLE) return 0.0f;
    float weight = 0.0f;
    void *mapped = nullptr;
    vkMapMemory(GN_Vk.device, GN_Vk.probeUBOMemory, 0, sizeof(GN3d::sPROBE_UBO), 0, &mapped);
    if(mapped) {
        std::memcpy(&weight, (const char *)mapped + 32, sizeof(float)); // weight@32
        vkUnmapMemory(GN_Vk.device, GN_Vk.probeUBOMemory);
    }
    return weight;
}

void GN_VkSetProbeWeight(float weight) {
    if(GN_Vk.probeUBOMemory == VK_NULL_HANDLE) return;
    if(!(weight >= 0.0f)) weight = 0.0f; // NaN/负数 → 关
    if(weight > 1.0f) weight = 1.0f;
    void *mapped = nullptr;
    vkMapMemory(GN_Vk.device, GN_Vk.probeUBOMemory, 0, sizeof(GN3d::sPROBE_UBO), 0, &mapped);
    if(mapped) {
        std::memcpy((char *)mapped + 32, &weight, sizeof(float));
        vkUnmapMemory(GN_Vk.device, GN_Vk.probeUBOMemory);
    }
}

// 每帧最多 2 个探针向当前环境 SH9 混合（alpha=0.15）；weight=0 或无变化时不上传。
void GN_VkUpdateProbes() {
    if(GN_Vk.probeUBOMemory == VK_NULL_HANDLE || !sProbeGrid.IsValid()) return;
    if(probeWeight() <= 0.0f) return;
    static uint32_t cursor = 0;
    float sh9[9][4];
    readEnvironmentSh(sh9);
    bool changed = false;
    for(int n = 0; n < 2; ++n) {
        const uint32_t index = cursor % sProbeGrid.GetProbeCount();
        ++cursor;
        if(sProbeGrid.BlendProbeSh(index, sh9, 0.15f)) changed = true;
    }
    if(changed) uploadProbeTextures();
}

} // namespace GNg
