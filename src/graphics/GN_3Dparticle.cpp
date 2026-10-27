// GN_3Dparticle.cpp — 3D 粒子：固定池、按速率发射、圆锥随机方向、重力/阻尼、寿命回收、颜色插值。
//
// 确定性：随机数用固定种子 xorshift32；同参数 + 同 dt 序列结果可逐位重复（发射槽按空闲栈顺序分配）。
// 渲染（实例缓冲/纹理/加法混合）留 P8；本文件只做 CPU 模拟。
#include <GN.h>
#include "GN_3Dparticle.h"

#include <cmath>

namespace GN3d {

namespace {

const float kParticleMaxDt = 0.1f;
const float kParticleTwoPi = 6.2831853072f;
const uint32_t kParticleSeed = 0x2545F491u;

GNg::sVEC3 makeVec3(float x, float y, float z) {
    GNg::sVEC3 v = {x, y, z};
    return v;
}

GNg::sVEC3 addVec(const GNg::sVEC3 &a, const GNg::sVEC3 &b) {
    return makeVec3(a.x + b.x, a.y + b.y, a.z + b.z);
}

GNg::sVEC3 subVec(const GNg::sVEC3 &a, const GNg::sVEC3 &b) {
    return makeVec3(a.x - b.x, a.y - b.y, a.z - b.z);
}

GNg::sVEC3 scaleVec(const GNg::sVEC3 &v, float s) {
    return makeVec3(v.x * s, v.y * s, v.z * s);
}

float dotVec(const GNg::sVEC3 &a, const GNg::sVEC3 &b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

GNg::sVEC3 crossVec(const GNg::sVEC3 &a, const GNg::sVEC3 &b) {
    return makeVec3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}

bool normalizeVec(const GNg::sVEC3 &v, GNg::sVEC3 &out) {
    const float lengthSq = dotVec(v, v);
    if(lengthSq < 1e-12f) return false;
    out = scaleVec(v, 1.0f / sqrtf(lengthSq));
    return true;
}

bool finiteVec(const GNg::sVEC3 &v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

float lerpFloat(float from, float to, float t) {
    return from + (to - from) * t;
}

GNg::sVEC4 lerpColor(const GNg::sVEC4 &from, const GNg::sVEC4 &to, float t) {
    return GNg::sVEC4(lerpFloat(from.x, to.x, t), lerpFloat(from.y, to.y, t),
                      lerpFloat(from.z, to.z, t), lerpFloat(from.w, to.w, t));
}

} // namespace

cPARTICLE_SYSTEM::cPARTICLE_SYSTEM()
    : aActiveCount(0), aRngState(kParticleSeed), aEmitAccumulator(0.0f) {
    aDesc.aOrigin = makeVec3(0.0f, 0.0f, 0.0f);
    aDesc.aDirection = makeVec3(0.0f, 1.0f, 0.0f);
    aDesc.aSpread = 0.0f;
    aDesc.aSpeedMin = 0.0f;
    aDesc.aSpeedMax = 0.0f;
    aDesc.aRate = 0.0f;
    aDesc.aMaxParticles = 0;
    aDesc.aLifeMin = 1.0f;
    aDesc.aLifeMax = 1.0f;
    aDesc.aGravity = makeVec3(0.0f, -9.81f, 0.0f);
    aDesc.aDrag = 0.0f;
    aDesc.aColorStart = GNg::sVEC4(1.0f, 1.0f, 1.0f, 1.0f);
    aDesc.aColorEnd = GNg::sVEC4(1.0f, 1.0f, 1.0f, 0.0f);
}

cPARTICLE_SYSTEM::~cPARTICLE_SYSTEM() {}

bool cPARTICLE_SYSTEM::create(const sPARTICLE_EMITTER_DESC &desc) {
    if(desc.aMaxParticles == 0) return false;
    if(!(desc.aRate >= 0.0f) || !std::isfinite(desc.aRate)) return false;
    if(!(desc.aLifeMin > 0.0f) || !std::isfinite(desc.aLifeMin)) return false;
    if(!(desc.aLifeMax >= desc.aLifeMin) || !std::isfinite(desc.aLifeMax)) return false;
    if(!(desc.aSpeedMin >= 0.0f) || !(desc.aSpeedMax >= desc.aSpeedMin)) return false;
    if(!(desc.aSpread >= 0.0f) || !(desc.aSpread <= 3.14159265f)) return false;
    if(!(desc.aDrag >= 0.0f) || !std::isfinite(desc.aDrag)) return false;
    if(!finiteVec(desc.aOrigin) || !finiteVec(desc.aGravity) || !finiteVec(desc.aDirection)) {
        return false;
    }

    aDesc = desc;
    if(!normalizeVec(aDesc.aDirection, aDesc.aDirection)) {
        aDesc.aDirection = makeVec3(0.0f, 1.0f, 0.0f);
    }
    aParticles.assign(aDesc.aMaxParticles, sPARTICLE());
    aAlive.assign(aDesc.aMaxParticles, 0);
    aFreeSlots.clear();
    aFreeSlots.reserve(aDesc.aMaxParticles);
    for(uint32_t i = 0; i < aDesc.aMaxParticles; ++i) aFreeSlots.push_back(i);
    aActiveCount = 0;
    aEmitAccumulator = 0.0f;
    aRngState = kParticleSeed;
    return true;
}

void cPARTICLE_SYSTEM::reset() {
    if(aParticles.empty()) return;
    aAlive.assign(aParticles.size(), 0);
    aFreeSlots.clear();
    for(uint32_t i = 0; i < (uint32_t)aParticles.size(); ++i) aFreeSlots.push_back(i);
    aActiveCount = 0;
    aEmitAccumulator = 0.0f;
    aRngState = kParticleSeed;
}

void cPARTICLE_SYSTEM::update(float dt) {
    if(aParticles.empty() || !(dt > 0.0f)) return;
    if(dt > kParticleMaxDt) dt = kParticleMaxDt;
    updateParticles(dt);
    aEmitAccumulator += aDesc.aRate * dt;
    emitDueParticles();
}

void cPARTICLE_SYSTEM::updateParticles(float dt) {
    const float gravityScale = dt;
    const float dragScale = 1.0f / (1.0f + aDesc.aDrag * dt);
    for(size_t i = 0; i < aParticles.size(); ++i) {
        if(!aAlive[i]) continue;
        sPARTICLE &particle = aParticles[i];
        particle.aAge += dt;
        if(particle.aAge >= particle.aLife) {
            aAlive[i] = 0;
            aFreeSlots.push_back((uint32_t)i);
            if(aActiveCount > 0) --aActiveCount;
            continue;
        }
        particle.aVelocity = addVec(particle.aVelocity, scaleVec(aDesc.aGravity, gravityScale));
        particle.aVelocity = scaleVec(particle.aVelocity, dragScale);
        particle.aPosition = addVec(particle.aPosition, scaleVec(particle.aVelocity, dt));
        const float lifeRatio = particle.aAge / particle.aLife;
        particle.aColor = lerpColor(aDesc.aColorStart, aDesc.aColorEnd, lifeRatio);
    }
}

void cPARTICLE_SYSTEM::emitDueParticles() {
    while(aEmitAccumulator >= 1.0f) {
        uint32_t slot = 0;
        if(!findFreeSlot(slot)) {
            if(aEmitAccumulator > 1.0f) aEmitAccumulator = 1.0f; // 池满：限制积压，避免后续爆发
            return;
        }
        spawnParticle(slot);
        aEmitAccumulator -= 1.0f;
    }
}

bool cPARTICLE_SYSTEM::findFreeSlot(uint32_t &outSlot) {
    if(aFreeSlots.empty()) return false;
    outSlot = aFreeSlots.back();
    aFreeSlots.pop_back();
    return true;
}

void cPARTICLE_SYSTEM::spawnParticle(uint32_t slot) {
    sPARTICLE &particle = aParticles[slot];
    particle.aPosition = aDesc.aOrigin;
    const GNg::sVEC3 direction = randomConeDirection();
    const float speed = lerpFloat(aDesc.aSpeedMin, aDesc.aSpeedMax, nextRandom());
    particle.aVelocity = scaleVec(direction, speed);
    particle.aAge = 0.0f;
    particle.aLife = lerpFloat(aDesc.aLifeMin, aDesc.aLifeMax, nextRandom());
    particle.aSize = 1.0f;
    particle.aColor = aDesc.aColorStart;
    aAlive[slot] = 1;
    ++aActiveCount;
}

float cPARTICLE_SYSTEM::nextRandom() {
    aRngState ^= aRngState << 13;
    aRngState ^= aRngState >> 17;
    aRngState ^= aRngState << 5;
    return (float)(aRngState & 0xFFFFFFu) / 16777216.0f; // [0,1)
}

GNg::sVEC3 cPARTICLE_SYSTEM::randomConeDirection() {
    if(aDesc.aSpread <= 0.0f) return aDesc.aDirection;
    const GNg::sVEC3 helper = (std::fabs(aDesc.aDirection.y) < 0.9f) ? makeVec3(0.0f, 1.0f, 0.0f)
                                                                   : makeVec3(1.0f, 0.0f, 0.0f);
    GNg::sVEC3 axisU;
    normalizeVec(crossVec(helper, aDesc.aDirection), axisU);
    const GNg::sVEC3 axisV = crossVec(aDesc.aDirection, axisU);
    const float cosMax = cosf(aDesc.aSpread);
    const float cosTheta = 1.0f - nextRandom() * (1.0f - cosMax); // 圆锥内均匀
    const float sinTheta = sqrtf(1.0f - cosTheta * cosTheta > 0.0f ? 1.0f - cosTheta * cosTheta : 0.0f);
    const float phi = kParticleTwoPi * nextRandom();
    const GNg::sVEC3 lateral = addVec(scaleVec(axisU, cosf(phi)), scaleVec(axisV, sinf(phi)));
    return addVec(scaleVec(aDesc.aDirection, cosTheta), scaleVec(lateral, sinTheta));
}

} // namespace GN3d
