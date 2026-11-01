// 前向 3D pass：开始/结束、Blinn-Phong 与 PBR 绘制。
#include <GN.h>
#include "GN_Vk.h"
#include "vkInternal.h"

#include <cstring>

namespace GNg {

// P10：最近一次 PBR 绘制提交的 indexCount（诊断/验收钩子）。
namespace {
uint32_t s_last3DIndexCount = 0;
}

uint32_t GN_VkLast3DIndexCount() { return s_last3DIndexCount; }

void GN_VkBegin3DPass(const sMAT44 *viewProj, const sVEC3 *cameraPos,
                      const void *lightUBO, uint32_t lightUBOSize) {
    if(!GN_Vk.initialized || s_in3DPass) return;

    // ★ 重置缓冲偏移 (每个 3D Pass 内各 draw call 使用独立区域)
    s_vtxOfs3D = 0;
    s_idxOfs3D = 0;

    // 刷新当前 2D 批次
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

    // 结束当前渲染通道 (2D 或 空)
    if(s_inRenderPass) {
        vkCmdEndRenderPass(GN_Vk.commandBuffers[GN_Vk.currentFrame]);
        s_inRenderPass = false;
    }

    // 更新 3D 场景 UBO
    {
        VkSceneUBO ubo;
        memcpy(ubo.viewProj, viewProj->m, sizeof(ubo.viewProj));
        ubo.cameraPos[0] = cameraPos->x;
        ubo.cameraPos[1] = cameraPos->y;
        ubo.cameraPos[2] = cameraPos->z;
        ubo.cameraPos[3] = 0.0f;
        // 从 lightUBO 读取环境光 (若未提供则使用默认值)
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

    // 上传光照 UBO
    if(lightUBO && lightUBOSize > 0) {
        void *ld;
        vkMapMemory(GN_Vk.device, GN_Vk.lightMemory3D, 0, lightUBOSize, 0, &ld);
        memcpy(ld, lightUBO, lightUBOSize);
        vkUnmapMemory(GN_Vk.device, GN_Vk.lightMemory3D);
    }

    // L4：sky-view LUT 重建（前向 pass 路径，render pass 之外录制；此前只有 PBR/延迟路径接了）
    recordAtmosphereLutUpdate(GN_Vk.commandBuffers[GN_Vk.currentFrame]);
    // 开始 3D render pass
    {
        VkClearValue cvs[3];
        cvs[0].color = {{0.05f, 0.05f, 0.1f, 1.0f}}; // MSAA color — 深蓝背景
        cvs[1].depthStencil = {1.0f, 0}; // MSAA depth
        cvs[2].color = {{0.0f, 0.0f, 0.0f, 1.0f}}; // Resolve — 会被覆盖
        VkRenderPassBeginInfo rpi = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        rpi.renderPass = GN_Vk.renderPass3D;
        rpi.framebuffer = s_framebuffers3D[GN_Vk.currentFrame];
        rpi.renderArea = {{0, 0}, GN_Vk.swapchainExtent};
        rpi.clearValueCount = 3;
        rpi.pClearValues = cvs;
        vkCmdBeginRenderPass(GN_Vk.commandBuffers[GN_Vk.currentFrame], &rpi, VK_SUBPASS_CONTENTS_INLINE);
    }

    vkCmdBindPipeline(GN_Vk.commandBuffers[GN_Vk.currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, GN_Vk.pipeline3D);
    vkCmdBindDescriptorSets(GN_Vk.commandBuffers[GN_Vk.currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                            GN_Vk.pipelineLayout3D, 0, 1, &GN_Vk.descriptorSet3D, 0, 0);

    s_in3DPass = true;
    s_inRenderPass = true; // 3D 渲染通道活跃
}

void GN_VkEnd3DPass() {
    if(!GN_Vk.initialized || !s_in3DPass) return;

    vkCmdEndRenderPass(GN_Vk.commandBuffers[GN_Vk.currentFrame]);

    s_in3DPass = false;
    s_inRenderPass = false; // 3D pass 结束，不重启 2D pass（避免 CLEAR 清掉 3D 内容）
    // L1：3D 输出统一线性 HDR → 合成（曝光/色调映射/Bloom）到 swapchain；
    // 2D 叠层随后以 LOAD 方式画在其上。
    GN_VkCompositeToSwapchain();
}

void GN_VkDraw3D(const void *vertices, uint32_t vertexCount,
                 const uint32_t *indices, uint32_t indexCount,
                 const sMAT44 *modelMatrix,
                 float diffuseR, float diffuseG, float diffuseB,
                 float specularR, float specularG, float specularB,
                 float shininess, const sIMAGE *texture) {
    if(!GN_Vk.initialized || !s_in3DPass || !vertices || vertexCount == 0) return;

    // ── 更新纹理描述符 (binding 1) ──
    // 有纹理→用纹理, 无纹理→用默认白色 (与 diffuse 相乘无影响)
    {
        VkImageView imgView = (texture && texture->vkImageView)
                                  ? texture->vkImageView
                                  : GN_Vk.defaultImageView;
        VkSampler sampler = (texture && texture->vkSampler)
                                ? texture->vkSampler
                                : GN_Vk.defaultSampler;

        VkDescriptorImageInfo dii = {};
        dii.sampler = sampler;
        dii.imageView = imgView;
        dii.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkWriteDescriptorSet w = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        w.dstSet = GN_Vk.descriptorSet3D;
        w.dstBinding = 1;
        w.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        w.descriptorCount = 1;
        w.pImageInfo = &dii;
        vkUpdateDescriptorSets(GN_Vk.device, 1, &w, 0, nullptr);

        // 重新绑定描述符集以确保更新生效
        vkCmdBindDescriptorSets(GN_Vk.commandBuffers[GN_Vk.currentFrame],
                                VK_PIPELINE_BIND_POINT_GRAPHICS,
                                GN_Vk.pipelineLayout3D, 0, 1, &GN_Vk.descriptorSet3D, 0, nullptr);
    }

    // 上传顶点数据 (动态缓冲, 每 draw call 独立区域)
    uint32_t vtxSize = vertexCount * sizeof(VkVertex3D);
    uint32_t idxSize = (indices && indexCount > 0) ? indexCount * sizeof(uint32_t) : 0;
    Ensure3DBufferCapacity(vtxSize, idxSize);

    // 写入顶点到当前偏移
    void *d;
    vkMapMemory(GN_Vk.device, GN_Vk.vertexMemory3D, s_vtxOfs3D, vtxSize, 0, &d);
    memcpy(d, vertices, vtxSize);
    vkUnmapMemory(GN_Vk.device, GN_Vk.vertexMemory3D);

    // 绑定管线 (自给自足, 兼容 skybox 等中间管线切换)
    vkCmdBindPipeline(GN_Vk.commandBuffers[GN_Vk.currentFrame],
                      VK_PIPELINE_BIND_POINT_GRAPHICS, GN_Vk.pipeline3D);

    VkDeviceSize offs[] = {s_vtxOfs3D};
    vkCmdBindVertexBuffers(GN_Vk.commandBuffers[GN_Vk.currentFrame], 0, 1,
                           &GN_Vk.vertexBuffer3D, offs);
    s_vtxOfs3D += vtxSize;

    // Push constants (Blinn-Phong 材质)
    VkModelPushConstBP pc;
    memcpy(pc.model, modelMatrix->m, 64);
    pc.diffuse[0] = diffuseR;
    pc.diffuse[1] = diffuseG;
    pc.diffuse[2] = diffuseB;
    pc.diffuse[3] = 1.0f;
    pc.specular[0] = specularR;
    pc.specular[1] = specularG;
    pc.specular[2] = specularB;
    pc.specular[3] = shininess;
    vkCmdPushConstants(GN_Vk.commandBuffers[GN_Vk.currentFrame],
                       GN_Vk.pipelineLayout3D,
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(VkModelPushConstBP), &pc);

    if(indices && indexCount > 0) {
        // 写入索引到当前偏移
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

void GN_VkDraw3DPBR(const void *vertices, uint32_t vertexCount,
                    const uint32_t *indices, uint32_t indexCount,
                    const sMAT44 *modelMatrix,
                    float diffuseR, float diffuseG, float diffuseB,
                    float roughness, float metallic, float ao,
                    const sIMAGE *albedoTex, const sIMAGE *normalTex, float alphaCutoff,
                    bool alphaBlend) {
    if(!GN_Vk.initialized || !s_in3DPass || !vertices || vertexCount == 0) return;
    s_last3DIndexCount = (indices && indexCount > 0) ? indexCount : 0;

    // ── 更新纹理描述符 (binding 1 = albedo, binding 3 = normal) ──
    {
        VkImageView albedoView = (albedoTex && albedoTex->vkImageView)
                                     ? albedoTex->vkImageView
                                     : GN_Vk.defaultImageView;
        VkSampler albedoSamp = (albedoTex && albedoTex->vkSampler)
                                   ? albedoTex->vkSampler
                                   : GN_Vk.defaultSampler;

        VkImageView normalView = (normalTex && normalTex->vkImageView)
                                     ? normalTex->vkImageView
                                     : GN_Vk.defaultImageView;
        VkSampler normalSamp = (normalTex && normalTex->vkSampler)
                                   ? normalTex->vkSampler
                                   : GN_Vk.defaultSampler;

        VkDescriptorImageInfo diiAlbedo = {};
        diiAlbedo.sampler = albedoSamp;
        diiAlbedo.imageView = albedoView;
        diiAlbedo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkDescriptorImageInfo diiNormal = {};
        diiNormal.sampler = normalSamp;
        diiNormal.imageView = normalView;
        diiNormal.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkWriteDescriptorSet w[2] = {
            {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET},
            {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET}};
        w[0].dstSet = GN_Vk.descriptorSetPBR;
        w[0].dstBinding = 1;
        w[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        w[0].descriptorCount = 1;
        w[0].pImageInfo = &diiAlbedo;

        w[1].dstSet = GN_Vk.descriptorSetPBR;
        w[1].dstBinding = 3;
        w[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        w[1].descriptorCount = 1;
        w[1].pImageInfo = &diiNormal;

        vkUpdateDescriptorSets(GN_Vk.device, 2, w, 0, nullptr);

        // 重新绑定描述符集
        vkCmdBindDescriptorSets(GN_Vk.commandBuffers[GN_Vk.currentFrame],
                                VK_PIPELINE_BIND_POINT_GRAPHICS,
                                GN_Vk.pipelineLayoutPBR, 0, 1, &GN_Vk.descriptorSetPBR, 0, nullptr);
    }

    // Push model matrix + PBR material (albedo/roughness/metallic/ao)
    VkModelPushConstPBR pc;
    memcpy(pc.model, modelMatrix->m, sizeof(pc.model));
    pc.albedo[0] = diffuseR;
    pc.albedo[1] = diffuseG;
    pc.albedo[2] = diffuseB;
    pc.albedo[3] = 1.0f;
    pc.pbrParams[0] = roughness;
    pc.pbrParams[1] = metallic;
    pc.pbrParams[2] = ao;
    pc.pbrParams[3] = alphaCutoff; // gn3d_pbr.frag: pc.specular.w，0=不裁
    vkCmdPushConstants(GN_Vk.commandBuffers[GN_Vk.currentFrame],
                       GN_Vk.pipelineLayoutPBR,
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(VkModelPushConstPBR), &pc);

    // 上传顶点数据 (每 draw call 独立区域)
    uint32_t vtxSize = vertexCount * sizeof(VkVertex3D);
    uint32_t idxSize = (indices && indexCount > 0) ? indexCount * sizeof(uint32_t) : 0;
    Ensure3DBufferCapacity(vtxSize, idxSize);

    void *d;
    vkMapMemory(GN_Vk.device, GN_Vk.vertexMemory3D, s_vtxOfs3D, vtxSize, 0, &d);
    memcpy(d, vertices, vtxSize);
    vkUnmapMemory(GN_Vk.device, GN_Vk.vertexMemory3D);

    // 绑定 PBR 管线：BLEND 材质走透明变体；混合管线缺失时回退不透明（观感退化，不崩）。
    VkPipeline pbrPipeline = alphaBlend ? GN_Vk.pipelinePBRBlend : GN_Vk.pipelinePBR;
    if(!pbrPipeline) pbrPipeline = GN_Vk.pipelinePBR;
    vkCmdBindPipeline(GN_Vk.commandBuffers[GN_Vk.currentFrame],
                      VK_PIPELINE_BIND_POINT_GRAPHICS, pbrPipeline);

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

} // namespace GNg
