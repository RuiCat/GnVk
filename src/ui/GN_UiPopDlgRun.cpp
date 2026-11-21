// GN_UiPopDlgRun.cpp — GNu::cSIM_DIALOG::UpdateRun 状态机（绘制+输入）

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
// code-modularity: no-split — 单个 cSIM_DIALOG::UpdateRun 状态机（绘制+输入），本体 1017 有效行（整文件 1036，其余为版权头按行计数的部分）；拆分需重写输入/布局状态变量，登记技术债，独立批次处理
#ifdef _GN_UIPOPDLG_H_
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
                                // B5f：pointer 容量 = maxValue，按上限截断。
                                GN_StrCopyToLimit((char *)_chd->pointer, (int)_chd->maxValue, sf_info[0]->global_filename);
                                GT_ChildTextValueSetShowStr(_chd); //简短显示文本更新
                            } else if(_chd->name[0] == '=') {
                                // B5f：pointer 容量 = maxValue，按上限截断。
                                GN_StrCopyToLimit((char *)_chd->pointer, (int)_chd->maxValue, fb->aOpenDir);
                                GT_ChildTextValueSetShowStr(_chd); //简短显示文本更新
                            }
                            if(zDetailSetState == eDETAIL_SET_PATH_SELECTF) zNowtime = 0;
                            set_text_ok = 1;
                        }
                    } else {
                        char *value_str = _text_in->GetTextData(); //输入确定好就获取数据
                        if(value_str) {
                            // B5f：pointer 容量 = maxValue，按上限截断。
                            GN_StrCopyToLimit((char *)_chd->pointer, (int)_chd->maxValue, value_str); //复制编辑文本到修改文本缓冲里实现赋值
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

                if(!color_chg && GN::GetImeContext().aImeCount && _text_in->IsInputing()) { //有颜色值输入更改
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
#endif //_GN_UIPOPDLG_H_
