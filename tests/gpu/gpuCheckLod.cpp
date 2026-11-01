// gpuCheckLod.cpp — P10 动态 LOD 验收：两级网格在近/远距离选不同级，绘制提交的 indexCount 随之下降。
//
// 覆盖三条：
//   1) lod.select：同一网格在近（cam 10）/远（cam 1000）距离分别选 0 / 1 级；
//   2) lod.indexCount：按选级结果绘制时，GN_VkLast3DIndexCount() == 该级索引数（36 / 6）；
//   3) lod.singleLevel：无 LOD 的网格（levels 空）仍提交 LOD0 的 36 个索引（回归保护）。
#include <GN.h>
#include "GN_Vk.h"
#include "GN_3Dlod.h"
#include "GN_3Dlight.h"

#include "gpuCheckLod.h"
#include "gpuCheckReport.h"
#include "gpuPixelRead.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

namespace GpuCheck {
namespace {

const uint32_t kLod0IndexCount = 36;
const uint32_t kLod1IndexCount = 6;
const float kPi = 3.14159265358979323846f;
const float kColorTolerance = 0.05f;

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

// 前面（+z）四边形：4 顶点 6 索引，位置与 LOD0 立方体的前面完全一致（避免切换时轮廓变化）。
void buildFrontFace(float *vertices, uint32_t *indices) {
    const float xs[4] = {-0.5f, 0.5f, 0.5f, -0.5f};
    const float ys[4] = {-0.5f, -0.5f, 0.5f, 0.5f};
    for(int i = 0; i < 4; ++i) {
        float *vertex = vertices + i * 11u;
        std::memset(vertex, 0, 11u * sizeof(float));
        vertex[0] = xs[i];
        vertex[1] = ys[i];
        vertex[2] = 0.5f; // 立方体前面
        vertex[3] = 0.0f;
        vertex[4] = 0.0f;
        vertex[5] = 1.0f;
        vertex[6] = 1.0f;
        vertex[9] = (i == 1 || i == 2) ? 1.0f : 0.0f;
        vertex[10] = (i >= 2) ? 1.0f : 0.0f;
    }
    const uint32_t order[6] = {0, 2, 1, 0, 3, 2}; // 与 gpuCheckMaterial 一致：Vulkan Y 翻转后为正面
    for(uint32_t i = 0; i < 6; ++i) indices[i] = order[i];
}

void buildFullCube(float *vertices, uint32_t *indices) {
    static const float kFaceCorners[6][12] = {
        {0.5f, -0.5f, -0.5f, 0.5f, 0.5f, -0.5f, 0.5f, 0.5f, 0.5f, 0.5f, -0.5f, 0.5f},
        {-0.5f, -0.5f, 0.5f, -0.5f, 0.5f, 0.5f, -0.5f, 0.5f, -0.5f, -0.5f, -0.5f, -0.5f},
        {-0.5f, 0.5f, -0.5f, -0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, -0.5f},
        {-0.5f, -0.5f, 0.5f, -0.5f, -0.5f, -0.5f, 0.5f, -0.5f, -0.5f, 0.5f, -0.5f, 0.5f},
        {-0.5f, -0.5f, 0.5f, 0.5f, -0.5f, 0.5f, 0.5f, 0.5f, 0.5f, -0.5f, 0.5f, 0.5f},
        {0.5f, -0.5f, -0.5f, -0.5f, -0.5f, -0.5f, -0.5f, 0.5f, -0.5f, 0.5f, 0.5f, -0.5f}};
    static const float kFaceNormals[6][3] = {{1, 0, 0},  {-1, 0, 0}, {0, 1, 0},
                                             {0, -1, 0}, {0, 0, 1},  {0, 0, -1}};
    for(uint32_t face = 0; face < 6; ++face) {
        for(uint32_t corner = 0; corner < 4; ++corner) {
            float *vertex = vertices + (face * 4u + corner) * 11u;
            std::memset(vertex, 0, 11u * sizeof(float));
            vertex[0] = kFaceCorners[face][corner * 3u];
            vertex[1] = kFaceCorners[face][corner * 3u + 1u];
            vertex[2] = kFaceCorners[face][corner * 3u + 2u];
            vertex[3] = kFaceNormals[face][0];
            vertex[4] = kFaceNormals[face][1];
            vertex[5] = kFaceNormals[face][2];
            vertex[6] = 1.0f;
            vertex[9] = (corner == 1u || corner == 2u) ? 1.0f : 0.0f;
            vertex[10] = (corner >= 2u) ? 1.0f : 0.0f;
        }
        uint32_t *index = indices + face * 6u;
        const uint32_t base = face * 4u;
        // 与 LOD1 前面同一绕序（Vulkan Y 翻转后为正面），保证两级中心像素可直接对比。
        index[0] = base + 0u;
        index[1] = base + 2u;
        index[2] = base + 1u;
        index[3] = base + 0u;
        index[4] = base + 3u;
        index[5] = base + 2u;
    }
}

// 两级网格：LOD0 = 完整立方体，LOD1 = 只有前面的四边形（同 AABB、maxError 0.5m）。
void buildLodMesh(GN3d::sMESH &mesh, std::vector<float> &lod0Verts, std::vector<uint32_t> &lod0Idx,
                  std::vector<float> &lod1Verts, std::vector<uint32_t> &lod1Idx) {
    mesh.Release();
    mesh.aLods = new GN3d::sMESH_LOD[2];
    mesh.aLodCount = 2;

    lod0Verts.assign(24u * 11u, 0.0f);
    lod0Idx.assign(36u, 0u);
    buildFullCube(&lod0Verts[0], &lod0Idx[0]);
    mesh.aLods[0].aVertices = new float[lod0Verts.size()];
    std::memcpy(mesh.aLods[0].aVertices, &lod0Verts[0], lod0Verts.size() * sizeof(float));
    mesh.aLods[0].aIndices = new uint32_t[lod0Idx.size()];
    std::memcpy(mesh.aLods[0].aIndices, &lod0Idx[0], lod0Idx.size() * sizeof(uint32_t));
    mesh.aLods[0].aVertexCount = 24;
    mesh.aLods[0].aIndexCount = 36;
    mesh.aLods[0].aMaxError = 0.0f;

    lod1Verts.assign(4u * 11u, 0.0f);
    lod1Idx.assign(6u, 0u);
    buildFrontFace(&lod1Verts[0], &lod1Idx[0]);
    mesh.aLods[1].aVertices = new float[lod1Verts.size()];
    std::memcpy(mesh.aLods[1].aVertices, &lod1Verts[0], lod1Verts.size() * sizeof(float));
    mesh.aLods[1].aIndices = new uint32_t[lod1Idx.size()];
    std::memcpy(mesh.aLods[1].aIndices, &lod1Idx[0], lod1Idx.size() * sizeof(uint32_t));
    mesh.aLods[1].aVertexCount = 4;
    mesh.aLods[1].aIndexCount = 6;
    mesh.aLods[1].aMaxError = 0.5f;
}

GN3d::sLOD_VIEW makeView(float cameraZ) {
    GN3d::sLOD_VIEW view;
    std::memset(&view, 0, sizeof(view));
    view.aScreenHeight = 480.0f;
    view.aFovYRad = 60.0f * kPi / 180.0f;
    view.aCamPos = GNg::sVEC3{0.0f, 0.0f, cameraZ};
    return view;
}

// 绘制一帧：identity viewProj（位置即裁剪空间），返回是否成功。
bool drawFrame(const GNg::sMAT44 &identity, const GN3d::sLIGHT_UBO &light, const GN3d::cMODEL &model,
               const uint32_t *levels, uint32_t levelCount) {
    const GNg::sVEC3 eye = {0.0f, 0.0f, 1.0f};
    if(!GNg::GN_VkBeginFrame()) return false;
    GNg::GN_VkBegin3DPassPBR(&identity, &eye, &light, sizeof(light));
    model.DrawPBRWithLod(levels, levelCount);
    GNg::GN_VkEnd3DPass();
    GNg::GN_VkEndFrame();
    vkQueueWaitIdle(GNg::GN_Vk.graphicsQueue);
    return true;
}

void reportSelectAndIndexCount() {
    GN3d::cMODEL model;
    model.aMeshes = new GN3d::sMESH[1];
    model.aMeshCount = 1;
    model.aVisible = true;
    std::vector<float> lod0Verts;
    std::vector<uint32_t> lod0Idx;
    std::vector<float> lod1Verts;
    std::vector<uint32_t> lod1Idx;
    buildLodMesh(model.aMeshes[0], lod0Verts, lod0Idx, lod1Verts, lod1Idx);
    model.aWorldAABB =
        GN3d::sAABB(GNg::sVEC3{-0.5f, -0.5f, 0.2f}, GNg::sVEC3{0.5f, 0.5f, 1.2f});
    model.UpdateTransform();

    GN3d::sLOD_VIEW nearView = makeView(10.0f);
    GN3d::sLOD_VIEW farView = makeView(1000.0f);
    const uint32_t nearLevel =
        GN3d::selectLod(model.aMeshes[0], model.aWorldAABB, nearView, 1.0f, 0);
    const uint32_t farLevel =
        GN3d::selectLod(model.aMeshes[0], model.aWorldAABB, farView, 1.0f, 0);
    char selectDetail[160];
    std::snprintf(selectDetail, sizeof(selectDetail), "near=%u far=%u expected 0/1", nearLevel,
                  farLevel);
    if(nearLevel == 0u && farLevel == 1u) ReportPass("lod.select", selectDetail);
    else ReportFail("lod.select", selectDetail);

    GNg::sMAT44 identity;
    GNg::Mat44Identity(&identity);
    const GN3d::sLIGHT_UBO light = makeAmbientLight();

    uint32_t levels[1] = {nearLevel};
    const bool nearOk = drawFrame(identity, light, model, levels, 1);
    const uint32_t nearIndexCount = GNg::GN_VkLast3DIndexCount();
    levels[0] = farLevel;
    const bool farOk = drawFrame(identity, light, model, levels, 1);
    const uint32_t farIndexCount = GNg::GN_VkLast3DIndexCount();
    char indexDetail[192];
    std::snprintf(indexDetail, sizeof(indexDetail),
                  "nearIdx=%u farIdx=%u expected 36/6 (draw frames ok=%d/%d)", nearIndexCount,
                  farIndexCount, (int)nearOk, (int)farOk);
    if(nearOk && farOk && nearIndexCount == kLod0IndexCount && farIndexCount == kLod1IndexCount) {
        ReportPass("lod.indexCount", indexDetail);
    } else {
        ReportFail("lod.indexCount", indexDetail);
    }

    // 单级回归：levels 为空 ⇒ 直接画 LOD0（36 索引），与旧 DrawPBR 行为一致。
    const bool singleOk = drawFrame(identity, light, model, 0, 0);
    const uint32_t singleIndexCount = GNg::GN_VkLast3DIndexCount();
    char singleDetail[160];
    std::snprintf(singleDetail, sizeof(singleDetail), "idx=%u draw=%d expected 36 (levels=none)",
                  singleIndexCount, (int)singleOk);
    if(singleOk && singleIndexCount == kLod0IndexCount) ReportPass("lod.singleLevel", singleDetail);
    else ReportFail("lod.singleLevel", singleDetail);

    // 像素回归：LOD0 与 LOD1 都包含立方体前面，中心像素应一致（±0.05）。
    std::vector<uint8_t> lod0Pixels;
    std::vector<uint8_t> lod1Pixels;
    const int width = (int)GNg::GN_Vk.swapchainExtent.width;
    const int height = (int)GNg::GN_Vk.swapchainExtent.height;
    levels[0] = 0;
    bool pixelOk = drawFrame(identity, light, model, levels, 1) &&
                   ReadSwapchain(0, 0, width, height, lod0Pixels);
    levels[0] = 1;
    pixelOk = pixelOk && drawFrame(identity, light, model, levels, 1) &&
              ReadSwapchain(0, 0, width, height, lod1Pixels);
    if(!pixelOk) {
        ReportFail("lod.pixels", "frame or readback failed");
    } else {
        const sFLOAT_PIXEL a = SampleSwapchain(lod0Pixels, width, width / 2, height / 2);
        const sFLOAT_PIXEL b = SampleSwapchain(lod1Pixels, width, width / 2, height / 2);
        const bool same = std::fabs(a.r - b.r) <= kColorTolerance &&
                          std::fabs(a.g - b.g) <= kColorTolerance &&
                          std::fabs(a.b - b.b) <= kColorTolerance;
        char detail[192];
        std::snprintf(detail, sizeof(detail), "LOD0=(%.3f,%.3f,%.3f) LOD1=(%.3f,%.3f,%.3f)",
                      a.r, a.g, a.b, b.r, b.g, b.b);
        if(same) ReportPass("lod.pixels", detail);
        else ReportFail("lod.pixels", detail);
    }

    delete[] model.aMeshes;
    model.aMeshes = 0;
    model.aMeshCount = 0;
}

} // namespace

void RunLodChecks() {
    if(!GNg::GN_Vk.initialized) {
        ReportSkip("lod.setup", "GN_Vk not initialized");
        return;
    }
    reportSelectAndIndexCount();
}

} // namespace GpuCheck
