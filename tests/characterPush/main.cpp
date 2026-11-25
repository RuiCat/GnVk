// tests/characterPush/main.cpp — C2 演示：角色走过一排箱子，把它们撞开
//
// 展示 cCHARACTER 接上 cPHYSICS_WORLD 之后的新行为：
//   * 位置分离：角色压住箱子时把箱子推开；
//   * 速度传递：角色朝箱子运动时把动能传过去（走过去 vs 冲刺撞过去，差别明显）。
//
// 时间轴（10 秒 @30fps）：
//   0.0–1.5s  起步：角色从左侧走向箱子阵列（慢走 aWalkSpeed）
//   1.5–5.0s  撞进第一排：箱子被推开、互相碰撞
//   5.0–8.0s  **冲刺**（aSprintSpeed）：推得更猛，后排被撞散
//   8.0–10.0s 停下，箱子继续滑行/倒下
#include <GN.h>
#include "GN_Character.h"
#include "GN_Entity.h"
#include "GN_Gizmo.h"
#include "GN_Physics.h"

#include <png.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {

int gWidth = 1280;
int gHeight = 720;
int gFrames = 300; // 10 s @ 30fps
const char *gOutDir = ".local/video/character-push-frames";
bool gVerbose = false;
float gTime = 0.0f;

GN3d::cLIGHT *gLight = nullptr;
GN3d::cCAMERA gCamera;
GN3d::cMODEL *gGround = nullptr;
std::vector<GN3d::cMODEL *> gStatics;

// 物理 + 角色
GN3d::cPHYSICS_WORLD gWorld;
GN3d::cPHYSICS_CONTROLLER gPhysics;
GN3d::cCHARACTER gCharacter;
GN3d::cMODEL *gCharacterModel = nullptr;
GN3d::cMODEL *gCharacterHead = nullptr;

// 箱子（物理 + 模型一一对应）
struct sCrate {
    GN3d::cMODEL *aModel;
    uint32_t aHandle;
};
std::vector<sCrate> gCrates;

// 线段缓冲
std::vector<GN3d::sEDIT_LINE> gSceneLines;
std::vector<GN3d::sEDIT_LINE> gOverlayLines;

float gSpeedThisFrame = 0.0f;
int gPushedTotal = 0;

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

void initLights() {
    gLight = new GN3d::cLIGHT();
    gLight->AddDirectional(GNg::sVEC3{0.42f, -0.76f, -0.50f}, GNg::sVEC3{1.0f, 0.98f, 0.94f},
                           1.9f, false);
    gLight->AddDirectional(GNg::sVEC3{-0.6f, -0.3f, 0.65f}, GNg::sVEC3{0.45f, 0.55f, 0.80f}, 0.35f,
                           false);
    gLight->SetAmbient(GNg::sVEC3{0.24f, 0.26f, 0.31f}, 1.0f);
}

void buildScene() {
    gWorld.SetFixedTimestep(1.0f / 60.0f, 4);
    gWorld.SetGravity(GNg::sVEC3{0.0f, -9.81f, 0.0f});
    gPhysics.SetWorld(&gWorld);

    gGround = GN3d::CreateCubeModel(1.0f, 0.115f, 0.12f, 0.135f);
    gGround->SetScale(30.0f, 0.1f, 24.0f);
    gGround->SetPosition(0.0f, -0.05f, 0.0f);
    gGround->computeWorldAABB();

    // 物理地面
    gWorld.AddBody(GN3d::MakeBoxBody(GN3d::eBODY_STATIC, GNg::sVEC3{0.0f, -0.5f, 0.0f},
                                     GNg::sVEC3{15.0f, 0.5f, 12.0f}, 0.0f));

    // 角色：站在 (-5, ?, 0)
    GN3d::sCHARACTER_DESC desc;
    desc.aRadius = 0.32f;
    desc.aHeight = 1.75f;
    desc.aWalkSpeed = 2.6f;
    desc.aSprintSpeed = 5.2f;
    desc.aStepHeight = 0.35f;
    gCharacter.Init(desc);
    gCharacter.SetPosition(GNg::sVEC3{-4.6f, desc.aHeight * 0.5f + 0.05f, 0.0f});
    gCharacter.SetPhysicsWorld(&gWorld);
    gCharacter.SetPushStrength(1.0f);

    // 角色的碰撞几何（地面 + 四周矮墙，防止掉出去）
    gCharacter.AddFloor(GNg::sVEC3{0.0f, 0.0f, 0.0f}, 15.0f, 12.0f, 0.0f);

    // 角色模型：胶囊（用一个细长的箱子代替，能看出朝向与站立高度）
    // 角色模型：身体（矮宽）+ 头（小方块）。一根细柱子看不出"这是个人"。
    gCharacterModel = GN3d::CreateCubeModel(1.0f, 0.92f, 0.70f, 0.34f);
    gCharacterModel->SetScale(desc.aRadius * 2.2f, desc.aHeight * 0.72f, desc.aRadius * 2.2f);
    gCharacterModel->computeWorldAABB();
    gCharacterHead = GN3d::CreateCubeModel(1.0f, 0.98f, 0.80f, 0.52f);
    gCharacterHead->SetScale(desc.aRadius * 1.9f, desc.aRadius * 1.9f, desc.aRadius * 1.9f);
    gCharacterHead->computeWorldAABB();

    // 箱子阵列：3 排 × 5 列，间隔 1.1 m（略大于箱宽 1.0，留缝隙）
    const int rows = 3;
    const int cols = 4;
    for (int row = 0; row < rows; ++row) {
        for (int col = 0; col < cols; ++col) {
            const float x = -2.2f + (float)col * 1.15f;
            const float z = -1.2f + (float)row * 1.2f;
            const float y = 0.5f;
            // 不同排稍微不同色调，便于看清是分开的物体
            const float tint = 0.55f + 0.12f * (float)row;
            sCrate crate;
            crate.aModel = GN3d::CreateCubeModel(1.0f, tint, tint * 0.88f, tint * 0.66f);
            crate.aModel->SetPosition(x, y, z);
            crate.aModel->computeWorldAABB();
            crate.aHandle = gWorld.AddBody(GN3d::MakeBoxBody(
                GN3d::eBODY_DYNAMIC, GNg::sVEC3{x, y, z}, GNg::sVEC3{0.5f, 0.5f, 0.5f}, 1.0f));
            gCrates.push_back(crate);
        }
    }

    // 背景柱子
    const float positions[5][3] = {{-8.0f, 0.0f, -6.0f}, {8.0f, 0.0f, -6.0f},
                                   {-8.0f, 0.0f, 6.0f}, {8.0f, 0.0f, 6.0f},
                                   {0.0f, 0.0f, -7.5f}};
    for (int i = 0; i < 5; ++i) {
        GN3d::cMODEL *pillar = GN3d::CreateCubeModel(1.0f, 0.19f, 0.20f, 0.23f);
        pillar->SetScale(0.4f, 1.6f, 0.4f);
        pillar->SetPosition(positions[i][0], 0.8f, positions[i][2]);
        pillar->computeWorldAABB();
        gStatics.push_back(pillar);
    }
}

// 相机：侧面视角，能同时看到角色与箱子阵列
bool buildCamera(float aspect, GNg::sMAT44 &viewProj, GNg::sMAT44 &view, GNg::sVEC3 &eye) {
    // 沿 -Z 方向偏一点，微微俯视
    // 相机跟随角色 + 看向前方（箱子阵列方向），跟随平滑
    const float followX = gCharacter.Position().x;
    const GNg::sVEC3 focus = {followX + 1.0f, 1.05f, 0.0f};
    gCamera.SetOrbit(focus, 9.5f, 1.10f, 0.42f);
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
    GNg::GN_VkSetEnvironmentLighting(GN3d::eENV_LIGHTING_INDOOR_UNIFORM, 0.20f);
    GNg::GN_VkSetExposure(-0.10f);
    GNg::GN_VkSetBloom(0.28f, 1.15f, 0.30f);
    std::fprintf(stderr, "[characterPush] 角色 + %u 个箱子 + %u 个柱子\n", (unsigned)gCrates.size(),
                 (unsigned)gStatics.size());
}

GN_CALL void AppUnInit() {
    for (size_t i = 0; i < gCrates.size(); ++i) delete gCrates[i].aModel;
    gCrates.clear();
    for (size_t i = 0; i < gStatics.size(); ++i) delete gStatics[i];
    gStatics.clear();
    delete gCharacterModel;
    gCharacterModel = nullptr;
    delete gCharacterHead;
    gCharacterHead = nullptr;
    delete gGround;
    gGround = nullptr;
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

    // ── 1) 角色控制脚本 ──
    // 0.0–5.0 慢走；5.0–8.0 冲刺；8.0 之后停下
    bool sprint = false;
    float moveInput = 0.0f;
    // 慢走穿过前两列 → 停顿 → 冲刺撞第三列 → 停下看箱子滑行
    if (t < 2.2f) {
        moveInput = 1.0f;
        sprint = false;
    } else if (t >= 2.8f && t < 3.8f) {
        moveInput = 1.0f;
        sprint = true;
    }
    gSpeedThisFrame = sprint ? gCharacter.Desc().aSprintSpeed : gCharacter.Desc().aWalkSpeed;

    // 物理与角色按固定 60Hz 推进（视频 30fps，每帧两步）
    const int subSteps = 2;
    const float fixedDt = 1.0f / 60.0f;
    uint32_t pushedThisFrame = 0;
    for (int i = 0; i < subSteps; ++i) {
        gWorld.Step(fixedDt);
        gCharacter.Move(GNg::sVEC3{moveInput, 0.0f, 0.0f}, sprint, false, fixedDt);
        pushedThisFrame += gCharacter.LastPushedCount();
    }
    if (pushedThisFrame > 0) gPushedTotal += (int)pushedThisFrame;

    // 角色模型跟随胶囊位置（脚底对齐）
    {
        const GNg::sVEC3 foot = gCharacter.FootPosition();
        const float height = gCharacter.Desc().aHeight;
        gCharacterModel->SetPosition(foot.x, foot.y + height * 0.36f, foot.z);
        gCharacterModel->computeWorldAABB();
        gCharacterHead->SetPosition(foot.x, foot.y + height * 0.86f, foot.z);
        gCharacterHead->computeWorldAABB();
    }
    // 箱子模型跟随刚体
    for (size_t i = 0; i < gCrates.size(); ++i) {
        const GN3d::sRIGID_BODY *body = gWorld.GetBody(gCrates[i].aHandle);
        if (body == nullptr) continue;
        gCrates[i].aModel->SetPosition(body->aPos.x, body->aPos.y, body->aPos.z);
        gCrates[i].aModel->SetRotation(body->aRot);
        gCrates[i].aModel->computeWorldAABB();
    }

    // ── 2) 相机 ──
    const float aspect = (float)gWidth / (float)gHeight;
    GNg::sMAT44 view;
    GNg::sMAT44 viewProj;
    GNg::sVEC3 eye;
    if (!buildCamera(aspect, viewProj, view, eye)) return;

    // ── 3) 3D pass ──
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
    for (size_t i = 0; i < gCrates.size(); ++i) gCrates[i].aModel->DrawPBRSorted(eye);
    gCharacterModel->DrawPBRSorted(eye);
    gCharacterHead->DrawPBRSorted(eye);

    // ── 4) 线段：网格 + 角色胶囊线框（看清碰撞体）──
    gSceneLines.clear();
    gOverlayLines.clear();
    {
        GN3d::sGRID_DESC grid;
        grid.aCellSize = 1.0f;
        grid.aHalfCells = 10;
        grid.aMajorEvery = 5;
        GN3d::buildGridLines(grid, gSceneLines);
    }
    // 角色胶囊的线框（竖直圆柱 + 上下半球简化为两圈）
    {
        const GNg::sVEC3 center = gCharacter.Position();
        const float radius = gCharacter.Desc().aRadius;
        const float half = (gCharacter.Desc().aHeight - radius * 2.0f) * 0.5f;
        const uint32_t color = 0xFF4AC8E0;
        const int kSeg = 24;
        for (int ring = 0; ring < 4; ++ring) {
            const float y = center.y + ((ring < 2) ? -half : half);
            const float r = (ring % 2 == 0) ? radius : radius * 0.72f;
            GNg::sVEC3 prev = {center.x + r, y, center.z};
            for (int s = 1; s <= kSeg; ++s) {
                const float angle = (float)s * 6.2831853f / (float)kSeg;
                const GNg::sVEC3 point = {center.x + std::cos(angle) * r, y,
                                          center.z + std::sin(angle) * r};
                gSceneLines.push_back(GN3d::sEDIT_LINE(prev, point, color));
                prev = point;
            }
        }
    }

    if (!gSceneLines.empty()) GNg::GN_VkDrawEditLines(&gSceneLines[0], (uint32_t)gSceneLines.size());

    frame.End();

    if (gVerbose && ((int)(t * 30.0f) % 15 == 0)) {
        std::fprintf(stderr, "[dbg] t=%.2f charX=%.2f speed=%.1f pushedTotal=%d\n", t,
                     gCharacter.Position().x, gSpeedThisFrame, gPushedTotal);
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
        std::fprintf(stderr, "[characterPush] 第 %d 帧回读失败\n", index);
        return;
    }
    char path[512];
    std::snprintf(path, sizeof(path), "%s/frame_%05d.png", gOutDir, index);
    if (!writePng(path, rgba, w, h)) std::fprintf(stderr, "[characterPush] 写 %s 失败\n", path);
    GN_GrabFrameFree(rgba);
    if (index % 30 == 0) std::fprintf(stderr, "[characterPush] 帧 %d/%d\n", index, gFrames);
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
    if (std::system(cmd) != 0) std::fprintf(stderr, "[characterPush] 清理输出目录失败\n");

    std::fprintf(stderr, "[characterPush] %dx%d，%d 帧（%.1f s）→ %s\n", gWidth, gHeight, gFrames,
                 (float)gFrames / 30.0f, gOutDir);

    GN_InitLoadCallBackFunc(AppInit, AppUnInit);
    GN_InitLoopCallBackFunc(AppDisplay2D, AppDisplay3D, AppRun);
    if (!GN_StartRunHeadless((short)gWidth, (short)gHeight, gFrames, onFrame)) {
        std::fprintf(stderr, "[characterPush] 初始化失败（无 Vulkan 设备？）\n");
        return 2;
    }
    std::printf("[characterPush] 完成：%d 帧 → %s\n", gFrames, gOutDir);
    return 0;
}
