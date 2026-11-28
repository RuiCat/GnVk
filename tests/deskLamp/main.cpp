// tests/deskLamp/main.cpp — 「夜读 · 台灯延时」电影渲染
//
// 程序化搭建书桌 + 台灯（底座/立柱/横臂/锥形灯罩/灯泡）+ 桌上物件（翻开的书/书堆/马克杯/球镇纸），
// 灯泡处暖色点光带 cube shadow（软半影），弱冷环境光做月光补光，bloom 突出灯泡辉光。
// 延时动画：暗场 → 开关闪烁 → 暖光渐亮 → 稳定；相机缓慢环绕。
//
// 输出：逐帧 PNG 到 --out 目录，make deskLampVideo 调 ffmpeg 编码成 mp4。
// 运行：GN_StartRunHeadless 离屏渲染（容器用 lavapipe 软件 Vulkan），无需窗口/显示。
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

int gWidth = 1280;
int gHeight = 720;
int gFrames = 300; // 30fps × 10s
const char *gOutDir = ".local/video/deskLamp-frames";
bool gVerbose = false;
float gTime = 0.0f;

GN3d::cLIGHT *gLight = nullptr;
GN3d::cCAMERA gCamera;
int gLampSlot = -1;
GNg::sVEC3 gBulbPos{0.0f, 0.0f, 0.0f};

std::vector<GN3d::cMODEL *> gModels;   // 所有模型（含投影体）
std::vector<GN3d::cMODEL *> gCasters;  // 投影体
GN3d::cMODEL *gBulb = nullptr;
GN3d::cMODEL *gFloor = nullptr;
GN3d::cMODEL *gWall = nullptr;

const float kDeskTop = 0.75f;
const float kLampFull = 4.8f;
const GNg::sVEC3 kWarm{1.0f, 0.72f, 0.42f};

// ── PNG 写出（RGBA → RGB）──
bool writePng(const char *path, const unsigned char *rgba, int width, int height) {
    FILE *file = std::fopen(path, "wb");
    if (!file) return false;
    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    png_infop info = png_create_info_struct(png);
    if (!png || !info || setjmp(png_jmpbuf(png))) {
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

// ── 程序化网格：截锥/圆柱（Y 轴，底 y=0 半径 rBot，顶 y=h 半径 rTop）──
// 顶点 11 float：pos3 / normal3 / color3(=1) / uv2。aDiffuse 承载颜色。
GN3d::cMODEL *makeCone(float rTop, float rBot, float h, int slices, float r, float g, float b,
                       bool capTop, bool capBot) {
    GN3d::cMODEL *m = new GN3d::cMODEL();
    std::memcpy(m->aName, "Cone", sizeof("Cone"));
    m->aMeshCount = 1;
    m->aMeshes = new GN3d::sMESH[1];
    GN3d::sMESH &mesh = m->aMeshes[0];
    mesh.aDiffuse[0] = r; mesh.aDiffuse[1] = g; mesh.aDiffuse[2] = b;
    mesh.aShininess = 24.0f;

    const float PI = 3.14159265358979f;
    int rings = 2;
    int sideV = rings * (slices + 1);
    int sideI = slices * 6;
    int capV = (capTop ? (slices + 2) : 0) + (capBot ? (slices + 2) : 0);
    int capI = (capTop ? slices * 3 : 0) + (capBot ? slices * 3 : 0);
    int vc = sideV + capV, ic = sideI + capI;
    float *v = new float[vc * 11];
    uint32_t *idx = new uint32_t[ic];
    float ny = (rBot - rTop);
    float nl = std::sqrt(h * h + ny * ny);
    if (nl < 1e-5f) nl = 1e-5f;

    int vi = 0;
    auto put = [&](float x, float y, float z, float nx, float nyy, float nz, float u, float vv) {
        int o = vi * 11;
        v[o + 0] = x; v[o + 1] = y; v[o + 2] = z;
        v[o + 3] = nx; v[o + 4] = nyy; v[o + 5] = nz;
        v[o + 6] = 1; v[o + 7] = 1; v[o + 8] = 1;
        v[o + 9] = u; v[o + 10] = vv;
        return vi++;
    };
    // 侧面两环
    for (int ring = 0; ring < 2; ++ring) {
        float y = ring == 0 ? 0.0f : h;
        float rad = ring == 0 ? rBot : rTop;
        for (int j = 0; j <= slices; ++j) {
            float th = 2.0f * PI * (float)j / (float)slices;
            float c = std::cos(th), s = std::sin(th);
            put(c * rad, y, s * rad, c * h / nl, ny / nl, s * h / nl,
                (float)j / slices, (float)ring);
        }
    }
    int ii = 0;
    for (int j = 0; j < slices; ++j) {
        int b0 = j, b1 = j + 1, t0 = (slices + 1) + j, t1 = (slices + 1) + j + 1;
        idx[ii++] = b0; idx[ii++] = t0; idx[ii++] = b1;
        idx[ii++] = b1; idx[ii++] = t0; idx[ii++] = t1;
    }
    // 顶盖（法线 +Y）
    if (capTop) {
        int center = put(0, h, 0, 0, 1, 0, 0.5f, 0.5f);
        int first = vi;
        for (int j = 0; j <= slices; ++j) {
            float th = 2.0f * PI * (float)j / slices;
            put(std::cos(th) * rTop, h, std::sin(th) * rTop, 0, 1, 0, 0, 0);
        }
        for (int j = 0; j < slices; ++j) { idx[ii++] = center; idx[ii++] = first + j; idx[ii++] = first + j + 1; }
    }
    // 底盖（法线 -Y）
    if (capBot) {
        int center = put(0, 0, 0, 0, -1, 0, 0.5f, 0.5f);
        int first = vi;
        for (int j = 0; j <= slices; ++j) {
            float th = 2.0f * PI * (float)j / slices;
            put(std::cos(th) * rBot, 0, std::sin(th) * rBot, 0, -1, 0, 0, 0);
        }
        for (int j = 0; j < slices; ++j) { idx[ii++] = center; idx[ii++] = first + j + 1; idx[ii++] = first + j; }
    }
    mesh.aVertices = v; mesh.aIndices = idx;
    mesh.aVertexCount = (uint32_t)vc; mesh.aIndexCount = (uint32_t)ic;
    m->computeWorldAABB();
    return m;
}

GN3d::cMODEL *box(float sx, float sy, float sz, float r, float g, float b, float shin = 16.0f) {
    GN3d::cMODEL *m = GN3d::CreateCubeModel(1.0f, r, g, b);
    m->SetScale(sx, sy, sz);
    m->aMeshes[0].aShininess = shin;
    return m;
}

void place(GN3d::cMODEL *m, float x, float y, float z, bool caster = true) {
    m->SetPosition(x, y, z);
    m->computeWorldAABB();
    gModels.push_back(m);
    if (caster) gCasters.push_back(m);
}

void buildScene() {
    // 地板（深色木纹）与背景墙
    gFloor = box(14.0f, 0.1f, 14.0f, 0.10f, 0.085f, 0.075f, 8.0f);
    place(gFloor, 0.0f, -0.05f, 0.0f, false);
    gWall = box(14.0f, 6.0f, 0.1f, 0.13f, 0.13f, 0.16f, 6.0f);
    place(gWall, 0.0f, 3.0f, -1.6f, false);

    // 书桌：桌面 + 四腿
    GN3d::cMODEL *top = box(1.7f, 0.06f, 0.95f, 0.32f, 0.20f, 0.12f, 20.0f);
    place(top, 0.0f, kDeskTop - 0.03f, 0.0f);
    const float lx[4] = {-0.78f, 0.78f, -0.78f, 0.78f};
    const float lz[4] = {-0.40f, -0.40f, 0.40f, 0.40f};
    for (int i = 0; i < 4; ++i) {
        GN3d::cMODEL *leg = box(0.07f, kDeskTop - 0.06f, 0.07f, 0.20f, 0.13f, 0.08f, 12.0f);
        place(leg, lx[i], (kDeskTop - 0.06f) * 0.5f, lz[i]);
    }

    // 台灯（建筑师灯）：底座 + 立柱 + 横臂 + 锥形灯罩 + 灯泡
    const float bx = -0.55f, bz = -0.22f;
    GN3d::cMODEL *base = makeCone(0.13f, 0.15f, 0.04f, 28, 0.16f, 0.16f, 0.18f, true, true);
    place(base, bx, kDeskTop, bz);
    GN3d::cMODEL *post = box(0.035f, 0.42f, 0.035f, 0.20f, 0.20f, 0.22f, 48.0f);
    place(post, bx, kDeskTop + 0.21f, bz);
    // 横臂：从立柱顶伸向桌面中心上方
    GN3d::cMODEL *arm = box(0.46f, 0.03f, 0.03f, 0.20f, 0.20f, 0.22f, 48.0f);
    place(arm, bx + 0.23f, kDeskTop + 0.42f, bz + 0.06f);
    // 灯罩：开口向下的截锥（顶小底大），金属深色
    const float sx = bx + 0.46f, sz = bz + 0.12f, sy = kDeskTop + 0.40f;
    GN3d::cMODEL *shade = makeCone(0.045f, 0.15f, 0.16f, 32, 0.13f, 0.13f, 0.15f, true, false);
    place(shade, sx, sy, sz);
    // 灯泡：亮暖球（bloom 辉光），位于灯罩口内
    gBulbPos = GNg::sVEC3{sx, sy - 0.02f, sz};
    gBulb = GN3d::CreateSphereModel(0.05f, 16, 1.0f, 0.85f, 0.6f);
    gBulb->aMeshes[0].aDiffuse[0] = 0.0f; // 初始熄灭
    gBulb->aMeshes[0].aDiffuse[1] = 0.0f;
    gBulb->aMeshes[0].aDiffuse[2] = 0.0f;
    place(gBulb, gBulbPos.x, gBulbPos.y, gBulbPos.z);

    // 桌上物件
    // 翻开的书：两片微翘的白页 + 封面
    GN3d::cMODEL *cover = box(0.30f, 0.02f, 0.22f, 0.35f, 0.12f, 0.10f, 10.0f);
    place(cover, 0.05f, kDeskTop + 0.01f, 0.12f);
    GN3d::cMODEL *pageL = box(0.145f, 0.015f, 0.21f, 0.92f, 0.90f, 0.85f, 6.0f);
    GNg::sQUAT q; GNg::sVEC3 axis{0, 0, 1}; GNg::QuatFromAxisAngle(&q, &axis, 0.10f);
    pageL->SetRotation(q);
    place(pageL, -0.025f, kDeskTop + 0.03f, 0.12f);
    GN3d::cMODEL *pageR = box(0.145f, 0.015f, 0.21f, 0.92f, 0.90f, 0.85f, 6.0f);
    GNg::QuatFromAxisAngle(&q, &axis, -0.10f);
    pageR->SetRotation(q);
    place(pageR, 0.125f, kDeskTop + 0.03f, 0.12f);
    // 书堆（三本，错色）
    const float bc[3][3] = {{0.18f, 0.30f, 0.45f}, {0.45f, 0.22f, 0.18f}, {0.22f, 0.38f, 0.26f}};
    for (int i = 0; i < 3; ++i) {
        GN3d::cMODEL *bk = box(0.24f, 0.035f, 0.17f, bc[i][0], bc[i][1], bc[i][2], 14.0f);
        GNg::sVEC3 ay{0, 1, 0}; GNg::QuatFromAxisAngle(&q, &ay, (i - 1) * 0.12f);
        bk->SetRotation(q);
        place(bk, 0.50f, kDeskTop + 0.018f + i * 0.036f, -0.18f);
    }
    // 马克杯（圆柱）
    GN3d::cMODEL *mug = makeCone(0.05f, 0.045f, 0.10f, 24, 0.80f, 0.78f, 0.72f, false, true);
    place(mug, 0.32f, kDeskTop, 0.28f);
    // 球镇纸
    GN3d::cMODEL *ball = GN3d::CreateSphereModel(0.045f, 20, 0.55f, 0.58f, 0.62f);
    ball->aMeshes[0].aShininess = 96.0f;
    place(ball, -0.18f, kDeskTop + 0.045f, 0.30f);
}

void initLights() {
    gLight = new GN3d::cLIGHT();
    // 月光：弱冷方向光（不投影，仅补光）
    gLight->AddDirectional(GNg::sVEC3{0.3f, -0.8f, 0.4f}, GNg::sVEC3{0.45f, 0.52f, 0.70f}, 0.10f, false);
    // 台灯：暖色点光（灯泡）+ cube shadow（软半影），灯罩遮挡向上光，初始强度 0（熄灭）
    gLampSlot = gLight->AddPoint(gBulbPos, 8.0f, kWarm, 0.0f, true, 0.10f);
    gLight->SetAmbient(GNg::sVEC3{0.040f, 0.044f, 0.058f}, 1.0f);
    gLight->ClearCubeShadowRanges();
}

// 暖光动画曲线：暗场 → 闪烁 → 渐亮 → 稳定（含细微暖颤）
float lampIntensity(float t) {
    if (t < 0.4f) return 0.0f;
    if (t < 0.75f) {
        // 开关闪烁：确定性尖峰
        float f = (t - 0.4f) / 0.35f;
        float spike = (std::sin(f * 47.0f) > 0.3f) ? 1.0f : 0.12f;
        return kLampFull * 0.7f * spike;
    }
    if (t < 2.0f) {
        float f = (t - 0.75f) / 1.25f;
        float smooth = f * f * (3.0f - 2.0f * f); // smoothstep
        return kLampFull * (0.25f + 0.75f * smooth);
    }
    return kLampFull * (1.0f + 0.02f * std::sin(t * 9.0f)); // 稳定 + 微颤
}

bool buildCamera(float aspect, GNg::sMAT44 &viewProj, GNg::sMAT44 &view, GNg::sVEC3 &eye) {
    // 缓慢环绕：方位角 0.55 → 0.92 rad（约 21°），俯仰微变
    float az = 0.55f + 0.37f * (gTime / 10.0f);
    float el = 0.34f + 0.04f * std::sin(gTime * 0.5f);
    gCamera.SetOrbit(GNg::sVEC3{0.0f, 0.82f, 0.0f}, 2.3f, az, el);
    gCamera.SetFov(46.0f);
    gCamera.SetNF(0.05f, 60.0f);
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
    GNg::GN_VkSetEnvironmentLighting(GN3d::eENV_LIGHTING_INDOOR_UNIFORM, 0.05f);
    GNg::GN_VkSetExposure(0.45f);
    GNg::GN_VkSetBloom(0.30f, 1.0f, 0.45f);
    std::fprintf(stderr, "[deskLamp] 场景就绪：%zu 模型，%zu 投影体，灯泡槽 %d\n",
                 gModels.size(), gCasters.size(), gLampSlot);
}

GN_CALL void AppUnInit() {
    for (size_t i = 0; i < gModels.size(); ++i) delete gModels[i];
    gModels.clear();
    gCasters.clear();
    delete gLight;
    gLight = nullptr;
}

GN_CALL void AppRun(unsigned int nTime) { (void)nTime; }
GN_CALL void AppDisplay3D(unsigned int nTime) { (void)nTime; }

GN_CALL void AppDisplay2D(unsigned int nTime) {
    (void)nTime;
    if (!gLight) return;

    // 暖光动画 + 灯泡自发光同步
    float inten = lampIntensity(gTime);
    gLight->SetIntensity(gLampSlot, inten);
    float glow = inten / kLampFull;
    gBulb->aMeshes[0].aDiffuse[0] = 3.2f * glow;
    gBulb->aMeshes[0].aDiffuse[1] = 2.4f * glow;
    gBulb->aMeshes[0].aDiffuse[2] = 1.5f * glow;

    const float aspect = (float)gWidth / (float)gHeight;
    GNg::sMAT44 view, viewProj;
    GNg::sVEC3 eye;
    if (!buildCamera(aspect, viewProj, view, eye)) return;

    // cube shadow（主 pass 之前）
    const float cubeNear = 0.05f, cubeFar = 6.0f;
    gLight->AssignCubeShadowLayers(cubeNear, cubeFar, 1u);
    GNg::GN_VkResetCubeShadowLightCount();
    for (int i = 0; i < (int)GN_MAX_LIGHTS; ++i) {
        const int32_t layer = (int32_t)gLight->GetUBO()->cubeShadowRange[i][2];
        if (layer < 0) continue;
        GNg::sVEC3 lp{i == gLampSlot ? gBulbPos.x : 0.0f, i == gLampSlot ? gBulbPos.y : 0.0f,
                      i == gLampSlot ? gBulbPos.z : 0.0f};
        if (!GNg::GN_VkCubeShadowBeginForLight((uint32_t)layer, lp, cubeNear, cubeFar)) continue;
        for (uint32_t face = 0; face < 6; ++face) {
            GNg::GN_VkCubeShadowFaceBegin(lp, face, cubeNear, cubeFar);
            for (size_t c = 0; c < gCasters.size(); ++c) gCasters[c]->DrawShadow();
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
    gWall->DrawPBRSorted(eye);
    for (size_t i = 0; i < gModels.size(); ++i) {
        if (gModels[i] == gFloor || gModels[i] == gWall) continue;
        gModels[i]->DrawPBRSorted(eye);
    }
    frame.End();

    gTime += 1.0f / 30.0f;
}

} // extern "C"

namespace {
void onFrame(int index) {
    int w = 0, h = 0, size = 0;
    unsigned char *rgba = GN_GrabFrame(&w, &h, &size);
    if (!rgba || w <= 0 || h <= 0) return;
    char path[512];
    std::snprintf(path, sizeof(path), "%s/frame_%05d.png", gOutDir, index);
    writePng(path, rgba, w, h);
    GN_GrabFrameFree(rgba);
    if (index % 30 == 0) std::fprintf(stderr, "[deskLamp] 帧 %d/%d\n", index, gFrames);
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
    if (std::system(cmd) != 0) std::fprintf(stderr, "[deskLamp] 清理目录失败\n");
    std::fprintf(stderr, "[deskLamp] %dx%d，%d 帧 → %s\n", gWidth, gHeight, gFrames, gOutDir);
    GN_InitLoadCallBackFunc(AppInit, AppUnInit);
    GN_InitLoopCallBackFunc(AppDisplay2D, AppDisplay3D, AppRun);
    if (!GN_StartRunHeadless((short)gWidth, (short)gHeight, gFrames, onFrame)) {
        std::fprintf(stderr, "[deskLamp] 初始化失败（无 Vulkan 设备？）\n");
        return 2;
    }
    std::printf("[deskLamp] 完成：%d 帧\n", gFrames);
    return 0;
}