// testPhysicsMesh.cpp — 静态三角网：输入校验/退化剔除、球与胶囊在三角网地面上的静止、BVH 多三角形。
#include "testFramework.h"

#include <GN.h>
#include "GN_Physics.h"
#include "GN_3Dmodel.h"

#include <cmath>
#include <cstdint>

using namespace GN3d;

namespace {

const float kDt = 1.0f / 60.0f;

void setVertexPosition(float *vertices, uint32_t index, float x, float y, float z) {
    float *vertex = vertices + index * 11u;
    for(int i = 0; i < 11; ++i) vertex[i] = 0.0f;
    vertex[0] = x;
    vertex[1] = y;
    vertex[2] = z;
}

// 一块 [-size,size]²、y=height 的方形地面（2 个三角形）。
void buildQuadFloor(sMESH &mesh, float size, float height) {
    mesh.aVertexCount = 4;
    mesh.aVertices = new float[4 * 11];
    setVertexPosition(mesh.aVertices, 0, -size, height, -size);
    setVertexPosition(mesh.aVertices, 1, size, height, -size);
    setVertexPosition(mesh.aVertices, 2, -size, height, size);
    setVertexPosition(mesh.aVertices, 3, size, height, size);
    mesh.aIndexCount = 6;
    mesh.aIndices = new uint32_t[6];
    mesh.aIndices[0] = 0;
    mesh.aIndices[1] = 1;
    mesh.aIndices[2] = 2;
    mesh.aIndices[3] = 1;
    mesh.aIndices[4] = 3;
    mesh.aIndices[5] = 2;
}

// cells×cells 个四边形（2×cells² 个三角形）的地面。
void buildGridFloor(sMESH &mesh, int cells, float size, float height) {
    const int vertexSide = cells + 1;
    mesh.aVertexCount = (uint32_t)(vertexSide * vertexSide);
    mesh.aVertices = new float[mesh.aVertexCount * 11];
    for(int z = 0; z < vertexSide; ++z) {
        for(int x = 0; x < vertexSide; ++x) {
            const float px = -size + 2.0f * size * (float)x / (float)cells;
            const float pz = -size + 2.0f * size * (float)z / (float)cells;
            setVertexPosition(mesh.aVertices, (uint32_t)(z * vertexSide + x), px, height, pz);
        }
    }
    mesh.aIndexCount = (uint32_t)(cells * cells * 6);
    mesh.aIndices = new uint32_t[mesh.aIndexCount];
    uint32_t cursor = 0;
    for(int z = 0; z < cells; ++z) {
        for(int x = 0; x < cells; ++x) {
            const uint32_t i0 = (uint32_t)(z * vertexSide + x);
            const uint32_t i1 = i0 + 1u;
            const uint32_t i2 = i0 + (uint32_t)vertexSide;
            const uint32_t i3 = i2 + 1u;
            mesh.aIndices[cursor++] = i0;
            mesh.aIndices[cursor++] = i1;
            mesh.aIndices[cursor++] = i2;
            mesh.aIndices[cursor++] = i1;
            mesh.aIndices[cursor++] = i3;
            mesh.aIndices[cursor++] = i2;
        }
    }
}

bool allFinite(const GNg::sVEC3 &v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

} // namespace

// 输入校验：空网格、越界索引、退化三角形被忽略，合法三角形计数正确。
TEST_CASE(meshValidationAndDegenerateFilter) {
    cPHYSICS_WORLD world;
    sMESH empty;
    world.AddStaticMesh(empty);
    CHECK_EQ(world.GetStaticTriangleCount(), 0u);

    {
        sMESH mesh;
        buildQuadFloor(mesh, 1.0f, 0.0f);
        world.AddStaticMesh(mesh);
        CHECK_EQ(world.GetStaticTriangleCount(), 2u);
    }

    {
        sMESH mesh;
        mesh.aVertexCount = 3;
        mesh.aVertices = new float[3 * 11];
        setVertexPosition(mesh.aVertices, 0, 0.0f, 0.0f, 0.0f);
        setVertexPosition(mesh.aVertices, 1, 1.0f, 0.0f, 0.0f);
        setVertexPosition(mesh.aVertices, 2, 2.0f, 0.0f, 2.0f);
        mesh.aIndexCount = 6;
        mesh.aIndices = new uint32_t[6];
        mesh.aIndices[0] = 0;
        mesh.aIndices[1] = 1;
        mesh.aIndices[2] = 9; // 越界 ⇒ 跳过
        mesh.aIndices[3] = 0; // 退化三角形（0,0,2）⇒ 跳过
        mesh.aIndices[4] = 0;
        mesh.aIndices[5] = 2;
        world.AddStaticMesh(mesh);
        CHECK_EQ(world.GetStaticTriangleCount(), 2u); // 仍只有前面的合法地面
    }
}

// 球落在三角网地面上：静止高度 = 半径。
TEST_CASE(meshSphereRestsOnTriangles) {
    cPHYSICS_WORLD world;
    {
        sMESH floor;
        buildQuadFloor(floor, 5.0f, 0.0f);
        world.AddStaticMesh(floor);
    }
    CHECK_EQ(world.GetStaticTriangleCount(), 2u);
    const uint32_t handle =
        world.AddBody(MakeSphereBody(eBODY_DYNAMIC, GNg::sVEC3{1.0f, 2.0f, 1.0f}, 0.5f, 1.0f));
    CHECK(handle != cPHYSICS_WORLD::INVALID_HANDLE);
    for(int i = 0; i < 240; ++i) world.Step(kDt);

    const sRIGID_BODY *body = world.GetBody(handle);
    CHECK(body != 0);
    if(!body) return;
    CHECK(allFinite(body->aPos));
    CHECK_NEAR(body->aPos.y, 0.5f, 0.05f);
    CHECK(std::fabs(body->aLinVel.y) < 0.1f);
}

// 胶囊（横放）落在三角网地面上：静止高度 = 半径。
TEST_CASE(meshCapsuleRestsOnTriangles) {
    cPHYSICS_WORLD world;
    {
        sMESH floor;
        buildQuadFloor(floor, 5.0f, 0.0f);
        world.AddStaticMesh(floor);
    }
    sRIGID_BODY capsule = MakeCapsuleBody(eBODY_DYNAMIC, GNg::sVEC3{0.0f, 2.0f, 0.0f}, 0.2f, 0.5f, 1.0f);
    GNg::sVEC3 zAxis{0.0f, 0.0f, 1.0f};
    GNg::QuatFromAxisAngle(&capsule.aRot, &zAxis, 1.5707963f); // 轴向变为 X（横放）
    const uint32_t handle = world.AddBody(capsule);
    CHECK(handle != cPHYSICS_WORLD::INVALID_HANDLE);
    for(int i = 0; i < 240; ++i) world.Step(kDt);

    const sRIGID_BODY *body = world.GetBody(handle);
    CHECK(body != 0);
    if(!body) return;
    CHECK(allFinite(body->aPos));
    CHECK(body->aPos.y > 0.15f); // 半径 0.2 附近（允许姿态微调）
    CHECK(body->aPos.y < 0.4f);
}

// 多三角形 BVH：200 个三角形的地面，球仍能找到正确高度（宽相不漏）。
TEST_CASE(meshBvhFindsTrianglesInGrid) {
    cPHYSICS_WORLD world;
    {
        sMESH grid;
        buildGridFloor(grid, 10, 5.0f, 0.0f);
        world.AddStaticMesh(grid);
    }
    CHECK_EQ(world.GetStaticTriangleCount(), 200u);
    const uint32_t handle =
        world.AddBody(MakeSphereBody(eBODY_DYNAMIC, GNg::sVEC3{0.3f, 2.0f, 0.3f}, 0.5f, 1.0f));
    CHECK(handle != cPHYSICS_WORLD::INVALID_HANDLE);
    for(int i = 0; i < 240; ++i) world.Step(kDt);

    const sRIGID_BODY *body = world.GetBody(handle);
    CHECK(body != 0);
    if(!body) return;
    CHECK(allFinite(body->aPos));
    CHECK_NEAR(body->aPos.y, 0.5f, 0.05f);
}
