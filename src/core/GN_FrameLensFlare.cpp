// GN_FrameLensFlare.cpp — cLENS_FLARE/cLENS_FLARE_OBJ 与镜头光环序列化。
#include "GN.h"
#include "GN_Def.h"
#include "GN_FrameInternal.h"
#ifdef _GN_FRAME_H_
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
GN_API int GT_ConvertImageNameForLoad_s(char *img_name_out, int cap, const char *img_name, unsigned int name_len);

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
    // 旧 ABI：场景装载的唯一调用点传 file_path[384]（GN_2Dscene.cpp GT_Scene2DLoadShareObjData）。
    return zDataLoad(rpos, load_path_, 384);
}
uint8 *GNg::cLENS_FLARE::zDataLoad(uint8 *rpos, char *load_path_, int load_path_cap) {
    unsigned int img_name_len;
    memcpy(&img_name_len, rpos, 4);
    rpos += 4; //读图象名长度
    if(img_name_len == 0xffffffff) zImg.InitToData(GN_SysDir(GT_DefLensFlareFilePath));
    else {
        if(img_name_len > 1) {
            // 加固（STRCPY 审计 B5i）：按 load_path_cap 截断，避免把图片名续写越出调用方路径缓冲。
            int base = (int)strlen(load_path_);
            if(base < load_path_cap) {
                GT_ConvertImageNameForLoad_s(load_path_ + base, load_path_cap - base, (char *)rpos, img_name_len - 1);
                zImg.InitToData(load_path_);
            }
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
#endif //_GN_FRAME_H_
