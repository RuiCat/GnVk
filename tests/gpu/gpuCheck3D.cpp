// P1b 3D 站点验收实现：用 identity VP 的全屏四边形固定绕序/深度，逐站点回读像素与深度。
#include "gpuCheck3D.h"

#include <GN.h>
#include "GN_Vk.h"
#include "GN_3Dlight.h"
#include "giParams.h" // G2a：srgbDecode（G-Buffer 附件 1 为 RGBA8_SRGB）
#include "postParams.h" // L1：POST UBO（曝光/色调映射）
#include "atmosphereParams.h" // L4c：大气 LUT CPU 期望值

#include "gpuCheckReport.h"
#include "gpuPixelRead.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

namespace GpuCheck {
namespace {

const float kColorTolerance = 0.02f;
const uint32_t kQuadIndexAB[6] = {0, 1, 2, 0, 2, 3};
const uint32_t kQuadIndexBA[6] = {0, 2, 1, 0, 3, 2};

struct sQUAD {
    GNg::VkVertex3D verts[4];
};

void MakeQuad(sQUAD &quad, float z) {
    std::memset(&quad, 0, sizeof(quad));
    const float xs[4] = {-1.0f, 1.0f, 1.0f, -1.0f};
    const float ys[4] = {-1.0f, -1.0f, 1.0f, 1.0f};
    for(int i = 0; i < 4; ++i) {
        quad.verts[i].px = xs[i];
        quad.verts[i].py = ys[i];
        quad.verts[i].pz = z;
        quad.verts[i].nx = 0.0f;
        quad.verts[i].ny = 0.0f;
        quad.verts[i].nz = 1.0f;
        quad.verts[i].tx = 1.0f; // G-Buffer 顶点着色器对零切线 normalize 会得到 NaN
        quad.verts[i].ty = 0.0f;
        quad.verts[i].tz = 0.0f;
        quad.verts[i].u = (i == 1 || i == 2) ? 1.0f : 0.0f;
        quad.verts[i].v = (i >= 2) ? 1.0f : 0.0f;
    }
}

GN3d::sLIGHT_UBO MakeLight(bool withSun) {
    GN3d::sLIGHT_UBO light;
    std::memset(&light, 0, sizeof(light));
    light.ambientLight[0] = 1.0f;
    light.ambientLight[1] = 1.0f;
    light.ambientLight[2] = 1.0f;
    light.ambientLight[3] = 1.0f;
    light.shadowParams[0] = 2048.0f; // L3c：阴影图分辨率（旧 shadowMapSize 已并入 shadowParams）
    light.shadowParams[1] = 1.0f;    // 本站点用单级阴影
    if(withSun) {
        light.lightCount = 1.0f;
        light.lights[0].color[0] = 1.0f;
        light.lights[0].color[1] = 1.0f;
        light.lights[0].color[2] = 1.0f;
        light.lights[0].color[3] = 1.0f;
        light.lights[0].position[3] = 0.0f; // 方向光
        light.lights[0].direction[0] = 0.0f;
        light.lights[0].direction[1] = 0.0f;
        light.lights[0].direction[2] = -1.0f;
        light.lights[0].direction[3] = 0.0f; // 无阴影采样
        light.lights[0].attenuation[0] = 1.0f;
    }
    return light;
}

bool BeginFrame() {
    return GNg::GN_VkBeginFrame();
}

void EndFrameAndWait() {
    GNg::GN_VkEndFrame();
    vkQueueWaitIdle(GNg::GN_Vk.graphicsQueue);
}

bool ReadCenter(std::vector<uint8_t> &pixels, sFLOAT_PIXEL &center) {
    const uint32_t width = GNg::GN_Vk.swapchainExtent.width;
    const uint32_t height = GNg::GN_Vk.swapchainExtent.height;
    if(!ReadSwapchain(0, 0, (int)width, (int)height, pixels)) return false;
    center = SampleSwapchain(pixels, (int)width, (int)(width / 2), (int)(height / 2));
    return true;
}

const char *PixelText(const sFLOAT_PIXEL &p) {
    static char text[128];
    std::snprintf(text, sizeof(text), "RGBA=%.3f,%.3f,%.3f,%.3f", p.r, p.g, p.b, p.a);
    return text;
}

bool CheckNonClear(const char *name, const sFLOAT_PIXEL &p) {
    const float maxChannel = p.r > p.g ? (p.r > p.b ? p.r : p.b) : (p.g > p.b ? p.g : p.b);
    if(maxChannel > 0.04f) {
        ReportPass(name, PixelText(p));
        return true;
    }
    ReportFail(name, PixelText(p));
    return false;
}

bool CheckGreen(const char *name, const sFLOAT_PIXEL &p) {
    if(std::fabs(p.r) <= kColorTolerance && std::fabs(p.g - 1.0f) <= kColorTolerance &&
       std::fabs(p.b) <= kColorTolerance) {
        ReportPass(name, PixelText(p));
        return true;
    }
    ReportFail(name, PixelText(p));
    return false;
}

bool CheckForwardClear(const char *name, const sFLOAT_PIXEL &p) {
    // 前向 pass 的 clear = 线性 (0.05,0.05,0.1)。
    if(std::fabs(p.r - 0.05f) <= kColorTolerance && std::fabs(p.g - 0.05f) <= kColorTolerance &&
       std::fabs(p.b - 0.1f) <= kColorTolerance) {
        ReportPass(name, PixelText(p));
        return true;
    }
    ReportFail(name, PixelText(p));
    return false;
}

bool CheckBlack(const char *name, const sFLOAT_PIXEL &p) {
    if(p.r <= kColorTolerance && p.g <= kColorTolerance && p.b <= kColorTolerance) {
        ReportPass(name, PixelText(p));
        return true;
    }
    ReportFail(name, PixelText(p));
    return false;
}

void RunForwardChecks(const GNg::sMAT44 &identity, const GNg::sMAT44 &model,
                      const GNg::sVEC3 &eye, const GN3d::sLIGHT_UBO &light) {
    if(!GNg::GN_Vk.pipeline3D) {
        ReportSkip("3d.forward", "pipeline3D missing (see pipe.3d.forward)");
        return;
    }
    sQUAD quad;
    std::vector<uint8_t> pixels;
    sFLOAT_PIXEL pixel;

    MakeQuad(quad, 0.5f);
    if(BeginFrame()) {
        GNg::GN_VkBegin3DPass(&identity, &eye, &light, sizeof(light));
        GNg::GN_VkDraw3D(quad.verts, 4, kQuadIndexBA, 6, &model, 0.0f, 1.0f, 0.0f);
        GNg::GN_VkEnd3DPass();
        EndFrameAndWait();
        if(ReadCenter(pixels, pixel)) CheckGreen("3d.forward", pixel);
        else ReportFail("3d.forward", "swapchain readback failed");
    } else {
        ReportFail("3d.forward", "GN_VkBeginFrame failed");
    }

    // AB 绕序在前向管线（CCW 正面 + 正视口）应被背面剔除，只剩 clear 色。
    if(BeginFrame()) {
        GNg::GN_VkBegin3DPass(&identity, &eye, &light, sizeof(light));
        GNg::GN_VkDraw3D(quad.verts, 4, kQuadIndexAB, 6, &model, 1.0f, 0.0f, 0.0f);
        GNg::GN_VkEnd3DPass();
        EndFrameAndWait();
        if(ReadCenter(pixels, pixel)) CheckForwardClear("3d.forward.cull", pixel);
        else ReportFail("3d.forward.cull", "swapchain readback failed");
    } else {
        ReportFail("3d.forward.cull", "GN_VkBeginFrame failed");
    }

    // 深度 LESS：后绘制的近平面应覆盖先绘制的远平面。
    sQUAD nearQuad;
    sQUAD farQuad;
    MakeQuad(nearQuad, 0.3f);
    MakeQuad(farQuad, 0.7f);
    if(BeginFrame()) {
        GNg::GN_VkBegin3DPass(&identity, &eye, &light, sizeof(light));
        GNg::GN_VkDraw3D(farQuad.verts, 4, kQuadIndexBA, 6, &model, 1.0f, 0.0f, 0.0f);
        GNg::GN_VkDraw3D(nearQuad.verts, 4, kQuadIndexBA, 6, &model, 0.0f, 1.0f, 0.0f);
        GNg::GN_VkEnd3DPass();
        EndFrameAndWait();
        if(ReadCenter(pixels, pixel)) CheckGreen("3d.forward.depth", pixel);
        else ReportFail("3d.forward.depth", "swapchain readback failed");
    } else {
        ReportFail("3d.forward.depth", "GN_VkBeginFrame failed");
    }
}

void RunShadowCheck(const GNg::sMAT44 &identity, const GNg::sMAT44 &model) {
    if(!GNg::GN_Vk.shadowPipeline) {
        ReportSkip("3d.shadow", "shadowPipeline missing (see pipe.3d.shadow)");
        return;
    }
    sQUAD quad;
    MakeQuad(quad, 0.5f);
    if(!BeginFrame()) {
        ReportFail("3d.shadow", "GN_VkBeginFrame failed");
        return;
    }
    GNg::GN_VkShadowPassBegin(&identity);
    GNg::GN_VkShadowDraw(quad.verts, 4, kQuadIndexAB, 6, &model);
    GNg::GN_VkShadowPassEnd();
    EndFrameAndWait();

    std::vector<float> depth;
    if(!ReadDepthImage(GNg::GN_Vk.shadowImage, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
                       GNg::GN_Vk.shadowMapSize, GNg::GN_Vk.shadowMapSize, depth)) {
        ReportFail("3d.shadow", "depth readback failed");
        return;
    }
    const uint32_t size = GNg::GN_Vk.shadowMapSize;
    const float center = depth[(size / 2) * size + size / 2];
    char detail[128];
    std::snprintf(detail, sizeof(detail), "shadowDepth=%.3f expected~0.5", center);
    if(center > 0.3f && center < 0.7f) ReportPass("3d.shadow", detail);
    else ReportFail("3d.shadow", detail);
}

void RunPbrChecks(const GNg::sMAT44 &identity, const GNg::sMAT44 &model,
                  const GNg::sVEC3 &eye, const GN3d::sLIGHT_UBO &light) {
    if(!GNg::GN_Vk.pipelinePBR) {
        ReportSkip("3d.pbr", "pipelinePBR missing (see pipe.3d.pbr)");
        return;
    }
    sQUAD quad;
    MakeQuad(quad, 0.5f);
    std::vector<uint8_t> pixels;
    sFLOAT_PIXEL pixel;

    if(BeginFrame()) {
        GNg::GN_VkBegin3DPassPBR(&identity, &eye, &light, sizeof(light));
        GNg::GN_VkDraw3DPBR(quad.verts, 4, kQuadIndexBA, 6, &model, 1.0f, 0.0f, 0.0f,
                            0.9f, 0.0f, 1.0f);
        GNg::GN_VkEnd3DPass();
        EndFrameAndWait();
        if(ReadCenter(pixels, pixel)) CheckNonClear("3d.pbr", pixel);
        else ReportFail("3d.pbr", "swapchain readback failed");
    } else {
        ReportFail("3d.pbr", "GN_VkBeginFrame failed");
    }

    if(BeginFrame()) {
        GNg::GN_VkBegin3DPassPBR(&identity, &eye, &light, sizeof(light));
        GNg::GN_VkDraw3DPBR(quad.verts, 4, kQuadIndexAB, 6, &model, 1.0f, 0.0f, 0.0f,
                            0.9f, 0.0f, 1.0f);
        GNg::GN_VkEnd3DPass();
        EndFrameAndWait();
        if(ReadCenter(pixels, pixel)) CheckBlack("3d.pbr.cull", pixel);
        else ReportFail("3d.pbr.cull", "swapchain readback failed");
    } else {
        ReportFail("3d.pbr.cull", "GN_VkBeginFrame failed");
    }
}

// 直接用 G-Buffer 管线绘制：绕开 GN_VkDraw3D_GBuffer 在命令缓冲录制中更新已绑定
// 描述符集的路径问题（该问题会让部分驱动把整条命令缓冲判为无效）。
void DrawGBufferQuadRaw(const sQUAD &quad, const GNg::sMAT44 &model,
                        float albedoR, float albedoG, float albedoB,
                        float roughness, float metallic, float ao) {
    const VkDevice device = GNg::GN_Vk.device;
    VkCommandBuffer cb = GNg::GN_Vk.commandBuffers[GNg::GN_Vk.currentFrame];
    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, GNg::GN_Vk.gBufferPipeline);
    vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, GNg::GN_Vk.gBufferPipelineLayout,
                            0, 1, &GNg::GN_Vk.gBufferDescriptorSet, 0, 0);

    GNg::VkModelPushConstPBR pc;
    std::memset(&pc, 0, sizeof(pc));
    std::memcpy(pc.model, model.m, sizeof(pc.model));
    pc.albedo[0] = albedoR;
    pc.albedo[1] = albedoG;
    pc.albedo[2] = albedoB;
    pc.albedo[3] = 1.0f;
    pc.pbrParams[0] = roughness;
    pc.pbrParams[1] = metallic;
    pc.pbrParams[2] = ao;
    vkCmdPushConstants(cb, GNg::GN_Vk.gBufferPipelineLayout,
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(pc), &pc);

    void *mapped = 0;
    vkMapMemory(device, GNg::GN_Vk.vertexMemory3D, 0, sizeof(quad.verts), 0, &mapped);
    std::memcpy(mapped, quad.verts, sizeof(quad.verts));
    vkUnmapMemory(device, GNg::GN_Vk.vertexMemory3D);
    // G-Buffer 管线 frontFace=CCW；identity VP（无 Y 翻转）下 BA 才是帧缓冲 CCW ⇒ 正面。
    vkMapMemory(device, GNg::GN_Vk.indexMemory3D, 0, sizeof(kQuadIndexBA), 0, &mapped);
    std::memcpy(mapped, kQuadIndexBA, sizeof(kQuadIndexBA));
    vkUnmapMemory(device, GNg::GN_Vk.indexMemory3D);

    VkDeviceSize vertexOffset = 0;
    vkCmdBindVertexBuffers(cb, 0, 1, &GNg::GN_Vk.vertexBuffer3D, &vertexOffset);
    vkCmdBindIndexBuffer(cb, GNg::GN_Vk.indexBuffer3D, 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(cb, 6, 1, 0, 0, 0);
}

void RunDeferredChecks(const GNg::sMAT44 &identity, const GNg::sMAT44 &model,
                       const GNg::sVEC3 &eye, const GN3d::sLIGHT_UBO &light) {
    if(!GNg::GN_Vk.gBufferPipeline || !GNg::GN_Vk.deferredLightPipeline) {
        ReportSkip("3d.deferred", "gBuffer/deferredLight pipeline missing");
        return;
    }
    sQUAD quad;
    MakeQuad(quad, 0.5f);
    std::vector<uint8_t> pixels;
    sFLOAT_PIXEL pixel;

    if(!BeginFrame()) {
        ReportFail("3d.deferred", "GN_VkBeginFrame failed");
        return;
    }
    GNg::GN_VkBegin3DPassDeferred(&identity, &eye, &light, sizeof(light));
    DrawGBufferQuadRaw(quad, model, 0.9f, 0.1f, 0.2f, 0.5f, 0.0f, 1.0f);
    // G2b/G4：延迟路径的可选 pass（默认 GI UBO 开关为 0 时两者直接返回，不改变本用例结果）。
    // 放在 DeferredLighting 之前，保证桌面验收开启 SSAO/SSGI 时真的有调用点。
    GNg::GN_VkSSAOPass();
    GNg::GN_VkSSGIPass();
    GNg::GN_VkUpdateProbes(); // G5b-2：探针每帧更新入口（weight=0 时直接返回，no-op）
    GNg::GN_VkDeferredLighting(&identity, &eye, &light, sizeof(light));
    EndFrameAndWait();

    if(ReadCenter(pixels, pixel)) CheckNonClear("3d.deferred.light", pixel);
    else ReportFail("3d.deferred.light", "swapchain readback failed");

    std::vector<uint8_t> albedo;
    // G2a：附件 1 = RGBA8_SRGB（4 B/px）；读取原始 sRGB 字节后用 srgbDecode 还原线性值比较。
    if(!ReadColorImage(GNg::GN_Vk.gAlbedoMetalImage, VK_FORMAT_R8G8B8A8_SRGB,
                       VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                       GNg::GN_Vk.swapchainExtent.width, GNg::GN_Vk.swapchainExtent.height,
                       albedo)) {
        ReportFail("3d.deferred.gbuffer", "albedo readback failed");
        return;
    }
    const uint32_t width = GNg::GN_Vk.swapchainExtent.width;
    const size_t offset = ((size_t)(GNg::GN_Vk.swapchainExtent.height / 2) * width +
                           width / 2) * 4u;
    const float r = GN3d::srgbDecode((float)albedo[offset + 0] / 255.0f);
    const float g = GN3d::srgbDecode((float)albedo[offset + 1] / 255.0f);
    const float b = GN3d::srgbDecode((float)albedo[offset + 2] / 255.0f);
    char detail[160];
    std::snprintf(detail, sizeof(detail), "gbufferAlbedo=%.3f,%.3f,%.3f expected~0.9,0.1,0.2",
                  r, g, b);
    if(std::fabs(r - 0.9f) < 0.01f && std::fabs(g - 0.1f) < 0.01f &&
       std::fabs(b - 0.2f) < 0.01f) {
        ReportPass("3d.deferred.gbuffer", detail);
    } else {
        ReportFail("3d.deferred.gbuffer", detail);
    }
}

void RunCompositeCheck(const GNg::sMAT44 &identity, const GNg::sMAT44 &model,
                       const GNg::sVEC3 &eye, const GN3d::sLIGHT_UBO &light) {
    if(!GNg::GN_Vk.compositePipeline || !GNg::GN_Vk.pipelinePBR) {
        ReportSkip("3d.composite", "composite/PBR pipeline missing");
        return;
    }
    sQUAD quad;
    MakeQuad(quad, 0.5f);
    if(!BeginFrame()) {
        ReportFail("3d.composite", "GN_VkBeginFrame failed");
        return;
    }
    GNg::GN_VkBegin3DPassPBR_HDR(&identity, &eye, &light, sizeof(light));
    GNg::GN_VkDraw3DPBR(quad.verts, 4, kQuadIndexBA, 6, &model, 1.0f, 0.0f, 0.0f, 0.9f, 0.0f, 1.0f);
    GNg::GN_VkEnd3DPass();
    GNg::GN_VkCompositeToSwapchain();
    EndFrameAndWait();

    std::vector<uint8_t> pixels;
    sFLOAT_PIXEL pixel;
    if(ReadCenter(pixels, pixel)) CheckNonClear("3d.composite", pixel);
    else ReportFail("3d.composite", "swapchain readback failed");
}

void RunSkyboxCheck(const GNg::sMAT44 &identity, const GNg::sVEC3 &eye,
                    const GN3d::sLIGHT_UBO &light) {
    if(!GNg::GN_Vk.skyboxPipeline) {
        ReportSkip("3d.skybox", "skyboxPipeline missing (see pipe.skybox)");
        return;
    }
    // 天空盒用真实透视 VP：identity 会让立方体落在远平面之外，读回只剩 clear 色。
    GNg::sVEC3 target = {0.0f, 0.0f, 0.0f};
    GNg::sVEC3 up = {0.0f, 1.0f, 0.0f};
    GNg::sMAT44 view;
    GNg::Mat44LookAtRH(&view, &eye, &target, &up);
    GNg::sMAT44 proj;
    GNg::Mat44PerspectiveVK(&proj, 0.785398f, 640.0f / 480.0f, 0.1f, 100.0f);
    GNg::sMAT44 vp;
    GNg::Mat44Mul(&vp, &proj, &view);
    (void)identity;
    if(!BeginFrame()) {
        ReportFail("3d.skybox", "GN_VkBeginFrame failed");
        return;
    }
    GNg::GN_VkBegin3DPass(&vp, &eye, &light, sizeof(light));
    GNg::GN_VkDrawSkybox(&vp);
    GNg::GN_VkEnd3DPass();
    EndFrameAndWait();

    std::vector<uint8_t> pixels;
    sFLOAT_PIXEL pixel;
    if(!ReadCenter(pixels, pixel)) {
        ReportFail("3d.skybox", "swapchain readback failed");
        return;
    }
    char detail[128];
    std::snprintf(detail, sizeof(detail), "skybox RGBA=%.3f,%.3f,%.3f", pixel.r, pixel.g, pixel.b);
    const float maxChannel = pixel.r > pixel.g ? (pixel.r > pixel.b ? pixel.r : pixel.b)
                                               : (pixel.g > pixel.b ? pixel.g : pixel.b);
    const float diff = std::fabs(pixel.r - 0.05f) + std::fabs(pixel.g - 0.05f) +
                       std::fabs(pixel.b - 0.1f);
    if(maxChannel > 0.02f && diff > 0.05f) ReportPass("3d.skybox", detail);
    else ReportFail("3d.skybox", detail);
}

// L1：后处理数值断言——线性 1.0 的绿色四边形经不同 tonemap 后的中心像素。
// 期望值来自 postParams.cpp 的 CPU 参考公式（ACES(1)=2.54/3.16、Reinhard(1)=0.5、ACES(2)=10.1/11.04）。
void RunPostChecks(const GNg::sMAT44 &identity, const GNg::sMAT44 &model,
                   const GNg::sVEC3 &eye, const GN3d::sLIGHT_UBO &light) {
    if(!GNg::GN_Vk.compositePipeline) {
        ReportSkip("3d.post.aces", "compositePipeline missing (see pipe.3d.composite)");
        return;
    }
    sQUAD quad;
    MakeQuad(quad, 0.5f);
    std::vector<uint8_t> pixels;
    sFLOAT_PIXEL pixel;

    const auto expectGreen = [&](const char *name, const GN3d::sPOST_UBO &post, float expected) {
        GNg::GN_VkSetPost(post);
        if(!BeginFrame()) {
            ReportFail(name, "GN_VkBeginFrame failed");
            return;
        }
        GNg::GN_VkBegin3DPass(&identity, &eye, &light, sizeof(light));
        GNg::GN_VkDraw3D(quad.verts, 4, kQuadIndexBA, 6, &model, 0.0f, 1.0f, 0.0f);
        GNg::GN_VkEnd3DPass();
        EndFrameAndWait();
        if(!ReadCenter(pixels, pixel)) {
            ReportFail(name, "swapchain readback failed");
            return;
        }
        char detail[160];
        std::snprintf(detail, sizeof(detail), "g=%.4f expected=%.4f", pixel.g, expected);
        if(std::fabs(pixel.g - expected) <= 0.012f) ReportPass(name, detail);
        else ReportFail(name, detail);
    };

    GN3d::sPOST_UBO aces = GN3d::makeDefaultPostUbo();
    aces.bloomIntensity = 0.0f; // 关 Bloom，只验 tonemap
    aces.exposure = 1.0f;
    aces.tonemapMode = GN3d::eTONEMAP_ACES;
    expectGreen("3d.post.aces", aces, 2.54f / 3.16f);

    GN3d::sPOST_UBO reinhard = aces;
    reinhard.tonemapMode = GN3d::eTONEMAP_REINHARD;
    expectGreen("3d.post.reinhard", reinhard, 0.5f);

    GN3d::sPOST_UBO exposure = aces;
    exposure.exposure = 2.0f; // ACES(2)
    expectGreen("3d.post.exposure", exposure, 10.1f / 11.04f);

    // 恢复回归档，避免影响后续站点。
    GNg::GN_VkSetPost(GN3d::makeLegacyPostUbo());
}

// L4c：大气 LUT 上传校验——回读透射率纹理，与 CPU 期望值逐点比较；sky-view 中心项需有限。
void RunAtmosphereLutCheck() {
    VkImage trans = VK_NULL_HANDLE;
    VkImage sky = VK_NULL_HANDLE;
    if(!GNg::GN_VkGetAtmosphereLut(&trans, &sky)) {
        ReportSkip("atmo.lut", "atmosphere LUT not ready");
        return;
    }
    const uint32_t tw = (uint32_t)GN3d::ATMOSPHERE_TRANSMITTANCE_WIDTH;
    const uint32_t th = (uint32_t)GN3d::ATMOSPHERE_TRANSMITTANCE_HEIGHT;
    std::vector<uint8_t> bytes;
    if(!ReadColorImage(trans, VK_FORMAT_R16G16B16A16_SFLOAT,
                       VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, tw, th, bytes)) {
        ReportFail("atmo.lut", "transmittance readback failed");
        return;
    }
    GN3d::sATMOSPHERE_PARAMS params = GN3d::makeDefaultAtmosphereParams();
    std::vector<float> expected((size_t)tw * (size_t)th * 4u, 0.0f);
    if(!GN3d::computeTransmittanceLut(params, (int)tw, (int)th, expected.data())) {
        ReportFail("atmo.lut", "CPU transmittance LUT failed");
        return;
    }
    const uint32_t points[3][2] = {{1u, 1u}, {tw / 2u, th / 2u}, {tw - 2u, th - 2u}};
    float maxError = 0.0f;
    for(int i = 0; i < 3; ++i) {
        const uint32_t x = points[i][0];
        const uint32_t y = points[i][1];
        const size_t offset = ((size_t)y * (size_t)tw + (size_t)x) * 8u;
        if(offset + 2u > bytes.size()) continue;
        uint16_t half = 0;
        std::memcpy(&half, bytes.data() + offset, sizeof(half));
        const float gpuValue = GpuCheck::HalfToFloat(half);
        const float cpuValue = expected[((size_t)y * (size_t)tw + (size_t)x) * 4u];
        const float error = std::fabs(gpuValue - cpuValue);
        if(error > maxError) maxError = error;
    }

    // L4：sky-view LUT 的 GPU 数值 vs CPU 参考（与 init 相同的 45° 太阳；容差 2%）。
    std::vector<uint8_t> skyBytes;
    bool skyFinite = false;
    float skyMaxErr = 0.0f;
    const uint32_t sw = (uint32_t)GN3d::ATMOSPHERE_SKYVIEW_LUT_WIDTH;
    const uint32_t sh = (uint32_t)GN3d::ATMOSPHERE_SKYVIEW_LUT_HEIGHT;
    if(ReadColorImage(sky, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                      sw, sh, skyBytes) &&
       skyBytes.size() >= (size_t)sw * sh * 8u) {
        std::vector<float> skyCpu((size_t)sw * (size_t)sh * 4u, 0.0f);
        const float sun[3] = {0.5f, 0.70710678f, 0.5f};
        if(GN3d::computeSkyViewLut(params, expected.data(), (int)tw, (int)th, sun, (int)sw, (int)sh,
                                   skyCpu.data())) {
            skyFinite = true;
            const uint32_t points[3][2] = {{sw / 8u, sh / 8u},
                                           {sw / 2u, sh / 2u},
                                           {sw - 9u, sh - 9u}};
            for(int i = 0; i < 3; ++i) {
                const size_t texel = ((size_t)points[i][1] * sw + points[i][0]);
                for(int c = 0; c < 3; ++c) {
                    uint16_t half = 0;
                    std::memcpy(&half, skyBytes.data() + texel * 8u + (size_t)c * 2u, sizeof(half));
                    const float gpuValue = GpuCheck::HalfToFloat(half);
                    const float cpuValue = skyCpu[texel * 4u + (size_t)c];
                    if(!std::isfinite(gpuValue) || !std::isfinite(cpuValue)) {
                        skyFinite = false;
                        continue;
                    }
                    if(cpuValue <= 1.0e-4f) continue;
                    const float rel = std::fabs(gpuValue - cpuValue) / cpuValue;
                    if(rel > skyMaxErr) skyMaxErr = rel;
                }
            }
        }
    }
    char skyDetail[160];
    std::snprintf(skyDetail, sizeof(skyDetail), "skyMaxRelErr=%.4f", skyMaxErr);
    if(!skyFinite) ReportSkip("atmo.skyview", "sky-view readback/CPU reference failed");
    else if(skyMaxErr <= 0.02f) ReportPass("atmo.skyview", skyDetail);
    else ReportFail("atmo.skyview", skyDetail);

    char detail[160];
    std::snprintf(detail, sizeof(detail), "transMaxErr=%.5f sky=%s", maxError,
                  skyFinite ? "finite" : "bad");
    if(maxError <= 0.02f && skyFinite) ReportPass("atmo.lut", detail);
    else ReportFail("atmo.lut", detail);
}

// L3c：4 级 CSM 接线就绪性（按层内容读回需桌面 GPU；此处断言级数与阴影图存在）。
void RunShadowCascadeCheck() {
    const uint32_t count = GNg::GN_VkShadowCascadeCount();
    char detail[128];
    std::snprintf(detail, sizeof(detail), "cascadeCount=%u shadowImage=%s", count,
                  GNg::GN_Vk.shadowImage != VK_NULL_HANDLE ? "ready" : "missing");
    if(count == 4u && GNg::GN_Vk.shadowImage != VK_NULL_HANDLE) ReportPass("3d.shadow.cascades", detail);
    else ReportFail("3d.shadow.cascades", detail);
}

} // namespace

void Run3DChecks() {
    GNg::sMAT44 identity;
    GNg::Mat44Identity(&identity);
    GNg::sMAT44 model;
    GNg::Mat44Identity(&model);
    GNg::sVEC3 eye = {0.0f, 0.0f, 1.0f};
    const GN3d::sLIGHT_UBO ambient = MakeLight(false);
    const GN3d::sLIGHT_UBO sun = MakeLight(true);

    RunShadowCheck(identity, model); // 先跑阴影，确保 PBR 采样时深度图布局有效
    RunShadowCascadeCheck();
    RunForwardChecks(identity, model, eye, ambient);
    RunPbrChecks(identity, model, eye, sun);
    RunDeferredChecks(identity, model, eye, sun);
    RunCompositeCheck(identity, model, eye, sun);
    RunAtmosphereLutCheck();

    RunSkyboxCheck(identity, eye, ambient);
    RunPostChecks(identity, model, eye, ambient);
}

} // namespace GpuCheck
