// testIK.cpp — 两骨 IK（C3b），纯 CPU。
//
// 五组语义：
//   1. 可达：踝精确到达目标、两段骨长保持不变、膝朝极向量那侧弯；
//   2. 不可达（太远）：钳到 L1+L2、腿伸直（膝在髋踝连线上）；
//   3. 不可达（太近）：钳到 |L1-L2|；
//   4. 权重：0 = 原样、1 = 全解、中间 = 位置插值；
//   5. 层级：cIK_SOLVER 能从骨架姿势里取出正确的链。
#include "testFramework.h"

#include <GN.h>
#include "GN_IK.h"

#include <cmath>
#include <vector>

using namespace GN3d;

namespace {

const float kPi = 3.14159265358979323846f;

float dist(const GNg::sVEC3 &a, const GNg::sVEC3 &b) {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    const float dz = a.z - b.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

// 腿下垂的静止姿势：髋 (0,2,0)、膝 (0,1,0)、踝 (0,0,0) ⇒ L1 = L2 = 1
sIK_CHAIN makeLegChain() {
    sIK_CHAIN chain;
    chain.aRootPos = GNg::sVEC3{0.0f, 2.0f, 0.0f};
    chain.aJointPos = GNg::sVEC3{0.0f, 1.0f, 0.0f};
    chain.aEndPos = GNg::sVEC3{0.0f, 0.0f, 0.0f};
    chain.aPole = GNg::sVEC3{0.0f, 0.0f, 1.0f}; // 膝朝前（+Z）
    chain.aWeight = 1.0f;
    return chain;
}

} // namespace

// ── 1. 可达 ─────────────────────────────────────────────
TEST_CASE(ikReachesTargetKeepingBoneLengths) {
    sIK_CHAIN chain = makeLegChain();
    // 目标：正下方 1.2 m（L1+L2=2 之内可达 ⇒ 腿要弯）
    chain.aTarget = GNg::sVEC3{0.0f, 0.8f, 0.0f};
    const sIK_RESULT r = solveTwoBoneIK(chain);
    CHECK(r.aReachable);
    // 踝应精确到达目标
    CHECK_NEAR(dist(r.aEndPos, chain.aTarget), 0.0f, 1e-5);
    // 骨长保持不变（IK 不能拉长骨头）
    CHECK_NEAR(dist(r.aJointPos, chain.aRootPos), 1.0f, 1e-4);
    CHECK_NEAR(dist(r.aEndPos, r.aJointPos), 1.0f, 1e-4);
}

TEST_CASE(ikKneeBendsTowardPole) {
    sIK_CHAIN chain = makeLegChain();
    chain.aTarget = GNg::sVEC3{0.0f, 0.8f, 0.0f}; // 正下方，膝必须弯出去
    chain.aPole = GNg::sVEC3{0.0f, 0.0f, 1.0f}; // 前方 = +Z
    const sIK_RESULT r = solveTwoBoneIK(chain);
    CHECK(r.aReachable);
    // 膝应朝 +Z 弯（pole 那侧）
    CHECK(r.aJointPos.z > 0.05f);

    // 把 pole 反过来 ⇒ 膝朝 -Z
    chain.aPole = GNg::sVEC3{0.0f, 0.0f, -1.0f};
    const sIK_RESULT r2 = solveTwoBoneIK(chain);
    CHECK(r2.aJointPos.z < -0.05f);
}

TEST_CASE(ikTargetActuallyReachedVariousDistances) {
    // 遍历一串可达距离，逐个验证"踝到达 + 骨长不变 + 膝不翻面"
    for (int i = 1; i <= 19; ++i) {
        const float d = 0.15f + (float)i * 0.09f; // 0.24 .. 1.86（L1+L2=2）
        sIK_CHAIN chain = makeLegChain();
        chain.aTarget = GNg::sVEC3{0.0f, 2.0f - d, 0.0f};
        const sIK_RESULT r = solveTwoBoneIK(chain);
        CHECK(r.aReachable);
        CHECK_NEAR(dist(r.aEndPos, chain.aTarget), 0.0f, 1e-4);
        CHECK_NEAR(dist(r.aJointPos, chain.aRootPos), 1.0f, 1e-3);
        CHECK_NEAR(dist(r.aEndPos, r.aJointPos), 1.0f, 1e-3);
        // 膝始终朝 +Z（极向量侧）
        CHECK(r.aJointPos.z > 0.0f);
    }
}

// ── 2. 太远（伸不直）──────────────────────────────────
TEST_CASE(ikTargetTooFarClampsToMaxReach) {
    sIK_CHAIN chain = makeLegChain();
    chain.aTarget = GNg::sVEC3{0.0f, 2.0f - 5.0f, 0.0f}; // 5 m 外，远超 2 m
    const sIK_RESULT r = solveTwoBoneIK(chain);
    CHECK(!r.aReachable);
    // 踝应在 L1+L2 处（沿目标方向）
    CHECK_NEAR(dist(r.aEndPos, chain.aRootPos), 2.0f, 1e-4);
    // 腿必须**伸直**：膝在髋→踝连线上（垂直偏差 ≈ 0）
    // 这里方向是正下方，所以膝的 x/z 都该 ≈ 0
    CHECK_NEAR(r.aJointPos.x, 0.0f, 1e-4);
    CHECK_NEAR(r.aJointPos.z, 0.0f, 1e-4);
    CHECK_NEAR(r.aJointPos.y, 1.0f, 1e-4);
}

// ── 3. 太近（比 |L1-L2| 还近）─────────────────────────
TEST_CASE(ikTargetTooCloseClampsToMinReach) {
    sIK_CHAIN chain;
    // 不等长骨：L1=1.5、L2=0.5 ⇒ |L1-L2| = 1
    chain.aRootPos = GNg::sVEC3{0.0f, 2.0f, 0.0f};
    chain.aJointPos = GNg::sVEC3{0.0f, 0.5f, 0.0f};
    chain.aEndPos = GNg::sVEC3{0.0f, 0.0f, 0.0f};
    chain.aPole = GNg::sVEC3{0.0f, 0.0f, 1.0f};
    chain.aWeight = 1.0f;
    // 目标离髋只有 0.2（< 1）
    chain.aTarget = GNg::sVEC3{0.0f, 1.8f, 0.0f};
    const sIK_RESULT r = solveTwoBoneIK(chain);
    CHECK(!r.aReachable);
    // 踝被钳到 minReach = 1
    CHECK_NEAR(dist(r.aEndPos, chain.aRootPos), 1.0f, 1e-4);
    // 骨长仍保持
    CHECK_NEAR(dist(r.aJointPos, chain.aRootPos), 1.5f, 1e-3);
    CHECK_NEAR(dist(r.aEndPos, r.aJointPos), 0.5f, 1e-3);
}

// ── 4. 权重 ────────────────────────────────────────────
TEST_CASE(ikWeightZeroKeepsAnimation) {
    sIK_CHAIN chain = makeLegChain();
    chain.aTarget = GNg::sVEC3{1.0f, 1.0f, 0.0f}; // 会大幅改动
    chain.aWeight = 0.0f;
    const sIK_RESULT r = solveTwoBoneIK(chain);
    // 完全不动
    CHECK_NEAR(dist(r.aJointPos, chain.aJointPos), 0.0f, 1e-6);
    CHECK_NEAR(dist(r.aEndPos, chain.aEndPos), 0.0f, 1e-6);
    CHECK(r.aReachable);
}

TEST_CASE(ikWeightBlendsInPositionSpace) {
    sIK_CHAIN chain = makeLegChain();
    chain.aTarget = GNg::sVEC3{0.0f, 0.8f, 0.0f};
    chain.aWeight = 1.0f;
    const sIK_RESULT full = solveTwoBoneIK(chain);

    chain.aWeight = 0.5f;
    const sIK_RESULT half = solveTwoBoneIK(chain);

    // 半权重下踝应落在"原始位置"与"全解位置"的中点
    const GNg::sVEC3 expect{chain.aEndPos.x + (full.aEndPos.x - chain.aEndPos.x) * 0.5f,
                            chain.aEndPos.y + (full.aEndPos.y - chain.aEndPos.y) * 0.5f,
                            chain.aEndPos.z + (full.aEndPos.z - chain.aEndPos.z) * 0.5f};
    CHECK_NEAR(dist(half.aEndPos, expect), 0.0f, 1e-4);
}

TEST_CASE(ikWeightClampedAndNonFinite) {
    sIK_CHAIN chain = makeLegChain();
    chain.aTarget = GNg::sVEC3{0.0f, 0.8f, 0.0f};

    // 权重 > 1 钳到 1（结果与 weight=1 相同）
    chain.aWeight = 5.0f;
    const sIK_RESULT r5 = solveTwoBoneIK(chain);
    chain.aWeight = 1.0f;
    const sIK_RESULT r1 = solveTwoBoneIK(chain);
    CHECK_NEAR(dist(r5.aEndPos, r1.aEndPos), 0.0f, 1e-6);

    // NaN 权重：原样返回（不动）
    chain.aWeight = std::nanf("");
    const sIK_RESULT rn = solveTwoBoneIK(chain);
    CHECK_NEAR(dist(rn.aEndPos, chain.aEndPos), 0.0f, 1e-6);
    CHECK(!rn.aReachable); // 未求解
}

TEST_CASE(ikNonFiniteInputsAreIgnored) {
    sIK_CHAIN chain = makeLegChain();
    chain.aTarget = GNg::sVEC3{0.0f, 0.8f, 0.0f};
    chain.aTarget.x = std::nanf("");
    const sIK_RESULT r = solveTwoBoneIK(chain);
    // 原样返回，不产生 NaN 输出
    CHECK(std::isfinite(r.aJointPos.x));
    CHECK(std::isfinite(r.aEndPos.y));
    CHECK_NEAR(dist(r.aEndPos, chain.aEndPos), 0.0f, 1e-6);
}

// 骨长为 0：无解，原样返回
TEST_CASE(ikZeroLengthBoneIsRejected) {
    sIK_CHAIN chain = makeLegChain();
    chain.aJointPos = chain.aRootPos; // L1 = 0
    chain.aTarget = GNg::sVEC3{0.0f, 0.8f, 0.0f};
    const sIK_RESULT r = solveTwoBoneIK(chain);
    CHECK_NEAR(dist(r.aEndPos, chain.aEndPos), 0.0f, 1e-6);
}

// 目标与髋重合：方向未定义，不能崩
TEST_CASE(ikTargetAtRootDoesNotProduceNaN) {
    sIK_CHAIN chain = makeLegChain();
    chain.aTarget = chain.aRootPos;
    const sIK_RESULT r = solveTwoBoneIK(chain);
    CHECK(std::isfinite(r.aJointPos.x));
    CHECK(std::isfinite(r.aJointPos.y));
    CHECK(std::isfinite(r.aJointPos.z));
    CHECK(std::isfinite(r.aEndPos.x));
    CHECK(std::isfinite(r.aEndPos.y));
    CHECK(std::isfinite(r.aEndPos.z));
}

// 极向量与髋→踝方向平行（退化）：不能崩，且膝不应翻到反侧
TEST_CASE(ikDegeneratePoleFallsBackSafely) {
    sIK_CHAIN chain = makeLegChain();
    chain.aTarget = GNg::sVEC3{0.0f, 0.8f, 0.0f};
    chain.aPole = GNg::sVEC3{0.0f, -1.0f, 0.0f}; // 与髋→踝平行
    const sIK_RESULT r = solveTwoBoneIK(chain);
    CHECK(r.aReachable);
    CHECK_NEAR(dist(r.aEndPos, chain.aTarget), 0.0f, 1e-4);
    CHECK(std::isfinite(r.aJointPos.x));
    CHECK(std::isfinite(r.aJointPos.z));
    // 兜底用"当前膝盖方向"⇒ 膝仍在 Y 方向附近（不跳到奇怪的地方）
    CHECK_NEAR(dist(r.aJointPos, chain.aRootPos), 1.0f, 1e-3);
}

// ── 5. 四元数工具 ──────────────────────────────────────
TEST_CASE(ikQuatMulMatchesRotationComposition) {
    // 先转 90° 绕 Y，再转 90° 绕 X，与"依次施加"的结果比对
    GNg::sVEC3 axisY{0.0f, 1.0f, 0.0f};
    GNg::sVEC3 axisX{1.0f, 0.0f, 0.0f};
    GNg::sQUAT qy, qx;
    GNg::QuatFromAxisAngle(&qy, &axisY, kPi * 0.5f);
    GNg::QuatFromAxisAngle(&qx, &axisX, kPi * 0.5f);

    GNg::sQUAT combined;
    GNg::QuatMul(&combined, &qx, &qy); // 先 qy 再 qx

    // 对向量 (0,0,1)：先绕 Y 转 90° ⇒ (1,0,0)；再绕 X 转 90° ⇒ (1,0,0)（X 轴不变）
    GNg::sVEC3 v{0.0f, 0.0f, 1.0f};
    GNg::sVEC3 out;
    GNg::QuatRotateVec3(&out, &v, &combined);
    CHECK_NEAR(out.x, 1.0f, 1e-4);
    CHECK_NEAR(out.y, 0.0f, 1e-4);
    CHECK_NEAR(out.z, 0.0f, 1e-4);

    // 顺序反过来结果不同（验证乘法确实有序）
    GNg::sQUAT reversed;
    GNg::QuatMul(&reversed, &qy, &qx);
    GNg::sVEC3 out2;
    GNg::QuatRotateVec3(&out2, &v, &reversed);
    CHECK_NEAR(out2.x, 0.0f, 1e-4);
    CHECK_NEAR(out2.y, -1.0f, 1e-4);
}

TEST_CASE(ikQuatFromToBasic) {
    GNg::sVEC3 from{0.0f, -1.0f, 0.0f};
    GNg::sVEC3 to{1.0f, 0.0f, 0.0f};
    GNg::sQUAT q = quatFromTo(from, to);
    GNg::sVEC3 out;
    GNg::QuatRotateVec3(&out, &from, &q);
    CHECK_NEAR(out.x, to.x, 1e-4);
    CHECK_NEAR(out.y, to.y, 1e-4);
    CHECK_NEAR(out.z, to.z, 1e-4);

    // 同向：单位四元数
    GNg::sQUAT same = quatFromTo(from, from);
    CHECK_NEAR(same.w, 1.0f, 1e-6);

    // 反向：仍能得到正确旋转（转 180°，方向任选）
    GNg::sVEC3 back{0.0f, 1.0f, 0.0f};
    GNg::sQUAT flip = quatFromTo(from, back);
    GNg::sVEC3 out2;
    GNg::QuatRotateVec3(&out2, &from, &flip);
    CHECK_NEAR(out2.y, 1.0f, 1e-3);

    // 零向量：安全返回单位
    GNg::sVEC3 zero{0.0f, 0.0f, 0.0f};
    GNg::sQUAT safe = quatFromTo(zero, to);
    CHECK_NEAR(safe.w, 1.0f, 1e-6);
}

// ── 6. 骨架级取链 ──────────────────────────────────────
namespace {

// 造一个最小骨架：hip(0) → knee(1) → ankle(2)
sSKELETON makeThreeJointSkeleton() {
    sSKELETON sk;
    sk.aJoints.resize(3);
    sk.aJoints[0].aParent = -1;
    sk.aJoints[0].aBindTranslation = GNg::sVEC3{0.0f, 2.0f, 0.0f};
    sk.aJoints[1].aParent = 0;
    sk.aJoints[1].aBindTranslation = GNg::sVEC3{0.0f, -1.0f, 0.0f};
    sk.aJoints[2].aParent = 1;
    sk.aJoints[2].aBindTranslation = GNg::sVEC3{0.0f, -1.0f, 0.0f};
    for (int i = 0; i < 3; ++i) {
        GNg::QuatIdentity(&sk.aJoints[i].aBindRotation);
        sk.aJoints[i].aBindScale = GNg::sVEC3{1.0f, 1.0f, 1.0f};
    }
    return sk;
}

// 本地姿势 = 绑定姿势（腿竖直下垂）
std::vector<sANIM_TRANSFORM> bindPoseLocal(const sSKELETON &sk) {
    std::vector<sANIM_TRANSFORM> local(sk.aJoints.size());
    for (size_t i = 0; i < sk.aJoints.size(); ++i) {
        local[i].aTranslation = sk.aJoints[i].aBindTranslation;
        local[i].aRotation = sk.aJoints[i].aBindRotation;
        local[i].aScale = sk.aJoints[i].aBindScale;
    }
    return local;
}

} // namespace

TEST_CASE(ikSolverComputesWorldPositions) {
    sSKELETON sk = makeThreeJointSkeleton();
    cIK_SOLVER solver;
    solver.SetSkeleton(&sk);
    const std::vector<sANIM_TRANSFORM> local = bindPoseLocal(sk);

    std::vector<GNg::sVEC3> world;
    CHECK(solver.ComputeWorldPositions(local, world));
    CHECK_EQ((uint32_t)world.size(), 3u);
    // hip 在 (0,2,0)，膝在 (0,1,0)，踝在 (0,0,0)
    CHECK_NEAR(world[0].y, 2.0f, 1e-5);
    CHECK_NEAR(world[1].y, 1.0f, 1e-5);
    CHECK_NEAR(world[2].y, 0.0f, 1e-5);

    // 尺寸不符：拒绝
    std::vector<sANIM_TRANSFORM> wrong(2);
    CHECK(!solver.ComputeWorldPositions(wrong, world));
    // 无骨架：拒绝
    cIK_SOLVER empty;
    CHECK(!empty.ComputeWorldPositions(local, world));
}

TEST_CASE(ikSolverBuildsChainFromSkeleton) {
    sSKELETON sk = makeThreeJointSkeleton();
    cIK_SOLVER solver;
    solver.SetSkeleton(&sk);
    const std::vector<sANIM_TRANSFORM> local = bindPoseLocal(sk);

    sIK_CHAIN chain;
    CHECK(solver.BuildChain(local, 2, GNg::sVEC3{0.0f, 0.3f, 0.0f},
                            GNg::sVEC3{0.0f, 0.0f, 1.0f}, 1.0f, chain));
    CHECK_NEAR(chain.aRootPos.y, 2.0f, 1e-5);  // 髋
    CHECK_NEAR(chain.aJointPos.y, 1.0f, 1e-5); // 膝
    CHECK_NEAR(chain.aEndPos.y, 0.0f, 1e-5);   // 踝
    CHECK_NEAR(chain.aWeight, 1.0f, 1e-6);

    // 末端关节是根（没有父）：拒绝
    sIK_CHAIN bad;
    CHECK(!solver.BuildChain(local, 0, GNg::sVEC3{0, 0, 0}, GNg::sVEC3{0, 0, 1}, 1.0f, bad));
    // 只有一层父（膝是根）：拒绝（需要祖父）
    CHECK(!solver.BuildChain(local, 1, GNg::sVEC3{0, 0, 0}, GNg::sVEC3{0, 0, 1}, 1.0f, bad));
    // 越界：拒绝
    CHECK(!solver.BuildChain(local, 99, GNg::sVEC3{0, 0, 0}, GNg::sVEC3{0, 0, 1}, 1.0f, bad));
}

// 端到端：脚插进地面 0.25 m，IK 把它拉回地面
TEST_CASE(ikEndToEndFootPlantedOnGround) {
    sSKELETON sk = makeThreeJointSkeleton();
    cIK_SOLVER solver;
    solver.SetSkeleton(&sk);
    const std::vector<sANIM_TRANSFORM> local = bindPoseLocal(sk);

    sIK_CHAIN chain;
    // 地面 y=0.25（脚原本在 y=0，插进去 0.25）
    const GNg::sVEC3 groundTarget{0.0f, 0.25f, 0.0f};
    CHECK(solver.BuildChain(local, 2, groundTarget, GNg::sVEC3{0.0f, 0.0f, 1.0f}, 1.0f, chain));

    const sIK_RESULT r = solveTwoBoneIK(chain);
    CHECK(r.aReachable);
    // 踝到达地面目标
    CHECK_NEAR(dist(r.aEndPos, groundTarget), 0.0f, 1e-4);
    // 骨长保持
    CHECK_NEAR(dist(r.aJointPos, chain.aRootPos), 1.0f, 1e-3);
    CHECK_NEAR(dist(r.aEndPos, r.aJointPos), 1.0f, 1e-3);
    // 膝朝前（极向量 +Z）
    CHECK(r.aJointPos.z > 0.0f);
}
