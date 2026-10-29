// gpuCheckParticle.cpp — P9 GPU 粒子验收：compute 模拟读写 SSBO + CPU 对拍 + 实例化绘制回读。
//
// 覆盖契约六条：
//   1) particle.create：容量 64 的 SSBO 创建成功、回读接口可用；
//   2) particle.compute：dispatch 一次 dt=0.1、重力 (0,-10,0) 后 v.y=-1、p.y=-0.1；
//   3) particle.cpuMatch：同一初始数据下 GPU 结果与 stepParticlesCpu 逐粒子一致；
//   4) particle.death：age+dt >= life 时 compute 把 aAlive 清 0；
//   5) particle.draw：单个 billboard 覆盖屏幕中心，回读颜色 ≈ 绘制参数（±0.03）；
//   6) particle.perf：65536 粒子 CPU 步进 vs GPU dispatch 的耗时对比（信息性，同时校验一致性）；
//   另加 particle.invalid：dt<=0 / 空指针 / 越界上传 / pass 外绘制 / 重复销毁都安全。
#include <GN.h>
#include "GN_Vk.h"

#include "gpuCheckParticle.h"
#include "gpuCheckReport.h"
#include "gpuPixelRead.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

namespace GpuCheck {
namespace {

using Clock = std::chrono::steady_clock;

const uint32_t kCapacity = 64;
const uint32_t kPerfCapacity = 65536;
const float kColorTolerance = 0.03f;
const float kMathTolerance = 1e-4f;

GN3d::sPARTICLE_SIM_DESC makeSim() {
    GN3d::sPARTICLE_SIM_DESC sim = {{0.0f, -10.0f, 0.0f}, 0.0f};
    return sim;
}

GN3d::sGPU_PARTICLE makeParticle(float life) {
    GN3d::sGPU_PARTICLE p;
    std::memset(&p, 0, sizeof(p));
    p.aPos[0] = 0.0f;
    p.aPos[1] = 0.0f;
    p.aPos[2] = 0.5f;
    p.aLife = life;
    p.aSize = 0.5f;
    p.aAlive = 1.0f;
    return p;
}

std::vector<GN3d::sGPU_PARTICLE> makeParticles(uint32_t count, float life) {
    std::vector<GN3d::sGPU_PARTICLE> particles(count, makeParticle(life));
    return particles;
}

void reportCreate() {
    char detail[160];
    const bool created = GNg::GN_VkParticleCreate(kCapacity, makeSim());
    uint32_t count = 0;
    const void *data = GNg::GN_VkParticleData(&count);
    std::snprintf(detail, sizeof(detail), "create=%d capacity=%u mapped=%p", (int)created, count,
                  data);
    if(created && count == kCapacity && data) ReportPass("particle.create", detail);
    else ReportFail("particle.create", detail);
}

// 只保留 index 0 存活；其余显式标死，确保绘制检查只画一个 billboard。
void reportCompute() {
    std::vector<GN3d::sGPU_PARTICLE> particles = makeParticles(kCapacity, 10.0f);
    for(uint32_t i = 1; i < kCapacity; ++i) particles[i].aAlive = 0.0f;
    if(!GNg::GN_VkParticleUpload(&particles[0], kCapacity, 0)) {
        ReportFail("particle.compute", "upload failed");
        return;
    }
    if(!GNg::GN_VkParticleSimulate(0.1f)) {
        ReportFail("particle.compute", "simulate failed");
        return;
    }
    uint32_t count = 0;
    const GN3d::sGPU_PARTICLE *gpu = GNg::GN_VkParticleData(&count);
    if(!gpu || count != kCapacity) {
        ReportFail("particle.compute", "readback unavailable");
        return;
    }
    const float vy = gpu[0].aVel[1];
    const float py = gpu[0].aPos[1];
    char detail[192];
    std::snprintf(detail, sizeof(detail), "v.y=%.6f (expected -1.000000) p.y=%.6f (expected -0.100000)",
                  vy, py);
    if(std::fabs(vy + 1.0f) <= kMathTolerance && std::fabs(py + 0.1f) <= kMathTolerance) {
        ReportPass("particle.compute", detail);
    } else {
        ReportFail("particle.compute", detail);
    }
}

// 同一份初始数据的 CPU 步进 vs GPU 结果逐粒子比对（位置/速度/年龄/存活）。
void reportCpuMatch() {
    std::vector<GN3d::sGPU_PARTICLE> seed = makeParticles(kCapacity, 10.0f);
    for(uint32_t i = 0; i < kCapacity; ++i) {
        seed[i].aVel[0] = (float)(i % 5) * 0.3f;
        seed[i].aVel[1] = (float)(i % 3) * 0.5f;
        seed[i].aPos[2] = 0.5f + 0.01f * (float)i;
    }
    std::vector<GN3d::sGPU_PARTICLE> cpu = seed;
    if(!GNg::GN_VkParticleUpload(&seed[0], kCapacity, 0)) {
        ReportFail("particle.cpuMatch", "upload failed");
        return;
    }
    if(!GNg::GN_VkParticleSimulate(1.0f / 60.0f)) {
        ReportFail("particle.cpuMatch", "simulate failed");
        return;
    }
    GN3d::stepParticlesCpu(&cpu[0], kCapacity, makeSim(), 1.0f / 60.0f);

    uint32_t count = 0;
    const GN3d::sGPU_PARTICLE *gpu = GNg::GN_VkParticleData(&count);
    if(!gpu || count != kCapacity) {
        ReportFail("particle.cpuMatch", "readback unavailable");
        return;
    }
    float maxDiff = 0.0f;
    for(uint32_t i = 0; i < kCapacity; ++i) {
        for(int axis = 0; axis < 3; ++axis) {
            maxDiff = std::fmax(maxDiff, std::fabs(gpu[i].aPos[axis] - cpu[i].aPos[axis]));
            maxDiff = std::fmax(maxDiff, std::fabs(gpu[i].aVel[axis] - cpu[i].aVel[axis]));
        }
        maxDiff = std::fmax(maxDiff, std::fabs(gpu[i].aAge - cpu[i].aAge));
        maxDiff = std::fmax(maxDiff, std::fabs(gpu[i].aAlive - cpu[i].aAlive));
    }
    char detail[160];
    std::snprintf(detail, sizeof(detail), "particles=%u max|gpu-cpu|=%.7f tol=%.1e", kCapacity,
                  maxDiff, (double)kMathTolerance);
    if(maxDiff <= kMathTolerance) ReportPass("particle.cpuMatch", detail);
    else ReportFail("particle.cpuMatch", detail);
}

void reportDeath() {
    std::vector<GN3d::sGPU_PARTICLE> particles = makeParticles(kCapacity, 0.05f);
    if(!GNg::GN_VkParticleUpload(&particles[0], kCapacity, 0)) {
        ReportFail("particle.death", "upload failed");
        return;
    }
    if(!GNg::GN_VkParticleSimulate(0.1f)) {
        ReportFail("particle.death", "simulate failed");
        return;
    }
    uint32_t count = 0;
    const GN3d::sGPU_PARTICLE *gpu = GNg::GN_VkParticleData(&count);
    char detail[160];
    const bool aliveCleared = gpu && count == kCapacity && gpu[0].aAlive == 0.0f;
    std::snprintf(detail, sizeof(detail), "life=0.05 dt=0.1 alive=%.0f age=%.3f (expected 0)",
                  gpu ? gpu[0].aAlive : -1.0f, gpu ? gpu[0].aAge : -1.0f);
    if(aliveCleared) ReportPass("particle.death", detail);
    else ReportFail("particle.death", detail);
}

bool readCenterPixel(sFLOAT_PIXEL &out) {
    std::vector<uint8_t> pixels;
    const uint32_t width = GNg::GN_Vk.swapchainExtent.width;
    const uint32_t height = GNg::GN_Vk.swapchainExtent.height;
    if(!ReadSwapchain(0, 0, (int)width, (int)height, pixels)) return false;
    out = SampleSwapchain(pixels, (int)width, (int)(width / 2), (int)(height / 2));
    return true;
}

void reportDraw() {
    std::vector<GN3d::sGPU_PARTICLE> particles = makeParticles(kCapacity, 10.0f);
    for(uint32_t i = 1; i < kCapacity; ++i) particles[i].aAlive = 0.0f;
    if(!GNg::GN_VkParticleUpload(&particles[0], kCapacity, 0)) {
        ReportFail("particle.draw", "upload failed");
        return;
    }

    GN3d::sPARTICLE_DRAW_PARAMS params;
    std::memset(&params, 0, sizeof(params));
    for(int i = 0; i < 4; ++i) params.aViewProj[i * 5] = 1.0f; // 单位矩阵：位置即裁剪空间
    params.aCameraRight[0] = 1.0f;
    params.aCameraUp[1] = 1.0f;
    params.aSize = 0.5f;
    params.aColorStart[0] = 0.2f;
    params.aColorStart[1] = 0.8f;
    params.aColorStart[2] = 0.3f;
    params.aColorStart[3] = 1.0f;
    params.aColorEnd[0] = 0.2f;
    params.aColorEnd[1] = 0.8f;
    params.aColorEnd[2] = 0.3f;
    params.aColorEnd[3] = 1.0f;
    params.aCount = 1;

    GNg::sMAT44 identity;
    GNg::Mat44Identity(&identity);
    GNg::sVEC3 eye = {0.0f, 0.0f, 1.0f};
    if(!GNg::GN_VkBeginFrame()) {
        ReportFail("particle.draw", "GN_VkBeginFrame failed");
        return;
    }
    GNg::GN_VkBegin3DPass(&identity, &eye, 0, 0);
    const bool drew = GNg::GN_VkParticleDraw(params);
    GNg::GN_VkEnd3DPass();
    GNg::GN_VkEndFrame();
    vkQueueWaitIdle(GNg::GN_Vk.graphicsQueue);
    if(!drew) {
        ReportFail("particle.draw", "GN_VkParticleDraw returned false");
        return;
    }
    sFLOAT_PIXEL pixel = {0.0f, 0.0f, 0.0f, 0.0f};
    if(!readCenterPixel(pixel)) {
        ReportFail("particle.draw", "swapchain readback failed");
        return;
    }
    char detail[192];
    std::snprintf(detail, sizeof(detail), "RGBA=%.3f,%.3f,%.3f,%.3f expected 0.200,0.800,0.300,1.000",
                  pixel.r, pixel.g, pixel.b, pixel.a);
    if(std::fabs(pixel.r - 0.2f) <= kColorTolerance &&
       std::fabs(pixel.g - 0.8f) <= kColorTolerance &&
       std::fabs(pixel.b - 0.3f) <= kColorTolerance) {
        ReportPass("particle.draw", detail);
    } else {
        ReportFail("particle.draw", detail);
    }
}

// CPU 步进 vs GPU dispatch 的墙钟对比；同时保持 CPU/GPU 结果一致（信息性性能数字）。
void reportPerf() {
    if(!GNg::GN_VkParticleCreate(kPerfCapacity, makeSim())) {
        ReportFail("particle.perf", "create 65536 failed");
        return;
    }
    std::vector<GN3d::sGPU_PARTICLE> seed = makeParticles(kPerfCapacity, 100.0f);
    for(uint32_t i = 0; i < kPerfCapacity; ++i) {
        seed[i].aPos[0] = (float)(i % 128) * 0.01f;
        seed[i].aVel[1] = 1.0f;
    }
    if(!GNg::GN_VkParticleUpload(&seed[0], kPerfCapacity, 0)) {
        ReportFail("particle.perf", "upload 65536 failed");
        return;
    }
    std::vector<GN3d::sGPU_PARTICLE> cpu = seed;
    const float dt = 1.0f / 60.0f;
    const int steps = 4;

    vkQueueWaitIdle(GNg::GN_Vk.graphicsQueue); // 保证上一帧已结束，避免读写竞争
    const Clock::time_point gpuStart = Clock::now();
    bool gpuOk = true;
    for(int i = 0; i < steps; ++i) gpuOk = gpuOk && GNg::GN_VkParticleSimulate(dt);
    const Clock::time_point gpuEnd = Clock::now();

    const Clock::time_point cpuStart = Clock::now();
    for(int i = 0; i < steps; ++i) GN3d::stepParticlesCpu(&cpu[0], kPerfCapacity, makeSim(), dt);
    const Clock::time_point cpuEnd = Clock::now();

    uint32_t count = 0;
    const GN3d::sGPU_PARTICLE *gpu = GNg::GN_VkParticleData(&count);
    char detail[224];
    if(!gpuOk || !gpu || count != kPerfCapacity) {
        std::snprintf(detail, sizeof(detail), "gpuOk=%d count=%u", (int)gpuOk, count);
        ReportFail("particle.perf", detail);
        return;
    }
    const double gpuMs = std::chrono::duration<double, std::milli>(gpuEnd - gpuStart).count();
    const double cpuMs = std::chrono::duration<double, std::milli>(cpuEnd - cpuStart).count();
    const float diff = std::fabs(gpu[kPerfCapacity - 1].aPos[1] - cpu[kPerfCapacity - 1].aPos[1]);
    std::snprintf(detail, sizeof(detail),
                  "65536 x %d steps: gpu=%.2fms (%.2f ms/step) cpu=%.2fms (%.2f ms/step) "
                  "lastParticle.dy=%.6f",
                  steps, gpuMs, gpuMs / steps, cpuMs, cpuMs / steps, diff);
    if(diff <= kMathTolerance) ReportPass("particle.perf", detail);
    else ReportFail("particle.perf", detail);
}

void reportInvalid() {
    const bool badDt = !GNg::GN_VkParticleSimulate(0.0f) && !GNg::GN_VkParticleSimulate(-1.0f);
    const GN3d::sGPU_PARTICLE one = makeParticle(1.0f);
    const bool badUpload = !GNg::GN_VkParticleUpload(0, 1, 0) &&
                           !GNg::GN_VkParticleUpload(&one, 1, kCapacity); // 越界
    GN3d::sPARTICLE_DRAW_PARAMS params;
    std::memset(&params, 0, sizeof(params));
    for(int i = 0; i < 4; ++i) params.aViewProj[i * 5] = 1.0f;
    params.aCameraRight[0] = 1.0f;
    params.aCameraUp[1] = 1.0f;
    params.aSize = 0.5f;
    params.aColorStart[3] = 1.0f;
    params.aColorEnd[3] = 1.0f;
    params.aCount = 1;
    const bool outsidePass = !GNg::GN_VkParticleDraw(params); // 3D pass 外必须拒绝
    GN3d::sPARTICLE_DRAW_PARAMS zeroSize = params;
    zeroSize.aSize = 0.0f;
    const bool zeroSizeRejected = !GNg::GN_VkParticleDraw(zeroSize);

    GNg::GN_VkParticleDestroy();
    GNg::GN_VkParticleDestroy(); // 重复销毁安全
    const bool afterDestroy = !GNg::GN_VkParticleSimulate(0.016f) && GNg::GN_VkParticleData(0) == 0;

    char detail[192];
    std::snprintf(detail, sizeof(detail),
                  "badDt=%d badUpload=%d outsidePass=%d zeroSize=%d afterDestroy=%d", (int)badDt,
                  (int)badUpload, (int)outsidePass, (int)zeroSizeRejected, (int)afterDestroy);
    if(badDt && badUpload && outsidePass && zeroSizeRejected && afterDestroy) {
        ReportPass("particle.invalid", detail);
    } else {
        ReportFail("particle.invalid", detail);
    }
}

} // namespace

void RunParticleChecks() {
    if(!GNg::GN_Vk.initialized) {
        ReportSkip("particle.setup", "GN_Vk not initialized");
        return;
    }
    reportCreate();
    reportCompute();
    reportCpuMatch();
    reportDeath();
    reportDraw();
    reportInvalid(); // 结束时销毁系统
    reportPerf(); // 重新创建 65536 系统做耗时对比
    GNg::GN_VkParticleDestroy();
}

} // namespace GpuCheck
