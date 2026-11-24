// testEditCommand.cpp — 编辑命令与撤销栈（A4）+ 场景选择集（A3）纯 CPU 单测。
//
// 这两块都是"编辑器没有就不能用"的东西，且完全不需要 GPU：
//   * 撤销栈：快照式命令、redo 清空规则、容量裁剪、无变化命令拒绝；
//   * 选择集：单选/Ctrl 切换、主选中、实体移出场景时同步清理。
#include "testFramework.h"

#include <GN.h>
#include "GN_EditCommand.h"
#include "GN_Scene.h"

#include <cmath>
#include <cstring>
#include <vector>

using namespace GN3d;

namespace {

cMODEL *makeCube(float x, float y, float z) {
    cMODEL *model = CreateCubeModel(1.0f);
    model->SetPosition(x, y, z);
    model->computeWorldAABB();
    return model;
}

} // namespace

// ── 变换快照 ───────────────────────────────────────────
TEST_CASE(editTransformCaptureApply) {
    cMODEL *model = CreateCubeModel(1.0f);
    model->SetPosition(1.0f, 2.0f, 3.0f);
    model->SetScale(2.0f, 3.0f, 4.0f);

    const sTRANSFORM_STATE captured = captureTransform(*model);
    CHECK_NEAR(captured.aPosition.x, 1.0f, 1e-6);
    CHECK_NEAR(captured.aPosition.y, 2.0f, 1e-6);
    CHECK_NEAR(captured.aScale.y, 3.0f, 1e-6);

    // 改掉，再写回
    model->SetPosition(9.0f, 9.0f, 9.0f);
    model->SetScale(1.0f, 1.0f, 1.0f);
    applyTransform(captured, model);
    CHECK_NEAR(model->aPosition.x, 1.0f, 1e-6);
    CHECK_NEAR(model->aScale.z, 4.0f, 1e-6);
    // 写回后矩阵必须被重建（否则渲染用的是旧矩阵）
    CHECK_NEAR(model->aTransform.m[12], 1.0f, 1e-6);

    // 空指针安全
    applyTransform(captured, nullptr);
    delete model;
}

TEST_CASE(editTransformEquals) {
    sTRANSFORM_STATE a;
    sTRANSFORM_STATE b;
    CHECK(transformEquals(a, b)); // 默认值相等
    b.aPosition.x = 1e-7f;
    // **逐位**比较：1e-7 的差别也算不同（撤销要精确回到原值，不是"差不多"）
    CHECK(!transformEquals(a, b));
    b.aPosition.x = 0.0f;
    CHECK(transformEquals(a, b));
    b.aScale.y = 2.0f;
    CHECK(!transformEquals(a, b));
}

// ── 撤销栈基础 ─────────────────────────────────────────
TEST_CASE(undoStackPushUndoRedo) {
    cMODEL *model = makeCube(0.0f, 0.0f, 0.0f);
    cUNDO_STACK stack;
    CHECK(!stack.CanUndo());
    CHECK(!stack.CanRedo());
    CHECK_EQ(stack.UndoDepth(), 0u);

    // 记录起始位置，移动，压栈
    const sTRANSFORM_STATE before = captureTransform(*model);
    model->SetPosition(5.0f, 0.0f, 0.0f);
    model->computeWorldAABB();
    CHECK(stack.PushTransform(model, before, "move"));
    CHECK(stack.CanUndo());
    CHECK_EQ(stack.UndoDepth(), 1u);

    // 撤销 → 回到 0
    CHECK(stack.Undo());
    CHECK_NEAR(model->aPosition.x, 0.0f, 1e-6);
    CHECK(stack.CanRedo());
    CHECK(!stack.CanUndo());

    // 重做 → 回到 5
    CHECK(stack.Redo());
    CHECK_NEAR(model->aPosition.x, 5.0f, 1e-6);
    CHECK(stack.CanUndo());
    CHECK(!stack.CanRedo());

    delete model;
}

TEST_CASE(undoStackRejectsNoChangeCommand) {
    cMODEL *model = makeCube(0.0f, 0.0f, 0.0f);
    cUNDO_STACK stack;
    // 位置没变 ⇒ 不该进历史（否则按十次撤销才退一步）
    const sTRANSFORM_STATE before = captureTransform(*model);
    CHECK(!stack.PushTransform(model, before, "noop"));
    CHECK_EQ(stack.UndoDepth(), 0u);

    // 之后真的动了：能进
    model->SetPosition(1.0f, 0.0f, 0.0f);
    CHECK(stack.PushTransform(model, before, "move"));
    CHECK_EQ(stack.UndoDepth(), 1u);
    delete model;
}

TEST_CASE(undoStackRejectsNullTarget) {
    cUNDO_STACK stack;
    sEDIT_COMMAND command;
    command.aTarget = nullptr;
    command.aBefore.aPosition.x = 0.0f;
    command.aAfter.aPosition.x = 1.0f;
    CHECK(!stack.Push(command)); // 撤销时会崩
    CHECK_EQ(stack.UndoDepth(), 0u);
    CHECK(!stack.PushTransform(nullptr, sTRANSFORM_STATE(), "x"));
}

TEST_CASE(undoStackEmptyOperations) {
    cUNDO_STACK stack;
    CHECK(!stack.Undo()); // 空栈撤销返回 false，不崩
    CHECK(!stack.Redo());
    CHECK(stack.PeekUndo() == nullptr);
    CHECK(stack.PeekRedo() == nullptr);
}

// Push 新命令必须清空 redo 栈（标准的线性历史）
TEST_CASE(undoStackPushClearsRedo) {
    cMODEL *model = makeCube(0.0f, 0.0f, 0.0f);
    cUNDO_STACK stack;

    sTRANSFORM_STATE before = captureTransform(*model);
    model->SetPosition(1.0f, 0.0f, 0.0f);
    stack.PushTransform(model, before, "a");

    before = captureTransform(*model);
    model->SetPosition(2.0f, 0.0f, 0.0f);
    stack.PushTransform(model, before, "b");
    CHECK_EQ(stack.UndoDepth(), 2u);

    CHECK(stack.Undo()); // 回到 1
    CHECK_EQ(stack.RedoDepth(), 1u);

    // 现在做一个新操作：redo 历史必须被清掉（否则会出现两条历史链打架）
    before = captureTransform(*model);
    model->SetPosition(7.0f, 0.0f, 0.0f);
    CHECK(stack.PushTransform(model, before, "c"));
    CHECK_EQ(stack.RedoDepth(), 0u);
    CHECK(!stack.CanRedo());

    delete model;
}

// **被拒绝**的命令不该毁掉 redo 历史（先校验再清）
TEST_CASE(undoStackRejectedCommandKeepsRedo) {
    cMODEL *model = makeCube(0.0f, 0.0f, 0.0f);
    cUNDO_STACK stack;
    sTRANSFORM_STATE before = captureTransform(*model);
    model->SetPosition(3.0f, 0.0f, 0.0f);
    stack.PushTransform(model, before, "a");
    CHECK(stack.Undo());
    CHECK_EQ(stack.RedoDepth(), 1u);

    // 空命令：被拒绝
    const sTRANSFORM_STATE same = captureTransform(*model);
    CHECK(!stack.PushTransform(model, same, "noop"));
    CHECK_EQ(stack.RedoDepth(), 1u); // redo 还在

    delete model;
}

TEST_CASE(undoStackCapacityTrimsOldest) {
    cMODEL *model = makeCube(0.0f, 0.0f, 0.0f);
    cUNDO_STACK stack;
    stack.SetCapacity(3);

    for (int i = 1; i <= 5; ++i) {
        const sTRANSFORM_STATE before = captureTransform(*model);
        model->SetPosition((float)i, 0.0f, 0.0f);
        stack.PushTransform(model, before, "step");
    }
    // 只保留最近 3 条（位置 2→3、3→4、4→5）
    CHECK_EQ(stack.UndoDepth(), 3u);

    // 撤销三次回到位置 2（不是 0 —— 最早的记录已经被丢掉了）
    CHECK(stack.Undo());
    CHECK(stack.Undo());
    CHECK(stack.Undo());
    CHECK_NEAR(model->aPosition.x, 2.0f, 1e-6);
    CHECK(!stack.CanUndo());

    // 容量 0 会被钳到 1
    stack.SetCapacity(0);
    CHECK_EQ(stack.Capacity(), 1u);
    delete model;
}

TEST_CASE(undoStackLabelTruncation) {
    cMODEL *model = makeCube(0.0f, 0.0f, 0.0f);
    cUNDO_STACK stack;
    const sTRANSFORM_STATE before = captureTransform(*model);
    model->SetPosition(1.0f, 0.0f, 0.0f);
    // 超长标签必须被安全截断且以 '\0' 结尾
    CHECK(stack.PushTransform(model, before,
                              "这是一个非常非常非常非常长的操作名称，超过缓冲区长度"));
    const sEDIT_COMMAND *top = stack.PeekUndo();
    CHECK(top != nullptr);
    CHECK(std::strlen(top->aLabel) < sizeof(top->aLabel));
    delete model;
}

// 多对象、交叉撤销：每条命令记住自己的目标
TEST_CASE(undoStackMultipleTargets) {
    cMODEL *a = makeCube(0.0f, 0.0f, 0.0f);
    cMODEL *b = makeCube(0.0f, 5.0f, 0.0f);
    cUNDO_STACK stack;

    sTRANSFORM_STATE beforeA = captureTransform(*a);
    a->SetPosition(1.0f, 0.0f, 0.0f);
    stack.PushTransform(a, beforeA, "moveA");

    sTRANSFORM_STATE beforeB = captureTransform(*b);
    b->SetPosition(1.0f, 5.0f, 0.0f);
    stack.PushTransform(b, beforeB, "moveB");

    // 撤销两次：b 先回退，再 a
    CHECK(stack.Undo());
    CHECK_NEAR(b->aPosition.x, 0.0f, 1e-6);
    CHECK_NEAR(a->aPosition.x, 1.0f, 1e-6); // a 还没动
    CHECK(stack.Undo());
    CHECK_NEAR(a->aPosition.x, 0.0f, 1e-6);

    delete a;
    delete b;
}

TEST_CASE(undoStackClear) {
    cMODEL *model = makeCube(0.0f, 0.0f, 0.0f);
    cUNDO_STACK stack;
    const sTRANSFORM_STATE before = captureTransform(*model);
    model->SetPosition(1.0f, 0.0f, 0.0f);
    stack.PushTransform(model, before, "a");
    stack.Undo();
    CHECK(stack.CanRedo());
    stack.Clear();
    CHECK(!stack.CanUndo());
    CHECK(!stack.CanRedo());
    delete model;
}

// ── 选择集（A3）────────────────────────────────────────
namespace {

struct sSceneFixture {
    cSCENE aScene;
    std::vector<cENTITY *> aEntities;
    std::vector<cMODEL *> aModels;

    sSceneFixture() {}
    ~sSceneFixture() { clear(); }

    cENTITY *add(float x) {
        cMODEL *model = makeCube(x, 0.0f, 0.0f);
        cENTITY *entity = new cENTITY();
        entity->aMesh.SetModel(model);
        aScene.Add(entity);
        aEntities.push_back(entity);
        aModels.push_back(model);
        return entity;
    }
    void clear() {
        for (size_t i = 0; i < aEntities.size(); ++i) {
            aEntities[i]->aMesh.SetModel(nullptr);
            delete aEntities[i];
        }
        for (size_t i = 0; i < aModels.size(); ++i) delete aModels[i];
        aEntities.clear();
        aModels.clear();
    }
};

} // namespace

TEST_CASE(selectionSingleSelect) {
    sSceneFixture fixture;
    cENTITY *a = fixture.add(0.0f);
    cENTITY *b = fixture.add(2.0f);
    CHECK(!fixture.aScene.Select(a) == false); // 返回 true
    CHECK_EQ(fixture.aScene.SelectionCount(), 1u);
    CHECK(fixture.aScene.IsSelected(a));
    CHECK(fixture.aScene.Primary() == a);

    // 单选语义：选 b 会清掉 a
    CHECK(fixture.aScene.Select(b));
    CHECK_EQ(fixture.aScene.SelectionCount(), 1u);
    CHECK(!fixture.aScene.IsSelected(a));
    CHECK(fixture.aScene.Primary() == b);

    // 不在场景里的实体：拒绝
    cENTITY stranger;
    CHECK(!fixture.aScene.Select(&stranger));
    CHECK(fixture.aScene.Primary() == b); // 没被清掉
}

TEST_CASE(selectionToggleAndMulti) {
    sSceneFixture fixture;
    cENTITY *a = fixture.add(0.0f);
    cENTITY *b = fixture.add(2.0f);
    cENTITY *c = fixture.add(4.0f);

    // Ctrl+点击：逐个加入。**primary 保持在第一个被选中的对象上** ——
    // 追加不是"切换当前操作对象"（否则 Gizmo 会跳到最后一个点中的对象，
    // 而调用方手里还拿着原来那个，撤销命令的目标就错位了）。
    CHECK(fixture.aScene.Toggle(a));
    CHECK(fixture.aScene.Primary() == a);
    CHECK(fixture.aScene.Toggle(b));
    CHECK(fixture.aScene.Toggle(c));
    CHECK_EQ(fixture.aScene.SelectionCount(), 3u);
    CHECK(fixture.aScene.Primary() == a); // 仍是第一个

    // 再 Toggle 一次：取消
    CHECK(fixture.aScene.Toggle(b));
    CHECK_EQ(fixture.aScene.SelectionCount(), 2u);
    CHECK(!fixture.aScene.IsSelected(b));
    CHECK(fixture.aScene.IsSelected(a));
    CHECK(fixture.aScene.Primary() == a); // 取消一个非 primary 的，primary 不变

    // 重复加入：返回 false
    CHECK(!fixture.aScene.AddToSelection(a));
    CHECK_EQ(fixture.aScene.SelectionCount(), 2u);

    // SelectByIndex
    CHECK(fixture.aScene.SelectByIndex(1));
    CHECK(fixture.aScene.Primary() == b);
    CHECK(!fixture.aScene.SelectByIndex(99)); // 越界

    fixture.aScene.ClearSelection();
    CHECK_EQ(fixture.aScene.SelectionCount(), 0u);
    CHECK(fixture.aScene.Primary() == nullptr);
    CHECK(fixture.aScene.SelectedAt(0) == nullptr); // 越界安全
}

// 实体被移出场景时必须同步清理选择集 —— 这是"选择集放在 cSCENE 里"的主要理由。
TEST_CASE(selectionClearedWhenEntityRemoved) {
    sSceneFixture fixture;
    cENTITY *a = fixture.add(0.0f);
    cENTITY *b = fixture.add(2.0f);
    fixture.aScene.Select(a);
    fixture.aScene.AddToSelection(b);
    CHECK_EQ(fixture.aScene.SelectionCount(), 2u);
    CHECK(fixture.aScene.Primary() == a); // AddToSelection 不改 primary

    // 移除**主选中**：primary 要落到剩下的元素上（不能是悬空指针）
    CHECK(fixture.aScene.Remove(a));
    CHECK_EQ(fixture.aScene.SelectionCount(), 1u);
    CHECK(!fixture.aScene.IsSelected(a));
    CHECK(fixture.aScene.Primary() == b);

    // 移除最后一个
    CHECK(fixture.aScene.Remove(b));
    CHECK_EQ(fixture.aScene.SelectionCount(), 0u);
    CHECK(fixture.aScene.Primary() == nullptr);
    if (false) {

    }
}

TEST_CASE(selectionClearedOnSceneClear) {
    sSceneFixture fixture;
    cENTITY *a = fixture.add(0.0f);
    fixture.aScene.Select(a);
    fixture.aScene.Clear(); // 清场景
    CHECK_EQ(fixture.aScene.SelectionCount(), 0u);
    CHECK(fixture.aScene.Primary() == nullptr);
    CHECK_EQ(fixture.aScene.Count(), 0u);
}

// ── 端到端：拖拽 → 压栈 → 撤销 ─────────────────────────
// 这是两块能力的交汇点，也是它们真正被使用的方式。
TEST_CASE(undoIntegratesWithGizmoDrag) {
    cMODEL *model = makeCube(0.0f, 0.0f, 0.0f);
    cUNDO_STACK stack;

    // 模拟一次拖拽：记起始快照 → 改位置 → 压栈
    const sTRANSFORM_STATE dragStart = captureTransform(*model);
    model->SetPosition(1.5f, 0.0f, 0.0f);
    model->computeWorldAABB();
    CHECK(stack.PushTransform(model, dragStart, "移动"));

    // 再拖一次
    const sTRANSFORM_STATE dragStart2 = captureTransform(*model);
    model->SetPosition(3.0f, 0.0f, 0.0f);
    model->computeWorldAABB();
    CHECK(stack.PushTransform(model, dragStart2, "移动"));

    // 撤销两次回到原点
    CHECK(stack.Undo());
    CHECK_NEAR(model->aPosition.x, 1.5f, 1e-6);
    CHECK(stack.Undo());
    CHECK_NEAR(model->aPosition.x, 0.0f, 1e-6);

    delete model;
}
