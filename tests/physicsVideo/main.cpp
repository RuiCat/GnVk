// tests/physicsVideo/main.cpp — 多实体物理碰撞场景 → 逐帧 PNG（电影渲染）
//
// 目的：把 P1–P6 攒下来的能力**一次性用起来**，产出一段能看的画面证据：
//   物理（cPHYSICS_WORLD / cPHYSICS_CONTROLLER 快照插值）
//   + 实体（cENTITY：网格控制器 LOD/剔除/排序）
//   + 场景（cSCENE：统一剔除 / LOD / 排序，共享物理控制器）
//   + 渲染编排（cRENDER_FRAME：阴影级联 → 3D pass → 收尾）
//   + 时钟（cGAME_CLOCK：60Hz 物理 / 30fps 输出，alpha 插值）
//
// 输出：每帧写一张 PNG 到 `--out` 目录，之后由 make physicsVideo 调 ffmpeg 编码成 mp4。
//
// 运行：GN_HEADLESS 由 GN_StartRunHeadless 提供，不需要窗口/显示；需要 Vulkan 设备
//      （容器里用 lavapipe 软件实现）。纯 CPU 物理 + 软件光栅，任何机器都能跑。
#include <GN.h>
#include "GN_Entity.h"
#include "GN_Scene.h"
#include "GN_GameClock.h"
#include "GN_ShadowCascade.h"

#include <png.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {

// ── 配置（可用环境变量覆盖，默认值对应一段 8 秒 720p 视频）──
int gWidth = 1280;
int gHeight = 720;
int gFrames = 240; // 30 fps × 8 s
const char *gOutDir = ".local/video/frames";
bool gShadows = true;
bool gSkybox = false; // 关掉天空盒：物理演示用深色背景更干净（天空很亮，会盖掉主体）
bool gVerbose = false;

// ── 场景状态 ──
// cENTITY 含 unique_ptr（不可拷贝），所以用指针容器而不是值容器。
// 这本身是个有用的提醒：实体是"身份"而不是"值"，复制一个实体没有意义。
struct sBody {
    GN3d::cENTITY *aEntity;
    GN3d::cMODEL *aModel;
    uint32_t aHandle;
    bool aDynamic;
    sBody() : aEntity(nullptr), aModel(nullptr), aHandle(0), aDynamic(false) {}
};

std::vector<sBody> gBodies;
GN3d::cPHYSICS_WORLD gWorld;
GN3d::cPHYSICS_CONTROLLER gPhysics; // 共享控制器：整个场景只有它推进物理
GN3d::cSCENE gScene;
GN3d::cLIGHT *gLight = nullptr;
GN3d::cCAMERA gCamera;
GN::cGAME_CLOCK gClock;
GN3d::sSHADOW_STATE gShadow;
GN3d::cMODEL *gGround = nullptr; // 地面单独画（不进 scene：它不需要剔除/排序）
GNg::sVEC3 gFocus = {0.0f, 1.2f, 0.0f};

// ── PNG 写出（RGBA8 从上到下）──
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
    png_set_compression_level(png, 3); // 帧多，速度优先
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

// ── 造场景 ──
void addStaticBox(const GNg::sVEC3 &pos, const GNg::sVEC3 &half, float r, float g, float b) {
    sBody body;
    body.aEntity = new GN3d::cENTITY();
    body.aModel = GN3d::CreateCubeModel(1.0f, r, g, b);
    body.aModel->SetScale(half.x * 2.0f, half.y * 2.0f, half.z * 2.0f);
    body.aModel->SetPosition(pos.x, pos.y, pos.z);
    body.aEntity->aMesh.SetModel(body.aModel);
    body.aEntity->aMesh.SetSortTransparent(false);
    body.aHandle = gWorld.AddBody(GN3d::MakeBoxBody(GN3d::eBODY_STATIC, pos, half, 0.0f));
    body.aDynamic = false;
    // 静态体不登记到物理控制器（它们不动，也就不需要插值）
    gScene.Add(body.aEntity);
    gBodies.push_back(body);
}

void addDynamicBox(const GNg::sVEC3 &pos, float halfSize, float r, float g, float b) {
    sBody body;
    body.aEntity = new GN3d::cENTITY();
    body.aModel = GN3d::CreateCubeModel(halfSize * 2.0f, r, g, b);
    body.aModel->SetPosition(pos.x, pos.y, pos.z);
    body.aEntity->aMesh.SetModel(body.aModel);
    body.aHandle = gWorld.AddBody(GN3d::MakeBoxBody(GN3d::eBODY_DYNAMIC, pos,
                                                    GNg::sVEC3{halfSize, halfSize, halfSize},
                                                    1.0f));
    body.aDynamic = true;
    body.aEntity->aId = (uint32_t)(gBodies.size() + 1);
    gPhysics.AttachBody(body.aEntity->aId, body.aHandle);
    gScene.Add(body.aEntity);
    gBodies.push_back(body);
}

void addDynamicSphere(const GNg::sVEC3 &pos, float radius, float r, float g, float b,
                      const GNg::sVEC3 &velocity, float mass = 1.2f) {
    sBody body;
    body.aEntity = new GN3d::cENTITY();
    body.aModel = GN3d::CreateSphereModel(radius, 16, r, g, b);
    body.aModel->SetPosition(pos.x, pos.y, pos.z);
    body.aEntity->aMesh.SetModel(body.aModel);
    GN3d::sRIGID_BODY rigid = GN3d::MakeSphereBody(GN3d::eBODY_DYNAMIC, pos, radius, mass);
    rigid.aLinVel = velocity;
    body.aHandle = gWorld.AddBody(rigid);
    body.aDynamic = true;
    body.aEntity->aId = (uint32_t)(gBodies.size() + 1);
    gPhysics.AttachBody(body.aEntity->aId, body.aHandle);
    gScene.Add(body.aEntity);
    gBodies.push_back(body);
}

// 确定性伪随机（同样的种子 → 同样的画面，便于回归）
struct sLCG {
    uint32_t aState;
    explicit sLCG(uint32_t seed) : aState(seed ? seed : 1u) {}
    float unit() {
        aState = aState * 1664525u + 1013904223u;
        return (float)(aState >> 8) / (float)(1u << 24);
    }
    float range(float low, float high) { return low + (high - low) * unit(); }
};

void buildScene() {
    gWorld.SetFixedTimestep(1.0f / 60.0f, 4);
    gWorld.SetGravity(GNg::sVEC3{0.0f, -9.81f, 0.0f});
    gWorld.setContinuousCollision(true); // 高处落下时不穿地

    // ── 地面：20×20 的静态盒（顶面 y = 0），模型单独画 ──
    gGround = GN3d::CreateCubeModel(1.0f, 0.10f, 0.105f, 0.115f);
    gGround->SetScale(20.0f, 1.0f, 20.0f);
    gGround->SetPosition(0.0f, -0.5f, 0.0f);
    gGround->computeWorldAABB();
    gWorld.AddBody(GN3d::MakeBoxBody(GN3d::eBODY_STATIC, GNg::sVEC3{0.0f, -0.5f, 0.0f},
                                     GNg::sVEC3{10.0f, 0.5f, 10.0f}, 0.0f));

    // ── 四周围墙（把东西留在画面里）──
    addStaticBox(GNg::sVEC3{0.0f, 0.6f, -9.6f}, GNg::sVEC3{9.6f, 0.6f, 0.2f}, 0.155f, 0.16f, 0.185f);
    addStaticBox(GNg::sVEC3{0.0f, 0.6f, 9.6f}, GNg::sVEC3{9.6f, 0.6f, 0.2f}, 0.155f, 0.16f, 0.185f);
    addStaticBox(GNg::sVEC3{-9.6f, 0.6f, 0.0f}, GNg::sVEC3{0.2f, 0.6f, 9.6f}, 0.155f, 0.16f, 0.185f);
    addStaticBox(GNg::sVEC3{9.6f, 0.6f, 0.0f}, GNg::sVEC3{0.2f, 0.6f, 9.6f}, 0.155f, 0.16f, 0.185f);

    // ── 金字塔：底层 5×5 缺角 → 递减，共 55 个箱子 ──
    // 用色相环上均匀取色，堆起来层次分明；边长 0.7，层高 0.72 留 2 cm 缝隙。
    const float half = 0.32f;
    int layer = 0;
    for (int ring = 5; ring >= 1; ring -= 1) {
        const float y = half + (float)layer * 0.655f;
        const float span = (float)(ring - 1) * 0.655f * 0.5f;
        for (int iz = 0; iz < ring; ++iz) {
            for (int ix = 0; ix < ring; ++ix) {
                const float x = -span + (float)ix * 0.655f;
                const float z = -span + (float)iz * 0.655f;
                // 色相：按高度分层（层内按行列微调）
                const float hue = 0.02f + 0.62f * ((float)layer / 5.0f);
                const float light = 0.55f + 0.06f * (float)(ix - iz);
                float r = 0.0f;
                float g = 0.0f;
                float b = 0.0f;
                // HSV → RGB（h ∈ [0,1)）
                const float hh = hue * 6.0f;
                const int sector = (int)hh % 6;
                const float f = hh - std::floor(hh);
                const float p = light * (1.0f - 0.55f);
                const float q = light * (1.0f - 0.55f * f);
                const float t = light * (1.0f - 0.55f * (1.0f - f));
                switch (sector) {
                case 0: r = light; g = t; b = p; break;
                case 1: r = q; g = light; b = p; break;
                case 2: r = p; g = light; b = t; break;
                case 3: r = p; g = q; b = light; break;
                case 4: r = t; g = p; b = light; break;
                default: r = light; g = p; b = q; break;
                }
                addDynamicBox(GNg::sVEC3{x, y, z}, half, r, g, b);
            }
        }
        ++layer;
    }

    // ── 从正上方砸下来的重球（真正的"拆塔"）：小范围内密集落点 + 向下初速 ──
    sLCG rng(20261125u);
    for (int i = 0; i < 16; ++i) {
        const float angle = (float)i * 0.3927f; // 每 22.5°
        const float ring = (i % 3 == 0) ? 0.0f : ((i % 3 == 1) ? 0.55f : 1.05f);
        const float x = std::cos(angle) * ring;
        const float z = std::sin(angle) * ring;
        const float y = 11.0f + (float)(i % 5) * 0.9f;
        const GNg::sVEC3 velocity = {x * 0.35f, -8.5f - rng.unit() * 2.0f, z * 0.35f};
        addDynamicSphere(GNg::sVEC3{x, y, z}, 0.34f, 0.96f, 0.93f, 0.86f, velocity, 4.0f);
    }

    // 一个大球从侧面砸进来（视频的"主角"）
    addDynamicSphere(GNg::sVEC3{-10.5f, 1.15f, 0.0f}, 0.75f, 1.0f, 0.42f, 0.22f,
                     GNg::sVEC3{15.0f, -0.2f, 0.35f}, 8.0f);
}

void initLights() {
    gLight = new GN3d::cLIGHT();
    if (std::getenv("VIDEO_NOLIGHT") != nullptr) {
        // 诊断：完全不给光（环境光 0、无方向光），并且关掉 GI。画面应当接近全黑。
        gLight->SetAmbient(GNg::sVEC3{0.0f, 0.0f, 0.0f}, 0.0f);
        GNg::GN_VkEnableGI(false);
        return;
    }
    if (std::getenv("VIDEO_LIGHT_DEBUG") != nullptr) {
        // 诊断：只留一盏有影方向光 + 极弱环境光。阴影若正常，未被照到的地面应当接近全黑。
        gLight->AddDirectional(GNg::sVEC3{0.46f, -0.74f, -0.49f}, GNg::sVEC3{1, 1, 1}, 2.0f, true);
        gLight->SetAmbient(GNg::sVEC3{0.02f, 0.02f, 0.02f}, 1.0f);
        return;
    }
    const GNg::sVEC3 sunDir = {0.46f, -0.74f, -0.49f}; // 从场景**对面**照过来 ⇒ 影子朝镜头方向摊开
    const GNg::sVEC3 sunColor = {1.0f, 0.96f, 0.88f};
    gLight->AddDirectional(sunDir, sunColor, 2.2f, true); // 主光（有影）
    const GNg::sVEC3 ambient = {0.10f, 0.12f, 0.17f};
    gLight->SetAmbient(ambient, 1.0f);
    // 冷色补光：从右后方打一点轮廓，避免背面死黑
    const GNg::sVEC3 fillDir = {-0.62f, -0.30f, 0.72f};
    const GNg::sVEC3 fillColor = {0.42f, 0.55f, 0.85f};
    gLight->AddDirectional(fillDir, fillColor, 0.30f, false);
}

// 相机：绕场景缓慢环绕 + 轻微俯视；返回 viewProj
bool buildCamera(float time, float aspect, GNg::sMAT44 &viewProj, GNg::sMAT44 &view,
                 GNg::sVEC3 &eye) {
    const float yaw = 0.55f + time * 0.16f; // 8 秒转约 73°
    const float distance = 12.0f - std::fmin(time * 0.25f, 2.0f); // 缓慢推近
    const float pitch = 0.19f;
    gCamera.SetOrbit(gFocus, distance, yaw, pitch);
    gCamera.SetFov(42.0f);
    gCamera.SetNF(0.1f, 120.0f);
    GNg::sMAT44 projection;
    if (!gCamera.BuildViewProj(aspect, &view, &projection, &viewProj)) return false;
    eye = gCamera.GetCoordV();
    return true;
}

// 阴影：按场景 AABB + 相机拟合 4 级 CSM
void fitShadows(const GNg::sMAT44 &view, const GNg::sVEC3 &eye, float aspect) {
    if (!gShadows || gLight == nullptr) {
        gShadow.Clear();
        return;
    }
    GN3d::sAABB box;
    box.aMin = GNg::sVEC3{-9.6f, 0.0f, -9.6f};
    box.aMax = GNg::sVEC3{9.6f, 9.0f, 9.6f};
    GN3d::sSHADOW_FIT_DESC desc;
    desc.aCameraView = view;
    desc.aCameraPos = eye;
    desc.aFovYRad = gCamera.aFovY * GN3d::CAMERA_DEG_TO_RAD;
    desc.aAspect = aspect;
    desc.aLightDirection = GNg::sVEC3{0.46f, -0.74f, -0.49f};
    desc.aCasterBox = box;
    desc.aCatchMargin = 0.0f;
    desc.aClampToGround = true;
    desc.aCascadeCount = std::getenv("VIDEO_CASCADES") ? (uint32_t)std::atoi(std::getenv("VIDEO_CASCADES")) : GN3d::SHADOW_CASCADE_MAX;
    desc.aShadowMapSize = 2048;
    desc.aSplitLambda = 0.5f;
    desc.aTexelSnapping = true;
    desc.aEngineCascadeLayers = GNg::GN_VkShadowCascadeCount();
    GN3d::fitShadowState(desc, gLight, &gShadow);
}

} // namespace

// ── 引擎回调 ──
extern "C" {

GN_CALL void AppInit() {
    gPhysics.SetWorld(&gWorld);
    gScene.SetPhysicsController(&gPhysics); // 整个场景只推进一次物理
    gClock.SetFixedStep(1.0f / 60.0f, 4);
    gCamera.Init();
    buildScene();
    initLights();
    // 环境光照：用引擎的"均匀环境光"预设（G1b）替代天空 IBL。
    // 为什么：天空 IBL 的亮度会盖过方向光 —— 实测在完全关掉方向光与环境光后画面仍然
    // 被照亮（IBL 贡献），于是「方向光的阴影」只占最终色的一小部分，投影几乎看不出来。
    // 换成弱均匀环境光之后，方向光成为主光，阴影才立得住。
    GNg::GN_VkSetEnvironmentLighting(GN3d::eENV_LIGHTING_INDOOR_UNIFORM, 0.10f);
    GNg::GN_VkSetExposure(0.15f); // 环境光 + 两盏平行光偏亮，收一点避免死白
    std::fprintf(stderr, "[physicsVideo] 实体 %u 个（动态 %u），三角网/刚体就绪\n",
                 (unsigned)gBodies.size(), (unsigned)gPhysics.BindingCount());
}

GN_CALL void AppUnInit() {
    for (size_t i = 0; i < gBodies.size(); ++i) {
        gBodies[i].aEntity->aMesh.SetModel(nullptr);
        delete gBodies[i].aEntity;
        delete gBodies[i].aModel;
    }
    gBodies.clear();
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

    // 1) 时钟：物理固定 60Hz，视频按 30fps 出帧 ⇒ 每帧推进 2 个物理子步
    gClock.Tick(1.0f / 30.0f);
    const float dt = gClock.DeltaTime();
    const float alpha = gClock.Alpha();

    // 2) 场景：物理推进一次 → 逐实体同步 → 剔除/LOD/排序
    gScene.Update(dt, alpha);

    // 3) 相机
    const float aspect = (float)gWidth / (float)gHeight;
    GNg::sMAT44 view;
    GNg::sMAT44 viewProj;
    GNg::sVEC3 eye;
    if (!buildCamera(gClock.TotalTime(), aspect, viewProj, view, eye)) return;

    // 4) 阴影级联（写 sLIGHT_UBO）
    fitShadows(view, eye, aspect);

    // 5) 剔除 + LOD + 排序（不透明近→远、透明远→近）
    GN3d::sLOD_VIEW lodView;
    lodView.aScreenHeight = (float)gHeight;
    lodView.aFovYRad = gCamera.aFovY * GN3d::CAMERA_DEG_TO_RAD;
    lodView.aViewProj = viewProj;
    lodView.aCamPos = eye;

    GNg::cRENDER_FRAME frame;
    // 投影体：所有动态实体（静态墙也投，但它们在边上，影响小；这里只投动态的）
    std::vector<const GN3d::cMODEL *> casters;
    casters.reserve(gBodies.size());
    for (size_t i = 0; i < gBodies.size(); ++i) {
        if (gBodies[i].aDynamic) casters.push_back(gBodies[i].aModel);
    }
    if (gShadow.aValid && !casters.empty()) {
        frame.ShadowPass(casters.data(), (uint32_t)casters.size(), gShadow.aLightViewProj,
                         gShadow.aCount);
    }

    GNg::sRENDER_FRAME_DESC desc;
    desc.aViewProj = &viewProj;
    desc.aCameraPos = &eye;
    desc.aLightUBO = gLight->GetUBO();
    desc.aLightUBOSize = sizeof(GN3d::sLIGHT_UBO);
    desc.aDrawSkybox = gSkybox;
    if (!frame.Begin(desc)) return;

    // 地面 → 场景（按 scene 排好的顺序）
    gGround->DrawPBRSorted(eye);
    const uint32_t visible = gScene.PrepareRender(lodView, nullptr); // 不剔除：相机在场景外
    if (gVerbose) {
        std::fprintf(stderr,
                     "[dbg] visible=%u draw=%u shadowValid=%d cascades=%u layers=%u "
                     "shadowPipe=%s vp0[12..14]=(%.2f,%.2f,%.2f) smSize=%.0f\n",
                     visible, gScene.Draw(eye), gShadow.aValid ? 1 : 0, gShadow.aCount,
                     GNg::GN_VkShadowCascadeCount(),
                     GNg::GN_VkTestPipeline((int)GNg::eTEST_PIPE_SHADOW) != VK_NULL_HANDLE ? "ok"
                                                                                           : "NULL",
                     gShadow.aLightViewProj[0].m[12], gShadow.aLightViewProj[0].m[13],
                     gShadow.aLightViewProj[0].m[14], gShadow.aMapSize);
    } else {
        gScene.Draw(eye);
    }

    frame.End();
}

} // extern "C"

// ── 入口：解析参数 → 逐帧渲染 → 写 PNG ──
namespace {

void onFrame(int index) {
    int w = 0;
    int h = 0;
    int size = 0;
    unsigned char *rgba = GN_GrabFrame(&w, &h, &size);
    if (rgba == nullptr || w <= 0 || h <= 0) {
        std::fprintf(stderr, "[physicsVideo] 第 %d 帧回读失败\n", index);
        return;
    }
    char path[512];
    std::snprintf(path, sizeof(path), "%s/frame_%05d.png", gOutDir, index);
    if (!writePng(path, rgba, w, h)) {
        std::fprintf(stderr, "[physicsVideo] 写 %s 失败\n", path);
    }
    GN_GrabFrameFree(rgba);
    if (index % 10 == 0) std::fprintf(stderr, "[physicsVideo] 帧 %d/%d\n", index, gFrames);
}

} // namespace

int main(int argc, char **argv) {
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--out") == 0 && i + 1 < argc) {
            gOutDir = argv[++i];
        } else if (std::strcmp(argv[i], "--frames") == 0 && i + 1 < argc) {
            gFrames = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--size") == 0 && i + 2 < argc) {
            gWidth = std::atoi(argv[++i]);
            gHeight = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--no-shadows") == 0) {
            gShadows = false;
        } else if (std::strcmp(argv[i], "--no-skybox") == 0) {
            gSkybox = false;
        } else if (std::strcmp(argv[i], "--verbose") == 0) {
            gVerbose = true;
        }
    }
    if (gFrames < 1) gFrames = 1;

    // 输出目录（先清掉旧帧，避免上一次残留的帧被编码进视频）
    char cmd[512];
    std::snprintf(cmd, sizeof(cmd), "mkdir -p '%s' && rm -f '%s'/*.png", gOutDir, gOutDir);
    if (std::system(cmd) != 0) std::fprintf(stderr, "[physicsVideo] 清理输出目录失败\n");

    std::fprintf(stderr, "[physicsVideo] %dx%d，%d 帧，阴影=%d → %s\n", gWidth, gHeight, gFrames,
                 gShadows ? 1 : 0, gOutDir);

    GN_InitLoadCallBackFunc(AppInit, AppUnInit);
    GN_InitLoopCallBackFunc(AppDisplay2D, AppDisplay3D, AppRun);
    const bool ok = GN_StartRunHeadless((short)gWidth, (short)gHeight, gFrames, onFrame);
    if (!ok) {
        std::fprintf(stderr, "[physicsVideo] 初始化失败（无 Vulkan 设备？）\n");
        return 2;
    }
    std::printf("[physicsVideo] 完成：%d 帧 → %s\n", gFrames, gOutDir);
    return 0;
}
