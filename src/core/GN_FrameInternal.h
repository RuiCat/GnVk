// GN_FrameInternal.h — 2D 帧动画模块内部共享宏与 helper（GN_Frame.cpp 拆分后共用）。
#ifndef _GN_FRAME_INTERNAL_H_
#define _GN_FRAME_INTERNAL_H_
#include "GN.h"
#include "GN_Def.h"

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

inline void GT_AmEvsStartEndValueSwap(GNg::sAM_EVENT *evs, unsigned int evn) {
    GNg::sAM_EVENT *ev_end = evs + evn;
    for(; evs < ev_end; ++evs) GN_Swap(float, evs->start_value, evs->end_value);
}

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
#define GT_TMP_FRAME_TEXCOORD_RNG_CPT                                      \
    zRcS0.left += zRcS0.right * updatetime; /*处理纹理自动移动动画的偏移*/ \
    zRcS0.top += zRcS0.bottom * updatetime;                                \
    float uv_left = zRcS.left + zRcS0.left; /*整合两个UV范围*/             \
    float uv_top = zRcS.top + zRcS0.top;                                   \
    float uv_right = uv_left + zRcS.right;                                 \
    float uv_bottom = uv_top + zRcS.bottom;
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
#define GT_TMP_FRAME_ZT_BRIGHT_UPDATE                                                 \
    if(zFrame2) {                                                                     \
        if(zGen_speed == zMax_gen_speed) { /*处理光亮增加*/                           \
            ztBright += (short)(768 * zGen_once_num / zMax_num); /*直接一下加到最大*/ \
            if(ztBright > 255) ztBright = 255;                                        \
        } else ztBn = (uint8)(zEnd_time / 30 / updatetime + 1); /*带有缓动的增加*/    \
    }
#define GT_TMP_LAUNCHER_FONT_RE_OTHER_STATE \
    zTrackingState = 0;                     \
    zSrcAmMode = 0;                         \
    zIsGenEndStop = zActivity_time ? false : true;
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

#endif //_GN_FRAME_INTERNAL_H_
