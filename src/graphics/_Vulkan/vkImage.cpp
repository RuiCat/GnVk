// sIMAGE 生命周期与 CPU 回读：构造/析构、复制、Release、CopyOut、DataGet。
#include <GN.h>
#include "GN_Vk.h"
#include "vkInternal.h"

#include <cstdint>
#include <cstring>

namespace GNg {

sIMAGE::sIMAGE() : width(128), height(128), uRange(1), vRange(1), texnum(0), fileName(0), hasMipmap(false), add_0000(false), cloneImg(false), vkImage(VK_NULL_HANDLE), vkImageMemory(VK_NULL_HANDLE), vkImageView(VK_NULL_HANDLE), vkSampler(VK_NULL_HANDLE), vkDescriptorSet(VK_NULL_HANDLE), vkIsLinear(false), skipDefer(false), z__attrb(0) {}
sIMAGE::~sIMAGE() {
    Release();
}

sIMAGE::sIMAGE(const sIMAGE &src) : sIMAGE() {
    *this = src;
}

sIMAGE &sIMAGE::operator=(const sIMAGE &src) {
    if(this != &src) {
        Release();
        width = src.width;
        height = src.height;
        uRange = src.uRange;
        vRange = src.vRange;
        texnum = src.texnum;
        cFormat = src.cFormat;
        add_0000 = src.add_0000;
    }
    return *this;
}

bool sIMAGE::RenderTargetSupport() {
    return true;
}

void sIMAGE::Release(bool immed) {
    if(vkImageView) {
        vkDestroyImageView(GN_Vk.device, vkImageView, 0);
        vkImageView = VK_NULL_HANDLE;
    }
    if(vkImage) {
        vkDestroyImage(GN_Vk.device, vkImage, 0);
        vkImage = VK_NULL_HANDLE;
    }
    if(vkImageMemory) {
        vkFreeMemory(GN_Vk.device, vkImageMemory, 0);
        vkImageMemory = VK_NULL_HANDLE;
    }
    if(vkSampler) {
        vkDestroySampler(GN_Vk.device, vkSampler, 0);
        vkSampler = VK_NULL_HANDLE;
    }
    texnum = 0;
}

void sIMAGE::CopyOut(sIMAGE *dst) {
    if(!dst || !GN_Vk.initialized) return;
    if(!vkImage) {
        *dst = *this;
        return;
    }
    // GPU copy: read source pixels → write to destination
    uint8_t *px = DataGet();
    if(px) {
        dst->CreateEmptyToData(width, height, 1, 0, 1, false, false);
        dst->DataFill(px, CR_RGBA, width, height, 0, 0, 0, false);
        dst->uRange = uRange;
        dst->vRange = vRange;
        delete[] px;
    } else {
        *dst = *this;
    }
}

P<sIMAGE> sIMAGE::CopyOut(int x, int y, int w, int h, bool alphaOnly, bool flipY) {
    P<sIMAGE> p(1);
    if(!GN_Vk.initialized || !vkImage || w <= 0 || h <= 0) {
        *p = *this;
        return p;
    }
    // Read full image, extract sub-region
    uint8_t *src = DataGet();
    if(!src) {
        *p = *this;
        return p;
    }
    int sx = x < 0 ? 0 : (x >= width ? width - 1 : x);
    int sy = y < 0 ? 0 : (y >= height ? height - 1 : y);
    int sw = (sx + w > width) ? width - sx : w;
    int sh = (sy + h > height) ? height - sy : h;
    uint8_t *sub = new uint8_t[sw * sh * 4];
    for(int row = 0; row < sh; row++) {
        int srcRow = flipY ? (height - 1 - (sy + row)) : (sy + row);
        for(int col = 0; col < sw; col++) {
            int si = ((srcRow * width) + sx + col) * 4;
            int di = (row * sw + col) * 4;
            if(alphaOnly) {
                sub[di + 0] = src[si + 3];
                sub[di + 1] = src[si + 3];
                sub[di + 2] = src[si + 3];
                sub[di + 3] = src[si + 3];
            } else {
                sub[di + 0] = src[si + 0];
                sub[di + 1] = src[si + 1];
                sub[di + 2] = src[si + 2];
                sub[di + 3] = src[si + 3];
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
    if(!GN_Vk.initialized || !vkImage) {
        *p = *this;
        return p;
    }
    uint8_t *px = DataGet();
    if(px) {
        p->CreateEmptyToData(width, height, 1, 0, 1, false, false);
        p->DataFill(px, CR_RGBA, width, height, 0, 0, 0, false);
        p->uRange = uRange;
        p->vRange = vRange;
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
    VkBuffer stagingBuf;
    VkDeviceMemory stagingMem;
    CreateBuf(sz, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
              stagingBuf, stagingMem);
    // One-time command: transition → copy → transition back
    VkCommandBuffer cb;
    {
        VkCommandBufferAllocateInfo ai = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        ai.commandPool = GN_Vk.commandPool;
        ai.commandBufferCount = 1;
        VkResult res = vkAllocateCommandBuffers(GN_Vk.device, &ai, &cb);
        VkCheck(res, "vkAllocateCommandBuffers in DataGet");
    }
    {
        VkCommandBufferBeginInfo bi = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cb, &bi);
    }
    // Transition image to TRANSFER_SRC
    {
        VkImageMemoryBarrier b = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        b.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        b.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.image = vkImage;
        b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        b.subresourceRange.levelCount = 1;
        b.subresourceRange.layerCount = 1;
        b.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        b.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, 0, 0, 0, 1, &b);
    }
    // Copy image to buffer
    {
        VkBufferImageCopy rgn = {};
        rgn.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        rgn.imageSubresource.layerCount = 1;
        rgn.imageExtent = {(uint32_t)width, (uint32_t)height, 1};
        vkCmdCopyImageToBuffer(cb, vkImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, stagingBuf, 1, &rgn);
    }
    // Transition back
    {
        VkImageMemoryBarrier b = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        b.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        b.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.image = vkImage;
        b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        b.subresourceRange.levelCount = 1;
        b.subresourceRange.layerCount = 1;
        b.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, 0, 0, 0, 1, &b);
    }
    vkEndCommandBuffer(cb);
    {
        VkFenceCreateInfo fci = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        VkFence f;
        vkCreateFence(GN_Vk.device, &fci, 0, &f);
        VkSubmitInfo si = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
        si.commandBufferCount = 1;
        si.pCommandBuffers = &cb;
        vkQueueSubmit(GN_Vk.graphicsQueue, 1, &si, f);
        vkWaitForFences(GN_Vk.device, 1, &f, VK_TRUE, UINT64_MAX);
        vkDestroyFence(GN_Vk.device, f, 0);
    }
    vkFreeCommandBuffers(GN_Vk.device, GN_Vk.commandPool, 1, &cb);
    // Read back
    uint8_t *result = new uint8_t[sz];
    void *d;
    vkMapMemory(GN_Vk.device, stagingMem, 0, sz, 0, &d);
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
    int sx = x < 0 ? 0 : (x >= width ? width - 1 : x);
    int sy = y < 0 ? 0 : (y >= height ? height - 1 : y);
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
    if(w) *w = iw;
    h = (short)ih;
    x = 0;
    y = 0;
    rw = (short)iw;
    fmt = (unsigned short)ifmt;
    (void)flip;
    return data;
}

} // namespace GNg
