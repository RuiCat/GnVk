// tests/cornellBox/cornellHud.cpp — Cornell Box 左上角叠层
//
// 职责：显示相机位姿、光照变体、GI 开关与快捷键；不参与场景/GI 计算。
// 文本走引擎 cTEXT_ART，由帧末的 2D 文本层统一绘制（与 mmdViewer 的叠层同机制）。
#include "cornellShared.h"

#include <cstdio>

namespace Cornell {

GNg::cTEXT_ART gHudText;
bool gShowHud = true;
bool gSsaoOn = true;
float gSsgiStrength = 0.0f;
bool gEngineGiOn = false;

void initHud() {
    gHudText.Init(nullptr);
    gHudText.SetFreeTypeFont(16);
}

void releaseHud() {
    gHudText.Release();
}

void drawHud() {
    if (!gShowHud || !GNg::ViewCur) return;
    int lineHeight = static_cast<int>(gHudText.GetFontRangeY()) + 2;
    if (lineHeight < 8) lineHeight = 18;
    const int x = GNg::ViewCur->ortho_left + 16;
    int y = GNg::ViewCur->ortho_top + 16;
    char line[256];
    // 深色阴影 + 主色：过曝光照下浅色文本仍可读（不改变 2D 管线的 sRGB 约定）。
    auto drawLine = [&](char *text, int px, int py, unsigned int color) {
        gHudText.OutText(text, 0, px + 1, py + 1, 0, 0, 0xB0000000u, GNg::eBLEND_NORMAL);
        gHudText.OutText(text, 0, px, py, 0, 0, color, GNg::eBLEND_NORMAL);
    };

    const GNg::sVEC3 pos = gCamera.GetCoordV();
    const GNg::sVEC3 forward = gCamera.Forward();
    const char *mode = gHwRtPreview ? "HW RT"
                        : (gGpuRtPreview ? "GPU RT" : (gCpuRtPreview ? "CPU RT" : "engine"));
    std::snprintf(line, sizeof(line), "Cornell Box (%s)  light=%s", mode,
                  gLightMode == 0 ? "sun" : "point");
    drawLine(line, x, y, 0xFFFFFFFFu);
    y += lineHeight;

    std::snprintf(line, sizeof(line), "camera pos(%.2f %.2f %.2f) fwd(%.2f %.2f %.2f)",
                  pos.x, pos.y, pos.z, forward.x, forward.y, forward.z);
    drawLine(line, x, y, 0xFFCCCCCCu);
    y += lineHeight;

    std::snprintf(line, sizeof(line), "GI: IBL=%s  SSAO=%s  SSGI=%.2f",
                  gEngineGiOn ? "on" : "off", gSsaoOn ? "on" : "off",
                  static_cast<double>(gSsgiStrength));
    drawLine(line, x, y, 0xFF99DDFFu);
    y += lineHeight;

    std::snprintf(line, sizeof(line), "FPS %.1f  frame %.1f ms  RT %.1f ms  upload %.1f ms  spp %d",
                  static_cast<double>(gFps), static_cast<double>(gFrameMs),
                  static_cast<double>(gRtWaitMs), static_cast<double>(gUploadMs),
                  gRtSamples);
    drawLine(line, x, y, 0xFFAADDAAu);
    y += lineHeight;

    std::snprintf(line, sizeof(line), "1-4 pose  WASD/QE move  drag look  F orbit");
    drawLine(line, x, y, 0xFFFFDD88u);
    y += lineHeight;

    std::snprintf(line, sizeof(line), "T CPU  Y GPU  U HW RT  O SSAO  P SSGI  I IBL  K light  R reset  H hud  F12 shot");
    drawLine(line, x, y, 0xFFFFDD88u);
}

} // namespace Cornell
