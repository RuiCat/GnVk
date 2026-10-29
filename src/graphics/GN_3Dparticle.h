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
//   * 形状：CONE 沿 `aDirection` 发射；BOX 在 `aOrigin ± aBoxHalfExtent` 内均匀生成位置（方向仍取圆锥）。
//   * 发射方向：以 `aDirection` 为轴的圆锥（半锥角 `aSpread`），速度在 `[aSpeedMin, aSpeedMax]`；
//     `aDirection` 为零向量时退回 +Y。
//   * 尺寸：spawn 时按独立随机流（`aSizeRngState`）在 `[aSizeMin, aSizeMax]` 取值，不消耗主随机流；
//     旧 desc 不设新字段 ⇒ 两值均为 1，`aSize` 恒为 1.0。
//   * 风：update 中 `v += (aWind - v) * min(1, aWindResponse*dt)`（重力之后、位移之前）；
//     `aWindResponse <= 0` 时完全跳过（旧 desc 逐位不变）。
//   * 回收：`setRecycleBox` 后逐轴把位置 wrap 回 `[center-half, center+half)`；half<=0 的轴不回收。
//   * 颜色 = aColorStart → aColorEnd 按 age/life 线性插值。
//   * 本层不做 GPU 实例缓冲/纹理/混合模式（`Upload`/`Draw` 在 P8 与渲染器一起落地）。
#include "GN_3Dmath.h"

#include <cstdint>
#include <vector>

namespace GN3d {

// 发射体形状：CONE = 以 aDirection 为轴的圆锥；BOX = 在 aOrigin ± aBoxHalfExtent 的盒内均匀取点。
enum class ParticleShape { CONE = 0, BOX = 1 };

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

    // 以下为 P11 新增项；不设时保持旧行为（CONE / 尺寸 1.0 / 无风）
    GNg::sVEC3 aBoxHalfExtent = {0.0f, 0.0f, 0.0f}; // BOX：以 aOrigin 为中心的生成半范围
    float aSizeMin = 1.0f; // > 0
    float aSizeMax = 1.0f; // >= aSizeMin
    GNg::sVEC3 aWind = {0.0f, 0.0f, 0.0f}; // 风场速度（有限值）
    float aWindResponse = 0.0f; // >= 0；0 表示无风
    ParticleShape aShape = ParticleShape::CONE;
    // 帧内相位打散：同一帧生成的一批粒子沿各自速度预推进 (i+0.5)/N 帧位移。
    // 不开启时同帧粒子都在同一高度，持续发射会形成横向片状"分层/空白间隔"。
    // 默认 false，保持旧 desc 的行为与既有单测逐位不变。
    bool aSpreadSpawnPhase = false;
};

class cPARTICLE_SYSTEM {
  public:
    cPARTICLE_SYSTEM();
    ~cPARTICLE_SYSTEM();

    // 参数非法返回 false，不修改既有系统；成功则重建池/随机流，并清空回收盒。
    bool create(const sPARTICLE_EMITTER_DESC &desc);
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
    // 逐轴回收盒：位置越界后按 2*halfExtent wrap；halfExtent <= 0 的轴不回收。
    void setRecycleBox(const GNg::sVEC3 &center, const GNg::sVEC3 &halfExtent);
    // 风场：response >= 0；0 表示无风（update 中完全跳过）。
    void setWind(const GNg::sVEC3 &wind, float response);

  private:
    void emitDueParticles(float dt);
    void updateParticles(float dt);
    bool findFreeSlot(uint32_t &outSlot);
    void spawnParticle(uint32_t slot);
    float nextRandom(); // [0,1)
    float nextSizeRandom(); // 尺寸独立随机流 [0,1)
    GNg::sVEC3 randomConeDirection();

    sPARTICLE_EMITTER_DESC aDesc;
    std::vector<sPARTICLE> aParticles;
    std::vector<uint8_t> aAlive;
    std::vector<uint32_t> aFreeSlots; // 空闲槽栈
    uint32_t aActiveCount;
    uint32_t aRngState;
    uint32_t aSizeRngState; // 尺寸随机流，独立于 aRngState
    float aEmitAccumulator;
    GNg::sVEC3 aRecycleCenter;
    GNg::sVEC3 aRecycleHalfExtent;
};

} // namespace GN3d
#endif // _GN_3DPARTICLE_H_
