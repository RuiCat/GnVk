// 3D 资源释放与绘制缓冲扩容：GN_VkShutdown3D / Ensure3DBufferCapacity。
#include <GN.h>
#include "GN_Vk.h"
#include "vkInternal.h"

namespace GNg {

// ── 3D 绘制缓冲容量追踪 (共享于 GN_VkDraw3D / GN_VkDraw3DPBR / GN_VkShadowDraw) ──
static uint32_t s_vtxCap3D = 0;
static uint32_t s_idxCap3D = 0;

// ── 3D 缓冲写入偏移 (同一帧内多个 draw call 写入不同区域, 避免相互覆盖) ──
GN_VK_INTERNAL VkDeviceSize s_vtxOfs3D = 0;
GN_VK_INTERNAL VkDeviceSize s_idxOfs3D = 0;

// ── 辅助: 确保 3D 顶点/索引缓冲容量 >= (当前偏移 + 本次需求) ──
// 每个 draw call 独占缓冲的一段区域, 不会覆盖同一帧内其他 draw call 的数据.
void Ensure3DBufferCapacity(uint32_t vtxSize, uint32_t idxSize) {
    VkDeviceSize needVtx = s_vtxOfs3D + vtxSize;
    VkDeviceSize needIdx = s_idxOfs3D + idxSize;
    // 顶点缓冲
    if(needVtx > s_vtxCap3D) {
        if(GN_Vk.vertexBuffer3D) {
            vkDestroyBuffer(GN_Vk.device, GN_Vk.vertexBuffer3D, nullptr);
            GN_Vk.vertexBuffer3D = VK_NULL_HANDLE;
        }
        if(GN_Vk.vertexMemory3D) {
            vkFreeMemory(GN_Vk.device, GN_Vk.vertexMemory3D, nullptr);
            GN_Vk.vertexMemory3D = VK_NULL_HANDLE;
        }
        // 按 2x 扩容减少重分配次数
        VkDeviceSize allocVtx = needVtx * 2;
        CreateBuf(allocVtx, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                  GN_Vk.vertexBuffer3D, GN_Vk.vertexMemory3D);
        s_vtxCap3D = allocVtx;
    }
    // 索引缓冲
    if(needIdx > s_idxCap3D) {
        if(GN_Vk.indexBuffer3D) {
            vkDestroyBuffer(GN_Vk.device, GN_Vk.indexBuffer3D, nullptr);
            GN_Vk.indexBuffer3D = VK_NULL_HANDLE;
        }
        if(GN_Vk.indexMemory3D) {
            vkFreeMemory(GN_Vk.device, GN_Vk.indexMemory3D, nullptr);
            GN_Vk.indexMemory3D = VK_NULL_HANDLE;
        }
        VkDeviceSize allocIdx = needIdx * 2;
        CreateBuf(allocIdx, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                  GN_Vk.indexBuffer3D, GN_Vk.indexMemory3D);
        s_idxCap3D = allocIdx;
    }
}

void GN_VkShutdown3D() {
    if(GN_Vk.descriptorSet3D) { GN_Vk.descriptorSet3D = VK_NULL_HANDLE; } // pool freed elsewhere
    if(GN_Vk.descriptorSetPBR) { GN_Vk.descriptorSetPBR = VK_NULL_HANDLE; }
    if(GN_Vk.uniformBuffer3D) {
        vkDestroyBuffer(GN_Vk.device, GN_Vk.uniformBuffer3D, 0);
        GN_Vk.uniformBuffer3D = VK_NULL_HANDLE;
    }
    if(GN_Vk.uniformMemory3D) {
        vkFreeMemory(GN_Vk.device, GN_Vk.uniformMemory3D, 0);
        GN_Vk.uniformMemory3D = VK_NULL_HANDLE;
    }
    if(GN_Vk.lightBuffer3D) {
        vkDestroyBuffer(GN_Vk.device, GN_Vk.lightBuffer3D, 0);
        GN_Vk.lightBuffer3D = VK_NULL_HANDLE;
    }
    if(GN_Vk.lightMemory3D) {
        vkFreeMemory(GN_Vk.device, GN_Vk.lightMemory3D, 0);
        GN_Vk.lightMemory3D = VK_NULL_HANDLE;
    }
    // PBR
    if(GN_Vk.pipelinePBR) {
        vkDestroyPipeline(GN_Vk.device, GN_Vk.pipelinePBR, 0);
        GN_Vk.pipelinePBR = VK_NULL_HANDLE;
    }
    if(GN_Vk.pipelineLayoutPBR) {
        vkDestroyPipelineLayout(GN_Vk.device, GN_Vk.pipelineLayoutPBR, 0);
        GN_Vk.pipelineLayoutPBR = VK_NULL_HANDLE;
    }
    if(GN_Vk.descriptorLayoutPBR) {
        vkDestroyDescriptorSetLayout(GN_Vk.device, GN_Vk.descriptorLayoutPBR, 0);
        GN_Vk.descriptorLayoutPBR = VK_NULL_HANDLE;
    }
    // Shadow
    if(GN_Vk.shadowPipeline) {
        vkDestroyPipeline(GN_Vk.device, GN_Vk.shadowPipeline, 0);
        GN_Vk.shadowPipeline = VK_NULL_HANDLE;
    }
    if(GN_Vk.shadowPipelineLayout) {
        vkDestroyPipelineLayout(GN_Vk.device, GN_Vk.shadowPipelineLayout, 0);
        GN_Vk.shadowPipelineLayout = VK_NULL_HANDLE;
    }
    if(GN_Vk.shadowDescriptorLayout) {
        vkDestroyDescriptorSetLayout(GN_Vk.device, GN_Vk.shadowDescriptorLayout, 0);
        GN_Vk.shadowDescriptorLayout = VK_NULL_HANDLE;
    }
    if(GN_Vk.shadowUBOBuffer) {
        vkDestroyBuffer(GN_Vk.device, GN_Vk.shadowUBOBuffer, 0);
        GN_Vk.shadowUBOBuffer = VK_NULL_HANDLE;
    }
    if(GN_Vk.shadowUBOMemory) {
        vkFreeMemory(GN_Vk.device, GN_Vk.shadowUBOMemory, 0);
        GN_Vk.shadowUBOMemory = VK_NULL_HANDLE;
    }
    if(GN_Vk.shadowFramebuffer) {
        vkDestroyFramebuffer(GN_Vk.device, GN_Vk.shadowFramebuffer, 0);
        GN_Vk.shadowFramebuffer = VK_NULL_HANDLE;
    }
    if(GN_Vk.shadowRenderPass) {
        vkDestroyRenderPass(GN_Vk.device, GN_Vk.shadowRenderPass, 0);
        GN_Vk.shadowRenderPass = VK_NULL_HANDLE;
    }
    if(GN_Vk.shadowSampler) {
        vkDestroySampler(GN_Vk.device, GN_Vk.shadowSampler, 0);
        GN_Vk.shadowSampler = VK_NULL_HANDLE;
    }
    if(GN_Vk.shadowView) {
        vkDestroyImageView(GN_Vk.device, GN_Vk.shadowView, 0);
        GN_Vk.shadowView = VK_NULL_HANDLE;
    }
    if(GN_Vk.shadowImage) {
        vkDestroyImage(GN_Vk.device, GN_Vk.shadowImage, 0);
        GN_Vk.shadowImage = VK_NULL_HANDLE;
    }
    if(GN_Vk.shadowMemory) {
        vkFreeMemory(GN_Vk.device, GN_Vk.shadowMemory, 0);
        GN_Vk.shadowMemory = VK_NULL_HANDLE;
    }
    if(GN_Vk.pipeline3D) {
        vkDestroyPipeline(GN_Vk.device, GN_Vk.pipeline3D, 0);
        GN_Vk.pipeline3D = VK_NULL_HANDLE;
    }
    if(GN_Vk.pipelineLayout3D) {
        vkDestroyPipelineLayout(GN_Vk.device, GN_Vk.pipelineLayout3D, 0);
        GN_Vk.pipelineLayout3D = VK_NULL_HANDLE;
    }
    if(GN_Vk.descriptorLayout3D) {
        vkDestroyDescriptorSetLayout(GN_Vk.device, GN_Vk.descriptorLayout3D, 0);
        GN_Vk.descriptorLayout3D = VK_NULL_HANDLE;
    }
    if(s_framebuffers3D) {
        for(uint32_t i = 0; i < GN_Vk.swapchainImageCount; i++)
            if(s_framebuffers3D[i]) vkDestroyFramebuffer(GN_Vk.device, s_framebuffers3D[i], 0);
        delete[] s_framebuffers3D;
        s_framebuffers3D = nullptr;
    }
    if(GN_Vk.renderPass3D) {
        vkDestroyRenderPass(GN_Vk.device, GN_Vk.renderPass3D, 0);
        GN_Vk.renderPass3D = VK_NULL_HANDLE;
    }
    if(GN_Vk.depthView) {
        vkDestroyImageView(GN_Vk.device, GN_Vk.depthView, 0);
        GN_Vk.depthView = VK_NULL_HANDLE;
    }
    if(GN_Vk.depthImage) {
        vkDestroyImage(GN_Vk.device, GN_Vk.depthImage, 0);
        GN_Vk.depthImage = VK_NULL_HANDLE;
    }
    if(GN_Vk.depthMemory) {
        vkFreeMemory(GN_Vk.device, GN_Vk.depthMemory, 0);
        GN_Vk.depthMemory = VK_NULL_HANDLE;
    }
    // MSAA
    if(GN_Vk.msaaColorView) {
        vkDestroyImageView(GN_Vk.device, GN_Vk.msaaColorView, 0);
        GN_Vk.msaaColorView = VK_NULL_HANDLE;
    }
    if(GN_Vk.msaaColorImage) {
        vkDestroyImage(GN_Vk.device, GN_Vk.msaaColorImage, 0);
        GN_Vk.msaaColorImage = VK_NULL_HANDLE;
    }
    if(GN_Vk.msaaColorMemory) {
        vkFreeMemory(GN_Vk.device, GN_Vk.msaaColorMemory, 0);
        GN_Vk.msaaColorMemory = VK_NULL_HANDLE;
    }
    // HDR + Composite
    if(GN_Vk.compositeDescriptorSet) { GN_Vk.compositeDescriptorSet = VK_NULL_HANDLE; }
    if(GN_Vk.compositePipeline) {
        vkDestroyPipeline(GN_Vk.device, GN_Vk.compositePipeline, 0);
        GN_Vk.compositePipeline = VK_NULL_HANDLE;
    }
    if(GN_Vk.compositePipelineLayout) {
        vkDestroyPipelineLayout(GN_Vk.device, GN_Vk.compositePipelineLayout, 0);
        GN_Vk.compositePipelineLayout = VK_NULL_HANDLE;
    }
    if(GN_Vk.compositeDescriptorLayout) {
        vkDestroyDescriptorSetLayout(GN_Vk.device, GN_Vk.compositeDescriptorLayout, 0);
        GN_Vk.compositeDescriptorLayout = VK_NULL_HANDLE;
    }
    if(GN_Vk.compositeFramebuffers) {
        for(uint32_t i = 0; i < GN_Vk.swapchainImageCount; i++)
            if(GN_Vk.compositeFramebuffers[i]) vkDestroyFramebuffer(GN_Vk.device, GN_Vk.compositeFramebuffers[i], 0);
        delete[] GN_Vk.compositeFramebuffers;
        GN_Vk.compositeFramebuffers = nullptr;
    }
    if(GN_Vk.compositeRenderPass) {
        vkDestroyRenderPass(GN_Vk.device, GN_Vk.compositeRenderPass, 0);
        GN_Vk.compositeRenderPass = VK_NULL_HANDLE;
    }
    if(GN_Vk.hdrSampler) {
        vkDestroySampler(GN_Vk.device, GN_Vk.hdrSampler, 0);
        GN_Vk.hdrSampler = VK_NULL_HANDLE;
    }
    if(GN_Vk.hdrView) {
        vkDestroyImageView(GN_Vk.device, GN_Vk.hdrView, 0);
        GN_Vk.hdrView = VK_NULL_HANDLE;
    }
    if(GN_Vk.hdrImage) {
        vkDestroyImage(GN_Vk.device, GN_Vk.hdrImage, 0);
        GN_Vk.hdrImage = VK_NULL_HANDLE;
    }
    if(GN_Vk.hdrMemory) {
        vkFreeMemory(GN_Vk.device, GN_Vk.hdrMemory, 0);
        GN_Vk.hdrMemory = VK_NULL_HANDLE;
    }
    if(GN_Vk.hdrFramebuffers) {
        for(uint32_t i = 0; i < GN_Vk.swapchainImageCount; i++)
            if(GN_Vk.hdrFramebuffers[i]) vkDestroyFramebuffer(GN_Vk.device, GN_Vk.hdrFramebuffers[i], 0);
        delete[] GN_Vk.hdrFramebuffers;
        GN_Vk.hdrFramebuffers = nullptr;
    }
    if(GN_Vk.hdrRenderPass) {
        vkDestroyRenderPass(GN_Vk.device, GN_Vk.hdrRenderPass, 0);
        GN_Vk.hdrRenderPass = VK_NULL_HANDLE;
    }
    // Skybox
    if(GN_Vk.skyboxPipeline) {
        vkDestroyPipeline(GN_Vk.device, GN_Vk.skyboxPipeline, 0);
        GN_Vk.skyboxPipeline = VK_NULL_HANDLE;
    }
    if(GN_Vk.skyboxPipelineLayout) {
        vkDestroyPipelineLayout(GN_Vk.device, GN_Vk.skyboxPipelineLayout, 0);
        GN_Vk.skyboxPipelineLayout = VK_NULL_HANDLE;
    }
    if(GN_Vk.skyboxDescriptorLayout) {
        vkDestroyDescriptorSetLayout(GN_Vk.device, GN_Vk.skyboxDescriptorLayout, 0);
        GN_Vk.skyboxDescriptorLayout = VK_NULL_HANDLE;
    }
    if(GN_Vk.skyboxUBOBuffer) {
        vkDestroyBuffer(GN_Vk.device, GN_Vk.skyboxUBOBuffer, 0);
        GN_Vk.skyboxUBOBuffer = VK_NULL_HANDLE;
    }
    if(GN_Vk.skyboxUBOMemory) {
        vkFreeMemory(GN_Vk.device, GN_Vk.skyboxUBOMemory, 0);
        GN_Vk.skyboxUBOMemory = VK_NULL_HANDLE;
    }
    if(GN_Vk.skyboxSampler) {
        vkDestroySampler(GN_Vk.device, GN_Vk.skyboxSampler, 0);
        GN_Vk.skyboxSampler = VK_NULL_HANDLE;
    }
    if(GN_Vk.skyboxView) {
        vkDestroyImageView(GN_Vk.device, GN_Vk.skyboxView, 0);
        GN_Vk.skyboxView = VK_NULL_HANDLE;
    }
    if(GN_Vk.skyboxImage) {
        vkDestroyImage(GN_Vk.device, GN_Vk.skyboxImage, 0);
        GN_Vk.skyboxImage = VK_NULL_HANDLE;
    }
    if(GN_Vk.skyboxMemory) {
        vkFreeMemory(GN_Vk.device, GN_Vk.skyboxMemory, 0);
        GN_Vk.skyboxMemory = VK_NULL_HANDLE;
    }
    // Deferred
    if(GN_Vk.deferredLightPipeline) { vkDestroyPipeline(GN_Vk.device, GN_Vk.deferredLightPipeline, 0); }
    if(GN_Vk.deferredLightPipelineLayout) { vkDestroyPipelineLayout(GN_Vk.device, GN_Vk.deferredLightPipelineLayout, 0); }
    if(GN_Vk.deferredLightDescriptorLayout) { vkDestroyDescriptorSetLayout(GN_Vk.device, GN_Vk.deferredLightDescriptorLayout, 0); }
    if(GN_Vk.deferredLightFramebuffers) {
        for(uint32_t i = 0; i < GN_Vk.swapchainImageCount; i++)
            if(GN_Vk.deferredLightFramebuffers[i]) vkDestroyFramebuffer(GN_Vk.device, GN_Vk.deferredLightFramebuffers[i], 0);
        delete[] GN_Vk.deferredLightFramebuffers;
    }
    if(GN_Vk.deferredLightRenderPass) { vkDestroyRenderPass(GN_Vk.device, GN_Vk.deferredLightRenderPass, 0); }
    if(GN_Vk.gBufferPipeline) { vkDestroyPipeline(GN_Vk.device, GN_Vk.gBufferPipeline, 0); }
    if(GN_Vk.gBufferPipelineLayout) { vkDestroyPipelineLayout(GN_Vk.device, GN_Vk.gBufferPipelineLayout, 0); }
    if(GN_Vk.gBufferDescriptorLayout) { vkDestroyDescriptorSetLayout(GN_Vk.device, GN_Vk.gBufferDescriptorLayout, 0); }
    if(GN_Vk.gBufferSampler) { vkDestroySampler(GN_Vk.device, GN_Vk.gBufferSampler, 0); }
    if(GN_Vk.gBufferFramebuffers) { delete[] GN_Vk.gBufferFramebuffers; }
    if(GN_Vk.gBufferRenderPass) { vkDestroyRenderPass(GN_Vk.device, GN_Vk.gBufferRenderPass, 0); }
    {
        VkImageView *vs[] = {&GN_Vk.gAlbedoView, &GN_Vk.gNormalView, &GN_Vk.gPBRView, &GN_Vk.gDepthView};
        VkImage *ims[] = {&GN_Vk.gAlbedoImage, &GN_Vk.gNormalImage, &GN_Vk.gPBRImage, &GN_Vk.gDepthImage};
        VkDeviceMemory *ms[] = {&GN_Vk.gAlbedoMemory, &GN_Vk.gNormalMemory, &GN_Vk.gPBRMemory, &GN_Vk.gDepthMemory};
        for(int i = 0; i < 4; i++) {
            if(*vs[i]) {
                vkDestroyImageView(GN_Vk.device, *vs[i], 0);
                *vs[i] = VK_NULL_HANDLE;
            }
            if(*ims[i]) {
                vkDestroyImage(GN_Vk.device, *ims[i], 0);
                *ims[i] = VK_NULL_HANDLE;
            }
            if(*ms[i]) {
                vkFreeMemory(GN_Vk.device, *ms[i], 0);
                *ms[i] = VK_NULL_HANDLE;
            }
        }
    }
}

} // namespace GNg
