// GN_3Dbatch.cpp — P9-c 绘制项批处理：稳定排序与状态切换统计（纯 CPU，无 Vulkan 依赖）。
//
// 契约：
//   * 排序键依次为 pipelineKey ↑、materialKey ↑、depth ↑、order ↑；使用 std::stable_sort，
//     与调用方给定的原始顺序一起构成确定性结果。
//   * 深度比较写成 `<` 双向判定，NaN 深度不参与比较（落到 aOrder），保证严格弱序。
//   * 本文件只做排序/统计，不做实际绘制；调用方按排序结果提交即可减少状态切换。
#include "GN_3Dbatch.h"

#include <algorithm>

namespace GN3d {

namespace {

bool itemLess(const sBATCH_DRAW_ITEM &a, const sBATCH_DRAW_ITEM &b) {
    if(a.aPipelineKey != b.aPipelineKey) return a.aPipelineKey < b.aPipelineKey;
    if(a.aMaterialKey != b.aMaterialKey) return a.aMaterialKey < b.aMaterialKey;
    if(a.aDepth < b.aDepth) return true;
    if(b.aDepth < a.aDepth) return false;
    return a.aOrder < b.aOrder;
}

} // namespace

void sortBatchItems(sBATCH_DRAW_ITEM *items, uint32_t count) {
    if(!items || count < 2) return;
    std::stable_sort(items, items + count, itemLess);
}

uint32_t countBatchSwitches(const sBATCH_DRAW_ITEM *items, uint32_t count) {
    if(!items || count < 2) return 0;
    uint32_t switches = 0;
    for(uint32_t i = 1; i < count; ++i) {
        if(items[i].aPipelineKey != items[i - 1].aPipelineKey ||
           items[i].aMaterialKey != items[i - 1].aMaterialKey) {
            ++switches;
        }
    }
    return switches;
}

} // namespace GN3d
