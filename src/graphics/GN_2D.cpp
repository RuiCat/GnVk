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
#ifdef _GN_2D_H_
/***************************************************** 2D 视口类 ****************************************************************************/
GN::sLIST<GNg::cVIEW2D *> GNg::cVIEW2D::zTotalList;
P<GNg::sIMAGE> GNg::cVIEW2D::screenImg;
unsigned int GNg::cVIEW2D::screenImgUpdateFps;

void GNg::cVIEW2D::BeginRender2D() {
    ViewCur = this; // Vulkan: frame already started by GF_FrameCycleProc2, just switch active view
    return;
    if(sIMAGE::RenderTargetGet() && this != ViewCur) {
        GN_Log("渲染到图象时...不能再设置渲染子视图!!\n" /*"render to imageing... can not set sub view render!!\n"*/);
        return;
    }
    ViewCur = this;

    if(zHasOrthoOfsTg) { //如果正交投影有偏移目标
        //计算插值
        GN_ValueChange(short, ortho_left, zOrthoLeftTg, 0.2f, )
            GN_ValueChange(short, ortho_top, zOrthoTopTg, 0.2f, )
                ortho_right = ortho_left + orthoW;
        ortho_bottom = ortho_top + orthoH;
        orthoOffsetX = zOrthoLeftSrc - ortho_left;
        orthoOffsetY = zOrthoTopSrc - ortho_top;

        if(zOrthoLeftTg == ortho_left && zOrthoTopTg == ortho_top) zHasOrthoOfsTg = false; //到补间结束的一刻
    }
    GT_ReSetRenderView2D(this, true);
}
void GNg::cVIEW2D::EndRender2Dand3D() {
    GNg::GN_VkEndFrame();
    return; // Vulkan: end frame (skip old GL code)
#ifdef GT_TMP_COMPATI_DX_DDRAW /*模版代码:重架设2D渲染状态*/
    if(!strcmp(GN_GetInterfaceInfoRender(), "DirectDraw")) {
        ViewCur->BeginRender2D();
    } else
#endif
        ViewCur->BeginRender2D();
}
void GNg::cVIEW2D::zScissorRectSet(int x, int y, int w, int h) {
    // Vulkan: delegate to Vulkan backend scissor
    GNg::GN_VkSetScissor(x, y, w, h, true);
}
#define GN_SCISSOR_LV_NUM 32 //剪裁范围嵌套最大层数
GNg::sRECT GT_ScissorRect[GN_SCISSOR_LV_NUM];
uint8 GT_ScissorRectLv = 0;

void GNg::cVIEW2D::ScissorRectSetup(int x, int y, int w, int h, bool is_include) {
#ifdef GN_DEBUG
    if(GT_ScissorRectLv >= GN_SCISSOR_LV_NUM) {
        GN_Log("剪裁范围嵌套超出最大限制%d个!\n", GN_SCISSOR_LV_NUM);
    }
#endif
    GNg::sRECT *p_rect = GT_ScissorRect + GT_ScissorRectLv;
    p_rect->left = x;
    p_rect->right = x + w;
    p_rect->top = y;
    p_rect->bottom = y + h;

    if(is_include && GT_ScissorRectLv) {
        GNg::RectOverlap(p_rect, p_rect, (p_rect - 1));
        x = p_rect->left, y = p_rect->top;
        w = p_rect->right - x, h = p_rect->bottom - y;
    }
    GT_ScissorEnable(1);
    zScissorRectSet(x, y, w, h);
    ++GT_ScissorRectLv;
}
void GNg::cVIEW2D::ScissorRectRemove() {
    switch(GT_ScissorRectLv) {
        case 0:
#ifdef GN_DEBUG
            GN_Log("调用 cVIEW2D::ScissorRectRemove 失败! 剪裁层已经是根层了!!\n");
#endif
            break;
        case 1:
            --GT_ScissorRectLv;
            GNg::GN_VkSetScissor(0, 0, 0, 0, false);
            break;
        default: {
            --GT_ScissorRectLv;
            GNg::sRECT *p_rect = GT_ScissorRect + (GT_ScissorRectLv - 1);
            zScissorRectSet(p_rect->left, p_rect->top, p_rect->right - p_rect->left, p_rect->bottom - p_rect->top);
        }
    }
}
uint8 GNg::cVIEW2D::ScissorRectGet(GNg::sRECT *rc) {
    if(rc && GT_ScissorRectLv) *rc = GT_ScissorRect[GT_ScissorRectLv - 1];
    return GT_ScissorRectLv;
}
uint8 GNg::cVIEW2D::GetRangeTouchDownId(short left, short top, short right, short bottom) {
#ifdef _GN_MAIN_H_
    uint8 ti = 0, txn = GN::GetTouchMaxNum();
    for(; ti < txn; ++ti)
#else
    const uint8 ti = 0;
#endif
    {
        if(GN::TouchZ[ti] == 0xff && touchX[ti] > left && touchX[ti] < right && touchY[ti] > top && touchY[ti] < bottom) return ti;
    }
    return 0xff;
}
uint8 GNg::cVIEW2D::GetRangeTouchDownId(const GNg::sRECT *rc) {
#ifdef _GN_MAIN_H_
    uint8 ti = 0, txn = GN::GetTouchMaxNum();
    for(; ti < txn; ++ti)
#else
    const uint8 ti = 0;
#endif
    {
        if(GN::TouchZ[ti] == 0xff && touchX[ti] > (short)rc->left && touchX[ti] < (short)rc->right && touchY[ti] > (short)rc->top && touchY[ti] < (short)rc->bottom) return ti;
    }
    return 0xff;
}
void GNg::cVIEW2D::SaveToFile(const char *filename, short vx, short vy, short vw, short vh) {
#ifdef GN_DEBUG
    if(!filename) { GN_Log("调用 cVIEW2D::SaveToFile 没有指定好保存文件名!\n"); }
#endif
#define GT_TMP_VIEW2D_SAVE_RANGE_ENSURE                    \
    vx += viewX;                                           \
    vy += viewY;                                           \
    if(vw <= 0) vw = viewW;                                \
    if(vh <= 0) vh = viewH;                                \
    if(vx < ViewCur->viewX) vx = ViewCur->viewX;           \
    if(vy < ViewCur->viewY) vy = ViewCur->viewY;           \
    if(vx + vw > ViewCur->viewW) vw = ViewCur->viewW - vx; \
    if(vy + vh > ViewCur->viewH) vh = ViewCur->viewH - vy

    char fn[384];
    strcpy(fn, filename);
    char *extname = GN_StrGetExtName(fn);
    if(!extname) {
        GN_Log("cVIEW2D::SaveToFile 文件名格式有错!!\n");
        return;
    }
    strcpy(extname, "png");
    GT_TMP_VIEW2D_SAVE_RANGE_ENSURE;

    bool col_turn;
    GNg::COLOR_FORMAT cfmt;
#ifdef GT_TMP_COMPATI_DX_DDRAW
    char *render_info = GN_GetInterfaceInfoRender();
    if(strncmp(render_info, "Direct", 6)) { //OpenGL 处理
#endif
        col_turn = true;
        cfmt = GNg::CR_RGBA;
#ifdef GT_TMP_COMPATI_DX_DDRAW
    } else {
        if(render_info[6] == '3') { //Direct3D 处理
            col_turn = false;
            cfmt = GNg::CR_RGBA;
        } else { //DirectDraw 处理
            col_turn = false;
            cfmt = ImgFmtGetFormat(FBufferColorBit);

#define GT_TMP_VIEW2D_SAVE_RANGE_ENSURE_DDRAW                                \
    vx += (short)((float)ViewCur->ortho_left / GNg::ViewCur->zScreenRatioW); \
    vy += (short)((float)ViewCur->ortho_top / GNg::ViewCur->zScreenRatioH);  \
    if(vx + vw > ViewCur->zSrcScreenW) vw = ViewCur->zSrcScreenW - vx;       \
    if(vy + vh > ViewCur->zSrcScreenH) vh = ViewCur->zSrcScreenH - vy

            GT_TMP_VIEW2D_SAVE_RANGE_ENSURE_DDRAW;
        }
    }
#endif
    int size_out;
    uint8 *data = GNg::GN_VkReadBack(vx, vy, vw, vh, &size_out);
    if(!data) return;
    if(!SavePngFile(fn, data, cfmt, vw, vh, col_turn))
        GN_Log("调用 cVIEW2D::SaveToFile %s 有错!\n", fn);

    GNg::GN_VkReadBackFree(data);
}
uint8 *GNg::cVIEW2D::SaveToData(int *size_out, short vx, short vy, short vw, short vh, bool col_turn) {
    GT_TMP_VIEW2D_SAVE_RANGE_ENSURE;
#ifdef GT_TMP_COMPATI_DX_DDRAW
    if(!strcmp(GN_GetInterfaceInfoRender(), "DirectDraw")) {
        GT_TMP_VIEW2D_SAVE_RANGE_ENSURE_DDRAW;
    }
#endif
    return GNg::GN_VkReadBack(vx, vy, vw, vh, size_out);
}
void GNg::cVIEW2D::SaveToImageData(GNg::sIMAGE *img_out, short vx, short vy, short vw, short vh, bool us_mipmap, bool col_turn) {
#ifdef GN_DEBUG
    if(!img_out) {
        GN_Log("调用 cVIEW2D::SaveToImageData 还没有指定好图象!!\n" /*"cVIEW2D::SaveToImageData do not image!!\n"*/);
    }
#endif
    GT_TMP_VIEW2D_SAVE_RANGE_ENSURE;
    GNg::COLOR_FORMAT cfmt;
    bool fmt_neq;
#ifdef GT_TMP_COMPATI_DX_DDRAW
    char *render_info = GN_GetInterfaceInfoRender();
    if(strncmp(render_info, "Direct", 6)) { //OpenGL 处理
#endif
        cfmt = FBufferColorBit >= 24 ? ImgFmtGetFormat(FBufferColorBit) : GNg::CR_RGBA;
        fmt_neq = img_out->cFormat != cfmt;
#ifdef GT_TMP_COMPATI_DX_DDRAW
    } else {
        if(render_info[6] == '3') { //Direct3D 处理
            cfmt = ImgFmtGetFormat(FBufferColorBit);
            fmt_neq = img_out->cFormat != cfmt;
        } else { //DirectDraw 处理
            cfmt = GNg::CR_RGB;
            fmt_neq = false;
            GT_TMP_VIEW2D_SAVE_RANGE_ENSURE_DDRAW;
        }
    }
#endif
    //模板代码: 这段和 GNg::cVIEW2D::SaveToImageData 的是差不多的
    if(fmt_neq || !img_out->texture || (short)img_out->width != vw || (short)img_out->height != vh)
        img_out->CreateEmptyToData(vw, vh, 1, cfmt, 1, us_mipmap);

    GT_CopyToImageFromFrameBuffer(img_out, vx, vy, vw, vh, col_turn);
}
GNg::cVIEW2D::~cVIEW2D() {
    GN_DeleteArray(touchX);
    GN_DeleteArray(touchY);
    GN_DeleteArray(touchSX);
    GN_DeleteArray(touchSY);
    zTotalList.remove(this);
}
GNg::cVIEW2D::cVIEW2D() : touchX(0), touchY(0), touchSX(0), touchSY(0),
                          zHasOrthoOfsIng(false), zHasOrthoOfsTg(false),
                          orthoOffsetX(0), orthoOffsetY(0),
                          softKeyboardW(0), softKeyboardH(0) {
    const short scr_w = 800, scr_h = 600;
    zBackScreenW = scr_w;
    zBackScreenH = scr_h;

    ortho_left = 0;
    ortho_top = 0;
    ortho_right = scr_w;
    ortho_bottom = scr_h;
    orthoW = scr_w;
    orthoH = scr_h;

    viewX = 0;
    viewY = 0;
    viewW = zSrcScreenW;
    viewH = zSrcScreenH;

    zSrcScreenW = scr_w;
    zSrcScreenH = scr_h;
    zScreenRatioW = (float)scr_w / (float)zSrcScreenW;
    zScreenRatioH = (float)scr_h / (float)zSrcScreenH;
    zCameraFar = 1000.0f;

    zTotalList.push_back(this);
}
void GNg::cVIEW2D::Init(short view_x, short view_y, short view_w, short view_h, short back_src_left, short back_src_top, short back_src_right, short back_src_bottom) {
    if(!touchX) {
#ifdef _GN_MAIN_H_
        unsigned char tn = GN::GetTouchMaxNum();
#else
        unsigned char tn = 1;
#endif
        touchX = new short[tn];
        touchY = new short[tn];
        touchSX = new short[tn];
        touchSY = new short[tn];
    }
    viewX = view_x;
    viewY = view_y;
    viewW = view_w;
    viewH = view_h;

    ortho_left = back_src_left;
    ortho_top = back_src_top;
    ortho_right = back_src_right ? back_src_right : (ortho_left + view_w);
    ortho_bottom = back_src_bottom ? back_src_bottom : (ortho_top + view_h);
    orthoW = ortho_right - ortho_left;
    orthoH = ortho_bottom - ortho_top;

    zBackScreenW = orthoW;
    zBackScreenH = orthoH;
    zSrcScreenW = view_w;
    zSrcScreenH = view_h;
    zScreenRatioW = (float)zBackScreenW / (float)zSrcScreenW;
    zScreenRatioH = (float)zBackScreenH / (float)zSrcScreenH;
}
void GNg::cVIEW2D::ReSize(short new_x, short new_y, short new_w, short new_h, bool auto_scale2D, bool auto_scale3D) {
    viewX = new_x;
    viewY = new_y;
    viewW = new_w;
    viewH = new_h;

    if(auto_scale2D && !zHasOrthoOfsIng) {
        ortho_left = ((viewW - zSrcScreenW) / -2) * zScreenRatioW;
        ortho_right = ((viewW - zSrcScreenW) / 2 + zSrcScreenW) * zScreenRatioW;
        ortho_top = ((viewH - zSrcScreenH) / -2) * zScreenRatioH;
        ortho_bottom = ((viewH - zSrcScreenH) / 2 + zSrcScreenH) * zScreenRatioH;
        orthoW = ortho_right - ortho_left;
        orthoH = ortho_bottom - ortho_top;
    }
}
GN_API GNg::cVIEW2D GNg::View;
GN_API GNg::cVIEW2D *GNg::ViewCur = &GNg::View;

GN_API GNg::sIMAGE GNg::Image;
GN_API GNg::sIMAGE GNg::BackScreen128;
bool GNg::RenderRotSupport = true;

uint8 *GNg::sIMAGE_OBJ::zDataSave(uint8 *wpos, GN::sLIST<P<sIMAGE>> &imgs_collect) {
    memcpy(wpos, (sDRAW_AREA2 *)this, sizeof(sDRAW_AREA2));
    wpos += sizeof(sDRAW_AREA2);
    memcpy(wpos, &bmode, 2);
    wpos += 2;

    unsigned int pointer_id;
    if(img) {
        pointer_id = imgs_collect.add_no_eq2(img);
    } else pointer_id = 0xffffffff; /*该值代表没引用指针*/
    memcpy(wpos, &pointer_id, 4);
    wpos += 4;
    return wpos;
}
uint8 *GNg::sIMAGE_OBJ::zDataLoad(uint8 *rpos, P<P<sIMAGE>> &imgs_ref) {
    memcpy((sDRAW_AREA2 *)this, rpos, sizeof(sDRAW_AREA2));
    rpos += sizeof(sDRAW_AREA2);
    memcpy(&bmode, rpos, 2);
    rpos += 2;

    unsigned int pointer_id;
    memcpy(&pointer_id, rpos, 4);
    rpos += 4;
    if(pointer_id != 0xffffffff) img = imgs_ref[pointer_id];
    return rpos;
}
GN_API GNg::eIMG_FILE_TYPE GNg::CheckHasImgFmtTypeFromImgPath(const sIMAGE *img, void *ph) {
    char filename[384];
    strcpy(filename, img->fileName);
    char *pstr = filename + strlen(filename), *pstr1;
    if(img->add_0000) {
        strcpy(pstr, "0000");
        pstr1 = pstr + 4;
    } else pstr1 = pstr;
    *pstr1 = '.';
    ++pstr1;

    uint8 i = 0, n = eIMG_FILE_TYPE_NUM;
    for(; i < n; ++i) {
        strcpy(pstr1, ImgFileTypeNames[i]);
        if(!GNf::Access(pstr1, 0, ph)) return (eIMG_FILE_TYPE)i;
    }
    return eIMG_FILE_UNKNOWN;
}
GN_API P<GNg::sIMAGE> GNg::CopyOutImageAlpha(sIMAGE *img, unsigned char linearlv) {
    P<GNg::sIMAGE> dw_img(1);
    GNg::sIMAGE *_dw_img = dw_img;
    _dw_img->CreateEmptyToData(img->width, img->height, 1, GNg::CR_ALPHA, linearlv, img->hasMipmap);
    GNg::cVIEW2D *view = GNg::ViewCur;
    _dw_img->BeginRender();
    ImageDraw(img, view->ortho_left, view->ortho_top, view->orthoW, view->orthoH, 0xffffffff, 0, 0);
    _dw_img->EndRender();
    return dw_img;
}
GN_API P<char> GNg::ExportImageFile(const char *exp_path, GNg::sIMAGE *img) {
    P<char> save_path;
    if(img->fileName) {
        char img_path[384];
        unsigned int fnlen = strlen(img->fileName);
        strncpy(img_path, img->fileName, fnlen);
        char *p_ext_name = img_path + fnlen;
        if(img->add_0000) {
            strcpy(p_ext_name, "0000");
            p_ext_name += 4;
        }
        char *media_data = 0;
        int media_size;
        unsigned int eni = 0, enn; /*模版代码:尝试用所有被支持的图片文件扩展名来加载指定图片数据*/
        char **ext_names = GN_StrSplitToData(enn, GetSupportImgFmtFileExtName(), ',');
        for(; eni < enn; ++eni) {
            strcpy(p_ext_name, ext_names[eni]);
            media_data = GNf::FileFullLoadToMemory(img_path, &media_size);
            if(media_data) break; //检测到已存在文件
        }
        GN_DeletePPArray(ext_names, enn) if(media_data) {
            char *filename = GN_StrGetFileName(img_path);
            save_path.New(strlen(exp_path) + strlen(filename) + 2);
            sprintf(save_path, "%s/%s", exp_path, filename);
            GNf::sFILE *fp = GNf::Open(save_path, "wb");
            if(fp) {
                GNf::Write(media_data, 1, media_size, fp);
                GNf::Close(fp);
            } else {
                GN_Log("导出图象文件 %s 失败!\n", (char *)save_path);
                save_path.Delete();
            }
            delete[] media_data;
        }
    }
    return save_path;
}
GN_API bool GNg::SaveImageToFile(const char *filename, sIMAGE *img, short x, short y, short w, short h) {
#ifdef GN_DEBUG
    if(!filename) {
        GN_Log("调用 SaveImageToFile 没有指定好保存文件名!\n" /*"SaveImageToFile do not filename!!\n"*/);
    }
#endif
    char fn[384];
    strcpy(fn, filename);
    char *_name = GN_StrGetExtName(fn);
    if(_name) --_name;
    else _name = (char *)fn + strlen(fn);

    if(w <= 0) w = img->width;
    if(h <= 0) h = img->height;

    uint8 *data;
    int data_size;
    unsigned short i = 0;
    for(; i < img->texnum; ++i) {
        data = img->DataGet(&data_size, x, y, w, h, i, true);
        if(!data) return false;
        if(img->texnum > 1) sprintf(_name, "%04d.png", i);
        else strcpy(_name, ".png");
        if(!SavePngFile(fn, data, img->cFormat, w, h, true))
            GN_Log("调用 sIMAGE::SaveToFile %s 有错!\n", fn);
        delete[] data;
    }
    return true;
}
/************************************************* 绘图补充函数 ********************************************************************************************************************/
GN_API void GNg::ImageDrawRng(sIMAGE *img, int x, int y, int w, int h, unsigned int color, unsigned short tex_id, unsigned short blendmode, float scl, GNu::eNEAR_TYPE ntype) {
    if(!w) w = img->width;
    if(!h) h = img->height;
    switch(ntype) {
        case GNu::eNEAR_NULL: {
            float sinv = 1.0f - scl, scl2 = scl * 0.5f;
            float scl_2 = 0.5f + scl2;
            scl2 = 0.5f - scl2;
            GNg::ImageDraw(img, x + (int)((float)(w >> 1) * sinv), y + (int)((float)(h >> 1) * sinv), w * scl, h * scl, color, tex_id, blendmode, scl2 * img->uRange, scl2 * img->vRange, scl_2 * img->uRange, scl_2 * img->vRange);
        } break;
        case GNu::eNEAR_LEFT:
            GNg::ImageDraw(img, x, y, w * scl, h, color, tex_id, blendmode, 0.0f, 0.0f, scl * img->uRange, img->vRange);
            break;
        case GNu::eNEAR_RIGHT:
            GNg::ImageDraw(img, x + (int)((float)w * (1.0f - scl)), y, w * scl, h, color, tex_id, blendmode, (1.0f - scl) * img->uRange, 0.0f, img->uRange, img->vRange);
            break;
        case GNu::eNEAR_TOP:
            GNg::ImageDraw(img, x, y, w, h * scl, color, tex_id, blendmode, 0.0f, 0.0f, img->uRange, scl * img->vRange);
            break;
        case GNu::eNEAR_BOTTOM:
            GNg::ImageDraw(img, x, y + (int)((float)h * (1.0f - scl)), w, h * scl, color, tex_id, blendmode, 0.0f, (1.0f - scl) * img->vRange, img->uRange, img->vRange);
            break;
    }
}
GN_API void GNg::ImageDrawRot(sIMAGE *img, int x, int y, int w, int h, float r, float cu, float cv, unsigned int color, unsigned short texID, unsigned short blendmode) {
    GT_TMP_GET_IMAGE_WIDTH_AND_UV

    float vt[8], _vt[8];
    _vt[0] = (float)(-w) * cu;
    _vt[1] = (float)(-h) * cv;
    _vt[2] = _vt[0] + (float)w;
    _vt[3] = _vt[1];
    _vt[4] = _vt[2];
    _vt[5] = _vt[1] + (float)h;
    _vt[6] = _vt[0];
    _vt[7] = _vt[5];

    sMAT33 rot_mat;
    M33GetRot(&rot_mat, r);
    rot_mat.m31 = (float)x;
    rot_mat.m32 = (float)y;
    M33TransformCoords((sVEC2 *)vt, (sVEC2 *)_vt, 4, &rot_mat);
    ImageDraw4P(img, vt, color, texID, blendmode, left_top_u, left_top_v, right_bottom_u, right_bottom_v);
}
GN_API void GNg::Draw2DLine(sIMAGE *img, int x, int y, int x2, int y2, int line_w, float cv, unsigned int color, unsigned short texID, unsigned short blendmode) {
    sVEC2 p1(x, y), p2(x2, y2);
    Draw2DLine(img, &p1, &p2, line_w, cv, color, texID, blendmode);
}
GN_API void GNg::Draw2DLine(sIMAGE *img, const sVEC2 *p1, const sVEC2 *p2, int line_w, float cv, unsigned int color, unsigned short texID, unsigned short blendmode) {
    float left_top_u = 0.0f, left_top_v = 0.0f, right_bottom_u = 1.0f, right_bottom_v = 1.0f;
    sVEC2 vw = *p2 - *p1, vh, vt[4];
    float len = Vec2Length(&vw);
    if(img) {
        right_bottom_u = img->uRange * (len / (float)img->width);
        right_bottom_v = img->vRange;
    }
    Vec2Right(&vh, &vw);
    vh *= line_w / len;
    vt[0] = vh * (-cv) + (*p1);
    vt[1] = vt[0] + vw;
    vt[3] = vt[0] + vh;
    vt[2] = vt[3] + vw;

    ImageDraw4P(img, (float *)vt, color, texID, blendmode, 0.0f, 0.0f, right_bottom_u, right_bottom_v);
}
GN_API void GNg::ImageDraw9Square(sIMAGE *img9, int x, int y, int w, int h, unsigned int color, unsigned short blendmode, uint8 repeat) {
    if(!img9 || img9->texnum != 9) ImageDraw(img9, x, y, w, h, color, 0, blendmode);
    else {
        int imgw = img9->width, imgh = img9->height;
        int cw = w - imgw - imgw, ch = h - imgh - imgh;
        int x1 = x + imgw, y1 = y + imgh, x2 = x + w - imgw, y2 = y + h - imgh;
        float u = img9->uRange, v = img9->vRange;
        if(repeat & 1) {
            u = (float)cw / (float)imgw;
            v = (float)ch / (float)imgh;
        }
        if(repeat < 2) ImageDraw(img9, x1, y1, cw, ch, color, 4, blendmode, 0.0f, 0.0f, u, v); //中间
        ImageDraw(img9, x, y, imgw, imgh, color, 0, blendmode); //绘制左上角
        ImageDraw(img9, x2, y, imgw, imgh, color, 2, blendmode); //绘制右上角
        ImageDraw(img9, x, y2, imgw, imgh, color, 6, blendmode); //绘制左下角
        ImageDraw(img9, x2, y2, imgw, imgh, color, 8, blendmode); //绘制右下角

        ImageDraw(img9, x1, y, cw, imgh, color, 1, blendmode, 0.0f, 0.0f, u, img9->vRange); //上边
        ImageDraw(img9, x, y1, imgw, ch, color, 3, blendmode, 0.0f, 0.0f, img9->uRange, v); //左边
        ImageDraw(img9, x2, y1, imgw, ch, color, 5, blendmode, 0.0f, 0.0f, img9->uRange, v); //右边
        ImageDraw(img9, x1, y2, cw, imgh, color, 7, blendmode, 0.0f, 0.0f, u, img9->vRange); //下边
    }
}
GN_API void GNg::ImageDrawMaskRect(GNg::sIMAGE *img, int x, int y, int w, int h, unsigned int color, unsigned short texID, unsigned short blendmode, const GNg::sRECT *rects, unsigned short rect_n) {
#ifdef GN_DEBUG
    if(!rects || !rect_n) GN_Log("调用 ImageDrawMaskRect(...) 参数:rects 不能为空!\n");
#endif
    //该函数暂时只对1个矩形避开处理 更多的以后再补充
    if(rects->top - y > h || rects->bottom < y) return;
    int x1, w1, l = rects->left - x;
    if(l > w) return;
    else if(l > 0) {
        x1 = rects->left;
        ImageDraw(img, x, y, l, h, color, texID, blendmode);
    }
    l = x + w;
    int right = rects->right;
    if(right < x) return;
    else if(right < l) {
        w1 = right - rects->left;
        ImageDraw(img, right, y, l - right, h, color, texID, blendmode);
    }
    l = rects->top - y;
    if(l > 0)
        ImageDraw(img, x1, y, w1, l, color, texID, blendmode);

    l = y + h;
    if(rects->bottom < l)
        ImageDraw(img, x1, rects->bottom, w1, l - rects->bottom, color, texID, blendmode);

    //之后的其它矩形......
}
GN_API void GNg::ImageDrawMaskRect(GNg::sIMAGE *img, int x, int y, int w, int h, unsigned int color, unsigned short texID, unsigned short blendmode, float left_top_u, float left_top_v, float right_bottom_u, float right_bottom_v, const GNg::sRECT *rects, unsigned short rect_n) {
#ifdef GN_DEBUG
    if(!rects || !rect_n) GN_Log("调用 ImageDrawMaskRect(...) 参数:rects 不能为空!\n" /*"ImageDrawMaskRect(...) parame: rects is NULL!\n"*/);
#endif
    //该函数暂时只对1个矩形避开处理 更多的以后再补充
    if(rects->top - y > h || rects->bottom < y) return;
    float ul = right_bottom_u - left_top_u, vl = right_bottom_v - left_top_v;
    float u, u2;
    int x1, w1, l = rects->left - x;
    if(l > w) return;
    else if(l > 0) {
        x1 = rects->left;
        u = ul * ((float)l / (float)w) + left_top_u;
        ImageDraw(img, x, y, l, h, color, texID, blendmode, left_top_u, left_top_v, u, right_bottom_v);
    }
    l = x + w;
    int right = rects->right;
    if(right < x) return;
    else if(right < l) {
        w1 = right - rects->left;
        u2 = ul * ((float)(right - x) / (float)w) + left_top_u;
        ImageDraw(img, right, y, l - right, h, color, texID, blendmode, u2, left_top_v, right_bottom_u, right_bottom_v);
    }
    l = rects->top - y;
    if(l > 0)
        ImageDraw(img, x1, y, w1, l, color, texID, blendmode, u, left_top_v, u2, vl * ((float)l / (float)h) + left_top_v);

    l = y + h;
    if(rects->bottom < l)
        ImageDraw(img, x1, rects->bottom, w1, l - rects->bottom, color, texID, blendmode, u, vl * ((float)(rects->bottom - y) / (float)h) + left_top_v, u2, right_bottom_v);
    /*
	if(--rect_n){ //递归继续分割
        //...
        GNg::ImageDrawMaskRect(img,x1, y, w1, h ,color ,texID , blendmode  ,u,left_top_v,u2,right_bottom_v, ++rects,rect_n);
	}*/
}
GN_API void GNg::ImageDrawDitherBlur(sIMAGE *img, int x, int y, int w, int h, unsigned int color, unsigned short texID, unsigned short blendmode, unsigned short count, float dither) {
    ImageDraw(img, x, y, w, h, color, texID, blendmode); //先画一个在中央

#define GT_TMP_DRAW_IMG_DITHER_BLUR_V                             \
    sVEC2 v(w > h ? (float)w * dither : (float)h * dither, 0.0f); \
    ((uint8 *)(&color))[3] /= 3;                                  \
    float ag = 360.f / (float)(count);                            \
    unsigned short ci = 0;                                        \
    for(; ci < count; ++ci) {

    GT_TMP_DRAW_IMG_DITHER_BLUR_V
    ImageDraw(img, x + (int)v.x, y + (int)v.y, w, h, color, texID, blendmode);
    Vec2Rot(&v, ag);
}
}
GN_API void GNg::ImageDrawDitherBlur(sIMAGE *img, int x, int y, int w, int h, unsigned int color, unsigned short texID, unsigned short blendmode, float left_top_u, float left_top_v, float right_bottom_u, float right_bottom_v, unsigned short count, float dither) {
    ImageDraw(img, x, y, w, h, color, texID, blendmode, left_top_u, left_top_v, right_bottom_u, right_bottom_v); //先画一个在中央

    GT_TMP_DRAW_IMG_DITHER_BLUR_V
    ImageDraw(img, x + (int)v.x, y + (int)v.y, w, h, color, texID, blendmode, left_top_u, left_top_v, right_bottom_u, right_bottom_v);
    Vec2Rot(&v, ag);
}
}
GN_API void GNg::ImageDrawDitherBlurProjToViewRng(sIMAGE *img, int x, int y, int w, int h, unsigned int color, unsigned short texID, unsigned short blendmode, cVIEW2D *view, unsigned short count, float dither, bool is_col_turn) {
    float ufr = (1.0f / (float)view->orthoW), /*模版代码:绘制图像投影到视口的部分*/
        vfr = (1.0f / (float)view->orthoH);
    float v0 = (float)(y - view->ortho_top) * vfr,
          v1 = (float)(y + h - view->ortho_top) * vfr;
    if(is_col_turn) {
        v0 = 1.0f - v0;
        v1 = 1.0f - v1;
    }
    ImageDrawDitherBlur(img, x, y, w, h, color, texID, blendmode,
                        (float)(x - view->ortho_left) * ufr * img->uRange,
                        v0 * img->vRange,
                        (float)(x + w - view->ortho_left) * ufr * img->uRange,
                        v1 * img->vRange,
                        count, dither);
}
GN_API void GNg::Draw2DLinesToMem(unsigned char *p_memory, int mw, int mh, short color_bit, const int *px, const int *py, int count, int line_w, unsigned int color, bool isLoop, const sRECT *scissor) {
}
/**************************************************  靠取景范围边筐功能部分  *************************************************************/

GN_API GNg::sVEC2 GNu::Near(float near_pointU, float near_pointV, int offset_x, int offset_y) {
    GNg::cVIEW2D *view = GNg::ViewCur;
    return GNg::sVEC2(
        (float)view->ortho_left + ((float)view->orthoW * near_pointU) + (float)offset_x,
        (float)view->ortho_top + ((float)view->orthoH * near_pointV) + (float)offset_y);
}
GN_API GNg::sVEC2 GNu::Near(float near_pointU, float near_pointV, GNg::sVEC2 offset) {
    GNg::cVIEW2D *view = GNg::ViewCur;
    return GNg::sVEC2(
        (float)view->ortho_left + ((float)view->orthoW * near_pointU) + offset.x,
        (float)view->ortho_top + ((float)view->orthoH * near_pointV) + offset.y);
}
/*****************************************************************************************************************************************/
GN_API void GN_RenderBegin2D() {
    GT_ReSetRenderState2D();
    if(GNg::ViewCur->zHasOrthoOfsTg) GNg::ViewCur->BeginRender2D(); //架设主视口准备2D渲染
}
GN_API void GF_InitGraphics(const int *window, short view_w, short view_h, short color_bit, short back_src_left, short back_src_top, short back_src_right, short back_src_bottom, bool full_screen, bool window_zoom) {
    GNg::sWINDOW::Hwnd = (int *)window;
    GNg::sWINDOW::WindowZoom = window_zoom;
    GNg::sWINDOW::FullScreen = full_screen;
    GNg::sWINDOW::FBufferColorBit = color_bit;

    GNg::ViewCur->Init(0, 0, view_w, view_h, back_src_left, back_src_top, back_src_right, back_src_bottom);
    GT_InitG2D(GNg::ViewCur);
    GNg::RenderRotSupport = GN_Has3DAccelerate();
    GNg::ViewCur->BeginRender2D(); //架设主视口准备2D渲染
    GNg::BackScreen128.CreateEmptyToData(128, 128);
}
GN_API void GF_UnInitGraphics() {
    GNu::cTREE_VIEW::aDefaultUnfoldImg.Delete();
    GNg::cVIEW2D::screenImg.Delete();
    GNg::BackScreen128.Release();
    GNg::ViewCur->~cVIEW2D();
    if(!GNg::cVIEW2D::zTotalList.empty()) GNg::cVIEW2D::zTotalList.clear(); //删除总视图队列
    GT_UnInitG2D();
}
#endif //endif _GN_2D_H_
