// vkParticleCompute.cpp — P9 GPU 粒子模拟：SSBO 存储缓冲 + compute 管线 + 调度。
//
// 契约：
//   * 粒子数据存于一个 64B/粒子的 host-visible SSBO（STORAGE_BUFFER，持久映射），
//     CPU 通过 GN_VkParticleUpload 写入、通过 GN_VkParticleData 读回（fence 之后）。
//   * compute 管线经 GN_Vk.aPipelineCache 创建（ePIPE_COMPUTE），set 0 =
//     binding0 UNIFORM_BUFFER（模拟参数）、binding1 STORAGE_BUFFER（粒子）。
//   * 每次 GN_VkParticleSimulate 用独立命令缓冲提交并等待 fence：调用方须保证
//     上一帧已提交完成（引擎在帧边界等待），v1 不跨帧流水化。
//   * GPU 不可用/未创建时返回 false，CPU 回退（GN3d::stepParticlesCpu）仍可用。
#include <GN.h>
#include "GN_Vk.h"
#include "vkInternal.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

namespace GNg {

namespace {

// 模拟参数 UBO（std140，与 gn3d_particle.comp 的 ParticleSimUBO 逐字段一致）。
struct sPARTICLE_SIM_UBO {
    float aGravity[4]; // xyz = 重力，w = dt
    float aParams[4]; // x = drag，y = 累计时间，z = 粒子数，w = 保留
};

VkDescriptorSetLayout s_particleSetLayout = VK_NULL_HANDLE;
VkPipelineLayout s_particlePipelineLayout = VK_NULL_HANDLE;
VkPipeline s_particlePipeline = VK_NULL_HANDLE;

VkCommandBuffer s_particleCmd = VK_NULL_HANDLE;
VkFence s_particleFence = VK_NULL_HANDLE;

VkBuffer s_particleBuffer = VK_NULL_HANDLE;
VkDeviceMemory s_particleMemory = VK_NULL_HANDLE;
void *s_particleMapped = 0;
uint32_t s_particleCapacity = 0;

VkBuffer s_simUboBuffer = VK_NULL_HANDLE;
VkDeviceMemory s_simUboMemory = VK_NULL_HANDLE;
void *s_simUboMapped = 0;
VkDescriptorSet s_simSet = VK_NULL_HANDLE;

uint32_t s_particleCountHint = 0;
GN3d::sPARTICLE_SIM_DESC s_simDesc = {{0.0f, 0.0f, 0.0f}, 0.0f};
float s_simTime = 0.0f;

bool createParticlePipeline() {
    // set 0：binding0 = 模拟参数 UBO，binding1 = 粒子 SSBO，均只在 compute 阶段使用。
    VkDescriptorSetLayoutBinding bindings[2] = {};
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    VkDescriptorSetLayoutCreateInfo dslci = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    dslci.bindingCount = 2;
    dslci.pBindings = bindings;
    if(vkCreateDescriptorSetLayout(GN_Vk.device, &dslci, 0, &s_particleSetLayout) != VK_SUCCESS) {
        s_particleSetLayout = VK_NULL_HANDLE;
        return false;
    }

    VkPipelineLayoutCreateInfo plci = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    plci.setLayoutCount = 1;
    plci.pSetLayouts = &s_particleSetLayout;
    if(vkCreatePipelineLayout(GN_Vk.device, &plci, 0, &s_particlePipelineLayout) != VK_SUCCESS) {
        s_particlePipelineLayout = VK_NULL_HANDLE;
        return false;
    }

    std::vector<uint32_t> compSpv = LoadSPIRVFile("assets/shaders/gn3d_particle.comp.spv");
    if(compSpv.empty()) {
        fprintf(stderr, "[GN_Vk] 粒子计算着色器加载失败: gn3d_particle.comp.spv\n");
        return false;
    }
    GN3d::sPIPELINE_DESC desc; // 计算管线忽略图形字段
    desc.aKind = GN3d::ePIPE_COMPUTE;
    desc.aCompSpv = compSpv.data();
    desc.aCompSize = compSpv.size() * sizeof(uint32_t);
    desc.aLayout = s_particlePipelineLayout;
    s_particlePipeline = GN_Vk.aPipelineCache.GetOrCreate(desc);
    if(!s_particlePipeline) {
        fprintf(stderr, "[GN_Vk] 粒子计算管线创建失败: %s\n", GN_Vk.aPipelineCache.GetLastError());
        return false;
    }
    return true;
}

bool createParticleCommandBuffer() {
    VkCommandBufferAllocateInfo cbai = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    cbai.commandPool = GN_Vk.commandPool;
    cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbai.commandBufferCount = 1;
    if(vkAllocateCommandBuffers(GN_Vk.device, &cbai, &s_particleCmd) != VK_SUCCESS) {
        s_particleCmd = VK_NULL_HANDLE;
        return false;
    }
    VkFenceCreateInfo fci = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    if(vkCreateFence(GN_Vk.device, &fci, 0, &s_particleFence) != VK_SUCCESS) {
        s_particleFence = VK_NULL_HANDLE;
        return false;
    }
    return true;
}

void writeSimDescriptorSet() {
    if(s_simSet == VK_NULL_HANDLE) return;
    VkDescriptorBufferInfo ubo = {};
    ubo.buffer = s_simUboBuffer;
    ubo.offset = 0;
    ubo.range = sizeof(sPARTICLE_SIM_UBO);
    VkDescriptorBufferInfo ssbo = {};
    ssbo.buffer = s_particleBuffer;
    ssbo.offset = 0;
    ssbo.range = VK_WHOLE_SIZE;
    VkWriteDescriptorSet writes[2] = {{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET},
                                     {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET}};
    writes[0].dstSet = s_simSet;
    writes[0].dstBinding = 0;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[0].descriptorCount = 1;
    writes[0].pBufferInfo = &ubo;
    writes[1].dstSet = s_simSet;
    writes[1].dstBinding = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[1].descriptorCount = 1;
    writes[1].pBufferInfo = &ssbo;
    vkUpdateDescriptorSets(GN_Vk.device, 2, writes, 0, 0);
}

void releaseParticleBuffers() {
    if(!GN_Vk.device) {
        s_particleBuffer = VK_NULL_HANDLE;
        s_particleMemory = VK_NULL_HANDLE;
        s_simUboBuffer = VK_NULL_HANDLE;
        s_simUboMemory = VK_NULL_HANDLE;
        s_particleMapped = 0;
        s_simUboMapped = 0;
        s_particleCapacity = 0;
        return;
    }
    if(s_simSet != VK_NULL_HANDLE && GN_Vk.descriptorPool) {
        vkFreeDescriptorSets(GN_Vk.device, GN_Vk.descriptorPool, 1, &s_simSet);
    }
    s_simSet = VK_NULL_HANDLE;
    particleDrawUnbindBuffer();
    if(s_particleBuffer != VK_NULL_HANDLE) vkDestroyBuffer(GN_Vk.device, s_particleBuffer, 0);
    if(s_particleMemory != VK_NULL_HANDLE) vkFreeMemory(GN_Vk.device, s_particleMemory, 0);
    if(s_simUboBuffer != VK_NULL_HANDLE) vkDestroyBuffer(GN_Vk.device, s_simUboBuffer, 0);
    if(s_simUboMemory != VK_NULL_HANDLE) vkFreeMemory(GN_Vk.device, s_simUboMemory, 0);
    s_particleBuffer = VK_NULL_HANDLE;
    s_particleMemory = VK_NULL_HANDLE;
    s_simUboBuffer = VK_NULL_HANDLE;
    s_simUboMemory = VK_NULL_HANDLE;
    s_particleMapped = 0;
    s_simUboMapped = 0;
    s_particleCapacity = 0;
    s_particleCountHint = 0;
}

} // namespace

bool init3DParticle() {
    if(s_particlePipeline != VK_NULL_HANDLE) return true;
    if(!GN_Vk.device) return false; // 注意：本函数在 GN_Vk.initialized 置位之前调用
    if(!createParticlePipeline() || !createParticleCommandBuffer()) {
        destroyParticle();
        return false;
    }
    return true;
}

void destroyParticle() {
    releaseParticleBuffers();
    if(GN_Vk.device) {
        if(s_particleFence != VK_NULL_HANDLE) {
            vkDestroyFence(GN_Vk.device, s_particleFence, 0);
        }
        if(s_particleCmd != VK_NULL_HANDLE && GN_Vk.commandPool) {
            vkFreeCommandBuffers(GN_Vk.device, GN_Vk.commandPool, 1, &s_particleCmd);
        }
        if(s_particlePipelineLayout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(GN_Vk.device, s_particlePipelineLayout, 0);
        }
        if(s_particleSetLayout != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(GN_Vk.device, s_particleSetLayout, 0);
        }
    }
    s_particleFence = VK_NULL_HANDLE;
    s_particleCmd = VK_NULL_HANDLE;
    s_particlePipelineLayout = VK_NULL_HANDLE;
    s_particleSetLayout = VK_NULL_HANDLE;
    s_particlePipeline = VK_NULL_HANDLE; // 管线归 aPipelineCache 所有
}

bool GN_VkParticleCreate(uint32_t capacity, const GN3d::sPARTICLE_SIM_DESC &sim) {
    if(!GN_Vk.device || !GN_Vk.descriptorPool) return false;
    if(s_particlePipeline == VK_NULL_HANDLE) return false;
    if(capacity == 0 || capacity > GN3d::PARTICLE_GPU_MAX_CAPACITY) return false;
    if(!GN3d::validateParticleSimDesc(sim)) return false;

    releaseParticleBuffers(); // 允许重复 Create：先释放旧系统

    const VkDeviceSize bytes = (VkDeviceSize)capacity * sizeof(GN3d::sGPU_PARTICLE);
    CreateBuf(bytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
              s_particleBuffer, s_particleMemory);
    if(s_particleBuffer == VK_NULL_HANDLE || s_particleMemory == VK_NULL_HANDLE) {
        releaseParticleBuffers();
        return false;
    }
    if(vkMapMemory(GN_Vk.device, s_particleMemory, 0, VK_WHOLE_SIZE, 0, &s_particleMapped) !=
       VK_SUCCESS) {
        s_particleMapped = 0;
        releaseParticleBuffers();
        return false;
    }
    memset(s_particleMapped, 0, (size_t)bytes); // 全部空槽，避免未初始化数据被绘制

    CreateBuf(sizeof(sPARTICLE_SIM_UBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
              s_simUboBuffer, s_simUboMemory);
    if(s_simUboBuffer == VK_NULL_HANDLE || s_simUboMemory == VK_NULL_HANDLE ||
       vkMapMemory(GN_Vk.device, s_simUboMemory, 0, VK_WHOLE_SIZE, 0, &s_simUboMapped) !=
           VK_SUCCESS) {
        s_simUboMapped = 0;
        releaseParticleBuffers();
        return false;
    }

    VkDescriptorSetAllocateInfo dai = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    dai.descriptorPool = GN_Vk.descriptorPool;
    dai.descriptorSetCount = 1;
    dai.pSetLayouts = &s_particleSetLayout;
    if(vkAllocateDescriptorSets(GN_Vk.device, &dai, &s_simSet) != VK_SUCCESS) {
        s_simSet = VK_NULL_HANDLE;
        releaseParticleBuffers();
        return false;
    }
    writeSimDescriptorSet();

    s_particleCapacity = capacity;
    s_particleCountHint = capacity;
    s_simDesc = sim;
    s_simTime = 0.0f;
    if(!particleDrawBindBuffer(s_particleBuffer, capacity)) {
        releaseParticleBuffers();
        return false;
    }
    return true;
}

bool GN_VkParticleUpload(const GN3d::sGPU_PARTICLE *particles, uint32_t count, uint32_t offset) {
    if(!s_particleMapped || !particles || count == 0) return false;
    if(offset > s_particleCapacity || count > s_particleCapacity - offset) return false;
    const size_t bytes = (size_t)count * sizeof(GN3d::sGPU_PARTICLE);
    memcpy((unsigned char *)s_particleMapped + (size_t)offset * sizeof(GN3d::sGPU_PARTICLE),
           particles, bytes);
    return true;
}

bool GN_VkParticleSimulate(float dt) {
    if(!s_particleMapped || !s_simUboMapped || !s_particleBuffer) return false;
    if(s_particlePipeline == VK_NULL_HANDLE || s_particleCmd == VK_NULL_HANDLE ||
       s_particleFence == VK_NULL_HANDLE) {
        return false;
    }
    if(!(dt > 0.0f) || !std::isfinite(dt)) return false;
    const uint32_t count = s_particleCountHint;
    if(count == 0) return false;

    // 重力/阻尼来自创建时的 sPARTICLE_SIM_DESC；dt 与本次粒子数逐帧更新。
    sPARTICLE_SIM_UBO &ubo = *(sPARTICLE_SIM_UBO *)s_simUboMapped;
    ubo.aGravity[0] = s_simDesc.aGravity[0];
    ubo.aGravity[1] = s_simDesc.aGravity[1];
    ubo.aGravity[2] = s_simDesc.aGravity[2];
    ubo.aGravity[3] = dt;
    ubo.aParams[0] = s_simDesc.aDrag;
    ubo.aParams[1] = s_simTime;
    ubo.aParams[2] = (float)count;
    ubo.aParams[3] = 0.0f;

    VkCommandBufferBeginInfo cbbi = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    cbbi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if(vkBeginCommandBuffer(s_particleCmd, &cbbi) != VK_SUCCESS) return false;

    VkBufferMemoryBarrier barriers[2] = {{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER},
                                         {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER}};
    barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[0].buffer = s_particleBuffer;
    barriers[0].offset = 0;
    barriers[0].size = VK_WHOLE_SIZE;
    barriers[0].srcAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
    barriers[0].dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    vkCmdPipelineBarrier(s_particleCmd,
                         VK_PIPELINE_STAGE_HOST_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, 0, 1, &barriers[0], 0, 0);

    vkCmdBindPipeline(s_particleCmd, VK_PIPELINE_BIND_POINT_COMPUTE, s_particlePipeline);
    vkCmdBindDescriptorSets(s_particleCmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                            s_particlePipelineLayout, 0, 1, &s_simSet, 0, 0);
    const uint32_t groups = (count + GN3d::PARTICLE_GPU_WORKGROUP_SIZE - 1) /
                            GN3d::PARTICLE_GPU_WORKGROUP_SIZE;
    vkCmdDispatch(s_particleCmd, groups, 1, 1);

    // compute 写 → 后续图形读 / 主机回读：同一队列内已提交顺序可用，这里补齐内存依赖。
    barriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[1].buffer = s_particleBuffer;
    barriers[1].offset = 0;
    barriers[1].size = VK_WHOLE_SIZE;
    barriers[1].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barriers[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_HOST_READ_BIT;
    vkCmdPipelineBarrier(s_particleCmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_VERTEX_SHADER_BIT |
                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_HOST_BIT,
                         0, 0, 0, 1, &barriers[1], 0, 0);

    if(vkEndCommandBuffer(s_particleCmd) != VK_SUCCESS) return false;

    VkSubmitInfo si = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
    si.commandBufferCount = 1;
    si.pCommandBuffers = &s_particleCmd;
    vkResetFences(GN_Vk.device, 1, &s_particleFence);
    if(vkQueueSubmit(GN_Vk.graphicsQueue, 1, &si, s_particleFence) != VK_SUCCESS) return false;
    const VkResult wait = vkWaitForFences(GN_Vk.device, 1, &s_particleFence, VK_TRUE, 2000000000ull);
    if(wait != VK_SUCCESS) {
        fprintf(stderr, "[GN_Vk] 粒子 compute 等待超时: %d\n", (int)wait);
        return false;
    }
    s_simTime += dt;
    return true;
}

const GN3d::sGPU_PARTICLE *GN_VkParticleData(uint32_t *outCount) {
    if(outCount) *outCount = s_particleCapacity;
    if(!s_particleMapped) return 0;
    return (const GN3d::sGPU_PARTICLE *)s_particleMapped;
}

void GN_VkParticleDestroy() { releaseParticleBuffers(); }

} // namespace GNg
