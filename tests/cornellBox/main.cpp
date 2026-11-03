// tests/cornellBox/main.cpp — Cornell Box 测试程序入口（C0/C1）
//
// 职责：窗口/回调接线、延迟路径绘制顺序、GI 开关与快捷键。
// 用法：make cornellBox && LD_LIBRARY_PATH=. ./build/cornellBox
// 独立于 mmdViewer：无模型资产、无天气/云/大气；场景与相机见 cornellScene/cornellCamera。
#include "cornellShared.h"
#include "cornellCapture.h"
#include "cornellCpuRt.h"
#include "cornellGpuRt.h"
#include "cornellRtDevice.h"
#include "cornellPng.h"

#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <vector>

namespace {

float gDelta = 0.016f;
bool gCaptureRequested = false;
const int kRtWidth = 640;
const int kRtHeight = 400;
const int kGpuRtSamples = 16; // 每帧新增路径数（相机静止时逐帧累积；移动时仅本帧样本）
const int kHwRtSamples = 16;  // 硬件 ray query 每帧样本数（2058 三角形 BVH，比解析求交慢）
GNg::sIMAGE *gRtImage = nullptr;
bool gAutoCapture = false; // CORNELL_CAPTURE_ENGINE=1：引擎模式渲染若干帧后 F12 截图并退出
int gAutoFrame = 0;
Cornell::sRayQueryDevice gRayQueryDevice;
bool gActiveRtBackend = -1; // -1 = 未初始化；否则为 eGpuRtBackend

} // namespace

namespace Cornell {
bool gCpuRtPreview = false;
bool gGpuRtPreview = false;
bool gHwRtPreview = false;
}

void applyGiSwitches() {
    GNg::GN_VkSetSsaoEnabled(Cornell::gSsaoOn);
    GNg::GN_VkSetSsgiStrength(Cornell::gSsgiStrength);
    GNg::GN_VkEnableGI(Cornell::gEngineGiOn);
}

// 惰性切换模块后端：解析 compute 用引擎设备，ray query 用独立 AS 设备。
bool ensureRtBackend(Cornell::eGpuRtBackend backend) {
    if (Cornell::isGpuRtReady() && gActiveRtBackend == static_cast<int>(backend)) return true;
    if (Cornell::isGpuRtReady()) Cornell::shutdownGpuRt();
    gActiveRtBackend = -1;
    Cornell::sGpuRtContext ctx;
    if (backend == Cornell::eGPU_RT_RAY_QUERY) {
        if (GNg::GN_VkRayQuerySupported()) {
            // 引擎设备已启用 AS/ray_query：硬件光追直接用引擎设备（引擎级加速）。
            ctx = {GNg::GN_Vk.device, GNg::GN_Vk.physicalDevice, GNg::GN_Vk.graphicsQueue,
                   GNg::GN_Vk.graphicsFamily};
        } else {
            if (gRayQueryDevice.aDevice == VK_NULL_HANDLE &&
                !Cornell::createRayQueryDevice(&gRayQueryDevice)) {
                std::fprintf(stderr, "[cornellBox] ray-query 设备不可用\n");
                return false;
            }
            ctx = {gRayQueryDevice.aDevice, gRayQueryDevice.aPhysical, gRayQueryDevice.aQueue,
                   gRayQueryDevice.aQueueFamily};
        }
    } else {
        ctx = {GNg::GN_Vk.device, GNg::GN_Vk.physicalDevice, GNg::GN_Vk.graphicsQueue,
               GNg::GN_Vk.graphicsFamily};
    }
    if (!Cornell::initGpuRt(ctx, kRtWidth, kRtHeight, backend)) {
        std::fprintf(stderr, "[cornellBox] 光追后端初始化失败（backend=%d）\n",
                     static_cast<int>(backend));
        return false;
    }
    gActiveRtBackend = static_cast<int>(backend);
    return true;
}

extern "C" {

GN_CALL void AppInit() {
    if (!Cornell::initScene()) {
        std::fprintf(stderr, "[cornellBox] 场景/光源初始化失败\n");
    }
    Cornell::initCamera();
    Cornell::initHud();
    Cornell::ensureCaptureDir();
    std::fprintf(stderr, "[cornellBox] 引擎设备硬件光追(VK_KHR_ray_query)=%s\n",
                 GNg::GN_VkRayQuerySupported() ? "on" : "off");
    // 默认开启 GI/SSGI：Cornell 程序的目的就是看间接光；A/S 留给相机移动，GI 键改用 O/P/I/K。
    Cornell::gEngineGiOn = true;
    Cornell::gSsaoOn = true;
    Cornell::gSsgiStrength = 1.0f;
    applyGiSwitches();
    // 后处理参数：不调用 GN_VkSetPost 时 POST UBO 是未初始化的（合成会读垃圾 → 品红条纹）。
    GN3d::sPOST_UBO post = GN3d::makeDefaultPostUbo();
    // 引擎合成 pass 的 FXAA：延迟路径是 1-sample G-Buffer，没有 MSAA，开 FXAA 抗锯齿。
    post.fxaa = 1.0f;
    GNg::GN_VkSetPost(post);
    // 无人值守自检入口：CORNELL_GPU_RT=1 启动即进 GPU 光追预览（桌面/截图验证用）。
    const char *gpuRtEnv = std::getenv("CORNELL_GPU_RT");
    if (gpuRtEnv && gpuRtEnv[0] == '1') Cornell::gGpuRtPreview = true;
    const char *hwRtEnv = std::getenv("CORNELL_HW_RT");
    if (hwRtEnv && hwRtEnv[0] == '1') {
        Cornell::gHwRtPreview = true;
        Cornell::gGpuRtPreview = false;
    }
    // 无人值守引擎对拍：引擎模式渲染 30 帧（等 GI 稳定）→ F12 → 34 帧退出。
    const char *engineOnlyEnv = std::getenv("CORNELL_ENGINE_ONLY");
    if (engineOnlyEnv && engineOnlyEnv[0] == '1') {
        Cornell::gCpuRtPreview = false;
        Cornell::gGpuRtPreview = false;
        Cornell::gHwRtPreview = false;
        Cornell::gShowHud = false;
    }
    const char *captureEnv = std::getenv("CORNELL_CAPTURE_ENGINE");
    if (captureEnv && captureEnv[0] == '1') {
        gAutoCapture = true;
        Cornell::gCpuRtPreview = false;
        Cornell::gGpuRtPreview = false;
        Cornell::gHwRtPreview = false;
        Cornell::gShowHud = false; // 对拍截图不要 HUD
    }
}

GN_CALL void AppUnInit() {
    Cornell::shutdownGpuRt();
    Cornell::destroyRayQueryDevice(&gRayQueryDevice);
    gActiveRtBackend = -1;
    if (gRtImage) {
        delete gRtImage;
        gRtImage = nullptr;
    }
    Cornell::releaseHud();
    Cornell::releaseScene();
}

GN_CALL void AppDisplay2D(unsigned int nTime) {
    (void)nTime;
    if (!Cornell::gLight) return;

    // 相机 aspect 必须跟「实际渲染目标」= swapchain 一致，而不是窗口 framebuffer：
    // niri 会把窗口 tile 成竖长条（1122x1459），但 swapchain 仍是 1280x800，
    // 若用 framebuffer 的 aspect，视口会变成窄竖条、只见背面墙。
    int fbWidth = 1280;
    int fbHeight = 800;
    if (GNg::GN_Vk.swapchainExtent.width > 0u && GNg::GN_Vk.swapchainExtent.height > 0u) {
        fbWidth = static_cast<int>(GNg::GN_Vk.swapchainExtent.width);
        fbHeight = static_cast<int>(GNg::GN_Vk.swapchainExtent.height);
    }
    float aspect = static_cast<float>(fbWidth) / static_cast<float>(fbHeight);

    // T = CPU 软光追预览；Y = GPU compute 软光追预览（同一套 ray-scene 求交，
    // 优先用 GPU，初始化/渲染失败时自动回退 CPU，保证没有 GPU 也能看）。
    if (Cornell::gGpuRtPreview || Cornell::gHwRtPreview || Cornell::gCpuRtPreview) {
        const GNg::sVEC3 eye = Cornell::gCamera.GetCoordV();
        const GNg::sVEC3 fwd = Cornell::gCamera.Forward();
        const GNg::sVEC3 right = Cornell::gCamera.Right();
        const GNg::sVEC3 up = Cornell::gCamera.Up();
        const float eye3[3] = {eye.x, eye.y, eye.z};
        const float fwd3[3] = {fwd.x, fwd.y, fwd.z};
        const float right3[3] = {right.x, right.y, right.z};
        const float up3[3] = {up.x, up.y, up.z};
        std::vector<unsigned char> pixels;
        bool rendered = false;
        if (Cornell::gGpuRtPreview) {
            if (!ensureRtBackend(Cornell::eGPU_RT_ANALYTIC)) {
                Cornell::gGpuRtPreview = false;
                Cornell::gCpuRtPreview = true;
            } else {
                rendered = Cornell::renderGpuRt(kRtWidth, kRtHeight, eye3, fwd3, right3, up3,
                                                Cornell::gCamera.aFovY, Cornell::gLightMode,
                                                kGpuRtSamples, &pixels);
                if (!rendered) {
                    std::fprintf(stderr, "[cornellBox] GPU RT 渲染失败，回退 CPU RT\n");
                    Cornell::gGpuRtPreview = false;
                    Cornell::gCpuRtPreview = true;
                }
            }
        } else if (Cornell::gHwRtPreview) {
            if (!ensureRtBackend(Cornell::eGPU_RT_RAY_QUERY)) {
                std::fprintf(stderr, "[cornellBox] 硬件光追不可用，回退 GPU compute RT\n");
                Cornell::gHwRtPreview = false;
                Cornell::gGpuRtPreview = true;
            } else {
                rendered = Cornell::renderGpuRt(kRtWidth, kRtHeight, eye3, fwd3, right3, up3,
                                                Cornell::gCamera.aFovY, Cornell::gLightMode,
                                                kHwRtSamples, &pixels);
                if (!rendered) {
                    std::fprintf(stderr, "[cornellBox] HW RT 渲染失败，回退 GPU compute RT\n");
                    Cornell::gHwRtPreview = false;
                    Cornell::gGpuRtPreview = true;
                }
            }
        }
        if (!rendered && Cornell::gCpuRtPreview) {
            rendered = Cornell::renderCpuRt(kRtWidth, kRtHeight, eye3, fwd3, right3, up3,
                                            Cornell::gCamera.aFovY, Cornell::gLightMode, &pixels);
        }
        if (rendered) {
            if (!gRtImage) gRtImage = new GNg::sIMAGE();
            gRtImage->DataFill(pixels.data(), GNg::CR_RGBA, kRtWidth, kRtHeight);
        }
        // F12 在预览模式保存光追原图（不含 HUD），供与 PT 参考对拍。
        if (gCaptureRequested) {
            gCaptureRequested = false;
            if (rendered) {
                char path[256];
                std::snprintf(path, sizeof(path), ".local/cornellbox/rt-%ld.png",
                              static_cast<long>(std::time(nullptr)));
                if (Cornell::writeRgbPng(path, pixels, kRtWidth, kRtHeight)) {
                    std::printf("[cornellBox] 光追截图: %s\n", path);
                }
            }
        }
        GNg::GN_VkBegin2DPass();
        if (gRtImage && gRtImage->vkImageView) {
            GNg::ImageDraw4P(gRtImage, 0, 0, fbWidth, fbHeight, 0xFFFFFFFFu, 0xFFFFFFFFu,
                             0xFFFFFFFFu, 0xFFFFFFFFu, 0);
        }
        Cornell::drawHud();
        return;
    }

    GNg::sMAT44 view;
    GNg::sMAT44 projection;
    GNg::sMAT44 viewProjection;
    if (!Cornell::gCamera.BuildViewProj(aspect, &view, &projection, &viewProjection)) {
        return;
    }
    const GNg::sVEC3 camera = Cornell::gCamera.GetCoordV();
    const GN3d::sLIGHT_UBO *light = Cornell::gLight->GetUBO();

    GNg::GN_VkBegin3DPassDeferred(&viewProjection, &camera, light, sizeof(GN3d::sLIGHT_UBO));
    Cornell::drawScene(viewProjection, camera);
    // 顺序与引擎契约一致：G-Buffer → SSAO → SSGI → 探针 → 延迟光照（内部自动合成）。
    GNg::GN_VkSSAOPass();
    GNg::GN_VkSSGIPass();
    GNg::GN_VkUpdateProbes();
    GNg::GN_VkDeferredLighting(&viewProjection, &camera, light, sizeof(GN3d::sLIGHT_UBO));

    // F12：在 composite 之后、HUD 之前把 swapchain→staging 拷贝录进本帧命令缓冲，
    // 下一帧帧首写盘（两阶段；帧中直接读回会读到上一帧/未定义内容）。
    if (gCaptureRequested) {
        gCaptureRequested = false;
        const bool captureHdr = std::getenv("CORNELL_CAPTURE_HDR") != nullptr;
        const bool captureAlbedo = std::getenv("CORNELL_CAPTURE_ALBEDO") != nullptr;
        const bool captureNormal = std::getenv("CORNELL_CAPTURE_NORMAL") != nullptr;
        char path[256];
        std::snprintf(path, sizeof(path), ".local/cornellbox/%s-%ld.png",
                      captureHdr ? "hdr"
                                 : (captureAlbedo ? "albedo" : (captureNormal ? "normal" : "engine")),
                      static_cast<long>(std::time(nullptr)));
        const bool ok = captureHdr    ? Cornell::requestHdrCapture(path)
                        : captureAlbedo ? Cornell::requestAlbedoCapture(path)
                        : captureNormal ? Cornell::requestNormalCapture(path)
                                        : Cornell::requestSwapchainCapture(path);
        if (ok) {
            std::printf("[cornellBox] 截图已排队（下一帧写盘）: %s\n", path);
        } else {
            std::fprintf(stderr, "[cornellBox] 截图排队失败: %s\n", path);
        }
    }

    GNg::GN_VkBegin2DPass();
    Cornell::drawHud();
}

GN_CALL void AppDisplay3D(unsigned int nTime) {
    (void)nTime;
}

GN_CALL void AppRun(unsigned int nTime) {
    // 两阶段截图：上一帧命令缓冲已提交并在 BeginFrame 等待过 fence，此处安全读 staging。
    Cornell::finishPendingCapture();
    static unsigned int lastTime = 0;
    gDelta = static_cast<float>(nTime - lastTime) / 1000.0f;
    lastTime = nTime;
    if (gDelta <= 0.0f || gDelta > 0.5f) gDelta = 0.016f;

    if (GN::Key1[GK_H]) Cornell::gShowHud = !Cornell::gShowHud;
    if (GN::Key1[GK_F12]) gCaptureRequested = true;
    if (GN::Key1[GK_T]) {
        Cornell::gCpuRtPreview = !Cornell::gCpuRtPreview;
        if (Cornell::gCpuRtPreview) {
            Cornell::gGpuRtPreview = false;
            Cornell::gHwRtPreview = false;
        }
    }
    if (GN::Key1[GK_Y]) {
        Cornell::gGpuRtPreview = !Cornell::gGpuRtPreview;
        if (Cornell::gGpuRtPreview) {
            Cornell::gCpuRtPreview = false;
            Cornell::gHwRtPreview = false;
        }
    }
    if (GN::Key1[GK_U]) {
        Cornell::gHwRtPreview = !Cornell::gHwRtPreview;
        if (Cornell::gHwRtPreview) {
            Cornell::gCpuRtPreview = false;
            Cornell::gGpuRtPreview = false;
        }
    }
    if (GN::Key1[GK_1]) Cornell::resetCamera(0);
    if (GN::Key1[GK_2]) Cornell::resetCamera(1);
    if (GN::Key1[GK_3]) Cornell::resetCamera(2);
    if (GN::Key1[GK_4]) Cornell::resetCamera(3);
    if (GN::Key1[GK_R]) Cornell::resetCamera(0);
    if (GN::Key1[GK_O]) {
        Cornell::gSsaoOn = !Cornell::gSsaoOn;
        GNg::GN_VkSetSsaoEnabled(Cornell::gSsaoOn);
    }
    if (GN::Key1[GK_P]) {
        Cornell::gSsgiStrength += 0.5f;
        if (Cornell::gSsgiStrength > 1.0f) Cornell::gSsgiStrength = 0.0f;
        GNg::GN_VkSetSsgiStrength(Cornell::gSsgiStrength);
    }
    if (GN::Key1[GK_I]) {
        Cornell::gEngineGiOn = !Cornell::gEngineGiOn;
        GNg::GN_VkEnableGI(Cornell::gEngineGiOn);
    }
    if (GN::Key1[GK_K]) {
        Cornell::rebuildLights(Cornell::gLightMode == 0 ? 1 : 0);
    }

    Cornell::updateCamera(gDelta);

    if (gAutoCapture) {
        ++gAutoFrame;
        if (gAutoFrame == 30) gCaptureRequested = true;
        if (gAutoFrame == 34 && GT_MainWindow) {
            glfwSetWindowShouldClose(GT_MainWindow, GLFW_TRUE);
        }
    }
}

} // extern "C"

GN_MAIN;
GN_InitLoadCallBackFunc(AppInit, AppUnInit);
GN_InitLoopCallBackFunc(AppDisplay2D, AppDisplay3D, AppRun);
GN_StartRun("Cornell Box (GnVk)", 1280, 800, 0, false, true, true, 0, 0, 0, 0);
GN_EXIT;
