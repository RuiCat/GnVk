/****************************************************************************
 *  GN_mini — Vulkan 交换链与窗口 resize
 *  职责：交换链格式选择（sRGB 优先）、2D 叠层 render pass 的 framebuffer 创建/销毁、
 *        resize 时 swapchain/视图/framebuffer/2D 管线/3D 目标的完整重建编排。
 *  2026-11-26 自 GN_Vk.cpp move-only 拆出（REFACTOR_PLAN §13 B2）。
 ****************************************************************************/
#include <GN.h>
#include "GN_Vk.h"
#include "vkInternal.h"
#include <cstdio>
#include <cstddef>

namespace GNg {

VkRenderPass s_renderPass2DOverlay = VK_NULL_HANDLE;
VkFramebuffer *s_framebuffers2DOverlay = nullptr;


// 交换链格式选择：优先 8-bit sRGB（硬件负责 linear→sRGB），否则退回驱动给出的
// 第一个格式；非 sRGB 格式由 composite 的 srgbEncode 分支显式编码。
bool isSrgbSwapchainFormat(VkFormat format) {
    return format == VK_FORMAT_B8G8R8A8_SRGB || format == VK_FORMAT_R8G8B8A8_SRGB;
}

VkFormat chooseSwapchainFormat(const VkSurfaceFormatKHR *formats, uint32_t count) {
    if(!formats || count == 0) return VK_FORMAT_B8G8R8A8_SRGB;
    const VkFormat preferred[] = {VK_FORMAT_B8G8R8A8_SRGB, VK_FORMAT_R8G8B8A8_SRGB};
    for(uint32_t p = 0; p < 2; ++p) {
        for(uint32_t i = 0; i < count; ++i) {
            if(formats[i].format == preferred[p]) return preferred[p];
        }
    }
    for(uint32_t i = 0; i < count; ++i) {
        if(formats[i].format != VK_FORMAT_UNDEFINED) return formats[i].format;
    }
    return VK_FORMAT_B8G8R8A8_SRGB;
}

bool GN_VkSwapchainIsSrgb() {
    return isSrgbSwapchainFormat(GN_Vk.swapchainFormat);
}

void destroyOverlayFramebuffers() {
    if(!s_framebuffers2DOverlay) return;
    for(uint32_t i = 0; i < GN_Vk.swapchainImageCount; i++) {
        if(s_framebuffers2DOverlay[i]) {
            vkDestroyFramebuffer(GN_Vk.device, s_framebuffers2DOverlay[i], 0);
        }
    }
    delete[] s_framebuffers2DOverlay;
    s_framebuffers2DOverlay = nullptr;
}

void createOverlayFramebuffers() {
    if(s_renderPass2DOverlay == VK_NULL_HANDLE) return;
    s_framebuffers2DOverlay = new VkFramebuffer[GN_Vk.swapchainImageCount];
    for(uint32_t i = 0; i < GN_Vk.swapchainImageCount; i++) {
        s_framebuffers2DOverlay[i] = VK_NULL_HANDLE;
        VkFramebufferCreateInfo fci = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        fci.renderPass = s_renderPass2DOverlay;
        fci.attachmentCount = 1;
        fci.pAttachments = &GN_Vk.swapchainViews[i];
        fci.width = GN_Vk.swapchainExtent.width;
        fci.height = GN_Vk.swapchainExtent.height;
        fci.layers = 1;
        vkCreateFramebuffer(GN_Vk.device, &fci, 0, &s_framebuffers2DOverlay[i]);
    }
}

// task-26：2D 叠层管线也是固定视口（仅在 scissor 上动态），resize 后按新 extent 重建。
static bool recreate2DPipelines() {
    if(GN_Vk.pipelineLayout == VK_NULL_HANDLE || GN_Vk.renderPass == VK_NULL_HANDLE) return false;
    auto vertSpv = LoadSPIRVFile("assets/shaders/gn2d.vert.spv");
    auto fragSpv = LoadSPIRVFile("assets/shaders/gn2d.frag.spv");
    if(vertSpv.empty() || fragSpv.empty()) return false;
    GN3d::sVERTEX_LAYOUT layout2D;
    layout2D.Add(0, (uint32_t)offsetof(VkVertex, x), VK_FORMAT_R32G32_SFLOAT);
    layout2D.Add(1, (uint32_t)offsetof(VkVertex, u), VK_FORMAT_R32G32_SFLOAT);
    layout2D.Add(2, (uint32_t)offsetof(VkVertex, r), VK_FORMAT_R32G32B32A32_SFLOAT);
    layout2D.aStride = (uint32_t)sizeof(VkVertex);
    GN3d::sPIPELINE_DESC desc2D;
    desc2D.aKind = GN3d::ePIPE_GRAPHICS;
    desc2D.aVertSpv = vertSpv.data();
    desc2D.aVertSize = vertSpv.size() * sizeof(uint32_t);
    desc2D.aFragSpv = fragSpv.data();
    desc2D.aFragSize = fragSpv.size() * sizeof(uint32_t);
    desc2D.aVertexLayout = layout2D;
    desc2D.aRenderPass = GN_Vk.renderPass;
    desc2D.aSubpass = 0;
    desc2D.aLayout = GN_Vk.pipelineLayout;
    desc2D.aDepthTest = false;
    desc2D.aDepthWrite = false;
    desc2D.aDepthStencil = false;
    desc2D.aCullBack = true;
    desc2D.aFrontFace = VK_FRONT_FACE_CLOCKWISE;
    desc2D.aBlend = GN3d::eBLEND_NONE;
    desc2D.aTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    desc2D.aSamples = VK_SAMPLE_COUNT_1_BIT;
    desc2D.aColorAttachmentCount = 1;
    desc2D.aDynamicViewport = false;
    desc2D.aViewport = VkViewport{0.0f, 0.0f, (float)GN_Vk.swapchainExtent.width,
                                  (float)GN_Vk.swapchainExtent.height, 0.0f, 1.0f};
    desc2D.aDynamicScissor = true;
    desc2D.aScissor = VkRect2D{{0, 0}, GN_Vk.swapchainExtent};
    desc2D.aCustomBlend = true;
    desc2D.aBlendSrcColor = VK_BLEND_FACTOR_SRC_ALPHA;
    desc2D.aBlendDstColor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    desc2D.aBlendSrcAlpha = VK_BLEND_FACTOR_ONE;
    desc2D.aBlendDstAlpha = VK_BLEND_FACTOR_ZERO;
    VkPipeline pipeline = GN_Vk.aPipelineCache.GetOrCreate(desc2D);
    desc2D.aBlendDstColor = VK_BLEND_FACTOR_ONE;
    VkPipeline pipelineBright = GN_Vk.aPipelineCache.GetOrCreate(desc2D);
    desc2D.aBlendSrcColor = VK_BLEND_FACTOR_DST_COLOR;
    desc2D.aBlendDstColor = VK_BLEND_FACTOR_ZERO;
    VkPipeline pipelineMultiply = GN_Vk.aPipelineCache.GetOrCreate(desc2D);
    desc2D.aCustomBlend = false;
    desc2D.aBlend = GN3d::eBLEND_NONE;
    VkPipeline pipelineNull = GN_Vk.aPipelineCache.GetOrCreate(desc2D);
    if(!pipeline || !pipelineBright || !pipelineMultiply || !pipelineNull) {
        fprintf(stderr, "[GN_Vk] 警告: resize 后 2D 管线重建失败: %s\n",
                GN_Vk.aPipelineCache.GetLastError());
        return false;
    }
    GN_Vk.pipeline = pipeline;
    GN_Vk.pipelineBright = pipelineBright;
    GN_Vk.pipelineMultiply = pipelineMultiply;
    GN_Vk.pipelineNull = pipelineNull;
    return true;
}

// ── 窗口尺寸变化：重建 swapchain、视图与 framebuffer（3D 目标需随窗口重建） ──
void GN_VkResize(int w, int h) {
    if(w <= 0 || h <= 0) return;
    GN_Vk.viewWidth = w;
    GN_Vk.viewHeight = h;
    if(!GN_Vk.initialized) return;
    // 重建 swapchain 和相关资源以适应新尺寸
    vkDeviceWaitIdle(GN_Vk.device);
    // 引用旧 swapchainView / 旧尺寸 view 的 framebuffer 必须在销毁 view 之前释放；新 view 建好后重建。
    destroyOverlayFramebuffers();
#ifdef GN_USING_3D
    destroy3DCompositeFramebuffers();
    destroy3DDeferredLightFramebuffers();
    destroy3DForwardFramebuffers(); // s_framebuffers3D 引用旧 swapchainView/旧 msaa/depth
    destroy3DHdrTargets();          // HDR fb + image/view/sampler 都是旧尺寸
#endif
    // 销毁旧 swapchain 资源
    for(uint32_t i = 0; i < GN_Vk.swapchainImageCount; i++) vkDestroyFramebuffer(GN_Vk.device, GN_Vk.framebuffers[i], 0);
    delete[] GN_Vk.framebuffers;
    for(uint32_t i = 0; i < GN_Vk.swapchainImageCount; i++) vkDestroyImageView(GN_Vk.device, GN_Vk.swapchainViews[i], 0);
    delete[] GN_Vk.swapchainViews;
    if(GN_Vk.headless) {
        destroyOffscreenSwapchain();
    } else {
        delete[] GN_Vk.swapchainImages;
        vkDestroySwapchainKHR(GN_Vk.device, GN_Vk.swapchain, 0);
    }
    if(GN_Vk.headless) {
        // 无窗口：直接按新尺寸重建离屏图（后续 framebuffer / 3D 目标重建路径窗窗口模式共用）
        if(!createOffscreenSwapchain((uint32_t)w, (uint32_t)h, kHeadlessImageCount)) {
            fprintf(stderr, "[GN_Vk] resize: 离屏目标重建失败\n");
            return;
        }
    } else {
    // 重建 swapchain
    VkSurfaceCapabilitiesKHR caps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(GN_Vk.physicalDevice, GN_Vk.surface, &caps);
    GN_Vk.swapchainExtent = caps.currentExtent;
    if(GN_Vk.swapchainExtent.width == 0xffffffff || GN_Vk.swapchainExtent.width == 0) {
        GN_Vk.swapchainExtent.width = (uint32_t)w;
        GN_Vk.swapchainExtent.height = (uint32_t)h;
    }
    uint32_t fmtCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(GN_Vk.physicalDevice, GN_Vk.surface, &fmtCount, 0);
    VkSurfaceFormatKHR *fmts = new VkSurfaceFormatKHR[fmtCount];
    vkGetPhysicalDeviceSurfaceFormatsKHR(GN_Vk.physicalDevice, GN_Vk.surface, &fmtCount, fmts);
    GN_Vk.swapchainFormat = chooseSwapchainFormat(fmts, fmtCount);
    delete[] fmts;
    fprintf(stderr, "[GN_Vk] swapchain format=%d (%s)\n", (int)GN_Vk.swapchainFormat,
            isSrgbSwapchainFormat(GN_Vk.swapchainFormat) ? "sRGB" : "non-sRGB");
    VkSwapchainCreateInfoKHR sci = {VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    sci.surface = GN_Vk.surface;
    // 校验层报过 minImageCount(2) < surface caps 的最小值（本机 4）；按 caps 取，避免交换链违规。
    sci.minImageCount = caps.minImageCount > 2u ? caps.minImageCount : 2u;
    sci.imageFormat = GN_Vk.swapchainFormat;
    sci.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    sci.imageExtent = GN_Vk.swapchainExtent;
    sci.imageArrayLayers = 1;
    sci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    sci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    sci.preTransform = caps.currentTransform;
    sci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    sci.presentMode = ChoosePresentMode(GN_Vk.physicalDevice, GN_Vk.surface);
    sci.clipped = VK_TRUE;
    fprintf(stderr, "[GN_Vk] present mode=%d (resize)\n", (int)sci.presentMode);
    VkCheck(vkCreateSwapchainKHR(GN_Vk.device, &sci, 0, &GN_Vk.swapchain), "vkCreateSwapchainKHR(resize)");
    vkGetSwapchainImagesKHR(GN_Vk.device, GN_Vk.swapchain, &GN_Vk.swapchainImageCount, 0);
    GN_Vk.swapchainImages = new VkImage[GN_Vk.swapchainImageCount];
    GN_Vk.swapchainViews = new VkImageView[GN_Vk.swapchainImageCount];
    vkGetSwapchainImagesKHR(GN_Vk.device, GN_Vk.swapchain, &GN_Vk.swapchainImageCount, GN_Vk.swapchainImages);
    for(uint32_t i = 0; i < GN_Vk.swapchainImageCount; i++) GN_Vk.swapchainViews[i] = MakeImageView(GN_Vk.swapchainImages[i], GN_Vk.swapchainFormat);
    } // end of: headless / swapchain 重建
    // 重建 framebuffers
    GN_Vk.framebuffers = new VkFramebuffer[GN_Vk.swapchainImageCount];
    for(uint32_t i = 0; i < GN_Vk.swapchainImageCount; i++) {
        VkFramebufferCreateInfo fci = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        fci.renderPass = GN_Vk.renderPass;
        fci.attachmentCount = 1;
        fci.pAttachments = &GN_Vk.swapchainViews[i];
        fci.width = GN_Vk.swapchainExtent.width;
        fci.height = GN_Vk.swapchainExtent.height;
        fci.layers = 1;
        vkCreateFramebuffer(GN_Vk.device, &fci, 0, &GN_Vk.framebuffers[i]);
    }
    createOverlayFramebuffers();
    recreate2DPipelines(); // task-26：2D 叠层固定视口也随新尺寸重建（失败只告警）
#ifdef GN_USING_3D
    // ── 3D 资源按新尺寸完整重建（顺序敏感：fb 引用 view，管线固定视口）──
    // L1：先重建 HDR 目标（renderPass3D 的 resolve 附件 + 延迟光照颜色附件都引用它），
    // 再重建 MSAA/depth 与引用新 hdrView 的 3D forward framebuffer。
    recreate3DHdrTargets();
    recreate3DTargets();
    // G-Buffer 不引用 swapchainView，但图像/视图/内存/framebuffer 与固定视口管线都依赖尺寸。
    recreate3DGbufferTargets();
    // G2b：SSAO 目标/管线随尺寸重建，并重写引用 G-Buffer normal/depth 的描述符。
    recreate3DSsaoTargets();
    // G4：SSGI 半分辨率目标/管线随尺寸重建，并重写引用 G-Buffer 的描述符与延迟 binding13。
    recreate3DSsgiTargets();
    // 延迟光照：新 swapchainView 的 framebuffer + 指向新 G-Buffer 视图的 binding 1..4 + 新尺寸管线。
    create3DDeferredLightTargets();
    // G0b：调试视图管线固定视口，按新 extent 重建（旧管线留在缓存）。
    recreate3DGbufferDebugPipeline();
    // L1d：Bloom mip 图/fb/管线按新尺寸重建，并重写 composite binding3（新 level0 视图）。
    if(!recreate3DBloomTargets()) {
        fprintf(stderr, "[GN_Vk] 警告: resize 后 Bloom 目标重建失败（L1d）\n");
    }
    // 合成：framebuffer（新 swapchainView）+ 描述符 binding1（新 hdrView）+ 固定视口管线。
    recreate3DCompositeTargets();
    // 固定视口管线：前向/PBR/天空盒（从 assets 重新加载 SPIR-V；缺失只告警，旧管线仍可用）。
    auto vertSpv = LoadSPIRVFile("assets/shaders/gn3d.vert.spv");
    auto fragSpv = LoadSPIRVFile("assets/shaders/gn3d.frag.spv");
    if(vertSpv.empty() || fragSpv.empty()) {
        fprintf(stderr, "[GN_Vk] 警告: resize 时前向着色器缺失，继续沿用旧管线\n");
    } else {
        create3DForwardPipeline(vertSpv, fragSpv);
    }
    auto pbrVert = LoadSPIRVFile("assets/shaders/gn3d_pbr.vert.spv");
    auto pbrFrag = LoadSPIRVFile("assets/shaders/gn3d_pbr.frag.spv");
    if(pbrVert.empty() || pbrFrag.empty()) {
        fprintf(stderr, "[GN_Vk] 警告: resize 时 PBR 着色器缺失，继续沿用旧管线\n");
    } else {
        create3DPbrPipeline(pbrVert, pbrFrag);
    }
    create3DSkyboxPipeline();
    // task-26：其余固定视口管线（曾遗漏，导致 resize 后包围盒/坐标轴偏移）按新 extent 重建。
    if(!recreateDebugLinesPipeline()) {
        fprintf(stderr, "[GN_Vk] 警告: resize 后调试线框管线未重建（沿用旧管线）\n");
    }
    if(!recreateMaterialPipeline()) {
        fprintf(stderr, "[GN_Vk] 警告: resize 后材质管线未重建（沿用旧管线）\n");
    }
    if(!recreateSkinnedPipeline()) {
        fprintf(stderr, "[GN_Vk] 警告: resize 后蒙皮管线未重建（沿用旧管线）\n");
    }
    if(!recreateParticleDrawPipeline()) {
        fprintf(stderr, "[GN_Vk] 警告: resize 后粒子管线未重建（沿用旧管线）\n");
    }
    if(!recreateClothDrawPipeline()) {
        fprintf(stderr, "[GN_Vk] 警告: resize 后布料管线未重建（沿用旧管线）\n");
    }
    if(!recreateDecalPipeline()) {
        fprintf(stderr, "[GN_Vk] 警告: resize 后贴花管线未重建（沿用旧管线）\n");
    }
    if(!recreatePrecipitationPipeline()) {
        fprintf(stderr, "[GN_Vk] 警告: resize 后降水管线未重建（沿用旧管线）\n");
    }
    if(!recreateSkinnedSsboPipeline()) {
        fprintf(stderr, "[GN_Vk] 警告: resize 后 SSBO 蒙皮管线未重建（沿用旧管线）\n");
    }
#endif
}

} // namespace GNg
