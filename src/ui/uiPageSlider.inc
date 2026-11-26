// GN_mini UI 实现片段：cSELECT_CALL / cPAGE_BAR / cUI_PAGE_BAR / cSELECT_BUTTON / cUI_SELECT_BUTTON / cSELECT_RECT / cSLIDER。
// 2026-11-26 自 GN_UiBase.cpp 第 714-1084 行 move-only 拆出（REFACTOR_PLAN §13 B4）。
// 由 GN_UiBase.cpp 按原顺序包含（单编译单元，static/宏语义不变）；不是独立头文件。
/***************************************************** cSELECT_CALL *************************************************************************/
void GNu::cSELECT_CALL::Init(short count) {
    zHots.New(count);
    float *_host = zHots;
    short i = 0;
    for(; i < count; ++i) _host[i] = (i == aSelect ? 1.0f : 0.0f);
}
void GNu::cSELECT_CALL::UpdateRun(void (*proc)(short, short, short, float hot, void *), void *us_arg, bool cp, short dx, short dy, short size, short dist, bool erect, float time_raito) {
    if(zHots.Us() > 1) Init(zHots.Num());
    float *_host = zHots;
    short i = 0, count = zHots.Num(), dx2, dy2, selid = aSelect;
    short mouse_mx = GNg::ViewCur->mouseMX, mouse_my = GNg::ViewCur->mouseMY;
    if(erect) { /*模板代码:检测和处理每个项的选择*/
        bool m_inl = cp && mouse_mx >= dx && mouse_mx <= dx + size;
        for(; i < count; ++i) {
            if(m_inl && mouse_my >= dy && mouse_my <= dy + dist) {
                UiActionCall((cUI_SELECT_BUTTON *)((cSELECT_BUTTON *)this), i + 1);
                if(i != selid) UiActionCall((cUI_SELECT_BUTTON *)((cSELECT_BUTTON *)this), -(selid + 1));
                aSelect = i;
            }
            if(i == selid) dy2 = dy;
            else proc(dx, dy, i, _host[i], us_arg);
            dy += dist;
        }
        if(selid < count) proc(dx, dy2, selid, _host[selid], us_arg);
    } else { /*模板代码:检测和处理每个项的选择*/
        bool m_inl = cp && mouse_my >= dy && mouse_my <= dy + size;
        for(; i < count; ++i) {
            if(m_inl && mouse_mx >= dx && mouse_mx <= dx + dist) {
                UiActionCall((cUI_SELECT_BUTTON *)((cSELECT_BUTTON *)this), i + 1);
                if(i != selid) UiActionCall((cUI_SELECT_BUTTON *)((cSELECT_BUTTON *)this), -(selid + 1));
                aSelect = i;
            }
            if(i == selid) dx2 = dx;
            else proc(dx, dy, i, _host[i], us_arg);
            dx += dist;
        }
        if(selid < count) proc(dx2, dy, selid, _host[selid], us_arg);
    }
    time_raito *= 0.2f;
    for(i = 0; i < count; ++i) _host[i] += ((i == aSelect ? 1.0f : 0.0f) - _host[i]) * time_raito;
}
void GNu::cPAGE_BAR::Init(P<P<char>> names, const GNg::cTEXT_ART *text_style, short l, short h) {
    unsigned int ci = 0, cn = names.Num();
    cSELECT_CALL::Init(cn);
    zNames = names;
    if(text_style) zTextOut = (GNg::cTEXT_ART *)text_style;
    else zTextOut = &GN::RunRec.aFont;
    GNg::sARRANGE_FMT *tfmt = zTextOut->GetArrangeFormat();
    if(l) zL = l;
    else {
        P<char> *_names = names;
        unsigned int max_len = 0, len;
        for(; ci < cn; ++ci) {
            len = strlen(_names[ci]);
            if(len > max_len) max_len = len;
        }
        max_len += 2;
        zL = max_len * (tfmt->erect_print ? (zTextOut->zFontMapFormat == GNg::eFONT_FREETYPE ? (tfmt->j_compartment * 3 / 5) : tfmt->j_compartment) : tfmt->i_compartment);
    }
    if(h) zH = h;
    else zH = (tfmt->erect_print ? (zTextOut->zFontMapFormat == GNg::eFONT_FREETYPE ? (tfmt->i_compartment << 1) : tfmt->i_compartment) : tfmt->j_compartment);
}
void GT_PageDrawCall(short dx, short dy, short id, float hot, void *us_arg) {
    unsigned int *ccs = (unsigned int *)(((void **)us_arg)[0]);
    GNu::cPAGE_BAR *_pb = (GNu::cPAGE_BAR *)(((void **)us_arg)[1]);
    int tw = *((int *)(((void **)us_arg)[2])), tfw, tfh;
    GNg::sARRANGE_FMT *tfmt = _pb->zTextOut->GetArrangeFormat();
    if(tfmt->erect_print) {
        tfw = _pb->zH, tfh = _pb->zL;
        if(tfmt->overturn_print) {
            short h2 = (short)((hot) * (float)tfw);
            if(ccs[2]) GNg::ImageDraw(0, dx, dy, h2, tfh, ccs[2]);
            GNg::ImageDraw(0, dx + h2, dy, tfw - h2, tfh, ccs[1]);
        } else {
            short h2 = (short)((1.0f - hot) * (float)tfw);
            GNg::ImageDraw(0, dx, dy, h2, tfh, ccs[1]);
            if(ccs[2]) GNg::ImageDraw(0, dx + h2, dy, tfw - h2, tfh, ccs[2]);
        }
        if(ccs[3]) GNg::LineFrame2dDraw(dx, dy, tfw, tfh, 1, ccs[3]);

        dy += 5;
        tfh -= 10;
    } else {
        tfw = _pb->zL, tfh = _pb->zH;
        short h2 = (short)((1.0f - hot) * (float)tfh);
        GNg::ImageDraw(0, dx, dy, tfw, h2, ccs[1]);
        if(ccs[2]) GNg::ImageDraw(0, dx, dy + h2, tfw, tfh - h2, ccs[2]);
        if(ccs[3]) GNg::LineFrame2dDraw(dx, dy, tfw, tfh, 1, ccs[3]);

        dx += 5;
        tfw -= 10;
    }
    for(tw += dx; dx < tw; ++dx) _pb->zTextOut->OutText(_pb->zNames[id], 0, dx, dy, tfw, tfh, ccs[0]);
}
void GNu::cPAGE_BAR::UpdateRun(bool cp, short dx, short dy, short dist, int tw, unsigned int text_color, unsigned int back_color, unsigned int focus_color, unsigned int line_color, float time_raito) {
    unsigned int ccs[4] = {text_color, back_color, focus_color, line_color};
    void *args[3] = {ccs, this, &tw};
    GNg::sARRANGE_FMT *tfmt = zTextOut->GetArrangeFormat();
    float nu = tfmt->near_pointU, nv = tfmt->near_pointV;
    if(!(tfmt->erect_print && tfmt->overturn_print)) tfmt->near_pointU = 0.5f;
    tfmt->near_pointV = 0.5f;
    cSELECT_CALL::UpdateRun(GT_PageDrawCall, (void *)args, cp, dx, dy, zH, dist ? dist : zL, tfmt->erect_print, time_raito);
    tfmt->near_pointU = nu;
    tfmt->near_pointV = nv;
}
void GNu::cPAGE_BAR::zUpdateRun(bool cp, short dx, short dy, short dist, int tw, unsigned int text_color, unsigned int back_color, unsigned int focus_color, unsigned int line_color, float time_raito) {
    unsigned int ccs[4] = {text_color, back_color, focus_color, line_color};
    void *args[3] = {ccs, this, &tw};
    cSELECT_CALL::UpdateRun(GT_PageDrawCall, (void *)args, cp, dx, dy, zH, dist ? dist : zL, zTextOut->GetArrangeFormat()->erect_print, time_raito);
}
void GNu::cPAGE_BAR::GetOnecWH(short &w, short &h) {
    if(zTextOut->GetArrangeFormat()->erect_print) {
        w = zH;
        h = zL;
    } else {
        h = zH;
        w = zL;
    }
}
void GNu::cSELECT_BUTTON::Init(P<GNg::sIMAGE> button_style) {
    cSELECT_CALL::Init(button_style->texnum);
    zImg = button_style;
}
void GT_SelbuttonDrawCall(short dx, short dy, short id, float hot, void *us_arg) {
    unsigned int *ccs = (unsigned int *)(((void **)us_arg)[0]);
    GNg::sIMAGE *_img = (GNg::sIMAGE *)(((void **)us_arg)[1]);
    if(hot < 0.0005f || ccs[0] == ccs[1]) GNg::ImageDraw(_img, dx, dy, 0, 0, ccs[0], id);
    else {
        float add_r = 0.15f;
        unsigned int c = ccs[1];
        if(hot < 0.9995f) {
            add_r *= hot;
            if(c != ccs[0]) c = GNg::BlendColor(ccs[0], c, hot);
        }
        int dw = (float)_img->width * add_r, dh = (float)_img->height * add_r;
        GNg::ImageDraw(_img, dx - (dw >> 1), dy - (dh >> 1), _img->width + dw, _img->height + dh, c, id);
    }
}
void GNu::cSELECT_BUTTON::UpdateRun(bool cp, short dx, short dy, short dist, unsigned int color, unsigned int focus_color, bool erect, float time_raito) {
    unsigned int ccs[2] = {color, focus_color};
    void *args[2] = {ccs, zImg.Begin()};
    cSELECT_CALL::UpdateRun(GT_SelbuttonDrawCall, (void *)args, cp, dx, dy, erect ? zImg->width : zImg->height, dist ? dist : (erect ? zImg->height : zImg->width), erect, time_raito);
}
void GNu::cSELECT_BUTTON::GetOnecWH(short &w, short &h) {
    w = zImg->width;
    h = zImg->height;
}
/*------------------------------------------------------------------------------------------------------------------------------------------*/
void GNu::cUI_PAGE_BAR::GetSize(short &_w, short &_h) {
    if(zHots) {
        short _l = (aStride ? (zHots.Num() - 1) * aStride + zL : zHots.Num() * zL);
        if(zTextOut->GetArrangeFormat()->erect_print) {
            _h = _l;
            _w = zH;
        } else {
            _w = _l;
            _h = zH;
        }
    } else {
        _w = 0;
        _h = 0;
    }
}
void GNu::cUI_PAGE_BAR::UpdateRun(unsigned int updatetime, sUI_LEVEL *parent) {
    short dx, dy;
    unsigned int dc = DrawBeginPosGet(dx, dy, parent);
    cPAGE_BAR::UpdateRun(GN::MouseI[GM_LEFT] && RecvInputUiObject == this, dx, dy, aStride, aFontLineW, dc, GNg_MultiplyColorRatio(aBackColor, parent->c), GNg_MultiplyColorRatio(aFocusColor, parent->c), GNg_MultiplyColorRatio(aLineColor, parent->c), aAmSheep);
    GetSize(w, h);
    if(aAutoLayerObjShow) {
        GN::sLIST<P<iUI_OBJECT>>::iterator itu = parent->node->aSubs.begin(), etu = parent->node->aSubs.end();
        for(; itu != etu; ++itu) {
            if((iUI_OBJECT *)(*itu) == (iUI_OBJECT *)this) {
                ++itu;
                iUI_OBJECT *_uobj;
                float *_hots = zHots;
                unsigned int ci = 0, cn = zHots.Num();
                for(; ci < cn && itu != etu; ++itu) {
                    _uobj = *itu;
                    if(_uobj->aType == GNg::eOBJ_GROUP || _uobj->aType == GNu::eUI_SCROLL_RECT) {
                        ((cUI_GROUP *)_uobj)->c = ((uint8)(255.0f * _hots[ci]) << 24) | (((cUI_GROUP *)_uobj)->c & 0x00ffffff);
                        ++ci;
                    }
                }
                break;
            }
        }
    }
}
void GNu::cUI_PAGE_BAR::GetDrawArea(GNg::sDRAW_AREA2 &out, cUI_GROUP *parent) {
    out.w = w;
    out.h = h;
    out.c = DrawBeginPosGet(out.x, out.y, (GNg::sDRAW_AREA2 *)&(parent->gx));
}
P<GNu::iUI_OBJECT> GNu::cUI_PAGE_BAR::Clone() {
    P<cUI_PAGE_BAR> ui_page(1);
    *ui_page = *this;
    ui_page->zHots.Copy(zHots);
    return ui_page;
}
void GNu::cUI_SELECT_BUTTON::GetSize(short &_w, short &_h) {
    if(zHots) {
        GNg::sIMAGE *_img = zImg;
        if(aIsErectArrange) {
            _h = (aStride ? (zHots.Num() - 1) * aStride + _img->height : zHots.Num() * _img->height);
            _w = _img->width;
        } else {
            _w = (aStride ? (zHots.Num() - 1) * aStride + _img->width : zHots.Num() * _img->width);
            _h = _img->height;
        }
    } else {
        _w = 0;
        _h = 0;
    }
}
void GNu::cUI_SELECT_BUTTON::UpdateRun(unsigned int updatetime, sUI_LEVEL *parent) {
    short dx, dy;
    unsigned int dc = DrawBeginPosGet(dx, dy, parent);
    cSELECT_BUTTON::UpdateRun(GN::MouseI[GM_LEFT] && RecvInputUiObject == this, dx, dy, aStride, dc, GNg_MultiplyColorRatio(aFocusColor, parent->c), aIsErectArrange, aAmSheep);
    GetSize(w, h);
}
void GNu::cUI_SELECT_BUTTON::GetDrawArea(GNg::sDRAW_AREA2 &out, cUI_GROUP *parent) {
    out.w = w;
    out.h = h;
    out.c = DrawBeginPosGet(out.x, out.y, (GNg::sDRAW_AREA2 *)&(parent->gx));
}
P<GNu::iUI_OBJECT> GNu::cUI_SELECT_BUTTON::Clone() {
    P<cUI_SELECT_BUTTON> ui_sb(1);
    *ui_sb = *this;
    ui_sb->zHots.Copy(zHots);
    return ui_sb;
}
/***************************************************** cSELECT_RECT *************************************************************************/
GNu::cSELECT_RECT::cSELECT_RECT() : zP2(0, 0, 0, 0), zRect(0, 0, 0, 0), zHideSleep(1), zHideTime(0) {}

void GNu::cSELECT_RECT::Init(P<GNg::sIMAGE> bg_img, int hide_sleep) {
    zBg = bg_img;
    zHideSleep = hide_sleep + 1;
}
GNg::sRECT *GNu::cSELECT_RECT::UpdateRun(unsigned int updatatime, unsigned int color, unsigned short blendmode, unsigned int line_color, unsigned short line_blendmode, int line_w) {
    // 处理鼠标选择 ------------------------------------------------------------
    GNg::sRECT *_rc = 0;
    if(GN::Mouse[GM_LEFT] && !GN::Mouse1[GM_LEFT]) {
        zP2.right = GNg::ViewCur->mouseMX;
        zP2.bottom = GNg::ViewCur->mouseMY;
        if(zP2.right != zP2.left && zP2.bottom != zP2.top) {
            GNg::RectMakeFromP2(zRect, zP2.left, zP2.top, zP2.right, zP2.bottom);
            _rc = &zRect;
            zHideTime = zHideSleep;
        }
    } else {
        zP2.left = zP2.right = GNg::ViewCur->mouseMX;
        zP2.top = zP2.bottom = GNg::ViewCur->mouseMY;
    }
    // 显示选择筐 ----------------------------------------------------------------------------------------------------------------------
    if(zHideTime > 0) {
        int rx = zRect.left, ry = zRect.top, rw = zRect.right - zRect.left, rh = zRect.bottom - zRect.top;
        uint8 a = (uint8)(((color & 0xff000000) >> 24) * zHideTime / zHideSleep);
        if(a > 2) {
            if(zBg.Has() && zBg->texnum == 9) GNg::ImageDraw9Square(zBg, rx, ry, rw, rh, (a << 24) | (color & 0x00ffffff), blendmode); //九格边筐图
            else GNg::ImageDraw(zBg, rx, ry, rw, rh, (a << 24) | (color & 0x00ffffff), 0, blendmode);
        }
        if(!zBg && !line_w) line_w = 1;
        if(line_w) {
            a = (uint8)(((line_color & 0xff000000) >> 24) * zHideTime / zHideSleep);
            if(a > 2) GNg::LineFrame2dDraw(rx, ry, rw, rh, line_w, (a << 24) | (line_color & 0x00ffffff), line_blendmode);
        }
        zHideTime -= (int)updatatime;
    }
    return _rc;
}
void GNu::cSELECT_RECT::Hide() {
    zP2.left = zP2.right;
    zP2.top = zP2.bottom;
    zHideTime = 0;
}
/********************************************* cSLIDER ********************************************************/
P<GNu::iUI_OBJECT> GNu::cSLIDER::Clone() {
    P<cSLIDER> u_slider(1);
    *u_slider = *this;
    return u_slider;
}
void GNu::cSLIDER::GetDrawArea(GNg::sDRAW_AREA2 &out, cUI_GROUP *parent) {
    DrawRangeGet(out, (GNg::sDRAW_AREA2 *)&(parent->gx));
}
void GNu::cSLIDER::UpdateRun(unsigned int updatetime, sUI_LEVEL *parent) {
    GNg::sDRAW_AREA2 rng;
    DrawRangeGet(rng, parent);
    short dx = rng.x, dy = rng.y;
    unsigned int dc = rng.c;
    GNg::ImageDraw(0, dx, dy, rng.w, rng.h, dc, 0, bmode);
    if(img) {
        short iw = aIconW ? aIconW : img->width, ih = aIconH ? aIconH : img->height;
        if(zState == 2) {
            short oo = zOfs;
            if(w > h) {
                zOfs = GNg::ViewCur->mouseMX - dx;
                GN_ValueClamp(zOfs, 0, rng.w);
                dx += zOfs - (iw >> 1);
                dy += (rng.h >> 1) - (ih >> 1);
            } else {
                zOfs = GNg::ViewCur->mouseMY - dy;
                GN_ValueClamp(zOfs, 0, rng.h);
                dy += zOfs - (ih >> 1);
                dx += (rng.w >> 1) - (iw >> 1);
            }
#define GT_TMP_SLIDER_PRESS_PROC                                                                                          \
    if(!GN::Mouse[GM_LEFT]) {                                                                                             \
        zState = 1;                                                                                                       \
        UiActionCall(this, (unsigned int)zOfs * 65535 / (w > h ? w : h));                                                 \
    } else if(zOfs != oo) {                                                                                               \
        short crs = zOfs > oo ? zOfs - oo : oo - zOfs;                                                                    \
        if(crs > 5 || !(GN::TimeGetRunFpsCount() & 31)) UiActionCall(this, (unsigned int)zOfs * 65535 / (w > h ? w : h)); \
    }                                                                                                                     \
    dc = aPressColor;

            GT_TMP_SLIDER_PRESS_PROC
        } else {
            short iw2 = iw < 0 ? -iw : iw, ih2 = ih < 0 ? -ih : ih;
            zState = 0;
            if(RecvInputUiObject == this && ((w > h ? GNg::ViewCur->HitRange(dx - (iw2 >> 1), dy + ((rng.h - ih2) >> 1), rng.w + iw2, ih2) : GNg::ViewCur->HitRange(dx + ((rng.w - iw2) >> 1), dy - (ih2 >> 1), iw2, rng.h + ih2)))) {
                if(GN::Mouse1[GM_LEFT]) zState = 2;
                else zState = 1;
            }
            if(aAmSheep < 1.0f) {
                zFocusRatio += ((zState ? 1.0f : 0.0f) - zFocusRatio) * aAmSheep;
                dc = GNg::BlendColor(aIconColor, aFocusColor, zFocusRatio);
            } else {
                if(zState) {
                    zFocusRatio = 1.0f;
                    dc = aFocusColor;
                } else {
                    zFocusRatio = 0.0f;
                    dc = aIconColor;
                }
            }
            if(w > h) {
                dx += zOfs - (iw >> 1);
                dy += (rng.h >> 1) - (ih >> 1);
            } else {
                dy += zOfs - (ih >> 1);
                dx += (rng.w >> 1) - (iw >> 1);
            }
        }
        GNg::ImageDraw(img, dx, dy, aIconW, aIconH, GNg_MultiplyColorRatio(dc, parent->c), 0, aIconBlend);
    } else {
        if(zState == 2) {
            short oo = zOfs;
            if(w > h) {
                zOfs = GNg::ViewCur->mouseMX - dx;
                GN_ValueClamp(zOfs, 0, rng.w);
            } else {
                zOfs = GNg::ViewCur->mouseMY - dy;
                GN_ValueClamp(zOfs, 0, rng.h);
            }
            GT_TMP_SLIDER_PRESS_PROC
        } else {
            zState = 0;
            if(RecvInputUiObject == this && GNg::ViewCur->HitRange(dx, dy, rng.w, rng.h)) {
                if(GN::Mouse1[GM_LEFT]) zState = 2;
                else zState = 1;
            }
            dc = aFocusColor;
        }
        dc = GNg_MultiplyColorRatio(dc, parent->c);
        short len = zOfs - (aIconW << 1);
        if(len < 0) len = 0;
        if(w > h) GNg::ImageDraw(0, dx + aIconW, dy + aIconW, len, rng.h - (aIconW << 1), dc, 0, bmode);
        else GNg::ImageDraw(0, dx + aIconW, dy + aIconW, rng.w - (aIconW << 1), len, dc, 0, bmode);
    }
}
