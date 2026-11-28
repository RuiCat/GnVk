// GN_NetTransContext.h — 网络传输上下文（B7-6，c4）：外部通知 socket/端口（原 GN_NetTrans.cpp gNetTransContext）。结构体定义留 GN_NetTrans.cpp（依赖 P<GNn::cSOC_UDP>）。
// 由 sEngineContext 持有（GN_GetEngineContext()）；老 API 经全局 GN_NetTransState 工作，行为不变。
// 头仅前向声明 + extern 实例（门面只取地址）；结构体定义在实现文件，依赖类型可见处。
#ifndef _GN_NETTRANS_CONTEXT_H_
#define _GN_NETTRANS_CONTEXT_H_

namespace GNg {
struct GN_NetTransContext;
extern GN_NetTransContext GN_NetTransState; // 默认上下文（仿 GN_Vk / GN_Core）
} // namespace GNg

#endif // _GN_NETTRANS_CONTEXT_H_
