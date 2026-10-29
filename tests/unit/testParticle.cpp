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

// 复算 cpp 内的主随机流（固定种子 xorshift32），用于验证旧 desc 的速度公式。
uint32_t xorshift32Next(uint32_t &state) {
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

float xorshift32Float(uint32_t &state) {
    return (float)(xorshift32Next(state) & 0xFFFFFFu) / 16777216.0f;
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

// BOX 生成：位置在 aOrigin ± halfExtent 内，三轴都出现非零偏移（速度/重力置 0 以固定位置）。
TEST_CASE(particleBoxSpawnsInsideVolume) {
    cPARTICLE_SYSTEM system;
    sPARTICLE_EMITTER_DESC desc = makeDesc();
    desc.aShape = ParticleShape::BOX;
    desc.aOrigin = GNg::sVEC3{5.0f, 0.0f, 0.0f};
    desc.aBoxHalfExtent = GNg::sVEC3{2.0f, 1.0f, 3.0f};
    desc.aSpeedMin = 0.0f;
    desc.aSpeedMax = 0.0f;
    desc.aGravity = GNg::sVEC3{0.0f, 0.0f, 0.0f};
    desc.aLifeMin = 5.0f;
    desc.aLifeMax = 5.0f;
    desc.aRate = 200.0f;
    CHECK(system.create(desc));
    for(int i = 0; i < 3; ++i) system.update(0.1f);
    CHECK_EQ(system.activeCount(), 60u); // 200 * 0.1 * 3 帧
    const sPARTICLE *particles = system.particles();
    CHECK(particles != 0);
    if(!particles) return;
    bool offsetX = false;
    bool offsetY = false;
    bool offsetZ = false;
    for(uint32_t i = 0; i < system.capacity(); ++i) {
        if(!system.alive(i)) continue;
        const GNg::sVEC3 &p = particles[i].aPosition;
        CHECK(p.x >= 3.0f && p.x <= 7.0f);
        CHECK(p.y >= -1.0f && p.y <= 1.0f);
        CHECK(p.z >= -3.0f && p.z <= 3.0f);
        if(p.x != 5.0f) offsetX = true;
        if(p.y != 0.0f) offsetY = true;
        if(p.z != 0.0f) offsetZ = true;
    }
    CHECK(offsetX);
    CHECK(offsetY);
    CHECK(offsetZ);
}

// BOX 校验：全 0 / 任一轴为负都拒绝；至少一轴 > 0 合法；CONE 忽略盒范围。
TEST_CASE(particleBoxRejectsInvalidExtent) {
    cPARTICLE_SYSTEM system;
    sPARTICLE_EMITTER_DESC desc = makeDesc();
    desc.aShape = ParticleShape::BOX;
    desc.aBoxHalfExtent = GNg::sVEC3{0.0f, 0.0f, 0.0f};
    CHECK(!system.create(desc));
    CHECK(!system.isCreated());

    desc.aBoxHalfExtent = GNg::sVEC3{-1.0f, 1.0f, 1.0f};
    CHECK(!system.create(desc));

    desc.aBoxHalfExtent = GNg::sVEC3{1.0f, 0.0f, 0.0f};
    CHECK(system.create(desc)); // 只有一轴 > 0 也合法

    desc.aShape = ParticleShape::CONE;
    desc.aBoxHalfExtent = GNg::sVEC3{0.0f, 0.0f, 0.0f};
    cPARTICLE_SYSTEM cone;
    CHECK(cone.create(desc)); // CONE 忽略盒范围
}

// 尺寸范围：所有 spawn 的 aSize ∈ [0.5, 2.0]，且样本确实覆盖两端。
TEST_CASE(particleSizeRangeRespected) {
    cPARTICLE_SYSTEM system;
    sPARTICLE_EMITTER_DESC desc = makeDesc();
    desc.aSizeMin = 0.5f;
    desc.aSizeMax = 2.0f;
    desc.aSpeedMin = 0.0f;
    desc.aSpeedMax = 0.0f;
    desc.aGravity = GNg::sVEC3{0.0f, 0.0f, 0.0f};
    desc.aLifeMin = 5.0f;
    desc.aLifeMax = 5.0f;
    desc.aRate = 200.0f;
    CHECK(system.create(desc));
    for(int i = 0; i < 3; ++i) system.update(0.1f);
    CHECK_EQ(system.activeCount(), 60u);
    const sPARTICLE *particles = system.particles();
    CHECK(particles != 0);
    if(!particles) return;
    bool sawSmall = false;
    bool sawLarge = false;
    for(uint32_t i = 0; i < system.capacity(); ++i) {
        if(!system.alive(i)) continue;
        const float size = particles[i].aSize;
        CHECK(size >= 0.5f && size <= 2.0f);
        if(size < 0.9f) sawSmall = true;
        if(size > 1.2f) sawLarge = true;
    }
    CHECK(sawSmall);
    CHECK(sawLarge);
}

// 风：1s 内 vx 单调趋近 5（9 次 0.2 混合），x 严格单调增；response=0 对照 vx 恒 0。
TEST_CASE(particleWindConvergesToWind) {
    cPARTICLE_SYSTEM system;
    sPARTICLE_EMITTER_DESC desc = makeDesc();
    desc.aSpread = 0.0f;
    desc.aSpeedMin = 0.0f;
    desc.aSpeedMax = 0.0f;
    desc.aGravity = GNg::sVEC3{0.0f, 0.0f, 0.0f};
    desc.aDrag = 0.0f;
    desc.aRate = 10.0f;
    desc.aMaxParticles = 32;
    desc.aLifeMin = 5.0f;
    desc.aLifeMax = 5.0f;
    desc.aWind = GNg::sVEC3{5.0f, 0.0f, 0.0f};
    desc.aWindResponse = 2.0f;
    CHECK(system.create(desc));
    const uint32_t firstSlot = system.capacity() - 1; // 首个粒子取空闲栈顶（最大下标）
    system.update(0.1f);
    CHECK_EQ(system.activeCount(), 1u);
    CHECK(system.alive(firstSlot));
    const sPARTICLE *particles = system.particles();
    CHECK(particles != 0);
    if(!particles) return;
    float previousX = particles[firstSlot].aPosition.x;
    bool monotonic = true;
    for(int i = 0; i < 9; ++i) {
        system.update(0.1f);
        const float x = particles[firstSlot].aPosition.x;
        if(!(x > previousX)) monotonic = false;
        previousX = x;
    }
    CHECK(monotonic);
    const float vx = particles[firstSlot].aVelocity.x;
    CHECK(vx > 2.0f);
    CHECK(vx <= 5.0f);
    // 9 步：v(k+1) = 0.8*v(k) + 1（blend = 2.0 * 0.1），v(9) = 5*(1-0.8^9)
    CHECK_NEAR(vx, 5.0f * (1.0f - std::pow(0.8f, 9.0f)), 1e-3f);

    // 对照：response=0 ⇒ 风完全跳过，vx 与 x 恒为 0。
    cPARTICLE_SYSTEM control;
    sPARTICLE_EMITTER_DESC controlDesc = desc;
    controlDesc.aWindResponse = 0.0f;
    CHECK(control.create(controlDesc));
    for(int i = 0; i < 10; ++i) control.update(0.1f);
    CHECK(control.activeCount() > 0u);
    const sPARTICLE *controlParticles = control.particles();
    CHECK(controlParticles != 0);
    if(!controlParticles) return;
    for(uint32_t i = 0; i < control.capacity(); ++i) {
        if(!control.alive(i)) continue;
        CHECK_EQ(controlParticles[i].aVelocity.x, 0.0f);
        CHECK_EQ(controlParticles[i].aPosition.x, 0.0f);
    }
}

// 回收盒：向上 20/s 跑 2s，y 被 wrap 回 [-5,5)；不设回收盒的对照组有粒子飞过 5。
TEST_CASE(particleRecycleWrapsInsideBox) {
    cPARTICLE_SYSTEM system;
    sPARTICLE_EMITTER_DESC desc = makeDesc();
    desc.aShape = ParticleShape::BOX;
    desc.aOrigin = GNg::sVEC3{0.0f, 0.0f, 0.0f};
    desc.aBoxHalfExtent = GNg::sVEC3{1.0f, 1.0f, 1.0f};
    desc.aDirection = GNg::sVEC3{0.0f, 1.0f, 0.0f};
    desc.aSpread = 0.0f;
    desc.aSpeedMin = 20.0f;
    desc.aSpeedMax = 20.0f;
    desc.aGravity = GNg::sVEC3{0.0f, 0.0f, 0.0f};
    desc.aRate = 20.0f;
    desc.aMaxParticles = 64;
    desc.aLifeMin = 5.0f;
    desc.aLifeMax = 5.0f;
    CHECK(system.create(desc));
    system.setRecycleBox(GNg::sVEC3{0.0f, 0.0f, 0.0f}, GNg::sVEC3{5.0f, 5.0f, 5.0f});
    for(int i = 0; i < 20; ++i) system.update(0.1f); // 2s
    CHECK(system.activeCount() > 0u);
    const sPARTICLE *particles = system.particles();
    CHECK(particles != 0);
    if(!particles) return;
    for(uint32_t i = 0; i < system.capacity(); ++i) {
        if(!system.alive(i)) continue;
        CHECK(particles[i].aPosition.y >= -5.0f);
        CHECK(particles[i].aPosition.y < 5.0f);
    }

    // 对照组：同一参数、不设回收盒 ⇒ 2s 后必然有粒子 y > 5。
    cPARTICLE_SYSTEM control;
    CHECK(control.create(desc));
    for(int i = 0; i < 20; ++i) control.update(0.1f);
    CHECK(control.activeCount() > 0u);
    const sPARTICLE *controlParticles = control.particles();
    CHECK(controlParticles != 0);
    if(!controlParticles) return;
    bool escaped = false;
    for(uint32_t i = 0; i < control.capacity(); ++i) {
        if(!control.alive(i)) continue;
        if(controlParticles[i].aPosition.y > 5.0f) escaped = true;
    }
    CHECK(escaped);
}

// 旧 desc（不设新字段）：aSize 恰为 1.0，速度仍是旧公式（固定种子首个随机数），风无效。
TEST_CASE(particleLegacyDescUnchanged) {
    cPARTICLE_SYSTEM system;
    sPARTICLE_EMITTER_DESC desc = makeDesc();
    desc.aSpread = 0.0f; // 方向取 aDirection，不消耗随机数；首个随机数给速度
    desc.aSpeedMin = 1.0f;
    desc.aSpeedMax = 2.0f;
    desc.aLifeMin = 5.0f;
    desc.aLifeMax = 5.0f;
    desc.aRate = 10.0f;
    desc.aMaxParticles = 1;
    CHECK(system.create(desc));
    const sPARTICLE *particles = system.particles();
    CHECK(particles != 0);
    if(!particles) return;
    system.update(0.1f);
    CHECK_EQ(system.activeCount(), 1u);
    uint32_t state = 0x2545F491u; // 与 cpp 中 kParticleSeed 相同
    const float expectedSpeed = 1.0f + (2.0f - 1.0f) * xorshift32Float(state);
    CHECK_NEAR(particles[0].aVelocity.y, expectedSpeed, 1e-6f);
    CHECK_EQ(particles[0].aVelocity.x, 0.0f);
    CHECK_EQ(particles[0].aVelocity.z, 0.0f);
    CHECK_EQ(particles[0].aSize, 1.0f); // sizeMin=sizeMax=1 ⇒ 恰为 1.0
    // 默认 aWindResponse=0：再跑一帧风仍不影响 x/z 速度。
    system.update(0.1f);
    CHECK_EQ(particles[0].aVelocity.x, 0.0f);
    CHECK_EQ(particles[0].aVelocity.z, 0.0f);
}

// 池满后的稳态：发射器必须当帧补齐空出的槽位，不能让存活数被寿命抽干再瞬间灌满。
// 旧实现（池满时把积压钳到 1）会产生锯齿：alive 从满池掉到很低再冲回，画面上表现为
// 「粒子分层 / 空白间隔」。这里用 BOX + 回收盒模拟查看器的雪，检查后段 alive 持续贴近容量。
TEST_CASE(particleFullPoolStaysSteady) {
    cPARTICLE_SYSTEM system;
    sPARTICLE_EMITTER_DESC desc = makeDesc();
    desc.aShape = ParticleShape::BOX;
    desc.aBoxHalfExtent = GNg::sVEC3{5.0f, 0.0f, 5.0f};
    desc.aOrigin = GNg::sVEC3{0.0f, 10.0f, 0.0f};
    desc.aDirection = GNg::sVEC3{0.0f, -1.0f, 0.0f};
    desc.aSpread = 0.25f;
    desc.aSpeedMin = 2.0f;
    desc.aSpeedMax = 4.0f;
    desc.aGravity = GNg::sVEC3{0.0f, -2.5f, 0.0f};
    desc.aRate = 30000.0f; // 远高于填满所需，池会满
    desc.aMaxParticles = 2000;
    desc.aLifeMin = 3.0f;
    desc.aLifeMax = 4.0f;
    desc.aSizeMin = 0.15f;
    desc.aSizeMax = 0.25f;
    CHECK(system.create(desc));
    system.setRecycleBox(GNg::sVEC3{0.0f, 5.0f, 0.0f}, GNg::sVEC3{5.0f, 5.0f, 5.0f});

    // 预热 3 s（填满池），随后统计 5 s 内的存活数。
    for (int i = 0; i < 180; ++i) system.update(1.0f / 60.0f);
    uint32_t minAlive = 0xFFFFFFFFu;
    uint32_t maxAlive = 0;
    for (int i = 0; i < 300; ++i) {
        system.update(1.0f / 60.0f);
        const uint32_t alive = system.activeCount();
        if (alive < minAlive) minAlive = alive;
        if (alive > maxAlive) maxAlive = alive;
    }
    std::printf("  full-pool: minAlive=%u maxAlive=%u cap=%u\n", minAlive, maxAlive,
                system.capacity());
    // 稳态必须贴近容量（≥95%），且波动很小（min/max ≥ 0.97）。
    CHECK(minAlive >= (system.capacity() * 95u) / 100u);
    CHECK(minAlive * 100u >= maxAlive * 97u);
}
