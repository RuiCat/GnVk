// 3D 绘制 pass：阴影映射、HDR→Swapchain 合成、天空盒、PBR（含 HDR 变体）与延迟渲染。
#include <GN.h>
#include "GN_Vk.h"
#include "vkInternal.h"

#include <cstring>

namespace GNg {

// B7-2（2026-11-26）：pass 状态已收敛进 GN_Vk（c4）——inShadowPass/shadowPassPipeline/
// aSkyParams/aDeferredViewProj/hasDeferredViewProj 见 vkContext.inc（s3DPassRes 与顶层命名成员）。
// shadowPassPipeline 的由来：GN_VkShadowDraw 曾硬绑定 CSM shadowPipeline，cube pass 绑的
// cubeShadowPipeline 立刻被覆盖（静态 viewport 2048，内容缩在右下角）；改为按该开关选管线。

// ═══════════════════════════════════════════════════════════
//  阴影映射 Pass
// ═══════════════════════════════════════════════════════════

void GN_VkShadowPassBegin(const sMAT44 *lightViewProj, uint32_t cascadeIndex) {
    if(!GN_Vk.initialized || !GN_Vk.shadowPipeline || GN_Vk.inShadowPass) return;
    if(GN_Vk.in3DPass) return;
    // 描述符集分配失败时跳过本 pass（不绑非法 set）；阴影降级而非崩溃。
    VkDescriptorSet cascadeSet = shadowCascadeDescriptorSet(cascadeIndex);
    if(cascadeSet == VK_NULL_HANDLE) return;
    // 不重置 3D 绘制偏移、也不释放退休缓冲：4 级 cascade 在同一个 command buffer
    // 内录制，重置偏移会让前一级的 draw 在提交后读到后一级写入的顶点/索引；
    // 偏移由 GN_VkBeginFrame 每帧归零，退休缓冲只在该处 fence 之后释放。

    // 刷新 2D 批次并结束当前渲染通道
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
    if(GN_Vk.inRenderPass) {
        vkCmdEndRenderPass(GN_Vk.commandBuffers[GN_Vk.currentFrame]);
        GN_Vk.inRenderPass = false;
    }

    // 写入本级的 light VP 槽（每级 64 B，槽距 256 B；常驻映射，逐级不互相覆盖）。
    // 旧实现只有 64 B 单槽：4 级录制期间反复覆盖同一槽，提交后所有级都读到最后一级矩阵。
    writeShadowCascadeLightVP(cascadeIndex, lightViewProj->m);

    VkClearValue cv;
    cv.depthStencil = {1.0f, 0};
    VkRenderPassBeginInfo rpi{}; rpi.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpi.renderPass = GN_Vk.shadowRenderPass;
    // L3c：framebuffer 选择对应级联层（view 已绑定 layer，renderArea 仍是单层尺寸）。
    rpi.framebuffer = shadowFramebufferForCascade(cascadeIndex);
    rpi.renderArea = {{0, 0}, {GN_Vk.shadowMapSize, GN_Vk.shadowMapSize}};
    rpi.clearValueCount = 1;
    rpi.pClearValues = &cv;
    vkCmdBeginRenderPass(GN_Vk.commandBuffers[GN_Vk.currentFrame], &rpi, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(GN_Vk.commandBuffers[GN_Vk.currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, GN_Vk.shadowPipeline);
    // 每级绑定自己的描述符集（指向本级 UBO 槽）；单集会再次退化为“所有级同一矩阵”。
    vkCmdBindDescriptorSets(GN_Vk.commandBuffers[GN_Vk.currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                            GN_Vk.shadowPipelineLayout, 0, 1, &cascadeSet, 0, 0);
    GN_Vk.inShadowPass = true;
    GN_Vk.shadowPassPipeline = VK_NULL_HANDLE; // CSM 用默认那条
    GN_Vk.inRenderPass = true; // 阴影渲染通道活跃
}


// ═══════════════════════════════════════════════════════════
//  B2：点光/聚光 cube shadow（6 面）
// ═══════════════════════════════════════════════════════════
//
// 6 个面的朝向按 Vulkan cube 约定（与 OpenGL/D3D 一致的右手系）：
//   +X, -X, +Y, -Y, +Z, -Z
// 每面用 90° 视场 + look-at 构造 view，正交/透视都用透视（点光源是透视的）。
//
// 注意：Vulkan 的 cube 采样方向与"渲染时的 face 索引"必须一致，
// 否则阴影会出现在错误的一侧。这里刻意用与规范一致的 up 向量：
//   ±Y 面不能用 (0,1,0) 当 up（与视线平行，叉积退化），换 (0,0,±1)。
namespace {
// B7-2：inCubeShadowPass/cubeShadowLightPos/cubeShadowLightIndex 已收敛进 GN_Vk.s3DPassRes。
const sVEC3 kCubeFaceDir[6] = {{1.0f, 0.0f, 0.0f},  {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f},
                               {0.0f, -1.0f, 0.0f}, {0.0f, 0.0f, 1.0f},  {0.0f, 0.0f, -1.0f}};
const sVEC3 kCubeFaceUp[6] = {{0.0f, -1.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, {0.0f, 0.0f, 1.0f},
                              {0.0f, 0.0f, -1.0f}, {0.0f, -1.0f, 0.0f}, {0.0f, -1.0f, 0.0f}};
} // namespace

uint32_t GN_VkCubeShadowSize() { return GN_Vk.cubeShadowSize; }

bool GN_VkCubeShadowBegin(const sVEC3 &lightPos, float nearZ, float farZ) {
    // 旧签名：等价于"渲染第 0 盏灯"。保留是为了不破坏已有调用方。
    return GN_VkCubeShadowBeginForLight(0, lightPos, nearZ, farZ);
}

bool GN_VkCubeShadowBeginForLight(uint32_t lightIndex, const sVEC3 &lightPos, float nearZ,
                                 float farZ) {
    if(!GN_Vk.initialized) return false;
    if(GN_Vk.cubeShadowImage == VK_NULL_HANDLE || GN_Vk.cubeShadowRenderPass == VK_NULL_HANDLE) {
        return false;
    }
    if(lightIndex >= GN_MAX_CUBE_SHADOW_LIGHTS) return false;
    if(GN_Vk.in3DPass || GN_Vk.inShadowPass || GN_Vk.inCubeShadowPass) return false;
    if(!(farZ > nearZ + 1.0e-4f)) return false;
    GN_Vk.cubeShadowLightPos = lightPos;
    GN_Vk.cubeShadowLightIndex = lightIndex;

    // 与 CSM 的 ShadowPassBegin 同理：先把挂起的 2D 批次刷掉、结束当前 render pass，
    // 否则在 render pass 内再 BeginRenderPass 是非法的。
    if(GN_Vk.vcnt > 0) {
        void *d;
        vkMapMemory(GN_Vk.device, GN_Vk.vertexMemory, 0, GN_Vk.vcnt * sizeof(VkVertex), 0, &d);
        memcpy(d, GN_Vk.vbuf, GN_Vk.vcnt * sizeof(VkVertex));
        vkUnmapMemory(GN_Vk.device, GN_Vk.vertexMemory);
        uint32_t prevEnd = 0;
        for(uint32_t i = 0; i < GN_Vk.batchCount; i++) {
            uint32_t count = GN_Vk.batchEnd[i] - prevEnd;
            if(count > 0) {
                vkCmdBindDescriptorSets(GN_Vk.commandBuffers[GN_Vk.currentFrame],
                                        VK_PIPELINE_BIND_POINT_GRAPHICS, GN_Vk.pipelineLayout, 0,
                                        1, &GN_Vk.batchDS[i], 0, 0);
                VkDeviceSize offs[] = {prevEnd * sizeof(VkVertex)};
                vkCmdBindVertexBuffers(GN_Vk.commandBuffers[GN_Vk.currentFrame], 0, 1,
                                       &GN_Vk.vertexBuffer, offs);
                vkCmdDraw(GN_Vk.commandBuffers[GN_Vk.currentFrame], count, 1, 0, 0);
            }
            prevEnd = GN_Vk.batchEnd[i];
        }
        GN_Vk.vcnt = 0;
        GN_Vk.batchCount = 0;
        GN_Vk.lastDS = VK_NULL_HANDLE;
    }
    if(GN_Vk.inRenderPass) {
        vkCmdEndRenderPass(GN_Vk.commandBuffers[GN_Vk.currentFrame]);
        GN_Vk.inRenderPass = false;
    }
    GN_Vk.inCubeShadowPass = true;
    return true;
}

void GN_VkCubeShadowFaceBegin(const sVEC3 &lightPos, uint32_t face, float nearZ, float farZ) {
    if(!GN_Vk.inCubeShadowPass || face >= 6) return;
    // 帧缓冲索引 = 灯号 * 6 + 面号（见 GN_CUBE_SHADOW_TOTAL_LAYERS 的说明）
    const uint32_t slot = GN_Vk.cubeShadowLightIndex * 6u + face;
    if(slot >= GN_CUBE_SHADOW_TOTAL_LAYERS) return;
    if(GN_Vk.cubeShadowFramebuffer[slot] == VK_NULL_HANDLE) {
        fprintf(stderr, "[cube] face %u (light %u) framebuffer NULL\n", face,
                GN_Vk.cubeShadowLightIndex);
        return;
    }
    if(GN_Vk.cubeShadowPipeline == VK_NULL_HANDLE) {
        static bool warned = false;
        if(!warned) { warned = true; fprintf(stderr, "[cube] cubeShadowPipeline NULL\n"); }
        return;
    }

    // 构造该面的 view-proj：90° 透视 + 朝面方向看
    const sVEC3 target = {lightPos.x + kCubeFaceDir[face].x, lightPos.y + kCubeFaceDir[face].y,
                          lightPos.z + kCubeFaceDir[face].z};
    sMAT44 view;
    Mat44LookAtRH(&view, &lightPos, &target, &kCubeFaceUp[face]);
    sMAT44 proj;
    Mat44PerspectiveVK(&proj, 1.5707963f, 1.0f, nearZ, farZ); // 90°, 正方形
    sMAT44 viewProj;
    Mat44Mul(&viewProj, &proj, &view);
    // 阴影 pass 的 push constant 就是模型矩阵（与 CSM 一致），light VP 走 UBO 槽 0。
    // cube 与 CSM 共用 shadowPipelineLayout / shadowPipeline（同一个 depth-only 管线），
    // 所以这里同样写到槽 0 —— 但**不能**和 CSM 同帧交叉使用：CSM 在帧首一次录完 4 级，
    // cube 在其后录 6 面，两者各自读自己那一次写入的值（命令缓冲顺序保证）。
    // 每面写自己的槽（4..9）。写同一个槽会让 6 个面都用最后一次的矩阵。
    writeCubeShadowLightVP(slot, viewProj.m); // slot = light*6 + face

    VkClearValue cv;
    cv.depthStencil = {1.0f, 0};
    VkRenderPassBeginInfo rpi{}; rpi.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpi.renderPass = GN_Vk.cubeShadowRenderPass;
    rpi.framebuffer = GN_Vk.cubeShadowFramebuffer[slot];
    rpi.renderArea = {{0, 0}, {GN_Vk.cubeShadowSize, GN_Vk.cubeShadowSize}};
    rpi.clearValueCount = 1;
    rpi.pClearValues = &cv;
    vkCmdBeginRenderPass(GN_Vk.commandBuffers[GN_Vk.currentFrame], &rpi,
                         VK_SUBPASS_CONTENTS_INLINE);
    // **必须用 cube 专用管线**：CSM 那条的 viewport 写死 2048，用它渲染 1024 的
    // framebuffer 会把内容缩到右下角（实测 bbox x[686,1023] y[683,767]）。
    vkCmdBindPipeline(GN_Vk.commandBuffers[GN_Vk.currentFrame],
                      VK_PIPELINE_BIND_POINT_GRAPHICS, GN_Vk.cubeShadowPipeline);
    // 动态 viewport/scissor：覆盖整张面
    VkViewport vp = {0.0f, 0.0f, (float)GN_Vk.cubeShadowSize, (float)GN_Vk.cubeShadowSize, 0.0f,
                     1.0f};
    VkRect2D sc = {{0, 0}, {GN_Vk.cubeShadowSize, GN_Vk.cubeShadowSize}};
    vkCmdSetViewport(GN_Vk.commandBuffers[GN_Vk.currentFrame], 0, 1, &vp);
    vkCmdSetScissor(GN_Vk.commandBuffers[GN_Vk.currentFrame], 0, 1, &sc);
    VkDescriptorSet set = cubeShadowFaceDescriptorSet(slot);
    if(set != VK_NULL_HANDLE) {
        vkCmdBindDescriptorSets(GN_Vk.commandBuffers[GN_Vk.currentFrame],
                                VK_PIPELINE_BIND_POINT_GRAPHICS, GN_Vk.shadowPipelineLayout, 0, 1,
                                &set, 0, 0);
    }
    GN_Vk.shadowPassPipeline = GN_Vk.cubeShadowPipeline; // 让 GN_VkShadowDraw 用 cube 那条
    GN_Vk.inShadowPass = true; // 让 GN_VkShadowDraw 认这个 pass
    GN_Vk.inRenderPass = true;
}

void GN_VkCubeShadowFaceEnd() {
    if(!GN_Vk.inCubeShadowPass || !GN_Vk.inShadowPass) return;
    vkCmdEndRenderPass(GN_Vk.commandBuffers[GN_Vk.currentFrame]);
    GN_Vk.inShadowPass = false;
    GN_Vk.inRenderPass = false;
}

void GN_VkCubeShadowEnd() {
    if(!GN_Vk.inCubeShadowPass) return;
    if(GN_Vk.inShadowPass) GN_VkCubeShadowFaceEnd();
    GN_Vk.inCubeShadowPass = false;
    // 记录"本帧至少渲染到第几盏灯"。着色器侧看到某灯的 layer >= 0 才会去采样，
    // 所以这里只是给诊断/验收用（GN_VkCubeShadowLightCount）。
    if(GN_Vk.cubeShadowLightCount < GN_Vk.cubeShadowLightIndex + 1) {
        GN_Vk.cubeShadowLightCount = GN_Vk.cubeShadowLightIndex + 1;
    }
}

uint32_t GN_VkCubeShadowLightCount() { return GN_Vk.cubeShadowLightCount; }

void GN_VkResetCubeShadowLightCount() { GN_Vk.cubeShadowLightCount = 0; }

void GN_VkShadowPassEnd() {
    if(!GN_Vk.initialized || !GN_Vk.inShadowPass) return;
    vkCmdEndRenderPass(GN_Vk.commandBuffers[GN_Vk.currentFrame]);
    GN_Vk.inShadowPass = false;
    GN_Vk.inRenderPass = false; // 阴影渲染通道结束，无活跃通道
}

void GN_VkShadowDraw(const void *vertices, uint32_t vertexCount,
                     const uint32_t *indices, uint32_t indexCount,
                     const sMAT44 *modelMatrix) {
    if(!GN_Vk.initialized || !GN_Vk.inShadowPass || !vertices || vertexCount == 0) return;

    // Push model matrix
    vkCmdPushConstants(GN_Vk.commandBuffers[GN_Vk.currentFrame],
                       GN_Vk.shadowPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, 64, modelMatrix->m);

    // 上传顶点 (每 draw call 独立区域)
    uint32_t vtxSize = vertexCount * sizeof(VkVertex3D);
    uint32_t idxSize = (indices && indexCount > 0) ? indexCount * sizeof(uint32_t) : 0;
    Ensure3DBufferCapacity(vtxSize, idxSize);

    void *d;
    vkMapMemory(GN_Vk.device, GN_Vk.vertexMemory3D, GN_Vk.vtxOfs3D, vtxSize, 0, &d);
    memcpy(d, vertices, vtxSize);
    vkUnmapMemory(GN_Vk.device, GN_Vk.vertexMemory3D);

    // 绑定阴影管线：优先用 pass 指定的那条（cube 需要它自己的动态 viewport 版本）
    const VkPipeline activePipeline =
        (GN_Vk.shadowPassPipeline != VK_NULL_HANDLE) ? GN_Vk.shadowPassPipeline : GN_Vk.shadowPipeline;
    vkCmdBindPipeline(GN_Vk.commandBuffers[GN_Vk.currentFrame],
                      VK_PIPELINE_BIND_POINT_GRAPHICS, activePipeline);

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

// ═══════════════════════════════════════════════════════════
//  HDR→Swapchain 合成 (Bloom + 色调映射)
//  必须在 GN_VkEnd3DPass() 之后调用 (3D 已结束, HDR 纹理就绪)
// ═══════════════════════════════════════════════════════════
void GN_VkCompositeToSwapchain() {
    if(!GN_Vk.initialized || !GN_Vk.compositePipeline) return;
    if(!GN_Vk.hdrView || !GN_Vk.hdrSampler) return;
    if(!isPostReady()) return; // L1：POST UBO 未就绪时不录 draw（binding 2 未绑定）
    if(!isBloomReady()) return; // L1d：Bloom 结果图未就绪（binding 3 未绑定）

    // 更新 composite descriptor: 绑定当前 HDR 纹理
    VkDescriptorImageInfo hii = {};
    hii.sampler = GN_Vk.hdrSampler;
    hii.imageView = GN_Vk.hdrView;
    hii.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    VkWriteDescriptorSet wc{}; wc.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    wc.dstSet = GN_Vk.compositeDescriptorSet;
    wc.dstBinding = 1;
    wc.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    wc.descriptorCount = 1;
    wc.pImageInfo = &hii;
    vkUpdateDescriptorSets(GN_Vk.device, 1, &wc, 0, nullptr);

    // L1d：先录制 Bloom mip 链（HDR → 半分辨率亮部 → 逐级下采样 → tent 上采样）。
    recordBloomPasses(GN_Vk.commandBuffers[GN_Vk.currentFrame]);

    // 开始合成 pass → swapchain
    VkClearValue cv;
    cv.color = {{0.0f, 0.0f, 0.0f, 1.0f}};
    VkRenderPassBeginInfo rpi{}; rpi.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
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
    GN_Vk.inRenderPass = false;
}

// ═══════════════════════════════════════════════════════════
//  天空盒渲染
//  必须在一个活跃的 RenderPass 中调用 (在 Clear 之后, 3D 绘制之前)
// ═══════════════════════════════════════════════════════════
void GN_VkSetSky(const VkSkyParams &params) { GN_Vk.aSkyParams = params; }
// G3：环境重烘读取当前天空参数（天气/时间）。
void GN_VkGetSky(VkSkyParams *out) {
    if(out) *out = GN_Vk.aSkyParams;
}

void GN_VkDrawSkybox(const sMAT44 *viewProj) {
    if(!GN_Vk.initialized || !GN_Vk.skyboxPipeline || !GN_Vk.in3DPass) return;

    // UBO: viewProj(64) + VkSkyParams(80) = 144 B
    float ubo[36];
    memcpy(ubo, viewProj->m, 64);
    memcpy(ubo + 16, &GN_Vk.aSkyParams, sizeof(VkSkyParams));
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
    if(!GN_Vk.initialized || GN_Vk.in3DPass) return;
    // 偏移不按 pass 归零：同一帧可能先录 4 级 shadow pass，归零会覆盖其顶点数据
    // （已录制命令在提交时才读取缓冲）。帧首由 GN_VkBeginFrame 归零。
    // 刷新 2D 批次 (与 GN_VkBegin3DPass 相同逻辑)
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
    // 结束当前渲染通道 (2D 或 空 — 阴影 Pass 后可能已无活跃通道)
    if(GN_Vk.inRenderPass) {
        vkCmdEndRenderPass(GN_Vk.commandBuffers[GN_Vk.currentFrame]);
        GN_Vk.inRenderPass = false;
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
    // L4：sky-view LUT 重建（参数/太阳变化时；render pass 之外录制）
    recordAtmosphereLutUpdate(GN_Vk.commandBuffers[GN_Vk.currentFrame]);
    // 开始 3D render pass (PBR)
    {
        VkClearValue cvs[3];
        cvs[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
        cvs[1].depthStencil = {1.0f, 0};
        cvs[2].color = {{0.0f, 0.0f, 0.0f, 1.0f}}; // resolve clear
        VkRenderPassBeginInfo rpi{}; rpi.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rpi.renderPass = GN_Vk.renderPass3D;
        rpi.framebuffer = GN_Vk.framebuffers3D[GN_Vk.currentFrame];
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
    GN_Vk.in3DPass = true;
    GN_Vk.inRenderPass = true; // 3D 渲染通道活跃
}

// ═══════════════════════════════════════════════════════════
//  3D PBR Pass — HDR 变体（L1 起与 GN_VkBegin3DPassPBR 同一实现：
//  renderPass3D 统一 resolve 到 HDR 目标，由合成阶段曝光/tonemap）
// ═══════════════════════════════════════════════════════════
void GN_VkBegin3DPassPBR_HDR(const sMAT44 *viewProj, const sVEC3 *cameraPos,
                             const void *lightUBO, uint32_t lightUBOSize) {
    GN_VkBegin3DPassPBR(viewProj, cameraPos, lightUBO, lightUBOSize);
}

// ═══════════════════════════════════════════════════════════
//  延迟渲染: G-Buffer Pass
// ═══════════════════════════════════════════════════════════
void GN_VkBegin3DPassDeferred(const sMAT44 *viewProj, const sVEC3 *cameraPos,
                              const void *lightUBO, uint32_t lightUBOSize) {
    if(!GN_Vk.initialized || GN_Vk.in3DPass || !GN_Vk.gBufferPipeline) return;
    GN_Vk.aDeferredViewProj = *viewProj;
    GN_Vk.hasDeferredViewProj = true;
    // 同 PBR 路径：偏移按帧单调追加，帧首归零（避免覆盖同帧阴影 pass 的顶点数据）。
    // 刷新 2D 批次
    if(GN_Vk.vcnt > 0) {
        void *d;
        vkMapMemory(GN_Vk.device, GN_Vk.vertexMemory, 0, GN_Vk.vcnt * sizeof(VkVertex), 0, &d);
        memcpy(d, GN_Vk.vbuf, GN_Vk.vcnt * sizeof(VkVertex));
        vkUnmapMemory(GN_Vk.device, GN_Vk.vertexMemory);
        uint32_t prevEnd = 0;
        for(uint32_t i = 0; i < GN_Vk.batchCount; i++) {
            uint32_t count = GN_Vk.batchEnd[i] - prevEnd;
            if(count > 0) { vkCmdDraw(GN_Vk.commandBuffers[GN_Vk.currentFrame], count, 1, 0, 0); }
            prevEnd = GN_Vk.batchEnd[i];
        }
        GN_Vk.vcnt = 0;
        GN_Vk.batchCount = 0;
        GN_Vk.lastDS = VK_NULL_HANDLE;
    }
    if(GN_Vk.inRenderPass) {
        vkCmdEndRenderPass(GN_Vk.commandBuffers[GN_Vk.currentFrame]);
        GN_Vk.inRenderPass = false;
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
        // L4：sky-view LUT 重建（G-Buffer pass 之前，render pass 之外）
        recordAtmosphereLutUpdate(GN_Vk.commandBuffers[GN_Vk.currentFrame]);
        VkRenderPassBeginInfo rpi{}; rpi.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rpi.renderPass = GN_Vk.gBufferRenderPass;
        rpi.framebuffer = GN_Vk.gBufferFramebuffers[0];
        rpi.renderArea = {{0, 0}, GN_Vk.swapchainExtent};
        rpi.clearValueCount = 4;
        rpi.pClearValues = cvs;
        vkCmdBeginRenderPass(GN_Vk.commandBuffers[GN_Vk.currentFrame], &rpi, VK_SUBPASS_CONTENTS_INLINE);
    }
    vkCmdBindPipeline(GN_Vk.commandBuffers[GN_Vk.currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, GN_Vk.gBufferPipeline);
    vkCmdBindDescriptorSets(GN_Vk.commandBuffers[GN_Vk.currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, GN_Vk.gBufferPipelineLayout, 0, 1, &GN_Vk.gBufferDescriptorSet, 0, 0);
    GN_Vk.in3DPass = true;
    GN_Vk.inRenderPass = true;
}

// G2b：SSAO Pass（在 GN_VkDraw3D_GBuffer 之后、GN_VkDeferredLighting 之前调用）。
// GI UBO 的 indirect.w<=0 时直接返回（默认关，不跑 pass，观感/开销不变）。
void GN_VkSSAOPass() {
    if(!GN_Vk.initialized || GN_Vk.ssaoPipeline == VK_NULL_HANDLE) return;
    if(!isSsaoEnabled() || !GN_Vk.hasDeferredViewProj) return;
    if(GN_Vk.in3DPass) {
        vkCmdEndRenderPass(GN_Vk.commandBuffers[GN_Vk.currentFrame]);
        GN_Vk.in3DPass = false;
        GN_Vk.inRenderPass = false;
    }
    updateSsaoUniforms(GN_Vk.aDeferredViewProj);
    recordSsaoPasses(GN_Vk.commandBuffers[GN_Vk.currentFrame]);
    GN_Vk.inRenderPass = false;
}

// G4：SSGI Pass（在 GN_VkSSAOPass 之后、GN_VkDeferredLighting 之前调用）。
// GI UBO 的 envParams.w<=0 时直接返回（默认关，不跑 pass，观感/开销不变）。
void GN_VkSSGIPass() {
    if(!GN_Vk.initialized || GN_Vk.ssgiPipeline == VK_NULL_HANDLE) return;
    if(!isSsgiEnabled() || !GN_Vk.hasDeferredViewProj) return;
    if(GN_Vk.in3DPass) {
        vkCmdEndRenderPass(GN_Vk.commandBuffers[GN_Vk.currentFrame]);
        GN_Vk.in3DPass = false;
        GN_Vk.inRenderPass = false;
    }
    updateSsgiUniforms(GN_Vk.aDeferredViewProj);
    recordSsgiPasses(GN_Vk.commandBuffers[GN_Vk.currentFrame]);
    GN_Vk.inRenderPass = false;
}

// ═══════════════════════════════════════════════════════════
//  延迟光照 Pass (读取 G-Buffer → 光照计算 → swapchain)
// ═══════════════════════════════════════════════════════════
// ═══════════════════════════════════════════════════════════
//  G-Buffer 绘制 (复用 PBR push constant, 写入 G-Buffer MRT)
// ═══════════════════════════════════════════════════════════
// ═══════════════════════════════════════════════════════
//  G-Buffer 纹理环集
//  与 vk3dForward.cpp 的 PBR 环集同一理由：gBufferDescriptorSet 在本帧命令缓冲里已绑定，
//  再改写就会触发 VUID-vkUpdateDescriptorSets-None-03047（命令缓冲 invalid，后续 draw 被丢弃）。
//  改为每个 (albedo, normal) 组合一个私有 set，帧首从规范集同步。
//  注：G-Buffer 布局是 0/1/3（binding 2 不存在），不能一次 copy 0..3，需分开拷。
// ═══════════════════════════════════════════════════════
namespace {

struct sGBUFFER_TEXTURE_SET {
    VkImageView aAlbedo;
    VkSampler aAlbedoSampler;
    VkImageView aNormal;
    VkSampler aNormalSampler;
    VkDescriptorSet aSet;
};

std::vector<sGBUFFER_TEXTURE_SET> s_gbufferTextureSets;

void syncGBufferTextureSet(const sGBUFFER_TEXTURE_SET &entry) {
    // 只拷 binding 0（SceneUBO）。binding 1/3 在本函数末尾直接写成本环集的纹理，
    // 没必要先从规范集拷过来再覆盖。binding 2 在 G-Buffer 布局里**不存在**
    // （槽位 2 是光照 UBO，G-Buffer pass 不读光源），所以不能一次 copy 0..3。
    VkCopyDescriptorSet copy{}; copy.sType = VK_STRUCTURE_TYPE_COPY_DESCRIPTOR_SET;
    copy.srcSet = GN_Vk.gBufferDescriptorSet;
    copy.srcBinding = 0;
    copy.dstSet = entry.aSet;
    copy.dstBinding = 0;
    copy.descriptorCount = 1;
    vkUpdateDescriptorSets(GN_Vk.device, 0, nullptr, 1, &copy);

    VkDescriptorImageInfo albedoInfo = {};
    albedoInfo.sampler = entry.aAlbedoSampler ? entry.aAlbedoSampler : GN_Vk.defaultSampler;
    albedoInfo.imageView = entry.aAlbedo ? entry.aAlbedo : GN_Vk.defaultImageView;
    albedoInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    VkDescriptorImageInfo normalInfo = {};
    normalInfo.sampler = entry.aNormalSampler ? entry.aNormalSampler : GN_Vk.defaultSampler;
    normalInfo.imageView = entry.aNormal ? entry.aNormal : GN_Vk.defaultImageView;
    normalInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    VkWriteDescriptorSet writes[2]{}; writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET; writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
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

VkDescriptorSet findOrCreateGBufferSet(VkImageView albedo, VkSampler albedoSampler,
                                       VkImageView normal, VkSampler normalSampler) {
    for(size_t i = 0; i < s_gbufferTextureSets.size(); ++i) {
        const sGBUFFER_TEXTURE_SET &entry = s_gbufferTextureSets[i];
        if(entry.aAlbedo == albedo && entry.aAlbedoSampler == albedoSampler &&
           entry.aNormal == normal && entry.aNormalSampler == normalSampler) {
            return entry.aSet;
        }
    }
    if(GN_Vk.descriptorPool == VK_NULL_HANDLE || GN_Vk.gBufferDescriptorLayout == VK_NULL_HANDLE) {
        return GN_Vk.gBufferDescriptorSet;
    }
    if(s_gbufferTextureSets.size() >= kTextureSetCacheCap) {
        return GN_Vk.gBufferDescriptorSet; // 录制中不释放、不改写规范集
    }
    VkDescriptorSetAllocateInfo dai{}; dai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    dai.descriptorPool = GN_Vk.descriptorPool;
    dai.descriptorSetCount = 1;
    dai.pSetLayouts = &GN_Vk.gBufferDescriptorLayout;
    VkDescriptorSet set = VK_NULL_HANDLE;
    if(vkAllocateDescriptorSets(GN_Vk.device, &dai, &set) != VK_SUCCESS) {
        fprintf(stderr, "[GN_Vk] G-Buffer 纹理描述符集分配失败（回退共享 set）\n");
        return GN_Vk.gBufferDescriptorSet;
    }
    sGBUFFER_TEXTURE_SET entry;
    entry.aAlbedo = albedo;
    entry.aAlbedoSampler = albedoSampler;
    entry.aNormal = normal;
    entry.aNormalSampler = normalSampler;
    entry.aSet = set;
    syncGBufferTextureSet(entry); // 首用前补齐（环集未绑定过，合法）
    s_gbufferTextureSets.push_back(entry);
    return set;
}

} // namespace

// 帧首（命令缓冲未开始录制）把规范集的 0/3 号绑定同步到所有 G-Buffer 环集。
void refreshGBufferTextureSets() {
    if(GN_Vk.gBufferDescriptorSet == VK_NULL_HANDLE) return;
    for(size_t i = 0; i < s_gbufferTextureSets.size(); ++i) {
        syncGBufferTextureSet(s_gbufferTextureSets[i]);
    }
}

void resetGBufferTextureSetCache() {
    ReleaseRingSets(s_gbufferTextureSets, GN_Vk.descriptorPool, GN_Vk.device);
}

void GN_VkDraw3D_GBuffer(const void *vertices, uint32_t vertexCount,
                         const uint32_t *indices, uint32_t indexCount,
                         const sMAT44 *modelMatrix,
                         float albedoR, float albedoG, float albedoB,
                         float roughness, float metallic, float ao,
                         const sIMAGE *albedoTex, const sIMAGE *normalTex,
                         float alphaCutoff) {
    if(!GN_Vk.initialized || !GN_Vk.in3DPass || !vertices || vertexCount == 0) return;
    if(!GN_Vk.gBufferPipeline) return;

    // 不能改写已绑定的 gBufferDescriptorSet（本帧已录制），改为绑定纹理环集。
    {
        const VkImageView aView =
            (albedoTex && albedoTex->vkImageView) ? albedoTex->vkImageView : GN_Vk.defaultImageView;
        const VkSampler aSamp =
            (albedoTex && albedoTex->vkSampler) ? albedoTex->vkSampler : GN_Vk.defaultSampler;
        const VkImageView nView =
            (normalTex && normalTex->vkImageView) ? normalTex->vkImageView : GN_Vk.defaultImageView;
        const VkSampler nSamp =
            (normalTex && normalTex->vkSampler) ? normalTex->vkSampler : GN_Vk.defaultSampler;
        VkDescriptorSet set = findOrCreateGBufferSet(aView, aSamp, nView, nSamp);
        vkCmdBindDescriptorSets(GN_Vk.commandBuffers[GN_Vk.currentFrame],
                                VK_PIPELINE_BIND_POINT_GRAPHICS, GN_Vk.gBufferPipelineLayout, 0,
                                1, &set, 0, 0);
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
    vkMapMemory(GN_Vk.device, GN_Vk.vertexMemory3D, GN_Vk.vtxOfs3D, vtxSize, 0, &d);
    memcpy(d, vertices, vtxSize);
    vkUnmapMemory(GN_Vk.device, GN_Vk.vertexMemory3D);
    vkCmdBindPipeline(GN_Vk.commandBuffers[GN_Vk.currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, GN_Vk.gBufferPipeline);
    VkDeviceSize offs[] = {GN_Vk.vtxOfs3D};
    vkCmdBindVertexBuffers(GN_Vk.commandBuffers[GN_Vk.currentFrame], 0, 1, &GN_Vk.vertexBuffer3D, offs);
    GN_Vk.vtxOfs3D += vtxSize;
    if(indices && indexCount > 0) {
        void *id;
        vkMapMemory(GN_Vk.device, GN_Vk.indexMemory3D, GN_Vk.idxOfs3D, idxSize, 0, &id);
        memcpy(id, indices, idxSize);
        vkUnmapMemory(GN_Vk.device, GN_Vk.indexMemory3D);
        vkCmdBindIndexBuffer(GN_Vk.commandBuffers[GN_Vk.currentFrame], GN_Vk.indexBuffer3D, GN_Vk.idxOfs3D, VK_INDEX_TYPE_UINT32);
        GN_Vk.idxOfs3D += idxSize;
        vkCmdDrawIndexed(GN_Vk.commandBuffers[GN_Vk.currentFrame], indexCount, 1, 0, 0, 0);
    } else {
        vkCmdDraw(GN_Vk.commandBuffers[GN_Vk.currentFrame], vertexCount, 1, 0, 0);
    }
}

void GN_VkDeferredLighting(const sMAT44 *viewProj, const sVEC3 *cameraPos,
                           const void *lightUBO, uint32_t lightUBOSize) {
    if(!GN_Vk.initialized || !GN_Vk.deferredLightPipeline) return;
    // 必须在 G-Buffer Pass 结束后调用
    if(GN_Vk.in3DPass) {
        vkCmdEndRenderPass(GN_Vk.commandBuffers[GN_Vk.currentFrame]);
        GN_Vk.in3DPass = false;
        GN_Vk.inRenderPass = false;
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
    // L4：sky-view LUT 重建（延迟光照之前；若本帧无 3D pass 也保证 LUT 与太阳同步）
    recordAtmosphereLutUpdate(GN_Vk.commandBuffers[GN_Vk.currentFrame]);
    // 开始延迟光照 Pass → swapchain
    {
        VkClearValue cv;
        cv.color = {{0, 0, 0, 1}};
        VkRenderPassBeginInfo rpi{}; rpi.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
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
    GN_Vk.inRenderPass = false;
    // L1：延迟光照写入 HDR 目标后统一走合成（曝光/色调映射/Bloom）。
    GN_VkCompositeToSwapchain();
}

} // namespace GNg
