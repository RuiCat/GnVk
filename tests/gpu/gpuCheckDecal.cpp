// gpuCheckDecal.cpp — P11-S6b 地面贴花验收：pass 边界 / 空输入 / 提交 / 像素可见。
//
// 覆盖四条：
//   1) decal.outsidePass：3D pass 外调用返回 false；
//   2) decal.empty：aCount == 0 或 aItems == nullptr 返回 false；
//   3) decal.draw：64 条贴花在 3D pass 内提交返回 true（覆盖 2× 顶点缓冲扩容）；
//   4) decal.pixels：真实透视相机俯视地面，原点处的红色水渍贴花让中心像素转红。
#include <GN.h>
#include "GN_Vk.h"
#include "GN_3Dlight.h"

#include "gpuCheckDecal.h"
#include "gpuCheckReport.h"
#include "gpuPixelRead.h"

#include <cstring>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

namespace GpuCheck {
namespace {

bool makeGroundCamera(GNg::sMAT44 &viewProj) {
    const float width = (float)GNg::GN_Vk.swapchainExtent.width;
    const float height = (float)GNg::GN_Vk.swapchainExtent.height;
    const float aspect = height > 0.0f ? width / height : 1.0f;
    const GNg::sVEC3 eye = {0.0f, 6.0f, 6.0f};
    const GNg::sVEC3 target = {0.0f, 0.0f, 0.0f};
    const GNg::sVEC3 up = {0.0f, 1.0f, 0.0f};
    GNg::sMAT44 view;
    GNg::sMAT44 projection;
    GNg::Mat44LookAtRH(&view, &eye, &target, &up);
    GNg::Mat44PerspectiveVK(&projection, 1.04719755f, aspect, 0.1f, 200.0f);
    GNg::Mat44Mul(&viewProj, &projection, &view);
    return true;
}

void fillItem(GN3d::sDECAL_RENDER_ITEM &item, float x, float z, float size, float r, float g,
              float b, float a, uint32_t kind) {
    std::memset(&item, 0, sizeof(item));
    item.aPosition[0] = x;
    item.aPosition[1] = 0.02f;
    item.aPosition[2] = z;
    item.aSize = size;
    item.aColor[0] = r;
    item.aColor[1] = g;
    item.aColor[2] = b;
    item.aColor[3] = a;
    item.aKind = kind;
}

bool submitFrame(const GN3d::sDECAL_RENDER_ITEM *items, uint32_t count, bool &submitted) {
    submitted = false;
    GNg::sMAT44 viewProj;
    makeGroundCamera(viewProj);
    GNg::sDECAL_DRAW_PARAMS params;
    params.aViewProj = viewProj;
    params.aItems = items;
    params.aCount = count;
    params.aAlphaScale = 1.0f;
    if(!GNg::GN_VkBeginFrame()) return false;
    const GNg::sVEC3 eye = {0.0f, 6.0f, 6.0f};
    GNg::GN_VkBegin3DPass(&viewProj, &eye, 0, 0);
    submitted = GNg::GN_VkDrawDecals(params);
    GNg::GN_VkEnd3DPass();
    GNg::GN_VkEndFrame();
    vkQueueWaitIdle(GNg::GN_Vk.graphicsQueue);
    return true;
}

bool readCenterPixel(sFLOAT_PIXEL &out) {
    std::vector<uint8_t> pixels;
    const int width = (int)GNg::GN_Vk.swapchainExtent.width;
    const int height = (int)GNg::GN_Vk.swapchainExtent.height;
    if(!ReadSwapchain(0, 0, width, height, pixels)) return false;
    out = SampleSwapchain(pixels, width, width / 2, height / 2);
    return true;
}

void reportOutsidePass() {
    GN3d::sDECAL_RENDER_ITEM item;
    fillItem(item, 0.0f, 0.0f, 0.5f, 1.0f, 0.0f, 0.0f, 1.0f, 0u);
    GNg::sMAT44 viewProj;
    makeGroundCamera(viewProj);
    GNg::sDECAL_DRAW_PARAMS params;
    params.aViewProj = viewProj;
    params.aItems = &item;
    params.aCount = 1;
    params.aAlphaScale = 1.0f;
    bool rejected = false;
    if(GNg::GN_VkBeginFrame()) {
        rejected = !GNg::GN_VkDrawDecals(params);
        GNg::GN_VkEndFrame();
        vkQueueWaitIdle(GNg::GN_Vk.graphicsQueue);
    }
    char detail[128];
    std::snprintf(detail, sizeof(detail), "outside3DPass rejected=%d (expected 1)", (int)rejected);
    if(rejected) ReportPass("decal.outsidePass", detail);
    else ReportFail("decal.outsidePass", detail);
}

void reportEmpty() {
    GN3d::sDECAL_RENDER_ITEM item;
    fillItem(item, 0.0f, 0.0f, 0.5f, 1.0f, 1.0f, 1.0f, 1.0f, 0u);
    bool zeroCountSubmitted = true;
    bool nullItemsSubmitted = true;
    const bool zeroFrame = submitFrame(&item, 0, zeroCountSubmitted);
    const bool nullFrame = submitFrame(nullptr, 4, nullItemsSubmitted);
    const bool rejected = zeroFrame && nullFrame && !zeroCountSubmitted && !nullItemsSubmitted;
    char detail[160];
    std::snprintf(detail, sizeof(detail), "count0=%d nullptr=%d frames=%d/%d (expected all rejected)",
                  (int)!zeroCountSubmitted, (int)!nullItemsSubmitted, (int)zeroFrame,
                  (int)nullFrame);
    if(rejected) ReportPass("decal.empty", detail);
    else ReportFail("decal.empty", detail);
}

void reportDraw() {
    std::vector<GN3d::sDECAL_RENDER_ITEM> items(64);
    for(uint32_t i = 0; i < 64; ++i) {
        const float angle = (float)i * 0.09817477f; // 2π/64
        const float radius = 1.0f + 0.02f * (float)i;
        fillItem(items[i], std::cos(angle) * radius, std::sin(angle) * radius, 0.4f, 0.5f, 0.7f,
                 1.0f, 0.6f, (i % 2u));
    }
    bool submitted = false;
    const bool frameOk = submitFrame(&items[0], (uint32_t)items.size(), submitted);
    char detail[128];
    std::snprintf(detail, sizeof(detail), "items=%u submitted=%d frameOk=%d",
                  (uint32_t)items.size(), (int)submitted, (int)frameOk);
    if(frameOk && submitted) ReportPass("decal.draw", detail);
    else ReportFail("decal.draw", detail);
}

void reportPixels() {
    std::vector<GN3d::sDECAL_RENDER_ITEM> none;
    bool ignored = false;
    sFLOAT_PIXEL clearPixel = {0.0f, 0.0f, 0.0f, 0.0f};
    const bool clearOk = submitFrame(nullptr, 0, ignored) && readCenterPixel(clearPixel);

    GN3d::sDECAL_RENDER_ITEM item;
    fillItem(item, 0.0f, 0.0f, 2.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0u); // 红色水渍
    bool submitted = false;
    sFLOAT_PIXEL redPixel = {0.0f, 0.0f, 0.0f, 0.0f};
    const bool redOk = submitFrame(&item, 1, submitted) && submitted && readCenterPixel(redPixel);

    char detail[224];
    std::snprintf(detail, sizeof(detail),
                  "center clear=(%.3f,%.3f,%.3f) decal=(%.3f,%.3f,%.3f) expected red (r > 0.4)",
                  clearPixel.r, clearPixel.g, clearPixel.b, redPixel.r, redPixel.g, redPixel.b);
    if(clearOk && redOk && redPixel.r > 0.4f && redPixel.r > clearPixel.r + 0.2f) {
        ReportPass("decal.pixels", detail);
    } else {
        ReportFail("decal.pixels", detail);
    }
}

// 朝向标记回归：在 PBR 管线里画一块「只在下半空间」的竖四边形（y∈[0,2]），相机在 y=5 平视。
// 若渲染朝向正确，它应出现在画面下半（75% 高度处有颜色、25% 处为清屏）；若 Y 轴被翻，两处互换。
void reportGroundPlacement(bool usePbr, const char *name) {
    GNg::sMAT44 view;
    GNg::sMAT44 projection;
    GNg::sMAT44 viewProj;
    const GNg::sVEC3 eye = {0.0f, 5.0f, 20.0f};
    const GNg::sVEC3 target = {0.0f, 5.0f, 0.0f};
    const GNg::sVEC3 up = {0.0f, 1.0f, 0.0f};
    GNg::Mat44LookAtRH(&view, &eye, &target, &up);
    const float width = (float)GNg::GN_Vk.swapchainExtent.width;
    const float height = (float)GNg::GN_Vk.swapchainExtent.height;
    GNg::Mat44PerspectiveVK(&projection, 1.04719755f, width / (height > 0.0f ? height : 1.0f), 0.1f,
                            200.0f);
    GNg::Mat44Mul(&viewProj, &projection, &view);

    // y∈[0,2] 的竖四边形（低于相机 y=5）。
    float vertices[4 * 11];
    std::memset(vertices, 0, sizeof(vertices));
    const float xs[4] = {-6.0f, 6.0f, 6.0f, -6.0f};
    const float ys[4] = {0.0f, 0.0f, 2.0f, 2.0f};
    for(int i = 0; i < 4; ++i) {
        float *v = vertices + i * 11;
        v[0] = xs[i];
        v[1] = ys[i];
        v[2] = 0.0f;
        v[3] = 0.0f; // 法线 +Z（朝相机）
        v[4] = 0.0f;
        v[5] = 1.0f;
        v[6] = 1.0f;
        v[9] = (xs[i] + 6.0f) / 12.0f;
        v[10] = ys[i] * 0.5f;
    }
    // 正高度视口 + 世界 CCW 内容 ⇒ 3D pass frontFace=CW；decal 管线 cullBack=false，绕序不影响结果。
    const uint32_t indices[6] = {0, 1, 2, 0, 2, 3};

    bool frameOk = false;
    GN3d::sLIGHT_UBO light;
    std::memset(&light, 0, sizeof(light));
    light.ambientLight[0] = 1.0f;
    light.ambientLight[1] = 1.0f;
    light.ambientLight[2] = 1.0f;
    light.ambientLight[3] = 1.0f;
    light.shadowParams[0] = 2048.0f; // L3c：旧 shadowMapSize 已并入 shadowParams
    if(GNg::GN_VkBeginFrame()) {
        GNg::GN_VkBegin3DPass(&viewProj, &eye, &light, sizeof(light));
        GNg::sMAT44 identity;
        GNg::Mat44Identity(&identity);
        if(usePbr) {
            GNg::GN_VkDraw3DPBR(vertices, 4, indices, 6, &identity, 1.0f, 1.0f, 1.0f, 0.9f, 0.0f,
                                1.0f);
        } else {
            GNg::GN_VkDraw3D(vertices, 4, indices, 6, &identity, 1.0f, 1.0f, 1.0f);
        }
        GNg::GN_VkEnd3DPass();
        GNg::GN_VkEndFrame();
        vkQueueWaitIdle(GNg::GN_Vk.graphicsQueue);
        frameOk = true;
    }

    std::vector<uint8_t> pixels;
    const int w = (int)GNg::GN_Vk.swapchainExtent.width;
    const int h = (int)GNg::GN_Vk.swapchainExtent.height;
    const bool readOk = frameOk && ReadSwapchain(0, 0, w, h, pixels);
    // 沿画面中列扫描，找最亮的一行：正确朝向时它应落在下半屏（> 55% 高度），
    // Y 翻转时落到上半屏。
    float brightest = -1.0f;
    int brightestRow = -1;
    for(int y = 0; y < h; ++y) {
        const sFLOAT_PIXEL pixel = SampleSwapchain(pixels, w, w / 2, y);
        const float lum = 0.2126f * pixel.r + 0.7152f * pixel.g + 0.0722f * pixel.b;
        if(lum > brightest) {
            brightest = lum;
            brightestRow = y;
        }
    }
    const float rowNorm = h > 1 ? (float)brightestRow / (float)(h - 1) : 0.0f;
    char detail[224];
    std::snprintf(detail, sizeof(detail),
                  "brightestRow=%d (%.0f%% of height) lum=%.3f expected >55%% (lower half)",
                  brightestRow, rowNorm * 100.0f, brightest);
    if(readOk && brightest > 0.12f && rowNorm > 0.55f) {
        ReportPass(name, detail);
    } else {
        ReportFail(name, detail);
    }
}

} // namespace

void RunDecalChecks() {
    if(!GNg::GN_Vk.initialized) {
        ReportSkip("decal.setup", "GN_Vk not initialized");
        return;
    }
    reportOutsidePass();
    reportEmpty();
    reportDraw();
    reportPixels();
    reportGroundPlacement(false, "ground.placement");
    reportGroundPlacement(true, "ground.pbr");
}

} // namespace GpuCheck
