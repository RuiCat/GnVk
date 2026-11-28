// GN_UiPopDlgMenu.cpp — GNu::sSIM_MENU（选单/子项数据与读写默认值）

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
#include "GN_UiPopDlgInner.h"
#ifdef _GN_UIPOPDLG_H_
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
    /* 已核（B5f）：new char[len+1]，len = strlen(name)。 */ \
    memcpy(chd.name, name, strlen(name) + 1);

#ifdef GN_DEBUG

#define GT_TMP_sSIM_MENU_OutRangeWarring \
    if(out_range) GN_Log("调用sSIM_MENU::InitValueToData %s : 值超出限定范围!\n", name);

    GT_TMP_sSIM_MENU_HasInitValue(text) if(!text_buf_size) {
        GN_Log("调用 sSIM_MENU::InitValueToData 文本缓冲没有指定好\n");
        return;
    }
#else

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
    // 已核（B5f）：enumName = new char[text_buf_size]，text 已在函数前面按 text_buf_size-1 截断。
    memcpy(chd.enumName, text, strlen(text) + 1); //原初始值保存到列举名里
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

    GT_TMP_sSIM_MENU_HasInitValue(value) bool out_range = false; (void)out_range; (void)out_range;
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
    GT_TMP_sSIM_MENU_HasInitValue(value) bool out_range = false; (void)out_range; (void)out_range;
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
    GT_TMP_sSIM_MENU_HasInitValue(value) bool out_range = false; (void)out_range; (void)out_range;
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
    // 已核（B5f）：new char[len+1]，len = strlen(name)。
    memcpy(menu->zCaption, name, strlen(name) + 1);
}
void GNu::sSIM_MENU::InitTextToData(const char *text, int *value, P<GNg::sIMAGE> icon) {
#define GT_TMP_SIMPLE_MENU_InitText(VALUE_TYPE) \
    zChild.push_back(GT_sSP_CHILD());           \
    GT_sSP_CHILD &chd = zChild.back();          \
    chd.type = GT_SP_TEXT;                      \
    int len = strlen(text);                     \
    chd.name = new char[len + 1];               \
    /* 已核（B5f）：new char[len+1]，len = strlen(text)。 */              \
    memcpy(chd.name, text, strlen(text) + 1);                     \
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
sPopDlgContext &GN_UiPopDlgContext() {
    static sPopDlgContext ctx;
    return ctx;
}

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
    // B5f：pointer 容量 = maxValue，按上限截断。
    GN_StrCopyToLimit((char *)zChild[si].pointer, (int)zChild[si].maxValue, new_name);
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
            _chd->addValue = GN_UiPopDlgContext().aSimMenuTextOffsetPos;
            GN_UiPopDlgContext().aSimMenuTextOffsetPos += _chd->maxValue;
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
                        if(mask > (1u << (unsigned int)(_chd->maxValue)) - 1) {
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
                    _chd->addValue = GN_UiPopDlgContext().aSimMenuTextOffsetPos;
                    GN_UiPopDlgContext().aSimMenuTextOffsetPos += _chd->maxValue;
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
                // B5f：pointer 容量 = maxValue，按上限截断。
                GN_StrCopyToLimit((char *)(_chd->pointer), (int)_chd->maxValue, _chd->enumName);
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
#endif //_GN_UIPOPDLG_H_
