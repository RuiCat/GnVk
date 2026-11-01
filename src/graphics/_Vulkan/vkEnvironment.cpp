// 环境烘焙与 GI GPU 资源（G1a，环境来源方案 B）：
// GPU 6 面烘焙解析天空 → 回读 → CPU 预滤波（GGX mip 链 / SH9 / BRDF LUT）
// → 上传预滤波 cubemap、BRDF LUT 与 256 B GI UBO。G1b 负责着色器与描述符槽位接线。
#include <GN.h>
#include "GN_Vk.h"
#include "vkInternal.h"
#include "giParams.h"

#include <cstdio>
#include <cstring>
#include <vector>

namespace GNg {
namespace {

// 烘焙分辨率：128²×6。任务允许降到 64² 以省启动时间；先按 128 实现并在风险里登记
// CPU 预滤波（约 25M 次重要性采样）可能拖慢启动。mip 级数与 giParams 默认 envParams 一致。
const uint32_t ENV_BAKE_SIZE = 128;
const int ENV_MIP_COUNT = 5;
const int ENV_FACE_COUNT = 6;
const VkFormat ENV_BAKE_FORMAT = VK_FORMAT_R16G16B16A16_SFLOAT;
// 每面 UBO 占 256 B（128 B 数据 + 对齐填充）：minUniformBufferOffsetAlignment 上限为 256，
// 用 256 步长可保证 6 个描述符偏移在任意设备上都合法。
const uint32_t ENV_BAKE_UBO_STRIDE = 256;

// 每面 viewProj：GL 约定 lookAt（面片 up）+ Mat44PerspectiveVK（已含 Vulkan Y 翻转）。
// 面序固定 +X,-X,+Y,-Y,+Z,-Z，与 giParams 的 cubemap 面序及 Vulkan cube 采样约定一致。
// 无 GPU 无法运行验证，朝向（上下/左右）属于本任务首要未验证风险。
void buildFaceViewProj(int face, float outViewProj[16]) {
    static const float dirs[ENV_FACE_COUNT][3] = {
        {1.0f, 0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f},
        {0.0f, -1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, -1.0f}};
    static const float ups[ENV_FACE_COUNT][3] = {
        {0.0f, -1.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, {0.0f, 0.0f, 1.0f},
        {0.0f, 0.0f, -1.0f}, {0.0f, -1.0f, 0.0f}, {0.0f, -1.0f, 0.0f}};
    const float halfPi = 1.5707963267948966f;
    sMAT44 proj;
    Mat44PerspectiveVK(&proj, halfPi, 1.0f, 0.1f, 10.0f);
    sVEC3 eye = {0.0f, 0.0f, 0.0f};
    sVEC3 center = {dirs[face][0], dirs[face][1], dirs[face][2]};
    sVEC3 up = {ups[face][0], ups[face][1], ups[face][2]};
    sMAT44 view;
    Mat44LookAtRH(&view, &eye, &center, &up);
    sMAT44 viewProj;
    Mat44Mul(&viewProj, &proj, &view);
    memcpy(outViewProj, viewProj.m, sizeof(float) * 16);
}

// 一次性命令缓冲：录制完成后提交并等待 fence（不依赖队列空闲）。
VkCommandBuffer beginOneTimeCommands() {
    VkCommandBuffer cb = VK_NULL_HANDLE;
    VkCommandBufferAllocateInfo ai = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandPool = GN_Vk.commandPool;
    ai.commandBufferCount = 1;
    vkAllocateCommandBuffers(GN_Vk.device, &ai, &cb);
    VkCommandBufferBeginInfo bi = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cb, &bi);
    return cb;
}

bool endAndWaitCommands(VkCommandBuffer cb) {
    VkResult endResult = vkEndCommandBuffer(cb);
    VkFence fence = VK_NULL_HANDLE;
    VkFenceCreateInfo fci = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    VkResult fenceResult = vkCreateFence(GN_Vk.device, &fci, 0, &fence);
    VkResult submitResult = VK_ERROR_UNKNOWN;
    if(fenceResult == VK_SUCCESS) {
        VkSubmitInfo si = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
        si.commandBufferCount = 1;
        si.pCommandBuffers = &cb;
        submitResult = vkQueueSubmit(GN_Vk.graphicsQueue, 1, &si, fence);
        if(submitResult == VK_SUCCESS) {
            vkWaitForFences(GN_Vk.device, 1, &fence, VK_TRUE, UINT64_MAX);
        }
        vkDestroyFence(GN_Vk.device, fence, 0);
    }
    vkFreeCommandBuffers(GN_Vk.device, GN_Vk.commandPool, 1, &cb);
    if(endResult != VK_SUCCESS || fenceResult != VK_SUCCESS || submitResult != VK_SUCCESS) {
        fprintf(stderr, "[GN_Vk] 环境烘焙命令提交失败（end=%d fence=%d submit=%d）\n",
                (int)endResult, (int)fenceResult, (int)submitResult);
        return false;
    }
    return true;
}

// ── 烘焙目标：128²×6 RGBA16F cubemap、6 张 face 视图、专用 render pass + 6 个 framebuffer ──
bool createEnvBakeTargets() {
    const uint32_t size = ENV_BAKE_SIZE;
    VkImageCreateInfo ii = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    ii.imageType = VK_IMAGE_TYPE_2D;
    ii.extent = {size, size, 1};
    ii.mipLevels = 1;
    ii.arrayLayers = ENV_FACE_COUNT;
    ii.format = ENV_BAKE_FORMAT;
    ii.tiling = VK_IMAGE_TILING_OPTIMAL;
    ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    ii.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
               VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    ii.samples = VK_SAMPLE_COUNT_1_BIT;
    ii.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ii.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    if(vkCreateImage(GN_Vk.device, &ii, 0, &GN_Vk.envBakeImage) != VK_SUCCESS) return false;
    VkMemoryRequirements mr;
    vkGetImageMemoryRequirements(GN_Vk.device, GN_Vk.envBakeImage, &mr);
    VkMemoryAllocateInfo mai = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    mai.allocationSize = mr.size;
    mai.memoryTypeIndex = FindMemType(mr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if(vkAllocateMemory(GN_Vk.device, &mai, 0, &GN_Vk.envBakeMemory) != VK_SUCCESS) return false;
    vkBindImageMemory(GN_Vk.device, GN_Vk.envBakeImage, GN_Vk.envBakeMemory, 0);
    for(int face = 0; face < ENV_FACE_COUNT; face++) {
        VkImageViewCreateInfo vi = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        vi.image = GN_Vk.envBakeImage;
        vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
        vi.format = ENV_BAKE_FORMAT;
        vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        vi.subresourceRange.baseMipLevel = 0;
        vi.subresourceRange.levelCount = 1;
        vi.subresourceRange.baseArrayLayer = (uint32_t)face;
        vi.subresourceRange.layerCount = 1;
        if(vkCreateImageView(GN_Vk.device, &vi, 0, &GN_Vk.envBakeFaceViews[face]) != VK_SUCCESS) {
            return false;
        }
    }
    VkAttachmentDescription ad = {};
    ad.format = ENV_BAKE_FORMAT;
    ad.samples = VK_SAMPLE_COUNT_1_BIT;
    ad.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    ad.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    ad.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    ad.finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
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
    if(vkCreateRenderPass(GN_Vk.device, &rpi, 0, &GN_Vk.envBakeRenderPass) != VK_SUCCESS) {
        return false;
    }
    GN_Vk.envBakeFramebuffers = new VkFramebuffer[ENV_FACE_COUNT];
    for(int face = 0; face < ENV_FACE_COUNT; face++) {
        VkFramebufferCreateInfo fci = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        fci.renderPass = GN_Vk.envBakeRenderPass;
        fci.attachmentCount = 1;
        fci.pAttachments = &GN_Vk.envBakeFaceViews[face];
        fci.width = size;
        fci.height = size;
        fci.layers = 1;
        if(vkCreateFramebuffer(GN_Vk.device, &fci, 0, &GN_Vk.envBakeFramebuffers[face]) !=
           VK_SUCCESS) {
            return false;
        }
    }
    GN_Vk.envBakeSize = (int)ENV_BAKE_SIZE;
    return true;
}

// ── 烘焙描述符/管线：每面一个 144 B UBO（6×256 B 连续缓冲）+ 0/1/2/3 四槽，逐面绑定 ──
bool createEnvBakePipeline() {
    VkDescriptorSetLayoutBinding dslb[4] = {};
    dslb[0].binding = 0;
    dslb[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    dslb[0].descriptorCount = 1;
    dslb[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    // L4：1 保留（cubemap），2/3 = 透射率 / sky-view LUT（与天空管线同孔径）。
    for(int b = 1; b <= 3; ++b) {
        dslb[b].binding = (uint32_t)b;
        dslb[b].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        dslb[b].descriptorCount = 1;
        dslb[b].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    }
    VkDescriptorSetLayoutCreateInfo dslci = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    dslci.bindingCount = 4;
    dslci.pBindings = dslb;
    if(vkCreateDescriptorSetLayout(GN_Vk.device, &dslci, 0, &GN_Vk.envBakeDescriptorLayout) !=
       VK_SUCCESS) {
        return false;
    }
    VkPipelineLayoutCreateInfo plci = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    plci.setLayoutCount = 1;
    plci.pSetLayouts = &GN_Vk.envBakeDescriptorLayout;
    if(vkCreatePipelineLayout(GN_Vk.device, &plci, 0, &GN_Vk.envBakePipelineLayout) != VK_SUCCESS) {
        return false;
    }
    CreateBuf(ENV_FACE_COUNT * ENV_BAKE_UBO_STRIDE, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
              GN_Vk.envBakeUBOBuffer, GN_Vk.envBakeUBOMemory);
    if(GN_Vk.envBakeUBOBuffer == VK_NULL_HANDLE) return false;
    VkDescriptorSetAllocateInfo dai = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    dai.descriptorPool = GN_Vk.descriptorPool;
    dai.descriptorSetCount = 1;
    dai.pSetLayouts = &GN_Vk.envBakeDescriptorLayout;
    // L4：烘焙用同一套天空 shader ⇒ 必须绑定透射率 / sky-view LUT（2/3），否则采样未绑定描述符。
    VkImageView lutTransView = VK_NULL_HANDLE;
    VkImageView lutSkyView = VK_NULL_HANDLE;
    VkSampler lutTransSampler = VK_NULL_HANDLE;
    VkSampler lutSkySampler = VK_NULL_HANDLE;
    const bool lutReady = GN_VkGetAtmosphereLutViews(&lutTransView, &lutSkyView, &lutTransSampler,
                                                     &lutSkySampler);
    if(!lutReady) {
        fprintf(stderr, "[GN_Vk] 警告: 环境烘焙时大气 LUT 未就绪，天空烘焙回退解析路径（L4）\n");
    }
    VkDescriptorImageInfo lutPair[2] = {
        {lutTransSampler, lutTransView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
        {lutSkySampler, lutSkyView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL}};
    void *mapped = nullptr;
    vkMapMemory(GN_Vk.device, GN_Vk.envBakeUBOMemory, 0, ENV_FACE_COUNT * ENV_BAKE_UBO_STRIDE, 0, &mapped);
    for(int face = 0; face < ENV_FACE_COUNT; face++) {
        // 每面 144 B：viewProj(64 B) + VkSkyParams(80 B)；天空参数用默认值（G3 负责天气重烘）。
        VkSkyParams skyParams;
        uint8_t *slot = (uint8_t *)mapped + face * ENV_BAKE_UBO_STRIDE;
        buildFaceViewProj(face, (float *)slot);
        memcpy(slot + 64, &skyParams, sizeof(VkSkyParams));
        vkAllocateDescriptorSets(GN_Vk.device, &dai, &GN_Vk.envBakeDescriptorSets[face]);
        VkDescriptorBufferInfo dbi = {GN_Vk.envBakeUBOBuffer,
                                      (VkDeviceSize)(face * ENV_BAKE_UBO_STRIDE), 144};
        VkWriteDescriptorSet w[3] = {{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET},
                                     {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET},
                                     {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET}};
        w[0].dstSet = GN_Vk.envBakeDescriptorSets[face];
        w[0].dstBinding = 0;
        w[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        w[0].descriptorCount = 1;
        w[0].pBufferInfo = &dbi;
        vkUpdateDescriptorSets(GN_Vk.device, 1, w, 0, 0);
        if(lutReady) {
            for(int b = 0; b < 2; ++b) {
                w[b].dstSet = GN_Vk.envBakeDescriptorSets[face];
                w[b].dstBinding = 2u + (uint32_t)b;
                w[b].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                w[b].descriptorCount = 1;
                w[b].pImageInfo = &lutPair[b];
            }
            vkUpdateDescriptorSets(GN_Vk.device, 2, w, 0, 0);
        }
    }
    if(mapped) vkUnmapMemory(GN_Vk.device, GN_Vk.envBakeUBOMemory);
    auto svSpv = LoadSPIRVFile("assets/shaders/gn3d_skybox.vert.spv");
    auto sfSpv = LoadSPIRVFile("assets/shaders/gn3d_skybox.frag.spv");
    if(svSpv.empty() || sfSpv.empty()) {
        fprintf(stderr, "[GN_Vk] 环境烘焙缺少天空盒 SPIR-V（assets/shaders/gn3d_skybox.*.spv）\n");
        return false;
    }
    GN3d::sPIPELINE_DESC desc;
    desc.aKind = GN3d::ePIPE_GRAPHICS;
    desc.aVertSpv = svSpv.data();
    desc.aVertSize = svSpv.size() * sizeof(uint32_t);
    desc.aFragSpv = sfSpv.data();
    desc.aFragSize = sfSpv.size() * sizeof(uint32_t);
    desc.aRenderPass = GN_Vk.envBakeRenderPass;
    desc.aSubpass = 0;
    desc.aLayout = GN_Vk.envBakePipelineLayout;
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
    desc.aViewport = VkViewport{0.0f, 0.0f, (float)ENV_BAKE_SIZE, (float)ENV_BAKE_SIZE, 0.0f, 1.0f};
    desc.aScissor = VkRect2D{{0, 0}, {ENV_BAKE_SIZE, ENV_BAKE_SIZE}};
    desc.aCustomBlend = false;
    GN_Vk.envBakePipeline = GN_Vk.aPipelineCache.GetOrCreate(desc);
    if(!GN_Vk.envBakePipeline) {
        fprintf(stderr, "[GN_Vk] 环境烘焙管线创建失败: %s\n", GN_Vk.aPipelineCache.GetLastError());
        return false;
    }
    return true;
}

// ── 烘焙 + 回读：逐面渲染解析天空，拷回 CPU 得到 6×128² 线性 RGBA float ──
// G3：每次调用先用给定 sky 参数刷新 6 面 UBO 的天空段（viewProj 保持初始化时写入值），
// 因此同一套目标/管线/描述符可反复重烘（天气切换）。
bool bakeSkyCubemap(const VkSkyParams &skyParams, std::vector<float> &envFaces) {
    const uint32_t size = ENV_BAKE_SIZE;
    if(GN_Vk.envBakeUBOMemory != VK_NULL_HANDLE) {
        void *mapped = nullptr;
        vkMapMemory(GN_Vk.device, GN_Vk.envBakeUBOMemory, 0,
                    ENV_FACE_COUNT * ENV_BAKE_UBO_STRIDE, 0, &mapped);
        if(mapped) {
            for(int face = 0; face < ENV_FACE_COUNT; face++) {
                memcpy((uint8_t *)mapped + face * ENV_BAKE_UBO_STRIDE + 64, &skyParams,
                       sizeof(VkSkyParams));
            }
            vkUnmapMemory(GN_Vk.device, GN_Vk.envBakeUBOMemory);
        }
    }
    const VkDeviceSize faceBytes = (VkDeviceSize)size * size * 8; // RGBA16F = 8 B/px
    const VkDeviceSize totalBytes = faceBytes * ENV_FACE_COUNT;
    VkBuffer staging = VK_NULL_HANDLE;
    VkDeviceMemory stagingMem = VK_NULL_HANDLE;
    CreateBuf(totalBytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging,
              stagingMem);
    if(staging == VK_NULL_HANDLE) return false;
    VkCommandBuffer cb = beginOneTimeCommands();
    for(int face = 0; face < ENV_FACE_COUNT; face++) {
        VkClearValue cv;
        cv.color = {{0.0f, 0.0f, 0.0f, 1.0f}};
        VkRenderPassBeginInfo rpi = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        rpi.renderPass = GN_Vk.envBakeRenderPass;
        rpi.framebuffer = GN_Vk.envBakeFramebuffers[face];
        rpi.renderArea = {{0, 0}, {size, size}};
        rpi.clearValueCount = 1;
        rpi.pClearValues = &cv;
        vkCmdBeginRenderPass(cb, &rpi, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, GN_Vk.envBakePipeline);
        vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, GN_Vk.envBakePipelineLayout, 0,
                                1, &GN_Vk.envBakeDescriptorSets[face], 0, nullptr);
        vkCmdDraw(cb, 36, 1, 0, 0);
        vkCmdEndRenderPass(cb);
    }
    // render pass 写 → transfer 读
    {
        VkImageMemoryBarrier b = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        b.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        b.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.image = GN_Vk.envBakeImage;
        b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        b.subresourceRange.levelCount = 1;
        b.subresourceRange.layerCount = ENV_FACE_COUNT;
        b.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        b.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, 0, 0, 0, 1, &b);
    }
    VkBufferImageCopy regions[ENV_FACE_COUNT] = {};
    for(int face = 0; face < ENV_FACE_COUNT; face++) {
        regions[face].bufferOffset = faceBytes * (VkDeviceSize)face;
        regions[face].imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        regions[face].imageSubresource.mipLevel = 0;
        regions[face].imageSubresource.baseArrayLayer = (uint32_t)face;
        regions[face].imageSubresource.layerCount = 1;
        regions[face].imageExtent = {size, size, 1};
    }
    vkCmdCopyImageToBuffer(cb, GN_Vk.envBakeImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, staging,
                           ENV_FACE_COUNT, regions);
    // 采样布局留给 G3 重烘/调试使用
    {
        VkImageMemoryBarrier b = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        b.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        b.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.image = GN_Vk.envBakeImage;
        b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        b.subresourceRange.levelCount = 1;
        b.subresourceRange.layerCount = ENV_FACE_COUNT;
        b.srcAccessMask = 0;
        b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, 0, 0, 0, 1, &b);
    }
    const bool ok = endAndWaitCommands(cb);
    if(ok) {
        void *mapped = nullptr;
        vkMapMemory(GN_Vk.device, stagingMem, 0, totalBytes, 0, &mapped);
        const unsigned short *halfPixels = (const unsigned short *)mapped;
        envFaces.resize((size_t)ENV_FACE_COUNT * size * size * 4);
        const size_t floatCount = (size_t)ENV_FACE_COUNT * size * size * 4;
        for(size_t i = 0; i < floatCount; i++) {
            envFaces[i] = GN_ValueHalfToFloat(halfPixels[i]);
        }
        vkUnmapMemory(GN_Vk.device, stagingMem);
    }
    vkDestroyBuffer(GN_Vk.device, staging, 0);
    vkFreeMemory(GN_Vk.device, stagingMem, 0);
    return ok;
}

// ── CPU：GGX 预滤波（mip 主序 RGBA float）+ SH9；BRDF LUT 由 giParams 现成实现生成 ──
bool prefilterEnvironment(const std::vector<float> &envFaces, std::vector<float> &prefiltered,
                          float sh[GN3d::SH9_FLOAT_COUNT]) {
    const int floatCount = (int)GN3d::prefilterCubemapFloatCount((int)ENV_BAKE_SIZE, ENV_MIP_COUNT);
    if(floatCount <= 0) return false;
    prefiltered.assign((size_t)floatCount, 0.0f);
    if(!GN3d::prefilterCubemap(envFaces.data(), (int)ENV_BAKE_SIZE, 4, ENV_MIP_COUNT,
                               prefiltered.data())) {
        fprintf(stderr, "[GN_Vk] CPU 预滤波失败（face=%u mip=%d）\n", ENV_BAKE_SIZE, ENV_MIP_COUNT);
        return false;
    }
    if(!GN3d::shProjectCubemap(envFaces.data(), (int)ENV_BAKE_SIZE, 4, sh)) {
        fprintf(stderr, "[GN_Vk] SH9 投影失败\n");
        return false;
    }
    return true;
}

// 浮点数组 → RGBA16F/RG16F 半精度字节（每 float 一个 half，布局不变）。
std::vector<unsigned short> toHalfWords(const float *values, size_t count) {
    std::vector<unsigned short> halves(count);
    for(size_t i = 0; i < count; i++) halves[i] = GN_ValueFloatToHalf(values[i]);
    return halves;
}

// ── 预滤波 cubemap：图像/内存在这里创建（重烘时先建新图，成功后再换掉旧图）──
bool createPrefilteredImage(VkImage *outImage, VkDeviceMemory *outMemory, VkImageView *outView) {
    const uint32_t size = ENV_BAKE_SIZE;
    const uint32_t mipCount = (uint32_t)ENV_MIP_COUNT;
    VkImageCreateInfo ii = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    ii.imageType = VK_IMAGE_TYPE_2D;
    ii.extent = {size, size, 1};
    ii.mipLevels = mipCount;
    ii.arrayLayers = ENV_FACE_COUNT;
    ii.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    ii.tiling = VK_IMAGE_TILING_OPTIMAL;
    ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    ii.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    ii.samples = VK_SAMPLE_COUNT_1_BIT;
    ii.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ii.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    if(vkCreateImage(GN_Vk.device, &ii, 0, outImage) != VK_SUCCESS) return false;
    VkMemoryRequirements mr;
    vkGetImageMemoryRequirements(GN_Vk.device, *outImage, &mr);
    VkMemoryAllocateInfo mai = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    mai.allocationSize = mr.size;
    mai.memoryTypeIndex = FindMemType(mr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if(vkAllocateMemory(GN_Vk.device, &mai, 0, outMemory) != VK_SUCCESS) return false;
    vkBindImageMemory(GN_Vk.device, *outImage, *outMemory, 0);
    VkImageViewCreateInfo vi = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    vi.image = *outImage;
    vi.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
    vi.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    vi.subresourceRange.levelCount = mipCount;
    vi.subresourceRange.layerCount = ENV_FACE_COUNT;
    return vkCreateImageView(GN_Vk.device, &vi, 0, outView) == VK_SUCCESS;
}

// 把 CPU 预滤波数据（mip 主序、每级 6 面 RGBA float）上传到指定图像；失败返回 false。
bool uploadPrefilteredData(VkImage image, const std::vector<float> &prefiltered) {
    const uint32_t size = ENV_BAKE_SIZE;
    const uint32_t mipCount = (uint32_t)ENV_MIP_COUNT;
    const std::vector<unsigned short> halves = toHalfWords(prefiltered.data(), prefiltered.size());
    const VkDeviceSize bytes = (VkDeviceSize)halves.size() * 2;
    VkBuffer staging = VK_NULL_HANDLE;
    VkDeviceMemory stagingMem = VK_NULL_HANDLE;
    CreateBuf(bytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging,
              stagingMem);
    if(staging == VK_NULL_HANDLE) return false;
    void *mapped = nullptr;
    vkMapMemory(GN_Vk.device, stagingMem, 0, bytes, 0, &mapped);
    memcpy(mapped, halves.data(), (size_t)bytes);
    vkUnmapMemory(GN_Vk.device, stagingMem);
    VkCommandBuffer cb = beginOneTimeCommands();
    {
        VkImageMemoryBarrier b = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        b.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        b.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.image = image;
        b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        b.subresourceRange.levelCount = mipCount;
        b.subresourceRange.layerCount = ENV_FACE_COUNT;
        b.srcAccessMask = 0;
        b.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             0, 0, 0, 0, 0, 1, &b);
    }
    std::vector<VkBufferImageCopy> regions;
    for(uint32_t mip = 0; mip < mipCount; mip++) {
        const uint32_t levelSize = (size >> mip) > 0 ? (size >> mip) : 1;
        const size_t mipOffset = GN3d::prefilterCubemapMipOffset((int)size, (int)mipCount, (int)mip);
        for(uint32_t face = 0; face < ENV_FACE_COUNT; face++) {
            VkBufferImageCopy region = {};
            region.bufferOffset = (VkDeviceSize)(mipOffset + (size_t)face * levelSize * levelSize * 4) * 2;
            region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            region.imageSubresource.mipLevel = mip;
            region.imageSubresource.baseArrayLayer = face;
            region.imageSubresource.layerCount = 1;
            region.imageExtent = {levelSize, levelSize, 1};
            regions.push_back(region);
        }
    }
    vkCmdCopyBufferToImage(cb, staging, image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, (uint32_t)regions.size(),
                           regions.data());
    {
        VkImageMemoryBarrier b = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        b.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        b.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.image = image;
        b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        b.subresourceRange.levelCount = mipCount;
        b.subresourceRange.layerCount = ENV_FACE_COUNT;
        b.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, 0, 0, 0, 1, &b);
    }
    const bool ok = endAndWaitCommands(cb);
    vkDestroyBuffer(GN_Vk.device, staging, 0);
    vkFreeMemory(GN_Vk.device, stagingMem, 0);
    return ok;
}

// 初始化路径：建预滤波 cubemap + 采样器并上传（G3 重烘走 createPrefilteredImage/uploadPrefilteredData）。
bool uploadPrefilteredCubemap(const std::vector<float> &prefiltered) {
    VkSamplerCreateInfo sci = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    sci.magFilter = VK_FILTER_LINEAR;
    sci.minFilter = VK_FILTER_LINEAR;
    sci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sci.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sci.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sci.maxLod = (float)(ENV_MIP_COUNT - 1);
    if(vkCreateSampler(GN_Vk.device, &sci, 0, &GN_Vk.envPrefilteredSampler) != VK_SUCCESS) {
        return false;
    }
    if(!createPrefilteredImage(&GN_Vk.envPrefilteredImage, &GN_Vk.envPrefilteredMemory,
                               &GN_Vk.envPrefilteredView)) {
        return false;
    }
    if(!uploadPrefilteredData(GN_Vk.envPrefilteredImage, prefiltered)) return false;
    GN_Vk.envPrefilteredMipCount = ENV_MIP_COUNT;
    return true;
}

// ── 上传：128² BRDF LUT（RG16F，线性 + CLAMP）──
bool uploadBrdfLut() {
    const uint32_t size = (uint32_t)GN3d::BRDF_LUT_SIZE;
    std::vector<float> lut((size_t)size * size * 2);
    if(!GN3d::generateBrdfLut(lut.data(), (int)size, (int)size)) return false;
    VkImageCreateInfo ii = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    ii.imageType = VK_IMAGE_TYPE_2D;
    ii.extent = {size, size, 1};
    ii.mipLevels = 1;
    ii.arrayLayers = 1;
    ii.format = VK_FORMAT_R16G16_SFLOAT;
    ii.tiling = VK_IMAGE_TILING_OPTIMAL;
    ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    ii.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    ii.samples = VK_SAMPLE_COUNT_1_BIT;
    ii.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if(vkCreateImage(GN_Vk.device, &ii, 0, &GN_Vk.brdfLutImage) != VK_SUCCESS) return false;
    VkMemoryRequirements mr;
    vkGetImageMemoryRequirements(GN_Vk.device, GN_Vk.brdfLutImage, &mr);
    VkMemoryAllocateInfo mai = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    mai.allocationSize = mr.size;
    mai.memoryTypeIndex = FindMemType(mr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if(vkAllocateMemory(GN_Vk.device, &mai, 0, &GN_Vk.brdfLutMemory) != VK_SUCCESS) return false;
    vkBindImageMemory(GN_Vk.device, GN_Vk.brdfLutImage, GN_Vk.brdfLutMemory, 0);
    VkImageViewCreateInfo vi = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    vi.image = GN_Vk.brdfLutImage;
    vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vi.format = VK_FORMAT_R16G16_SFLOAT;
    vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    vi.subresourceRange.levelCount = 1;
    vi.subresourceRange.layerCount = 1;
    if(vkCreateImageView(GN_Vk.device, &vi, 0, &GN_Vk.brdfLutView) != VK_SUCCESS) return false;
    VkSamplerCreateInfo sci = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    sci.magFilter = VK_FILTER_LINEAR;
    sci.minFilter = VK_FILTER_LINEAR;
    sci.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sci.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    if(vkCreateSampler(GN_Vk.device, &sci, 0, &GN_Vk.brdfLutSampler) != VK_SUCCESS) return false;
    const std::vector<unsigned short> halves = toHalfWords(lut.data(), lut.size());
    const VkDeviceSize bytes = (VkDeviceSize)halves.size() * 2;
    VkBuffer staging = VK_NULL_HANDLE;
    VkDeviceMemory stagingMem = VK_NULL_HANDLE;
    CreateBuf(bytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging,
              stagingMem);
    if(staging == VK_NULL_HANDLE) return false;
    void *mapped = nullptr;
    vkMapMemory(GN_Vk.device, stagingMem, 0, bytes, 0, &mapped);
    memcpy(mapped, halves.data(), (size_t)bytes);
    vkUnmapMemory(GN_Vk.device, stagingMem);
    VkCommandBuffer cb = beginOneTimeCommands();
    {
        VkImageMemoryBarrier b = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        b.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        b.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.image = GN_Vk.brdfLutImage;
        b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        b.subresourceRange.levelCount = 1;
        b.subresourceRange.layerCount = 1;
        b.srcAccessMask = 0;
        b.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             0, 0, 0, 0, 0, 1, &b);
    }
    VkBufferImageCopy region = {};
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.layerCount = 1;
    region.imageExtent = {size, size, 1};
    vkCmdCopyBufferToImage(cb, staging, GN_Vk.brdfLutImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           1, &region);
    {
        VkImageMemoryBarrier b = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        b.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        b.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.image = GN_Vk.brdfLutImage;
        b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        b.subresourceRange.levelCount = 1;
        b.subresourceRange.layerCount = 1;
        b.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, 0, 0, 0, 1, &b);
    }
    const bool ok = endAndWaitCommands(cb);
    vkDestroyBuffer(GN_Vk.device, staging, 0);
    vkFreeMemory(GN_Vk.device, stagingMem, 0);
    return ok;
}

// ── GI UBO：256 B host-visible，默认值 + 真实 SH9 + 实际 mip 数 ──
bool createGiUbo(const float sh[GN3d::SH9_FLOAT_COUNT]) {
    CreateBuf(sizeof(GN3d::sGI_UBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
              GN_Vk.giUBOBuffer, GN_Vk.giUBOMemory);
    if(GN_Vk.giUBOBuffer == VK_NULL_HANDLE) return false;
    GN3d::sGI_UBO ubo = GN3d::makeDefaultGiUbo();
    memcpy(ubo.sh, sh, sizeof(ubo.sh));
    ubo.envParams[1] = (float)ENV_MIP_COUNT; // 预滤波 mip 数
    void *mapped = nullptr;
    vkMapMemory(GN_Vk.device, GN_Vk.giUBOMemory, 0, sizeof(GN3d::sGI_UBO), 0, &mapped);
    memcpy(mapped, &ubo, sizeof(ubo));
    vkUnmapMemory(GN_Vk.device, GN_Vk.giUBOMemory);
    return true;
}

// ── G3：重烘成功后把 PBR/deferred 的预滤波 cubemap 槽位指向新视图 ──
void updateEnvironmentDescriptors(VkImageView view) {
    VkDescriptorImageInfo info = {GN_Vk.envPrefilteredSampler, view,
                                  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    VkWriteDescriptorSet w[2] = {{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET},
                                 {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET}};
    w[0].dstSet = GN_Vk.descriptorSetPBR;
    w[0].dstBinding = 8; // G1b：PBR 预滤波 cubemap
    w[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    w[0].descriptorCount = 1;
    w[0].pImageInfo = &info;
    w[1].dstSet = GN_Vk.deferredLightDescriptorSet;
    w[1].dstBinding = 10; // G1b：延迟预滤波 cubemap
    w[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    w[1].descriptorCount = 1;
    w[1].pImageInfo = &info;
    if(GN_Vk.descriptorSetPBR) vkUpdateDescriptorSets(GN_Vk.device, 1, &w[0], 0, 0);
    if(GN_Vk.deferredLightDescriptorSet) vkUpdateDescriptorSets(GN_Vk.device, 1, &w[1], 0, 0);
}

} // namespace

// 初始化入口：GN_VkInit3D 在天空盒之后调用一次。任何一步失败只告警，不阻断 3D 初始化。
// 重烘分辨率与启动统一为 128²（复用同一套 bake 目标；CPU 预滤波约 25M 次采样/次，
// 只在天气切换完成后触发，启动成本已知风险见 G1a）。
void init3DEnvironment() {
    if(GN_Vk.envBakeImage != VK_NULL_HANDLE) return; // 幂等
    if(!createEnvBakeTargets() || !createEnvBakePipeline()) {
        fprintf(stderr, "[GN_Vk] 警告: 环境烘焙目标/管线不可用（G1a）\n");
        return;
    }
    std::vector<float> envFaces;
    if(!bakeSkyCubemap(VkSkyParams(), envFaces)) {
        fprintf(stderr, "[GN_Vk] 警告: 环境 6 面烘焙回读失败（G1a）\n");
        return;
    }
    std::vector<float> prefiltered;
    float sh[GN3d::SH9_FLOAT_COUNT] = {};
    if(!prefilterEnvironment(envFaces, prefiltered, sh)) return;
    if(!uploadPrefilteredCubemap(prefiltered)) {
        fprintf(stderr, "[GN_Vk] 警告: 预滤波 cubemap 上传失败（G1a）\n");
    }
    if(!uploadBrdfLut()) fprintf(stderr, "[GN_Vk] 警告: BRDF LUT 上传失败（G1a）\n");
    if(!createGiUbo(sh)) fprintf(stderr, "[GN_Vk] 警告: GI UBO 创建失败（G1a）\n");
}

// G3 重烘主体：用给定天空参数重烘 6 面 → CPU 预滤波/SH9 → 新建并上传预滤波 cubemap
// → 全部成功后才换句柄/重写描述符/更新 SH9（失败时旧图与旧 SH9 原样保留）。
bool bakeAndUploadEnvironment(const VkSkyParams &skyParams) {
    std::vector<float> envFaces;
    if(!bakeSkyCubemap(skyParams, envFaces)) return false;
    std::vector<float> prefiltered;
    float sh[GN3d::SH9_FLOAT_COUNT] = {};
    if(!prefilterEnvironment(envFaces, prefiltered, sh)) return false;
    VkImage newImage = VK_NULL_HANDLE;
    VkDeviceMemory newMemory = VK_NULL_HANDLE;
    VkImageView newView = VK_NULL_HANDLE;
    if(!createPrefilteredImage(&newImage, &newMemory, &newView)) return false;
    if(!uploadPrefilteredData(newImage, prefiltered)) {
        vkDestroyImageView(GN_Vk.device, newView, 0);
        vkDestroyImage(GN_Vk.device, newImage, 0);
        vkFreeMemory(GN_Vk.device, newMemory, 0);
        return false;
    }
    const VkImage oldImage = GN_Vk.envPrefilteredImage;
    const VkDeviceMemory oldMemory = GN_Vk.envPrefilteredMemory;
    const VkImageView oldView = GN_Vk.envPrefilteredView;
    GN_Vk.envPrefilteredImage = newImage;
    GN_Vk.envPrefilteredMemory = newMemory;
    GN_Vk.envPrefilteredView = newView;
    GN_Vk.envPrefilteredMipCount = ENV_MIP_COUNT;
    updateEnvironmentDescriptors(newView);
    // GI UBO 只更新 SH9（偏移 48，144 B）；indirect/aoParams/invViewProj 保留当前值。
    if(GN_Vk.giUBOMemory != VK_NULL_HANDLE) {
        void *mapped = nullptr;
        vkMapMemory(GN_Vk.device, GN_Vk.giUBOMemory, 0, sizeof(GN3d::sGI_UBO), 0, &mapped);
        if(mapped) {
            memcpy((char *)mapped + offsetof(GN3d::sGI_UBO, sh), sh, sizeof(sh));
            vkUnmapMemory(GN_Vk.device, GN_Vk.giUBOMemory);
        }
    }
    if(oldView) vkDestroyImageView(GN_Vk.device, oldView, 0);
    if(oldImage) vkDestroyImage(GN_Vk.device, oldImage, 0);
    if(oldMemory) vkFreeMemory(GN_Vk.device, oldMemory, 0);
    return true;
}

// 公开入口：天气切换后按需重烘（调用方保证在 3D pass 之外；内部再兜底检查）。
bool GN_VkRebakeEnvironment() {
    if(!GN_Vk.initialized || GN_Vk.envBakeImage == VK_NULL_HANDLE ||
       GN_Vk.envBakePipeline == VK_NULL_HANDLE || GN_Vk.envPrefilteredImage == VK_NULL_HANDLE) {
        return false;
    }
    if(s_in3DPass || s_inRenderPass) return false; // 不允许在 pass 内重烘
    vkDeviceWaitIdle(GN_Vk.device);
    VkSkyParams sky;
    GN_VkGetSky(&sky);
    return bakeAndUploadEnvironment(sky);
}

// 释放 G1a 资源：vk3dShutdown 在设备仍有效时调用。
void destroy3DEnvironment() {
    if(GN_Vk.giUBOBuffer) vkDestroyBuffer(GN_Vk.device, GN_Vk.giUBOBuffer, 0);
    if(GN_Vk.giUBOMemory) vkFreeMemory(GN_Vk.device, GN_Vk.giUBOMemory, 0);
    GN_Vk.giUBOBuffer = VK_NULL_HANDLE;
    GN_Vk.giUBOMemory = VK_NULL_HANDLE;
    if(GN_Vk.brdfLutSampler) vkDestroySampler(GN_Vk.device, GN_Vk.brdfLutSampler, 0);
    if(GN_Vk.brdfLutView) vkDestroyImageView(GN_Vk.device, GN_Vk.brdfLutView, 0);
    if(GN_Vk.brdfLutImage) vkDestroyImage(GN_Vk.device, GN_Vk.brdfLutImage, 0);
    if(GN_Vk.brdfLutMemory) vkFreeMemory(GN_Vk.device, GN_Vk.brdfLutMemory, 0);
    GN_Vk.brdfLutSampler = VK_NULL_HANDLE;
    GN_Vk.brdfLutView = VK_NULL_HANDLE;
    GN_Vk.brdfLutImage = VK_NULL_HANDLE;
    GN_Vk.brdfLutMemory = VK_NULL_HANDLE;
    if(GN_Vk.envPrefilteredSampler) vkDestroySampler(GN_Vk.device, GN_Vk.envPrefilteredSampler, 0);
    if(GN_Vk.envPrefilteredView) vkDestroyImageView(GN_Vk.device, GN_Vk.envPrefilteredView, 0);
    if(GN_Vk.envPrefilteredImage) vkDestroyImage(GN_Vk.device, GN_Vk.envPrefilteredImage, 0);
    if(GN_Vk.envPrefilteredMemory) vkFreeMemory(GN_Vk.device, GN_Vk.envPrefilteredMemory, 0);
    GN_Vk.envPrefilteredSampler = VK_NULL_HANDLE;
    GN_Vk.envPrefilteredView = VK_NULL_HANDLE;
    GN_Vk.envPrefilteredImage = VK_NULL_HANDLE;
    GN_Vk.envPrefilteredMemory = VK_NULL_HANDLE;
    GN_Vk.envPrefilteredMipCount = 0;
    if(GN_Vk.envBakeFramebuffers) {
        for(int face = 0; face < ENV_FACE_COUNT; face++) {
            if(GN_Vk.envBakeFramebuffers[face]) {
                vkDestroyFramebuffer(GN_Vk.device, GN_Vk.envBakeFramebuffers[face], 0);
            }
        }
        delete[] GN_Vk.envBakeFramebuffers;
        GN_Vk.envBakeFramebuffers = nullptr;
    }
    if(GN_Vk.envBakeRenderPass) vkDestroyRenderPass(GN_Vk.device, GN_Vk.envBakeRenderPass, 0);
    GN_Vk.envBakePipeline = VK_NULL_HANDLE; // 归 aPipelineCache
    if(GN_Vk.envBakePipelineLayout) {
        vkDestroyPipelineLayout(GN_Vk.device, GN_Vk.envBakePipelineLayout, 0);
    }
    if(GN_Vk.envBakeDescriptorLayout) {
        vkDestroyDescriptorSetLayout(GN_Vk.device, GN_Vk.envBakeDescriptorLayout, 0);
    }
    if(GN_Vk.envBakeUBOBuffer) vkDestroyBuffer(GN_Vk.device, GN_Vk.envBakeUBOBuffer, 0);
    if(GN_Vk.envBakeUBOMemory) vkFreeMemory(GN_Vk.device, GN_Vk.envBakeUBOMemory, 0);
    for(int face = 0; face < ENV_FACE_COUNT; face++) {
        if(GN_Vk.envBakeFaceViews[face]) {
            vkDestroyImageView(GN_Vk.device, GN_Vk.envBakeFaceViews[face], 0);
        }
        GN_Vk.envBakeFaceViews[face] = VK_NULL_HANDLE;
        GN_Vk.envBakeDescriptorSets[face] = VK_NULL_HANDLE; // 随描述符池释放
    }
    if(GN_Vk.envBakeImage) vkDestroyImage(GN_Vk.device, GN_Vk.envBakeImage, 0);
    if(GN_Vk.envBakeMemory) vkFreeMemory(GN_Vk.device, GN_Vk.envBakeMemory, 0);
    GN_Vk.envBakeImage = VK_NULL_HANDLE;
    GN_Vk.envBakeMemory = VK_NULL_HANDLE;
    GN_Vk.envBakeRenderPass = VK_NULL_HANDLE;
    GN_Vk.envBakePipelineLayout = VK_NULL_HANDLE;
    GN_Vk.envBakeDescriptorLayout = VK_NULL_HANDLE;
    GN_Vk.envBakeUBOBuffer = VK_NULL_HANDLE;
    GN_Vk.envBakeUBOMemory = VK_NULL_HANDLE;
}

// 运行期更新 GI UBO（G1b/调用方使用）：直接写 host-visible 256 B 缓冲。
void GN_VkSetGI(const GN3d::sGI_UBO &ubo) {
    if(!GN_Vk.initialized || GN_Vk.giUBOMemory == VK_NULL_HANDLE) return;
    void *mapped = nullptr;
    vkMapMemory(GN_Vk.device, GN_Vk.giUBOMemory, 0, sizeof(GN3d::sGI_UBO), 0, &mapped);
    if(mapped) {
        memcpy(mapped, &ubo, sizeof(ubo));
        vkUnmapMemory(GN_Vk.device, GN_Vk.giUBOMemory);
    }
}

} // namespace GNg
