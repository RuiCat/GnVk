// P8 渲染接入验收：cullDrawModelsPBR 的三立方体剔除绘制（可见数 + 像素）与 3D 调试线框（中心像素颜色）。
#include <GN.h>
#include "GN_Vk.h"
#include "GN_3Dlight.h"

#include "gpuCheckCull.h"
#include "gpuCheckReport.h"
#include "gpuPixelRead.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

namespace GpuCheck {
namespace {

// 前向 pass 的 clear 色：线性 (0.05,0.05,0.1)。
const float kClearR = 0.05f;
const float kClearG = 0.05f;
const float kClearB = 0.10f;
const float kBackgroundTolerance = 0.02f;
// 验收用正交相机：世界 x∈[-4,4]、y∈[-3,3]、z∈[0,1]（Vulkan NDC 深度）。
const float kViewHalfX = 4.0f;
const float kViewHalfY = 3.0f;

// 白色环境光的 LIGHT_UBO（PBR 立方体只吃环境项，颜色接近 albedo）。
GN3d::sLIGHT_UBO makeAmbientLight() {
    GN3d::sLIGHT_UBO light;
    std::memset(&light, 0, sizeof(light));
    light.ambientLight[0] = 1.0f;
    light.ambientLight[1] = 1.0f;
    light.ambientLight[2] = 1.0f;
    light.ambientLight[3] = 1.0f;
    light.shadowMapSize = 2048.0f;
    return light;
}

// 单位立方体（边长 1，中心在原点）：6 面 × 4 顶点 + 36 索引，绕序与 CreateCubeModel 一致。
// 不用 CreateCubeModel：它的 aVertexCount=36 但只写入 24 个顶点，computeWorldAABB 会读到未初始化
// 数据（GN_3Dmodel.cpp 的已知缺陷，已越界上报，不在 P8 写域内修复）。
void buildCubeMesh(GN3d::sMESH &mesh, float r, float g, float b) {
    static const float kFaceCorners[6][12] = {
        {0.5f, -0.5f, -0.5f, 0.5f, 0.5f, -0.5f, 0.5f, 0.5f, 0.5f, 0.5f, -0.5f, 0.5f},
        {-0.5f, -0.5f, 0.5f, -0.5f, 0.5f, 0.5f, -0.5f, 0.5f, -0.5f, -0.5f, -0.5f, -0.5f},
        {-0.5f, 0.5f, -0.5f, -0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, -0.5f},
        {-0.5f, -0.5f, 0.5f, -0.5f, -0.5f, -0.5f, 0.5f, -0.5f, -0.5f, 0.5f, -0.5f, 0.5f},
        {-0.5f, -0.5f, 0.5f, 0.5f, -0.5f, 0.5f, 0.5f, 0.5f, 0.5f, -0.5f, 0.5f, 0.5f},
        {0.5f, -0.5f, -0.5f, -0.5f, -0.5f, -0.5f, -0.5f, 0.5f, -0.5f, 0.5f, 0.5f, -0.5f}};
    static const float kFaceNormals[6][3] = {{1, 0, 0},  {-1, 0, 0}, {0, 1, 0},
                                             {0, -1, 0}, {0, 0, 1},  {0, 0, -1}};
    const uint32_t vertexCount = 24;
    mesh.aVertexCount = vertexCount;
    mesh.aIndexCount = 36;
    mesh.aVertices = new float[vertexCount * 11];
    mesh.aIndices = new uint32_t[mesh.aIndexCount];
    for(uint32_t face = 0; face < 6; ++face) {
        for(uint32_t corner = 0; corner < 4; ++corner) {
            float *vertex = mesh.aVertices + (face * 4u + corner) * 11u;
            for(int k = 0; k < 11; ++k) vertex[k] = 0.0f;
            vertex[0] = kFaceCorners[face][corner * 3u + 0u];
            vertex[1] = kFaceCorners[face][corner * 3u + 1u];
            vertex[2] = kFaceCorners[face][corner * 3u + 2u];
            vertex[3] = kFaceNormals[face][0];
            vertex[4] = kFaceNormals[face][1];
            vertex[5] = kFaceNormals[face][2];
            vertex[6] = 1.0f; // tangent
            vertex[9] = (corner == 1u || corner == 2u) ? 1.0f : 0.0f;
            vertex[10] = (corner >= 2u) ? 1.0f : 0.0f;
        }
        uint32_t *index = mesh.aIndices + face * 6u;
        const uint32_t base = face * 4u;
        index[0] = base + 0u;
        index[1] = base + 1u;
        index[2] = base + 2u;
        index[3] = base + 0u;
        index[4] = base + 2u;
        index[5] = base + 3u;
    }
    mesh.aDiffuse[0] = r;
    mesh.aDiffuse[1] = g;
    mesh.aDiffuse[2] = b;
}

// 边长 1 的立方体（+Z 面朝向相机）；z 取 0.4 让整体 z∈[-0.1,0.9] 落在正交深度 [0,1] 内，
// 不会因为远平面裁掉 +Z 面而只剩被背面剔除的 -Z 面。
GN3d::cMODEL *makeCubeModel(float worldX) {
    GN3d::cMODEL *model = new GN3d::cMODEL();
    model->aMeshCount = 1;
    model->aMeshes = new GN3d::sMESH[1];
    buildCubeMesh(model->aMeshes[0], 0.8f, 0.8f, 0.8f);
    model->SetPosition(worldX, 0.0f, 0.4f);
    model->computeWorldAABB();
    return model;
}

bool beginFrame() {
    return GNg::GN_VkBeginFrame();
}

void endFrameAndWait() {
    GNg::GN_VkEndFrame();
    vkQueueWaitIdle(GNg::GN_Vk.graphicsQueue);
}

bool readPixels(std::vector<uint8_t> &pixels, int &width, int &height) {
    width = (int)GNg::GN_Vk.swapchainExtent.width;
    height = (int)GNg::GN_Vk.swapchainExtent.height;
    return ReadSwapchain(0, 0, width, height, pixels);
}

int projectX(float worldX, int width) {
    return (int)((worldX / kViewHalfX * 0.5f + 0.5f) * (float)width);
}

int projectY(float worldY, int height) {
    return (int)((worldY / kViewHalfY * 0.5f + 0.5f) * (float)height);
}

bool isBackground(const sFLOAT_PIXEL &pixel) {
    return std::fabs(pixel.r - kClearR) <= kBackgroundTolerance &&
           std::fabs(pixel.g - kClearG) <= kBackgroundTolerance &&
           std::fabs(pixel.b - kClearB) <= kBackgroundTolerance;
}

// PBR pass 的 clear 色是黑 (0,0,0)，与 GN_VkBegin3DPassPBR 的 clear value 对应。
bool isPbrBackground(const sFLOAT_PIXEL &pixel) {
    return pixel.r <= kBackgroundTolerance && pixel.g <= kBackgroundTolerance &&
           pixel.b <= kBackgroundTolerance;
}

bool isDrawn(const sFLOAT_PIXEL &pixel) {
    const float maxChannel = pixel.r > pixel.g ? (pixel.r > pixel.b ? pixel.r : pixel.b)
                                               : (pixel.g > pixel.b ? pixel.g : pixel.b);
    return maxChannel > 0.04f;
}

bool matchesColor(const sFLOAT_PIXEL &pixel, float r, float g, float b, float tolerance) {
    return std::fabs(pixel.r - r) <= tolerance && std::fabs(pixel.g - g) <= tolerance &&
           std::fabs(pixel.b - b) <= tolerance;
}

void reportPixel(const char *name, bool ok, const sFLOAT_PIXEL &pixel, const char *expected) {
    char detail[192];
    std::snprintf(detail, sizeof(detail), "RGBA=%.3f,%.3f,%.3f,%.3f %s",
                  pixel.r, pixel.g, pixel.b, pixel.a, expected);
    if(ok) ReportPass(name, detail);
    else ReportFail(name, detail);
}

// 三立方体：渲染视锥覆盖 x∈[-4,4]，剔除视锥只覆盖 x∈[-1,3] ⇒ 只画原点与 +2x。
void runCullDrawCheck(const GN3d::sFRUSTUM &cullFrustum, const GNg::sMAT44 &viewProj,
                      const GN3d::sLIGHT_UBO &light) {
    GN3d::cMODEL *models[3] = {makeCubeModel(0.0f), makeCubeModel(2.0f), makeCubeModel(-2.0f)};

    const GNg::sVEC3 eye = {0.0f, 0.0f, 1.0f};
    uint32_t visible = 0xFFFFFFFFu;
    uint32_t drawn = 0;
    bool frameOk = beginFrame();
    if(frameOk) {
        GNg::GN_VkBegin3DPassPBR(&viewProj, &eye, &light, sizeof(light));
        drawn = cullDrawModelsPBR(cullFrustum, models, 3, &visible);
        GNg::GN_VkEnd3DPass();
        endFrameAndWait();
    } else {
        ReportFail("cull.draw.frame", "GN_VkBeginFrame failed");
    }

    {
        char detail[160];
        std::snprintf(detail, sizeof(detail), "drawn=%u visible=%u expected 2/2", drawn, visible);
        if(frameOk && drawn == 2u && visible == 2u) ReportPass("cull.draw.count", detail);
        else ReportFail("cull.draw.count", detail);
    }

    if(frameOk) {
        std::vector<uint8_t> pixels;
        int width = 0;
        int height = 0;
        if(!readPixels(pixels, width, height)) {
            ReportFail("cull.draw.origin", "swapchain readback failed");
        } else {
            const sFLOAT_PIXEL origin =
                SampleSwapchain(pixels, width, projectX(0.0f, width), projectY(0.0f, height));
            const sFLOAT_PIXEL plusX =
                SampleSwapchain(pixels, width, projectX(2.0f, width), projectY(0.0f, height));
            const sFLOAT_PIXEL minusX =
                SampleSwapchain(pixels, width, projectX(-2.0f, width), projectY(0.0f, height));
            reportPixel("cull.draw.origin", isDrawn(origin), origin, "expected non-background");
            reportPixel("cull.draw.plusX", isDrawn(plusX), plusX, "expected non-background");
            reportPixel("cull.draw.minusX", isPbrBackground(minusX), minusX,
                        "expected black PBR clear (culled)");
        }
    }

    for(int i = 0; i < 3; ++i) delete models[i];
}

// 空指针/空列表安全：都不绘制、outVisible 归零。
void runEmptyCheck(const GN3d::sFRUSTUM &cullFrustum) {
    uint32_t visible = 0xFFFFFFFFu;
    const uint32_t drawnNull = cullDrawModelsPBR(cullFrustum, 0, 3, &visible);
    const uint32_t visibleNull = visible;
    GN3d::cMODEL *models[1] = {0};
    const uint32_t drawnZero = cullDrawModelsPBR(cullFrustum, models, 0, &visible);

    char detail[160];
    std::snprintf(detail, sizeof(detail), "nullDrawn=%u nullVisible=%u zeroCountDrawn=%u",
                  drawnNull, visibleNull, drawnZero);
    if(drawnNull == 0u && visibleNull == 0u && drawnZero == 0u && visible == 0u) {
        ReportPass("cull.empty", detail);
    } else {
        ReportFail("cull.empty", detail);
    }
}

// 过屏幕中心的水平调试线：中心像素 ≈ 请求颜色，线外仍是背景色。
void runDebugLineCheck(const GNg::sMAT44 &viewProj, const GN3d::sLIGHT_UBO &light) {
    const int height = (int)GNg::GN_Vk.swapchainExtent.height;
    // 1px 线必须落在中心像素的行中心：屏幕几何中心是 4 个像素的公共角点，
    // 线恰好压在行边界时光栅化只覆盖相邻两行各一半，MSAA resolve 后颜色会被背景稀释。
    // 正交投影的 bottom/top 已交换（m[5] 为负 = Y 翻转），世界 y 到屏幕 y_ndc 要再取反。
    const float centerRowNdc = 2.0f * ((float)(height / 2) + 0.5f) / (float)height - 1.0f;
    const float lineWorldY = -centerRowNdc * kViewHalfY;

    GN3d::sDEBUG_LINE line;
    line.aStart.x = -kViewHalfX;
    line.aStart.y = lineWorldY;
    line.aStart.z = 0.5f;
    line.aEnd.x = kViewHalfX;
    line.aEnd.y = lineWorldY;
    line.aEnd.z = 0.5f;
    const unsigned int color = 0xFFFF8000u; // 不透明橙：1.0,0.502,0.0
    const float expectR = 1.0f;
    const float expectG = 128.0f / 255.0f;
    const float expectB = 0.0f;
    const float colorTolerance = 0.05f;

    const GNg::sVEC3 eye = {0.0f, 0.0f, 1.0f};
    if(!beginFrame()) {
        ReportFail("cull.debugLine", "GN_VkBeginFrame failed");
        return;
    }
    GNg::GN_VkBegin3DPass(&viewProj, &eye, &light, sizeof(light));
    GNg::GN_VkDrawDebugLines(&line, 1, color);
    GNg::GN_VkEnd3DPass();
    endFrameAndWait();

    std::vector<uint8_t> pixels;
    int width = 0;
    int readHeight = 0;
    if(!readPixels(pixels, width, readHeight)) {
        ReportFail("cull.debugLine", "swapchain readback failed");
        return;
    }
    const sFLOAT_PIXEL center =
        SampleSwapchain(pixels, width, width / 2, readHeight / 2);
    reportPixel("cull.debugLine", matchesColor(center, expectR, expectG, expectB, colorTolerance),
                center, "expected RGBA~1.000,0.502,0.000");
    const sFLOAT_PIXEL offLine =
        SampleSwapchain(pixels, width, width / 2, projectY(kViewHalfY * 0.5f, readHeight));
    reportPixel("cull.debugLine.outside", isBackground(offLine), offLine, "expected background");
}

} // namespace

// P8 验收入口：剔除绘制（3 立方体 + 空列表）与调试线框像素检查。
void RunCullChecks() {
    // 与真实透视投影一致：Mat44OrthoVK 的 bottom/top 传入 (下半,上半) 让 m[5] 为负（Y 翻转），
    // 世界 CCW 内容才能按正常朝向成像，并被 frontFace=CCW 的 3D pass 判为正面。
    GNg::sMAT44 viewProj;
    GNg::Mat44OrthoVK(&viewProj, -kViewHalfX, kViewHalfX, kViewHalfY, -kViewHalfY, 0.0f, 1.0f);

    // 剔除视锥比渲染视锥窄：世界 x∈[-1,3]，只覆盖原点与 +2x（-2x 在左面之外）。
    GNg::sMAT44 cullViewProj;
    GNg::Mat44OrthoVK(&cullViewProj, -1.0f, 3.0f, 1.0f, -1.0f, 0.0f, 1.0f);
    const GN3d::sFRUSTUM cullFrustum = GN3d::sFRUSTUM::FromViewProj(cullViewProj);

    const GN3d::sLIGHT_UBO light = makeAmbientLight();
    runCullDrawCheck(cullFrustum, viewProj, light);
    runEmptyCheck(cullFrustum);
    runDebugLineCheck(viewProj, light);
}

} // namespace GpuCheck
