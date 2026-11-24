// GN_EditCommand.cpp — 变换快照 + 撤销栈。
//
// 实现口径见头文件。这里只强调两处容易写错的地方：
//   1. `Push` 必须先清 redo **再**压栈，且清空要在"命令确实被接受"之后 ——
//      否则一条被拒绝的空命令会把用户的 redo 历史清掉。
//   2. 容量裁剪要发生在压栈之后，从**最老**的丢（erase 头部）。丢最新的会让
//      "刚做的操作撤销不了"，与直觉相反。

#include <GN.h>
#include "GN_EditCommand.h"
#include "GN_3Dmodel.h" // cMODEL::SetPosition/SetRotation/SetScale

#include <cstring>

namespace GN3d {

// ═══════════════════════════════════════════════════════════
//  sTRANSFORM_STATE
// ═══════════════════════════════════════════════════════════
sTRANSFORM_STATE::sTRANSFORM_STATE()
    : aPosition{0.0f, 0.0f, 0.0f}, aRotation(), aScale{1.0f, 1.0f, 1.0f} {}

sTRANSFORM_STATE captureTransform(const cMODEL &model) {
    sTRANSFORM_STATE state;
    state.aPosition = model.aPosition;
    state.aRotation = model.aRotation;
    state.aScale = model.aScale;
    return state;
}

void applyTransform(const sTRANSFORM_STATE &state, cMODEL *model) {
    if(model == 0) return;
    // 三个 setter 各自会调 UpdateTransform()，所以最后一次调用之后矩阵是正确的。
    // 分开调而不是直接写字段：让 cMODEL 保持"任何状态变化都经过 setter"的约定
    // （将来 setter 里加了物理同步/脏标记，这里自动获益）。
    model->SetPosition(state.aPosition.x, state.aPosition.y, state.aPosition.z);
    model->SetRotation(state.aRotation);
    model->SetScale(state.aScale.x, state.aScale.y, state.aScale.z);
}

bool transformEquals(const sTRANSFORM_STATE &lhs, const sTRANSFORM_STATE &rhs) {
    // **逐位相等**而不是容差比较：撤销的语义是"精确回到原值"。
    // 用容差会让"拖了 1e-7 m"的操作进历史（或反过来被吞掉），两种都错。
    return lhs.aPosition.x == rhs.aPosition.x && lhs.aPosition.y == rhs.aPosition.y &&
           lhs.aPosition.z == rhs.aPosition.z && lhs.aRotation.x == rhs.aRotation.x &&
           lhs.aRotation.y == rhs.aRotation.y && lhs.aRotation.z == rhs.aRotation.z &&
           lhs.aRotation.w == rhs.aRotation.w && lhs.aScale.x == rhs.aScale.x &&
           lhs.aScale.y == rhs.aScale.y && lhs.aScale.z == rhs.aScale.z;
}

// ═══════════════════════════════════════════════════════════
//  sEDIT_COMMAND
// ═══════════════════════════════════════════════════════════
sEDIT_COMMAND::sEDIT_COMMAND() : aTarget(0) { aLabel[0] = '\0'; }

// ═══════════════════════════════════════════════════════════
//  cUNDO_STACK
// ═══════════════════════════════════════════════════════════
cUNDO_STACK::cUNDO_STACK() : aCapacity(64) {}

void cUNDO_STACK::SetCapacity(uint32_t capacity) {
    if(capacity == 0) capacity = 1;
    aCapacity = capacity;
    // 缩小时从最老的丢
    while(aUndo.size() > aCapacity) aUndo.erase(aUndo.begin());
    while(aRedo.size() > aCapacity) aRedo.erase(aRedo.begin());
}

uint32_t cUNDO_STACK::Capacity() const { return aCapacity; }

bool cUNDO_STACK::Push(const sEDIT_COMMAND &command) {
    if(command.aTarget == 0) return false; // 撤销时会崩
    if(transformEquals(command.aBefore, command.aAfter)) return false; // 无变化不进历史

    // 先做校验再清 redo：被拒绝的命令不该毁掉用户的 redo 历史
    aRedo.clear();
    aUndo.push_back(command);
    while(aUndo.size() > aCapacity) aUndo.erase(aUndo.begin()); // 丢最老的
    return true;
}

bool cUNDO_STACK::PushTransform(cMODEL *target, const sTRANSFORM_STATE &before,
                                const char *label) {
    if(target == 0) return false;
    sEDIT_COMMAND command;
    command.aTarget = target;
    command.aBefore = before;
    command.aAfter = captureTransform(*target);
    if(label != 0) {
        // 保证截断后仍以 '\0' 结尾
        std::strncpy(command.aLabel, label, sizeof(command.aLabel) - 1u);
        command.aLabel[sizeof(command.aLabel) - 1u] = '\0';
    }
    return Push(command);
}

bool cUNDO_STACK::Undo() {
    if(aUndo.empty()) return false;
    sEDIT_COMMAND command = aUndo.back();
    aUndo.pop_back();
    applyTransform(command.aBefore, command.aTarget);
    aRedo.push_back(command);
    return true;
}

bool cUNDO_STACK::Redo() {
    if(aRedo.empty()) return false;
    sEDIT_COMMAND command = aRedo.back();
    aRedo.pop_back();
    applyTransform(command.aAfter, command.aTarget);
    aUndo.push_back(command);
    return true;
}

bool cUNDO_STACK::CanUndo() const { return !aUndo.empty(); }

bool cUNDO_STACK::CanRedo() const { return !aRedo.empty(); }

uint32_t cUNDO_STACK::UndoDepth() const { return (uint32_t)aUndo.size(); }

uint32_t cUNDO_STACK::RedoDepth() const { return (uint32_t)aRedo.size(); }

void cUNDO_STACK::Clear() {
    aUndo.clear();
    aRedo.clear();
}

const sEDIT_COMMAND *cUNDO_STACK::PeekUndo() const {
    if(aUndo.empty()) return 0;
    return &aUndo.back();
}

const sEDIT_COMMAND *cUNDO_STACK::PeekRedo() const {
    if(aRedo.empty()) return 0;
    return &aRedo.back();
}

} // namespace GN3d
