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

#include<GN.h>
#ifdef  _GN_SOUND_H_
#if defined _WIN32_ || defined _WIN32_WCE //----------- 使用 MMSYSTEM 音效 ------------------------------------------------------
  #include<windows.h>
 #ifdef _WIN32_
  #include<mmsystem.h>
  #pragma comment(lib,"winmm.lib")
 #else
  #include<mmsystem.h>
  #pragma comment(lib,"mmtimer.lib")
 #endif
 #ifndef _WIN32_WCE
  struct GT_s_SBUF{
	  char         *buffer;      //音频数据指针
      unsigned int  buffersize;  //音频数据大小
	  WAVEFORMATEX  soundFMT;    //音频格式
  };
  bool GT_SoundInitError = false;
 #endif
#endif
/*
#ifdef _WIN32_
#define _WIN32_WCE  //要求windows平台强制使用mmsystem音效
#endif
*/
#ifndef _WIN32_WCE //-------------------------使用 OpenAL 3D音效--------------------------------------------------------
 #ifdef _LINUX_
  #include <linux/soundcard.h>

  #include <AL/al.h>
  #include <AL/alc.h>
  //#include <AL/alu.h>
  //#include <AL/alut.h>
 #elif defined _MAC_ || defined GT_USING_IOS
  #include <soundcard.h>
  #include <OpenAL/al.h>
  #include <OpenAL/alc.h>
 #else //_WIN32_
  #include <AL/al.h>
  #include <AL/alc.h>
//#include <AL/alu.h>
//#include <AL/alut.h>
  #pragma comment(lib,"OpenAL32.lib")
//  #pragma comment(lib,"alut.lib")
 #endif
#endif
//------------------------------------------------------ GNa_sSOUND -----------------------------------------------------------
GNa_sSOUND::GNa_sSOUND():z__attrb(0){}
GNa_sSOUND::~GNa_sSOUND(){ Release(); }

bool GNa_sSOUND::InitToData(unsigned char *data,unsigned short format,unsigned short channels,unsigned short bit,unsigned int freq,unsigned int size,bool pointsound){
    Release();
   #ifndef _WIN32_WCE
    ALuint *buf = new unsigned int;
	z__attrb = buf;
    alGenBuffers(1, buf);
	if(alGetError() != AL_NO_ERROR){
        Release();
		delete [] data;
		return false;
	}
	format = (bit==8 ? (channels==1?AL_FORMAT_MONO8:AL_FORMAT_STEREO8) : (channels==1?AL_FORMAT_MONO16:AL_FORMAT_STEREO16)); /*模版代码:分析音频格式*/
	if(pointsound && channels != 1){
	   ALenum tformat;
	   uint8 *tdata = 0;
       int  tsize;
       int di = 0;
	   switch(format){ //把双声道取左声道数据转为单声道 才能用到3D定位音效
	   case AL_FORMAT_STEREO8:
		   tsize = size >> 1;
		   tdata = new uint8[tsize];
           for(;di < tsize; ++di)tdata[di] = data[di<<1];
           tformat = AL_FORMAT_MONO8;
		   break;
	   case AL_FORMAT_STEREO16:{
           tsize = size >> 1;
		   tdata = new unsigned char[tsize];
		   unsigned short *tdata2 = (unsigned short*)tdata, *tdata1 = (unsigned short*)data;
		   int tsize2 = tsize >> 1;
		   for(;di < tsize2; ++di)tdata2[di] = tdata1[di<<1];
           tformat = AL_FORMAT_MONO16;
		   }break;
	   } 
	      alBufferData(*buf,tformat,tdata,tsize, freq); //传输数据到OpenAL
	   if(tdata)delete []tdata;							//释放转换用临时数据
	}else alBufferData(*buf, format, data, size, freq);
	
	delete [] data;
	if(alGetError() != AL_NO_ERROR){
        Release();
	    return false;
	}
   #else
    GT_s_SBUF *buf = new GT_s_SBUF;
	z__attrb = buf;
    buf->soundFMT.wFormatTag = format;
    buf->soundFMT.nChannels = channels;
    buf->soundFMT.wBitsPerSample = bit;
    buf->soundFMT.nSamplesPerSec = freq;
    buf->buffersize = size;
    buf->buffer = (char*)data;
	buf->soundFMT.nBlockAlign = channels * bit/8;
	buf->soundFMT.nAvgBytesPerSec = buf->soundFMT.nBlockAlign * freq;
   #endif
	return true;
}
void GNa_sSOUND::Release(){
   #ifndef _WIN32_WCE
    ALuint *buf =  (ALuint*)z__attrb;
    if(buf){
	    if(buf[0])alDeleteBuffers(1, buf);
		delete buf;
        z__attrb = 0;
	}
   #else
    GT_s_SBUF *buf =  (GT_s_SBUF*)z__attrb;
    if(buf){
		if(buf->buffer){ delete [] buf->buffer; }
		delete buf;
        z__attrb = 0;
	}
   #endif
}
#ifdef GN_HAS_ALSA_VOLUME
#include <alsa/asoundlib.h>
#endif
/************************************************* 类 cSOUND_SRC ***************************************************/
GNa::cSOUND_SRC::cSOUND_SRC():z__attrb(0){}
GNa::cSOUND_SRC::~cSOUND_SRC(){ Release(); }

#ifndef _WIN32_WCE
struct GT_sSSRC					   //发声源附加属性
{
    ALuint         *zSource;          //发声源组
	uint8          *zPauseState;	  //记录各个声音源当前是否暂停中
	unsigned short  zCount;           //个数

	//获取指定ID位置上的发声源( ID )
	inline ALuint GetSrc(unsigned short id){ return (zCount == 0xffff? (ALuint)((uintptr_t)zSource) : zSource[id % zCount]); }
};
void GNa::cSOUND_SRC::Release(){
	GT_sSSRC *sr = (GT_sSSRC*)z__attrb;
	if(sr){
		if(sr->zCount == 0xffff){
			ALuint src = (ALuint)((uintptr_t)(sr->zSource));
			alSourceStop(src);
			alDeleteSources(1, &src);
		}else{
			ALuint *srcs = sr->zSource;
			unsigned short di = 0, dn = sr->zCount;
			for(; di < dn; ++di)alSourceStop(srcs[di]);
			alDeleteSources(dn, srcs);
			delete [] sr->zPauseState;
		}
		delete sr; 
		z__attrb = 0;
	}
}
void GNa::cSOUND_SRC::InitToData(const GNa_sSOUND *sound,bool playloop,unsigned short overlap_num,float volume,float pitch){
	if(!(sound && sound->z__attrb))return;
	Release();
	const bool pitch_change  = pitch  > 1.001f || pitch  < 0.999f;
	const bool volume_change = volume > 1.001f || volume < 0.999f;
	ALuint *buf =  (ALuint*)sound->z__attrb;

    GT_sSSRC *sr = new GT_sSSRC;
	z__attrb = sr;
	if(overlap_num < 2){
		sr->zCount = 0xffff;
		ALuint sid;
		alGenSources(1, &sid);
		sr->zSource = (ALuint*)sid;
		sr->zPauseState = (uint8*)1;

	   #define GT_TMP_SET_SOUND_SRC_STATE \
		alSourcei(sid, AL_BUFFER,  *buf);\
        alSourcei(sid, AL_LOOPING, playloop);\
		if(pitch_change) alSourcef(sid, AL_PITCH, pitch);\
        if(volume_change)alSourcef(sid, AL_GAIN,  volume);\
		/*alSourcefv(sid, AL_POSITION, SourcePos[2]);*/
        /*alSourcefv(sid, AL_VELOCITY, SourceVel[2]);*/
		/*alSourcef (sid, AL_ROLLOFF_FACTOR, 0.0f);    //该值影响变弱的覆盖距离 当值为0(默认值)将关闭它 此时无论声源多远 都将听到声音*/
		/*alSourcei (sid, AL_SOURCE_RELATIVE,AL_TRUE); //位置是否与听众有联系,默认为AL_FLASE*/

		GT_TMP_SET_SOUND_SRC_STATE
	}else{
		sr->zCount = overlap_num;
		uint8 *pste = new uint8[overlap_num+(overlap_num<<2)];
		unsigned int *srcs = (unsigned int*)(pste + overlap_num), sid;
		sr->zPauseState = pste;
		sr->zSource = srcs;
		alGenSources(overlap_num, srcs);
		unsigned short i = 0;
		for(; i < overlap_num; ++i){
			sid = srcs[i];
			GT_TMP_SET_SOUND_SRC_STATE
			pste[i] = 1;
		}
	}
}
void GNa::cSOUND_SRC::Play(unsigned short ID){
	if(z__attrb)alSourcePlay(((GT_sSSRC*)z__attrb)->GetSrc(ID));
}
void GNa::cSOUND_SRC::Stop(unsigned short ID){
	if(z__attrb)alSourceStop(((GT_sSSRC*)z__attrb)->GetSrc(ID));
}
void GNa::cSOUND_SRC::Pause(unsigned short ID){
	if(z__attrb){
		GT_sSSRC *sr = (GT_sSSRC *)z__attrb;
		if(sr->zCount == 0xffff){
			if(sr->zPauseState    )alSourcePause((ALuint)((uintptr_t)(sr->zSource)));
			else				   alSourcePlay ((ALuint)((uintptr_t)(sr->zSource)));
			sr->zPauseState = (uint8*)(sr->zPauseState?0:1);
		}else{
			unsigned short id = ID % sr->zCount;
			if(sr->zPauseState[id])alSourcePause(sr->zSource[id]);
			else				   alSourcePlay (sr->zSource[id]);
			sr->zPauseState[id] = !sr->zPauseState[id];
		}
	}
}
void GNa::cSOUND_SRC::Rewind(unsigned short ID){
	if(z__attrb)alSourceRewind(((GT_sSSRC*)z__attrb)->GetSrc(ID));
}
void GNa::cSOUND_SRC::SetVolume(unsigned short ID,float value){
	if(z__attrb)alSourcef(((GT_sSSRC*)z__attrb)->GetSrc(ID), AL_GAIN, value);
}
void GNa::cSOUND_SRC::SetPitch (unsigned short ID,float value){
	if(z__attrb)alSourcef(((GT_sSSRC*)z__attrb)->GetSrc(ID), AL_PITCH, value);
}
void GNa::cSOUND_SRC::SetPosi(unsigned short ID,float x,float y,float z){
	if(z__attrb)alSource3f(((GT_sSSRC*)z__attrb)->GetSrc(ID), AL_POSITION, x,y,z);
}
void GNa::cSOUND_SRC::SetVelocity(unsigned short ID,float vx,float vy,float vz){
	if(z__attrb)alSource3f(((GT_sSSRC*)z__attrb)->GetSrc(ID), AL_VELOCITY, vx,vy,vz);
}
#else
  struct GT_s_SSRC{
      WAVEHDR	      *zWHDR_Ptr;   //指向音频缓存的结构
      HWAVEOUT		  *zSource;     //声音源
      unsigned short   zCount;      //个数
      bool		      *zIsPlaying;  //临时处理用
      bool            *zPauseState; //临时处理用
      bool            *zRewindLock; //临时处理用

	  GT_s_SSRC():
		  zWHDR_Ptr   ( 0),
		  zSource     ( 0),
		  zCount      ( 0),
          zIsPlaying  ( 0),
		  zPauseState ( 0),
		  zRewindLock ( 0)
	  {}
	 ~GT_s_SSRC(){
	      this->Release();
	  }
	  void zStop(unsigned short ID){
	      if(!zIsPlaying[ID])return;
	      if(waveOutReset(zSource[ID])!= MMSYSERR_NOERROR)GN_Log("sound_source stop Reset error!!\n");
	      if(waveOutUnprepareHeader(zSource[ID],&zWHDR_Ptr[ID],sizeof(WAVEHDR)) != MMSYSERR_NOERROR)GN_Log("sound_source stop UnprepareHeader error!!\n");
	      zIsPlaying[ID] = false;
	  }
	  void Release(){
	      if(zSource){
			  unsigned short di;
		      for(di=0; di<zCount; di++){
				    this->zStop(di);
			        waveOutClose(zSource[di]);
			  }
			  delete []zSource;
              zSource = 0;
          }
		  GN_DeleteArray(zIsPlaying);
		  GN_DeleteArray(zPauseState);
		  GN_DeleteArray(zRewindLock);
	      GN_DeleteArray(zWHDR_Ptr);
	  }
  };
void GNa::cSOUND_SRC::Release(){
	GT_s_SSRC *sr = (GT_s_SSRC *)z__attrb;
	if(sr){
        delete sr;
		z__attrb = 0;
	}
}
void GNa::cSOUND_SRC::InitToData(const GNa_sSOUND *sound,bool playloop,unsigned short overlap_num,float volume,float pitch){
	if(GT_SoundInitError || !sound || !sound->z__attrb)return;
	this->Release();
	GT_s_SBUF *sb = (GT_s_SBUF *)sound->z__attrb;
    GT_s_SSRC *sr = new GT_s_SSRC;
	z__attrb = sr;
	sr->zCount = overlap_num;
	sr->zSource = new HWAVEOUT[overlap_num];

	WAVEFORMATEX soundFmt;
	soundFmt.wFormatTag = sb->soundFMT.wFormatTag;
	soundFmt.nChannels  = sb->soundFMT.nChannels;
	soundFmt.wBitsPerSample= sb->soundFMT.wBitsPerSample;
	soundFmt.nBlockAlign = soundFmt.nChannels * soundFmt.wBitsPerSample/8;
	if(pitch>1.001f||pitch<0.999f){
		if(pitch>4.35f)pitch = 4.35f;
	    if(pitch<0.003f)pitch= 0.003f;
		soundFmt.nSamplesPerSec= (DWORD)(sb->soundFMT.nSamplesPerSec * pitch);
	}else soundFmt.nSamplesPerSec= sb->soundFMT.nSamplesPerSec;
	soundFmt.nAvgBytesPerSec = soundFmt.nBlockAlign * soundFmt.nSamplesPerSec;
	MMRESULT res;
	unsigned short di;
	int ri;
	for(di=0; di < overlap_num; di++){
		res = waveOutOpen(&sr->zSource[di],WAVE_MAPPER,&soundFmt,0,0, CALLBACK_NULL);
	    if (res != MMSYSERR_NOERROR){
		    GN_Log("sound_source init error!!\n");
			for(ri = (int)di-1; ri >= 0; ri--)waveOutClose(sr->zSource[ri]);
			delete []sr->zSource; sr->zSource = 0;
            delete sr; z__attrb = 0;
			return;
	    }
	}
	DWORD flags,loop;
	if(playloop){
	    flags = WHDR_BEGINLOOP | WHDR_ENDLOOP;
	    loop = 10000000;
	}else{
		flags = 0;
		loop  = 0;
	}
	sr->zIsPlaying = new bool[overlap_num];
	sr->zPauseState= new bool[overlap_num];
	sr->zRewindLock= new bool[overlap_num];
	sr->zWHDR_Ptr  = new WAVEHDR[overlap_num];
	for(di = 0; di < overlap_num; di++){
        sr->zIsPlaying[di] = 0;
		sr->zPauseState[di]= 1;
		sr->zRewindLock[di]= 0;
        sr->zWHDR_Ptr[di].dwFlags = flags;
        sr->zWHDR_Ptr[di].dwLoops = loop;
        sr->zWHDR_Ptr[di].lpData  = sb->buffer;
        sr->zWHDR_Ptr[di].dwBufferLength = sb->buffersize;
	}
}
void GNa::cSOUND_SRC::Play(unsigned short ID){
	//检测一下引用到的数据和ID是否正确
   #define GT_TEMP_SSRC_ID_CHECK \
	if(!z__attrb)return;\
    GT_s_SSRC *sr = (GT_s_SSRC *)z__attrb;\
	unsigned short id = ID % sr->zCount

    GT_TEMP_SSRC_ID_CHECK;
	UINT whdr_size = sizeof(WAVEHDR);
	if(sr->zIsPlaying[id])sr->zStop(id);
	if(waveOutPrepareHeader(sr->zSource[id], &(sr->zWHDR_Ptr[id]),whdr_size) != MMSYSERR_NOERROR){ GN_Log("sound_source play PrepareHeader error!!\n"); return; }
    if(waveOutWrite        (sr->zSource[id], &(sr->zWHDR_Ptr[id]),whdr_size) != MMSYSERR_NOERROR){ GN_Log("sound_source play Write error!!\n");         return; }
    sr->zIsPlaying[id] = true;
}
void GNa::cSOUND_SRC::Stop(unsigned short ID){
	GT_TEMP_SSRC_ID_CHECK;
	if(sr->zRewindLock[id])return;
	if(!sr->zIsPlaying[id])return;
	if(waveOutReset(sr->zSource[id])!= MMSYSERR_NOERROR)GN_Log("sound_source stop Reset error!!\n");
	if(waveOutUnprepareHeader(sr->zSource[id],&(sr->zWHDR_Ptr[id]),sizeof(WAVEHDR)) != MMSYSERR_NOERROR)GN_Log("sound_source stop UnprepareHeader error!!\n");
    sr->zIsPlaying[id] = false;
}
void GNa::cSOUND_SRC::Pause(unsigned short ID){
    GT_TEMP_SSRC_ID_CHECK;
	if(sr->zRewindLock[id])return;
	if(sr->zPauseState[id])waveOutPause  (sr->zSource[id]);
	else                   waveOutRestart(sr->zSource[id]);
    sr->zPauseState[id] = !(sr->zPauseState[id]);
}
void GNa::cSOUND_SRC::Rewind(unsigned short ID){
	GT_TEMP_SSRC_ID_CHECK;
	sr->zRewindLock[id] = !(sr->zRewindLock[id]);
}
void GNa::cSOUND_SRC::SetVolume(unsigned short ID,float value){
	GT_TEMP_SSRC_ID_CHECK;
	if (value > 1.0f)value = 1.0f;
	if (value < 0.0f)value = 0.0f;
	WORD wNewVolume = (WORD)(value * 65535.0f);
	waveOutSetVolume(sr->zSource[id],(wNewVolume<<16)|wNewVolume );
}
void GNa::cSOUND_SRC::SetPitch (unsigned short ID,float value){
	GT_TEMP_SSRC_ID_CHECK;
	DWORD pitch = (DWORD)(65536.0f*value);
	waveOutSetPitch(sr->zSource[id],pitch);
}
void GNa::cSOUND_SRC::SetPosi(unsigned short ID,float x,float y,float z){}
void GNa::cSOUND_SRC::SetVelocity(unsigned short ID,float vx,float vy,float vz){}
#endif
unsigned short GNa::cSOUND_SRC::GetOverlapNum(){
	if(z__attrb){
		GT_sSSRC *sr = (GT_sSSRC*)z__attrb;
		return (sr->zCount == 0xffff ? 1 : sr->zCount);
	}
	return 0;
}
/******************************************************** GT_cSOUND_STREAM **************************************************************/
struct GT_sSSBUF	  //音频流缓冲器
{
   #ifndef _WIN32_WCE
    ALuint            zBuffer[2];     //轮替播放的双缓冲号
	ALuint            zSource;        //发声源
    unsigned short    zFmt;           //AL的音频格式
	char             *zTempData;      //用于接收数据的缓冲
   #else
    WAVEHDR           zBuffer[2];     //双缓冲交换实现流播放
	HWAVEOUT          zSource;        //声音关联的源
	WAVEFORMATEX      zFmt;           //MMSystem的音频格式结构
	float             zVolume;        //音量
   #endif
	unsigned int      zFreq;          //音频采样率
    bool              zIsPlayBegin;   //是否准备开始播放
    int8              zBn;            //待播放分段数

    void Release(){
       #ifndef _WIN32_WCE
		if(zSource){
			alSourceStop(zSource);
			alSourceUnqueueBuffers(zSource, 2, zBuffer);
			alDeleteSources(1, &zSource);
			alDeleteBuffers(2, zBuffer);
            zSource = 0; zBn = 0;
		}
		GN_DeleteArray(zTempData);
       #else
        if(zBuffer[0].lpData || zBuffer[1].lpData){
			UINT wavhdr_len = sizeof(WAVEHDR);

			if(zBuffer[0].lpData){
				waveOutUnprepareHeader(zSource,&zBuffer[0],wavhdr_len);
                delete []zBuffer[0].lpData;
				zBuffer[0].lpData = 0 ;
			}
			if(zBuffer[1].lpData){
				waveOutUnprepareHeader(zSource,&zBuffer[1],wavhdr_len);
                delete []zBuffer[1].lpData;
				zBuffer[1].lpData = 0 ;
			}
			zBn = 0;
			waveOutReset(zSource);
			if(waveOutClose(zSource) != MMSYSERR_NOERROR)GN_Log("BGM_MUSIC.Release waveOutClose error!\n");
		}
       #endif
    }
	GT_sSSBUF()
	   #ifndef _WIN32_WCE
        :zSource(0),zTempData( 0)
       #endif
	{
	   #ifdef _WIN32_WCE
        zBuffer[0].lpData = 0;
        zBuffer[1].lpData = 0;
       #endif
	}
   ~GT_sSSBUF(){ Release(); }
};
/*----------------------------------------------------------------------------------------------------------------------------------*/
GT_cSOUND_STREAM::GT_cSOUND_STREAM():z__attrb(0){};
GT_cSOUND_STREAM::~GT_cSOUND_STREAM(){ Release(); }

void GT_cSOUND_STREAM::Release(){
    GT_sSSBUF *ssb = (GT_sSSBUF*)z__attrb;
	if(ssb){ delete ssb; z__attrb = 0; }
}
#ifdef _WIN32_WCE
//waveout数据播放完时的回调  播放流的关键
void CALLBACK GT_WaveOutProc(HWAVEOUT hwo,UINT uMsg,DWORD_PTR dwInstance,DWORD_PTR dwParam1,DWORD_PTR dwParam2){
	GT_sSSBUF *ssbuf = (GT_sSSBUF*)(((GT_cSOUND_STREAM*)dwInstance)->z__attrb);
	if(uMsg == WOM_DONE && ssbuf->zBn >= 0){
		LPWAVEHDR pWaveHeader = (LPWAVEHDR)dwParam1;
	    UINT wavhdr_len = sizeof(WAVEHDR);
	    if(pWaveHeader->dwBufferLength){     //释放用完的数据
			waveOutUnprepareHeader(hwo,pWaveHeader,wavhdr_len);
			pWaveHeader->dwBufferLength = 0;
			ssbuf->zBn--; //标记已播放完一段缓冲
		}
        pWaveHeader->dwBufferLength = ((GT_cSOUND_STREAM*)dwInstance)->zOnLoad(pWaveHeader->lpData);
		if(pWaveHeader->dwBufferLength){
            if(ssbuf->zVolume > 1.001f || ssbuf->zVolume < 0.999f){  //动态调整音量处理
			    if(ssbuf->zFmt.wBitsPerSample == 8){
			        GN_MemRatio(unsigned char,pWaveHeader->lpData,pWaveHeader->dwBufferLength,ssbuf->zVolume);
                }else if(ssbuf->zFmt.wBitsPerSample == 16){
                    GN_MemRatio(short,pWaveHeader->lpData,pWaveHeader->dwBufferLength,ssbuf->zVolume);
			    }
			}
			waveOutPrepareHeader(hwo, pWaveHeader, wavhdr_len);
            waveOutWrite(hwo, pWaveHeader, wavhdr_len);
			ssbuf->zBn++;
		}
	}
}
#endif
bool GT_cSOUND_STREAM::zInitStream(unsigned int buf_max_size, unsigned short format, unsigned short channels, unsigned short bit, unsigned int freq, float volume, float pitch){
    GT_sSSBUF *ssb = new GT_sSSBUF;
    ssb->zFreq = freq;
	ssb->zBn = 0;
   #ifndef _WIN32_WCE
	alGenBuffers(2,   ssb->zBuffer);     //分配内存缓冲
	if (alGetError() != AL_NO_ERROR){ delete ssb; return false; }
	ALuint sid;
	alGenSources(1, &sid);
	if (alGetError() != AL_NO_ERROR){ delete ssb; return false; }
	ssb->zSource = sid;
	ssb->zFmt = (bit==8 ? (channels==1?AL_FORMAT_MONO8:AL_FORMAT_STEREO8) : (channels==1?AL_FORMAT_MONO16:AL_FORMAT_STEREO16)); /*模版代码:分析音频格式*/
	ssb->zTempData = new char[buf_max_size];

	//先读两段数据
    unsigned int load_size = zOnLoad(ssb->zTempData);
	if(load_size)alBufferData(ssb->zBuffer[0], ssb->zFmt, ssb->zTempData, load_size, freq);
	
				 load_size = zOnLoad(ssb->zTempData);
    if(load_size)alBufferData(ssb->zBuffer[1], ssb->zFmt, ssb->zTempData, load_size, freq);
	
	alSourceQueueBuffers(sid, 2, ssb->zBuffer);

    //设置音量与效果等
	if(pitch  > 1.001f || pitch  < 0.999f)alSourcef(sid, AL_PITCH, pitch);  //音调 值范围 4.35f 至 0.003f
    if(volume > 1.001f || volume < 0.999f)alSourcef(sid, AL_GAIN,  volume); //音量 值范围 1.0f  至 0.0f 不能为负数 否则会出错
  //alSourcefv(sid, AL_POSITION, SourcePos[0]); //声源位置
  //alSourcefv(sid, AL_VELOCITY, SourceVel[0]); //声源速度
    alSourcei(sid, AL_LOOPING,  AL_FALSE );     //是否循环
   #else
	if(GT_SoundInitError)return false;
	 WAVEFORMATEX *_fmt = &(ssb->zFmt);
	_fmt->wFormatTag = format;
	_fmt->nChannels  = channels;
	_fmt->wBitsPerSample= bit;
	_fmt->nBlockAlign = _fmt->nChannels * _fmt->wBitsPerSample/8;
	if(pitch > 1.001f || pitch < 0.999f)_fmt->nSamplesPerSec= (DWORD)(freq * pitch);
	else _fmt->nSamplesPerSec= freq;
	_fmt->nAvgBytesPerSec = _fmt->nBlockAlign * _fmt->nSamplesPerSec;

	if(waveOutOpen(&(ssb->zSource),WAVE_MAPPER, _fmt,(DWORD)(GT_WaveOutProc),(DWORD)(this),CALLBACK_FUNCTION)!= MMSYSERR_NOERROR){
		GN_Log("BGM_MUSIC init error!!\n");
		waveOutClose(ssb->zSource);
		delete ssb;
		return false;
	}
	ssb->zBuffer[0].lpData  = new char[buf_max_size];
    ssb->zBuffer[0].dwFlags = 0;
    ssb->zBuffer[0].dwLoops = 0;
    ssb->zBuffer[1].lpData  = new char[buf_max_size];
    ssb->zBuffer[1].dwFlags = 0;
    ssb->zBuffer[1].dwLoops = 0;

	//先读两节数据
	load_size = zOnLoad(ssb->zBuffer[0].lpData);
	if(load_size){
	    ssb->zBuffer[0].dwBufferLength = load_size;
	}
    load_size = zOnLoad(ssb->zBuffer[1].lpData);
	if(load_size){
	    ssb->zBuffer[1].dwBufferLength = load_size;
	}
    ssb->zVolume = volume;

	//调整音量缓冲数值
    if(volume > 1.001f || volume < 0.999f){ //动态调整音量处理
		if(bit == 8){
			if(ssb->zBuffer[0].dwBufferLength){ GN_MemRatio(unsigned char,ssb->zBuffer[0].lpData,ssb->zBuffer[0].dwBufferLength, volume); }
			if(ssb->zBuffer[1].dwBufferLength){ GN_MemRatio(unsigned char,ssb->zBuffer[1].lpData,ssb->zBuffer[1].dwBufferLength, volume); }
        }else if(bit == 16){
			if(ssb->zBuffer[0].dwBufferLength){ GN_MemRatio(short,ssb->zBuffer[0].lpData,ssb->zBuffer[0].dwBufferLength, volume); }
			if(ssb->zBuffer[1].dwBufferLength){ GN_MemRatio(short,ssb->zBuffer[1].lpData,ssb->zBuffer[1].dwBufferLength, volume); }
		}
    }
   #endif
	ssb->zIsPlayBegin = true;
    z__attrb = ssb;
    return true;
}
GNa::eBGM_PLAY_STATE GT_cSOUND_STREAM::zUpdateStream(unsigned int updatetime, bool play_ing){
	if(!z__attrb)return GNa::eBGM_NO_DATA;
    GT_sSSBUF *ssb = (GT_sSSBUF*)z__attrb;

   #ifndef _WIN32_WCE
	if(ssb->zIsPlayBegin){
		ssb->zBn = 2;
	    alSourcePlay(ssb->zSource); //开始播放
        ssb->zIsPlayBegin = false;
        return GNa::eBGM_PLAY_ING;
	}
	int proce;
    alGetSourcei(ssb->zSource, AL_BUFFERS_PROCESSED, &proce);  //检查缓冲队列正在处理到那一个
	if(proce == 1){
       unsigned int load_size = zOnLoad(ssb->zTempData);
	   if(load_size){
            ALuint rbuf;
		    alSourceUnqueueBuffers(ssb->zSource, 1, &rbuf);
		    alBufferData(rbuf, ssb->zFmt, ssb->zTempData, load_size, ssb->zFreq);
            alSourceQueueBuffers(ssb->zSource, 1, &rbuf);
	   }
	}else if(proce > 1){  //卡停或播完的处理
	    alSourceStop(ssb->zSource);
		if(play_ing){
			ssb->zBn = 2;
			alSourcePlay(ssb->zSource); //还需再播放
		}else{
			ssb->zBn = 0;
            return GNa::eBGM_PAUSE;		//两段缓冲都播完了还未更新数据就返回暂停
		}
	}
   #else
	if(ssb->zIsPlayBegin){ //开始播放
	    UINT whdr_size = sizeof(WAVEHDR);
		if(waveOutPrepareHeader(ssb->zSource,&(ssb->zBuffer[0]),whdr_size) != MMSYSERR_NOERROR){ return GNa::eBGM_UPDATA_ERROR; }
        if(waveOutWrite        (ssb->zSource,&(ssb->zBuffer[0]),whdr_size) != MMSYSERR_NOERROR){ return GNa::eBGM_UPDATA_ERROR; }
		if(waveOutPrepareHeader(ssb->zSource,&(ssb->zBuffer[1]),whdr_size) != MMSYSERR_NOERROR){ return GNa::eBGM_UPDATA_ERROR; }
        if(waveOutWrite        (ssb->zSource,&(ssb->zBuffer[1]),whdr_size) != MMSYSERR_NOERROR){ return GNa::eBGM_UPDATA_ERROR; }
        ssb->zBn = 2;
		ssb->zIsPlayBegin = false;
        return GNa::eBGM_PLAY_ING;
	}
	if(ssb->zBn <= 0){ //再播放
		if(play_ing){
		    if(!ssb->zBuffer[0].dwBufferLength)GT_WaveOutProc(ssb->zSource,WOM_DONE,(DWORD)(this),(DWORD)(&(ssb->zBuffer[0])),0);
		    if(!ssb->zBuffer[1].dwBufferLength)GT_WaveOutProc(ssb->zSource,WOM_DONE,(DWORD)(this),(DWORD)(&(ssb->zBuffer[1])),0);
		}else{
			//ssb->zBn = 0;
			return GNa::eBGM_PAUSE; //两段缓冲都播完还未更新数据就返回是暂停
		}
	}
   #endif
	return GNa::eBGM_PLAY_ING;
}
void GT_cSOUND_STREAM::zSetVolume(float value){
    GT_sSSBUF *ssb = (GT_sSSBUF*)z__attrb;
   #ifndef _WIN32_WCE
    alSourcef(ssb->zSource, AL_GAIN, value);
   #else
    ssb->zVolume = value;
   #endif
}
void GT_cSOUND_STREAM::zSetPitch (float value){
    GT_sSSBUF *ssb = (GT_sSSBUF*)z__attrb;
   #ifndef _WIN32_WCE
    alSourcef(ssb->zSource, AL_PITCH, value);
   #else
	WAVEFORMATEX *_fmt = &(ssb->zFmt);
	if(value > 1.001f || value < 0.999f)_fmt->nSamplesPerSec = (DWORD)(ssb->zFreq * value);
	else _fmt->nSamplesPerSec = ssb->zFreq;
	_fmt->nAvgBytesPerSec = _fmt->nBlockAlign * _fmt->nSamplesPerSec;

	if(ssb->zIsPlayBegin){ //未开始播放
		waveOutClose(ssb->zSource);
	    waveOutOpen(&(ssb->zSource),WAVE_MAPPER,_fmt,(DWORD)(GT_WaveOutProc),(DWORD)(this),CALLBACK_FUNCTION);

	}else{ //开始了播放就要 Reset UnprepareHeader
	    UINT wavhdr_len = sizeof(WAVEHDR);
        int8 _bn = ssb->zBn;
		ssb->zBn = -1;
		waveOutReset(ssb->zSource);
	    waveOutUnprepareHeader(ssb->zSource,&(ssb->zBuffer[0]),wavhdr_len);
		waveOutUnprepareHeader(ssb->zSource,&(ssb->zBuffer[1]),wavhdr_len);
        waveOutClose(ssb->zSource);
	    waveOutOpen(&(ssb->zSource),WAVE_MAPPER,_fmt,(DWORD)(GT_WaveOutProc),(DWORD)(this),CALLBACK_FUNCTION);
		if(_bn > 0){
			waveOutPrepareHeader(ssb->zSource,&(ssb->zBuffer[0]),wavhdr_len);
            waveOutWrite        (ssb->zSource,&(ssb->zBuffer[0]),wavhdr_len);
			waveOutPrepareHeader(ssb->zSource,&(ssb->zBuffer[1]),wavhdr_len);
            waveOutWrite        (ssb->zSource,&(ssb->zBuffer[1]),wavhdr_len);
            ssb->zBn = 2;
		}else ssb->zBn = 0;
	}
   #endif
}
unsigned int GT_cSOUND_STREAM::zOnLoad(char *data){ return 0; }

//------------------------------------------------------ 系统音量设置 ----------------------------------------------------------------
#if defined GT_USING_GLUT   && defined _MAC_
#include<GL/mac_ext.h>
#endif
#if defined GT_USING_ANDROID_JNI && defined _GN_MAIN_H_
extern thread_local JNIEnv *GT_JEnv;
extern JavaVM    *GT_Jvm;
extern jclass     GT_Japp;
extern jmethodID  GT_J_SetSystemVol;
#endif
GN_SOUND_API void GNa::SetSystemVolume(float vol){
  #ifdef _INC_MMSYSTEM
   #ifndef _WIN32_WCE
	MMRESULT result;
	MIXERCONTROLDETAILS mcd = {0};
    MIXERCONTROLDETAILS_UNSIGNED mcdu = {0};
	MIXERCONTROL mc = {0};

	mcdu.dwValue    = (unsigned int)(vol*65535.0f);
	mcd.cbStruct    = sizeof(MIXERCONTROLDETAILS);
	mcd.hwndOwner   = 0;
	mcd.dwControlID = 0; //g_mcMixer.dwControlID;
	mcd.paDetails   = &mcdu;
	mcd.cbDetails   = sizeof(MIXERCONTROLDETAILS_UNSIGNED);
	mcd.cChannels   = 1;
	result = mixerSetControlDetails(0/*(HMIXEROBJ)g_hVolumeMixer*/, &mcd, MIXER_SETCONTROLDETAILSF_VALUE);
   #else
    if (vol > 1.0f)vol = 1.0f;
	if (vol < 0.0f)vol = 0.0f;
	WORD wNewVolume = (WORD)(vol * 65535.0f);
	waveOutSetVolume(0,(wNewVolume<<16)|wNewVolume );
   #endif
  #elif defined _MAC_ && defined __MAC_EXT__
  	 glut_mac_setSystemVol(vol);
  #elif defined GT_USING_IOS && defined _GNIOSFRAMEWORK_H_
	 GT_IOS_SetSystemVol(vol);
  #else
   #ifdef GT_USING_ANDROID_JNI
     #ifdef _GN_MAIN_H_
      if(!GT_JEnv)GT_Jvm->AttachCurrentThread(&GT_JEnv,NULL);
      GT_JEnv->CallStaticVoidMethod(GT_Japp, GT_J_SetSystemVol , (jfloat)vol);
     #endif
   #else // LINUX / other Unix
    #ifdef GN_HAS_ALSA_VOLUME
    {
        snd_mixer_t *mixer;
        if (snd_mixer_open(&mixer, 0) >= 0) {
            snd_mixer_attach(mixer, "default");
            snd_mixer_selem_register(mixer, NULL, NULL);
            snd_mixer_load(mixer);
            snd_mixer_elem_t *elem = NULL;
            for (const char *name : {"Master", "PCM", "Headphone"}) {
                snd_mixer_selem_id_t *sid;
                snd_mixer_selem_id_alloca(&sid);
                snd_mixer_selem_id_set_index(sid, 0);
                snd_mixer_selem_id_set_name(sid, name);
                elem = snd_mixer_find_selem(mixer, sid);
                if (elem && snd_mixer_selem_has_playback_volume(elem)) break;
                elem = NULL;
            }
            if (!elem) {
                for (elem = snd_mixer_first_elem(mixer); elem;
                     elem = snd_mixer_elem_next(elem)) {
                    if (snd_mixer_selem_has_playback_volume(elem)) break;
                }
            }
            if (elem) {
                long minv, maxv;
                snd_mixer_selem_get_playback_volume_range(elem, &minv, &maxv);
                if (vol > 1.0f) vol = 1.0f;
                if (vol < 0.0f) vol = 0.0f;
                long target = minv + (long)((maxv - minv) * (double)vol);
                snd_mixer_selem_set_playback_volume_all(elem, target);
            }
            snd_mixer_close(mixer);
        }
    }
    #elif defined(_LINUX_)
        int volume = (int)(vol*100.0f);
        volume = (volume | (volume<<8));
        ioctl(0, MIXER_WRITE(SOUND_MIXER_VOLUME), &volume);
    #endif
   #endif
  #endif
}
GN_SOUND_API float GNa::GetSystemVolume(){
  #ifdef GN_HAS_ALSA_VOLUME
    snd_mixer_t *mixer;
    if (snd_mixer_open(&mixer, 0) >= 0) {
        snd_mixer_attach(mixer, "default");
        snd_mixer_selem_register(mixer, NULL, NULL);
        snd_mixer_load(mixer);
        snd_mixer_elem_t *elem = NULL;
        for (const char *name : {"Master", "PCM", "Headphone"}) {
            snd_mixer_selem_id_t *sid;
            snd_mixer_selem_id_alloca(&sid);
            snd_mixer_selem_id_set_index(sid, 0);
            snd_mixer_selem_id_set_name(sid, name);
            elem = snd_mixer_find_selem(mixer, sid);
            if (elem && snd_mixer_selem_has_playback_volume(elem)) break;
            elem = NULL;
        }
        if (!elem) {
            for (elem = snd_mixer_first_elem(mixer); elem;
                 elem = snd_mixer_elem_next(elem)) {
                if (snd_mixer_selem_has_playback_volume(elem)) break;
            }
        }
        float result = 0.5f;
        if (elem) {
            long minv, maxv, cur;
            snd_mixer_selem_get_playback_volume_range(elem, &minv, &maxv);
            snd_mixer_selem_get_playback_volume(elem, SND_MIXER_SCHN_FRONT_LEFT, &cur);
            if (maxv > minv) result = (float)(cur - minv) / (float)(maxv - minv);
        }
        snd_mixer_close(mixer);
        return result;
    }
  #endif
    return 0.5f;
}
//------------------------------------------------------ 其它全局函数 ----------------------------------------------------------------
#ifndef _WIN32_WCE
GN_SOUND_API void GT_ReSetSoundMode2D(){
	//OpenAL像是右手坐标系 把Z方向翻转 即可像左手坐标系那样使用了
    ALfloat listen_ori[] = { 0.0f, 0.0f, -1.0f,  0.0f, 1.0f, 0.0f }; // 听者方向 at 和 up 方向
    ALfloat listen_pos[] = { 0.0f, 0.0f, 0.0f }; // 听者位置
    ALfloat listen_vel[] = { 0.0f, 0.0f, 0.0f }; // 听者速度
	alListenerfv(AL_POSITION,    listen_pos);
    alListenerfv(AL_VELOCITY,    listen_vel);
    alListenerfv(AL_ORIENTATION, listen_ori);
}
GN_SOUND_API void GT_ReSetSoundMode3D(){ }

ALCdevice   *GT_AlDevice = 0;
ALCcontext  *GT_Context  = 0;
GN_SOUND_API bool GT_InitSound(){
    #ifdef AL_ALUT_H
     char c = alutInit(0, 0);
    #else
     GT_AlDevice = alcOpenDevice(0);
     if (!GT_AlDevice)return false;
     GT_Context = alcCreateContext(GT_AlDevice, 0);
     if(!GT_Context){
         alcCloseDevice(GT_AlDevice);
         return false;
     }
     if(!alcMakeContextCurrent(GT_Context)){
         alcDestroyContext(GT_Context);
         alcCloseDevice(GT_AlDevice);
         return false;
     }
    #endif
	 return true;
}
GN_SOUND_API bool GT_UnInitSound(){
    #ifdef AL_ALUT_H
     alutExit();
    #else
     if(GT_Context){
         alcMakeContextCurrent(0);
         alcDestroyContext(GT_Context);
         if(alcGetError(GT_AlDevice) != ALC_NO_ERROR)return false;
         GT_Context = 0;
     }
     if(GT_AlDevice){
         if(!alcCloseDevice(GT_AlDevice))return false;
         GT_AlDevice = 0;
     }
    #endif
	 return true;
}
GN_SOUND_API bool GN_HasSoundAccelerate(){ return true; }

GN_SOUND_API char *const GN_GetInterfaceInfoSound(){
    static const char soundInfo[] = "OpenAL 1";
	return (char *const)soundInfo;
}
#else
GN_SOUND_API void GT_ReSetSoundMode2D(){

}
GN_SOUND_API void GT_ReSetSoundMode3D(){

}
GN_SOUND_API bool GT_InitSound(){
	if(waveOutGetNumDevs()){
	    GT_SoundInitError = false;
		return true;
	}else{ //没有能使用的设备
	    GT_SoundInitError = true;
		return false;
	}
}
GN_SOUND_API bool GT_UnInitSound(){

	return true;
}
GN_SOUND_API bool GN_HasSoundAccelerate(){ return false; }

GN_SOUND_API char *const GN_GetInterfaceInfoSound(){
   #ifdef _WIN32_WCE
    static const char soundInfo[] = "MMSystem";
   #else
	static const char soundInfo[] = "OpenAL 1";
   #endif
	return (char *const)soundInfo;
}
#endif // _WIN32_WCE
#endif // _GN_SOUND_H_
