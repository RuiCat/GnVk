// Vulkan 纹理 IO：PNG 文件/内存加载、图集打包、保存与帧缓冲回读。
#include <GN.h>
#include "GN_Vk.h"
#include "vkInternal.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <png.h>

namespace GNg {

// ── PNG 加载辅助函数 ────────────────────────────────────
static void PngReadCB(png_structp png, png_bytep data, png_size_t len) {
    fread(data, 1, len, (FILE *)png_get_io_ptr(png));
}
static uint8_t *LoadPngFile(const char *path, int *w_out, int *h_out) {
    FILE *fp = fopen(path, "rb");
    if(!fp) return nullptr;
    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
    if(!png) {
        fclose(fp);
        return nullptr;
    }
    png_infop info = png_create_info_struct(png);
    if(!info) {
        png_destroy_read_struct(&png, 0, 0);
        fclose(fp);
        return nullptr;
    }
    png_set_read_fn(png, fp, PngReadCB);
    png_read_info(png, info);
    int w = png_get_image_width(png, info), h = png_get_image_height(png, info);
    // Expand to RGBA
    if(png_get_color_type(png, info) == PNG_COLOR_TYPE_RGB)
        png_set_filler(png, 0xFF, PNG_FILLER_AFTER);
    if(png_get_color_type(png, info) == PNG_COLOR_TYPE_PALETTE)
        png_set_palette_to_rgb(png);
    if(png_get_color_type(png, info) == PNG_COLOR_TYPE_GRAY && png_get_bit_depth(png, info) < 8)
        png_set_expand_gray_1_2_4_to_8(png);
    if(png_get_valid(png, info, PNG_INFO_tRNS))
        png_set_tRNS_to_alpha(png);
    if(png_get_color_type(png, info) == PNG_COLOR_TYPE_GRAY ||
       png_get_color_type(png, info) == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_gray_to_rgb(png);
    png_read_update_info(png, info);
    w = png_get_image_width(png, info);
    h = png_get_image_height(png, info);
    png_bytep *rows = new png_bytep[h];
    uint8_t *px = new uint8_t[w * h * 4];
    for(int i = 0; i < h; i++) rows[i] = px + i * w * 4;
    png_read_image(png, rows);
    png_destroy_read_struct(&png, &info, 0);
    fclose(fp);
    delete[] rows;
    *w_out = w;
    *h_out = h;
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
        out->width = fw;
        out->height = fh;
        out->uRange = 1.0f;
        out->vRange = 1.0f;
        out->texnum = 1;
        UploadImageToGPU(out, fw, fh, firstFrame);
        delete[] firstFrame;
        return true;
    }

    // ── 多帧图集打包 ──────────────────────────────
    // 最优行列网格
    int cols, rows;
    if(num == 9) {
        cols = 3;
        rows = 3;
    } else if(num <= 2) {
        cols = num;
        rows = 1;
    } else {
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
    out->width = atlasW;
    out->height = atlasH;
    out->uRange = 1.0f;
    out->vRange = 1.0f;
    out->texnum = num;
    UploadImageToGPU(out, atlasW, atlasH, atlas);
    delete[] atlas;
    return true;
}

// Extended image load with optional resize
bool ImageLoadEx(const char *fn, sIMAGE *out, int w, int h) {
    if(!ImageLoad(fn, 1, out, 0)) return false;
    (void)w;
    (void)h; // resize not yet implemented
    return true;
}

// 从内存 PNG 载入纹理：解码复用 GN_3Dtexture 的 PNG 解码器（含 setjmp 错误回收），
// 再走 CreateEmptyToData/DataFill 的延迟上传路径。
bool ImageLoadFromMem(const void *data, int len, sIMAGE *out) {
    if(!data || len <= 0 || !out) return false;
    GN3d::sTEXTURE_IMAGE image;
    if(!GN3d::decodePngMemory(data, (size_t)len, image)) return false;
    out->CreateEmptyToData((int)image.aWidth, (int)image.aHeight, 1, 0, 1, false, false);
    out->DataFill(image.aPixels.data(), CR_RGBA, (int)image.aWidth, (int)image.aHeight, 0, 0, 0,
                  false);
    return true;
}

// Save image to PNG file
void ImageSaveToFile(const char *fn, sIMAGE *img) {
    if(!fn || !img || !GN_Vk.initialized) return;
    uint8_t *px = img->DataGet();
    if(!px) return;
    // Write PNG via libpng
    FILE *fp = fopen(fn, "wb");
    if(!fp) {
        delete[] px;
        return;
    }
    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
    if(!png) {
        fclose(fp);
        delete[] px;
        return;
    }
    png_infop info = png_create_info_struct(png);
    if(!info) {
        png_destroy_write_struct(&png, 0);
        fclose(fp);
        delete[] px;
        return;
    }
    png_init_io(png, fp);
    png_set_IHDR(png, info, img->width, img->height, 8, PNG_COLOR_TYPE_RGBA,
                 PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_bytep *rows = new png_bytep[img->height];
    for(int i = 0; i < img->height; i++) rows[i] = px + i * img->width * 4;
    png_set_rows(png, info, rows);
    png_write_png(png, info, PNG_TRANSFORM_IDENTITY, 0);
    delete[] rows;
    delete[] px;
    png_destroy_write_struct(&png, &info);
    fclose(fp);
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
    if(x < 0) {
        w += x;
        x = 0;
    }
    if(y < 0) {
        h += y;
        y = 0;
    }
    if(x + w > (int)sw) w = sw - x;
    if(y + h > (int)sh) h = sh - y;
    if(w <= 0 || h <= 0) return nullptr;
    VkDeviceSize sz = (VkDeviceSize)w * h * 4;
    // Create staging buffer
    VkBuffer stagingBuf;
    VkDeviceMemory stagingMem;
    CreateBuf(sz, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
              stagingBuf, stagingMem);
    VkImage srcImg = GN_Vk.swapchainImages[GN_Vk.currentFrame];
    // One-time command: transition → copy region → transition back
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
    {
        VkImageMemoryBarrier b = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        b.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        b.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.image = srcImg;
        b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        b.subresourceRange.levelCount = 1;
        b.subresourceRange.layerCount = 1;
        b.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        b.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, 0, 0, 0, 1, &b);
    }
    {
        VkBufferImageCopy rgn = {};
        rgn.bufferOffset = 0;
        rgn.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        rgn.imageSubresource.layerCount = 1;
        rgn.imageOffset = {x, y, 0};
        rgn.imageExtent = {(uint32_t)w, (uint32_t)h, 1};
        vkCmdCopyImageToBuffer(cb, srcImg, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, stagingBuf, 1, &rgn);
    }
    {
        VkImageMemoryBarrier b = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        b.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        b.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.image = srcImg;
        b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        b.subresourceRange.levelCount = 1;
        b.subresourceRange.layerCount = 1;
        b.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        b.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, 0, 0, 0, 1, &b);
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
    // Read back
    uint8_t *result = new uint8_t[sz];
    void *d;
    vkMapMemory(GN_Vk.device, stagingMem, 0, sz, 0, &d);
    memcpy(result, d, sz);
    vkUnmapMemory(GN_Vk.device, stagingMem);
    vkDestroyBuffer(GN_Vk.device, stagingBuf, 0);
    vkFreeMemory(GN_Vk.device, stagingMem, 0);
    if(size_out) *size_out = (int)sz;
    return result;
}

void GN_VkReadBackFree(uint8_t *data) {
    delete[] data;
}

} // namespace GNg
