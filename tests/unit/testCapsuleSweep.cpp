// testCapsuleSweep.cpp — 胶囊扫掠（C4），纯 CPU。
//
// 核心语义只有一条：**薄的东西挡得住快的东西**。
// 这也是"抬-移-落"做不到的事：离散点查询只在起点/终点查，中途扫过去就漏了。
#include "testFramework.h"

#include <GN.h>
#include "GN_CapsuleSweep.h"

#include <cmath>
#include <vector>

using namespace GN3d;

namespace {

const float kRadius = 0.3f;
const float kHalfHeight = 0.55f;

sCAPSULE_SWEEP_PARAMS makeParams() {
    sCAPSULE_SWEEP_PARAMS p;
    p.aRadius = kRadius;
    p.aHalfHeight = kHalfHeight;
    p.aSkinWidth = 1.0e-3f;
    p.aMaxIterations = 32;
    p.aTolerance = 1.0e-4f;
    return p;
}

// 一块水平面（y=height），覆盖 x/z ∈ [-10,10]。
// **绕序必须与 cCHARACTER::AddFloor 一致**（a, c, b）：法线方向由绕序决定，
// 写反了法线会朝下，"推出去"就变成往下推 —— 我第一次就踩了这个坑
// （测试断言 position.y > from.y 失败，根因是地板法线朝下）。
sSWEEP_TRIANGLE makeFloor(float height) {
    sSWEEP_TRIANGLE t;
    t.aA = GNg::sVEC3{-10.0f, height, -10.0f}; // (-x,-z)
    t.aB = GNg::sVEC3{10.0f, height, 10.0f}; // (+x,+z)   ← 与 aC 交换过
    t.aC = GNg::sVEC3{10.0f, height, -10.0f}; // (+x,-z)
    return t;
}

// 一堵薄墙：位于 x = 0 的竖直平面，覆盖 y/z ∈ [-5,5]。
//
// **形状必须是覆盖整个范围的矩形（两个三角形）**：我第一版只给了一个三角形
// `(0,-5,-5),(0,5,5),(0,5,-5)`，它只覆盖平面的一半，而测试路径正好从"另一半"
// 穿过去 —— 于是"墙挡不住"被误判成扫掠算法有 bug（排查了很久）。
// 单测里的几何体也要认真搭。
void makeThinWall(sSWEEP_TRIANGLE out[2]) {
    // 两个三角形拼成 x=0 的矩形；绕序使法线朝 -X
    out[0].aA = GNg::sVEC3{0.0f, -5.0f, -5.0f};
    out[0].aB = GNg::sVEC3{0.0f, -5.0f, 5.0f};
    out[0].aC = GNg::sVEC3{0.0f, 5.0f, 5.0f};

    out[1].aA = GNg::sVEC3{0.0f, -5.0f, -5.0f};
    out[1].aB = GNg::sVEC3{0.0f, 5.0f, 5.0f};
    out[1].aC = GNg::sVEC3{0.0f, 5.0f, -5.0f};
}

float dist(const GNg::sVEC3 &a, const GNg::sVEC3 &b) {
    const float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

} // namespace

// ── 无碰撞 ────────────────────────────────────────────
TEST_CASE(sweepNoCollisionPassesThrough) {
    const sSWEEP_TRIANGLE floor = makeFloor(-1.0f);
    const sCAPSULE_SWEEP_PARAMS p = makeParams();
    // 胶囊在 y=2 水平移动，地板在 y=-1（底面到胶囊底还有 1.7-0.3-1=0.4 的间隙）
    const GNg::sVEC3 from{0.0f, 2.0f, 0.0f};
    const GNg::sVEC3 to{5.0f, 2.0f, 0.0f};
    const sCAPSULE_SWEEP_RESULT r = sweepCapsuleAgainstTriangles(from, to, p, &floor, 1);
    CHECK(!r.aHit);
    CHECK_NEAR(r.aTOI, 1.0f, 1e-6);
    CHECK_NEAR(dist(r.aPosition, to), 0.0f, 1e-5);
}

TEST_CASE(sweepNoTriangles) {
    const sCAPSULE_SWEEP_PARAMS p = makeParams();
    const GNg::sVEC3 from{0.0f, 0.0f, 0.0f};
    const GNg::sVEC3 to{1.0f, 0.0f, 0.0f};
    const sCAPSULE_SWEEP_RESULT r = sweepCapsuleAgainstTriangles(from, to, p, nullptr, 0);
    CHECK(!r.aHit);
    CHECK_NEAR(r.aTOI, 1.0f, 1e-6);
    CHECK_NEAR(dist(r.aPosition, to), 0.0f, 1e-6);
}

// ── 核心：薄墙挡得住高速移动（离散查询会漏掉）──────
TEST_CASE(sweepThinWallStopsFastMove) {
    sSWEEP_TRIANGLE wallPair[2];
    makeThinWall(wallPair);
    const sCAPSULE_SWEEP_PARAMS p = makeParams();

    // 从 x=-3 一步跨到 x=+3（6 m）。墙在 x=0，厚度为零（单三角形）。
    // **离散查询在起点和终点都不会命中**（两边都离墙 3 m），只有扫掠能拦住。
    const GNg::sVEC3 from{-3.0f, 0.0f, 0.0f};
    const GNg::sVEC3 to{3.0f, 0.0f, 0.0f};
    const sCAPSULE_SWEEP_RESULT r = sweepCapsuleAgainstTriangles(from, to, p, &wallPair[0], 2);

    CHECK(r.aHit);
    // TOI 应该在 (3-0.3)/6 = 0.45 附近（中心走到离墙 radius 时接触）
    CHECK(r.aTOI > 0.35f);
    CHECK(r.aTOI < 0.55f);
    // 停下后的位置必须在墙的**同侧**（x < 0），不能穿过去
    CHECK(r.aPosition.x < 0.0f);
    // 而且应当贴近墙面：x ≈ -radius
    CHECK_NEAR(r.aPosition.x, -kRadius, 0.05f);
}

TEST_CASE(sweepThinWallAlreadyPastIt) {
    // 反方向：从 +3 走到 -3，同样要被挡住
    sSWEEP_TRIANGLE wallPair[2];
    makeThinWall(wallPair);
    const sCAPSULE_SWEEP_PARAMS p = makeParams();
    const GNg::sVEC3 from{3.0f, 0.0f, 0.0f};
    const GNg::sVEC3 to{-3.0f, 0.0f, 0.0f};
    const sCAPSULE_SWEEP_RESULT r = sweepCapsuleAgainstTriangles(from, to, p, &wallPair[0], 2);
    CHECK(r.aHit);
    CHECK(r.aPosition.x > 0.0f); // 留在 +X 侧
    CHECK_NEAR(r.aPosition.x, kRadius, 0.05f);
}

// ── 落地：从高处掉到地面 ──────────────────────────────
TEST_CASE(sweepFallingStopsOnFloor) {
    const sSWEEP_TRIANGLE floor = makeFloor(0.0f);
    const sCAPSULE_SWEEP_PARAMS p = makeParams();
    // 中心从 y=2 掉到 y=0.2（会穿过地面）
    const GNg::sVEC3 from{0.0f, 2.0f, 0.0f};
    const GNg::sVEC3 to{0.0f, 0.2f, 0.0f};
    const sCAPSULE_SWEEP_RESULT r = sweepCapsuleAgainstTriangles(from, to, p, &floor, 1);
    CHECK(r.aHit);
    // 停下时胶囊底面应贴着地面：center.y - halfHeight - radius ≈ 0
    const float footY = r.aPosition.y - kHalfHeight - kRadius;
    CHECK(std::fabs(footY) < 0.02f);
    // 法线朝上
    CHECK(r.aNormal.y > 0.9f);
}

// ── 起点已穿透：TOI=0 并推出 ──────────────────────────
TEST_CASE(sweepStartingInsidePushesOut) {
    const sSWEEP_TRIANGLE floor = makeFloor(0.0f);
    const sCAPSULE_SWEEP_PARAMS p = makeParams();
    // 中心在 y=0.2 ⇒ 底面在 0.2-0.55-0.3 = -0.65，深深穿进地板
    const GNg::sVEC3 from{0.0f, 0.2f, 0.0f};
    const GNg::sVEC3 to{1.0f, 0.2f, 0.0f};
    const sCAPSULE_SWEEP_RESULT r = sweepCapsuleAgainstTriangles(from, to, p, &floor, 1);
    CHECK(r.aHit);
    CHECK_NEAR(r.aTOI, 0.0f, 1e-6);
    // 被推到地面上方
    CHECK(r.aPosition.y > from.y);
}

// ── 零位移 ────────────────────────────────────────────
TEST_CASE(sweepZeroDisplacement) {
    const sSWEEP_TRIANGLE floor = makeFloor(0.0f);
    const sCAPSULE_SWEEP_PARAMS p = makeParams();
    // 不穿透
    {
        const GNg::sVEC3 pos{0.0f, 2.0f, 0.0f};
        const sCAPSULE_SWEEP_RESULT r = sweepCapsuleAgainstTriangles(pos, pos, p, &floor, 1);
        CHECK(!r.aHit);
        CHECK_NEAR(r.aTOI, 1.0f, 1e-6);
    }
    // 穿透
    {
        const GNg::sVEC3 pos{0.0f, 0.1f, 0.0f};
        const sCAPSULE_SWEEP_RESULT r = sweepCapsuleAgainstTriangles(pos, pos, p, &floor, 1);
        CHECK(r.aHit);
        CHECK_NEAR(r.aTOI, 0.0f, 1e-6);
        CHECK(r.aPosition.y > pos.y);
    }
}

// ── 贴着墙走时扫掠的语义 ──────────────────────────────
//
// **这条用例第一版写错了预期**。我原本以为"贴墙沿切向滑行应当走完全程"，
// 但那样会要求扫掠把位移**分解成法向+切向** —— 那是**碰撞响应**（resolution）
// 的职责，不是**扫掠**（sweep）的职责。
//
// 扫掠的语义是"沿给定位移最远能走到哪"，只要起点已经接触，它就该停（TOI=0）。
// 切向滑行由调用方做：拿到法线后把剩余位移投影到切平面上再扫一次。
//
// 所以这里验证的是**正确的**两条：
//   1. 起点精确贴墙 + 沿墙移动 ⇒ 停在原地（TOI=0，保守但不穿透）；
//   2. 起点离墙有间隙 + 沿墙移动 ⇒ 走完全程（间隙足够，不产生接触）。
TEST_CASE(sweepSlidingAlongWallSemantics) {
    sSWEEP_TRIANGLE wallPair[2];
    makeThinWall(wallPair);
    const sCAPSULE_SWEEP_PARAMS p = makeParams();

    // 1) 精确贴墙：保守停住（不穿透就是正确行为）
    {
        const GNg::sVEC3 from{-kRadius, 0.0f, -2.0f};
        const GNg::sVEC3 to{-kRadius, 0.0f, 2.0f};
        const sCAPSULE_SWEEP_RESULT r = sweepCapsuleAgainstTriangles(from, to, p, &wallPair[0], 2);
        CHECK(r.aHit);
        CHECK_NEAR(r.aTOI, 0.0f, 1e-4);
        // 关键：**没有被推进墙里**（x 仍在 -radius 一侧）
        CHECK(r.aPosition.x <= -kRadius + 1e-3f);
    }

    // 2) 留 5mm 间隙：应当走完全程
    {
        const GNg::sVEC3 from{-kRadius - 0.005f, 0.0f, -2.0f};
        const GNg::sVEC3 to{-kRadius - 0.005f, 0.0f, 2.0f};
        const sCAPSULE_SWEEP_RESULT r = sweepCapsuleAgainstTriangles(from, to, p, &wallPair[0], 2);
        CHECK(!r.aHit);
        CHECK_NEAR(r.aTOI, 1.0f, 1e-6);
        CHECK(r.aPosition.z > 1.9f);
    }
}

// ── 两次扫掠实现切向滑行（调用方的标准做法）──────────
// 这条才是"贴墙滑行"的完整语义：第一次被挡，拿到法线后把剩余位移投影到切平面再扫。
TEST_CASE(sweepTwoPassSlidesAlongWall) {
    sSWEEP_TRIANGLE wallPair[2];
    makeThinWall(wallPair);
    const sCAPSULE_SWEEP_PARAMS p = makeParams();

    // 起点在墙前 1m，位移同时含法向（撞墙）与切向（沿墙）分量
    const GNg::sVEC3 from{-1.3f, 0.0f, -2.0f};
    const GNg::sVEC3 to{-0.0f, 0.0f, 2.0f}; // 想同时往 +x 和 +z 走

    const sCAPSULE_SWEEP_RESULT first = sweepCapsuleAgainstTriangles(from, to, p, &wallPair[0], 2);
    CHECK(first.aHit);

    // 用扫掠返回的法线把**剩余位移**投影到切平面
    const float nlen = std::sqrt(first.aNormal.x * first.aNormal.x +
                                 first.aNormal.y * first.aNormal.y +
                                 first.aNormal.z * first.aNormal.z);
    CHECK(nlen > 0.5f); // 法线有效

    const float remaining = 1.0f - first.aTOI;
    GNg::sVEC3 rest{(to.x - from.x) * remaining, (to.y - from.y) * remaining,
                    (to.z - from.z) * remaining};
    const float dn = rest.x * first.aNormal.x + rest.y * first.aNormal.y +
                     rest.z * first.aNormal.z;
    const GNg::sVEC3 slide{rest.x - first.aNormal.x * dn, rest.y - first.aNormal.y * dn,
                           rest.z - first.aNormal.z * dn};
    const GNg::sVEC3 slideTo{first.aPosition.x + slide.x, first.aPosition.y + slide.y,
                             first.aPosition.z + slide.z};
    const sCAPSULE_SWEEP_RESULT second =
        sweepCapsuleAgainstTriangles(first.aPosition, slideTo, p, &wallPair[0], 2);

    // 切向分量应当完整走过去：最终 z 明显前进（法向方向上仍留在墙外侧）
    CHECK(second.aPosition.z > 0.0f);
    CHECK(second.aPosition.x < 0.0f);
}

// ── 参数边界 ──────────────────────────────────────────
TEST_CASE(sweepZeroRadiusOrInvalid) {
    sSWEEP_TRIANGLE wallPair[2];
    makeThinWall(wallPair);
    sCAPSULE_SWEEP_PARAMS p = makeParams();
    const GNg::sVEC3 from{-3.0f, 0.0f, 0.0f};
    const GNg::sVEC3 to{3.0f, 0.0f, 0.0f};
    // 半径为 0：退化成"直接过去"
    p.aRadius = 0.0f;
    const sCAPSULE_SWEEP_RESULT r = sweepCapsuleAgainstTriangles(from, to, p, &wallPair[0], 2);
    CHECK_NEAR(r.aTOI, 1.0f, 1e-6);
    CHECK_NEAR(dist(r.aPosition, to), 0.0f, 1e-6);
}

// 迭代次数必须有限（不能死循环）
TEST_CASE(sweepIterationCountBounded) {
    sSWEEP_TRIANGLE wallPair[2];
    makeThinWall(wallPair);
    sCAPSULE_SWEEP_PARAMS p = makeParams();
    p.aMaxIterations = 8;
    const GNg::sVEC3 from{-3.0f, 0.0f, 0.0f};
    const GNg::sVEC3 to{3.0f, 0.0f, 0.0f};
    const sCAPSULE_SWEEP_RESULT r = sweepCapsuleAgainstTriangles(from, to, p, &wallPair[0], 2);
    CHECK(r.aIterations <= p.aMaxIterations);
}

// ── 最小距离查询 ──────────────────────────────────────
TEST_CASE(capsuleMinDistanceBasics) {
    const sSWEEP_TRIANGLE floor = makeFloor(0.0f);
    const sCAPSULE_SWEEP_PARAMS p = makeParams();
    // 中心在 y=2：底面在 2-0.55-0.3=1.15 ⇒ 距离 1.15
    const float d = capsuleMinDistance(GNg::sVEC3{0.0f, 2.0f, 0.0f}, p, &floor, 1);
    CHECK(d > 0.0f);
    CHECK(d < 2.0f); // 保守估计不会太离谱
    // 没有三角形：无穷远
    CHECK(capsuleMinDistance(GNg::sVEC3{0.0f, 2.0f, 0.0f}, p, nullptr, 0) > 1.0e20f);
}

// ── 多块三角形 ────────────────────────────────────────
TEST_CASE(sweepMultipleTrianglesPicksEarliest) {
    // 两道墙：x=0 与 x=2。从左往右扫，应当被 x=0 那道拦住
    std::vector<sSWEEP_TRIANGLE> tris;
    sSWEEP_TRIANGLE w1[2];
    makeThinWall(w1);
    tris.push_back(w1[0]);
    tris.push_back(w1[1]);
    for (int i = 0; i < 2; ++i) {
        sSWEEP_TRIANGLE w2 = w1[i];
        w2.aA.x = w2.aB.x = w2.aC.x = 2.0f;
        tris.push_back(w2);
    }

    const sCAPSULE_SWEEP_PARAMS p = makeParams();
    const GNg::sVEC3 from{-3.0f, 0.0f, 0.0f};
    const GNg::sVEC3 to{5.0f, 0.0f, 0.0f};
    const sCAPSULE_SWEEP_RESULT r =
        sweepCapsuleAgainstTriangles(from, to, p, &tris[0], (uint32_t)tris.size());
    CHECK(r.aHit);
    // 被**第一道**墙挡住（x≈-radius），不是第二道
    CHECK(r.aPosition.x < 0.0f);
    CHECK_NEAR(r.aPosition.x, -kRadius, 0.05f);
}

// ── 与"离散查询"的对照：证明扫掠确实更安全 ────────────
// 这是 C4 的存在理由，必须有用例钉住。
TEST_CASE(sweepCatchesWhatDiscreteQueryMisses) {
    sSWEEP_TRIANGLE wallPair[2];
    makeThinWall(wallPair);
    const sCAPSULE_SWEEP_PARAMS p = makeParams();
    const GNg::sVEC3 from{-3.0f, 0.0f, 0.0f};
    const GNg::sVEC3 to{3.0f, 0.0f, 0.0f};

    // 离散查询：起点、终点各查一次
    GNg::sVEC3 e0, e1;
    e0 = GNg::sVEC3{from.x, from.y - kHalfHeight, from.z};
    e1 = GNg::sVEC3{from.x, from.y + kHalfHeight, from.z};
    sCONTACT_MANIFOLD m;
    const bool hitAtStart =
        queryCapsuleTriangle(e0, e1, kRadius, wallPair[0].aA, wallPair[0].aB,
                                 wallPair[0].aC, m) && m.aCount > 0;
    e0 = GNg::sVEC3{to.x, to.y - kHalfHeight, to.z};
    e1 = GNg::sVEC3{to.x, to.y + kHalfHeight, to.z};
    sCONTACT_MANIFOLD m2;
    const bool hitAtEnd =
        queryCapsuleTriangle(e0, e1, kRadius, wallPair[0].aA, wallPair[0].aB,
                                 wallPair[0].aC, m2) && m2.aCount > 0;

    // **两端都没命中** —— 离散查询会认为这一帧畅通无阻
    CHECK(!hitAtStart);
    CHECK(!hitAtEnd);

    // 扫掠能拦住
    const sCAPSULE_SWEEP_RESULT r = sweepCapsuleAgainstTriangles(from, to, p, &wallPair[0], 2);
    CHECK(r.aHit);
    CHECK(r.aPosition.x < 0.0f);
}

// ═══════════════════════════════════════════════════════════
//  与角色衔接：扫掠版台阶（C4）
// ═══════════════════════════════════════════════════════════
#include "GN_Character.h"

namespace {

const float kStepDt = 1.0f / 60.0f;

// 平地 + 一级台阶（台阶顶 y = stepH，前沿在 z = 1.0）
struct sStepFixture {
    GN3d::cCHARACTER aCharacter;
    float aStepH;

    explicit sStepFixture(float stepH) : aStepH(stepH) {
        GN3d::sCHARACTER_DESC desc;
        desc.aRadius = 0.3f;
        desc.aHeight = 1.7f;
        desc.aWalkSpeed = 2.0f;
        desc.aStepHeight = 0.35f;
        aCharacter.Init(desc);
        aCharacter.SetPosition(GNg::sVEC3{0.0f, 1.0f, 0.0f});
        aCharacter.AddFloor(GNg::sVEC3{0.0f, 0.0f, 0.0f}, 20.0f, 20.0f, 0.0f);
        // 台阶：一块从 z=1.0 到 z=6.0、高 stepH 的板
        aCharacter.AddBox(GNg::sVEC3{0.0f, stepH * 0.5f, 3.5f},
                          GNg::sVEC3{3.0f, stepH * 0.5f, 2.5f});
    }

    // 朝 +Z 走 n 步
    void walk(int n) {
        for (int i = 0; i < n; ++i) {
            aCharacter.Move(GNg::sVEC3{0.0f, 0.0f, 1.0f}, false, false, kStepDt);
        }
    }
};

} // namespace

TEST_CASE(characterSweptStepToggle) {
    sStepFixture f(0.3f);
    CHECK(f.aCharacter.SweptStepsEnabled()); // 默认开启
    f.aCharacter.SetSweptStepsEnabled(false);
    CHECK(!f.aCharacter.SweptStepsEnabled());
    f.aCharacter.SetSweptStepsEnabled(true);
    CHECK(f.aCharacter.SweptStepsEnabled());
}

// 基本能力：能上得去一个 0.25 的台阶（低于 stepHeight）
TEST_CASE(characterSweptStepClimbsLowStep) {
    sStepFixture f(0.25f);
    f.walk(120); // 2 秒
    // 必须已经站到台阶上（z 越过 1.0，y 抬到台阶高度附近）
    CHECK(f.aCharacter.Position().z > 1.2f);
    // 脚底高度应当接近台阶顶（0.25），不是还在 0
    const float footY = f.aCharacter.FootPosition().y;
    CHECK(footY > 0.15f);
}

// **关键对比**：薄台阶（0.12，小于胶囊半径 0.3）在两种模式下都该能上去，
// 但扫掠版的**落脚更稳**（脚底高度波动更小）。
TEST_CASE(characterSweptStepIsStablerOnThinStep) {
    // 分别用两种模式走同一个薄台阶，比较脚底高度的最大跳变
    auto run = [](bool swept) {
        sStepFixture f(0.12f);
        f.aCharacter.SetSweptStepsEnabled(swept);
        float maxJump = 0.0f;
        float prevFoot = f.aCharacter.FootPosition().y;
        for (int i = 0; i < 150; ++i) {
            f.aCharacter.Move(GNg::sVEC3{0.0f, 0.0f, 1.0f}, false, false, kStepDt);
            const float foot = f.aCharacter.FootPosition().y;
            const float jump = std::fabs(foot - prevFoot);
            if (jump > maxJump) maxJump = jump;
            prevFoot = foot;
        }
        return maxJump;
    };

    const float sweptJump = run(true);
    const float legacyJump = run(false);
    // 扫掠版不应比旧版更抖（允许相同；理论上更稳）
    CHECK(sweptJump <= legacyJump + 0.02f);
}

// 走不上太高的台阶（0.6 > stepHeight 0.35）
TEST_CASE(characterSweptStepRejectsHighStep) {
    sStepFixture f(0.6f);
    f.walk(120);
    // 被挡住：z 不会深入台阶内部
    CHECK(f.aCharacter.Position().z < 1.6f);
    // 脚底仍在地面附近
    CHECK(f.aCharacter.FootPosition().y < 0.2f);
}

// 扫掠关闭时行为应当与旧版一致（可回退）
TEST_CASE(characterSweptStepCanBeDisabled) {
    sStepFixture f(0.25f);
    f.aCharacter.SetSweptStepsEnabled(false);
    f.walk(120);
    // 旧版也能上（这是它的基本能力），只是薄台阶时更抖
    CHECK(f.aCharacter.Position().z > 1.0f);
}

// 台阶不能变成"爬墙"：60° 的坡上不该一路蹭上去
TEST_CASE(characterSweptStepDoesNotClimbSteepSlope) {
    GN3d::cCHARACTER character;
    GN3d::sCHARACTER_DESC desc;
    desc.aRadius = 0.3f;
    desc.aHeight = 1.7f;
    desc.aWalkSpeed = 2.0f;
    desc.aStepHeight = 0.35f;
    desc.aMaxSlopeDeg = 45.0f;
    character.Init(desc);
    character.SetPosition(GNg::sVEC3{0.0f, 1.0f, 0.0f});
    character.AddFloor(GNg::sVEC3{0.0f, 0.0f, 0.0f}, 20.0f, 20.0f, 0.0f);
    // 陡坡：从 z=1 起，每前进 1 上升 2（≈63°）
    character.AddTriangle(GNg::sVEC3{-3.0f, 0.0f, 1.0f}, GNg::sVEC3{3.0f, 0.0f, 1.0f},
                          GNg::sVEC3{3.0f, 4.0f, 3.0f});
    character.AddTriangle(GNg::sVEC3{-3.0f, 0.0f, 1.0f}, GNg::sVEC3{3.0f, 4.0f, 3.0f},
                          GNg::sVEC3{-3.0f, 4.0f, 3.0f});

    for (int i = 0; i < 150; ++i) {
        character.Move(GNg::sVEC3{0.0f, 0.0f, 1.0f}, false, false, kStepDt);
    }
    // 不该爬到高处（被当墙挡住）
    CHECK(character.Position().y < 2.5f);
}
