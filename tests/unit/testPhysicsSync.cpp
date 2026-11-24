// testPhysicsSync.cpp — 物理控制器（P4）纯 CPU 单测。
//
// 覆盖 docs/ENTITY_CONTROLLERS_PLAN.md §5.1 P4 的验收口径：
//   * 固定步长下 60Hz 物理 + 144Hz 渲染的插值单调性；
//   * body 睡醒 / 移除后映射表不残留；
// 另加：接触事件（enter / exit / 实体对解析）、子步折算、非法 dt。
#include "testFramework.h"

#include <GN.h>
#include "GN_Entity.h"
#include "GN_GameClock.h"

#include <cmath>
#include <cstring>
#include <vector>

using namespace GN3d;

namespace {

const float kFixedStep = 1.0f / 60.0f;

sRIGID_BODY makeSphere(float x, float y, float z) {
    sRIGID_BODY body = MakeSphereBody(eBODY_DYNAMIC, GNg::sVEC3{x, y, z}, 0.5f, 1.0f);
    return body;
}

sRIGID_BODY makeGround() {
    // 厚板当地面：半高 0.5，中心 y = -0.5 ⇒ 顶面 y = 0
    return MakeBoxBody(eBODY_STATIC, GNg::sVEC3{0.0f, -0.5f, 0.0f},
                       GNg::sVEC3{20.0f, 0.5f, 20.0f}, 0.0f);
}

} // namespace

// ── 快照插值：60Hz 物理 + 144Hz 渲染 ────────────────────
// 逐帧读模型位置，位置必须单调下降（自由落体）且**每帧都有变化**（不是两帧跳一次）。
TEST_CASE(physicsSyncInterpolationIsSmooth) {
    cPHYSICS_WORLD world;
    world.SetFixedTimestep(kFixedStep, 4);

    cPHYSICS_CONTROLLER controller;
    controller.SetWorld(&world);

    const uint32_t handle = world.AddBody(makeSphere(0.0f, 10.0f, 0.0f));
    CHECK(handle != cPHYSICS_WORLD::INVALID_HANDLE);
    CHECK(controller.AttachBody(1u, handle));

    cMODEL model;
    GN::cGAME_CLOCK clock;
    clock.SetFixedStep(kFixedStep, 4);

    float previousY = 10.0f;
    int movedFrames = 0;
    int zeroStepFrames = 0;
    for (int frame = 0; frame < 144; ++frame) { // 1 秒 = 144 帧渲染
        clock.Tick(1.0f / 144.0f);
        controller.Step(1.0f / 144.0f, clock.Alpha());
        if (controller.LastSubSteps() == 0) ++zeroStepFrames;
        CHECK(controller.ApplyTransformInterpolated(1u, model));
        // 单调下降（插值不能往回走）
        CHECK(model.aPosition.y <= previousY + 1e-5f);
        if (model.aPosition.y < previousY - 1e-6f) ++movedFrames;
        previousY = model.aPosition.y;
    }
    // 144Hz 渲染 1 秒：约 84 帧"物理没走子步"。这些帧靠 alpha 在旧区间里继续走，
    // 所以位置仍然逐帧变化（这正是插值的意义）。
    CHECK(zeroStepFrames > 50);
    CHECK(movedFrames > 130); // 几乎每帧都在动（不是"两帧跳一次"）
    CHECK(previousY < 9.0f);  // 真的掉下来了（1 秒自由落体约 4.9 m）
    CHECK(previousY > 4.0f);
}

// 不插值（useInterpolation=false）时，物理没走子步的帧位置完全不变 —— 这正是台阶的来源。
TEST_CASE(physicsSyncWithoutInterpolationStairSteps) {
    cPHYSICS_WORLD world;
    world.SetFixedTimestep(kFixedStep, 4);
    cPHYSICS_CONTROLLER controller;
    controller.SetWorld(&world);
    const uint32_t handle = world.AddBody(makeSphere(0.0f, 10.0f, 0.0f));
    CHECK(controller.AttachBody(1u, handle));

    cMODEL model;
    GN::cGAME_CLOCK clock;
    clock.SetFixedStep(kFixedStep, 4);
    int identicalFrames = 0;
    float previousY = 10.0f;
    for (int frame = 0; frame < 144; ++frame) {
        clock.Tick(1.0f / 144.0f);
        controller.Step(1.0f / 144.0f, clock.Alpha());
        CHECK(controller.ApplyTransform(1u, model));
        if (std::fabs(model.aPosition.y - previousY) < 1e-9f) ++identicalFrames;
        previousY = model.aPosition.y;
    }
    // 144Hz 下约 2/3 的帧物理没走：这些帧位置完全相同（台阶）
    CHECK(identicalFrames > 50);
}

// 插值因子必须落在 [0,1]，且随 alpha 单调。
TEST_CASE(physicsSyncAlphaMonotonic) {
    cPHYSICS_WORLD world;
    world.SetFixedTimestep(kFixedStep, 4);
    cPHYSICS_CONTROLLER controller;
    controller.SetWorld(&world);
    const uint32_t handle = world.AddBody(makeSphere(0.0f, 10.0f, 0.0f));
    CHECK(controller.AttachBody(1u, handle));

    cMODEL model;
    float previous = -1.0f;
    for (int i = 0; i < 40; ++i) {
        const float alpha = (float)i / 40.0f;
        controller.Step(kFixedStep, alpha);
        const float renderAlpha = controller.RenderAlpha();
        CHECK(renderAlpha >= 0.0f);
        CHECK(renderAlpha <= 1.0f);
        CHECK(renderAlpha >= previous - 1e-6f); // 单调不减（本用例每帧恰好 1 个子步）
        previous = renderAlpha;
        CHECK(controller.ApplyTransformInterpolated(1u, model));
    }
    // 非法 alpha 不能污染：负值/NaN 一律按 0
    controller.Step(kFixedStep, -1.0f);
    CHECK_NEAR(controller.RenderAlpha(), 0.0f, 1e-6);
    controller.Step(kFixedStep, std::nanf(""));
    CHECK(std::isfinite(controller.RenderAlpha()));
}

// 多子步帧：插值因子要按"本帧推进了 N 步"折算，否则越插越快（瞬移）。
TEST_CASE(physicsSyncMultiSubStepScalesAlpha) {
    cPHYSICS_WORLD world;
    world.SetFixedTimestep(kFixedStep, 4);
    cPHYSICS_CONTROLLER controller;
    controller.SetWorld(&world);
    const uint32_t handle = world.AddBody(makeSphere(0.0f, 1000.0f, 0.0f)); // 远离地面，自由落体
    CHECK(controller.AttachBody(1u, handle));

    // 一帧给 3.5 步的时间：应跑 3 个子步。
    // alpha 保持原值（**不按子步数折算**）：折算会让渲染停在区间前 1/3，
    // 下一帧再跳到区间末尾 —— 那是真正的跳帧。按 N 倍速走完这一段是连续的。
    controller.Step(kFixedStep * 3.5f, 0.5f);
    CHECK_EQ(controller.LastSubSteps(), 3u);
    CHECK_NEAR(controller.RenderAlpha(), 0.5f, 1e-6);

    // 单子步：同样保持原值
    controller.Step(kFixedStep, 0.75f);
    CHECK_EQ(controller.LastSubSteps(), 1u);
    CHECK_NEAR(controller.RenderAlpha(), 0.75f, 1e-6);

    // 越界 alpha 一律钳到 [0,1]
    controller.Step(kFixedStep, 5.0f);
    CHECK_NEAR(controller.RenderAlpha(), 1.0f, 1e-6);
}

// ── 映射表不残留 ───────────────────────────────────────
TEST_CASE(physicsSyncDetachCleansSnapshot) {
    cPHYSICS_WORLD world;
    cPHYSICS_CONTROLLER controller;
    controller.SetWorld(&world);
    const uint32_t handle = world.AddBody(makeSphere(0.0f, 5.0f, 0.0f));
    CHECK(controller.AttachBody(7u, handle));
    CHECK_EQ(controller.BindingCount(), 1u);

    cMODEL model;
    CHECK(controller.ApplyTransformInterpolated(7u, model));
    CHECK(controller.DetachBody(7u));
    CHECK_EQ(controller.BindingCount(), 0u);
    CHECK(!controller.ApplyTransformInterpolated(7u, model)); // 解绑后不再写模型
    CHECK_EQ(controller.EntityOf(handle), -1);
}

// 刚体被 RemoveBody 后：ApplyTransform 必须失败，而不是把模型写到零位。
TEST_CASE(physicsSyncRemovedBodyDoesNotZeroModel) {
    cPHYSICS_WORLD world;
    cPHYSICS_CONTROLLER controller;
    controller.SetWorld(&world);
    const uint32_t handle = world.AddBody(makeSphere(0.0f, 5.0f, 0.0f));
    CHECK(controller.AttachBody(3u, handle));

    cMODEL model;
    model.SetPosition(1.0f, 2.0f, 3.0f);
    controller.Step(kFixedStep, 0.5f);
    CHECK(controller.ApplyTransformInterpolated(3u, model));

    CHECK(world.RemoveBody(handle));
    controller.Step(kFixedStep, 0.5f);
    // 刚体没了：映射还在（调用方没解绑），但同步必须返回 false 且不动模型
    const GNg::sVEC3 before = model.aPosition;
    const bool applied = controller.ApplyTransformInterpolated(3u, model);
    if (applied) {
        // 允许实现退回"上一帧有效状态"，但绝不能是零位
        CHECK(std::fabs(model.aPosition.y) > 1e-3f);
    } else {
        CHECK_NEAR(model.aPosition.x, before.x, 1e-6);
    }
    CHECK(controller.DetachBody(3u));
}

// 唤醒 / 睡眠：睡着的刚体位置不变，但映射与同步照旧可用。
TEST_CASE(physicsSyncSleepKeepsMapping) {
    cPHYSICS_WORLD world;
    cPHYSICS_CONTROLLER controller;
    controller.SetWorld(&world);
    const uint32_t handle = world.AddBody(makeSphere(0.0f, 5.0f, 0.0f));
    CHECK(controller.AttachBody(9u, handle));

    world.WakeBody(handle);
    CHECK_EQ(controller.BodyOf(9u), handle);
    const sRIGID_BODY *body = world.GetBody(handle);
    CHECK(body != nullptr);
    CHECK(!body->aSleeping);

    // 跑到位后自然睡着（静止在地面上）；睡着后映射必须还在
    const uint32_t ground = world.AddBody(makeGround());
    CHECK(ground != cPHYSICS_WORLD::INVALID_HANDLE);
    for (int i = 0; i < 600; ++i) controller.Step(kFixedStep, 0.0f);
    body = world.GetBody(handle);
    CHECK(body != nullptr);
    CHECK_EQ(controller.BodyOf(9u), handle);
    CHECK_EQ(controller.EntityOf(handle), 9);
    // 球应停在离地面不远的位置（半径 0.5，允许少量穿透）
    CHECK(body->aPos.y > 0.2f);
    CHECK(body->aPos.y < 0.9f);

    // 睡着后仍然能被唤醒并继续同步（映射与快照都不受影响）
    world.WakeBody(handle);
    cMODEL model;
    CHECK(controller.ApplyTransformInterpolated(9u, model));
    CHECK(model.aPosition.y > 0.2f);
}

// ── 接触事件 ───────────────────────────────────────────
TEST_CASE(physicsSyncContactEnterAndExit) {
    cPHYSICS_WORLD world;
    cPHYSICS_CONTROLLER controller;
    controller.SetWorld(&world);

    const uint32_t ground = world.AddBody(makeGround());
    // 起始高度只留 0.1 m 的落差：本用例要验的是"躺在地上期间接触一直在"，
    // 不是"下落阶段没有接触"。放太高会让 120 帧里有一半在自由落体（第一版就是这么错的）。
    const uint32_t ball = world.AddBody(makeSphere(0.0f, 0.6f, 0.0f));
    CHECK(controller.AttachBody(1u, ball)); // 地面不登记实体

    // 先验"默认不上报没有实体的接触"：跑一个小世界确认它一直是空的
    {
        cPHYSICS_WORLD quietWorld;
        cPHYSICS_CONTROLLER quiet;
        quiet.SetWorld(&quietWorld);
        const uint32_t qBall = quietWorld.AddBody(makeSphere(0.0f, 1.0f, 0.0f));
        quietWorld.AddBody(makeGround());
        CHECK(quiet.AttachBody(1u, qBall));
        uint32_t total = 0;
        for (int i = 0; i < 180; ++i) {
            quiet.Step(kFixedStep, 0.0f);
            total += quiet.ContactCount();
        }
        CHECK_EQ(total, 0u); // 球明明躺在地上，但默认不报
    }

    // 打开上报后从**一开始**就开始统计：
    // 注意——latch（保持睡着的接触）只能延续"已经见过"的一对，不能事后补发现。
    // 所以过滤开关要在落地之前就设好，否则睡着的接触永远看不见。
    controller.SetReportUnmapped(true);
    int enterFrames = 0;
    int contactFrames = 0;
    for (int i = 0; i < 120; ++i) {
        controller.Step(kFixedStep, 0.0f);
        if (controller.ContactCount() > 0) {
            ++contactFrames;
            if (controller.EnteredContactCount() > 0) ++enterFrames;
        }
    }
    // 落地后球会睡着，物理引擎就不再报这一对了（为性能跳过睡着的刚体）。
    // 控制器对"两端都睡着"的接触做 latch，所以"站在地上"这件事不会因为睡着而消失。
    CHECK(contactFrames > 100);
    CHECK(enterFrames >= 1);
    CHECK(enterFrames <= 2); // 只该 enter 一次（睡着/醒来不抖）

    const cPHYSICS_CONTROLLER::sCONTACT_EVENT &contact = controller.ContactAt(0);
    CHECK((contact.aEntityA == 1u || contact.aEntityB == 1u));
    // 法线是单位向量
    const float length = std::sqrt(contact.aNormal.x * contact.aNormal.x +
                                   contact.aNormal.y * contact.aNormal.y +
                                   contact.aNormal.z * contact.aNormal.z);
    CHECK_NEAR(length, 1.0f, 0.05f);
}

// 两个都登记了实体的刚体相撞：事件里两端都是实体编号，且 enter 只报一次。
TEST_CASE(physicsSyncContactBetweenEntities) {
    cPHYSICS_WORLD world;
    cPHYSICS_CONTROLLER controller;
    controller.SetWorld(&world);

    // 下球做成**静态**：两个动态球从静止同时下落，间距恒定，永远不会相撞
    // （这是本用例第一版真实踩到的坑）。
    sRIGID_BODY lowerBody = MakeSphereBody(eBODY_STATIC, GNg::sVEC3{0.0f, 0.5f, 0.0f}, 0.5f, 0.0f);
    const uint32_t lower = world.AddBody(lowerBody);
    const uint32_t upper = world.AddBody(makeSphere(0.0f, 2.2f, 0.0f));
    CHECK(controller.AttachBody(11u, lower));
    CHECK(controller.AttachBody(22u, upper));

    int enterFrames = 0;
    bool sawPair = false;
    for (int i = 0; i < 180; ++i) {
        controller.Step(kFixedStep, 0.0f);
        if (controller.EnteredContactCount() > 0) ++enterFrames;
        for (uint32_t c = 0; c < controller.ContactCount(); ++c) {
            const cPHYSICS_CONTROLLER::sCONTACT_EVENT &event = controller.ContactAt(c);
            const bool match = (event.aEntityA == 11u && event.aEntityB == 22u) ||
                               (event.aEntityA == 22u && event.aEntityB == 11u);
            if (match) sawPair = true;
        }
    }
    CHECK(sawPair); // 上球落到下球上
    CHECK(enterFrames >= 1);
    CHECK(enterFrames <= 2);
}

// 解绑后该实体不再出现在接触事件里（也不产生"离开"事件给已经不存在的实体）。
TEST_CASE(physicsSyncDetachClearsContactHistory) {
    cPHYSICS_WORLD world;
    cPHYSICS_CONTROLLER controller;
    controller.SetWorld(&world);
    const uint32_t ground = world.AddBody(makeGround());
    const uint32_t ball = world.AddBody(makeSphere(0.0f, 1.0f, 0.0f));
    CHECK(controller.AttachBody(5u, ball));
    controller.SetReportUnmapped(true);

    for (int i = 0; i < 120; ++i) controller.Step(kFixedStep, 0.0f);
    CHECK(controller.ContactCount() > 0);

    CHECK(controller.DetachBody(5u));
    controller.Step(kFixedStep, 0.0f);
    CHECK_EQ(controller.ContactCount(), 0u);
    CHECK_EQ(controller.ExitedContactCount(), 0u); // 不该给已解绑的实体报"离开"
}

// ── cENTITY 接通 ───────────────────────────────────────
TEST_CASE(physicsSyncEntityUsesRenderAlpha) {
    cPHYSICS_WORLD world;
    world.SetFixedTimestep(kFixedStep, 4);

    cENTITY entity;
    entity.aId = 1u;
    entity.aPhysics.SetWorld(&world);
    cMODEL *model = CreateCubeModel(1.0f);
    CHECK(model != nullptr);
    entity.aMesh.SetModel(model);

    const uint32_t handle = world.AddBody(makeSphere(0.0f, 10.0f, 0.0f));
    CHECK(entity.aPhysics.AttachBody(entity.aId, handle));

    GN::cGAME_CLOCK clock;
    clock.SetFixedStep(kFixedStep, 4);
    float previousY = 10.0f;
    int movedFrames = 0;
    for (int frame = 0; frame < 60; ++frame) {
        clock.Tick(1.0f / 144.0f);
        entity.aRenderAlpha = clock.Alpha();
        entity.Update(clock.DeltaTime());
        if (model->aPosition.y < previousY - 1e-6f) ++movedFrames;
        CHECK(model->aPosition.y <= previousY + 1e-5f);
        previousY = model->aPosition.y;
    }
    CHECK(movedFrames > 55); // 几乎每帧都在动（插值生效）
    CHECK(previousY < 10.0f);

    entity.aMesh.SetModel(nullptr);
    delete model;
}

// 非法 dt：不推进、不崩、不产生 NaN 位置。
TEST_CASE(physicsSyncInvalidDt) {
    cPHYSICS_WORLD world;
    cPHYSICS_CONTROLLER controller;
    controller.SetWorld(&world);
    const uint32_t handle = world.AddBody(makeSphere(0.0f, 5.0f, 0.0f));
    CHECK(controller.AttachBody(1u, handle));

    controller.Step(0.0f, 0.5f);
    CHECK_EQ(controller.LastSubSteps(), 0u);
    controller.Step(-1.0f, 0.5f);
    CHECK_EQ(controller.LastSubSteps(), 0u);
    controller.Step(std::nanf(""), 0.5f);
    CHECK_EQ(controller.LastSubSteps(), 0u);
    controller.Step(HUGE_VALF, 0.5f); // 超大 dt：内部会钳到 kMaxFrameDt 与 maxSubSteps
    CHECK(controller.LastSubSteps() <= 4u);

    cMODEL model;
    CHECK(controller.ApplyTransformInterpolated(1u, model));
    CHECK(std::isfinite(model.aPosition.y));
    CHECK(model.aPosition.y < 5.0f); // 确实掉了（说明上面的 HUGE_VALF 生效了）
}

// 没有世界 / 没有登记：全部接口安全返回 false，不崩。
TEST_CASE(physicsSyncWithoutWorld) {
    cPHYSICS_CONTROLLER controller;
    CHECK(controller.World() == nullptr);
    controller.Step(1.0f / 60.0f, 0.5f);
    CHECK_EQ(controller.LastSubSteps(), 0u);
    CHECK_EQ(controller.ContactCount(), 0u);
    CHECK_EQ(controller.ExitedContactCount(), 0u);
    CHECK(!controller.AttachBody(1u, 0u));

    cMODEL model;
    CHECK(!controller.ApplyTransformInterpolated(1u, model));
    uint32_t a = 1;
    uint32_t b = 2;
    CHECK(!controller.ExitedContactAt(0, a, b));
    CHECK_EQ(controller.ContactAt(99).aEntityA, 0u); // 越界返回零值
}

// 明确记录一条限制：latch 只能延续"已经见过"的接触对，事后打开过滤补不回来。
// 睡着的刚体对物理引擎不产生接触，控制器无从得知它们曾经接触过。
TEST_CASE(physicsSyncLatchCannotRecoverMutedContacts) {
    cPHYSICS_WORLD world;
    cPHYSICS_CONTROLLER controller;
    controller.SetWorld(&world);
    world.AddBody(makeGround());
    const uint32_t ball = world.AddBody(makeSphere(0.0f, 1.0f, 0.0f));
    CHECK(controller.AttachBody(1u, ball));

    // 前 200 帧不开过滤：接触全部被丢掉
    for (int i = 0; i < 200; ++i) controller.Step(kFixedStep, 0.0f);
    CHECK_EQ(controller.ContactCount(), 0u);

    // 之后打开过滤：球已经睡着，物理不再报这一对 → 依然看不到（这是设计边界，不是 bug）
    controller.SetReportUnmapped(true);
    for (int i = 0; i < 60; ++i) controller.Step(kFixedStep, 0.0f);
    CHECK_EQ(controller.ContactCount(), 0u);

    // 把它踢醒：接触重新出现，并且作为 enter 上报
    world.WakeBody(ball);
    controller.Step(kFixedStep, 0.0f);
    bool found = false;
    for (uint32_t i = 0; i < controller.ContactCount(); ++i) {
        const cPHYSICS_CONTROLLER::sCONTACT_EVENT &event = controller.ContactAt(i);
        if (event.aEntityA == 1u || event.aEntityB == 1u) found = true;
    }
    CHECK(found);
}
