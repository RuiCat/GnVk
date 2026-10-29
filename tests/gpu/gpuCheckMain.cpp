// GPU 桌面验收入口：11 条 P1b 管线 + 2D 四混合 + 3D 六站点 + P1c 材质实例 + P2 GPU 蒙皮 + P8 剔除/调试线框。
//
// 运行：make gpuCheck
// 真实 GPU 桌面验收必须用本机 /dev/dri 上的 Vulkan 设备；无设备时退出码 2 = SKIP。
#include <GN.h>
#include "GN_Vk.h"

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
#include "gpuCheckCloud.h"
#include "gpuCheckPrecipitation.h"
#include "gpuCheckDecal.h"
#include "gpuCheckReport.h"
#include "gpuCheckSkin.h"
#include "gpuCheckTexture.h"
#include "gpuPixelRead.h"

#include <GLFW/glfw3.h>

#include <cstdio>
#include <cstring>

namespace {

void CheckPipeline(const char *name, VkPipeline pipeline) {
    if(pipeline != VK_NULL_HANDLE) {
        GpuCheck::ReportPass(name, "pipeline created");
    } else {
        GpuCheck::ReportFail(name, "pipeline is VK_NULL_HANDLE");
    }
}

void CheckCreation() {
    const unsigned count = (unsigned)GNg::GN_Vk.aPipelineCache.GetCount();
    const unsigned long long miss = GNg::GN_Vk.aPipelineCache.GetMissCount();
    const unsigned long long hit = GNg::GN_Vk.aPipelineCache.GetHitCount();
    char detail[160];
    std::snprintf(detail, sizeof(detail), "count=%u miss=%llu hit=%llu expected >=11 (P1b 11 + 新增子系统)",
                  count, miss, hit);
    // P1c/P2/P8 的管线也在 GN_VkInit 里经同一缓存创建，因此只要求不少于 P1b 的 11 条。
    if(count >= 11 && hit == 0) GpuCheck::ReportPass("cache.entries", detail);
    else GpuCheck::ReportFail("cache.entries", detail);

    CheckPipeline("pipe.2d.normal", GNg::GN_Vk.pipeline);
    CheckPipeline("pipe.2d.bright", GNg::GN_Vk.pipelineBright);
    CheckPipeline("pipe.2d.multiply", GNg::GN_Vk.pipelineMultiply);
    CheckPipeline("pipe.2d.null", GNg::GN_Vk.pipelineNull);
    CheckPipeline("pipe.3d.forward", GNg::GN_Vk.pipeline3D);
    CheckPipeline("pipe.3d.pbr", GNg::GN_Vk.pipelinePBR);
    CheckPipeline("pipe.3d.shadow", GNg::GN_Vk.shadowPipeline);
    CheckPipeline("pipe.composite", GNg::GN_Vk.compositePipeline);
    CheckPipeline("pipe.skybox", GNg::GN_Vk.skyboxPipeline);
    CheckPipeline("pipe.gbuffer", GNg::GN_Vk.gBufferPipeline);
    CheckPipeline("pipe.deferredLight", GNg::GN_Vk.deferredLightPipeline);
}

void PrintDevice() {
    VkPhysicalDeviceProperties props;
    std::memset(&props, 0, sizeof(props));
    vkGetPhysicalDeviceProperties(GNg::GN_Vk.physicalDevice, &props);
    std::printf("DEVICE: %s (vendor=0x%04x device=0x%04x api=%u.%u.%u)\n",
                props.deviceName, props.vendorID, props.deviceID,
                VK_VERSION_MAJOR(props.apiVersion), VK_VERSION_MINOR(props.apiVersion),
                VK_VERSION_PATCH(props.apiVersion));
    std::printf("SWAPCHAIN: %ux%u format=%d bytesPerPixel=%u\n",
                GNg::GN_Vk.swapchainExtent.width, GNg::GN_Vk.swapchainExtent.height,
                (int)GNg::GN_Vk.swapchainFormat, GpuCheck::SwapchainBytesPerPixel());
}

} // namespace

int main() {
    std::printf("==== P1b GPU pipeline acceptance ====\n");
    if(!glfwInit()) {
        GpuCheck::ReportSkip("gpu.init", "glfwInit failed (no display)");
        return 2;
    }
    glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE); // Wayland 隐藏窗口会在 present 处卡住
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow *window = glfwCreateWindow(640, 480, "GnVk P1b GPU Check", 0, 0);
    if(!window) {
        glfwTerminate();
        GpuCheck::ReportSkip("gpu.init", "glfwCreateWindow failed");
        return 2;
    }
    GNg::GN_VkInit(window, 640, 480);
    if(!GNg::GN_Vk.initialized) {
        glfwDestroyWindow(window);
        glfwTerminate();
        GpuCheck::ReportSkip("gpu.init", "GN_VkInit failed: no Vulkan device");
        GpuCheck::PrintSummary();
        return 2;
    }

    PrintDevice();
    CheckCreation();
    GpuCheck::Run2DChecks();
    GpuCheck::Run3DChecks();
    GpuCheck::RunMaterialChecks();
    GpuCheck::RunTextureChecks();
    GpuCheck::RunSkinChecks();
    GpuCheck::RunCullChecks();
    GpuCheck::RunParticleChecks();
    GpuCheck::RunClothChecks();
    GpuCheck::RunSkinSsboChecks();
    GpuCheck::RunBatchChecks();
    GpuCheck::RunLodChecks();
    GpuCheck::RunFogChecks();
    GpuCheck::RunCloudChecks();
    GpuCheck::RunPrecipitationChecks();
    GpuCheck::RunDecalChecks();

    GNg::GN_VkShutdown();
    if(!GNg::GN_Vk.initialized && GNg::GN_Vk.aPipelineCache.GetCount() == 0) {
        GpuCheck::ReportPass("gpu.shutdown", "device released, pipeline cache empty");
    } else {
        GpuCheck::ReportFail("gpu.shutdown", "device still initialized or cache not released");
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    GpuCheck::PrintSummary();
    return GpuCheck::GetFailCount() == 0 ? 0 : 1;
}
