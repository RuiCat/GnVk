// GN_mini Vulkan 头片段：测试钩子（A1）。
// 2026-11-26 自 GN_Vk.h 第 1039-1121 行 move-only 拆出（REFACTOR_PLAN §13 B4）。
// 由 GN_Vk.h 按原顺序包含；不是独立头文件。
// ═══════════════════════════════════════════════════════════
//  测试钩子（A1）
//  目的：让验收程序不再直读 GN_VkContext 的字段。以后重排/分组内部状态时，
//  只需维持这里的稳定面不变，不必同步改 17 个 gpuCheck 文件。
//  约定：
//    * 只暴露“测断言需要的东西”，不暴露整个 context；新增字段需写清用途；
//    * GetTestHooks 在无设备时返回零值快照（device == VK_NULL_HANDLE）。
// ═══════════════════════════════════════════════════════════
namespace GNg {

// 测试需要的管线（名字对应引擎内部固定管线）。
enum eTEST_PIPE {
    eTEST_PIPE_2D = 0,
    eTEST_PIPE_2D_BRIGHT,
    eTEST_PIPE_2D_MULTIPLY,
    eTEST_PIPE_2D_NULL,
    eTEST_PIPE_3D_FORWARD,
    eTEST_PIPE_3D_PBR,
    eTEST_PIPE_SHADOW,
    eTEST_PIPE_COMPOSITE,
    eTEST_PIPE_SKYBOX,
    eTEST_PIPE_GBUFFER,
    eTEST_PIPE_DEFERRED_LIGHT,
    eTEST_PIPE_COUNT
};

struct sTEST_HOOKS {
    // ── 设备与队列 ──
    VkDevice device;
    VkPhysicalDevice physicalDevice;
    VkQueue graphicsQueue;
    uint32_t graphicsFamily;
    bool initialized;

    // ── 帧内录制 ──
    VkCommandPool commandPool;
    VkDescriptorPool descriptorPool;
    VkCommandBuffer commandBuffer; // = commandBuffers[currentFrame]
    uint32_t currentFrame;

    // ── 交换链 ──
    VkExtent2D extent;
    VkFormat format;
    VkImage currentSwapImage; // = swapchainImages[currentFrame]

    // ── 内部图像（像素/深度回读用）──
    VkImage shadowImage;
    uint32_t shadowMapSize;
    // B2：点光/聚光 cube shadow（诊断/验收用）
    VkImage cubeShadowImage;
    VkImageView cubeShadowView;
    uint32_t cubeShadowSize;
    VkImage hdrImage;
    VkImage gAlbedoMetalImage;
    VkImage gNormalRoughImage;
    VkDescriptorSet gBufferDescriptorSet;
    VkPipelineLayout gBufferPipelineLayout;

    // ── 3D 动态顶点/索引缓冲 ──
    // 仅供 "绕开引擎高层 draw、直接录 G-Buffer 命令" 的用例（gpuCheck3D 的
    // DrawGBufferQuadRaw）；正常绘制路径不该用。
    VkBuffer vertexBuffer3D;
    VkDeviceMemory vertexMemory3D;
    VkBuffer indexBuffer3D;
    VkDeviceMemory indexMemory3D;

    // ── 管线缓存统计 ──
    unsigned pipelineCount;
    unsigned long long pipelineMiss;
    unsigned long long pipelineHit;
};

// 取当前钩子快照（每次调用重新填充；无设备时全零）。
void GN_VkGetTestHooks(sTEST_HOOKS *out);
// 按 id 查管线（每次实时查，resize 安全）；未创建返回 VK_NULL_HANDLE。
VkPipeline GN_VkTestPipeline(int pipeId);

} // namespace GNg

// 一帧的编排（阴影 → 3D pass → 2D 叠层起点）。定义在 vkRenderFrame.cpp，
// 依赖上面的 sFRAME_3D_SCOPE / GN_VkDrawShadowCascades，所以放在文件末尾包含。
#include "vkRenderFrame.h"

