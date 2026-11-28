// sIMAGE 数据上传/填充：InitToData / CreateEmptyToData / DataFill（PNG IO 见 vkImageIo.cpp）。
#include <GN.h>
#include "GN_Vk.h"
#include "vkInternal.h"

#include <cstdint>
#include <cstring>

namespace GNg {

namespace {
// sIMAGE 的采样寻址只接受 glTF 用到的三种；其余（含未初始化值）回退 CLAMP_TO_EDGE，
// 与旧实现固定 CLAMP 的 2D/UI 行为一致。
VkSamplerAddressMode imageWrapMode(VkSamplerAddressMode mode) {
    switch(mode) {
    case VK_SAMPLER_ADDRESS_MODE_REPEAT:
    case VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT:
    case VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE:
        return mode;
    default:
        return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    }
}
} // namespace

void sIMAGE::InitToData(char *filename, unsigned short imgnum, int fmt, bool mip, unsigned char lv) {
    Release();
    ImageLoad(filename, imgnum, this, fmt);
    if(filename) {
        this->fileName = filename;
        this->add_0000 = true;
    }
}

P<sIMAGE> ImageLoad(const char *fn, unsigned short num, int fmt) {
    P<sIMAGE> p(1);
    ImageLoad(fn, num, &(*p), fmt);
    return p;
}

// ── 更多缺失 API ──────────────────────────────────────────

void sIMAGE::CreateEmptyToData(int w, int h, unsigned short imgnum, int fmt,
                               unsigned char lv, bool mip, bool immed) {
    // Create actual Vulkan texture resources (unlike the old stub)
    if(!GN_Vk.initialized || w <= 0 || h <= 0) return;
    Release();

    VkFormat vkFmt = vkSrgb ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;
    VkDeviceSize sz = (VkDeviceSize)w * (VkDeviceSize)h * 4;

    // Create image
    {
        VkImageCreateInfo ici{}; ici.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        ici.imageType = VK_IMAGE_TYPE_2D;
        ici.extent = {(uint32_t)w, (uint32_t)h, 1};
        ici.mipLevels = 1;
        ici.arrayLayers = 1;
        ici.format = vkFmt;
        ici.tiling = VK_IMAGE_TILING_OPTIMAL;
        ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        ici.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        ici.samples = VK_SAMPLE_COUNT_1_BIT;
        ici.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        {
            VkResult res = vkCreateImage(GN_Vk.device, &ici, 0, &vkImage);
            VkCheck(res, "vkCreateImage in CreateEmptyToData");
            if(res != VK_SUCCESS) {
                vkImage = VK_NULL_HANDLE;
                return;
            }
        }
        VkMemoryRequirements mr;
        vkGetImageMemoryRequirements(GN_Vk.device, vkImage, &mr);
        VkMemoryAllocateInfo mai{}; mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        mai.allocationSize = mr.size;
        mai.memoryTypeIndex = FindMemType(mr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        {
            VkResult res = vkAllocateMemory(GN_Vk.device, &mai, 0, &vkImageMemory);
            if(res != VK_SUCCESS) {
                VkCheck(res, "vkAllocateMemory in CreateEmptyToData");
                vkDestroyImage(GN_Vk.device, vkImage, 0);
                vkImage = VK_NULL_HANDLE;
                vkImageMemory = VK_NULL_HANDLE;
                return;
            }
        }
        {
            VkResult res = vkBindImageMemory(GN_Vk.device, vkImage, vkImageMemory, 0);
            VkCheck(res, "vkBindImageMemory in CreateEmptyToData");
        }
    }

    // Create image view
    vkImageView = MakeImageView(vkImage, vkFmt);

    // Create sampler
    {
        VkSamplerCreateInfo sci{}; sci.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        sci.magFilter = VK_FILTER_LINEAR;
        sci.minFilter = VK_FILTER_LINEAR;
        sci.addressModeU = imageWrapMode(vkWrapU);
        sci.addressModeV = imageWrapMode(vkWrapV);
        VkResult res = vkCreateSampler(GN_Vk.device, &sci, 0, &vkSampler);
        VkCheck(res, "vkCreateSampler in CreateEmptyToData");
    }

    // Allocate descriptor set
    {
        VkDescriptorSetAllocateInfo dsai{}; dsai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        dsai.descriptorPool = GN_Vk.descriptorPool;
        dsai.descriptorSetCount = 1;
        dsai.pSetLayouts = &GN_Vk.descriptorLayout;
        VkResult res = vkAllocateDescriptorSets(GN_Vk.device, &dsai, &vkDescriptorSet);
        VkCheck(res, "vkAllocateDescriptorSets in CreateEmptyToData");
        VkDescriptorBufferInfo dbi = {GN_Vk.uniformBuffer, 0, sizeof(VkUBO)};
        VkDescriptorImageInfo dii = {vkSampler, vkImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        VkWriteDescriptorSet wds[2]{}; wds[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET; wds[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        wds[0].dstSet = vkDescriptorSet;
        wds[0].dstBinding = 0;
        wds[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        wds[0].descriptorCount = 1;
        wds[0].pBufferInfo = &dbi;
        wds[1].dstSet = vkDescriptorSet;
        wds[1].dstBinding = 1;
        wds[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        wds[1].descriptorCount = 1;
        wds[1].pImageInfo = &dii;
        vkUpdateDescriptorSets(GN_Vk.device, 2, wds, 0, 0);
    }

    // Set metadata
    width = w;
    height = h;
    uRange = 1.0f;
    vRange = 1.0f;
    texnum = 1;
    (void)imgnum;
    (void)lv;
    (void)mip;
    (void)immed;
    (void)fmt;
    (void)sz;

    // Transition image from UNDEFINED to SHADER_READ_ONLY (required before DataFill/rendering)
    {
        VkCommandBuffer cb;
        {
            VkCommandBufferAllocateInfo ai{}; ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            ai.commandPool = GN_Vk.commandPool;
            ai.commandBufferCount = 1;
            VkResult res = vkAllocateCommandBuffers(GN_Vk.device, &ai, &cb);
            VkCheck(res, "vkAllocateCommandBuffers in CreateEmptyToData");
        }
        {
            VkCommandBufferBeginInfo bi{}; bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            vkBeginCommandBuffer(cb, &bi);
        }
        {
            VkImageMemoryBarrier b{}; b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            b.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            b.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            b.image = vkImage;
            b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            b.subresourceRange.levelCount = 1;
            b.subresourceRange.layerCount = 1;
            b.srcAccessMask = 0;
            b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                 VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, 0, 0, 0, 1, &b);
        }
        vkEndCommandBuffer(cb);
        {
            VkSubmitInfo si{}; si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            si.commandBufferCount = 1;
            si.pCommandBuffers = &cb;
            vkQueueSubmit(GN_Vk.graphicsQueue, 1, &si, VK_NULL_HANDLE);
            vkQueueWaitIdle(GN_Vk.graphicsQueue);
        }
        vkFreeCommandBuffers(GN_Vk.device, GN_Vk.commandPool, 1, &cb);
    }
}

// Vulkan-backed DataFill: uploads in-memory pixel data to GPU texture.
// If VkImage already exists (from CreateEmptyToData), does sub-region update
// using sw/sh as offset_x/offset_y. Otherwise creates a new texture.
void sIMAGE::DataFill(uint8 *data, int fmt, int w, int h, int sw, int sh, unsigned short *texid, bool mip) {
    if(!GN_Vk.initialized || !data) return;
    if(w <= 0) w = width;
    if(h <= 0) h = height;
    if(w <= 0 || h <= 0) return;
    int offset_x = sw, offset_y = sh; // repurpose sw/sh as sub-region offset
    (void)texid;
    (void)mip;

    // If we already have a VkImage, do sub-region update
    if(vkImage != VK_NULL_HANDLE) {
        int srcBpp;
        bool expandRGBA = false;
        switch(fmt) {
            case CR_ALPHA:
            case CR_LUMINANCE:
                expandRGBA = true;
                srcBpp = 1;
                break;
            case CR_LUMINANCE_ALPHA:
                expandRGBA = true;
                srcBpp = 2;
                break;
            case CR_RGB: srcBpp = 3; break;
            case CR_RGBA: srcBpp = 4; break;
            default: srcBpp = 4; break;
        }
        VkDeviceSize dstSz = (VkDeviceSize)w * (VkDeviceSize)h * 4;
        void *d = BeginImageUpload(dstSz);
        if(!d) return;
        if(expandRGBA || srcBpp == 3) {
            uint8_t *dst8 = (uint8_t *)d;
            for(int i = 0; i < w * h; i++) {
                if(srcBpp == 1) {
                    uint8_t a = ((uint8_t *)data)[i];
                    dst8[i * 4] = a;
                    dst8[i * 4 + 1] = a;
                    dst8[i * 4 + 2] = a;
                    dst8[i * 4 + 3] = a;
                } else if(srcBpp == 2) {
                    uint8_t l = ((uint8_t *)data)[i * 2], a2 = ((uint8_t *)data)[i * 2 + 1];
                    dst8[i * 4] = l;
                    dst8[i * 4 + 1] = l;
                    dst8[i * 4 + 2] = l;
                    dst8[i * 4 + 3] = a2;
                } else {
                    dst8[i * 4] = ((uint8_t *)data)[i * 3];
                    dst8[i * 4 + 1] = ((uint8_t *)data)[i * 3 + 1];
                    dst8[i * 4 + 2] = ((uint8_t *)data)[i * 3 + 2];
                    dst8[i * 4 + 3] = 0xFF;
                }
            }
        } else {
            memcpy(d, data, dstSz);
        }

        // pass内: 延迟上传到帧首 (must copy before unmap!)
        // skipDefer: FreeType字体atlas等需即时更新的纹理不走延迟
        if(GN_Vk.inRenderPass && GN_Vk.pendingUploadCnt < 256 && !skipDefer) {
            GN_Vk.pendingUploads[GN_Vk.pendingUploadCnt].img = this;
            GN_Vk.pendingUploads[GN_Vk.pendingUploadCnt].data = new uint8[dstSz];
            memcpy(GN_Vk.pendingUploads[GN_Vk.pendingUploadCnt].data, d, dstSz);
            GN_Vk.pendingUploads[GN_Vk.pendingUploadCnt].w = w;
            GN_Vk.pendingUploads[GN_Vk.pendingUploadCnt].h = h;
            GN_Vk.pendingUploads[GN_Vk.pendingUploadCnt].fmt = CR_RGBA; // staging已展开为RGBA
            GN_Vk.pendingUploadCnt++;
            return;
        }
        EndImageUpload(vkImage, (uint32_t)w, (uint32_t)h, offset_x, offset_y, false);
        return;
    }

    // Map GN_mini COLOR_FORMAT to Vulkan format.
    // CR_ALPHA/CR_LUMINANCE are expanded to RGBA in staging so the shader
    // sees (A,A,A,A) per pixel — compatible with OpenGL GL_ALPHA behaviour
    // where sampled colour is (0,0,0,alpha) and the shader multiplies
    // component-wise: outColor = texture * fragColor.
    bool expandToRGBA = false;
    int srcBpp = 4;
    switch(fmt) {
        case CR_ALPHA:
            expandToRGBA = true;
            srcBpp = 1;
            break;
        case CR_LUMINANCE:
            expandToRGBA = true;
            srcBpp = 1;
            break;
        case CR_LUMINANCE_ALPHA:
            expandToRGBA = true;
            srcBpp = 2;
            break;
        case CR_RGBA: srcBpp = 4; break;
        case CR_RGB: srcBpp = 4; break; // stored as RGBA
        default: srcBpp = 4; break;
    (void)srcBpp; // B9-3: 仅 expandToRGBA 分支用固定展开，srcBpp 保留作记录
    }
    VkFormat vkFmt = vkSrgb ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;
    VkDeviceSize dstSz = (VkDeviceSize)w * (VkDeviceSize)h * 4;

    // Release any previous Vulkan resources
    Release();

    void *d = BeginImageUpload(dstSz);
    if(!d) return;
    if(expandToRGBA) {
        // Expand narrow format to RGBA in staging
        uint8_t *dst = (uint8_t *)d;
        if(fmt == CR_ALPHA) {
            for(int i = 0; i < w * h; i++) {
                uint8_t a = ((uint8_t *)data)[i];
                dst[i * 4 + 0] = a;
                dst[i * 4 + 1] = a;
                dst[i * 4 + 2] = a;
                dst[i * 4 + 3] = a;
            }
        } else if(fmt == CR_LUMINANCE) {
            for(int i = 0; i < w * h; i++) {
                uint8_t l = ((uint8_t *)data)[i];
                dst[i * 4 + 0] = l;
                dst[i * 4 + 1] = l;
                dst[i * 4 + 2] = l;
                dst[i * 4 + 3] = 0xFF;
            }
        } else { // CR_LUMINANCE_ALPHA
            for(int i = 0; i < w * h; i++) {
                uint8_t l = ((uint8_t *)data)[i * 2 + 0];
                uint8_t a = ((uint8_t *)data)[i * 2 + 1];
                dst[i * 4 + 0] = l;
                dst[i * 4 + 1] = l;
                dst[i * 4 + 2] = l;
                dst[i * 4 + 3] = a;
            }
        }
    } else {
        memcpy(d, data, dstSz);
    }
    // pass内延迟: 新纹理创建也需要避免pass内queue submit
    if(GN_Vk.inRenderPass && GN_Vk.pendingUploadCnt < 256 && !skipDefer) {
        GN_Vk.pendingUploads[GN_Vk.pendingUploadCnt].img = this;
        GN_Vk.pendingUploads[GN_Vk.pendingUploadCnt].data = new uint8[dstSz];
        memcpy(GN_Vk.pendingUploads[GN_Vk.pendingUploadCnt].data, d, dstSz);
        GN_Vk.pendingUploads[GN_Vk.pendingUploadCnt].w = w;
        GN_Vk.pendingUploads[GN_Vk.pendingUploadCnt].h = h;
        GN_Vk.pendingUploads[GN_Vk.pendingUploadCnt].fmt = CR_RGBA;
        GN_Vk.pendingUploadCnt++;
        width = w;
        height = h;
        uRange = 1.0f;
        vRange = 1.0f;
        texnum = 1;
        return;
    }
    // Create destination image
    {
        VkImageCreateInfo ici{}; ici.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        ici.imageType = VK_IMAGE_TYPE_2D;
        ici.extent = {(uint32_t)w, (uint32_t)h, 1};
        ici.mipLevels = 1;
        ici.arrayLayers = 1;
        ici.format = vkFmt;
        ici.tiling = VK_IMAGE_TILING_OPTIMAL;
        ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        ici.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        ici.samples = VK_SAMPLE_COUNT_1_BIT;
        ici.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        {
            VkResult res = vkCreateImage(GN_Vk.device, &ici, 0, &vkImage);
            VkCheck(res, "vkCreateImage in DataFill");
        }
        VkMemoryRequirements mr;
        vkGetImageMemoryRequirements(GN_Vk.device, vkImage, &mr);
        VkMemoryAllocateInfo mai{}; mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        mai.allocationSize = mr.size;
        mai.memoryTypeIndex = FindMemType(mr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        {
            VkResult res = vkAllocateMemory(GN_Vk.device, &mai, 0, &vkImageMemory);
            VkCheck(res, "vkAllocateMemory in DataFill");
        }
        {
            VkResult res = vkBindImageMemory(GN_Vk.device, vkImage, vkImageMemory, 0);
            VkCheck(res, "vkBindImageMemory in DataFill");
        }
        vkIsLinear = false;
    }

    EndImageUpload(vkImage, (uint32_t)w, (uint32_t)h, 0, 0, true);


    // Create image view
    vkImageView = MakeImageView(vkImage, vkFmt);

    // Create sampler
    {
        VkSamplerCreateInfo sci{}; sci.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        sci.magFilter = VK_FILTER_LINEAR;
        sci.minFilter = VK_FILTER_LINEAR;
        sci.addressModeU = imageWrapMode(vkWrapU);
        sci.addressModeV = imageWrapMode(vkWrapV);
        VkResult res = vkCreateSampler(GN_Vk.device, &sci, 0, &vkSampler);
        VkCheck(res, "vkCreateSampler in DataFill");
    }

    // Allocate descriptor set
    {
        VkDescriptorSetAllocateInfo dsai{}; dsai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        dsai.descriptorPool = GN_Vk.descriptorPool;
        dsai.descriptorSetCount = 1;
        dsai.pSetLayouts = &GN_Vk.descriptorLayout;
        VkResult res = vkAllocateDescriptorSets(GN_Vk.device, &dsai, &vkDescriptorSet);
        VkCheck(res, "vkAllocateDescriptorSets in DataFill");
        VkDescriptorBufferInfo dbi = {GN_Vk.uniformBuffer, 0, sizeof(VkUBO)};
        VkDescriptorImageInfo dii = {vkSampler, vkImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        VkWriteDescriptorSet wds[2]{}; wds[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET; wds[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        wds[0].dstSet = vkDescriptorSet;
        wds[0].dstBinding = 0;
        wds[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        wds[0].descriptorCount = 1;
        wds[0].pBufferInfo = &dbi;
        wds[1].dstSet = vkDescriptorSet;
        wds[1].dstBinding = 1;
        wds[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        wds[1].descriptorCount = 1;
        wds[1].pImageInfo = &dii;
        vkUpdateDescriptorSets(GN_Vk.device, 2, wds, 0, 0);
    }

    // Update sIMAGE metadata
    width = w;
    height = h;
    uRange = 1.0f;
    vRange = 1.0f;
    texnum = 1;
}

} // namespace GNg
