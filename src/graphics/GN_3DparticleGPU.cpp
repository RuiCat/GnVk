// GN_3DparticleGPU.cpp — P9 GPU 粒子契约的 CPU 侧实现：参数校验 + CPU 回退步进。
//
// 契约：
//   * 与 assets/shaders/gn3d_particle.comp 同一套公式（半隐式 Euler + 阻尼 + 寿命），
//     两份实现的结果差异是 GPU 验收的判据（testParticleGPU / gpuCheck particle.cpuMatch）。
//   * 本文件不触碰 Vulkan：无 GPU / 未初始化时 CPU 回退路径仍可用。
#include "GN_3DparticleGPU.h"

#include <cmath>
#include <cstddef>

namespace GN3d {

namespace {

bool isFinite3(const float v[3]) {
    return std::isfinite(v[0]) && std::isfinite(v[1]) && std::isfinite(v[2]);
}

bool isFinite4(const float v[4]) {
    return std::isfinite(v[0]) && std::isfinite(v[1]) && std::isfinite(v[2]) &&
           std::isfinite(v[3]);
}

} // namespace

bool validateParticleSimDesc(const sPARTICLE_SIM_DESC &desc) {
    if(!isFinite3(desc.aGravity)) return false;
    if(!std::isfinite(desc.aDrag) || desc.aDrag < 0.0f) return false;
    return true;
}

bool validateParticleDrawParams(const sPARTICLE_DRAW_PARAMS &params) {
    if(!isFinite3(params.aCameraRight) || !isFinite3(params.aCameraUp)) return false;
    if(!std::isfinite(params.aSize) || params.aSize <= 0.0f) return false;
    if(!isFinite4(params.aColorStart) || !isFinite4(params.aColorEnd)) return false;
    if(params.aCount == 0) return false;
    for(int i = 0; i < 16; ++i) {
        if(!std::isfinite(params.aViewProj[i])) return false;
    }
    return true;
}

void stepParticlesCpu(sGPU_PARTICLE *particles, uint32_t count,
                      const sPARTICLE_SIM_DESC &desc, float dt) {
    if(!particles || count == 0 || !(dt > 0.0f) || !std::isfinite(dt)) return;
    if(!validateParticleSimDesc(desc)) return;
    const float damping = 1.0f / (1.0f + desc.aDrag * dt);
    for(uint32_t i = 0; i < count; ++i) {
        sGPU_PARTICLE &p = particles[i];
        if(!(p.aAlive >= 0.5f)) continue; // 0 或 NaN 都视为空槽
        for(int axis = 0; axis < 3; ++axis) {
            p.aVel[axis] = (p.aVel[axis] + desc.aGravity[axis] * dt) * damping;
            p.aPos[axis] += p.aVel[axis] * dt;
        }
        p.aAge += dt;
        if(p.aAge >= p.aLife) {
            p.aAlive = 0.0f;
            p.aPad = 0.0f;
        }
    }
}

} // namespace GN3d
