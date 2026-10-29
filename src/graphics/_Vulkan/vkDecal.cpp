// vkDecal.cpp — P11-S6b：地面贴花绘制（水平四边形 + 软边/环状 alpha）。
//
// 契约（GN_Vk.h 冻结）：GN_VkDrawDecals / GN_VkDecalDestroy。
//   * 每条贴花 = 世界 XZ 平面上的一块四边形（y 由调用方给，通常贴地 +0.02）；
//     uv 供片元做软圆斑（kind != SPLASH_RING）或溅射环（kind == SPLASH_RING）。
//   * alpha 混合、深度测试开（LESS_OR_EQUAL，允许贴花与地面共面）、深度写关、不剔除。
//   * 4 个顶点缓冲槽位 =（currentFrame % 2）× 帧内调用序号，同帧可多次调用不互相覆盖。
#include <GN.h>
#include "GN_Vk.h"
#include "vkInternal.h"

#include <cstring>
#include <vector>

namespace GNg {
namespace {

// 顶点：世界位置 + uv + 颜色 + 种类（float，便于与着色器 float 通道对齐）。
struct sDECAL_VERTEX {
    float aPosition[3];
    float aUv[2];
    float aColor[4];
    float aKind;
};

// push constant：viewProj(64) + params(16)：x = alphaScale。
struct sDECAL_PUSH {
    float aViewProj[16];
    float aParams[4];
};

const uint32_t kSlotCount = 4;
const uint32_t kInitialVertexCapacity = 512;
const uint32_t kQuadOrder[6] = {0, 1, 2, 0, 2, 3};
const float kCornerX[4] = {-1.0f, 1.0f, 1.0f, -1.0f};
const float kCornerZ[4] = {-1.0f, -1.0f, 1.0f, 1.0f};

struct sVERTEX_SLOT {
    VkBuffer aBuffer;
    VkDeviceMemory aMemory;
    uint32_t aCapacity; // 顶点个数（非字节）
};

sVERTEX_SLOT s_slots[kSlotCount];
VkPipelineLayout s_pipelineLayout = VK_NULL_HANDLE;
VkPipeline s_pipeline = VK_NULL_HANDLE;
uint32_t s_slotFrame = 0xFFFFFFFFu;
uint32_t s_callInFrame = 0;

void releaseSlot(sVERTEX_SLOT &slot) {
    if(GN_Vk.device != VK_NULL_HANDLE) {
        if(slot.aBuffer != VK_NULL_HANDLE) vkDestroyBuffer(GN_Vk.device, slot.aBuffer, nullptr);
        if(slot.aMemory != VK_NULL_HANDLE) vkFreeMemory(GN_Vk.device, slot.aMemory, nullptr);
    }
    slot.aBuffer = VK_NULL_HANDLE;
    slot.aMemory = VK_NULL_HANDLE;
    slot.aCapacity = 0;
}

void releaseAllSlots() {
    for(uint32_t i = 0; i < kSlotCount; ++i) releaseSlot(s_slots[i]);
}

// 容量不足时按 2× 扩容；旧缓冲在新缓冲创建成功后才释放（失败保留旧状态）。
bool ensureSlotCapacity(sVERTEX_SLOT &slot, uint32_t vertexCount) {
    if(vertexCount <= slot.aCapacity) return true;
    uint32_t newCapacity = slot.aCapacity ? slot.aCapacity : kInitialVertexCapacity;
    while(newCapacity < vertexCount) {
        if(newCapacity > 0x7FFFFFFFu) return false;
        newCapacity *= 2;
    }
    VkBuffer newBuffer = VK_NULL_HANDLE;
    VkDeviceMemory newMemory = VK_NULL_HANDLE;
    CreateBuf((VkDeviceSize)newCapacity * sizeof(sDECAL_VERTEX), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
              newBuffer, newMemory);
    if(newBuffer == VK_NULL_HANDLE || newMemory == VK_NULL_HANDLE) {
        if(GN_Vk.device != VK_NULL_HANDLE) {
            if(newBuffer != VK_NULL_HANDLE) vkDestroyBuffer(GN_Vk.device, newBuffer, nullptr);
            if(newMemory != VK_NULL_HANDLE) vkFreeMemory(GN_Vk.device, newMemory, nullptr);
        }
        return false;
    }
    releaseSlot(slot);
    slot.aBuffer = newBuffer;
    slot.aMemory = newMemory;
    slot.aCapacity = newCapacity;
    return true;
}

GN3d::sPIPELINE_DESC makePipelineDesc() {
    GN3d::sVERTEX_LAYOUT vertexLayout;
    vertexLayout.Add(0, (uint32_t)offsetof(sDECAL_VERTEX, aPosition), VK_FORMAT_R32G32B32_SFLOAT);
    vertexLayout.Add(1, (uint32_t)offsetof(sDECAL_VERTEX, aUv), VK_FORMAT_R32G32_SFLOAT);
    vertexLayout.Add(2, (uint32_t)offsetof(sDECAL_VERTEX, aColor), VK_FORMAT_R32G32B32A32_SFLOAT);
    vertexLayout.Add(3, (uint32_t)offsetof(sDECAL_VERTEX, aKind), VK_FORMAT_R32_SFLOAT);
    vertexLayout.aStride = (uint32_t)sizeof(sDECAL_VERTEX);

    GN3d::sPIPELINE_DESC desc;
    desc.aKind = GN3d::ePIPE_GRAPHICS;
    desc.aVertexLayout = vertexLayout;
    desc.aRenderPass = GN_Vk.renderPass3D;
    desc.aSubpass = 0;
    desc.aLayout = s_pipelineLayout;
    desc.aDepthTest = true;
    desc.aDepthWrite = false;
    desc.aDepthCompare = VK_COMPARE_OP_LESS_OR_EQUAL; // 与地面共面也可通过
    desc.aDepthStencil = true;
    desc.aCullBack = false;
    desc.aFrontFace = VK_FRONT_FACE_CLOCKWISE; // 世界 CCW + Y 翻转
    desc.aBlend = GN3d::eBLEND_ALPHA;
    desc.aTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    desc.aSamples = GN_Vk.msaaSamples;
    desc.aColorAttachmentCount = 1;
    desc.aDynamicViewport = false;
    desc.aDynamicScissor = false;
    // 与场景一致的正高度视口：竖直方向由投影矩阵决定；负高度视口会把贴花整片上下镜像。
    desc.aViewport = VkViewport{0.0f, 0.0f, (float)GN_Vk.swapchainExtent.width,
                                (float)GN_Vk.swapchainExtent.height, 0.0f, 1.0f};
    desc.aScissor = VkRect2D{{0, 0}, GN_Vk.swapchainExtent};
    desc.aCustomBlend = false;
    return desc;
}

bool createPipeline() {
    if(GN_Vk.device == VK_NULL_HANDLE || GN_Vk.renderPass3D == VK_NULL_HANDLE) return false;

    std::vector<uint32_t> vertSpv = LoadSPIRVFile("assets/shaders/gn3d_decal.vert.spv");
    std::vector<uint32_t> fragSpv = LoadSPIRVFile("assets/shaders/gn3d_decal.frag.spv");
    if(vertSpv.empty() || fragSpv.empty()) return false;

    VkPushConstantRange pushRange = {};
    pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushRange.offset = 0;
    pushRange.size = sizeof(sDECAL_PUSH);
    VkPipelineLayoutCreateInfo layoutInfo = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    layoutInfo.setLayoutCount = 0;
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &pushRange;
    if(vkCreatePipelineLayout(GN_Vk.device, &layoutInfo, nullptr, &s_pipelineLayout) != VK_SUCCESS) {
        s_pipelineLayout = VK_NULL_HANDLE;
        return false;
    }

    GN3d::sPIPELINE_DESC desc = makePipelineDesc();
    desc.aVertSpv = vertSpv.data();
    desc.aVertSize = vertSpv.size() * sizeof(uint32_t);
    desc.aFragSpv = fragSpv.data();
    desc.aFragSize = fragSpv.size() * sizeof(uint32_t);
    s_pipeline = GN_Vk.aPipelineCache.GetOrCreate(desc);
    if(s_pipeline == VK_NULL_HANDLE) {
        if(s_pipelineLayout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(GN_Vk.device, s_pipelineLayout, nullptr);
            s_pipelineLayout = VK_NULL_HANDLE;
        }
        return false;
    }
    return true;
}

// 一条贴花写成 6 个顶点（世界 XZ 平面）。
void writeDecalQuad(sDECAL_VERTEX *out, const GN3d::sDECAL_RENDER_ITEM &item,
                    float alphaScale) {
    const float halfSize = item.aSize * 0.5f;
    const float alpha = item.aColor[3] * alphaScale;
    for(uint32_t i = 0; i < 6; ++i) {
        const uint32_t corner = kQuadOrder[i];
        const float sx = kCornerX[corner];
        const float sz = kCornerZ[corner];
        out[i].aPosition[0] = item.aPosition[0] + sx * halfSize;
        out[i].aPosition[1] = item.aPosition[1];
        out[i].aPosition[2] = item.aPosition[2] + sz * halfSize;
        out[i].aUv[0] = sx * 0.5f + 0.5f;
        out[i].aUv[1] = sz * 0.5f + 0.5f;
        out[i].aColor[0] = item.aColor[0];
        out[i].aColor[1] = item.aColor[1];
        out[i].aColor[2] = item.aColor[2];
        out[i].aColor[3] = alpha;
        out[i].aKind = (float)item.aKind;
    }
}

uint32_t acquireSlot() {
    if(GN_Vk.currentFrame != s_slotFrame) {
        s_slotFrame = GN_Vk.currentFrame;
        s_callInFrame = 0;
    }
    const uint32_t base = (s_slotFrame % 2u) * 2u;
    const uint32_t slot = base + (s_callInFrame % 2u);
    ++s_callInFrame;
    return slot;
}

} // namespace

bool GN_VkDrawDecals(const sDECAL_DRAW_PARAMS &params) {
    if(!GN_Vk.initialized || !s_in3DPass) return false;
    if(params.aCount == 0 || params.aItems == nullptr) return false;
    if(params.aCount > 0xFFFFFFFFu / 6u) return false;
    if(s_pipeline == VK_NULL_HANDLE && !createPipeline()) return false;

    const uint32_t slotIndex = acquireSlot();
    sVERTEX_SLOT &slot = s_slots[slotIndex];
    const uint32_t vertexCount = params.aCount * 6u;
    if(!ensureSlotCapacity(slot, vertexCount)) return false;

    const VkDeviceSize byteSize = (VkDeviceSize)vertexCount * sizeof(sDECAL_VERTEX);
    void *mapped = nullptr;
    if(vkMapMemory(GN_Vk.device, slot.aMemory, 0, byteSize, 0, &mapped) != VK_SUCCESS ||
       mapped == nullptr) {
        return false;
    }
    sDECAL_VERTEX *out = (sDECAL_VERTEX *)mapped;
    for(uint32_t i = 0; i < params.aCount; ++i) {
        writeDecalQuad(&out[i * 6u], params.aItems[i], params.aAlphaScale);
    }
    vkUnmapMemory(GN_Vk.device, slot.aMemory);

    sDECAL_PUSH push = {};
    std::memcpy(push.aViewProj, params.aViewProj.m, sizeof(push.aViewProj));
    push.aParams[0] = params.aAlphaScale;

    VkCommandBuffer commandBuffer = GN_Vk.commandBuffers[GN_Vk.currentFrame];
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, s_pipeline);
    vkCmdPushConstants(commandBuffer, s_pipelineLayout,
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push),
                       &push);
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &slot.aBuffer, &offset);
    vkCmdDraw(commandBuffer, vertexCount, 1, 0, 0);
    return true;
}

void GN_VkDecalDestroy() {
    releaseAllSlots();
    if(GN_Vk.device != VK_NULL_HANDLE && s_pipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(GN_Vk.device, s_pipelineLayout, nullptr);
    }
    s_pipelineLayout = VK_NULL_HANDLE;
    s_pipeline = VK_NULL_HANDLE;
    s_slotFrame = 0xFFFFFFFFu;
    s_callInFrame = 0;
}

} // namespace GNg
