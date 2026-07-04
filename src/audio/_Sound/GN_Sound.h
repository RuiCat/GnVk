#pragma once
#ifndef _GN_SOUND_H_
#define _GN_SOUND_H_

/*请在编译环境里的预处理定义添加以下其中一个宏来确定代码的应用模式*/
//#define GN_SOUND_USING_SDK                       //SDK方式使用
//#define GN_SOUND_USING_LIB_EXPORTS               //动态库导出
//#define GN_SOUND_USING_SOURCE_CODE               //源代码编辑

#ifdef GN_SOUND_USING_SOURCE_CODE          //源代码编辑
#define GN_SOUND_API
#define GN_SOUND_CALL
#elif defined GN_SOUND_USING_LIB_EXPORTS   //动态库导出
 #if defined _WIN32_ || defined _WIN32_WCE
  #define GN_SOUND_API      __declspec(dllexport)
  #define GN_SOUND_CALL     __declspec(dllimport)
 #else
  #define GN_SOUND_API      extern
  #define GN_SOUND_CALL     extern
 #endif
#else //------------------------------//SDK 方式使用
 #if defined _WIN32_ || defined _WIN32_WCE
  #define GN_SOUND_API      __declspec(dllimport)
  #define GN_SOUND_CALL     __declspec(dllexport)
 #else
  #define GN_SOUND_API
  #define GN_SOUND_CALL
 #endif
#endif
/*-----------------------------------------------------------------------------------------------------------------------------------*/
#if !defined(GN_SOUND_USING_LIB_EXPORTS) && !defined(GN_USING_SDK_INTEGRATION) && !defined(GN_SOUND_USING_SOURCE_CODE)
#pragma comment(lib,"GN_Sound_win32.lib")
#endif
#ifndef  WAVE_FORMAT_PCM
#define  WAVE_FORMAT_UNKNOWN  0x0000  //未知音频格式
#define  WAVE_FORMAT_PCM      0x0001  //无压缩PCM音频格式
#endif
/****************************************************** Sound 模块必需实现的功能 **************************************************************/
struct GN_SOUND_API GNa_sSOUND	  //声音基本对象
{
    void				 *z__attrb;		//私有附加属性 主要用来记录音频实际相关数据

    GNa_sSOUND();
   ~GNa_sSOUND();

    //释放清理内部数据
    void Release();

    //初始化声音基本对象(托管音频数据, 音频格式, 声道数, 声音位数, 频率, 数据字节大小, 是否定位点声音)返回是否成功 !!参数data传进去后会自动被托管起来 调用后可不必delete [] data !!该函是能够重入而不会造成内存涉漏的
    bool InitToData(unsigned char *data, unsigned short format, unsigned short channel, unsigned short bit, unsigned int freq, unsigned int size, bool point_sound);
};
namespace GNa {

class GN_SOUND_API cSOUND_SRC     //★★★发声源点类
{
	void				 *z__attrb;		//私有附加属性

   public: /*★用户调用接口------------------------------------------------------------------------------------------------------------------*/
    cSOUND_SRC();
   ~cSOUND_SRC();

   //释放清理内部数据
   void Release();

   //初始化(声音数据结构, 是否循环播放, 发声会重叠的个数 一般1个就够了, 默认音量, 默认音调) !!该函是能够重入而不会造成内存涉漏的 !!没使用硬件加速不支持设置默认音量
   void InitToData(const GNa_sSOUND *sound, bool playloop=false, unsigned short overlap_num=1, float volume=1.0f, float pitch=1.0f);

   //以下这些函数 如果ID访问出轨 将会自动用 % 取模到重叠个数以内

   //播放(编号)
   void Play  (unsigned short ID=0);
   //停止播放(编号)
   void Stop  (unsigned short ID=0);
   //暂停或播放(编号)
   void Pause (unsigned short ID=0);
   //锁定不能进行暂停或停止(编号)
   void Rewind(unsigned short ID=0);

   //设置音量大小(编号, 音量值0.0f至1.0f)  !没使用硬件加速声音设备都是整体调节音量而不能独立调节每一段
   void SetVolume(unsigned short ID=0, float volume=1.0f);
   //设置音调高低(编号, 音调值0.003f至4.35f 默认为正常效果)  !没使用硬件加速下很多声音设备都不支持动态调整这个效果
   void SetPitch (unsigned short ID=0, float  pitch=1.0f);
   //设置发生点的位置(编号, 观察坐标系的位置坐标x, 坐标y, 坐标z)
   void SetPosi(unsigned short ID=0, float x=0.0f, float y=0.0f, float z=0.0f);
   //设置发生点的速度(编号, 速度方向x, 方向y, 方向z)
   void SetVelocity(unsigned short ID=0, float vx=0.0f, float vy=0.0f, float vz=-0.01f);

   //获得该对象发声重叠的个数
   unsigned short GetOverlapNum();
};

//设置系统音量( 音量水平 取值0.0f至1.0f)
GN_SOUND_API void  SetSystemVolume(float volume);
GN_SOUND_API float GetSystemVolume();

enum eBGM_PLAY_STATE{		//返回播放状态类型
	 eBGM_NO_DATA,			 //没有指定播放数据
     eBGM_PLAY_BEGIN,		 //数据已加载准备播放
	 eBGM_PLAY_ING,			 //播放中
     eBGM_PAUSE,			 //暂停播放,或播放完了
	 eBGM_STOP,				 //设置了停止更新播放
	 eBGM_UPDATA_ERROR		 //加载更新错误
};
}; //end namespace GNa

class GN_SOUND_API GT_cSOUND_STREAM   //音频流基本类 该类用双缓冲来实现平滑流播放
{public:
    void				 *z__attrb;		//私有附加属性

    GT_cSOUND_STREAM();
   ~GT_cSOUND_STREAM();

    //释放清理内部数据
    void Release();

	//初始化播放流设备( 最大播放缓冲大小, 音频格式, 声道数, 声音位数, 频率,播放音量, 播放音调 ) 返回是否成功
    bool zInitStream(unsigned int buf_max_size, unsigned short format, unsigned short channel, unsigned short bit, unsigned int freq, float volume, float pitch);

	//更新播放流(帧间隔时间, 是否要求继续播放)返回内部播放状态 详情请参看 GNa::eBGM_PLAY_STATE 的定义
	GNa::eBGM_PLAY_STATE zUpdateStream(unsigned int updatetime, bool play_ing);

	//设置音量大小(音量值0.0f至1.0f)
	void zSetVolume(float volume=1.0f);
	//设置音调高低(音调值0.003f至4.35f 默认为正常效果)
    void zSetPitch (float pitch=1.0f);

    //当播放需要装填音频数据时的处理(需要装填音频数据缓冲的指针)要求返回今次装填了多少个字节 如果返回0音频流将不会继续播放 !!请不要装填比方法 zInitStream 参数:buf_max_size大的数据
	virtual unsigned int zOnLoad(char *data);
};

//重设置2D声音播放模式() !该函会在帧循环调用渲染2D代码前被调用来设置声音播放为2D模式
GN_SOUND_API void GT_ReSetSoundMode2D();
//重设置3D声音播放模式() !该函会在帧循环调用渲染3D代码前被调用来设置声音播放为3D模式
GN_SOUND_API void GT_ReSetSoundMode3D();

GN_SOUND_API bool GT_InitSound();                    //初始化声音模块相关的东西() 返回是否成功
GN_SOUND_API bool GT_UnInitSound();                  //释放声音模块相关的东西()   返回是否成功

GN_SOUND_API bool        GN_HasSoundAccelerate();    //获得声音播放是否有支持硬件加速()
GN_SOUND_API char *const GN_GetInterfaceInfoSound(); //获得声音播放模块信息 如: "OpenAL 1" "DirectSound" "MMSystem"

#endif  //endif _GN_SOUND_H_
