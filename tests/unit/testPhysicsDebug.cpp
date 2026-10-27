// testPhysicsDebug.cpp — 物理调试线框：线数量口径、几何范围、接触点/法线标记。
#include "testFramework.h"

#include <GN.h>
#include "GN_Physics.h"

#include <cmath>
#include <cstddef>

using namespace GN3d;

namespace {

bool allFinite(const sDEBUG_DRAW &draw) {
    for(size_t i = 0; i < draw.aLines.size(); ++i) {
        const GNg::sVEC3 &start = draw.aLines[i].aStart;
        const GNg::sVEC3 &end = draw.aLines[i].aEnd;
        if(!std::isfinite(start.x) || !std::isfinite(start.y) || !std::isfinite(start.z)) return false;
        if(!std::isfinite(end.x) || !std::isfinite(end.y) || !std::isfinite(end.z)) return false;
    }
    return true;
}

float length(const GNg::sVEC3 &v) {
    return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
}

} // namespace

// 空世界 + 预置线段：DebugDraw 必须先清空收集器。
TEST_CASE(debugDrawEmptyWorld) {
    cPHYSICS_WORLD world;
    sDEBUG_DRAW draw;
    draw.addLine(GNg::sVEC3{0.0f, 0.0f, 0.0f}, GNg::sVEC3{1.0f, 1.0f, 1.0f});
    world.DebugDraw(draw);
    CHECK(draw.aLines.empty());
}

// 盒：12 条棱；端点包围盒等于半尺寸。
TEST_CASE(debugDrawBoxWireframe) {
    cPHYSICS_WORLD world;
    world.AddBody(MakeBoxBody(eBODY_STATIC, GNg::sVEC3{0.0f, 0.0f, 0.0f},
                              GNg::sVEC3{1.0f, 2.0f, 3.0f}, 0.0f));
    sDEBUG_DRAW draw;
    world.DebugDraw(draw);

    CHECK_EQ(draw.aLines.size(), 12u);
    CHECK(allFinite(draw));
    float maxX = 0.0f;
    float maxY = 0.0f;
    float maxZ = 0.0f;
    for(size_t i = 0; i < draw.aLines.size(); ++i) {
        const GNg::sVEC3 *points[2] = {&draw.aLines[i].aStart, &draw.aLines[i].aEnd};
        for(int p = 0; p < 2; ++p) {
            const float ax = std::fabs(points[p]->x);
            const float ay = std::fabs(points[p]->y);
            const float az = std::fabs(points[p]->z);
            if(ax > maxX) maxX = ax;
            if(ay > maxY) maxY = ay;
            if(az > maxZ) maxZ = az;
        }
    }
    CHECK_NEAR(maxX, 1.0f, 1e-4f);
    CHECK_NEAR(maxY, 2.0f, 1e-4f);
    CHECK_NEAR(maxZ, 3.0f, 1e-4f);
}

// 球：3 个正交圆 × 24 段 = 72 条；所有端点到球心距离 = 半径。
TEST_CASE(debugDrawSphereWireframe) {
    const GNg::sVEC3 center{1.0f, 2.0f, 3.0f};
    cPHYSICS_WORLD world;
    world.AddBody(MakeSphereBody(eBODY_STATIC, center, 0.5f, 0.0f));
    sDEBUG_DRAW draw;
    world.DebugDraw(draw);

    CHECK_EQ(draw.aLines.size(), 72u);
    CHECK(allFinite(draw));
    float minDistance = 1e30f;
    float maxDistance = -1e30f;
    for(size_t i = 0; i < draw.aLines.size(); ++i) {
        const GNg::sVEC3 *points[2] = {&draw.aLines[i].aStart, &draw.aLines[i].aEnd};
        for(int p = 0; p < 2; ++p) {
            GNg::sVEC3 delta;
            delta.x = points[p]->x - center.x;
            delta.y = points[p]->y - center.y;
            delta.z = points[p]->z - center.z;
            const float distance = length(delta);
            if(distance < minDistance) minDistance = distance;
            if(distance > maxDistance) maxDistance = distance;
        }
    }
    CHECK_NEAR(minDistance, 0.5f, 1e-3f);
    CHECK_NEAR(maxDistance, 0.5f, 1e-3f);
}

// 胶囊：2 个端圆 × 24 + 4 条侧线 = 52 条；端圆在圆柱段两端（Y = 中心 ± 半长），径向 = 半径。
TEST_CASE(debugDrawCapsuleWireframe) {
    cPHYSICS_WORLD world;
    world.AddBody(MakeCapsuleBody(eBODY_STATIC, GNg::sVEC3{0.0f, 2.0f, 0.0f}, 0.3f, 1.0f, 0.0f));
    sDEBUG_DRAW draw;
    world.DebugDraw(draw);

    CHECK_EQ(draw.aLines.size(), 52u);
    CHECK(allFinite(draw));
    float minY = 1e30f;
    float maxY = -1e30f;
    float maxRadial = 0.0f;
    for(size_t i = 0; i < draw.aLines.size(); ++i) {
        const GNg::sVEC3 *points[2] = {&draw.aLines[i].aStart, &draw.aLines[i].aEnd};
        for(int p = 0; p < 2; ++p) {
            if(points[p]->y < minY) minY = points[p]->y;
            if(points[p]->y > maxY) maxY = points[p]->y;
            const float radial = sqrtf(points[p]->x * points[p]->x + points[p]->z * points[p]->z);
            if(radial > maxRadial) maxRadial = radial;
        }
    }
    CHECK_NEAR(minY, 1.0f, 1e-3f); // 2 - 半长 1（端圆位置；半球顶不在线框里）
    CHECK_NEAR(maxY, 3.0f, 1e-3f); // 2 + 半长 1
    CHECK_NEAR(maxRadial, 0.3f, 1e-3f);
}

// 接触标记：两个重叠球（各 72 条）后有 1 个接触点 ⇒ 额外 4 条（十字 + 0.2 长法线）。
TEST_CASE(debugDrawContactMarkers) {
    cPHYSICS_WORLD world;
    world.SetGravity(GNg::sVEC3{0.0f, 0.0f, 0.0f});
    world.AddBody(MakeSphereBody(eBODY_STATIC, GNg::sVEC3{0.0f, 0.0f, 0.0f}, 0.5f, 0.0f));
    world.AddBody(MakeSphereBody(eBODY_DYNAMIC, GNg::sVEC3{0.8f, 0.0f, 0.0f}, 0.5f, 1.0f));
    world.Step(1.0f / 60.0f);
    CHECK(world.GetContactCount() >= 1u);

    sDEBUG_DRAW draw;
    world.DebugDraw(draw);
    CHECK_EQ(draw.aLines.size(), 72u * 2u + 4u * world.GetContactCount());
    CHECK(allFinite(draw));

    bool normalLineFound = false;
    for(size_t i = 0; i < draw.aLines.size(); ++i) {
        GNg::sVEC3 delta;
        delta.x = draw.aLines[i].aEnd.x - draw.aLines[i].aStart.x;
        delta.y = draw.aLines[i].aEnd.y - draw.aLines[i].aStart.y;
        delta.z = draw.aLines[i].aEnd.z - draw.aLines[i].aStart.z;
        if(std::fabs(length(delta) - 0.2f) < 1e-4f) normalLineFound = true;
    }
    CHECK(normalLineFound);
}
