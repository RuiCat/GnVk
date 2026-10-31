// G0b：G-Buffer 四通道调试视图。复用延迟光照的 render pass / framebuffer /
// 描述符集（deferredLightDescriptorSet）与全屏三角形顶点着色器；
// 只在 3D pass 之外调用，通道越界钳制到 0..3。
#include <GN.h>
#include "GN_Vk.h"
#include "vkInternal.h"

#include <cstdio>

namespace GNg {
namespace {

bool createDebugPipelineLayout() {
    VkPushConstantRange pcr = {};
    pcr.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    pcr.offset = 0;
    pcr.size = sizeof(int);
    VkPipelineLayoutCreateInfo pl = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    pl.setLayoutCount = 1;
    pl.pSetLayouts = &GN_Vk.deferredLightDescriptorLayout;
    pl.pushConstantRangeCount = 1;
    pl.pPushConstantRanges = &pcr;
    return vkCreatePipelineLayout(GN_Vk.device, &pl, 0, &GN_Vk.gBufferDebugPipelineLayout) ==
           VK_SUCCESS;
}

// 固定视口：resize 后按新 extent 重新 GetOrCreate。
bool createDebugPipeline() {
    auto vertSpv = LoadSPIRVFile("assets/shaders/gn3d_deferred_light.vert.spv");
    auto fragSpv = LoadSPIRVFile("assets/shaders/gn3d_gbuffer_debug.frag.spv");
    if(vertSpv.empty() || fragSpv.empty()) {
        fprintf(stderr, "[GN_Vk] 调试视图缺少 SPIR-V（gn3d_deferred_light.vert / gn3d_gbuffer_debug.frag）\n");
        return false;
    }
    GN3d::sPIPELINE_DESC desc;
    desc.aKind = GN3d::ePIPE_GRAPHICS;
    desc.aVertSpv = vertSpv.data();
    desc.aVertSize = vertSpv.size() * sizeof(uint32_t);
    desc.aFragSpv = fragSpv.data();
    desc.aFragSize = fragSpv.size() * sizeof(uint32_t);
    desc.aRenderPass = GN_Vk.deferredLightRenderPass;
    desc.aSubpass = 0;
    desc.aLayout = GN_Vk.gBufferDebugPipelineLayout;
    desc.aDepthTest = false;
    desc.aDepthWrite = false;
    desc.aDepthStencil = false;
    desc.aCullBack = false;
    desc.aFrontFace = VK_FRONT_FACE_CLOCKWISE;
    desc.aBlend = GN3d::eBLEND_NONE;
    desc.aTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    desc.aSamples = VK_SAMPLE_COUNT_1_BIT;
    desc.aColorAttachmentCount = 1;
    desc.aDynamicViewport = false;
    desc.aDynamicScissor = false;
    desc.aViewport = VkViewport{0.0f, 0.0f, (float)GN_Vk.swapchainExtent.width,
                                (float)GN_Vk.swapchainExtent.height, 0.0f, 1.0f};
    desc.aScissor = VkRect2D{{0, 0}, GN_Vk.swapchainExtent};
    desc.aCustomBlend = false;
    GN_Vk.gBufferDebugPipeline = GN_Vk.aPipelineCache.GetOrCreate(desc);
    if(!GN_Vk.gBufferDebugPipeline) {
        fprintf(stderr, "[GN_Vk] 调试视图管线创建失败: %s\n", GN_Vk.aPipelineCache.GetLastError());
        return false;
    }
    return true;
}

} // namespace

void init3DGbufferDebug() {
    if(GN_Vk.gBufferDebugPipelineLayout != VK_NULL_HANDLE) return; // 幂等
    if(GN_Vk.deferredLightDescriptorLayout == VK_NULL_HANDLE ||
       GN_Vk.deferredLightRenderPass == VK_NULL_HANDLE) {
        return; // 延迟资源未就绪（着色器缺失/初始化失败）
    }
    if(!createDebugPipelineLayout()) {
        fprintf(stderr, "[GN_Vk] 警告: 调试视图管线布局创建失败（G0b）\n");
        return;
    }
    createDebugPipeline();
}

void recreate3DGbufferDebugPipeline() {
    if(GN_Vk.gBufferDebugPipelineLayout == VK_NULL_HANDLE) return;
    createDebugPipeline();
}

void destroy3DGbufferDebug() {
    GN_Vk.gBufferDebugPipeline = VK_NULL_HANDLE; // 归 aPipelineCache
    if(GN_Vk.gBufferDebugPipelineLayout) {
        vkDestroyPipelineLayout(GN_Vk.device, GN_Vk.gBufferDebugPipelineLayout, 0);
        GN_Vk.gBufferDebugPipelineLayout = VK_NULL_HANDLE;
    }
}

// 四通道调试视图：必须在 3D pass 之外调用（内部兜底检查 s_in3DPass/s_inRenderPass）。
void GN_VkDebugGBuffer(int channel) {
    if(!GN_Vk.initialized || GN_Vk.gBufferDebugPipeline == VK_NULL_HANDLE) return;
    if(s_in3DPass || s_inRenderPass) return; // 3D pass 内不允许重开同一目标
    if(channel < 0) channel = 0;
    if(channel > 3) channel = 3;
    VkCommandBuffer cb = GN_Vk.commandBuffers[GN_Vk.currentFrame];
    VkClearValue cv;
    cv.color = {{0.0f, 0.0f, 0.0f, 1.0f}};
    VkRenderPassBeginInfo rpi = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    rpi.renderPass = GN_Vk.deferredLightRenderPass;
    rpi.framebuffer = GN_Vk.deferredLightFramebuffers[GN_Vk.currentFrame];
    rpi.renderArea = {{0, 0}, GN_Vk.swapchainExtent};
    rpi.clearValueCount = 1;
    rpi.pClearValues = &cv;
    vkCmdBeginRenderPass(cb, &rpi, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, GN_Vk.gBufferDebugPipeline);
    vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, GN_Vk.gBufferDebugPipelineLayout,
                            0, 1, &GN_Vk.deferredLightDescriptorSet, 0, nullptr);
    vkCmdPushConstants(cb, GN_Vk.gBufferDebugPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                       sizeof(int), &channel);
    vkCmdDraw(cb, 3, 1, 0, 0);
    vkCmdEndRenderPass(cb);
    s_inRenderPass = false;
}

} // namespace GNg
