// 2D 绘制：纹理/纯色四边形、逐顶点色、旋转与边框（写入共享顶点批）。
#include <GN.h>
#include "GN_Vk.h"
#include "vkInternal.h"

#include <cmath>
#include <cstring>

namespace GNg {

// 2D 绘制
void ImageDraw(sIMAGE *img, int x, int y, int w, int h, unsigned int color, unsigned short texID, unsigned short blendmode, float u1, float v1, float u2, float v2) {
    if(!GN_Vk.initialized) return;
    // Allow null image for solid color drawing (used by text/line drawing)
    if(img && !img->vkImageView) return;
    if(!gVBuf) {
        gVMax = 65536;
        gVBuf = new VkVertex[gVMax];
    }
    FlushBatchIfScissorDirty();
    VkDescriptorSet ds;
    if(img && img->vkDescriptorSet) ds = img->vkDescriptorSet;
    else if(GN_Vk.defaultDS) ds = GN_Vk.defaultDS;
    else return;
    BatchSwitch(ds);
    vkCmdBindDescriptorSets(GN_Vk.commandBuffers[GN_Vk.currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, GN_Vk.pipelineLayout, 0, 1, &ds, 0, 0);
    // Select blend pipeline per draw call
    static unsigned short s_lastBlendMode = 1;
    if(blendmode != s_lastBlendMode) {
        VkPipeline pipe = GN_Vk.pipeline; // default: eBLEND_NORMAL (1)
        switch(blendmode) {
            case 0: pipe = GN_Vk.pipelineNull; break; // NULL: no blend
            case 2: pipe = GN_Vk.pipelineBright; break; // BRIGHT: additive
            case 11:
                pipe = GN_Vk.pipelineMultiply;
                break; // MODULATE: multiply
            // Other modes fall back to normal blend (1)
            default: pipe = GN_Vk.pipeline; break;
        }
        vkCmdBindPipeline(GN_Vk.commandBuffers[GN_Vk.currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
        s_lastBlendMode = blendmode;
    }
    float u0 = (u1 == 0 && v1 == 0 && u2 == 0 && v2 == 0) ? 0.0f : u1, v0 = (u1 == 0 && v1 == 0 && u2 == 0 && v2 == 0) ? 0.0f : v1;
    float ue = (u1 == 0 && v1 == 0 && u2 == 0 && v2 == 0) ? (img ? img->uRange : 1.0f) : u2, ve = (u1 == 0 && v1 == 0 && u2 == 0 && v2 == 0) ? (img ? img->vRange : 1.0f) : v2;
    // ── texID 子纹理映射 (多帧图集: 9-slice, GB2312字体等) ──
    if(img && img->texnum > 1 && texID < img->texnum) {
        // 最优行列网格布局
        int cols, rows;
        if(img->texnum == 9) {
            cols = 3;
            rows = 3;
        } else if(img->texnum <= 2) {
            cols = img->texnum;
            rows = 1;
        } else {
            cols = (int)ceilf(sqrtf((float)img->texnum));
            rows = (int)ceilf((float)img->texnum / (float)cols);
        }
        int tid = texID > 0 ? (int)texID : 0;
        int col = tid % cols, row = tid / cols;
        float fw = 1.0f / (float)cols; // 单帧在图集中的归一化宽
        float fh = 1.0f / (float)rows; // 单帧在图集中的归一化高
        if(u1 == 0 && v1 == 0 && u2 == 0 && v2 == 0) {
            // 无显式UV: 使用整帧
            u0 = img->uRange * col * fw;
            v0 = img->vRange * row * fh;
            ue = img->uRange * (col + 1) * fw;
            ve = img->vRange * (row + 1) * fh;
        } else {
            // 显式UV偏移到图集中对应帧的位置
            u0 = img->uRange * (col * fw + u1 * fw);
            v0 = img->vRange * (row * fh + v1 * fh);
            ue = img->uRange * (col * fw + u2 * fw);
            ve = img->vRange * (row * fh + v2 * fh);
        }
    }
    float dw = w ? w : (img ? (float)img->width : 1.0f), dh = h ? h : (img ? (float)img->height : 1.0f);
    float r = ((color >> 16) & 0xff) / 255.0f, g = ((color >> 8) & 0xff) / 255.0f, b = (color & 0xff) / 255.0f, a = ((color >> 24) & 0xff) / 255.0f;
    float x2 = x + dw, y2 = y + dh;
    VkVertex vv[6] = {{x, y, u0, v0, r, g, b, a}, {x2, y, ue, v0, r, g, b, a}, {x2, y2, ue, ve, r, g, b, a}, {x, y, u0, v0, r, g, b, a}, {x2, y2, ue, ve, r, g, b, a}, {x, y2, u0, ve, r, g, b, a}};
    GrowVBufIfNeeded(6);
    memcpy(gVBuf + gVCnt, vv, sizeof(vv));
    gVCnt += 6;
}

void LineFrame2dDraw(int x, int y, int w, int h, int lw, unsigned int color, unsigned short blend) {
    if(!GN_Vk.initialized) return;
    FlushBatchIfScissorDirty();
    float r = ((color >> 16) & 0xff) / 255.0f, g = ((color >> 8) & 0xff) / 255.0f, b = (color & 0xff) / 255.0f, a = ((color >> 24) & 0xff) / 255.0f;
    // Simplified: draw 4 thin rects
    ImageDraw(0, x, y, w, lw, color, 0, 0, 0, 0, 0, 0);
    ImageDraw(0, x, y + h - lw, w, lw, color, 0, 0, 0, 0, 0, 0);
    ImageDraw(0, x, y, lw, h, color, 0, 0, 0, 0, 0, 0);
    ImageDraw(0, x + w - lw, y, lw, h, color, 0, 0, 0, 0, 0, 0);
}

// 4-vertex quad with per-vertex colors — supports gradient effects
void ImageDraw4P(sIMAGE *img, int x, int y, int w, int h,
                 unsigned int c0, unsigned int c1,
                 unsigned int c2, unsigned int c3,
                 unsigned short blend) {
    if(!GN_Vk.initialized) return;
    if(img && !img->vkImageView) return;
    if(!gVBuf) {
        gVMax = 65536;
        gVBuf = new VkVertex[gVMax];
    }
    FlushBatchIfScissorDirty();
    VkDescriptorSet ds;
    if(img && img->vkDescriptorSet) ds = img->vkDescriptorSet;
    else if(GN_Vk.defaultDS) ds = GN_Vk.defaultDS;
    else return;

    BatchSwitch(ds);
    vkCmdBindDescriptorSets(GN_Vk.commandBuffers[GN_Vk.currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, GN_Vk.pipelineLayout, 0, 1, &ds, 0, 0);

    auto fc = [](unsigned int c) -> float { return ((c >> 24) & 0xff) / 255.0f; };
    auto fr = [](unsigned int c) -> float { return ((c >> 16) & 0xff) / 255.0f; };
    auto fg = [](unsigned int c) -> float { return ((c >> 8) & 0xff) / 255.0f; };
    auto fb = [](unsigned int c) -> float { return (c & 0xff) / 255.0f; };
    float u0 = 0.0f, v0 = 0.0f, ue = img ? img->uRange : 1.0f, ve = img ? img->vRange : 1.0f;
    float x2 = x + w, y2 = y + h;
    VkVertex vv[6] = {
        {(float)x, (float)y, u0, v0, fr(c0), fg(c0), fb(c0), fc(c0)},
        {(float)x2, (float)y, ue, v0, fr(c1), fg(c1), fb(c1), fc(c1)},
        {(float)x2, (float)y2, ue, ve, fr(c2), fg(c2), fb(c2), fc(c2)},
        {(float)x, (float)y, u0, v0, fr(c0), fg(c0), fb(c0), fc(c0)},
        {(float)x2, (float)y2, ue, ve, fr(c2), fg(c2), fb(c2), fc(c2)},
        {(float)x, (float)y2, u0, ve, fr(c3), fg(c3), fb(c3), fc(c3)}};
    GrowVBufIfNeeded(6);
    memcpy(gVBuf + gVCnt, vv, sizeof(vv));
    gVCnt += 6;
}

// Float-vertex overload: uses caller-provided vertex positions
void ImageDraw4P(sIMAGE *img, float *vt, unsigned int color, unsigned short texID,
                 unsigned short blend, float u1, float v1, float u2, float v2) {
    if(!GN_Vk.initialized) return;
    if(img && !img->vkImageView) return;
    if(!gVBuf) {
        gVMax = 65536;
        gVBuf = new VkVertex[gVMax];
    }
    FlushBatchIfScissorDirty();
    VkDescriptorSet ds;
    if(img && img->vkDescriptorSet) ds = img->vkDescriptorSet;
    else if(GN_Vk.defaultDS) ds = GN_Vk.defaultDS;
    else return;
    (void)texID;

    BatchSwitch(ds);
    vkCmdBindDescriptorSets(GN_Vk.commandBuffers[GN_Vk.currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, GN_Vk.pipelineLayout, 0, 1, &ds, 0, 0);

    float r = ((color >> 16) & 0xff) / 255.0f, g = ((color >> 8) & 0xff) / 255.0f, b = (color & 0xff) / 255.0f, a = ((color >> 24) & 0xff) / 255.0f;
    VkVertex vv[6] = {
        {vt[0], vt[1], u1, v1, r, g, b, a}, {vt[2], vt[3], u2, v1, r, g, b, a}, {vt[4], vt[5], u2, v2, r, g, b, a}, {vt[0], vt[1], u1, v1, r, g, b, a}, {vt[4], vt[5], u2, v2, r, g, b, a}, {vt[6], vt[7], u1, v2, r, g, b, a}};
    GrowVBufIfNeeded(6);
    memcpy(gVBuf + gVCnt, vv, sizeof(vv));
    gVCnt += 6;
}

// Rotation drawing: apply 2D rotation around center point (cx,cy within 0..1)
void ImageDrawRot(sIMAGE *img, int x, int y, int w, int h,
                  float angle, float cx, float cy,
                  unsigned int color, int frame_id) {
    (void)frame_id;
    if(!GN_Vk.initialized) return;
    if(img && !img->vkImageView) return;
    FlushBatchIfScissorDirty();
    float rad = angle * 3.14159265f / 180.0f;
    float co = cosf(rad), si = sinf(rad);
    float px[4] = {0.0f, (float)w, (float)w, 0.0f};
    float py[4] = {0.0f, 0.0f, (float)h, (float)h};
    for(int i = 0; i < 4; i++) {
        float dx = px[i] - cx * w, dy = py[i] - cy * h;
        px[i] = x + cx * w + dx * co - dy * si;
        py[i] = y + cy * h + dx * si + dy * co;
    }
    float u0 = 0.0f, v0 = 0.0f, ue = img ? img->uRange : 1.0f, ve = img ? img->vRange : 1.0f;
    float r = ((color >> 16) & 0xff) / 255.0f, g = ((color >> 8) & 0xff) / 255.0f, b = (color & 0xff) / 255.0f, a = ((color >> 24) & 0xff) / 255.0f;
    VkVertex vv[6] = {
        {px[0], py[0], u0, v0, r, g, b, a}, {px[1], py[1], ue, v0, r, g, b, a}, {px[2], py[2], ue, ve, r, g, b, a}, {px[0], py[0], u0, v0, r, g, b, a}, {px[2], py[2], ue, ve, r, g, b, a}, {px[3], py[3], u0, ve, r, g, b, a}};
    if(!gVBuf) {
        gVMax = 65536;
        gVBuf = new VkVertex[gVMax];
    }
    if(img && img->vkDescriptorSet) {
        // using shared s_lastDS
        if(img->vkDescriptorSet != s_lastDS && gVCnt > 0) {
            void *d;
            vkMapMemory(GN_Vk.device, GN_Vk.vertexMemory, 0, gVCnt * sizeof(VkVertex), 0, &d);
            memcpy(d, gVBuf, gVCnt * sizeof(VkVertex));
            vkUnmapMemory(GN_Vk.device, GN_Vk.vertexMemory);
            VkDeviceSize o[] = {0};
            vkCmdBindVertexBuffers(GN_Vk.commandBuffers[GN_Vk.currentFrame], 0, 1, &GN_Vk.vertexBuffer, o);
            vkCmdDraw(GN_Vk.commandBuffers[GN_Vk.currentFrame], gVCnt, 1, 0, 0);
            gVCnt = 0;
        }
        s_lastDS = img->vkDescriptorSet;
        vkCmdBindDescriptorSets(GN_Vk.commandBuffers[GN_Vk.currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, GN_Vk.pipelineLayout, 0, 1, &img->vkDescriptorSet, 0, 0);
    }
    GrowVBufIfNeeded(6);
    memcpy(gVBuf + gVCnt, vv, sizeof(vv));
    gVCnt += 6;
}

void ImageDrawDitherBlur(sIMAGE *img, int x, int y, int w, int h,
                         unsigned int color, unsigned short tid,
                         unsigned short blend, int blur, float dither) {
    ImageDraw(img, x, y, w, h, color, tid, blend);
}

void ImageDrawMP(sIMAGE *img, sDRAW_RANGE *ranges, int count,
                 unsigned int color, unsigned short blend) {
    for(int i = 0; i < count; i++)
        ImageDraw(img, (int)ranges[i].x, (int)ranges[i].y,
                  (int)ranges[i].w, (int)ranges[i].h, color, ranges[i].tid, blend,
                  ranges[i].u1, ranges[i].v1, ranges[i].u2, ranges[i].v2);
}

void Draw2DAlphaDirectionSet(unsigned char dir) {
    (void)dir;
}
unsigned char Draw2DAlphaDirectionGet() {
    return 0;
}

// Alpha color key state
static unsigned int s_alphaColorKey = 0;
void SetAlphaColorKey(unsigned int color) {
    s_alphaColorKey = color;
}

// Extended ImageDraw with full UV control
void ImageDrawFull(sIMAGE *img, int x, int y, int w, int h,
                   unsigned int color, unsigned short texID,
                   unsigned short blend, float u1, float v1, float u2, float v2) {
    ImageDraw(img, x, y, w, h, color, texID, blend, u1, v1, u2, v2);
}

// Quick text draw — convenience wrapper
// Full implementation requires cTEXT_ART (GN_2D.h); stubbed for now as
// actual text rendering goes through cTEXT_ART::Show() / GN_RunTextOut
void TextDraw(void *textArt, const char *text, int x, int y, int w, int h) {
    (void)textArt;
    (void)text;
    (void)x;
    (void)y;
    (void)w;
    (void)h;
    // Text rendering in Vulkan is handled by GN_RunTextOut (core/GN_RunRec.cpp)
    // and cTEXT_ART::Show (graphics/GN_Font.cpp) which use ImageDrawMP internally.
}

// Draw2DLine — line segment (GN_Vk.h float-overload version)
void Draw2DLine(sIMAGE *img, float x1, float y1, float x2, float y2,
                float width, float center) {
    if(!GN_Vk.initialized) return;
    // Compute perpendicular direction and build a rotated thin quad
    float dx = x2 - x1, dy = y2 - y1;
    float len = sqrtf(dx * dx + dy * dy);
    if(len < 0.001f) return;
    float nx = -dy / len, ny = dx / len; // perpendicular
    float hw = width * 0.5f;
    float cOff = (center - 0.5f) * width;
    float ox = nx * cOff, oy = ny * cOff;
    float px[4] = {x1 + ox - nx * hw, x2 + ox - nx * hw, x2 + ox + nx * hw, x1 + ox + nx * hw};
    float py[4] = {y1 + oy - ny * hw, y2 + oy - ny * hw, y2 + oy + ny * hw, y1 + oy + ny * hw};
    float vt[8] = {px[0], py[0], px[1], py[1], px[2], py[2], px[3], py[3]};
    ImageDraw4P(img, vt, 0xffffffff, 0, 0, 0.0f, 0.0f, 1.0f, 1.0f);
}

} // namespace GNg
