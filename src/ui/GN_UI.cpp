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
#ifdef _GN_UI_H_
GN_API const char *GNu::UiObjTypeNames[eUI_TYPE_NUM] = {"UI组", GNg::ObjTypeNames[1], GNg::ObjTypeNames[2], GNg::ObjTypeNames[3], GNg::ObjTypeNames[4], GNg::ObjTypeNames[5], GNg::ObjTypeNames[6], GNg::ObjTypeNames[7],
                                                        "底筐", "靠边图像", "靠边动画图筐", "静态文本", "输入筐", "颜色版", "滚动筐", "滚动列表", "滚动嵌套UI列表", "图标选择列表", "文本选择列表", "滚动列表树", "文件浏缆", "简单对话筐", "选择按钮联", "分页栏", "滑动条", "触屏虚拟按钮", "触屏虚拟控杆", "数值提取描述"};

GN_API const char *GNu::UiObjCodeClassNames[eUI_TYPE_NUM] = {"cUI_GROUP", "sUI_IMAGE", "cLENS_FLARE_OBJ", "sUI_FRAME_RC", "cUI_FRAME", "cUI_FRAME_LAUNCHER", "cUI_FRAME_LAUNCHER2D", "cUI_FRAME_LAUNCHER_FONT",
                                                             "cUI_BACK_FRAME", "cUI_IMAGE", "cUI_FRAME_RC", "cUI_TEXT_ART", "cUI_TEXT_INPUT", "cPALETTE", "cSCROLL_RECT", "cSCROLL_LIST", "cSCROLL_LIST_GUI", "cSELECT_LIST_ICON", "cSELECT_LIST_TEXT", "cTREE_VIEW", "cFILE_BROWSER", "cUI_SIM_DIALOG", "cUI_SELECT_BUTTON", "cUI_PAGE_BAR", "cSLIDER", "cBUTTON_TOUCH", "cJOYSTICK", "cVALUE_EXTRACT_FILE"};
P<GNu::iUI_OBJECT> GT_DefUiObjectCreate(unsigned int _type) {
    switch(_type) {
        case GNg::eOBJ_IMAGE: return P<GNu::sUI_IMAGE>(1);
        case GNu::eUI_IMAGE: return P<GNu::cUI_IMAGE>(1);
        case GNg::eOBJ_FRAME_RC: return P<GNu::sUI_FRAME_RC>(1);
        case GNu::eUI_FRAME_RC: return P<GNu::cUI_FRAME_RC>(1);
        case GNg::eOBJ_FRAME: return P<GNu::cUI_FRAME>(1);
        case GNg::eOBJ_FRAME_LAUNCHER: return P<GNu::cUI_FRAME_LAUNCHER>(1);
        case GNg::eOBJ_FRAME_LAUNCHER2D: return P<GNu::cUI_FRAME_LAUNCHER2D>(1);
        case GNg::eOBJ_FRAME_LAUNCHER_FONT: return P<GNu::cUI_FRAME_LAUNCHER_FONT>(1);
        case GNu::eUI_TEXT_ART: return P<GNu::cUI_TEXT_ART>(1);
        case GNu::eUI_TEXT_IN: return P<GNu::cUI_TEXT_INPUT>(1);
        case GNu::eUI_PAGE_BAR: return P<GNu::cUI_PAGE_BAR>(1);
        case GNu::eUI_SELECT_BUTTON: return P<GNu::cUI_SELECT_BUTTON>(1);
        case GNu::eUI_SLIDER: return P<GNu::cSLIDER>(1);
        case GNu::eUI_SELECT_LIST_ICON: return P<GNu::cSELECT_LIST_ICON>(1);
        case GNu::eUI_SELECT_LIST_TEXT: return P<GNu::cSELECT_LIST_TEXT>(1);
        case GNu::eUI_SCROLL_LIST_GUI: return P<GNu::cSCROLL_LIST_GUI>(1);
        case GNu::eUI_TREE_VIEW: return P<GNu::cTREE_VIEW>(1);
        case GNu::eUI_FILE_BROWSER: return P<GNu::cFILE_BROWSER>(1);
        case GNu::eUI_PALETTE: return P<GNu::cPALETTE>(1);
        case GNu::eUI_BUTTON_TOUCH: return P<GNu::cBUTTON_TOUCH>(1);
        case GNu::eUI_JOYSTICK: return P<GNu::cJOYSTICK>(1);
        case GNu::eUI_SIM_DIALOG: return P<GNu::cUI_SIM_DIALOG>(1);
        case GNu::eUI_SCROLL_RECT: return P<GNu::cSCROLL_RECT>(1);
        case GNg::eOBJ_GROUP: return P<GNu::cUI_GROUP>(1);
        case GNu::eUI_BACK_FRAME: return P<GNu::cUI_BACK_FRAME>(1);
        case GNu::eUI_VALUE_EXTRACT: return P<GNu::cVALUE_EXTRACT_FILE>(1);
    }
    return P_();
}
unsigned int GT_DefUiShareObjSave(unsigned int type, P<uint8> obj, uint8 *data_out) {
    if(data_out) *data_out = 0;
    return 1;
}
unsigned int GT_DefUiShareObjLoad(P<uint8> &obj, const uint8 *data) {
    return 1;
}
void GT_DefUiActionProc(GNu::iUI_OBJECT *uobj, int action) {}

GN_API P<GNu::iUI_OBJECT> (*GNu::UiObjectCreate)(unsigned int _type) = GT_DefUiObjectCreate;
GN_API unsigned int (*GNu::UiShareObjSave)(unsigned int type, P<uint8> obj, uint8 *data_out) = GT_DefUiShareObjSave;
GN_API unsigned int (*GNu::UiShareObjLoad)(P<uint8> &obj, const uint8 *data) = GT_DefUiShareObjLoad;
GN_API void (*GNu::UiActionCall)(iUI_OBJECT *uobj, int action) = GT_DefUiActionProc;

GN::sLIST<int *> GT_UIObjectPlugins;

GN_API void GN_PluginUIObjectAdd(int *module, GT_PFUNC_VOID create_func) {
    GT_UIObjectPlugins.push_back(module);
}
GN_API int GN_PluginsUIObjectRemove() {
    GNu::UiObjectCreate = GT_DefUiObjectCreate;
    GNu::UiShareObjSave = GT_DefUiShareObjSave;
    GNu::UiShareObjLoad = GT_DefUiShareObjLoad;
    return GT_RemoveListPlugins(GT_UIObjectPlugins);
}
GN_API GNu::iUI_OBJECT *GNu::RecvInputUiObject = 0;
GN_API GNu::iUI_OBJECT *GNu::RequestInputUiObject = 0;
void GT_DrawBlurScreenImg(short _x, short _y, short _w, short _h, unsigned int color, unsigned short blmode, GNg::sIMAGE *arg);

void GNu::cUI_NEAR_OBJECT::StatisticSize(cUI_GROUP *parent) {
    EnsureInclude(parent);
}
void GNu::cUI_NEAR_OBJECT::GetDrawArea(GNg::sDRAW_AREA2 &out, cUI_GROUP *parent) {
    DrawRangeGet(out, (GNg::sDRAW_AREA2 *)&(parent->gx));
}
void GT_CloneUiGroupAttrb(GNu::cUI_GROUP *tg, /*const*/ GNu::cUI_GROUP *_ug) {
    memcpy((GNg::sDRAW_IN_AREA *)tg, (GNg::sDRAW_IN_AREA *)_ug, sizeof(GNg::sDRAW_IN_AREA));
    tg->lw = _ug->lw;
    tg->lh = _ug->lh;
    tg->aUsScissor = _ug->aUsScissor;

    GN::sLIST<P<GNu::iUI_OBJECT>>::iterator it = _ug->aSubs.begin(), et = _ug->aSubs.end();
    for(; it != et; ++it) tg->aSubs.push_back((*it)->Clone());
}
//GNu::cUI_GROUP::~cUI_GROUP(){}
P<GNu::iUI_OBJECT> GNu::cUI_GROUP::Clone() {
    P<cUI_GROUP> ug(1);
    GT_CloneUiGroupAttrb(ug, this);
    return ug;
}
void GNu::cUI_GROUP::Resize(GNg::cVIEW2D *view) {
    if(view) {
        x = view->ortho_left, lw = view->orthoW;
        y = view->ortho_top, lh = view->orthoH;
    } else {
        x = 0, lw = 128;
        y = 0, lh = 128;
    }
}
void GNu::cUI_GROUP::StatisticSize(cUI_GROUP *parent) {
    GN::sLIST<P<iUI_OBJECT>>::iterator itu = aSubs.begin(), etu = aSubs.end();
    if(aUsScissor) {
        for(; itu != etu; ++itu) (*itu)->StatisticSize(this);
        w = lw;
        h = lh;
    } else {
        w = lw;
        h = lh;
        for(; itu != etu; ++itu) (*itu)->StatisticSize(this);
    }
    EnsureInclude(parent); //按布局方式撑大范围给上一级
}
void GNu::cUI_GROUP::UpdateRun(unsigned int updatetime, sUI_LEVEL *parent) {
    if(!(c & 0xff000000)) return;
    sUI_LEVEL uilv; /*模版代码按布局方式计算到绘制绝对坐标*/
    sUI_LEVEL *_uilv = &uilv;
    uilv.node = this;
    uilv.parent = parent;
    DrawRangeGet(uilv, parent);

    GN::sLIST<P<iUI_OBJECT>>::iterator itu = aSubs.begin(), etu = aSubs.end();
    if(itu != etu) {
        iUI_OBJECT *_ui = (*itu);
        bool frist_is_bkf = _ui->aType == GNg::eOBJ_FRAME_RC && ((sUI_FRAME_RC *)_ui)->zAm_mode > GNg::eAM_ONCE_KEEP;
        if(frist_is_bkf) {
            _ui->UpdateRun(updatetime, _uilv);
            if((_uilv->c & 0xff000000) < 0x0a000000) return;
            ++itu;
        }
        GNg::cVIEW2D *view = GNg::ViewCur;
        if(aUsScissor) view->ScissorRectSetup(uilv.x, uilv.y, uilv.w, uilv.h);

        if(gw != uilv.w || gh != uilv.h) {
            for(; itu != etu; ++itu) (*itu)->OnRePos(_uilv);
            itu = aSubs.begin();
            if(frist_is_bkf) ++itu;
        } else if(gx != uilv.x || gy != uilv.y) {
            short mx = uilv.x - gx, my = uilv.y - gy;
            for(; itu != etu; ++itu) (*itu)->OnMove(mx, my);
            itu = aSubs.begin();
            if(frist_is_bkf) ++itu;
        }
        short mouse_x = view->mouseMX, mouse_y = view->mouseMY; /*GNg::ViewCur->HitRange*/
        if(mouse_x > uilv.x && mouse_x < uilv.x + uilv.w && mouse_y > uilv.y && mouse_y < uilv.y + uilv.h) RequestInputUiObject = this;

        if(RecvInputUiObject == this) {
            for(; itu != etu; ++itu) {
                RecvInputUiObject = _ui = (*itu);
                _ui->UpdateRun(updatetime, _uilv);
            }
        } else {
            for(; itu != etu; ++itu) (*itu)->UpdateRun(updatetime, _uilv);
        }
        if(aUsScissor) view->ScissorRectRemove();
    }
    gx = uilv.x;
    gy = uilv.y;
    gw = uilv.w;
    gh = uilv.h;
    gc = uilv.c;
}
void GNu::cUI_GROUP::GetDrawArea(GNg::sDRAW_AREA2 &out, cUI_GROUP *parent) {
    out.x = gx;
    out.y = gy;
    out.w = gw;
    out.h = gh;
    out.c = gc;
}
P<GNu::iUI_OBJECT> GNu::sUI_IMAGE::Clone() {
    P<sUI_IMAGE> uimg(1);
    *uimg = *this;
    return uimg;
}
P<GNu::iUI_OBJECT> GNu::cUI_IMAGE::Clone() {
    P<cUI_IMAGE> uimg(1);
    *uimg = *this;
    return uimg;
}
void GNu::sUI_IMAGE::GetDrawArea(GNg::sDRAW_AREA2 &out, cUI_GROUP *parent) {
    out.x = x + parent->gx;
    out.y = y + parent->gy;
    out.w = w;
    out.h = h;
    out.c = GNg_MultiplyColorRatio(c, parent->gc);
}
void GNu::sUI_IMAGE::UpdateRun(unsigned int updatetime, sUI_LEVEL *parent) {
    unsigned int dc = GNg_MultiplyColorRatio(c, parent->c); //GN_RunValueCheck2(parent->x,6); GN_RunValueCheck2(parent->y,7); GN_RunValueCheck2(x,8); GN_RunValueCheck2(y,9); GN_RunValueCheck2(w,10); GN_RunValueCheck2(h,11);  GN_RunValueCheck2(dc,12);
    GNg::ImageDraw(img, parent->x + x, parent->y + y, w, h, dc, 0, bmode);
}
void GNu::cUI_IMAGE::UpdateRun(unsigned int updatetime, sUI_LEVEL *parent) {
    GNg::sDRAW_AREA2 rng;
    DrawRangeGet(rng, parent);
    GNg::ImageDraw(img, rng.x, rng.y, rng.w, rng.h, rng.c, 0, bmode);
}
void GNu::cUI_IMAGE::GetDrawArea(GNg::sDRAW_AREA2 &out, cUI_GROUP *parent) {
    cUI_IMAGE::DrawRangeGet(out, (GNg::sDRAW_AREA2 *)&(parent->gx));
}
void GNu::cUI_IMAGE::DrawRangeGet(GNg::sDRAW_AREA2 &out, GNg::sDRAW_AREA2 *rng) {
    out.w = w;
    out.h = h;
    if(ntype_x) {
        if(ntype_x == 1) out.x = x + rng->x;
        else if(ntype_x == 3) {
            if(w > 0) {
                out.x = x + rng->x;
                out.w = rng->w - x - nmin_x;
            } else {
                out.w = x + w - (rng->w - nmin_x);
                out.x = (rng->w - nmin_x) + rng->x;
            }
        } else out.x = rng->x + rng->w - (x + w);
    } else out.x = rng->x + (short)((int)(rng->w - (nmin_x << 1) - (w > 0 ? w : -w)) * (*((unsigned short *)&x)) >> 16) + nmin_x - (w > 0 ? 0 : w);
    if(ntype_y) {
        if(ntype_y == 1) out.y = y + rng->y;
        else if(ntype_y == 3) {
            if(h > 0) {
                out.y = y + rng->y;
                out.h = rng->h - y - nmin_y;
            } else {
                out.h = y + h - (rng->h - nmin_y);
                out.y = (rng->h - nmin_y) + rng->y;
            }
        } else out.y = rng->y + rng->h - (y + h);
    } else out.y = rng->y + (short)((int)(rng->h - (nmin_y << 1) - (h > 0 ? h : -h)) * (*((unsigned short *)&y)) >> 16) + nmin_y - (h > 0 ? 0 : h);

    out.c = GNg_MultiplyColorRatio(c, rng->c);
}
void GNu::cUI_IMAGE::StatisticSize(cUI_GROUP *rng) {
    //EnsureInclude(parent);
    short size;
    if(ntype_x != eNEAR_SCALE) {
        if(ntype_x) size = (w > 0 ? w : 0) + x + nmin_x;
        else size = (w > 0 ? w : -w) + (nmin_x << 1);
        if(rng->w < size) rng->w = size;
    }
    if(ntype_y != eNEAR_SCALE) {
        if(ntype_y) size = (h > 0 ? h : 0) + y + nmin_y;
        else size = (h > 0 ? h : -h) + (nmin_y << 1);
        if(rng->h < size) rng->h = size;
    }
}
void GNu::cUI_BACK_FRAME::StatisticSize(cUI_GROUP *parent) {
    if(parent->w < w) parent->w = w;
    if(parent->h < h) parent->h = h;
}
void GNu::cUI_BACK_FRAME::GetDrawArea(GNg::sDRAW_AREA2 &out, cUI_GROUP *parent) {
#define GT_TMP_GET_BACK_FRAME_AREA(X, Y, W, H, C, PNODE) \
    C = GNg_MultiplyColorRatio(c, rng->c);               \
    if(x > 0 || w < PNODE->lw) {                         \
        X = rng->x + x;                                  \
        W = w;                                           \
    } else {                                             \
        X = rng->x;                                      \
        W = rng->w;                                      \
    }                                                    \
    if(y > 0 || h < PNODE->lh) {                         \
        Y = rng->y + y;                                  \
        H = h;                                           \
    } else {                                             \
        Y = rng->y;                                      \
        H = rng->h;                                      \
    }

    GNg::sDRAW_AREA2 *rng = (GNg::sDRAW_AREA2 *)&(parent->gx);
    GT_TMP_GET_BACK_FRAME_AREA(out.x, out.y, out.w, out.h, out.c, parent)
}
void GNu::cUI_BACK_FRAME::UpdateRun(unsigned int updatetime, sUI_LEVEL *rng) {
    short dx, dy, dw, dh;
    GT_TMP_GET_BACK_FRAME_AREA(dx, dy, dw, dh, unsigned int fc, rng->node)
    GNg::sIMAGE *_img = img;
    if(aBackBlurLv) GT_DrawBlurScreenImg(dx, dy, dw, dh, fc, (aBackBlurLv << 10) | (((uint8)aBackRepeat | 2) << 8) | (_img ? GNg::eBLEND_DST_ALPHA_MASK : bmode), _img);
    else GNg::ImageDraw9Square(_img, dx, dy, dw, dh, fc, bmode, aBackRepeat);
    if(RecvInputUiObject != this) return;

    if(aDragBarH) {
        if(zDragMask == 1) { //顶栏拖动处理
            if(GN::Mouse[GM_LEFT]) {
                if(GN::MouseMKS & GMM_LEFT) {
                    short msx = GNg::ViewCur->mouseMSX, msy = GNg::ViewCur->mouseMSY;
                    cUI_GROUP *_p = rng->node;
#define GT_TMP_BACK_FRAME_BAR_OFS(X, W)                             \
    if(_p->ntype_##X & 1) {                                         \
        _p->X += ms##X;                                             \
        if(_p->ntype_##X == 3) _p->nmin_##X -= ms##X;               \
    } else if(_p->ntype_##X == 2) _p->X -= ms##X;                   \
    else {                                                          \
        int loc_##W = rng->parent->W - (_p->nmin_##X << 1) - _p->W; \
        if(loc_##W) _p->X += ms##X * 65535 / loc_##W;               \
    }
                    GT_TMP_BACK_FRAME_BAR_OFS(x, w)
                    GT_TMP_BACK_FRAME_BAR_OFS(y, h)
                }
            } else zDragMask = 0;
        } else if(GN::Mouse1[GM_LEFT] && GNg::ViewCur->HitRange(dx, dy, dw, aDragBarH)) zDragMask = 1;
    }
    if(aZoom) {
        if(zDragMask & 254) {
            if(GN::Mouse[GM_LEFT]) {
                if(GN::MouseMKS & GMM_LEFT) { //拖拉边筐处理
                    GNg::cVIEW2D *view = GNg::ViewCur;
                    short msx = view->mouseMSX, msy = view->mouseMSY;
                    cUI_GROUP *_p = rng->node;
                    if(zDragMask & 2) {
                        if(msy > h - 64) msy = h - 64;
                        if(y > 0 || h < _p->lh) { //内顶边
                            y += msy;
                            h -= msy;
                        } else { //外顶边
                            _p->lh -= msy;
                            h -= msy;
                            if(_p->ntype_y & 1) _p->y += msy;
                            else if(!_p->ntype_y) {
                                int loc_h = rng->parent->h - (_p->nmin_y << 1) - _p->h;
                                if(loc_h + msy) *((unsigned short *)&_p->y) = (loc_h * (*((unsigned short *)&_p->y)) + ((int)msy << 16)) / (loc_h + msy);
                            }
                        }
                    } else if(zDragMask & 4) {
                        if(msy < 64 - h) msy = 64 - h;
                        if(y > 0 || h < _p->lh) { //内下边
                            h += msy;
                        } else { //外下边
                            _p->lh += msy;
                            h += msy;
                            if(_p->ntype_y == 2) _p->y -= msy;
                            else if(_p->ntype_y == 3) _p->nmin_y -= msy;
                            else if(!_p->ntype_y) {
                                int loc_h = rng->parent->h - (_p->nmin_y << 1) - _p->h;
                                *((unsigned short *)&_p->y) = loc_h * (*((unsigned short *)&_p->y) + 1) / (loc_h - msy);
                            }
                        }
                    }
                    if(zDragMask & 8) {
                        if(msx > w - 64) msx = w - 64;
                        if(x > 0 || w < _p->lw) { //内左边
                            x += msx;
                            w -= msx;
                        } else { //外左边
                            _p->lw -= msx;
                            w -= msx;
                            if(_p->ntype_x & 1) _p->x += msx;
                            else if(!_p->ntype_x) {
                                int loc_w = rng->parent->w - (_p->nmin_x << 1) - _p->w;
                                if(loc_w + msx) *((unsigned short *)&_p->x) = (loc_w * (*((unsigned short *)&_p->x)) + ((int)msx << 16)) / (loc_w + msx);
                            }
                        }
                    } else if(zDragMask & 16) {
                        if(msx < 64 - w) msx = 64 - w;
                        if(x > 0 || w < _p->lw) { //内右边
                            w += msx;
                        } else { //外右边
                            _p->lw += msx;
                            w += msx;
                            if(_p->ntype_x == 2) _p->x -= msx;
                            else if(_p->ntype_x == 3) _p->nmin_x -= msx;
                            else if(!_p->ntype_x) {
                                int loc_w = rng->parent->w - (_p->nmin_x << 1) - _p->w;
                                *((unsigned short *)&_p->x) = loc_w * (*((unsigned short *)&_p->x) + 1) / (loc_w - msx);
                            }
                        }
                    }
                }
            } else zDragMask = 0;
        } else if(GN::Mouse1[GM_LEFT] && GNg::ViewCur->HitRange(dx, dy, dw, dh)) {
            short mouse_x = GNg::ViewCur->mouseMX, mouse_y = GNg::ViewCur->mouseMY;
            if(mouse_y < dy + 5) {
                if(zDragMask == 1) zDragMask = 2;
                else zDragMask |= 2;
            } else if(mouse_y > dy + dh - 5) {
                if(zDragMask == 1) zDragMask = 4;
                zDragMask |= 4;
            }
            if(mouse_x < dx + 5) {
                if(zDragMask == 1) zDragMask = 8;
                zDragMask |= 8;
            } else if(mouse_x > dx + dw - 5) {
                if(zDragMask == 1) zDragMask = 16;
                zDragMask |= 16;
            }
        }
    }
}
P<GNu::iUI_OBJECT> GNu::cUI_BACK_FRAME::Clone() {
    P<cUI_BACK_FRAME> bf(1);
    *bf = *this;
    return bf;
}
P<GNu::iUI_OBJECT> GNu::sUI_FRAME_RC::Clone() {
    P<sUI_FRAME_RC> ufrc(1);
    *ufrc = *this;
    return ufrc;
}
void GNu::sUI_FRAME_RC::GetDrawArea(GNg::sDRAW_AREA2 &out, cUI_GROUP *parent) {
    out.c = GNg_MultiplyColorRatio(aColor, parent->gc);
    out.x = (short)aFrame_ts.x + parent->gx;
    out.y = (short)aFrame_ts.y + parent->gy;
    out.w = 0;
    out.h = 0;
}
void GNu::sUI_FRAME_RC::UpdateRun(unsigned int updatetime, sUI_LEVEL *parent) {
#define GT_TMP_UI_FRAME_UPDATERUN(X, Y, C)    \
    float sx = X, sy = Y;                     \
    unsigned int sc = C;                      \
    C = GNg_MultiplyColorRatio(C, parent->c); \
    X += (float)parent->x;                    \
    Y += (float)parent->y;                    \
    PlayFrame(updatetime);                    \
    C = sc;                                   \
    X = sx;                                   \
    Y = sy;

    GT_TMP_UI_FRAME_UPDATERUN(aFrame_ts.x, aFrame_ts.y, aColor)
    if(zAm_mode > GNg::eAM_ONCE_KEEP) {
        if(aBlendmode) {
            parent->x = aLevel_range.left;
            parent->w = aLevel_range.GetWidth();
            parent->y = aLevel_range.top;
            parent->h = aLevel_range.GetHeight();
        } else {
            float wf = (float)(-parent->w) * GetOriPointU();
            parent->x += wf * zFrame_ts.sx - wf + zFrame_ts.x;
            parent->w = (float)parent->w * zFrame_ts.sx;
            float hf = (float)(-parent->h) * GetOriPointV();
            parent->y += hf * zFrame_ts.sy - hf + zFrame_ts.y;
            parent->h = (float)parent->h * zFrame_ts.sy;
        }
        if(parent->w < 0) {
            parent->x += parent->w;
            parent->w = -(parent->w);
        }
        if(parent->h < 0) {
            parent->y += parent->h;
            parent->h = -(parent->h);
        }
        parent->c = aLevel_color;
    }
}
#if 0
void GNu::cUI_FRAME_RC::StatisticSize(cUI_GROUP *parent){
	float fw, fh;
	GetOriLocalSize(&fw, &fh);
	w = (short)(fw * aFrame_ts.sx);
	h = (short)(fh * aFrame_ts.sy);
	x = (short)aFrame_ts.x;
	y = (short)aFrame_ts.y;

	EnsureInclude(parent);
}
#endif
P<GNu::iUI_OBJECT> GNu::cUI_FRAME_RC::Clone() {
    P<cUI_FRAME_RC> ufrc(1);
    *ufrc = *this;
    return ufrc;
}
void GNu::cUI_FRAME_RC::zSetDrawPos(short dx, short dy) {
    aFrame_ts.x = (float)dx;
    aFrame_ts.y = (float)dy;
    if(zRcLoc.left) aFrame_ts.x -= zRcLoc.left * aFrame_ts.sx; //修正旋转中心原点不再左上角的不直观问题
    if(zRcLoc.top) aFrame_ts.y -= zRcLoc.top * aFrame_ts.sy;
    if(aFrame_ts.sx < 0.0f) aFrame_ts.x += (float)w;
    if(aFrame_ts.sy < 0.0f) aFrame_ts.y += (float)h;
}
void GNu::cUI_FRAME_RC::UpdateRun(unsigned int updatetime, sUI_LEVEL *parent) {
    c = aColor;
    short dx, dy;
    aColor = DrawBeginPosGet(dx, dy, parent);
    float sx = aFrame_ts.x, sy = aFrame_ts.y;
    zSetDrawPos(dx, dy);
    PlayFrame(updatetime);
    aColor = c;
    aFrame_ts.x = sx;
    aFrame_ts.y = sy;
}
void GNu::cUI_FRAME_RC::GetDrawArea(GNg::sDRAW_AREA2 &out, cUI_GROUP *parent) {
    out.w = w;
    out.h = h;
    out.c = DrawBeginPosGet(out.x, out.y, (GNg::sDRAW_AREA2 *)&(parent->gx));
}
void GNu::cUI_FRAME::GetDrawArea(GNg::sDRAW_AREA2 &out, cUI_GROUP *parent) {
    ((sUI_FRAME_RC *)this)->GetDrawArea(out, parent);
}
P<GNu::iUI_OBJECT> GNu::cUI_FRAME::Clone() {
    P<cUI_FRAME> uframe(1);
    *uframe = *this;
    return uframe;
}
P<GNu::iUI_OBJECT> GNu::cUI_FRAME_LAUNCHER::Clone() {
    P<cUI_FRAME_LAUNCHER> u_launch(1);
    u_launch->PerfectCopy(*((GNg::cFRAME_LAUNCHER *)this));
    return u_launch;
}
P<GNu::iUI_OBJECT> GNu::cUI_FRAME_LAUNCHER2D::Clone() {
    P<cUI_FRAME_LAUNCHER2D> u_launch2d(1);
    u_launch2d->PerfectCopy(*((GNg::cFRAME_LAUNCHER2D *)this));
    return u_launch2d;
}
P<GNu::iUI_OBJECT> GNu::cUI_FRAME_LAUNCHER_FONT::Clone() {
    P<cUI_FRAME_LAUNCHER_FONT> u_flaunch(1);
    cUI_FRAME_LAUNCHER_FONT *_u_flaunch = u_flaunch;
    memcpy((GNg::sDRAW_IN_AREA *)((cUI_NEAR_OBJECT *)_u_flaunch), (GNg::sDRAW_IN_AREA *)((cUI_NEAR_OBJECT *)this), sizeof(GNg::sDRAW_IN_AREA));
    _u_flaunch->PerfectCopy(*((GNg::cFRAME_LAUNCHER_FONT *)this));
    return u_flaunch;
}
void GNu::cUI_FRAME::UpdateRun(unsigned int updatetime, sUI_LEVEL *parent) {
    GT_TMP_UI_FRAME_UPDATERUN(aFrame_ts.x, aFrame_ts.y, aColor)
}
void GNu::cUI_FRAME_LAUNCHER::UpdateRun(unsigned int updatetime, sUI_LEVEL *parent) {
    GT_TMP_UI_FRAME_UPDATERUN(aGenRngX, aGenRngY, aGenColor)
}
void GNu::cUI_FRAME_LAUNCHER2D::UpdateRun(unsigned int updatetime, sUI_LEVEL *parent) {
    GT_TMP_UI_FRAME_UPDATERUN(aGenRngX, aGenRngY, aGenColor)
}
void GNu::cUI_FRAME_LAUNCHER_FONT::UpdateRun(unsigned int updatetime, sUI_LEVEL *parent) {
    if(zActMouseAuto) {
        zActMouseAuto = (RecvInputUiObject == this);
        if(!PlayFrame(updatetime, parent->c)) {
            if(zDisplay_num) {
                zDisplay_num = 0;
                UiActionCall(this, eUI_ACT_VANISH);
            }
        }
        zActMouseAuto = true;
    } else PlayFrame(updatetime, parent->c);
}
void GNu::cUI_FRAME_LAUNCHER::GetDrawArea(GNg::sDRAW_AREA2 &out, cUI_GROUP *parent) {
    out.x = (short)aGenRngX + parent->gx;
    out.y = (short)aGenRngY + parent->gy;
    out.w = aGenRngW;
    out.h = aGenRngH;
    out.c = GNg_MultiplyColorRatio(aGenColor, parent->gc);
}
void GNu::cUI_FRAME_LAUNCHER2D::GetDrawArea(GNg::sDRAW_AREA2 &out, cUI_GROUP *parent) {
    float w2 = aGenRngW * 0.5f, h2 = aGenRngH * 0.5f; /*模版代码:粗略计算复合图筐发射器范围*/
    GNg::sVEC2 cp = zGenRngAx * w2 + zGenRngAy * h2 + GNg::sVEC2(aGenRngX, aGenRngY);
    out.x = (short)(cp.x - w2) + parent->gx;
    out.y = (short)(cp.y - h2) + parent->gy;
    out.w = aGenRngW;
    out.h = aGenRngH;
    out.c = GNg_MultiplyColorRatio(aGenColor, parent->gc);
}
void GNu::cUI_FRAME_LAUNCHER_FONT::GetDrawArea(GNg::sDRAW_AREA2 &out, cUI_GROUP *parent) {
    out.x = aGenRngX;
    out.y = aGenRngY;
    out.w = aGenRngW;
    out.h = aGenRngH;
    out.c = GNg_MultiplyColorRatio(aGenColor, parent->gc);
}
void GNu::cUI_FRAME_LAUNCHER_FONT::OnRePos(sUI_LEVEL *parent) {
    short dx, dy;
    DrawBeginPosGet(dx, dy, parent);
    if(!aGenRngH) {
        dx += (float)w * zParame.near_pointU;
        dy += (float)h * zParame.near_pointV;
    }
    GNg::cFRAME_LAUNCHER_FONT::Move(dx - (short)aGenRngX, dy - (short)aGenRngY);
}
void GNu::cUI_FRAME_LAUNCHER_FONT::OnMove(short mx, short my) {
    GNg::cFRAME_LAUNCHER_FONT::Move(mx, my);
}
#if 0
void GNu::cUI_FRAME_LAUNCHER_FONT::StatisticSize(cUI_GROUP *parent){
	w = (short)aGenRngW; 
	h = (short)aGenRngH;
	x = (short)aGenRngX;
	y = (short)aGenRngY;
	
	EnsureInclude(parent);
}
#endif
P<GNu::iUI_OBJECT> GNu::cUI_TEXT_ART::Clone() {
    P<GNu::cUI_TEXT_ART> ui_text(1);
    GNu::cUI_TEXT_ART *_ui_text = ui_text;
    *_ui_text = *this;
    char *cnt = _ui_text->text;
    _ui_text->text = new char[strlen(cnt) + 1];
    // TODO: verify buffer size
    strcpy(_ui_text->text, cnt);
    return ui_text;
}
void GNu::cUI_TEXT_ART::UpdateRun(unsigned int updatetime, sUI_LEVEL *parent) {
    if(fw && fh) {
        w = fw;
        h = fh;
        GNg::sDRAW_AREA2 rng;
        DrawRangeGet(rng, parent);
        textstyle->OutText(text, 0, rng.x, rng.y, rng.w, rng.h, rng.c, blendmode, offsetx, offsety);
    } else {
        short dx, dy;
        unsigned int dc = DrawBeginPosGet(dx, dy, parent);
        textstyle->OutText(text, 0, dx, dy, 0, 0, dc, blendmode, offsetx, offsety);
        GNg::sRECT rc2;
        textstyle->GetTextRange(&rc2, textstyle->GetVisualStrCount());
        w = rc2.GetWidth();
        h = rc2.GetHeight();
    }
}
void GNu::cUI_TEXT_ART::GetDrawArea(GNg::sDRAW_AREA2 &out, cUI_GROUP *parent) {
    if(fw && fh) DrawRangeGet(out, (GNg::sDRAW_AREA2 *)&(parent->gx));
    else {
        out.w = w;
        out.h = h;
        out.c = DrawBeginPosGet(out.x, out.y, (GNg::sDRAW_AREA2 *)&(parent->gx));
    }
}
/*-----------------------------------------------------------------------------------------------------------------------------*/
void GNu::cUI_PTR::UnComment(P<iUI_OBJECT> *obj) {
    if(aPtr && zParent) {
        if(obj) *obj = *aPtr;
        zParent->aSubs.del(aPtr);
        aPtr = 0;
    }
}
void GNu::cUI_GROUP::Comment(P<iUI_OBJECT> obj, cUI_PTR *ptr_out) {
    if(ptr_out) {
        ptr_out->zParent = this;
        ptr_out->aPtr = aSubs.add(obj);
    } else aSubs.push_back(obj);
}
void GNu::cUI_PTR::InsertFront(P<iUI_OBJECT> obj, cUI_PTR *ptr_out) {
    if(aPtr && zParent) {
        GN::sLIST<P<iUI_OBJECT>>::iterator ito = zParent->aSubs.insert(++(zParent->aSubs.to_iterator(aPtr)), obj);
        if(ptr_out) {
            ptr_out->zParent = zParent;
            ptr_out->aPtr = &(*ito);
        }
    }
}
void GNu::cUI_PTR::InsertBack(P<iUI_OBJECT> obj, cUI_PTR *ptr_out) {
    if(aPtr && zParent) {
        GN::sLIST<P<iUI_OBJECT>>::iterator ito = zParent->aSubs.insert(zParent->aSubs.to_iterator(aPtr), obj);
        if(ptr_out) {
            ptr_out->zParent = zParent;
            ptr_out->aPtr = &(*ito);
        }
    }
}
int GNu::cUI_PTR::OrderMove(int fn) {
    if(fn && aPtr && zParent)
        return zParent->aSubs.order(&aPtr, fn);
    else return 0;
}
GNu::cUI_PTR GNu::cUI_PTR::Clone() {
    GNu::cUI_PTR new_obj;
    if(aPtr && zParent) {
        new_obj.zParent = zParent;
        new_obj.aPtr = zParent->aSubs.add((*aPtr)->Clone());
    }
    return new_obj;
}
/*-----------------------------------------------------------------------------------------------------------------------------*/
//从组里查找UI组件(组, 查找UI组件名, 当前在UI组件名表里历遍到的名, UI组件地址输出)找到UI组件了返回0 否则返回当前历遍到的名
static char **GT_SearchUiObjFromGroup(GNu::cUI_GROUP *node, const char *name, char **ppname, GNu::cUI_PTR *_optr) {
    GN::sLIST<P<GNu::iUI_OBJECT>>::iterator ito = node->aSubs.begin(), eto = node->aSubs.end();
    for(; ito != eto; ++ito) {
        if(!strcmp(*ppname, name)) {
            _optr->zParent = node;
            _optr->aPtr = &(*ito);
            return 0;
        }
        ++ppname;
        if((*ito)->aType == GNg::eOBJ_GROUP || (*ito)->aType == GNu::eUI_SCROLL_RECT) {
            ppname = GT_SearchUiObjFromGroup(CVP(GNu::cUI_GROUP)(*ito), name, ppname, _optr);
            if(!ppname) return 0;
        }
    }
    return ppname;
}
//从组里查找指定UI组件(组, UI组件, UI组件地址输出)返回是否找到了UI组件
static bool GT_SearchUiObjFromGroup(GNu::cUI_GROUP *node, P<GNu::iUI_OBJECT> &obj, GNu::cUI_PTR *_optr) {
    GN::sLIST<P<GNu::iUI_OBJECT>>::iterator ito = node->aSubs.begin(), eto = node->aSubs.end();
    for(; ito != eto; ++ito) {
        if((*ito) == obj) {
            _optr->zParent = node;
            _optr->aPtr = &(*ito);
            return true;
        }
        if(((*ito)->aType == GNg::eOBJ_GROUP || (*ito)->aType == GNu::eUI_SCROLL_RECT) && GT_SearchUiObjFromGroup(CVP(GNu::cUI_GROUP)(*ito), obj, _optr)) return true;
    }
    return false;
}
GNu::cUI_PTR GNu::cUI_GROUP::Search(P<iUI_OBJECT> obj) {
    GNu::cUI_PTR optr;
    if(aType == GNg::eOBJ_GROUP || aType == GNu::eUI_SCROLL_RECT) GT_SearchUiObjFromGroup(this, obj, &optr);
    return optr;
}
GNu::cUI_PTR GNu::cUI_SCENE::Search(const char *name) {
    GT_TMP_SEARCH_SCENE_PTR(GNu::cUI_PTR, UiObj, 0)
}
extern GNu::cSIM_DIALOGEX *GT_FullSelectPathDlg;
bool GNu::cUI_SCENE::zNeedScreenImg = false;
unsigned int GNu::cUI_SCENE::zNeedScreenImgObjN = 0;

void GNu::cUI_SCENE::zDrawFinishProc() {
    if(zNeedScreenImgObjN) {
        zNeedScreenImgObjN = 0;
        zNeedScreenImg = true;
    }
}
void GNu::cUI_SCENE::Release() {
    aSubs.clear();
    zObjNameTable.Delete();
    GN_DeleteArray(zObjNameBuf);
    aFontMapList.clear();
    aTextStyleList.clear();
}
void GNu::cUI_SCENE::UpdateRun(unsigned int updatetime, sUI_LEVEL *parent) {
    if(zNeedScreenImg) {
        zNeedScreenImg = false;
        if(GNg::cVIEW2D::screenImgUpdateFps != GN::TimeGetRunFpsCount()) {
            GNg::cVIEW2D::screenImgUpdateFps = GN::TimeGetRunFpsCount();
            if(!GNg::cVIEW2D::screenImg) {
                if(GNg::sIMAGE::RenderTargetSupport) {
                    GNg::cVIEW2D::screenImg.New(2);
                    GNg::cVIEW2D::screenImg->CreateEmptyToData(128, 128);
                } else GNg::cVIEW2D::screenImg.New(1);
            }
            if(GNg::sIMAGE::RenderTargetSupport) {
                GNg::sIMAGE *scr_img = GNg::cVIEW2D::screenImg;
                GNg::sIMAGE *scr_img1 = scr_img + 1;
                GNg::ViewCur->SaveToImageData(scr_img1, 0, 0, 0, 0, false, true);
                scr_img->BeginRender();
                GNg::ImageDraw(scr_img1, 0, 0, scr_img1->width, scr_img1->height, 0xffffffff, 0, 0);
                scr_img->EndRender();
            } else
                GNg::ViewCur->SaveToImageData(GNg::cVIEW2D::screenImg, 0, 0, 0, 0, false, true);
        }
    }
    cUI_GROUP::UpdateRun(updatetime, parent);
}
void GNu::cUI_SCENE::UpdatePlay(unsigned int updatetime) {
    if(GT_FullSelectPathDlg) {
        GT_FullSelectPathDlg->UpdateRun(updatetime);
        if(GT_FullSelectPathDlg->GetCurDetailSetState() != eDETAIL_SET_PATH_SELECTF) GT_FullSelectPathDlg = 0;
        return;
    }
    //Resize(GNg::ViewCur); //主UI场景范围与视口截取范围相同

    GN::sLIST<P<iUI_OBJECT>>::iterator itu = aSubs.begin(), etu = aSubs.end();
    for(; itu != etu; ++itu) (*itu)->StatisticSize(this);
    w = lw;
    h = lh;

    sUI_LEVEL uilv0;
    memset(&uilv0, 0, sizeof(uilv0));
    uilv0.c = 0xffffffff;
    UpdateRun(updatetime, &uilv0);
}
GN_API void GT_WriteFramesToFile(unsigned int *rc_pos, uint8 *buf, uint8 in_launc, GN::sLIST<GNg::cFRAME *> &list_frame, GNg::cFRAME_RC::sREF_OBJSW *_in_frame_objs, GNf::sFILE *fp);
GN_API uint8 *GT_ReadFrames(uint8 *rpos, P<GNg::cFRAME *> &p_frames, GNg::cFRAME_RC::sREF_OBJSR *_frame_ref_objs);

void GT_Scene2DLoadShareObjData(GT_sSCENE2D_SHARE_OBJS_R &ref_objs, unsigned int *data_pos, const char *file_name, char *fbuf, float &progress);
void GT_Scene2DSaveShareObjData(GT_sSCENE2D_SHARE_OBJS &share_objs, GN::sLIST<GNg::sIMAGE *> *, unsigned int *rc_pos /*[10]*/, const char *exp_pathfile, GNf::sFILE *fp);
uint8 *GT_Scene2DLoadObjNameTable(uint8 *rpos, char **pname_buf, P<char *> &name_table);
char *GT_Scene2DLoadFileHead(const char *func_name, const char *file_name, const char *ext_name1);
GNf::sFILE *GT_Scene2DSaveFileHead(const char *class_name, const char *filename, uint8 *file_head /*[4]*/, int name_pos, char *name_buf, P<char *> &name_table);

//读取UI组件数据(当前读取位置, 当前加入UI组件的组, 引用共享对象集)返回最后的读完位置
uint8 *GT_UiObjLoadData(uint8 *rpos, GN::sLIST<P<GNu::iUI_OBJECT>> *cur_group, GNu::cSHARE_DATA *_ref_objs) {
    unsigned int size = *((unsigned int *)rpos), utype = *((unsigned int *)(rpos + 5));
    if(utype == GNg::eOBJ_GROUP || utype == GNu::eUI_SCROLL_RECT) { //GN_Log("A:%d %d %d\n", size, (int)rpos[4], utype);
        GNu::cUI_GROUP *_ug;
        if(utype == GNg::eOBJ_GROUP) {
            P<GNu::cUI_GROUP> ug(1);
            cur_group->push_back(ug);
            _ug = ug;
        } else {
            P<GNu::cSCROLL_RECT> ug2(1);
            cur_group->push_back(ug2);
            _ug = ug2;
        }
        _ug->DataLoad(_ref_objs, rpos + 4);
        rpos += size;
        while(*((unsigned int *)rpos) != 0xffffffff) rpos = GT_UiObjLoadData(rpos, &(_ug->aSubs), _ref_objs);
        rpos += 4;
    } else { //GN_Log("a:%d %d %d\n", size, (int)(rpos[4]), utype);
        P<GNu::iUI_OBJECT> node = GNu::UiObjectCreate(utype); //GN_Log("b\n");
        if(!node || !node->DataLoad(_ref_objs, rpos + 4)) { //读取失败 没装插件或插件版本太老不支持
            P<GNu::sUI_IMAGE> ui_img(1); //指定一个小白图来占位
            ui_img->Init(0, 0, 28, 28, 0xffffffff);
            ui_img->bmode = 1;
            node = ui_img;
        } //GN_Log("c\n");
        cur_group->push_back(node); //GN_Log("d\n");
        rpos += size;
    } //GN_Log("e\n");
    return rpos;
}
//写入UI组件数据到文件(UI组件, 文件, 临时缓冲, 缓冲大小, 收集共享对象集)
void GT_UiObjSaveData(GNu::iUI_OBJECT *node, GNf::sFILE *_fp, uint8 *buf, unsigned int buf_size, GNu::cSHARE_DATA *_share_objs) {
    unsigned int size, utype = node->aType;
    if(utype == GNg::eOBJ_GROUP || utype == GNu::eUI_SCROLL_RECT) {
        size = node->DataSave(_share_objs, buf + 4) + 4; //GN_Log("A:%d %d\n", size,  utype);
        *((unsigned int *)buf) = size;
        GNf::Write((char *)buf, 1, size, _fp);
        GN::sLIST<P<GNu::iUI_OBJECT>>::iterator ito = ((GNu::cUI_GROUP *)node)->aSubs.begin(), eto = ((GNu::cUI_GROUP *)node)->aSubs.end();
        for(; ito != eto; ++ito) GT_UiObjSaveData(*ito, _fp, buf, buf_size, _share_objs);
        unsigned int endg = 0xffffffff;
        GNf::Write((char *)&endg, 1, 4, _fp); //写入结束标记
    } else {
        if(utype == GNu::eUI_TEXT_ART || utype == GNu::eUI_TEXT_IN || utype == GNu::eUI_PALETTE || utype >= GNu::eUI_TYPE_NUM) {
            size = node->DataSave(_share_objs, 0) + 4;
            if(size > buf_size) {
                uint8 *buf_ = new uint8[size];
                node->DataSave(_share_objs, buf_ + 4);
                *((unsigned int *)buf_) = size;
                GNf::Write((char *)buf_, 1, size, _fp);
                delete[] buf_;
                return;
            }
        }
        size = node->DataSave(_share_objs, buf + 4) + 4; //GN_Log("a:%d %d\n", size, utype);
        *((unsigned int *)buf) = size;
        GNf::Write((char *)buf, 1, size, _fp);
    }
}
bool GNu::cUI_SCENE::InitLoadToData(const char *file_name, float &progress) {
    progress = 0.0f;
    char *fbuf = GT_Scene2DLoadFileHead("cUI_SCENE::InitLoadToData()", file_name, "gui");
    this->Release(); //GN_Log("2\n");
    progress = 0.1f;
    uint8 *rpos = (uint8 *)fbuf + 3;
    uint8 version = *rpos;
    ++rpos;
    unsigned int *data_pos = (unsigned int *)rpos;
    rpos += (18 << 2); //GN_Log("3\n");
    rpos = GT_Scene2DLoadObjNameTable(rpos, &zObjNameBuf, zObjNameTable); //GN_Log("4\n");
    progress = 0.11f;
    if(*((unsigned int *)rpos) == 0 || *((unsigned int *)(rpos + 5)) != GNg::eOBJ_GROUP) {
        delete[] fbuf;
        GN_Log("读取 %s 失败 文件可能损坏!\n", file_name);
        return false;
    }
    uint8 *r_node_pos = rpos;
    GT_cUI_SHARE_DATA_R share_objs; //GN_Log("5\n");
    GT_Scene2DLoadShareObjData(share_objs, data_pos, file_name, fbuf, progress); //GN_Log("6\n");
    if(data_pos[17] & 0x0000ffff) {
        unsigned int fmi = 0, fmn = data_pos[17] & 0x0000ffff;
        share_objs.image_refs.New(fmn);
        GNg::sIMAGE **_pimgs = share_objs.frame_ref_objs.pimgs, **_image_refs = share_objs.image_refs;
        // 初始化为NULL，防止野指针
        for(fmi = 0; fmi < fmn; ++fmi) _image_refs[fmi] = nullptr;
        fmi = 0;
        _pimgs += share_objs.frame_ref_objs.pimgs.Num() - fmn;
        for(; fmi < fmn; ++fmi) { //复制出各个字体图
            if(!_pimgs[fmi]) continue; // 跳过加载失败的字体(如被移除的GB2312)
            P<GNg::sIMAGE> ft_img(1);
            *ft_img = *(_pimgs[fmi]);
            ft_img->cloneImg = false;
            _pimgs[fmi]->cloneImg = true;
            aFontMapList.push_back(ft_img);
            _image_refs[fmi] = ft_img;
        }
    } //GN_Log("7\n");
    GT_ReadFrames((uint8 *)fbuf + data_pos[16], share_objs.pframes_rc, &(share_objs.frame_ref_objs)); //GN_Log("8\n");//读取独立的正规动画图筐
    progress = 0.79f;
    unsigned int si = (data_pos[17] & 0xffff0000) >> 16, num;
    if(si < 0xffff) {
        delete(share_objs.pframes[si]);
        share_objs.pframes[si] = GNu::cSCROLL_LIST::CheckInitDefaultFrameAct(); //替换成系统默认的效果筐
    }
    rpos = (uint8 *)fbuf + data_pos[15]; /*跳转到读取各个其它自定义共享对象*/
    memcpy(&num, rpos, 4);
    rpos += 4;
    if(num) {
        share_objs.sobj_refs.New(num);
        P<uint8> *_sobj_refs = share_objs.sobj_refs;
        for(si = 0; si < num; ++si) {
            UiShareObjLoad(_sobj_refs[si], rpos + 4);
            rpos += *((unsigned int *)rpos);
        }
    }
    progress = 0.795f;
    rpos = (uint8 *)fbuf + data_pos[14]; /*跳转到读取各个独立的声音对象*/
    memcpy(&num, rpos, 4);
    rpos += 4;
    if(num) {
        share_objs.psounds.New(num);
        P<GNa::sSOUND> *_sounds = share_objs.psounds;
        for(si = 0; si < num; ++si) {
            if(rpos[0]) {
                _sounds[si].New(1);
                if(!GNa::SoundLoad((char *)rpos, _sounds[si])) _sounds[si].Delete();
            }
            rpos += strlen((char *)rpos) + 1;
        }
    } //GN_Log("9\n");
    progress = 0.798f;
    rpos = (uint8 *)fbuf + data_pos[13]; /*跳转到读取各个文本*/
    memcpy(&num, rpos, 4);
    rpos += 4;
    if(num) {
        share_objs.text_refs.New(num);
        P<char> *_text_refs = share_objs.text_refs;
        for(si = 0; si < num; ++si) {
            if(rpos[0]) _text_refs[si]((char *)rpos, 0);
            rpos += strlen((char *)rpos) + 1;
        }
    }
    progress = 0.799f;
    rpos = (uint8 *)fbuf + data_pos[12]; /*跳转到读取各个文本数组*/
    memcpy(&num, rpos, 4);
    rpos += 4;
    if(num) {
        share_objs.ptexts_s.New(num);
        P<P<char>> *_ptexts_s = share_objs.ptexts_s;
        for(si = 0; si < num; ++si) rpos = GN::ObjectRefIdLoad(rpos, _ptexts_s[si], share_objs.text_refs);
    } //GN_Log("10\n");
    rpos = (uint8 *)fbuf + data_pos[11]; /*跳转到读取各个文本式样*/
    memcpy(&num, rpos, 4);
    rpos += 4;
    if(num) {
        share_objs.text_style_refs.New(num);
        GNg::cTEXT_ART **_text_style_refs = share_objs.text_style_refs;
        for(si = 0; si < num; ++si) {
            P<GNg::cTEXT_ART> tstyle(1);
            rpos = tstyle->zDataLoad(rpos, share_objs.image_refs);
            aTextStyleList.push_back(tstyle);
            _text_style_refs[si] = tstyle;
        }
    }
    progress = 0.8f;
    rpos = (uint8 *)fbuf + data_pos[10]; /*跳转到读取各个内嵌UI子组件*/
    memcpy(&num, rpos, 4);
    rpos += 4;
    if(num) {
        GN::sLIST<P<GNu::iUI_OBJECT>> uobj_list;
        for(si = 0; si < num; ++si) rpos = GT_UiObjLoadData(rpos, &uobj_list, &share_objs);
        share_objs.puobjs.New(uobj_list);
    } //GN_Log("11\n");
    progress = 0.82f;
    rpos = r_node_pos;
    this->DataLoad(&share_objs, rpos + 4);
    rpos += *((unsigned int *)rpos); //GN_Log("12\n");		//跟节点
    {
        int loop_safety = 10000;
        while(*((unsigned int *)rpos) != 0xffffffff && loop_safety-- > 0) {
            rpos = GT_UiObjLoadData(rpos, &aSubs, &share_objs);
        }
    }
    //rpos += 4;
    delete[] fbuf; //GN_Log("13\n");
    num = share_objs.ppframes.Num();
    if(num) {
        GNg::cFRAME **_pframes = share_objs.pframes;
        P<GNg::cFRAME> *_ppframes = share_objs.ppframes;
        for(si = 0; si < num; ++si) {
            if(_ppframes[si]) delete _pframes[si]; //删除已转为安全指针的原指针数据
        }
    }
    progress = 1.0f;
    return true;
}
bool GNu::cUI_SCENE::SaveToFile(const char *filename, uint8 version, bool exp_img, cUI_GROUP *node) {
    uint8 file_head[4] = {'g', 'u', 'i', version};
    GNf::sFILE *fp = GT_Scene2DSaveFileHead("cUI_SCENE", filename, file_head, (18 << 2), zObjNameBuf, zObjNameTable);
    if(!fp) return false;

    GT_cUI_SHARE_DATA_W share_objs;
    if(aTextStyleList.size()) GN::ListSpliceList(share_objs.text_style_list, aTextStyleList);
    if(aFontMapList.size()) GN::ListSpliceList(share_objs.image_list, aFontMapList);
    if(!node) node = this;
    const unsigned int buf_size1 = sizeof(GNg::cFRAME) << 3;
    const unsigned int buf_size = buf_size1 + (sizeof(GNg::sAM_NODE) << 5);
    uint8 buf[buf_size];
    //GN_Log("s1\n");
    GT_UiObjSaveData(node, fp, buf, buf_size, &share_objs); //GN_Log("s2\n");
    unsigned int rc_pos[18];
    uint8 *wpos;
    rc_pos[10] = (unsigned int)GNf::Tell(fp);
    unsigned int num = share_objs.list_uobj.size();
    GNf::Write((char *)&num, 4, 1, fp);
    if(num) { //写入各个内嵌UI子组件
        GN::sLIST<P<GNu::iUI_OBJECT>>::iterator iu = share_objs.list_uobj.begin(), eu = share_objs.list_uobj.end();
        for(; iu != eu; ++iu) GT_UiObjSaveData(*iu, fp, buf, buf_size, &share_objs);
    } //GN_Log("s2_1\n");
    rc_pos[11] = (unsigned int)GNf::Tell(fp);
    num = share_objs.text_style_list.size();
    GNf::Write((char *)&num, 4, 1, fp);
    if(num) { //写入各个文本式样
        GN::sLIST<GNg::cTEXT_ART *>::iterator itx = share_objs.text_style_list.begin(), etx = share_objs.text_style_list.end();
        for(; itx != etx; ++itx) {
            wpos = (*itx)->zDataSave(buf, share_objs.image_list);
            GNf::Write((char *)buf, 1, wpos - buf, fp);
        }
    } //GN_Log("s2_2\n");
    rc_pos[12] = (unsigned int)GNf::Tell(fp);
    num = share_objs.list_texts.size();
    GNf::Write((char *)&num, 4, 1, fp);
    if(num) { //写入各个文本数组
        GN::sLIST<P<P<char>>>::iterator istrs = share_objs.list_texts.begin(), estrs = share_objs.list_texts.end();
        for(; istrs != estrs; ++istrs) {
            if(((*istrs).Num() + 1) << 2 > buf_size) {
                uint8 *buf_ = new uint8[((*istrs).Num() + 1) << 2];
                wpos = GN::ObjectRefIdSave(buf_, *istrs, share_objs.text_list);
                GNf::Write((char *)buf_, 1, wpos - buf_, fp);
                delete[] buf_;
            } else {
                wpos = GN::ObjectRefIdSave(buf, *istrs, share_objs.text_list);
                GNf::Write((char *)buf, 1, wpos - buf, fp);
            }
        }
    } //GN_Log("s2_3\n");
    rc_pos[13] = (unsigned int)GNf::Tell(fp);
    num = share_objs.text_list.size();
    GNf::Write((char *)&num, 4, 1, fp);
    if(num) { //写入各个文本
        GN::sLIST<P<char>>::iterator istr = share_objs.text_list.begin(), estr = share_objs.text_list.end();
        unsigned int num1;
        for(num = 0; istr != estr; ++istr) {
            num1 = (*istr).Num();
            if(num + num1 > buf_size) {
                if(num) {
                    GNf::Write((char *)buf, 1, num, fp);
                    num = 0;
                }
                if(num1 > buf_size) {
                    GNf::Write((*istr), 1, num1, fp);
                    continue;
                }
            }
            strncpy((char *)buf + num, (*istr), num1);
            num += num1;
        }
        if(num) GNf::Write((char *)buf, 1, num, fp);
    } //GN_Log("s2_4\n");
    rc_pos[14] = (unsigned int)GNf::Tell(fp);
    num = share_objs.list_sound.size();
    GNf::Write((char *)&num, 4, 1, fp);
    if(num) { //写入各个声音斋文件名
        GN::sLIST<P<GNa::sSOUND>>::iterator isu = share_objs.list_sound.begin(), esu = share_objs.list_sound.end();
        char exp_path[384], *_filename;
        if(exp_img) {
            strncpy(exp_path, filename, sizeof(exp_path) - 1);
            exp_path[sizeof(exp_path) - 1] = 0;
            _filename = GN_StrGetFileName(exp_path);
        }
        GNf::sFILE *fp1;
        int fsize;
        unsigned int num1;
        char *soud_filename, *fbuf;
        for(num = 0; isu != esu; ++isu) {
            soud_filename = GN_StrGetFileName((*isu)->fileName);
            if(exp_img) { //导出声音文件
                // TODO: verify buffer size
                strcpy(_filename, soud_filename);
                fbuf = GNf::FileFullLoadToMemory((*isu)->fileName, &fsize);
                if(fbuf) {
                    fp1 = GNf::Open(exp_path, "wb");
                    if(fp1) {
                        GNf::Write(fbuf, 1, fsize, fp1);
                        GNf::Close(fp1);
                    }
                    delete[] fbuf;
                }
            }
            num1 = strlen(soud_filename) + 1; //写斋文件
            if(num + num1 > buf_size) {
                GNf::Write((char *)buf, 1, num, fp);
                num = 0;
            }
            strncpy((char *)buf + num, soud_filename, num1);
            num += num1;
        }
        if(num) GNf::Write((char *)buf, 1, num, fp);
    }
    rc_pos[15] = (unsigned int)GNf::Tell(fp);
    num = share_objs.sobj_type_list.size();
    GNf::Write((char *)&num, 4, 1, fp); //GN_Log("s2_5:%d\n", num);
    if(num) { //写入各个其它自定义共享对象
        GN::sLIST<unsigned int>::iterator iot = share_objs.sobj_type_list.begin(), eot = share_objs.sobj_type_list.end();
        GN::sLIST<P<uint8>>::iterator isob = share_objs.sobj_list.begin();
        for(; iot != eot; ++iot, ++isob) {
            num = UiShareObjSave(*iot, *isob, 0) + 4;
            if(num <= buf_size) {
                UiShareObjSave(*iot, *isob, buf + 4);
                *((unsigned int *)buf) = num;
                GNf::Write((char *)buf, 1, num, fp);
            } else {
                uint8 *buf_ = new uint8[num];
                UiShareObjSave(*iot, *isob, buf_ + 4);
                *((unsigned int *)buf_) = num;
                GNf::Write((char *)buf_, 1, num, fp);
                delete[] buf_;
            }
        }
    } //GN_Log("s3\n");
    GT_WriteFramesToFile(rc_pos + 16, buf, 0, share_objs.list_frame_rc, &(share_objs.in_frame_objs), fp); //GN_Log("s4\n"); //写入独立的正规动画图筐
    rc_pos[17] = share_objs.image_list.size(); //记录字图数
    GT_Scene2DSaveShareObjData(share_objs, &(share_objs.image_list), rc_pos, exp_img ? filename : 0, fp); //GN_Log("s6\n");
    if(share_objs.list_fileicon.size()) {
        GN::sLIST<GNg::cFRAME *>::iterator itf = share_objs.list_fileicon.begin(), etf = share_objs.list_fileicon.end();
        for(; itf != etf; ++itf) (*itf)->zPlay_obj = GNu::cFILE_BROWSER::GetIcon();
    }
    rc_pos[17] |= share_objs.scroll_act_frame_id << 16; //并入默认点中演变效果筐所在ID

    GNf::Seek(fp, 4, GNf::eSEEK_START); //跳到文件开始的第4个字节
    GNf::Write((char *)rc_pos, 4, 18, fp); //写入各种资源存放在文件的偏移位置信息
    GNf::Close(fp); //GN_Log("s7\n");
    return true;
}
//打印出UI组里的UI组件代码类名和名称(组, 空格跳过段, 当前组件ID, 当前历遍到的名)返回当前历遍到的名
static char **GT_PrintUiGroupNodeClassName(GNu::cUI_GROUP *node, char *space_seg, unsigned int &id, char **ppname) {
    unsigned short utype;
    GN::sLIST<P<GNu::iUI_OBJECT>>::iterator it = node->aSubs.begin(), et = node->aSubs.end();
    for(; it != et; ++it) {
        utype = (*it)->aType;
        GN_Log("%s%s    %s    %d\n", space_seg, *ppname, utype >= GNu::eUI_TYPE_NUM ? "?UNKNOWN" : GNu::UiObjCodeClassNames[utype], id);
        ++ppname;
        ++id;
        if(utype == GNg::eOBJ_GROUP || utype == GNu::eUI_SCROLL_RECT)
            ppname = GT_PrintUiGroupNodeClassName(CVP(GNu::cUI_GROUP)(*it), space_seg - 4, id, ppname);
    }
    return ppname;
}
void GNu::cUI_SCENE::PrintAllNodeClassName() {
    unsigned int id = 0;
    char **ppname = zObjNameTable;
    GN_Log("%s    %s    %d\n", *ppname, UiObjCodeClassNames[aType], id);
    ++ppname;
    ++id;
    char space_buf[2049];
    memset(space_buf, ' ', 2048);
    space_buf[2048] = 0;
    GT_PrintUiGroupNodeClassName(this, space_buf + 2044, id, ppname);
}
GN_API P<GNu::iUI_OBJECT> GNu::UiObjectSearch(P<iUI_OBJECT> &ug, unsigned int &id) {
    if(!id) return ug;
    --id;
    P<iUI_OBJECT> ret;
    if(ug->aType == GNg::eOBJ_GROUP || ug->aType == GNu::eUI_SCROLL_RECT) {
        cUI_GROUP *_ug = CVP(cUI_GROUP) ug;
        GN::sLIST<P<iUI_OBJECT>>::iterator it = _ug->aSubs.begin(), et = _ug->aSubs.end();
        for(; !ret && it != et; ++it) ret = UiObjectSearch(*it, id);
    }
    return ret;
}
bool GT_UiObjectIdSearch(GNu::iUI_OBJECT *uobj, GNu::iUI_OBJECT *tg, unsigned int &id) {
    GN::sLIST<P<GNu::iUI_OBJECT>>::iterator it = ((GNu::cUI_GROUP *)uobj)->aSubs.begin(), et = ((GNu::cUI_GROUP *)uobj)->aSubs.end();
    for(; it != et; ++it) {
        ++id;
        uobj = (*it);
        if(tg == uobj) return true;
        if(uobj->aType == GNg::eOBJ_GROUP || uobj->aType == GNu::eUI_SCROLL_RECT) {
            if(GT_UiObjectIdSearch(uobj, tg, id)) return true;
        }
    }
    return false;
}
GN_API unsigned int GNu::UiObjectIdSearch(iUI_OBJECT *ug, iUI_OBJECT *tg) {
    if(tg == ug) return 0;
    unsigned int id = 0;
    if(ug->aType == GNg::eOBJ_GROUP || ug->aType == GNu::eUI_SCROLL_RECT) GT_UiObjectIdSearch(ug, tg, id);
    return id;
}
GN_API unsigned short *GNu::UiObjectsTypeCollect(iUI_OBJECT *uobj, unsigned short *type) {
    *type = uobj->aType;
    ++type;
    if(uobj->aType == GNg::eOBJ_GROUP || uobj->aType == GNu::eUI_SCROLL_RECT) {
        GN::sLIST<P<iUI_OBJECT>>::iterator it = ((cUI_GROUP *)uobj)->aSubs.begin(), et = ((cUI_GROUP *)uobj)->aSubs.end();
        for(; it != et; ++it) type = UiObjectsTypeCollect(*it, type);
    }
    return type;
}
/*****************************************************************************************************************/
uint8 *GNu::cUI_NEAR_OBJECT::zNearAreaDataSave(uint8 *wpos) {
    *((unsigned int *)wpos) = aType;
    wpos += 4;
    memcpy(wpos, &x, 2);
    wpos += 2;
    memcpy(wpos, &y, 2);
    wpos += 2;
    memcpy(wpos, &w, 2);
    wpos += 2;
    memcpy(wpos, &h, 2);
    wpos += 2;
    memcpy(wpos, &c, 4);
    wpos += 4;
    memcpy(wpos, &nmin_x, 2);
    wpos += 2;
    memcpy(wpos, &nmin_y, 2);
    wpos += 2;
    memcpy(wpos, &ntype_x, 1);
    wpos += 1;
    memcpy(wpos, &ntype_y, 1);
    wpos += 1;
    return wpos;
}
uint8 *GNu::cUI_NEAR_OBJECT::zNearAreaDataLoad(uint8 *rpos) {
    aType = *((unsigned int *)rpos);
    rpos += 4;
    memcpy(&x, rpos, 2);
    rpos += 2;
    memcpy(&y, rpos, 2);
    rpos += 2;
    memcpy(&w, rpos, 2);
    rpos += 2;
    memcpy(&h, rpos, 2);
    rpos += 2;
    memcpy(&c, rpos, 4);
    rpos += 4;
    memcpy(&nmin_x, rpos, 2);
    rpos += 2;
    memcpy(&nmin_y, rpos, 2);
    rpos += 2;
    memcpy(&ntype_x, rpos, 1);
    rpos += 1;
    memcpy(&ntype_y, rpos, 1);
    rpos += 1;
    return rpos;
}
unsigned int GNu::cUI_GROUP::DataSave(cSHARE_DATA *share_objs, uint8 *data_out) {
    uint8 loc_buf[64];
    if(!data_out) data_out = loc_buf;
    uint8 *wpos = data_out;
    *wpos = 1;
    ++wpos; //版本
    w = lw;
    h = lh;
    wpos = ((cUI_NEAR_OBJECT *)((GNg::sDRAW_IN_AREA *)this))->zNearAreaDataSave(wpos);
    memcpy(wpos, &aUsScissor, 1);
    wpos += 1;
    return wpos - data_out;
}
unsigned int GNu::cUI_GROUP::DataLoad(cSHARE_DATA *share_objs, const uint8 *data) {
    uint8 *rpos = (uint8 *)data;
    uint8 version = *rpos;
    ++rpos;
    if(version > 1) return 0; //版本
    rpos = ((cUI_NEAR_OBJECT *)((GNg::sDRAW_IN_AREA *)this))->zNearAreaDataLoad(rpos);
    memcpy(&aUsScissor, rpos, 1);
    rpos += 1;
    lw = w;
    lh = h;
    return rpos - data;
}
unsigned int GNu::sUI_IMAGE::DataSave(cSHARE_DATA *share_objs, uint8 *data_out) {
    uint8 loc_buf[64];
    if(!data_out) data_out = loc_buf;
    uint8 *wpos = data_out;
    *wpos = 1;
    ++wpos; //版本
    *((unsigned int *)wpos) = aType;
    wpos += 4;
    memcpy(wpos, &x, 2);
    wpos += 2;
    memcpy(wpos, &y, 2);
    wpos += 2;
    memcpy(wpos, &w, 2);
    wpos += 2;
    memcpy(wpos, &h, 2);
    wpos += 2;
    memcpy(wpos, &c, 4);
    wpos += 4;
    unsigned int pointer_id;
    GT_TMP_OBJECT_WRITE_INDEX((*share_objs->pimage_list), img);
    memcpy(wpos, &bmode, 2);
    wpos += 2;
    return wpos - data_out;
}
unsigned int GNu::sUI_IMAGE::DataLoad(cSHARE_DATA *share_objs, const uint8 *data) {
    uint8 *rpos = (uint8 *)data;
    uint8 version = *rpos;
    ++rpos;
    if(version > 1) return 0; //版本
    aType = *((unsigned int *)rpos);
    rpos += 4;
    memcpy(&x, rpos, 2);
    rpos += 2;
    memcpy(&y, rpos, 2);
    rpos += 2;
    memcpy(&w, rpos, 2);
    rpos += 2;
    memcpy(&h, rpos, 2);
    rpos += 2;
    memcpy(&c, rpos, 4);
    rpos += 4;
    unsigned int pointer_id;
    memcpy(&pointer_id, rpos, 4);
    rpos += 4;
    if(pointer_id != 0xffffffff) img = (*(share_objs->pimage_refs))[pointer_id];
    memcpy(&bmode, rpos, 2);
    rpos += 2;
    return rpos - data;
}
unsigned int GNu::cUI_IMAGE::DataSave(cSHARE_DATA *share_objs, uint8 *data_out) {
    uint8 loc_buf[64];
    if(!data_out) data_out = loc_buf;
    uint8 *wpos = data_out;
    *wpos = 1;
    ++wpos; //版本
    wpos = zNearAreaDataSave(wpos);

    unsigned int pointer_id;
    GT_TMP_OBJECT_WRITE_INDEX((*share_objs->pimage_list), img);
    memcpy(wpos, &bmode, 2);
    wpos += 2;
    return wpos - data_out;
}
unsigned int GNu::cUI_IMAGE::DataLoad(cSHARE_DATA *share_objs, const uint8 *data) {
    uint8 *rpos = (uint8 *)data;
    uint8 version = *rpos;
    ++rpos;
    if(version > 1) return 0; //版本
    rpos = zNearAreaDataLoad(rpos);

    unsigned int pointer_id;
    memcpy(&pointer_id, rpos, 4);
    rpos += 4;
    if(pointer_id != 0xffffffff) img = (*(share_objs->pimage_refs))[pointer_id];
    memcpy(&bmode, rpos, 2);
    rpos += 2;
    return rpos - data;
}
unsigned int GNu::cUI_BACK_FRAME::DataSave(cSHARE_DATA *share_objs, uint8 *data_out) {
    unsigned int gsize = sUI_IMAGE::DataSave(share_objs, data_out);
    uint8 loc_buf[16];
    if(!data_out) data_out = loc_buf;
    else data_out += gsize;
    uint8 *wpos = data_out;
    memcpy(wpos, &aBackRepeat, 1);
    wpos += 1;
    memcpy(wpos, &aBackBlurLv, 1);
    wpos += 1;
    memcpy(wpos, &aZoom, 1);
    wpos += 1;
    memcpy(wpos, &aDragBarH, 1);
    wpos += 1;
    return (wpos - data_out) + gsize;
}
unsigned int GNu::cUI_BACK_FRAME::DataLoad(cSHARE_DATA *share_objs, const uint8 *data) {
    uint8 *rpos = (uint8 *)data;
    if(rpos[0] > 1) return 0;
    rpos += sUI_IMAGE::DataLoad(share_objs, rpos);
    memcpy(&aBackRepeat, rpos, 1);
    rpos += 1;
    memcpy(&aBackBlurLv, rpos, 1);
    rpos += 1;
    memcpy(&aZoom, rpos, 1);
    rpos += 1;
    memcpy(&aDragBarH, rpos, 1);
    rpos += 1;
    return rpos - data;
}
unsigned int GNu::sUI_FRAME_RC::DataSave(cSHARE_DATA *share_objs, uint8 *data_out) {
    uint8 loc_buf[640];
    if(!data_out) data_out = loc_buf;
    uint8 *wpos = data_out;
    *wpos = 1;
    ++wpos; //版本
    *((unsigned int *)wpos) = aType;
    wpos += 4;
    wpos = zDataSave(wpos, &(((GT_cUI_SHARE_DATA_W *)share_objs)->in_frame_objs), 0);
    return wpos - data_out;
}
unsigned int GNu::sUI_FRAME_RC::DataLoad(cSHARE_DATA *share_objs, const uint8 *data) {
    uint8 *rpos = (uint8 *)data;
    uint8 version = *rpos;
    ++rpos;
    if(version > 1) return 0; //版本
    aType = *((unsigned int *)rpos);
    rpos += 4;
    rpos = zDataLoad(rpos, &(((GT_cUI_SHARE_DATA_R *)share_objs)->frame_ref_objs));
    return rpos - data;
}
unsigned int GNu::cUI_FRAME_RC::DataSave(cSHARE_DATA *share_objs, uint8 *data_out) {
    uint8 loc_buf[640];
    if(!data_out) data_out = loc_buf;
    uint8 *wpos = data_out;
    *wpos = 1;
    ++wpos; //版本
    wpos = zNearAreaDataSave(wpos);
    wpos = zDataSave(wpos, &(((GT_cUI_SHARE_DATA_W *)share_objs)->in_frame_objs), 0);
    return wpos - data_out;
}
unsigned int GNu::cUI_FRAME_RC::DataLoad(cSHARE_DATA *share_objs, const uint8 *data) {
    uint8 *rpos = (uint8 *)data;
    uint8 version = *rpos;
    ++rpos;
    if(version > 1) return 0; //版本
    rpos = zNearAreaDataLoad(rpos);
    rpos = zDataLoad(rpos, &(((GT_cUI_SHARE_DATA_R *)share_objs)->frame_ref_objs));
    return rpos - data;
}
unsigned int GNu::cUI_FRAME::DataSave(cSHARE_DATA *share_objs, uint8 *data_out) {
    return ((sUI_FRAME_RC *)this)->sUI_FRAME_RC::DataSave(share_objs, data_out);
}
unsigned int GNu::cUI_FRAME::DataLoad(cSHARE_DATA *share_objs, const uint8 *data) {
    return ((sUI_FRAME_RC *)this)->sUI_FRAME_RC::DataLoad(share_objs, data);
}
#define GT_TMP_UI_FLAUNCHER_DATA_SAVE_DEF(UCLASS)                                                                                    \
    unsigned int GNu::UCLASS::DataSave(cSHARE_DATA *share_objs, uint8 *data_out) {                                                   \
        uint8 loc_buf[640];                                                                                                          \
        if(!data_out) data_out = loc_buf;                                                                                            \
        uint8 *wpos = data_out;                                                                                                      \
        *wpos = 1;                                                                                                                   \
        ++wpos; /*版本*/                                                                                                             \
        *((unsigned int *)wpos) = aType;                                                                                             \
        wpos += 4;                                                                                                                   \
        wpos = zDataSave(wpos, ((GT_cUI_SHARE_DATA_W *)share_objs)->list_frame, ((GT_cUI_SHARE_DATA_W *)share_objs)->list_keep_obj); \
        return wpos - data_out;                                                                                                      \
    }                                                                                                                                \
    unsigned int GNu::UCLASS::DataLoad(cSHARE_DATA *share_objs, const uint8 *data) {                                                 \
        uint8 *rpos = (uint8 *)data;                                                                                                 \
        uint8 version = *rpos;                                                                                                       \
        ++rpos;                                                                                                                      \
        if(version > 1) return 0; /*版本*/                                                                                           \
        aType = *((unsigned int *)rpos);                                                                                             \
        rpos += 4;                                                                                                                   \
        rpos = zDataLoad(rpos, ((GT_cUI_SHARE_DATA_R *)share_objs)->pframes, ((GT_cUI_SHARE_DATA_R *)share_objs)->pkeep_objs);       \
        return rpos - data;                                                                                                          \
    }
GT_TMP_UI_FLAUNCHER_DATA_SAVE_DEF(cUI_FRAME_LAUNCHER)
GT_TMP_UI_FLAUNCHER_DATA_SAVE_DEF(cUI_FRAME_LAUNCHER2D)

unsigned int GNu::cUI_FRAME_LAUNCHER_FONT::DataSave(cSHARE_DATA *share_objs, uint8 *data_out) {
    uint8 loc_buf[640];
    if(!data_out) data_out = loc_buf;
    uint8 *wpos = data_out;
    *wpos = 1;
    ++wpos; //版本
    wpos = zNearAreaDataSave(wpos);
    wpos = zDataSave(wpos, ((GT_cUI_SHARE_DATA_W *)share_objs)->list_frame, ((GT_cUI_SHARE_DATA_W *)share_objs)->list_keep_obj);
    return wpos - data_out;
}
unsigned int GNu::cUI_FRAME_LAUNCHER_FONT::DataLoad(cSHARE_DATA *share_objs, const uint8 *data) {
    uint8 *rpos = (uint8 *)data;
    uint8 version = *rpos;
    ++rpos;
    if(version > 1) return 0; //版本
    rpos = zNearAreaDataLoad(rpos);
    rpos = zDataLoad(rpos, ((GT_cUI_SHARE_DATA_R *)share_objs)->pframes, ((GT_cUI_SHARE_DATA_R *)share_objs)->pkeep_objs);
    return rpos - data;
}
unsigned int GNu::cUI_TEXT_ART::DataSave(cSHARE_DATA *share_objs, uint8 *data_out) {
    uint8 loc_buf[65536 + 64];
    if(!data_out) data_out = loc_buf;
    uint8 *wpos = data_out;
    *wpos = 1;
    ++wpos; //版本
    wpos = zNearAreaDataSave(wpos);

    unsigned int pointer_id;
    GT_TMP_TEXTSTYLE_WRITE_INDEX(share_objs->text_style_list, textstyle)
    memcpy(wpos, &blendmode, 2);
    wpos += 2;
    memcpy(wpos, &offsetx, 2);
    wpos += 2;
    memcpy(wpos, &offsety, 2);
    wpos += 2;
    memcpy(wpos, &fw, 2);
    wpos += 2;
    memcpy(wpos, &fh, 2);
    wpos += 2;
    unsigned int tsize = 0;
    if(text) {
        tsize = strlen(text) + 1;
        memcpy(wpos, &tsize, 4);
        wpos += 4;
        memcpy(wpos, text, tsize);
        wpos += tsize;
    } else
        memcpy(wpos, &tsize, 4);
    wpos += 4;
    return wpos - data_out;
}
unsigned int GNu::cUI_TEXT_ART::DataLoad(cSHARE_DATA *share_objs, const uint8 *data) {
    uint8 *rpos = (uint8 *)data;
    uint8 version = *rpos;
    ++rpos;
    if(version > 1) return 0; //版本
    rpos = zNearAreaDataLoad(rpos);

    unsigned int pointer_id;
    GT_TMP_TEXTSTYLE_READ_INDEX(share_objs->text_style_refs, textstyle)
    if(!textstyle) textstyle = &GN::RunRec.aFont; // 字体缺失时用默认
    memcpy(&blendmode, rpos, 2);
    rpos += 2;
    memcpy(&offsetx, rpos, 2);
    rpos += 2;
    memcpy(&offsety, rpos, 2);
    rpos += 2;
    memcpy(&fw, rpos, 2);
    rpos += 2;
    memcpy(&fh, rpos, 2);
    rpos += 2;
    unsigned int tsize = *((unsigned int *)rpos);
    rpos += 4;
    GN_DeleteArray(text);
    if(tsize) {
        text = new char[tsize];
        memcpy(text, rpos, tsize);
        rpos += tsize;
    }
    return rpos - data;
}
#endif //_GN_UI_H_
