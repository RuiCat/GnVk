#pragma once
#ifndef _GN_FONT_H_
#define _GN_FONT_H_

namespace GNg {
struct sTEXT_SET : public sDRAW_AREA //文本输出描述
{
    char *text; //文本内容
    unsigned short count; //文本输出个数 可以为0 这样就输出整个文本
    unsigned short blendmode; //混合模式
    int offsetx; //在筐内的偏移X量
    int offsety; //在筐内的偏移Y量

    inline sTEXT_SET() : text(0), count(0) {}
    inline ~sTEXT_SET() {
        GN_DeleteArray(text);
        count = 0;
    }
};
struct GN_API sDRAW_ARRANGE //绘制排列器
{
    sARRANGE_FMT zParame; //排列格式
    sDRAW_RANGE *zDrawRngData; //记录排好绘制单元范围
    unsigned short zRowSIDs[256]; //记录每行的开头是第几个可视绘制单元 最大可记录256行
    uint8 zRowNum; //记录排好的可视行数
    uint8 zFontMapFormat; //字体图格式
    short zTotRngW; //记录排好的内容实际所占宽度
    short zTotRngH; //记录排好的内容实际所占高度
    float zTu_w; //每格截取纹理的U范围
    float zTv_h; //每格截取纹理的V范围

    void zClassifyFontMapFormat(sIMAGE *fontmap); //归类字体图格式并预计算一些内部数据以加速处理(字体图像)
    void zArrangeFmtEnsure(); //确保排列格式各参数正确性
    uint8 *zDataSave(uint8 *wpos); //保存该对象的2进制数据(当前写入位置)返回最后的写完位置
    uint8 *zDataLoad(uint8 *rpos); //读取该对象的2进制数据(当前读取位置)返回最后的读完位置

    sDRAW_ARRANGE();
    ~sDRAW_ARRANGE() { this->Release(); }

    //释放内部资源
    void Release() {
        GN_DeleteArray(zDrawRngData);
        zRowNum = 0;
    }

    //设置排列格式(格式)
    //	void SetArrangeFormat(GNg::sARRANGE_FMT *_fmt){ zParame = *_fmt; }

    //获得排列格式()返回格式
    inline sARRANGE_FMT *const GetArrangeFormat() { return &zParame; }

    inline short GetFontRangeX() { return zParame.i_compartment; } //获得文本字体每只字输出向X方向的跨距()
    inline short GetFontRangeY() { return zParame.j_compartment; } //获得文本字体每只字输出向Y方向的跨距()
    inline short GetFontWidth() { return zParame.width; } //获得文本的字体宽度()
    inline short GetFontHeight() { return zParame.height; } //获得文本的字体高度()

    //获得上次排好的第几个可视绘制单元或字符的排位信息（第几个可视绘制单元或字符, 排位信息输出)返回指定字符是否有正确排位  这函数一般要跟在 SetText 或 OutText 或已经排好内容后调用
    inline bool GetPosInfoFromVID(unsigned short vid, GNg::sDRAW_RANGE *rng_out) {
        if(zDrawRngData) {
            *rng_out = zDrawRngData[vid];
            return true;
        } else return false;
    }

    //获得用特定的文本筐来显示指定文本时该内容在筐内的总偏移量(要显示的文本, 输出个数 值0表示输出全部, 文本筐宽, 文本筐高) 返回总偏移量  !!只适用于有筐内详细显示内容的情况
    int GetInRectOffsetTotalH(char *text, unsigned short count, int w, int h);

    //获取确定排好的字符准确范围(范围输出, 排好的字符数, 单个字符内的局部水平偏移, 单个字符内的局部垂直偏移)
    void GetTextRange(sRECT *_range_out, int count, float ofs_u = 0.0f, float ofs_v = 0.0f);

    //获得文本指定可见字符个数的字符串(装填字符串的缓冲 需分配好足够空间 如果获取中文可能需要个数乘2 + 1的空间, 获取字符个数 不能大于实际文本显示个数)
    void GetText(char *text_out, int count);
};
/********************************************************* FONT_MAP **********************************************************************/

enum eFONT_MAP_FORMAT { //字体图格式
    eFONT_NUMBER = 0, //只有数字的格式
    eFONT_ASCII = 1, //ASCII码字符格式
    eFONT_FREETYPE = 2 //FreeType 实时渲染 (UTF-8, 替代旧 GB2312)
};
enum eFONT_MAPLACK_SHOW_MODE { //字体式样图缺失时的显示处理方式
    eFONT_MAPLACK_SHOW_NULL, //没有特殊处理 就让它运行出错
    eFONT_MAPLACK_SHOW_CUBE, //用方格代替每个字
    eFONT_MAPLACK_SHOW_DEFAULT //用系统默认字体代替显示
};
GN_API eFONT_MAP_FORMAT QueryFontMapFormat(sIMAGE *fontmap); //查询字体图格式(字体图)返回格式

/*********************************************************** cTEXT_ART ******************************************************************/

class GN_API cTEXT_ART : public sDRAW_ARRANGE //★★★艺术文字输出器 !!在WINCE平台下使用文字颜色只能由字库的颜色确定 不能动态调整颜色
{
    //GT_sDRAW_ARRANGE::SetArrangeFormat;
  protected:
    sIMAGE *zpFontMap; //使用的字体图像
    int zFx, zFy, zFw, zFh; //文本筐范围X Y W H
    int zTextCount; //记录排好的格数
    int zDrawRngCount; //绘制单元范围缓冲可容纳个数
    unsigned int zDrawRngNST; //排好数与缓冲可容纳数接近时的最近时间
  public:
    uint8 *zDataSave(uint8 *wpos, GN::sLIST<GNg::sIMAGE *> &img_collect); //保存该对象的2进制数据(当前写入位置, 收集到的字图式样列表)返回最后的写完位置
    uint8 *zDataLoad(uint8 *rpos, P<GNg::sIMAGE *> &img_refs); //读取该对象的2进制数据(当前读取位置, 字图式样引用组)返回最后的读完位置

  public: //★用户接口 -----------------------------------------------------------------------------------------------------------
    //!!!!快速输出提示:尽量使用 Number 类型和无筐的 这样可以少很多判断 性能更好
    //!!!!使用技巧提示:定以一个对象可以作很多地方的文本输出 这样比较适合在输出地方多 但文本字数较少的情况 这样显示比较方便
    //                 当遇到显示字数很多的文本或需要交互检测文字的位置信息等 适合一个输出对象对应一个文本 这样显示比较高效
    //构造
    cTEXT_ART();
    ~cTEXT_ART() { this->Release(); }

    //释放内部资源
    void Release();

    //初始化(   字体图,    排列格式   , 字体式样图缺失时的显示处理方式)
    void Init(sIMAGE *font_map, sARRANGE_FMT *format_parame = &ArrangeFmt, eFONT_MAPLACK_SHOW_MODE lack_type = eFONT_MAPLACK_SHOW_CUBE);

    //显示输出斋数字   (要显示的数字,        坐标X,   坐标Y,    高度,    宽度,         文字颜色,                           混合模式)   !输出斋数字不记录交互反馈信息以提高速度
    void TextOutNumber(unsigned int number, int x = 0, int y = 0, int w = 0, int h = 0, unsigned int color = 0xffffffff, unsigned short blendmode = 1);

    //显示输出文本(要显示的文本, 文本输出个数 值0表示输出全部, 坐标X, 坐标Y, 文本筐宽, 文本筐高,    文字颜色,                   混合模式,       文本在筐内的偏移X量, 文本在筐内的偏移Y量)返回显示了的字数
    int OutText(char *text, unsigned short count = 0, int x = 0, int y = 0, int w = 0, int h = 0, unsigned int color = 0xffffffff, unsigned short blendmode = 1, int offset_x = 0, int offset_y = 0);
    //显示输出文本(文本输出描述)返回显示了的字数
    inline int OutText(sTEXT_SET *_desc) { return OutText(_desc->text, _desc->count, _desc->x, _desc->y, _desc->w, _desc->h, _desc->c, _desc->blendmode, _desc->offsetx, _desc->offsety); }

    //设置输出文本(要显示的文本, 文本输出个数 值0表示输出全部, 坐标X, 坐标Y, 文本筐宽, 文本筐高, 文本在筐内的偏移X量, 文本在筐内的偏移Y量)返回排好了的字数
    int SetText(char *text, unsigned short count = 0, int x = 0, int y = 0, int w = 0, int h = 0, int offset_x = 0, int offset_y = 0);
    //围绕一个指定筐输出文本(是否围绕在筐内, 靠筐那边, 要显示的文本, 文本输出个数 值0表示输出全部, 围绕筐X坐标, 围绕筐Y坐标, 围绕筐宽, 围绕筐高)返回排好了的字数 !调用这个函数 文字排列格式的自动填满不能为有
    int SetText(bool in_rect, GNu::eNEAR_TYPE near_type, char *text, unsigned short count, int x, int y, int w, int h);

    //显示已设置好的文本(文字颜色,   混合模式) !用SetText与Show两个组合灵活运用运行效率会比OutText高 因为同一个输出对象显示本文内容或显示位置等产生变化才有需要调用SetText平时更新重绘画面时仅调用Show即可
    void Show(unsigned int color = 0xffffffff, unsigned short blendmode = 1);

    //从文本内查找获得在屏幕上显示的字符ID(指定字节位置, 返回选到的字符ID, 返回对应找到字符所在的行号）返回指定字符有没显示在屏幕上  这函数一般要跟在SetText或OutText后调用
    bool GetVIDfromText(unsigned short buf_id, unsigned short *str_id_out, uint8 *row_id_out);

    //从点坐标计算获得在屏幕上显示的字符ID(选择点坐标X, 选择点坐标Y, 返回选到的字符ID, 返回对应找到字符所在的行号, 返回字符位置信息) 返回是否成功选中字符  这函数一般要跟在SetText或OutText后调用
    bool GetVIDfromCoord(int px, int py, unsigned short *str_id_out, uint8 *row_id_out, sDRAW_RANGE *info_out);

    //获得文本显示范围(范围左上角坐标X输出, 坐标Y输出, 范围宽度输出, 范围高度输出) !!调用这个前必须先调用过 OutText 或 SetText 设定过字体排位后才能有正确的结果输出
    void GetTextShowRange(short *x, short *y, short *w, short *h);

    //获得最近一次设置文本筐的范围(范围左上角坐标X输出, 坐标Y输出,范围宽度输出, 范围高度输出) !!调用这个前必须先调用过 OutText 或 SetText 设定过文本筐后后才能有正确的结果输出
    inline void GetTextFrameRange(int *x, int *y, int *w, int *h) { *x = zFx, *y = zFy, *w = zFw, *h = zFh; }

    inline int GetVisualStrCount() { return zTextCount; } //获得文本的可视文字数()
    inline sIMAGE *GetFontMap() { return zpFontMap; } //获得该对象正在使用的字体图()
    inline bool IsInited() { return zpFontMap != &Image; } //返回该对象是否已经初始化好()

    void CopyData(cTEXT_ART &src); //复制数据(复制源) !调用后复制源已排好的数据会被清除
#ifdef GN_HAS_FREETYPE
    bool SetFreeTypeFont(int fontSizePx = 24); //启用 FreeType 系统字体渲染
#endif
};
/*****************************************************************************************************************************************/

struct GT_sTEXT_LABLE_STR //标签文本
{
    P<char> lableStr; //标签内容
    GNg::sCOLOR color; //标签颜色
    bool absolute; //是否用绝对相同的识别方式

    unsigned int (*onCpProc)(const char *str, unsigned short start_vid, unsigned short end_vid); //指向当点击到标签时的处理

    GT_sTEXT_LABLE_STR() : onCpProc(0) {}
    GT_sTEXT_LABLE_STR(const char *str, bool _absolute, GNg::sCOLOR c, unsigned int (*on_cp_proc)(const char *str, unsigned short start_vid, unsigned short end_vid)) : lableStr(str, 0), absolute(_absolute), color(c), onCpProc(on_cp_proc) {}
};
struct GT_sTEXT_LABLE_RANGE : public GT_sSEGMENT_S //标签文本段
{
    GT_sTEXT_LABLE_STR *pLable; //指向标签文本
    P<char> extStr; //保存相对名的扩展字符用
    unsigned short startVid; //标签段首个字符可视ID
    unsigned short endVid; //标签段尾个字符可视ID
    unsigned char startRow; //标签段的首落在可视字符的行号

    unsigned int CallLableProc(); //执行标签的处理

    GT_sTEXT_LABLE_RANGE() : GT_sSEGMENT_S(0, 0), pLable(0) {}
    GT_sTEXT_LABLE_RANGE(GT_sTEXT_LABLE_STR *_pLable, unsigned short _start, unsigned short _end) : GT_sSEGMENT_S(_start, _end), pLable(_pLable) {}
};
class GN_API cTEXT_ARTEX : public cTEXT_ART //★艺术文字增强输出器 比基类增多了对标签文本的处理
{
    GN::sLIST<GT_sTEXT_LABLE_RANGE> zLableRanges;
    GN::sLIST<GT_sTEXT_LABLE_STR> zLables;

  public: //★用户接口 -----------------------------------------------------------------------------------------------------------
    //清除标签
    inline void LableClear() {
        zLableRanges.clear();
        zLables.clear();
    }

    //标签设置(文本内被视为标签的字符串, 是否绝对相同的字符才被视为标签, 标签颜色, 当点中标签时执行的处理(点中标签的完整字符串, 要求返回一个颜色用来做点到标签时的颜色 点中标签的第一个可见字符是当前显示中的第几个可视字符, 最末一个字符是第几个可视字符)) 可多次调用来设置同一个文本内显示多个标签
    void LableSet(const char *lable_str, bool _absolute, sCOLOR c, unsigned int (*on_cp_proc)(const char *str, unsigned short start_vid, unsigned short end_vid));
    //计算生成标签段(生成标签的文本) !预先生成标签段以提高处理速度 通常在更改了显示文本 或标签发生了变化时要调用这个来重新生成对应的标签段  该函数必需比 SetText 先调用
    void LableSegmentGen(const char *text);

    //设置输出文本     (要显示的文本, 文本输出个数 值0表示输出全部, 坐标X, 坐标Y, 文本筐宽, 文本筐高, 文本在筐内的偏移X量, 文本在筐内的偏移Y量)返回排好了的字数 !!这个性能开销比较大 如显示文本内容或显示位置没有变化 尽量不要每帧都调用
    int SetText(char *text, unsigned short count = 0, int x = 0, int y = 0, int w = 0, int h = 0, int offset_x = 0, int offset_y = 0);

    //显示已设置好的文本(文字颜色,   混合模式)
    void Show(unsigned int color = 0xffffffff, unsigned short blendmode = 1);

    /*这里的函数调用的先后顺序一般为:
		LableSet(...);
		LableSegmentGen(text);
		SetText(text, ...);
		Show(...);
     
	    LableSet , LableSegmentGen , SetText 一般不要每帧调用 第一次打开时当然要3个都调用
		之后当只有文本位置或筐内偏移改变了仅调用SetText即可
	    当显示文本发生改变时必需再调用一次LableSegmentGen和SetText
		如果更改或设置多了标签条也必须重调用一次LableSegmentGen和SetText
    */
};
}; // namespace GNg

#endif //endif _GN_FONT_H_
