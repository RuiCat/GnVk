// tests/cornellBox/cornellGpuRtProbe.cpp — GPU compute 光追预览的自检（无窗口）
//
// 用法：make cornellGpuRtProbe && ./build/cornellGpuRtProbe
// 自建一个无窗口 Vulkan 设备（不依赖引擎/GLFW），跑 compute 着色器，并与
// renderCpuRt 的同 seed 输出（320×200×8 spp）逐像素对比：均值、MAE、最大差、
// nonBlack%；然后测 640×400 下 8/16/32 spp 的帧耗时，给预览默认采样数提供依据。
// 无 Vulkan 设备时打印 SKIP 并返回 0（不作为 CI 失败）。
#include "cornellCpuRt.h"
#include "cornellGeometry.h"
#include "cornellGpuRt.h"
#include "cornellPng.h"

#include <vulkan/vulkan.h>

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
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

// 最小实例：无窗口、无 surface、无扩展。
VkInstance createInstance() {
    VkApplicationInfo app = {VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app.pApplicationName = "cornellGpuRtProbe";
    app.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app.pEngineName = "none";
    app.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app.apiVersion = VK_API_VERSION_1_0;
    VkInstanceCreateInfo ici = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    ici.pApplicationInfo = &app;
    VkInstance instance = VK_NULL_HANDLE;
    if (vkCreateInstance(&ici, nullptr, &instance) != VK_SUCCESS) return VK_NULL_HANDLE;
    return instance;
}

bool pickDevice(VkInstance instance, VkPhysicalDevice *physical, uint32_t *family) {
    uint32_t count = 0;
    if (vkEnumeratePhysicalDevices(instance, &count, nullptr) != VK_SUCCESS || count == 0) {
        return false;
    }
    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(instance, &count, devices.data());
    for (uint32_t d = 0; d < count; ++d) {
        uint32_t queueCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(devices[d], &queueCount, nullptr);
        std::vector<VkQueueFamilyProperties> queues(queueCount);
        vkGetPhysicalDeviceQueueFamilyProperties(devices[d], &queueCount, queues.data());
        for (uint32_t q = 0; q < queueCount; ++q) {
            if (queues[q].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                *physical = devices[d];
                *family = q;
                return true;
            }
        }
    }
    return false;
}

bool createDevice(VkPhysicalDevice physical, uint32_t family, VkDevice *device, VkQueue *queue) {
    const float priority = 1.0f;
    VkDeviceQueueCreateInfo qci = {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    qci.queueFamilyIndex = family;
    qci.queueCount = 1;
    qci.pQueuePriorities = &priority;
    VkDeviceCreateInfo dci = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    dci.queueCreateInfoCount = 1;
    dci.pQueueCreateInfos = &qci;
    if (vkCreateDevice(physical, &dci, nullptr, device) != VK_SUCCESS) return false;
    vkGetDeviceQueue(*device, family, 0, queue);
    return true;
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

void imageStats(const std::vector<unsigned char> &rgba, int width, int height, double mean[3],
                double *nonBlackPct) {
    const int pixels = width * height;
    double sum[3] = {0.0, 0.0, 0.0};
    int nonBlack = 0;
    for (int i = 0; i < pixels; ++i) {
        const unsigned char *p = &rgba[static_cast<size_t>(i) * 4u];
        for (int c = 0; c < 3; ++c) sum[c] += p[c];
        if (p[0] + p[1] + p[2] > 30) ++nonBlack;
    }
    for (int c = 0; c < 3; ++c) mean[c] = sum[c] / pixels;
    *nonBlackPct = 100.0 * nonBlack / pixels;
}

double noiseMetric(const std::vector<unsigned char> &rgba, int width, int x0, int y0, int x1,
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
    VkInstance instance = createInstance();
    if (instance == VK_NULL_HANDLE) {
        std::fprintf(stderr, "[cornellGpuRtProbe] 无法创建 Vulkan 实例（驱动缺失？）\n");
        return 0;
    }
    VkPhysicalDevice physical = VK_NULL_HANDLE;
    uint32_t family = 0;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue queue = VK_NULL_HANDLE;
    if (!pickDevice(instance, &physical, &family) ||
        !createDevice(physical, family, &device, &queue)) {
        std::printf("[cornellGpuRtProbe] SKIP: 无可用图形设备\n");
        vkDestroyInstance(instance, nullptr);
        return 0;
    }
    std::printf("[cornellGpuRtProbe] device ok, queueFamily=%u\n", family);

    const Cornell::sPose &pose = Cornell::CORNELL_POSES[2];
    float eye[3];
    float fwd[3];
    float right[3];
    float up[3];
    cameraFromPose(pose, eye, fwd, right, up);

    Cornell::sGpuRtContext ctx = {device, physical, queue, family};
    if (!Cornell::initGpuRt(ctx, kWidth, kHeight)) {
        std::fprintf(stderr, "[cornellGpuRtProbe] initGpuRt 失败\n");
        vkDestroyDevice(device, nullptr);
        vkDestroyInstance(instance, nullptr);
        return 1;
    }

    std::vector<unsigned char> cpuPixels;
    std::vector<unsigned char> gpuPixels;
    if (!Cornell::renderCpuRt(kWidth, kHeight, eye, fwd, right, up, 45.0f, 0, &cpuPixels)) {
        std::fprintf(stderr, "[cornellGpuRtProbe] renderCpuRt 失败\n");
        return 1;
    }
    if (!Cornell::renderGpuRt(kWidth, kHeight, eye, fwd, right, up, 45.0f, 0, kSamples,
                              &gpuPixels)) {
        std::fprintf(stderr, "[cornellGpuRtProbe] renderGpuRt 失败\n");
        return 1;
    }

    double cpuMean[3];
    double gpuMean[3];
    double cpuNonBlack = 0.0;
    double gpuNonBlack = 0.0;
    imageStats(cpuPixels, kWidth, kHeight, cpuMean, &cpuNonBlack);
    imageStats(gpuPixels, kWidth, kHeight, gpuMean, &gpuNonBlack);

    const int pixels = kWidth * kHeight;
    double sumAbs[3] = {0.0, 0.0, 0.0};
    int maxDiff = 0;
    int over8 = 0;
    for (int i = 0; i < pixels; ++i) {
        const unsigned char *a = &cpuPixels[static_cast<size_t>(i) * 4u];
        const unsigned char *b = &gpuPixels[static_cast<size_t>(i) * 4u];
        for (int c = 0; c < 3; ++c) {
            const int diff = std::abs(static_cast<int>(a[c]) - static_cast<int>(b[c]));
            sumAbs[c] += diff;
            if (diff > maxDiff) maxDiff = diff;
        }
        if (std::abs(a[0] - b[0]) > 8 || std::abs(a[1] - b[1]) > 8 ||
            std::abs(a[2] - b[2]) > 8) {
            ++over8;
        }
    }
    const double mae =
        (sumAbs[0] + sumAbs[1] + sumAbs[2]) / (3.0 * static_cast<double>(pixels));
    std::printf("[cornellGpuRtProbe] CPU mean=(%.1f,%.1f,%.1f) nonBlack=%.1f%%\n", cpuMean[0],
                cpuMean[1], cpuMean[2], cpuNonBlack);
    std::printf("[cornellGpuRtProbe] GPU mean=(%.1f,%.1f,%.1f) nonBlack=%.1f%%\n", gpuMean[0],
                gpuMean[1], gpuMean[2], gpuNonBlack);
    std::printf("[cornellGpuRtProbe] MAE=%.3f maxDiff=%d pixelsOver8=%.2f%%\n", mae, maxDiff,
                100.0 * over8 / pixels);

    Cornell::writeRgbPng(".local/cornellbox/gpu-rt-probe.png", gpuPixels, kWidth, kHeight);
    Cornell::writeRgbPng(".local/cornellbox/cpu-rt-ref.png", cpuPixels, kWidth, kHeight);

    // 累积收敛验证：同一相机连续 32 帧 × 16 spp，平坦墙面噪声应至少降到 1/3。
    Cornell::shutdownGpuRt();
    double accumulatedRatio = 0.0;
    if (Cornell::initGpuRt(ctx, kWidth, kHeight)) {
        std::vector<unsigned char> frame;
        Cornell::renderGpuRt(kWidth, kHeight, eye, fwd, right, up, 45.0f, 0, 16, &frame);
        const double noise1 = noiseMetric(frame, kWidth, 20, 40, 140, 160);
        for (int i = 0; i < 31; ++i) {
            Cornell::renderGpuRt(kWidth, kHeight, eye, fwd, right, up, 45.0f, 0, 16, &frame);
        }
        const double noise32 = noiseMetric(frame, kWidth, 20, 40, 140, 160);
        accumulatedRatio = noise1 > 1.0e-6 ? noise1 / noise32 : 0.0;
        std::printf("[cornellGpuRtProbe] 累积收敛: frame1=%.2f frame32=%.2f ratio=%.2fx\n", noise1,
                    noise32, accumulatedRatio);
        Cornell::writeRgbPng(".local/cornellbox/gpu-rt-accum32.png", frame, kWidth, kHeight);
    }

    // 640×400 耗时：8/16/32 spp（每档先预热 1 帧，再取 5 帧平均）。
    double timings[4] = {-1.0, -1.0, -1.0, -1.0};
    const int sampleCounts[4] = {8, 32, 64, 128};
    for (int s = 0; s < 4; ++s) {
        Cornell::shutdownGpuRt();
        if (!Cornell::initGpuRt(ctx, 640, 400)) continue;
        std::vector<unsigned char> pixels;
        if (!Cornell::renderGpuRt(640, 400, eye, fwd, right, up, 45.0f, 0, sampleCounts[s],
                                  &pixels)) {
            continue;
        }
        const auto t0 = std::chrono::steady_clock::now();
        const int frames = 5;
        for (int f = 0; f < frames; ++f) {
            Cornell::renderGpuRt(640, 400, eye, fwd, right, up, 45.0f, 0, sampleCounts[s],
                                 &pixels);
        }
        const auto t1 = std::chrono::steady_clock::now();
        timings[s] = std::chrono::duration<double, std::milli>(t1 - t0).count() / frames;
        std::printf("[cornellGpuRtProbe] 640x400 %2d spp: %.2f ms/frame\n", sampleCounts[s],
                    timings[s]);
    }
    Cornell::shutdownGpuRt();
    vkDestroyDevice(device, nullptr);
    vkDestroyInstance(instance, nullptr);

    const bool pass = mae <= 2.0 && accumulatedRatio >= 3.0 &&
                      std::fabs(gpuMean[0] - cpuMean[0]) <= 2.0 &&
                      std::fabs(gpuMean[1] - cpuMean[1]) <= 2.0 &&
                      std::fabs(gpuMean[2] - cpuMean[2]) <= 2.0 && gpuNonBlack >= 50.0;
    std::printf("[cornellGpuRtProbe] %s\n", pass ? "PASS" : "FAIL");
    return pass ? 0 : 1;
}
