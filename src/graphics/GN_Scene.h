#pragma once
#ifndef _GN_SCENE_H_
#define _GN_SCENE_H_
// ╔══════════════════════════════════════════════════════════╗
// ║  GN_mini — 场景（P6：统一剔除 / LOD / 排序）             ║
// ╚══════════════════════════════════════════════════════════╝
//
// 为什么需要它：P1–P5 把"每个实体的控制器"做好了，但**"这一帧要画谁、按什么顺序画"
// 仍然散在调用方**。两个示例各自手写"算相机 → 拟合阴影 → 遍历模型画"，
// 换一个场景就要再抄一遍，而排序/剔除一旦抄错就是画面错误（透明物体顺序错、物体凭空消失）。
//
// 这一层的职责就三件事，全部在一个函数里完成（`PrepareRender`）：
//   1. **剔除**：逐实体做视锥剔除（内部走 `cMESH_CONTROLLER::Prepare`，与 GPU 无关）；
//   2. **LOD**：逐网格选级，策略固化在 `sLOD_POLICY`（像素误差 + 屏幕占比）；
//   3. **排序**：不透明**由近到远**（早写深度、少 overdraw），透明**由远到近**（混合正确）。
//
// 与 `GN_RenderPipeline.h` 的区别：那边是**逐材质/逐层**的排序契约（跨 pass 复用）；
// 这里是**逐实体**的粗排序，粒度到"模型"而不是"网格"。两者不冲突：
//   scene 决定"先画哪个实体"，cMODEL::DrawPBRSorted 决定"实体内部先画哪个网格"。
//
// 不引入 ECS：`cSCENE` 只存指针数组（plan §7）。
#include <GN.h> // 必须先于本组头：GN_API 等基础宏
#include "GN_3Dlod.h" // sLOD_VIEW / sLOD_POLICY
#include "GN_3Dmodel.h" // cMODEL

#include "GN_Spatial.h" // sFRUSTUM

#include <cstdint>
#include <vector>

namespace GN3d {

// 为什么只前置声明 cENTITY：本头会被 <GN.h> 在 GN_Entity.h 之前/之后以不同顺序包含，
// 直接 include GN_Entity.h 会撞上"GN_Entity.h 先被 TU 自己包含（守卫已置位）→
// 本头里的 include 变成空操作 → cENTITY 还没定义"的循环守卫问题。
// 本头的所有接口只需要 cENTITY*（不透明指针），完整类型只在 .cpp 里用。
class cENTITY;

// 一条绘制项：实体 + 本帧的排序键。
struct sDRAW_ITEM {
    cENTITY *aEntity;
    float aDistance; // 相机到实体世界 AABB 中心的距离（排序键）
    bool aTransparent; // 透明物体排在后面、由远到近

    sDRAW_ITEM();
};

class cSCENE {
  public:
    cSCENE();

    // ── 成员 ──
    // 加入/移除（非拥有；同一指针重复加入会被忽略）。entity 必须非空。
    bool Add(cENTITY *entity);
    bool Remove(cENTITY *entity);
    void Clear();
    uint32_t Count() const;
    cENTITY *At(uint32_t index) const; // 越界返回 nullptr

    // ── 每帧 ──
    // 固定顺序更新所有实体（物理 → 动画 → 骨骼，见 cENTITY::Update）。
    void Update(float dt);

    // 剔除 + LOD + 排序，产出 `DrawOrder()`。frustum 为空 = 不剔除（只排序）。
    // 返回可见实体数。等价于"这一帧要画几个"。
    uint32_t PrepareRender(const sLOD_VIEW &view, const sFRUSTUM *frustum);

    uint32_t VisibleCount() const;
    const sDRAW_ITEM &DrawItem(uint32_t index) const; // 越界返回零值项（aEntity = nullptr）
    const std::vector<sDRAW_ITEM> &DrawOrder() const;

    // 按 DrawOrder 逐个绘制（实体级顺序；实体内部的网格排序仍由 cMESH_CONTROLLER 决定）。
    uint32_t Draw(const GNg::sVEC3 &cameraPos) const;
    // 阴影：按 DrawOrder 逐个提交投影体。
    uint32_t DrawShadow() const;

    // ── 策略 ──
    void SetLodPolicy(const sLOD_POLICY &policy);
    const sLOD_POLICY &LodPolicy() const;
    void SetCullEnabled(bool enabled); // 默认 true
    bool CullEnabled() const;
    void SetSortEnabled(bool enabled); // 默认 true（关闭时保持加入顺序，便于对拍）
    bool SortEnabled() const;

    // 诊断
    uint32_t LastCulledCount() const; // 参与剔除的实体数（有模型的）
    uint32_t LastRejectedCount() const; // 被剔除掉的数量

  private:
    std::vector<cENTITY *> aEntities;
    std::vector<sDRAW_ITEM> aDrawOrder;
    sLOD_POLICY aPolicy;
    bool aCull;
    bool aSort;
    uint32_t aCulled;
    uint32_t aRejected;
};

} // namespace GN3d
#endif // _GN_SCENE_H_
