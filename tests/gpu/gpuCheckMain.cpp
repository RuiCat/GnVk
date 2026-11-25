// GPU 桌面验收入口：11 条 P1b 管线 + 2D 四混合 + 3D 六站点 + P1c 材质实例 + P2 GPU 蒙皮 + P8 剔除/调试线框。
//
// 运行：make gpuCheck
// 真实 GPU 桌面验收必须用本机 /dev/dri 上的 Vulkan 设备；无设备时退出码 2 = SKIP。
#include <GN.h>
#include "gpuTestHooks.h"
#include "GN_Vk.h"
#include "postParams.h" // L1：旧色值站点统一走线性直通（NONE + 关 Bloom）

#include "gpuCheck2D.h"
#include "gpuCheck3D.h"
#include "gpuCheckCull.h"
#include "gpuCheckMaterial.h"
#include "gpuCheckParticle.h"
#include "gpuCheckBatch.h"
#include "gpuCheckCloth.h"
#include "gpuCheckSkinSsbo.h"
#include "gpuCheckLod.h"
#include "gpuCheckFog.h"
#include "gpuCheckLight.h"
#include "gpuCheckPrecipitation.h"
#include "gpuCheckDecal.h"
#include "gpuCheckReport.h"
#include "gpuCheckSkin.h"
#include "gpuCheckEntity.h"
#include "gpuCheckTexture.h"
#include "gpuPixelRead.h"

#include <GLFW/glfw3.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

// 引擎状态一律走 gpuTestHooks 的稳定面（A1），不再直读 GN_Vk。
using namespace GpuCheck;

void CheckPipeline(const char *name, VkPipeline pipeline) {
    if(pipeline != VK_NULL_HANDLE) {
        GpuCheck::ReportPass(name, "pipeline created");
    } else {
        GpuCheck::ReportFail(name, "pipeline is VK_NULL_HANDLE");
    }
}

void CheckCreation() {
    const unsigned count = (unsigned)Hooks().pipelineCount;
    const unsigned long long miss = Hooks().pipelineMiss;
    const unsigned long long hit = Hooks().pipelineHit;
    char detail[160];
    std::snprintf(detail, sizeof(detail), "count=%u miss=%llu hit=%llu expected >=11 (P1b 11 + 新增子系统)",
                  count, miss, hit);
    // P1c/P2/P8 的管线也在 GN_VkInit 里经同一缓存创建，因此只要求不少于 P1b 的 11 条。
    if(count >= 11 && hit == 0) GpuCheck::ReportPass("cache.entries", detail);
    else GpuCheck::ReportFail("cache.entries", detail);

    CheckPipeline("pipe.2d.normal", Pipe(GNg::eTEST_PIPE_2D));
    CheckPipeline("pipe.2d.bright", Pipe(GNg::eTEST_PIPE_2D_BRIGHT));
    CheckPipeline("pipe.2d.multiply", Pipe(GNg::eTEST_PIPE_2D_MULTIPLY));
    CheckPipeline("pipe.2d.null", Pipe(GNg::eTEST_PIPE_2D_NULL));
    CheckPipeline("pipe.3d.forward", Pipe(GNg::eTEST_PIPE_3D_FORWARD));
    CheckPipeline("pipe.3d.pbr", Pipe(GNg::eTEST_PIPE_3D_PBR));
    CheckPipeline("pipe.3d.shadow", Pipe(GNg::eTEST_PIPE_SHADOW));
    CheckPipeline("pipe.composite", Pipe(GNg::eTEST_PIPE_COMPOSITE));
    CheckPipeline("pipe.skybox", Pipe(GNg::eTEST_PIPE_SKYBOX));
    CheckPipeline("pipe.gbuffer", Pipe(GNg::eTEST_PIPE_GBUFFER));
    CheckPipeline("pipe.deferredLight", Pipe(GNg::eTEST_PIPE_DEFERRED_LIGHT));
}

void PrintDevice() {
    VkPhysicalDeviceProperties props;
    std::memset(&props, 0, sizeof(props));
    vkGetPhysicalDeviceProperties(PhysDev(), &props);
    std::printf("DEVICE: %s (vendor=0x%04x device=0x%04x api=%u.%u.%u)\n",
                props.deviceName, props.vendorID, props.deviceID,
                VK_VERSION_MAJOR(props.apiVersion), VK_VERSION_MINOR(props.apiVersion),
                VK_VERSION_PATCH(props.apiVersion));
    std::printf("SWAPCHAIN: %ux%u format=%d bytesPerPixel=%u\n",
                Extent().width, Extent().height,
                (int)SwapFormat(), GpuCheck::SwapchainBytesPerPixel());
}

} // namespace

int main() {
    std::printf("==== P1b GPU pipeline acceptance ====\n");
    // 无窗口：GN_HEADLESS=1 或 GN_HEADLESS_FRAMES>0 都算（后者是 GN_Main 的统一开关）。
    const char *headlessEnv = std::getenv("GN_HEADLESS");
    const char *framesEnv = std::getenv("GN_HEADLESS_FRAMES");
    const bool headless = (headlessEnv && headlessEnv[0] && headlessEnv[0] != '0') ||
                          (framesEnv && std::atoi(framesEnv) > 0);
    GLFWwindow *window = 0;
    if(headless) {
        std::printf("[info] 无窗口模式：跳过窗口创建，走离屏渲染\n");
    } else {
        if(!glfwInit()) {
            GpuCheck::ReportSkip("gpu.init", "glfwInit failed (no display)");
            return 2;
        }
        glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE); // Wayland 隐藏窗口会在 present 处卡住
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        window = glfwCreateWindow(640, 480, "GnVk P1b GPU Check", 0, 0);
        if(!window) {
            glfwTerminate();
            GpuCheck::ReportSkip("gpu.init", "glfwCreateWindow failed");
            return 2;
        }
    }
    GNg::GN_VkInit(window, 640, 480); // window=0 → 无窗口离屏路径
    if(!EngineReady()) {
        if(window) glfwDestroyWindow(window);
        if(!headless) glfwTerminate();
        GpuCheck::ReportSkip("gpu.init", "GN_VkInit failed: no Vulkan device");
        GpuCheck::PrintSummary();
        return 2;
    }

    PrintDevice();
    // L1：3D 输出统一进 HDR 合成；旧站点按“线性直通 + 关 Bloom”回归，像素期望值保持不变。
    GNg::GN_VkSetPost(GN3d::makeLegacyPostUbo());
    CheckCreation();
    GpuCheck::Run2DChecks();
    GpuCheck::Run3DChecks();
    GpuCheck::RunMaterialChecks();
    GpuCheck::RunTextureChecks();
    GpuCheck::RunSkinChecks();
    GpuCheck::RunEntityChecks(); // P2：骨骼控制器 → 蒙皮绘制（实体层接线）
    GpuCheck::RunCullChecks();
    GpuCheck::RunParticleChecks();
    GpuCheck::RunClothChecks();
    GpuCheck::RunSkinSsboChecks();
    GpuCheck::RunBatchChecks();
    GpuCheck::RunLodChecks();
    GpuCheck::RunFogChecks();
    GpuCheck::RunLightChecks(); // 光照系统端到端：改光源 → 像素跟着变
    GpuCheck::RunCubeShadowChecks(); // B2：点光/聚光 cube shadow
    GpuCheck::RunPrecipitationChecks();
    GpuCheck::RunDecalChecks();

    // 描述符改写体检（VUID-vkUpdateDescriptorSets-None-03047）：渲染通道内改写
    // 已绑定的规范集必须为 0 次。total>0 同时证明守卫确实被走到（不是恒为 0 的空壳）。
    {
        const uint32_t total = GNg::GN_VkDescriptorUpdatesTotal();
        const uint32_t bad = GNg::GN_VkDescriptorUpdatesWhileBound();
        char detail[256];
        std::snprintf(detail, sizeof(detail),
                      "updates=%u, 通道内改写已绑定规范集=%u（首个站点: %s / %s）", total, bad,
                      GNg::GN_VkDescriptorUpdateSite() ? GNg::GN_VkDescriptorUpdateSite() : "无",
                      GNg::GN_VkDescriptorUpdateSetName() ? GNg::GN_VkDescriptorUpdateSetName()
                                                         : "无");
        if (bad == 0u && total > 0u) GpuCheck::ReportPass("desc.noUpdateWhileBound", detail);
        else GpuCheck::ReportFail("desc.noUpdateWhileBound", detail);
    }

    GNg::GN_VkShutdown();
    if(!EngineReady() && Hooks().pipelineCount == 0) {
        GpuCheck::ReportPass("gpu.shutdown", "device released, pipeline cache empty");
    } else {
        GpuCheck::ReportFail("gpu.shutdown", "device still initialized or cache not released");
    }

    glfwDestroyWindow(window);
    if(!headless) glfwTerminate();
    GpuCheck::PrintSummary();
    return GpuCheck::GetFailCount() == 0 ? 0 : 1;
}
