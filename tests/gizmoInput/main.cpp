// tests/gizmoInput/main.cpp — 控件交互演示：真实鼠标路径 + 吸附 + 相机跟随
//
// 与 tests/editorGizmo 的区别：那条视频是**脚本直接改物体位置**（"假装拖拽"）。
// 这一条走完整链路，一步不跳：
//
//   模拟鼠标像素 → gizmoRayFromScreen → pickGizmo → cGIZMO_INTERACTION::Drag
//                → 写回 cMODEL 的 TRS → 相机跟随（BuildCameraTarget + SetTarget）
//
// 鼠标从哪来：无窗口渲染没有真鼠标，所以这里用**程序化轨迹**生成像素坐标
// （按下 → 沿轴移动 → 松开），但**像素到变换的那一段完全是真的**。
// 这也是为什么吸/不吸附能用肉眼看出差别：位置是算出来的，不是摆出来的。
//
// 时间轴（10 秒 @30fps）：
//   0.0–2.0s  空闲：网格 + 坐标轴 + 物体 + 包围盒；鼠标悬停在 X 轴上（高亮）
//   2.0–4.5s  **不吸附**拖 X 轴：物体停在 2.37 这种"手抖"的位置
//   4.5–7.0s  **吸附 1 m**拖 Z 轴：物体精确落在整数格上
//   7.0–8.5s  吸附 15° 转 Y 环
//   8.5–10.0s 吸附 0.25 倍缩 Y 轴
#include <GN.h>
#include "GN_Entity.h"
#include "GN_Gizmo.h"
#include "GN_GameClock.h"

#include <png.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {

int gWidth = 1280;
int gHeight = 720;
int gFrames = 300; // 10 s @ 30 fps
const char *gOutDir = ".local/video/gizmo-input-frames";
bool gVerbose = false;
float gTime = 0.0f;

// 时间轴
const float kPhaseHoverEnd = 2.0f;
const float kPhaseDragXEnd = 4.5f;
const float kPhaseDragZEnd = 7.0f;
const float kPhaseRotateEnd = 8.5f;

// 场景
GN3d::cLIGHT *gLight = nullptr;
GN3d::cCAMERA gCamera;
GN3d::cMODEL *gObject = nullptr;
GN3d::cMODEL *gGround = nullptr;
std::vector<GN3d::cMODEL *> gStatics;

// 物体 TRS（拖拽直接写这些）
GNg::sVEC3 gPosition = {0.0f, 1.0f, 0.0f};
GNg::sQUAT gRotation;
GNg::sVEC3 gScale = {1.0f, 1.0f, 1.0f};

// 交互
GN3d::cGIZMO_INTERACTION gInteraction;
GN3d::sGIZMO_DESC gGizmo;
GN3d::eGIZMO_AXIS gHoverAxis = GN3d::eGIZMO_AXIS_NONE;
// 每阶段只 Begin 一次
int gBeginPhase = -1;

// 鼠标轨迹的缓存：**阶段开始时算一次**按下/终点像素，之后在像素空间插值。
//
// 为什么不能每帧重算：第一版用 `projectToScreen(物体位置 + travel)` 每帧求目标像素，
// 而相机又跟随物体 ⇒ 物体一动、目标像素就飘，鼠标追着移动的目标跑，形成正反馈，
// 位置从 0 一路飞到 59 m（实测）。真实鼠标是**屏幕上的匀速移动**，与物体位置无关，
// 所以必须先把两端像素固定下来。
float gPressPixelX = 0.0f;
float gPressPixelY = 0.0f;
float gEndPixelX = 0.0f;
float gEndPixelY = 0.0f;
bool gPixelsReady = false;

// 拖拽期间不跟随：真实编辑器里拖拽时相机不动（否则手感会"追着跑"）
bool gDragging = false;

std::vector<GN3d::sEDIT_LINE> gSceneLines;
std::vector<GN3d::sEDIT_LINE> gOverlayLines;

// 相机跟随
GN3d::cENTITY gCameraAnchor; // 只用来 BuildCameraTarget（跟随物体）

// ── PNG ──
bool writePng(const char *path, const unsigned char *rgba, int width, int height) {
    std::FILE *file = std::fopen(path, "wb");
    if (file == nullptr) return false;
    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if (png == nullptr) {
        std::fclose(file);
        return false;
    }
    png_infop info = png_create_info_struct(png);
    if (info == nullptr) {
        png_destroy_write_struct(&png, nullptr);
        std::fclose(file);
        return false;
    }
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

float smoothstep01(float x) {
    if (x <= 0.0f) return 0.0f;
    if (x >= 1.0f) return 1.0f;
    return x * x * (3.0f - 2.0f * x);
}
float phase01(float t, float begin, float end) {
    if (!(end > begin)) return 0.0f;
    return smoothstep01((t - begin) / (end - begin));
}

void applyTransform() {
    gObject->SetPosition(gPosition.x, gPosition.y, gPosition.z);
    gObject->SetRotation(gRotation);
    gObject->SetScale(gScale.x, gScale.y, gScale.z);
    gObject->computeWorldAABB();
}

void buildScene() {
    gGround = GN3d::CreateCubeModel(1.0f, 0.115f, 0.12f, 0.135f);
    gGround->SetScale(26.0f, 0.1f, 26.0f);
    gGround->SetPosition(0.0f, -0.05f, 0.0f);
    gGround->computeWorldAABB();

    gObject = GN3d::CreateCubeModel(1.2f, 0.66f, 0.62f, 0.56f);
    applyTransform();

    const float positions[8][3] = {{-5.2f, 0.0f, -3.6f}, {5.2f, 0.0f, -3.6f},
                                   {-5.2f, 0.0f, 4.8f}, {5.2f, 0.0f, 4.8f},
                                   {0.0f, 0.0f, -5.2f}, {0.0f, 0.0f, 6.2f},
                                   {-2.8f, 0.0f, 6.8f}, {2.8f, 0.0f, -6.4f}};
    for (int i = 0; i < 8; ++i) {
        GN3d::cMODEL *pillar = GN3d::CreateCubeModel(1.0f, 0.19f, 0.20f, 0.23f);
        pillar->SetScale(0.38f, 1.5f, 0.38f);
        pillar->SetPosition(positions[i][0], 0.75f, positions[i][2]);
        pillar->computeWorldAABB();
        gStatics.push_back(pillar);
    }
}

void initLights() {
    gLight = new GN3d::cLIGHT();
    gLight->AddDirectional(GNg::sVEC3{0.40f, -0.78f, -0.48f}, GNg::sVEC3{1.0f, 0.98f, 0.94f},
                           1.9f, false);
    gLight->AddDirectional(GNg::sVEC3{-0.6f, -0.3f, 0.65f}, GNg::sVEC3{0.45f, 0.55f, 0.80f}, 0.35f,
                           false);
    gLight->SetAmbient(GNg::sVEC3{0.24f, 0.26f, 0.31f}, 1.0f);
}

// 由当前 TRS 构造控件描述
GN3d::sGIZMO_DESC makeGizmo(GN3d::eGIZMO_MODE mode, const GNg::sVEC3 &cameraPos, float fovYRad,
                            GN3d::eGIZMO_AXIS highlight) {
    GN3d::sGIZMO_DESC desc;
    desc.aMode = mode;
    desc.aOrigin = gObject->aWorldAABB.Center();
    desc.aLocalSpace = (mode != GN3d::eGIZMO_TRANSLATE);
    GN3d::gizmoBasisFromRotation(gRotation, &desc.aBasis);
    const float dx = cameraPos.x - desc.aOrigin.x;
    const float dy = cameraPos.y - desc.aOrigin.y;
    const float dz = cameraPos.z - desc.aOrigin.z;
    const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
    const float byScreen = GN3d::gizmoScreenSize(distance, fovYRad, 0.20f);
    const GNg::sVEC3 extent = gObject->aWorldAABB.Extent();
    const float radius =
        std::sqrt(extent.x * extent.x + extent.y * extent.y + extent.z * extent.z);
    desc.aSize = byScreen > radius * 1.7f ? byScreen : radius * 1.7f;
    desc.aHighlight = highlight;
    return desc;
}

// 把世界点投影到屏幕像素（与 gizmoRayFromScreen 互逆；**不翻转 y**）
void projectToScreen(const GNg::sMAT44 &viewProj, const GNg::sVEC3 &world, float *outX,
                     float *outY) {
    const float clipX = viewProj.m[0] * world.x + viewProj.m[4] * world.y +
                        viewProj.m[8] * world.z + viewProj.m[12];
    const float clipY = viewProj.m[1] * world.x + viewProj.m[5] * world.y +
                        viewProj.m[9] * world.z + viewProj.m[13];
    const float clipW = viewProj.m[3] * world.x + viewProj.m[7] * world.y +
                        viewProj.m[11] * world.z + viewProj.m[15];
    *outX = (clipX / clipW * 0.5f + 0.5f) * (float)gWidth;
    *outY = (clipY / clipW * 0.5f + 0.5f) * (float)gHeight;
}

// 相机：斜角俯视，物体移动时跟随（用 cCAMERA 的 follow 机制）
bool buildCamera(float dt, GNg::sMAT44 &viewProj, GNg::sMAT44 &view, GNg::sVEC3 &eye) {
    // 相机目标 ← 实体的世界 AABB 中心（走的是本轮新增的 BuildCameraTarget）
    if (!gDragging) {
        GN3d::sCAMERA_TARGET target;
        if (gCameraAnchor.BuildCameraTarget(&target, 0.35f)) {
            gCamera.SetTarget(target, 0.08f);
        }
    }
    gCamera.Update(dt);
    const float aspect = (float)gWidth / (float)gHeight;
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

    // 相机：初始斜角，之后靠 SetTarget 跟随物体
    gCamera.SetOrbit(GNg::sVEC3{0.0f, 1.0f, 0.0f}, 11.0f, 0.72f, 0.42f);
    gCamera.SetFov(44.0f);
    gCamera.SetNF(0.1f, 120.0f);

    // 相机锚点：把物体的模型借给它（BuildCameraTarget 读的是 aMesh.Model()）
    gCameraAnchor.aMesh.SetModel(gObject);

    GNg::GN_VkSetEnvironmentLighting(GN3d::eENV_LIGHTING_INDOOR_UNIFORM, 0.20f);
    GNg::GN_VkSetExposure(-0.10f);
    GNg::GN_VkSetBloom(0.28f, 1.15f, 0.30f);
    std::fprintf(stderr, "[gizmoInput] 对象 + %u 柱子，鼠标路径为程序化轨迹\n",
                 (unsigned)gStatics.size());
}

GN_CALL void AppUnInit() {
    gCameraAnchor.aMesh.SetModel(nullptr);
    delete gObject;
    gObject = nullptr;
    delete gGround;
    gGround = nullptr;
    for (size_t i = 0; i < gStatics.size(); ++i) delete gStatics[i];
    gStatics.clear();
    delete gLight;
    gLight = nullptr;
}

GN_CALL void AppRun(unsigned int nTime) { (void)nTime; }
GN_CALL void AppDisplay3D(unsigned int nTime) { (void)nTime; }

GN_CALL void AppDisplay2D(unsigned int nTime) {
    (void)nTime;
    if (gLight == nullptr) return;

    const float t = gTime;
    const float dt = 1.0f / 30.0f;

    // ── 1) 决定本阶段的交互模式 ──
    GN3d::eGIZMO_MODE mode = GN3d::eGIZMO_TRANSLATE;
    GN3d::sGIZMO_SNAP snap;
    int phase = 0; // 0=hover 1=dragX 2=dragZ 3=rotate 4=scale
    if (t < kPhaseHoverEnd) {
        phase = 0;
    } else if (t < kPhaseDragXEnd) {
        phase = 1;
    } else if (t < kPhaseDragZEnd) {
        phase = 2;
        snap.aTranslateStep = 1.0f; // 吸附 1 米
    } else if (t < kPhaseRotateEnd) {
        phase = 3;
        mode = GN3d::eGIZMO_ROTATE;
        snap.aRotateStepRad = 0.2617994f; // 15°
    } else {
        phase = 4;
        mode = GN3d::eGIZMO_SCALE;
        snap.aScaleStep = 0.25f;
    }

    // ── 2) 相机（先算，因为控件尺寸依赖相机距离）──
    GNg::sMAT44 view;
    GNg::sMAT44 viewProj;
    GNg::sVEC3 eye;
    if (!buildCamera(dt, viewProj, view, eye)) return;
    const float fovYRad = gCamera.aFovY * GN3d::CAMERA_DEG_TO_RAD;

    // ── 3) 控件描述 + 交互 ──
    gGizmo = makeGizmo(mode, eye, fovYRad, gHoverAxis);

    // 每个阶段的鼠标轨迹：**第一次进入该阶段时**把两端像素算好，之后在像素空间插值。
    if (gBeginPhase != phase) {
        gPixelsReady = false;
    }

    float mouseX = 0.0f;
    float mouseY = 0.0f;
    float progress = 0.0f;
    if (phase == 1) {
        progress = phase01(t, kPhaseHoverEnd + 0.2f, kPhaseDragXEnd - 0.5f);
    } else if (phase == 2) {
        progress = phase01(t, kPhaseDragXEnd + 0.2f, kPhaseDragZEnd - 0.5f);
    } else if (phase == 3) {
        progress = phase01(t, kPhaseDragZEnd + 0.2f, kPhaseRotateEnd - 0.4f);
    } else if (phase == 4) {
        progress = phase01(t, kPhaseRotateEnd + 0.2f, 9.9f);
    }

    // 第一次进入拖拽阶段：固定按下点与终点像素（此时物体还在起始位姿，控件尺寸也定了）
    if (!gPixelsReady && phase >= 1) {
        const float size = gGizmo.aSize;
        const GNg::sVEC3 o = gGizmo.aOrigin;
        if (phase == 1) {
            projectToScreen(viewProj, GNg::sVEC3{o.x + 0.7f * size, o.y, o.z}, &gPressPixelX,
                            &gPressPixelY);
            projectToScreen(viewProj, GNg::sVEC3{o.x + 0.7f * size + 2.37f, o.y, o.z},
                            &gEndPixelX, &gEndPixelY);
        } else if (phase == 2) {
            projectToScreen(viewProj, GNg::sVEC3{o.x, o.y, o.z + 0.7f * size}, &gPressPixelX,
                            &gPressPixelY);
            projectToScreen(viewProj, GNg::sVEC3{o.x, o.y, o.z + 0.7f * size + 3.4f},
                            &gEndPixelX, &gEndPixelY);
        } else if (phase == 3) {
            const float radius = size * 0.86f;
            // Y 环上 45° 处按下（x≠0 且 z≠0 ⇒ 只属于 Y 环）
            const float k = 0.70710678f;
            projectToScreen(viewProj,
                            GNg::sVEC3{o.x + radius * k, o.y, o.z - radius * k}, &gPressPixelX,
                            &gPressPixelY);
            const float endAngle = 1.2f;
            projectToScreen(viewProj,
                            GNg::sVEC3{o.x + std::sin(endAngle) * radius, o.y,
                                       o.z - std::cos(endAngle) * radius},
                            &gEndPixelX, &gEndPixelY);
        } else {
            projectToScreen(viewProj, GNg::sVEC3{o.x, o.y + 0.8f * size, o.z}, &gPressPixelX,
                            &gPressPixelY);
            projectToScreen(viewProj, GNg::sVEC3{o.x, o.y + 1.35f * size, o.z}, &gEndPixelX,
                            &gEndPixelY);
        }
        gPixelsReady = true;
    }

    if (phase >= 1) {
        // 像素空间插值：鼠标在屏幕上匀速移动（真实鼠标就是这样）
        mouseX = gPressPixelX + (gEndPixelX - gPressPixelX) * progress;
        mouseY = gPressPixelY + (gEndPixelY - gPressPixelY) * progress;
        if (gBeginPhase != phase) {
            if (gInteraction.Active()) gInteraction.End();
            gInteraction.Begin(gGizmo, snap, &gObject->aTransform, &gPosition, &gRotation,
                               &gScale, gPressPixelX, gPressPixelY, viewProj, (float)gWidth,
                               (float)gHeight);
            gBeginPhase = phase;
            gDragging = gInteraction.Active();
            if (gVerbose) {
                std::fprintf(stderr, "[gizmoInput] phase%d Begin axis=%d (%s)\n", phase,
                             (int)gInteraction.Axis(),
                             gInteraction.Active() ? "ok" : "MISS");
            }
        }
    }

    // 拖拽（真实路径）
    if (gInteraction.Active()) {
        gInteraction.Drag(mouseX, mouseY, viewProj, (float)gWidth, (float)gHeight);
        gHoverAxis = gInteraction.Axis();
    } else if (phase == 0) {
        gDragging = false;
        // 空闲时做 hover：鼠标在 X 轴附近
        float hx = 0.0f;
        float hy = 0.0f;
        projectToScreen(viewProj, GNg::sVEC3{gGizmo.aOrigin.x + 0.9f * gGizmo.aSize,
                                             gGizmo.aOrigin.y, gGizmo.aOrigin.z},
                        &hx, &hy);
        gHoverAxis = GN3d::cGIZMO_INTERACTION::Hover(&gGizmo, hx, hy, viewProj, (float)gWidth,
                                                    (float)gHeight);
    }

    applyTransform();

    // ── 4) 3D pass ──
    GNg::cRENDER_FRAME frame;
    GNg::sRENDER_FRAME_DESC desc;
    desc.aViewProj = &viewProj;
    desc.aCameraPos = &eye;
    desc.aLightUBO = gLight->GetUBO();
    desc.aLightUBOSize = sizeof(GN3d::sLIGHT_UBO);
    desc.aDrawSkybox = false;
    if (!frame.Begin(desc)) return;

    gGround->DrawPBRSorted(eye);
    for (size_t i = 0; i < gStatics.size(); ++i) gStatics[i]->DrawPBRSorted(eye);
    gObject->DrawPBRSorted(eye);

    // ── 5) 控件线段 ──
    gSceneLines.clear();
    gOverlayLines.clear();
    {
        GN3d::sGRID_DESC grid;
        grid.aCellSize = 1.0f;
        grid.aHalfCells = 11;
        grid.aMajorEvery = 5;
        GN3d::buildGridLines(grid, gSceneLines);
    }
    GN3d::buildAxesLines(GNg::sVEC3{0.0f, 0.02f, 0.0f}, 2.4f, gSceneLines);
    {
        const GNg::sVEC3 center = gObject->aWorldAABB.Center();
        const float boxDistance = std::sqrt((eye.x - center.x) * (eye.x - center.x) +
                                            (eye.y - center.y) * (eye.y - center.y) +
                                            (eye.z - center.z) * (eye.z - center.z));
        const float pad = GN3d::suggestedBoxPadding(gObject->aWorldAABB, boxDistance,
                                                    (float)gHeight, fovYRad, 2.5f);
        GN3d::buildBoxLines(gObject->aWorldAABB, GN3d::gizmoColor::kBoundingBox, gSceneLines, pad);
        GN3d::buildBoxCornerMarks(gObject->aWorldAABB, 0.18f, GN3d::gizmoColor::kBoundingBox,
                                 gSceneLines, pad);
    }
    GN3d::buildGizmoLines(gGizmo, gOverlayLines);

    if (!gSceneLines.empty()) GNg::GN_VkDrawEditLines(&gSceneLines[0], (uint32_t)gSceneLines.size());
    if (!gOverlayLines.empty())
        GNg::GN_VkDrawEditLinesOverlay(&gOverlayLines[0], (uint32_t)gOverlayLines.size());

    frame.End();

    if (gVerbose && ((int)(t * 30.0f) % 30 == 0)) {
        std::fprintf(stderr,
                     "[dbg] t=%.1f phase=%d pos=(%.2f,%.2f,%.2f) rotW=%.3f scaleY=%.2f "
                     "active=%d axis=%d hover=%d\n",
                     t, phase, gPosition.x, gPosition.y, gPosition.z, gRotation.w, gScale.y,
                     gInteraction.Active() ? 1 : 0, (int)gInteraction.Axis(), (int)gHoverAxis);
    }

    gTime += dt;
}

} // extern "C"

namespace {

void onFrame(int index) {
    int w = 0;
    int h = 0;
    int size = 0;
    unsigned char *rgba = GN_GrabFrame(&w, &h, &size);
    if (rgba == nullptr || w <= 0 || h <= 0) {
        std::fprintf(stderr, "[gizmoInput] 第 %d 帧回读失败\n", index);
        return;
    }
    char path[512];
    std::snprintf(path, sizeof(path), "%s/frame_%05d.png", gOutDir, index);
    if (!writePng(path, rgba, w, h)) std::fprintf(stderr, "[gizmoInput] 写 %s 失败\n", path);
    GN_GrabFrameFree(rgba);
    if (index % 30 == 0) std::fprintf(stderr, "[gizmoInput] 帧 %d/%d\n", index, gFrames);
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
    if (gFrames < 1) gFrames = 1;

    char cmd[512];
    std::snprintf(cmd, sizeof(cmd), "mkdir -p '%s' && rm -f '%s'/*.png", gOutDir, gOutDir);
    if (std::system(cmd) != 0) std::fprintf(stderr, "[gizmoInput] 清理输出目录失败\n");

    std::fprintf(stderr, "[gizmoInput] %dx%d，%d 帧（%.1f s）→ %s\n", gWidth, gHeight, gFrames,
                 (float)gFrames / 30.0f, gOutDir);

    GN_InitLoadCallBackFunc(AppInit, AppUnInit);
    GN_InitLoopCallBackFunc(AppDisplay2D, AppDisplay3D, AppRun);
    if (!GN_StartRunHeadless((short)gWidth, (short)gHeight, gFrames, onFrame)) {
        std::fprintf(stderr, "[gizmoInput] 初始化失败（无 Vulkan 设备？）\n");
        return 2;
    }
    std::printf("[gizmoInput] 完成：%d 帧 → %s\n", gFrames, gOutDir);
    return 0;
}
