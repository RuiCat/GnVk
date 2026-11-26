// testRenderState.cpp — B7-2：渲染状态余量收敛进 GN_Vk（s3DPassRes/sRtRes/sSwapchainRes/s2DRes）。
// 验证迁移后的默认值与纯状态 API 往返；不依赖 Vulkan 设备（与 testEngineContext 同口径）。
#include "testFramework.h"

#include <GN.h>
#include "GN_Vk.h"
#include "vkSkinnedDispatch.h" // GN_VkLastSkinnedPath 声明（独立头，见该头注释）

using GNg::GN_Vk;

TEST_CASE(renderState3DPassDefaults) {
    // 3D pass 状态已从 vk3dInit/vk3dPasses/vk3dShutdown/vkSkinnedDispatch 收敛进 GN_Vk（B7-2）。
    // 单测进程无 Vulkan 设备，各 Begin*Pass 均被 initialized 守卫挡回，标志应保持初值。
    CHECK(!GN_Vk.in3DPass);
    CHECK(!GN_Vk.inShadowPass);
    CHECK(!GN_Vk.inCubeShadowPass);
    CHECK(GN_Vk.cubeShadowLightIndex == 0);
    CHECK(GN_Vk.shadowPassPipeline == VK_NULL_HANDLE);
    CHECK(GN_Vk.framebuffers3D == nullptr);
    CHECK(GN_Vk.vtxOfs3D == 0);
    CHECK(GN_Vk.idxOfs3D == 0);
    CHECK(GN_Vk.vtxCap3D == 0);
    CHECK(GN_Vk.idxCap3D == 0);
    CHECK(!GN_Vk.hasDeferredViewProj);
    CHECK(GN_Vk.aRetired3DBuffers.empty()); // 退休表：无设备则无扩容退休
    CHECK(GN_Vk.cubeShadowLightPos.x == 0.0f);
    CHECK(GN_Vk.cubeShadowLightPos.y == 0.0f);
    CHECK(GN_Vk.cubeShadowLightPos.z == 0.0f);
    // 诊断 API 与上下文成员一致（收敛不变量）
    CHECK(GNg::GN_VkLastSkinnedPath() == GN_Vk.lastSkinnedPath);
}

TEST_CASE(renderStateRtOverlayAndColorKey) {
    // RT 与 2D 叠层状态收敛进 GN_Vk.sRtRes / sSwapchainRes（B7-2）；无设备时保持初值。
    CHECK(GN_Vk.rtRenderPass == VK_NULL_HANDLE);
    CHECK(GN_Vk.rtFramebuffer == VK_NULL_HANDLE);
    CHECK(GN_Vk.activeRT == nullptr);
    CHECK(!GN_Vk.rtInProgress);
    CHECK(GN_Vk.renderPass2DOverlay == VK_NULL_HANDLE);
    CHECK(GN_Vk.framebuffers2DOverlay == nullptr);
    // alphaColorKey 收敛进 s2DRes：SetAlphaColorKey 纯状态往返
    GNg::SetAlphaColorKey(0x123456u);
    CHECK(GN_Vk.alphaColorKey == 0x123456u);
    GNg::SetAlphaColorKey(0u);
    CHECK(GN_Vk.alphaColorKey == 0u);
}

TEST_CASE(renderStateSkyParamsRoundTrip) {
    // 天空参数收敛为顶层命名成员 aSkyParams（NSDMI 初值与原 static 一致）：Set/Get 往返。
    CHECK(GN_Vk.aSkyParams.sunIntensity == 1.0f); // 默认值未被迁移改变
    GNg::VkSkyParams p;
    p.sunIntensity = 2.5f;
    p.cloudCoverage = 0.75f;
    p.sunDirection[0] = 0.1f;
    GNg::GN_VkSetSky(p);
    CHECK(GN_Vk.aSkyParams.sunIntensity == 2.5f);
    GNg::VkSkyParams out;
    GNg::GN_VkGetSky(&out);
    CHECK(std::memcmp(&out, &p, sizeof(GNg::VkSkyParams)) == 0);
    GNg::GN_VkSetSky(GNg::VkSkyParams{}); // 恢复默认，避免跨用例状态泄漏
    CHECK(GN_Vk.aSkyParams.sunIntensity == 1.0f);
}
