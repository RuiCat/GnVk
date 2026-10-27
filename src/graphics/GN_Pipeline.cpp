// 管线描述校验 / 内容键 / 内容相等：纯 CPU，不触碰 Vulkan 设备。
#include "GN_Pipeline.h"

#include <cstring>

namespace GN3d {
namespace {

const uint64_t FNV_OFFSET_BASIS = 14695981039346656037ULL;
const uint64_t FNV_PRIME = 1099511628211ULL;
const uint32_t MAX_COLOR_ATTACHMENTS = 8;

void MixU64(uint64_t &hash, uint64_t value) {
    for (int i = 0; i < 8; ++i) {
        hash ^= (value >> (i * 8)) & 0xFFULL;
        hash *= FNV_PRIME;
    }
}

// 浮点按位混入/比较：避免 -0.0 与 +0.0 在键与相等判定上给出不同结论。
void MixFloat(uint64_t &hash, float value) {
    uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    MixU64(hash, bits);
}

void MixI32(uint64_t &hash, int32_t value) {
    uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    MixU64(hash, bits);
}

bool SameFloat(float a, float b) {
    uint32_t aBits = 0;
    uint32_t bBits = 0;
    std::memcpy(&aBits, &a, sizeof(aBits));
    std::memcpy(&bBits, &b, sizeof(bBits));
    return aBits == bBits;
}

bool SameViewport(const VkViewport &a, const VkViewport &b) {
    return SameFloat(a.x, b.x) && SameFloat(a.y, b.y) && SameFloat(a.width, b.width) &&
           SameFloat(a.height, b.height) && SameFloat(a.minDepth, b.minDepth) &&
           SameFloat(a.maxDepth, b.maxDepth);
}

bool SameScissor(const VkRect2D &a, const VkRect2D &b) {
    return a.offset.x == b.offset.x && a.offset.y == b.offset.y &&
           a.extent.width == b.extent.width && a.extent.height == b.extent.height;
}

bool IsValidBlendFactor(VkBlendFactor factor) {
    return factor != VK_BLEND_FACTOR_MAX_ENUM && static_cast<int>(factor) >= 0;
}

void MixLayout(uint64_t &hash, const sVERTEX_LAYOUT &layout) {
    MixU64(hash, layout.aAttrCount);
    MixU64(hash, layout.aStride);
    const uint32_t count = layout.aAttrCount < sVERTEX_LAYOUT::MAX_ATTRS
                               ? layout.aAttrCount
                               : sVERTEX_LAYOUT::MAX_ATTRS;
    for (uint32_t i = 0; i < count; ++i) {
        MixU64(hash, layout.aAttrs[i].aLocation);
        MixU64(hash, layout.aAttrs[i].aOffset);
        MixU64(hash, static_cast<uint64_t>(layout.aAttrs[i].aFormat));
    }
}

uint32_t FormatByteSize(VkFormat format) {
    switch (format) {
    case VK_FORMAT_R32G32B32A32_SFLOAT:
    case VK_FORMAT_R32G32B32A32_SINT:
    case VK_FORMAT_R32G32B32A32_UINT:
        return 16;
    case VK_FORMAT_R32G32B32_SFLOAT:
    case VK_FORMAT_R32G32B32_SINT:
    case VK_FORMAT_R32G32B32_UINT:
        return 12;
    case VK_FORMAT_R32G32_SFLOAT:
    case VK_FORMAT_R32G32_SINT:
    case VK_FORMAT_R32G32_UINT:
        return 8;
    case VK_FORMAT_R32_SFLOAT:
    case VK_FORMAT_R32_SINT:
    case VK_FORMAT_R32_UINT:
        return 4;
    case VK_FORMAT_R16G16B16A16_SFLOAT:
    case VK_FORMAT_R16G16B16A16_SINT:
    case VK_FORMAT_R16G16B16A16_UINT:
        return 8;
    case VK_FORMAT_R16G16_SFLOAT:
    case VK_FORMAT_R16G16_SINT:
    case VK_FORMAT_R16G16_UINT:
        return 4;
    case VK_FORMAT_R16_SFLOAT:
    case VK_FORMAT_R16_SINT:
    case VK_FORMAT_R16_UINT:
        return 2;
    case VK_FORMAT_R8G8B8A8_UNORM:
    case VK_FORMAT_R8G8B8A8_SNORM:
    case VK_FORMAT_R8G8B8A8_SINT:
    case VK_FORMAT_R8G8B8A8_UINT:
        return 4;
    default:
        return 0; // 未知格式不推进 stride，由调用方显式设置 aStride
    }
}

bool SameBytes(const void *a, size_t aBytes, const void *b, size_t bBytes) {
    if (aBytes != bBytes) return false;
    if (aBytes == 0) return true;
    if (a == 0 || b == 0) return false;
    return std::memcmp(a, b, aBytes) == 0;
}

bool SameLayout(const sVERTEX_LAYOUT &a, const sVERTEX_LAYOUT &b) {
    if (a.aAttrCount != b.aAttrCount || a.aStride != b.aStride) return false;
    const uint32_t count = a.aAttrCount < sVERTEX_LAYOUT::MAX_ATTRS
                               ? a.aAttrCount
                               : sVERTEX_LAYOUT::MAX_ATTRS;
    for (uint32_t i = 0; i < count; ++i) {
        if (a.aAttrs[i].aLocation != b.aAttrs[i].aLocation) return false;
        if (a.aAttrs[i].aOffset != b.aAttrs[i].aOffset) return false;
        if (a.aAttrs[i].aFormat != b.aAttrs[i].aFormat) return false;
    }
    return true;
}

bool SameGraphicsState(const sPIPELINE_DESC &a, const sPIPELINE_DESC &b) {
    if (!SameLayout(a.aVertexLayout, b.aVertexLayout)) return false;
    if (a.aRenderPass != b.aRenderPass || a.aSubpass != b.aSubpass) return false;
    if (a.aDepthTest != b.aDepthTest || a.aDepthWrite != b.aDepthWrite) return false;
    if (a.aDepthCompare != b.aDepthCompare || a.aDepthStencil != b.aDepthStencil) return false;
    if (a.aDepthBiasEnable != b.aDepthBiasEnable) return false;
    if (!SameFloat(a.aDepthBiasConstant, b.aDepthBiasConstant)) return false;
    if (!SameFloat(a.aDepthBiasSlope, b.aDepthBiasSlope)) return false;
    if (a.aCullBack != b.aCullBack) return false;
    if (a.aFrontFace != b.aFrontFace || a.aBlend != b.aBlend) return false;
    if (a.aTopology != b.aTopology || a.aSamples != b.aSamples) return false;
    if (a.aColorAttachmentCount != b.aColorAttachmentCount) return false;
    if (a.aDynamicViewport != b.aDynamicViewport) return false;
    if (a.aDynamicScissor != b.aDynamicScissor) return false;
    if (!SameViewport(a.aViewport, b.aViewport)) return false;
    if (!SameScissor(a.aScissor, b.aScissor)) return false;
    if (a.aCustomBlend != b.aCustomBlend) return false;
    if (a.aBlendSrcColor != b.aBlendSrcColor || a.aBlendDstColor != b.aBlendDstColor) {
        return false;
    }
    if (a.aBlendSrcAlpha != b.aBlendSrcAlpha || a.aBlendDstAlpha != b.aBlendDstAlpha) {
        return false;
    }
    if (!SameBytes(a.aVertSpv, a.aVertSize, b.aVertSpv, b.aVertSize)) return false;
    return SameBytes(a.aFragSpv, a.aFragSize, b.aFragSpv, b.aFragSize);
}

} // namespace

sVERTEX_ATTR::sVERTEX_ATTR() : aLocation(0), aOffset(0), aFormat(VK_FORMAT_UNDEFINED) {}

sVERTEX_LAYOUT::sVERTEX_LAYOUT() : aAttrCount(0), aStride(0) {}

void sVERTEX_LAYOUT::Clear() {
    aAttrCount = 0;
    aStride = 0;
}

bool sVERTEX_LAYOUT::Add(uint32_t location, uint32_t offset, VkFormat format) {
    if (aAttrCount >= MAX_ATTRS) return false;
    if (format == VK_FORMAT_UNDEFINED) return false;
    aAttrs[aAttrCount].aLocation = location;
    aAttrs[aAttrCount].aOffset = offset;
    aAttrs[aAttrCount].aFormat = format;
    ++aAttrCount;
    const uint32_t bytes = FormatByteSize(format);
    if (bytes > 0 && offset + bytes > aStride) aStride = offset + bytes;
    return true;
}

sPIPELINE_DESC::sPIPELINE_DESC()
    : aKind(ePIPE_GRAPHICS),
      aVertSpv(0), aVertSize(0), aFragSpv(0), aFragSize(0), aCompSpv(0), aCompSize(0),
      aRenderPass(VK_NULL_HANDLE), aSubpass(0), aLayout(VK_NULL_HANDLE),
      aDepthTest(true), aDepthWrite(true), aDepthCompare(VK_COMPARE_OP_LESS),
      aDepthStencil(true),
      aDepthBiasEnable(false), aDepthBiasConstant(0.0f), aDepthBiasSlope(0.0f),
      aCullBack(true), aFrontFace(VK_FRONT_FACE_COUNTER_CLOCKWISE), aBlend(eBLEND_NONE),
      aTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST), aSamples(VK_SAMPLE_COUNT_1_BIT),
      aColorAttachmentCount(1),
      aDynamicViewport(true), aDynamicScissor(true),
      aViewport{0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f},
      aScissor{{0, 0}, {1, 1}},
      aCustomBlend(false),
      aBlendSrcColor(VK_BLEND_FACTOR_ONE), aBlendDstColor(VK_BLEND_FACTOR_ZERO),
      aBlendSrcAlpha(VK_BLEND_FACTOR_ONE), aBlendDstAlpha(VK_BLEND_FACTOR_ZERO) {}

bool ValidatePipelineDesc(const sPIPELINE_DESC &desc, const char **reason) {
    if (reason) *reason = 0;
    if (desc.aKind != ePIPE_GRAPHICS && desc.aKind != ePIPE_COMPUTE) {
        if (reason) *reason = "管线类型（aKind）无效";
        return false;
    }
    if (desc.aLayout == VK_NULL_HANDLE) {
        if (reason) *reason = "管线布局（aLayout）为空";
        return false;
    }

    if (desc.aKind == ePIPE_COMPUTE) {
        if (desc.aCompSpv == 0 || desc.aCompSize == 0) {
            if (reason) *reason = "计算管线缺少计算着色器 SPIR-V";
            return false;
        }
        if (!IsSpirv(desc.aCompSpv, desc.aCompSize)) {
            if (reason) *reason = "计算着色器不是有效的 SPIR-V";
            return false;
        }
        return true;
    }

    if (desc.aVertSpv == 0 || desc.aVertSize == 0) {
        if (reason) *reason = "图形管线缺少顶点着色器 SPIR-V";
        return false;
    }
    if (!IsSpirv(desc.aVertSpv, desc.aVertSize)) {
        if (reason) *reason = "顶点着色器不是有效的 SPIR-V";
        return false;
    }
    if (desc.aRenderPass == VK_NULL_HANDLE) {
        if (reason) *reason = "图形管线 renderPass 为空";
        return false;
    }
    if (desc.aColorAttachmentCount == 0) {
        // 深度专用（阴影 pass）：只有顶点阶段，subpass 固定 0。
        if (desc.aFragSpv != 0 || desc.aFragSize != 0) {
            if (reason) *reason = "深度专用管线（colorAttachmentCount=0）不能提供片段着色器";
            return false;
        }
        if (desc.aSubpass != 0) {
            if (reason) *reason = "深度专用管线 subpass 必须为 0";
            return false;
        }
    } else {
        if (desc.aFragSpv == 0 || desc.aFragSize == 0) {
            if (reason) *reason = "图形管线缺少片段着色器 SPIR-V";
            return false;
        }
        if (!IsSpirv(desc.aFragSpv, desc.aFragSize)) {
            if (reason) *reason = "片段着色器不是有效的 SPIR-V";
            return false;
        }
        if (desc.aColorAttachmentCount > MAX_COLOR_ATTACHMENTS) {
            if (reason) *reason = "图形管线 colorAttachmentCount 超过 8";
            return false;
        }
        if (desc.aSubpass >= desc.aColorAttachmentCount) {
            if (reason) *reason = "图形管线 subpass 超出附件范围";
            return false;
        }
    }
    const uint32_t sampleCount = static_cast<uint32_t>(desc.aSamples);
    if (sampleCount == 0 || (sampleCount & (sampleCount - 1)) != 0) {
        if (reason) *reason = "图形管线 aSamples 不是单个采样数位";
        return false;
    }
    if (!desc.aDynamicViewport) {
        if (!(desc.aViewport.width > 0.0f)) {
            if (reason) *reason = "固定视口 width 必须大于 0";
            return false;
        }
        if (!(desc.aViewport.height != 0.0f)) {
            if (reason) *reason = "固定视口 height 不能为 0";
            return false;
        }
    }
    if (!desc.aDynamicScissor) {
        if (desc.aScissor.extent.width == 0 || desc.aScissor.extent.height == 0) {
            if (reason) *reason = "固定 scissor 的 extent 宽高必须大于 0";
            return false;
        }
    }
    if (desc.aCustomBlend) {
        if (!IsValidBlendFactor(desc.aBlendSrcColor) ||
            !IsValidBlendFactor(desc.aBlendDstColor) ||
            !IsValidBlendFactor(desc.aBlendSrcAlpha) ||
            !IsValidBlendFactor(desc.aBlendDstAlpha)) {
            if (reason) *reason = "自定义混合因子不是合法的 VkBlendFactor";
            return false;
        }
    }
    return true;
}

uint64_t ComputePipelineKey(const sPIPELINE_DESC &desc) {
    uint64_t hash = FNV_OFFSET_BASIS;
    MixU64(hash, static_cast<uint64_t>(desc.aKind));
    MixU64(hash, (uint64_t)(desc.aLayout));

    if (desc.aKind == ePIPE_COMPUTE) {
        MixU64(hash, static_cast<uint64_t>(desc.aCompSize));
        MixU64(hash, HashSpirv(desc.aCompSpv, desc.aCompSize));
        return hash;
    }

    MixU64(hash, static_cast<uint64_t>(desc.aVertSize));
    MixU64(hash, HashSpirv(desc.aVertSpv, desc.aVertSize));
    MixU64(hash, static_cast<uint64_t>(desc.aFragSize));
    MixU64(hash, HashSpirv(desc.aFragSpv, desc.aFragSize));
    MixLayout(hash, desc.aVertexLayout);
    MixU64(hash, (uint64_t)(desc.aRenderPass));
    MixU64(hash, static_cast<uint64_t>(desc.aSubpass));
    MixU64(hash, static_cast<uint64_t>(desc.aDepthTest));
    MixU64(hash, static_cast<uint64_t>(desc.aDepthWrite));
    MixU64(hash, static_cast<uint64_t>(desc.aDepthCompare));
    MixU64(hash, static_cast<uint64_t>(desc.aDepthStencil));
    MixU64(hash, static_cast<uint64_t>(desc.aDepthBiasEnable));
    MixFloat(hash, desc.aDepthBiasConstant);
    MixFloat(hash, desc.aDepthBiasSlope);
    MixU64(hash, static_cast<uint64_t>(desc.aCullBack));
    MixU64(hash, static_cast<uint64_t>(desc.aFrontFace));
    MixU64(hash, static_cast<uint64_t>(desc.aBlend));
    MixU64(hash, static_cast<uint64_t>(desc.aTopology));
    MixU64(hash, static_cast<uint64_t>(desc.aSamples));
    MixU64(hash, static_cast<uint64_t>(desc.aColorAttachmentCount));
    MixU64(hash, static_cast<uint64_t>(desc.aDynamicViewport));
    MixU64(hash, static_cast<uint64_t>(desc.aDynamicScissor));
    MixFloat(hash, desc.aViewport.x);
    MixFloat(hash, desc.aViewport.y);
    MixFloat(hash, desc.aViewport.width);
    MixFloat(hash, desc.aViewport.height);
    MixFloat(hash, desc.aViewport.minDepth);
    MixFloat(hash, desc.aViewport.maxDepth);
    MixI32(hash, desc.aScissor.offset.x);
    MixI32(hash, desc.aScissor.offset.y);
    MixU64(hash, static_cast<uint64_t>(desc.aScissor.extent.width));
    MixU64(hash, static_cast<uint64_t>(desc.aScissor.extent.height));
    MixU64(hash, static_cast<uint64_t>(desc.aCustomBlend));
    MixU64(hash, static_cast<uint64_t>(desc.aBlendSrcColor));
    MixU64(hash, static_cast<uint64_t>(desc.aBlendDstColor));
    MixU64(hash, static_cast<uint64_t>(desc.aBlendSrcAlpha));
    MixU64(hash, static_cast<uint64_t>(desc.aBlendDstAlpha));
    return hash;
}

bool PipelineDescEqual(const sPIPELINE_DESC &a, const sPIPELINE_DESC &b) {
    if (a.aKind != b.aKind || a.aLayout != b.aLayout) return false;
    if (a.aKind == ePIPE_COMPUTE) {
        return SameBytes(a.aCompSpv, a.aCompSize, b.aCompSpv, b.aCompSize);
    }
    return SameGraphicsState(a, b);
}

} // namespace GN3d
