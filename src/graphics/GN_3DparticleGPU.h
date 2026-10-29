#pragma once
#ifndef _GN_3DPARTICLE_GPU_H_
#define _GN_3DPARTICLE_GPU_H_
// ╔══════════════════════════════════════════════════════════╗
// ║  GN_mini — P9 GPU 粒子：SSBO 数据契约 + CPU 回退实现      ║
// ╚══════════════════════════════════════════════════════════╝
//
// 契约：
//   * `sGPU_PARTICLE` 是 CPU / compute / 绘制三方共用的唯一内存布局（std430，64 字节，
//     四个 vec4 对齐），改字段必须同时改 `assets/shaders/gn3d_particle.{comp,vert}`。
//   * `sPARTICLE_SIM_DESC` = 模拟参数（重力 + 阻尼）；`dt` 由每次 step 单独传入。
//   * `stepParticlesCpu` 是同一套公式的 CPU 回退实现（半隐式 Euler + 阻尼 + 寿命），
//     同时用作 GPU 结果的对拍参考：同参数 + 同 dt 下 CPU/GPU 结果须一致（容差 1e-5）。
//   * 寿命到头只把 `aAlive` 清 0（不自动重生）；重生由调用方重新 Upload 决定。
//   * 渲染参数（viewProj / billboard 右上与上向量 / 尺寸 / 颜色渐变）走 `sPARTICLE_DRAW_PARAMS`，
//     与模拟参数分开，便于同一份粒子数据用不同视觉参数绘制。
#include <cstdint>

namespace GN3d {

// SSBO 容量上限：64B/粒子 ⇒ 262144 粒子 = 16MB，超过上限的 create 直接拒绝。
const uint32_t PARTICLE_GPU_MAX_CAPACITY = 262144u;
// 一个 compute workgroup 的粒子数，与 gn3d_particle.comp 的 local_size_x 一致。
const uint32_t PARTICLE_GPU_WORKGROUP_SIZE = 64u;

struct sGPU_PARTICLE {
    float aPos[3]; // 世界位置
    float aAge; // 已存活秒数
    float aVel[3]; // 世界速度
    float aLife; // 总寿命（秒，> 0）
    float aColor[4]; // RGBA（v1 由绘制参数覆盖，保留给后续 per-particle 颜色）
    float aSize; // 世界单位尺寸（v1 由绘制参数覆盖）
    float aSeed; // 随机种子（保留给后续 GPU 重生）
    float aAlive; // 1 = 存活，0 = 空槽（绘制时丢弃）
    float aPad; // 对齐填充，必须保持 0
};

struct sPARTICLE_SIM_DESC {
    float aGravity[3]; // 重力加速度（世界坐标，m/s²）
    float aDrag; // 阻尼系数（>= 0）；速度衰减 1/(1+drag*dt)
};

// 绘制 UBO 的 CPU 镜像（std140：每行 16 字节，与 gn3d_particle.vert 的
// ParticleDrawUBO 逐字段一致；w 分量只为对齐，不当数据用）。
struct sPARTICLE_DRAW_PARAMS {
    float aViewProj[16]; // 与 GN_VkBegin3DPass 相同的 VP（列主序）
    float aCameraRight[4]; // xyz = 摄像机右向量（billboard 展开方向）
    float aCameraUp[4]; // xyz = 摄像机上向量
    float aColorStart[4]; // age=0 的颜色（RGBA）
    float aColorEnd[4]; // age=life 的颜色（RGBA）
    float aSize; // billboard 世界半宽（> 0）
    uint32_t aCount; // 本次绘制的实例数（<= 系统容量）
    float aTime; // 累计时间（保留）
    float aPad; // 保留，必须为 0
};

// 模拟参数合法：重力/阻尼有限且 aDrag >= 0。
bool validateParticleSimDesc(const sPARTICLE_SIM_DESC &desc);
// 绘制参数合法：VP/方向/颜色有限、aSize > 0 且 aCount > 0。
bool validateParticleDrawParams(const sPARTICLE_DRAW_PARAMS &params);
// CPU 回退：对前 count 个粒子推进一步（与 compute 同一公式）。
// dt <= 0 / particles 为空 / count == 0 时不改任何数据；已死槽位保持不变。
void stepParticlesCpu(sGPU_PARTICLE *particles, uint32_t count,
                      const sPARTICLE_SIM_DESC &desc, float dt);

} // namespace GN3d
#endif // _GN_3DPARTICLE_GPU_H_
