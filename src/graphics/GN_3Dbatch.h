#pragma once
#ifndef _GN_3DBATCH_H_
#define _GN_3DBATCH_H_
// ╔══════════════════════════════════════════════════════════╗
// ║  GN_mini — P9 绘制项批处理：排序键与状态切换统计          ║
// ╚══════════════════════════════════════════════════════════╝
//
// 契约：
//   * 纯 CPU、无 Vulkan 依赖；不改任何既有绘制 API，调用方按排序后的顺序提交即可。
//   * 排序为**稳定**排序，键依次为：aPipelineKey 升序、aMaterialKey 升序、aDepth 升序、
//     aOrder 升序（保证同键同深度的项保持调用方给定顺序）。
//   * `countBatchSwitches` 统计相邻项之间"管线键或材质键发生变化"的次数（同一键连续 = 0 次切换）。
#include <cstdint>

namespace GN3d {

struct sBATCH_DRAW_ITEM {
    const void *aModel; // 不透明句柄（本层不解引用，排序/统计不读模型内容）
    uint64_t aPipelineKey; // 管线键（同键可省一次管线绑定）
    uint64_t aMaterialKey; // 材质键（同键可省一次描述符集绑定）
    float aDepth; // 视空间深度（越小越近；同键内前→后）
    uint32_t aOrder; // 调用方给定的原始序号（稳定排序的兜底键）
};

// 稳定排序（count == 0 / items 为空时安全返回）；排序后 aOrder 保留原始序号便于校验。
void sortBatchItems(sBATCH_DRAW_ITEM *items, uint32_t count);

// 返回相邻项之间管线/材质键变化的次数（count <= 1 时为 0）。
uint32_t countBatchSwitches(const sBATCH_DRAW_ITEM *items, uint32_t count);

} // namespace GN3d
#endif // _GN_3DBATCH_H_
