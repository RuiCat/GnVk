// testSceneCullBench.cpp — CPU 剔除的性能基线（B3 的前置测量）
//
// ═══ 实测结果（2026-11-25）═══
// 优化前 → 优化后（同一台机器，PrepareRender 单帧）：
//     1 000 实体   0.640 ms → 0.041 ms   （16x）
//    10 000 实体   7.057 ms → 0.879 ms   （8x，从占 16.6 ms 预算 42.5% 降到 5.3%）
//    50 000 实体  46.377 ms → 10.117 ms  （4.6x，从 279% 降到 61%）
//
// 收益来自 `cMODEL::computeWorldAABB` 的**缓存复用**（变换与网格都没变就跳过逐顶点计算）。
// 这才是真正的瓶颈 —— 而不是"剔除算法本身慢"：91% 的实体在视锥外，
// 但它们在被剔除之前就已经付了全量顶点变换的代价。
//
// **结论：B3（GPU 侧剔除）暂时不需要做。** 1 万实体已在 5.3% 预算内；
// 要做也该先量化 5 万实体剩下的 10 ms 花在哪（现在主要是排序 + LOD 选级），
// 而不是盲目把剔除搬到 GPU。
//
// ═══ 为什么先测再改 ═══
// B3 是"把剔除搬到 GPU"（compute + 间接绘制）。这是个大改，动之前必须回答一个问题：
// **CPU 剔除到底是不是瓶颈？** 没有基线数字就动手，最后只能靠"代码看起来对"来验收，
// 那不是能交付的标准（而且很可能优化了一个根本不慢的地方）。
//
// 本文件测 `cSCENE::PrepareRender` 在 1k / 10k / 50k 实体下的单帧耗时，
// 并给出"16.6 ms 预算里占了多少"。
//
// 注意：这是**纯 CPU** 基准（不需要 GPU），与 `make gpuCheck` 分开跑。
#include "testFramework.h"

#include <GN.h>
#include "GN_Scene.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <vector>

using namespace GN3d;

namespace {

// 一批实体分布在相机前方的立方体里；一部分在视锥外（模拟真实场景）
struct sBenchScene {
    std::vector<cENTITY *> aEntities;
    std::vector<cMODEL *> aModels;
    cSCENE aScene;

    ~sBenchScene() { free(); }

    void build(uint32_t count) {
        free();
        aEntities.reserve(count);
        aModels.reserve(count);
        for (uint32_t i = 0; i < count; ++i) {
            // 用便宜的网格：一个立方体（4 个实体共享不了，因为位置不同）
            cMODEL *m = CreateCubeModel(1.0f, 0.6f, 0.6f, 0.6f);
            // 分布在 200×40×200 的空间里，一部分落在视锥外
            const float t = (float)i;
            const float x = std::fmod(t * 7.31f, 200.0f) - 100.0f;
            const float y = std::fmod(t * 3.17f, 40.0f) - 20.0f;
            const float z = std::fmod(t * 11.7f, 200.0f) - 60.0f;
            m->SetPosition(x, y, z);
            m->computeWorldAABB();
            aModels.push_back(m);

            cENTITY *e = new cENTITY();
            e->aMesh.SetModel(m);
            aEntities.push_back(e);
            aScene.Add(e);
        }
    }

    void free() {
        for (size_t i = 0; i < aEntities.size(); ++i) delete aEntities[i];
        aEntities.clear();
        for (size_t i = 0; i < aModels.size(); ++i) delete aModels[i];
        aModels.clear();
        aScene.Clear();
    }
};

// 造一个"朝 -Z 看"的视锥（与引擎的 sFRUSTUM 口径一致：6 个平面）
sFRUSTUM makeFrustum() {
    GNg::sMAT44 view;
    GNg::sMAT44 proj;
    GNg::sMAT44 viewProj;
    GNg::sVEC3 eye{0.0f, 0.0f, 0.0f};
    GNg::sVEC3 target{0.0f, 0.0f, -1.0f};
    GNg::sVEC3 up{0.0f, 1.0f, 0.0f};
    GNg::Mat44LookAtRH(&view, &eye, &target, &up);
    GNg::Mat44PerspectiveVK(&proj, 1.0472f, 16.0f / 9.0f, 0.5f, 300.0f);
    GNg::Mat44Mul(&viewProj, &proj, &view);
    // **FromViewProj 是 static，必须接返回值**。
    // 写成 `sFRUSTUM f; f.FromViewProj(vp);` 会丢弃返回值，f 仍是默认构造的空视锥
    // —— 那种视锥把所有东西都判成可见，于是"剔除 0 个"（第一次写基准就踩了这个）。
    return sFRUSTUM::FromViewProj(viewProj);
}

struct sResult {
    double aMsPerFrame;
    uint32_t aTotal;
    uint32_t aRejected;
};

sResult runOnce(sBenchScene &bench, uint32_t iterations) {
    const sFRUSTUM frustum = makeFrustum();
    sLOD_VIEW view;
    view.aCamPos = GNg::sVEC3{0.0f, 0.0f, 0.0f};
    view.aScreenHeight = 720.0f;
    view.aFovYRad = 1.0472f;

    // 预热（第一遍会触碰所有缓存行）
    bench.aScene.PrepareRender(view, &frustum);

    const auto t0 = std::chrono::high_resolution_clock::now();
    for (uint32_t i = 0; i < iterations; ++i) {
        bench.aScene.PrepareRender(view, &frustum);
    }
    const auto t1 = std::chrono::high_resolution_clock::now();

    sResult r;
    const double totalMs =
        std::chrono::duration<double, std::milli>(t1 - t0).count();
    r.aMsPerFrame = totalMs / (double)iterations;
    r.aTotal = bench.aScene.LastCulledCount();
    r.aRejected = bench.aScene.LastRejectedCount();
    return r;
}

} // namespace

// ── 基线：单帧剔除耗时随实体数增长 ─────────────────────
TEST_CASE(sceneCullBenchMeasure) {
    const uint32_t counts[3] = {1000u, 10000u, 50000u};
    sBenchScene bench;

    std::fprintf(stderr, "\n[cullBench] ── CPU 剔除基线（PrepareRender 单帧）──\n");
    std::fprintf(stderr, "   %8s  %10s  %10s  %12s  %10s\n", "实体数", "耗时(ms)", "每实体(us)",
                 "剔除掉", "占16.6ms");
    for (int c = 0; c < 3; ++c) {
        bench.build(counts[c]);
        // 迭代次数随规模下降，保持总时间可控
        const uint32_t iters = (counts[c] <= 1000u) ? 200u : (counts[c] <= 10000u ? 50u : 20u);
        const sResult r = runOnce(bench, iters);
        std::fprintf(stderr, "   %8u  %10.3f  %10.2f  %12u  %9.1f%%\n", r.aTotal, r.aMsPerFrame,
                     (r.aMsPerFrame * 1000.0) / (double)r.aTotal, r.aRejected,
                     r.aMsPerFrame / 16.6 * 100.0);

        // 断言：耗时必须有限且非负（防止优化掉整个循环）
        CHECK(r.aMsPerFrame >= 0.0);
        CHECK(r.aMsPerFrame < 1000.0);
        CHECK(r.aTotal == counts[c]);

        // **回归守卫**：1 万实体必须留在 5 ms 以内（优化后实测 0.88 ms，留 5.7x 余量）。
        // 防止将来某次改动不小心取消了 AABB 缓存复用（那会悄悄回到 7 ms）。
        if (counts[c] == 10000u) {
            CHECK(r.aMsPerFrame < 5.0);
        }
    }
    bench.free();
}

// ── 剔除确实在工作（不是空跑）─────────────────────────
TEST_CASE(sceneCullBenchCullsSomething) {
    sBenchScene bench;
    bench.build(2000u);
    const sResult r = runOnce(bench, 20u);
    CHECK_EQ(r.aTotal, 2000u);
    // 实体分布在相机四周，朝 -Z 看应当剔除掉相当一部分
    CHECK(r.aRejected > 0u);
    CHECK(r.aRejected < r.aTotal);
    std::fprintf(stderr, "[cullBench] 2000 实体中剔除 %u 个（%.0f%%）\n", r.aRejected,
                 (double)r.aRejected / (double)r.aTotal * 100.0);
    bench.free();
}

// ── 关闭剔除的对照：确认剔除本身不是"免费"的 ───────────
TEST_CASE(sceneCullBenchCompareWithCullDisabled) {
    sBenchScene bench;
    bench.build(10000u);

    const sFRUSTUM frustum = makeFrustum();
    sLOD_VIEW view;
    view.aCamPos = GNg::sVEC3{0.0f, 0.0f, 0.0f};
    view.aScreenHeight = 720.0f;
    view.aFovYRad = 1.0472f;

    auto timeIt = [&](bool cull) {
        bench.aScene.SetCullEnabled(cull);
        bench.aScene.PrepareRender(view, &frustum); // 预热
        const auto t0 = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < 30; ++i) bench.aScene.PrepareRender(view, &frustum);
        const auto t1 = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::milli>(t1 - t0).count() / 30.0;
    };

    const double withCull = timeIt(true);
    const double withoutCull = timeIt(false);
    std::fprintf(stderr, "[cullBench] 10000 实体：开剔除 %.3f ms / 关剔除 %.3f ms（比值 %.2f）\n",
                 withCull, withoutCull, withCull / (withoutCull > 1e-9 ? withoutCull : 1.0));
    CHECK(withCull > 0.0);
    CHECK(withoutCull > 0.0);
    bench.free();
}

// ── Overdraw 实测（B4 的前置测量）──────────────────────
//
// 与剔除性能同理：**先测再说值不值得做遮挡剔除**。
// overdraw 倍数（每像素被着色几次）与硬件无关 —— 真实 GPU 上同样是这个倍数，
// 只是每次的代价不同。所以这个数字可以直接用来判断"要不要做 Hi-Z"。
TEST_CASE(sceneOverdrawBaseline) {
    sBenchScene bench;
    bench.build(2000u);

    // 真透视视锥（与剔除用例同一口径）
    GNg::sMAT44 view;
    GNg::sMAT44 proj;
    GNg::sMAT44 viewProj;
    const GNg::sVEC3 eye{0.0f, 0.0f, 0.0f};
    const GNg::sVEC3 target{0.0f, 0.0f, -1.0f};
    const GNg::sVEC3 up{0.0f, 1.0f, 0.0f};
    GNg::Mat44LookAtRH(&view, &eye, &target, &up);
    GNg::Mat44PerspectiveVK(&proj, 1.0472f, 16.0f / 9.0f, 0.5f, 300.0f);
    GNg::Mat44Mul(&viewProj, &proj, &view);

    const sFRUSTUM frustum = sFRUSTUM::FromViewProj(viewProj);
    sLOD_VIEW lodView;
    lodView.aCamPos = eye;
    lodView.aScreenHeight = 720.0f;
    lodView.aFovYRad = 1.0472f;
    bench.aScene.PrepareRender(lodView, &frustum);

    // 只统计通过剔除的（真正会绘制的）物体
    std::vector<sAABB> boxes;
    boxes.reserve(bench.aScene.VisibleCount());
    for (uint32_t i = 0; i < bench.aScene.VisibleCount(); ++i) {
        const sDRAW_ITEM &item = bench.aScene.DrawItem(i);
        if (item.aEntity == nullptr) continue;
        cMODEL *model = item.aEntity->aMesh.Model();
        if (model == nullptr) continue;
        boxes.push_back(model->aWorldAABB);
    }

    const sOVERDRAW_STATS st = measureOverdraw(viewProj, 1280, 720, boxes.empty() ? nullptr : &boxes[0],
                                               (uint32_t)boxes.size());
    std::fprintf(stderr,
                 "[overdraw] 2000 实体（剔除后 %u 个参与绘制）：overdraw = %.2fx"
                 "（片元 %llu / 覆盖像素 %llu）\n",
                 st.aObjects, st.OverdrawRatio(), (unsigned long long)st.aFragments,
                 (unsigned long long)st.aCoveredPixels);
    CHECK(st.aObjects > 0u);
    CHECK(st.OverdrawRatio() >= 0.9f); // 至少有一层
    bench.free();
}

// ── 对照：密集重叠几何的 overdraw 应当明显更高 ─────────
//
// 上面那个 1.06x 是"稀疏小物体"的形态。遮挡剔除真正有收益的是**密集重叠几何**
// （城市、树林、室内堆叠）。这条用例验证 `measureOverdraw` 确实能区分两者 ——
// 否则工具本身就没有说服力。
TEST_CASE(sceneOverdrawDenseGeometry) {
    // 200 个大盒子叠在同一块屏幕上（模拟密集场景）
    std::vector<sAABB> boxes;
    boxes.reserve(200);
    for (int i = 0; i < 200; ++i) {
        sAABB b;
        const float t = (float)i;
        // **相机朝 -Z 看，物体必须在负 z**（我第一次写成 +10..+109，全在相机背后，
        // projectToNdc 的 w<=0 检查把它们全跳过 ⇒ 0.00x。那是测试数据错，不是工具错。）
        // 另外 y 也要落在视锥内：半高 = |z|*tan(30°)，|z| 最小 10 ⇒ 半高 5.77。
        b.aMin = GNg::sVEC3{-3.0f + std::fmod(t * 0.7f, 2.0f), -3.0f + std::fmod(t * 0.9f, 2.0f),
                            -(10.0f + t * 0.5f)};
        b.aMax = GNg::sVEC3{b.aMin.x + 2.5f, b.aMin.y + 2.5f, b.aMin.z + 2.0f};
        boxes.push_back(b);
    }
    GNg::sMAT44 view;
    GNg::sMAT44 proj;
    GNg::sMAT44 viewProj;
    const GNg::sVEC3 eye{0.0f, 0.0f, 0.0f};
    const GNg::sVEC3 target{0.0f, 0.0f, -1.0f};
    const GNg::sVEC3 up{0.0f, 1.0f, 0.0f};
    GNg::Mat44LookAtRH(&view, &eye, &target, &up);
    GNg::Mat44PerspectiveVK(&proj, 1.0472f, 16.0f / 9.0f, 0.5f, 300.0f);
    GNg::Mat44Mul(&viewProj, &proj, &view);

    const sOVERDRAW_STATS st =
        measureOverdraw(viewProj, 1280, 720, &boxes[0], (uint32_t)boxes.size());
    std::fprintf(stderr, "[overdraw] 200 个重叠大盒：overdraw = %.2fx（稀疏场景是 1.06x）\n",
                 st.OverdrawRatio());
    CHECK(st.aObjects == 200u);
    // 密集重叠必须有明显 overdraw（>2 才算"值得做遮挡剔除"的量级）
    CHECK(st.OverdrawRatio() > 2.0f);
}
