/****************************************************************************
 *  GN_mini — Vulkan 渲染目标（离屏 RT）
 *  职责：sIMAGE 渲染目标入口（BeginRender/EndRender/RenderTargetGet）、
 *        RT render pass/framebuffer 创建与切回主 pass、RT 资源销毁。
 *  2026-11-26 自 GN_Vk.cpp move-only 拆出（REFACTOR_PLAN §13 B2）。
 ****************************************************************************/
#include <GN.h>
#include "GN_Vk.h"
#include "vkInternal.h"
#include <cstring>

namespace GNg {

// ── RenderTarget 静态变量 (需在 shutdown 之前) ────────
static VkRenderPass s_rtRenderPass = VK_NULL_HANDLE;
static VkFramebuffer s_rtFramebuffer = VK_NULL_HANDLE;

// ═══════════════════════════════════════════════════════════
//  RenderTarget 离屏渲染 (定义在帧循环变量之后)
// ═══════════════════════════════════════════════════════════
sIMAGE *s_activeRT = nullptr;
bool s_rtInProgress = false;

static VkRenderPass CreateRTRenderPass() {
    VkAttachmentDescription ad = {};
    ad.format = VK_FORMAT_R8G8B8A8_UNORM;
    ad.samples = VK_SAMPLE_COUNT_1_BIT;
    ad.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    ad.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    ad.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    ad.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    VkAttachmentReference ar = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkSubpassDescription sp = {};
    sp.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    sp.colorAttachmentCount = 1;
    sp.pColorAttachments = &ar;
    VkRenderPassCreateInfo rpi = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    rpi.attachmentCount = 1;
    rpi.pAttachments = &ad;
    rpi.subpassCount = 1;
    rpi.pSubpasses = &sp;
    VkRenderPass rp;
    VkResult res = vkCreateRenderPass(GN_Vk.device, &rpi, 0, &rp);
    VkCheck(res, "vkCreateRenderPass in CreateRTRenderPass");
    return rp;
}

void BeginRTRenderPass() {
    if(!s_activeRT || !s_activeRT->vkImage || s_rtInProgress) return;
    FlushVertexBatch();
    if(s_inRenderPass) {
        vkCmdEndRenderPass(GN_Vk.commandBuffers[GN_Vk.currentFrame]);
        s_inRenderPass = false;
    }
    {
        VkImageMemoryBarrier b = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        b.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        b.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.image = s_activeRT->vkImage;
        b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        b.subresourceRange.levelCount = 1;
        b.subresourceRange.layerCount = 1;
        b.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        b.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        vkCmdPipelineBarrier(GN_Vk.commandBuffers[GN_Vk.currentFrame],
                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                             0, 0, 0, 0, 0, 1, &b);
    }
    if(!s_rtRenderPass) s_rtRenderPass = CreateRTRenderPass();
    if(s_rtFramebuffer) vkDestroyFramebuffer(GN_Vk.device, s_rtFramebuffer, 0);
    {
        VkFramebufferCreateInfo fci = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        fci.renderPass = s_rtRenderPass;
        fci.attachmentCount = 1;
        fci.pAttachments = &s_activeRT->vkImageView;
        fci.width = s_activeRT->width;
        fci.height = s_activeRT->height;
        fci.layers = 1;
        VkResult res = vkCreateFramebuffer(GN_Vk.device, &fci, 0, &s_rtFramebuffer);
        VkCheck(res, "vkCreateFramebuffer in BeginRTRenderPass");
    }
    {
        VkClearValue cv = {{{0.0f, 0.0f, 0.0f, 0.0f}}};
        VkRenderPassBeginInfo rpi = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        rpi.renderPass = s_rtRenderPass;
        rpi.framebuffer = s_rtFramebuffer;
        rpi.renderArea = {{0, 0}, {s_activeRT->width, s_activeRT->height}};
        rpi.clearValueCount = 1;
        rpi.pClearValues = &cv;
        vkCmdBeginRenderPass(GN_Vk.commandBuffers[GN_Vk.currentFrame], &rpi, VK_SUBPASS_CONTENTS_INLINE);
    }
    vkCmdBindPipeline(GN_Vk.commandBuffers[GN_Vk.currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, GN_Vk.pipeline);
    {
        VkRect2D sc;
        sc.offset.x = 0;
        sc.offset.y = 0;
        sc.extent.width = s_activeRT->width;
        sc.extent.height = s_activeRT->height;
        vkCmdSetScissor(GN_Vk.commandBuffers[GN_Vk.currentFrame], 0, 1, &sc);
    }
    {
        VkUBO ubo;
        float proj[16] = {2.0f / s_activeRT->width, 0, 0, 0, 0, 2.0f / s_activeRT->height, 0, 0, 0, 0, -1, 0, -1, -1, 0, 1};
        memcpy(ubo.proj, proj, sizeof(proj));
        void *data;
        vkMapMemory(GN_Vk.device, GN_Vk.uniformMemory, 0, sizeof(ubo), 0, &data);
        memcpy(data, &ubo, sizeof(ubo));
        vkUnmapMemory(GN_Vk.device, GN_Vk.uniformMemory);
    }
    GN_Vk.vertexCount = 0;
    s_rtInProgress = true;
}

void EndRTRenderPass() {
    if(!s_rtInProgress) return;
    FlushVertexBatch();
    vkCmdEndRenderPass(GN_Vk.commandBuffers[GN_Vk.currentFrame]);
    {
        VkImageMemoryBarrier b = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        b.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        b.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.image = s_activeRT->vkImage;
        b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        b.subresourceRange.levelCount = 1;
        b.subresourceRange.layerCount = 1;
        b.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(GN_Vk.commandBuffers[GN_Vk.currentFrame],
                             VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                             0, 0, 0, 0, 0, 1, &b);
    }
    {
        VkClearValue cv = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
        VkRenderPassBeginInfo rpi = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        rpi.renderPass = GN_Vk.renderPass;
        rpi.framebuffer = GN_Vk.framebuffers[GN_Vk.currentFrame];
        rpi.renderArea = {{0, 0}, GN_Vk.swapchainExtent};
        rpi.clearValueCount = 1;
        rpi.pClearValues = &cv;
        vkCmdBeginRenderPass(GN_Vk.commandBuffers[GN_Vk.currentFrame], &rpi, VK_SUBPASS_CONTENTS_INLINE);
    }
    vkCmdBindPipeline(GN_Vk.commandBuffers[GN_Vk.currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, GN_Vk.pipeline);
    {
        VkRect2D sc;
        sc.offset.x = 0;
        sc.offset.y = 0;
        sc.extent = GN_Vk.swapchainExtent;
        vkCmdSetScissor(GN_Vk.commandBuffers[GN_Vk.currentFrame], 0, 1, &sc);
    }
    {
        VkUBO ubo;
        float L = 0, R = (float)GN_Vk.swapchainExtent.width, T = (float)GN_Vk.swapchainExtent.height, B = 0;
        float proj[16] = {2.0f / (R - L), 0, 0, 0, 0, 2.0f / (T - B), 0, 0, 0, 0, -1, 0, -(R + L) / (R - L), -(T + B) / (T - B), 0, 1};
        memcpy(ubo.proj, proj, sizeof(proj));
        void *data;
        vkMapMemory(GN_Vk.device, GN_Vk.uniformMemory, 0, sizeof(ubo), 0, &data);
        memcpy(data, &ubo, sizeof(ubo));
        vkUnmapMemory(GN_Vk.device, GN_Vk.uniformMemory);
    }
    GN_Vk.vertexCount = 0;
    s_rtInProgress = false;
    s_inRenderPass = true; // 已切回主渲染通道
    s_activeRT = nullptr;
}

void sIMAGE::BeginRender(int msaa, bool is3d, bool isnew) {
    (void)msaa;
    (void)is3d;
    (void)isnew;
    if(!GN_Vk.initialized) return;
    if(!vkImage || width <= 0 || height <= 0) {
        if(!vkImage) CreateEmptyToData(width > 0 ? width : GN_Vk.viewWidth,
                                       height > 0 ? height : GN_Vk.viewHeight, 1, 0, 1, false, false);
    }
    if(!vkImage) return;
    s_activeRT = this;
    BeginRTRenderPass();
}

void sIMAGE::EndRender() {
    if(!GN_Vk.initialized || !s_rtInProgress) return;
    EndRTRenderPass();
}

void *sIMAGE::RenderTargetGet(sIMAGE *img) {
    if(img) return img;
    return s_activeRT;
}

// RT 资源销毁：GN_VkShutdown 调用（设备销毁之前）。
void vkRenderTargetShutdown() {
    if(s_rtFramebuffer) {
        vkDestroyFramebuffer(GN_Vk.device, s_rtFramebuffer, 0);
        s_rtFramebuffer = VK_NULL_HANDLE;
    }
    if(s_rtRenderPass) {
        vkDestroyRenderPass(GN_Vk.device, s_rtRenderPass, 0);
        s_rtRenderPass = VK_NULL_HANDLE;
    }
}

} // namespace GNg
