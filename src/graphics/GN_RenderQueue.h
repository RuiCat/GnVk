// GN_RenderQueue.h — 层队列（P3 CPU 核心）：收集渲染项、按层解析过滤、层内稳定排序。
// 纯 CPU：不分配 GPU 资源；顶点/索引/材质为借用指针，调用方保证活到 reset。
#ifndef _GN_RENDER_QUEUE_H_
#define _GN_RENDER_QUEUE_H_

#include "GN_RenderPipeline.h"

#include <cstddef>
#include <cstdint>

namespace GN3d {

// 固定容量队列（kMaxRenderItems）；单线程使用。
struct sRENDER_QUEUE {
    sRENDER_ITEM aItems[kMaxRenderItems];
    uint32_t aCount;
};

void resetRenderQueue(sRENDER_QUEUE *queue);
// 校验并入队；失败返回 false 并写 reason（容量满/非法 item）。
bool submitRenderItem(sRENDER_QUEUE *queue, const sRENDER_ITEM &item, char *reason, size_t reasonSize);
uint32_t renderQueueCount(const sRENDER_QUEUE *queue);
// 收集某层可绘制项（层掩码解析 + 入口存在）到 out 并层内排序；容量不足返回 false。
bool collectRenderLayer(const sRENDER_QUEUE *queue, eRENDER_LAYER layer, const GNg::sMAT44 &view,
                        sRENDER_ITEM *out, uint32_t capacity, uint32_t *outCount,
                        char *reason, size_t reasonSize);
// 统计某层可绘制数量（不排序），供调度与验收使用。
uint32_t countRenderLayer(const sRENDER_QUEUE *queue, eRENDER_LAYER layer);

} // namespace GN3d
#endif // _GN_RENDER_QUEUE_H_
