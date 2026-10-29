// P1b 2D 四混合验收实现：把 swapchain 回读统一解码为线性 RGBA，再做像素数学。
#include "gpuCheck2D.h"

#include <GN.h>
#include "GN_Vk.h"

#include "gpuCheckReport.h"
#include "gpuPixelRead.h"

#include <cmath>
#include <cstdio>
#include <vector>

namespace GpuCheck {
namespace {

const float kTolerance = 0.012f;

bool Near(float actual, float expected) {
    return std::fabs(actual - expected) <= kTolerance;
}

const char *CompareText(const sFLOAT_PIXEL &p, float r, float g, float b, float a) {
    static char text[192];
    std::snprintf(text, sizeof(text), "actual RGBA=%.3f,%.3f,%.3f,%.3f expected=%.3f,%.3f,%.3f,%.3f",
                  p.r, p.g, p.b, p.a, r, g, b, a);
    return text;
}

void CheckPixel(const char *name, const sFLOAT_PIXEL &p, float r, float g, float b, float a) {
    if(Near(p.r, r) && Near(p.g, g) && Near(p.b, b) && Near(p.a, a)) {
        ReportPass(name, CompareText(p, r, g, b, a));
    } else {
        ReportFail(name, CompareText(p, r, g, b, a));
    }
}

void RunBlendCase(const char *name, unsigned short blendMode, float expectedR, float expectedG,
                  float expectedB, float expectedA, float expectedBgB) {
    const uint32_t width = GNg::GN_Vk.swapchainExtent.width;
    const uint32_t height = GNg::GN_Vk.swapchainExtent.height;
    if(!GNg::GN_VkBeginFrame()) {
        ReportFail(name, "GN_VkBeginFrame failed");
        return;
    }
    // 每帧首绘用 normal(1)，避免 vkDraw2d.cpp 的 static s_lastBlendMode 跨帧残留。
    GNg::ImageDraw(0, 0, 0, (int)width, (int)height, 0xFF000080u, 0, 1, 0, 0, 0, 0);
    GNg::ImageDraw(0, (int)(width / 4), (int)(height / 4), (int)(width / 2), (int)(height / 2),
                   0x80800000u, 0, blendMode, 0, 0, 0, 0);
    GNg::GN_VkEndFrame();
    vkQueueWaitIdle(GNg::GN_Vk.graphicsQueue);

    std::vector<uint8_t> pixels;
    if(!ReadSwapchain(0, 0, (int)width, (int)height, pixels)) {
        ReportFail(name, "GN_VkReadBack returned null");
        return;
    }
    CheckPixel(name, SampleSwapchain(pixels, (int)width, (int)(width / 2), (int)(height / 2)),
               expectedR, expectedG, expectedB, expectedA);
    char bgName[96];
    std::snprintf(bgName, sizeof(bgName), "%s.background", name);
    CheckPixel(bgName, SampleSwapchain(pixels, (int)width, (int)(width / 8), (int)(height / 8)),
               0.0f, 0.0f, expectedBgB, 1.0f);
}

} // namespace

void Run2DChecks() {
    if(!GNg::GN_Vk.pipeline || !GNg::GN_Vk.pipelineBright || !GNg::GN_Vk.pipelineMultiply ||
       !GNg::GN_Vk.pipelineNull) {
        ReportSkip("2d.pipelines", "one or more 2D pipelines missing");
        return;
    }
    // 颜色约定：ARGB。背景 = 半亮蓝，前景 = 半亮红且 alpha=0.5。
    const float alpha = 0x80 / 255.0f;
    const float src = 0x80 / 255.0f;
    const float dst = 0x80 / 255.0f;

    // alpha: (a*src, 0, (1-a)*dst, a)
    RunBlendCase("2d.alpha", 1, alpha * src, 0.0f, (1.0f - alpha) * dst, alpha, dst);
    // bright: 颜色 = src*alpha + dst，alpha 因子 ONE/ZERO
    RunBlendCase("2d.bright", 2, alpha * src, 0.0f, dst, alpha, dst);
    // multiply: 颜色 = src*dst（红×蓝→0）
    RunBlendCase("2d.multiply", 11, 0.0f, 0.0f, 0.0f, alpha, 0.0f);
    // null: 直接覆盖，不做混合
    RunBlendCase("2d.null", 0, src, 0.0f, 0.0f, alpha, dst);
}

} // namespace GpuCheck
