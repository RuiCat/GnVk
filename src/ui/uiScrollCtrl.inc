// GN_mini UI 实现片段：cSCROLL_CTRL / cSCROLL_RECT 滚动基础控件。
// 2026-11-26 自 GN_UiBase.cpp 第 29-191 行 move-only 拆出（REFACTOR_PLAN §13 B4）。
// 由 GN_UiBase.cpp 按原顺序包含（单编译单元，static/宏语义不变）；不是独立头文件。
/************************************************************** cSCROLL_RECT *********************************************************************/
GNu::cSCROLL_CTRL::cSCROLL_CTRL() : zOffsetSheep(0.1f), zRepeatNum(0), zIsMouseFocus(false) {
    zFev.InitColor(0, 0, 0x50ffffff, 0xffffffff);
}

void GNu::cSCROLL_CTRL::SetOffsetBorderEF(short focus_flash_time, P<GNg::sIMAGE> offset_arrow, int8 repeat_num) {
    zFev.segment_time = (unsigned int)focus_flash_time;
    if(offset_arrow) zOffsetArrow = offset_arrow;
    zRepeatNum = repeat_num;
    GT_UiInitArrowAm();
}
void GNu::cSCROLL_RECT::zResetOffsetAndAm() {
    zTextOffset = 0.0f;
    zTextOffsettg = 0.0f;
    zOffsetSheep = 0.1f;
    GT_TMP_UI_OffsetArrowInit();
}
void GNu::cSCROLL_RECT::InitStyleToData(short _x, short _y, short _w, short _h, short total_h, unsigned int focus_color, unsigned short focus_blend) {
    x = _x, y = _y, w = _w, h = _h;
    lw = _w;
    lh = _h;

#define GT_TMP_SCROLL_RECT_INIT_STYLE   \
    zMaxOffset = (unsigned int)total_h; \
    aFocusColor = focus_color;          \
    aFocusBlend = focus_blend;          \
    zResetOffsetAndAm();
    GT_TMP_SCROLL_RECT_INIT_STYLE
}
void GT_CloneUiGroupAttrb(GNu::cUI_GROUP *_ug, /*const*/ GNu::cUI_GROUP *src);

void GNu::cSCROLL_RECT::InitStyleToData(const cUI_GROUP *ui_group, short total_h, unsigned int focus_color, unsigned short focus_blend) {
    GT_CloneUiGroupAttrb((cUI_GROUP *)this, (cUI_GROUP *)ui_group);
    GT_TMP_SCROLL_RECT_INIT_STYLE
}
unsigned int GNu::cSCROLL_RECT::UpdateRun(unsigned int updatatime) {
    if((short)zMaxOffset <= h) return 1;
    int tx, ty, tw, th, mouse_x = GNg::ViewCur->mouseMX, mouse_y = GNg::ViewCur->mouseMY;
    unsigned int focus_color = zArrowAm.aLevel_color;

    bool ddraw_mode = !GN_Has3DAccelerate(); //滚动控制 ---------------------------------------------------------------------------------------------
    zIsMouseFocus = (short)mouse_x > x && (short)mouse_x < x + w && (short)mouse_y > y && (short)mouse_y < y + h;
    if(!GN::Mouse[GM_LEFT]) zKeyOnDown = false;
    if(zIsMouseFocus && GN::Mouse1[GM_LEFT]) zKeyOnDown = true;

    zArrowAm.aBlendmode = 0;
    short move_dist = 0;
    if(zKeyOnDown && GN::MouseM) { //按紧鼠标左键拖动
        move_dist = (short)GNg::ViewCur->mouseMSY;
        if(move_dist < -38 || move_dist > 38) move_dist <<= 1;
        else zTextOffset += (zTextOffsettg - zTextOffset) * GN::TimeRatioL * 0.8f;

    } else if(zIsMouseFocus) {
        if(GN::Mouse1[GM_TOP]) move_dist = h >> 2; //鼠标滚轮上
        else if(GN::Mouse1[GM_BOTTOM]) move_dist = -(h >> 2); //鼠标滚轮下
        else { //鼠标移动到筐边沿的拖动处理
            tx = (int)x;
            ty = (int)y;
            tw = (int)w;
            th = zOffsetArrow->width;
            zArrowAm.aFrame_ts.sy = (float)w / (float)zOffsetArrow->height;
            if(zRepeatNum) zArrowAm.aFrame_ts.sy /= (float)zRepeatNum;
            if(GNg::TestPointIntRect(mouse_x, mouse_y, tx, ty, tw, th)) {
                if(ddraw_mode)
                    GNg::ImageDraw(0, tx, ty, tw, th, focus_color, 0, aFocusBlend); //绘制拖动条
                else {
                    zArrowAm.aFrame_ts.x = (float)(tx + (tw >> 1));
                    zArrowAm.aFrame_ts.y = (float)ty;
                    zArrowAm.aFrame_ts.r = 270.0f; /*模版代码:旋转一下拖动条*/
                    zArrowAm.aBlendmode = aFocusBlend;
                    zArrowAm.PlayFrame(updatatime);
                }
                move_dist = (short)((float)(h * 12 / 600) * GN::TimeRatioL);
            }
            ty = y + h - zOffsetArrow->width;

            if(!move_dist && GNg::TestPointIntRect(mouse_x, mouse_y, tx, ty, tw, th)) {
                if(ddraw_mode)
                    GNg::ImageDraw(0, tx, ty, tw, th, focus_color, 0, aFocusBlend); //绘制拖动条
                else {
                    zArrowAm.aFrame_ts.x = (float)(tx + (tw >> 1));
                    zArrowAm.aFrame_ts.y = (float)(ty + th);
                    zArrowAm.aFrame_ts.r = 90.0f; /*模版代码:旋转一下拖动条*/
                    zArrowAm.aBlendmode = aFocusBlend;
                    zArrowAm.PlayFrame(updatatime);
                }
                move_dist = (short)((float)(h * 12 / 600) * -GN::TimeRatioL);
            }
        }
    }
    if(!ddraw_mode) {
        if(!zArrowAm.aBlendmode) zArrowAm.PlayFrame(updatatime); //如箭头没有被显示也运行更新一下 以便有动画颜色变化
    }
    if(move_dist > 0) {
        if(zTextOffsettg < 0.0f) {
            if(zTextOffsettg + (float)move_dist > 0.0f) zTextOffsettg = 0.0f;
            else zTextOffsettg += (float)move_dist;
        }
    } else if(move_dist < 0) {
        if((float)((short)zMaxOffset - h) > (-(zTextOffsettg + (float)move_dist))) zTextOffsettg += (float)move_dist;
        else zTextOffsettg = -((float)((short)zMaxOffset - h));
    }
    zTextOffset += (zTextOffsettg - zTextOffset) * zOffsetSheep * GN::TimeRatioL;
    return 1;
}
void GNu::cSCROLL_RECT::UpdateRun(unsigned int updatetime, sUI_LEVEL *parent) {
    GNg::sDRAW_AREA2 rng, *_rng = (GNg::sDRAW_AREA2 *)&(this->x);
    DrawRangeGet(rng, parent);
    bool not_ctrl = (RecvInputUiObject != this);
    short py = parent->y;
    parent->y += GetColOffset();
    if(aUsScissor) {
        aUsScissor = false;
        GNg::ViewCur->ScissorRectSetup(rng.x, rng.y, rng.w, rng.h);
        cUI_GROUP::UpdateRun(updatetime, parent);
        GNg::ViewCur->ScissorRectRemove();
        aUsScissor = true;
    } else
        cUI_GROUP::UpdateRun(updatetime, parent);
    parent->y = py;

    GNg::sDRAW_AREA2 rng_bk = *_rng;
    *_rng = rng;
    zArrowAm.aColor = GNg_MultiplyColorRatio(aFocusColor, parent->c);
    if(not_ctrl) {
        GNg::ViewCur->mouseMX += 10000;
        GNu::cSCROLL_RECT::UpdateRun(updatetime);
        GNg::ViewCur->mouseMX -= 10000;
    } else
        GNu::cSCROLL_RECT::UpdateRun(updatetime);
    *_rng = rng_bk;
}
void GNu::cSCROLL_CTRL::zSpecClone() {
    if(zArrowAm.zFEvent) zArrowAm.zFEvent = &zFev;
}
P<GNu::iUI_OBJECT> GNu::cSCROLL_RECT::Clone() {
    P<GNu::cSCROLL_RECT> ui_srect(1);
    GNu::cSCROLL_RECT *_ui_srect = ui_srect;
    GT_CloneUiGroupAttrb(_ui_srect, this);
    *((cSCROLL_CTRL *)_ui_srect) = *((cSCROLL_CTRL *)this);
    _ui_srect->zSpecClone();
    _ui_srect->zMaxOffset = zMaxOffset;
    return ui_srect;
}
void GNu::cSCROLL_RECT::SetPos(short _x, short _y, short _w, short _h) {
    x = _x, y = _y, w = _w, h = _h;
    lw = _w;
    lh = _h;
}
void GNu::cSCROLL_RECT::GetPos(short *_x, short *_y, short *_w, short *_h) {
    *_x = x, *_y = y, *_w = w, *_h = h;
}
void GNu::cSCROLL_RECT::SetOffsetRatio(float value) {
    GN_ValueClamp(value, 0.0f, 1.0f);
    zTextOffsettg = (-((float)((short)zMaxOffset - h))) * value;
}
void GNu::cSCROLL_RECT::StatisticFitMaxOffset() {
    GN::sLIST<P<iUI_OBJECT>>::iterator itu = aSubs.begin(), etu = aSubs.end();
    w = lw;
    h = lh;
    for(; itu != etu; ++itu) (*itu)->StatisticSize(this);
    zMaxOffset = h;
}
