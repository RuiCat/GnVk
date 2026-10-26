#ifndef _GND_H_
#define _GND_H_
/************************************************* 2D 内容 ***************************************************************/
#ifdef GT_USING_OPENGL
//获得矫正红蓝分量的颜色(  颜色 )  由于不同的系统关系, 系统颜色可能会有红色和蓝色掉转的情况
#define GNg_GetCorrectColor(CR) ((CR & 0xff00ff00) | ((CR & 0x000000ff) << 16) | ((CR & 0x00ff0000) >> 16))
#else
#define GNg_GetCorrectColor(CR) (CR)
#endif
#define GT_TMP_GET_IMAGE_WIDTH_AND_UV                                                         \
    float left_top_u = 0.0f, left_top_v = 0.0f, right_bottom_u = 1.0f, right_bottom_v = 1.0f; \
    if(img) {                                                                                 \
        w = w ? w : img->width;                                                               \
        h = h ? h : img->height;                                                              \
        right_bottom_u = img->uRange;                                                         \
        right_bottom_v = img->vRange;                                                         \
    }
/************************************************* 文字处理内容 *************************************************************/

//选择范围处理( 是否中文字的比较方法1, 是否中文字的比较方法2, 范围处理方法 )
#define GT_DrawTextSelectRange(CMP_IsCn1, CMP_IsCn2, PROC)                                                                                                        \
    if(_fmt->erect_print) {                                                                                                                                       \
        /*多行选择筐*/                                                                                                                                            \
        for(rj = min_row_id, ri = min_id; rj < max_row_id; rj++, ri = row_sids[rj]) {                                                                             \
            _arrg2 = _dw_rng + ri;                                                                                                                                \
            rx = CMP_IsCn1 ? _arrg2->x : _arrg2->x - (_arrg2->w >> 1);                                                                                            \
            ry = _arrg2->y;                                                                                                                                       \
            _arrg = _dw_rng + (row_sids[rj + 1] - 1);                                                                                                             \
            rw = CMP_IsCn2 ? _arrg->w : _arrg->w << 1;                                                                                                            \
            rh = _arrg->y + _arrg->h - ry;                                                                                                                        \
            PROC                                                                                                                                                  \
        } /*尾行选择筐*/                                                                                                                                          \
        _arrg2 = _dw_rng + ri;                                                                                                                                    \
        rx = CMP_IsCn1 ? _arrg2->x : _arrg2->x - (_arrg2->w >> 1);                                                                                                \
        ry = _arrg2->y;                                                                                                                                           \
        _arrg = _dw_rng + max_id;                                                                                                                                 \
        rw = CMP_IsCn2 ? _arrg->w : _arrg->w << 1;                                                                                                                \
        rh = _arrg->y + _arrg->h - ry - (int)((float)_arrg->h / (float)_fmt->height * (float)_fmt->j_compartment); /*忽略最后一个不显示以刮分清楚插入符的上下边*/ \
        PROC                                                                                                                                                      \
    } else {                                                                                                                                                      \
        if(_fmt->overturn_print) { /*横方向翻转输出选择范围的处理*/                                                                                               \
            /*多行选择筐*/                                                                                                                                        \
            for(rj = min_row_id, ri = min_id; rj < max_row_id; rj++, ri = row_sids[rj]) {                                                                         \
                _arrg = _dw_rng + (row_sids[rj + 1] - 1);                                                                                                         \
                rx = _arrg->x;                                                                                                                                    \
                ry = _arrg->y;                                                                                                                                    \
                _arrg2 = _dw_rng + ri;                                                                                                                            \
                rw = _arrg2->x + _arrg2->w - rx;                                                                                                                  \
                rh = _arrg2->y + _arrg2->h - ry;                                                                                                                  \
                PROC                                                                                                                                              \
            } /*尾行选择筐*/                                                                                                                                      \
            _arrg = _dw_rng + ri;                                                                                                                                 \
            rx = _dw_rng[max_id].x + ((float)_arrg->w / (float)_fmt->width * (float)_fmt->i_compartment); /*忽略最后一个不显示以区分插入符的左右边*/              \
            ry = _dw_rng[max_id].y;                                                                                                                               \
            rw = _arrg->x + _arrg->w - rx;                                                                                                                        \
            rh = _arrg->y + _arrg->h - ry;                                                                                                                        \
            PROC                                                                                                                                                  \
        } else { /*其它方式输出选择范围的处理*/                                                                                                                   \
            /*多行选择筐*/                                                                                                                                        \
            for(rj = min_row_id, ri = min_id; rj < max_row_id; rj++, ri = row_sids[rj]) {                                                                         \
                rx = _dw_rng[ri].x;                                                                                                                               \
                ry = _dw_rng[ri].y;                                                                                                                               \
                _arrg = _dw_rng + (row_sids[rj + 1] - 1);                                                                                                         \
                rw = _arrg->x + _arrg->w - rx;                                                                                                                    \
                rh = _arrg->y + _arrg->h - ry;                                                                                                                    \
                PROC                                                                                                                                              \
            } /*尾行选择筐*/                                                                                                                                      \
            rx = _dw_rng[ri].x;                                                                                                                                   \
            ry = _dw_rng[ri].y;                                                                                                                                   \
            _arrg = _dw_rng + max_id;                                                                                                                             \
            rw = _arrg->x + _arrg->w - rx - (int)((float)_arrg->w / (float)_fmt->width * (float)_fmt->i_compartment); /*忽略最后一个不显示以区分插入符的左右边*/  \
            rh = _arrg->y + _arrg->h - ry;                                                                                                                        \
            PROC                                                                                                                                                  \
        }                                                                                                                                                         \
    }
/********************************************** 保存与读取时的共享引用对象 ***********************************************/
struct GT_sSCENE2D_SHARE_OBJS_R //2D场景共享引用对象
{
    GNg::cFRAME_RC::sREF_OBJSR frame_ref_objs; //所有在动画图筐里的共享引用对象
    P<GNg::cFRAME *> pframes; //所有在发射器里的共享筐对象
    P<P<char>> pkeep_objs; //所有在发射器里的保留鉴别对象
    P<P<GNg::sIMAGE>> pimages; //所有独立的图像
    P<P<GNg::cLENS_FLARE>> pflares; //所有光环效果式样
};
struct GT_sSCENE2D_SHARE_OBJS //2D场景共享对象
{
    GNg::cFRAME_RC::sREF_OBJSW in_frame_objs; //所有在动画图筐里的共享对象
    GN::sLIST<GNg::cFRAME *> list_frame; //所有在发射器里的共享筐对象
    GN::sLIST<P<char>> list_keep_obj; //所有在发射器里的保留鉴别对象
    GN::sLIST<P<GNg::sIMAGE>> list_img; //所有独立的图像
    GN::sLIST<P<GNg::cLENS_FLARE>> list_flare; //所有光环效果式样
};
class GT_cUI_SHARE_DATA_R : public GNu::cSHARE_DATA, public GT_sSCENE2D_SHARE_OBJS_R {
  public:
    P<P<P<char>>> ptexts_s; //所有文本数组引用对象
    P<P<GNu::iUI_OBJECT>> puobjs; //所有UI子组件引用对象
    P<GNg::cFRAME *> pframes_rc; //所有独立的正规动画图筐
    P<P<GNa::sSOUND>> psounds; //所有独立的声音对象

    P<P<GNg::cFRAME>> ppframes; //个别在发射器里的共享筐安全指针对象 用于转换某些 pframes 里的指针

    GT_cUI_SHARE_DATA_R() { pimage_refs = &pimages; }
    virtual ~GT_cUI_SHARE_DATA_R() {}
};
class GT_cUI_SHARE_DATA_W : public GNu::cSHARE_DATA, public GT_sSCENE2D_SHARE_OBJS {
  public:
    GN::sLIST<P<P<char>>> list_texts; //文本数组列表
    GN::sLIST<P<GNu::iUI_OBJECT>> list_uobj; //UI子组件列表
    GN::sLIST<GNg::cFRAME *> list_frame_rc; //所有独立的正规动画图筐
    GN::sLIST<P<GNa::sSOUND>> list_sound; //所有独立的声音对象

    GN::sLIST<GNg::cFRAME *> list_fileicon; //使用了默认文件图标的动图筐
    unsigned int scroll_act_frame_id; //滚动列表默认点中的演变效果筐所在队列位置ID

    GT_cUI_SHARE_DATA_W() : scroll_act_frame_id(0xffff) { pimage_list = &list_img; }
    virtual ~GT_cUI_SHARE_DATA_W() {}
};
#define GT_TMP_SCROLL_LIST_DATA_SAVE(ACT_FM)                                                   \
    GT_cUI_SHARE_DATA_W *sdw = (GT_cUI_SHARE_DATA_W *)share_objs;                              \
    wpos = zNearAreaDataSave(wpos);                                                            \
    wpos = cSCROLL_CTRL::zDataSave(wpos, sdw->list_img);                                       \
    unsigned int pointer_id;                                                                   \
    GT_TMP_OBJECT_WRITE_INDEX(sdw->list_img, zFocusImg);                                       \
    unsigned int *act_frame_id = (unsigned int *)wpos;                                         \
    wpos = zFFrames->zDataSave(wpos, sdw->list_frame, sdw->list_keep_obj, false);              \
    if(act_frame_id[3] != 0xffffffff && zFFrames->zActFrame == (GNg::cFRAME *)ACT_FM) {        \
        sdw->scroll_act_frame_id = act_frame_id[3]; /*记录默认点中的演变效果筐所在队列位置ID*/ \
    }                                                                                          \
    GNg::cFRAME *_fm_icon = zFrameIcon;                                                        \
    GT_TMP_OBJECT_WRITE_INDEX(sdw->list_frame, _fm_icon);

#define GT_TMP_SCROLL_LIST_DATA_LOAD                                          \
    GT_cUI_SHARE_DATA_R *sdr = (GT_cUI_SHARE_DATA_R *)share_objs;             \
    rpos = zNearAreaDataLoad(rpos);                                           \
    rpos = cSCROLL_CTRL::zDataLoad(rpos, sdr->pimages);                       \
    unsigned int pointer_id;                                                  \
    memcpy(&pointer_id, rpos, 4);                                             \
    rpos += 4;                                                                \
    if(pointer_id != 0xffffffff) zFocusImg = sdr->pimages[pointer_id];        \
    zFFrames.New(1);                                                          \
    rpos = zFFrames->zDataLoad(rpos, sdr->pframes, sdr->pkeep_objs, false);   \
    zReArrange = true;                                                        \
    memcpy(&pointer_id, rpos, 4);                                             \
    rpos += 4;                                                                \
    if(pointer_id != 0xffffffff) {                                            \
        if(!sdr->ppframes) sdr->ppframes.New(sdr->pframes.Num());             \
        if(sdr->ppframes[pointer_id]) zFrameIcon = sdr->ppframes[pointer_id]; \
        else {                                                                \
            zFrameIcon.New(1);                                                \
            *zFrameIcon = *(sdr->pframes[pointer_id]);                        \
            sdr->ppframes[pointer_id] = zFrameIcon;                           \
        }

#define GT_TMP_LOAD_FONT_FRAME_TRANSFER                                                     \
    if(zFFrames->zFontFrame == sdr->pframes[pointer_id]) zFFrames->zFontFrame = zFrameIcon; \
    }

#define GT_TMP_SCROLL_LIST_ZONING_RESET               \
    uint8 locbuf[2048];                               \
    GN::sTBUF_INT tbuf(locbuf, 2048);                 \
    char *pic_id = (char *)tbuf.GetSpace(zZoningNum); \
    memset(pic_id, 0, zZoningNum);                    \
    zCheckResetZoningCount(pic_id, zFFrames);

#define GT_TMP_SEARCH_SCENE_PTR(CLASS, TOBJ, THIS)           \
    CLASS optr;                                              \
    char **ppname = zObjNameTable;                           \
    if(!strcmp(*ppname, name)) {                             \
        optr.aPtr = THIS;                                    \
        return optr;                                         \
    }                                                        \
    GT_Search##TOBJ##FromGroup(this, name, ++ppname, &optr); \
    return optr;

/************************************************* UI 内容 ***************************************************************/
//初始化箭头UI动画
#define GT_UiInitArrowAm()                                                                                                                             \
    {                                                                                                                                                  \
        if(zFev.segment_time) zArrowAm.Init(zOffsetArrow, 1, &zFev, GNg::Ts2D, 1.0f, 0.5f, aFocusBlend, aFocusColor, GNg::eAM_REPEAT, 0, -zRepeatNum); \
        else zArrowAm.Init(zOffsetArrow, 0, 0, GNg::Ts2D, 1.0f, 0.5f, aFocusBlend, aFocusColor, GNg::eAM_NOAM, 0, -zRepeatNum);                        \
    }
#define GT_TMP_UI_OffsetArrowInit()                \
    if(!zOffsetArrow) {                            \
        zOffsetArrow = GN::RunRec.aGV_OffsetArrow; \
        GT_UiInitArrowAm();                        \
    }                                              \
    zArrowAm.aColor = aFocusColor

#define GT_TMP_UI_SET_EFFECT                            \
    zFev.segment_time = (unsigned int)focus_flash_time; \
    if(effect) zFFrames = effect;                       \
    if(offset_arrow) zOffsetArrow = offset_arrow;       \
    zFocusImg = focus_style;                            \
    zRepeatNum = repeat_num;                            \
    GT_UiInitArrowAm();                                 \
    zTextOffset = 1.0f;                                 \
    zTextOffsettg = 1.0f;                               \
    zReArrange = true

#define GT_TMP_DEF_FRAME_INIT_ARGS_ONCE_KEEP , GNg::Ts2D, 0.5f, 0.5f, 1, 0xffffffff, GNg::eAM_ONCE_KEEP

#define GT_TMP_FFRAME_RANGE_GET                                                           \
    int tx, ty, tw, th, mouse_x = GNg::ViewCur->mouseMX, mouse_y = GNg::ViewCur->mouseMY; \
    int zx = (int)_fframe->aGenRngX, zy = (int)_fframe->aGenRngY,                         \
        zw = (int)_fframe->aGenRngW, zh = (int)_fframe->aGenRngH;                         \
    int zr = zw + zx, zb = zh + zy

#define GT_TMP_UI_GET_SET_POS_AND_OTHER_FUNC_DEF(CLASS_TYPE)                                                                             \
    short GNu::CLASS_TYPE::GetParalleShowCount() {                                                                                       \
        GNg::cFRAME_LAUNCHER_FONT *_fframe = zFFrames;                                                                                   \
        GNg::sARRANGE_FMT &ffmt = _fframe->zParame;                                                                                      \
        short arng_n = 1;                                                                                                                \
        if(ffmt.erect_print) {                                                                                                           \
            if((short)(_fframe->aGenRngH) > ffmt.height)                                                                                 \
                arng_n = ((short)(_fframe->aGenRngH) - ffmt.height) / ffmt.j_compartment + 1;                                            \
        } else {                                                                                                                         \
            if((short)(_fframe->aGenRngW) > ffmt.width)                                                                                  \
                arng_n = ((short)(_fframe->aGenRngW) - ffmt.width) / ffmt.i_compartment + 1;                                             \
        }                                                                                                                                \
        return arng_n;                                                                                                                   \
    }                                                                                                                                    \
    void GNu::CLASS_TYPE::SetPos(short _x, short _y, short _w, short _h) {                                                               \
        GNg::sRECT2 rc2d((float)_x, (float)_y, (float)(_x + _w), (float)(_y + _h));                                                      \
        SetPos(rc2d);                                                                                                                    \
    }                                                                                                                                    \
    void GNu::CLASS_TYPE::GetPos(short *_x, short *_y, short *_w, short *_h) {                                                           \
        GNg::cFRAME_LAUNCHER_FONT *_fframe = zFFrames;                                                                                   \
        if(_fframe) {                                                                                                                    \
            *_x = (short)_fframe->aGenRngX;                                                                                              \
            *_y = (short)_fframe->aGenRngY;                                                                                              \
            *_w = (short)_fframe->aGenRngW;                                                                                              \
            *_h = (short)_fframe->aGenRngH;                                                                                              \
        }                                                                                                                                \
    }                                                                                                                                    \
    GNg::sVEC2 GNu::CLASS_TYPE::GetPosXY() {                                                                                             \
        if(zFFrames) return GNg::sVEC2(zFFrames->aGenRngX, zFFrames->aGenRngY);                                                          \
        else return GNg::Vector2D;                                                                                                       \
    }                                                                                                                                    \
    void GNu::CLASS_TYPE::SetPos(GNg::sRECT2 &range) {                                                                                   \
        GNg::cFRAME_LAUNCHER_FONT *_fframe = zFFrames;                                                                                   \
        if(!_fframe) return;                                                                                                             \
        if(((short)range.right - (short)range.left == (short)_fframe->aGenRngW) &&                                                       \
           ((short)range.bottom - (short)range.top == (short)_fframe->aGenRngH)) {                                                       \
            _fframe->Move((int)range.left - (int)_fframe->aGenRngX, (int)range.top - (int)_fframe->aGenRngY);                            \
        } else {                                                                                                                         \
            zReArrange = true;                                                                                                           \
            int arng_n = (int)GetParalleShowCount();                                                                                     \
            _fframe->aGenRngX = range.left;                                                                                              \
            _fframe->aGenRngY = range.top;                                                                                               \
            _fframe->aGenRngW = range.right - range.left;                                                                                \
            _fframe->aGenRngH = range.bottom - range.top;                                                                                \
            int arng_n_now = (int)GetParalleShowCount();                                                                                 \
            if(arng_n != arng_n_now) {                                                                                                   \
                int compartment = (int)(_fframe->zParame.erect_print ? _fframe->zParame.i_compartment : _fframe->zParame.j_compartment); \
                int n = (int)zTextOffsettg / compartment;                                                                                \
                if(n) {                                                                                                                  \
                    int _ofs = (int)zTextOffsettg - (n * compartment);                                                                   \
                    zTextOffsettg = (float)(((n * arng_n) / arng_n_now) * compartment + _ofs);                                           \
                    zTextOffset = zTextOffsettg;                                                                                         \
                }                                                                                                                        \
            }                                                                                                                            \
        }                                                                                                                                \
    }                                                                                                                                    \
    void GNu::CLASS_TYPE::GetPos(GNg::sRECT2 &range) {                                                                                   \
        if(zFFrames) zFFrames->GetRange(&range);                                                                                         \
        else range = GNg::Rect2D;                                                                                                        \
    }                                                                                                                                    \
    void GNu::CLASS_TYPE::GetDrawArea(GNg::sDRAW_AREA2 &out, cUI_GROUP *parent) {                                                        \
        GetPos(&out.x, &out.y, &out.w, &out.h);                                                                                          \
        out.c = parent ? parent->gc : 0xffffffff;                                                                                        \
    }                                                                                                                                    \
    void GNu::CLASS_TYPE::OnRePos(sUI_LEVEL *parent) {                                                                                   \
        GNg::sDRAW_AREA2 rng;                                                                                                            \
        DrawRangeGet(rng, parent);                                                                                                       \
        GNg::sRECT2 rc2d((float)rng.x, (float)rng.y, (float)(rng.x + rng.w), (float)(rng.y + rng.h));                                    \
        SetPos(rc2d);                                                                                                                    \
    }                                                                                                                                    \
    void GNu::CLASS_TYPE::OnMove(short mx, short my) {                                                                                   \
        zFFrames->Move(mx, my);                                                                                                          \
    }
#ifdef GT_TMP_COMPATI_DX_DDRAW
#define GT_TMP_DDRAW_MODE_GET const bool ddraw_mode = !GN_Has3DAccelerate();
#define GT_TMP_DDRAW_SCROLL_FOCUS                                                  \
    if(ddraw_mode) GNg::ImageDraw(0, tx, ty, tw, th, focus_color, 0, aFocusBlend); \
    else
#define GT_TMP_NOT_DDRAW_MODE !ddraw_mode &&
#else
#define GT_TMP_DDRAW_MODE_GET
#define GT_TMP_DDRAW_SCROLL_FOCUS
#define GT_TMP_NOT_DDRAW_MODE
#endif
#define GT_TMP_UI_SCROLL_VIEW(MR, END_RANGE, NUM, LIMIT)                                                                                 \
    GT_TMP_DDRAW_MODE_GET                                                                                                                \
    if(!GN::Mouse[GM_LEFT]) zKeyOnDown = false;                                                                                          \
    if(zIsMouseFocus && GN::Mouse1[GM_LEFT]) zKeyOnDown = true;                                                                          \
                                                                                                                                         \
    zArrowAm.aBlendmode = 0;                                                                                                             \
    zArrowAm.aColor = aFocusColor;                                                                                                       \
    int move_dist = (int)MR;                                                                                                             \
    if(zKeyOnDown && GN::MouseM) { /*按紧鼠标左键拖动*/                                                                                  \
        if(pfmt->erect_print) move_dist = GNg::ViewCur->mouseMSX;                                                                        \
        else move_dist = GNg::ViewCur->mouseMSY;                                                                                         \
        if(move_dist < -38 || move_dist > 38) move_dist <<= 1;                                                                           \
        else zTextOffset += (zTextOffsettg - zTextOffset) * GN::TimeRatioL * 0.8f;                                                       \
                                                                                                                                         \
    } else if(zIsMouseFocus) {                                                                                                           \
        if(GN::Mouse1[GM_TOP]) { /*鼠标滚轮上*/                                                                                          \
            if(pfmt->erect_print) move_dist = pfmt->i_compartment;                                                                       \
            else move_dist = pfmt->j_compartment;                                                                                        \
                                                                                                                                         \
        } else if(GN::Mouse1[GM_BOTTOM]) { /*鼠标滚轮下*/                                                                                \
            if(pfmt->erect_print) move_dist = -pfmt->i_compartment;                                                                      \
            else move_dist = -pfmt->j_compartment;                                                                                       \
                                                                                                                                         \
        } else { /*鼠标移动到筐边沿的拖动处理*/                                                                                          \
            tx = zx;                                                                                                                     \
            ty = zy;                                                                                                                     \
            if(pfmt->erect_print) {                                                                                                      \
                th = zh;                                                                                                                 \
                tw = zOffsetArrow->width * zArrowAm.aFrame_ts.sx;                                                                        \
                zArrowAm.aFrame_ts.sy = _fframe->aGenRngH / (float)zOffsetArrow->height;                                                 \
                if(zRepeatNum) zArrowAm.aFrame_ts.sy /= (float)zRepeatNum;                                                               \
                if(GNg::TestPointIntRect(mouse_x, mouse_y, tx, ty, tw, th)) {                                                            \
                    GT_TMP_DDRAW_SCROLL_FOCUS {                                                                                          \
                        zArrowAm.aFrame_ts.x = (float)tx;                                                                                \
                        zArrowAm.aFrame_ts.y = (float)(ty + (th >> 1));                                                                  \
                        zArrowAm.aFrame_ts.r = 180.0f; /*模版代码:旋转一下拖动条*/                                                       \
                        zArrowAm.aBlendmode = aFocusBlend;                                                                               \
                        zArrowAm.PlayFrame(updatatime);                                                                                  \
                    }                                                                                                                    \
                    move_dist = (int)(12.0f * GN::TimeRatioL);                                                                           \
                }                                                                                                                        \
                tx = zr - tw;                                                                                                            \
            } else {                                                                                                                     \
                tw = zw;                                                                                                                 \
                th = zOffsetArrow->width * zArrowAm.aFrame_ts.sx;                                                                        \
                zArrowAm.aFrame_ts.sy = _fframe->aGenRngW / (float)zOffsetArrow->height;                                                 \
                if(zRepeatNum) zArrowAm.aFrame_ts.sy /= (float)zRepeatNum;                                                               \
                if(GNg::TestPointIntRect(mouse_x, mouse_y, tx, ty, tw, th)) {                                                            \
                    GT_TMP_DDRAW_SCROLL_FOCUS {                                                                                          \
                        zArrowAm.aFrame_ts.x = (float)(tx + (tw >> 1));                                                                  \
                        zArrowAm.aFrame_ts.y = (float)ty;                                                                                \
                        zArrowAm.aFrame_ts.r = 270.0f; /*模版代码:旋转一下拖动条*/                                                       \
                        zArrowAm.aBlendmode = aFocusBlend;                                                                               \
                        zArrowAm.PlayFrame(updatatime);                                                                                  \
                    }                                                                                                                    \
                    move_dist = (int)(12.0f * GN::TimeRatioL);                                                                           \
                }                                                                                                                        \
                ty = zb - th;                                                                                                            \
            }                                                                                                                            \
            if(!move_dist && GNg::TestPointIntRect(mouse_x, mouse_y, tx, ty, tw, th)) {                                                  \
                GT_TMP_DDRAW_SCROLL_FOCUS {                                                                                              \
                    if(pfmt->erect_print) {                                                                                              \
                        zArrowAm.aFrame_ts.x = (float)(tx + (tw));                                                                       \
                        zArrowAm.aFrame_ts.y = (float)(ty + (th >> 1));                                                                  \
                        zArrowAm.aFrame_ts.r = 0.0f; /*旋转拖动条一下来显示*/                                                            \
                    } else {                                                                                                             \
                        zArrowAm.aFrame_ts.x = (float)(tx + (tw >> 1));                                                                  \
                        zArrowAm.aFrame_ts.y = (float)(ty + (th));                                                                       \
                        zArrowAm.aFrame_ts.r = 90.0f;                                                                                    \
                    }                                                                                                                    \
                    zArrowAm.aBlendmode = aFocusBlend;                                                                                   \
                    zArrowAm.PlayFrame(updatatime);                                                                                      \
                }                                                                                                                        \
                move_dist = (int)(12.0f * -GN::TimeRatioL);                                                                              \
            }                                                                                                                            \
        }                                                                                                                                \
    }                                                                                                                                    \
    if(GT_TMP_NOT_DDRAW_MODE !zArrowAm.aBlendmode) zArrowAm.PlayFrame(updatatime); /*如箭头没有被显示也运行更新一下 以便有动画颜色变化*/ \
                                                                                                                                         \
    LIMIT if(pfmt->erect_print && pfmt->overturn_print) { /*竖排列与翻转输出处理有点特殊的增加偏移量的处理*/                             \
        if(move_dist < 0) {                                                                                                              \
            if(zTextOffsettg > 0.0f) {                                                                                                   \
                if(zTextOffsettg + (float)move_dist < 0.0f) zTextOffsettg = 0.0f;                                                        \
                else zTextOffsettg += (float)move_dist;                                                                                  \
            }                                                                                                                            \
        } else if(move_dist > 0) {                                                                                                       \
            if((float)(NUM / _fframe->zRowSIDs[1] + 1) * (float)(pfmt->i_compartment) < zTextOffsettg + (_fframe->aGenRngW))             \
                ;                                                                                                                        \
            else zTextOffsettg += (float)move_dist;                                                                                      \
        }                                                                                                                                \
    }                                                                                                                                    \
    else { /*一般的增量偏移处理*/                                                                                                        \
        if(move_dist > 0) {                                                                                                              \
            if(zTextOffsettg < 0.0f) {                                                                                                   \
                if(zTextOffsettg + (float)move_dist > 0.0f) zTextOffsettg = 0.0f;                                                        \
                else zTextOffsettg += (float)move_dist;                                                                                  \
            }                                                                                                                            \
        } else if(move_dist < 0) {                                                                                                       \
            float col_num = (_fframe->zRowNum > 1) ? (float)(NUM / _fframe->zRowSIDs[1] + END_RANGE) : 1.0f;                             \
            bool is_end;                                                                                                                 \
            if(pfmt->erect_print) is_end = col_num * (float)(-pfmt->i_compartment) > zTextOffsettg - _fframe->aGenRngW;                  \
            else is_end = col_num * (float)(-pfmt->j_compartment) > zTextOffsettg - _fframe->aGenRngH;                                   \
            if(!is_end) zTextOffsettg += (float)move_dist;                                                                               \
        }                                                                                                                                \
    }                                                                                                                                    \
    float ofs_crs = (zTextOffsettg - zTextOffset) * zOffsetSheep * GN::TimeRatioL;                                                       \
    if((ofs_crs < 0.0f ? -ofs_crs : ofs_crs) > 0.3f) zReArrange = true;                                                                  \
    zTextOffset += ofs_crs

//滚动筐设置排位范围准备
#define GT_TMP_ScrollSetTextRangeBegin(TNUM)                                                                            \
    if(zLastStartPos && !ret_am) { /*恢复一下由于优化改过的文件索引号*/                                                 \
        unsigned int fn_i = 0, fn_n = _fframe->zMax_num;                                                                \
        GNg::sDRAW_RANGE *arrd_ = _fframe->zDrawRngData;                                                                \
        for(; fn_i < fn_n; ++fn_i) arrd_[fn_i].strid -= zLastStartPos;                                                  \
    }                                                                                                                   \
    GNg::sARRANGE_FMT *parame = &(_fframe->zParame);                                                                    \
    int _icon_offset = (int)zTextOffset;                                                                                \
    unsigned int arng_num = (unsigned int)GetParalleShowCount(), set_count = TNUM;                                      \
    unsigned int pid_start = 0, icon_offset = (unsigned int)(_icon_offset >= 0 ? _icon_offset : (-_icon_offset));       \
    if(set_count / arng_num + 1 > 254) { /*检测并裁短一下要 SetText 的范围 以防超出 SetText 最大256行的限制*/           \
        unsigned int compartment = (unsigned int)(parame->erect_print ? parame->i_compartment : parame->j_compartment); \
        pid_start = icon_offset / compartment;                                                                          \
        icon_offset -= pid_start * compartment;                                                                         \
        if(_icon_offset >= 0) _icon_offset = icon_offset;                                                               \
        else _icon_offset = -((int)icon_offset);                                                                        \
        pid_start *= arng_num;                                                                                          \
        set_count -= pid_start;                                                                                         \
        if(set_count / arng_num + 1 > 254) set_count = arng_num * 254;                                                  \
    }

#define GT_SELECT_LIST_ProcAndDrawFocus(PROC, LCR, FIMG)                                                                                       \
    if(sl->PROC) {                                                                                                                             \
        unsigned int focus_color = color | 0x00ffffff;                                                                                         \
        focus_color = GNg_MultiplyColorRatio(focus_color, sl->LCR);                                                                            \
        if(sl->aSelectFrameStyle == GNu::eRECT_DRAW_FILL) GNg::ImageDraw(sl->FIMG, x, y, w, h, focus_color, 0, sl->aFocusBlend);               \
        else if(sl->aSelectFrameStyle == GNu::eRECT_DRAW_WIRE) GNg::LineFrame2dDraw(x, y, w, h, sl->aLineWidth, focus_color, sl->aFocusBlend); \
    }

#define GT_SELECT_LIST_DrawText(TEXTOUT, TEXT)                                                                                           \
    unsigned int bkc = sl->aBackColor;                                                                                                   \
    if(bkc & 0xff000000) {                                                                                                               \
        if(sl->aAffectColor) bkc = GNg_MultiplyColorRatio(bkc, color);                                                                   \
        switch(sl->aBackStyle) {                                                                                                         \
            case GNu::eRECT_DRAW_NULL: GNg::ImageDraw(0, x, y + h - 1, w, 1, bkc); break;                                                \
            case GNu::eRECT_DRAW_FILL: GNg::ImageDraw(0, x + 2, y + sl->aLineWidth, w - 4, h - (sl->aLineWidth << 1), bkc); break;       \
            case GNu::eRECT_DRAW_WIRE: GNg::LineFrame2dDraw(x + 2, y + sl->aLineWidth, w - 4, h - (sl->aLineWidth << 1), 1, bkc); break; \
        }                                                                                                                                \
    }                                                                                                                                    \
    TEXTOUT->SetText(TEXT, 0, x, y, w, h); /*显示各可视选项的文本标签*/                                                                  \
    TEXTOUT->Show(sl->aAffectColor ? GNg_MultiplyColorRatio(sl->aTextColor, color) : sl->aTextColor)

#define GT_TMP_SCROLL_LIST_UPDATE(PROC, ADD_PROC)                 \
    if(RecvInputUiObject != this) GNg::ViewCur->mouseMX += 10000; \
    unsigned int ret = PROC;                                      \
    if(RecvInputUiObject != this) GNg::ViewCur->mouseMX -= 10000; \
    ADD_PROC                                                      \
    if(zState && !ret) UiActionCall(this, eUI_ACT_VANISH);        \
    zState = ret < 2 ? ret : 2;

#ifdef GN_USING_ADDITIONAL_LIB
/*********************************************** 写入G2D,GUI格式内容 *************************************************************/
//封包写对象内部数据的函数( 局部缓冲大小 , 对象类型 )
#define GT_TMP_PACK_DATA_SAVE(BSIZE, CLASS) \
    uint8 loc_buf[BSIZE];                   \
    if(!data_out) data_out = loc_buf;       \
    uint8 *wpos = data_out;                 \
    *wpos = 1;                              \
    ++wpos; /*版本*/                        \
    wpos = CLASS ::zDataSave(wpos, share_objs);

//封包读对象内部数据的函数( 对象类型 )
#define GT_TMP_PACK_DATA_LOAD(CLASS)   \
    uint8 *rpos = (uint8 *)data;       \
    uint8 version = *rpos;             \
    ++rpos;                            \
    if(version > 1) return 0; /*版本*/ \
    rpos = CLASS ::zDataLoad(rpos, share_objs);

//写入引用文本式样的索引号( 文本式样引用列表 , 当前引用的文本式样指针 )
#define GT_TMP_TEXTSTYLE_WRITE_INDEX(_LIST, pTEXT)             \
    {                                                          \
        if(pTEXT && pTEXT != &GN::RunRec.aFont) {              \
            pointer_id = _LIST.add_no_eq2(pTEXT);              \
        } else pointer_id = 0xffffffff; /*该值代表没引用指针*/ \
        memcpy(wpos, &pointer_id, 4);                          \
        wpos += 4;                                             \
    }
//读取索引号引用的文本式样( 文本式样引组 , 要读取的文本式样指针 )
#define GT_TMP_TEXTSTYLE_READ_INDEX(_ARRAY, pTEXT)                                   \
    {                                                                                \
        memcpy(&pointer_id, rpos, 4);                                                \
        rpos += 4;                                                                   \
        if(pointer_id == 0xffffffff) pTEXT = &GN::RunRec.aFont;                      \
        else if(pointer_id < (unsigned int)_ARRAY.Num()) pTEXT = _ARRAY[pointer_id]; \
        else pTEXT = &GN::RunRec.aFont; /* 索引越界，回退默认字体 */                 \
    }
//写入引用数据结构的索引号( 数据结构引用列表 , 当前引用的数据指针 )
#define GT_TMP_OBJECT_WRITE_INDEX(_LIST, pDATA)                \
    {                                                          \
        if(pDATA) {                                            \
            pointer_id = _LIST.add_no_eq2(pDATA);              \
        } else pointer_id = 0xffffffff; /*该值代表没引用指针*/ \
        memcpy(wpos, &pointer_id, 4);                          \
        wpos += 4;                                             \
    }
//写入引用数组的索引号( 数组引用列表 , 当前引用的数组指针 , 附加处理 )
#define GT_TMP_ARRAY_WRITE_INDEX(_LIST, ARRAY, ADD_PROC)       \
    {                                                          \
        if(ARRAY) {                                            \
            unsigned int eid = _LIST.size();                   \
            pointer_id = _LIST.add_no_eq2(ARRAY);              \
            if(pointer_id == eid) ADD_PROC;                    \
        } else pointer_id = 0xffffffff; /*该值代表没引用指针*/ \
        memcpy(wpos, &pointer_id, 4);                          \
        wpos += 4;                                             \
    }
#endif //GN_USING_ADDITIONAL_LIB
#endif //_GND_H_
