// GN_ParallelContext.h — 并行任务上下文（B7-6，c4）：两路空闲任务轮表头（原 GN_Parallel.cpp gParallelContext）。结构体定义留 GN_Parallel.cpp（依赖 GN_sTASK_LIST_NODE）。
// 由 sEngineContext 持有（GN_GetEngineContext()）；老 API 经全局 GN_ParallelState 工作，行为不变。
// 头仅前向声明 + extern 实例（门面只取地址）；结构体定义在实现文件，依赖类型可见处。
#ifndef _GN_PARALLEL_CONTEXT_H_
#define _GN_PARALLEL_CONTEXT_H_

namespace GNg {
struct GN_ParallelContext;
extern GN_ParallelContext GN_ParallelState; // 默认上下文（仿 GN_Vk / GN_Core）
} // namespace GNg

#endif // _GN_PARALLEL_CONTEXT_H_
