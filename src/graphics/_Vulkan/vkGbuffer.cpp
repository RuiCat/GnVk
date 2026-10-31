// G-Buffer 资源：附件枚举映射、图像/视图/内存、render pass（含显式 subpass 依赖）、
// framebuffer、描述符与管线的创建/销毁，以及 resize 时对尺寸相关部分的重建。
#include <GN.h>
#include "GN_Vk.h"
#include "vkInternal.h"

#include <cstdio>

namespace GNg {

// 三个颜色附件的格式（与 gn3d_gbuffer.frag 的 MRT 输出一一对应）；深度用设备选定格式。
// G2a：G-Buffer 打包 24→16 B/px（保守方案，docs/GI_PLAN.md §5.2）。
// 附件 0 保持 RGBA16F（法线精度关键）；附件 1/2 改 RGBA8（各 4 B/px），深度不变。
static const VkFormat sGBufferColorFormats[3] = {
    VK_FORMAT_R16G16B16A16_SFLOAT, // 附件 0：Normal(RGB) + Roughness(A)（8 B/px）
    VK_FORMAT_R8G8B8A8_SRGB,       // 附件 1：Albedo(RGB，硬件 sRGB 编码) + Metallic(A)（4 B/px）
    VK_FORMAT_R8G8B8A8_UNORM       // 附件 2：AO(R)/反射率(G)/材质ID(B)/alpha(A)（4 B/px）
};

// 按附件索引取 GN_Vk 中的句柄：创建/销毁/外部绑定共用这张映射，避免顺序与内容错位。
static VkImage *gbufferImage(eGBUFFER_ATTACHMENT a) {
    switch(a) {
    case eGBUFFER_NORMAL_ROUGH: return &GN_Vk.gNormalRoughImage;
    case eGBUFFER_ALBEDO_METAL: return &GN_Vk.gAlbedoMetalImage;
    case eGBUFFER_AO: return &GN_Vk.gAoImage;
    case eGBUFFER_DEPTH: return &GN_Vk.gDepthImage;
    default: return nullptr;
    }
}

static VkDeviceMemory *gbufferMemory(eGBUFFER_ATTACHMENT a) {
    switch(a) {
    case eGBUFFER_NORMAL_ROUGH: return &GN_Vk.gNormalRoughMemory;
    case eGBUFFER_ALBEDO_METAL: return &GN_Vk.gAlbedoMetalMemory;
    case eGBUFFER_AO: return &GN_Vk.gAoMemory;
    case eGBUFFER_DEPTH: return &GN_Vk.gDepthMemory;
    default: return nullptr;
    }
}

static VkImageView *gbufferView(eGBUFFER_ATTACHMENT a) {
    switch(a) {
    case eGBUFFER_NORMAL_ROUGH: return &GN_Vk.gNormalRoughView;
    case eGBUFFER_ALBEDO_METAL: return &GN_Vk.gAlbedoMetalView;
    case eGBUFFER_AO: return &GN_Vk.gAoView;
    case eGBUFFER_DEPTH: return &GN_Vk.gDepthView;
    default: return nullptr;
    }
}

// 供延迟光照描述符绑定（binding 1..4）按附件枚举取视图。
VkImageView gbufferAttachmentView(eGBUFFER_ATTACHMENT a) {
    VkImageView *view = gbufferView(a);
    return view ? *view : VK_NULL_HANDLE;
}

// 单个附件：image + device-local memory + view（颜色/深度共用，仅格式、usage、aspect 不同）。
static void createGBufferAttachment(eGBUFFER_ATTACHMENT a) {
    VkImage *img = gbufferImage(a);
    VkDeviceMemory *mem = gbufferMemory(a);
    VkImageView *view = gbufferView(a);
    if(!img || !mem || !view) return;
    const bool depth = (a == eGBUFFER_DEPTH);
    VkImageCreateInfo ii = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    ii.imageType = VK_IMAGE_TYPE_2D;
    ii.extent = {GN_Vk.swapchainExtent.width, GN_Vk.swapchainExtent.height, 1};
    ii.mipLevels = 1;
    ii.arrayLayers = 1;
    ii.format = depth ? GN_Vk.depthFormat : sGBufferColorFormats[a];
    ii.tiling = VK_IMAGE_TILING_OPTIMAL;
    ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    ii.usage = depth
                   ? (VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                      VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
                   : (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                      VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    ii.samples = VK_SAMPLE_COUNT_1_BIT;
    ii.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    vkCreateImage(GN_Vk.device, &ii, 0, img);
    VkMemoryRequirements mr;
    vkGetImageMemoryRequirements(GN_Vk.device, *img, &mr);
    VkMemoryAllocateInfo mai = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    mai.allocationSize = mr.size;
    mai.memoryTypeIndex = FindMemType(mr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    vkAllocateMemory(GN_Vk.device, &mai, 0, mem);
    vkBindImageMemory(GN_Vk.device, *img, *mem, 0);
    VkImageViewCreateInfo vi = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    vi.image = *img;
    vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vi.format = ii.format;
    vi.subresourceRange.aspectMask = depth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    vi.subresourceRange.levelCount = 1;
    vi.subresourceRange.layerCount = 1;
    vkCreateImageView(GN_Vk.device, &vi, 0, view);
}

// 三张颜色 + 深度：句柄名与内容一一对应（NormalRough/AlbedoMetal/AO/Depth）。
static void createGBufferAttachments() {
    for(uint32_t i = 0; i < 3; ++i) {
        createGBufferAttachment((eGBUFFER_ATTACHMENT)i);
    }
    createGBufferAttachment(eGBUFFER_DEPTH);
}

// 销毁顺序：view → image → memory，销毁后句柄归零，供 resize/重复调用安全复用。
static void destroyGBufferAttachments() {
    for(uint32_t i = 0; i < eGBUFFER_ATTACHMENT_COUNT; ++i) {
        eGBUFFER_ATTACHMENT a = (eGBUFFER_ATTACHMENT)i;
        VkImageView *view = gbufferView(a);
        VkImage *img = gbufferImage(a);
        VkDeviceMemory *mem = gbufferMemory(a);
        if(*view) {
            vkDestroyImageView(GN_Vk.device, *view, 0);
            *view = VK_NULL_HANDLE;
        }
        if(*img) {
            vkDestroyImage(GN_Vk.device, *img, 0);
            *img = VK_NULL_HANDLE;
        }
        if(*mem) {
            vkFreeMemory(GN_Vk.device, *mem, 0);
            *mem = VK_NULL_HANDLE;
        }
    }
}

// G-Buffer 采样器（最近邻，尺寸无关，resize 不重建）。
static void createGBufferSampler() {
    VkSamplerCreateInfo sci = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    sci.magFilter = VK_FILTER_NEAREST;
    sci.minFilter = VK_FILTER_NEAREST;
    sci.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sci.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    vkCreateSampler(GN_Vk.device, &sci, 0, &GN_Vk.gBufferSampler);
}

// G-Buffer render pass：4 附件、1 subpass，并显式声明两侧依赖。
// 依赖 0（EXTERNAL→subpass0）：上一帧/上一 pass 对 G-Buffer 的采样读必须完成，才允许本帧写附件。
// 依赖 1（subpass0→EXTERNAL）：本帧颜色/深度写对后续 pass 的 FRAGMENT_SHADER 采样可见。
// 尺寸无关，resize 不重建。
static void createGBufferRenderPass() {
    VkAttachmentDescription ads[eGBUFFER_ATTACHMENT_COUNT] = {};
    for(uint32_t i = 0; i < 3; ++i) {
        ads[i].format = sGBufferColorFormats[i];
        ads[i].samples = VK_SAMPLE_COUNT_1_BIT;
        ads[i].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        ads[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        ads[i].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        ads[i].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }
    ads[eGBUFFER_DEPTH].format = GN_Vk.depthFormat;
    ads[eGBUFFER_DEPTH].samples = VK_SAMPLE_COUNT_1_BIT;
    ads[eGBUFFER_DEPTH].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    ads[eGBUFFER_DEPTH].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    ads[eGBUFFER_DEPTH].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    ads[eGBUFFER_DEPTH].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    VkAttachmentReference crs[3] = {
        {(uint32_t)eGBUFFER_NORMAL_ROUGH, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
        {(uint32_t)eGBUFFER_ALBEDO_METAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
        {(uint32_t)eGBUFFER_AO, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL}};
    VkAttachmentReference dr = {eGBUFFER_DEPTH, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
    VkSubpassDescription sp = {};
    sp.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    sp.colorAttachmentCount = 3;
    sp.pColorAttachments = crs;
    sp.pDepthStencilAttachment = &dr;
    VkSubpassDependency deps[2] = {};
    deps[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    deps[0].dstSubpass = 0;
    deps[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    deps[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                           VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    deps[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    deps[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    deps[1].srcSubpass = 0;
    deps[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    deps[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                           VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    deps[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    deps[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    deps[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    VkRenderPassCreateInfo rpi = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    rpi.attachmentCount = eGBUFFER_ATTACHMENT_COUNT;
    rpi.pAttachments = ads;
    rpi.subpassCount = 1;
    rpi.pSubpasses = &sp;
    rpi.dependencyCount = 2;
    rpi.pDependencies = deps;
    VkResult res = vkCreateRenderPass(GN_Vk.device, &rpi, 0, &GN_Vk.gBufferRenderPass);
    VkCheck(res, "gBufferRenderPass");
}

// 单份 G-Buffer framebuffer（与单帧 in flight 匹配；双帧并发时需 ×N）。
static void createGBufferFramebuffer() {
    if(GN_Vk.gBufferRenderPass == VK_NULL_HANDLE) return;
    GN_Vk.gBufferFramebuffers = new VkFramebuffer[1];
    GN_Vk.gBufferFramebuffers[0] = VK_NULL_HANDLE;
    VkImageView atts[eGBUFFER_ATTACHMENT_COUNT];
    for(uint32_t i = 0; i < eGBUFFER_ATTACHMENT_COUNT; ++i) {
        atts[i] = gbufferAttachmentView((eGBUFFER_ATTACHMENT)i);
    }
    VkFramebufferCreateInfo fci = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    fci.renderPass = GN_Vk.gBufferRenderPass;
    fci.attachmentCount = eGBUFFER_ATTACHMENT_COUNT;
    fci.pAttachments = atts;
    fci.width = GN_Vk.swapchainExtent.width;
    fci.height = GN_Vk.swapchainExtent.height;
    fci.layers = 1;
    VkResult res = vkCreateFramebuffer(GN_Vk.device, &fci, 0, &GN_Vk.gBufferFramebuffers[0]);
    VkCheck(res, "gBufferFramebuffer");
}

static void destroyGBufferFramebuffer() {
    if(!GN_Vk.gBufferFramebuffers) return;
    if(GN_Vk.gBufferFramebuffers[0]) {
        vkDestroyFramebuffer(GN_Vk.device, GN_Vk.gBufferFramebuffers[0], 0);
    }
    delete[] GN_Vk.gBufferFramebuffers;
    GN_Vk.gBufferFramebuffers = nullptr;
}

// G-Buffer 描述符：binding 0=SceneUBO、1=albedo 纹理、3=法线纹理（材质纹理每 draw 重写）。
// 布局、管线布局与描述符集与尺寸无关，resize 保留（池只创建一次且无 FREE/RESET）。
static void createGBufferDescriptors() {
    VkDescriptorSetLayoutBinding dslb[3] = {};
    dslb[0].binding = 0;
    dslb[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    dslb[0].descriptorCount = 1;
    dslb[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    dslb[1].binding = 1;
    dslb[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    dslb[1].descriptorCount = 1;
    dslb[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    dslb[2].binding = 3;
    dslb[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    dslb[2].descriptorCount = 1;
    dslb[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    VkDescriptorSetLayoutCreateInfo dci = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    dci.bindingCount = 3;
    dci.pBindings = dslb;
    vkCreateDescriptorSetLayout(GN_Vk.device, &dci, 0, &GN_Vk.gBufferDescriptorLayout);
    {
        VkPushConstantRange pcr = {};
        pcr.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        pcr.offset = 0;
        pcr.size = sizeof(VkModelPushConstPBR);
        VkPipelineLayoutCreateInfo pl = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        pl.setLayoutCount = 1;
        pl.pSetLayouts = &GN_Vk.gBufferDescriptorLayout;
        pl.pushConstantRangeCount = 1;
        pl.pPushConstantRanges = &pcr;
        vkCreatePipelineLayout(GN_Vk.device, &pl, 0, &GN_Vk.gBufferPipelineLayout);
    }
    VkDescriptorSetAllocateInfo dai = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    dai.descriptorPool = GN_Vk.descriptorPool;
    dai.descriptorSetCount = 1;
    dai.pSetLayouts = &GN_Vk.gBufferDescriptorLayout;
    vkAllocateDescriptorSets(GN_Vk.device, &dai, &GN_Vk.gBufferDescriptorSet);
    VkDescriptorBufferInfo dbi = {GN_Vk.uniformBuffer3D, 0, sizeof(VkSceneUBO)};
    VkDescriptorImageInfo dii = {GN_Vk.defaultSampler, GN_Vk.defaultImageView,
                                 VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    VkWriteDescriptorSet w[3] = {{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET},
                                 {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET},
                                 {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET}};
    w[0].dstSet = GN_Vk.gBufferDescriptorSet;
    w[0].dstBinding = 0;
    w[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    w[0].descriptorCount = 1;
    w[0].pBufferInfo = &dbi;
    w[1].dstSet = GN_Vk.gBufferDescriptorSet;
    w[1].dstBinding = 1;
    w[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    w[1].descriptorCount = 1;
    w[1].pImageInfo = &dii;
    w[2].dstSet = GN_Vk.gBufferDescriptorSet;
    w[2].dstBinding = 3;
    w[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    w[2].descriptorCount = 1;
    w[2].pImageInfo = &dii;
    vkUpdateDescriptorSets(GN_Vk.device, 3, w, 0, 0);
}

// G-Buffer 管线：固定视口/裁剪，resize 后必须按新尺寸重新创建（管线缓存按内容键复用）。
static void createGBufferPipeline() {
    uint32_t sw = GN_Vk.swapchainExtent.width;
    uint32_t sh = GN_Vk.swapchainExtent.height;
    auto gvSpv = LoadSPIRVFile("assets/shaders/gn3d_gbuffer.vert.spv");
    auto gfSpv = LoadSPIRVFile("assets/shaders/gn3d_gbuffer.frag.spv");
    if(gvSpv.empty() || gfSpv.empty()) return;
    GN3d::sVERTEX_LAYOUT layoutGB;
    layoutGB.Add(0, (uint32_t)offsetof(VkVertex3D, px), VK_FORMAT_R32G32B32_SFLOAT);
    layoutGB.Add(1, (uint32_t)offsetof(VkVertex3D, nx), VK_FORMAT_R32G32B32_SFLOAT);
    layoutGB.Add(2, (uint32_t)offsetof(VkVertex3D, tx), VK_FORMAT_R32G32B32_SFLOAT);
    layoutGB.Add(3, (uint32_t)offsetof(VkVertex3D, u), VK_FORMAT_R32G32_SFLOAT);
    layoutGB.aStride = (uint32_t)sizeof(VkVertex3D);
    GN3d::sPIPELINE_DESC descGB;
    descGB.aKind = GN3d::ePIPE_GRAPHICS;
    descGB.aVertSpv = gvSpv.data();
    descGB.aVertSize = gvSpv.size() * sizeof(uint32_t);
    descGB.aFragSpv = gfSpv.data();
    descGB.aFragSize = gfSpv.size() * sizeof(uint32_t);
    descGB.aVertexLayout = layoutGB;
    descGB.aRenderPass = GN_Vk.gBufferRenderPass;
    descGB.aSubpass = 0;
    descGB.aLayout = GN_Vk.gBufferPipelineLayout;
    descGB.aDepthTest = true;
    descGB.aDepthWrite = true;
    descGB.aDepthCompare = VK_COMPARE_OP_LESS;
    descGB.aDepthStencil = true;
    descGB.aCullBack = true;
    // 世界 CCW + 正高度视口 + 投影 Y 翻转 ⇒ 帧缓冲 CCW
    descGB.aFrontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    descGB.aBlend = GN3d::eBLEND_NONE;
    descGB.aTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    descGB.aSamples = VK_SAMPLE_COUNT_1_BIT;
    descGB.aColorAttachmentCount = 3;
    descGB.aDynamicViewport = false;
    descGB.aDynamicScissor = false;
    descGB.aViewport = VkViewport{0.0f, 0.0f, (float)sw, (float)sh, 0.0f, 1.0f};
    descGB.aScissor = VkRect2D{{0, 0}, {sw, sh}};
    descGB.aCustomBlend = false;
    GN_Vk.gBufferPipeline = GN_Vk.aPipelineCache.GetOrCreate(descGB);
    if(!GN_Vk.gBufferPipeline) {
        fprintf(stderr, "[GN_Vk] G-Buffer 管线创建失败: %s\n", GN_Vk.aPipelineCache.GetLastError());
    }
}

// 创建全部 G-Buffer 资源（GN_VkInit3D 调用一次）。
void init3DGbuffer() {
    createGBufferAttachments();
    createGBufferSampler();
    createGBufferRenderPass();
    createGBufferFramebuffer();
    createGBufferDescriptors();
    createGBufferPipeline();
}

// 统一销毁：vk3dShutdown 只调这一个函数，避免与 resize 各写一份释放逻辑。
void destroy3DGbuffer() {
    if(GN_Vk.gBufferPipeline) GN_Vk.gBufferPipeline = VK_NULL_HANDLE; // 归 aPipelineCache 持有
    destroyGBufferFramebuffer();
    destroyGBufferAttachments();
    if(GN_Vk.gBufferSampler) {
        vkDestroySampler(GN_Vk.device, GN_Vk.gBufferSampler, 0);
        GN_Vk.gBufferSampler = VK_NULL_HANDLE;
    }
    if(GN_Vk.gBufferRenderPass) {
        vkDestroyRenderPass(GN_Vk.device, GN_Vk.gBufferRenderPass, 0);
        GN_Vk.gBufferRenderPass = VK_NULL_HANDLE;
    }
    if(GN_Vk.gBufferPipelineLayout) {
        vkDestroyPipelineLayout(GN_Vk.device, GN_Vk.gBufferPipelineLayout, 0);
        GN_Vk.gBufferPipelineLayout = VK_NULL_HANDLE;
    }
    if(GN_Vk.gBufferDescriptorLayout) {
        vkDestroyDescriptorSetLayout(GN_Vk.device, GN_Vk.gBufferDescriptorLayout, 0);
        GN_Vk.gBufferDescriptorLayout = VK_NULL_HANDLE;
    }
    GN_Vk.gBufferDescriptorSet = VK_NULL_HANDLE; // 随描述符池统一释放
}

// resize：只重建尺寸相关部分（图像/视图/内存/framebuffer + 固定视口管线）；
// 采样器、render pass、描述符布局与描述符集保留，避免池无 FREE/RESET 时泄漏。
void recreate3DGbufferTargets() {
    destroyGBufferFramebuffer();
    destroyGBufferAttachments();
    createGBufferAttachments();
    createGBufferFramebuffer();
    createGBufferPipeline();
}

} // namespace GNg
