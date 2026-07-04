/****************************************************************************
 *  GN_mini — Vulkan 后端实现
 *  替代原 OpenGL _Graphics 模块
 ****************************************************************************/
#include <GN.h>
#include "GN_Vk.h"
#include <cstdio>
#include <vector>
#include <cstring>
#include <png.h>
#include <fstream>

namespace GNg {

// ── 从文件加载 SPIR-V (构建时由 glslangValidator 编译) ──────
// 优先 assets/shaders/*.spv，失败回退嵌入式 SPIR-V
static std::vector<uint32_t> LoadSPIRVFile(const char *path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if(!f.is_open()) return {};
    size_t sz = f.tellg(); f.seekg(0);
    std::vector<uint32_t> spv(sz / sizeof(uint32_t));
    f.read((char*)spv.data(), sz);
    return spv;
}

GN_VkContext GN_Vk;

// ── 前向声明 ──────────────────────────────────────────
static void CreateBuf(VkDeviceSize sz, VkBufferUsageFlags u, VkMemoryPropertyFlags p, VkBuffer &b, VkDeviceMemory &m);

// ── RenderTarget 静态变量 (需在 shutdown 之前) ────────
static VkRenderPass s_rtRenderPass = VK_NULL_HANDLE;
static VkFramebuffer s_rtFramebuffer = VK_NULL_HANDLE;

// ── 渲染通道追踪 (所有 pass 共用) ──
static bool s_inRenderPass = false;

// ── 顶点结构 ──────────────────────────────────────────────
struct VkVertex {
    float x, y;
    float u, v;
    float r, g, b, a;
};

// ── UBO 结构 ──────────────────────────────────────────────
struct VkUBO {
    float proj[16];
};

// ViewCur in GN_2D.cpp










// ViewCur in GN_2D.cpp


















// sIMAGE 补充方法
// ── 前向声明 (RenderTarget 辅助函数) ──────────────────
static void BeginRTRenderPass();
static void EndRTRenderPass();
extern sIMAGE *s_activeRT;
extern bool s_rtInProgress;

sIMAGE::sIMAGE() : width(128), height(128), uRange(1), vRange(1), texnum(0), fileName(0), hasMipmap(false), add_0000(false), cloneImg(false), vkImage(VK_NULL_HANDLE), vkImageMemory(VK_NULL_HANDLE), vkImageView(VK_NULL_HANDLE), vkSampler(VK_NULL_HANDLE), vkDescriptorSet(VK_NULL_HANDLE), vkIsLinear(false), skipDefer(false), z__attrb(0) {}
sIMAGE::~sIMAGE() { Release(); }

// ── RenderTarget 方法 ──────────────────────────────────
bool sIMAGE::RenderTargetSupport() { return true; }

void sIMAGE::BeginRender(int msaa, bool is3d, bool isnew) {
    (void)msaa; (void)is3d; (void)isnew;
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

void sIMAGE::CopyOut(sIMAGE *dst) {
    if(!dst || !GN_Vk.initialized) return;
    if(!vkImage) { *dst = *this; return; }
    // GPU copy: read source pixels → write to destination
    uint8_t *px = DataGet();
    if(px) {
        dst->CreateEmptyToData(width, height, 1, 0, 1, false, false);
        dst->DataFill(px, CR_RGBA, width, height, 0, 0, 0, false);
        dst->uRange = uRange; dst->vRange = vRange;
        delete[] px;
    } else {
        *dst = *this;
    }
}

P<sIMAGE> sIMAGE::CopyOut(int x, int y, int w, int h, bool alphaOnly, bool flipY) {
    P<sIMAGE> p(1);
    if(!GN_Vk.initialized || !vkImage || w <= 0 || h <= 0) { *p = *this; return p; }
    // Read full image, extract sub-region
    uint8_t *src = DataGet();
    if(!src) { *p = *this; return p; }
    int sx = x < 0 ? 0 : (x >= width ? width-1 : x);
    int sy = y < 0 ? 0 : (y >= height ? height-1 : y);
    int sw = (sx + w > width) ? width - sx : w;
    int sh = (sy + h > height) ? height - sy : h;
    uint8_t *sub = new uint8_t[sw * sh * 4];
    for(int row = 0; row < sh; row++) {
        int srcRow = flipY ? (height - 1 - (sy + row)) : (sy + row);
        for(int col = 0; col < sw; col++) {
            int si = ((srcRow * width) + sx + col) * 4;
            int di = (row * sw + col) * 4;
            if(alphaOnly) {
                sub[di+0] = src[si+3]; sub[di+1] = src[si+3];
                sub[di+2] = src[si+3]; sub[di+3] = src[si+3];
            } else {
                sub[di+0]=src[si+0]; sub[di+1]=src[si+1];
                sub[di+2]=src[si+2]; sub[di+3]=src[si+3];
            }
        }
    }
    delete[] src;
    p->CreateEmptyToData(sw, sh, 1, 0, 1, false, false);
    p->DataFill(sub, CR_RGBA, sw, sh, 0, 0, 0, false);
    delete[] sub;
    return p;
}

P<sIMAGE> sIMAGE::CopyOut() {
    P<sIMAGE> p(1);
    if(!GN_Vk.initialized || !vkImage) { *p = *this; return p; }
    uint8_t *px = DataGet();
    if(px) {
        p->CreateEmptyToData(width, height, 1, 0, 1, false, false);
        p->DataFill(px, CR_RGBA, width, height, 0, 0, 0, false);
        p->uRange = uRange; p->vRange = vRange;
        delete[] px;
    } else {
        *p = *this;
    }
    return p;
}










// ── GPU→CPU pixel readback ──────────────────────────────
uint8 *sIMAGE::DataGet(int *w, int *h, int *fmt) {
    if(w) *w = width;
    if(h) *h = height;
    if(fmt) *fmt = cFormat;
    if(!GN_Vk.initialized || !vkImage || width <= 0 || height <= 0) return nullptr;
    VkDeviceSize sz = (VkDeviceSize)width * (VkDeviceSize)height * 4;
    // Create staging buffer
    VkBuffer stagingBuf; VkDeviceMemory stagingMem;
    CreateBuf(sz, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
              stagingBuf, stagingMem);
    // One-time command: transition → copy → transition back
    VkCommandBuffer cb;
    {VkCommandBufferAllocateInfo ai={VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
     ai.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY; ai.commandPool=GN_Vk.commandPool; ai.commandBufferCount=1;
     VkResult res = vkAllocateCommandBuffers(GN_Vk.device,&ai,&cb);
     VkCheck(res, "vkAllocateCommandBuffers in DataGet");}
    {VkCommandBufferBeginInfo bi={VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
     bi.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT; vkBeginCommandBuffer(cb,&bi);}
    // Transition image to TRANSFER_SRC
    {VkImageMemoryBarrier b={VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
     b.oldLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
     b.newLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
     b.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED; b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
     b.image=vkImage; b.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;
     b.subresourceRange.levelCount=1; b.subresourceRange.layerCount=1;
     b.srcAccessMask=VK_ACCESS_SHADER_READ_BIT; b.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;
     vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                          VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0,0,0,0, 1, &b);}
    // Copy image to buffer
    {VkBufferImageCopy rgn={}; rgn.imageSubresource.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;
     rgn.imageSubresource.layerCount=1; rgn.imageExtent={(uint32_t)width,(uint32_t)height,1};
     vkCmdCopyImageToBuffer(cb, vkImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, stagingBuf, 1, &rgn);}
    // Transition back
    {VkImageMemoryBarrier b={VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
     b.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
     b.newLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
     b.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED; b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
     b.image=vkImage; b.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;
     b.subresourceRange.levelCount=1; b.subresourceRange.layerCount=1;
     b.srcAccessMask=VK_ACCESS_TRANSFER_READ_BIT; b.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;
     vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT,
                          VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0,0,0,0, 1, &b);}
    vkEndCommandBuffer(cb);
    {VkFenceCreateInfo fci={VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
     VkFence f; vkCreateFence(GN_Vk.device,&fci,0,&f);
     VkSubmitInfo si={VK_STRUCTURE_TYPE_SUBMIT_INFO}; si.commandBufferCount=1;
     si.pCommandBuffers=&cb; vkQueueSubmit(GN_Vk.graphicsQueue,1,&si,f);
     vkWaitForFences(GN_Vk.device,1,&f,VK_TRUE,UINT64_MAX);
     vkDestroyFence(GN_Vk.device,f,0);}
    vkFreeCommandBuffers(GN_Vk.device, GN_Vk.commandPool, 1, &cb);
    // Read back
    uint8_t *result = new uint8_t[sz];
    void *d; vkMapMemory(GN_Vk.device, stagingMem, 0, sz, 0, &d);
    memcpy(result, d, sz);
    vkUnmapMemory(GN_Vk.device, stagingMem);
    vkDestroyBuffer(GN_Vk.device, stagingBuf, 0);
    vkFreeMemory(GN_Vk.device, stagingMem, 0);
    return result;
}



uint8 *sIMAGE::DataGetEx(int x, int y, int w, int h, unsigned short *fmt, bool flip) {
    if(fmt) *fmt = (unsigned short)cFormat;
    // Use CopyOut sub-region logic then return raw pixels
    if(!GN_Vk.initialized || !vkImage || w <= 0 || h <= 0) return nullptr;
    uint8_t *src = DataGet();
    if(!src) return nullptr;
    int sx = x < 0 ? 0 : (x >= width ? width-1 : x);
    int sy = y < 0 ? 0 : (y >= height ? height-1 : y);
    int sw = (sx + w > width) ? width - sx : w;
    int sh = (sy + h > height) ? height - sy : h;
    uint8_t *sub = new uint8_t[sw * sh * 4];
    for(int row = 0; row < sh; row++) {
        int srcRow = flip ? (height - 1 - (sy + row)) : (sy + row);
        memcpy(sub + row * sw * 4, src + (srcRow * width + sx) * 4, sw * 4);
    }
    delete[] src;
    return sub;
}

uint8 *sIMAGE::DataGet(int *w, short &h, short &x, short &y, short &rw, unsigned short &fmt, bool flip) {
    // Delegate to main DataGet; sub-region info passed back through reference params
    int iw = 0, ih = 0, ifmt = 0;
    uint8_t *data = DataGet(&iw, &ih, &ifmt);
    if(w) *w = iw; h = (short)ih; x = 0; y = 0; rw = (short)iw; fmt = (unsigned short)ifmt;
    (void)flip;
    return data;
}

// ═══════════════════════════════════════════════════════════
//  Vulkan 核心实现
// ═══════════════════════════════════════════════════════════
// ═══════════════════════════════════════════════════════════
//  VkCheck — Vulkan 错误报告
//  将 VkResult 映射为可读字符串并输出到 stderr
// ═══════════════════════════════════════════════════════════
void VkCheck(VkResult err, const char *msg) {
    if (err == VK_SUCCESS) return;

    const char *errStr = "UNKNOWN";
    switch (err) {
        case VK_NOT_READY:                      errStr = "NOT_READY"; break;
        case VK_TIMEOUT:                        errStr = "TIMEOUT"; break;
        case VK_EVENT_SET:                      errStr = "EVENT_SET"; break;
        case VK_EVENT_RESET:                    errStr = "EVENT_RESET"; break;
        case VK_INCOMPLETE:                     errStr = "INCOMPLETE"; break;
        case VK_ERROR_OUT_OF_HOST_MEMORY:       errStr = "OUT_OF_HOST_MEMORY"; break;
        case VK_ERROR_OUT_OF_DEVICE_MEMORY:     errStr = "OUT_OF_DEVICE_MEMORY"; break;
        case VK_ERROR_INITIALIZATION_FAILED:    errStr = "INITIALIZATION_FAILED"; break;
        case VK_ERROR_DEVICE_LOST:              errStr = "DEVICE_LOST"; break;
        case VK_ERROR_MEMORY_MAP_FAILED:        errStr = "MEMORY_MAP_FAILED"; break;
        case VK_ERROR_LAYER_NOT_PRESENT:        errStr = "LAYER_NOT_PRESENT"; break;
        case VK_ERROR_EXTENSION_NOT_PRESENT:    errStr = "EXTENSION_NOT_PRESENT"; break;
        case VK_ERROR_FEATURE_NOT_PRESENT:      errStr = "FEATURE_NOT_PRESENT"; break;
        case VK_ERROR_INCOMPATIBLE_DRIVER:      errStr = "INCOMPATIBLE_DRIVER"; break;
        case VK_ERROR_TOO_MANY_OBJECTS:         errStr = "TOO_MANY_OBJECTS"; break;
        case VK_ERROR_FORMAT_NOT_SUPPORTED:     errStr = "FORMAT_NOT_SUPPORTED"; break;
        case VK_ERROR_FRAGMENTED_POOL:          errStr = "FRAGMENTED_POOL"; break;
        case VK_ERROR_SURFACE_LOST_KHR:         errStr = "SURFACE_LOST"; break;
        case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: errStr = "NATIVE_WINDOW_IN_USE"; break;
        case VK_SUBOPTIMAL_KHR:                 errStr = "SUBOPTIMAL"; break;
        case VK_ERROR_OUT_OF_DATE_KHR:          errStr = "OUT_OF_DATE"; break;
        default: break;
    }
    fprintf(stderr, "[GN_Vk] ERROR %s: %s (code=%d)\n",
            msg ? msg : "(no message)", errStr, (int)err);
}

static uint32_t FindMemType(uint32_t f, VkMemoryPropertyFlags p) {
    VkPhysicalDeviceMemoryProperties mp;
    vkGetPhysicalDeviceMemoryProperties(GN_Vk.physicalDevice, &mp);
    for(uint32_t i=0;i<mp.memoryTypeCount;i++) if((f&(1<<i))&&(mp.memoryTypes[i].propertyFlags&p)==p) return i;
    return 0;
}

static void CreateBuf(VkDeviceSize sz, VkBufferUsageFlags u, VkMemoryPropertyFlags p, VkBuffer &b, VkDeviceMemory &m) {
    VkBufferCreateInfo ci={VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO}; ci.size=sz; ci.usage=u; ci.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
    VkResult res = vkCreateBuffer(GN_Vk.device,&ci,0,&b);
    VkCheck(res, "vkCreateBuffer in CreateBuf");
    if(res != VK_SUCCESS) return;
    VkMemoryRequirements r; vkGetBufferMemoryRequirements(GN_Vk.device,b,&r);
    VkMemoryAllocateInfo ai={VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO}; ai.allocationSize=r.size; ai.memoryTypeIndex=FindMemType(r.memoryTypeBits,p);
    res = vkAllocateMemory(GN_Vk.device,&ai,0,&m);
    VkCheck(res, "vkAllocateMemory in CreateBuf");
    if(res != VK_SUCCESS) return;
    res = vkBindBufferMemory(GN_Vk.device,b,m,0);
    VkCheck(res, "vkBindBufferMemory in CreateBuf");
}

static VkImageView MakeImageView(VkImage img, VkFormat fmt) {
    VkImageViewCreateInfo ci={VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    ci.image=img; ci.viewType=VK_IMAGE_VIEW_TYPE_2D; ci.format=fmt;
    ci.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT; ci.subresourceRange.levelCount=1; ci.subresourceRange.layerCount=1;
    VkImageView v; VkResult res = vkCreateImageView(GN_Vk.device,&ci,0,&v);
    VkCheck(res, "vkCreateImageView in MakeImageView");
    return v;
}

void GN_VkInit(GLFWwindow *window, int w, int h) {
    if(GN_Vk.initialized) return;
    GN_Vk.viewWidth=w; GN_Vk.viewHeight=h;

    // Instance
    VkApplicationInfo ai={VK_STRUCTURE_TYPE_APPLICATION_INFO}; ai.pApplicationName="GN_mini"; ai.apiVersion=VK_API_VERSION_1_0;
    uint32_t extCount=0; const char **exts=glfwGetRequiredInstanceExtensions(&extCount);
    VkInstanceCreateInfo ici={VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO}; ici.pApplicationInfo=&ai; ici.enabledExtensionCount=extCount; ici.ppEnabledExtensionNames=exts;
    VkCheck(vkCreateInstance(&ici,0,&GN_Vk.instance), "vkCreateInstance");

    // Surface
    glfwCreateWindowSurface(GN_Vk.instance,window,0,&GN_Vk.surface);

    // Physical device
    uint32_t dc=0; vkEnumeratePhysicalDevices(GN_Vk.instance,&dc,0);
    VkPhysicalDevice *devs=new VkPhysicalDevice[dc]; vkEnumeratePhysicalDevices(GN_Vk.instance,&dc,devs);
    GN_Vk.physicalDevice=devs[0]; delete[] devs;

    // Queue family
    uint32_t qc=0; vkGetPhysicalDeviceQueueFamilyProperties(GN_Vk.physicalDevice,&qc,0);
    VkQueueFamilyProperties *qps=new VkQueueFamilyProperties[qc]; vkGetPhysicalDeviceQueueFamilyProperties(GN_Vk.physicalDevice,&qc,qps);
    for(uint32_t i=0;i<qc;i++){VkBool32 pr;vkGetPhysicalDeviceSurfaceSupportKHR(GN_Vk.physicalDevice,i,GN_Vk.surface,&pr);if(qps[i].queueFlags&VK_QUEUE_GRAPHICS_BIT&&pr){GN_Vk.graphicsFamily=i;break;}}
    delete[] qps;

    // Device
    float qp=1.0f; VkDeviceQueueCreateInfo qci={VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO}; qci.queueFamilyIndex=GN_Vk.graphicsFamily; qci.queueCount=1; qci.pQueuePriorities=&qp;
    const char *dexts[]={VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    VkDeviceCreateInfo dci={VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO}; dci.queueCreateInfoCount=1; dci.pQueueCreateInfos=&qci; dci.enabledExtensionCount=1; dci.ppEnabledExtensionNames=dexts;
    VkCheck(vkCreateDevice(GN_Vk.physicalDevice,&dci,0,&GN_Vk.device), "vkCreateDevice");
    vkGetDeviceQueue(GN_Vk.device,GN_Vk.graphicsFamily,0,&GN_Vk.graphicsQueue);

    // Swapchain
    VkSurfaceCapabilitiesKHR caps; vkGetPhysicalDeviceSurfaceCapabilitiesKHR(GN_Vk.physicalDevice,GN_Vk.surface,&caps);
    GN_Vk.swapchainExtent=caps.currentExtent;
    if(GN_Vk.swapchainExtent.width==0xffffffff||GN_Vk.swapchainExtent.width==0){
        GN_Vk.swapchainExtent.width=w>0?w:800; GN_Vk.swapchainExtent.height=h>0?h:600;
    }
    // Query supported formats
    uint32_t fmtCount; vkGetPhysicalDeviceSurfaceFormatsKHR(GN_Vk.physicalDevice,GN_Vk.surface,&fmtCount,0);
    VkSurfaceFormatKHR *fmts=new VkSurfaceFormatKHR[fmtCount]; vkGetPhysicalDeviceSurfaceFormatsKHR(GN_Vk.physicalDevice,GN_Vk.surface,&fmtCount,fmts);
    GN_Vk.swapchainFormat=fmts[0].format; delete[] fmts;
    if(GN_Vk.swapchainFormat==VK_FORMAT_UNDEFINED) GN_Vk.swapchainFormat=VK_FORMAT_B8G8R8A8_SRGB;
    VkSwapchainCreateInfoKHR sci={VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR}; sci.surface=GN_Vk.surface; sci.minImageCount=2;
    sci.imageFormat=GN_Vk.swapchainFormat; sci.imageColorSpace=VK_COLOR_SPACE_SRGB_NONLINEAR_KHR; sci.imageExtent=GN_Vk.swapchainExtent;
    sci.imageArrayLayers=1; sci.imageUsage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; sci.imageSharingMode=VK_SHARING_MODE_EXCLUSIVE;
    sci.preTransform=caps.currentTransform; sci.compositeAlpha=VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; sci.presentMode=VK_PRESENT_MODE_FIFO_KHR;     sci.clipped=VK_TRUE;
    VkCheck(vkCreateSwapchainKHR(GN_Vk.device,&sci,0,&GN_Vk.swapchain), "vkCreateSwapchainKHR");
    vkGetSwapchainImagesKHR(GN_Vk.device,GN_Vk.swapchain,&GN_Vk.swapchainImageCount,0);
    GN_Vk.swapchainImages=new VkImage[GN_Vk.swapchainImageCount]; GN_Vk.swapchainViews=new VkImageView[GN_Vk.swapchainImageCount];
    vkGetSwapchainImagesKHR(GN_Vk.device,GN_Vk.swapchain,&GN_Vk.swapchainImageCount,GN_Vk.swapchainImages);
    for(uint32_t i=0;i<GN_Vk.swapchainImageCount;i++) GN_Vk.swapchainViews[i]=MakeImageView(GN_Vk.swapchainImages[i],GN_Vk.swapchainFormat);

    // Render pass
    VkAttachmentDescription ad={}; ad.format=GN_Vk.swapchainFormat; ad.samples=VK_SAMPLE_COUNT_1_BIT; ad.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR; ad.storeOp=VK_ATTACHMENT_STORE_OP_STORE; ad.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED; ad.finalLayout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    VkAttachmentReference ar={0,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkSubpassDescription sp={}; sp.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS; sp.colorAttachmentCount=1; sp.pColorAttachments=&ar;
    VkRenderPassCreateInfo rpi={VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO}; rpi.attachmentCount=1; rpi.pAttachments=&ad; rpi.subpassCount=1; rpi.pSubpasses=&sp;
    VkCheck(vkCreateRenderPass(GN_Vk.device,&rpi,0,&GN_Vk.renderPass), "vkCreateRenderPass");

    // Shaders — 必须从 .spv 文件加载 (make shaders 编译 GLSL→SPIR-V)
    #include "gn2d_vert_spv.h"
    #include "gn2d_frag_spv.h"
    VkShaderModuleCreateInfo sm={VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    VkShaderModule vm,fm;

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
    sm.codeSize=vertSpv.size()*sizeof(uint32_t); sm.pCode=vertSpv.data();
    vkCreateShaderModule(GN_Vk.device,&sm,0,&vm);

    sm.codeSize=fragSpv.size()*sizeof(uint32_t); sm.pCode=fragSpv.data();
    vkCreateShaderModule(GN_Vk.device,&sm,0,&fm);

    // Pipeline
    VkPipelineShaderStageCreateInfo ss[2]={{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO},{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO}};
    ss[0].stage=VK_SHADER_STAGE_VERTEX_BIT; ss[0].module=vm; ss[0].pName="main";
    ss[1].stage=VK_SHADER_STAGE_FRAGMENT_BIT; ss[1].module=fm; ss[1].pName="main";

    VkVertexInputBindingDescription vib={0,sizeof(VkVertex),VK_VERTEX_INPUT_RATE_VERTEX};
    VkVertexInputAttributeDescription via[4]={{0,0,VK_FORMAT_R32G32_SFLOAT,offsetof(VkVertex,x)},{1,0,VK_FORMAT_R32G32_SFLOAT,offsetof(VkVertex,u)},{2,0,VK_FORMAT_R32G32B32A32_SFLOAT,offsetof(VkVertex,r)}};
    VkPipelineVertexInputStateCreateInfo vis={VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO}; vis.vertexBindingDescriptionCount=1; vis.pVertexBindingDescriptions=&vib; vis.vertexAttributeDescriptionCount=3; vis.pVertexAttributeDescriptions=via;
    VkPipelineInputAssemblyStateCreateInfo ias={VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO}; ias.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkViewport vp={0,0,(float)GN_Vk.swapchainExtent.width,(float)GN_Vk.swapchainExtent.height,0,1};
    VkRect2D sc={{0,0},GN_Vk.swapchainExtent};
    VkPipelineViewportStateCreateInfo vps={VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO}; vps.viewportCount=1; vps.pViewports=&vp; vps.scissorCount=1; vps.pScissors=&sc;
    VkPipelineRasterizationStateCreateInfo rs={VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO}; rs.polygonMode=VK_POLYGON_MODE_FILL; rs.cullMode=VK_CULL_MODE_BACK_BIT; rs.frontFace=VK_FRONT_FACE_CLOCKWISE; rs.lineWidth=1.0f;
    VkPipelineMultisampleStateCreateInfo ms={VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO}; ms.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;
    VkPipelineColorBlendAttachmentState cba={}; cba.blendEnable=VK_TRUE; cba.srcColorBlendFactor=VK_BLEND_FACTOR_SRC_ALPHA; cba.dstColorBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA; cba.colorBlendOp=VK_BLEND_OP_ADD; cba.srcAlphaBlendFactor=VK_BLEND_FACTOR_ONE; cba.dstAlphaBlendFactor=VK_BLEND_FACTOR_ZERO; cba.colorWriteMask=0xf;
    VkPipelineColorBlendStateCreateInfo cbs={VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO}; cbs.attachmentCount=1; cbs.pAttachments=&cba;

    // Dynamic state: scissor (for UI clipping)
    VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dsci={VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dsci.dynamicStateCount=1; dsci.pDynamicStates=dynamicStates;

    VkDescriptorSetLayoutBinding dslb[2]={}; dslb[0].binding=0; dslb[0].descriptorType=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; dslb[0].descriptorCount=1; dslb[0].stageFlags=VK_SHADER_STAGE_VERTEX_BIT;
    dslb[1].binding=1; dslb[1].descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; dslb[1].descriptorCount=1; dslb[1].stageFlags=VK_SHADER_STAGE_FRAGMENT_BIT;
    VkDescriptorSetLayoutCreateInfo dslci={VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO}; dslci.bindingCount=2; dslci.pBindings=dslb;
    VkCheck(vkCreateDescriptorSetLayout(GN_Vk.device,&dslci,0,&GN_Vk.descriptorLayout), "vkCreateDescriptorSetLayout");
    VkPipelineLayoutCreateInfo plci={VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO}; plci.setLayoutCount=1; plci.pSetLayouts=&GN_Vk.descriptorLayout;
    VkCheck(vkCreatePipelineLayout(GN_Vk.device,&plci,0,&GN_Vk.pipelineLayout), "vkCreatePipelineLayout");

    VkGraphicsPipelineCreateInfo pci={VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO}; pci.stageCount=2; pci.pStages=ss; pci.pVertexInputState=&vis; pci.pInputAssemblyState=&ias; pci.pViewportState=&vps; pci.pRasterizationState=&rs; pci.pMultisampleState=&ms; pci.pColorBlendState=&cbs; pci.pDynamicState=&dsci; pci.layout=GN_Vk.pipelineLayout; pci.renderPass=GN_Vk.renderPass; pci.subpass=0;
    VkCheck(vkCreateGraphicsPipelines(GN_Vk.device,0,1,&pci,0,&GN_Vk.pipeline), "vkCreateGraphicsPipelines");
    // Additive blend pipeline (also needs dynamic state)
    pci.pDynamicState=&dsci;
    // Additive blend pipeline (eBLEND_BRIGHT)
    {   VkPipelineColorBlendAttachmentState cbaAdd = cba;
        cbaAdd.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
        VkPipelineColorBlendStateCreateInfo cbsAdd = cbs;
        cbsAdd.pAttachments = &cbaAdd;
        pci.pColorBlendState = &cbsAdd;
        vkCreateGraphicsPipelines(GN_Vk.device,0,1,&pci,0,&GN_Vk.pipelineBright);
    }
    // Multiply blend pipeline (eBLEND_MODULATE = 11)
    {   VkPipelineColorBlendAttachmentState cbaMul = cba;
        cbaMul.srcColorBlendFactor = VK_BLEND_FACTOR_DST_COLOR;
        cbaMul.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
        VkPipelineColorBlendStateCreateInfo cbsMul = cbs;
        cbsMul.pAttachments = &cbaMul;
        pci.pColorBlendState = &cbsMul;
        vkCreateGraphicsPipelines(GN_Vk.device,0,1,&pci,0,&GN_Vk.pipelineMultiply);
    }
    // No-blend pipeline (eBLEND_NULL = 0)
    {   VkPipelineColorBlendAttachmentState cbaNull = cba;
        cbaNull.blendEnable = VK_FALSE;
        VkPipelineColorBlendStateCreateInfo cbsNull = cbs;
        cbsNull.pAttachments = &cbaNull;
        pci.pColorBlendState = &cbsNull;
        vkCreateGraphicsPipelines(GN_Vk.device,0,1,&pci,0,&GN_Vk.pipelineNull);
    }
    vkDestroyShaderModule(GN_Vk.device,vm,0); vkDestroyShaderModule(GN_Vk.device,fm,0);

    // Framebuffers
    GN_Vk.framebuffers=new VkFramebuffer[GN_Vk.swapchainImageCount];
    for(uint32_t i=0;i<GN_Vk.swapchainImageCount;i++){VkFramebufferCreateInfo fci={VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO}; fci.renderPass=GN_Vk.renderPass; fci.attachmentCount=1; fci.pAttachments=&GN_Vk.swapchainViews[i]; fci.width=GN_Vk.swapchainExtent.width; fci.height=GN_Vk.swapchainExtent.height; fci.layers=1; vkCreateFramebuffer(GN_Vk.device,&fci,0,&GN_Vk.framebuffers[i]);}

    // Command pool
    VkCommandPoolCreateInfo cpci={VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO}; cpci.queueFamilyIndex=GN_Vk.graphicsFamily;
    VkCheck(vkCreateCommandPool(GN_Vk.device,&cpci,0,&GN_Vk.commandPool), "vkCreateCommandPool");
    GN_Vk.commandBuffers=new VkCommandBuffer[GN_Vk.swapchainImageCount];
    VkCommandBufferAllocateInfo cbai={VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO}; cbai.commandPool=GN_Vk.commandPool; cbai.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY; cbai.commandBufferCount=GN_Vk.swapchainImageCount;
    vkAllocateCommandBuffers(GN_Vk.device,&cbai,GN_Vk.commandBuffers);

    // UBO (必须在默认纹理之前创建)
    CreateBuf(sizeof(VkUBO),VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,GN_Vk.uniformBuffer,GN_Vk.uniformMemory);

    // Vertex buffer (预分配足够大，避免运行时 GrowVBufIfNeeded 销毁重建)
    GN_Vk.vertexCapacity=524288;  // 512K 顶点 ≈ 16MB
    CreateBuf(GN_Vk.vertexCapacity*sizeof(VkVertex),VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,GN_Vk.vertexBuffer,GN_Vk.vertexMemory);

    // Sampler (必须在默认纹理之前创建)
    VkSamplerCreateInfo sci2={VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO}; sci2.magFilter=VK_FILTER_LINEAR; sci2.minFilter=VK_FILTER_LINEAR; sci2.addressModeU=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE; sci2.addressModeV=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    vkCreateSampler(GN_Vk.device,&sci2,0,&GN_Vk.defaultSampler);

    // Descriptor pool (必须在默认纹理之前创建)
    VkDescriptorPoolSize dps[2]={{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1024},{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1024}};
    VkDescriptorPoolCreateInfo dpci={VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO}; dpci.maxSets=1024; dpci.poolSizeCount=2; dpci.pPoolSizes=dps;
    vkCreateDescriptorPool(GN_Vk.device,&dpci,0,&GN_Vk.descriptorPool);

    // ── 创建 1x1 白色默认纹理 (用于纯色绘制) ──────────────
    // 注意: 必须在 UBO / sampler / descriptorPool 创建之后，因为它们被 descriptor set 引用
    {
        // 使用 host-visible 内存直接写入，避免复杂的 staging
        VkImageCreateInfo ii={VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        ii.imageType=VK_IMAGE_TYPE_2D; ii.extent={1,1,1}; ii.mipLevels=1; ii.arrayLayers=1;
        ii.format=VK_FORMAT_R8G8B8A8_UNORM; ii.tiling=VK_IMAGE_TILING_LINEAR;
        ii.initialLayout=VK_IMAGE_LAYOUT_PREINITIALIZED;
        ii.usage=VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        ii.samples=VK_SAMPLE_COUNT_1_BIT; ii.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
        VkImage dimg; VkDeviceMemory dmem;
        if(vkCreateImage(GN_Vk.device,&ii,0,&dimg)==VK_SUCCESS){
            GN_Vk.defaultImage = dimg; // 存储用于 shutdown 清理
            VkMemoryRequirements mr; vkGetImageMemoryRequirements(GN_Vk.device,dimg,&mr);
            VkMemoryAllocateInfo ai={VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
            ai.allocationSize=mr.size;
            ai.memoryTypeIndex=FindMemType(mr.memoryTypeBits,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            if(vkAllocateMemory(GN_Vk.device,&ai,0,&dmem)==VK_SUCCESS){
                GN_Vk.defaultImageMemory = dmem; // 存储用于 shutdown 清理
                vkBindImageMemory(GN_Vk.device,dimg,dmem,0);
                // 直接写入白色像素
                VkImageSubresource sub={VK_IMAGE_ASPECT_COLOR_BIT,0,0};
                VkSubresourceLayout layout;
                vkGetImageSubresourceLayout(GN_Vk.device,dimg,&sub,&layout);
                void *d; vkMapMemory(GN_Vk.device,dmem,0,VK_WHOLE_SIZE,0,&d);
                uint32_t *px=(uint32_t*)((char*)d+layout.offset);
                *px=0xFFFFFFFF; // 白色
                vkUnmapMemory(GN_Vk.device,dmem);
                // 转换为 shader-readable layout
                VkCommandBuffer cb; {VkCommandBufferAllocateInfo ai2={VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};ai2.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;ai2.commandPool=GN_Vk.commandPool;ai2.commandBufferCount=1;vkAllocateCommandBuffers(GN_Vk.device,&ai2,&cb);}
                {VkCommandBufferBeginInfo bi={VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};bi.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;vkBeginCommandBuffer(cb,&bi);}
                {VkImageMemoryBarrier b={VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};b.oldLayout=VK_IMAGE_LAYOUT_PREINITIALIZED;b.newLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;b.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;b.image=dimg;b.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;b.subresourceRange.levelCount=1;b.subresourceRange.layerCount=1;b.srcAccessMask=VK_ACCESS_HOST_WRITE_BIT;b.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;vkCmdPipelineBarrier(cb,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,0,0,0,0,0,1,&b);}
                vkEndCommandBuffer(cb);
                {VkSubmitInfo si={VK_STRUCTURE_TYPE_SUBMIT_INFO};si.commandBufferCount=1;si.pCommandBuffers=&cb;vkQueueSubmit(GN_Vk.graphicsQueue,1,&si,VK_NULL_HANDLE);vkQueueWaitIdle(GN_Vk.graphicsQueue);}
                vkFreeCommandBuffers(GN_Vk.device,GN_Vk.commandPool,1,&cb);
                // 创建 image view 和 descriptor set
                VkImageView div=MakeImageView(dimg,VK_FORMAT_R8G8B8A8_UNORM);
                GN_Vk.defaultImageView = div;  // 存储供 3D 无纹理绘制降级
                VkDescriptorSetAllocateInfo dai={VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
                dai.descriptorPool=GN_Vk.descriptorPool;dai.descriptorSetCount=1;dai.pSetLayouts=&GN_Vk.descriptorLayout;
                vkAllocateDescriptorSets(GN_Vk.device,&dai,&GN_Vk.defaultDS);
                VkDescriptorBufferInfo dbi={GN_Vk.uniformBuffer,0,sizeof(VkUBO)};
                VkDescriptorImageInfo dii={GN_Vk.defaultSampler,div,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
                VkWriteDescriptorSet w[2]={{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET},{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET}};
                w[0].dstSet=GN_Vk.defaultDS;w[0].dstBinding=0;w[0].descriptorType=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;w[0].descriptorCount=1;w[0].pBufferInfo=&dbi;
                w[1].dstSet=GN_Vk.defaultDS;w[1].dstBinding=1;w[1].descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;w[1].descriptorCount=1;w[1].pImageInfo=&dii;
                vkUpdateDescriptorSets(GN_Vk.device,2,w,0,0);
            }
        }
    }

    // Sync
    VkSemaphoreCreateInfo sci3={VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO}; vkCreateSemaphore(GN_Vk.device,&sci3,0,&GN_Vk.imageAvailable); vkCreateSemaphore(GN_Vk.device,&sci3,0,&GN_Vk.renderFinished);
    VkFenceCreateInfo fci2={VK_STRUCTURE_TYPE_FENCE_CREATE_INFO}; fci2.flags=VK_FENCE_CREATE_SIGNALED_BIT;     vkCreateFence(GN_Vk.device,&fci2,0,&GN_Vk.inFlightFence);

#ifdef GN_USING_3D
    GN_VkInit3D();
#endif

    GN_Vk.initialized=true;
}

void GN_VkShutdown() {
    if(!GN_Vk.initialized) return;
    vkDeviceWaitIdle(GN_Vk.device);
#ifdef GN_USING_3D
    GN_VkShutdown3D();
#endif
    vkDestroySemaphore(GN_Vk.device,GN_Vk.imageAvailable,0); vkDestroySemaphore(GN_Vk.device,GN_Vk.renderFinished,0);
    vkDestroyFence(GN_Vk.device,GN_Vk.inFlightFence,0);
    vkDestroyDescriptorPool(GN_Vk.device,GN_Vk.descriptorPool,0);
    vkDestroySampler(GN_Vk.device,GN_Vk.defaultSampler,0);
    // 清理默认纹理资源 (1x1 白色)
    if(GN_Vk.defaultImageView != VK_NULL_HANDLE){ vkDestroyImageView(GN_Vk.device,GN_Vk.defaultImageView,0); GN_Vk.defaultImageView = VK_NULL_HANDLE; }
    if(GN_Vk.defaultImage != VK_NULL_HANDLE){ vkDestroyImage(GN_Vk.device,GN_Vk.defaultImage,0); GN_Vk.defaultImage = VK_NULL_HANDLE; }
    if(GN_Vk.defaultImageMemory != VK_NULL_HANDLE){ vkFreeMemory(GN_Vk.device,GN_Vk.defaultImageMemory,0); GN_Vk.defaultImageMemory = VK_NULL_HANDLE; }
    vkDestroyBuffer(GN_Vk.device,GN_Vk.vertexBuffer,0); vkFreeMemory(GN_Vk.device,GN_Vk.vertexMemory,0);
    vkDestroyBuffer(GN_Vk.device,GN_Vk.uniformBuffer,0); vkFreeMemory(GN_Vk.device,GN_Vk.uniformMemory,0);
    vkDestroyCommandPool(GN_Vk.device,GN_Vk.commandPool,0);
    for(uint32_t i=0;i<GN_Vk.swapchainImageCount;i++) vkDestroyFramebuffer(GN_Vk.device,GN_Vk.framebuffers[i],0);
    delete[] GN_Vk.framebuffers; delete[] GN_Vk.commandBuffers;
    for(uint32_t i=0;i<GN_Vk.swapchainImageCount;i++) vkDestroyImageView(GN_Vk.device,GN_Vk.swapchainViews[i],0);
    delete[] GN_Vk.swapchainViews; delete[] GN_Vk.swapchainImages;
    vkDestroySwapchainKHR(GN_Vk.device,GN_Vk.swapchain,0);
    vkDestroyPipeline(GN_Vk.device,GN_Vk.pipeline,0); vkDestroyPipeline(GN_Vk.device,GN_Vk.pipelineBright,0); vkDestroyPipeline(GN_Vk.device,GN_Vk.pipelineMultiply,0); vkDestroyPipeline(GN_Vk.device,GN_Vk.pipelineNull,0); vkDestroyPipelineLayout(GN_Vk.device,GN_Vk.pipelineLayout,0);
    vkDestroyDescriptorSetLayout(GN_Vk.device,GN_Vk.descriptorLayout,0);
    if(s_rtFramebuffer){vkDestroyFramebuffer(GN_Vk.device,s_rtFramebuffer,0);s_rtFramebuffer=VK_NULL_HANDLE;}
    if(s_rtRenderPass){vkDestroyRenderPass(GN_Vk.device,s_rtRenderPass,0);s_rtRenderPass=VK_NULL_HANDLE;}
    vkDestroyRenderPass(GN_Vk.device,GN_Vk.renderPass,0);
    vkDestroyDevice(GN_Vk.device,0); vkDestroySurfaceKHR(GN_Vk.instance,GN_Vk.surface,0); vkDestroyInstance(GN_Vk.instance,0);
    GN_Vk.initialized=false;
}

// ═══════════════════════════════════════════════════════════
//  Scissor 裁剪状态
// ═══════════════════════════════════════════════════════════
static VkRect2D s_currentScissor = {{0,0},{0,0}};
static bool    s_scissorEnabled = false;
// pass内DataFill暂存的staging buffer, 在GN_VkEndFrame释放
static VkBuffer s_pendingStagingBufs[32]; static VkDeviceMemory s_pendingStagingMems[32]; static int s_pendingStagingCnt = 0;
// 延迟纹理上传: pass内记录, 帧首GN_VkBeginFrame处理
struct sPendingUpload { sIMAGE *img; uint8 *data; int w,h,fmt; };
static sPendingUpload s_pendingUploads[256];  // 支持大量纹理(文件浏览器等)
static int s_pendingUploadCnt = 0;
static bool    s_scissorDirty   = false; // 首次由 GN_VkBeginFrame 设置

void GN_VkSetScissor(int x, int y, int w, int h, bool enable) {
    s_scissorEnabled = enable;
    if(enable) {
        s_currentScissor.offset = {x, y};
        s_currentScissor.extent = {(uint32_t)w, (uint32_t)h};
    } else {
        s_currentScissor.offset = {0, 0};
        s_currentScissor.extent = GN_Vk.swapchainExtent;
    }
    s_scissorDirty = true;
}

// 帧循环
static VkVertex *gVBuf=0; static uint32_t gVMax=0,gVCnt=0;
static VkDescriptorSet s_lastDS = VK_NULL_HANDLE;
static VkDescriptorSet s_batchDS[256];
static uint32_t s_batchEnd[256];  // cumulative vertex count at each batch boundary
static uint32_t s_batchCount = 0;

// Record batch boundary when descriptor set changes
static void BatchSwitch(VkDescriptorSet ds) {
    if(ds == s_lastDS) return;
    if(gVCnt > 0 && s_batchCount < 255) {
        s_batchEnd[s_batchCount] = gVCnt;
        s_batchDS[s_batchCount] = s_lastDS;
        s_batchCount++;
    }
    s_lastDS = ds;
}

// Flush current batch and apply new scissor if it changed
static void FlushBatchIfScissorDirty() {
    if(!s_scissorDirty) return;
    // Force batch boundary before scissor change
    if(gVCnt > 0 && s_batchCount < 255 && s_lastDS != VK_NULL_HANDLE) {
        s_batchEnd[s_batchCount] = gVCnt;
        s_batchDS[s_batchCount] = s_lastDS;
        s_batchCount++;
        s_lastDS = VK_NULL_HANDLE; // force new batch
    }
    vkCmdSetScissor(GN_Vk.commandBuffers[GN_Vk.currentFrame], 0, 1, &s_currentScissor);
    s_scissorDirty = false;
}

// ═══════════════════════════════════════════════════════════
//  RenderTarget 离屏渲染 (定义在帧循环变量之后)
// ═══════════════════════════════════════════════════════════
sIMAGE  *s_activeRT        = nullptr;
bool     s_rtInProgress    = false;

static VkRenderPass CreateRTRenderPass() {
    VkAttachmentDescription ad={};
    ad.format=VK_FORMAT_R8G8B8A8_UNORM;
    ad.samples=VK_SAMPLE_COUNT_1_BIT;
    ad.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR;
    ad.storeOp=VK_ATTACHMENT_STORE_OP_STORE;
    ad.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;
    ad.finalLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    VkAttachmentReference ar={0,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkSubpassDescription sp={};
    sp.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS;
    sp.colorAttachmentCount=1; sp.pColorAttachments=&ar;
    VkRenderPassCreateInfo rpi={VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    rpi.attachmentCount=1; rpi.pAttachments=&ad;
    rpi.subpassCount=1; rpi.pSubpasses=&sp;
    VkRenderPass rp;
    VkResult res = vkCreateRenderPass(GN_Vk.device,&rpi,0,&rp);
    VkCheck(res, "vkCreateRenderPass in CreateRTRenderPass");
    return rp;
}

static void FlushVertexBatch() {
    if(gVCnt == 0) return;
    if(s_batchCount < 256 && s_lastDS != VK_NULL_HANDLE) {
        s_batchEnd[s_batchCount] = gVCnt;
        s_batchDS[s_batchCount] = s_lastDS;
        s_batchCount++;
    }
    void *d; vkMapMemory(GN_Vk.device,GN_Vk.vertexMemory,0,gVCnt*sizeof(VkVertex),0,&d);
    memcpy(d,gVBuf,gVCnt*sizeof(VkVertex)); vkUnmapMemory(GN_Vk.device,GN_Vk.vertexMemory);
    uint32_t prevEnd = 0;
    for(uint32_t i = 0; i < s_batchCount; i++) {
        uint32_t count = s_batchEnd[i] - prevEnd;
        if(count == 0) continue;
        vkCmdBindDescriptorSets(GN_Vk.commandBuffers[GN_Vk.currentFrame],
            VK_PIPELINE_BIND_POINT_GRAPHICS,GN_Vk.pipelineLayout,0,1,&s_batchDS[i],0,0);
        VkDeviceSize offs[]={prevEnd * sizeof(VkVertex)};
        vkCmdBindVertexBuffers(GN_Vk.commandBuffers[GN_Vk.currentFrame],0,1,&GN_Vk.vertexBuffer,offs);
        vkCmdDraw(GN_Vk.commandBuffers[GN_Vk.currentFrame],count,1,0,0);
        prevEnd = s_batchEnd[i];
    }
    gVCnt=0; s_batchCount=0; s_lastDS=VK_NULL_HANDLE;
}

static void BeginRTRenderPass() {
    if(!s_activeRT || !s_activeRT->vkImage || s_rtInProgress) return;
    FlushVertexBatch();
    if(s_inRenderPass) {
        vkCmdEndRenderPass(GN_Vk.commandBuffers[GN_Vk.currentFrame]);
        s_inRenderPass = false;
    }
    {VkImageMemoryBarrier b={VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    b.oldLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    b.newLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    b.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
    b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
    b.image=s_activeRT->vkImage;
    b.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;
    b.subresourceRange.levelCount=1; b.subresourceRange.layerCount=1;
    b.srcAccessMask=VK_ACCESS_SHADER_READ_BIT;
    b.dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    vkCmdPipelineBarrier(GN_Vk.commandBuffers[GN_Vk.currentFrame],
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        0, 0,0,0,0, 1, &b);}
    if(!s_rtRenderPass) s_rtRenderPass = CreateRTRenderPass();
    if(s_rtFramebuffer) vkDestroyFramebuffer(GN_Vk.device,s_rtFramebuffer,0);
    {VkFramebufferCreateInfo fci={VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    fci.renderPass=s_rtRenderPass;
    fci.attachmentCount=1; fci.pAttachments=&s_activeRT->vkImageView;
    fci.width=s_activeRT->width; fci.height=s_activeRT->height; fci.layers=1;
    VkResult res = vkCreateFramebuffer(GN_Vk.device,&fci,0,&s_rtFramebuffer);
    VkCheck(res, "vkCreateFramebuffer in BeginRTRenderPass");}
    {VkClearValue cv={{{0.0f,0.0f,0.0f,0.0f}}};
    VkRenderPassBeginInfo rpi={VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    rpi.renderPass=s_rtRenderPass; rpi.framebuffer=s_rtFramebuffer;
    rpi.renderArea={{0,0},{s_activeRT->width, s_activeRT->height}};
    rpi.clearValueCount=1; rpi.pClearValues=&cv;
    vkCmdBeginRenderPass(GN_Vk.commandBuffers[GN_Vk.currentFrame],&rpi,VK_SUBPASS_CONTENTS_INLINE);}
    vkCmdBindPipeline(GN_Vk.commandBuffers[GN_Vk.currentFrame],VK_PIPELINE_BIND_POINT_GRAPHICS,GN_Vk.pipeline);
    {VkRect2D sc; sc.offset.x=0;sc.offset.y=0; sc.extent.width=s_activeRT->width;sc.extent.height=s_activeRT->height;
    vkCmdSetScissor(GN_Vk.commandBuffers[GN_Vk.currentFrame],0,1,&sc);}
    {VkUBO ubo; float proj[16]={2.0f/s_activeRT->width,0,0,0, 0,2.0f/s_activeRT->height,0,0, 0,0,-1,0, -1,-1,0,1};
    memcpy(ubo.proj,proj,sizeof(proj));
    void *data; vkMapMemory(GN_Vk.device,GN_Vk.uniformMemory,0,sizeof(ubo),0,&data);
    memcpy(data,&ubo,sizeof(ubo)); vkUnmapMemory(GN_Vk.device,GN_Vk.uniformMemory);}
    GN_Vk.vertexCount=0;
    s_rtInProgress = true;
}

static void EndRTRenderPass() {
    if(!s_rtInProgress) return;
    FlushVertexBatch();
    vkCmdEndRenderPass(GN_Vk.commandBuffers[GN_Vk.currentFrame]);
    {VkImageMemoryBarrier b={VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    b.oldLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    b.newLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    b.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
    b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
    b.image=s_activeRT->vkImage;
    b.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;
    b.subresourceRange.levelCount=1; b.subresourceRange.layerCount=1;
    b.srcAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    b.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(GN_Vk.commandBuffers[GN_Vk.currentFrame],
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0, 0,0,0,0, 1, &b);}
    {VkClearValue cv={{{0.0f,0.0f,0.0f,1.0f}}};
    VkRenderPassBeginInfo rpi={VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    rpi.renderPass=GN_Vk.renderPass; rpi.framebuffer=GN_Vk.framebuffers[GN_Vk.currentFrame];
    rpi.renderArea={{0,0},GN_Vk.swapchainExtent};
    rpi.clearValueCount=1; rpi.pClearValues=&cv;
    vkCmdBeginRenderPass(GN_Vk.commandBuffers[GN_Vk.currentFrame],&rpi,VK_SUBPASS_CONTENTS_INLINE);}
    vkCmdBindPipeline(GN_Vk.commandBuffers[GN_Vk.currentFrame],VK_PIPELINE_BIND_POINT_GRAPHICS,GN_Vk.pipeline);
    {VkRect2D sc; sc.offset.x=0;sc.offset.y=0; sc.extent=GN_Vk.swapchainExtent;
    vkCmdSetScissor(GN_Vk.commandBuffers[GN_Vk.currentFrame],0,1,&sc);}
    {VkUBO ubo; float L=0,R=(float)GN_Vk.swapchainExtent.width,T=(float)GN_Vk.swapchainExtent.height,B=0;
    float proj[16]={2.0f/(R-L),0,0,0, 0,2.0f/(T-B),0,0, 0,0,-1,0, -(R+L)/(R-L),-(T+B)/(T-B),0,1};
    memcpy(ubo.proj,proj,sizeof(proj));
    void *data; vkMapMemory(GN_Vk.device,GN_Vk.uniformMemory,0,sizeof(ubo),0,&data);
    memcpy(data,&ubo,sizeof(ubo)); vkUnmapMemory(GN_Vk.device,GN_Vk.uniformMemory);}
    GN_Vk.vertexCount=0;
    s_rtInProgress = false;
    s_inRenderPass = true;  // 已切回主渲染通道
    s_activeRT = nullptr;
}

bool GN_VkBeginFrame() {
    if(!GN_Vk.initialized) return false;
    // TODO: 处理pass内延迟的纹理上传 (临时禁用排查崩溃)
    // for(int i=0;i<s_pendingUploadCnt;i++){ ... }
    // 注意: 必须在vkWaitForFences之后处理, 否则GPU还在用旧帧资源!
    vkWaitForFences(GN_Vk.device,1,&GN_Vk.inFlightFence,VK_TRUE,UINT64_MAX);
    // 处理pass内延迟的纹理上传 (必须在vkWaitForFences之后)
    for(int i=0;i<s_pendingUploadCnt;i++){
        if(s_pendingUploads[i].img && s_pendingUploads[i].data){
            s_pendingUploads[i].img->DataFill(s_pendingUploads[i].data,
                s_pendingUploads[i].fmt, s_pendingUploads[i].w, s_pendingUploads[i].h, 0, 0);
        }
        delete[] s_pendingUploads[i].data;
    }
    s_pendingUploadCnt = 0;
    vkResetFences(GN_Vk.device,1,&GN_Vk.inFlightFence);
    VkResult res=vkAcquireNextImageKHR(GN_Vk.device,GN_Vk.swapchain,UINT64_MAX,GN_Vk.imageAvailable,0,&GN_Vk.currentFrame);
    if(res!=VK_SUCCESS&&res!=VK_SUBOPTIMAL_KHR) return false;
    vkResetCommandBuffer(GN_Vk.commandBuffers[GN_Vk.currentFrame],0);
    VkCommandBufferBeginInfo bi={VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO}; bi.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(GN_Vk.commandBuffers[GN_Vk.currentFrame],&bi);
    VkClearValue cv={{{0.0f,0.0f,0.0f,1.0f}}};
    VkRenderPassBeginInfo rpi={VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO}; rpi.renderPass=GN_Vk.renderPass; rpi.framebuffer=GN_Vk.framebuffers[GN_Vk.currentFrame]; rpi.renderArea={{0,0},GN_Vk.swapchainExtent}; rpi.clearValueCount=1; rpi.pClearValues=&cv;
    vkCmdBeginRenderPass(GN_Vk.commandBuffers[GN_Vk.currentFrame],&rpi,VK_SUBPASS_CONTENTS_INLINE);
    s_inRenderPass = true; // 标记2D渲染通道活跃
    vkCmdBindPipeline(GN_Vk.commandBuffers[GN_Vk.currentFrame],VK_PIPELINE_BIND_POINT_GRAPHICS,GN_Vk.pipeline);
    // 初始化动态 scissor 为全视口
    {VkRect2D fs; fs.offset.x=0;fs.offset.y=0; fs.extent=GN_Vk.swapchainExtent;
    vkCmdSetScissor(GN_Vk.commandBuffers[GN_Vk.currentFrame],0,1,&fs);}
    VkUBO ubo; float L=0,R=(float)GN_Vk.swapchainExtent.width,T=(float)GN_Vk.swapchainExtent.height,B=0;
    float proj[16]={2.0f/(R-L),0,0,0, 0,2.0f/(T-B),0,0, 0,0,-1,0, -(R+L)/(R-L),-(T+B)/(T-B),0,1};
    memcpy(ubo.proj,proj,sizeof(proj));
    void *data; vkMapMemory(GN_Vk.device,GN_Vk.uniformMemory,0,sizeof(ubo),0,&data); memcpy(data,&ubo,sizeof(ubo)); vkUnmapMemory(GN_Vk.device,GN_Vk.uniformMemory);
    GN_Vk.vertexCount=0;
    gVCnt=0; s_batchCount=0; s_lastDS=VK_NULL_HANDLE; // reset for new frame
    return true;
}

void GN_VkEndFrame() {
    if(!GN_Vk.initialized) return;
    // Close final batch
    if(gVCnt > 0 && s_batchCount < 256) {
        s_batchEnd[s_batchCount] = gVCnt;
        s_batchDS[s_batchCount] = s_lastDS;
        s_batchCount++;
    }
    // Upload all vertices at once
    if(gVCnt > 0) {
        void *d; vkMapMemory(GN_Vk.device,GN_Vk.vertexMemory,0,gVCnt*sizeof(VkVertex),0,&d);
        memcpy(d,gVBuf,gVCnt*sizeof(VkVertex)); vkUnmapMemory(GN_Vk.device,GN_Vk.vertexMemory);
    }
    // Draw each batch with its own descriptor set and vertex offset
    uint32_t prevEnd = 0;
    for(uint32_t i = 0; i < s_batchCount; i++) {
        uint32_t count = s_batchEnd[i] - prevEnd;
        if(count == 0) continue;
        vkCmdBindDescriptorSets(GN_Vk.commandBuffers[GN_Vk.currentFrame],
            VK_PIPELINE_BIND_POINT_GRAPHICS,GN_Vk.pipelineLayout,0,1,&s_batchDS[i],0,0);
        VkDeviceSize offs[]={prevEnd * sizeof(VkVertex)};
        vkCmdBindVertexBuffers(GN_Vk.commandBuffers[GN_Vk.currentFrame],0,1,&GN_Vk.vertexBuffer,offs);
        vkCmdDraw(GN_Vk.commandBuffers[GN_Vk.currentFrame],count,1,0,0);
        prevEnd = s_batchEnd[i];
    }
    gVCnt=0; s_batchCount=0;
    // 仅在有活跃渲染通道时结束它
    if(s_inRenderPass){
        vkCmdEndRenderPass(GN_Vk.commandBuffers[GN_Vk.currentFrame]);
        s_inRenderPass = false;
    }
    vkEndCommandBuffer(GN_Vk.commandBuffers[GN_Vk.currentFrame]);
    VkSubmitInfo si={VK_STRUCTURE_TYPE_SUBMIT_INFO}; VkPipelineStageFlags ws[]={VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    si.waitSemaphoreCount=1; si.pWaitSemaphores=&GN_Vk.imageAvailable; si.pWaitDstStageMask=ws; si.commandBufferCount=1; si.pCommandBuffers=&GN_Vk.commandBuffers[GN_Vk.currentFrame]; si.signalSemaphoreCount=1; si.pSignalSemaphores=&GN_Vk.renderFinished;
    vkQueueSubmit(GN_Vk.graphicsQueue,1,&si,GN_Vk.inFlightFence);
    VkPresentInfoKHR pi={VK_STRUCTURE_TYPE_PRESENT_INFO_KHR}; pi.waitSemaphoreCount=1; pi.pWaitSemaphores=&GN_Vk.renderFinished; pi.swapchainCount=1; pi.pSwapchains=&GN_Vk.swapchain; pi.pImageIndices=&GN_Vk.currentFrame;
    vkQueuePresentKHR(GN_Vk.graphicsQueue,&pi);
}

void GN_VkResize(int w, int h) {
    if(w <= 0 || h <= 0) return;
    GN_Vk.viewWidth = w;
    GN_Vk.viewHeight = h;
    if(!GN_Vk.initialized) return;
    // 重建 swapchain 和相关资源以适应新尺寸
    vkDeviceWaitIdle(GN_Vk.device);
    // 销毁旧 swapchain 资源
    for(uint32_t i=0;i<GN_Vk.swapchainImageCount;i++) vkDestroyFramebuffer(GN_Vk.device,GN_Vk.framebuffers[i],0);
    delete[] GN_Vk.framebuffers;
    for(uint32_t i=0;i<GN_Vk.swapchainImageCount;i++) vkDestroyImageView(GN_Vk.device,GN_Vk.swapchainViews[i],0);
    delete[] GN_Vk.swapchainViews; delete[] GN_Vk.swapchainImages;
    vkDestroySwapchainKHR(GN_Vk.device,GN_Vk.swapchain,0);
    // 重建 swapchain
    VkSurfaceCapabilitiesKHR caps; vkGetPhysicalDeviceSurfaceCapabilitiesKHR(GN_Vk.physicalDevice,GN_Vk.surface,&caps);
    GN_Vk.swapchainExtent = caps.currentExtent;
    if(GN_Vk.swapchainExtent.width==0xffffffff||GN_Vk.swapchainExtent.width==0){
        GN_Vk.swapchainExtent.width=(uint32_t)w; GN_Vk.swapchainExtent.height=(uint32_t)h;
    }
    uint32_t fmtCount; vkGetPhysicalDeviceSurfaceFormatsKHR(GN_Vk.physicalDevice,GN_Vk.surface,&fmtCount,0);
    VkSurfaceFormatKHR *fmts=new VkSurfaceFormatKHR[fmtCount]; vkGetPhysicalDeviceSurfaceFormatsKHR(GN_Vk.physicalDevice,GN_Vk.surface,&fmtCount,fmts);
    GN_Vk.swapchainFormat=fmts[0].format; delete[] fmts;
    if(GN_Vk.swapchainFormat==VK_FORMAT_UNDEFINED) GN_Vk.swapchainFormat=VK_FORMAT_B8G8R8A8_SRGB;
    VkSwapchainCreateInfoKHR sci={VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR}; sci.surface=GN_Vk.surface; sci.minImageCount=2;
    sci.imageFormat=GN_Vk.swapchainFormat; sci.imageColorSpace=VK_COLOR_SPACE_SRGB_NONLINEAR_KHR; sci.imageExtent=GN_Vk.swapchainExtent;
    sci.imageArrayLayers=1; sci.imageUsage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; sci.imageSharingMode=VK_SHARING_MODE_EXCLUSIVE;
    sci.preTransform=caps.currentTransform; sci.compositeAlpha=VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; sci.presentMode=VK_PRESENT_MODE_FIFO_KHR; sci.clipped=VK_TRUE;
    VkCheck(vkCreateSwapchainKHR(GN_Vk.device,&sci,0,&GN_Vk.swapchain), "vkCreateSwapchainKHR(resize)");
    vkGetSwapchainImagesKHR(GN_Vk.device,GN_Vk.swapchain,&GN_Vk.swapchainImageCount,0);
    GN_Vk.swapchainImages=new VkImage[GN_Vk.swapchainImageCount]; GN_Vk.swapchainViews=new VkImageView[GN_Vk.swapchainImageCount];
    vkGetSwapchainImagesKHR(GN_Vk.device,GN_Vk.swapchain,&GN_Vk.swapchainImageCount,GN_Vk.swapchainImages);
    for(uint32_t i=0;i<GN_Vk.swapchainImageCount;i++) GN_Vk.swapchainViews[i]=MakeImageView(GN_Vk.swapchainImages[i],GN_Vk.swapchainFormat);
    // 重建 framebuffers
    GN_Vk.framebuffers=new VkFramebuffer[GN_Vk.swapchainImageCount];
    for(uint32_t i=0;i<GN_Vk.swapchainImageCount;i++){VkFramebufferCreateInfo fci={VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO}; fci.renderPass=GN_Vk.renderPass; fci.attachmentCount=1; fci.pAttachments=&GN_Vk.swapchainViews[i]; fci.width=GN_Vk.swapchainExtent.width; fci.height=GN_Vk.swapchainExtent.height; fci.layers=1; vkCreateFramebuffer(GN_Vk.device,&fci,0,&GN_Vk.framebuffers[i]);}
}

// Ensure CPU vertex buffer can hold 'need' more vertices
// ⚠️ 已知风险: vkDestroyBuffer/vkFreeMemory 在未等待帧完成时调用,
//    可能导致 device lost。此处在每帧 BeginFrame 前无显式同步。
//    CreateBuf 内部 vkCreateBuffer/vkAllocateMemory 可能分配失败
//    但不做错误处理(函数为void, 调用者不检查) — 依赖 vulkan validation layers 捕获。
static void GrowVBufIfNeeded(uint32_t need) {
    if(gVCnt + need <= gVMax) return;
    while(gVCnt + need > gVMax) gVMax *= 2;
    VkVertex *nb = new VkVertex[gVMax];
    memcpy(nb, gVBuf, gVCnt * sizeof(VkVertex));
    delete[] gVBuf; gVBuf = nb;
    vkDestroyBuffer(GN_Vk.device, GN_Vk.vertexBuffer, 0);
    vkFreeMemory(GN_Vk.device, GN_Vk.vertexMemory, 0);
    CreateBuf(gVMax*sizeof(VkVertex), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
              GN_Vk.vertexBuffer, GN_Vk.vertexMemory);
}

// 2D 绘制
void ImageDraw(sIMAGE *img, int x, int y, int w, int h, unsigned int color, unsigned short texID, unsigned short blendmode, float u1, float v1, float u2, float v2) {
    if(!GN_Vk.initialized) return;
    // Allow null image for solid color drawing (used by text/line drawing)
    if(img && !img->vkImageView) return;
    if(!gVBuf){gVMax=65536;gVBuf=new VkVertex[gVMax];}
    FlushBatchIfScissorDirty();
    VkDescriptorSet ds;
    if(img && img->vkDescriptorSet) ds = img->vkDescriptorSet;
    else if(GN_Vk.defaultDS) ds = GN_Vk.defaultDS;
    else return;
    BatchSwitch(ds);
    vkCmdBindDescriptorSets(GN_Vk.commandBuffers[GN_Vk.currentFrame],VK_PIPELINE_BIND_POINT_GRAPHICS,GN_Vk.pipelineLayout,0,1,&ds,0,0);
    // Select blend pipeline per draw call
    static unsigned short s_lastBlendMode = 1;
    if(blendmode != s_lastBlendMode) {
        VkPipeline pipe = GN_Vk.pipeline; // default: eBLEND_NORMAL (1)
        switch(blendmode) {
            case 0:  pipe = GN_Vk.pipelineNull;     break; // NULL: no blend
            case 2:  pipe = GN_Vk.pipelineBright;    break; // BRIGHT: additive
            case 11: pipe = GN_Vk.pipelineMultiply;  break; // MODULATE: multiply
            // Other modes fall back to normal blend (1)
            default: pipe = GN_Vk.pipeline;          break;
        }
        vkCmdBindPipeline(GN_Vk.commandBuffers[GN_Vk.currentFrame],VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
        s_lastBlendMode = blendmode;
    }
    float u0=(u1==0&&v1==0&&u2==0&&v2==0)?0.0f:u1, v0=(u1==0&&v1==0&&u2==0&&v2==0)?0.0f:v1;
    float ue=(u1==0&&v1==0&&u2==0&&v2==0)?(img?img->uRange:1.0f):u2, ve=(u1==0&&v1==0&&u2==0&&v2==0)?(img?img->vRange:1.0f):v2;
    // ── texID 子纹理映射 (多帧图集: 9-slice, GB2312字体等) ──
    if(img && img->texnum > 1 && texID < img->texnum) {
        // 最优行列网格布局
        int cols, rows;
        if(img->texnum == 9)      { cols = 3; rows = 3; }
        else if(img->texnum <= 2) { cols = img->texnum; rows = 1; }
        else {
            cols = (int)ceilf(sqrtf((float)img->texnum));
            rows = (int)ceilf((float)img->texnum / (float)cols);
        }
        int tid = texID > 0 ? (int)texID : 0;
        int col = tid % cols, row = tid / cols;
        float fw = 1.0f / (float)cols;  // 单帧在图集中的归一化宽
        float fh = 1.0f / (float)rows;  // 单帧在图集中的归一化高
        if(u1==0 && v1==0 && u2==0 && v2==0) {
            // 无显式UV: 使用整帧
            u0 = img->uRange * col * fw;       v0 = img->vRange * row * fh;
            ue = img->uRange * (col+1) * fw;   ve = img->vRange * (row+1) * fh;
        } else {
            // 显式UV偏移到图集中对应帧的位置
            u0 = img->uRange * (col * fw + u1 * fw);
            v0 = img->vRange * (row * fh + v1 * fh);
            ue = img->uRange * (col * fw + u2 * fw);
            ve = img->vRange * (row * fh + v2 * fh);
        }
    }
    float dw=w?w:(img?(float)img->width:1.0f),dh=h?h:(img?(float)img->height:1.0f);
    float r=((color>>16)&0xff)/255.0f,g=((color>>8)&0xff)/255.0f,b=(color&0xff)/255.0f,a=((color>>24)&0xff)/255.0f;
    float x2=x+dw,y2=y+dh;
    VkVertex vv[6]={{x,y,u0,v0,r,g,b,a},{x2,y,ue,v0,r,g,b,a},{x2,y2,ue,ve,r,g,b,a},{x,y,u0,v0,r,g,b,a},{x2,y2,ue,ve,r,g,b,a},{x,y2,u0,ve,r,g,b,a}};
    GrowVBufIfNeeded(6);
    memcpy(gVBuf+gVCnt,vv,sizeof(vv));gVCnt+=6;
}

void LineFrame2dDraw(int x,int y,int w,int h,int lw,unsigned int color,unsigned short blend){
    if(!GN_Vk.initialized)return;
    FlushBatchIfScissorDirty();
    float r=((color>>16)&0xff)/255.0f,g=((color>>8)&0xff)/255.0f,b=(color&0xff)/255.0f,a=((color>>24)&0xff)/255.0f;
    // Simplified: draw 4 thin rects 
    ImageDraw(0,x,y,w,lw,color,0,0,0,0,0,0); ImageDraw(0,x,y+h-lw,w,lw,color,0,0,0,0,0,0);
    ImageDraw(0,x,y,lw,h,color,0,0,0,0,0,0); ImageDraw(0,x+w-lw,y,lw,h,color,0,0,0,0,0,0);
}

// 4-vertex quad with per-vertex colors — supports gradient effects
void ImageDraw4P(sIMAGE *img, int x, int y, int w, int h,
                 unsigned int c0, unsigned int c1,
                 unsigned int c2, unsigned int c3,
                 unsigned short blend) {
    if(!GN_Vk.initialized) return;
    if(img && !img->vkImageView) return;
    if(!gVBuf){gVMax=65536;gVBuf=new VkVertex[gVMax];}
    FlushBatchIfScissorDirty();
    VkDescriptorSet ds;
    if(img && img->vkDescriptorSet) ds = img->vkDescriptorSet;
    else if(GN_Vk.defaultDS) ds = GN_Vk.defaultDS;
    else return;

    BatchSwitch(ds);
    vkCmdBindDescriptorSets(GN_Vk.commandBuffers[GN_Vk.currentFrame],VK_PIPELINE_BIND_POINT_GRAPHICS,GN_Vk.pipelineLayout,0,1,&ds,0,0);

    auto fc = [](unsigned int c) -> float { return ((c>>24)&0xff)/255.0f; };
    auto fr = [](unsigned int c) -> float { return ((c>>16)&0xff)/255.0f; };
    auto fg = [](unsigned int c) -> float { return ((c>>8)&0xff)/255.0f; };
    auto fb = [](unsigned int c) -> float { return (c&0xff)/255.0f; };
    float u0=0.0f, v0=0.0f, ue=img?img->uRange:1.0f, ve=img?img->vRange:1.0f;
    float x2=x+w, y2=y+h;
    VkVertex vv[6]={
        {(float)x,(float)y,  u0,v0, fr(c0),fg(c0),fb(c0),fc(c0)},
        {(float)x2,(float)y, ue,v0, fr(c1),fg(c1),fb(c1),fc(c1)},
        {(float)x2,(float)y2,ue,ve, fr(c2),fg(c2),fb(c2),fc(c2)},
        {(float)x,(float)y,  u0,v0, fr(c0),fg(c0),fb(c0),fc(c0)},
        {(float)x2,(float)y2,ue,ve, fr(c2),fg(c2),fb(c2),fc(c2)},
        {(float)x,(float)y2, u0,ve, fr(c3),fg(c3),fb(c3),fc(c3)}
    };
    GrowVBufIfNeeded(6);
    memcpy(gVBuf+gVCnt,vv,sizeof(vv));gVCnt+=6;
}

// Float-vertex overload: uses caller-provided vertex positions
void ImageDraw4P(sIMAGE *img, float *vt, unsigned int color, unsigned short texID,
                 unsigned short blend, float u1, float v1, float u2, float v2) {
    if(!GN_Vk.initialized) return;
    if(img && !img->vkImageView) return;
    if(!gVBuf){gVMax=65536;gVBuf=new VkVertex[gVMax];}
    FlushBatchIfScissorDirty();
    VkDescriptorSet ds;
    if(img && img->vkDescriptorSet) ds = img->vkDescriptorSet;
    else if(GN_Vk.defaultDS) ds = GN_Vk.defaultDS;
    else return;
    (void)texID;

    BatchSwitch(ds);
    vkCmdBindDescriptorSets(GN_Vk.commandBuffers[GN_Vk.currentFrame],VK_PIPELINE_BIND_POINT_GRAPHICS,GN_Vk.pipelineLayout,0,1,&ds,0,0);

    float r=((color>>16)&0xff)/255.0f,g=((color>>8)&0xff)/255.0f,b=(color&0xff)/255.0f,a=((color>>24)&0xff)/255.0f;
    VkVertex vv[6]={
        {vt[0],vt[1],u1,v1,r,g,b,a}, {vt[2],vt[3],u2,v1,r,g,b,a}, {vt[4],vt[5],u2,v2,r,g,b,a},
        {vt[0],vt[1],u1,v1,r,g,b,a}, {vt[4],vt[5],u2,v2,r,g,b,a}, {vt[6],vt[7],u1,v2,r,g,b,a}
    };
    GrowVBufIfNeeded(6);
    memcpy(gVBuf+gVCnt,vv,sizeof(vv));gVCnt+=6;
}

// Rotation drawing: apply 2D rotation around center point (cx,cy within 0..1)
void ImageDrawRot(sIMAGE *img, int x, int y, int w, int h,
                  float angle, float cx, float cy,
                  unsigned int color, int frame_id) {
    (void)frame_id;
    if(!GN_Vk.initialized) return;
    if(img && !img->vkImageView) return;
    FlushBatchIfScissorDirty();
    float rad = angle * 3.14159265f / 180.0f;
    float co = cosf(rad), si = sinf(rad);
    float px[4]={0.0f,(float)w,(float)w,0.0f};
    float py[4]={0.0f,0.0f,(float)h,(float)h};
    for(int i=0;i<4;i++){
        float dx=px[i]-cx*w, dy=py[i]-cy*h;
        px[i]=x + cx*w + dx*co - dy*si;
        py[i]=y + cy*h + dx*si + dy*co;
    }
    float u0=0.0f,v0=0.0f,ue=img?img->uRange:1.0f,ve=img?img->vRange:1.0f;
    float r=((color>>16)&0xff)/255.0f,g=((color>>8)&0xff)/255.0f,b=(color&0xff)/255.0f,a=((color>>24)&0xff)/255.0f;
    VkVertex vv[6]={
        {px[0],py[0],u0,v0,r,g,b,a},{px[1],py[1],ue,v0,r,g,b,a},{px[2],py[2],ue,ve,r,g,b,a},
        {px[0],py[0],u0,v0,r,g,b,a},{px[2],py[2],ue,ve,r,g,b,a},{px[3],py[3],u0,ve,r,g,b,a}
    };
    if(!gVBuf){gVMax=65536;gVBuf=new VkVertex[gVMax];}
    if(img && img->vkDescriptorSet){
        // using shared s_lastDS
        if(img->vkDescriptorSet!=s_lastDS&&gVCnt>0){void *d;vkMapMemory(GN_Vk.device,GN_Vk.vertexMemory,0,gVCnt*sizeof(VkVertex),0,&d);memcpy(d,gVBuf,gVCnt*sizeof(VkVertex));vkUnmapMemory(GN_Vk.device,GN_Vk.vertexMemory);VkDeviceSize o[]={0};vkCmdBindVertexBuffers(GN_Vk.commandBuffers[GN_Vk.currentFrame],0,1,&GN_Vk.vertexBuffer,o);vkCmdDraw(GN_Vk.commandBuffers[GN_Vk.currentFrame],gVCnt,1,0,0);gVCnt=0;}
        s_lastDS=img->vkDescriptorSet;
        vkCmdBindDescriptorSets(GN_Vk.commandBuffers[GN_Vk.currentFrame],VK_PIPELINE_BIND_POINT_GRAPHICS,GN_Vk.pipelineLayout,0,1,&img->vkDescriptorSet,0,0);
    }
    GrowVBufIfNeeded(6);
    memcpy(gVBuf+gVCnt,vv,sizeof(vv));gVCnt+=6;
}

void sIMAGE::Release(bool immed) {
    if(vkImageView){vkDestroyImageView(GN_Vk.device,vkImageView,0);vkImageView=VK_NULL_HANDLE;}
    if(vkImage){vkDestroyImage(GN_Vk.device,vkImage,0);vkImage=VK_NULL_HANDLE;}
    if(vkImageMemory){vkFreeMemory(GN_Vk.device,vkImageMemory,0);vkImageMemory=VK_NULL_HANDLE;}
    if(vkSampler){vkDestroySampler(GN_Vk.device,vkSampler,0);vkSampler=VK_NULL_HANDLE;}
    texnum=0;
}

// ── PNG 加载辅助函数 ────────────────────────────────────
static void PngReadCB(png_structp png, png_bytep data, png_size_t len){fread(data,1,len,(FILE*)png_get_io_ptr(png));}
static uint8_t* LoadPngFile(const char *path, int *w_out, int *h_out) {
    FILE *fp = fopen(path, "rb");
    if(!fp) return nullptr;
    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING,0,0,0);
    if(!png){fclose(fp);return nullptr;}
    png_infop info = png_create_info_struct(png);
    if(!info){png_destroy_read_struct(&png,0,0);fclose(fp);return nullptr;}
    png_set_read_fn(png, fp, PngReadCB);
    png_read_info(png, info);
    int w = png_get_image_width(png,info), h = png_get_image_height(png,info);
    // Expand to RGBA
    if(png_get_color_type(png,info) == PNG_COLOR_TYPE_RGB)
        png_set_filler(png, 0xFF, PNG_FILLER_AFTER);
    if(png_get_color_type(png,info) == PNG_COLOR_TYPE_PALETTE)
        png_set_palette_to_rgb(png);
    if(png_get_color_type(png,info) == PNG_COLOR_TYPE_GRAY && png_get_bit_depth(png,info) < 8)
        png_set_expand_gray_1_2_4_to_8(png);
    if(png_get_valid(png,info,PNG_INFO_tRNS))
        png_set_tRNS_to_alpha(png);
    if(png_get_color_type(png,info) == PNG_COLOR_TYPE_GRAY || 
       png_get_color_type(png,info) == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_gray_to_rgb(png);
    png_read_update_info(png, info);
    w = png_get_image_width(png, info); h = png_get_image_height(png, info);
    png_bytep *rows = new png_bytep[h];
    uint8_t *px = new uint8_t[w * h * 4];
    for(int i = 0; i < h; i++) rows[i] = px + i * w * 4;
    png_read_image(png, rows);
    png_destroy_read_struct(&png, &info, 0);
    fclose(fp);
    delete[] rows;
    *w_out = w; *h_out = h;
    return px;
}

// ── 通用 GPU 纹理上传 ─────────────────────────────────
static void UploadImageToGPU(sIMAGE *out, int w, int h, uint8_t *px) {
    // Delegate to DataFill — 自动处理pass内延迟上传
    out->DataFill(px, CR_RGBA, w, h, 0, 0, 0, false);
}

// ═══════════════════════════════════════════════════════════
//  ImageLoad — 支持单帧 & 多帧图集打包
// ═══════════════════════════════════════════════════════════
bool ImageLoad(const char *filename, unsigned short num, sIMAGE *out, int fmt) {
    if(!out || num == 0 || !GN_Vk.initialized) return false;
    (void)fmt;
    out->Release();

    // ── 加载第一帧获取尺寸 ──────────────────────────
    char path[512];
    snprintf(path, sizeof(path), "%s0000.png", filename);
    int fw = 0, fh = 0;
    uint8_t *firstFrame = LoadPngFile(path, &fw, &fh);
    if(!firstFrame) return false;

    if(num == 1) {
        // ── 单帧快速路径 ─────────────────────────
        out->width = fw; out->height = fh;
        out->uRange = 1.0f; out->vRange = 1.0f; out->texnum = 1;
        UploadImageToGPU(out, fw, fh, firstFrame);
        delete[] firstFrame;
        return true;
    }

	// ── 多帧图集打包 ──────────────────────────────
	// 最优行列网格
	int cols, rows;
	if(num == 9)      { cols = 3; rows = 3; }
	else if(num <= 2) { cols = num; rows = 1; }
	else {
		cols = (int)ceilf(sqrtf((float)num));
		rows = (int)ceilf((float)num / (float)cols);
	}
	int atlasW = fw * cols, atlasH = fh * rows;

    // 分配图集临时缓冲区
    uint8_t *atlas = new uint8_t[atlasW * atlasH * 4];
    memset(atlas, 0, (size_t)atlasW * atlasH * 4);

    // 拷贝第一帧到 (0,0)
    for(int r = 0; r < fh; r++)
        memcpy(atlas + (r * atlasW) * 4, firstFrame + (r * fw) * 4, (size_t)fw * 4);
    delete[] firstFrame;

    // 加载剩余帧
    for(unsigned short fi = 1; fi < num; fi++) {
        snprintf(path, sizeof(path), "%s%04d.png", filename, fi);
        int ffw = 0, ffh = 0;
        uint8_t *frame = LoadPngFile(path, &ffw, &ffh);
        if(!frame) continue;
        int col = fi % cols, row = fi / cols;
        int ox = col * fw, oy = row * fh;
        int copyW = ffw < fw ? ffw : fw;
        int copyH = ffh < fh ? ffh : fh;
        for(int r = 0; r < copyH; r++)
            memcpy(atlas + ((oy + r) * atlasW + ox) * 4,
                   frame + (r * ffw) * 4, (size_t)copyW * 4);
        delete[] frame;
    }

    // 上传图集到 GPU
    out->width = atlasW; out->height = atlasH;
    out->uRange = 1.0f; out->vRange = 1.0f;
    out->texnum = num;
    UploadImageToGPU(out, atlasW, atlasH, atlas);
    delete[] atlas;
    return true;
}


void RenderFinish(bool only3d) { 
    (void)only3d;
    // Vulkan: end the current frame and present
    GNg::GN_VkEndFrame();
}
// ── 缺失的 API 实现 ────────────────────────────────────────
void sIMAGE::InitToData(char *filename, unsigned short imgnum, int fmt, bool mip, unsigned char lv) {
    Release();
    ImageLoad(filename, imgnum, this, fmt);
    if (filename) { this->fileName = filename; this->add_0000 = true; }
}

void ImageDrawDitherBlur(sIMAGE *img, int x, int y, int w, int h,
                         unsigned int color, unsigned short tid,
                         unsigned short blend, int blur, float dither) {
    ImageDraw(img, x, y, w, h, color, tid, blend);
}

void ImageDrawMP(sIMAGE *img, sDRAW_RANGE *ranges, int count,
                 unsigned int color, unsigned short blend) {
    for (int i = 0; i < count; i++)
        ImageDraw(img, (int)ranges[i].x, (int)ranges[i].y,
                  (int)ranges[i].w, (int)ranges[i].h, color, ranges[i].tid, blend,
                  ranges[i].u1, ranges[i].v1, ranges[i].u2, ranges[i].v2);
}

P<sIMAGE> ImageLoad(const char *fn, unsigned short num, int fmt) {
    P<sIMAGE> p(1);
    ImageLoad(fn, num, &(*p), fmt);
    return p;
}


// ── 更多缺失 API ──────────────────────────────────────────
sIMAGE& sIMAGE::operator=(const sIMAGE &src) {
    if(this != &src) {
        Release();
        width=src.width; height=src.height; uRange=src.uRange; vRange=src.vRange;
        texnum=src.texnum; cFormat=src.cFormat; add_0000=src.add_0000;
    }
    return *this;
}

void sIMAGE::CreateEmptyToData(int w, int h, unsigned short imgnum, int fmt,
                               unsigned char lv, bool mip, bool immed) {
    // Create actual Vulkan texture resources (unlike the old stub)
    if(!GN_Vk.initialized || w <= 0 || h <= 0) return;
    Release();

    VkFormat vkFmt = VK_FORMAT_R8G8B8A8_UNORM;
    VkDeviceSize sz = (VkDeviceSize)w * (VkDeviceSize)h * 4;

    // Create image
    {VkImageCreateInfo ici={VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    ici.imageType=VK_IMAGE_TYPE_2D; ici.extent={(uint32_t)w,(uint32_t)h,1};
    ici.mipLevels=1; ici.arrayLayers=1; ici.format=vkFmt;
    ici.tiling=VK_IMAGE_TILING_OPTIMAL; ici.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;
    ici.usage=VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT;
    ici.samples=VK_SAMPLE_COUNT_1_BIT; ici.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
    {VkResult res = vkCreateImage(GN_Vk.device,&ici,0,&vkImage);
     VkCheck(res, "vkCreateImage in CreateEmptyToData");
     if(res != VK_SUCCESS) { vkImage=VK_NULL_HANDLE; return; }}
    VkMemoryRequirements mr; vkGetImageMemoryRequirements(GN_Vk.device,vkImage,&mr);
    VkMemoryAllocateInfo mai={VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    mai.allocationSize=mr.size; mai.memoryTypeIndex=FindMemType(mr.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    {VkResult res = vkAllocateMemory(GN_Vk.device,&mai,0,&vkImageMemory);
    if(res != VK_SUCCESS) {
        VkCheck(res, "vkAllocateMemory in CreateEmptyToData");
        vkDestroyImage(GN_Vk.device,vkImage,0); vkImage=VK_NULL_HANDLE;
        vkImageMemory=VK_NULL_HANDLE; return;
    }}
    {VkResult res = vkBindImageMemory(GN_Vk.device,vkImage,vkImageMemory,0);
     VkCheck(res, "vkBindImageMemory in CreateEmptyToData");}}

    // Create image view
    vkImageView = MakeImageView(vkImage, vkFmt);

    // Create sampler
    {VkSamplerCreateInfo sci={VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    sci.magFilter=VK_FILTER_LINEAR; sci.minFilter=VK_FILTER_LINEAR;
    sci.addressModeU=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sci.addressModeV=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    VkResult res = vkCreateSampler(GN_Vk.device,&sci,0,&vkSampler);
    VkCheck(res, "vkCreateSampler in CreateEmptyToData");}

    // Allocate descriptor set
    {VkDescriptorSetAllocateInfo dsai={VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    dsai.descriptorPool=GN_Vk.descriptorPool; dsai.descriptorSetCount=1;
    dsai.pSetLayouts=&GN_Vk.descriptorLayout;
    VkResult res = vkAllocateDescriptorSets(GN_Vk.device,&dsai,&vkDescriptorSet);
    VkCheck(res, "vkAllocateDescriptorSets in CreateEmptyToData");
    VkDescriptorBufferInfo dbi={GN_Vk.uniformBuffer,0,sizeof(VkUBO)};
    VkDescriptorImageInfo dii={vkSampler,vkImageView,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    VkWriteDescriptorSet wds[2]={{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET},{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET}};
    wds[0].dstSet=vkDescriptorSet; wds[0].dstBinding=0;
    wds[0].descriptorType=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; wds[0].descriptorCount=1; wds[0].pBufferInfo=&dbi;
    wds[1].dstSet=vkDescriptorSet; wds[1].dstBinding=1;
    wds[1].descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; wds[1].descriptorCount=1; wds[1].pImageInfo=&dii;
    vkUpdateDescriptorSets(GN_Vk.device,2,wds,0,0);}

    // Set metadata
    width=w; height=h; uRange=1.0f; vRange=1.0f; texnum=1;
    (void)imgnum; (void)lv; (void)mip; (void)immed; (void)fmt;
    (void)sz;

    // Transition image from UNDEFINED to SHADER_READ_ONLY (required before DataFill/rendering)
    {VkCommandBuffer cb;
    {VkCommandBufferAllocateInfo ai={VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
     ai.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY; ai.commandPool=GN_Vk.commandPool; ai.commandBufferCount=1;
     VkResult res = vkAllocateCommandBuffers(GN_Vk.device,&ai,&cb);
     VkCheck(res, "vkAllocateCommandBuffers in CreateEmptyToData");}
    {VkCommandBufferBeginInfo bi={VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
     bi.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT; vkBeginCommandBuffer(cb,&bi);}
    {VkImageMemoryBarrier b={VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
     b.oldLayout=VK_IMAGE_LAYOUT_UNDEFINED;
     b.newLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
     b.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED; b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
     b.image=vkImage; b.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;
     b.subresourceRange.levelCount=1; b.subresourceRange.layerCount=1;
     b.srcAccessMask=0; b.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;
     vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                          VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0,0,0,0, 1, &b);}
    vkEndCommandBuffer(cb);
    {VkSubmitInfo si={VK_STRUCTURE_TYPE_SUBMIT_INFO}; si.commandBufferCount=1;
     si.pCommandBuffers=&cb; vkQueueSubmit(GN_Vk.graphicsQueue,1,&si,VK_NULL_HANDLE);
     vkQueueWaitIdle(GN_Vk.graphicsQueue);}
    vkFreeCommandBuffers(GN_Vk.device,GN_Vk.commandPool,1,&cb);}
}

// Vulkan-backed DataFill: uploads in-memory pixel data to GPU texture.
// If VkImage already exists (from CreateEmptyToData), does sub-region update
// using sw/sh as offset_x/offset_y. Otherwise creates a new texture.
void sIMAGE::DataFill(uint8 *data, int fmt, int w, int h, int sw, int sh, unsigned short *texid, bool mip) {
    if(!GN_Vk.initialized || !data) return;
    if(w <= 0) w = width;
    if(h <= 0) h = height;
    if(w <= 0 || h <= 0) return;
    int offset_x = sw, offset_y = sh;  // repurpose sw/sh as sub-region offset
    (void)texid; (void)mip;

    // If we already have a VkImage, do sub-region update
    if(vkImage != VK_NULL_HANDLE) {
        int srcBpp; bool expandRGBA = false;
        switch(fmt) {
            case CR_ALPHA: case CR_LUMINANCE: expandRGBA=true; srcBpp=1; break;
            case CR_LUMINANCE_ALPHA: expandRGBA=true; srcBpp=2; break;
            case CR_RGB:  srcBpp=3; break;
            case CR_RGBA: srcBpp=4; break;
            default:      srcBpp=4; break;
        }
        VkDeviceSize srcSz = (VkDeviceSize)w * (VkDeviceSize)h * srcBpp;
        VkDeviceSize dstSz = (VkDeviceSize)w * (VkDeviceSize)h * 4;
        VkBuffer stagingBuf; VkDeviceMemory stagingMem;
        CreateBuf(dstSz, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                  stagingBuf, stagingMem);
        void *d; vkMapMemory(GN_Vk.device, stagingMem, 0, dstSz, 0, &d);
        if(expandRGBA || srcBpp == 3) {
            uint8_t *dst8 = (uint8_t*)d;
            for(int i = 0; i < w * h; i++) {
                if(srcBpp == 1) {
                    uint8_t a = ((uint8_t*)data)[i];
                    dst8[i*4]=a; dst8[i*4+1]=a; dst8[i*4+2]=a; dst8[i*4+3]=a;
                } else if(srcBpp == 2) {
                    uint8_t l=((uint8_t*)data)[i*2], a2=((uint8_t*)data)[i*2+1];
                    dst8[i*4]=l; dst8[i*4+1]=l; dst8[i*4+2]=l; dst8[i*4+3]=a2;
                } else {
                    dst8[i*4  ]=((uint8_t*)data)[i*3];
                    dst8[i*4+1]=((uint8_t*)data)[i*3+1];
                    dst8[i*4+2]=((uint8_t*)data)[i*3+2];
                    dst8[i*4+3]=0xFF;
                }
            }
        } else { memcpy(d, data, dstSz); }

        // pass内: 延迟上传到帧首 (must copy before unmap!)
        // skipDefer: FreeType字体atlas等需即时更新的纹理不走延迟
        if(s_inRenderPass && s_pendingUploadCnt < 256 && !skipDefer){
            s_pendingUploads[s_pendingUploadCnt].img = this;
        s_pendingUploads[s_pendingUploadCnt].data = new uint8[dstSz];
        memcpy(s_pendingUploads[s_pendingUploadCnt].data, d, dstSz);
        s_pendingUploads[s_pendingUploadCnt].w = w;
        s_pendingUploads[s_pendingUploadCnt].h = h;
        s_pendingUploads[s_pendingUploadCnt].fmt = CR_RGBA; // staging已展开为RGBA
        s_pendingUploadCnt++;
        vkUnmapMemory(GN_Vk.device, stagingMem);
        vkDestroyBuffer(GN_Vk.device, stagingBuf, 0);
        vkFreeMemory(GN_Vk.device, stagingMem, 0);
        return;
    }
    vkUnmapMemory(GN_Vk.device, stagingMem);

    {VkCommandBufferAllocateInfo ai={VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        ai.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY; ai.commandPool=GN_Vk.commandPool; ai.commandBufferCount=1;
        VkCommandBuffer cb; VkResult res = vkAllocateCommandBuffers(GN_Vk.device,&ai,&cb);
        VkCheck(res, "vkAllocateCommandBuffers in DataFill(sub-region)");
        VkCommandBufferBeginInfo bi={VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        bi.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT; vkBeginCommandBuffer(cb,&bi);
        VkImageMemoryBarrier b1={VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        b1.oldLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; b1.newLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        b1.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED; b1.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
        b1.image=vkImage; b1.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;
        b1.subresourceRange.levelCount=1; b1.subresourceRange.layerCount=1;
        b1.srcAccessMask=VK_ACCESS_SHADER_READ_BIT; b1.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;
        vkCmdPipelineBarrier(cb,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,0,0,0,1,&b1);
        VkBufferImageCopy rgn={}; rgn.bufferOffset=0; rgn.imageSubresource.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;
        rgn.imageSubresource.layerCount=1; rgn.imageOffset={offset_x, offset_y, 0};
        rgn.imageExtent={(uint32_t)w,(uint32_t)h,1};
        vkCmdCopyBufferToImage(cb,stagingBuf,vkImage,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&rgn);
        VkImageMemoryBarrier b2={VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        b2.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL; b2.newLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        b2.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED; b2.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
        b2.image=vkImage; b2.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;
        b2.subresourceRange.levelCount=1; b2.subresourceRange.layerCount=1;
        b2.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT; b2.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(cb,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,0,0,0,0,0,1,&b2);
        vkEndCommandBuffer(cb);
        VkFence f; VkFenceCreateInfo fci={VK_STRUCTURE_TYPE_FENCE_CREATE_INFO}; vkCreateFence(GN_Vk.device,&fci,0,&f);
        VkSubmitInfo si={VK_STRUCTURE_TYPE_SUBMIT_INFO}; si.commandBufferCount=1; si.pCommandBuffers=&cb;
        vkQueueSubmit(GN_Vk.graphicsQueue,1,&si,f); vkWaitForFences(GN_Vk.device,1,&f,VK_TRUE,UINT64_MAX);
        vkDestroyFence(GN_Vk.device,f,0);
        vkFreeCommandBuffers(GN_Vk.device,GN_Vk.commandPool,1,&cb);}
        vkDestroyBuffer(GN_Vk.device,stagingBuf,0); vkFreeMemory(GN_Vk.device,stagingMem,0);
        return;
    }

    // Map GN_mini COLOR_FORMAT to Vulkan format.
    // CR_ALPHA/CR_LUMINANCE are expanded to RGBA in staging so the shader
    // sees (A,A,A,A) per pixel — compatible with OpenGL GL_ALPHA behaviour
    // where sampled colour is (0,0,0,alpha) and the shader multiplies
    // component-wise: outColor = texture * fragColor.
    bool expandToRGBA = false; int srcBpp = 4;
    switch(fmt) {
        case CR_ALPHA:     expandToRGBA = true; srcBpp = 1; break;
        case CR_LUMINANCE: expandToRGBA = true; srcBpp = 1; break;
        case CR_LUMINANCE_ALPHA: expandToRGBA = true; srcBpp = 2; break;
        case CR_RGBA:      srcBpp = 4; break;
        case CR_RGB:       srcBpp = 4; break; // stored as RGBA
        default:           srcBpp = 4; break;
    }
    VkFormat vkFmt = VK_FORMAT_R8G8B8A8_UNORM;
    VkDeviceSize srcSz = (VkDeviceSize)w * (VkDeviceSize)h * srcBpp;
    VkDeviceSize dstSz = (VkDeviceSize)w * (VkDeviceSize)h * 4;

    // Release any previous Vulkan resources
    Release();

    // Create staging buffer
    VkBuffer stagingBuf; VkDeviceMemory stagingMem;
    CreateBuf(dstSz, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
              stagingBuf, stagingMem);
    void *d; vkMapMemory(GN_Vk.device, stagingMem, 0, dstSz, 0, &d);
    if(expandToRGBA) {
        // Expand narrow format to RGBA in staging
        uint8_t *dst = (uint8_t*)d;
        if(fmt == CR_ALPHA) {
            for(int i = 0; i < w * h; i++) {
                uint8_t a = ((uint8_t*)data)[i];
                dst[i*4+0] = a; dst[i*4+1] = a; dst[i*4+2] = a; dst[i*4+3] = a;
            }
        } else if(fmt == CR_LUMINANCE) {
            for(int i = 0; i < w * h; i++) {
                uint8_t l = ((uint8_t*)data)[i];
                dst[i*4+0] = l; dst[i*4+1] = l; dst[i*4+2] = l; dst[i*4+3] = 0xFF;
            }
        } else { // CR_LUMINANCE_ALPHA
            for(int i = 0; i < w * h; i++) {
                uint8_t l = ((uint8_t*)data)[i*2+0];
                uint8_t a = ((uint8_t*)data)[i*2+1];
                dst[i*4+0] = l; dst[i*4+1] = l; dst[i*4+2] = l; dst[i*4+3] = a;
            }
        }
    } else {
        memcpy(d, data, dstSz);
    }
    // pass内延迟: 新纹理创建也需要避免pass内queue submit
    if(s_inRenderPass && s_pendingUploadCnt < 256 && !skipDefer){
        s_pendingUploads[s_pendingUploadCnt].img = this;
        s_pendingUploads[s_pendingUploadCnt].data = new uint8[dstSz];
        memcpy(s_pendingUploads[s_pendingUploadCnt].data, d, dstSz);
        s_pendingUploads[s_pendingUploadCnt].w = w;
        s_pendingUploads[s_pendingUploadCnt].h = h;
        s_pendingUploads[s_pendingUploadCnt].fmt = CR_RGBA;
        s_pendingUploadCnt++;
        vkUnmapMemory(GN_Vk.device, stagingMem);
        vkDestroyBuffer(GN_Vk.device, stagingBuf, 0);
        vkFreeMemory(GN_Vk.device, stagingMem, 0);
        width = w; height = h; uRange = 1.0f; vRange = 1.0f; texnum = 1;
        return;
    }
    vkUnmapMemory(GN_Vk.device, stagingMem);

    // Create destination image
    {VkImageCreateInfo ici={VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    ici.imageType=VK_IMAGE_TYPE_2D; ici.extent={(uint32_t)w,(uint32_t)h,1};
    ici.mipLevels=1; ici.arrayLayers=1; ici.format=vkFmt;
    ici.tiling=VK_IMAGE_TILING_OPTIMAL; ici.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;
    ici.usage=VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT;
    ici.samples=VK_SAMPLE_COUNT_1_BIT; ici.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
    {VkResult res = vkCreateImage(GN_Vk.device,&ici,0,&vkImage);
     VkCheck(res, "vkCreateImage in DataFill");}
    VkMemoryRequirements mr; vkGetImageMemoryRequirements(GN_Vk.device,vkImage,&mr);
    VkMemoryAllocateInfo mai={VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    mai.allocationSize=mr.size; mai.memoryTypeIndex=FindMemType(mr.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    {VkResult res = vkAllocateMemory(GN_Vk.device,&mai,0,&vkImageMemory);
     VkCheck(res, "vkAllocateMemory in DataFill");}
    {VkResult res = vkBindImageMemory(GN_Vk.device,vkImage,vkImageMemory,0);
     VkCheck(res, "vkBindImageMemory in DataFill");}
    vkIsLinear = false;}

    // One-time command: transition → copy → transition
    {VkCommandBufferAllocateInfo ai={VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    ai.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY; ai.commandPool=GN_Vk.commandPool; ai.commandBufferCount=1;
    VkCommandBuffer cb; VkResult res = vkAllocateCommandBuffers(GN_Vk.device,&ai,&cb);
    VkCheck(res, "vkAllocateCommandBuffers in DataFill");
    {VkCommandBufferBeginInfo bi={VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    bi.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT; vkBeginCommandBuffer(cb,&bi);}
    {VkImageMemoryBarrier b1={VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    b1.oldLayout=VK_IMAGE_LAYOUT_UNDEFINED; b1.newLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    b1.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED; b1.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
    b1.image=vkImage; b1.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;
    b1.subresourceRange.levelCount=1; b1.subresourceRange.layerCount=1;
    b1.srcAccessMask=0; b1.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(cb,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,0,0,0,1,&b1);}
    {VkBufferImageCopy rgn={}; rgn.imageSubresource.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;
    rgn.imageSubresource.layerCount=1; rgn.imageExtent={(uint32_t)w,(uint32_t)h,1};
    vkCmdCopyBufferToImage(cb,stagingBuf,vkImage,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&rgn);}
    {VkImageMemoryBarrier b2={VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    b2.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL; b2.newLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    b2.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED; b2.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
    b2.image=vkImage; b2.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;
    b2.subresourceRange.levelCount=1; b2.subresourceRange.layerCount=1;
    b2.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT; b2.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cb,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,0,0,0,0,0,1,&b2);}
    vkEndCommandBuffer(cb);
    {VkFenceCreateInfo fci={VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
     VkFence uploadFence; vkCreateFence(GN_Vk.device, &fci, 0, &uploadFence);
     VkSubmitInfo si={VK_STRUCTURE_TYPE_SUBMIT_INFO}; si.commandBufferCount=1; si.pCommandBuffers=&cb;
     vkQueueSubmit(GN_Vk.graphicsQueue,1,&si,uploadFence);
     vkWaitForFences(GN_Vk.device,1,&uploadFence,VK_TRUE,UINT64_MAX);
     vkDestroyFence(GN_Vk.device,uploadFence,0);}
    vkFreeCommandBuffers(GN_Vk.device,GN_Vk.commandPool,1,&cb);}

    // Cleanup staging
    vkDestroyBuffer(GN_Vk.device,stagingBuf,0); vkFreeMemory(GN_Vk.device,stagingMem,0);

    // Create image view
    vkImageView = MakeImageView(vkImage, vkFmt);

    // Create sampler
    {VkSamplerCreateInfo sci={VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    sci.magFilter=VK_FILTER_LINEAR; sci.minFilter=VK_FILTER_LINEAR;
    sci.addressModeU=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sci.addressModeV=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    VkResult res = vkCreateSampler(GN_Vk.device,&sci,0,&vkSampler);
    VkCheck(res, "vkCreateSampler in DataFill");}

    // Allocate descriptor set
    {VkDescriptorSetAllocateInfo dsai={VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    dsai.descriptorPool=GN_Vk.descriptorPool; dsai.descriptorSetCount=1;
    dsai.pSetLayouts=&GN_Vk.descriptorLayout;
    VkResult res = vkAllocateDescriptorSets(GN_Vk.device,&dsai,&vkDescriptorSet);
    VkCheck(res, "vkAllocateDescriptorSets in DataFill");
    VkDescriptorBufferInfo dbi={GN_Vk.uniformBuffer,0,sizeof(VkUBO)};
    VkDescriptorImageInfo dii={vkSampler,vkImageView,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    VkWriteDescriptorSet wds[2]={{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET},{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET}};
    wds[0].dstSet=vkDescriptorSet; wds[0].dstBinding=0;
    wds[0].descriptorType=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; wds[0].descriptorCount=1; wds[0].pBufferInfo=&dbi;
    wds[1].dstSet=vkDescriptorSet; wds[1].dstBinding=1;
    wds[1].descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; wds[1].descriptorCount=1; wds[1].pImageInfo=&dii;
    vkUpdateDescriptorSets(GN_Vk.device,2,wds,0,0);}

    // Update sIMAGE metadata
    width = w; height = h; uRange = 1.0f; vRange = 1.0f; texnum = 1;
}

void Draw2DAlphaDirectionSet(unsigned char dir) { (void)dir; }
unsigned char Draw2DAlphaDirectionGet() { return 0; }

sIMAGE::sIMAGE(const sIMAGE &src) : sIMAGE() { *this = src; }

// ═══════════════════════════════════════════════════════════
//  补充 API 实现 (GN_Vk.h 声明但之前缺失)
// ═══════════════════════════════════════════════════════════

// Alpha color key state
static unsigned int s_alphaColorKey = 0;
void SetAlphaColorKey(unsigned int color) { s_alphaColorKey = color; }

// Extended ImageDraw with full UV control
void ImageDrawFull(sIMAGE *img, int x, int y, int w, int h,
                   unsigned int color, unsigned short texID,
                   unsigned short blend, float u1, float v1, float u2, float v2) {
    ImageDraw(img, x, y, w, h, color, texID, blend, u1, v1, u2, v2);
}

// Quick text draw — convenience wrapper
// Full implementation requires cTEXT_ART (GN_2D.h); stubbed for now as
// actual text rendering goes through cTEXT_ART::Show() / GN_RunTextOut
void TextDraw(void *textArt, const char *text, int x, int y, int w, int h) {
    (void)textArt; (void)text; (void)x; (void)y; (void)w; (void)h;
    // Text rendering in Vulkan is handled by GN_RunTextOut (core/GN_RunRec.cpp)
    // and cTEXT_ART::Show (graphics/GN_Font.cpp) which use ImageDrawMP internally.
}

// Extended image load with optional resize
bool ImageLoadEx(const char *fn, sIMAGE *out, int w, int h) {
    if(!ImageLoad(fn, 1, out, 0)) return false;
    (void)w; (void)h; // resize not yet implemented
    return true;
}

// Load PNG from memory buffer
bool ImageLoadFromMem(const void *data, int len, sIMAGE *out) {
    if(!data || len <= 0 || !out) return false;
    // Use libpng with custom read from memory
    struct MemReader { const uint8_t *ptr; int remaining; };
    MemReader mr = {(const uint8_t*)data, len};
    auto readCB = [](png_structp png, png_bytep buf, png_size_t sz) {
        MemReader *mr = (MemReader*)png_get_io_ptr(png);
        if((int)sz > mr->remaining) sz = mr->remaining;
        memcpy(buf, mr->ptr, sz);
        mr->ptr += sz; mr->remaining -= (int)sz;
    };
    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
    if(!png) return false;
    png_infop info = png_create_info_struct(png);
    if(!info) { png_destroy_read_struct(&png, 0, 0); return false; }
    png_set_read_fn(png, &mr, readCB);
    png_read_info(png, info);
    int iw = png_get_image_width(png, info), ih = png_get_image_height(png, info);
    // Expand to RGBA as in ImageLoad
    if(png_get_color_type(png,info) == PNG_COLOR_TYPE_RGB)
        png_set_filler(png, 0xFF, PNG_FILLER_AFTER);
    if(png_get_color_type(png,info) == PNG_COLOR_TYPE_PALETTE)
        png_set_palette_to_rgb(png);
    if(png_get_color_type(png,info) == PNG_COLOR_TYPE_GRAY && png_get_bit_depth(png,info) < 8)
        png_set_expand_gray_1_2_4_to_8(png);
    if(png_get_valid(png,info,PNG_INFO_tRNS))
        png_set_tRNS_to_alpha(png);
    if(png_get_color_type(png,info) == PNG_COLOR_TYPE_GRAY || 
       png_get_color_type(png,info) == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_gray_to_rgb(png);
    png_read_update_info(png, info);
    iw = png_get_image_width(png, info); ih = png_get_image_height(png, info);
    png_bytep *rows = new png_bytep[ih];
    uint8_t *px = new uint8_t[iw * ih * 4];
    for(int i = 0; i < ih; i++) rows[i] = px + i * iw * 4;
    png_read_image(png, rows);
    png_destroy_read_struct(&png, &info, 0);
    delete[] rows;
    // Upload to GPU via DataFill
    out->CreateEmptyToData(iw, ih, 1, 0, 1, false, false);
    out->DataFill(px, CR_RGBA, iw, ih, 0, 0, 0, false);
    delete[] px;
    return true;
}

// Save image to PNG file
void ImageSaveToFile(const char *fn, sIMAGE *img) {
    if(!fn || !img || !GN_Vk.initialized) return;
    uint8_t *px = img->DataGet();
    if(!px) return;
    // Write PNG via libpng
    FILE *fp = fopen(fn, "wb");
    if(!fp) { delete[] px; return; }
    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
    if(!png) { fclose(fp); delete[] px; return; }
    png_infop info = png_create_info_struct(png);
    if(!info) { png_destroy_write_struct(&png, 0); fclose(fp); delete[] px; return; }
    png_init_io(png, fp);
    png_set_IHDR(png, info, img->width, img->height, 8, PNG_COLOR_TYPE_RGBA,
                 PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_bytep *rows = new png_bytep[img->height];
    for(int i = 0; i < img->height; i++) rows[i] = px + i * img->width * 4;
    png_set_rows(png, info, rows);
    png_write_png(png, info, PNG_TRANSFORM_IDENTITY, 0);
    delete[] rows; delete[] px;
    png_destroy_write_struct(&png, &info);
    fclose(fp);
}

// Draw2DLine — line segment (GN_Vk.h float-overload version)
void Draw2DLine(sIMAGE *img, float x1, float y1, float x2, float y2,
                float width, float center) {
    if(!GN_Vk.initialized) return;
    // Compute perpendicular direction and build a rotated thin quad
    float dx = x2 - x1, dy = y2 - y1;
    float len = sqrtf(dx*dx + dy*dy);
    if(len < 0.001f) return;
    float nx = -dy / len, ny = dx / len; // perpendicular
    float hw = width * 0.5f;
    float cOff = (center - 0.5f) * width;
    float ox = nx * cOff, oy = ny * cOff;
    float px[4] = {x1 + ox - nx*hw, x2 + ox - nx*hw, x2 + ox + nx*hw, x1 + ox + nx*hw};
    float py[4] = {y1 + oy - ny*hw, y2 + oy - ny*hw, y2 + oy + ny*hw, y1 + oy + ny*hw};
    float vt[8] = {px[0],py[0], px[1],py[1], px[2],py[2], px[3],py[3]};
    ImageDraw4P(img, vt, 0xffffffff, 0, 0, 0.0f, 0.0f, 1.0f, 1.0f);
}

// ImageToData — simple GPU readback wrapper
bool ImageToData(sIMAGE *img, void *out, int fmt) {
    (void)fmt;
    if(!img || !out) return false;
    uint8_t *px = img->DataGet();
    if(!px) return false;
    VkDeviceSize sz = (VkDeviceSize)img->width * (VkDeviceSize)img->height * 4;
    memcpy(out, px, sz);
    delete[] px;
    return true;
}

// ═══════════════════════════════════════════════════════════
//  帧缓冲回读 (替代 OpenGL glReadPixels)
// ═══════════════════════════════════════════════════════════
uint8_t *GN_VkReadBack(int x, int y, int w, int h, int *size_out) {
    if(!GN_Vk.initialized || w <= 0 || h <= 0) return nullptr;
    uint32_t sw = GN_Vk.swapchainExtent.width, sh = GN_Vk.swapchainExtent.height;
    if(x < 0) { w += x; x = 0; }
    if(y < 0) { h += y; y = 0; }
    if(x + w > (int)sw) w = sw - x;
    if(y + h > (int)sh) h = sh - y;
    if(w <= 0 || h <= 0) return nullptr;
    VkDeviceSize sz = (VkDeviceSize)w * h * 4;
    // Create staging buffer
    VkBuffer stagingBuf; VkDeviceMemory stagingMem;
    CreateBuf(sz, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
              stagingBuf, stagingMem);
    VkImage srcImg = GN_Vk.swapchainImages[GN_Vk.currentFrame];
    // One-time command: transition → copy region → transition back
    VkCommandBuffer cb;
    {VkCommandBufferAllocateInfo ai={VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
     ai.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY; ai.commandPool=GN_Vk.commandPool; ai.commandBufferCount=1;
     vkAllocateCommandBuffers(GN_Vk.device,&ai,&cb);}
    {VkCommandBufferBeginInfo bi={VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
     bi.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT; vkBeginCommandBuffer(cb,&bi);}
    {VkImageMemoryBarrier b={VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
     b.oldLayout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
     b.newLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
     b.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED; b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
     b.image=srcImg; b.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;
     b.subresourceRange.levelCount=1; b.subresourceRange.layerCount=1;
     b.srcAccessMask=VK_ACCESS_MEMORY_READ_BIT; b.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;
     vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                          VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0,0,0,0, 1, &b);}
    {VkBufferImageCopy rgn={}; rgn.bufferOffset=0;
     rgn.imageSubresource.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;
     rgn.imageSubresource.layerCount=1;
     rgn.imageOffset={x, y, 0}; rgn.imageExtent={(uint32_t)w, (uint32_t)h, 1};
     vkCmdCopyImageToBuffer(cb, srcImg, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, stagingBuf, 1, &rgn);}
    {VkImageMemoryBarrier b={VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
     b.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
     b.newLayout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
     b.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED; b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
     b.image=srcImg; b.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;
     b.subresourceRange.levelCount=1; b.subresourceRange.layerCount=1;
     b.srcAccessMask=VK_ACCESS_TRANSFER_READ_BIT; b.dstAccessMask=VK_ACCESS_MEMORY_READ_BIT;
     vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT,
                          VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0,0,0,0, 1, &b);}
    vkEndCommandBuffer(cb);
    {VkSubmitInfo si={VK_STRUCTURE_TYPE_SUBMIT_INFO}; si.commandBufferCount=1;
     si.pCommandBuffers=&cb; vkQueueSubmit(GN_Vk.graphicsQueue,1,&si,VK_NULL_HANDLE);
     vkQueueWaitIdle(GN_Vk.graphicsQueue);}
    vkFreeCommandBuffers(GN_Vk.device, GN_Vk.commandPool, 1, &cb);
    // Read back
    uint8_t *result = new uint8_t[sz];
    void *d; vkMapMemory(GN_Vk.device, stagingMem, 0, sz, 0, &d);
    memcpy(result, d, sz);
    vkUnmapMemory(GN_Vk.device, stagingMem);
    vkDestroyBuffer(GN_Vk.device, stagingBuf, 0);
    vkFreeMemory(GN_Vk.device, stagingMem, 0);
    if(size_out) *size_out = (int)sz;
    return result;
}

void GN_VkReadBackFree(uint8_t *data) { delete[] data; }

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

// — 3D pass 状态 —
static bool s_in3DPass = false;
static VkFramebuffer *s_framebuffers3D = nullptr;

// — 阴影 pass 状态 —
static bool s_inShadowPass = false;

// ── 3D 绘制缓冲容量追踪 (共享于 GN_VkDraw3D / GN_VkDraw3DPBR / GN_VkShadowDraw) ──
static uint32_t s_vtxCap3D = 0;
static uint32_t s_idxCap3D = 0;

// ── 辅助: 确保 3D 顶点/索引缓冲容量足够, 不足则重新分配 ──
// ⚠️ 三个绘制函数 (Blinn-Phong / PBR / Shadow) 共享同一对缓冲,
//     每次绘制前都会重新上传顶点/索引数据, 因此数据正确性有保证.
static void Ensure3DBufferCapacity(uint32_t vtxSize, uint32_t idxSize) {
    // 顶点缓冲
    if (vtxSize > s_vtxCap3D) {
        if (GN_Vk.vertexBuffer3D) {
            vkDestroyBuffer(GN_Vk.device, GN_Vk.vertexBuffer3D, nullptr);
            GN_Vk.vertexBuffer3D = VK_NULL_HANDLE;
        }
        if (GN_Vk.vertexMemory3D) {
            vkFreeMemory(GN_Vk.device, GN_Vk.vertexMemory3D, nullptr);
            GN_Vk.vertexMemory3D = VK_NULL_HANDLE;
        }
        CreateBuf(vtxSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                  GN_Vk.vertexBuffer3D, GN_Vk.vertexMemory3D);
        s_vtxCap3D = vtxSize;
    }
    // 索引缓冲
    if (idxSize > s_idxCap3D) {
        if (GN_Vk.indexBuffer3D) {
            vkDestroyBuffer(GN_Vk.device, GN_Vk.indexBuffer3D, nullptr);
            GN_Vk.indexBuffer3D = VK_NULL_HANDLE;
        }
        if (GN_Vk.indexMemory3D) {
            vkFreeMemory(GN_Vk.device, GN_Vk.indexMemory3D, nullptr);
            GN_Vk.indexMemory3D = VK_NULL_HANDLE;
        }
        CreateBuf(idxSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                  GN_Vk.indexBuffer3D, GN_Vk.indexMemory3D);
        s_idxCap3D = idxSize;
    }
}

void GN_VkInit3D() {
    // ── 查询 MSAA 支持 ──────────────────────────────
    {VkPhysicalDeviceProperties props;
     vkGetPhysicalDeviceProperties(GN_Vk.physicalDevice, &props);
     VkSampleCountFlags counts = props.limits.framebufferColorSampleCounts
                               & props.limits.framebufferDepthSampleCounts;
     if (counts & VK_SAMPLE_COUNT_4_BIT)  GN_Vk.msaaSamples = VK_SAMPLE_COUNT_4_BIT;
     else if (counts & VK_SAMPLE_COUNT_2_BIT) GN_Vk.msaaSamples = VK_SAMPLE_COUNT_2_BIT;
     else GN_Vk.msaaSamples = VK_SAMPLE_COUNT_1_BIT;
    }

    // ── 寻找合适的深度格式 ──────────────────────────
    VkFormat depthFormats[] = {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D16_UNORM};
    GN_Vk.depthFormat = VK_FORMAT_D32_SFLOAT;
    for(auto df : depthFormats) {
        VkFormatProperties fp;
        vkGetPhysicalDeviceFormatProperties(GN_Vk.physicalDevice, df, &fp);
        if(fp.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
            GN_Vk.depthFormat = df; break;
        }
    }
    uint32_t sw = GN_Vk.swapchainExtent.width;
    uint32_t sh = GN_Vk.swapchainExtent.height;

    // ── 创建 MSAA 颜色缓冲 ───────────────────────────
    {VkImageCreateInfo ii={VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    ii.imageType=VK_IMAGE_TYPE_2D; ii.extent={sw, sh, 1};
    ii.mipLevels=1; ii.arrayLayers=1; ii.format=GN_Vk.swapchainFormat;
    ii.tiling=VK_IMAGE_TILING_OPTIMAL; ii.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;
    ii.usage=VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT|VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    ii.samples=GN_Vk.msaaSamples; ii.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
    vkCreateImage(GN_Vk.device,&ii,0,&GN_Vk.msaaColorImage);
    VkMemoryRequirements mr; vkGetImageMemoryRequirements(GN_Vk.device,GN_Vk.msaaColorImage,&mr);
    VkMemoryAllocateInfo mai={VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    mai.allocationSize=mr.size; mai.memoryTypeIndex=FindMemType(mr.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    vkAllocateMemory(GN_Vk.device,&mai,0,&GN_Vk.msaaColorMemory);
    vkBindImageMemory(GN_Vk.device,GN_Vk.msaaColorImage,GN_Vk.msaaColorMemory,0);}
    // MSAA color view
    {VkImageViewCreateInfo vi={VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    vi.image=GN_Vk.msaaColorImage; vi.viewType=VK_IMAGE_VIEW_TYPE_2D; vi.format=GN_Vk.swapchainFormat;
    vi.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT; vi.subresourceRange.levelCount=1; vi.subresourceRange.layerCount=1;
    vkCreateImageView(GN_Vk.device,&vi,0,&GN_Vk.msaaColorView);}

    // ── 创建 MSAA 深度缓冲 ───────────────────────────
    {VkImageCreateInfo ii={VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    ii.imageType=VK_IMAGE_TYPE_2D; ii.extent={sw, sh, 1};
    ii.mipLevels=1; ii.arrayLayers=1; ii.format=GN_Vk.depthFormat;
    ii.tiling=VK_IMAGE_TILING_OPTIMAL; ii.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;
    ii.usage=VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    ii.samples=GN_Vk.msaaSamples; ii.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
    vkCreateImage(GN_Vk.device,&ii,0,&GN_Vk.depthImage);
    VkMemoryRequirements dmr; vkGetImageMemoryRequirements(GN_Vk.device,GN_Vk.depthImage,&dmr);
    VkMemoryAllocateInfo dmai={VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    dmai.allocationSize=dmr.size; dmai.memoryTypeIndex=FindMemType(dmr.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    vkAllocateMemory(GN_Vk.device,&dmai,0,&GN_Vk.depthMemory);
    vkBindImageMemory(GN_Vk.device,GN_Vk.depthImage,GN_Vk.depthMemory,0);}
    {VkImageViewCreateInfo vi={VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    vi.image=GN_Vk.depthImage; vi.viewType=VK_IMAGE_VIEW_TYPE_2D; vi.format=GN_Vk.depthFormat;
    vi.subresourceRange.aspectMask=VK_IMAGE_ASPECT_DEPTH_BIT; vi.subresourceRange.levelCount=1; vi.subresourceRange.layerCount=1;
    vkCreateImageView(GN_Vk.device,&vi,0,&GN_Vk.depthView);}

    // ── 创建 3D Render Pass (3 附件: MSAA Color + MSAA Depth + Resolve) ──
    VkAttachmentDescription ads[3]={};
    // [0] MSAA Color — 多重采样, 瞬态 (不需要保存)
    ads[0].format=GN_Vk.swapchainFormat; ads[0].samples=GN_Vk.msaaSamples;
    ads[0].loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR; ads[0].storeOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;
    ads[0].initialLayout=VK_IMAGE_LAYOUT_UNDEFINED; ads[0].finalLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    // [1] MSAA Depth — 多重采样, 瞬态
    ads[1].format=GN_Vk.depthFormat; ads[1].samples=GN_Vk.msaaSamples;
    ads[1].loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR; ads[1].storeOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;
    ads[1].initialLayout=VK_IMAGE_LAYOUT_UNDEFINED; ads[1].finalLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    // [2] Resolve — 单采样, 输出到 swapchain
    ads[2].format=GN_Vk.swapchainFormat; ads[2].samples=VK_SAMPLE_COUNT_1_BIT;
    ads[2].loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR; ads[2].storeOp=VK_ATTACHMENT_STORE_OP_STORE;
    ads[2].initialLayout=VK_IMAGE_LAYOUT_UNDEFINED; ads[2].finalLayout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference cr={0,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};         // MSAA color
    VkAttachmentReference dr={1,VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};  // MSAA depth
    VkAttachmentReference rr={2,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};          // resolve target
    VkSubpassDescription sp={}; sp.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS;
    sp.colorAttachmentCount=1; sp.pColorAttachments=&cr;
    sp.pResolveAttachments=&rr;   // ★ MSAA resolve
    sp.pDepthStencilAttachment=&dr;

    VkRenderPassCreateInfo rpi={VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    rpi.attachmentCount=3; rpi.pAttachments=ads; rpi.subpassCount=1; rpi.pSubpasses=&sp;
    vkCreateRenderPass(GN_Vk.device,&rpi,0,&GN_Vk.renderPass3D);

    // 创建 3D framebuffers (3 附件: MSAA color + MSAA depth + swapchain resolve)
    s_framebuffers3D = new VkFramebuffer[GN_Vk.swapchainImageCount];
    for(uint32_t i=0;i<GN_Vk.swapchainImageCount;i++){
        VkImageView atts[3]={GN_Vk.msaaColorView, GN_Vk.depthView, GN_Vk.swapchainViews[i]};
        VkFramebufferCreateInfo fci={VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        fci.renderPass=GN_Vk.renderPass3D; fci.attachmentCount=3; fci.pAttachments=atts;
        fci.width=sw; fci.height=sh; fci.layers=1;
        vkCreateFramebuffer(GN_Vk.device,&fci,0,&s_framebuffers3D[i]);
    }

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

    VkShaderModule vm3d, fm3d;
    {VkShaderModuleCreateInfo sm={VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    sm.codeSize=vertSpv.size()*sizeof(uint32_t); sm.pCode=vertSpv.data();
    vkCreateShaderModule(GN_Vk.device,&sm,0,&vm3d);}
    {VkShaderModuleCreateInfo sm={VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    sm.codeSize=fragSpv.size()*sizeof(uint32_t); sm.pCode=fragSpv.data();
    vkCreateShaderModule(GN_Vk.device,&sm,0,&fm3d);}

    // ── 3D 描述符布局 ────────────────────────────────
    // Binding 0: Scene UBO (uniform buffer) — 顶点+片段着色器
    // Binding 1: Texture sampler (combined image sampler) — 片段着色器
    // Binding 2: Light UBO (uniform buffer) — 片段着色器
    {VkDescriptorSetLayoutBinding dslb[3]={};
    dslb[0].binding=0; dslb[0].descriptorType=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    dslb[0].descriptorCount=1; dslb[0].stageFlags=VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT;
    dslb[1].binding=1; dslb[1].descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    dslb[1].descriptorCount=1; dslb[1].stageFlags=VK_SHADER_STAGE_FRAGMENT_BIT;
    dslb[2].binding=2; dslb[2].descriptorType=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    dslb[2].descriptorCount=1; dslb[2].stageFlags=VK_SHADER_STAGE_FRAGMENT_BIT;
    VkDescriptorSetLayoutCreateInfo dslci={VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    dslci.bindingCount=3; dslci.pBindings=dslb;
    vkCreateDescriptorSetLayout(GN_Vk.device,&dslci,0,&GN_Vk.descriptorLayout3D);}

    // ── 3D 管线布局 (含 Push Constant) ───────────────
    {VkPushConstantRange pcr={};
    pcr.stageFlags=VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT;
    pcr.offset=0; pcr.size=sizeof(VkModelPushConstBP);
    VkPipelineLayoutCreateInfo plci={VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    plci.setLayoutCount=1; plci.pSetLayouts=&GN_Vk.descriptorLayout3D;
    plci.pushConstantRangeCount=1; plci.pPushConstantRanges=&pcr;
    vkCreatePipelineLayout(GN_Vk.device,&plci,0,&GN_Vk.pipelineLayout3D);}

    // ── 3D 图形管线 ──────────────────────────────────
    {VkPipelineShaderStageCreateInfo ss[2]={{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO},{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO}};
    ss[0].stage=VK_SHADER_STAGE_VERTEX_BIT; ss[0].module=vm3d; ss[0].pName="main";
    ss[1].stage=VK_SHADER_STAGE_FRAGMENT_BIT; ss[1].module=fm3d; ss[1].pName="main";

    VkVertexInputBindingDescription vib={0,sizeof(VkVertex3D),VK_VERTEX_INPUT_RATE_VERTEX};
    VkVertexInputAttributeDescription via[4]={
        {0,0,VK_FORMAT_R32G32B32_SFLOAT,offsetof(VkVertex3D,px)},       // pos
        {1,0,VK_FORMAT_R32G32B32_SFLOAT,offsetof(VkVertex3D,nx)},       // normal
        {2,0,VK_FORMAT_R32G32B32_SFLOAT,offsetof(VkVertex3D,tx)},       // tangent
        {3,0,VK_FORMAT_R32G32_SFLOAT,offsetof(VkVertex3D,u)}            // uv
    };
    VkPipelineVertexInputStateCreateInfo vis={VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vis.vertexBindingDescriptionCount=1; vis.pVertexBindingDescriptions=&vib;
    vis.vertexAttributeDescriptionCount=4; vis.pVertexAttributeDescriptions=via;

    VkPipelineInputAssemblyStateCreateInfo ias={VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    ias.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkViewport vp={0,0,(float)GN_Vk.swapchainExtent.width,(float)GN_Vk.swapchainExtent.height,0,1};
    VkRect2D sc={{0,0},GN_Vk.swapchainExtent};
    VkPipelineViewportStateCreateInfo vps={VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    vps.viewportCount=1; vps.pViewports=&vp; vps.scissorCount=1; vps.pScissors=&sc;

    VkPipelineRasterizationStateCreateInfo rs={VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    rs.polygonMode=VK_POLYGON_MODE_FILL; rs.cullMode=VK_CULL_MODE_BACK_BIT;
    rs.frontFace=VK_FRONT_FACE_COUNTER_CLOCKWISE; rs.lineWidth=1.0f;
    // Cube 网格逆时针绕序, Vulkan Y-flip 后仍为逆时针 → 用 COUNTER_CLOCKWISE

    VkPipelineMultisampleStateCreateInfo ms={VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    ms.rasterizationSamples=GN_Vk.msaaSamples;
    
    VkPipelineDepthStencilStateCreateInfo dss={VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    dss.depthTestEnable=VK_TRUE; dss.depthWriteEnable=VK_TRUE;
    dss.depthCompareOp=VK_COMPARE_OP_LESS;

    VkPipelineColorBlendAttachmentState cba={};
    cba.colorWriteMask=0xf; cba.blendEnable=VK_FALSE; // 3D 不使用混合

    VkPipelineColorBlendStateCreateInfo cbs={VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    cbs.attachmentCount=1; cbs.pAttachments=&cba;

    VkGraphicsPipelineCreateInfo pci={VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    pci.stageCount=2; pci.pStages=ss; pci.pVertexInputState=&vis;
    pci.pInputAssemblyState=&ias; pci.pViewportState=&vps;
    pci.pRasterizationState=&rs; pci.pMultisampleState=&ms;
    pci.pDepthStencilState=&dss; pci.pColorBlendState=&cbs;
    pci.layout=GN_Vk.pipelineLayout3D; pci.renderPass=GN_Vk.renderPass3D; pci.subpass=0;
    vkCreateGraphicsPipelines(GN_Vk.device,0,1,&pci,0,&GN_Vk.pipeline3D);

    vkDestroyShaderModule(GN_Vk.device,vm3d,0); vkDestroyShaderModule(GN_Vk.device,fm3d,0);}

    // ── 3D 场景 UBO ──────────────────────────────────
    CreateBuf(sizeof(VkSceneUBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
              GN_Vk.uniformBuffer3D, GN_Vk.uniformMemory3D);

    // ── 3D 光照 UBO ──────────────────────────────────
    // 最大: GN_MAX_LIGHTS(8) * sizeof(sLIGHT_GPU)(80) + 20 = 660 bytes
    CreateBuf(1024, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
              GN_Vk.lightBuffer3D, GN_Vk.lightMemory3D);

    // ── 默认 3D 描述符集 ─────────────────────────────
    {VkDescriptorSetAllocateInfo dai={VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    dai.descriptorPool=GN_Vk.descriptorPool; dai.descriptorSetCount=1;
    dai.pSetLayouts=&GN_Vk.descriptorLayout3D;
    vkAllocateDescriptorSets(GN_Vk.device,&dai,&GN_Vk.descriptorSet3D);
    VkDescriptorBufferInfo dbi={GN_Vk.uniformBuffer3D,0,sizeof(VkSceneUBO)};
    VkDescriptorBufferInfo lbi={GN_Vk.lightBuffer3D,0,1024};
    VkDescriptorImageInfo dii={GN_Vk.defaultSampler, GN_Vk.defaultImageView,
                                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    VkWriteDescriptorSet w[3]={{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET},
                                {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET},
                                {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET}};
    w[0].dstSet=GN_Vk.descriptorSet3D; w[0].dstBinding=0;
    w[0].descriptorType=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; w[0].descriptorCount=1; w[0].pBufferInfo=&dbi;
    w[1].dstSet=GN_Vk.descriptorSet3D; w[1].dstBinding=1;
    w[1].descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; w[1].descriptorCount=1; w[1].pImageInfo=&dii;
    w[2].dstSet=GN_Vk.descriptorSet3D; w[2].dstBinding=2;
    w[2].descriptorType=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; w[2].descriptorCount=1; w[2].pBufferInfo=&lbi;
    vkUpdateDescriptorSets(GN_Vk.device,3,w,0,0);}

    // ═══════════════════════════════════════════════════════
    //  PBR 描述符布局 (Binding 5 = skyboxCubemap for IBL)
    // ═══════════════════════════════════════════════════════
    {VkDescriptorSetLayoutBinding dslb[6]={};
    dslb[0].binding=0; dslb[0].descriptorType=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    dslb[0].descriptorCount=1; dslb[0].stageFlags=VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT;
    dslb[1].binding=1; dslb[1].descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    dslb[1].descriptorCount=1; dslb[1].stageFlags=VK_SHADER_STAGE_FRAGMENT_BIT;
    dslb[2].binding=2; dslb[2].descriptorType=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    dslb[2].descriptorCount=1; dslb[2].stageFlags=VK_SHADER_STAGE_FRAGMENT_BIT;
    dslb[3].binding=3; dslb[3].descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    dslb[3].descriptorCount=1; dslb[3].stageFlags=VK_SHADER_STAGE_FRAGMENT_BIT;
    dslb[4].binding=4; dslb[4].descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    dslb[4].descriptorCount=1; dslb[4].stageFlags=VK_SHADER_STAGE_FRAGMENT_BIT;
    dslb[5].binding=5; dslb[5].descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    dslb[5].descriptorCount=1; dslb[5].stageFlags=VK_SHADER_STAGE_FRAGMENT_BIT;
    VkDescriptorSetLayoutCreateInfo dslci={VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    dslci.bindingCount=6; dslci.pBindings=dslb;
    vkCreateDescriptorSetLayout(GN_Vk.device,&dslci,0,&GN_Vk.descriptorLayoutPBR);}

    // ── PBR 管线布局 ─────────────────────────────────
    {VkPushConstantRange pcr={};
    pcr.stageFlags=VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT;
    pcr.offset=0; pcr.size=sizeof(VkModelPushConstPBR);
    VkPipelineLayoutCreateInfo plci={VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    plci.setLayoutCount=1; plci.pSetLayouts=&GN_Vk.descriptorLayoutPBR;
    plci.pushConstantRangeCount=1; plci.pPushConstantRanges=&pcr;
    vkCreatePipelineLayout(GN_Vk.device,&plci,0,&GN_Vk.pipelineLayoutPBR);}

    // ── 加载 PBR 着色器并创建管线 ───────────────────
    {auto pbrVert = LoadSPIRVFile("assets/shaders/gn3d_pbr.vert.spv");
    auto pbrFrag = LoadSPIRVFile("assets/shaders/gn3d_pbr.frag.spv");
    // 嵌入式 SPIR-V 回退
    if(pbrVert.empty()) pbrVert.assign(assets_shaders_gn3d_pbr_vert_spv,
        assets_shaders_gn3d_pbr_vert_spv + assets_shaders_gn3d_pbr_vert_spv_len);
    if(pbrFrag.empty()) pbrFrag.assign(assets_shaders_gn3d_pbr_frag_spv,
        assets_shaders_gn3d_pbr_frag_spv + assets_shaders_gn3d_pbr_frag_spv_len);
    VkShaderModule pvm=0, pfm=0;
    if(!pbrVert.empty() && !pbrFrag.empty()){
        {VkShaderModuleCreateInfo sm={VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        sm.codeSize=pbrVert.size()*sizeof(uint32_t); sm.pCode=pbrVert.data();
        vkCreateShaderModule(GN_Vk.device,&sm,0,&pvm);}
        {VkShaderModuleCreateInfo sm={VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        sm.codeSize=pbrFrag.size()*sizeof(uint32_t); sm.pCode=pbrFrag.data();
        vkCreateShaderModule(GN_Vk.device,&sm,0,&pfm);}

        VkPipelineShaderStageCreateInfo ss[2]={{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO},{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO}};
        ss[0].stage=VK_SHADER_STAGE_VERTEX_BIT; ss[0].module=pvm; ss[0].pName="main";
        ss[1].stage=VK_SHADER_STAGE_FRAGMENT_BIT; ss[1].module=pfm; ss[1].pName="main";

        VkVertexInputBindingDescription vib={0,sizeof(VkVertex3D),VK_VERTEX_INPUT_RATE_VERTEX};
        VkVertexInputAttributeDescription via[4]={
            {0,0,VK_FORMAT_R32G32B32_SFLOAT,offsetof(VkVertex3D,px)},
            {1,0,VK_FORMAT_R32G32B32_SFLOAT,offsetof(VkVertex3D,nx)},
            {2,0,VK_FORMAT_R32G32B32_SFLOAT,offsetof(VkVertex3D,tx)},
            {3,0,VK_FORMAT_R32G32_SFLOAT,offsetof(VkVertex3D,u)}
        };
        VkPipelineVertexInputStateCreateInfo vis={VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        vis.vertexBindingDescriptionCount=1; vis.pVertexBindingDescriptions=&vib;
        vis.vertexAttributeDescriptionCount=4; vis.pVertexAttributeDescriptions=via;

        VkPipelineInputAssemblyStateCreateInfo ias={VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
        ias.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        VkViewport vp={0,0,(float)GN_Vk.swapchainExtent.width,(float)GN_Vk.swapchainExtent.height,0,1};
        VkRect2D sc={{0,0},GN_Vk.swapchainExtent};
        VkPipelineViewportStateCreateInfo vps={VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
        vps.viewportCount=1; vps.pViewports=&vp; vps.scissorCount=1; vps.pScissors=&sc;
        VkPipelineRasterizationStateCreateInfo rs={VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
        rs.polygonMode=VK_POLYGON_MODE_FILL; rs.cullMode=VK_CULL_MODE_BACK_BIT;
        rs.frontFace=VK_FRONT_FACE_CLOCKWISE; rs.lineWidth=1.0f;
        VkPipelineMultisampleStateCreateInfo ms={VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
        ms.rasterizationSamples=GN_Vk.msaaSamples;
        VkPipelineDepthStencilStateCreateInfo dss={VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
        dss.depthTestEnable=VK_TRUE; dss.depthWriteEnable=VK_TRUE;
    dss.depthCompareOp=VK_COMPARE_OP_LESS;
        VkPipelineColorBlendAttachmentState cba={}; cba.colorWriteMask=0xf;
        VkPipelineColorBlendStateCreateInfo cbs={VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
        cbs.attachmentCount=1; cbs.pAttachments=&cba;
        VkGraphicsPipelineCreateInfo pci={VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
        pci.stageCount=2; pci.pStages=ss; pci.pVertexInputState=&vis;
        pci.pInputAssemblyState=&ias; pci.pViewportState=&vps;
        pci.pRasterizationState=&rs; pci.pMultisampleState=&ms;
        pci.pDepthStencilState=&dss; pci.pColorBlendState=&cbs;
        pci.layout=GN_Vk.pipelineLayoutPBR; pci.renderPass=GN_Vk.renderPass3D; pci.subpass=0;
        vkCreateGraphicsPipelines(GN_Vk.device,0,1,&pci,0,&GN_Vk.pipelinePBR);

        vkDestroyShaderModule(GN_Vk.device,pvm,0); vkDestroyShaderModule(GN_Vk.device,pfm,0);
    }}

    // ═══════════════════════════════════════════════════════
    //  阴影映射系统
    // ═══════════════════════════════════════════════════════
    uint32_t smSize = GN_Vk.shadowMapSize;
    // 创建 Shadow Map 图像
    {VkImageCreateInfo ii={VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    ii.imageType=VK_IMAGE_TYPE_2D; ii.extent={smSize,smSize,1};
    ii.mipLevels=1; ii.arrayLayers=1; ii.format=VK_FORMAT_D32_SFLOAT;
    ii.tiling=VK_IMAGE_TILING_OPTIMAL; ii.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;
    ii.usage=VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT|VK_IMAGE_USAGE_SAMPLED_BIT;
    ii.samples=VK_SAMPLE_COUNT_1_BIT; ii.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
    vkCreateImage(GN_Vk.device,&ii,0,&GN_Vk.shadowImage);
    VkMemoryRequirements mr; vkGetImageMemoryRequirements(GN_Vk.device,GN_Vk.shadowImage,&mr);
    VkMemoryAllocateInfo mai={VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    mai.allocationSize=mr.size; mai.memoryTypeIndex=FindMemType(mr.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    vkAllocateMemory(GN_Vk.device,&mai,0,&GN_Vk.shadowMemory);
    vkBindImageMemory(GN_Vk.device,GN_Vk.shadowImage,GN_Vk.shadowMemory,0);}

    {VkImageViewCreateInfo vi={VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    vi.image=GN_Vk.shadowImage; vi.viewType=VK_IMAGE_VIEW_TYPE_2D; vi.format=VK_FORMAT_D32_SFLOAT;
    vi.subresourceRange.aspectMask=VK_IMAGE_ASPECT_DEPTH_BIT; vi.subresourceRange.levelCount=1; vi.subresourceRange.layerCount=1;
    vkCreateImageView(GN_Vk.device,&vi,0,&GN_Vk.shadowView);}

    // Shadow sampler (compare mode + border color)
    {VkSamplerCreateInfo sci={VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    sci.magFilter=VK_FILTER_LINEAR; sci.minFilter=VK_FILTER_LINEAR;
    sci.addressModeU=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    sci.addressModeV=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    sci.addressModeW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    sci.borderColor=VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    sci.compareEnable=VK_TRUE; sci.compareOp=VK_COMPARE_OP_LESS;
    vkCreateSampler(GN_Vk.device,&sci,0,&GN_Vk.shadowSampler);}

    // Shadow render pass (depth-only)
    {VkAttachmentDescription ad={};
    ad.format=VK_FORMAT_D32_SFLOAT; ad.samples=VK_SAMPLE_COUNT_1_BIT;
    ad.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR; ad.storeOp=VK_ATTACHMENT_STORE_OP_STORE;
    ad.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED; ad.finalLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    VkAttachmentReference dr={0,VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
    VkSubpassDescription sp={}; sp.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS;
    sp.pDepthStencilAttachment=&dr;
    VkRenderPassCreateInfo rpi={VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    rpi.attachmentCount=1; rpi.pAttachments=&ad; rpi.subpassCount=1; rpi.pSubpasses=&sp;
    vkCreateRenderPass(GN_Vk.device,&rpi,0,&GN_Vk.shadowRenderPass);}

    {VkFramebufferCreateInfo fci={VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    fci.renderPass=GN_Vk.shadowRenderPass; fci.attachmentCount=1; fci.pAttachments=&GN_Vk.shadowView;
    fci.width=smSize; fci.height=smSize; fci.layers=1;
    vkCreateFramebuffer(GN_Vk.device,&fci,0,&GN_Vk.shadowFramebuffer);}

    // Shadow 描述符布局 (Binding 0 = UBO for light VP)
    {VkDescriptorSetLayoutBinding dslb[1]={};
    dslb[0].binding=0; dslb[0].descriptorType=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    dslb[0].descriptorCount=1; dslb[0].stageFlags=VK_SHADER_STAGE_VERTEX_BIT;
    VkDescriptorSetLayoutCreateInfo dslci={VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    dslci.bindingCount=1; dslci.pBindings=dslb;
    vkCreateDescriptorSetLayout(GN_Vk.device,&dslci,0,&GN_Vk.shadowDescriptorLayout);}

    {VkPushConstantRange pcr={};
    pcr.stageFlags=VK_SHADER_STAGE_VERTEX_BIT; pcr.offset=0; pcr.size=64; // just model mat4
    VkPipelineLayoutCreateInfo plci={VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    plci.setLayoutCount=1; plci.pSetLayouts=&GN_Vk.shadowDescriptorLayout;
    plci.pushConstantRangeCount=1; plci.pPushConstantRanges=&pcr;
    vkCreatePipelineLayout(GN_Vk.device,&plci,0,&GN_Vk.shadowPipelineLayout);}

    // Shadow UBO
    CreateBuf(64, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
              GN_Vk.shadowUBOBuffer, GN_Vk.shadowUBOMemory);

    // Shadow descriptor set
    {VkDescriptorSetAllocateInfo dai={VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    dai.descriptorPool=GN_Vk.descriptorPool; dai.descriptorSetCount=1;
    dai.pSetLayouts=&GN_Vk.shadowDescriptorLayout;
    vkAllocateDescriptorSets(GN_Vk.device,&dai,&GN_Vk.shadowDescriptorSet);
    VkDescriptorBufferInfo dbi={GN_Vk.shadowUBOBuffer,0,64};
    VkWriteDescriptorSet w={VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    w.dstSet=GN_Vk.shadowDescriptorSet; w.dstBinding=0;
    w.descriptorType=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; w.descriptorCount=1; w.pBufferInfo=&dbi;
    vkUpdateDescriptorSets(GN_Vk.device,1,&w,0,0);}

    // Shadow pipeline
    {auto svSpv = LoadSPIRVFile("assets/shaders/gn3d_shadow.vert.spv");
    // 嵌入式 SPIR-V 回退
    if(svSpv.empty()) svSpv.assign(assets_shaders_gn3d_shadow_vert_spv,
        assets_shaders_gn3d_shadow_vert_spv + assets_shaders_gn3d_shadow_vert_spv_len);
    VkShaderModule svm=0;
    if(!svSpv.empty()){
        {VkShaderModuleCreateInfo sm={VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        sm.codeSize=svSpv.size()*sizeof(uint32_t); sm.pCode=svSpv.data();
        vkCreateShaderModule(GN_Vk.device,&sm,0,&svm);}
        VkPipelineShaderStageCreateInfo ss={VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
        ss.stage=VK_SHADER_STAGE_VERTEX_BIT; ss.module=svm; ss.pName="main";
        VkVertexInputBindingDescription vib={0,sizeof(VkVertex3D),VK_VERTEX_INPUT_RATE_VERTEX};
        VkVertexInputAttributeDescription via={0,0,VK_FORMAT_R32G32B32_SFLOAT,offsetof(VkVertex3D,px)};
        VkPipelineVertexInputStateCreateInfo vis={VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        vis.vertexBindingDescriptionCount=1; vis.pVertexBindingDescriptions=&vib;
        vis.vertexAttributeDescriptionCount=1; vis.pVertexAttributeDescriptions=&via;
        VkPipelineInputAssemblyStateCreateInfo ias={VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
        ias.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        VkViewport vp={0,0,(float)smSize,(float)smSize,0,1};
        VkRect2D sc={{0,0},{smSize,smSize}};
        VkPipelineViewportStateCreateInfo vps={VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
        vps.viewportCount=1; vps.pViewports=&vp; vps.scissorCount=1; vps.pScissors=&sc;
        VkPipelineRasterizationStateCreateInfo rs={VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
        rs.polygonMode=VK_POLYGON_MODE_FILL; rs.cullMode=VK_CULL_MODE_BACK_BIT;
        rs.frontFace=VK_FRONT_FACE_CLOCKWISE; rs.lineWidth=1.0f;
        rs.depthBiasEnable=VK_TRUE; rs.depthBiasConstantFactor=1.25f; rs.depthBiasSlopeFactor=1.75f;
        VkPipelineMultisampleStateCreateInfo ms={VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
        ms.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;
        VkPipelineDepthStencilStateCreateInfo dss={VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
        dss.depthTestEnable=VK_TRUE; dss.depthWriteEnable=VK_TRUE; dss.depthCompareOp=VK_COMPARE_OP_LESS;
        VkGraphicsPipelineCreateInfo pci={VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
        pci.stageCount=1; pci.pStages=&ss; pci.pVertexInputState=&vis;
        pci.pInputAssemblyState=&ias; pci.pViewportState=&vps; pci.pRasterizationState=&rs;
        pci.pMultisampleState=&ms; pci.pDepthStencilState=&dss;
        pci.layout=GN_Vk.shadowPipelineLayout; pci.renderPass=GN_Vk.shadowRenderPass; pci.subpass=0;
        vkCreateGraphicsPipelines(GN_Vk.device,0,1,&pci,0,&GN_Vk.shadowPipeline);
        vkDestroyShaderModule(GN_Vk.device,svm,0);
    }}

    // ── 创建 PBR 描述符集 (必须在 Shadow Map 创建之后) ──
    {VkDescriptorSetAllocateInfo dai={VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    dai.descriptorPool=GN_Vk.descriptorPool; dai.descriptorSetCount=1;
    dai.pSetLayouts=&GN_Vk.descriptorLayoutPBR;
    vkAllocateDescriptorSets(GN_Vk.device,&dai,&GN_Vk.descriptorSetPBR);

    VkDescriptorBufferInfo dbiScene={GN_Vk.uniformBuffer3D,0,sizeof(VkSceneUBO)};
    VkDescriptorBufferInfo dbiLight={GN_Vk.lightBuffer3D,0,1024};
    VkDescriptorImageInfo diiTex={GN_Vk.defaultSampler, GN_Vk.defaultImageView,
                                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    VkDescriptorImageInfo diiNormal={GN_Vk.defaultSampler, GN_Vk.defaultImageView,
                                      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    VkDescriptorImageInfo diiShadow={GN_Vk.shadowSampler,GN_Vk.shadowView,
                                      VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL};

    VkWriteDescriptorSet w[5]={{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET},
                                {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET},
                                {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET},
                                {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET},
                                {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET}};
    w[0].dstSet=GN_Vk.descriptorSetPBR; w[0].dstBinding=0;
    w[0].descriptorType=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; w[0].descriptorCount=1;
    w[0].pBufferInfo=&dbiScene;
    w[1].dstSet=GN_Vk.descriptorSetPBR; w[1].dstBinding=1;
    w[1].descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; w[1].descriptorCount=1;
    w[1].pImageInfo=&diiTex;
    w[2].dstSet=GN_Vk.descriptorSetPBR; w[2].dstBinding=2;
    w[2].descriptorType=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; w[2].descriptorCount=1;
    w[2].pBufferInfo=&dbiLight;
    w[3].dstSet=GN_Vk.descriptorSetPBR; w[3].dstBinding=3;
    w[3].descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; w[3].descriptorCount=1;
    w[3].pImageInfo=&diiNormal;
    w[4].dstSet=GN_Vk.descriptorSetPBR; w[4].dstBinding=4;
    w[4].descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; w[4].descriptorCount=1;
    w[4].pImageInfo=&diiShadow;
    vkUpdateDescriptorSets(GN_Vk.device,5,w,0,0);
    // Binding 5 (skybox cubemap) 将在天空盒初始化后更新
    }

    // ═══════════════════════════════════════════════════════
    //  HDR 离屏目标 (R16G16B16A16_SFLOAT — 支持 HDR > 1.0)
    // ═══════════════════════════════════════════════════════
    {VkImageCreateInfo ii={VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    ii.imageType=VK_IMAGE_TYPE_2D; ii.extent={sw, sh, 1};
    ii.mipLevels=1; ii.arrayLayers=1; ii.format=VK_FORMAT_R16G16B16A16_SFLOAT;
    ii.tiling=VK_IMAGE_TILING_OPTIMAL; ii.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;
    ii.usage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_SAMPLED_BIT;
    ii.samples=VK_SAMPLE_COUNT_1_BIT; ii.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
    vkCreateImage(GN_Vk.device,&ii,0,&GN_Vk.hdrImage);
    VkMemoryRequirements hmr; vkGetImageMemoryRequirements(GN_Vk.device,GN_Vk.hdrImage,&hmr);
    VkMemoryAllocateInfo hmai={VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    hmai.allocationSize=hmr.size; hmai.memoryTypeIndex=FindMemType(hmr.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    vkAllocateMemory(GN_Vk.device,&hmai,0,&GN_Vk.hdrMemory);
    vkBindImageMemory(GN_Vk.device,GN_Vk.hdrImage,GN_Vk.hdrMemory,0);}
    {VkImageViewCreateInfo vi={VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    vi.image=GN_Vk.hdrImage; vi.viewType=VK_IMAGE_VIEW_TYPE_2D; vi.format=VK_FORMAT_R16G16B16A16_SFLOAT;
    vi.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT; vi.subresourceRange.levelCount=1; vi.subresourceRange.layerCount=1;
    vkCreateImageView(GN_Vk.device,&vi,0,&GN_Vk.hdrView);}
    {VkSamplerCreateInfo sci={VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    sci.magFilter=VK_FILTER_LINEAR; sci.minFilter=VK_FILTER_LINEAR;
    sci.addressModeU=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sci.addressModeV=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    vkCreateSampler(GN_Vk.device,&sci,0,&GN_Vk.hdrSampler);}

    // ── 3D HDR RenderPass (3 附件: MSAA Color + MSAA Depth + HDR Resolve) ──
    // 与 renderPass3D 结构完全相同, 仅 resolve 附件格式不同
    {VkAttachmentDescription hds[3]={};
    hds[0].format=GN_Vk.swapchainFormat; hds[0].samples=GN_Vk.msaaSamples;
    hds[0].loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR; hds[0].storeOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;
    hds[0].initialLayout=VK_IMAGE_LAYOUT_UNDEFINED; hds[0].finalLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    hds[1].format=GN_Vk.depthFormat; hds[1].samples=GN_Vk.msaaSamples;
    hds[1].loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR; hds[1].storeOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;
    hds[1].initialLayout=VK_IMAGE_LAYOUT_UNDEFINED; hds[1].finalLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    hds[2].format=VK_FORMAT_R16G16B16A16_SFLOAT; hds[2].samples=VK_SAMPLE_COUNT_1_BIT;
    hds[2].loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR; hds[2].storeOp=VK_ATTACHMENT_STORE_OP_STORE;
    hds[2].initialLayout=VK_IMAGE_LAYOUT_UNDEFINED; hds[2].finalLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    VkAttachmentReference hcr={0,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkAttachmentReference hdr={1,VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
    VkAttachmentReference hrr={2,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkSubpassDescription hsp={}; hsp.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS;
    hsp.colorAttachmentCount=1; hsp.pColorAttachments=&hcr;
    hsp.pResolveAttachments=&hrr; hsp.pDepthStencilAttachment=&hdr;
    VkRenderPassCreateInfo hrpi={VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    hrpi.attachmentCount=3; hrpi.pAttachments=hds; hrpi.subpassCount=1; hrpi.pSubpasses=&hsp;
    vkCreateRenderPass(GN_Vk.device,&hrpi,0,&GN_Vk.hdrRenderPass);}
    // HDR Framebuffers (per swapchain image: MSAA Color + MSAA Depth + HDR resolve)
    GN_Vk.hdrFramebuffers = new VkFramebuffer[GN_Vk.swapchainImageCount];
    for(uint32_t i=0;i<GN_Vk.swapchainImageCount;i++){
        VkImageView hatts[3]={GN_Vk.msaaColorView, GN_Vk.depthView, GN_Vk.hdrView};
        VkFramebufferCreateInfo fci={VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        fci.renderPass=GN_Vk.hdrRenderPass; fci.attachmentCount=3; fci.pAttachments=hatts;
        fci.width=sw; fci.height=sh; fci.layers=1;
        vkCreateFramebuffer(GN_Vk.device,&fci,0,&GN_Vk.hdrFramebuffers[i]);
    }

    // ── 编译合成着色器 ──
    auto compVertSpv = LoadSPIRVFile("assets/shaders/gn3d_composite.vert.spv");
    auto compFragSpv = LoadSPIRVFile("assets/shaders/gn3d_composite.frag.spv");
    VkShaderModule csvm=0, csfm=0;
    if(!compVertSpv.empty() && !compFragSpv.empty()){
        {VkShaderModuleCreateInfo sm={VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        sm.codeSize=compVertSpv.size()*sizeof(uint32_t); sm.pCode=compVertSpv.data();
        vkCreateShaderModule(GN_Vk.device,&sm,0,&csvm);}
        {VkShaderModuleCreateInfo sm={VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        sm.codeSize=compFragSpv.size()*sizeof(uint32_t); sm.pCode=compFragSpv.data();
        vkCreateShaderModule(GN_Vk.device,&sm,0,&csfm);}

        // 描述符布局: Binding 1 = HDR sampler (Fragment)
        {VkDescriptorSetLayoutBinding dslb[1]={};
        dslb[0].binding=1; dslb[0].descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        dslb[0].descriptorCount=1; dslb[0].stageFlags=VK_SHADER_STAGE_FRAGMENT_BIT;
        VkDescriptorSetLayoutCreateInfo dslci={VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        dslci.bindingCount=1; dslci.pBindings=dslb;
        vkCreateDescriptorSetLayout(GN_Vk.device,&dslci,0,&GN_Vk.compositeDescriptorLayout);}
        // 管线布局
        {VkPipelineLayoutCreateInfo plci={VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        plci.setLayoutCount=1; plci.pSetLayouts=&GN_Vk.compositeDescriptorLayout;
        vkCreatePipelineLayout(GN_Vk.device,&plci,0,&GN_Vk.compositePipelineLayout);}

        // 管线: 全屏三角形, 无顶点输入
        VkPipelineShaderStageCreateInfo ss[2]={{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO},{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO}};
        ss[0].stage=VK_SHADER_STAGE_VERTEX_BIT; ss[0].module=csvm; ss[0].pName="main";
        ss[1].stage=VK_SHADER_STAGE_FRAGMENT_BIT; ss[1].module=csfm; ss[1].pName="main";
        VkPipelineVertexInputStateCreateInfo vis={VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        vis.vertexBindingDescriptionCount=0; vis.vertexAttributeDescriptionCount=0;
        VkPipelineInputAssemblyStateCreateInfo ias={VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
        ias.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        VkViewport vp={0,0,(float)sw,(float)sh,0,1};
        VkRect2D sc={{0,0},{sw,sh}};
        VkPipelineViewportStateCreateInfo vps={VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
        vps.viewportCount=1; vps.pViewports=&vp; vps.scissorCount=1; vps.pScissors=&sc;
        VkPipelineRasterizationStateCreateInfo rs={VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
        rs.polygonMode=VK_POLYGON_MODE_FILL; rs.cullMode=VK_CULL_MODE_NONE; rs.lineWidth=1.0f;
        VkPipelineMultisampleStateCreateInfo ms={VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
        ms.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;
        VkPipelineColorBlendAttachmentState cba={}; cba.colorWriteMask=0xf;
        VkPipelineColorBlendStateCreateInfo cbs={VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
        cbs.attachmentCount=1; cbs.pAttachments=&cba;

        // Composite RenderPass: swapchain 格式, 单采样
        {VkAttachmentDescription cad={};
        cad.format=GN_Vk.swapchainFormat; cad.samples=VK_SAMPLE_COUNT_1_BIT;
        cad.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR; cad.storeOp=VK_ATTACHMENT_STORE_OP_STORE;
        cad.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED; cad.finalLayout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        VkAttachmentReference car={0,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        VkSubpassDescription csp={}; csp.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS;
        csp.colorAttachmentCount=1; csp.pColorAttachments=&car;
        VkRenderPassCreateInfo crpi={VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
        crpi.attachmentCount=1; crpi.pAttachments=&cad; crpi.subpassCount=1; crpi.pSubpasses=&csp;
        vkCreateRenderPass(GN_Vk.device,&crpi,0,&GN_Vk.compositeRenderPass);}
        GN_Vk.compositeFramebuffers=new VkFramebuffer[GN_Vk.swapchainImageCount];
        for(uint32_t i=0;i<GN_Vk.swapchainImageCount;i++){
            VkFramebufferCreateInfo fci={VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
            fci.renderPass=GN_Vk.compositeRenderPass; fci.attachmentCount=1;
            fci.pAttachments=&GN_Vk.swapchainViews[i]; fci.width=sw; fci.height=sh; fci.layers=1;
            vkCreateFramebuffer(GN_Vk.device,&fci,0,&GN_Vk.compositeFramebuffers[i]);}

        VkGraphicsPipelineCreateInfo pci={VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
        pci.stageCount=2; pci.pStages=ss; pci.pVertexInputState=&vis;
        pci.pInputAssemblyState=&ias; pci.pViewportState=&vps;
        pci.pRasterizationState=&rs; pci.pMultisampleState=&ms;
        pci.pColorBlendState=&cbs; pci.layout=GN_Vk.compositePipelineLayout;
        pci.renderPass=GN_Vk.compositeRenderPass; pci.subpass=0;
        vkCreateGraphicsPipelines(GN_Vk.device,0,1,&pci,0,&GN_Vk.compositePipeline);
        vkDestroyShaderModule(GN_Vk.device,csvm,0); vkDestroyShaderModule(GN_Vk.device,csfm,0);

        // 分配描述符集并绑定 HDR 纹理
        {VkDescriptorSetAllocateInfo dai={VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        dai.descriptorPool=GN_Vk.descriptorPool; dai.descriptorSetCount=1;
        dai.pSetLayouts=&GN_Vk.compositeDescriptorLayout;
        vkAllocateDescriptorSets(GN_Vk.device,&dai,&GN_Vk.compositeDescriptorSet);
        VkDescriptorImageInfo hii={GN_Vk.hdrSampler,GN_Vk.hdrView,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        VkWriteDescriptorSet wc={VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        wc.dstSet=GN_Vk.compositeDescriptorSet; wc.dstBinding=1;
        wc.descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; wc.descriptorCount=1;
        wc.pImageInfo=&hii;
        vkUpdateDescriptorSets(GN_Vk.device,1,&wc,0,0);}
    }

    // ═══════════════════════════════════════════════════════
    //  天空盒 Cubemap (6面, 256×256, 程序化渐变天空)
    // ═══════════════════════════════════════════════════════
    {
        const int smSize = 256;
        // 创建 cubemap image
        {VkImageCreateInfo ii={VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        ii.imageType=VK_IMAGE_TYPE_2D; ii.extent={(uint32_t)smSize,(uint32_t)smSize,1};
        ii.mipLevels=1; ii.arrayLayers=6;
        ii.format=VK_FORMAT_R8G8B8A8_UNORM;
        ii.tiling=VK_IMAGE_TILING_OPTIMAL; ii.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;
        ii.usage=VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT;
        ii.samples=VK_SAMPLE_COUNT_1_BIT; ii.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
        ii.flags=VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        vkCreateImage(GN_Vk.device,&ii,0,&GN_Vk.skyboxImage);
        VkMemoryRequirements mr; vkGetImageMemoryRequirements(GN_Vk.device,GN_Vk.skyboxImage,&mr);
        VkMemoryAllocateInfo mai={VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        mai.allocationSize=mr.size; mai.memoryTypeIndex=FindMemType(mr.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        vkAllocateMemory(GN_Vk.device,&mai,0,&GN_Vk.skyboxMemory);
        vkBindImageMemory(GN_Vk.device,GN_Vk.skyboxImage,GN_Vk.skyboxMemory,0);}
        // Image view (cubemap)
        {VkImageViewCreateInfo vi={VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        vi.image=GN_Vk.skyboxImage; vi.viewType=VK_IMAGE_VIEW_TYPE_CUBE;
        vi.format=VK_FORMAT_R8G8B8A8_UNORM;
        vi.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;
        vi.subresourceRange.levelCount=1; vi.subresourceRange.layerCount=6;
        vkCreateImageView(GN_Vk.device,&vi,0,&GN_Vk.skyboxView);}
        // Sampler (linear, clamp)
        {VkSamplerCreateInfo sci={VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        sci.magFilter=VK_FILTER_LINEAR; sci.minFilter=VK_FILTER_LINEAR;
        sci.addressModeU=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sci.addressModeV=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sci.addressModeW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        vkCreateSampler(GN_Vk.device,&sci,0,&GN_Vk.skyboxSampler);}

        // 生成程序化天空渐变 (每面 256×256)
        uint32_t faceSize = smSize * smSize * 4;
        uint32_t totalSize = faceSize * 6;
        uint8_t *pixels = new uint8_t[totalSize];
        // +X, -X, +Y, -Y, +Z, -Z (Vulkan cubemap 标准顺序)
        for(int face=0; face<6; face++){
            uint8_t *dst = pixels + face * faceSize;
            float faceH; // 每个面的"上方向"在天空渐变中的位置
            for(int y=0; y<smSize; y++){
                float fy = (float)y / (float)(smSize-1); // 0=底部, 1=顶部
                for(int x=0; x<smSize; x++){
                    int idx = (y*smSize + x)*4;
                    // 天空渐变: 顶部深蓝 → 底部浅蓝白
                    float t = fy;
                    float r = 0.35f + 0.25f * t + 0.35f * (1.0f-t);
                    float g = 0.45f + 0.25f * t + 0.50f * (1.0f-t);
                    float b = 0.60f + 0.30f * t + 0.60f * (1.0f-t);
                    // 近地平线加亮
                    if(fy < 0.3f){ float f = 1.0f - fy/0.3f; r+=0.25f*f; g+=0.20f*f; b+=0.15f*f; }
                    // 顶部加深
                    if(fy > 0.85f){ float f = (fy-0.85f)/0.15f; r-=0.05f*f; g-=0.05f*f; b+=0.10f*f; }
                    if(r>1.0f)r=1.0f; if(g>1.0f)g=1.0f; if(b>1.0f)b=1.0f;
                    dst[idx+0]=(uint8_t)(r*255);
                    dst[idx+1]=(uint8_t)(g*255);
                    dst[idx+2]=(uint8_t)(b*255);
                    dst[idx+3]=255;
                }
            }
        }
        // 上传到 GPU
        {VkBuffer staging; VkDeviceMemory stagingMem;
        CreateBuf(totalSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                  staging, stagingMem);
        void *d; vkMapMemory(GN_Vk.device,stagingMem,0,totalSize,0,&d);
        memcpy(d,pixels,totalSize); vkUnmapMemory(GN_Vk.device,stagingMem);
        delete[] pixels;

        VkCommandBuffer cb;
        {VkCommandBufferAllocateInfo ai={VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        ai.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY; ai.commandPool=GN_Vk.commandPool; ai.commandBufferCount=1;
        vkAllocateCommandBuffers(GN_Vk.device,&ai,&cb);}
        {VkCommandBufferBeginInfo bi={VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        bi.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT; vkBeginCommandBuffer(cb,&bi);}
        // 过渡 UNDEFINED → TRANSFER_DST
        {VkImageMemoryBarrier b={VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        b.oldLayout=VK_IMAGE_LAYOUT_UNDEFINED; b.newLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        b.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED; b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
        b.image=GN_Vk.skyboxImage; b.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;
        b.subresourceRange.levelCount=1; b.subresourceRange.layerCount=6;
        b.srcAccessMask=0; b.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;
        vkCmdPipelineBarrier(cb,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,0,0,0,1,&b);}
        // 拷贝 6 个面
        VkBufferImageCopy regions[6]={};
        for(int f=0; f<6; f++){
            regions[f].bufferOffset = f * faceSize;
            regions[f].imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            regions[f].imageSubresource.mipLevel = 0;
            regions[f].imageSubresource.baseArrayLayer = (uint32_t)f;
            regions[f].imageSubresource.layerCount = 1;
            regions[f].imageExtent = {(uint32_t)smSize, (uint32_t)smSize, 1};
        }
        vkCmdCopyBufferToImage(cb,staging,GN_Vk.skyboxImage,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,6,regions);
        // 过渡 TRANSFER_DST → SHADER_READ_ONLY
        {VkImageMemoryBarrier b={VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        b.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL; b.newLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        b.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED; b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
        b.image=GN_Vk.skyboxImage; b.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;
        b.subresourceRange.levelCount=1; b.subresourceRange.layerCount=6;
        b.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT; b.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(cb,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,0,0,0,0,0,1,&b);}
        vkEndCommandBuffer(cb);
        {VkSubmitInfo si={VK_STRUCTURE_TYPE_SUBMIT_INFO}; si.commandBufferCount=1;
        si.pCommandBuffers=&cb; vkQueueSubmit(GN_Vk.graphicsQueue,1,&si,VK_NULL_HANDLE);
        vkQueueWaitIdle(GN_Vk.graphicsQueue);}
        vkFreeCommandBuffers(GN_Vk.device,GN_Vk.commandPool,1,&cb);
        vkDestroyBuffer(GN_Vk.device,staging,0); vkFreeMemory(GN_Vk.device,stagingMem,0);}
    }

    // ── PBR 描述符集 Binding 5 → 天空盒 Cubemap (IBL) ──
    if(GN_Vk.skyboxView && GN_Vk.skyboxSampler && GN_Vk.descriptorSetPBR){
        VkDescriptorImageInfo sii={GN_Vk.skyboxSampler,GN_Vk.skyboxView,
                                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        VkWriteDescriptorSet ws={VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        ws.dstSet=GN_Vk.descriptorSetPBR; ws.dstBinding=5;
        ws.descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; ws.descriptorCount=1;
        ws.pImageInfo=&sii;
        vkUpdateDescriptorSets(GN_Vk.device,1,&ws,0,0);
    }

    // ── 编译天空盒着色器 + 创建管线 ──
    {
        auto svSpv = LoadSPIRVFile("assets/shaders/gn3d_skybox.vert.spv");
        auto sfSpv = LoadSPIRVFile("assets/shaders/gn3d_skybox.frag.spv");
        VkShaderModule svm=0, sfm=0;
        if(!svSpv.empty() && !sfSpv.empty()){
            {VkShaderModuleCreateInfo sm={VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
            sm.codeSize=svSpv.size()*sizeof(uint32_t); sm.pCode=svSpv.data();
            vkCreateShaderModule(GN_Vk.device,&sm,0,&svm);}
            {VkShaderModuleCreateInfo sm={VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
            sm.codeSize=sfSpv.size()*sizeof(uint32_t); sm.pCode=sfSpv.data();
            vkCreateShaderModule(GN_Vk.device,&sm,0,&sfm);}
        }
        if(svm && sfm){
            // 描述符布局: Binding 0 = UBO (invProj+invView), Binding 1 = Cubemap
            VkDescriptorSetLayoutBinding dslb[2]={};
            dslb[0].binding=0; dslb[0].descriptorType=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            dslb[0].descriptorCount=1; dslb[0].stageFlags=VK_SHADER_STAGE_VERTEX_BIT;
            dslb[1].binding=1; dslb[1].descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            dslb[1].descriptorCount=1; dslb[1].stageFlags=VK_SHADER_STAGE_FRAGMENT_BIT;
            VkDescriptorSetLayoutCreateInfo dslci={VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
            dslci.bindingCount=2; dslci.pBindings=dslb;
            vkCreateDescriptorSetLayout(GN_Vk.device,&dslci,0,&GN_Vk.skyboxDescriptorLayout);

            VkPipelineLayoutCreateInfo plci={VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
            plci.setLayoutCount=1; plci.pSetLayouts=&GN_Vk.skyboxDescriptorLayout;
            vkCreatePipelineLayout(GN_Vk.device,&plci,0,&GN_Vk.skyboxPipelineLayout);

            // UBO (64 bytes: viewProj)
            CreateBuf(64, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                      GN_Vk.skyboxUBOBuffer, GN_Vk.skyboxUBOMemory);

            // Descriptor set
            {VkDescriptorSetAllocateInfo dai={VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
            dai.descriptorPool=GN_Vk.descriptorPool; dai.descriptorSetCount=1;
            dai.pSetLayouts=&GN_Vk.skyboxDescriptorLayout;
            vkAllocateDescriptorSets(GN_Vk.device,&dai,&GN_Vk.skyboxDescriptorSet);
            VkDescriptorBufferInfo dbi={GN_Vk.skyboxUBOBuffer,0,64};
            VkDescriptorImageInfo dii={GN_Vk.skyboxSampler,GN_Vk.skyboxView,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
            VkWriteDescriptorSet w[2]={{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET},{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET}};
            w[0].dstSet=GN_Vk.skyboxDescriptorSet; w[0].dstBinding=0;
            w[0].descriptorType=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; w[0].descriptorCount=1; w[0].pBufferInfo=&dbi;
            w[1].dstSet=GN_Vk.skyboxDescriptorSet; w[1].dstBinding=1;
            w[1].descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; w[1].descriptorCount=1; w[1].pImageInfo=&dii;
            vkUpdateDescriptorSets(GN_Vk.device,2,w,0,0);}

            // Pipeline (全屏三角形, 深度测试=LEQUAL, 深度写入=OFF)
            VkPipelineShaderStageCreateInfo ss[2]={{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO},{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO}};
            ss[0].stage=VK_SHADER_STAGE_VERTEX_BIT; ss[0].module=svm; ss[0].pName="main";
            ss[1].stage=VK_SHADER_STAGE_FRAGMENT_BIT; ss[1].module=sfm; ss[1].pName="main";
            VkPipelineVertexInputStateCreateInfo vis={VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
            VkPipelineInputAssemblyStateCreateInfo ias={VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
            ias.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            VkViewport vp={0,0,(float)sw,(float)sh,0,1};
            VkRect2D sc={{0,0},{sw,sh}};
            VkPipelineViewportStateCreateInfo vps={VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
            vps.viewportCount=1; vps.pViewports=&vp; vps.scissorCount=1; vps.pScissors=&sc;
            VkPipelineRasterizationStateCreateInfo rs={VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
            rs.polygonMode=VK_POLYGON_MODE_FILL; rs.cullMode=VK_CULL_MODE_NONE; rs.lineWidth=1.0f;
            rs.frontFace=VK_FRONT_FACE_CLOCKWISE;
            VkPipelineMultisampleStateCreateInfo ms={VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
            ms.rasterizationSamples=GN_Vk.msaaSamples;
            VkPipelineDepthStencilStateCreateInfo dss={VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
            dss.depthTestEnable=VK_TRUE; dss.depthWriteEnable=VK_FALSE;
            dss.depthCompareOp=VK_COMPARE_OP_LESS_OR_EQUAL;
            VkPipelineColorBlendAttachmentState cba={}; cba.colorWriteMask=0xf;
            VkPipelineColorBlendStateCreateInfo cbs={VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
            cbs.attachmentCount=1; cbs.pAttachments=&cba;
            VkGraphicsPipelineCreateInfo pci={VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
            pci.stageCount=2; pci.pStages=ss; pci.pVertexInputState=&vis;
            pci.pInputAssemblyState=&ias; pci.pViewportState=&vps;
            pci.pRasterizationState=&rs; pci.pMultisampleState=&ms;
            pci.pDepthStencilState=&dss; pci.pColorBlendState=&cbs;
            pci.layout=GN_Vk.skyboxPipelineLayout;
            pci.renderPass=GN_Vk.renderPass3D; pci.subpass=0;
            vkCreateGraphicsPipelines(GN_Vk.device,0,1,&pci,0,&GN_Vk.skyboxPipeline);
            vkDestroyShaderModule(GN_Vk.device,svm,0); vkDestroyShaderModule(GN_Vk.device,sfm,0);
        }
    }

    // ═══════════════════════════════════════════════════════
    //  延迟渲染 G-Buffer (3 颜色附件 + 深度)
    // ═══════════════════════════════════════════════════════
    {
        // G-Buffer 纹理: albedo (R8G8B8A8), normal+rough (R16G16B16A16), PBR (R8G8B8A8)
        VkFormat gFormats[3] = {VK_FORMAT_R16G16B16A16_SFLOAT, VK_FORMAT_R16G16B16A16_SFLOAT, VK_FORMAT_R16G16B16A16_SFLOAT};
        VkImage* gImages[3] = {&GN_Vk.gAlbedoImage, &GN_Vk.gNormalImage, &GN_Vk.gPBRImage};
        VkDeviceMemory* gMems[3] = {&GN_Vk.gAlbedoMemory, &GN_Vk.gNormalMemory, &GN_Vk.gPBRMemory};
        VkImageView* gViews[3] = {&GN_Vk.gAlbedoView, &GN_Vk.gNormalView, &GN_Vk.gPBRView};
        for(int i=0; i<3; i++){
            VkImageCreateInfo ii={VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
            ii.imageType=VK_IMAGE_TYPE_2D; ii.extent={sw,sh,1}; ii.mipLevels=1; ii.arrayLayers=1;
            ii.format=gFormats[i]; ii.tiling=VK_IMAGE_TILING_OPTIMAL;
            ii.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;
            ii.usage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_SAMPLED_BIT;
            ii.samples=VK_SAMPLE_COUNT_1_BIT; ii.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
            vkCreateImage(GN_Vk.device,&ii,0,gImages[i]);
            VkMemoryRequirements mr; vkGetImageMemoryRequirements(GN_Vk.device,*gImages[i],&mr);
            VkMemoryAllocateInfo mai={VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
            mai.allocationSize=mr.size; mai.memoryTypeIndex=FindMemType(mr.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
            vkAllocateMemory(GN_Vk.device,&mai,0,gMems[i]);
            vkBindImageMemory(GN_Vk.device,*gImages[i],*gMems[i],0);
            VkImageViewCreateInfo vi={VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
            vi.image=*gImages[i]; vi.viewType=VK_IMAGE_VIEW_TYPE_2D; vi.format=gFormats[i];
            vi.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT; vi.subresourceRange.levelCount=1; vi.subresourceRange.layerCount=1;
            vkCreateImageView(GN_Vk.device,&vi,0,gViews[i]);
        }
        // G-Buffer 深度 (1采样)
        {VkImageCreateInfo ii={VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        ii.imageType=VK_IMAGE_TYPE_2D; ii.extent={sw,sh,1}; ii.mipLevels=1; ii.arrayLayers=1;
        ii.format=GN_Vk.depthFormat; ii.tiling=VK_IMAGE_TILING_OPTIMAL;
        ii.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;
        ii.usage=VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT|VK_IMAGE_USAGE_SAMPLED_BIT;
        ii.samples=VK_SAMPLE_COUNT_1_BIT; ii.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
        vkCreateImage(GN_Vk.device,&ii,0,&GN_Vk.gDepthImage);
        VkMemoryRequirements mr; vkGetImageMemoryRequirements(GN_Vk.device,GN_Vk.gDepthImage,&mr);
        VkMemoryAllocateInfo mai={VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        mai.allocationSize=mr.size; mai.memoryTypeIndex=FindMemType(mr.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        vkAllocateMemory(GN_Vk.device,&mai,0,&GN_Vk.gDepthMemory);
        vkBindImageMemory(GN_Vk.device,GN_Vk.gDepthImage,GN_Vk.gDepthMemory,0);
        VkImageViewCreateInfo vi={VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        vi.image=GN_Vk.gDepthImage; vi.viewType=VK_IMAGE_VIEW_TYPE_2D; vi.format=GN_Vk.depthFormat;
        vi.subresourceRange.aspectMask=VK_IMAGE_ASPECT_DEPTH_BIT; vi.subresourceRange.levelCount=1; vi.subresourceRange.layerCount=1;
        vkCreateImageView(GN_Vk.device,&vi,0,&GN_Vk.gDepthView);}
        // G-Buffer 采样器
        {VkSamplerCreateInfo sci={VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        sci.magFilter=VK_FILTER_NEAREST; sci.minFilter=VK_FILTER_NEAREST;
        sci.addressModeU=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sci.addressModeV=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        vkCreateSampler(GN_Vk.device,&sci,0,&GN_Vk.gBufferSampler);}

        // G-Buffer RenderPass (4 附件, 非MSAA)
        {VkAttachmentDescription ads[4]={};
        ads[0].format=gFormats[0]; ads[0].samples=VK_SAMPLE_COUNT_1_BIT;
        ads[0].loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR; ads[0].storeOp=VK_ATTACHMENT_STORE_OP_STORE;
        ads[0].initialLayout=VK_IMAGE_LAYOUT_UNDEFINED; ads[0].finalLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        ads[1].format=gFormats[1]; ads[1].samples=VK_SAMPLE_COUNT_1_BIT;
        ads[1].loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR; ads[1].storeOp=VK_ATTACHMENT_STORE_OP_STORE;
        ads[1].initialLayout=VK_IMAGE_LAYOUT_UNDEFINED; ads[1].finalLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        ads[2].format=gFormats[2]; ads[2].samples=VK_SAMPLE_COUNT_1_BIT;
        ads[2].loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR; ads[2].storeOp=VK_ATTACHMENT_STORE_OP_STORE;
        ads[2].initialLayout=VK_IMAGE_LAYOUT_UNDEFINED; ads[2].finalLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        ads[3].format=GN_Vk.depthFormat; ads[3].samples=VK_SAMPLE_COUNT_1_BIT;
        ads[3].loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR; ads[3].storeOp=VK_ATTACHMENT_STORE_OP_STORE;
        ads[3].initialLayout=VK_IMAGE_LAYOUT_UNDEFINED; ads[3].finalLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        VkAttachmentReference crs[3]={{0,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},{1,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},{2,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL}};
        VkAttachmentReference dr={3,VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
        VkSubpassDescription sp={}; sp.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS;
        sp.colorAttachmentCount=3; sp.pColorAttachments=crs; sp.pDepthStencilAttachment=&dr;
        VkRenderPassCreateInfo rpi={VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
        rpi.attachmentCount=4; rpi.pAttachments=ads; rpi.subpassCount=1; rpi.pSubpasses=&sp;
        vkCreateRenderPass(GN_Vk.device,&rpi,0,&GN_Vk.gBufferRenderPass);}
        GN_Vk.gBufferFramebuffers=new VkFramebuffer[1];
        {VkImageView atts[4]={GN_Vk.gAlbedoView,GN_Vk.gNormalView,GN_Vk.gPBRView,GN_Vk.gDepthView};
        VkFramebufferCreateInfo fci={VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        fci.renderPass=GN_Vk.gBufferRenderPass; fci.attachmentCount=4; fci.pAttachments=atts;
        fci.width=sw; fci.height=sh; fci.layers=1;
        VkResult fr = vkCreateFramebuffer(GN_Vk.device,&fci,0,&GN_Vk.gBufferFramebuffers[0]);
        VkCheck(fr, "gBufferFramebuffer");}
    }

    // ── G-Buffer 着色器 + 管线 ──
    {auto gvSpv=LoadSPIRVFile("assets/shaders/gn3d_gbuffer.vert.spv");
     auto gfSpv=LoadSPIRVFile("assets/shaders/gn3d_gbuffer.frag.spv");
     VkShaderModule gvm=0,gfm=0;
     if(!gvSpv.empty()&&!gfSpv.empty()){
         {VkShaderModuleCreateInfo sm={VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO}; sm.codeSize=gvSpv.size()*4;sm.pCode=gvSpv.data();vkCreateShaderModule(GN_Vk.device,&sm,0,&gvm);}
         {VkShaderModuleCreateInfo sm={VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO}; sm.codeSize=gfSpv.size()*4;sm.pCode=gfSpv.data();vkCreateShaderModule(GN_Vk.device,&sm,0,&gfm);}
     }
     if(gvm&&gfm){
         // 描述符布局: Binding 0=SceneUBO, 1=texSampler, 3=normalSampler (复用PBR部分)
         VkDescriptorSetLayoutBinding dslb[3]={};
         dslb[0].binding=0; dslb[0].descriptorType=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; dslb[0].descriptorCount=1; dslb[0].stageFlags=VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT;
         dslb[1].binding=1; dslb[1].descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; dslb[1].descriptorCount=1; dslb[1].stageFlags=VK_SHADER_STAGE_FRAGMENT_BIT;
         dslb[2].binding=3; dslb[2].descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; dslb[2].descriptorCount=1; dslb[2].stageFlags=VK_SHADER_STAGE_FRAGMENT_BIT;
         VkDescriptorSetLayoutCreateInfo dci={VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO}; dci.bindingCount=3; dci.pBindings=dslb;
         vkCreateDescriptorSetLayout(GN_Vk.device,&dci,0,&GN_Vk.gBufferDescriptorLayout);
         {VkPushConstantRange pcr={}; pcr.stageFlags=VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT; pcr.offset=0; pcr.size=sizeof(VkModelPushConstPBR);
          VkPipelineLayoutCreateInfo pl={VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO}; pl.setLayoutCount=1; pl.pSetLayouts=&GN_Vk.gBufferDescriptorLayout; pl.pushConstantRangeCount=1; pl.pPushConstantRanges=&pcr;
          vkCreatePipelineLayout(GN_Vk.device,&pl,0,&GN_Vk.gBufferPipelineLayout);}
         // Descriptor set
         {VkDescriptorSetAllocateInfo dai={VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO}; dai.descriptorPool=GN_Vk.descriptorPool; dai.descriptorSetCount=1; dai.pSetLayouts=&GN_Vk.gBufferDescriptorLayout;
          vkAllocateDescriptorSets(GN_Vk.device,&dai,&GN_Vk.gBufferDescriptorSet);
          VkDescriptorBufferInfo dbi={GN_Vk.uniformBuffer3D,0,sizeof(VkSceneUBO)};
          VkDescriptorImageInfo dii={GN_Vk.defaultSampler,GN_Vk.defaultImageView,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
          VkWriteDescriptorSet w[3]={{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET},{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET},{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET}};
          w[0].dstSet=GN_Vk.gBufferDescriptorSet; w[0].dstBinding=0; w[0].descriptorType=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; w[0].descriptorCount=1; w[0].pBufferInfo=&dbi;
          w[1].dstSet=GN_Vk.gBufferDescriptorSet; w[1].dstBinding=1; w[1].descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; w[1].descriptorCount=1; w[1].pImageInfo=&dii;
          w[2].dstSet=GN_Vk.gBufferDescriptorSet; w[2].dstBinding=3; w[2].descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; w[2].descriptorCount=1; w[2].pImageInfo=&dii;
          vkUpdateDescriptorSets(GN_Vk.device,3,w,0,0);}
         // Pipeline
         VkPipelineShaderStageCreateInfo ss[2]={{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO},{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO}};
         ss[0].stage=VK_SHADER_STAGE_VERTEX_BIT; ss[0].module=gvm; ss[0].pName="main";
         ss[1].stage=VK_SHADER_STAGE_FRAGMENT_BIT; ss[1].module=gfm; ss[1].pName="main";
         VkVertexInputBindingDescription vib={0,sizeof(VkVertex3D),VK_VERTEX_INPUT_RATE_VERTEX};
         VkVertexInputAttributeDescription via[4]={{0,0,VK_FORMAT_R32G32B32_SFLOAT,offsetof(VkVertex3D,px)},{1,0,VK_FORMAT_R32G32B32_SFLOAT,offsetof(VkVertex3D,nx)},{2,0,VK_FORMAT_R32G32B32_SFLOAT,offsetof(VkVertex3D,tx)},{3,0,VK_FORMAT_R32G32_SFLOAT,offsetof(VkVertex3D,u)}};
         VkPipelineVertexInputStateCreateInfo vis={VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO}; vis.vertexBindingDescriptionCount=1; vis.pVertexBindingDescriptions=&vib; vis.vertexAttributeDescriptionCount=4; vis.pVertexAttributeDescriptions=via;
         VkPipelineInputAssemblyStateCreateInfo ias={VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO}; ias.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
         VkViewport vp={0,0,(float)sw,(float)sh,0,1}; VkRect2D sc={{0,0},{sw,sh}};
         VkPipelineViewportStateCreateInfo vps={VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO}; vps.viewportCount=1; vps.pViewports=&vp; vps.scissorCount=1; vps.pScissors=&sc;
         VkPipelineRasterizationStateCreateInfo rs={VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO}; rs.polygonMode=VK_POLYGON_MODE_FILL; rs.cullMode=VK_CULL_MODE_BACK_BIT; rs.frontFace=VK_FRONT_FACE_CLOCKWISE; rs.lineWidth=1.0f;
         VkPipelineMultisampleStateCreateInfo ms={VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO}; ms.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;
         VkPipelineDepthStencilStateCreateInfo dss={VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO}; dss.depthTestEnable=VK_TRUE; dss.depthWriteEnable=VK_TRUE; dss.depthCompareOp=VK_COMPARE_OP_LESS;
         VkPipelineColorBlendAttachmentState cba[3]={}; for(int i=0;i<3;i++) cba[i].colorWriteMask=0xf;
         VkPipelineColorBlendStateCreateInfo cbs={VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO}; cbs.attachmentCount=3; cbs.pAttachments=cba;
         VkGraphicsPipelineCreateInfo pci={VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO}; pci.stageCount=2; pci.pStages=ss; pci.pVertexInputState=&vis; pci.pInputAssemblyState=&ias; pci.pViewportState=&vps; pci.pRasterizationState=&rs; pci.pMultisampleState=&ms; pci.pDepthStencilState=&dss; pci.pColorBlendState=&cbs; pci.layout=GN_Vk.gBufferPipelineLayout; pci.renderPass=GN_Vk.gBufferRenderPass; pci.subpass=0;
         vkCreateGraphicsPipelines(GN_Vk.device,0,1,&pci,0,&GN_Vk.gBufferPipeline);
         vkDestroyShaderModule(GN_Vk.device,gvm,0); vkDestroyShaderModule(GN_Vk.device,gfm,0);
     }}

    // ── 延迟光照着色器 + 管线 ──
    {auto dlv=LoadSPIRVFile("assets/shaders/gn3d_deferred_light.vert.spv");
     auto dlf=LoadSPIRVFile("assets/shaders/gn3d_deferred_light.frag.spv");
     VkShaderModule dvm=0,dfm=0;
     if(!dlv.empty()&&!dlf.empty()){
         {VkShaderModuleCreateInfo sm={VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO}; sm.codeSize=dlv.size()*4;sm.pCode=dlv.data();vkCreateShaderModule(GN_Vk.device,&sm,0,&dvm);}
         {VkShaderModuleCreateInfo sm={VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO}; sm.codeSize=dlf.size()*4;sm.pCode=dlf.data();vkCreateShaderModule(GN_Vk.device,&sm,0,&dfm);}
     }
     if(dvm&&dfm){
         // 描述符: 0=SceneUBO,1=gAlbedo,2=gNormal,3=gPBR,4=gDepth,5=shadow,6=skybox,7=LightUBO
         VkDescriptorSetLayoutBinding dlb[8]={};
         dlb[0].binding=0; dlb[0].descriptorType=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; dlb[0].descriptorCount=1; dlb[0].stageFlags=VK_SHADER_STAGE_FRAGMENT_BIT;
         for(int i=1;i<=4;i++){dlb[i].binding=(uint32_t)i; dlb[i].descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; dlb[i].descriptorCount=1; dlb[i].stageFlags=VK_SHADER_STAGE_FRAGMENT_BIT;}
         dlb[5].binding=5; dlb[5].descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; dlb[5].descriptorCount=1; dlb[5].stageFlags=VK_SHADER_STAGE_FRAGMENT_BIT;
         dlb[6].binding=6; dlb[6].descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; dlb[6].descriptorCount=1; dlb[6].stageFlags=VK_SHADER_STAGE_FRAGMENT_BIT;
         dlb[7].binding=7; dlb[7].descriptorType=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; dlb[7].descriptorCount=1; dlb[7].stageFlags=VK_SHADER_STAGE_FRAGMENT_BIT;
         VkDescriptorSetLayoutCreateInfo dci={VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO}; dci.bindingCount=8; dci.pBindings=dlb;
         vkCreateDescriptorSetLayout(GN_Vk.device,&dci,0,&GN_Vk.deferredLightDescriptorLayout);
         {VkPipelineLayoutCreateInfo pl={VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO}; pl.setLayoutCount=1; pl.pSetLayouts=&GN_Vk.deferredLightDescriptorLayout;
          vkCreatePipelineLayout(GN_Vk.device,&pl,0,&GN_Vk.deferredLightPipelineLayout);}
         // Descriptor set
         {VkDescriptorSetAllocateInfo dai={VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO}; dai.descriptorPool=GN_Vk.descriptorPool; dai.descriptorSetCount=1; dai.pSetLayouts=&GN_Vk.deferredLightDescriptorLayout;
          vkAllocateDescriptorSets(GN_Vk.device,&dai,&GN_Vk.deferredLightDescriptorSet);
          VkDescriptorBufferInfo dbi0={GN_Vk.uniformBuffer3D,0,sizeof(VkSceneUBO)};
          VkDescriptorBufferInfo dbi7={GN_Vk.lightBuffer3D,0,1024};
          VkDescriptorImageInfo gii={GN_Vk.gBufferSampler,GN_Vk.gAlbedoView,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
          VkDescriptorImageInfo nii={GN_Vk.gBufferSampler,GN_Vk.gNormalView,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
          VkDescriptorImageInfo pii={GN_Vk.gBufferSampler,GN_Vk.gPBRView,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
          VkDescriptorImageInfo dii={GN_Vk.gBufferSampler,GN_Vk.gDepthView,VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL};
          VkDescriptorImageInfo sii={GN_Vk.shadowSampler,GN_Vk.shadowView,VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL};
          VkDescriptorImageInfo cii={GN_Vk.skyboxSampler,GN_Vk.skyboxView,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
          VkWriteDescriptorSet w[8]={};
          for(int i=0;i<8;i++) w[i].sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
          w[0].dstSet=GN_Vk.deferredLightDescriptorSet; w[0].dstBinding=0; w[0].descriptorType=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; w[0].descriptorCount=1; w[0].pBufferInfo=&dbi0;
          w[1].dstSet=GN_Vk.deferredLightDescriptorSet; w[1].dstBinding=1; w[1].descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; w[1].descriptorCount=1; w[1].pImageInfo=&gii;
          w[2].dstSet=GN_Vk.deferredLightDescriptorSet; w[2].dstBinding=2; w[2].descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; w[2].descriptorCount=1; w[2].pImageInfo=&nii;
          w[3].dstSet=GN_Vk.deferredLightDescriptorSet; w[3].dstBinding=3; w[3].descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; w[3].descriptorCount=1; w[3].pImageInfo=&pii;
          w[4].dstSet=GN_Vk.deferredLightDescriptorSet; w[4].dstBinding=4; w[4].descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; w[4].descriptorCount=1; w[4].pImageInfo=&dii;
          w[5].dstSet=GN_Vk.deferredLightDescriptorSet; w[5].dstBinding=5; w[5].descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; w[5].descriptorCount=1; w[5].pImageInfo=&sii;
          w[6].dstSet=GN_Vk.deferredLightDescriptorSet; w[6].dstBinding=6; w[6].descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; w[6].descriptorCount=1; w[6].pImageInfo=&cii;
          w[7].dstSet=GN_Vk.deferredLightDescriptorSet; w[7].dstBinding=7; w[7].descriptorType=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; w[7].descriptorCount=1; w[7].pBufferInfo=&dbi7;
          vkUpdateDescriptorSets(GN_Vk.device,8,w,0,0);}
         // 延迟光照 RenderPass → swapchain (复用 composite render pass 格式)
         {VkAttachmentDescription ad={}; ad.format=GN_Vk.swapchainFormat; ad.samples=VK_SAMPLE_COUNT_1_BIT; ad.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR; ad.storeOp=VK_ATTACHMENT_STORE_OP_STORE; ad.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED; ad.finalLayout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
          VkAttachmentReference ar={0,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
          VkSubpassDescription sp={}; sp.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS; sp.colorAttachmentCount=1; sp.pColorAttachments=&ar;
          VkRenderPassCreateInfo rpi={VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO}; rpi.attachmentCount=1; rpi.pAttachments=&ad; rpi.subpassCount=1; rpi.pSubpasses=&sp;
          vkCreateRenderPass(GN_Vk.device,&rpi,0,&GN_Vk.deferredLightRenderPass);}
         GN_Vk.deferredLightFramebuffers=new VkFramebuffer[GN_Vk.swapchainImageCount];
         for(uint32_t i=0;i<GN_Vk.swapchainImageCount;i++){
             VkFramebufferCreateInfo fci={VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO}; fci.renderPass=GN_Vk.deferredLightRenderPass; fci.attachmentCount=1; fci.pAttachments=&GN_Vk.swapchainViews[i]; fci.width=sw; fci.height=sh; fci.layers=1;
             vkCreateFramebuffer(GN_Vk.device,&fci,0,&GN_Vk.deferredLightFramebuffers[i]);}
         // Pipeline (全屏三角形)
         VkPipelineShaderStageCreateInfo ss[2]={{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO},{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO}};
         ss[0].stage=VK_SHADER_STAGE_VERTEX_BIT; ss[0].module=dvm; ss[0].pName="main"; ss[1].stage=VK_SHADER_STAGE_FRAGMENT_BIT; ss[1].module=dfm; ss[1].pName="main";
         VkPipelineVertexInputStateCreateInfo vis={VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
         VkPipelineInputAssemblyStateCreateInfo ias={VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO}; ias.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
         VkViewport vp={0,0,(float)sw,(float)sh,0,1}; VkRect2D sc={{0,0},{sw,sh}};
         VkPipelineViewportStateCreateInfo vps={VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO}; vps.viewportCount=1; vps.pViewports=&vp; vps.scissorCount=1; vps.pScissors=&sc;
         VkPipelineRasterizationStateCreateInfo rs={VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO}; rs.polygonMode=VK_POLYGON_MODE_FILL; rs.cullMode=VK_CULL_MODE_NONE; rs.lineWidth=1.0f;
         VkPipelineMultisampleStateCreateInfo ms={VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO}; ms.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;
         VkPipelineColorBlendAttachmentState cba={}; cba.colorWriteMask=0xf;
         VkPipelineColorBlendStateCreateInfo cbs={VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO}; cbs.attachmentCount=1; cbs.pAttachments=&cba;
         VkGraphicsPipelineCreateInfo pci={VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO}; pci.stageCount=2; pci.pStages=ss; pci.pVertexInputState=&vis; pci.pInputAssemblyState=&ias; pci.pViewportState=&vps; pci.pRasterizationState=&rs; pci.pMultisampleState=&ms; pci.pColorBlendState=&cbs; pci.layout=GN_Vk.deferredLightPipelineLayout; pci.renderPass=GN_Vk.deferredLightRenderPass; pci.subpass=0;
         vkCreateGraphicsPipelines(GN_Vk.device,0,1,&pci,0,&GN_Vk.deferredLightPipeline);
         vkDestroyShaderModule(GN_Vk.device,dvm,0); vkDestroyShaderModule(GN_Vk.device,dfm,0);
     }}
}

void GN_VkShutdown3D() {
    if(GN_Vk.descriptorSet3D) {GN_Vk.descriptorSet3D=VK_NULL_HANDLE;} // pool freed elsewhere
    if(GN_Vk.descriptorSetPBR) {GN_Vk.descriptorSetPBR=VK_NULL_HANDLE;}
    if(GN_Vk.uniformBuffer3D) {vkDestroyBuffer(GN_Vk.device,GN_Vk.uniformBuffer3D,0); GN_Vk.uniformBuffer3D=VK_NULL_HANDLE;}
    if(GN_Vk.uniformMemory3D) {vkFreeMemory(GN_Vk.device,GN_Vk.uniformMemory3D,0); GN_Vk.uniformMemory3D=VK_NULL_HANDLE;}
    if(GN_Vk.lightBuffer3D) {vkDestroyBuffer(GN_Vk.device,GN_Vk.lightBuffer3D,0); GN_Vk.lightBuffer3D=VK_NULL_HANDLE;}
    if(GN_Vk.lightMemory3D) {vkFreeMemory(GN_Vk.device,GN_Vk.lightMemory3D,0); GN_Vk.lightMemory3D=VK_NULL_HANDLE;}
    // PBR
    if(GN_Vk.pipelinePBR) {vkDestroyPipeline(GN_Vk.device,GN_Vk.pipelinePBR,0); GN_Vk.pipelinePBR=VK_NULL_HANDLE;}
    if(GN_Vk.pipelineLayoutPBR) {vkDestroyPipelineLayout(GN_Vk.device,GN_Vk.pipelineLayoutPBR,0); GN_Vk.pipelineLayoutPBR=VK_NULL_HANDLE;}
    if(GN_Vk.descriptorLayoutPBR) {vkDestroyDescriptorSetLayout(GN_Vk.device,GN_Vk.descriptorLayoutPBR,0); GN_Vk.descriptorLayoutPBR=VK_NULL_HANDLE;}
    // Shadow
    if(GN_Vk.shadowPipeline) {vkDestroyPipeline(GN_Vk.device,GN_Vk.shadowPipeline,0); GN_Vk.shadowPipeline=VK_NULL_HANDLE;}
    if(GN_Vk.shadowPipelineLayout) {vkDestroyPipelineLayout(GN_Vk.device,GN_Vk.shadowPipelineLayout,0); GN_Vk.shadowPipelineLayout=VK_NULL_HANDLE;}
    if(GN_Vk.shadowDescriptorLayout) {vkDestroyDescriptorSetLayout(GN_Vk.device,GN_Vk.shadowDescriptorLayout,0); GN_Vk.shadowDescriptorLayout=VK_NULL_HANDLE;}
    if(GN_Vk.shadowUBOBuffer) {vkDestroyBuffer(GN_Vk.device,GN_Vk.shadowUBOBuffer,0); GN_Vk.shadowUBOBuffer=VK_NULL_HANDLE;}
    if(GN_Vk.shadowUBOMemory) {vkFreeMemory(GN_Vk.device,GN_Vk.shadowUBOMemory,0); GN_Vk.shadowUBOMemory=VK_NULL_HANDLE;}
    if(GN_Vk.shadowFramebuffer) {vkDestroyFramebuffer(GN_Vk.device,GN_Vk.shadowFramebuffer,0); GN_Vk.shadowFramebuffer=VK_NULL_HANDLE;}
    if(GN_Vk.shadowRenderPass) {vkDestroyRenderPass(GN_Vk.device,GN_Vk.shadowRenderPass,0); GN_Vk.shadowRenderPass=VK_NULL_HANDLE;}
    if(GN_Vk.shadowSampler) {vkDestroySampler(GN_Vk.device,GN_Vk.shadowSampler,0); GN_Vk.shadowSampler=VK_NULL_HANDLE;}
    if(GN_Vk.shadowView) {vkDestroyImageView(GN_Vk.device,GN_Vk.shadowView,0); GN_Vk.shadowView=VK_NULL_HANDLE;}
    if(GN_Vk.shadowImage) {vkDestroyImage(GN_Vk.device,GN_Vk.shadowImage,0); GN_Vk.shadowImage=VK_NULL_HANDLE;}
    if(GN_Vk.shadowMemory) {vkFreeMemory(GN_Vk.device,GN_Vk.shadowMemory,0); GN_Vk.shadowMemory=VK_NULL_HANDLE;}
    if(GN_Vk.pipeline3D) {vkDestroyPipeline(GN_Vk.device,GN_Vk.pipeline3D,0); GN_Vk.pipeline3D=VK_NULL_HANDLE;}
    if(GN_Vk.pipelineLayout3D) {vkDestroyPipelineLayout(GN_Vk.device,GN_Vk.pipelineLayout3D,0); GN_Vk.pipelineLayout3D=VK_NULL_HANDLE;}
    if(GN_Vk.descriptorLayout3D) {vkDestroyDescriptorSetLayout(GN_Vk.device,GN_Vk.descriptorLayout3D,0); GN_Vk.descriptorLayout3D=VK_NULL_HANDLE;}
    if(s_framebuffers3D){
        for(uint32_t i=0;i<GN_Vk.swapchainImageCount;i++) if(s_framebuffers3D[i]) vkDestroyFramebuffer(GN_Vk.device,s_framebuffers3D[i],0);
        delete[] s_framebuffers3D; s_framebuffers3D=nullptr;
    }
    if(GN_Vk.renderPass3D) {vkDestroyRenderPass(GN_Vk.device,GN_Vk.renderPass3D,0); GN_Vk.renderPass3D=VK_NULL_HANDLE;}
    if(GN_Vk.depthView) {vkDestroyImageView(GN_Vk.device,GN_Vk.depthView,0); GN_Vk.depthView=VK_NULL_HANDLE;}
    if(GN_Vk.depthImage) {vkDestroyImage(GN_Vk.device,GN_Vk.depthImage,0); GN_Vk.depthImage=VK_NULL_HANDLE;}
    if(GN_Vk.depthMemory) {vkFreeMemory(GN_Vk.device,GN_Vk.depthMemory,0); GN_Vk.depthMemory=VK_NULL_HANDLE;}
    // MSAA
    if(GN_Vk.msaaColorView) {vkDestroyImageView(GN_Vk.device,GN_Vk.msaaColorView,0); GN_Vk.msaaColorView=VK_NULL_HANDLE;}
    if(GN_Vk.msaaColorImage) {vkDestroyImage(GN_Vk.device,GN_Vk.msaaColorImage,0); GN_Vk.msaaColorImage=VK_NULL_HANDLE;}
    if(GN_Vk.msaaColorMemory) {vkFreeMemory(GN_Vk.device,GN_Vk.msaaColorMemory,0); GN_Vk.msaaColorMemory=VK_NULL_HANDLE;}
    // HDR + Composite
    if(GN_Vk.compositeDescriptorSet) {GN_Vk.compositeDescriptorSet=VK_NULL_HANDLE;}
    if(GN_Vk.compositePipeline) {vkDestroyPipeline(GN_Vk.device,GN_Vk.compositePipeline,0); GN_Vk.compositePipeline=VK_NULL_HANDLE;}
    if(GN_Vk.compositePipelineLayout) {vkDestroyPipelineLayout(GN_Vk.device,GN_Vk.compositePipelineLayout,0); GN_Vk.compositePipelineLayout=VK_NULL_HANDLE;}
    if(GN_Vk.compositeDescriptorLayout) {vkDestroyDescriptorSetLayout(GN_Vk.device,GN_Vk.compositeDescriptorLayout,0); GN_Vk.compositeDescriptorLayout=VK_NULL_HANDLE;}
    if(GN_Vk.compositeFramebuffers){for(uint32_t i=0;i<GN_Vk.swapchainImageCount;i++) if(GN_Vk.compositeFramebuffers[i]) vkDestroyFramebuffer(GN_Vk.device,GN_Vk.compositeFramebuffers[i],0); delete[] GN_Vk.compositeFramebuffers; GN_Vk.compositeFramebuffers=nullptr;}
    if(GN_Vk.compositeRenderPass) {vkDestroyRenderPass(GN_Vk.device,GN_Vk.compositeRenderPass,0); GN_Vk.compositeRenderPass=VK_NULL_HANDLE;}
    if(GN_Vk.hdrSampler) {vkDestroySampler(GN_Vk.device,GN_Vk.hdrSampler,0); GN_Vk.hdrSampler=VK_NULL_HANDLE;}
    if(GN_Vk.hdrView) {vkDestroyImageView(GN_Vk.device,GN_Vk.hdrView,0); GN_Vk.hdrView=VK_NULL_HANDLE;}
    if(GN_Vk.hdrImage) {vkDestroyImage(GN_Vk.device,GN_Vk.hdrImage,0); GN_Vk.hdrImage=VK_NULL_HANDLE;}
    if(GN_Vk.hdrMemory) {vkFreeMemory(GN_Vk.device,GN_Vk.hdrMemory,0); GN_Vk.hdrMemory=VK_NULL_HANDLE;}
    if(GN_Vk.hdrFramebuffers){for(uint32_t i=0;i<GN_Vk.swapchainImageCount;i++) if(GN_Vk.hdrFramebuffers[i]) vkDestroyFramebuffer(GN_Vk.device,GN_Vk.hdrFramebuffers[i],0); delete[] GN_Vk.hdrFramebuffers; GN_Vk.hdrFramebuffers=nullptr;}
    if(GN_Vk.hdrRenderPass) {vkDestroyRenderPass(GN_Vk.device,GN_Vk.hdrRenderPass,0); GN_Vk.hdrRenderPass=VK_NULL_HANDLE;}
    // Skybox
    if(GN_Vk.skyboxPipeline) {vkDestroyPipeline(GN_Vk.device,GN_Vk.skyboxPipeline,0); GN_Vk.skyboxPipeline=VK_NULL_HANDLE;}
    if(GN_Vk.skyboxPipelineLayout) {vkDestroyPipelineLayout(GN_Vk.device,GN_Vk.skyboxPipelineLayout,0); GN_Vk.skyboxPipelineLayout=VK_NULL_HANDLE;}
    if(GN_Vk.skyboxDescriptorLayout) {vkDestroyDescriptorSetLayout(GN_Vk.device,GN_Vk.skyboxDescriptorLayout,0); GN_Vk.skyboxDescriptorLayout=VK_NULL_HANDLE;}
    if(GN_Vk.skyboxUBOBuffer) {vkDestroyBuffer(GN_Vk.device,GN_Vk.skyboxUBOBuffer,0); GN_Vk.skyboxUBOBuffer=VK_NULL_HANDLE;}
    if(GN_Vk.skyboxUBOMemory) {vkFreeMemory(GN_Vk.device,GN_Vk.skyboxUBOMemory,0); GN_Vk.skyboxUBOMemory=VK_NULL_HANDLE;}
    if(GN_Vk.skyboxSampler) {vkDestroySampler(GN_Vk.device,GN_Vk.skyboxSampler,0); GN_Vk.skyboxSampler=VK_NULL_HANDLE;}
    if(GN_Vk.skyboxView) {vkDestroyImageView(GN_Vk.device,GN_Vk.skyboxView,0); GN_Vk.skyboxView=VK_NULL_HANDLE;}
    if(GN_Vk.skyboxImage) {vkDestroyImage(GN_Vk.device,GN_Vk.skyboxImage,0); GN_Vk.skyboxImage=VK_NULL_HANDLE;}
    if(GN_Vk.skyboxMemory) {vkFreeMemory(GN_Vk.device,GN_Vk.skyboxMemory,0); GN_Vk.skyboxMemory=VK_NULL_HANDLE;}
    // Deferred
    if(GN_Vk.deferredLightPipeline){vkDestroyPipeline(GN_Vk.device,GN_Vk.deferredLightPipeline,0);}
    if(GN_Vk.deferredLightPipelineLayout){vkDestroyPipelineLayout(GN_Vk.device,GN_Vk.deferredLightPipelineLayout,0);}
    if(GN_Vk.deferredLightDescriptorLayout){vkDestroyDescriptorSetLayout(GN_Vk.device,GN_Vk.deferredLightDescriptorLayout,0);}
    if(GN_Vk.deferredLightFramebuffers){for(uint32_t i=0;i<GN_Vk.swapchainImageCount;i++)if(GN_Vk.deferredLightFramebuffers[i])vkDestroyFramebuffer(GN_Vk.device,GN_Vk.deferredLightFramebuffers[i],0);delete[] GN_Vk.deferredLightFramebuffers;}
    if(GN_Vk.deferredLightRenderPass){vkDestroyRenderPass(GN_Vk.device,GN_Vk.deferredLightRenderPass,0);}
    if(GN_Vk.gBufferPipeline){vkDestroyPipeline(GN_Vk.device,GN_Vk.gBufferPipeline,0);}
    if(GN_Vk.gBufferPipelineLayout){vkDestroyPipelineLayout(GN_Vk.device,GN_Vk.gBufferPipelineLayout,0);}
    if(GN_Vk.gBufferDescriptorLayout){vkDestroyDescriptorSetLayout(GN_Vk.device,GN_Vk.gBufferDescriptorLayout,0);}
    if(GN_Vk.gBufferSampler){vkDestroySampler(GN_Vk.device,GN_Vk.gBufferSampler,0);}
    if(GN_Vk.gBufferFramebuffers){delete[] GN_Vk.gBufferFramebuffers;}
    if(GN_Vk.gBufferRenderPass){vkDestroyRenderPass(GN_Vk.device,GN_Vk.gBufferRenderPass,0);}
    {VkImageView* vs[]={&GN_Vk.gAlbedoView,&GN_Vk.gNormalView,&GN_Vk.gPBRView,&GN_Vk.gDepthView};
     VkImage* ims[]={&GN_Vk.gAlbedoImage,&GN_Vk.gNormalImage,&GN_Vk.gPBRImage,&GN_Vk.gDepthImage};
     VkDeviceMemory* ms[]={&GN_Vk.gAlbedoMemory,&GN_Vk.gNormalMemory,&GN_Vk.gPBRMemory,&GN_Vk.gDepthMemory};
     for(int i=0;i<4;i++){if(*vs[i]){vkDestroyImageView(GN_Vk.device,*vs[i],0);*vs[i]=VK_NULL_HANDLE;} if(*ims[i]){vkDestroyImage(GN_Vk.device,*ims[i],0);*ims[i]=VK_NULL_HANDLE;} if(*ms[i]){vkFreeMemory(GN_Vk.device,*ms[i],0);*ms[i]=VK_NULL_HANDLE;}}}
}

void GN_VkBegin3DPass(const sMAT44 *viewProj, const sVEC3 *cameraPos,
                      const void *lightUBO, uint32_t lightUBOSize) {
    if(!GN_Vk.initialized || s_in3DPass) return;

    // 刷新当前 2D 批次
    if(gVCnt > 0) {
        void *d; vkMapMemory(GN_Vk.device,GN_Vk.vertexMemory,0,gVCnt*sizeof(VkVertex),0,&d);
        memcpy(d,gVBuf,gVCnt*sizeof(VkVertex)); vkUnmapMemory(GN_Vk.device,GN_Vk.vertexMemory);
        uint32_t prevEnd=0;
        for(uint32_t i=0;i<s_batchCount;i++){
            uint32_t count=s_batchEnd[i]-prevEnd;
            if(count>0){
                vkCmdBindDescriptorSets(GN_Vk.commandBuffers[GN_Vk.currentFrame],VK_PIPELINE_BIND_POINT_GRAPHICS,GN_Vk.pipelineLayout,0,1,&s_batchDS[i],0,0);
                VkDeviceSize offs[]={prevEnd*sizeof(VkVertex)};
                vkCmdBindVertexBuffers(GN_Vk.commandBuffers[GN_Vk.currentFrame],0,1,&GN_Vk.vertexBuffer,offs);
                vkCmdDraw(GN_Vk.commandBuffers[GN_Vk.currentFrame],count,1,0,0);
            }
            prevEnd=s_batchEnd[i];
        }
        gVCnt=0; s_batchCount=0; s_lastDS=VK_NULL_HANDLE;
    }

    // 结束当前渲染通道 (2D 或 空)
    if(s_inRenderPass){
        vkCmdEndRenderPass(GN_Vk.commandBuffers[GN_Vk.currentFrame]);
        s_inRenderPass = false;
    }

    // 更新 3D 场景 UBO
    {VkSceneUBO ubo;
    memcpy(ubo.viewProj, viewProj->m, sizeof(ubo.viewProj));
    ubo.cameraPos[0]=cameraPos->x; ubo.cameraPos[1]=cameraPos->y;
    ubo.cameraPos[2]=cameraPos->z; ubo.cameraPos[3]=0.0f;
    // 从 lightUBO 读取环境光 (若未提供则使用默认值)
    if(lightUBO && lightUBOSize >= sizeof(GN3d::sLIGHT_UBO)){
        const GN3d::sLIGHT_UBO *lubo = (const GN3d::sLIGHT_UBO*)lightUBO;
        ubo.ambientLight[0]=lubo->ambientLight[0];
        ubo.ambientLight[1]=lubo->ambientLight[1];
        ubo.ambientLight[2]=lubo->ambientLight[2];
        ubo.ambientLight[3]=lubo->ambientLight[3];
    }else{
        ubo.ambientLight[0]=0.15f; ubo.ambientLight[1]=0.15f;
        ubo.ambientLight[2]=0.2f;  ubo.ambientLight[3]=1.0f;
    }
    void *data; vkMapMemory(GN_Vk.device,GN_Vk.uniformMemory3D,0,sizeof(ubo),0,&data);
    memcpy(data,&ubo,sizeof(ubo)); vkUnmapMemory(GN_Vk.device,GN_Vk.uniformMemory3D);}

    // 上传光照 UBO
    if(lightUBO && lightUBOSize > 0){
        void *ld; vkMapMemory(GN_Vk.device,GN_Vk.lightMemory3D,0,lightUBOSize,0,&ld);
        memcpy(ld,lightUBO,lightUBOSize); vkUnmapMemory(GN_Vk.device,GN_Vk.lightMemory3D);
    }

    // 开始 3D render pass
    {VkClearValue cvs[3];
    cvs[0].color={{0.05f,0.05f,0.1f,1.0f}};  // MSAA color — 深蓝背景
    cvs[1].depthStencil={1.0f, 0};             // MSAA depth
    cvs[2].color={{0.0f,0.0f,0.0f,1.0f}};      // Resolve — 会被覆盖
    VkRenderPassBeginInfo rpi={VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    rpi.renderPass=GN_Vk.renderPass3D; rpi.framebuffer=s_framebuffers3D[GN_Vk.currentFrame];
    rpi.renderArea={{0,0},GN_Vk.swapchainExtent}; rpi.clearValueCount=3; rpi.pClearValues=cvs;
    vkCmdBeginRenderPass(GN_Vk.commandBuffers[GN_Vk.currentFrame],&rpi,VK_SUBPASS_CONTENTS_INLINE);}

    vkCmdBindPipeline(GN_Vk.commandBuffers[GN_Vk.currentFrame],VK_PIPELINE_BIND_POINT_GRAPHICS,GN_Vk.pipeline3D);
    vkCmdBindDescriptorSets(GN_Vk.commandBuffers[GN_Vk.currentFrame],VK_PIPELINE_BIND_POINT_GRAPHICS,
        GN_Vk.pipelineLayout3D,0,1,&GN_Vk.descriptorSet3D,0,0);

    s_in3DPass = true;
    s_inRenderPass = true;  // 3D 渲染通道活跃
}

void GN_VkEnd3DPass() {
    if(!GN_Vk.initialized || !s_in3DPass) return;

    vkCmdEndRenderPass(GN_Vk.commandBuffers[GN_Vk.currentFrame]);

    s_in3DPass = false;
    s_inRenderPass = false;  // 3D pass 结束，不重启 2D pass（避免 CLEAR 清掉 3D 内容）
}

void GN_VkDraw3D(const void *vertices, uint32_t vertexCount,
                 const uint32_t *indices, uint32_t indexCount,
                 const sMAT44 *modelMatrix,
                 float diffuseR, float diffuseG, float diffuseB,
                 float specularR, float specularG, float specularB,
                 float shininess, const sIMAGE *texture) {
    if(!GN_Vk.initialized || !s_in3DPass || !vertices || vertexCount==0) return;

    // ── 更新纹理描述符 (binding 1) ──
    // 有纹理→用纹理, 无纹理→用默认白色 (与 diffuse 相乘无影响)
    {
        VkImageView imgView = (texture && texture->vkImageView)
                              ? texture->vkImageView : GN_Vk.defaultImageView;
        VkSampler   sampler = (texture && texture->vkSampler)
                              ? texture->vkSampler   : GN_Vk.defaultSampler;

        VkDescriptorImageInfo dii = {};
        dii.sampler     = sampler;
        dii.imageView   = imgView;
        dii.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkWriteDescriptorSet w = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        w.dstSet          = GN_Vk.descriptorSet3D;
        w.dstBinding      = 1;
        w.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        w.descriptorCount = 1;
        w.pImageInfo      = &dii;
        vkUpdateDescriptorSets(GN_Vk.device, 1, &w, 0, nullptr);

        // 重新绑定描述符集以确保更新生效
        vkCmdBindDescriptorSets(GN_Vk.commandBuffers[GN_Vk.currentFrame],
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            GN_Vk.pipelineLayout3D, 0, 1, &GN_Vk.descriptorSet3D, 0, nullptr);
    }

    // 上传顶点数据 (动态缓冲, 需要时扩容)
    uint32_t vtxSize = vertexCount * sizeof(VkVertex3D);
    uint32_t idxSize = (indices && indexCount > 0) ? indexCount * sizeof(uint32_t) : 0;
    Ensure3DBufferCapacity(vtxSize, idxSize);

    // 写入顶点
    void *d; vkMapMemory(GN_Vk.device, GN_Vk.vertexMemory3D, 0, vtxSize, 0, &d);
    memcpy(d, vertices, vtxSize);
    vkUnmapMemory(GN_Vk.device, GN_Vk.vertexMemory3D);

    // 绑定管线 (自给自足, 兼容 skybox 等中间管线切换)
    vkCmdBindPipeline(GN_Vk.commandBuffers[GN_Vk.currentFrame],
        VK_PIPELINE_BIND_POINT_GRAPHICS, GN_Vk.pipeline3D);

    VkDeviceSize offs[] = {0};
    vkCmdBindVertexBuffers(GN_Vk.commandBuffers[GN_Vk.currentFrame], 0, 1,
                           &GN_Vk.vertexBuffer3D, offs);

    // Push constants (Blinn-Phong 材质)
    VkModelPushConstBP pc;
    memcpy(pc.model, modelMatrix->m, 64);
    pc.diffuse[0] = diffuseR;  pc.diffuse[1] = diffuseG;
    pc.diffuse[2] = diffuseB;  pc.diffuse[3] = 1.0f;
    pc.specular[0] = specularR; pc.specular[1] = specularG;
    pc.specular[2] = specularB; pc.specular[3] = shininess;
    vkCmdPushConstants(GN_Vk.commandBuffers[GN_Vk.currentFrame],
                       GN_Vk.pipelineLayout3D,
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(VkModelPushConstBP), &pc);

    if (indices && indexCount > 0) {
        // 写入索引
        void *id; vkMapMemory(GN_Vk.device, GN_Vk.indexMemory3D, 0, idxSize, 0, &id);
        memcpy(id, indices, idxSize);
        vkUnmapMemory(GN_Vk.device, GN_Vk.indexMemory3D);

        vkCmdBindIndexBuffer(GN_Vk.commandBuffers[GN_Vk.currentFrame],
                             GN_Vk.indexBuffer3D, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(GN_Vk.commandBuffers[GN_Vk.currentFrame], indexCount, 1, 0, 0, 0);
    } else {
        vkCmdDraw(GN_Vk.commandBuffers[GN_Vk.currentFrame], vertexCount, 1, 0, 0);
    }
}

void GN_VkDraw3DPBR(const void *vertices, uint32_t vertexCount,
                    const uint32_t *indices, uint32_t indexCount,
                    const sMAT44 *modelMatrix,
                    float diffuseR, float diffuseG, float diffuseB,
                    float roughness, float metallic, float ao,
                    const sIMAGE *albedoTex, const sIMAGE *normalTex) {
    if(!GN_Vk.initialized || !s_in3DPass || !vertices || vertexCount==0) return;

    // ── 更新纹理描述符 (binding 1 = albedo, binding 3 = normal) ──
    {
        VkImageView albedoView = (albedoTex && albedoTex->vkImageView)
                                 ? albedoTex->vkImageView : GN_Vk.defaultImageView;
        VkSampler   albedoSamp = (albedoTex && albedoTex->vkSampler)
                                 ? albedoTex->vkSampler   : GN_Vk.defaultSampler;

        VkImageView normalView = (normalTex && normalTex->vkImageView)
                                 ? normalTex->vkImageView : GN_Vk.defaultImageView;
        VkSampler   normalSamp = (normalTex && normalTex->vkSampler)
                                 ? normalTex->vkSampler   : GN_Vk.defaultSampler;

        VkDescriptorImageInfo diiAlbedo = {};
        diiAlbedo.sampler     = albedoSamp;
        diiAlbedo.imageView   = albedoView;
        diiAlbedo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkDescriptorImageInfo diiNormal = {};
        diiNormal.sampler     = normalSamp;
        diiNormal.imageView   = normalView;
        diiNormal.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkWriteDescriptorSet w[2] = {
            {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET},
            {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET}
        };
        w[0].dstSet          = GN_Vk.descriptorSetPBR;
        w[0].dstBinding      = 1;
        w[0].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        w[0].descriptorCount = 1;
        w[0].pImageInfo      = &diiAlbedo;

        w[1].dstSet          = GN_Vk.descriptorSetPBR;
        w[1].dstBinding      = 3;
        w[1].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        w[1].descriptorCount = 1;
        w[1].pImageInfo      = &diiNormal;

        vkUpdateDescriptorSets(GN_Vk.device, 2, w, 0, nullptr);

        // 重新绑定描述符集
        vkCmdBindDescriptorSets(GN_Vk.commandBuffers[GN_Vk.currentFrame],
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            GN_Vk.pipelineLayoutPBR, 0, 1, &GN_Vk.descriptorSetPBR, 0, nullptr);
    }

    // Push model matrix + PBR material (albedo/roughness/metallic/ao)
    VkModelPushConstPBR pc;
    memcpy(pc.model, modelMatrix->m, sizeof(pc.model));
    pc.albedo[0] = diffuseR; pc.albedo[1] = diffuseG;
    pc.albedo[2] = diffuseB; pc.albedo[3] = 1.0f;
    pc.pbrParams[0] = roughness; pc.pbrParams[1] = metallic;
    pc.pbrParams[2] = ao;        pc.pbrParams[3] = 0.0f;
    vkCmdPushConstants(GN_Vk.commandBuffers[GN_Vk.currentFrame],
        GN_Vk.pipelineLayoutPBR,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        0, sizeof(VkModelPushConstPBR), &pc);

    // 上传顶点数据 (动态扩容, shared buffers)
    uint32_t vtxSize = vertexCount * sizeof(VkVertex3D);
    uint32_t idxSize = (indices && indexCount > 0) ? indexCount * sizeof(uint32_t) : 0;
    Ensure3DBufferCapacity(vtxSize, idxSize);

    void *d; vkMapMemory(GN_Vk.device, GN_Vk.vertexMemory3D, 0, vtxSize, 0, &d);
    memcpy(d, vertices, vtxSize);
    vkUnmapMemory(GN_Vk.device, GN_Vk.vertexMemory3D);

    // 绑定 PBR 管线
    vkCmdBindPipeline(GN_Vk.commandBuffers[GN_Vk.currentFrame],
        VK_PIPELINE_BIND_POINT_GRAPHICS, GN_Vk.pipelinePBR);

    VkDeviceSize offs[] = {0};
    vkCmdBindVertexBuffers(GN_Vk.commandBuffers[GN_Vk.currentFrame], 0, 1,
                           &GN_Vk.vertexBuffer3D, offs);

    if (indices && indexCount > 0) {
        void *id; vkMapMemory(GN_Vk.device, GN_Vk.indexMemory3D, 0, idxSize, 0, &id);
        memcpy(id, indices, idxSize);
        vkUnmapMemory(GN_Vk.device, GN_Vk.indexMemory3D);

        vkCmdBindIndexBuffer(GN_Vk.commandBuffers[GN_Vk.currentFrame],
                             GN_Vk.indexBuffer3D, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(GN_Vk.commandBuffers[GN_Vk.currentFrame], indexCount, 1, 0, 0, 0);
    } else {
        vkCmdDraw(GN_Vk.commandBuffers[GN_Vk.currentFrame], vertexCount, 1, 0, 0);
    }
}

void GN_VkUpload3DMesh(VkBuffer *outVB, VkDeviceMemory *outVM,
                       VkBuffer *outIB, VkDeviceMemory *outIM,
                       const void *vertices, uint32_t vertexCount, uint32_t vertexStride,
                       const uint32_t *indices, uint32_t indexCount) {
    if(!GN_Vk.initialized) return;

    if(vertices && vertexCount>0){
        VkDeviceSize sz = (VkDeviceSize)vertexCount * vertexStride;
        VkBuffer staging; VkDeviceMemory stagingMem;
        CreateBuf(sz, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                  staging, stagingMem);
        void *d; vkMapMemory(GN_Vk.device,stagingMem,0,sz,0,&d);
        memcpy(d,vertices,sz); vkUnmapMemory(GN_Vk.device,stagingMem);

        CreateBuf(sz, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, *outVB, *outVM);

        VkCommandBuffer cb;
        {VkCommandBufferAllocateInfo ai={VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        ai.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY; ai.commandPool=GN_Vk.commandPool; ai.commandBufferCount=1;
        vkAllocateCommandBuffers(GN_Vk.device,&ai,&cb);}
        {VkCommandBufferBeginInfo bi={VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        bi.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT; vkBeginCommandBuffer(cb,&bi);}
        {VkBufferCopy rgn={}; rgn.size=sz; vkCmdCopyBuffer(cb,staging,*outVB,1,&rgn);}
        vkEndCommandBuffer(cb);
        {VkSubmitInfo si={VK_STRUCTURE_TYPE_SUBMIT_INFO}; si.commandBufferCount=1;
        si.pCommandBuffers=&cb; vkQueueSubmit(GN_Vk.graphicsQueue,1,&si,VK_NULL_HANDLE);
        vkQueueWaitIdle(GN_Vk.graphicsQueue);}
        vkFreeCommandBuffers(GN_Vk.device,GN_Vk.commandPool,1,&cb);
        vkDestroyBuffer(GN_Vk.device,staging,0); vkFreeMemory(GN_Vk.device,stagingMem,0);
    }

    if(indices && indexCount>0){
        VkDeviceSize sz = indexCount * sizeof(uint32_t);
        VkBuffer staging; VkDeviceMemory stagingMem;
        CreateBuf(sz, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                  staging, stagingMem);
        void *d; vkMapMemory(GN_Vk.device,stagingMem,0,sz,0,&d);
        memcpy(d,indices,sz); vkUnmapMemory(GN_Vk.device,stagingMem);

        CreateBuf(sz, VK_BUFFER_USAGE_INDEX_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, *outIB, *outIM);

        VkCommandBuffer cb;
        {VkCommandBufferAllocateInfo ai={VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        ai.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY; ai.commandPool=GN_Vk.commandPool; ai.commandBufferCount=1;
        vkAllocateCommandBuffers(GN_Vk.device,&ai,&cb);}
        {VkCommandBufferBeginInfo bi={VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        bi.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT; vkBeginCommandBuffer(cb,&bi);}
        {VkBufferCopy rgn={}; rgn.size=sz; vkCmdCopyBuffer(cb,staging,*outIB,1,&rgn);}
        vkEndCommandBuffer(cb);
        {VkSubmitInfo si={VK_STRUCTURE_TYPE_SUBMIT_INFO}; si.commandBufferCount=1;
        si.pCommandBuffers=&cb; vkQueueSubmit(GN_Vk.graphicsQueue,1,&si,VK_NULL_HANDLE);
        vkQueueWaitIdle(GN_Vk.graphicsQueue);}
        vkFreeCommandBuffers(GN_Vk.device,GN_Vk.commandPool,1,&cb);
        vkDestroyBuffer(GN_Vk.device,staging,0); vkFreeMemory(GN_Vk.device,stagingMem,0);
    }
}

void GN_VkRelease3DMesh(VkBuffer vb, VkDeviceMemory vm,
                        VkBuffer ib, VkDeviceMemory im) {
    if(!GN_Vk.initialized) return;
    if(vb) {vkDestroyBuffer(GN_Vk.device,vb,0);}
    if(vm) {vkFreeMemory(GN_Vk.device,vm,0);}
    if(ib) {vkDestroyBuffer(GN_Vk.device,ib,0);}
    if(im) {vkFreeMemory(GN_Vk.device,im,0);}
}

// ═══════════════════════════════════════════════════════════
//  PBR 渲染 (使用 PBR 管线)
// ═══════════════════════════════════════════════════════════
void GN_VkBegin3DPassPBR(const sMAT44 *viewProj, const sVEC3 *cameraPos,
                         const void *lightUBO, uint32_t lightUBOSize){
    if(!GN_Vk.initialized || s_in3DPass) return;
    // 刷新 2D 批次 (与 GN_VkBegin3DPass 相同逻辑)
    if(gVCnt > 0) {
        void *d; vkMapMemory(GN_Vk.device,GN_Vk.vertexMemory,0,gVCnt*sizeof(VkVertex),0,&d);
        memcpy(d,gVBuf,gVCnt*sizeof(VkVertex)); vkUnmapMemory(GN_Vk.device,GN_Vk.vertexMemory);
        uint32_t prevEnd=0;
        for(uint32_t i=0;i<s_batchCount;i++){
            uint32_t count=s_batchEnd[i]-prevEnd;
            if(count>0){
                vkCmdBindDescriptorSets(GN_Vk.commandBuffers[GN_Vk.currentFrame],VK_PIPELINE_BIND_POINT_GRAPHICS,GN_Vk.pipelineLayout,0,1,&s_batchDS[i],0,0);
                VkDeviceSize offs[]={prevEnd*sizeof(VkVertex)};
                vkCmdBindVertexBuffers(GN_Vk.commandBuffers[GN_Vk.currentFrame],0,1,&GN_Vk.vertexBuffer,offs);
                vkCmdDraw(GN_Vk.commandBuffers[GN_Vk.currentFrame],count,1,0,0);
            }
            prevEnd=s_batchEnd[i];
        }
        gVCnt=0; s_batchCount=0; s_lastDS=VK_NULL_HANDLE;
    }
    // 结束当前渲染通道 (2D 或 空 — 阴影 Pass 后可能已无活跃通道)
    if(s_inRenderPass){
        vkCmdEndRenderPass(GN_Vk.commandBuffers[GN_Vk.currentFrame]);
        s_inRenderPass = false;
    }

    // 更新场景 UBO
    {VkSceneUBO ubo;
    memcpy(ubo.viewProj, viewProj->m, sizeof(ubo.viewProj));
    ubo.cameraPos[0]=cameraPos->x; ubo.cameraPos[1]=cameraPos->y;
    ubo.cameraPos[2]=cameraPos->z; ubo.cameraPos[3]=0.0f;
    if(lightUBO && lightUBOSize >= sizeof(GN3d::sLIGHT_UBO)){
        const GN3d::sLIGHT_UBO *lubo = (const GN3d::sLIGHT_UBO*)lightUBO;
        ubo.ambientLight[0]=lubo->ambientLight[0]; ubo.ambientLight[1]=lubo->ambientLight[1];
        ubo.ambientLight[2]=lubo->ambientLight[2]; ubo.ambientLight[3]=lubo->ambientLight[3];
    }else{
        ubo.ambientLight[0]=0.15f; ubo.ambientLight[1]=0.15f; ubo.ambientLight[2]=0.2f; ubo.ambientLight[3]=1.0f;
    }
    void *data; vkMapMemory(GN_Vk.device,GN_Vk.uniformMemory3D,0,sizeof(ubo),0,&data);
    memcpy(data,&ubo,sizeof(ubo)); vkUnmapMemory(GN_Vk.device,GN_Vk.uniformMemory3D);}
    if(lightUBO && lightUBOSize>0){
        void *ld; vkMapMemory(GN_Vk.device,GN_Vk.lightMemory3D,0,lightUBOSize,0,&ld);
        memcpy(ld,lightUBO,lightUBOSize); vkUnmapMemory(GN_Vk.device,GN_Vk.lightMemory3D);
    }
    // 开始 3D render pass (PBR)
    {VkClearValue cvs[3];
    cvs[0].color={{0.0f,0.0f,0.0f,1.0f}}; cvs[1].depthStencil={1.0f,0};
    cvs[2].color={{0.0f,0.0f,0.0f,1.0f}};  // resolve clear
    VkRenderPassBeginInfo rpi={VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    rpi.renderPass=GN_Vk.renderPass3D; rpi.framebuffer=s_framebuffers3D[GN_Vk.currentFrame];
    rpi.renderArea={{0,0},GN_Vk.swapchainExtent}; rpi.clearValueCount=3; rpi.pClearValues=cvs;
    vkCmdBeginRenderPass(GN_Vk.commandBuffers[GN_Vk.currentFrame],&rpi,VK_SUBPASS_CONTENTS_INLINE);}
    // 使用 PBR 管线
    vkCmdBindPipeline(GN_Vk.commandBuffers[GN_Vk.currentFrame],VK_PIPELINE_BIND_POINT_GRAPHICS,GN_Vk.pipelinePBR);
    // 绑定 PBR 描述符集 (含 normal sampler + shadow sampler)
    vkCmdBindDescriptorSets(GN_Vk.commandBuffers[GN_Vk.currentFrame],VK_PIPELINE_BIND_POINT_GRAPHICS,
        GN_Vk.pipelineLayoutPBR,0,1,&GN_Vk.descriptorSetPBR,0,0);
    s_in3DPass = true;
    s_inRenderPass = true;  // 3D 渲染通道活跃
}

// ═══════════════════════════════════════════════════════════
//  3D PBR Pass — HDR 变体 (渲染到 HDR 目标, 供后续合成)
// ═══════════════════════════════════════════════════════════
void GN_VkBegin3DPassPBR_HDR(const sMAT44 *viewProj, const sVEC3 *cameraPos,
                              const void *lightUBO, uint32_t lightUBOSize){
    if(!GN_Vk.initialized || s_in3DPass) return;
    if(gVCnt > 0) {
        void *d; vkMapMemory(GN_Vk.device,GN_Vk.vertexMemory,0,gVCnt*sizeof(VkVertex),0,&d);
        memcpy(d,gVBuf,gVCnt*sizeof(VkVertex)); vkUnmapMemory(GN_Vk.device,GN_Vk.vertexMemory);
        uint32_t prevEnd=0;
        for(uint32_t i=0;i<s_batchCount;i++){
            uint32_t count=s_batchEnd[i]-prevEnd;
            if(count>0){
                vkCmdBindDescriptorSets(GN_Vk.commandBuffers[GN_Vk.currentFrame],VK_PIPELINE_BIND_POINT_GRAPHICS,GN_Vk.pipelineLayout,0,1,&s_batchDS[i],0,0);
                VkDeviceSize offs[]={prevEnd*sizeof(VkVertex)};
                vkCmdBindVertexBuffers(GN_Vk.commandBuffers[GN_Vk.currentFrame],0,1,&GN_Vk.vertexBuffer,offs);
                vkCmdDraw(GN_Vk.commandBuffers[GN_Vk.currentFrame],count,1,0,0);
            }
            prevEnd=s_batchEnd[i];
        }
        gVCnt=0; s_batchCount=0; s_lastDS=VK_NULL_HANDLE;
    }
    if(s_inRenderPass){
        vkCmdEndRenderPass(GN_Vk.commandBuffers[GN_Vk.currentFrame]);
        s_inRenderPass = false;
    }
    {VkSceneUBO ubo;
    memcpy(ubo.viewProj, viewProj->m, sizeof(ubo.viewProj));
    ubo.cameraPos[0]=cameraPos->x; ubo.cameraPos[1]=cameraPos->y;
    ubo.cameraPos[2]=cameraPos->z; ubo.cameraPos[3]=0.0f;
    if(lightUBO && lightUBOSize >= sizeof(GN3d::sLIGHT_UBO)){
        const GN3d::sLIGHT_UBO *lubo = (const GN3d::sLIGHT_UBO*)lightUBO;
        ubo.ambientLight[0]=lubo->ambientLight[0]; ubo.ambientLight[1]=lubo->ambientLight[1];
        ubo.ambientLight[2]=lubo->ambientLight[2]; ubo.ambientLight[3]=lubo->ambientLight[3];
    }else{
        ubo.ambientLight[0]=0.15f; ubo.ambientLight[1]=0.15f; ubo.ambientLight[2]=0.2f; ubo.ambientLight[3]=1.0f;
    }
    void *data; vkMapMemory(GN_Vk.device,GN_Vk.uniformMemory3D,0,sizeof(ubo),0,&data);
    memcpy(data,&ubo,sizeof(ubo)); vkUnmapMemory(GN_Vk.device,GN_Vk.uniformMemory3D);}
    if(lightUBO && lightUBOSize>0){
        void *ld; vkMapMemory(GN_Vk.device,GN_Vk.lightMemory3D,0,lightUBOSize,0,&ld);
        memcpy(ld,lightUBO,lightUBOSize); vkUnmapMemory(GN_Vk.device,GN_Vk.lightMemory3D);
    }
    {VkClearValue cvs[3];
    cvs[0].color={{0.0f,0.0f,0.0f,1.0f}}; cvs[1].depthStencil={1.0f,0};
    cvs[2].color={{0.0f,0.0f,0.0f,1.0f}};
    VkRenderPassBeginInfo rpi={VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    rpi.renderPass=GN_Vk.hdrRenderPass; rpi.framebuffer=GN_Vk.hdrFramebuffers[GN_Vk.currentFrame];
    rpi.renderArea={{0,0},GN_Vk.swapchainExtent}; rpi.clearValueCount=3; rpi.pClearValues=cvs;
    vkCmdBeginRenderPass(GN_Vk.commandBuffers[GN_Vk.currentFrame],&rpi,VK_SUBPASS_CONTENTS_INLINE);}
    vkCmdBindPipeline(GN_Vk.commandBuffers[GN_Vk.currentFrame],VK_PIPELINE_BIND_POINT_GRAPHICS,GN_Vk.pipelinePBR);
    vkCmdBindDescriptorSets(GN_Vk.commandBuffers[GN_Vk.currentFrame],VK_PIPELINE_BIND_POINT_GRAPHICS,
        GN_Vk.pipelineLayoutPBR,0,1,&GN_Vk.descriptorSetPBR,0,0);
    s_in3DPass = true;
    s_inRenderPass = true;
}

// ═══════════════════════════════════════════════════════════
//  阴影映射 Pass
// ═══════════════════════════════════════════════════════════

void GN_VkShadowPassBegin(const sMAT44 *lightViewProj){
    if(!GN_Vk.initialized || !GN_Vk.shadowPipeline || s_inShadowPass) return;
    // 必须在 3D pass 之外调用
    if(s_in3DPass) return;

    // 刷新 2D 批次并结束当前渲染通道
    if(gVCnt > 0) {
        void *d; vkMapMemory(GN_Vk.device,GN_Vk.vertexMemory,0,gVCnt*sizeof(VkVertex),0,&d);
        memcpy(d,gVBuf,gVCnt*sizeof(VkVertex)); vkUnmapMemory(GN_Vk.device,GN_Vk.vertexMemory);
        uint32_t prevEnd=0;
        for(uint32_t i=0;i<s_batchCount;i++){
            uint32_t count=s_batchEnd[i]-prevEnd;
            if(count>0){
                vkCmdBindDescriptorSets(GN_Vk.commandBuffers[GN_Vk.currentFrame],VK_PIPELINE_BIND_POINT_GRAPHICS,GN_Vk.pipelineLayout,0,1,&s_batchDS[i],0,0);
                VkDeviceSize offs[]={prevEnd*sizeof(VkVertex)};
                vkCmdBindVertexBuffers(GN_Vk.commandBuffers[GN_Vk.currentFrame],0,1,&GN_Vk.vertexBuffer,offs);
                vkCmdDraw(GN_Vk.commandBuffers[GN_Vk.currentFrame],count,1,0,0);
            }
            prevEnd=s_batchEnd[i];
        }
        gVCnt=0; s_batchCount=0; s_lastDS=VK_NULL_HANDLE;
    }
    if(s_inRenderPass){
        vkCmdEndRenderPass(GN_Vk.commandBuffers[GN_Vk.currentFrame]);
        s_inRenderPass = false;
    }

    // 上传 light VP
    {void *d; vkMapMemory(GN_Vk.device,GN_Vk.shadowUBOMemory,0,64,0,&d);
    memcpy(d, lightViewProj->m, 64); vkUnmapMemory(GN_Vk.device,GN_Vk.shadowUBOMemory);}

    VkClearValue cv; cv.depthStencil={1.0f,0};
    VkRenderPassBeginInfo rpi={VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    rpi.renderPass=GN_Vk.shadowRenderPass; rpi.framebuffer=GN_Vk.shadowFramebuffer;
    rpi.renderArea={{0,0},{GN_Vk.shadowMapSize,GN_Vk.shadowMapSize}};
    rpi.clearValueCount=1; rpi.pClearValues=&cv;
    vkCmdBeginRenderPass(GN_Vk.commandBuffers[GN_Vk.currentFrame],&rpi,VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(GN_Vk.commandBuffers[GN_Vk.currentFrame],VK_PIPELINE_BIND_POINT_GRAPHICS,GN_Vk.shadowPipeline);
    vkCmdBindDescriptorSets(GN_Vk.commandBuffers[GN_Vk.currentFrame],VK_PIPELINE_BIND_POINT_GRAPHICS,
        GN_Vk.shadowPipelineLayout,0,1,&GN_Vk.shadowDescriptorSet,0,0);
    s_inShadowPass = true;
    s_inRenderPass = true;  // 阴影渲染通道活跃
}

void GN_VkShadowPassEnd(){
    if(!GN_Vk.initialized || !s_inShadowPass) return;
    vkCmdEndRenderPass(GN_Vk.commandBuffers[GN_Vk.currentFrame]);
    s_inShadowPass = false;
    s_inRenderPass = false;  // 阴影渲染通道结束，无活跃通道
}

void GN_VkShadowDraw(const void *vertices, uint32_t vertexCount,
                     const uint32_t *indices, uint32_t indexCount,
                     const sMAT44 *modelMatrix){
    if(!GN_Vk.initialized || !s_inShadowPass || !vertices || vertexCount==0) return;

    // Push model matrix
    vkCmdPushConstants(GN_Vk.commandBuffers[GN_Vk.currentFrame],
        GN_Vk.shadowPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, 64, modelMatrix->m);

    // 上传顶点 (动态扩容, shared buffers)
    uint32_t vtxSize = vertexCount * sizeof(VkVertex3D);
    uint32_t idxSize = (indices && indexCount > 0) ? indexCount * sizeof(uint32_t) : 0;
    Ensure3DBufferCapacity(vtxSize, idxSize);

    void *d; vkMapMemory(GN_Vk.device, GN_Vk.vertexMemory3D, 0, vtxSize, 0, &d);
    memcpy(d, vertices, vtxSize);
    vkUnmapMemory(GN_Vk.device, GN_Vk.vertexMemory3D);

    // 绑定阴影管线
    vkCmdBindPipeline(GN_Vk.commandBuffers[GN_Vk.currentFrame],
        VK_PIPELINE_BIND_POINT_GRAPHICS, GN_Vk.shadowPipeline);

    VkDeviceSize offs[] = {0};
    vkCmdBindVertexBuffers(GN_Vk.commandBuffers[GN_Vk.currentFrame], 0, 1,
                           &GN_Vk.vertexBuffer3D, offs);

    if (indices && indexCount > 0) {
        void *id; vkMapMemory(GN_Vk.device, GN_Vk.indexMemory3D, 0, idxSize, 0, &id);
        memcpy(id, indices, idxSize);
        vkUnmapMemory(GN_Vk.device, GN_Vk.indexMemory3D);

        vkCmdBindIndexBuffer(GN_Vk.commandBuffers[GN_Vk.currentFrame],
                             GN_Vk.indexBuffer3D, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(GN_Vk.commandBuffers[GN_Vk.currentFrame], indexCount, 1, 0, 0, 0);
    } else {
        vkCmdDraw(GN_Vk.commandBuffers[GN_Vk.currentFrame], vertexCount, 1, 0, 0);
    }
}

// ═══════════════════════════════════════════════════════════
//  HDR→Swapchain 合成 (Bloom + 色调映射)
//  必须在 GN_VkEnd3DPass() 之后调用 (3D 已结束, HDR 纹理就绪)
// ═══════════════════════════════════════════════════════════
void GN_VkCompositeToSwapchain() {
    if(!GN_Vk.initialized || !GN_Vk.compositePipeline) return;
    if(!GN_Vk.hdrView || !GN_Vk.hdrSampler) return;

    // 更新 composite descriptor: 绑定当前 HDR 纹理
    VkDescriptorImageInfo hii={};
    hii.sampler     = GN_Vk.hdrSampler;
    hii.imageView   = GN_Vk.hdrView;
    hii.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    VkWriteDescriptorSet wc={VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    wc.dstSet          = GN_Vk.compositeDescriptorSet;
    wc.dstBinding      = 1;
    wc.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    wc.descriptorCount = 1;
    wc.pImageInfo      = &hii;
    vkUpdateDescriptorSets(GN_Vk.device, 1, &wc, 0, nullptr);

    // 开始合成 pass → swapchain
    VkClearValue cv; cv.color={{0.0f,0.0f,0.0f,1.0f}};
    VkRenderPassBeginInfo rpi={VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    rpi.renderPass=GN_Vk.compositeRenderPass;
    rpi.framebuffer=GN_Vk.compositeFramebuffers[GN_Vk.currentFrame];
    rpi.renderArea={{0,0},GN_Vk.swapchainExtent};
    rpi.clearValueCount=1; rpi.pClearValues=&cv;
    vkCmdBeginRenderPass(GN_Vk.commandBuffers[GN_Vk.currentFrame],&rpi,VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(GN_Vk.commandBuffers[GN_Vk.currentFrame],VK_PIPELINE_BIND_POINT_GRAPHICS,GN_Vk.compositePipeline);
    vkCmdBindDescriptorSets(GN_Vk.commandBuffers[GN_Vk.currentFrame],VK_PIPELINE_BIND_POINT_GRAPHICS,
        GN_Vk.compositePipelineLayout,0,1,&GN_Vk.compositeDescriptorSet,0,0);

    // 全屏三角形 (3 顶点, 无缓冲)
    vkCmdDraw(GN_Vk.commandBuffers[GN_Vk.currentFrame], 3, 1, 0, 0);

    vkCmdEndRenderPass(GN_Vk.commandBuffers[GN_Vk.currentFrame]);
    s_inRenderPass = false;
}

// ═══════════════════════════════════════════════════════════
//  天空盒渲染
//  必须在一个活跃的 RenderPass 中调用 (在 Clear 之后, 3D 绘制之前)
// ═══════════════════════════════════════════════════════════
void GN_VkDrawSkybox(const sMAT44 *viewProj){
    if(!GN_Vk.initialized || !GN_Vk.skyboxPipeline || !s_in3DPass) return;

    // 上传 viewProj
    void *d; vkMapMemory(GN_Vk.device, GN_Vk.skyboxUBOMemory, 0, 64, 0, &d);
    memcpy(d, viewProj->m, 64);
    vkUnmapMemory(GN_Vk.device, GN_Vk.skyboxUBOMemory);

    vkCmdBindPipeline(GN_Vk.commandBuffers[GN_Vk.currentFrame],
        VK_PIPELINE_BIND_POINT_GRAPHICS, GN_Vk.skyboxPipeline);
    vkCmdBindDescriptorSets(GN_Vk.commandBuffers[GN_Vk.currentFrame],
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        GN_Vk.skyboxPipelineLayout, 0, 1, &GN_Vk.skyboxDescriptorSet, 0, 0);
    // 36 顶点 (单位立方体), 无缓冲
    vkCmdDraw(GN_Vk.commandBuffers[GN_Vk.currentFrame], 36, 1, 0, 0);
}

// ═══════════════════════════════════════════════════════════
//  延迟渲染: G-Buffer Pass
// ═══════════════════════════════════════════════════════════
void GN_VkBegin3DPassDeferred(const sMAT44 *viewProj, const sVEC3 *cameraPos,
                               const void *lightUBO, uint32_t lightUBOSize){
    if(!GN_Vk.initialized || s_in3DPass || !GN_Vk.gBufferPipeline) return;
    // 刷新 2D 批次
    if(gVCnt > 0) {
        void *d; vkMapMemory(GN_Vk.device,GN_Vk.vertexMemory,0,gVCnt*sizeof(VkVertex),0,&d);
        memcpy(d,gVBuf,gVCnt*sizeof(VkVertex)); vkUnmapMemory(GN_Vk.device,GN_Vk.vertexMemory);
        uint32_t prevEnd=0;
        for(uint32_t i=0;i<s_batchCount;i++){
            uint32_t count=s_batchEnd[i]-prevEnd;
            if(count>0){ vkCmdDraw(GN_Vk.commandBuffers[GN_Vk.currentFrame],count,1,0,0); }
            prevEnd=s_batchEnd[i];
        }
        gVCnt=0; s_batchCount=0; s_lastDS=VK_NULL_HANDLE;
    }
    if(s_inRenderPass){vkCmdEndRenderPass(GN_Vk.commandBuffers[GN_Vk.currentFrame]); s_inRenderPass=false;}
    // 上传 SceneUBO
    {VkSceneUBO ubo; memcpy(ubo.viewProj,viewProj->m,64); ubo.cameraPos[0]=cameraPos->x; ubo.cameraPos[1]=cameraPos->y; ubo.cameraPos[2]=cameraPos->z; ubo.cameraPos[3]=0; void *d; vkMapMemory(GN_Vk.device,GN_Vk.uniformMemory3D,0,sizeof(ubo),0,&d); memcpy(d,&ubo,sizeof(ubo)); vkUnmapMemory(GN_Vk.device,GN_Vk.uniformMemory3D);}
    (void)lightUBO; (void)lightUBOSize;
    // 开始 G-Buffer RenderPass
    {VkClearValue cvs[4]; cvs[0].color={{0,0,0,1}}; cvs[1].color={{0,0,0,1}}; cvs[2].color={{0,0,0,1}}; cvs[3].depthStencil={1,0};
    VkRenderPassBeginInfo rpi={VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO}; rpi.renderPass=GN_Vk.gBufferRenderPass; rpi.framebuffer=GN_Vk.gBufferFramebuffers[0]; rpi.renderArea={{0,0},GN_Vk.swapchainExtent}; rpi.clearValueCount=4; rpi.pClearValues=cvs;
    vkCmdBeginRenderPass(GN_Vk.commandBuffers[GN_Vk.currentFrame],&rpi,VK_SUBPASS_CONTENTS_INLINE);}
    vkCmdBindPipeline(GN_Vk.commandBuffers[GN_Vk.currentFrame],VK_PIPELINE_BIND_POINT_GRAPHICS,GN_Vk.gBufferPipeline);
    vkCmdBindDescriptorSets(GN_Vk.commandBuffers[GN_Vk.currentFrame],VK_PIPELINE_BIND_POINT_GRAPHICS,GN_Vk.gBufferPipelineLayout,0,1,&GN_Vk.gBufferDescriptorSet,0,0);
    s_in3DPass=true; s_inRenderPass=true;
}

// ═══════════════════════════════════════════════════════════
//  延迟光照 Pass (读取 G-Buffer → 光照计算 → swapchain)
// ═══════════════════════════════════════════════════════════
// ═══════════════════════════════════════════════════════════
//  G-Buffer 绘制 (复用 PBR push constant, 写入 G-Buffer MRT)
// ═══════════════════════════════════════════════════════════
void GN_VkDraw3D_GBuffer(const void *vertices, uint32_t vertexCount,
                          const uint32_t *indices, uint32_t indexCount,
                          const sMAT44 *modelMatrix,
                          float albedoR, float albedoG, float albedoB,
                          float roughness, float metallic, float ao,
                          const sIMAGE *albedoTex, const sIMAGE *normalTex){
    if(!GN_Vk.initialized || !s_in3DPass || !vertices || vertexCount==0) return;
    if(!GN_Vk.gBufferPipeline) return;

    // 更新纹理描述符
    {
        VkImageView aView = (albedoTex&&albedoTex->vkImageView)?albedoTex->vkImageView:GN_Vk.defaultImageView;
        VkSampler   aSamp = (albedoTex&&albedoTex->vkSampler)?albedoTex->vkSampler:GN_Vk.defaultSampler;
        VkImageView nView = (normalTex&&normalTex->vkImageView)?normalTex->vkImageView:GN_Vk.defaultImageView;
        VkSampler   nSamp = (normalTex&&normalTex->vkSampler)?normalTex->vkSampler:GN_Vk.defaultSampler;
        VkDescriptorImageInfo aii={}; aii.sampler=aSamp; aii.imageView=aView; aii.imageLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        VkDescriptorImageInfo nii={}; nii.sampler=nSamp; nii.imageView=nView; nii.imageLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        VkWriteDescriptorSet w[2]={{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET},{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET}};
        w[0].dstSet=GN_Vk.gBufferDescriptorSet; w[0].dstBinding=1; w[0].descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; w[0].descriptorCount=1; w[0].pImageInfo=&aii;
        w[1].dstSet=GN_Vk.gBufferDescriptorSet; w[1].dstBinding=3; w[1].descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; w[1].descriptorCount=1; w[1].pImageInfo=&nii;
        vkUpdateDescriptorSets(GN_Vk.device,2,w,0,0);
        vkCmdBindDescriptorSets(GN_Vk.commandBuffers[GN_Vk.currentFrame],VK_PIPELINE_BIND_POINT_GRAPHICS,GN_Vk.gBufferPipelineLayout,0,1,&GN_Vk.gBufferDescriptorSet,0,0);
    }
    // Push constants (PBR 参数 → G-Buffer)
    VkModelPushConstPBR pc;
    memcpy(pc.model,modelMatrix->m,64);
    pc.albedo[0]=albedoR; pc.albedo[1]=albedoG; pc.albedo[2]=albedoB; pc.albedo[3]=1.0f;
    pc.pbrParams[0]=roughness; pc.pbrParams[1]=metallic; pc.pbrParams[2]=ao; pc.pbrParams[3]=0;
    vkCmdPushConstants(GN_Vk.commandBuffers[GN_Vk.currentFrame],GN_Vk.gBufferPipelineLayout,VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,0,sizeof(VkModelPushConstPBR),&pc);
    // 顶点数据
    uint32_t vtxSize=vertexCount*sizeof(VkVertex3D);
    uint32_t idxSize=(indices&&indexCount>0)?indexCount*sizeof(uint32_t):0;
    Ensure3DBufferCapacity(vtxSize,idxSize);
    void *d; vkMapMemory(GN_Vk.device,GN_Vk.vertexMemory3D,0,vtxSize,0,&d); memcpy(d,vertices,vtxSize); vkUnmapMemory(GN_Vk.device,GN_Vk.vertexMemory3D);
    vkCmdBindPipeline(GN_Vk.commandBuffers[GN_Vk.currentFrame],VK_PIPELINE_BIND_POINT_GRAPHICS,GN_Vk.gBufferPipeline);
    VkDeviceSize offs[]={0}; vkCmdBindVertexBuffers(GN_Vk.commandBuffers[GN_Vk.currentFrame],0,1,&GN_Vk.vertexBuffer3D,offs);
    if(indices&&indexCount>0){
        void *id; vkMapMemory(GN_Vk.device,GN_Vk.indexMemory3D,0,idxSize,0,&id); memcpy(id,indices,idxSize); vkUnmapMemory(GN_Vk.device,GN_Vk.indexMemory3D);
        vkCmdBindIndexBuffer(GN_Vk.commandBuffers[GN_Vk.currentFrame],GN_Vk.indexBuffer3D,0,VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(GN_Vk.commandBuffers[GN_Vk.currentFrame],indexCount,1,0,0,0);
    }else{vkCmdDraw(GN_Vk.commandBuffers[GN_Vk.currentFrame],vertexCount,1,0,0);}
}

void GN_VkDeferredLighting(const sMAT44 *viewProj, const sVEC3 *cameraPos,
                            const void *lightUBO, uint32_t lightUBOSize){
    if(!GN_Vk.initialized || !GN_Vk.deferredLightPipeline) return;
    // 必须在 G-Buffer Pass 结束后调用
    if(s_in3DPass){vkCmdEndRenderPass(GN_Vk.commandBuffers[GN_Vk.currentFrame]); s_in3DPass=false; s_inRenderPass=false;}
    // 上传 SceneUBO + LightUBO
    {VkSceneUBO ubo; memcpy(ubo.viewProj,viewProj->m,64); ubo.cameraPos[0]=cameraPos->x; ubo.cameraPos[1]=cameraPos->y; ubo.cameraPos[2]=cameraPos->z; ubo.cameraPos[3]=0;
    if(lightUBO && lightUBOSize >= sizeof(GN3d::sLIGHT_UBO)){
        const GN3d::sLIGHT_UBO *lubo = (const GN3d::sLIGHT_UBO*)lightUBO;
        ubo.ambientLight[0]=lubo->ambientLight[0]; ubo.ambientLight[1]=lubo->ambientLight[1];
        ubo.ambientLight[2]=lubo->ambientLight[2]; ubo.ambientLight[3]=lubo->ambientLight[3];
    }else{
        ubo.ambientLight[0]=0.15f; ubo.ambientLight[1]=0.15f; ubo.ambientLight[2]=0.2f; ubo.ambientLight[3]=1.0f;
    }
    void *d; vkMapMemory(GN_Vk.device,GN_Vk.uniformMemory3D,0,sizeof(ubo),0,&d); memcpy(d,&ubo,sizeof(ubo)); vkUnmapMemory(GN_Vk.device,GN_Vk.uniformMemory3D);}
    if(lightUBO&&lightUBOSize>0){void *ld; vkMapMemory(GN_Vk.device,GN_Vk.lightMemory3D,0,lightUBOSize,0,&ld); memcpy(ld,lightUBO,lightUBOSize); vkUnmapMemory(GN_Vk.device,GN_Vk.lightMemory3D);}
    // 开始延迟光照 Pass → swapchain
    {VkClearValue cv; cv.color={{0,0,0,1}};
    VkRenderPassBeginInfo rpi={VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO}; rpi.renderPass=GN_Vk.deferredLightRenderPass; rpi.framebuffer=GN_Vk.deferredLightFramebuffers[GN_Vk.currentFrame]; rpi.renderArea={{0,0},GN_Vk.swapchainExtent}; rpi.clearValueCount=1; rpi.pClearValues=&cv;
    vkCmdBeginRenderPass(GN_Vk.commandBuffers[GN_Vk.currentFrame],&rpi,VK_SUBPASS_CONTENTS_INLINE);}
    vkCmdBindPipeline(GN_Vk.commandBuffers[GN_Vk.currentFrame],VK_PIPELINE_BIND_POINT_GRAPHICS,GN_Vk.deferredLightPipeline);
    vkCmdBindDescriptorSets(GN_Vk.commandBuffers[GN_Vk.currentFrame],VK_PIPELINE_BIND_POINT_GRAPHICS,GN_Vk.deferredLightPipelineLayout,0,1,&GN_Vk.deferredLightDescriptorSet,0,0);
    vkCmdDraw(GN_Vk.commandBuffers[GN_Vk.currentFrame],3,1,0,0);
    vkCmdEndRenderPass(GN_Vk.commandBuffers[GN_Vk.currentFrame]);
    s_inRenderPass=false;
}

#endif // GN_USING_3D

} // namespace GNg
