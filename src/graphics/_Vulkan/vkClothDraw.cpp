// vkClothDraw.cpp — P9-a GPU 布料绘制：顶点 pulling 的网格管线（SSBO + 网格索引缓冲）。
//
// 契约：
//   * 只在 3D pass 内可用；参数非法/pass 外/未创建返回 false。
//   * set 0：binding 0 = 布料顶点 SSBO（顶点着色器只读）；索引缓冲按 W×H 网格在首次绘制时生成。
//   * 管线经 GN_Vk.aPipelineCache（renderPass3D，MSAA 与既有 3D 一致）；深度测试开/写开、双面。
//   * 顶点数据来自 vkClothCompute 的 A 缓冲（GN_VkClothData 同源），缓冲重建时重写描述符集。
#include <GN.h>
#include "GN_Vk.h"
#include "vkInternal.h"

#include <cstdio>
#include <cstring>
#include <vector>

namespace GNg {

namespace {

// push constant：viewProj(64) + color(16) + params(16)，与 gn3d_cloth.vert 一致。
struct alignas(16) sCLOTH_DRAW_PUSH {
    float aViewProj[16];
    float aColor[4];
    float aParams[4]; // x = width, y = height
};

VkDescriptorSetLayout s_clothDrawSetLayout = VK_NULL_HANDLE;
VkPipelineLayout s_clothDrawPipelineLayout = VK_NULL_HANDLE;
VkPipeline s_clothDrawPipeline = VK_NULL_HANDLE;
std::vector<uint32_t> s_clothDrawVertSpv;
std::vector<uint32_t> s_clothDrawFragSpv;

VkBuffer s_clothIndexBuffer = VK_NULL_HANDLE;
VkDeviceMemory s_clothIndexMemory = VK_NULL_HANDLE;
uint32_t s_clothIndexCount = 0;

VkDescriptorSet s_clothDrawSet = VK_NULL_HANDLE;
VkBuffer s_clothDrawBoundBuffer = VK_NULL_HANDLE;

GN3d::sPIPELINE_DESC makeClothDrawPipelineDesc() {
    GN3d::sPIPELINE_DESC desc;
    desc.aKind = GN3d::ePIPE_GRAPHICS;
    desc.aVertSpv = s_clothDrawVertSpv.data();
    desc.aVertSize = s_clothDrawVertSpv.size() * sizeof(uint32_t);
    desc.aFragSpv = s_clothDrawFragSpv.data();
    desc.aFragSize = s_clothDrawFragSpv.size() * sizeof(uint32_t);
    desc.aVertexLayout.Clear(); // 顶点 pulling：无顶点属性/无顶点缓冲
    desc.aVertexLayout.aStride = 0;
    desc.aRenderPass = GN_Vk.renderPass3D;
    desc.aSubpass = 0;
    desc.aLayout = s_clothDrawPipelineLayout;
    desc.aDepthTest = true;
    desc.aDepthWrite = true;
    desc.aDepthCompare = VK_COMPARE_OP_LESS;
    desc.aDepthStencil = true;
    desc.aCullBack = false; // 布料双面可见
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

bool createClothDrawPipelines() {
    VkDescriptorSetLayoutBinding binding = {};
    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    VkDescriptorSetLayoutCreateInfo layoutInfo = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &binding;
    if(vkCreateDescriptorSetLayout(GN_Vk.device, &layoutInfo, nullptr, &s_clothDrawSetLayout) !=
       VK_SUCCESS) {
        s_clothDrawSetLayout = VK_NULL_HANDLE;
        return false;
    }

    VkPushConstantRange pushRange = {};
    pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushRange.offset = 0;
    pushRange.size = sizeof(sCLOTH_DRAW_PUSH);
    VkPipelineLayoutCreateInfo pipelineInfo = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    pipelineInfo.setLayoutCount = 1;
    pipelineInfo.pSetLayouts = &s_clothDrawSetLayout;
    pipelineInfo.pushConstantRangeCount = 1;
    pipelineInfo.pPushConstantRanges = &pushRange;
    if(vkCreatePipelineLayout(GN_Vk.device, &pipelineInfo, nullptr, &s_clothDrawPipelineLayout) !=
       VK_SUCCESS) {
        s_clothDrawPipelineLayout = VK_NULL_HANDLE;
        return false;
    }

    s_clothDrawVertSpv = LoadSPIRVFile("assets/shaders/gn3d_cloth.vert.spv");
    s_clothDrawFragSpv = LoadSPIRVFile("assets/shaders/gn3d_cloth.frag.spv");
    if(s_clothDrawVertSpv.empty() || s_clothDrawFragSpv.empty()) {
        fprintf(stderr, "[GN_Vk] 布料绘制着色器加载失败: gn3d_cloth.vert/frag.spv\n");
        return false;
    }
    GN3d::sPIPELINE_DESC desc = makeClothDrawPipelineDesc();
    s_clothDrawPipeline = GN_Vk.aPipelineCache.GetOrCreate(desc);
    if(!s_clothDrawPipeline) {
        fprintf(stderr, "[GN_Vk] 布料绘制管线创建失败: %s\n", GN_Vk.aPipelineCache.GetLastError());
        return false;
    }
    return true;
}

// 按 W×H 规则网格生成索引：每格两个三角形（绕序无关，管线双面）。
bool ensureClothIndexBuffer(const GN3d::sCLOTH_GPU_DESC &desc) {
    if(s_clothIndexBuffer != VK_NULL_HANDLE) return true;
    const uint32_t width = desc.aWidth;
    const uint32_t height = desc.aHeight;
    if(width < 2 || height < 2) return false;
    const uint32_t cells = (width - 1u) * (height - 1u);
    const uint32_t indexCount = cells * 6u;
    const VkDeviceSize bytes = (VkDeviceSize)indexCount * sizeof(uint32_t);
    CreateBuf(bytes, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
              s_clothIndexBuffer, s_clothIndexMemory);
    if(s_clothIndexBuffer == VK_NULL_HANDLE || s_clothIndexMemory == VK_NULL_HANDLE) return false;
    void *mapped = nullptr;
    if(vkMapMemory(GN_Vk.device, s_clothIndexMemory, 0, VK_WHOLE_SIZE, 0, &mapped) != VK_SUCCESS ||
       mapped == nullptr) {
        return false;
    }
    uint32_t *indices = (uint32_t *)mapped;
    uint32_t cursor = 0;
    for(uint32_t row = 0; row + 1u < height; ++row) {
        for(uint32_t column = 0; column + 1u < width; ++column) {
            const uint32_t topLeft = row * width + column;
            const uint32_t topRight = topLeft + 1u;
            const uint32_t bottomLeft = topLeft + width;
            const uint32_t bottomRight = bottomLeft + 1u;
            indices[cursor++] = topLeft;
            indices[cursor++] = bottomLeft;
            indices[cursor++] = bottomRight;
            indices[cursor++] = topLeft;
            indices[cursor++] = bottomRight;
            indices[cursor++] = topRight;
        }
    }
    vkUnmapMemory(GN_Vk.device, s_clothIndexMemory);
    s_clothIndexCount = indexCount;
    return true;
}

void releaseClothDrawIndexBuffer() {
    if(GN_Vk.device) {
        if(s_clothIndexBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(GN_Vk.device, s_clothIndexBuffer, nullptr);
        }
        if(s_clothIndexMemory != VK_NULL_HANDLE) {
            vkFreeMemory(GN_Vk.device, s_clothIndexMemory, nullptr);
        }
    }
    s_clothIndexBuffer = VK_NULL_HANDLE;
    s_clothIndexMemory = VK_NULL_HANDLE;
    s_clothIndexCount = 0;
}

bool ensureClothDrawSet(VkBuffer buffer) {
    if(s_clothDrawSet == VK_NULL_HANDLE) {
        VkDescriptorSetAllocateInfo allocInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        allocInfo.descriptorPool = GN_Vk.descriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &s_clothDrawSetLayout;
        if(vkAllocateDescriptorSets(GN_Vk.device, &allocInfo, &s_clothDrawSet) != VK_SUCCESS) {
            s_clothDrawSet = VK_NULL_HANDLE;
            return false;
        }
        s_clothDrawBoundBuffer = VK_NULL_HANDLE;
    }
    if(s_clothDrawBoundBuffer == buffer) return true;

    VkDescriptorBufferInfo bufferInfo = {};
    bufferInfo.buffer = buffer;
    bufferInfo.offset = 0;
    bufferInfo.range = VK_WHOLE_SIZE;
    VkWriteDescriptorSet write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    write.dstSet = s_clothDrawSet;
    write.dstBinding = 0;
    write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    write.descriptorCount = 1;
    write.pBufferInfo = &bufferInfo;
    vkUpdateDescriptorSets(GN_Vk.device, 1, &write, 0, 0);
    s_clothDrawBoundBuffer = buffer;
    return true;
}

} // namespace

void destroyClothDraw() {
    if(s_clothDrawSet != VK_NULL_HANDLE && GN_Vk.device && GN_Vk.descriptorPool) {
        vkFreeDescriptorSets(GN_Vk.device, GN_Vk.descriptorPool, 1, &s_clothDrawSet);
    }
    s_clothDrawSet = VK_NULL_HANDLE;
    s_clothDrawBoundBuffer = VK_NULL_HANDLE;
    releaseClothDrawIndexBuffer();
    if(GN_Vk.device) {
        if(s_clothDrawPipelineLayout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(GN_Vk.device, s_clothDrawPipelineLayout, nullptr);
        }
        if(s_clothDrawSetLayout != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(GN_Vk.device, s_clothDrawSetLayout, nullptr);
        }
    }
    s_clothDrawPipelineLayout = VK_NULL_HANDLE;
    s_clothDrawSetLayout = VK_NULL_HANDLE;
    s_clothDrawPipeline = VK_NULL_HANDLE; // 归 aPipelineCache
    s_clothDrawVertSpv.clear();
    s_clothDrawFragSpv.clear();
}

bool GN_VkClothDraw(const sMAT44 &viewProj, float r, float g, float b) {
    if(!GN_Vk.initialized || !s_in3DPass) return false;
    const GN3d::sCLOTH_GPU_DESC *desc = clothGpuDesc();
    const VkBuffer buffer = clothGpuVertexBuffer();
    const uint32_t count = clothGpuVertexCount();
    if(!desc || buffer == VK_NULL_HANDLE || count == 0) return false;
    if((uint64_t)desc->aWidth * (uint64_t)desc->aHeight != (uint64_t)count) return false;

    if(s_clothDrawPipeline == VK_NULL_HANDLE) {
        if(!createClothDrawPipelines()) {
            destroyClothDraw();
            return false;
        }
    }
    if(!ensureClothIndexBuffer(*desc)) return false;
    if(!ensureClothDrawSet(buffer)) return false;

    sCLOTH_DRAW_PUSH push = {};
    memcpy(push.aViewProj, viewProj.m, sizeof(push.aViewProj));
    push.aColor[0] = r;
    push.aColor[1] = g;
    push.aColor[2] = b;
    push.aColor[3] = 1.0f;
    push.aParams[0] = (float)desc->aWidth;
    push.aParams[1] = (float)desc->aHeight;

    VkCommandBuffer commandBuffer = GN_Vk.commandBuffers[GN_Vk.currentFrame];
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, s_clothDrawPipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            s_clothDrawPipelineLayout, 0, 1, &s_clothDrawSet, 0, nullptr);
    vkCmdPushConstants(commandBuffer, s_clothDrawPipelineLayout,
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                       sizeof(push), &push);
    vkCmdBindIndexBuffer(commandBuffer, s_clothIndexBuffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(commandBuffer, s_clothIndexCount, 1, 0, 0, 0);
    return true;
}

// task-26：按当前 swapchainExtent 重建布料固定视口管线（init 与 resize 共用）。
bool recreateClothDrawPipeline() {
    if(s_clothDrawPipelineLayout == VK_NULL_HANDLE) return false;
    if(s_clothDrawVertSpv.empty() || s_clothDrawFragSpv.empty()) return false;
    GN3d::sPIPELINE_DESC desc = makeClothDrawPipelineDesc();
    VkPipeline pipeline = GN_Vk.aPipelineCache.GetOrCreate(desc);
    if(pipeline == VK_NULL_HANDLE) {
        fprintf(stderr, "[GN_Vk] 布料绘制管线重建失败: %s\n", GN_Vk.aPipelineCache.GetLastError());
        return false;
    }
    s_clothDrawPipeline = pipeline;
    return true;
}

} // namespace GNg
