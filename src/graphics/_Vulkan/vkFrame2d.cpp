// 2D 帧循环：帧开始/结束、顶点批状态、scissor、延迟上传队列与批提交。
#include <GN.h>
#include "GN_Vk.h"
#include "vkInternal.h"
#include "vkValidation.h" // 校验模式下的帧内命令缓冲分段标签

#include <cstring>

namespace GNg {

// ── Scissor 裁剪状态 ──────────────────────────────────────
VkRect2D s_currentScissor = {{0, 0}, {0, 0}};
bool s_scissorEnabled = false;
bool s_scissorDirty = false; // 首次由 GN_VkBeginFrame 设置

// ── 延迟纹理上传队列（帧首处理）──────────────────────────
sPendingUpload s_pendingUploads[256]; // 支持大量纹理(文件浏览器等)
int s_pendingUploadCnt = 0;

// ── 渲染通道状态 ─────────────────────────────────────────
bool s_inRenderPass = false;

// ── 顶点批状态 ───────────────────────────────────────────
VkVertex *gVBuf = 0;
uint32_t gVMax = 0, gVCnt = 0;
VkDescriptorSet s_lastDS = VK_NULL_HANDLE;
VkDescriptorSet s_batchDS[256];
uint32_t s_batchEnd[256]; // cumulative vertex count at each batch boundary
uint32_t s_batchCount = 0;

void GN_VkSetScissor(int x, int y, int w, int h, bool enable) {
    s_scissorEnabled = enable;
    if(enable) {
        s_currentScissor.offset = {x, y};
        s_currentScissor.extent = {(uint32_t)w, (uint32_t)h};
    } else {
        s_currentScissor.offset = {0, 0};
        s_currentScissor.extent = GN_Vk.swapchainExtent;
    }
    s_scissorDirty = true;
}

// Record batch boundary when descriptor set changes
void BatchSwitch(VkDescriptorSet ds) {
    if(ds == s_lastDS) return;
    if(gVCnt > 0 && s_batchCount < 255) {
        s_batchEnd[s_batchCount] = gVCnt;
        s_batchDS[s_batchCount] = s_lastDS;
        s_batchCount++;
    }
    s_lastDS = ds;
}

// Flush current batch and apply new scissor if it changed
void FlushBatchIfScissorDirty() {
    if(!s_scissorDirty) return;
    // Force batch boundary before scissor change
    if(gVCnt > 0 && s_batchCount < 255 && s_lastDS != VK_NULL_HANDLE) {
        s_batchEnd[s_batchCount] = gVCnt;
        s_batchDS[s_batchCount] = s_lastDS;
        s_batchCount++;
        s_lastDS = VK_NULL_HANDLE; // force new batch
    }
    vkCmdSetScissor(GN_Vk.commandBuffers[GN_Vk.currentFrame], 0, 1, &s_currentScissor);
    s_scissorDirty = false;
}

void FlushVertexBatch() {
    if(gVCnt == 0) return;
    if(s_batchCount < 256 && s_lastDS != VK_NULL_HANDLE) {
        s_batchEnd[s_batchCount] = gVCnt;
        s_batchDS[s_batchCount] = s_lastDS;
        s_batchCount++;
    }
    void *d;
    vkMapMemory(GN_Vk.device, GN_Vk.vertexMemory, 0, gVCnt * sizeof(VkVertex), 0, &d);
    memcpy(d, gVBuf, gVCnt * sizeof(VkVertex));
    vkUnmapMemory(GN_Vk.device, GN_Vk.vertexMemory);
    uint32_t prevEnd = 0;
    for(uint32_t i = 0; i < s_batchCount; i++) {
        uint32_t count = s_batchEnd[i] - prevEnd;
        if(count == 0) continue;
        vkCmdBindDescriptorSets(GN_Vk.commandBuffers[GN_Vk.currentFrame],
                                VK_PIPELINE_BIND_POINT_GRAPHICS, GN_Vk.pipelineLayout, 0, 1, &s_batchDS[i], 0, 0);
        VkDeviceSize offs[] = {prevEnd * sizeof(VkVertex)};
        vkCmdBindVertexBuffers(GN_Vk.commandBuffers[GN_Vk.currentFrame], 0, 1, &GN_Vk.vertexBuffer, offs);
        vkCmdDraw(GN_Vk.commandBuffers[GN_Vk.currentFrame], count, 1, 0, 0);
        prevEnd = s_batchEnd[i];
    }
    gVCnt = 0;
    s_batchCount = 0;
    s_lastDS = VK_NULL_HANDLE;
}

// Ensure CPU vertex buffer can hold 'need' more vertices
// ⚠️ 已知风险: vkDestroyBuffer/vkFreeMemory 在未等待帧完成时调用,
//    可能导致 device lost。此处在每帧 BeginFrame 前无显式同步。
//    CreateBuf 内部 vkCreateBuffer/vkAllocateMemory 可能分配失败
//    但不做错误处理(函数为void, 调用者不检查) — 依赖 vulkan validation layers 捕获。
void GrowVBufIfNeeded(uint32_t need) {
    if(gVCnt + need <= gVMax) return;
    while(gVCnt + need > gVMax) gVMax *= 2;
    VkVertex *nb = new VkVertex[gVMax];
    memcpy(nb, gVBuf, gVCnt * sizeof(VkVertex));
    delete[] gVBuf;
    gVBuf = nb;
    vkDestroyBuffer(GN_Vk.device, GN_Vk.vertexBuffer, 0);
    vkFreeMemory(GN_Vk.device, GN_Vk.vertexMemory, 0);
    CreateBuf(gVMax * sizeof(VkVertex), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
              GN_Vk.vertexBuffer, GN_Vk.vertexMemory);
}

bool GN_VkBeginFrame() {
    if(!GN_Vk.initialized) return false;
    // pass 内延迟纹理上传：必须在 vkWaitForFences 之后处理，否则 GPU 还在用旧帧资源。
    vkWaitForFences(GN_Vk.device, 1, &GN_Vk.inFlightFence, VK_TRUE, UINT64_MAX);
    // 3D 动态缓冲退休表：只有在这里（上一帧 fence 已完成）释放才安全。
    // CSM 4 级在同一 command buffer 内录制，pass 内释放会命中同帧 UAF。
    releaseRetired3DBuffers();
    // 3D 绘制偏移按帧单调追加（阴影 4 级 + 主 pass），帧首归零。
    // 旧实现在每个 pass 开头归零，4 级 cascade 的 draw 会在提交前互相覆盖顶点数据。
    s_vtxOfs3D = 0;
    s_idxOfs3D = 0;
    // 描述符集同步必须在命令缓冲开始录制之前（录制中改写已绑定的 set 会让 CB 失效）。
    applyRingResetIfRequested(); // 纹理解放后作废旧环集（含 vkFreeDescriptorSets，必须先于本帧分配）
    refreshPBRTextureSets();
    refreshForwardTextureSets(); // 前向 Blinn-Phong 的纹理环集
    refreshGBufferTextureSets(); // G-Buffer 的纹理环集
    // 处理pass内延迟的纹理上传 (必须在vkWaitForFences之后)
    for(int i = 0; i < s_pendingUploadCnt; i++) {
        if(s_pendingUploads[i].img && s_pendingUploads[i].data) {
            s_pendingUploads[i].img->DataFill(s_pendingUploads[i].data,
                                              s_pendingUploads[i].fmt, s_pendingUploads[i].w, s_pendingUploads[i].h, 0, 0);
        }
        delete[] s_pendingUploads[i].data;
    }
    s_pendingUploadCnt = 0;
    vkResetFences(GN_Vk.device, 1, &GN_Vk.inFlightFence);
    if(GN_Vk.headless) {
        // 无窗口：没有交换链可 acquire，离屏图按帧轮转（fence 已在上面等过，复用安全）
        const uint32_t n = GN_Vk.swapchainImageCount ? GN_Vk.swapchainImageCount : 1u;
        GN_Vk.currentFrame = (GN_Vk.currentFrame + 1u) % n;
    } else {
        VkResult res = vkAcquireNextImageKHR(GN_Vk.device, GN_Vk.swapchain, UINT64_MAX, GN_Vk.imageAvailable, 0, &GN_Vk.currentFrame);
        if(res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR) return false;
    }
    vkResetCommandBuffer(GN_Vk.commandBuffers[GN_Vk.currentFrame], 0);
    VkCommandBufferBeginInfo bi = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(GN_Vk.commandBuffers[GN_Vk.currentFrame], &bi);
    VkClearValue cv = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
    VkRenderPassBeginInfo rpi = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    rpi.renderPass = GN_Vk.renderPass;
    rpi.framebuffer = GN_Vk.framebuffers[GN_Vk.currentFrame];
    rpi.renderArea = {{0, 0}, GN_Vk.swapchainExtent};
    rpi.clearValueCount = 1;
    rpi.pClearValues = &cv;
    vkCmdBeginRenderPass(GN_Vk.commandBuffers[GN_Vk.currentFrame], &rpi, VK_SUBPASS_CONTENTS_INLINE);
    s_inRenderPass = true; // 标记2D渲染通道活跃
    vkValidationBeginLabel(GN_Vk.commandBuffers[GN_Vk.currentFrame], "frame-2D");
    vkCmdBindPipeline(GN_Vk.commandBuffers[GN_Vk.currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, GN_Vk.pipeline);
    // 初始化动态 scissor 为全视口
    {
        VkRect2D fs;
        fs.offset.x = 0;
        fs.offset.y = 0;
        fs.extent = GN_Vk.swapchainExtent;
        vkCmdSetScissor(GN_Vk.commandBuffers[GN_Vk.currentFrame], 0, 1, &fs);
    }
    VkUBO ubo;
    float L = 0, R = (float)GN_Vk.swapchainExtent.width, T = (float)GN_Vk.swapchainExtent.height, B = 0;
    float proj[16] = {2.0f / (R - L), 0, 0, 0, 0, 2.0f / (T - B), 0, 0, 0, 0, -1, 0, -(R + L) / (R - L), -(T + B) / (T - B), 0, 1};
    memcpy(ubo.proj, proj, sizeof(proj));
    void *data;
    vkMapMemory(GN_Vk.device, GN_Vk.uniformMemory, 0, sizeof(ubo), 0, &data);
    memcpy(data, &ubo, sizeof(ubo));
    vkUnmapMemory(GN_Vk.device, GN_Vk.uniformMemory);
    GN_Vk.vertexCount = 0;
    gVCnt = 0;
    s_batchCount = 0;
    s_lastDS = VK_NULL_HANDLE; // reset for new frame
    return true;
}

void GN_VkEndFrame() {
    if(!GN_Vk.initialized) return;
    // Close final batch
    if(gVCnt > 0 && s_batchCount < 256) {
        s_batchEnd[s_batchCount] = gVCnt;
        s_batchDS[s_batchCount] = s_lastDS;
        s_batchCount++;
    }
    // Upload all vertices at once
    if(gVCnt > 0) {
        void *d;
        vkMapMemory(GN_Vk.device, GN_Vk.vertexMemory, 0, gVCnt * sizeof(VkVertex), 0, &d);
        memcpy(d, gVBuf, gVCnt * sizeof(VkVertex));
        vkUnmapMemory(GN_Vk.device, GN_Vk.vertexMemory);
    }
    // Draw each batch with its own descriptor set and vertex offset
    uint32_t prevEnd = 0;
    for(uint32_t i = 0; i < s_batchCount; i++) {
        uint32_t count = s_batchEnd[i] - prevEnd;
        if(count == 0) continue;
        vkCmdBindDescriptorSets(GN_Vk.commandBuffers[GN_Vk.currentFrame],
                                VK_PIPELINE_BIND_POINT_GRAPHICS, GN_Vk.pipelineLayout, 0, 1, &s_batchDS[i], 0, 0);
        VkDeviceSize offs[] = {prevEnd * sizeof(VkVertex)};
        vkCmdBindVertexBuffers(GN_Vk.commandBuffers[GN_Vk.currentFrame], 0, 1, &GN_Vk.vertexBuffer, offs);
        vkCmdDraw(GN_Vk.commandBuffers[GN_Vk.currentFrame], count, 1, 0, 0);
        prevEnd = s_batchEnd[i];
    }
    gVCnt = 0;
    s_batchCount = 0;
    // 仅在有活跃渲染通道时结束它
    if(s_inRenderPass) {
        vkCmdEndRenderPass(GN_Vk.commandBuffers[GN_Vk.currentFrame]);
        s_inRenderPass = false;
    }
    vkEndCommandBuffer(GN_Vk.commandBuffers[GN_Vk.currentFrame]);
    if(GN_Vk.headless) {
        // 无窗口：不等待/不信号呈现信号量，也没有 present，提交后由 fence 负责同步
        VkSubmitInfo hsi = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
        hsi.commandBufferCount = 1;
        hsi.pCommandBuffers = &GN_Vk.commandBuffers[GN_Vk.currentFrame];
        vkQueueSubmit(GN_Vk.graphicsQueue, 1, &hsi, GN_Vk.inFlightFence);
        return;
    }
    VkSubmitInfo si = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
    VkPipelineStageFlags ws[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    si.waitSemaphoreCount = 1;
    si.pWaitSemaphores = &GN_Vk.imageAvailable;
    si.pWaitDstStageMask = ws;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &GN_Vk.commandBuffers[GN_Vk.currentFrame];
    si.signalSemaphoreCount = 1;
    si.pSignalSemaphores = &GN_Vk.renderFinished;
    vkQueueSubmit(GN_Vk.graphicsQueue, 1, &si, GN_Vk.inFlightFence);
    VkPresentInfoKHR pi = {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    pi.waitSemaphoreCount = 1;
    pi.pWaitSemaphores = &GN_Vk.renderFinished;
    pi.swapchainCount = 1;
    pi.pSwapchains = &GN_Vk.swapchain;
    pi.pImageIndices = &GN_Vk.currentFrame;
    vkQueuePresentKHR(GN_Vk.graphicsQueue, &pi);
}

void GN_VkBegin2DPass() {
    if(!GN_Vk.initialized || s_inRenderPass) return; // 已有活跃通道时不重复开
    if(s_renderPass2DOverlay == VK_NULL_HANDLE || s_framebuffers2DOverlay == nullptr) return;
    VkCommandBuffer commandBuffer = GN_Vk.commandBuffers[GN_Vk.currentFrame];
    VkRenderPassBeginInfo rpi = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    rpi.renderPass = s_renderPass2DOverlay;
    rpi.framebuffer = s_framebuffers2DOverlay[GN_Vk.currentFrame];
    rpi.renderArea = {{0, 0}, GN_Vk.swapchainExtent};
    rpi.clearValueCount = 0; // loadOp=LOAD：沿用 3D 的结果
    vkCmdBeginRenderPass(commandBuffer, &rpi, VK_SUBPASS_CONTENTS_INLINE);
    s_inRenderPass = true;
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, GN_Vk.pipeline);
    VkRect2D fullScreen = {{0, 0}, GN_Vk.swapchainExtent};
    vkCmdSetScissor(commandBuffer, 0, 1, &fullScreen);
}

// 历史遗留：pass 内 DataFill 的 staging 暂存数组（当前未使用；释放逻辑已并入 vkFrame2d.cpp）
static VkBuffer s_pendingStagingBufs[32];
static VkDeviceMemory s_pendingStagingMems[32];
static int s_pendingStagingCnt = 0;

// ── 帧结束入口：转发到 vkFrame2d.cpp 的 GN_VkEndFrame ──
void RenderFinish(bool only3d) {
    (void)only3d;
    // Vulkan: end the current frame and present
    GNg::GN_VkEndFrame();
}
} // namespace GNg
