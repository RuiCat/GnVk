// vkCloud.cpp — P11-S2b 中景 billboard 云层：无顶点缓冲的 tile 四边形 + 程序化 FBM alpha。
//
// 契约（GN_Vk.h 冻结）：
//   * GN_VkDrawCloud：aTilesPerAxis² × 3 层个四边形（每片 6 顶点、无顶点缓冲），顶点着色器按
//     gl_VertexIndex 算出层/tile/角点；只在 3D pass 内提交。
//   * GN_VkCloudDestroy：释放 UBO / 描述符集 / 管线布局；可重复调用（管线归 aPipelineCache）。
//
// 实现要点：
//   * 布局选「水平面四边形」变体：云层是 y = aLayerHeight 的水平面，法线 = 世界 +Y；
//     面内两轴 = 相机右向的水平投影（CPU 取 viewProj 第 0 行；M = P·V 的第 0 行 = P00·相机右向，
//     不用已知对纯平移矩阵出错的 Mat44Inverse）与 cross(+Y, right)。
//     tile 格点在同一正交基上铺开 ⇒ 相邻四边形共边，无接缝。
//     噪声按世界 xz 取样，因此相机偏航只移动 tile 边界、不旋转云纹。
//   * 渲染状态：alpha 混合、深度测试开、深度写关、不剔除；renderPass3D + GN_Vk.msaaSamples。
//   * set 0 binding 0 = 160 B host-visible UBO（sCLOUD_UBO 与 gn3d_cloud.vert/frag 逐字段一致）。
//   * 管线/布局/描述符集在首次 Draw 时惰性创建；失败返回 false 并回滚本次创建。
#include <GN.h>
#include "GN_Vk.h"
#include "vkInternal.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

namespace GNg {

namespace {

// 云盘层数与层间距（层高 = aLayerHeight - L*间距；tile 尺寸与风速按层放大）。
const uint32_t kCloudLayers = 3;
const float kCloudLayerSpacing = 40.0f;

// 与 gn3d_cloud.vert/frag 的 CloudUBO 逐字段对应（std140，全部 vec4 对齐）。
struct alignas(16) sCLOUD_UBO {
    float aViewProj[16]; // 64 B
    float aCameraPos[4]; // xyz = 相机世界位置, w = nearFade
    float aWindTime[4]; // x/y = 风 (x,z), z = 时间秒
    float aParams0[4]; // x = tileSize, y = tilesPerAxis, z = coverage, w = sharpness
    float aParams1[4]; // x = opacity, y = 底层高度, z = 层数, w = 层间距
    float aCameraRight[4]; // xyz = 相机右向的水平投影（单位）
    float aCameraUp[4]; // xyz = 世界 +Y（云层法线）
};
static_assert(sizeof(sCLOUD_UBO) == 160, "sCLOUD_UBO 必须为 160 字节");

// 本模块持有的 GPU 资源；VkPipeline 归 aPipelineCache 所有。
VkDescriptorSetLayout s_cloudSetLayout = VK_NULL_HANDLE;
VkPipelineLayout s_cloudPipelineLayout = VK_NULL_HANDLE;
VkPipeline s_cloudPipeline = VK_NULL_HANDLE;
VkBuffer s_cloudUboBuffer = VK_NULL_HANDLE;
VkDeviceMemory s_cloudUboMemory = VK_NULL_HANDLE;
void *s_cloudUboMapped = 0;
VkDescriptorSet s_cloudSet = VK_NULL_HANDLE;
std::vector<uint32_t> s_cloudVertSpv;
std::vector<uint32_t> s_cloudFragSpv;

GN3d::sPIPELINE_DESC makeCloudPipelineDesc() {
    GN3d::sPIPELINE_DESC desc;
    desc.aKind = GN3d::ePIPE_GRAPHICS;
    desc.aVertSpv = s_cloudVertSpv.data();
    desc.aVertSize = s_cloudVertSpv.size() * sizeof(uint32_t);
    desc.aFragSpv = s_cloudFragSpv.data();
    desc.aFragSize = s_cloudFragSpv.size() * sizeof(uint32_t);
    desc.aVertexLayout.Clear(); // 无顶点缓冲：gl_VertexIndex 展开
    desc.aVertexLayout.aStride = 0;
    desc.aRenderPass = GN_Vk.renderPass3D;
    desc.aSubpass = 0;
    desc.aLayout = s_cloudPipelineLayout;
    desc.aDepthTest = true;
    desc.aDepthWrite = false; // 半透明云层不写深度
    desc.aDepthCompare = VK_COMPARE_OP_LESS;
    desc.aDepthStencil = true;
    desc.aCullBack = false;
    desc.aFrontFace = VK_FRONT_FACE_CLOCKWISE; // 世界 CCW + Y 翻转
    desc.aBlend = GN3d::eBLEND_ALPHA; // srcAlpha / oneMinusSrcAlpha
    desc.aTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    desc.aSamples = GN_Vk.msaaSamples;
    desc.aColorAttachmentCount = 1;
    desc.aDynamicViewport = false;
    desc.aDynamicScissor = false;
    desc.aViewport = VkViewport{0.0f, 0.0f, (float)GN_Vk.swapchainExtent.width,
                                (float)GN_Vk.swapchainExtent.height, 0.0f, 1.0f};
    desc.aScissor = VkRect2D{{0, 0}, GN_Vk.swapchainExtent};
    desc.aCustomBlend = false;
    return desc;
}

// 创建 set 布局 / 管线布局 / UBO / 描述符集 / 管线；失败时调用方负责 Destroy 回滚。
bool createCloudResources() {
    if(GN_Vk.device == VK_NULL_HANDLE || GN_Vk.renderPass3D == VK_NULL_HANDLE ||
       GN_Vk.descriptorPool == VK_NULL_HANDLE) {
        return false;
    }

    VkDescriptorSetLayoutBinding binding = {};
    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    VkDescriptorSetLayoutCreateInfo layoutInfo = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &binding;
    if(vkCreateDescriptorSetLayout(GN_Vk.device, &layoutInfo, nullptr, &s_cloudSetLayout) !=
       VK_SUCCESS) {
        s_cloudSetLayout = VK_NULL_HANDLE;
        return false;
    }

    VkPipelineLayoutCreateInfo pipelineInfo = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    pipelineInfo.setLayoutCount = 1;
    pipelineInfo.pSetLayouts = &s_cloudSetLayout;
    if(vkCreatePipelineLayout(GN_Vk.device, &pipelineInfo, nullptr, &s_cloudPipelineLayout) !=
       VK_SUCCESS) {
        s_cloudPipelineLayout = VK_NULL_HANDLE;
        return false;
    }

    CreateBuf(sizeof(sCLOUD_UBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
              s_cloudUboBuffer, s_cloudUboMemory);
    if(s_cloudUboBuffer == VK_NULL_HANDLE || s_cloudUboMemory == VK_NULL_HANDLE) return false;
    if(vkMapMemory(GN_Vk.device, s_cloudUboMemory, 0, VK_WHOLE_SIZE, 0, &s_cloudUboMapped) !=
           VK_SUCCESS ||
       s_cloudUboMapped == nullptr) {
        s_cloudUboMapped = 0;
        return false;
    }
    memset(s_cloudUboMapped, 0, sizeof(sCLOUD_UBO));

    s_cloudVertSpv = LoadSPIRVFile("assets/shaders/gn3d_cloud.vert.spv");
    s_cloudFragSpv = LoadSPIRVFile("assets/shaders/gn3d_cloud.frag.spv");
    if(s_cloudVertSpv.empty() || s_cloudFragSpv.empty()) {
        fprintf(stderr, "[GN_Vk] 云层着色器加载失败: gn3d_cloud.vert/frag.spv\n");
        return false;
    }

    VkDescriptorSetAllocateInfo allocInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    allocInfo.descriptorPool = GN_Vk.descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &s_cloudSetLayout;
    if(vkAllocateDescriptorSets(GN_Vk.device, &allocInfo, &s_cloudSet) != VK_SUCCESS) {
        s_cloudSet = VK_NULL_HANDLE;
        return false;
    }
    VkDescriptorBufferInfo bufferInfo = {};
    bufferInfo.buffer = s_cloudUboBuffer;
    bufferInfo.offset = 0;
    bufferInfo.range = (VkDeviceSize)sizeof(sCLOUD_UBO);
    VkWriteDescriptorSet write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    write.dstSet = s_cloudSet;
    write.dstBinding = 0;
    write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    write.descriptorCount = 1;
    write.pBufferInfo = &bufferInfo;
    vkUpdateDescriptorSets(GN_Vk.device, 1, &write, 0, nullptr);

    GN3d::sPIPELINE_DESC desc = makeCloudPipelineDesc();
    s_cloudPipeline = GN_Vk.aPipelineCache.GetOrCreate(desc);
    if(s_cloudPipeline == VK_NULL_HANDLE) {
        fprintf(stderr, "[GN_Vk] 云层管线创建失败: %s\n", GN_Vk.aPipelineCache.GetLastError());
        return false;
    }
    return true;
}

// 相机世界右向的水平投影：M = P·V 的第 0 行 = P00 · 相机右向（P 在 x 上只有对角缩放），
// 列主序下该行就是 (m[0], m[4], m[8], m[12])，因此无需矩阵求逆。
// 退化（非有限 / 近零水平分量）时回退到 (1,0,0)，保证 shader 里的基仍是单位正交。
void computeCameraRight(const sMAT44 &viewProj, float *outRight) {
    outRight[0] = 1.0f;
    outRight[1] = 0.0f;
    outRight[2] = 0.0f;
    const float rx = viewProj.m[0];
    const float rz = viewProj.m[8];
    const float horizontal = sqrtf(rx * rx + rz * rz);
    if(!std::isfinite(horizontal) || horizontal < 1.0e-4f) return;
    outRight[0] = rx / horizontal;
    outRight[2] = rz / horizontal;
}

void fillCloudUniforms(const sCLOUD_DRAW_PARAMS &params) {
    sCLOUD_UBO ubo;
    memset(&ubo, 0, sizeof(ubo));
    memcpy(ubo.aViewProj, params.aViewProj.m, sizeof(ubo.aViewProj));
    ubo.aCameraPos[0] = params.aCameraPos.x;
    ubo.aCameraPos[1] = params.aCameraPos.y;
    ubo.aCameraPos[2] = params.aCameraPos.z;
    ubo.aCameraPos[3] = params.aNearFade;
    ubo.aWindTime[0] = params.aWind.x;
    ubo.aWindTime[1] = params.aWind.z;
    ubo.aWindTime[2] = params.aTimeSeconds;
    ubo.aParams0[0] = params.aTileSize;
    ubo.aParams0[1] = (float)params.aTilesPerAxis;
    ubo.aParams0[2] = params.aCoverage;
    ubo.aParams0[3] = params.aSharpness;
    ubo.aParams1[0] = params.aOpacity;
    ubo.aParams1[1] = params.aLayerHeight;
    ubo.aParams1[2] = (float)kCloudLayers;
    ubo.aParams1[3] = kCloudLayerSpacing;
    computeCameraRight(params.aViewProj, ubo.aCameraRight);
    ubo.aCameraUp[1] = 1.0f; // 云层法线 = 世界 +Y
    if(s_cloudUboMapped) memcpy(s_cloudUboMapped, &ubo, sizeof(ubo));
}

} // namespace

bool GN_VkDrawCloud(const sCLOUD_DRAW_PARAMS &params) {
    if(!GN_Vk.initialized || !s_in3DPass) return false;
    if(params.aTilesPerAxis == 0 || params.aTilesPerAxis > 64 || !(params.aTileSize > 0.0f)) {
        return false;
    }

    if(s_cloudPipeline == VK_NULL_HANDLE) {
        if(!createCloudResources()) {
            GN_VkCloudDestroy();
            return false;
        }
    }
    fillCloudUniforms(params);

    const uint32_t tiles = params.aTilesPerAxis;
    VkCommandBuffer commandBuffer = GN_Vk.commandBuffers[GN_Vk.currentFrame];
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, s_cloudPipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, s_cloudPipelineLayout,
                            0, 1, &s_cloudSet, 0, nullptr);
    vkCmdDraw(commandBuffer, tiles * tiles * kCloudLayers * 6u, 1, 0, 0);
    return true;
}

void GN_VkCloudDestroy() {
    if(GN_Vk.device) {
        if(s_cloudSet != VK_NULL_HANDLE && GN_Vk.descriptorPool) {
            vkFreeDescriptorSets(GN_Vk.device, GN_Vk.descriptorPool, 1, &s_cloudSet);
        }
        if(s_cloudUboBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(GN_Vk.device, s_cloudUboBuffer, nullptr);
        }
        if(s_cloudUboMemory != VK_NULL_HANDLE) {
            vkFreeMemory(GN_Vk.device, s_cloudUboMemory, nullptr);
        }
        if(s_cloudPipelineLayout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(GN_Vk.device, s_cloudPipelineLayout, nullptr);
        }
        if(s_cloudSetLayout != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(GN_Vk.device, s_cloudSetLayout, nullptr);
        }
    }
    s_cloudSet = VK_NULL_HANDLE;
    s_cloudUboBuffer = VK_NULL_HANDLE;
    s_cloudUboMemory = VK_NULL_HANDLE;
    s_cloudUboMapped = 0;
    s_cloudPipelineLayout = VK_NULL_HANDLE;
    s_cloudSetLayout = VK_NULL_HANDLE;
    s_cloudPipeline = VK_NULL_HANDLE; // 归 aPipelineCache
    s_cloudVertSpv.clear();
    s_cloudFragSpv.clear();
}

} // namespace GNg
