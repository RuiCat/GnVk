// 3D 资源释放与绘制缓冲扩容：GN_VkShutdown3D / Ensure3DBufferCapacity。
#include <GN.h>
#include "GN_Vk.h"
#include "vkInternal.h"

#include <vector>

namespace GNg {

// B7-2（2026-11-26）：容量/偏移/退休表已收敛进 GN_Vk（c4）——vtxCap3D/idxCap3D/
// vtxOfs3D/idxOfs3D/retireTick3D 见 s3DPassRes，aRetired3DBuffers 为顶层命名成员
// （条目类型 sRETIRED_3D_BUFFER 迁 vkImageTypes.inc）。
//
// 退休表为什么存在：3D 动态缓冲扩容时，旧缓冲可能仍被“当前帧已录制的命令”引用；
// 立即 vkDestroyBuffer/vkFreeMemory 会让 GPU 在提交后读到已释放的显存
// （实测表现为 imoScene 首帧 gfx ring 页错误 → GPU reset → 合成器崩溃）。
// 因此旧缓冲先进入退休表，并且只在 GN_VkBeginFrame 的 vkWaitForFences 之后
// （上一帧提交已全部完成）释放；任何 3D pass 内都不释放。
//
// 旧实现按“pass 序号 +2 的余量”在 3D pass 开头释放：4 级 CSM 在同一个 command
// buffer 里逐级 Begin/DrawShadow，第 1 级扩容退休的缓冲会被第 3/4 级的 Begin
// 释放掉，而第 1 级已录制的 draw 仍引用它 ⇒ 同帧 UAF（真机 GPU 页错误）。

static void retire3DBuffer(VkBuffer buffer, VkDeviceMemory memory) {
    if(buffer == VK_NULL_HANDLE && memory == VK_NULL_HANDLE) return;
    sRETIRED_3D_BUFFER entry;
    entry.aBuffer = buffer;
    entry.aMemory = memory;
    entry.aRetireTick = GN_Vk.retireTick3D;
    GN_Vk.aRetired3DBuffers.push_back(entry);
}

// 只在 GN_VkBeginFrame 的 vkWaitForFences 之后调用。
// 释放上一帧（及更早）退休的缓冲；本帧退休的条目留到下一帧首，保证
// “已录制但未完成”的命令永远不会读到已释放显存。
void releaseRetired3DBuffers() {
    ++GN_Vk.retireTick3D;
    size_t kept = 0;
    for(size_t i = 0; i < GN_Vk.aRetired3DBuffers.size(); ++i) {
        const sRETIRED_3D_BUFFER &entry = GN_Vk.aRetired3DBuffers[i];
        if(entry.aRetireTick < GN_Vk.retireTick3D) {
            if(entry.aBuffer != VK_NULL_HANDLE) {
                vkDestroyBuffer(GN_Vk.device, entry.aBuffer, nullptr);
            }
            if(entry.aMemory != VK_NULL_HANDLE) {
                vkFreeMemory(GN_Vk.device, entry.aMemory, nullptr);
            }
        } else {
            GN_Vk.aRetired3DBuffers[kept++] = entry;
        }
    }
    GN_Vk.aRetired3DBuffers.resize(kept);
}

// ── 辅助: 确保 3D 顶点/索引缓冲容量 >= (当前偏移 + 本次需求) ──
// 每个 draw call 独占缓冲的一段区域, 不会覆盖同一帧内其他 draw call 的数据.
void Ensure3DBufferCapacity(uint32_t vtxSize, uint32_t idxSize) {
    VkDeviceSize needVtx = GN_Vk.vtxOfs3D + vtxSize;
    VkDeviceSize needIdx = GN_Vk.idxOfs3D + idxSize;
    // 顶点缓冲
    if(needVtx > GN_Vk.vtxCap3D) {
        retire3DBuffer(GN_Vk.vertexBuffer3D, GN_Vk.vertexMemory3D);
        GN_Vk.vertexBuffer3D = VK_NULL_HANDLE;
        GN_Vk.vertexMemory3D = VK_NULL_HANDLE;
        // 按 2x 扩容减少重分配次数
        VkDeviceSize allocVtx = needVtx * 2;
        CreateBuf(allocVtx, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                  GN_Vk.vertexBuffer3D, GN_Vk.vertexMemory3D);
        GN_Vk.vtxCap3D = allocVtx;
    }
    // 索引缓冲
    if(needIdx > GN_Vk.idxCap3D) {
        retire3DBuffer(GN_Vk.indexBuffer3D, GN_Vk.indexMemory3D);
        GN_Vk.indexBuffer3D = VK_NULL_HANDLE;
        GN_Vk.indexMemory3D = VK_NULL_HANDLE;
        VkDeviceSize allocIdx = needIdx * 2;
        CreateBuf(allocIdx, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                  GN_Vk.indexBuffer3D, GN_Vk.indexMemory3D);
        GN_Vk.idxCap3D = allocIdx;
    }
}

void GN_VkShutdown3D() {
    // 先释放扩容过程中退休的旧缓冲（此刻设备仍有效）。
    for(size_t i = 0; i < GN_Vk.aRetired3DBuffers.size(); ++i) {
        if(GN_Vk.aRetired3DBuffers[i].aBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(GN_Vk.device, GN_Vk.aRetired3DBuffers[i].aBuffer, nullptr);
        }
        if(GN_Vk.aRetired3DBuffers[i].aMemory != VK_NULL_HANDLE) {
            vkFreeMemory(GN_Vk.device, GN_Vk.aRetired3DBuffers[i].aMemory, nullptr);
        }
    }
    GN_Vk.aRetired3DBuffers.clear();

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
    // S5 / S6b：降水、贴花的动态顶点缓冲/管线布局（管线归 aPipelineCache）。
    GN_VkCloudCubeDestroy(); // L5b：立方体云 UBO/描述符集/管线布局
    GN_VkPrecipitationDestroy();
    GN_VkDecalDestroy();

    if(GN_Vk.descriptorSet3D) { GN_Vk.descriptorSet3D = VK_NULL_HANDLE; } // pool freed elsewhere
    if(GN_Vk.descriptorSetPBR) { GN_Vk.descriptorSetPBR = VK_NULL_HANDLE; }
    // 三套纹理环集缓存里的句柄随描述符池一起失效，必须清掉，否则下次 init 会
    // 拿着悬垂句柄去比对/绑定（旧 image view 也已销毁）。
    resetTextureSetCaches();
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
    // L3c：释放 4 级 shadow UBO 的常驻映射（必须先于 vkFreeMemory）。
    releaseShadowCascadeUbo();
    if(GN_Vk.shadowUBOBuffer) {
        vkDestroyBuffer(GN_Vk.device, GN_Vk.shadowUBOBuffer, 0);
        GN_Vk.shadowUBOBuffer = VK_NULL_HANDLE;
    }
    if(GN_Vk.shadowUBOMemory) {
        vkFreeMemory(GN_Vk.device, GN_Vk.shadowUBOMemory, 0);
        GN_Vk.shadowUBOMemory = VK_NULL_HANDLE;
    }
    // L3c：4 级 CSM 的层视图/framebuffer（必须在 shadowRenderPass/shadowImage 之前释放）
    destroy3DShadowCascades();
    destroy3DCubeShadow(); // B2
    GN_Vk.cubeShadowPipeline = VK_NULL_HANDLE; // VkPipeline 由 aPipelineCache 统一销毁
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
    if(GN_Vk.framebuffers3D) {
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
    destroy3DBloom(); // L1d：Bloom mip 图/fb/pass/layout
    destroy3DAtmosphereLut(); // L4c：大气 LUT 纹理/sampler
    destroy3DPost(); // L1：POST UBO（composite binding 2）
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
