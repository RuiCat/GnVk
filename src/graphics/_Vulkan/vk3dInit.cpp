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
    uint32_t sw = GN_Vk.swapchainExtent.width;
    uint32_t sh = GN_Vk.swapchainExtent.height;

    // ── 创建 MSAA 颜色缓冲 ───────────────────────────
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
        vkCreateImage(GN_Vk.device, &ii, 0, &GN_Vk.msaaColorImage);
        VkMemoryRequirements mr;
        vkGetImageMemoryRequirements(GN_Vk.device, GN_Vk.msaaColorImage, &mr);
        VkMemoryAllocateInfo mai = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        mai.allocationSize = mr.size;
        mai.memoryTypeIndex = FindMemType(mr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        vkAllocateMemory(GN_Vk.device, &mai, 0, &GN_Vk.msaaColorMemory);
        vkBindImageMemory(GN_Vk.device, GN_Vk.msaaColorImage, GN_Vk.msaaColorMemory, 0);
    }
    // MSAA color view
    {
        VkImageViewCreateInfo vi = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        vi.image = GN_Vk.msaaColorImage;
        vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
        vi.format = GN_Vk.swapchainFormat;
        vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        vi.subresourceRange.levelCount = 1;
        vi.subresourceRange.layerCount = 1;
        vkCreateImageView(GN_Vk.device, &vi, 0, &GN_Vk.msaaColorView);
    }

    // ── 创建 MSAA 深度缓冲 ───────────────────────────
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
        vkCreateImage(GN_Vk.device, &ii, 0, &GN_Vk.depthImage);
        VkMemoryRequirements dmr;
        vkGetImageMemoryRequirements(GN_Vk.device, GN_Vk.depthImage, &dmr);
        VkMemoryAllocateInfo dmai = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        dmai.allocationSize = dmr.size;
        dmai.memoryTypeIndex = FindMemType(dmr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        vkAllocateMemory(GN_Vk.device, &dmai, 0, &GN_Vk.depthMemory);
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
        vkCreateImageView(GN_Vk.device, &vi, 0, &GN_Vk.depthView);
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

    // 创建 3D framebuffers (3 附件: MSAA color + MSAA depth + swapchain resolve)
    s_framebuffers3D = new VkFramebuffer[GN_Vk.swapchainImageCount];
    for(uint32_t i = 0; i < GN_Vk.swapchainImageCount; i++) {
        VkImageView atts[3] = {GN_Vk.msaaColorView, GN_Vk.depthView, GN_Vk.swapchainViews[i]};
        VkFramebufferCreateInfo fci = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        fci.renderPass = GN_Vk.renderPass3D;
        fci.attachmentCount = 3;
        fci.pAttachments = atts;
        fci.width = sw;
        fci.height = sh;
        fci.layers = 1;
        vkCreateFramebuffer(GN_Vk.device, &fci, 0, &s_framebuffers3D[i]);
    }
}

// 前向 3D 管线：着色器模块、描述符布局、管线布局、图形管线、场景/光照 UBO、默认描述符集。
void init3DForward(const std::vector<uint32_t> &vertSpv,
                   const std::vector<uint32_t> &fragSpv) {
    VkShaderModule vm3d, fm3d;
    {
        VkShaderModuleCreateInfo sm = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        sm.codeSize = vertSpv.size() * sizeof(uint32_t);
        sm.pCode = vertSpv.data();
        vkCreateShaderModule(GN_Vk.device, &sm, 0, &vm3d);
    }
    {
        VkShaderModuleCreateInfo sm = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        sm.codeSize = fragSpv.size() * sizeof(uint32_t);
        sm.pCode = fragSpv.data();
        vkCreateShaderModule(GN_Vk.device, &sm, 0, &fm3d);
    }

    // ── 3D 描述符布局 ────────────────────────────────
    // Binding 0: Scene UBO (uniform buffer) — 顶点+片段着色器
    // Binding 1: Texture sampler (combined image sampler) — 片段着色器
    // Binding 2: Light UBO (uniform buffer) — 片段着色器
    {
        VkDescriptorSetLayoutBinding dslb[3] = {};
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
        VkDescriptorSetLayoutCreateInfo dslci = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        dslci.bindingCount = 3;
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

    // ── 3D 图形管线 ──────────────────────────────────
    {
        VkPipelineShaderStageCreateInfo ss[2] = {{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO}, {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO}};
        ss[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        ss[0].module = vm3d;
        ss[0].pName = "main";
        ss[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        ss[1].module = fm3d;
        ss[1].pName = "main";

        VkVertexInputBindingDescription vib = {0, sizeof(VkVertex3D), VK_VERTEX_INPUT_RATE_VERTEX};
        VkVertexInputAttributeDescription via[4] = {
            {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(VkVertex3D, px)}, // pos
            {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(VkVertex3D, nx)}, // normal
            {2, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(VkVertex3D, tx)}, // tangent
            {3, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(VkVertex3D, u)} // uv
        };
        VkPipelineVertexInputStateCreateInfo vis = {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        vis.vertexBindingDescriptionCount = 1;
        vis.pVertexBindingDescriptions = &vib;
        vis.vertexAttributeDescriptionCount = 4;
        vis.pVertexAttributeDescriptions = via;

        VkPipelineInputAssemblyStateCreateInfo ias = {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
        ias.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        VkViewport vp = {0, 0, (float)GN_Vk.swapchainExtent.width, (float)GN_Vk.swapchainExtent.height, 0, 1};
        VkRect2D sc = {{0, 0}, GN_Vk.swapchainExtent};
        VkPipelineViewportStateCreateInfo vps = {VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
        vps.viewportCount = 1;
        vps.pViewports = &vp;
        vps.scissorCount = 1;
        vps.pScissors = &sc;

        VkPipelineRasterizationStateCreateInfo rs = {VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
        rs.polygonMode = VK_POLYGON_MODE_FILL;
        rs.cullMode = VK_CULL_MODE_BACK_BIT;
        rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rs.lineWidth = 1.0f;
        // viewport Y翻转 + m[5]=-f 双翻抵消 → NDC CCW → COUNTER_CLOCKWISE=正面 ✓

        VkPipelineMultisampleStateCreateInfo ms = {VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
        ms.rasterizationSamples = GN_Vk.msaaSamples;

        VkPipelineDepthStencilStateCreateInfo dss = {VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
        dss.depthTestEnable = VK_TRUE;
        dss.depthWriteEnable = VK_TRUE;
        dss.depthCompareOp = VK_COMPARE_OP_LESS;

        VkPipelineColorBlendAttachmentState cba = {};
        cba.colorWriteMask = 0xf;
        cba.blendEnable = VK_FALSE; // 3D 不使用混合

        VkPipelineColorBlendStateCreateInfo cbs = {VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
        cbs.attachmentCount = 1;
        cbs.pAttachments = &cba;

        VkGraphicsPipelineCreateInfo pci = {VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
        pci.stageCount = 2;
        pci.pStages = ss;
        pci.pVertexInputState = &vis;
        pci.pInputAssemblyState = &ias;
        pci.pViewportState = &vps;
        pci.pRasterizationState = &rs;
        pci.pMultisampleState = &ms;
        pci.pDepthStencilState = &dss;
        pci.pColorBlendState = &cbs;
        pci.layout = GN_Vk.pipelineLayout3D;
        pci.renderPass = GN_Vk.renderPass3D;
        pci.subpass = 0;
        vkCreateGraphicsPipelines(GN_Vk.device, 0, 1, &pci, 0, &GN_Vk.pipeline3D);

        vkDestroyShaderModule(GN_Vk.device, vm3d, 0);
        vkDestroyShaderModule(GN_Vk.device, fm3d, 0);
    }

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
        VkDescriptorImageInfo dii = {GN_Vk.defaultSampler, GN_Vk.defaultImageView,
                                     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        VkWriteDescriptorSet w[3] = {{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET},
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
        vkUpdateDescriptorSets(GN_Vk.device, 3, w, 0, 0);
    }
}

// PBR 管线：描述符布局（含 IBL cubemap）、管线布局与图形管线。
void init3DPbr(const std::vector<uint32_t> &pbrVert,
               const std::vector<uint32_t> &pbrFrag) {
    // ═══════════════════════════════════════════════════════
    //  PBR 描述符布局 (Binding 5 = skyboxCubemap for IBL)
    // ═══════════════════════════════════════════════════════
    {
        VkDescriptorSetLayoutBinding dslb[6] = {};
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
        VkDescriptorSetLayoutCreateInfo dslci = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        dslci.bindingCount = 6;
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

    // ── 加载 PBR 着色器并创建管线 ───────────────────
    VkShaderModule pvm = 0, pfm = 0;
    if(!pbrVert.empty() && !pbrFrag.empty()) {
        {
            VkShaderModuleCreateInfo sm = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
            sm.codeSize = pbrVert.size() * sizeof(uint32_t);
            sm.pCode = pbrVert.data();
            vkCreateShaderModule(GN_Vk.device, &sm, 0, &pvm);
        }
        {
            VkShaderModuleCreateInfo sm = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
            sm.codeSize = pbrFrag.size() * sizeof(uint32_t);
            sm.pCode = pbrFrag.data();
            vkCreateShaderModule(GN_Vk.device, &sm, 0, &pfm);
        }

        VkPipelineShaderStageCreateInfo ss[2] = {{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO}, {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO}};
        ss[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        ss[0].module = pvm;
        ss[0].pName = "main";
        ss[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        ss[1].module = pfm;
        ss[1].pName = "main";

        VkVertexInputBindingDescription vib = {0, sizeof(VkVertex3D), VK_VERTEX_INPUT_RATE_VERTEX};
        VkVertexInputAttributeDescription via[4] = {
            {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(VkVertex3D, px)},
            {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(VkVertex3D, nx)},
            {2, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(VkVertex3D, tx)},
            {3, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(VkVertex3D, u)}};
        VkPipelineVertexInputStateCreateInfo vis = {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        vis.vertexBindingDescriptionCount = 1;
        vis.pVertexBindingDescriptions = &vib;
        vis.vertexAttributeDescriptionCount = 4;
        vis.pVertexAttributeDescriptions = via;

        VkPipelineInputAssemblyStateCreateInfo ias = {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
        ias.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        // viewport Y 翻转: y=H, height=-H 抵消投影 Y-flip, 使 NDC CCW 保持 CCW
        VkViewport vp = {0, (float)GN_Vk.swapchainExtent.height, (float)GN_Vk.swapchainExtent.width, -(float)GN_Vk.swapchainExtent.height, 0, 1};
        VkRect2D sc = {{0, 0}, GN_Vk.swapchainExtent};
        VkPipelineViewportStateCreateInfo vps = {VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
        vps.viewportCount = 1;
        vps.pViewports = &vp;
        vps.scissorCount = 1;
        vps.pScissors = &sc;
        VkPipelineRasterizationStateCreateInfo rs = {VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
        rs.polygonMode = VK_POLYGON_MODE_FILL;
        rs.cullMode = VK_CULL_MODE_BACK_BIT;
        rs.frontFace = VK_FRONT_FACE_CLOCKWISE;
        rs.lineWidth = 1.0f;
        VkPipelineMultisampleStateCreateInfo ms = {VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
        ms.rasterizationSamples = GN_Vk.msaaSamples;
        VkPipelineDepthStencilStateCreateInfo dss = {VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
        dss.depthTestEnable = VK_TRUE;
        dss.depthWriteEnable = VK_TRUE;
        dss.depthCompareOp = VK_COMPARE_OP_LESS;
        VkPipelineColorBlendAttachmentState cba = {};
        cba.colorWriteMask = 0xf;
        VkPipelineColorBlendStateCreateInfo cbs = {VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
        cbs.attachmentCount = 1;
        cbs.pAttachments = &cba;
        VkGraphicsPipelineCreateInfo pci = {VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
        pci.stageCount = 2;
        pci.pStages = ss;
        pci.pVertexInputState = &vis;
        pci.pInputAssemblyState = &ias;
        pci.pViewportState = &vps;
        pci.pRasterizationState = &rs;
        pci.pMultisampleState = &ms;
        pci.pDepthStencilState = &dss;
        pci.pColorBlendState = &cbs;
        pci.layout = GN_Vk.pipelineLayoutPBR;
        pci.renderPass = GN_Vk.renderPass3D;
        pci.subpass = 0;
        vkCreateGraphicsPipelines(GN_Vk.device, 0, 1, &pci, 0, &GN_Vk.pipelinePBR);

        vkDestroyShaderModule(GN_Vk.device, pvm, 0);
        vkDestroyShaderModule(GN_Vk.device, pfm, 0);
    }
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
        ii.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
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

    // Shadow pipeline
    VkShaderModule svm = 0;
    if(!shadowVertSpv.empty()) {
        {
            VkShaderModuleCreateInfo sm = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
            sm.codeSize = shadowVertSpv.size() * sizeof(uint32_t);
            sm.pCode = shadowVertSpv.data();
            vkCreateShaderModule(GN_Vk.device, &sm, 0, &svm);
        }
        VkPipelineShaderStageCreateInfo ss = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
        ss.stage = VK_SHADER_STAGE_VERTEX_BIT;
        ss.module = svm;
        ss.pName = "main";
        VkVertexInputBindingDescription vib = {0, sizeof(VkVertex3D), VK_VERTEX_INPUT_RATE_VERTEX};
        VkVertexInputAttributeDescription via = {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(VkVertex3D, px)};
        VkPipelineVertexInputStateCreateInfo vis = {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        vis.vertexBindingDescriptionCount = 1;
        vis.pVertexBindingDescriptions = &vib;
        vis.vertexAttributeDescriptionCount = 1;
        vis.pVertexAttributeDescriptions = &via;
        VkPipelineInputAssemblyStateCreateInfo ias = {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
        ias.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        VkViewport vp = {0, 0, (float)smSize, (float)smSize, 0, 1};
        VkRect2D sc = {{0, 0}, {smSize, smSize}};
        VkPipelineViewportStateCreateInfo vps = {VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
        vps.viewportCount = 1;
        vps.pViewports = &vp;
        vps.scissorCount = 1;
        vps.pScissors = &sc;
        VkPipelineRasterizationStateCreateInfo rs = {VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
        rs.polygonMode = VK_POLYGON_MODE_FILL;
        rs.cullMode = VK_CULL_MODE_BACK_BIT;
        rs.frontFace = VK_FRONT_FACE_CLOCKWISE;
        rs.lineWidth = 1.0f;
        rs.depthBiasEnable = VK_TRUE;
        rs.depthBiasConstantFactor = 1.25f;
        rs.depthBiasSlopeFactor = 1.75f;
        VkPipelineMultisampleStateCreateInfo ms = {VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
        ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        VkPipelineDepthStencilStateCreateInfo dss = {VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
        dss.depthTestEnable = VK_TRUE;
        dss.depthWriteEnable = VK_TRUE;
        dss.depthCompareOp = VK_COMPARE_OP_LESS;
        VkGraphicsPipelineCreateInfo pci = {VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
        pci.stageCount = 1;
        pci.pStages = &ss;
        pci.pVertexInputState = &vis;
        pci.pInputAssemblyState = &ias;
        pci.pViewportState = &vps;
        pci.pRasterizationState = &rs;
        pci.pMultisampleState = &ms;
        pci.pDepthStencilState = &dss;
        pci.layout = GN_Vk.shadowPipelineLayout;
        pci.renderPass = GN_Vk.shadowRenderPass;
        pci.subpass = 0;
        vkCreateGraphicsPipelines(GN_Vk.device, 0, 1, &pci, 0, &GN_Vk.shadowPipeline);
        vkDestroyShaderModule(GN_Vk.device, svm, 0);
    }
}

// HDR 离屏目标 + 合成管线：HDR 图像/视图/采样器、HDR render pass 与 framebuffer、
// 合成（色调映射）管线与描述符集。

} // namespace GNg
