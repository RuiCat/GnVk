// vkParticleDraw.cpp — P9 GPU 粒子实例化绘制：顶点 pulling 的 billboard 管线。
//
// 契约：
//   * 无顶点缓冲 / 无索引缓冲：每实例 6 顶点，quad 角点由 gl_VertexIndex 常量表给出，
//     粒子数据由 gl_InstanceIndex 直接索引 SSBO（set 0 binding 1，只读）。
//   * set 0 = binding0 UNIFORM_BUFFER（绘制参数）、binding1 STORAGE_BUFFER（粒子）；
//     描述符集在粒子 SSBO 创建后由 particleDrawBindBuffer 分配并写入。
//   * 管线经 GN_Vk.aPipelineCache（renderPass3D）：深度测试开、深度写关、双面、标准 alpha 混合。
//   * 只在 3D pass 内可用（s_in3DPass），pass 外调用返回 false 且不改任何状态。
#include <GN.h>
#include "GN_Vk.h"
#include "vkInternal.h"

#include <cstdio>
#include <cstring>
#include <vector>

namespace GNg {

namespace {

VkDescriptorSetLayout s_particleDrawSetLayout = VK_NULL_HANDLE;
VkPipelineLayout s_particleDrawPipelineLayout = VK_NULL_HANDLE;
VkPipeline s_particleDrawPipeline = VK_NULL_HANDLE;

VkBuffer s_drawUboBuffer = VK_NULL_HANDLE;
VkDeviceMemory s_drawUboMemory = VK_NULL_HANDLE;
void *s_drawUboMapped = 0;

VkDescriptorSet s_drawSet = VK_NULL_HANDLE;
VkBuffer s_boundBuffer = VK_NULL_HANDLE;
uint32_t s_boundCapacity = 0;

void releaseDrawDescriptorSet() {
    if(s_drawSet != VK_NULL_HANDLE && GN_Vk.device && GN_Vk.descriptorPool) {
        vkFreeDescriptorSets(GN_Vk.device, GN_Vk.descriptorPool, 1, &s_drawSet);
    }
    s_drawSet = VK_NULL_HANDLE;
    s_boundBuffer = VK_NULL_HANDLE;
    s_boundCapacity = 0;
}

void writeDrawDescriptorSet() {
    VkDescriptorBufferInfo ubo = {};
    ubo.buffer = s_drawUboBuffer;
    ubo.offset = 0;
    ubo.range = sizeof(GN3d::sPARTICLE_DRAW_PARAMS);
    VkDescriptorBufferInfo ssbo = {};
    ssbo.buffer = s_boundBuffer;
    ssbo.offset = 0;
    ssbo.range = VK_WHOLE_SIZE;
    VkWriteDescriptorSet writes[2] = {{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET},
                                      {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET}};
    writes[0].dstSet = s_drawSet;
    writes[0].dstBinding = 0;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[0].descriptorCount = 1;
    writes[0].pBufferInfo = &ubo;
    writes[1].dstSet = s_drawSet;
    writes[1].dstBinding = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[1].descriptorCount = 1;
    writes[1].pBufferInfo = &ssbo;
    vkUpdateDescriptorSets(GN_Vk.device, 2, writes, 0, 0);
}

bool createDrawPipeline() {
    VkDescriptorSetLayoutBinding bindings[2] = {};
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    VkDescriptorSetLayoutCreateInfo dslci = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    dslci.bindingCount = 2;
    dslci.pBindings = bindings;
    if(vkCreateDescriptorSetLayout(GN_Vk.device, &dslci, 0, &s_particleDrawSetLayout) !=
       VK_SUCCESS) {
        s_particleDrawSetLayout = VK_NULL_HANDLE;
        return false;
    }

    VkPipelineLayoutCreateInfo plci = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    plci.setLayoutCount = 1;
    plci.pSetLayouts = &s_particleDrawSetLayout;
    if(vkCreatePipelineLayout(GN_Vk.device, &plci, 0, &s_particleDrawPipelineLayout) != VK_SUCCESS) {
        s_particleDrawPipelineLayout = VK_NULL_HANDLE;
        return false;
    }

    // 固定视口管线走可重入 helper（init 与 resize 共用同一份描述）。
    return recreateParticleDrawPipeline();
}

void destroyDrawPipeline() {
    if(!GN_Vk.device) return;
    if(s_particleDrawPipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(GN_Vk.device, s_particleDrawPipelineLayout, 0);
        s_particleDrawPipelineLayout = VK_NULL_HANDLE;
    }
    if(s_particleDrawSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(GN_Vk.device, s_particleDrawSetLayout, 0);
        s_particleDrawSetLayout = VK_NULL_HANDLE;
    }
    s_particleDrawPipeline = VK_NULL_HANDLE; // 归 aPipelineCache
}

} // namespace

bool init3DParticleDraw() {
    if(s_particleDrawPipeline != VK_NULL_HANDLE) return true;
    if(!GN_Vk.device || !GN_Vk.renderPass3D) return false; // 早于 GN_Vk.initialized 置位

    CreateBuf(sizeof(GN3d::sPARTICLE_DRAW_PARAMS), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
              s_drawUboBuffer, s_drawUboMemory);
    if(s_drawUboBuffer == VK_NULL_HANDLE || s_drawUboMemory == VK_NULL_HANDLE ||
       vkMapMemory(GN_Vk.device, s_drawUboMemory, 0, VK_WHOLE_SIZE, 0, &s_drawUboMapped) !=
           VK_SUCCESS) {
        s_drawUboMapped = 0;
        destroyParticleDraw();
        return false;
    }
    if(!createDrawPipeline()) {
        destroyParticleDraw();
        return false;
    }
    return true;
}

void destroyParticleDraw() {
    releaseDrawDescriptorSet();
    destroyDrawPipeline();
    if(GN_Vk.device) {
        if(s_drawUboBuffer != VK_NULL_HANDLE) vkDestroyBuffer(GN_Vk.device, s_drawUboBuffer, 0);
        if(s_drawUboMemory != VK_NULL_HANDLE) vkFreeMemory(GN_Vk.device, s_drawUboMemory, 0);
    }
    s_drawUboBuffer = VK_NULL_HANDLE;
    s_drawUboMemory = VK_NULL_HANDLE;
    s_drawUboMapped = 0;
}

bool particleDrawBindBuffer(VkBuffer buffer, uint32_t capacity) {
    if(!GN_Vk.device || !GN_Vk.descriptorPool || !s_drawUboMapped) return false;
    if(buffer == VK_NULL_HANDLE || capacity == 0) return false;
    if(s_drawSet == VK_NULL_HANDLE) {
        VkDescriptorSetAllocateInfo dai = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        dai.descriptorPool = GN_Vk.descriptorPool;
        dai.descriptorSetCount = 1;
        dai.pSetLayouts = &s_particleDrawSetLayout;
        if(vkAllocateDescriptorSets(GN_Vk.device, &dai, &s_drawSet) != VK_SUCCESS) {
            s_drawSet = VK_NULL_HANDLE;
            return false;
        }
    }
    s_boundBuffer = buffer;
    s_boundCapacity = capacity;
    writeDrawDescriptorSet();
    return true;
}

void particleDrawUnbindBuffer() { releaseDrawDescriptorSet(); }

bool GN_VkParticleDraw(const GN3d::sPARTICLE_DRAW_PARAMS &params) {
    if(!GN_Vk.initialized || !s_in3DPass) return false;
    if(!s_drawUboMapped || s_drawSet == VK_NULL_HANDLE || s_particleDrawPipeline == VK_NULL_HANDLE) {
        return false;
    }
    if(!GN3d::validateParticleDrawParams(params)) return false;
    const uint32_t count = params.aCount < s_boundCapacity ? params.aCount : s_boundCapacity;
    if(count == 0) return false;

    GN3d::sPARTICLE_DRAW_PARAMS ubo = params;
    ubo.aCount = count;
    memcpy(s_drawUboMapped, &ubo, sizeof(ubo));

    VkCommandBuffer cb = GN_Vk.commandBuffers[GN_Vk.currentFrame];
    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, s_particleDrawPipeline);
    vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, s_particleDrawPipelineLayout, 0, 1,
                            &s_drawSet, 0, 0);
    vkCmdDraw(cb, 6, count, 0, 0);
    return true;
}

// task-26：按当前 swapchainExtent 重建粒子 billboard 固定视口管线（init 与 resize 共用）。
bool recreateParticleDrawPipeline() {
    if(s_particleDrawPipelineLayout == VK_NULL_HANDLE ||
       GN_Vk.renderPass3D == VK_NULL_HANDLE) {
        return false;
    }
    auto vertSpv = LoadSPIRVFile("assets/shaders/gn3d_particle.vert.spv");
    auto fragSpv = LoadSPIRVFile("assets/shaders/gn3d_particle.frag.spv");
    if(vertSpv.empty() || fragSpv.empty()) return false;
    GN3d::sPIPELINE_DESC desc;
    desc.aKind = GN3d::ePIPE_GRAPHICS;
    desc.aVertSpv = vertSpv.data();
    desc.aVertSize = vertSpv.size() * sizeof(uint32_t);
    desc.aFragSpv = fragSpv.data();
    desc.aFragSize = fragSpv.size() * sizeof(uint32_t);
    desc.aVertexLayout.Clear();
    desc.aVertexLayout.aStride = 0;
    desc.aRenderPass = GN_Vk.renderPass3D;
    desc.aSubpass = 0;
    desc.aLayout = s_particleDrawPipelineLayout;
    desc.aDepthTest = true;
    desc.aDepthWrite = false;
    desc.aDepthCompare = VK_COMPARE_OP_LESS;
    desc.aDepthStencil = true;
    desc.aCullBack = false;
    desc.aFrontFace = VK_FRONT_FACE_CLOCKWISE;
    desc.aBlend = GN3d::eBLEND_ALPHA;
    desc.aTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    desc.aSamples = GN_Vk.msaaSamples;
    desc.aColorAttachmentCount = 1;
    desc.aDynamicViewport = false;
    desc.aDynamicScissor = false;
    desc.aViewport = VkViewport{0.0f, 0.0f, (float)GN_Vk.swapchainExtent.width,
                                (float)GN_Vk.swapchainExtent.height, 0.0f, 1.0f};
    desc.aScissor = VkRect2D{{0, 0}, GN_Vk.swapchainExtent};
    desc.aCustomBlend = false;
    VkPipeline pipeline = GN_Vk.aPipelineCache.GetOrCreate(desc);
    if(pipeline == VK_NULL_HANDLE) {
        fprintf(stderr, "[GN_Vk] 粒子绘制管线重建失败: %s\n", GN_Vk.aPipelineCache.GetLastError());
        return false;
    }
    s_particleDrawPipeline = pipeline;
    return true;
}

} // namespace GNg
