// GN_UserThreadContext.h — 用户线程上下文（B7-6，c4）：用户线程登记表（原 sysProcess.inc gUserThreadContext）。结构体定义留 sysProcess.inc（依赖局部 GT_sTHREAD_ARG）。
// 由 sEngineContext 持有（GN_GetEngineContext()）；老 API 经全局 GN_UserThreadState 工作，行为不变。
// 头仅前向声明 + extern 实例（门面只取地址）；结构体定义在实现文件，依赖类型可见处。
#ifndef _GN_USERTHREAD_CONTEXT_H_
#define _GN_USERTHREAD_CONTEXT_H_

namespace GNg {
struct GN_UserThreadContext;
extern GN_UserThreadContext GN_UserThreadState; // 默认上下文（仿 GN_Vk / GN_Core）
} // namespace GNg

#endif // _GN_USERTHREAD_CONTEXT_H_
