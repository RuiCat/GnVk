// vkTestHooks.cpp — 测试钩子的填充实现。
//
// 设计取舍：这里只做「读 GN_VkContext、填 sTEST_HOOKS」，不含任何逻辑。
// 保持它足够笨，才能在后续 GN_VkContext 分组时只改这一个文件。
#include <GN.h>
#include "GN_Vk.h"

#include <cstring>

namespace GNg {
namespace {

VkPipeline pipelineById(int pipeId) {
    switch(pipeId) {
    case eTEST_PIPE_2D: return GN_Vk.pipeline;
    case eTEST_PIPE_2D_BRIGHT: return GN_Vk.pipelineBright;
    case eTEST_PIPE_2D_MULTIPLY: return GN_Vk.pipelineMultiply;
    case eTEST_PIPE_2D_NULL: return GN_Vk.pipelineNull;
    case eTEST_PIPE_3D_FORWARD: return GN_Vk.pipeline3D;
    case eTEST_PIPE_3D_PBR: return GN_Vk.pipelinePBR;
    case eTEST_PIPE_SHADOW: return GN_Vk.shadowPipeline;
    case eTEST_PIPE_COMPOSITE: return GN_Vk.compositePipeline;
    case eTEST_PIPE_SKYBOX: return GN_Vk.skyboxPipeline;
    case eTEST_PIPE_GBUFFER: return GN_Vk.gBufferPipeline;
    case eTEST_PIPE_DEFERRED_LIGHT: return GN_Vk.deferredLightPipeline;
    default: return VK_NULL_HANDLE;
    }
}

} // namespace

VkPipeline GN_VkTestPipeline(int pipeId) {
#ifdef GN_USING_3D
    if(!GN_Vk.initialized) return VK_NULL_HANDLE;
#else
    return VK_NULL_HANDLE;
#endif
    return pipelineById(pipeId);
}

void GN_VkGetTestHooks(sTEST_HOOKS *out) {
    if(out == nullptr) return;
    std::memset(out, 0, sizeof(*out));
    if(!GN_Vk.initialized) return;

    out->device = GN_Vk.device;
    out->physicalDevice = GN_Vk.physicalDevice;
    out->graphicsQueue = GN_Vk.graphicsQueue;
    out->graphicsFamily = GN_Vk.graphicsFamily;
    out->initialized = true;

    out->commandPool = GN_Vk.commandPool;
    out->descriptorPool = GN_Vk.descriptorPool;
    out->currentFrame = GN_Vk.currentFrame;
    out->commandBuffer = (GN_Vk.commandBuffers != nullptr)
                             ? GN_Vk.commandBuffers[GN_Vk.currentFrame]
                             : VK_NULL_HANDLE;

    out->extent = GN_Vk.swapchainExtent;
    out->format = GN_Vk.swapchainFormat;
    out->currentSwapImage = (GN_Vk.swapchainImages != nullptr)
                                ? GN_Vk.swapchainImages[GN_Vk.currentFrame]
                                : VK_NULL_HANDLE;

#ifdef GN_USING_3D
    out->shadowImage = GN_Vk.shadowImage;
    out->cubeShadowImage = GN_Vk.cubeShadowImage;
    out->cubeShadowView = GN_Vk.cubeShadowView;
    out->cubeShadowSize = GN_Vk.cubeShadowSize;
    out->shadowMapSize = GN_Vk.shadowMapSize;
    out->hdrImage = GN_Vk.hdrImage;
    out->gAlbedoMetalImage = GN_Vk.gAlbedoMetalImage;
    out->gNormalRoughImage = GN_Vk.gNormalRoughImage;
    out->gBufferDescriptorSet = GN_Vk.gBufferDescriptorSet;
    out->gBufferPipelineLayout = GN_Vk.gBufferPipelineLayout;
    out->vertexBuffer3D = GN_Vk.vertexBuffer3D;
    out->vertexMemory3D = GN_Vk.vertexMemory3D;
    out->indexBuffer3D = GN_Vk.indexBuffer3D;
    out->indexMemory3D = GN_Vk.indexMemory3D;
#endif

    out->pipelineCount = (unsigned)GN_Vk.aPipelineCache.GetCount();
    out->pipelineMiss = GN_Vk.aPipelineCache.GetMissCount();
    out->pipelineHit = GN_Vk.aPipelineCache.GetHitCount();
}

} // namespace GNg
