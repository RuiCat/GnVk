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
    // G0b：调试视图管线布局（管线本体归 aPipelineCache，已先于本函数释放）。
    destroy3DGbufferDebug();
    // G5b-2：探针 9× 3D 纹理/sampler/UBO（固定尺寸，不随 resize）。
    destroy3DProbes();
    // G4：SSGI 半分辨率目标/管线/描述符（管线归 aPipelineCache）。
    destroy3DSsgi();
    // G2b：SSAO 目标/管线/描述符（管线归 aPipelineCache，这里释放图像/布局/UBO/采样器）。
    destroy3DSsao();
    // G1a：环境烘焙/预滤波 cubemap/BRDF LUT/GI UBO（先于描述符池与设备销毁；管线归缓存）。
    destroy3DEnvironment();
    // P1c / P2 / P8：先释放新增子系统的缓冲/布局（此时 device 仍有效）
    destroyMaterial();
    destroySkinned();
    destroyDebugLines();
    // P9：先释放粒子 SSBO/描述符集，再销毁两半的管线布局。
    destroyParticle();
    destroyParticleDraw();
    // P9 剩余片：SSBO 蒙皮与 GPU 布料（管线归缓存，这里释放缓冲/描述符集/布局）。
    destroySkinnedSsbo();
    destroyCloth();
    // P11-S3：雾 UBO（描述符集随池释放，这里只销毁缓冲/内存）。
    destroyAtmosphere();
    // P11-S2b / S5 / S6b：云层、降水、贴花的动态顶点缓冲/管线布局（管线归 aPipelineCache）。
    GN_VkCloudDestroy();
    GN_VkPrecipitationDestroy();
    GN_VkDecalDestroy();

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
    // PBR 管线由 cPIPELINE_CACHE 持有，统一在 GN_VkShutdown 的 Release 中销毁
    if(GN_Vk.pipelinePBR) {
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
    // Shadow（管线由 cPIPELINE_CACHE 持有）
    if(GN_Vk.shadowPipeline) {
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
    // 3D 前向管线由 cPIPELINE_CACHE 持有，统一在 GN_VkShutdown 的 Release 中销毁
    if(GN_Vk.pipeline3D) {
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
        destroy3DForwardFramebuffers();
    }
    if(GN_Vk.renderPass3D) {
        vkDestroyRenderPass(GN_Vk.device, GN_Vk.renderPass3D, 0);
        GN_Vk.renderPass3D = VK_NULL_HANDLE;
    }
    // task-8：MSAA/depth 目标走与 resize 相同的 destroy helper（深度视图/图像/内存 + MSAA 三者）。
    if(GN_Vk.depthView || GN_Vk.depthImage || GN_Vk.depthMemory || GN_Vk.msaaColorView ||
       GN_Vk.msaaColorImage || GN_Vk.msaaColorMemory) {
        destroy3DMsaaTargets();
    }
    // HDR + Composite
    if(GN_Vk.compositeDescriptorSet) { GN_Vk.compositeDescriptorSet = VK_NULL_HANDLE; }
    if(GN_Vk.compositePipeline) {
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
    destroy3DCompositeFramebuffers();
    if(GN_Vk.compositeRenderPass) {
        vkDestroyRenderPass(GN_Vk.device, GN_Vk.compositeRenderPass, 0);
        GN_Vk.compositeRenderPass = VK_NULL_HANDLE;
    }
    if(GN_Vk.hdrSampler || GN_Vk.hdrView || GN_Vk.hdrImage || GN_Vk.hdrMemory ||
       GN_Vk.hdrFramebuffers) {
        destroy3DHdrTargets();
    }
    if(GN_Vk.hdrRenderPass) {
        vkDestroyRenderPass(GN_Vk.device, GN_Vk.hdrRenderPass, 0);
        GN_Vk.hdrRenderPass = VK_NULL_HANDLE;
    }
    // Skybox
    if(GN_Vk.skyboxPipeline) {
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
    if(GN_Vk.deferredLightPipeline) { GN_Vk.deferredLightPipeline = VK_NULL_HANDLE; }
    if(GN_Vk.deferredLightPipelineLayout) { vkDestroyPipelineLayout(GN_Vk.device, GN_Vk.deferredLightPipelineLayout, 0); }
    if(GN_Vk.deferredLightDescriptorLayout) { vkDestroyDescriptorSetLayout(GN_Vk.device, GN_Vk.deferredLightDescriptorLayout, 0); }
    if(GN_Vk.deferredLightFramebuffers) {
        destroy3DDeferredLightFramebuffers();
    }
    if(GN_Vk.deferredLightRenderPass) { vkDestroyRenderPass(GN_Vk.device, GN_Vk.deferredLightRenderPass, 0); }
    // G-Buffer 统一销毁（vkGbuffer.cpp）：图像/视图/内存/framebuffer/render pass/布局/采样器一处释放。
    destroy3DGbuffer();
}

} // namespace GNg
