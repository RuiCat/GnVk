#pragma once
#ifndef GN_3D_DECAL_H
#define GN_3D_DECAL_H
// ╔══════════════════════════════════════════════════════════╗
// ║  GN_mini — 3D 地面贴花（CPU 池；渲染在 _Vulkan/vkDecal） ║
// ╚══════════════════════════════════════════════════════════╝
//
// 契约：
//   * 固定容量池（`create(capacity)`），槽位一经创建不变；池满时**覆盖最老**的槽（环形写游标）。
//   * 寿命：`aLife > 0` 时 `aAge >= aLife` 即回收；`aLife <= 0` 表示永久贴花（不自动消失）。
//   * 渐隐：存活且非永久的贴花在最后 30% 寿命内 alpha 线性降到 0（`decalFadeAlpha`）。
//   * 本层是纯 CPU：不依赖 Vulkan、不分配 GPU 资源、不渲染；渲染侧用
//     `gatherDecalRenderItems` 把存活贴花转成渲染条目（alpha 已乘渐隐）。
#include <GN.h> // 先定义 GN_API / 模块宏，GN_3Dmath.h 依赖它
#include "GN_3Dmath.h"

#include <cstdint>
#include <vector>

namespace GN3d {

// 贴花种类：0 = 软圆斑（水渍），1 = 环状溅射（雨滴打在地面上的环）。
enum class DecalKind : uint32_t { SPOT = 0, SPLASH_RING = 1 };

struct sDECAL {
    GNg::sVEC3 aPosition; // 世界位置（贴花中心，调用方保证略高于地面）
    float aSize; // 世界直径（m，> 0）
    GNg::sVEC4 aColor; // rgba；a = 初始不透明度（0..1）
    float aLife; // 总寿命（s）；<= 0 表示永久
    float aAge; // 已存活时间（s）
    uint32_t aKind; // DecalKind
};

// 渲染条目（只含渲染需要的字段，避免把 CPU 池结构直接塞进绘制参数）。
struct sDECAL_RENDER_ITEM {
    float aPosition[3];
    float aSize;
    float aColor[4]; // alpha 已乘寿命渐隐
    uint32_t aKind;
};

class cDECAL_SYSTEM {
  public:
    cDECAL_SYSTEM();
    ~cDECAL_SYSTEM();

    // capacity == 0 或池已创建时参数非法 ⇒ 返回 false 且不改变既有系统。
    bool create(uint32_t capacity);
    // 清空所有贴花（保留容量与游标位置）。
    void clear();

    // 生成一个贴花。参数非法（size <= 0、alpha 越界、位置非有限、life < 0）返回 false 且不修改；
    // 池满时覆盖最老的槽位并返回 true。
    bool spawn(const sDECAL &decal);

    // 推进寿命：dt 非有限/<= 0 忽略，dt 钳在 0.1 s；超龄存活数减少。
    void update(float dt);

    bool isCreated() const { return aCapacity != 0; }
    uint32_t capacity() const { return aCapacity; }
    uint32_t activeCount() const { return aActiveCount; }
    const sDECAL *decals() const { return aDecals.empty() ? 0 : &aDecals[0]; }
    bool alive(uint32_t index) const {
        return index < aAlive.size() && aAlive[index] != 0;
    }
    // 当前不透明度：未存活 / 已过渐隐段时可能为 0。
    float decalFadeAlpha(uint32_t index) const;

  private:
    void recycleSlot(uint32_t index);

    std::vector<sDECAL> aDecals;
    std::vector<uint8_t> aAlive;
    uint32_t aCapacity;
    uint32_t aActiveCount;
    uint32_t aCursor; // 下一个写入槽（池满时覆盖最老）
};

// 把存活贴花转成渲染条目（alpha 已乘渐隐）；返回写入条目数（<= maxOut，out 为空返回 0）。
uint32_t gatherDecalRenderItems(const cDECAL_SYSTEM &system, sDECAL_RENDER_ITEM *out,
                                uint32_t maxOut);

} // namespace GN3d
#endif // GN_3D_DECAL_H
