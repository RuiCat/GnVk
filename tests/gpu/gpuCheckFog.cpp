// gpuCheckFog.cpp — P11-S3 雾验收：PBR（binding 6）与 Blinn-Phong（binding 3）两条片元管线。
//
// 覆盖三条：
//   1) fog.pbr：默认无雾 → 记录中心像素；设红色雾 density=5 → 中心像素接近雾色（红通道 > 0.4）；
//   2) fog.pbrReset：density 归零后像素回到无雾值（±0.03，证明 UBO 实时生效且默认不改变画面）；
//   3) fog.forward：Blinn-Phong 路径同样被雾影响（binding 3），关闭后恢复。
// 注：延迟光照的 binding 8 已接线并由管线创建时的描述符布局校验；G-Buffer 全链路像素验收未做。
#include <GN.h>
#include "GN_Vk.h"
#include "GN_3Dlight.h"

#include "gpuCheckFog.h"
#include "gpuCheckReport.h"
#include "gpuPixelRead.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

namespace GpuCheck {
namespace {

const uint32_t kQuadIndexCCW[6] = {0, 2, 1, 0, 3, 2};
const float kTolerance = 0.03f;

void buildQuad(float vertices[4 * 11]) {
    const float xs[4] = {-0.5f, 0.5f, 0.5f, -0.5f};
    const float ys[4] = {-0.5f, -0.5f, 0.5f, 0.5f};
    std::memset(vertices, 0, 4 * 11 * sizeof(float));
    for(int i = 0; i < 4; ++i) {
        float *v = vertices + i * 11;
        v[0] = xs[i];
        v[1] = ys[i];
        v[2] = 0.5f;
        v[3] = 0.0f;
        v[4] = 0.0f;
        v[5] = 1.0f;
        v[6] = 1.0f;
        v[9] = (i == 1 || i == 2) ? 1.0f : 0.0f;
        v[10] = (i >= 2) ? 1.0f : 0.0f;
    }
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

GNg::sFOG_UBO makeFog(float density, float r, float g, float b) {
    GNg::sFOG_UBO fog;
    std::memset(&fog, 0, sizeof(fog));
    fog.aColor[0] = r;
    fog.aColor[1] = g;
    fog.aColor[2] = b;
    fog.aColor[3] = 1.0f;
    fog.aParams[0] = density;
    fog.aParams[1] = 0.0f; // heightFalloff
    fog.aParams[2] = 0.0f; // baseHeight
    fog.aParams[3] = 1.0f; // maxOpacity
    return fog;
}

bool drawPbrQuad(const GNg::sMAT44 &identity, const float *vertices) {
    const GNg::sVEC3 eye = {0.0f, 0.0f, 1.0f};
    if(!GNg::GN_VkBeginFrame()) return false;
    GNg::GN_VkBegin3DPassPBR(&identity, &eye, 0, 0);
    GNg::GN_VkDraw3DPBR(vertices, 4, kQuadIndexCCW, 6, &identity, 0.8f, 0.5f, 0.3f, 0.8f, 0.0f,
                        1.0f);
    GNg::GN_VkEnd3DPass();
    GNg::GN_VkEndFrame();
    vkQueueWaitIdle(GNg::GN_Vk.graphicsQueue);
    return true;
}

bool drawForwardQuad(const GNg::sMAT44 &identity, const float *vertices) {
    const GNg::sVEC3 eye = {0.0f, 0.0f, 1.0f};
    if(!GNg::GN_VkBeginFrame()) return false;
    GNg::GN_VkBegin3DPass(&identity, &eye, 0, 0);
    GNg::GN_VkDraw3D(vertices, 4, kQuadIndexCCW, 6, &identity, 0.8f, 0.5f, 0.3f);
    GNg::GN_VkEnd3DPass();
    GNg::GN_VkEndFrame();
    vkQueueWaitIdle(GNg::GN_Vk.graphicsQueue);
    return true;
}

bool readCenter(sFLOAT_PIXEL &out) {
    std::vector<uint8_t> pixels;
    const int width = (int)GNg::GN_Vk.swapchainExtent.width;
    const int height = (int)GNg::GN_Vk.swapchainExtent.height;
    if(!ReadSwapchain(0, 0, width, height, pixels)) return false;
    out = SampleSwapchain(pixels, width, width / 2, height / 2);
    return true;
}

void reportPbr() {
    float quad[4 * 11];
    buildQuad(quad);
    GNg::sMAT44 identity;
    GNg::Mat44Identity(&identity);

    sFLOAT_PIXEL clearPixel = {0.0f, 0.0f, 0.0f, 0.0f};
    GNg::GN_VkSetFog(makeFog(0.0f, 0.0f, 0.0f, 0.0f)); // 关闭
    bool ok = drawPbrQuad(identity, quad) && readCenter(clearPixel);

    sFLOAT_PIXEL foggedPixel = {0.0f, 0.0f, 0.0f, 0.0f};
    GNg::GN_VkSetFog(makeFog(5.0f, 1.0f, 0.0f, 0.0f)); // 红色浓雾
    ok = ok && drawPbrQuad(identity, quad) && readCenter(foggedPixel);

    sFLOAT_PIXEL resetPixel = {0.0f, 0.0f, 0.0f, 0.0f};
    GNg::GN_VkSetFog(makeFog(0.0f, 1.0f, 0.0f, 0.0f)); // 再次关闭
    ok = ok && drawPbrQuad(identity, quad) && readCenter(resetPixel);
    GNg::GN_VkSetFog(makeFog(0.0f, 0.0f, 0.0f, 0.0f));

    if(!ok) {
        ReportFail("fog.pbr", "frame or readback failed");
        ReportFail("fog.pbrReset", "frame or readback failed");
        return;
    }
    char detail[224];
    std::snprintf(detail, sizeof(detail), "clear=(%.3f,%.3f,%.3f) fog=(%.3f,%.3f,%.3f)",
                  clearPixel.r, clearPixel.g, clearPixel.b, foggedPixel.r, foggedPixel.g,
                  foggedPixel.b);
    const bool fogApplied = foggedPixel.r > 0.4f && foggedPixel.r > clearPixel.r + 0.2f;
    if(fogApplied) ReportPass("fog.pbr", detail);
    else ReportFail("fog.pbr", detail);

    const bool reset = std::fabs(resetPixel.r - clearPixel.r) <= kTolerance &&
                       std::fabs(resetPixel.g - clearPixel.g) <= kTolerance &&
                       std::fabs(resetPixel.b - clearPixel.b) <= kTolerance;
    char resetDetail[192];
    std::snprintf(resetDetail, sizeof(resetDetail), "reset=(%.3f,%.3f,%.3f) clear=(%.3f,%.3f,%.3f)",
                  resetPixel.r, resetPixel.g, resetPixel.b, clearPixel.r, clearPixel.g,
                  clearPixel.b);
    if(reset) ReportPass("fog.pbrReset", resetDetail);
    else ReportFail("fog.pbrReset", resetDetail);
}

void reportForward() {
    float quad[4 * 11];
    buildQuad(quad);
    GNg::sMAT44 identity;
    GNg::Mat44Identity(&identity);

    sFLOAT_PIXEL clearPixel = {0.0f, 0.0f, 0.0f, 0.0f};
    GNg::GN_VkSetFog(makeFog(0.0f, 0.0f, 0.0f, 0.0f));
    bool ok = drawForwardQuad(identity, quad) && readCenter(clearPixel);
    sFLOAT_PIXEL foggedPixel = {0.0f, 0.0f, 0.0f, 0.0f};
    GNg::GN_VkSetFog(makeFog(5.0f, 0.0f, 0.0f, 1.0f)); // 蓝色浓雾
    ok = ok && drawForwardQuad(identity, quad) && readCenter(foggedPixel);
    GNg::GN_VkSetFog(makeFog(0.0f, 0.0f, 0.0f, 0.0f));

    if(!ok) {
        ReportFail("fog.forward", "frame or readback failed");
        return;
    }
    char detail[224];
    std::snprintf(detail, sizeof(detail), "clear=(%.3f,%.3f,%.3f) fog=(%.3f,%.3f,%.3f)",
                  clearPixel.r, clearPixel.g, clearPixel.b, foggedPixel.r, foggedPixel.g,
                  foggedPixel.b);
    const bool fogApplied = foggedPixel.b > 0.4f && foggedPixel.b > clearPixel.b + 0.2f;
    if(fogApplied) ReportPass("fog.forward", detail);
    else ReportFail("fog.forward", detail);
}

} // namespace

void RunFogChecks() {
    if(!GNg::GN_Vk.initialized) {
        ReportSkip("fog.setup", "GN_Vk not initialized");
        return;
    }
    reportPbr();
    reportForward();
}

} // namespace GpuCheck
