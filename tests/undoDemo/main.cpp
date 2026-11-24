// tests/undoDemo/main.cpp — 多选 + 撤销/重做演示
//
// 展示本轮新增的两块能力（都在 cSCENE / cUNDO_STACK 里，纯 CPU）：
//   A3 选择集：单选、Ctrl 切换、主选中（Gizmo 挂点）、实体移出场景时同步清理；
//   A4 撤销栈：拖拽产生命令、Ctrl+Z 回退、Ctrl+Y 重做、新操作清 redo。
//
// 时间轴（12 秒 @30fps）：
//   0.0–2.0s   单击选中 A（包围盒高亮 + Gizmo 挂在 A 上）
//   2.0–3.5s   Ctrl 点击 B、C → 三选（三个包围盒同时高亮）
//   3.5–6.0s   拖动 A（多选时只动主选中）→ 压入一条命令
//   6.0–7.0s   再拖一次 → 第二条命令
//   7.0–9.0s   **撤销两次**：A 依次退回中间位、起始位（画面上能看懂"历史回退"）
//   9.0–10.5s  **重做一次**：回到中间位
//   10.5–12.0s 新操作（拖 C）→ redo 栈被清空，重做不再可用
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
int gFrames = 360; // 12 s @ 30 fps
const char *gOutDir = ".local/video/gizmo-input-frames";
bool gVerbose = false;
float gTime = 0.0f;

// 时间轴
const float kPhaseSelectEnd = 2.0f;    // 单选 A
const float kPhaseMultiEnd = 3.5f;     // Ctrl 多选
const float kPhaseDrag1End = 6.0f;     // 第一次拖拽（→ 命令 1）
const float kPhaseDrag2End = 7.0f;     // 第二次拖拽（→ 命令 2）
const float kPhaseUndoEnd = 9.0f;      // 撤销两次
const float kPhaseRedoEnd = 10.5f;     // 重做一次
// 之后：新操作拖 C，清空 redo

// 场景
GN3d::cLIGHT *gLight = nullptr;
GN3d::cCAMERA gCamera;
GN3d::cMODEL *gObject = nullptr;
GN3d::cMODEL *gGround = nullptr;
std::vector<GN3d::cMODEL *> gStatics;

// 物体 TRS（拖拽直接写主选中的那个）
GNg::sVEC3 gPosition = {0.0f, 1.0f, 0.0f};
GNg::sQUAT gRotation;
GNg::sVEC3 gScale = {1.0f, 1.0f, 1.0f};

// 场景 + 选择 + 撤销（本轮的主角）
GN3d::cSCENE gScene;
GN3d::cUNDO_STACK gUndo;

// 交互
GN3d::cGIZMO_INTERACTION gInteraction;
GN3d::sGIZMO_DESC gGizmo;
GN3d::eGIZMO_AXIS gHoverAxis = GN3d::eGIZMO_AXIS_NONE;
int gBeginPhase = -1;

// 鼠标轨迹缓存（像素空间，见 gizmoInput 的注释：不能在 world space 算）
float gPressPixelX = 0.0f;
float gPressPixelY = 0.0f;
float gEndPixelX = 0.0f;
float gEndPixelY = 0.0f;
bool gPixelsReady = false;
bool gDragging = false;

// 拖拽开始时的快照（拖拽结束时用来压栈）
GN3d::sTRANSFORM_STATE gDragStartState;

std::vector<GN3d::sEDIT_LINE> gSceneLines;
std::vector<GN3d::sEDIT_LINE> gOverlayLines;

// 相机跟随
GN3d::cENTITY gCameraAnchor;
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

// 把主选中对象的位置同步到 gPosition（切换选中时用）
void syncFromPrimary() {
    GN3d::cENTITY *primary = gScene.Primary();
    if (primary == nullptr || primary->aMesh.Model() == nullptr) return;
    gObject = primary->aMesh.Model();
    gPosition = gObject->aPosition;
    gRotation = gObject->aRotation;
    gScale = gObject->aScale;
}

// 三个可编辑对象（A/B/C），拖拽与选择都在它们之间发生
const int kEditableCount = 3;
GN3d::cMODEL *gEditable[kEditableCount] = {nullptr, nullptr, nullptr};
GN3d::cENTITY gEditableEntity[kEditableCount];
// 间距 3.6、对象边长 1.0：拖拽总位移 2.4 之后 A 的右边缘仍在 B 的左边缘之外
// （第一版间距 2.6 + 位移 3.6 直接把 A 拖进了 B 里，画面像两个盒子粘在一起）
const GNg::sVEC3 kEditableStart[kEditableCount] = {{-3.6f, 1.0f, 0.0f},
                                                   {0.0f, 1.0f, 0.0f},
                                                   {3.6f, 1.0f, 0.0f}};
const float kEditableTint[kEditableCount][3] = {{0.72f, 0.46f, 0.38f},
                                                {0.48f, 0.66f, 0.44f},
                                                {0.42f, 0.52f, 0.74f}};

void buildScene() {
    gGround = GN3d::CreateCubeModel(1.0f, 0.115f, 0.12f, 0.135f);
    gGround->SetScale(26.0f, 0.1f, 26.0f);
    gGround->SetPosition(0.0f, -0.05f, 0.0f);
    gGround->computeWorldAABB();

    for (int i = 0; i < kEditableCount; ++i) {
        gEditable[i] = GN3d::CreateCubeModel(1.0f, kEditableTint[i][0], kEditableTint[i][1],
                                             kEditableTint[i][2]);
        gEditable[i]->SetPosition(kEditableStart[i].x, kEditableStart[i].y, kEditableStart[i].z);
        gEditable[i]->computeWorldAABB();
        gEditableEntity[i].aMesh.SetModel(gEditable[i]);
        gEditableEntity[i].aId = (uint32_t)(i + 1);
        gScene.Add(&gEditableEntity[i]);
    }
    // gObject 仍然指向 A：拖拽/撤销逻辑沿用（只操作"当前主选中"）
    gObject = gEditable[0];
    gPosition = kEditableStart[0];
    applyTransform();

    const float positions[6][3] = {{-5.6f, 0.0f, -4.0f}, {5.6f, 0.0f, -4.0f},
                                   {-5.6f, 0.0f, 5.2f}, {5.6f, 0.0f, 5.2f},
                                   {0.0f, 0.0f, -5.8f}, {0.0f, 0.0f, 6.6f}};
    for (int i = 0; i < 6; ++i) {
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

// 由当前 TRS 构造控件描述（Gizmo 挂在主选中对象上）
GN3d::sGIZMO_DESC makeGizmo(GN3d::eGIZMO_MODE mode, const GNg::sVEC3 &cameraPos, float fovYRad,
                            GN3d::eGIZMO_AXIS highlight) {
    GN3d::sGIZMO_DESC desc;
    desc.aMode = mode;
    desc.aOrigin = gObject->aWorldAABB.Center();
    desc.aLocalSpace = false;
    GN3d::gizmoBasisFromRotation(gRotation, &desc.aBasis);
    const float dx = cameraPos.x - desc.aOrigin.x;
    const float dy = cameraPos.y - desc.aOrigin.y;
    const float dz = cameraPos.z - desc.aOrigin.z;
    const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
    const float byScreen = GN3d::gizmoScreenSize(distance, fovYRad, 0.19f);
    const GNg::sVEC3 extent = gObject->aWorldAABB.Extent();
    const float radius =
        std::sqrt(extent.x * extent.x + extent.y * extent.y + extent.z * extent.z);
    desc.aSize = byScreen > radius * 1.7f ? byScreen : radius * 1.7f;
    desc.aHighlight = highlight;
    return desc;
}

// 世界 → 屏幕像素（**不翻转 y**：Vulkan 的 NDC y 与像素同向）
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

// 相机：跟随主选中对象
bool buildCamera(float dt, GNg::sMAT44 &viewProj, GNg::sMAT44 &view, GNg::sVEC3 &eye) {
    if (!gDragging) {
        GN3d::sCAMERA_TARGET target;
        if (gCameraAnchor.BuildCameraTarget(&target, 0.30f)) gCamera.SetTarget(target, 0.08f);
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

    // 初始：单选 A
    gScene.Select(&gEditableEntity[0]);
    syncFromPrimary();

    gCamera.SetOrbit(GNg::sVEC3{gPosition.x + 1.2f, 1.0f, 0.0f}, 15.0f, 0.70f, 0.44f);
    gCamera.SetFov(44.0f);
    gCamera.SetNF(0.1f, 120.0f);
    gCameraAnchor.aMesh.SetModel(gObject);

    GNg::GN_VkSetEnvironmentLighting(GN3d::eENV_LIGHTING_INDOOR_UNIFORM, 0.20f);
    GNg::GN_VkSetExposure(-0.10f);
    GNg::GN_VkSetBloom(0.28f, 1.15f, 0.30f);
    std::fprintf(stderr, "[undoDemo] 可编辑对象 %d 个，柱子 %u 个\n", kEditableCount,
                 (unsigned)gStatics.size());
}

GN_CALL void AppUnInit() {
    gCameraAnchor.aMesh.SetModel(nullptr);
    for (int i = 0; i < kEditableCount; ++i) {
        gEditableEntity[i].aMesh.SetModel(nullptr);
        delete gEditable[i];
        gEditable[i] = nullptr;
    }
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

    // ── 1) 时间轴：决定本帧处于哪个阶段 ──
    int phase = 0;
    if (t < kPhaseSelectEnd) phase = 0;       // 单选 A
    else if (t < kPhaseMultiEnd) phase = 1;   // Ctrl 多选 B、C
    else if (t < kPhaseDrag1End) phase = 2;   // 拖 A（命令 1）
    else if (t < kPhaseDrag2End) phase = 3;   // 再拖 A（命令 2）
    else if (t < kPhaseUndoEnd) phase = 4;    // 撤销两次
    else if (t < kPhaseRedoEnd) phase = 5;    // 重做一次
    else phase = 6;                          // 新操作（清 redo）

    // ── 2) 选择集：按阶段一次性施加 ──
    // 用 gBeginPhase 作为"阶段进入一次"的标记（该变量在拖拽里也用，含义一致）
    static int sAppliedPhase = -1;
    if (sAppliedPhase != phase) {
        if (phase == 1) {
            // Ctrl 点击 B、C：在 A 的基础上追加
            gScene.AddToSelection(&gEditableEntity[1]);
            gScene.AddToSelection(&gEditableEntity[2]);
        } else if (phase == 6) {
            // 切到单选 C（新操作的对象），并真的拖它一下
            gScene.Select(&gEditableEntity[2]);
            syncFromPrimary();
        }
        sAppliedPhase = phase;
    }

    // ── 3) 撤销 / 重做：在阶段开始的那一刻执行一次 ──
    static int sUndoDone = 0;
    static int sRedoDone = 0;
    if (phase == 4) {
        // 前 1 秒撤销第 2 条命令，后 1 秒撤销第 1 条
        const int want = (t - kPhaseDrag2End > 1.0f) ? 2 : 1;
        while (sUndoDone < want && gUndo.Undo()) {
            ++sUndoDone;
            syncFromPrimary(); // 撤销后用对象的真实位置刷新 gPosition
        }
    }
    if (phase == 5 && sRedoDone == 0) {
        if (gUndo.Redo()) {
            ++sRedoDone;
            syncFromPrimary();
        }
    }

    // ── 4) 相机 ──
    GNg::sMAT44 view;
    GNg::sMAT44 viewProj;
    GNg::sVEC3 eye;
    if (!buildCamera(dt, viewProj, view, eye)) return;
    const float fovYRad = gCamera.aFovY * GN3d::CAMERA_DEG_TO_RAD;

    // ── 5) 拖拽（阶段 2/3/6）──
    const bool dragPhase = (phase == 2 || phase == 3 || phase == 6);
    if (dragPhase) {
        gGizmo = makeGizmo(GN3d::eGIZMO_TRANSLATE, eye, fovYRad, GN3d::eGIZMO_AXIS_X);

        // 每次进入新的拖拽阶段：重新算两端像素
        if (gBeginPhase != phase) {
            gPixelsReady = false;
            if (gInteraction.Active()) gInteraction.End();
            gInteraction.End();
        }

        float progress = 0.0f;
        if (phase == 2) progress = phase01(t, kPhaseMultiEnd + 0.15f, kPhaseDrag1End - 0.4f);
        else if (phase == 3) progress = phase01(t, kPhaseDrag1End + 0.1f, kPhaseDrag2End - 0.2f);
        else progress = phase01(t, kPhaseRedoEnd + 0.2f, 11.9f);

        if (!gPixelsReady) {
            const float size = gGizmo.aSize;
            const GNg::sVEC3 o = gGizmo.aOrigin;
            const float travel = (phase == 2) ? 1.3f : ((phase == 3) ? 1.1f : 1.2f);
            projectToScreen(viewProj, GNg::sVEC3{o.x + 0.7f * size, o.y, o.z}, &gPressPixelX,
                            &gPressPixelY);
            projectToScreen(viewProj, GNg::sVEC3{o.x + 0.7f * size + travel, o.y, o.z},
                            &gEndPixelX, &gEndPixelY);
            gPixelsReady = true;
        }

        const float mouseX = gPressPixelX + (gEndPixelX - gPressPixelX) * progress;
        const float mouseY = gPressPixelY + (gEndPixelY - gPressPixelY) * progress;

        if (gBeginPhase != phase) {
            // 记录拖拽起点快照（撤销要用）
            gDragStartState = GN3d::captureTransform(*gObject);
            gInteraction.Begin(gGizmo, GN3d::sGIZMO_SNAP(), &gObject->aTransform, &gPosition,
                               &gRotation, &gScale, gPressPixelX, gPressPixelY, viewProj,
                               (float)gWidth, (float)gHeight);
            gBeginPhase = phase;
            gDragging = gInteraction.Active();
            if (gVerbose) {
                std::fprintf(stderr, "[undoDemo] phase%d drag begin axis=%d (%s)\n", phase,
                             (int)gInteraction.Axis(),
                             gInteraction.Active() ? "ok" : "MISS");
            }
        }

        if (gInteraction.Active()) {
            gInteraction.Drag(mouseX, mouseY, viewProj, (float)gWidth, (float)gHeight);
            gHoverAxis = gInteraction.Axis();
        }

        // 拖拽结束（进度到 1）→ 压栈
        if (progress >= 0.999f && gInteraction.Active()) {
            gInteraction.End();
            gDragging = false;
            // PushTransform 会自己读当前状态作为 aAfter，并在"无变化"时拒绝
            if (gUndo.PushTransform(gObject, gDragStartState, "拖拽")) {
                if (gVerbose) {
                    std::fprintf(stderr, "[undoDemo] phase%d pushed, undo=%u redo=%u\n", phase,
                                 gUndo.UndoDepth(), gUndo.RedoDepth());
                }
            }
        }
    } else {
        if (gInteraction.Active()) {
            gInteraction.End();
            gDragging = false;
        }
        gGizmo = makeGizmo(GN3d::eGIZMO_TRANSLATE, eye, fovYRad, gHoverAxis);
    }

    applyTransform();

    // ── 6) 3D pass ──
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
    for (int i = 0; i < kEditableCount; ++i) gEditable[i]->DrawPBRSorted(eye);

    // ── 7) 线段：网格 + 每个选中对象一个包围盒 + Gizmo ──
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
    for (int i = 0; i < kEditableCount; ++i) {
        GN3d::cMODEL *model = gEditable[i];
        const bool selected = gScene.IsSelected(&gEditableEntity[i]);
        const GNg::sVEC3 center = model->aWorldAABB.Center();
        const float distance = std::sqrt((eye.x - center.x) * (eye.x - center.x) +
                                        (eye.y - center.y) * (eye.y - center.y) +
                                        (eye.z - center.z) * (eye.z - center.z));
        const float pad =
            GN3d::suggestedBoxPadding(model->aWorldAABB, distance, (float)gHeight, fovYRad, 2.5f);
        // 选中的用高亮色，未选中的用低调色 —— 一眼看出多选状态
        const uint32_t color = selected ? GN3d::gizmoColor::kHighlight : 0xFF5A5548u;
        GN3d::buildBoxLines(model->aWorldAABB, color, gSceneLines, pad);
        if (selected) {
            GN3d::buildBoxCornerMarks(model->aWorldAABB, 0.18f, color, gSceneLines, pad);
        }
    }
    GN3d::buildGizmoLines(gGizmo, gOverlayLines);

    if (!gSceneLines.empty()) GNg::GN_VkDrawEditLines(&gSceneLines[0], (uint32_t)gSceneLines.size());
    if (!gOverlayLines.empty())
        GNg::GN_VkDrawEditLinesOverlay(&gOverlayLines[0], (uint32_t)gOverlayLines.size());

    frame.End();

    if (gVerbose && ((int)(t * 30.0f) % 15 == 0)) {
        std::fprintf(stderr,
                     "[dbg] t=%.2f phase=%d sel=%u primary=%d x=%.2f undo=%u redo=%u\n", t, phase,
                     gScene.SelectionCount(), (int)(gScene.Primary() == &gEditableEntity[0] ? 0
                                                    : (gScene.Primary() == &gEditableEntity[1] ? 1
                                                       : (gScene.Primary() == &gEditableEntity[2] ? 2 : -1))),
                     gPosition.x, gUndo.UndoDepth(), gUndo.RedoDepth());
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
        std::fprintf(stderr, "[undoDemo] 第 %d 帧回读失败\n", index);
        return;
    }
    char path[512];
    std::snprintf(path, sizeof(path), "%s/frame_%05d.png", gOutDir, index);
    if (!writePng(path, rgba, w, h)) std::fprintf(stderr, "[undoDemo] 写 %s 失败\n", path);
    GN_GrabFrameFree(rgba);
    if (index % 30 == 0) std::fprintf(stderr, "[undoDemo] 帧 %d/%d\n", index, gFrames);
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
    if (std::system(cmd) != 0) std::fprintf(stderr, "[undoDemo] 清理输出目录失败\n");

    std::fprintf(stderr, "[undoDemo] %dx%d，%d 帧（%.1f s）→ %s\n", gWidth, gHeight, gFrames,
                 (float)gFrames / 30.0f, gOutDir);

    GN_InitLoadCallBackFunc(AppInit, AppUnInit);
    GN_InitLoopCallBackFunc(AppDisplay2D, AppDisplay3D, AppRun);
    if (!GN_StartRunHeadless((short)gWidth, (short)gHeight, gFrames, onFrame)) {
        std::fprintf(stderr, "[undoDemo] 初始化失败（无 Vulkan 设备？）\n");
        return 2;
    }
    std::printf("[undoDemo] 完成：%d 帧 → %s\n", gFrames, gOutDir);
    return 0;
}
