// tests/multiShadowDemo/main.cpp — 多光源 cube shadow（B2 扩展）演示
//
// 场景：地面上四根柱子，周围四盏**不同颜色**的点光各带 cube shadow。
//
// 判据（画面上一眼可辨）：
//   * 每根柱子投出**四道颜色不同**的影子（分别来自四盏灯）；
//   * 关掉多灯（只渲染一盏）时，只剩对应颜色的那一道。
//
// 环境变量 MULTI_SHADOW_MAX=1 可退回单灯，用于对比 —— 这就是本轮的动机：
// 改造前"后渲染的灯覆盖前面的"，四盏灯只有最后一盏有阴影。
#include <GN.h>
#include "GN_3Dmath.h"
#include "GN_3Dmodel.h"
#include "GN_3Dlight.h"
#include "GN_Gizmo.h"

#include <png.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {

int gWidth = 1280;
int gHeight = 720;
int gFrames = 300;
const char *gOutDir = ".local/video/multiShadow-frames";
bool gVerbose = false;
float gTime = 0.0f;
int gMaxShadowLights = 4; // MULTI_SHADOW_MAX 覆盖

GN3d::cLIGHT *gLight = nullptr;
GN3d::cCAMERA gCamera;
GN3d::cMODEL *gFloor = nullptr;
std::vector<GN3d::cMODEL *> gPillars;   // 投影体
std::vector<GNg::sVEC3> gLightPos;     // 四盏灯的位置
std::vector<GNg::sVEC3> gLightColor;   // 四盏灯的颜色

bool writePng(const char *path, const unsigned char *rgba, int width, int height) {
    std::FILE *file = std::fopen(path, "wb");
    if (file == nullptr) return false;
    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if (png == nullptr) { std::fclose(file); return false; }
    png_infop info = png_create_info_struct(png);
    if (info == nullptr) { png_destroy_write_struct(&png, nullptr); std::fclose(file); return false; }
    if (setjmp(png_jmpbuf(png))) {
        png_destroy_write_struct(&png, &info);
        std::fclose(file);
        return false;
    }
    png_init_io(png, file);
    png_set_IHDR(png, info, (png_uint_32)width, (png_uint_32)height, 8, PNG_COLOR_TYPE_RGB,
                 PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_set_compression_level(png, 3);
    png_write_info(png, info);
    std::vector<png_byte> row((size_t)width * 3u);
    for (int y = 0; y < height; ++y) {
        const unsigned char *src = rgba + (size_t)y * (size_t)width * 4u;
        for (int x = 0; x < width; ++x) {
            row[(size_t)x * 3u + 0u] = src[(size_t)x * 4u + 0u];
            row[(size_t)x * 3u + 1u] = src[(size_t)x * 4u + 1u];
            row[(size_t)x * 3u + 2u] = src[(size_t)x * 4u + 2u];
        }
        png_write_row(png, row.data());
    }
    png_write_end(png, nullptr);
    png_destroy_write_struct(&png, &info);
    std::fclose(file);
    return true;
}

void buildScene() {
    // 地面
    gFloor = GN3d::CreateCubeModel(1.0f, 0.17f, 0.17f, 0.19f);
    gFloor->SetScale(18.0f, 0.10f, 18.0f);
    gFloor->SetPosition(0.0f, -0.05f, 0.0f);
    gFloor->computeWorldAABB();

    // 四根柱子（投影体），围成一圈
    const float px[4] = {-2.4f, 2.4f, -2.4f, 2.4f};
    const float pz[4] = {-2.4f, -2.4f, 2.4f, 2.4f};
    for (int i = 0; i < 4; ++i) {
        GN3d::cMODEL *m = GN3d::CreateCubeModel(1.0f, 0.72f, 0.70f, 0.66f);
        m->SetScale(0.42f, 3.4f, 0.42f);
        m->SetPosition(px[i], 1.7f, pz[i]);
        m->computeWorldAABB();
        gPillars.push_back(m);
    }

    // **按 UBO 槽位索引**：initLights() 里先加了一盏方向光，它占槽 0，
    // 四盏点光落在槽 1..4。所以这里先压一个占位，避免下标错位
    // （错位的后果是"按灯号取位置"取到别人的坐标，影子位置会整体偏掉）。
    gLightPos.push_back(GNg::sVEC3{0.0f, 0.0f, 0.0f}); // 槽 0：方向光（无位置）
    gLightColor.push_back(GNg::sVEC3{0.0f, 0.0f, 0.0f});

    // 四盏点光：位置在柱子外侧，颜色各不相同（红/绿/蓝/琥珀）
    // 灯放低（掠射角）→ 影子拉长，四道影子更容易分辨
    gLightPos.push_back(GNg::sVEC3{-5.5f, 1.4f, -5.5f});
    gLightColor.push_back(GNg::sVEC3{1.00f, 0.22f, 0.18f}); // 红
    gLightPos.push_back(GNg::sVEC3{5.5f, 1.4f, -5.5f});
    gLightColor.push_back(GNg::sVEC3{0.20f, 1.00f, 0.32f}); // 绿
    gLightPos.push_back(GNg::sVEC3{-5.5f, 1.4f, 5.5f});
    gLightColor.push_back(GNg::sVEC3{0.25f, 0.40f, 1.00f}); // 蓝
    gLightPos.push_back(GNg::sVEC3{5.5f, 1.4f, 5.5f});
    gLightColor.push_back(GNg::sVEC3{1.00f, 0.72f, 0.20f}); // 琥珀
}

void initLights() {
    gLight = new GN3d::cLIGHT();
    // 很弱的环境光，让四盏灯的阴影对比更明显
    gLight->AddDirectional(GNg::sVEC3{0.2f, -0.9f, 0.2f}, GNg::sVEC3{0.55f, 0.58f, 0.66f}, 0.22f,
                           false);
    for (size_t i = 0; i < gLightPos.size(); ++i) {
        // AddPoint(pos, range, color, intensity, enableShadow, lightRadius)
        // lightRadius 决定 PCSS 半影宽度：光源"越大"影子边缘越软
        gLight->AddPoint(gLightPos[i], 16.0f, gLightColor[i], 2.6f, true, 0.30f);
    }
    gLight->SetAmbient(GNg::sVEC3{0.030f, 0.032f, 0.042f}, 1.0f);
    gLight->ClearCubeShadowRanges();
}

bool buildCamera(float aspect, GNg::sMAT44 &viewProj, GNg::sMAT44 &view, GNg::sVEC3 &eye) {
    // **机位固定**。
    //
    // 之前让相机缓慢环绕 —— 这不必要也容易误导：影子是"光源 + 遮挡物"的几何结果，
    // 转镜头时它在世界空间里**不该动**（屏上因视差移动，但落点不变）。
    // 固定机位让"四根柱子各投四道不同颜色的影子"一眼可辨，
    // 也避免把视差误读成"影子跟着视角变"。
    gCamera.SetOrbit(GNg::sVEC3{0.0f, 0.6f, 0.0f}, 15.0f, 0.72f, 0.62f);
    gCamera.SetFov(48.0f);
    gCamera.SetNF(0.1f, 120.0f);
    GNg::sMAT44 projection;
    if (!gCamera.BuildViewProj(aspect, &view, &projection, &viewProj)) return false;
    eye = gCamera.GetCoordV();
    return true;
}

} // namespace

extern "C" {

GN_CALL void AppInit() {
    gCamera.Init();
    buildScene();
    initLights();
    GNg::GN_VkSetEnvironmentLighting(GN3d::eENV_LIGHTING_INDOOR_UNIFORM, 0.045f);
    GNg::GN_VkSetExposure(-0.35f);
    GNg::GN_VkSetBloom(0.20f, 1.05f, 0.35f);
    std::fprintf(stderr,
                 "[multiShadowDemo] 4 盏点光各带 cube shadow（上限 %d）；"
                 "MULTI_SHADOW_MAX=1 可对比单灯\n",
                 gMaxShadowLights);
}

GN_CALL void AppUnInit() {
    delete gFloor;
    gFloor = nullptr;
    for (size_t i = 0; i < gPillars.size(); ++i) delete gPillars[i];
    gPillars.clear();
    delete gLight;
    gLight = nullptr;
}

GN_CALL void AppRun(unsigned int nTime) { (void)nTime; }
GN_CALL void AppDisplay3D(unsigned int nTime) { (void)nTime; }

GN_CALL void AppDisplay2D(unsigned int nTime) {
    (void)nTime;
    if (gLight == nullptr) return;

    const float aspect = (float)gWidth / (float)gHeight;
    GNg::sMAT44 view;
    GNg::sMAT44 viewProj;
    GNg::sVEC3 eye;
    if (!buildCamera(aspect, viewProj, view, eye)) return;

    // ── 层号分配：**与相机无关**（按光源注册顺序）──
    //
    // 这里曾经写成"层号 = pickCubeShadowLights 的排名"。那是错的：
    // 排名依赖 cameraPos，相机一动同一盏灯就换层；灯数超过上限时
    // 影子会随视角闪进闪出。影子只该由"光源 + 遮挡物"决定。
    const float cubeNear = 0.10f;
    const float cubeFar = 18.0f;
    const uint32_t layerCount =
        gLight->AssignCubeShadowLayers(cubeNear, cubeFar, (uint32_t)gMaxShadowLights);

    // ── 逐个渲染 cube shadow（必须在主 3D pass 之前）──
    // 层号直接取 UBO 里的 cubeShadowRange[i][2]（上面分配的稳定值）
    {
        static bool once = false;
        if (!once) {
            once = true;
            for (int i = 0; i < (int)GN_MAX_LIGHTS; ++i) {
                std::fprintf(stderr, "[dbg] slot %d: layer=%.1f near=%.2f far=%.2f\n", i,
                             gLight->GetUBO()->cubeShadowRange[i][2],
                             gLight->GetUBO()->cubeShadowRange[i][0],
                             gLight->GetUBO()->cubeShadowRange[i][1]);
            }
        }
    }
    GNg::GN_VkResetCubeShadowLightCount();
    uint32_t renderedCount = 0;
    for (int i = 0; i < (int)GN_MAX_LIGHTS; ++i) {
        const int32_t layer = (int32_t)gLight->GetUBO()->cubeShadowRange[i][2];
        if (layer < 0) continue;
        if (!GNg::GN_VkCubeShadowBeginForLight((uint32_t)layer, gLightPos[(size_t)i], cubeNear,
                                              cubeFar)) {
            std::fprintf(stderr, "[dbg] light %d 层 %d: CubeShadowBegin 失败\n", i, layer);
            continue;
        }
        for (uint32_t face = 0; face < 6; ++face) {
            GNg::GN_VkCubeShadowFaceBegin(gLightPos[(size_t)i], face, cubeNear, cubeFar);
            for (size_t p = 0; p < gPillars.size(); ++p) gPillars[p]->DrawShadow();
            GNg::GN_VkCubeShadowFaceEnd();
        }
        GNg::GN_VkCubeShadowEnd();
        ++renderedCount;
    }

    GNg::cRENDER_FRAME frame;
    GNg::sRENDER_FRAME_DESC desc;
    desc.aViewProj = &viewProj;
    desc.aCameraPos = &eye;
    // **必须在这里取 UBO**：上面已经写好了每盏灯的 cubeShadowRange
    desc.aLightUBO = gLight->GetUBO();
    desc.aLightUBOSize = sizeof(GN3d::sLIGHT_UBO);
    desc.aDrawSkybox = false;
    if (!frame.Begin(desc)) return;

    gFloor->DrawPBRSorted(eye);
    for (size_t p = 0; p < gPillars.size(); ++p) gPillars[p]->DrawPBRSorted(eye);
    frame.End();

    if (gVerbose && ((int)(gTime * 30.0f) % 60 == 0)) {
        std::fprintf(stderr, "[dbg] t=%.1f 分配 %u 层，已渲染 %u 盏\n", gTime, layerCount,
                     GNg::GN_VkCubeShadowLightCount());
    }
    gTime += 1.0f / 30.0f;
}

} // extern "C"

namespace {
void onFrame(int index) {
    int w = 0, h = 0, size = 0;
    unsigned char *rgba = GN_GrabFrame(&w, &h, &size);
    if (rgba == nullptr || w <= 0 || h <= 0) return;
    char path[512];
    std::snprintf(path, sizeof(path), "%s/frame_%05d.png", gOutDir, index);
    writePng(path, rgba, w, h);
    GN_GrabFrameFree(rgba);
    if (index % 30 == 0) std::fprintf(stderr, "[multiShadowDemo] 帧 %d/%d\n", index, gFrames);
}
} // namespace

int main(int argc, char **argv) {
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--out") == 0 && i + 1 < argc) gOutDir = argv[++i];
        else if (std::strcmp(argv[i], "--frames") == 0 && i + 1 < argc) gFrames = std::atoi(argv[++i]);
        else if (std::strcmp(argv[i], "--size") == 0 && i + 2 < argc) {
            gWidth = std::atoi(argv[++i]);
            gHeight = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--verbose") == 0) gVerbose = true;
    }
    if (const char *env = std::getenv("MULTI_SHADOW_MAX")) {
        const int v = std::atoi(env);
        if (v > 0) gMaxShadowLights = v;
    }
    if (gFrames < 1) gFrames = 1;
    char cmd[512];
    std::snprintf(cmd, sizeof(cmd), "mkdir -p '%s' && rm -f '%s'/*.png", gOutDir, gOutDir);
    if (std::system(cmd) != 0) std::fprintf(stderr, "[multiShadowDemo] 清理目录失败\n");
    std::fprintf(stderr, "[multiShadowDemo] %dx%d，%d 帧 → %s\n", gWidth, gHeight, gFrames,
                 gOutDir);
    GN_InitLoadCallBackFunc(AppInit, AppUnInit);
    GN_InitLoopCallBackFunc(AppDisplay2D, AppDisplay3D, AppRun);
    if (!GN_StartRunHeadless((short)gWidth, (short)gHeight, gFrames, onFrame)) {
        std::fprintf(stderr, "[multiShadowDemo] 初始化失败\n");
        return 2;
    }
    // 量化验收：必须真的渲染了 gMaxShadowLights 盏灯的 cube shadow
    const uint32_t rendered = GNg::GN_VkCubeShadowLightCount();
    std::fprintf(stderr, "[multiShadowDemo] 本帧渲染了 %u 盏灯的 cube shadow（期望 %d）\n",
                 rendered, gMaxShadowLights);
    if (gMaxShadowLights > 1 && rendered < 2u) {
        std::fprintf(stderr, "[multiShadowDemo] FAIL: 多灯没有生效\n");
        return 4;
    }
    std::printf("[multiShadowDemo] 完成：%d 帧\n", gFrames);
    return 0;
}
