/****************************************************************************
 *  GN_mini — Vulkan 无窗口离屏渲染目标
 *  职责：替代 swapchain 的离屏图/视图/显存创建与销毁（headless 路径）。
 *  除「图的来源」外与交换链镜像完全等价，绘制路径无 if(headless) 渗透。
 *  2026-11-26 自 GN_Vk.cpp move-only 拆出（REFACTOR_PLAN §13 B2）。
 ****************************************************************************/
#include <GN.h>
#include "GN_Vk.h"
#include "vkInternal.h"
#include <cstdio>

namespace GNg {

// ═══════════════════════════════════════════════════════════
//  无窗口（离屏）渲染目标：替代 swapchain
//  除「图的来源」以外，离屏图与交换链镜像在引擎里完全等价（同格式/尺寸/张数，同样挂在
//  swapchainImages/Views/framebuffers 上），因此 render pass、2D/3D 管线、每帧命令缓冲与
//  读回全部沿用窗口模式的同一套代码——没有任何 if(headless) 渗透到绘制路径里。
// ═══════════════════════════════════════════════════════════
const uint32_t kHeadlessImageCount = 3;

// 优先带 sRGB 的 8bit 格式（与窗口模式同一色彩路径），必须同时支持颜色附件与回读。
static VkFormat chooseOffscreenFormat() {
    const VkFormat candidates[] = {VK_FORMAT_B8G8R8A8_SRGB, VK_FORMAT_R8G8B8A8_SRGB,
                                   VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM};
    const VkFormatFeatureFlags need =
        VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | VK_FORMAT_FEATURE_TRANSFER_SRC_BIT;
    for(size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); ++i) {
        VkFormatProperties fp;
        vkGetPhysicalDeviceFormatProperties(GN_Vk.physicalDevice, candidates[i], &fp);
        if((fp.optimalTilingFeatures & need) == need) return candidates[i];
    }
    return VK_FORMAT_UNDEFINED;
}

bool createOffscreenSwapchain(uint32_t w, uint32_t h, uint32_t count) {
    const VkFormat fmt = chooseOffscreenFormat();
    if(fmt == VK_FORMAT_UNDEFINED) {
        GN_LogErr("[GN_Vk] FATAL: 找不到可作渲染目标且可回读的 8bit 格式\n");
        return false;
    }
    if(count == 0) count = 1;
    GN_Vk.swapchainFormat = fmt;
    GN_Vk.swapchainExtent = {w, h};
    GN_Vk.swapchainImageCount = count;
    GN_Vk.swapchainImages = new VkImage[count];
    GN_Vk.swapchainViews = new VkImageView[count];
    GN_Vk.swapchainMemories = new VkDeviceMemory[count];
    for(uint32_t i = 0; i < count; ++i) {
        GN_Vk.swapchainImages[i] = VK_NULL_HANDLE;
        GN_Vk.swapchainViews[i] = VK_NULL_HANDLE;
        GN_Vk.swapchainMemories[i] = VK_NULL_HANDLE;
    }
    for(uint32_t i = 0; i < count; ++i) {
        VkImageCreateInfo ci = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        ci.imageType = VK_IMAGE_TYPE_2D;
        ci.format = fmt;
        ci.extent = {w, h, 1};
        ci.mipLevels = 1;
        ci.arrayLayers = 1;
        ci.samples = VK_SAMPLE_COUNT_1_BIT;
        ci.tiling = VK_IMAGE_TILING_OPTIMAL;
        // 颜色附件（渲染目标）+ 回读源；不参与采样，避免与 sIMAGE 的采样路径混淆。
        ci.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        VkResult res = vkCreateImage(GN_Vk.device, &ci, 0, &GN_Vk.swapchainImages[i]);
        if(res != VK_SUCCESS) {
            fprintf(stderr, "[GN_Vk] FATAL: 离屏图像创建失败 err=%d\n", (int)res);
            return false;
        }
        VkMemoryRequirements mr;
        vkGetImageMemoryRequirements(GN_Vk.device, GN_Vk.swapchainImages[i], &mr);
        VkMemoryAllocateInfo ai = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        ai.allocationSize = mr.size;
        ai.memoryTypeIndex = FindMemType(mr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        res = vkAllocateMemory(GN_Vk.device, &ai, 0, &GN_Vk.swapchainMemories[i]);
        if(res != VK_SUCCESS) {
            fprintf(stderr, "[GN_Vk] FATAL: 离屏图像显存分配失败 err=%d\n", (int)res);
            return false;
        }
        vkBindImageMemory(GN_Vk.device, GN_Vk.swapchainImages[i], GN_Vk.swapchainMemories[i], 0);
        GN_Vk.swapchainViews[i] = MakeImageView(GN_Vk.swapchainImages[i], fmt);
        if(GN_Vk.swapchainViews[i] == VK_NULL_HANDLE) {
            fprintf(stderr, "[GN_Vk] FATAL: 离屏图像视图创建失败\n");
            return false;
        }
    }
    fprintf(stderr, "[GN_Vk] 无窗口离屏目标: %ux%u × %u 张 (format=%d)\n", w, h, count,
            (int)fmt);
    return true;
}

// 只释放离屏图与显存；view 由调用方按统一顺序销毁（与 swapchain 路径一致）。
void destroyOffscreenSwapchain() {
    if(GN_Vk.swapchainImages) {
        for(uint32_t i = 0; i < GN_Vk.swapchainImageCount; ++i) {
            if(GN_Vk.swapchainImages[i] != VK_NULL_HANDLE) {
                vkDestroyImage(GN_Vk.device, GN_Vk.swapchainImages[i], 0);
            }
        }
        delete[] GN_Vk.swapchainImages;
        GN_Vk.swapchainImages = 0;
    }
    if(GN_Vk.swapchainMemories) {
        for(uint32_t i = 0; i < GN_Vk.swapchainImageCount; ++i) {
            if(GN_Vk.swapchainMemories[i] != VK_NULL_HANDLE) {
                vkFreeMemory(GN_Vk.device, GN_Vk.swapchainMemories[i], 0);
            }
        }
        delete[] GN_Vk.swapchainMemories;
        GN_Vk.swapchainMemories = 0;
    }
}

} // namespace GNg
