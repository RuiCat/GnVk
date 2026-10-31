// 3D 初始化集合：渲染目标、前向/PBR 管线、阴影、HDR/合成、天空盒、G-Buffer、延迟光照。
// 由 GN_VkInit3D 按顺序编排；各 init3D* 函数只负责一个阶段。
#include <GN.h>
#include "GN_Vk.h"
#include "vkInternal.h"

namespace GNg {

void init3DTargets() {
    // ── 查询 MSAA 支持 ──────────────────────────────
    {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(GN_Vk.physicalDevice, &props);
        VkSampleCountFlags counts = props.limits.framebufferColorSampleCounts & props.limits.framebufferDepthSampleCounts;
        if(counts & VK_SAMPLE_COUNT_4_BIT) GN_Vk.msaaSamples = VK_SAMPLE_COUNT_4_BIT;
        else if(counts & VK_SAMPLE_COUNT_2_BIT) GN_Vk.msaaSamples = VK_SAMPLE_COUNT_2_BIT;
        else GN_Vk.msaaSamples = VK_SAMPLE_COUNT_1_BIT;
    }

    // ── 寻找合适的深度格式 ──────────────────────────
    VkFormat depthFormats[] = {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D16_UNORM};
    GN_Vk.depthFormat = VK_FORMAT_D32_SFLOAT;
    for(auto df : depthFormats) {
        VkFormatProperties fp;
        vkGetPhysicalDeviceFormatProperties(GN_Vk.physicalDevice, df, &fp);
        if(fp.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
            GN_Vk.depthFormat = df;
            break;
        }
    }
    // MSAA 颜色/深度目标由可重入 helper 创建（resize 复用同一路径）。
    if(!create3DMsaaTargets()) {
        fprintf(stderr, "[GN_Vk] 警告: MSAA/depth 目标创建失败\n");
    }

    // ── 创建 3D Render Pass (3 附件: MSAA Color + MSAA Depth + Resolve) ──
    VkAttachmentDescription ads[3] = {};
    // [0] MSAA Color — 多重采样, 瞬态 (不需要保存)
    ads[0].format = GN_Vk.swapchainFormat;
    ads[0].samples = GN_Vk.msaaSamples;
    ads[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    ads[0].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    ads[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    ads[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    // [1] MSAA Depth — 多重采样, 瞬态
    ads[1].format = GN_Vk.depthFormat;
    ads[1].samples = GN_Vk.msaaSamples;
    ads[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    ads[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    ads[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    ads[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    // [2] Resolve — 单采样, 输出到 swapchain
    ads[2].format = GN_Vk.swapchainFormat;
    ads[2].samples = VK_SAMPLE_COUNT_1_BIT;
    ads[2].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    ads[2].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    ads[2].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    ads[2].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference cr = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL}; // MSAA color
    VkAttachmentReference dr = {1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL}; // MSAA depth
    VkAttachmentReference rr = {2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL}; // resolve target
    VkSubpassDescription sp = {};
    sp.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    sp.colorAttachmentCount = 1;
    sp.pColorAttachments = &cr;
    sp.pResolveAttachments = &rr; // ★ MSAA resolve
    sp.pDepthStencilAttachment = &dr;

    VkRenderPassCreateInfo rpi = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    rpi.attachmentCount = 3;
    rpi.pAttachments = ads;
    rpi.subpassCount = 1;
    rpi.pSubpasses = &sp;
    vkCreateRenderPass(GN_Vk.device, &rpi, 0, &GN_Vk.renderPass3D);

    // 3D framebuffers（引用新的 MSAA/depth/swapchainView；resize 走同一 helper）。
    create3DForwardFramebuffers();
}

// ── 可重入的 3D 目标创建/销毁（GN_VkResize 与 GN_VkShutdown3D 共用）──
void destroy3DForwardFramebuffers() {
    if(!s_framebuffers3D) return;
    for(uint32_t i = 0; i < GN_Vk.swapchainImageCount; i++) {
        if(s_framebuffers3D[i]) vkDestroyFramebuffer(GN_Vk.device, s_framebuffers3D[i], 0);
    }
    delete[] s_framebuffers3D;
    s_framebuffers3D = nullptr;
}

void destroy3DMsaaTargets() {
    if(GN_Vk.msaaColorView) {
        vkDestroyImageView(GN_Vk.device, GN_Vk.msaaColorView, 0);
        GN_Vk.msaaColorView = VK_NULL_HANDLE;
    }
    if(GN_Vk.msaaColorImage) {
        vkDestroyImage(GN_Vk.device, GN_Vk.msaaColorImage, 0);
        GN_Vk.msaaColorImage = VK_NULL_HANDLE;
    }
    if(GN_Vk.msaaColorMemory) {
        vkFreeMemory(GN_Vk.device, GN_Vk.msaaColorMemory, 0);
        GN_Vk.msaaColorMemory = VK_NULL_HANDLE;
    }
    if(GN_Vk.depthView) {
        vkDestroyImageView(GN_Vk.device, GN_Vk.depthView, 0);
        GN_Vk.depthView = VK_NULL_HANDLE;
    }
    if(GN_Vk.depthImage) {
        vkDestroyImage(GN_Vk.device, GN_Vk.depthImage, 0);
        GN_Vk.depthImage = VK_NULL_HANDLE;
    }
    if(GN_Vk.depthMemory) {
        vkFreeMemory(GN_Vk.device, GN_Vk.depthMemory, 0);
        GN_Vk.depthMemory = VK_NULL_HANDLE;
    }
}

bool create3DMsaaTargets() {
    const uint32_t sw = GN_Vk.swapchainExtent.width;
    const uint32_t sh = GN_Vk.swapchainExtent.height;
    {
        VkImageCreateInfo ii = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        ii.imageType = VK_IMAGE_TYPE_2D;
        ii.extent = {sw, sh, 1};
        ii.mipLevels = 1;
        ii.arrayLayers = 1;
        ii.format = GN_Vk.swapchainFormat;
        ii.tiling = VK_IMAGE_TILING_OPTIMAL;
        ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        ii.usage = VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        ii.samples = GN_Vk.msaaSamples;
        ii.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if(vkCreateImage(GN_Vk.device, &ii, 0, &GN_Vk.msaaColorImage) != VK_SUCCESS) return false;
        VkMemoryRequirements mr;
        vkGetImageMemoryRequirements(GN_Vk.device, GN_Vk.msaaColorImage, &mr);
        VkMemoryAllocateInfo mai = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        mai.allocationSize = mr.size;
        mai.memoryTypeIndex = FindMemType(mr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if(vkAllocateMemory(GN_Vk.device, &mai, 0, &GN_Vk.msaaColorMemory) != VK_SUCCESS) return false;
        vkBindImageMemory(GN_Vk.device, GN_Vk.msaaColorImage, GN_Vk.msaaColorMemory, 0);
    }
    {
        VkImageViewCreateInfo vi = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        vi.image = GN_Vk.msaaColorImage;
        vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
        vi.format = GN_Vk.swapchainFormat;
        vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        vi.subresourceRange.levelCount = 1;
        vi.subresourceRange.layerCount = 1;
        if(vkCreateImageView(GN_Vk.device, &vi, 0, &GN_Vk.msaaColorView) != VK_SUCCESS) return false;
    }
    {
        VkImageCreateInfo ii = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        ii.imageType = VK_IMAGE_TYPE_2D;
        ii.extent = {sw, sh, 1};
        ii.mipLevels = 1;
        ii.arrayLayers = 1;
        ii.format = GN_Vk.depthFormat;
        ii.tiling = VK_IMAGE_TILING_OPTIMAL;
        ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        ii.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        ii.samples = GN_Vk.msaaSamples;
        ii.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if(vkCreateImage(GN_Vk.device, &ii, 0, &GN_Vk.depthImage) != VK_SUCCESS) return false;
        VkMemoryRequirements dmr;
        vkGetImageMemoryRequirements(GN_Vk.device, GN_Vk.depthImage, &dmr);
        VkMemoryAllocateInfo dmai = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        dmai.allocationSize = dmr.size;
        dmai.memoryTypeIndex = FindMemType(dmr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if(vkAllocateMemory(GN_Vk.device, &dmai, 0, &GN_Vk.depthMemory) != VK_SUCCESS) return false;
        vkBindImageMemory(GN_Vk.device, GN_Vk.depthImage, GN_Vk.depthMemory, 0);
    }
    {
        VkImageViewCreateInfo vi = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        vi.image = GN_Vk.depthImage;
        vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
        vi.format = GN_Vk.depthFormat;
        vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        vi.subresourceRange.levelCount = 1;
        vi.subresourceRange.layerCount = 1;
        if(vkCreateImageView(GN_Vk.device, &vi, 0, &GN_Vk.depthView) != VK_SUCCESS) return false;
    }
    return true;
}

void create3DForwardFramebuffers() {
    if(GN_Vk.renderPass3D == VK_NULL_HANDLE) return;
    s_framebuffers3D = new VkFramebuffer[GN_Vk.swapchainImageCount];
    for(uint32_t i = 0; i < GN_Vk.swapchainImageCount; i++) {
        s_framebuffers3D[i] = VK_NULL_HANDLE;
        VkImageView atts[3] = {GN_Vk.msaaColorView, GN_Vk.depthView, GN_Vk.swapchainViews[i]};
        VkFramebufferCreateInfo fci = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        fci.renderPass = GN_Vk.renderPass3D;
        fci.attachmentCount = 3;
        fci.pAttachments = atts;
        fci.width = GN_Vk.swapchainExtent.width;
        fci.height = GN_Vk.swapchainExtent.height;
        fci.layers = 1;
        vkCreateFramebuffer(GN_Vk.device, &fci, 0, &s_framebuffers3D[i]);
    }
}

// resize：先销毁引用旧 swapchainView/旧尺寸的 framebuffer 与 MSAA/depth，再按新尺寸重建。
void recreate3DTargets() {
    destroy3DForwardFramebuffers();
    destroy3DMsaaTargets();
    if(!create3DMsaaTargets()) {
        fprintf(stderr, "[GN_Vk] 警告: MSAA/depth 目标重建失败（resize）\n");
    }
    create3DForwardFramebuffers();
}

// 前向 3D 管线（固定视口，resize 时按新 extent 重建；字段与 init3DForward 一致）。
void create3DForwardPipeline(const std::vector<uint32_t> &vertSpv,
                             const std::vector<uint32_t> &fragSpv) {
    if(vertSpv.empty() || fragSpv.empty() || GN_Vk.renderPass3D == VK_NULL_HANDLE ||
       GN_Vk.pipelineLayout3D == VK_NULL_HANDLE) {
        return;
    }
    GN3d::sVERTEX_LAYOUT layout3D;
    layout3D.Add(0, (uint32_t)offsetof(VkVertex3D, px), VK_FORMAT_R32G32B32_SFLOAT);
    layout3D.Add(1, (uint32_t)offsetof(VkVertex3D, nx), VK_FORMAT_R32G32B32_SFLOAT);
    layout3D.Add(2, (uint32_t)offsetof(VkVertex3D, tx), VK_FORMAT_R32G32B32_SFLOAT);
    layout3D.Add(3, (uint32_t)offsetof(VkVertex3D, u), VK_FORMAT_R32G32_SFLOAT);
    layout3D.aStride = (uint32_t)sizeof(VkVertex3D);
    GN3d::sPIPELINE_DESC desc;
    desc.aKind = GN3d::ePIPE_GRAPHICS;
    desc.aVertSpv = vertSpv.data();
    desc.aVertSize = vertSpv.size() * sizeof(uint32_t);
    desc.aFragSpv = fragSpv.data();
    desc.aFragSize = fragSpv.size() * sizeof(uint32_t);
    desc.aVertexLayout = layout3D;
    desc.aRenderPass = GN_Vk.renderPass3D;
    desc.aSubpass = 0;
    desc.aLayout = GN_Vk.pipelineLayout3D;
    desc.aDepthTest = true;
    desc.aDepthWrite = true;
    desc.aDepthCompare = VK_COMPARE_OP_LESS;
    desc.aDepthStencil = true;
    desc.aCullBack = true;
    desc.aFrontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    desc.aBlend = GN3d::eBLEND_NONE;
    desc.aTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    desc.aSamples = GN_Vk.msaaSamples;
    desc.aColorAttachmentCount = 1;
    desc.aDynamicViewport = false;
    desc.aDynamicScissor = false;
    desc.aViewport = VkViewport{0.0f, 0.0f, (float)GN_Vk.swapchainExtent.width,
                                (float)GN_Vk.swapchainExtent.height, 0.0f, 1.0f};
    desc.aScissor = VkRect2D{{0, 0}, GN_Vk.swapchainExtent};
    desc.aCustomBlend = false;
    GN_Vk.pipeline3D = GN_Vk.aPipelineCache.GetOrCreate(desc);
    if(!GN_Vk.pipeline3D) {
        fprintf(stderr, "[GN_Vk] 3D 前向管线创建失败: %s\n", GN_Vk.aPipelineCache.GetLastError());
    }
}

// PBR 管线（固定视口，resize 时按新 extent 重建）。
void create3DPbrPipeline(const std::vector<uint32_t> &pbrVert,
                         const std::vector<uint32_t> &pbrFrag) {
    if(pbrVert.empty() || pbrFrag.empty() || GN_Vk.renderPass3D == VK_NULL_HANDLE ||
       GN_Vk.pipelineLayoutPBR == VK_NULL_HANDLE) {
        return;
    }
    GN3d::sVERTEX_LAYOUT layoutPBR;
    layoutPBR.Add(0, (uint32_t)offsetof(VkVertex3D, px), VK_FORMAT_R32G32B32_SFLOAT);
    layoutPBR.Add(1, (uint32_t)offsetof(VkVertex3D, nx), VK_FORMAT_R32G32B32_SFLOAT);
    layoutPBR.Add(2, (uint32_t)offsetof(VkVertex3D, tx), VK_FORMAT_R32G32B32_SFLOAT);
    layoutPBR.Add(3, (uint32_t)offsetof(VkVertex3D, u), VK_FORMAT_R32G32_SFLOAT);
    layoutPBR.aStride = (uint32_t)sizeof(VkVertex3D);
    GN3d::sPIPELINE_DESC descPBR;
    descPBR.aKind = GN3d::ePIPE_GRAPHICS;
    descPBR.aVertSpv = pbrVert.data();
    descPBR.aVertSize = pbrVert.size() * sizeof(uint32_t);
    descPBR.aFragSpv = pbrFrag.data();
    descPBR.aFragSize = pbrFrag.size() * sizeof(uint32_t);
    descPBR.aVertexLayout = layoutPBR;
    descPBR.aRenderPass = GN_Vk.renderPass3D;
    descPBR.aSubpass = 0;
    descPBR.aLayout = GN_Vk.pipelineLayoutPBR;
    descPBR.aDepthTest = true;
    descPBR.aDepthWrite = true;
    descPBR.aDepthCompare = VK_COMPARE_OP_LESS;
    descPBR.aDepthStencil = true;
    descPBR.aCullBack = true;
    descPBR.aFrontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    descPBR.aBlend = GN3d::eBLEND_NONE;
    descPBR.aTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    descPBR.aSamples = GN_Vk.msaaSamples;
    descPBR.aColorAttachmentCount = 1;
    descPBR.aDynamicViewport = false;
    descPBR.aDynamicScissor = false;
    descPBR.aViewport = VkViewport{0.0f, 0.0f, (float)GN_Vk.swapchainExtent.width,
                                   (float)GN_Vk.swapchainExtent.height, 0.0f, 1.0f};
    descPBR.aScissor = VkRect2D{{0, 0}, GN_Vk.swapchainExtent};
    descPBR.aCustomBlend = false;
    GN_Vk.pipelinePBR = GN_Vk.aPipelineCache.GetOrCreate(descPBR);
    if(!GN_Vk.pipelinePBR) {
        fprintf(stderr, "[GN_Vk] PBR 管线创建失败: %s\n", GN_Vk.aPipelineCache.GetLastError());
    }

    // 透明变体（BLEND）：标准 srcAlpha/oneMinusSrcAlpha，深度测试开、写关；双面
    // （MMD 头发/裙边多为双面）。固定在 create3DPbrPipeline 里一起建，resize 走同一入口重建。
    GN3d::sPIPELINE_DESC descPBRBlend = descPBR;
    descPBRBlend.aDepthWrite = false;
    descPBRBlend.aCullBack = false;
    descPBRBlend.aBlend = GN3d::eBLEND_ALPHA;
    GN_Vk.pipelinePBRBlend = GN_Vk.aPipelineCache.GetOrCreate(descPBRBlend);
    if(!GN_Vk.pipelinePBRBlend) {
        fprintf(stderr, "[GN_Vk] PBR 混合管线创建失败: %s\n", GN_Vk.aPipelineCache.GetLastError());
    }
}

// 天空盒管线（固定视口，resize 时按新 extent 重建；需要 assets 下的 SPIR-V）。
void create3DSkyboxPipeline() {
    auto svSpv = LoadSPIRVFile("assets/shaders/gn3d_skybox.vert.spv");
    auto sfSpv = LoadSPIRVFile("assets/shaders/gn3d_skybox.frag.spv");
    if(svSpv.empty() || sfSpv.empty() || GN_Vk.renderPass3D == VK_NULL_HANDLE ||
       GN_Vk.skyboxPipelineLayout == VK_NULL_HANDLE) {
        return;
    }
    GN3d::sPIPELINE_DESC descSky;
    descSky.aKind = GN3d::ePIPE_GRAPHICS;
    descSky.aVertSpv = svSpv.data();
    descSky.aVertSize = svSpv.size() * sizeof(uint32_t);
    descSky.aFragSpv = sfSpv.data();
    descSky.aFragSize = sfSpv.size() * sizeof(uint32_t);
    descSky.aRenderPass = GN_Vk.renderPass3D;
    descSky.aSubpass = 0;
    descSky.aLayout = GN_Vk.skyboxPipelineLayout;
    descSky.aDepthTest = true;
    descSky.aDepthWrite = false;
    descSky.aDepthCompare = VK_COMPARE_OP_LESS_OR_EQUAL;
    descSky.aDepthStencil = true;
    descSky.aCullBack = false;
    descSky.aFrontFace = VK_FRONT_FACE_CLOCKWISE;
    descSky.aBlend = GN3d::eBLEND_NONE;
    descSky.aTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    descSky.aSamples = GN_Vk.msaaSamples;
    descSky.aColorAttachmentCount = 1;
    descSky.aDynamicViewport = false;
    descSky.aDynamicScissor = false;
    descSky.aViewport = VkViewport{0.0f, 0.0f, (float)GN_Vk.swapchainExtent.width,
                                   (float)GN_Vk.swapchainExtent.height, 0.0f, 1.0f};
    descSky.aScissor = VkRect2D{{0, 0}, GN_Vk.swapchainExtent};
    descSky.aCustomBlend = false;
    GN_Vk.skyboxPipeline = GN_Vk.aPipelineCache.GetOrCreate(descSky);
    if(!GN_Vk.skyboxPipeline) {
        fprintf(stderr, "[GN_Vk] 天空盒管线创建失败: %s\n", GN_Vk.aPipelineCache.GetLastError());
    }
}

// 前向 3D 管线：着色器模块、描述符布局、管线布局、图形管线、场景/光照 UBO、默认描述符集。
void init3DForward(const std::vector<uint32_t> &vertSpv,
                   const std::vector<uint32_t> &fragSpv) {
    // 着色器模块由管线缓存（P1b）统一创建与持有，这里不再建 VkShaderModule

    // ── 3D 描述符布局 ────────────────────────────────
    // Binding 0: Scene UBO (uniform buffer) — 顶点+片段着色器
    // Binding 1: Texture sampler (combined image sampler) — 片段着色器
    // Binding 2: Light UBO (uniform buffer) — 片段着色器
    // Binding 3: Fog/Weather UBO (uniform buffer) — 片段着色器（P11-S3）
    // Binding 4: GI UBO (uniform buffer) — 片段着色器（G1b；与 §3.7 冻结槽位表一致）
    {
        VkDescriptorSetLayoutBinding dslb[5] = {};
        dslb[0].binding = 0;
        dslb[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        dslb[0].descriptorCount = 1;
        dslb[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        dslb[1].binding = 1;
        dslb[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        dslb[1].descriptorCount = 1;
        dslb[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        dslb[2].binding = 2;
        dslb[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        dslb[2].descriptorCount = 1;
        dslb[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        dslb[3].binding = 3;
        dslb[3].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        dslb[3].descriptorCount = 1;
        dslb[3].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        dslb[4].binding = 4;
        dslb[4].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        dslb[4].descriptorCount = 1;
        dslb[4].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        VkDescriptorSetLayoutCreateInfo dslci = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        dslci.bindingCount = 5;
        dslci.pBindings = dslb;
        vkCreateDescriptorSetLayout(GN_Vk.device, &dslci, 0, &GN_Vk.descriptorLayout3D);
    }

    // ── 3D 管线布局 (含 Push Constant) ───────────────
    {
        VkPushConstantRange pcr = {};
        pcr.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        pcr.offset = 0;
        pcr.size = sizeof(VkModelPushConstBP);
        VkPipelineLayoutCreateInfo plci = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        plci.setLayoutCount = 1;
        plci.pSetLayouts = &GN_Vk.descriptorLayout3D;
        plci.pushConstantRangeCount = 1;
        plci.pPushConstantRanges = &pcr;
        vkCreatePipelineLayout(GN_Vk.device, &plci, 0, &GN_Vk.pipelineLayout3D);
    }

    // ── 3D 图形管线（可重入 helper，resize 复用同一路径）──────
    create3DForwardPipeline(vertSpv, fragSpv);

    // ── 3D 场景 UBO ──────────────────────────────────
    CreateBuf(sizeof(VkSceneUBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
              GN_Vk.uniformBuffer3D, GN_Vk.uniformMemory3D);

    // ── 3D 光照 UBO ──────────────────────────────────
    // 最大: GN_MAX_LIGHTS(8) * sizeof(sLIGHT_GPU)(80) + 20 = 660 bytes
    CreateBuf(1024, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
              GN_Vk.lightBuffer3D, GN_Vk.lightMemory3D);

    // ── 默认 3D 描述符集 ─────────────────────────────
    {
        VkDescriptorSetAllocateInfo dai = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        dai.descriptorPool = GN_Vk.descriptorPool;
        dai.descriptorSetCount = 1;
        dai.pSetLayouts = &GN_Vk.descriptorLayout3D;
        vkAllocateDescriptorSets(GN_Vk.device, &dai, &GN_Vk.descriptorSet3D);
        VkDescriptorBufferInfo dbi = {GN_Vk.uniformBuffer3D, 0, sizeof(VkSceneUBO)};
        VkDescriptorBufferInfo lbi = {GN_Vk.lightBuffer3D, 0, 1024};
        VkDescriptorBufferInfo fbi = {atmosphereFogBuffer(), 0, atmosphereFogRange()};
        VkDescriptorImageInfo dii = {GN_Vk.defaultSampler, GN_Vk.defaultImageView,
                                     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        VkWriteDescriptorSet w[4] = {{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET},
                                     {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET},
                                     {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET},
                                     {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET}};
        w[0].dstSet = GN_Vk.descriptorSet3D;
        w[0].dstBinding = 0;
        w[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        w[0].descriptorCount = 1;
        w[0].pBufferInfo = &dbi;
        w[1].dstSet = GN_Vk.descriptorSet3D;
        w[1].dstBinding = 1;
        w[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        w[1].descriptorCount = 1;
        w[1].pImageInfo = &dii;
        w[2].dstSet = GN_Vk.descriptorSet3D;
        w[2].dstBinding = 2;
        w[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        w[2].descriptorCount = 1;
        w[2].pBufferInfo = &lbi;
        w[3].dstSet = GN_Vk.descriptorSet3D;
        w[3].dstBinding = 3;
        w[3].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        w[3].descriptorCount = 1;
        w[3].pBufferInfo = &fbi;
        vkUpdateDescriptorSets(GN_Vk.device, 4, w, 0, 0);
    }
}

// PBR 管线：描述符布局（含 IBL cubemap）、管线布局与图形管线。
void init3DPbr(const std::vector<uint32_t> &pbrVert,
               const std::vector<uint32_t> &pbrFrag) {
    // ═══════════════════════════════════════════════════════
    //  PBR 描述符布局 (Binding 5 = skyboxCubemap for IBL)
    //  G1b：7 = GI UBO（§3.7 冻结槽位），8 = 预滤波环境 cubemap，9 = BRDF LUT
    // ═══════════════════════════════════════════════════════
    {
        VkDescriptorSetLayoutBinding dslb[10] = {};
        dslb[0].binding = 0;
        dslb[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        dslb[0].descriptorCount = 1;
        dslb[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        dslb[1].binding = 1;
        dslb[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        dslb[1].descriptorCount = 1;
        dslb[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        dslb[2].binding = 2;
        dslb[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        dslb[2].descriptorCount = 1;
        dslb[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        dslb[3].binding = 3;
        dslb[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        dslb[3].descriptorCount = 1;
        dslb[3].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        dslb[4].binding = 4;
        dslb[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        dslb[4].descriptorCount = 1;
        dslb[4].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        dslb[5].binding = 5;
        dslb[5].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        dslb[5].descriptorCount = 1;
        dslb[5].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        // P11-S3：binding 6 = 雾 UBO（与 §3.6 冻结槽位表一致）。
        dslb[6].binding = 6;
        dslb[6].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        dslb[6].descriptorCount = 1;
        dslb[6].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        dslb[7].binding = 7;
        dslb[7].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        dslb[7].descriptorCount = 1;
        dslb[7].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        dslb[8].binding = 8;
        dslb[8].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        dslb[8].descriptorCount = 1;
        dslb[8].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        dslb[9].binding = 9;
        dslb[9].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        dslb[9].descriptorCount = 1;
        dslb[9].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        VkDescriptorSetLayoutCreateInfo dslci = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        dslci.bindingCount = 10;
        dslci.pBindings = dslb;
        vkCreateDescriptorSetLayout(GN_Vk.device, &dslci, 0, &GN_Vk.descriptorLayoutPBR);
    }

    // ── PBR 管线布局 ─────────────────────────────────
    {
        VkPushConstantRange pcr = {};
        pcr.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        pcr.offset = 0;
        pcr.size = sizeof(VkModelPushConstPBR);
        VkPipelineLayoutCreateInfo plci = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        plci.setLayoutCount = 1;
        plci.pSetLayouts = &GN_Vk.descriptorLayoutPBR;
        plci.pushConstantRangeCount = 1;
        plci.pPushConstantRanges = &pcr;
        vkCreatePipelineLayout(GN_Vk.device, &plci, 0, &GN_Vk.pipelineLayoutPBR);
    }

    // ── 加载 PBR 着色器并创建管线（可重入 helper，resize 复用同一路径）────
    create3DPbrPipeline(pbrVert, pbrFrag);
}

// 阴影映射：深度图像/视图/采样器、render pass、framebuffer、描述符集与管线。
void init3DShadow(const std::vector<uint32_t> &shadowVertSpv) {
    uint32_t smSize = GN_Vk.shadowMapSize;
    // 创建 Shadow Map 图像
    {
        VkImageCreateInfo ii = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        ii.imageType = VK_IMAGE_TYPE_2D;
        ii.extent = {smSize, smSize, 1};
        ii.mipLevels = 1;
        ii.arrayLayers = 1;
        ii.format = VK_FORMAT_D32_SFLOAT;
        ii.tiling = VK_IMAGE_TILING_OPTIMAL;
        ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        ii.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                   VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        ii.samples = VK_SAMPLE_COUNT_1_BIT;
        ii.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        vkCreateImage(GN_Vk.device, &ii, 0, &GN_Vk.shadowImage);
        VkMemoryRequirements mr;
        vkGetImageMemoryRequirements(GN_Vk.device, GN_Vk.shadowImage, &mr);
        VkMemoryAllocateInfo mai = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        mai.allocationSize = mr.size;
        mai.memoryTypeIndex = FindMemType(mr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        vkAllocateMemory(GN_Vk.device, &mai, 0, &GN_Vk.shadowMemory);
        vkBindImageMemory(GN_Vk.device, GN_Vk.shadowImage, GN_Vk.shadowMemory, 0);
    }

    {
        VkImageViewCreateInfo vi = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        vi.image = GN_Vk.shadowImage;
        vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
        vi.format = VK_FORMAT_D32_SFLOAT;
        vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        vi.subresourceRange.levelCount = 1;
        vi.subresourceRange.layerCount = 1;
        vkCreateImageView(GN_Vk.device, &vi, 0, &GN_Vk.shadowView);
    }

    // Shadow sampler (compare mode + border color)
    {
        VkSamplerCreateInfo sci = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        sci.magFilter = VK_FILTER_LINEAR;
        sci.minFilter = VK_FILTER_LINEAR;
        sci.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        sci.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        sci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        sci.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
        sci.compareEnable = VK_TRUE;
        sci.compareOp = VK_COMPARE_OP_LESS;
        vkCreateSampler(GN_Vk.device, &sci, 0, &GN_Vk.shadowSampler);
    }

    // Shadow render pass (depth-only)
    {
        VkAttachmentDescription ad = {};
        ad.format = VK_FORMAT_D32_SFLOAT;
        ad.samples = VK_SAMPLE_COUNT_1_BIT;
        ad.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        ad.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        ad.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        ad.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        VkAttachmentReference dr = {0, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
        VkSubpassDescription sp = {};
        sp.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        sp.pDepthStencilAttachment = &dr;
        VkRenderPassCreateInfo rpi = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
        rpi.attachmentCount = 1;
        rpi.pAttachments = &ad;
        rpi.subpassCount = 1;
        rpi.pSubpasses = &sp;
        vkCreateRenderPass(GN_Vk.device, &rpi, 0, &GN_Vk.shadowRenderPass);
    }

    {
        VkFramebufferCreateInfo fci = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        fci.renderPass = GN_Vk.shadowRenderPass;
        fci.attachmentCount = 1;
        fci.pAttachments = &GN_Vk.shadowView;
        fci.width = smSize;
        fci.height = smSize;
        fci.layers = 1;
        vkCreateFramebuffer(GN_Vk.device, &fci, 0, &GN_Vk.shadowFramebuffer);
    }

    // Shadow 描述符布局 (Binding 0 = UBO for light VP)
    {
        VkDescriptorSetLayoutBinding dslb[1] = {};
        dslb[0].binding = 0;
        dslb[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        dslb[0].descriptorCount = 1;
        dslb[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        VkDescriptorSetLayoutCreateInfo dslci = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        dslci.bindingCount = 1;
        dslci.pBindings = dslb;
        vkCreateDescriptorSetLayout(GN_Vk.device, &dslci, 0, &GN_Vk.shadowDescriptorLayout);
    }

    {
        VkPushConstantRange pcr = {};
        pcr.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        pcr.offset = 0;
        pcr.size = 64; // just model mat4
        VkPipelineLayoutCreateInfo plci = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        plci.setLayoutCount = 1;
        plci.pSetLayouts = &GN_Vk.shadowDescriptorLayout;
        plci.pushConstantRangeCount = 1;
        plci.pPushConstantRanges = &pcr;
        vkCreatePipelineLayout(GN_Vk.device, &plci, 0, &GN_Vk.shadowPipelineLayout);
    }

    // Shadow UBO
    CreateBuf(64, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
              GN_Vk.shadowUBOBuffer, GN_Vk.shadowUBOMemory);

    // Shadow descriptor set
    {
        VkDescriptorSetAllocateInfo dai = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        dai.descriptorPool = GN_Vk.descriptorPool;
        dai.descriptorSetCount = 1;
        dai.pSetLayouts = &GN_Vk.shadowDescriptorLayout;
        vkAllocateDescriptorSets(GN_Vk.device, &dai, &GN_Vk.shadowDescriptorSet);
        VkDescriptorBufferInfo dbi = {GN_Vk.shadowUBOBuffer, 0, 64};
        VkWriteDescriptorSet w = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        w.dstSet = GN_Vk.shadowDescriptorSet;
        w.dstBinding = 0;
        w.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        w.descriptorCount = 1;
        w.pBufferInfo = &dbi;
        vkUpdateDescriptorSets(GN_Vk.device, 1, &w, 0, 0);
    }

    // Shadow pipeline（P1b：深度专用走缓存：无片元阶段、0 颜色附件、深度偏移 1.25/1.75）
    if(!shadowVertSpv.empty()) {
        GN3d::sVERTEX_LAYOUT layoutShadow;
        layoutShadow.Add(0, (uint32_t)offsetof(VkVertex3D, px), VK_FORMAT_R32G32B32_SFLOAT);
        layoutShadow.aStride = (uint32_t)sizeof(VkVertex3D);

        GN3d::sPIPELINE_DESC descShadow;
        descShadow.aKind = GN3d::ePIPE_GRAPHICS;
        descShadow.aVertSpv = shadowVertSpv.data();
        descShadow.aVertSize = shadowVertSpv.size() * sizeof(uint32_t);
        descShadow.aFragSpv = 0; // 深度专用：只提交顶点阶段
        descShadow.aFragSize = 0;
        descShadow.aVertexLayout = layoutShadow;
        descShadow.aRenderPass = GN_Vk.shadowRenderPass;
        descShadow.aSubpass = 0;
        descShadow.aLayout = GN_Vk.shadowPipelineLayout;
        descShadow.aDepthTest = true;
        descShadow.aDepthWrite = true;
        descShadow.aDepthCompare = VK_COMPARE_OP_LESS;
        descShadow.aDepthStencil = true;
        descShadow.aDepthBiasEnable = true;
        descShadow.aDepthBiasConstant = 1.25f;
        descShadow.aDepthBiasSlope = 1.75f;
        descShadow.aCullBack = true;
        // 阴影用 Mat44OrthoVK（无 Y 翻转）⇒ 世界 CCW 被垂直镜像成帧缓冲 CW
        descShadow.aFrontFace = VK_FRONT_FACE_CLOCKWISE;
        descShadow.aBlend = GN3d::eBLEND_NONE;
        descShadow.aTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        descShadow.aSamples = VK_SAMPLE_COUNT_1_BIT;
        descShadow.aColorAttachmentCount = 0;
        descShadow.aDynamicViewport = false;
        descShadow.aDynamicScissor = false;
        descShadow.aViewport = VkViewport{0.0f, 0.0f, (float)smSize, (float)smSize, 0.0f, 1.0f};
        descShadow.aScissor = VkRect2D{{0, 0}, {smSize, smSize}};
        descShadow.aCustomBlend = false;
        GN_Vk.shadowPipeline = GN_Vk.aPipelineCache.GetOrCreate(descShadow);
        if(!GN_Vk.shadowPipeline) {
            fprintf(stderr, "[GN_Vk] 阴影管线创建失败: %s\n", GN_Vk.aPipelineCache.GetLastError());
        }
    }
}

// HDR 离屏目标 + 合成管线：HDR 图像/视图/采样器、HDR render pass 与 framebuffer、
// 合成（色调映射）管线与描述符集。

} // namespace GNg
