#pragma once
#ifndef _GN_3DPARTICLE_H_
#define _GN_3DPARTICLE_H_
// ╔══════════════════════════════════════════════════════════╗
// ║  GN_mini — 3D 粒子（CPU 池 + 发射器；实例上传/绘制留 P8） ║
// ╚══════════════════════════════════════════════════════════╝
//
// 契约：
//   * 粒子池固定容量 `aMaxParticles`；每帧按 `rate * dt` 发射（累加器取整，多余留到下一帧）。
//   * 生命周期 `[aLifeMin, aLifeMax]`；超龄槽回收；同参数 + 同 dt 序列 ⇒ 结果可重复（固定 xorshift 种子）。
//   * 运动：v += gravity*dt；v *= 1/(1+drag*dt)；位置 += v*dt（半隐式 Euler）。
//   * 发射方向：以 `aDirection` 为轴的圆锥（半锥角 `aSpread`），速度在 `[aSpeedMin, aSpeedMax]`；
//     `aDirection` 为零向量时退回 +Y。
//   * 颜色 = aColorStart → aColorEnd 按 age/life 线性插值；`aSize` 固定 1.0（渲染尺寸参数 P8 再做）。
//   * 本层不做 GPU 实例缓冲/纹理/混合模式（`Upload`/`Draw` 在 P8 与渲染器一起落地）。
#include "GN_3Dmath.h"

#include <cstdint>
#include <vector>

namespace GN3d {

struct sPARTICLE {
    GNg::sVEC3 aPosition;
    GNg::sVEC3 aVelocity;
    float aAge;
    float aLife;
    float aSize;
    GNg::sVEC4 aColor;
};

struct sPARTICLE_EMITTER_DESC {
    GNg::sVEC3 aOrigin; // 发射位置
    GNg::sVEC3 aDirection; // 主方向（零向量按 +Y）
    float aSpread; // 半锥角（弧度，0..π）
    float aSpeedMin; // >= 0
    float aSpeedMax; // >= aSpeedMin
    float aRate; // 每秒发射数（>= 0）
    uint32_t aMaxParticles; // > 0
    float aLifeMin; // > 0
    float aLifeMax; // >= aLifeMin
    GNg::sVEC3 aGravity; // 默认 (0,-9.81,0)
    float aDrag; // >= 0；速度衰减 1/(1+drag*dt)
    GNg::sVEC4 aColorStart;
    GNg::sVEC4 aColorEnd;
};

class cPARTICLE_SYSTEM {
  public:
    cPARTICLE_SYSTEM();
    ~cPARTICLE_SYSTEM();

    bool create(const sPARTICLE_EMITTER_DESC &desc); // 参数非法返回 false，不修改既有系统
    void reset(); // 清空存活粒子并重置累加器/随机种子（保留参数）
    void update(float dt); // dt <= 0 忽略；dt 钳在 0.1s 内

    bool isCreated() const { return !aParticles.empty(); }
    uint32_t capacity() const { return (uint32_t)aParticles.size(); }
    uint32_t activeCount() const { return aActiveCount; }
    const sPARTICLE *particles() const { return aParticles.empty() ? 0 : &aParticles[0]; }
    bool alive(uint32_t index) const {
        return index < aAlive.size() && aAlive[index] != 0;
    }

    void setOrigin(const GNg::sVEC3 &origin) { aDesc.aOrigin = origin; }
    void setRate(float rate) {
        if(rate >= 0.0f) aDesc.aRate = rate;
    }
    void setGravity(const GNg::sVEC3 &gravity) { aDesc.aGravity = gravity; }

  private:
    void emitDueParticles();
    void updateParticles(float dt);
    bool findFreeSlot(uint32_t &outSlot);
    void spawnParticle(uint32_t slot);
    float nextRandom(); // [0,1)
    GNg::sVEC3 randomConeDirection();

    sPARTICLE_EMITTER_DESC aDesc;
    std::vector<sPARTICLE> aParticles;
    std::vector<uint8_t> aAlive;
    std::vector<uint32_t> aFreeSlots; // 空闲槽栈
    uint32_t aActiveCount;
    uint32_t aRngState;
    float aEmitAccumulator;
};

} // namespace GN3d
#endif // _GN_3DPARTICLE_H_
