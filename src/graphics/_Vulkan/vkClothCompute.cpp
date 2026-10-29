// vkClothCompute.cpp — P9-a GPU 布料模拟：双 SSBO 乒乓 + compute 管线 + fence 同步。
//
// 契约：
//   * 顶点 = GN3d::sCLOTH_GPU_VERTEX（32B），两块 host-visible SSBO（A/B）乒乓：
//     积分 A→B，随后每轮约束在 A/B 之间翻转（Jacobi，整轮读旧值写另一块）；
//     迭代结束若结果在 B 则 vkCmdCopyBuffer 回 A，保证 GN_VkClothData/绘制始终读 A。
//   * 单条命令缓冲内完成全部 dispatch，最后提交并等待 fence（v1 不跨帧流水化）。
//   * 管线经 GN_Vk.aPipelineCache（ePIPE_COMPUTE）；惰性初始化，只查 device。
#include <GN.h>
#include "GN_Vk.h"
#include "vkInternal.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

namespace GNg {

namespace {

const uint32_t CLOTH_WORKGROUP_SIZE = 64; // 与 gn3d_cloth.comp 的 local_size_x 一致

struct sCLOTH_PUSH {
    float aGravityDt[4]; // xyz = 重力, w = dt
    float aParams0[4]; // x = spacing, y = damping, z = width, w = height
    float aParams1[4]; // x = mode（0 积分 / 1 约束）
};

VkDescriptorSetLayout s_clothSetLayout = VK_NULL_HANDLE;
VkPipelineLayout s_clothPipelineLayout = VK_NULL_HANDLE;
VkPipeline s_clothPipeline = VK_NULL_HANDLE;
VkCommandBuffer s_clothCmd = VK_NULL_HANDLE;
VkFence s_clothFence = VK_NULL_HANDLE;

VkBuffer s_clothBuffers[2] = {VK_NULL_HANDLE, VK_NULL_HANDLE};
VkDeviceMemory s_clothMemories[2] = {VK_NULL_HANDLE, VK_NULL_HANDLE};
void *s_clothMapped[2] = {0, 0};
VkDescriptorSet s_clothSets[2] = {VK_NULL_HANDLE, VK_NULL_HANDLE};
uint32_t s_clothVertexCount = 0;
GN3d::sCLOTH_GPU_DESC s_clothDesc = {};

void releaseClothBuffers() {
    if(GN_Vk.device) {
        for(uint32_t i = 0; i < 2; ++i) {
            if(s_clothSets[i] != VK_NULL_HANDLE && GN_Vk.descriptorPool) {
                vkFreeDescriptorSets(GN_Vk.device, GN_Vk.descriptorPool, 1, &s_clothSets[i]);
            }
            s_clothSets[i] = VK_NULL_HANDLE;
            if(s_clothBuffers[i] != VK_NULL_HANDLE) {
                vkDestroyBuffer(GN_Vk.device, s_clothBuffers[i], nullptr);
            }
            if(s_clothMemories[i] != VK_NULL_HANDLE) {
                vkFreeMemory(GN_Vk.device, s_clothMemories[i], nullptr);
            }
            s_clothBuffers[i] = VK_NULL_HANDLE;
            s_clothMemories[i] = VK_NULL_HANDLE;
            s_clothMapped[i] = 0;
        }
    }
    s_clothVertexCount = 0;
    s_clothDesc = GN3d::sCLOTH_GPU_DESC();
}

void writeClothDescriptorSet(uint32_t setIndex, VkBuffer src, VkBuffer dst) {
    VkDescriptorBufferInfo srcInfo = {};
    srcInfo.buffer = src;
    srcInfo.offset = 0;
    srcInfo.range = VK_WHOLE_SIZE;
    VkDescriptorBufferInfo dstInfo = {};
    dstInfo.buffer = dst;
    dstInfo.offset = 0;
    dstInfo.range = VK_WHOLE_SIZE;
    VkWriteDescriptorSet writes[2] = {{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET},
                                      {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET}};
    writes[0].dstSet = s_clothSets[setIndex];
    writes[0].dstBinding = 0;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[0].descriptorCount = 1;
    writes[0].pBufferInfo = &srcInfo;
    writes[1].dstSet = s_clothSets[setIndex];
    writes[1].dstBinding = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[1].descriptorCount = 1;
    writes[1].pBufferInfo = &dstInfo;
    vkUpdateDescriptorSets(GN_Vk.device, 2, writes, 0, 0);
}

void clothBarrier(VkCommandBuffer commandBuffer, VkAccessFlags srcAccess, VkAccessFlags dstAccess) {
    VkBufferMemoryBarrier barriers[2] = {{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER},
                                         {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER}};
    for(int i = 0; i < 2; ++i) {
        barriers[i].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barriers[i].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barriers[i].buffer = s_clothBuffers[i];
        barriers[i].offset = 0;
        barriers[i].size = VK_WHOLE_SIZE;
        barriers[i].srcAccessMask = srcAccess;
        barriers[i].dstAccessMask = dstAccess;
    }
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, 0, 2, barriers, 0, 0);
}

bool createClothPipelines() {
    VkDescriptorSetLayoutBinding bindings[2] = {};
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    VkDescriptorSetLayoutCreateInfo layoutInfo = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    layoutInfo.bindingCount = 2;
    layoutInfo.pBindings = bindings;
    if(vkCreateDescriptorSetLayout(GN_Vk.device, &layoutInfo, nullptr, &s_clothSetLayout) !=
       VK_SUCCESS) {
        s_clothSetLayout = VK_NULL_HANDLE;
        return false;
    }

    VkPushConstantRange pushRange = {};
    pushRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushRange.offset = 0;
    pushRange.size = sizeof(sCLOTH_PUSH);
    VkPipelineLayoutCreateInfo pipelineInfo = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    pipelineInfo.setLayoutCount = 1;
    pipelineInfo.pSetLayouts = &s_clothSetLayout;
    pipelineInfo.pushConstantRangeCount = 1;
    pipelineInfo.pPushConstantRanges = &pushRange;
    if(vkCreatePipelineLayout(GN_Vk.device, &pipelineInfo, nullptr, &s_clothPipelineLayout) !=
       VK_SUCCESS) {
        s_clothPipelineLayout = VK_NULL_HANDLE;
        return false;
    }

    std::vector<uint32_t> compSpv = LoadSPIRVFile("assets/shaders/gn3d_cloth.comp.spv");
    if(compSpv.empty()) {
        fprintf(stderr, "[GN_Vk] 布料计算着色器加载失败: gn3d_cloth.comp.spv\n");
        return false;
    }
    GN3d::sPIPELINE_DESC desc;
    desc.aKind = GN3d::ePIPE_COMPUTE;
    desc.aCompSpv = compSpv.data();
    desc.aCompSize = compSpv.size() * sizeof(uint32_t);
    desc.aLayout = s_clothPipelineLayout;
    s_clothPipeline = GN_Vk.aPipelineCache.GetOrCreate(desc);
    if(!s_clothPipeline) {
        fprintf(stderr, "[GN_Vk] 布料计算管线创建失败: %s\n", GN_Vk.aPipelineCache.GetLastError());
        return false;
    }
    return true;
}

} // namespace

bool init3DCloth() {
    if(s_clothPipeline != VK_NULL_HANDLE) return true;
    if(!GN_Vk.device || !GN_Vk.descriptorPool) return false; // 早于 GN_Vk.initialized 置位
    if(!createClothPipelines()) {
        destroyCloth();
        return false;
    }
    VkCommandBufferAllocateInfo cbai = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    cbai.commandPool = GN_Vk.commandPool;
    cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbai.commandBufferCount = 1;
    if(vkAllocateCommandBuffers(GN_Vk.device, &cbai, &s_clothCmd) != VK_SUCCESS) {
        s_clothCmd = VK_NULL_HANDLE;
        destroyCloth();
        return false;
    }
    VkFenceCreateInfo fci = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    if(vkCreateFence(GN_Vk.device, &fci, 0, &s_clothFence) != VK_SUCCESS) {
        s_clothFence = VK_NULL_HANDLE;
        destroyCloth();
        return false;
    }
    return true;
}

void destroyCloth() {
    destroyClothDraw(); // 先释放绘制侧的描述符集/索引缓冲（引用了 A 缓冲）
    releaseClothBuffers();
    if(GN_Vk.device) {
        if(s_clothFence != VK_NULL_HANDLE) vkDestroyFence(GN_Vk.device, s_clothFence, nullptr);
        if(s_clothCmd != VK_NULL_HANDLE && GN_Vk.commandPool) {
            vkFreeCommandBuffers(GN_Vk.device, GN_Vk.commandPool, 1, &s_clothCmd);
        }
        if(s_clothPipelineLayout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(GN_Vk.device, s_clothPipelineLayout, nullptr);
        }
        if(s_clothSetLayout != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(GN_Vk.device, s_clothSetLayout, nullptr);
        }
    }
    s_clothFence = VK_NULL_HANDLE;
    s_clothCmd = VK_NULL_HANDLE;
    s_clothPipelineLayout = VK_NULL_HANDLE;
    s_clothSetLayout = VK_NULL_HANDLE;
    s_clothPipeline = VK_NULL_HANDLE; // 归 aPipelineCache
}

bool GN_VkClothCreate(const GN3d::sCLOTH_GPU_DESC &desc) {
    if(!GN_Vk.device) return false;
    if(!GN3d::validateClothGpuDesc(desc)) return false;
    if(!init3DCloth()) return false;
    releaseClothBuffers();

    const uint32_t count = desc.aWidth * desc.aHeight;
    const VkDeviceSize bytes = (VkDeviceSize)count * sizeof(GN3d::sCLOTH_GPU_VERTEX);
    for(uint32_t i = 0; i < 2; ++i) {
        CreateBuf(bytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                             VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                  s_clothBuffers[i], s_clothMemories[i]);
        if(s_clothBuffers[i] == VK_NULL_HANDLE || s_clothMemories[i] == VK_NULL_HANDLE) {
            releaseClothBuffers();
            return false;
        }
        if(vkMapMemory(GN_Vk.device, s_clothMemories[i], 0, VK_WHOLE_SIZE, 0,
                       &s_clothMapped[i]) != VK_SUCCESS) {
            s_clothMapped[i] = 0;
            releaseClothBuffers();
            return false;
        }
        memset(s_clothMapped[i], 0, (size_t)bytes);
    }

    VkDescriptorSetLayout clothLayouts[2] = {s_clothSetLayout, s_clothSetLayout};
    VkDescriptorSetAllocateInfo allocInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    allocInfo.descriptorPool = GN_Vk.descriptorPool;
    allocInfo.descriptorSetCount = 2;
    allocInfo.pSetLayouts = clothLayouts;
    if(vkAllocateDescriptorSets(GN_Vk.device, &allocInfo, s_clothSets) != VK_SUCCESS) {
        s_clothSets[0] = VK_NULL_HANDLE;
        s_clothSets[1] = VK_NULL_HANDLE;
        releaseClothBuffers();
        return false;
    }
    // set0：A → B；set1：B → A。
    writeClothDescriptorSet(0, s_clothBuffers[0], s_clothBuffers[1]);
    writeClothDescriptorSet(1, s_clothBuffers[1], s_clothBuffers[0]);

    s_clothVertexCount = count;
    s_clothDesc = desc;
    return true;
}

bool GN_VkClothUpload(const GN3d::sCLOTH_GPU_VERTEX *vertices, uint32_t count) {
    if(!vertices || count == 0 || !s_clothMapped[0]) return false;
    if(count != s_clothVertexCount) return false;
    const size_t bytes = (size_t)count * sizeof(GN3d::sCLOTH_GPU_VERTEX);
    memcpy(s_clothMapped[0], vertices, bytes);
    return true;
}

bool GN_VkClothSimulate(float dt) {
    if(!s_clothMapped[0] || s_clothVertexCount == 0) return false;
    if(!(dt > 0.0f) || !std::isfinite(dt)) return false;
    if(s_clothPipeline == VK_NULL_HANDLE || s_clothCmd == VK_NULL_HANDLE ||
       s_clothFence == VK_NULL_HANDLE) {
        return false;
    }

    const uint32_t count = s_clothVertexCount;
    const uint32_t groups = (count + CLOTH_WORKGROUP_SIZE - 1) / CLOTH_WORKGROUP_SIZE;
    sCLOTH_PUSH push = {};
    push.aGravityDt[0] = s_clothDesc.aGravity[0];
    push.aGravityDt[1] = s_clothDesc.aGravity[1];
    push.aGravityDt[2] = s_clothDesc.aGravity[2];
    push.aGravityDt[3] = dt;
    push.aParams0[0] = s_clothDesc.aSpacing;
    push.aParams0[1] = s_clothDesc.aDamping;
    push.aParams0[2] = (float)s_clothDesc.aWidth;
    push.aParams0[3] = (float)s_clothDesc.aHeight;
    push.aParams1[0] = 0.0f;

    VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if(vkBeginCommandBuffer(s_clothCmd, &beginInfo) != VK_SUCCESS) return false;

    // 主机写入（Upload）→ compute 读取。
    VkBufferMemoryBarrier hostBarrier[2] = {{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER},
                                            {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER}};
    for(int i = 0; i < 2; ++i) {
        hostBarrier[i].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        hostBarrier[i].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        hostBarrier[i].buffer = s_clothBuffers[i];
        hostBarrier[i].offset = 0;
        hostBarrier[i].size = VK_WHOLE_SIZE;
        hostBarrier[i].srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
        hostBarrier[i].dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    }
    vkCmdPipelineBarrier(s_clothCmd, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         0, 0, 0, 2, hostBarrier, 0, 0);

    vkCmdBindPipeline(s_clothCmd, VK_PIPELINE_BIND_POINT_COMPUTE, s_clothPipeline);
    vkCmdBindDescriptorSets(s_clothCmd, VK_PIPELINE_BIND_POINT_COMPUTE, s_clothPipelineLayout, 0, 1,
                            &s_clothSets[0], 0, nullptr);
    vkCmdPushConstants(s_clothCmd, s_clothPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                       sizeof(push), &push);
    vkCmdDispatch(s_clothCmd, groups, 1, 1);

    // 积分 A→B 之后，每轮约束在 B→A、A→B 之间翻转。
    push.aParams1[0] = 1.0f;
    bool sourceInB = true;
    for(uint32_t iteration = 0; iteration < s_clothDesc.aIterations; ++iteration) {
        clothBarrier(s_clothCmd, VK_ACCESS_SHADER_WRITE_BIT,
                     VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
        const uint32_t setIndex = sourceInB ? 1u : 0u; // set1 = B→A, set0 = A→B
        vkCmdBindDescriptorSets(s_clothCmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                                s_clothPipelineLayout, 0, 1, &s_clothSets[setIndex], 0, nullptr);
        vkCmdPushConstants(s_clothCmd, s_clothPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                           sizeof(push), &push);
        vkCmdDispatch(s_clothCmd, groups, 1, 1);
        sourceInB = !sourceInB;
    }

    // 最终结果：积分写 B；每轮迭代翻转，迭代奇数次时结果在 A。
    const bool resultInA = (s_clothDesc.aIterations % 2u) == 1u;
    if(!resultInA) {
        VkBufferMemoryBarrier copyBarrier = {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
        copyBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        copyBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        copyBarrier.buffer = s_clothBuffers[1];
        copyBarrier.offset = 0;
        copyBarrier.size = VK_WHOLE_SIZE;
        copyBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        copyBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        vkCmdPipelineBarrier(s_clothCmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 1, &copyBarrier, 0,
                             nullptr);
        VkBufferMemoryBarrier copyDst = copyBarrier;
        copyDst.buffer = s_clothBuffers[0];
        copyDst.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        copyDst.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_HOST_READ_BIT;
        VkBufferCopy region = {};
        region.size = (VkDeviceSize)count * sizeof(GN3d::sCLOTH_GPU_VERTEX);
        vkCmdCopyBuffer(s_clothCmd, s_clothBuffers[1], s_clothBuffers[0], 1, &region);
        vkCmdPipelineBarrier(s_clothCmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_HOST_BIT, 0,
                             0, nullptr, 1, &copyDst, 0, nullptr);
    } else {
        VkBufferMemoryBarrier finalBarrier = {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
        finalBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        finalBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        finalBarrier.buffer = s_clothBuffers[0];
        finalBarrier.offset = 0;
        finalBarrier.size = VK_WHOLE_SIZE;
        finalBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        finalBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_HOST_READ_BIT;
        vkCmdPipelineBarrier(s_clothCmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_HOST_BIT, 0,
                             0, nullptr, 1, &finalBarrier, 0, nullptr);
    }

    if(vkEndCommandBuffer(s_clothCmd) != VK_SUCCESS) return false;

    VkSubmitInfo submitInfo = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &s_clothCmd;
    vkResetFences(GN_Vk.device, 1, &s_clothFence);
    if(vkQueueSubmit(GN_Vk.graphicsQueue, 1, &submitInfo, s_clothFence) != VK_SUCCESS) return false;
    if(vkWaitForFences(GN_Vk.device, 1, &s_clothFence, VK_TRUE, 2000000000ull) != VK_SUCCESS) {
        fprintf(stderr, "[GN_Vk] 布料 compute 等待超时\n");
        return false;
    }
    return true;
}

const GN3d::sCLOTH_GPU_VERTEX *GN_VkClothData(uint32_t *outCount) {
    if(outCount) *outCount = s_clothVertexCount;
    if(!s_clothMapped[0]) return 0;
    return (const GN3d::sCLOTH_GPU_VERTEX *)s_clothMapped[0];
}

VkBuffer clothGpuVertexBuffer() { return s_clothBuffers[0]; }

uint32_t clothGpuVertexCount() { return s_clothVertexCount; }

const GN3d::sCLOTH_GPU_DESC *clothGpuDesc() { return &s_clothDesc; }

// 公开释放入口（GN_Vk.h）：与内部 destroyCloth 同一实现，可重复调用。
void GN_VkClothDestroy() { destroyCloth(); }

} // namespace GNg
