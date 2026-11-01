// vkBloom.cpp — L1d：HDR Bloom mip 链（13-tap 下采样 + 9-tap tent 上采样 + ONE/ONE 加法）。
//
// 流程（在 composite 之前录制，必须位于 3D/延迟 pass 之外、composite render pass 之前）：
//   1) 首级：HDR 目标 → 半分辨率，乘曝光 + 软阈值取亮部；
//   2) 逐级 13-tap 下采样到 1/N 分辨率；
//   3) 从最小级 9-tap tent 加法上采样回半分辨率；
//   4) composite 采样 level0 叠加（强度由 POST UBO.bloomIntensity 控制）。
// 顶点复用 gn3d_composite.vert（全屏三角形 + fragUV）。
#include <GN.h>
#include "GN_Vk.h"
#include "vkInternal.h"

#include <cstring>

namespace GNg {

namespace {

const uint32_t BLOOM_MIP_COUNT = 5; // 半分辨率再逐级减半

struct sBloomPush {
    float threshold; // <0 = 仅下采样（已曝光/已阈值）
    float knee;
    float exposure;
    float pad;
};

struct sBloomLevel {
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkFramebuffer downFb = VK_NULL_HANDLE;
    VkFramebuffer upFb = VK_NULL_HANDLE;
    VkExtent2D extent = {0, 0};
};

sBloomLevel s_levels[BLOOM_MIP_COUNT];
VkSampler s_sampler = VK_NULL_HANDLE;
VkDescriptorSetLayout s_setLayout = VK_NULL_HANDLE;
VkPipelineLayout s_pipelineLayout = VK_NULL_HANDLE;
VkRenderPass s_downPass = VK_NULL_HANDLE;
VkRenderPass s_upPass = VK_NULL_HANDLE;
VkDescriptorSet s_downSets[BLOOM_MIP_COUNT] = {};
VkDescriptorSet s_upSets[BLOOM_MIP_COUNT] = {}; // j 级用于“源 = level j+1、目标 = level j”
VkPipeline s_downPipe[BLOOM_MIP_COUNT] = {};
VkPipeline s_upPipe[BLOOM_MIP_COUNT] = {};
bool s_setsAllocated = false;
bool s_ready = false;

const VkFormat BLOOM_FORMAT = VK_FORMAT_R16G16B16A16_SFLOAT;

void destroyBloomTargets() {
    for(uint32_t i = 0; i < BLOOM_MIP_COUNT; ++i) {
        sBloomLevel &level = s_levels[i];
        if(level.downFb) vkDestroyFramebuffer(GN_Vk.device, level.downFb, 0);
        if(level.upFb) vkDestroyFramebuffer(GN_Vk.device, level.upFb, 0);
        if(level.view) vkDestroyImageView(GN_Vk.device, level.view, 0);
        if(level.image) vkDestroyImage(GN_Vk.device, level.image, 0);
        if(level.memory) vkFreeMemory(GN_Vk.device, level.memory, 0);
        level = sBloomLevel();
    }
    for(uint32_t i = 0; i < BLOOM_MIP_COUNT; ++i) {
        s_downPipe[i] = VK_NULL_HANDLE;
        s_upPipe[i] = VK_NULL_HANDLE;
    }
    s_ready = false;
}

bool createBloomImage(sBloomLevel &level, uint32_t width, uint32_t height) {
    level.extent = {width > 0 ? width : 1, height > 0 ? height : 1};
    VkImageCreateInfo ii = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    ii.imageType = VK_IMAGE_TYPE_2D;
    ii.extent = {level.extent.width, level.extent.height, 1};
    ii.mipLevels = 1;
    ii.arrayLayers = 1;
    ii.format = BLOOM_FORMAT;
    ii.tiling = VK_IMAGE_TILING_OPTIMAL;
    ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    ii.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    ii.samples = VK_SAMPLE_COUNT_1_BIT;
    ii.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if(vkCreateImage(GN_Vk.device, &ii, 0, &level.image) != VK_SUCCESS) return false;

    VkMemoryRequirements mr;
    vkGetImageMemoryRequirements(GN_Vk.device, level.image, &mr);
    VkMemoryAllocateInfo mai = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    mai.allocationSize = mr.size;
    mai.memoryTypeIndex = FindMemType(mr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if(vkAllocateMemory(GN_Vk.device, &mai, 0, &level.memory) != VK_SUCCESS) return false;
    vkBindImageMemory(GN_Vk.device, level.image, level.memory, 0);

    VkImageViewCreateInfo vi = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    vi.image = level.image;
    vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vi.format = BLOOM_FORMAT;
    vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    vi.subresourceRange.levelCount = 1;
    vi.subresourceRange.layerCount = 1;
    if(vkCreateImageView(GN_Vk.device, &vi, 0, &level.view) != VK_SUCCESS) return false;

    VkFramebufferCreateInfo fci = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    fci.renderPass = s_downPass;
    fci.attachmentCount = 1;
    fci.pAttachments = &level.view;
    fci.width = level.extent.width;
    fci.height = level.extent.height;
    fci.layers = 1;
    if(vkCreateFramebuffer(GN_Vk.device, &fci, 0, &level.downFb) != VK_SUCCESS) return false;

    fci.renderPass = s_upPass;
    if(vkCreateFramebuffer(GN_Vk.device, &fci, 0, &level.upFb) != VK_SUCCESS) return false;
    return true;
}

VkRenderPass createBloomPass(VkAttachmentLoadOp loadOp, VkImageLayout initialLayout) {
    VkAttachmentDescription ad = {};
    ad.format = BLOOM_FORMAT;
    ad.samples = VK_SAMPLE_COUNT_1_BIT;
    ad.loadOp = loadOp;
    ad.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    ad.initialLayout = initialLayout;
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
    VkRenderPass pass = VK_NULL_HANDLE;
    vkCreateRenderPass(GN_Vk.device, &rpi, 0, &pass);
    return pass;
}

bool createBloomLayouts() {
    VkDescriptorSetLayoutBinding binding = {};
    binding.binding = 1;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    VkDescriptorSetLayoutCreateInfo dslci = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    dslci.bindingCount = 1;
    dslci.pBindings = &binding;
    if(vkCreateDescriptorSetLayout(GN_Vk.device, &dslci, 0, &s_setLayout) != VK_SUCCESS) return false;

    VkPushConstantRange range = {};
    range.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    range.offset = 0;
    range.size = sizeof(sBloomPush);
    VkPipelineLayoutCreateInfo plci = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    plci.setLayoutCount = 1;
    plci.pSetLayouts = &s_setLayout;
    plci.pushConstantRangeCount = 1;
    plci.pPushConstantRanges = &range;
    return vkCreatePipelineLayout(GN_Vk.device, &plci, 0, &s_pipelineLayout) == VK_SUCCESS;
}

bool createBloomSampler() {
    VkSamplerCreateInfo sci = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    sci.magFilter = VK_FILTER_LINEAR;
    sci.minFilter = VK_FILTER_LINEAR;
    sci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    sci.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sci.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    return vkCreateSampler(GN_Vk.device, &sci, 0, &s_sampler) == VK_SUCCESS;
}

bool allocateBloomSets() {
    if(s_setsAllocated) return true;
    const uint32_t total = BLOOM_MIP_COUNT + (BLOOM_MIP_COUNT - 1);
    VkDescriptorSetLayout layouts[BLOOM_MIP_COUNT * 2] = {};
    for(uint32_t i = 0; i < total; ++i) layouts[i] = s_setLayout;
    VkDescriptorSetAllocateInfo dai = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    dai.descriptorPool = GN_Vk.descriptorPool;
    dai.descriptorSetCount = total;
    dai.pSetLayouts = layouts;
    VkDescriptorSet sets[BLOOM_MIP_COUNT * 2] = {};
    if(vkAllocateDescriptorSets(GN_Vk.device, &dai, sets) != VK_SUCCESS) return false;
    for(uint32_t i = 0; i < BLOOM_MIP_COUNT; ++i) s_downSets[i] = sets[i];
    for(uint32_t i = 0; i < BLOOM_MIP_COUNT - 1; ++i) s_upSets[i] = sets[BLOOM_MIP_COUNT + i];
    s_setsAllocated = true;
    return true;
}

void writeBloomDescriptors() {
    VkDescriptorImageInfo downInfo[BLOOM_MIP_COUNT] = {};
    VkDescriptorImageInfo upInfo[BLOOM_MIP_COUNT] = {};
    VkWriteDescriptorSet writes[BLOOM_MIP_COUNT * 2] = {};
    uint32_t count = 0;

    for(uint32_t i = 0; i < BLOOM_MIP_COUNT; ++i) {
        downInfo[i].sampler = s_sampler;
        downInfo[i].imageView = (i == 0) ? GN_Vk.hdrView : s_levels[i - 1].view;
        downInfo[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        writes[count].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[count].dstSet = s_downSets[i];
        writes[count].dstBinding = 1;
        writes[count].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[count].descriptorCount = 1;
        writes[count].pImageInfo = &downInfo[i];
        ++count;
    }
    for(uint32_t j = 0; j < BLOOM_MIP_COUNT - 1; ++j) {
        upInfo[j].sampler = s_sampler;
        upInfo[j].imageView = s_levels[j + 1].view;
        upInfo[j].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        writes[count].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[count].dstSet = s_upSets[j];
        writes[count].dstBinding = 1;
        writes[count].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[count].descriptorCount = 1;
        writes[count].pImageInfo = &upInfo[j];
        ++count;
    }
    vkUpdateDescriptorSets(GN_Vk.device, count, writes, 0, 0);
}

bool createBloomPipeline(VkPipeline *out, const char *fragPath, uint32_t levelIndex,
                         bool additiveBlend) {
    std::vector<uint32_t> vertSpv = LoadSPIRVFile("assets/shaders/gn3d_composite.vert.spv");
    std::vector<uint32_t> fragSpv = LoadSPIRVFile(fragPath);
    if(vertSpv.empty() || fragSpv.empty() || s_pipelineLayout == VK_NULL_HANDLE) return false;
    GN3d::sPIPELINE_DESC desc;
    desc.aKind = GN3d::ePIPE_GRAPHICS;
    desc.aVertSpv = vertSpv.data();
    desc.aVertSize = vertSpv.size() * sizeof(uint32_t);
    desc.aFragSpv = fragSpv.data();
    desc.aFragSize = fragSpv.size() * sizeof(uint32_t);
    desc.aRenderPass = additiveBlend ? s_upPass : s_downPass;
    desc.aSubpass = 0;
    desc.aLayout = s_pipelineLayout;
    desc.aDepthTest = false;
    desc.aDepthWrite = false;
    desc.aDepthStencil = false;
    desc.aCullBack = false;
    desc.aFrontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    desc.aBlend = additiveBlend ? GN3d::eBLEND_ADDITIVE : GN3d::eBLEND_NONE;
    desc.aTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    desc.aSamples = VK_SAMPLE_COUNT_1_BIT;
    desc.aColorAttachmentCount = 1;
    desc.aDynamicViewport = false;
    desc.aDynamicScissor = false;
    desc.aViewport = VkViewport{0.0f, 0.0f, (float)s_levels[levelIndex].extent.width,
                                (float)s_levels[levelIndex].extent.height, 0.0f, 1.0f};
    desc.aScissor = VkRect2D{{0, 0}, s_levels[levelIndex].extent};
    desc.aCustomBlend = false;
    *out = GN_Vk.aPipelineCache.GetOrCreate(desc);
    return *out != VK_NULL_HANDLE;
}

bool createBloomPipelines() {
    for(uint32_t i = 0; i < BLOOM_MIP_COUNT; ++i) {
        if(!createBloomPipeline(&s_downPipe[i], "assets/shaders/gn3d_bloom_down.frag.spv", i, false)) {
            return false;
        }
    }
    for(uint32_t j = 0; j < BLOOM_MIP_COUNT - 1; ++j) {
        if(!createBloomPipeline(&s_upPipe[j], "assets/shaders/gn3d_bloom_up.frag.spv", j, true)) {
            return false;
        }
    }
    return true;
}

} // namespace

bool init3DBloom() {
    if(s_downPass != VK_NULL_HANDLE) return true;
    if(!GN_Vk.device || GN_Vk.hdrView == VK_NULL_HANDLE) return false;
    s_downPass = createBloomPass(VK_ATTACHMENT_LOAD_OP_CLEAR, VK_IMAGE_LAYOUT_UNDEFINED);
    s_upPass = createBloomPass(VK_ATTACHMENT_LOAD_OP_LOAD, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    if(!s_downPass || !s_upPass || !createBloomLayouts() || !createBloomSampler() ||
       !allocateBloomSets()) {
        return false;
    }
    return recreate3DBloomTargets();
}

void destroy3DBloom() {
    destroyBloomTargets();
    if(s_upPass) vkDestroyRenderPass(GN_Vk.device, s_upPass, 0);
    if(s_downPass) vkDestroyRenderPass(GN_Vk.device, s_downPass, 0);
    s_upPass = VK_NULL_HANDLE;
    s_downPass = VK_NULL_HANDLE;
    if(s_pipelineLayout) vkDestroyPipelineLayout(GN_Vk.device, s_pipelineLayout, 0);
    if(s_setLayout) vkDestroyDescriptorSetLayout(GN_Vk.device, s_setLayout, 0);
    if(s_sampler) vkDestroySampler(GN_Vk.device, s_sampler, 0);
    s_pipelineLayout = VK_NULL_HANDLE;
    s_setLayout = VK_NULL_HANDLE;
    s_sampler = VK_NULL_HANDLE;
}

bool recreate3DBloomTargets() {
    destroyBloomTargets();
    if(s_downPass == VK_NULL_HANDLE || s_setLayout == VK_NULL_HANDLE) return false;
    const uint32_t halfWidth = GN_Vk.swapchainExtent.width > 1 ? GN_Vk.swapchainExtent.width / 2 : 1;
    const uint32_t halfHeight = GN_Vk.swapchainExtent.height > 1 ? GN_Vk.swapchainExtent.height / 2 : 1;
    uint32_t width = halfWidth;
    uint32_t height = halfHeight;
    for(uint32_t i = 0; i < BLOOM_MIP_COUNT; ++i) {
        if(!createBloomImage(s_levels[i], width, height)) {
            destroyBloomTargets();
            return false;
        }
        width = width > 1 ? width / 2 : 1;
        height = height > 1 ? height / 2 : 1;
    }
    writeBloomDescriptors();
    if(!createBloomPipelines()) {
        destroyBloomTargets();
        return false;
    }
    // composite binding 3 = bloom level0（sampler 结果图）。
    if(GN_Vk.compositeDescriptorSet != VK_NULL_HANDLE && s_levels[0].view != VK_NULL_HANDLE) {
        VkDescriptorImageInfo bi = {};
        bi.sampler = s_sampler;
        bi.imageView = s_levels[0].view;
        bi.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        VkWriteDescriptorSet w = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        w.dstSet = GN_Vk.compositeDescriptorSet;
        w.dstBinding = 3;
        w.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        w.descriptorCount = 1;
        w.pImageInfo = &bi;
        vkUpdateDescriptorSets(GN_Vk.device, 1, &w, 0, 0);
    }
    s_ready = true;
    return true;
}

bool isBloomReady() { return s_ready; }

void recordBloomPasses(VkCommandBuffer cb) {
    if(!s_ready || cb == VK_NULL_HANDLE) return;
    GN3d::sPOST_UBO post;
    GN_VkGetPost(&post);

    for(uint32_t i = 0; i < BLOOM_MIP_COUNT; ++i) {
        VkClearValue clear = {};
        clear.color = {{0.0f, 0.0f, 0.0f, 1.0f}};
        VkRenderPassBeginInfo rpi = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        rpi.renderPass = s_downPass;
        rpi.framebuffer = s_levels[i].downFb;
        rpi.renderArea = {{0, 0}, s_levels[i].extent};
        rpi.clearValueCount = 1;
        rpi.pClearValues = &clear;
        vkCmdBeginRenderPass(cb, &rpi, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, s_downPipe[i]);
        vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, s_pipelineLayout, 0, 1,
                                &s_downSets[i], 0, 0);
        sBloomPush pc;
        pc.threshold = (i == 0) ? post.bloomThreshold : -1.0f;
        pc.knee = post.bloomKnee;
        pc.exposure = post.exposure;
        pc.pad = 0.0f;
        vkCmdPushConstants(cb, s_pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);
        vkCmdDraw(cb, 3, 1, 0, 0);
        vkCmdEndRenderPass(cb);
    }

    for(int j = (int)BLOOM_MIP_COUNT - 2; j >= 0; --j) {
        VkRenderPassBeginInfo rpi = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        rpi.renderPass = s_upPass;
        rpi.framebuffer = s_levels[j].upFb;
        rpi.renderArea = {{0, 0}, s_levels[j].extent};
        rpi.clearValueCount = 0;
        rpi.pClearValues = nullptr;
        vkCmdBeginRenderPass(cb, &rpi, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, s_upPipe[j]);
        vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, s_pipelineLayout, 0, 1,
                                &s_upSets[j], 0, 0);
        sBloomPush pc = {-1.0f, 0.0f, 1.0f, 0.0f};
        vkCmdPushConstants(cb, s_pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);
        vkCmdDraw(cb, 3, 1, 0, 0);
        vkCmdEndRenderPass(cb);
    }
}

} // namespace GNg
