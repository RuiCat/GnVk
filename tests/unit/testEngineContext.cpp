// testEngineContext.cpp — B7-1：sEngineContext 默认门面 + 2D 帧状态收敛进 GN_Vk.s2DRes。
#include "testFramework.h"

#include <GN.h>
#include "GN_EngineContext.h"
#include "GN_Vk.h"
#include "GN_CoreContext.h" // B7-3：core 上下文默认值
#include "GN_FileContext.h" // B7-3：file 上下文默认值

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

TEST_CASE(engineContextCoreAndFilePointers) {
    // B7-3：core/file 子系统上下文接入默认门面
    sEngineContext &ctx = GN_GetEngineContext();
    CHECK(ctx.core == &GNg::GN_Core);
    CHECK(ctx.file == &GNg::GN_FileState);
}

TEST_CASE(coreFileContextDefaults) {
    // B7-3：路径目录/格式缓存/日志模式的初值与原文件内 static 一致
    CHECK(std::strcmp(GN_SysDir(0), "assets/") == 0);
    CHECK(GN_AppDir(0)[0] == 0);
    CHECK(GNg::GN_Core.aImgRowEndID == 0);
    CHECK(GNg::GN_Core.aAudioExtNames[0] == 0);
    CHECK(GNg::GN_Core.aImageExtNames[0] == 0);
    CHECK(GNg::GN_FileState.aRunLogOpenMode[0] == 'w');
}
