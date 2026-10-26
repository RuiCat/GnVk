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
#ifdef _GN_AUDIO_H_

GN_API bool GNa::SoundLoad(char *filename, sSOUND *sound, bool pointsound) {
    unsigned short chls, bit, fmt = GNf::GetExtNamePos(filename);
    unsigned int frq, sie;
    uint8 *data = FileAudio->LoadData(filename, &fmt, &chls, &bit, &frq, &sie);
    if(!data) {
        GN_Log("调用SoundLoad:%s 失败!\n", filename);
        return false;
    }
    if(sound->GNa_sSOUND::InitToData(data, fmt, chls, bit, frq, sie, pointsound)) {
        sound->time = ComputePlayTime(chls, bit, frq, sie); //计算到音频播放时间
        GN_StrcpyData(sound->fileName, filename);
        return true;
    } else {
        GN_Log("调用SoundLoad:%s 数据传输有错!\n", filename);
        return false;
    }
}
/********************************************************** sSOUND ****************************************************************/
GNa::sSOUND::sSOUND() : time(0), fileName(0) {}
GNa::sSOUND::~sSOUND() {
    GN_DeleteArray(fileName);
}

GN_API GNa::sSOUND GNa::Sound;

void GNa::sSOUND::InitToData(char *filename, bool pointsound) {
    Release();
    SoundLoad(filename, this, pointsound);
}
/********************************************************** cBGM_MUSIC ************************************************************/
GNa::cBGM_MUSIC::cBGM_MUSIC() : zSize(0), ztTime(0), zState(eBGM_NO_DATA), zSpec(0), zLoad(0) {}

void GNa::cBGM_MUSIC::Release() {
    zSpec = 0;
    if(zLoad) {
        zLoad->Close();
        zLoad = 0;
    }
    GT_cSOUND_STREAM::Release();
    zState = eBGM_NO_DATA;
}
GNa::cBGM_MUSIC::~cBGM_MUSIC() {
    this->Release();
}

bool GNa::cBGM_MUSIC::InitToData(char *_filename, int _loop) {
    if(!_filename || !_filename[0]) return false;
    zProgtg = -1.0f;
    zVolume = 1.0f;
    ztVoltg = 0;
    ztVol = 100000000;
    zPitch = 1.0f;
    ztPittg = 0;
    ztPit = 100000000;
    zLoop_n = _loop;
    zLoop = _loop;

    zFormat = GNf::GetExtNamePos(_filename);
    zLoad = FileAudio->Open(_filename, &zFormat, &zChannel, &zBit, &zFreq, &zSize, GNf::sFILE::DefaultType);
    if(!zLoad) {
        GN_Log("调用 cBGM_MUSIC::InitToData 失败! 找不到文件或不支持的格式: %s !\n", GNf::sFILE::DefaultType == GNf::eFILE_STD ? _filename : "内容");
        return false;
    }
    ztSize = 0;
    zBufferSize = zFreq; //缓冲大小
    zUnitCount = zSize / zChannel / (zBit >> 3); //计算出总样本数

    zTime = (unsigned int)((double)zUnitCount / (double)zFreq * 1000.0); //计算音频总时间
    return this->zInit();
}
bool GNa::cBGM_MUSIC::zInit() {
    iFILE_AUDIO::cSTREAM *load = zLoad;
    zLoad = 0; //使能不关闭文件
    this->Release(); //清除释放缓冲对象和临时内存 以防中途换音频出现问题
    zLoad = load;
    if(zInitStream(zBufferSize, zFormat, zChannel, zBit, zFreq, zVolume, zPitch)) {
        zState = eBGM_PLAY_BEGIN;
        return true;
    }
    GN_Log("调用 cBGM_MUSIC::zInit 有错!!\n");
    return false;
}
unsigned int GNa::cBGM_MUSIC::zOnLoad(char *data) {
    if(zLoop && zState != eBGM_STOP) {
        ztTime = (unsigned int)((float)zTime * ((float)ztSize / (float)zSize)); //计算刷新当前播放到的时间
        unsigned int tzt_size = ztSize;
        unsigned int read_size = zLoad->Read(data, zBufferSize);
        ztSize += read_size;
        if(!read_size || ztSize >= zSize) { //数据播完或最后一段
            zLoad->Seek(0.0f);
            ztSize = 0; //进度恢复为0

            --zLoop;
            if(zLoop < -10000) zLoop = -1; //使能循环播放
            zLoop_n = zLoop;
        }
        return read_size;
    }
    return 0;
}
GNa::eBGM_PLAY_STATE GNa::cBGM_MUSIC::UpdataPlay(unsigned int updatetime) {
    if(zState == eBGM_STOP) return zState;
    zState = zUpdateStream(updatetime, zLoop ? true : false); //更新音频流
    if(zState == eBGM_UPDATA_ERROR) {
        GN_Log("调用 cBGM_MUSIC::UpdataPlay 有错!\n");
        return zState;
    } else if(zState == eBGM_NO_DATA) return zState;

    if(zProgtg > -0.0001f) { //重设更新播放进度
        if(zProgtg > 0.999999f) zProgtg = 0.999999f;
        zLoad->Seek(zProgtg);
        ztSize = (unsigned int)(zSize * zProgtg); //计算出所剩还没播放的数据长度
        zProgtg = -1.0f;
    }
    if(ztVol <= ztVoltg) { //控制音量调整延迟
        ztVol += updatetime;
        float vol_ratio = (ztVol < ztVoltg) ? (float)ztVol / (float)ztVoltg : 1.0f;
        zVolume = zVolst + (zVoltg - zVolst) * vol_ratio;
        zSetVolume(zVolume);
    }
    if(ztPit <= ztPittg) { //控制音调调整延迟
        ztPit += updatetime;
        float pit_ratio = (ztPit < ztPittg) ? (float)ztPit / (float)ztPittg : 1.0f;
        zPitch = zPitst + (zPittg - zPitst) * pit_ratio;
        zSetPitch(zPitch);
    }
    return zState;
}
void GNa::cBGM_MUSIC::Pause() {
    zLoop = zLoop ? 0 : zLoop_n;
}

void GNa::cBGM_MUSIC::Stop() {
    if(zState == eBGM_PLAY_ING) zState = eBGM_STOP;
}
void GNa::cBGM_MUSIC::AddPlayCount(int _loop) {
    if(zLoop >= 0 && zLoop_n >= 0) zLoop_n = zLoop += _loop;
}
void GNa::cBGM_MUSIC::Play() {
    if(zLoop) {
        zLoad->Seek(0.0f);
        ztSize = 0;
        this->zInit();
    } else zLoop = zLoop_n;
}
void GNa::cBGM_MUSIC::SetStage(float _state) {
    if(_state >= -0.0f && _state <= 1.0f) zProgtg = _state;
}
void GNa::cBGM_MUSIC::SetVolume(float value, int s_time) {
    if(value > 1.0f) value = 1.0f;
    else if(value < 0.0f) value = 0.0f;
    zVoltg = value;
    zVolst = zVolume;
    ztVoltg = s_time;
    ztVol = 0;
}
void GNa::cBGM_MUSIC::SetPitch(float value, int s_time) {
    if(GN_HasSoundAccelerate()) {
        zPittg = value;
        zPitst = zPitch;
        ztPittg = s_time;
        ztPit = 0;
    } else {
        zPitch = value;
        zSetPitch(zPitch);
    }
}
void GNa::cBGM_MUSIC::SyncTime(unsigned int tgtime) {
    unsigned int intval_time = (zBufferSize * 1000 / zFreq) / (zSize / zUnitCount); //计算音频缓冲时间间隔
    if(tgtime + intval_time <= zTime) {
        int temptime = (int)((int)ztTime) - ((int)tgtime);
        if(temptime < -100 /*进度慢了*/ || temptime > (int)intval_time /*进度快了*/)
            zProgtg = ((float)(tgtime + intval_time)) / (float)zTime;
    }
}
#ifdef GN_USING_ADDITIONAL_LIB
bool GNa::cBGM_MUSIC::SetSpecFormat(int spec_num, int proc_ch, int proc_compart, float LOtoHI_ratio, int max_LOhz, int max_HIhz) {
    return 1;
}
int GNa::cBGM_MUSIC::GetSpecData(float *spec_data_out) {
    return 0;
}
#endif
/***********************************************************************************************************************************/
GN_API void GN_SoundPlayBegin2D() {
    GT_ReSetSoundMode2D();
}
GN_API void GN_SoundPlayBegin3D() {
    GT_ReSetSoundMode3D();
}
GN_API void GN_InitSound() {
    if(GT_SystemInited & (1 << 3)) return;
    if(!GT_InitSound()) GN_Log("调用 GN_InitSound 初始化声音设备失败!\n");
    GT_SystemInited |= 1 << 3;
}
GN_API void GN_UnInitSound() {
    if(!GT_UnInitSound()) GN_Log("调用 GN_UnInitSound 释放声音设备资源失败!\n");
    GT_SystemInited -= 1 << 3;
}
#endif //_GN_AUDIO_H_