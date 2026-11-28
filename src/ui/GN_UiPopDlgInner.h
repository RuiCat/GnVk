// GN_UiPopDlgInner.h — GN_UiPopDlg*.cpp 共享的内部辅助声明与宏（非公开 API）
#ifndef _GN_UIPOPDLG_INNER_H_
#define _GN_UIPOPDLG_INNER_H_

#include "GN.h"
#include "GN_UiPopDlg.h"

// 简单对话筐内容类型（原 GN_UiPopDlg.cpp:109-117；拆分后多个 TU 共用）
enum GT_SP_OPTION_TYPE { //简单对话筐内容类型
    GT_SP_VALUE_TEXT, //文本变量
    GT_SP_VALUE_COLOR, //颜色变量
    GT_SP_VALUE_INT, //整形变量
    GT_SP_VALUE_FLOAT, //浮点变量

    GT_SP_MENU, //选单
    GT_SP_TEXT //文本提示
};

// DA-M2 批 4：原两个 ad-hoc extern 全局收敛进上下文，跨 TU 改用访问器（定义在 GN_UiPopDlgMenu.cpp）。
struct sPopDlgContext {
    float aSimMenuTextOffsetPos = 0.0f; //临时记录文本读写时的偏移记数
    GNu::cSIM_DIALOGEX *aFullSelectPathDlg = 0;
};
sPopDlgContext &GN_UiPopDlgContext();

// 文件级辅助函数（原 static；拆分后 Menu / Run 两个 TU 共用，保持内部链接）
//设置文本变量显示的字符串(变量项)
static inline void GT_ChildTextValueSetShowStr(GT_sSP_CHILD *_chd) {
    GN_DeleteArray(_chd->enumIds);
    char *tbuf = (char *)(_chd->pointer);
    int tlen = strlen(tbuf), vlen = (int)_chd->minValue;
    if(tlen > vlen) {
        _chd->enumIds = new short[vlen + 1];
        char *sbuf = (char *)_chd->enumIds, *_name = _chd->name;
        sbuf[vlen] = 0;
        if((_name[0] == '+' || _name[0] == '=' || _name[0] == '>') && (int)_chd->maxValue == GN_MAXFILEPATH) {
            --vlen;
            --tlen;
            sbuf[0] = '.';
            sbuf[1] = '.';
            sbuf[2] = '.'; //在前列显示...
            while(vlen > 2) {
                if(tbuf[tlen] & 128) {
                    sbuf[vlen] = tbuf[tlen];
                    --vlen;
                    --tlen;
                    sbuf[vlen] = tbuf[tlen];
                    --vlen;
                    --tlen;
                } else {
                    sbuf[vlen] = tbuf[tlen];
                    --vlen;
                    --tlen;
                }
            }
        } else {
            int vi2 = 0; // B9-3: 避免阴影外层 vi2
            vlen -= 3;
            sbuf[vlen] = '.';
            sbuf[vlen + 1] = '.';
            sbuf[vlen + 2] = '.'; //在结尾显示...
            if(_name[0] == '*') {
                while(vi2 < vlen) {
                    if(tbuf[vi2] & 128) {
                        sbuf[vi2] = '*';
                        ++vi2;
                        sbuf[vi2] = '*';
                        ++vi2;
                    } else {
                        sbuf[vi2] = '*';
                        ++vi2;
                    }
                }
            } else {
                while(vi2 < vlen) { //这段类似上一段
                    if(tbuf[vi2] & 128) {
                        sbuf[vi2] = tbuf[vi2];
                        ++vi2;
                        sbuf[vi2] = tbuf[vi2];
                        ++vi2;
                    } else {
                        sbuf[vi2] = tbuf[vi2];
                        ++vi2;
                    }
                }
            }
        }
    } else {
        _chd->enumIds = new short[tlen + 1];
        if(_chd->name[0] == '*') {
            char *sbuf = (char *)_chd->enumIds;
            memset(sbuf, '*', tlen);
            sbuf[tlen] = 0;
            // B5f：enumIds 容量 = (tlen+1)*2 字节，按上限截断。
        } else GN_StrCopyToLimit((char *)_chd->enumIds, (tlen + 1) * 2, tbuf);
    }
}

// 跨 TU 使用的宏（自原文件逐字搬入，保证每个使用它的 TU 都可见）
#ifdef GN_DEBUG
#define GT_TMP_sSIM_MENU_HasInitValue(PTR)                                       \
    if(!name || !name[0]) {                                                      \
        GN_Log("调用sSIM_MENU::InitValueToData 变量名没有指定好!\n");            \
        return;                                                                  \
    }                                                                            \
    if(!PTR) {                                                                   \
        GN_Log("调用sSIM_MENU::InitValueToData %s 变量指针没有指定好!\n", name); \
        return;                                                                  \
    }
#else
#define GT_TMP_sSIM_MENU_HasInitValue(PTR) \
    if(!name || !name[0] || !PTR) return;
#endif

#define GT_SIMPLE_MENU_SearchMinEnumValue(VAR, VMIN) \
    if(VAR) {                                        \
        int mki = 1, inv = 1, value1 = (int)VAR;     \
        for(; mki <= 32; ++mki) {                    \
            if(value1 & inv) {                       \
                VMIN = (float)mki;                   \
                break;                               \
            } /*最小定为值有设置的最小一个列举项*/   \
            inv <<= 1;                               \
        }                                            \
    } else VMIN = 0.0f

#define GT_TMP_MAKE_SIM_ARROW_ICON                                          \
    int w2 = 1, tw = (int)zTwidth;                                          \
    while(w2 < tw) { w2 <<= 1; }                                            \
    if(w2 - tw > tw - (w2 >> 1)) w2 >>= 1; /*获得一个最接近字宽的2N次方数*/ \
    MakeSimpleArrowIcon(zArrowMap, w2, (int)((float)zTheight * 0.7f), 0, 1);

#define GT_TMP_SIM_DIALOG_DRAW_COMPARTENT_CPT    \
    GNg::sARRANGE_FMT &tfmt = zTextOut.zParame;  \
    short ti = tfmt.i_compartment,               \
          tj_ = tfmt.j_compartment;              \
    short ti2 = ti << 1,                         \
          tj = (tj_ < zIconH) ? zIconH : tj_,    \
          tj2 = (tj_ < zIconH2) ? zIconH2 : tj_; \
    short iw = zIconW ? zIconW + ti : 0;

#define GT_TMP_CHILD_DRAW_POSX_CPT                                     \
    short icx = dx; /*变量图标显示X*/                                  \
    short vnx = icx + iw; /*变量名显示X*/                              \
    short va1x = vnx + zNameW + (ti << 1) + (ti >> 1); /*左箭头显示X*/ \
    short vvx = va1x + (ti >> 1); /*变量值显示X*/                      \
    short va2x = vvx + zValueW + (ti >> 1); /*右箭头显示X*/

#define GT_DLG_BNAME_SIZE (2 * 3 * 9)

//按宽高来构建矩形(矩形位置X , 位置Y, 矩形宽, 矩形高)
#define GNg_RectMakeFromWH(RC, X, Y, W, H) \
    {                                      \
        RC.left = X;                       \
        RC.top = Y;                        \
        RC.right = W;                      \
        RC.bottom = H;                     \
    }

#define GT_TMP_SIM_DIALOG_EffectTransTimeSet() \
    if(zEffect->zNodes) {                      \
        zEffect->SeekSegment(0);               \
        zNowtime = zEffect->zEndtime;          \
    } else zNowtime = zEffect->zEndtime > 1200 ? 1200 : zEffect->zEndtime

// 非 static 辅助函数的前置声明（定义在 GN_UiPopDlgSetup.cpp）
void GT_DlgSetToolInfoStateReset(GT_sVALUE_SET_TOOL *_set_tool, const GNg::cFRAME_RC *_ef, unsigned int ca);
void GT_DlgSetToolInfoStateUpdate(GT_sVALUE_SET_TOOL *_set_tool, const GNg::cFRAME_RC *_ef, unsigned int ca, unsigned int t);

#endif // _GN_UIPOPDLG_INNER_H_
