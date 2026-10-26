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
#ifdef _GN_UITEXTIN_H_
/***************************************************** 文本输入 *************************************************************/
extern bool GT_ImeOn;
extern bool GT_ImeOnRecv;
extern uint8 GT_ImeCount;
extern char GT_ImeChar[];

GNu::cTEXT_INPUT *GNu::cTEXT_INPUT::zCurIn = 0;
uint8 GNu::cTEXT_INPUT::zBackAlpha = 0;
unsigned short GNu::cTEXT_INPUT::aMaxCancelNum = 128;

GNu::cTEXT_INPUT::cTEXT_INPUT() : zBuf(0), zShowView(&GNg::View), aDisableControl(false), aLock(false)
#ifndef GN_USING_ADDITIONAL_LIB //GN_ADD_CONTEN_PRIVATE
                                  ,
                                  aTabKeyHandle(true)
#endif
{
}
GNu::cTEXT_INPUT::~cTEXT_INPUT() {
    this->Release();
}

void GNu::cTEXT_INPUT::Release() {
    GN_DeleteArray(zBuf);
    SetPrimaryCancel();
    if(zCurIn == this) zCurIn = 0;
}
void GNu::cTEXT_INPUT::zCheckAddSave() {
    if(zInputSaveOn) {
        if(!zCancelIng) { /*模版代码:记录输入操作 以便用来作撤消用*/
            zAddSave(true, zSaveStartBid, zLastEndBid - zSaveStartBid, &zBuf[zSaveStartBid], zBid);
            zLastEndBid = zBid;
        } else zSaveStartBid = zBid;
    }
}
void GNu::cTEXT_INPUT::zEraseSaveNode() {
    if(zInputSaveOn && zCancelIng) {
        if(zIt == zSaveList.end()) zSaveList.clear();
        else {
            GN::sLIST<GT_sINPUT_SAVE>::iterator it = zSaveList.begin();
            for(; it != zIt;) it = zSaveList.erase(it); //删除前段记录 保持刚插入的记录在最前面
        }
        zCancelIng = false;
    }
}
void GNu::cTEXT_INPUT::InitStyle(const GNg::cTEXT_ART *text_style, short x, short y, short w, short h,
                                 unsigned int text_color, unsigned int focus_color, unsigned short text_blend, unsigned short focus_blend,
                                 P<GNg::sIMAGE> ret_arrow) {
    if(text_style) {
        ((GNg::cTEXT_ART *)text_style)->Release();
        zTextOut.Release();
        zTextOut = *text_style;
        zShowReady = true;
    } else if(GN::RunRec.zGWPrintReady) {
        GN::RunRec.aFont.Release();
        zTextOut.Release();
        zTextOut = GN::RunRec.aFont;
        zShowReady = true;
    } else {
        zShowReady = false;
        return;
    }
    //颜色赋值
    aTextColor = text_color ? text_color : GT_AppDefaultColorFont;
    aFocusColor = focus_color;
    aTextBlend = text_blend;
    aFocusBlend = focus_blend;
    zDlgx = x, zDlgy = y;
    zDlgw = w, zDlgh = h;

    if(ret_arrow) zRetArrow = ret_arrow;
}
void GNu::cTEXT_INPUT::InitInputFmtToData(eTX_IN_TYPE type, const char *default_v, unsigned short max_input_num) {
    zSaveList.clear();
    GN_DeleteArray(zBuf);

    zInType = type;
    if(zInType == eTX_IN_TEXT_LOCK)
        ;
    else if(zInType > eTX_IN_PASSWORD && max_input_num > 16) max_input_num = 16; //如输入数字的话 16位就够了
    zMaxInNum = max_input_num;
    zBuf = new char[zMaxInNum + 2]; //后增加两个字符 1:预留输入符 0:结束符 编辑筐和普通文字输出处理算法上不同之处在 每行的末端都会自动加个:1 的代表为插入符
    zBuf[zMaxInNum + 1] = 0;
    zBuf[0] = 1; //插入点在起始
    zBuf[1] = 0;
    zArrangeText(); //先排列一下文本
    zSetBid(0);
    zEid = zBid2 = zBid;
    zLastEndBid = zSaveStartBid = zBid;
    zOffsetx = 0;
    zOffsetxtg = 0;
    zOffsetxad = 0;
    zOffsety = 0;
    zOffsetytg = 0;
    zOffsetyad = 0;
    zOffsetN1 = 2;
    zOffsetN2 = 4;
    zOffsetSheep = 0.1f;
    zImeIn = false;
    zLastMK = false;
    zMousePass = false;
    zArgText = false;
    zCancelIng = false;
    zInputSaveOn = true;
    InputText(default_v);
}
bool GNu::cTEXT_INPUT::InitLoadToData(const char *file_name) {
#ifdef GN_DEBUG
    if(!file_name || !file_name[0]) {
        GN_Log("调用 cTEXT_INPUT::InitLoadToData 失败! 找不到文件 %s!\n", GNf::sFILE::DefaultType < GNf::eFILE_MEM ? file_name : "内容");
    }
#endif
    int size;
    char *txt_buf = GNf::FileFullLoadToMemoryText((char *)file_name, &size);
    if(txt_buf) {
        InitInputFmtToData(eTX_IN_TEXT, txt_buf);
        delete[] txt_buf;
        return true;
    }
    GN_Log("调用 cTEXT_INPUT::InitLoadToData 失败! 未能读取到文件 %s!\n", GNf::sFILE::DefaultType < GNf::eFILE_MEM ? file_name : "内容");
    return false;
}
void GNu::cTEXT_INPUT::SetOffsetEF(float sheep, uint8 n1, uint8 n2) {
    zOffsetSheep = sheep;
    zOffsetN1 = n1;
    zOffsetN2 = n2;
}
void GNu::cTEXT_INPUT::SetPos(short x, short y, short w, short h) {
    zDlgx = x, zDlgy = y;
    zDlgw = w, zDlgh = h;
    zArgText = true;
}
bool GNu::cTEXT_INPUT::IsMouseFocus() {
    return ((short)zShowView->mouseMX > zDlgx && (short)zShowView->mouseMX < zDlgx + zDlgw &&
            (short)zShowView->mouseMY > zDlgy && (short)zShowView->mouseMY < zDlgy + zDlgh);
}
void GNu::cTEXT_INPUT::zSetBid(unsigned short buf_id, bool save_last_xy) {
    zBid = buf_id > zEid ? zEid : buf_id; //预防用户乱设造成出轨

    switch(zInType) { //更新一下其它输入过滤状态 -------------------------------------------------------------------------------------------------
        case eTX_IN_INT:
            if(zBuf[0] == '0' || (zBuf[0] == '-' && zBuf[1] == '0')) zDelText(1, zEid); //如是整数首个字符是0 或首个是负号第2个是0 表示要删除后面的所有字符
        case eTX_IN_FLOAT:
            if(!zBid) {
                zIn_ = '-';
                if(zBuf[0] == '-') zDelText(0, 1); //删除后面的多余负号
            } else zIn_ = 50;
            break;
        case eTX_IN_UINT:
            //if(zBid)zIns = zBid? 0 :1;
            if(zBuf[0] == '0') zDelText(1, zEid); //如是整数首个字符是0 表示要删除后面的所有字符
            zIn_ = 50;
            break;
            //case eTX_IN_HEX: zIn_ = 50; break;
    }
    //检索文本输入点对应的显示焦点在那个可视字符里
    if(zTextOut.GetVisualStrCount() == zEid + 1) zVid = zBid;
    else {
        unsigned short vid;
        if(zTextOut.GetVIDfromText(zBid, &vid, &zVrow_id)) zVid = vid;
        else zVid = 65535;
    }
    if(save_last_xy && zVid < zTextOut.GetVisualStrCount()) {
        GNg::sDRAW_RANGE *_rng_data = zTextOut.zDrawRngData + zVid;
        zOffsetLastx = _rng_data->x - zDlgx - zOffsetx; //记下焦点所在筐内的局部坐标
        zOffsetLasty = _rng_data->y - zDlgy - zOffsety;
    }
}
void GNu::cTEXT_INPUT::SetPointer(unsigned short buf_id) {
    zBid = buf_id > zEid ? zEid : buf_id; //预防用户乱设造成出轨

    switch(zInType) {
        case eTX_IN_TEXT:
        case eTX_IN_TEXT_LOCK:
        case eTX_IN_PASSWORD: {
            int id = zBid;
            buf_id = 0;
            while((zBuf[id] & 128) && id >= 0) {
                --id;
                ++buf_id; //向前翻搜 以搜到英文或到文本起始为止
            }
            if(buf_id > 1 && !(buf_id & 1)) --zBid; //处理是中文时以免在字中间折断
        } break;
#if 0
    case eTX_IN_FLOAT:
	case eTX_IN_INT:
	case eTX_IN_UINT:
	case eTX_IN_HEX:
		break;
#endif
    }
    zSetBid(zBid);
    zBid2 = zBid;
}
void GNu::cTEXT_INPUT::InputText(const char *text, unsigned short len) {
    if(!text || aLock) return;
    if(!len) len = strlen(text);
    unsigned short add_num = zMaxInNum - zEid, isno_space = 0;

    if(add_num > len) add_num = len; //能增加的字数
    else isno_space = 1;

    if(!add_num) { /*没空间或没字符输入*/
        if(text[0] == 8 /*BACK键*/) {
            zBackSpaceDelTextProc();
        } else zFuntionKeyProc(text[0]);
        return;
    }
    uint8 locbuf[16384]; //16K
    GN::sTBUF_INT tbuf(locbuf, 16384);
    char *filter_text = (char *)tbuf.GetSpace(add_num << 1);
    unsigned short ai, in_count = 0, has_enter = 0;

    switch(zInType) { /*过滤转换输入来的字符*/
        case eTX_IN_TEXT:
        case eTX_IN_TEXT_LOCK:
        case eTX_IN_PASSWORD: /*---------------------------------------------------------------------------------------------*/
            for(ai = 0; ai < add_num; ++ai) { //将不属于表示文本的字符转换一下
                if(text[ai] == 8 /*BACK键*/) { //退格符删除文字
                    zBackSpaceDelTextProc();

                } else if(zFuntionKeyProc(text[ai]))
                    ;
                else if(text[ai] == 27 /*ESC键*/)
                    ;
                else {
                    if(text[ai] == 0x0d /*回车键*/ || text[ai] == '\n') {
                        if(zInType == eTX_IN_TEXT) {
                            filter_text[in_count++] = 1;
                            add_num -= isno_space;
                            filter_text[in_count] = '\n';
                            if(text[ai + 1] == '\n') ++ai;
                            has_enter = 1;
                            zCheckAddSave();
                            ++in_count;
                        }
                    } else {
                        uint8 *text_code = (uint8 *)(text + ai);
                        if(text[ai] == 9 /*TAB键*/) {
#ifdef GN_USING_ADDITIONAL_LIB //GN_ADD_CONTEN_PRIVATE
                            zCheckAddSave();
#else
                            if(aTabKeyHandle) zCheckAddSave();
                            else continue;
#endif
                        } //过滤掉其它未知字符 未知字符用■来代替
                        if((text_code[0] & 128) && ai + 1 < add_num) {
                            if(text_code[0] < 0xa1 || text_code[0] > 0xf7 || !(text_code[1] & 128) || text_code[1] < (char)0xa0) {
                                filter_text[in_count++] = 0xa1;
                                filter_text[in_count++] = 0xf6;
                            } else {
                                filter_text[in_count++] = text[ai];
                                filter_text[in_count++] = text[ai + 1];
                            }
                            ++ai;
                        } else filter_text[in_count++] = text[ai];
                    }
                }
            }
            break;
        case eTX_IN_FLOAT: { /*-----------------------------------------------------------------------------------------------*/
            unsigned short dot_id = 65535;
            for(ai = 0; ai < len; ++ai)
                if(text[ai] == '.') dot_id = ai; //搜索最后一点的位置
            for(ai = 0; ai < add_num; ++ai) { //将不属于浮点数表示的字符忽略掉
                if(text[ai] == 8 /*BACK键*/) {
                    zBackSpaceDelTextNumberProc();
                } else if(zFuntionKeyProc(text[ai]))
                    ;
                else if(text[ai] == zIn_ || ai == dot_id || (text[ai] >= '0' && text[ai] <= '9')) {
                    if(ai == dot_id) { //输入的是小数点处理一下
                        uint8 di = 0;
                        for(; di < zEid; ++di) {
                            if(zBuf[di] == '.') { //检索原先文本内是否有小数点 有就删除它
                                if(di < zBid) {
                                    --zBid;
                                    --zVid;
                                    zBid2 = zBid;
                                }
                                for(; zBuf[di + 1]; ++di) zBuf[di] = zBuf[di + 1]; //把后面不删除的字符续在删除点上
                                zBuf[--zEid] = 1;
                                zBuf[zEid + 1] = 0; //记下新的文本结束符
                                break;
                            }
                        }
                    }
                    zIn_ = 50;
                    filter_text[in_count++] = text[ai];
                }
            }
        } break;
        case eTX_IN_INT: /*--------------------------------------------------------------------------------------------------*/
        case eTX_IN_UINT:
        case eTX_IN_HEX:
            for(ai = 0; ai < add_num; ++ai) { //将不属于整型数表示的字符忽略掉
                if(text[ai] == 8 /*BACK键*/) {
                    zBackSpaceDelTextNumberProc();
                } else if(zFuntionKeyProc(text[ai]))
                    ;
                else if((zInType == eTX_IN_HEX && ((text[ai] >= '0' && text[ai] <= '9') || (text[ai] >= 97 && text[ai] <= 102) || (text[ai] >= 65 && text[ai] <= 70))) ||
                        (text[ai] == zIn_ || (text[ai] >= '0' && text[ai] <= '9'))) {
                    zIn_ = 50;
                    filter_text[in_count++] = text[ai];
                }
            }
            break;
    } /*------------------------------------------------------------------------------------------------------------------*/
    if(in_count) {
        if(zLastEndBid != zBid && zInputSaveOn) { //换了位置输入文字 记录上次输入操作
            if(!zCancelIng) {
                zAddSave(true, zSaveStartBid, zLastEndBid - zSaveStartBid, &zBuf[zSaveStartBid], zBid);
            } else {
                zSaveStartBid = zBid;
            }
        }
        zEraseSaveNode(); //如果正在撤消当中有新输入即可剪断撤消位置之前的记录

        if(zBid != zBid2) { //如有选定范围的内容就先删除它
            zDelText(zBid, zBid2);
            if(zMoveTextFrame && zVid >= zTextOut.GetVisualStrCount()) {
                zReOffsetLast = true;
                zOffsetxtg = -(zOffsetLastx - (short)((float)zDlgw * zTextOut.zParame.near_pointU));
                zOffsetytg = -(zOffsetLasty - (short)((float)zDlgh * zTextOut.zParame.near_pointV));
            }
            zBid2 = zBid;
        }
        int iid = (int)zEid, ied = (int)zBid;
        for(; iid >= ied; iid--) zBuf[iid + in_count] = zBuf[iid]; //向后搬动字符留出在插入符里增加的字符空间
        zEid += in_count;
        zBuf[zEid + 1] = 0; //记下新的文本结束符

        for(ai = 0; ai < in_count; ++ai) zBuf[zBid + ai] = filter_text[ai]; //复制输入进来的文字

        zArrangeText();
        zArgText = false;
        zSetBid(zBid + in_count); //新当前插入位置
        zOffsetCtrlNext4(has_enter);
        zLastEndBid = zBid;
    } /*------------------------------------------------------------------------------------------------------------------*/
}
void GNu::cTEXT_INPUT::SetPrimary(float nearU, float nearV, bool refer_focusU, bool refer_focusV) {
    if(zShowView->zHasOrthoOfsIng || zShowView->zHasOrthoOfsTg) return;
    short nw = zDlgw, nh = zDlgh, tx = zDlgx, ty = zDlgy;
    if(refer_focusU || refer_focusV) {
        if(zVid < zTextOut.GetVisualStrCount()) { /*--------------------------------------------------------*/
            GNg::sDRAW_RANGE *_rw_range = zTextOut.zDrawRngData + zVid;
            int fx = _rw_range->x, fy = _rw_range->y,
                fw = _rw_range->w, fh = _rw_range->h;
            if(zTextOut.zParame.erect_print) {
                if(zTextOut.zParame.overturn_print) //竖向翻转输出
                    fh = fh < 0 ? (zDlgh >> 2) + 1 : (fh >> 2) + 1;
                else //竖向顺向输出
                    fh = fh < 0 ? (zDlgh >> 2) + 1 : (fh >> 2) + 1;
            } else {
                if(zTextOut.zParame.overturn_print) { //横向翻转输出
                    if(fw < 0) fw = zDlgw;
                    int fx2 = fx + fw;
                    fw = (fw >> 2) + 1;
                    fx = fx2 - fw;
                } else //横向顺向输出
                    fw = fw < 0 ? (zDlgw >> 2) + 1 : (fw >> 2) + 1;
            }
            if(refer_focusU) {
                nw = (short)fw;
                tx = (short)fx;
            }
            if(refer_focusV) {
                nh = (short)fh;
                ty = (short)fy;
            }
        }
    }
    zShowView->zOrthoLeftSrc = zShowView->ortho_left;
    zShowView->zOrthoTopSrc = zShowView->ortho_top;
    zShowView->zHasOrthoOfsIng = true;
    zShowView->zOrthoLeftTg = tx - (short)((float)(zShowView->orthoW - nw) * nearU);
    zShowView->zOrthoTopTg = ty - (short)((float)(zShowView->orthoH - nh) * nearV);
    zShowView->zHasOrthoOfsTg = true;

    if(!zRetArrow) {
        zRetArrow.New(1);
        //zRetArrow->InitToData(GN_SysDir("ui/point0_"), 1, GNg::CR_ALPHA);
        const int size = 32;
        zRetArrow->CreateEmptyToData(size, size, 1, GNg::CR_ALPHA);
        uint8 p_arrow_map[size * size];
        GNg::GenArrowDataToMem(p_arrow_map, size, size, 1);
        zRetArrow->DataFill(p_arrow_map, GNg::CR_ALPHA, size, size);
    }
}
void GNu::cTEXT_INPUT::SetPrimaryCancel() {
    if(!zShowView->zHasOrthoOfsIng || zShowView->zHasOrthoOfsTg) return;
    zShowView->zOrthoLeftTg = zShowView->zOrthoLeftSrc;
    zShowView->zOrthoTopTg = zShowView->zOrthoTopSrc;
    zShowView->zHasOrthoOfsTg = true;
    zShowView->zHasOrthoOfsIng = false;
}
char *GNu::cTEXT_INPUT::GetTextData(bool select_only) {
    char *p_buf, *p_buf2;
    unsigned int size;
    if(select_only) {
        if(zBid < zBid2) p_buf = zBuf + zBid, p_buf2 = zBuf + zBid2;
        else p_buf = zBuf + zBid2, p_buf2 = zBuf + zBid;
        size = p_buf2 - p_buf;

    } else {
        p_buf = zBuf, p_buf2 = &zBuf[zEid];
        size = zEid;
    }
    char *ret_text = new char[size + 1];
    char *pstr = ret_text;
    while(p_buf < p_buf2) { //复制文本数据
        if(*p_buf != 1) {
            *pstr = *p_buf;
            ++pstr;
        } //过滤值为1的插入符
        ++p_buf;
    }
    *pstr = 0;
    return ret_text;
}
void GNu::cTEXT_INPUT::zDelText(unsigned short buf_id1, unsigned short buf_id2) {
    if(aLock) return;
    unsigned short del_num, di;
    if(buf_id1 == buf_id2) return;
    else if(buf_id1 < buf_id2) { //计算删除数与找到删除起始点
        del_num = buf_id2 - buf_id1;
    } else {
        del_num = buf_id1 - buf_id2;
        buf_id1 = buf_id2;
    }
    if(zInputSaveOn) {
        if(!zCancelIng) { /*模版代码:记录输入操作 以便用来作撤消用*/
            zAddSave(true, zSaveStartBid, zLastEndBid - zSaveStartBid, zBuf + zSaveStartBid, zBid);
            //zLastEndBid = zBid;
        } else zSaveStartBid = zBid;

        zEraseSaveNode(); //如果正在撤消当中有新删除即可剪断撤消位置之前的记录
        zAddSave(false, buf_id1, del_num, zBuf + buf_id1, buf_id1); //记录输入操作 以便用来作撤消用
    }
    for(di = buf_id1; zBuf[di + del_num]; ++di) zBuf[di] = zBuf[di + del_num]; //把后面不删除的字符续在删除点上

    if(zEid >= del_num) zEid -= del_num;
    else zEid = 0;
    zBuf[zEid] = 1;
    zBuf[zEid + 1] = 0; //记下新的文本结束符
    zArrangeText(); //文本改变了就重排列一下
    zArgText = false;
    zSetBid(buf_id1); //新输入点为删除起始点
    zLastEndBid = zBid;
}
void GNu::cTEXT_INPUT::zBackSpaceDelTextProc() {
    if(zBid == zBid2) {
        if(zBid) {
            unsigned short is_enter = (unsigned short)(zBuf[zBid - 1] == '\n');
            zDelText(zBid - 1 - ((zBuf[zBid - 1] & 128) >> 7 || is_enter), zBid);
            zOffsetCtrlBack4(is_enter);
        }
    } else {
        zDelText(zBid, zBid2);
        if(zMoveTextFrame && zVid >= zTextOut.GetVisualStrCount()) {
            zOffsetxtg = -(zOffsetLastx - (short)((float)zDlgw * zTextOut.zParame.near_pointU));
            zOffsetytg = -(zOffsetLasty - (short)((float)zDlgh * zTextOut.zParame.near_pointV));
            zReOffsetLast = true;
        }
        zBid2 = zBid;
    }
}
void GNu::cTEXT_INPUT::zBackSpaceDelTextNumberProc() {
    if(zBid == zBid2) {
        if(zBid) zDelText(zBid - 1, zBid); //退格符删除文字
    } else {
        zDelText(zBid, zBid2);
        if(zMoveTextFrame && zVid >= zTextOut.GetVisualStrCount()) {
            zOffsetxtg = -(zOffsetLastx - (short)((float)zDlgw * zTextOut.zParame.near_pointU));
            zOffsetytg = -(zOffsetLasty - (short)((float)zDlgh * zTextOut.zParame.near_pointV));
            zReOffsetLast = true;
        }
    }
    zBid2 = zBid;
    if(zMoveTextFrame && zVid >= zTextOut.GetVisualStrCount()) {
        if(zTextOut.zParame.erect_print) {
            if(!zOffsetyad) zOffsetyad = zDlgh >> zOffsetN1;
        } else {
            if(zTextOut.zParame.overturn_print) {
                if(!zOffsetxad) zOffsetxad = -zDlgw >> zOffsetN1;
            } else {
                if(!zOffsetxad) zOffsetxad = zDlgw >> zOffsetN1;
            }
        }
    }
}
bool GNu::cTEXT_INPUT::zFuntionKeyProc(char key) {
    if(key == 1) { //全选
        if(zInputSaveOn) {
            zBid = zEid;
            zBid2 = 0;
            zOffsetLastx = 0;
            zOffsetLasty = 0;
        }
        return true;
    } else if(key == 3) { //复制
        if(zBid != zBid2) {
            char *c_text = GetTextData(true);
            GN_StrSetPasteText(c_text); //复制内容到粘贴版
            GN_DeleteArray(c_text);
        }
        return true;
    } else if(key == 24) { //剪切
        if(zBid != zBid2) {
            char *c_text = GetTextData(true);
            GN_StrSetPasteText(c_text); //复制内容到粘贴版
            GN_DeleteArray(c_text);
            zDelText(zBid, zBid2); //一并删除选中内容
        }
        return true;
    } else if(key == 22) { //粘贴
        zEraseSaveNode();
        zInputSaveOn = false;
        zAddSave(true, zSaveStartBid, zLastEndBid - zSaveStartBid, zBuf + zSaveStartBid, zBid);
        char *paste_text = (char *)GN_StrGetPasteText();
        if(paste_text) InputText(paste_text);
        zAddSave(true, zSaveStartBid, zLastEndBid - zSaveStartBid, zBuf + zSaveStartBid, zBid);
        return true;
    } else if(key == 26) { //撤消
        if(!zCancelIng) {
            zAddSave(true, zSaveStartBid, zLastEndBid - zSaveStartBid, zBuf + zSaveStartBid, zBid);
        }
        if(zSaveList.size() && zIt != zSaveList.end()) {
            zInputSaveOn = false;
            if(zIt->isAdd) {
                zDelText(zIt->bid, zIt->bid + zIt->strNum);
                zSaveStartBid = zBid2 = zBid;
            } else {
                SetPointer(zIt->bid);
                InputText(zIt->str, zIt->strNum);
                zSaveStartBid = zBid2 = zBid;
            }
            ++zIt;
            zCancelIng = true;
        }
        return true;
    } else if(key == 25) { //重做
        if(zCancelIng) {
            --zIt;
            zInputSaveOn = false;
            if(!zIt->isAdd) {
                zDelText(zIt->bid, zIt->bid + zIt->strNum);
                zSaveStartBid = zBid2 = zBid;
            } else {
                SetPointer(zIt->bid);
                InputText(zIt->str, zIt->strNum);
                zSaveStartBid = zBid2 = zBid;
            }
            if(zIt == zSaveList.begin()) zCancelIng = false;
        }
        return true;
    }
    return false;
}
void GNu::cTEXT_INPUT::zOffsetBidForErectPrint(bool is_enter, bool is_back) {
    if(zMoveTextFrame) { /*模版代码:文本输入偏移控制*/
        if(zVid >= zTextOut.GetVisualStrCount()) {
            if(!zOffsetyad && (zDlgh - zOffsety < zOffsetLasty || -zOffsety > zOffsetLasty)) {
                zOffsetxtg = -(zOffsetLastx - (short)((float)zDlgw * zTextOut.zParame.near_pointU));
                zOffsetytg = -(zOffsetLasty - (short)((float)zDlgh * zTextOut.zParame.near_pointV));
                zReOffsetLast = true;
            } else if(!zOffsetyad) {
                if(is_back) {
                    if(is_enter) zOffsetyad = -(zDlgh >> zOffsetN2);
                    else zOffsetyad = zDlgh >> zOffsetN1;
                } else {
                    if(is_enter) zOffsetyad = zDlgh >> zOffsetN2;
                    else zOffsetyad = -(zDlgh >> zOffsetN1);
                }
            }
        } else {
            GNg::sDRAW_RANGE *_rng_data = zTextOut.zDrawRngData + zVid;
            if(is_back ? (_rng_data->x > (zDlgx + zDlgw - (_rng_data->w << 1))) : (_rng_data->x < (zDlgx + (_rng_data->w << 3 - ((zBuf[_rng_data->strid] & 128) >> 7))))) {
                zOffsetxtg -= (zDlgw >> zOffsetN1);
            }
        }
    }
    zBid2 = zBid;
}
//下一位偏移控制( X或Y , 是否经过换行 , 行控制X或Y, 行宽或高有关 , 加号? , 减号? , 加号2? , 换行移位比较方法)
#define GT_TMP_TEXTIN_NEXT_BIT(XY, IS_ENTER, RXY, RWH, _T_, ___, _TT_, CONDITION)                    \
    if(zMoveTextFrame) { /*模版代码:文本输入偏移控制*/                                               \
        if(zVid >= zTextOut.GetVisualStrCount()) {                                                   \
            if(!zOffset##XY##ad && (zDlgh - zOffsety < zOffsetLasty || -zOffsety > zOffsetLasty)) {  \
                zOffsetxtg = -(zOffsetLastx - (short)((float)zDlgw * zTextOut.zParame.near_pointU)); \
                zOffsetytg = -(zOffsetLasty - (short)((float)zDlgh * zTextOut.zParame.near_pointV)); \
                zReOffsetLast = true;                                                                \
            } else if(!zOffset##XY##ad) {                                                            \
                if(IS_ENTER) zOffset##XY##ad = _T_ >> zOffsetN2;                                     \
                else zOffset##XY##ad = ___ >> zOffsetN1;                                             \
            }                                                                                        \
        } else if(CONDITION) {                                                                       \
            zOffset##RXY##tg _TT_## = (zDlg##RWH >> zOffsetN1); /*边靠位是齐整的就靠齐边筐*/         \
        }                                                                                            \
    }                                                                                                \
    zBid2 = zBid

//退一位偏移控制( X或Y , 是否经过换行 , 行控制X或Y, 行宽或高有关 , 加号? , 减号? )
#define GT_TMP_TEXTIN_BACK_BIT(XY, IS_ENTER, RXY, RWH, _T_, ___) \
    GT_TMP_TEXTIN_NEXT_BIT(XY, IS_ENTER, RXY, RWH, ___, _T_, +, zTextOut.zDrawRngData[zVid].RXY < (zDlg##RXY + (zTextOut.zDrawRngData[zVid].RWH << 1)))

void GNu::cTEXT_INPUT::zOffsetCtrlNextForErectPrint(bool is_enter) {
    GT_TMP_TEXTIN_NEXT_BIT(y, is_enter, x, w, zDlgh, -zDlgh, -, zTextOut.zDrawRngData[zVid].x > (zDlgx + zDlgw - (zTextOut.zDrawRngData[zVid].w << 3 - ((zBuf[zTextOut.zDrawRngData[zVid].strid] & 128) >> 7))));
}
void GNu::cTEXT_INPUT::zOffsetCtrlNext(bool is_enter, short ead, short ad) {
    GT_TMP_TEXTIN_NEXT_BIT(x, is_enter, y, h, ead, ad, -, zTextOut.zDrawRngData[zVid].y > (zDlgy + zDlgh - (zTextOut.zDrawRngData[zVid].h << 1)));
}
void GNu::cTEXT_INPUT::zOffsetCtrlNext4(bool is_enter) {
    if(zTextOut.zParame.erect_print) {
        if(zTextOut.zParame.overturn_print) //竖向翻转输出
            zOffsetBidForErectPrint(is_enter, false);
        else //竖向顺着输出
            zOffsetCtrlNextForErectPrint(is_enter);
    } else { //横向翻转输出
        if(zTextOut.zParame.overturn_print)
            zOffsetCtrlNext(is_enter, -zDlgw, zDlgw);
        else //横向顺着输出
            zOffsetCtrlNext(is_enter, zDlgw, -zDlgw);
    }
}
void GNu::cTEXT_INPUT::zOffsetCtrlBackForErectPrint(bool is_enter) {
    GT_TMP_TEXTIN_BACK_BIT(y, is_enter, x, w, zDlgh, -zDlgh);
}
void GNu::cTEXT_INPUT::zOffsetCtrlBack(bool is_enter, short ead, short ad) {
    GT_TMP_TEXTIN_BACK_BIT(x, is_enter, y, h, ead, ad);
}
void GNu::cTEXT_INPUT::zOffsetCtrlBack4(bool is_enter) {
    if(zTextOut.zParame.erect_print) {
        if(zTextOut.zParame.overturn_print) { //竖向翻转输出
            zOffsetBidForErectPrint(is_enter, true);
        } else //竖向顺着输出
            zOffsetCtrlBackForErectPrint(is_enter);
    } else { //横向翻转输出
        if(zTextOut.zParame.overturn_print)
            zOffsetCtrlBack(is_enter, -zDlgw, zDlgw);
        else //横向顺着输出
            zOffsetCtrlBack(is_enter, zDlgw, -zDlgw);
    }
}
void GNu::cTEXT_INPUT::zOffsetCtrlMouseSelectErectPrint(short mousem_x, short mousem_y) {
    //鼠标选择偏移控制( X或Y , 宽或高 , 补充判断 , 行控制X或Y, 行宽或高 , 靠位U或V偏移 , 比较方法 )
#define GT_TMP_TEXTIN_MOUSE_SELECT_OFS(XY, WH, ACMP, RXY, RWH, ParamUV, CMP)                                                                       \
    unsigned short bid = 0;                                                                                                                        \
    GNg::sDRAW_RANGE char_info;                                                                                                                    \
    if(zTextOut.GetVIDfromCoord(mousem_x, mousem_y, &zVid, &zVrow_id, &char_info)) { /*鼠标左键点中文字*/                                          \
        if(zBuf[char_info.strid] && zBuf[char_info.strid] != 1) { /*点中的字符非结束与插入符*/                                                     \
            if((mousem_##XY > char_info.XY + (char_info.WH >> 1)) ACMP) bid = 1 + ((zBuf[char_info.strid] & 128) >> 7); /*判断显示焦点的确切位置*/ \
        }                                                                                                                                          \
    }                                                                                                                                              \
    zSetBid(char_info.strid + bid);                                                                                                                \
    if(zMoveTextFrame) {                                                                                                                           \
        GNg::sARRANGE_FMT *_fmt = &zTextOut.zParame;                                                                                               \
        GNg::sDRAW_RANGE *_arrg = zTextOut.zDrawRngData + zVid;                                                                                    \
        if(zVid >= zTextOut.GetVisualStrCount() || _arrg->XY > (zDlg##XY + zDlg##WH - (_arrg->WH << 1))) { /*单个字符遍移是否到边界*/              \
            if(!zMousePass || zVrow_id == zTextOut.zRowNum - 1) zOffset##XY##tg -= (zDlg##WH >> zOffsetN2);                                        \
        } else if(_arrg->XY < (zDlg##XY + (_arrg->WH << 1))) {                                                                                     \
            zOffset##XY##tg += (zDlg##WH >> zOffsetN2);                                                                                            \
        }                                                                                                                                          \
        if(zVid < zTextOut.GetVisualStrCount()) { /*------- 选中行没有文字时处理 ---------------------------------------*/                         \
            if(_arrg->strid < zEid && _arrg->strid > 0 && (CMP)) /*选不到字符并选到其他行*/                                                        \
                goto GT_GOTO_TextInSetSelectOfs##XY;                                                                                               \
            else zMousePass = false;                                                                                                               \
        } else {                                                                                                                                   \
            GT_GOTO_TextInSetSelectOfs##XY : zMousePass = true;                                                                                    \
            zVmx = mousem_x, zVmy = mousem_y;                                                                                                      \
            if(mousem_##XY - zDlg##XY > (short)((float)zDlg##WH * ParamUV) + zOffset##XY) /*选到看不到的字符后面*/                                 \
                zOffset##XY##tg += (zDlg##WH >> zOffsetN2 + 1);                                                                                    \
            else /*选到看不到的字符前面*/                                                                                                          \
                zOffset##XY##tg -= (zDlg##WH >> zOffsetN2 + 1);                                                                                    \
        }                                                                                                                                          \
        /*-------------------------------------------------------------------------------------------------------------*/                          \
        if(zVid < zTextOut.GetVisualStrCount()) {                                                                                                  \
            if(_arrg->RXY > (zDlg##RXY + zDlg##RWH - (_arrg->RWH << 1))) /*行遍移是否到边界了*/                                                    \
                zOffset##RXY##tg -= (zDlg##RWH >> zOffsetN2);                                                                                      \
            else if(_arrg->RXY < (zDlg##RXY + (_arrg->RWH << 1)))                                                                                  \
                zOffset##RXY##tg += (zDlg##RWH >> zOffsetN2);                                                                                      \
        }                                                                                                                                          \
    }
    GT_TMP_TEXTIN_MOUSE_SELECT_OFS(y, h, , x, w, _fmt->near_pointV,
                                   !(_fmt->auto_enter & GNg::eENTER_TOP_BOTTOM) && (_fmt->overturn_print
                                                                                        ? (((zBuf[zBid] & 128) >> 7) ? mousem_x > _arrg->x + _arrg->w + 1 || mousem_x < _arrg->x + _arrg->w + 1 - (_fmt->i_compartment << 1) : mousem_x > _arrg->x + (_arrg->w * 3 >> 1) + 1 || mousem_x < _arrg->x + (_arrg->w * 3 >> 1) + 1 - (_fmt->i_compartment << 1))
                                                                                        : (((zBuf[zBid] & 128) >> 7) ? mousem_x < _arrg->x || mousem_x > _arrg->x + (_fmt->i_compartment << 1) : mousem_x < _arrg->x - (_arrg->w >> 1) || mousem_x > _arrg->x + (_fmt->i_compartment << 1) - (_arrg->w >> 1))))
}
void GNu::cTEXT_INPUT::zOffsetCtrlMouseSelect(short mousem_x, short mousem_y) {
    GT_TMP_TEXTIN_MOUSE_SELECT_OFS(x, w, != zTextOut.zParame.overturn_print, y, h, _fmt->near_pointU, !(_fmt->auto_enter & GNg::eENTER_LEFT_RIGHT) && (mousem_y < _arrg->y || mousem_y > _arrg->y + _fmt->j_compartment))
}
void GNu::cTEXT_INPUT::zArrangeText() {
#ifdef GN_DEBUG
    if(!zShowReady) {
        GN_Log("cTEXT_INPUT 还没指定文字输出对象 请先调用 InitStyle\n");
        return;
    }
    if(!zBuf) {
        GN_Log("cTEXT_INPUT 还没指定输入格式 请先调用 InitInputFmtToData\n");
        return;
    }
#endif
    if(zInType == eTX_IN_PASSWORD) { //是密码类型显示为"*"号 最多显示64个
        char pw[64 + 2];
        char *bptr = zBuf, *pptr = pw;
        while(*bptr) {
            if(*bptr != '\n' && *bptr != 1) *pptr = '*'; //把文本除换行符外都转成*号
            else *pptr = *bptr;
            ++pptr, ++bptr;
        }
        pw[zEid] = 1;
        pw[zEid + 1] = 0;
        zTextOut.SetText(pw, 0, zDlgx, zDlgy, zDlgw, zDlgh, zOffsetx, zOffsety);
    } else
        zTextOut.SetText(zBuf, 0, zDlgx, zDlgy, zDlgw, zDlgh, zOffsetx, zOffsety);
}
void GNu::cTEXT_INPUT::zAddSave(bool is_add, unsigned short _bid, unsigned short str_count, char *_str, unsigned short seek_bid) {
    if(!str_count || !aMaxCancelNum) {
        zSaveStartBid = seek_bid;
        return;
    }
    if((unsigned short)zSaveList.size() >= aMaxCancelNum) zSaveList.pop_back();
    zSaveList.push_front(GT_sINPUT_SAVE());
    zIt = zSaveList.begin();
    GT_sINPUT_SAVE *save = &(*zIt);
    save->bid = _bid;
    save->isAdd = is_add;
    save->strNum = str_count;
    GN_CopyArray(save->str, _str, str_count, char);
    zSaveStartBid = seek_bid;
}
void GNu::cTEXT_INPUT::UpdateRun(unsigned int updatetime) {
    if(!zShowReady || !zBuf) {
#ifdef GN_DEBUG
        GN_Log("调用 cTEXT_INPUT::UpdateRun() 前还没初始化好! 请先调用 cTEXT_INPUT::InitStyle(...) 和 cTEXT_INPUT::InitInputFmtToData(...)!\n");
#endif
        return;
    }
    if(aDisableControl) {
        if(zArgText) {
            zArrangeText();
            zArgText = false;
            zSetBid(zBid, false);
        }
        zTextOut.Show(aTextColor, aTextBlend); //不控制仅显示
        return;
    }
    zShowView = GNg::ViewCur;
    short mousem_x = zMousePass ? zVmx : (short)zShowView->mouseMX,
          mousem_y = zMousePass ? zVmy : (short)zShowView->mouseMY;
    /*-------------------------------------- 处理突出主要显示时的背景和箭头点击判断 ----------------------------------------------------------*/
    if(zShowView->zHasOrthoOfsIng) {
        if(zCurIn == this) {
            uint8 ba_o = (uint8)((float)(127.0f - (float)zBackAlpha) * 0.2f);
            if(ba_o) zBackAlpha += ba_o;
            else if(zBackAlpha != 127) ++zBackAlpha;
            int dx = zShowView->ortho_right - zRetArrow->width - 10, dy = zShowView->ortho_top + 10, dw = zRetArrow->width, dh = zRetArrow->width;
            if(GN::Mouse[GM_LEFT] &&
               zShowView->mouseMX > dx && zShowView->mouseMX < dx + dw &&
               zShowView->mouseMY > dy && zShowView->mouseMY < dy + dh) {
                mousem_x = 20000;
                SetPrimaryCancel(); //点中返回箭头就退出突出主要显示模式
            }
            GNg::ImageDraw(0, zShowView->ortho_left, zShowView->ortho_top, zShowView->orthoW, zShowView->orthoH, zBackAlpha << 24);
            GNg::ImageDraw(zRetArrow, dx, dy, dw, dh, (zBackAlpha << 25) | (aTextColor & 0x00ffffff));
        }
    } else if(zBackAlpha) {
        if(zCurIn == this) {
            uint8 ba_o = (uint8)((float)zBackAlpha * 0.2f);
            if(ba_o) zBackAlpha -= ba_o;
            else if(zBackAlpha) --zBackAlpha;
            int dx = zShowView->ortho_right - zRetArrow->width - 10, dy = zShowView->ortho_top + 10;
            GNg::ImageDraw(0, zShowView->ortho_left, zShowView->ortho_top, zShowView->orthoW, zShowView->orthoH, zBackAlpha << 24);
            GNg::ImageDraw(zRetArrow, dx, dy, zRetArrow->width, zRetArrow->height, (zBackAlpha << 25) | (aTextColor & 0x00ffffff));
        }
    } /*---------------------------------------------------------------------------------------------------------------------------------------*/
    GNg::sARRANGE_FMT *_fmt = &(zTextOut.zParame);
    zMoveTextFrame = !(zInType > eTX_IN_TEXT || _fmt->rect_fill || ((_fmt->auto_enter & GNg::eENTER_TOP_BOTTOM) && (_fmt->auto_enter & GNg::eENTER_LEFT_RIGHT)));
    if(zMoveTextFrame) {
        short mfw, mfh, max_ofs_l, max_ofs_r, max_ofs_t, max_ofs_b, last_offset, o_o;

        //处理筐内偏移量
        if(zVid < zTextOut.GetVisualStrCount()) {
            zOffsetxad = 0;
            zOffsetyad = 0;
        } else if(!zReOffsetLast) {
            zOffsetxtg += zOffsetxad;
            zOffsetytg += zOffsetyad;
        }
        //控制内容在筐内拖动的范围 -----------------------------------------------------------
        if(zTextOut.zTotRngW > zDlgw) {
            mfw = zTextOut.zTotRngW - zDlgw;
            max_ofs_r = (short)((float)(mfw)*_fmt->near_pointU);
            max_ofs_l = mfw - max_ofs_r;
            if(max_ofs_r) max_ofs_r += zDlgw >> zOffsetN1;
            if(max_ofs_l) max_ofs_l += zDlgw >> zOffsetN1;
        } else {
            max_ofs_r = 0;
            max_ofs_l = 0;
        }
        if(_fmt->overturn_print) {
            if(zOffsetxtg > max_ofs_l) zOffsetxtg = max_ofs_l;
            else if(zOffsetxtg < -max_ofs_r) zOffsetxtg = -max_ofs_r;
        } else {
            if(zOffsetxtg > max_ofs_r) zOffsetxtg = max_ofs_r;
            else if(zOffsetxtg < -max_ofs_l) zOffsetxtg = -max_ofs_l;
        }
        if(zTextOut.zTotRngH > zDlgh) {
            mfh = zTextOut.zTotRngH - zDlgh;
            max_ofs_b = (short)((float)(mfh)*_fmt->near_pointV);
            max_ofs_t = mfh - max_ofs_b;
            if(max_ofs_b) max_ofs_b += zDlgh >> zOffsetN1;
            if(max_ofs_t) max_ofs_t += zDlgh >> zOffsetN1;
        } else {
            max_ofs_b = 0;
            max_ofs_t = 0;
        }
        if(zOffsetytg > max_ofs_b) zOffsetytg = max_ofs_b;
        else if(zOffsetytg < -max_ofs_t) zOffsetytg = -max_ofs_t;

#define GT_TMP_SLINEAR_OFFSET(XY)                                                                           \
    last_offset = zOffset##XY;                                                                              \
    o_o = (short)((float)(zOffset##XY##tg - zOffset##XY) * zOffsetSheep);                                   \
    if(o_o) zOffset##XY += o_o;                                                                             \
    else if(zOffset##XY##tg != zOffset##XY) zOffset##XY += (short)(zOffset##XY##tg > zOffset##XY ? 1 : -1); \
    if(zOffset##XY != last_offset) zArgText = true;

        GT_TMP_SLINEAR_OFFSET(x) //计算X偏移量的插值
        GT_TMP_SLINEAR_OFFSET(y) //计算y偏移量的插值
        if(!zArgText) zReOffsetLast = false;
    } /*---------------------------------------------------------------------------------------------------------------------------------------*/

    if(zfAm_s_e) zfNowtime -= updatetime; // --------------------------------------- 处理焦点颜色闪动
    else zfNowtime += updatetime;
    if(zfNowtime > 200) {
        zfAm_s_e = 1;
        zfNowtime = 200;
    } else if(zfNowtime < 0) {
        zfAm_s_e = 0;
        zfNowtime = 0;
    }
    unsigned int fc = GNg::LinearColorMultiplyRatio(0x20ffffff, 0xffffffff, zfNowtime, 200, aFocusColor);

    //处理文本输入运行 --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
    if(zMousePass || GN::Mouse[GM_LEFT]) { //处理鼠标点击
        if(!zLastMK) {
            if(mousem_x > zDlgx && mousem_x < zDlgx + zDlgw && mousem_y > zDlgy && mousem_y < zDlgy + zDlgh) { //鼠标点中文本筐
                if(!zCurIn) {
                    if(!aLock && GN::TouchZ[0] && !zImeIn) SetPrimary(0.5f, 0.1f, false, true); //在小屏的设备系统时需自动开启突出主要显示模式 以免被软键盘遮挡了输入
                }
                zCurIn = this;
                zImeIn = true;
                if(_fmt->erect_print) zOffsetCtrlMouseSelectErectPrint(mousem_x, mousem_y);
                else zOffsetCtrlMouseSelect(mousem_x, mousem_y);
            } else {
                if(zCurIn == this) {
                    if(GN::TouchZ[0] && !zMousePass) SetPrimaryCancel();
                }
                zImeIn = zMousePass;
                zMousePass = false;
                zCurIn = 0;
            }
            zBid2 = zBid;
        } else if(zImeIn && (zMousePass || GN::MouseM || GN::Mouse[GM_TOP] || GN::Mouse[GM_BOTTOM] || (mousem_x < zDlgx) || (mousem_x > zDlgx + zDlgw) || (mousem_y < zDlgy) || (mousem_y > zDlgy + zDlgh))) {
            short fx = zOffsetLastx, fy = zOffsetLasty;
            if(_fmt->erect_print) zOffsetCtrlMouseSelectErectPrint(mousem_x, mousem_y);
            else zOffsetCtrlMouseSelect(mousem_x, mousem_y);
            zOffsetLastx = fx;
            zOffsetLasty = fy;
        }
    }
    zLastMK = GN::Mouse[GM_LEFT];

    if(zImeIn) { /*-------------------------------------------------------------------------------------------------------------------------------------*/
        GT_ImeOn = (!aLock || GN::GetTouchMaxNum() < 2);
        if(zMoveTextFrame) {
            if(!GN::Mouse[GM_RIGHT]) zKeyOnDown = false;
            if(GN::Mouse1[GM_RIGHT] && IsMouseFocus()) zKeyOnDown = true;
            if(zKeyOnDown && GN::MouseM) { //按住鼠标右键拖动
                if(_fmt->erect_print) zOffsetxtg += (short)zShowView->mouseMSX;
                else zOffsetytg += (short)zShowView->mouseMSY;
            } else if(GN::Mouse[GM_TOP]) zOffsetytg += zDlgh >> zOffsetN1;
            else if(GN::Mouse[GM_BOTTOM]) zOffsetytg -= zDlgh >> zOffsetN1;
        }
        unsigned short k_back, k_next, k_top_row, k_bottom_row;
        if(_fmt->erect_print) { // ------ 处理键盘控制 按不同输出类型交换键位
            if(_fmt->overturn_print) {
                k_top_row = GK_RIGHT;
                k_bottom_row = GK_LEFT; //垂直翻转输出
            } else {
                k_top_row = GK_LEFT;
                k_bottom_row = GK_RIGHT; //垂直顺向输出
            }
            k_back = GK_UP;
            k_next = GK_DOWN;
        } else {
            if(_fmt->overturn_print) {
                k_back = GK_RIGHT;
                k_next = GK_LEFT; //水平翻转输出
            } else {
                k_back = GK_LEFT;
                k_next = GK_RIGHT; //水平顺向输出
            }
            k_top_row = GK_UP;
            k_bottom_row = GK_DOWN;
        }
        if(GN::Key11[k_back]) { //退一位
            if(zBid) {
                unsigned short is_enter = (unsigned short)(zBuf[zBid - 1] == '\n');
                zSetBid(zBid - 1 - ((zBuf[zBid - 1] & 128) >> 7 || is_enter));
                zOffsetCtrlBack4(is_enter);
            }
        }
        if(GN::Key11[k_next]) { //下一位
            if(zBid < zEid) {
                unsigned short is_enter = (unsigned short)(zBuf[zBid] == 1);
                if(!zOffsetxad || !is_enter) zSetBid(zBid + 1 + ((zBuf[zBid] & 128) >> 7 || is_enter));
                zOffsetCtrlNext4(is_enter);
            }
        }
        if(GN::Key11[k_top_row] || GN::Key11[k_bottom_row]) {
            if(zVid < zTextOut.GetVisualStrCount()) { //上一行或下一行
                short fx = zOffsetLastx, fy = zOffsetLasty;
                if((_fmt->erect_print
                        ? (zOffsetx - zOffsetxtg < _fmt->i_compartment && zOffsetx - zOffsetxtg > -_fmt->i_compartment)
                        : (zOffsety - zOffsetytg < _fmt->j_compartment && zOffsety - zOffsetytg > -_fmt->j_compartment))) {
                    GNg::sDRAW_RANGE *_arrg = zTextOut.zDrawRngData + zVid;
                    int offset;
                    if(_fmt->erect_print) { //计算一个虚以坐标来摸以在这里点选上一行或下一行来获得移动的新焦点
                        offset = (_arrg->w >> ((zBuf[zBid] & 128) >> 7)) * ((GN::Key11[k_top_row] ? -_fmt->i_compartment : _fmt->i_compartment) << 1) / _fmt->width;
                        if(_fmt->overturn_print) offset = _arrg->x - offset;
                        else offset = _arrg->x + offset;
                        mousem_y = zOffsetLasty + zOffsety + zDlgy;
                        mousem_x = offset + 2;
                        zOffsetCtrlMouseSelectErectPrint(mousem_x, mousem_y);
                    } else {
                        offset = _arrg->y + _arrg->h * (GN::Key11[k_top_row] ? -_fmt->j_compartment : _fmt->j_compartment) / _fmt->height;
                        mousem_x = zOffsetLastx + zOffsetx + zDlgx;
                        mousem_y = offset + 2;
                        zOffsetCtrlMouseSelect(mousem_x, mousem_y);
                    }
                }
                if(GN::Key11[k_top_row]) zOffsetCtrlBack4(true);
                else zOffsetCtrlNext4(true);
                zOffsetLastx = fx;
                zOffsetLasty = fy;
            } else if(zMoveTextFrame) {
                if(zDlgh - zOffsety < zOffsetLasty || -zOffsety > zOffsetLasty) {
                    zOffsetxtg = -(zOffsetLastx - (short)((float)zDlgw * _fmt->near_pointU));
                    zOffsetytg = -(zOffsetLasty - (short)((float)zDlgh * _fmt->near_pointV));
                    zReOffsetLast = true;
                }
            }
        }
        if(GN::Key11[GK_DELETE]) {
            if(zBid == zBid2) {
                if(zBid < zEid) zDelText(zBid, zBid + 1 + ((zBuf[zBid] & 128) >> 7 || (unsigned short)(zBuf[zBid] == 1)));
            } else {
                zDelText(zBid, zBid2);
                if(zVid >= zTextOut.GetVisualStrCount() && zMoveTextFrame) {
                    zOffsetxtg = -(zOffsetLastx - (short)((float)zDlgw * _fmt->near_pointU));
                    zOffsetytg = -(zOffsetLasty - (short)((float)zDlgh * _fmt->near_pointV));
                    zReOffsetLast = true;
                }
            }
            zBid2 = zBid;
        }
        /*--------------------------------------------------------------------------------------*/
        if(GT_ImeCount) InputText(GT_ImeChar, GT_ImeCount); //处理文字输入
        if(zArgText) {
            zArrangeText();
            zArgText = false;
            zSetBid(zBid, false);
        }

        if(zVid < zTextOut.GetVisualStrCount()) { /*---------------------------------------------*/
            if(aFocusColor & 0xff000000) { //显示输入焦点
                GNg::sDRAW_RANGE *focus_range = zTextOut.zDrawRngData + zVid;
                int fx = focus_range->x;
                int fy = focus_range->y;
                int fw = focus_range->w;
                int fh = focus_range->h;
                if(_fmt->erect_print) { //处理插入符在不同排法的形状变化
                    fh = fh < 0 ? (zDlgh >> 2) + 1 : (fh >> 2) + 1;
                } else {
                    if(_fmt->overturn_print) {
                        if(fw < 0) fw = zDlgw;
                        int fx2 = fx + fw;
                        fw = (fw >> 2) + 1;
                        fx = fx2 - fw;
                    } else {
                        fw = fw < 0 ? (zDlgw >> 2) + 1 : (fw >> 2) + 1;
                    }
                }
                if(!aLock) GNg::ImageDraw(0, fx, fy, fw, fh, fc, 0, aFocusBlend); //显示插入符
                if(zCurIn == this)
                    GN_SetImeFouscePos((short)((fx - zShowView->ortho_left) / zShowView->zScreenRatioW), (short)((fy + fh + 50 - zShowView->ortho_top) / zShowView->zScreenRatioH));
            }
        }
    } else {
        if(zArgText) {
            zArrangeText();
            zArgText = false;
            zSetBid(zBid, false);
        }
    }
    //-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
    zInputSaveOn = true;
    //GNg::ImageDraw(0,zOffsetLastx+ zOffsetx + zDlgx,zOffsetLasty+ zOffsety + zDlgy,5,5,0xffffff00,0,1); //显示选择范围的另外一点

    if(!aLock && zBid != zBid2) { // 处理选择范围----------------------------------------------------------------------------------
        unsigned short min_id = 0, max_id = 0;
        uint8 min_row_id = 0, max_row_id = 0;
        if(zBid < zBid2) {
            zTextOut.GetVIDfromText(zBid, &min_id, &min_row_id);
            zTextOut.GetVIDfromText(zBid2, &max_id, &max_row_id);
        } else {
            zTextOut.GetVIDfromText(zBid2, &min_id, &min_row_id);
            zTextOut.GetVIDfromText(zBid, &max_id, &max_row_id);
        }
        if(min_id < max_id) { // 选择范围显示 -------------------------------------------------------------------------------------------------------------
            unsigned int sel_color = aFocusColor;
            ((uint8 *)&sel_color)[3] >>= 1; //使焦点颜色减淡一些

            GNg::sDRAW_RANGE *_arrg, *_arrg2, *_dw_rng = zTextOut.zDrawRngData;
            unsigned short *row_sids = zTextOut.zRowSIDs, ri, rj;
            int rx, ry, rw, rh;
            GNg::ViewCur->ScissorRectSetup(zDlgx, zDlgy, zDlgw, zDlgh);
            GT_DrawTextSelectRange(((zBuf[_arrg2->strid] & 128) >> 7), ((zBuf[_arrg->strid] & 128) >> 7), GNg::ImageDraw(0, rx, ry, rw, rh, sel_color, 0, aFocusBlend);)
                GNg::ViewCur->ScissorRectRemove();
        }
    }
    zTextOut.Show(aTextColor, aTextBlend); //显示文本内容------------------------------------------------------------------------------------------------------
    /*
	GN_RunValueCheck2(zBid, 23);
	GN_RunValueCheck2(zBid2, 24);
	GN_RunValueCheck2(zEid, 25);
	GN_RunValueCheck2(zLastEndBid, 26);
	GN_RunValueCheck2(zSaveStartBid, 27);
	GN_RunValueCheck2(zSaveList.size(), 28);
	GN_RunValueCheck2(zTextOut.GetVisualStrCount(), 29);
	*/
}
#ifndef GN_USING_ADDITIONAL_LIB //GN_ADD_CONTEN_PRIVATE
void GNu::cTEXT_INPUT::SetText(const char *text, unsigned short len) {
    last_imein = zImeIn;
    InitInputFmtToData(zInType, text, zMaxInNum);
    zImeIn = last_imein;
    zOffsetN1 = 2;
    zOffsetN2 = 4;
}
void GNu::cTEXT_INPUT::SetFocus() {
    zShowView->mouseMX = zDlgx + 1;
    zShowView->mouseMY = zDlgy + 1;
    GN::Mouse[GM_LEFT] = true;
    aDisableControl = false;
    UpdateRun(0);
    GN::Mouse[GM_LEFT] = false;
    SetPointer(65535);
}
#endif
void GNu::cTEXT_INPUT::CopyData(const cTEXT_INPUT &src) {
    this->Release();
    InitStyle(&(src.zTextOut), src.zDlgx, src.zDlgy, src.zDlgw, src.zDlgh, src.aTextColor, src.aFocusColor, src.aTextBlend, src.aFocusBlend);
    zRetArrow = src.zRetArrow;
    char *cnt = ((cTEXT_INPUT *)&src)->GetTextData();
    InitInputFmtToData(((cTEXT_INPUT *)&src)->GetInputType(), cnt, src.zMaxInNum);
    delete[] cnt;
    SetOffsetEF(src.zOffsetSheep, src.zOffsetN1, src.zOffsetN2);
    ((cTEXT_INPUT *)&src)->zArgText = true;
}
P<GNu::iUI_OBJECT> GNu::cUI_TEXT_INPUT::Clone() {
    P<cUI_TEXT_INPUT> new_clone(1);
    cUI_TEXT_INPUT *_new_clone = new_clone;
    *((cUI_NEAR_OBJECT *)_new_clone) = *((cUI_NEAR_OBJECT *)this);
    _new_clone->CopyData(*((cTEXT_INPUT *)this));
    return new_clone;
}
void GNu::cUI_TEXT_INPUT::UpdateRun(unsigned int updatetime, sUI_LEVEL *parent) {
    c = aTextColor;
    GNg::sDRAW_AREA2 rng;
    DrawRangeGet(rng, parent);
    aTextColor = rng.c;
    unsigned int fcr = aFocusColor;
    aFocusColor = GNg_MultiplyColorRatio(fcr, parent->c);
    if(zDlgx != rng.x || zDlgy != rng.y || zDlgw != rng.w || zDlgh != rng.h) {
        zDlgx = rng.x;
        zDlgy = rng.y;
        zDlgw = rng.w;
        zDlgh = rng.h;
        zArgText = true; //SetPos
    }
    if(RecvInputUiObject != this) zShowView->mouseMX += 10000;
    cTEXT_INPUT::UpdateRun(updatetime);
    if(RecvInputUiObject != this) zShowView->mouseMX -= 10000;
    aTextColor = c;
    aFocusColor = fcr;
}
unsigned int GNu::cUI_TEXT_INPUT::DataSave(cSHARE_DATA *share_objs, uint8 *data_out) {
    uint8 loc_buf[65536 + 64];
    if(!data_out) data_out = loc_buf;
    uint8 *wpos = data_out;
    *wpos = 1;
    ++wpos; //版本
    wpos = zNearAreaDataSave(wpos);
    wpos = cTEXT_INPUT::zDataSave(wpos, share_objs->image_list);
    return wpos - data_out;
}
unsigned int GNu::cUI_TEXT_INPUT::DataLoad(cSHARE_DATA *share_objs, const uint8 *data) {
    uint8 *rpos = (uint8 *)data;
    uint8 version = *rpos;
    ++rpos;
    if(version > 1) return 0; //版本
    rpos = zNearAreaDataLoad(rpos);
    rpos = cTEXT_INPUT::zDataLoad(rpos, share_objs->image_refs);
    aTextColor = c;
    return rpos - data;
}
uint8 *GNu::cTEXT_INPUT::zDataSave(uint8 *wpos, GN::sLIST<GNg::sIMAGE *> &img_collect) {
    *wpos = (aLock ? 1 : 0) | (aDisableControl ? 2 : 0);
    wpos += 1;
    *wpos = zOffsetN1;
    wpos += 1;
    *wpos = zOffsetN2;
    wpos += 1;
    memcpy(wpos, &zInType, 1);
    wpos += 1;
    memcpy(wpos, &zMaxInNum, 2);
    wpos += 2;
    memcpy(wpos, &aTextBlend, 2);
    wpos += 2;
    memcpy(wpos, &aFocusBlend, 2);
    wpos += 2;
    memcpy(wpos, &aFocusColor, 4);
    wpos += 4;
    memcpy(wpos, &zOffsetSheep, 4);
    wpos += 4;
    char *cnt = GetTextData();
    unsigned int tsize = 0;
    if(cnt) {
        tsize = strlen(cnt) + 1;
        memcpy(wpos, &tsize, 2);
        wpos += 2;
        memcpy(wpos, cnt, tsize);
        wpos += tsize;
        delete[] cnt;
    } else {
        memcpy(wpos, &tsize, 2);
        wpos += 2;
    }
    wpos = zTextOut.zDataSave(wpos, img_collect);
    return wpos;
}
uint8 *GNu::cTEXT_INPUT::zDataLoad(uint8 *rpos, P<GNg::sIMAGE *> &img_refs) {
    aLock = ((*rpos) & 1) ? true : false;
    aDisableControl = ((*rpos) & 2) ? true : false;
    rpos += 1;
    zOffsetN1 = *rpos;
    rpos += 1;
    zOffsetN2 = *rpos;
    rpos += 1;
    zInType = (eTX_IN_TYPE)(*rpos);
    rpos += 1;
    memcpy(&zMaxInNum, rpos, 2);
    rpos += 2;
    memcpy(&aTextBlend, rpos, 2);
    rpos += 2;
    memcpy(&aFocusBlend, rpos, 2);
    rpos += 2;
    memcpy(&aFocusColor, rpos, 4);
    rpos += 4;
    memcpy(&zOffsetSheep, rpos, 4);
    rpos += 4;
    unsigned short tsize;
    memcpy(&tsize, rpos, 2);
    rpos += 2;
    char *cnt = (char *)rpos;
    uint8 *after_text = rpos + tsize;
    uint8 saved_byte = *after_text; // 保存被覆盖的字节
    *after_text = 0; // 空终止文本
    rpos = after_text;
    rpos = zTextOut.zDataLoad(rpos, img_refs);
    zShowReady = true;
    InitInputFmtToData(zInType, tsize ? cnt : 0, zMaxInNum);
    *after_text = saved_byte; // 恢复原字节
    return rpos;
}
#endif //_GN_UITEXTIN_H_
