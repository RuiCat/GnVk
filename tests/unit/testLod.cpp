// testLod.cpp — P10 动态 LOD：误差公式、按距离选级、滞后防抖、单级回归与守卫。
#include "testFramework.h"

#include <GN.h>
#include "GN_3Dlod.h"

#include <cmath>
#include <cstring>

using namespace GN3d;

namespace {

const float kPi = 3.14159265358979323846f;

sLOD_VIEW makeView(float screenHeight, float fovYRad, float cameraZ) {
    sLOD_VIEW view;
    std::memset(&view, 0, sizeof(view));
    view.aScreenHeight = screenHeight;
    view.aFovYRad = fovYRad;
    view.aCamPos = GNg::sVEC3{0.0f, 0.0f, cameraZ};
    return view;
}

sAABB makeBox() { return sAABB(GNg::sVEC3{-0.01f, -0.01f, -0.01f}, GNg::sVEC3{0.01f, 0.01f, 0.01f}); }

// 构造带 errors[] 个级别的网格（只填误差，几何数据为空：选级不需要几何）。
void attachLods(sMESH &mesh, const float *errors, uint32_t count) {
    mesh.aLods = new sMESH_LOD[count];
    mesh.aLodCount = count;
    for(uint32_t i = 0; i < count; ++i) mesh.aLods[i].aMaxError = errors[i];
}

void detachLods(sMESH &mesh) {
    delete[] mesh.aLods;
    mesh.aLods = 0;
    mesh.aLodCount = 0;
}

} // namespace

// 误差公式：S = err * H / (2 * d * tan(fov/2))；err == 0 的级误差恒为 0。
TEST_CASE(lodErrorFormula) {
    const sAABB box = makeBox();
    const sLOD_VIEW view = makeView(1000.0f, kPi * 0.5f, 10.0f); // tan(45°) = 1
    sMESH_LOD lod;
    lod.aMaxError = 0.1f;
    const float expected = 0.1f * 1000.0f / (2.0f * (10.0f - 0.01f) * 1.0f);
    CHECK_NEAR(lodScreenError(lod, box, view), expected, 0.01);

    sMESH_LOD zeroError;
    zeroError.aMaxError = 0.0f;
    CHECK_NEAR(lodScreenError(zeroError, box, view), 0.0f, 1e-9);
}

// 按距离选级：err {0, 0.1, 1.0}、H=1000、fov=90° ⇒ d=10/100/1000 选 0/1/2。
TEST_CASE(lodSelectByDistance) {
    const float errors[3] = {0.0f, 0.1f, 1.0f};
    sMESH mesh;
    attachLods(mesh, errors, 3);
    const sAABB box = makeBox();
    CHECK_EQ(selectLod(mesh, box, makeView(1000.0f, kPi * 0.5f, 10.0f), 1.0f, 0), 0u);
    CHECK_EQ(selectLod(mesh, box, makeView(1000.0f, kPi * 0.5f, 100.0f), 1.0f, 0), 1u);
    CHECK_EQ(selectLod(mesh, box, makeView(1000.0f, kPi * 0.5f, 1000.0f), 1.0f, 0), 2u);
    // 阈值放大到 100 px 时，最近距离也可用最粗级。
    CHECK_EQ(selectLod(mesh, box, makeView(1000.0f, kPi * 0.5f, 10.0f), 100.0f, 0), 2u);
    detachLods(mesh);
}

// 滞后：在降档阈值带内往返 100 次，切级次数 <= 2（不抖动）。
TEST_CASE(lodHysteresisOscillation) {
    const float errors[2] = {0.0f, 0.1f};
    sMESH mesh;
    attachLods(mesh, errors, 2);
    const sAABB box = makeBox();

    uint32_t level = 0;
    uint32_t switches = 0;
    for(int i = 0; i < 100; ++i) {
        const float distance = (i % 2 == 0) ? 60.0f : 70.0f; // S1 在 0.83 ↔ 0.71 之间
        const uint32_t next = selectLod(mesh, box, makeView(1000.0f, kPi * 0.5f, distance), 1.0f,
                                        level);
        if(next != level) ++switches;
        level = next;
    }
    CHECK(switches <= 2u);
    CHECK_EQ(level, 1u); // 最终停在粗级
    detachLods(mesh);
}

// 相机在包围盒内/贴脸、FOV 或视口非法：误差为 +INF，不得降级。
TEST_CASE(lodGuards) {
    const float errors[2] = {0.0f, 1.0f};
    sMESH mesh;
    attachLods(mesh, errors, 2);
    const sAABB box = makeBox();

    CHECK_EQ(selectLod(mesh, box, makeView(1000.0f, kPi * 0.5f, 0.0f), 1.0f, 0), 0u);
    CHECK(!std::isfinite(lodScreenError(mesh.aLods[1], box, makeView(0.0f, kPi * 0.5f, 100.0f))));
    CHECK(!std::isfinite(lodScreenError(mesh.aLods[1], box, makeView(1000.0f, 0.0f, 100.0f))));
    CHECK(!std::isfinite(lodScreenError(mesh.aLods[1], box, makeView(1000.0f, 3.2f, 100.0f))));

    sAABB emptyBox; // 空盒：保守按最近距离 ⇒ +INF
    CHECK(!std::isfinite(lodScreenError(mesh.aLods[1], emptyBox, makeView(1000.0f, kPi * 0.5f, 100.0f))));
    detachLods(mesh);
}

// 单级回归：无 LOD / 只有一个 LOD 时选级恒为 0（既有单级绘制路径不变）。
TEST_CASE(lodSingleLevelRegression) {
    sMESH plain;
    const sAABB box = makeBox();
    const sLOD_VIEW view = makeView(1000.0f, kPi * 0.5f, 1000.0f);
    CHECK_EQ(selectLod(plain, box, view, 1.0f, 0), 0u);

    const float single[1] = {5.0f}; // 即使误差很大也只有 0 级
    attachLods(plain, single, 1);
    CHECK_EQ(selectLod(plain, box, view, 1.0f, 0), 0u);
    detachLods(plain);
}

// 模型层批量选级：每网格独立选级，句柄为空/容量不足时安全。
TEST_CASE(lodSelectForModel) {
    const float errors[2] = {0.0f, 1.0f};
    cMODEL model;
    model.aMeshes = new sMESH[1];
    model.aMeshCount = 1;
    model.aVisible = true;
    attachLods(model.aMeshes[0], errors, 2);
    model.aWorldAABB = makeBox();

    uint32_t levels[1] = {0};
    CHECK_EQ(selectLodForModel(model, makeView(1000.0f, kPi * 0.5f, 1000.0f), 1.0f, levels, 1), 1u);
    CHECK_EQ(levels[0], 1u);
    CHECK_EQ(selectLodForModel(model, makeView(1000.0f, kPi * 0.5f, 10.0f), 1.0f, levels, 1), 1u);
    CHECK_EQ(levels[0], 0u);
    CHECK_EQ(selectLodForModel(model, makeView(1000.0f, kPi * 0.5f, 10.0f), 1.0f, 0, 1), 0u);
    CHECK_EQ(selectLodForModel(model, makeView(1000.0f, kPi * 0.5f, 10.0f), 1.0f, levels, 0), 0u);

    detachLods(model.aMeshes[0]);
    delete[] model.aMeshes;
    model.aMeshes = 0;
    model.aMeshCount = 0;
}
