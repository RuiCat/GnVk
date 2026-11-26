// GN_mini 引擎上下文门面（B7-1，c4 状态收敛，2026-11-26）。
//
// 目标：全仓全局/static 状态逐步收敛进各子系统上下文，再由 sEngineContext 统一持有；
// 老 API 继续经默认上下文工作，行为不变（REFACTOR_PLAN §13 B7）。
// 本批（B7-1）：渲染子系统指针 vk（默认指向全局 GN_Vk）；2D 帧状态已迁入 GN_Vk.s2DRes。
// 后续批次：资源缓存 / 输入 / UI 等子系统上下文指针依次加入。
#ifndef _GN_ENGINECONTEXT_H_
#define _GN_ENGINECONTEXT_H_

#include "GN.h" // GN_API（L2→L1 向下包含，分层合规）

namespace GNg { struct GN_VkContext; } // 定义在 _Vulkan/GN_Vk.h；前向声明避免 L2→L3 越级包含

struct sEngineContext {
    GNg::GN_VkContext *vk; // 渲染子系统上下文；默认指向全局 GN_Vk
    sEngineContext();
};

// 默认上下文门面：进程内单例；多实例支持随 c4 推进逐步开放
GN_API sEngineContext &GN_GetEngineContext();

#endif //_GN_ENGINECONTEXT_H_