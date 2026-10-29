// gpuCheckCloud.cpp — P11-S2b 中景 billboard 云层验收：pass 边界 + 提交 + alpha 混合像素。
//
// 覆盖四条：
//   1) cloud.outsidePass：pass 外返回 false；pass 内 tiles=0 / tiles=65 / tileSize=0 也被拒绝；
//   2) cloud.draw：3D pass 内合法参数返回 true（管线/UBO 惰性创建成功）；
//   3) cloud.pixels：同一相机先取无云基线、再画云，画面上方 4 个采样点亮度提升 > 0.02；
//   4) cloud.destroy：GN_VkCloudDestroy 可重复调用，之后 pass 外绘制仍返回 false。
// 相机在云层（y=20）下方、约 45° 仰角：水平云层铺满画面上方，采样点稳定落在云层上。
#include <GN.h>
#include "GN_Vk.h"

#include "gpuCheckCloud.h"
#include "gpuCheckReport.h"
#include "gpuPixelRead.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

namespace GpuCheck {
namespace {

const float kPi = 3.14159265f;
const float kLayerHeight = 20.0f;
const float kTileSize = 200.0f;
const uint32_t kTilesPerAxis = 4;
const float kLuminanceEpsilon = 0.02f;

struct sCLOUD_CAMERA {
    GNg::sMAT44 aViewProj;
    GNg::sVEC3 aEye;
};

// 相机在 y=2 仰望 (0,12,10)：垂直 fov 60° ⇒ 画面所有射线都向上，水平云层 y=20 铺满画面。
sCLOUD_CAMERA makeCloudCamera() {
    sCLOUD_CAMERA camera;
    const GNg::sVEC3 eye = {0.0f, 2.0f, 0.0f};
    const GNg::sVEC3 center = {0.0f, 12.0f, 10.0f};
    const GNg::sVEC3 up = {0.0f, 1.0f, 0.0f};
    const float width = (float)GNg::GN_Vk.swapchainExtent.width;
    const float height = (float)GNg::GN_Vk.swapchainExtent.height;
    const float aspect = height > 0.0f ? width / height : 1.0f;

    GNg::sMAT44 view;
    GNg::Mat44LookAtRH(&view, &eye, &center, &up);
    GNg::sMAT44 projection;
    GNg::Mat44PerspectiveVK(&projection, 60.0f * kPi / 180.0f, aspect, 0.1f, 500.0f);
    GNg::Mat44Mul(&camera.aViewProj, &projection, &view);
    camera.aEye = eye;
    return camera;
}

GNg::sCLOUD_DRAW_PARAMS makeCloudParams(const sCLOUD_CAMERA &camera) {
    GNg::sCLOUD_DRAW_PARAMS params;
    params.aViewProj = camera.aViewProj;
    params.aCameraPos = camera.aEye;
    params.aLayerHeight = kLayerHeight;
    params.aWind.x = 0.0f;
    params.aWind.y = 0.0f;
    params.aWind.z = 0.0f;
    params.aTileSize = kTileSize;
    params.aTilesPerAxis = kTilesPerAxis;
    params.aCoverage = 1.0f; // 满覆盖：alpha ≈ 1，采样点不落在噪声空洞里
    params.aSharpness = 0.9f; // 硬边缘：soft 仅 0.02，避免空洞
    params.aOpacity = 1.0f;
    params.aTimeSeconds = 0.0f;
    params.aNearFade = 0.0f;
    return params;
}

// 提交一帧（3D pass 内可选画云），pixels 非空时回读整张 swapchain。
bool renderCloudFrame(const sCLOUD_CAMERA &camera, const GNg::sCLOUD_DRAW_PARAMS &params,
                      bool withCloud, bool *drew, std::vector<uint8_t> *pixels) {
    if(drew) *drew = false;
    if(!GNg::GN_VkBeginFrame()) return false;
    GNg::GN_VkBegin3DPass(&camera.aViewProj, &camera.aEye, nullptr, 0);
    if(withCloud && drew) *drew = GNg::GN_VkDrawCloud(params);
    GNg::GN_VkEnd3DPass();
    GNg::GN_VkEndFrame();
    vkQueueWaitIdle(GNg::GN_Vk.graphicsQueue);
    if(!pixels) return true;
    const int width = (int)GNg::GN_Vk.swapchainExtent.width;
    const int height = (int)GNg::GN_Vk.swapchainExtent.height;
    return ReadSwapchain(0, 0, width, height, *pixels);
}

float luminance(const sFLOAT_PIXEL &pixel) {
    return 0.2126f * pixel.r + 0.7152f * pixel.g + 0.0722f * pixel.b;
}

void reportOutsidePass(const sCLOUD_CAMERA &camera, const GNg::sCLOUD_DRAW_PARAMS &params) {
    const bool outsidePass = !GNg::GN_VkDrawCloud(params);
    bool invalidRejected = false;
    // pass 内的非法参数必须在同一个「BeginFrame → Begin3DPass → ... → End3DPass → EndFrame」帧里测。
    if(GNg::GN_VkBeginFrame()) {
        GNg::GN_VkBegin3DPass(&camera.aViewProj, &camera.aEye, nullptr, 0);
        GNg::sCLOUD_DRAW_PARAMS bad = params;
        bad.aTilesPerAxis = 0;
        const bool zeroTiles = !GNg::GN_VkDrawCloud(bad);
        bad.aTilesPerAxis = 65;
        const bool tooManyTiles = !GNg::GN_VkDrawCloud(bad);
        bad.aTilesPerAxis = kTilesPerAxis;
        bad.aTileSize = 0.0f;
        const bool zeroTileSize = !GNg::GN_VkDrawCloud(bad);
        GNg::GN_VkEnd3DPass();
        GNg::GN_VkEndFrame();
        vkQueueWaitIdle(GNg::GN_Vk.graphicsQueue);
        invalidRejected = zeroTiles && tooManyTiles && zeroTileSize;
    }
    char detail[192];
    std::snprintf(detail, sizeof(detail), "outsidePass=%d invalidRejected=%d",
                  (int)outsidePass, (int)invalidRejected);
    if(outsidePass && invalidRejected) ReportPass("cloud.outsidePass", detail);
    else ReportFail("cloud.outsidePass", detail);
}

void reportDraw(const sCLOUD_CAMERA &camera, const GNg::sCLOUD_DRAW_PARAMS &params) {
    bool drew = false;
    const bool frameOk = renderCloudFrame(camera, params, true, &drew, nullptr);
    char detail[192];
    std::snprintf(detail, sizeof(detail), "submit=%d tiles=%u tileSize=%.0f", (int)drew,
                  params.aTilesPerAxis, params.aTileSize);
    if(frameOk && drew) ReportPass("cloud.draw", detail);
    else ReportFail("cloud.draw", detail);
}

void reportPixels(const sCLOUD_CAMERA &camera, const GNg::sCLOUD_DRAW_PARAMS &params) {
    std::vector<uint8_t> clearPixels;
    std::vector<uint8_t> cloudPixels;
    bool drew = false;
    const bool ok = renderCloudFrame(camera, params, false, nullptr, &clearPixels) &&
                    renderCloudFrame(camera, params, true, &drew, &cloudPixels);
    if(!ok || !drew) {
        ReportFail("cloud.pixels", "frame/draw/readback failed");
        return;
    }

    const int width = (int)GNg::GN_Vk.swapchainExtent.width;
    const int height = (int)GNg::GN_Vk.swapchainExtent.height;
    const float sampleX[4] = {0.30f, 0.50f, 0.70f, 0.50f};
    const float sampleY[4] = {0.35f, 0.30f, 0.35f, 0.55f};
    bool allBrighter = true;
    float minDelta = 1.0f;
    char points[192] = {0};
    size_t used = 0;
    for(int i = 0; i < 4; ++i) {
        const int x = (int)(sampleX[i] * (float)width);
        const int y = (int)(sampleY[i] * (float)height);
        const sFLOAT_PIXEL clearPixel = SampleSwapchain(clearPixels, width, x, y);
        const sFLOAT_PIXEL cloudPixel = SampleSwapchain(cloudPixels, width, x, y);
        const float delta = luminance(cloudPixel) - luminance(clearPixel);
        minDelta = std::fmin(minDelta, delta);
        if(delta <= kLuminanceEpsilon) allBrighter = false;
        if(used < sizeof(points)) {
            const int written = std::snprintf(points + used, sizeof(points) - used,
                                             "%s(%d,%d)%.3f", i ? " " : "", x, y, (double)delta);
            if(written > 0) used += (size_t)written;
        }
    }
    char detail[384];
    std::snprintf(detail, sizeof(detail), "minDelta=%.3f clear=(%.3f,%.3f,%.3f) %s", minDelta,
                  luminance(SampleSwapchain(clearPixels, width, width / 2, height / 2)),
                  luminance(SampleSwapchain(cloudPixels, width, width / 2, height / 2)), points);
    if(allBrighter) ReportPass("cloud.pixels", detail);
    else ReportFail("cloud.pixels", detail);
}

void reportDestroy(const GNg::sCLOUD_DRAW_PARAMS &params) {
    GNg::GN_VkCloudDestroy();
    GNg::GN_VkCloudDestroy(); // 可重复调用
    const bool stillNeedsPass = !GNg::GN_VkDrawCloud(params);
    char detail[160];
    std::snprintf(detail, sizeof(detail), "doubleDestroy=1 outsidePassAfterDestroy=%d",
                  (int)stillNeedsPass);
    if(stillNeedsPass) ReportPass("cloud.destroy", detail);
    else ReportFail("cloud.destroy", detail);
}

} // namespace

void RunCloudChecks() {
    if(!GNg::GN_Vk.initialized) {
        ReportSkip("cloud.setup", "GN_Vk not initialized");
        return;
    }
    const sCLOUD_CAMERA camera = makeCloudCamera();
    const GNg::sCLOUD_DRAW_PARAMS params = makeCloudParams(camera);
    reportOutsidePass(camera, params);
    reportDraw(camera, params);
    reportPixels(camera, params);
    reportDestroy(params);
}

} // namespace GpuCheck
