// tests/shadowProbe/main.cpp — 定量验证：阴影是**世界空间**的，不随相机转动。
//
// 用户提问："影子不应该随视角发生改变吗？"
//
// 物理事实：阴影由**光源位置 + 遮挡物几何**决定，与相机无关。
// 会随视角变的只有镜面高光。所以正确行为是：
//   相机转动      ⇒ 同一个世界点的明暗**不变**
//   光源移动      ⇒ 同一个世界点的明暗**改变**
//
// 本程序做三次渲染，每次把**同一组世界点**投影到屏幕并采样像素：
//   帧 0：相机 yaw = A，灯在 +X
//   帧 1：相机 yaw = B（转了 ~40°），灯仍在 +X   ← 若阴影跟着相机转，这里读数会变
//   帧 2：相机 yaw = A，灯移到 -X                ← 若阴影由灯决定，这里明暗互换
//
// 验收（任一不满足即返回非 0）：
//   1. 帧 0 与帧 1：背光点比受光点暗，且**两者比值接近**（阴影没动）
//   2. 帧 2：明暗关系**反转**（灯动了，阴影也跟着动）
#include <GN.h>
#include "GN_3Dmath.h"
#include "GN_3Dmodel.h"
#include "GN_3Dlight.h"

#include <png.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {

int gWidth = 640;
int gHeight = 480;
const char *gOutDir = ".local/video/shadow-probe";
bool gVerbose = false;

GN3d::cLIGHT *gLight = nullptr;
GN3d::cCAMERA gCamera;
GN3d::cMODEL *gFloor = nullptr;
GN3d::cMODEL *gPillar = nullptr;

int gFrameIndex = 0;

// 三次渲染的设定：相机 yaw 与灯的位置
struct sPhase {
    float aYaw;
    float aLightX;
    const char *aName;
};
const sPhase kPhases[3] = {
    {0.30f, 3.5f, "相机A 灯+X"},
    {1.00f, 3.5f, "相机B 灯+X"}, // 相机转了约 40°
    {0.30f, -3.5f, "相机A 灯-X"}, // 灯换边
};

// 探测的世界点（地面 y=0.02）
const float kProbeY = 0.02f;
const float kProbeX = 1.45f;
const float kPillarHalfW = 0.25f;

struct sReading {
    int aPhase;
    bool aValid;
    float aShadowSide; // 世界点 -X 处亮度
    float aLitSide; // 世界点 +X 处亮度
    int aShadowPx;
    int aLitPx;
    float aCameraYaw;
    float aLightX;
};
std::vector<sReading> gReadings;

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
    gFloor = GN3d::CreateCubeModel(1.0f, 0.42f, 0.42f, 0.44f);
    gFloor->SetScale(20.0f, 0.10f, 20.0f);
    gFloor->SetPosition(0.0f, -0.05f, 0.0f);
    gFloor->computeWorldAABB();

    gPillar = GN3d::CreateCubeModel(1.0f, 0.72f, 0.70f, 0.66f);
    gPillar->SetScale(kPillarHalfW * 2.0f, 2.4f, kPillarHalfW * 2.0f);
    gPillar->SetPosition(0.0f, 1.2f, 0.0f);
    gPillar->computeWorldAABB();
}

void rebuildLight(float lightX) {
    delete gLight;
    gLight = new GN3d::cLIGHT();
    // 只留一盏点光 + 极弱环境：阴影对比才清楚
    gLight->AddPoint(GNg::sVEC3{lightX, 2.0f, 0.0f}, 14.0f, GNg::sVEC3{1.0f, 0.96f, 0.90f}, 3.2f,
                     true, 0.12f);
    gLight->SetAmbient(GNg::sVEC3{0.020f, 0.021f, 0.026f}, 1.0f);
    gLight->ClearCubeShadowRanges();
    gLight->SetCubeShadowRangeForLight(0, 0.10f, 16.0f, 0);
}

bool buildCamera(float aspect, float yaw, GNg::sMAT44 &viewProj, GNg::sVEC3 &eye) {
    // 较高俯视：地面点投影稳定，且两次 yaw 都能看到 ±X 两侧
    gCamera.SetOrbit(GNg::sVEC3{0.0f, 0.4f, 0.0f}, 9.0f, yaw, 0.85f);
    gCamera.SetFov(50.0f);
    gCamera.SetNF(0.1f, 120.0f);
    GNg::sMAT44 view;
    GNg::sMAT44 projection;
    if (!gCamera.BuildViewProj(aspect, &view, &projection, &viewProj)) return false;
    eye = gCamera.GetCoordV();
    return true;
}

// 世界点 → 像素坐标。返回 false 表示不在屏幕内。
// 口径与 gizmoRayFromScreen 的逆：NDC y **不额外翻转**（Mat44PerspectiveVK 已经翻过）。
bool worldToPixel(const GNg::sMAT44 &viewProj, const GNg::sVEC3 &p, int w, int h, int &outX,
                  int &outY) {
    const float x = viewProj.m[0] * p.x + viewProj.m[4] * p.y + viewProj.m[8] * p.z + viewProj.m[12];
    const float y = viewProj.m[1] * p.x + viewProj.m[5] * p.y + viewProj.m[9] * p.z + viewProj.m[13];
    const float wc = viewProj.m[3] * p.x + viewProj.m[7] * p.y + viewProj.m[11] * p.z + viewProj.m[15];
    if (!(std::fabs(wc) > 1e-6f)) return false;
    const float ndcX = x / wc;
    const float ndcY = y / wc;
    if (ndcX < -1.0f || ndcX > 1.0f || ndcY < -1.0f || ndcY > 1.0f) return false;
    outX = (int)((ndcX * 0.5f + 0.5f) * (float)w);
    outY = (int)((ndcY * 0.5f + 0.5f) * (float)h);
    if (outX < 1 || outY < 1 || outX >= w - 1 || outY >= h - 1) return false;
    return true;
}

// 取 5×5 平均亮度（0..1，按感知权重）
float sampleLuma(const unsigned char *rgba, int w, int h, int cx, int cy) {
    float sum = 0.0f;
    int n = 0;
    for (int dy = -2; dy <= 2; ++dy) {
        for (int dx = -2; dx <= 2; ++dx) {
            const int x = cx + dx;
            const int y = cy + dy;
            if (x < 0 || y < 0 || x >= w || y >= h) continue;
            const unsigned char *p = rgba + ((size_t)y * (size_t)w + (size_t)x) * 4u;
            sum += 0.2126f * (float)p[0] + 0.7152f * (float)p[1] + 0.0722f * (float)p[2];
            ++n;
        }
    }
    return (n > 0) ? (sum / (float)n / 255.0f) : 0.0f;
}

} // namespace

extern "C" {

GN_CALL void AppInit() {
    gCamera.Init();
    buildScene();
    rebuildLight(kPhases[0].aLightX);
    GNg::GN_VkSetEnvironmentLighting(GN3d::eENV_LIGHTING_INDOOR_UNIFORM, 0.0f);
    GNg::GN_VkSetExposure(0.0f);
    GNg::GN_VkSetBloom(0.0f, 1.0f, 0.0f); // 关掉 bloom：它会糊掉明暗对比
    std::fprintf(stderr, "[shadowProbe] 三次渲染，验证阴影是否随视角变化\n");
}

GN_CALL void AppUnInit() {
    delete gFloor;
    gFloor = nullptr;
    delete gPillar;
    gPillar = nullptr;
    delete gLight;
    gLight = nullptr;
}

GN_CALL void AppRun(unsigned int nTime) { (void)nTime; }
GN_CALL void AppDisplay3D(unsigned int nTime) { (void)nTime; }

GN_CALL void AppDisplay2D(unsigned int nTime) {
    (void)nTime;
    if (gLight == nullptr || gFrameIndex >= 3) return;
    const sPhase &ph = kPhases[gFrameIndex];

    // 灯的位置可能变（帧 2）
    if (std::fabs(gLight->GetUBO()->lights[0].position[0] - ph.aLightX) > 1e-4f) {
        rebuildLight(ph.aLightX);
    }

    const float aspect = (float)gWidth / (float)gHeight;
    GNg::sMAT44 viewProj;
    GNg::sVEC3 eye;
    if (!buildCamera(aspect, ph.aYaw, viewProj, eye)) return;

    // cube shadow（单灯，层 0）
    const GNg::sVEC3 lightPos{ph.aLightX, 2.0f, 0.0f};
    if (GNg::GN_VkCubeShadowBeginForLight(0, lightPos, 0.10f, 16.0f)) {
        for (uint32_t face = 0; face < 6; ++face) {
            GNg::GN_VkCubeShadowFaceBegin(lightPos, face, 0.10f, 16.0f);
            gPillar->DrawShadow();
            GNg::GN_VkCubeShadowFaceEnd();
        }
        GNg::GN_VkCubeShadowEnd();
    }

    GNg::cRENDER_FRAME frame;
    GNg::sRENDER_FRAME_DESC desc;
    desc.aViewProj = &viewProj;
    desc.aCameraPos = &eye;
    desc.aLightUBO = gLight->GetUBO();
    desc.aLightUBOSize = sizeof(GN3d::sLIGHT_UBO);
    desc.aDrawSkybox = false;
    if (!frame.Begin(desc)) return;
    gFloor->DrawPBRSorted(eye);
    gPillar->DrawPBRSorted(eye);
    frame.End();

    // 把这一帧要采样的像素坐标记下来（onFrame 里用）
    extern int gProbeShadowPx[3][2];
    extern int gProbeLitPx[3][2];
    extern bool gProbeValid[3];
    int x = 0, y = 0;
    const GNg::sVEC3 pShadow{-kProbeX, kProbeY, 0.0f}; // -X 侧
    const GNg::sVEC3 pLit{kProbeX, kProbeY, 0.0f}; // +X 侧
    const bool okS = worldToPixel(viewProj, pShadow, gWidth, gHeight, x, y);
    gProbeShadowPx[gFrameIndex][0] = x;
    gProbeShadowPx[gFrameIndex][1] = y;
    int x2 = 0, y2 = 0;
    const bool okL = worldToPixel(viewProj, pLit, gWidth, gHeight, x2, y2);
    gProbeLitPx[gFrameIndex][0] = x2;
    gProbeLitPx[gFrameIndex][1] = y2;
    gProbeValid[gFrameIndex] = okS && okL;
    if (gVerbose) {
        std::fprintf(stderr, "[probe] 帧%d(%s) yaw=%.2f 灯X=%+.1f  -X侧像素(%d,%d)  +X侧像素(%d,%d)\n",
                     gFrameIndex, ph.aName, ph.aYaw, ph.aLightX, x, y, x2, y2);
    }
}

} // extern "C"

int gProbeShadowPx[3][2] = {{0, 0}, {0, 0}, {0, 0}};
int gProbeLitPx[3][2] = {{0, 0}, {0, 0}, {0, 0}};
bool gProbeValid[3] = {false, false, false};

namespace {
void onFrame(int index) {
    int w = 0, h = 0, size = 0;
    unsigned char *rgba = GN_GrabFrame(&w, &h, &size);
    if (rgba == nullptr || w <= 0 || h <= 0) return;
    char path[512];
    std::snprintf(path, sizeof(path), "%s/frame_%05d.png", gOutDir, index);
    writePng(path, rgba, w, h);

    if (index < 3) {
        sReading r;
        r.aPhase = index;
        r.aCameraYaw = kPhases[index].aYaw;
        r.aLightX = kPhases[index].aLightX;
        r.aValid = gProbeValid[index];
        if (r.aValid) {
            r.aShadowPx = gProbeShadowPx[index][0];
            int sy = gProbeShadowPx[index][1];
            r.aLitPx = gProbeLitPx[index][0];
            int ly = gProbeLitPx[index][1];
            r.aShadowSide = sampleLuma(rgba, w, h, r.aShadowPx, sy);
            r.aLitSide = sampleLuma(rgba, w, h, r.aLitPx, ly);
        } else {
            r.aShadowPx = r.aLitPx = -1;
            r.aShadowSide = r.aLitSide = -1.0f;
        }
        gReadings.push_back(r);
    }
    GN_GrabFrameFree(rgba);
    ++gFrameIndex;
}
} // namespace

int main(int argc, char **argv) {
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--out") == 0 && i + 1 < argc) gOutDir = argv[++i];
        else if (std::strcmp(argv[i], "--size") == 0 && i + 2 < argc) {
            gWidth = std::atoi(argv[++i]);
            gHeight = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--verbose") == 0) gVerbose = true;
    }
    char cmd[512];
    std::snprintf(cmd, sizeof(cmd), "mkdir -p '%s' && rm -f '%s'/*.png", gOutDir, gOutDir);
    if (std::system(cmd) != 0) std::fprintf(stderr, "[shadowProbe] 清理目录失败\n");
    GN_InitLoadCallBackFunc(AppInit, AppUnInit);
    GN_InitLoopCallBackFunc(AppDisplay2D, AppDisplay3D, AppRun);
    if (!GN_StartRunHeadless((short)gWidth, (short)gHeight, 3, onFrame)) {
        std::fprintf(stderr, "[shadowProbe] 初始化失败\n");
        return 2;
    }

    // ── 报告 + 验收 ──
    std::fprintf(stderr, "\n[shadowProbe] ── 世界点亮度（5x5 平均，0..1）──\n");
    for (size_t i = 0; i < gReadings.size(); ++i) {
        const sReading &r = gReadings[i];
        std::fprintf(stderr, "  帧%d %-12s yaw=%.2f 灯X=%+.1f : -X侧=%.4f  +X侧=%.4f  %s\n", r.aPhase,
                     kPhases[r.aPhase].aName, r.aCameraYaw, r.aLightX, r.aShadowSide, r.aLitSide,
                     r.aValid ? "" : "(投影失败)");
    }
    if (gReadings.size() < 3) {
        std::fprintf(stderr, "[shadowProbe] FAIL: 读数不足\n");
        return 3;
    }
    int failures = 0;

    // 1) 相机转了，同一世界点的明暗应当基本不变
    const sReading &f0 = gReadings[0];
    const sReading &f1 = gReadings[1];
    if (!f0.aValid || !f1.aValid) {
        std::fprintf(stderr, "[shadowProbe] FAIL: 帧 0/1 投影失败\n");
        ++failures;
    } else {
        // 阴影侧必须比受光侧暗（灯在 +X，影子落在 -X）
        const bool s0 = f0.aShadowSide < f0.aLitSide;
        const bool s1 = f1.aShadowSide < f1.aLitSide;
        // 两次相机不同，同一世界点的亮度比值应当接近（允许 15% 相对差）
        const float ratio0 = (f0.aLitSide > 1e-5f) ? (f0.aShadowSide / f0.aLitSide) : 0.0f;
        const float ratio1 = (f1.aLitSide > 1e-5f) ? (f1.aShadowSide / f1.aLitSide) : 0.0f;
        const float relDiff = std::fabs(ratio0 - ratio1) / std::fmax(std::fmax(ratio0, ratio1), 1e-4f);
        char detail[256];
        std::snprintf(detail, sizeof(detail),
                      "帧0 影子/受光=%.3f, 帧1(相机转40°)=%.3f, 相对差 %.1f%%", ratio0, ratio1,
                      relDiff * 100.0f);
        if (s0 && s1 && relDiff < 0.15f) {
            std::fprintf(stderr, "[PASS] shadow.viewIndependence  %s\n", detail);
        } else {
            std::fprintf(stderr, "[FAIL] shadow.viewIndependence  %s（明暗%s%s）\n", detail,
                         s0 ? "" : " 帧0方向反了;", s1 ? "" : " 帧1方向反了;");
            ++failures;
        }
    }

    // 2) 灯换边 → 明暗关系反转（证明阴影由灯决定）
    const sReading &f2 = gReadings[2];
    if (!f2.aValid) {
        std::fprintf(stderr, "[shadowProbe] FAIL: 帧 2 投影失败\n");
        ++failures;
    } else {
        // 灯在 -X ⇒ 影子落在 +X ⇒ 现在 +X 侧更暗
        char detail[256];
        std::snprintf(detail, sizeof(detail), "灯移到 -X 后：-X侧=%.4f +X侧=%.4f（期望 +X 更暗）",
                      f2.aShadowSide, f2.aLitSide);
        if (f2.aLitSide < f2.aShadowSide) {
            std::fprintf(stderr, "[PASS] shadow.lightDriven  %s\n", detail);
        } else {
            std::fprintf(stderr, "[FAIL] shadow.lightDriven  %s\n", detail);
            ++failures;
        }
    }

    if (failures != 0) {
        std::fprintf(stderr, "[shadowProbe] 失败 %d 项\n", failures);
        return 4;
    }
    std::printf("[shadowProbe] 通过：阴影是世界空间的（不随相机变），且由光源位置决定\n");
    return 0;
}
