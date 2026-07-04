#include<GN.h>
#ifndef _GN_MEDIA_H_
#define _GN_MEDIA_H_

#define GNm_VERSION "1.0"  //★★★ 版本

/*请在编译环境里的预处理定义添加以下其中一个宏来确定代码的应用模式*/
//#define GNm_USING_SDK                       //SDK方式使用
//#define GNm_USING_LIB_EXPORTS               //动态库导出
//#define GNm_USING_SOURCE_CODE               //源代码编辑

#ifdef GNm_USING_SOURCE_CODE			//源代码编辑
#define GNm_API   
#define GNm_CALL   
#elif defined GNm_USING_LIB_EXPORTS		//动态库导出
 #if defined _WIN32_ || defined _WIN32_WCE
  #define GNm_API      __declspec(dllexport)
  #define GNm_CALL     __declspec(dllimport)
 #else
  #define GNm_API      extern
  #define GNm_CALL     extern
 #endif
#else //------------------------------	//SDK 方式使用
 #if defined _WIN32_ || defined _WIN32_WCE
  #define GNm_API      __declspec(dllimport)
  #define GNm_CALL     __declspec(dllexport)
 #else
  #define GNm_API   
  #define GNm_CALL   
 #endif
#endif
/*-----------------------------------------------------------------------------------------------------------------------------------*/
#if !defined(GNm_USING_SOURCE_CODE) && !defined(GN_USING_SDK_INTEGRATION) && !defined(GNm_USING_LIB_EXPORTS)
#pragma comment(lib, "GN_Media_win32.LIB")
#endif
GNm_API char *const GNm_GetVersion();     //获得引擎多媒体版本号

/*****************************************************************************************************************************************/

namespace GNm{
 
class GNm_API cVIDEO    // ★★★视频流播放
{   
	void				 *z__attrb;		    //内部私有属性数据
   public:
    cVIDEO();
   ~cVIDEO();
    void Release();                         //释放对象资源

	//初始化(路径文件名, 需播放多少次 负数为循环播放, 是否尽量使视频能同步上经历的时间 这会增加额外开销, 是否处理视频里的声音)返回是否成功
	bool InitToData(const char * filename, int loop=0, bool sync_time=true, bool audio=true);

	//更新文件流影像到图像里(帧间隔时间) 返回现在播放到的时间:0表示已经播放结束 这需挂在循环里面不断调用 否侧会中断没有显示
	unsigned int UpdataToImage(unsigned int updatatime);

	//更新文件流播放(帧间隔时间,  屏幕坐标X, 坐标Y,  宽度 默认为填满整个屏幕,  高度 默认为填满整个屏幕,  颜色,  混合模式) 返回现在播放到的时间:0表示已经播放结束 这需挂在循环里面不断调用 否侧会中断没有显示
	unsigned int UpdataPlay(unsigned int updatatime, int x=0, int y=0, int w=0, int h=0, unsigned int  color=0xffffffff, unsigned short blendmode=GNg::eBLEND_NORMAL);

	void SetStage(float  state);            //设置播放进度(进度 0.0f至1.0f)
	void SetSpeed(float speed);             //设置播放速率(速率) !如果有指定播放速率 那就不要再用以经乘了陪数的时间传给 updatatime 参数了
	
	void SetVolume(float vol=1.0f);			//设置音量大小(音量值0.0f至1.0f） !!没有硬件加速下设置了其他音量值会消费一些CPU来处理减声

	void Stop();                            //停止播放
	void Play();                            //播放
	void Pause();                           //暂停或继续播放
	void AddPlayCount(int loop = 1);        //增加播放次数(次数)
	bool IsPlaying();                       //判断当前是否正在播放
	bool IsPause();                         //判断当前是否暂停播放
	bool IsBusy();                          //判断当前是否正在忙碌读取数据中
	
	P<GNg::sIMAGE> GetImage();              //获得视频解出来的内部影像图()返回图像
	unsigned int   GetEndTime();            //获得文件总播放时间()返回时间 单位:毫秒
	unsigned int   GetNowTime();            //获得文件现在播放到的时间()返回时间 单位:毫秒
	
	int			   GetLoopNum();		    //获得还需要播放循环的次数 <0:表示无限循环
	
	float		   GetVolume();				//获得当前音量大小
	unsigned int   GetFreq();				//获得音频采样率
	unsigned short GetBit();				//获得音频位数
	unsigned short GetChannel();			//获得音频声道数
	char*		   GetFileName();			//获得播放视频的路径文件名 当视频流还没初始化 返回""
};
};//end namespace GNm
#endif //_GN_MEDIA_H_