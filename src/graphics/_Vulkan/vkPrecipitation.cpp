// vkPrecipitation.cpp — P11-S5 降水：CPU 展开的软边 billboard（雨丝 / 雪团）。
//
// 契约（GN_Vk.h 冻结）：
//   * GN_VkDrawPrecipitation：世界空间 billboard，alpha 混合、深度测试开、深度写关、不剔除；
//     参数非法 / 未初始化 / 3D pass 外返回 false。
//   * GN_VkPrecipitationDestroy：释放顶点缓冲与管线布局；可重复调用（管线归 aPipelineCache）。
//
// 形状（片元按 aStreakScale 选模式，避免"实心长方形面片"）：
//   * aStreakScale > 1.5（雨）：细芯 + 头部亮、尾部淡的雨丝，沿**粒子速度方向**取向（有风会斜），
//     侧向宽度只有中心 15% 是全亮，两端做 smoothstep 渐隐。
//   * aStreakScale <= 1.5（雪）：以 uv 中心为圆心的径向软斑，不再是方形。
//
// 顶点缓冲：4 个槽位 = (currentFrame % 2) × 帧内调用序号(0..1)。
//   * 同帧可安全提交两类降水（雨 + 雪各一次），不再需要调用方合并；
//   * 槽位按帧奇偶隔离，BeginFrame 对该帧 fence 的等待保证上一轮同槽位数据已用完。
#include <GN.h>
#include "GN_Vk.h"
#include "vkInternal.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace GNg {
namespace {

// 内部顶点：世界坐标 + 形状 uv + 展开后的 RGBA（与 precipitation.vert 的 location 0/1/2 对应）。
struct sPRECIP_VERTEX {
    float aPosition[3];
    float aUv[2];
    float aColor[4];
};

// push constant：viewProj(64) + params(16)：x = streakScale, y = alphaScale。
struct sPRECIP_PUSH {
    float aViewProj[16];
    float aParams[4];
};

const uint32_t kSlotCount = 4; // (帧奇偶 2) × (帧内最多 2 次调用)
const float kMinDirectionLengthSq = 1.0e-8f;

struct sVERTEX_SLOT {
    VkBuffer aBuffer;
    VkDeviceMemory aMemory;
    uint32_t aCapacity; // 顶点个数（非字节）
};

sVERTEX_SLOT s_slots[kSlotCount];
VkPipelineLayout s_pipelineLayout = VK_NULL_HANDLE;
VkPipeline s_pipeline = VK_NULL_HANDLE;
uint32_t s_slotFrame = 0xFFFFFFFFu; // 上一次绘制时的 GN_Vk.currentFrame
uint32_t s_callInFrame = 0; // 本帧内第几次调用（0..kSlotCount/2-1）

// 四边形四角在 (侧向, 沿速度) 上的系数：沿速度 0 = 头部（亮），1 = 尾部（淡）。
const int32_t kCornerSide[4] = {-1, 1, 1, -1};
const int32_t kCornerAlong[4] = {0, 0, 1, 1};
const uint32_t kQuadOrder[6] = {0, 1, 2, 0, 2, 3};
const uint32_t kInitialVertexCapacity = 1024;

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

void makeVec(float x, float y, float z, GNg::sVEC3 &out) {
    out.x = x;
    out.y = y;
    out.z = z;
}

float lengthSq(const GNg::sVEC3 &v) {
    return v.x * v.x + v.y * v.y + v.z * v.z;
}

void crossVec(const GNg::sVEC3 &a, const GNg::sVEC3 &b, GNg::sVEC3 &out) {
    makeVec(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x, out);
}

bool normalizeVec(const GNg::sVEC3 &v, GNg::sVEC3 &out) {
    const float lenSq = lengthSq(v);
    if(!(lenSq > kMinDirectionLengthSq)) return false;
    const float inv = 1.0f / std::sqrt(lenSq);
    makeVec(v.x * inv, v.y * inv, v.z * inv, out);
    return true;
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
    CreateBuf((VkDeviceSize)newCapacity * sizeof(sPRECIP_VERTEX), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
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
    vertexLayout.Add(0, (uint32_t)offsetof(sPRECIP_VERTEX, aPosition), VK_FORMAT_R32G32B32_SFLOAT);
    vertexLayout.Add(1, (uint32_t)offsetof(sPRECIP_VERTEX, aUv), VK_FORMAT_R32G32_SFLOAT);
    vertexLayout.Add(2, (uint32_t)offsetof(sPRECIP_VERTEX, aColor), VK_FORMAT_R32G32B32A32_SFLOAT);
    vertexLayout.aStride = (uint32_t)sizeof(sPRECIP_VERTEX);

    GN3d::sPIPELINE_DESC desc;
    desc.aKind = GN3d::ePIPE_GRAPHICS;
    desc.aVertexLayout = vertexLayout;
    desc.aRenderPass = GN_Vk.renderPass3D;
    desc.aSubpass = 0;
    desc.aLayout = s_pipelineLayout;
    desc.aDepthTest = true; // 与场景几何做深度比较
    desc.aDepthWrite = false; // 半透明雨雪不写深度
    desc.aDepthCompare = VK_COMPARE_OP_LESS;
    desc.aDepthStencil = true;
    desc.aCullBack = false; // billboard 双面可见
    desc.aFrontFace = VK_FRONT_FACE_CLOCKWISE; // 世界 CCW + Y 翻转
    desc.aBlend = GN3d::eBLEND_ALPHA;
    desc.aTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    desc.aSamples = GN_Vk.msaaSamples;
    desc.aColorAttachmentCount = 1;
    desc.aDynamicViewport = false;
    desc.aDynamicScissor = false;
    // 与场景一致的正高度视口：竖直方向由投影矩阵决定；负高度视口会让雨雪上下颠倒。
    desc.aViewport = VkViewport{0.0f, 0.0f, (float)GN_Vk.swapchainExtent.width,
                                (float)GN_Vk.swapchainExtent.height, 0.0f, 1.0f};
    desc.aScissor = VkRect2D{{0, 0}, GN_Vk.swapchainExtent};
    desc.aCustomBlend = false;
    return desc;
}

// 惰性创建 pipelineLayout + 管线（管线归 aPipelineCache，布局归本模块）。
bool createPipeline() {
    // 只查 device / renderPass3D，不依赖 GN_Vk.initialized 的置位时机。
    if(GN_Vk.device == VK_NULL_HANDLE || GN_Vk.renderPass3D == VK_NULL_HANDLE) return false;

    std::vector<uint32_t> vertSpv = LoadSPIRVFile("assets/shaders/precipitation.vert.spv");
    std::vector<uint32_t> fragSpv = LoadSPIRVFile("assets/shaders/precipitation.frag.spv");
    if(vertSpv.empty() || fragSpv.empty()) return false;

    VkPushConstantRange pushRange = {};
    pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushRange.offset = 0;
    pushRange.size = sizeof(sPRECIP_PUSH);
    VkPipelineLayoutCreateInfo layoutInfo = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    layoutInfo.setLayoutCount = 0; // 无描述符集：输入全部走顶点属性 + push constant
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

// 一颗粒子写成 6 个顶点：沿速度方向拉长、侧向展开，uv 供片元做软边。
void writeParticleQuad(sPRECIP_VERTEX *out, const GN3d::sPARTICLE &particle,
                       const sPRECIP_DRAW_PARAMS &params, const GNg::sVEC3 &forward) {
    GNg::sVEC3 streakDir;
    if(!normalizeVec(particle.aVelocity, streakDir)) {
        streakDir = params.aCameraUp; // 静止粒子：屏幕竖直
        if(!normalizeVec(streakDir, streakDir)) makeVec(0.0f, 1.0f, 0.0f, streakDir);
    }
    GNg::sVEC3 side;
    crossVec(streakDir, forward, side);
    if(!normalizeVec(side, side)) side = params.aCameraRight; // 正对视线：退回相机右向

    const float halfWidth = particle.aSize * 0.5f;
    const float halfLength = halfWidth * params.aStreakScale;
    const float alpha = particle.aColor.w * params.aAlphaScale;
    for(uint32_t i = 0; i < 6; ++i) {
        const uint32_t corner = kQuadOrder[i];
        const float sideSign = (float)kCornerSide[corner];
        const float alongSign = 1.0f - 2.0f * (float)kCornerAlong[corner]; // 头 +1 / 尾 -1
        out[i].aPosition[0] = particle.aPosition.x + side.x * sideSign * halfWidth +
                              streakDir.x * alongSign * halfLength;
        out[i].aPosition[1] = particle.aPosition.y + side.y * sideSign * halfWidth +
                              streakDir.y * alongSign * halfLength;
        out[i].aPosition[2] = particle.aPosition.z + side.z * sideSign * halfWidth +
                              streakDir.z * alongSign * halfLength;
        out[i].aUv[0] = sideSign * 0.5f + 0.5f;
        out[i].aUv[1] = (float)kCornerAlong[corner];
        out[i].aColor[0] = particle.aColor.x;
        out[i].aColor[1] = particle.aColor.y;
        out[i].aColor[2] = particle.aColor.z;
        out[i].aColor[3] = alpha;
    }
}

// 本帧内的调用槽位：帧奇偶 → 半区，帧内序号 → 区内槽位；同帧超过 2 次调用时轮转复用。
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

// 在已开启的 3D pass 内提交 aCount 个 billboard；pass 外/未初始化/参数非法返回 false。
bool GN_VkDrawPrecipitation(const sPRECIP_DRAW_PARAMS &params) {
    if(!GN_Vk.initialized || !s_in3DPass) return false;
    if(params.aCount == 0 || params.aParticles == nullptr) return false;
    if(!(params.aStreakScale > 0.0f)) return false;
    if(params.aCount > 0xFFFFFFFFu / 6u) return false; // 顶点数溢出
    if(s_pipeline == VK_NULL_HANDLE && !createPipeline()) return false;

    const uint32_t slotIndex = acquireSlot();
    sVERTEX_SLOT &slot = s_slots[slotIndex];
    const uint32_t vertexCount = params.aCount * 6u;
    if(!ensureSlotCapacity(slot, vertexCount)) return false;

    GNg::sVEC3 forward;
    crossVec(params.aCameraRight, params.aCameraUp, forward);
    if(!normalizeVec(forward, forward)) makeVec(0.0f, 0.0f, -1.0f, forward);

    const VkDeviceSize byteSize = (VkDeviceSize)vertexCount * sizeof(sPRECIP_VERTEX);
    void *mapped = nullptr;
    if(vkMapMemory(GN_Vk.device, slot.aMemory, 0, byteSize, 0, &mapped) != VK_SUCCESS ||
       mapped == nullptr) {
        return false;
    }
    sPRECIP_VERTEX *out = (sPRECIP_VERTEX *)mapped;
    for(uint32_t i = 0; i < params.aCount; ++i) {
        writeParticleQuad(&out[i * 6u], params.aParticles[i], params, forward);
    }
    vkUnmapMemory(GN_Vk.device, slot.aMemory);

    sPRECIP_PUSH push = {};
    std::memcpy(push.aViewProj, params.aViewProj.m, sizeof(push.aViewProj));
    push.aParams[0] = params.aStreakScale;
    push.aParams[1] = params.aAlphaScale;
    push.aParams[2] = 0.0f;
    push.aParams[3] = 0.0f;

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

// 释放四个槽位的顶点缓冲与 pipelineLayout；可重复调用（管线归 aPipelineCache）。
void GN_VkPrecipitationDestroy() {
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
