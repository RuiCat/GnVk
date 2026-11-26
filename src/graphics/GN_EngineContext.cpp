// GN_mini 引擎上下文门面实现（B7-1/B7-3，c4 状态收敛，2026-11-26）。
#include "GN_EngineContext.h"
#include "_Vulkan/GN_Vk.h" // 分层白名单：默认门面需取全局 GN_Vk 地址（GN_3Dmodel 同例）
#include "GN_CoreContext.h" // B7-3：core 上下文（L2→L1 向下包含）
#include "GN_FileContext.h" // B7-3：file 上下文（L2→L1 向下包含）

sEngineContext::sEngineContext() : vk(&GNg::GN_Vk), core(&GNg::GN_Core), file(&GNg::GN_FileState) {}

sEngineContext &GN_GetEngineContext() {
    static sEngineContext ctx; // 默认上下文：老 API 行为不变
    return ctx;
}