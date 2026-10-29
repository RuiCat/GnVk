// testCull.cpp — cMODEL 世界 AABB（顶点×变换）与视锥剔除（可见列表/上限/空模型）。
#include "testFramework.h"

#include <GN.h>
#include "GN_3Dmodel.h"

#include <cmath>

using namespace GN3d;

namespace {

void buildCube(cMODEL &model, float half) {
    model.aMeshes = new sMESH[1];
    model.aMeshCount = 1;
    sMESH &mesh = model.aMeshes[0];
    mesh.aVertexCount = 8;
    mesh.aVertices = new float[8 * 11];
    const float signs[8][3] = {{-1.0f, -1.0f, -1.0f}, {1.0f, -1.0f, -1.0f}, {-1.0f, 1.0f, -1.0f},
                               {1.0f, 1.0f, -1.0f},   {-1.0f, -1.0f, 1.0f}, {1.0f, -1.0f, 1.0f},
                               {-1.0f, 1.0f, 1.0f},   {1.0f, 1.0f, 1.0f}};
    for(uint32_t i = 0; i < 8; ++i) {
        float *vertex = mesh.aVertices + i * 11u;
        for(int k = 0; k < 11; ++k) vertex[k] = 0.0f;
        vertex[0] = signs[i][0] * half;
        vertex[1] = signs[i][1] * half;
        vertex[2] = signs[i][2] * half;
    }
    mesh.aIndexCount = 36;
    mesh.aIndices = new uint32_t[36];
    for(uint32_t i = 0; i < 36; ++i) mesh.aIndices[i] = 0;
}

GNg::sMAT44 identityMatrix() {
    GNg::sMAT44 matrix;
    matrix.Init(true);
    return matrix;
}

} // namespace

// 世界 AABB：顶点 × 变换；无网格返回 false 并保持空盒。
TEST_CASE(cullWorldAABB) {
    cMODEL model;
    buildCube(model, 0.5f);
    CHECK(model.computeWorldAABB());
    CHECK_NEAR(model.aWorldAABB.aMin.x, -0.5f, 1e-5f);
    CHECK_NEAR(model.aWorldAABB.aMax.y, 0.5f, 1e-5f);

    model.SetScale(2.0f, 2.0f, 2.0f);
    CHECK(model.computeWorldAABB());
    CHECK_NEAR(model.aWorldAABB.aMin.x, -1.0f, 1e-5f);
    CHECK_NEAR(model.aWorldAABB.aMax.z, 1.0f, 1e-5f);

    model.SetPosition(5.0f, 0.0f, 0.0f);
    CHECK(model.computeWorldAABB());
    CHECK_NEAR(model.aWorldAABB.aMin.x, 4.0f, 1e-5f);
    CHECK_NEAR(model.aWorldAABB.aMax.x, 6.0f, 1e-5f);

    cMODEL empty;
    CHECK(!empty.computeWorldAABB());
    CHECK(!empty.isVisibleIn(sFRUSTUM::FromViewProj(identityMatrix())));
}

// 视锥可见性：单位视锥（NDC x/y ∈ [-1,1]，z ∈ [0,1]）下位置/可见开关。
TEST_CASE(cullFrustumVisibility) {
    const sFRUSTUM frustum = sFRUSTUM::FromViewProj(identityMatrix());

    cMODEL origin;
    buildCube(origin, 0.5f);
    CHECK(origin.computeWorldAABB());
    CHECK(origin.isVisibleIn(frustum)); // z 跨过近平面 0

    cMODEL farRight;
    buildCube(farRight, 0.5f);
    farRight.SetPosition(10.0f, 0.0f, 0.0f);
    CHECK(farRight.computeWorldAABB());
    CHECK(!farRight.isVisibleIn(frustum));

    cMODEL behind;
    buildCube(behind, 0.5f);
    behind.SetPosition(0.0f, 0.0f, -5.0f); // 整个在近平面之后
    CHECK(behind.computeWorldAABB());
    CHECK(!behind.isVisibleIn(frustum));

    cMODEL inside;
    buildCube(inside, 0.2f);
    inside.SetPosition(0.0f, 0.0f, 0.4f); // z 0.2..0.6 完全在 [0,1] 内
    CHECK(inside.computeWorldAABB());
    CHECK(inside.isVisibleIn(frustum));

    inside.aVisible = false;
    CHECK(!inside.isVisibleIn(frustum));
}

// 可见列表：只写可见模型下标；支持上限与空指针。
TEST_CASE(cullVisibleList) {
    const sFRUSTUM frustum = sFRUSTUM::FromViewProj(identityMatrix());
    cMODEL visibleA;
    buildCube(visibleA, 0.5f);
    CHECK(visibleA.computeWorldAABB());
    cMODEL hidden;
    buildCube(hidden, 0.5f);
    hidden.SetPosition(10.0f, 0.0f, 0.0f);
    CHECK(hidden.computeWorldAABB());
    cMODEL visibleC;
    buildCube(visibleC, 0.2f);
    visibleC.SetPosition(0.0f, 0.0f, 0.4f);
    CHECK(visibleC.computeWorldAABB());

    const cMODEL *models[4] = {&visibleA, &hidden, &visibleC, 0};
    uint32_t out[4] = {0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu};
    const uint32_t count = cullModels(frustum, models, 4, out, 4);
    CHECK_EQ(count, 2u);
    CHECK_EQ(out[0], 0u);
    CHECK_EQ(out[1], 2u);

    uint32_t limited[1] = {0xFFFFFFFFu};
    CHECK_EQ(cullModels(frustum, models, 4, limited, 1), 1u);
    CHECK_EQ(limited[0], 0u);

    CHECK_EQ(cullModels(frustum, models, 4, out, 0), 0u); // maxOut=0
    CHECK_EQ(cullModels(frustum, 0, 4, out, 4), 0u); // 空数组
}

// 剔除绘制：复用可见列表，返回绘制数并写 outVisible；无 GPU 时绘制是 no-op，只验计数。
TEST_CASE(cullDrawModelsPBRCount) {
    const sFRUSTUM frustum = sFRUSTUM::FromViewProj(identityMatrix());
    cMODEL visibleA;
    buildCube(visibleA, 0.5f);
    CHECK(visibleA.computeWorldAABB());
    cMODEL hidden;
    buildCube(hidden, 0.5f);
    hidden.SetPosition(10.0f, 0.0f, 0.0f);
    CHECK(hidden.computeWorldAABB());
    cMODEL visibleC;
    buildCube(visibleC, 0.2f);
    visibleC.SetPosition(0.0f, 0.0f, 0.4f);
    CHECK(visibleC.computeWorldAABB());

    cMODEL *models[4] = {&visibleA, &hidden, &visibleC, 0};
    uint32_t visible = 0xFFFFFFFFu;
    CHECK_EQ(cullDrawModelsPBR(frustum, models, 4, &visible), 2u);
    CHECK_EQ(visible, 2u);

    CHECK_EQ(cullDrawModelsPBR(frustum, models, 4, 0), 2u); // outVisible 可空
}

// 剔除绘制边界：空指针/空列表返回 0 且 outVisible 归零；全部可见时 outVisible 取上限 count。
TEST_CASE(cullDrawModelsPBREdgeCases) {
    const sFRUSTUM frustum = sFRUSTUM::FromViewProj(identityMatrix());
    uint32_t visible = 0xFFFFFFFFu;

    CHECK_EQ(cullDrawModelsPBR(frustum, 0, 4, &visible), 0u); // models 为空
    CHECK_EQ(visible, 0u);
    CHECK_EQ(cullDrawModelsPBR(frustum, 0, 0, 0), 0u); // 全空指针

    cMODEL allVisible;
    buildCube(allVisible, 0.5f);
    CHECK(allVisible.computeWorldAABB());
    cMODEL *models[1] = {&allVisible};
    CHECK_EQ(cullDrawModelsPBR(frustum, models, 1, &visible), 1u);
    CHECK_EQ(visible, 1u); // 上限 = count

    CHECK_EQ(cullDrawModelsPBR(frustum, models, 0, &visible), 0u); // count=0
    CHECK_EQ(visible, 0u);
}
