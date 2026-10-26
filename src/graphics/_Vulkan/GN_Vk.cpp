/****************************************************************************
 *  GN_mini — Vulkan 后端主入口
 *  职责：设备/交换链初始化与关闭、窗口 resize、RenderTarget 辅助、
 *        sIMAGE 数据上传/填充、3D 初始化编排。
 *  拆出的模块：
 *    vkDevice.cpp   设备级辅助（SPIR-V 读取/内存/缓冲/视图）
 *    vkImageIo.cpp  纹理 PNG IO 与图集
 *    vkImage.cpp    sIMAGE 生命周期与回读
 *    vkFrame2d.cpp  2D 帧循环与批状态
 *    vkDraw2d.cpp   2D 绘制入口
 *    vk3d*.cpp      3D 网格/pass/前向/初始化/释放
 ****************************************************************************/
#include <GN.h>
#include "GN_Vk.h"
#include "vkInternal.h"
#include <cstdio>
#include <cstring>

namespace GNg {

GN_VkContext GN_Vk;

// ── RenderTarget 静态变量 (需在 shutdown 之前) ────────
static VkRenderPass s_rtRenderPass = VK_NULL_HANDLE;
static VkFramebuffer s_rtFramebuffer = VK_NULL_HANDLE;

// ── RenderTarget 状态（帧循环/批状态见 vkFrame2d.cpp） ──

// ViewCur 定义在 core（GN_RunRec.cpp 等），本文件不涉及

// ViewCur 定义在 core（GN_RunRec.cpp 等），本文件不涉及

// sIMAGE 补充方法
// ── 前向声明 (RenderTarget 辅助函数) ──────────────────
static void BeginRTRenderPass();
static void EndRTRenderPass();
extern sIMAGE *s_activeRT;
extern bool s_rtInProgress;

void sIMAGE::BeginRender(int msaa, bool is3d, bool isnew) {
    (void)msaa;
    (void)is3d;
    (void)isnew;
    if(!GN_Vk.initialized) return;
    if(!vkImage || width <= 0 || height <= 0) {
        if(!vkImage) CreateEmptyToData(width > 0 ? width : GN_Vk.viewWidth,
                                       height > 0 ? height : GN_Vk.viewHeight, 1, 0, 1, false, false);
    }
    if(!vkImage) return;
    s_activeRT = this;
    BeginRTRenderPass();
}

void sIMAGE::EndRender() {
    if(!GN_Vk.initialized || !s_rtInProgress) return;
    EndRTRenderPass();
}

void *sIMAGE::RenderTargetGet(sIMAGE *img) {
    if(img) return img;
    return s_activeRT;
}

// ═══════════════════════════════════════════════════════════
//  设备与交换链初始化（辅助函数见 vkDevice.cpp；2D/3D 分流见 vk*2d.cpp / vk3d*.cpp）
// ═══════════════════════════════════════════════════════════

void GN_VkInit(GLFWwindow *window, int w, int h) {
    if(GN_Vk.initialized) return;
    GN_Vk.viewWidth = w;
    GN_Vk.viewHeight = h;

    // Instance
    VkApplicationInfo ai = {VK_STRUCTURE_TYPE_APPLICATION_INFO};
    ai.pApplicationName = "GN_mini";
    ai.apiVersion = VK_API_VERSION_1_0;
    uint32_t extCount = 0;
    const char **exts = glfwGetRequiredInstanceExtensions(&extCount);
    VkInstanceCreateInfo ici = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    ici.pApplicationInfo = &ai;
    ici.enabledExtensionCount = extCount;
    ici.ppEnabledExtensionNames = exts;
    VkResult res = vkCreateInstance(&ici, 0, &GN_Vk.instance);
    VkCheck(res, "vkCreateInstance");
    if(res != VK_SUCCESS || GN_Vk.instance == VK_NULL_HANDLE) return;

    // Surface
    res = glfwCreateWindowSurface(GN_Vk.instance, window, 0, &GN_Vk.surface);
    VkCheck(res, "glfwCreateWindowSurface");
    if(res != VK_SUCCESS || GN_Vk.surface == VK_NULL_HANDLE) return;

    // Physical device
    uint32_t dc = 0;
    res = vkEnumeratePhysicalDevices(GN_Vk.instance, &dc, 0);
    if(res != VK_SUCCESS || dc == 0) {
        fprintf(stderr, "[GN_Vk] FATAL: 未找到可用的 Vulkan 物理设备 (err=%d, count=%u)\n", (int)res, dc);
        return;
    }
    VkPhysicalDevice *devs = new VkPhysicalDevice[dc];
    vkEnumeratePhysicalDevices(GN_Vk.instance, &dc, devs);
    GN_Vk.physicalDevice = devs[0];
    delete[] devs;

    // Queue family
    uint32_t qc = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(GN_Vk.physicalDevice, &qc, 0);
    VkQueueFamilyProperties *qps = new VkQueueFamilyProperties[qc];
    vkGetPhysicalDeviceQueueFamilyProperties(GN_Vk.physicalDevice, &qc, qps);
    bool queueFound = false;
    for(uint32_t i = 0; i < qc; i++) {
        VkBool32 pr;
        vkGetPhysicalDeviceSurfaceSupportKHR(GN_Vk.physicalDevice, i, GN_Vk.surface, &pr);
        if(qps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT && pr) {
            GN_Vk.graphicsFamily = i;
            queueFound = true;
            break;
        }
    }
    delete[] qps;
    if(!queueFound) {
        fprintf(stderr, "[GN_Vk] FATAL: 未找到支持图形+呈现的队列族\n");
        return;
    }

    // Device
    float qp = 1.0f;
    VkDeviceQueueCreateInfo qci = {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    qci.queueFamilyIndex = GN_Vk.graphicsFamily;
    qci.queueCount = 1;
    qci.pQueuePriorities = &qp;
    const char *dexts[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    VkDeviceCreateInfo dci = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    dci.queueCreateInfoCount = 1;
    dci.pQueueCreateInfos = &qci;
    dci.enabledExtensionCount = 1;
    dci.ppEnabledExtensionNames = dexts;
    VkCheck(vkCreateDevice(GN_Vk.physicalDevice, &dci, 0, &GN_Vk.device), "vkCreateDevice");
    vkGetDeviceQueue(GN_Vk.device, GN_Vk.graphicsFamily, 0, &GN_Vk.graphicsQueue);

    // Swapchain
    VkSurfaceCapabilitiesKHR caps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(GN_Vk.physicalDevice, GN_Vk.surface, &caps);
    GN_Vk.swapchainExtent = caps.currentExtent;
    if(GN_Vk.swapchainExtent.width == 0xffffffff || GN_Vk.swapchainExtent.width == 0) {
        GN_Vk.swapchainExtent.width = w > 0 ? w : 800;
        GN_Vk.swapchainExtent.height = h > 0 ? h : 600;
    }
    // Query supported formats
    uint32_t fmtCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(GN_Vk.physicalDevice, GN_Vk.surface, &fmtCount, 0);
    VkSurfaceFormatKHR *fmts = new VkSurfaceFormatKHR[fmtCount];
    vkGetPhysicalDeviceSurfaceFormatsKHR(GN_Vk.physicalDevice, GN_Vk.surface, &fmtCount, fmts);
    GN_Vk.swapchainFormat = fmts[0].format;
    delete[] fmts;
    if(GN_Vk.swapchainFormat == VK_FORMAT_UNDEFINED) GN_Vk.swapchainFormat = VK_FORMAT_B8G8R8A8_SRGB;
    VkSwapchainCreateInfoKHR sci = {VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    sci.surface = GN_Vk.surface;
    sci.minImageCount = 2;
    sci.imageFormat = GN_Vk.swapchainFormat;
    sci.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    sci.imageExtent = GN_Vk.swapchainExtent;
    sci.imageArrayLayers = 1;
    sci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    sci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    sci.preTransform = caps.currentTransform;
    sci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    sci.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    sci.clipped = VK_TRUE;
    VkCheck(vkCreateSwapchainKHR(GN_Vk.device, &sci, 0, &GN_Vk.swapchain), "vkCreateSwapchainKHR");
    vkGetSwapchainImagesKHR(GN_Vk.device, GN_Vk.swapchain, &GN_Vk.swapchainImageCount, 0);
    GN_Vk.swapchainImages = new VkImage[GN_Vk.swapchainImageCount];
    GN_Vk.swapchainViews = new VkImageView[GN_Vk.swapchainImageCount];
    vkGetSwapchainImagesKHR(GN_Vk.device, GN_Vk.swapchain, &GN_Vk.swapchainImageCount, GN_Vk.swapchainImages);
    for(uint32_t i = 0; i < GN_Vk.swapchainImageCount; i++) GN_Vk.swapchainViews[i] = MakeImageView(GN_Vk.swapchainImages[i], GN_Vk.swapchainFormat);

    // Render pass
    VkAttachmentDescription ad = {};
    ad.format = GN_Vk.swapchainFormat;
    ad.samples = VK_SAMPLE_COUNT_1_BIT;
    ad.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    ad.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    ad.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    ad.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    VkAttachmentReference ar = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkSubpassDescription sp = {};
    sp.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    sp.colorAttachmentCount = 1;
    sp.pColorAttachments = &ar;
    VkRenderPassCreateInfo rpi = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    rpi.attachmentCount = 1;
    rpi.pAttachments = &ad;
    rpi.subpassCount = 1;
    rpi.pSubpasses = &sp;
    VkCheck(vkCreateRenderPass(GN_Vk.device, &rpi, 0, &GN_Vk.renderPass), "vkCreateRenderPass");

// Shaders — 必须从 .spv 文件加载 (make shaders 编译 GLSL→SPIR-V)
#include "gn2d_vert_spv.h"
#include "gn2d_frag_spv.h"
    VkShaderModuleCreateInfo sm = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    VkShaderModule vm, fm;

    auto vertSpv = LoadSPIRVFile("assets/shaders/gn2d.vert.spv");
    auto fragSpv = LoadSPIRVFile("assets/shaders/gn2d.frag.spv");

    if(vertSpv.empty()) {
        fprintf(stderr, "[GN_Vk] FATAL: 无法加载着色器 assets/shaders/gn2d.vert.spv\n");
        return;
    }
    if(fragSpv.empty()) {
        fprintf(stderr, "[GN_Vk] FATAL: 无法加载着色器 assets/shaders/gn2d.frag.spv\n");
        return;
    }
    sm.codeSize = vertSpv.size() * sizeof(uint32_t);
    sm.pCode = vertSpv.data();
    vkCreateShaderModule(GN_Vk.device, &sm, 0, &vm);

    sm.codeSize = fragSpv.size() * sizeof(uint32_t);
    sm.pCode = fragSpv.data();
    vkCreateShaderModule(GN_Vk.device, &sm, 0, &fm);

    // Pipeline
    VkPipelineShaderStageCreateInfo ss[2] = {{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO}, {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO}};
    ss[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    ss[0].module = vm;
    ss[0].pName = "main";
    ss[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    ss[1].module = fm;
    ss[1].pName = "main";

    VkVertexInputBindingDescription vib = {0, sizeof(VkVertex), VK_VERTEX_INPUT_RATE_VERTEX};
    VkVertexInputAttributeDescription via[4] = {{0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(VkVertex, x)}, {1, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(VkVertex, u)}, {2, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(VkVertex, r)}};
    VkPipelineVertexInputStateCreateInfo vis = {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vis.vertexBindingDescriptionCount = 1;
    vis.pVertexBindingDescriptions = &vib;
    vis.vertexAttributeDescriptionCount = 3;
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
    rs.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rs.lineWidth = 1.0f;
    VkPipelineMultisampleStateCreateInfo ms = {VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    VkPipelineColorBlendAttachmentState cba = {};
    cba.blendEnable = VK_TRUE;
    cba.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    cba.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    cba.colorBlendOp = VK_BLEND_OP_ADD;
    cba.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    cba.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    cba.colorWriteMask = 0xf;
    VkPipelineColorBlendStateCreateInfo cbs = {VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    cbs.attachmentCount = 1;
    cbs.pAttachments = &cba;

    // Dynamic state: scissor (for UI clipping)
    VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dsci = {VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dsci.dynamicStateCount = 1;
    dsci.pDynamicStates = dynamicStates;

    VkDescriptorSetLayoutBinding dslb[2] = {};
    dslb[0].binding = 0;
    dslb[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    dslb[0].descriptorCount = 1;
    dslb[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    dslb[1].binding = 1;
    dslb[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    dslb[1].descriptorCount = 1;
    dslb[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    VkDescriptorSetLayoutCreateInfo dslci = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    dslci.bindingCount = 2;
    dslci.pBindings = dslb;
    VkCheck(vkCreateDescriptorSetLayout(GN_Vk.device, &dslci, 0, &GN_Vk.descriptorLayout), "vkCreateDescriptorSetLayout");
    VkPipelineLayoutCreateInfo plci = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    plci.setLayoutCount = 1;
    plci.pSetLayouts = &GN_Vk.descriptorLayout;
    VkCheck(vkCreatePipelineLayout(GN_Vk.device, &plci, 0, &GN_Vk.pipelineLayout), "vkCreatePipelineLayout");

    VkGraphicsPipelineCreateInfo pci = {VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    pci.stageCount = 2;
    pci.pStages = ss;
    pci.pVertexInputState = &vis;
    pci.pInputAssemblyState = &ias;
    pci.pViewportState = &vps;
    pci.pRasterizationState = &rs;
    pci.pMultisampleState = &ms;
    pci.pColorBlendState = &cbs;
    pci.pDynamicState = &dsci;
    pci.layout = GN_Vk.pipelineLayout;
    pci.renderPass = GN_Vk.renderPass;
    pci.subpass = 0;
    VkCheck(vkCreateGraphicsPipelines(GN_Vk.device, 0, 1, &pci, 0, &GN_Vk.pipeline), "vkCreateGraphicsPipelines");
    // Additive blend pipeline (also needs dynamic state)
    pci.pDynamicState = &dsci;
    // Additive blend pipeline (eBLEND_BRIGHT)
    {
        VkPipelineColorBlendAttachmentState cbaAdd = cba;
        cbaAdd.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
        VkPipelineColorBlendStateCreateInfo cbsAdd = cbs;
        cbsAdd.pAttachments = &cbaAdd;
        pci.pColorBlendState = &cbsAdd;
        vkCreateGraphicsPipelines(GN_Vk.device, 0, 1, &pci, 0, &GN_Vk.pipelineBright);
    }
    // Multiply blend pipeline (eBLEND_MODULATE = 11)
    {
        VkPipelineColorBlendAttachmentState cbaMul = cba;
        cbaMul.srcColorBlendFactor = VK_BLEND_FACTOR_DST_COLOR;
        cbaMul.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
        VkPipelineColorBlendStateCreateInfo cbsMul = cbs;
        cbsMul.pAttachments = &cbaMul;
        pci.pColorBlendState = &cbsMul;
        vkCreateGraphicsPipelines(GN_Vk.device, 0, 1, &pci, 0, &GN_Vk.pipelineMultiply);
    }
    // No-blend pipeline (eBLEND_NULL = 0)
    {
        VkPipelineColorBlendAttachmentState cbaNull = cba;
        cbaNull.blendEnable = VK_FALSE;
        VkPipelineColorBlendStateCreateInfo cbsNull = cbs;
        cbsNull.pAttachments = &cbaNull;
        pci.pColorBlendState = &cbsNull;
        vkCreateGraphicsPipelines(GN_Vk.device, 0, 1, &pci, 0, &GN_Vk.pipelineNull);
    }
    vkDestroyShaderModule(GN_Vk.device, vm, 0);
    vkDestroyShaderModule(GN_Vk.device, fm, 0);

    // Framebuffers
    GN_Vk.framebuffers = new VkFramebuffer[GN_Vk.swapchainImageCount];
    for(uint32_t i = 0; i < GN_Vk.swapchainImageCount; i++) {
        VkFramebufferCreateInfo fci = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        fci.renderPass = GN_Vk.renderPass;
        fci.attachmentCount = 1;
        fci.pAttachments = &GN_Vk.swapchainViews[i];
        fci.width = GN_Vk.swapchainExtent.width;
        fci.height = GN_Vk.swapchainExtent.height;
        fci.layers = 1;
        vkCreateFramebuffer(GN_Vk.device, &fci, 0, &GN_Vk.framebuffers[i]);
    }

    // Command pool
    VkCommandPoolCreateInfo cpci = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    cpci.queueFamilyIndex = GN_Vk.graphicsFamily;
    VkCheck(vkCreateCommandPool(GN_Vk.device, &cpci, 0, &GN_Vk.commandPool), "vkCreateCommandPool");
    GN_Vk.commandBuffers = new VkCommandBuffer[GN_Vk.swapchainImageCount];
    VkCommandBufferAllocateInfo cbai = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    cbai.commandPool = GN_Vk.commandPool;
    cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbai.commandBufferCount = GN_Vk.swapchainImageCount;
    vkAllocateCommandBuffers(GN_Vk.device, &cbai, GN_Vk.commandBuffers);

    // UBO (必须在默认纹理之前创建)
    CreateBuf(sizeof(VkUBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, GN_Vk.uniformBuffer, GN_Vk.uniformMemory);

    // Vertex buffer (预分配足够大，避免运行时 GrowVBufIfNeeded 销毁重建)
    GN_Vk.vertexCapacity = 524288; // 512K 顶点 ≈ 16MB
    CreateBuf(GN_Vk.vertexCapacity * sizeof(VkVertex), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, GN_Vk.vertexBuffer, GN_Vk.vertexMemory);

    // Sampler (必须在默认纹理之前创建)
    VkSamplerCreateInfo sci2 = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    sci2.magFilter = VK_FILTER_LINEAR;
    sci2.minFilter = VK_FILTER_LINEAR;
    sci2.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sci2.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    vkCreateSampler(GN_Vk.device, &sci2, 0, &GN_Vk.defaultSampler);

    // Descriptor pool (必须在默认纹理之前创建)
    VkDescriptorPoolSize dps[2] = {{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1024}, {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1024}};
    VkDescriptorPoolCreateInfo dpci = {VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    dpci.maxSets = 1024;
    dpci.poolSizeCount = 2;
    dpci.pPoolSizes = dps;
    vkCreateDescriptorPool(GN_Vk.device, &dpci, 0, &GN_Vk.descriptorPool);

    // ── 创建 1x1 白色默认纹理 (用于纯色绘制) ──────────────
    // 注意: 必须在 UBO / sampler / descriptorPool 创建之后，因为它们被 descriptor set 引用
    {
        // 使用 host-visible 内存直接写入，避免复杂的 staging
        VkImageCreateInfo ii = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        ii.imageType = VK_IMAGE_TYPE_2D;
        ii.extent = {1, 1, 1};
        ii.mipLevels = 1;
        ii.arrayLayers = 1;
        ii.format = VK_FORMAT_R8G8B8A8_UNORM;
        ii.tiling = VK_IMAGE_TILING_LINEAR;
        ii.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
        ii.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        ii.samples = VK_SAMPLE_COUNT_1_BIT;
        ii.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        VkImage dimg;
        VkDeviceMemory dmem;
        if(vkCreateImage(GN_Vk.device, &ii, 0, &dimg) == VK_SUCCESS) {
            GN_Vk.defaultImage = dimg; // 存储用于 shutdown 清理
            VkMemoryRequirements mr;
            vkGetImageMemoryRequirements(GN_Vk.device, dimg, &mr);
            VkMemoryAllocateInfo ai = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
            ai.allocationSize = mr.size;
            ai.memoryTypeIndex = FindMemType(mr.memoryTypeBits,
                                             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            if(vkAllocateMemory(GN_Vk.device, &ai, 0, &dmem) == VK_SUCCESS) {
                GN_Vk.defaultImageMemory = dmem; // 存储用于 shutdown 清理
                vkBindImageMemory(GN_Vk.device, dimg, dmem, 0);
                // 直接写入白色像素
                VkImageSubresource sub = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0};
                VkSubresourceLayout layout;
                vkGetImageSubresourceLayout(GN_Vk.device, dimg, &sub, &layout);
                void *d;
                vkMapMemory(GN_Vk.device, dmem, 0, VK_WHOLE_SIZE, 0, &d);
                uint32_t *px = (uint32_t *)((char *)d + layout.offset);
                *px = 0xFFFFFFFF; // 白色
                vkUnmapMemory(GN_Vk.device, dmem);
                // 转换为 shader-readable layout
                VkCommandBuffer cb;
                {
                    VkCommandBufferAllocateInfo ai2 = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
                    ai2.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
                    ai2.commandPool = GN_Vk.commandPool;
                    ai2.commandBufferCount = 1;
                    vkAllocateCommandBuffers(GN_Vk.device, &ai2, &cb);
                }
                {
                    VkCommandBufferBeginInfo bi = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
                    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
                    vkBeginCommandBuffer(cb, &bi);
                }
                {
                    VkImageMemoryBarrier b = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
                    b.oldLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
                    b.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                    b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                    b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                    b.image = dimg;
                    b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                    b.subresourceRange.levelCount = 1;
                    b.subresourceRange.layerCount = 1;
                    b.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
                    b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                    vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, 0, 0, 0, 1, &b);
                }
                vkEndCommandBuffer(cb);
                {
                    VkSubmitInfo si = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
                    si.commandBufferCount = 1;
                    si.pCommandBuffers = &cb;
                    vkQueueSubmit(GN_Vk.graphicsQueue, 1, &si, VK_NULL_HANDLE);
                    vkQueueWaitIdle(GN_Vk.graphicsQueue);
                }
                vkFreeCommandBuffers(GN_Vk.device, GN_Vk.commandPool, 1, &cb);
                // 创建 image view 和 descriptor set
                VkImageView div = MakeImageView(dimg, VK_FORMAT_R8G8B8A8_UNORM);
                GN_Vk.defaultImageView = div; // 存储供 3D 无纹理绘制降级
                VkDescriptorSetAllocateInfo dai = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
                dai.descriptorPool = GN_Vk.descriptorPool;
                dai.descriptorSetCount = 1;
                dai.pSetLayouts = &GN_Vk.descriptorLayout;
                vkAllocateDescriptorSets(GN_Vk.device, &dai, &GN_Vk.defaultDS);
                VkDescriptorBufferInfo dbi = {GN_Vk.uniformBuffer, 0, sizeof(VkUBO)};
                VkDescriptorImageInfo dii = {GN_Vk.defaultSampler, div, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
                VkWriteDescriptorSet w[2] = {{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET}, {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET}};
                w[0].dstSet = GN_Vk.defaultDS;
                w[0].dstBinding = 0;
                w[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                w[0].descriptorCount = 1;
                w[0].pBufferInfo = &dbi;
                w[1].dstSet = GN_Vk.defaultDS;
                w[1].dstBinding = 1;
                w[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                w[1].descriptorCount = 1;
                w[1].pImageInfo = &dii;
                vkUpdateDescriptorSets(GN_Vk.device, 2, w, 0, 0);
            }
        }
    }

    // Sync
    VkSemaphoreCreateInfo sci3 = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    vkCreateSemaphore(GN_Vk.device, &sci3, 0, &GN_Vk.imageAvailable);
    vkCreateSemaphore(GN_Vk.device, &sci3, 0, &GN_Vk.renderFinished);
    VkFenceCreateInfo fci2 = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    fci2.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    vkCreateFence(GN_Vk.device, &fci2, 0, &GN_Vk.inFlightFence);

#ifdef GN_USING_3D
    GN_VkInit3D();
#endif

    GN_Vk.initialized = true;
}

// ── 设备与交换链关闭：先等 GPU 空闲，再释放 3D/2D 资源与设备对象 ──
void GN_VkShutdown() {
    if(!GN_Vk.initialized) return;
    vkDeviceWaitIdle(GN_Vk.device);
#ifdef GN_USING_3D
    GN_VkShutdown3D();
#endif
    vkDestroySemaphore(GN_Vk.device, GN_Vk.imageAvailable, 0);
    vkDestroySemaphore(GN_Vk.device, GN_Vk.renderFinished, 0);
    vkDestroyFence(GN_Vk.device, GN_Vk.inFlightFence, 0);
    vkDestroyDescriptorPool(GN_Vk.device, GN_Vk.descriptorPool, 0);
    vkDestroySampler(GN_Vk.device, GN_Vk.defaultSampler, 0);
    // 清理默认纹理资源 (1x1 白色)
    if(GN_Vk.defaultImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(GN_Vk.device, GN_Vk.defaultImageView, 0);
        GN_Vk.defaultImageView = VK_NULL_HANDLE;
    }
    if(GN_Vk.defaultImage != VK_NULL_HANDLE) {
        vkDestroyImage(GN_Vk.device, GN_Vk.defaultImage, 0);
        GN_Vk.defaultImage = VK_NULL_HANDLE;
    }
    if(GN_Vk.defaultImageMemory != VK_NULL_HANDLE) {
        vkFreeMemory(GN_Vk.device, GN_Vk.defaultImageMemory, 0);
        GN_Vk.defaultImageMemory = VK_NULL_HANDLE;
    }
    vkDestroyBuffer(GN_Vk.device, GN_Vk.vertexBuffer, 0);
    vkFreeMemory(GN_Vk.device, GN_Vk.vertexMemory, 0);
    vkDestroyBuffer(GN_Vk.device, GN_Vk.uniformBuffer, 0);
    vkFreeMemory(GN_Vk.device, GN_Vk.uniformMemory, 0);
    vkDestroyCommandPool(GN_Vk.device, GN_Vk.commandPool, 0);
    for(uint32_t i = 0; i < GN_Vk.swapchainImageCount; i++) vkDestroyFramebuffer(GN_Vk.device, GN_Vk.framebuffers[i], 0);
    delete[] GN_Vk.framebuffers;
    delete[] GN_Vk.commandBuffers;
    for(uint32_t i = 0; i < GN_Vk.swapchainImageCount; i++) vkDestroyImageView(GN_Vk.device, GN_Vk.swapchainViews[i], 0);
    delete[] GN_Vk.swapchainViews;
    delete[] GN_Vk.swapchainImages;
    vkDestroySwapchainKHR(GN_Vk.device, GN_Vk.swapchain, 0);
    vkDestroyPipeline(GN_Vk.device, GN_Vk.pipeline, 0);
    vkDestroyPipeline(GN_Vk.device, GN_Vk.pipelineBright, 0);
    vkDestroyPipeline(GN_Vk.device, GN_Vk.pipelineMultiply, 0);
    vkDestroyPipeline(GN_Vk.device, GN_Vk.pipelineNull, 0);
    vkDestroyPipelineLayout(GN_Vk.device, GN_Vk.pipelineLayout, 0);
    vkDestroyDescriptorSetLayout(GN_Vk.device, GN_Vk.descriptorLayout, 0);
    if(s_rtFramebuffer) {
        vkDestroyFramebuffer(GN_Vk.device, s_rtFramebuffer, 0);
        s_rtFramebuffer = VK_NULL_HANDLE;
    }
    if(s_rtRenderPass) {
        vkDestroyRenderPass(GN_Vk.device, s_rtRenderPass, 0);
        s_rtRenderPass = VK_NULL_HANDLE;
    }
    vkDestroyRenderPass(GN_Vk.device, GN_Vk.renderPass, 0);
    vkDestroyDevice(GN_Vk.device, 0);
    vkDestroySurfaceKHR(GN_Vk.instance, GN_Vk.surface, 0);
    vkDestroyInstance(GN_Vk.instance, 0);
    GN_Vk.initialized = false;
}

// 历史遗留：pass 内 DataFill 的 staging 暂存数组（当前未使用；释放逻辑已并入 vkFrame2d.cpp）
static VkBuffer s_pendingStagingBufs[32];
static VkDeviceMemory s_pendingStagingMems[32];
static int s_pendingStagingCnt = 0;

// ═══════════════════════════════════════════════════════════
//  RenderTarget 离屏渲染 (定义在帧循环变量之后)
// ═══════════════════════════════════════════════════════════
sIMAGE *s_activeRT = nullptr;
bool s_rtInProgress = false;

static VkRenderPass CreateRTRenderPass() {
    VkAttachmentDescription ad = {};
    ad.format = VK_FORMAT_R8G8B8A8_UNORM;
    ad.samples = VK_SAMPLE_COUNT_1_BIT;
    ad.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    ad.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    ad.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    ad.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    VkAttachmentReference ar = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkSubpassDescription sp = {};
    sp.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    sp.colorAttachmentCount = 1;
    sp.pColorAttachments = &ar;
    VkRenderPassCreateInfo rpi = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    rpi.attachmentCount = 1;
    rpi.pAttachments = &ad;
    rpi.subpassCount = 1;
    rpi.pSubpasses = &sp;
    VkRenderPass rp;
    VkResult res = vkCreateRenderPass(GN_Vk.device, &rpi, 0, &rp);
    VkCheck(res, "vkCreateRenderPass in CreateRTRenderPass");
    return rp;
}

static void BeginRTRenderPass() {
    if(!s_activeRT || !s_activeRT->vkImage || s_rtInProgress) return;
    FlushVertexBatch();
    if(s_inRenderPass) {
        vkCmdEndRenderPass(GN_Vk.commandBuffers[GN_Vk.currentFrame]);
        s_inRenderPass = false;
    }
    {
        VkImageMemoryBarrier b = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        b.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        b.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.image = s_activeRT->vkImage;
        b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        b.subresourceRange.levelCount = 1;
        b.subresourceRange.layerCount = 1;
        b.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        b.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        vkCmdPipelineBarrier(GN_Vk.commandBuffers[GN_Vk.currentFrame],
                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                             0, 0, 0, 0, 0, 1, &b);
    }
    if(!s_rtRenderPass) s_rtRenderPass = CreateRTRenderPass();
    if(s_rtFramebuffer) vkDestroyFramebuffer(GN_Vk.device, s_rtFramebuffer, 0);
    {
        VkFramebufferCreateInfo fci = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        fci.renderPass = s_rtRenderPass;
        fci.attachmentCount = 1;
        fci.pAttachments = &s_activeRT->vkImageView;
        fci.width = s_activeRT->width;
        fci.height = s_activeRT->height;
        fci.layers = 1;
        VkResult res = vkCreateFramebuffer(GN_Vk.device, &fci, 0, &s_rtFramebuffer);
        VkCheck(res, "vkCreateFramebuffer in BeginRTRenderPass");
    }
    {
        VkClearValue cv = {{{0.0f, 0.0f, 0.0f, 0.0f}}};
        VkRenderPassBeginInfo rpi = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        rpi.renderPass = s_rtRenderPass;
        rpi.framebuffer = s_rtFramebuffer;
        rpi.renderArea = {{0, 0}, {s_activeRT->width, s_activeRT->height}};
        rpi.clearValueCount = 1;
        rpi.pClearValues = &cv;
        vkCmdBeginRenderPass(GN_Vk.commandBuffers[GN_Vk.currentFrame], &rpi, VK_SUBPASS_CONTENTS_INLINE);
    }
    vkCmdBindPipeline(GN_Vk.commandBuffers[GN_Vk.currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, GN_Vk.pipeline);
    {
        VkRect2D sc;
        sc.offset.x = 0;
        sc.offset.y = 0;
        sc.extent.width = s_activeRT->width;
        sc.extent.height = s_activeRT->height;
        vkCmdSetScissor(GN_Vk.commandBuffers[GN_Vk.currentFrame], 0, 1, &sc);
    }
    {
        VkUBO ubo;
        float proj[16] = {2.0f / s_activeRT->width, 0, 0, 0, 0, 2.0f / s_activeRT->height, 0, 0, 0, 0, -1, 0, -1, -1, 0, 1};
        memcpy(ubo.proj, proj, sizeof(proj));
        void *data;
        vkMapMemory(GN_Vk.device, GN_Vk.uniformMemory, 0, sizeof(ubo), 0, &data);
        memcpy(data, &ubo, sizeof(ubo));
        vkUnmapMemory(GN_Vk.device, GN_Vk.uniformMemory);
    }
    GN_Vk.vertexCount = 0;
    s_rtInProgress = true;
}

static void EndRTRenderPass() {
    if(!s_rtInProgress) return;
    FlushVertexBatch();
    vkCmdEndRenderPass(GN_Vk.commandBuffers[GN_Vk.currentFrame]);
    {
        VkImageMemoryBarrier b = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        b.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        b.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.image = s_activeRT->vkImage;
        b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        b.subresourceRange.levelCount = 1;
        b.subresourceRange.layerCount = 1;
        b.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(GN_Vk.commandBuffers[GN_Vk.currentFrame],
                             VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                             0, 0, 0, 0, 0, 1, &b);
    }
    {
        VkClearValue cv = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
        VkRenderPassBeginInfo rpi = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        rpi.renderPass = GN_Vk.renderPass;
        rpi.framebuffer = GN_Vk.framebuffers[GN_Vk.currentFrame];
        rpi.renderArea = {{0, 0}, GN_Vk.swapchainExtent};
        rpi.clearValueCount = 1;
        rpi.pClearValues = &cv;
        vkCmdBeginRenderPass(GN_Vk.commandBuffers[GN_Vk.currentFrame], &rpi, VK_SUBPASS_CONTENTS_INLINE);
    }
    vkCmdBindPipeline(GN_Vk.commandBuffers[GN_Vk.currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, GN_Vk.pipeline);
    {
        VkRect2D sc;
        sc.offset.x = 0;
        sc.offset.y = 0;
        sc.extent = GN_Vk.swapchainExtent;
        vkCmdSetScissor(GN_Vk.commandBuffers[GN_Vk.currentFrame], 0, 1, &sc);
    }
    {
        VkUBO ubo;
        float L = 0, R = (float)GN_Vk.swapchainExtent.width, T = (float)GN_Vk.swapchainExtent.height, B = 0;
        float proj[16] = {2.0f / (R - L), 0, 0, 0, 0, 2.0f / (T - B), 0, 0, 0, 0, -1, 0, -(R + L) / (R - L), -(T + B) / (T - B), 0, 1};
        memcpy(ubo.proj, proj, sizeof(proj));
        void *data;
        vkMapMemory(GN_Vk.device, GN_Vk.uniformMemory, 0, sizeof(ubo), 0, &data);
        memcpy(data, &ubo, sizeof(ubo));
        vkUnmapMemory(GN_Vk.device, GN_Vk.uniformMemory);
    }
    GN_Vk.vertexCount = 0;
    s_rtInProgress = false;
    s_inRenderPass = true; // 已切回主渲染通道
    s_activeRT = nullptr;
}

// ── 窗口尺寸变化：重建 swapchain、视图与 framebuffer（3D 目标需随窗口重建） ──
void GN_VkResize(int w, int h) {
    if(w <= 0 || h <= 0) return;
    GN_Vk.viewWidth = w;
    GN_Vk.viewHeight = h;
    if(!GN_Vk.initialized) return;
    // 重建 swapchain 和相关资源以适应新尺寸
    vkDeviceWaitIdle(GN_Vk.device);
    // 销毁旧 swapchain 资源
    for(uint32_t i = 0; i < GN_Vk.swapchainImageCount; i++) vkDestroyFramebuffer(GN_Vk.device, GN_Vk.framebuffers[i], 0);
    delete[] GN_Vk.framebuffers;
    for(uint32_t i = 0; i < GN_Vk.swapchainImageCount; i++) vkDestroyImageView(GN_Vk.device, GN_Vk.swapchainViews[i], 0);
    delete[] GN_Vk.swapchainViews;
    delete[] GN_Vk.swapchainImages;
    vkDestroySwapchainKHR(GN_Vk.device, GN_Vk.swapchain, 0);
    // 重建 swapchain
    VkSurfaceCapabilitiesKHR caps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(GN_Vk.physicalDevice, GN_Vk.surface, &caps);
    GN_Vk.swapchainExtent = caps.currentExtent;
    if(GN_Vk.swapchainExtent.width == 0xffffffff || GN_Vk.swapchainExtent.width == 0) {
        GN_Vk.swapchainExtent.width = (uint32_t)w;
        GN_Vk.swapchainExtent.height = (uint32_t)h;
    }
    uint32_t fmtCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(GN_Vk.physicalDevice, GN_Vk.surface, &fmtCount, 0);
    VkSurfaceFormatKHR *fmts = new VkSurfaceFormatKHR[fmtCount];
    vkGetPhysicalDeviceSurfaceFormatsKHR(GN_Vk.physicalDevice, GN_Vk.surface, &fmtCount, fmts);
    GN_Vk.swapchainFormat = fmts[0].format;
    delete[] fmts;
    if(GN_Vk.swapchainFormat == VK_FORMAT_UNDEFINED) GN_Vk.swapchainFormat = VK_FORMAT_B8G8R8A8_SRGB;
    VkSwapchainCreateInfoKHR sci = {VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    sci.surface = GN_Vk.surface;
    sci.minImageCount = 2;
    sci.imageFormat = GN_Vk.swapchainFormat;
    sci.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    sci.imageExtent = GN_Vk.swapchainExtent;
    sci.imageArrayLayers = 1;
    sci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    sci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    sci.preTransform = caps.currentTransform;
    sci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    sci.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    sci.clipped = VK_TRUE;
    VkCheck(vkCreateSwapchainKHR(GN_Vk.device, &sci, 0, &GN_Vk.swapchain), "vkCreateSwapchainKHR(resize)");
    vkGetSwapchainImagesKHR(GN_Vk.device, GN_Vk.swapchain, &GN_Vk.swapchainImageCount, 0);
    GN_Vk.swapchainImages = new VkImage[GN_Vk.swapchainImageCount];
    GN_Vk.swapchainViews = new VkImageView[GN_Vk.swapchainImageCount];
    vkGetSwapchainImagesKHR(GN_Vk.device, GN_Vk.swapchain, &GN_Vk.swapchainImageCount, GN_Vk.swapchainImages);
    for(uint32_t i = 0; i < GN_Vk.swapchainImageCount; i++) GN_Vk.swapchainViews[i] = MakeImageView(GN_Vk.swapchainImages[i], GN_Vk.swapchainFormat);
    // 重建 framebuffers
    GN_Vk.framebuffers = new VkFramebuffer[GN_Vk.swapchainImageCount];
    for(uint32_t i = 0; i < GN_Vk.swapchainImageCount; i++) {
        VkFramebufferCreateInfo fci = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        fci.renderPass = GN_Vk.renderPass;
        fci.attachmentCount = 1;
        fci.pAttachments = &GN_Vk.swapchainViews[i];
        fci.width = GN_Vk.swapchainExtent.width;
        fci.height = GN_Vk.swapchainExtent.height;
        fci.layers = 1;
        vkCreateFramebuffer(GN_Vk.device, &fci, 0, &GN_Vk.framebuffers[i]);
    }
}

// ── 帧结束入口：转发到 vkFrame2d.cpp 的 GN_VkEndFrame ──
void RenderFinish(bool only3d) {
    (void)only3d;
    // Vulkan: end the current frame and present
    GNg::GN_VkEndFrame();
}
// ── 缺失的 API 实现 ────────────────────────────────────────
// ── sIMAGE 数据上传/填充（PNG IO 与图集见 vkImageIo.cpp） ──

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
    init3DTargets();

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
    init3DShadow(svSpv);

    // ── 创建 PBR 描述符集 (必须在 Shadow Map 创建之后) ──
    {
        VkDescriptorSetAllocateInfo dai = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        dai.descriptorPool = GN_Vk.descriptorPool;
        dai.descriptorSetCount = 1;
        dai.pSetLayouts = &GN_Vk.descriptorLayoutPBR;
        vkAllocateDescriptorSets(GN_Vk.device, &dai, &GN_Vk.descriptorSetPBR);

        VkDescriptorBufferInfo dbiScene = {GN_Vk.uniformBuffer3D, 0, sizeof(VkSceneUBO)};
        VkDescriptorBufferInfo dbiLight = {GN_Vk.lightBuffer3D, 0, 1024};
        VkDescriptorImageInfo diiTex = {GN_Vk.defaultSampler, GN_Vk.defaultImageView,
                                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        VkDescriptorImageInfo diiNormal = {GN_Vk.defaultSampler, GN_Vk.defaultImageView,
                                           VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        VkDescriptorImageInfo diiShadow = {GN_Vk.shadowSampler, GN_Vk.shadowView,
                                           VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL};

        VkWriteDescriptorSet w[5] = {{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET},
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
        vkUpdateDescriptorSets(GN_Vk.device, 5, w, 0, 0);
        // Binding 5 (skybox cubemap) 将在天空盒初始化后更新
    }

    init3DHdr();

    init3DSkybox();
    init3DGbuffer();
    init3DDeferredLight();
}

#endif // GN_USING_3D

} // namespace GNg
