// GN_ClipboardContext.h — 剪贴板上下文（B7-6，c4）：剪贴板文本缓冲（原 sysProcClipboard.inc gClipboardContext）。结构体定义留 sysProcClipboard.inc。
// 由 sEngineContext 持有（GN_GetEngineContext()）；老 API 经全局 GN_ClipboardState 工作，行为不变。
// 头仅前向声明 + extern 实例（门面只取地址）；结构体定义在实现文件，依赖类型可见处。
#ifndef _GN_CLIPBOARD_CONTEXT_H_
#define _GN_CLIPBOARD_CONTEXT_H_

namespace GNg {
struct GN_ClipboardContext;
extern GN_ClipboardContext GN_ClipboardState; // 默认上下文（仿 GN_Vk / GN_Core）
} // namespace GNg

#endif // _GN_CLIPBOARD_CONTEXT_H_
