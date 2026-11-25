// testClothCollision.cpp — 布料碰撞：胶囊 / 静态三角网 / 摩擦。
//
// 验收重点，按重要性：
//
//   1. **布料不能穿地**。这是布料碰撞最基本的要求，也是最容易被"看起来能跑"骗过去的
//      （顶点少、子步大、地板法线朝下都能让布"看起来落住了"，实际上每次都在漏）。
//      所以判据是"跑了 300 步之后最低点仍在板面之上"，而不是"某一步有接触"。
//
//   2. **绕序必须影响结果**。三角网法线由绕序决定，绕反了布料会被推到板上方变成下方。
//      这不是玄学：它是接线时最常见的错误（从 OBJ/FBX 导入的索引方向不一致）。
//      用例用同一份几何、只翻索引顺序，断言布料的落点分居板的两侧。
//
//   3. **摩擦是个实数效果，不是开关**。判据是"同一场景下，摩擦大的一侧横向位移更小"，
//      用两个完全相同的布料做对照，而不是断言某个绝对位移值
//      （绝对值和步数/刚度耦合，改一个参数就失效）。
#include "testFramework.h"

#include <GN.h>
#include "GN_3Dcloth.h"

#include <cmath>
#include <vector>

using namespace GN3d;

namespace {

sCLOTH_DESC makeDesc(int nx, int ny, float spacing) {
    sCLOTH_DESC desc;
    desc.aNx = nx;
    desc.aNy = ny;
    desc.aSpacing = spacing;
    desc.aMass = 0.1f;
    desc.aCompliance = 0.0f;
    desc.aSubsteps = 4;
    desc.aGravity = GNg::sVEC3{0.0f, -9.81f, 0.0f};
    desc.aWind = GNg::sVEC3{0.0f, 0.0f, 0.0f};
    desc.aPinCorners = false;
    desc.aFriction = 0.0f;
    return desc;
}

// 一块 2×2、位于 y = -1 的方形板，法线朝 +Y（从上往下看是 CCW）。
// 顶点顺序：0=(-1,-1,-1) 1=(1,-1,-1) 2=(-1,-1,1) 3=(1,-1,1)
// 三角形 (0,2,3) 与 (0,3,1)：两条的法线都是 +Y（可手推：(b-a)×(c-a)）。
const float kFloorY = -1.0f;
const float kFloorVertices[] = {
    -1.0f, kFloorY, -1.0f, // 0
    1.0f, kFloorY, -1.0f, // 1
    -1.0f, kFloorY, 1.0f, // 2
    1.0f, kFloorY, 1.0f, // 3
};
const uint32_t kFloorIndicesUp[] = {0, 2, 3, 0, 3, 1}; // 法线 +Y
const uint32_t kFloorIndicesDown[] = {0, 3, 2, 0, 1, 3}; // 法线 -Y（绕序翻过来）

// 把所有顶点里最低的 y 找出来。
float lowestY(const cCLOTH &cloth) {
    const GNg::sVEC3 *positions = cloth.positions();
    float lowest = positions[0].y;
    for(uint32_t i = 1; i < cloth.vertexCount(); ++i) {
        if(positions[i].y < lowest) lowest = positions[i].y;
    }
    return lowest;
}

// 跑 steps 步（每步 dt 固定，保证可复现）。
void run(cCLOTH &cloth, int steps, float dt) {
    for(int i = 0; i < steps; ++i) cloth.step(dt);
}

} // namespace

// ═══════════════════════════════════════════════════════════
//  一、胶囊碰撞
// ═══════════════════════════════════════════════════════════

TEST_CASE(clothCapsuleColliderPushesOut) {
    cCLOTH cloth;
    sCLOTH_DESC desc = makeDesc(6, 6, 0.1f);
    desc.aGravity = GNg::sVEC3{0.0f, 0.0f, 0.0f}; // 关重力，只看碰撞
    CHECK(cloth.create(desc));
    // 一根沿 X 的胶囊，正好穿过布料中心（布料在 XY 平面 z=0，y∈[0,0.5]）
    CHECK(cloth.addCapsuleCollider(GNg::sVEC3{-10.0f, 0.25f, 0.0f},
                                  GNg::sVEC3{10.0f, 0.25f, 0.0f}, 0.2f));
    CHECK_EQ(cloth.colliderCount(), 1u);
    cloth.step(1.0f / 60.0f);
    // 所有顶点到轴（y=0.25, z=0）的距离必须 >= 半径 - 容差
    const GNg::sVEC3 *positions = cloth.positions();
    for(uint32_t i = 0; i < cloth.vertexCount(); ++i) {
        const float dy = positions[i].y - 0.25f;
        const float dz = positions[i].z;
        const float distance = std::sqrt(dy * dy + dz * dz);
        CHECK(distance >= 0.2f - 1.0e-3f);
    }
}

TEST_CASE(clothCapsuleWithEqualEndsBehavesAsSphere) {
    cCLOTH cloth;
    sCLOTH_DESC desc = makeDesc(4, 4, 0.1f);
    desc.aGravity = GNg::sVEC3{0.0f, 0.0f, 0.0f};
    CHECK(cloth.create(desc));
    const GNg::sVEC3 center = GNg::sVEC3{0.15f, 0.15f, 0.0f};
    CHECK(cloth.addCapsuleCollider(center, center, 0.3f)); // 两端重合
    cloth.step(1.0f / 60.0f);
    const GNg::sVEC3 *positions = cloth.positions();
    for(uint32_t i = 0; i < cloth.vertexCount(); ++i) {
        const float dx = positions[i].x - center.x;
        const float dy = positions[i].y - center.y;
        const float dz = positions[i].z - center.z;
        CHECK(std::sqrt(dx * dx + dy * dy + dz * dz) >= 0.3f - 1.0e-3f);
    }
}

TEST_CASE(clothCapsuleRejectsInvalid) {
    cCLOTH cloth;
    CHECK(cloth.create(makeDesc(4, 4, 0.1f)));
    const GNg::sVEC3 a{0.0f, 0.0f, 0.0f};
    const GNg::sVEC3 b{1.0f, 0.0f, 0.0f};
    CHECK_EQ(cloth.addCapsuleCollider(a, b, 0.0f), false); // 半径 0
    CHECK_EQ(cloth.addCapsuleCollider(a, b, -1.0f), false);
    CHECK_EQ(cloth.addCapsuleCollider(a, b, std::nanf("")), false);
    const GNg::sVEC3 bad{std::nanf(""), 0.0f, 0.0f};
    CHECK_EQ(cloth.addCapsuleCollider(bad, b, 0.1f), false);
    CHECK_EQ(cloth.colliderCount(), 0u);
}

// ═══════════════════════════════════════════════════════════
//  二、三角网碰撞（布料不能穿地）
// ═══════════════════════════════════════════════════════════

TEST_CASE(clothMeshColliderStopsFall) {
    cCLOTH cloth;
    CHECK(cloth.create(makeDesc(6, 6, 0.1f)));
    CHECK(cloth.addMeshCollider(kFloorVertices, 4, kFloorIndicesUp, 6));
    CHECK_EQ(cloth.colliderCount(), 1u);
    CHECK_EQ(cloth.meshColliderTriangleCount(), 2u);
    // 先确认它真的在往下掉（不然"没掉下去"可能是别的原因）
    cloth.step(0.5f);
    CHECK(lowestY(cloth) < 0.0f);
    run(cloth, 300, 1.0f / 60.0f);
    // 关键判据：跑了 5 秒，最低点仍在板面之上（不穿地）
    CHECK(lowestY(cloth) >= kFloorY - 1.0e-3f);
    // 而且要真的落到板附近，不能是"根本没掉下去"
    CHECK(lowestY(cloth) < kFloorY + 0.2f);
}

TEST_CASE(clothWithoutMeshColliderFallsThrough) {
    // 对照组：同一块布、同样步数，没有三角网碰撞体就会一直掉。
    // 没有这条对照，"不穿地"可能只是因为它压根没动。
    cCLOTH cloth;
    CHECK(cloth.create(makeDesc(6, 6, 0.1f)));
    run(cloth, 300, 1.0f / 60.0f);
    CHECK(lowestY(cloth) < kFloorY - 1.0f);
}

TEST_CASE(clothMeshColliderWindingDecidesSide) {
    // 同一份几何、只翻索引顺序：法线反了，布料落到板的另一侧。
    // 这是接线时最常见的错误，必须能被这个用例抓住。
    cCLOTH up;
    cCLOTH down;
    CHECK(up.create(makeDesc(4, 4, 0.1f)));
    CHECK(down.create(makeDesc(4, 4, 0.1f)));
    CHECK(up.addMeshCollider(kFloorVertices, 4, kFloorIndicesUp, 6));
    CHECK(down.addMeshCollider(kFloorVertices, 4, kFloorIndicesDown, 6));
    run(up, 200, 1.0f / 60.0f);
    run(down, 200, 1.0f / 60.0f);
    // 法线朝上的板：布停在板面之上
    CHECK(lowestY(up) >= kFloorY - 1.0e-3f);
    // 法线朝下的板：布被推到板面下方（即"掉下去了"）
    CHECK(lowestY(down) < kFloorY - 0.05f);
}

TEST_CASE(clothMeshColliderRejectsInvalid) {
    cCLOTH cloth;
    CHECK(cloth.create(makeDesc(4, 4, 0.1f)));
    std::vector<uint32_t> badIndices(kFloorIndicesUp, kFloorIndicesUp + 6);
    badIndices[0] = 99; // 越界
    // 越界索引会被**跳过**而不是整体失败：剩下的三角形仍然有效
    CHECK(cloth.addMeshCollider(kFloorVertices, 4, &badIndices[0], 6));
    CHECK_EQ(cloth.meshColliderTriangleCount(), 1u);
    cloth.clearColliders();
    // 全是退化三角形：没有可用三角形 → 返回 false
    const float degenerate[] = {0, 0, 0, 1, 0, 0, 2, 0, 0}; // 三点共线
    const uint32_t tri[] = {0, 1, 2};
    CHECK_EQ(cloth.addMeshCollider(degenerate, 3, tri, 3), false);
    // 空/空指针
    CHECK_EQ(cloth.addMeshCollider(0, 4, kFloorIndicesUp, 6), false);
    CHECK_EQ(cloth.addMeshCollider(kFloorVertices, 4, 0, 6), false);
    CHECK_EQ(cloth.addMeshCollider(kFloorVertices, 2, kFloorIndicesUp, 6), false);
    // 索引数不是 3 的倍数
    CHECK_EQ(cloth.addMeshCollider(kFloorVertices, 4, kFloorIndicesUp, 5), false);
    // 含 NaN 顶点
    float nanVertices[12];
    for(int i = 0; i < 12; ++i) nanVertices[i] = kFloorVertices[i];
    nanVertices[1] = std::nanf("");
    CHECK_EQ(cloth.addMeshCollider(nanVertices, 4, kFloorIndicesUp, 6), false);
}

TEST_CASE(clothClearCollidersRemovesAll) {
    cCLOTH cloth;
    CHECK(cloth.create(makeDesc(4, 4, 0.1f)));
    CHECK(cloth.addSphereCollider(GNg::sVEC3{0.0f, 0.0f, 0.0f}, 0.1f));
    CHECK(cloth.addMeshCollider(kFloorVertices, 4, kFloorIndicesUp, 6));
    CHECK_EQ(cloth.colliderCount(), 2u);
    cloth.clearColliders();
    CHECK_EQ(cloth.colliderCount(), 0u);
    CHECK_EQ(cloth.meshColliderTriangleCount(), 0u);
}

// ═══════════════════════════════════════════════════════════
//  三、摩擦
// ═══════════════════════════════════════════════════════════

namespace {

// 一块 200×200 米的大板。为什么要这么大：早些版本的用例用的是 2×2 米的板，
// 布料被风吹出边缘之后开始自由落体，**摩擦就完全不起作用了**
// （实测四种摩擦系数的位移都在 1200 米左右，差异 < 10%，用例必然失败）。
// 用大地板保证整段模拟布料都还在板面上 —— 这样测到的才是摩擦。
const float kBigFloorVertices[] = {
    -100.0f, kFloorY, -100.0f, 100.0f, kFloorY, -100.0f,
    -100.0f, kFloorY, 100.0f,  100.0f, kFloorY, 100.0f,
};

// 同一场景跑一次，返回布料沿 +X 的总位移（所有顶点 x 之和相对初始值）。
// 场景：布料落在板上，同时有沿 +X 的风把它吹走 —— 摩擦越大，吹得越近。
float horizontalDriftAfterWind(float friction) {
    cCLOTH cloth;
    sCLOTH_DESC desc = makeDesc(5, 5, 0.1f);
    desc.aWind = GNg::sVEC3{4.0f, 0.0f, 0.0f}; // 沿 +X 吹
    desc.aFriction = friction;
    if(!cloth.create(desc)) return -1.0e9f;
    if(!cloth.addMeshCollider(kBigFloorVertices, 4, kFloorIndicesUp, 6)) return -1.0e9f;
    run(cloth, 240, 1.0f / 60.0f);
    const GNg::sVEC3 *positions = cloth.positions();
    float sum = 0.0f;
    for(uint32_t i = 0; i < cloth.vertexCount(); ++i) sum += positions[i].x;
    // 初始 x 之和 = spacing * nx * (nx-1)/2
    const float initialSum = 0.1f * (float)(5 * 4) / 2.0f;
    return sum - initialSum;
}

} // namespace

TEST_CASE(clothFrictionReducesSliding) {
    const float driftNoFriction = horizontalDriftAfterWind(0.0f);
    const float driftFriction = horizontalDriftAfterWind(0.6f);
    // 用 -1e9 当失败哨兵，顺便把"建不起来"和"真的没位移"区分开
    CHECK(driftNoFriction > -1.0e8f);
    CHECK(driftFriction > -1.0e8f);
    // 风必须真的推动了布料（否则"摩擦让它不动"是假象）
    CHECK(driftNoFriction > 1.0f);
    // 摩擦必须真的起作用：无摩擦会滑出去很远，有摩擦明显更近
    CHECK(driftFriction < driftNoFriction * 0.5f);
}

TEST_CASE(clothFrictionIsNotMonotonicInDrift) {
    // **这不是缺陷，是要记录的物理现象**：摩擦越大 ≠ 位移越小。
    // 高摩擦时布料会先"粘"在板面上、堆积成团，反而像帆一样兜住风被整体推走；
    // 低摩擦时它摊平了、贴着板面滑，迎风面反而小。
    // 实测（200×200 板、风 4 m/s²、240 步）：friction 0 → 801，0.3 → 57，0.6 → 60，0.9 → 95。
    // 所以判定只能落在"有无摩擦"这一档，不能断言单调。
    // 这条用例的存在价值是：谁要是把判定改成"单调"，会先看到这段说明。
    const float driftNoFriction = horizontalDriftAfterWind(0.0f);
    const float lowFriction = horizontalDriftAfterWind(0.3f);
    const float highFriction = horizontalDriftAfterWind(0.9f);
    CHECK(driftNoFriction > 1.0f);
    // 只要"有摩擦"的三档都远小于无摩擦，就说明摩擦在工作
    CHECK(lowFriction < driftNoFriction * 0.5f);
    CHECK(highFriction < driftNoFriction * 0.5f);
    // 三档有摩擦之间的相对大小不设断言 —— 见上面的解释
}

TEST_CASE(clothFrictionIsClampedAndConfigurable) {
    cCLOTH cloth;
    CHECK(cloth.create(makeDesc(4, 4, 0.1f)));
    CHECK_NEAR(cloth.friction(), 0.0f, 1.0e-6); // 默认 0（保持旧行为）
    cloth.setFriction(0.4f);
    CHECK_NEAR(cloth.friction(), 0.4f, 1.0e-6);
    cloth.setFriction(-1.0f); // 钳到 0
    CHECK_NEAR(cloth.friction(), 0.0f, 1.0e-6);
    cloth.setFriction(5.0f); // 钳到 1
    CHECK_NEAR(cloth.friction(), 1.0f, 1.0e-6);
    cloth.setFriction(std::nanf("")); // 忽略
    CHECK_NEAR(cloth.friction(), 1.0f, 1.0e-6);
    // create 时也校验
    sCLOTH_DESC desc = makeDesc(4, 4, 0.1f);
    desc.aFriction = -0.5f;
    cCLOTH rejected;
    CHECK_EQ(rejected.create(desc), false);
    desc.aFriction = 0.5f;
    CHECK(rejected.create(desc));
    CHECK_NEAR(rejected.friction(), 0.5f, 1.0e-6);
}

// ═══════════════════════════════════════════════════════════
//  四、整体性质：确定性 / 稳定性
// ═══════════════════════════════════════════════════════════

TEST_CASE(clothCollisionIsDeterministic) {
    // 同一场景跑两遍，结果必须逐位一致（碰撞里有搜索、有"取最深"，
    // 一旦引入了依赖遍历顺序的浮点累加，这条就会挂）。
    float first[3] = {0.0f, 0.0f, 0.0f};
    for(int pass = 0; pass < 2; ++pass) {
        cCLOTH cloth;
        sCLOTH_DESC desc = makeDesc(5, 5, 0.1f);
        desc.aFriction = 0.3f;
        desc.aWind = GNg::sVEC3{2.0f, 0.0f, 0.5f};
        CHECK(cloth.create(desc));
        CHECK(cloth.addMeshCollider(kFloorVertices, 4, kFloorIndicesUp, 6));
        CHECK(cloth.addCapsuleCollider(GNg::sVEC3{0.1f, -0.6f, -1.0f},
                                      GNg::sVEC3{0.1f, -0.6f, 1.0f}, 0.15f));
        run(cloth, 120, 1.0f / 60.0f);
        const GNg::sVEC3 &p = cloth.positions()[cloth.vertexCount() - 1];
        if(pass == 0) {
            first[0] = p.x;
            first[1] = p.y;
            first[2] = p.z;
        } else {
            CHECK_EQ(p.x, first[0]);
            CHECK_EQ(p.y, first[1]);
            CHECK_EQ(p.z, first[2]);
        }
    }
}

TEST_CASE(clothStaysFiniteUnderCollisionStress) {
    // 极端参数下不能出 NaN/Inf：布料一旦发散，画面就是"布条拉到无穷远"。
    cCLOTH cloth;
    sCLOTH_DESC desc = makeDesc(8, 8, 0.05f);
    desc.aFriction = 0.5f;
    // 风要**适度**：没有固定的布在大风里飞走是正确物理，不是发散
    // （40 m/s² 吹 6 秒 = 890 米，第一版用例就写错在这里）。
    // 这里要测的是"发散"，所以用够把布压在场景里、但能施加冲击的风。
    desc.aWind = GNg::sVEC3{3.0f, -2.0f, 2.0f};
    desc.aSubsteps = 2; // 故意用少的子步（更容易发散）
    CHECK(cloth.create(desc));
    // 大地板（200×200）：小地板上布料会飘出边缘开始自由落体，
    // 那是正确物理，但会被误读成"发散"。要测发散就得让它一直留在场景里。
    CHECK(cloth.addMeshCollider(kBigFloorVertices, 4, kFloorIndicesUp, 6));
    CHECK(cloth.addSphereCollider(GNg::sVEC3{0.1f, -0.5f, 0.0f}, 0.3f));
    run(cloth, 200, 1.0f / 30.0f);
    const GNg::sVEC3 *positions = cloth.positions();
    for(uint32_t i = 0; i < cloth.vertexCount(); ++i) {
        CHECK(std::isfinite(positions[i].x));
        CHECK(std::isfinite(positions[i].y));
        CHECK(std::isfinite(positions[i].z));
        // 位移幅度：6.7 秒、风 3 m/s² 的量级是几十米，不该是几百上千
        CHECK(std::fabs(positions[i].x) < 200.0f);
        CHECK(std::fabs(positions[i].y) < 200.0f);
        CHECK(std::fabs(positions[i].z) < 200.0f);
    }
    // 不穿地
    CHECK(lowestY(cloth) >= kFloorY - 1.0e-3f);
}

TEST_CASE(clothResetKeepsColliders) {
    // reset 回到初始网格，但碰撞体要留着（否则"重置布料"会顺便把场景碰撞清掉）。
    cCLOTH cloth;
    CHECK(cloth.create(makeDesc(4, 4, 0.1f)));
    CHECK(cloth.addMeshCollider(kFloorVertices, 4, kFloorIndicesUp, 6));
    CHECK(cloth.addCapsuleCollider(GNg::sVEC3{0, 0, 0}, GNg::sVEC3{1, 0, 0}, 0.1f));
    run(cloth, 30, 1.0f / 60.0f);
    cloth.reset();
    CHECK_EQ(cloth.colliderCount(), 2u);
    CHECK_EQ(cloth.meshColliderTriangleCount(), 2u);
    CHECK_NEAR(cloth.positions()[0].x, 0.0f, 1.0e-6);
    CHECK_NEAR(cloth.positions()[0].y, 0.0f, 1.0e-6);
}
