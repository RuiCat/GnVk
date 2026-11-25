// tests/animBlend/main.cpp — D3 演示：混合树（1D 混合空间）+ 动画层遮罩
//
// 画面：三列火柴人，同一个骨架，同一个时间轴，**只有动画驱动方式不同**：
//   A 列：单 clip 播放 walk（基准）
//   B 列：1D 混合空间，参数从 walk 速度**连续扫到** run 速度
//         —— 步幅连续变大，中间没有"整段切换"的跳变
//   C 列：基础层 = 走路的腿，覆盖层 = 挥手臂（遮罩只作用于上半身）
//         —— 腿在走、手臂在挥，两段动画**同时**播
//
// 这是这两个类最核心的两个用途，摆在一起比文字更直观。
#include <GN.h>
#include "GN_3Dmath.h"
#include "GN_3Danim.h"
#include "GN_AnimBlend.h"

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
const char *gOutDir = ".local/video/animBlend-frames";
bool gVerbose = false;
float gTime = 0.0f;

GN3d::cLIGHT *gLight = nullptr;
GN3d::cCAMERA gCamera;
GN3d::cMODEL *gGround = nullptr;

// 火柴人：每个身体部位是一个立方体，挂在关节上
struct sStick {
    GN3d::cMODEL *aBody;
    GN3d::cMODEL *aHead;
    GN3d::cMODEL *aArmL;
    GN3d::cMODEL *aArmR;
    GN3d::cMODEL *aLegL;
    GN3d::cMODEL *aLegR;
    float aBaseX; // 站立位置
    float aTint;
};
std::vector<sStick> gSticks;

// 三列各自的动画驱动
GN3d::cANIM_PLAYER gPlayer;         // A：单 clip
GN3d::cANIM_BLEND_1D gBlend;        // B：混合空间
GN3d::cANIM_LAYER_STACK gStack;     // C：层栈
GN3d::cANIM_BLEND_1D gLowerBody;    // C 的基础层
GN3d::cANIM_BLEND_1D gUpperBody;    // C 的覆盖层

// clips（程序化生成，仓库里没有模型资产）
GN3d::sANIM_CLIP *gWalk = nullptr;
GN3d::sANIM_CLIP *gRun = nullptr;
GN3d::sANIM_CLIP *gWave = nullptr;

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

// 造一段"摆腿"的 clip：腿关节绕 X 轴摆，幅度/频率可调。
// 程序化生成的原因：仓库 .gitignore 掉了所有模型/动画资产，验收只能用 fixtures。
GN3d::sANIM_CLIP *makeLegSwingClip(float amplitude, float duration) {
    GN3d::sANIM_CLIP *clip = new GN3d::sANIM_CLIP();
    GN3d::sANIM_CHANNEL channel;
    channel.aNode = 0; // 关节 0 = 髋（腿的根）
    channel.aPath = GN3d::eANIM_ROTATION;
    channel.aInterp = GN3d::eANIM_LINEAR;
    // 一个完整循环：0 → +A → 0 → -A → 0
    const int kKeys = 5;
    for (int i = 0; i < kKeys; ++i) {
        const float t = duration * (float)i / (float)(kKeys - 1);
        float angle = 0.0f;
        if (i == 1) angle = amplitude;
        else if (i == 3) angle = -amplitude;
        GNg::sQUAT q = {};
        q.w = std::cos(angle * 0.5f);
        q.x = std::sin(angle * 0.5f); // 绕 X
        channel.aTimes.push_back(t);
        channel.aQuatValues.push_back(q);
    }
    clip->aChannels.push_back(channel);
    clip->updateDuration();
    return clip;
}

// 造一段"挥手"的 clip：手臂关节绕 Z 轴摆（频率更高，与腿不同步）
GN3d::sANIM_CLIP *makeArmSwingClip(float amplitude, float duration) {
    GN3d::sANIM_CLIP *clip = new GN3d::sANIM_CLIP();
    GN3d::sANIM_CHANNEL channel;
    channel.aNode = 1; // 关节 1 = 肩
    channel.aPath = GN3d::eANIM_ROTATION;
    channel.aInterp = GN3d::eANIM_LINEAR;
    const int kKeys = 5;
    for (int i = 0; i < kKeys; ++i) {
        const float t = duration * (float)i / (float)(kKeys - 1);
        float angle = 0.0f;
        if (i == 1) angle = amplitude;
        else if (i == 3) angle = -amplitude;
        GNg::sQUAT q = {};
        q.w = std::cos(angle * 0.5f);
        q.z = std::sin(angle * 0.5f); // 绕 Z
        channel.aTimes.push_back(t);
        channel.aQuatValues.push_back(q);
    }
    clip->aChannels.push_back(channel);
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
    gGround->SetScale(30.0f, 0.1f, 16.0f);
    gGround->SetPosition(0.0f, -0.05f, 0.0f);
    gGround->computeWorldAABB();

    // 程序化 clips
    gWalk = makeLegSwingClip(0.30f, 1.0f);  // 慢：大幅 1.0s 循环
    gRun = makeLegSwingClip(0.65f, 0.55f);  // 快：更大更急 0.55s 循环
    gWave = makeArmSwingClip(0.55f, 0.42f); // 手臂：最快

    // A：单播 walk
    gPlayer.play(gWalk, true, 1.0f);

    // B：walk↔run 混合空间
    gBlend.AddPoint(1.5f, gWalk);
    gBlend.AddPoint(4.5f, gRun);
    gBlend.Play(true);

    // C：层栈（基础 = 纯走路；覆盖 = 挥手，遮罩只影响"上半身"关节）
    gLowerBody.AddPoint(0.0f, gWalk);
    gLowerBody.Play(true);
    gUpperBody.AddPoint(0.0f, gWave);
    gUpperBody.Play(true);
    gStack.SetBase(&gLowerBody);
    // 遮罩：关节 0（髋/腿）= 0 不受挥手影响；关节 1（肩/臂）= 1 完全生效
    const float mask[2] = {0.0f, 1.0f};
    gStack.AddLayer(&gUpperBody, 1.0f, mask, 2);

    // 三列火柴人
    const float xs[3] = {-2.5f, 0.0f, 2.5f};
    const float tints[3] = {0.62f, 0.42f, 0.40f};
    for (int i = 0; i < 3; ++i) {
        sStick stick;
        stick.aBaseX = xs[i];
        stick.aTint = tints[i];
        stick.aBody = GN3d::CreateCubeModel(1.0f, tints[i], tints[i] * 0.92f, tints[i] * 0.85f);
        stick.aBody->SetScale(0.46f, 0.52f, 0.26f);
        stick.aHead = GN3d::CreateCubeModel(1.0f, 0.95f, 0.82f, 0.68f);
        stick.aHead->SetScale(0.26f, 0.26f, 0.26f);
        stick.aArmL = GN3d::CreateCubeModel(1.0f, tints[i] * 0.9f, tints[i] * 0.82f, tints[i] * 0.75f);
        stick.aArmL->SetScale(0.13f, 0.56f, 0.13f);
        stick.aArmR = GN3d::CreateCubeModel(1.0f, tints[i] * 0.9f, tints[i] * 0.82f, tints[i] * 0.75f);
        stick.aArmR->SetScale(0.13f, 0.56f, 0.13f);
        stick.aLegL = GN3d::CreateCubeModel(1.0f, tints[i] * 0.7f, tints[i] * 0.65f, tints[i] * 0.6f);
        stick.aLegL->SetScale(0.15f, 0.72f, 0.15f);
        stick.aLegR = GN3d::CreateCubeModel(1.0f, tints[i] * 0.7f, tints[i] * 0.65f, tints[i] * 0.6f);
        stick.aLegR->SetScale(0.15f, 0.72f, 0.15f);
        gSticks.push_back(stick);
    }
}

// 由 clip 的采样结果摆放火柴人
void poseStick(sStick &stick, float hipAngle, float shoulderAngle) {
    const float x = stick.aBaseX;
    const float legLen = 0.72f;
    const float hipY = legLen; // 腿底落地（y=0）
    const float bodyH = 0.52f;
    const float headH = 0.26f;

    // ── 腿：绕 X 轴前后摆（**约定 angle>0 = 向前 +Z**）──
    // 腿默认朝下（局部 -Y）。绕 X 转 φ 后方向是 (0,-cosφ,-sinφ)；
    // 要让下端往 +Z 走必须用 φ = -angle，所以四元数 x 分量取 **-sin(angle/2)**。
    // 之前这里用 +sin（位置偏移还写在了 X 上），腿会朝侧向张开而不是前后摆。
    for (int side = 0; side < 2; ++side) {
        const float sx = (side == 0) ? -0.13f : 0.13f;
        const float a = (side == 0) ? hipAngle : -hipAngle;
        GN3d::cMODEL *leg = (side == 0) ? stick.aLegL : stick.aLegR;
        leg->SetPosition(x + sx, hipY - std::cos(a) * legLen * 0.5f,
                         std::sin(a) * legLen * 0.5f);
        GNg::sQUAT q = {};
        q.w = std::cos(a * 0.5f);
        q.x = -std::sin(a * 0.5f); // 注意负号
        leg->SetRotation(q);
        leg->computeWorldAABB();
    }

    // ── 身体与头 ──
    stick.aBody->SetPosition(x, hipY + bodyH * 0.5f, 0.0f);
    stick.aBody->computeWorldAABB();
    stick.aHead->SetPosition(x, hipY + bodyH + headH * 0.5f, 0.0f);
    stick.aHead->computeWorldAABB();

    // ── 手臂：绕 Z 轴左右张开（这个轴是对的：绕 Z 转会把下端推向 ±X）──
    const float armY = hipY + bodyH * 0.86f;
    const float armLen = 0.56f;
    const float s1 = shoulderAngle;
    const float s2 = -shoulderAngle * 0.6f;
    // 左臂：绕 Z 转 +s 时 (0,-1,0) → (sin s, -cos s, 0)，下端往 +X
    // 左边手臂要往 -X 张，所以用 -s1
    stick.aArmL->SetPosition(x - 0.26f - std::sin(std::fabs(s1)) * armLen * 0.5f,
                             armY - std::cos(s1) * armLen * 0.5f, 0.0f);
    stick.aArmR->SetPosition(x + 0.26f + std::sin(std::fabs(s2)) * armLen * 0.5f,
                             armY - std::cos(s2) * armLen * 0.5f, 0.0f);
    {
        GNg::sQUAT q = {};
        q.w = std::cos(-s1 * 0.5f);
        q.z = std::sin(-s1 * 0.5f);
        stick.aArmL->SetRotation(q);
        GNg::sQUAT q2 = {};
        q2.w = std::cos(s2 * 0.5f);
        q2.z = std::sin(s2 * 0.5f);
        stick.aArmR->SetRotation(q2);
    }
    for (GN3d::cMODEL *m : {stick.aBody, stick.aHead, stick.aArmL, stick.aArmR, stick.aLegL,
                            stick.aLegR}) {
        m->computeWorldAABB();
    }
}

// 从 clip 采样一个关节的"绕 X 角度"（用四元数反推，够本演示用）
float sampleHipAngle(const GN3d::sANIM_CLIP *clip, float time) {
    GN3d::sANIM_TRANSFORM t;
    if (!clip->sample(0, time, t)) return 0.0f;
    return 2.0f * std::asin(std::max(-1.0f, std::min(1.0f, t.aRotation.x)));
}

float sampleShoulderAngle(const GN3d::sANIM_CLIP *clip, float time) {
    GN3d::sANIM_TRANSFORM t;
    if (!clip->sample(1, time, t)) return 0.0f;
    return 2.0f * std::asin(std::max(-1.0f, std::min(1.0f, t.aRotation.z)));
}

bool buildCamera(float aspect, GNg::sMAT44 &viewProj, GNg::sMAT44 &view, GNg::sVEC3 &eye) {
    // 斜 45° 侧视 + 微俯：既能看清腿的前后摆动，又能把三列分清楚
    gCamera.SetOrbit(GNg::sVEC3{0.0f, 1.10f, 0.0f}, 8.2f, 1.05f, 0.58f);
    gCamera.SetFov(44.0f);
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
    std::fprintf(stderr, "[animBlend] 三列火柴人：A=单clip B=混合空间 C=层栈遮罩\n");
}

GN_CALL void AppUnInit() {
    for (size_t i = 0; i < gSticks.size(); ++i) {
        delete gSticks[i].aBody;
        delete gSticks[i].aHead;
        delete gSticks[i].aArmL;
        delete gSticks[i].aArmR;
        delete gSticks[i].aLegL;
        delete gSticks[i].aLegR;
    }
    gSticks.clear();
    delete gWalk;
    delete gRun;
    delete gWave;
    gWalk = gRun = gWave = nullptr;
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

    // ── B：参数在 1.5 → 4.5 之间来回扫（每 4 秒一个来回）──
    const float sweep = 0.5f - 0.5f * std::cos(t * 1.5707963f); // 0..1..0 周期 4s
    const float parameter = 1.5f + (4.5f - 1.5f) * sweep;
    gBlend.SetParameter(parameter);

    // ── 推进三列的时间轴 ──
    gPlayer.update(dt);
    gBlend.Update(dt);
    gStack.Update(dt);

    // ── A：单 clip（相位 = player.time / walk.duration）──
    {
        const float phase = gWalk->aDuration > 0.0f ? gPlayer.time() / gWalk->aDuration : 0.0f;
        const float hip = sampleHipAngle(gWalk, phase * gWalk->aDuration);
        poseStick(gSticks[0], hip, 0.0f); // 手臂不挥
    }
    // ── B：混合空间（取混合后的髋角）──
    {
        GN3d::sANIM_TRANSFORM out;
        float hip = 0.0f;
        if (gBlend.Sample(0, out)) hip = 2.0f * std::asin(std::max(-1.0f, std::min(1.0f,
                                                                          out.aRotation.x)));
        poseStick(gSticks[1], hip, 0.0f);
    }
    // ── C：层栈（髋来自基础层、肩来自覆盖层）──
    {
        GN3d::sANIM_TRANSFORM out;
        float hip = 0.0f;
        float shoulder = 0.0f;
        if (gStack.Sample(0, out))
            hip = 2.0f * std::asin(std::max(-1.0f, std::min(1.0f, out.aRotation.x)));
        if (gStack.Sample(1, out))
            shoulder = 2.0f * std::asin(std::max(-1.0f, std::min(1.0f, out.aRotation.z)));
        poseStick(gSticks[2], hip, shoulder);
    }

    // ── 渲染 ──
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
    for (size_t i = 0; i < gSticks.size(); ++i) {
        gSticks[i].aBody->DrawPBRSorted(eye);
        gSticks[i].aHead->DrawPBRSorted(eye);
        gSticks[i].aArmL->DrawPBRSorted(eye);
        gSticks[i].aArmR->DrawPBRSorted(eye);
        gSticks[i].aLegL->DrawPBRSorted(eye);
        gSticks[i].aLegR->DrawPBRSorted(eye);
    }
    frame.End();

    if (gVerbose && ((int)(t * 30.0f) % 30 == 0)) {
        std::fprintf(stderr, "[dbg] t=%.2f blendParam=%.2f (sweep %.2f)\n", t, parameter, sweep);
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
    if (index % 30 == 0) std::fprintf(stderr, "[animBlend] 帧 %d/%d\n", index, gFrames);
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
    if (std::system(cmd) != 0) std::fprintf(stderr, "[animBlend] 清理目录失败\n");
    std::fprintf(stderr, "[animBlend] %dx%d，%d 帧 → %s\n", gWidth, gHeight, gFrames, gOutDir);
    GN_InitLoadCallBackFunc(AppInit, AppUnInit);
    GN_InitLoopCallBackFunc(AppDisplay2D, AppDisplay3D, AppRun);
    if (!GN_StartRunHeadless((short)gWidth, (short)gHeight, gFrames, onFrame)) {
        std::fprintf(stderr, "[animBlend] 初始化失败\n");
        return 2;
    }
    std::printf("[animBlend] 完成：%d 帧\n", gFrames);
    return 0;
}
