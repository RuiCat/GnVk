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

#include <cmath>
#include <cstdint>
#include <cstdio>
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
    float aAccum[4];         // x=frameStartSample, y=reset(1=清空累积), z/w 保留
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
    VkDescriptorPool aDescriptorPool;
    VkDescriptorSet aDescriptorSet;
    sBuffer aOutBuffer;
    sBuffer aSceneBuffer;
    sBuffer aParamsBuffer;
    sBuffer aAccumBuffer;
    unsigned int aAccumTotal;
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
const char *kRayQueryShaderPath = "assets/shaders/gn_cornell_rq.comp.spv";
// 累积上限：达到后停止 dispatch、回放缓存（约 34 秒 @ 16 spp/帧，足够静态观察）。
const unsigned int kMaxAccumSamples = 32768u;

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

    VkDescriptorSetLayoutBinding bindings[8] = {};
    for (int i = 0; i < 8; ++i) {
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
    VkDescriptorSetLayoutCreateInfo dslci = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    dslci.bindingCount = rayQuery ? 8u : 4u;
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
    sizes[0].descriptorCount = rayQuery ? 6u : 3u;
    sizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    sizes[1].descriptorCount = 1;
    sizes[2].type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    sizes[2].descriptorCount = 1;
    VkDescriptorPoolCreateInfo dpci = {VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    dpci.maxSets = 1;
    dpci.poolSizeCount = rayQuery ? 3u : 2u;
    dpci.pPoolSizes = sizes;
    if (vkCreateDescriptorPool(s_state.aCtx.aDevice, &dpci, nullptr, &s_state.aDescriptorPool) !=
        VK_SUCCESS) {
        return false;
    }
    VkDescriptorSetAllocateInfo dsai = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    dsai.descriptorPool = s_state.aDescriptorPool;
    dsai.descriptorSetCount = 1;
    dsai.pSetLayouts = &s_state.aSetLayout;
    if (vkAllocateDescriptorSets(s_state.aCtx.aDevice, &dsai, &s_state.aDescriptorSet) !=
        VK_SUCCESS) {
        return false;
    }

    const VkDeviceSize outBytes = static_cast<VkDeviceSize>(width) * height * 4u;
    if (!createVkBuffer(s_state.aCtx.aDevice, s_state.aCtx.aPhysicalDevice, outBytes,
                        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                        true, &s_state.aOutBuffer)) {
        return false;
    }
    const std::vector<float> scene = packScene(geom);
    const VkDeviceSize sceneBytes = static_cast<VkDeviceSize>(scene.size()) * sizeof(float);
    if (!createVkBuffer(s_state.aCtx.aDevice, s_state.aCtx.aPhysicalDevice, sceneBytes,
                        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                        true, &s_state.aSceneBuffer)) {
        return false;
    }
    std::memcpy(s_state.aSceneBuffer.aMapped, scene.data(), static_cast<size_t>(sceneBytes));
    if (!createVkBuffer(s_state.aCtx.aDevice, s_state.aCtx.aPhysicalDevice, sizeof(sRtParams),
                        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                        true, &s_state.aParamsBuffer)) {
        return false;
    }
    const VkDeviceSize accumBytes = static_cast<VkDeviceSize>(width) * height * 16u;
    if (!createVkBuffer(s_state.aCtx.aDevice, s_state.aCtx.aPhysicalDevice, accumBytes,
                        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, false, &s_state.aAccumBuffer)) {
        return false;
    }

    VkDescriptorBufferInfo outInfo = {s_state.aOutBuffer.aBuffer, 0, VK_WHOLE_SIZE};
    VkDescriptorBufferInfo sceneInfo = {s_state.aSceneBuffer.aBuffer, 0, VK_WHOLE_SIZE};
    VkDescriptorBufferInfo paramsInfo = {s_state.aParamsBuffer.aBuffer, 0, sizeof(sRtParams)};
    VkDescriptorBufferInfo accumInfo = {s_state.aAccumBuffer.aBuffer, 0, VK_WHOLE_SIZE};
    VkDescriptorBufferInfo positionInfo = {s_state.aAsScene.aVertexBuffer.aBuffer, 0,
                                           VK_WHOLE_SIZE};
    VkDescriptorBufferInfo normalInfo = {s_state.aAsScene.aNormalBuffer.aBuffer, 0,
                                         VK_WHOLE_SIZE};
    VkDescriptorBufferInfo metaInfo = {s_state.aAsScene.aTriMetaBuffer.aBuffer, 0, VK_WHOLE_SIZE};
    VkWriteDescriptorSetAccelerationStructureKHR asWrite = {
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR};
    asWrite.accelerationStructureCount = 1;
    asWrite.pAccelerationStructures = &s_state.aAsScene.aTlas;
    VkWriteDescriptorSet writes[8] = {};
    const uint32_t writeCount = rayQuery ? 8u : 4u;
    for (uint32_t i = 0; i < writeCount; ++i) {
        writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[i].dstSet = s_state.aDescriptorSet;
        writes[i].dstBinding = i;
        writes[i].descriptorCount = 1;
    }
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[0].pBufferInfo = &outInfo;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[1].pBufferInfo = &sceneInfo;
    writes[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[2].pBufferInfo = &paramsInfo;
    writes[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[3].pBufferInfo = &accumInfo;
    if (rayQuery) {
        writes[4].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
        writes[4].pNext = &asWrite;
        writes[5].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[5].pBufferInfo = &positionInfo;
        writes[6].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[6].pBufferInfo = &normalInfo;
        writes[7].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[7].pBufferInfo = &metaInfo;
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
    if (!createPipeline() || !createCommandAndFence() ||
        !createDescriptorsAndBuffers(width, height)) {
        std::fprintf(stderr, "[cornellGpuRt] Vulkan 资源创建失败\n");
        shutdownGpuRt();
        return false;
    }
    s_state.aReady = true;
    return true;
}

void shutdownGpuRt() {
    const VkDevice device = s_state.aCtx.aDevice;
    if (device != VK_NULL_HANDLE) {
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
        destroyVkBuffer(device, &s_state.aAccumBuffer);
        if (s_state.aDescriptorPool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(device, s_state.aDescriptorPool, nullptr);
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

bool renderGpuRt(int width, int height, const float eye[3], const float fwd[3],
                 const float right[3], const float up[3], float fovYDeg, int lightMode,
                 int sampleCount, std::vector<unsigned char> *outRgba) {
    if (!outRgba || !s_state.aReady || width != s_state.aWidth || height != s_state.aHeight) {
        return false;
    }
    if (sampleCount <= 0) sampleCount = 8;
    sRtParams params = {};
    params.aEye[0] = eye[0];
    params.aEye[1] = eye[1];
    params.aEye[2] = eye[2];
    params.aFwd[0] = fwd[0];
    params.aFwd[1] = fwd[1];
    params.aFwd[2] = fwd[2];
    params.aRight[0] = right[0];
    params.aRight[1] = right[1];
    params.aRight[2] = right[2];
    params.aUp[0] = up[0];
    params.aUp[1] = up[1];
    params.aUp[2] = up[2];
    params.aCam[0] = std::tan(fovYDeg * 0.5f * 3.14159265359f / 180.0f);
    params.aCam[1] = static_cast<float>(width) / static_cast<float>(height);
    params.aCam[2] = static_cast<float>(lightMode);
    params.aCam[3] = static_cast<float>(sampleCount);
    params.aSize[0] = static_cast<float>(width);
    params.aSize[1] = static_cast<float>(height);
    params.aSunDir[0] = SUN_DIRECTION.x;
    params.aSunDir[1] = SUN_DIRECTION.y;
    params.aSunDir[2] = SUN_DIRECTION.z;
    params.aLightPos[0] = ROOM_WIDTH * 0.5f;
    params.aLightPos[1] = ROOM_HEIGHT - 0.2f;
    params.aLightPos[2] = ROOM_DEPTH * 0.5f;
    params.aCounts[0] = s_state.aQuadCount;
    params.aCounts[1] = s_state.aSphereCount;
    params.aCounts[2] = s_state.aMaterialCount;
    // 相机/光照未变则逐帧累积（噪点按 1/√N 收敛）；变化时从 0 重来。
    const bool reset = cameraChanged(eye, fwd, right, up, fovYDeg, lightMode);
    if (reset) {
        s_state.aAccumTotal = 0;
        s_state.aLastPixels.clear();
    }
    if (s_state.aAccumTotal >= kMaxAccumSamples && !s_state.aLastPixels.empty()) {
        *outRgba = s_state.aLastPixels;
        return true;
    }
    params.aAccum[0] = static_cast<float>(s_state.aAccumTotal);
    params.aAccum[1] = reset ? 1.0f : 0.0f;
    std::memcpy(s_state.aParamsBuffer.aMapped, &params, sizeof(params));

    VkCommandBufferBeginInfo bi = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (vkBeginCommandBuffer(s_state.aCommandBuffer, &bi) != VK_SUCCESS) return false;
    vkCmdBindPipeline(s_state.aCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, s_state.aPipeline);
    vkCmdBindDescriptorSets(s_state.aCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                            s_state.aPipelineLayout, 0, 1, &s_state.aDescriptorSet, 0, nullptr);
    const uint32_t groupsX = (static_cast<uint32_t>(width) + 7u) / 8u;
    const uint32_t groupsY = (static_cast<uint32_t>(height) + 7u) / 8u;
    vkCmdDispatch(s_state.aCommandBuffer, groupsX, groupsY, 1);
    if (vkEndCommandBuffer(s_state.aCommandBuffer) != VK_SUCCESS) return false;

    VkSubmitInfo si = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
    si.commandBufferCount = 1;
    si.pCommandBuffers = &s_state.aCommandBuffer;
    vkResetFences(s_state.aCtx.aDevice, 1, &s_state.aFence);
    if (vkQueueSubmit(s_state.aCtx.aQueue, 1, &si, s_state.aFence) != VK_SUCCESS) return false;
    if (vkWaitForFences(s_state.aCtx.aDevice, 1, &s_state.aFence, VK_TRUE, UINT64_MAX) !=
        VK_SUCCESS) {
        return false;
    }

    outRgba->resize(static_cast<size_t>(width) * height * 4u);
    std::memcpy(outRgba->data(), s_state.aOutBuffer.aMapped, outRgba->size());
    s_state.aAccumTotal += static_cast<unsigned int>(sampleCount);
    s_state.aLastPixels = *outRgba;
    std::memcpy(s_state.aPrevEye, eye, sizeof(float) * 3u);
    std::memcpy(s_state.aPrevFwd, fwd, sizeof(float) * 3u);
    std::memcpy(s_state.aPrevRight, right, sizeof(float) * 3u);
    std::memcpy(s_state.aPrevUp, up, sizeof(float) * 3u);
    s_state.aPrevFov = fovYDeg;
    s_state.aPrevLight = lightMode;
    s_state.aPrevValid = true;
    return true;
}

} // namespace Cornell
