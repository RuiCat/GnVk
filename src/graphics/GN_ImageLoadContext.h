// GN_ImageLoadContext.h — 图像加载上下文（B7-7，c4）：最近一次图像加载类型名（原 GN_2Dtype.cpp gImageLoadContext）。结构体定义留 GN_2Dtype.cpp（依赖 GNg::ImgFileTypeNames）。
// 由 sEngineContext 持有；老 API 经全局 GN_ImageLoadState 工作，行为不变。头仅前向声明 + extern 实例（门面只取地址）。
#ifndef _GN_IMAGELOAD_CONTEXT_H_
#define _GN_IMAGELOAD_CONTEXT_H_

namespace GNg {
struct GN_ImageLoadContext;
extern GN_ImageLoadContext GN_ImageLoadState; // 默认上下文（仿 GN_Vk / GN_Core）
} // namespace GNg

#endif // _GN_IMAGELOAD_CONTEXT_H_
