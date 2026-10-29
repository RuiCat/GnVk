// vkDebugDraw.cpp — P8 调试线框：把 CPU 线段列表展开成世界坐标顶点，写进 3D 动态顶点缓冲后按 LINE_LIST 绘制。
//
// 契约：
//   * 输入为 GN3d::sDEBUG_LINE 数组（世界坐标，两个端点）；颜色统一由调用方给（0xAARRGGBB）。
//   * 复用 3D pass 的场景 UBO（set 0 binding 0）与深度附件；深度测试开、深度写关，线框不参与遮挡。
//   * 管线经 GN_Vk.aPipelineCache 创建（renderPass3D）；资源在 init/destroy 成对释放，不修改输入。
#include <GN.h>
#include "GN_Vk.h"
#include "vkInternal.h"

#include <cstddef>
#include <cstdio>

namespace GNg {
namespace {

// 内部顶点：世界坐标 + 展开后的 RGBA，与 gn3d_debug.vert 的 location 0/1 一一对应。
struct sDEBUG_VERTEX {
    float aPosition[3];
    float aColor[4];
};

// 本模块持有的 GPU 资源；VkPipeline 归 aPipelineCache 所有，这里只存句柄用于绘制。
VkPipelineLayout s_debugPipelineLayout = VK_NULL_HANDLE;
VkDescriptorSetLayout s_debugDescriptorLayout = VK_NULL_HANDLE;
VkDescriptorSet s_debugDescriptorSet = VK_NULL_HANDLE;
VkPipeline s_debugPipeline = VK_NULL_HANDLE;

// 0xAARRGGBB → RGBA 浮点（每通道 /255）。
void unpackColor(unsigned int color, float *rgba) {
    rgba[0] = (float)((color >> 16) & 0xFFu) / 255.0f;
    rgba[1] = (float)((color >> 8) & 0xFFu) / 255.0f;
    rgba[2] = (float)(color & 0xFFu) / 255.0f;
    rgba[3] = (float)((color >> 24) & 0xFFu) / 255.0f;
}

// 把一条线段写成两个内部顶点（起点、终点），颜色相同。
void writeSegment(sDEBUG_VERTEX *outStart, sDEBUG_VERTEX *outEnd,
                  const GN3d::sDEBUG_LINE &line, const float *rgba) {
    outStart->aPosition[0] = line.aStart.x;
    outStart->aPosition[1] = line.aStart.y;
    outStart->aPosition[2] = line.aStart.z;
    outEnd->aPosition[0] = line.aEnd.x;
    outEnd->aPosition[1] = line.aEnd.y;
    outEnd->aPosition[2] = line.aEnd.z;
    for(int i = 0; i < 4; ++i) {
        outStart->aColor[i] = rgba[i];
        outEnd->aColor[i] = rgba[i];
    }
}

} // namespace

// 创建调试线框的 set 0 布局、管线布局、描述符集与 LINE_LIST 管线；失败返回 false。
bool init3DDebugLines() {
    // 注意：GN_VkInit3D() 在 GN_Vk.initialized = true 之前调用本函数，不能检查 initialized。
    if(GN_Vk.device == VK_NULL_HANDLE || GN_Vk.renderPass3D == VK_NULL_HANDLE ||
       GN_Vk.uniformBuffer3D == VK_NULL_HANDLE) {
        return false;
    }

    const std::vector<uint32_t> vertSpv = LoadSPIRVFile("assets/shaders/gn3d_debug.vert.spv");
    const std::vector<uint32_t> fragSpv = LoadSPIRVFile("assets/shaders/gn3d_debug.frag.spv");
    if(vertSpv.empty() || fragSpv.empty()) return false;

    // set 0 binding 0 = 场景 UBO（顶点阶段读 viewProj）。
    {
        VkDescriptorSetLayoutBinding binding = {};
        binding.binding = 0;
        binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        binding.descriptorCount = 1;
        binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        VkDescriptorSetLayoutCreateInfo layoutInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        layoutInfo.bindingCount = 1;
        layoutInfo.pBindings = &binding;
        if(vkCreateDescriptorSetLayout(GN_Vk.device, &layoutInfo, 0, &s_debugDescriptorLayout) != VK_SUCCESS) {
            destroyDebugLines();
            return false;
        }
    }
    {
        VkPipelineLayoutCreateInfo layoutInfo = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        layoutInfo.setLayoutCount = 1;
        layoutInfo.pSetLayouts = &s_debugDescriptorLayout;
        if(vkCreatePipelineLayout(GN_Vk.device, &layoutInfo, 0, &s_debugPipelineLayout) != VK_SUCCESS) {
            destroyDebugLines();
            return false;
        }
    }
    {
        VkDescriptorSetAllocateInfo allocInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        allocInfo.descriptorPool = GN_Vk.descriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &s_debugDescriptorLayout;
        if(vkAllocateDescriptorSets(GN_Vk.device, &allocInfo, &s_debugDescriptorSet) != VK_SUCCESS) {
            destroyDebugLines();
            return false;
        }
        VkDescriptorBufferInfo sceneInfo = {GN_Vk.uniformBuffer3D, 0, sizeof(VkSceneUBO)};
        VkWriteDescriptorSet write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        write.dstSet = s_debugDescriptorSet;
        write.dstBinding = 0;
        write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        write.descriptorCount = 1;
        write.pBufferInfo = &sceneInfo;
        vkUpdateDescriptorSets(GN_Vk.device, 1, &write, 0, 0);
    }

    GN3d::sVERTEX_LAYOUT vertexLayout;
    vertexLayout.Add(0, (uint32_t)offsetof(sDEBUG_VERTEX, aPosition), VK_FORMAT_R32G32B32_SFLOAT);
    vertexLayout.Add(1, (uint32_t)offsetof(sDEBUG_VERTEX, aColor), VK_FORMAT_R32G32B32A32_SFLOAT);
    vertexLayout.aStride = (uint32_t)sizeof(sDEBUG_VERTEX);

    const uint32_t width = GN_Vk.swapchainExtent.width;
    const uint32_t height = GN_Vk.swapchainExtent.height;
    GN3d::sPIPELINE_DESC desc;
    desc.aKind = GN3d::ePIPE_GRAPHICS;
    desc.aVertSpv = vertSpv.data();
    desc.aVertSize = vertSpv.size() * sizeof(uint32_t);
    desc.aFragSpv = fragSpv.data();
    desc.aFragSize = fragSpv.size() * sizeof(uint32_t);
    desc.aVertexLayout = vertexLayout;
    desc.aRenderPass = GN_Vk.renderPass3D;
    desc.aSubpass = 0;
    desc.aLayout = s_debugPipelineLayout;
    desc.aDepthTest = true; // 线框参与深度比较
    desc.aDepthWrite = false; // 不写深度：后续绘制不被线框遮挡
    desc.aDepthCompare = VK_COMPARE_OP_LESS_OR_EQUAL;
    desc.aDepthStencil = true;
    desc.aCullBack = false; // 线段无正反面
    desc.aFrontFace = VK_FRONT_FACE_CLOCKWISE;
    desc.aBlend = GN3d::eBLEND_ALPHA; // 允许 0xAARRGGBB 的 alpha 生效
    desc.aTopology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
    desc.aSamples = GN_Vk.msaaSamples;
    desc.aColorAttachmentCount = 1;
    desc.aDynamicViewport = false;
    desc.aDynamicScissor = false;
    desc.aViewport = VkViewport{0.0f, 0.0f, (float)width, (float)height, 0.0f, 1.0f};
    desc.aScissor = VkRect2D{{0, 0}, {width, height}};
    desc.aCustomBlend = false;

    s_debugPipeline = GN_Vk.aPipelineCache.GetOrCreate(desc);
    if(!s_debugPipeline) {
        std::fprintf(stderr, "[GN_Vk] 调试线框管线创建失败: %s\n", GN_Vk.aPipelineCache.GetLastError());
        destroyDebugLines();
        return false;
    }
    return true;
}

// 释放调试线框的管线布局与描述符布局；可重复调用（管线归缓存、描述符集归描述符池）。
void destroyDebugLines() {
    if(GN_Vk.device != VK_NULL_HANDLE) {
        if(s_debugPipelineLayout) {
            vkDestroyPipelineLayout(GN_Vk.device, s_debugPipelineLayout, 0);
        }
        if(s_debugDescriptorLayout) {
            vkDestroyDescriptorSetLayout(GN_Vk.device, s_debugDescriptorLayout, 0);
        }
    }
    s_debugPipelineLayout = VK_NULL_HANDLE;
    s_debugDescriptorLayout = VK_NULL_HANDLE;
    s_debugDescriptorSet = VK_NULL_HANDLE; // 描述符池由 GN_VkShutdown 统一销毁
    s_debugPipeline = VK_NULL_HANDLE; // VkPipeline 由 aPipelineCache 统一销毁
}

// 在已开启的 3D pass 内绘制 lineCount 条线段；未初始化/不在 pass 内/空输入时安全返回。
void GN_VkDrawDebugLines(const void *lines, uint32_t lineCount, unsigned int color) {
    if(!GN_Vk.initialized || !s_in3DPass || lines == 0 || lineCount == 0) return;
    if(s_debugPipeline == VK_NULL_HANDLE || s_debugPipelineLayout == VK_NULL_HANDLE) return;

    const GN3d::sDEBUG_LINE *segments = (const GN3d::sDEBUG_LINE *)lines;
    const uint32_t vertexCount = lineCount * 2u;
    const uint32_t vertexSize = vertexCount * (uint32_t)sizeof(sDEBUG_VERTEX);

    Ensure3DBufferCapacity(vertexSize, 0);
    if(GN_Vk.vertexBuffer3D == VK_NULL_HANDLE || GN_Vk.vertexMemory3D == VK_NULL_HANDLE) return;

    float rgba[4];
    unpackColor(color, rgba);

    void *mapped = 0;
    vkMapMemory(GN_Vk.device, GN_Vk.vertexMemory3D, s_vtxOfs3D, vertexSize, 0, &mapped);
    if(mapped == 0) return;
    sDEBUG_VERTEX *out = (sDEBUG_VERTEX *)mapped;
    for(uint32_t i = 0; i < lineCount; ++i) {
        writeSegment(&out[i * 2u], &out[i * 2u + 1u], segments[i], rgba);
    }
    vkUnmapMemory(GN_Vk.device, GN_Vk.vertexMemory3D);

    VkCommandBuffer cmd = GN_Vk.commandBuffers[GN_Vk.currentFrame];
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, s_debugPipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, s_debugPipelineLayout,
                            0, 1, &s_debugDescriptorSet, 0, 0);
    VkDeviceSize offset = s_vtxOfs3D;
    vkCmdBindVertexBuffers(cmd, 0, 1, &GN_Vk.vertexBuffer3D, &offset);
    s_vtxOfs3D += vertexSize;
    vkCmdDraw(cmd, vertexCount, 1, 0, 0);
}

} // namespace GNg
