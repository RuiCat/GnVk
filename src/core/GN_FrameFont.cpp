// GN_FrameFont.cpp — cFRAME_LAUNCHER_FONT（文字动画图框发射器）。
#include "GN.h"
#include "GN_Def.h"
#include "GN_FrameInternal.h"
#ifdef _GN_FRAME_H_
/************************************************************** cFRAME_LAUNCHER_FONT *****************************************************************************************/
GNg::sDRAW_AREA *GNg::cFRAME_LAUNCHER_FONT::zDrawRangeInfo = 0;

char GNg::cFRAME_LAUNCHER_FONT::aPicIndex[128] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
    32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
    64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95,
    96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127};
#define GT_FRAME_LAUNCHER_FONT_RELEASE \
    /*GN_DeleteArray(zText_s);*/       \
    GN_DeleteArray(zActTime_s);        \
    GN_DeleteArray(zActState_s);       \
    zMax_num = 0

#define GT_FRAME_LAUNCHER_FONT_RELEASE_OBJ \
    if(zObjsKeep.Us() == 1) {              \
        GN_Delete(zFontFrame);             \
        GN_Delete(zActFrame);              \
    }
GNg::cFRAME_LAUNCHER_FONT::cFRAME_LAUNCHER_FONT() : zFontFrame(0),
                                                    zActFrame(0),
                                                    zActTime_s(0),
                                                    zActState_s(0),
                                                    zActMouseAuto(false) {
    zMax_num = 0;
}
GNg::cFRAME_LAUNCHER_FONT::~cFRAME_LAUNCHER_FONT() {
    GT_FRAME_LAUNCHER_FONT_RELEASE;
    GT_FRAME_LAUNCHER_FONT_RELEASE_OBJ
}
void GNg::cFRAME_LAUNCHER_FONT::zRelease() {
    GT_FRAME_LAUNCHER_FONT_RELEASE;
    cFRAME_LAUNCHER::zRelease();
    sDRAW_ARRANGE::Release();
}
void GNg::cFRAME_LAUNCHER_FONT::Release() {
    zRelease();
    GT_FRAME_LAUNCHER_FONT_RELEASE_OBJ
    cFRAME_LAUNCHER::Release();
}
void GNg::cFRAME_LAUNCHER_FONT::zCopyFontsData(const cFRAME_LAUNCHER_FONT &src, bool cpy_state) {
    if(cpy_state) zTrackingState = src.zTrackingState;
    else {
        zTrackingState = 0;
        zRunUpdate = src.zFontFrame ? true : false;
    }
    zFontFrame = src.zFontFrame;
    zParame = src.zParame;
    zFontMapFormat = src.zFontMapFormat;
    zTu_w = src.zTu_w;
    zTv_h = src.zTv_h;
    ztCU = src.ztCU;
    ztCV = src.ztCV;
    zIsFontMap = src.zIsFontMap;
    zGen_once_numB = src.zGen_once_numB;
    zIsGenEndStop = src.zIsGenEndStop;
    zSrcAmMode = src.zSrcAmMode;
    zUsScissor = src.zUsScissor;
    zActMouseAuto = src.zActMouseAuto;
    zActMouseKey = src.zActMouseKey;
    zActFrame = src.zActFrame;

    if(src.zDrawRngData) {
        zTotRngW = src.zTotRngW;
        zTotRngH = src.zTotRngH;
        zRowNum = src.zRowNum;
        memcpy(zRowSIDs, src.zRowSIDs, sizeof(zRowSIDs));

        zDrawRngData = new sDRAW_RANGE[zMax_num];
        memcpy(zDrawRngData, src.zDrawRngData, sizeof(sDRAW_RANGE) * zMax_num);
    } else zDrawRngData = 0;

    if(src.zActTime_s) {
        zActTime_s = new unsigned int[zMax_num];
        if(cpy_state) memcpy(zActTime_s, src.zActTime_s, zMax_num << 2);
        else memset(zActTime_s, 0, zMax_num << 2);
    } else zActTime_s = 0;

    if(src.zActState_s) {
        zActState_s = new unsigned char[zMax_num];
        if(cpy_state) memcpy(zActState_s, src.zActState_s, zMax_num);
        else memset(zActState_s, 0, zMax_num);
    } else zActState_s = 0;
}
GNg::cFRAME_LAUNCHER_FONT::cFRAME_LAUNCHER_FONT(const cFRAME_LAUNCHER_FONT &src) : cFRAME_LAUNCHER(src) {
    zCopyFontsData(src, false);
}
GNg::cFRAME_LAUNCHER_FONT &GNg::cFRAME_LAUNCHER_FONT::operator=(const cFRAME_LAUNCHER_FONT &src) {
    this->Release();
    this->cFRAME_LAUNCHER::operator=(src);
    zCopyFontsData(src, false);
    return *this;
}
void GNg::cFRAME_LAUNCHER_FONT::PerfectCopy(const cFRAME_LAUNCHER_FONT &src) {
    this->Release();
    cFRAME_LAUNCHER::PerfectCopy(src);
    zCopyFontsData(src, true);
}
void GNg::cFRAME_LAUNCHER_FONT::InitToData(cFRAME *_font_frame,
                                           char *_text, unsigned short _textlen,
                                           sRECT2 *_gen_range,
                                           sARRANGE_FMT *_parame,
                                           float _gen_angle, unsigned int _gen_speed, unsigned short _gen_once_num,
                                           int8 _pose_range, int8 _angle_range, int8 _size_range, int8 _color_range,
                                           unsigned int _activity_count,
                                           cFRAME *_frame, cFRAME *_frame2, cFRAME *act_chg_frame, bool is_pic) {
    //-- 初始化字筐绘制属性 --------------------------------------------------------------
    if(!_font_frame) return;
    zFontFrame = _font_frame;
    zParame = *_parame;
    if(zFontFrame->zPlay_obj) { //分类字体和重设置筐宽高等信息
        sIMAGE *fontmap = zFontFrame->zPlay_obj;
        int8 &rcs_wn = zFontFrame->zRcS_w_num, &rcs_hn = zFontFrame->zRcS_h_num;
        ztCU = zFontFrame->zRcLoc.left * (rcs_wn ? rcs_wn : 1) / (float)fontmap->width;
        ztCV = zFontFrame->zRcLoc.top * (rcs_hn ? rcs_hn : 1) / (float)fontmap->height;
        if(is_pic) {
            if(!zParame.width) zParame.width = fontmap->width; //使用默认宽度
            if(!zParame.height) zParame.height = fontmap->height; //使用默认高度
            zTu_w = zFontFrame->zRcS.right;
            zTv_h = zFontFrame->zRcS.bottom;
            zFontMapFormat = eFONT_NUMBER;
            zIsFontMap = false;
        } else {
            zClassifyFontMapFormat(fontmap);
            switch(zFontMapFormat) {
                case eFONT_NUMBER:
                    rcs_wn = 10;
                    rcs_hn = 1;
                    zIsFontMap = true;
                    break;
                case eFONT_ASCII:
                    rcs_wn = 16;
                    rcs_hn = 8;
                    zIsFontMap = true;
                    break;
                case eFONT_FREETYPE:
                    rcs_wn = 32;
                    rcs_hn = 18;
                    zIsFontMap = true;
                    break;
            }
        }
        zArrangeFmtEnsure();
        if(!zParame.i_compartment) zParame.i_compartment = 1;
        if(!zParame.j_compartment) zParame.j_compartment = 1;
        zFontFrame->zRcS.bottom = zTv_h;
    }
    //-- 初始化继承来的属性 --------------------------------------------------------------
    zFrame = _frame;
    zFrame2 = _frame2;
    SetRange(_gen_range);
    aGenColor = 0xffffffff;
    aGen_angle = _gen_angle;
    zGen_speed = _gen_speed ? _gen_speed : 1;
    zMax_gen_speed = zGen_speed;
    zGen_once_num = _gen_once_num;
    zMax_once_num = zGen_once_num;

    aPose_range = _pose_range;
    aAngle_range = _angle_range;
    aSize_range = _size_range;
    aColor_range = _color_range;
    zActivity_time = _activity_count;
    zRunUpdate = true;
    GT_TMP_LAUNCHER_RESET_STATE
    zMax_num = 0;
    zGen_once_numB = zGen_once_num;


    GT_TMP_LAUNCHER_FONT_RE_OTHER_STATE
    zEnd_time = zFontFrame->GetEndtime();
    zActFrame = act_chg_frame;
    SetText(_text, _textlen); //设置要显示的文字
}
void GNg::cFRAME_LAUNCHER_FONT::SetText(char *text, unsigned short _textlen, int offset_x, int offset_y, bool reset_am) {
    if(!zFontFrame) return;
    unsigned int last_maxnum;
    if(reset_am) {
        zRelease();
        if(zSrcAmMode) { //要重设了就还原上次经逆播修改的开始值和结束值
            if(zFontFrame->zFEvent) GT_AmEvsStartEndValueSwap(zFontFrame->zFEvent, zFontFrame->zEventcount);
            zFontFrame->zAm_mode = zSrcAmMode;
            zSrcAmMode = 0;
        }
    } else {
        if(zSrcAmMode) return;
        last_maxnum = zMax_num;
        zMax_num = 0;
    }
    int tx = (int)aGenRngX, ty = (int)aGenRngY;
    int tw = (int)aGenRngW, th = (int)aGenRngH;
    if(tw < 0) tw = 0;
    if(th < 0) th = 0;
    zUsScissor = (!zParame.rect_fill && tw && th);

    GNg::sDRAW_RANGE *pd_text = zDrawRngData;
    zDrawRngData = 0;
    int dw_rng_conut = 0;
    zMax_num = (unsigned int)GNg::ComputeArrangeRangeData(&zDrawRngData, dw_rng_conut, text, _textlen, tx, ty, tw, th, offset_x, offset_y, &zParame, zFontMapFormat, zTu_w, zTv_h, zRowSIDs, &zRowNum, &zTotRngW, &zTotRngH);

    if(zIsGenEndStop) zActivity_time = zMax_num;
    zObjNowtime = 0;
    zGen_num = 0;
    zGen_count = 0;
    zDisplay_num = 0;
    if(!zMax_num) return;

    if(!zIsFontMap) { //反向计算出图片索引号
        GNg::sDRAW_RANGE *_rngs = zDrawRngData;
        unsigned int dri = 0, cpn;
        if(zUsScissor) {
            for(; dri < zMax_num; ++dri) _rngs[dri].tid = ((unsigned short)(_rngs[dri].u1 / zTu_w + 48.0f));
        } else {
            unsigned short tid;
            for(; dri < zMax_num;) {
                tid = ((unsigned short)(_rngs[dri].u1 / zTu_w + 48.0f));
                if(tid == 0xff) { //排除掉换行符
                    cpn = zMax_num - (dri + 1);
                    if(cpn) memcpy(_rngs + dri, _rngs + dri + 1, sizeof(GNg::sDRAW_RANGE) * cpn);
                    --zMax_num;
                } else _rngs[dri++].tid = tid;
            }
            uint8 rni = 1; //行首ID也偏移一下
            for(; rni < zRowNum; ++rni) zRowSIDs[rni] -= rni - 1;
            if(!zMax_num) return;
        }
    }
    //修正筐范围位置 以弥补字中心点不在左上角的其他位置
#define GT_TMP_DRAW_RNG_OFS_CPT                                                                    \
    int ow = (int)(zTu_w / (zDrawRngData->u2 - zDrawRngData->u1) * (float)zDrawRngData->w * ztCU); \
    int oh = (int)((float)zDrawRngData->h * ztCV);

    GT_TMP_DRAW_RNG_OFS_CPT
    //aGenRngX += ow;
    //aGenRngY += oh;

    if(reset_am || !pd_text) {
        zMax_once_num = zGen_once_num = (zGen_once_numB ? zGen_once_numB : (unsigned short)zMax_num);

#define GT_TMP_NEW_FRAMES_MEM_SPACE               \
    zColor_s = new unsigned int[zMax_num];        \
    zNowtime_s = new unsigned int[zMax_num];      \
    zTransf_s = malloc(zTsUnitSize() * zMax_num); \
    sTRANSF2D *tss = (sTRANSF2D *)zTransf_s;

        GT_TMP_NEW_FRAMES_MEM_SPACE
        unsigned int i = 0;
        for(; i < zMax_num; ++i) {
            zDrawRngData[i].x -= ow;
            zDrawRngData[i].y -= oh;
            tss[i].x = (float)zDrawRngData[i].x;
            tss[i].y = (float)zDrawRngData[i].y;
            tss[i].sx = 1.0f;
            tss[i].sy = 1.0f;
            tss[i].r = 0.0f;
        }
        const unsigned int dsize = zMax_num << 2;
        memset(zColor_s, 0xff, dsize);
        memset(zNowtime_s, 0xff, dsize);
        if(zActFrame) {
            zActTime_s = new unsigned int[zMax_num];
            memset(zActTime_s, 0, dsize);
            zActState_s = new uint8[zMax_num];
            memset(zActState_s, 0, zMax_num);
        }
    } else {
        zMax_once_num = zGen_once_num = (unsigned short)zMax_num;

        unsigned short str_start = pd_text->strid,
                       str_end = pd_text[last_maxnum - 1].strid;
        const unsigned short str_range = str_end - str_start + 1; //计算出上次排好的字筐最大内部索引值的容纳个数
        unsigned int *last_str_idxs = new unsigned int[str_range];
        memset(last_str_idxs, 0xff, str_range << 2); //值为0xffffffff表示为空的上次未显示的
        unsigned int i = 0;
        for(; i < last_maxnum; ++i) last_str_idxs[pd_text[i].strid - str_start] = i; //按内部字符号索引记录摆放号

        GNg::sTRANSF2D *pdata_ts = (sTRANSF2D *)zTransf_s;
        unsigned int *pdata_color = zColor_s;
        unsigned int *pdata_time = zNowtime_s, last_arng_id;
        GT_TMP_NEW_FRAMES_MEM_SPACE
        if(zActFrame) {
            unsigned int *pd_act_time = zActTime_s;
            uint8 *pd_act_state = zActState_s;
            zActTime_s = new unsigned int[zMax_num];
            zActState_s = new uint8[zMax_num];

#define GT_TMP_FONTFRAME_SET_STATE_BEGIN                                                             \
    for(i = 0; i < zMax_num; ++i) {                                                                  \
        zDrawRngData[i].x -= ow;                                                                     \
        zDrawRngData[i].y -= oh;                                                                     \
        tss[i].x = (float)zDrawRngData[i].x;                                                         \
        tss[i].y = (float)zDrawRngData[i].y;                                                         \
                                                                                                     \
        unsigned short str_cur = zDrawRngData[i].strid;                                              \
        if(str_cur < str_start || str_cur > str_end) last_arng_id = 0xffffffff; /*标记为上次未显示*/ \
        else last_arng_id = last_str_idxs[str_cur - str_start];                                      \
        if(last_arng_id == 0xffffffff) { /*未显示的就重设它*/                                        \
            tss[i].sx = 1.0f;                                                                        \
            tss[i].sy = 1.0f;                                                                        \
            tss[i].r = 0.0f;                                                                         \
            zColor_s[i] = 0xffffffff;                                                                \
            zNowtime_s[i] = 0xffffffff;

#define GT_TMP_FONTFRAME_SET_STATE_END                                          \
    tss[i].sx = pdata_ts[last_arng_id].sx; /*把之前有显示的旧数据赋予给新位置*/ \
    tss[i].sy = pdata_ts[last_arng_id].sy;                                      \
    tss[i].r = pdata_ts[last_arng_id].r;                                        \
    zColor_s[i] = pdata_color[last_arng_id];                                    \
    zNowtime_s[i] = pdata_time[last_arng_id];                                   \
    }                                                                           \
    }
            GT_TMP_FONTFRAME_SET_STATE_BEGIN
            zActTime_s[i] = 0;
            zActState_s[i] = 0;
        } else {
            zActTime_s[i] = pd_act_time[last_arng_id];
            zActState_s[i] = pd_act_state[last_arng_id];
            GT_TMP_FONTFRAME_SET_STATE_END
            delete[] pd_act_time;
            delete[] pd_act_state;
        }
        else {
            GT_TMP_FONTFRAME_SET_STATE_BEGIN
        }
        else {
            GT_TMP_FONTFRAME_SET_STATE_END} free(pdata_ts); //上次调用SetText生成的数据无用了就删除
        delete[] pdata_color;
        delete[] pdata_time;
        delete[] pd_text;
        delete[] last_str_idxs;
    }
#ifdef _GN_SOUND_H_
    if(zFontFrame->zSouObj->z__attrb) zFontFrame->InitSoundToData(zFontFrame->zSouObj, zMax_num);
#endif
}
void GNg::cFRAME_LAUNCHER_FONT::SetSubText(unsigned int id, char *text) {
    if(!text || !(*text)) return;
    char t_col, t_row;
    float tu, tv;
    unsigned int ti = 0, id_ = id % zMax_num;
    switch(zFontMapFormat) {
        case eFONT_ASCII:
            while(text[ti]) {
                if(text[ti] == '\n') {
                    ++ti;
                    continue;
                }
                t_col = text[ti] >> 4;
                t_row = text[ti++] & 0x0f; /*ASCII编码方式*/
                tu = (float)t_row * zTu_w;
                tv = (float)t_col * zTv_h;
                if(id_ >= zMax_num) id_ -= zMax_num;
                zDrawRngData[id_].u1 = tu;
                zDrawRngData[id_].v1 = tv;
                zDrawRngData[id_].u2 = tu + zTu_w;
                zDrawRngData[id_].v2 = tv + zTv_h;
                zDrawRngData[id_].tid = 0;
                zNowtime_s[id_++] = 0;
            }
            break;
        case eFONT_FREETYPE:
            unsigned short texid;
            char t_col2;
            while(text[ti]) {
                if(text[ti] == '\n') {
                    ++ti;
                    continue;
                }
                if(id_ >= zMax_num) id_ -= zMax_num;
                if(text[ti] & 128) { //汉字
                    t_col = text[ti] - 0xa1;
                    texid = t_col / 3;
                    t_col2 = (t_col - texid * 3) * 6;
                    t_col = (text[ti + 1] - 0xa0);
                    t_row = t_col & 0x0f; //GB2312编码方式
                    t_col >>= 4;
                    t_col = t_col + t_col2;
                    tu = (float)t_row * zTu_w;
                    tv = (float)t_col * zTv_h;
                    zDrawRngData[id_].u2 = tu + zTu_w;
                    if(zDrawRngData[id_].tid == 29) zDrawRngData[id_].w <<= 1;
                    zDrawRngData[id_].tid = texid;
                    ++ti;
                } else {
                    t_col = text[ti] >> 4;
                    t_row = text[ti] & 0x0f; //汉字中的ASCII编码
                    tu = (float)t_row * zTu_w;
                    tv = (float)t_col * zTv_h;
                    zDrawRngData[id_].u2 = tu + (zTu_w * 0.5f);
                    if(zDrawRngData[id_].tid != 29) zDrawRngData[id_].w >>= 1;
                    zDrawRngData[id_].tid = 29;
                }
                zDrawRngData[id_].u1 = tu;
                zDrawRngData[id_].v1 = tv;
                zDrawRngData[id_].v2 = tv + zTv_h;
                zNowtime_s[id_++] = 0;
                ++ti;
            }
            break;
        case eFONT_NUMBER:
            while(text[ti]) {
                if(id_ >= zMax_num) id_ -= zMax_num;
                tu = (float)((text[ti++] - '0') * zTu_w); /*斋数字编码方式*/
                zDrawRngData[id_].u1 = tu;
                zDrawRngData[id_].v1 = 0.0f;
                zDrawRngData[id_].u2 = tu + zTu_w;
                zDrawRngData[id_].v2 = zTv_h;
                zDrawRngData[id_].tid = 0;
                zNowtime_s[id_++] = 0;
            }
            break;
    }
}
void GNg::cFRAME_LAUNCHER_FONT::SelectActChange(unsigned int id) {
    if(!zActFrame) return;
    zActState_s[id % zMax_num] = 1;
}
void GNg::cFRAME_LAUNCHER_FONT::CancelActChange(unsigned int id) {
    if(zActFrame) {
        id = id % zMax_num;
        if(zActState_s[id] == 1) {
            if(zActFrame->zAm_mode == eAM_LOOP || zActFrame->zAm_mode == eAM_REPEAT) {
                zActFrame->SetRuntime(zActTime_s + id);
                zActFrame->zAmTimeProce(0);
                zActTime_s[id] = zActFrame->zNowtime;
            } else if(zActTime_s[id] > zActFrame->zEndtime) zActTime_s[id] = zActFrame->zEndtime;
            zActState_s[id] = 2;
        }
    }
}
bool GNg::cFRAME_LAUNCHER_FONT::zToActFramePrco(unsigned int id, unsigned int updatetime) {
    if(zActState_s[id]) {
        zActFrame->SetRuntime(zActTime_s + id);
        if(zActFrame->zAmTimeProce(0)) return true;
        zActFrame->zEventProce(0);
        MultTransf2D(&(zActFrame->aLevel_ts), &(zActFrame->zFrame_ts), &(zActFrame->aFrame_ts));
        MultTransf2D(&(zFontFrame->aFrame_ts), &(zActFrame->aLevel_ts), ((sTRANSF2D *)zTransf_s) + id);
        zActFrame->aLevel_color = MultiplyColorRatio(zActFrame->zColor, zActFrame->aColor);
        zFontFrame->aColor = MultiplyColorRatio(zActFrame->aLevel_color, zColor_s[id]);
        if(zActState_s[id] == 1) {
            zActTime_s[id] += updatetime;
            if(zActTime_s[id] > zActFrame->zEndtime) {
                if(zActFrame->zAm_mode == eAM_ONCE_KEEP) zActTime_s[id] = zActFrame->zEndtime;
                if(zActMouseAuto) CancelActChange(id);
            }
        } else {
            zActTime_s[id] -= updatetime;
            if(zActTime_s[id] > 0xffff0000) {
                zActTime_s[id] = 0;
                zActState_s[id] = 0;
            }
        }
    } else {
        zFontFrame->aFrame_ts = ((sTRANSF2D *)zTransf_s)[id];
        zFontFrame->aColor = zColor_s[id];
    }
    return false;
}
void GNg::cFRAME_LAUNCHER_FONT::zPlayFontFrame(unsigned int id) {
#define GT_TMP_PLAY_FONT_FRAME                                                                  \
    sDRAW_RANGE *_draw_rng = zDrawRngData + id;                                                 \
    zFontFrame->zTexID = _draw_rng->tid; /*设置纹理ID*/                                         \
    if(zIsFontMap) {                                                                            \
        zFontFrame->zRcS0.left = _draw_rng->u1; /*设置纹理坐标*/                                \
        zFontFrame->zRcS0.top = _draw_rng->v1;                                                  \
        zFontFrame->zRcS.right = _draw_rng->u2 - _draw_rng->u1; /*设置纹理取格宽度 汉字占两格*/ \
    }                                                                                           \
    zFontFrame->zRcLoc.left = (float)_draw_rng->w * ztCU;                                       \
    zFontFrame->zRcLoc.right = (float)_draw_rng->w;                                             \
    zFontFrame->zRcLoc.top = (float)_draw_rng->h * ztCV;                                        \
    zFontFrame->zRcLoc.bottom = (float)_draw_rng->h;                                            \
                                                                                                \
    zFontFrame->SetRuntime(zNowtime_s + id);                                                    \
    zFontFrame->PlayFrame(0);

    GT_TMP_PLAY_FONT_FRAME
}
void GNg::cFRAME_LAUNCHER_FONT::zPlayFontFrameAndRangeCollect(unsigned int id) {
    GT_TMP_PLAY_FONT_FRAME

    if(zTrackingState == 1) { //点选跟踪检测
        if(TestPointIntQuads(zPointX, zPointY,
                             zFontFrame->aLevel_range.left, zFontFrame->aLevel_range.top,
                             zFontFrame->aLevel_ts.x, zFontFrame->aLevel_ts.y,
                             zFontFrame->aLevel_range.right, zFontFrame->aLevel_range.bottom)) {
            zSelectedId = id;
            zTrackingState = 6;
        }
    }
    if(zDrawRangeInfo) { //需要范围记录
        sDRAW_AREA *draw_info = zDrawRangeInfo + id;
        draw_info->x = (int)zFontFrame->aLevel_range.left;
        draw_info->y = (int)zFontFrame->aLevel_range.top;
        draw_info->w = (int)zFontFrame->aLevel_range.right - draw_info->x;
        draw_info->h = (int)zFontFrame->aLevel_range.bottom - draw_info->y;
        draw_info->c = zFontFrame->aLevel_color;
    }
}
unsigned int GNg::cFRAME_LAUNCHER_FONT::GetEndtime() {
    unsigned int gen_count = zActivity_time ? zActivity_time : zMax_num;
    unsigned int gen_round = gen_count / zGen_once_num;
    if(gen_round * zGen_once_num < gen_count) ++gen_round;
    return gen_round * zGen_speed + zEnd_time;
}
unsigned int GNg::cFRAME_LAUNCHER_FONT::PlayFrame(unsigned int updatetime) {
    if(zMax_num && zFontFrame) {
        if(!zRunUpdate) updatetime = 0; //暂停
        else zObjNowtime += updatetime;
        bool is_am_hide = (zFontFrame->zAm_mode == eAM_ONCE_HIDE);

        if(zActivity_time && zGen_count + zGen_once_num > zActivity_time) {
            if(is_am_hide && zObjNowtime > zGen_speed * zGen_num + zEnd_time) return 0; //播放已经结束
        } else {
            if(zObjNowtime > zGen_speed * zGen_num) { //到了生成时间就生成筐
                unsigned int id1;
                unsigned short gen_num = 0;
                for(; gen_num < zGen_once_num; ++gen_num) {
                    id1 = (zGen_count++) % zMax_num;
                    if(zNowtime_s[id1] == 0xffffffff) zCreateUnit(id1); //生成筐
                }
                ++zGen_num; //生成次数加1

                GT_TMP_FRAME_ZT_BRIGHT_UPDATE
            }
        }
        if(zActFrame && zActMouseAuto) { //当使用了鼠标自动动态演变
            if(zActMouseKey < 0 || GN::Mouse[zActMouseKey]) {
                if(zTrackingState > 2) zActState_s[zSelectedId] = 1;
                zTrackingState = 1; //1通知PlayFrame要顺便查找
                zPointX = ViewCur->mouseMX; //设定检索点为鼠标移动位置
                zPointY = ViewCur->mouseMY;
            }
        } //------------------------------------------------------------------------------------------------
        if(zUsScissor) ViewCur->ScissorRectSetup((int)aGenRngX, (int)aGenRngY, (int)aGenRngW, (int)aGenRngH);
        zDisplay_num = 0;
        sTRANSF2D *tss = (sTRANSF2D *)zTransf_s;
        bool need_track = (zTrackingState == 1);
        void (cFRAME_LAUNCHER_FONT::*pf_play_font_frame)(unsigned int) = (need_track || zDrawRangeInfo) ? &cFRAME_LAUNCHER_FONT::zPlayFontFrameAndRangeCollect : &cFRAME_LAUNCHER_FONT::zPlayFontFrame;
        unsigned int time_value = is_am_hide ? zEnd_time : 0xffffffff, id = 0;
        if(zFrame) {
            if(zActFrame) { //有附加表面效果和动态演变 ---------
                for(; id < zMax_num; ++id) {
                    if(zNowtime_s[id] >= time_value) continue;
                    if(zToActFramePrco(id, updatetime)) continue;
                    zFontFrame->zTargetID = id;
                    zFrame->aFrame_ts = zFontFrame->aFrame_ts;
                    (this->*pf_play_font_frame)(id);
                    zFrame->SetRuntime(zNowtime_s + id);
                    zFrame->PlayFrame(0);
                    zNowtime_s[id] += updatetime;
                    ++zDisplay_num;
                }
            } else { //有附加表面效果 -------------------
                for(; id < zMax_num; ++id) {
                    if(zNowtime_s[id] >= time_value) continue;
                    zFontFrame->aFrame_ts = tss[id];
                    zFontFrame->aColor = zColor_s[id];
                    zFontFrame->zTargetID = id;
                    zFrame->aFrame_ts = tss[id];
                    (this->*pf_play_font_frame)(id);
                    zFrame->SetRuntime(zNowtime_s + id);
                    zFrame->PlayFrame(0);
                    zNowtime_s[id] += updatetime;
                    ++zDisplay_num;
                }
            }
        } else { //没有附加表面效果和有动态演变------
            if(zActFrame) {
                for(; id < zMax_num; ++id) {
                    if(zNowtime_s[id] >= time_value) continue;
                    if(zToActFramePrco(id, updatetime)) continue;
                    zFontFrame->zTargetID = id;
                    (this->*pf_play_font_frame)(id);
                    zNowtime_s[id] += updatetime;
                    ++zDisplay_num;
                }
            } else { //没有附加表面效果------------------
                for(; id < zMax_num; ++id) {
                    if(zNowtime_s[id] >= time_value) continue;
                    zFontFrame->aFrame_ts = tss[id];
                    zFontFrame->aColor = zColor_s[id];
                    zFontFrame->zTargetID = id;
                    (this->*pf_play_font_frame)(id);
                    zNowtime_s[id] += updatetime;
                    ++zDisplay_num;
                }
            }
        }
        if(need_track) {
            if(zTrackingState == 1) zTrackingState = 0;
        } else if(zTrackingState > 2) --zTrackingState;

        if(zFontFrame->zAm_mode == eAM_ONCE_KEEP) {
            for(id = 0; id < zMax_num; ++id) { //使超出时间的保持在结尾 以防止设置了播放一次停住受到其他动画变换影响
                if(zNowtime_s[id] < 0xffffffff && zNowtime_s[id] > zEnd_time) zNowtime_s[id] = zEnd_time;
            }
        }
        if(zUsScissor) ViewCur->ScissorRectRemove(); //-----------------------------------------------------
    } // endif zFontFrame
    if(zFrame2) zUpdateFrame2(updatetime); //播放附加的效果
    return zObjNowtime;
}
unsigned int GNg::cFRAME_LAUNCHER_FONT::PlayFrame(unsigned int updatetime, unsigned int dc) {
    if(dc == 0xffffffff) return PlayFrame(updatetime);
    /*else if(zFontFrame->zAm_mode && zFontFrame->zColor == 0xffffffff){ //有动画与无颜色变化动画时
		zFontFrame->zColor = dc; //直接使内部色调等于父层颜色
		return PlayFrame(updatetime);
	}*/
    else {
        if(aColor_range == 127 && aGenColor == 0xffffffff) { //直接用父层颜色绘制
            unsigned int id = 0;
            for(; id < zMax_num; ++id) zColor_s[id] = dc;
            return PlayFrame(updatetime);
        } else { //处理相互颜色影响
            uint8 buf[1024];
            GN::sTBUF_INT tbuf(buf, 1024);
            unsigned int *dcs = zColor_s;
            zColor_s = (unsigned int *)tbuf.GetSpace(zMax_num << 2);
            unsigned int id = 0;
            for(; id < zMax_num; ++id) zColor_s[id] = GNg::MultiplyColorRatio(dcs[id], dc);
            unsigned int ret = PlayFrame(updatetime);
            zColor_s = dcs;
            return ret;
        }
    }
}
void GNg::cFRAME_LAUNCHER_FONT::ReSetPlaytime() {
    zResetState();
    zGen_num = 0;
    zGen_count = 0;
    sTRANSF2D *tss = (sTRANSF2D *)zTransf_s;
    unsigned int i = 0;
    for(; i < zMax_num; ++i) {
        tss[i].sx = 1.0f;
        tss[i].sy = 1.0f;
        tss[i].r = 0.0f;
    }
    memset(zColor_s, 0xff, zMax_num << 2);
    memset(zNowtime_s, 0xff, zMax_num << 2);
}
void GNg::cFRAME_LAUNCHER_FONT::Move(int x, int y) {
    aGenRngX += (float)x;
    aGenRngY += (float)y;
    sTRANSF2D *tss = (sTRANSF2D *)zTransf_s;
    unsigned int i = 0;
    for(; i < zMax_num; ++i) {
        tss[i].x += (float)x;
        tss[i].y += (float)y;
        zDrawRngData[i].x += x;
        zDrawRngData[i].y += y;
    }
}
void GNg::cFRAME_LAUNCHER_FONT::ToHide() {
    if(zSrcAmMode || !zFontFrame || !zFontFrame->zFEvent || !zFontFrame->zAm_mode) return;
    unsigned int stt = 0, evi = 0, evn = zFontFrame->zEventcount;
    sAM_EVENT *evs = zFontFrame->zFEvent;
    for(; evi < evn; ++evi) {
        if(evs[evi].start_time > stt) stt = evs[evi].start_time;
    }
    GT_AmEvsStartEndValueSwap(evs, evn);
    unsigned int id = 0;
    for(; id < zMax_num; ++id) //还未出现的就标记为已经播放结束 否则 取之前的播放剩余时间
        zNowtime_s[id] = (zNowtime_s[id] == 0xffffffff ? zEnd_time : zEnd_time - zNowtime_s[id] + stt);

    zGen_count = zActivity_time;
    zObjNowtime = zGen_speed * zGen_num;
    zSrcAmMode = zFontFrame->zAm_mode;
    zFontFrame->zAm_mode = GNg::eAM_ONCE_HIDE;
}
void GNg::cFRAME_LAUNCHER_FONT::StopGenerate() {
    if(zActivity_time) {
        if(zActivity_time > zGen_count) zActivity_time = zGen_count;
    } else zActivity_time = zGen_count;
}
void GNg::cFRAME_LAUNCHER_FONT::ContinueGenerate(unsigned int count) {
    if(zActivity_time) {
        if(count) zActivity_time += count;
        else zActivity_time = 0;
        if(zObjNowtime > zGen_num * zGen_speed) zObjNowtime = zGen_num * zGen_speed;
    }
}
void GNg::cFRAME_LAUNCHER_FONT::GetTextRange(sRECT *_range_out, bool is_stark) {
#ifdef GN_DEBUG
    if(!_range_out || !zDrawRngData) {
        GN_Log("调用 cFRAME_LAUNCHER_FONT::GetTextRange 参数不能为空的!\n");
        return;
    }
#endif
    if(!is_stark) {
        _range_out->left = 0;
        _range_out->top = 0;
        _range_out->right = zTotRngW;
        _range_out->bottom = zTotRngH;
        return;
    }
    sDRAW_ARRANGE::GetTextRange(_range_out, zMax_num, ztCU, ztCV);
}
void GNg::cFRAME_LAUNCHER_FONT::GetTextRange(sRECT2 *_range_out, bool is_stark) {
    sRECT rect;
    GetTextRange(&rect, is_stark);
    _range_out->left = (float)rect.left;
    _range_out->top = (float)rect.top;
    _range_out->right = (float)rect.right;
    _range_out->bottom = (float)rect.bottom;
}
bool GNg::cFRAME_LAUNCHER_FONT::GetFontFrameRange(unsigned int id, sQUADS2 *out) {
    if(!zFontFrame || !zMax_num || !out) return false;
    id = id % zMax_num;
    bool ret = true;
    if(zNowtime_s[id] >= ((zFontFrame->zAm_mode == eAM_ONCE_HIDE) ? zEnd_time : 0xffffffff)) ret = false;
    if(zActFrame) zToActFramePrco(id, 0);
    else zFontFrame->aFrame_ts = ((sTRANSF2D *)zTransf_s)[id];
    zFontFrame->aColor = 0x00ffffff;
    zPlayFontFrame(id);
    out->p0.x = zFontFrame->aLevel_range.left;
    out->p0.y = zFontFrame->aLevel_range.top;
    out->p1.x = zFontFrame->aLevel_ts.x;
    out->p1.y = zFontFrame->aLevel_ts.y;
    out->p2.x = zFontFrame->aLevel_range.right;
    out->p2.y = zFontFrame->aLevel_range.bottom;
    return ret;
}
void GNg::cFRAME_LAUNCHER_FONT::SetSelectPoint(int x, int y) {
    zPointX = (float)x;
    zPointY = (float)y;
    zTrackingState = 1; // 1通知PlayFrame需要顺便查找
}
bool GNg::cFRAME_LAUNCHER_FONT::GetSelectFontFrame(unsigned int *id_out) {
    if(id_out && zTrackingState > 2) {
        *id_out = zSelectedId;
        if(!zActMouseAuto) zTrackingState = 0;
        return true;
    }
    return false;
}
unsigned int *GNg::cFRAME_LAUNCHER_FONT::GetFontFrameID_Data(const sRECT *rect, unsigned int *id_num_out) {
#ifdef GN_DEBUG
    if(!rect || !id_num_out) {
        GN_Log("调用 GNg::cFRAME_LAUNCHER_FONT::GetFontFrameID_Data 参数不能为空的!\n");
        return 0;
    }
#endif
    if(!zMax_num) {
        *id_num_out = 0;
        return 0;
    }
    unsigned int *id_out = new unsigned int[zMax_num];
    GT_TMP_DRAW_RNG_OFS_CPT
    sRECT rc2;
    unsigned int id = 0, n = 0;
    for(; id < zDisplay_num; ++id) {
        rc2.left = zDrawRngData[id].x + ow;
        rc2.top = zDrawRngData[id].y + oh;
        rc2.right = zDrawRngData[id].w + rc2.left;
        rc2.bottom = zDrawRngData[id].h + rc2.top;

        if(TestRectIntRect(rect, &rc2)) { id_out[n++] = id; }
    }
    *id_num_out = n;
    if(!n) {
        delete[] id_out;
        id_out = 0;
    }
    return id_out;
}
void GNg::cFRAME_LAUNCHER_FONT::ControlActFromMouseAuto(bool yes, short onDown) {
    zActMouseAuto = yes;
    zActMouseKey = onDown > 4 ? 4 : (int8)onDown; //防止超出鼠标键范围
}
#endif //_GN_FRAME_H_
