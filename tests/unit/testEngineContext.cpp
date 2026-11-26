// testEngineContext.cpp — B7-1：sEngineContext 默认门面 + 2D 帧状态收敛进 GN_Vk.s2DRes。
#include "testFramework.h"

#include <GN.h>
#include "GN_EngineContext.h"
#include "GN_Vk.h"

using GNg::GN_Vk;

TEST_CASE(engineContextDefaultFacade) {
    sEngineContext &ctx = GN_GetEngineContext();
    CHECK(ctx.vk == &GN_Vk); // 默认门面指向全局渲染上下文
    CHECK(&GN_GetEngineContext() == &ctx); // 进程内同一实例
}

TEST_CASE(engineContextScissorState) {
    // scissor 状态已从 vkFrame2d.cpp 文件级全局收敛进 GN_Vk.s2DRes（B7-1）
    GNg::GN_VkSetScissor(3, 5, 100, 80, true);
    CHECK(GN_Vk.scissorEnabled);
    CHECK(GN_Vk.scissorDirty);
    CHECK(GN_Vk.scissorRect.offset.x == 3);
    CHECK(GN_Vk.scissorRect.offset.y == 5);
    CHECK(GN_Vk.scissorRect.extent.width == 100);
    CHECK(GN_Vk.scissorRect.extent.height == 80);
    GNg::GN_VkSetScissor(0, 0, 0, 0, false);
    CHECK(!GN_Vk.scissorEnabled);
    CHECK(GN_Vk.scissorRect.offset.x == 0);
    CHECK(GN_Vk.scissorRect.offset.y == 0);
}