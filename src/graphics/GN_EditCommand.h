#pragma once
#ifndef _GN_EDIT_COMMAND_H_
#define _GN_EDIT_COMMAND_H_

// ╔══════════════════════════════════════════════════════════╗
// ║  GN_mini — 编辑命令与撤销栈（A4）                        ║
// ╚══════════════════════════════════════════════════════════╝
//
// 为什么需要它：上一轮把 Gizmo 接上了鼠标，拖一下物体就动了 —— 但**拖错了就回不去**。
// 没有撤销的编辑器不能用，所以这是"接完输入"之后立刻要补的东西。
//
// 设计取舍（每条都有用例）：
//   * **只做变换命令**。`sTRANSFORM_STATE` 只有 pos/rot/scale 三项，因为这是
//     Gizmo 能产生的全部变化。不做通用的"任意操作"命令基类 —— 那需要虚函数、
//     clone、序列化，而眼下唯一的需求就是撤销拖拽。将来真需要再加。
//   * **快照式而不是增量式**。命令存 before/after 两份完整状态，不是"Δ+0.5m"。
//     增量式在"拖拽中途改吸附步长"、"两次拖拽叠在同一条轴上"这类场景下会漂移；
//     快照式永远精确，代价只是 3 个 vec。
//   * **`Push` 新命令会清空 redo 栈**（标准行为）。否则会出现"撤销掉 A，再做一个
//     B，然后 redo 把 A 又贴回来"这种两条历史链互相打架的状态。
//   * **容量满时丢最老的**（FIFO），不是拒绝新命令 —— 用户更希望"最近的能撤销"。
//   * **before == after 的命令被拒绝**。拖动时松手在同一个像素上不该产生一条历史，
//     否则按十次撤销才退一步，手感极差。
//
// 与 Gizmo 的衔接：`cGIZMO_INTERACTION` 在 Begin 时记下起始 TRS，
// 调用方在 End 时用 `captureTransform` 拿当前状态，两条一起 Push 即可。

#include <GN.h> // 必须先于本组头（GN_API 等基础宏）
#include "GN_3Dmath.h" // sVEC3 / sQUAT / sMAT44

#include <cstdint>
#include <vector>

namespace GN3d {

class cMODEL;

// ═══════════════════════════════════════════════════════════
//  变换快照
// ═══════════════════════════════════════════════════════════
struct sTRANSFORM_STATE {
    GNg::sVEC3 aPosition;
    GNg::sQUAT aRotation;
    GNg::sVEC3 aScale;

    sTRANSFORM_STATE();
};

// 从模型读出 / 写回。applyTransform 会调 `cMODEL::UpdateTransform()`（重建矩阵），
// 但**不会**重算世界 AABB —— 那是每帧一次的操作，由调用方决定何时做。
sTRANSFORM_STATE captureTransform(const cMODEL &model);
void applyTransform(const sTRANSFORM_STATE &state, cMODEL *model);

// 逐位比较（容差比较在这里是错的：撤销要"精确回到原值"，不是"差不多"）。
bool transformEquals(const sTRANSFORM_STATE &lhs, const sTRANSFORM_STATE &rhs);

// ═══════════════════════════════════════════════════════════
//  命令
// ═══════════════════════════════════════════════════════════
struct sEDIT_COMMAND {
    cMODEL *aTarget; // 非拥有；为 nullptr 的命令会被 Push 拒绝
    sTRANSFORM_STATE aBefore;
    sTRANSFORM_STATE aAfter;
    char aLabel[24]; // 便于 UI 显示"撤销：移动"

    sEDIT_COMMAND();
};

// ═══════════════════════════════════════════════════════════
//  撤销栈
// ═══════════════════════════════════════════════════════════
class cUNDO_STACK {
  public:
    cUNDO_STACK();

    // 容量上限（默认 64）。缩小时从最老的开始丢。0 会被钳到 1。
    void SetCapacity(uint32_t capacity);
    uint32_t Capacity() const;

    // 压入一条命令。以下情况返回 false 且不改变任何状态：
    //   * aTarget == nullptr（撤销时会崩）；
    //   * before 与 after 完全相同（见文件头）。
    // 成功时**清空 redo 栈**。
    bool Push(const sEDIT_COMMAND &command);

    // 便捷重载：直接从"模型 + 起始快照"构造。
    bool PushTransform(cMODEL *target, const sTRANSFORM_STATE &before, const char *label = 0);

    // 撤销/重做。无可撤销项时返回 false。
    // 返回 true 表示目标模型已被改写（调用方通常需要重算 AABB）。
    bool Undo();
    bool Redo();

    bool CanUndo() const;
    bool CanRedo() const;
    uint32_t UndoDepth() const;
    uint32_t RedoDepth() const;
    void Clear();

    // 栈顶（下一次 Undo / Redo 会作用的那条）。空栈返回 nullptr。
    const sEDIT_COMMAND *PeekUndo() const;
    const sEDIT_COMMAND *PeekRedo() const;

  private:
    std::vector<sEDIT_COMMAND> aUndo;
    std::vector<sEDIT_COMMAND> aRedo;
    uint32_t aCapacity;
};

} // namespace GN3d
#endif // _GN_EDIT_COMMAND_H_
