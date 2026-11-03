// tests/cornellBox/cornellHwRtProbe.cpp — 硬件 ray-query 光追预览的自检（无窗口）
//
// 用法：make cornellHwRtProbe && ./build/cornellHwRtProbe
// 自建 VK_KHR_ray_query 设备，跑 ray-query 路径追踪，与 renderCpuRt 同 seed 输出
// （320×200×8 spp）对比；再做 32 帧累积收敛与 640×400 多 spp 耗时；无设备时 SKIP。
#include "cornellCpuRt.h"
#include "cornellGeometry.h"
#include "cornellGpuRt.h"
#include "cornellPng.h"
#include "cornellRtDevice.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <vector>

namespace {

const int kWidth = 320;
const int kHeight = 200;
const int kSamples = 8;

void crossf(const float a[3], const float b[3], float out[3]) {
    out[0] = a[1] * b[2] - a[2] * b[1];
    out[1] = a[2] * b[0] - a[0] * b[2];
    out[2] = a[0] * b[1] - a[1] * b[0];
}

void normalizef(float v[3]) {
    const float len = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if (len > 1.0e-8f) {
        v[0] /= len;
        v[1] /= len;
        v[2] /= len;
    }
}

void cameraFromPose(const Cornell::sPose &pose, float eye[3], float fwd[3], float right[3],
                    float up[3]) {
    eye[0] = pose.aPosition.x;
    eye[1] = pose.aPosition.y;
    eye[2] = pose.aPosition.z;
    fwd[0] = pose.aFocus.x - eye[0];
    fwd[1] = pose.aFocus.y - eye[1];
    fwd[2] = pose.aFocus.z - eye[2];
    normalizef(fwd);
    const float worldUp[3] = {0.0f, 1.0f, 0.0f};
    crossf(fwd, worldUp, right);
    normalizef(right);
    crossf(right, fwd, up);
}

double imageNoise(const std::vector<unsigned char> &rgba, int width, int x0, int y0, int x1,
                  int y1) {
    double sum = 0.0;
    int count = 0;
    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            double local = 0.0;
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    const size_t i = (static_cast<size_t>(y + dy) * width + (x + dx)) * 4u;
                    local += rgba[i] * 0.2126 + rgba[i + 1] * 0.7152 + rgba[i + 2] * 0.0722;
                }
            }
            const size_t c = (static_cast<size_t>(y) * width + x) * 4u;
            const double center = rgba[c] * 0.2126 + rgba[c + 1] * 0.7152 + rgba[c + 2] * 0.0722;
            sum += std::fabs(center - local / 9.0);
            ++count;
        }
    }
    return count > 0 ? sum / count : 0.0;
}

} // namespace

int main() {
    Cornell::sRayQueryDevice device;
    if (!Cornell::createRayQueryDevice(&device)) {
        std::printf("[cornellHwRtProbe] SKIP: 无 ray-query 设备\n");
        return 0;
    }
    const Cornell::sGpuRtContext ctx = {device.aDevice, device.aPhysical, device.aQueue,
                                        device.aQueueFamily};
    if (!Cornell::initGpuRt(ctx, kWidth, kHeight, Cornell::eGPU_RT_RAY_QUERY)) {
        std::fprintf(stderr, "[cornellHwRtProbe] ray-query 初始化失败\n");
        Cornell::destroyRayQueryDevice(&device);
        return 1;
    }

    const Cornell::sPose &pose = Cornell::CORNELL_POSES[2];
    float eye[3];
    float fwd[3];
    float right[3];
    float up[3];
    cameraFromPose(pose, eye, fwd, right, up);

    std::vector<unsigned char> cpuPixels;
    std::vector<unsigned char> gpuPixels;
    if (!Cornell::renderCpuRt(kWidth, kHeight, eye, fwd, right, up, 45.0f, 0, &cpuPixels) ||
        !Cornell::renderGpuRt(kWidth, kHeight, eye, fwd, right, up, 45.0f, 0, kSamples,
                              &gpuPixels)) {
        std::fprintf(stderr, "[cornellHwRtProbe] 渲染失败\n");
        return 1;
    }

    double cpuMean[3] = {0.0, 0.0, 0.0};
    double gpuMean[3] = {0.0, 0.0, 0.0};
    const int pixels = kWidth * kHeight;
    double sumAbs[3] = {0.0, 0.0, 0.0};
    int maxDiff = 0;
    for (int i = 0; i < pixels; ++i) {
        const unsigned char *a = &cpuPixels[static_cast<size_t>(i) * 4u];
        const unsigned char *b = &gpuPixels[static_cast<size_t>(i) * 4u];
        for (int c = 0; c < 3; ++c) {
            cpuMean[c] += a[c];
            gpuMean[c] += b[c];
            const int diff = std::abs(static_cast<int>(a[c]) - static_cast<int>(b[c]));
            sumAbs[c] += diff;
            if (diff > maxDiff) maxDiff = diff;
        }
    }
    double mae = 0.0;
    for (int c = 0; c < 3; ++c) {
        cpuMean[c] /= pixels;
        gpuMean[c] /= pixels;
        mae += sumAbs[c] / pixels;
    }
    mae /= 3.0;
    std::printf("[cornellHwRtProbe] CPU mean=(%.1f,%.1f,%.1f)\n", cpuMean[0], cpuMean[1],
                cpuMean[2]);
    std::printf("[cornellHwRtProbe] RQ  mean=(%.1f,%.1f,%.1f) MAE=%.3f maxDiff=%d\n", gpuMean[0],
                gpuMean[1], gpuMean[2], mae, maxDiff);
    Cornell::writeRgbPng(".local/cornellbox/hw-rt-probe.png", gpuPixels, kWidth, kHeight);
    Cornell::writeRgbPng(".local/cornellbox/hw-rt-cpu-ref.png", cpuPixels, kWidth, kHeight);

    // 累积收敛（同一相机 32 帧 × 16 spp）。
    Cornell::shutdownGpuRt();
    double ratio = 0.0;
    if (Cornell::initGpuRt(ctx, kWidth, kHeight, Cornell::eGPU_RT_RAY_QUERY)) {
        std::vector<unsigned char> frame;
        Cornell::renderGpuRt(kWidth, kHeight, eye, fwd, right, up, 45.0f, 0, 16, &frame);
        const double noise1 = imageNoise(frame, kWidth, 20, 40, 140, 160);
        for (int i = 0; i < 31; ++i) {
            Cornell::renderGpuRt(kWidth, kHeight, eye, fwd, right, up, 45.0f, 0, 16, &frame);
        }
        const double noise32 = imageNoise(frame, kWidth, 20, 40, 140, 160);
        ratio = noise1 > 1.0e-6 ? noise1 / noise32 : 0.0;
        std::printf("[cornellHwRtProbe] 累积收敛: frame1=%.2f frame32=%.2f ratio=%.2fx\n", noise1,
                    noise32, ratio);
        Cornell::writeRgbPng(".local/cornellbox/hw-rt-accum32.png", frame, kWidth, kHeight);
    }

    // 640×400 耗时。
    const int sampleCounts[3] = {8, 32, 64};
    for (int s = 0; s < 3; ++s) {
        Cornell::shutdownGpuRt();
        if (!Cornell::initGpuRt(ctx, 640, 400, Cornell::eGPU_RT_RAY_QUERY)) break;
        std::vector<unsigned char> frame;
        Cornell::renderGpuRt(640, 400, eye, fwd, right, up, 45.0f, 0, sampleCounts[s], &frame);
        const auto t0 = std::chrono::steady_clock::now();
        for (int f = 0; f < 5; ++f) {
            Cornell::renderGpuRt(640, 400, eye, fwd, right, up, 45.0f, 0, sampleCounts[s],
                                 &frame);
        }
        const auto t1 = std::chrono::steady_clock::now();
        std::printf("[cornellHwRtProbe] 640x400 %2d spp: %.2f ms/frame\n", sampleCounts[s],
                    std::chrono::duration<double, std::milli>(t1 - t0).count() / 5.0);
    }
    Cornell::shutdownGpuRt();
    Cornell::destroyRayQueryDevice(&device);

    const bool pass = mae <= 4.0 && ratio >= 3.0 && gpuMean[0] > 1.0 && gpuMean[1] > 1.0 &&
                      gpuMean[2] > 1.0;
    std::printf("[cornellHwRtProbe] %s\n", pass ? "PASS" : "FAIL");
    return pass ? 0 : 1;
}
