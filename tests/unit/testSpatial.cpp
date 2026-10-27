// testSpatial.cpp — 3D 空间划分单测：AABB/射线/视锥/静态 BVH/动态 AABB 树，
// 随机对照全部使用固定种子，可重复运行。
#include "testFramework.h"

#include <GN.h>
#include "GN_Spatial.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <vector>

using namespace GN3d;

namespace {

const float kPi = 3.14159265358979f;

// 固定种子线性同余随机数：保证每次运行结果一致。
struct sLCG {
    uint32_t aState;

    void Seed(uint32_t value) { aState = value; }
    uint32_t NextU32() {
        aState = aState * 1664525u + 1013904223u;
        return aState;
    }
    float NextFloat() { return (float)(NextU32() >> 8) * (1.0f / 16777216.0f); }
    float Range(float low, float high) { return low + (high - low) * NextFloat(); }
};

GNg::sVEC3 MakeVec3(float x, float y, float z) {
    const GNg::sVEC3 v = {x, y, z};
    return v;
}

sAABB MakeRandomBox(sLCG &rng, float limit, float minHalf, float maxHalf) {
    const GNg::sVEC3 center = MakeVec3(rng.Range(-limit, limit), rng.Range(-limit, limit),
                                       rng.Range(-limit, limit));
    const GNg::sVEC3 extent = MakeVec3(rng.Range(minHalf, maxHalf), rng.Range(minHalf, maxHalf),
                                       rng.Range(minHalf, maxHalf));
    return sAABB::FromCenterExtent(center, extent);
}

// 与用例共用的测试相机：60° 透视 + RH LookAt，前视 -Z。
sFRUSTUM MakeTestFrustum(GNg::sMAT44 &viewProjOut) {
    GNg::sMAT44 projection;
    GNg::sMAT44 view;
    GNg::Mat44PerspectiveVK(&projection, 60.0f * kPi / 180.0f, 16.0f / 9.0f, 0.5f, 100.0f);
    const GNg::sVEC3 eye = MakeVec3(0.0f, 0.0f, 10.0f);
    const GNg::sVEC3 center = MakeVec3(0.0f, 0.0f, 0.0f);
    const GNg::sVEC3 up = MakeVec3(0.0f, 1.0f, 0.0f);
    GNg::Mat44LookAtRH(&view, &eye, &center, &up);
    GNg::Mat44Mul(&viewProjOut, &projection, &view);
    return sFRUSTUM::FromViewProj(viewProjOut);
}

float PlaneDistance(const GNg::sVEC4 &plane, const GNg::sVEC3 &point) {
    return plane.x * point.x + plane.y * point.y + plane.z * point.z + plane.w;
}

} // namespace

TEST_CASE(spatialAabbBasics) {
    sAABB empty;
    CHECK(!empty.IsValid());
    CHECK(empty.aMin.x == FLT_MAX && empty.aMax.x == -FLT_MAX);
    empty.Reset();
    CHECK(!empty.IsValid());

    sAABB box(MakeVec3(-1.0f, -2.0f, -3.0f), MakeVec3(4.0f, 5.0f, 6.0f));
    CHECK(box.IsValid());
    CHECK_NEAR(box.Center().x, 1.5f, 1e-5f); CHECK_NEAR(box.Extent().x, 2.5f, 1e-5f); CHECK_NEAR(box.Extent().y, 3.5f, 1e-5f);
    CHECK_NEAR(box.SurfaceArea(), 286.0f, 1e-3f);
    CHECK_NEAR(box.Volume(), 315.0f, 1e-3f);
    CHECK_EQ(box.LongestAxis(), 2);

    sAABB inverted(MakeVec3(1.0f, 0.0f, 0.0f), MakeVec3(0.0f, 1.0f, 1.0f));
    CHECK(!inverted.IsValid()); CHECK_NEAR(inverted.SurfaceArea(), 0.0f, 0.0f);

    // Expand(点)：空盒包一个点后退化为零体积盒，再包第二个点成为正常盒。
    sAABB grown;
    grown.Expand(MakeVec3(1.0f, 2.0f, 3.0f));
    CHECK(grown.IsValid());
    CHECK_NEAR(grown.aMin.x, 1.0f, 1e-6f);
    grown.Expand(MakeVec3(-1.0f, 0.0f, 5.0f));
    CHECK_NEAR(grown.aMin.x, -1.0f, 1e-6f); CHECK_NEAR(grown.aMin.z, 3.0f, 1e-6f); CHECK_NEAR(grown.aMax.z, 5.0f, 1e-6f);

    // Expand(盒) 与 Expand(pad)。
    sAABB unit(MakeVec3(-1.0f, -1.0f, -1.0f), MakeVec3(1.0f, 1.0f, 1.0f));
    sAABB other(MakeVec3(2.0f, 2.0f, 2.0f), MakeVec3(3.0f, 3.0f, 3.0f));
    sAABB unionBox;
    unionBox.Expand(unit);
    unionBox.Expand(other);
    CHECK_NEAR(unionBox.aMin.x, -1.0f, 1e-6f); CHECK_NEAR(unionBox.aMax.x, 3.0f, 1e-6f);
    sAABB padded = unit;
    padded.Expand(0.5f);
    CHECK_NEAR(padded.aMin.x, -1.5f, 1e-6f); CHECK_NEAR(padded.aMax.y, 1.5f, 1e-6f);
    CHECK_NEAR(padded.SurfaceArea(), 54.0f, 1e-4f); // 3x3x3

    // Overlaps：接触即重叠。
    sAABB touching(MakeVec3(1.0f, -1.0f, -1.0f), MakeVec3(2.0f, 1.0f, 1.0f));
    sAABB apart(MakeVec3(1.01f, -1.0f, -1.0f), MakeVec3(2.0f, 1.0f, 1.0f));
    CHECK(unit.Overlaps(touching));
    CHECK(!unit.Overlaps(apart));
    CHECK(unit.Overlaps(unit));
    CHECK(!empty.Overlaps(unit));

    // Contains：点/盒，边界算包含。
    CHECK(unit.Contains(MakeVec3(1.0f, 1.0f, 1.0f))); CHECK(unit.Contains(MakeVec3(0.0f, 0.0f, 0.0f)));
    CHECK(!unit.Contains(MakeVec3(1.01f, 0.0f, 0.0f))); CHECK(!empty.Contains(MakeVec3(0.0f, 0.0f, 0.0f)));
    sAABB inner(MakeVec3(-0.5f, -0.5f, -0.5f), MakeVec3(0.5f, 0.5f, 0.5f));
    CHECK(unit.Contains(inner)); CHECK(!inner.Contains(unit)); CHECK(!empty.Contains(inner));

    // FromCenterExtent。
    sAABB fromCenter = sAABB::FromCenterExtent(MakeVec3(1.0f, 2.0f, 3.0f),
                                               MakeVec3(1.0f, 1.0f, 1.0f));
    CHECK_NEAR(fromCenter.aMin.x, 0.0f, 1e-6f); CHECK_NEAR(fromCenter.aMin.y, 1.0f, 1e-6f); CHECK_NEAR(fromCenter.aMax.z, 4.0f, 1e-6f);
}

TEST_CASE(spatialAabbDegenerate) {
    // 零体积盒：合法、面积/体积为 0，能包含自身所在平面上的点。
    const GNg::sVEC3 point = MakeVec3(2.0f, 3.0f, 4.0f);
    sAABB zero(point, point);
    CHECK(zero.IsValid());
    CHECK_NEAR(zero.SurfaceArea(), 0.0f, 0.0f); CHECK_NEAR(zero.Volume(), 0.0f, 0.0f); CHECK_EQ(zero.LongestAxis(), 0);
    CHECK(zero.Contains(point));
    CHECK(zero.Overlaps(sAABB(MakeVec3(0.0f, 0.0f, 0.0f), MakeVec3(10.0f, 10.0f, 10.0f))));

    // 空盒的查询都不得崩溃且返回“无”。
    sAABB empty;
    CHECK(!empty.Contains(point));
    CHECK(!empty.Contains(zero));
    CHECK(!empty.Overlaps(empty));
    CHECK_NEAR(empty.SurfaceArea(), 0.0f, 0.0f); CHECK_NEAR(empty.Volume(), 0.0f, 0.0f); CHECK_EQ(empty.LongestAxis(), 0);

    // 最长轴：唯一次长轴也不会误判。
    sAABB flat(MakeVec3(-1.0f, -5.0f, -2.0f), MakeVec3(1.0f, 5.0f, 2.0f));
    CHECK_EQ(flat.LongestAxis(), 1);
}

TEST_CASE(spatialRayAabb) {
    const sAABB box(MakeVec3(-1.0f, -1.0f, -1.0f), MakeVec3(1.0f, 1.0f, 1.0f));
    float t = -1.0f;

    // 正命中：进入距离 = 4。
    CHECK(RayIntersectAABB(MakeVec3(0.0f, 0.0f, -5.0f), MakeVec3(0.0f, 0.0f, 1.0f), box, 10.0f, t));
    CHECK_NEAR(t, 4.0f, 1e-5f);

    // maxT 之外的命中不算；恰好等于 maxT 仍算命中（t > maxT 才未命中）。
    CHECK(!RayIntersectAABB(MakeVec3(0.0f, 0.0f, -5.0f), MakeVec3(0.0f, 0.0f, 1.0f), box, 3.9f, t)); CHECK(RayIntersectAABB(MakeVec3(0.0f, 0.0f, -5.0f), MakeVec3(0.0f, 0.0f, 1.0f), box, 4.0f, t));
    CHECK_NEAR(t, 4.0f, 1e-5f);

    // 背离盒子的射线；轴平行射线：偏移出板则未命中，落在板内则命中。
    CHECK(!RayIntersectAABB(MakeVec3(0.0f, 0.0f, 5.0f), MakeVec3(0.0f, 0.0f, 1.0f), box, 100.0f, t));
    CHECK(!RayIntersectAABB(MakeVec3(2.0f, 0.0f, -5.0f), MakeVec3(0.0f, 0.0f, 1.0f), box, 100.0f, t));
    CHECK(RayIntersectAABB(MakeVec3(-5.0f, 0.0f, 0.0f), MakeVec3(1.0f, 0.0f, 0.0f), box, 100.0f, t));
    CHECK_NEAR(t, 4.0f, 1e-5f);
    CHECK(RayIntersectAABB(MakeVec3(0.0f, -5.0f, 0.0f), MakeVec3(0.0f, 1.0f, 0.0f), box, 100.0f, t));
    CHECK_NEAR(t, 4.0f, 1e-5f);

    // 起点在盒内（含贴面）时进入距离为 0。
    bool originInside = false;
    CHECK(RayIntersectAABBInside(MakeVec3(0.0f, 0.0f, 0.0f), MakeVec3(0.3f, 0.4f, 0.5f), box,
                                 10.0f, t, &originInside));
    CHECK(originInside); CHECK_NEAR(t, 0.0f, 1e-6f);
    CHECK(RayIntersectAABBInside(MakeVec3(1.0f, 0.0f, 0.0f), MakeVec3(1.0f, 0.0f, 0.0f), box,
                                 10.0f, t, &originInside));
    CHECK(originInside); CHECK_NEAR(t, 0.0f, 1e-6f);
    CHECK(RayIntersectAABBInside(MakeVec3(0.0f, 0.0f, -5.0f), MakeVec3(0.0f, 0.0f, 1.0f), box,
                                 10.0f, t, &originInside));
    CHECK(!originInside); CHECK_NEAR(t, 4.0f, 1e-5f);

    // 零方向射线：起点在盒内命中 t=0，否则未命中。
    CHECK(RayIntersectAABB(MakeVec3(0.0f, 0.0f, 0.0f), MakeVec3(0.0f, 0.0f, 0.0f), box, 10.0f, t));
    CHECK_NEAR(t, 0.0f, 1e-6f);
    CHECK(!RayIntersectAABB(MakeVec3(5.0f, 0.0f, 0.0f), MakeVec3(0.0f, 0.0f, 0.0f), box, 10.0f, t));

    // 零体积盒：射线穿过该点时命中，进入距离即到达该点的距离。
    const sAABB pointBox(MakeVec3(2.0f, 2.0f, 2.0f), MakeVec3(2.0f, 2.0f, 2.0f));
    CHECK(RayIntersectAABB(MakeVec3(0.0f, 2.0f, 2.0f), MakeVec3(1.0f, 0.0f, 0.0f), pointBox, 10.0f, t));
    CHECK_NEAR(t, 2.0f, 1e-5f);

    // 空盒不崩溃，返回未命中。
    const sAABB empty;
    CHECK(!RayIntersectAABB(MakeVec3(0.0f, 0.0f, -5.0f), MakeVec3(0.0f, 0.0f, 1.0f), empty, 10.0f, t));
}

TEST_CASE(spatialFrustumPoint) {
    GNg::sMAT44 viewProj;
    const sFRUSTUM frustum = MakeTestFrustum(viewProj);
    for(int i = 0; i < 6; ++i) {
        const GNg::sVEC4 &plane = frustum.aPlanes[i];
        CHECK_NEAR(sqrtf(plane.x * plane.x + plane.y * plane.y + plane.z * plane.z), 1.0f, 1e-4f);
    }

    // 与“投影后 NDC 判定”逐点对照；贴边界的点跳过，避免浮点边界歧义。
    sLCG rng;
    rng.Seed(20261027u);
    int compared = 0;
    int insideCount = 0;
    for(int i = 0; i < 300; ++i) {
        const GNg::sVEC3 point = MakeVec3(rng.Range(-15.0f, 15.0f), rng.Range(-15.0f, 15.0f),
                                          rng.Range(-15.0f, 15.0f));
        GNg::sVEC4 point4(point, 1.0f);
        GNg::sVEC4 clip;
        GNg::Mat44TransformV4(&clip, &point4, &viewProj);
        if(std::fabs(clip.w) < 1e-3f) continue;

        bool ndcInside = false;
        bool ambiguous = false;
        if(clip.w > 0.0f) {
            const float ndcX = clip.x / clip.w;
            const float ndcY = clip.y / clip.w;
            const float ndcZ = clip.z / clip.w;
            const float margin = std::min(std::min(1.0f - std::fabs(ndcX), 1.0f - std::fabs(ndcY)),
                                          std::min(ndcZ, 1.0f - ndcZ));
            if(margin > 1e-3f) {
                ndcInside = true;
            } else if(margin > -1e-3f) {
                ambiguous = true;
            }
        }
        if(ambiguous) continue;

        float minDistance = FLT_MAX;
        for(int k = 0; k < 6; ++k) {
            const float distance = PlaneDistance(frustum.aPlanes[k], point);
            if(distance < minDistance) minDistance = distance;
        }
        if(std::fabs(minDistance) < 1e-3f) continue;

        CHECK_EQ(frustum.ContainsPoint(point), ndcInside);
        ++compared;
        if(ndcInside) ++insideCount;
    }
    CHECK(compared >= 200); CHECK(insideCount > 20); // 采样里确实同时覆盖了内、外两类
}

TEST_CASE(spatialFrustumAabb) {
    GNg::sMAT44 viewProj;
    const sFRUSTUM frustum = MakeTestFrustum(viewProj);

    // 与“存在一个平面使 8 个角点全在外侧”的暴力判据逐一对照。
    sLCG rng;
    rng.Seed(777u);
    int compared = 0;
    int outsideCount = 0;
    for(int i = 0; i < 300; ++i) {
        const sAABB box = MakeRandomBox(rng, 16.0f, 0.1f, 3.0f);
        bool expectIntersect = true;
        bool ambiguous = false;
        for(int k = 0; k < 6 && !ambiguous; ++k) {
            float maxDistance = -FLT_MAX;
            for(int corner = 0; corner < 8; ++corner) {
                const GNg::sVEC3 point = MakeVec3((corner & 1) ? box.aMax.x : box.aMin.x,
                                                  (corner & 2) ? box.aMax.y : box.aMin.y,
                                                  (corner & 4) ? box.aMax.z : box.aMin.z);
                const float distance = PlaneDistance(frustum.aPlanes[k], point);
                if(distance > maxDistance) maxDistance = distance;
            }
            if(std::fabs(maxDistance) < 1e-3f) ambiguous = true;
            if(maxDistance < 0.0f) expectIntersect = false;
        }
        if(ambiguous) continue;

        CHECK_EQ(frustum.IntersectsAABB(box), expectIntersect);
        ++compared;
        if(!expectIntersect) ++outsideCount;
    }
    CHECK(compared >= 200); CHECK(outsideCount > 20);
}

TEST_CASE(spatialFrustumPlaneOrder) {
    GNg::sMAT44 viewProj;
    const sFRUSTUM frustum = MakeTestFrustum(viewProj);

    // 契约固定顺序：左/右/下/上/近/远；Y 由 Mat44PerspectiveVK 翻转，故
    // 世界 +Y 越过的是 NDC 的“下”面（索引 2），世界 -Y 越过索引 3。
    const GNg::sVEC3 outsidePoint[6] = {
        MakeVec3(-100.0f, 0.0f, 0.0f), // 0 左
        MakeVec3(100.0f, 0.0f, 0.0f), // 1 右
        MakeVec3(0.0f, 100.0f, 0.0f), // 2 下（NDC 下方 = 世界 +Y）
        MakeVec3(0.0f, -100.0f, 0.0f), // 3 上
        MakeVec3(0.0f, 0.0f, 9.8f), // 4 近（比 nearZ 更近）
        MakeVec3(0.0f, 0.0f, -200.0f), // 5 远（超出 farZ）
    };
    for(int expect = 0; expect < 6; ++expect) {
        for(int k = 0; k < 6; ++k) {
            const float distance = PlaneDistance(frustum.aPlanes[k], outsidePoint[expect]);
            if(k == expect) {
                CHECK(distance < 0.0f);
            } else {
                CHECK(distance >= 0.0f);
            }
        }
        CHECK(!frustum.ContainsPoint(outsidePoint[expect]));
    }
}

TEST_CASE(spatialBvhBuildAndQueries) {
    sLCG rng;
    rng.Seed(4242u);
    const uint32_t count = 512;
    std::vector<sAABB> boxes(count);
    for(uint32_t i = 0; i < count; ++i) boxes[i] = MakeRandomBox(rng, 25.0f, 0.2f, 2.0f);

    cBVH bvh;
    bvh.Build(boxes.data(), count);
    CHECK(bvh.IsBuilt());
    CHECK_EQ(bvh.GetBoxCount(), count);
    CHECK(bvh.GetNodeCount() > 0);
    CHECK(bvh.GetNodeCount() < 2 * count + 8);

    // 包围盒查询：集合与暴力重叠集合完全相等，且结果升序。
    for(int query = 0; query < 40; ++query) {
        const sAABB queryBox = MakeRandomBox(rng, 26.0f, 0.5f, 5.0f);
        std::vector<uint32_t> got;
        bvh.QueryAABB(queryBox, got);
        std::vector<uint32_t> expected;
        for(uint32_t i = 0; i < count; ++i) {
            if(boxes[i].Overlaps(queryBox)) expected.push_back(i);
        }
        CHECK(std::is_sorted(got.begin(), got.end()));
        CHECK_EQ(got.size(), expected.size());
        for(size_t i = 0; i < got.size() && i < expected.size(); ++i) CHECK_EQ(got[i], expected[i]);
    }

    // 视锥查询：与逐盒视锥判定完全相等。
    GNg::sMAT44 viewProj;
    const sFRUSTUM frustum = MakeTestFrustum(viewProj);
    {
        std::vector<uint32_t> got;
        bvh.QueryFrustum(frustum, got);
        std::vector<uint32_t> expected;
        for(uint32_t i = 0; i < count; ++i) {
            if(frustum.IntersectsAABB(boxes[i])) expected.push_back(i);
        }
        CHECK(std::is_sorted(got.begin(), got.end()));
        CHECK_EQ(got.size(), expected.size());
        for(size_t i = 0; i < got.size() && i < expected.size(); ++i) CHECK_EQ(got[i], expected[i]);
        CHECK(!got.empty());
    }

    // 重建为较小子集后计数同步更新；Clear 后回到空状态（空结构查询见 spatialBvhEmpty）。
    bvh.Build(boxes.data(), 10);
    CHECK_EQ(bvh.GetBoxCount(), 10u);
    bvh.Clear();
    CHECK(!bvh.IsBuilt());
    CHECK_EQ(bvh.GetBoxCount(), 0u);
}

TEST_CASE(spatialBvhEmpty) {
    cBVH bvh;
    CHECK(!bvh.IsBuilt());
    CHECK_EQ(bvh.GetBoxCount(), 0u);
    CHECK_EQ(bvh.GetNodeCount(), 0u);

    GNg::sMAT44 viewProj;
    const sFRUSTUM frustum = MakeTestFrustum(viewProj);
    std::vector<uint32_t> out;
    bvh.QueryFrustum(frustum, out);
    CHECK(out.empty());
    bvh.QueryAABB(sAABB(MakeVec3(-1.0f, -1.0f, -1.0f), MakeVec3(1.0f, 1.0f, 1.0f)), out);
    CHECK(out.empty());

    sRAY_HIT hit;
    CHECK(!bvh.Raycast(MakeVec3(0.0f, 0.0f, -5.0f), MakeVec3(0.0f, 0.0f, 1.0f), 10.0f, hit));
    CHECK(!hit.aHit);

    // 空指针 / 零数量构建不得崩溃。
    bvh.Build(0, 128);
    CHECK(!bvh.IsBuilt());
    const sAABB one(MakeVec3(-1.0f, -1.0f, -1.0f), MakeVec3(1.0f, 1.0f, 1.0f));
    bvh.Build(&one, 0);
    CHECK(!bvh.IsBuilt());
}

TEST_CASE(spatialBvhRaycast) {
    sLCG rng;
    rng.Seed(31415u);
    const uint32_t count = 400;
    std::vector<sAABB> boxes(count);
    for(uint32_t i = 0; i < count; ++i) boxes[i] = MakeRandomBox(rng, 25.0f, 0.2f, 2.0f);

    cBVH bvh;
    bvh.Build(boxes.data(), count);

    const float maxT = 200.0f;
    for(int trial = 0; trial < 20; ++trial) {
        const GNg::sVEC3 origin = MakeVec3(rng.Range(-30.0f, 30.0f), rng.Range(-30.0f, 30.0f),
                                           rng.Range(-30.0f, 30.0f));
        // 一半射线瞄准某个盒中心，保证既有命中也有未命中。
        GNg::sVEC3 target;
        if(trial % 2 == 0) {
            target = boxes[rng.NextU32() % count].Center();
        } else {
            target = MakeVec3(rng.Range(-30.0f, 30.0f), rng.Range(-30.0f, 30.0f),
                              rng.Range(-30.0f, 30.0f));
        }
        GNg::sVEC3 dir = MakeVec3(target.x - origin.x, target.y - origin.y, target.z - origin.z);
        const float length = sqrtf(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
        if(length < 1e-4f) continue;
        dir = MakeVec3(dir.x / length, dir.y / length, dir.z / length);

        sRAY_HIT hit;
        const bool gotHit = bvh.Raycast(origin, dir, maxT, hit);

        float bestT = FLT_MAX;
        float nextT = FLT_MAX;
        uint32_t bestIndex = 0;
        for(uint32_t i = 0; i < count; ++i) {
            float t = 0.0f;
            if(!RayIntersectAABB(origin, dir, boxes[i], maxT, t)) continue;
            if(t < bestT - 1e-6f) {
                nextT = bestT;
                bestT = t;
                bestIndex = i;
            } else if(std::fabs(t - bestT) <= 1e-6f) {
                if(i < bestIndex) bestIndex = i;
            } else if(t < nextT) {
                nextT = t;
            }
        }
        const bool bruteHit = bestT < FLT_MAX;
        CHECK_EQ(gotHit, bruteHit); CHECK_EQ(hit.aHit, bruteHit);
        if(gotHit && bruteHit) {
            CHECK_NEAR(hit.aT, bestT, 1e-4f);
            if(nextT - bestT > 1e-3f) CHECK_EQ(hit.aIndex, bestIndex);
            CHECK_NEAR(hit.aPoint.x, origin.x + dir.x * bestT, 1e-3f);
            CHECK_NEAR(hit.aPoint.y, origin.y + dir.y * bestT, 1e-3f); CHECK_NEAR(hit.aPoint.z, origin.z + dir.z * bestT, 1e-3f);
        }
    }

    // 同 t 时必须取索引较小的盒子（含起点在盒内 t=0 的情形）。
    const sAABB duplicate(MakeVec3(-1.0f, -1.0f, -1.0f), MakeVec3(1.0f, 1.0f, 1.0f));
    const sAABB duplicates[3] = {duplicate, duplicate, duplicate};
    cBVH tieBvh;
    tieBvh.Build(duplicates, 3);
    sRAY_HIT tieHit;
    CHECK(tieBvh.Raycast(MakeVec3(0.0f, 0.0f, -5.0f), MakeVec3(0.0f, 0.0f, 1.0f), 100.0f, tieHit));
    CHECK_EQ(tieHit.aIndex, 0u);
    CHECK_NEAR(tieHit.aT, 4.0f, 1e-5f);
    CHECK(tieBvh.Raycast(MakeVec3(0.0f, 0.0f, 0.0f), MakeVec3(0.0f, 1.0f, 0.0f), 100.0f, tieHit));
    CHECK_EQ(tieHit.aIndex, 0u);
    CHECK_NEAR(tieHit.aT, 0.0f, 1e-6f);

    // 远离所有盒的射线：未命中且 hit 复位。
    CHECK(!bvh.Raycast(MakeVec3(1000.0f, 1000.0f, 1000.0f), MakeVec3(1.0f, 0.0f, 0.0f), 50.0f, tieHit)); CHECK(!tieHit.aHit);
}

TEST_CASE(spatialDynamicTreeRandomOps) {
    sLCG rng;
    rng.Seed(987654321u);
    cDYNAMIC_AABB_TREE tree;

    std::vector<uint32_t> live;
    std::vector<bool> used;
    std::vector<sAABB> boxes;
    std::vector<void *> users;
    const uint32_t maxLive = 110;
    int queryCount = 0;
    int opCount = 0;

    for(int step = 0; step < 350; ++step) {
        const uint32_t op = rng.NextU32() % 100u;
        const bool canInsert = live.size() < maxLive;
        if((op < 45u && canInsert) || live.empty()) {
            if(!canInsert) continue;
            const sAABB box = MakeRandomBox(rng, 20.0f, 0.1f, 3.0f);
            void *user = (void *)(uintptr_t)(0x1000u + (uint32_t)step * 4u);
            const uint32_t handle = tree.Insert(box, user);
            CHECK(handle != cDYNAMIC_AABB_TREE::INVALID_HANDLE);
            if(handle >= used.size()) {
                used.resize(handle + 1, false);
                boxes.resize(handle + 1);
                users.resize(handle + 1, 0);
            }
            CHECK(!used[handle]);
            used[handle] = true;
            boxes[handle] = box;
            users[handle] = user;
            live.push_back(handle);

            sAABB stored;
            CHECK(tree.GetBox(handle, stored)); CHECK_NEAR(stored.aMin.x, box.aMin.x, 1e-6f);
            CHECK_NEAR(stored.aMax.z, box.aMax.z, 1e-6f); CHECK(tree.GetUser(handle) == user);
        } else if(op < 75u) {
            const uint32_t handle = live[rng.NextU32() % (uint32_t)live.size()];
            const sAABB box = MakeRandomBox(rng, 22.0f, 0.1f, 4.0f);
            tree.Update(handle, box);
            boxes[handle] = box;
            sAABB stored;
            CHECK(tree.GetBox(handle, stored)); CHECK_NEAR(stored.aMin.y, box.aMin.y, 1e-6f);
            CHECK_NEAR(stored.aMax.y, box.aMax.y, 1e-6f);
        } else {
            const size_t pick = (size_t)(rng.NextU32() % (uint32_t)live.size());
            const uint32_t handle = live[pick];
            tree.Remove(handle);
            used[handle] = false;
            live[pick] = live.back();
            live.pop_back();
            sAABB stored;
            CHECK(!tree.GetBox(handle, stored));
            CHECK(tree.GetUser(handle) == 0);
        }
        ++opCount;
        CHECK_EQ(tree.GetCount(), (uint32_t)live.size());

        if(step % 20 == 0) {
            for(int query = 0; query < 5; ++query) {
                const sAABB queryBox = MakeRandomBox(rng, 24.0f, 0.5f, 6.0f);
                std::vector<uint32_t> expected;
                for(size_t i = 0; i < live.size(); ++i) {
                    if(boxes[live[i]].Overlaps(queryBox)) expected.push_back(live[i]);
                }
                std::vector<uint32_t> gotHandles;
                tree.QueryHandles(queryBox, gotHandles);
                std::sort(gotHandles.begin(), gotHandles.end());
                std::sort(expected.begin(), expected.end());
                CHECK_EQ(gotHandles.size(), expected.size());
                for(size_t i = 0; i < gotHandles.size() && i < expected.size(); ++i) {
                    CHECK_EQ(gotHandles[i], expected[i]);
                }

                std::vector<void *> gotUsers;
                tree.Query(queryBox, gotUsers);
                std::vector<uintptr_t> gotValues;
                std::vector<uintptr_t> expectedValues;
                for(size_t i = 0; i < gotUsers.size(); ++i) gotValues.push_back((uintptr_t)gotUsers[i]);
                for(size_t i = 0; i < expected.size(); ++i) {
                    expectedValues.push_back((uintptr_t)users[expected[i]]);
                }
                std::sort(gotValues.begin(), gotValues.end());
                std::sort(expectedValues.begin(), expectedValues.end());
                CHECK_EQ(gotValues.size(), expectedValues.size());
                for(size_t i = 0; i < gotValues.size() && i < expectedValues.size(); ++i) {
                    CHECK_EQ(gotValues[i], expectedValues[i]);
                }
                ++queryCount;
            }
        }
    }
    CHECK(queryCount > 0); CHECK(opCount >= 300);

    // 全部移除后树必须回到空状态。
    while(!live.empty()) {
        tree.Remove(live.back());
        live.pop_back();
    }
    CHECK_EQ(tree.GetCount(), 0u);
    const sAABB all(MakeVec3(-100.0f, -100.0f, -100.0f), MakeVec3(100.0f, 100.0f, 100.0f));
    std::vector<uint32_t> handles;
    tree.QueryHandles(all, handles);
    CHECK(handles.empty());
    std::vector<void *> usersOut;
    tree.Query(all, usersOut);
    CHECK(usersOut.empty());
}

TEST_CASE(spatialDynamicTreeInvalidHandles) {
    cDYNAMIC_AABB_TREE tree;
    const sAABB box(MakeVec3(-1.0f, -1.0f, -1.0f), MakeVec3(1.0f, 1.0f, 1.0f));

    // 空树查询不崩溃且返回空。
    std::vector<uint32_t> handles;
    std::vector<void *> users;
    tree.QueryHandles(box, handles);
    tree.Query(box, users);
    CHECK(handles.empty()); CHECK(users.empty());

    const uint32_t handle = tree.Insert(box, (void *)0x1);
    CHECK(handle != cDYNAMIC_AABB_TREE::INVALID_HANDLE);
    CHECK_EQ(tree.GetCount(), 1u);

    // 越界 / INVALID_HANDLE 上的所有操作都必须是安全无操作。
    tree.Update(cDYNAMIC_AABB_TREE::INVALID_HANDLE, box); tree.Remove(cDYNAMIC_AABB_TREE::INVALID_HANDLE);
    tree.Update(123456u, box);
    tree.Remove(123456u);
    CHECK_EQ(tree.GetCount(), 1u);

    sAABB out;
    out.Reset();
    CHECK(!tree.GetBox(cDYNAMIC_AABB_TREE::INVALID_HANDLE, out)); CHECK(!tree.GetBox(123456u, out));
    CHECK(tree.GetUser(cDYNAMIC_AABB_TREE::INVALID_HANDLE) == 0); CHECK(tree.GetUser(123456u) == 0);

    // 已删除句柄的重复操作。
    tree.Remove(handle); tree.Remove(handle);
    tree.Update(handle, box);
    CHECK_EQ(tree.GetCount(), 0u);
    CHECK(!tree.GetBox(handle, out));
    CHECK(tree.GetUser(handle) == 0);

    // Clear 之后旧句柄同样无效。
    const uint32_t again = tree.Insert(box, (void *)0x2); CHECK(again != cDYNAMIC_AABB_TREE::INVALID_HANDLE);
    tree.Clear();
    CHECK_EQ(tree.GetCount(), 0u);
    CHECK(!tree.GetBox(again, out));
    tree.QueryHandles(box, handles);
    CHECK(handles.empty());

    // 空盒（invalid）作为对象插入后，其它对象仍必须可被查询到。
    const uint32_t emptyHandle = tree.Insert(sAABB()); const uint32_t validHandle = tree.Insert(box);
    std::vector<uint32_t> found;
    tree.QueryHandles(box, found);
    CHECK(std::find(found.begin(), found.end(), validHandle) != found.end());
    CHECK(std::find(found.begin(), found.end(), emptyHandle) == found.end());
    tree.Remove(validHandle); tree.Remove(emptyHandle);
    CHECK_EQ(tree.GetCount(), 0u);

    // 单元素树：插入后立即移除，覆盖叶子即根的分支。
    const uint32_t single = tree.Insert(box); CHECK(single != cDYNAMIC_AABB_TREE::INVALID_HANDLE);
    CHECK(tree.GetUser(single) == 0);
    tree.Remove(single);
    CHECK_EQ(tree.GetCount(), 0u);
}
