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
#include "cornellProbeBake.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <vector>

namespace {

float gDelta = 0.016f;
bool gCaptureRequested = false;
const int kRtWidth = 640;
const int kRtHeight = 400;
const int kGpuRtSamples = 16;       // 静止：每帧累积样本数
const int kGpuRtMovingSamples = 16; // 相机移动：与静止同档；compute 已很便宜，优先降噪点
const int kHwRtSamples = 16;        // 硬件 ray query 静止样本数（BVH 比解析求交慢）
const int kHwRtMovingSamples = 8;   // 硬件 ray query 移动样本数（BVH 较慢，先取 8）
GNg::sIMAGE *gRtImage = nullptr;
bool gAutoCapture = false; // CORNELL_CAPTURE_ENGINE=1：引擎模式渲染若干帧后 F12 截图并退出
bool gRtAutoCapture = false; // CORNELL_RT_CAPTURE=1：GPU RT 预览若干帧后 F12 截图并退出
bool gRtHold = false;        // CORNELL_RT_HOLD=1：截图后不自动退出（供外部抓窗口）
bool gAutoMove = false;      // CORNELL_AUTO_MOVE=1：每帧转一点相机，验证移动档时域
bool gProbesEnabled = false; // CORNELL_PROBES=1：烘焙并使用 SH9 探针
// B3c-2 异步探针更新：CORNELL_PROBE_ASYNC=1 启用；预算/样本/hysteresis 可调。
bool gProbeAsync = false;
bool gProbeRelocGpu = false; // CORNELL_PROBE_RELOCATION_GPU=1：GPU 6 轴 relocation（B3b-full）
float gProbeRelocMaxOffset = 0.0f; // 0.5×spacing，用于 dump 时检查 clamp
bool gProbeAsyncVerify = false;   // CORNELL_PROBE_ASYNC_VERIFY=1：跑 N 帧后回读探针并退出
unsigned int gProbeAsyncBudget = 64;
unsigned int gProbeAsyncSamples = 4;
float gProbeAsyncHysteresis = 0.9f;
unsigned int gProbeAsyncFrames = 0;
unsigned int gProbeAsyncFrame = 0;
const char *gProbeAsyncDump = nullptr;
bool gAutoLightSwitch = false; // CORNELL_AUTO_LIGHT_SWITCH=1：第 3 帧切光，测试热切换
Cornell::sProbeBakeResult gProbeBake[2];
bool gProbeBakeValid[2] = {false, false};

// 按 light mode 加载/上传探针；K 切光时复用另一份 asset，无需重烘。
bool uploadProbesForLight(int lightMode) {
    if (!gProbesEnabled || !Cornell::isGpuRtReady()) return false;
    const int mode = (lightMode == 1) ? 1 : 0;
    if (!gProbeBakeValid[mode]) {
        gProbeBakeValid[mode] = Cornell::bakeProbeGrid(mode, 4, 16, &gProbeBake[mode]);
        std::fprintf(stderr, "[cornellBox] probe bake[%d]=%s probes=%zu\n", mode,
                     gProbeBakeValid[mode] ? "ok" : "FAIL",
                     gProbeBake[mode].aSh9.size() / 36u);
    }
    if (!gProbeBakeValid[mode]) return false;
    if (std::getenv("CORNELL_PROBES_CONST")) {
        for (size_t i = 0; i < gProbeBake[mode].aSh9.size(); i += 36u) {
            for (int k = 0; k < 36; ++k) gProbeBake[mode].aSh9[i + k] = 0.0f;
            gProbeBake[mode].aSh9[i + 0] = 1.0f;
            gProbeBake[mode].aSh9[i + 1] = 1.0f;
            gProbeBake[mode].aSh9[i + 2] = 1.0f;
        }
    }
    float normalBias = 0.0f;
    if (const char *biasText = std::getenv("CORNELL_PROBE_NORMAL_BIAS")) {
        const double fraction = std::atof(biasText);
        if (fraction > 0.0) {
            normalBias = static_cast<float>(fraction) * gProbeBake[mode].aSpacing;
            const float maxBias = 0.5f * gProbeBake[mode].aSpacing;
            if (normalBias > maxBias) normalBias = maxBias;
        }
    }
    const char *relocationText = std::getenv("CORNELL_PROBE_RELOCATION");
    // GPU relocation 接管时不再在上传期跑 CPU v1-lite（两套算法会互相覆盖）。
    const bool relocation = relocationText && std::atoi(relocationText) != 0 && !gProbeRelocGpu;
    const bool ok = Cornell::gpuRtUploadProbes(
        gProbeBake[mode].aSh9.data(), gProbeBake[mode].aDepth.data(),
        &gProbeBake[mode].aOrigin.x, gProbeBake[mode].aSpacing, gProbeBake[mode].aCounts[0],
        gProbeBake[mode].aCounts[1], gProbeBake[mode].aCounts[2],
        gProbeBake[mode].aDepthRes, 1.0f, normalBias, relocation);
    if (ok && gProbeAsync) {
        const bool asyncOk = Cornell::gpuRtEnableProbeAsync(
            gProbeAsyncBudget, gProbeAsyncSamples, gProbeAsyncHysteresis);
        std::fprintf(stderr,
                     "[cornellBox] probe async=%s budget=%u spp=%u hysteresis=%.2f\n",
                     asyncOk ? "on" : "FAIL", gProbeAsyncBudget, gProbeAsyncSamples,
                     gProbeAsyncHysteresis);
        gProbeRelocMaxOffset = 0.5f * gProbeBake[mode].aSpacing;
        if (asyncOk && gProbeRelocGpu) {
            const bool relocOk = Cornell::gpuRtEnableProbeRelocationGpu(true);
            std::fprintf(stderr, "[cornellBox] probe relocation gpu=%s\n",
                         relocOk ? "on" : "FAIL");
        }
        const char *inactiveEnv = std::getenv("CORNELL_PROBE_TEST_INACTIVE");
        if (asyncOk && inactiveEnv) {
            const int parsed = std::atoi(inactiveEnv);
            const unsigned int inactiveCount = parsed > 0 ? static_cast<unsigned int>(parsed) : 0u;
            const bool marked = Cornell::gpuRtMarkInactiveProbes(inactiveCount);
            std::fprintf(stderr, "[cornellBox] probe inactive test: n=%u marked=%d\n",
                         inactiveCount, marked ? 1 : 0);
        }
    }
    std::fprintf(stderr, "[cornellBox] probe upload[%d]=%s normalBias=%.4f relocation=%d\n", mode,
                 ok ? "ok" : "FAIL", normalBias, relocation ? 1 : 0);
    return ok;
}
// 回读异步更新后的探针 SH9/状态并打印稳定指纹（一次全量 vs 预算分批应逐位一致）。
void dumpProbeAsyncResults() {
    std::vector<float> sh;
    std::vector<unsigned int> state;
    if (!Cornell::gpuRtReadbackProbes(&sh, &state)) {
        std::fprintf(stderr, "[probeAsync] readback FAIL\n");
        return;
    }
    unsigned long long hash = 1469598103934665603ull;
    double dcSum = 0.0;
    size_t probeCount = sh.size() / 36u;
    for (size_t i = 0; i < sh.size(); ++i) {
        unsigned int bits = 0u;
        std::memcpy(&bits, &sh[i], sizeof(bits));
        hash ^= bits;
        hash *= 1099511628211ull;
    }
    for (size_t p = 0; p < probeCount; ++p) {
        dcSum += (sh[p * 36u + 0] + sh[p * 36u + 1] + sh[p * 36u + 2]) / 3.0;
    }
    unsigned int ageZero = 0u;
    unsigned int ageOne = 0u;
    unsigned int ageMax = 0u;
    for (size_t p = 0; p < state.size() / 4u; ++p) {
        const unsigned int age = state[p * 4u];
        if (age == 0u) ++ageZero;
        if (age == 1u) ++ageOne;
        if (age > ageMax) ageMax = age;
    }
    std::fprintf(stderr,
                 "[probeAsync] probes=%zu hash=%016llx dcMean=%.6f ages{zero:%u,one:%u,max:%u}\n",
                 probeCount, hash, probeCount ? dcSum / static_cast<double>(probeCount) : 0.0,
                 ageZero, ageOne, ageMax);
    if (gProbeRelocGpu) {
        std::vector<float> offsets;
        if (Cornell::gpuRtReadbackRelocation(&offsets)) {
            size_t moved = 0;
            double maxAbs = 0.0;
            size_t bad = 0;
            for (size_t i = 0; i + 3 < offsets.size(); i += 4u) {
                const double len = std::sqrt(static_cast<double>(offsets[i]) * offsets[i] +
                                             static_cast<double>(offsets[i + 1]) * offsets[i + 1] +
                                             static_cast<double>(offsets[i + 2]) * offsets[i + 2]);
                if (len > 1.0e-6) ++moved;
                if (len > maxAbs) maxAbs = len;
                if (!(offsets[i] == offsets[i]) || !(offsets[i + 1] == offsets[i + 1]) ||
                    !(offsets[i + 2] == offsets[i + 2])) {
                    ++bad;
                }
            }
            std::fprintf(stderr, "[probeReloc] moved=%zu/%zu maxLen=%.5f nan=%zu clamp=%.5f\n",
                         moved, offsets.size() / 4u, maxAbs, bad, gProbeRelocMaxOffset);
            if (gProbeAsyncDump) {
                char path[512];
                std::snprintf(path, sizeof(path), "%s.reloc.bin", gProbeAsyncDump);
                FILE *rf = std::fopen(path, "wb");
                if (rf) {
                    std::fwrite(offsets.data(), sizeof(float), offsets.size(), rf);
                    std::fclose(rf);
                }
            }
        } else {
            std::fprintf(stderr, "[probeReloc] readback FAIL\n");
        }
    }
    if (gProbeAsyncDump) {
        char path[512];
        std::snprintf(path, sizeof(path), "%s.sh.bin", gProbeAsyncDump);
        FILE *f = std::fopen(path, "wb");
        if (f) {
            std::fwrite(sh.data(), sizeof(float), sh.size(), f);
            std::fclose(f);
        }
        std::snprintf(path, sizeof(path), "%s.state.bin", gProbeAsyncDump);
        f = std::fopen(path, "wb");
        if (f) {
            std::fwrite(state.data(), sizeof(unsigned int), state.size(), f);
            std::fclose(f);
        }
    }
}
int gAutoFrame = 0;
Cornell::sRayQueryDevice gRayQueryDevice;
bool gActiveRtBackend = -1; // -1 = 未初始化；否则为 eGpuRtBackend
unsigned int gLastUploadedSerial = 0; // 已上传到纹理的光追帧序号
bool gProfile = false;                // CORNELL_PROFILE=1：每 60 帧打印平均耗时
double gProfileFrameSum = 0.0;
double gProfileRtSum = 0.0;
double gProfileUploadSum = 0.0;
int gProfileSamples = 0;
int gProfileFrames = 0;

double nowMs() {
    using clock = std::chrono::steady_clock;
    return std::chrono::duration<double, std::milli>(clock::now().time_since_epoch()).count();
}

} // namespace

namespace Cornell {
bool gCpuRtPreview = false;
bool gGpuRtPreview = false;
bool gHwRtPreview = false;
float gFps = 0.0f;
float gFrameMs = 0.0f;
float gRtWaitMs = 0.0f;
float gUploadMs = 0.0f;
int gRtSamples = 0;
unsigned int gRtAccum = 0;
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
    gLastUploadedSerial = 0;
    gActiveRtBackend = static_cast<int>(backend);
    if (gProbesEnabled) uploadProbesForLight(Cornell::gLightMode);
    return true;
}

extern "C" {

GN_CALL void AppInit() {
    gProfile = std::getenv("CORNELL_PROFILE") != nullptr;
    gProbesEnabled = std::getenv("CORNELL_PROBES") != nullptr;
    gAutoLightSwitch = std::getenv("CORNELL_AUTO_LIGHT_SWITCH") != nullptr;
    // 已落地的运行时 DDGI（异步更新 + relocation）是 cornellBox 探针路径的默认：
    //   - 设了 CORNELL_PROBES 且未显式给 CORNELL_PROBE_ASYNC → 默认开启运行时更新；
    //   - CORNELL_PROBE_ASYNC=0 → 退回静态 GPV4 烘焙（旧路径，逐字节可回归）。
    const char *asyncEnv = std::getenv("CORNELL_PROBE_ASYNC");
    if (asyncEnv) {
        gProbeAsync = asyncEnv[0] != '0';
    } else if (gProbesEnabled) {
        gProbeAsync = true;
    }
    if (const char *budgetEnv = std::getenv("CORNELL_PROBE_UPDATE_BUDGET")) {
        const int parsed = std::atoi(budgetEnv);
        gProbeAsyncBudget = parsed <= 0 ? 0u : static_cast<unsigned int>(parsed);
    }
    if (const char *sppEnv = std::getenv("CORNELL_PROBE_SPP")) {
        const int parsed = std::atoi(sppEnv);
        if (parsed > 0) gProbeAsyncSamples = static_cast<unsigned int>(parsed);
    }
    if (const char *hystEnv = std::getenv("CORNELL_PROBE_HYSTERESIS")) {
        const double parsed = std::atof(hystEnv);
        if (parsed >= 0.0 && parsed <= 1.0) gProbeAsyncHysteresis = static_cast<float>(parsed);
    }
    if (const char *relocGpuEnv = std::getenv("CORNELL_PROBE_RELOCATION_GPU")) {
        gProbeRelocGpu = relocGpuEnv[0] != '0';
        if (gProbeRelocGpu) gProbeAsync = true; // relocation 依赖同批 depth 更新
    }
    if (const char *verifyEnv = std::getenv("CORNELL_PROBE_ASYNC_VERIFY")) {
        gProbeAsyncVerify = verifyEnv[0] != '0';
    }
    if (const char *framesEnv = std::getenv("CORNELL_PROBE_ASYNC_FRAMES")) {
        const int parsed = std::atoi(framesEnv);
        if (parsed > 0) gProbeAsyncFrames = static_cast<unsigned int>(parsed);
    }
    gProbeAsyncDump = std::getenv("CORNELL_PROBE_ASYNC_DUMP");
    if (gProbeAsyncVerify && gProbeAsyncFrames == 0u) gProbeAsyncFrames = 8u;
    if (gProbeAsync || gProbeAsyncVerify) gProbesEnabled = true;
    if (std::getenv("CORNELL_BAKE_TEST")) {
        Cornell::sProbeBakeResult result;
        const bool ok = Cornell::bakeProbeGrid(0, 8, 8, &result);
        std::fprintf(stderr, "[cornellBox] bake=%s probes=%zu firstSh=(%.4f %.4f %.4f)\n",
                     ok ? "ok" : "FAIL", result.aSh9.size() / 36u,
                     result.aSh9.empty() ? 0.0f : result.aSh9[0],
                     result.aSh9.empty() ? 0.0f : result.aSh9[1],
                     result.aSh9.empty() ? 0.0f : result.aSh9[2]);
        if (GT_MainWindow) glfwSetWindowShouldClose(GT_MainWindow, GLFW_TRUE);
        return;
    }
    if (!Cornell::initScene()) {
        std::fprintf(stderr, "[cornellBox] 场景/光源初始化失败\n");
    }
    Cornell::initCamera();
    // 无人值守光照变体：CORNELL_LIGHT=point 切到经典 point 模式（默认 sun）。
    const char *lightEnv = std::getenv("CORNELL_LIGHT");
    if (lightEnv && (lightEnv[0] == 'p' || lightEnv[0] == 'P' || lightEnv[0] == '1')) {
        Cornell::rebuildLights(1);
    }
    Cornell::initHud();
    Cornell::ensureCaptureDir();
    std::fprintf(stderr, "[cornellBox] 引擎设备硬件光追(VK_KHR_ray_query)=%s\n",
                 GNg::GN_VkRayQuerySupported() ? "on" : "off");
    // S1 验证：用引擎 API 从 Cornell 网格构建设备场景 AS（GI 的 ray query 前置）。
    if (GNg::GN_VkRayQuerySupported()) {
        GNg::GN_VkRayQueryBeginScene();
        for (size_t i = 0; i < Cornell::gMeshes.size(); ++i) {
            const Cornell::sMesh &m = Cornell::gMeshes[i];
            if (m.aVertices.empty() || m.aIndices.size() < 3) continue;
            std::vector<float> positions(m.aVertices.size() * 3u);
            std::vector<float> normals(m.aVertices.size() * 3u);
            for (size_t v = 0; v < m.aVertices.size(); ++v) {
                positions[v * 3 + 0] = m.aVertices[v].px;
                positions[v * 3 + 1] = m.aVertices[v].py;
                positions[v * 3 + 2] = m.aVertices[v].pz;
                normals[v * 3 + 0] = m.aVertices[v].nx;
                normals[v * 3 + 1] = m.aVertices[v].ny;
                normals[v * 3 + 2] = m.aVertices[v].nz;
            }
            GNg::GN_VkRayQueryAddMesh(positions.data(), normals.data(), m.aIndices.data(),
                                      static_cast<uint32_t>(m.aVertices.size()),
                                      static_cast<uint32_t>(m.aIndices.size()),
                                      static_cast<uint32_t>(i));
        }
        const bool asOk = GNg::GN_VkRayQueryEndScene();
        std::fprintf(stderr, "[cornellBox] 引擎场景 AS=%s tlas=%p\n", asOk ? "ok" : "FAIL",
                     (void *)GNg::GN_VkRayQuerySceneTlas());
    }
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
    // 无人值守 RT 预览截图：llvmpipe/真机都能跑，用来验证 point 曝光与 sRGB 上传。
    const char *rtCaptureEnv = std::getenv("CORNELL_RT_CAPTURE");
    if (rtCaptureEnv && rtCaptureEnv[0] == '1') {
        gAutoCapture = true;
        gRtAutoCapture = true;
        Cornell::gCpuRtPreview = false;
        Cornell::gGpuRtPreview = true;
        Cornell::gHwRtPreview = false;
        Cornell::gShowHud = true;
        const char *holdEnv = std::getenv("CORNELL_RT_HOLD");
        gRtHold = holdEnv && holdEnv[0] == '1';
    }
    const char *moveEnv = std::getenv("CORNELL_AUTO_MOVE");
    gAutoMove = moveEnv && moveEnv[0] == '1';
    // 测试用：RT_CAPTURE 默认 GPU 解析路径；再给一个 HW 覆盖。
    if (std::getenv("CORNELL_HW_RT") && std::getenv("CORNELL_HW_RT")[0] == '1') {
        Cornell::gHwRtPreview = true;
        Cornell::gGpuRtPreview = false;
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

    Cornell::gRtWaitMs = 0.0f;
    Cornell::gUploadMs = 0.0f;
    Cornell::gRtSamples = 0;
    Cornell::gRtAccum = 0;

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
        bool fromGpu = false;
        bool newPixels = false;
        if (Cornell::gGpuRtPreview) {
            if (!ensureRtBackend(Cornell::eGPU_RT_ANALYTIC)) {
                Cornell::gGpuRtPreview = false;
                Cornell::gCpuRtPreview = true;
            } else {
                // 顺序：collect（等上一帧）→ DataFill（上传上一帧）→ submit（本帧），
                // 让本帧 compute 与本次上传/绘制重叠；不能先 submit 再 DataFill。
                if (!Cornell::gpuRtCollect(&pixels)) {
                    std::fprintf(stderr, "[cornellBox] GPU RT 回读失败，回退 CPU RT\n");
                    Cornell::gGpuRtPreview = false;
                    Cornell::gCpuRtPreview = true;
                } else {
                    const Cornell::sGpuRtStats &stats = Cornell::gpuRtStats();
                    newPixels =
                        !pixels.empty() && stats.aCompletedSerial != gLastUploadedSerial;
                    const bool moved = Cornell::gpuRtCameraMoved(
                        eye3, fwd3, right3, up3, Cornell::gCamera.aFovY, Cornell::gLightMode);
                    const int samples = moved ? kGpuRtMovingSamples : kGpuRtSamples;
                    if (!Cornell::gpuRtSubmit(kRtWidth, kRtHeight, eye3, fwd3, right3, up3,
                                              Cornell::gCamera.aFovY, Cornell::gLightMode,
                                              samples)) {
                        std::fprintf(stderr, "[cornellBox] GPU RT 提交失败，回退 CPU RT\n");
                        Cornell::gGpuRtPreview = false;
                        Cornell::gCpuRtPreview = true;
                    } else {
                        rendered = true;
                        fromGpu = true;
                    }
                }
            }
        } else if (Cornell::gHwRtPreview) {
            if (!ensureRtBackend(Cornell::eGPU_RT_RAY_QUERY)) {
                std::fprintf(stderr, "[cornellBox] 硬件光追不可用，回退 GPU compute RT\n");
                Cornell::gHwRtPreview = false;
                Cornell::gGpuRtPreview = true;
            } else {
                if (!Cornell::gpuRtCollect(&pixels)) {
                    std::fprintf(stderr, "[cornellBox] HW RT 回读失败，回退 GPU compute RT\n");
                    Cornell::gHwRtPreview = false;
                    Cornell::gGpuRtPreview = true;
                } else {
                    const Cornell::sGpuRtStats &stats = Cornell::gpuRtStats();
                    newPixels =
                        !pixels.empty() && stats.aCompletedSerial != gLastUploadedSerial;
                    const bool moved = Cornell::gpuRtCameraMoved(
                        eye3, fwd3, right3, up3, Cornell::gCamera.aFovY, Cornell::gLightMode);
                    const int samples = moved ? kHwRtMovingSamples : kHwRtSamples;
                    if (!Cornell::gpuRtSubmit(kRtWidth, kRtHeight, eye3, fwd3, right3, up3,
                                              Cornell::gCamera.aFovY, Cornell::gLightMode,
                                              samples)) {
                        std::fprintf(stderr, "[cornellBox] HW RT 提交失败，回退 GPU compute RT\n");
                        Cornell::gHwRtPreview = false;
                        Cornell::gGpuRtPreview = true;
                    } else {
                        rendered = true;
                        fromGpu = true;
                    }
                }
            }
        }
        if (!rendered && Cornell::gCpuRtPreview) {
            rendered = Cornell::renderCpuRt(kRtWidth, kRtHeight, eye3, fwd3, right3, up3,
                                            Cornell::gCamera.aFovY, Cornell::gLightMode, &pixels);
            newPixels = rendered && !pixels.empty();
            fromGpu = false;
        }
        if (rendered) {
            if (!gRtImage) {
                gRtImage = new GNg::sIMAGE();
            }
            // 交换链是 sRGB 时，RT 图像按 sRGB 纹理上传：采样硬件解码，2D 输出再编码一次，
            // 与引擎 composite 的 sRGB 约定一致；非 sRGB 交换链保持 UNORM 直通。
            // 交换链格式变化（resize/重建）时重建纹理，避免 UNORM/sRGB 视图错配。
            const bool wantSrgb = GNg::GN_VkSwapchainIsSrgb();
            if (gRtImage->vkSrgb != wantSrgb) {
                if (gRtImage->vkImageView != VK_NULL_HANDLE) gRtImage->Release();
                gRtImage->vkSrgb = wantSrgb;
            }
            const Cornell::sGpuRtStats &stats = Cornell::gpuRtStats();
            if (newPixels) {
                const double uploadStart = nowMs();
                gRtImage->DataFill(pixels.data(), GNg::CR_RGBA, kRtWidth, kRtHeight);
                Cornell::gUploadMs = static_cast<float>(nowMs() - uploadStart);
                if (fromGpu) gLastUploadedSerial = stats.aCompletedSerial;
            }
            if (fromGpu) {
                Cornell::gRtSamples = stats.aLastSamples;
                Cornell::gRtAccum = stats.aAccumSamples;
                if (newPixels) {
                    Cornell::gRtWaitMs =
                        static_cast<float>(stats.aWaitMs + stats.aReadbackMs);
                }
            }
        }
        // F12 在预览模式保存光追原图（不含 HUD），供与 PT 参考对拍。
        if (gCaptureRequested) {
            gCaptureRequested = false;
            if (rendered && !pixels.empty()) {
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
    static int sAutoLightFrame = 0;
    if (gAutoLightSwitch && ++sAutoLightFrame == 3) {
        Cornell::rebuildLights(Cornell::gLightMode == 0 ? 1 : 0);
        if (gProbesEnabled) uploadProbesForLight(Cornell::gLightMode);
    }
    // 两阶段截图：上一帧命令缓冲已提交并在 BeginFrame 等待过 fence，此处安全读 staging。
    Cornell::finishPendingCapture();
    static unsigned int lastTime = 0;
    static float smoothFrameMs = 16.7f;
    const bool clockValid = (lastTime != 0u && nTime > lastTime);
    const float rawDelta = clockValid ? static_cast<float>(nTime - lastTime) / 1000.0f : 0.0f;
    lastTime = nTime;
    if (clockValid) {
        // 慢帧（CPU 光追）直接显示真实耗时，不拿 0.9/0.1 平滑掩盖。
        smoothFrameMs = rawDelta > 0.5f ? rawDelta * 1000.0f
                                        : smoothFrameMs * 0.9f + rawDelta * 1000.0f * 0.1f;
    }
    Cornell::gFrameMs = smoothFrameMs;
    Cornell::gFps = smoothFrameMs > 0.01f ? 1000.0f / smoothFrameMs : 0.0f;
    gDelta = rawDelta;
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
        if (gProbesEnabled) uploadProbesForLight(Cornell::gLightMode);
    }

    Cornell::updateCamera(gDelta);
    if (gAutoMove) {
        if (Cornell::gCamera.aMode == GN3d::eCAMERA_MODE_ORBIT) {
            Cornell::gCamera.OrbitRotate(0.002f, 0.0f);
        } else {
            Cornell::gCamera.SelfRotLR(0.002f);
        }
    }

    if (gProfile) {
        gProfileFrameSum += Cornell::gFrameMs;
        gProfileRtSum += Cornell::gRtWaitMs;
        gProfileUploadSum += Cornell::gUploadMs;
        gProfileSamples += Cornell::gRtSamples;
        ++gProfileFrames;
        if (gProfileFrames >= 60) {
            const double inv = 1.0 / static_cast<double>(gProfileFrames);
            const Cornell::sGpuRtStats &stats = Cornell::gpuRtStats();
            std::fprintf(stderr,
                         "[cornellBox] profile frames=%d avg frame=%.2fms fps=%.1f "
                         "rt=%.2fms (submit=%.2f wait=%.2f readback=%.2f) "
                         "upload=%.2fms spp=%d accum=%u\n",
                         gProfileFrames, gProfileFrameSum * inv, Cornell::gFps,
                         gProfileRtSum * inv, stats.aSubmitMs, stats.aWaitMs,
                         stats.aReadbackMs, gProfileUploadSum * inv,
                         static_cast<int>(static_cast<double>(gProfileSamples) * inv + 0.5),
                         Cornell::gRtAccum);
            gProfileFrameSum = 0.0;
            gProfileRtSum = 0.0;
            gProfileUploadSum = 0.0;
            gProfileSamples = 0;
            gProfileFrames = 0;
        }
    }

    if (gProbeAsyncVerify) {
        ++gProbeAsyncFrame;
        if (gProbeAsyncFrame == gProbeAsyncFrames + 1u) {
            dumpProbeAsyncResults();
            if (GT_MainWindow) glfwSetWindowShouldClose(GT_MainWindow, GLFW_TRUE);
        }
    }

    if (gAutoCapture) {
        ++gAutoFrame;
        const int captureAt = gRtAutoCapture ? 8 : 30;
        if (gAutoFrame == captureAt) gCaptureRequested = true;
        if (!gRtHold && gAutoFrame == captureAt + 4 && GT_MainWindow) {
            glfwSetWindowShouldClose(GT_MainWindow, GLFW_TRUE);
        }
    }
}

} // extern "C"

GN_MAIN;
GN_InitLoadCallBackFunc(AppInit, AppUnInit);
GN_InitLoopCallBackFunc(AppDisplay2D, AppDisplay3D, AppRun);
// CORNELL_HEADLESS=1（或 GN_HEADLESS_FRAMES=N）：无窗口（离屏）运行——不建窗口/surface/交换链，
// 也不需要 X11/Wayland。第 30 帧用 GN_GrabFrame 取像素写 PNG，供无显示环境做画面回归。
const int envHeadlessFrames = GN_HeadlessFramesRequested();
const char *headlessEnv = std::getenv("CORNELL_HEADLESS");
const bool cornellHeadless = envHeadlessFrames > 0 || (headlessEnv && headlessEnv[0] == '1');
if (cornellHeadless) {
    const int frames = envHeadlessFrames > 0 ? envHeadlessFrames : 34;
    GN_StartRunHeadless(1280, 800, frames, [](int index) {
        if (index != 29) return; // 与 CORNELL_CAPTURE_ENGINE 的第 30 帧对齐
        int w = 0, h = 0, size = 0;
        unsigned char *rgba = GN_GrabFrame(&w, &h, &size);
        if (!rgba) {
            std::fprintf(stderr, "[cornellBox] 无窗口抓帧失败\n");
            return;
        }
        Cornell::ensureCaptureDir();
        Cornell::writeRgbaPngFile(".local/cornellbox/headless-engine.png", rgba, w, h);
        GN_GrabFrameFree(rgba);
    });
} else {
    GN_StartRun("Cornell Box (GnVk)", 1280, 800, 0, false, true, true, 0, 0, 0, 0);
}
GN_EXIT;
