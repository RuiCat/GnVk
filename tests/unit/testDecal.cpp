// testDecal.cpp — 3D 地面贴花：容量/参数校验、寿命回收、永久贴花、池满覆盖最老、末段渐隐、渲染条目转换。
#include "testFramework.h"

#include <GN.h>
#include "GN_3Ddecal.h"

#include <cmath>
#include <cstring>
#include <limits>

using namespace GN3d;

namespace {

sDECAL makeDecal(float x, float z, float size, float life) {
    sDECAL decal;
    std::memset(&decal, 0, sizeof(decal));
    decal.aPosition = GNg::sVEC3{x, 0.0f, z};
    decal.aSize = size;
    decal.aColor = GNg::sVEC4(0.8f, 0.85f, 0.95f, 0.6f);
    decal.aLife = life;
    decal.aAge = 0.0f;
    decal.aKind = (uint32_t)DecalKind::SPOT;
    return decal;
}

} // namespace

TEST_CASE(decalCreateValidation) {
    cDECAL_SYSTEM system;
    CHECK(!system.create(0)); // 容量 0 非法
    CHECK(system.create(8));
    CHECK(system.capacity() == 8u);
    CHECK(system.activeCount() == 0u);
    CHECK(!system.create(4)); // 已创建：不允许静默重建
    CHECK(system.capacity() == 8u);
}

TEST_CASE(decalSpawnRejectsInvalid) {
    cDECAL_SYSTEM system;
    CHECK(system.create(4));
    CHECK(!system.spawn(makeDecal(0.0f, 0.0f, 0.0f, 1.0f))); // size == 0
    CHECK(!system.spawn(makeDecal(0.0f, 0.0f, -1.0f, 1.0f))); // size < 0
    CHECK(!system.spawn(makeDecal(0.0f, 0.0f, 0.5f, -1.0f))); // life < 0

    sDECAL badAlpha = makeDecal(0.0f, 0.0f, 0.5f, 1.0f);
    badAlpha.aColor.w = 1.5f;
    CHECK(!system.spawn(badAlpha));

    sDECAL badPosition = makeDecal(0.0f, 0.0f, 0.5f, 1.0f);
    badPosition.aPosition.x = std::numeric_limits<float>::quiet_NaN();
    CHECK(!system.spawn(badPosition));
    CHECK(system.activeCount() == 0u); // 非法输入不改变系统

    CHECK(system.spawn(makeDecal(0.0f, 0.0f, 0.5f, 1.0f)));
    CHECK(system.activeCount() == 1u);
}

TEST_CASE(decalLifetimeRecycles) {
    cDECAL_SYSTEM system;
    CHECK(system.create(4));
    CHECK(system.spawn(makeDecal(1.0f, 2.0f, 0.5f, 1.0f)));
    // update 把 dt 钳在 0.1 s（与粒子一致），所以推进 0.5 s 要分 5 步。
    for(int i = 0; i < 5; ++i) system.update(0.1f);
    CHECK(system.activeCount() == 1u);
    CHECK(system.alive(0));
    CHECK_NEAR(system.decals()[0].aAge, 0.5, 1e-6);
    for(int i = 0; i < 6; ++i) system.update(0.1f); // 总 1.1s > life 1.0s
    CHECK(system.activeCount() == 0u);
    CHECK(!system.alive(0));
    CHECK_NEAR(system.decalFadeAlpha(0), 0.0, 1e-6);
}

TEST_CASE(decalPermanentKeepsAlpha) {
    cDECAL_SYSTEM system;
    CHECK(system.create(2));
    CHECK(system.spawn(makeDecal(0.0f, 0.0f, 1.0f, 0.0f))); // life == 0 ⇒ 永久
    for(int i = 0; i < 100; ++i) system.update(0.1f);
    CHECK(system.activeCount() == 1u);
    CHECK_NEAR(system.decalFadeAlpha(0), 0.6, 1e-6);
}

TEST_CASE(decalOverwritesOldestWhenFull) {
    cDECAL_SYSTEM system;
    CHECK(system.create(3));
    CHECK(system.spawn(makeDecal(1.0f, 0.0f, 0.5f, 0.0f)));
    CHECK(system.spawn(makeDecal(2.0f, 0.0f, 0.5f, 0.0f)));
    CHECK(system.spawn(makeDecal(3.0f, 0.0f, 0.5f, 0.0f)));
    CHECK(system.activeCount() == 3u);
    // 第 4 个覆盖最老的槽 0（x == 1.0 的那条）。
    CHECK(system.spawn(makeDecal(4.0f, 0.0f, 0.5f, 0.0f)));
    CHECK(system.activeCount() == 3u);
    CHECK_NEAR(system.decals()[0].aPosition.x, 4.0, 1e-6);
    CHECK_NEAR(system.decals()[1].aPosition.x, 2.0, 1e-6);
    CHECK_NEAR(system.decals()[2].aPosition.x, 3.0, 1e-6);
}

TEST_CASE(decalFadeInLastSegment) {
    cDECAL_SYSTEM system;
    CHECK(system.create(2));
    CHECK(system.spawn(makeDecal(0.0f, 0.0f, 0.5f, 10.0f))); // alpha 0.6，渐隐段 = 最后 3s
    system.update(6.9f); // 钳到 0.1 ⇒ 需要手动多步
    for(int i = 0; i < 68; ++i) system.update(0.1f); // 合计 6.9s
    CHECK_NEAR(system.decalFadeAlpha(0), 0.6, 1e-4);
    system.update(0.1f); // 7.0s
    CHECK_NEAR(system.decalFadeAlpha(0), 0.6, 1e-4);
    for(int i = 0; i < 15; ++i) system.update(0.1f); // 8.5s ⇒ remaining 1.5 / 3.0
    CHECK_NEAR(system.decalFadeAlpha(0), 0.3, 1e-4);
}

TEST_CASE(decalGatherRenderItems) {
    cDECAL_SYSTEM system;
    CHECK(system.create(4));
    CHECK(system.spawn(makeDecal(0.0f, 0.0f, 0.5f, 10.0f)));
    CHECK(system.spawn(makeDecal(1.0f, 0.0f, 0.5f, 0.0f))); // 永久
    sDECAL_RENDER_ITEM items[4];
    std::memset(items, 0, sizeof(items));
    CHECK_EQ(gatherDecalRenderItems(system, items, 4), 2u);
    CHECK_NEAR(items[0].aPosition[0], 0.0, 1e-6);
    CHECK_NEAR(items[0].aColor[3], 0.6, 1e-6);
    CHECK_NEAR(items[1].aPosition[0], 1.0, 1e-6);
    // maxOut 截断
    CHECK_EQ(gatherDecalRenderItems(system, items, 1), 1u);
    // 空指针安全
    CHECK_EQ(gatherDecalRenderItems(system, 0, 4), 0u);
    // 清空后无条目
    system.clear();
    CHECK_EQ(gatherDecalRenderItems(system, items, 4), 0u);
}
