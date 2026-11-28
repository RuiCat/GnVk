// GN_mini 引擎上下文门面实现（B7-1/B7-3，c4 状态收敛，2026-11-26）。
#include "GN_EngineContext.h"
#include "_Vulkan/GN_Vk.h" // 分层白名单：默认门面需取全局 GN_Vk 地址（GN_3Dmodel 同例）
#include "GN_CoreContext.h" // B7-3：core 上下文（L2→L1 向下包含）
#include "GN_FileContext.h" // B7-3：file 上下文（L2→L1 向下包含）
#include "GN_InputContext.h" // B7-4：input 上下文（L2→L5 向上，按门面职责白名单登记）
#include "GN_UiBrowserContext.h" // B7-4：ui 文件浏览上下文（同上）
#include "_Main/GN_MainContext.h" // B7-4：platform 上下文（L2→L0 向下；头在 platform/_Main）
#include "_Sound/GN_SoundContext.h" // B7-4：audio 上下文（L2→L5 向上，白名单登记；头在 audio/_Sound）
#include "GN_ParallelContext.h" // B7-6：parallel 上下文（L2→L1 向下）
#include "GN_NetTransContext.h" // B7-6：network 上下文（L2→L5 向上，白名单登记；只取地址）
#include "_Sys/GN_UserThreadContext.h" // B7-6：用户线程上下文（L2→L0 向下）
#include "_Sys/GN_ClipboardContext.h" // B7-6：剪贴板上下文（L2→L0 向下）

sEngineContext::sEngineContext()
    : vk(&GNg::GN_Vk), core(&GNg::GN_Core), file(&GNg::GN_FileState),
      input(&GNg::GN_InputState), ui(&GNg::GN_UiBrowserState),
      platform(&GNg::GN_MainState), audio(&GNg::GN_SoundState),
      parallel(&GNg::GN_ParallelState), nettrans(&GNg::GN_NetTransState),
      userthread(&GNg::GN_UserThreadState), clipboard(&GNg::GN_ClipboardState) {}

sEngineContext &GN_GetEngineContext() {
    static sEngineContext ctx; // 默认上下文：老 API 行为不变
    return ctx;
}