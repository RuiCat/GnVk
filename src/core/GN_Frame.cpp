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
#ifdef _GN_FRAME_H_
/************************************************* sAM_EVENT ************************************************************/
GNg::sAM_EVENT::sAM_EVENT(unsigned short _event_mode, unsigned int _start_time, unsigned int _end_time, float _start_value, float _end_value, float _slinear_value, unsigned short _slinear_mode) {
#define GT_TMP_AM_EVENT_INIT                                              \
    event_mode = _event_mode;                                             \
    start_time = _start_time;                                             \
    segment_time = _end_time > _start_time ? _end_time - _start_time : 0; \
    start_value = _start_value;                                           \
    end_value = _end_value;                                               \
    slinear_value = _slinear_value;                                       \
    slinear_mode = _slinear_mode;

    GT_TMP_AM_EVENT_INIT
}
void GNg::sAM_EVENT::Init(unsigned short _event_mode, unsigned int _start_time, unsigned int _end_time, float _start_value, float _end_value, float _slinear_value, unsigned short _slinear_mode) {
    GT_TMP_AM_EVENT_INIT
}
void GNg::sAM_EVENT::InitColor(unsigned int _start_time, unsigned int _end_time, unsigned int _start_value, unsigned int _end_value, float _slinear_value, unsigned short _slinear_mode) {
    event_mode = eEVENT_COLOR;
    start_time = _start_time;
    segment_time = _end_time > _start_time ? _end_time - _start_time : 0;
    start_value = *((float *)(&_start_value));
    end_value = *((float *)(&_end_value));
    slinear_value = _slinear_value;
    slinear_mode = _slinear_mode;
}
/******************************************  类cFRAME_RC  **********************************************/
#define GT_LEVEL_RANGE_CPT                                          \
    aLevel_range.left = zRcLoc.left * aLevel_ts.sx + aLevel_ts.x;   \
    aLevel_range.right = zRcLoc.right * aLevel_ts.sx + aLevel_ts.x; \
    aLevel_range.top = zRcLoc.top * aLevel_ts.sy + aLevel_ts.y;     \
    aLevel_range.bottom = zRcLoc.bottom * aLevel_ts.sy + aLevel_ts.y;

#define GT_TMP_EVENTS_ENDTIME_CPT(FM)                                        \
    FM zEndtime = 0;                                                         \
    unsigned int ev_endtime;                                                 \
    GNg::sAM_EVENT *_ev = FM zFEvent, *ev_end = FM zFEvent + FM zEventcount; \
    for(; _ev < ev_end; ++_ev) {                                             \
        ev_endtime = _ev->start_time + _ev->segment_time;                    \
        if(ev_endtime > FM zEndtime) FM zEndtime = ev_endtime;               \
    }
GNg::cFRAME_RC::cFRAME_RC() : aFrame_ts(Ts2D), zFrame_ts(Ts2D),
                              zPlay_obj(&Image),
                              zRcLoc(0.0f, 0.0f, 128.0f, 128.0f),
                              //截图范围
                              zRcS(0.0f, 0.0f, 1.0f, 1.0f), //默认截取完整图象
                              zRcS0(0.0f, 0.0f, 0.0f, 0.0f),
                              zRcS_w_num(0),
                              zRcS_h_num(0),
                              //	aColor(0xffffffff),
                              //  zColor(0xffffffff),
                              aBlendmode(1),
                              //	zTexID(0),

                              zFEvent(0),
                              zEventcount(0),
                              zSubFrame(0),
                              zSubCount(0),

                              zTexIndex(0),
                              zTexIndexCount(0),
                              zTexOnceTime(33),
#ifdef _GN_SOUND_H_
                              zSouObj(&GNa::Sound),
                              zSouEndTime(0),
                              zSouStartTime(0),
                              zTargetID(0),
#endif
                              zFrameTsReItem(31),
                              aLevel_color(0xffffffff),
                              aLevel_ts(Ts2D) {
    aLevel_range = zRcLoc;
}
GNg::cFRAME_RC &GNg::cFRAME_RC::operator=(const cFRAME_RC &src) {
// NOTE: memcpy(this,...) on non-trivial class is technically UB but safe in practice
// because (1) same-type copy preserves vtable pointer, (2) pointer members are
// immediately fixed up after copy, (3) class has no virtual base classes.
// TODO: refactor to field-by-field assignment for C++ compliance.
#define GT_TMP_FRAME_RC_COPY_DATA                                         \
    this->Release();                                                      \
    zObjsKeep.Delete();                                                   \
    zNodes.Delete();                                                      \
    {                                                                     \
        char *dst = (char *)this;                                         \
        const char *s = (const char *)&src;                               \
        for(size_t _i = 0; _i < sizeof(cFRAME_RC); ++_i) dst[_i] = s[_i]; \
    }                                                                     \
    zNodes.zData = 0;                                                     \
    zObjsKeep.zData = 0;

    GT_TMP_FRAME_RC_COPY_DATA
    zNodes = src.zNodes;
    zObjsKeep = src.zObjsKeep;
    return *this;
}
void GNg::cFRAME_RC::CopyData(const cFRAME_RC &src) {
    GT_TMP_FRAME_RC_COPY_DATA
    /*if(src.zObjsKeep)*/ zObjsKeep.New(1);
    zNodes.Copy(src.zNodes);
    if(zFEvent) {
        zFEvent = new sAM_EVENT[zEventcount];
        memcpy(zFEvent, src.zFEvent, sizeof(sAM_EVENT) * zEventcount);
    }
    if(zTexIndex) {
        zTexIndex = new uint8[zTexIndexCount];
        memcpy(zTexIndex, src.zTexIndex, zTexIndexCount);
    }
    if(zPlay_obj != &Image) {
        P<sIMAGE> img = zPlay_obj->CopyOut(0, 0, 0, 0, false, true);
        zPlay_obj = new sIMAGE;
        *zPlay_obj = *img;
        zPlay_obj->cloneImg = false;
        img->cloneImg = true;
    }
    if(zSubFrame) {
        unsigned short si = 0;
        if(sizeof(cFRAME_RC) == zSubFrame->GetClassSize()) {
            zSubFrame = new cFRAME_RC[zSubCount];
            for(; si < zSubCount; ++si) zSubFrame[si].CopyData(src.zSubFrame[si]);
        } else {
            cFRAME *sfam = new cFRAME[zSubCount], *sfam2 = (cFRAME *)(src.zSubFrame);
            for(; si < zSubCount; ++si) sfam[si].CopyData(sfam2[si]);
            zSubFrame = sfam;
        }
    }
#ifdef _GN_SOUND_H_
    zSouObj = &GNa::Sound;
    GNa::cSOUND_SRC ssrc;
    memcpy(&zSound, &ssrc, sizeof(GNa::cSOUND_SRC));
#endif
}
void GNg::cFRAME_RC::Release() {
    if(zObjsKeep.Us() == 1) {
        if(zPlay_obj != &Image) {
            delete zPlay_obj;
            zPlay_obj = &Image;
        }
        GN_DeleteArray(zFEvent);
        GN_DeleteArray(zTexIndex);
        if(zSubFrame) {
            if(sizeof(cFRAME_RC) == zSubFrame->GetClassSize()) delete[] zSubFrame;
            else delete[](cFRAME *)zSubFrame;
            zSubFrame = 0;
        }
#ifdef _GN_SOUND_H_
        if(zSouObj != &GNa::Sound) {
            delete zSouObj;
            zSouObj = &GNa::Sound;
        }
#endif
        zObjsKeep.Delete();
    }
}
inline void GT_ResetTransf2D(GNg::sTRANSF2D &ts, uint8 refg) {
    if(refg == 31) ts = GNg::Ts2D;
    else {
        if(refg & 1) ts.x = 0.0f;
        if(refg & 2) ts.y = 0.0f;
        if(refg & 4) ts.sx = 1.0f;
        if(refg & 8) ts.sx = 1.0f;
        if(refg & 16) ts.r = 0.0f;
    }
}
void GNg::cFRAME_RC::Init(sIMAGE *_play_obj, unsigned int _eventcount, sAM_EVENT *_fevent, sTRANSF2D _ts, float centre_pointU, float centre_pointV, unsigned short _blendmode, unsigned int _color, unsigned short _am_mode, int8 _rcS_w_num, int8 _rcS_h_num) {
    zFEvent = _fevent;
    zEventcount = _eventcount;
    zRuntime = 0;
    zNowtime = 0;
    zStarttime = 0;
    GT_TMP_EVENTS_ENDTIME_CPT() /*统计动画结束时间*/
    zFrame_ts = Ts2D;
    aFrame_ts = _ts;
#ifdef GT_TMP_COMPATI_DX_DDRAW
    float tex_crd_warp = 0.0f;
    char *rd_info = GN_GetInterfaceInfoRender();
    if(!strncmp(rd_info, "Direct3", 7)) tex_crd_warp = 1.0f / (float)zPlay_obj->width; //修正Direct3D的纹理坐标的一些偏差
#else
    const float tex_crd_warp = 0.0f;
#endif
    if(_play_obj != (sIMAGE *)1) {
        float wr = 1.0f, hr = 1.0f;
        if(sizeof(GNg::cFRAME_RC) == GetClassSize()) {
            wr = 1.0f - centre_pointU;
            hr = 1.0f - centre_pointV;
        }
        if(_play_obj) zPlay_obj = _play_obj;
        else zPlay_obj = &Image;
        zRcS_w_num = _rcS_w_num;
        zRcS_h_num = _rcS_h_num;

        //计算放大旋转定点的位置和截图范围
        if(zRcS_w_num > 0) {
            float rwn_inv = 1.0f / (float)zRcS_w_num;
            zRcLoc.left = (float)zPlay_obj->width * (-centre_pointU) * rwn_inv;
            zRcLoc.right = (float)zPlay_obj->width * wr * rwn_inv;
            zRcS.right = (zPlay_obj->uRange - tex_crd_warp) * rwn_inv; //计算到每格所占纹理的宽度
        } else if(zRcS_w_num < 0) {
            float ratiow = (float)(-zRcS_w_num);
            zRcLoc.left = (float)zPlay_obj->width * (-centre_pointU) * ratiow;
            zRcLoc.right = (float)zPlay_obj->width * wr * ratiow;
            zRcS.right = (zPlay_obj->uRange - tex_crd_warp) * ratiow;
            zRcS_w_num = 0;
        } else {
            zRcLoc.left = (float)zPlay_obj->width * (-centre_pointU);
            zRcLoc.right = (float)zPlay_obj->width * wr;
            zRcS.right = zPlay_obj->uRange - tex_crd_warp;
        }
        if(zRcS_h_num > 0) {
            float rhn_inv = 1.0f / (float)zRcS_h_num;
            zRcLoc.top = (float)zPlay_obj->height * (-centre_pointV) * rhn_inv;
            zRcLoc.bottom = (float)zPlay_obj->height * hr * rhn_inv;
            zRcS.bottom = (zPlay_obj->vRange - tex_crd_warp) * rhn_inv; //计算到每格所占纹理的高度
        } else if(zRcS_h_num < 0) {
            float ratioh = (float)(-zRcS_h_num);
            zRcLoc.top = (float)zPlay_obj->height * (-centre_pointV) * ratioh;
            zRcLoc.bottom = (float)zPlay_obj->height * hr * ratioh;
            zRcS.bottom = (zPlay_obj->vRange - tex_crd_warp) * ratioh;
            zRcS_h_num = 0;
        } else {
            zRcLoc.top = (float)zPlay_obj->height * (-centre_pointV);
            zRcLoc.bottom = (float)zPlay_obj->height * hr;
            zRcS.bottom = zPlay_obj->vRange - tex_crd_warp;
        }
    }
    zTexID = 0;
    zRcS.left = 0.0f + tex_crd_warp;
    zRcS.top = 0.0f + tex_crd_warp;
    zRcS0.left = 0.0f;
    zRcS0.top = 0.0f;
    zRcS0.right = 0.0f;
    zRcS0.bottom = 0.0f;

    zColor = 0xffffffff;
    aColor = _color;
    aBlendmode = _blendmode;
    zSubFrame = 0;
    zSubCount = 0;
    zAm_mode = zEventcount ? _am_mode : 0;

    zTexIndex = 0;
    //	zTexIndexCount = 0;
    //	zTexOnceTime   = 33;

#ifdef _GN_SOUND_H_
    zSouObj = &GNa::Sound;
    zSouEndTime = 0;
    zSouStartTime = 0;
    zTargetID = 0;
#endif
    aLevel_color = aColor;
    aLevel_ts = aFrame_ts;
    GT_LEVEL_RANGE_CPT;
}
void GNg::cFRAME_RC::InitSubFrame(cFRAME_RC *_sub_frame, unsigned short _sub_count) {
    zSubFrame = _sub_frame;
    zSubCount = _sub_count;
}
void GNg::cFRAME_RC::InitTID(unsigned short _tex_index_count, unsigned char *_tex_index, unsigned int _tex_once_time, unsigned short _am_mode) {
    zTexIndex = _tex_index;
    zTexIndexCount = _tex_index_count;
    zTexOnceTime = _tex_once_time;

    unsigned int e_time = zTexOnceTime * zTexIndexCount;
    zEndtime = e_time > zEndtime ? e_time : zEndtime;

    if(zAm_mode == eAM_NOAM) zAm_mode = eAM_REPEAT;
    if(_am_mode < 10) zAm_mode = _am_mode;
}
#ifdef _GN_SOUND_H_
void GNg::cFRAME_RC::InitSoundToData(GNa::sSOUND *sound_obj, unsigned short overlap_num) {
    if(sound_obj) zSouObj = sound_obj;
    else zSouObj = &GNa::Sound;
    zSound.InitToData(zSouObj, false, overlap_num);
}
#else
void GNg::cFRAME_RC::InitSoundToData(void *sound_obj, unsigned short overlap_num) {
}
#endif
void GNg::cFRAME_RC::GetOriLocalSize(float *wout, float *hout) {
    bool is_rc = sizeof(GNg::cFRAME_RC) == GetClassSize();
    if(wout) *wout = is_rc ? zRcLoc.right - zRcLoc.left : zRcLoc.right;
    if(hout) *hout = is_rc ? zRcLoc.bottom - zRcLoc.top : zRcLoc.bottom;
}
void GNg::cFRAME_RC::GetFrameSize(int *w_out, int *h_out) {
    if(w_out) {
        if(zRcS_w_num)
            *w_out = (int)((zPlay_obj->width / zRcS_w_num) * aFrame_ts.sx * zFrame_ts.sx);
        else *w_out = (int)(zPlay_obj->width * aFrame_ts.sx * zFrame_ts.sx);
    }
    if(h_out) {
        if(zRcS_h_num)
            *h_out = (int)((zPlay_obj->height / zRcS_h_num) * aFrame_ts.sy * zFrame_ts.sy);
        else *h_out = (int)(zPlay_obj->height * aFrame_ts.sy * zFrame_ts.sy);
    }
}
void GNg::cFRAME_RC::zEventProce(unsigned int updatetime) {
    unsigned int stime;
    sAM_EVENT *_ev = zFEvent, *ev_end = zFEvent + zEventcount;
    for(; _ev < ev_end; ++_ev) {
        if(zNowtime >= _ev->start_time && (stime = zNowtime - _ev->start_time) <= _ev->segment_time) {
            if(!_ev->slinear_mode) { //一般线性插值
                switch(_ev->event_mode) {
                    case eEVENT_MOVE_Y:
                        zFrame_ts.y = Linear(_ev->start_value, _ev->end_value, stime, _ev->segment_time);
                        break;
                    case eEVENT_MOVE_X:
                        zFrame_ts.x = Linear(_ev->start_value, _ev->end_value, stime, _ev->segment_time);
                        break;
                    case eEVENT_COLOR:
                        zColor = LinearColor(*((unsigned int *)(&_ev->start_value)), *((unsigned int *)(&_ev->end_value)), stime, _ev->segment_time);
                        break;
                    case eEVENT_SIZE_X | eEVENT_SIZE_Y:
                        zFrame_ts.sx = Linear(_ev->start_value, _ev->end_value, stime, _ev->segment_time);
                        zFrame_ts.sy = zFrame_ts.sx;
                        break;
                    case eEVENT_TEXID:
                        zTexID = Linear((unsigned short)_ev->start_value, (unsigned short)_ev->end_value + 1, stime, _ev->segment_time);
                        if(zTexID > (zPlay_obj->texnum - 1)) zTexID = zPlay_obj->texnum - 1; //预防纹理ID号出轨
                        break;
                    case eEVENT_ROTATION:
                        zFrame_ts.r = Linear(_ev->start_value, _ev->end_value, stime, _ev->segment_time);
                        break;
                    case eEVENT_SIZE_X:
                        zFrame_ts.sx = Linear(_ev->start_value, _ev->end_value, stime, _ev->segment_time);
                        break;
                    case eEVENT_SIZE_Y:
                        zFrame_ts.sy = Linear(_ev->start_value, _ev->end_value, stime, _ev->segment_time);
                        break;
                    case eEVENT_MOVE_X | eEVENT_MOVE_Y:
                        zFrame_ts.x = Linear(_ev->start_value, _ev->end_value, stime, _ev->segment_time);
                        zFrame_ts.y = zFrame_ts.x;
                        break;
#ifdef _GN_SOUND_H_
                    case eEVENT_SOUND:
                        if(zSouObj->z__attrb) {
                            if(stime < (updatetime ? updatetime : 20)) {
                                zSound.Play(zTargetID);
                                zSouEndTime = _ev->start_time + zSouObj->time;
                                zSouStartTime = _ev->start_time;
                            }
                        }
                        break;
#endif
                        /*case eEVENT_BLEND:
                 aBlendmode= BL::Linear((unsigned short)_ev->start_value,(unsigned short)_ev->end_value,stime,_ev->segment_time);
			     break;*/
                    case eEVENT_TEXMOV_X:
                        if(zRcS_w_num) {
                            uint8 trcsw = (uint8)Linear(_ev->start_value, _ev->end_value, stime, _ev->segment_time);
                            uint8 j_num = trcsw / zRcS_w_num;
                            zRcS.left = zRcS.right * (trcsw - j_num * zRcS_w_num);
                            zRcS.top = zRcS_h_num < 2 ? 0.0f : zRcS.bottom * (j_num % zRcS_h_num);

                        } else zRcS.left = Linear(_ev->start_value, _ev->end_value, stime, _ev->segment_time);
                        break;
                    case eEVENT_TEXMOV_Y:
                        if(zRcS_h_num) {
                            uint8 trcsh = (uint8)Linear(_ev->start_value, _ev->end_value, stime, _ev->segment_time);
                            uint8 i_num = trcsh / zRcS_h_num;
                            zRcS.top = zRcS.bottom * (trcsh - i_num * zRcS_h_num);
                            zRcS.left = zRcS_w_num < 2 ? 0.0f : zRcS.right * (i_num % zRcS_w_num);

                        } else zRcS.top = Linear(_ev->start_value, _ev->end_value, stime, _ev->segment_time);
                        break;
                    case eEVENT_TEXMOV_X | eEVENT_TEXMOV_Y:
                        if(zRcS_w_num) {
                            uint8 trcsw = (uint8)Linear(_ev->start_value, _ev->end_value, stime, _ev->segment_time);
                            zRcS.left = zRcS.right * (trcsw % zRcS_w_num);
                            zRcS.top = zRcS_h_num < 2 ? 0.0f : zRcS.bottom * (trcsw % zRcS_h_num);
                        } else {
                            zRcS.left = Linear(_ev->start_value, _ev->end_value, stime, _ev->segment_time);
                            zRcS.top = zRcS.left;
                        }
                        break;
                }
            } else { //有缓动的插值
                switch(_ev->event_mode) {
                    case eEVENT_MOVE_Y:
                        zFrame_ts.y = SLinear(_ev->start_value, _ev->end_value, stime, _ev->segment_time, _ev->slinear_value, _ev->slinear_mode);
                        break;
                    case eEVENT_MOVE_X:
                        zFrame_ts.x = SLinear(_ev->start_value, _ev->end_value, stime, _ev->segment_time, _ev->slinear_value, _ev->slinear_mode);
                        break;
                    case eEVENT_COLOR:
                        zColor = SLinearColor(*((unsigned int *)(&_ev->start_value)), *((unsigned int *)(&_ev->end_value)), stime, _ev->segment_time, _ev->slinear_value, _ev->slinear_mode);
                        break;
                    case eEVENT_SIZE_X | eEVENT_SIZE_Y:
                        zFrame_ts.sx = SLinear(_ev->start_value, _ev->end_value, stime, _ev->segment_time, _ev->slinear_value, _ev->slinear_mode);
                        zFrame_ts.sy = zFrame_ts.sx;
                        break;
                    case eEVENT_TEXID:
                        zTexID = (unsigned short)SLinear(_ev->start_value, _ev->end_value + 1.0f, stime, _ev->segment_time, _ev->slinear_value, _ev->slinear_mode);
                        if(zTexID > (zPlay_obj->texnum - 1)) zTexID = zPlay_obj->texnum - 1; //预防纹理ID号出轨
                        break;
                    case eEVENT_ROTATION:
                        zFrame_ts.r = SLinear(_ev->start_value, _ev->end_value, stime, _ev->segment_time, _ev->slinear_value, _ev->slinear_mode);
                        break;
                    case eEVENT_SIZE_X:
                        zFrame_ts.sx = SLinear(_ev->start_value, _ev->end_value, stime, _ev->segment_time, _ev->slinear_value, _ev->slinear_mode);
                        break;
                    case eEVENT_SIZE_Y:
                        zFrame_ts.sy = SLinear(_ev->start_value, _ev->end_value, stime, _ev->segment_time, _ev->slinear_value, _ev->slinear_mode);
                        break;
                    case eEVENT_MOVE_X | eEVENT_MOVE_Y:
                        zFrame_ts.x = SLinear(_ev->start_value, _ev->end_value, stime, _ev->segment_time, _ev->slinear_value, _ev->slinear_mode);
                        zFrame_ts.y = zFrame_ts.x;
                        break;
#ifdef _GN_SOUND_H_
                    case eEVENT_SOUND:
                        if(zSouObj->z__attrb) {
                            if(stime < (updatetime ? updatetime : 20)) {
                                zSound.Play(zTargetID);
                                zSouEndTime = _ev->start_time + zSouObj->time;
                                zSouStartTime = _ev->start_time;
                            }
                        }
                        break;
#endif
                        /*case eEVENT_BLEND:
                 aBlendmode=(unsigned short)BL::SLinear(_ev->start_value,_ev->end_value,stime,_ev->segment_time,_ev->slinear_value,_ev->slinear_mode);
			     break;*/
                    case eEVENT_TEXMOV_X:
                        if(zRcS_w_num) {
                            uint8 trcsw = (uint8)SLinear(_ev->start_value, _ev->end_value, stime, _ev->segment_time, _ev->slinear_value, _ev->slinear_mode);
                            uint8 j_num = trcsw / zRcS_w_num;
                            zRcS.left = zRcS.right * (trcsw - j_num * zRcS_w_num);
                            zRcS.top = zRcS_h_num < 2 ? 0.0f : zRcS.bottom * (j_num % zRcS_h_num);
                            ;

                        } else zRcS.left = SLinear(_ev->start_value, _ev->end_value, stime, _ev->segment_time, _ev->slinear_value, _ev->slinear_mode);
                        break;
                    case eEVENT_TEXMOV_Y:
                        if(zRcS_h_num) {
                            uint8 trcsh = (uint8)SLinear(_ev->start_value, _ev->end_value, stime, _ev->segment_time, _ev->slinear_value, _ev->slinear_mode);
                            uint8 i_num = trcsh / zRcS_h_num;
                            zRcS.top = zRcS.bottom * (trcsh - i_num * zRcS_h_num);
                            zRcS.left = zRcS_w_num < 2 ? 0.0f : zRcS.right * (i_num % zRcS_w_num);
                        } else zRcS.top = SLinear(_ev->start_value, _ev->end_value, stime, _ev->segment_time, _ev->slinear_value, _ev->slinear_mode);
                        break;
                    case eEVENT_TEXMOV_X | eEVENT_TEXMOV_Y:
                        if(zRcS_w_num) {
                            uint8 trcsw = (uint8)SLinear(_ev->start_value, _ev->end_value, stime, _ev->segment_time, _ev->slinear_value, _ev->slinear_mode);
                            zRcS.left = zRcS.right * (trcsw % zRcS_w_num);
                            zRcS.top = zRcS_h_num < 2 ? 0.0f : zRcS.bottom * (trcsw % zRcS_h_num);
                        } else {
                            zRcS.left = SLinear(_ev->start_value, _ev->end_value, stime, _ev->segment_time, _ev->slinear_value, _ev->slinear_mode);
                            zRcS.top = zRcS.left;
                        }
                        break;
                }
            }
        }
    }
}
void GNg::cFRAME_RC::zTexIndexProce() {
    if(zTexIndex) {
        unsigned short t_index = (unsigned short)(zNowtime / zTexOnceTime);
        if(zRcS_w_num) {
            uint8 j_num = t_index / zRcS_w_num;
            zRcS.left = zRcS.right * t_index; //循环播放索引动画
            zRcS.top = zRcS.bottom * j_num;
        } else {
            if(t_index > zTexIndexCount - 1) t_index = zTexIndexCount - 1; //播放到最后一张停住 当其他动画比索引动画长时 确保索引不出轨
            zTexID = zTexIndex[t_index];
            if(zTexID > zPlay_obj->texnum - 1) zTexID = zPlay_obj->texnum - 1; //预防纹理ID出轨
        }
    }
}
unsigned int GNg::cFRAME_RC::PlayFrame(unsigned int updatetime, cFRAME_RC *father) {
    if(zAm_mode) {
#define GT_TMP_FRAME_TIME_PROCE                                     \
    switch(zAmTimeProce(updatetime)) { /*时间和播放模式的控制处理*/ \
        case GNg::eAM_ONCE_HIDE: return 0;                          \
        case GNg::eAM_ONCE_KEEP: break;                             \
        case GNg::eAM_LOOP: /*刚从新循环时就恢复一下状态*/          \
            GT_ResetTransf2D(zFrame_ts, zFrameTsReItem);            \
            zColor = 0xffffffff;                                    \
            zTexID = 0;                                             \
        default:                                                    \
            zEventProce(updatetime); /*处理补间动画插值*/           \
            break;                                                  \
    }                                                               \
    zTexIndexProce(); /*当有索引动画的时候 索引动画会覆盖纹理ID动画插值*/

        GT_TMP_FRAME_TIME_PROCE
        aLevel_color = MultiplyColorRatio(zColor, aColor); //混合动画插值颜色和用户指定颜色
        MultTransf2D(&aLevel_ts, &zFrame_ts, &aFrame_ts); //混合动画插值变换和用户指定变换

    } else {
        aLevel_color = aColor; //直接传递颜色
        aLevel_ts = aFrame_ts;
    }
    if(father) {
        aLevel_color = MultiplyColorRatio(aLevel_color, father->aLevel_color); //混合子父框颜色变化影响
        MultTransf2D(&aLevel_ts, &aLevel_ts, &(father->aLevel_ts)); //混合子父框包含变换信息
    }
    /*处理子筐的东西*/
#define GT_TMP_FRAME_PLAY_SUB(PTYPE, FUNC)                                                                \
    if(zSubCount) {                                                                                       \
        unsigned short sci;                                                                               \
        if(updatetime) {                                                                                  \
            for(sci = 0; sci < zSubCount; ++sci)                                                          \
                (PTYPE(zSubFrame))[sci].FUNC(updatetime, this); /*递归处理子筐	这样子筐通常渲染在主筐后*/ \
        } else {                                                                                          \
            for(sci = 0; sci < zSubCount; ++sci) {                                                        \
                (PTYPE(zSubFrame))[sci].SetRuntime(&zRuntime);                                            \
                (PTYPE(zSubFrame))[sci].FUNC(0, this);                                                    \
            }                                                                                             \
        }                                                                                                 \
        if(!aBlendmode) return zRuntime;                                                                  \
    }
    GT_TMP_FRAME_PLAY_SUB(, PlayFrame)

    GT_LEVEL_RANGE_CPT;
    int dw = (int)(aLevel_range.right - aLevel_range.left);
    int dh = (int)(aLevel_range.bottom - aLevel_range.top);

    if(aBlendmode && (aLevel_color & 0xff000000)) {
        /*处理纹理截图控制信息*/
#define GT_TMP_FRAME_TEXCOORD_RNG_CPT                                      \
    zRcS0.left += zRcS0.right * updatetime; /*处理纹理自动移动动画的偏移*/ \
    zRcS0.top += zRcS0.bottom * updatetime;                                \
    float uv_left = zRcS.left + zRcS0.left; /*整合两个UV范围*/             \
    float uv_top = zRcS.top + zRcS0.top;                                   \
    float uv_right = uv_left + zRcS.right;                                 \
    float uv_bottom = uv_top + zRcS.bottom;

        GT_TMP_FRAME_TEXCOORD_RNG_CPT
        ImageDraw(zPlay_obj, (int)aLevel_range.left, (int)aLevel_range.top, dw, dh, aLevel_color, zTexID, aBlendmode, uv_left, uv_top, uv_right, uv_bottom);
    }
#ifdef _GN_SOUND_H_
    //处理声音定位( 图筐中心X , 图筐中心Y , 图框缩放比例 )
#define GT_TMP_FRAME_SOUND_PROC(SPX, SPY, SCL)                                           \
    if(zNowtime < zSouEndTime && zNowtime >= zSouStartTime) {                            \
        float sp_x = SPX, sp_y = SPY;                                                    \
        float ratio_w = (float)(ViewCur->zBackScreenW >> 1),                             \
              ratio_h = (float)(ViewCur->zBackScreenH >> 1);                             \
        sp_x = (sp_x - ratio_w) / ratio_w * 1.2f; /*把屏幕坐标映射为3D声音坐标*/         \
        sp_y = (sp_y - ratio_h) / ratio_h * 1.2f;                                        \
        float sp_z = 1.0f / (SCL * SCL) * -4.0f; /*按近大远小逻辑来换算成3D声音坐标的Z*/ \
        zSound.SetPosi(zTargetID, sp_x, sp_y, sp_z);                                     \
    }
    GT_TMP_FRAME_SOUND_PROC(aLevel_range.left + (float)(dw >> 1), sp_y = aLevel_range.top + (float)(dh >> 1), aLevel_ts.sx)
#endif
    return zRuntime;
}
void GNg::cFRAME_RC::ReSetPlaytime(eAM_TIME_RESET_OP re_op) {
    if(zNodes) {
        zSegmentID = 0;
        zStarttime = 0;
        zEndtime = zNodes->node_time;
        zAm_mode = zNodes->am_mode;
    }
    zRuntime = 0;
    zNowtime = 0;
    GT_ResetTransf2D(zFrame_ts, zFrameTsReItem);
    zColor = 0xffffffff;
    zTexID = 0;
    zAm_s_e = 0;
    if(re_op == eAM_TIME_RESET_SELF) return;

    if(zSubCount) {
        unsigned short sci = 0;
        if(sizeof(cFRAME_RC) == zSubFrame->GetClassSize()) {
            for(; sci < zSubCount; ++sci) zSubFrame[sci].ReSetPlaytime(re_op);
        } else {
            for(; sci < zSubCount; ++sci) ((cFRAME *)zSubFrame)[sci].ReSetPlaytime(re_op);
        }
    }
}
void GNg::cFRAME_RC::SetAmMode(unsigned short am_mode) {
    if(zNowtime > zEndtime) zNowtime = zEndtime;
    zAm_mode = am_mode;
    if(am_mode == eAM_REPEAT) zAm_s_e = true;
}
inline void GT_AmEvsStartEndValueSwap(GNg::sAM_EVENT *evs, unsigned int evn) {
    GNg::sAM_EVENT *ev_end = evs + evn;
    for(; evs < ev_end; ++evs) GN_Swap(float, evs->start_value, evs->end_value);
}
void GNg::cFRAME_RC::SwapAmStartEnd() {
    GT_AmEvsStartEndValueSwap(zFEvent, zEventcount);
}
void GNg::cFRAME_RC::TextureMove(float vx, float vy) {
    if(zRcS_w_num) zRcS0.right = GN::TimeRatio * vx * 0.001f / (float)zRcS_w_num;
    else zRcS0.right = GN::TimeRatio * vx * 0.001f;
    if(zRcS_h_num) zRcS0.bottom = GN::TimeRatio * vy * 0.001f / zRcS_h_num;
    else zRcS0.bottom = GN::TimeRatio * vy * 0.001f;
}
void GNg::cFRAME_RC::TextureStopMove() {
    zRcS0.right = 0.0f;
    zRcS0.bottom = 0.0f;
}
void GNg::cFRAME_RC::GetTexturePosi(float *px, float *py) {
    *px = zRcS0.left;
    *py = zRcS0.top;
}
void GNg::cFRAME_RC::SetTexturePosi(float px, float py) {
    zRcS0.left = px;
    zRcS0.top = py;
}
void GNg::cFRAME_RC::SetTextureSquare(short i_num, short j_num) {
    zRcS0.left = zRcS.right * (float)(i_num % zRcS_w_num);
    zRcS0.top = zRcS.bottom * (float)(j_num % zRcS_h_num);
}
void GNg::cFRAME_RC::SetTextureID(unsigned short tex_id) {
    if(tex_id >= zPlay_obj->texnum) tex_id = zPlay_obj->texnum - 1;
    zTexID = tex_id;
}
int8 GNg::cFRAME_RC::GetTextureSquareWN() {
    if(zRcS_w_num) return zRcS_w_num;
    float num = zRcS.right / zPlay_obj->uRange;
    if(num > 1.0f) return (int8)(-GN_Round(num));
    return 0;
}
int8 GNg::cFRAME_RC::GetTextureSquareHN() {
    if(zRcS_h_num) return zRcS_h_num;
    float num = zRcS.bottom / zPlay_obj->vRange;
    if(num > 1.0f) return (int8)(-GN_Round(num));
    return 0;
}
float GNg::cFRAME_RC::GetOriPointU() {
    return -zRcLoc.left / (sizeof(GNg::cFRAME_RC) == GetClassSize() ? zRcLoc.right - zRcLoc.left : zRcLoc.right);
}
float GNg::cFRAME_RC::GetOriPointV() {
    return -zRcLoc.top / (sizeof(GNg::cFRAME_RC) == GetClassSize() ? zRcLoc.bottom - zRcLoc.top : zRcLoc.bottom);
}
void GNg::cFRAME_RC::zSelectTarget(unsigned short id) {
#ifdef _GN_SOUND_H_
    zTargetID = id;
#endif
}
int GNg::cFRAME_RC::GetClassSize() {
    return sizeof(/*cFRAME_RC*/ *this);
}
bool GNg::cFRAME_RC::HitPointTest(int x, int y) {
    sRECT2 rect; //当有镜象就把它反过来做判断 --------------------------------
    int8 i = 0;
    if(aFrame_ts.sx < 0.0f) ++i;
    if(zFrame_ts.sx < 0.0f) ++i;
    if(i & 1) {
        rect.left = aLevel_range.right, rect.right = aLevel_range.left;
    } else {
        rect.left = aLevel_range.left, rect.right = aLevel_range.right;
    }
    i = 0;
    if(aFrame_ts.sy < 0.0f) ++i;
    if(zFrame_ts.sy < 0.0f) ++i;
    if(i & 1) {
        rect.top = aLevel_range.bottom, rect.bottom = aLevel_range.top;
    } else {
        rect.top = aLevel_range.top, rect.bottom = aLevel_range.bottom;
    }

    sVEC2 p((float)x, (float)y);
    return TestPointIntRect(&p, &rect);
}
bool GNg::cFRAME_RC::HitFrameTest(cFRAME_RC *frame) {
    if(sizeof(cFRAME_RC) == frame->GetClassSize()) {
        return TestRectIntRect(&aLevel_range, &(frame->aLevel_range));
    } else {
        sQUADS2 q1(aLevel_range.left, aLevel_range.top,
                   aLevel_range.right, aLevel_range.top,
                   aLevel_range.right, aLevel_range.bottom);
        sQUADS2 q2(frame->aLevel_range.left, frame->aLevel_range.top,
                   frame->aLevel_ts.x, frame->aLevel_ts.y,
                   frame->aLevel_range.right, frame->aLevel_range.bottom);
        return TestQuadsIntQuads(&q1, &q2);
    }
}
/*************************************** 类 cFRAME ***********************************************/
GNg::cFRAME &GNg::cFRAME::operator=(const cFRAME &src) {
    this->cFRAME_RC::operator=(src);
    return *this;
}
GNg::cFRAME *GNg::cFRAME::zScreenFrame = 0;

void GNg::cFRAME::SetToScreenFRAME() {
    sMAT33 loc_mat, act_mat;
    M33GetTransf2D(&act_mat, &aFrame_ts);
    M33GetTransf2D(&loc_mat, &zFrame_ts);
    MultM33(&aLevel_world, &loc_mat, &act_mat);

    aLevel_color = MultiplyColorRatio(zColor, aColor); //混合动画插值颜色和用户指定颜色
    cFRAME::zScreenFrame = this;
}
GNg::cFRAME *GNg::cFRAME::RemoveScreenFRAME() {
    cFRAME *ret = zScreenFrame;
    zScreenFrame = 0;
    return ret;
}
int GNg::cFRAME::GetClassSize() {
    return sizeof(/*cFRAME*/ *this);
}
bool GNg::cFRAME::HitPointTest(int x, int y) {
    sVEC2 p0(aLevel_range.left, aLevel_range.top),
        p1(aLevel_ts.x, aLevel_ts.y),
        p2(aLevel_range.right, aLevel_range.bottom);
    sQUADS2 qs;
    int8 i = 0; //当有镜象就把它反过来做判断 ---------------------
    if(aFrame_ts.sx < 0.0f) ++i;
    if(zFrame_ts.sx < 0.0f) ++i;
    if(i & 1) {
        qs.p0 = p1;
        qs.p1 = p0;
    } else {
        qs.p0 = p0;
        qs.p1 = p1;
    }
    i = 0;
    if(aFrame_ts.sy < 0.0f) ++i;
    if(zFrame_ts.sy < 0.0f) ++i;
    if(i & 1) {
        qs.p2 = qs.p1;
        qs.p1 = p2;
        qs.p0 += (qs.p1 - qs.p2);
    } else {
        qs.p2 = p2;
    }

    sVEC2 p((float)x, (float)y);
    return TestPointIntQuads(&p, &qs);
}
bool GNg::cFRAME::HitFrameTest(cFRAME_RC *frame) {
    sRECT2 *rc2 = &(frame->aLevel_range);
    if(sizeof(cFRAME_RC) == frame->GetClassSize()) {
        sQUADS2 q1(rc2->left, rc2->top,
                   rc2->right, rc2->top,
                   rc2->right, rc2->bottom);
        sQUADS2 q2(aLevel_range.left, aLevel_range.top,
                   aLevel_ts.x, aLevel_ts.y,
                   aLevel_range.right, aLevel_range.bottom);
        return TestQuadsIntQuads(&q1, &q2);
    } else {
        sQUADS2 q3(aLevel_range.left, aLevel_range.top,
                   aLevel_ts.x, aLevel_ts.y,
                   aLevel_range.right, aLevel_range.bottom);
        sQUADS2 q4(rc2->left, rc2->top,
                   frame->aLevel_ts.x, frame->aLevel_ts.y,
                   rc2->right, rc2->bottom);
        return TestQuadsIntQuads(&q3, &q4);
    }
}
unsigned int GNg::cFRAME::PlayFrame(unsigned int updatetime, cFRAME *father) {
    if(zAm_mode) {
        GT_TMP_FRAME_TIME_PROCE
        aLevel_color = MultiplyColorRatio(zColor, aColor); //混合动画插值颜色和用户指定颜色
        MultTransf2D(&aLevel_world, &zFrame_ts, &aFrame_ts); //混合动画插值变换和用户指定变换
    } else {
        aLevel_color = aColor;
        M33GetTransf2D(&aLevel_world, &aFrame_ts); //把变换信息转换为矩阵
    }
    if(father) {
        aLevel_color = MultiplyColorRatio(aLevel_color, father->aLevel_color); //混合子父框颜色变化影响
        MultM33T(&aLevel_world, &(father->aLevel_world)); //组合子父框变换矩阵
    }
    GT_TMP_FRAME_PLAY_SUB((cFRAME *), PlayFrame)

    /*模板代码:变换框内顶点出来*/
    sVEC2 vx2(zRcLoc.left, zRcLoc.top);
    float rcw = zRcLoc.right, rch = zRcLoc.bottom;
    float vt[8], rcwx = rcw * aLevel_world.m11, rcwy = rcw * aLevel_world.m12, rchx = rch * aLevel_world.m21, rchy = rch * aLevel_world.m22;
    vt[0] = GN_Vec21DotM33Col(vx2., aLevel_world., 1);
    vt[1] = GN_Vec21DotM33Col(vx2., aLevel_world., 2);
    vt[2] = vt[0] + rcwx;
    vt[3] = vt[1] + rcwy;
    vt[4] = vt[2] + rchx;
    vt[5] = vt[3] + rchy;
    vt[6] = vt[0] + rchx;
    vt[7] = vt[1] + rchy;

#ifdef GT_TMP_COMPATI_DX_DDRAW
    if(RenderRotSupport) {
#endif
        aLevel_range.left = vt[0], aLevel_range.top = vt[1];
        aLevel_ts.x = vt[2], aLevel_ts.y = vt[3];
        aLevel_range.right = vt[4], aLevel_range.bottom = vt[5];
        if(aBlendmode && (aLevel_color & 0xff000000)) {
            GT_TMP_FRAME_TEXCOORD_RNG_CPT
            ImageDraw4P(zPlay_obj, vt, aLevel_color, zTexID, aBlendmode, uv_left, uv_top, uv_right, uv_bottom);
        }
#ifdef GT_TMP_COMPATI_DX_DDRAW
    } else { //DirectDraw 不带旋转会强型转正
        float cx = (vt[0] + vt[4]) * 0.5f, cy = (vt[1] + vt[5]) * 0.5f;
        float vwx = vt[2] - vt[0], vwy = vt[3] - vt[1];
        float vhx = vt[6] - vt[0], vhy = vt[7] - vt[1];
        float vwl = sqrtf(vwx * vwx + vwy * vwy); //v1 至 v0的长度作为宽
        float vhl = sqrtf(vhx * vhx + vhy * vhy); //v3 至 v0的长度作为高

        aLevel_range.left = cx - vwl * 0.5f;
        aLevel_range.top = cy - vhl * 0.5f;
        aLevel_range.right = aLevel_range.left + vwl;
        aLevel_range.bottom = aLevel_range.top + vhl;
        aLevel_ts.x = aLevel_range.right;
        aLevel_ts.y = aLevel_range.top;
        if(aBlendmode && (aLevel_color & 0xff000000)) {
            GT_TMP_FRAME_TEXCOORD_RNG_CPT
            ImageDraw(zPlay_obj, (int)(aLevel_range.left), (int)(aLevel_range.top), (int)vwl, (int)vhl, aLevel_color, zTexID, aBlendmode, uv_left, uv_top, uv_right, uv_bottom);
        }
    }
#endif
#ifdef _GN_SOUND_H_
    GT_TMP_FRAME_SOUND_PROC((aLevel_range.left + aLevel_range.right) * 0.5f, (aLevel_range.top + aLevel_range.bottom) * 0.5f, aLevel_world.m11)
#endif
    return zRuntime;
}
/******************************************************* cFRAME_LAUNCHER_ *********************************************************/
GNg::cFRAME_LAUNCHER_::cFRAME_LAUNCHER_() : zFrame(0),
                                            zFrame2(0),
                                            //aGen_angle     = 0.0f;
                                            //zGen_speed     = 1000;
                                            //zMax_gen_speed = zGen_speed;
                                            //zGen_once_num  = 1;
                                            //zMax_once_num  = zGen_once_num;
                                            zTransf_s(0),
                                            zColor_s(0),
                                            zNowtime_s(0),
                                            zRunUpdate(false) {}
void GNg::cFRAME_LAUNCHER_::zCopyData(const cFRAME_LAUNCHER_ &src, bool cpy_state) {
    zFrame = src.zFrame;
    zFrame2 = src.zFrame2;
    aGenColor = src.aGenColor;
    aGen_angle = src.aGen_angle;
    zObjsKeep = src.zObjsKeep;
    if(cpy_state) {
        zGen_speed = src.zGen_speed;
        zMax_gen_speed = src.zMax_gen_speed;
        zGen_once_num = src.zGen_once_num;
        zMax_once_num = src.zMax_once_num;
        zMax_num = src.zMax_num;

        zObjNowtime = src.zObjNowtime;
        zDisplay_num = src.zDisplay_num;
        ztBright = src.ztBright;
        ztBn = src.ztBn;
        ztSn = src.ztSn;
        ztSize = src.ztSize;
        ztCn = src.ztCn;
        zTa = src.zTa;
        zTr = src.zTr;
        zTg = src.zTg;
        zTb = src.zTb;
    } else {
        zMax_gen_speed = src.zMax_gen_speed;
        zGen_speed = zMax_gen_speed;
        zMax_once_num = src.zMax_once_num;
        zGen_once_num = zMax_once_num;
        zMax_num = src.zMax_num;

        zResetState();
    }
    aGen_ts_flags = src.aGen_ts_flags;
    aPose_range = src.aPose_range;
    aAngle_range = src.aAngle_range;
    aSize_range = src.aSize_range;
    aColor_range = src.aColor_range;
    zActivity_time = src.zActivity_time;
    zEnd_time = src.zEnd_time;
    if(cpy_state) {
        zGen_num = src.zGen_num;
        zGen_count = src.zGen_count;
        zRunUpdate = src.zRunUpdate;

        if(src.zColor_s) { /*深复制到新对象*/
            zColor_s = new unsigned int[zMax_num];
            memcpy(zColor_s, src.zColor_s, zMax_num << 2);
        } else zColor_s = 0;
        if(src.zNowtime_s) { /*深复制到新对象*/
            zNowtime_s = new unsigned int[zMax_num];
            memcpy(zNowtime_s, src.zNowtime_s, zMax_num << 2);
        } else zNowtime_s = 0;
    } else {
        zGen_num = 0;
        zGen_count = 0;
        zRunUpdate = zFrame ? true : false;

        if(src.zColor_s) {
            zColor_s = new unsigned int[zMax_num];
            //memset(zColor_s, 0xff, zMax_num<<2);
        } else zColor_s = 0;
        if(src.zNowtime_s) {
            zNowtime_s = new unsigned int[zMax_num];
            memset(zNowtime_s, 0xff, zMax_num << 2);
        } else zNowtime_s = 0;
    }
    if(src.zTransf_s) {
        unsigned int dsize = zTsUnitSize() * zMax_num;
        zTransf_s = malloc(dsize);
        if(cpy_state) memcpy(zTransf_s, src.zTransf_s, dsize);
        /*else{
			unsigned int  i = 0;
			for(;i<zMax_num;++i)_tss[i] = Ts2D;
		}*/
    } else zTransf_s = 0;
}
GNg::cFRAME_LAUNCHER_::cFRAME_LAUNCHER_(const cFRAME_LAUNCHER_ &src) {
    zCopyData(src, false);
}
GNg::cFRAME_LAUNCHER_ &GNg::cFRAME_LAUNCHER_::operator=(const cFRAME_LAUNCHER_ &src) {
    this->Release();
    zCopyData(src, false);
    return *this;
}
void GNg::cFRAME_LAUNCHER_::PerfectCopy(const cFRAME_LAUNCHER_ &src) {
    this->Release();
    zCopyData(src, true);
}
void GNg::cFRAME_LAUNCHER_::InitToData(cFRAME *_frame, float _gen_angle, unsigned int _gen_speed, unsigned short _gen_once_num, int8 _pose_range, int8 _angle_range, int8 _size_range, int8 _color_range, unsigned int _activity_time, cFRAME *_frame2) {
    zFrame = _frame;
    zFrame2 = _frame2;
    aGenColor = 0xffffffff;
    aGen_angle = _gen_angle;
    zGen_speed = _gen_speed ? _gen_speed : 1;
    zMax_gen_speed = zGen_speed;
    zGen_once_num = _gen_once_num ? _gen_once_num : 1;
    zMax_once_num = zGen_once_num;
    aGen_ts_flags = 0;
    aPose_range = _pose_range;
    aAngle_range = _angle_range;
    aSize_range = _size_range;
    aColor_range = _color_range;
    zActivity_time = _activity_time;
    zRunUpdate = true;
    zEnd_time = zFrame->GetEndtime();
    zReAllocBuffer();
}
void GNg::cFRAME_LAUNCHER_::zReAllocBuffer(bool reset) {
    unsigned int last_maxnum = zMax_num;
    zMax_num = (((unsigned int)(zEnd_time / zMax_gen_speed)) + 1) * zMax_once_num;
    if(reset) {
        this->zRelease();
        zTransf_s = malloc(zTsUnitSize() * zMax_num);
        zColor_s = new unsigned int[zMax_num];
        zNowtime_s = new unsigned int[zMax_num];
        ReSetPlaytime();
    } else {
        void *tss = malloc(zTsUnitSize() * zMax_num);
        unsigned int *ccs = new unsigned int[zMax_num];
        unsigned int *nts = new unsigned int[zMax_num];
        if(zMax_num < last_maxnum) last_maxnum = zMax_num;
        else if(zMax_num > last_maxnum) memset(nts + last_maxnum, 0xff, (zMax_num - last_maxnum) << 2);
        memcpy(tss, zTransf_s, zTsUnitSize() * last_maxnum);
        memcpy(ccs, zColor_s, last_maxnum << 2);
        memcpy(nts, zNowtime_s, last_maxnum << 2);
        this->zRelease();
        zTransf_s = tss;
        zColor_s = ccs;
        zNowtime_s = nts;
    }
#ifdef _GN_SOUND_H_
    if(zFrame && zFrame->zSouObj->z__attrb) zFrame->InitSoundToData(zFrame->zSouObj, zMax_num);
#endif
}
unsigned int GNg::cFRAME_LAUNCHER_::zTsUnitSize() {
    return sizeof(sTRANSF2D);
}
void GNg::cFRAME_LAUNCHER_::zRelease() {
    GN_MemFree(zTransf_s);
    GN_DeleteArray(zColor_s);
    GN_DeleteArray(zNowtime_s);
}
void GNg::cFRAME_LAUNCHER_::Release() {
    zRelease();
    if(zObjsKeep.Us() == 1) {
        GN_Delete(zFrame);
        GN_Delete(zFrame2);
        zObjsKeep.Delete();
    }
}
unsigned int GNg::cFRAME_LAUNCHER_::zGetColorChange(int speed) { //返回颜色幻彩值
    if(zTr) { //GT_TMP_NEXT_CHANGE_COLOR
        if(zTr >= 255 && !zTg) zTb -= speed;
    } else if(zTb < 255) zTb += speed;

    if(zTb) {
        if(zTb >= 255 && zTg) zTg -= speed;
        else if(zTr < 255 && zTr) zTr += speed;
        else if(!zTr && !zTg) zTr += speed;
    } else if(zTg >= 255 && zTr) zTr -= speed;
    else zTg += speed;

    unsigned int cc;
    uint8 *p = (uint8 *)&cc;
    p[0] = zTb;
    p[1] = zTg;
    p[2] = zTr;
    p[3] = 255;
    return cc;
}
float GNg::cFRAME_LAUNCHER_::zCreateUnit_RotSizeAndColor(unsigned int id, float &rot_out) {
    float size;
    if(aSize_range) { //大小比例
        if(aSize_range == 127) size = 1.0f;
        else if(aSize_range < 0) {
            uint8 temps = (uint8)((aSize_range + 128) * zGen_count); //快捷截断超出的范围 效果相当于%255
            size = (float)temps / 255.0f * 0.5f + 0.5f;
        } else {
            uint8 s = 127 - aSize_range;
            if(ztSn) {
                if(ztSize - s < 0) ztSn = 0;
                else ztSize -= s;
                size = (float)ztSize / 255.0f * 0.5f + 0.5f;
            } else {
                if(ztSize + s > 255) ztSn = 1;
                else ztSize += s;
                size = (float)ztSize / 255.0f * 0.5f + 0.5f;
            }
        }
    } else size = GN_Random(0.5f) + 0.5f; //随机大小

    if(aAngle_range) { /*旋转角度*/
        uint8 tempc = (uint8)(*((uint8 *)(&aAngle_range)) * zGen_count); //快捷截断超出的范围 效果相当于%255
        rot_out = ((float)tempc / 255.0f * aGen_angle - aGen_angle * 0.5f);
    } else rot_out = (GN_Random(aGen_angle) - aGen_angle * 0.5f); //在额定范围内产生随机角度数

    if(aColor_range) { //颜色
        if(aColor_range == 127) zColor_s[id] = aGenColor;
        else if(aColor_range > 0) {
            if(aColor_range < 60) zColor_s[id] = zGetColorChange(51);
            else zColor_s[id] = zGetColorChange(127 - aColor_range); //幻彩变化
            if(aGenColor != 0xffffffff) zColor_s[id] = MultiplyColorRatio(zColor_s[id], aGenColor);
        } else { //值为负数的
            uint8 c = 129 + aColor_range;
            if(ztCn) {
                if(zTa - c < 0) ztCn = 0;
                else zTa -= c;
            } else {
                if(zTa + c > 255) ztCn = 1;
                else zTa += c;
            }
            uint8 ca = aGenColor >> 24;
            zColor_s[id] = (ca == 0xff ? (zTa << 24) : ((unsigned int)zTa * (unsigned int)ca << 16)) | (aGenColor & 0x00ffffff);
        }
    } else {
        zColor_s[id] = (unsigned int)(GN_Random(0x00ffffff)) | ((unsigned int)(GN_Random(0x77000000)) + 0x99000000); //随机颜色 包含随机透明
        if(aGenColor != 0xffffffff) zColor_s[id] = MultiplyColorRatio(zColor_s[id], aGenColor);
    }
    zNowtime_s[id] = 0; //标记可重新播放
    return size;
}
void GNg::cFRAME_LAUNCHER::zCreateUnit(unsigned int id) {
    sTRANSF2D *_ts = ((sTRANSF2D *)zTransf_s) + id;
    _ts->sx = zCreateUnit_RotSizeAndColor(id, _ts->r);
    _ts->sy = _ts->sx;

#define GT_TMP_FRAME_LAUNCHER_GEN_POS(ADD_PROC)                                       \
    if(aPose_range) { /*设置新筐位置*/                                                \
                                                                                      \
    } else {                                                                          \
        _ts->x = GN_Random(aGenRngW) + aGenRngX; /*在额定范围内产生随机数x*/          \
        _ts->y = GN_Random(aGenRngH) ADD_PROC + aGenRngY; /*在额定范围内产生随机数y*/ \
    }
    GT_TMP_FRAME_LAUNCHER_GEN_POS()
}
void GNg::cFRAME_LAUNCHER2D::zCreateUnit(unsigned int id) {
    sTRANSF2D *_ts = ((sTRANSF2D *)zTransf_s) + id;
    _ts->sx = zCreateUnit_RotSizeAndColor(id, _ts->r);
    _ts->sy = _ts->sx;

    if(aGen_ts_flags) {
        if(aPose_range) { //设置新筐位置

        } else *((sVEC2 *)&(_ts->x)) = zGenRngAx * GN_Random(aGenRngW) + zGenRngAy * GN_Random(aGenRngH) + sVEC2(aGenRngX, aGenRngY); //在额定范围内产生随机位置

        if(aGen_ts_flags & eTRANSF_SCL) {
            _ts->sx *= zGenRngSclX;
            _ts->sy *= zGenRngSclY;
        }
        if(aGen_ts_flags & eTRANSF_ROT) _ts->r += zGenRngRot;
    } else {
        GT_TMP_FRAME_LAUNCHER_GEN_POS(*((-zGenRngAx.y) * zGenRngAy.x + zGenRngAx.x * zGenRngAy.y < 0.0f /*有垂直翻转*/ ? -1.0f : 1.0f))
    }
}
GNg::sVEC2 GNg::cFRAME_LAUNCHER::GetCentre() {
    return sVEC2(aGenRngW * 0.5f + aGenRngX, aGenRngH * 0.5f + aGenRngY);
}

GNg::sVEC2 GNg::cFRAME_LAUNCHER2D::GetCentre() {
    if(aGen_ts_flags) return zGenRngAx * (aGenRngW * 0.5f) + zGenRngAy * (aGenRngH * 0.5f) + sVEC2(aGenRngX, aGenRngY);
    else return GNg::cFRAME_LAUNCHER::GetCentre();
}
void GNg::cFRAME_LAUNCHER::zPlayFrame2(unsigned int updatetime, unsigned int color) {
    *((sVEC2 *)&(zFrame2->aFrame_ts.x)) = this->GetCentre();
    zFrame2->aColor = color;
    zFrame2->PlayFrame(updatetime);
}
void GNg::cFRAME_LAUNCHER_::zPlayFrames(unsigned int updatetime) {
    sTRANSF2D *tss = (sTRANSF2D *)zTransf_s;
    unsigned int ID = 0;
    for(; ID < zMax_num; ++ID) {
#define GT_TMP_PLAY_FRAMES(TIME)                      \
    if(TIME < zEnd_time) {                            \
        zFrame->aFrame_ts = tss[ID];                  \
        zFrame->aColor = zColor_s[ID];                \
        zFrame->zTargetID = ID;                       \
        zFrame->SetRuntime(&TIME);                    \
        zFrame->PlayFrame(0); /*模板代码:播放筐单元*/ \
        zNowtime_s[ID] += updatetime;                 \
        ++zDisplay_num;

        GT_TMP_PLAY_FRAMES(zNowtime_s[ID])
    }
}
}
void GNg::cFRAME_LAUNCHER_::zUpdateFrame2(unsigned int updatetime) {
    if(zObjsKeep.Us() > 1) {
        updatetime /= zObjsKeep.Us();
        if(!updatetime) updatetime = 1;
    }
    if(zGen_speed == zMax_gen_speed) { //处理光亮增加量;
        ztBright -= (short)((512 - ((zGen_speed - zMax_gen_speed) * 350 / zGen_speed)) * updatetime / zEnd_time); //处理光亮减少
        if(ztBright < 0) ztBright = 0; //直接一下加到最大
    } else {
        if(ztBn) { //带有缓动的增加
            ztBright += (short)(168 * zGen_once_num / zMax_num);
            if(ztBright > 255) ztBright = 255;
            --ztBn;
        } else {
            ztBright -= (short)(512 * updatetime / zEnd_time);
            if(ztBright < 0) ztBright = 0;
        }
    }
    zPlayFrame2(updatetime, (ztBright << 24) | 0x00ffffff);
}
unsigned int GNg::cFRAME_LAUNCHER_::PlayFrame(unsigned int updatetime) {
    if(zFrame) {
        if(zActivity_time && zObjNowtime > zActivity_time + zEnd_time) return 0; /*模板代码:表示播放已经结束*/
        if(zRunUpdate) zObjNowtime += updatetime;
        else updatetime = 0; //暂停
        // 筐生成处理 ---------------------------------------------------------------------------------------
        if(zActivity_time == 0 || zObjNowtime <= zActivity_time) { /*模版代码:在活动时间范围内*/
            if(zObjNowtime > zGen_speed * zGen_num) { //到生成时间
                unsigned int gi = 0;
                for(; gi < zGen_once_num; ++gi) zCreateUnit((zGen_count++) % zMax_num); //创建筐单位
                ++zGen_num; //生成次数加1

#define GT_TMP_FRAME_ZT_BRIGHT_UPDATE                                                 \
    if(zFrame2) {                                                                     \
        if(zGen_speed == zMax_gen_speed) { /*处理光亮增加*/                           \
            ztBright += (short)(768 * zGen_once_num / zMax_num); /*直接一下加到最大*/ \
            if(ztBright > 255) ztBright = 255;                                        \
        } else ztBn = (uint8)(zEnd_time / 30 / updatetime + 1); /*带有缓动的增加*/    \
    }
                GT_TMP_FRAME_ZT_BRIGHT_UPDATE
            }
        }
        zDisplay_num = 0;
        zPlayFrames(updatetime); //更新播放处理
    }
    if(zFrame2) zUpdateFrame2(updatetime); //播放附加效果筐
    return zObjNowtime;
}
void GNg::cFRAME_LAUNCHER_::SetGenerateSpeed(unsigned int _time) {
    zGen_speed = _time < zMax_gen_speed ? zMax_gen_speed : _time;
    zGen_num = zGen_speed * zGen_num / zGen_speed;
}
void GNg::cFRAME_LAUNCHER_::SetGenerateOnceNum(unsigned short _once_num) {
    zGen_once_num = _once_num > zMax_once_num ? zMax_once_num : _once_num;
}
void GNg::cFRAME_LAUNCHER_::SetSubNowtime(unsigned int id, unsigned int _time) {
    if(zNowtime_s) zNowtime_s[id % zMax_num] = _time;
}
void GNg::cFRAME_LAUNCHER_::SetSubColor(unsigned int id, unsigned int color) {
    if(zColor_s) zColor_s[id % zMax_num] = color;
}
void GNg::cFRAME_LAUNCHER_::SetSubTS(unsigned int id, const sTRANSF2D *ts) {
    if(zTransf_s && ts) ((sTRANSF2D *)zTransf_s)[id % zMax_num] = *ts;
}
unsigned int GNg::cFRAME_LAUNCHER_::GetGenerateID(unsigned int id) {
    unsigned int eid = zGen_count % zMax_num;
    return zGen_count - (id < eid ? eid : (eid + zMax_num)) + id;
}
void GNg::cFRAME_LAUNCHER_::StopGenerate(void) {
    if(zActivity_time) {
        if(zActivity_time > zObjNowtime) zActivity_time = zObjNowtime;
    } else zActivity_time = zObjNowtime;
}
void GNg::cFRAME_LAUNCHER_::ContinueGenerate(unsigned int _time) {
    if(_time) {
        if(zActivity_time) {
            if(zObjNowtime > zActivity_time) zObjNowtime = zActivity_time;
            zActivity_time += _time;
        }
    } else {
        if(zActivity_time && zObjNowtime > zActivity_time) zObjNowtime = zActivity_time;
        zActivity_time = 0;
    }
}
void GNg::cFRAME_LAUNCHER_::zResetState() {
#define GT_TMP_LAUNCHER_RESET_STATE \
    ztBright = 0;                   \
    ztBn = 0;                       \
    ztSn = 0;                       \
    ztSize = 0;                     \
    ztCn = 0;                       \
    zTr = 255;                      \
    zTg = 255;                      \
    zTb = 0;                        \
    zTa = 0;

    GT_TMP_LAUNCHER_RESET_STATE
    zObjNowtime = 0;
    zDisplay_num = 0;
}
void GNg::cFRAME_LAUNCHER_::ReSetPlaytime() {
    zResetState();
    zGen_num = 0;
    zGen_count = 0;
    //unsigned int i = 0;
    //for(; i<zMax_num; ++i)zTransf_s[i] = Ts2D;
    //memset(zColor_s,   0xff, zMax_num<<2);
    memset(zNowtime_s, 0xff, zMax_num << 2);
}
/************************************************* cFRAME_LAUNCHER ***************************************************/
GNg::cFRAME_LAUNCHER::cFRAME_LAUNCHER() : aGenRngX(0.0f), aGenRngY(0.0f), aGenRngW(256.0f), aGenRngH(256.0f) {}

GNg::cFRAME_LAUNCHER::cFRAME_LAUNCHER(const cFRAME_LAUNCHER &src) : cFRAME_LAUNCHER_(src), aGenRngX(src.aGenRngX), aGenRngY(src.aGenRngY), aGenRngW(src.aGenRngW), aGenRngH(src.aGenRngH) {}

GNg::cFRAME_LAUNCHER &GNg::cFRAME_LAUNCHER::operator=(const cFRAME_LAUNCHER &src) {
    this->cFRAME_LAUNCHER_::operator=(src);
    aGenRngX = src.aGenRngX;
    aGenRngY = src.aGenRngY;
    aGenRngW = src.aGenRngW;
    aGenRngH = src.aGenRngH;
    return *this;
}
void GNg::cFRAME_LAUNCHER::PerfectCopy(const cFRAME_LAUNCHER &src) {
    cFRAME_LAUNCHER_::PerfectCopy(src);
    aGenRngX = src.aGenRngX;
    aGenRngY = src.aGenRngY;
    aGenRngW = src.aGenRngW;
    aGenRngH = src.aGenRngH;
}
void GNg::cFRAME_LAUNCHER::SetRange(const sRECT2 *_range) {
    aGen_ts_flags = 0;
    aGenRngX = _range->left;
    aGenRngY = _range->top;
    aGenRngW = _range->right - _range->left;
    aGenRngH = _range->bottom - _range->top;
}
void GNg::cFRAME_LAUNCHER::GetRange(sRECT2 *range_out) {
    range_out->left = aGenRngX;
    range_out->top = aGenRngY;
    range_out->right = aGenRngX + aGenRngW;
    range_out->bottom = aGenRngY + aGenRngH;
}
/************************************************* cFRAME_LAUNCHER2D ***************************************************/
GNg::cFRAME_LAUNCHER2D::cFRAME_LAUNCHER2D() : zGenRngSclX(1.0f), zGenRngSclY(1.0f), zGenRngRot(0.0f), zGenRngAx(1.0f, 0.0f), zGenRngAy(0.0f, 1.0f) {}

GNg::cFRAME_LAUNCHER2D::cFRAME_LAUNCHER2D(const cFRAME_LAUNCHER2D &src) {
    zCopyData2(src, false);
}
GNg::cFRAME_LAUNCHER2D &GNg::cFRAME_LAUNCHER2D::operator=(const cFRAME_LAUNCHER2D &src) {
    this->cFRAME_LAUNCHER::operator=(src);
    zCopyData2(src, false);
    return *this;
}
void GNg::cFRAME_LAUNCHER2D::PerfectCopy(const cFRAME_LAUNCHER2D &src) {
    cFRAME_LAUNCHER::PerfectCopy(src);
    zCopyData2(src, true);
}
void GNg::cFRAME_LAUNCHER2D::zCopyData2(const cFRAME_LAUNCHER2D &src, bool cpy_state) {
    zGenRngSclX = src.zGenRngSclX;
    zGenRngSclY = src.zGenRngSclY;
    zGenRngRot = src.zGenRngRot;
    zGenRngAx = src.zGenRngAx;
    zGenRngAy = src.zGenRngAy;

    if(src.aAddEffectSeeds) {
        cFRAME_LAUNCHER2D *_src = (cFRAME_LAUNCHER2D *)&src;
        unsigned int si = 0, sn = _src->aAddEffectSeeds.Num();
        aAddEffectSeeds.New(sn);
        P<cSEED> *sdd = aAddEffectSeeds, *sds = _src->aAddEffectSeeds;
        if(cpy_state) {
            for(; si < sn; ++si) {
                sdd[si].New(1);
                sdd[si]->zPerfectCopy(*(sds[si]));
            }
        } else {
            cSEED *_sd;
            for(; si < sn; ++si) {
                sdd[si].New(1);
                _sd = sds[si];
                *(sdd[si]) = *(_sd);
                sdd[si]->zGenCount = 0;
                if(_sd->zChilds) sdd[si]->zCopyToChilds(_sd->zChilds, _sd->zChilds->zActivity_time);
            }
        }
    } else aAddEffectSeeds.Delete();
}
void GNg::cFRAME_LAUNCHER2D::ReSetPlaytime() {
    cFRAME_LAUNCHER_::ReSetPlaytime();
    if(aAddEffectSeeds) {
        P<cSEED> *sd_cur = aAddEffectSeeds;
        P<cSEED> *sd_end = sd_cur + aAddEffectSeeds.Num();
        for(; sd_cur < sd_end; ++sd_cur) (*sd_cur)->ResetChildsTime();
    }
}
void GNg::cFRAME_LAUNCHER2D::zSetPos(const cFRAME *f) {
    if(aGen_ts_flags & eMODULATE_COLOR) aGenColor = f->aLevel_color;
    zGenRngSclX = f->zFrame_ts.sx * f->aFrame_ts.sx;
    zGenRngSclY = f->zFrame_ts.sy * f->aFrame_ts.sy;
    zGenRngRot = f->zFrame_ts.r + f->aFrame_ts.r;
    zGenRngAx = *((sVEC2 *)&(f->aLevel_world.m11));
    zGenRngAy = *((sVEC2 *)&(f->aLevel_world.m21));
    sVEC2 *ts = (sVEC2 *)&(f->aLevel_world.m31);

#define GT_TMP_FRAME_LAUNCHER2D_RNG_XY_CPT                                     \
    sVEC2 lcp = zGenRngAx * (aGenRngW * 0.5f) + zGenRngAy * (aGenRngH * 0.5f); \
    aGenRngX = ts->x - lcp.x;                                                  \
    aGenRngY = ts->y - lcp.y;

    GT_TMP_FRAME_LAUNCHER2D_RNG_XY_CPT
}
void GNg::cFRAME_LAUNCHER2D::SetPos(const GNg::sTRANSF2D *ts) {
    sMAT33 tm;
    M33GetTransf2D(&tm, ts);
    zGenRngAx = *((sVEC2 *)&tm.m11);
    zGenRngAy = *((sVEC2 *)&tm.m21);
    zGenRngSclX = ts->sx;
    zGenRngSclY = ts->sy;
    zGenRngRot = ts->r;
    GT_TMP_FRAME_LAUNCHER2D_RNG_XY_CPT
}
void GNg::cFRAME_LAUNCHER2D::GetPos(GNg::sTRANSF2D *ts_out) {
    *((sVEC2 *)&(ts_out->x)) = GetCentre();
    ts_out->sx = zGenRngSclX;
    ts_out->sy = zGenRngSclY;
    ts_out->r = zGenRngRot;
}
void GNg::cFRAME_LAUNCHER2D::GetRange(sRECT2 *range_out) {
    if(aGen_ts_flags) { //GetRangeBoxFromVertexs
        float l = aGenRngX, t = aGenRngY, r = aGenRngX, b = aGenRngY;
        float vwx = zGenRngAx.x * aGenRngW, vwy = zGenRngAx.y * aGenRngW;
        float vhx = zGenRngAy.x * aGenRngH, vhy = zGenRngAy.y * aGenRngH;

        float px = vwx + aGenRngX, py = vwy + aGenRngY;
        if(px < l) l = px;
        else if(px > r) r = px;
        if(py < t) t = py;
        else if(py > b) b = py;

        px = vhx + aGenRngX, py = vhy + aGenRngY;
        if(px < l) l = px;
        else if(px > r) r = px;
        if(py < t) t = py;
        else if(py > b) b = py;

        px += vwx, py += vwy;
        if(px < l) l = px;
        else if(px > r) r = px;
        if(py < t) t = py;
        else if(py > b) b = py;

        range_out->left = l;
        range_out->right = r;
        range_out->top = t;
        range_out->bottom = b;

    } else GNg::cFRAME_LAUNCHER::GetRange(range_out);
}
void GNg::cFRAME_LAUNCHER2D::zPlayFrames(unsigned int updatetime) {
    if(aAddEffectSeeds) {
        P<cSEED> *sd_st = aAddEffectSeeds;
        P<cSEED> *sd_end = sd_st + aAddEffectSeeds.Num(), *sd_cur;
        cSEED *_sd;

        sTRANSF2D *tss = (sTRANSF2D *)zTransf_s;
        unsigned int ID = 0, time0;
        for(; ID < zMax_num; ++ID) {
            time0 = zNowtime_s[ID];
            GT_TMP_PLAY_FRAMES(time0)
            for(sd_cur = sd_st; sd_cur < sd_end; ++sd_cur) {
                _sd = *sd_cur;
                if(zNowtime_s[ID] > _sd->zBirthTime) {
                    if(time0 <= _sd->zBirthTime) { _sd->zCreateChild(zFrame, GetGenerateID(ID)); } /*通知生成子对象*/
                    else if(_sd->aFollow)
                        _sd->zSetChildPos(zFrame, GetGenerateID(ID));
                }
            }
        }
    }
    for(sd_cur = sd_st; sd_cur < sd_end; ++sd_cur) (*sd_cur)->UpdatePlay(updatetime);
}
else cFRAME_LAUNCHER_::zPlayFrames(updatetime);
}
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

#define GT_TMP_LAUNCHER_FONT_RE_OTHER_STATE \
    zTrackingState = 0;                     \
    zSrcAmMode = 0;                         \
    zIsGenEndStop = zActivity_time ? false : true;

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
/********************************************************* cLENS_FLARE ********************************************************************************/
static const char *GT_DefLensFlareFilePath = "effects/camerabright_"; //默认光环式样文件路径（assets/effects/，与仓库资源目录一致）

bool GNg::cLENS_FLARE::InitToData(char *imgfile_name, float size, unsigned int flash_interval, unsigned char flash_value, short ScreenW, short ScreenH) {
    zImg.Release();
    if(!imgfile_name) imgfile_name = GN_SysDir(GT_DefLensFlareFilePath);
    if(!ImageLoad(imgfile_name, 1, &zImg)) return false;
    zImg.width = (int)((float)zImg.width * size);
    zImg.height = (int)((float)zImg.height * size);
    zCp.x = (float)(ScreenW >> 1);
    zCp.y = (float)(ScreenH >> 1);
    zFlareColor = ((255 - flash_value) << 24) | 0x00ffffff;
    zEndtime = flash_interval;
    zNowtime = 0;
    zAm_s_e = false;
    return true;
}
void GNg::cLENS_FLARE::Display(unsigned int updatetime, float lightSource_x, float lightSource_y, unsigned int color, bool boost_up) {
    if(!zImg.texture) return;
    if(zNowtime > zEndtime) zAm_s_e = true;
    else if(zNowtime < updatetime) zAm_s_e = false;
    if(zAm_s_e) zNowtime -= updatetime;
    else zNowtime += updatetime;
    unsigned int level_color = LinearColorMultiplyRatio(0xffffffff, zFlareColor, zNowtime, zEndtime, color); //处理颜色融合
    sIMAGE *_img = &zImg;
    float dir_x = zCp.x - lightSource_x, dir_y = zCp.y - lightSource_y, bright_w, bright_h;

    //绘制镜头光环(纹理截取坐标U, V, U2, V2, 相对向量长度比率 , 大小 )
#define GT_DrawLensFlare(tu, tv, tue, tve, len_ratio, size)               \
    bright_w = (float)zImg.width * (tue - tu) size;                       \
    bright_h = (float)zImg.height * (tve - tv) size;                      \
    ImageDraw(_img,                                                       \
              (int)(dir_x * len_ratio - bright_w * 0.5f) + lightSource_x, \
              (int)(dir_y * len_ratio - bright_h * 0.5f) + lightSource_y, \
              (int)bright_w, (int)bright_h, level_color, 0, 2, tu * zImg.uRange, tv * zImg.vRange, tue * zImg.uRange, tve * zImg.vRange)

    GT_DrawLensFlare(0.0f, 0.0f, 0.776f, 0.776f, 0.0f, );
    GT_DrawLensFlare(0.776f, 0.0f, 0.816f, 0.042f, 0.4f, );
    GT_DrawLensFlare(0.776f, 0.084f, 0.888f, 0.195f, 0.8f, );
    GT_DrawLensFlare(0.776f, 0.042f, 0.816f, 0.084f, 1.0f, );
    GT_DrawLensFlare(0.888f, 0.084f, 1.0f, 0.195f, 1.2f, );
    GT_DrawLensFlare(0.776f, 0.195f, 0.888f, 0.306f, 1.50f, );
    GT_DrawLensFlare(0.776f, 0.306f, 0.984f, 0.514f, 1.6f, );
    GT_DrawLensFlare(0.888f, 0.084f, 1.0f, 0.195f, 2.05f, );
    GT_DrawLensFlare(0.776f, 0.514f, 0.94f, 0.676f, 2.45f, );
    if(boost_up) {
        GT_DrawLensFlare(0.0f, 0.776f, 0.103f, 0.879f, -0.3f, );
        GT_DrawLensFlare(0.103f, 0.776f, 0.327f, 1.0f, 0.5f, );
        GT_DrawLensFlare(0.327f, 0.776f, 0.553f, 1.0f, 0.96f, );
        GT_DrawLensFlare(0.553f, 0.776f, 0.776f, 1.0f, 1.35f, );
        GT_DrawLensFlare(0.776f, 0.776f, 1.0f, 1.0f, 1.7f, *1.5f);
        GT_DrawLensFlare(0.776f, 0.776f, 1.0f, 1.0f, 2.1f, *2.5f);
    }
}
void GNg::cLENS_FLARE_OBJ::PosDraw() {
    if(aShowFlags & 2) ImageDraw(0, (int)aSrcX, (int)aSrcY, 2, 2, aColor);
}
void GNg::cLENS_FLARE_OBJ::Display(unsigned int updatetime) {
    if(aShowFlags & 2) {
        int data_size;
        uint8 *scr_data = GNg::ViewCur->SaveToData(&data_size, (short)aSrcX, (short)aSrcY, 1, 1);
        if(scr_data) { /*模版代码:检测镜头光环可见性并显示它*/
            if(data_size >= 4) {
                unsigned int bcolor = *((unsigned int *)scr_data);
                if(GNg_GetCorrectColor(bcolor) == aColor)
                    aStyle->Display((float)updatetime / (float)(aStyle.Us()), aSrcX, aSrcY, aColor, aShowFlags & 1);
            }
            delete[] scr_data;
        }
    } else aStyle->Display((float)updatetime / (float)(aStyle.Us()), aSrcX, aSrcY, aColor, aShowFlags & 1);
}
/*******************************************************************************************************************************/
uint8 *GNg::sAM_EVENT::zDataSave(uint8 *wpos) {
    memcpy(wpos, &event_mode, 2);
    wpos += 2;
    memcpy(wpos, &start_time, 4);
    wpos += 4;
    memcpy(wpos, &segment_time, 4);
    wpos += 4;
    memcpy(wpos, &start_value, 4);
    wpos += 4;
    memcpy(wpos, &end_value, 4);
    wpos += 4;
    memcpy(wpos, &slinear_value, 4);
    wpos += 4;
    memcpy(wpos, &slinear_mode, 2);
    wpos += 2;
    return wpos;
}
uint8 *GNg::sAM_EVENT::zDataLoad(uint8 *rpow) {
    memcpy(&event_mode, rpow, 2);
    rpow += 2;
    memcpy(&start_time, rpow, 4);
    rpow += 4;
    memcpy(&segment_time, rpow, 4);
    rpow += 4;
    memcpy(&start_value, rpow, 4);
    rpow += 4;
    memcpy(&end_value, rpow, 4);
    rpow += 4;
    memcpy(&slinear_value, rpow, 4);
    rpow += 4;
    memcpy(&slinear_mode, rpow, 2);
    rpow += 2;
    return rpow;
}
uint8 *GNg::cFRAME_RC::zDataSave(uint8 *wpos, sREF_OBJSW *objs_collect, uint8 in_launc) {
    wpos = sAM_CTRL::zDataSave(wpos);
    unsigned int pointer_id, rcsize = sizeof(sRECT2);
    memcpy(wpos, &zRcLoc, rcsize);
    wpos += rcsize;
    memcpy(wpos, &zRcS, rcsize);
    wpos += rcsize;
    memcpy(wpos, &zRcS0, rcsize);
    wpos += rcsize;
    memcpy(wpos, &zRcS_w_num, 1);
    wpos += 1;
    memcpy(wpos, &zRcS_h_num, 1);
    wpos += 1;
    memcpy(wpos, &zTexOnceTime, 4);
    wpos += 4;
#ifdef _GN_SOUND_H_
    unsigned short overlap_num = zSound.GetOverlapNum();
    memcpy(wpos, &zSouStartTime, 4);
    wpos += 4;
    memcpy(wpos, &zSouEndTime, 4);
    wpos += 4;
    memcpy(wpos, &overlap_num, 2);
    wpos += 2;
#else
    pointer_id = 0;
    memcpy(wpos, &pointer_id, 4);
    wpos += 4;
    memcpy(wpos, &pointer_id, 4);
    wpos += 4;
    memcpy(wpos, &pointer_id, 2);
    wpos += 2;
#endif
    memcpy(wpos, &aBlendmode, 2);
    wpos += 2;
    if(in_launc) {
        unsigned short launc_tag = 0xffff;
        memcpy(wpos, &launc_tag, 2);
        wpos += 2;
    } else {
#ifdef _GN_SOUND_H_
        memcpy(wpos, &zTargetID, 2);
        wpos += 2;
#else
        //pointer_id = 0;
        memcpy(wpos, &pointer_id, 2);
        wpos += 2;
#endif
        memcpy(wpos, &aColor, 4);
        wpos += 4;
        unsigned int ts_size = sizeof(sTRANSF2D);
        memcpy(wpos, &aFrame_ts, ts_size);
        wpos += ts_size;
        memcpy(wpos, &zFrame_ts, ts_size);
        wpos += ts_size;
        *wpos = zFrameTsReItem;
        ++wpos;
    }
    if(zObjsKeep) {
        pointer_id = objs_collect->pkeeps.add_no_eq2(zObjsKeep);
    } else pointer_id = 0xffffffff; /*该值代表没引用指针*/
    memcpy(wpos, &pointer_id, 4);
    wpos += 4;

    if(zPlay_obj != &Image && in_launc != 2) {
        pointer_id = objs_collect->pimgs.add_no_eq2(zPlay_obj);
    } else pointer_id = 0xffffffff; /*该值代表没引用指针*/
    memcpy(wpos, &pointer_id, 4);
    wpos += 4;

    GT_TMP_ARRAY_WRITE_INDEX(objs_collect->pevs, zFEvent, objs_collect->evns.push_back(zEventcount))
    GT_TMP_ARRAY_WRITE_INDEX(objs_collect->pframes, (in_launc == 2 ? 0 : zSubFrame), objs_collect->fmns.push_back(zSubCount))
    GT_TMP_ARRAY_WRITE_INDEX(objs_collect->ptidxs, zTexIndex, objs_collect->tidxns.push_back(zTexIndexCount))
    pointer_id = 0xffffffff;
#ifdef _GN_SOUND_H_
    if(zSouObj != &GNa::Sound) pointer_id = objs_collect->psounds.add_no_eq2(zSouObj);
#endif
    memcpy(wpos, &pointer_id, 4);
    wpos += 4;
    return wpos;
}
uint8 *GNg::cFRAME_RC::zDataLoad(uint8 *rpos, sREF_OBJSR *objs_ref) {
    rpos = sAM_CTRL::zDataLoad(rpos);
    unsigned int pointer_id, rcsize = sizeof(sRECT2);
    memcpy(&zRcLoc, rpos, rcsize);
    rpos += rcsize;
    memcpy(&zRcS, rpos, rcsize);
    rpos += rcsize;
    memcpy(&zRcS0, rpos, rcsize);
    rpos += rcsize;
    memcpy(&zRcS_w_num, rpos, 1);
    rpos += 1;
    memcpy(&zRcS_h_num, rpos, 1);
    rpos += 1;
    memcpy(&zTexOnceTime, rpos, 4);
    rpos += 4;
#ifdef _GN_SOUND_H_
    unsigned short overlap_num;
    memcpy(&zSouStartTime, rpos, 4);
    rpos += 4;
    memcpy(&zSouEndTime, rpos, 4);
    rpos += 4;
    memcpy(&overlap_num, rpos, 2);
    rpos += 2;
#else
    rpos += 4;
    rpos += 4;
    rpos += 2;
#endif
    memcpy(&aBlendmode, rpos, 2);
    rpos += 2;
    unsigned short launc_tag;
    memcpy(&launc_tag, rpos, 2);
    rpos += 2;
    if(launc_tag != 0xffff) {
#ifdef _GN_SOUND_H_
        zTargetID = launc_tag;
#endif
        memcpy(&aColor, rpos, 4);
        rpos += 4;
        unsigned int ts_size = sizeof(sTRANSF2D);
        memcpy(&aFrame_ts, rpos, ts_size);
        rpos += ts_size;
        memcpy(&zFrame_ts, rpos, ts_size);
        rpos += ts_size;
        zFrameTsReItem = *rpos;
        ++rpos;
    }
    memcpy(&pointer_id, rpos, 4);
    rpos += 4;
    if(pointer_id != 0xffffffff) zObjsKeep = objs_ref->pkeeps[pointer_id];

    memcpy(&pointer_id, rpos, 4);
    rpos += 4;
    if(pointer_id == 0xffffffff) zPlay_obj = &Image;
    else zPlay_obj = objs_ref->pimgs[pointer_id];

    memcpy(&pointer_id, rpos, 4);
    rpos += 4;
    if(pointer_id != 0xffffffff) {
        zFEvent = objs_ref->pevs[pointer_id];
        zEventcount = objs_ref->evns[pointer_id];
    }
    memcpy(&pointer_id, rpos, 4);
    rpos += 4;
    if(pointer_id != 0xffffffff) {
        zSubFrame = objs_ref->pframes[pointer_id];
        zSubCount = objs_ref->fmns[pointer_id];
    }
    memcpy(&pointer_id, rpos, 4);
    rpos += 4;
    if(pointer_id != 0xffffffff) {
        zTexIndex = objs_ref->ptidxs[pointer_id];
        zTexIndexCount = objs_ref->tidxns[pointer_id];
    }
    memcpy(&pointer_id, rpos, 4);
    rpos += 4;
#ifdef _GN_SOUND_H_
    if(pointer_id == 0xffffffff) zSouObj = &GNa::Sound;
    else {
        zSouObj = (GNa::sSOUND *)objs_ref->psounds[pointer_id];
        zSound.InitToData(zSouObj, false, overlap_num);
    }
#endif
    zColor = 0xffffffff;
    zTexID = 0;
    return rpos;
}
uint8 *GNg::cFRAME_LAUNCHER_::zDataSave(uint8 *wpos, GN::sLIST<cFRAME *> &frames_collect, GN::sLIST<P<char>> &keep_obj_collec) {
    unsigned int pointer_id;
    GT_TMP_OBJECT_WRITE_INDEX(frames_collect, zFrame);
    GT_TMP_OBJECT_WRITE_INDEX(frames_collect, zFrame2);
    GT_TMP_OBJECT_WRITE_INDEX(keep_obj_collec, zObjsKeep);
    memcpy(wpos, &zActivity_time, 4);
    wpos += 4;
    memcpy(wpos, &zObjNowtime, 4);
    wpos += 4;
    memcpy(wpos, &zGen_num, 4);
    wpos += 4;
    memcpy(wpos, &zGen_speed, 4);
    wpos += 4;
    memcpy(wpos, &zMax_gen_speed, 4);
    wpos += 4;
    memcpy(wpos, &zGen_once_num, 2);
    wpos += 2;
    memcpy(wpos, &zMax_once_num, 2);
    wpos += 2;
    memcpy(wpos, &aGen_angle, 4);
    wpos += 4;
    memcpy(wpos, &aGenColor, 4);
    wpos += 4;
    memcpy(wpos, &aGen_ts_flags, 1);
    wpos += 1;
    memcpy(wpos, &aPose_range, 1);
    wpos += 1;
    memcpy(wpos, &aAngle_range, 1);
    wpos += 1;
    memcpy(wpos, &aSize_range, 1);
    wpos += 1;
    memcpy(wpos, &aColor_range, 1);
    wpos += 1;
    memcpy(wpos, &zRunUpdate, 1);
    wpos += 1;
    return wpos;
}
uint8 *GNg::cFRAME_LAUNCHER_::zDataLoad(uint8 *rpos, cFRAME **frame_refs, P<char> *keep_obj_refs) {
    unsigned int pointer_id;
    memcpy(&pointer_id, rpos, 4);
    rpos += 4;
    if(pointer_id != 0xffffffff) zFrame = frame_refs[pointer_id];
    memcpy(&pointer_id, rpos, 4);
    rpos += 4;
    if(pointer_id != 0xffffffff) zFrame2 = frame_refs[pointer_id];
    memcpy(&pointer_id, rpos, 4);
    rpos += 4;
    if(pointer_id != 0xffffffff) zObjsKeep = keep_obj_refs[pointer_id];
    unsigned int obj_time, gen_num;
    memcpy(&zActivity_time, rpos, 4);
    rpos += 4;
    memcpy(&obj_time, rpos, 4);
    rpos += 4;
    memcpy(&gen_num, rpos, 4);
    rpos += 4;
    memcpy(&zGen_speed, rpos, 4);
    rpos += 4;
    memcpy(&zMax_gen_speed, rpos, 4);
    rpos += 4;
    memcpy(&zGen_once_num, rpos, 2);
    rpos += 2;
    memcpy(&zMax_once_num, rpos, 2);
    rpos += 2;
    memcpy(&aGen_angle, rpos, 4);
    rpos += 4;
    memcpy(&aGenColor, rpos, 4);
    rpos += 4;
    memcpy(&aGen_ts_flags, rpos, 1);
    rpos += 1;
    memcpy(&aPose_range, rpos, 1);
    rpos += 1;
    memcpy(&aAngle_range, rpos, 1);
    rpos += 1;
    memcpy(&aSize_range, rpos, 1);
    rpos += 1;
    memcpy(&aColor_range, rpos, 1);
    rpos += 1;
    memcpy(&zRunUpdate, rpos, 1);
    rpos += 1;

    zEnd_time = zFrame ? zFrame->GetEndtime() : 0;
    zReAllocBuffer();
    zObjNowtime = obj_time;
    zGen_num = gen_num;
    return rpos;
}
uint8 *GNg::cFRAME_LAUNCHER::zDataSave(uint8 *wpos, GN::sLIST<cFRAME *> &frames_collect, GN::sLIST<P<char>> &keep_obj_collec) {
    wpos = GNg::cFRAME_LAUNCHER_::zDataSave(wpos, frames_collect, keep_obj_collec);
    memcpy(wpos, &aGenRngW, 4);
    wpos += 4;
    memcpy(wpos, &aGenRngH, 4);
    wpos += 4;
    memcpy(wpos, &aGenRngX, 4);
    wpos += 4;
    memcpy(wpos, &aGenRngY, 4);
    wpos += 4;
    return wpos;
}
uint8 *GNg::cFRAME_LAUNCHER::zDataLoad(uint8 *rpos, cFRAME **frame_refs, P<char> *keep_obj_refs) {
    rpos = GNg::cFRAME_LAUNCHER_::zDataLoad(rpos, frame_refs, keep_obj_refs);
    memcpy(&aGenRngW, rpos, 4);
    rpos += 4;
    memcpy(&aGenRngH, rpos, 4);
    rpos += 4;
    memcpy(&aGenRngX, rpos, 4);
    rpos += 4;
    memcpy(&aGenRngY, rpos, 4);
    rpos += 4;
    return rpos;
}
uint8 *GNg::cFRAME_LAUNCHER2D::zDataSave(uint8 *wpos, GN::sLIST<cFRAME *> &frames_collect, GN::sLIST<P<char>> &keep_obj_collec) {
    wpos = GNg::cFRAME_LAUNCHER::zDataSave(wpos, frames_collect, keep_obj_collec);
    memcpy(wpos, &zGenRngSclX, 4);
    wpos += 4;
    memcpy(wpos, &zGenRngSclY, 4);
    wpos += 4;
    memcpy(wpos, &zGenRngRot, 4);
    wpos += 4;
    memcpy(wpos, &zGenRngAx, 8);
    wpos += 8;
    memcpy(wpos, &zGenRngAy, 8);
    wpos += 8;

    unsigned int si = 0, len = aAddEffectSeeds.Num();
    memcpy(wpos, &len, 4);
    wpos += 4;
    if(len) { /*模版代码:写入筐单元附着子效果数据*/
        P<cSEED> *_seed = aAddEffectSeeds;
        cSEED *_sd;
        for(; si < len; ++si) {
            _sd = _seed[si];
            memcpy(wpos, &(_sd->GetBirthTime()), 4);
            wpos += 4;
            memcpy(wpos, &(_sd->aFollow), 1);
            wpos += 1;
            wpos = _sd->GetSourceStyle()->zDataSave(wpos, frames_collect, keep_obj_collec);
        }
    }
    return wpos;
}
uint8 *GNg::cFRAME_LAUNCHER2D::zDataLoad(uint8 *rpos, cFRAME **frame_refs, P<char> *keep_obj_refs) {
    rpos = GNg::cFRAME_LAUNCHER::zDataLoad(rpos, frame_refs, keep_obj_refs);
    memcpy(&zGenRngSclX, rpos, 4);
    rpos += 4;
    memcpy(&zGenRngSclY, rpos, 4);
    rpos += 4;
    memcpy(&zGenRngRot, rpos, 4);
    rpos += 4;
    memcpy(&zGenRngAx, rpos, 8);
    rpos += 8;
    memcpy(&zGenRngAy, rpos, 8);
    rpos += 8;

    unsigned int si = 0, len;
    memcpy(&len, rpos, 4);
    rpos += 4;
    if(len) { /*模本代码:读取图框发射器附着子效果数据*/
        bool follow;
        unsigned int btime;
        cFRAME_LAUNCHER2D tmp, *s0;
        aAddEffectSeeds.New(len);
        P<cSEED> *_seed = aAddEffectSeeds;
        for(; si < len; ++si) {
            memcpy(&btime, rpos, 4);
            rpos += 4;
            follow = *((bool *)rpos);
            rpos += 1;
            rpos = tmp.zDataLoad(rpos, frame_refs, keep_obj_refs);
            _seed[si].New(1);
            if(tmp.aAddEffectSeeds) {
                P<P<cSEED>> seed_bk = tmp.aAddEffectSeeds;
                tmp.aAddEffectSeeds.Delete();
                _seed[si]->InitToData(this, &tmp, btime, follow);
                s0 = _seed[si]->GetSourceStyle();
                s0->aAddEffectSeeds = seed_bk;
            } else {
                _seed[si]->InitToData(this, &tmp, btime, follow);
            }
        }
    }
    return rpos;
}
uint8 *GNg::sDRAW_ARRANGE::zDataSave(uint8 *wpos) {
    memcpy(wpos, &zFontMapFormat, 1);
    wpos += 1;

    memcpy(wpos, &(zParame.rect_fill), 1);
    wpos += 1;
    memcpy(wpos, &(zParame.near_pointU), 4);
    wpos += 4;
    memcpy(wpos, &(zParame.near_pointV), 4);
    wpos += 4;
    memcpy(wpos, &(zParame.i_compartment), 2);
    wpos += 2;
    memcpy(wpos, &(zParame.j_compartment), 2);
    wpos += 2;
    memcpy(wpos, &(zParame.width), 2);
    wpos += 2;
    memcpy(wpos, &(zParame.height), 2);
    wpos += 2;
    memcpy(wpos, &(zParame.auto_enter), 1);
    wpos += 1;
    memcpy(wpos, &(zParame.erect_print), 1);
    wpos += 1;
    memcpy(wpos, &(zParame.overturn_print), 1);
    wpos += 1;

    memcpy(wpos, &zTu_w, 4);
    wpos += 4;
    memcpy(wpos, &zTv_h, 4);
    wpos += 4;
    return wpos;
}
uint8 *GNg::sDRAW_ARRANGE::zDataLoad(uint8 *rpos) {
    memcpy(&zFontMapFormat, rpos, 1);
    rpos += 1;

    memcpy(&(zParame.rect_fill), rpos, 1);
    rpos += 1;
    memcpy(&(zParame.near_pointU), rpos, 4);
    rpos += 4;
    memcpy(&(zParame.near_pointV), rpos, 4);
    rpos += 4;
    memcpy(&(zParame.i_compartment), rpos, 2);
    rpos += 2;
    memcpy(&(zParame.j_compartment), rpos, 2);
    rpos += 2;
    memcpy(&(zParame.width), rpos, 2);
    rpos += 2;
    memcpy(&(zParame.height), rpos, 2);
    rpos += 2;
    memcpy(&(zParame.auto_enter), rpos, 1);
    rpos += 1;
    memcpy(&(zParame.erect_print), rpos, 1);
    rpos += 1;
    memcpy(&(zParame.overturn_print), rpos, 1);
    rpos += 1;

    memcpy(&zTu_w, rpos, 4);
    rpos += 4;
    memcpy(&zTv_h, rpos, 4);
    rpos += 4;
    return rpos;
}
uint8 *GNg::cTEXT_ART::zDataSave(uint8 *wpos, GN::sLIST<GNg::sIMAGE *> &img_collect) {
    wpos = sDRAW_ARRANGE::zDataSave(wpos);
    unsigned int pointer_id = 0xffffffff;
    if(zpFontMap) {
        if(zpFontMap == &Image) pointer_id = 0xfffffffe;
        else if(zpFontMap == GN::RunRec.aFont.GetFontMap()) pointer_id = 0xfffffffd;
        else pointer_id = img_collect.add_no_eq2(zpFontMap);
    }
    memcpy(wpos, &pointer_id, 4);
    wpos += 4;
    return wpos;
}
uint8 *GNg::cTEXT_ART::zDataLoad(uint8 *rpos, P<GNg::sIMAGE *> &img_refs) {
    rpos = sDRAW_ARRANGE::zDataLoad(rpos);
    unsigned int pointer_id = *((unsigned int *)rpos);
    rpos += 4;
    if(pointer_id == 0xffffffff) zpFontMap = 0;
    else if(pointer_id == 0xfffffffe) zpFontMap = &Image;
    else if(pointer_id == 0xfffffffd) zpFontMap = GN::RunRec.aFont.GetFontMap();
    else if(pointer_id < (unsigned int)img_refs.Num() && img_refs[pointer_id])
        zpFontMap = img_refs[pointer_id];
    else zpFontMap = GN::RunRec.aFont.GetFontMap(); // fallback: 字体缺失时用默认字体
    return rpos;
}
uint8 *GNg::cFRAME_LAUNCHER_FONT::zTextSave(uint8 *wpos) {
    if(zIsFontMap) {
        GetText((char *)(wpos + 4), zMax_num);
        ((unsigned int *)wpos)[0] = strlen((char *)(wpos + 4));
        wpos += 4 + ((unsigned int *)wpos)[0];
    } else {
        ((unsigned int *)wpos)[0] = zMax_num;
        wpos += 4;
        float u_inv = 1.0f / zTu_w;
        sDRAW_RANGE *pd_text = zDrawRngData;
        unsigned int dri = 0;
        for(; dri < zMax_num; ++dri) wpos[dri] = (uint8)(pd_text[dri].u1 * u_inv + 48.0f);
        wpos += zMax_num;
    }
    return wpos;
}
uint8 *GNg::cFRAME_LAUNCHER_FONT::zDataSave(uint8 *wpos, GN::sLIST<cFRAME *> &frames_collect, GN::sLIST<P<char>> &keep_obj_collect, bool inc_text) {
    unsigned int acttime_bk = zActivity_time;
    if(zIsGenEndStop) zActivity_time = 0;
    unsigned int pointer_id = 0;
    memcpy(wpos, zFrame ? &zFrame->aColor : &pointer_id, 4);
    wpos += 4;
    memcpy(wpos, zActFrame ? &zActFrame->aColor : &pointer_id, 4);
    wpos += 4;
    GT_TMP_OBJECT_WRITE_INDEX(frames_collect, zFontFrame);
    GT_TMP_OBJECT_WRITE_INDEX(frames_collect, zActFrame);
    wpos = cFRAME_LAUNCHER::zDataSave(wpos, frames_collect, keep_obj_collect);
    wpos = sDRAW_ARRANGE::zDataSave(wpos);
    memcpy(wpos, &ztCU, 4);
    wpos += 4;
    memcpy(wpos, &ztCV, 4);
    wpos += 4;
    memcpy(wpos, &zIsFontMap, 1);
    wpos += 1;
    memcpy(wpos, &zActMouseAuto, 1);
    wpos += 1;
    memcpy(wpos, &zActMouseKey, 1);
    wpos += 1;
    memcpy(wpos, &zGen_once_numB, 2);
    wpos += 2;
    zActivity_time = acttime_bk;

    if(inc_text) { //找出当前显示字符串
        wpos = zTextSave(wpos);
    } else {
        ((unsigned int *)wpos)[0] = 0;
        wpos += 4;
    }
    return wpos;
}
uint8 *GNg::cFRAME_LAUNCHER_FONT::zDataLoad(uint8 *rpos, cFRAME **frame_refs, P<char> *keep_obj_refs, bool set_text) {
    unsigned int pointer_id, *fm_color = (unsigned int *)rpos;
    rpos += 8;
    memcpy(&pointer_id, rpos, 4);
    rpos += 4;
    if(pointer_id != 0xffffffff) zFontFrame = frame_refs[pointer_id];
    memcpy(&pointer_id, rpos, 4);
    rpos += 4;
    if(pointer_id != 0xffffffff) zActFrame = frame_refs[pointer_id];
    rpos = cFRAME_LAUNCHER::zDataLoad(rpos, frame_refs, keep_obj_refs);
    rpos = sDRAW_ARRANGE::zDataLoad(rpos);
    memcpy(&ztCU, rpos, 4);
    rpos += 4;
    memcpy(&ztCV, rpos, 4);
    rpos += 4;
    memcpy(&zIsFontMap, rpos, 1);
    rpos += 1;
    memcpy(&zActMouseAuto, rpos, 1);
    rpos += 1;
    memcpy(&zActMouseKey, rpos, 1);
    rpos += 1;
    memcpy(&zGen_once_numB, rpos, 2);
    rpos += 2;

    if(zFrame) zFrame->aColor = fm_color[0];
    if(zActFrame) zActFrame->aColor = fm_color[1];
    if(zFontFrame) zEnd_time = zFontFrame->GetEndtime();
    GT_TMP_LAUNCHER_FONT_RE_OTHER_STATE

    memcpy(&pointer_id, rpos, 4);
    rpos += 4;
    if(pointer_id) {
        if(set_text) SetText((char *)rpos, pointer_id);
        rpos += pointer_id;
    }
    return rpos;
}
GN_API uint8 *GT_WriteImageNameToBuffer(uint8 *wpos, GNg::sIMAGE *_img, char *img_name /*[64]*/, bool texn_out = false);
GN_API void GT_ConvertImageNameForLoad(char *img_name_out, const char *img_name, unsigned int name_len);

uint8 *GNg::cLENS_FLARE::zDataSave(uint8 *wpos, char *imgexp_path) {
    if(zImg.fileName && !strcmp(zImg.fileName, GN_SysDir(GT_DefLensFlareFilePath))) {
        *((unsigned int *)wpos) = 0xffffffff;
        wpos += 4;
    } else {
        char img_name[64];
        wpos = GT_WriteImageNameToBuffer(wpos, &zImg, img_name);
        if(imgexp_path) GNg::ExportImageFile(imgexp_path, &zImg);
    }
    short imgw = zImg.width, imgh = zImg.height;
    memcpy(wpos, &imgw, 2);
    wpos += 2;
    memcpy(wpos, &imgh, 2);
    wpos += 2;
    memcpy(wpos, &zEndtime, 4);
    wpos += 4;
    memcpy(wpos, &zFlareColor, 4);
    wpos += 4;
    return wpos;
}
uint8 *GNg::cLENS_FLARE::zDataLoad(uint8 *rpos, char *load_path_) {
    unsigned int img_name_len;
    memcpy(&img_name_len, rpos, 4);
    rpos += 4; //读图象名长度
    if(img_name_len == 0xffffffff) zImg.InitToData(GN_SysDir(GT_DefLensFlareFilePath));
    else {
        if(img_name_len > 1) {
            GT_ConvertImageNameForLoad(load_path_ + strlen(load_path_), (char *)rpos, img_name_len - 1);
            zImg.InitToData(load_path_);
        }
        rpos += img_name_len;
    }
    short imgw, imgh;
    memcpy(&imgw, rpos, 2);
    rpos += 2;
    memcpy(&imgh, rpos, 2);
    rpos += 2;
    memcpy(&zEndtime, rpos, 4);
    rpos += 4;
    memcpy(&zFlareColor, rpos, 4);
    rpos += 4;
    zImg.width = imgw;
    zImg.height = imgh;
    zNowtime = 0;
    zAm_s_e = false;
    zCp.x = (float)(GNg::ViewCur->zBackScreenW >> 1);
    zCp.y = (float)(GNg::ViewCur->zBackScreenH >> 1);
    return rpos;
}
uint8 *GNg::cLENS_FLARE_OBJ::zDataSave(uint8 *wpos, GN::sLIST<P<cLENS_FLARE>> &flare_collect) {
    unsigned int pointer_id;
    GT_TMP_OBJECT_WRITE_INDEX(flare_collect, aStyle);

    memcpy(wpos, &aSrcX, 4);
    wpos += 4;
    memcpy(wpos, &aSrcY, 4);
    wpos += 4;
    memcpy(wpos, &aColor, 4);
    wpos += 4;
    memcpy(wpos, &aShowFlags, 1);
    wpos += 1;
    return wpos;
}
uint8 *GNg::cLENS_FLARE_OBJ::zDataLoad(uint8 *rpos, P<P<cLENS_FLARE>> &flare_refs) {
    unsigned int pointer_id;
    memcpy(&pointer_id, rpos, 4);
    rpos += 4;
    if(pointer_id != 0xffffffff) aStyle = flare_refs[pointer_id];

    memcpy(&aSrcX, rpos, 4);
    rpos += 4;
    memcpy(&aSrcY, rpos, 4);
    rpos += 4;
    memcpy(&aColor, rpos, 4);
    rpos += 4;
    memcpy(&aShowFlags, rpos, 1);
    rpos += 1;
    return rpos;
}
#endif // _GN_FRAME_H_