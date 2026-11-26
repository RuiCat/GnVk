// 前向 3D pass：开始/结束、Blinn-Phong 与 PBR 绘制。
#include <GN.h>
#include "GN_Vk.h"
#include "vkInternal.h"
#include "vkValidation.h" // 校验模式下的帧内命令缓冲分段标签

#include <cstring>

namespace GNg {

namespace {

// 纹理描述符环集：按“albedo/normal 纹理组合”为每个组合分配独立的 set。
// 原因：若在单帧内反复改写同一个 descriptorSetPBR，GPU 提交后读到的是最后一次写入的内容
// （多个不同贴图的网格会全部采样到同一张），而且改写已绑定的集本身违规
// （VUID-vkUpdateDescriptorSets-None-03047）。环集只在帧首从规范 set 拷静态绑定
// （PBR 12 个：场景/光源/阴影/天空盒/雾/GI/LUT），再用本组合的纹理覆盖 binding 1/3。
struct sPBR_TEXTURE_SET {
    VkImageView aAlbedo;
    VkSampler aAlbedoSampler;
    VkImageView aNormal;
    VkSampler aNormalSampler;
    VkDescriptorSet aSet;
};

std::vector<sPBR_TEXTURE_SET> s_pbrTextureSets;

// 前向（Blinn-Phong）路径的纹理环集：同一理由（见上方注释）。
struct sFORWARD_TEXTURE_SET {
    VkImageView aAlbedo;
    VkSampler aSampler;
    VkDescriptorSet aSet;
};

std::vector<sFORWARD_TEXTURE_SET> s_forwardTextureSets;

// 环集缓存的状态：已同步到的规范集代际号 + 「需要整体重建」标记。
uint32_t s_pbrSyncedGen = 0;
uint32_t s_forwardSyncedGen = 0;
bool s_ringResetRequested = false;

// 定义在下方：把规范 set 的 5 个绑定同步到指定纹理 set，再覆盖 binding 1。
void syncForwardTextureSet(const sFORWARD_TEXTURE_SET &entry);

VkDescriptorSet findOrCreateForwardSet(VkImageView albedo, VkSampler sampler) {
    for(size_t i = 0; i < s_forwardTextureSets.size(); ++i) {
        const sFORWARD_TEXTURE_SET &entry = s_forwardTextureSets[i];
        if(entry.aAlbedo == albedo && entry.aSampler == sampler) return entry.aSet;
    }
    if(GN_Vk.descriptorPool == VK_NULL_HANDLE || GN_Vk.descriptorLayout3D == VK_NULL_HANDLE) {
        return GN_Vk.descriptorSet3D;
    }
    if(s_forwardTextureSets.size() >= kTextureSetCacheCap) {
        return GN_Vk.descriptorSet3D; // 同 PBR：录制中不释放、不改写规范集
    }
    VkDescriptorSetAllocateInfo dai = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    dai.descriptorPool = GN_Vk.descriptorPool;
    dai.descriptorSetCount = 1;
    dai.pSetLayouts = &GN_Vk.descriptorLayout3D;
    VkDescriptorSet set = VK_NULL_HANDLE;
    if(vkAllocateDescriptorSets(GN_Vk.device, &dai, &set) != VK_SUCCESS) {
        fprintf(stderr, "[GN_Vk] 前向纹理描述符集分配失败（回退共享 set）\n");
        return GN_Vk.descriptorSet3D;
    }
    sFORWARD_TEXTURE_SET entry;
    entry.aAlbedo = albedo;
    entry.aSampler = sampler;
    entry.aSet = set;
    syncForwardTextureSet(entry); // 首次使用前补齐 5 个绑定（环集未绑定过，合法）
    s_forwardTextureSets.push_back(entry);
    return set;
}

// 定义在下方：把规范 set 的 12 个绑定同步到指定纹理 set，再覆盖 binding 1/3。
void syncPBRTextureSet(const sPBR_TEXTURE_SET &entry);

VkDescriptorSet findOrCreatePBRSet(VkImageView albedo, VkSampler albedoSampler,
                                   VkImageView normal, VkSampler normalSampler) {
    for(size_t i = 0; i < s_pbrTextureSets.size(); ++i) {
        const sPBR_TEXTURE_SET &entry = s_pbrTextureSets[i];
        if(entry.aAlbedo == albedo && entry.aAlbedoSampler == albedoSampler &&
           entry.aNormal == normal && entry.aNormalSampler == normalSampler) {
            return entry.aSet;
        }
    }
    if(GN_Vk.descriptorPool == VK_NULL_HANDLE || GN_Vk.descriptorLayoutPBR == VK_NULL_HANDLE) {
        return GN_Vk.descriptorSetPBR;
    }
    // 池总共只有 1024 个描述符（每套 PBR 环集 4 UBO + 8 采样器），无上限地长会把池吃光。
    // 到顶就不能再分配：此时正在录制命令缓冲，既不能 vkFreeDescriptorSets（会释放本帧
    // 已绑定的集），也不能写规范集。退化成绑规范集（只支持一种纹理组合），只告警一次。
    if(s_pbrTextureSets.size() >= kTextureSetCacheCap) {
        static bool warned = false;
        if(!warned) {
            warned = true;
            fprintf(stderr,
                    "[GN_Vk] 警告: PBR 纹理环集已达上限 %u，后续 draw 回退共享 set"
                    "（同一帧内多纹理组合会互相覆盖）\n",
                    kTextureSetCacheCap);
        }
        return GN_Vk.descriptorSetPBR;
    }
    VkDescriptorSetAllocateInfo dai = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    dai.descriptorPool = GN_Vk.descriptorPool;
    dai.descriptorSetCount = 1;
    dai.pSetLayouts = &GN_Vk.descriptorLayoutPBR;
    VkDescriptorSet set = VK_NULL_HANDLE;
    if(vkAllocateDescriptorSets(GN_Vk.device, &dai, &set) != VK_SUCCESS) {
        fprintf(stderr, "[GN_Vk] PBR 纹理描述符集分配失败（回退共享 set）\n");
        return GN_Vk.descriptorSetPBR;
    }
    sPBR_TEXTURE_SET entry;
    entry.aAlbedo = albedo;
    entry.aAlbedoSampler = albedoSampler;
    entry.aNormal = normal;
    entry.aNormalSampler = normalSampler;
    entry.aSet = set;
    syncPBRTextureSet(entry); // 首次使用前先补齐 12 个绑定，避免首帧读到未初始化描述符
    s_pbrTextureSets.push_back(entry);
    return set;
}

// 同步单个纹理 set：从规范 set 复制 13 个绑定（B2 起含 cube shadow），再覆盖 binding 1/3。
void syncPBRTextureSet(const sPBR_TEXTURE_SET &entry) {
        // 逐绑定复制：一次 copy 跨多个绑定会要求这些绑定的 descriptorCount 一致
        // （校验层 VUID-VkCopyDescriptorSet-srcSet-00349：「next binding ... not consistent」），
        // 这里改成每个绑定一条 copy，各 count=1，语义明确且无告警。
        // B2：12 -> 13（含 binding 12 的 cube shadow）。新增槽位自动被复制，
        // 不需要在每个纹理组合里单独写一遍。
        for(uint32_t binding = 0; binding < 13; ++binding) {
            VkCopyDescriptorSet copy = {VK_STRUCTURE_TYPE_COPY_DESCRIPTOR_SET};
            copy.srcSet = GN_Vk.descriptorSetPBR;
            copy.srcBinding = binding;
            copy.srcArrayElement = 0;
            copy.dstSet = entry.aSet;
            copy.dstBinding = binding;
            copy.dstArrayElement = 0;
            copy.descriptorCount = 1;
            vkUpdateDescriptorSets(GN_Vk.device, 0, nullptr, 1, &copy);
        }

        VkDescriptorImageInfo albedoInfo = {};
        albedoInfo.sampler = entry.aAlbedoSampler ? entry.aAlbedoSampler : GN_Vk.defaultSampler;
        albedoInfo.imageView = entry.aAlbedo ? entry.aAlbedo : GN_Vk.defaultImageView;
        albedoInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        VkDescriptorImageInfo normalInfo = {};
        normalInfo.sampler = entry.aNormalSampler ? entry.aNormalSampler : GN_Vk.defaultSampler;
        normalInfo.imageView = entry.aNormal ? entry.aNormal : GN_Vk.defaultImageView;
        normalInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        VkWriteDescriptorSet writes[2] = {{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET},
                                          {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET}};
        writes[0].dstSet = entry.aSet;
        writes[0].dstBinding = 1;
        writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[0].descriptorCount = 1;
        writes[0].pImageInfo = &albedoInfo;
        writes[1].dstSet = entry.aSet;
        writes[1].dstBinding = 3;
        writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[1].descriptorCount = 1;
        writes[1].pImageInfo = &normalInfo;
        vkUpdateDescriptorSets(GN_Vk.device, 2, writes, 0, nullptr);
}

// 前向环集同步：descriptorLayout3D 的 5 个绑定（0 场景/1 纹理/2 光源/3 雾/4 GI）连续，
// 一次 copy 搞定，再覆盖 binding 1 的纹理与采样器。
void syncForwardTextureSet(const sFORWARD_TEXTURE_SET &entry) {
        // 同上：逐绑定复制，避免多绑定连续复制触发校验层 VUID。
        for(uint32_t binding = 0; binding < 5; ++binding) {
            VkCopyDescriptorSet copy = {VK_STRUCTURE_TYPE_COPY_DESCRIPTOR_SET};
            copy.srcSet = GN_Vk.descriptorSet3D;
            copy.srcBinding = binding;
            copy.dstSet = entry.aSet;
            copy.dstBinding = binding;
            copy.descriptorCount = 1;
            vkUpdateDescriptorSets(GN_Vk.device, 0, nullptr, 1, &copy);
        }

        VkDescriptorImageInfo info = {};
        info.sampler = entry.aSampler ? entry.aSampler : GN_Vk.defaultSampler;
        info.imageView = entry.aAlbedo ? entry.aAlbedo : GN_Vk.defaultImageView;
        info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        VkWriteDescriptorSet write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        write.dstSet = entry.aSet;
        write.dstBinding = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.descriptorCount = 1;
        write.pImageInfo = &info;
        vkUpdateDescriptorSets(GN_Vk.device, 1, &write, 0, nullptr);
}

// G-Buffer 环集：由 vk3dPasses.cpp 维护（layout 是 0/1/3 非连续，需要分开拷）。
} // namespace

// 帧首（命令缓冲尚未开始录制、且规范集代际号已变）把规范 set 的 12 个绑定同步到所有纹理 set。
// 校验层结论：在录制中改写「已绑定」的描述符集（未开 UPDATE_AFTER_BIND）会让命令缓冲进入
// invalid state，之后的 bind/draw 全被丢弃 —— 这是 cull.debugLine 全图 0 像素的根因。
void refreshPBRTextureSets() {
    if(GN_Vk.descriptorSetPBR == VK_NULL_HANDLE) {
        return;
    }
    // 代际号没变 = 规范集的静态绑定没改过，环集里的内容仍然有效，直接跳过。
    // 这一条以前每帧对每套环集做 12 次 copy + 2 次 write，纹理一多就是纯浪费。
    const uint32_t gen = GN_VkCanonicalDescGeneration();
    if(gen == s_pbrSyncedGen) return;
    for(size_t i = 0; i < s_pbrTextureSets.size(); ++i) {
        syncPBRTextureSet(s_pbrTextureSets[i]);
    }
    s_pbrSyncedGen = gen;
}

// 前向路径的环集同步（同上：只在帧首、命令缓冲尚未录制时调用）。
void refreshForwardTextureSets() {
    if(GN_Vk.descriptorSet3D == VK_NULL_HANDLE) {
        return;
    }
    const uint32_t gen = GN_VkCanonicalDescGeneration();
    if(gen == s_forwardSyncedGen) return;
    for(size_t i = 0; i < s_forwardTextureSets.size(); ++i) {
        syncForwardTextureSet(s_forwardTextureSets[i]);
    }
    s_forwardSyncedGen = gen;
}

// 释放一套环集（池开了 FREE_DESCRIPTOR_SET_BIT，必须显式归还，否则描述符永久占用）。

// 纹理解放会让环集里缓存的 image view / sampler 句柄失效，整体作废重。
// 只做标记、不在录制中释放，避开「释放仍被引用的描述符集」的时机问题。
void GN_VkRequestTextureSetCacheReset() { s_ringResetRequested = true; }

// 帧首调用：把上帧标记的重置真正做掉（此时上一帧的 fence 已等过）。
void applyRingResetIfRequested() {
    if(!s_ringResetRequested) return;
    s_ringResetRequested = false;
    ReleaseRingSets(s_pbrTextureSets, GN_Vk.descriptorPool, GN_Vk.device);
    ReleaseRingSets(s_forwardTextureSets, GN_Vk.descriptorPool, GN_Vk.device);
    resetGBufferTextureSetCache();
    s_pbrSyncedGen = 0;
    s_forwardSyncedGen = 0;
}

void resetTextureSetCaches() {
    ReleaseRingSets(s_pbrTextureSets, GN_Vk.descriptorPool, GN_Vk.device);
    ReleaseRingSets(s_forwardTextureSets, GN_Vk.descriptorPool, GN_Vk.device);
    resetGBufferTextureSetCache(); // 定义在 vk3dPasses.cpp
    s_pbrSyncedGen = 0;
    s_forwardSyncedGen = 0;
    s_ringResetRequested = false;
}

// P10：最近一次 PBR 绘制提交的 indexCount（诊断/验收钩子）。
namespace {
uint32_t s_last3DIndexCount = 0;
}

uint32_t GN_VkLast3DIndexCount() { return s_last3DIndexCount; }

void GN_VkBegin3DPass(const sMAT44 *viewProj, const sVEC3 *cameraPos,
                      const void *lightUBO, uint32_t lightUBOSize) {
    if(!GN_Vk.initialized || GN_Vk.in3DPass) return;

    // 偏移不按 pass 归零：同一帧可能先录 4 级 shadow pass，归零会覆盖其顶点数据
    // （已录制命令在提交时才读取缓冲）。帧首由 GN_VkBeginFrame 归零，
    // 扩容退休的旧缓冲也只在那里（fence 之后）释放。
    // 纹理集同步已移到 GN_VkBeginFrame（录制开始前）：录制中改写已绑定的 set 会让命令缓冲失效。

    vkValidationEndLabel(GN_Vk.commandBuffers[GN_Vk.currentFrame]);
    vkValidationBeginLabel(GN_Vk.commandBuffers[GN_Vk.currentFrame], "flush2D");
    // 刷新当前 2D 批次
    if(GN_Vk.vcnt > 0) {
        void *d;
        vkMapMemory(GN_Vk.device, GN_Vk.vertexMemory, 0, GN_Vk.vcnt * sizeof(VkVertex), 0, &d);
        memcpy(d, GN_Vk.vbuf, GN_Vk.vcnt * sizeof(VkVertex));
        vkUnmapMemory(GN_Vk.device, GN_Vk.vertexMemory);
        uint32_t prevEnd = 0;
        for(uint32_t i = 0; i < GN_Vk.batchCount; i++) {
            uint32_t count = GN_Vk.batchEnd[i] - prevEnd;
            if(count > 0) {
                vkCmdBindDescriptorSets(GN_Vk.commandBuffers[GN_Vk.currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, GN_Vk.pipelineLayout, 0, 1, &GN_Vk.batchDS[i], 0, 0);
                VkDeviceSize offs[] = {prevEnd * sizeof(VkVertex)};
                vkCmdBindVertexBuffers(GN_Vk.commandBuffers[GN_Vk.currentFrame], 0, 1, &GN_Vk.vertexBuffer, offs);
                vkCmdDraw(GN_Vk.commandBuffers[GN_Vk.currentFrame], count, 1, 0, 0);
            }
            prevEnd = GN_Vk.batchEnd[i];
        }
        GN_Vk.vcnt = 0;
        GN_Vk.batchCount = 0;
        GN_Vk.lastDS = VK_NULL_HANDLE;
    }

    // 结束当前渲染通道 (2D 或 空)
    if(GN_Vk.inRenderPass) {
        vkCmdEndRenderPass(GN_Vk.commandBuffers[GN_Vk.currentFrame]);
        GN_Vk.inRenderPass = false;
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
        rpi.framebuffer = GN_Vk.framebuffers3D[GN_Vk.currentFrame];
        rpi.renderArea = {{0, 0}, GN_Vk.swapchainExtent};
        rpi.clearValueCount = 3;
        rpi.pClearValues = cvs;
        vkValidationEndLabel(GN_Vk.commandBuffers[GN_Vk.currentFrame]);
        vkValidationBeginLabel(GN_Vk.commandBuffers[GN_Vk.currentFrame], "renderPass3D");
        vkCmdBeginRenderPass(GN_Vk.commandBuffers[GN_Vk.currentFrame], &rpi, VK_SUBPASS_CONTENTS_INLINE);
    }

    vkCmdBindPipeline(GN_Vk.commandBuffers[GN_Vk.currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, GN_Vk.pipeline3D);
    vkCmdBindDescriptorSets(GN_Vk.commandBuffers[GN_Vk.currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                            GN_Vk.pipelineLayout3D, 0, 1, &GN_Vk.descriptorSet3D, 0, 0);

    GN_Vk.in3DPass = true;
    GN_Vk.inRenderPass = true; // 3D 渲染通道活跃
}

bool GN_VkIsIn3DPass() {
    return GN_Vk.in3DPass;
}

void GN_VkEnd3DPass() {
    if(!GN_Vk.initialized || !GN_Vk.in3DPass) return;

    vkCmdEndRenderPass(GN_Vk.commandBuffers[GN_Vk.currentFrame]);

    GN_Vk.in3DPass = false;
    GN_Vk.inRenderPass = false; // 3D pass 结束，不重启 2D pass（避免 CLEAR 清掉 3D 内容）
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
    if(!GN_Vk.initialized || !GN_Vk.in3DPass || !vertices || vertexCount == 0) return;

    // ── 绑定本 draw 的纹理描述符集 ──
    // 不能改写已绑定的 descriptorSet3D：本帧命令缓冲已把它绑定并录制过，
    // 再 vkUpdateDescriptorSets 会触发 VUID-vkUpdateDescriptorSets-None-03047，
    // 部分驱动会把整条命令缓冲判为 invalid 并丢弃其后的 draw。
    // 改为每个纹理组合一个私有 set（帧首从规范集同步），这里只做绑定。
    {
        const VkImageView imgView = (texture && texture->vkImageView) ? texture->vkImageView
                                                                     : GN_Vk.defaultImageView;
        const VkSampler sampler =
            (texture && texture->vkSampler) ? texture->vkSampler : GN_Vk.defaultSampler;
        VkDescriptorSet set = findOrCreateForwardSet(imgView, sampler);
        vkCmdBindDescriptorSets(GN_Vk.commandBuffers[GN_Vk.currentFrame],
                                VK_PIPELINE_BIND_POINT_GRAPHICS, GN_Vk.pipelineLayout3D, 0, 1,
                                &set, 0, nullptr);
    }

    // 上传顶点数据 (动态缓冲, 每 draw call 独立区域)
    uint32_t vtxSize = vertexCount * sizeof(VkVertex3D);
    uint32_t idxSize = (indices && indexCount > 0) ? indexCount * sizeof(uint32_t) : 0;
    Ensure3DBufferCapacity(vtxSize, idxSize);

    // 写入顶点到当前偏移
    void *d;
    vkMapMemory(GN_Vk.device, GN_Vk.vertexMemory3D, GN_Vk.vtxOfs3D, vtxSize, 0, &d);
    memcpy(d, vertices, vtxSize);
    vkUnmapMemory(GN_Vk.device, GN_Vk.vertexMemory3D);

    // 绑定管线 (自给自足, 兼容 skybox 等中间管线切换)
    vkCmdBindPipeline(GN_Vk.commandBuffers[GN_Vk.currentFrame],
                      VK_PIPELINE_BIND_POINT_GRAPHICS, GN_Vk.pipeline3D);

    VkDeviceSize offs[] = {GN_Vk.vtxOfs3D};
    vkCmdBindVertexBuffers(GN_Vk.commandBuffers[GN_Vk.currentFrame], 0, 1,
                           &GN_Vk.vertexBuffer3D, offs);
    GN_Vk.vtxOfs3D += vtxSize;

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
        vkMapMemory(GN_Vk.device, GN_Vk.indexMemory3D, GN_Vk.idxOfs3D, idxSize, 0, &id);
        memcpy(id, indices, idxSize);
        vkUnmapMemory(GN_Vk.device, GN_Vk.indexMemory3D);

        vkCmdBindIndexBuffer(GN_Vk.commandBuffers[GN_Vk.currentFrame],
                             GN_Vk.indexBuffer3D, GN_Vk.idxOfs3D, VK_INDEX_TYPE_UINT32);
        GN_Vk.idxOfs3D += idxSize;
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
                    bool alphaBlend, float emissiveR, float emissiveG, float emissiveB,
                    float emissiveIntensity) {
    if(!GN_Vk.initialized || !GN_Vk.in3DPass || !vertices || vertexCount == 0) return;
    s_last3DIndexCount = (indices && indexCount > 0) ? indexCount : 0;

    // ── 每个纹理组合一份独立描述符集（避免单帧内改写同一 set 导致所有网格采样同一贴图）──
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
        VkDescriptorSet pbrSet = findOrCreatePBRSet(albedoView, albedoSamp, normalView, normalSamp);
        vkCmdBindDescriptorSets(GN_Vk.commandBuffers[GN_Vk.currentFrame],
                                VK_PIPELINE_BIND_POINT_GRAPHICS,
                                GN_Vk.pipelineLayoutPBR, 0, 1, &pbrSet, 0, nullptr);
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
    pc.emissive[0] = emissiveR;
    pc.emissive[1] = emissiveG;
    pc.emissive[2] = emissiveB;
    pc.emissive[3] = emissiveIntensity;
    vkCmdPushConstants(GN_Vk.commandBuffers[GN_Vk.currentFrame],
                       GN_Vk.pipelineLayoutPBR,
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(VkModelPushConstPBR), &pc);

    // 上传顶点数据 (每 draw call 独立区域)
    uint32_t vtxSize = vertexCount * sizeof(VkVertex3D);
    uint32_t idxSize = (indices && indexCount > 0) ? indexCount * sizeof(uint32_t) : 0;
    Ensure3DBufferCapacity(vtxSize, idxSize);

    void *d;
    vkMapMemory(GN_Vk.device, GN_Vk.vertexMemory3D, GN_Vk.vtxOfs3D, vtxSize, 0, &d);
    memcpy(d, vertices, vtxSize);
    vkUnmapMemory(GN_Vk.device, GN_Vk.vertexMemory3D);

    // 绑定 PBR 管线：BLEND 材质走透明变体；混合管线缺失时回退不透明（观感退化，不崩）。
    VkPipeline pbrPipeline = alphaBlend ? GN_Vk.pipelinePBRBlend : GN_Vk.pipelinePBR;
    if(!pbrPipeline) pbrPipeline = GN_Vk.pipelinePBR;
    vkCmdBindPipeline(GN_Vk.commandBuffers[GN_Vk.currentFrame],
                      VK_PIPELINE_BIND_POINT_GRAPHICS, pbrPipeline);

    VkDeviceSize offs[] = {GN_Vk.vtxOfs3D};
    vkCmdBindVertexBuffers(GN_Vk.commandBuffers[GN_Vk.currentFrame], 0, 1,
                           &GN_Vk.vertexBuffer3D, offs);
    GN_Vk.vtxOfs3D += vtxSize;

    if(indices && indexCount > 0) {
        void *id;
        vkMapMemory(GN_Vk.device, GN_Vk.indexMemory3D, GN_Vk.idxOfs3D, idxSize, 0, &id);
        memcpy(id, indices, idxSize);
        vkUnmapMemory(GN_Vk.device, GN_Vk.indexMemory3D);

        vkCmdBindIndexBuffer(GN_Vk.commandBuffers[GN_Vk.currentFrame],
                             GN_Vk.indexBuffer3D, GN_Vk.idxOfs3D, VK_INDEX_TYPE_UINT32);
        GN_Vk.idxOfs3D += idxSize;
        vkCmdDrawIndexed(GN_Vk.commandBuffers[GN_Vk.currentFrame], indexCount, 1, 0, 0, 0);
    } else {
        vkCmdDraw(GN_Vk.commandBuffers[GN_Vk.currentFrame], vertexCount, 1, 0, 0);
    }
}

} // namespace GNg
