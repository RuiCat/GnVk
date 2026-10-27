// 3D 初始化（场景相关）：HDR/合成、天空盒、G-Buffer、延迟光照。
#include <GN.h>
#include "GN_Vk.h"
#include "vkInternal.h"

namespace GNg {

void init3DHdr() {
    uint32_t sw = GN_Vk.swapchainExtent.width;
    uint32_t sh = GN_Vk.swapchainExtent.height;

    // ═══════════════════════════════════════════════════════
    //  HDR 离屏目标 (R16G16B16A16_SFLOAT — 支持 HDR > 1.0)
    // ═══════════════════════════════════════════════════════
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
        vkCreateImage(GN_Vk.device, &ii, 0, &GN_Vk.hdrImage);
        VkMemoryRequirements hmr;
        vkGetImageMemoryRequirements(GN_Vk.device, GN_Vk.hdrImage, &hmr);
        VkMemoryAllocateInfo hmai = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        hmai.allocationSize = hmr.size;
        hmai.memoryTypeIndex = FindMemType(hmr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        vkAllocateMemory(GN_Vk.device, &hmai, 0, &GN_Vk.hdrMemory);
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
    // HDR Framebuffers (per swapchain image: MSAA Color + MSAA Depth + HDR resolve)
    GN_Vk.hdrFramebuffers = new VkFramebuffer[GN_Vk.swapchainImageCount];
    for(uint32_t i = 0; i < GN_Vk.swapchainImageCount; i++) {
        VkImageView hatts[3] = {GN_Vk.msaaColorView, GN_Vk.depthView, GN_Vk.hdrView};
        VkFramebufferCreateInfo fci = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        fci.renderPass = GN_Vk.hdrRenderPass;
        fci.attachmentCount = 3;
        fci.pAttachments = hatts;
        fci.width = sw;
        fci.height = sh;
        fci.layers = 1;
        vkCreateFramebuffer(GN_Vk.device, &fci, 0, &GN_Vk.hdrFramebuffers[i]);
    }

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
        GN_Vk.compositeFramebuffers = new VkFramebuffer[GN_Vk.swapchainImageCount];
        for(uint32_t i = 0; i < GN_Vk.swapchainImageCount; i++) {
            VkFramebufferCreateInfo fci = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
            fci.renderPass = GN_Vk.compositeRenderPass;
            fci.attachmentCount = 1;
            fci.pAttachments = &GN_Vk.swapchainViews[i];
            fci.width = sw;
            fci.height = sh;
            fci.layers = 1;
            vkCreateFramebuffer(GN_Vk.device, &fci, 0, &GN_Vk.compositeFramebuffers[i]);
        }

        // 合成管线（P1b）：无顶点输入、无深度附件、不剔除、单采样、无混合、固定视口
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
        descComp.aFrontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE; // 旧 pci 未设置 frontFace（默认 CCW）
        descComp.aBlend = GN3d::eBLEND_NONE;
        descComp.aTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        descComp.aSamples = VK_SAMPLE_COUNT_1_BIT;
        descComp.aColorAttachmentCount = 1;
        descComp.aDynamicViewport = false;
        descComp.aDynamicScissor = false;
        descComp.aViewport = VkViewport{0.0f, 0.0f, (float)sw, (float)sh, 0.0f, 1.0f};
        descComp.aScissor = VkRect2D{{0, 0}, {sw, sh}};
        descComp.aCustomBlend = false;
        GN_Vk.compositePipeline = GN_Vk.aPipelineCache.GetOrCreate(descComp);
        if(!GN_Vk.compositePipeline) {
            fprintf(stderr, "[GN_Vk] 合成管线创建失败: %s\n", GN_Vk.aPipelineCache.GetLastError());
        }

        // 分配描述符集并绑定 HDR 纹理
        {
            VkDescriptorSetAllocateInfo dai = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
            dai.descriptorPool = GN_Vk.descriptorPool;
            dai.descriptorSetCount = 1;
            dai.pSetLayouts = &GN_Vk.compositeDescriptorLayout;
            vkAllocateDescriptorSets(GN_Vk.device, &dai, &GN_Vk.compositeDescriptorSet);
            VkDescriptorImageInfo hii = {GN_Vk.hdrSampler, GN_Vk.hdrView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
            VkWriteDescriptorSet wc = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            wc.dstSet = GN_Vk.compositeDescriptorSet;
            wc.dstBinding = 1;
            wc.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            wc.descriptorCount = 1;
            wc.pImageInfo = &hii;
            vkUpdateDescriptorSets(GN_Vk.device, 1, &wc, 0, 0);
        }
    }
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
        for(int face = 0; face < 6; face++) {
            uint8_t *dst = pixels + face * faceSize;
            float faceH; // 每个面的"上方向"在天空渐变中的位置
            for(int y = 0; y < smSize; y++) {
                float fy = (float)y / (float)(smSize - 1); // 0=底部, 1=顶部
                for(int x = 0; x < smSize; x++) {
                    int idx = (y * smSize + x) * 4;
                    // 天空渐变: 顶部深蓝 → 底部浅蓝白
                    float t = fy;
                    float r = 0.35f + 0.25f * t + 0.35f * (1.0f - t);
                    float g = 0.45f + 0.25f * t + 0.50f * (1.0f - t);
                    float b = 0.60f + 0.30f * t + 0.60f * (1.0f - t);
                    // 近地平线加亮
                    if(fy < 0.3f) {
                        float f = 1.0f - fy / 0.3f;
                        r += 0.25f * f;
                        g += 0.20f * f;
                        b += 0.15f * f;
                    }
                    // 顶部加深
                    if(fy > 0.85f) {
                        float f = (fy - 0.85f) / 0.15f;
                        r -= 0.05f * f;
                        g -= 0.05f * f;
                        b += 0.10f * f;
                    }
                    if(r > 1.0f) r = 1.0f;
                    if(g > 1.0f) g = 1.0f;
                    if(b > 1.0f) b = 1.0f;
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
            dslb[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
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

            // UBO (64 bytes: viewProj)
            CreateBuf(64, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                      GN_Vk.skyboxUBOBuffer, GN_Vk.skyboxUBOMemory);

            // Descriptor set
            {
                VkDescriptorSetAllocateInfo dai = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
                dai.descriptorPool = GN_Vk.descriptorPool;
                dai.descriptorSetCount = 1;
                dai.pSetLayouts = &GN_Vk.skyboxDescriptorLayout;
                vkAllocateDescriptorSets(GN_Vk.device, &dai, &GN_Vk.skyboxDescriptorSet);
                VkDescriptorBufferInfo dbi = {GN_Vk.skyboxUBOBuffer, 0, 64};
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

            // 天空盒管线（P1b）：无顶点输入、不剔除、正面 CLOCKWISE、MSAA、深度 LEQUAL 不写、固定视口
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
            descSky.aViewport = VkViewport{0.0f, 0.0f, (float)sw, (float)sh, 0.0f, 1.0f};
            descSky.aScissor = VkRect2D{{0, 0}, {sw, sh}};
            descSky.aCustomBlend = false;
            GN_Vk.skyboxPipeline = GN_Vk.aPipelineCache.GetOrCreate(descSky);
            if(!GN_Vk.skyboxPipeline) {
                fprintf(stderr, "[GN_Vk] 天空盒管线创建失败: %s\n", GN_Vk.aPipelineCache.GetLastError());
            }
        }
    }
}

// 延迟渲染 G-Buffer：三张颜色附件 + 深度、render pass/framebuffer、G-Buffer 管线。
void init3DGbuffer() {
    uint32_t sw = GN_Vk.swapchainExtent.width;
    uint32_t sh = GN_Vk.swapchainExtent.height;

    // G-Buffer 纹理: albedo (R8G8B8A8), normal+rough (R16G16B16A16), PBR (R8G8B8A8)
    VkFormat gFormats[3] = {VK_FORMAT_R16G16B16A16_SFLOAT, VK_FORMAT_R16G16B16A16_SFLOAT, VK_FORMAT_R16G16B16A16_SFLOAT};
    VkImage *gImages[3] = {&GN_Vk.gAlbedoImage, &GN_Vk.gNormalImage, &GN_Vk.gPBRImage};
    VkDeviceMemory *gMems[3] = {&GN_Vk.gAlbedoMemory, &GN_Vk.gNormalMemory, &GN_Vk.gPBRMemory};
    VkImageView *gViews[3] = {&GN_Vk.gAlbedoView, &GN_Vk.gNormalView, &GN_Vk.gPBRView};
    for(int i = 0; i < 3; i++) {
        VkImageCreateInfo ii = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        ii.imageType = VK_IMAGE_TYPE_2D;
        ii.extent = {sw, sh, 1};
        ii.mipLevels = 1;
        ii.arrayLayers = 1;
        ii.format = gFormats[i];
        ii.tiling = VK_IMAGE_TILING_OPTIMAL;
        ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        ii.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        ii.samples = VK_SAMPLE_COUNT_1_BIT;
        ii.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        vkCreateImage(GN_Vk.device, &ii, 0, gImages[i]);
        VkMemoryRequirements mr;
        vkGetImageMemoryRequirements(GN_Vk.device, *gImages[i], &mr);
        VkMemoryAllocateInfo mai = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        mai.allocationSize = mr.size;
        mai.memoryTypeIndex = FindMemType(mr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        vkAllocateMemory(GN_Vk.device, &mai, 0, gMems[i]);
        vkBindImageMemory(GN_Vk.device, *gImages[i], *gMems[i], 0);
        VkImageViewCreateInfo vi = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        vi.image = *gImages[i];
        vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
        vi.format = gFormats[i];
        vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        vi.subresourceRange.levelCount = 1;
        vi.subresourceRange.layerCount = 1;
        vkCreateImageView(GN_Vk.device, &vi, 0, gViews[i]);
    }
    // G-Buffer 深度 (1采样)
    {
        VkImageCreateInfo ii = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        ii.imageType = VK_IMAGE_TYPE_2D;
        ii.extent = {sw, sh, 1};
        ii.mipLevels = 1;
        ii.arrayLayers = 1;
        ii.format = GN_Vk.depthFormat;
        ii.tiling = VK_IMAGE_TILING_OPTIMAL;
        ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        ii.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        ii.samples = VK_SAMPLE_COUNT_1_BIT;
        ii.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        vkCreateImage(GN_Vk.device, &ii, 0, &GN_Vk.gDepthImage);
        VkMemoryRequirements mr;
        vkGetImageMemoryRequirements(GN_Vk.device, GN_Vk.gDepthImage, &mr);
        VkMemoryAllocateInfo mai = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        mai.allocationSize = mr.size;
        mai.memoryTypeIndex = FindMemType(mr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        vkAllocateMemory(GN_Vk.device, &mai, 0, &GN_Vk.gDepthMemory);
        vkBindImageMemory(GN_Vk.device, GN_Vk.gDepthImage, GN_Vk.gDepthMemory, 0);
        VkImageViewCreateInfo vi = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        vi.image = GN_Vk.gDepthImage;
        vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
        vi.format = GN_Vk.depthFormat;
        vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        vi.subresourceRange.levelCount = 1;
        vi.subresourceRange.layerCount = 1;
        vkCreateImageView(GN_Vk.device, &vi, 0, &GN_Vk.gDepthView);
    }
    // G-Buffer 采样器
    {
        VkSamplerCreateInfo sci = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        sci.magFilter = VK_FILTER_NEAREST;
        sci.minFilter = VK_FILTER_NEAREST;
        sci.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sci.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        vkCreateSampler(GN_Vk.device, &sci, 0, &GN_Vk.gBufferSampler);
    }

    // G-Buffer RenderPass (4 附件, 非MSAA)
    {
        VkAttachmentDescription ads[4] = {};
        ads[0].format = gFormats[0];
        ads[0].samples = VK_SAMPLE_COUNT_1_BIT;
        ads[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        ads[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        ads[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        ads[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        ads[1].format = gFormats[1];
        ads[1].samples = VK_SAMPLE_COUNT_1_BIT;
        ads[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        ads[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        ads[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        ads[1].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        ads[2].format = gFormats[2];
        ads[2].samples = VK_SAMPLE_COUNT_1_BIT;
        ads[2].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        ads[2].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        ads[2].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        ads[2].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        ads[3].format = GN_Vk.depthFormat;
        ads[3].samples = VK_SAMPLE_COUNT_1_BIT;
        ads[3].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        ads[3].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        ads[3].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        ads[3].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        VkAttachmentReference crs[3] = {{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL}, {1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL}, {2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL}};
        VkAttachmentReference dr = {3, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
        VkSubpassDescription sp = {};
        sp.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        sp.colorAttachmentCount = 3;
        sp.pColorAttachments = crs;
        sp.pDepthStencilAttachment = &dr;
        VkRenderPassCreateInfo rpi = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
        rpi.attachmentCount = 4;
        rpi.pAttachments = ads;
        rpi.subpassCount = 1;
        rpi.pSubpasses = &sp;
        vkCreateRenderPass(GN_Vk.device, &rpi, 0, &GN_Vk.gBufferRenderPass);
    }
    GN_Vk.gBufferFramebuffers = new VkFramebuffer[1];
    {
        VkImageView atts[4] = {GN_Vk.gAlbedoView, GN_Vk.gNormalView, GN_Vk.gPBRView, GN_Vk.gDepthView};
        VkFramebufferCreateInfo fci = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        fci.renderPass = GN_Vk.gBufferRenderPass;
        fci.attachmentCount = 4;
        fci.pAttachments = atts;
        fci.width = sw;
        fci.height = sh;
        fci.layers = 1;
        VkResult fr = vkCreateFramebuffer(GN_Vk.device, &fci, 0, &GN_Vk.gBufferFramebuffers[0]);
        VkCheck(fr, "gBufferFramebuffer");
    }

    // ── G-Buffer 着色器 + 管线 ──
    {
        auto gvSpv = LoadSPIRVFile("assets/shaders/gn3d_gbuffer.vert.spv");
        auto gfSpv = LoadSPIRVFile("assets/shaders/gn3d_gbuffer.frag.spv");
        if(!gvSpv.empty() && !gfSpv.empty()) {
            // 描述符布局: Binding 0=SceneUBO, 1=texSampler, 3=normalSampler (复用PBR部分)
            VkDescriptorSetLayoutBinding dslb[3] = {};
            dslb[0].binding = 0;
            dslb[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            dslb[0].descriptorCount = 1;
            dslb[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
            dslb[1].binding = 1;
            dslb[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            dslb[1].descriptorCount = 1;
            dslb[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
            dslb[2].binding = 3;
            dslb[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            dslb[2].descriptorCount = 1;
            dslb[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
            VkDescriptorSetLayoutCreateInfo dci = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
            dci.bindingCount = 3;
            dci.pBindings = dslb;
            vkCreateDescriptorSetLayout(GN_Vk.device, &dci, 0, &GN_Vk.gBufferDescriptorLayout);
            {
                VkPushConstantRange pcr = {};
                pcr.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
                pcr.offset = 0;
                pcr.size = sizeof(VkModelPushConstPBR);
                VkPipelineLayoutCreateInfo pl = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
                pl.setLayoutCount = 1;
                pl.pSetLayouts = &GN_Vk.gBufferDescriptorLayout;
                pl.pushConstantRangeCount = 1;
                pl.pPushConstantRanges = &pcr;
                vkCreatePipelineLayout(GN_Vk.device, &pl, 0, &GN_Vk.gBufferPipelineLayout);
            }
            // Descriptor set
            {
                VkDescriptorSetAllocateInfo dai = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
                dai.descriptorPool = GN_Vk.descriptorPool;
                dai.descriptorSetCount = 1;
                dai.pSetLayouts = &GN_Vk.gBufferDescriptorLayout;
                vkAllocateDescriptorSets(GN_Vk.device, &dai, &GN_Vk.gBufferDescriptorSet);
                VkDescriptorBufferInfo dbi = {GN_Vk.uniformBuffer3D, 0, sizeof(VkSceneUBO)};
                VkDescriptorImageInfo dii = {GN_Vk.defaultSampler, GN_Vk.defaultImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
                VkWriteDescriptorSet w[3] = {{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET}, {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET}, {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET}};
                w[0].dstSet = GN_Vk.gBufferDescriptorSet;
                w[0].dstBinding = 0;
                w[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                w[0].descriptorCount = 1;
                w[0].pBufferInfo = &dbi;
                w[1].dstSet = GN_Vk.gBufferDescriptorSet;
                w[1].dstBinding = 1;
                w[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                w[1].descriptorCount = 1;
                w[1].pImageInfo = &dii;
                w[2].dstSet = GN_Vk.gBufferDescriptorSet;
                w[2].dstBinding = 3;
                w[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                w[2].descriptorCount = 1;
                w[2].pImageInfo = &dii;
                vkUpdateDescriptorSets(GN_Vk.device, 3, w, 0, 0);
            }
            // G-Buffer 管线（P1b）：4 顶点属性、3 颜色附件、背面剔除 CLOCKWISE、MSAA1、深度 LESS
            GN3d::sVERTEX_LAYOUT layoutGB;
            layoutGB.Add(0, (uint32_t)offsetof(VkVertex3D, px), VK_FORMAT_R32G32B32_SFLOAT);
            layoutGB.Add(1, (uint32_t)offsetof(VkVertex3D, nx), VK_FORMAT_R32G32B32_SFLOAT);
            layoutGB.Add(2, (uint32_t)offsetof(VkVertex3D, tx), VK_FORMAT_R32G32B32_SFLOAT);
            layoutGB.Add(3, (uint32_t)offsetof(VkVertex3D, u), VK_FORMAT_R32G32_SFLOAT);
            layoutGB.aStride = (uint32_t)sizeof(VkVertex3D);

            GN3d::sPIPELINE_DESC descGB;
            descGB.aKind = GN3d::ePIPE_GRAPHICS;
            descGB.aVertSpv = gvSpv.data();
            descGB.aVertSize = gvSpv.size() * sizeof(uint32_t);
            descGB.aFragSpv = gfSpv.data();
            descGB.aFragSize = gfSpv.size() * sizeof(uint32_t);
            descGB.aVertexLayout = layoutGB;
            descGB.aRenderPass = GN_Vk.gBufferRenderPass;
            descGB.aSubpass = 0;
            descGB.aLayout = GN_Vk.gBufferPipelineLayout;
            descGB.aDepthTest = true;
            descGB.aDepthWrite = true;
            descGB.aDepthCompare = VK_COMPARE_OP_LESS;
            descGB.aDepthStencil = true;
            descGB.aCullBack = true;
            descGB.aFrontFace = VK_FRONT_FACE_CLOCKWISE;
            descGB.aBlend = GN3d::eBLEND_NONE;
            descGB.aTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            descGB.aSamples = VK_SAMPLE_COUNT_1_BIT;
            descGB.aColorAttachmentCount = 3;
            descGB.aDynamicViewport = false;
            descGB.aDynamicScissor = false;
            descGB.aViewport = VkViewport{0.0f, 0.0f, (float)sw, (float)sh, 0.0f, 1.0f};
            descGB.aScissor = VkRect2D{{0, 0}, {sw, sh}};
            descGB.aCustomBlend = false;
            GN_Vk.gBufferPipeline = GN_Vk.aPipelineCache.GetOrCreate(descGB);
            if(!GN_Vk.gBufferPipeline) {
                fprintf(stderr, "[GN_Vk] G-Buffer 管线创建失败: %s\n", GN_Vk.aPipelineCache.GetLastError());
            }
        }
    }
}

// 延迟光照：8 binding 描述符、交换链 render pass/framebuffer、全屏三角形管线。
void init3DDeferredLight() {
    uint32_t sw = GN_Vk.swapchainExtent.width;
    uint32_t sh = GN_Vk.swapchainExtent.height;

    // ── 延迟光照着色器 + 管线 ──
    {
        auto dlv = LoadSPIRVFile("assets/shaders/gn3d_deferred_light.vert.spv");
        auto dlf = LoadSPIRVFile("assets/shaders/gn3d_deferred_light.frag.spv");
        if(!dlv.empty() && !dlf.empty()) {
            // 描述符: 0=SceneUBO,1=gAlbedo,2=gNormal,3=gPBR,4=gDepth,5=shadow,6=skybox,7=LightUBO
            VkDescriptorSetLayoutBinding dlb[8] = {};
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
            VkDescriptorSetLayoutCreateInfo dci = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
            dci.bindingCount = 8;
            dci.pBindings = dlb;
            vkCreateDescriptorSetLayout(GN_Vk.device, &dci, 0, &GN_Vk.deferredLightDescriptorLayout);
            {
                VkPipelineLayoutCreateInfo pl = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
                pl.setLayoutCount = 1;
                pl.pSetLayouts = &GN_Vk.deferredLightDescriptorLayout;
                vkCreatePipelineLayout(GN_Vk.device, &pl, 0, &GN_Vk.deferredLightPipelineLayout);
            }
            // Descriptor set
            {
                VkDescriptorSetAllocateInfo dai = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
                dai.descriptorPool = GN_Vk.descriptorPool;
                dai.descriptorSetCount = 1;
                dai.pSetLayouts = &GN_Vk.deferredLightDescriptorLayout;
                vkAllocateDescriptorSets(GN_Vk.device, &dai, &GN_Vk.deferredLightDescriptorSet);
                VkDescriptorBufferInfo dbi0 = {GN_Vk.uniformBuffer3D, 0, sizeof(VkSceneUBO)};
                VkDescriptorBufferInfo dbi7 = {GN_Vk.lightBuffer3D, 0, 1024};
                VkDescriptorImageInfo gii = {GN_Vk.gBufferSampler, GN_Vk.gAlbedoView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
                VkDescriptorImageInfo nii = {GN_Vk.gBufferSampler, GN_Vk.gNormalView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
                VkDescriptorImageInfo pii = {GN_Vk.gBufferSampler, GN_Vk.gPBRView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
                VkDescriptorImageInfo dii = {GN_Vk.gBufferSampler, GN_Vk.gDepthView, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL};
                VkDescriptorImageInfo sii = {GN_Vk.shadowSampler, GN_Vk.shadowView, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL};
                VkDescriptorImageInfo cii = {GN_Vk.skyboxSampler, GN_Vk.skyboxView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
                VkWriteDescriptorSet w[8] = {};
                for(int i = 0; i < 8; i++) w[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                w[0].dstSet = GN_Vk.deferredLightDescriptorSet;
                w[0].dstBinding = 0;
                w[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                w[0].descriptorCount = 1;
                w[0].pBufferInfo = &dbi0;
                w[1].dstSet = GN_Vk.deferredLightDescriptorSet;
                w[1].dstBinding = 1;
                w[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                w[1].descriptorCount = 1;
                w[1].pImageInfo = &gii;
                w[2].dstSet = GN_Vk.deferredLightDescriptorSet;
                w[2].dstBinding = 2;
                w[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                w[2].descriptorCount = 1;
                w[2].pImageInfo = &nii;
                w[3].dstSet = GN_Vk.deferredLightDescriptorSet;
                w[3].dstBinding = 3;
                w[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                w[3].descriptorCount = 1;
                w[3].pImageInfo = &pii;
                w[4].dstSet = GN_Vk.deferredLightDescriptorSet;
                w[4].dstBinding = 4;
                w[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                w[4].descriptorCount = 1;
                w[4].pImageInfo = &dii;
                w[5].dstSet = GN_Vk.deferredLightDescriptorSet;
                w[5].dstBinding = 5;
                w[5].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                w[5].descriptorCount = 1;
                w[5].pImageInfo = &sii;
                w[6].dstSet = GN_Vk.deferredLightDescriptorSet;
                w[6].dstBinding = 6;
                w[6].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                w[6].descriptorCount = 1;
                w[6].pImageInfo = &cii;
                w[7].dstSet = GN_Vk.deferredLightDescriptorSet;
                w[7].dstBinding = 7;
                w[7].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                w[7].descriptorCount = 1;
                w[7].pBufferInfo = &dbi7;
                vkUpdateDescriptorSets(GN_Vk.device, 8, w, 0, 0);
            }
            // 延迟光照 RenderPass → swapchain (复用 composite render pass 格式)
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
            GN_Vk.deferredLightFramebuffers = new VkFramebuffer[GN_Vk.swapchainImageCount];
            for(uint32_t i = 0; i < GN_Vk.swapchainImageCount; i++) {
                VkFramebufferCreateInfo fci = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
                fci.renderPass = GN_Vk.deferredLightRenderPass;
                fci.attachmentCount = 1;
                fci.pAttachments = &GN_Vk.swapchainViews[i];
                fci.width = sw;
                fci.height = sh;
                fci.layers = 1;
                vkCreateFramebuffer(GN_Vk.device, &fci, 0, &GN_Vk.deferredLightFramebuffers[i]);
            }
            // 延迟光照管线（P1b）：全屏三角形、无顶点输入、不剔除、无深度状态、单采样、1 颜色附件
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
            descDL.aFrontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE; // 旧 pci 未设置 frontFace（默认 CCW）
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
    }
}

} // namespace GNg
