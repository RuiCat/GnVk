/****************************************************************************
 *  GN_mini — FreeType 字形缓存实现
 *  替代预生成 PNG 字库，支持 TTF/OTF + 完整 Unicode
 ****************************************************************************/
#ifdef GN_HAS_FREETYPE

#include <GN.h>
#include "GN_FontFT.h"
#include <cstring>
#include <cstdio>

namespace GNg {

// ── 全局默认字体 ──────────────────────────────────────
cFREETYPE_FONT *FT_FontSys = nullptr;

// ── 系统默认中文字体搜索路径 ──────────────────────────
static const char *kSystemFontPaths[] = {
    // Linux
    "/usr/share/fonts/truetype/noto/NotoSansCJK-Regular.ttc",
    "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
    "/usr/share/fonts/truetype/droid/DroidSansFallbackFull.ttf",
    "/usr/share/fonts/truetype/wqy/wqy-zenhei.ttc",
    "/usr/share/fonts/truetype/wqy/wqy-microhei.ttc",
    "/usr/share/fonts/noto-cjk/NotoSansCJK-Regular.ttc",
    "/usr/share/fonts/google-noto-cjk/NotoSansCJK-Regular.ttc",
    "/usr/share/fonts/truetype/arphic/uming.ttc",
    "/usr/share/fonts/truetype/arphic/ukai.ttc",
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    // macOS
    "/System/Library/Fonts/PingFang.ttc",
    "/System/Library/Fonts/STHeiti Light.ttc",
    "/Library/Fonts/Arial Unicode.ttf",
    // Windows (MinGW 交叉编译时)
    "C:/Windows/Fonts/msyh.ttc",
    "C:/Windows/Fonts/simsun.ttc",
    nullptr
};

bool FT_LoadSystemFont(int fontSizePx) {
    if(FT_FontSys && FT_FontSys->GetAtlas() && FT_FontSys->GetAtlas()->vkImage) return true;
    if(!FT_FontSys) FT_FontSys = new cFREETYPE_FONT();
    for(int i = 0; kSystemFontPaths[i]; i++) {
        FILE *fp = fopen(kSystemFontPaths[i], "rb");
        if(fp) { fclose(fp); }
        else continue;
        if(FT_FontSys->Load(kSystemFontPaths[i], fontSizePx))
            return true;
    }
    return false;
}

// ═══════════════════════════════════════════════════════════
//  cFREETYPE_FONT 实现
// ═══════════════════════════════════════════════════════════

cFREETYPE_FONT::cFREETYPE_FONT()
    : m_ftLib(nullptr), m_ftFace(nullptr), m_atlas(nullptr)
    , m_atlasSize(0), m_fontSize(0)
    , m_cursorX(0), m_cursorY(0), m_rowH(0) {}

cFREETYPE_FONT::~cFREETYPE_FONT() { Release(); }

void cFREETYPE_FONT::Release() {
    if(m_ftFace) { FT_Done_Face(m_ftFace); m_ftFace = nullptr; }
    if(m_ftLib)  { FT_Done_FreeType(m_ftLib); m_ftLib = nullptr; }
    if(m_atlas)  { m_atlas->Release(); m_atlas = nullptr; }
    m_cache.clear();
    m_atlasSize = 0;
}

bool cFREETYPE_FONT::Load(const char *ttfPath, int fontSizePx) {
    Release();
    if(FT_Init_FreeType(&m_ftLib) != 0) return false;
    if(FT_New_Face(m_ftLib, ttfPath, 0, &m_ftFace) != 0) {
        FT_Done_FreeType(m_ftLib); m_ftLib = nullptr;
        return false;
    }
    FT_Set_Pixel_Sizes(m_ftFace, 0, fontSizePx);
    m_fontSize = fontSizePx;
    // 初始图集 1024×1024 (按需扩容)
    if(!InitAtlas(1024)) { Release(); return false; }
    return true;
}

bool cFREETYPE_FONT::InitAtlas(int size) {
    m_atlas = new sIMAGE();
    m_atlas->CreateEmptyToData(size, size, 1, 0, 1, false, true);
    if(!m_atlas->vkImage) { delete m_atlas; m_atlas = nullptr; return false; }
    m_atlas->texnum = 1;  // 单纹理图集 (非多帧)
    m_atlasSize = size;
    m_cursorX = 1; m_cursorY = 1; m_rowH = 0;
    return true;
}

bool cFREETYPE_FONT::GrowAtlas() {
    // 翻倍图集尺寸 (上限 8192)
    if(m_atlasSize >= 8192) return false;
    int newSize = m_atlasSize * 2;
    // 读取旧图集数据
    unsigned char *oldPx = m_atlas->DataGet();
    if(!oldPx) return false;
    // 创建新图集
    sIMAGE *newAtlas = new sIMAGE();
    newAtlas->CreateEmptyToData(newSize, newSize, 1, 0, 1, false, true);
    unsigned char *newPx = new unsigned char[newSize * newSize * 4];
    memset(newPx, 0, newSize * newSize * 4);
    // 拷贝旧数据到左上角
    for(int r = 0; r < m_atlasSize; r++)
        memcpy(newPx + r * newSize * 4, oldPx + r * m_atlasSize * 4, m_atlasSize * 4);
    newAtlas->DataFill(newPx, CR_RGBA, newSize, newSize, 0, 0, nullptr, false);
    delete[] newPx; delete[] oldPx;
    // 更新 UV (新图集尺寸变了，但 UV 是归一化的 0~1，自动适配)
    m_atlas->Release();
    *m_atlas = *newAtlas; // 注意：这里用了浅拷贝，Vulkan 资源需要手动处理
    // 实际上需要正确转移资源:
    m_atlas->vkImage = newAtlas->vkImage;
    m_atlas->vkImageMemory = newAtlas->vkImageMemory;
    m_atlas->vkImageView = newAtlas->vkImageView;
    m_atlas->vkSampler = newAtlas->vkSampler;
    m_atlas->vkDescriptorSet = newAtlas->vkDescriptorSet;
    m_atlas->width = newSize; m_atlas->height = newSize;
    m_atlas->uRange = 1.0f; m_atlas->vRange = 1.0f;
    m_atlasSize = newSize;
    // 重置打包游标
    m_cursorX = 1; m_cursorY = m_atlasSize/2; // 从中间开始（旧数据在上半）
    m_rowH = 0;
    // 清除缓存 (旧 UV 已失效)
    m_cache.clear();
    return true;
}

bool cFREETYPE_FONT::PackIntoAtlas(int w, int h, const unsigned char *bitmap,
                                    FT_GlyphEntry *out) {
    // 加 1 像素间距避免纹理采样溢出
    int pw = w + 1, ph = h + 1;
    // 检查当前行
    if(m_cursorX + pw > m_atlasSize) {
        m_cursorX = 1;
        m_cursorY += m_rowH + 1;
        m_rowH = 0;
    }
    // 检查是否超出底部
    if(m_cursorY + ph > m_atlasSize) {
        if(!GrowAtlas()) return false;
        // 扩容后重试
        return PackIntoAtlas(w, h, bitmap, out);
    }
    // 写入图集
    int atlasW = m_atlasSize;
    // 读取当前图集行数据，修改后写回（用 DataFill 子区域更新）
    unsigned char *row = new unsigned char[pw * ph * 4];
    for(int r = 0; r < h; r++) {
        for(int c = 0; c < w; c++) {
            int idx = (r * pw + c) * 4;
            unsigned char a = bitmap[r * w + c];
            row[idx+0] = 255; row[idx+1] = 255;
            row[idx+2] = 255; row[idx+3] = a;
        }
        // 间距列填透明
        int gapIdx = (r * pw + w) * 4;
        row[gapIdx+0]=255; row[gapIdx+1]=255; row[gapIdx+2]=255; row[gapIdx+3]=0;
    }
    // 间距行填透明
    memset(row + h * pw * 4, 0, pw * 4);
    m_atlas->DataFill(row, CR_RGBA, pw, ph, m_cursorX, m_cursorY, nullptr, false);
    delete[] row;
    // 计算 UV
    out->u0 = (float)m_cursorX / (float)m_atlasSize;
    out->v0 = (float)m_cursorY / (float)m_atlasSize;
    out->u1 = (float)(m_cursorX + w) / (float)m_atlasSize;
    out->v1 = (float)(m_cursorY + h) / (float)m_atlasSize;
    out->w = w; out->h = h;
    // 更新游标
    m_cursorX += pw;
    if(ph > m_rowH) m_rowH = ph;
    return true;
}

bool cFREETYPE_FONT::GetGlyph(uint32_t charCode, FT_GlyphEntry *out) {
    if(!m_ftFace || !out) return false;

    // 缓存命中
    auto it = m_cache.find(charCode);
    if(it != m_cache.end()) {
        *out = it->second;
        return true;
    }

    // 加载字形
    FT_UInt glyphIdx = FT_Get_Char_Index(m_ftFace, charCode);
    if(glyphIdx == 0) return false; // 字符不存在于字体中

    if(FT_Load_Glyph(m_ftFace, glyphIdx, FT_LOAD_DEFAULT) != 0)
        return false;

    // 光栅化
    if(FT_Render_Glyph(m_ftFace->glyph, FT_RENDER_MODE_NORMAL) != 0)
        return false;

    FT_GlyphSlot slot = m_ftFace->glyph;
    FT_Bitmap *bmp = &slot->bitmap;

    if(bmp->width == 0 || bmp->rows == 0) {
        // 空格等空白字符：只记录度量
        FT_GlyphEntry entry = {};
        entry.bearingX = (short)(slot->bitmap_left);
        entry.bearingY = (short)(slot->bitmap_top);
        entry.advance  = (short)(slot->advance.x >> 6);
        entry.w = 0; entry.h = 0;
        entry.u0 = entry.v0 = entry.u1 = entry.v1 = 0;
        m_cache[charCode] = entry;
        *out = entry;
        return true;
    }

    // 打包到图集
    FT_GlyphEntry entry;
    if(!PackIntoAtlas(bmp->width, bmp->rows, bmp->buffer, &entry)) {
        return false;
    }

    entry.bearingX = (short)(slot->bitmap_left);
    entry.bearingY = (short)(slot->bitmap_top);
    entry.advance  = (short)(slot->advance.x >> 6);

    m_cache[charCode] = entry;
    *out = entry;
    return true;
}

int cFREETYPE_FONT::GetAscender() const {
    if(!m_ftFace) return m_fontSize;
    return (int)(m_ftFace->size->metrics.ascender >> 6);
}

int cFREETYPE_FONT::GetDescender() const {
    if(!m_ftFace) return m_fontSize / 4;
    return (int)(-(m_ftFace->size->metrics.descender >> 6));
}

int cFREETYPE_FONT::GetLineHeight() const {
    if(!m_ftFace) return (int)(m_fontSize * 1.5f);
    return (int)(m_ftFace->size->metrics.height >> 6);
}

} // namespace GNg

// ═══════════════════════════════════════════════════════════
//  FreeType 文本排版 — 集成到 cTEXT_ART::SetText
// ═══════════════════════════════════════════════════════════

// 为 FreeType 字体计算 sDRAW_RANGE 排列
// 返回排好的字符数，-1 表示失败
int GNg::FT_ComputeArrangeData(
    sDRAW_RANGE **data_out, int &buf_count,
    char *text, unsigned short count,
    int x, int y, int w, int h,
    int text_ofs_x, int text_ofs_y,
    const sARRANGE_FMT *parm,
    unsigned short *row_hids_out, unsigned char *row_count_out,
    short *text_w_out, short *text_h_out)
{
    if(!FT_FontSys || !FT_FontSys->GetAtlas()) return -1;
    (void)row_hids_out;

    // ── 计算总字符数 ──────────────────────────────
    unsigned short t_count = count;
    if(!t_count) t_count = (unsigned short)strlen(text);
    if(t_count == 0) { *row_count_out = 0; return 0; }

    // ── 分配/扩展缓冲区 ──────────────────────────
    if(!*data_out || buf_count < (int)t_count + 64) {
        GN_DeleteArray(*data_out);
        buf_count = t_count + 256;
        *data_out = new sDRAW_RANGE[buf_count];
    }
    sDRAW_RANGE *rng = *data_out;
    if(!rng) return -1;

    // ── 布局参数 ──────────────────────────────────
    int penX = x + text_ofs_x;
    int penY = y + text_ofs_y;
    int lineHeight = FT_FontSys->GetLineHeight();
    int ascender   = FT_FontSys->GetAscender();
    int maxW = w ? w : 32767;
    int maxH = h ? h : 32767;
    bool erect = parm->erect_print;

    if(!lineHeight) lineHeight = (int)(FT_FontSys->GetFontSize() * 1.5f);
    if(!ascender) ascender = FT_FontSys->GetFontSize();

    unsigned short id = 0;
    unsigned char jnum = 0;
    int rowStartX = penX;
    int maxRowW = 0, totalH = lineHeight;

    // ── 逐字符排版 ────────────────────────────────
    for(unsigned short ti = 0; ti < t_count; ) {
        char c = text[ti];
        if(c == '\n') {
            penX = rowStartX;
            penY += lineHeight;
            totalH += lineHeight;
            jnum++;
            ti++;
            if(penY + lineHeight > y + maxH && maxH > 0) break;
            continue;
        }

        // ── UTF-8 解码为 Unicode 码点 ──────────────
        uint32_t cp;
        int adv = 1;
        unsigned char uc = (unsigned char)c;
        if(uc < 0x80) {
            cp = uc; adv = 1;
        } else if((uc & 0xE0) == 0xC0 && ti + 1 < t_count) {
            cp = ((uc & 0x1F) << 6) | ((unsigned char)text[ti+1] & 0x3F);
            adv = 2;
        } else if((uc & 0xF0) == 0xE0 && ti + 2 < t_count) {
            cp = ((uc & 0x0F) << 12) | (((unsigned char)text[ti+1] & 0x3F) << 6)
               | ((unsigned char)text[ti+2] & 0x3F);
            adv = 3;
        } else if((uc & 0xF8) == 0xF0 && ti + 3 < t_count) {
            cp = ((uc & 0x07) << 18) | (((unsigned char)text[ti+1] & 0x3F) << 12)
               | (((unsigned char)text[ti+2] & 0x3F) << 6) | ((unsigned char)text[ti+3] & 0x3F);
            adv = 4;
        } else {
            ti++; continue; // 无效 UTF-8，跳过
        }

        FT_GlyphEntry glyph;
        if(!FT_FontSys->GetGlyph(cp, &glyph)) {
            ti += adv; // 跳过该字符
            continue;
        }

        int gW = glyph.w ? glyph.w : glyph.advance;
        int gH = glyph.h ? glyph.h : lineHeight;

        // 换行检查
        if(penX + glyph.advance > rowStartX + maxW && maxW > 0 && penX != rowStartX) {
            penX = rowStartX;
            penY += lineHeight;
            totalH += lineHeight;
            jnum++;
            if(penY + lineHeight > y + maxH && maxH > 0) break;
        }

        if(glyph.w > 0 || glyph.h > 0) {
            // 计算绘制位置（考虑 bearing）
            int dx = penX + glyph.bearingX;
            int dy = penY + ascender - glyph.bearingY;
            if(erect) {
                // 竖排：交换 x/y
            }

            rng[id].x   = (float)dx;
            rng[id].y   = (float)dy;
            rng[id].w   = (float)gW;
            rng[id].h   = (float)gH;
            rng[id].u1  = glyph.u0;
            rng[id].v1  = glyph.v0;
            rng[id].u2  = glyph.u1;
            rng[id].v2  = glyph.v1;
            rng[id].tid = 0;
            id++;
        }

        penX += glyph.advance;
        if(penX - rowStartX > maxRowW) maxRowW = penX - rowStartX;
        ti += adv;
    }

    *row_count_out = jnum + 1;
    if(text_w_out)  *text_w_out  = (short)maxRowW;
    if(text_h_out)  *text_h_out  = (short)totalH;

    return (int)id;
}

#endif // GN_HAS_FREETYPE
