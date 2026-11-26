// GN_mini UI 实现片段：cSELECT_LIST_ICON / cSELECT_LIST_TEXT 选择列表。
// 2026-11-26 自 GN_UiBase.cpp 第 463-713 行 move-only 拆出（REFACTOR_PLAN §13 B4）。
// 由 GN_UiBase.cpp 按原顺序包含（单编译单元，static/宏语义不变）；不是独立头文件。
bool GNu::cSELECT_LIST_ICON::zSelectProc(unsigned int zid, bool has_focus) {
    bool draw;
    if(has_focus) { /*模版代码:选择列表选择处理*/
        if(zFocusLastID != zid) {
            zFocusLastID = zid;
            zIsFocusFrameShow = true;
        }
        draw = zIsFocusFrameShow;
        if(GN::MouseI[GM_LEFT]) { //选择处理
            if(zMultSelect) { //多选
                if(zid < 32) {
                    if(zMsLastState && (GN::Key[GK_LSHIFT] || GN::Key[GK_RSHIFT])) { //快捷键段选
                        if(zMsLastState == 1) //加段
                            aSelected |= (zMsLastID < zid) ? (((1 << (zMsLastID)) - 1) ^ ((1 << (zid + 1)) - 1)) : (((1 << (zid)) - 1) ^ ((1 << (zMsLastID + 1)) - 1));
                        else if(zMsLastState == 2) //减段
                            aSelected &= ~((zMsLastID < zid) ? (((1 << (zMsLastID)) - 1) ^ ((1 << (zid + 1)) - 1)) : (((1 << (zid)) - 1) ^ ((1 << (zMsLastID + 1)) - 1)));

                        zMsLastState = 0;
                    } else {
                        unsigned int inv = 1 << zid;
                        if(aSelected & inv) {
                            aSelected -= inv;
                            zIsFocusFrameShow = false;
                            zMsLastState = 2;
                        } else {
                            aSelected += inv;
                            zIsFocusFrameShow = true;
                            zMsLastState = 1;
                        }
                    }
                    zMsLastID = zid;
                }
            } else aSelected = zid + 1; //单选
        }
        return draw;
    } else { //高亮显示其它已被选中的选项
        if(zMultSelect) return aSelected & (1 << zid);
        else return aSelected == zid + 1;
    }
}
void GNu::cSELECT_LIST_ICON::zReSelectState() {
    if(zFFrames->zActFrame) zFFrames->ControlActFromMouseAuto(false); /*如是可多选并图标有动态变化就取消自动变化*/
    zMsLastState = 0;
    zSelectedLast = 0;
    aSelected = 0;
    zIsFocusFrameShow = true;
    zFocusLastID = 0xffffffff;
}
void GNu::cSELECT_LIST_ICON::SetMultSelect(bool yes) {
    zMultSelect = yes;
    zReSelectState();
    if(zMultSelect && zZoningNum > 32) {
        zZoningNum = 32;
        GNu::cSCROLL_LIST::RePos();
    }
}
bool GNu::cSELECT_LIST_ICON::InitStyleToData(P<GNg::sIMAGE> icons, bool mult_select, short _x, short _y, short _w, short _h, bool default_am, short dw, short dh, unsigned int focus_color, unsigned short focus_blend) {
    GNg::cFRAME_LAUNCHER_FONT *_fframe = zFFrames;
    GNg::sIMAGE *_icons = icons;
    if(_icons) zZoningNum = (unsigned int)_icons->texnum;
    else if(_fframe && _fframe->zFontFrame && _fframe->zFontFrame->zPlay_obj)
        zZoningNum = (unsigned int)_fframe->zFontFrame->zPlay_obj->texnum;
    else {
#ifdef GN_DEBUG
        GN_Log("调用 cSELECT_LIST_ICON::InitStyleToData 失败! 还没有指定图标图象!\n");
#endif
        zZoningNum = 0;
        return false;
    }
    zIcons = icons;
    zMultSelect = mult_select;

    if(zZoningNum > 128) { //检察图标数是否超出限制
        GN_Log("调用 cSELECT_LIST_ICON::InitStyleToData 失败! 图标数不能 > 128!!\n" /*"cSELECT_LIST_ICON::InitStyleToData faile icon count > 128!!\n"*/);
        zZoningNum = 128;
    }
    if(zMultSelect && zZoningNum > 32) {
        GN_Log("调用 cSELECT_LIST_ICON::InitStyleToData 失败! 多选模式时图标数不能 > 32!!\n");
        zZoningNum = 32;
    }
    if(!_w) _w = (short)(_icons ? _icons->width : _fframe->zFontFrame->zPlay_obj->width);
    if(!dw) dw = _w;
    if(!dh) dh = _w;
    zInternalImg = _icons;
    bool ret = cSCROLL_LIST::InitStyleToData(0, _x, _y, _w, _h, default_am, dw, dh, zZoningNum, focus_color, focus_blend);
    cSCROLL_LIST::SetZoningDrawCall(GT_SelectListIconDrawCall, this);
    zInternalImg = 0;
    aShowBG = true;
    zReSelectState();
    return ret;
}
unsigned int GNu::cSELECT_LIST_ICON::UpdateRun(unsigned int updatatime, unsigned int dc) {
#ifdef GT_TMP_COMPATI_DX_DDRAW
    if(!GN_Has3DAccelerate() && aSelectFrameStyle == eRECT_DRAW_FILL) aSelectFrameStyle = eRECT_DRAW_WIRE; //在不带透明的图形环境下不使用实色填充
#endif
    if(zMultSelect && zIsMouseFocus && GN::Key1[GK_A] && (GN::Key[GK_LCONTROL] || GN::Key[GK_RCONTROL])) {
        aSelected = (1 << zZoningNum) - 1; // 快捷键全选
        zIsFocusFrameShow = true;
    }
    unsigned int ret = cSCROLL_LIST::UpdateRun(updatatime, aType == eUI_SELECT_LIST_TEXT, dc);
    if(ret && zFFrames->zActFrame) { //处理图标活动变化
        GNg::cFRAME_LAUNCHER_FONT *_fframe = zFFrames;
        GNg::sDRAW_RANGE *rng_data = _fframe->zDrawRngData;
        unsigned int fi = 0, fn = _fframe->zMax_num, inv;
        if(zReArrange || zSelectedLast != aSelected) {
            if(zMultSelect) {
                if(zReArrange) {
                    for(; fi < fn; ++fi) {
                        inv = (unsigned int)(1 << rng_data[fi].strid);
                        if(inv & aSelected) _fframe->SelectActChange(fi);
                        else _fframe->CancelActChange(fi);
                    }
                } else {
                    for(; fi < fn; ++fi) {
                        inv = (unsigned int)(1 << rng_data[fi].strid);
                        if((inv & zSelectedLast) != (inv & aSelected)) {
                            if(inv & aSelected) _fframe->SelectActChange(fi);
                            else _fframe->CancelActChange(fi);
                        }
                    }
                }
            } else {
                for(; fi < fn; ++fi) {
                    inv = (unsigned int)(rng_data[fi].strid + 1);
                    if(inv == aSelected) _fframe->SelectActChange(fi);
                    else if(inv == zSelectedLast) _fframe->CancelActChange(fi);
                }
            }
        }
    }
    zSelectedLast = aSelected;
    return ret;
}
void GT_SendSelectChangeAction(GNu::cSELECT_LIST_ICON *sel_icon, unsigned int last_sel, unsigned int sel) {
    if(sel_icon->IsMultSelect()) {
        unsigned int i = 1, bit = 1;
        for(; i <= 32; ++i, bit <<= 1) {
            if((last_sel & bit) != (sel & bit)) GNu::UiActionCall(sel_icon, (sel & bit) ? i : -i);
        }
    } else {
        if(sel) GNu::UiActionCall(sel_icon, sel);
        if(last_sel) GNu::UiActionCall(sel_icon, -last_sel);
    }
}
void GNu::cSELECT_LIST_ICON::UpdateRun(unsigned int updatetime, sUI_LEVEL *parent) {
    unsigned int last_sel = aSelected;
    GT_TMP_SCROLL_LIST_UPDATE(UpdateRun(updatetime, parent->c), if(last_sel != aSelected) GT_SendSelectChangeAction(this, last_sel, aSelected);)
}
P<GNu::iUI_OBJECT> GNu::cSELECT_LIST_ICON::Clone() {
    P<cSELECT_LIST_ICON> ui_scr_list(1);
    *ui_scr_list = *this;
    ui_scr_list->zSpecClone();
    return ui_scr_list;
}
/***************************************************** cSELECT_LIST_TEXT *************************************************************************/
void GNu::cSELECT_LIST_TEXT::zOnReArranged(unsigned short sp, unsigned short sp_last, bool ret_am) {
    GNg::cFRAME_LAUNCHER_FONT *_fframe = zFFrames;
    if(_fframe->zActFrame && sp != sp_last && !ret_am) {
        GNg::sDRAW_RANGE *rng_data = _fframe->zDrawRngData;
        unsigned int act_end_time = _fframe->zActFrame->zEndtime, fi = 0, fn = _fframe->zMax_num;
        unsigned char *act_state_d = _fframe->zActState_s;
        unsigned int *act_time_d = _fframe->zActTime_s;
        for(; fi < fn; ++fi) {
            if(aSelected == (unsigned int)(rng_data[fi].strid + 1)) { //满腹激活动画
                _fframe->SelectActChange(fi);
                act_time_d[fi] = act_end_time;
            } else { //取消激活动画
                act_time_d[fi] = 0;
                act_state_d[fi] = 0;
            }
        }
    }
}
/*static*/ void GT_SelectListTextDrawCall(int x, int y, int w, int h, unsigned int color, unsigned int zid, unsigned int vid, bool has_focus, void *arg) {
    GNu::cSELECT_LIST_TEXT *sl = (GNu::cSELECT_LIST_TEXT *)arg;
    GT_SELECT_LIST_DrawText(sl->zTextOut, sl->zLabels[zid]);
    GT_SELECT_LIST_ProcAndDrawFocus(zSelectProc(zid, has_focus), zArrowAm.aLevel_color, zFocusImg);
}
static const char *GT_ERR_SelListTextInitFail = "调用 cSELECT_LIST_TEXT::InitStyleToData 失败! %s!\n";

bool GNu::cSELECT_LIST_TEXT::InitStyleToData(P<P<char>> labels, const GNg::cTEXT_ART *text_style, unsigned int text_color, bool mult_select, short _x, short _y, short _w, short _h, bool default_am, short dw, short dh, unsigned int focus_color, unsigned short focus_blend) {
    if(text_style) zTextOut = (GNg::cTEXT_ART *)text_style;
    else if(GN::RunRec.zGWPrintReady) {
        GN::RunRec.aFont.Release(); //如调试信息已开启 也可用调试信息的文本式样来显示
        zTextOut = &GN::RunRec.aFont;
    } else {
        zZoningNum = 0;
        GN_Log(GT_ERR_SelListTextInitFail, "还没指定文本式样");
        return false;
    }
    if(labels) {
        zLabels = labels;
        zZoningNum = labels.Num();
    } else {
        zZoningNum = 0;
#ifdef GN_DEBUG
        GN_Log(GT_ERR_SelListTextInitFail, "还没指定标签");
#endif
        return false;
    }
    aTextColor = text_color ? text_color : GT_AppDefaultColorFont;
    zMultSelect = mult_select;
    if(mult_select && zZoningNum > 32) { //检测图标数是否超出限制
        GN_Log(GT_ERR_SelListTextInitFail, "多选模式下图标数不能 > 32");
        zZoningNum = 32;
    }
    // 使能自动适应宽高 ------------------------------------------------------------
    GNg::sARRANGE_FMT &tfmt = zTextOut->zParame;
    if(!_w || !_h) {
        unsigned int str_num, max_str_num = 0, li = 0, ln = zLabels.Num();
        for(; li < ln; ++li) { //统计最长的标签有几个字符
            str_num = zLabels[li].Num();
            if(str_num > max_str_num) max_str_num = str_num;
        }
        if(tfmt.erect_print) {
            if(!_w) _w = tfmt.i_compartment * (short)zLabels.Num() + 1;
            if(!_h) _h = tfmt.j_compartment * (short)max_str_num + 1;
        } else {
            if(!_w) _w = tfmt.i_compartment * (short)max_str_num + 1;
            if(!_h) _h = tfmt.j_compartment * (short)zLabels.Num() + 1;
        }
    }
    if(!dw) {
        if(tfmt.erect_print) dw = tfmt.i_compartment + 1;
        else dw = _w;
    }
    if(!dh) {
        if(tfmt.erect_print) dh = _h;
        else dh = tfmt.j_compartment + 1;
    }
    // 初始化继承类相关数据 -----------------------------------------------------------------------------------
    bool ret = cSCROLL_LIST::InitStyleToData(0, _x, _y, _w, _h, default_am, dw, dh, zZoningNum, focus_color, focus_blend);
    cSCROLL_LIST::SetZoningDrawCall(GT_SelectListTextDrawCall, this);
    //zArrowAm.zFrame_ts.x -= 50;
    zArrowAm.aFrame_ts.sx = (float)tfmt.height / (float)zOffsetArrow->width;
    zReSelectState();
    return ret;
}
void GNu::cSELECT_LIST_TEXT::SetLables(P<P<char>> labels) {
    zLabels = labels;
    zZoningNum = labels.Num();
    if(zMultSelect && zZoningNum > 32) zZoningNum = 32; //检测图标数是否超出限制
    zReSelectState();
    RePos();
}
P<GNu::iUI_OBJECT> GNu::cSELECT_LIST_TEXT::Clone() {
    P<cSELECT_LIST_TEXT> ui_scr_list(1);
    *ui_scr_list = *this;
    ui_scr_list->zSpecClone();
    return ui_scr_list;
}
