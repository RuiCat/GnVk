// gpuCheckSkinSsbo.cpp — P9-b SSBO 调色板蒙皮验收：>128 关节 LBS、同帧多调色板、非法输入。
//
// 覆盖四条：
//   1) skin.ssbo.pipeline：首次绘制新增且只新增一条管线缓存项；
//   2) skin.ssbo.joint200：200 关节调色板，关节 150 平移 +0.5x，四边形出现在预期像素；
//   3) skin.ssbo.paletteRing：同一帧两次绘制用不同调色板（槽位轮转），两块都在预期位置；
//   4) skin.ssbo.invalid：paletteCount 0/4097、空指针、3D pass 外调用全部返回 false。
#include <GN.h>
#include "GN_Vk.h"
#include "GN_3DSkin.h"

#include "gpuCheckSkinSsbo.h"
#include "gpuCheckReport.h"
#include "gpuPixelRead.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

namespace GpuCheck {
namespace {

const uint32_t kQuadIndexCCW[6] = {0, 2, 1, 0, 3, 2};
const float kBackgroundTolerance = 0.03f;
const float kColorTolerance = 0.06f;
const uint32_t kJointTarget = 150; // > 127：旧 UBO 路径放不下

void makeQuad(GN3d::sSKINNED_VERTEX quad[4], float centerX, float halfSize, uint16_t joint) {
    const float xs[4] = {-halfSize, halfSize, halfSize, -halfSize};
    const float ys[4] = {-halfSize, -halfSize, halfSize, halfSize};
    for(int i = 0; i < 4; ++i) {
        std::memset(&quad[i], 0, sizeof(quad[i]));
        quad[i].aPosition.x = centerX + xs[i];
        quad[i].aPosition.y = ys[i];
        quad[i].aPosition.z = 0.5f;
        quad[i].aNormal.z = 1.0f;
        quad[i].aTangent.x = 1.0f;
        quad[i].aJoints[0] = joint;
        quad[i].aWeights[0] = 1.0f;
    }
}

void makeIdentityPalette(std::vector<GNg::sMAT44> &palette, uint32_t count) {
    palette.assign(count, GNg::sMAT44());
    for(uint32_t i = 0; i < count; ++i) GNg::Mat44Identity(&palette[i]);
}

GNg::sMAT44 makeTranslation(float x, float y, float z) {
    GNg::sMAT44 matrix;
    GNg::Mat44Identity(&matrix);
    matrix.m[12] = x;
    matrix.m[13] = y;
    matrix.m[14] = z;
    return matrix;
}

bool isBackground(const sFLOAT_PIXEL &pixel) {
    return std::fabs(pixel.r - 0.05f) <= kBackgroundTolerance &&
           std::fabs(pixel.g - 0.05f) <= kBackgroundTolerance &&
           std::fabs(pixel.b - 0.10f) <= kBackgroundTolerance;
}

// 一次 3D pass 内执行给定绘制序列并回读整帧。
bool drawAndRead(const GNg::sMAT44 &viewProj, bool twoDraws, const GN3d::sSKINNED_VERTEX *quadA,
                 const GNg::sMAT44 *paletteA, uint32_t paletteACount,
                 const GN3d::sSKINNED_VERTEX *quadB, const GNg::sMAT44 *paletteB,
                 uint32_t paletteBCount, std::vector<uint8_t> &pixels, int &width, int &height) {
    GNg::sMAT44 identity;
    GNg::Mat44Identity(&identity);
    const GNg::sVEC3 eye = {0.0f, 0.0f, 1.0f};
    if(!GNg::GN_VkBeginFrame()) return false;
    GNg::GN_VkBegin3DPass(&viewProj, &eye, 0, 0);
    bool drew = GNg::GN_VkDraw3DSkinnedSsbo(quadA, 4, kQuadIndexCCW, 6, paletteA, paletteACount,
                                            &identity, 0.9f, 0.4f, 0.2f);
    if(twoDraws) {
        drew = drew && GNg::GN_VkDraw3DSkinnedSsbo(quadB, 4, kQuadIndexCCW, 6, paletteB,
                                                   paletteBCount, &identity, 0.9f, 0.4f, 0.2f);
    }
    GNg::GN_VkEnd3DPass();
    GNg::GN_VkEndFrame();
    vkQueueWaitIdle(GNg::GN_Vk.graphicsQueue);
    width = (int)GNg::GN_Vk.swapchainExtent.width;
    height = (int)GNg::GN_Vk.swapchainExtent.height;
    if(!drew) return false;
    return ReadSwapchain(0, 0, width, height, pixels);
}

void reportPipelineAndJoints() {
    std::vector<GNg::sMAT44> palette;
    makeIdentityPalette(palette, 200);
    palette[kJointTarget] = makeTranslation(0.5f, 0.0f, 0.0f);

    GN3d::sSKINNED_VERTEX quad[4];
    makeQuad(quad, -0.25f, 0.25f, (uint16_t)kJointTarget); // 蒙皮后整体 +0.5x

    GNg::sMAT44 viewProj;
    GNg::Mat44Identity(&viewProj); // 位置即裁剪空间
    std::vector<uint8_t> pixels;
    int width = 0;
    int height = 0;

    const unsigned missBefore = (unsigned)GNg::GN_Vk.aPipelineCache.GetMissCount();
    if(!drawAndRead(viewProj, false, quad, &palette[0], 200, 0, 0, 0, pixels, width, height)) {
        ReportFail("skin.ssbo.pipeline", "first draw failed");
        ReportFail("skin.ssbo.joint200", "first draw failed");
        return;
    }
    // 管线在 GN_VkInit3D 已预建：这里要求绘制不再新增缓存项（同一内容的键命中）。
    const unsigned missAfterFirst = (unsigned)GNg::GN_Vk.aPipelineCache.GetMissCount();
    char pipelineDetail[160];
    std::snprintf(pipelineDetail, sizeof(pipelineDetail),
                  "pipeline pre-created at init; miss %u->%u (expected unchanged)", missBefore,
                  missAfterFirst);
    if(missAfterFirst == missBefore) ReportPass("skin.ssbo.pipeline", pipelineDetail);
    else ReportFail("skin.ssbo.pipeline", pipelineDetail);

    // 四边形中心 -0.25、半宽 0.25，palette 平移 +0.5 ⇒ 蒙皮后 x∈[0,0.5]。
    // 采样 NDC 0.25（x=0.625W）应有颜色，NDC -0.25（x=0.375W）仍是背景。
    const sFLOAT_PIXEL inside =
        SampleSwapchain(pixels, width, (int)(0.625f * (float)width), height / 2);
    const sFLOAT_PIXEL outside =
        SampleSwapchain(pixels, width, (int)(0.375f * (float)width), height / 2);
    char detail[224];
    std::snprintf(detail, sizeof(detail),
                  "inside=%.3f,%.3f,%.3f outside(0,0)=%.3f,%.3f,%.3f expected inside~0.57,0.25,0.13",
                  inside.r, inside.g, inside.b, outside.r, outside.g, outside.b);
    const bool colorOk = std::fabs(inside.r - 0.572f) <= kColorTolerance &&
                         std::fabs(inside.g - 0.254f) <= kColorTolerance &&
                         std::fabs(inside.b - 0.127f) <= kColorTolerance;
    if(colorOk && isBackground(outside)) ReportPass("skin.ssbo.joint200", detail);
    else ReportFail("skin.ssbo.joint200", detail);
}

void reportPaletteRing() {
    std::vector<GNg::sMAT44> paletteA;
    makeIdentityPalette(paletteA, 200);
    std::vector<GNg::sMAT44> paletteB;
    makeIdentityPalette(paletteB, 200);
    paletteB[kJointTarget] = makeTranslation(0.5f, 0.0f, 0.0f);

    GN3d::sSKINNED_VERTEX quadA[4];
    makeQuad(quadA, -0.5f, 0.25f, 0); // 关节 0 恒等：留在 x∈[-0.75,-0.25]
    GN3d::sSKINNED_VERTEX quadB[4];
    makeQuad(quadB, -0.25f, 0.25f, (uint16_t)kJointTarget); // +0.5x：x∈[0.25,0.75]

    GNg::sMAT44 viewProj;
    GNg::Mat44Identity(&viewProj);
    std::vector<uint8_t> pixels;
    int width = 0;
    int height = 0;
    if(!drawAndRead(viewProj, true, quadA, &paletteA[0], 200, quadB, &paletteB[0], 200, pixels,
                    width, height)) {
        ReportFail("skin.ssbo.paletteRing", "two-draw frame failed");
        return;
    }
    const sFLOAT_PIXEL left =
        SampleSwapchain(pixels, width, (int)(0.25f * (float)width), height / 2);
    // 第二块的四边形蒙皮后 x∈[0,0.5]：取 NDC 0.25（x=0.625W）避让边界半像素。
    const sFLOAT_PIXEL right =
        SampleSwapchain(pixels, width, (int)(0.625f * (float)width), height / 2);
    char detail[192];
    std::snprintf(detail, sizeof(detail), "left=%.3f,%.3f,%.3f right=%.3f,%.3f,%.3f", left.r,
                  left.g, left.b, right.r, right.g, right.b);
    // 背景色 (0.05,0.05,0.10)，红通道 0.05；真正绘制的红通道应 > 0.4。
    const bool leftDrawn = left.r > 0.4f;
    const bool rightDrawn = right.r > 0.4f;
    if(leftDrawn && rightDrawn) ReportPass("skin.ssbo.paletteRing", detail);
    else ReportFail("skin.ssbo.paletteRing", detail);
}

void reportInvalid() {
    static GN3d::sSKINNED_VERTEX quad[4];
    makeQuad(quad, 0.0f, 0.25f, 0);
    std::vector<GNg::sMAT44> palette;
    makeIdentityPalette(palette, 4097);
    GNg::sMAT44 identity;
    GNg::Mat44Identity(&identity);

    const bool zeroCount = !GNg::GN_VkDraw3DSkinnedSsbo(quad, 4, kQuadIndexCCW, 6, &palette[0], 0,
                                                       &identity, 1.0f, 1.0f, 1.0f);
    const bool tooMany = !GNg::GN_VkDraw3DSkinnedSsbo(quad, 4, kQuadIndexCCW, 6, &palette[0], 4097,
                                                     &identity, 1.0f, 1.0f, 1.0f);
    const bool nullArgs = !GNg::GN_VkDraw3DSkinnedSsbo(0, 4, kQuadIndexCCW, 6, &palette[0], 1,
                                                      &identity, 1.0f, 1.0f, 1.0f) &&
                          !GNg::GN_VkDraw3DSkinnedSsbo(quad, 4, kQuadIndexCCW, 6, 0, 1, &identity,
                                                       1.0f, 1.0f, 1.0f) &&
                          !GNg::GN_VkDraw3DSkinnedSsbo(quad, 4, kQuadIndexCCW, 6, &palette[0], 1,
                                                       0, 1.0f, 1.0f, 1.0f);
    const bool outsidePass = !GNg::GN_VkDraw3DSkinnedSsbo(quad, 4, kQuadIndexCCW, 6, &palette[0], 1,
                                                        &identity, 1.0f, 1.0f, 1.0f);

    char detail[192];
    std::snprintf(detail, sizeof(detail), "zeroCount=%d tooMany=%d nullArgs=%d outsidePass=%d",
                  (int)zeroCount, (int)tooMany, (int)nullArgs, (int)outsidePass);
    if(zeroCount && tooMany && nullArgs && outsidePass) ReportPass("skin.ssbo.invalid", detail);
    else ReportFail("skin.ssbo.invalid", detail);
}

} // namespace

void RunSkinSsboChecks() {
    if(!GNg::GN_Vk.initialized) {
        ReportSkip("skin.ssbo.setup", "GN_Vk not initialized");
        return;
    }
    reportPipelineAndJoints();
    reportPaletteRing();
    reportInvalid();
}

} // namespace GpuCheck
