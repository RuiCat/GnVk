// VkPipeline 缓存：图形/计算管线的创建、按内容键复用与销毁，内部复用 cSHADER_CACHE。
#include "GN_Pipeline.h"

#include <cstring>

namespace GN3d {
namespace {

const uint32_t MAX_COLOR_ATTACHMENTS = 8;

// 自定义因子优先：既有 2D 的 alpha 因子是 ONE/ZERO，必须能精确复刻而不是走 eBLEND_ALPHA 预设。
void FillBlendAttachment(const sPIPELINE_DESC &desc, VkPipelineColorBlendAttachmentState &out) {
    out = VkPipelineColorBlendAttachmentState();
    out.colorWriteMask = 0xF;
    if (desc.aCustomBlend) {
        out.blendEnable = VK_TRUE;
        out.srcColorBlendFactor = desc.aBlendSrcColor;
        out.dstColorBlendFactor = desc.aBlendDstColor;
        out.colorBlendOp = VK_BLEND_OP_ADD;
        out.srcAlphaBlendFactor = desc.aBlendSrcAlpha;
        out.dstAlphaBlendFactor = desc.aBlendDstAlpha;
        out.alphaBlendOp = VK_BLEND_OP_ADD;
        return;
    }
    switch (desc.aBlend) {
    case eBLEND_ALPHA:
        out.blendEnable = VK_TRUE;
        out.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        out.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        out.colorBlendOp = VK_BLEND_OP_ADD;
        out.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        out.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        out.alphaBlendOp = VK_BLEND_OP_ADD;
        break;
    case eBLEND_ADDITIVE:
        out.blendEnable = VK_TRUE;
        out.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
        out.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
        out.colorBlendOp = VK_BLEND_OP_ADD;
        out.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        out.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        out.alphaBlendOp = VK_BLEND_OP_ADD;
        break;
    case eBLEND_MULTIPLY:
        out.blendEnable = VK_TRUE;
        out.srcColorBlendFactor = VK_BLEND_FACTOR_DST_COLOR;
        out.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
        out.colorBlendOp = VK_BLEND_OP_ADD;
        out.srcAlphaBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;
        out.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        out.alphaBlendOp = VK_BLEND_OP_ADD;
        break;
    case eBLEND_NONE:
    default:
        out.blendEnable = VK_FALSE;
        break;
    }
}

void FillVertexInput(const sVERTEX_LAYOUT &layout, VkVertexInputBindingDescription &binding,
                     VkVertexInputAttributeDescription *attrs,
                     VkPipelineVertexInputStateCreateInfo &info) {
    info = VkPipelineVertexInputStateCreateInfo();
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    if (layout.aAttrCount == 0) return;

    binding.binding = 0;
    binding.stride = layout.aStride;
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    const uint32_t count = layout.aAttrCount < sVERTEX_LAYOUT::MAX_ATTRS
                               ? layout.aAttrCount
                               : sVERTEX_LAYOUT::MAX_ATTRS;
    for (uint32_t i = 0; i < count; ++i) {
        attrs[i].location = layout.aAttrs[i].aLocation;
        attrs[i].binding = 0;
        attrs[i].format = layout.aAttrs[i].aFormat;
        attrs[i].offset = layout.aAttrs[i].aOffset;
    }
    info.vertexBindingDescriptionCount = 1;
    info.pVertexBindingDescriptions = &binding;
    info.vertexAttributeDescriptionCount = count;
    info.pVertexAttributeDescriptions = attrs;
}

VkPipeline CreateGraphicsPipeline(VkDevice device, const sPIPELINE_DESC &desc,
                                  VkShaderModule vertModule, VkShaderModule fragModule,
                                  const char **error) {
    // 深度专用管线（colorAttachmentCount=0）没有片元模块，只提交顶点阶段。
    VkPipelineShaderStageCreateInfo stages[2] = {};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vertModule;
    stages[0].pName = "main";
    uint32_t stageCount = 1;
    if (fragModule != VK_NULL_HANDLE) {
        stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        stages[1].module = fragModule;
        stages[1].pName = "main";
        stageCount = 2;
    }

    VkVertexInputBindingDescription binding = {};
    VkVertexInputAttributeDescription attrs[sVERTEX_LAYOUT::MAX_ATTRS];
    std::memset(attrs, 0, sizeof(attrs));
    VkPipelineVertexInputStateCreateInfo vertexInput = {};
    FillVertexInput(desc.aVertexLayout, binding, attrs, vertexInput);

    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = desc.aTopology;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    // 固定态用描述里的 viewport/scissor（各 3D 管线统一为正高度全屏视口，竖直方向交给投影矩阵）；
    // 动态态下这两个指针被 VK_DYNAMIC_STATE_* 忽略，但仍提供合法内存。
    VkPipelineViewportStateCreateInfo viewportState = {};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &desc.aViewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &desc.aScissor;

    VkDynamicState dynamicStates[2];
    uint32_t dynamicStateCount = 0;
    if (desc.aDynamicViewport) dynamicStates[dynamicStateCount++] = VK_DYNAMIC_STATE_VIEWPORT;
    if (desc.aDynamicScissor) dynamicStates[dynamicStateCount++] = VK_DYNAMIC_STATE_SCISSOR;
    VkPipelineDynamicStateCreateInfo dynamicState = {};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = dynamicStateCount;
    dynamicState.pDynamicStates = dynamicStateCount > 0 ? dynamicStates : 0;

    VkPipelineRasterizationStateCreateInfo raster = {};
    raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    raster.polygonMode = VK_POLYGON_MODE_FILL;
    raster.cullMode = desc.aCullBack ? VK_CULL_MODE_BACK_BIT : VK_CULL_MODE_NONE;
    raster.frontFace = desc.aFrontFace;
    raster.lineWidth = 1.0f;
    raster.depthBiasEnable = desc.aDepthBiasEnable ? VK_TRUE : VK_FALSE;
    raster.depthBiasConstantFactor = desc.aDepthBiasConstant;
    raster.depthBiasSlopeFactor = desc.aDepthBiasSlope;

    VkPipelineMultisampleStateCreateInfo multisample = {};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = desc.aSamples;

    VkPipelineDepthStencilStateCreateInfo depthStencil = {};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = desc.aDepthTest ? VK_TRUE : VK_FALSE;
    depthStencil.depthWriteEnable = desc.aDepthWrite ? VK_TRUE : VK_FALSE;
    depthStencil.depthCompareOp = desc.aDepthCompare;

    VkPipelineColorBlendAttachmentState blendAttachments[MAX_COLOR_ATTACHMENTS];
    const uint32_t attachmentCount = desc.aColorAttachmentCount < MAX_COLOR_ATTACHMENTS
                                         ? desc.aColorAttachmentCount
                                         : MAX_COLOR_ATTACHMENTS;
    for (uint32_t i = 0; i < attachmentCount; ++i) {
        FillBlendAttachment(desc, blendAttachments[i]);
    }
    VkPipelineColorBlendStateCreateInfo colorBlend = {};
    colorBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlend.attachmentCount = attachmentCount;
    colorBlend.pAttachments = attachmentCount > 0 ? blendAttachments : 0;

    VkGraphicsPipelineCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    info.stageCount = stageCount;
    info.pStages = stages;
    info.pVertexInputState = &vertexInput;
    info.pInputAssemblyState = &inputAssembly;
    info.pViewportState = &viewportState;
    info.pRasterizationState = &raster;
    info.pMultisampleState = &multisample;
    // aDepthStencil=false（2D 所在 renderPass 无深度附件）时不提供深度/模板状态。
    info.pDepthStencilState = desc.aDepthStencil ? &depthStencil : 0;
    info.pColorBlendState = &colorBlend;
    info.pDynamicState = &dynamicState;
    info.layout = desc.aLayout;
    info.renderPass = desc.aRenderPass;
    info.subpass = desc.aSubpass;

    VkPipeline pipeline = VK_NULL_HANDLE;
    const VkResult result =
        vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &info, 0, &pipeline);
    if (result != VK_SUCCESS) {
        if (error) *error = "vkCreateGraphicsPipelines 失败";
        return VK_NULL_HANDLE;
    }
    return pipeline;
}

VkPipeline CreateComputePipeline(VkDevice device, const sPIPELINE_DESC &desc,
                                 VkShaderModule compModule, const char **error) {
    VkComputePipelineCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    info.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    info.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    info.stage.module = compModule;
    info.stage.pName = "main";
    info.layout = desc.aLayout;

    VkPipeline pipeline = VK_NULL_HANDLE;
    const VkResult result =
        vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &info, 0, &pipeline);
    if (result != VK_SUCCESS) {
        if (error) *error = "vkCreateComputePipelines 失败";
        return VK_NULL_HANDLE;
    }
    return pipeline;
}

} // namespace

cPIPELINE_CACHE::cPIPELINE_CACHE()
    : aDevice(VK_NULL_HANDLE), aHits(0), aMisses(0), aLastError("") {}

// 析构即 Release：调用方必须在销毁 VkDevice 之前销毁缓存（或提前显式 Release）。
cPIPELINE_CACHE::~cPIPELINE_CACHE() { Release(); }

void cPIPELINE_CACHE::Init(VkDevice device) {
    Release();
    aDevice = device;
    aShaders.Init(device);
    aHits = 0;
    aMisses = 0;
    aLastError = "";
}

void cPIPELINE_CACHE::Release() {
    if (aDevice != VK_NULL_HANDLE) {
        for (size_t i = 0; i < aEntries.size(); ++i) {
            if (aEntries[i].aPipeline != VK_NULL_HANDLE) {
                vkDestroyPipeline(aDevice, aEntries[i].aPipeline, 0);
            }
        }
    }
    aEntries.clear();
    aShaders.Release();
    aDevice = VK_NULL_HANDLE;
    aHits = 0;
    aMisses = 0;
    aLastError = "";
}

VkPipeline cPIPELINE_CACHE::GetOrCreate(const sPIPELINE_DESC &desc, bool *created) {
    if (created) *created = false;
    if (aDevice == VK_NULL_HANDLE) {
        aLastError = "管线缓存未 Init 或已 Release（VkDevice 为空）";
        return VK_NULL_HANDLE;
    }

    const char *reason = 0;
    if (!ValidatePipelineDesc(desc, &reason)) {
        aLastError = reason ? reason : "管线描述无效";
        return VK_NULL_HANDLE;
    }

    const uint64_t key = ComputePipelineKey(desc);
    for (size_t i = 0; i < aEntries.size(); ++i) {
        if (aEntries[i].aKey == key) {
            ++aHits;
            aLastError = "";
            return aEntries[i].aPipeline;
        }
    }

    ++aMisses;
    aLastError = "";
    VkPipeline pipeline = VK_NULL_HANDLE;
    if (desc.aKind == ePIPE_COMPUTE) {
        VkShaderModule compModule = aShaders.GetOrCreate(desc.aCompSpv, desc.aCompSize);
        if (compModule == VK_NULL_HANDLE) {
            aLastError = "计算着色器模块创建失败";
            return VK_NULL_HANDLE;
        }
        const char *createError = 0;
        pipeline = CreateComputePipeline(aDevice, desc, compModule, &createError);
        if (pipeline == VK_NULL_HANDLE) {
            aLastError = createError ? createError : "计算管线创建失败";
            return VK_NULL_HANDLE;
        }
    } else {
        VkShaderModule vertModule = aShaders.GetOrCreate(desc.aVertSpv, desc.aVertSize);
        if (vertModule == VK_NULL_HANDLE) {
            aLastError = "顶点着色器模块创建失败";
            return VK_NULL_HANDLE;
        }
        VkShaderModule fragModule = VK_NULL_HANDLE;
        if (desc.aFragSpv != 0 && desc.aFragSize != 0) {
            fragModule = aShaders.GetOrCreate(desc.aFragSpv, desc.aFragSize);
            if (fragModule == VK_NULL_HANDLE) {
                aLastError = "片段着色器模块创建失败";
                return VK_NULL_HANDLE;
            }
        }
        const char *createError = 0;
        pipeline = CreateGraphicsPipeline(aDevice, desc, vertModule, fragModule, &createError);
        if (pipeline == VK_NULL_HANDLE) {
            aLastError = createError ? createError : "图形管线创建失败";
            return VK_NULL_HANDLE;
        }
    }

    sENTRY entry;
    entry.aKey = key;
    entry.aPipeline = pipeline;
    aEntries.push_back(entry);
    if (created) *created = true;
    return pipeline;
}

} // namespace GN3d
