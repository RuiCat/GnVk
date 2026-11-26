/****************************************************************************
 *  GN_mini — Vulkan 设备/交换链初始化
 *  职责：GN_VkInit —— 实例/设备/表面/交换链（或离屏目标）/render pass/
 *        2D 管线/命令缓冲/同步对象创建，末尾编排 GN_VkInit3D。
 *  2026-11-26 自 GN_Vk.cpp move-only 拆出（REFACTOR_PLAN §13 B2）。
 ****************************************************************************/
#include <GN.h>
#include "GN_Vk.h"
#include "vkInternal.h"
#include "vkValidation.h" // GN_VK_VALIDATION=1 时启用 Khronos 校验层
#include <cstdio>
#include <cstring>
#include <vector>

namespace GNg {

// ═══════════════════════════════════════════════════════════
//  设备与交换链初始化（辅助函数见 vkDevice.cpp；2D/3D 分流见 vk*2d.cpp / vk3d*.cpp）
// ═══════════════════════════════════════════════════════════

void GN_VkInit(GLFWwindow *window, int w, int h) {
    if(GN_Vk.initialized) return;
    // window == 0 → 无窗口（离屏）模式：不建 surface、不建 swapchain、不需要任何窗口系统。
    const bool headless = (window == nullptr);
    GN_Vk.headless = headless;
    GN_Vk.swapFinalLayout =
        headless ? VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL : VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    GN_Vk.viewWidth = w;
    GN_Vk.viewHeight = h;

    // Instance
    VkApplicationInfo ai = {VK_STRUCTURE_TYPE_APPLICATION_INFO};
    ai.pApplicationName = "GN_mini";
    ai.apiVersion = VK_API_VERSION_1_1; // 1.1: 硬件光追(AS/ray_query)与 Features2 需要
    // 无窗口模式：实例不需要任何 surface 扩展（GLFW 在没有窗口时也不会给出）
    uint32_t extCount = 0;
    const char **exts = headless ? 0 : glfwGetRequiredInstanceExtensions(&extCount);
    if(headless) extCount = 0;
    VkInstanceCreateInfo ici = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    ici.pApplicationInfo = &ai;
    // 可选：GN_VK_VALIDATION=1 时追加校验层与 VK_EXT_debug_utils（未装层则不加）。
    static std::vector<const char *> s_layerNames;
    static std::vector<const char *> s_extNames;
    if(extCount > 0) s_extNames.assign(exts, exts + extCount); // GLFW 要求的扩展（含 surface）
#if defined(__APPLE__)
    // MoltenVK：端口枚举模式。不声明它时 vkEnumeratePhysicalDevices 直接返回
    // VK_ERROR_INCOMPATIBLE_DRIVER（在 macOS 上表现为“没有任何 Vulkan 设备”）。
    // 扩展本身若不存在（真 Apple 原生驱动）则不加，避免校验层报错。
    {
        uint32_t ic = 0;
        if(vkEnumerateInstanceExtensionProperties(nullptr, &ic, nullptr) == VK_SUCCESS && ic > 0) {
            std::vector<VkExtensionProperties> ieps(ic);
            if(vkEnumerateInstanceExtensionProperties(nullptr, &ic, ieps.data()) == VK_SUCCESS) {
                for(uint32_t i = 0; i < ic; ++i) {
                    if(std::strcmp(ieps[i].extensionName,
                                   "VK_KHR_portability_enumeration") == 0) {
                        s_extNames.push_back("VK_KHR_portability_enumeration");
                        ici.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
                        break;
                    }
                }
            }
        }
    }
#endif
    if(vkValidationAvailable()) {
        vkValidationAppendInstanceParams(s_layerNames, s_extNames);
        ici.enabledLayerCount = (uint32_t)s_layerNames.size();
        ici.ppEnabledLayerNames = s_layerNames.empty() ? 0 : &s_layerNames[0];
    }
    ici.enabledExtensionCount = (uint32_t)s_extNames.size();
    ici.ppEnabledExtensionNames = s_extNames.empty() ? 0 : &s_extNames[0];
    VkResult res = vkCreateInstance(&ici, 0, &GN_Vk.instance);
    VkCheck(res, "vkCreateInstance");
    if(res != VK_SUCCESS || GN_Vk.instance == VK_NULL_HANDLE) return;
    vkValidationCreateMessenger(GN_Vk.instance);

    // Surface（无窗口模式没有 surface）
    if(!headless) {
        res = glfwCreateWindowSurface(GN_Vk.instance, window, 0, &GN_Vk.surface);
        VkCheck(res, "glfwCreateWindowSurface");
        if(res != VK_SUCCESS || GN_Vk.surface == VK_NULL_HANDLE) return;
    }

    // Physical device
    uint32_t dc = 0;
    res = vkEnumeratePhysicalDevices(GN_Vk.instance, &dc, 0);
    if(res != VK_SUCCESS || dc == 0) {
        GN_LogErr("[GN_Vk] FATAL: 未找到可用的 Vulkan 物理设备 (err=%d, count=%u)\n", (int)res, dc);
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
        if(headless) { // 无 surface：只要求图形队列
            if(qps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                GN_Vk.graphicsFamily = i;
                queueFound = true;
                break;
            }
            continue;
        }
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

    // 硬件光追（RT）能力：仅在驱动支持时启用 AS/ray_query 扩展与特性，否则保持原样。
    std::vector<const char *> dexts;
    if(!headless) dexts.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME); // 离屏渲染不需要交换链扩展
    bool rayQuery = false;
    VkPhysicalDeviceAccelerationStructureFeaturesKHR asFeat = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR};
    VkPhysicalDeviceRayQueryFeaturesKHR rqFeat = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR};
    VkPhysicalDeviceBufferDeviceAddressFeatures bdaFeat = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES};
    {
        uint32_t ec = 0;
        vkEnumerateDeviceExtensionProperties(GN_Vk.physicalDevice, nullptr, &ec, nullptr);
        std::vector<VkExtensionProperties> eps(ec);
        vkEnumerateDeviceExtensionProperties(GN_Vk.physicalDevice, nullptr, &ec, eps.data());
        bool hasAs = false, hasRq = false, hasDho = false, hasBda = false;
        bool hasDi = false, hasS14 = false;
        bool hasPortabilitySubset = false;
        for (uint32_t i = 0; i < ec; ++i) {
            const char *n = eps[i].extensionName;
            if (std::strcmp(n, VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME) == 0) hasAs = true;
            else if (std::strcmp(n, VK_KHR_RAY_QUERY_EXTENSION_NAME) == 0) hasRq = true;
            else if (std::strcmp(n, VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME) == 0) hasDho = true;
            else if (std::strcmp(n, VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME) == 0) hasBda = true;
            else if (std::strcmp(n, VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME) == 0) hasDi = true;
            else if (std::strcmp(n, VK_KHR_SPIRV_1_4_EXTENSION_NAME) == 0) hasS14 = true;
            else if (std::strcmp(n, "VK_KHR_portability_subset") == 0) hasPortabilitySubset = true;
        }
        // MoltenVK / 某些移植层：设备声明了 VK_KHR_portability_subset 就必须启用它，
        // 否则 vkCreateDevice 报 VUID-VkDeviceCreateInfo-pProperties-04451。
        if (hasPortabilitySubset) dexts.push_back("VK_KHR_portability_subset");
        if (hasAs && hasRq) {
            VkPhysicalDeviceFeatures2 f2 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
            asFeat.pNext = &rqFeat;
            rqFeat.pNext = &bdaFeat;
            f2.pNext = &asFeat;
            vkGetPhysicalDeviceFeatures2(GN_Vk.physicalDevice, &f2);
            // 校验层报过的完整依赖链：ray_query → spirv_1_4 + acceleration_structure；
            // acceleration_structure → deferred_host_operations + descriptor_indexing (+ buffer_device_address)。
            // 任何一环缺失就不开光追，避免创建出违规设备。
            if (asFeat.accelerationStructure && rqFeat.rayQuery && bdaFeat.bufferDeviceAddress && hasDho &&
                hasBda && hasDi && hasS14) {
                rayQuery = true;
                dexts.push_back(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
                dexts.push_back(VK_KHR_RAY_QUERY_EXTENSION_NAME);
                dexts.push_back(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
                dexts.push_back(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
                dexts.push_back(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);
                dexts.push_back(VK_KHR_SPIRV_1_4_EXTENSION_NAME);
                // spirv_1_4 还要求 shader_float_controls（校验层点名）
                dexts.push_back(VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME);
            }
        }
    }

    VkDeviceCreateInfo dci = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    dci.queueCreateInfoCount = 1;
    dci.pQueueCreateInfos = &qci;
    dci.enabledExtensionCount = static_cast<uint32_t>(dexts.size());
    dci.ppEnabledExtensionNames = dexts.data();
    if (rayQuery) {
        asFeat.accelerationStructure = VK_TRUE;
        rqFeat.rayQuery = VK_TRUE;
        bdaFeat.bufferDeviceAddress = VK_TRUE;
        dci.pNext = &asFeat;
    }
    VkCheck(vkCreateDevice(GN_Vk.physicalDevice, &dci, 0, &GN_Vk.device), "vkCreateDevice");
    GN_Vk.rayQuerySupported = rayQuery;
    vkGetDeviceQueue(GN_Vk.device, GN_Vk.graphicsFamily, 0, &GN_Vk.graphicsQueue);
    // P1b：设备就绪后初始化管线缓存（所有缓存的管线在 GN_VkShutdown 里统一 Release）
    GN_Vk.aPipelineCache.Init(GN_Vk.device);

    // 渲染目标：窗口模式 = swapchain；无窗口模式 = 自建离屏图（同格式/尺寸/张数约定）
    if(headless) {
        const uint32_t ow = (uint32_t)(w > 0 ? w : 800);
        const uint32_t oh = (uint32_t)(h > 0 ? h : 600);
        if(!createOffscreenSwapchain(ow, oh, kHeadlessImageCount)) {
            fprintf(stderr, "[GN_Vk] FATAL: 离屏目标创建失败\n");
            return;
        }
    } else {
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
    GN_Vk.swapchainFormat = chooseSwapchainFormat(fmts, fmtCount);
    delete[] fmts;
    fprintf(stderr, "[GN_Vk] swapchain format=%d (%s)\n", (int)GN_Vk.swapchainFormat,
            isSrgbSwapchainFormat(GN_Vk.swapchainFormat) ? "sRGB" : "non-sRGB");
    VkSwapchainCreateInfoKHR sci = {VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    sci.surface = GN_Vk.surface;
    // 校验层报过 minImageCount(2) < surface caps 的最小值（本机 4）；按 caps 取，避免交换链违规。
    sci.minImageCount = caps.minImageCount > 2u ? caps.minImageCount : 2u;
    sci.imageFormat = GN_Vk.swapchainFormat;
    sci.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    sci.imageExtent = GN_Vk.swapchainExtent;
    sci.imageArrayLayers = 1;
    sci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    sci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    sci.preTransform = caps.currentTransform;
    sci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    sci.presentMode = ChoosePresentMode(GN_Vk.physicalDevice, GN_Vk.surface);
    sci.clipped = VK_TRUE;
    fprintf(stderr, "[GN_Vk] present mode=%d (GN_VSYNC=0 时优先 IMMEDIATE/MAILBOX)\n",
            (int)sci.presentMode);
    VkCheck(vkCreateSwapchainKHR(GN_Vk.device, &sci, 0, &GN_Vk.swapchain), "vkCreateSwapchainKHR");
    vkGetSwapchainImagesKHR(GN_Vk.device, GN_Vk.swapchain, &GN_Vk.swapchainImageCount, 0);
    GN_Vk.swapchainImages = new VkImage[GN_Vk.swapchainImageCount];
    GN_Vk.swapchainViews = new VkImageView[GN_Vk.swapchainImageCount];
    vkGetSwapchainImagesKHR(GN_Vk.device, GN_Vk.swapchain, &GN_Vk.swapchainImageCount, GN_Vk.swapchainImages);
    for(uint32_t i = 0; i < GN_Vk.swapchainImageCount; i++) GN_Vk.swapchainViews[i] = MakeImageView(GN_Vk.swapchainImages[i], GN_Vk.swapchainFormat);
    } // end of: headless 离屏目标 / swapchain

    // Render pass
    VkAttachmentDescription ad = {};
    ad.format = GN_Vk.swapchainFormat;
    ad.samples = VK_SAMPLE_COUNT_1_BIT;
    ad.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    ad.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    ad.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    ad.finalLayout = GN_Vk.swapFinalLayout; // 窗口=PRESENT_SRC；无窗口=TRANSFER_SRC
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

    // 描述符布局与管线布局必须先于管线创建（缓存只接收已有的 VkPipelineLayout）
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

    // Pipeline（P1b：改由 cPIPELINE_CACHE 创建；描述字段逐项对齐旧 VkPipelineCreateInfo）
    // 顶点布局：0=pos(vec2) 1=uv(vec2) 2=color(vec4)，单一 binding、stride=sizeof(VkVertex)
    GN3d::sVERTEX_LAYOUT layout2D;
    layout2D.Add(0, (uint32_t)offsetof(VkVertex, x), VK_FORMAT_R32G32_SFLOAT);
    layout2D.Add(1, (uint32_t)offsetof(VkVertex, u), VK_FORMAT_R32G32_SFLOAT);
    layout2D.Add(2, (uint32_t)offsetof(VkVertex, r), VK_FORMAT_R32G32B32A32_SFLOAT);
    layout2D.aStride = (uint32_t)sizeof(VkVertex);

    GN3d::sPIPELINE_DESC desc2D;
    desc2D.aKind = GN3d::ePIPE_GRAPHICS;
    desc2D.aVertSpv = vertSpv.data();
    desc2D.aVertSize = vertSpv.size() * sizeof(uint32_t);
    desc2D.aFragSpv = fragSpv.data();
    desc2D.aFragSize = fragSpv.size() * sizeof(uint32_t);
    desc2D.aVertexLayout = layout2D;
    desc2D.aRenderPass = GN_Vk.renderPass; // 只有颜色附件，无深度
    desc2D.aSubpass = 0;
    desc2D.aLayout = GN_Vk.pipelineLayout;
    desc2D.aDepthTest = false;
    desc2D.aDepthWrite = false;
    desc2D.aDepthStencil = false; // 旧 2D 管线 pDepthStencilState = 空
    desc2D.aCullBack = true;
    desc2D.aFrontFace = VK_FRONT_FACE_CLOCKWISE;
    desc2D.aBlend = GN3d::eBLEND_NONE;
    desc2D.aTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    desc2D.aSamples = VK_SAMPLE_COUNT_1_BIT;
    desc2D.aColorAttachmentCount = 1;
    desc2D.aDynamicViewport = false; // 旧 2D 管线视口固定
    desc2D.aViewport = VkViewport{0.0f, 0.0f, (float)GN_Vk.swapchainExtent.width,
                                  (float)GN_Vk.swapchainExtent.height, 0.0f, 1.0f};
    desc2D.aDynamicScissor = true; // 旧 2D 管线仅 SCISSOR 动态（UI 裁剪）
    desc2D.aScissor = VkRect2D{{0, 0}, GN_Vk.swapchainExtent};
    // 旧 2D 混合：color=(SRC_ALPHA, ONE_MINUS_SRC_ALPHA)，alpha=(ONE, ZERO)
    desc2D.aCustomBlend = true;
    desc2D.aBlendSrcColor = VK_BLEND_FACTOR_SRC_ALPHA;
    desc2D.aBlendDstColor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    desc2D.aBlendSrcAlpha = VK_BLEND_FACTOR_ONE;
    desc2D.aBlendDstAlpha = VK_BLEND_FACTOR_ZERO;
    GN_Vk.pipeline = GN_Vk.aPipelineCache.GetOrCreate(desc2D);

    desc2D.aBlendDstColor = VK_BLEND_FACTOR_ONE; // eBLEND_BRIGHT（加法）
    GN_Vk.pipelineBright = GN_Vk.aPipelineCache.GetOrCreate(desc2D);

    desc2D.aBlendSrcColor = VK_BLEND_FACTOR_DST_COLOR; // eBLEND_MODULATE（乘法）
    desc2D.aBlendDstColor = VK_BLEND_FACTOR_ZERO;
    GN_Vk.pipelineMultiply = GN_Vk.aPipelineCache.GetOrCreate(desc2D);

    desc2D.aCustomBlend = false; // eBLEND_NULL（不混合）
    desc2D.aBlend = GN3d::eBLEND_NONE;
    GN_Vk.pipelineNull = GN_Vk.aPipelineCache.GetOrCreate(desc2D);

    if(!GN_Vk.pipeline || !GN_Vk.pipelineBright || !GN_Vk.pipelineMultiply || !GN_Vk.pipelineNull) {
        fprintf(stderr, "[GN_Vk] FATAL: 2D 管线创建失败: %s\n", GN_Vk.aPipelineCache.GetLastError());
        return;
    }

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

    // 2D 叠层通道：3D pass 之后用它把 HUD/文本画在 3D 画面上（loadOp=LOAD 保留已有画面）。
    // 以 swapFinalLayout 为起点：窗口模式是 PRESENT_SRC（承接上一条 pass 的结果），
    // 无窗口模式是 TRANSFER_SRC。两种模式下叠层通道都 loadOp=LOAD，不清掉 3D/合成画面。
    if(s_renderPass2DOverlay == VK_NULL_HANDLE) {
        VkAttachmentDescription adOverlay = {};
        adOverlay.format = GN_Vk.swapchainFormat;
        adOverlay.samples = VK_SAMPLE_COUNT_1_BIT;
        adOverlay.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        adOverlay.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        adOverlay.initialLayout = GN_Vk.swapFinalLayout;
        adOverlay.finalLayout = GN_Vk.swapFinalLayout;
        VkAttachmentReference arOverlay = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        VkSubpassDescription spOverlay = {};
        spOverlay.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        spOverlay.colorAttachmentCount = 1;
        spOverlay.pColorAttachments = &arOverlay;
        VkRenderPassCreateInfo rpiOverlay = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
        rpiOverlay.attachmentCount = 1;
        rpiOverlay.pAttachments = &adOverlay;
        rpiOverlay.subpassCount = 1;
        rpiOverlay.pSubpasses = &spOverlay;
        vkCreateRenderPass(GN_Vk.device, &rpiOverlay, 0, &s_renderPass2DOverlay);
        createOverlayFramebuffers();
    }

    // Command pool
    VkCommandPoolCreateInfo cpci = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    cpci.queueFamilyIndex = GN_Vk.graphicsFamily;
    cpci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; // 每帧 vkResetCommandBuffer 需要
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
    // P9：新增 STORAGE_BUFFER 尺寸（粒子 SSBO：compute set + 绘制 set 各一份）。
    // 纹理环集（kTextureSetCacheCap=256）每套占 8 个 COMBINED_IMAGE_SAMPLER + 4 个 UBO，
    // 池子按 256 套的峰值预留，否则分配失败仍会回退共享 set（与白模同一现象）。
    VkDescriptorPoolSize dps[3] = {{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 4096},
                                   {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 8192},
                                   {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 512}};
    VkDescriptorPoolCreateInfo dpci = {VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    // P1c：允许材质实例单独归还描述符集（GN_VkMaterialDestroy），否则创建/销毁循环会耗尽池。
    dpci.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    dpci.maxSets = 2048;
    dpci.poolSizeCount = 3;
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

// 在 3D pass 之后重新打开 2D 叠层通道：之后排队的 2D/UI/文本顶点会在 GN_VkEndFrame
// 的批次刷新里被画进这个通道（loadOp=LOAD，不清掉 3D 画面），再随帧提交呈现。
} // namespace GNg
