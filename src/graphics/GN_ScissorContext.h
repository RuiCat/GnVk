// GN_ScissorContext.h — 2D 剪裁嵌套栈上下文（B7-7，c4）：嵌套矩形栈 + 当前层（原 GN_2D.cpp gScissorContext）。L2 API 层状态，区别于 GN_Vk.s2DRes 后端当前剪裁。结构体定义留 GN_2D.cpp（依赖 GNg::sRECT/GN_SCISSOR_LV_NUM）。
// 由 sEngineContext 持有；老 API 经全局 GN_ScissorState 工作，行为不变。头仅前向声明 + extern 实例（门面只取地址）。
#ifndef _GN_SCISSOR_CONTEXT_H_
#define _GN_SCISSOR_CONTEXT_H_

namespace GNg {
struct GN_ScissorContext;
extern GN_ScissorContext GN_ScissorState; // 默认上下文（仿 GN_Vk / GN_Core）
} // namespace GNg

#endif // _GN_SCISSOR_CONTEXT_H_
