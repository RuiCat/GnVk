// GN_mini UI 实现片段：cBUTTON_TOUCH / cJOYSTICK 触摸与摇杆。
// 2026-11-26 自 GN_UiBase.cpp 第 1995-2148 行 move-only 拆出（REFACTOR_PLAN §13 B4）。
// 由 GN_UiBase.cpp 按原顺序包含（单编译单元，static/宏语义不变）；不是独立头文件。
/****************************************************************************************************************************/
P<GNu::iUI_OBJECT> GNu::cBUTTON_TOUCH::Clone() {
    P<cBUTTON_TOUCH> ui_bt(1);
    *ui_bt = *this;

    return ui_bt;
}
void GNu::cBUTTON_TOUCH::UpdateRun(unsigned int updatetime, sUI_LEVEL *parent) {
    GNg::sDRAW_AREA2 rng;
    cUI_IMAGE::DrawRangeGet(rng, parent);
    short aRngL = rng.x, aRngT = rng.y, aRngR, aRngB;
    if(rng.w < 0) {
        aRngR = rng.x;
        aRngL = rng.x + rng.w;
    } else aRngR = rng.x + rng.w;
    if(rng.h < 0) {
        aRngB = rng.y;
        aRngT = rng.y + rng.h;
    } else aRngB = rng.y + rng.h;

#define GT_TMP_BUTTON_T_UPDATE_RUN(CMP)                                                                                                                \
    if(zTid == 255) {                                                                                                                                  \
        CMP zTid = GNg::ViewCur->GetRangeTouchDownId(aRngL, aRngT, aRngR, aRngB);                                                                      \
        if(zTid != 255) {                                                                                                                              \
            if(aMappingKey) GF_SetKeyState(aMappingKey, 1);                                                                                            \
            aTouchZ = 255;                                                                                                                             \
        } else aTouchZ = 0;                                                                                                                            \
    } else {                                                                                                                                           \
        aTouchZ = GN::TouchZ[zTid];                                                                                                                    \
        GNg::cVIEW2D *_view = GNg::ViewCur;                                                                                                            \
        if(aTouchZ <= 1 || _view->touchX[zTid] < aRngL || _view->touchX[zTid] > aRngR || _view->touchY[zTid] < aRngT || _view->touchY[zTid] > aRngB) { \
            if(aMappingKey) GF_SetKeyState(aMappingKey, 0);                                                                                            \
            zTid = 255;                                                                                                                                \
        }                                                                                                                                              \
    }
    GT_TMP_BUTTON_T_UPDATE_RUN(if(RecvInputUiObject == this))
    GNg::ImageDraw(img, rng.x, rng.y, rng.w, rng.h, rng.c);
}
void GNu::cBUTTON_T::UpdateRun(unsigned int updatetime) {
    GT_TMP_BUTTON_T_UPDATE_RUN()
    GNg::ImageDraw(aIcon, aRngL, aRngT, aRngR - aRngL, aRngB - aRngT, aColor, 0, aBlend);
}
void GNu::cJOYSTICK::zReCptNearRange() {
    GNg::cVIEW2D *_view = GNg::ViewCur;
    if(zVpUV.x < 0.5f) {
        zRngR = GNu::NearUx(zVpUV.x);
        zRngL = _view->ortho_left;
    } else {
        zRngL = GNu::NearUx(zVpUV.x);
        zRngR = _view->ortho_right;
    }
    if(zVpUV.y < 0.5f) {
        zRngB = GNu::NearVy(zVpUV.y);
        zRngT = _view->ortho_top;
    } else {
        zRngT = GNu::NearVy(zVpUV.y);
        zRngB = _view->ortho_bottom;
    }
}
void GNu::cJOYSTICK::zInit(float back_disk_r) {
    if(back_disk_r) {
        if(back_disk_r > 0.0f) zVectorScl = 1.0f / back_disk_r;
    } else {
        short w = zRngR - zRngL, h = zRngB - zRngT;
        zVectorScl = 1.0f / (float)((w < h ? w : h) >> 2);
    }
    zTid = 255;
    aVector = GNg::Vector2D;
    aPressure = 0.0f;
}
P<GNu::iUI_OBJECT> GNu::cJOYSTICK::Clone() {
    P<cJOYSTICK> ui_joy(1);
    *ui_joy = *this;
    return ui_joy;
}
void GNu::cJOYSTICK::UpdateRun(unsigned int updatetime, unsigned int color) {
    GNg::cVIEW2D *_view = GNg::ViewCur;
    if(zVpUV.x > 0.0f && GN::ViewReSized) zReCptNearRange();
    //GNg::ImageDraw(0, zRngL, zRngT, zRngR - zRngL, zRngB - zRngT, 0x6600ff00);
    if(zTid == 255) {
        zTid = _view->GetRangeTouchDownId(zRngL, zRngT, zRngR, zRngB);
        if(zTid != 255) {
            zCpX = _view->touchX[zTid];
            zCpY = _view->touchY[zTid];
        }
    } else {
        if(GN::TouchZ[zTid] <= 1) {
            zTid = 255;
            aVector = GNg::Vector2D;
            aPressure = 0.0f;
        } else {
            aVector.x = (float)(_view->touchX[zTid] - zCpX) * zVectorScl;
            aVector.y = (float)(_view->touchY[zTid] - zCpY) * zVectorScl;
            aPressure = (float)GN::TouchZ[zTid] / 254.0f;

            float vlen = GNg::Vec2Length(&aVector);
            if(vlen > 1.0f) aVector /= vlen;
            if(vlen > aVectorMaxLen) {
                GNg::sVEC2 cpofs = aVector * ((vlen - aVectorMaxLen) / (vlen * zVectorScl));
                zCpX += (short)cpofs.x;
                zCpY += (short)cpofs.y;
            }
            unsigned int bc = GNg_MultiplyColorRatio(aBackDiskColor, color);
            if(bc & 0xff000000) {
                GNg::sIMAGE *back_img = aBackDiskImg ? aBackDiskImg : GN::RunRec.aGV_Point32;
                GNg::ImageDraw(back_img, zCpX - (back_img->width >> 1), zCpY - (back_img->height >> 1), 0, 0, bc);
            }
            if(aArrowImg) {
                GNg::sIMAGE *arrow_img = aArrowImg;
                GNg::sVEC2 ax, ay = (vlen < 1.0f ? aVector / vlen : aVector) * (float)arrow_img->width;
                GNg::Vec2Right(&ax, &ay);
                GNg::sVEC2 cp1 = (aVector / zVectorScl) + GNg::sVEC2((float)zCpX, (float)zCpY);
                GNg::sVEC2 vts[4];
                vts[0] = cp1 + ((ay - ax) * 0.5f);
                vts[1] = vts[0] + ax;
                vts[2] = vts[1] - ay;
                vts[3] = vts[2] - ax;
                GNg::ImageDraw4P(arrow_img, (float *)vts, GNg_MultiplyColorRatio(aArrowColor, color), 0, 1, 0.0f, 0.0f, arrow_img->uRange, arrow_img->vRange);
            }
        }
        if(aVecMappingKeyType) {
            short kl, kt, kr, kb;
            if(aVecMappingKeyType == 1) {
                kl = GK_A, kt = GK_W, kr = GK_D, kb = GK_S;
            } else {
                kl = GK_LEFT, kt = GK_UP, kr = GK_RIGHT, kb = GK_DOWN;
            }
            bool pres = aVector.x < -aVectorMappingRef;
            if(pres != GN::Key[kl]) GF_SetKeyState(kl, pres);
            pres = aVector.x > aVectorMappingRef;
            if(pres != GN::Key[kr]) GF_SetKeyState(kr, pres);
            pres = aVector.y < -aVectorMappingRef;
            if(pres != GN::Key[kt]) GF_SetKeyState(kt, pres);
            pres = aVector.y > aVectorMappingRef;
            if(pres != GN::Key[kb]) GF_SetKeyState(kb, pres);

            if(aPressMappingKey) {
                pres = aPressure > aPressMappingRef;
                if(pres != GN::Key[aPressMappingKey]) GF_SetKeyState(aPressMappingKey, pres);
            }
        }
    }
}
void GNu::cJOYSTICK::UpdateRun(unsigned int updatetime, sUI_LEVEL *parent) {
    UpdateRun(updatetime, parent->c);
    if(RecvInputUiObject != this) zTid = 255;
}
void GNu::cJOYSTICK::GetDrawArea(GNg::sDRAW_AREA2 &out, cUI_GROUP *parent) {
    out.x = zRngL;
    out.y = zRngT;
    out.w = zRngR - zRngL;
    out.h = zRngB - zRngT;
    out.c = parent ? parent->gc : 0xffffffff;
}
