// 3D 绘制 pass：阴影映射、HDR→Swapchain 合成、天空盒、PBR（含 HDR 变体）与延迟渲染。
#include <GN.h>
#include "GN_Vk.h"
#include "vkInternal.h"

#include <cstring>

namespace GNg {

bool s_inShadowPass = false;

namespace {
// 天空参数：GN_VkSetSky 写入，GN_VkDrawSkybox 每帧连同 viewProj 一起上传。
VkSkyParams s_skyParams;
// G2b：延迟 pass 最近一次 viewProj（SSAO 用它求 invViewProj 写 UBO）。
static sMAT44 s_deferredViewProj;
static bool s_hasDeferredViewProj = false;
} // namespace

// ═══════════════════════════════════════════════════════════
//  阴影映射 Pass
// ═══════════════════════════════════════════════════════════

void GN_VkShadowPassBegin(const sMAT44 *lightViewProj) {
    if(!GN_Vk.initialized || !GN_Vk.shadowPipeline || s_inShadowPass) return;
    if(s_in3DPass) return;
    s_vtxOfs3D = 0;
    s_idxOfs3D = 0;

    // 刷新 2D 批次并结束当前渲染通道
    if(gVCnt > 0) {
        void *d;
        vkMapMemory(GN_Vk.device, GN_Vk.vertexMemory, 0, gVCnt * sizeof(VkVertex), 0, &d);
        memcpy(d, gVBuf, gVCnt * sizeof(VkVertex));
        vkUnmapMemory(GN_Vk.device, GN_Vk.vertexMemory);
        uint32_t prevEnd = 0;
        for(uint32_t i = 0; i < s_batchCount; i++) {
            uint32_t count = s_batchEnd[i] - prevEnd;
            if(count > 0) {
                vkCmdBindDescriptorSets(GN_Vk.commandBuffers[GN_Vk.currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, GN_Vk.pipelineLayout, 0, 1, &s_batchDS[i], 0, 0);
                VkDeviceSize offs[] = {prevEnd * sizeof(VkVertex)};
                vkCmdBindVertexBuffers(GN_Vk.commandBuffers[GN_Vk.currentFrame], 0, 1, &GN_Vk.vertexBuffer, offs);
                vkCmdDraw(GN_Vk.commandBuffers[GN_Vk.currentFrame], count, 1, 0, 0);
            }
            prevEnd = s_batchEnd[i];
        }
        gVCnt = 0;
        s_batchCount = 0;
        s_lastDS = VK_NULL_HANDLE;
    }
    if(s_inRenderPass) {
        vkCmdEndRenderPass(GN_Vk.commandBuffers[GN_Vk.currentFrame]);
        s_inRenderPass = false;
    }

    // 上传 light VP
    {
        void *d;
        vkMapMemory(GN_Vk.device, GN_Vk.shadowUBOMemory, 0, 64, 0, &d);
        memcpy(d, lightViewProj->m, 64);
        vkUnmapMemory(GN_Vk.device, GN_Vk.shadowUBOMemory);
    }

    VkClearValue cv;
    cv.depthStencil = {1.0f, 0};
    VkRenderPassBeginInfo rpi = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    rpi.renderPass = GN_Vk.shadowRenderPass;
    rpi.framebuffer = GN_Vk.shadowFramebuffer;
    rpi.renderArea = {{0, 0}, {GN_Vk.shadowMapSize, GN_Vk.shadowMapSize}};
    rpi.clearValueCount = 1;
    rpi.pClearValues = &cv;
    vkCmdBeginRenderPass(GN_Vk.commandBuffers[GN_Vk.currentFrame], &rpi, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(GN_Vk.commandBuffers[GN_Vk.currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, GN_Vk.shadowPipeline);
    vkCmdBindDescriptorSets(GN_Vk.commandBuffers[GN_Vk.currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                            GN_Vk.shadowPipelineLayout, 0, 1, &GN_Vk.shadowDescriptorSet, 0, 0);
    s_inShadowPass = true;
    s_inRenderPass = true; // 阴影渲染通道活跃
}

void GN_VkShadowPassEnd() {
    if(!GN_Vk.initialized || !s_inShadowPass) return;
    vkCmdEndRenderPass(GN_Vk.commandBuffers[GN_Vk.currentFrame]);
    s_inShadowPass = false;
    s_inRenderPass = false; // 阴影渲染通道结束，无活跃通道
}

void GN_VkShadowDraw(const void *vertices, uint32_t vertexCount,
                     const uint32_t *indices, uint32_t indexCount,
                     const sMAT44 *modelMatrix) {
    if(!GN_Vk.initialized || !s_inShadowPass || !vertices || vertexCount == 0) return;

    // Push model matrix
    vkCmdPushConstants(GN_Vk.commandBuffers[GN_Vk.currentFrame],
                       GN_Vk.shadowPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, 64, modelMatrix->m);

    // 上传顶点 (每 draw call 独立区域)
    uint32_t vtxSize = vertexCount * sizeof(VkVertex3D);
    uint32_t idxSize = (indices && indexCount > 0) ? indexCount * sizeof(uint32_t) : 0;
    Ensure3DBufferCapacity(vtxSize, idxSize);

    void *d;
    vkMapMemory(GN_Vk.device, GN_Vk.vertexMemory3D, s_vtxOfs3D, vtxSize, 0, &d);
    memcpy(d, vertices, vtxSize);
    vkUnmapMemory(GN_Vk.device, GN_Vk.vertexMemory3D);

    // 绑定阴影管线
    vkCmdBindPipeline(GN_Vk.commandBuffers[GN_Vk.currentFrame],
                      VK_PIPELINE_BIND_POINT_GRAPHICS, GN_Vk.shadowPipeline);

    VkDeviceSize offs[] = {s_vtxOfs3D};
    vkCmdBindVertexBuffers(GN_Vk.commandBuffers[GN_Vk.currentFrame], 0, 1,
                           &GN_Vk.vertexBuffer3D, offs);
    s_vtxOfs3D += vtxSize;

    if(indices && indexCount > 0) {
        void *id;
        vkMapMemory(GN_Vk.device, GN_Vk.indexMemory3D, s_idxOfs3D, idxSize, 0, &id);
        memcpy(id, indices, idxSize);
        vkUnmapMemory(GN_Vk.device, GN_Vk.indexMemory3D);

        vkCmdBindIndexBuffer(GN_Vk.commandBuffers[GN_Vk.currentFrame],
                             GN_Vk.indexBuffer3D, s_idxOfs3D, VK_INDEX_TYPE_UINT32);
        s_idxOfs3D += idxSize;
        vkCmdDrawIndexed(GN_Vk.commandBuffers[GN_Vk.currentFrame], indexCount, 1, 0, 0, 0);
    } else {
        vkCmdDraw(GN_Vk.commandBuffers[GN_Vk.currentFrame], vertexCount, 1, 0, 0);
    }
}

// ═══════════════════════════════════════════════════════════
//  HDR→Swapchain 合成 (Bloom + 色调映射)
//  必须在 GN_VkEnd3DPass() 之后调用 (3D 已结束, HDR 纹理就绪)
// ═══════════════════════════════════════════════════════════
void GN_VkCompositeToSwapchain() {
    if(!GN_Vk.initialized || !GN_Vk.compositePipeline) return;
    if(!GN_Vk.hdrView || !GN_Vk.hdrSampler) return;

    // 更新 composite descriptor: 绑定当前 HDR 纹理
    VkDescriptorImageInfo hii = {};
    hii.sampler = GN_Vk.hdrSampler;
    hii.imageView = GN_Vk.hdrView;
    hii.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    VkWriteDescriptorSet wc = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    wc.dstSet = GN_Vk.compositeDescriptorSet;
    wc.dstBinding = 1;
    wc.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    wc.descriptorCount = 1;
    wc.pImageInfo = &hii;
    vkUpdateDescriptorSets(GN_Vk.device, 1, &wc, 0, nullptr);

    // 开始合成 pass → swapchain
    VkClearValue cv;
    cv.color = {{0.0f, 0.0f, 0.0f, 1.0f}};
    VkRenderPassBeginInfo rpi = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    rpi.renderPass = GN_Vk.compositeRenderPass;
    rpi.framebuffer = GN_Vk.compositeFramebuffers[GN_Vk.currentFrame];
    rpi.renderArea = {{0, 0}, GN_Vk.swapchainExtent};
    rpi.clearValueCount = 1;
    rpi.pClearValues = &cv;
    vkCmdBeginRenderPass(GN_Vk.commandBuffers[GN_Vk.currentFrame], &rpi, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(GN_Vk.commandBuffers[GN_Vk.currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, GN_Vk.compositePipeline);
    vkCmdBindDescriptorSets(GN_Vk.commandBuffers[GN_Vk.currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                            GN_Vk.compositePipelineLayout, 0, 1, &GN_Vk.compositeDescriptorSet, 0, 0);

    // 全屏三角形 (3 顶点, 无缓冲)
    vkCmdDraw(GN_Vk.commandBuffers[GN_Vk.currentFrame], 3, 1, 0, 0);

    vkCmdEndRenderPass(GN_Vk.commandBuffers[GN_Vk.currentFrame]);
    s_inRenderPass = false;
}

// ═══════════════════════════════════════════════════════════
//  天空盒渲染
//  必须在一个活跃的 RenderPass 中调用 (在 Clear 之后, 3D 绘制之前)
// ═══════════════════════════════════════════════════════════
void GN_VkSetSky(const VkSkyParams &params) { s_skyParams = params; }
// G3：环境重烘读取当前天空参数（天气/时间）。
void GN_VkGetSky(VkSkyParams *out) {
    if(out) *out = s_skyParams;
}

void GN_VkDrawSkybox(const sMAT44 *viewProj) {
    if(!GN_Vk.initialized || !GN_Vk.skyboxPipeline || !s_in3DPass) return;

    // UBO: viewProj(64) + VkSkyParams(64) = 128 B
    float ubo[32];
    memcpy(ubo, viewProj->m, 64);
    memcpy(ubo + 16, &s_skyParams, sizeof(VkSkyParams));
    void *d;
    vkMapMemory(GN_Vk.device, GN_Vk.skyboxUBOMemory, 0, sizeof(ubo), 0, &d);
    memcpy(d, ubo, sizeof(ubo));
    vkUnmapMemory(GN_Vk.device, GN_Vk.skyboxUBOMemory);

    vkCmdBindPipeline(GN_Vk.commandBuffers[GN_Vk.currentFrame],
                      VK_PIPELINE_BIND_POINT_GRAPHICS, GN_Vk.skyboxPipeline);
    vkCmdBindDescriptorSets(GN_Vk.commandBuffers[GN_Vk.currentFrame],
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            GN_Vk.skyboxPipelineLayout, 0, 1, &GN_Vk.skyboxDescriptorSet, 0, 0);
    // 36 顶点 (单位立方体), 无缓冲
    vkCmdDraw(GN_Vk.commandBuffers[GN_Vk.currentFrame], 36, 1, 0, 0);
}

// ═══════════════════════════════════════════════════════════
//  PBR 渲染 (使用 PBR 管线)
// ═══════════════════════════════════════════════════════════
void GN_VkBegin3DPassPBR(const sMAT44 *viewProj, const sVEC3 *cameraPos,
                         const void *lightUBO, uint32_t lightUBOSize) {
    if(!GN_Vk.initialized || s_in3DPass) return;
    s_vtxOfs3D = 0;
    s_idxOfs3D = 0;
    // 刷新 2D 批次 (与 GN_VkBegin3DPass 相同逻辑)
    if(gVCnt > 0) {
        void *d;
        vkMapMemory(GN_Vk.device, GN_Vk.vertexMemory, 0, gVCnt * sizeof(VkVertex), 0, &d);
        memcpy(d, gVBuf, gVCnt * sizeof(VkVertex));
        vkUnmapMemory(GN_Vk.device, GN_Vk.vertexMemory);
        uint32_t prevEnd = 0;
        for(uint32_t i = 0; i < s_batchCount; i++) {
            uint32_t count = s_batchEnd[i] - prevEnd;
            if(count > 0) {
                vkCmdBindDescriptorSets(GN_Vk.commandBuffers[GN_Vk.currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, GN_Vk.pipelineLayout, 0, 1, &s_batchDS[i], 0, 0);
                VkDeviceSize offs[] = {prevEnd * sizeof(VkVertex)};
                vkCmdBindVertexBuffers(GN_Vk.commandBuffers[GN_Vk.currentFrame], 0, 1, &GN_Vk.vertexBuffer, offs);
                vkCmdDraw(GN_Vk.commandBuffers[GN_Vk.currentFrame], count, 1, 0, 0);
            }
            prevEnd = s_batchEnd[i];
        }
        gVCnt = 0;
        s_batchCount = 0;
        s_lastDS = VK_NULL_HANDLE;
    }
    // 结束当前渲染通道 (2D 或 空 — 阴影 Pass 后可能已无活跃通道)
    if(s_inRenderPass) {
        vkCmdEndRenderPass(GN_Vk.commandBuffers[GN_Vk.currentFrame]);
        s_inRenderPass = false;
    }

    // 更新场景 UBO
    {
        VkSceneUBO ubo;
        memcpy(ubo.viewProj, viewProj->m, sizeof(ubo.viewProj));
        ubo.cameraPos[0] = cameraPos->x;
        ubo.cameraPos[1] = cameraPos->y;
        ubo.cameraPos[2] = cameraPos->z;
        ubo.cameraPos[3] = 0.0f;
        if(lightUBO && lightUBOSize >= sizeof(GN3d::sLIGHT_UBO)) {
            const GN3d::sLIGHT_UBO *lubo = (const GN3d::sLIGHT_UBO *)lightUBO;
            ubo.ambientLight[0] = lubo->ambientLight[0];
            ubo.ambientLight[1] = lubo->ambientLight[1];
            ubo.ambientLight[2] = lubo->ambientLight[2];
            ubo.ambientLight[3] = lubo->ambientLight[3];
        } else {
            ubo.ambientLight[0] = 0.15f;
            ubo.ambientLight[1] = 0.15f;
            ubo.ambientLight[2] = 0.2f;
            ubo.ambientLight[3] = 1.0f;
        }
        void *data;
        vkMapMemory(GN_Vk.device, GN_Vk.uniformMemory3D, 0, sizeof(ubo), 0, &data);
        memcpy(data, &ubo, sizeof(ubo));
        vkUnmapMemory(GN_Vk.device, GN_Vk.uniformMemory3D);
    }
    if(lightUBO && lightUBOSize > 0) {
        void *ld;
        vkMapMemory(GN_Vk.device, GN_Vk.lightMemory3D, 0, lightUBOSize, 0, &ld);
        memcpy(ld, lightUBO, lightUBOSize);
        vkUnmapMemory(GN_Vk.device, GN_Vk.lightMemory3D);
    }
    // 开始 3D render pass (PBR)
    {
        VkClearValue cvs[3];
        cvs[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
        cvs[1].depthStencil = {1.0f, 0};
        cvs[2].color = {{0.0f, 0.0f, 0.0f, 1.0f}}; // resolve clear
        VkRenderPassBeginInfo rpi = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        rpi.renderPass = GN_Vk.renderPass3D;
        rpi.framebuffer = s_framebuffers3D[GN_Vk.currentFrame];
        rpi.renderArea = {{0, 0}, GN_Vk.swapchainExtent};
        rpi.clearValueCount = 3;
        rpi.pClearValues = cvs;
        vkCmdBeginRenderPass(GN_Vk.commandBuffers[GN_Vk.currentFrame], &rpi, VK_SUBPASS_CONTENTS_INLINE);
    }
    // 使用 PBR 管线
    vkCmdBindPipeline(GN_Vk.commandBuffers[GN_Vk.currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, GN_Vk.pipelinePBR);
    // 绑定 PBR 描述符集 (含 normal sampler + shadow sampler)
    vkCmdBindDescriptorSets(GN_Vk.commandBuffers[GN_Vk.currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                            GN_Vk.pipelineLayoutPBR, 0, 1, &GN_Vk.descriptorSetPBR, 0, 0);
    s_in3DPass = true;
    s_inRenderPass = true; // 3D 渲染通道活跃
}

// ═══════════════════════════════════════════════════════════
//  3D PBR Pass — HDR 变体 (渲染到 HDR 目标, 供后续合成)
// ═══════════════════════════════════════════════════════════
void GN_VkBegin3DPassPBR_HDR(const sMAT44 *viewProj, const sVEC3 *cameraPos,
                             const void *lightUBO, uint32_t lightUBOSize) {
    if(!GN_Vk.initialized || s_in3DPass) return;
    s_vtxOfs3D = 0;
    s_idxOfs3D = 0;
    if(gVCnt > 0) {
        void *d;
        vkMapMemory(GN_Vk.device, GN_Vk.vertexMemory, 0, gVCnt * sizeof(VkVertex), 0, &d);
        memcpy(d, gVBuf, gVCnt * sizeof(VkVertex));
        vkUnmapMemory(GN_Vk.device, GN_Vk.vertexMemory);
        uint32_t prevEnd = 0;
        for(uint32_t i = 0; i < s_batchCount; i++) {
            uint32_t count = s_batchEnd[i] - prevEnd;
            if(count > 0) {
                vkCmdBindDescriptorSets(GN_Vk.commandBuffers[GN_Vk.currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, GN_Vk.pipelineLayout, 0, 1, &s_batchDS[i], 0, 0);
                VkDeviceSize offs[] = {prevEnd * sizeof(VkVertex)};
                vkCmdBindVertexBuffers(GN_Vk.commandBuffers[GN_Vk.currentFrame], 0, 1, &GN_Vk.vertexBuffer, offs);
                vkCmdDraw(GN_Vk.commandBuffers[GN_Vk.currentFrame], count, 1, 0, 0);
            }
            prevEnd = s_batchEnd[i];
        }
        gVCnt = 0;
        s_batchCount = 0;
        s_lastDS = VK_NULL_HANDLE;
    }
    if(s_inRenderPass) {
        vkCmdEndRenderPass(GN_Vk.commandBuffers[GN_Vk.currentFrame]);
        s_inRenderPass = false;
    }
    {
        VkSceneUBO ubo;
        memcpy(ubo.viewProj, viewProj->m, sizeof(ubo.viewProj));
        ubo.cameraPos[0] = cameraPos->x;
        ubo.cameraPos[1] = cameraPos->y;
        ubo.cameraPos[2] = cameraPos->z;
        ubo.cameraPos[3] = 0.0f;
        if(lightUBO && lightUBOSize >= sizeof(GN3d::sLIGHT_UBO)) {
            const GN3d::sLIGHT_UBO *lubo = (const GN3d::sLIGHT_UBO *)lightUBO;
            ubo.ambientLight[0] = lubo->ambientLight[0];
            ubo.ambientLight[1] = lubo->ambientLight[1];
            ubo.ambientLight[2] = lubo->ambientLight[2];
            ubo.ambientLight[3] = lubo->ambientLight[3];
        } else {
            ubo.ambientLight[0] = 0.15f;
            ubo.ambientLight[1] = 0.15f;
            ubo.ambientLight[2] = 0.2f;
            ubo.ambientLight[3] = 1.0f;
        }
        void *data;
        vkMapMemory(GN_Vk.device, GN_Vk.uniformMemory3D, 0, sizeof(ubo), 0, &data);
        memcpy(data, &ubo, sizeof(ubo));
        vkUnmapMemory(GN_Vk.device, GN_Vk.uniformMemory3D);
    }
    if(lightUBO && lightUBOSize > 0) {
        void *ld;
        vkMapMemory(GN_Vk.device, GN_Vk.lightMemory3D, 0, lightUBOSize, 0, &ld);
        memcpy(ld, lightUBO, lightUBOSize);
        vkUnmapMemory(GN_Vk.device, GN_Vk.lightMemory3D);
    }
    {
        VkClearValue cvs[3];
        cvs[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
        cvs[1].depthStencil = {1.0f, 0};
        cvs[2].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
        VkRenderPassBeginInfo rpi = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        rpi.renderPass = GN_Vk.hdrRenderPass;
        rpi.framebuffer = GN_Vk.hdrFramebuffers[GN_Vk.currentFrame];
        rpi.renderArea = {{0, 0}, GN_Vk.swapchainExtent};
        rpi.clearValueCount = 3;
        rpi.pClearValues = cvs;
        vkCmdBeginRenderPass(GN_Vk.commandBuffers[GN_Vk.currentFrame], &rpi, VK_SUBPASS_CONTENTS_INLINE);
    }
    vkCmdBindPipeline(GN_Vk.commandBuffers[GN_Vk.currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, GN_Vk.pipelinePBR);
    vkCmdBindDescriptorSets(GN_Vk.commandBuffers[GN_Vk.currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                            GN_Vk.pipelineLayoutPBR, 0, 1, &GN_Vk.descriptorSetPBR, 0, 0);
    s_in3DPass = true;
    s_inRenderPass = true;
}

// ═══════════════════════════════════════════════════════════
//  延迟渲染: G-Buffer Pass
// ═══════════════════════════════════════════════════════════
void GN_VkBegin3DPassDeferred(const sMAT44 *viewProj, const sVEC3 *cameraPos,
                              const void *lightUBO, uint32_t lightUBOSize) {
    if(!GN_Vk.initialized || s_in3DPass || !GN_Vk.gBufferPipeline) return;
    s_deferredViewProj = *viewProj;
    s_hasDeferredViewProj = true;
    s_vtxOfs3D = 0;
    s_idxOfs3D = 0;
    // 刷新 2D 批次
    if(gVCnt > 0) {
        void *d;
        vkMapMemory(GN_Vk.device, GN_Vk.vertexMemory, 0, gVCnt * sizeof(VkVertex), 0, &d);
        memcpy(d, gVBuf, gVCnt * sizeof(VkVertex));
        vkUnmapMemory(GN_Vk.device, GN_Vk.vertexMemory);
        uint32_t prevEnd = 0;
        for(uint32_t i = 0; i < s_batchCount; i++) {
            uint32_t count = s_batchEnd[i] - prevEnd;
            if(count > 0) { vkCmdDraw(GN_Vk.commandBuffers[GN_Vk.currentFrame], count, 1, 0, 0); }
            prevEnd = s_batchEnd[i];
        }
        gVCnt = 0;
        s_batchCount = 0;
        s_lastDS = VK_NULL_HANDLE;
    }
    if(s_inRenderPass) {
        vkCmdEndRenderPass(GN_Vk.commandBuffers[GN_Vk.currentFrame]);
        s_inRenderPass = false;
    }
    // 上传 SceneUBO
    {
        VkSceneUBO ubo;
        memcpy(ubo.viewProj, viewProj->m, 64);
        ubo.cameraPos[0] = cameraPos->x;
        ubo.cameraPos[1] = cameraPos->y;
        ubo.cameraPos[2] = cameraPos->z;
        ubo.cameraPos[3] = 0;
        void *d;
        vkMapMemory(GN_Vk.device, GN_Vk.uniformMemory3D, 0, sizeof(ubo), 0, &d);
        memcpy(d, &ubo, sizeof(ubo));
        vkUnmapMemory(GN_Vk.device, GN_Vk.uniformMemory3D);
    }
    (void)lightUBO;
    (void)lightUBOSize;
    // 开始 G-Buffer RenderPass
    {
        VkClearValue cvs[4];
        cvs[0].color = {{0, 0, 0, 1}};
        cvs[1].color = {{0, 0, 0, 1}};
        cvs[2].color = {{0, 0, 0, 1}};
        cvs[3].depthStencil = {1, 0};
        VkRenderPassBeginInfo rpi = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        rpi.renderPass = GN_Vk.gBufferRenderPass;
        rpi.framebuffer = GN_Vk.gBufferFramebuffers[0];
        rpi.renderArea = {{0, 0}, GN_Vk.swapchainExtent};
        rpi.clearValueCount = 4;
        rpi.pClearValues = cvs;
        vkCmdBeginRenderPass(GN_Vk.commandBuffers[GN_Vk.currentFrame], &rpi, VK_SUBPASS_CONTENTS_INLINE);
    }
    vkCmdBindPipeline(GN_Vk.commandBuffers[GN_Vk.currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, GN_Vk.gBufferPipeline);
    vkCmdBindDescriptorSets(GN_Vk.commandBuffers[GN_Vk.currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, GN_Vk.gBufferPipelineLayout, 0, 1, &GN_Vk.gBufferDescriptorSet, 0, 0);
    s_in3DPass = true;
    s_inRenderPass = true;
}

// G2b：SSAO Pass（在 GN_VkDraw3D_GBuffer 之后、GN_VkDeferredLighting 之前调用）。
// GI UBO 的 indirect.w<=0 时直接返回（默认关，不跑 pass，观感/开销不变）。
void GN_VkSSAOPass() {
    if(!GN_Vk.initialized || GN_Vk.ssaoPipeline == VK_NULL_HANDLE) return;
    if(!isSsaoEnabled() || !s_hasDeferredViewProj) return;
    if(s_in3DPass) {
        vkCmdEndRenderPass(GN_Vk.commandBuffers[GN_Vk.currentFrame]);
        s_in3DPass = false;
        s_inRenderPass = false;
    }
    updateSsaoUniforms(s_deferredViewProj);
    recordSsaoPasses(GN_Vk.commandBuffers[GN_Vk.currentFrame]);
    s_inRenderPass = false;
}

// G4：SSGI Pass（在 GN_VkSSAOPass 之后、GN_VkDeferredLighting 之前调用）。
// GI UBO 的 envParams.w<=0 时直接返回（默认关，不跑 pass，观感/开销不变）。
void GN_VkSSGIPass() {
    if(!GN_Vk.initialized || GN_Vk.ssgiPipeline == VK_NULL_HANDLE) return;
    if(!isSsgiEnabled() || !s_hasDeferredViewProj) return;
    if(s_in3DPass) {
        vkCmdEndRenderPass(GN_Vk.commandBuffers[GN_Vk.currentFrame]);
        s_in3DPass = false;
        s_inRenderPass = false;
    }
    updateSsgiUniforms(s_deferredViewProj);
    recordSsgiPasses(GN_Vk.commandBuffers[GN_Vk.currentFrame]);
    s_inRenderPass = false;
}

// ═══════════════════════════════════════════════════════════
//  延迟光照 Pass (读取 G-Buffer → 光照计算 → swapchain)
// ═══════════════════════════════════════════════════════════
// ═══════════════════════════════════════════════════════════
//  G-Buffer 绘制 (复用 PBR push constant, 写入 G-Buffer MRT)
// ═══════════════════════════════════════════════════════════
void GN_VkDraw3D_GBuffer(const void *vertices, uint32_t vertexCount,
                         const uint32_t *indices, uint32_t indexCount,
                         const sMAT44 *modelMatrix,
                         float albedoR, float albedoG, float albedoB,
                         float roughness, float metallic, float ao,
                         const sIMAGE *albedoTex, const sIMAGE *normalTex,
                         float alphaCutoff) {
    if(!GN_Vk.initialized || !s_in3DPass || !vertices || vertexCount == 0) return;
    if(!GN_Vk.gBufferPipeline) return;

    // 更新纹理描述符
    {
        VkImageView aView = (albedoTex && albedoTex->vkImageView) ? albedoTex->vkImageView : GN_Vk.defaultImageView;
        VkSampler aSamp = (albedoTex && albedoTex->vkSampler) ? albedoTex->vkSampler : GN_Vk.defaultSampler;
        VkImageView nView = (normalTex && normalTex->vkImageView) ? normalTex->vkImageView : GN_Vk.defaultImageView;
        VkSampler nSamp = (normalTex && normalTex->vkSampler) ? normalTex->vkSampler : GN_Vk.defaultSampler;
        VkDescriptorImageInfo aii = {};
        aii.sampler = aSamp;
        aii.imageView = aView;
        aii.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        VkDescriptorImageInfo nii = {};
        nii.sampler = nSamp;
        nii.imageView = nView;
        nii.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        VkWriteDescriptorSet w[2] = {{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET}, {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET}};
        w[0].dstSet = GN_Vk.gBufferDescriptorSet;
        w[0].dstBinding = 1;
        w[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        w[0].descriptorCount = 1;
        w[0].pImageInfo = &aii;
        w[1].dstSet = GN_Vk.gBufferDescriptorSet;
        w[1].dstBinding = 3;
        w[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        w[1].descriptorCount = 1;
        w[1].pImageInfo = &nii;
        vkUpdateDescriptorSets(GN_Vk.device, 2, w, 0, 0);
        vkCmdBindDescriptorSets(GN_Vk.commandBuffers[GN_Vk.currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, GN_Vk.gBufferPipelineLayout, 0, 1, &GN_Vk.gBufferDescriptorSet, 0, 0);
    }
    // Push constants (PBR 参数 → G-Buffer)
    VkModelPushConstPBR pc;
    memcpy(pc.model, modelMatrix->m, 64);
    pc.albedo[0] = albedoR;
    pc.albedo[1] = albedoG;
    pc.albedo[2] = albedoB;
    pc.albedo[3] = 1.0f;
    pc.pbrParams[0] = roughness;
    pc.pbrParams[1] = metallic;
    pc.pbrParams[2] = ao;
    pc.pbrParams[3] = alphaCutoff; // gbuffer.frag 用 pc.specular.w 做 alpha discard（0=不裁）
    vkCmdPushConstants(GN_Vk.commandBuffers[GN_Vk.currentFrame], GN_Vk.gBufferPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(VkModelPushConstPBR), &pc);
    // 顶点数据 (每 draw call 独立区域)
    uint32_t vtxSize = vertexCount * sizeof(VkVertex3D);
    uint32_t idxSize = (indices && indexCount > 0) ? indexCount * sizeof(uint32_t) : 0;
    Ensure3DBufferCapacity(vtxSize, idxSize);
    void *d;
    vkMapMemory(GN_Vk.device, GN_Vk.vertexMemory3D, s_vtxOfs3D, vtxSize, 0, &d);
    memcpy(d, vertices, vtxSize);
    vkUnmapMemory(GN_Vk.device, GN_Vk.vertexMemory3D);
    vkCmdBindPipeline(GN_Vk.commandBuffers[GN_Vk.currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, GN_Vk.gBufferPipeline);
    VkDeviceSize offs[] = {s_vtxOfs3D};
    vkCmdBindVertexBuffers(GN_Vk.commandBuffers[GN_Vk.currentFrame], 0, 1, &GN_Vk.vertexBuffer3D, offs);
    s_vtxOfs3D += vtxSize;
    if(indices && indexCount > 0) {
        void *id;
        vkMapMemory(GN_Vk.device, GN_Vk.indexMemory3D, s_idxOfs3D, idxSize, 0, &id);
        memcpy(id, indices, idxSize);
        vkUnmapMemory(GN_Vk.device, GN_Vk.indexMemory3D);
        vkCmdBindIndexBuffer(GN_Vk.commandBuffers[GN_Vk.currentFrame], GN_Vk.indexBuffer3D, s_idxOfs3D, VK_INDEX_TYPE_UINT32);
        s_idxOfs3D += idxSize;
        vkCmdDrawIndexed(GN_Vk.commandBuffers[GN_Vk.currentFrame], indexCount, 1, 0, 0, 0);
    } else {
        vkCmdDraw(GN_Vk.commandBuffers[GN_Vk.currentFrame], vertexCount, 1, 0, 0);
    }
}

void GN_VkDeferredLighting(const sMAT44 *viewProj, const sVEC3 *cameraPos,
                           const void *lightUBO, uint32_t lightUBOSize) {
    if(!GN_Vk.initialized || !GN_Vk.deferredLightPipeline) return;
    // 必须在 G-Buffer Pass 结束后调用
    if(s_in3DPass) {
        vkCmdEndRenderPass(GN_Vk.commandBuffers[GN_Vk.currentFrame]);
        s_in3DPass = false;
        s_inRenderPass = false;
    }
    // G2b：写 GI UBO 的 invViewProj（偏移 192）——延迟 shader 用它重建世界位置，
    // 替代每像素 inverse(vp)。用修复后的 Mat44Inverse（纯平移矩阵也已正确）。
    if(GN_Vk.giUBOMemory != VK_NULL_HANDLE) {
        sMAT44 inverse;
        Mat44Inverse(&inverse, viewProj);
        void *giMapped = nullptr;
        vkMapMemory(GN_Vk.device, GN_Vk.giUBOMemory, 0, sizeof(GN3d::sGI_UBO), 0, &giMapped);
        if(giMapped) {
            memcpy((char *)giMapped + 192, inverse.m, sizeof(float) * 16);
            vkUnmapMemory(GN_Vk.device, GN_Vk.giUBOMemory);
        }
    }
    // 上传 SceneUBO + LightUBO
    {
        VkSceneUBO ubo;
        memcpy(ubo.viewProj, viewProj->m, 64);
        ubo.cameraPos[0] = cameraPos->x;
        ubo.cameraPos[1] = cameraPos->y;
        ubo.cameraPos[2] = cameraPos->z;
        ubo.cameraPos[3] = 0;
        if(lightUBO && lightUBOSize >= sizeof(GN3d::sLIGHT_UBO)) {
            const GN3d::sLIGHT_UBO *lubo = (const GN3d::sLIGHT_UBO *)lightUBO;
            ubo.ambientLight[0] = lubo->ambientLight[0];
            ubo.ambientLight[1] = lubo->ambientLight[1];
            ubo.ambientLight[2] = lubo->ambientLight[2];
            ubo.ambientLight[3] = lubo->ambientLight[3];
        } else {
            ubo.ambientLight[0] = 0.15f;
            ubo.ambientLight[1] = 0.15f;
            ubo.ambientLight[2] = 0.2f;
            ubo.ambientLight[3] = 1.0f;
        }
        void *d;
        vkMapMemory(GN_Vk.device, GN_Vk.uniformMemory3D, 0, sizeof(ubo), 0, &d);
        memcpy(d, &ubo, sizeof(ubo));
        vkUnmapMemory(GN_Vk.device, GN_Vk.uniformMemory3D);
    }
    if(lightUBO && lightUBOSize > 0) {
        void *ld;
        vkMapMemory(GN_Vk.device, GN_Vk.lightMemory3D, 0, lightUBOSize, 0, &ld);
        memcpy(ld, lightUBO, lightUBOSize);
        vkUnmapMemory(GN_Vk.device, GN_Vk.lightMemory3D);
    }
    // 开始延迟光照 Pass → swapchain
    {
        VkClearValue cv;
        cv.color = {{0, 0, 0, 1}};
        VkRenderPassBeginInfo rpi = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        rpi.renderPass = GN_Vk.deferredLightRenderPass;
        rpi.framebuffer = GN_Vk.deferredLightFramebuffers[GN_Vk.currentFrame];
        rpi.renderArea = {{0, 0}, GN_Vk.swapchainExtent};
        rpi.clearValueCount = 1;
        rpi.pClearValues = &cv;
        vkCmdBeginRenderPass(GN_Vk.commandBuffers[GN_Vk.currentFrame], &rpi, VK_SUBPASS_CONTENTS_INLINE);
    }
    vkCmdBindPipeline(GN_Vk.commandBuffers[GN_Vk.currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, GN_Vk.deferredLightPipeline);
    vkCmdBindDescriptorSets(GN_Vk.commandBuffers[GN_Vk.currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, GN_Vk.deferredLightPipelineLayout, 0, 1, &GN_Vk.deferredLightDescriptorSet, 0, 0);
    vkCmdDraw(GN_Vk.commandBuffers[GN_Vk.currentFrame], 3, 1, 0, 0);
    vkCmdEndRenderPass(GN_Vk.commandBuffers[GN_Vk.currentFrame]);
    s_inRenderPass = false;
}

} // namespace GNg
