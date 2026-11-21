// GN_FrameLauncher.cpp — cFRAME 与 cFRAME_LAUNCHER/cFRAME_LAUNCHER2D（2D 图框、发射器）。
#include "GN.h"
#include "GN_Def.h"
#include "GN_FrameInternal.h"
#ifdef _GN_FRAME_H_
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
#endif //_GN_FRAME_H_
