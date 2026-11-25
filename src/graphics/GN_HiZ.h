#pragma once
#ifndef _GN_HIZ_H_
#define _GN_HIZ_H_
// ╔══════════════════════════════════════════════════════════╗
// ║  GN_mini — Hi-Z 遮挡剔除 + Overdraw 测量（B4，纯 CPU）    ║
// ╚══════════════════════════════════════════════════════════╝
//
// ═══ 先说清楚这份实现的边界 ═══
// 真实的 Hi-Z 遮挡剔除在 GPU 上跑：从上一帧的深度缓冲建金字塔，用 compute 测试
// 下一帧的物体包围盒，结果写进间接绘制参数。**本实现是 CPU 版**，原因是本容器
// 只有 lavapipe（软件 Vulkan）—— 在软件光栅化上测 overdraw 的性能特征与真实 GPU
// 完全不同，测出来的数字没有意义。
//
// 所以这里交付的是**算法与数据结构**：完整单测、与暴力算法逐例对拍。
// 它能给你两件确定的东西：
//   1. `measureOverdraw` —— overdraw **倍数**（每个像素被着色几次）。这个数字
//      与硬件无关，真实 GPU 上同样是这个倍数，只是每次的代价不同。
//   2. `cHIZ` —— 金字塔构建 + 遮挡测试，正确性可验证。
// **未验证的性能收益**必须在接入 GPU 时实测，不能拿 CPU 版的结果当承诺。
//
// ═══ 算法 ═══
// 深度金字塔：第 0 级 = 全分辨率深度；第 k 级 = 第 k-1 级每 2×2 取**最大**深度。
// 取 max 是为了保守：只有"整块区域的最远点都比物体最近点更近"才能判定物体被遮挡，
// 否则可能误剔。
//
// 测试一个物体：把它的世界 AABB 投影到屏幕（8 角点 → 屏幕矩形 + 最近深度），
// 选出"能完整覆盖该矩形的最小 mip 级"，取那一级的 max 深度比较。

#include "GN_3Dmath.h"
#include "GN_Spatial.h"

#include <cstdint>
#include <vector>

namespace GN3d {

// ═══════════════════════════════════════════════════════════
//  Overdraw 测量
// ═══════════════════════════════════════════════════════════
struct sOVERDRAW_STATS {
    uint64_t aFragments; // 所有物体覆盖的片元总数（= 片元着色器调用次数）
    uint64_t aCoveredPixels; // 至少有 1 个片元覆盖的像素数
    uint32_t aObjects; // 参与统计的物体数
    uint32_t aWidth;
    uint32_t aHeight;

    sOVERDRAW_STATS();
    // 每像素平均被覆盖几次（= overdraw 倍数）。无覆盖时返回 0。
    float OverdrawRatio() const;
};

// 用每个物体的世界 AABB（8 角点投影到屏幕的矩形）统计覆盖次数。
//
// **这是保守上界**：真实三角形通常比 AABB 小，所以实际 overdraw 不会高于这个值。
// 用它来判断"要不要做遮挡剔除"是合适的 —— 上界都不高就没必要做。
//
// viewProj：世界 → NDC（Vulkan 约定，y 朝下、z ∈ [0,1]）。
// boxes/count：物体的世界 AABB。
// 返回统计结果；内部按 (w*h) 分配计数缓冲，w*h 过大（> 16M）时返回全 0 的统计
// （避免为了一个诊断把内存打爆）。
sOVERDRAW_STATS measureOverdraw(const GNg::sMAT44 &viewProj, uint32_t width, uint32_t height,
                                const sAABB *boxes, uint32_t count);

// ═══════════════════════════════════════════════════════════
//  Hi-Z 深度金字塔
// ═══════════════════════════════════════════════════════════
class cHIZ {
  public:
    cHIZ();

    // 用一张全分辨率深度图建金字塔。第 0 级复制输入。
    // depth：width*height 个深度值（NDC 深度，0=near，1=far；**1 表示"没有几何"**
    // 也正好是"最远"，与取 max 的约定天然一致）。
    // 尺寸非 2 的幂时逐级 ceil 到半（1 停）。
    void Build(const float *depth, uint32_t width, uint32_t height);

    void Clear();
    bool IsBuilt() const { return !aLevels.empty(); }
    uint32_t LevelCount() const { return (uint32_t)aLevels.size(); }
    uint32_t LevelWidth(uint32_t level) const;
    uint32_t LevelHeight(uint32_t level) const;
    // 某级某像素的 max 深度。越界返回 1.0（最远 = 不遮挡任何东西）。
    float DepthAt(uint32_t level, uint32_t x, uint32_t y) const;

    // 遮挡测试：世界 AABB 是否**完全**被已有深度遮挡。
    //
    // 保守：任一条件不满足就返回 false（宁可多画，不可漏画）。
    //   * 未建金字塔 / 视口为空 ⇒ false
    //   * AABB 有任何角点在视锥外 / 跨越近平面 ⇒ false（屏幕矩形不可靠）
    //   * AABB 的最小屏幕矩形覆盖到金字塔外 ⇒ false
    bool IsOccluded(const GNg::sMAT44 &viewProj, uint32_t viewportW, uint32_t viewportH,
                    const sAABB &box) const;

    // 诊断：本次测试用了哪一级（IsOccluded 之后可读）。
    uint32_t LastTestedLevel() const { return aLastLevel; }

  private:
    struct sLevel {
        uint32_t aWidth;
        uint32_t aHeight;
        std::vector<float> aDepths;
    };
    std::vector<sLevel> aLevels;
    mutable uint32_t aLastLevel;
};

} // namespace GN3d
#endif // _GN_HIZ_H_
