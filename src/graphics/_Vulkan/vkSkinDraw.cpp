// vkSkinDraw.cpp — P2 GPU 蒙皮绘制：蒙皮管线、关节调色板描述符与索引绘制。
//
// 契约：
//   * 顶点输入 = GN3d::sSKINNED_VERTEX（68 字节，loc0..loc5：pos/normal/uv/tangent/joints/weights）。
//   * set 0：binding 0 = 场景 UBO（VkSceneUBO，复用 uniformBuffer3D），binding 1 = 关节调色板 8KB UBO。
//   * 调色板每帧写入环形槽位（按 GN_Vk.currentFrame 选择，见 vkSkin.cpp），不覆盖在飞帧的数据；
//     每个槽位各有一个描述符集，避免在命令缓冲在飞时更新描述符集。
//   * 管线经 GN_Vk.aPipelineCache 创建（键含 SPIR-V 内容），renderPass = renderPass3D。
//   * 蒙皮在模型空间完成后再乘 modelMatrix；frag 只做 albedo × Lambert 近似。
#include <GN.h>
#include "GN_Vk.h"
#include "GN_3DSkin.h"
#include "vkInternal.h"

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <vector>

namespace GNg {

// 调色板环形槽位（实现在 vkSkin.cpp）：槽位数、按帧选槽、槽缓冲与写入。
GN_VK_INTERNAL uint32_t skinPaletteSlotCount();
GN_VK_INTERNAL uint32_t skinPaletteSlotForFrame(uint32_t frame);
GN_VK_INTERNAL bool skinPaletteSlotReady();
GN_VK_INTERNAL VkBuffer skinPaletteSlotBuffer(uint32_t slot);
GN_VK_INTERNAL VkDeviceSize skinPaletteSlotBytes();
GN_VK_INTERNAL bool writeSkinPalette(uint32_t slot, const GN3d::sSKIN_UBO &palette);

namespace {

// 蒙皮 push constant：模型矩阵 + albedo（与 gn3d_skinned.vert/.frag 的 ModelPC 逐字段一致）。
struct alignas(16) sSKIN_PUSH_CONST {
    float aModel[16];
    float aColor[4];
};

VkDescriptorSetLayout s_skinDescriptorLayout = VK_NULL_HANDLE;
VkPipelineLayout s_skinPipelineLayout = VK_NULL_HANDLE;
VkPipeline s_skinPipeline = VK_NULL_HANDLE;
std::vector<VkDescriptorSet> s_skinDescriptorSets;

// 着色器字节必须在管线缓存键计算期间保持有效（缓存创建时读取，探测时重算键）。
std::vector<uint32_t> s_skinVertSpv;
std::vector<uint32_t> s_skinFragSpv;

// 构造蒙皮管线描述：固定 set 0 布局、68 字节顶点布局、renderPass3D、MSAA 4x + 深度测试。
GN3d::sPIPELINE_DESC makeSkinPipelineDesc() {
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
    desc.aVertSpv = s_skinVertSpv.data();
    desc.aVertSize = s_skinVertSpv.size() * sizeof(uint32_t);
    desc.aFragSpv = s_skinFragSpv.data();
    desc.aFragSize = s_skinFragSpv.size() * sizeof(uint32_t);
    desc.aVertexLayout = layout;
    desc.aRenderPass = GN_Vk.renderPass3D;
    desc.aSubpass = 0;
    desc.aLayout = s_skinPipelineLayout;
    desc.aDepthTest = true;
    desc.aDepthWrite = true;
    desc.aDepthCompare = VK_COMPARE_OP_LESS;
    desc.aDepthStencil = true;
    desc.aCullBack = false; // 验收四边形两种绕序都要可见，避免绕序影响像素判定
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

// 创建 set 0 布局（binding 0 场景 UBO，binding 1 调色板 UBO）与含 80B push constant 的管线布局。
bool createSkinLayouts() {
    if(s_skinDescriptorLayout != VK_NULL_HANDLE || s_skinPipelineLayout != VK_NULL_HANDLE) return true;

    VkDescriptorSetLayoutBinding bindings[2] = {};
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    layoutInfo.bindingCount = 2;
    layoutInfo.pBindings = bindings;
    if(vkCreateDescriptorSetLayout(GN_Vk.device, &layoutInfo, nullptr, &s_skinDescriptorLayout) != VK_SUCCESS) {
        return false;
    }

    VkPushConstantRange pushRange = {};
    pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushRange.offset = 0;
    pushRange.size = sizeof(sSKIN_PUSH_CONST);

    VkPipelineLayoutCreateInfo pipelineInfo = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    pipelineInfo.setLayoutCount = 1;
    pipelineInfo.pSetLayouts = &s_skinDescriptorLayout;
    pipelineInfo.pushConstantRangeCount = 1;
    pipelineInfo.pPushConstantRanges = &pushRange;
    if(vkCreatePipelineLayout(GN_Vk.device, &pipelineInfo, nullptr, &s_skinPipelineLayout) != VK_SUCCESS) {
        vkDestroyDescriptorSetLayout(GN_Vk.device, s_skinDescriptorLayout, nullptr);
        s_skinDescriptorLayout = VK_NULL_HANDLE;
        return false;
    }
    return true;
}

// 每个调色板槽位分配一个描述符集：binding 0 指向场景 UBO，binding 1 指向该槽位的调色板 UBO。
bool createSkinDescriptorSets() {
    if(!skinPaletteSlotReady()) return false;
    const uint32_t slotCount = skinPaletteSlotCount();
    if(slotCount == 0) return false;

    std::vector<VkDescriptorSetLayout> layouts(slotCount, s_skinDescriptorLayout);
    std::vector<VkDescriptorSet> sets(slotCount, VK_NULL_HANDLE);
    VkDescriptorSetAllocateInfo allocInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    allocInfo.descriptorPool = GN_Vk.descriptorPool;
    allocInfo.descriptorSetCount = slotCount;
    allocInfo.pSetLayouts = layouts.data();
    if(vkAllocateDescriptorSets(GN_Vk.device, &allocInfo, sets.data()) != VK_SUCCESS) return false;

    for(uint32_t slot = 0; slot < slotCount; ++slot) {
        VkDescriptorBufferInfo sceneInfo = {GN_Vk.uniformBuffer3D, 0, sizeof(VkSceneUBO)};
        VkDescriptorBufferInfo paletteInfo = {skinPaletteSlotBuffer(slot), 0, skinPaletteSlotBytes()};
        VkWriteDescriptorSet writes[2] = {
            {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET}, {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET}};
        writes[0].dstSet = sets[slot];
        writes[0].dstBinding = 0;
        writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writes[0].descriptorCount = 1;
        writes[0].pBufferInfo = &sceneInfo;
        writes[1].dstSet = sets[slot];
        writes[1].dstBinding = 1;
        writes[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writes[1].descriptorCount = 1;
        writes[1].pBufferInfo = &paletteInfo;
        vkUpdateDescriptorSets(GN_Vk.device, 2, writes, 0, nullptr);
    }
    s_skinDescriptorSets = sets;
    return true;
}

// 经管线缓存创建蒙皮管线；缓存键含 SPIR-V 内容与全部固定态字段。
bool createSkinPipeline() {
    GN3d::sPIPELINE_DESC desc = makeSkinPipelineDesc();
    bool created = false;
    s_skinPipeline = GN_Vk.aPipelineCache.GetOrCreate(desc, &created);
    if(s_skinPipeline == VK_NULL_HANDLE) {
        std::fprintf(stderr, "[GN_Vk] 蒙皮管线创建失败: %s\n", GN_Vk.aPipelineCache.GetLastError());
        return false;
    }
    return true;
}

} // namespace

// 初始化蒙皮子系统：加载着色器、创建调色板环形缓冲/描述符集/管线。失败返回 false（只告警）。
bool init3DSkinned() {
    if(GN_Vk.device == VK_NULL_HANDLE) return false;
    if(s_skinPipeline != VK_NULL_HANDLE) return true;

    s_skinVertSpv = LoadSPIRVFile("assets/shaders/gn3d_skinned.vert.spv");
    s_skinFragSpv = LoadSPIRVFile("assets/shaders/gn3d_skinned.frag.spv");
    if(s_skinVertSpv.empty() || s_skinFragSpv.empty()) {
        std::fprintf(stderr, "[GN_Vk] 蒙皮着色器缺失（assets/shaders/gn3d_skinned.*.spv）\n");
        return false;
    }

    if(!createSkinPaletteBuffer() || !createSkinLayouts() || !createSkinDescriptorSets() ||
       !createSkinPipeline()) {
        destroySkinned();
        return false;
    }
    return true;
}

// 释放蒙皮子系统；调色板缓冲/布局在此销毁，管线归 aPipelineCache 所有（由缓存统一 Release）。
void destroySkinned() {
    if(GN_Vk.device != VK_NULL_HANDLE) {
        if(s_skinPipelineLayout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(GN_Vk.device, s_skinPipelineLayout, nullptr);
        }
        if(s_skinDescriptorLayout != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(GN_Vk.device, s_skinDescriptorLayout, nullptr);
        }
    }
    s_skinPipelineLayout = VK_NULL_HANDLE;
    s_skinDescriptorLayout = VK_NULL_HANDLE;
    s_skinPipeline = VK_NULL_HANDLE;
    s_skinDescriptorSets.clear();
    s_skinVertSpv.clear();
    s_skinFragSpv.clear();
    destroySkinPaletteBuffer();
}

// 测试钩子：返回当前蒙皮管线句柄（VK_NULL_HANDLE = 未创建）。
VkPipeline GN_VkSkinPipeline() { return s_skinPipeline; }

// 测试钩子：用同一描述再次向 aPipelineCache 请求蒙皮管线，命中（created=false 且句柄一致）
// 返回 true —— 证明管线确实经缓存创建且缓存键稳定。
bool GN_VkSkinPipelineProbe(VkPipeline *outPipeline) {
    if(outPipeline) *outPipeline = s_skinPipeline;
    if(s_skinPipeline == VK_NULL_HANDLE || s_skinPipelineLayout == VK_NULL_HANDLE) return false;

    GN3d::sPIPELINE_DESC desc = makeSkinPipelineDesc();
    bool created = true;
    VkPipeline pipeline = GN_Vk.aPipelineCache.GetOrCreate(desc, &created);
    if(outPipeline) *outPipeline = pipeline;
    return pipeline != VK_NULL_HANDLE && pipeline == s_skinPipeline && !created;
}

// 绘制蒙皮网格：打包调色板到当前帧槽位后按 sSKINNED_VERTEX 提交。
// 未初始化/不在 3D pass 内/参数为空/调色板写入失败返回 false（不崩溃）。
bool GN_VkDraw3DSkinned(const void *vertices, uint32_t vertexCount,
                        const uint32_t *indices, uint32_t indexCount,
                        const sMAT44 *palette, uint32_t paletteCount,
                        const sMAT44 *modelMatrix,
                        float r, float g, float b) {
    if(!GN_Vk.initialized || !s_in3DPass) return false;
    if(vertices == nullptr || vertexCount == 0 || palette == nullptr || paletteCount == 0 ||
       modelMatrix == nullptr) {
        return false;
    }
    if(s_skinPipeline == VK_NULL_HANDLE || s_skinPipelineLayout == VK_NULL_HANDLE) return false;

    const uint32_t slot = skinPaletteSlotForFrame(GN_Vk.currentFrame);
    if(slot >= s_skinDescriptorSets.size()) return false;

    GN3d::sSKIN_UBO paletteUbo;
    if(GN3d::packPalette(palette, paletteCount, paletteUbo) == 0) return false;
    if(!writeSkinPalette(slot, paletteUbo)) return false;

    const uint32_t vtxSize = vertexCount * (uint32_t)sizeof(GN3d::sSKINNED_VERTEX);
    const uint32_t idxSize = (indices && indexCount > 0) ? indexCount * (uint32_t)sizeof(uint32_t) : 0;
    Ensure3DBufferCapacity(vtxSize, idxSize);

    VkCommandBuffer commandBuffer = GN_Vk.commandBuffers[GN_Vk.currentFrame];
    void *mapped = nullptr;
    if(vkMapMemory(GN_Vk.device, GN_Vk.vertexMemory3D, s_vtxOfs3D, vtxSize, 0, &mapped) != VK_SUCCESS ||
       mapped == nullptr) {
        return false;
    }
    std::memcpy(mapped, vertices, vtxSize);
    vkUnmapMemory(GN_Vk.device, GN_Vk.vertexMemory3D);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, s_skinPipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, s_skinPipelineLayout,
                            0, 1, &s_skinDescriptorSets[slot], 0, nullptr);

    sSKIN_PUSH_CONST pushConst;
    std::memcpy(pushConst.aModel, modelMatrix->m, sizeof(pushConst.aModel));
    pushConst.aColor[0] = r;
    pushConst.aColor[1] = g;
    pushConst.aColor[2] = b;
    pushConst.aColor[3] = 1.0f;
    vkCmdPushConstants(commandBuffer, s_skinPipelineLayout,
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(pushConst), &pushConst);

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
        std::memcpy(indexMapped, indices, idxSize);
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
