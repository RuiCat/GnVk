// testComputeCubeModel.cpp — 几何体生成器的世界 AABB 必须正确（P6 期间抓到并修掉的缺陷）。
//
// 历史：`CreateCubeModel` 曾把 6 面 × 4 顶点 = 24 个顶点写成 aVertexCount = 36，
// 于是顶点数组里后 12 个是**未初始化内存**，`computeWorldAABB()` 会算出 ±1e15 量级的垃圾盒。
// 这个缺陷不会崩、不会画错，只会让所有依赖 AABB 的逻辑（视锥剔除/实体排序/LOD 选级/
// 阴影拟合）静默失效 —— 正是"画面偶尔有东西不见/排序乱跳"那类最难查的问题。
// `tests/gpu/gpuCheckCull.cpp` 早就绕开了它（自己造立方体），但根因一直在。
//
// 现在断言的是**正确性**，不是"缺陷仍在"。
#include "testFramework.h"

#include <GN.h>
#include <cmath>
#include <cstdio>

TEST_CASE(computeCubeModelBoundsMatchSize) {
    const float sizes[3] = {1.0f, 2.0f, 0.5f};
    for (int i = 0; i < 3; ++i) {
        const float size = sizes[i];
        GN3d::cMODEL *model = GN3d::CreateCubeModel(size);
        CHECK(model != nullptr);
        CHECK_EQ(model->aMeshCount, 1u);
        const GN3d::sMESH &mesh = model->aMeshes[0];
        // 6 面 × 4 顶点 = 24；36 个索引（每面 2 三角）
        CHECK_EQ(mesh.aVertexCount, 24u);
        CHECK_EQ(mesh.aIndexCount, 36u);

        model->SetPosition(3.0f, -2.0f, 5.0f);
        CHECK(model->computeWorldAABB());
        const float half = size * 0.5f;
        CHECK_NEAR(model->aWorldAABB.aMin.x, 3.0f - half, 1e-4);
        CHECK_NEAR(model->aWorldAABB.aMin.y, -2.0f - half, 1e-4);
        CHECK_NEAR(model->aWorldAABB.aMin.z, 5.0f - half, 1e-4);
        CHECK_NEAR(model->aWorldAABB.aMax.x, 3.0f + half, 1e-4);
        CHECK_NEAR(model->aWorldAABB.aMax.y, -2.0f + half, 1e-4);
        CHECK_NEAR(model->aWorldAABB.aMax.z, 5.0f + half, 1e-4);

        // 所有顶点都在盒内（没有任何越界顶点）
        for (uint32_t v = 0; v < mesh.aVertexCount; ++v) {
            const float *p = mesh.aVertices + v * 11u;
            CHECK(std::fabs(p[0]) <= half + 1e-4f);
            CHECK(std::fabs(p[1]) <= half + 1e-4f);
            CHECK(std::fabs(p[2]) <= half + 1e-4f);
        }
        // 索引全部落在顶点范围内
        for (uint32_t k = 0; k < mesh.aIndexCount; ++k) {
            CHECK(mesh.aIndices[k] < mesh.aVertexCount);
        }
        delete model;
    }
}

TEST_CASE(computeSphereModelBoundsMatchRadius) {
    GN3d::cMODEL *model = GN3d::CreateSphereModel(2.0f, 8);
    CHECK(model != nullptr);
    model->SetPosition(1.0f, 4.0f, -3.0f);
    CHECK(model->computeWorldAABB());
    // 球面顶点在半径内（分段近似，容差放到 15%）
    CHECK(model->aWorldAABB.aMax.y <= 4.0f + 2.0f * 1.15f);
    CHECK(model->aWorldAABB.aMin.y >= 4.0f - 2.0f * 1.15f);
    CHECK(model->aWorldAABB.aMax.x <= 1.0f + 2.0f * 1.15f);
    CHECK(model->aWorldAABB.aMin.x >= 1.0f - 2.0f * 1.15f);
    delete model;
}
