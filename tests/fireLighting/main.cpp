// tests/fireLighting/main.cpp — 灯光 / 火焰场景 → 逐帧 PNG（电影渲染）
//
// 这一段专门展示**光照**：同一堆火、同一套几何，靠改变光源组合与后处理，走出四种完全
// 不同的氛围。全部使用引擎现有接口，没有为这段视频新增私有渲染路径：
//
//   A 火光（0.0–3.0s）  只有火焰的点光源。环境光 0、GI 关、无方向光 —— 证明画面里每一
//                       一点亮度都来自那几盏点光。
//   B 月光（3.0–5.5s）  冷蓝方向光渐亮 + 补光。火光相对减弱，色温对比拉到最大。
//   C 聚光（5.5–8.0s）  一盏暖白聚光灯从天顶扫过场景（会 Moving）。方向光收回。
//   D 爆发（8.0–10.5s） 全光源收起，只剩火 + 火星爆发（发射率与初速拉高），Bloom 加强。
//
// 火焰怎么做的（三段，缺一不可）：
//   1. **粒子**：GPU 计算模拟（`GN_VkParticleSimulate`），重力**朝上**（浮力）+ 阻尼，
//      寿命短的粒子上升快 —— 这是"火向上窜"的来源。CPU 每帧回收死粒子并重新播种。
//   2. **点光源**：4 盏橙色点光在火堆内做低频抖动（每盏相位不同）。粒子本身不受光，
//      真正被照亮的是地面、石头、木头 —— 没有这几盏灯，火焰只是贴图。
//   3. **Bloom**：阈值压到 0.55、强度提到 1.4，让粒子与高光溢出（`GN_VkSetBloom`）。
#include <GN.h>
#include "GN_Entity.h"
#include "GN_Scene.h"
#include "GN_GameClock.h"
#include "GN_3DparticleGPU.h"

#include <png.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {

// ── 配置 ──
int gWidth = 1280;
int gHeight = 720;
int gFrames = 315;                 // 30 fps × 10.5 s
int gSkipFrames = 0;               // 快进：只跑模拟不写图（抽查某一阶段用）
float gFrameDt = 1.0f / 30.0f;
const char *gOutDir = ".local/video/fire-frames";
bool gShadows = false; // 点光不支持阴影；方向光阴影在这种火光主导的画面里反而干扰
bool gVerbose = false;

// 时间轴（秒）
const float kPhaseFireEnd = 3.0f;
const float kPhaseMoonEnd = 5.5f;
const float kPhaseSpotEnd = 8.0f;

// ── 场景 ──
struct sStatic {
    GN3d::cMODEL *aModel;
};
std::vector<sStatic> gProps; // 石头 / 木柴 / 地形
GN3d::cLIGHT *gLight = nullptr;
GN3d::cCAMERA gCamera;
GN3d::cSCENE gScene;
GN::cGAME_CLOCK gClock;

// 点光句柄（cLIGHT 的索引）
int gFireLight[4] = {-1, -1, -1, -1};
int gMoonLight = -1;
int gFillLight = -1;
int gSpotLight = -1;

// 粒子状态（CPU 侧权威副本：GPU 只做积分，生死与播种在 CPU）
const uint32_t kParticles = 6144;
std::vector<GN3d::sGPU_PARTICLE> gParticles;
GNg::sVEC3 gFireOrigin = {0.0f, 0.62f, 0.0f};
uint32_t gSpawnCursor = 0;
float gTime = 0.0f;
float gEmitRate = 620.0f; // 每秒新生粒子数（按帧折算）

// ── 确定性随机 ──
struct sRNG {
    uint32_t aState;
    explicit sRNG(uint32_t seed) : aState(seed ? seed : 1u) {}
    float unit() {
        aState ^= aState << 13;
        aState ^= aState >> 17;
        aState ^= aState << 5;
        return (float)(aState >> 8) / (float)(1u << 24);
    }
    float range(float low, float high) { return low + (high - low) * unit(); }
};
sRNG gRng(0x5eed1234u);

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

// ── 场景构建 ──
void addBox(const GNg::sVEC3 &pos, const GNg::sVEC3 &half, float r, float g, float b,
            float rotY = 0.0f) {
    sStatic prop;
    prop.aModel = GN3d::CreateCubeModel(1.0f, r, g, b);
    prop.aModel->SetScale(half.x * 2.0f, half.y * 2.0f, half.z * 2.0f);
    prop.aModel->SetPosition(pos.x, pos.y, pos.z);
    if (rotY != 0.0f) prop.aModel->SetRotation(GNg::sQUAT(0.0f, std::sin(rotY * 0.5f), 0.0f,
                                                         std::cos(rotY * 0.5f)));
    prop.aModel->computeWorldAABB();
    gProps.push_back(prop);
}

void buildScene() {
    // 地面：暗色（火光打上去才有层次）
    addBox(GNg::sVEC3{0.0f, -0.05f, 0.0f}, GNg::sVEC3{14.0f, 0.05f, 14.0f}, 0.10f, 0.095f, 0.09f);

    // 火塘：一圈石头（12 块，半径 1.15）
    for (int i = 0; i < 12; ++i) {
        const float angle = (float)i * 6.2831853f / 12.0f;
        const float x = std::cos(angle) * 1.15f;
        const float z = std::sin(angle) * 1.15f;
        const float shade = 0.30f + 0.05f * (float)(i % 3);
        addBox(GNg::sVEC3{x, 0.13f, z}, GNg::sVEC3{0.17f, 0.13f, 0.17f}, shade, shade * 0.96f,
               shade * 0.90f, angle);
    }

    // 柴堆：几根交叉的木头（能看出火光的方向性）
    addBox(GNg::sVEC3{-0.18f, 0.30f, 0.0f}, GNg::sVEC3{0.55f, 0.07f, 0.07f}, 0.26f, 0.16f, 0.09f,
           0.42f);
    addBox(GNg::sVEC3{0.20f, 0.30f, 0.0f}, GNg::sVEC3{0.55f, 0.07f, 0.07f}, 0.24f, 0.15f, 0.08f,
           -0.55f);
    addBox(GNg::sVEC3{0.0f, 0.42f, 0.18f}, GNg::sVEC3{0.48f, 0.06f, 0.06f}, 0.22f, 0.14f, 0.08f,
           1.57f);

    // 周围：几块木箱与一堵矮墙，用来"接"火光与月光
    addBox(GNg::sVEC3{-3.4f, 0.45f, -2.2f}, GNg::sVEC3{0.45f, 0.45f, 0.45f}, 0.42f, 0.30f, 0.18f,
           0.3f);
    addBox(GNg::sVEC3{-2.9f, 0.35f, -3.1f}, GNg::sVEC3{0.35f, 0.35f, 0.35f}, 0.38f, 0.27f, 0.16f,
           -0.6f);
    addBox(GNg::sVEC3{3.1f, 0.5f, -2.6f}, GNg::sVEC3{0.5f, 0.5f, 0.5f}, 0.36f, 0.26f, 0.15f);
    addBox(GNg::sVEC3{2.7f, 0.28f, -3.4f}, GNg::sVEC3{0.28f, 0.28f, 0.28f}, 0.40f, 0.29f, 0.17f,
           0.9f);
    // 矮墙（-z 方向），月光会从墙后打进来
    addBox(GNg::sVEC3{0.0f, 0.6f, -5.2f}, GNg::sVEC3{6.0f, 0.6f, 0.18f}, 0.34f, 0.325f, 0.31f);
    // 一根立木（聚光灯扫过时能在墙上投出长影）
    addBox(GNg::sVEC3{2.2f, 1.2f, -1.4f}, GNg::sVEC3{0.12f, 1.2f, 0.12f}, 0.30f, 0.22f, 0.14f);
}

void initLights() {
    gLight = new GN3d::cLIGHT();
    // 环境光 0：A 段的画面亮度全部来自点光，这是"火光照明"的证据。
    gLight->SetAmbient(GNg::sVEC3{0.0f, 0.0f, 0.0f}, 1.0f);

    // 四盏火点光：小范围、暖橙、位置略有偏移 ⇒ 照亮石头内侧的弧面
    const float offsets[4][3] = {{0.16f, 0.0f, 0.10f},
                                 {-0.14f, 0.06f, 0.12f},
                                 {0.02f, 0.10f, -0.16f},
                                 {-0.06f, 0.16f, -0.02f}};
    for (int i = 0; i < 4; ++i) {
        gFireLight[i] = gLight->AddPoint(
            GNg::sVEC3{gFireOrigin.x + offsets[i][0], gFireOrigin.y + offsets[i][1],
                       gFireOrigin.z + offsets[i][2]},
            6.5f, GNg::sVEC3{1.0f, 0.52f, 0.16f}, 2.4f);
    }

    // 月光：冷蓝方向光（B 段渐亮）
    gMoonLight = gLight->AddDirectional(GNg::sVEC3{0.34f, -0.72f, -0.60f},
                                        GNg::sVEC3{0.32f, 0.48f, 1.0f}, 0.0f, false);
    // 补光：极弱的中性光，避免背光面死黑（A 段也为 0）
    gFillLight = gLight->AddDirectional(GNg::sVEC3{-0.55f, -0.25f, 0.75f},
                                        GNg::sVEC3{0.55f, 0.60f, 0.85f}, 0.0f, false);
    // 聚光灯：从天顶斜扫
    gSpotLight = gLight->AddSpot(GNg::sVEC3{0.0f, 7.5f, 0.0f}, GNg::sVEC3{0.0f, -1.0f, 0.0f},
                                 16.0f, 0.15f, 0.38f, GNg::sVEC3{1.0f, 0.95f, 0.86f}, 0.0f, false);

    std::fprintf(stderr,
                 "[fireLighting] 光源：4×point=%d,%d,%d,%d moon=%d fill=%d spot=%d\n",
                 gFireLight[0], gFireLight[1], gFireLight[2], gFireLight[3], gMoonLight,
                 gFillLight, gSpotLight);
}

// ── 粒子 ──
void initParticles() {
    GN3d::sPARTICLE_SIM_DESC sim;
    // 重力朝上（浮力）+ 阻尼：火苗上升、逐渐变慢 —— 这是"火"与"喷泉"的区别
    sim.aGravity[0] = 0.0f;
    sim.aGravity[1] = 3.4f;
    sim.aGravity[2] = 0.0f;
    sim.aDrag = 0.85f;
    if (!GNg::GN_VkParticleCreate(kParticles, sim)) {
        std::fprintf(stderr, "[fireLighting] 粒子系统创建失败\n");
        return;
    }
    gParticles.assign(kParticles, GN3d::sGPU_PARTICLE());
    std::memset(gParticles.data(), 0, sizeof(GN3d::sGPU_PARTICLE) * kParticles);
    GNg::GN_VkParticleUpload(gParticles.data(), kParticles, 0);
}

void spawnOne(GN3d::sGPU_PARTICLE &p, float burst) {
    // 火舌根部：在一个小圆盘内随机，速度主要向上 + 一点外扩
    const float angle = gRng.range(0.0f, 6.2831853f);
    const float radius = std::sqrt(gRng.unit()) * 0.22f;
    p.aPos[0] = gFireOrigin.x + std::cos(angle) * radius;
    p.aPos[1] = gFireOrigin.y - 0.22f + gRng.range(0.0f, 0.10f);
    p.aPos[2] = gFireOrigin.z + std::sin(angle) * radius;
    const float rise = gRng.range(1.2f, 3.4f) * burst;
    p.aVel[0] = std::cos(angle) * radius * 1.4f + gRng.range(-0.25f, 0.25f);
    p.aVel[1] = rise;
    p.aVel[2] = std::sin(angle) * radius * 1.4f + gRng.range(-0.25f, 0.25f);
    p.aAge = 0.0f;
    // 8% 是"火星"：寿命长、初速高、尺寸小 ⇒ 会窜到火上方再慢慢飘落
    // 火星：比例小、寿命短、尺寸小。第一版给到 1.9–3.2 s，结果它们飘到画面顶部时
    // 才刚过寿命 1/3，颜色仍是亮黄 ⇒ 看起来像一堆散景圆点，而不是火星。
    const bool ember = gRng.unit() < 0.05f;
    if (ember) {
        p.aLife = gRng.range(1.05f, 1.85f);
        p.aVel[1] = gRng.range(2.6f, 4.0f);
        p.aSize = gRng.range(0.020f, 0.045f);
    } else {
        p.aLife = gRng.range(0.42f, 0.95f) / (0.7f + 0.3f * burst);
        p.aSize = gRng.range(0.11f, 0.22f) * (0.7f + 0.5f * burst);
    }
    p.aSeed = gRng.unit();
    p.aAlive = 1.0f;
    p.aColor[0] = 1.0f;
    p.aColor[1] = 1.0f;
    p.aColor[2] = 1.0f;
    p.aColor[3] = 1.0f;
}

// 每帧：GPU 积分 → 回读 → 回收死粒子并按发射率播种 → 上传
void updateParticles(float dt, float burst) {
    if (!GNg::GN_VkParticleSimulate(dt)) return;
    uint32_t count = 0;
    const GN3d::sGPU_PARTICLE *gpu = GNg::GN_VkParticleData(&count);
    if (gpu != nullptr && count == kParticles) {
        std::memcpy(gParticles.data(), gpu, sizeof(GN3d::sGPU_PARTICLE) * kParticles);
    }

    // 播种：按发射率算出本帧应新建多少个，从死槽里取（游标轮转，避免总是抢同几个槽）
    const float rate = gEmitRate * burst;
    static float accumulator = 0.0f;
    accumulator += rate * dt;
    uint32_t budget = (uint32_t)accumulator;
    accumulator -= (float)budget;

    uint32_t scanned = 0;
    while (budget > 0 && scanned < kParticles) {
        const uint32_t index = gSpawnCursor % kParticles;
        ++gSpawnCursor;
        ++scanned;
        if (gParticles[index].aAlive >= 0.5f) continue; // 活的跳过
        spawnOne(gParticles[index], burst);
        --budget;
    }
    GNg::GN_VkParticleUpload(gParticles.data(), kParticles, 0);
}

// ── 光照时间轴 ──
float smoothstep01(float x) {
    if (x <= 0.0f) return 0.0f;
    if (x >= 1.0f) return 1.0f;
    return x * x * (3.0f - 2.0f * x);
}

// 火光抖动：三个不同频率的正弦叠加（比随机数更像火焰，且可复现）
float flicker(float t, float phase) {
    const float a = std::sin(t * 11.3f + phase) * 0.5f;
    const float b = std::sin(t * 23.7f + phase * 2.1f) * 0.3f;
    const float c = std::sin(t * 5.1f + phase * 0.7f) * 0.2f;
    return 1.0f + (a + b + c) * 0.28f;
}

void updateLighting() {
    const float t = gTime;

    // ── A/B/C/D 四段的权重 ──
    const float fireWeight = 1.0f; // 火一直在烧
    const float moonWeight =
        smoothstep01((t - kPhaseFireEnd) / 1.2f) * (1.0f - smoothstep01((t - kPhaseSpotEnd + 0.6f) / 1.0f));
    const float spotPhase = smoothstep01((t - kPhaseMoonEnd) / 1.0f) *
                            (1.0f - smoothstep01((t - kPhaseSpotEnd) / 0.8f));
    const float burst = 1.0f + 1.6f * smoothstep01((t - kPhaseSpotEnd) / 1.2f);
    gEmitRate = 620.0f;

    // 火点光：强度抖动 + 位置微动（火舌在动，光源也该动）
    for (int i = 0; i < 4; ++i) {
        if (gFireLight[i] < 0) continue;
        const float phase = (float)i * 1.9f;
        const float f = flicker(t, phase);
        // 月光起来之后把火光压下去：两种主光都满强度时，画面只会变成"什么都亮"，
        // 看不出主光换了。
        const float base = 2.4f * fireWeight * burst * (1.0f - 0.45f * moonWeight);
        gLight->SetIntensity(gFireLight[i], base * f);
        // 色温随抖动偏移：越亮越偏黄白
        const float warm = 0.42f + 0.14f * (f - 1.0f);
        gLight->SetColor(gFireLight[i], GNg::sVEC3{1.0f, warm, 0.12f + 0.06f * (f - 1.0f)});
        gLight->SetPosition(gFireLight[i],
                            GNg::sVEC3{gFireOrigin.x + 0.14f * std::sin(t * 3.1f + phase),
                                        gFireOrigin.y + 0.10f * std::sin(t * 4.7f + phase) + 0.06f,
                                        gFireOrigin.z + 0.14f * std::cos(t * 2.7f + phase)});
    }

    // 月光
    if (gMoonLight >= 0) gLight->SetIntensity(gMoonLight, 2.45f * moonWeight);
    if (gFillLight >= 0) gLight->SetIntensity(gFillLight, 0.42f * moonWeight);

    // 聚光灯：沿一个椭圆扫过场景，锥角同时呼吸
    if (gSpotLight >= 0) {
        const float intensity = 17.0f * spotPhase;
        gLight->SetIntensity(gSpotLight, intensity);
        const float sweep = t - kPhaseMoonEnd;
        const float px = std::sin(sweep * 0.85f) * 3.2f;
        const float pz = std::cos(sweep * 0.62f) * 2.4f;
        gLight->SetPosition(gSpotLight, GNg::sVEC3{px, 7.5f, pz});
        gLight->SetDirection(gSpotLight, GNg::sVEC3{-px * 0.32f, -1.0f, -pz * 0.32f});
    }

    // 环境光：A 段为 0（只靠火光），B/C 段抬一点点冷色，D 段回落到接近 0
    const float ambient = 0.010f * moonWeight + 0.030f * spotPhase;
    gLight->SetAmbient(GNg::sVEC3{0.55f, 0.65f, 1.0f}, ambient);

    // 后处理：夜景压曝光；聚光段提亮；爆发段加强 Bloom
    const float ev = -0.35f + 0.45f * spotPhase + 0.10f * moonWeight;
    GNg::GN_VkSetExposure(ev);
    const float bloomIntensity = 0.72f + 0.50f * smoothstep01((t - kPhaseSpotEnd) / 1.0f);
    GNg::GN_VkSetBloom(bloomIntensity, 0.55f, 0.30f);

    gEmitRate = 620.0f * (1.0f + 0.55f * smoothstep01((t - kPhaseSpotEnd) / 1.0f));
    (void)fireWeight;
}

// ── 相机：绕火塘缓慢环绕，越到后面越低（看清地面上的光斑）──
bool buildCamera(float time, float aspect, GNg::sMAT44 &viewProj, GNg::sMAT44 &view,
                 GNg::sVEC3 &eye, GNg::sVEC3 &right, GNg::sVEC3 &up) {
    const float yaw = 0.30f + time * 0.135f;
    const float distance = 6.4f - std::fmin(time * 0.10f, 1.4f);
    const float pitch = 0.30f - std::fmin(time * 0.008f, 0.12f);
    const GNg::sVEC3 focus = {0.0f, 1.05f, 0.0f};
    gCamera.SetOrbit(focus, distance, yaw, pitch);
    gCamera.SetFov(46.0f);
    gCamera.SetNF(0.08f, 80.0f);
    GNg::sMAT44 projection;
    if (!gCamera.BuildViewProj(aspect, &view, &projection, &viewProj)) return false;
    eye = gCamera.GetCoordV();

    // billboard 用的相机右/上向量：从 view 矩阵的前两行取（右手系、行主序存储）
    right = GNg::sVEC3{view.m[0], view.m[1], view.m[2]};
    up = GNg::sVEC3{view.m[4], view.m[5], view.m[6]};
    return true;
}

} // namespace

// ── 引擎回调 ──
extern "C" {

GN_CALL void AppInit() {
    gClock.SetFixedStep(gFrameDt, 1);
    gCamera.Init();
    buildScene();
    initLights();
    initParticles();
    // 夜景不复用天空 IBL（否则"环境光 0"也还是亮的，火光就显不出来）
    GNg::GN_VkSetEnvironmentLighting(GN3d::eENV_LIGHTING_INDOOR_UNIFORM, 0.02f);
    GNg::GN_VkSetTonemapMode(GN3d::eTONEMAP_ACES);
    GNg::GN_VkSetBloom(0.72f, 0.62f, 0.28f);
    std::fprintf(stderr, "[fireLighting] 道具 %u 个，粒子容量 %u\n", (unsigned)gProps.size(),
                 kParticles);
}

GN_CALL void AppUnInit() {
    GNg::GN_VkParticleDestroy();
    for (size_t i = 0; i < gProps.size(); ++i) delete gProps[i].aModel;
    gProps.clear();
    delete gLight;
    gLight = nullptr;
}

GN_CALL void AppRun(unsigned int nTime) { (void)nTime; }
GN_CALL void AppDisplay3D(unsigned int nTime) { (void)nTime; }

GN_CALL void AppDisplay2D(unsigned int nTime) {
    (void)nTime;
    if (gLight == nullptr) return;

    const float aspect = (float)gWidth / (float)gHeight;

    // 1) 光照时间轴 + 粒子（粒子要先算完，本帧才画得对）
    updateLighting();
    const float burst = 1.0f + 1.6f * smoothstep01((gTime - kPhaseSpotEnd) / 1.2f);
    updateParticles(gFrameDt, burst);

    // 2) 相机
    GNg::sMAT44 view;
    GNg::sMAT44 viewProj;
    GNg::sVEC3 eye;
    GNg::sVEC3 right;
    GNg::sVEC3 up;
    if (!buildCamera(gTime, aspect, viewProj, view, eye, right, up)) return;

    // 3) 3D pass
    GNg::cRENDER_FRAME frame;
    GNg::sRENDER_FRAME_DESC desc;
    desc.aViewProj = &viewProj;
    desc.aCameraPos = &eye;
    desc.aLightUBO = gLight->GetUBO();
    desc.aLightUBOSize = sizeof(GN3d::sLIGHT_UBO);
    desc.aDrawSkybox = false; // 夜空：保留清屏色，天空盒会在火光场景里抢注意力
    if (!frame.Begin(desc)) return;

    // 4) 几何（全部静态，直接画；这里没有动态体，剔除/排序的收益接近零）
    for (size_t i = 0; i < gProps.size(); ++i) gProps[i].aModel->DrawPBRSorted(eye);

    // 5) 火焰粒子：从"炽白"到"暗红+透明"的一段渐变
    GN3d::sPARTICLE_DRAW_PARAMS params;
    std::memset(&params, 0, sizeof(params));
    std::memcpy(params.aViewProj, viewProj.m, sizeof(params.aViewProj));
    params.aCameraRight[0] = right.x;
    params.aCameraRight[1] = right.y;
    params.aCameraRight[2] = right.z;
    params.aCameraUp[0] = up.x;
    params.aCameraUp[1] = up.y;
    params.aCameraUp[2] = up.z;
    // age=0：接近白热（略偏黄，>1 的值交给 Bloom/ACES 处理）
    // 起始亮度压到 ~1.7：第一版用 2.6 时火心整块过曝成白饼（ACES 之后细节全丢）。
    // 火焰的"亮"应该来自叠层与 Bloom，不是单粒子数值大。
    params.aColorStart[0] = 1.75f;
    params.aColorStart[1] = 1.12f;
    params.aColorStart[2] = 0.36f;
    params.aColorStart[3] = 0.62f;
    // age=life：暗红、几乎透明
    params.aColorEnd[0] = 0.55f;
    params.aColorEnd[1] = 0.075f;
    params.aColorEnd[2] = 0.02f;
    params.aColorEnd[3] = 0.0f;
    params.aSize = 0.24f;
    params.aCount = kParticles;
    params.aTime = gTime;
    if (!GNg::GN_VkParticleDraw(params) && gVerbose) {
        std::fprintf(stderr, "[fireLighting] 粒子绘制被拒绝（pass 内？%d）\n",
                     GNg::GN_VkIsIn3DPass() ? 1 : 0);
    }

    frame.End();

    gTime += gFrameDt;
    gClock.Tick(gFrameDt);
}

} // extern "C"

namespace {

void onFrame(int index) {
    int w = 0;
    int h = 0;
    int size = 0;
    unsigned char *rgba = GN_GrabFrame(&w, &h, &size);
    if (rgba == nullptr || w <= 0 || h <= 0) {
        std::fprintf(stderr, "[fireLighting] 第 %d 帧回读失败\n", index);
        return;
    }
    char path[512];
    std::snprintf(path, sizeof(path), "%s/frame_%05d.png", gOutDir, index);
    if (!writePng(path, rgba, w, h)) std::fprintf(stderr, "[fireLighting] 写 %s 失败\n", path);
    GN_GrabFrameFree(rgba);
    if (index % 15 == 0) std::fprintf(stderr, "[fireLighting] 帧 %d/%d\n", index, gFrames);
}

} // namespace

int main(int argc, char **argv) {
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--out") == 0 && i + 1 < argc) gOutDir = argv[++i];
        else if (std::strcmp(argv[i], "--frames") == 0 && i + 1 < argc) gFrames = std::atoi(argv[++i]);
        else if (std::strcmp(argv[i], "--size") == 0 && i + 2 < argc) {
            gWidth = std::atoi(argv[++i]);
            gHeight = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--fps") == 0 && i + 1 < argc) {
            gFrameDt = 1.0f / (float)std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--start") == 0 && i + 1 < argc) {
            gSkipFrames = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--verbose") == 0) gVerbose = true;
    }
    if (gFrames < 1) gFrames = 1;

    char cmd[512];
    std::snprintf(cmd, sizeof(cmd), "mkdir -p '%s' && rm -f '%s'/*.png", gOutDir, gOutDir);
    if (std::system(cmd) != 0) std::fprintf(stderr, "[fireLighting] 清理输出目录失败\n");

    std::fprintf(stderr, "[fireLighting] %dx%d，%d 帧（%.1f s @ %.0f fps）→ %s\n", gWidth, gHeight,
                 gFrames, (float)gFrames * gFrameDt, 1.0f / gFrameDt, gOutDir);

    // 快进：先把场景推进 gSkipFrames 帧（不写图），让粒子/时间轴到达目标阶段。
    const int totalFrames = gFrames + gSkipFrames;
    GN_InitLoadCallBackFunc(AppInit, AppUnInit);
    GN_InitLoopCallBackFunc(AppDisplay2D, AppDisplay3D, AppRun);
    if (!GN_StartRunHeadless((short)gWidth, (short)gHeight, totalFrames,
                             [](int index) {
                                 if (index < gSkipFrames) return; // 只推进，不落盘
                                 onFrame(index - gSkipFrames);
                             })) {
        std::fprintf(stderr, "[fireLighting] 初始化失败（无 Vulkan 设备？）\n");
        return 2;
    }
    std::printf("[fireLighting] 完成：%d 帧 → %s\n", gFrames, gOutDir);
    return 0;
}
