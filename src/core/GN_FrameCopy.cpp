// GN_FrameCopy.cpp — cFRAME_RC 的逐字段复制：operator=（浅共享）与 CopyData（深复制）
// 共用的成员拷贝。替代原实现的 memcpy/逐字节覆写（对含虚函数与非平凡成员的类属 UB）。
// 只复制标量、普通指针与 POD 成员；安全指针 zNodes/zObjsKeep 与音频发声点 zSound
// 由调用方按浅/深语义单独处理（zSound 持有 OpenAL 发声源，共享会导致双重释放）。
#include <GN.h>

#ifdef _GN_FRAME_H_

void GNg::cFRAME_RC::zCopyFields(const cFRAME_RC &src) {
    // ── sAM_CTRL 基类 ──
    zRuntime = src.zRuntime;
    zNowtime = src.zNowtime;
    zEndtime = src.zEndtime;
    zStarttime = src.zStarttime;
    zAm_mode = src.zAm_mode;
    zAm_s_e = src.zAm_s_e;
    zHasTRS = src.zHasTRS;
    zPlaying = src.zPlaying;
    zSegmentID = src.zSegmentID;
    ztAm_mode = src.ztAm_mode;
    ztEndtime = src.ztEndtime;
    zTargetTime = src.zTargetTime;
    zStartRunTime = src.zStartRunTime;
    // ── cFRAME_RC ──
    zFrame_ts = src.zFrame_ts;
    zRcLoc = src.zRcLoc;
    zRcS0 = src.zRcS0;
    zRcS = src.zRcS;
    zColor = src.zColor;
    zTexID = src.zTexID;
    zRcS_w_num = src.zRcS_w_num;
    zRcS_h_num = src.zRcS_h_num;
    zFEvent = src.zFEvent;
    zEventcount = src.zEventcount;
    zSubFrame = src.zSubFrame;
    zSubCount = src.zSubCount;
    zTexIndex = src.zTexIndex;
    zTexIndexCount = src.zTexIndexCount;
    zTexOnceTime = src.zTexOnceTime;
    zPlay_obj = src.zPlay_obj;
#ifdef _GN_SOUND_H_
    zSouObj = src.zSouObj;
    zSouStartTime = src.zSouStartTime;
    zSouEndTime = src.zSouEndTime;
#endif
    zTargetID = src.zTargetID;
    zFrameTsReItem = src.zFrameTsReItem;
    // ── 用户接口区 ──
    aLevel_range = src.aLevel_range;
    aLevel_ts = src.aLevel_ts;
    aLevel_color = src.aLevel_color;
    aFrame_ts = src.aFrame_ts;
    aColor = src.aColor;
    aBlendmode = src.aBlendmode;
}

#endif //_GN_FRAME_H_
