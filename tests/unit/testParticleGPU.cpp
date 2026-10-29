// testParticleGPU.cpp — P9 GPU 粒子：内存布局、参数校验、CPU 回退步进（GPU 对拍参考实现）。
#include "testFramework.h"

#include <GN.h>
#include "GN_3DparticleGPU.h"

#include <cmath>
#include <cstddef>
#include <cstring>

using namespace GN3d;

namespace {

sGPU_PARTICLE makeParticle(float px, float py, float pz, float vx, float vy, float vz,
                           float life, float alive) {
    sGPU_PARTICLE p;
    std::memset(&p, 0, sizeof(p));
    p.aPos[0] = px;
    p.aPos[1] = py;
    p.aPos[2] = pz;
    p.aVel[0] = vx;
    p.aVel[1] = vy;
    p.aVel[2] = vz;
    p.aLife = life;
    p.aAlive = alive;
    p.aSize = 1.0f;
    return p;
}

sPARTICLE_DRAW_PARAMS makeDrawParams() {
    sPARTICLE_DRAW_PARAMS params;
    std::memset(&params, 0, sizeof(params));
    for(int i = 0; i < 4; ++i) params.aViewProj[i * 5] = 1.0f; // 单位矩阵
    params.aCameraRight[0] = 1.0f;
    params.aCameraUp[1] = 1.0f;
    params.aSize = 0.5f;
    params.aColorStart[3] = 1.0f;
    params.aColorEnd[3] = 1.0f;
    params.aCount = 1;
    return params;
}

} // namespace

// 内存布局：64 字节、四个 vec4 对齐、SSBO 上限 262144（= 16MB）。
TEST_CASE(particleGpuLayout) {
    CHECK_EQ(sizeof(sGPU_PARTICLE), (size_t)64);
    CHECK_EQ(offsetof(sGPU_PARTICLE, aPos), (size_t)0);
    CHECK_EQ(offsetof(sGPU_PARTICLE, aAge), (size_t)12);
    CHECK_EQ(offsetof(sGPU_PARTICLE, aVel), (size_t)16);
    CHECK_EQ(offsetof(sGPU_PARTICLE, aLife), (size_t)28);
    CHECK_EQ(offsetof(sGPU_PARTICLE, aColor), (size_t)32);
    CHECK_EQ(offsetof(sGPU_PARTICLE, aSize), (size_t)48);
    CHECK_EQ(offsetof(sGPU_PARTICLE, aAlive), (size_t)56);
    CHECK_EQ(offsetof(sGPU_PARTICLE, aPad), (size_t)60);
    CHECK_EQ(PARTICLE_GPU_MAX_CAPACITY, 262144u);
    CHECK_EQ(PARTICLE_GPU_WORKGROUP_SIZE, 64u);
    // 绘制 UBO 镜像 = 144B（mat4 + 5×vec4），必须与 std140 布局一致。
    CHECK_EQ(sizeof(sPARTICLE_DRAW_PARAMS), (size_t)144);
}

// 模拟参数：负阻尼、NaN 重力都拒绝。
TEST_CASE(particleGpuSimDescValidation) {
    sPARTICLE_SIM_DESC ok = {{0.0f, -9.81f, 0.0f}, 0.25f};
    CHECK(validateParticleSimDesc(ok));

    sPARTICLE_SIM_DESC negativeDrag = ok;
    negativeDrag.aDrag = -0.1f;
    CHECK(!validateParticleSimDesc(negativeDrag));

    sPARTICLE_SIM_DESC nanGravity = ok;
    nanGravity.aGravity[1] = std::nanf("");
    CHECK(!validateParticleSimDesc(nanGravity));

    sPARTICLE_SIM_DESC infDrag = ok;
    infDrag.aDrag = INFINITY;
    CHECK(!validateParticleSimDesc(infDrag));
}

// 绘制参数：size/count 必须为正，VP 与方向必须有限。
TEST_CASE(particleGpuDrawParamsValidation) {
    sPARTICLE_DRAW_PARAMS ok = makeDrawParams();
    CHECK(validateParticleDrawParams(ok));

    sPARTICLE_DRAW_PARAMS zeroSize = ok;
    zeroSize.aSize = 0.0f;
    CHECK(!validateParticleDrawParams(zeroSize));

    sPARTICLE_DRAW_PARAMS zeroCount = ok;
    zeroCount.aCount = 0;
    CHECK(!validateParticleDrawParams(zeroCount));

    sPARTICLE_DRAW_PARAMS nanVp = ok;
    nanVp.aViewProj[3] = std::nanf("");
    CHECK(!validateParticleDrawParams(nanVp));
}

// CPU 回退 = compute 的参考实现：半隐式 Euler + 阻尼。
TEST_CASE(particleCpuStepGravityDrag) {
    sPARTICLE_SIM_DESC desc = {{0.0f, -10.0f, 0.0f}, 0.0f};
    sGPU_PARTICLE p = makeParticle(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 10.0f, 1.0f);
    stepParticlesCpu(&p, 1, desc, 0.1f);
    CHECK_NEAR(p.aVel[1], -1.0f, 1e-5);
    CHECK_NEAR(p.aPos[1], -0.1f, 1e-5);
    CHECK_NEAR(p.aAge, 0.1f, 1e-5);
    CHECK_NEAR(p.aAlive, 1.0f, 1e-6);

    // 阻尼：v = (10 + 0) / (1 + 1*0.1) = 9.0909…
    sPARTICLE_SIM_DESC drag = {{0.0f, 0.0f, 0.0f}, 1.0f};
    sGPU_PARTICLE q = makeParticle(0.0f, 0.0f, 0.0f, 0.0f, 10.0f, 0.0f, 10.0f, 1.0f);
    stepParticlesCpu(&q, 1, drag, 0.1f);
    CHECK_NEAR(q.aVel[1], 10.0f / 1.1f, 1e-5);
    CHECK_NEAR(q.aPos[1], 10.0f / 1.1f * 0.1f, 1e-5);
}

// 寿命：age + dt >= life ⇒ 清 aAlive；已死槽位不被积分改动。
TEST_CASE(particleCpuStepLifetime) {
    sPARTICLE_SIM_DESC desc = {{0.0f, -10.0f, 0.0f}, 0.0f};
    sGPU_PARTICLE dead = makeParticle(1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 10.0f, 0.0f);
    const sGPU_PARTICLE before = dead;
    stepParticlesCpu(&dead, 1, desc, 0.1f);
    CHECK(std::memcmp(&dead, &before, sizeof(dead)) == 0);

    sGPU_PARTICLE expiring = makeParticle(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.05f, 1.0f);
    stepParticlesCpu(&expiring, 1, desc, 0.1f);
    CHECK_NEAR(expiring.aAlive, 0.0f, 1e-6);

    // 刚好到期（age == life）也算死。
    sGPU_PARTICLE exact = makeParticle(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.1f, 1.0f);
    stepParticlesCpu(&exact, 1, desc, 0.1f);
    CHECK_NEAR(exact.aAlive, 0.0f, 1e-6);
}

// 非法输入：空指针、dt <= 0、count 0、非法参数都不改动数据。
TEST_CASE(particleCpuStepGuards) {
    sPARTICLE_SIM_DESC desc = {{0.0f, -10.0f, 0.0f}, 0.0f};
    sGPU_PARTICLE p = makeParticle(0.0f, 0.0f, 0.0f, 1.0f, 2.0f, 3.0f, 10.0f, 1.0f);
    const sGPU_PARTICLE before = p;

    sGPU_PARTICLE empty = p;
    stepParticlesCpu(&p, 1, desc, 0.0f); // dt = 0 忽略
    CHECK(std::memcmp(&p, &before, sizeof(p)) == 0);
    stepParticlesCpu(&p, 1, desc, -0.5f); // 负 dt 忽略
    CHECK(std::memcmp(&p, &before, sizeof(p)) == 0);
    stepParticlesCpu(&p, 0, desc, 0.1f); // count = 0 忽略
    CHECK(std::memcmp(&p, &before, sizeof(p)) == 0);
    stepParticlesCpu(0, 1, desc, 0.1f); // 空指针安全

    sPARTICLE_SIM_DESC bad = desc;
    bad.aDrag = -1.0f;
    stepParticlesCpu(&p, 1, bad, 0.1f); // 非法参数忽略
    CHECK(std::memcmp(&p, &before, sizeof(p)) == 0);

    stepParticlesCpu(&empty, 1, desc, 0.1f);
    CHECK(std::memcmp(&empty, &before, sizeof(empty)) != 0); // 合法步进必定改动数据
}
