// vkCloudCube.cpp — L5b 云块：实例化**包围盒**（36 顶点/实例，无顶点缓冲），盒内体积云。
//
// 契约（GN_Vk.h sCLOUD_CUBE_PARAMS / sCLOUD_CUBE_INSTANCE）：
//   * GN_VkDrawCloudCubes：一次 draw（36 顶点 × N 实例），只在 3D pass 内提交；
//     N > 128 或实例指针为空返回 false（上限由 UBO 的 aInstances[128] 决定）。
//   * GN_VkCloudCubeDestroy：释放 UBO / 描述符集 / 管线布局；可重复调用（管线归 aPipelineCache）。
//
// 实现要点：
//   * 方块 = 云的**包围盒**，不是云的形状：顶点着色器反转绕序，CULL_BACK 后只画盒子背面，
//     片元用射线-盒求交在盒内 raymarch 噪声密度（软边由盒内边界渐变保证）。
//   * 着色器按 gl_VertexIndex 展开 6 面 × 2 三角形，按 gl_InstanceIndex 取方块参数；
//     UBO 4240 B host-visible，与 gn3d_cloud_cube.vert/frag 的 CubeUBO 逐字段一致。
//   * 动态 viewport/scissor ⇒ 不需要在 resize 链里重建（resize 后按新 extent 直接绘制）。
//   * 渲染状态：预乘 alpha 混合、深度测试开 / 深度写关、背面剔除（= 只画盒子远侧表面）。
//   * 管线/布局/描述符集在首次 Draw 时惰性创建；失败回滚。
#include <GN.h>
#include "GN_Vk.h"
#include "vkInternal.h"

#include <cstdio>
#include <cstring>
#include <vector>

namespace GNg {

namespace {

// 与着色器 aInstances[128] / aInstanceTint[32] 对齐。
const uint32_t kCloudCubeMaxInstances = 128;
// 方块 = 6 面 × 2 三角形 × 3 顶点。
const uint32_t kCloudCubeVertexCount = 36;

struct alignas(16) sCLOUD_CUBE_UBO {
    float aViewProj[16];      // 64 B
    float aCamera[4];         // xyz = 相机世界位置, w = 时间(秒)
    float aSun[4];            // xyz = 指向太阳, w = 强度
    float aAmbient[4];        // rgb = 天空环境色
    float aFog[4];            // rgb = 雾色, w = 密度(1/m)
    float aCount[4];          // x = 实例数
    float aInstances[128 * 4]; // xyz = 中心, w = 半边长
    float aTints[128 * 4];     // rgb = 色调, w = 密度倍率
};
static_assert(sizeof(sCLOUD_CUBE_UBO) == 4240, "sCLOUD_CUBE_UBO 必须为 1168 字节");

VkDescriptorSetLayout s_cubeSetLayout = VK_NULL_HANDLE;
VkPipelineLayout s_cubePipelineLayout = VK_NULL_HANDLE;
VkPipeline s_cubePipeline = VK_NULL_HANDLE;
VkBuffer s_cubeUboBuffer = VK_NULL_HANDLE;
VkDeviceMemory s_cubeUboMemory = VK_NULL_HANDLE;
void *s_cubeUboMapped = 0;
VkDescriptorSet s_cubeSet = VK_NULL_HANDLE;
std::vector<uint32_t> s_cubeVertSpv;
std::vector<uint32_t> s_cubeFragSpv;

GN3d::sPIPELINE_DESC makeCloudCubePipelineDesc() {
    GN3d::sPIPELINE_DESC desc;
    desc.aKind = GN3d::ePIPE_GRAPHICS;
    desc.aVertSpv = s_cubeVertSpv.data();
    desc.aVertSize = s_cubeVertSpv.size() * sizeof(uint32_t);
    desc.aFragSpv = s_cubeFragSpv.data();
    desc.aFragSize = s_cubeFragSpv.size() * sizeof(uint32_t);
    desc.aVertexLayout.Clear(); // 无顶点缓冲：gl_VertexIndex / gl_InstanceIndex 展开
    desc.aVertexLayout.aStride = 0;
    desc.aRenderPass = GN_Vk.renderPass3D;
    desc.aSubpass = 0;
    desc.aLayout = s_cubePipelineLayout;
    desc.aDepthTest = true;
    desc.aDepthWrite = false; // 半透明云不写深度：地面/模型随后绘制会正常覆盖
    desc.aDepthCompare = VK_COMPARE_OP_LESS_OR_EQUAL;
    desc.aDepthStencil = true;
    // 方块只是云的包围盒：顶点着色器反转绕序，CULL_BACK 后只保留盒子**背面**（远侧表面），
    // 片元据此拿到射线出盒点，再在盒内 raymarch 密度（云的外形由噪声决定，不是盒子）。
    desc.aCullBack = true;
    desc.aFrontFace = VK_FRONT_FACE_CLOCKWISE; // 世界 CCW + Y 翻转
    desc.aBlend = GN3d::eBLEND_NONE;
    desc.aTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    desc.aSamples = GN_Vk.msaaSamples;
    desc.aColorAttachmentCount = 1;
    desc.aDynamicViewport = true;
    desc.aDynamicScissor = true;
    // 预乘 alpha：dst' = C + dst × (1 - a)，a = 1 - 透射率（与片元输出一致）。
    desc.aCustomBlend = true;
    desc.aBlendSrcColor = VK_BLEND_FACTOR_ONE;
    desc.aBlendDstColor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    desc.aBlendSrcAlpha = VK_BLEND_FACTOR_ONE;
    desc.aBlendDstAlpha = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    return desc;
}

void fillCloudCubeUniforms(const sCLOUD_CUBE_PARAMS &params) {
    sCLOUD_CUBE_UBO ubo;
    std::memset(&ubo, 0, sizeof(ubo));
    std::memcpy(ubo.aViewProj, params.aViewProj.m, sizeof(ubo.aViewProj));
    ubo.aCamera[0] = params.aCameraPos.x;
    ubo.aCamera[1] = params.aCameraPos.y;
    ubo.aCamera[2] = params.aCameraPos.z;
    ubo.aSun[0] = params.aSunDir.x;
    ubo.aSun[1] = params.aSunDir.y;
    ubo.aSun[2] = params.aSunDir.z;
    ubo.aSun[3] = params.aSunIntensity;
    ubo.aAmbient[0] = params.aAmbient.x;
    ubo.aAmbient[1] = params.aAmbient.y;
    ubo.aAmbient[2] = params.aAmbient.z;
    ubo.aFog[0] = params.aFogColor.x;
    ubo.aFog[1] = params.aFogColor.y;
    ubo.aFog[2] = params.aFogColor.z;
    ubo.aFog[3] = params.aFogDensity;
    ubo.aCount[0] = (float)params.aCount;
    ubo.aCount[1] = params.aBaseAltitude; // 全场景统一云底：密度在它以下归零（平底）
    ubo.aCount[2] = params.aOvercast;     // 阴雨程度：压暗云体
    for(uint32_t i = 0; i < params.aCount; ++i) {
        const sCLOUD_CUBE_INSTANCE &instance = params.aInstances[i];
        ubo.aInstances[i * 4 + 0] = instance.aCenter[0];
        ubo.aInstances[i * 4 + 1] = instance.aCenter[1];
        ubo.aInstances[i * 4 + 2] = instance.aCenter[2];
        ubo.aInstances[i * 4 + 3] = instance.aHalfSize;
        ubo.aTints[i * 4 + 0] = instance.aTint[0];
        ubo.aTints[i * 4 + 1] = instance.aTint[1];
        ubo.aTints[i * 4 + 2] = instance.aTint[2];
        ubo.aTints[i * 4 + 3] = instance.aAlpha;
    }
    if(s_cubeUboMapped) std::memcpy(s_cubeUboMapped, &ubo, sizeof(ubo));
}

bool createCloudCubeResources() {
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
    if(vkCreateDescriptorSetLayout(GN_Vk.device, &layoutInfo, nullptr, &s_cubeSetLayout) !=
       VK_SUCCESS) {
        s_cubeSetLayout = VK_NULL_HANDLE;
        return false;
    }
    VkPipelineLayoutCreateInfo pipelineInfo = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    pipelineInfo.setLayoutCount = 1;
    pipelineInfo.pSetLayouts = &s_cubeSetLayout;
    if(vkCreatePipelineLayout(GN_Vk.device, &pipelineInfo, nullptr, &s_cubePipelineLayout) !=
       VK_SUCCESS) {
        s_cubePipelineLayout = VK_NULL_HANDLE;
        return false;
    }
    CreateBuf(sizeof(sCLOUD_CUBE_UBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
              s_cubeUboBuffer, s_cubeUboMemory);
    if(s_cubeUboBuffer == VK_NULL_HANDLE || s_cubeUboMemory == VK_NULL_HANDLE) return false;
    if(vkMapMemory(GN_Vk.device, s_cubeUboMemory, 0, VK_WHOLE_SIZE, 0, &s_cubeUboMapped) !=
           VK_SUCCESS ||
       s_cubeUboMapped == nullptr) {
        s_cubeUboMapped = 0;
        return false;
    }
    std::memset(s_cubeUboMapped, 0, sizeof(sCLOUD_CUBE_UBO));

    s_cubeVertSpv = LoadSPIRVFile("assets/shaders/gn3d_cloud_cube.vert.spv");
    s_cubeFragSpv = LoadSPIRVFile("assets/shaders/gn3d_cloud_cube.frag.spv");
    if(s_cubeVertSpv.empty() || s_cubeFragSpv.empty()) {
        fprintf(stderr, "[GN_Vk] 立方体云着色器加载失败: gn3d_cloud_cube.vert/frag.spv\n");
        return false;
    }
    VkDescriptorSetAllocateInfo allocInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    allocInfo.descriptorPool = GN_Vk.descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &s_cubeSetLayout;
    if(vkAllocateDescriptorSets(GN_Vk.device, &allocInfo, &s_cubeSet) != VK_SUCCESS) {
        s_cubeSet = VK_NULL_HANDLE;
        return false;
    }
    VkDescriptorBufferInfo bufferInfo = {};
    bufferInfo.buffer = s_cubeUboBuffer;
    bufferInfo.offset = 0;
    bufferInfo.range = (VkDeviceSize)sizeof(sCLOUD_CUBE_UBO);
    VkWriteDescriptorSet write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    write.dstSet = s_cubeSet;
    write.dstBinding = 0;
    write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    write.descriptorCount = 1;
    write.pBufferInfo = &bufferInfo;
    vkUpdateDescriptorSets(GN_Vk.device, 1, &write, 0, nullptr);

    GN3d::sPIPELINE_DESC desc = makeCloudCubePipelineDesc();
    s_cubePipeline = GN_Vk.aPipelineCache.GetOrCreate(desc);
    if(s_cubePipeline == VK_NULL_HANDLE) {
        fprintf(stderr, "[GN_Vk] 立方体云管线创建失败: %s\n",
                GN_Vk.aPipelineCache.GetLastError());
        return false;
    }
    return true;
}

} // namespace

bool GN_VkDrawCloudCubes(const sCLOUD_CUBE_PARAMS &params) {
    if(!GN_Vk.initialized || !s_in3DPass) return false;
    if(params.aInstances == nullptr || params.aCount == 0 ||
       params.aCount > kCloudCubeMaxInstances) {
        return false;
    }
    if(s_cubePipeline == VK_NULL_HANDLE) {
        if(!createCloudCubeResources()) {
            GN_VkCloudCubeDestroy();
            return false;
        }
    }
    fillCloudCubeUniforms(params);

    VkCommandBuffer cb = GN_Vk.commandBuffers[GN_Vk.currentFrame];
    VkViewport viewport = {0.0f, 0.0f, (float)GN_Vk.swapchainExtent.width,
                           (float)GN_Vk.swapchainExtent.height, 0.0f, 1.0f};
    VkRect2D scissor = {{0, 0}, GN_Vk.swapchainExtent};
    vkCmdSetViewport(cb, 0, 1, &viewport);
    vkCmdSetScissor(cb, 0, 1, &scissor);
    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, s_cubePipeline);
    vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, s_cubePipelineLayout, 0, 1,
                            &s_cubeSet, 0, nullptr);
    vkCmdDraw(cb, kCloudCubeVertexCount, params.aCount, 0, 0);
    return true;
}

void GN_VkCloudCubeDestroy() {
    if(GN_Vk.device) {
        if(s_cubeSet != VK_NULL_HANDLE && GN_Vk.descriptorPool) {
            vkFreeDescriptorSets(GN_Vk.device, GN_Vk.descriptorPool, 1, &s_cubeSet);
        }
        if(s_cubeUboBuffer != VK_NULL_HANDLE) vkDestroyBuffer(GN_Vk.device, s_cubeUboBuffer, nullptr);
        if(s_cubeUboMemory != VK_NULL_HANDLE) vkFreeMemory(GN_Vk.device, s_cubeUboMemory, nullptr);
        if(s_cubePipelineLayout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(GN_Vk.device, s_cubePipelineLayout, nullptr);
        }
        if(s_cubeSetLayout != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(GN_Vk.device, s_cubeSetLayout, nullptr);
        }
    }
    s_cubeSet = VK_NULL_HANDLE;
    s_cubeUboBuffer = VK_NULL_HANDLE;
    s_cubeUboMemory = VK_NULL_HANDLE;
    s_cubeUboMapped = 0;
    s_cubePipelineLayout = VK_NULL_HANDLE;
    s_cubeSetLayout = VK_NULL_HANDLE;
    s_cubePipeline = VK_NULL_HANDLE; // 归 aPipelineCache
    s_cubeVertSpv.clear();
    s_cubeFragSpv.clear();
}

} // namespace GNg
