// testAABBCache.cpp — 世界 AABB 的缓存优化：正确性对拍 + 失效规则
//
// 背景（B3 前置测量）：逐顶点算世界 AABB 在 1 万实体时单帧 7 ms（占 16.6 ms 预算 42%），
// 5 万实体 46 ms。优化成"局部 AABB 缓存 + 8 角点变换"后必须回答两件事：
//   1. 结果与逐顶点**完全一致**吗？（数学上应当逐位相同）
//   2. 变换/网格变了之后会正确失效吗？（缓存类优化最容易漏的就是这里）
#include "testFramework.h"

#include <GN.h>
#include "GN_3Dmodel.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <vector>

using namespace GN3d;

namespace {

// 独立的参考实现：逐顶点变换求世界 AABB（与优化前的代码同一口径）
bool referenceWorldAABB(const cMODEL &model, sAABB &out) {
    float minX = 1e30f, minY = 1e30f, minZ = 1e30f;
    float maxX = -1e30f, maxY = -1e30f, maxZ = -1e30f;
    bool any = false;
    for (uint32_t mi = 0; mi < model.aMeshCount; ++mi) {
        const sMESH &mesh = model.aMeshes[mi];
        if (mesh.aVertices == nullptr || mesh.aVertexCount == 0) continue;
        for (uint32_t vi = 0; vi < mesh.aVertexCount; ++vi) {
            const float *v = mesh.aVertices + vi * 11u;
            const GNg::sVEC3 local{v[0], v[1], v[2]};
            GNg::sVEC3 w;
            GNg::Mat44TransformCoord(&w, &local, &model.aTransform);
            if (w.x < minX) minX = w.x;
            if (w.y < minY) minY = w.y;
            if (w.z < minZ) minZ = w.z;
            if (w.x > maxX) maxX = w.x;
            if (w.y > maxY) maxY = w.y;
            if (w.z > maxZ) maxZ = w.z;
            any = true;
        }
    }
    if (!any) return false;
    out.aMin.x = minX;
    out.aMin.y = minY;
    out.aMin.z = minZ;
    out.aMax.x = maxX;
    out.aMax.y = maxY;
    out.aMax.z = maxZ;
    return true;
}

bool aabbNear(const sAABB &a, const sAABB &b, float eps) {
    return std::fabs(a.aMin.x - b.aMin.x) <= eps && std::fabs(a.aMin.y - b.aMin.y) <= eps &&
           std::fabs(a.aMin.z - b.aMin.z) <= eps && std::fabs(a.aMax.x - b.aMax.x) <= eps &&
           std::fabs(a.aMax.y - b.aMax.y) <= eps && std::fabs(a.aMax.z - b.aMax.z) <= eps;
}

const float kEps = 1e-4f;

} // namespace

// ── 1. 与逐顶点结果一致（各种变换）────────────────────
TEST_CASE(aabbCacheMatchesReferenceUnderTransforms) {
    struct sCase {
        GNg::sVEC3 pos;
        GNg::sQUAT rot;
        GNg::sVEC3 scale;
    };
    std::vector<sCase> cases;
    {
        sCase c;
        c.pos = GNg::sVEC3{0, 0, 0};
        GNg::QuatIdentity(&c.rot);
        c.scale = GNg::sVEC3{1, 1, 1};
        cases.push_back(c);
    }
    {
        sCase c;
        c.pos = GNg::sVEC3{3.5f, -2.0f, 7.25f};
        GNg::QuatIdentity(&c.rot);
        c.scale = GNg::sVEC3{2.0f, 0.5f, 1.5f};
        cases.push_back(c);
    }
    {
        sCase c;
        c.pos = GNg::sVEC3{-1.0f, 4.0f, 0.5f};
        const GNg::sVEC3 axis{0.0f, 1.0f, 0.0f};
        GNg::QuatFromAxisAngle(&c.rot, &axis, 0.7f); // 绕 Y 转 40°
        c.scale = GNg::sVEC3{1, 1, 1};
        cases.push_back(c);
    }
    {
        sCase c;
        c.pos = GNg::sVEC3{0.25f, 0.25f, -0.25f};
        const GNg::sVEC3 axis{0.577f, 0.577f, 0.577f}; // 任意轴
        GNg::QuatFromAxisAngle(&c.rot, &axis, 1.1f);
        c.scale = GNg::sVEC3{0.5f, 3.0f, 1.2f};
        cases.push_back(c);
    }

    for (size_t i = 0; i < cases.size(); ++i) {
        cMODEL *m = CreateCubeModel(1.0f, 0.5f, 0.5f, 0.5f);
        m->SetPosition(cases[i].pos.x, cases[i].pos.y, cases[i].pos.z);
        m->SetRotation(cases[i].rot);
        m->SetScale(cases[i].scale.x, cases[i].scale.y, cases[i].scale.z);
        m->UpdateTransform();

        sAABB ref;
        CHECK(referenceWorldAABB(*m, ref));
        CHECK(m->computeWorldAABB());
        CHECK(aabbNear(m->aWorldAABB, ref, kEps));
        delete m;
    }
}

// 球体网格（顶点更多，覆盖非立方体形状）
TEST_CASE(aabbCacheMatchesReferenceForSphere) {
    cMODEL *m = CreateSphereModel(1.0f, 16, 0.5f, 0.5f, 0.5f);
    m->SetPosition(2.0f, -1.0f, 3.0f);
    m->SetScale(1.5f, 0.8f, 1.1f);
    const GNg::sVEC3 axis{1.0f, 0.0f, 0.0f};
    GNg::sQUAT q;
    GNg::QuatFromAxisAngle(&q, &axis, 0.5f);
    m->SetRotation(q);
    m->UpdateTransform();

    sAABB ref;
    CHECK(referenceWorldAABB(*m, ref));
    CHECK(m->computeWorldAABB());
    CHECK(aabbNear(m->aWorldAABB, ref, kEps));
    delete m;
}

// ── 2. 变换改变后必须刷新 ─────────────────────────────
TEST_CASE(aabbCacheUpdatesAfterTransformChange) {
    cMODEL *m = CreateCubeModel(1.0f, 0.5f, 0.5f, 0.5f);
    m->SetPosition(0.0f, 0.0f, 0.0f);
    m->UpdateTransform();
    CHECK(m->computeWorldAABB());
    const float centerBefore = (m->aWorldAABB.aMin.x + m->aWorldAABB.aMax.x) * 0.5f;
    CHECK_NEAR(centerBefore, 0.0f, 1e-4);

    // 移动 10 m：AABB 中心必须跟着走
    m->SetPosition(10.0f, 0.0f, 0.0f);
    m->UpdateTransform();
    CHECK(m->computeWorldAABB());
    const float centerAfter = (m->aWorldAABB.aMin.x + m->aWorldAABB.aMax.x) * 0.5f;
    CHECK_NEAR(centerAfter, 10.0f, 1e-4);

    // 与参考实现一致
    sAABB ref;
    CHECK(referenceWorldAABB(*m, ref));
    CHECK(aabbNear(m->aWorldAABB, ref, kEps));
    delete m;
}

// 反复在"同一变换"上调用应当是幂等的（快速路径）
TEST_CASE(aabbCacheFastPathIsIdempotent) {
    cMODEL *m = CreateCubeModel(1.0f, 0.5f, 0.5f, 0.5f);
    m->SetPosition(1.0f, 2.0f, 3.0f);
    m->UpdateTransform();
    CHECK(m->computeWorldAABB());
    const sAABB first = m->aWorldAABB;
    for (int i = 0; i < 20; ++i) {
        CHECK(m->computeWorldAABB());
        CHECK(aabbNear(m->aWorldAABB, first, 0.0f)); // 逐位一致
    }
    delete m;
}

// ── 3. 换网格必须失效（缓存类优化最容易漏的地方）──────
//
// 缓存的键是「变换逐位相同 && 网格数相同」。换网格时若 meshCount 没变、
// 变换也没变，就必须靠 InvalidateLocalBounds() 显式失效 —— 这里验证它确实生效。
TEST_CASE(aabbCacheInvalidatedByMeshChange) {
    cMODEL *m = CreateCubeModel(1.0f, 0.5f, 0.5f, 0.5f);
    m->SetPosition(0.0f, 0.0f, 0.0f);
    m->UpdateTransform();
    CHECK(m->computeWorldAABB());
    const float sizeBefore = m->aWorldAABB.aMax.x - m->aWorldAABB.aMin.x;

    // 直接改网格顶点（模拟"换了网格数据但 meshCount 没变"）：
    // 缓存若不当场失效，这里就会拿到旧 AABB。
    CHECK(m->aMeshes != nullptr);
    CHECK(m->aMeshCount > 0u);
    for (uint32_t v = 0; v < m->aMeshes[0].aVertexCount; ++v) {
        float *vert = m->aMeshes[0].aVertices + v * 11u;
        vert[0] *= 4.0f; // 局部 X 放大 4 倍
    }
    m->InvalidateLocalBounds();
    CHECK(m->computeWorldAABB());
    const float sizeAfter = m->aWorldAABB.aMax.x - m->aWorldAABB.aMin.x;
    CHECK_NEAR(sizeAfter, sizeBefore * 4.0f, 1e-3f);

    // 与参考实现一致
    sAABB ref;
    CHECK(referenceWorldAABB(*m, ref));
    CHECK(aabbNear(m->aWorldAABB, ref, kEps));
    delete m;
}

// 缩放改变后尺寸必须跟着变（验证变换失效路径）
TEST_CASE(aabbCacheUpdatesAfterScaleChange) {
    cMODEL *m = CreateCubeModel(1.0f, 0.5f, 0.5f, 0.5f);
    m->SetScale(1.0f, 1.0f, 1.0f);
    m->UpdateTransform();
    CHECK(m->computeWorldAABB());
    const float s1 = m->aWorldAABB.aMax.x - m->aWorldAABB.aMin.x;

    m->SetScale(3.0f, 1.0f, 1.0f);
    m->UpdateTransform();
    CHECK(m->computeWorldAABB());
    const float s3 = m->aWorldAABB.aMax.x - m->aWorldAABB.aMin.x;
    CHECK_NEAR(s3, s1 * 3.0f, 1e-3f);
    delete m;
}

// ── 4. 无网格：返回 false 且空盒 ───────────────────────
TEST_CASE(aabbCacheEmptyModel) {
    cMODEL m;
    CHECK(!m.computeWorldAABB());
    // 空盒：min > max
    CHECK(m.aWorldAABB.aMin.x > m.aWorldAABB.aMax.x);
    m.InvalidateLocalBounds();
    CHECK(!m.computeWorldAABB());
}

// ── 5. 性能：快速路径应当显著快于首次计算 ──────────────
TEST_CASE(aabbCacheFastPathIsFaster) {
    const uint32_t count = 3000;
    std::vector<cMODEL *> models;
    models.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
        cMODEL *m = CreateCubeModel(1.0f, 0.5f, 0.5f, 0.5f);
        m->SetPosition((float)i * 0.1f, 0.0f, 0.0f);
        m->UpdateTransform();
        models.push_back(m);
    }

    auto timeIt = [&](int iters) {
        const auto t0 = std::chrono::high_resolution_clock::now();
        for (int it = 0; it < iters; ++it) {
            for (uint32_t i = 0; i < count; ++i) models[i]->computeWorldAABB();
        }
        const auto t1 = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::milli>(t1 - t0).count() / (double)iters;
    };

    // 首次（缓存冷，要算局部 AABB）
    const double cold = timeIt(1);
    // 热路径（变换没变，应当只比较 16 个 float）
    const double hot = timeIt(50);

    std::fprintf(stderr,
                 "[aabbCache] %u 模型：冷 %.3f ms / 热 %.3f ms（快 %.1fx）\n", count, cold, hot,
                 cold / (hot > 1e-9 ? hot : 1e-9));
    CHECK(hot < cold * 0.5); // 热路径至少快一倍

    for (size_t i = 0; i < models.size(); ++i) delete models[i];
}
