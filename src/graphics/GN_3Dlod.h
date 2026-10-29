#pragma once
#ifndef _GN_3DLOD_H_
#define _GN_3DLOD_H_
// ╔══════════════════════════════════════════════════════════╗
// ║  GN_mini — P10 动态 LOD：屏幕像素误差选级（纯 CPU）       ║
// ╚══════════════════════════════════════════════════════════╝
//
// 契约：
//   * 误差公式 `S = aMaxError * screenHeight / (2 * d * tan(fovY / 2))`，d = 相机到世界 AABB 的
//     最近距离（保守）；相机在盒内 / 距离为 0 / 视口或 FOV 非法时返回 +INF（不得降级）。
//   * `selectLod` 返回**从粗到细第一个 S <= 阈值的级**（即最粗的可接受级），默认阈值 1.0 px；
//     由粗变细立即生效，由细变粗需要额外余量（降档阈值 = 0.8 × 阈值）防抖。
//   * 无状态函数 + 显式 `prevLevel` 输入：调用方跨帧持有上一帧级别，便于单测与多 pass 复用。
//   * `aLodCount == 0`（或 `aLods == 0`）退化为单级：选级恒为 0，绘制与旧路径一致。
//
// 与 §3.4 草案的差异（已登记到 docs/3D_FEATURE_PLAN.md）：草案签名未带世界 AABB，但公式需要它，
// 因此显式传入 `worldAabb`（通常取 `cMODEL::aWorldAABB`），避免隐式假设模型在原点。
#include "GN_3Dmath.h"
#include "GN_3Dmodel.h"
#include "GN_Spatial.h"

#include <cstdint>

namespace GN3d {

struct sLOD_VIEW {
    float aScreenHeight; // 视口像素高（> 0）
    float aFovYRad; // 垂直 FOV（弧度，(0, π)）
    GNg::sMAT44 aViewProj; // 保留（v1 选级只用距离/误差；跨 pass 复用时供调用方核对）
    GNg::sVEC3 aCamPos; // 相机世界位置
};

// 该级在当前视角下的屏幕像素误差；不可判定时返回 +INF。
float lodScreenError(const sMESH_LOD &lod, const sAABB &worldAabb, const sLOD_VIEW &view);

// 选级：maxScreenErrorPx <= 0 时按默认 1.0 px；prevLevel 越界会被钳到 [0, aLodCount-1]。
uint32_t selectLod(const sMESH &mesh, const sAABB &worldAabb, const sLOD_VIEW &view,
                   float maxScreenErrorPx, uint32_t prevLevel);

// 便捷入口：按模型世界 AABB 给每个网格选级，写入 levels[0..aMeshCount)（容量不足时截断）。
// 返回写入的级数；任一网格无 LOD 时写 0（单级行为）。
uint32_t selectLodForModel(const cMODEL &model, const sLOD_VIEW &view, float maxScreenErrorPx,
                           uint32_t *levels, uint32_t levelCapacity);

} // namespace GN3d
#endif // _GN_3DLOD_H_
