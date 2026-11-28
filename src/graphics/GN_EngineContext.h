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
struct GN_InputContext; // 定义在 input/GN_InputContext.h（L5，B7-4）
struct GN_UiBrowserContext; // 定义在 ui/GN_UiBrowserContext.h（L5，B7-4）
struct GN_MainContext; // 定义在 platform/_Main/GN_MainContext.h（L0，B7-4）
struct GN_SoundContext; // 定义在 audio/_Sound/GN_SoundContext.h（L5，B7-4）
struct GN_ParallelContext; // 定义在 core/GN_ParallelContext.h（L1，B7-6；定义体留 GN_Parallel.cpp）
struct GN_NetTransContext; // 定义在 network/GN_NetTransContext.h（L5，B7-6；定义体留 GN_NetTrans.cpp）
struct GN_UserThreadContext; // 定义在 platform/_Sys/GN_UserThreadContext.h（L0，B7-6）
struct GN_ClipboardContext; // 定义在 platform/_Sys/GN_ClipboardContext.h（L0，B7-6）
} // namespace GNg

struct sEngineContext {
    GNg::GN_VkContext *vk; // 渲染子系统上下文；默认指向全局 GN_Vk
    GNg::GN_CoreContext *core; // core 子系统上下文（B7-3）；默认指向全局 GN_Core
    GNg::GN_FileContext *file; // file 子系统上下文（B7-3）；默认指向全局 GN_FileState
    GNg::GN_InputContext *input; // 输入子系统上下文（B7-4）；默认指向全局 GN_InputState
    GNg::GN_UiBrowserContext *ui; // ui 文件浏览上下文（B7-4）；默认指向全局 GN_UiBrowserState
    GNg::GN_MainContext *platform; // 平台主循环上下文（B7-4）；默认指向全局 GN_MainState
    GNg::GN_SoundContext *audio; // 音频子系统上下文（B7-4）；默认指向全局 GN_SoundState
    GNg::GN_ParallelContext *parallel; // 并行任务上下文（B7-6）；默认指向全局 GN_ParallelState
    GNg::GN_NetTransContext *nettrans; // 网络传输上下文（B7-6）；默认指向全局 GN_NetTransState
    GNg::GN_UserThreadContext *userthread; // 用户线程上下文（B7-6）；默认指向全局 GN_UserThreadState
    GNg::GN_ClipboardContext *clipboard; // 剪贴板上下文（B7-6）；默认指向全局 GN_ClipboardState
    sEngineContext();
};

// 默认上下文门面：进程内单例；多实例支持随 c4 推进逐步开放
GN_API sEngineContext &GN_GetEngineContext();

#endif //_GN_ENGINECONTEXT_H_