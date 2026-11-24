// testScene.cpp — 场景级剔除 / LOD / 排序（P6）纯 CPU 单测。
//
// 覆盖 docs/ENTITY_CONTROLLERS_PLAN.md §5.1 P6 的验收口径：
//   * "1000 个实体的剔除结果与逐实体暴力判定一致"（本文件的核心用例）；
//   * LOD 选级策略（像素误差 + 屏幕占比）；
//   * 实体级排序：不透明近→远、透明远→近、透明永远在不透明之后。
#include "testFramework.h"

#include <GN.h>
#include "GN_Scene.h"

#include <cmath>
#include <cstring>
#include <vector>

using namespace GN3d;

namespace {

// 确定性伪随机（不用 <random>，保证跨平台逐位一致）
struct sLCG {
    uint32_t aState;
    explicit sLCG(uint32_t seed) : aState(seed ? seed : 1u) {}
    uint32_t next() {
        aState = aState * 1664525u + 1013904223u;
        return aState;
    }
    float unit() { return (float)(next() >> 8) / (float)(1u << 24); } // [0,1)
    float range(float low, float high) { return low + (high - low) * unit(); }
};

// 建一个立方体模型实体（每个实体一个 cMODEL，用完要删）
struct sTestEntity {
    cENTITY *aEntity;
    cMODEL *aModel;
};

// 自建盒模型：36 个顶点（6 面 × 2 三角形）全部显式写入。
// **不要用 CreateCubeModel**：那个函数的顶点数组是 new float[36*11] 但只填了前 24 个顶点，
// computeWorldAABB 会把后面 12 个未初始化顶点算进包围盒 —— 它自己的 AABB 就是垃圾。
// （这是既有缺陷，不属 P6 范围，但任何依赖 AABB 的用例都不能踩它。）
cMODEL *makeBoxModel(float size) {
    const float h = size * 0.5f;
    cMODEL *model = new cMODEL();
    model->aMeshCount = 1;
    model->aMeshes = new sMESH[1];
    sMESH &mesh = model->aMeshes[0];
    mesh.aVertexCount = 36;
    mesh.aIndexCount = 36;
    mesh.aVertices = new float[36 * 11];
    mesh.aIndices = new uint32_t[36];
    for (int i = 0; i < 36; ++i) mesh.aIndices[i] = (uint32_t)i;
    // 6 个面，每面 4 角 + 法线
    const float nx[6] = {1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    const float ny[6] = {0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f};
    const float nz[6] = {0.0f, 0.0f, 0.0f, 0.0f, 1.0f, -1.0f};
    for (int face = 0; face < 6; ++face) {
        for (int corner = 0; corner < 6; ++corner) {
            const int vi = face * 6 + corner;
            // 每个面按象限铺 4 个角，(0,1,2) 与 (0,2,3)
            const int order[6] = {0, 1, 2, 0, 2, 3};
            const int idx = order[corner];
            const float sx = (idx == 1 || idx == 2) ? 1.0f : -1.0f;
            const float sy = (idx == 2 || idx == 3) ? 1.0f : -1.0f;
            float px = nx[face] * h;
            float py = ny[face] * h;
            float pz = nz[face] * h;
            if (nx[face] == 0.0f) px = sx * h;
            if (ny[face] == 0.0f) py = sy * h;
            if (nz[face] == 0.0f) pz = sx * h;
            float *v = mesh.aVertices + vi * 11;
            v[0] = px;
            v[1] = py;
            v[2] = pz;
            v[3] = nx[face];
            v[4] = ny[face];
            v[5] = nz[face];
            v[6] = 1.0f;
            v[7] = 0.0f;
            v[8] = 0.0f;
            v[9] = 0.0f;
            v[10] = 1.0f;
        }
    }
    return model;
}

sTestEntity makeCubeEntity(float x, float y, float z, float size = 1.0f) {
    sTestEntity out;
    out.aModel = makeBoxModel(size);
    out.aEntity = new cENTITY();
    out.aEntity->aMesh.SetModel(out.aModel);
    out.aModel->SetPosition(x, y, z);
    out.aModel->computeWorldAABB();
    return out;
}

void destroyEntity(sTestEntity &entity) {
    if (entity.aEntity != nullptr) {
        entity.aEntity->aMesh.SetModel(nullptr);
        delete entity.aEntity;
        entity.aEntity = nullptr;
    }
    delete entity.aModel;
    entity.aModel = nullptr;
}

sLOD_VIEW makeView(const GNg::sVEC3 &camera, float screenHeight = 800.0f) {
    sLOD_VIEW view;
    view.aScreenHeight = screenHeight;
    view.aFovYRad = 0.8f;
    view.aCamPos = camera;
    GNg::sMAT44 identity;
    GNg::Mat44Identity(&identity);
    view.aViewProj = identity;
    return view;
}

} // namespace

// ── 核心：1000 个实体的剔除 == 逐实体暴力判定 ───────────────
// 批量路径（cSCENE → cMESH_CONTROLLER::Prepare）与"手算 AABB + 视锥求交"必须完全一致。
// 这条用例的价值在于两者是**不同代码路径**：批量路径会刷新 AABB、读 aVisible、
// 走控制器开关；暴力路径直接用 cMODEL 的世界 AABB 与 sFRUSTUM。任何一边漂移都会红。
TEST_CASE(sceneCullMatchesBruteForce1000) {
    const int kCount = 1000;
    std::vector<sTestEntity> objects;
    objects.reserve((size_t)kCount);
    sLCG rng(20261124u);

    // 真相机（GN3d::cCAMERA 已有单测）：距离 20 m 看向原点，45° 视场
    cCAMERA camera;
    camera.Init();
    camera.SetFov(45.0f);
    camera.SetNF(0.1f, 200.0f);
    camera.SetOrbit(GNg::sVEC3{0.0f, 0.0f, 0.0f}, 20.0f, 0.0f, 0.1f);
    GNg::sMAT44 viewMatrix;
    GNg::sMAT44 projectionMatrix;
    GNg::sMAT44 viewProj;
    CHECK(camera.BuildViewProj(1.6f, &viewMatrix, &projectionMatrix, &viewProj));

    // 实体散布在 ±25 的立方体内（相机在 20 m 外：既有视锥内也有视锥外）
    for (int i = 0; i < kCount; ++i) {
        const float x = rng.range(-25.0f, 25.0f);
        const float y = rng.range(-25.0f, 25.0f);
        const float z = rng.range(-25.0f, 25.0f);
        objects.push_back(makeCubeEntity(x, y, z, rng.range(0.3f, 2.0f)));
    }

    cSCENE scene;
    for (int i = 0; i < kCount; ++i) CHECK(scene.Add(objects[(size_t)i].aEntity));
    CHECK_EQ(scene.Count(), (uint32_t)kCount);
    // 重复加入被忽略
    CHECK(!scene.Add(objects[0].aEntity));
    CHECK_EQ(scene.Count(), (uint32_t)kCount);

    // 视锥来自上面那台相机；两条路径读的是同一个 sFRUSTUM / 同一批 AABB。
    const sFRUSTUM frustum = sFRUSTUM::FromViewProj(viewProj);
    sLOD_VIEW view = makeView(camera.GetCoordV());
    view.aFovYRad = camera.aFovY * CAMERA_DEG_TO_RAD;
    view.aViewProj = viewProj;

    const uint32_t visible = scene.PrepareRender(view, &frustum);

    // 暴力判定：逐实体 computeWorldAABB + frustum 求交（不经过控制器）
    uint32_t bruteVisible = 0;
    std::vector<uint32_t> bruteIndices;
    for (int i = 0; i < kCount; ++i) {
        cMODEL *model = objects[(size_t)i].aModel;
        model->computeWorldAABB();
        if (frustum.IntersectsAABB(model->aWorldAABB)) {
            ++bruteVisible;
            bruteIndices.push_back((uint32_t)i);
        }
    }

    char detail[256];
    std::snprintf(detail, sizeof(detail), "%u 个实体：批量可见 %u / 暴力可见 %u（剔除 %u，拒绝 %u）",
                  (unsigned)kCount, visible, bruteVisible, scene.LastCulledCount(),
                  scene.LastRejectedCount());
    CHECK_EQ(visible, bruteVisible);

    // 逐项核对：批量结果里的每个实体，暴力判定也必须是可见；反之亦然。
    // （上面已比数量，这里比身份 —— 数量相同的两组也可能不是同一组。）
    std::vector<uint8_t> bruteMask((size_t)kCount, 0);
    for (size_t i = 0; i < bruteIndices.size(); ++i) bruteMask[bruteIndices[i]] = 1;
    uint32_t mismatched = 0;
    for (uint32_t i = 0; i < scene.VisibleCount(); ++i) {
        CHECK(scene.DrawItem(i).aEntity != nullptr);
    }
    // 逐实体确认可见性一致（对象是逐个 new 的，不能靠指针算术，只能线性反查）
    for (int i = 0; i < kCount; ++i) {
        const bool batchVisible = [&]() {
            for (uint32_t k = 0; k < scene.VisibleCount(); ++k) {
                if (scene.DrawItem(k).aEntity == objects[(size_t)i].aEntity) return true;
            }
            return false;
        }();
        if (batchVisible != (bruteMask[(size_t)i] != 0)) ++mismatched;
    }
    CHECK_EQ(mismatched, 0u);
    if (visible != bruteVisible || mismatched != 0) {
        std::fprintf(stderr, "  [scene] %s\n", detail);
    }
    // 视锥内应当有一部分、但不该是全部（否则用例没有区分力）
    CHECK(visible > 0);
    CHECK(visible < (uint32_t)kCount);

    for (size_t i = 0; i < objects.size(); ++i) destroyEntity(objects[i]);
}

// 关闭剔除时：全部可见（frustum 被忽略），但排序仍生效。
TEST_CASE(sceneCullDisabledShowsEverything) {
    std::vector<sTestEntity> objects;
    // 全部放在 x ∈ [50, 430]：远离以原点为中心的 NDC 立方体（identity viewProj）
    for (int i = 0; i < 20; ++i) {
        objects.push_back(makeCubeEntity(50.0f + (float)i * 20.0f, 0.0f, 0.0f));
    }
    cSCENE scene;
    for (size_t i = 0; i < objects.size(); ++i) scene.Add(objects[i].aEntity);

    GNg::sMAT44 identity;
    GNg::Mat44Identity(&identity);
    const sFRUSTUM frustum = sFRUSTUM::FromViewProj(identity);
    const sLOD_VIEW view = makeView(GNg::sVEC3{0.0f, 0.0f, 0.0f});

    CHECK_EQ(scene.PrepareRender(view, &frustum), 0u); // 全部被剔除
    scene.SetCullEnabled(false);
    CHECK_EQ(scene.PrepareRender(view, &frustum), (uint32_t)objects.size()); // 全可见

    for (size_t i = 0; i < objects.size(); ++i) destroyEntity(objects[i]);
}

// ── 排序 ───────────────────────────────────────────────
TEST_CASE(sceneSortsOpaqueNearToFar) {
    std::vector<sTestEntity> objects;
    // 三个不透明立方体，沿 -z 由近到远（相机在原点看向 -z）
    objects.push_back(makeCubeEntity(0.0f, 0.0f, -5.0f));
    objects.push_back(makeCubeEntity(0.0f, 0.0f, -20.0f));
    objects.push_back(makeCubeEntity(0.0f, 0.0f, -10.0f));

    cSCENE scene;
    for (size_t i = 0; i < objects.size(); ++i) scene.Add(objects[i].aEntity);
    const sLOD_VIEW view = makeView(GNg::sVEC3{0.0f, 0.0f, 0.0f});
    CHECK_EQ(scene.PrepareRender(view, nullptr), 3u);

    // 期望顺序：z=-5（近）→ z=-10 → z=-20（远）
    CHECK(scene.DrawItem(0).aEntity == objects[0].aEntity);
    CHECK(scene.DrawItem(1).aEntity == objects[2].aEntity);
    CHECK(scene.DrawItem(2).aEntity == objects[1].aEntity);
    CHECK(scene.DrawItem(0).aDistance < scene.DrawItem(1).aDistance);
    CHECK(scene.DrawItem(1).aDistance < scene.DrawItem(2).aDistance);

    for (size_t i = 0; i < objects.size(); ++i) destroyEntity(objects[i]);
}

TEST_CASE(sceneSortsTransparentFarToNearAndLast) {
    std::vector<sTestEntity> objects;
    // 两个不透明（近、远）+ 两个透明（近、远）
    objects.push_back(makeCubeEntity(0.0f, 0.0f, -4.0f));  // 不透明 近
    objects.push_back(makeCubeEntity(0.0f, 0.0f, -30.0f)); // 不透明 远
    objects.push_back(makeCubeEntity(0.0f, 0.0f, -6.0f));  // 透明 近
    objects.push_back(makeCubeEntity(0.0f, 0.0f, -25.0f)); // 透明 远

    // 把 2、3 号标成透明（材质 alphaMode = BLEND）
    for (int i = 2; i <= 3; ++i) {
        sMESH &mesh = objects[(size_t)i].aModel->aMeshes[0];
        if (mesh.aMaterial == nullptr) {
            objects[(size_t)i].aModel->aMaterials = new sMATERIAL[1];
            objects[(size_t)i].aModel->aMaterialCount = 1;
            mesh.aMaterial = &objects[(size_t)i].aModel->aMaterials[0];
        }
        mesh.aMaterial->aAlphaMode = eALPHA_BLEND;
    }
    CHECK(objects[2].aEntity->aMesh.HasTransparentMaterial());
    CHECK(!objects[0].aEntity->aMesh.HasTransparentMaterial());

    cSCENE scene;
    for (size_t i = 0; i < objects.size(); ++i) scene.Add(objects[i].aEntity);
    const sLOD_VIEW view = makeView(GNg::sVEC3{0.0f, 0.0f, 0.0f});
    CHECK_EQ(scene.PrepareRender(view, nullptr), 4u);

    // 期望：不透明（近→远）= 0, 1；透明（远→近）= 3, 2
    CHECK(scene.DrawItem(0).aEntity == objects[0].aEntity);
    CHECK(scene.DrawItem(1).aEntity == objects[1].aEntity);
    CHECK(scene.DrawItem(2).aEntity == objects[3].aEntity);
    CHECK(scene.DrawItem(3).aEntity == objects[2].aEntity);
    // 全部透明都在不透明之后
    CHECK(!scene.DrawItem(0).aTransparent);
    CHECK(!scene.DrawItem(1).aTransparent);
    CHECK(scene.DrawItem(2).aTransparent);
    CHECK(scene.DrawItem(3).aTransparent);

    for (size_t i = 0; i < objects.size(); ++i) destroyEntity(objects[i]);
}

// 关闭排序：保持加入顺序（便于"只是想画画、不想改顺序"的调用方，也给对拍留口子）
TEST_CASE(sceneSortCanBeDisabled) {
    std::vector<sTestEntity> objects;
    objects.push_back(makeCubeEntity(0.0f, 0.0f, -20.0f)); // 远
    objects.push_back(makeCubeEntity(0.0f, 0.0f, -5.0f));  // 近
    cSCENE scene;
    scene.SetSortEnabled(false);
    for (size_t i = 0; i < objects.size(); ++i) scene.Add(objects[i].aEntity);
    const sLOD_VIEW view = makeView(GNg::sVEC3{0.0f, 0.0f, 0.0f});
    CHECK_EQ(scene.PrepareRender(view, nullptr), 2u);
    CHECK(scene.DrawItem(0).aEntity == objects[0].aEntity); // 加入顺序
    CHECK(scene.DrawItem(1).aEntity == objects[1].aEntity);
    for (size_t i = 0; i < objects.size(); ++i) destroyEntity(objects[i]);
}

// ── 成员管理 ───────────────────────────────────────────
TEST_CASE(sceneAddRemoveClear) {
    std::vector<sTestEntity> objects;
    for (int i = 0; i < 4; ++i) objects.push_back(makeCubeEntity((float)i, 0.0f, -5.0f));

    cSCENE scene;
    CHECK(!scene.Add(nullptr));
    for (size_t i = 0; i < objects.size(); ++i) CHECK(scene.Add(objects[i].aEntity));
    CHECK_EQ(scene.Count(), 4u);
    CHECK(scene.At(0) == objects[0].aEntity);
    CHECK(scene.At(99) == nullptr);

    CHECK(scene.Remove(objects[1].aEntity));
    CHECK(!scene.Remove(objects[1].aEntity)); // 已移除
    CHECK_EQ(scene.Count(), 3u);

    scene.Clear();
    CHECK_EQ(scene.Count(), 0u);
    CHECK_EQ(scene.VisibleCount(), 0u); // 清空后立即无绘制项

    for (size_t i = 0; i < objects.size(); ++i) destroyEntity(objects[i]);
}

// 没有网格的实体（纯逻辑实体）不参与绘制，也不算进剔除计数。
TEST_CASE(sceneIgnoresEntitiesWithoutModel) {
    sTestEntity withModel = makeCubeEntity(0.0f, 0.0f, -5.0f);
    cENTITY *logicOnly = new cENTITY(); // 没挂模型
    cSCENE scene;
    scene.Add(withModel.aEntity);
    scene.Add(logicOnly);

    const sLOD_VIEW view = makeView(GNg::sVEC3{0.0f, 0.0f, 0.0f});
    CHECK_EQ(scene.PrepareRender(view, nullptr), 1u);
    CHECK_EQ(scene.LastCulledCount(), 1u); // 只有带模型的参与
    CHECK(scene.DrawItem(0).aEntity == withModel.aEntity);
    CHECK(scene.DrawItem(99).aEntity == nullptr); // 越界安全

    delete logicOnly;
    destroyEntity(withModel);
}

// ── LOD 策略 ───────────────────────────────────────────
// 1000 个实体的场景也要能"真的选级"：这里用带 LOD 的网格验策略两半（误差 + 覆盖率）。
TEST_CASE(sceneLodPolicyCoverageForcesLod0) {
    // 造一个带两级 LOD 的网格：LOD1 的几何误差 0.5 m
    cMODEL *model = new cMODEL();
    model->aMeshCount = 1;
    model->aMeshes = new sMESH[1];
    sMESH &mesh = model->aMeshes[0];
    mesh.aVertexCount = 4;
    mesh.aIndexCount = 1; // 顶点数据不参与 LOD 选级，只用于 AABB
    mesh.aVertices = new float[4 * 11];
    std::memset(mesh.aVertices, 0, sizeof(float) * 4 * 11);
    for (int v = 0; v < 4; ++v) {
        mesh.aVertices[v * 11 + 0] = (v == 1 || v == 2) ? 1.0f : -1.0f;
        mesh.aVertices[v * 11 + 1] = (v >= 2) ? 1.0f : -1.0f;
    }
    mesh.aLodCount = 2;
    mesh.aLods = new sMESH_LOD[2];
    mesh.aLods[1].aMaxError = 0.5f;
    model->computeWorldAABB();

    cENTITY *entity = new cENTITY();
    entity->aMesh.SetModel(model);

    sLOD_VIEW view = makeView(GNg::sVEC3{0.0f, 0.0f, 30.0f}); // 30 m 外：误差允许降级
    // 远距离 + 关闭覆盖率修正 ⇒ 应降到 LOD1
    sLOD_POLICY policy;
    policy.aMaxScreenErrorPx = 1.0f;
    policy.aMinCoverageForLod0 = 0.0f;
    entity->aMesh.SetLodPolicy(policy);
    entity->PrepareRender(view, nullptr);
    const uint32_t farLevel = entity->aMesh.LodLevel(0);

    // 同样的距离，但把覆盖率阈值设为很小 ⇒ 强制 LOD0
    policy.aMinCoverageForLod0 = 0.0001f;
    entity->aMesh.SetLodPolicy(policy);
    entity->PrepareRender(view, nullptr);
    const uint32_t forcedLevel = entity->aMesh.LodLevel(0);

    char detail[200];
    std::snprintf(detail, sizeof(detail), "远距离 LOD=%u，覆盖率强制后=%u（覆盖率 %.4f）", farLevel,
                  forcedLevel, lodScreenCoverage(model->aWorldAABB, view));
    CHECK(forcedLevel == 0u); // 覆盖率修正一定会拉到 LOD0
    CHECK(farLevel >= forcedLevel); // 策略只会更精细，不会更粗

    // 近距离（相机贴脸）：默认策略本来就该是 LOD0
    sLOD_VIEW nearView = makeView(GNg::sVEC3{0.0f, 0.0f, 1.5f});
    policy.aMinCoverageForLod0 = 0.25f;
    entity->aMesh.SetLodPolicy(policy);
    entity->PrepareRender(nearView, nullptr);
    CHECK_EQ(entity->aMesh.LodLevel(0), 0u);

    entity->aMesh.SetModel(nullptr);
    delete entity;
    delete model;
}

TEST_CASE(sceneLodCoverageMonotonic) {
    // 覆盖率随距离单调不增（相机靠近 → 占比变大）
    sAABB box;
    box.aMin = GNg::sVEC3{-1.0f, -1.0f, -1.0f};
    box.aMax = GNg::sVEC3{1.0f, 1.0f, 1.0f};
    float previous = 2.0f;
    for (int i = 1; i <= 20; ++i) {
        const sLOD_VIEW view = makeView(GNg::sVEC3{0.0f, 0.0f, (float)i * 2.0f});
        const float coverage = lodScreenCoverage(box, view);
        CHECK(coverage <= previous + 1e-6f);
        CHECK(coverage >= 0.0f);
        CHECK(coverage <= 1.0f);
        previous = coverage;
    }
    // 相机在盒内：占满屏幕
    const sLOD_VIEW inside = makeView(GNg::sVEC3{0.0f, 0.0f, 0.0f});
    CHECK_NEAR(lodScreenCoverage(box, inside), 1.0f, 1e-6f);
    // 视图非法：0
    sLOD_VIEW bad;
    bad.aScreenHeight = 0.0f;
    bad.aFovYRad = 0.8f;
    CHECK_NEAR(lodScreenCoverage(box, bad), 0.0f, 1e-6);
}

// cSCENE 的 Update 会驱动所有实体（这里只验不崩 + 有在跑）
TEST_CASE(sceneUpdateDrivesEntities) {
    sTestEntity object = makeCubeEntity(0.0f, 10.0f, -5.0f);
    cPHYSICS_WORLD world;
    object.aEntity->aId = 1u;
    object.aEntity->aPhysics.SetWorld(&world);
    const uint32_t handle = world.AddBody(MakeSphereBody(eBODY_DYNAMIC,
                                                         GNg::sVEC3{0.0f, 10.0f, 0.0f}, 0.5f, 1.0f));
    CHECK(object.aEntity->aPhysics.AttachBody(1u, handle));

    cSCENE scene;
    scene.Add(object.aEntity);
    for (int i = 0; i < 30; ++i) scene.Update(1.0f / 60.0f);
    const sRIGID_BODY *body = world.GetBody(handle);
    CHECK(body != nullptr);
    CHECK(body->aPos.y < 10.0f); // 物理在跑
    destroyEntity(object);
}

// ── 1000 个实体的排序也是 O(n log n) 且必须稳定 ─────────
// 与剔除用例互补：那条比"画谁"，这条比"画的顺序"。距离相同的实体必须保持加入顺序
// （stable_sort），否则同帧两次调用的顺序会变 —— 透明物体闪烁的经典来源。
TEST_CASE(sceneSortStableForEqualDistances1000) {
    const int kCount = 1000;
    std::vector<sTestEntity> objects;
    objects.reserve((size_t)kCount);
    // 全部放在**同一个中心**、大小不同：AABB 中心相同 ⇒ 距离逐位相等，
    // 于是排序键完全相同，顺序只能由 stable_sort 保留"加入顺序"。
    // （放在圆上不行：sin/cos 让距离差 ~1e-6，那已经是不同键，先后由距离决定而非稳定性。）
    for (int i = 0; i < kCount; ++i) {
        objects.push_back(makeCubeEntity(0.0f, 0.0f, -20.0f, 0.5f + (float)(i % 7) * 0.1f));
    }
    cSCENE scene;
    for (int i = 0; i < kCount; ++i) scene.Add(objects[(size_t)i].aEntity);
    const sLOD_VIEW view = makeView(GNg::sVEC3{0.0f, 0.0f, 0.0f});
    CHECK_EQ(scene.PrepareRender(view, nullptr), (uint32_t)kCount);
    CHECK_EQ(scene.LastRejectedCount(), 0u); // 不剔除时一个都不该被拒

    // 距离相同的项必须按加入顺序出现（stable），而且两次调用的顺序完全一致。
    std::vector<cENTITY *> firstOrder;
    for (uint32_t i = 0; i < scene.VisibleCount(); ++i) {
        firstOrder.push_back(scene.DrawItem(i).aEntity);
    }
    scene.PrepareRender(view, nullptr);
    uint32_t orderChanged = 0;
    for (uint32_t i = 0; i < scene.VisibleCount(); ++i) {
        if (scene.DrawItem(i).aEntity != firstOrder[i]) ++orderChanged;
    }
    CHECK_EQ(orderChanged, 0u);

    // 等距项的相对顺序：按距离分桶后桶内必须保持加入顺序
    uint32_t unstable = 0;
    for (uint32_t i = 1; i < scene.VisibleCount(); ++i) {
        const sDRAW_ITEM &prev = scene.DrawItem(i - 1);
        const sDRAW_ITEM &curr = scene.DrawItem(i);
        if (std::fabs(prev.aDistance - curr.aDistance) > 1e-4f) continue;
        // 同距离：加入顺序靠前的必须排在前面
        uint32_t prevIndex = 0;
        uint32_t currIndex = 0;
        for (int k = 0; k < kCount; ++k) {
            if (objects[(size_t)k].aEntity == prev.aEntity) prevIndex = (uint32_t)k;
            if (objects[(size_t)k].aEntity == curr.aEntity) currIndex = (uint32_t)k;
        }
        if (prevIndex > currIndex) ++unstable;
    }
    CHECK_EQ(unstable, 0u);

    for (size_t i = 0; i < objects.size(); ++i) destroyEntity(objects[i]);
}
