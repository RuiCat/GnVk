// G2b：SSAO 资源与 pass（世界空间 16 点 SSAO → 4×4 双边模糊 → 延迟合成）。
// 默认关：GI UBO indirect.w=0 时 GN_VkSSAOPass 直接返回，观感/开销与接线前一致。
#include <GN.h>
#include "GN_Vk.h"
#include "vkInternal.h"
#include "giParams.h"

#include <cstdio>
#include <cstring>

namespace GNg {
namespace {

const VkFormat SSAO_FORMAT = VK_FORMAT_R8_UNORM;

// 单张 R8 AO 目标（raw/blur 共用）。
bool createSsaoImage(VkImage *image, VkDeviceMemory *memory, VkImageView *view) {
    VkImageCreateInfo ii = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    ii.imageType = VK_IMAGE_TYPE_2D;
    ii.extent = {GN_Vk.swapchainExtent.width, GN_Vk.swapchainExtent.height, 1};
    ii.mipLevels = 1;
    ii.arrayLayers = 1;
    ii.format = SSAO_FORMAT;
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
    vi.format = SSAO_FORMAT;
    vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    vi.subresourceRange.levelCount = 1;
    vi.subresourceRange.layerCount = 1;
    return vkCreateImageView(GN_Vk.device, &vi, 0, view) == VK_SUCCESS;
}

// SSAO/blur 共用的单附件 render pass：R8、final=SHADER_READ，含两侧外部依赖。
bool createSsaoRenderPass(VkRenderPass *renderPass) {
    VkAttachmentDescription ad = {};
    ad.format = SSAO_FORMAT;
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

bool createSsaoFramebuffer(VkRenderPass renderPass, VkImageView view, VkFramebuffer *fb) {
    VkFramebufferCreateInfo fci = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    fci.renderPass = renderPass;
    fci.attachmentCount = 1;
    fci.pAttachments = &view;
    fci.width = GN_Vk.swapchainExtent.width;
    fci.height = GN_Vk.swapchainExtent.height;
    fci.layers = 1;
    return vkCreateFramebuffer(GN_Vk.device, &fci, 0, fb) == VK_SUCCESS;
}

// 全屏三角形管线（顶点复用 gn3d_deferred_light.vert；固定视口随 resize 重建）。
bool createSsaoPipeline(VkRenderPass renderPass, VkPipelineLayout layout, const char *fragPath,
                        VkPipeline *pipeline) {
    auto vertSpv = LoadSPIRVFile("assets/shaders/gn3d_deferred_light.vert.spv");
    auto fragSpv = LoadSPIRVFile(fragPath);
    if(vertSpv.empty() || fragSpv.empty()) {
        fprintf(stderr, "[GN_Vk] SSAO 缺少 SPIR-V：%s\n", fragPath);
        return false;
    }
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
    desc.aViewport = VkViewport{0.0f, 0.0f, (float)GN_Vk.swapchainExtent.width,
                                (float)GN_Vk.swapchainExtent.height, 0.0f, 1.0f};
    desc.aScissor = VkRect2D{{0, 0}, GN_Vk.swapchainExtent};
    desc.aCustomBlend = false;
    *pipeline = GN_Vk.aPipelineCache.GetOrCreate(desc);
    if(!*pipeline) {
        fprintf(stderr, "[GN_Vk] SSAO 管线创建失败: %s\n", GN_Vk.aPipelineCache.GetLastError());
        return false;
    }
    return true;
}

// 描述符写入：SSAO set 引用 G-Buffer normal/depth；blur set 再引用 rawAO；延迟 binding12=blurAO。
void writeSsaoDescriptors() {
    if(!GN_Vk.ssaoDescriptorSet || !GN_Vk.ssaoBlurDescriptorSet) return;
    VkDescriptorBufferInfo ubo = {GN_Vk.ssaoUBOBuffer, 0, sizeof(GN3d::sSSAO_UBO)};
    VkDescriptorImageInfo normal = {GN_Vk.ssaoSampler, GN_Vk.gNormalRoughView,
                                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    VkDescriptorImageInfo depth = {GN_Vk.ssaoSampler, GN_Vk.gDepthView,
                                   VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL};
    VkDescriptorImageInfo rawAo = {GN_Vk.ssaoSampler, GN_Vk.ssaoRawView,
                                   VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    VkDescriptorImageInfo blurAo = {GN_Vk.ssaoSampler, GN_Vk.ssaoBlurView,
                                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    VkWriteDescriptorSet w[6] = {};
    for(int i = 0; i < 6; i++) w[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    // SSAO set：0=UBO、1=normal、2=depth
    w[0].dstSet = GN_Vk.ssaoDescriptorSet;
    w[0].dstBinding = 0;
    w[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    w[0].descriptorCount = 1;
    w[0].pBufferInfo = &ubo;
    w[1].dstSet = GN_Vk.ssaoDescriptorSet;
    w[1].dstBinding = 1;
    w[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    w[1].descriptorCount = 1;
    w[1].pImageInfo = &normal;
    w[2].dstSet = GN_Vk.ssaoDescriptorSet;
    w[2].dstBinding = 2;
    w[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    w[2].descriptorCount = 1;
    w[2].pImageInfo = &depth;
    // blur set：0=UBO、1=rawAO、2=depth、3=normal
    w[3].dstSet = GN_Vk.ssaoBlurDescriptorSet;
    w[3].dstBinding = 0;
    w[3].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    w[3].descriptorCount = 1;
    w[3].pBufferInfo = &ubo;
    w[4].dstSet = GN_Vk.ssaoBlurDescriptorSet;
    w[4].dstBinding = 1;
    w[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    w[4].descriptorCount = 1;
    w[4].pImageInfo = &rawAo;
    w[5].dstSet = GN_Vk.ssaoBlurDescriptorSet;
    w[5].dstBinding = 2;
    w[5].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    w[5].descriptorCount = 1;
    w[5].pImageInfo = &depth;
    vkUpdateDescriptorSets(GN_Vk.device, 6, w, 0, 0);
    // blur set 的第三个 sampler 单独写（binding 3=normal）
    VkWriteDescriptorSet wn = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    wn.dstSet = GN_Vk.ssaoBlurDescriptorSet;
    wn.dstBinding = 3;
    wn.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    wn.descriptorCount = 1;
    wn.pImageInfo = &normal;
    vkUpdateDescriptorSets(GN_Vk.device, 1, &wn, 0, 0);
    // 延迟布局 12 = blurAO（G2b 合成用）
    if(GN_Vk.deferredLightDescriptorSet) {
        VkWriteDescriptorSet wd = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        wd.dstSet = GN_Vk.deferredLightDescriptorSet;
        wd.dstBinding = 12;
        wd.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        wd.descriptorCount = 1;
        wd.pImageInfo = &blurAo;
        vkUpdateDescriptorSets(GN_Vk.device, 1, &wd, 0, 0);
    }
}

// 销毁尺寸相关资源（image/view/framebuffer）；保留 render pass/布局/描述符集/UBO。
void destroySsaoTargets() {
    if(GN_Vk.ssaoFramebuffer) vkDestroyFramebuffer(GN_Vk.device, GN_Vk.ssaoFramebuffer, 0);
    if(GN_Vk.ssaoBlurFramebuffer) vkDestroyFramebuffer(GN_Vk.device, GN_Vk.ssaoBlurFramebuffer, 0);
    if(GN_Vk.ssaoRawView) vkDestroyImageView(GN_Vk.device, GN_Vk.ssaoRawView, 0);
    if(GN_Vk.ssaoBlurView) vkDestroyImageView(GN_Vk.device, GN_Vk.ssaoBlurView, 0);
    if(GN_Vk.ssaoRawImage) vkDestroyImage(GN_Vk.device, GN_Vk.ssaoRawImage, 0);
    if(GN_Vk.ssaoBlurImage) vkDestroyImage(GN_Vk.device, GN_Vk.ssaoBlurImage, 0);
    if(GN_Vk.ssaoRawMemory) vkFreeMemory(GN_Vk.device, GN_Vk.ssaoRawMemory, 0);
    if(GN_Vk.ssaoBlurMemory) vkFreeMemory(GN_Vk.device, GN_Vk.ssaoBlurMemory, 0);
    GN_Vk.ssaoFramebuffer = VK_NULL_HANDLE;
    GN_Vk.ssaoBlurFramebuffer = VK_NULL_HANDLE;
    GN_Vk.ssaoRawView = VK_NULL_HANDLE;
    GN_Vk.ssaoBlurView = VK_NULL_HANDLE;
    GN_Vk.ssaoRawImage = VK_NULL_HANDLE;
    GN_Vk.ssaoBlurImage = VK_NULL_HANDLE;
    GN_Vk.ssaoRawMemory = VK_NULL_HANDLE;
    GN_Vk.ssaoBlurMemory = VK_NULL_HANDLE;
}

bool createSsaoTargets() {
    return createSsaoImage(&GN_Vk.ssaoRawImage, &GN_Vk.ssaoRawMemory, &GN_Vk.ssaoRawView) &&
           createSsaoImage(&GN_Vk.ssaoBlurImage, &GN_Vk.ssaoBlurMemory, &GN_Vk.ssaoBlurView) &&
           createSsaoFramebuffer(GN_Vk.ssaoRenderPass, GN_Vk.ssaoRawView,
                                 &GN_Vk.ssaoFramebuffer) &&
           createSsaoFramebuffer(GN_Vk.ssaoBlurRenderPass, GN_Vk.ssaoBlurView,
                                 &GN_Vk.ssaoBlurFramebuffer);
}

bool createSsaoLayouts() {
    VkDescriptorSetLayoutBinding ssaoBindings[3] = {};
    ssaoBindings[0].binding = 0;
    ssaoBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    ssaoBindings[0].descriptorCount = 1;
    ssaoBindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    for(int i = 1; i <= 2; i++) {
        ssaoBindings[i].binding = (uint32_t)i;
        ssaoBindings[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        ssaoBindings[i].descriptorCount = 1;
        ssaoBindings[i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    }
    VkDescriptorSetLayoutCreateInfo slci = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    slci.bindingCount = 3;
    slci.pBindings = ssaoBindings;
    if(vkCreateDescriptorSetLayout(GN_Vk.device, &slci, 0, &GN_Vk.ssaoDescriptorLayout) != VK_SUCCESS)
        return false;
    VkDescriptorSetLayoutBinding blurBindings[4] = {};
    blurBindings[0].binding = 0;
    blurBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    blurBindings[0].descriptorCount = 1;
    blurBindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    for(int i = 1; i <= 3; i++) {
        blurBindings[i].binding = (uint32_t)i;
        blurBindings[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        blurBindings[i].descriptorCount = 1;
        blurBindings[i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    }
    slci.bindingCount = 4;
    slci.pBindings = blurBindings;
    if(vkCreateDescriptorSetLayout(GN_Vk.device, &slci, 0, &GN_Vk.ssaoBlurDescriptorLayout) !=
       VK_SUCCESS)
        return false;
    VkPipelineLayoutCreateInfo pl = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    pl.setLayoutCount = 1;
    pl.pSetLayouts = &GN_Vk.ssaoDescriptorLayout;
    if(vkCreatePipelineLayout(GN_Vk.device, &pl, 0, &GN_Vk.ssaoPipelineLayout) != VK_SUCCESS)
        return false;
    pl.pSetLayouts = &GN_Vk.ssaoBlurDescriptorLayout;
    if(vkCreatePipelineLayout(GN_Vk.device, &pl, 0, &GN_Vk.ssaoBlurPipelineLayout) != VK_SUCCESS)
        return false;
    VkDescriptorSetAllocateInfo dai = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    dai.descriptorPool = GN_Vk.descriptorPool;
    dai.descriptorSetCount = 1;
    dai.pSetLayouts = &GN_Vk.ssaoDescriptorLayout;
    if(vkAllocateDescriptorSets(GN_Vk.device, &dai, &GN_Vk.ssaoDescriptorSet) != VK_SUCCESS)
        return false;
    dai.pSetLayouts = &GN_Vk.ssaoBlurDescriptorLayout;
    return vkAllocateDescriptorSets(GN_Vk.device, &dai, &GN_Vk.ssaoBlurDescriptorSet) == VK_SUCCESS;
}

bool createSsaoSampler() {
    VkSamplerCreateInfo sci = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    sci.magFilter = VK_FILTER_NEAREST;
    sci.minFilter = VK_FILTER_NEAREST;
    sci.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sci.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    return vkCreateSampler(GN_Vk.device, &sci, 0, &GN_Vk.ssaoSampler) == VK_SUCCESS;
}

bool createSsaoUbo() {
    CreateBuf(sizeof(GN3d::sSSAO_UBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
              GN_Vk.ssaoUBOBuffer, GN_Vk.ssaoUBOMemory);
    if(GN_Vk.ssaoUBOBuffer == VK_NULL_HANDLE) return false;
    const GN3d::sSSAO_UBO ubo = GN3d::makeDefaultSsaoUbo();
    void *mapped = nullptr;
    vkMapMemory(GN_Vk.device, GN_Vk.ssaoUBOMemory, 0, sizeof(ubo), 0, &mapped);
    if(mapped) {
        memcpy(mapped, &ubo, sizeof(ubo));
        vkUnmapMemory(GN_Vk.device, GN_Vk.ssaoUBOMemory);
    }
    return true;
}

} // namespace

void init3DSsao() {
    if(GN_Vk.ssaoRawImage != VK_NULL_HANDLE) return; // 幂等
    if(!createSsaoRenderPass(&GN_Vk.ssaoRenderPass) ||
       !createSsaoRenderPass(&GN_Vk.ssaoBlurRenderPass) || !createSsaoSampler() ||
       !createSsaoUbo() || !createSsaoTargets() || !createSsaoLayouts()) {
        fprintf(stderr, "[GN_Vk] 警告: SSAO 资源初始化失败（G2b）\n");
        return;
    }
    if(!createSsaoPipeline(GN_Vk.ssaoRenderPass, GN_Vk.ssaoPipelineLayout,
                           "assets/shaders/gn3d_ssao.frag.spv", &GN_Vk.ssaoPipeline) ||
       !createSsaoPipeline(GN_Vk.ssaoBlurRenderPass, GN_Vk.ssaoBlurPipelineLayout,
                           "assets/shaders/gn3d_ssao_blur.frag.spv", &GN_Vk.ssaoBlurPipeline)) {
        fprintf(stderr, "[GN_Vk] 警告: SSAO 管线不可用（G2b）\n");
        return;
    }
    writeSsaoDescriptors();
}

void destroy3DSsao() {
    destroySsaoTargets();
    if(GN_Vk.ssaoRenderPass) vkDestroyRenderPass(GN_Vk.device, GN_Vk.ssaoRenderPass, 0);
    if(GN_Vk.ssaoBlurRenderPass) vkDestroyRenderPass(GN_Vk.device, GN_Vk.ssaoBlurRenderPass, 0);
    GN_Vk.ssaoPipeline = VK_NULL_HANDLE;     // 归 aPipelineCache
    GN_Vk.ssaoBlurPipeline = VK_NULL_HANDLE;
    if(GN_Vk.ssaoPipelineLayout) vkDestroyPipelineLayout(GN_Vk.device, GN_Vk.ssaoPipelineLayout, 0);
    if(GN_Vk.ssaoBlurPipelineLayout)
        vkDestroyPipelineLayout(GN_Vk.device, GN_Vk.ssaoBlurPipelineLayout, 0);
    if(GN_Vk.ssaoDescriptorLayout)
        vkDestroyDescriptorSetLayout(GN_Vk.device, GN_Vk.ssaoDescriptorLayout, 0);
    if(GN_Vk.ssaoBlurDescriptorLayout)
        vkDestroyDescriptorSetLayout(GN_Vk.device, GN_Vk.ssaoBlurDescriptorLayout, 0);
    if(GN_Vk.ssaoUBOBuffer) vkDestroyBuffer(GN_Vk.device, GN_Vk.ssaoUBOBuffer, 0);
    if(GN_Vk.ssaoUBOMemory) vkFreeMemory(GN_Vk.device, GN_Vk.ssaoUBOMemory, 0);
    if(GN_Vk.ssaoSampler) vkDestroySampler(GN_Vk.device, GN_Vk.ssaoSampler, 0);
    GN_Vk.ssaoRenderPass = VK_NULL_HANDLE;
    GN_Vk.ssaoBlurRenderPass = VK_NULL_HANDLE;
    GN_Vk.ssaoPipelineLayout = VK_NULL_HANDLE;
    GN_Vk.ssaoBlurPipelineLayout = VK_NULL_HANDLE;
    GN_Vk.ssaoDescriptorLayout = VK_NULL_HANDLE;
    GN_Vk.ssaoDescriptorSet = VK_NULL_HANDLE;
    GN_Vk.ssaoBlurDescriptorLayout = VK_NULL_HANDLE;
    GN_Vk.ssaoBlurDescriptorSet = VK_NULL_HANDLE;
    GN_Vk.ssaoUBOBuffer = VK_NULL_HANDLE;
    GN_Vk.ssaoUBOMemory = VK_NULL_HANDLE;
    GN_Vk.ssaoSampler = VK_NULL_HANDLE;
}

void recreate3DSsaoTargets() {
    if(GN_Vk.ssaoRawImage == VK_NULL_HANDLE) return; // 初始化失败/未启用
    destroySsaoTargets();
    if(!createSsaoTargets()) {
        fprintf(stderr, "[GN_Vk] 警告: SSAO 目标 resize 重建失败（G2b）\n");
        return;
    }
    createSsaoPipeline(GN_Vk.ssaoRenderPass, GN_Vk.ssaoPipelineLayout,
                       "assets/shaders/gn3d_ssao.frag.spv", &GN_Vk.ssaoPipeline);
    createSsaoPipeline(GN_Vk.ssaoBlurRenderPass, GN_Vk.ssaoBlurPipelineLayout,
                       "assets/shaders/gn3d_ssao_blur.frag.spv", &GN_Vk.ssaoBlurPipeline);
    writeSsaoDescriptors();
}

bool isSsaoEnabled() {
    if(GN_Vk.giUBOMemory == VK_NULL_HANDLE) return false;
    float enabled = 0.0f; // GI UBO：indirect[4] 的 w（偏移 12）
    void *mapped = nullptr;
    vkMapMemory(GN_Vk.device, GN_Vk.giUBOMemory, 0, sizeof(GN3d::sGI_UBO), 0, &mapped);
    if(mapped) {
        memcpy(&enabled, (const char *)mapped + 12, sizeof(float));
        vkUnmapMemory(GN_Vk.device, GN_Vk.giUBOMemory);
    }
    return enabled > 0.5f;
}

void updateSsaoUniforms(const sMAT44 &viewProj) {
    if(GN_Vk.ssaoUBOMemory == VK_NULL_HANDLE) return;
    sMAT44 inverse;
    Mat44Inverse(&inverse, &viewProj);
    void *mapped = nullptr;
    vkMapMemory(GN_Vk.device, GN_Vk.ssaoUBOMemory, 0, sizeof(GN3d::sSSAO_UBO), 0, &mapped);
    if(mapped) {
        memcpy(mapped, viewProj.m, sizeof(float) * 16);
        memcpy((char *)mapped + 64, inverse.m, sizeof(float) * 16);
        vkUnmapMemory(GN_Vk.device, GN_Vk.ssaoUBOMemory);
    }
}

void recordSsaoPasses(VkCommandBuffer cb) {
    const VkClearValue clearValue = {{{1.0f, 1.0f, 1.0f, 1.0f}}}; // R8：1=无遮蔽
    VkRenderPassBeginInfo rpi = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    rpi.renderPass = GN_Vk.ssaoRenderPass;
    rpi.framebuffer = GN_Vk.ssaoFramebuffer;
    rpi.renderArea = {{0, 0}, GN_Vk.swapchainExtent};
    rpi.clearValueCount = 1;
    rpi.pClearValues = &clearValue;
    vkCmdBeginRenderPass(cb, &rpi, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, GN_Vk.ssaoPipeline);
    vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, GN_Vk.ssaoPipelineLayout, 0, 1,
                            &GN_Vk.ssaoDescriptorSet, 0, nullptr);
    vkCmdDraw(cb, 3, 1, 0, 0);
    vkCmdEndRenderPass(cb);
    // blur pass 读取 rawAO（其 finalLayout 已由 SSAO pass 的依赖转 SHADER_READ）
    rpi.renderPass = GN_Vk.ssaoBlurRenderPass;
    rpi.framebuffer = GN_Vk.ssaoBlurFramebuffer;
    vkCmdBeginRenderPass(cb, &rpi, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, GN_Vk.ssaoBlurPipeline);
    vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, GN_Vk.ssaoBlurPipelineLayout, 0, 1,
                            &GN_Vk.ssaoBlurDescriptorSet, 0, nullptr);
    vkCmdDraw(cb, 3, 1, 0, 0);
    vkCmdEndRenderPass(cb);
}

} // namespace GNg
