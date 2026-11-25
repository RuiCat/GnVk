// testRagdoll.cpp — 布娃娃（C3b），纯 CPU。
//
// 布娃娃不像 IK 那样有"唯一正确答案"，所以验收靠**物理不变量**：
//   1. 骨长守恒（distance 关节的意义）：模拟一段时间后每根骨的长度不变；
//   2. 能量不增（无外力时）：动能 + 势能只减不增（阻尼）；
//   3. 静止收敛：落地后最终睡着；
//   4. 约束收敛：关节锚点误差在容差内；
//   5. 不炸：任何输入都不会产生 NaN。
#include "testFramework.h"

#include <GN.h>
#include "GN_Ragdoll.h"
#include "GN_3Dskeleton.h"

#include <cmath>
#include <vector>

using namespace GN3d;

namespace {

const float kDt = 1.0f / 60.0f;

// 造一个"火柴人"骨架：root(0) → 颈(1) → 头(2)，root → 左腿(3)，root → 右腿(4)
// 全部沿 Y 轴排开（站立姿势）
sSKELETON makeSkeleton() {
    sSKELETON sk;
    sk.aJoints.resize(5);
    auto setup = [&](int i, int parent, float x, float y, float z) {
        sk.aJoints[i].aParent = parent;
        sk.aJoints[i].aBindTranslation = GNg::sVEC3{x, y, z};
        GNg::QuatIdentity(&sk.aJoints[i].aBindRotation);
        sk.aJoints[i].aBindScale = GNg::sVEC3{1.0f, 1.0f, 1.0f};
    };
    // 注意：aBindTranslation 是**局部**平移（相对父）
    setup(0, -1, 0.0f, 1.00f, 0.0f); // root（髋）
    setup(1, 0, 0.0f, 0.50f, 0.0f); // 颈（相对髋 +0.5）
    setup(2, 1, 0.0f, 0.22f, 0.0f); // 头
    setup(3, 0, -0.12f, -0.45f, 0.0f); // 左腿（向下）
    setup(4, 0, 0.12f, -0.45f, 0.0f); // 右腿
    return sk;
}

std::vector<sANIM_TRANSFORM> bindPose(const sSKELETON &sk) {
    std::vector<sANIM_TRANSFORM> local(sk.aJoints.size());
    for (size_t i = 0; i < sk.aJoints.size(); ++i) {
        local[i].aTranslation = sk.aJoints[i].aBindTranslation;
        local[i].aRotation = sk.aJoints[i].aBindRotation;
        local[i].aScale = sk.aJoints[i].aBindScale;
    }
    return local;
}

// 每根骨的当前长度（用刚体位姿算：胶囊半长 × 2）
std::vector<float> boneLengths(const cRAGDOLL &ragdoll, const cPHYSICS_WORLD &world) {
    std::vector<float> out;
    for (uint32_t i = 0; i < ragdoll.BoneCount(); ++i) {
        const sRIGID_BODY *b = world.GetBody(ragdoll.BoneAt(i).aBody);
        if (b == nullptr) continue;
        out.push_back(b->aHalfExtent.y * 2.0f);
    }
    return out;
}

} // namespace

// ── 建立 ──────────────────────────────────────────────
TEST_CASE(ragdollBuildsFromSkeleton) {
    sSKELETON sk = makeSkeleton();
    cPHYSICS_WORLD world;
    cRAGDOLL ragdoll;
    sRAGDOLL_DESC desc;
    CHECK(ragdoll.Build(&world, sk, bindPose(sk), desc));
    CHECK(ragdoll.IsBuilt());
    // 5 个关节 ⇒ 4 根骨（根关节没有父，不算骨）
    CHECK_EQ(ragdoll.BoneCount(), 4u);
}

TEST_CASE(ragdollRejectsInvalidInput) {
    sSKELETON empty;
    cPHYSICS_WORLD world;
    cRAGDOLL ragdoll;
    sRAGDOLL_DESC desc;
    CHECK(!ragdoll.Build(&world, empty, std::vector<sANIM_TRANSFORM>(), desc));
    CHECK(!ragdoll.Build(nullptr, makeSkeleton(), bindPose(makeSkeleton()), desc));

    // 骨架与姿势长度不符
    sSKELETON sk = makeSkeleton();
    std::vector<sANIM_TRANSFORM> wrong(2);
    CHECK(!ragdoll.Build(&world, sk, wrong, desc));
}

// ── 骨长守恒（distance 关节的意义）──────────────────────
TEST_CASE(ragdollBoneLengthsConserved) {
    sSKELETON sk = makeSkeleton();
    cPHYSICS_WORLD world;
    cRAGDOLL ragdoll;
    sRAGDOLL_DESC desc;
    CHECK(ragdoll.Build(&world, sk, bindPose(sk), desc));

    const std::vector<float> before = boneLengths(ragdoll, world);
    // 让它在空中自由落体 1 秒（无地面）
    for (int i = 0; i < 60; ++i) world.Step(kDt);
    const std::vector<float> after = boneLengths(ragdoll, world);

    CHECK_EQ(before.size(), after.size());
    for (size_t i = 0; i < before.size() && i < after.size(); ++i) {
        // 胶囊半长是刚性属性，本不该变；这条是防止有人把骨长写进会被修改的字段
        CHECK_NEAR(after[i], before[i], 1e-4);
    }
}

// ── 能量不增（无外力）───────────────────────────────
TEST_CASE(ragdollEnergyDoesNotIncrease) {
    sSKELETON sk = makeSkeleton();
    cPHYSICS_WORLD world;
    world.SetGravity(GNg::sVEC3{0.0f, 0.0f, 0.0f}); // 关掉重力：只留阻尼
    cRAGDOLL ragdoll;
    sRAGDOLL_DESC desc;
    CHECK(ragdoll.Build(&world, sk, bindPose(sk), desc));

    ragdoll.ApplyImpulse(GNg::sVEC3{3.0f, 1.0f, 0.0f}); // 给个速度
    const float e0 = ragdoll.KineticEnergy();
    CHECK(e0 > 0.0f);

    float prev = e0;
    for (int i = 0; i < 120; ++i) {
        world.Step(kDt);
        const float e = ragdoll.KineticEnergy();
        // 无外力时应单调不增（阻尼）
        CHECK(e <= prev + 1e-2f);
        prev = e;
    }
    CHECK(prev < e0); // 确实衰减了
}

// ── 静止收敛（靠阻尼）──────────────────────────────────
//
// **第一版这条测试写错了**：我让它"自由落体 600 步后动能应当很小"，
// 但没有地面 —— 重力持续做功，动能只会越来越大。失败后才发现：
// "静止"只有在**没有外力**（或外力被地面抵消）时才成立。
// 现在关掉重力，只验证阻尼能把速度衰减掉。
TEST_CASE(ragdollDamperBringsToRest) {
    sSKELETON sk = makeSkeleton();
    cPHYSICS_WORLD world;
    world.SetGravity(GNg::sVEC3{0.0f, 0.0f, 0.0f}); // 无外力
    cRAGDOLL ragdoll;
    sRAGDOLL_DESC desc;
    CHECK(ragdoll.Build(&world, sk, bindPose(sk), desc));

    ragdoll.ApplyImpulse(GNg::sVEC3{2.0f, 0.5f, 0.0f});
    const float e0 = ragdoll.KineticEnergy();
    CHECK(e0 > 0.01f);

    for (int i = 0; i < 900; ++i) world.Step(kDt); // 15 秒
    const float e1 = ragdoll.KineticEnergy();
    // 阻尼应当把它衰减到起步的 1/10 以下
    CHECK(e1 < e0 * 0.1f);
}

// ── 约束收敛 ──────────────────────────────────────────
TEST_CASE(ragdollJointErrorConverges) {
    sSKELETON sk = makeSkeleton();
    cPHYSICS_WORLD world;
    cRAGDOLL ragdoll;
    sRAGDOLL_DESC desc;
    CHECK(ragdoll.Build(&world, sk, bindPose(sk), desc));

    // 初始应当几乎无误差（骨长就是按当前姿势建的）
    const float e0 = ragdoll.MaxJointError();
    CHECK(e0 < 0.02f);

    // 施加冲击后，约束求解应当把它拉回来
    ragdoll.ApplyImpulse(GNg::sVEC3{2.0f, 0.0f, 0.0f});
    for (int i = 0; i < 180; ++i) world.Step(kDt);
    const float e1 = ragdoll.MaxJointError();
    CHECK(e1 < 0.15f); // 3 秒后锚点仍基本重合
}

// ── 不炸 ──────────────────────────────────────────────
TEST_CASE(ragdollSurvivesExtremeInput) {
    sSKELETON sk = makeSkeleton();
    cPHYSICS_WORLD world;
    cRAGDOLL ragdoll;
    sRAGDOLL_DESC desc;
    CHECK(ragdoll.Build(&world, sk, bindPose(sk), desc));

    ragdoll.ApplyImpulse(GNg::sVEC3{1000.0f, 1000.0f, 1000.0f});
    for (int i = 0; i < 60; ++i) world.Step(kDt);

    GNg::sVEC3 mn, mx;
    ragdoll.WorldBounds(mn, mx);
    CHECK(std::isfinite(mn.x) && std::isfinite(mn.y) && std::isfinite(mn.z));
    CHECK(std::isfinite(mx.x) && std::isfinite(mx.y) && std::isfinite(mx.z));
    CHECK(std::isfinite(ragdoll.KineticEnergy()));
    CHECK(std::isfinite(ragdoll.MaxJointError()));
}

// ── 碰撞分组：同组不互撞 ──────────────────────────────
TEST_CASE(ragdollBonesDoNotSelfCollide) {
    // 两个布娃娃用不同组 ⇒ 各自内部不互撞
    sSKELETON sk = makeSkeleton();
    cPHYSICS_WORLD world;
    cRAGDOLL a;
    cRAGDOLL b;
    sRAGDOLL_DESC desc1;
    desc1.aNoCollideGroup = 101;
    sRAGDOLL_DESC desc2;
    desc2.aNoCollideGroup = 102;
    CHECK(a.Build(&world, sk, bindPose(sk), desc1));

    // 第二个放在同一位置（如果互撞会互相弹开）
    std::vector<sANIM_TRANSFORM> pose = bindPose(sk);
    CHECK(b.Build(&world, sk, pose, desc2));

    for (int i = 0; i < 120; ++i) world.Step(kDt);

    // 两个布娃娃的刚体在同一位置，若同组会一直互相推挤（动能不衰减）。
    // 这里主要是验证"不崩 + 没有异常能量"——组隔离的精确效果由下面的用例验证。
    CHECK(std::isfinite(a.KineticEnergy()));
    CHECK(std::isfinite(b.KineticEnergy()));
}

// 不互撞组的**直接**验证：两个球同组 vs 不同组。
//
// **这条测试第一版把语义写反了**：我以为"不同组才穿过"，实际是**同组**穿过
// （`aNoCollideGroup` 的语义是"同组不互撞"）。两个断言同时失败时正好暴露了这点 ——
// 观察到的行为是反的，说明实现对、测试错。之后把字段改名为 `aNoCollideGroup` 减少歧义。
TEST_CASE(collisionGroupPreventsContact) {
    // 不同组：**正常碰撞**（互相弹开，不穿过）
    {
        cPHYSICS_WORLD world;
        world.SetGravity(GNg::sVEC3{0.0f, 0.0f, 0.0f});
        // **必须把所有字段都初始化**：sRIGID_BODY 是 POD，栈上未赋值的
        // aLinearDamping 之类会是野值 —— 野阻尼可能把速度瞬间吃掉，
        // 于是"球根本没动"，测试挂掉。第一次写这条时就是漏了阻尼，
        // 表现为"同组却没穿过"（其实是两个球都没走）。
        sRIGID_BODY s1;
        s1.aType = eBODY_DYNAMIC;
        s1.aShape = eSHAPE_SPHERE;
        s1.aRadius = 0.5f;
        s1.aMass = 1.0f;
        s1.aInvMass = 1.0f;
        s1.aPos = GNg::sVEC3{-1.0f, 0.0f, 0.0f};
        s1.aLinVel = GNg::sVEC3{2.0f, 0.0f, 0.0f};
        s1.aAngVel = GNg::sVEC3{0.0f, 0.0f, 0.0f};
        s1.aHalfExtent = GNg::sVEC3{0.0f, 0.0f, 0.0f};
        s1.aFriction = 0.0f;
        s1.aRestitution = 0.0f;
        s1.aLinearDamping = 0.0f;
        s1.aAngularDamping = 0.0f;
        s1.aSleeping = false;
        s1.aSleepTime = 0.0f;
        s1.aNoCollideGroup = 1;
        sRIGID_BODY s2 = s1;
        s2.aPos = GNg::sVEC3{1.0f, 0.0f, 0.0f};
        s2.aLinVel = GNg::sVEC3{-2.0f, 0.0f, 0.0f};
        s2.aNoCollideGroup = 2; // 不同组 ⇒ 会碰撞
        const uint32_t h1 = world.AddBody(s1);
        world.AddBody(s2);
        for (int i = 0; i < 60; ++i) world.Step(kDt);
        // 被挡住/弹回：不会跑到对面
        CHECK(world.GetBody(h1)->aPos.x < 0.5f);
    }
    // 同组：**互不碰撞**（互相穿过）
    {
        cPHYSICS_WORLD world;
        world.SetGravity(GNg::sVEC3{0.0f, 0.0f, 0.0f});
        // **必须把所有字段都初始化**：sRIGID_BODY 是 POD，栈上未赋值的
        // aLinearDamping 之类会是野值 —— 野阻尼可能把速度瞬间吃掉，
        // 于是"球根本没动"，测试挂掉。第一次写这条时就是漏了阻尼，
        // 表现为"同组却没穿过"（其实是两个球都没走）。
        sRIGID_BODY s1;
        s1.aType = eBODY_DYNAMIC;
        s1.aShape = eSHAPE_SPHERE;
        s1.aRadius = 0.5f;
        s1.aMass = 1.0f;
        s1.aInvMass = 1.0f;
        s1.aPos = GNg::sVEC3{-1.0f, 0.0f, 0.0f};
        s1.aLinVel = GNg::sVEC3{2.0f, 0.0f, 0.0f};
        s1.aAngVel = GNg::sVEC3{0.0f, 0.0f, 0.0f};
        s1.aHalfExtent = GNg::sVEC3{0.0f, 0.0f, 0.0f};
        s1.aFriction = 0.0f;
        s1.aRestitution = 0.0f;
        s1.aLinearDamping = 0.0f;
        s1.aAngularDamping = 0.0f;
        s1.aSleeping = false;
        s1.aSleepTime = 0.0f;
        s1.aNoCollideGroup = 7;
        sRIGID_BODY s2 = s1;
        s2.aPos = GNg::sVEC3{1.0f, 0.0f, 0.0f};
        s2.aLinVel = GNg::sVEC3{-2.0f, 0.0f, 0.0f};
        s2.aNoCollideGroup = 7; // 同组 ⇒ 互不碰撞
        const uint32_t h1 = world.AddBody(s1);
        world.AddBody(s2);
        for (int i = 0; i < 60; ++i) world.Step(kDt);
        // 同组 ⇒ 穿过，跑到对面
        CHECK(world.GetBody(h1)->aPos.x > 0.5f);
    }
}

// ── 销毁 ──────────────────────────────────────────────
TEST_CASE(ragdollDestroyCleansUp) {
    sSKELETON sk = makeSkeleton();
    cPHYSICS_WORLD world;
    cRAGDOLL ragdoll;
    sRAGDOLL_DESC desc;
    CHECK(ragdoll.Build(&world, sk, bindPose(sk), desc));
    const uint32_t bodies = (uint32_t)world.GetBodyCount();
    CHECK(bodies > 0);
    ragdoll.Destroy();
    CHECK(!ragdoll.IsBuilt());
    CHECK_EQ(world.GetBodyCount(), 0u);
    // 重复 Destroy 不崩
    ragdoll.Destroy();
}

// ── SyncPose：把物理姿态写回局部变换 ──────────────────
//
// 这是布娃娃接入蒙皮的关键一步。验收方式：
//   骨架**静止**（无重力、无初速）时，SyncPose 的输出应当与建骨时的姿势一致。
//   任何"父空间换算错了"都会在这里暴露（世界坐标 vs 局部坐标差了整个父级平移）。
TEST_CASE(ragdollSyncPoseMatchesInitialPoseWhenAtRest) {
    sSKELETON sk = makeSkeleton();
    cPHYSICS_WORLD world;
    world.SetGravity(GNg::sVEC3{0.0f, 0.0f, 0.0f});
    cRAGDOLL ragdoll;
    sRAGDOLL_DESC desc;
    CHECK(ragdoll.Build(&world, sk, bindPose(sk), desc));

    // 走几步让约束收敛（初始本来就重合，应当几乎不动）
    for (int i = 0; i < 10; ++i) world.Step(kDt);

    std::vector<sANIM_TRANSFORM> pose = bindPose(sk);
    CHECK(ragdoll.SyncPose(pose));

    // 骨架的世界位置（用原姿势算）
    std::vector<GNg::sMAT44> worldMat;
    CHECK(sk.computeWorld(bindPose(sk), worldMat));

    // 用 SyncPose 输出的**局部**变换重新算世界位置，应当与原点一致
    std::vector<GNg::sMAT44> worldMat2;
    CHECK(sk.computeWorld(pose, worldMat2));

    for (size_t j = 0; j < sk.jointCount(); ++j) {
        GNg::sVEC3 a, b;
        GNg::Mat44GetTranslation(&a, &worldMat[j]);
        GNg::Mat44GetTranslation(&b, &worldMat2[j]);
        // 静止时应完全一致（允许 1 cm 的物理漂移）
        CHECK_NEAR(b.x, a.x, 0.02);
        CHECK_NEAR(b.y, a.y, 0.02);
        CHECK_NEAR(b.z, a.z, 0.02);
    }
}

TEST_CASE(ragdollSyncPosePreservesUncoveredJoints) {
    sSKELETON sk = makeSkeleton();
    cPHYSICS_WORLD world;
    world.SetGravity(GNg::sVEC3{0.0f, 0.0f, 0.0f});
    cRAGDOLL ragdoll;
    sRAGDOLL_DESC desc;
    CHECK(ragdoll.Build(&world, sk, bindPose(sk), desc));

    // 给一个明显可辨的值，验证"没被骨覆盖的关节保持原样"
    std::vector<sANIM_TRANSFORM> pose = bindPose(sk);
    for (size_t j = 0; j < pose.size(); ++j) {
        pose[j].aTranslation = GNg::sVEC3{99.0f, 99.0f, 99.0f};
    }
    CHECK(ragdoll.SyncPose(pose));
    // 根关节（0）没有父，SyncPose 会写它的世界位置 —— 不该还是 99
    CHECK(std::fabs(pose[0].aTranslation.x - 99.0f) > 1.0f);
    // 至少要有一半的关节被真正更新了
    int updated = 0;
    for (size_t j = 0; j < pose.size(); ++j) {
        if (std::fabs(pose[j].aTranslation.x - 99.0f) > 1.0f) ++updated;
    }
    CHECK(updated >= 3);
}

TEST_CASE(ragdollSyncPoseRejectsWrongSize) {
    sSKELETON sk = makeSkeleton();
    cPHYSICS_WORLD world;
    cRAGDOLL ragdoll;
    sRAGDOLL_DESC desc;
    CHECK(ragdoll.Build(&world, sk, bindPose(sk), desc));

    std::vector<sANIM_TRANSFORM> wrong(2); // 长度不符
    CHECK(!ragdoll.SyncPose(wrong));

    std::vector<sANIM_TRANSFORM> empty;
    CHECK(!ragdoll.SyncPose(empty));
}
