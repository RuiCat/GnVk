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
#include "GN_UiPopDlgInner.h" //DA-M2 批 4：取 sPopDlgContext/GN_UiPopDlgContext() 声明
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
    if(aBackBlurLv) GT_DrawBlurScreenImg(dx, dy, dw, dh, fc, (aBackBlurLv << 10) | (((uint8)aBackRepeat | 2) << 8) | (_img ? (unsigned int)GNg::eBLEND_DST_ALPHA_MASK : bmode), _img);
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
    // 已核（B5f）：new char[strlen(cnt)+1]，精确容量。
    memcpy(_ui_text->text, cnt, strlen(cnt) + 1);
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
                if(GNg::sIMAGE::RenderTargetSupport()) { //要调用：只写函数名测的是函数地址，恒真
                    GNg::cVIEW2D::screenImg.New(2);
                    GNg::cVIEW2D::screenImg->CreateEmptyToData(128, 128);
                } else GNg::cVIEW2D::screenImg.New(1);
            }
            if(GNg::sIMAGE::RenderTargetSupport()) { //要调用：只写函数名测的是函数地址，恒真
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
    if(GN_UiPopDlgContext().aFullSelectPathDlg) {
        GN_UiPopDlgContext().aFullSelectPathDlg->UpdateRun(updatetime);
        if(GN_UiPopDlgContext().aFullSelectPathDlg->GetCurDetailSetState() != eDETAIL_SET_PATH_SELECTF) GN_UiPopDlgContext().aFullSelectPathDlg = 0;
        return;
    }
    //Resize(GNg::ViewCur); //主UI场景范围与视口截取范围相同

    GN::sLIST<P<iUI_OBJECT>>::iterator itu = aSubs.begin(), etu = aSubs.end();
    for(; itu != etu; ++itu) (*itu)->StatisticSize(this);
    w = lw;
    h = lh;

    sUI_LEVEL uilv0;
    memset((void *)&uilv0, 0, sizeof(uilv0)); //全 POD 布局（坐标/颜色字段），置零是契约
    uilv0.c = 0xffffffff;
    UpdateRun(updatetime, &uilv0);
}
#endif //_GN_UI_H_
