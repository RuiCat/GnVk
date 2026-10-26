// sIMAGE 数据上传/填充：InitToData / CreateEmptyToData / DataFill（PNG IO 见 vkImageIo.cpp）。
#include <GN.h>
#include "GN_Vk.h"
#include "vkInternal.h"

#include <cstdint>
#include <cstring>

namespace GNg {

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

    VkFormat vkFmt = VK_FORMAT_R8G8B8A8_UNORM;
    VkDeviceSize sz = (VkDeviceSize)w * (VkDeviceSize)h * 4;

    // Create image
    {
        VkImageCreateInfo ici = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
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
        VkMemoryAllocateInfo mai = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
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
        VkSamplerCreateInfo sci = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        sci.magFilter = VK_FILTER_LINEAR;
        sci.minFilter = VK_FILTER_LINEAR;
        sci.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sci.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        VkResult res = vkCreateSampler(GN_Vk.device, &sci, 0, &vkSampler);
        VkCheck(res, "vkCreateSampler in CreateEmptyToData");
    }

    // Allocate descriptor set
    {
        VkDescriptorSetAllocateInfo dsai = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        dsai.descriptorPool = GN_Vk.descriptorPool;
        dsai.descriptorSetCount = 1;
        dsai.pSetLayouts = &GN_Vk.descriptorLayout;
        VkResult res = vkAllocateDescriptorSets(GN_Vk.device, &dsai, &vkDescriptorSet);
        VkCheck(res, "vkAllocateDescriptorSets in CreateEmptyToData");
        VkDescriptorBufferInfo dbi = {GN_Vk.uniformBuffer, 0, sizeof(VkUBO)};
        VkDescriptorImageInfo dii = {vkSampler, vkImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        VkWriteDescriptorSet wds[2] = {{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET}, {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET}};
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
            VkCommandBufferAllocateInfo ai = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
            ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            ai.commandPool = GN_Vk.commandPool;
            ai.commandBufferCount = 1;
            VkResult res = vkAllocateCommandBuffers(GN_Vk.device, &ai, &cb);
            VkCheck(res, "vkAllocateCommandBuffers in CreateEmptyToData");
        }
        {
            VkCommandBufferBeginInfo bi = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
            bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            vkBeginCommandBuffer(cb, &bi);
        }
        {
            VkImageMemoryBarrier b = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
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
            VkSubmitInfo si = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
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
        VkDeviceSize srcSz = (VkDeviceSize)w * (VkDeviceSize)h * srcBpp;
        VkDeviceSize dstSz = (VkDeviceSize)w * (VkDeviceSize)h * 4;
        VkBuffer stagingBuf;
        VkDeviceMemory stagingMem;
        CreateBuf(dstSz, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                  stagingBuf, stagingMem);
        void *d;
        vkMapMemory(GN_Vk.device, stagingMem, 0, dstSz, 0, &d);
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
        if(s_inRenderPass && s_pendingUploadCnt < 256 && !skipDefer) {
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

        {
            VkCommandBufferAllocateInfo ai = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
            ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            ai.commandPool = GN_Vk.commandPool;
            ai.commandBufferCount = 1;
            VkCommandBuffer cb;
            VkResult res = vkAllocateCommandBuffers(GN_Vk.device, &ai, &cb);
            VkCheck(res, "vkAllocateCommandBuffers in DataFill(sub-region)");
            VkCommandBufferBeginInfo bi = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
            bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            vkBeginCommandBuffer(cb, &bi);
            VkImageMemoryBarrier b1 = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
            b1.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            b1.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            b1.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            b1.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            b1.image = vkImage;
            b1.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            b1.subresourceRange.levelCount = 1;
            b1.subresourceRange.layerCount = 1;
            b1.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
            b1.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, 0, 0, 0, 1, &b1);
            VkBufferImageCopy rgn = {};
            rgn.bufferOffset = 0;
            rgn.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            rgn.imageSubresource.layerCount = 1;
            rgn.imageOffset = {offset_x, offset_y, 0};
            rgn.imageExtent = {(uint32_t)w, (uint32_t)h, 1};
            vkCmdCopyBufferToImage(cb, stagingBuf, vkImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &rgn);
            VkImageMemoryBarrier b2 = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
            b2.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            b2.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            b2.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            b2.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            b2.image = vkImage;
            b2.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            b2.subresourceRange.levelCount = 1;
            b2.subresourceRange.layerCount = 1;
            b2.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            b2.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, 0, 0, 0, 1, &b2);
            vkEndCommandBuffer(cb);
            VkFence f;
            VkFenceCreateInfo fci = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
            vkCreateFence(GN_Vk.device, &fci, 0, &f);
            VkSubmitInfo si = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
            si.commandBufferCount = 1;
            si.pCommandBuffers = &cb;
            vkQueueSubmit(GN_Vk.graphicsQueue, 1, &si, f);
            vkWaitForFences(GN_Vk.device, 1, &f, VK_TRUE, UINT64_MAX);
            vkDestroyFence(GN_Vk.device, f, 0);
            vkFreeCommandBuffers(GN_Vk.device, GN_Vk.commandPool, 1, &cb);
        }
        vkDestroyBuffer(GN_Vk.device, stagingBuf, 0);
        vkFreeMemory(GN_Vk.device, stagingMem, 0);
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
    }
    VkFormat vkFmt = VK_FORMAT_R8G8B8A8_UNORM;
    VkDeviceSize srcSz = (VkDeviceSize)w * (VkDeviceSize)h * srcBpp;
    VkDeviceSize dstSz = (VkDeviceSize)w * (VkDeviceSize)h * 4;

    // Release any previous Vulkan resources
    Release();

    // Create staging buffer
    VkBuffer stagingBuf;
    VkDeviceMemory stagingMem;
    CreateBuf(dstSz, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
              stagingBuf, stagingMem);
    void *d;
    vkMapMemory(GN_Vk.device, stagingMem, 0, dstSz, 0, &d);
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
    if(s_inRenderPass && s_pendingUploadCnt < 256 && !skipDefer) {
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
        width = w;
        height = h;
        uRange = 1.0f;
        vRange = 1.0f;
        texnum = 1;
        return;
    }
    vkUnmapMemory(GN_Vk.device, stagingMem);

    // Create destination image
    {
        VkImageCreateInfo ici = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
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
        VkMemoryAllocateInfo mai = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
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

    // One-time command: transition → copy → transition
    {
        VkCommandBufferAllocateInfo ai = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        ai.commandPool = GN_Vk.commandPool;
        ai.commandBufferCount = 1;
        VkCommandBuffer cb;
        VkResult res = vkAllocateCommandBuffers(GN_Vk.device, &ai, &cb);
        VkCheck(res, "vkAllocateCommandBuffers in DataFill");
        {
            VkCommandBufferBeginInfo bi = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
            bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            vkBeginCommandBuffer(cb, &bi);
        }
        {
            VkImageMemoryBarrier b1 = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
            b1.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            b1.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            b1.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            b1.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            b1.image = vkImage;
            b1.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            b1.subresourceRange.levelCount = 1;
            b1.subresourceRange.layerCount = 1;
            b1.srcAccessMask = 0;
            b1.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, 0, 0, 0, 1, &b1);
        }
        {
            VkBufferImageCopy rgn = {};
            rgn.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            rgn.imageSubresource.layerCount = 1;
            rgn.imageExtent = {(uint32_t)w, (uint32_t)h, 1};
            vkCmdCopyBufferToImage(cb, stagingBuf, vkImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &rgn);
        }
        {
            VkImageMemoryBarrier b2 = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
            b2.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            b2.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            b2.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            b2.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            b2.image = vkImage;
            b2.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            b2.subresourceRange.levelCount = 1;
            b2.subresourceRange.layerCount = 1;
            b2.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            b2.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, 0, 0, 0, 1, &b2);
        }
        vkEndCommandBuffer(cb);
        {
            VkFenceCreateInfo fci = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
            VkFence uploadFence;
            vkCreateFence(GN_Vk.device, &fci, 0, &uploadFence);
            VkSubmitInfo si = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
            si.commandBufferCount = 1;
            si.pCommandBuffers = &cb;
            vkQueueSubmit(GN_Vk.graphicsQueue, 1, &si, uploadFence);
            vkWaitForFences(GN_Vk.device, 1, &uploadFence, VK_TRUE, UINT64_MAX);
            vkDestroyFence(GN_Vk.device, uploadFence, 0);
        }
        vkFreeCommandBuffers(GN_Vk.device, GN_Vk.commandPool, 1, &cb);
    }

    // Cleanup staging
    vkDestroyBuffer(GN_Vk.device, stagingBuf, 0);
    vkFreeMemory(GN_Vk.device, stagingMem, 0);

    // Create image view
    vkImageView = MakeImageView(vkImage, vkFmt);

    // Create sampler
    {
        VkSamplerCreateInfo sci = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        sci.magFilter = VK_FILTER_LINEAR;
        sci.minFilter = VK_FILTER_LINEAR;
        sci.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sci.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        VkResult res = vkCreateSampler(GN_Vk.device, &sci, 0, &vkSampler);
        VkCheck(res, "vkCreateSampler in DataFill");
    }

    // Allocate descriptor set
    {
        VkDescriptorSetAllocateInfo dsai = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        dsai.descriptorPool = GN_Vk.descriptorPool;
        dsai.descriptorSetCount = 1;
        dsai.pSetLayouts = &GN_Vk.descriptorLayout;
        VkResult res = vkAllocateDescriptorSets(GN_Vk.device, &dsai, &vkDescriptorSet);
        VkCheck(res, "vkAllocateDescriptorSets in DataFill");
        VkDescriptorBufferInfo dbi = {GN_Vk.uniformBuffer, 0, sizeof(VkUBO)};
        VkDescriptorImageInfo dii = {vkSampler, vkImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        VkWriteDescriptorSet wds[2] = {{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET}, {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET}};
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
