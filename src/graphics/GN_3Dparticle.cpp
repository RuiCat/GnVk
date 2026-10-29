// GN_3Dparticle.cpp — 3D 粒子：固定池、按速率发射、CONE/BOX 生成、重力/阻尼/风、寿命回收、
// 逐轴回收盒折返、尺寸范围、颜色插值。
//
// 确定性：主随机流（方向/速度/寿命）与尺寸随机流分开，均用固定种子 xorshift32；
// 同参数 + 同 dt 序列结果可逐位重复（发射槽按空闲栈顺序分配）。尺寸流独立 ⇒ 旧 desc 逐位不变。
// 渲染（实例缓冲/纹理/加法混合）留 P8；本文件只做 CPU 模拟。
#include <GN.h>
#include "GN_3Dparticle.h"

#include <cmath>

namespace GN3d {

namespace {

const float kParticleMaxDt = 0.1f;
const float kParticleTwoPi = 6.2831853072f;
const uint32_t kParticleSeed = 0x2545F491u;
const uint32_t kParticleSizeSeed = 0x9E3779B9u;

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

// 逐轴折返：把坐标 wrap 到 [center-half, center+half)；half <= 0 的轴不动。
void wrapCoordinate(float &coordinate, float center, float halfExtent) {
    if(!(halfExtent > 0.0f)) return;
    const float span = 2.0f * halfExtent;
    float offset = fmodf(coordinate - (center - halfExtent), span);
    if(offset < 0.0f) offset += span;
    coordinate = center - halfExtent + offset;
}

GNg::sVEC4 lerpColor(const GNg::sVEC4 &from, const GNg::sVEC4 &to, float t) {
    return GNg::sVEC4(lerpFloat(from.x, to.x, t), lerpFloat(from.y, to.y, t),
                      lerpFloat(from.z, to.z, t), lerpFloat(from.w, to.w, t));
}

} // namespace

cPARTICLE_SYSTEM::cPARTICLE_SYSTEM()
    : aActiveCount(0), aRngState(kParticleSeed), aSizeRngState(kParticleSizeSeed),
      aEmitAccumulator(0.0f) {
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
    aRecycleCenter = makeVec3(0.0f, 0.0f, 0.0f);
    aRecycleHalfExtent = makeVec3(0.0f, 0.0f, 0.0f);
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
    if(!(desc.aSizeMin > 0.0f) || !std::isfinite(desc.aSizeMin)) return false;
    if(!(desc.aSizeMax >= desc.aSizeMin) || !std::isfinite(desc.aSizeMax)) return false;
    if(!finiteVec(desc.aWind)) return false;
    if(!(desc.aWindResponse >= 0.0f) || !std::isfinite(desc.aWindResponse)) return false;
    if(desc.aShape == ParticleShape::BOX) {
        // BOX：三轴 >= 0、至少一轴 > 0；CONE 忽略 aBoxHalfExtent（可为 0）。
        if(!finiteVec(desc.aBoxHalfExtent)) return false;
        if(desc.aBoxHalfExtent.x < 0.0f || desc.aBoxHalfExtent.y < 0.0f ||
           desc.aBoxHalfExtent.z < 0.0f) {
            return false;
        }
        if(!(desc.aBoxHalfExtent.x > 0.0f || desc.aBoxHalfExtent.y > 0.0f ||
             desc.aBoxHalfExtent.z > 0.0f)) {
            return false;
        }
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
    aSizeRngState = kParticleSizeSeed;
    aRecycleCenter = makeVec3(0.0f, 0.0f, 0.0f);
    aRecycleHalfExtent = makeVec3(0.0f, 0.0f, 0.0f);
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
    aSizeRngState = kParticleSizeSeed;
}

void cPARTICLE_SYSTEM::update(float dt) {
    if(aParticles.empty() || !(dt > 0.0f)) return;
    if(dt > kParticleMaxDt) dt = kParticleMaxDt;
    updateParticles(dt);
    aEmitAccumulator += aDesc.aRate * dt;
    emitDueParticles(dt);
}

void cPARTICLE_SYSTEM::updateParticles(float dt) {
    const float gravityScale = dt;
    const float dragScale = 1.0f / (1.0f + aDesc.aDrag * dt);
    // response <= 0 时完全跳过风（不产生任何浮点运算，旧 desc 逐位不变）。
    const bool windActive = aDesc.aWindResponse > 0.0f;
    float windBlend = 0.0f;
    if(windActive) {
        windBlend = aDesc.aWindResponse * dt;
        if(windBlend > 1.0f) windBlend = 1.0f;
    }
    const bool recycleActive = aRecycleHalfExtent.x > 0.0f || aRecycleHalfExtent.y > 0.0f ||
                               aRecycleHalfExtent.z > 0.0f;
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
        if(windActive) {
            particle.aVelocity.x += (aDesc.aWind.x - particle.aVelocity.x) * windBlend;
            particle.aVelocity.y += (aDesc.aWind.y - particle.aVelocity.y) * windBlend;
            particle.aVelocity.z += (aDesc.aWind.z - particle.aVelocity.z) * windBlend;
        }
        particle.aPosition = addVec(particle.aPosition, scaleVec(particle.aVelocity, dt));
        if(recycleActive) {
            wrapCoordinate(particle.aPosition.x, aRecycleCenter.x, aRecycleHalfExtent.x);
            wrapCoordinate(particle.aPosition.y, aRecycleCenter.y, aRecycleHalfExtent.y);
            wrapCoordinate(particle.aPosition.z, aRecycleCenter.z, aRecycleHalfExtent.z);
        }
        const float lifeRatio = particle.aAge / particle.aLife;
        particle.aColor = lerpColor(aDesc.aColorStart, aDesc.aColorEnd, lifeRatio);
    }
}

void cPARTICLE_SYSTEM::emitDueParticles(float dt) {
    if(aEmitAccumulator < 1.0f) return;
    uint32_t want = (uint32_t)aEmitAccumulator;
    if(want > (uint32_t)aFreeSlots.size()) {
        want = (uint32_t)aFreeSlots.size();
        // 池满：只保留至多「一个最大帧（0.1 s）」的待发射量，避免长停顿后爆发；
        // 旧实现把积压钳到 1，池满后每帧只补 1 颗，存活数会被寿命抽干到近空再瞬间灌满，
        // 表现为「粒子分层 / 空白间隔」的锯齿。保留 0.1 s 的额度后，槽位一释放就能当帧补齐。
        const float maxBacklog = aDesc.aRate * kParticleMaxDt;
        if(aEmitAccumulator > maxBacklog) aEmitAccumulator = maxBacklog;
    }
    for(uint32_t i = 0; i < want; ++i) {
        uint32_t slot = 0;
        if(!findFreeSlot(slot)) return;
        spawnParticle(slot);
        // 帧内相位打散：把本帧这一批沿各自速度预推进 (i+0.5)/want 帧位移。
        // 否则同帧粒子都在同一高度，持续发射会形成一条条横向"片子"（近处可见空白间隔）。
        if(aDesc.aSpreadSpawnPhase && want > 1u) {
            sPARTICLE &particle = aParticles[slot];
            const float phase = ((float)i + 0.5f) / (float)want;
            const float advance = dt * phase;
            particle.aPosition = addVec(particle.aPosition, scaleVec(particle.aVelocity, advance));
            particle.aAge += advance;
        }
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
    // BOX：在 aOrigin ± halfExtent 内均匀取点（每轴一次主随机流抽取）；CONE 不消耗主随机流。
    GNg::sVEC3 position = aDesc.aOrigin;
    if(aDesc.aShape == ParticleShape::BOX) {
        position.x += (nextRandom() * 2.0f - 1.0f) * aDesc.aBoxHalfExtent.x;
        position.y += (nextRandom() * 2.0f - 1.0f) * aDesc.aBoxHalfExtent.y;
        position.z += (nextRandom() * 2.0f - 1.0f) * aDesc.aBoxHalfExtent.z;
    }
    particle.aPosition = position;
    const GNg::sVEC3 direction = randomConeDirection();
    const float speed = lerpFloat(aDesc.aSpeedMin, aDesc.aSpeedMax, nextRandom());
    particle.aVelocity = scaleVec(direction, speed);
    particle.aAge = 0.0f;
    particle.aLife = lerpFloat(aDesc.aLifeMin, aDesc.aLifeMax, nextRandom());
    particle.aSize = lerpFloat(aDesc.aSizeMin, aDesc.aSizeMax, nextSizeRandom()); // 独立流
    particle.aColor = aDesc.aColorStart;
    aAlive[slot] = 1;
    ++aActiveCount;
}

void cPARTICLE_SYSTEM::setRecycleBox(const GNg::sVEC3 &center, const GNg::sVEC3 &halfExtent) {
    if(!finiteVec(center) || !finiteVec(halfExtent)) return;
    aRecycleCenter = center;
    aRecycleHalfExtent = halfExtent; // <= 0 的轴在 update 中不回收
}

void cPARTICLE_SYSTEM::setWind(const GNg::sVEC3 &wind, float response) {
    if(!finiteVec(wind)) return;
    if(!(response >= 0.0f) || !std::isfinite(response)) return;
    aDesc.aWind = wind;
    aDesc.aWindResponse = response;
}

float cPARTICLE_SYSTEM::nextRandom() {
    aRngState ^= aRngState << 13;
    aRngState ^= aRngState >> 17;
    aRngState ^= aRngState << 5;
    return (float)(aRngState & 0xFFFFFFu) / 16777216.0f; // [0,1)
}

float cPARTICLE_SYSTEM::nextSizeRandom() {
    aSizeRngState ^= aSizeRngState << 13;
    aSizeRngState ^= aSizeRngState >> 17;
    aSizeRngState ^= aSizeRngState << 5;
    return (float)(aSizeRngState & 0xFFFFFFu) / 16777216.0f; // [0,1)
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
