// GN_mini 引擎上下文门面（B7-1，c4 状态收敛，2026-11-26）。
//
// 目标：全仓全局/static 状态逐步收敛进各子系统上下文，再由 sEngineContext 统一持有；
// 老 API 继续经默认上下文工作，行为不变（REFACTOR_PLAN §13 B7）。
// B7-1：渲染子系统指针 vk（默认指向全局 GN_Vk）；2D 帧状态已迁入 GN_Vk.s2DRes。
// B7-3：core/file 子系统指针（默认指向全局 GN_Core / GN_FileState）。
// 后续批次：输入 / UI 等子系统上下文指针依次加入。
#ifndef _GN_ENGINECONTEXT_H_
#define _GN_ENGINECONTEXT_H_

#include "GN.h" // GN_API（L2→L1 向下包含，分层合规）

namespace GNg {
struct GN_VkContext; // 定义在 _Vulkan/GN_Vk.h；前向声明避免 L2→L3 越级包含
struct GN_CoreContext; // 定义在 core/GN_CoreContext.h（L1）
struct GN_FileContext; // 定义在 file/GN_FileContext.h（L1）
} // namespace GNg

struct sEngineContext {
    GNg::GN_VkContext *vk; // 渲染子系统上下文；默认指向全局 GN_Vk
    GNg::GN_CoreContext *core; // core 子系统上下文（B7-3）；默认指向全局 GN_Core
    GNg::GN_FileContext *file; // file 子系统上下文（B7-3）；默认指向全局 GN_FileState
    sEngineContext();
};

// 默认上下文门面：进程内单例；多实例支持随 c4 推进逐步开放
GN_API sEngineContext &GN_GetEngineContext();

#endif //_GN_ENGINECONTEXT_H_