#ifndef _GN_FONT_FT_H_
#define _GN_FONT_FT_H_
// ╔══════════════════════════════════════════════════════════╗
// ║  GN_mini — FreeType 实时字形渲染                       ║
// ║  替代预生成 PNG 字库，支持任意 TTF/OTF + 完整 Unicode  ║
// ╚══════════════════════════════════════════════════════════╝
#ifdef GN_HAS_FREETYPE

#include <ft2build.h>
#include FT_FREETYPE_H
#include <unordered_map>

namespace GNg {

// 单个字形在图集中的缓存条目
struct FT_GlyphEntry {
    float u0, v0, u1, v1;   // 在图集中的 UV 坐标
    short w, h;              // 字形像素尺寸
    short bearingX, bearingY;// 方位偏移 (相对基线)
    short advance;            // 水平步进宽度
};

// ═══════════════════════════════════════════════════════════
//  FreeType 字形缓存 — 动态图集 + 按需光栅化
// ═══════════════════════════════════════════════════════════
class cFREETYPE_FONT {
public:
    cFREETYPE_FONT();
    ~cFREETYPE_FONT();

    // 加载 TTF/OTF 字体并指定渲染字号
    bool Load(const char *ttfPath, int fontSizePx);

    // 获取指定字符的字形信息 (自动光栅化+缓存)
    bool GetGlyph(uint32_t charCode, FT_GlyphEntry *out);

    // 获取字形图集纹理 (供 ImageDrawMP 使用)
    struct sIMAGE *GetAtlas() { return m_atlas; }

    // 获取字体度量
    int  GetAscender()  const;
    int  GetDescender() const;
    int  GetLineHeight() const;
    int  GetFontSize()  const { return m_fontSize; }

    void Release();

private:
    FT_Library    m_ftLib;
    FT_Face       m_ftFace;
    sIMAGE       *m_atlas;
    int           m_atlasSize;
    int           m_fontSize;
    int           m_cursorX, m_cursorY, m_rowH;

    std::unordered_map<uint32_t, FT_GlyphEntry> m_cache;

    bool InitAtlas(int size);
    bool PackIntoAtlas(int w, int h, const unsigned char *bitmap,
                       FT_GlyphEntry *out);
    bool GrowAtlas();
};

// 全局默认字体实例
extern cFREETYPE_FONT *FT_FontSys;

// 便捷函数：加载系统默认中文字体
bool FT_LoadSystemFont(int fontSizePx = 24);

// 集成到 cTEXT_ART 的排版函数
struct sDRAW_RANGE;
struct sARRANGE_FMT;
int FT_ComputeArrangeData(
    sDRAW_RANGE **data_out, int &buf_count,
    char *text, unsigned short count,
    int x, int y, int w, int h,
    int text_ofs_x, int text_ofs_y,
    const sARRANGE_FMT *parm,
    unsigned short *row_hids_out, unsigned char *row_count_out,
    short *text_w_out, short *text_h_out);

} // namespace GNg

#endif // GN_HAS_FREETYPE
#endif // _GN_FONT_FT_H_
