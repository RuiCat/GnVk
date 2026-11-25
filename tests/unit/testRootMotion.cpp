// testRootMotion.cpp — Root Motion（C3），纯 CPU。
//
// 三条语义：
//   1. 提取：首帧只记录、局部位移按当前朝向旋转、转角取最短弧；
//   2. 防护：超限的"瞬移"整帧丢弃，且丢弃后历史仍前移（不会把瞬移拆成两次合法位移）；
//   3. 与角色衔接：root motion 启用后 Move 的水平速度**完全**由动画位移决定，
//      wishDir 的速度分量被忽略。
#include "testFramework.h"

#include <GN.h>
#include "GN_RootMotion.h"

#include <cmath>
#include <vector>

using namespace GN3d;

namespace {

const float kPi = 3.14159265358979323846f;

// 造一个根变换
sANIM_TRANSFORM makeRoot(float x, float y, float z, float yaw) {
    sANIM_TRANSFORM t;
    t.aTranslation = GNg::sVEC3{x, y, z};
    t.aRotation = GNg::sQUAT();
    t.aRotation.w = std::cos(yaw * 0.5f);
    t.aRotation.y = std::sin(yaw * 0.5f);
    t.aScale = GNg::sVEC3{1.0f, 1.0f, 1.0f};
    return t;
}

} // namespace

// ── 基础提取 ───────────────────────────────────────────
TEST_CASE(rootMotionFirstFrameOnlyRecords) {
    cROOT_MOTION_EXTRACTOR ex;
    CHECK(!ex.HasHistory());

    // 首帧：**即使根关节不在原点也不产生位移**
    // （很多 clip 的第 0 帧根关节有偏移，直接当位移会让角色"跳"）
    sROOT_MOTION motion;
    CHECK(!ex.Extract(makeRoot(5.0f, 0.0f, 3.0f, 0.0f), &motion));
    CHECK(ex.HasHistory());
    CHECK(!motion.aValid);
    CHECK_NEAR(motion.aDeltaPosition.x, 0.0f, 1e-6);

    // 第二帧：产生位移
    CHECK(ex.Extract(makeRoot(5.5f, 0.0f, 3.0f, 0.0f), &motion));
    CHECK(motion.aValid);
    CHECK_NEAR(motion.aDeltaPosition.x, 0.5f, 1e-5);
    CHECK_NEAR(motion.aDeltaPosition.z, 0.0f, 1e-5);
}

TEST_CASE(rootMotionReset) {
    cROOT_MOTION_EXTRACTOR ex;
    ex.Extract(makeRoot(0.0f, 0.0f, 0.0f, 0.0f), nullptr);
    CHECK(ex.HasHistory());
    ex.Reset();
    CHECK(!ex.HasHistory());
    // Reset 后第一帧又只记录
    sROOT_MOTION motion;
    CHECK(!ex.Extract(makeRoot(100.0f, 0.0f, 0.0f, 0.0f), &motion));
}

// 局部位移要按**当前朝向**旋转到世界
TEST_CASE(rootMotionRotatesByFacingYaw) {
    cROOT_MOTION_EXTRACTOR ex;
    // 朝向 +90°（面向 -X 还是 +X 取决于约定，这里只验证旋转本身）
    ex.SetFacingYaw(kPi * 0.5f);
    CHECK_NEAR(ex.FacingYaw(), kPi * 0.5f, 1e-5);

    ex.Extract(makeRoot(0.0f, 0.0f, 0.0f, 0.0f), nullptr);
    sROOT_MOTION motion;
    // 动画里"往前 1 m"（局部 +Z）
    CHECK(ex.Extract(makeRoot(0.0f, 0.0f, 1.0f, 0.0f), &motion));
    // rotateYaw((0,0,1), 90°) = (sin90, 0, cos90) = (1, 0, 0)
    CHECK_NEAR(motion.aDeltaPosition.x, 1.0f, 1e-5);
    CHECK_NEAR(motion.aDeltaPosition.z, 0.0f, 1e-5);
}

TEST_CASE(rootMotionYawExtraction) {
    cROOT_MOTION_EXTRACTOR ex;
    ex.Extract(makeRoot(0.0f, 0.0f, 0.0f, 0.0f), nullptr);
    sROOT_MOTION motion;
    // 转 30°
    CHECK(ex.Extract(makeRoot(0.0f, 0.0f, 0.0f, 0.5235988f), &motion));
    CHECK_NEAR(motion.aDeltaYaw, 0.5235988f, 1e-4);

    // **跨越 ±180°**：从 170° 转到 -170°（实际只转了 20°，不是 -340°）
    ex.Reset();
    ex.Extract(makeRoot(0.0f, 0.0f, 0.0f, 170.0f * kPi / 180.0f), nullptr);
    CHECK(ex.Extract(makeRoot(0.0f, 0.0f, 0.0f, -170.0f * kPi / 180.0f), &motion));
    CHECK_NEAR(std::fabs(motion.aDeltaYaw), 20.0f * kPi / 180.0f, 1e-4);
}

// ── 防护：瞬移检测 ────────────────────────────────────
TEST_CASE(rootMotionRejectsTeleport) {
    cROOT_MOTION_EXTRACTOR ex;
    ex.SetLimits(2.0f, 4.0f);
    CHECK_NEAR(ex.MaxStep(), 2.0f, 1e-6);
    CHECK_NEAR(ex.MaxYawStep(), 4.0f, 1e-6);

    ex.Extract(makeRoot(0.0f, 0.0f, 0.0f, 0.0f), nullptr);
    sROOT_MOTION motion;
    // 一帧跳出 100 m：整帧丢弃
    CHECK(!ex.Extract(makeRoot(100.0f, 0.0f, 0.0f, 0.0f), &motion));
    CHECK(!motion.aValid);

    // **关键**：历史已经前移到 100 —— 下一帧走 0.5 m 应当是 0.5，不是 99.5
    CHECK(ex.Extract(makeRoot(100.5f, 0.0f, 0.0f, 0.0f), &motion));
    CHECK_NEAR(motion.aDeltaPosition.x, 0.5f, 1e-5);
}

TEST_CASE(rootMotionRejectsYawTeleport) {
    cROOT_MOTION_EXTRACTOR ex;
    ex.SetLimits(2.0f, 1.0f); // 最大转角 1 rad
    ex.Extract(makeRoot(0.0f, 0.0f, 0.0f, 0.0f), nullptr);
    sROOT_MOTION motion;
    // 一帧转 2 rad：丢弃
    CHECK(!ex.Extract(makeRoot(0.0f, 0.0f, 0.0f, 2.0f), &motion));
    // 下一帧小幅转：正常
    CHECK(ex.Extract(makeRoot(0.0f, 0.0f, 0.0f, 2.3f), &motion));
    CHECK_NEAR(motion.aDeltaYaw, 0.3f, 1e-4);
}

TEST_CASE(rootMotionSetLimitsClamps) {
    cROOT_MOTION_EXTRACTOR ex;
    // 非法值回落到默认
    ex.SetLimits(-1.0f, 0.0f);
    CHECK_NEAR(ex.MaxStep(), 2.0f, 1e-6);
    CHECK_NEAR(ex.MaxYawStep(), 4.0f, 1e-6);
    ex.SetLimits(std::nanf(""), std::nanf(""));
    CHECK_NEAR(ex.MaxStep(), 2.0f, 1e-6);
}

TEST_CASE(rootMotionRejectsNonFinite) {
    cROOT_MOTION_EXTRACTOR ex;
    ex.Extract(makeRoot(0.0f, 0.0f, 0.0f, 0.0f), nullptr);
    sANIM_TRANSFORM bad = makeRoot(1.0f, 0.0f, 0.0f, 0.0f);
    bad.aTranslation.x = std::nanf("");
    sROOT_MOTION motion;
    CHECK(!ex.Extract(bad, &motion));
    CHECK(!motion.aValid);
    // 脏数据不推进历史：下一帧仍按上一次的有效位置算
    CHECK(ex.Extract(makeRoot(0.5f, 0.0f, 0.0f, 0.0f), &motion));
    CHECK_NEAR(motion.aDeltaPosition.x, 0.5f, 1e-5);
}

TEST_CASE(rootMotionNullOutIsSafe) {
    cROOT_MOTION_EXTRACTOR ex;
    ex.Extract(makeRoot(0.0f, 0.0f, 0.0f, 0.0f), nullptr);
    // out 为空指针也能正常推进历史
    CHECK(ex.Extract(makeRoot(1.0f, 0.0f, 0.0f, 0.0f), nullptr));
    sROOT_MOTION motion;
    CHECK(ex.Extract(makeRoot(1.5f, 0.0f, 0.0f, 0.0f), &motion));
    CHECK_NEAR(motion.aDeltaPosition.x, 0.5f, 1e-5);
}

// ── 工具函数 ───────────────────────────────────────────
TEST_CASE(rootMotionQuatYaw) {
    // 单位四元数 → 0
    GNg::sQUAT identity = {};
    identity.w = 1.0f;
    CHECK_NEAR(quatYaw(identity), 0.0f, 1e-6);

    // 绕 Y 转 90°
    GNg::sQUAT q90 = {};
    q90.w = std::cos(kPi * 0.25f);
    q90.y = std::sin(kPi * 0.25f);
    CHECK_NEAR(quatYaw(q90), kPi * 0.5f, 1e-4);

    // 未归一化的四元数也要给出正确角度（内部归一化）
    GNg::sQUAT scaled = q90;
    scaled.w *= 3.0f;
    scaled.y *= 3.0f;
    CHECK_NEAR(quatYaw(scaled), kPi * 0.5f, 1e-4);

    // 零四元数：安全返回 0
    GNg::sQUAT zero = {};
    CHECK_NEAR(quatYaw(zero), 0.0f, 1e-6);
}

TEST_CASE(rootMotionRotateYawVec) {
    // y 分量不受影响
    const GNg::sVEC3 v{1.0f, 5.0f, 0.0f};
    const GNg::sVEC3 r = rotateYaw(v, kPi * 0.5f);
    CHECK_NEAR(r.x, 0.0f, 1e-5);
    CHECK_NEAR(r.y, 5.0f, 1e-6);
    CHECK_NEAR(r.z, -1.0f, 1e-5);
    // 0° 恒等
    const GNg::sVEC3 same = rotateYaw(v, 0.0f);
    CHECK_NEAR(same.x, 1.0f, 1e-6);
    CHECK_NEAR(same.z, 0.0f, 1e-6);
}

// ═══════════════════════════════════════════════════════════
//  与 cCHARACTER 衔接
// ═══════════════════════════════════════════════════════════
namespace {

const float kStep = 1.0f / 60.0f;

// 平地 + 角色（root motion 场景的公共夹具）
struct sRootFixture {
    GN3d::cCHARACTER aCharacter;
    sRootFixture() {
        sCHARACTER_DESC desc;
        desc.aRadius = 0.32f;
        desc.aHeight = 1.75f;
        desc.aWalkSpeed = 2.0f;
        desc.aSprintSpeed = 6.0f;
        desc.aStepHeight = 0.35f;
        aCharacter.Init(desc);
        aCharacter.SetPosition(GNg::sVEC3{0.0f, desc.aHeight * 0.5f + 0.05f, 0.0f});
        aCharacter.AddFloor(GNg::sVEC3{0.0f, 0.0f, 0.0f}, 60.0f, 60.0f, 0.0f);
    }
};

} // namespace

TEST_CASE(characterRootMotionOverridesWishDir) {
    sRootFixture f;
    CHECK(!f.aCharacter.RootMotionEnabled());
    f.aCharacter.SetRootMotionEnabled(true);
    CHECK(f.aCharacter.RootMotionEnabled());

    const float startX = f.aCharacter.Position().x;
    // 每帧注入 +0.05 m 的位移，**同时**给一个朝 -X 的 wishDir
    // root motion 启用时 wishDir 必须被忽略
    for (int i = 0; i < 30; ++i) {
        f.aCharacter.ApplyRootMotion(GNg::sVEC3{0.05f, 0.0f, 0.0f}, kStep);
        f.aCharacter.Move(GNg::sVEC3{-1.0f, 0.0f, 0.0f}, false, false, kStep);
    }
    const float moved = f.aCharacter.Position().x - startX;
    // 30 * 0.05 = 1.5 m（朝 +X，而不是 wishDir 的 -X）
    CHECK_NEAR(moved, 1.5f, 0.15f);
    CHECK(moved > 0.0f);
}

TEST_CASE(characterRootMotionDisabledUsesWishDir) {
    sRootFixture f;
    f.aCharacter.SetRootMotionEnabled(false);
    const float startX = f.aCharacter.Position().x;
    for (int i = 0; i < 30; ++i) {
        // 未启用时 ApplyRootMotion 是空操作
        f.aCharacter.ApplyRootMotion(GNg::sVEC3{0.0f, 0.0f, 5.0f}, kStep);
        f.aCharacter.Move(GNg::sVEC3{0.0f, 0.0f, 1.0f}, false, false, kStep);
    }
    // 走 walkSpeed 2.0 * 0.5s = 1.0 m（沿 +Z）
    CHECK_NEAR(f.aCharacter.Position().z, 1.0f, 0.15f);
    CHECK_NEAR(f.aCharacter.Position().x, startX, 0.05f);
}

// 注入的位移只消费一次：不重新注入就不会继续走
TEST_CASE(characterRootMotionConsumedOnce) {
    sRootFixture f;
    f.aCharacter.SetRootMotionEnabled(true);
    f.aCharacter.ApplyRootMotion(GNg::sVEC3{0.1f, 0.0f, 0.0f}, kStep);
    const float startX = f.aCharacter.Position().x;
    f.aCharacter.Move(GNg::sVEC3{0.0f, 0.0f, 0.0f}, false, false, kStep);
    const float afterFirst = f.aCharacter.Position().x - startX;
    CHECK(afterFirst > 0.05f);

    // 不再注入：后续帧不该继续前进
    for (int i = 0; i < 20; ++i) {
        f.aCharacter.Move(GNg::sVEC3{0.0f, 0.0f, 0.0f}, false, false, kStep);
    }
    const float afterIdle = f.aCharacter.Position().x - startX;
    CHECK_NEAR(afterIdle, afterFirst, 0.06f);
}

// 覆盖而不是叠加：一帧内多次注入只算最后一次
TEST_CASE(characterRootMotionOverwritesNotAccumulates) {
    sRootFixture f;
    f.aCharacter.SetRootMotionEnabled(true);
    f.aCharacter.ApplyRootMotion(GNg::sVEC3{1.0f, 0.0f, 0.0f}, kStep);
    f.aCharacter.ApplyRootMotion(GNg::sVEC3{0.1f, 0.0f, 0.0f}, kStep); // 覆盖
    CHECK_NEAR(f.aCharacter.PendingRootMotion().x, 0.1f, 1e-6);

    const float startX = f.aCharacter.Position().x;
    f.aCharacter.Move(GNg::sVEC3{0.0f, 0.0f, 0.0f}, false, false, kStep);
    // 只走 0.1，不是 1.1
    const float moved = f.aCharacter.Position().x - startX;
    CHECK(moved < 0.25f);
}

TEST_CASE(characterRootMotionFacingYaw) {
    sRootFixture f;
    f.aCharacter.SetRootMotionEnabled(true);
    CHECK_NEAR(f.aCharacter.FacingYaw(), 0.0f, 1e-6);
    f.aCharacter.AddFacingYaw(0.5f);
    CHECK_NEAR(f.aCharacter.FacingYaw(), 0.5f, 1e-5);
    // 累积后归一化到 [-pi, pi]
    f.aCharacter.AddFacingYaw(kPi);
    CHECK(f.aCharacter.FacingYaw() >= -kPi - 1e-4f);
    CHECK(f.aCharacter.FacingYaw() <= kPi + 1e-4f);
    // 非法值被忽略
    const float before = f.aCharacter.FacingYaw();
    f.aCharacter.AddFacingYaw(std::nanf(""));
    CHECK_NEAR(f.aCharacter.FacingYaw(), before, 1e-6);
}

// 关闭时清掉待消费位移（避免"关掉后还走一步"）
TEST_CASE(characterRootMotionDisableClearsPending) {
    sRootFixture f;
    f.aCharacter.SetRootMotionEnabled(true);
    f.aCharacter.ApplyRootMotion(GNg::sVEC3{1.0f, 0.0f, 0.0f}, kStep);
    CHECK(f.aCharacter.PendingRootMotion().x > 0.0f);
    f.aCharacter.SetRootMotionEnabled(false);
    CHECK_NEAR(f.aCharacter.PendingRootMotion().x, 0.0f, 1e-6);
}

// 端到端：提取器 → 角色。动画走 2 m，角色就该走 2 m（脚不滑）
TEST_CASE(rootMotionEndToEndNoFootSlide) {
    sRootFixture f;
    f.aCharacter.SetRootMotionEnabled(true);

    cROOT_MOTION_EXTRACTOR ex;
    // 动画：每帧根关节前进 0.04 m（60fps ⇒ 2.4 m/s），共 60 帧 = 2.4 m
    float animX = 0.0f;
    ex.Extract(makeRoot(animX, 0.0f, 0.0f, 0.0f), nullptr); // 首帧只记录

    const float startX = f.aCharacter.Position().x;
    const int kFrames = 60;
    for (int i = 0; i < kFrames; ++i) {
        animX += 0.04f;
        sROOT_MOTION motion;
        if (ex.Extract(makeRoot(animX, 0.0f, 0.0f, 0.0f), &motion)) {
            f.aCharacter.ApplyRootMotion(motion.aDeltaPosition, kStep);
        }
        f.aCharacter.Move(GNg::sVEC3{0.0f, 0.0f, 0.0f}, false, false, kStep);
    }
    // 角色位移应当 ≈ 动画位移（2.4 m）；留 15% 容差给解穿透/摩擦细节
    const float moved = f.aCharacter.Position().x - startX;
    CHECK(moved > 2.0f);
    CHECK(moved < 2.8f);
}

// ═══════════════════════════════════════════════════════════
//  循环回绕补偿
// ═══════════════════════════════════════════════════════════
// 循环型 clip 的根关节从 0 走到 strideLength 再回绕到 0。
// 不补偿的话：丢掉回绕帧 ⇒ 每循环少走一步（实测 4 秒只走 0.03 m）；
// 当正常位移 ⇒ 角色被倒退一整步。
TEST_CASE(rootMotionLoopWrapCompensation) {
    cROOT_MOTION_EXTRACTOR ex;
    ex.SetLoopStride(0.9f);
    CHECK_NEAR(ex.LoopStride(), 0.9f, 1e-6);
    CHECK_EQ(ex.WrapCount(), 0u);

    ex.Extract(makeRoot(0.0f, 0.0f, 0.80f, 0.0f), nullptr);
    sROOT_MOTION motion;
    // 正常前进 0.05
    CHECK(ex.Extract(makeRoot(0.0f, 0.0f, 0.85f, 0.0f), &motion));
    CHECK_NEAR(motion.aDeltaPosition.z, 0.05f, 1e-5);

    // **回绕**：0.85 → 0.0（跳 -0.85，接近 -strideLength）
    // 期望：等价于"再往前 0.05"（0.85 → 0.90），不是倒退 0.85
    CHECK(ex.Extract(makeRoot(0.0f, 0.0f, 0.0f, 0.0f), &motion));
    CHECK_NEAR(motion.aDeltaPosition.z, 0.05f, 1e-4);
    CHECK_EQ(ex.WrapCount(), 1u);

    // 回绕之后继续正常前进
    CHECK(ex.Extract(makeRoot(0.0f, 0.0f, 0.03f, 0.0f), &motion));
    CHECK_NEAR(motion.aDeltaPosition.z, 0.03f, 1e-5);
    CHECK_EQ(ex.WrapCount(), 1u); // 没再回绕
}

TEST_CASE(rootMotionLoopStrideDisabledByDefault) {
    cROOT_MOTION_EXTRACTOR ex;
    CHECK_NEAR(ex.LoopStride(), 0.0f, 1e-6); // 默认关闭
    ex.Extract(makeRoot(0.0f, 0.0f, 0.0f, 0.0f), nullptr);
    sROOT_MOTION motion;
    // 无补偿时，一次 -0.8 的跳变超过默认上限 2.0？没有 —— 所以它是"合法位移"，
    // 角色会倒退。这正是"默认关闭"的含义：原地动画不该被乱补偿。
    CHECK(ex.Extract(makeRoot(0.0f, 0.0f, -0.8f, 0.0f), &motion));
    CHECK_NEAR(motion.aDeltaPosition.z, -0.8f, 1e-4);
    CHECK_EQ(ex.WrapCount(), 0u);
}

TEST_CASE(rootMotionLoopStrideIgnoresForwardJump) {
    cROOT_MOTION_EXTRACTOR ex;
    ex.SetLoopStride(0.9f);
    ex.Extract(makeRoot(0.0f, 0.0f, 0.0f, 0.0f), nullptr);
    sROOT_MOTION motion;
    // 正向跳 0.9（不是回绕）：不该被补偿
    CHECK(ex.Extract(makeRoot(0.0f, 0.0f, 0.9f, 0.0f), &motion));
    CHECK_NEAR(motion.aDeltaPosition.z, 0.9f, 1e-4);
    CHECK_EQ(ex.WrapCount(), 0u);
}

TEST_CASE(rootMotionLoopStrideIgnoresUnrelatedJumps) {
    cROOT_MOTION_EXTRACTOR ex;
    ex.SetLoopStride(0.9f);
    ex.Extract(makeRoot(0.0f, 0.0f, 0.0f, 0.0f), nullptr);
    sROOT_MOTION motion;
    // -0.3 的回退：长度与 strideLength(0.9) 差太远，不算回绕
    CHECK(ex.Extract(makeRoot(0.0f, 0.0f, -0.3f, 0.0f), &motion));
    CHECK_NEAR(motion.aDeltaPosition.z, -0.3f, 1e-4);
    CHECK_EQ(ex.WrapCount(), 0u);
}

TEST_CASE(rootMotionLoopStrideInvalidValues) {
    cROOT_MOTION_EXTRACTOR ex;
    ex.SetLoopStride(-5.0f);
    CHECK_NEAR(ex.LoopStride(), 0.0f, 1e-6); // 负值 → 关闭
    ex.SetLoopStride(std::nanf(""));
    CHECK_NEAR(ex.LoopStride(), 0.0f, 1e-6);
    // Reset 清 wrap 计数但保留 stride 设置
    ex.SetLoopStride(1.0f);
    ex.Reset();
    CHECK_NEAR(ex.LoopStride(), 1.0f, 1e-6);
    CHECK_EQ(ex.WrapCount(), 0u);
}

// 端到端：循环 3 圈后角色总位移应当 ≈ 3 * strideLength
TEST_CASE(rootMotionLoopEndToEndAccumulates) {
    sRootFixture f;
    f.aCharacter.SetRootMotionEnabled(true);

    const float stride = 0.9f;
    const int kFramesPerLoop = 60;
    const float perFrame = stride / (float)kFramesPerLoop;

    cROOT_MOTION_EXTRACTOR ex;
    ex.SetLoopStride(stride);
    ex.Extract(makeRoot(0.0f, 0.0f, 0.0f, 0.0f), nullptr);

    const float startZ = f.aCharacter.Position().z;
    float animZ = 0.0f;
    int loops = 0;
    for (int i = 0; i < kFramesPerLoop * 3; ++i) {
        animZ += perFrame;
        if (animZ >= stride) {
            animZ -= stride;
            ++loops;
        }
        sROOT_MOTION motion;
        if (ex.Extract(makeRoot(0.0f, 0.0f, animZ, 0.0f), &motion)) {
            f.aCharacter.ApplyRootMotion(motion.aDeltaPosition, kStep);
        }
        f.aCharacter.Move(GNg::sVEC3{0.0f, 0.0f, 0.0f}, false, false, kStep);
    }
    // 3 圈 = 2.7 m
    const float moved = f.aCharacter.Position().z - startZ;
    CHECK(loops >= 2); // 确实回绕过
    CHECK(ex.WrapCount() >= 2u);
    CHECK(moved > 2.3f);
    CHECK(moved < 3.1f);
}
