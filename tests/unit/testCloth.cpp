// testCloth.cpp — 布料 XPBD：参数校验/拓扑、固定角、球碰撞、确定性、Reset、法线。
#include "testFramework.h"

#include <GN.h>
#include "GN_3Dcloth.h"

#include <cmath>
#include <cstring>

using namespace GN3d;

namespace {

const float kDt = 1.0f / 60.0f;

sCLOTH_DESC makeDesc(int nx, int ny) {
    sCLOTH_DESC desc;
    desc.aNx = nx;
    desc.aNy = ny;
    desc.aSpacing = 0.2f;
    desc.aMass = 0.1f;
    desc.aCompliance = 0.0f;
    desc.aSubsteps = 4;
    desc.aGravity = GNg::sVEC3{0.0f, -9.81f, 0.0f};
    desc.aWind = GNg::sVEC3{0.0f, 0.0f, 0.0f};
    desc.aPinCorners = true;
    return desc;
}

float vecLength(const GNg::sVEC3 &v) {
    return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
}

bool allFinite(const GNg::sVEC3 &v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

} // namespace

// 参数校验 + 拓扑计数 + 初始网格位置。
TEST_CASE(clothCreateValidationAndCounts) {
    cCLOTH cloth;
    CHECK(!cloth.isCreated());
    CHECK_EQ(cloth.vertexCount(), 0u);
    CHECK_EQ(cloth.triangleCount(), 0u);
    CHECK(cloth.positions() == 0);

    sCLOTH_DESC bad = makeDesc(1, 4);
    CHECK(!cloth.create(bad));
    bad = makeDesc(4, 4);
    bad.aSpacing = 0.0f;
    CHECK(!cloth.create(bad));
    bad = makeDesc(4, 4);
    bad.aMass = 0.0f;
    CHECK(!cloth.create(bad));
    bad = makeDesc(4, 4);
    bad.aCompliance = -1.0f;
    CHECK(!cloth.create(bad));

    cCLOTH grid;
    CHECK(grid.create(makeDesc(5, 4)));
    CHECK(grid.isCreated());
    CHECK_EQ(grid.vertexCount(), 20u);
    CHECK_EQ(grid.triangleCount(), 24u);
    const GNg::sVEC3 *positions = grid.positions();
    const uint32_t *indices = grid.indices();
    CHECK(positions != 0 && indices != 0);
    if(!positions || !indices) return;
    CHECK_NEAR(positions[0].x, 0.0f, 1e-6f);
    CHECK_NEAR(positions[0].y, 0.0f, 1e-6f);
    CHECK_NEAR(positions[4].x, 0.8f, 1e-6f); // 第 0 行第 4 列
    CHECK_NEAR(positions[5].y, 0.2f, 1e-6f); // 第 1 行第 0 列
    for(uint32_t i = 0; i < grid.triangleCount() * 3u; ++i) CHECK(indices[i] < grid.vertexCount());
}

// 四角固定：角点不动；有柔度时重力使中间下垂，侧风使布面离面偏移。
TEST_CASE(clothPinnedCornersStayAndSag) {
    cCLOTH cloth;
    sCLOTH_DESC desc = makeDesc(5, 5);
    desc.aCompliance = 0.02f; // 软布：允许在固定角之间下垂
    CHECK(cloth.create(desc));
    const GNg::sVEC3 *positions = cloth.positions();
    CHECK(positions != 0);
    if(!positions) return;
    const uint32_t cornerIndices[4] = {0u, 4u, 20u, 24u};
    const GNg::sVEC3 corners[4] = {positions[0], positions[4], positions[20], positions[24]};
    const GNg::sVEC3 centerBefore = positions[12];

    for(int i = 0; i < 120; ++i) cloth.step(kDt);
    CHECK(positions[12].y < centerBefore.y - 0.01f); // 重力方向下垂

    cloth.setWind(GNg::sVEC3{0.0f, 0.0f, 5.0f}); // 侧风把布面吹离 z=0 平面
    for(int i = 0; i < 120; ++i) cloth.step(kDt);
    CHECK(positions[12].z > 0.02f);
    CHECK(allFinite(positions[12]));

    for(int i = 0; i < 4; ++i) {
        CHECK(std::memcmp(&positions[cornerIndices[i]], &corners[i], sizeof(GNg::sVEC3)) == 0);
    }
}

// 球体碰撞：所有顶点被推到球面外（v1 球体代理）。
TEST_CASE(clothSphereColliderPushOut) {
    cCLOTH cloth;
    CHECK(cloth.create(makeDesc(5, 5)));
    const GNg::sVEC3 center{0.4f, 0.4f, 0.0f};
    const float radius = 0.15f;
    CHECK(cloth.addSphereCollider(center, radius));
    CHECK_EQ(cloth.colliderCount(), 1u);
    CHECK(!cloth.addSphereCollider(center, 0.0f));
    for(int i = 0; i < 120; ++i) cloth.step(kDt);

    const GNg::sVEC3 *positions = cloth.positions();
    CHECK(positions != 0);
    if(!positions) return;
    for(uint32_t i = 0; i < cloth.vertexCount(); ++i) {
        const GNg::sVEC3 delta{positions[i].x - center.x, positions[i].y - center.y,
                               positions[i].z - center.z};
        CHECK(vecLength(delta) >= radius - 1e-3f);
    }
}

// 确定性：同一参数两次模拟结果逐位相同。
TEST_CASE(clothDeterministicRepeat) {
    cCLOTH first;
    cCLOTH second;
    CHECK(first.create(makeDesc(4, 4)));
    CHECK(second.create(makeDesc(4, 4)));
    for(int i = 0; i < 90; ++i) {
        first.step(kDt);
        second.step(kDt);
    }
    const GNg::sVEC3 *a = first.positions();
    const GNg::sVEC3 *b = second.positions();
    CHECK(a != 0 && b != 0);
    if(!a || !b) return;
    CHECK(std::memcmp(a, b, sizeof(GNg::sVEC3) * first.vertexCount()) == 0);
}

// Reset：回到初始网格；碰撞球保留。
TEST_CASE(clothResetRestoresInitialGrid) {
    cCLOTH cloth;
    CHECK(cloth.create(makeDesc(4, 4)));
    CHECK(cloth.addSphereCollider(GNg::sVEC3{0.3f, 0.3f, 0.0f}, 0.1f));
    for(int i = 0; i < 60; ++i) cloth.step(kDt);
    cloth.reset();
    CHECK_EQ(cloth.colliderCount(), 1u);
    const GNg::sVEC3 *positions = cloth.positions();
    CHECK(positions != 0);
    if(!positions) return;
    for(int y = 0; y < 4; ++y) {
        for(int x = 0; x < 4; ++x) {
            const GNg::sVEC3 &position = positions[y * 4 + x];
            CHECK_NEAR(position.x, (float)x * 0.2f, 1e-6f);
            CHECK_NEAR(position.y, (float)y * 0.2f, 1e-6f);
            CHECK_NEAR(position.z, 0.0f, 1e-6f);
        }
    }
}

// 法线：Step 后有限且为单位长度。
TEST_CASE(clothNormalsUnitAfterStep) {
    cCLOTH cloth;
    CHECK(cloth.create(makeDesc(5, 5)));
    for(int i = 0; i < 30; ++i) cloth.step(kDt);
    const GNg::sVEC3 *normals = cloth.normals();
    CHECK(normals != 0);
    if(!normals) return;
    for(uint32_t i = 0; i < cloth.vertexCount(); ++i) {
        CHECK(allFinite(normals[i]));
        CHECK_NEAR(vecLength(normals[i]), 1.0f, 1e-3f);
    }
}
