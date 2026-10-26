#ifndef _GN_AUDIO_H_
#define _GN_AUDIO_H_

namespace GNa {
struct sSOUND;

//读取声音文件(路径文件名, 装载声音的对象, 是否定位点声音)返回读取是否成功
GN_API bool SoundLoad(char *filename, sSOUND *sound, bool point_sound = true);

struct GN_API sSOUND : public GNa_sSOUND //声音结构
{
    unsigned int time; //音频的播放时间
    char *fileName; //保存音频读取时使用的路径文件名

    sSOUND();
    ~sSOUND();

    //初始化声音对象(路径文件名,  是否定位点声音)!!这个其实里面是调用了 GNa::SoundLoad 该函是能够重入而不会造成内存涉漏的
    void InitToData(char *filename, bool point_sound = true);
};
extern GN_API sSOUND Sound;

class GN_API cBGM_MUSIC : protected GT_cSOUND_STREAM //★★★背景BGM音乐播放器 !!注意该类只播放大于频率22050的音频流
{
    iFILE_AUDIO::cSTREAM *zLoad; //文件流读取器
    void *zSpec; //音色分频器

    unsigned int zSize; //音频总数据长度
    unsigned int ztSize; //当前播放到的数据长度
    unsigned int zBufferSize; //缓冲大小字节
    unsigned int zUnitCount; //音频总样本数
    unsigned int zTime; //音频总播放时间
    unsigned int ztTime; //当前播放的时间点

    float zProgtg; //目标播放进度比率
    float zVolume; //播放音量
    float zPitch; //播放音调
    float zVoltg; //变化目标音量
    float zPittg; //变化目标音调
    float zVolst; //上次的开始音量
    float zPitst; //上次的开始音调
    int ztVol; //音量延迟临时变化时间
    int ztPit; //音调延迟临时变化时间
    int ztVoltg; //音量延迟目标时间
    int ztPittg; //音调延迟目标时间

    unsigned int zFreq; //音频采样率
    unsigned short zBit; //音频位数
    unsigned short zChannel; //音频声道数
    unsigned short zFormat; //音频格式

    int zLoop; //播放多少次 负数为循环播放
    int zLoop_n; //临时记下剩下要播放的次数
    eBGM_PLAY_STATE zState; //播放状态

    bool zInit(); //内部的加载方法

    //当播放需要装填音频数据时的处理(需要装填音频数据缓冲的指针)要求返回今次装填了多少个字节 如果返回0音频流将不会继续播放 !!请不要装填比方法 zInitStream 参数:buf_max_size大的数据
    virtual unsigned int zOnLoad(char *data);

  public: /*★用户调用接口------------------------------------------------------------------------------------------------------------------*/
    cBGM_MUSIC(void);
    ~cBGM_MUSIC(void);

    //释放资源
    void Release();

    //初始化BGM对象 可以实现中途动态换歌(路径文件名, 要循环播放的次数:-1为不断循环)返回是否成功 该函是能够重入而不会造成内存涉漏的
    bool InitToData(char *_filename, int _loop = -1);

    //更新文件流播放(帧间隔时间)返回:播放状态 详情请参看GNa::eBGM_PLAY_STATE  !它已经包含调用Play的作用 执行以后会自动播放了 这要在循环里面不断调用 否侧歌曲会中断
    eBGM_PLAY_STATE UpdataPlay(unsigned int updatetime = 15);

    void AddPlayCount(int _loop = 1); //增加播放次数(次数)
    void Play(); //播放
    void Stop(); //停止播放
    void Pause(); //用来暂停或播放

    //设置音量大小(音量值0.0f至1.0f, 延迟过度时间） !!没有硬件加速下设置了其他音量值会消费一些CPU速度来处理减声
    void SetVolume(float volume = 1.0f, int s_time = 0);
    //设置音调高低(音调值0.003f至4.35f 默认为正常效果, 延迟过度时间） !!没有硬件加速下设置延迟过度时间不产生作用 只能即时作出响应 !注意如果有指定播放音调 那就不要再用以经乘了陪值的时间传给updatatime参数了
    void SetPitch(float pitch = 1.0f, int s_time = 0);
    //设置播放进度(进度比率0.0f - 1.0f)
    void SetStage(float _state);

    inline float GetVolume() { return zVolume; } //获得当前音量大小
    inline float GetPitch() { return zPitch; } //获得当前音调高低
    inline eBGM_PLAY_STATE GetState() { return zState; } //获得最近播放状态

    inline unsigned int GetSize() { return zSize; } //获得音频总数据大小
    inline unsigned int GetFreq() { return zFreq; } //获得音频采样率
    inline unsigned short GetBit() { return zBit; } //获得音频位数
    inline unsigned short GetChannel() { return zChannel; } //获得音频声道数
    inline unsigned short GetFormat() { return zFormat; } //获得音频格式

    inline int GetLoopNum() { return zLoop_n; } //获得还需要播放循环的次数 -1:表示无限循环
    inline unsigned int GetTime() { return zTime; } //获得文件总播放时间
    inline unsigned int GetNowTime() { return ztTime; } //获得现在播放到的时间

    //同步播放时间(希望同步到的播放时间) 参数时间值要在歌曲的总播放时间内 超过则不产生作用 这最好能在循环里面不断调用 一般跟UpdataPlay()之后
    void SyncTime(unsigned int tg_time);

#ifdef GN_USING_ADDITIONAL_LIB
    //设置音色分频格式(要分频的通道数, 处理声道, 音频数据的处理间格1-num, 低频到高频的采样范围比重一般0.5f-1.5f, 频率通道最低频采样范围, 频率通道最高频采样范围)返回设置是否成功
    bool SetSpecFormat(int spec_num, int proc_ch = 0, int proc_compartment = 1, float LtoH_ratio = 0.8f, int max_Lhz = 0, int max_Hhz = 0);
    //获得音色分频数据(数据输出)返回分频的通道数  这最好能在循环里面不断调用 一般跟UpdataPlay()之后
    int GetSpecData(float *spec_data_out);
#endif
};
}; //end namespace GNa

//声音准备为2D播放模式()
GN_API void GN_SoundPlayBegin2D();
//声音准备为3D播放模式()
GN_API void GN_SoundPlayBegin3D();

//初始化引擎声音设备()
GN_API void GN_InitSound();
//释放引擎声音设备()
GN_API void GN_UnInitSound();

#endif //_GN_AUDIO_H_