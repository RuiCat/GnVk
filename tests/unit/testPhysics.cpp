// testPhysics.cpp — 物理世界端到端单测：下落静止、堆叠稳定、对撞、射线、休眠唤醒、确定性、非法输入。
#include "testFramework.h"

#include <GN.h>
#include "GN_Physics.h"

#include <cmath>
#include <cstring>
#include <vector>

using namespace GN3d;

namespace {

const float kPi = 3.14159265358979f;

// 一个常见场景：静态地面（顶面在 y=0）。
uint32_t AddGround(cPHYSICS_WORLD &world) {
    sRIGID_BODY ground = MakeBoxBody(eBODY_STATIC, GNg::sVEC3{0.0f, -0.25f, 0.0f},
                                     GNg::sVEC3{10.0f, 0.25f, 10.0f}, 0.0f);
    return world.AddBody(ground);
}

bool AllFinite(const GNg::sVEC3 &v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

} // namespace

TEST_CASE(physicsDropRestsOnGround) {
    cPHYSICS_WORLD world;
    AddGround(world);
    sRIGID_BODY sphere = MakeSphereBody(eBODY_DYNAMIC, GNg::sVEC3{0.0f, 2.0f, 0.0f}, 0.5f, 1.0f);
    const uint32_t handle = world.AddBody(sphere);
    CHECK(handle != cPHYSICS_WORLD::INVALID_HANDLE);

    for(int i = 0; i < 240; ++i) world.Step(1.0f / 60.0f); // 4 秒

    const sRIGID_BODY *body = world.GetBody(handle);
    CHECK(body != 0);
    if(!body) return;
    CHECK(AllFinite(body->aPos));
    CHECK_NEAR(body->aPos.y, 0.5f, 0.05f); // 半径 0.5，落在地面顶
    CHECK(std::fabs(body->aPos.x) < 0.02f);
    CHECK(std::fabs(body->aLinVel.y) < 0.05f);
    CHECK(body->aSleeping); // 静止后入睡
}

TEST_CASE(physicsBoxStackStable) {
    cPHYSICS_WORLD world;
    AddGround(world);
    const GNg::sVEC3 half{0.5f, 0.5f, 0.5f};
    const uint32_t b0 = world.AddBody(MakeBoxBody(eBODY_DYNAMIC, GNg::sVEC3{0.0f, 0.5f, 0.0f}, half, 1.0f));
    const uint32_t b1 = world.AddBody(MakeBoxBody(eBODY_DYNAMIC, GNg::sVEC3{0.0f, 1.5f, 0.0f}, half, 1.0f));
    const uint32_t b2 = world.AddBody(MakeBoxBody(eBODY_DYNAMIC, GNg::sVEC3{0.0f, 2.5f, 0.0f}, half, 1.0f));
    CHECK(b0 != cPHYSICS_WORLD::INVALID_HANDLE);
    CHECK(b1 != cPHYSICS_WORLD::INVALID_HANDLE);
    CHECK(b2 != cPHYSICS_WORLD::INVALID_HANDLE);

    for(int i = 0; i < 300; ++i) world.Step(1.0f / 60.0f); // 5 秒

    const sRIGID_BODY *top = world.GetBody(b2);
    const sRIGID_BODY *mid = world.GetBody(b1);
    const sRIGID_BODY *low = world.GetBody(b0);
    CHECK(top && mid && low);
    if(!top || !mid || !low) return;
    CHECK(AllFinite(top->aPos));
    // 不塌：三层仍在原位附近（允许小幅下沉/滑动）
    CHECK(std::fabs(low->aPos.y - 0.5f) < 0.15f);
    CHECK(std::fabs(mid->aPos.y - 1.5f) < 0.2f);
    CHECK(std::fabs(top->aPos.y - 2.5f) < 0.25f);
    CHECK(std::fabs(top->aPos.x) < 0.2f);
    CHECK(std::fabs(top->aPos.z) < 0.2f);
    CHECK(top->aSleeping);
    CHECK(mid->aSleeping);
    CHECK(low->aSleeping);
}

// 6 层堆叠：warm starting + 30 次速度迭代后能稳定入睡（20 次迭代时 6 层会缓慢下沉坍塌）。
TEST_CASE(physicsTallStackStable) {
    cPHYSICS_WORLD world;
    AddGround(world);
    const GNg::sVEC3 half{0.5f, 0.5f, 0.5f};
    uint32_t handles[6];
    for(int i = 0; i < 6; ++i) {
        handles[i] = world.AddBody(
            MakeBoxBody(eBODY_DYNAMIC, GNg::sVEC3{0.0f, 0.5f + (float)i, 0.0f}, half, 1.0f));
        CHECK(handles[i] != cPHYSICS_WORLD::INVALID_HANDLE);
    }

    for(int i = 0; i < 300; ++i) world.Step(1.0f / 60.0f); // 5 秒

    for(int i = 0; i < 6; ++i) {
        const sRIGID_BODY *body = world.GetBody(handles[i]);
        CHECK(body != 0);
        if(!body) continue;
        CHECK(AllFinite(body->aPos));
        CHECK_NEAR(body->aPos.y, 0.5f + (float)i, 0.15f);
        CHECK(std::fabs(body->aPos.x) < 0.2f);
        CHECK(std::fabs(body->aPos.z) < 0.2f);
        CHECK(body->aSleeping);
    }
}

// CCD v1：薄地面（顶面 y=0、厚 0.1），半径 0.1 的球以 120 m/s 下落（一步位移约 2m）。
// 开 CCD 时在 TOI 处停住且进入方向法向速度清零；不开则中心穿到地面顶面以下。
TEST_CASE(physicsCcdPreventsTunneling) {
    cPHYSICS_WORLD fast;
    fast.AddBody(MakeBoxBody(eBODY_STATIC, GNg::sVEC3{0.0f, -0.05f, 0.0f},
                             GNg::sVEC3{10.0f, 0.05f, 10.0f}, 0.0f));
    sRIGID_BODY fastBall = MakeSphereBody(eBODY_DYNAMIC, GNg::sVEC3{0.0f, 2.0f, 0.0f}, 0.1f, 1.0f);
    fastBall.aLinVel.y = -120.0f;
    const uint32_t fastHandle = fast.AddBody(fastBall);
    CHECK(fastHandle != cPHYSICS_WORLD::INVALID_HANDLE);
    fast.setContinuousCollision(true);
    fast.Step(1.0f / 60.0f);

    const sRIGID_BODY *stopped = fast.GetBody(fastHandle);
    CHECK(stopped != 0);
    if(!stopped) return;
    CHECK(stopped->aPos.y > 0.09f); // 停在地面顶面 + 半径附近（不是穿过去）
    CHECK(stopped->aPos.y < 0.11f);
    CHECK(stopped->aLinVel.y > -0.01f); // 进入方向的法向速度已清零

    // 对照组：同一初态不开 CCD，一步后球心已在地面顶面以下（穿透未被拦）。
    cPHYSICS_WORLD tunnel;
    tunnel.AddBody(MakeBoxBody(eBODY_STATIC, GNg::sVEC3{0.0f, -0.05f, 0.0f},
                               GNg::sVEC3{10.0f, 0.05f, 10.0f}, 0.0f));
    sRIGID_BODY tunnelBall = MakeSphereBody(eBODY_DYNAMIC, GNg::sVEC3{0.0f, 2.0f, 0.0f}, 0.1f, 1.0f);
    tunnelBall.aLinVel.y = -120.0f;
    const uint32_t tunnelHandle = tunnel.AddBody(tunnelBall);
    CHECK(tunnelHandle != cPHYSICS_WORLD::INVALID_HANDLE);
    tunnel.Step(1.0f / 60.0f);

    const sRIGID_BODY *passed = tunnel.GetBody(tunnelHandle);
    CHECK(passed != 0);
    if(!passed) return;
    CHECK(passed->aPos.y < 0.05f);
}

// CCD 对胶囊同样有效（包围半径 = 圆柱半长 + 半径），两步内被薄地面拦住。
TEST_CASE(physicsCcdCapsuleStops) {
    cPHYSICS_WORLD world;
    world.setContinuousCollision(true);
    world.AddBody(MakeBoxBody(eBODY_STATIC, GNg::sVEC3{0.0f, -0.05f, 0.0f},
                              GNg::sVEC3{10.0f, 0.05f, 10.0f}, 0.0f));
    sRIGID_BODY capsule = MakeCapsuleBody(eBODY_DYNAMIC, GNg::sVEC3{0.0f, 3.0f, 0.0f}, 0.2f, 0.5f, 1.0f);
    capsule.aLinVel.y = -90.0f;
    const uint32_t handle = world.AddBody(capsule);
    CHECK(handle != cPHYSICS_WORLD::INVALID_HANDLE);

    for(int i = 0; i < 4; ++i) world.Step(1.0f / 60.0f);

    const sRIGID_BODY *body = world.GetBody(handle);
    CHECK(body != 0);
    if(!body) return;
    CHECK(AllFinite(body->aPos));
    CHECK(body->aPos.y > 0.65f); // 竖直胶囊落在半径 0.7 附近
    CHECK(body->aPos.y < 0.75f);
}

// CCD 开启不影响常速接触：普通下落仍静止在地面并入睡。
TEST_CASE(physicsCcdKeepsNormalContacts) {
    cPHYSICS_WORLD world;
    world.setContinuousCollision(true);
    AddGround(world);
    const uint32_t handle = world.AddBody(
        MakeSphereBody(eBODY_DYNAMIC, GNg::sVEC3{0.0f, 2.0f, 0.0f}, 0.5f, 1.0f));
    CHECK(handle != cPHYSICS_WORLD::INVALID_HANDLE);

    for(int i = 0; i < 240; ++i) world.Step(1.0f / 60.0f);

    const sRIGID_BODY *body = world.GetBody(handle);
    CHECK(body != 0);
    if(!body) return;
    CHECK_NEAR(body->aPos.y, 0.5f, 0.05f);
    CHECK(body->aSleeping);
}

TEST_CASE(physicsHeadOnSpheresSeparate) {
    cPHYSICS_WORLD world;
    world.SetGravity(GNg::sVEC3{0.0f, 0.0f, 0.0f});
    sRIGID_BODY a = MakeSphereBody(eBODY_DYNAMIC, GNg::sVEC3{-1.0f, 0.0f, 0.0f}, 0.5f, 1.0f);
    a.aLinVel.x = 1.0f;
    sRIGID_BODY b = MakeSphereBody(eBODY_DYNAMIC, GNg::sVEC3{1.0f, 0.0f, 0.0f}, 0.5f, 1.0f);
    b.aLinVel.x = -1.0f;
    const uint32_t ha = world.AddBody(a);
    const uint32_t hb = world.AddBody(b);
    CHECK(ha != cPHYSICS_WORLD::INVALID_HANDLE && hb != cPHYSICS_WORLD::INVALID_HANDLE);

    for(int i = 0; i < 180; ++i) world.Step(1.0f / 60.0f); // 3 秒，含碰撞

    const sRIGID_BODY *left = world.GetBody(ha);
    const sRIGID_BODY *right = world.GetBody(hb);
    CHECK(left && right);
    if(!left || !right) return;
    CHECK(left->aLinVel.x < 0.05f); // 弹性 0：碰后不再互相压入
    CHECK(right->aLinVel.x > -0.05f);
    const float dx = right->aPos.x - left->aPos.x;
    CHECK(dx > 0.95f); // 不再重叠（半径和 1.0）
}

TEST_CASE(physicsRaycastHitsNearest) {
    cPHYSICS_WORLD world;
    AddGround(world);
    const uint32_t lower = world.AddBody(
        MakeSphereBody(eBODY_DYNAMIC, GNg::sVEC3{0.0f, 1.0f, 0.0f}, 0.5f, 1.0f));
    const uint32_t upper = world.AddBody(
        MakeSphereBody(eBODY_STATIC, GNg::sVEC3{0.0f, 3.0f, 0.0f}, 0.5f, 0.0f));
    CHECK(lower != cPHYSICS_WORLD::INVALID_HANDLE && upper != cPHYSICS_WORLD::INVALID_HANDLE);

    sRAY_HIT hit;
    const bool ok = world.Raycast(GNg::sVEC3{0.0f, 10.0f, 0.0f}, GNg::sVEC3{0.0f, -1.0f, 0.0f},
                                  100.0f, hit);
    CHECK(ok);
    CHECK_EQ(hit.aIndex, upper); // 最近的球是 3m 处那个
    CHECK_NEAR(hit.aT, 6.5f, 1e-2f);

    sRAY_HIT closeHit;
    // maxT 只够打到上层以下：命中上层球还是下层？从 y=10 出发，上层球顶在 3.5，t=6.5
    CHECK(world.Raycast(GNg::sVEC3{0.0f, 10.0f, 0.0f}, GNg::sVEC3{0.0f, -1.0f, 0.0f}, 6.0f, closeHit) == false);

    sRAY_HIT miss;
    CHECK(!world.Raycast(GNg::sVEC3{50.0f, 10.0f, 0.0f}, GNg::sVEC3{0.0f, -1.0f, 0.0f}, 100.0f, miss));
    CHECK(!world.Raycast(GNg::sVEC3{0.0f, 10.0f, 0.0f}, GNg::sVEC3{0.0f, -1.0f, 0.0f}, -1.0f, miss));
}

TEST_CASE(physicsSleepThenWake) {
    cPHYSICS_WORLD world;
    AddGround(world);
    const uint32_t handle =
        world.AddBody(MakeSphereBody(eBODY_DYNAMIC, GNg::sVEC3{0.0f, 0.6f, 0.0f}, 0.5f, 1.0f));
    for(int i = 0; i < 180; ++i) world.Step(1.0f / 60.0f);
    const sRIGID_BODY *sleeping = world.GetBody(handle);
    CHECK(sleeping && sleeping->aSleeping);
    CHECK_EQ(world.GetAwakeCount(), 0u);

    sRIGID_BODY *body = world.GetBody(handle);
    CHECK(body != 0);
    if(!body) return;
    body->aLinVel.x = 2.0f;
    world.WakeBody(handle);
    for(int i = 0; i < 30; ++i) world.Step(1.0f / 60.0f);
    const sRIGID_BODY *moved = world.GetBody(handle);
    CHECK(moved && moved->aPos.x > 0.2f);
}

TEST_CASE(physicsRemoveBody) {
    cPHYSICS_WORLD world;
    AddGround(world);
    const uint32_t a = world.AddBody(MakeSphereBody(eBODY_DYNAMIC, GNg::sVEC3{0, 1, 0}, 0.5f, 1.0f));
    const uint32_t b = world.AddBody(MakeSphereBody(eBODY_DYNAMIC, GNg::sVEC3{1, 1, 0}, 0.5f, 1.0f));
    CHECK_EQ(world.GetBodyCount(), 3u);
    CHECK(world.RemoveBody(a));
    CHECK_EQ(world.GetBodyCount(), 2u);
    CHECK(world.GetBody(a) == 0);
    CHECK(!world.RemoveBody(a)); // 重复删除不崩溃
    CHECK(!world.RemoveBody(99999u));
    for(int i = 0; i < 60; ++i) world.Step(1.0f / 60.0f); // 删除后继续步进不崩溃
    CHECK(world.GetBody(b) != 0);
}

TEST_CASE(physicsDeterministic) {
    sRIGID_BODY initial[3];
    std::vector<GNg::sVEC3> positionsA;
    std::vector<GNg::sVEC3> positionsB;
    cPHYSICS_WORLD worldA;
    cPHYSICS_WORLD worldB;
    for(int which = 0; which < 2; ++which) {
        cPHYSICS_WORLD &world = which == 0 ? worldA : worldB;
        std::vector<GNg::sVEC3> &out = which == 0 ? positionsA : positionsB;
        AddGround(world);
        const uint32_t h0 =
            world.AddBody(MakeBoxBody(eBODY_DYNAMIC, GNg::sVEC3{0.0f, 1.0f, 0.0f},
                                      GNg::sVEC3{0.5f, 0.5f, 0.5f}, 1.0f));
        const uint32_t h1 =
            world.AddBody(MakeSphereBody(eBODY_DYNAMIC, GNg::sVEC3{0.3f, 2.0f, 0.0f}, 0.4f, 1.0f));
        (void)initial;
        for(int i = 0; i < 240; ++i) world.Step(1.0f / 60.0f);
        out.clear();
        out.push_back(world.GetBody(h0)->aPos);
        out.push_back(world.GetBody(h1)->aPos);
    }
    CHECK_EQ(positionsA.size(), positionsB.size());
    for(size_t i = 0; i < positionsA.size(); ++i) {
        CHECK_EQ(std::memcmp(&positionsA[i], &positionsB[i], sizeof(GNg::sVEC3)), 0);
    }
}

TEST_CASE(physicsKinematicPushesDynamic) {
    cPHYSICS_WORLD world;
    world.SetGravity(GNg::sVEC3{0.0f, 0.0f, 0.0f});
    sRIGID_BODY pusher = MakeBoxBody(eBODY_KINEMATIC, GNg::sVEC3{0.0f, 0.0f, 0.0f},
                                     GNg::sVEC3{0.5f, 0.5f, 0.5f}, 0.0f);
    pusher.aLinVel.x = 2.0f;
    const uint32_t hp = world.AddBody(pusher);
    const uint32_t hs =
        world.AddBody(MakeSphereBody(eBODY_DYNAMIC, GNg::sVEC3{1.2f, 0.0f, 0.0f}, 0.5f, 1.0f));
    CHECK(hp != cPHYSICS_WORLD::INVALID_HANDLE && hs != cPHYSICS_WORLD::INVALID_HANDLE);

    for(int i = 0; i < 60; ++i) world.Step(1.0f / 60.0f); // 1 秒
    const sRIGID_BODY *sphere = world.GetBody(hs);
    const sRIGID_BODY *box = world.GetBody(hp);
    CHECK(sphere && box);
    if(!sphere || !box) return;
    CHECK(sphere->aPos.x > 1.2f); // 被运动学盒推动
    CHECK_NEAR(box->aLinVel.x, 2.0f, 1e-5f); // 运动学速度不被求解器改
}

TEST_CASE(physicsInvalidInput) {
    cPHYSICS_WORLD world;
    sRIGID_BODY bad = MakeSphereBody(eBODY_DYNAMIC, GNg::sVEC3{0, 1, 0}, 0.5f, 0.0f);
    CHECK_EQ(world.AddBody(bad), cPHYSICS_WORLD::INVALID_HANDLE); // 动态体必须 mass>0
    bad = MakeSphereBody(eBODY_DYNAMIC, GNg::sVEC3{0, 1, 0}, 0.0f, 1.0f);
    CHECK_EQ(world.AddBody(bad), cPHYSICS_WORLD::INVALID_HANDLE); // 半径必须>0
    bad = MakeBoxBody(eBODY_DYNAMIC, GNg::sVEC3{0, 1, 0}, GNg::sVEC3{0.0f, 0.5f, 0.5f}, 1.0f);
    CHECK_EQ(world.AddBody(bad), cPHYSICS_WORLD::INVALID_HANDLE); // 盒半尺寸必须>0

    world.Step(0.0f);
    world.Step(-1.0f);
    world.Step(std::nanf("")); // 都不应推进或崩溃
    CHECK_EQ(world.GetBodyCount(), 0u);
    CHECK(world.GetBody(0) == 0);
    CHECK(!world.RemoveBody(0));

    sRAY_HIT hit;
    CHECK(!world.Raycast(GNg::sVEC3{0, 0, 0}, GNg::sVEC3{0, 1, 0}, 10.0f, hit));
}

TEST_CASE(physicsStaticBodyNeverMoves) {
    cPHYSICS_WORLD world;
    const uint32_t ground = AddGround(world);
    world.AddBody(MakeSphereBody(eBODY_DYNAMIC, GNg::sVEC3{0.0f, 1.5f, 0.0f}, 0.5f, 2.0f));
    for(int i = 0; i < 300; ++i) world.Step(1.0f / 60.0f);
    const sRIGID_BODY *body = world.GetBody(ground);
    CHECK(body != 0);
    if(!body) return;
    CHECK_NEAR(body->aPos.y, -0.25f, 1e-6f);
    CHECK_NEAR(body->aLinVel.y, 0.0f, 1e-6f);
    CHECK_NEAR(body->aAngVel.x, 0.0f, 1e-6f);
}

TEST_CASE(physicsRestitutionBounce) {
    cPHYSICS_WORLD world;
    sRIGID_BODY ground = MakeBoxBody(eBODY_STATIC, GNg::sVEC3{0.0f, -0.25f, 0.0f},
                                     GNg::sVEC3{5.0f, 0.25f, 5.0f}, 0.0f);
    ground.aRestitution = 0.8f;
    world.AddBody(ground);
    sRIGID_BODY ball = MakeSphereBody(eBODY_DYNAMIC, GNg::sVEC3{0.0f, 3.0f, 0.0f}, 0.5f, 1.0f);
    ball.aRestitution = 0.8f;
    const uint32_t handle = world.AddBody(ball);

    float maxUpSpeed = 0.0f;
    for(int i = 0; i < 240; ++i) {
        world.Step(1.0f / 60.0f);
        const sRIGID_BODY *body = world.GetBody(handle);
        if(body && body->aLinVel.y > maxUpSpeed) maxUpSpeed = body->aLinVel.y;
    }
    CHECK(maxUpSpeed > 1.0f); // 撞地后确实弹起
}

// ═══════════════════════════════════════════════════════════
//  胶囊（v1 第三形状）
// ═══════════════════════════════════════════════════════════

TEST_CASE(physicsCapsuleLiesOnGround) {
    cPHYSICS_WORLD world;
    AddGround(world);
    sRIGID_BODY capsule = MakeCapsuleBody(eBODY_DYNAMIC, GNg::sVEC3{0.0f, 1.2f, 0.0f}, 0.5f, 1.0f, 1.0f);
    GNg::sVEC3 zAxis{0.0f, 0.0f, 1.0f};
    GNg::QuatFromAxisAngle(&capsule.aRot, &zAxis, 1.5707963f); // 绕 Z 转 90°：轴变为水平（X 方向）
    const uint32_t handle = world.AddBody(capsule);
    CHECK(handle != cPHYSICS_WORLD::INVALID_HANDLE);

    for(int i = 0; i < 240; ++i) world.Step(1.0f / 60.0f);

    const sRIGID_BODY *body = world.GetBody(handle);
    CHECK(body != 0);
    if(!body) return;
    CHECK(AllFinite(body->aPos));
    CHECK_NEAR(body->aPos.y, 0.5f, 0.08f); // 水平躺倒时质心离地 = 半径
    CHECK(body->aSleeping);
}

TEST_CASE(physicsCapsuleDropsAndSettles) {
    cPHYSICS_WORLD world;
    AddGround(world);
    const uint32_t handle = world.AddBody(
        MakeCapsuleBody(eBODY_DYNAMIC, GNg::sVEC3{0.0f, 3.0f, 0.0f}, 0.3f, 0.8f, 1.0f));
    CHECK(handle != cPHYSICS_WORLD::INVALID_HANDLE);
    for(int i = 0; i < 300; ++i) world.Step(1.0f / 60.0f);
    const sRIGID_BODY *body = world.GetBody(handle);
    CHECK(body != 0);
    if(!body) return;
    CHECK(AllFinite(body->aPos));
    // 竖直站立时质心 1.1；倒下时 0.3；两者之间都算稳定落地
    CHECK(body->aPos.y > 0.25f && body->aPos.y < 1.2f);
    CHECK(std::fabs(body->aLinVel.y) < 0.1f);
}

TEST_CASE(physicsCapsuleRaycast) {
    cPHYSICS_WORLD world;
    const uint32_t handle =
        world.AddBody(MakeCapsuleBody(eBODY_STATIC, GNg::sVEC3{0.0f, 2.0f, 0.0f}, 0.3f, 0.8f, 0.0f));
    CHECK(handle != cPHYSICS_WORLD::INVALID_HANDLE);
    sRAY_HIT hit;
    CHECK(world.Raycast(GNg::sVEC3{0.0f, 10.0f, 0.0f}, GNg::sVEC3{0.0f, -1.0f, 0.0f}, 100.0f, hit));
    CHECK_EQ(hit.aIndex, handle);
    CHECK_NEAR(hit.aT, 10.0f - (2.0f + 0.8f + 0.3f), 0.1f); // 顶端半球
}

TEST_CASE(physicsCapsuleInvalidInput) {
    cPHYSICS_WORLD world;
    CHECK_EQ(world.AddBody(MakeCapsuleBody(eBODY_DYNAMIC, GNg::sVEC3{0, 1, 0}, 0.0f, 0.5f, 1.0f)),
             cPHYSICS_WORLD::INVALID_HANDLE);
    CHECK_EQ(world.AddBody(MakeCapsuleBody(eBODY_DYNAMIC, GNg::sVEC3{0, 1, 0}, 0.5f, -0.5f, 1.0f)),
             cPHYSICS_WORLD::INVALID_HANDLE);
    // halfHeight = 0：退化成球，合法
    CHECK(world.AddBody(MakeCapsuleBody(eBODY_DYNAMIC, GNg::sVEC3{0, 1, 0}, 0.5f, 0.0f, 1.0f)) !=
          cPHYSICS_WORLD::INVALID_HANDLE);
}
