// testEngineContext.cpp — B7-1：sEngineContext 默认门面 + 2D 帧状态收敛进 GN_Vk.s2DRes。
#include "testFramework.h"

#include <GN.h>
#include "GN_EngineContext.h"
#include "GN_ParallelContext.h" // B7-6
#include "GN_NetTransContext.h" // B7-6
#include "_Sys/GN_UserThreadContext.h" // B7-6
#include "_Sys/GN_ClipboardContext.h" // B7-6
#include "GN_ScissorContext.h" // B7-7
#include "GN_ImageLoadContext.h" // B7-7
#include "GN_Vk.h"
#include "GN_CoreContext.h" // B7-3：core 上下文默认值
#include "GN_FileContext.h" // B7-3：file 上下文默认值
#include "GN_InputContext.h" // B7-4：input 上下文默认值
#include "GN_UiBrowserContext.h" // B7-4：ui 上下文默认值
#include "_Main/GN_MainContext.h" // B7-4：platform 上下文默认值（头在 platform/_Main）
#include "_Sound/GN_SoundContext.h" // B7-4：audio 上下文默认值（头在 audio/_Sound）

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

TEST_CASE(engineContextSubsystemPointers) {
    // B7-4：input/ui/platform/audio 子系统上下文接入默认门面
    sEngineContext &ctx = GN_GetEngineContext();
    CHECK(ctx.input == &GNg::GN_InputState);
    CHECK(ctx.ui == &GNg::GN_UiBrowserState);
    CHECK(ctx.platform == &GNg::GN_MainState);
    CHECK(ctx.audio == &GNg::GN_SoundState);
}

TEST_CASE(subsystemContextDefaults) {
    // B7-4：迁移后初值与原文件内 static 一致（只读断言，避免污染其它用例）
    CHECK(GNg::GN_InputState.aInputNotIsMapping);
    CHECK(GNg::GN_MainState.aPendingResizeW == 0);
    CHECK(GNg::GN_MainState.aFirstCursor);
    CHECK(GNg::GN_MainState.aImeBufLen == 0);
    CHECK(GNg::GN_UiBrowserState.aIconBackW == -1);
    CHECK(GNg::GN_UiBrowserState.aLastFileBrowser == nullptr);
    CHECK(GNg::GN_SoundState.aAlDevice == nullptr);
}

TEST_CASE(engineContextMultiInstanceContract) {
    // 多实例契约：栈上第二个门面实例与单例指向同一批默认子系统上下文。
    // 各子系统真正独立实例化尚未开放（c4 逐步开放）；本用例锁定当前默认语义。
    sEngineContext second;
    sEngineContext &def = GN_GetEngineContext();
    CHECK(&second != &def);
    CHECK(second.vk == def.vk && second.core == def.core && second.file == def.file);
    CHECK(second.input == def.input && second.ui == def.ui);
    CHECK(second.platform == def.platform && second.audio == def.audio);
    CHECK(second.parallel == def.parallel && second.nettrans == def.nettrans);
    CHECK(second.userthread == def.userthread && second.clipboard == def.clipboard);
    CHECK(second.core == &GNg::GN_Core && second.platform == &GNg::GN_MainState);
    CHECK(second.parallel == &GNg::GN_ParallelState && second.nettrans == &GNg::GN_NetTransState);
    CHECK(second.userthread == &GNg::GN_UserThreadState && second.clipboard == &GNg::GN_ClipboardState);
    CHECK(second.scissor == def.scissor && second.imageload == def.imageload);
    CHECK(second.scissor == &GNg::GN_ScissorState && second.imageload == &GNg::GN_ImageLoadState);
}

TEST_CASE(coreContextSecondInstanceIndependent) {
    // 上下文结构本身可多实例：本地实例默认值与全局一致，写入互不影响
    GNg::GN_CoreContext local;
    CHECK(std::strcmp(local.aSysDir, "assets/") == 0);
    CHECK(local.aAppDir[0] == 0);
    CHECK(local.aSysFile == local.aSysDir + 7);
    CHECK(local.aAppFile == local.aAppDir);
    local.aImgRowEndID = 42;
    CHECK(local.aImgRowEndID == 42);
    CHECK(GNg::GN_Core.aImgRowEndID == 0); // 全局未受影响
    // 平凡上下文同理
    GNg::GN_MainContext mc;
    mc.aFirstCursor = false;
    CHECK(!mc.aFirstCursor);
    CHECK(GNg::GN_MainState.aFirstCursor); // 全局仍是首帧标记
    GNg::GN_FileContext fc;
    CHECK(fc.aRunLogOpenMode[0] == 'w');
    CHECK(fc.aPluginsDir[0] == 'P');
}

TEST_CASE(migratedFunctionStaticDefaults) {
    // B7-5 迁入的函数内 static 初值与原 static 一致（选只由显式调用改写的项，避免跨用例顺序依赖）
    CHECK(GNg::GN_InputState.aOutPutTestChange == 10000);
    CHECK(GNg::GN_InputState.aSensorRx == 0.0f);
    CHECK(GNg::GN_InputState.aSensorRz == 0.0f);
    CHECK(!GNg::GN_InputState.aImeContext.aImeOn);
}
