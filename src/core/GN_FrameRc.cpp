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
#include "GN_FrameInternal.h"
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
    if(this == &src) return *this;
    // 逐字段复制（zCopyFields），安全指针按浅共享语义重新挂接；zSound 不复制：
    // 它持有 OpenAL 发声源，原字节拷贝会让两个对象共享同一发声源并在析构时双重释放。
    this->Release();
    zNodes.Delete();
    zObjsKeep.Delete();
    zCopyFields(src);
    zNodes = src.zNodes;
    zObjsKeep = src.zObjsKeep;
    return *this;
}
void GNg::cFRAME_RC::CopyData(const cFRAME_RC &src) {
    if(this == &src) return;
    this->Release();
    zNodes.Delete();
    zObjsKeep.Delete();
    zCopyFields(src);
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
    // 发声点不随复制共享（原 memcpy 会留下悬空 OpenAL 发声源）：释放本对象的发声源，
    // 由调用方在需要时用 InitSoundToData 重新初始化。
    zSouObj = &GNa::Sound;
    zSound.Release();
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
    GT_TMP_FRAME_PLAY_SUB(, PlayFrame)

    GT_LEVEL_RANGE_CPT;
    int dw = (int)(aLevel_range.right - aLevel_range.left);
    int dh = (int)(aLevel_range.bottom - aLevel_range.top);

    if(aBlendmode && (aLevel_color & 0xff000000)) {
        /*处理纹理截图控制信息*/

        GT_TMP_FRAME_TEXCOORD_RNG_CPT
        ImageDraw(zPlay_obj, (int)aLevel_range.left, (int)aLevel_range.top, dw, dh, aLevel_color, zTexID, aBlendmode, uv_left, uv_top, uv_right, uv_bottom);
    }
#ifdef _GN_SOUND_H_
    //处理声音定位( 图筐中心X , 图筐中心Y , 图框缩放比例 )
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
#endif //_GN_FRAME_H_
