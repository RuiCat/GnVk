// gpuCheckPrecipitation.cpp — P11-S5 降水验收：pass 外/空输入拒绝、512 粒子提交、中心像素可见。
//
// 覆盖四条（检查名固定）：
//   1) precipitation.outsidePass：3D pass 外（2D pass 内）调用返回 false；
//   2) precipitation.empty：aCount == 0 或 aParticles == nullptr 返回 false；
//   3) precipitation.draw：512 个粒子在 3D pass 内提交返回 true（顺带验证 2× 扩容路径）；
//   4) precipitation.pixels：identity viewProj 下把 size=0.5、alpha=1、纯红的粒子放在相机正前方
//      （裁剪空间中心），回读中心像素应 ≈ 纯红：r > 0.4 且比无粒子帧的 r 高 0.2 以上。
#include <GN.h>
#include "GN_Vk.h"

#include "gpuCheckPrecipitation.h"
#include "gpuCheckReport.h"
#include "gpuPixelRead.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

namespace GpuCheck {
namespace {

const uint32_t kDrawCount = 512;

void fillParticle(GN3d::sPARTICLE &particle, float x, float y, float z, float size,
                  const GNg::sVEC4 &color) {
    std::memset(&particle, 0, sizeof(particle));
    particle.aPosition.x = x;
    particle.aPosition.y = y;
    particle.aPosition.z = z;
    particle.aSize = size;
    particle.aColor = color;
}

// 统一装配绘制参数：identity viewProj（顶点位置即裁剪空间）、右/上 = X/Y 轴。
GNg::sPRECIP_DRAW_PARAMS makeParams(const std::vector<GN3d::sPARTICLE> &particles,
                                    float streakScale) {
    GNg::sPRECIP_DRAW_PARAMS params;
    std::memset(&params, 0, sizeof(params));
    GNg::Mat44Identity(&params.aViewProj);
    params.aCameraRight = {1.0f, 0.0f, 0.0f};
    params.aCameraUp = {0.0f, 1.0f, 0.0f};
    params.aParticles = particles.empty() ? nullptr : &particles[0];
    params.aCount = (uint32_t)particles.size();
    params.aStreakScale = streakScale;
    params.aAlphaScale = 1.0f;
    return params;
}

// 完整的一帧：BeginFrame → Begin3DPass → Draw → End3DPass → EndFrame → 等待 GPU。
bool submitFrame(const GNg::sPRECIP_DRAW_PARAMS &params, bool &submitted) {
    submitted = false;
    if(!GNg::GN_VkBeginFrame()) return false;
    const GNg::sVEC3 eye = {0.0f, 0.0f, 1.0f};
    GNg::GN_VkBegin3DPass(&params.aViewProj, &eye, 0, 0);
    submitted = GNg::GN_VkDrawPrecipitation(params);
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

// 3D pass 外（此时 2D pass 活跃）必须拒绝，且不产生任何提交。
bool drawOutsidePass(const GNg::sPRECIP_DRAW_PARAMS &params) {
    if(!GNg::GN_VkBeginFrame()) return false;
    const bool rejected = !GNg::GN_VkDrawPrecipitation(params);
    GNg::GN_VkEndFrame();
    vkQueueWaitIdle(GNg::GN_Vk.graphicsQueue);
    return rejected;
}

void reportOutsidePass() {
    std::vector<GN3d::sPARTICLE> particles(1);
    fillParticle(particles[0], 0.0f, 0.0f, 0.5f, 0.1f, GNg::sVEC4(1.0f, 0.0f, 0.0f, 1.0f));
    const GNg::sPRECIP_DRAW_PARAMS params = makeParams(particles, 1.0f);
    const bool rejected = drawOutsidePass(params);
    char detail[160];
    std::snprintf(detail, sizeof(detail), "outside3DPass rejected=%d (expected 1)",
                  (int)rejected);
    if(rejected) ReportPass("precipitation.outsidePass", detail);
    else ReportFail("precipitation.outsidePass", detail);
}

void reportEmpty() {
    std::vector<GN3d::sPARTICLE> particles(1);
    fillParticle(particles[0], 0.0f, 0.0f, 0.5f, 0.1f, GNg::sVEC4(1.0f, 1.0f, 1.0f, 1.0f));
    GNg::sPRECIP_DRAW_PARAMS emptyCount = makeParams(particles, 1.0f);
    emptyCount.aCount = 0;
    GNg::sPRECIP_DRAW_PARAMS nullParticles = makeParams(particles, 1.0f);
    nullParticles.aParticles = nullptr;

    bool emptySubmitted = true;
    bool nullSubmitted = true;
    const bool emptyFrame = submitFrame(emptyCount, emptySubmitted);
    const bool nullFrame = submitFrame(nullParticles, nullSubmitted);
    const bool rejected = emptyFrame && nullFrame && !emptySubmitted && !nullSubmitted;
    char detail[192];
    std::snprintf(detail, sizeof(detail),
                  "count0=%d nullptr=%d frames=%d/%d (expected all rejected)",
                  (int)!emptySubmitted, (int)!nullSubmitted, (int)emptyFrame, (int)nullFrame);
    if(rejected) ReportPass("precipitation.empty", detail);
    else ReportFail("precipitation.empty", detail);
}

// 512 个粒子铺满视口中央区域：只验证提交成功（覆盖容量 2× 扩容）。
void reportDraw() {
    std::vector<GN3d::sPARTICLE> particles(kDrawCount);
    for(uint32_t i = 0; i < kDrawCount; ++i) {
        const float column = (float)(i % 32u);
        const float row = (float)(i / 32u);
        fillParticle(particles[i], (column - 15.5f) / 20.0f, (row - 7.5f) / 20.0f, 0.5f, 0.02f,
                     GNg::sVEC4(0.3f, 0.5f, 0.9f, 0.5f));
    }
    const GNg::sPRECIP_DRAW_PARAMS params = makeParams(particles, 4.0f);
    bool submitted = false;
    const bool frameOk = submitFrame(params, submitted);
    char detail[160];
    std::snprintf(detail, sizeof(detail), "particles=%u submitted=%d frameOk=%d", kDrawCount,
                  (int)submitted, (int)frameOk);
    if(frameOk && submitted) ReportPass("precipitation.draw", detail);
    else ReportFail("precipitation.draw", detail);
}

// 真实透视相机：eye=(0,0,5) 看向原点，60° fov。
// 必须用透视：identity viewProj 的 gl_Position.w 恒为 1，会被 precipitation.frag 的
// 近相机淡出 smoothstep(1.5, 6.0, w) 判成 alpha=0（这不是管线缺陷，是 identity 不产生视深）。
void makePerspectiveViewProj(GNg::sMAT44 &viewProj) {
    const GNg::sVEC3 eye = {0.0f, 0.0f, 5.0f};
    const GNg::sVEC3 center = {0.0f, 0.0f, 0.0f};
    const GNg::sVEC3 up = {0.0f, 1.0f, 0.0f};
    GNg::sMAT44 view;
    GNg::sMAT44 projection;
    GNg::Mat44LookAtRH(&view, &eye, &center, &up);
    const float width = (float)GNg::GN_Vk.swapchainExtent.width;
    const float height = (float)GNg::GN_Vk.swapchainExtent.height;
    const float aspect = height > 0.0f ? width / height : 1.0f;
    GNg::Mat44PerspectiveVK(&projection, 1.04719755f, aspect, 0.1f, 100.0f);
    GNg::Mat44Mul(&viewProj, &projection, &view);
}

// 中心像素：无粒子帧 vs 单个纯红大粒子帧（透视 VP，粒子在原点 ⇒ 视深 w=5）。
void reportPixels() {
    GNg::sMAT44 viewProj;
    makePerspectiveViewProj(viewProj);

    std::vector<GN3d::sPARTICLE> none;
    GNg::sPRECIP_DRAW_PARAMS clearParams = makeParams(none, 1.0f);
    clearParams.aViewProj = viewProj;
    bool ignored = false;
    sFLOAT_PIXEL clearPixel = {0.0f, 0.0f, 0.0f, 0.0f};
    const bool clearOk = submitFrame(clearParams, ignored) && readCenterPixel(clearPixel);

    std::vector<GN3d::sPARTICLE> particles(1);
    fillParticle(particles[0], 0.0f, 0.0f, 0.0f, 0.5f, GNg::sVEC4(1.0f, 0.0f, 0.0f, 1.0f));
    GNg::sPRECIP_DRAW_PARAMS redParams = makeParams(particles, 1.0f);
    redParams.aViewProj = viewProj;
    bool submitted = false;
    sFLOAT_PIXEL redPixel = {0.0f, 0.0f, 0.0f, 0.0f};
    const bool redOk = submitFrame(redParams, submitted) && submitted && readCenterPixel(redPixel);

    if(!clearOk || !redOk) {
        ReportFail("precipitation.pixels",
                   "frame or swapchain readback failed (clear/red frame)");
        return;
    }
    // 只要求中心 r 明显高于无粒子帧：该处 alpha=1，红粒子完全覆盖背景。
    const bool visible = redPixel.r > 0.4f && redPixel.r > clearPixel.r + 0.2f;
    char detail[256];
    std::snprintf(detail, sizeof(detail),
                  "center clear=(%.3f,%.3f,%.3f) red=(%.3f,%.3f,%.3f) expected red~1 "
                  "(r > %.2f)",
                  clearPixel.r, clearPixel.g, clearPixel.b, redPixel.r, redPixel.g, redPixel.b,
                  clearPixel.r + 0.2f);
    if(visible) ReportPass("precipitation.pixels", detail);
    else ReportFail("precipitation.pixels", detail);
}

// 不透明四边形（VkVertex3D 11 float，与 PBR 管线一致）：世界空间、+Z 朝相机、世界 CCW。
// 世界 CCW + 透视（Y 翻转）= 帧缓冲 CCW ⇒ PBR 的 frontFace=CCW 判为正面。
const uint32_t kQuadIndexWorldCcw[6] = {0, 1, 2, 0, 2, 3};

void buildQuadPbr(float vertices[4 * 11], float half, float z) {
    const float xs[4] = {-half, half, half, -half};
    const float ys[4] = {-half, -half, half, half};
    std::memset(vertices, 0, 4 * 11 * sizeof(float));
    for(int i = 0; i < 4; ++i) {
        float *v = vertices + i * 11;
        v[0] = xs[i];
        v[1] = ys[i];
        v[2] = z;
        v[5] = 1.0f;
        v[6] = 1.0f;
    }
}

// 与查看器同序的一帧：Begin3DPass → (可选) PBR 不透明四边形 → 降水 → End3DPass → EndFrame。
bool submitFrameWithGeometry(const GNg::sPRECIP_DRAW_PARAMS &params, bool withQuad,
                             bool &submitted) {
    submitted = false;
    if(!GNg::GN_VkBeginFrame()) return false;
    const GNg::sVEC3 eye = {0.0f, 0.0f, 5.0f};
    GNg::GN_VkBegin3DPass(&params.aViewProj, &eye, 0, 0);
    if(withQuad) {
        float quad[4 * 11];
        buildQuadPbr(quad, 1.0f, 0.0f);
        GNg::sMAT44 identity;
        GNg::Mat44Identity(&identity);
        GNg::GN_VkDraw3DPBR(quad, 4, kQuadIndexWorldCcw, 6, &identity, 0.1f, 0.4f, 0.6f, 0.8f,
                            0.0f, 1.0f);
    }
    submitted = GNg::GN_VkDrawPrecipitation(params);
    GNg::GN_VkEnd3DPass();
    GNg::GN_VkEndFrame();
    vkQueueWaitIdle(GNg::GN_Vk.graphicsQueue);
    return true;
}

// 真实透视相机：eye=(0,0,5) 看向原点，60° fov；粒子在原点 ⇒ 应落在画面中心。
void reportPerspective() {
    const GNg::sVEC3 eye = {0.0f, 0.0f, 5.0f};
    const GNg::sVEC3 center = {0.0f, 0.0f, 0.0f};
    const GNg::sVEC3 up = {0.0f, 1.0f, 0.0f};
    GNg::sMAT44 view;
    GNg::sMAT44 projection;
    GNg::sMAT44 viewProj;
    GNg::Mat44LookAtRH(&view, &eye, &center, &up);
    const float width = (float)GNg::GN_Vk.swapchainExtent.width;
    const float height = (float)GNg::GN_Vk.swapchainExtent.height;
    const float aspect = height > 0.0f ? width / height : 1.0f;
    GNg::Mat44PerspectiveVK(&projection, 1.04719755f, aspect, 0.1f, 100.0f);
    GNg::Mat44Mul(&viewProj, &projection, &view);

    std::vector<GN3d::sPARTICLE> particles(1);
    fillParticle(particles[0], 0.0f, 0.0f, 0.0f, 0.6f, GNg::sVEC4(1.0f, 0.0f, 0.0f, 1.0f));
    GNg::sPRECIP_DRAW_PARAMS params = makeParams(particles, 1.0f);
    params.aViewProj = viewProj; // 覆盖 identity：需要真实透视矩阵
    bool submitted = false;
    sFLOAT_PIXEL pixel = {0.0f, 0.0f, 0.0f, 0.0f};
    const bool ok = submitFrame(params, submitted) && submitted && readCenterPixel(pixel);
    char detail[224];
    std::snprintf(detail, sizeof(detail),
                  "perspective submitted=%d center=(%.3f,%.3f,%.3f) expected red (r > 0.4)",
                  (int)submitted, pixel.r, pixel.g, pixel.b);
    if(ok && pixel.r > 0.4f) ReportPass("precipitation.perspective", detail);
    else ReportFail("precipitation.perspective", detail);
}

// 关键回归：不透明四边形（世界 z=0，视深 5）先画，粒子（世界 z=1，视深 4，更近）后画
// ⇒ 粒子必须覆盖它。这正是"雨点没有落在角色/几何体上"会失败的地方（深度测试把近处半透明物体剔掉）。
void reportOverGeometry() {
    GNg::sMAT44 viewProj;
    makePerspectiveViewProj(viewProj);
    std::vector<GN3d::sPARTICLE> particles(1);
    fillParticle(particles[0], 0.0f, 0.0f, 1.0f, 0.6f, GNg::sVEC4(1.0f, 0.0f, 0.0f, 1.0f));
    GNg::sPRECIP_DRAW_PARAMS withParticles = makeParams(particles, 1.0f);
    withParticles.aViewProj = viewProj;
    GNg::sPRECIP_DRAW_PARAMS empty = makeParams(std::vector<GN3d::sPARTICLE>(), 1.0f);
    empty.aViewProj = withParticles.aViewProj;

    bool quadSubmitted = false;
    sFLOAT_PIXEL quadPixel = {0.0f, 0.0f, 0.0f, 0.0f};
    const bool quadOk =
        submitFrameWithGeometry(empty, true, quadSubmitted) && readCenterPixel(quadPixel);
    bool particleSubmitted = false;
    sFLOAT_PIXEL overPixel = {0.0f, 0.0f, 0.0f, 0.0f};
    const bool overOk = submitFrameWithGeometry(withParticles, true, particleSubmitted) &&
                        particleSubmitted && readCenterPixel(overPixel);

    char detail[224];
    std::snprintf(detail, sizeof(detail),
                  "quadOnly=(%.3f,%.3f,%.3f) withParticles=(%.3f,%.3f,%.3f) expected red over quad",
                  quadPixel.r, quadPixel.g, quadPixel.b, overPixel.r, overPixel.g, overPixel.b);
    const bool visible = quadOk && overOk && overPixel.r > 0.4f &&
                         overPixel.r > quadPixel.r + 0.2f;
    if(visible) ReportPass("precipitation.overGeometry", detail);
    else ReportFail("precipitation.overGeometry", detail);
}

// 查看器同参数回归：81920 颗雪、粒子柱 60×24×60 m、相机在 ~66 m 外（截图为证：
// 模拟侧 alive=81920/81920、vel.y=-5.97，但画面上看不到雪）。
// 这条复刻查看器的粒子数量/尺寸/相机距离，用来区分「管线在大批量下不画」与「查看器接线错」。
void reportSnowCurtain() {
    const GNg::sVEC3 eye = {3.5f, 26.77f, -62.94f};
    const GNg::sVEC3 forward = {0.0f, -0.13f, 0.99f};
    const GNg::sVEC3 center = {eye.x + forward.x * 10.0f, eye.y + forward.y * 10.0f,
                               eye.z + forward.z * 10.0f};
    const GNg::sVEC3 up = {0.0f, 1.0f, 0.0f};
    GNg::sMAT44 view;
    GNg::sMAT44 projection;
    GNg::sMAT44 viewProj;
    GNg::Mat44LookAtRH(&view, &eye, &center, &up);
    const float width = (float)GNg::GN_Vk.swapchainExtent.width;
    const float height = (float)GNg::GN_Vk.swapchainExtent.height;
    const float aspect = height > 0.0f ? width / height : 1.0f;
    GNg::Mat44PerspectiveVK(&projection, 0.785398f, aspect, 0.179f, 600.0f);
    GNg::Mat44Mul(&viewProj, &projection, &view);

    // 与查看器 cameraBasis 相同的右/上：right = normalize(forward × worldUp)，up = right × forward。
    GNg::sVEC3 right;
    GNg::sVEC3 camUp;
    GNg::Vec3Cross(&right, &forward, &up);
    GNg::Vec3Normalize(&right, &right);
    GNg::Vec3Cross(&camUp, &right, &forward);
    GNg::Vec3Normalize(&camUp, &camUp);

    const uint32_t count = 81920;
    std::vector<GN3d::sPARTICLE> particles(count);
    for (uint32_t i = 0; i < count; ++i) {
        const float fx = (float)(i % 160u) / 159.0f;
        const float fz = (float)((i / 160u) % 160u) / 159.0f;
        const float fy = (float)((i / 25600u) % 4u) / 3.0f;
        fillParticle(particles[i], -30.0f + 60.0f * fx, 24.0f * fy, -30.0f + 60.0f * fz, 0.20f,
                     GNg::sVEC4(0.97f, 0.98f, 1.0f, 0.95f));
        particles[i].aVelocity.x = 1.90f;
        particles[i].aVelocity.y = -5.97f;
        particles[i].aVelocity.z = -0.46f;
    }
    GNg::sPRECIP_DRAW_PARAMS params = makeParams(particles, 1.0f);
    params.aViewProj = viewProj;
    params.aCameraRight = right;
    params.aCameraUp = camUp;

    bool submitted = false;
    const bool frameOk = submitFrame(params, submitted);

    std::vector<uint8_t> pixels;
    const int w = (int)width;
    const int h = (int)height;
    const bool readOk = frameOk && ReadSwapchain(0, 0, w, h, pixels);
    long lit = 0;
    if (readOk) {
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                const sFLOAT_PIXEL p = SampleSwapchain(pixels, w, x, y);
                if (p.r > 0.2f && p.g > 0.2f && p.b > 0.2f) ++lit;
            }
        }
    }
    char detail[256];
    std::snprintf(detail, sizeof(detail),
                  "submitted=%d frameOk=%d litPixels=%ld/%d (expect > 2%%)",
                  (int)submitted, (int)frameOk, lit, w * h);
    if (readOk && submitted && lit > (long)(w * h / 50)) ReportPass("precipitation.snowCurtain", detail);
    else ReportFail("precipitation.snowCurtain", detail);
}

// 定义在文件末（同一匿名命名空间）。
void reportFlipOrientation();
void reportScreenAgreement();

} // namespace

void RunPrecipitationChecks() {
    if(!GNg::GN_Vk.initialized) {
        ReportSkip("precipitation.setup", "GN_Vk not initialized");
        return;
    }
    reportOutsidePass();
    reportEmpty();
    reportDraw();
    reportPixels();
    reportPerspective();
    reportOverGeometry();
    reportSnowCurtain();
    reportFlipOrientation();
    reportScreenAgreement();
}

} // namespace GpuCheck

namespace GpuCheck {
namespace {

// 朝向回归（主人反馈：包围盒显示粒子从上往下，但渲染出的粒子从下往上）。
// 相机在 (0,0,5) 看原点；粒子放在世界 y=-2（轴下方）⇒ 屏幕上必须落在**下半**（行 > H/2）。
// 若落在上半，说明降水路径相对场景被 Y 翻转。
void reportFlipOrientation() {
    GNg::sMAT44 viewProj;
    makePerspectiveViewProj(viewProj);
    std::vector<GN3d::sPARTICLE> particles(1);
    fillParticle(particles[0], 0.0f, -2.0f, 0.0f, 0.6f, GNg::sVEC4(1.0f, 0.0f, 0.0f, 1.0f));
    GNg::sPRECIP_DRAW_PARAMS params = makeParams(particles, 1.0f);
    params.aViewProj = viewProj;

    bool submitted = false;
    const bool frameOk = submitFrame(params, submitted);
    std::vector<uint8_t> pixels;
    const int w = (int)GNg::GN_Vk.swapchainExtent.width;
    const int h = (int)GNg::GN_Vk.swapchainExtent.height;
    const bool readOk = frameOk && ReadSwapchain(0, 0, w, h, pixels);
    long litRows = 0;
    double rowSum = 0.0;
    long lit = 0;
    if (readOk) {
        for (int y = 0; y < h; ++y) {
            long rowLit = 0;
            for (int x = 0; x < w; ++x) {
                const sFLOAT_PIXEL p = SampleSwapchain(pixels, w, x, y);
                if (p.r > 0.4f && p.g < 0.3f) ++rowLit;
            }
            if (rowLit > 0) {
                ++litRows;
                rowSum += (double)y;
                lit += rowLit;
            }
        }
    }
    const double meanRow = litRows > 0 ? rowSum / (double)litRows : -1.0;
    char detail[256];
    std::snprintf(detail, sizeof(detail),
                  "submitted=%d lit=%ld meanRow=%.1f/%d (expect lower half > %d)",
                  (int)submitted, lit, meanRow, h, h / 2);
    if (readOk && submitted && lit > 0 && meanRow > (double)(h / 2)) {
        ReportPass("precipitation.orientation", detail);
    } else {
        ReportFail("precipitation.orientation", detail);
    }
}

} // namespace

} // namespace GpuCheck

namespace GpuCheck {
namespace {

// 对账（主人反馈：包围盒正确、粒子不在盒子里且向上走）：
// 用查看器同款相机（(34.70,26.36,-6.89) 看向原点）放一颗已知世界位置的粒子，
// 比较「CPU 投影出的中心」与「实际渲染出的红像素质心」。两者必须一致（±3 px）。
void reportScreenAgreement() {
    const GNg::sVEC3 eye = {34.70f, 26.36f, -6.89f};
    const GNg::sVEC3 center = {0.0f, 6.0f, 0.0f};
    const GNg::sVEC3 up = {0.0f, 1.0f, 0.0f};
    GNg::sMAT44 view;
    GNg::sMAT44 projection;
    GNg::sMAT44 viewProj;
    GNg::Mat44LookAtRH(&view, &eye, &center, &up);
    const float width = (float)GNg::GN_Vk.swapchainExtent.width;
    const float height = (float)GNg::GN_Vk.swapchainExtent.height;
    const float aspect = height > 0.0f ? width / height : 1.0f;
    GNg::Mat44PerspectiveVK(&projection, 0.785398f, aspect, 0.179f, 600.0f);
    GNg::Mat44Mul(&viewProj, &projection, &view);

    // 粒子放在已知世界点（模型前方偏上），用 CPU 同一矩阵算期望屏幕坐标。
    const GNg::sVEC3 particlePos = {2.0f, 8.0f, 4.0f};
    const float *m = viewProj.m;
    const float cx = m[0] * particlePos.x + m[4] * particlePos.y + m[8] * particlePos.z + m[12];
    const float cy = m[1] * particlePos.x + m[5] * particlePos.y + m[9] * particlePos.z + m[13];
    const float cw = m[3] * particlePos.x + m[7] * particlePos.y + m[11] * particlePos.z + m[15];
    const int expectX = (int)((cx / cw * 0.5f + 0.5f) * width);
    const int expectY = (int)((cy / cw * 0.5f + 0.5f) * height);

    std::vector<GN3d::sPARTICLE> particles(1);
    fillParticle(particles[0], particlePos.x, particlePos.y, particlePos.z, 0.9f,
                 GNg::sVEC4(1.0f, 0.0f, 0.0f, 1.0f));
    GNg::sPRECIP_DRAW_PARAMS params = makeParams(particles, 1.0f);
    params.aViewProj = viewProj;

    bool submitted = false;
    const bool frameOk = submitFrame(params, submitted);
    std::vector<uint8_t> pixels;
    const int w = (int)width;
    const int h = (int)height;
    const bool readOk = frameOk && ReadSwapchain(0, 0, w, h, pixels);
    long sumX = 0, sumY = 0, lit = 0;
    if (readOk) {
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                const sFLOAT_PIXEL p = SampleSwapchain(pixels, w, x, y);
                if (p.r > 0.4f && p.g < 0.3f) {
                    sumX += x;
                    sumY += y;
                    ++lit;
                }
            }
        }
    }
    const int actualX = lit > 0 ? (int)(sumX / lit) : -1;
    const int actualY = lit > 0 ? (int)(sumY / lit) : -1;
    char detail[256];
    std::snprintf(detail, sizeof(detail),
                  "expect=(%d,%d) actual=(%d,%d) lit=%ld fov/camera=viewer-like",
                  expectX, expectY, actualX, actualY, lit);
    const bool ok = readOk && submitted && lit > 0 && std::abs(actualX - expectX) <= 3 &&
                    std::abs(actualY - expectY) <= 3;
    if (ok) ReportPass("precipitation.screenAgreement", detail);
    else ReportFail("precipitation.screenAgreement", detail);
}

} // namespace
} // namespace GpuCheck
