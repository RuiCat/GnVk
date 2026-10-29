// vkSkinSsbo.cpp — P9-b SSBO 调色板蒙皮：关节矩阵存 SSBO，突破 128 关节 UBO 上限。
//
// 契约：
//   * 顶点格式 = GN3d::sSKINNED_VERTEX（68B，与 P2 UBO 路径相同）。
//   * set 0：binding 0 = 场景 UBO（GN_Vk.uniformBuffer3D），binding 1 = SSBO `mat4 palette[]`
//     （capacity 4096；索引在着色器内钳到 4095）。
//   * 调色板按 3 个环形槽位写入（槽位随帧号 + 本帧绘制计数轮转），避免在飞帧被覆盖。
//   * 管线经 GN_Vk.aPipelineCache 创建（renderPass3D、MSAA 与既有 3D 一致）。
//   * 惰性初始化：首次绘制时创建（只查 device，不依赖 GN_Vk.initialized 的置位顺序）。
#include <GN.h>
#include "GN_Vk.h"
#include "GN_3DSkin.h"
#include "vkInternal.h"

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <vector>

namespace GNg {

namespace {

const uint32_t SKIN_SSBO_MAX_JOINTS = 4096;
const uint32_t SKIN_SSBO_SLOTS = 3;
const VkDeviceSize SKIN_SSBO_BYTES =
    (VkDeviceSize)SKIN_SSBO_MAX_JOINTS * (VkDeviceSize)sizeof(sMAT44);

// push constant：模型矩阵 + albedo（与 gn3d_skinned_ssbo.vert/.frag 的 ModelPC 一致）。
struct alignas(16) sSKIN_SSBO_PUSH {
    float aModel[16];
    float aColor[4];
};

VkDescriptorSetLayout s_ssboSetLayout = VK_NULL_HANDLE;
VkPipelineLayout s_ssboPipelineLayout = VK_NULL_HANDLE;
VkPipeline s_ssboPipeline = VK_NULL_HANDLE;
std::vector<uint32_t> s_ssboVertSpv;
std::vector<uint32_t> s_ssboFragSpv;

VkBuffer s_paletteBuffers[SKIN_SSBO_SLOTS] = {};
VkDeviceMemory s_paletteMemories[SKIN_SSBO_SLOTS] = {};
void *s_paletteMapped[SKIN_SSBO_SLOTS] = {};
VkDescriptorSet s_paletteSets[SKIN_SSBO_SLOTS] = {};
uint32_t s_slotCursor = 0;

GN3d::sPIPELINE_DESC makeSkinSsboPipelineDesc() {
    GN3d::sVERTEX_LAYOUT layout;
    layout.Add(0, (uint32_t)offsetof(GN3d::sSKINNED_VERTEX, aPosition), VK_FORMAT_R32G32B32_SFLOAT);
    layout.Add(1, (uint32_t)offsetof(GN3d::sSKINNED_VERTEX, aNormal), VK_FORMAT_R32G32B32_SFLOAT);
    layout.Add(2, (uint32_t)offsetof(GN3d::sSKINNED_VERTEX, aUv), VK_FORMAT_R32G32_SFLOAT);
    layout.Add(3, (uint32_t)offsetof(GN3d::sSKINNED_VERTEX, aTangent), VK_FORMAT_R32G32B32_SFLOAT);
    layout.Add(4, (uint32_t)offsetof(GN3d::sSKINNED_VERTEX, aJoints), VK_FORMAT_R16G16B16A16_UINT);
    layout.Add(5, (uint32_t)offsetof(GN3d::sSKINNED_VERTEX, aWeights), VK_FORMAT_R32G32B32A32_SFLOAT);
    layout.aStride = (uint32_t)sizeof(GN3d::sSKINNED_VERTEX);

    GN3d::sPIPELINE_DESC desc;
    desc.aKind = GN3d::ePIPE_GRAPHICS;
    desc.aVertSpv = s_ssboVertSpv.data();
    desc.aVertSize = s_ssboVertSpv.size() * sizeof(uint32_t);
    desc.aFragSpv = s_ssboFragSpv.data();
    desc.aFragSize = s_ssboFragSpv.size() * sizeof(uint32_t);
    desc.aVertexLayout = layout;
    desc.aRenderPass = GN_Vk.renderPass3D;
    desc.aSubpass = 0;
    desc.aLayout = s_ssboPipelineLayout;
    desc.aDepthTest = true;
    desc.aDepthWrite = true;
    desc.aDepthCompare = VK_COMPARE_OP_LESS;
    desc.aDepthStencil = true;
    desc.aCullBack = false; // 与 P2 蒙皮路径一致：验收四边形两种绕序都可见
    desc.aFrontFace = VK_FRONT_FACE_CLOCKWISE; // 世界 CCW + Y 翻转
    desc.aBlend = GN3d::eBLEND_NONE;
    desc.aTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    desc.aSamples = GN_Vk.msaaSamples;
    desc.aColorAttachmentCount = 1;
    desc.aDynamicViewport = false;
    desc.aDynamicScissor = false;
    desc.aViewport = VkViewport{0.0f, 0.0f, (float)GN_Vk.swapchainExtent.width,
                                (float)GN_Vk.swapchainExtent.height, 0.0f, 1.0f};
    desc.aScissor = VkRect2D{{0, 0}, GN_Vk.swapchainExtent};
    desc.aCustomBlend = false;
    return desc;
}

bool createSkinSsboLayouts() {
    VkDescriptorSetLayoutBinding bindings[2] = {};
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    layoutInfo.bindingCount = 2;
    layoutInfo.pBindings = bindings;
    if(vkCreateDescriptorSetLayout(GN_Vk.device, &layoutInfo, nullptr, &s_ssboSetLayout) !=
       VK_SUCCESS) {
        s_ssboSetLayout = VK_NULL_HANDLE;
        return false;
    }

    VkPushConstantRange pushRange = {};
    pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushRange.offset = 0;
    pushRange.size = sizeof(sSKIN_SSBO_PUSH);
    VkPipelineLayoutCreateInfo pipelineInfo = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    pipelineInfo.setLayoutCount = 1;
    pipelineInfo.pSetLayouts = &s_ssboSetLayout;
    pipelineInfo.pushConstantRangeCount = 1;
    pipelineInfo.pPushConstantRanges = &pushRange;
    if(vkCreatePipelineLayout(GN_Vk.device, &pipelineInfo, nullptr, &s_ssboPipelineLayout) !=
       VK_SUCCESS) {
        vkDestroyDescriptorSetLayout(GN_Vk.device, s_ssboSetLayout, nullptr);
        s_ssboSetLayout = VK_NULL_HANDLE;
        s_ssboPipelineLayout = VK_NULL_HANDLE;
        return false;
    }
    return true;
}

bool createSkinSsboPaletteSlot(uint32_t slot, VkDescriptorSetLayout setLayout,
                               VkPipelineLayout pipelineLayout) {
    CreateBuf(SKIN_SSBO_BYTES, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
              s_paletteBuffers[slot], s_paletteMemories[slot]);
    if(s_paletteBuffers[slot] == VK_NULL_HANDLE || s_paletteMemories[slot] == VK_NULL_HANDLE) {
        return false;
    }
    if(vkMapMemory(GN_Vk.device, s_paletteMemories[slot], 0, VK_WHOLE_SIZE, 0,
                   &s_paletteMapped[slot]) != VK_SUCCESS) {
        s_paletteMapped[slot] = 0;
        return false;
    }
    memset(s_paletteMapped[slot], 0, (size_t)SKIN_SSBO_BYTES);

    VkDescriptorSetAllocateInfo allocInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    allocInfo.descriptorPool = GN_Vk.descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &setLayout;
    if(vkAllocateDescriptorSets(GN_Vk.device, &allocInfo, &s_paletteSets[slot]) != VK_SUCCESS) {
        s_paletteSets[slot] = VK_NULL_HANDLE;
        return false;
    }

    VkDescriptorBufferInfo sceneInfo = {};
    sceneInfo.buffer = GN_Vk.uniformBuffer3D;
    sceneInfo.offset = 0;
    sceneInfo.range = sizeof(VkSceneUBO);
    VkDescriptorBufferInfo paletteInfo = {};
    paletteInfo.buffer = s_paletteBuffers[slot];
    paletteInfo.offset = 0;
    paletteInfo.range = VK_WHOLE_SIZE;
    VkWriteDescriptorSet writes[2] = {{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET},
                                      {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET}};
    writes[0].dstSet = s_paletteSets[slot];
    writes[0].dstBinding = 0;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[0].descriptorCount = 1;
    writes[0].pBufferInfo = &sceneInfo;
    writes[1].dstSet = s_paletteSets[slot];
    writes[1].dstBinding = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[1].descriptorCount = 1;
    writes[1].pBufferInfo = &paletteInfo;
    vkUpdateDescriptorSets(GN_Vk.device, 2, writes, 0, 0);
    (void)pipelineLayout;
    return true;
}

} // namespace

bool init3DSkinnedSsbo() {
    if(s_ssboPipeline != VK_NULL_HANDLE) return true;
    if(!GN_Vk.device || !GN_Vk.renderPass3D || !GN_Vk.descriptorPool) return false;

    s_ssboVertSpv = LoadSPIRVFile("assets/shaders/gn3d_skinned_ssbo.vert.spv");
    s_ssboFragSpv = LoadSPIRVFile("assets/shaders/gn3d_skinned_ssbo.frag.spv");
    if(s_ssboVertSpv.empty() || s_ssboFragSpv.empty()) {
        fprintf(stderr, "[GN_Vk] SSBO 蒙皮着色器加载失败: gn3d_skinned_ssbo.vert/frag.spv\n");
        return false;
    }
    if(!createSkinSsboLayouts()) {
        destroySkinnedSsbo();
        return false;
    }
    for(uint32_t slot = 0; slot < SKIN_SSBO_SLOTS; ++slot) {
        if(!createSkinSsboPaletteSlot(slot, s_ssboSetLayout, s_ssboPipelineLayout)) {
            destroySkinnedSsbo();
            return false;
        }
    }

    GN3d::sPIPELINE_DESC desc = makeSkinSsboPipelineDesc();
    s_ssboPipeline = GN_Vk.aPipelineCache.GetOrCreate(desc);
    if(!s_ssboPipeline) {
        fprintf(stderr, "[GN_Vk] SSBO 蒙皮管线创建失败: %s\n", GN_Vk.aPipelineCache.GetLastError());
        destroySkinnedSsbo();
        return false;
    }
    return true;
}

void destroySkinnedSsbo() {
    if(GN_Vk.device) {
        for(uint32_t slot = 0; slot < SKIN_SSBO_SLOTS; ++slot) {
            if(s_paletteSets[slot] != VK_NULL_HANDLE && GN_Vk.descriptorPool) {
                vkFreeDescriptorSets(GN_Vk.device, GN_Vk.descriptorPool, 1, &s_paletteSets[slot]);
            }
            s_paletteSets[slot] = VK_NULL_HANDLE;
            if(s_paletteBuffers[slot] != VK_NULL_HANDLE) {
                vkDestroyBuffer(GN_Vk.device, s_paletteBuffers[slot], nullptr);
            }
            if(s_paletteMemories[slot] != VK_NULL_HANDLE) {
                vkFreeMemory(GN_Vk.device, s_paletteMemories[slot], nullptr);
            }
            s_paletteBuffers[slot] = VK_NULL_HANDLE;
            s_paletteMemories[slot] = VK_NULL_HANDLE;
            s_paletteMapped[slot] = 0;
        }
        if(s_ssboPipelineLayout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(GN_Vk.device, s_ssboPipelineLayout, nullptr);
        }
        if(s_ssboSetLayout != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(GN_Vk.device, s_ssboSetLayout, nullptr);
        }
    }
    s_ssboPipelineLayout = VK_NULL_HANDLE;
    s_ssboSetLayout = VK_NULL_HANDLE;
    s_ssboPipeline = VK_NULL_HANDLE; // 管线归 aPipelineCache 所有
    s_ssboVertSpv.clear();
    s_ssboFragSpv.clear();
    s_slotCursor = 0;
}

bool GN_VkDraw3DSkinnedSsbo(const void *vertices, uint32_t vertexCount,
                            const uint32_t *indices, uint32_t indexCount,
                            const sMAT44 *palette, uint32_t paletteCount,
                            const sMAT44 *modelMatrix,
                            float r, float g, float b) {
    if(!GN_Vk.initialized || !s_in3DPass) return false;
    if(vertices == nullptr || vertexCount == 0 || palette == nullptr || modelMatrix == nullptr) {
        return false;
    }
    if(paletteCount == 0 || paletteCount > SKIN_SSBO_MAX_JOINTS) return false;
    if(!init3DSkinnedSsbo()) return false; // 惰性初始化（幂等）

    const uint32_t slot = (GN_Vk.currentFrame + s_slotCursor++) % SKIN_SSBO_SLOTS;
    if(s_paletteMapped[slot] == 0 || s_paletteSets[slot] == VK_NULL_HANDLE) return false;
    memcpy(s_paletteMapped[slot], palette, (size_t)paletteCount * sizeof(sMAT44));

    const uint32_t vtxSize = vertexCount * (uint32_t)sizeof(GN3d::sSKINNED_VERTEX);
    const uint32_t idxSize =
        (indices && indexCount > 0) ? indexCount * (uint32_t)sizeof(uint32_t) : 0;
    Ensure3DBufferCapacity(vtxSize, idxSize);

    VkCommandBuffer commandBuffer = GN_Vk.commandBuffers[GN_Vk.currentFrame];
    void *mapped = nullptr;
    if(vkMapMemory(GN_Vk.device, GN_Vk.vertexMemory3D, s_vtxOfs3D, vtxSize, 0, &mapped) !=
           VK_SUCCESS ||
       mapped == nullptr) {
        return false;
    }
    memcpy(mapped, vertices, vtxSize);
    vkUnmapMemory(GN_Vk.device, GN_Vk.vertexMemory3D);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, s_ssboPipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, s_ssboPipelineLayout, 0,
                            1, &s_paletteSets[slot], 0, nullptr);

    sSKIN_SSBO_PUSH pushConst;
    memcpy(pushConst.aModel, modelMatrix->m, sizeof(pushConst.aModel));
    pushConst.aColor[0] = r;
    pushConst.aColor[1] = g;
    pushConst.aColor[2] = b;
    pushConst.aColor[3] = 1.0f;
    vkCmdPushConstants(commandBuffer, s_ssboPipelineLayout,
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                       sizeof(pushConst), &pushConst);

    VkDeviceSize vertexOffset = s_vtxOfs3D;
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &GN_Vk.vertexBuffer3D, &vertexOffset);
    s_vtxOfs3D += vtxSize;

    if(idxSize > 0) {
        void *indexMapped = nullptr;
        if(vkMapMemory(GN_Vk.device, GN_Vk.indexMemory3D, s_idxOfs3D, idxSize, 0, &indexMapped) !=
               VK_SUCCESS ||
           indexMapped == nullptr) {
            return false;
        }
        memcpy(indexMapped, indices, idxSize);
        vkUnmapMemory(GN_Vk.device, GN_Vk.indexMemory3D);
        vkCmdBindIndexBuffer(commandBuffer, GN_Vk.indexBuffer3D, s_idxOfs3D, VK_INDEX_TYPE_UINT32);
        s_idxOfs3D += idxSize;
        vkCmdDrawIndexed(commandBuffer, indexCount, 1, 0, 0, 0);
    } else {
        vkCmdDraw(commandBuffer, vertexCount, 1, 0, 0);
    }
    return true;
}

} // namespace GNg
