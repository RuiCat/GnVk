// tests/rootMotion/main.cpp — C3 演示：Root Motion 消除脚滑
//
// 两列火柴人，同时在"走"，位移来源不同：
//   左：**代码驱动**（wishDir × walkSpeed）。动画步子与代码速度独立 → 脚会滑。
//   右：**Root Motion**（位移来自动画根关节）。动画走多远就走多远 → 脚不滑。
//
// 为了让"滑"看得出来，地面上画了等距刻度线：脚落点相对刻度的漂移一眼就能看到。
#include <GN.h>
#include "GN_3Dmath.h"
#include "GN_3Danim.h"
#include "GN_RootMotion.h"
#include "GN_Character.h"
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
int gFrames = 360; // 12 s @ 30fps
const char *gOutDir = ".local/video/rootMotion-frames";
bool gVerbose = false;
float gTime = 0.0f;

GN3d::cLIGHT *gLight = nullptr;
GN3d::cCAMERA gCamera;
GN3d::cMODEL *gGround = nullptr;

// 两列角色
struct sWalker {
    GN3d::cCHARACTER aCharacter;
    GN3d::cMODEL *aBody;
    GN3d::cMODEL *aHead;
    GN3d::cMODEL *aLegL;
    GN3d::cMODEL *aLegR;
    float aStartX;
    float aZ;
    float aTint;
    bool aRootMotion;
};
std::vector<sWalker> gWalkers;

// 动画
GN3d::sANIM_CLIP *gWalkClip = nullptr;
GN3d::cANIM_PLAYER gLeftPlayer;     // 左：只用来摆腿
GN3d::cANIM_BLEND_1D gRightBlend;   // 右：混合空间（输出也用于提取 root）
GN3d::cROOT_MOTION_EXTRACTOR gExtractor;

std::vector<GN3d::sEDIT_LINE> gLines;

// 动画根关节每帧前进的距离（"动画里走多远"）
const float kAnimStepPerFrame = 0.0125f; // 60fps ⇒ 0.75 m/s（故意与代码速度不同）

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

// 造一段走路 clip：根关节（node 0）沿 +Z 匀速前进，腿（node 1）摆动。
// 循环时根关节**回绕到 0** —— 这正是 root motion 需要"瞬移检测"的场景。
GN3d::sANIM_CLIP *makeWalkClip(float duration, float strideLength) {
    GN3d::sANIM_CLIP *clip = new GN3d::sANIM_CLIP();
    // 根：0 → strideLength（循环回绕）
    {
        GN3d::sANIM_CHANNEL root;
        root.aNode = 0;
        root.aPath = GN3d::eANIM_TRANSLATION;
        root.aInterp = GN3d::eANIM_LINEAR;
        root.aTimes.push_back(0.0f);
        root.aTimes.push_back(duration);
        root.aVecValues.push_back(GNg::sVEC3{0.0f, 0.0f, 0.0f});
        root.aVecValues.push_back(GNg::sVEC3{0.0f, 0.0f, strideLength});
        clip->aChannels.push_back(root);
    }
    // 腿：绕 X 摆动（一个循环 = 两步）
    {
        GN3d::sANIM_CHANNEL leg;
        leg.aNode = 1;
        leg.aPath = GN3d::eANIM_ROTATION;
        leg.aInterp = GN3d::eANIM_LINEAR;
        const float amp = 0.35f;
        const int kKeys = 5;
        for (int i = 0; i < kKeys; ++i) {
            const float t = duration * (float)i / (float)(kKeys - 1);
            float angle = 0.0f;
            if (i == 1) angle = amp;
            else if (i == 3) angle = -amp;
            GNg::sQUAT q = {};
            q.w = std::cos(angle * 0.5f);
            q.x = std::sin(angle * 0.5f);
            leg.aTimes.push_back(t);
            leg.aQuatValues.push_back(q);
        }
        clip->aChannels.push_back(leg);
    }
    clip->updateDuration();
    return clip;
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
    gGround = GN3d::CreateCubeModel(1.0f, 0.115f, 0.12f, 0.135f);
    gGround->SetScale(40.0f, 0.1f, 14.0f);
    gGround->SetPosition(0.0f, -0.05f, 0.0f);
    gGround->computeWorldAABB();

    // 一个走路循环 = 1 s，动画里前进 0.9 m
    gWalkClip = makeWalkClip(1.0f, 0.9f);
    // 关键：循环型 clip 必须告诉提取器步幅，否则回绕那一帧会被当瞬移丢弃，
    // 角色每循环少走一步（实测 4 秒只走 0.03 m）
    gExtractor.SetLoopStride(0.9f);
    gLeftPlayer.play(gWalkClip, true, 1.0f);
    gRightBlend.AddPoint(0.0f, gWalkClip);
    gRightBlend.Play(true);

    const float xs[2] = {-1.6f, 1.6f};
    const float tints[2] = {0.72f, 0.45f};
    for (int i = 0; i < 2; ++i) {
        sWalker w;
        w.aStartX = xs[i];
        w.aZ = 0.0f;
        w.aTint = tints[i];
        w.aRootMotion = (i == 1);

        GN3d::sCHARACTER_DESC desc;
        desc.aRadius = 0.30f;
        desc.aHeight = 1.70f;
        // 左列：代码速度**故意**与动画不匹配（2.2 vs 0.9），让"滑"看得见
        desc.aWalkSpeed = 2.2f;
        desc.aStepHeight = 0.35f;
        w.aCharacter.Init(desc);
        w.aCharacter.SetPosition(GNg::sVEC3{xs[i], desc.aHeight * 0.5f + 0.05f, 0.0f});
        w.aCharacter.AddFloor(GNg::sVEC3{0.0f, 0.0f, 0.0f}, 20.0f, 7.0f, 0.0f);
        w.aCharacter.SetRootMotionEnabled(w.aRootMotion);

        w.aBody = GN3d::CreateCubeModel(1.0f, tints[i], tints[i] * 0.92f, tints[i] * 0.85f);
        w.aBody->SetScale(0.52f, 0.88f, 0.28f);
        w.aHead = GN3d::CreateCubeModel(1.0f, 0.95f, 0.84f, 0.70f);
        w.aHead->SetScale(0.32f, 0.32f, 0.32f);
        w.aLegL = GN3d::CreateCubeModel(1.0f, tints[i] * 0.7f, tints[i] * 0.64f, tints[i] * 0.58f);
        w.aLegL->SetScale(0.17f, 0.76f, 0.17f);
        w.aLegR = GN3d::CreateCubeModel(1.0f, tints[i] * 0.7f, tints[i] * 0.64f, tints[i] * 0.58f);
        w.aLegR->SetScale(0.17f, 0.76f, 0.17f);
        gWalkers.push_back(w);
    }
}

void poseWalker(sWalker &w, float legAngle) {
    const GNg::sVEC3 foot = w.aCharacter.FootPosition();
    // 角色朝向 +Z；腿在 X 轴上左右分开，前后摆动
    const float x = foot.x;
    const float z = foot.z;
    const float hipY = foot.y + 1.05f;
    const float legLen = 0.76f;
    for (int side = 0; side < 2; ++side) {
        const float sx = (side == 0) ? -0.13f : 0.13f;
        const float angle = (side == 0) ? legAngle : -legAngle;
        GN3d::cMODEL *leg = (side == 0) ? w.aLegL : w.aLegR;
        leg->SetPosition(x + sx, hipY - std::cos(angle) * legLen * 0.5f,
                         z + std::sin(angle) * legLen * 0.5f);
        GNg::sQUAT q = {};
        q.w = std::cos(angle * 0.5f);
        q.x = std::sin(angle * 0.5f);
        leg->SetRotation(q);
        leg->computeWorldAABB();
    }
    w.aBody->SetPosition(x, hipY + 0.44f, z);
    w.aBody->computeWorldAABB();
    w.aHead->SetPosition(x, hipY + 1.08f, z);
    w.aHead->computeWorldAABB();
}

bool buildCamera(float aspect, GNg::sMAT44 &viewProj, GNg::sMAT44 &view, GNg::sVEC3 &eye) {
    // 侧视：脚落点相对地面刻度的漂移在这个角度最清楚
    // 跟随较慢的那一列（root motion），左列跑得快会离开画面 —— 这正是演示想表达的差异
    const float midZ = (gWalkers.size() > 1) ? gWalkers[1].aCharacter.Position().z : 0.0f;
    // 斜上方俯视：既看得到脚落点，也看得到地面刻度线的间距
    gCamera.SetOrbit(GNg::sVEC3{0.0f, 0.80f, midZ + 1.2f}, 8.0f, 1.05f, 0.62f);
    gCamera.SetFov(46.0f);
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
    std::fprintf(stderr, "[rootMotion] 左=代码驱动(speed 2.2) 右=root motion(动画 0.9 m/循环)\n");
}

GN_CALL void AppUnInit() {
    for (size_t i = 0; i < gWalkers.size(); ++i) {
        delete gWalkers[i].aBody;
        delete gWalkers[i].aHead;
        delete gWalkers[i].aLegL;
        delete gWalkers[i].aLegR;
    }
    gWalkers.clear();
    delete gWalkClip;
    gWalkClip = nullptr;
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
    const float fixedDt = 1.0f / 60.0f;

    // 动画推进（两个角色共用同一段 clip，但各自独立相位不会漂）
    gLeftPlayer.update(dt);
    gRightBlend.Update(dt);

    for (int sub = 0; sub < 2; ++sub) {
        // ── 左：代码驱动 ──
        gWalkers[0].aCharacter.Move(GNg::sVEC3{0.0f, 0.0f, 1.0f}, false, false, fixedDt);

        // ── 右：root motion ──
        // 从混合树采样根关节，提取本帧位移，交给角色
        GN3d::sANIM_TRANSFORM root;
        if (gRightBlend.Sample(0, root)) {
            GN3d::sROOT_MOTION motion;
            gExtractor.SetFacingYaw(0.0f); // 朝向 +Z（0）
            if (gExtractor.Extract(root, &motion)) {
                gWalkers[1].aCharacter.ApplyRootMotion(motion.aDeltaPosition, fixedDt);
            }
        }
        gWalkers[1].aCharacter.Move(GNg::sVEC3{0.0f, 0.0f, 0.0f}, false, false, fixedDt);
    }

    // 摆姿势：腿角来自 clip 采样
    {
        GN3d::sANIM_TRANSFORM leg;
        const float leftTime = gWalkClip->aDuration > 0.0f
                                   ? std::fmod(gLeftPlayer.time(), gWalkClip->aDuration)
                                   : 0.0f;
        float leftAngle = 0.0f;
        if (gWalkClip->sample(1, leftTime, leg))
            leftAngle = 2.0f * std::asin(std::max(-1.0f, std::min(1.0f, leg.aRotation.x)));
        poseWalker(gWalkers[0], leftAngle);

        float rightAngle = 0.0f;
        if (gRightBlend.Sample(1, leg))
            rightAngle = 2.0f * std::asin(std::max(-1.0f, std::min(1.0f, leg.aRotation.x)));
        poseWalker(gWalkers[1], rightAngle);
    }

    // 相机
    const float aspect = (float)gWidth / (float)gHeight;
    GNg::sMAT44 view;
    GNg::sMAT44 viewProj;
    GNg::sVEC3 eye;
    if (!buildCamera(aspect, viewProj, view, eye)) return;

    GNg::cRENDER_FRAME frame;
    GNg::sRENDER_FRAME_DESC desc;
    desc.aViewProj = &viewProj;
    desc.aCameraPos = &eye;
    desc.aLightUBO = gLight->GetUBO();
    desc.aLightUBOSize = sizeof(GN3d::sLIGHT_UBO);
    desc.aDrawSkybox = false;
    if (!frame.Begin(desc)) return;

    gGround->DrawPBRSorted(eye);
    for (size_t i = 0; i < gWalkers.size(); ++i) {
        sWalker &w = gWalkers[i];
        w.aBody->DrawPBRSorted(eye);
        w.aHead->DrawPBRSorted(eye);
        w.aLegL->DrawPBRSorted(eye);
        w.aLegR->DrawPBRSorted(eye);
    }

    // 地面刻度（每 0.3 m 一条）+ 两列的出发点标记
    gLines.clear();
    for (int i = -10; i <= 60; ++i) {
        const float z = (float)i * 0.3f;
        const uint32_t color = (i % 5 == 0) ? 0xFF888888u : 0xFF3A3A3Au;
        gLines.push_back(GN3d::sEDIT_LINE(GNg::sVEC3{-6.0f, 0.01f, z},
                                          GNg::sVEC3{6.0f, 0.01f, z}, color));
    }
    // 起点竖线（z=0）
    gLines.push_back(GN3d::sEDIT_LINE(GNg::sVEC3{-5.0f, 0.01f, 0.0f},
                                      GNg::sVEC3{5.0f, 0.01f, 0.0f}, 0xFFFFFF66u));
    // 两列当前脚位标记（横线，便于看"走了多远"）
    for (size_t i = 0; i < gWalkers.size(); ++i) {
        const GNg::sVEC3 foot = gWalkers[i].aCharacter.FootPosition();
        const uint32_t color = (i == 0) ? 0xFF66BBFFu : 0xFF66FF88u;
        gLines.push_back(GN3d::sEDIT_LINE(GNg::sVEC3{foot.x - 0.45f, 0.02f, foot.z},
                                          GNg::sVEC3{foot.x + 0.45f, 0.02f, foot.z}, color));
    }
    if (!gLines.empty()) GNg::GN_VkDrawEditLines(&gLines[0], (uint32_t)gLines.size());

    frame.End();

    if (gVerbose && ((int)(t * 30.0f) % 30 == 0)) {
        std::fprintf(stderr, "[dbg] t=%.1f  zCode=%.2f  zRoot=%.2f\n", t,
                     gWalkers[0].aCharacter.Position().z, gWalkers[1].aCharacter.Position().z);
    }
    gTime += dt;
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
    if (index % 30 == 0) std::fprintf(stderr, "[rootMotion] 帧 %d/%d\n", index, gFrames);
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
    if (std::system(cmd) != 0) std::fprintf(stderr, "[rootMotion] 清理目录失败\n");
    std::fprintf(stderr, "[rootMotion] %dx%d，%d 帧 → %s\n", gWidth, gHeight, gFrames, gOutDir);
    GN_InitLoadCallBackFunc(AppInit, AppUnInit);
    GN_InitLoopCallBackFunc(AppDisplay2D, AppDisplay3D, AppRun);
    if (!GN_StartRunHeadless((short)gWidth, (short)gHeight, gFrames, onFrame)) {
        std::fprintf(stderr, "[rootMotion] 初始化失败\n");
        return 2;
    }
    std::printf("[rootMotion] 完成：%d 帧\n", gFrames);
    return 0;
}
