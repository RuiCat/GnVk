#pragma once
#ifndef _GN_PIPELINE_H_
#define _GN_PIPELINE_H_
// ╔══════════════════════════════════════════════════════════╗
// ║  GN_mini — 通用图形/计算管线描述与缓存                    ║
// ║  显式描述（不引 SPIRV-Cross），按内容哈希复用 VkPipeline   ║
// ╚══════════════════════════════════════════════════════════╝
//
// 契约（冻结）：
//   * sPIPELINE_DESC 是管线的唯一描述来源；缓存键 = 描述字段 + SPIR-V 内容哈希；
//   * 同一键只创建一次 VkPipeline，destroy 归 cPIPELINE_CACHE 所有；
//   * 计算管线忽略图形相关字段；图形管线缺 VS/FS 视为无效描述；
//   * Validate / ComputePipelineKey / PipelineDescEqual 不触碰 Vulkan 设备，可无 GPU 单测。

#include "GN_Shader.h"
#include <vulkan/vulkan.h>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace GN3d {

enum ePIPELINE_KIND {
    ePIPE_GRAPHICS = 0,
    ePIPE_COMPUTE = 1
};

enum ePIPELINE_BLEND {
    eBLEND_NONE = 0, // 不混合
    eBLEND_ALPHA = 1, // 标准 alpha 混合
    eBLEND_ADDITIVE = 2, // 加法混合
    eBLEND_MULTIPLY = 3 // 乘法混合
};

// 单 binding 交错顶点属性；location 与着色器 layout(location=?) 对应
struct sVERTEX_ATTR {
    uint32_t aLocation;
    uint32_t aOffset;
    VkFormat aFormat;

    sVERTEX_ATTR();
};

struct sVERTEX_LAYOUT {
    static const uint32_t MAX_ATTRS = 16;

    sVERTEX_ATTR aAttrs[MAX_ATTRS];
    uint32_t aAttrCount;
    uint32_t aStride;

    sVERTEX_LAYOUT();
    void Clear();
    // 追加一个属性；aStride 由已知 VkFormat 的字节宽自动推进（未知格式不推进），
    // 调用方可在 Add 之后显式覆盖 aStride（例如压缩/交错布局）。失败返回 false。
    bool Add(uint32_t location, uint32_t offset, VkFormat format);
};

// ═══════════════════════════════════════════════════════════
//  sPIPELINE_DESC — 一条管线的完整描述
// ═══════════════════════════════════════════════════════════
struct sPIPELINE_DESC {
    ePIPELINE_KIND aKind;

    // 着色器：指向调用方持有的 SPIR-V 字节，只在缓存创建时读取
    const void *aVertSpv;
    size_t aVertSize;
    const void *aFragSpv;
    size_t aFragSize;
    const void *aCompSpv; // 计算管线用
    size_t aCompSize;

    sVERTEX_LAYOUT aVertexLayout;
    VkRenderPass aRenderPass;
    uint32_t aSubpass;
    VkPipelineLayout aLayout;

    bool aDepthTest;
    bool aDepthWrite;
    VkCompareOp aDepthCompare;
    // true（默认）= 提供深度/模板状态；false = pDepthStencilState 传空
    // （无深度附件的 renderPass，如 2D，必须为 false，否则违反 Vulkan 校验）。
    bool aDepthStencil;
    // 深度偏移（阴影 pass 用；aDepthBiasEnable=false 时两个因子被忽略）
    bool aDepthBiasEnable;
    float aDepthBiasConstant;
    float aDepthBiasSlope;
    bool aCullBack;
    VkFrontFace aFrontFace;
    ePIPELINE_BLEND aBlend;
    VkPrimitiveTopology aTopology;
    VkSampleCountFlagBits aSamples;
    // 颜色附件数：0 表示**深度专用**管线（阴影 pass）：不接受片元着色器，颜色混合状态为空
    uint32_t aColorAttachmentCount;

    // viewport / scissor：动态（命令缓冲设置）或固定（用下面的值）。
    // 固定视口用于既有 2D/3D 管线（含 PBR 的 Y 翻转负高度视口），迁移期必须逐字段一致。
    bool aDynamicViewport;
    bool aDynamicScissor;
    VkViewport aViewport;
    VkRect2D aScissor;

    // aCustomBlend = true 时用下面四个因子（精确复刻既有混合状态），否则按 aBlend 预设。
    bool aCustomBlend;
    VkBlendFactor aBlendSrcColor;
    VkBlendFactor aBlendDstColor;
    VkBlendFactor aBlendSrcAlpha;
    VkBlendFactor aBlendDstAlpha;

    sPIPELINE_DESC(); // 默认：图形管线 + 深度测试 + 背面剔除 + 无混合 + 三角形列表 + 动态 viewport/scissor
};

// 无效描述返回 false，并把原因写入 reason（可为 0）。
// 除缺顶点着色器/layout/renderPass 外，以下也判无效：着色器无 SPIR-V 魔数、
// colorAttachmentCount 为 0 且提供了片元着色器（深度专用不应有 FS）、colorAttachmentCount >8、
// 有颜色附件时 subpass >= colorAttachmentCount、aSamples 不是单个采样数位。
// 深度专用（colorAttachmentCount == 0）时：只创建顶点阶段，颜色混合状态 attachmentCount = 0，
// subpass 必须为 0。
bool ValidatePipelineDesc(const sPIPELINE_DESC &desc, const char **reason = 0);

// 缓存键：所有描述字段 + SPIR-V 内容（不依赖指针值）
uint64_t ComputePipelineKey(const sPIPELINE_DESC &desc);

// 描述相等（内容比较，不含指针本身；SPIR-V 按字节比较）
bool PipelineDescEqual(const sPIPELINE_DESC &a, const sPIPELINE_DESC &b);

// ═══════════════════════════════════════════════════════════
//  cPIPELINE_CACHE — VkPipeline 缓存
// ═══════════════════════════════════════════════════════════
class cPIPELINE_CACHE {
  public:
    cPIPELINE_CACHE();
    ~cPIPELINE_CACHE();

    void Init(VkDevice device);
    void Release(); // 销毁全部管线与模块；可重复调用

    // 命中返回已有管线；未命中则创建。失败返回 VK_NULL_HANDLE，原因见 GetLastError()。
    // 命中判据是 ComputePipelineKey 的 64 位内容键，**不做**描述二次比较（碰撞为理论风险）；
    // 管线生命周期归缓存所有，须在 VkDevice 销毁前 Release/析构。
    VkPipeline GetOrCreate(const sPIPELINE_DESC &desc, bool *created = 0);

    uint32_t GetCount() const { return (uint32_t)aEntries.size(); }
    uint64_t GetHitCount() const { return aHits; }
    uint64_t GetMissCount() const { return aMisses; }
    const char *GetLastError() const { return aLastError; }

  private:
    struct sENTRY {
        uint64_t aKey;
        VkPipeline aPipeline;
    };

    std::vector<sENTRY> aEntries;
    cSHADER_CACHE aShaders;
    VkDevice aDevice;
    uint64_t aHits;
    uint64_t aMisses;
    const char *aLastError;
};

} // namespace GN3d
#endif // _GN_PIPELINE_H_
