// tests/cornellBox/cornellGpuRt.cpp — GPU compute 光追预览的 Vulkan 实现
//
// 职责：建 compute 管线（gn_cornell_rt.comp.spv）、场景 SSBO、参数 UBO 与
// 主机可见的 RGBA8 输出缓冲；每帧提交一次 dispatch + fence 等待，然后回读像素。
// 与 cornellCpuRt.cpp 共享同一份 buildCornellGeometry(false) 几何与同一套光照公式，
// 因此 8 spp 时两者应逐像素接近（自检探针用 MAE 判定）。
//
// 依赖边界：只用 Vulkan 1.0 核心能力（storage buffer + compute），不依赖引擎；
// 调用方负责提供 sGpuRtContext。读回的像素布局为小端 R,G,B,A 字节，可直接喂 sIMAGE::DataFill。
#include "cornellGpuRt.h"

#include "cornellGeometry.h"
#include "cornellRtAs.h"
#include "cornellVkUtil.h"
#include "GN_3DprobeRelocation.h"
#include "GN_3DprobeUpdate.h"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace Cornell {

namespace {

// 与 assets/shaders/gn_cornell_rt.comp 的 RtParams（std140）逐字段一致。
struct sRtParams {
    float aEye[4];
    float aFwd[4];
    float aRight[4];
    float aUp[4];
    float aCam[4];   // x=tanHalfFov, y=aspect, z=lightMode, w=sampleCount
    float aSize[4];  // x=width, y=height
    float aSunDir[4];
    float aLightPos[4];
    unsigned int aCounts[4]; // x=quadCount, y=sphereCount, z=materialCount
    float aAccum[4];         // x=frameStartSample, y=reset(保留), z/w 保留
    float aPrevEye[4];       // 上一帧相机基（用于重投影）
    float aPrevFwd[4];
    float aPrevRight[4];
    float aPrevUp[4];
    float aTemporal[4];      // x=historyValid, y=moving, z=targetSamples, w=reserved
};

using sBuffer = sVkBuffer;

struct sState {
    bool aReady;
    sGpuRtContext aCtx;
    int aWidth;
    int aHeight;
    VkShaderModule aShader;
    VkDescriptorSetLayout aSetLayout;
    VkPipelineLayout aPipelineLayout;
    VkPipeline aPipeline;
    VkCommandPool aCommandPool;
    VkCommandBuffer aCommandBuffer;
    VkFence aFence;
    bool aPending; // 已提交、尚未等待回读的帧
    VkDescriptorPool aDescriptorPool;
    VkDescriptorSet aDescriptorSets[2];
    sBuffer aOutBuffer;
    sBuffer aSceneBuffer;
    sBuffer aParamsBuffer;
    sBuffer aHistoryColor[2];
    sBuffer aHistoryDepth[2];
    int aHistoryIndex;
    bool aHistoryValid;
    sBuffer aProbeShBuffer;
    sBuffer aProbeDepthBuffer;
    sBuffer aProbeOffsetBuffer;
    sBuffer aProbeStateBuffer;
    sBuffer aProbeUboBuffer;
    bool aProbeReady;
    // B3c-2：异步探针更新（compute dispatch + 原地混合 + SSBO 状态/年龄）。
    VkShaderModule aUpdateShader;
    VkPipeline aUpdatePipeline;
    VkShaderModule aRelocShader;
    VkPipeline aRelocPipeline;
    bool aProbeRelocGpuEnabled;
    bool aProbeAsyncEnabled;
    float aProbeHysteresis;
    unsigned int aProbeBudget;   // 0 = 每帧全量（校验用）
    unsigned int aProbeSamples;  // 每 texel 的 radiance 样本数
    unsigned int aProbeFrameIndex;
    unsigned int aProbeCount;
    GN3d::sPROBE_UPDATE_SCHEDULER aProbeScheduler;
    unsigned int aAccumTotal;
    unsigned int aTargetSamples;
    sGpuRtStats aStats;
    float aPrevEye[3];
    float aPrevFwd[3];
    float aPrevRight[3];
    float aPrevUp[3];
    float aPrevFov;
    int aPrevLight;
    bool aPrevValid;
    std::vector<unsigned char> aLastPixels;
    unsigned int aQuadCount;
    unsigned int aSphereCount;
    unsigned int aMaterialCount;
    eGpuRtBackend aBackend;
    sRtAsScene aAsScene;
};

sState s_state = {};

const char *kAnalyticShaderPath = "assets/shaders/gn_cornell_rt.comp.spv";
const char *kProbeUpdateShaderPath = "assets/shaders/gn_cornell_probe_update.comp.spv";
const char *kProbeRelocationShaderPath =
    "assets/shaders/gn_cornell_probe_relocation.comp.spv";
const char *kRayQueryShaderPath = "assets/shaders/gn_cornell_rq.comp.spv";
// 累积目标：静止时达到后停止 dispatch、回放缓存；CORNELL_RT_TARGET 可覆盖。
// 1024 样本 @ 16 spp/帧 = 64 帧，约 1–3 秒收敛，足够交互观察。
const unsigned int kDefaultTargetSamples = 1024u;

bool loadSpv(const char *path, std::vector<unsigned int> *out) {
    if (!out) return false;
    FILE *file = std::fopen(path, "rb");
    if (!file) {
        std::fprintf(stderr, "[cornellGpuRt] 无法打开着色器 %s（是否已 make shaders？）\n", path);
        return false;
    }
    std::fseek(file, 0, SEEK_END);
    const long size = std::ftell(file);
    std::fseek(file, 0, SEEK_SET);
    if (size <= 0 || size % 4 != 0) {
        std::fprintf(stderr, "[cornellGpuRt] 着色器大小异常: %s (%ld)\n", path, size);
        std::fclose(file);
        return false;
    }
    out->resize(static_cast<size_t>(size) / 4u);
    const size_t read = std::fread(out->data(), 1, static_cast<size_t>(size), file);
    std::fclose(file);
    if (read != static_cast<size_t>(size)) {
        std::fprintf(stderr, "[cornellGpuRt] 着色器读取不完整: %s\n", path);
        return false;
    }
    return true;
}

std::vector<float> packScene(const sCornellGeometry &geom) {
    std::vector<float> data;
    data.reserve(geom.aQuads.size() * 16u + geom.aSpheres.size() * 8u +
                 geom.aMaterials.size() * 8u);
    for (size_t i = 0; i < geom.aQuads.size(); ++i) {
        const sQuad &q = geom.aQuads[i];
        const float quad[16] = {q.aP0.x, q.aP0.y, q.aP0.z, static_cast<float>(q.aMaterial),
                                q.aU.x,  q.aU.y,  q.aU.z,  0.0f,
                                q.aV.x,  q.aV.y,  q.aV.z,  0.0f,
                                q.aNormal.x, q.aNormal.y, q.aNormal.z, 0.0f};
        data.insert(data.end(), quad, quad + 16);
    }
    for (size_t i = 0; i < geom.aSpheres.size(); ++i) {
        const sSphere &s = geom.aSpheres[i];
        const float sphere[8] = {s.aCenter.x, s.aCenter.y, s.aCenter.z, s.aRadius,
                                 static_cast<float>(s.aMaterial), 0.0f, 0.0f, 0.0f};
        data.insert(data.end(), sphere, sphere + 8);
    }
    for (size_t i = 0; i < geom.aMaterials.size(); ++i) {
        const sMaterial &m = geom.aMaterials[i];
        const float material[8] = {m.aAlbedo[0], m.aAlbedo[1], m.aAlbedo[2], m.aMetallic,
                                   m.aEmission[0], m.aEmission[1], m.aEmission[2], 0.0f};
        data.insert(data.end(), material, material + 8);
    }
    return data;
}

bool createPipeline() {
    const bool rayQuery = s_state.aBackend == eGPU_RT_RAY_QUERY;
    std::vector<unsigned int> spv;
    if (!loadSpv(rayQuery ? kRayQueryShaderPath : kAnalyticShaderPath, &spv)) return false;
    VkShaderModuleCreateInfo smci = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    smci.codeSize = spv.size() * sizeof(unsigned int);
    smci.pCode = spv.data();
    if (vkCreateShaderModule(s_state.aCtx.aDevice, &smci, nullptr, &s_state.aShader) !=
        VK_SUCCESS) {
        return false;
    }

    VkDescriptorSetLayoutBinding bindings[16] = {};
    for (int i = 0; i < 16; ++i) {
        bindings[i].binding = static_cast<uint32_t>(i);
        bindings[i].descriptorCount = 1;
        bindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    }
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[4].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    bindings[5].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[6].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[7].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[8].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[9].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[10].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[11].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[12].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[13].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[14].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[15].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; // ProbeState（B3c-2）
    VkDescriptorSetLayoutCreateInfo dslci = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    dslci.bindingCount = 16u;
    dslci.pBindings = bindings;
    if (vkCreateDescriptorSetLayout(s_state.aCtx.aDevice, &dslci, nullptr, &s_state.aSetLayout) !=
        VK_SUCCESS) {
        return false;
    }

    VkPipelineLayoutCreateInfo plci = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    plci.setLayoutCount = 1;
    plci.pSetLayouts = &s_state.aSetLayout;
    if (vkCreatePipelineLayout(s_state.aCtx.aDevice, &plci, nullptr, &s_state.aPipelineLayout) !=
        VK_SUCCESS) {
        return false;
    }

    VkComputePipelineCreateInfo cpci = {VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    cpci.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    cpci.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    cpci.stage.module = s_state.aShader;
    cpci.stage.pName = "main";
    cpci.layout = s_state.aPipelineLayout;
    if (vkCreateComputePipelines(s_state.aCtx.aDevice, VK_NULL_HANDLE, 1, &cpci, nullptr,
                                 &s_state.aPipeline) != VK_SUCCESS) {
        return false;
    }
    return true;
}

// 懒创建探针更新管线（与渲染共用 descriptor set layout / pipeline layout）；
// 默认路径不加载，CORNELL_PROBE_ASYNC=1 时才付出这份开销。
bool createProbeUpdatePipeline() {
    if (s_state.aUpdatePipeline != VK_NULL_HANDLE) return true;
    std::vector<unsigned int> spv;
    if (!loadSpv(kProbeUpdateShaderPath, &spv)) return false;
    VkShaderModuleCreateInfo smci = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    smci.codeSize = spv.size() * sizeof(unsigned int);
    smci.pCode = spv.data();
    if (vkCreateShaderModule(s_state.aCtx.aDevice, &smci, nullptr, &s_state.aUpdateShader) !=
        VK_SUCCESS) {
        return false;
    }
    VkComputePipelineCreateInfo cpci = {VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    cpci.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    cpci.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    cpci.stage.module = s_state.aUpdateShader;
    cpci.stage.pName = "main";
    cpci.layout = s_state.aPipelineLayout;
    return vkCreateComputePipelines(s_state.aCtx.aDevice, VK_NULL_HANDLE, 1, &cpci, nullptr,
                                    &s_state.aUpdatePipeline) == VK_SUCCESS;
}

// 懒创建 6 轴 relocation 管线（与更新管线共用同一个 descriptor set layout）。
bool createProbeRelocationPipeline() {
    if (s_state.aRelocPipeline != VK_NULL_HANDLE) return true;
    std::vector<unsigned int> spv;
    if (!loadSpv(kProbeRelocationShaderPath, &spv)) return false;
    VkShaderModuleCreateInfo smci = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    smci.codeSize = spv.size() * sizeof(unsigned int);
    smci.pCode = spv.data();
    if (vkCreateShaderModule(s_state.aCtx.aDevice, &smci, nullptr, &s_state.aRelocShader) !=
        VK_SUCCESS) {
        return false;
    }
    VkComputePipelineCreateInfo cpci = {VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    cpci.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    cpci.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    cpci.stage.module = s_state.aRelocShader;
    cpci.stage.pName = "main";
    cpci.layout = s_state.aPipelineLayout;
    return vkCreateComputePipelines(s_state.aCtx.aDevice, VK_NULL_HANDLE, 1, &cpci, nullptr,
                                    &s_state.aRelocPipeline) == VK_SUCCESS;
}

bool createCommandAndFence() {
    VkCommandPoolCreateInfo pci = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    pci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pci.queueFamilyIndex = s_state.aCtx.aQueueFamily;
    if (vkCreateCommandPool(s_state.aCtx.aDevice, &pci, nullptr, &s_state.aCommandPool) !=
        VK_SUCCESS) {
        return false;
    }
    VkCommandBufferAllocateInfo cai = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    cai.commandPool = s_state.aCommandPool;
    cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cai.commandBufferCount = 1;
    if (vkAllocateCommandBuffers(s_state.aCtx.aDevice, &cai, &s_state.aCommandBuffer) !=
        VK_SUCCESS) {
        return false;
    }
    VkFenceCreateInfo fci = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    return vkCreateFence(s_state.aCtx.aDevice, &fci, nullptr, &s_state.aFence) == VK_SUCCESS;
}

bool createDescriptorsAndBuffers(int width, int height) {
    const bool rayQuery = s_state.aBackend == eGPU_RT_RAY_QUERY;
    sCornellGeometry geom;
    buildCornellGeometry(false, &geom);
    s_state.aQuadCount = static_cast<unsigned int>(geom.aQuads.size());
    s_state.aSphereCount = static_cast<unsigned int>(geom.aSpheres.size());
    s_state.aMaterialCount = static_cast<unsigned int>(geom.aMaterials.size());
    if (rayQuery &&
        !buildRayQueryScene(s_state.aCtx.aDevice, s_state.aCtx.aPhysicalDevice,
                            s_state.aCtx.aQueue, s_state.aCtx.aQueueFamily, geom,
                            &s_state.aAsScene)) {
        std::fprintf(stderr, "[cornellGpuRt] 加速结构构建失败\n");
        return false;
    }

    VkDescriptorPoolSize sizes[3] = {};
    sizes[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    sizes[0].descriptorCount = 32u; // 2 sets × (0,1,3,5,6,7 + 8,9,10 + 12..15)
    sizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    sizes[1].descriptorCount = 4u; // params + probe UBO per set
    sizes[2].type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    // 即使解析路径不使用 binding 4，layout 里仍声明了 AS；池必须留出对应 descriptor，
    // 否则部分驱动（真机 RADV）会在 vkAllocateDescriptorSets 直接失败。
    sizes[2].descriptorCount = 2u;
    VkDescriptorPoolCreateInfo dpci = {VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    dpci.maxSets = 2;
    dpci.poolSizeCount = 3u;
    dpci.pPoolSizes = sizes;
    if (vkCreateDescriptorPool(s_state.aCtx.aDevice, &dpci, nullptr, &s_state.aDescriptorPool) !=
        VK_SUCCESS) {
        return false;
    }
    VkDescriptorSetLayout layouts[2] = {s_state.aSetLayout, s_state.aSetLayout};
    VkDescriptorSetAllocateInfo dsai = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    dsai.descriptorPool = s_state.aDescriptorPool;
    dsai.descriptorSetCount = 2;
    dsai.pSetLayouts = layouts;
    if (vkAllocateDescriptorSets(s_state.aCtx.aDevice, &dsai, s_state.aDescriptorSets) !=
        VK_SUCCESS) {
        return false;
    }

    const VkDeviceSize outBytes = static_cast<VkDeviceSize>(width) * height * 4u;
    if (!createVkBuffer(s_state.aCtx.aDevice, s_state.aCtx.aPhysicalDevice, outBytes,
                        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                        true, &s_state.aOutBuffer, true)) {
        std::fprintf(stderr, "[cornellGpuRt] out buffer 分配失败\n");
        return false;
    }
    const std::vector<float> scene = packScene(geom);
    const VkDeviceSize sceneBytes = static_cast<VkDeviceSize>(scene.size()) * sizeof(float);
    if (!createVkBuffer(s_state.aCtx.aDevice, s_state.aCtx.aPhysicalDevice, sceneBytes,
                        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                        true, &s_state.aSceneBuffer)) {
        std::fprintf(stderr, "[cornellGpuRt] scene buffer 分配失败\n");
        return false;
    }
    std::memcpy(s_state.aSceneBuffer.aMapped, scene.data(), static_cast<size_t>(sceneBytes));
    if (!createVkBuffer(s_state.aCtx.aDevice, s_state.aCtx.aPhysicalDevice, sizeof(sRtParams),
                        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                        true, &s_state.aParamsBuffer)) {
        std::fprintf(stderr, "[cornellGpuRt] params buffer 分配失败\n");
        return false;
    }
    const VkDeviceSize colorBytes = static_cast<VkDeviceSize>(width) * height * 16u;
    const VkDeviceSize depthBytes = static_cast<VkDeviceSize>(width) * height * 4u;
    for (int i = 0; i < 2; ++i) {
        if (!createVkBuffer(s_state.aCtx.aDevice, s_state.aCtx.aPhysicalDevice, colorBytes,
                            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, false,
                            &s_state.aHistoryColor[i]) ||
            !createVkBuffer(s_state.aCtx.aDevice, s_state.aCtx.aPhysicalDevice, depthBytes,
                            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, false,
                            &s_state.aHistoryDepth[i])) {
            std::fprintf(stderr, "[cornellGpuRt] history buffer %d 分配失败\n", i);
            return false;
        }
    }
    if (!createVkBuffer(s_state.aCtx.aDevice, s_state.aCtx.aPhysicalDevice, 96u,
                        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                        true, &s_state.aProbeUboBuffer, true) ||
        !createVkBuffer(s_state.aCtx.aDevice, s_state.aCtx.aPhysicalDevice, 4u,
                        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                        true, &s_state.aProbeShBuffer, true) ||
        !createVkBuffer(s_state.aCtx.aDevice, s_state.aCtx.aPhysicalDevice, 4u,
                        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                        true, &s_state.aProbeDepthBuffer, true)) {
        std::fprintf(stderr, "[cornellGpuRt] probe buffer 分配失败\n");
        return false;
    }
    if (!createVkBuffer(s_state.aCtx.aDevice, s_state.aCtx.aPhysicalDevice, 16u,
                        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                        true, &s_state.aProbeStateBuffer, true)) {
        std::fprintf(stderr, "[cornellGpuRt] probe state buffer 分配失败\n");
        return false;
    }
    std::memset(s_state.aProbeUboBuffer.aMapped, 0, 96u);
    std::memset(s_state.aProbeShBuffer.aMapped, 0, 4u);
    std::memset(s_state.aProbeDepthBuffer.aMapped, 0, 4u);
    std::memset(s_state.aProbeStateBuffer.aMapped, 0, 16u);

    VkDescriptorBufferInfo outInfo = {s_state.aOutBuffer.aBuffer, 0, VK_WHOLE_SIZE};
    VkDescriptorBufferInfo sceneInfo = {s_state.aSceneBuffer.aBuffer, 0, VK_WHOLE_SIZE};
    VkDescriptorBufferInfo paramsInfo = {s_state.aParamsBuffer.aBuffer, 0, sizeof(sRtParams)};
    VkDescriptorBufferInfo colorInfo[2] = {};
    VkDescriptorBufferInfo depthInfo[2] = {};
    for (int i = 0; i < 2; ++i) {
        colorInfo[i] = {s_state.aHistoryColor[i].aBuffer, 0, VK_WHOLE_SIZE};
        depthInfo[i] = {s_state.aHistoryDepth[i].aBuffer, 0, VK_WHOLE_SIZE};
    }
    VkDescriptorBufferInfo positionInfo = {s_state.aAsScene.aVertexBuffer.aBuffer, 0,
                                           VK_WHOLE_SIZE};
    VkDescriptorBufferInfo normalInfo = {s_state.aAsScene.aNormalBuffer.aBuffer, 0,
                                         VK_WHOLE_SIZE};
    VkDescriptorBufferInfo metaInfo = {s_state.aAsScene.aTriMetaBuffer.aBuffer, 0, VK_WHOLE_SIZE};
    VkDescriptorBufferInfo probeUboInfo = {s_state.aProbeUboBuffer.aBuffer, 0, 96u};
    VkDescriptorBufferInfo probeShInfo = {s_state.aProbeShBuffer.aBuffer, 0, VK_WHOLE_SIZE};
    VkDescriptorBufferInfo probeDepthInfo = {s_state.aProbeDepthBuffer.aBuffer, 0,
                                             VK_WHOLE_SIZE};
    VkDescriptorBufferInfo probeStateInfo = {s_state.aProbeStateBuffer.aBuffer, 0, VK_WHOLE_SIZE};
    VkWriteDescriptorSetAccelerationStructureKHR asWrite = {
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR};
    asWrite.accelerationStructureCount = 1;
    asWrite.pAccelerationStructures = &s_state.aAsScene.aTlas;

    VkWriteDescriptorSet writes[28] = {};
    uint32_t writeCount = 0;
    for (uint32_t setIndex = 0; setIndex < 2; ++setIndex) {
        const uint32_t read = setIndex;
        const uint32_t write = (setIndex + 1u) % 2u;
        const VkDescriptorSet dstSet = s_state.aDescriptorSets[setIndex];
        auto addWrite = [&](uint32_t binding, VkDescriptorType type,
                            const VkDescriptorBufferInfo *info, const void *next) {
            VkWriteDescriptorSet &w = writes[writeCount++];
            w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            w.dstSet = dstSet;
            w.dstBinding = binding;
            w.descriptorCount = 1;
            w.descriptorType = type;
            w.pBufferInfo = info;
            w.pNext = const_cast<void *>(next);
        };
        addWrite(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &outInfo, nullptr);
        addWrite(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &sceneInfo, nullptr);
        addWrite(2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &paramsInfo, nullptr);
        addWrite(3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &colorInfo[read], nullptr);
        addWrite(8, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &colorInfo[write], nullptr);
        addWrite(9, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &depthInfo[read], nullptr);
        addWrite(10, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &depthInfo[write], nullptr);
        addWrite(11, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &probeUboInfo, nullptr);
        addWrite(12, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &probeShInfo, nullptr);
        addWrite(13, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &probeDepthInfo, nullptr);
        addWrite(15, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &probeStateInfo, nullptr);
        if (rayQuery) {
            addWrite(4, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, nullptr, &asWrite);
            addWrite(5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &positionInfo, nullptr);
            addWrite(6, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &normalInfo, nullptr);
            addWrite(7, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &metaInfo, nullptr);
        }
    }
    vkUpdateDescriptorSets(s_state.aCtx.aDevice, writeCount, writes, 0, nullptr);
    return true;
}

} // namespace

bool initGpuRt(const sGpuRtContext &ctx, int width, int height, eGpuRtBackend backend) {
    if (s_state.aReady) {
        return s_state.aWidth == width && s_state.aHeight == height && s_state.aBackend == backend;
    }
    if (backend == eGPU_RT_RAY_QUERY && ctx.aQueueFamily == UINT32_MAX) return false;
    if (ctx.aDevice == VK_NULL_HANDLE || ctx.aPhysicalDevice == VK_NULL_HANDLE ||
        ctx.aQueue == VK_NULL_HANDLE || width <= 0 || height <= 0) {
        return false;
    }
    s_state.aCtx = ctx;
    s_state.aWidth = width;
    s_state.aHeight = height;
    s_state.aBackend = backend;
    s_state.aTargetSamples = kDefaultTargetSamples;
    s_state.aProbeBudget = 64u;
    s_state.aProbeSamples = 4u;
    s_state.aProbeHysteresis = 0.9f;
    if (const char *target = std::getenv("CORNELL_RT_TARGET")) {
        const int parsed = std::atoi(target);
        if (parsed > 0) s_state.aTargetSamples = static_cast<unsigned int>(parsed);
    }
    if (!createPipeline()) {
        std::fprintf(stderr, "[cornellGpuRt] createPipeline 失败\n");
        shutdownGpuRt();
        return false;
    }
    if (!createCommandAndFence()) {
        std::fprintf(stderr, "[cornellGpuRt] createCommandAndFence 失败\n");
        shutdownGpuRt();
        return false;
    }
    if (!createDescriptorsAndBuffers(width, height)) {
        std::fprintf(stderr, "[cornellGpuRt] createDescriptorsAndBuffers 失败\n");
        shutdownGpuRt();
        return false;
    }
    s_state.aReady = true;
    return true;
}

void shutdownGpuRt() {
    const VkDevice device = s_state.aCtx.aDevice;
    if (device != VK_NULL_HANDLE) {
        if (s_state.aPending && s_state.aFence != VK_NULL_HANDLE) {
            vkWaitForFences(device, 1, &s_state.aFence, VK_TRUE, UINT64_MAX);
            s_state.aPending = false;
        }
        if (s_state.aFence != VK_NULL_HANDLE) {
            vkDestroyFence(device, s_state.aFence, nullptr);
        }
        if (s_state.aCommandPool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(device, s_state.aCommandPool, nullptr);
        }
        destroyRayQueryScene(device, &s_state.aAsScene);
        destroyVkBuffer(device, &s_state.aOutBuffer);
        destroyVkBuffer(device, &s_state.aSceneBuffer);
        destroyVkBuffer(device, &s_state.aParamsBuffer);
        for (int i = 0; i < 2; ++i) {
            destroyVkBuffer(device, &s_state.aHistoryColor[i]);
            destroyVkBuffer(device, &s_state.aHistoryDepth[i]);
        }
        destroyVkBuffer(device, &s_state.aProbeShBuffer);
        destroyVkBuffer(device, &s_state.aProbeDepthBuffer);
        destroyVkBuffer(device, &s_state.aProbeOffsetBuffer);
        destroyVkBuffer(device, &s_state.aProbeStateBuffer);
        destroyVkBuffer(device, &s_state.aProbeUboBuffer);
        if (s_state.aDescriptorPool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(device, s_state.aDescriptorPool, nullptr);
        }
        if (s_state.aRelocPipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(device, s_state.aRelocPipeline, nullptr);
        }
        if (s_state.aUpdatePipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(device, s_state.aUpdatePipeline, nullptr);
        }
        if (s_state.aPipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(device, s_state.aPipeline, nullptr);
        }
        if (s_state.aPipelineLayout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(device, s_state.aPipelineLayout, nullptr);
        }
        if (s_state.aSetLayout != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(device, s_state.aSetLayout, nullptr);
        }
        if (s_state.aRelocShader != VK_NULL_HANDLE) {
            vkDestroyShaderModule(device, s_state.aRelocShader, nullptr);
        }
        if (s_state.aUpdateShader != VK_NULL_HANDLE) {
            vkDestroyShaderModule(device, s_state.aUpdateShader, nullptr);
        }
        if (s_state.aShader != VK_NULL_HANDLE) {
            vkDestroyShaderModule(device, s_state.aShader, nullptr);
        }
    }
    s_state = sState();
}

bool isGpuRtReady() {
    return s_state.aReady;
}

bool cameraChanged(const float eye[3], const float fwd[3], const float right[3],
                   const float up[3], float fovYDeg, int lightMode) {
    if (!s_state.aPrevValid) return true;
    if (lightMode != s_state.aPrevLight) return true;
    if (std::fabs(fovYDeg - s_state.aPrevFov) > 1.0e-5f) return true;
    return std::memcmp(eye, s_state.aPrevEye, sizeof(float) * 3u) != 0 ||
           std::memcmp(fwd, s_state.aPrevFwd, sizeof(float) * 3u) != 0 ||
           std::memcmp(right, s_state.aPrevRight, sizeof(float) * 3u) != 0 ||
           std::memcmp(up, s_state.aPrevUp, sizeof(float) * 3u) != 0;
}

double nowMs() {
    using clock = std::chrono::steady_clock;
    return std::chrono::duration<double, std::milli>(clock::now().time_since_epoch()).count();
}

// 等待最近一次提交并回读像素；没有 pending 帧时直接成功。
bool drainPendingFrame() {
    if (!s_state.aPending) return true;
    const double waitStart = nowMs();
    const VkResult result =
        vkWaitForFences(s_state.aCtx.aDevice, 1, &s_state.aFence, VK_TRUE, UINT64_MAX);
    const double readStart = nowMs();
    if (result != VK_SUCCESS) {
        s_state.aPending = false;
        return false;
    }
    const size_t bytes = static_cast<size_t>(s_state.aWidth) * s_state.aHeight * 4u;
    s_state.aLastPixels.resize(bytes);
    std::memcpy(s_state.aLastPixels.data(), s_state.aOutBuffer.aMapped, bytes);
    const double readEnd = nowMs();
    s_state.aPending = false;
    s_state.aStats.aWaitMs = readStart - waitStart;
    s_state.aStats.aReadbackMs = readEnd - readStart;
    s_state.aStats.aCompletedSerial++;
    s_state.aStats.aAccumSamples = s_state.aAccumTotal;
    return true;
}

void fillParams(int width, int height, const float eye[3], const float fwd[3],
                const float right[3], const float up[3], float fovYDeg, int lightMode,
                int sampleCount, bool reset, sRtParams *params) {
    *params = sRtParams();
    params->aEye[0] = eye[0];
    params->aEye[1] = eye[1];
    params->aEye[2] = eye[2];
    params->aFwd[0] = fwd[0];
    params->aFwd[1] = fwd[1];
    params->aFwd[2] = fwd[2];
    params->aRight[0] = right[0];
    params->aRight[1] = right[1];
    params->aRight[2] = right[2];
    params->aUp[0] = up[0];
    params->aUp[1] = up[1];
    params->aUp[2] = up[2];
    params->aCam[0] = std::tan(fovYDeg * 0.5f * 3.14159265359f / 180.0f);
    params->aCam[1] = static_cast<float>(width) / static_cast<float>(height);
    params->aCam[2] = static_cast<float>(lightMode);
    params->aCam[3] = static_cast<float>(sampleCount);
    params->aSize[0] = static_cast<float>(width);
    params->aSize[1] = static_cast<float>(height);
    params->aSunDir[0] = SUN_DIRECTION.x;
    params->aSunDir[1] = SUN_DIRECTION.y;
    params->aSunDir[2] = SUN_DIRECTION.z;
    params->aLightPos[0] = ROOM_WIDTH * 0.5f;
    params->aLightPos[1] = ROOM_HEIGHT - 0.2f;
    params->aLightPos[2] = ROOM_DEPTH * 0.5f;
    params->aCounts[0] = s_state.aQuadCount;
    params->aCounts[1] = s_state.aSphereCount;
    params->aCounts[2] = s_state.aMaterialCount;
    params->aAccum[0] = static_cast<float>(s_state.aAccumTotal);
    params->aAccum[1] = 0.0f; // 残留字段；历史由 aTemporal 控制
    for (int i = 0; i < 3; ++i) {
        params->aPrevEye[i] = s_state.aPrevValid ? s_state.aPrevEye[i] : eye[i];
        params->aPrevFwd[i] = s_state.aPrevValid ? s_state.aPrevFwd[i] : fwd[i];
        params->aPrevRight[i] = s_state.aPrevValid ? s_state.aPrevRight[i] : right[i];
        params->aPrevUp[i] = s_state.aPrevValid ? s_state.aPrevUp[i] : up[i];
    }
    params->aTemporal[0] = s_state.aHistoryValid ? 1.0f : 0.0f;
    params->aTemporal[1] = reset ? 1.0f : 0.0f;
    params->aTemporal[2] = static_cast<float>(s_state.aTargetSamples);
}

void rememberCamera(const float eye[3], const float fwd[3], const float right[3],
                    const float up[3], float fovYDeg, int lightMode) {
    std::memcpy(s_state.aPrevEye, eye, sizeof(float) * 3u);
    std::memcpy(s_state.aPrevFwd, fwd, sizeof(float) * 3u);
    std::memcpy(s_state.aPrevRight, right, sizeof(float) * 3u);
    std::memcpy(s_state.aPrevUp, up, sizeof(float) * 3u);
    s_state.aPrevFov = fovYDeg;
    s_state.aPrevLight = lightMode;
    s_state.aPrevValid = true;
}

bool submitFrame(int width, int height, const sRtParams &params, int sampleCount) {
    std::memcpy(s_state.aParamsBuffer.aMapped, &params, sizeof(params));
    VkCommandBufferBeginInfo bi = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (vkBeginCommandBuffer(s_state.aCommandBuffer, &bi) != VK_SUCCESS) return false;
    // B3c-2：先把本帧预算内的探针更新掉（原地混合），再屏障让渲染读到新值；
    // 全程不 vkWaitForFences / vkDeviceWaitIdle，CPU 不阻塞在更新上。
    if (s_state.aProbeAsyncEnabled && s_state.aProbeReady &&
        s_state.aUpdatePipeline != VK_NULL_HANDLE) {
        const GN3d::sPROBE_UPDATE_RANGE range =
            GN3d::nextProbeUpdateRange(&s_state.aProbeScheduler);
        if (range.aCount > 0u) {
            float *ubo = static_cast<float *>(s_state.aProbeUboBuffer.aMapped);
            if (ubo) {
                ubo[16] = static_cast<float>(range.aStart);
                ubo[17] = static_cast<float>(range.aCount);
                ubo[18] = s_state.aProbeHysteresis;
                ubo[19] = 1.0f;
            }
            vkCmdBindPipeline(s_state.aCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                              s_state.aUpdatePipeline);
            const VkDescriptorSet updateSet = s_state.aDescriptorSets[0];
            vkCmdBindDescriptorSets(s_state.aCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                                    s_state.aPipelineLayout, 0, 1, &updateSet, 0, nullptr);
            vkCmdDispatch(s_state.aCommandBuffer, range.aCount, 1, 1);
            VkMemoryBarrier probeBarrier = {VK_STRUCTURE_TYPE_MEMORY_BARRIER};
            probeBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            probeBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
            vkCmdPipelineBarrier(s_state.aCommandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &probeBarrier, 0,
                                 nullptr, 0, nullptr);
            // B3b-full：同一批探针的 depth 刚写新，紧接着做 6 轴 relocation（读 depth → 写 offset）。
            if (s_state.aProbeRelocGpuEnabled && s_state.aRelocPipeline != VK_NULL_HANDLE) {
                vkCmdBindPipeline(s_state.aCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                                  s_state.aRelocPipeline);
                vkCmdBindDescriptorSets(s_state.aCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                                        s_state.aPipelineLayout, 0, 1, &updateSet, 0, nullptr);
                vkCmdDispatch(s_state.aCommandBuffer, range.aCount, 1, 1);
                vkCmdPipelineBarrier(s_state.aCommandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                     VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &probeBarrier, 0,
                                     nullptr, 0, nullptr);
            }
        }
        ++s_state.aProbeFrameIndex;
    }
    vkCmdBindPipeline(s_state.aCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, s_state.aPipeline);
    VkDescriptorSet descriptorSet = s_state.aDescriptorSets[s_state.aHistoryIndex];
    vkCmdBindDescriptorSets(s_state.aCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                            s_state.aPipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
    const uint32_t groupsX = (static_cast<uint32_t>(width) + 7u) / 8u;
    const uint32_t groupsY = (static_cast<uint32_t>(height) + 7u) / 8u;
    vkCmdDispatch(s_state.aCommandBuffer, groupsX, groupsY, 1);
    if (vkEndCommandBuffer(s_state.aCommandBuffer) != VK_SUCCESS) return false;

    VkSubmitInfo si = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
    si.commandBufferCount = 1;
    si.pCommandBuffers = &s_state.aCommandBuffer;
    const double submitStart = nowMs();
    vkResetFences(s_state.aCtx.aDevice, 1, &s_state.aFence);
    if (vkQueueSubmit(s_state.aCtx.aQueue, 1, &si, s_state.aFence) != VK_SUCCESS) return false;
    s_state.aStats.aSubmitMs = nowMs() - submitStart;
    s_state.aPending = true;
    s_state.aHistoryIndex ^= 1;
    s_state.aHistoryValid = true;
    s_state.aAccumTotal += static_cast<unsigned int>(sampleCount);
    s_state.aStats.aLastSamples = sampleCount;
    s_state.aStats.aAccumSamples = s_state.aAccumTotal;
    return true;
}

bool renderGpuRt(int width, int height, const float eye[3], const float fwd[3],
                 const float right[3], const float up[3], float fovYDeg, int lightMode,
                 int sampleCount, std::vector<unsigned char> *outRgba) {
    if (!outRgba || !s_state.aReady || width != s_state.aWidth || height != s_state.aHeight) {
        return false;
    }
    if (!drainPendingFrame()) return false;
    if (sampleCount <= 0) sampleCount = 8;
    const bool reset = cameraChanged(eye, fwd, right, up, fovYDeg, lightMode);
    if (reset) s_state.aAccumTotal = 0;
    if (s_state.aPrevValid && lightMode != s_state.aPrevLight) s_state.aHistoryValid = false;
    if (!reset && s_state.aAccumTotal >= s_state.aTargetSamples && !s_state.aLastPixels.empty()) {
        *outRgba = s_state.aLastPixels;
        return true;
    }
    sRtParams params;
    fillParams(width, height, eye, fwd, right, up, fovYDeg, lightMode, sampleCount, reset,
               &params);
    if (!submitFrame(width, height, params, sampleCount)) return false;
    rememberCamera(eye, fwd, right, up, fovYDeg, lightMode);
    if (!drainPendingFrame()) return false;
    *outRgba = s_state.aLastPixels;
    return true;
}

bool gpuRtCollect(std::vector<unsigned char> *outRgba) {
    if (!outRgba || !s_state.aReady) return false;
    if (!drainPendingFrame()) return false;
    outRgba->clear();
    if (!s_state.aLastPixels.empty()) *outRgba = s_state.aLastPixels;
    return true;
}

bool gpuRtSubmit(int width, int height, const float eye[3], const float fwd[3],
                 const float right[3], const float up[3], float fovYDeg, int lightMode,
                 int sampleCount) {
    if (!s_state.aReady || s_state.aPending || width != s_state.aWidth ||
        height != s_state.aHeight) {
        return false;
    }
    if (sampleCount <= 0) sampleCount = 8;
    const bool reset = cameraChanged(eye, fwd, right, up, fovYDeg, lightMode);
    if (reset) s_state.aAccumTotal = 0;
    if (s_state.aPrevValid && lightMode != s_state.aPrevLight) s_state.aHistoryValid = false;
    if (!reset && s_state.aAccumTotal >= s_state.aTargetSamples && !s_state.aLastPixels.empty()) {
        return true; // 已收敛：不再 dispatch，复用上一帧
    }
    sRtParams params;
    fillParams(width, height, eye, fwd, right, up, fovYDeg, lightMode, sampleCount, reset,
               &params);
    if (!submitFrame(width, height, params, sampleCount)) return false;
    rememberCamera(eye, fwd, right, up, fovYDeg, lightMode);
    return true;
}

const sGpuRtStats &gpuRtStats() {
    return s_state.aStats;
}

bool gpuRtUploadProbes(const float *sh9, const float *depthCubemap, const float origin[3],
                       float spacing, unsigned int cx, unsigned int cy, unsigned int cz,
                       unsigned int depthRes, float weight, float normalBias, bool relocation) {
    if (!s_state.aReady || !sh9 || !depthCubemap || !origin || cx == 0u || cy == 0u || cz == 0u ||
        depthRes == 0u) {
        return false;
    }
    const VkDeviceSize bytes = static_cast<VkDeviceSize>(cx) * cy * cz * 36u * sizeof(float);
    sBuffer buffer;
    if (!createVkBuffer(s_state.aCtx.aDevice, s_state.aCtx.aPhysicalDevice, bytes,
                        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                        true, &buffer, true)) {
        return false;
    }
    std::memcpy(buffer.aMapped, sh9, static_cast<size_t>(bytes));
    const VkDeviceSize depthBytes = static_cast<VkDeviceSize>(cx) * cy * cz * 6u * depthRes *
                                    depthRes * 2u * sizeof(float);
    sBuffer depthBuffer;
    if (!createVkBuffer(s_state.aCtx.aDevice, s_state.aCtx.aPhysicalDevice, depthBytes,
                        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                        true, &depthBuffer, true)) {
        return false;
    }
    std::memcpy(depthBuffer.aMapped, depthCubemap, static_cast<size_t>(depthBytes));

    std::vector<float> offsets(static_cast<size_t>(cx) * cy * cz * 4u, 0.0f);
    if (relocation) {
        GN3d::sPROBE_RELOCATION_DESC reloc = {};
        reloc.aDepthMoments = depthCubemap;
        reloc.aProbeCount = cx * cy * cz;
        reloc.aDepthRes = depthRes;
        reloc.aSpacing = spacing;
        reloc.aThreshold = 0.5f;
        reloc.aMaxOffset = 0.5f * spacing;
        if (!GN3d::computeProbeRelocationOffsets(reloc, offsets.data())) return false;
        size_t moved = 0;
        float maxAbs = 0.0f;
        for (size_t i = 0; i < offsets.size(); i += 4u) {
            const float len = std::sqrt(offsets[i] * offsets[i] + offsets[i + 1] * offsets[i + 1] +
                                        offsets[i + 2] * offsets[i + 2]);
            if (len > 1.0e-6f) ++moved;
            if (len > maxAbs) maxAbs = len;
        }
        std::fprintf(stderr, "[cornellGpuRt] relocation offsets: moved=%zu/%zu max=%.4f\n", moved,
                     offsets.size() / 4u, maxAbs);
    }
    const VkDeviceSize offsetBytes = offsets.size() * sizeof(float);
    sBuffer offsetBuffer;
    if (!createVkBuffer(s_state.aCtx.aDevice, s_state.aCtx.aPhysicalDevice, offsetBytes,
                        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                        true, &offsetBuffer, true)) {
        return false;
    }
    std::memcpy(offsetBuffer.aMapped, offsets.data(), static_cast<size_t>(offsetBytes));

    // B3c-2：每探针状态（age/class/score/flags）；上传探针时重置，切光/重传后重新收敛。
    const unsigned int probeCount = cx * cy * cz;
    sBuffer stateBuffer;
    if (!createVkBuffer(s_state.aCtx.aDevice, s_state.aCtx.aPhysicalDevice,
                        static_cast<VkDeviceSize>(probeCount) * 16u,
                        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                        true, &stateBuffer, true)) {
        return false;
    }
    std::memset(stateBuffer.aMapped, 0, static_cast<size_t>(probeCount) * 16u);

    // 探针热切换时可能还有上一帧在飞；先等设备空闲再销毁旧 buffer。
    vkDeviceWaitIdle(s_state.aCtx.aDevice);
    float ubo[24] = {origin[0], origin[1], origin[2], 0.0f,
                    spacing, static_cast<float>(cx), static_cast<float>(cy),
                    static_cast<float>(cz), weight, static_cast<float>(depthRes), normalBias, 0.0f};
    ubo[12] = 0.5f * spacing; // relocation.x = maxOffset
    ubo[13] = 0.5f;           // relocation.y = insideThreshold
    ubo[16] = 0.0f;           // update.x = rangeStart
    ubo[17] = 0.0f;           // update.y = rangeCount
    ubo[18] = s_state.aProbeHysteresis > 0.0f ? s_state.aProbeHysteresis : 0.9f;
    ubo[19] = 0.0f;           // update.w = flags（host 每帧置 1）
    ubo[20] = static_cast<float>(s_state.aProbeSamples);
    ubo[21] = 65535.0f;       // state.y = maxAge
    ubo[22] = 0.001f;         // state.z = epsilon
    std::memcpy(s_state.aProbeUboBuffer.aMapped, ubo, sizeof(ubo));
    if (s_state.aProbeShBuffer.aMapped) {
        vkUnmapMemory(s_state.aCtx.aDevice, s_state.aProbeShBuffer.aMemory);
        s_state.aProbeShBuffer.aMapped = nullptr;
    }
    if (s_state.aProbeShBuffer.aBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(s_state.aCtx.aDevice, s_state.aProbeShBuffer.aBuffer, nullptr);
        s_state.aProbeShBuffer.aBuffer = VK_NULL_HANDLE;
    }
    if (s_state.aProbeShBuffer.aMemory != VK_NULL_HANDLE) {
        vkFreeMemory(s_state.aCtx.aDevice, s_state.aProbeShBuffer.aMemory, nullptr);
        s_state.aProbeShBuffer.aMemory = VK_NULL_HANDLE;
    }
    s_state.aProbeShBuffer = buffer;
    if (s_state.aProbeDepthBuffer.aMapped) {
        vkUnmapMemory(s_state.aCtx.aDevice, s_state.aProbeDepthBuffer.aMemory);
        s_state.aProbeDepthBuffer.aMapped = nullptr;
    }
    if (s_state.aProbeDepthBuffer.aBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(s_state.aCtx.aDevice, s_state.aProbeDepthBuffer.aBuffer, nullptr);
        s_state.aProbeDepthBuffer.aBuffer = VK_NULL_HANDLE;
    }
    if (s_state.aProbeDepthBuffer.aMemory != VK_NULL_HANDLE) {
        vkFreeMemory(s_state.aCtx.aDevice, s_state.aProbeDepthBuffer.aMemory, nullptr);
        s_state.aProbeDepthBuffer.aMemory = VK_NULL_HANDLE;
    }
    s_state.aProbeDepthBuffer = depthBuffer;
    if (s_state.aProbeOffsetBuffer.aMapped) {
        vkUnmapMemory(s_state.aCtx.aDevice, s_state.aProbeOffsetBuffer.aMemory);
        s_state.aProbeOffsetBuffer.aMapped = nullptr;
    }
    if (s_state.aProbeOffsetBuffer.aBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(s_state.aCtx.aDevice, s_state.aProbeOffsetBuffer.aBuffer, nullptr);
        s_state.aProbeOffsetBuffer.aBuffer = VK_NULL_HANDLE;
    }
    if (s_state.aProbeOffsetBuffer.aMemory != VK_NULL_HANDLE) {
        vkFreeMemory(s_state.aCtx.aDevice, s_state.aProbeOffsetBuffer.aMemory, nullptr);
        s_state.aProbeOffsetBuffer.aMemory = VK_NULL_HANDLE;
    }
    s_state.aProbeOffsetBuffer = offsetBuffer;
    destroyVkBuffer(s_state.aCtx.aDevice, &s_state.aProbeStateBuffer);
    s_state.aProbeStateBuffer = stateBuffer;
    s_state.aProbeCount = probeCount;
    GN3d::initProbeUpdateScheduler(&s_state.aProbeScheduler, probeCount, s_state.aProbeBudget);
    s_state.aProbeFrameIndex = 0u;
    VkDescriptorBufferInfo uboInfo = {s_state.aProbeUboBuffer.aBuffer, 0, 96u};
    VkDescriptorBufferInfo shInfo = {s_state.aProbeShBuffer.aBuffer, 0, VK_WHOLE_SIZE};
    VkDescriptorBufferInfo depthInfo = {s_state.aProbeDepthBuffer.aBuffer, 0, VK_WHOLE_SIZE};
    VkDescriptorBufferInfo offsetInfo = {s_state.aProbeOffsetBuffer.aBuffer, 0, VK_WHOLE_SIZE};
    VkDescriptorBufferInfo stateInfo = {s_state.aProbeStateBuffer.aBuffer, 0, VK_WHOLE_SIZE};
    for (int setIndex = 0; setIndex < 2; ++setIndex) {
        VkWriteDescriptorSet writes[5] = {};
        writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[0].dstSet = s_state.aDescriptorSets[setIndex];
        writes[0].dstBinding = 11;
        writes[0].descriptorCount = 1;
        writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writes[0].pBufferInfo = &uboInfo;
        writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[1].dstSet = s_state.aDescriptorSets[setIndex];
        writes[1].dstBinding = 12;
        writes[1].descriptorCount = 1;
        writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[1].pBufferInfo = &shInfo;
        writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[2].dstSet = s_state.aDescriptorSets[setIndex];
        writes[2].dstBinding = 13;
        writes[2].descriptorCount = 1;
        writes[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[2].pBufferInfo = &depthInfo;
        writes[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[3].dstSet = s_state.aDescriptorSets[setIndex];
        writes[3].dstBinding = 14;
        writes[3].descriptorCount = 1;
        writes[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[3].pBufferInfo = &offsetInfo;
        writes[4] = writes[3];
        writes[4].dstBinding = 15;
        writes[4].pBufferInfo = &stateInfo;
        vkUpdateDescriptorSets(s_state.aCtx.aDevice, 5, writes, 0, nullptr);
    }
    s_state.aProbeReady = true;
    // 换探针/光照后历史缓存失效，避免时域残留旧 GI。
    s_state.aHistoryValid = false;
    s_state.aAccumTotal = 0;
    return true;
}

bool gpuRtEnableProbeAsync(unsigned int budget, unsigned int samples, float hysteresis) {
    if (!s_state.aReady || !s_state.aProbeReady) return false;
    if (!createProbeUpdatePipeline()) {
        std::fprintf(stderr, "[cornellGpuRt] probe update 管线创建失败\n");
        return false;
    }
    float *ubo = static_cast<float *>(s_state.aProbeUboBuffer.aMapped);
    if (!ubo || !s_state.aProbeStateBuffer.aMapped) return false;
    s_state.aProbeBudget = budget;
    s_state.aProbeSamples = samples == 0u ? 4u : (samples > 32u ? 32u : samples);
    s_state.aProbeHysteresis = (hysteresis >= 0.0f && hysteresis <= 1.0f) ? hysteresis : 0.9f;
    ubo[18] = s_state.aProbeHysteresis;
    ubo[20] = static_cast<float>(s_state.aProbeSamples);
    GN3d::initProbeUpdateScheduler(&s_state.aProbeScheduler, s_state.aProbeCount,
                                   budget == 0u ? s_state.aProbeCount : budget);
    std::memset(s_state.aProbeStateBuffer.aMapped, 0,
                static_cast<size_t>(s_state.aProbeCount) * 16u);
    s_state.aProbeFrameIndex = 0u;
    s_state.aProbeAsyncEnabled = true;
    s_state.aHistoryValid = false;
    s_state.aAccumTotal = 0u;
    return true;
}

bool gpuRtProbeAsyncActive() {
    return s_state.aProbeAsyncEnabled;
}

bool gpuRtEnableProbeRelocationGpu(bool enabled) {
    s_state.aProbeRelocGpuEnabled = false;
    if (!enabled) return true;
    if (!s_state.aProbeReady || !s_state.aProbeAsyncEnabled) return false;
    if (!createProbeRelocationPipeline()) {
        std::fprintf(stderr, "[cornellGpuRt] probe relocation 管线创建失败\n");
        return false;
    }
    float *ubo = static_cast<float *>(s_state.aProbeUboBuffer.aMapped);
    if (!ubo) return false;
    const float spacing = ubo[4];
    ubo[12] = 0.5f * spacing;  // maxOffset（逐轴）
    ubo[13] = 0.5f;            // insideThreshold（×spacing 作为短射线长度）
    ubo[14] = 0.25f * spacing; // step（shader 内同值）
    ubo[15] = 1.0f;            // enable
    s_state.aProbeRelocGpuEnabled = true;
    return true;
}

bool gpuRtReadbackRelocation(std::vector<float> *outOffsets) {
    if (!outOffsets || !s_state.aProbeReady || s_state.aProbeCount == 0u) return false;
    if (!drainPendingFrame()) return false;
    if (!s_state.aProbeOffsetBuffer.aMapped) return false;
    outOffsets->resize(static_cast<size_t>(s_state.aProbeCount) * 4u);
    std::memcpy(outOffsets->data(), s_state.aProbeOffsetBuffer.aMapped,
                outOffsets->size() * sizeof(float));
    return true;
}

bool gpuRtMarkInactiveProbes(unsigned int count) {
    if (!s_state.aProbeAsyncEnabled || !s_state.aProbeStateBuffer.aMapped ||
        count > s_state.aProbeCount) {
        return false;
    }
    unsigned int *states = static_cast<unsigned int *>(s_state.aProbeStateBuffer.aMapped);
    for (unsigned int i = 0; i < count; ++i) states[i * 4u + 1u] = 2u; // class = inactive
    return true;
}

bool gpuRtReadbackProbes(std::vector<float> *outSh, std::vector<unsigned int> *outState) {
    if (!outSh || !outState || !s_state.aProbeReady) return false;
    if (!drainPendingFrame()) return false;
    const size_t shCount = static_cast<size_t>(s_state.aProbeCount) * 36u;
    const size_t stateCount = static_cast<size_t>(s_state.aProbeCount) * 4u;
    if (!s_state.aProbeShBuffer.aMapped || !s_state.aProbeStateBuffer.aMapped ||
        shCount == 0u) {
        return false;
    }
    outSh->resize(shCount);
    std::memcpy(outSh->data(), s_state.aProbeShBuffer.aMapped, shCount * sizeof(float));
    outState->resize(stateCount);
    std::memcpy(outState->data(), s_state.aProbeStateBuffer.aMapped,
                stateCount * sizeof(unsigned int));
    return true;
}

bool gpuRtCameraMoved(const float eye[3], const float fwd[3], const float right[3],
                      const float up[3], float fovYDeg, int lightMode) {
    return cameraChanged(eye, fwd, right, up, fovYDeg, lightMode);
}

} // namespace Cornell
