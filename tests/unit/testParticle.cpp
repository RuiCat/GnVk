// testParticle.cpp — 3D 粒子：参数校验、发射/寿终、容量、重力、确定性、Reset。
#include "testFramework.h"

#include <GN.h>
#include "GN_3Dparticle.h"

#include <cmath>
#include <cstring>

using namespace GN3d;

namespace {

sPARTICLE_EMITTER_DESC makeDesc() {
    sPARTICLE_EMITTER_DESC desc;
    desc.aOrigin = GNg::sVEC3{0.0f, 0.0f, 0.0f};
    desc.aDirection = GNg::sVEC3{0.0f, 1.0f, 0.0f};
    desc.aSpread = 0.5f;
    desc.aSpeedMin = 1.0f;
    desc.aSpeedMax = 2.0f;
    desc.aRate = 100.0f;
    desc.aMaxParticles = 200;
    desc.aLifeMin = 0.5f;
    desc.aLifeMax = 1.0f;
    desc.aGravity = GNg::sVEC3{0.0f, -9.81f, 0.0f};
    desc.aDrag = 0.0f;
    desc.aColorStart = GNg::sVEC4(1.0f, 1.0f, 1.0f, 1.0f);
    desc.aColorEnd = GNg::sVEC4(1.0f, 0.0f, 0.0f, 0.0f);
    return desc;
}

bool allFinite(const sPARTICLE &particle) {
    return std::isfinite(particle.aPosition.x) && std::isfinite(particle.aPosition.y) &&
           std::isfinite(particle.aPosition.z) && std::isfinite(particle.aVelocity.x) &&
           std::isfinite(particle.aVelocity.y) && std::isfinite(particle.aVelocity.z) &&
           std::isfinite(particle.aAge) && std::isfinite(particle.aLife);
}

} // namespace

// 参数校验：非法值拒绝；合法创建后容量正确、初始无存活粒子。
TEST_CASE(particleCreateValidation) {
    cPARTICLE_SYSTEM system;
    CHECK(!system.isCreated());
    CHECK_EQ(system.capacity(), 0u);

    sPARTICLE_EMITTER_DESC bad = makeDesc();
    bad.aMaxParticles = 0;
    CHECK(!system.create(bad));
    bad = makeDesc();
    bad.aRate = -1.0f;
    CHECK(!system.create(bad));
    bad = makeDesc();
    bad.aLifeMin = 0.0f;
    CHECK(!system.create(bad));
    bad = makeDesc();
    bad.aLifeMax = 0.1f; // < aLifeMin
    CHECK(!system.create(bad));
    bad = makeDesc();
    bad.aSpeedMax = 0.5f; // < aSpeedMin
    CHECK(!system.create(bad));
    bad = makeDesc();
    bad.aSpread = -0.1f;
    CHECK(!system.create(bad));
    bad = makeDesc();
    bad.aDrag = -0.1f;
    CHECK(!system.create(bad));

    cPARTICLE_SYSTEM valid;
    CHECK(valid.create(makeDesc()));
    CHECK(valid.isCreated());
    CHECK_EQ(valid.capacity(), 200u);
    CHECK_EQ(valid.activeCount(), 0u);
    CHECK(valid.particles() != 0);
}

// 发射与寿终：rate=100、dt=0.1 ⇒ 10 个；停发射后超过寿命全部回收。
TEST_CASE(particleEmitsAndExpires) {
    cPARTICLE_SYSTEM system;
    CHECK(system.create(makeDesc()));
    system.update(0.1f);
    CHECK_EQ(system.activeCount(), 10u); // 100 * 0.1

    system.setRate(0.0f);
    for(int i = 0; i < 12; ++i) system.update(0.1f); // 累计 1.2s（dt 被钳在 0.1s）
    CHECK_EQ(system.activeCount(), 0u);
}

// 容量：发射速率远大于池容量时，存活数被钳在 maxParticles。
TEST_CASE(particleCapacityRespected) {
    cPARTICLE_SYSTEM system;
    sPARTICLE_EMITTER_DESC desc = makeDesc();
    desc.aRate = 10000.0f;
    desc.aMaxParticles = 50;
    desc.aLifeMin = 5.0f;
    desc.aLifeMax = 5.0f;
    CHECK(system.create(desc));
    system.update(0.1f);
    CHECK_EQ(system.activeCount(), 50u);
    CHECK_EQ(system.capacity(), 50u);
}

// 重力：初速 0 的单个粒子，dt=0.2 后 v.y = -10*0.2，位置 y < 0。
TEST_CASE(particleGravityMovesParticle) {
    cPARTICLE_SYSTEM system;
    sPARTICLE_EMITTER_DESC desc = makeDesc();
    desc.aRate = 10.0f;
    desc.aSpeedMin = 0.0f;
    desc.aSpeedMax = 0.0f;
    desc.aLifeMin = 5.0f;
    desc.aLifeMax = 5.0f;
    desc.aGravity = GNg::sVEC3{0.0f, -10.0f, 0.0f};
    CHECK(system.create(desc));
    system.update(0.1f); // 10 * 0.1 = 1 个
    CHECK_EQ(system.activeCount(), 1u);
    system.setRate(0.0f);
    const sPARTICLE *particles = system.particles();
    CHECK(particles != 0);
    if(!particles) return;
    uint32_t aliveIndex = 0;
    bool found = false;
    for(uint32_t i = 0; i < system.capacity(); ++i) {
        if(system.alive(i)) {
            aliveIndex = i;
            found = true;
            break;
        }
    }
    CHECK(found);
    if(!found) return;
    system.update(0.1f); // dt 钳在 0.1s：v.y = -10*0.1
    system.update(0.1f); // 再一步：v.y = -2.0
    CHECK(allFinite(particles[aliveIndex]));
    CHECK_NEAR(particles[aliveIndex].aVelocity.y, -2.0f, 1e-3f); // -10 * 0.2
    CHECK(particles[aliveIndex].aPosition.y < 0.0f);
}

// 确定性：同参数两次模拟的池内容逐位相同。
TEST_CASE(particleDeterministicRepeat) {
    cPARTICLE_SYSTEM first;
    cPARTICLE_SYSTEM second;
    CHECK(first.create(makeDesc()));
    CHECK(second.create(makeDesc()));
    for(int i = 0; i < 60; ++i) {
        first.update(1.0f / 60.0f);
        second.update(1.0f / 60.0f);
    }
    CHECK_EQ(first.activeCount(), second.activeCount());
    const sPARTICLE *a = first.particles();
    const sPARTICLE *b = second.particles();
    CHECK(a != 0 && b != 0);
    if(!a || !b) return;
    CHECK(std::memcmp(a, b, sizeof(sPARTICLE) * first.capacity()) == 0);
}

// Reset：清空存活粒子与累加器；参数与容量保留。
TEST_CASE(particleResetClears) {
    cPARTICLE_SYSTEM system;
    CHECK(system.create(makeDesc()));
    system.update(0.5f);
    CHECK(system.activeCount() > 0u);
    system.reset();
    CHECK_EQ(system.activeCount(), 0u);
    CHECK_EQ(system.capacity(), 200u);
    system.update(0.1f); // 重置后仍能正常发射
    CHECK(system.activeCount() > 0u);
}
