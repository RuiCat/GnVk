// GN_UiPopDlg.cpp — 顶层 Entry（GT_sSP_CHILD::Release、绘制区与输入框绘制钩子）

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
void GT_sSP_CHILD::Release() {
    icon.Delete();
    GN_DeleteArray(name);
    GN_DeleteArray(enumName);
    GN_DeleteArray(enumIds);
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
#endif //_GN_UIPOPDLG_H_
