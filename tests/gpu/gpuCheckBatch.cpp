// gpuCheckBatch.cpp — P9-c 批处理验收：稳定排序/状态切换统计 + 排序前后绘制结果一致。
//
// 覆盖三条：
//   1) batch.sort：乱序 4 项排序后键单调不减、aOrder 集合不变；
//   2) batch.switches：乱序 3 次切换 → 排序后 1 次；
//   3) batch.draw：同一场景按乱序与排序各画一帧，两个采样点颜色一致（±0.02）。
#include <GN.h>
#include "GN_Vk.h"
#include "GN_3Dlight.h"
#include "GN_3Dbatch.h"

#include "gpuCheckBatch.h"
#include "gpuCheckReport.h"
#include "gpuPixelRead.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

namespace GpuCheck {
namespace {

const float kViewHalfX = 4.0f;
const float kViewHalfY = 3.0f;
const float kColorTolerance = 0.02f;

struct sCUBE {
    float aVerts[24 * 11];
    uint32_t aIndices[36];
    float aColor[3];
};

// 单位立方体（边长 1，中心在 (worldX, 0, 0.7)），11 float 顶点 = VkVertex3D。
void buildCube(sCUBE &cube, float worldX, float r, float g, float b) {
    static const float kFaceCorners[6][12] = {
        {0.5f, -0.5f, -0.5f, 0.5f, 0.5f, -0.5f, 0.5f, 0.5f, 0.5f, 0.5f, -0.5f, 0.5f},
        {-0.5f, -0.5f, 0.5f, -0.5f, 0.5f, 0.5f, -0.5f, 0.5f, -0.5f, -0.5f, -0.5f, -0.5f},
        {-0.5f, 0.5f, -0.5f, -0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, -0.5f},
        {-0.5f, -0.5f, 0.5f, -0.5f, -0.5f, -0.5f, 0.5f, -0.5f, -0.5f, 0.5f, -0.5f, 0.5f},
        {-0.5f, -0.5f, 0.5f, 0.5f, -0.5f, 0.5f, 0.5f, 0.5f, 0.5f, -0.5f, 0.5f, 0.5f},
        {0.5f, -0.5f, -0.5f, -0.5f, -0.5f, -0.5f, -0.5f, 0.5f, -0.5f, 0.5f, 0.5f, -0.5f}};
    static const float kFaceNormals[6][3] = {{1, 0, 0},  {-1, 0, 0}, {0, 1, 0},
                                             {0, -1, 0}, {0, 0, 1},  {0, 0, -1}};
    std::memset(&cube, 0, sizeof(cube));
    for(uint32_t face = 0; face < 6; ++face) {
        for(uint32_t corner = 0; corner < 4; ++corner) {
            float *vertex = cube.aVerts + (face * 4u + corner) * 11u;
            vertex[0] = kFaceCorners[face][corner * 3u] + worldX;
            vertex[1] = kFaceCorners[face][corner * 3u + 1u];
            // z=0.4 ⇒ 立方体 z∈[-0.1,0.9] 落在正交深度 [0,1] 内，+Z 面不会被远平面裁掉。
            vertex[2] = kFaceCorners[face][corner * 3u + 2u] + 0.4f;
            vertex[3] = kFaceNormals[face][0];
            vertex[4] = kFaceNormals[face][1];
            vertex[5] = kFaceNormals[face][2];
            vertex[6] = 1.0f;
            vertex[9] = (corner == 1u || corner == 2u) ? 1.0f : 0.0f;
            vertex[10] = (corner >= 2u) ? 1.0f : 0.0f;
        }
        uint32_t *index = cube.aIndices + face * 6u;
        const uint32_t base = face * 4u;
        index[0] = base + 0u;
        index[1] = base + 1u;
        index[2] = base + 2u;
        index[3] = base + 0u;
        index[4] = base + 2u;
        index[5] = base + 3u;
    }
    cube.aColor[0] = r;
    cube.aColor[1] = g;
    cube.aColor[2] = b;
}

GN3d::sLIGHT_UBO makeAmbientLight() {
    GN3d::sLIGHT_UBO light;
    std::memset(&light, 0, sizeof(light));
    light.ambientLight[0] = 1.0f;
    light.ambientLight[1] = 1.0f;
    light.ambientLight[2] = 1.0f;
    light.ambientLight[3] = 1.0f;
    light.shadowParams[0] = 2048.0f; // L3c：旧 shadowMapSize 已并入 shadowParams
    return light;
}

int projectX(float worldX, int width) {
    return (int)((worldX / kViewHalfX * 0.5f + 0.5f) * (float)width);
}

bool drawCubes(const GNg::sMAT44 &viewProj, const GN3d::sLIGHT_UBO &light,
               const GN3d::sBATCH_DRAW_ITEM *items, uint32_t count) {
    const GNg::sVEC3 eye = {0.0f, 0.0f, 1.0f};
    if(!GNg::GN_VkBeginFrame()) return false;
    GNg::GN_VkBegin3DPassPBR(&viewProj, &eye, &light, sizeof(light));
    GNg::sMAT44 identity;
    GNg::Mat44Identity(&identity);
    for(uint32_t i = 0; i < count; ++i) {
        // aModel 存的是排序后条目对应的立方体指针：提交顺序即为排序结果。
        const sCUBE *cube = (const sCUBE *)items[i].aModel;
        GNg::GN_VkDraw3DPBR(cube->aVerts, 24, cube->aIndices, 36, &identity, cube->aColor[0],
                            cube->aColor[1], cube->aColor[2], 0.8f, 0.0f, 1.0f);
    }
    GNg::GN_VkEnd3DPass();
    GNg::GN_VkEndFrame();
    vkQueueWaitIdle(GNg::GN_Vk.graphicsQueue);
    return true;
}

bool readFrame(std::vector<uint8_t> &pixels, int &width, int &height) {
    width = (int)GNg::GN_Vk.swapchainExtent.width;
    height = (int)GNg::GN_Vk.swapchainExtent.height;
    return ReadSwapchain(0, 0, width, height, pixels);
}

void reportSortCheck() {
    GN3d::sBATCH_DRAW_ITEM items[4];
    std::memset(items, 0, sizeof(items));
    const uint64_t pipelines[4] = {2, 1, 2, 1};
    const uint64_t materials[4] = {1, 2, 1, 2};
    for(uint32_t i = 0; i < 4; ++i) {
        items[i].aPipelineKey = pipelines[i];
        items[i].aMaterialKey = materials[i];
        items[i].aDepth = 0.5f;
        items[i].aOrder = i;
    }
    const uint32_t switchesBefore = GN3d::countBatchSwitches(items, 4);
    GN3d::sortBatchItems(items, 4);

    bool monotonic = true;
    uint32_t seen = 0;
    for(uint32_t i = 0; i < 4; ++i) {
        if(i > 0) {
            if(items[i].aPipelineKey < items[i - 1].aPipelineKey) monotonic = false;
            if(items[i].aPipelineKey == items[i - 1].aPipelineKey &&
               items[i].aMaterialKey < items[i - 1].aMaterialKey) {
                monotonic = false;
            }
        }
        seen |= (1u << items[i].aOrder);
    }
    const uint32_t switchesAfter = GN3d::countBatchSwitches(items, 4);
    char detail[192];
    std::snprintf(detail, sizeof(detail), "monotonic=%d orderSet=0x%x switches=%u", (int)monotonic,
                  seen, switchesAfter);
    if(monotonic && seen == 0xFu) ReportPass("batch.sort", detail);
    else ReportFail("batch.sort", detail);

    char switchDetail[160];
    std::snprintf(switchDetail, sizeof(switchDetail), "before=%u after=%u (expected 3 -> 1)",
                  switchesBefore, switchesAfter);
    if(switchesBefore == 3u && switchesAfter == 1u) ReportPass("batch.switches", switchDetail);
    else ReportFail("batch.switches", switchDetail);
}

void reportDrawCheck() {
    sCUBE cubes[4];
    buildCube(cubes[0], -2.4f, 0.8f, 0.2f, 0.2f);
    buildCube(cubes[1], -0.8f, 0.2f, 0.8f, 0.2f);
    buildCube(cubes[2], 0.8f, 0.8f, 0.2f, 0.2f);
    buildCube(cubes[3], 2.4f, 0.2f, 0.8f, 0.2f);

    GN3d::sBATCH_DRAW_ITEM items[4];
    std::memset(items, 0, sizeof(items));
    const uint64_t materials[4] = {1, 2, 1, 2};
    for(uint32_t i = 0; i < 4; ++i) {
        items[i].aModel = &cubes[i];
        items[i].aPipelineKey = 1;
        items[i].aMaterialKey = materials[i];
        items[i].aDepth = 0.7f;
        items[i].aOrder = i;
    }

    // bottom/top 传 (下半,上半) ⇒ m[5] 为负，等价真实透视投影的 Y 翻转；
    // 世界 CCW 内容才不会被 frontFace=CCW 的 PBR pass 当背面剔除。
    GNg::sMAT44 viewProj;
    GNg::Mat44OrthoVK(&viewProj, -kViewHalfX, kViewHalfX, kViewHalfY, -kViewHalfY, 0.0f, 1.0f);
    const GN3d::sLIGHT_UBO light = makeAmbientLight();

    std::vector<uint8_t> unsortedPixels;
    std::vector<uint8_t> sortedPixels;
    int width = 0;
    int height = 0;
    // 预热：既有缺陷 Ensure3DBufferCapacity 会在同一 pass 内扩容时销毁旧缓冲，
    // 导致本帧更早的绘制丢失。先画一帧把 3D 动态缓冲扩到足够大，再做对比。
    std::vector<uint8_t> warmupPixels;
    int warmupWidth = 0;
    int warmupHeight = 0;
    drawCubes(viewProj, light, items, 4);
    readFrame(warmupPixels, warmupWidth, warmupHeight);

    bool ok = drawCubes(viewProj, light, items, 4) && readFrame(unsortedPixels, width, height);

    GN3d::sBATCH_DRAW_ITEM sorted[4];
    std::memcpy(sorted, items, sizeof(sorted));
    GN3d::sortBatchItems(sorted, 4);
    ok = ok && drawCubes(viewProj, light, sorted, 4) && readFrame(sortedPixels, width, height);
    if(!ok) {
        ReportFail("batch.draw", "frame or readback failed");
        return;
    }

    const int xs[2] = {projectX(-2.4f, width), projectX(2.4f, width)};
    const int y = height / 2;
    bool same = true;
    char detail[256];
    std::snprintf(detail, sizeof(detail), "samples=");
    for(int i = 0; i < 2; ++i) {
        const sFLOAT_PIXEL a = SampleSwapchain(unsortedPixels, width, xs[i], y);
        const sFLOAT_PIXEL b = SampleSwapchain(sortedPixels, width, xs[i], y);
        const bool pixelSame = std::fabs(a.r - b.r) <= kColorTolerance &&
                               std::fabs(a.g - b.g) <= kColorTolerance &&
                               std::fabs(a.b - b.b) <= kColorTolerance;
        const bool drawn = (a.r + a.g + a.b) > 0.05f;
        if(!pixelSame || !drawn) same = false;
        char part[80];
        std::snprintf(part, sizeof(part), " [%d](%.2f,%.2f,%.2f)/(%.2f,%.2f,%.2f)", i, a.r, a.g,
                      a.b, b.r, b.g, b.b);
        strncat(detail, part, sizeof(detail) - strlen(detail) - 1);
    }
    if(same) ReportPass("batch.draw", detail);
    else ReportFail("batch.draw", detail);
}

} // namespace

void RunBatchChecks() {
    if(!GNg::GN_Vk.initialized) {
        ReportSkip("batch.setup", "GN_Vk not initialized");
        return;
    }
    reportSortCheck();
    reportDrawCheck();
}

} // namespace GpuCheck
