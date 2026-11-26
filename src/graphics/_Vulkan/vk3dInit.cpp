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
    if(GN_Vk.hdrView == VK_NULL_HANDLE) {
        fprintf(stderr, "[GN_Vk] 警告: HDR 目标不可用，renderPass3D 的 resolve 附件将无效（L1）\n");
    }

    // ── 创建 3D Render Pass (3 附件: MSAA Color + MSAA Depth + Resolve) ──
    VkAttachmentDescription ads[3] = {};
    // [0] MSAA Color — 多重采样, 瞬态；L1：格式必须与 resolve 附件一致（HDR RGBA16F）
    ads[0].format = VK_FORMAT_R16G16B16A16_SFLOAT;
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
    // [2] Resolve — 单采样，L1：解析到 HDR 目标，供合成阶段曝光/色调映射/Bloom
    ads[2].format = VK_FORMAT_R16G16B16A16_SFLOAT;
    ads[2].samples = VK_SAMPLE_COUNT_1_BIT;
    ads[2].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    ads[2].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    ads[2].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    ads[2].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

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
        ii.format = VK_FORMAT_R16G16B16A16_SFLOAT; // L1：与 renderPass3D resolve 附件一致
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
        vi.format = VK_FORMAT_R16G16B16A16_SFLOAT; // L1：与 MSAA 颜色附件格式一致
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
        VkImageView atts[3] = {GN_Vk.msaaColorView, GN_Vk.depthView, GN_Vk.hdrView};
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
    // L3c：含 4 级 CSM 矩阵与分割数据，长度 = sizeof(GN3d::sLIGHT_UBO)（976 B）。
    CreateBuf(sizeof(GN3d::sLIGHT_UBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
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
        VkDescriptorBufferInfo lbi = {GN_Vk.lightBuffer3D, 0, sizeof(GN3d::sLIGHT_UBO)};
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
        VkDescriptorSetLayoutBinding dslb[13] = {};
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
        // L4：10/11 = 透射率 / sky-view LUT（与天空管线同一组 LUT）。
        for(int b = 10; b <= 11; ++b) {
            dslb[b].binding = (uint32_t)b;
            dslb[b].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            dslb[b].descriptorCount = 1;
            dslb[b].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        }
        // B2：binding 12 = 点光/聚光的 cube shadow（samplerCube）。
        // 为什么新开一个槽位而不是复用 binding 4：4 是 2D array（CSM，方向光）。
        // cube 与 2D array 是**不同的 image view 类型**，描述符类型上就不能互换。
        dslb[12].binding = 12;
        dslb[12].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        dslb[12].descriptorCount = 1;
        dslb[12].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        VkDescriptorSetLayoutCreateInfo dslci = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        dslci.bindingCount = 13;
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

namespace {
// L3c：4 级 CSM 的按层 framebuffer 与层视图（阴影图 = 2048²×4 层 D32）。
const uint32_t kShadowCascadeCount = 4;
VkFramebuffer s_shadowCascadeFramebuffers[kShadowCascadeCount] = {};
VkImageView s_shadowCascadeViews[kShadowCascadeCount] = {};

// 每级 light VP 独立 UBO 槽：64 B 矩阵，槽距 256 B（覆盖所有驱动
// minUniformBufferOffsetAlignment 的上限；单槽会被最后一级覆盖）。
const uint32_t kShadowUBOSlotSize = 64;
const uint32_t kShadowUBOSlotStride = 256;
// B2：CSM 用槽 0..3，cube shadow 的 6 个面用槽 4..9。
// **cube 的面必须各占一个槽**：只写槽 0 的话 6 面都会读到最后一次写入的矩阵 ——
// 这与 CSM 早期"4 级共用一个槽"是同一个 bug（当时的表现是所有级联都用最后一级）。
const uint32_t kCubeShadowSlotBase = 4;
// **每"灯-面"一个槽**：24 个 face 各占一个（4 + 24 = 28）。
//
// 为什么必须 24 个而不是 6 个：命令缓冲是**延迟执行**的。若 4 盏灯复用 6 个槽，
// 录制第 2 盏灯时会把槽改写，而第 1 盏灯的 6 个 render pass 还没执行 ——
// 最终所有灯的 24 个面都会读到最后一盏灯的矩阵（阴影全错位）。
// 这与 CSM 早期"4 级共用一个槽"是同一个 bug。
const uint32_t kShadowUBOSlotCount = kCubeShadowSlotBase + GN_CUBE_SHADOW_TOTAL_LAYERS;
VkDescriptorSet s_shadowCascadeSets[kShadowCascadeCount] = {};
// B2：cube shadow 6 面各一份（绑槽 kCubeShadowSlotBase + face）
VkDescriptorSet s_cubeShadowFaceSets[GN_CUBE_SHADOW_TOTAL_LAYERS] = {};
// 常驻映射（初始化期 vkMapMemory 一次）：每级只写自己的槽，避免逐级 map/unmap。
void *s_shadowUBOMapped = nullptr;
} // namespace

VkFramebuffer shadowFramebufferForCascade(uint32_t cascadeIndex) {
    if(cascadeIndex >= kShadowCascadeCount) cascadeIndex = 0;
    return s_shadowCascadeFramebuffers[cascadeIndex];
}

VkDescriptorSet shadowCascadeDescriptorSet(uint32_t cascadeIndex) {
    if(cascadeIndex >= kShadowCascadeCount) cascadeIndex = 0;
    return s_shadowCascadeSets[cascadeIndex];
}

// 把一帧某一级的 light VP 写进该级独立 UBO 槽（槽距 256 B，常驻映射）。
void writeShadowCascadeLightVP(uint32_t cascadeIndex, const float *matrix16) {
    if(!s_shadowUBOMapped || !matrix16) return;
    if(cascadeIndex >= kShadowCascadeCount) cascadeIndex = 0;
    memcpy((char *)s_shadowUBOMapped + (size_t)cascadeIndex * kShadowUBOSlotStride, matrix16, 64);
}

// 释放常驻映射（GN_VkShutdown3D 在销毁 shadowUBOMemory 之前调用）。
void releaseShadowCascadeUbo() {
    if(s_shadowUBOMapped) {
        vkUnmapMemory(GN_Vk.device, GN_Vk.shadowUBOMemory);
        s_shadowUBOMapped = nullptr;
    }
}

uint32_t GN_VkShadowCascadeCount() { return kShadowCascadeCount; }

VkDescriptorSet cubeShadowFaceDescriptorSet(uint32_t slot) {
    // slot = lightIndex * 6 + face（不是 face 本身）
    if(slot >= GN_CUBE_SHADOW_TOTAL_LAYERS) return VK_NULL_HANDLE;
    return s_cubeShadowFaceSets[slot];
}

void writeCubeShadowLightVP(uint32_t slot, const float *matrix16) {
    // slot = lightIndex * 6 + face（共 24 个）
    if(!s_shadowUBOMapped || !matrix16) return;
    if(slot >= GN_CUBE_SHADOW_TOTAL_LAYERS) return;
    // **每 slot 一个 UBO 槽**：命令缓冲延迟执行，复用槽会让 24 个面读到最后一个矩阵
    memcpy((char *)s_shadowUBOMapped +
               (size_t)(kCubeShadowSlotBase + slot) * kShadowUBOSlotStride,
           matrix16, 64);
}

void destroy3DShadowCascades() {
    for(uint32_t i = 0; i < kShadowCascadeCount; ++i) {
        if(s_shadowCascadeFramebuffers[i] && GN_Vk.device) {
            vkDestroyFramebuffer(GN_Vk.device, s_shadowCascadeFramebuffers[i], 0);
        }
        if(s_shadowCascadeViews[i] && GN_Vk.device) {
            vkDestroyImageView(GN_Vk.device, s_shadowCascadeViews[i], 0);
        }
        s_shadowCascadeFramebuffers[i] = VK_NULL_HANDLE;
        s_shadowCascadeViews[i] = VK_NULL_HANDLE;
    }
    GN_Vk.shadowFramebuffer = VK_NULL_HANDLE;
}

// 阴影映射：深度图像/视图/采样器、render pass、framebuffer、描述符集与管线。
void init3DShadow(const std::vector<uint32_t> &shadowVertSpv) {
    uint32_t smSize = GN_Vk.shadowMapSize;
    // 创建 Shadow Map 图像（L3c：4 层数组，每级一层）
    {
        VkImageCreateInfo ii = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        ii.imageType = VK_IMAGE_TYPE_2D;
        ii.extent = {smSize, smSize, 1};
        ii.mipLevels = 1;
        ii.arrayLayers = kShadowCascadeCount;
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
        vi.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        vi.format = VK_FORMAT_D32_SFLOAT;
        vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        vi.subresourceRange.levelCount = 1;
        vi.subresourceRange.layerCount = kShadowCascadeCount;
        vkCreateImageView(GN_Vk.device, &vi, 0, &GN_Vk.shadowView);
    }

    // Shadow sampler — L3b：PCSS 需要原始深度（阻塞物搜索），因此关闭 compare、用 NEAREST；
    // 比较在着色器里手动做（textureLod(...).r）。border=白 保证图外按“无遮挡”。
    {
        VkSamplerCreateInfo sci = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        sci.magFilter = VK_FILTER_NEAREST;
        sci.minFilter = VK_FILTER_NEAREST;
        sci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        sci.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        sci.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        sci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        sci.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
        sci.compareEnable = VK_FALSE;
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

    // L3c：每级一个 2D 层视图 + framebuffer（render pass 的层通过视图选择，不用 multiview）。
    destroy3DShadowCascades();
    for(uint32_t c = 0; c < kShadowCascadeCount; ++c) {
        VkImageViewCreateInfo lvi = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        lvi.image = GN_Vk.shadowImage;
        lvi.viewType = VK_IMAGE_VIEW_TYPE_2D;
        lvi.format = VK_FORMAT_D32_SFLOAT;
        lvi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        lvi.subresourceRange.levelCount = 1;
        lvi.subresourceRange.baseArrayLayer = c;
        lvi.subresourceRange.layerCount = 1;
        if(vkCreateImageView(GN_Vk.device, &lvi, 0, &s_shadowCascadeViews[c]) != VK_SUCCESS) continue;

        VkFramebufferCreateInfo fci = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        fci.renderPass = GN_Vk.shadowRenderPass;
        fci.attachmentCount = 1;
        fci.pAttachments = &s_shadowCascadeViews[c];
        fci.width = smSize;
        fci.height = smSize;
        fci.layers = 1;
        if(vkCreateFramebuffer(GN_Vk.device, &fci, 0, &s_shadowCascadeFramebuffers[c]) != VK_SUCCESS) {
            continue;
        }
    }
    // 兼容旧调用点（gpuCheck 读 GN_Vk.shadowFramebuffer / 层 0）
    GN_Vk.shadowFramebuffer = s_shadowCascadeFramebuffers[0];

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

    // Shadow UBO：每级一个独立槽（64 B 矩阵，槽距 256 B）。
    // 旧实现只有 64 B / 单槽：4 级 cascade 每级写同一位置，提交后全部读到最后一级矩阵。
    CreateBuf(kShadowUBOSlotCount * kShadowUBOSlotStride, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
              GN_Vk.shadowUBOBuffer, GN_Vk.shadowUBOMemory);
    // 常驻映射（offset 0 一定满足 minMemoryMapAlignment）；逐级只写自己的槽。
    {
        void *mapped = nullptr;
        if(vkMapMemory(GN_Vk.device, GN_Vk.shadowUBOMemory, 0, VK_WHOLE_SIZE, 0, &mapped) != VK_SUCCESS) {
            mapped = nullptr;
        }
        s_shadowUBOMapped = mapped;
    }

    // Shadow descriptor sets：每级一份，各自绑定本级的 64 B 槽。
    {
        for(uint32_t c = 0; c < kShadowCascadeCount; ++c) {
            s_shadowCascadeSets[c] = VK_NULL_HANDLE;
        }
        VkDescriptorSetLayout layouts[kShadowCascadeCount];
        for(uint32_t c = 0; c < kShadowCascadeCount; ++c) {
            layouts[c] = GN_Vk.shadowDescriptorLayout;
        }
        VkDescriptorSetAllocateInfo dai = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        dai.descriptorPool = GN_Vk.descriptorPool;
        dai.descriptorSetCount = kShadowCascadeCount;
        dai.pSetLayouts = layouts;
        VkResult res = vkAllocateDescriptorSets(GN_Vk.device, &dai, s_shadowCascadeSets);
        if(res != VK_SUCCESS) {
            VkCheck(res, "vkAllocateDescriptorSets shadow cascade");
            // 失败时数组内容未定义：全部置空，ShadowPassBegin 会跳过（不绑非法 set）。
            for(uint32_t c = 0; c < kShadowCascadeCount; ++c) {
                s_shadowCascadeSets[c] = VK_NULL_HANDLE;
            }
        } else {
            for(uint32_t c = 0; c < kShadowCascadeCount; ++c) {
                VkDescriptorBufferInfo dbi = {GN_Vk.shadowUBOBuffer, (VkDeviceSize)c * kShadowUBOSlotStride,
                                              kShadowUBOSlotSize};
                VkWriteDescriptorSet w = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
                w.dstSet = s_shadowCascadeSets[c];
                w.dstBinding = 0;
                w.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                w.descriptorCount = 1;
                w.pBufferInfo = &dbi;
                vkUpdateDescriptorSets(GN_Vk.device, 1, &w, 0, 0);
            }
        }
        // 兼容旧读取点：shadowDescriptorSet 指向第 0 级。
        GN_Vk.shadowDescriptorSet = s_shadowCascadeSets[0];
    }

    // B2：cube shadow 的 6 面各一份描述符集（绑槽 4..9）。
    {
        for(uint32_t f = 0; f < GN_CUBE_SHADOW_TOTAL_LAYERS; ++f)
            s_cubeShadowFaceSets[f] = VK_NULL_HANDLE;
        // 24 个（4 灯 × 6 面）
        VkDescriptorSetLayout layouts[GN_CUBE_SHADOW_TOTAL_LAYERS];
        for(uint32_t f = 0; f < GN_CUBE_SHADOW_TOTAL_LAYERS; ++f)
            layouts[f] = GN_Vk.shadowDescriptorLayout;
        VkDescriptorSetAllocateInfo dai = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        dai.descriptorPool = GN_Vk.descriptorPool;
        dai.descriptorSetCount = GN_CUBE_SHADOW_TOTAL_LAYERS;
        dai.pSetLayouts = layouts;
        VkResult res = vkAllocateDescriptorSets(GN_Vk.device, &dai, s_cubeShadowFaceSets);
        if(res != VK_SUCCESS) {
            VkCheck(res, "vkAllocateDescriptorSets cube shadow");
            for(uint32_t f = 0; f < GN_CUBE_SHADOW_TOTAL_LAYERS; ++f)
                s_cubeShadowFaceSets[f] = VK_NULL_HANDLE;
        } else {
            for(uint32_t f = 0; f < GN_CUBE_SHADOW_TOTAL_LAYERS; ++f) {
                VkDescriptorBufferInfo dbi = {
                    GN_Vk.shadowUBOBuffer,
                    (VkDeviceSize)(kCubeShadowSlotBase + f) * kShadowUBOSlotStride,
                    kShadowUBOSlotSize};
                VkWriteDescriptorSet w = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
                w.dstSet = s_cubeShadowFaceSets[f];
                w.dstBinding = 0;
                w.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                w.descriptorCount = 1;
                w.pBufferInfo = &dbi;
                vkUpdateDescriptorSets(GN_Vk.device, 1, &w, 0, 0);
            }
        }
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

    // B2：cube 专用的 depth-only 管线（动态 viewport）。
    // 与上面那条**同一个 render pass 结构**（都是单深度附件、同格式），但 viewport 走动态，
    // 因为 cube 是 1024 而 CSM 是 2048 —— 复用会让渲染缩到图幅右下角（实测踩过）。
    if(!shadowVertSpv.empty() && GN_Vk.cubeShadowRenderPass != VK_NULL_HANDLE) {
        GN3d::sVERTEX_LAYOUT layoutCubeShadow;
        layoutCubeShadow.Add(0, (uint32_t)offsetof(VkVertex3D, px), VK_FORMAT_R32G32B32_SFLOAT);
        layoutCubeShadow.aStride = (uint32_t)sizeof(VkVertex3D);

        GN3d::sPIPELINE_DESC descCube;
        descCube.aKind = GN3d::ePIPE_GRAPHICS;
        descCube.aVertSpv = shadowVertSpv.data();
        descCube.aVertSize = shadowVertSpv.size() * sizeof(uint32_t);
        descCube.aFragSpv = 0;
        descCube.aFragSize = 0;
        descCube.aVertexLayout = layoutCubeShadow;
        descCube.aRenderPass = GN_Vk.cubeShadowRenderPass; // 注意：不是 shadowRenderPass
        descCube.aSubpass = 0;
        descCube.aLayout = GN_Vk.shadowPipelineLayout;
        descCube.aDepthTest = true;
        descCube.aDepthWrite = true;
        descCube.aDepthCompare = VK_COMPARE_OP_LESS;
        descCube.aDepthStencil = true;
        descCube.aDepthBiasEnable = true;
        descCube.aDepthBiasConstant = 1.25f;
        descCube.aDepthBiasSlope = 1.75f;
        descCube.aCullBack = true;
        descCube.aFrontFace = VK_FRONT_FACE_CLOCKWISE;
        descCube.aBlend = GN3d::eBLEND_NONE;
        descCube.aTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        descCube.aSamples = VK_SAMPLE_COUNT_1_BIT;
        descCube.aColorAttachmentCount = 0;
        // **必须显式给静态 viewport**：实测 aDynamicViewport=true + vkCmdSetViewport
        // 在这条路径上不生效（把 viewport 改成一半尺寸，渲染出的像素数一点没变）。
        // 保留 dynamicState 声明（让 SetViewport 仍然合法），但按 cube 尺寸写死。
        const uint32_t cubeSize = GN_Vk.cubeShadowSize > 0 ? GN_Vk.cubeShadowSize : 1024u;
        descCube.aDynamicViewport = true;
        descCube.aDynamicScissor = true;
        descCube.aViewport = VkViewport{0.0f, 0.0f, (float)cubeSize, (float)cubeSize, 0.0f, 1.0f};
        descCube.aScissor = VkRect2D{{0, 0}, {cubeSize, cubeSize}};
        descCube.aCustomBlend = false;
        GN_Vk.cubeShadowPipeline = GN_Vk.aPipelineCache.GetOrCreate(descCube);
        if(GN_Vk.cubeShadowPipeline == VK_NULL_HANDLE) {
            fprintf(stderr, "[GN_Vk] cube shadow 管线创建失败: %s\n",
                    GN_Vk.aPipelineCache.GetLastError());
        }
    }
}

// ═══════════════════════════════════════════════════════════
//  B2：点光/聚光 cube shadow（单个光源，6 面）
// ═══════════════════════════════════════════════════════════
// 为什么是"单个光源"：每盏点光都要 6 面 × N² 的深度图，8 盏光就是 48 张。
// 现实的取舍是"当前最重要的一盏"（火焰场景就是主火炬）—— 这也是多数实时引擎
// 在移动端/中端配置下的默认策略。需要多盏时把这个结构扩展成数组即可，接口不用变。
//
// 与 CSM 完全独立：CSM 是方向光的 2D array（4 层），cube 是点光的 6 面。
// 两者共用一张图会让渲染时机互相踩（CSM 在帧首按相机拟合，cube 要按点光位置）。
void init3DCubeShadow(uint32_t size) {
    if(size < 128u) size = 128u;
    if(size > 2048u) size = 2048u;
    if(GN_Vk.device == VK_NULL_HANDLE) return;

    VkImageCreateInfo ii = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    ii.imageType = VK_IMAGE_TYPE_2D;
    ii.extent = {size, size, 1};
    ii.mipLevels = 1;
    // **cube array**：每盏灯 6 层，共 6*N 层。
    // 这样一帧内可以给多盏灯的 cube shadow 各存一份，而不是后渲染的覆盖前面的。
    ii.arrayLayers = GN_CUBE_SHADOW_TOTAL_LAYERS;
    ii.format = VK_FORMAT_D32_SFLOAT;
    ii.tiling = VK_IMAGE_TILING_OPTIMAL;
    ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    ii.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    ii.samples = VK_SAMPLE_COUNT_1_BIT;
    ii.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    // **这个标志是 cube 视图的硬性要求**（VUID-VkImageViewCreateInfo-image-01003）：
    // 少了它在 vkCreateImageView(VIEW_TYPE_CUBE) 时会直接校验失败。
    ii.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    if(vkCreateImage(GN_Vk.device, &ii, 0, &GN_Vk.cubeShadowImage) != VK_SUCCESS) {
        fprintf(stderr, "[GN_Vk] cube shadow image 创建失败（点光阴影关闭）\n");
        GN_Vk.cubeShadowImage = VK_NULL_HANDLE;
        return;
    }
    VkMemoryRequirements mr;
    vkGetImageMemoryRequirements(GN_Vk.device, GN_Vk.cubeShadowImage, &mr);
    VkMemoryAllocateInfo mai = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    mai.allocationSize = mr.size;
    mai.memoryTypeIndex = FindMemType(mr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if(vkAllocateMemory(GN_Vk.device, &mai, 0, &GN_Vk.cubeShadowMemory) != VK_SUCCESS) {
        vkDestroyImage(GN_Vk.device, GN_Vk.cubeShadowImage, 0);
        GN_Vk.cubeShadowImage = VK_NULL_HANDLE;
        return;
    }
    vkBindImageMemory(GN_Vk.device, GN_Vk.cubeShadowImage, GN_Vk.cubeShadowMemory, 0);

    // CUBE_ARRAY 视图（采样）。layerCount 必须是**整个** 6*N：
    // CUBE_ARRAY 的每个"层"是一个完整立方图，索引就是灯号。
    {
        VkImageViewCreateInfo vi = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        vi.image = GN_Vk.cubeShadowImage;
        vi.viewType = VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
        vi.format = VK_FORMAT_D32_SFLOAT;
        vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        vi.subresourceRange.levelCount = 1;
        vi.subresourceRange.layerCount = GN_CUBE_SHADOW_TOTAL_LAYERS;
        vkCreateImageView(GN_Vk.device, &vi, 0, &GN_Vk.cubeShadowView);
    }
    // 每"灯-面"一个 2D 视图（渲染目标）。索引 = light * 6 + face。
    // 注意：渲染到 cubearray 的某一层**必须**经 2D(1 层) 视图，
    // 不能直接用 CUBE_ARRAY 视图当 attachment（那样 layerCount 对不上）。
    for(uint32_t i = 0; i < GN_CUBE_SHADOW_TOTAL_LAYERS; ++i) {
        VkImageViewCreateInfo vi = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        vi.image = GN_Vk.cubeShadowImage;
        vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
        vi.format = VK_FORMAT_D32_SFLOAT;
        vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        vi.subresourceRange.levelCount = 1;
        vi.subresourceRange.baseArrayLayer = i;
        vi.subresourceRange.layerCount = 1;
        vkCreateImageView(GN_Vk.device, &vi, 0, &GN_Vk.cubeShadowFaceView[i]);
    }

    // 采样器：手动比较（与 CSM 一致，PCSS 需要原始深度），border=白 = 图外算"无遮挡"
    {
        VkSamplerCreateInfo sci = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        sci.magFilter = VK_FILTER_LINEAR;
        sci.minFilter = VK_FILTER_LINEAR;
        sci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        sci.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        sci.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        sci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        sci.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
        sci.compareEnable = VK_FALSE;
        vkCreateSampler(GN_Vk.device, &sci, 0, &GN_Vk.cubeShadowSampler);
    }

    // depth-only render pass（与 CSM 同格式，但两者仍各自建一份：将来要给 cube 加
    // 不同的 loadOp/清屏色时不用改 CSM）
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
        vkCreateRenderPass(GN_Vk.device, &rpi, 0, &GN_Vk.cubeShadowRenderPass);
    }

    // 每"灯-面"一个 framebuffer（共 6*N）
    for(uint32_t i = 0; i < GN_CUBE_SHADOW_TOTAL_LAYERS; ++i) {
        VkFramebufferCreateInfo fci = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        fci.renderPass = GN_Vk.cubeShadowRenderPass;
        fci.attachmentCount = 1;
        fci.pAttachments = &GN_Vk.cubeShadowFaceView[i];
        fci.width = size;
        fci.height = size;
        fci.layers = 1;
        vkCreateFramebuffer(GN_Vk.device, &fci, 0, &GN_Vk.cubeShadowFramebuffer[i]);
    }
    GN_Vk.cubeShadowSize = size;
    GN_Vk.cubeShadowPipeline = VK_NULL_HANDLE;
    fprintf(stderr, "[GN_Vk] cube shadow 就绪: %u x %u x %d (每灯 6 面 \u00d7 %d 盏)\n",
            size, size, GN_CUBE_SHADOW_TOTAL_LAYERS, GN_MAX_CUBE_SHADOW_LIGHTS);
    GN_Vk.cubeShadowLightCount = 0;
}

void destroy3DCubeShadow() {
    if(GN_Vk.device == VK_NULL_HANDLE) return;
    for(uint32_t i = 0; i < GN_CUBE_SHADOW_TOTAL_LAYERS; ++i) {
        if(GN_Vk.cubeShadowFramebuffer[i]) {
            vkDestroyFramebuffer(GN_Vk.device, GN_Vk.cubeShadowFramebuffer[i], 0);
            GN_Vk.cubeShadowFramebuffer[i] = VK_NULL_HANDLE;
        }
        if(GN_Vk.cubeShadowFaceView[i]) {
            vkDestroyImageView(GN_Vk.device, GN_Vk.cubeShadowFaceView[i], 0);
            GN_Vk.cubeShadowFaceView[i] = VK_NULL_HANDLE;
        }
    }
    if(GN_Vk.cubeShadowRenderPass) {
        vkDestroyRenderPass(GN_Vk.device, GN_Vk.cubeShadowRenderPass, 0);
        GN_Vk.cubeShadowRenderPass = VK_NULL_HANDLE;
    }
    if(GN_Vk.cubeShadowSampler) {
        vkDestroySampler(GN_Vk.device, GN_Vk.cubeShadowSampler, 0);
        GN_Vk.cubeShadowSampler = VK_NULL_HANDLE;
    }
    if(GN_Vk.cubeShadowView) {
        vkDestroyImageView(GN_Vk.device, GN_Vk.cubeShadowView, 0);
        GN_Vk.cubeShadowView = VK_NULL_HANDLE;
    }
    if(GN_Vk.cubeShadowMemory) {
        vkFreeMemory(GN_Vk.device, GN_Vk.cubeShadowMemory, 0);
        GN_Vk.cubeShadowMemory = VK_NULL_HANDLE;
    }
    if(GN_Vk.cubeShadowImage) {
        vkDestroyImage(GN_Vk.device, GN_Vk.cubeShadowImage, 0);
        GN_Vk.cubeShadowImage = VK_NULL_HANDLE;
    }
    GN_Vk.cubeShadowSize = 0;
}

// HDR 离屏目标 + 合成管线：HDR 图像/视图/采样器、HDR render pass 与 framebuffer、
// 合成（色调映射）管线与描述符集。

// ═══════════════════════════════════════════════════════════
//  3D 渲染管线实现 (GN_USING_3D)
// ═══════════════════════════════════════════════════════════
#ifdef GN_USING_3D

#include "gn3d_vert_spv.h"
#include "gn3d_frag_spv.h"
#include "gn3d_pbr_vert_spv.h"
#include "gn3d_pbr_frag_spv.h"
#include "gn3d_shadow_vert_spv.h"

// — 渲染通道追踪 (所有 pass 共用, 需在 #ifdef GN_USING_3D 外声明) —

// — 3D pass 状态（定义在本文件，供 vk3d* 模块共享） —
bool s_in3DPass = false;
VkFramebuffer *s_framebuffers3D = nullptr;

// — 阴影 pass 状态 —
// s_inShadowPass 定义在 vk3dPasses.cpp

// ── 3D 初始化编排：各阶段实现见 vk3dInit.cpp ──
void GN_VkInit3D() {
    // L1：renderPass3D 的 resolve 附件是 HDR 目标，必须先于 init3DTargets 创建 hdrView。
    create3DHdrImages();
    init3DTargets();
    // P11-S3：雾 UBO 最先创建（前向/PBR/延迟三条管线的描述符集都要绑定它）。
    if(!init3DAtmosphere()) fprintf(stderr, "[GN_Vk] 警告: 雾 UBO 不可用（P11-S3）\n");

    // ── 加载 3D 着色器 ───────────────────────────────
    auto vertSpv = LoadSPIRVFile("assets/shaders/gn3d.vert.spv");
    auto fragSpv = LoadSPIRVFile("assets/shaders/gn3d.frag.spv");
    if(vertSpv.empty() || fragSpv.empty()) {
        // 回退到嵌入式 SPIR-V
        if(vertSpv.empty()) vertSpv.assign(assets_shaders_gn3d_vert_spv,
                                           assets_shaders_gn3d_vert_spv + assets_shaders_gn3d_vert_spv_len);
        if(fragSpv.empty()) fragSpv.assign(assets_shaders_gn3d_frag_spv,
                                           assets_shaders_gn3d_frag_spv + assets_shaders_gn3d_frag_spv_len);
    }

    init3DForward(vertSpv, fragSpv);

    // PBR 着色器加载（含嵌入式回退）
    auto pbrVert = LoadSPIRVFile("assets/shaders/gn3d_pbr.vert.spv");
    auto pbrFrag = LoadSPIRVFile("assets/shaders/gn3d_pbr.frag.spv");
    if(pbrVert.empty()) pbrVert.assign(assets_shaders_gn3d_pbr_vert_spv,
                                       assets_shaders_gn3d_pbr_vert_spv + assets_shaders_gn3d_pbr_vert_spv_len);
    if(pbrFrag.empty()) pbrFrag.assign(assets_shaders_gn3d_pbr_frag_spv,
                                       assets_shaders_gn3d_pbr_frag_spv + assets_shaders_gn3d_pbr_frag_spv_len);
    init3DPbr(pbrVert, pbrFrag);

    // ═══════════════════════════════════════════════════════
    //  阴影映射系统
    // ═══════════════════════════════════════════════════════
    // Shadow 着色器加载（含嵌入式回退）
    auto svSpv = LoadSPIRVFile("assets/shaders/gn3d_shadow.vert.spv");
    if(svSpv.empty()) svSpv.assign(assets_shaders_gn3d_shadow_vert_spv,
                                   assets_shaders_gn3d_shadow_vert_spv + assets_shaders_gn3d_shadow_vert_spv_len);
    // B2：cube shadow 的资源（image/view/sampler/pass/framebuffer）必须先建，
    // 因为 init3DShadow 里会用 cubeShadowRenderPass 建 cube 专用管线。
    init3DCubeShadow(1024);
    init3DShadow(svSpv); // 内部创建 shadowPipelineLayout + shadowPipeline + cubeShadowPipeline
    // cube shadow 的 image/pass/framebuffer 要早于上面的管线创建，
    // 但管线依赖 pass ⇒ 顺序是：先 init3DCubeShadow 的"资源"，再建管线。
    // 为避免拆分函数，这里把 init3DCubeShadow 放在 init3DShadow 之前：
    // 它只建 image/view/sampler/pass/framebuffer，不碰管线。


    // ── 创建 PBR 描述符集 (必须在 Shadow Map 创建之后) ──
    {
        VkDescriptorSetAllocateInfo dai = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        dai.descriptorPool = GN_Vk.descriptorPool;
        dai.descriptorSetCount = 1;
        dai.pSetLayouts = &GN_Vk.descriptorLayoutPBR;
        vkAllocateDescriptorSets(GN_Vk.device, &dai, &GN_Vk.descriptorSetPBR);

        VkDescriptorBufferInfo dbiScene = {GN_Vk.uniformBuffer3D, 0, sizeof(VkSceneUBO)};
        VkDescriptorBufferInfo dbiLight = {GN_Vk.lightBuffer3D, 0, sizeof(GN3d::sLIGHT_UBO)};
        VkDescriptorImageInfo diiTex = {GN_Vk.defaultSampler, GN_Vk.defaultImageView,
                                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        VkDescriptorImageInfo diiNormal = {GN_Vk.defaultSampler, GN_Vk.defaultImageView,
                                           VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        VkDescriptorImageInfo diiShadow = {GN_Vk.shadowSampler, GN_Vk.shadowView,
                                           VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL};

        // B2：binding 12 = cube shadow（点光/聚光）
        VkDescriptorImageInfo diiCubeShadow = {GN_Vk.cubeShadowSampler, GN_Vk.cubeShadowView,
                                               VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL};
        VkWriteDescriptorSet w[7] = {{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET},
                                     {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET},
                                     {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET},
                                     {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET},
                                     {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET},
                                     {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET},
                                     {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET}};
        w[0].dstSet = GN_Vk.descriptorSetPBR;
        w[0].dstBinding = 0;
        w[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        w[0].descriptorCount = 1;
        w[0].pBufferInfo = &dbiScene;
        w[1].dstSet = GN_Vk.descriptorSetPBR;
        w[1].dstBinding = 1;
        w[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        w[1].descriptorCount = 1;
        w[1].pImageInfo = &diiTex;
        w[2].dstSet = GN_Vk.descriptorSetPBR;
        w[2].dstBinding = 2;
        w[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        w[2].descriptorCount = 1;
        w[2].pBufferInfo = &dbiLight;
        w[3].dstSet = GN_Vk.descriptorSetPBR;
        w[3].dstBinding = 3;
        w[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        w[3].descriptorCount = 1;
        w[3].pImageInfo = &diiNormal;
        w[4].dstSet = GN_Vk.descriptorSetPBR;
        w[4].dstBinding = 4;
        w[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        w[4].descriptorCount = 1;
        w[4].pImageInfo = &diiShadow;
        // P11-S3：binding 6 = 雾 UBO（binding 5 留给天空盒 cubemap，在天空盒初始化后更新）。
        VkDescriptorBufferInfo dbiFog = {atmosphereFogBuffer(), 0, atmosphereFogRange()};
        w[5].dstSet = GN_Vk.descriptorSetPBR;
        w[5].dstBinding = 6;
        w[5].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        w[5].descriptorCount = 1;
        w[5].pBufferInfo = &dbiFog;
        w[6].dstSet = GN_Vk.descriptorSetPBR;
        w[6].dstBinding = 12;
        w[6].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        w[6].descriptorCount = 1;
        w[6].pImageInfo = &diiCubeShadow;
        vkUpdateDescriptorSets(GN_Vk.device, 7, w, 0, 0);
        // Binding 5 (skybox cubemap) 将在天空盒初始化后更新
    }

    init3DHdr();
    // L1d：Bloom mip 链（依赖 hdrView 与 composite 描述符集）。
    if(!init3DBloom()) fprintf(stderr, "[GN_Vk] 警告: Bloom 资源不可用（L1d）\n");
    // L1：POST UBO（曝光/色调映射/Bloom）+ composite 描述符 binding 2。
    init3DPost();
    // L4c：CPU 生成透射率/sky-view LUT 并上传 GPU（天空 shader 接线前先就绪）。
    if(!init3DAtmosphereLut()) fprintf(stderr, "[GN_Vk] 警告: 大气 LUT 不可用（L4c）\n");

    init3DSkybox();
    // G1a：环境来源方案 B——GPU 6 面烘焙解析天空，回读后 CPU 预滤波并上传 GI 资源。
    init3DEnvironment();
    init3DGbuffer();
    init3DDeferredLight();
    // L4：LUT 就绪后补写天空/PBR/延迟描述符的 2/3、10/11、24/25 槽位。
    wireAtmosphereDescriptors();
    // G0b：G-Buffer 四通道调试视图（复用延迟 render pass/描述符集；管线归 aPipelineCache）。
    init3DGbufferDebug();
    // G2b：SSAO 资源（默认关；依赖 G-Buffer 视图与延迟描述符集）。
    init3DSsao();
    // G4：SSGI 资源（默认关；依赖 G-Buffer 视图、延迟描述符集与 GI UBO）。
    init3DSsgi();
    // G5b-2：SH9 探针 9× 3D 纹理（固定尺寸；默认 weight=0，不采样）。
    init3DProbes();

    // P1c / P2 / P8：新增子系统（管线各自经 aPipelineCache 创建；失败只告警）
    if(!init3DMaterial()) fprintf(stderr, "[GN_Vk] 警告: 材质实例管线不可用（P1c）\n");
    if(!init3DSkinned()) fprintf(stderr, "[GN_Vk] 警告: GPU 蒙皮管线不可用（P2）\n");
    if(!init3DDebugLines()) fprintf(stderr, "[GN_Vk] 警告: 调试线框管线不可用（P8）\n");
    // P9：GPU 粒子（compute 模拟 + 实例化绘制）；SSBO 在 GN_VkParticleCreate 时分配。
    if(!init3DParticle()) fprintf(stderr, "[GN_Vk] 警告: GPU 粒子 compute 不可用（P9）\n");
    if(!init3DParticleDraw()) fprintf(stderr, "[GN_Vk] 警告: GPU 粒子绘制管线不可用（P9）\n");
    // P9 剩余片：SSBO 蒙皮（>128 关节）与 GPU 布料（管线预建，缓冲在各自 Create 时分配）。
    if(!init3DSkinnedSsbo()) fprintf(stderr, "[GN_Vk] 警告: SSBO 蒙皮管线不可用（P9）\n");
    if(!init3DCloth()) fprintf(stderr, "[GN_Vk] 警告: GPU 布料管线不可用（P9）\n");
    // G1b：三套描述符集与 G1a 资源均已存在，写入 GI 槽位（默认倍率 0 = 关闭，回退旧 IBL）。
    init3DGiBindings();
}

#endif // GN_USING_3D

} // namespace GNg
