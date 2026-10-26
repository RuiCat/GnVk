/****************************************************************************
Copyright (c) 2018-2025 gncreate Technologies Inc.

 http://www.gncreate.com
 http://www.img-power.com

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 ****************************************************************************/

#include "GN.h"
#include "GN_Def.h"
#ifdef _GN_UIPOPDLG_H_
void GT_sSP_CHILD::Release() {
    icon.Delete();
    GN_DeleteArray(name);
    GN_DeleteArray(enumName);
    GN_DeleteArray(enumIds);
}
//设置文本变量显示的字符串(变量项)
static void GT_ChildTextValueSetShowStr(GT_sSP_CHILD *_chd) {
    GN_DeleteArray(_chd->enumIds);
    char *tbuf = (char *)(_chd->pointer);
    int tlen = strlen(tbuf), vlen = (int)_chd->minValue, vi = 0;
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
            int vi = 0;
            vlen -= 3;
            sbuf[vlen] = '.';
            sbuf[vlen + 1] = '.';
            sbuf[vlen + 2] = '.'; //在结尾显示...
            if(_name[0] == '*') {
                while(vi < vlen) {
                    if(tbuf[vi] & 128) {
                        sbuf[vi] = '*';
                        ++vi;
                        sbuf[vi] = '*';
                        ++vi;
                    } else {
                        sbuf[vi] = '*';
                        ++vi;
                    }
                }
            } else {
                while(vi < vlen) { //这段类似上一段
                    if(tbuf[vi] & 128) {
                        sbuf[vi] = tbuf[vi];
                        ++vi;
                        sbuf[vi] = tbuf[vi];
                        ++vi;
                    } else {
                        sbuf[vi] = tbuf[vi];
                        ++vi;
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
            // TODO: verify buffer size
        } else strcpy((char *)_chd->enumIds, tbuf);
    }
}
/************************************************************* 简单对话筐 ***********************************************************************/
extern unsigned char GT_ImeCount;

enum GT_SP_OPTION_TYPE { //简单对话筐内容类型
    GT_SP_VALUE_TEXT, //文本变量
    GT_SP_VALUE_COLOR, //颜色变量
    GT_SP_VALUE_INT, //整形变量
    GT_SP_VALUE_FLOAT, //浮点变量

    GT_SP_MENU, //选单
    GT_SP_TEXT //文本提示
};
void GNu::sSIM_MENU::RemoveValue() {
#define GT_TMP_SIM_MENU_CLEAR_CHILD                \
    if(zChild.size()) {                            \
        short ci = 0, ce = zChild.size();          \
        for(; ci < ce; ++ci) zChild[ci].Release(); \
        zChild.clear();                            \
    }
    GT_TMP_SIM_MENU_CLEAR_CHILD;
}
void GNu::sSIM_MENU::RemoveValueAll() {
#define GT_TMP_SIM_MENU_REMOVE_ALL(re_func)                                                   \
    if(zChild.size()) {                                                                       \
        int ci = 0, ce = zChild.size();                                                       \
        for(; ci < ce; ++ci) {                                                                \
            if(zChild[ci].type == GT_SP_MENU) ((sSIM_MENU *)(zChild[ci].pointer))->re_func(); \
            zChild[ci].Release();                                                             \
        }                                                                                     \
        zChild.clear();                                                                       \
    }
    GT_TMP_SIM_MENU_REMOVE_ALL(RemoveValueAll);
}

GNu::sSIM_MENU::~sSIM_MENU() {
    this->Release();
}

void GNu::sSIM_MENU::Release() {
    GT_TMP_SIM_MENU_CLEAR_CHILD;
    GN_DeleteArray(zCaption);
    zIcon.Delete();
}
void GNu::sSIM_MENU::ReleaseAll() {
    GT_TMP_SIM_MENU_REMOVE_ALL(ReleaseAll);
    GN_DeleteArray(zCaption);
    zIcon.Delete();
}
void GNu::sSIM_MENU::InitValueToData(char *text, const char *name, int text_buf_size, int text_max_show_count, P<GNg::sIMAGE> icon) {
#define GT_TMP_sSIM_MENU_CopyName /*复制保存名称*/ \
    int len = strlen(name);                        \
    chd.name = new char[len + 1];                  \
    /* TODO: verify buffer size */                 \
    strcpy(chd.name, name);

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

#define GT_TMP_sSIM_MENU_OutRangeWarring \
    if(out_range) GN_Log("调用sSIM_MENU::InitValueToData %s : 值超出限定范围!\n", name);

    GT_TMP_sSIM_MENU_HasInitValue(text) if(!text_buf_size) {
        GN_Log("调用 sSIM_MENU::InitValueToData 文本缓冲没有指定好\n");
        return;
    }
#else
#define GT_TMP_sSIM_MENU_HasInitValue(PTR) \
    if(!name || !name[0] || !PTR) return;

#define GT_TMP_sSIM_MENU_OutRangeWarring

    if(!text_buf_size) return;
#endif
    text[text_buf_size - 1] = 0;
    zChild.push_back(GT_sSP_CHILD());
    GT_sSP_CHILD &chd = zChild.back();
    chd.type = GT_SP_VALUE_TEXT;
    chd.pointer = text;
    chd.maxValue = (float)text_buf_size;
    chd.minValue = (float)text_max_show_count;
    chd.icon = icon;
    GT_TMP_sSIM_MENU_CopyName

        GT_ChildTextValueSetShowStr(&chd);
    chd.enumName = new char[text_buf_size];
    // TODO: verify buffer size
    strcpy(chd.enumName, text); //原初始值保存到列举名里
}
void GNu::sSIM_MENU::InitValueToData(unsigned int *value, const char *name, int max_value, int min_value, int add_value, P<GNg::sIMAGE> icon) {
#define GT_TMP_sSIM_MENU_InitValue(TYPE) \
    zChild.push_back(GT_sSP_CHILD());    \
    GT_sSP_CHILD &chd = zChild.back();   \
    chd.type = TYPE;                     \
    chd.pointer = value;                 \
    chd.maxValue = (float)max_value;     \
    chd.minValue = (float)min_value;     \
    chd.addValue = (float)add_value;     \
    chd.icon = icon;                     \
    memcpy(&(chd.srcValue), value, 4);   \
    GT_TMP_sSIM_MENU_CopyName

    GT_TMP_sSIM_MENU_HasInitValue(value) bool out_range = false;
    int ca = (int)(((*value) & 0xff000000) >> 24);
    if(ca < min_value) {
        ca = min_value;
        *value = ((*value) & 0x00ffffff) | (ca << 24);
        out_range = true;
    } else if(ca > max_value) {
        ca = max_value;
        *value = ((*value) & 0x00ffffff) | (ca << 24);
        out_range = true;
    }
    GT_TMP_sSIM_MENU_OutRangeWarring
    GT_TMP_sSIM_MENU_InitValue(GT_SP_VALUE_COLOR)
}
void GNu::sSIM_MENU::InitValueToData(int *value, const char *name, int max_value, int min_value, int add_value, const char *enum_name, P<GNg::sIMAGE> icon) {
    GT_TMP_sSIM_MENU_HasInitValue(value) bool out_range = false;
    GT_TMP_sSIM_MENU_InitValue(GT_SP_VALUE_INT) if(enum_name && enum_name[0]) {
        char *pn = (char *)enum_name;
        uint8 li = 0;
        short i = 0, ei = 0, tmpn = 0;
        short tids[256];
        while(*pn) {
            if(*pn == '|') { //列举名里有分割符
                if(tmpn > ei) ei = tmpn;
                tids[li++] = i; //记下本次 | 字符在列举名的字符ID
                tmpn = 0;
            } else ++tmpn;
            ++i;
            ++pn;
        }
        if(li) { //有分割符
            if(add_value) { //单选自动适应有列举名的范围值
                chd.addValue = 1.0f;
                chd.maxValue = chd.minValue + (float)(li);
                if(*value > (int)chd.maxValue) {
                    *value = (int)chd.maxValue;
                GT_GOTO_LogSimMenuInitValueFaile:
                    memcpy(&chd.srcValue, value, 4);
#ifdef GN_DEBUG
                    GN_Log("调用sSIM_MENU::InitValueToData %s :值范围超出限定列举数!\n", name);
#endif
                } else if(*value < (int)chd.minValue) {
                    *value = (int)chd.minValue;
                    goto GT_GOTO_LogSimMenuInitValueFaile;
                }
            } else {
                if(li > 31) {
                    li = 31; //多选模式下不能超过32个列举
                    GN_Log("调用sSIM_MENU::InitValueToData %s 多选模式下不能超过32个列举!!\n", name);
                    i = tids[li];
                }
                chd.maxValue = (float)(li + 1); //最大值自动限制在可供列举项内
                if(*value > (2 << li) - 1) *value = (2 << li) - 1;
                else if(*value < 0) *value = 0;

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
                GT_SIMPLE_MENU_SearchMinEnumValue(*value, chd.minValue);
            }
            if(tmpn > ei) ei = tmpn; //计算到最长列举名称的字符数
            chd.enumIdnum = li + 3;
            chd.enumIds = new short[chd.enumIdnum];
            chd.enumIds[0] = -1; //首个成员值总是-1
            memcpy(chd.enumIds + 1, tids, li << 1); //将刚才记下的id复制到enumIds里
            chd.enumIds[chd.enumIdnum - 2] = i + 1; //倒数第二个记下'\0'结束的位置
            chd.enumIds[chd.enumIdnum - 1] = ei; //最后一个数为找到的最长列举名称的字符数

        } else if(!add_value) chd.addValue = 1.0f;
        chd.enumName = new char[i + 1]; //没分割即为附加注息 续后另开一行文字显示
        strncpy(chd.enumName, enum_name, i); //拷贝保存列举名
        chd.enumName[i] = 0;
    }
    else {
        if(*value < min_value) {
            *value = min_value;
            out_range = true;
        }
        if(*value > max_value) {
            *value = max_value;
            out_range = true;
        }
        GT_TMP_sSIM_MENU_OutRangeWarring
    }
}
void GNu::sSIM_MENU::InitValueToData(float *value, const char *name, float max_value, float min_value, float add_value, P<GNg::sIMAGE> icon) {
    GT_TMP_sSIM_MENU_HasInitValue(value) bool out_range = false;
    if(*value < min_value) {
        *value = min_value;
        out_range = true;
    }
    if(*value > max_value) {
        *value = max_value;
        out_range = true;
    }
    GT_TMP_sSIM_MENU_OutRangeWarring
    GT_TMP_sSIM_MENU_InitValue(GT_SP_VALUE_FLOAT)
}
void GNu::sSIM_MENU::InitSubMenuToData(sSIM_MENU *menu, const char *name, char menu_type, P<GNg::sIMAGE> icon) {
#ifdef GN_DEBUG
    if(!name || !name[0]) {
        GN_Log("调用sSIM_MENU::InitSubMenuToData 菜单名还没指定好!\n");
        return;
    }
    if(!menu) {
        GN_Log("调用sSIM_MENU::InitSubMenuToData %s 菜单指针还没指定好!\n", name);
        return;
    }
#else
    if(!name || !name[0] || !menu) return;
#endif
    zChild.push_back(GT_sSP_CHILD());
    GT_sSP_CHILD &chd = zChild.back();
    chd.type = GT_SP_MENU;
    chd.pointer = (void *)menu;
    chd.icon = icon;
    GN_ValueClamp(menu_type, 0, 3);
    menu->zType = menu_type;
    menu->zFather = this;
    menu->zIcon = icon;
    GT_TMP_sSIM_MENU_CopyName
        GN_DeleteArray(menu->zCaption);
    menu->zCaption = new char[len + 1];
    // TODO: verify buffer size
    strcpy(menu->zCaption, name);
}
void GNu::sSIM_MENU::InitTextToData(const char *text, int *value, P<GNg::sIMAGE> icon) {
#define GT_TMP_SIMPLE_MENU_InitText(VALUE_TYPE) \
    zChild.push_back(GT_sSP_CHILD());           \
    GT_sSP_CHILD &chd = zChild.back();          \
    chd.type = GT_SP_TEXT;                      \
    int len = strlen(text);                     \
    chd.name = new char[len + 1];               \
    /* TODO: verify buffer size */              \
    strcpy(chd.name, text);                     \
    chd.maxValue = (float)len;                  \
    chd.icon = icon;                            \
    if(value) {                                 \
        chd.pointer = value;                    \
        chd.maxValue += 18.0f;                  \
        chd.enumIdnum = VALUE_TYPE;             \
    }
    if(!text || !text[0]) {
#ifdef GN_DEBUG
        GN_Log("调用sSIM_MENU::InitTextToData 文本内容还没指定好!\n");
#endif
        return;
    }
    GT_TMP_SIMPLE_MENU_InitText(GT_SP_VALUE_INT)
}
void GNu::sSIM_MENU::InitTextToDataf(const char *text, float *value, P<GNg::sIMAGE> icon) {
    if(!text || !text[0]) {
#ifdef GN_DEBUG
        GN_Log("调用sSIM_MENU::InitTextToDataf 文本内容还没指定好!\n");
#endif
        return;
    }
    GT_TMP_SIMPLE_MENU_InitText(GT_SP_VALUE_FLOAT)
}
float GT_SimMenuTextOffsetPos; //临时记录文本读写时的偏移记数

bool GNu::sSIM_MENU::ReplaceChildName(const char *name, const char *new_name) {
#define GT_TMP_FIND_MENU_CHILD_FROM_NAME_BEGIN \
    if(zChild.empty()) return false;           \
    int si, se = zChild.size();                \
    for(si = 0; si < se; ++si) {               \
        if(!strcmp(zChild[si].name, name)) {

#define GT_TMP_FIND_MENU_CHILD_FROM_NAME_END(FUNC)                                     \
    return true;                                                                       \
    }                                                                                  \
    }                                                                                  \
    for(si = 0; si < se; ++si) {                                                       \
        if(zChild[si].type == GT_SP_MENU) {                                            \
            if(((sSIM_MENU *)(zChild[si].pointer))->FUNC(name, new_name)) return true; \
        }                                                                              \
    }                                                                                  \
    return false;

    GT_TMP_FIND_MENU_CHILD_FROM_NAME_BEGIN
    GN_StrcpyData(zChild[si].name, new_name);
    GT_TMP_FIND_MENU_CHILD_FROM_NAME_END(ReplaceChildName)
}
bool GNu::sSIM_MENU::InputChildContent(const char *name, const char *new_name) {
    GT_TMP_FIND_MENU_CHILD_FROM_NAME_BEGIN
    // TODO: verify buffer size
    strcpy((char *)zChild[si].pointer, new_name);
    GT_ChildTextValueSetShowStr(&zChild[si]);
    GT_TMP_FIND_MENU_CHILD_FROM_NAME_END(InputChildContent)
}
void GNu::sSIM_MENU::zWriteData(GNf::sFILE *fp) {
    if(zChild.empty()) return;
    GT_sSP_CHILD *_chd;
    int si, sn = zChild.size();
    for(si = 0; si < sn; ++si) {
        _chd = &(zChild[si]);
        if(_chd->type == GT_SP_VALUE_TEXT) {
            _chd->addValue = GT_SimMenuTextOffsetPos;
            GT_SimMenuTextOffsetPos += _chd->maxValue;
            GNf::Write((char *)&(_chd->maxValue), 4, 1, fp);

        } else if(_chd->type <= GT_SP_VALUE_FLOAT) { //先写入该选单的所有变量值
            GNf::Write((char *)(_chd->pointer), 4, 1, fp);
        }
    }
    for(si = 0; si < sn; ++si) {
        if(zChild[si].type == GT_SP_MENU) { //再查找子选单进入递归写入值
            ((sSIM_MENU *)(zChild[si].pointer))->zWriteData(fp);
        }
    }
}
void GNu::sSIM_MENU::zWriteText(GNf::sFILE *fp) {
    if(zChild.empty()) return;
    int si, sn = zChild.size();
    for(si = 0; si < sn; ++si) {
        if(zChild[si].type == GT_SP_VALUE_TEXT) {
            GNf::Write((char *)(zChild[si].pointer), 1, (int)(zChild[si].maxValue), fp); //写入文本内容
        }
    }
    for(si = 0; si < sn; ++si) {
        if(zChild[si].type == GT_SP_MENU) { //再查找子选单进入递归写文本内容
            ((sSIM_MENU *)(zChild[si].pointer))->zWriteText(fp);
        }
    }
}
void GNu::sSIM_MENU::zReadData(GNf::sFILE *fp, int &max_read_count) {
    if(max_read_count <= 0 || zChild.empty()) return;
    GT_sSP_CHILD *_chd;
    float read_num;
    int si, sn = zChild.size();
    for(si = 0; si < sn; ++si) {
        _chd = &(zChild[si]);
        if(_chd->type <= GT_SP_VALUE_FLOAT) { //先为选单的所有变量赋值
            switch(_chd->type) {
                case GT_SP_VALUE_FLOAT:
                    GNf::Read((char *)(_chd->pointer), 4, 1, fp);
                    read_num = *((float *)(_chd->pointer));
                    if(read_num < _chd->minValue || read_num > _chd->maxValue) { //检测读出的值超出范围时
                        max_read_count = GN_INT_MIN; //标记错误并立即返回取消所有变量的读取
                        return;
                    }
                    break;
                case GT_SP_VALUE_INT: {
                    GNf::Read((char *)(_chd->pointer), 4, 1, fp);
                    if(_chd->addValue) {
                        int v_ = *((int *)(_chd->pointer));
                        if(v_ < _chd->minValue || v_ > _chd->maxValue) {
                            max_read_count = GN_INT_MIN;
                            return;
                        }
                    } else { //多选项
                        unsigned int mask;
                        memcpy(&mask, _chd->pointer, 4);
                        if(mask > (1 << (unsigned int)(_chd->maxValue)) - 1) {
                            max_read_count = GN_INT_MIN;
                            return;
                        }
                    }
                } break;
                case GT_SP_VALUE_COLOR: {
                    unsigned int cr;
                    GNf::Read((char *)(_chd->pointer), 4, 1, fp);
                    memcpy(&cr, _chd->pointer, 4);
                    int ca = (cr & 0xff000000) >> 24;
                    if(ca < (int)(_chd->minValue) || ca > (int)(_chd->maxValue)) {
                        max_read_count = GN_INT_MIN;
                        return;
                    }
                } break;
                case GT_SP_VALUE_TEXT:
                    GNf::Read((char *)&read_num, 4, 1, fp);
                    if(_chd->maxValue != read_num) {
                        max_read_count = GN_INT_MIN;
                        return;
                    }
                    _chd->addValue = GT_SimMenuTextOffsetPos;
                    GT_SimMenuTextOffsetPos += _chd->maxValue;
                    break;
            }
            --max_read_count;
            if(max_read_count <= 0) return;
        }
    }
    for(si = 0; si < sn; ++si) {
        if(zChild[si].type == GT_SP_MENU) { //查找子选单进入递归赋值
            ((sSIM_MENU *)(zChild[si].pointer))->zReadData(fp, max_read_count);
            if(max_read_count <= 0) return;
        }
    }
}
void GNu::sSIM_MENU::zReadText(GNf::sFILE *fp, int &max_read_byte) {
    if(max_read_byte <= 0 || zChild.empty()) return;
    GT_sSP_CHILD *_chd;
    int si, sn = zChild.size(), txn;
    for(si = 0; si < sn; ++si) {
        _chd = &zChild[si];
        if(_chd->type == GT_SP_VALUE_TEXT) {
            txn = (int)(_chd->maxValue);
            if(max_read_byte >= txn) {
                GNf::Read((char *)(_chd->pointer), 1, txn, fp);
                ((char *)(_chd->pointer))[txn - 1] = 0;
                GT_ChildTextValueSetShowStr(_chd);
                max_read_byte -= txn;
            } else max_read_byte = GN_INT_MIN; //文本数据有误
            if(max_read_byte <= 0) return;
        }
    }
    for(si = 0; si < sn; ++si) {
        if(zChild[si].type == GT_SP_MENU) { //查找子选单进入递归赋值
            ((sSIM_MENU *)(zChild[si].pointer))->zReadText(fp, max_read_byte);
            if(max_read_byte <= 0) return;
        }
    }
}
void GNu::sSIM_MENU::zReDefault(bool re_sub) {
    if(zChild.empty()) return;
    GT_sSP_CHILD *_chd;
    int si, sn = zChild.size();
    for(si = 0; si < sn; ++si) {
        _chd = &(zChild[si]);
        switch(_chd->type) {
            case GT_SP_VALUE_INT: { //恢复整形默认值 默认值为 InitValueToDat 传入时的值
                memcpy(_chd->pointer, &(_chd->srcValue), 4);
                if(_chd->enumIds && !(_chd->addValue)) {
                    int value = *((int *)(_chd->pointer));
                    GT_SIMPLE_MENU_SearchMinEnumValue(value, _chd->minValue);
                }
            } break;
            case GT_SP_VALUE_COLOR: //恢复颜色默认值
            case GT_SP_VALUE_FLOAT: //恢复浮点默认值
                memcpy(_chd->pointer, &(_chd->srcValue), 4);
                break;
            case GT_SP_VALUE_TEXT: //恢复文本默认值
                // TODO: verify buffer size
                strcpy((char *)(_chd->pointer), _chd->enumName);
                GT_ChildTextValueSetShowStr(_chd);
                break;
        }
    }
    if(re_sub) {
        for(si = 0; si < sn; ++si) {
            if(zChild[si].type == GT_SP_MENU) { //再查找子选单进入递归恢复值
                ((sSIM_MENU *)(zChild[si].pointer))->zReDefault(re_sub);
            }
        }
    }
}
bool GNu::sSIM_MENU::zComputeMenuForDataOffset(int &offset_num_out, sSIM_MENU *menu) {
    if(menu == this || zChild.empty()) return 1;
    int si, sn = zChild.size();
    for(si = 0; si < sn; ++si) {
        if(zChild[si].type <= GT_SP_VALUE_FLOAT) {
            offset_num_out += 4; //先统计该选单的所有变量占字节数
        }
    }
    for(si = 0; si < sn; ++si) {
        if(zChild[si].type == GT_SP_MENU) { //再查找子选单进入递归统计
            if(((sSIM_MENU *)(zChild[si].pointer))->zComputeMenuForDataOffset(offset_num_out, menu)) return 1;
        }
    }
    return 0;
}
void GNu::sSIM_MENU::SetChildName(unsigned int id, const char *name) {
    if(id < zChild.size()) {
        GT_sSP_CHILD *_chd = &(zChild[id]);
        GN_StrcpyData(_chd->name, name);
        if(_chd->type == GT_SP_MENU && _chd->pointer) {
            sSIM_MENU *sub_menu = (sSIM_MENU *)(_chd->pointer);
            GN_StrcpyData(sub_menu->zCaption, name);
        }
    }
}
/********************************************************************* cSIM_DIALOG *******************************************************************/
char GNu::cSIM_DIALOG::aFuntionButtonName[2][3][9] = {{"取 消", "保 存", "默 认"}, {"CANCEL", "SAVE", "DEFAULT"}};

void GNu::cSIM_DIALOG::zDeleteArrowMap() {
    if(zUsGenArrowMap) {
        GN_Delete(zArrowMap);
        zUsGenArrowMap = 0;
    } else zArrowMap = 0;
}
void GNu::cSIM_DIALOG::zRelease_(bool all) {
    zTextOut.Release();
#ifdef _GN_SOUND_H_
    zSound.Release();
#endif
    zDeleteArrowMap();
    if(all) sSIM_MENU::ReleaseAll();
    else sSIM_MENU::Release();
    GN_DeleteArray(zButtonName);
    GN_DeleteArray(zFileName);
    zSetTool.Delete();

    zIsDetailSet = 0;
    zShowReady = 0;
    zState = 0;
    zFocusMap = 0;
    zTargetMenu = 0;
    zEffect = 0;
    zBackCaption = 0;
    zBackFrame = 0;
    zBackDrawProc = 0;
    aValueLoop = 1;
}
GNu::cSIM_DIALOG::cSIM_DIALOG() : zBackDrawProc(0),
                                  zValueChangeProc(0),
                                  zValueChangeArg(0),
                                  zFun1(0), zFun2(0), zFun3(0),
                                  zButtonName(0),
                                  zFileName(0),
                                  zShowReady(0),
                                  zState(0),
                                  zTargetMenu(0),
                                  zEffect(0),
                                  zFocusFlash(0),
                                  zFocusMap(0),
                                  zArrowMap(0),
                                  zBackCaption(0),
                                  zBackFrame(0),
                                  zUsGenArrowMap(0),
                                  aCaptextOfsY(0),
                                  aValueLoop(1),
                                  aSkipPos(0),
                                  zIsDetailSet(0),
                                  zKeyDOWN(GK_DOWN),
                                  zKeyUP(GK_UP),
                                  zKeyLEFT(0),
                                  zKeyRIGHT(1),
                                  zKeyOK(GK_RETURN),
                                  zUsMouse(0),
#ifdef _GN_SOUND_H_
                                  zSouObj(0)
#endif
{
}
GNu::cSIM_DIALOG::~cSIM_DIALOG() {
    zDeleteArrowMap();
    GN_DeleteArray(zButtonName);
    GN_DeleteArray(zFileName);

    aValueLoop = 1;
    zIsDetailSet = 0;
    zShowReady = 0;
    zState = 0;
    zTargetMenu = 0;
    zEffect = 0;
    zFocusMap = 0;
    zBackCaption = 0;
    zBackFrame = 0;
    zBackDrawProc = 0;
}
void GNu::cSIM_DIALOG::BeginInitToData(const char *fname) {
    if(fname && fname[0]) { //拷贝保存名称
        int len = strlen(fname);
        int di = len - 1;
        int de = (di - 6) > 0 ? (di - 6) : 0;
        for(; di > de; --di)
            if(fname[di] == '.') break;
        if(di == de) di = len;
        GN_DeleteArray(zFileName);
        zFileName = new char[di + 5];
        strncpy(zFileName, fname, di);
        // TODO: verify buffer size
        strcpy(zFileName + di, ".ygs");
    }
}
void GNu::cSIM_DIALOG::EndInitToData() {
    if(!zFileName) { //没指定文件名就为它创建一个默认文件
        char *def_file_name = "GameSetting.ygs";
        zFileName = new char[strlen(def_file_name) + 1];
        // TODO: verify buffer size
        strcpy(zFileName, def_file_name);
    }
    /*- ygs文件格式数据布局 --------\   /- ygt文件格式数据布局 ----------*\   
	|	char 文件类型标识4个字节    |	|	char 文件类型标识4个字节      |
    |	void *数据 每个占4个字节    |	|	void *数据 紧接的多段字符串   |
	\*------------------------------/   \--------------------------------*/

    // 读取文件数据初始化选单变量值 ---------------------------------------------------------------------------------------------------
    int file_var_num = GN_INT_MIN;
    GNf::sFILE *pfile = GNf::Open(zFileName, "rb", GNf::eFILE_SAFE);
    if(pfile) { //确认文件存在
        const char *err_str = "调用cSIM_DIALOG::EndInitToData 失败! %s: %s %s!\n";
        char file_type[4];
        int file_size = GNf::GetSize(pfile);
        file_var_num = (file_size >> 2) - 1; //计算得到变量值数
        GNf::Read(file_type, 1, 4, pfile);
        if(!strcmp(file_type, "YGS")) { //确认是该文件类型
            GT_SimMenuTextOffsetPos = 0.0f;
            zReadData(pfile, file_var_num); //递归读入数据 不够的保持默认值
            const char *err_str1 = "读取简单对话筐存档文件数据有错! 该.yg%c文件是过期的! 与当前对话筐变量条目或层次结构匹配不上!\n";
            if(file_var_num == GN_INT_MIN) {
                zReDefault(true); //出错时就恢复为默认值
                GN_Log(err_str1, 's'); //文件不对应 这可能在生成该文件后又在代码里修改过变量条目或文件搞错或损坏

            } else if(GT_SimMenuTextOffsetPos) { //有文本要读
                GNf::Close(pfile);
                char *name_end = zFileName + (strlen(zFileName) - 1);
                *name_end = 't';
                pfile = GNf::Open(zFileName, "rb", GNf::eFILE_SAFE);
                if(pfile) {
                    file_size = GNf::GetSize(pfile);
                    GNf::Read(file_type, 1, 4, pfile);
                    if(!strcmp(file_type, "YGT")) { //确认是该文件类型
                        file_var_num = file_size - 4;
                        zReadText(pfile, file_var_num); //递归读入文本 不够的保持默认值
                        if(file_var_num == GN_INT_MIN) {
                            zReDefault(true); //出错时就恢复为默认值
                            GN_Log(err_str1, 't'); //文件不对应 这可能在生成该文件后又在代码里修改过变量条目或文件搞错或损坏
                        }
                    } else {
                        file_var_num = GN_INT_MIN;
                        GN_Log(err_str, "文件", zFileName, "损坏");
                    }
                } else {
                    file_var_num = GN_INT_MIN;
                    GN_Log(err_str, "找不到文件", zFileName, "文本数据读取失败");
                }
                *name_end = 's';
            }
        } else {
            file_var_num = GN_INT_MIN;
            GN_Log(err_str, "文件", zFileName, "损坏");
        }
        GNf::Close(pfile);
    }
    if(file_var_num == GN_INT_MIN) { //当文件不存在时 将数据保存成一个新文件
        const char *err_str2 = "调用cSIM_DIALOG::EndInitToData 安全创建文件: %s 失败!\n";
        uint8 ygi = 0;
        for(; ygi < 2; ++ygi) { //安全保存数据会创建两个文件
            pfile = GNf::Open(zFileName, "wb", GNf::eFILE_SAFE);
            if(pfile) {
                char file_type2[] = "YGS";
                GNf::Write(file_type2, 1, 4, pfile); //写文件头
                GT_SimMenuTextOffsetPos = 0.0f;
                zWriteData(pfile); //递归写入数据
                GNf::Close(pfile);

                if(GT_SimMenuTextOffsetPos) { //有文本要写就另创建个ygt文件
                    char *name_end = zFileName + (strlen(zFileName) - 1);
                    *name_end = 't';
                    pfile = GNf::Open(zFileName, "wb", GNf::eFILE_SAFE);
                    if(pfile) {
                        file_type2[2] = 'T';
                        GNf::Write(file_type2, 1, 4, pfile); //写文件头
                        zWriteText(pfile); //递归写入文本内容
                        GNf::Close(pfile);
                        *name_end = 's';
                    } else {
                        *name_end = 's';
                        GN_Log(err_str2, zFileName);
                        break;
                    }
                }
            } else {
                GN_Log(err_str2, zFileName);
                break;
            }
        }
    }
}
bool GNu::cSIM_DIALOG::SetStyleToData(const char *caption, const GNg::cTEXT_ART *text_style, int8 type, unsigned int capt_color,
                                      unsigned int text_color, unsigned int back_color, unsigned int line_color, unsigned int focus_color,
                                      unsigned short text_blend, unsigned short back_blend, unsigned short line_blend, unsigned short focus_blend,
                                      unsigned short line_width, P<GNg::sIMAGE> icon) {
    GN_StrcpyData(zCaption, caption);
    if(!zCaption) {
        zShowReady = false;
        return false;
    }
    zShowReady = true;
    if(text_style) {
        ((GNg::cTEXT_ART *)text_style)->Release();
        zTextOut.Release();
        zTextOut = *text_style;
    } else if(GN::RunRec.zGWPrintReady) {
        GN::RunRec.aFont.Release();
        zTextOut.Release();
        zTextOut = GN::RunRec.aFont;
    } else {
        zShowReady = false;
        return false;
    }
    GNg::sARRANGE_FMT *p_arrg = zTextOut.GetArrangeFormat();
    if(p_arrg->erect_print || p_arrg->overturn_print) GN_Log("%s 文字式样参数的排列方法必须是从左到右的横方向输出!\n", "调用cSIM_DIALOG::SetStyleToData");

    // 记下文本式样起始状态
    GNg::sARRANGE_FMT &tfmt = zTextOut.zParame;
    tfmt.near_pointV = 0.5f;
    zTwidth = tfmt.width;
    zTi_compartment = tfmt.i_compartment;
    zTheight = tfmt.height;
    zTj_compartment = tfmt.j_compartment;

    // 颜色赋值
    aCaptextColor = capt_color ? capt_color : 0xffffffff;
    aTextColor = text_color ? text_color : GT_AppDefaultColorFont;
    aTextBlend = text_blend;
    aBackColor = back_color ? back_color : GT_AppDefaultColorBack;
    aBackBlend = back_blend;
    aLineColor = line_color ? line_color : GT_AppDefaultColorFont;
    aLineBlend = line_blend;
    aFocusColor = focus_color;
    aFocusBlend = focus_blend;
    aLineWidth = line_width;

    GN_ValueClamp(type, 0, 3);
    zType = type;
    zIcon = icon;

    //生成小箭头图
    if(!zArrowMap) {
        zArrowMap = new GNg::sIMAGE;

#define GT_TMP_MAKE_SIM_ARROW_ICON                                          \
    int w2 = 1, tw = (int)zTwidth;                                          \
    while(w2 < tw) { w2 <<= 1; }                                            \
    if(w2 - tw > tw - (w2 >> 1)) w2 >>= 1; /*获得一个最接近字宽的2N次方数*/ \
    MakeSimpleArrowIcon(zArrowMap, w2, (int)((float)zTheight * 0.7f), 0, 1);

        GT_TMP_MAKE_SIM_ARROW_ICON
        zUsGenArrowMap = true;
    }
    return zShowReady;
}
void GNu::cUI_SIM_DIALOG::ArrowMapTransfer() {
    if(zUsGenArrowMap && zArrowMap) {
        zArrowMap1.New(1);
        *zArrowMap1 = *zArrowMap;
        zArrowMap1->cloneImg = false;
        zArrowMap->cloneImg = true;
        GN_Delete(zArrowMap);
        zUsGenArrowMap = 0;
        zArrowMap = zArrowMap1;
    }
}
void GNu::cSIM_DIALOG::zSetActiveMenu(const sSIM_MENU *menu) {
    if(menu) {
        zTargetType = menu->zType;
        zTargetCaption = menu->zCaption;
        zTargetMenu = (sSIM_MENU *)menu;
        zTargetIcon = zTargetMenu->zIcon;
    }
    zStatisticsDlgTextNum(zTargetMenu, zTargetCaption); //预先计算好各项文本的格位个数与大小
    zStatisticsDlgSize(1.0f, 1.0f); //计算好各项文本的格位实际显示大小
    zDlgx = (short)(GNu::NearCx() - (zDlgw >> 1)) + zDlaOffsetX; //计算对话筐显示坐标 没效果时默认居中显示
    zDlgy = (short)(GNu::NearCy() - (zDlgh >> 1)) + zDlaOffsetY;
}
void GNu::cSIM_DIALOG::Pop() {
    if(!zShowReady) SetStyleToData(); //如还未设置显示式样就自动设置一下 若已开启了调试信息即可以借助它的文字显示
    if(zEffect) {
        zEffect->aFrame_ts.x = zT_ef_x;
        zEffect->aFrame_ts.y = zT_ef_y;
    }
    zDlaOffsetX = 0; //恢复原设置的对话筐弹出位置
    zDlaOffsetY = 0;
    zSetActiveMenu(this);
}
void GNu::cSIM_DIALOG::Pop(const char *new_cap) {
    GN_StrcpyData(zCaption, new_cap);
    Pop();
}
void GNu::cSIM_DIALOG::RePop() {
    zSetActiveMenu();
}
void GNu::cSIM_DIALOG::OffsetPos(int ofsx, int ofsy) {
    if(zEffect) {
        zEffect->aFrame_ts.x += ofsx;
        zEffect->aFrame_ts.y += ofsy;
        zDlgx += ofsx;
        zDlgy += ofsy;
    } else {
        zDlaOffsetX += ofsx;
        zDlaOffsetY += ofsy;
        zDlgx += ofsx;
        zDlgy += ofsy;
    }
}
void GNu::cSIM_DIALOG::SetFileSelectOpenDirectoryID(uint8 did) {
    if(zSetTool) zSetTool->fb_open_did = did;
}
unsigned int GNu::cSIM_DIALOG::GetEffectLevelColor() {
    return (zEffect ? zEffect->aLevel_color : 0xffffffff);
}
void GNu::cSIM_DIALOG::GetDlgPos(int &_x, int &_y, int &_w, int &_h) {
    _x = zDlgx, _y = zDlgy, _w = zDlgw, _h = zDlgh;
}
void GNu::cSIM_DIALOG::GetDlgRect(GNg::sRECT *rc_out) {
    rc_out->left = zDlgx;
    rc_out->top = zDlgy;
    rc_out->right = (zDlgx + zDlgw);
    rc_out->bottom = (zDlgy + zDlgh);
}
int GNu::cSIM_DIALOG::zGetLastDetailSetCY() {
    if(zSetTool) return zSetTool->fy + (zSetTool->fh >> 1);
    return -100000;
}
void GNu::cSIM_DIALOG::GetDetailSetPos(int &_x, int &_y, int &_w, int &_h) {
    if(!zDetailSetState == eDETAIL_SET_NULL) return;
    GT_sVALUE_SET_TOOL *_set_tool = zSetTool;
    _x = _set_tool->fx;
    _w = _set_tool->fw;
    _y = _set_tool->fy;
    _h = _set_tool->fh;
}
void GNu::cSIM_DIALOG::GetDetailSetPos(GNg::sRECT *rc_out) {
    if(!zDetailSetState == eDETAIL_SET_NULL) return;
    GT_sVALUE_SET_TOOL *_set_tool = zSetTool;
    rc_out->left = _set_tool->fx;
    rc_out->top = _set_tool->fy;
    rc_out->right = _set_tool->fx + _set_tool->fw;
    rc_out->bottom = _set_tool->fy + _set_tool->fh;
}
void GNu::cSIM_DIALOG::GetChildPos(int &_x, int &_y, int &_w, int &_h, short chd_id, eCHILD_CONTENT_TYPE content) {
    if(!zState) {
#ifdef GN_DEBUG
        GN_Log("调用cSIM_DIALOG::GetChildPos 还没准备好! 请先调用 Pop();\n");
#endif
        return;
    }
    if(!zChildNum) {
#ifdef GN_DEBUG
        GN_Log("调用cSIM_DIALOG::GetChildPos 失败! 由于没有子选项! 请先调用 InitValue___(...);\n");
#endif
        _x = 0;
        _y = 0;
        _w = 0;
        _h = 0;
        return;
    }
    if(chd_id < 0) chd_id = 0;
    else if(chd_id >= zChildNum) chd_id = zChildNum - 1; //防止出轨
    uint8 type = zTargetMenu->zChild[chd_id].type;

#define GT_TMP_SIM_DIALOG_DRAW_COMPARTENT_CPT    \
    GNg::sARRANGE_FMT &tfmt = zTextOut.zParame;  \
    short ti = tfmt.i_compartment,               \
          tj_ = tfmt.j_compartment;              \
    short ti2 = ti << 1,                         \
          tj = (tj_ < zIconH) ? zIconH : tj_,    \
          tj2 = (tj_ < zIconH2) ? zIconH2 : tj_; \
    short iw = zIconW ? zIconW + ti : 0;

    GT_TMP_SIM_DIALOG_DRAW_COMPARTENT_CPT //计算好绘制间距
        short th_ofs = (tj - tj_) >> 1; //输入文本筐显示高度相对该项显示高度的偏移量

    short draw_x = zDlgx + zBorderW + ti2,
          draw_y = zDlgy + zBorderW + tj2 + tj_,
          draw_w = zDlgw - (zBorderW << 1) - (ti2 << 1); //焦点缩至内边筐
    short dx, dw; //绘制焦点的当前位置
    if(type <= GT_SP_VALUE_FLOAT) {
        dx = draw_x + ti;
        dw = draw_w - ti2;
        dx += (short)((float)(dw - zSubValueW) * tfmt.near_pointU); //显示变量栏的焦点范围
    } else {
        dx = draw_x;
        dw = draw_w;
        if(content > eCHILD_CONTENT_NAME) content = eCHILD_CONTENT_NAME;
    }
    switch(content) {
        case eCHILD_CONTENT_ALL: //整个子选项范围
            _x = (dx - (ti >> 1));
            _w = (dw + ti);
            _y = (draw_y + chd_id * tj);
            _h = tj;
            return;
        case eCHILD_CONTENT_ICON: //子选项图标范围
            _x = dx;
            _w = zIconW;
            _y = (draw_y + chd_id * tj);
            _h = tj;
            return;
        case eCHILD_CONTENT_NAME: //子选项标题名范围
            _x = dx + iw;
            _w = type <= GT_SP_VALUE_FLOAT ? zNameW : (draw_x + draw_w - _x);
            _y = draw_y + chd_id * tj + th_ofs;
            _h = tj_;
            return;
    }
        // 剩下其它可调值的子选项内容 --------------------------------------------------------------------------------------------
#define GT_TMP_CHILD_DRAW_POSX_CPT                                     \
    short icx = dx; /*变量图标显示X*/                                  \
    short vnx = icx + iw; /*变量名显示X*/                              \
    short va1x = vnx + zNameW + (ti << 1) + (ti >> 1); /*左箭头显示X*/ \
    short vvx = va1x + (ti >> 1); /*变量值显示X*/                      \
    short va2x = vvx + zValueW + (ti >> 1); /*右箭头显示X*/

    GT_TMP_CHILD_DRAW_POSX_CPT //计算子选项各个内容显示位置X
        short arr_w = (short)((float)zArrowMap->width * (float)ti / (float)zTi_compartment); //计算经比例变换后的箭头宽
    if(type >= GT_SP_VALUE_COLOR) {
        short arr_h = (short)((float)zArrowMap->height * (float)ti / (float)zTi_compartment); //计算经比例变换后的箭头高
        short ah_ofs = (tj - arr_h) >> 1; //箭头显示高度偏移
        short ch_ofs = (tj - tfmt.height) >> 1; //颜色格显示高度偏移

        switch(content) {
            case eCHILD_CONTENT_VALUE: //子选项标题名范围
                _x = vvx;
                _w = zValueW;
                _y = draw_y + chd_id * tj + ch_ofs;
                _h = zTextOut.zParame.height;
                return;
            case eCHILD_CONTENT_ARWL: //子选项标题名范围
                _x = va1x - arr_w;
                _w = arr_w;
                _y = draw_y + chd_id * tj + ah_ofs;
                _h = arr_h;
                return;
            case eCHILD_CONTENT_ARWR: //子选项标题名范围
                _x = va2x;
                _w = arr_w;
                _y = draw_y + chd_id * tj + ah_ofs;
                _h = arr_h;
                return;
            default: { //eCHILD_CONTENT_LABLE 子选项标题名范围
                _x = va2x + ti + ti + (ti >> 1); //临时附加注息显示X
                _w = zEnumNameW;
                _y = draw_y + chd_id * tj + th_ofs;
                _h = tj_;
            }
                return;
        }
    } else if(type == GT_SP_VALUE_TEXT) { //子选项文本输入筐范围
        short text_in_w = va2x - va1x; //输入文本筐的宽
        _x = va1x - (arr_w >> 1);
        _w = text_in_w + arr_w;
        _y = draw_y + chd_id * tj + th_ofs;
        _h = tj_;
    }
}
void GNu::cSIM_DIALOG::GetChildRect(GNg::sRECT *rect_out, short chd_id, eCHILD_CONTENT_TYPE content) {
    int _x, _y, _w, _h;
    GetChildPos(_x, _y, _w, _h, chd_id, content);
    rect_out->left = _x;
    rect_out->right = _x + _w;
    rect_out->top = _y;
    rect_out->bottom = _y + _h;
}
#ifdef _GN_SOUND_H_
void GNu::cSIM_DIALOG::SetKeyToData(const GNa::sSOUND *sound, short ok, short down, short up, short left, short right, bool usMouse) {
    zSouObj = (GNa::sSOUND *)sound;
    zSound.InitToData(zSouObj);
#else
void GNu::cSIM_DIALOG::SetKeyToData(const void *key_sound, short ok, short down, short up, short left, short right, bool usMouse) {
#endif
    zKeyOK = ok;
    zKeyDOWN = down;
    zKeyUP = up;
    zKeyLEFT = left;
    zKeyRIGHT = right;
    zUsMouse = usMouse;
}
void GNu::cSIM_DIALOG::SetBackFrame(const GNg::sIMAGE *frame_img, const GNg::sIMAGE *caption_img, bool repeat, void (*draw_call)(short, short, short, short, unsigned int, unsigned short, GNg::sIMAGE *)) {
    zBackFrameRepeat = repeat;
    zBackCaption = (GNg::sIMAGE *)caption_img;
    zBackDrawProc = draw_call;
    if(draw_call) zBackFrame = (GNg::sIMAGE *)frame_img;
    else if(frame_img) {
        if(frame_img->texnum == 9) {
            zBackFrame = (GNg::sIMAGE *)frame_img;
        } else {
            zBackFrame = 0;
#ifdef GN_DEBUG
            GN_Log("调用cSIM_DIALOG::SetBackFrame 背景筐图不是有效的9宫格图!\n");
#endif
        }
    } else zBackFrame = 0;
}
void GNu::cSIM_DIALOG::SetEffect(GNg::cFRAME_RC *effect, short focus_flash_time, const GNg::sIMAGE *focus_style, const GNg::sIMAGE *arrow_style, bool affect_color) {
    if(effect) {
        zEffect = effect;
        GNg::sRECT2 *_rc = &(effect->zRcLoc);
        float ratio_w = (float)effect->zPlay_obj->width / (_rc->right - _rc->left);
        float ratio_h = (float)effect->zPlay_obj->height / (_rc->bottom - _rc->top);
        zT_effect_rect.left = _rc->left * ratio_w;
        zT_effect_rect.top = _rc->top * ratio_h;
        zT_effect_rect.right = _rc->right * ratio_w;
        zT_effect_rect.bottom = _rc->bottom * ratio_h;
        zT_ef_x = effect->aFrame_ts.x;
        zT_ef_y = effect->aFrame_ts.y;
        aAffectColor = affect_color;
    }
    zFocusMap = (GNg::sIMAGE *)focus_style;
    zFocusFlash = focus_flash_time;
    if(arrow_style) {
        zDeleteArrowMap();
        zArrowMap = (GNg::sIMAGE *)arrow_style;
    }
}
void GNu::cSIM_DIALOG::EnableDetailSetting(bool detail_set, unsigned int input_bg_color, unsigned short input_bg_blend, eSET_PATH_TYPE set_path_type) {
    eSET_PATH_TYPE set_path_type1 = zSetPathType;
    zSetPathType = set_path_type;
    zIsDetailSet = detail_set;
    aInputBackColor = input_bg_color;
    aInputBackBlend = input_bg_blend;

    if(zIsDetailSet) {
        GT_sVALUE_SET_TOOL *_set_tool;
        if(zSetTool) _set_tool = zSetTool;
        else {
            zSetTool.New(1);
            _set_tool = zSetTool;
            _set_tool->text_in->InitStyle(&zTextOut, 0, 0, 256, 32, aCaptextColor, aFocusColor, aTextBlend, aFocusBlend);
            _set_tool->palette->InitToData(0, 0, 256, 256);
            _set_tool->palette->InitDefaultTable();
        }
        if(set_path_type != eSET_PATH_INPUT) {
            cFILE_BROWSER::GetIcon();
            if(!_set_tool->dir_strs) {
                _set_tool->dir_strs = GNf::EnumSystemDirectory();

                _set_tool->ftf_strs.New(7);
                P<char> *_ft_strs = _set_tool->ftf_strs;
                (_ft_strs[0])("*", 0);
                (_ft_strs[1])("txt,html,json,ini,inf", 0);
                (_ft_strs[2])(GNg::GetSupportImgFmtFileExtName(), 0);
                (_ft_strs[3])(GNa::GetSupportAudioFmtFileExtName(), 0);
                (_ft_strs[4])("avi,flv,mp4,mgp,mkv", 0);
                (_ft_strs[5])("obj,fbx,gmh", 0);
                (_ft_strs[6])("zip,rar,7z", 0);

                _set_tool->fts_strs.New(4);
                _ft_strs = _set_tool->fts_strs;
                if(zTextOut.zFontMapFormat < GNg::eFONT_FREETYPE) {
                    (_ft_strs[0])("Arranged by name", 0);
                    (_ft_strs[1])("Arranged by size", 0);
                    (_ft_strs[2])("Arranged by type", 0);
                    (_ft_strs[3])("Arranged by modified time", 0);
                } else {
                    (_ft_strs[0])("按名称排列", 0);
                    (_ft_strs[1])("按大小排列", 0);
                    (_ft_strs[2])("按类型排列", 0);
                    (_ft_strs[3])("按修改时间排列", 0);
                }
            }
            if(set_path_type != set_path_type1) {
                _set_tool->file_browser.Delete();
                _set_tool->fb_select.Delete();
            }
        }
        _set_tool->info[0] = 0;
    } else zSetTool.Delete();
}
void GNu::cSIM_DIALOG::PathSelectDirectoryAdd(P<P<char>> dirs) {
    if(zSetTool && zSetTool->dir_strs) {
        dirs.Insert(zSetTool->dir_strs);
        zSetTool->dir_strs = dirs;
    }
}
void GNu::cSIM_DIALOG::ReplaceFuntion(char button_name[2][3][9], void (*fun1)(), void (*fun2)(), void (*fun3)()) {
    if(fun1) zType = 1;
    zFun1 = fun1;
    if(fun2) zType = 2;
    zFun2 = fun2;
    if(fun3) zType = 3;
    zFun3 = fun3;

    GN_DeleteArray(zButtonName);

#define GT_DLG_BNAME_SIZE (2 * 3 * 9)
    unsigned int size = GT_DLG_BNAME_SIZE;
    zButtonName = new char[size];
    memcpy(zButtonName, button_name, size); //保存按钮名
}
void GNu::cSIM_DIALOG::OnValueChange(void (*proc)(void *), void *us_arg) {
    zValueChangeProc = proc;
    zValueChangeArg = us_arg;
}
void GNu::cSIM_DIALOG::zStatisticsDlgTextNum(const sSIM_MENU *menu, const char *caption) {
    //- 统计正文区内各项要显示的最大字数 -----------------------------------------------------------
    zIconNW = 0, zIconNH = 0, zTotalLenNum = 0, zSubMenuNum = 0, zNameNum = 0, zEnumNameNum = 0;
    zValueNum = 10; //变量值预留10位
    short len, si, li;
    zChildNum = menu->zChild.size();
    GT_sSP_CHILD *_chd;
    for(si = 0; si < zChildNum; ++si) {
        _chd = (GT_sSP_CHILD *)&(((sSIM_MENU *)menu)->zChild[si]);
        if(_chd->icon) {
            len = (short)_chd->icon->width;
            if(zIconNW < len) zIconNW = len;
            len = (short)_chd->icon->height;
            if(zIconNH < len) zIconNH = len;
        }
        switch(_chd->type) {
            case GT_SP_VALUE_INT:
                len = strlen(_chd->name);
                if(zNameNum < len) zNameNum = len; //比较找出最长的名称是字符数
                if(_chd->enumName) { //有列举名
                    if(_chd->enumIds) { //且有分割
                        len = _chd->enumIds[_chd->enumIdnum - 1];
                        if(zValueNum < len) zValueNum = len;
                    } else { //没有分割
                        len = strlen(_chd->enumName);
                        if(zEnumNameNum < len) zEnumNameNum = len;
                    }
                }
                break;
            case GT_SP_VALUE_FLOAT:
                len = strlen(_chd->name);
                if(zNameNum < len) zNameNum = len; //比较找出最长的名称是字符数
                break;
            case GT_SP_VALUE_TEXT:
                len = strlen(_chd->name);
                if(zNameNum < len) zNameNum = len;
                len = (short)_chd->minValue;
                if(zValueNum < len) zValueNum = len;
                break;
            case GT_SP_VALUE_COLOR:
                len = strlen(_chd->name);
                if(zNameNum < len) zNameNum = len;
                break;
            case GT_SP_MENU:
                len = strlen(_chd->name);
                if(zSubMenuNum < len) zSubMenuNum = len;
                break;
            case GT_SP_TEXT:
                len = (short)_chd->maxValue;
                if(zSubMenuNum < len) zSubMenuNum = len;
                break;
        }
    }
    //- 找出横方向最大可能排的字符数 ---------------------------------------------------------------
    zTotalLenNum = zSubMenuNum + (2 << 1); //子选单边距两字宽
    if(caption) {
        zCaptionNum = strlen(caption);
        len = zCaptionNum + 2; //标提边距1字宽
        if(zTotalLenNum < len) zTotalLenNum = len;
    }
    zSubValueNum = zNameNum + zValueNum + zEnumNameNum + (3 << 1); //箭头占用3字宽
    len = zSubValueNum + (3 << 1); //子变量项边距3字宽
    if(zTotalLenNum < len) zTotalLenNum = len;

    if(zTargetType) {
        if(zTextOut.zFontMapFormat < GNg::eFONT_FREETYPE) {
            zFuncButtonNum = 9; //" DEFAULT ";
            len = strlen("  SAVE    CANCEL   DEFAULT ") + ((2 + 3) << 1); //功能按纽项间隔2字宽 边距3字宽
        } else {
            zFuncButtonNum = 9; //"  默 认  ";
            len = strlen("  保 存    取 消    默 认  ") + ((2 + 3) << 1);
        }
        if(zTotalLenNum < len) zTotalLenNum = len;
    }
    //重置一些其他内容变量值
    if(zSetTool) zSetTool->info[0] = 0;
    zDetailSetState = eDETAIL_SET_NULL;
    zDrawBackFrame.right = 0;
    zSelectLast = 0;
    zSelect = 0;
    _chd = zTargetMenu->zChild.data;
    while(zSelect < zChildNum && _chd[zSelect].type == GT_SP_TEXT) ++zSelect; //跳过文本提示
    if(zEffect) {
        zSelectLast = zSelect;
        zState = 8;
    } else zState = 2;
    zSelectLockID = 30000;
    zIsMoveDlg = 0;
    zfNowtime = 0;
    zNowtime = 0;
    zfAm_s_e = 0;
}
void GNu::cSIM_DIALOG::zStatisticsDlgSize(float ratio_w, float ratio_h) {
    // 计算出各项宽的实际尺寸 ----------------------------------------------------------------
    float char_w = (float)zTi_compartment * ratio_w;
    float char_h = (float)zTj_compartment * ratio_h;

    zSubValueW = (short)((float)zSubValueNum * char_w);
    zValueW = (short)((float)zValueNum * char_w);
    zNameW = (short)((float)zNameNum * char_w);
    zEnumNameW = (short)((float)zEnumNameNum * char_w);
    zFuncButtonW = (short)((float)zFuncButtonNum * char_w);
    zBorderW = aLineWidth >> 1 + 2; //标题距线筐缝隙2个象素宽

    zIconW = (short)((float)zIconNW * ratio_w);
    zIconH = (short)((float)zIconNH * ratio_h);
    zIconW2 = zTargetIcon ? (short)((float)zTargetIcon->width * ratio_w) : 0;
    zIconH2 = zTargetIcon ? (short)((float)zTargetIcon->height * ratio_h) : 0;

    short max_icn_w = zIconW > zIconW2 ? zIconW : zIconW2;
    if(max_icn_w) zDlgw = (short)((float)(zTotalLenNum + 1) * char_w) + (zBorderW << 1) + max_icn_w;
    else zDlgw = (short)((float)zTotalLenNum * char_w) + (zBorderW << 1);

    float icn1_h = (char_h < (float)zIconH) ? (float)zIconH : char_h;
    float icn1_h2 = (char_h < (float)zIconH2) ? (float)zIconH2 : char_h;
    //主内容顶离对话筐顶1字高与1标题栏高 主内容底离对话筐底2.5字高
    zDlgh = (short)(icn1_h2 + icn1_h * (float)(zChildNum) + char_h * (1.0f + 2.5f) + (float)(zBorderW << 1));
    if(zTargetType) zDlgh += char_h;

    GNg::sARRANGE_FMT &tfmt = zTextOut.zParame;
    tfmt.width = (short)((float)zTwidth * ratio_w); //字体大小也按比例缩放一下
    tfmt.height = (short)((float)zTheight * ratio_h);
    tfmt.i_compartment = (short)char_w;
    tfmt.j_compartment = (short)char_h;

    if(!tfmt.width) tfmt.width = 1; //确保排版值至少为1
    if(!tfmt.height) tfmt.height = 1;
    if(!tfmt.i_compartment) tfmt.i_compartment = 1;
    if(!tfmt.j_compartment) tfmt.j_compartment = 1;
}
void GNu::cSIM_DIALOG::zDetailSetReturnLongLinear(GT_sVALUE_SET_TOOL *_set_tool, const GNg::sRECT *rng) {
    if(zEffect) {
        _set_tool->fy = rng->top; //计算插值焦点显示位置
        _set_tool->fh = rng->bottom;
        _set_tool->linear_ing = true;
        float lr = 160.0f / (float)zEffect->zEndtime;
        GN::ValueChange(&(_set_tool->fx), (short)(rng->left), 0, 0, 0.5f, lr);
        GN::ValueChange(&(_set_tool->fw), (short)(rng->right), 0, &_set_tool->linear_ing, 0.5f, lr);
        zDetailSetState = 0xff;
    } else zDetailSetState = eDETAIL_SET_NULL;
}
void GNu::cSIM_DIALOG::zDetailSetReturnRangeLinear(GT_sVALUE_SET_TOOL *_set_tool, const GNg::sRECT *rng, unsigned int tg_color) {
    if(zEffect) {
        float lr = 130.0f / (float)zEffect->zEndtime;
        _set_tool->linear_ing = true;
        GN::ValueChange(&_set_tool->fy, (short)(rng->top), 0, 0, 0.5f, lr);
        GN::ValueChange(&_set_tool->fh, (short)(rng->bottom), 0, &_set_tool->linear_ing, 0.5f, lr);
        GN::ValueChange(&_set_tool->fx, (short)(rng->left), 0, 0, 0.5f, lr);
        GN::ValueChange(&_set_tool->fw, (short)(rng->right), 0, 0, 0.5f, lr);
        GN::ValueChangeColor(&_set_tool->fc, tg_color, 0, 0, 0.5f, lr);
        zDetailSetState = 0xfe;
    } else zDetailSetState = eDETAIL_SET_NULL;
}
/********************************************************************************************************/
//对话框设置工具提示信息状态重置(变量设置工具集, 动画效果框, 不透明度色)
void GT_DlgSetToolInfoStateReset(GT_sVALUE_SET_TOOL *_set_tool, const GNg::cFRAME_RC *_ef, unsigned int ca) {
    if(_ef) {
        float lr = 160.0f / (float)_ef->zEndtime;
        _set_tool->ic = ca & 0x00ffffff;
        GN::ValueChangeColor(&_set_tool->ic, ca, 0, 0, 0.5f, lr * 0.36f);
    } else _set_tool->ic = ca;
}
//对话框设置工具提示信息状态更新(变量设置工具集, 动画效果框, 不透明度色, 提示时间)
void GT_DlgSetToolInfoStateUpdate(GT_sVALUE_SET_TOOL *_set_tool, const GNg::cFRAME_RC *_ef, unsigned int ca, unsigned int t) {
    if(t > 1600) { //显示到时间要结束了
        if(_ef) {
            if((t - 1600) <= (unsigned int)_ef->zEndtime) {
                _set_tool->ic = ((_ef->zEndtime - (t - 1600)) * ((ca & 0xff000000) >> 24) / _ef->zEndtime << 24) | (_set_tool->ic & 0x00ffffff);
            } else {
                _set_tool->info[0] = 0;
            }
        } else _set_tool->info[0] = 0;
    }
}
//按宽高来构建矩形(矩形位置X , 位置Y, 矩形宽, 矩形高)
#define GNg_RectMakeFromWH(RC, X, Y, W, H) \
    {                                      \
        RC.left = X;                       \
        RC.top = Y;                        \
        RC.right = W;                      \
        RC.bottom = H;                     \
    }
GNu::cSIM_DIALOGEX *GT_FullSelectPathDlg = 0;

//绘制模糊屏幕图(坐标X, Y, 宽, 高, 颜色, 模糊级别与重复铺盖标记与混合模式, 蒙版图)
void GT_DrawBlurScreenImg(short _x, short _y, short _w, short _h, unsigned int color, unsigned short blmode, GNg::sIMAGE *img) {
    GNu::cUI_SCENE::zNeedScreenImgObjN++;
    uint8 blv = blmode >> 8;
    if(img) GNg::ImageDraw9Square(img, _x, _y, _w, _h, 0xffffffff, GNg::eBLEND_MODULATE, blv & 3);
    blv >>= 2;
    const float dithers[] = {0.0f, 0.0125f, 0.03f, 0.06f, 0.1f};
    const unsigned short dns[] = {1, 3, 8, 12, 12};
    if(GNg::cVIEW2D::screenImg) {
        GNg::ViewCur->ScissorRectSetup(_x, _y, _w, _h);
        GNg::ImageDrawDitherBlurProjToViewRng(GNg::cVIEW2D::screenImg, _x, _y, _w, _h, GNg::BlendColorRatio(color, 0xffffffff), 0, (blmode & 0xff), &GNg::View, dns[blv], dithers[blv], true);
        GNg::ViewCur->ScissorRectRemove();
    }
}
void GNu::cUI_SIM_DIALOG::GetDrawArea(GNg::sDRAW_AREA2 &out, cUI_GROUP *parent) {
    out.x = zDlgx;
    out.y = zDlgy;
    out.w = zDlgw;
    out.h = zDlgh;
    out.c = parent ? GNg_MultiplyColorRatio(c, parent->gc) : c;
}
void GNu::cUI_SIM_DIALOG::UpdateRun(unsigned int updatetime, sUI_LEVEL *parent) {
    if(!zState) {
        w = 0;
        h = 0;
        return;
    } else if(zState == 2 || zState == 8) {
        w = zDlgw;
        h = zDlgh;
    }

    void (*back_draw_proc_bk)(short, short, short, short, unsigned int, unsigned short, GNg::sIMAGE *) = zBackDrawProc;
    unsigned short blend_bk = aBackBlend;
    if(aBackBlurLv) {
        zBackDrawProc = GT_DrawBlurScreenImg;
        aBackBlend = (aBackBlurLv << 10) | (((uint8)zBackFrameRepeat | 2) << 8) | (zBackFrame ? GNg::eBLEND_DST_ALPHA_MASK : aBackBlend);
    }
    short last_keys[5];
    if(RecvInputUiObject != this) {
        memcpy(last_keys, &zKeyOK, 10);
        memset(&zKeyOK, 0, 10);
        GNg::ViewCur->mouseMX += 10000;
    }
    short dx, dy;
    if(zEffect) {
        GNg::sTRANSF2D &fts = zEffect->aFrame_ts;
        float s_x = fts.x, s_y = fts.y;
        c = zEffect->aColor;
        zEffect->aColor = DrawBeginPosGet(dx, dy, parent);
        GNg::sRECT2 *_rcloc = &(zEffect->zRcLoc);
        fts.x += (float)dx - (_rcloc->left * fts.sx + zT_ef_x);
        fts.y += (float)dy - (_rcloc->top * fts.sy + zT_ef_y);
        cSIM_DIALOGEX::UpdateRun(updatetime);
        zEffect->aColor = c;
        fts.x = s_x;
        fts.y = s_y;
    } else {
        DrawBeginPosGet(dx, dy, parent);
        zDlgx += dx - (zDlgx - zDlaOffsetX);
        zDlgy += dy - (zDlgy - zDlaOffsetY);
        cSIM_DIALOGEX::UpdateRun(updatetime);
    }
    if(RecvInputUiObject == this) {
        if(zDetailSetState == eDETAIL_SET_PATH_SELECTF) GT_FullSelectPathDlg = (cSIM_DIALOGEX *)this;
        else if(zSelect >= zChildNum && zState == 2 && zDetailSetState == eDETAIL_SET_NULL) {
            if(GN::Key1[zKeyOK]) UiActionCall(this, zSelect - zChildNum + 1);
            else if(GN::Mouse1[GM_LEFT] && (unsigned int)zSelect < zTmpRangeRect.Num()) {
                GNg::sRECT *_rc = zTmpRangeRect + zSelect;
                if(GNg::ViewCur->HitRange(_rc->left, _rc->top, _rc->right, _rc->bottom))
                    UiActionCall(this, zSelect - zChildNum + 1);
            }
        }
    } else {
        GNg::ViewCur->mouseMX -= 10000;
        memcpy(&zKeyOK, last_keys, 10);
    }
    if(!zState) UiActionCall(this, eUI_ACT_VANISH);
    if(aBackBlurLv) {
        zBackDrawProc = back_draw_proc_bk;
        aBackBlend = blend_bk;
    }
}
GNu::sSIM_MENU *GNu::cSIM_DIALOG::UpdateRun(unsigned int updatatime) {
    if(!zShowReady || !zState) return 0; //未准备好就返回
    unsigned int capt_color, text_color, back_color, line_color, focus_color, arrow_color, icon_color;
    bool can_show = true;
    if(zEffect) { // 有动画的处理 -----------------------------------------------------------------------------
        if(zState == 8) { //动画筐需要重设一些属性
            GNg::sRECT2 *_rcloc = &(zEffect->zRcLoc);
            GNg::sIMAGE *_ef_img = zEffect->zPlay_obj;
            _rcloc->left = zT_effect_rect.left * (float)zDlgw / (float)_ef_img->width;
            _rcloc->right = zT_effect_rect.right * (float)zDlgw / (float)_ef_img->width;
            _rcloc->top = zT_effect_rect.top * (float)zDlgh / (float)_ef_img->height;
            _rcloc->bottom = zT_effect_rect.bottom * (float)zDlgh / (float)_ef_img->height;
            zEffect->ReSetPlaytime();
            zState = 1;
        } else if(zState == 3) {
            zNowtime -= updatatime;
            if(zNowtime > 0xffff0000) {
                zTargetMenu = 0;
                zState = 0;
                return 0;
            }
        } else {
            if(zDetailSetState == eDETAIL_SET_PATH_SELECTF) {
                if((int)zNowtime - (int)updatatime > 0) zNowtime -= updatatime;
                else {
                    zNowtime = 0;
                    can_show = false;
                }
            } else zNowtime += (updatatime < zEffect->zEndtime) ? updatatime : ((zEffect->zEndtime / 3) << 1);
        }
        zEffect->SetRuntime(&zNowtime);
        if(zBackFrame) {
            unsigned short blmode_bk = zEffect->aBlendmode;
            zEffect->aBlendmode = 0; //使动画筐底图不显示
            zEffect->PlayFrame(0); //运行动画并计算出位置和颜色信息
            zEffect->aBlendmode = blmode_bk;
        } else zEffect->PlayFrame(0);
        if(zNowtime < zEffect->zEndtime) {
            zDlgx = (short)zEffect->aLevel_range.left;
            zDlgy = (short)zEffect->aLevel_range.top; //把效果筐的位置传给对话筐
            if(zDetailSetState == eDETAIL_SET_PATH_SELECTF)
                zStatisticsDlgSize(1.0f, 1.0f); //计算字距离和格位置对话筐大小和位置等信息
            else zStatisticsDlgSize(zEffect->aLevel_ts.sx, zEffect->aLevel_ts.sy);
        } else if(zState != 2) {
            zStatisticsDlgSize(zEffect->aLevel_ts.sx, zEffect->aLevel_ts.sy);
            zState = 2;
        }
        if(aAffectColor) { //需混和颜色
            icon_color = zEffect->aLevel_color;
            capt_color = GNg_MultiplyColorRatio(aCaptextColor, icon_color);
            text_color = GNg_MultiplyColorRatio(aTextColor, icon_color);
            line_color = GNg_MultiplyColorRatio(aLineColor, icon_color);
            focus_color = GNg::MultiplyColorRatio(aFocusColor, icon_color);
            back_color = GNg::MultiplyColorRatio(aBackColor, icon_color);
        } else goto GT_GOTO_SimdlgDrawColorGet;
    } else {
        if(zDetailSetState == eDETAIL_SET_PATH_SELECTF) can_show = false;

#define GT_SIM_DIALOG_DrawColorReset() \
    capt_color = aCaptextColor;        \
    text_color = aTextColor;           \
    line_color = aLineColor;           \
    focus_color = aFocusColor

    GT_GOTO_SimdlgDrawColorGet:
        GT_SIM_DIALOG_DrawColorReset();
        back_color = aBackColor;
        icon_color = 0xffffffff;
    }
    arrow_color = zUsGenArrowMap ? line_color : 0xffffffff;

    /* ------------------------------------------------------------------ 显示内容 -------------------------------------------------------------------------------------------------- */
    GT_TMP_SIM_DIALOG_DRAW_COMPARTENT_CPT //计算好绘制间距
        short iw2 = zIconW2 ? zIconW2 + ti : 0;
    short sel_num = zChildNum + zTargetType;
    if((short)zTmpRangeRect.Num() != sel_num + 1) zTmpRangeRect.New(sel_num + 1);
    GNg::sRECT *sel_rect = zTmpRangeRect; //选择判断范围数据 [主内容数]+[功能按钮]+[对话筐标题] !!为减少算量 这里.right和.bottom保存的是宽和高 并非右下坐标点
    if(can_show && (back_color & 0xff000000)) {
        if(zBackDrawProc) zBackDrawProc(zDlgx, zDlgy, zDlgw, zDlgh, back_color, aBackBlend, zBackFrame); //自定义底图
        else {
            if(zBackFrame) GNg::ImageDraw9Square(zBackFrame, zDlgx, zDlgy, zDlgw, zDlgh, back_color, aBackBlend, zBackFrameRepeat); //显示9宫格底图
            else if(!zEffect || zEffect->zPlay_obj == &GNg::Image) GNg::ImageDraw(0, zDlgx, zDlgy, zDlgw, zDlgh, back_color, 0, aBackBlend); //显示底色
        }
    }
    short draw_x, draw_y, draw_w, dx, dw, dxf; //用于记录当前绘制焦点的位置
    draw_x = zDlgx + zBorderW;
    draw_y = zDlgy + zBorderW;
    draw_w = zDlgw - (zBorderW << 1); //焦点缩到内边筐
    GNg_RectMakeFromWH(sel_rect[sel_num], draw_x, draw_y, draw_w, tj2); //标题栏判断范围
    if(can_show) {
        if(aLineBlend) {
            GNg::ImageDraw(zBackCaption, draw_x, draw_y, draw_w, tj2, line_color, 0, aLineBlend); //显示标题栏
            GNg::LineFrame2dDraw(zDlgx, zDlgy, zDlgw, zDlgh, aLineWidth, line_color, aLineBlend); //显示边筐
        } else if(zBackCaption) GNg::ImageDraw(zBackCaption, draw_x + aLineWidth, draw_y + aLineWidth, draw_w - (aLineWidth << 1), tj2 - (aLineWidth << 1), line_color, 0, aLineBlend ? aLineBlend : 1); //显示标题栏
    }
    draw_x += ti;
    draw_w -= ti2; //焦点缩到显示标题范围
    if(can_show) {
        if(zTargetIcon) GNg::ImageDraw(zTargetIcon, draw_x, draw_y, zIconW2, tj2, icon_color);
        zTextOut.SetText(zTargetCaption, 0, draw_x + iw2, draw_y + aCaptextOfsY, draw_w - iw2, tj2);
        zTextOut.Show(capt_color, aTextBlend); //显示标题
    }
    draw_x += ti;
    draw_y += (tj2 + tj_); //焦点缩到显示文本内容范围
    draw_w -= ti2;
    dxf = dx = draw_x + ti;
    dw = draw_w - ti2;
    dx += (short)((float)(dw - zSubValueW) * tfmt.near_pointU); //这个焦点范围是用于显示变量栏的

    GT_TMP_CHILD_DRAW_POSX_CPT //计算子选项各个内容显示位置X
        short vex = va2x + ti + ti + (ti >> 1); //附加注息显示位置X
    short text_in_w = va2x - va1x; //输入文本筐宽
    short th_ofs = (tj - tj_) >> 1; //输入文本筐显示高度相对该项显示高度的偏移量
    short arr_w = (short)((float)zArrowMap->width * (float)ti / (float)zTi_compartment); //计算经比例变换后的箭头宽
    short arr_h = (short)((float)zArrowMap->height * (float)ti / (float)zTi_compartment); //计算经比例变换后的箭头高
    short ah_ofs = (tj - arr_h) >> 1; //箭头显示高度偏移
    short ch_ofs = (tj - tfmt.height) >> 1; //颜色格显示高度偏移

#ifdef GT_TMP_COMPATI_DX_DDRAW
    bool has3Daccel = GN_Has3DAccelerate();
    if(!has3Daccel) { //在底下加多一层显示以弥补 DirectDraw 没透明混合表现的不足
        if(can_show && zDrawBackFrame.right) {
            if(zSelectLockID >= 30000)
                GNg::LineFrame2dDraw(zDrawBackFrame.left, zDrawBackFrame.top, zDrawBackFrame.right, zDrawBackFrame.bottom, aLineWidth, focus_color, aFocusBlend);
            else GNg::ImageDraw(0, zDrawBackFrame.left, zDrawBackFrame.top, zDrawBackFrame.right, zDrawBackFrame.bottom, focus_color, 0, aFocusBlend);
        }
    }
#define GT_SIM_DIALOG_DrawFocus(IMG, X, Y, W, H, C, B)                                                                            \
    if(has3Daccel) {                                                                                                              \
        GNg::ImageDraw(IMG, X, Y, W, H, C, 0, B);                                                                                 \
    } else {                                                                                                                      \
        zDrawBackFrame.left = (int)X, zDrawBackFrame.top = (int)Y, zDrawBackFrame.right = (int)W, zDrawBackFrame.bottom = (int)H; \
    }
#else
#define GT_SIM_DIALOG_DrawFocus(IMG, X, Y, W, H, C, B) \
    GNg::ImageDraw(IMG, X, Y, W, H, C, 0, B);
#endif
    GT_sVALUE_SET_TOOL *_set_tool = zSetTool;
    bool show_input_err = (_set_tool && _set_tool->info[0]);
    bool show_chd = can_show && zDetailSetState != eDETAIL_SET_TEXT_INPUT && zDetailSetState != eDETAIL_SET_COLOR_SELECT && zDetailSetState != eDETAIL_SET_PATH_SELECT && zDetailSetState != eDETAIL_SET_PATH_SELECTF;

    GT_sSP_CHILD *_chd;
    unsigned char chd_type;
    char v_str[256];
    short hi = -1, si = 0;
    if(show_input_err) hi = _set_tool->is; //确定是出错项的提示信息遮挡到的一项 不需要显示

    for(; si < zChildNum; ++si) { // -------------- 显示选单内各项 -------------------------------------------------------------------------------
        _chd = &(zTargetMenu->zChild[si]);
        chd_type = _chd->type;
        if(chd_type <= GT_SP_VALUE_FLOAT) {
            if(show_chd && si != hi && (si != zSelect || zDetailSetState != eDETAIL_SET_VALUE_SELECT)) {
                if(_chd->icon) GNg::ImageDraw(_chd->icon, icx, draw_y, zIconW, tj, icon_color);
                zTextOut.SetText(_chd->name, 0, vnx, draw_y, zNameW, tj);
                zTextOut.Show(text_color, aTextBlend); //显示变量名
                if(chd_type >= GT_SP_VALUE_COLOR) {
                    GNg::ImageDraw(zArrowMap, va1x, draw_y + ah_ofs, -arr_w, arr_h, arrow_color, 0, 1); //显示左箭头
                    char *e_name = _chd->enumName;
                    short *e_ids = _chd->enumIds;
                    if(e_ids) { //有列举'|'分割 的处理 --------------------------------------------------------------------------------
                        if(_chd->addValue) { //单选变量显示
                            int value = *((int *)(_chd->pointer));
                            int mx_value = (int)_chd->maxValue;
                            if(value > mx_value) value = mx_value;
                            value -= (int)(_chd->minValue); //使 value 表示为列举项ID
                            short en_id = e_ids[value] + 1;
                            zTextOut.SetText(e_name + en_id, (unsigned short)(e_ids[value + 1] - en_id), vvx, draw_y, zValueW, tj);
                        } else { //多选变量显示
                            int value = (int)_chd->minValue; //多选模式最小值记下了当前有选的最低位位置
                            if(value) { //并在筐内只显示一个最低位的列举名 其它都选中的没地方将不显示了
                                value -= 1;
                                short en_id = e_ids[value] + 1;
                                zTextOut.SetText(e_name + en_id, (unsigned short)(e_ids[value + 1] - en_id), vvx, draw_y, zValueW, tj);
                            } else zTextOut.SetText("-", 0, vvx, draw_y, zValueW, tj); //如没有选到东西项就显示为"-"
                        }
                        zTextOut.Show(text_color, aTextBlend); //显示列举变量值
                        e_name = 0;
                    } else {
                        if(chd_type == GT_SP_VALUE_COLOR) //正常颜色值的处理
                            GNg::ImageDraw(0, vvx, draw_y + ch_ofs, zValueW, tfmt.height, *((unsigned int *)_chd->pointer));
                        else if(si != zSelect || zDetailSetState != eDETAIL_SET_VALUE_INPUT) { //正常数据值的处理
                            if(chd_type == GT_SP_VALUE_INT) sprintf(v_str, "%d", *((int *)(_chd->pointer)));
                            else sprintf(v_str, "%.4f", *((float *)(_chd->pointer)));
                            zTextOut.SetText(v_str, 0, vvx, draw_y, zValueW, tj);
                            zTextOut.Show(text_color, aTextBlend); //显示变量值
                        }
                    } // -------------------------------------------------------------------------------------------------------------
                    GNg::ImageDraw(zArrowMap, va2x, draw_y + ah_ofs, arr_w, arr_h, arrow_color, 0, 1); //显示右箭头
                    if(zSelectLockID == si) { //该变量项有锁定时箭头亮起
                        GNg::ImageDraw(zArrowMap, va2x, draw_y + ah_ofs, arr_w, arr_h, 0xaaffbb88, 0, 2);
                        GNg::ImageDraw(zArrowMap, va1x, draw_y + ah_ofs, -arr_w, arr_h, 0xaaffbb88, 0, 2);
                    }
                    if(e_name) {
                        zTextOut.SetText(e_name, 0, vex, draw_y, zEnumNameW, tj);
                        zTextOut.Show(text_color, aTextBlend); //显示附加注息
                    }
                } else if(chd_type == GT_SP_VALUE_TEXT) {
                    zTextOut.SetText((char *)_chd->enumIds, 0, va1x, draw_y, text_in_w, tj);
                    zTextOut.Show(text_color, aTextBlend); //显示变量值
                    GNg::LineFrame2dDraw((int)va1x - (arr_w >> 1), (int)(draw_y + th_ofs), (int)(text_in_w + arr_w), (int)tj_, (int)aLineWidth, line_color, aLineBlend ? aLineBlend : 1);
                }
            }
            GNg_RectMakeFromWH(sel_rect[si], dx - (ti >> 1), draw_y, dw + ti, tj);

        } else if(chd_type == GT_SP_MENU) {
            GNg_RectMakeFromWH(sel_rect[si], draw_x - (ti >> 1), draw_y, draw_w + ti, tj);
            if(show_chd && si != hi) {
                if(_chd->icon) GNg::ImageDraw(_chd->icon, draw_x, draw_y, zIconW, tj, icon_color);
                zTextOut.SetText(_chd->name, 0, draw_x + iw, draw_y, draw_w - iw, tj);
                zTextOut.Show(capt_color, aTextBlend); //显示选项名
            }
        } else { //GT_SP_TEXT
            GNg_RectMakeFromWH(sel_rect[si], draw_x - (ti >> 1), draw_y, 0, 0);
            if(show_chd && si != hi) {
                if(_chd->icon) GNg::ImageDraw(_chd->icon, draw_x, draw_y, zIconW, tj, icon_color);
                if(_chd->pointer) {
                    if(_chd->enumIdnum == GT_SP_VALUE_FLOAT)
                        sprintf(v_str, _chd->name, *((float *)_chd->pointer));
                    else sprintf(v_str, _chd->name, *((int *)_chd->pointer));
                    zTextOut.SetText(v_str, 0, draw_x + iw, draw_y, draw_w - iw, tj); //显示跟随变量的静态文本
                } else zTextOut.SetText(_chd->name, 0, draw_x + iw, draw_y, draw_w - iw, tj); //显示静态文本
                zTextOut.Show(capt_color, aTextBlend);
            }
        }
        draw_y += tj;
    } //---------------------------------------------------------------------------------------------------------------------------------------
    if(zTargetType) {
        char *_butn_name = zButtonName ? zButtonName : (char *)aFuntionButtonName;
        if(zTextOut.zFontMapFormat < GNg::eFONT_FREETYPE) _butn_name += 3 * 9;
        draw_y += tj_;
        dxf += (short)((float)(dw - (zFuncButtonW * zTargetType + ti2 * (zTargetType - 1))) * tfmt.near_pointU); //起始显示功能按钮的焦点范围
        short v_ofs = 0, sfi = zChildNum;
        for(si = 0; si < zTargetType; ++si, ++sfi, v_ofs += zFuncButtonW + ti2) {
            GNg_RectMakeFromWH(sel_rect[sfi], dxf + v_ofs - (ti >> 1), draw_y, zFuncButtonW + ti, tj_);
            if(can_show) {
                zTextOut.SetText(_butn_name + si * 9, 0, dxf + v_ofs, draw_y, zFuncButtonW, tj_);
                zTextOut.Show(text_color, aTextBlend); //显示功能选择项
            }
        }
    } //---------------------------------------------------------------------------------------------------------------------------------------
    if(zState != 2) return zTargetMenu;

#ifdef GT_TMP_COMPATI_DX_DDRAW
    if(has3Daccel)
#endif
    {
        if(zFocusFlash && zSelectLockID >= 30000) { //处理焦点自动来回闪动
            if(zfAm_s_e) zfNowtime -= updatatime;
            else zfNowtime += updatatime;
            if(zfNowtime > zFocusFlash) {
                zfAm_s_e = 1;
                zfNowtime = zFocusFlash;
            } else if(zfNowtime < 0) {
                zfAm_s_e = 0;
                zfNowtime = 0;
            }
            focus_color = GNg::LinearColorMultiplyRatio(0x50ffffff, 0xffffffff, zfNowtime, zFocusFlash, focus_color);
        }
    }
    GNg::cVIEW2D *view = GNg::ViewCur;
    int mouse_mx = view->mouseMX, mouse_my = view->mouseMY;
    bool setting = false, _mouse_cp = GN::Mouse1[GM_LEFT],
         key_ok = GN::Key1[zKeyOK], key_up = GN::Key1[zKeyUP], key_down = GN::Key1[zKeyDOWN], key_left = GN::Key1[zKeyLEFT], key_right = GN::Key1[zKeyRIGHT];

    zIsMouseFocus = false;
    if(zDetailSetState > eDETAIL_SET_NULL) { //详细修改变量 -------------------------------------------------------------------------------------
        zIsMouseFocus = (zMouseCPx >= zDlgx && zMouseCPx <= zDlgx + zDlgw && zMouseCPy >= zDlgy && zMouseCPy <= zDlgy + zDlgh);
        GNu::cTEXT_INPUT *_text_in = _set_tool->text_in;
        if(zDetailSetState == eDETAIL_SET_VALUE_INPUT) { //变量值输入处理
            GT_SIM_DIALOG_DrawFocus(zFocusMap, _set_tool->fx, _set_tool->fy, _set_tool->fw, _set_tool->fh, focus_color, aFocusBlend)
                _text_in->UpdateRun(updatatime); //运行文本输入来接收用户输入
            if(!_set_tool->linear_ing) {
                if(key_ok || (_mouse_cp && !_set_tool->HitTest(mouse_mx, mouse_my))) {
                    char *value_str = _text_in->GetTextData(); //确定输入好就获取数据并赋值
                    if(value_str) {
                        _chd = &(zTargetMenu->zChild[zSelect]);
                        bool is_out_range = false;
                        if(_chd->type == GT_SP_VALUE_FLOAT) {
                            float valf = (float)atof(value_str);
                            if(valf > _chd->maxValue) {
                                valf = _chd->maxValue;
                                is_out_range = true;
                            } //超出浮点范围检测
                            else if(valf < _chd->minValue) {
                                valf = _chd->minValue;
                                is_out_range = true;
                            }
                            *((float *)(_chd->pointer)) = valf; //浮点赋值
                        } else {
                            int vali = atoi(value_str);
                            if(vali > (int)_chd->maxValue) {
                                vali = (int)_chd->maxValue;
                                is_out_range = true;
                            } //超出整型范围检测
                            else if(vali < (int)_chd->minValue) {
                                vali = (int)_chd->minValue;
                                is_out_range = true;
                            }
                            *((int *)(_chd->pointer)) = vali; //整型赋值
                        }
                        if(is_out_range) {
                            strncpy(_set_tool->info, zTextOut.zFontMapFormat < GNg::eFONT_FREETYPE ? " !Please enter: " : " ！请 输入: ", sizeof(_set_tool->info) - 1);
                            _set_tool->info[sizeof(_set_tool->info) - 1] = 0;
                            if(_chd->type == GT_SP_VALUE_FLOAT)
                                sprintf(_set_tool->info + strlen(_set_tool->info), "%.4f - %.4f", _chd->minValue, _chd->maxValue);
                            else
                                sprintf(_set_tool->info + strlen(_set_tool->info), "%d - %d", (int)_chd->minValue, (int)_chd->maxValue);
                            _set_tool->is = zSelect;
                            _set_tool->it = 0;
                            GT_DlgSetToolInfoStateReset(_set_tool, zEffect, capt_color);
                        }
                        delete[] value_str;
                        zDetailSetReturnLongLinear(_set_tool, sel_rect + zSelect);
                        if(zValueChangeProc) zValueChangeProc(zValueChangeArg);
                    }
                }
            }
        } else if(zDetailSetState == eDETAIL_SET_VALUE_SELECT) { //列举选择处理
            if(!zIsMouseFocus) zIsMouseFocus = _set_tool->HitTest(zMouseCPx, zMouseCPy);
            if(aLineBlend) GNg::LineFrame2dDraw(_set_tool->fx - zBorderW, _set_tool->fy - zBorderW, _set_tool->fw + (zBorderW << 1), _set_tool->fh + (zBorderW << 1), aLineWidth, line_color, aLineBlend); //显示边筐
            GT_SIM_DIALOG_DrawFocus(0, _set_tool->fx, _set_tool->fy, _set_tool->fw, _set_tool->fh, _set_tool->fc, aInputBackBlend) if(!_set_tool->linear_ing) {
                _set_tool->text_select->UpdateRun(updatatime); //运行列举文本列表给用户选取
                _chd = &(zTargetMenu->zChild[zSelect]);
                bool is_return = false;
                if(_chd->addValue) { //单选返回值
                    if(key_ok || GN::MouseI[GM_LEFT]) {
                        *((int *)_chd->pointer) = (_set_tool->text_select->aSelected - 1) + (int)_chd->minValue;
                        is_return = true;
                    }
                } else { //多选返回值
                    if(key_ok || (GN::MouseI[GM_LEFT] && !_set_tool->HitTest(mouse_mx, mouse_my))) {
                        unsigned int sel_enum = _set_tool->text_select->aSelected;
                        *((unsigned int *)_chd->pointer) = sel_enum;
                        GT_SIMPLE_MENU_SearchMinEnumValue(sel_enum, _chd->minValue);
                        is_return = true;
                    }
                }
                if(is_return) {
                    zDetailSetReturnLongLinear(_set_tool, sel_rect + zSelect);
                    if(zValueChangeProc) zValueChangeProc(zValueChangeArg);
                }
            }
        } else if(zDetailSetState == eDETAIL_SET_TEXT_INPUT || zDetailSetState == eDETAIL_SET_PATH_SELECT || zDetailSetState == eDETAIL_SET_PATH_SELECTF) { //文本输入处理
            if(zDetailSetState == eDETAIL_SET_PATH_SELECTF) {
                if(GN::ViewReSized) {
                    _set_tool->fx = view->ortho_left, _set_tool->fw = view->orthoW;
                    _set_tool->fy = view->ortho_top, _set_tool->fh = view->orthoH;
                    int fb_bar_h = tj + (ti >> 1);
                    _set_tool->file_browser->SetPos(_set_tool->fx + 3, _set_tool->fy + tj + ti, _set_tool->fw - ti, _set_tool->fh - tj - ti);
                    _set_tool->fb_select->SetPos(_set_tool->fx + ti2, _set_tool->fy + fb_bar_h, _set_tool->fw - (ti2 << 1), _set_tool->fh - fb_bar_h);
                }
                GT_SIM_DIALOG_DrawColorReset();
            }
            if(aLineBlend) GNg::LineFrame2dDraw(_set_tool->fx - zBorderW, _set_tool->fy - zBorderW, _set_tool->fw + (zBorderW << 1), _set_tool->fh + (zBorderW << 1), aLineWidth, line_color, aLineBlend); //显示边筐
            if(zBackDrawProc && zDetailSetState == eDETAIL_SET_PATH_SELECTF) {
                zBackDrawProc(_set_tool->fx, _set_tool->fy, _set_tool->fw, _set_tool->fh, _set_tool->fc, aInputBackBlend, zBackFrame);
            } else {
                GT_SIM_DIALOG_DrawFocus(0, _set_tool->fx, _set_tool->fy, _set_tool->fw, _set_tool->fh, _set_tool->fc, aInputBackBlend)
            }
            if(!_set_tool->linear_ing) {
#define GT_SIM_DIALOG_TextInColorChange() /*使显示文本渐渐淡入*/                                                                             \
    if(zEffect) {                                                                                                                            \
        _set_tool->it += updatatime;                                                                                                         \
        if(_set_tool->it >= (unsigned int)zEffect->zEndtime) _set_tool->it = (unsigned int)zEffect->zEndtime;                                \
        _text_in->aTextColor = ((unsigned int)((float)_set_tool->it / (float)zEffect->zEndtime * 255.0f) << 24) | (capt_color & 0x00ffffff); \
    } else _text_in->aTextColor = capt_color

                GT_SIM_DIALOG_TextInColorChange();

                _chd = &(zTargetMenu->zChild[zSelect]);
                bool chd_is_path = (zDetailSetState == eDETAIL_SET_PATH_SELECT || zDetailSetState == eDETAIL_SET_PATH_SELECTF), set_text_ok = false;
                GNu::cFILE_BROWSER *fb = _set_tool->file_browser;
                if(chd_is_path) {
                    if(_set_tool->fb_show) {
                        switch(fb->UpdateRun(updatatime)) { //文件浏缆更新显示
                            case 1:
                                if(!fb->aDisableControl && !fb->IsBusy()) set_text_ok = true;
                                break;
                            case 2:
                                if(fb->aDisableControl) _set_tool->fb_show = false;
                                break;
                        }
                    }
                    P<P<char>> sel_strs;
                    if(fb->aDisableControl) {
                        if(!_set_tool->fb_select->UpdateRun(updatatime)) fb->aDisableControl = false; //更新显示选择目录路径列表等

                        if(GN::MouseI[GM_LEFT] && !_set_tool->fb_show) {
                            unsigned int dir_sid = (unsigned int)_set_tool->fb_select->aSelected;
                            if(dir_sid) {
                                dir_sid -= 1;
                                sel_strs = _set_tool->fb_select->GetLables();
                                if(sel_strs == _set_tool->dir_strs) {
                                    if(dir_sid < _set_tool->dir_strs.Num()) {
                                        fb->SetOpenDirectory(_set_tool->dir_strs[dir_sid]);
                                        dir_sid = 0xffffffff;
                                    }
                                } else if(sel_strs == _set_tool->fts_strs) {
                                    if(dir_sid < _set_tool->fts_strs.Num()) {
                                        GNu::eFILE_SORT_TYPE fb_sortmode = (GNu::eFILE_SORT_TYPE)((unsigned int)GNu::eFILE_SORT_NAME + dir_sid);
                                        if(fb->GetSortMode() != fb_sortmode) {
                                            fb->SetSortMode(fb_sortmode);
                                            dir_sid = 0xffffffff;
                                        }
                                    }
                                } else if(sel_strs == _set_tool->ftf_strs) {
                                    if(dir_sid < _set_tool->ftf_strs.Num()) {
                                        if(strcmp(_set_tool->ftf_strs[dir_sid], "*")) fb->aFileTypeFilterStr = _set_tool->ftf_strs[dir_sid];
                                        else fb->aFileTypeFilterStr.Delete();
                                        fb->SetOpenDirectory(fb->aOpenDir);
                                        dir_sid = 0xffffffff;
                                    }
                                }
                            }
                            if(zEffect) {
                                _set_tool->fb_select->OkHide();
                                fb->RePop();
                            } else fb->aDisableControl = 0;
                            _set_tool->fb_show = 1;
                            _mouse_cp = 0;
                        }
                    }
                    int fb_bar_h = tj + (ti >> 1);
                    unsigned int fb_ca = _text_in->aTextColor | 0x00ffffff;
                    unsigned int fb_bc = GNg_MultiplyColorRatio(text_color, fb_ca);
                    GNg::ImageDraw(0, _set_tool->fx, _set_tool->fy, _set_tool->fw, fb_bar_h, GNg::MultiplyColorRatio(line_color, fb_ca), 0, aLineBlend ? aLineBlend : 1);

                    bool need_new_item = _chd->name[0] != '+' ? true : false;
                    const char *fb_buttons[4] = {"[ ok ]", "[ A ]", need_new_item ? "[                ]" : "[ * ]", "[ = ]"};
                    int fb_dx = (int)(_set_tool->fx + ti), fb_dy = (int)(_set_tool->fy + (ti >> 1));
                    int fb_bi = zSetPathType == eSET_PATH_SELECTF ? 0 : 1, fb_bl, fb_dx2, fb_bl2;
                    bool in_fb_btn_row = (!fb->aDisableControl && (short)mouse_my >= _set_tool->fy && (short)mouse_my <= _set_tool->fy + fb_bar_h);
                    for(; fb_bi < 4; ++fb_bi, fb_dx += ti + fb_bl) {
                        fb_bl = (strlen(fb_buttons[fb_bi]) + 1) * ti;
                        if(need_new_item && fb_bi == 2) {
                            fb_dx2 = fb_dx + ti;
                            fb_bl2 = fb_bl;
                        }
                        zTextOut.SetText((char *)fb_buttons[fb_bi], 0, fb_dx, fb_dy, fb_bl, tj);
                        if(in_fb_btn_row && mouse_mx >= fb_dx && mouse_mx <= fb_dx + fb_bl) {
                            if(_mouse_cp) {
                                _mouse_cp = false;
                                if(fb_bi) {
                                    if(zEffect) fb->OkHide();
                                    else _set_tool->fb_show = 0;

                                    if(need_new_item && fb_bi == 2)
                                        ;
                                    else {
                                        fb->aDisableControl = 1;
                                        switch(fb_bi) { //按选到的符号来弹出相应的选择列表
                                            case 1: sel_strs = _set_tool->fts_strs; break;
                                            case 2: sel_strs = _set_tool->ftf_strs; break;
                                            case 3: sel_strs = _set_tool->dir_strs; break;
                                        }
                                        _set_tool->fb_select->InitStyleToData(sel_strs, &zTextOut, capt_color, false,
                                                                              _set_tool->fx + ti2, _set_tool->fy + fb_bar_h, _set_tool->fw - (ti2 << 1), _set_tool->fh - fb_bar_h,
                                                                              false, 0, 0, focus_color, aFocusBlend);
                                    }
                                } else set_text_ok = 1;
                            }
                            zTextOut.Show(_text_in->aTextColor, _text_in->aTextBlend);
                        } else
                            zTextOut.Show(fb_bc, aTextBlend);
                    }
                    if(need_new_item) { //处理新建目录
                        bool last_is_inpt = _text_in->IsInputing();
                        short tinx, tiny, tinw, tinh;
                        _text_in->GetPos(&tinx, &tiny, &tinw, &tinh);
                        if(fb_dx2 != (int)tinx || fb_dy != (int)tiny || fb_bl2 != (int)tinw || tj != tinh) _text_in->SetPos(fb_dx2, fb_dy, fb_bl2, tj);
                        _text_in->UpdateRun(updatatime);

                        if(_mouse_cp && last_is_inpt && fb->IsMouseFocus()) {
                            char *new_dir_name = _text_in->GetTextData(); //输入确定好就获取数据并赋值
                            if(new_dir_name) {
                                char *_dir_name = new_dir_name;
                                while(*_dir_name) {
                                    if(GN_StrIsVisual(*_dir_name)) break;
                                    ++_dir_name;
                                }
                                if(*_dir_name) {
                                    char *pdstr = GN_StrHasPathDisableSymbol(_dir_name);
                                    if(pdstr) {
                                        _set_tool->is = 0;
                                        sprintf(_set_tool->info, "%s名不能包含这些符号 %s", _chd->name[0] == '>' ? "文档" : "目录", pdstr); //提示目录路径名不能包含的符号
                                        GT_DlgSetToolInfoStateReset(_set_tool, zEffect, capt_color);
                                    } else {
                                        bool egph = GNf::sFILE::EnableGlobalPath;
                                        GNf::sFILE::EnableGlobalPath = true;
                                        int fb_dir_len = strlen(fb->aOpenDir);
                                        if(_chd->name[0] == '>' && fb->aFileTypeFilterStr) {
                                            char *ftype_end = strchr(fb->aFileTypeFilterStr, ',');
                                            if(ftype_end) *ftype_end = 0;
                                            sprintf(fb->aOpenDir + fb_dir_len, "/%s.%s", _dir_name, fb->aFileTypeFilterStr.Begin());
                                            if(ftype_end) *ftype_end = ',';
                                        } else sprintf(fb->aOpenDir + fb_dir_len, "/%s", _dir_name);
                                        if(GNf::Access(fb->aOpenDir, 0)) {
                                            if(_chd->name[0] == '>') GNf::Close(GNf::Open(fb->aOpenDir, "wb"));
                                            else GNf::Mkdir(fb->aOpenDir);
                                        } else {
                                            _set_tool->is = 0;
                                            sprintf(_set_tool->info, "%s 已在该目录下存在", _dir_name); //目录已存在
                                            GT_DlgSetToolInfoStateReset(_set_tool, zEffect, capt_color);
                                        }
                                        fb->aOpenDir[fb_dir_len] = 0;
                                        GNf::sFILE::EnableGlobalPath = egph;
                                    }
                                }
                                delete[] new_dir_name;
                            }
                            if(zEffect) fb->RePop();
                            _set_tool->fb_show = true;
                            _mouse_cp = false;
                        }
                        if(_set_tool->info[0]) {
                            zTextOut.SetText(_set_tool->info, 0, fb_dx, fb_dy, _set_tool->fx + _set_tool->fw - fb_dx, tj);
                            zTextOut.Show(_set_tool->ic, aTextBlend); /*显示新建目录失败信息*/
                            GT_DlgSetToolInfoStateUpdate(_set_tool, zEffect, capt_color, _set_tool->is);
                            _set_tool->is += updatatime;
                            goto GT_GOTO_CheckInputReturnText;
                        }
                    }
                    zTextOut.SetText(fb->aOpenDir, 0, fb_dx, fb_dy, _set_tool->fx + _set_tool->fw - fb_dx, tj);
                    zTextOut.Show(_set_tool->fc, aInputBackBlend); //显示当前文件浏缆路径
                } else {
                    _text_in->UpdateRun(updatatime); //运行文本输入来接收用户输入的文本

                    if(zTextOut.zFontMapFormat < GNg::eFONT_FREETYPE)
                        sprintf(v_str, " Please enter the text within %d", (int)_chd->maxValue);
                    else
                        sprintf(v_str, " 请输入文字%d个以内", (int)_chd->maxValue >> 1);

                    zTextOut.SetText(v_str, 0, _set_tool->fx, _set_tool->fy + _set_tool->fh, _set_tool->fw, tj);
                    zTextOut.Show(_text_in->aTextColor, _text_in->aTextBlend); //显示可输入的个数提示
                }
            GT_GOTO_CheckInputReturnText:
                if(set_text_ok || (_mouse_cp && !_set_tool->HitTest(mouse_mx, mouse_my))) {
                    if(chd_is_path) {
                        if(!fb->aDisableControl && !fb->IsBusy()) {
                            P<P<GNf::sFILE_INFO>> sf_info = fb->GetSelectFileAndDirInfoData();
                            if(sf_info) {
                                // TODO: verify buffer size
                                strcpy((char *)_chd->pointer, sf_info[0]->global_filename);
                                GT_ChildTextValueSetShowStr(_chd); //简短显示文本更新
                            } else if(_chd->name[0] == '=') {
                                // TODO: verify buffer size
                                strcpy((char *)_chd->pointer, fb->aOpenDir);
                                GT_ChildTextValueSetShowStr(_chd); //简短显示文本更新
                            }
                            if(zDetailSetState == eDETAIL_SET_PATH_SELECTF) zNowtime = 0;
                            set_text_ok = 1;
                        }
                    } else {
                        char *value_str = _text_in->GetTextData(); //输入确定好就获取数据
                        if(value_str) {
                            // TODO: verify buffer size
                            strcpy((char *)_chd->pointer, value_str); //复制编辑文本到修改文本缓冲里实现赋值
                            delete[] value_str;
                            GT_ChildTextValueSetShowStr(_chd); //简短显示文本更新
                            set_text_ok = 1;
                        }
                    }
                }
                if(set_text_ok) {
                    zDetailSetReturnRangeLinear(_set_tool, sel_rect + zSelect, focus_color);
                    if(zValueChangeProc) zValueChangeProc(zValueChangeArg);
                }
            } else _set_tool->it = 0;

        } else if(zDetailSetState == eDETAIL_SET_COLOR_SELECT) { //处理颜色选择
            _chd = &(zTargetMenu->zChild[zSelect]);
            cPALETTE *ple = _set_tool->palette;
            ple->aDlgx = _set_tool->fx + 1, ple->aDlgw = _set_tool->fw - 2; //范围缩小1像素
            ple->aDlgy = _set_tool->fy + 1, ple->aDlgh = _set_tool->fh - 2;
            ple->aDisableControl = _text_in->IsInputing();
            bool color_chg = ple->UpdateRun(updatatime);
            GNg::sCOLOR cur_color = ple->GetColor();

            if(aLineBlend) GNg::LineFrame2dDraw(_set_tool->fx - zBorderW, _set_tool->fy - zBorderW, _set_tool->fw + (zBorderW << 1), _set_tool->fh + (zBorderW << 1), aLineWidth, line_color, aLineBlend); //显示边筐
            if(color_chg) {
                if(cur_color.BGRA.a > (uint8)_chd->maxValue) {
                    cur_color.BGRA.a = (uint8)_chd->maxValue;
                    ple->SetColor(cur_color);
                } else if(cur_color.BGRA.a < (uint8)_chd->minValue) {
                    cur_color.BGRA.a = (uint8)_chd->minValue;
                    ple->SetColor(cur_color);
                }
                *((unsigned int *)_chd->pointer) = cur_color.COLOR;
                sprintf(v_str, "%08x", cur_color.COLOR);
                _text_in->InitInputFmtToData(GNu::eTX_IN_HEX, v_str, 8);
                if(zValueChangeProc) zValueChangeProc(zValueChangeArg);
            }
            if(!_set_tool->linear_ing) {
                GT_SIM_DIALOG_TextInColorChange();
                _text_in->UpdateRun(updatatime); //运行文本输入来接收用户颜色值输入

                if(!color_chg && GT_ImeCount && _text_in->IsInputing()) { //有颜色值输入更改
                    char *color_str = _text_in->GetTextData();
                    if(color_str) {
                        GNg::sCOLOR in_color = 0;
                        sscanf(color_str, "%x", &(in_color.COLOR));
                        if(strlen(color_str) < 7) {
                            in_color.BGRA.a = (uint8)_chd->maxValue; //当输入值没有指定透明值就认定是最大值
                        } else {
                            if(in_color.BGRA.a > (uint8)_chd->maxValue) in_color.BGRA.a = (uint8)_chd->maxValue;
                            else if(in_color.BGRA.a < (uint8)_chd->minValue) in_color.BGRA.a = (uint8)_chd->minValue;
                        }
                        delete[] color_str;

                        if(in_color != cur_color) {
                            ple->SetColor(in_color);
                            *((unsigned int *)_chd->pointer) = in_color.COLOR;
                            if(zValueChangeProc) zValueChangeProc(zValueChangeArg);
                        }
                    }
                }
                if(key_ok || GN::Mouse2[GM_LEFT] || (_mouse_cp && !_text_in->IsMouseFocus() && !_set_tool->HitTest(mouse_mx, mouse_my))) {
                    zDetailSetReturnRangeLinear(_set_tool, sel_rect + zSelect, cur_color.COLOR);
                }
            } else _set_tool->it = 0;

        } else if(zDetailSetState == 0xfe) { //返回值中途插值实现动画效果
            if(aLineBlend) GNg::LineFrame2dDraw(_set_tool->fx - zBorderW, _set_tool->fy - zBorderW, _set_tool->fw + (zBorderW << 1), _set_tool->fh + (zBorderW << 1), aLineWidth, line_color, aLineBlend); //显示边筐
            GT_SIM_DIALOG_DrawFocus(0, _set_tool->fx, _set_tool->fy, _set_tool->fw, _set_tool->fh, _set_tool->fc, aInputBackBlend) if(!_set_tool->linear_ing) zDetailSetState = eDETAIL_SET_NULL;

        } else if(zDetailSetState == 0xff) { //返回值中途插值实现动画效果
            GT_SIM_DIALOG_DrawFocus(zFocusMap, _set_tool->fx, _set_tool->fy, _set_tool->fw, _set_tool->fh, focus_color, aFocusBlend) if(!_set_tool->linear_ing) zDetailSetState = eDETAIL_SET_NULL;
        }
        return zTargetMenu;
    } //------------------------------------- 输入超出范围提示显示 -----------------------------------------------------
    if(show_input_err) {
        GNg::sRECT *cur_select = sel_rect + _set_tool->is;
        zTextOut.SetText(_set_tool->info, 0, cur_select->left, cur_select->top, cur_select->right, cur_select->bottom);
        zTextOut.Show(_set_tool->ic, aTextBlend); /*显示值出轨提示信息*/
        _set_tool->it += updatatime;
        GT_DlgSetToolInfoStateUpdate(_set_tool, zEffect, capt_color, _set_tool->it);
    } //-------------------------------------------------------------------------------------------------------------------
#ifdef GT_TMP_COMPATI_DX_DDRAW
    if(has3Daccel) {
#endif
        if(sel_rect[zSelect].right) { //显示选择焦点
            GNg::ImageDraw(zFocusMap, sel_rect[zSelect].left, sel_rect[zSelect].top, sel_rect[zSelect].right, sel_rect[zSelect].bottom, focus_color, 0, aFocusBlend);
        }
#ifdef GT_TMP_COMPATI_DX_DDRAW
    } else zDrawBackFrame = sel_rect[zSelect];
#endif
    /******************************************************** 处理鼠标选择 ************************************************************************/
    if(!GN::Mouse[GM_LEFT]) zMouseLeft = 0;
    else if(!zMouseLeft) zMouseLeft = _mouse_cp;
    if(_mouse_cp) {
        zMouseCPx = view->mouseMX;
        zMouseCPy = view->mouseMY;
    }
    if(zUsMouse) {
        bool _mouse_i = GN::MouseI[GM_LEFT];
        if(zMouseLeft) {
            mouse_mx = zMouseCPx;
            mouse_my = zMouseCPy;
        }
        zIsMouseFocus = GNg::TestPointIntRect(mouse_mx, mouse_my, zDlgx, zDlgy, zDlgw, zDlgh);

        if(GN::MouseM || zMouseLeft || _mouse_i) { //鼠标按下左键 或 鼠标移动控制选择 或按下立即松开
            if(zIsMouseFocus) { //鼠标指针在对话筐里
                short mi;
                GNg::sRECT *_rect;
                if(mouse_my < sel_rect[zChildNum - 1].top + sel_rect[zChildNum - 1].bottom) {
                    if(mouse_my > sel_rect[sel_num].top + sel_rect[sel_num].bottom) {
                        //在主内容区里
                        mi = (short)((mouse_my - sel_rect->top) / tj);
                        if(mi >= zChildNum) mi = zChildNum - 1;
                        _rect = sel_rect + mi;
                        if(zChildNum && GNg::TestPointIntRect(mouse_mx, mouse_my, _rect->left, _rect->top, _rect->right, _rect->bottom)) { //选中内容项范围里
                            zSelect = mi;
                            if(zMouseLeft || _mouse_i) {
                                chd_type = zTargetMenu->zChild[mi].type;
                                if(chd_type <= GT_SP_VALUE_FLOAT && mouse_my <= sel_rect[mi].top + tj && mouse_my >= sel_rect[mi].top) { //鼠标点中了修改变量项
                                    if(chd_type == GT_SP_VALUE_TEXT) {
                                        if(_mouse_i && mouse_mx <= va1x + text_in_w && mouse_mx >= va1x) {
                                            setting = zIsDetailSet;
                                            zMouseLeft = 0;
                                        } else {
                                            key_ok = 1;
                                            zMouseLeft = 0;
                                        }
                                    } else {
                                        if(mouse_mx >= vvx && mouse_mx <= vvx + zValueW) { //在该项的变量值筐里拖动修改数值
                                            if(GN::MouseMKS & GMM_LEFT) {
                                                if(view->mouseMSX < 0) key_left = 1;
                                                else if(view->mouseMSX > 0) key_right = 1;
                                            } else if(_mouse_i) {
                                                setting = zIsDetailSet;
                                                zMouseLeft = 0;
                                            } else key_ok = 1;
                                        } else if(!_mouse_i && mouse_mx <= va1x && mouse_mx >= va1x - arr_w) {
                                            key_left = 1;
                                            zMouseLeft = 0;
                                        } //点中左箭头
                                        else if(!_mouse_i && mouse_mx <= va2x + arr_w && mouse_mx >= va2x) {
                                            key_right = 1;
                                            zMouseLeft = 0;
                                        } //点中右箭头
                                        else
                                            key_ok = 1;
                                    }
                                } else {
                                    key_ok = 1;
                                    zMouseLeft = 0;
                                }
                            }
                        } else zMouseLeft = 0;
                    } else {
                        zMouseLeft = 0;
                        zIsMoveDlg = GN::MouseMKS & GMM_LEFT; //在标题区里
                    }
                } else { //在功能按钮区里
                    GNg::sRECT *end_rect = sel_rect + sel_num;
                    for(_rect = sel_rect + zChildNum; _rect < end_rect; ++_rect) {
                        if(GNg::TestPointIntRect(mouse_mx, mouse_my, _rect->left, _rect->top, _rect->right, _rect->bottom)) {
                            zSelect = _rect - sel_rect;
                            if(zMouseLeft) key_ok = 1;
                            break;
                        }
                    }
                    zMouseLeft = 0;
                }
            } //结束对话筐区域的判断
            if(zIsMoveDlg && !aSkipPos) { //在拖动对话筐
                zIsMouseFocus = true;
                zIsMoveDlg = GN::MouseMKS & GMM_LEFT;
                OffsetPos(view->mouseMSX, view->mouseMSY); //调整对话筐显示位置
            }
            zSelectLockID = 30000; //鼠标移动或点击时顺便解除选择锁定
        }
    }
    /*----------------------------------------- 选择音 -------------------------------------------------------*/
#ifdef _GN_SOUND_H_
    if(zSouObj) {
        if(zSelectLast != zSelect) {
            zSound.Play(0);
            zSelectLast = zSelect; //选择更变发音
        } else if(key_ok && !zEffect && !zMouseLeft)
            zSound.Play(0); //按了确定发音
    }
#define GT_TMP_SIM_DIALOG_SoundPlay \
    if(!zMouseLeft) zSound.Play(0);
#else
#define GT_TMP_SIM_DIALOG_SoundPlay
#endif
    /*--------------------------------------- 键盘控制选择 ---------------------------------------------------*/
    if(key_down && zSelectLockID >= zChildNum) {
        ++zSelect;
        if(zSelect >= sel_num) zSelect -= sel_num; //选择超出结尾就重新跳到开头处
        while(zSelect < zChildNum && zTargetMenu->zChild[zSelect].type == GT_SP_TEXT) ++zSelect; //检查选择点如在文本区就跳到下一项
    }
    if(key_up && zSelectLockID >= zChildNum) {
        --zSelect;
        while(zSelect < zChildNum && zSelect >= 0 && zTargetMenu->zChild[zSelect].type == GT_SP_TEXT) --zSelect; //检查选择点如在文本区就跳到下一项
        if(zSelect < 0) zSelect += sel_num;
    }
    if(zSelect < zChildNum) { //选择焦点在内容区内
        _chd = &(zTargetMenu->zChild[zSelect]);
        if(setting) { //双击进入详细设置前准备 ---------------------------------------------------------------------------------------------
            GT_TMP_SIM_DIALOG_SoundPlay cTEXT_INPUT *_text_in = 0;
            _set_tool->info[0] = 0;
            if(_chd->type == GT_SP_VALUE_INT || _chd->type == GT_SP_VALUE_FLOAT) {
                _set_tool->fy = sel_rect[zSelect].top;
                _set_tool->fh = sel_rect[zSelect].bottom;

                if(_chd->enumIds) { //详细修改列举变量
                    cSELECT_LIST_TEXT *text_list = _set_tool->text_select;
                    if(zFocusMap && !text_list->zFocusImg) {
                        P<GNg::sIMAGE> fimg(1);
                        *fimg = *zFocusMap;
                        text_list->SetEffect(zFocusFlash, P_(), fimg);
                    }
                    short fn_ = (short)(_chd->enumIdnum - 2); //------ 计算列举文本列表的显示位置
                    short fx_ = vvx - (fn_ >> 1) * zValueW - fn_, fw_ = fn_ * zValueW + (fn_ << 1);
                    if(fw_ > view->orthoW) {
                        fw_ = view->orthoW;
                        fx_ = view->ortho_left;
                    } else if(fx_ < view->ortho_left) fx_ = view->ortho_left;
                    else if(fx_ + fw_ > view->ortho_right) fx_ -= fx_ + fw_ - view->ortho_right;

                    text_list->InitStyleToData(GN_StrSplitToData(_chd->enumName, '|'),
                                               &zTextOut, capt_color, _chd->addValue ? false : true,
                                               fx_, _set_tool->fy, fw_, _set_tool->fh, zEffect ? true : false, zValueW, 0, focus_color, aFocusBlend);

                    if(_set_tool->ts_transpose) {
                        _set_tool->ts_transpose = false;
                        text_list->Transpose(); //在第首初始化文本选择列表时转向一下默认摆放方向
                    }
                    fn_ = tj_ / 6; //可控制焦点底大于选择列表量
                    fx_ -= (fn_ >> 1);
                    _set_tool->fy -= (fn_ >> 1);
                    fw_ += fn_;
                    _set_tool->fh += fn_;
                    if(zEffect) { //返回插值设置
                        float lr = 160.0f / (float)zEffect->zEndtime;
                        _set_tool->linear_ing = true;
                        _set_tool->fc = focus_color;
                        _set_tool->fx = (short)sel_rect[zSelect].left;
                        _set_tool->fw = (short)sel_rect[zSelect].right;
                        GN::ValueChange(&_set_tool->fx, fx_, 0, 0, 0.5f, lr);
                        GN::ValueChange(&_set_tool->fw, fw_, 0, &_set_tool->linear_ing, 0.5f, lr);
                        GN::ValueChangeColor(&_set_tool->fc, aInputBackColor, 0, 0, 0.5f, lr);
                    } else {
                        _set_tool->linear_ing = false;
                        _set_tool->fc = aInputBackColor;
                        _set_tool->fx = fx_;
                        _set_tool->fw = fw_;
                    }
                    zDetailSetState = eDETAIL_SET_VALUE_SELECT;
                    if(_chd->addValue) text_list->aSelected = (unsigned int)(*((int *)_chd->pointer) - (int)_chd->minValue + 1); //单选列举值
                    else text_list->aSelected = *((unsigned int *)_chd->pointer); //多选列举值
                } else {
                    _text_in = _set_tool->text_in;
                    if(_chd->type == GT_SP_VALUE_INT) { //详细修改整形值
                        sprintf(v_str, "%d", *((int *)(_chd->pointer)));
                        _text_in->InitInputFmtToData(eTX_IN_INT, v_str, 10);
                    } else { //详细修改浮点值
                        sprintf(v_str, "%.4f", *((float *)(_chd->pointer)));
                        _text_in->InitInputFmtToData(eTX_IN_FLOAT, v_str, 10);
                    }
                    if(zEffect) { //----------------------------------------------- 计算焦点插值位置
                        float lr = 160.0f / (float)zEffect->zEndtime;
                        _set_tool->linear_ing = true;
                        _set_tool->fx = sel_rect[zSelect].left;
                        _set_tool->fw = sel_rect[zSelect].right;
                        GN::ValueChange(&_set_tool->fx, vvx, 0, 0, 0.5f, lr);
                        GN::ValueChange(&_set_tool->fw, zValueW, 0, &_set_tool->linear_ing, 0.5f, lr);
                    } else {
                        _set_tool->linear_ing = false;
                        _set_tool->fx = vvx;
                        _set_tool->fw = zValueW;
                    }
                    zDetailSetState = eDETAIL_SET_VALUE_INPUT;
                    _text_in->SetPos(vvx, _set_tool->fy, zValueW, _set_tool->fh); //更新文本筐位置
                }
            } else if(_chd->type == GT_SP_VALUE_COLOR || _chd->type == GT_SP_VALUE_TEXT) {
                short f_fy = sel_rect->top; //内容区位置Y
                short f_fh = sel_rect[zChildNum - 1].top + sel_rect[zChildNum - 1].bottom - f_fy; //内容区总高

                if(_chd->type == GT_SP_VALUE_COLOR) { //详细修改颜色值
                    _text_in = _set_tool->text_in;
                    unsigned int &pcolor = *((unsigned int *)(_chd->pointer));
                    sprintf(v_str, "%08x", pcolor);
                    _text_in->InitInputFmtToData(eTX_IN_HEX, v_str, 8);
                    _text_in->SetPos(draw_x + ti, f_fy + f_fh, ti * 10, tj);
                    _set_tool->palette->SetOffsetEF(zEffect ? 0.22f : 1.0f);
                    _set_tool->palette->SetColor(pcolor);
                    zDetailSetState = eDETAIL_SET_COLOR_SELECT;

                } else { //详细修改文本内容
                    if(zSetPathType != eSET_PATH_INPUT && (_chd->name[0] == '+' || _chd->name[0] == '=' || _chd->name[0] == '>') && (int)_chd->maxValue == GN_MAXFILEPATH) { //判断子项是否为修改文件路径
                        if(zSetPathType == eSET_PATH_SELECTF) {
                            if(zEffect) {
#define GT_TMP_SIM_DIALOG_EffectTransTimeSet() \
    if(zEffect->zNodes) {                      \
        zEffect->SeekSegment(0);               \
        zNowtime = zEffect->zEndtime;          \
    } else zNowtime = zEffect->zEndtime > 1200 ? 1200 : zEffect->zEndtime

                                GT_TMP_SIM_DIALOG_EffectTransTimeSet();
                            }
                            draw_x = view->ortho_left, draw_w = view->orthoW; //把放大目标焦点拉到整个视口的大小
                            f_fy = view->ortho_top, f_fh = view->orthoH;
                        }
                        cFILE_BROWSER *_fb;
                        if(!_set_tool->file_browser) { //如文件刘缆还未准备好 就先处始化它
                            P<GNg::cFRAME_LAUNCHER_FONT> fam_ef1;
                            GNg::cFRAME *_fb_am = _set_tool->fb_am;
                            if(zEffect) {
                                GNg::sAM_EVENT *_efev = zEffect->zFEvent, *_fb_ev = _set_tool->fb_ev;
                                GNg::sAM_EVENT *efev_end = _efev + zEffect->zEventcount;
                                for(; _efev < efev_end; ++_efev) { //看原动画事件如有颜色变化事件 有就抽取颜色事件
                                    if(_efev->event_mode == GNg::eEVENT_COLOR) {
                                        _fb_ev[1] = *_efev;
                                        break;
                                    }
                                }
                                _fb_ev->Init(GNg::eEVENT_MOVE_X, 0, zEffect->zFEvent ? zEffect->zFEvent->segment_time : 260, (float)(-(tj << 2)), 0.0f, 0.0f, GNg::L_START);
                                _fb_am[2].Init(0, _efev == efev_end ? 1 : 2, _fb_ev GT_TMP_DEF_FRAME_INIT_ARGS_ONCE_KEEP);
                                fam_ef1.New(1);
                                GNg::sARRANGE_FMT fmt1(0, 0.0f, 0.0f, 1, 1, 0, 0, GNg::eENTER_LEFT_RIGHT);
                                fam_ef1->InitToData(_fb_am + 2, GNg::cFRAME_LAUNCHER_FONT::aPicIndex, 1, &GNg::Rect2D, &fmt1, 0.0f, 80, 1, 127, 0, 127, 127, 0, 0, 0, 0, true);

                                _fb_ev[2].Init(GNg::eEVENT_SIZE_X | GNg::eEVENT_SIZE_Y, 0, 120, 1.0f, 1.2f);
                                _fb_am[1].Init(0, 1, _fb_ev + 2 GT_TMP_DEF_FRAME_INIT_ARGS_ONCE_KEEP);
                                _set_tool->fb_ev_icon(zEffect->zFEvent, zEffect->zEventcount);
                                _fb_am->Init(cFILE_BROWSER::GetIcon(), _set_tool->fb_ev_icon.Num(), _set_tool->fb_ev_icon GT_TMP_DEF_FRAME_INIT_ARGS_ONCE_KEEP);
                            } else _fb_am->Init(cFILE_BROWSER::GetIcon(), 0, 0 GT_TMP_DEF_FRAME_INIT_ARGS_ONCE_KEEP);

                            short fb_iw = 0, fb_ih = 0;
                            if(zSetPathType == eSET_PATH_SELECT) {
                                fb_iw = tj;
                                fb_ih = tj;
                            }
                            GNg::sARRANGE_FMT afmt(0, 0.0f, 0.0f, ti * 22, ti, fb_iw, fb_ih, GNg::eENTER_TOP_BOTTOM, 1, 0);
                            P<GNg::cFRAME_LAUNCHER_FONT> fam_ef(1);
                            fam_ef->InitToData(_fb_am, GNg::cFRAME_LAUNCHER_FONT::aPicIndex, 1, &GNg::Rect2D, &afmt, 0.0f, 6, zEffect ? 1 : 0, 127, 0, 127, 127, 0, 0, 0, zEffect ? (_fb_am + 1) : 0, true);

                            P<GNg::sIMAGE> focus_img;
                            if(zFocusMap) {
                                focus_img.New(1);
                                *focus_img = *zFocusMap;
                            }
                            _set_tool->file_browser.New(1);
                            _fb = _set_tool->file_browser;
                            _fb->SetEffect(zFocusFlash, fam_ef, focus_img);
                            _fb->InitStyleToData(&zTextOut, _set_tool->dir_strs[_set_tool->fb_open_did < (uint8)_set_tool->dir_strs.Num() ? _set_tool->fb_open_did : 0],
                                                 draw_x + 3, f_fy + tj + ti, draw_w - ti, f_fh - tj - ti, ti * 20, 0, eNEAR_RIGHT, aTextColor, aFocusColor, aTextBlend, aFocusBlend);

                            _set_tool->fb_select.New(1);
                            _set_tool->fb_select->SetEffect(zFocusFlash, fam_ef1, focus_img);
                            if(zEffect) _set_tool->fb_select->aAffectColor = 1; //效果动画色可影响选择列表色

                        } else { //如再次打开使用文件浏览就简单重置一下即可
                            _fb = _set_tool->file_browser;
                            if(_set_tool->fb_open_did < (uint8)_set_tool->dir_strs.Num()) _fb->SetOpenDirectory(_set_tool->dir_strs[_set_tool->fb_open_did]);
                            _fb->SetPos(draw_x + 3, f_fy + tj + ti, draw_w - ti, f_fh - tj - ti);
                            _fb->RePop();
                            if(zEffect) _fb->UpdateRun(1000000);
                        }
                        _set_tool->fb_open_did = 0xff;
                        if(_chd->name[0] == '=') _fb->aFileTypeFilterStr("___", 0);
                        else {
                            char *pext_name = (char *)_chd->pointer;
                            if(*pext_name == '*' && pext_name[1] == '.') {
                                if(pext_name[2] == '*') _fb->aFileTypeFilterStr.Delete();
                                else _fb->aFileTypeFilterStr(pext_name + 2, 0);
                            }
                        }
                        if(_chd->name[0] != '+') _set_tool->text_in->InitInputFmtToData(eTX_IN_TEXT, _chd->name[0] == '>' ? "    新建文档" : "   新建文件夹", 16);
                        _fb->aDisableControl = 0;
                        _set_tool->fb_show = 1;
                        zDetailSetState = zSetPathType == eSET_PATH_SELECTF ? eDETAIL_SET_PATH_SELECTF : eDETAIL_SET_PATH_SELECT;
                    } else {
                        _text_in = _set_tool->text_in;
                        if(_chd->name[0] == '*') _text_in->InitInputFmtToData(eTX_IN_PASSWORD, (char *)_chd->pointer, (unsigned short)_chd->maxValue);
                        else _text_in->InitInputFmtToData(eTX_IN_TEXT, (char *)_chd->pointer, (unsigned short)_chd->maxValue);
                        _text_in->SetPos(draw_x, f_fy, draw_w, f_fh);
                        zDetailSetState = eDETAIL_SET_TEXT_INPUT;
                    }
                }
                if(zEffect) { //计算焦点插值显示位置 ------------------------------------------------------------------
                    _set_tool->linear_ing = true;
                    float lr = 160.0f / (float)zEffect->zEndtime;
                    _set_tool->fx = vvx;
                    _set_tool->fw = zValueW;
                    _set_tool->fy = (short)sel_rect[zSelect].top;
                    _set_tool->fh = (short)sel_rect[zSelect].bottom;
                    _set_tool->fc = (_chd->type == GT_SP_VALUE_TEXT) ? focus_color : (*((unsigned int *)_chd->pointer));
                    GN::ValueChange(&_set_tool->fx, draw_x, 0, 0, 0.5f, lr);
                    GN::ValueChange(&_set_tool->fw, draw_w, 0, &_set_tool->linear_ing, 0.5f, lr);
                    GN::ValueChange(&_set_tool->fy, f_fy, 0, 0, 0.5f, lr);
                    GN::ValueChange(&_set_tool->fh, f_fh, 0, 0, 0.5f, lr);
                    GN::ValueChangeColor(&_set_tool->fc, aInputBackColor, 0, 0, 0.5f, lr);
                } else {
                    _set_tool->linear_ing = false;
                    _set_tool->fy = f_fy;
                    _set_tool->fh = f_fh;
                    _set_tool->fx = draw_x;
                    _set_tool->fw = draw_w;
                    _set_tool->fc = aInputBackColor;
                }
            }
            if(_text_in) { //文本筐显示颜色同步
                _text_in->aTextColor = capt_color;
                _text_in->aTextBlend = aTextBlend;
                _text_in->aFocusColor = focus_color;
                _text_in->aFocusBlend = aFocusBlend;
            }
        } else { //------------------------------------ 定义一些修改变量的方法宏 ---------------------------------------
#define GT_TMP_ChildEnumAdd() /*列举值加*/                               \
    t_valuei = (int)(_chd->minValue);                                    \
    t_valuei += 1;                                                       \
    if(t_valuei > (int)_chd->maxValue) {                                 \
        if(aValueLoop) t_valuei = 0;                                     \
        else t_valuei = (int)_chd->maxValue;                             \
    }                                                                    \
    _chd->minValue = (float)t_valuei;                                    \
    if(t_valuei) *((unsigned int *)_chd->pointer) = 1 << (t_valuei - 1); \
    else *((unsigned int *)_chd->pointer) = 0

#define GT_TMP_ChildEnumSub() /*列举值减*/                               \
    t_valuei = (int)(_chd->minValue);                                    \
    t_valuei -= 1;                                                       \
    if(t_valuei < 0) {                                                   \
        if(aValueLoop) t_valuei = (int)_chd->maxValue;                   \
        else t_valuei = 0;                                               \
    }                                                                    \
    _chd->minValue = (float)t_valuei;                                    \
    if(t_valuei) *((unsigned int *)_chd->pointer) = 1 << (t_valuei - 1); \
    else *((unsigned int *)_chd->pointer) = 0

#define GT_TMP_ChildColorAdd() /*颜色透明值加*/           \
    vc = *((unsigned int *)_chd->pointer);                \
    _va = (int)_chd->addValue;                            \
    va = (int)_chd->maxValue;                             \
    if((int)vc.BGRA.a + _va > va) {                       \
        if(aValueLoop) vc.BGRA.a = (uint8)_chd->minValue; \
        else vc.BGRA.a = (uint8)va;                       \
    } else vc.BGRA.a += (uint8)_va;                       \
    *((unsigned int *)_chd->pointer) = vc

#define GT_TMP_ChildColorSub() /*颜色透明值减*/           \
    vc = *((unsigned int *)_chd->pointer);                \
    _va = (int)_chd->addValue;                            \
    va = (int)_chd->minValue;                             \
    if((int)vc.BGRA.a - _va < va) {                       \
        if(aValueLoop) vc.BGRA.a = (uint8)_chd->maxValue; \
        else vc.BGRA.a = (uint8)va;                       \
    } else vc.BGRA.a -= (uint8)_va;                       \
    *((unsigned int *)_chd->pointer) = vc

#define GT_TMP_ChildValueAdd(value_type, p_v, t_v, add_proc) /*变量值加*/ \
    p_v = (value_type *)(_chd->pointer);                                  \
    *p_v += add_proc(value_type) _chd->addValue;                          \
    t_v = (value_type)_chd->maxValue;                                     \
    if(*p_v > t_v) {                                                      \
        if(aValueLoop) *p_v = (value_type)_chd->minValue;                 \
        else *p_v = t_v;                                                  \
    }

#define GT_TMP_ChildValueSub(value_type, p_v, t_v, add_proc) /*变量值减*/ \
    p_v = (value_type *)(_chd->pointer);                                  \
    *p_v -= add_proc(value_type) _chd->addValue;                          \
    t_v = (value_type)_chd->minValue;                                     \
    if(*p_v < t_v) {                                                      \
        if(aValueLoop) *p_v = (value_type)_chd->maxValue;                 \
        else *p_v = t_v;                                                  \
    }

#define GT_TMP_ChildValueOperate(Proc)                                                                                                            \
    GT_TMP_SIM_DIALOG_SoundPlay switch(_chd->type) {                                                                                              \
        case GT_SP_VALUE_INT: {                                                                                                                   \
            int *_valuei, t_valuei;                                                                                                               \
            if(_chd->enumIds && !(_chd->addValue)) { /*修改列举值*/                                                                               \
                GT_TMP_ChildEnum##Proc();                                                                                                         \
            } else { /*修改整形值*/                                                                                                               \
                GT_TMP_ChildValue##Proc(int, _valuei, t_valuei, );                                                                                \
            }                                                                                                                                     \
        } break;                                                                                                                                  \
        case GT_SP_VALUE_FLOAT: { /*修改浮点值*/                                                                                                  \
            float *_value_f, t_valuef;                                                                                                            \
            GT_TMP_ChildValue##Proc(float, _value_f, t_valuef, _chd->addValue < 0.0f ? ((*_value_f) ? (*_value_f) : 0.5f) * (-_chd->addValue) :); \
        } break;                                                                                                                                  \
        case GT_SP_VALUE_COLOR: { /*修改颜色值*/                                                                                                  \
            int va, _va;                                                                                                                          \
            GNg::sCOLOR vc;                                                                                                                       \
            GT_TMP_ChildColor##Proc();                                                                                                            \
        } break;                                                                                                                                  \
    }                                                                                                                                             \
    if(zValueChangeProc) zValueChangeProc(zValueChangeArg)
            //--------------------------------------------------------------------------------------------------------
            switch(_chd->type) {
                case GT_SP_VALUE_INT:
                case GT_SP_VALUE_FLOAT:
                case GT_SP_VALUE_COLOR: { //左右方向或在锁定值时上下方向都可以修改变量值
                    const bool locking = (zSelect == zSelectLockID);
                    if(key_left || (locking && key_up)) {
                        GT_TMP_ChildValueOperate(Sub); //值减
                    } else if(key_right || (locking && key_down)) {
                        GT_TMP_ChildValueOperate(Add); //值加
                    }
                    if(locking) {
                        if(key_ok) zSelectLockID = 30000; //无锁定时再按 OK 就取消锁定
                    } else if(key_ok) zSelectLockID = zSelect; //无锁定时按下 OK 就锁定变量
                } break;
                case GT_SP_MENU: //选中选单项,进入下一级选单
                    if(key_ok) zSetActiveMenu((sSIM_MENU *)(_chd->pointer));
                    break;
                    //case GT_SP_TEXT: break; //这样修改忽略文本值
            }
        } //--------------------------------------------------------------------------------------------------------
    } else {
        switch(zSelect - zChildNum) { //功能按钮的处理
            case 0: //点到取消
                if(key_ok) {
                    if(zFun1) zFun1();
                    else Cancel();
                }
                break;
            case 1: //点到保存
                if(key_ok) {
                    if(zFun2) zFun2();
                    else Save();
                }
                break;
            case 2: //点到默认
                if(key_ok) {
                    if(zFun3) zFun3();
                    else Default();
                }
                break;
        }
    }
    return zTargetMenu;
}
void GNu::cSIM_DIALOG::SetValueChange(bool value_loop) {
    aValueLoop = value_loop;
}
void GNu::cSIM_DIALOG::zHide() {
    if(zEffect) {
        GT_TMP_SIM_DIALOG_EffectTransTimeSet();
        zState = 3;
    } else {
        zState = 0;
        zTargetMenu = 0; //没父选单就发出结束消息
    }
}
void GNu::cSIM_DIALOG::Hide() {
    if(!zState || zState == 3) return;
    zDetailSetState = eDETAIL_SET_NULL;
    zHide();
}
void GNu::cSIM_DIALOG::Cancel() {
    if(!zState || zState == 3) return;

#define GT_TMP_SIM_DIALOG_Cancel()                                                            \
    zDetailSetState = eDETAIL_SET_NULL;                                                       \
    if(zTargetMenu->zFather) zSetActiveMenu(zTargetMenu->zFather); /*有父层就退回上一层选单*/ \
    else zHide()

    GT_TMP_SIM_DIALOG_Cancel();
}
void GNu::cSIM_DIALOG::zSave(int offset_num) {
    const char *err_str = "调用cSIM_DIALOG::Save ";
    GNf::sFILE *savefile = GNf::Open(zFileName, "r+b", GNf::eFILE_SAFE);
    if(savefile) { //文件存在就保存该层选单的数据
        char file_head[4];
        GNf::Read(file_head, 1, 4, savefile);
        if(!strcmp(file_head, "YGS")) {
            GT_SimMenuTextOffsetPos = 0.0f;
            GNf::Seek(savefile, offset_num, GNf::eSEEK_CUR); /*跳至变量要保存的文件正确位置*/
            GT_sSP_CHILD *_chd;
            short si = 0;
            for(; si < zChildNum; ++si) {
                _chd = &(zTargetMenu->zChild[si]);
                if(_chd->type == GT_SP_VALUE_TEXT) {
                    GNf::Write((char *)&(_chd->maxValue), 4, 1, savefile);
                    GT_SimMenuTextOffsetPos = 1.0f;
                } else if(_chd->type <= GT_SP_VALUE_FLOAT) GNf::Write((char *)(_chd->pointer), 4, 1, savefile); //写入当前选单的变量数据
            }
            if(GT_SimMenuTextOffsetPos) { //写入当前选单的文本内容
                GNf::Close(savefile);
                char *name_end = zFileName + (strlen(zFileName) - 1);
                *name_end = 't';
                savefile = GNf::Open(zFileName, "r+b", GNf::eFILE_SAFE);
                if(savefile) {
                    GNf::Read(file_head, 1, 4, savefile);
                    if(!strcmp(file_head, "YGT")) {
                        for(si = 0; si < zChildNum; ++si) {
                            _chd = &(zTargetMenu->zChild[si]);
                            if(_chd->type == GT_SP_VALUE_TEXT) {
                                GNf::Seek(savefile, 4 + (int)(_chd->addValue), GNf::eSEEK_START); //跳至文本要保存的位置
                                GNf::Write((char *)(_chd->pointer), 1, (int)(_chd->maxValue), savefile);
                            }
                        }
                    } else GN_Log("%s文件 : %s 损坏了!\n", err_str, zFileName);
                } else GN_Log("%s保存文本数据有错! 没找到对应的 ygt文件!\n", err_str);
                *name_end = 's';
            }
        } else GN_Log("%s文件 : %s 损坏了!\n", err_str, zFileName);
        GNf::Close(savefile);
    } else
        GN_Log("%s文件 : %s 没有存档数据!\n", err_str, zFileName);
}
void GNu::cSIM_DIALOG::Save() {
    if(!zState) return;
    if(zFileName) {
        int offset_num = 0; //计算该选单变量存放在文件的偏移位置
        zComputeMenuForDataOffset(offset_num, zTargetMenu);
        zSave(offset_num);
        zSave(offset_num);
    }
    GT_TMP_SIM_DIALOG_Cancel();
}
void GNu::cSIM_DIALOG::Default(bool this_also) {
    if(!zState) return;
    zTargetMenu->zReDefault(false);
    if(this_also && zTargetMenu != (GNu::sSIM_MENU *)this) this->zReDefault(false);
    if(zValueChangeProc) zValueChangeProc(zValueChangeArg);
}
/********************************************************** cSIM_DIALOGEX ******************************************************************/
struct GT_s_VALUE_TS //值转换信息
{
    int chgv; //中转修改值
    uint8 *tg_v; //目标修改值
};
typedef GN::sLIST<GT_s_VALUE_TS, GN::cALLOC_POOL<GT_s_VALUE_TS>> GT_s_VALUE_TS_LIST;
void GT_SimpleDlgExOnValueChange(void *us_arg) {
    ((GNu::cSIM_DIALOGEX *)us_arg)->zOnValueChangeProc(0);
}
void GNu::cSIM_DIALOGEX::zOnValueChangeProc(unsigned short *chg_id_out) {
    GT_s_VALUE_TS_LIST *_items = (GT_s_VALUE_TS_LIST *)zTsItems;
    GT_s_VALUE_TS_LIST::iterator ititm = _items->begin(), etitm = _items->end();
    if(chg_id_out) {
        unsigned short i = 0;
        *chg_id_out = 0xffff;
        GT_s_VALUE_TS *_vts;
        for(; ititm != etitm; ++ititm, ++i) {
            _vts = &(*ititm);
            if(*(_vts->tg_v) != (uint8)(_vts->chgv)) {
                *(_vts->tg_v) = (uint8)(_vts->chgv);
                *chg_id_out = i;
            }
        }
    } else {
        for(; ititm != etitm; ++ititm) {
            *(ititm->tg_v) = (uint8)(ititm->chgv);
        }
    }
    if(zValueChangeProc2) zValueChangeProc2(zValueChangeArg2);
}
void GNu::cSIM_DIALOGEX::UpValues() {
    GT_s_VALUE_TS_LIST *_items = (GT_s_VALUE_TS_LIST *)zTsItems;
    GT_s_VALUE_TS_LIST::iterator ititm = _items->begin(), etitm = _items->end();
    for(; ititm != etitm; ++ititm) {
        ititm->chgv = *(ititm->tg_v);
    }
}
GNu::cSIM_DIALOGEX::cSIM_DIALOGEX() : zValueChangeProc2(0), zValueChangeArg2(0) {
    zTsItems = new GT_s_VALUE_TS_LIST;
    cSIM_DIALOG::OnValueChange(GT_SimpleDlgExOnValueChange, this);
}
void GNu::cSIM_DIALOGEX::OnValueChange(void (*proc)(void *), void *us_arg) {
    zValueChangeProc2 = proc;
    zValueChangeArg2 = us_arg;
}
GNu::cSIM_DIALOGEX::~cSIM_DIALOGEX() {
    if(zTsItems) {
        GT_s_VALUE_TS_LIST *_items = (GT_s_VALUE_TS_LIST *)zTsItems;
        delete _items;
        zTsItems = 0;
    }
}
void GNu::cSIM_DIALOGEX::zRemoveValueRelatTsItems() {
    GT_s_VALUE_TS_LIST *_items = (GT_s_VALUE_TS_LIST *)zTsItems;
    GT_s_VALUE_TS_LIST::iterator ititm, etitm = _items->end();
    short ci, ce = zChild.size();
    if(ce && _items->size()) {
        void *_ptr;
        GT_s_VALUE_TS *_vts;
        for(ci = 0; ci < ce; ++ci) {
            _ptr = zChild[ci].pointer;
            for(ititm = _items->begin(); ititm != etitm;) {
                if(_ptr == (void *)&(ititm->chgv)) {
                    ititm = _items->erase(ititm);
                    break;
                } else ++ititm;
            }
        }
    }
}
void GNu::cSIM_DIALOGEX::RemoveValue() {
    zRemoveValueRelatTsItems();
    cSIM_DIALOG::RemoveValue();
}
void GNu::cSIM_DIALOGEX::RemoveValueAll() {
    ((GT_s_VALUE_TS_LIST *)zTsItems)->clear();
    cSIM_DIALOG::RemoveValueAll();
}
void GNu::cSIM_DIALOGEX::Release() {
    zRemoveValueRelatTsItems();
    cSIM_DIALOG::Release();
}
void GNu::cSIM_DIALOGEX::ReleaseAll() {
    ((GT_s_VALUE_TS_LIST *)zTsItems)->clear();
    cSIM_DIALOG::ReleaseAll();
}
void GNu::cSIM_DIALOGEX::InitValueToData1(sSIM_MENU *tg, uint8 *value, const char *name, uint8 max_value, uint8 min_value, uint8 add_value, const char *enum_name, P<GNg::sIMAGE> icon) {
    GT_TMP_sSIM_MENU_HasInitValue(value)
        GT_s_VALUE_TS_LIST *_items = (GT_s_VALUE_TS_LIST *)zTsItems;
    GT_s_VALUE_TS *_node = _items->add(GT_s_VALUE_TS());
    _node->tg_v = value;
    _node->chgv = *value;
    tg->InitValueToData(&(_node->chgv), name, max_value, min_value, add_value, enum_name, icon);
}
/******************************************************************************************************************************************************************************/
#ifdef _GN_SOUND_H_
void GNu::cUI_SIM_DIALOG::SetKeyToData(P<GNa::sSOUND> &sound, short ok, short down, short up, short left, short right, bool usMouse) {
    zSouObj = (GNa::sSOUND *)sound;
    zSound.InitToData(zSouObj);
    zKeyOK = ok;
    zKeyDOWN = down;
    zKeyUP = up;
    zKeyLEFT = left;
    zKeyRIGHT = right;
    zUsMouse = usMouse;
    zSouObj1 = sound;
}
#endif
void GNu::cUI_SIM_DIALOG::SetEffect(GNg::cFRAME_RC *effect, short focus_flash_time, P<GNg::sIMAGE> focus_style, P<GNg::sIMAGE> arrow_style, bool affect_color) {
    cSIM_DIALOG::SetEffect(effect, focus_flash_time, focus_style, arrow_style, affect_color);
    zFocusMap1 = focus_style;
    zArrowMap1 = arrow_style;
}
void GNu::cUI_SIM_DIALOG::SetBackFrame(P<GNg::sIMAGE> &frame_img, P<GNg::sIMAGE> &caption_img, bool repeat, void (*draw_call)(short, short, short, short, unsigned int, unsigned short, GNg::sIMAGE *)) {
    cSIM_DIALOG::SetBackFrame(frame_img, caption_img, repeat, draw_call);
    zBackCaption1 = caption_img;
    if(zBackFrame) zBackFrame1 = frame_img;
    else zBackFrame1.Delete();
}
GNu::cUI_SIM_DIALOG::~cUI_SIM_DIALOG() {
    if(GT_FullSelectPathDlg == (cSIM_DIALOGEX *)this) GT_FullSelectPathDlg = 0;
    if(zObjsKeep.Us() == 1) {
        GN_Delete(zEffect);
        zObjsKeep.Delete();
    }
}
P<GNu::iUI_OBJECT> GNu::cUI_SIM_DIALOG::Clone() {
    P<cUI_SIM_DIALOG> ui_dlg(1);
    cUI_SIM_DIALOG *_ui_dlg = ui_dlg;
    zTextOut.Release();
    void *ts_items_bk = _ui_dlg->zTsItems;
    *_ui_dlg = *this;
    _ui_dlg->zTsItems = ts_items_bk;
    memset(&(_ui_dlg->zChild), 0, sizeof(_ui_dlg->zChild));
    _ui_dlg->zCaption = 0;
    GN_StrcpyData(_ui_dlg->zCaption, zCaption);
    _ui_dlg->zFileName = 0;
    GN_StrcpyData(_ui_dlg->zFileName, zFileName);
    if(zButtonName) {
        _ui_dlg->zButtonName = new char[GT_DLG_BNAME_SIZE];
        memcpy(_ui_dlg->zButtonName, zButtonName, GT_DLG_BNAME_SIZE); //复制按钮名
    }
    _ui_dlg->zTmpRangeRect.Delete();
    _ui_dlg->zSetTool.Delete();
    _ui_dlg->zIsDetailSet = false;
    memset(&(_ui_dlg->zSound), 0, sizeof(_ui_dlg->zSound));
    if(zSouObj) _ui_dlg->zSound.InitToData(zSouObj);
    _ui_dlg->zValueChangeArg = (cSIM_DIALOGEX *)_ui_dlg;
    _ui_dlg->zFather = 0;
    //_ui_dlg->InitValueToData(&(_ui_dlg->aCaptextColor),				 " 标题颜色         ", 255, 0);
    //_ui_dlg->InitValueToData1(_ui_dlg, (uint8*)&(_ui_dlg->aSkipPos),	 " 锁定筐位置       ", 1,  0,  1, " 否| 是");
    //_ui_dlg->InitValueToData1(_ui_dlg, (uint8*)&(_ui_dlg->aValueLoop), " 值大小调整循环   ", 1,  0,  1, " 否| 是");
    _ui_dlg->zSetActiveMenu(_ui_dlg);
    return ui_dlg;
}
unsigned int GNu::cUI_SIM_DIALOG::DataSave(cSHARE_DATA *share_objs, uint8 *data_out) {
    uint8 loc_buf[1500];
    if(!data_out) data_out = loc_buf;
    uint8 *wpos = data_out;
    *wpos = 1;
    ++wpos; //版本
    wpos = zNearAreaDataSave(wpos);
    wpos = zTextOut.zDataSave(wpos, share_objs->image_list);
    GT_cUI_SHARE_DATA_W *sdw = (GT_cUI_SHARE_DATA_W *)share_objs;
    unsigned int pointer_id;
    GT_TMP_OBJECT_WRITE_INDEX(sdw->list_frame_rc, (GNg::cFRAME *)zEffect);
    GT_TMP_OBJECT_WRITE_INDEX(sdw->list_keep_obj, zObjsKeep);
    GT_TMP_OBJECT_WRITE_INDEX(sdw->list_img, zIcon);
    GT_TMP_OBJECT_WRITE_INDEX(sdw->list_img, zFocusMap1);
    GT_TMP_OBJECT_WRITE_INDEX(sdw->list_img, zArrowMap1);
    GT_TMP_OBJECT_WRITE_INDEX(sdw->list_img, zBackFrame1);
    GT_TMP_OBJECT_WRITE_INDEX(sdw->list_img, zBackCaption1);
    pointer_id = 0xffffffff;
#ifdef _GN_SOUND_H_
    if(zSouObj1) pointer_id = sdw->list_sound.add_no_eq2(zSouObj1);
#endif
    memcpy(wpos, &pointer_id, 4);
    wpos += 4;

    memcpy(wpos, &zT_ef_x, 4);
    wpos += 4;
    memcpy(wpos, &zT_ef_y, 4);
    wpos += 4;
    memcpy(wpos, &zT_effect_rect, 16);
    wpos += 16;
    memcpy(wpos, &zTwidth, 2);
    wpos += 2;
    memcpy(wpos, &zTheight, 2);
    wpos += 2;
    memcpy(wpos, &zTi_compartment, 2);
    wpos += 2;
    memcpy(wpos, &zTj_compartment, 2);
    wpos += 2;
    memcpy(wpos, &zDlaOffsetX, 2);
    wpos += 2;
    memcpy(wpos, &zDlaOffsetY, 2);
    wpos += 2;
    memcpy(wpos, &zSetPathType, 2);
    wpos += 2;
    memcpy(wpos, &zKeyOK, 2);
    wpos += 2;
    memcpy(wpos, &zKeyDOWN, 2);
    wpos += 2;
    memcpy(wpos, &zKeyUP, 2);
    wpos += 2;
    memcpy(wpos, &zKeyLEFT, 2);
    wpos += 2;
    memcpy(wpos, &zKeyRIGHT, 2);
    wpos += 2;
    memcpy(wpos, &zUsMouse, 1);
    wpos += 1;
    memcpy(wpos, &zState, 1);
    wpos += 1;
    memcpy(wpos, &zType, 1);
    wpos += 1;
    memcpy(wpos, &zBackFrameRepeat, 1);
    wpos += 1;
    memcpy(wpos, &zIsDetailSet, 1);
    wpos += 1;
    memcpy(wpos, &aBackBlurLv, 1);
    wpos += 1;
    memcpy(wpos, &aAffectColor, 1);
    wpos += 1;
    memcpy(wpos, &aValueLoop, 1);
    wpos += 1;
    memcpy(wpos, &aSkipPos, 1);
    wpos += 1;
    memcpy(wpos, &aCaptextColor, 4);
    wpos += 4;
    memcpy(wpos, &aTextColor, 4);
    wpos += 4;
    memcpy(wpos, &aBackColor, 4);
    wpos += 4;
    memcpy(wpos, &aLineColor, 4);
    wpos += 4;
    memcpy(wpos, &aFocusColor, 4);
    wpos += 4;
    memcpy(wpos, &aInputBackColor, 4);
    wpos += 4;
    memcpy(wpos, &aInputBackBlend, 2);
    wpos += 2;
    memcpy(wpos, &aTextBlend, 2);
    wpos += 2;
    memcpy(wpos, &aBackBlend, 2);
    wpos += 2;
    memcpy(wpos, &aLineBlend, 2);
    wpos += 2;
    memcpy(wpos, &aFocusBlend, 2);
    wpos += 2;
    memcpy(wpos, &aLineWidth, 2);
    wpos += 2;
    memcpy(wpos, &aCaptextOfsY, 2);
    wpos += 2;
    memcpy(wpos, &zFocusFlash, 2);
    wpos += 2;

    unsigned short len = zButtonName ? GT_DLG_BNAME_SIZE : 0;
    memcpy(wpos, &len, 2);
    wpos += 2;
    if(len) {
        memcpy(wpos, zButtonName, len);
        wpos += len;
    }

    len = zFileName ? strlen(zFileName) + 1 : 0;
    memcpy(wpos, &len, 2);
    wpos += 2;
    if(len) {
        memcpy(wpos, zFileName, len);
        wpos += len;
    }

    len = zCaption ? strlen(zCaption) + 1 : 0;
    memcpy(wpos, &len, 2);
    wpos += 2;
    if(len) {
        memcpy(wpos, zCaption, len);
        wpos += len;
    }
    return wpos - data_out;
}
unsigned int GNu::cUI_SIM_DIALOG::DataLoad(cSHARE_DATA *share_objs, const uint8 *data) {
    uint8 *rpos = (uint8 *)data;
    uint8 version = *rpos;
    ++rpos;
    if(version > 1) return 0; //版本
    rpos = zNearAreaDataLoad(rpos);
    rpos = zTextOut.zDataLoad(rpos, share_objs->image_refs);
    GT_cUI_SHARE_DATA_R *sdr = (GT_cUI_SHARE_DATA_R *)share_objs;
    unsigned int pointer_id;
    memcpy(&pointer_id, rpos, 4);
    rpos += 4;
    if(pointer_id != 0xffffffff) zEffect = sdr->pframes_rc[pointer_id];
    memcpy(&pointer_id, rpos, 4);
    rpos += 4;
    if(pointer_id != 0xffffffff) zObjsKeep = sdr->pkeep_objs[pointer_id];
    memcpy(&pointer_id, rpos, 4);
    rpos += 4;
    if(pointer_id != 0xffffffff) zIcon = sdr->pimages[pointer_id];
    memcpy(&pointer_id, rpos, 4);
    rpos += 4;
    if(pointer_id != 0xffffffff) zFocusMap1 = sdr->pimages[pointer_id];
    zFocusMap = zFocusMap1;
    memcpy(&pointer_id, rpos, 4);
    rpos += 4;
    if(pointer_id != 0xffffffff) zArrowMap1 = sdr->pimages[pointer_id];
    memcpy(&pointer_id, rpos, 4);
    rpos += 4;
    if(pointer_id != 0xffffffff) zBackFrame1 = sdr->pimages[pointer_id];
    zBackFrame = zBackFrame1;
    memcpy(&pointer_id, rpos, 4);
    rpos += 4;
    if(pointer_id != 0xffffffff) zBackCaption1 = sdr->pimages[pointer_id];
    zBackCaption = zBackCaption1;
    memcpy(&pointer_id, rpos, 4);
    rpos += 4;
#ifdef _GN_SOUND_H_
    if(pointer_id != 0xffffffff) {
        zSouObj1 = sdr->psounds[pointer_id];
        zSouObj = zSouObj1;
        zSound.InitToData(zSouObj);
    }
#endif
    eSET_PATH_TYPE set_path_type;
    memcpy(&zT_ef_x, rpos, 4);
    rpos += 4;
    memcpy(&zT_ef_y, rpos, 4);
    rpos += 4;
    memcpy(&zT_effect_rect, rpos, 16);
    rpos += 16;
    memcpy(&zTwidth, rpos, 2);
    rpos += 2;
    memcpy(&zTheight, rpos, 2);
    rpos += 2;
    memcpy(&zTi_compartment, rpos, 2);
    rpos += 2;
    memcpy(&zTj_compartment, rpos, 2);
    rpos += 2;
    memcpy(&zDlaOffsetX, rpos, 2);
    rpos += 2;
    memcpy(&zDlaOffsetY, rpos, 2);
    rpos += 2;
    memcpy(&set_path_type, rpos, 2);
    rpos += 2;
    memcpy(&zKeyOK, rpos, 2);
    rpos += 2;
    memcpy(&zKeyDOWN, rpos, 2);
    rpos += 2;
    memcpy(&zKeyUP, rpos, 2);
    rpos += 2;
    memcpy(&zKeyLEFT, rpos, 2);
    rpos += 2;
    memcpy(&zKeyRIGHT, rpos, 2);
    rpos += 2;
    memcpy(&zUsMouse, rpos, 1);
    rpos += 1;
    memcpy(&zState, rpos, 1);
    rpos += 1;
    memcpy(&zType, rpos, 1);
    rpos += 1;
    memcpy(&zBackFrameRepeat, rpos, 1);
    rpos += 1;
    memcpy(&zIsDetailSet, rpos, 1);
    rpos += 1;
    memcpy(&aBackBlurLv, rpos, 1);
    rpos += 1;
    memcpy(&aAffectColor, rpos, 1);
    rpos += 1;
    memcpy(&aValueLoop, rpos, 1);
    rpos += 1;
    memcpy(&aSkipPos, rpos, 1);
    rpos += 1;
    memcpy(&aCaptextColor, rpos, 4);
    rpos += 4;
    memcpy(&aTextColor, rpos, 4);
    rpos += 4;
    memcpy(&aBackColor, rpos, 4);
    rpos += 4;
    memcpy(&aLineColor, rpos, 4);
    rpos += 4;
    memcpy(&aFocusColor, rpos, 4);
    rpos += 4;
    memcpy(&aInputBackColor, rpos, 4);
    rpos += 4;
    memcpy(&aInputBackBlend, rpos, 2);
    rpos += 2;
    memcpy(&aTextBlend, rpos, 2);
    rpos += 2;
    memcpy(&aBackBlend, rpos, 2);
    rpos += 2;
    memcpy(&aLineBlend, rpos, 2);
    rpos += 2;
    memcpy(&aFocusBlend, rpos, 2);
    rpos += 2;
    memcpy(&aLineWidth, rpos, 2);
    rpos += 2;
    memcpy(&aCaptextOfsY, rpos, 2);
    rpos += 2;
    memcpy(&zFocusFlash, rpos, 2);
    rpos += 2;
    unsigned short len;
#define GT_TMP_LOAD_SIM_DLG_NAME(NAME) \
    memcpy(&len, rpos, 2);             \
    rpos += 2;                         \
    GN_DeleteArray(NAME);              \
    if(len) {                          \
        NAME = new char[len];          \
        memcpy(NAME, rpos, len);       \
        rpos += len;                   \
    }
    GT_TMP_LOAD_SIM_DLG_NAME(zButtonName)
    GT_TMP_LOAD_SIM_DLG_NAME(zFileName)
    GT_TMP_LOAD_SIM_DLG_NAME(zCaption)
    if(!zArrowMap1) {
        zArrowMap1.New(1);
        zArrowMap = zArrowMap1;
        GT_TMP_MAKE_SIM_ARROW_ICON
    } else zArrowMap = zArrowMap1;

    zShowReady = zCaption && zTextOut.IsInited();
    if(zIsDetailSet) EnableDetailSetting(zIsDetailSet, aInputBackColor, aInputBackBlend, set_path_type);
    else zSetPathType = set_path_type;
    if(zState) Pop();
    return rpos - data;
}
#include "Player/GN_EdPlugin.h"
GN_API void (*GP_SettingMenuInsert)(GP_eSETTING_CLASS set_class, void *_set_obj, P<P_> &other_objs, GNu::sSIM_MENU *pmenu) = 0;
GN_API GNu::iSETTING_MENU *(*GP_BaseSetMenuCreate)(GP_eMENU_TYPE menu_type, void *args) = 0;

#endif //_GN_UIPOPDLG_H_
