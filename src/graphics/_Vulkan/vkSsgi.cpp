// G4：半分辨率 SSGI 资源与 pass（raw → 5×5 单帧双边 → 延迟合成；默认关）。
// 开关：GI UBO 的 envParams.w（0=关）；关闭时 GN_VkSSGIPass 直接返回，不跑 pass。
#include <GN.h>
#include "GN_Vk.h"
#include "vkInternal.h"
#include "giParams.h"

#include <cstdio>
#include <cstring>

namespace GNg {
namespace {

const VkFormat SSGI_FORMAT = VK_FORMAT_R16G16B16A16_SFLOAT;

void halfExtent(uint32_t *width, uint32_t *height) {
    *width = GN_Vk.swapchainExtent.width / 2u;
    *height = GN_Vk.swapchainExtent.height / 2u;
    if(*width < 1u) *width = 1u;
    if(*height < 1u) *height = 1u;
}

bool createSsgiImage(VkImage *image, VkDeviceMemory *memory, VkImageView *view) {
    uint32_t width = 0, height = 0;
    halfExtent(&width, &height);
    VkImageCreateInfo ii = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    ii.imageType = VK_IMAGE_TYPE_2D;
    ii.extent = {width, height, 1};
    ii.mipLevels = 1;
    ii.arrayLayers = 1;
    ii.format = SSGI_FORMAT;
    ii.tiling = VK_IMAGE_TILING_OPTIMAL;
    ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    ii.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    ii.samples = VK_SAMPLE_COUNT_1_BIT;
    ii.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if(vkCreateImage(GN_Vk.device, &ii, 0, image) != VK_SUCCESS) return false;
    VkMemoryRequirements mr;
    vkGetImageMemoryRequirements(GN_Vk.device, *image, &mr);
    VkMemoryAllocateInfo mai = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    mai.allocationSize = mr.size;
    mai.memoryTypeIndex = FindMemType(mr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if(vkAllocateMemory(GN_Vk.device, &mai, 0, memory) != VK_SUCCESS) return false;
    vkBindImageMemory(GN_Vk.device, *image, *memory, 0);
    VkImageViewCreateInfo vi = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    vi.image = *image;
    vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vi.format = SSGI_FORMAT;
    vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    vi.subresourceRange.levelCount = 1;
    vi.subresourceRange.layerCount = 1;
    return vkCreateImageView(GN_Vk.device, &vi, 0, view) == VK_SUCCESS;
}

bool createSsgiRenderPass(VkRenderPass *renderPass) {
    VkAttachmentDescription ad = {};
    ad.format = SSGI_FORMAT;
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
    VkSubpassDependency deps[2] = {};
    deps[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    deps[0].dstSubpass = 0;
    deps[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    deps[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    deps[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    deps[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    deps[1].srcSubpass = 0;
    deps[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    deps[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    deps[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    deps[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    deps[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    VkRenderPassCreateInfo rpi = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    rpi.attachmentCount = 1;
    rpi.pAttachments = &ad;
    rpi.subpassCount = 1;
    rpi.pSubpasses = &sp;
    rpi.dependencyCount = 2;
    rpi.pDependencies = deps;
    return vkCreateRenderPass(GN_Vk.device, &rpi, 0, renderPass) == VK_SUCCESS;
}

bool createSsgiFramebuffer(VkRenderPass renderPass, VkImageView view, VkFramebuffer *fb) {
    uint32_t width = 0, height = 0;
    halfExtent(&width, &height);
    VkFramebufferCreateInfo fci = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    fci.renderPass = renderPass;
    fci.attachmentCount = 1;
    fci.pAttachments = &view;
    fci.width = width;
    fci.height = height;
    fci.layers = 1;
    return vkCreateFramebuffer(GN_Vk.device, &fci, 0, fb) == VK_SUCCESS;
}

bool createSsgiPipeline(VkRenderPass renderPass, VkPipelineLayout layout, const char *fragPath,
                        VkPipeline *pipeline) {
    auto vertSpv = LoadSPIRVFile("assets/shaders/gn3d_deferred_light.vert.spv");
    auto fragSpv = LoadSPIRVFile(fragPath);
    if(vertSpv.empty() || fragSpv.empty()) {
        fprintf(stderr, "[GN_Vk] SSGI 缺少 SPIR-V：%s\n", fragPath);
        return false;
    }
    uint32_t width = 0, height = 0;
    halfExtent(&width, &height);
    GN3d::sPIPELINE_DESC desc;
    desc.aKind = GN3d::ePIPE_GRAPHICS;
    desc.aVertSpv = vertSpv.data();
    desc.aVertSize = vertSpv.size() * sizeof(uint32_t);
    desc.aFragSpv = fragSpv.data();
    desc.aFragSize = fragSpv.size() * sizeof(uint32_t);
    desc.aRenderPass = renderPass;
    desc.aSubpass = 0;
    desc.aLayout = layout;
    desc.aDepthTest = false;
    desc.aDepthWrite = false;
    desc.aDepthStencil = false;
    desc.aCullBack = false;
    desc.aFrontFace = VK_FRONT_FACE_CLOCKWISE;
    desc.aBlend = GN3d::eBLEND_NONE;
    desc.aTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    desc.aSamples = VK_SAMPLE_COUNT_1_BIT;
    desc.aColorAttachmentCount = 1;
    desc.aDynamicViewport = false;
    desc.aDynamicScissor = false;
    desc.aViewport = VkViewport{0.0f, 0.0f, (float)width, (float)height, 0.0f, 1.0f};
    desc.aScissor = VkRect2D{{0, 0}, {width, height}};
    desc.aCustomBlend = false;
    *pipeline = GN_Vk.aPipelineCache.GetOrCreate(desc);
    if(!*pipeline) {
        fprintf(stderr, "[GN_Vk] SSGI 管线创建失败: %s\n", GN_Vk.aPipelineCache.GetLastError());
        return false;
    }
    return true;
}

void writeSsgiDescriptors() {
    if(!GN_Vk.ssgiDescriptorSet || !GN_Vk.ssgiDenoiseDescriptorSet) return;
    VkDescriptorBufferInfo ubo = {GN_Vk.ssgiUBOBuffer, 0, sizeof(GN3d::sSSGI_UBO)};
    VkDescriptorBufferInfo giUbo = {GN_Vk.giUBOBuffer, 0, sizeof(GN3d::sGI_UBO)};
    VkDescriptorImageInfo normal = {GN_Vk.ssgiSampler, GN_Vk.gNormalRoughView,
                                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    VkDescriptorImageInfo depth = {GN_Vk.ssgiSampler, GN_Vk.gDepthView,
                                   VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL};
    VkDescriptorImageInfo albedo = {GN_Vk.ssgiSampler, GN_Vk.gAlbedoMetalView,
                                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    VkDescriptorImageInfo raw = {GN_Vk.ssgiSampler, GN_Vk.ssgiRawView,
                                 VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    VkDescriptorImageInfo denoised = {GN_Vk.ssgiSampler, GN_Vk.ssgiDenoisedView,
                                     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    VkWriteDescriptorSet w[8] = {};
    for(int i = 0; i < 8; i++) w[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    const VkDescriptorSet ssgiSet = GN_Vk.ssgiDescriptorSet;
    const VkDescriptorSet denoiseSet = GN_Vk.ssgiDenoiseDescriptorSet;
    w[0].dstSet = ssgiSet;
    w[0].dstBinding = 0;
    w[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    w[0].descriptorCount = 1;
    w[0].pBufferInfo = &ubo;
    w[1].dstSet = ssgiSet;
    w[1].dstBinding = 1;
    w[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    w[1].descriptorCount = 1;
    w[1].pImageInfo = &normal;
    w[2].dstSet = ssgiSet;
    w[2].dstBinding = 2;
    w[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    w[2].descriptorCount = 1;
    w[2].pImageInfo = &depth;
    w[3].dstSet = ssgiSet;
    w[3].dstBinding = 3;
    w[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    w[3].descriptorCount = 1;
    w[3].pImageInfo = &albedo;
    w[4].dstSet = ssgiSet;
    w[4].dstBinding = 4;
    w[4].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    w[4].descriptorCount = 1;
    w[4].pBufferInfo = &giUbo;
    w[5].dstSet = denoiseSet;
    w[5].dstBinding = 0;
    w[5].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    w[5].descriptorCount = 1;
    w[5].pImageInfo = &raw;
    w[6].dstSet = denoiseSet;
    w[6].dstBinding = 1;
    w[6].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    w[6].descriptorCount = 1;
    w[6].pImageInfo = &depth;
    w[7].dstSet = denoiseSet;
    w[7].dstBinding = 2;
    w[7].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    w[7].descriptorCount = 1;
    w[7].pImageInfo = &normal;
    vkUpdateDescriptorSets(GN_Vk.device, 8, w, 0, 0);
    // 延迟 layout binding 13 = 去噪后的 SSGI（半分辨率，线性上采样）。
    if(GN_Vk.deferredLightDescriptorSet) {
        VkWriteDescriptorSet wd = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        wd.dstSet = GN_Vk.deferredLightDescriptorSet;
        wd.dstBinding = 13;
        wd.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        wd.descriptorCount = 1;
        wd.pImageInfo = &denoised;
        vkUpdateDescriptorSets(GN_Vk.device, 1, &wd, 0, 0);
    }
}

bool createSsgiLayouts() {
    VkDescriptorSetLayoutBinding ssgiBindings[5] = {};
    ssgiBindings[0].binding = 0;
    ssgiBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    ssgiBindings[0].descriptorCount = 1;
    ssgiBindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    for(int i = 1; i <= 3; i++) {
        ssgiBindings[i].binding = (uint32_t)i;
        ssgiBindings[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        ssgiBindings[i].descriptorCount = 1;
        ssgiBindings[i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    }
    ssgiBindings[4].binding = 4;
    ssgiBindings[4].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    ssgiBindings[4].descriptorCount = 1;
    ssgiBindings[4].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    VkDescriptorSetLayoutCreateInfo slci = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    slci.bindingCount = 5;
    slci.pBindings = ssgiBindings;
    if(vkCreateDescriptorSetLayout(GN_Vk.device, &slci, 0, &GN_Vk.ssgiDescriptorLayout) !=
       VK_SUCCESS)
        return false;
    VkDescriptorSetLayoutBinding denoiseBindings[3] = {};
    for(int i = 0; i < 3; i++) {
        denoiseBindings[i].binding = (uint32_t)i;
        denoiseBindings[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        denoiseBindings[i].descriptorCount = 1;
        denoiseBindings[i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    }
    slci.bindingCount = 3;
    slci.pBindings = denoiseBindings;
    if(vkCreateDescriptorSetLayout(GN_Vk.device, &slci, 0, &GN_Vk.ssgiDenoiseDescriptorLayout) !=
       VK_SUCCESS)
        return false;
    VkPipelineLayoutCreateInfo pl = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    pl.setLayoutCount = 1;
    pl.pSetLayouts = &GN_Vk.ssgiDescriptorLayout;
    if(vkCreatePipelineLayout(GN_Vk.device, &pl, 0, &GN_Vk.ssgiPipelineLayout) != VK_SUCCESS)
        return false;
    pl.pSetLayouts = &GN_Vk.ssgiDenoiseDescriptorLayout;
    if(vkCreatePipelineLayout(GN_Vk.device, &pl, 0, &GN_Vk.ssgiDenoisePipelineLayout) != VK_SUCCESS)
        return false;
    VkDescriptorSetAllocateInfo dai = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    dai.descriptorPool = GN_Vk.descriptorPool;
    dai.descriptorSetCount = 1;
    dai.pSetLayouts = &GN_Vk.ssgiDescriptorLayout;
    if(vkAllocateDescriptorSets(GN_Vk.device, &dai, &GN_Vk.ssgiDescriptorSet) != VK_SUCCESS)
        return false;
    dai.pSetLayouts = &GN_Vk.ssgiDenoiseDescriptorLayout;
    return vkAllocateDescriptorSets(GN_Vk.device, &dai, &GN_Vk.ssgiDenoiseDescriptorSet) ==
           VK_SUCCESS;
}

bool createSsgiSampler() {
    VkSamplerCreateInfo sci = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    sci.magFilter = VK_FILTER_LINEAR;
    sci.minFilter = VK_FILTER_LINEAR;
    sci.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sci.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    return vkCreateSampler(GN_Vk.device, &sci, 0, &GN_Vk.ssgiSampler) == VK_SUCCESS;
}

bool createSsgiUbo() {
    CreateBuf(sizeof(GN3d::sSSGI_UBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
              GN_Vk.ssgiUBOBuffer, GN_Vk.ssgiUBOMemory);
    if(GN_Vk.ssgiUBOBuffer == VK_NULL_HANDLE) return false;
    const GN3d::sSSGI_UBO ubo = GN3d::makeDefaultSsgiUbo();
    void *mapped = nullptr;
    vkMapMemory(GN_Vk.device, GN_Vk.ssgiUBOMemory, 0, sizeof(ubo), 0, &mapped);
    if(mapped) {
        memcpy(mapped, &ubo, sizeof(ubo));
        vkUnmapMemory(GN_Vk.device, GN_Vk.ssgiUBOMemory);
    }
    return true;
}

void destroySsgiTargets() {
    if(GN_Vk.ssgiFramebuffer) vkDestroyFramebuffer(GN_Vk.device, GN_Vk.ssgiFramebuffer, 0);
    if(GN_Vk.ssgiDenoiseFramebuffer)
        vkDestroyFramebuffer(GN_Vk.device, GN_Vk.ssgiDenoiseFramebuffer, 0);
    if(GN_Vk.ssgiRawView) vkDestroyImageView(GN_Vk.device, GN_Vk.ssgiRawView, 0);
    if(GN_Vk.ssgiDenoisedView) vkDestroyImageView(GN_Vk.device, GN_Vk.ssgiDenoisedView, 0);
    if(GN_Vk.ssgiRawImage) vkDestroyImage(GN_Vk.device, GN_Vk.ssgiRawImage, 0);
    if(GN_Vk.ssgiDenoisedImage) vkDestroyImage(GN_Vk.device, GN_Vk.ssgiDenoisedImage, 0);
    if(GN_Vk.ssgiRawMemory) vkFreeMemory(GN_Vk.device, GN_Vk.ssgiRawMemory, 0);
    if(GN_Vk.ssgiDenoisedMemory) vkFreeMemory(GN_Vk.device, GN_Vk.ssgiDenoisedMemory, 0);
    GN_Vk.ssgiFramebuffer = VK_NULL_HANDLE;
    GN_Vk.ssgiDenoiseFramebuffer = VK_NULL_HANDLE;
    GN_Vk.ssgiRawView = VK_NULL_HANDLE;
    GN_Vk.ssgiDenoisedView = VK_NULL_HANDLE;
    GN_Vk.ssgiRawImage = VK_NULL_HANDLE;
    GN_Vk.ssgiDenoisedImage = VK_NULL_HANDLE;
    GN_Vk.ssgiRawMemory = VK_NULL_HANDLE;
    GN_Vk.ssgiDenoisedMemory = VK_NULL_HANDLE;
}

bool createSsgiTargets() {
    return createSsgiImage(&GN_Vk.ssgiRawImage, &GN_Vk.ssgiRawMemory, &GN_Vk.ssgiRawView) &&
           createSsgiImage(&GN_Vk.ssgiDenoisedImage, &GN_Vk.ssgiDenoisedMemory,
                           &GN_Vk.ssgiDenoisedView) &&
           createSsgiFramebuffer(GN_Vk.ssgiRenderPass, GN_Vk.ssgiRawView,
                                 &GN_Vk.ssgiFramebuffer) &&
           createSsgiFramebuffer(GN_Vk.ssgiDenoiseRenderPass, GN_Vk.ssgiDenoisedView,
                                 &GN_Vk.ssgiDenoiseFramebuffer);
}

} // namespace

void init3DSsgi() {
    if(GN_Vk.ssgiRawImage != VK_NULL_HANDLE) return; // 幂等
    if(!createSsgiRenderPass(&GN_Vk.ssgiRenderPass) ||
       !createSsgiRenderPass(&GN_Vk.ssgiDenoiseRenderPass) || !createSsgiSampler() ||
       !createSsgiUbo() || !createSsgiTargets() || !createSsgiLayouts()) {
        fprintf(stderr, "[GN_Vk] 警告: SSGI 资源初始化失败（G4）\n");
        return;
    }
    if(!createSsgiPipeline(GN_Vk.ssgiRenderPass, GN_Vk.ssgiPipelineLayout,
                           "assets/shaders/gn3d_ssgi.frag.spv", &GN_Vk.ssgiPipeline) ||
       !createSsgiPipeline(GN_Vk.ssgiDenoiseRenderPass, GN_Vk.ssgiDenoisePipelineLayout,
                           "assets/shaders/gn3d_ssgi_denoise.frag.spv",
                           &GN_Vk.ssgiDenoisePipeline)) {
        fprintf(stderr, "[GN_Vk] 警告: SSGI 管线不可用（G4）\n");
        return;
    }
    writeSsgiDescriptors();
}

void destroy3DSsgi() {
    destroySsgiTargets();
    if(GN_Vk.ssgiRenderPass) vkDestroyRenderPass(GN_Vk.device, GN_Vk.ssgiRenderPass, 0);
    if(GN_Vk.ssgiDenoiseRenderPass)
        vkDestroyRenderPass(GN_Vk.device, GN_Vk.ssgiDenoiseRenderPass, 0);
    GN_Vk.ssgiPipeline = VK_NULL_HANDLE;      // 归 aPipelineCache
    GN_Vk.ssgiDenoisePipeline = VK_NULL_HANDLE;
    if(GN_Vk.ssgiPipelineLayout) vkDestroyPipelineLayout(GN_Vk.device, GN_Vk.ssgiPipelineLayout, 0);
    if(GN_Vk.ssgiDenoisePipelineLayout)
        vkDestroyPipelineLayout(GN_Vk.device, GN_Vk.ssgiDenoisePipelineLayout, 0);
    if(GN_Vk.ssgiDescriptorLayout)
        vkDestroyDescriptorSetLayout(GN_Vk.device, GN_Vk.ssgiDescriptorLayout, 0);
    if(GN_Vk.ssgiDenoiseDescriptorLayout)
        vkDestroyDescriptorSetLayout(GN_Vk.device, GN_Vk.ssgiDenoiseDescriptorLayout, 0);
    if(GN_Vk.ssgiUBOBuffer) vkDestroyBuffer(GN_Vk.device, GN_Vk.ssgiUBOBuffer, 0);
    if(GN_Vk.ssgiUBOMemory) vkFreeMemory(GN_Vk.device, GN_Vk.ssgiUBOMemory, 0);
    if(GN_Vk.ssgiSampler) vkDestroySampler(GN_Vk.device, GN_Vk.ssgiSampler, 0);
    GN_Vk.ssgiRenderPass = VK_NULL_HANDLE;
    GN_Vk.ssgiDenoiseRenderPass = VK_NULL_HANDLE;
    GN_Vk.ssgiPipelineLayout = VK_NULL_HANDLE;
    GN_Vk.ssgiDenoisePipelineLayout = VK_NULL_HANDLE;
    GN_Vk.ssgiDescriptorLayout = VK_NULL_HANDLE;
    GN_Vk.ssgiDescriptorSet = VK_NULL_HANDLE;
    GN_Vk.ssgiDenoiseDescriptorLayout = VK_NULL_HANDLE;
    GN_Vk.ssgiDenoiseDescriptorSet = VK_NULL_HANDLE;
    GN_Vk.ssgiUBOBuffer = VK_NULL_HANDLE;
    GN_Vk.ssgiUBOMemory = VK_NULL_HANDLE;
    GN_Vk.ssgiSampler = VK_NULL_HANDLE;
}

void recreate3DSsgiTargets() {
    if(GN_Vk.ssgiRawImage == VK_NULL_HANDLE) return;
    destroySsgiTargets();
    if(!createSsgiTargets()) {
        fprintf(stderr, "[GN_Vk] 警告: SSGI 目标 resize 重建失败（G4）\n");
        return;
    }
    createSsgiPipeline(GN_Vk.ssgiRenderPass, GN_Vk.ssgiPipelineLayout,
                       "assets/shaders/gn3d_ssgi.frag.spv", &GN_Vk.ssgiPipeline);
    createSsgiPipeline(GN_Vk.ssgiDenoiseRenderPass, GN_Vk.ssgiDenoisePipelineLayout,
                       "assets/shaders/gn3d_ssgi_denoise.frag.spv", &GN_Vk.ssgiDenoisePipeline);
    writeSsgiDescriptors();
}

bool isSsgiEnabled() {
    if(GN_Vk.giUBOMemory == VK_NULL_HANDLE) return false;
    float intensity = 0.0f; // GI UBO envParams[3]：indirect(16)+aoParams(16)+envParams.x/y/z 共 44 B 偏移
    void *mapped = nullptr;
    vkMapMemory(GN_Vk.device, GN_Vk.giUBOMemory, 0, sizeof(GN3d::sGI_UBO), 0, &mapped);
    if(mapped) {
        memcpy(&intensity, (const char *)mapped + 44, sizeof(float));
        vkUnmapMemory(GN_Vk.device, GN_Vk.giUBOMemory);
    }
    return intensity > 0.0f;
}

void updateSsgiUniforms(const sMAT44 &viewProj) {
    if(GN_Vk.ssgiUBOMemory == VK_NULL_HANDLE) return;
    uint32_t halfWidth = 0, halfHeight = 0;
    halfExtent(&halfWidth, &halfHeight);
    sMAT44 inverse;
    Mat44Inverse(&inverse, &viewProj);
    void *mapped = nullptr;
    vkMapMemory(GN_Vk.device, GN_Vk.ssgiUBOMemory, 0, sizeof(GN3d::sSSGI_UBO), 0, &mapped);
    if(mapped) {
        memcpy(mapped, viewProj.m, sizeof(float) * 16);
        memcpy((char *)mapped + 64, inverse.m, sizeof(float) * 16);
        float screen[4] = {1.0f / (float)GN_Vk.swapchainExtent.width,
                           1.0f / (float)GN_Vk.swapchainExtent.height,
                           (float)halfWidth, (float)halfHeight};
        memcpy((char *)mapped + 144, screen, sizeof(screen));
        vkUnmapMemory(GN_Vk.device, GN_Vk.ssgiUBOMemory);
    }
}

void recordSsgiPasses(VkCommandBuffer cb) {
    const VkClearValue clearValue = {{{0.0f, 0.0f, 0.0f, 0.0f}}};
    uint32_t width = 0, height = 0;
    halfExtent(&width, &height);
    VkRenderPassBeginInfo rpi = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    rpi.renderPass = GN_Vk.ssgiRenderPass;
    rpi.framebuffer = GN_Vk.ssgiFramebuffer;
    rpi.renderArea = {{0, 0}, {width, height}};
    rpi.clearValueCount = 1;
    rpi.pClearValues = &clearValue;
    vkCmdBeginRenderPass(cb, &rpi, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, GN_Vk.ssgiPipeline);
    vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, GN_Vk.ssgiPipelineLayout, 0, 1,
                            &GN_Vk.ssgiDescriptorSet, 0, nullptr);
    vkCmdDraw(cb, 3, 1, 0, 0);
    vkCmdEndRenderPass(cb);
    rpi.renderPass = GN_Vk.ssgiDenoiseRenderPass;
    rpi.framebuffer = GN_Vk.ssgiDenoiseFramebuffer;
    vkCmdBeginRenderPass(cb, &rpi, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, GN_Vk.ssgiDenoisePipeline);
    vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, GN_Vk.ssgiDenoisePipelineLayout,
                            0, 1, &GN_Vk.ssgiDenoiseDescriptorSet, 0, nullptr);
    vkCmdDraw(cb, 3, 1, 0, 0);
    vkCmdEndRenderPass(cb);
}

} // namespace GNg
