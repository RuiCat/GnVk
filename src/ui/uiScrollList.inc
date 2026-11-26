// GN_mini UI 实现片段：cSCROLL_LIST 滚动列表。
// 2026-11-26 自 GN_UiBase.cpp 第 192-462 行 move-only 拆出（REFACTOR_PLAN §13 B4）。
// 由 GN_UiBase.cpp 按原顺序包含（单编译单元，static/宏语义不变）；不是独立头文件。
/***************************************************** cSCROLL_LIST **************************************************************/
P<GNg::cFRAME> GNu::cSCROLL_LIST::zDefaultFrameAct;
GNg::sIMAGE *GNu::cSCROLL_LIST::zInternalImg = 0;
unsigned short GNu::cSCROLL_LIST::zCreateTotalNum = 0;

GNg::cFRAME *GNu::cSCROLL_LIST::CheckInitDefaultFrameAct() {
    if(zDefaultFrameAct) return zDefaultFrameAct;
    GNg::sAM_EVENT *fev = new GNg::sAM_EVENT[1];
    fev->Init(GNg::eEVENT_SIZE_X | GNg::eEVENT_SIZE_Y, 0, 120, 1.0f, 1.12f);
    zDefaultFrameAct.New(1);
    zDefaultFrameAct->Init(0, 1, fev GT_TMP_DEF_FRAME_INIT_ARGS_ONCE_KEEP);
    return zDefaultFrameAct;
}
GNu::cSCROLL_LIST::cSCROLL_LIST() : aShowBG(false), aShowInFrame(true), zDrawCall(0), zDrawCallHasUsArg(false), zIsErectArrange(true), zUsArg(0) {
    aType = GNu::eUI_SCROLL_LIST;
    c = 0xffffffff /*该类忽略该值*/;
    ++zCreateTotalNum; /*创建增加计数*/
}
GNu::cSCROLL_LIST::~cSCROLL_LIST() {
    --zCreateTotalNum;
    if(!zCreateTotalNum) { //已没其他对象存活就删除共享内容
        if(zDefaultFrameAct) {
            GN_DeleteArray(zDefaultFrameAct->zFEvent);
            zDefaultFrameAct.Delete();
        }
    }
}
void GNu::cSCROLL_LIST::SetZoningCount(unsigned int zoning_num) {
    zZoningNum = zoning_num;
    RePos();
}
void GNu::cSCROLL_LIST::SetZoningDrawCall(void (*draw_func)(int, int, int, int, unsigned int, unsigned int, unsigned int, bool, void *), void *usarg) {
    zUsArg = usarg;
    zDrawCallHasUsArg = true; //9 参签名：即使 usarg==0 也要按 9 参调用
    zDrawCall = (void (*)(int, int, int, int, unsigned int, unsigned int, unsigned int, bool))draw_func;
    // zFFrames 可能还没 Init（注册先于初始化的调用顺序）：空守卫，别解引用空指针。
    if(zFFrames && zFFrames->zMax_num != (unsigned int)zDrawRangeData.Num()) zDrawRangeData.New((unsigned int)zFFrames->zMax_num);
}
bool GNu::cSCROLL_LIST::InitStyleToData(void (*draw_func)(int, int, int, int, unsigned int, unsigned int, unsigned int, bool),
                                        short _x, short _y, short _w, short _h,
                                        bool default_am, short dw, short dh, unsigned int zoning_num,
                                        unsigned int focus_color, unsigned short focus_blend) {
    zDrawCall = draw_func;
    zDrawCallHasUsArg = false; //8 参签名
    zUsArg = 0; //清掉 9 参注册留下的用户参数，否则分派会按 9 参签名调 8 参回调（DA-1126-15）
    zZoningNum = zoning_num;
#ifndef GN_USING_ADDITIONAL_LIB //GN_ADD_CONTEN_PRIVATE
    if(!zoning_num) return false;
#endif
    aFocusColor = focus_color;
    aFocusBlend = focus_blend;
    zReArrange = false;
    zOffsetSheep = 0.1f;
    zTextOffset = 0.0f;
    zTextOffsettg = 0.0f;
    if(!dw) dw = _w;
    if(!dh) dh = _h >> 1;
    GT_TMP_UI_OffsetArrowInit();

    uint8 locbuf[2048];
    GN::sTBUF_INT tbuf(locbuf, 2048);
    char *pic_id;
    if(zInternalImg) pic_id = GNg::cFRAME_LAUNCHER_FONT::aPicIndex;
    else {
        pic_id = (char *)tbuf.GetSpace(zZoningNum);
        memset(pic_id, 0, zZoningNum);
    }
    GNg::cFRAME_LAUNCHER_FONT *_fframe = zFFrames;
    if(_fframe) {
        _fframe->aGenRngX = (float)_x;
        _fframe->aGenRngY = (float)_y;
        _fframe->aGenRngW = (float)_w;
        _fframe->aGenRngH = (float)_h;
        GNg::sARRANGE_FMT &ffmt = _fframe->zParame;
        ffmt.i_compartment += (dw - ffmt.width);
        ffmt.j_compartment += (dh - ffmt.height);
        ffmt.width = dw;
        ffmt.height = dh;

        if(zInternalImg) {
            GNg::cFRAME *ffam = _fframe->zFontFrame;
            ffam->zRcS.right *= zInternalImg->uRange / ffam->zPlay_obj->uRange;
            ffam->zRcS.bottom *= zInternalImg->vRange / ffam->zPlay_obj->vRange;
            _fframe->zTu_w = ffam->zRcS.right;
            _fframe->zTv_h = ffam->zRcS.bottom;
            if(ffam->zObjsKeep.Us() == 1 && ffam->zPlay_obj && ffam->zPlay_obj != &GNg::Image) delete(ffam->zPlay_obj);
            ffam->zPlay_obj = zInternalImg;
        }
    } else {
        if(!zFrameIcon) {
            zFrameIcon.New(1);
            if(default_am) {
                zFev2.New(2);
                zFev2->InitColor(0, 260, 0x00ffffff, 0xffffffff);
                zFev2[1].Init(GNg::eEVENT_SIZE_Y, 0, 260, 0.01f, 1.0f);
                zFrameIcon->Init(zInternalImg, 2, zFev2 GT_TMP_DEF_FRAME_INIT_ARGS_ONCE_KEEP);
            } else zFrameIcon->Init(zInternalImg, 0, 0 GT_TMP_DEF_FRAME_INIT_ARGS_ONCE_KEEP);
        }
        if(default_am) CheckInitDefaultFrameAct();

        GNg::sRECT2 arc2d((float)_x, (float)_y, (float)(_x + _w), (float)(_y + _h));
        GNg::sARRANGE_FMT afmt(false, 0.0f, 0.0f, 1, dh >> 4, dw, dh, zIsErectArrange ? GNg::eENTER_LEFT_RIGHT : GNg::eENTER_TOP_BOTTOM, !zIsErectArrange, false);
        zFFrames.New(1);
        zFFrames->InitToData(zFrameIcon, pic_id, (unsigned short)zZoningNum, &arc2d, &afmt, 0.0f, 100, default_am ? 1 : 0, 127, 0, 127, zInternalImg ? 127 : 5, 0, 0, 0, default_am ? zDefaultFrameAct.Begin() : 0, true);
        zFFrames->ControlActFromMouseAuto(default_am);
    }
    zCheckResetZoningCount(pic_id, _fframe);
    if(zDrawCall && zFFrames->zMax_num != (unsigned int)zDrawRangeData.Num()) zDrawRangeData.New((unsigned int)zFFrames->zMax_num);
    x = _x;
    y = _y;
    w = _w;
    h = _h;
    return true;
}
void GNu::cSCROLL_LIST::zCheckResetZoningCount(char *pic_id, void *last_fframe) {
    zLastStartPos = 0;
    unsigned int arng_num = (unsigned int)GetParalleShowCount(), set_count = zZoningNum;
    if(set_count / arng_num + 1 > 254) set_count = arng_num * 254; //检测并裁短一下SetText的范围 以防超出SetText最大256行的限制
    if(last_fframe || set_count != zZoningNum) zFFrames->SetText(pic_id, (unsigned short)set_count, 0, 0);
}
void GNu::cSCROLL_LIST::Transpose() {
#ifdef GN_DEBUG
    if(!zFFrames) { GN_Log("调用 cSCROLL_LIST::Transpose(...)失败! 还没初始化好!!\n"); }
#endif
    GNg::sARRANGE_FMT &ffmt = zFFrames->zParame;
    ffmt.erect_print = !ffmt.erect_print;
    switch(ffmt.auto_enter) {
        case GNg::eENTER_LEFT_RIGHT: ffmt.auto_enter = GNg::eENTER_TOP_BOTTOM; break;
        case GNg::eENTER_TOP_BOTTOM: ffmt.auto_enter = GNg::eENTER_LEFT_RIGHT; break;
    }
    zReArrange = true;
}
void GNu::cSCROLL_LIST::SetEffect(short focus_flash_time, P<GNg::cFRAME_LAUNCHER_FONT> effect, P<GNg::sIMAGE> focus_style, P<GNg::sIMAGE> offset_arrow, int8 repeat_num) {
    GT_TMP_UI_SET_EFFECT;
}
unsigned int GNu::cSCROLL_LIST::UpdateRun(unsigned int updatatime, bool bg_eq, unsigned int dc) {
    if(!zZoningNum) {
#ifdef GN_DEBUG
        GN_Log("调用 cSCROLL_LIST::UpdateRun(...) 被停止! 请先调用它的初始化函数\n");
#endif
        return 0;
    }
    GNg::cFRAME_LAUNCHER_FONT *_fframe = zFFrames;
    GNg::sARRANGE_FMT *pfmt = &(_fframe->zParame);
    if(zReArrange) {
        bool ret_am = GN_ValueCompareFloat(zTextOffset, zTextOffsettg, 0.5f);
        GT_TMP_ScrollSetTextRangeBegin(zZoningNum)
            uint8 locbuf[2048];
        GN::sTBUF_INT tbuf(locbuf, 2048);
        char *pic_id;
        if(bg_eq) {
            pic_id = (char *)tbuf.GetSpace(zZoningNum);
            memset(pic_id, 0, zZoningNum);
        } else {
#ifdef GN_DEBUG
            if(zZoningNum > 128) { GN_Log("调用cSCROLL_LIST::UpdateRun(...) 不同的图索引号不能大于128个!\n"); }
#endif
            pic_id = GNg::cFRAME_LAUNCHER_FONT::aPicIndex;
        }
        if(pfmt->erect_print) _fframe->SetText(pic_id + pid_start, set_count, _icon_offset, 0, ret_am);
        else _fframe->SetText(pic_id + pid_start, set_count, 0, _icon_offset, ret_am);
        if(pid_start) {
            GNg::sDRAW_RANGE *_arrd = _fframe->zDrawRngData;
            GNg::sDRAW_RANGE *arrd_end = _arrd + _fframe->zMax_num;
            unsigned short _pid_start = (unsigned short)pid_start;
            for(; _arrd < arrd_end; ++_arrd) _arrd->strid += _pid_start;
        }
        zOnReArranged(pid_start, zLastStartPos, ret_am);
        zLastStartPos = pid_start;
        zReArrange = false;
        if(zDrawCall && _fframe->zMax_num != zDrawRangeData.Num()) zDrawRangeData.New(_fframe->zMax_num);
    }
    // 更新显示区划 ---------------------------------------------------------------
    unsigned short blmode = _fframe->zFontFrame->aBlendmode;
    if(!aShowBG) _fframe->zFontFrame->aBlendmode = 0;
    _fframe->zUsScissor = aShowInFrame;
    _fframe->zDrawRangeInfo = zDrawRangeData;
    unsigned int ret = _fframe->PlayFrame(updatatime, dc);
    _fframe->zDrawRangeInfo = 0;
    _fframe->zFontFrame->aBlendmode = blmode;
    GT_TMP_FFRAME_RANGE_GET;
    zIsMouseFocus = mouse_x > zx && mouse_x < zr && mouse_y > zy && mouse_y < zb;
    if(!ret) return 0;

    // 处理绘制回调 -------------------------------------------------------------------------------------------------------------------------------------------------------------------------
    if(zDrawCall) {
        if(aShowInFrame) GNg::ViewCur->ScissorRectSetup(zx, zy, zw, zh);
        GNg::sDRAW_RANGE *_text_data = _fframe->zDrawRngData;
        unsigned int *time_data = _fframe->zNowtime_s, cmp_time = _fframe->zSrcAmMode ? _fframe->zEnd_time : 0xffffffff;
        GNg::sDRAW_AREA *rng_datas = zDrawRangeData, *_rng_info;
        unsigned int zvi = 0, zvn = _fframe->zMax_num;
        if(zDrawCallHasUsArg) { //按注册签名分派，不看 zUsArg（DA-1126-15）
            void (*draw_callex)(int, int, int, int, unsigned int, unsigned int, unsigned int, bool, void *) = (void (*)(int, int, int, int, unsigned int, unsigned int, unsigned int, bool, void *))zDrawCall;

#define GT_TMP_SCROLL_RECT_DRAW_ZONING   \
    for(; zvi < zvn; ++zvi) {            \
        if(time_data[zvi] < cmp_time) {  \
            _rng_info = rng_datas + zvi; \
            tx = _rng_info->x, ty = _rng_info->y, tw = _rng_info->w, th = _rng_info->h;

            GT_TMP_SCROLL_RECT_DRAW_ZONING
            draw_callex(tx, ty, tw, th, _rng_info->c, (unsigned int)_text_data[zvi].strid, zvi, (mouse_x >= tx && mouse_x <= tx + tw && mouse_y >= ty && mouse_y <= ty + th), zUsArg);
        }
    }
}
else {
    GT_TMP_SCROLL_RECT_DRAW_ZONING
    zDrawCall(tx, ty, tw, th, _rng_info->c, (unsigned int)_text_data[zvi].strid, zvi, (mouse_x >= tx && mouse_x <= tx + tw && mouse_y >= ty && mouse_y <= ty + th));
}
}
}
if(aShowInFrame) GNg::ViewCur->ScissorRectRemove();
}
unsigned int focus_color = zArrowAm.aLevel_color;

// 处理滚屏 ---------------------------------------------------------------------------------------------------------
const unsigned int add_row = ((zw >= pfmt->i_compartment) ? ((zZoningNum % (zw / pfmt->i_compartment)) ? 1 : 0) : 0);
GT_TMP_UI_SCROLL_VIEW(0, add_row, zZoningNum, if(zOnAddScroolOffset(move_dist)); else);
return zReArrange ? 1 : ret;
}
void GNu::cSCROLL_LIST::UpdateRun(unsigned int updatetime, sUI_LEVEL *parent){
    GT_TMP_SCROLL_LIST_UPDATE(UpdateRun(updatetime, true, parent -> c), )} P<GNu::iUI_OBJECT> GNu::cSCROLL_LIST::Clone() {
    P<cSCROLL_LIST> ui_scr_list(1);
    *ui_scr_list = *this;
    ui_scr_list->zSpecClone();
    return ui_scr_list;
}
int GNu::cSCROLL_LIST::GetTotalOffset() {
    GNg::cFRAME_LAUNCHER_FONT *_fframe = zFFrames;
    GNg::sARRANGE_FMT &ffmt = _fframe->zParame;
    int col_num = (_fframe->zRowNum > 1) ? (zZoningNum / _fframe->zRowSIDs[1]) : 1;
    if(ffmt.erect_print && ffmt.overturn_print) {
        return col_num * ffmt.i_compartment;
    } else {
        if(ffmt.erect_print) return col_num * (-ffmt.i_compartment);
        else return col_num * (-ffmt.j_compartment);
    }
}
void GNu::cSCROLL_LIST::SetOffsetRatio(float value, float flow_r) {
    if(!zZoningNum) return;
    GN_ValueClamp(value, 0.0f, 1.0f);
    zTextOffsettg = (float)GetTotalOffset() * value;
    if(flow_r) zTextOffset += (zTextOffsettg - zTextOffset) * flow_r;
}
void GNu::cSCROLL_LIST::RePos() {
    zTextOffset = 0.0f;
    zTextOffsettg = 0.0f;
    zReArrange = true;
}
void GNu::cSCROLL_LIST::OkHide() {
    zFFrames->ToHide();
}
GT_TMP_UI_GET_SET_POS_AND_OTHER_FUNC_DEF(cSCROLL_LIST)
#if 0
void GNu:cSCROLL_LIST::StatisticSize(cUI_GROUP *parent){\
	GNg::cFRAME_LAUNCHER_FONT * _fframe = zFFrames;\
\
	/*存在问题:该段之后可以移到预计算*/\
	w = (short)(_fframe->aGenRngW);\
	h = (short)(_fframe->aGenRngH);\
	x = (short)(_fframe->aGenRngX);\
	y = (short)(_fframe->aGenRngY);\
	\
	EnsureInclude(parent);\
}
#endif
/***************************************************** cSELECT_LIST_ICON *************************************************************************/
/*static*/ void GT_SelectListIconDrawCall(int x, int y, int w, int h, unsigned int color, unsigned int zid, unsigned int vid, bool has_focus, void *arg) {
    GNu::cSELECT_LIST_ICON *sl = (GNu::cSELECT_LIST_ICON *)arg;
    GT_SELECT_LIST_ProcAndDrawFocus(zSelectProc(zid, has_focus), zArrowAm.aLevel_color, zFocusImg);
}
