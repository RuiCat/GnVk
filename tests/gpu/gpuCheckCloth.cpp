// gpuCheckCloth.cpp — P9-a GPU 布料验收：compute 模拟（SSBO 乒乓）+ CPU 对拍 + 网格绘制。
//
// 覆盖五条：
//   1) cloth.create：8×8 布料的 SSBO/描述符创建成功，回读顶点数正确；
//   2) cloth.compute：20 步后 pinned 四角位不变、平均 y 下降、全有限、无发散；
//   3) cloth.cpuMatch：同一初始数据 1 步后 GPU 与 stepClothCpu 最大差 <= 1e-3；
//   4) cloth.draw：平面布料按 identity VP 绘制，中心像素 ≈ albedo（±0.05）；
//   5) cloth.invalid：dt<=0、count 不符上传、pass 外绘制、重复销毁都安全。
#include <GN.h>
#include "GN_Vk.h"
#include "GN_3DclothGPU.h"

#include "gpuCheckCloth.h"
#include "gpuCheckReport.h"
#include "gpuPixelRead.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

namespace GpuCheck {
namespace {

const uint32_t kWidth = 8;
const uint32_t kHeight = 8;
const float kSpacing = 0.15f;
const float kColorTolerance = 0.05f;
const float kMathTolerance = 1e-3f;

GN3d::sCLOTH_GPU_DESC makeClothDesc() {
    GN3d::sCLOTH_GPU_DESC desc;
    desc.aWidth = kWidth;
    desc.aHeight = kHeight;
    desc.aSpacing = kSpacing;
    desc.aGravity[0] = 0.0f;
    desc.aGravity[1] = -10.0f;
    desc.aGravity[2] = 0.0f;
    desc.aDamping = 0.1f;
    desc.aIterations = 4;
    return desc;
}

// 以原点为中心的规则网格（XY 平面，z = 0）：中心像素落在布料内部，便于像素回读。
std::vector<GN3d::sCLOTH_GPU_VERTEX> makeGrid(bool pinCorners) {
    std::vector<GN3d::sCLOTH_GPU_VERTEX> vertices((size_t)kWidth * kHeight);
    const float halfWidth = (float)(kWidth - 1u) * 0.5f;
    const float halfHeight = (float)(kHeight - 1u) * 0.5f;
    for(uint32_t row = 0; row < kHeight; ++row) {
        for(uint32_t column = 0; column < kWidth; ++column) {
            GN3d::sCLOTH_GPU_VERTEX &vertex = vertices[(size_t)row * kWidth + column];
            std::memset(&vertex, 0, sizeof(vertex));
            vertex.aPos[0] = ((float)column - halfWidth) * kSpacing;
            vertex.aPos[1] = ((float)row - halfHeight) * kSpacing;
            vertex.aPos[2] = 0.0f;
            vertex.aPrev[0] = vertex.aPos[0];
            vertex.aPrev[1] = vertex.aPos[1];
            vertex.aPrev[2] = vertex.aPos[2];
        }
    }
    if(pinCorners) {
        vertices[0].aPinned = 1.0f;
        vertices[kWidth - 1].aPinned = 1.0f;
        vertices[(size_t)(kHeight - 1) * kWidth].aPinned = 1.0f;
        vertices[(size_t)(kHeight - 1) * kWidth + kWidth - 1].aPinned = 1.0f;
    }
    return vertices;
}

bool allFinite(const GN3d::sCLOTH_GPU_VERTEX *vertices, uint32_t count) {
    for(uint32_t i = 0; i < count; ++i) {
        for(int axis = 0; axis < 3; ++axis) {
            if(!std::isfinite(vertices[i].aPos[axis])) return false;
            if(!std::isfinite(vertices[i].aPrev[axis])) return false;
        }
    }
    return true;
}

void reportCreate() {
    const bool created = GNg::GN_VkClothCreate(makeClothDesc());
    uint32_t count = 0;
    const void *data = GNg::GN_VkClothData(&count);
    char detail[160];
    std::snprintf(detail, sizeof(detail), "create=%d vertices=%u/data=%p expected %u", (int)created,
                  count, data, kWidth * kHeight);
    if(created && count == kWidth * kHeight && data) ReportPass("cloth.create", detail);
    else ReportFail("cloth.create", detail);
}

void reportCompute() {
    std::vector<GN3d::sCLOTH_GPU_VERTEX> seed = makeGrid(true);
    const GN3d::sCLOTH_GPU_VERTEX pinned[4] = {seed[0], seed[kWidth - 1],
                                               seed[(size_t)(kHeight - 1) * kWidth],
                                               seed[(size_t)(kHeight - 1) * kWidth + kWidth - 1]};
    float meanBefore = 0.0f;
    for(size_t i = 0; i < seed.size(); ++i) meanBefore += seed[i].aPos[1];
    meanBefore /= (float)seed.size();

    if(!GNg::GN_VkClothUpload(&seed[0], (uint32_t)seed.size())) {
        ReportFail("cloth.compute", "upload failed");
        return;
    }
    for(int step = 0; step < 20; ++step) {
        if(!GNg::GN_VkClothSimulate(1.0f / 60.0f)) {
            ReportFail("cloth.compute", "simulate failed");
            return;
        }
    }
    uint32_t count = 0;
    const GN3d::sCLOTH_GPU_VERTEX *gpu = GNg::GN_VkClothData(&count);
    if(!gpu || count != kWidth * kHeight) {
        ReportFail("cloth.compute", "readback unavailable");
        return;
    }
    const bool samePinned = gpu[0].aPos[1] == pinned[0].aPos[1] &&
                            gpu[kWidth - 1].aPos[1] == pinned[1].aPos[1] &&
                            gpu[(size_t)(kHeight - 1) * kWidth].aPos[1] == pinned[2].aPos[1] &&
                            gpu[(size_t)(kHeight - 1) * kWidth + kWidth - 1].aPos[1] ==
                                pinned[3].aPos[1];
    float meanAfter = 0.0f;
    for(uint32_t i = 0; i < count; ++i) meanAfter += gpu[i].aPos[1];
    meanAfter /= (float)count;
    const bool finite = allFinite(gpu, count);
    const bool sagged = meanAfter < meanBefore - 1e-3f;
    const bool bounded = std::fabs(meanAfter - meanBefore) < 5.0f; // 防发散
    char detail[224];
    std::snprintf(detail, sizeof(detail),
                  "meanY %.4f -> %.4f pinnedStable=%d finite=%d bounded=%d", meanBefore, meanAfter,
                  (int)samePinned, (int)finite, (int)bounded);
    if(samePinned && finite && sagged && bounded) ReportPass("cloth.compute", detail);
    else ReportFail("cloth.compute", detail);
}

void reportCpuMatch() {
    if(!GNg::GN_VkClothCreate(makeClothDesc())) {
        ReportFail("cloth.cpuMatch", "create failed");
        return;
    }
    std::vector<GN3d::sCLOTH_GPU_VERTEX> seed = makeGrid(true);
    seed[20].aPrev[0] = seed[20].aPos[0] + 0.01f; // 给一个非零初速，覆盖 Verlet 惯性项
    std::vector<GN3d::sCLOTH_GPU_VERTEX> cpu = seed;
    if(!GNg::GN_VkClothUpload(&seed[0], (uint32_t)seed.size())) {
        ReportFail("cloth.cpuMatch", "upload failed");
        return;
    }
    const float dt = 1.0f / 60.0f;
    if(!GNg::GN_VkClothSimulate(dt)) {
        ReportFail("cloth.cpuMatch", "simulate failed");
        return;
    }
    GN3d::stepClothCpu(&cpu[0], (uint32_t)cpu.size(), makeClothDesc(), dt);
    uint32_t count = 0;
    const GN3d::sCLOTH_GPU_VERTEX *gpu = GNg::GN_VkClothData(&count);
    if(!gpu || count != cpu.size()) {
        ReportFail("cloth.cpuMatch", "readback unavailable");
        return;
    }
    float maxDiff = 0.0f;
    for(uint32_t i = 0; i < count; ++i) {
        for(int axis = 0; axis < 3; ++axis) {
            maxDiff = std::fmax(maxDiff, std::fabs(gpu[i].aPos[axis] - cpu[i].aPos[axis]));
        }
    }
    char detail[160];
    std::snprintf(detail, sizeof(detail), "max|gpu-cpu|=%.6f tol=%.1e", maxDiff,
                  (double)kMathTolerance);
    if(maxDiff <= kMathTolerance) ReportPass("cloth.cpuMatch", detail);
    else ReportFail("cloth.cpuMatch", detail);
}

void reportDraw() {
    if(!GNg::GN_VkClothCreate(makeClothDesc())) {
        ReportFail("cloth.draw", "create failed");
        return;
    }
    std::vector<GN3d::sCLOTH_GPU_VERTEX> grid = makeGrid(false); // 平面布料：法线 +Z
    if(!GNg::GN_VkClothUpload(&grid[0], (uint32_t)grid.size())) {
        ReportFail("cloth.draw", "upload failed");
        return;
    }
    GNg::sMAT44 identity;
    GNg::Mat44Identity(&identity); // 位置即裁剪空间；z = 0 在近平面
    const GNg::sVEC3 eye = {0.0f, 0.0f, 1.0f};
    if(!GNg::GN_VkBeginFrame()) {
        ReportFail("cloth.draw", "GN_VkBeginFrame failed");
        return;
    }
    GNg::GN_VkBegin3DPass(&identity, &eye, 0, 0);
    const bool drew = GNg::GN_VkClothDraw(identity, 0.7f, 0.3f, 0.2f);
    GNg::GN_VkEnd3DPass();
    GNg::GN_VkEndFrame();
    vkQueueWaitIdle(GNg::GN_Vk.graphicsQueue);
    if(!drew) {
        ReportFail("cloth.draw", "GN_VkClothDraw returned false");
        return;
    }
    std::vector<uint8_t> pixels;
    const int width = (int)GNg::GN_Vk.swapchainExtent.width;
    const int height = (int)GNg::GN_Vk.swapchainExtent.height;
    if(!ReadSwapchain(0, 0, width, height, pixels)) {
        ReportFail("cloth.draw", "swapchain readback failed");
        return;
    }
    const sFLOAT_PIXEL center = SampleSwapchain(pixels, width, width / 2, height / 2);
    char detail[192];
    std::snprintf(detail, sizeof(detail), "RGBA=%.3f,%.3f,%.3f expected 0.700,0.300,0.200",
                  center.r, center.g, center.b);
    if(std::fabs(center.r - 0.7f) <= kColorTolerance &&
       std::fabs(center.g - 0.3f) <= kColorTolerance &&
       std::fabs(center.b - 0.2f) <= kColorTolerance) {
        ReportPass("cloth.draw", detail);
    } else {
        ReportFail("cloth.draw", detail);
    }
}

void reportInvalid() {
    const bool badDt = !GNg::GN_VkClothSimulate(0.0f) && !GNg::GN_VkClothSimulate(-1.0f);
    std::vector<GN3d::sCLOTH_GPU_VERTEX> grid = makeGrid(false);
    const bool badUpload =
        !GNg::GN_VkClothUpload(&grid[0], (uint32_t)grid.size() - 1u) &&
        !GNg::GN_VkClothUpload(0, (uint32_t)grid.size());
    GNg::sMAT44 identity;
    GNg::Mat44Identity(&identity);
    const bool outsidePass = !GNg::GN_VkClothDraw(identity, 1.0f, 1.0f, 1.0f);

    GNg::GN_VkClothDestroy();
    GNg::GN_VkClothDestroy(); // 重复销毁安全
    const bool afterDestroy = !GNg::GN_VkClothSimulate(0.016f) && GNg::GN_VkClothData(0) == 0;

    char detail[192];
    std::snprintf(detail, sizeof(detail), "badDt=%d badUpload=%d outsidePass=%d afterDestroy=%d",
                  (int)badDt, (int)badUpload, (int)outsidePass, (int)afterDestroy);
    if(badDt && badUpload && outsidePass && afterDestroy) ReportPass("cloth.invalid", detail);
    else ReportFail("cloth.invalid", detail);
}

} // namespace

void RunClothChecks() {
    if(!GNg::GN_Vk.initialized) {
        ReportSkip("cloth.setup", "GN_Vk not initialized");
        return;
    }
    reportCreate();
    reportCompute();
    reportCpuMatch();
    reportDraw();
    reportInvalid();
}

} // namespace GpuCheck
