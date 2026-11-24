// tests/editorGizmo/main.cpp — 编辑器基础控件演示 → 逐帧 PNG（电影渲染）
//
// 展示 `GN_Gizmo.h` 提供的六件基础控件，全部通过一个入口（`GN_VkDrawEditLines`）绘制：
//   1. 全局网格（主/次线 + 轴语义色）
//   2. 全局坐标轴（三根带箭头的轴线）
//   3. 局部对象的**移动 / 旋转 / 缩放**控件（Ctrl 顺序切换，含 hover 高亮与拖拽）
//   4. 包围盒（12 棱 + 角标）
//   5. 线框显示（三角网去重边）
//   6. 3D 样条曲线（Catmull-Rom / 闭环 / 控制点手柄）
//
// 时间轴（12 秒 @30fps）：
//   0.0–3.0s  网格 + 坐标轴 + 物体实体 + 包围盒
//   3.0–6.0s  切到线框显示，包围盒角标亮起
//   6.0–9.0s  移动控件：拖拽 X 轴，物体沿 X 移动（控件跟随）
//   9.0–10.5s 旋转控件：拖拽 Y 环，物体绕 Y 自转
//  10.5–12.0s 缩放控件：拖拽 Y 轴，物体拉伸；同时样条曲线逐段生长
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
int gSkipFrames = 0; // 快进（只推进不落盘），抽查某个阶段用
const char *gOutDir = ".local/video/gizmo-frames";
bool gVerbose = false;

// 时间轴分段（秒）
const float kSplineGrowBegin = 0.4f; // 样条开始生长
const float kSplineGrowEnd = 2.2f; // 样条长满
const float kPhaseWireBegin = 3.0f; // 切线框显示
const float kPhaseWireEnd = 6.0f;
const float kPhaseMoveEnd = 9.0f;
const float kPhaseRotateEnd = 10.5f;

// ── 场景 ──
GN3d::cLIGHT *gLight = nullptr;
GN3d::cCAMERA gCamera;
GN3d::cMODEL *gObject = nullptr; // 被编辑的对象（一个箱子）
GN3d::cMODEL *gGround = nullptr;
std::vector<GN3d::cMODEL *> gStatics; // 背景道具

// 受控件操作的对象状态
GNg::sVEC3 gObjectPos = {0.0f, 1.0f, 0.0f};
GNg::sQUAT gObjectRot;
GNg::sVEC3 gObjectScale = {1.0f, 1.0f, 1.0f};

// 样条控制点（世界坐标）。刻意有 y 起伏，才看得出是 3D 曲线而不是平面折线。
// 放在物体**后方**（z ≈ -1.6~-2.6）的空旷带：
//   * 太靠前（z 为正）会横穿镜头与控件之间，把控件和物体全划一遍；
//   * 太靠后（z < -3）会被后排柱子截断。
// 这同时让它成为背景内容 —— 3D 曲线该有的"有起伏、有前后"也就看出来了。
const GNg::sVEC3 kSplinePoints[7] = {{-5.4f, 0.35f, -1.6f}, {-3.5f, 1.80f, -2.5f},
                                     {-1.4f, 0.55f, -1.5f}, {0.8f, 2.35f, -2.6f},
                                     {2.9f, 0.80f, -1.6f}, {4.6f, 1.95f, -2.5f},
                                     {6.0f, 0.30f, -1.7f}};
const uint32_t kSplinePointCount = 7;

// 两套线段缓冲（复用，避免每帧分配）。**必须分开**：
//   * gSceneLines  —— 网格 / 样条 / 线框 / 包围盒：属于场景内容，要参与深度测试
//                     （柱子该挡住网格、物体该挡住样条）；
//   * gOverlayLines —— 只有变换控件：挂在物体中心、一半在物体内部，必须画在最上层。
// 一把梭全用 overlay 会让网格"浮"在柱子前面（第一版就是这样，每根柱子都被网格划过）。
std::vector<GN3d::sEDIT_LINE> gSceneLines;
std::vector<GN3d::sEDIT_LINE> gOverlayLines;
float gTime = 0.0f;

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

// ── 工具 ──
float smoothstep01(float x) {
    if (x <= 0.0f) return 0.0f;
    if (x >= 1.0f) return 1.0f;
    return x * x * (3.0f - 2.0f * x);
}
float phaseLocal(float t, float begin, float end) {
    if (!(end > begin)) return 0.0f;
    return smoothstep01((t - begin) / (end - begin));
}

void applyTransform() {
    gObject->SetPosition(gObjectPos.x, gObjectPos.y, gObjectPos.z);
    gObject->SetRotation(gObjectRot);
    gObject->SetScale(gObjectScale.x, gObjectScale.y, gObjectScale.z);
    gObject->computeWorldAABB();
}

void buildScene() {
    gGround = GN3d::CreateCubeModel(1.0f, 0.115f, 0.12f, 0.135f);
    gGround->SetScale(24.0f, 0.1f, 24.0f);
    gGround->SetPosition(0.0f, -0.05f, 0.0f);
    gGround->computeWorldAABB();

    // 被编辑的对象：一个略扁的箱子（能看清缩放与旋转）
    gObject = GN3d::CreateCubeModel(1.0f, 0.62f, 0.60f, 0.58f);
    applyTransform();

    // 背景道具：几个柱子，提供空间参照（看得出控件在动）
    const float positions[6][3] = {{-4.6f, 0.0f, -3.4f}, {4.6f, 0.0f, -3.4f},
                                   {-4.6f, 0.0f, 4.4f}, {4.6f, 0.0f, 4.4f},
                                   {0.0f, 0.0f, -4.6f}, {0.0f, 0.0f, 5.6f}};
    for (int i = 0; i < 6; ++i) {
        GN3d::cMODEL *pillar = GN3d::CreateCubeModel(1.0f, 0.20f, 0.21f, 0.24f);
        pillar->SetScale(0.42f, 1.7f, 0.42f);
        pillar->SetPosition(positions[i][0], 0.85f, positions[i][2]);
        pillar->computeWorldAABB();
        gStatics.push_back(pillar);
    }
}

void initLights() {
    gLight = new GN3d::cLIGHT();
    gLight->AddDirectional(GNg::sVEC3{0.40f, -0.78f, -0.48f}, GNg::sVEC3{1.0f, 0.98f, 0.94f},
                           1.9f, false);
    const GNg::sVEC3 fillDir = {-0.6f, -0.3f, 0.65f};
    const GNg::sVEC3 fillColor = {0.45f, 0.55f, 0.80f};
    gLight->AddDirectional(fillDir, fillColor, 0.35f, false);
    const GNg::sVEC3 ambient = {0.24f, 0.26f, 0.31f};
    gLight->SetAmbient(ambient, 1.0f);
}

// ── 相机 ──
bool buildCamera(float time, float aspect, GNg::sMAT44 &viewProj, GNg::sMAT44 &view,
                 GNg::sVEC3 &eye) {
    // 缓慢环绕 + 略微俯视：一个轴测风格的视角，网格和控件都看得清
    const float yaw = 0.62f + time * 0.055f;
    // 控件阶段（6 s 起）把相机拉近：12 m 外一个 1.6 m 的控件只有几十像素，
    // 细节（箭头/环/高亮）完全看不出来，演示就失去意义了。
    float distance = 12.5f - std::fmin(time * 0.12f, 1.6f);
    const float dolly = smoothstep01((time - kPhaseWireEnd + 0.6f) / 1.4f);
    distance -= 4.2f * dolly;
    const float pitch = 0.38f - 0.10f * dolly;
    // 焦点跟着物体走（物体在这段里沿 +X 移动），否则控件会跑出画面中心
    const GNg::sVEC3 focus = {0.4f + 0.55f * (gObjectPos.x + 1.6f) * 0.5f, 1.05f, 0.75f};
    gCamera.SetOrbit(focus, distance, yaw, pitch);
    gCamera.SetFov(44.0f);
    gCamera.SetNF(0.1f, 120.0f);
    GNg::sMAT44 projection;
    if (!gCamera.BuildViewProj(aspect, &view, &projection, &viewProj)) return false;
    eye = gCamera.GetCoordV();
    return true;
}

// 由物体当前位置推出控件描述。
GN3d::sGIZMO_DESC makeGizmo(GNg::sVEC3 cameraPos, float fovYRad, GN3d::eGIZMO_MODE mode,
                            GN3d::eGIZMO_AXIS highlight, bool localSpace) {
    GN3d::sGIZMO_DESC desc;
    desc.aMode = mode;
    // 控件挂在物体 AABB 中心（比原点更符合"选中物体"的直觉）
    const GNg::sVEC3 center = gObject->aWorldAABB.Center();
    desc.aOrigin = center;
    desc.aLocalSpace = localSpace;
    GN3d::gizmoBasisFromRotation(gObjectRot, &desc.aBasis);
    const float dx = cameraPos.x - center.x;
    const float dy = cameraPos.y - center.y;
    const float dz = cameraPos.z - center.z;
    const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
    // 尺寸取"屏幕恒定"与"大于物体"的较大值。只用屏幕尺寸时，1 m 的物体配上 1.26 m 的
    // 控件 ⇒ 箭头从中心伸出去只露出 0.13 m，几乎整根埋在物体里（实测画面里只剩箭头尖）。
    const float byScreen = GN3d::gizmoScreenSize(distance, fovYRad, 0.21f);
    const GNg::sVEC3 extent = gObject->aWorldAABB.Extent();
    const float radius =
        std::sqrt(extent.x * extent.x + extent.y * extent.y + extent.z * extent.z);
    const float byObject = radius * 1.75f;
    desc.aSize = byScreen > byObject ? byScreen : byObject;
    desc.aHighlight = highlight;
    return desc;
}

} // namespace

// ── 引擎回调 ──
extern "C" {

GN_CALL void AppInit() {
    gCamera.Init();
    buildScene();
    initLights();
    // 编辑器风格：均匀环境光（不用天空 IBL，否则网格会被天光洗淡）
    GNg::GN_VkSetEnvironmentLighting(GN3d::eENV_LIGHTING_INDOOR_UNIFORM, 0.18f);
    GNg::GN_VkSetExposure(-0.10f);
    GNg::GN_VkSetBloom(0.30f, 1.10f, 0.30f); // 阈值抬高：不要线框发光
    std::fprintf(stderr, "[editorGizmo] 对象 + %u 个柱子，样条 %u 个控制点\n",
                 (unsigned)gStatics.size(), kSplinePointCount);
}

GN_CALL void AppUnInit() {
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
    const float aspect = (float)gWidth / (float)gHeight;

    // ── 1) 时间轴：驱动物体状态 ──
    const float movePhase = phaseLocal(t, kPhaseWireEnd, kPhaseMoveEnd);
    const float rotatePhase = phaseLocal(t, kPhaseMoveEnd, kPhaseRotateEnd);
    const float scalePhase = phaseLocal(t, kPhaseRotateEnd, 12.0f);

    // 移动：沿 +X 走 3.2 m（控件跟着物体走）
    gObjectPos.x = -1.6f + 3.2f * movePhase;
    // 旋转：绕 Y 转 100°
    {
        const float angle = 1.7453293f * rotatePhase;
        gObjectRot = GNg::sQUAT(0.0f, std::sin(angle * 0.5f), 0.0f, std::cos(angle * 0.5f));
    }
    // 缩放：Y 方向拉到 2.2 倍
    gObjectScale.y = 1.0f + 1.2f * scalePhase;
    applyTransform();

    // ── 2) 相机 ──
    GNg::sMAT44 view;
    GNg::sMAT44 viewProj;
    GNg::sVEC3 eye;
    if (!buildCamera(t, aspect, viewProj, view, eye)) return;
    const float fovYRad = gCamera.aFovY * GN3d::CAMERA_DEG_TO_RAD;

    // 3D pass
    GNg::cRENDER_FRAME frame;
    GNg::sRENDER_FRAME_DESC desc;
    desc.aViewProj = &viewProj;
    desc.aCameraPos = &eye;
    desc.aLightUBO = gLight->GetUBO();
    desc.aLightUBOSize = sizeof(GN3d::sLIGHT_UBO);
    desc.aDrawSkybox = false;
    if (!frame.Begin(desc)) return;

    // ── 3) 实体几何 ──
    gGround->DrawPBRSorted(eye);
    for (size_t i = 0; i < gStatics.size(); ++i) gStatics[i]->DrawPBRSorted(eye);

    // 线框显示只在那一段开启。控件阶段回到实体：线框 + 包围盒 + 控件三样叠在一起
    // 完全看不清哪根是控件（第一版就是这样，旋转环和线框混成一团）。
    const bool wireframeMode = (t >= kPhaseWireBegin && t < kPhaseWireEnd);
    if (!wireframeMode) gObject->DrawPBRSorted(eye);

    // ── 4) 控件线段（全部走 GN_VkDrawEditLines，每线自带颜色）──
    gSceneLines.clear();
    gOverlayLines.clear();

    // 4a) 全局网格
    {
        GN3d::sGRID_DESC grid;
        grid.aCenter = GNg::sVEC3{0.0f, 0.0f, 0.0f};
        grid.aCellSize = 1.0f;
        grid.aHalfCells = 10;
        grid.aMajorEvery = 5;
        grid.aDrawAxes = true;
        grid.aFade = true;
        GN3d::buildGridLines(grid, gSceneLines);
    }

    // 4b) 全局坐标轴（原点处，稍长一点）
    GN3d::buildAxesLines(GNg::sVEC3{0.0f, 0.02f, 0.0f}, 2.6f, gSceneLines);

    // 4c) 包围盒（物体 AABB）
    {
        uint32_t boxColor = GN3d::gizmoColor::kBoundingBox;
        if (t >= kPhaseWireBegin && t < kPhaseWireEnd) {
            boxColor = GN3d::gizmoColor::kHighlight; // 线框阶段把包围盒强调出来
        } else if (t >= kPhaseWireEnd) {
            boxColor = 0xFF6A6248; // 控件阶段压暗：包围盒只是参照物，不该和控件抢
        }
        // padding：包围盒棱与物体表面同深度 → 深度测试会把线判为"更远"而整框消失。
        // 外扩 1.5% 对角长让线浮在表面之外（见 buildBoxLines 的注释）。
        const GNg::sVEC3 boxCenter = gObject->aWorldAABB.Center();
        const float boxDistance = std::sqrt((eye.x - boxCenter.x) * (eye.x - boxCenter.x) +
                                            (eye.y - boxCenter.y) * (eye.y - boxCenter.y) +
                                            (eye.z - boxCenter.z) * (eye.z - boxCenter.z));
        const float pad = GN3d::suggestedBoxPadding(gObject->aWorldAABB, boxDistance,
                                                    (float)gHeight, fovYRad, 2.5f);
        GN3d::buildBoxLines(gObject->aWorldAABB, boxColor, gSceneLines, pad);
        GN3d::buildBoxCornerMarks(gObject->aWorldAABB, 0.18f, boxColor, gSceneLines, pad);
    }

    // 4d) 线框显示：物体的三角网（去重边）+ 半透明实体打底
    if (wireframeMode) {
        const GN3d::sMESH &mesh = gObject->aMeshes[0];
        GN3d::buildIndexedWireframeLines(mesh.aVertices, mesh.aVertexCount, mesh.aIndices,
                                         mesh.aIndexCount, &gObject->aTransform,
                                         GN3d::gizmoColor::kWireframe, gSceneLines);
    }

    // 4e) 3D 样条曲线：先逐段生长，长满后一个标记沿曲线巡游（说明它是参数曲线）
    {
        GN3d::sSPLINE_DESC spline;
        spline.aMode = GN3d::eSPLINE_CATMULL_ROM;
        spline.aClosed = false;
        spline.aSamplesPerSegment = 12;
        spline.aTension = 0.5f;
        std::vector<GNg::sVEC3> samples;
        GN3d::sampleSpline(kSplinePoints, kSplinePointCount, spline, samples);

        // 生长：0.4s 开始、2.2s 长满
        const float grow = smoothstep01((t - kSplineGrowBegin) / (kSplineGrowEnd - kSplineGrowBegin));
        uint32_t limit = (uint32_t)(grow * (float)samples.size() + 0.999f);
        if (limit > (uint32_t)samples.size()) limit = (uint32_t)samples.size();
        for (uint32_t i = 1; i < limit; ++i) {
            gSceneLines.push_back(
                GN3d::sEDIT_LINE(samples[i - 1u], samples[i], GN3d::gizmoColor::kSpline));
        }

        // 巡游标记：t 在 0..1 之间匀速来回（用三角波，避免循环接缝处跳变）
        const float cycle = std::fmod(t * 0.34f, 2.0f);
        const float u = (cycle < 1.0f) ? cycle : (2.0f - cycle);
        const GNg::sVEC3 marker = GN3d::evaluateSpline(kSplinePoints, kSplinePointCount, spline, u);
        // 标记做成一个亮色小十字（线段少，不会糊住曲线）
        const float arm = 0.16f;
        const uint32_t markerColor = GN3d::gizmoColor::kHighlight;
        gSceneLines.push_back(GN3d::sEDIT_LINE(GNg::sVEC3{marker.x - arm, marker.y, marker.z},
                                               GNg::sVEC3{marker.x + arm, marker.y, marker.z},
                                               markerColor));
        gSceneLines.push_back(GN3d::sEDIT_LINE(GNg::sVEC3{marker.x, marker.y - arm, marker.z},
                                               GNg::sVEC3{marker.x, marker.y + arm, marker.z},
                                               markerColor));
        gSceneLines.push_back(GN3d::sEDIT_LINE(GNg::sVEC3{marker.x, marker.y, marker.z - arm},
                                               GNg::sVEC3{marker.x, marker.y, marker.z + arm},
                                               markerColor));

        // 控制点手柄（一直显示：让观众看到曲线"过点"）
        GN3d::buildSplineHandles(kSplinePoints, kSplinePointCount, 0.09f, true, gSceneLines);
    }

    // 4f) 变换控件
    {
        // 按时间轴切换模式，并假装鼠标悬停在正在拖拽的那根轴上
        GN3d::eGIZMO_MODE mode = GN3d::eGIZMO_TRANSLATE;
        GN3d::eGIZMO_AXIS highlight = GN3d::eGIZMO_AXIS_NONE;
        bool localSpace = false;
        if (t >= kPhaseWireEnd && t < kPhaseMoveEnd) {
            mode = GN3d::eGIZMO_TRANSLATE;
            highlight = GN3d::eGIZMO_AXIS_X; // 正在拖 X
            localSpace = false;
        } else if (t >= kPhaseMoveEnd && t < kPhaseRotateEnd) {
            mode = GN3d::eGIZMO_ROTATE;
            highlight = GN3d::eGIZMO_AXIS_Y; // 正在转 Y 环
            localSpace = true; // 旋转控件用局部空间更直观
        } else if (t >= kPhaseRotateEnd) {
            mode = GN3d::eGIZMO_SCALE;
            highlight = GN3d::eGIZMO_AXIS_Y; // 正在拉 Y
            localSpace = true;
        }
        if (t >= kPhaseWireBegin) {
            const GN3d::sGIZMO_DESC gizmo = makeGizmo(eye, fovYRad, mode, highlight, localSpace);
            GN3d::buildGizmoLines(gizmo, gOverlayLines);
        }
    }

    if (gVerbose && (int)(gTime * 30.0f) % 30 == 0) {
        const GN3d::sAABB &box = gObject->aWorldAABB;
        std::fprintf(stderr,
                     "[dbg] t=%.1f lines=%u aabb valid=%d min=(%.2f,%.2f,%.2f) "
                     "max=(%.2f,%.2f,%.2f)\n",
                     gTime, (unsigned)(gSceneLines.size() + gOverlayLines.size()), box.IsValid() ? 1 : 0,
                     box.aMin.x, box.aMin.y,
                     box.aMin.z, box.aMax.x, box.aMax.y, box.aMax.z);
    }
    // 场景线（网格/样条/线框/包围盒）：参与深度测试，被实体正确遮挡
    if (!gSceneLines.empty()) {
        GNg::GN_VkDrawEditLines(&gSceneLines[0], (uint32_t)gSceneLines.size());
    }
    // 控件：关深度测试，永远在最上层（DCC 的默认行为）
    if (!gOverlayLines.empty()) {
        const uint32_t drawn =
            GNg::GN_VkDrawEditLinesOverlay(&gOverlayLines[0], (uint32_t)gOverlayLines.size());
        if (gVerbose && drawn == 0) {
            std::fprintf(stderr, "[editorGizmo] 控件绘制被拒绝（pass 内？%d）\n",
                         GNg::GN_VkIsIn3DPass() ? 1 : 0);
        }
    }

    frame.End();

    gTime += 1.0f / 30.0f;
}

} // extern "C"

namespace {

void onFrame(int index) {
    int w = 0;
    int h = 0;
    int size = 0;
    unsigned char *rgba = GN_GrabFrame(&w, &h, &size);
    if (rgba == nullptr || w <= 0 || h <= 0) {
        std::fprintf(stderr, "[editorGizmo] 第 %d 帧回读失败\n", index);
        return;
    }
    char path[512];
    std::snprintf(path, sizeof(path), "%s/frame_%05d.png", gOutDir, index);
    if (!writePng(path, rgba, w, h)) std::fprintf(stderr, "[editorGizmo] 写 %s 失败\n", path);
    GN_GrabFrameFree(rgba);
    if (index % 30 == 0) std::fprintf(stderr, "[editorGizmo] 帧 %d/%d\n", index, gFrames);
}

} // namespace

int main(int argc, char **argv) {
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--out") == 0 && i + 1 < argc) gOutDir = argv[++i];
        else if (std::strcmp(argv[i], "--frames") == 0 && i + 1 < argc) gFrames = std::atoi(argv[++i]);
        else if (std::strcmp(argv[i], "--size") == 0 && i + 2 < argc) {
            gWidth = std::atoi(argv[++i]);
            gHeight = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--start") == 0 && i + 1 < argc) {
            gSkipFrames = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--verbose") == 0) gVerbose = true;
    }
    if (gFrames < 1) gFrames = 1;

    char cmd[512];
    std::snprintf(cmd, sizeof(cmd), "mkdir -p '%s' && rm -f '%s'/*.png", gOutDir, gOutDir);
    if (std::system(cmd) != 0) std::fprintf(stderr, "[editorGizmo] 清理输出目录失败\n");

    std::fprintf(stderr, "[editorGizmo] %dx%d，%d 帧（%.1f s）→ %s\n", gWidth, gHeight, gFrames,
                 (float)gFrames / 30.0f, gOutDir);

    GN_InitLoadCallBackFunc(AppInit, AppUnInit);
    GN_InitLoopCallBackFunc(AppDisplay2D, AppDisplay3D, AppRun);
    if (!GN_StartRunHeadless((short)gWidth, (short)gHeight, gFrames + gSkipFrames,
                             [](int index) {
                                 if (index < gSkipFrames) return; // 只推进，不落盘
                                 onFrame(index - gSkipFrames);
                             })) {
        std::fprintf(stderr, "[editorGizmo] 初始化失败（无 Vulkan 设备？）\n");
        return 2;
    }
    std::printf("[editorGizmo] 完成：%d 帧 → %s\n", gFrames, gOutDir);
    return 0;
}
