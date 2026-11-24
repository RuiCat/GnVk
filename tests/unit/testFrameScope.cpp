// testFrameScope.cpp — 3D pass 作用域守卫的契约。
//
// 为什么值得测：漏调 GN_VkEnd3DPass 的后果不是"少画一点"，而是两个静默黑屏：
//   1) s_in3DPass 永远 true ⇒ 之后所有 Begin3DPass 都被 `if(s_in3DPass) return;` 挡掉；
//   2) GN_VkCompositeToSwapchain 不被调用 ⇒ HDR 从不合成到交换链。
// 而"在绘制循环里加个 early return"是非常自然的写法，一旦发生极难排查。
//
// 这里在无设备的单测环境里验证守卫的**状态语义**：
// 设备没起来时 Begin 会失败，守卫必须如实报告 Valid()==false，
// 且析构不得做出任何破坏性动作（不崩）。
#include "testFramework.h"

#include <GN.h>

#ifdef GN_USING_3D
#include "GN_ShadowCascade.h" // SHADOW_CASCADE_MAX / sSHADOW_CASCADE_RESULT
#endif

// 守卫与查询接口都在 GN_USING_3D 段内。
#ifdef GN_USING_3D

// 设备未初始化时：Valid() 必须为 false（不能谎称已进入 pass）。
TEST_CASE(frameScopeInvalidWithoutDevice) {
    GNg::sMAT44 viewProj(true);
    GNg::sVEC3 eye = {0.0f, 1.0f, 5.0f};

    // 单测进程不初始化 Vulkan，所以 Begin 必然失败。
    CHECK(!GNg::GN_VkIsIn3DPass()); // 前提：确实不在 pass 内

    {
        GNg::sFRAME_3D_SCOPE frame(&viewProj, &eye, nullptr, 0, false);
        CHECK(!frame.Valid()); // 必须如实报告"没进去"
    }
    // 走到这里说明析构没有对未开始的 pass 乱调用（不崩）。
    CHECK(!GNg::GN_VkIsIn3DPass());
}

// 守卫析构后状态必须干净：不能残留 in3DPass。
TEST_CASE(frameScopeLeavesCleanState) {
    GNg::sMAT44 viewProj(true);
    GNg::sVEC3 eye = {0.0f, 0.0f, 1.0f};

    for (int i = 0; i < 3; ++i) { // 反复进出，状态不应累积
        GNg::sFRAME_3D_SCOPE frame(&viewProj, &eye, nullptr, 0, true);
        (void)frame.Valid();
    }
    CHECK(!GNg::GN_VkIsIn3DPass());
}

// 空指针不得崩（调用方可能传 nullptr 表示"用上次的"）。
TEST_CASE(frameScopeNullSafe) {
    GNg::sFRAME_3D_SCOPE frame(nullptr, nullptr, nullptr, 0, true);
    CHECK(!frame.Valid());
}

// drawShadowCascades：count=0 或空指针时必须安全返回（不得崩）。
TEST_CASE(shadowCascadesEmptySafe) {
    GN3d::cMODEL model;
    GNg::sMAT44 vps[GN3d::SHADOW_CASCADE_MAX];

    GNg::GN_VkDrawShadowCascades(model, vps, 0);      // count=0
    GNg::GN_VkDrawShadowCascades(model, nullptr, 4);  // 空指针
    CHECK(true); // 走到这里即未崩溃
}

#endif // GN_USING_3D
