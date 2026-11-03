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

    const GNg::sVEC3 pos = gCamera.GetCoordV();
    const GNg::sVEC3 forward = gCamera.Forward();
    const char *mode = gHwRtPreview ? "HW RT"
                        : (gGpuRtPreview ? "GPU RT" : (gCpuRtPreview ? "CPU RT" : "engine"));
    std::snprintf(line, sizeof(line), "Cornell Box (%s)  light=%s", mode,
                  gLightMode == 0 ? "sun" : "point");
    gHudText.OutText(line, 0, x, y, 0, 0, 0xFFFFFFFF, GNg::eBLEND_NORMAL);
    y += lineHeight;

    std::snprintf(line, sizeof(line), "camera pos(%.2f %.2f %.2f) fwd(%.2f %.2f %.2f)",
                  pos.x, pos.y, pos.z, forward.x, forward.y, forward.z);
    gHudText.OutText(line, 0, x, y, 0, 0, 0xFFCCCCCC, GNg::eBLEND_NORMAL);
    y += lineHeight;

    std::snprintf(line, sizeof(line), "GI: IBL=%s  SSAO=%s  SSGI=%.2f",
                  gEngineGiOn ? "on" : "off", gSsaoOn ? "on" : "off",
                  static_cast<double>(gSsgiStrength));
    gHudText.OutText(line, 0, x, y, 0, 0, 0xFF99DDFF, GNg::eBLEND_NORMAL);
    y += lineHeight;

    std::snprintf(line, sizeof(line), "1-4 pose  WASD/QE move  drag look  F orbit");
    gHudText.OutText(line, 0, x, y, 0, 0, 0xFFFFDD88, GNg::eBLEND_NORMAL);
    y += lineHeight;

    std::snprintf(line, sizeof(line), "T CPU  Y GPU  U HW RT  O SSAO  P SSGI  I IBL  K light  R reset  H hud  F12 shot");
    gHudText.OutText(line, 0, x, y, 0, 0, 0xFFFFDD88, GNg::eBLEND_NORMAL);
}

} // namespace Cornell
