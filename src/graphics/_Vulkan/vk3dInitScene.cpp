// 3D 初始化（场景相关）：HDR/合成、天空盒、G-Buffer、延迟光照。
#include <GN.h>
#include "GN_Vk.h"
#include "vkInternal.h"

namespace GNg {

void init3DHdr() {
    // HDR 目标图像/视图/采样器（resize 复用同一 helper）。
    create3DHdrImages();

    // ── 3D HDR RenderPass (3 附件: MSAA Color + MSAA Depth + HDR Resolve) ──
    // 与 renderPass3D 结构完全相同, 仅 resolve 附件格式不同
    {
        VkAttachmentDescription hds[3] = {};
        hds[0].format = GN_Vk.swapchainFormat;
        hds[0].samples = GN_Vk.msaaSamples;
        hds[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        hds[0].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        hds[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        hds[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        hds[1].format = GN_Vk.depthFormat;
        hds[1].samples = GN_Vk.msaaSamples;
        hds[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        hds[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        hds[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        hds[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        hds[2].format = VK_FORMAT_R16G16B16A16_SFLOAT;
        hds[2].samples = VK_SAMPLE_COUNT_1_BIT;
        hds[2].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        hds[2].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        hds[2].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        hds[2].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        VkAttachmentReference hcr = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        VkAttachmentReference hdr = {1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
        VkAttachmentReference hrr = {2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        VkSubpassDescription hsp = {};
        hsp.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        hsp.colorAttachmentCount = 1;
        hsp.pColorAttachments = &hcr;
        hsp.pResolveAttachments = &hrr;
        hsp.pDepthStencilAttachment = &hdr;
        VkRenderPassCreateInfo hrpi = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
        hrpi.attachmentCount = 3;
        hrpi.pAttachments = hds;
        hrpi.subpassCount = 1;
        hrpi.pSubpasses = &hsp;
        vkCreateRenderPass(GN_Vk.device, &hrpi, 0, &GN_Vk.hdrRenderPass);
    }
    // HDR framebuffers（引用 MSAA/depth/HDR view；resize 走同一 helper）
    create3DHdrFramebuffers();

    // ── 编译合成着色器 ──
    auto compVertSpv = LoadSPIRVFile("assets/shaders/gn3d_composite.vert.spv");
    auto compFragSpv = LoadSPIRVFile("assets/shaders/gn3d_composite.frag.spv");
    if(!compVertSpv.empty() && !compFragSpv.empty()) {
        // 描述符布局: Binding 1 = HDR sampler (Fragment)
        {
            VkDescriptorSetLayoutBinding dslb[1] = {};
            dslb[0].binding = 1;
            dslb[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            dslb[0].descriptorCount = 1;
            dslb[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
            VkDescriptorSetLayoutCreateInfo dslci = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
            dslci.bindingCount = 1;
            dslci.pBindings = dslb;
            vkCreateDescriptorSetLayout(GN_Vk.device, &dslci, 0, &GN_Vk.compositeDescriptorLayout);
        }
        // 管线布局
        {
            VkPipelineLayoutCreateInfo plci = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
            plci.setLayoutCount = 1;
            plci.pSetLayouts = &GN_Vk.compositeDescriptorLayout;
            vkCreatePipelineLayout(GN_Vk.device, &plci, 0, &GN_Vk.compositePipelineLayout);
        }

        // 管线（P1b）：全屏三角形、无顶点输入；描述在 renderPass 建好后交给管线缓存

        // Composite RenderPass: swapchain 格式, 单采样
        {
            VkAttachmentDescription cad = {};
            cad.format = GN_Vk.swapchainFormat;
            cad.samples = VK_SAMPLE_COUNT_1_BIT;
            cad.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            cad.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            cad.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            cad.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            VkAttachmentReference car = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
            VkSubpassDescription csp = {};
            csp.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
            csp.colorAttachmentCount = 1;
            csp.pColorAttachments = &car;
            VkRenderPassCreateInfo crpi = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
            crpi.attachmentCount = 1;
            crpi.pAttachments = &cad;
            crpi.subpassCount = 1;
            crpi.pSubpasses = &csp;
            vkCreateRenderPass(GN_Vk.device, &crpi, 0, &GN_Vk.compositeRenderPass);
        }
        create3DCompositeFramebuffers();

        // 合成管线（固定视口；resize 复用同一 helper）
        create3DCompositePipeline();

        // 分配描述符集并绑定 HDR 纹理（resize 时用 writeCompositeHdrBinding 重写）
        {
            VkDescriptorSetAllocateInfo dai = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
            dai.descriptorPool = GN_Vk.descriptorPool;
            dai.descriptorSetCount = 1;
            dai.pSetLayouts = &GN_Vk.compositeDescriptorLayout;
            vkAllocateDescriptorSets(GN_Vk.device, &dai, &GN_Vk.compositeDescriptorSet);
            writeCompositeHdrBinding();
        }
    }
}

// ── 可重入的 HDR/合成资源（GN_VkResize 与 GN_VkShutdown3D 共用）──
void create3DHdrImages() {
    const uint32_t sw = GN_Vk.swapchainExtent.width;
    const uint32_t sh = GN_Vk.swapchainExtent.height;
    {
        VkImageCreateInfo ii = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        ii.imageType = VK_IMAGE_TYPE_2D;
        ii.extent = {sw, sh, 1};
        ii.mipLevels = 1;
        ii.arrayLayers = 1;
        ii.format = VK_FORMAT_R16G16B16A16_SFLOAT;
        ii.tiling = VK_IMAGE_TILING_OPTIMAL;
        ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        ii.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        ii.samples = VK_SAMPLE_COUNT_1_BIT;
        ii.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if(vkCreateImage(GN_Vk.device, &ii, 0, &GN_Vk.hdrImage) != VK_SUCCESS) return;
        VkMemoryRequirements hmr;
        vkGetImageMemoryRequirements(GN_Vk.device, GN_Vk.hdrImage, &hmr);
        VkMemoryAllocateInfo hmai = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        hmai.allocationSize = hmr.size;
        hmai.memoryTypeIndex = FindMemType(hmr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if(vkAllocateMemory(GN_Vk.device, &hmai, 0, &GN_Vk.hdrMemory) != VK_SUCCESS) return;
        vkBindImageMemory(GN_Vk.device, GN_Vk.hdrImage, GN_Vk.hdrMemory, 0);
    }
    {
        VkImageViewCreateInfo vi = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        vi.image = GN_Vk.hdrImage;
        vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
        vi.format = VK_FORMAT_R16G16B16A16_SFLOAT;
        vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        vi.subresourceRange.levelCount = 1;
        vi.subresourceRange.layerCount = 1;
        vkCreateImageView(GN_Vk.device, &vi, 0, &GN_Vk.hdrView);
    }
    {
        VkSamplerCreateInfo sci = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        sci.magFilter = VK_FILTER_LINEAR;
        sci.minFilter = VK_FILTER_LINEAR;
        sci.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sci.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        vkCreateSampler(GN_Vk.device, &sci, 0, &GN_Vk.hdrSampler);
    }
}

void create3DHdrFramebuffers() {
    if(GN_Vk.hdrRenderPass == VK_NULL_HANDLE || GN_Vk.hdrView == VK_NULL_HANDLE) return;
    GN_Vk.hdrFramebuffers = new VkFramebuffer[GN_Vk.swapchainImageCount];
    for(uint32_t i = 0; i < GN_Vk.swapchainImageCount; i++) {
        GN_Vk.hdrFramebuffers[i] = VK_NULL_HANDLE;
        VkImageView hatts[3] = {GN_Vk.msaaColorView, GN_Vk.depthView, GN_Vk.hdrView};
        VkFramebufferCreateInfo fci = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        fci.renderPass = GN_Vk.hdrRenderPass;
        fci.attachmentCount = 3;
        fci.pAttachments = hatts;
        fci.width = GN_Vk.swapchainExtent.width;
        fci.height = GN_Vk.swapchainExtent.height;
        fci.layers = 1;
        vkCreateFramebuffer(GN_Vk.device, &fci, 0, &GN_Vk.hdrFramebuffers[i]);
    }
}

void destroy3DHdrTargets() {
    if(GN_Vk.hdrFramebuffers) {
        for(uint32_t i = 0; i < GN_Vk.swapchainImageCount; i++) {
            if(GN_Vk.hdrFramebuffers[i]) vkDestroyFramebuffer(GN_Vk.device, GN_Vk.hdrFramebuffers[i], 0);
        }
        delete[] GN_Vk.hdrFramebuffers;
        GN_Vk.hdrFramebuffers = nullptr;
    }
    if(GN_Vk.hdrSampler) {
        vkDestroySampler(GN_Vk.device, GN_Vk.hdrSampler, 0);
        GN_Vk.hdrSampler = VK_NULL_HANDLE;
    }
    if(GN_Vk.hdrView) {
        vkDestroyImageView(GN_Vk.device, GN_Vk.hdrView, 0);
        GN_Vk.hdrView = VK_NULL_HANDLE;
    }
    if(GN_Vk.hdrImage) {
        vkDestroyImage(GN_Vk.device, GN_Vk.hdrImage, 0);
        GN_Vk.hdrImage = VK_NULL_HANDLE;
    }
    if(GN_Vk.hdrMemory) {
        vkFreeMemory(GN_Vk.device, GN_Vk.hdrMemory, 0);
        GN_Vk.hdrMemory = VK_NULL_HANDLE;
    }
}

void recreate3DHdrTargets() {
    destroy3DHdrTargets();
    create3DHdrImages();
    create3DHdrFramebuffers();
}

// 合成描述符：binding 1 = HDR 纹理（resize 换新 hdrView 后重写）。
void writeCompositeHdrBinding() {
    if(GN_Vk.compositeDescriptorSet == VK_NULL_HANDLE || GN_Vk.hdrView == VK_NULL_HANDLE) return;
    VkDescriptorImageInfo hii = {GN_Vk.hdrSampler, GN_Vk.hdrView,
                                 VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    VkWriteDescriptorSet wc = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    wc.dstSet = GN_Vk.compositeDescriptorSet;
    wc.dstBinding = 1;
    wc.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    wc.descriptorCount = 1;
    wc.pImageInfo = &hii;
    vkUpdateDescriptorSets(GN_Vk.device, 1, &wc, 0, 0);
}

// 合成管线（固定视口；resize 按新 extent 重建；需要 assets 下的 SPIR-V）。
void create3DCompositePipeline() {
    auto compVertSpv = LoadSPIRVFile("assets/shaders/gn3d_composite.vert.spv");
    auto compFragSpv = LoadSPIRVFile("assets/shaders/gn3d_composite.frag.spv");
    if(compVertSpv.empty() || compFragSpv.empty() || GN_Vk.compositeRenderPass == VK_NULL_HANDLE ||
       GN_Vk.compositePipelineLayout == VK_NULL_HANDLE) {
        return;
    }
    GN3d::sPIPELINE_DESC descComp;
    descComp.aKind = GN3d::ePIPE_GRAPHICS;
    descComp.aVertSpv = compVertSpv.data();
    descComp.aVertSize = compVertSpv.size() * sizeof(uint32_t);
    descComp.aFragSpv = compFragSpv.data();
    descComp.aFragSize = compFragSpv.size() * sizeof(uint32_t);
    descComp.aRenderPass = GN_Vk.compositeRenderPass;
    descComp.aSubpass = 0;
    descComp.aLayout = GN_Vk.compositePipelineLayout;
    descComp.aDepthTest = false;
    descComp.aDepthWrite = false;
    descComp.aDepthStencil = false;
    descComp.aCullBack = false;
    descComp.aFrontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    descComp.aBlend = GN3d::eBLEND_NONE;
    descComp.aTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    descComp.aSamples = VK_SAMPLE_COUNT_1_BIT;
    descComp.aColorAttachmentCount = 1;
    descComp.aDynamicViewport = false;
    descComp.aDynamicScissor = false;
    descComp.aViewport = VkViewport{0.0f, 0.0f, (float)GN_Vk.swapchainExtent.width,
                                    (float)GN_Vk.swapchainExtent.height, 0.0f, 1.0f};
    descComp.aScissor = VkRect2D{{0, 0}, GN_Vk.swapchainExtent};
    descComp.aCustomBlend = false;
    GN_Vk.compositePipeline = GN_Vk.aPipelineCache.GetOrCreate(descComp);
    if(!GN_Vk.compositePipeline) {
        fprintf(stderr, "[GN_Vk] 合成管线创建失败: %s\n", GN_Vk.aPipelineCache.GetLastError());
    }
}

void recreate3DCompositeTargets() {
    destroy3DCompositeFramebuffers();
    create3DCompositeFramebuffers();
    writeCompositeHdrBinding();
    create3DCompositePipeline();
}

// 天空盒：程序化 Cubemap（6 面）、IBL 描述符绑定与天空盒管线。
void init3DSkybox() {
    uint32_t sw = GN_Vk.swapchainExtent.width;
    uint32_t sh = GN_Vk.swapchainExtent.height;
    {
        const int smSize = 256;
        // 创建 cubemap image
        {
            VkImageCreateInfo ii = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
            ii.imageType = VK_IMAGE_TYPE_2D;
            ii.extent = {(uint32_t)smSize, (uint32_t)smSize, 1};
            ii.mipLevels = 1;
            ii.arrayLayers = 6;
            ii.format = VK_FORMAT_R8G8B8A8_UNORM;
            ii.tiling = VK_IMAGE_TILING_OPTIMAL;
            ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            ii.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
            ii.samples = VK_SAMPLE_COUNT_1_BIT;
            ii.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            ii.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
            vkCreateImage(GN_Vk.device, &ii, 0, &GN_Vk.skyboxImage);
            VkMemoryRequirements mr;
            vkGetImageMemoryRequirements(GN_Vk.device, GN_Vk.skyboxImage, &mr);
            VkMemoryAllocateInfo mai = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
            mai.allocationSize = mr.size;
            mai.memoryTypeIndex = FindMemType(mr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
            vkAllocateMemory(GN_Vk.device, &mai, 0, &GN_Vk.skyboxMemory);
            vkBindImageMemory(GN_Vk.device, GN_Vk.skyboxImage, GN_Vk.skyboxMemory, 0);
        }
        // Image view (cubemap)
        {
            VkImageViewCreateInfo vi = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
            vi.image = GN_Vk.skyboxImage;
            vi.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
            vi.format = VK_FORMAT_R8G8B8A8_UNORM;
            vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            vi.subresourceRange.levelCount = 1;
            vi.subresourceRange.layerCount = 6;
            vkCreateImageView(GN_Vk.device, &vi, 0, &GN_Vk.skyboxView);
        }
        // Sampler (linear, clamp)
        {
            VkSamplerCreateInfo sci = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
            sci.magFilter = VK_FILTER_LINEAR;
            sci.minFilter = VK_FILTER_LINEAR;
            sci.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            sci.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            sci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            vkCreateSampler(GN_Vk.device, &sci, 0, &GN_Vk.skyboxSampler);
        }

        // 生成程序化天空渐变 (每面 256×256)
        uint32_t faceSize = smSize * smSize * 4;
        uint32_t totalSize = faceSize * 6;
        uint8_t *pixels = new uint8_t[totalSize];
        // +X, -X, +Y, -Y, +Z, -Z (Vulkan cubemap 标准顺序)
        // 关键：颜色必须由「该 texel 对应的世界方向」决定，不能用面内局部 y。
        // 旧实现每面都用局部 y 做渐变，相邻面在边界处亮度/颜色不连续，
        // 地面 IBL 反射时会把面接缝显示成随视角滑动的暗带（被误认为影子）。
        for(int face = 0; face < 6; face++) {
            uint8_t *dst = pixels + face * faceSize;
            for(int y = 0; y < smSize; y++) {
                const float v = 2.0f * ((float)y + 0.5f) / (float)smSize - 1.0f;
                for(int x = 0; x < smSize; x++) {
                    const float u = 2.0f * ((float)x + 0.5f) / (float)smSize - 1.0f;
                    float dx = 0.0f, dy = 0.0f, dz = 0.0f;
                    switch(face) {
                    case 0: dx = 1.0f;  dy = -v; dz = -u; break; // +X
                    case 1: dx = -1.0f; dy = -v; dz = u;  break; // -X
                    case 2: dx = u; dy = 1.0f;  dz = v;   break; // +Y
                    case 3: dx = u; dy = -1.0f; dz = -v;  break; // -Y
                    case 4: dx = u; dy = -v; dz = 1.0f;   break; // +Z
                    default: dx = -u; dy = -v; dz = -1.0f; break; // -Z
                    }
                    const float invLen = 1.0f / sqrtf(dx * dx + dy * dy + dz * dz);
                    dx *= invLen;
                    dy *= invLen;
                    dz *= invLen;
                    // 同一套世界空间渐变：地平线亮 → 天顶深蓝；地平线以下也取地平线色，
                    // 避免环境图下半球发黑（地面反射会把它显示成暗斑）。
                    const float up = dy > 0.0f ? dy : 0.0f;
                    const float t = powf(up, 0.7f);
                    float r = 0.72f + (0.28f - 0.72f) * t;
                    float g = 0.80f + (0.42f - 0.80f) * t;
                    float b = 0.92f + (0.72f - 0.92f) * t;
                    const int idx = (y * smSize + x) * 4;
                    dst[idx + 0] = (uint8_t)(r * 255);
                    dst[idx + 1] = (uint8_t)(g * 255);
                    dst[idx + 2] = (uint8_t)(b * 255);
                    dst[idx + 3] = 255;
                }
            }
        }
        // 上传到 GPU
        {
            VkBuffer staging;
            VkDeviceMemory stagingMem;
            CreateBuf(totalSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                      staging, stagingMem);
            void *d;
            vkMapMemory(GN_Vk.device, stagingMem, 0, totalSize, 0, &d);
            memcpy(d, pixels, totalSize);
            vkUnmapMemory(GN_Vk.device, stagingMem);
            delete[] pixels;

            VkCommandBuffer cb;
            {
                VkCommandBufferAllocateInfo ai = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
                ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
                ai.commandPool = GN_Vk.commandPool;
                ai.commandBufferCount = 1;
                vkAllocateCommandBuffers(GN_Vk.device, &ai, &cb);
            }
            {
                VkCommandBufferBeginInfo bi = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
                bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
                vkBeginCommandBuffer(cb, &bi);
            }
            // 过渡 UNDEFINED → TRANSFER_DST
            {
                VkImageMemoryBarrier b = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
                b.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                b.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                b.image = GN_Vk.skyboxImage;
                b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                b.subresourceRange.levelCount = 1;
                b.subresourceRange.layerCount = 6;
                b.srcAccessMask = 0;
                b.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, 0, 0, 0, 1, &b);
            }
            // 拷贝 6 个面
            VkBufferImageCopy regions[6] = {};
            for(int f = 0; f < 6; f++) {
                regions[f].bufferOffset = f * faceSize;
                regions[f].imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                regions[f].imageSubresource.mipLevel = 0;
                regions[f].imageSubresource.baseArrayLayer = (uint32_t)f;
                regions[f].imageSubresource.layerCount = 1;
                regions[f].imageExtent = {(uint32_t)smSize, (uint32_t)smSize, 1};
            }
            vkCmdCopyBufferToImage(cb, staging, GN_Vk.skyboxImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 6, regions);
            // 过渡 TRANSFER_DST → SHADER_READ_ONLY
            {
                VkImageMemoryBarrier b = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
                b.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                b.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                b.image = GN_Vk.skyboxImage;
                b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                b.subresourceRange.levelCount = 1;
                b.subresourceRange.layerCount = 6;
                b.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, 0, 0, 0, 1, &b);
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
            vkDestroyBuffer(GN_Vk.device, staging, 0);
            vkFreeMemory(GN_Vk.device, stagingMem, 0);
        }
    }

    // ── PBR 描述符集 Binding 5 → 天空盒 Cubemap (IBL) ──
    if(GN_Vk.skyboxView && GN_Vk.skyboxSampler && GN_Vk.descriptorSetPBR) {
        VkDescriptorImageInfo sii = {GN_Vk.skyboxSampler, GN_Vk.skyboxView,
                                     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        VkWriteDescriptorSet ws = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        ws.dstSet = GN_Vk.descriptorSetPBR;
        ws.dstBinding = 5;
        ws.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        ws.descriptorCount = 1;
        ws.pImageInfo = &sii;
        vkUpdateDescriptorSets(GN_Vk.device, 1, &ws, 0, 0);
    }

    // ── 编译天空盒着色器 + 创建管线 ──
    {
        auto svSpv = LoadSPIRVFile("assets/shaders/gn3d_skybox.vert.spv");
        auto sfSpv = LoadSPIRVFile("assets/shaders/gn3d_skybox.frag.spv");
        if(!svSpv.empty() && !sfSpv.empty()) {
            // 描述符布局: Binding 0 = UBO (invProj+invView), Binding 1 = Cubemap
            VkDescriptorSetLayoutBinding dslb[2] = {};
            dslb[0].binding = 0;
            dslb[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            dslb[0].descriptorCount = 1;
            dslb[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
            dslb[1].binding = 1;
            dslb[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            dslb[1].descriptorCount = 1;
            dslb[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
            VkDescriptorSetLayoutCreateInfo dslci = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
            dslci.bindingCount = 2;
            dslci.pBindings = dslb;
            vkCreateDescriptorSetLayout(GN_Vk.device, &dslci, 0, &GN_Vk.skyboxDescriptorLayout);

            VkPipelineLayoutCreateInfo plci = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
            plci.setLayoutCount = 1;
            plci.pSetLayouts = &GN_Vk.skyboxDescriptorLayout;
            vkCreatePipelineLayout(GN_Vk.device, &plci, 0, &GN_Vk.skyboxPipelineLayout);

            // UBO (128 bytes: viewProj 64 + VkSkyParams 64)
            CreateBuf(128, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                      GN_Vk.skyboxUBOBuffer, GN_Vk.skyboxUBOMemory);

            // Descriptor set
            {
                VkDescriptorSetAllocateInfo dai = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
                dai.descriptorPool = GN_Vk.descriptorPool;
                dai.descriptorSetCount = 1;
                dai.pSetLayouts = &GN_Vk.skyboxDescriptorLayout;
                vkAllocateDescriptorSets(GN_Vk.device, &dai, &GN_Vk.skyboxDescriptorSet);
                VkDescriptorBufferInfo dbi = {GN_Vk.skyboxUBOBuffer, 0, 128};
                VkDescriptorImageInfo dii = {GN_Vk.skyboxSampler, GN_Vk.skyboxView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
                VkWriteDescriptorSet w[2] = {{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET}, {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET}};
                w[0].dstSet = GN_Vk.skyboxDescriptorSet;
                w[0].dstBinding = 0;
                w[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                w[0].descriptorCount = 1;
                w[0].pBufferInfo = &dbi;
                w[1].dstSet = GN_Vk.skyboxDescriptorSet;
                w[1].dstBinding = 1;
                w[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                w[1].descriptorCount = 1;
                w[1].pImageInfo = &dii;
                vkUpdateDescriptorSets(GN_Vk.device, 2, w, 0, 0);
            }

            create3DSkyboxPipeline();
        }
    }
}

// ── 延迟光照 resize 支持：framebuffer 引用 swapchainView，binding 1..4 引用 G-Buffer 视图 ──

// 写入 binding 1..4：NormalRough / AlbedoMetal / AO / Depth（附件索引 + 1，与
// gn3d_deferred_light.frag 的声明一致）。初始建集与 resize 换视图共用同一份绑定顺序。
static void writeDeferredLightGBufferBindings() {
    if(GN_Vk.deferredLightDescriptorSet == VK_NULL_HANDLE) return;
    VkDescriptorImageInfo infos[eGBUFFER_ATTACHMENT_COUNT];
    VkWriteDescriptorSet w[eGBUFFER_ATTACHMENT_COUNT] = {};
    for(uint32_t i = 0; i < eGBUFFER_ATTACHMENT_COUNT; i++) {
        eGBUFFER_ATTACHMENT a = (eGBUFFER_ATTACHMENT)i;
        infos[i].sampler = GN_Vk.gBufferSampler;
        infos[i].imageView = gbufferAttachmentView(a);
        infos[i].imageLayout = (a == eGBUFFER_DEPTH)
                                   ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
                                   : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        w[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w[i].dstSet = GN_Vk.deferredLightDescriptorSet;
        w[i].dstBinding = i + 1;
        w[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        w[i].descriptorCount = 1;
        w[i].pImageInfo = &infos[i];
    }
    vkUpdateDescriptorSets(GN_Vk.device, eGBUFFER_ATTACHMENT_COUNT, w, 0, 0);
}

// 延迟光照 framebuffer 以 swapchainView 为附件：resize 销毁旧 view 前先销毁、换新 view 后重建。
void destroy3DDeferredLightFramebuffers() {
    if(!GN_Vk.deferredLightFramebuffers) return;
    for(uint32_t i = 0; i < GN_Vk.swapchainImageCount; i++) {
        if(GN_Vk.deferredLightFramebuffers[i]) {
            vkDestroyFramebuffer(GN_Vk.device, GN_Vk.deferredLightFramebuffers[i], 0);
        }
    }
    delete[] GN_Vk.deferredLightFramebuffers;
    GN_Vk.deferredLightFramebuffers = nullptr;
}

static void createDeferredLightFramebuffers() {
    if(GN_Vk.deferredLightRenderPass == VK_NULL_HANDLE) return;
    GN_Vk.deferredLightFramebuffers = new VkFramebuffer[GN_Vk.swapchainImageCount];
    for(uint32_t i = 0; i < GN_Vk.swapchainImageCount; i++) {
        GN_Vk.deferredLightFramebuffers[i] = VK_NULL_HANDLE;
        VkFramebufferCreateInfo fci = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        fci.renderPass = GN_Vk.deferredLightRenderPass;
        fci.attachmentCount = 1;
        fci.pAttachments = &GN_Vk.swapchainViews[i];
        fci.width = GN_Vk.swapchainExtent.width;
        fci.height = GN_Vk.swapchainExtent.height;
        fci.layers = 1;
        vkCreateFramebuffer(GN_Vk.device, &fci, 0, &GN_Vk.deferredLightFramebuffers[i]);
    }
}

// 延迟光照管线：全屏三角形、固定视口/裁剪随窗口尺寸变化，resize 时按新尺寸重建。
static void createDeferredLightPipeline(const std::vector<uint32_t> &dlv,
                                        const std::vector<uint32_t> &dlf) {
    if(dlv.empty() || dlf.empty() || GN_Vk.deferredLightRenderPass == VK_NULL_HANDLE) return;
    uint32_t sw = GN_Vk.swapchainExtent.width;
    uint32_t sh = GN_Vk.swapchainExtent.height;
    GN3d::sPIPELINE_DESC descDL;
    descDL.aKind = GN3d::ePIPE_GRAPHICS;
    descDL.aVertSpv = dlv.data();
    descDL.aVertSize = dlv.size() * sizeof(uint32_t);
    descDL.aFragSpv = dlf.data();
    descDL.aFragSize = dlf.size() * sizeof(uint32_t);
    descDL.aRenderPass = GN_Vk.deferredLightRenderPass;
    descDL.aSubpass = 0;
    descDL.aLayout = GN_Vk.deferredLightPipelineLayout;
    descDL.aDepthTest = false;
    descDL.aDepthWrite = false;
    descDL.aDepthStencil = false;
    descDL.aCullBack = false;
    descDL.aFrontFace = VK_FRONT_FACE_CLOCKWISE; // 世界 CCW + Y 翻转（全屏 pass，剔除已关）
    descDL.aBlend = GN3d::eBLEND_NONE;
    descDL.aTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    descDL.aSamples = VK_SAMPLE_COUNT_1_BIT;
    descDL.aColorAttachmentCount = 1;
    descDL.aDynamicViewport = false;
    descDL.aDynamicScissor = false;
    descDL.aViewport = VkViewport{0.0f, 0.0f, (float)sw, (float)sh, 0.0f, 1.0f};
    descDL.aScissor = VkRect2D{{0, 0}, {sw, sh}};
    descDL.aCustomBlend = false;
    GN_Vk.deferredLightPipeline = GN_Vk.aPipelineCache.GetOrCreate(descDL);
    if(!GN_Vk.deferredLightPipeline) {
        fprintf(stderr, "[GN_Vk] 延迟光照管线创建失败: %s\n", GN_Vk.aPipelineCache.GetLastError());
    }
}

// resize 后调用（必须在 recreate3DGbufferTargets 之后）：重建 framebuffer、重写 binding 1..4、重建管线。
void create3DDeferredLightTargets() {
    createDeferredLightFramebuffers();
    writeDeferredLightGBufferBindings();
    auto dlv = LoadSPIRVFile("assets/shaders/gn3d_deferred_light.vert.spv");
    auto dlf = LoadSPIRVFile("assets/shaders/gn3d_deferred_light.frag.spv");
    createDeferredLightPipeline(dlv, dlf);
}

// 合成 framebuffer 以 swapchainView 为附件：resize 同样先销毁、换新 view 后重建。
void destroy3DCompositeFramebuffers() {
    if(!GN_Vk.compositeFramebuffers) return;
    for(uint32_t i = 0; i < GN_Vk.swapchainImageCount; i++) {
        if(GN_Vk.compositeFramebuffers[i]) {
            vkDestroyFramebuffer(GN_Vk.device, GN_Vk.compositeFramebuffers[i], 0);
        }
    }
    delete[] GN_Vk.compositeFramebuffers;
    GN_Vk.compositeFramebuffers = nullptr;
}

void create3DCompositeFramebuffers() {
    if(GN_Vk.compositeRenderPass == VK_NULL_HANDLE) return;
    GN_Vk.compositeFramebuffers = new VkFramebuffer[GN_Vk.swapchainImageCount];
    for(uint32_t i = 0; i < GN_Vk.swapchainImageCount; i++) {
        GN_Vk.compositeFramebuffers[i] = VK_NULL_HANDLE;
        VkFramebufferCreateInfo fci = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        fci.renderPass = GN_Vk.compositeRenderPass;
        fci.attachmentCount = 1;
        fci.pAttachments = &GN_Vk.swapchainViews[i];
        fci.width = GN_Vk.swapchainExtent.width;
        fci.height = GN_Vk.swapchainExtent.height;
        fci.layers = 1;
        vkCreateFramebuffer(GN_Vk.device, &fci, 0, &GN_Vk.compositeFramebuffers[i]);
    }
}

// 延迟光照：24 binding 描述符、交换链 render pass/framebuffer、全屏三角形管线。
void init3DDeferredLight() {
    auto dlv = LoadSPIRVFile("assets/shaders/gn3d_deferred_light.vert.spv");
    auto dlf = LoadSPIRVFile("assets/shaders/gn3d_deferred_light.frag.spv");
    if(dlv.empty() || dlf.empty()) return;
    // 描述符: 0=SceneUBO,1=gNormalRough,2=gAlbedoMetal,3=gAo,4=gDepth,5=shadow,6=skybox,7=LightUBO,
    //         8=雾 UBO（P11-S3）,9=GI UBO,10=预滤波环境 cubemap,11=BRDF LUT（G1b）,
    //         12=SSAO blurAO（G2b）,13=SSGI denoised（G4）,14=probe UBO,15..23=探针 3D 纹理（G5b-2）
    VkDescriptorSetLayoutBinding dlb[24] = {};
    dlb[0].binding = 0;
    dlb[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    dlb[0].descriptorCount = 1;
    dlb[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    for(int i = 1; i <= 4; i++) {
        dlb[i].binding = (uint32_t)i;
        dlb[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        dlb[i].descriptorCount = 1;
        dlb[i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    }
    dlb[5].binding = 5;
    dlb[5].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    dlb[5].descriptorCount = 1;
    dlb[5].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    dlb[6].binding = 6;
    dlb[6].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    dlb[6].descriptorCount = 1;
    dlb[6].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    dlb[7].binding = 7;
    dlb[7].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    dlb[7].descriptorCount = 1;
    dlb[7].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    dlb[8].binding = 8;
    dlb[8].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    dlb[8].descriptorCount = 1;
    dlb[8].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    dlb[9].binding = 9;
    dlb[9].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    dlb[9].descriptorCount = 1;
    dlb[9].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    dlb[10].binding = 10;
    dlb[10].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    dlb[10].descriptorCount = 1;
    dlb[10].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    dlb[11].binding = 11;
    dlb[11].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    dlb[11].descriptorCount = 1;
    dlb[11].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    dlb[12].binding = 12;
    dlb[12].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    dlb[12].descriptorCount = 1;
    dlb[12].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    dlb[13].binding = 13;
    dlb[13].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    dlb[13].descriptorCount = 1;
    dlb[13].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    // G5b-2：14=probe UBO，15..23=9 张 SH9 探针 3D 纹理。
    dlb[14].binding = 14;
    dlb[14].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    dlb[14].descriptorCount = 1;
    dlb[14].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    for(int b = 15; b <= 23; b++) {
        dlb[b].binding = (uint32_t)b;
        dlb[b].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        dlb[b].descriptorCount = 1;
        dlb[b].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    }
    VkDescriptorSetLayoutCreateInfo dci = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    dci.bindingCount = 24;
    dci.pBindings = dlb;
    vkCreateDescriptorSetLayout(GN_Vk.device, &dci, 0, &GN_Vk.deferredLightDescriptorLayout);
    {
        VkPipelineLayoutCreateInfo pl = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        pl.setLayoutCount = 1;
        pl.pSetLayouts = &GN_Vk.deferredLightDescriptorLayout;
        vkCreatePipelineLayout(GN_Vk.device, &pl, 0, &GN_Vk.deferredLightPipelineLayout);
    }
    // Descriptor set：binding 1..4（G-Buffer）由 writeDeferredLightGBufferBindings 统一写入。
    {
        VkDescriptorSetAllocateInfo dai = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        dai.descriptorPool = GN_Vk.descriptorPool;
        dai.descriptorSetCount = 1;
        dai.pSetLayouts = &GN_Vk.deferredLightDescriptorLayout;
        vkAllocateDescriptorSets(GN_Vk.device, &dai, &GN_Vk.deferredLightDescriptorSet);
        VkDescriptorBufferInfo dbi0 = {GN_Vk.uniformBuffer3D, 0, sizeof(VkSceneUBO)};
        VkDescriptorBufferInfo dbi7 = {GN_Vk.lightBuffer3D, 0, 1024};
        VkDescriptorImageInfo sii = {GN_Vk.shadowSampler, GN_Vk.shadowView, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL};
        VkDescriptorImageInfo cii = {GN_Vk.skyboxSampler, GN_Vk.skyboxView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        VkDescriptorBufferInfo dbi8 = {atmosphereFogBuffer(), 0, atmosphereFogRange()};
        VkWriteDescriptorSet w[5] = {};
        for(int i = 0; i < 5; i++) w[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w[0].dstSet = GN_Vk.deferredLightDescriptorSet;
        w[0].dstBinding = 0;
        w[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        w[0].descriptorCount = 1;
        w[0].pBufferInfo = &dbi0;
        w[1].dstSet = GN_Vk.deferredLightDescriptorSet;
        w[1].dstBinding = 5;
        w[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        w[1].descriptorCount = 1;
        w[1].pImageInfo = &sii;
        w[2].dstSet = GN_Vk.deferredLightDescriptorSet;
        w[2].dstBinding = 6;
        w[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        w[2].descriptorCount = 1;
        w[2].pImageInfo = &cii;
        w[3].dstSet = GN_Vk.deferredLightDescriptorSet;
        w[3].dstBinding = 7;
        w[3].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        w[3].descriptorCount = 1;
        w[3].pBufferInfo = &dbi7;
        w[4].dstSet = GN_Vk.deferredLightDescriptorSet;
        w[4].dstBinding = 8;
        w[4].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        w[4].descriptorCount = 1;
        w[4].pBufferInfo = &dbi8;
        vkUpdateDescriptorSets(GN_Vk.device, 5, w, 0, 0);
        writeDeferredLightGBufferBindings();
    }
    // 延迟光照 RenderPass → swapchain (复用 composite render pass 格式)
    // 只与 swapchainFormat 有关、与尺寸无关，resize 不重建。
    {
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
        vkCreateRenderPass(GN_Vk.device, &rpi, 0, &GN_Vk.deferredLightRenderPass);
    }
    createDeferredLightFramebuffers();
    createDeferredLightPipeline(dlv, dlf);
}

} // namespace GNg
