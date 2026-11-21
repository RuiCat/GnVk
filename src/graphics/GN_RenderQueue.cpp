// GN_RenderQueue.cpp — 层队列的 CPU 实现：入队校验、按层过滤、层内排序。
#include <GN.h>

#include "GN_RenderPipeline.h"
#include "GN_RenderQueue.h"

#include <cstdio>
#include <cstring>

namespace GN3d {
namespace {

void setReason(char *reason, size_t reasonSize, const char *message) {
    if(reason && reasonSize) snprintf(reason, reasonSize, "%s", message);
}

bool layerDrawable(const sRENDER_ITEM &item, eRENDER_LAYER layer) {
    const uint32_t mask = ResolveLayerMask(item);
    if((mask & (1u << (uint32_t)layer)) == 0u) return false;
    return ResolveEntry(item, layer) != 0;
}

} // namespace

void resetRenderQueue(sRENDER_QUEUE *queue) {
    if(!queue) return;
    queue->aCount = 0;
}

bool submitRenderItem(sRENDER_QUEUE *queue, const sRENDER_ITEM &item, char *reason, size_t reasonSize) {
    if(reason && reasonSize) reason[0] = 0;
    if(!queue) { setReason(reason, reasonSize, "render queue is null"); return false; }
    char detail[128];
    if(!ValidateRenderItem(item, detail, sizeof(detail))) {
        setReason(reason, reasonSize, detail);
        return false;
    }
    if(queue->aCount >= kMaxRenderItems) { setReason(reason, reasonSize, "render queue is full"); return false; }
    queue->aItems[queue->aCount] = item;
    ++queue->aCount;
    return true;
}

uint32_t renderQueueCount(const sRENDER_QUEUE *queue) {
    return queue ? queue->aCount : 0u;
}

bool collectRenderLayer(const sRENDER_QUEUE *queue, eRENDER_LAYER layer, const GNg::sMAT44 &view,
                        sRENDER_ITEM *out, uint32_t capacity, uint32_t *outCount,
                        char *reason, size_t reasonSize) {
    if(reason && reasonSize) reason[0] = 0;
    if(!queue || !out || !outCount) { setReason(reason, reasonSize, "layer output arguments are null"); return false; }
    uint32_t count = 0;
    for(uint32_t i = 0; i < queue->aCount; ++i) {
        const sRENDER_ITEM &item = queue->aItems[i];
        if(!layerDrawable(item, layer)) continue;
        if(count >= capacity) { setReason(reason, reasonSize, "layer output buffer is full"); return false; }
        out[count++] = item;
    }
    if(count > 1u) SortRenderItems(out, count, layer, view);
    *outCount = count;
    return true;
}

uint32_t countRenderLayer(const sRENDER_QUEUE *queue, eRENDER_LAYER layer) {
    if(!queue) return 0u;
    uint32_t count = 0;
    for(uint32_t i = 0; i < queue->aCount; ++i) {
        if(layerDrawable(queue->aItems[i], layer)) ++count;
    }
    return count;
}

} // namespace GN3d
