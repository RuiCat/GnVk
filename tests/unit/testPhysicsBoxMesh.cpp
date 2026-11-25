// testPhysicsBoxMesh.cpp — 盒对静态三角网的碰撞（v1 补全"盒未实现"这个缺口）。
//
// 验收重点：
//
//   1. **盒子能停在三角网地面上**，高度 = 半高，且不会慢慢下沉/抖动。
//      这是"盒对地形"最常用的形态，也是这个功能存在的理由。
//   2. **不穿地**：无论初始位置多高、多少步，最低点都不能掉到板面下。
//   3. **斜坡上不会无限下滑**：有摩擦时盒子最终会停住（v1 的接触只有三角平面法线，
//      所以斜坡是靠摩擦停住的，这一条同时验证了摩擦真的接上了）。
//   4. **文档化的边界**：三角形尖刺扎进盒内的情形**不**产生接触 —— 与其假装支持，
//      不如用用例把这个边界钉住，将来能力提升时它会失败并提醒改文档。
#include "testFramework.h"

#include <GN.h>
#include "GN_Physics.h"
#include "GN_3Dmodel.h"

#include <cmath>
#include <cstdint>

using namespace GN3d;

namespace {

const float kDt = 1.0f / 60.0f;

void setVertex(float *vertices, uint32_t index, float x, float y, float z) {
    float *vertex = vertices + index * 11u;
    for(int i = 0; i < 11; ++i) vertex[i] = 0.0f;
    vertex[0] = x;
    vertex[1] = y;
    vertex[2] = z;
}

// y = height 的水平地面（2 个三角形）。法线朝 +Y（绕序 CCW，从上往下看）。
void buildFloor(sMESH &mesh, float size, float height) {
    mesh.aVertexCount = 4;
    mesh.aVertices = new float[4 * 11];
    setVertex(mesh.aVertices, 0, -size, height, -size);
    setVertex(mesh.aVertices, 1, size, height, -size);
    setVertex(mesh.aVertices, 2, -size, height, size);
    setVertex(mesh.aVertices, 3, size, height, size);
    mesh.aIndexCount = 6;
    mesh.aIndices = new uint32_t[6];
    // (0,2,3) 与 (0,3,1)：两条的法线都是 +Y（与布料模块的用例同一份几何）
    mesh.aIndices[0] = 0;
    mesh.aIndices[1] = 2;
    mesh.aIndices[2] = 3;
    mesh.aIndices[3] = 0;
    mesh.aIndices[4] = 3;
    mesh.aIndices[5] = 1;
}

// 绕 Z 轴倾斜 slopeRad 的斜坡，从原点附近铺开。
void buildRamp(sMESH &mesh, float size, float slopeRad) {
    const float c = std::cos(slopeRad);
    const float s = std::sin(slopeRad);
    const float k = 4.0f;
    mesh.aVertexCount = 4;
    mesh.aVertices = new float[4 * 11];
    // 四个角在 XZ 平面上取点，y 随 x 线性上升：y = x * tan(slope)
    const float xs[4] = {-size, size, -size, size};
    const float zs[4] = {-size, -size, size, size};
    for(int i = 0; i < 4; ++i) {
        setVertex(mesh.aVertices, (uint32_t)i, xs[i], xs[i] * s / c, zs[i]);
    }
    mesh.aIndexCount = 6;
    mesh.aIndices = new uint32_t[6];
    mesh.aIndices[0] = 0;
    mesh.aIndices[1] = 2;
    mesh.aIndices[2] = 3;
    mesh.aIndices[3] = 0;
    mesh.aIndices[4] = 3;
    mesh.aIndices[5] = 1;
    (void)k;
}

bool allFinite(const GNg::sVEC3 &v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

} // namespace

// ═══════════════════════════════════════════════════════════
//  一、盒停在三角网地面上
// ═══════════════════════════════════════════════════════════

TEST_CASE(boxMeshBoxRestsOnFloor) {
    cPHYSICS_WORLD world;
    {
        sMESH floor;
        buildFloor(floor, 10.0f, 0.0f);
        world.AddStaticMesh(floor);
    }
    CHECK_EQ(world.GetStaticTriangleCount(), 2u);
    const uint32_t handle = world.AddBody(MakeBoxBody(eBODY_DYNAMIC, GNg::sVEC3{0.0f, 2.0f, 0.0f},
                                                      GNg::sVEC3{0.4f, 0.4f, 0.4f}, 1.0f));
    CHECK(handle != cPHYSICS_WORLD::INVALID_HANDLE);
    for(int i = 0; i < 240; ++i) world.Step(kDt);
    const sRIGID_BODY *body = world.GetBody(handle);
    CHECK(body != 0);
    if(body == 0) return;
    CHECK(allFinite(body->aPos));
    // 落地后半高 0.4，允许一点位置修正余量
    CHECK_NEAR(body->aPos.y, 0.4f, 0.06f);
    CHECK(std::fabs(body->aLinVel.y) < 0.15f);
}

TEST_CASE(boxMeshDoesNotSinkOverTime) {
    // 跑更久（10 秒），确认不会慢慢下沉或抖动越来越厉害。
    cPHYSICS_WORLD world;
    {
        sMESH floor;
        buildFloor(floor, 10.0f, 0.0f);
        world.AddStaticMesh(floor);
    }
    const uint32_t handle = world.AddBody(MakeBoxBody(eBODY_DYNAMIC, GNg::sVEC3{0.0f, 1.0f, 0.0f},
                                                      GNg::sVEC3{0.3f, 0.3f, 0.3f}, 1.0f));
    for(int i = 0; i < 200; ++i) world.Step(kDt);
    const float early = world.GetBody(handle)->aPos.y;
    for(int i = 0; i < 400; ++i) world.Step(kDt);
    const float late = world.GetBody(handle)->aPos.y;
    CHECK(allFinite(world.GetBody(handle)->aPos));
    CHECK_NEAR(late, early, 0.02f); // 不继续下沉
    CHECK(late > 0.3f - 0.05f); // 没穿进板里
}

TEST_CASE(boxMeshNoTunnelingFromHighDrop) {
    // 从 20 米落下来：速度很快，但盒角一旦越过板面就会被推回去。
    cPHYSICS_WORLD world;
    {
        sMESH floor;
        buildFloor(floor, 10.0f, 0.0f);
        world.AddStaticMesh(floor);
    }
    const uint32_t handle = world.AddBody(MakeBoxBody(eBODY_DYNAMIC, GNg::sVEC3{0.0f, 20.0f, 0.0f},
                                                      GNg::sVEC3{0.5f, 0.5f, 0.5f}, 1.0f));
    for(int i = 0; i < 600; ++i) world.Step(kDt);
    const sRIGID_BODY *body = world.GetBody(handle);
    CHECK(allFinite(body->aPos));
    CHECK(body->aPos.y > 0.0f); // 没穿到板下
    CHECK(body->aPos.y < 2.0f); // 也没被弹飞
}

// ═══════════════════════════════════════════════════════════
//  二、斜坡（摩擦 + 平面法线）
// ═══════════════════════════════════════════════════════════

TEST_CASE(boxMeshSlidesDownRampWithoutFriction) {
    cPHYSICS_WORLD world;
    {
        sMESH ramp;
        buildRamp(ramp, 8.0f, 0.35f); // 约 20°
        world.AddStaticMesh(ramp);
    }
    sRIGID_BODY body = MakeBoxBody(eBODY_DYNAMIC, GNg::sVEC3{0.0f, 1.5f, 0.0f},
                                   GNg::sVEC3{0.3f, 0.3f, 0.3f}, 1.0f);
    body.aFriction = 0.0f; // 无摩擦：应该滑下去
    const uint32_t handle = world.AddBody(body);
    const float startX = world.GetBody(handle)->aPos.x;
    for(int i = 0; i < 180; ++i) world.Step(kDt);
    const sRIGID_BODY *result = world.GetBody(handle);
    CHECK(allFinite(result->aPos));
    // 斜坡朝 -X 方向下坡（y = x*tan，x 越小 y 越低）
    CHECK(result->aPos.x < startX - 0.5f);
}

TEST_CASE(boxMeshStopsOnRampWithFriction) {
    // 高摩擦 + 缓坡：应该基本停住（v1 只有平面法线，斜坡靠摩擦停住）。
    cPHYSICS_WORLD world;
    {
        sMESH ramp;
        buildRamp(ramp, 8.0f, 0.12f); // 约 7°，较缓
        world.AddStaticMesh(ramp);
    }
    sRIGID_BODY body = MakeBoxBody(eBODY_DYNAMIC, GNg::sVEC3{0.0f, 0.6f, 0.0f},
                                   GNg::sVEC3{0.3f, 0.3f, 0.3f}, 1.0f);
    body.aFriction = 1.0f;
    const uint32_t handle = world.AddBody(body);
    for(int i = 0; i < 180; ++i) world.Step(kDt);
    const float early = world.GetBody(handle)->aPos.x;
    for(int i = 0; i < 180; ++i) world.Step(kDt);
    const sRIGID_BODY *result = world.GetBody(handle);
    CHECK(allFinite(result->aPos));
    // 后 3 秒的位移要远小于自由下滑（不是"慢慢滑但也一直在滑"）
    CHECK(std::fabs(result->aPos.x - early) < 0.3f);
}

// ═══════════════════════════════════════════════════════════
//  三、查询接口 + 边界
// ═══════════════════════════════════════════════════════════

TEST_CASE(boxMeshQueryDetectsCornerPenetration) {
    // 直接问一句：盒子压在三角形上时有没有接触、法线朝向对不对。
    const GNg::sVEC3 a{-2.0f, 0.0f, -2.0f};
    const GNg::sVEC3 b{-2.0f, 0.0f, 2.0f};
    const GNg::sVEC3 c{2.0f, 0.0f, 2.0f};
    sRIGID_BODY box = MakeBoxBody(eBODY_DYNAMIC, GNg::sVEC3{0.0f, 0.2f, 0.0f},
                                  GNg::sVEC3{0.5f, 0.5f, 0.5f}, 1.0f);
    BodyUpdateWorldState(box);
    sCONTACT_MANIFOLD manifold;
    // 盒中心 y=0.2、半高 0.5 ⇒ 底面在 y=-0.3，穿进板里 0.3
    CHECK(queryBoxTriangle(box, a, b, c, manifold));
    CHECK(manifold.aCount > 0u);
    CHECK_NEAR(manifold.aNormal.y, 1.0f, 1.0e-3f); // 法线从三角网指向盒（+Y）
    // 穿透深度应该接近 0.3
    CHECK_NEAR(manifold.aPoints[0].aPenetration, 0.3f, 0.02f);

    // 盒子完全在板面上方：不该有接触
    sRIGID_BODY high = MakeBoxBody(eBODY_DYNAMIC, GNg::sVEC3{0.0f, 3.0f, 0.0f},
                                   GNg::sVEC3{0.5f, 0.5f, 0.5f}, 1.0f);
    BodyUpdateWorldState(high);
    sCONTACT_MANIFOLD none;
    CHECK_EQ(queryBoxTriangle(high, a, b, c, none), false);
    CHECK_EQ(none.aCount, 0u);

    // 盒子在三角形 footprint 之外（x=10）：最近点在三角形边上，不该被当成接触
    sRIGID_BODY outside = MakeBoxBody(eBODY_DYNAMIC, GNg::sVEC3{10.0f, -1.0f, 0.0f},
                                      GNg::sVEC3{0.5f, 0.5f, 0.5f}, 1.0f);
    BodyUpdateWorldState(outside);
    sCONTACT_MANIFOLD off;
    CHECK_EQ(queryBoxTriangle(outside, a, b, c, off), false);
}

TEST_CASE(boxMeshQueryRejectsDegenerateTriangle) {
    sRIGID_BODY box = MakeBoxBody(eBODY_DYNAMIC, GNg::sVEC3{0.0f, 0.0f, 0.0f},
                                  GNg::sVEC3{0.5f, 0.5f, 0.5f}, 1.0f);
    BodyUpdateWorldState(box);
    sCONTACT_MANIFOLD manifold;
    // 三点共线：没有法线，不能编一个出来
    const GNg::sVEC3 a{0.0f, 0.0f, 0.0f};
    const GNg::sVEC3 b{1.0f, 0.0f, 0.0f};
    const GNg::sVEC3 c{2.0f, 0.0f, 0.0f};
    CHECK_EQ(queryBoxTriangle(box, a, b, c, manifold), false);
}

TEST_CASE(boxMeshSpikeInsideBoxIsNotDetected) {
    // **文档化的边界**：三角形尖刺扎进盒体内部时，
    // 因为没有任何"盒角穿透三角形正面"，本实现不产生接触。
    // 这条用例存在的意义：一旦将来补上了这个能力，它会失败并提醒同步文档，
    // 而不是让文档继续写着"不支持"却其实已经支持了。
    const GNg::sVEC3 a{-0.1f, -5.0f, 0.0f};
    const GNg::sVEC3 b{0.1f, 5.0f, 0.0f};
    const GNg::sVEC3 c{0.0f, 0.0f, 0.1f};
    // 一根竖直的细长三角，穿过位于原点的盒子（半高 0.5）
    sRIGID_BODY box = MakeBoxBody(eBODY_DYNAMIC, GNg::sVEC3{0.0f, 0.0f, 0.0f},
                                  GNg::sVEC3{0.5f, 0.5f, 0.5f}, 1.0f);
    BodyUpdateWorldState(box);
    sCONTACT_MANIFOLD manifold;
    // 盒角都在三角形平面两侧之外（三角形法线朝 -Z 或 +Z，盒角 z=±0.5）
    const bool hit = queryBoxTriangle(box, a, b, c, manifold);
    CHECK_EQ(hit, false); // 当前实现的能力边界（见文件头）
}

// ═══════════════════════════════════════════════════════════
//  四、盒对盒仍然正常（回归守卫）
// ═══════════════════════════════════════════════════════════

TEST_CASE(boxMeshDynamicBoxLandsOnDynamicBox) {
    // 三角网分支的改动不能影响既有的盒-盒窄相。
    cPHYSICS_WORLD world;
    {
        sMESH floor;
        buildFloor(floor, 10.0f, 0.0f);
        world.AddStaticMesh(floor);
    }
    const uint32_t lower = world.AddBody(MakeBoxBody(eBODY_DYNAMIC, GNg::sVEC3{0.0f, 0.4f, 0.0f},
                                                     GNg::sVEC3{0.5f, 0.4f, 0.5f}, 1.0f));
    const uint32_t upper = world.AddBody(MakeBoxBody(eBODY_DYNAMIC, GNg::sVEC3{0.0f, 2.5f, 0.0f},
                                                     GNg::sVEC3{0.5f, 0.4f, 0.5f}, 1.0f));
    for(int i = 0; i < 300; ++i) world.Step(kDt);
    const sRIGID_BODY *low = world.GetBody(lower);
    const sRIGID_BODY *high = world.GetBody(upper);
    CHECK(allFinite(low->aPos));
    CHECK(allFinite(high->aPos));
    // 上盒要停在下盒之上（不是穿过去或叠在一起）
    CHECK(high->aPos.y > low->aPos.y + 0.5f);
}
