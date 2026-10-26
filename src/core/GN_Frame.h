#pragma once
#ifndef _GN_FRAME_H_
#define _GN_FRAME_H_
namespace GNg {

enum eEVENT_TYPE { //动画事件常量
    eEVENT_MOVE_X = 16, //X移动
    eEVENT_MOVE_Y = 32, //Y移动
    eEVENT_SIZE_X = 64, //X缩放             该值为负的话可以得到镜像
    eEVENT_SIZE_Y = 128, //Y缩放             该值为负的话可以得到镜像
    eEVENT_ROTATION = 256, //旋转
    eEVENT_COLOR = 512, //颜色变化
    eEVENT_TEXID = 1024, //图序变化          该值由0开始

    eEVENT_TEXMOV_X = 4096, //纹理水平移动变化  该值由0开始
    eEVENT_TEXMOV_Y = 8192, //纹理垂直移动变化  该值由0开始
    eEVENT_SOUND = 16384, //声音              该事件可以不设结束时间和开始结束值
    eEVENT_OTHER = 32768 //其它事件
};
struct GN_API sAM_EVENT { //动画事件  要控制动画筐运动必需填写事件信息
    unsigned int start_time; //事件触发时间
    unsigned int segment_time; //事件经历时间
    float start_value; //开始值
    float end_value; //结束值
    float slinear_value; //缓动值
    unsigned short slinear_mode; //缓动方式 取值参考 LINEAR_TYPE
    unsigned short event_mode; //事件类型 值为 eEVENT_TYPE 里的组合 可用 | 组合多个

    uint8 *zDataSave(uint8 *wpos); //保存该对象的2进制数据(当前写入位置)返回最后的写完位置
    uint8 *zDataLoad(uint8 *rpow); //读取该对象的2进制数据(当前读取位置)返回最后的读完位置

    /*---------------------------------- !! 动画事件设置注意事项 !! ------------------------------------\
	|                                                                                                   |
	|    稳定型: 设置每一种事件的时间应该要相接的 举例(由开始0--100,100--500,500--1000结束)             |
	|            该类型稳定不怕受窗口拉伸暂停卡机和其他因素影响系统运行带来影响                         |
	|                                                                                                   |
	|	 随便型: 设置每一种事件时间随便即可,      举例(任意开始100--200,360--520,450--1500结束)         |
	|	         该类型使用方便而且事件少对运行速度有利 另外灵活地使用它可以产生其它的变化如闪动跳帧    |
	|			 这样比较适合粒子动画                                                                   |
	|                                                                                                   |
	|        如果纹理是有分格的 (eEVENT_TEXMOV_X) (eEVENT_TEXMOV_Y) 这两个事件最好就只选其中之一        |
	|								    !!否则容易有意想不到的效果!!                                    |
	|                                                                                                   |
	\--------------------------------------------------------------------------------------------------*/

    inline sAM_EVENT() {}

    //构造(   事件类型,							事件开始时间,          事件结束时间              开始值,                  结束值,                  缓动值0.0f-1.0f,            缓动方式)
    sAM_EVENT(unsigned short _event_mode, unsigned int _start_time = 1, unsigned int _end_time = 1000, float _start_value = 0.0f, float _end_value = 128.0f, float _slinear_value = 0.0f, unsigned short _slinear_mode = 0);

    //初始化( 事件类型,							事件开始时间,          事件结束时间              开始值,                  结束值,                  缓动值0.0f-1.0f,            缓动方式)
    void Init(unsigned short _event_mode, unsigned int _start_time = 1, unsigned int _end_time = 1000, float _start_value = 0.0f, float _end_value = 128.0f, float _slinear_value = 0.0f, unsigned short _slinear_mode = 0);

    //初始化颜色(        事件开始时间,          事件结束时间                            开始颜色值,                    结束颜色值,				   缓动值0.0f-1.0f,            缓动方式)
    void InitColor(unsigned int _start_time = 1, unsigned int _end_time = 1000, unsigned int _start_value = 0xffffffff, unsigned int _end_value = 0xffffffff, float _slinear_value = 0.0f, unsigned short _slinear_mode = 0);
};
/**********************************************************************************************************************************/

/*- !!动画图筐注意事项:
	(1)在没有3D加速技术的纯2D底层图形接口下将会失去 镜像反转图像 透明混合等效果!
	(2)使用纹理移动和重复平铺纹理功能时 想要兼容低端老设备 纹理尺寸一定要是2n次方 否侧会显示不正确!
    (3)对于旋转中心在中间或其他地方的 图片尺寸最好是偶数 否则可能会产生小许模糊!
*/
class GN_API cFRAME_RC : public sAM_CTRL //★★★ 正规动画图筐 这个不能旋转 处理速度快
{
  public:
    sTRANSF2D zFrame_ts; //内部动画变换结构
    sRECT2 zRcLoc; //筐自身局部范围 此参数可以控制旋转或放大以那里为的中心 如果在cFRAME里right和bottom表示为宽与高
    sRECT2 zRcS0; //经过程序控制纹理移动的数据
    sRECT2 zRcS; //动画事件控制的纹理位置和截取的宽高度
    unsigned int zColor; //内部色调和透明度	 当透明度为0时不显示
    unsigned short zTexID; //当前要显示图象索引ID
    int8 zRcS_w_num; //纹理带有规格的水平格数
    int8 zRcS_h_num; //纹理带有规格的垂直格数

    sAM_EVENT *zFEvent; //事件数组
    unsigned int zEventcount; //事件数

    cFRAME_RC *zSubFrame; //子筐数组
    unsigned short zSubCount; //子筐数 如果没有子筐的一定要设为0 否侧容易出轨

    unsigned char *zTexIndex; //纹理索引数据
    unsigned short zTexIndexCount; //纹理索引数
    unsigned int zTexOnceTime; //每个纹理索引停留时间

    sIMAGE *zPlay_obj; //播放的图序
#ifdef _GN_SOUND_H_
    GNa::sSOUND *zSouObj; //声音对象
    GNa::cSOUND_SRC zSound; //发声点
    unsigned int zSouStartTime; //发声开始的时间
    unsigned int zSouEndTime; //声音播完的时间
#endif
    unsigned short zTargetID; //重叠应用对象的目标ID
    uint8 zFrameTsReItem; //内部动画变换结构需重置的项 可由多个值|组合 值意义 1:x 2:y 4:sx 8:sy 16:r
    P<char> zObjsKeep; //保留来鉴别有多少个复制副本

#define GT_TMP_FRAME_REF_OBJS_INFO_TYPE(TYPE, T) \
    struct TYPE {                                \
        T<P<char>> pkeeps;                       \
        T<sIMAGE *> pimgs;                       \
        T<sAM_EVENT *> pevs;                     \
        T<unsigned int> evns;                    \
        T<cFRAME_RC *> pframes;                  \
        T<unsigned short> fmns;                  \
        T<unsigned char *> ptidxs;               \
        T<unsigned short> tidxns;                \
        T<void *> psounds;                       \
    };
    GT_TMP_FRAME_REF_OBJS_INFO_TYPE(sREF_OBJSW, GN::sLIST)
    GT_TMP_FRAME_REF_OBJS_INFO_TYPE(sREF_OBJSR, P)

    uint8 *zDataSave(uint8 *wpos, sREF_OBJSW *objs_collect, uint8 in_launc); //保存该对象的2进制数据(当前写入位置, 收集到的对象列表, 该对象是否嵌入到图框发射里的 2为3D图框)返回最后的写完位置
    uint8 *zDataLoad(uint8 *rpos, sREF_OBJSR *objs_ref); //读取该对象的2进制数据(当前读取位置, 相关的对象引用数组信息)返回最后的读完位置

    void zSelectTarget(unsigned short id); //设置内部重叠的对象ID(ID)
    void zEventProce(unsigned int updatetime); //内部处理事件动画(帧间隔时间)
    void zTexIndexProce(); //内部处理图片索引动画()

  public: //★用户接口 ---------------------------------------------------------------------------------------------------------------------------------------------------
    sRECT2 aLevel_range; //#筐实时的判定范围 这可能会包含前几层父筐的影响集合 要获取准确值 最好先播放一次动画再获取
    sTRANSF2D aLevel_ts; //#深入到该动画筐层的变换信息 这可能会包含前几层父筐的变换集合 要获取准确值 最好先播放一次动画再获取
    unsigned int aLevel_color; //深入到该动画筐层的颜色 这可能会包含前几层父筐的颜色调整 要获取准确值 最好先播放一次动画再获取

    sTRANSF2D aFrame_ts; //给程序控制的位置变换综合描述
    unsigned int aColor; //给程序调节的色调和透明度  当透明度为0时不显示
    unsigned short aBlendmode; //混合模式 当为0时就不显示 相对于Flash里的空白帧 当筐仅作为组父对象且不作显示指定0可以不计算组碰撞范围以节省更多性能  其它值参考 eBLEND_MODE

    cFRAME_RC();
    cFRAME_RC &operator=(const cFRAME_RC &src);
    ~cFRAME_RC() { this->Release(); }
    virtual int GetClassSize(); //获得该类对象所占用的内存大小

    //释放内部资源
    void Release();

    //初始化(用于播放的图序 值1则忽略设置它,  事件数,          事件数组,          筐开始默认的摆放位置, 旋转中心原点落在相对图的横向比率, 旋转中心原点落在相对图的竖向比率, 混合模式,         默认的颜色,                     播放模式                  纹理的水平格数,    纹理的垂直格数) !后两参数 正数代表一张纹理有几多格数 0代表不变,负值代表重复平铺纹理格数
    void Init(sIMAGE *_play_obj, unsigned int _eventcount = 0, sAM_EVENT *_fevent = 0, sTRANSF2D _ts = Ts2D, float ori_pointU = 0.0f, float ori_pointV = 0.0f, unsigned short _blendmode = 1, unsigned int _color = 0xffffffff, unsigned short _am_mode = eAM_REPEAT, int8 _rcS_w_num = 0, int8 _rcS_h_num = 0);

    //初始化加入索引动画（索引数据个数,                  索引数组,                单格的停留时间,            播放模式)
    void InitTID(unsigned short _tex_index_count, unsigned char *_tex_index, unsigned int _tex_once_time, unsigned short _am_mode = 100);

    //初始化加入声音（  声音对象,   发声可重叠的个数:一般1个就够了 在它被多个对象所调用为子筐或粒子系统时才用多个重叠数)
#ifdef _GN_SOUND_H_
    void InitSoundToData(GNa::sSOUND *sound_obj, unsigned short overlap_num = 1);
#else
    void InitSoundToData(void *sound_obj, unsigned short overlap_num = 1);
#endif
    //插入子筐对象(  对象数组,    子筐数) !!子父对象类型必需一至 否则子父框架容易混乱 使结果难以预料
    void InitSubFrame(cFRAME_RC *_sub_frame, unsigned short _sub_count);

    //从另一个筐里复制数据而不共享指针(复制源)
    void CopyData(const cFRAME_RC &src);

    //保管外部连接对象 调用后 zPlay_obj zFEvent zSubFrame zTexIndex zSouObj 等外部对象数据不必在外释放了
    void KeepExternObjs() { zObjsKeep.New(1); }

    //播放动画(帧间隔时间, 父筐 0表示它是最顶层筐了)返回动画存活运行时间 返回0表示播放已经结束
    unsigned int PlayFrame(unsigned int updatetime, cFRAME_RC *father = 0);

    //重新设置该筐的动画时间 使到它恢复到初始状态(重设选项)
    void ReSetPlaytime(eAM_TIME_RESET_OP re_op = eAM_TIME_RESET_ALL);

    //交换动画事件起始值()
    void SwapAmStartEnd();

    //设置动画播放模式(播放模式)
    void SetAmMode(unsigned short am_mode);

    //引用子筐(子筐ID)
    inline cFRAME_RC &SubFrame(unsigned short id) { return zSubFrame[id]; }
    //获得子筐数
    inline unsigned short GetSubFrameNum() { return zSubCount; }

    //获得播放图序
    inline sIMAGE *GetImage() { return zPlay_obj; }
    //获得动画事件数组 !您应尽量不要修改里面的经历时间 这容易导致动画混乱 要修改请调用 Init 再初始化传入
    inline sAM_EVENT *GetFEvent() { return zFEvent; }
    //获得动画事件数 您可以再设置一些比原数小的值 以临时忽略排后面的动画事件播放
    unsigned int &GetEventcount() { return zEventcount; }

    //移动纹理(每秒向X方向移动的速度, 每秒向Y方向移动的速度)
    void TextureMove(float vx, float vy);
    //停止纹理移动
    void TextureStopMove();

    //设置纹理绝对位置偏移量(水平偏移量, 垂直偏移量)
    void SetTexturePosi(float px, float py);
    //获得纹理绝对位置偏移量(返回水平偏移量, 返回垂直偏移量)
    void GetTexturePosi(float *px, float *py);

    //设置分了格的纹理去到第j行第i列的区域(列ID, 行iD) 行列值由0开始
    void SetTextureSquare(short i_num, short j_num);
    //设置显示第几张纹理（纹理ID）如果同时在动画里设置了纹理ID变化 那么动画会代替这里的设定
    void SetTextureID(unsigned short tex_id);

    int8 GetTextureSquareWN(); //获得纹理水平分格数
    int8 GetTextureSquareHN(); //获得纹理垂直分格数
    float GetOriPointU(); //获得旋转中心原点落在相对图的横向比率
    float GetOriPointV(); //获得旋转中心原点落在相对图的竖向比率
    void GetOriLocalSize(float *wout, float *hout); //获得筐自身局部范围宽度和高度(返回宽度,返回高度)

    void GetFrameSize(int *w_out, int *h_out); //获得筐的实际宽度和高度(返回宽度, 返回高度) !这排除父筐影响 但包含自身变换影响

    bool HitFrameTest(cFRAME_RC *frame_obj); //检测该筐和指定筐有没碰撞(指定筐)
    bool HitPointTest(int x, int y); //检测指定点是否在该筐内(点坐标X, 坐标Y)
};
/***************************************************************************************************************************************/

class GN_API cFRAME : public cFRAME_RC //★★★ 动画图筐类 支持旋转等效果
{
    static cFRAME *zScreenFrame; //全局屏幕筐 能影响全部筐对象
  public:
    //★用户接口 ---------------------------------------------------------------------------------------------------------------------------------------------------
    sMAT33 aLevel_world; //#深入到该动画筐层的变换矩阵 这可能会包含前几层父筐的矩阵集合 要获取准确值 最好先播放一次动画再获取
    //	sTRANSF2D           aLevel_ts;		//#在该类里它用作是用 aLevel_ts.x aLevel_ts.y 来保存该筐实时跟踪的判定范围平行四边形的点1坐标

    cFRAME() { aLevel_world.Col3To001(); }
    cFRAME &operator=(const cFRAME &src);
    ~cFRAME() { this->Release(); }
    virtual int GetClassSize(); //获得该类对象所占用的内存大小

    void SetToScreenFRAME(); //把该动画筐设置成全局屏幕筐
    static cFRAME *RemoveScreenFRAME(); //取出全局屏幕筐 并还原全局屏幕筐为0

    //播放动画(帧间隔时间, 父筐 0表示它是最顶层筐了)返回动画存活运行时间 返回0表示播放已经结束
    unsigned int PlayFrame(unsigned int updatetime, cFRAME *father = zScreenFrame);

    //引用子筐(子筐ID)
    cFRAME &SubFrame(unsigned short id) { return ((cFRAME *)zSubFrame)[id]; }

    bool HitFrameTest(cFRAME_RC *frame_obj); //检测该筐和指定筐有没碰撞(指定筐)
    bool HitPointTest(int x, int y); //检测指定点是否在该筐内(点坐标X, 坐标Y)
};
/********************************************************** cFRAME_LAUNCHER ****************************************************************/
template <class T, class T2>
class cLAUNCHER_SEED;

class GN_API cFRAME_LAUNCHER_ //动画图筐发射器基类 用来随机产生很多相同的动画图筐的类 效果类似粒子系统
{
  public:
    cFRAME *zFrame; //发射单元动画筐
    cFRAME *zFrame2; //附加效果动画筐

    void *zTransf_s; //各个筐单元的位置大小综合信息数据
    unsigned int *zNowtime_s; //各个筐单元的现在播放时间数据
    unsigned int *zColor_s; //各个筐单元的颜色数据

    unsigned int zEnd_time; //动画筐的结束时间
    unsigned int zMax_num; //筐单元缓冲个数 也就是在同一时刻存在的最大数
    unsigned int zDisplay_num; //播放时现在正在显示的筐个数

    unsigned int zActivity_time; //筐发射的持续活动时间 如果是0的话 就代表永远不停发射
    unsigned int zObjNowtime; //发射器所经历的时间

    unsigned int zGen_num; //发射器已经生成筐的次数
    unsigned int zGen_count; //发射器已经生成筐的个数
    unsigned int zGen_speed; //筐单元的最快生产时间间隔
    unsigned int zMax_gen_speed; //锁定设置时筐的最快生产时间间隔
    unsigned short zGen_once_num; //一次生产筐单元的最大数目
    unsigned short zMax_once_num; //锁定设置时一次生产筐单元的最大数目

    bool zRunUpdate; //当前正在运行或暂停
    bool ztSn; //生成大小临时处理用的
    uint8 ztSize; //生成大小临时处理用的
    bool ztCn; //生成颜色临时处理用的
    uint8 zTa; //生成颜色临时处理用的
    uint8 zTr; //生成颜色临时处理用的
    uint8 zTg; //生成颜色临时处理用的
    uint8 zTb; //生成颜色临时处理用的

    uint8 ztBn; //附加效果筐的临时亮度来回表示值
    short ztBright; //附加效果筐的临时亮度

    P<char> zObjsKeep; //保留来鉴别有多少个复制副本

    void zRelease(); //仅释放单元数据
    void zResetState(); //重置一些内部状态
    void zReAllocBuffer(bool reset = true); //重分配处理匹配的缓冲(是否重新设置该系统的动画时间)
    virtual unsigned int zTsUnitSize(); //返回对应 zTransf_s 的单个框变换数据大小字节
    void zCopyData(const cFRAME_LAUNCHER_ &src, bool cpy_state); //复制内部数据(复制源, 是否复制已发射的框当前状态 否则重置为初始状态)
    unsigned int zGetColorChange(int speed); //获得色彩变幻中的一个颜色
    float zCreateUnit_RotSizeAndColor(unsigned int id, float &rot_out); //创建筐单位的旋转与大小和颜色(单位ID, 返回旋转角度)返回大小
    virtual void zCreateUnit(unsigned int id) {} //创建筐单元(单元ID)
    virtual void zPlayFrame2(unsigned int updatetime, unsigned int color) {} //播放附加效果的筐动画(更新时间, 色调)
    virtual void zPlayFrames(unsigned int updatetime); //播放发射筐动画(更新时间)
    void zUpdateFrame2(unsigned int updatetime); //更新播放附加效果的筐动画相关(更新时间)

    uint8 *zDataSave(uint8 *wpos, GN::sLIST<cFRAME *> &frames_collect, GN::sLIST<P<char>> &keep_obj_collect); //保存该对象的2进制数据(当前写入位置, 收集到的图框列表, 收集得到的保留鉴别对象)返回最后的写完位置
    uint8 *zDataLoad(uint8 *rpos, cFRAME **frame_refs, P<char> *keep_obj_refs); //读取该对象的2进制数据(当前读取位置, 图框引用组, 保留鉴别对象引用组)返回最后的读完位置

  public: //★用户接口 ---------------------------------------------------------------------------------------------------------------------------------------
    float aGen_angle; //筐的发射转向角度范围
    unsigned int aGenColor; //发射的筐整体色调
    uint8 aGen_ts_flags; //筐发射额外受变换影响标记集合 参考 eTRANSF_TYPE 可以用 | 来组合多个标记 您无需指定 eTRANSF_OFS 默认会自动包含这个处理
    int8 aPose_range; //生成筐位置的幅度 值(-128----127) 0:随机产生
    int8 aAngle_range; //筐角度转向的幅度 值(-128----127) 0:随机产生 正数为顺时针旋转发射 负数为逆时针旋转发射 值越大幅度越大
    int8 aSize_range; //筐大小变化的幅度 值(-128----127) 0:随机产生 127:大小不变 为其他正数的话大小来回变化 为负数的话间段大小变化 绝对值越大变得越慢
    int8 aColor_range; //筐颜色变化的幅度 值(-128----127) 0:随机产生 127:颜色不变 为其他正数的话只有颜色幻彩变 为负数的话只有透明变化 绝对值越大变得越慢 常用的有 126 124 122 120 116 114 112 110 68

    cFRAME_LAUNCHER_();
    cFRAME_LAUNCHER_(const cFRAME_LAUNCHER_ &src);
    cFRAME_LAUNCHER_ &operator=(const cFRAME_LAUNCHER_ &src);
    ~cFRAME_LAUNCHER_() { this->Release(); }

    //释放内部资源
    void Release();

    //初始化(发射单元参照动画筐,   筐发射转向角度范围,    筐发射间隔时间,              一次的发射筐量,                     生成位置幅度,          生成角度幅度,       生成大小幅度,      生成颜色变化幅度,          活动发射时间 即终止时间,   附加效果的动画筐)
    void InitToData(cFRAME *_frame, float _gen_angle = 0.0f, unsigned int _gen_speed = 1000, unsigned short _gen_once_num = 1, int8 _pose_range = 0, int8 _angle_range = 0, int8 _size_range = 127, int8 _color_range = 127, unsigned int _activity_time = 0, cFRAME *_frame2 = 0);

    //保管外部连接对象 调用后 zFrame zFrame2 等外部对象数据不必在外释放了
    void KeepExternObjs() { zObjsKeep.New(1); }

    //更新播放( 更新时间 ) 返回发射器所经历的时间 返回0代表已经播放结束
    unsigned int PlayFrame(unsigned int updatetime);

    //完美的复制 这个连里面每个筐的位置时间数据都会复制过来(要复制的源对象)
    void PerfectCopy(const cFRAME_LAUNCHER_ &src);

    //设置生产筐的时间间隔( 间隔时间豪秒 ) !注意这里设置的时间不能快过初始化时设定的时间
    void SetGenerateSpeed(unsigned int _time);

    //设置一次生产筐的最大数目( 个数 )  !注意这里设置的个数不能多于初始化时设定的个数
    void SetGenerateOnceNum(unsigned short _once_num);

    unsigned int GetGenerateID(unsigned int id); //获取当前子元素id对应的筐单位实际创建顺序的ID(子元素id)返回实际创建顺序ID
    inline unsigned int GetGenerateCount() { return zGen_count; } //获得已经生成筐的个数

    inline unsigned int GetDisplayMaxCount() { return zMax_num; } //获得发射器能同时显示的最大筐单元数
    inline unsigned int GetDisplayCount() { return zDisplay_num; } //获得发射器播放时现在正在显示的筐个数

    inline void SetNowtime(unsigned int _time) { zObjNowtime = _time; } //设置发射器所经历的时间
    inline unsigned int GetNowtime() { return zObjNowtime; } //获得发射器所经历的时间
    inline unsigned int GetEndtime() { return zActivity_time + zEnd_time; } //获得播放结束时间 若发射器为不停地发射就返回单个动画筐的结束时间
    inline bool PlayIsEnd() { return zActivity_time && zObjNowtime > zActivity_time + zEnd_time; } //判断播放是否已经结束

    inline cFRAME *GetFrame() { return zFrame; } //获得发射单元动画筐

    inline void Pause() { zRunUpdate = !zRunUpdate; } //暂停或继续播放
    void StopGenerate(); //停止生成动画筐
    void ContinueGenerate(unsigned int _time = 0); //继续生成动画筐(继续发射生成筐多少时间 默认值0为永远不停发射生成)
    void ReSetPlaytime(); //重新设置发射器的动画时间 使到它恢复到初始状态

    void SetSubNowtime(unsigned int id, unsigned int _time); //设置指定编号筐单元的现在播放时间(编号, 时间) !该调用应在发射器已经停止生成之后 以免子ID经常变动产生难预测的结果
    void SetSubColor(unsigned int id, unsigned int color); //设置指定编号筐单元的颜色(编号, 颜色)         !该调用应在发射器已经停止生成之后 以免子ID经常变动产生难预测的结果
    void SetSubTS(unsigned int id, const sTRANSF2D *ts); //设置指定编号筐单元的变换信息(编号, 变换信息) !该调用应在发射器已经停止生成之后 以免子ID经常变动产生难预测的结果
};
class GN_API cFRAME_LAUNCHER : public cFRAME_LAUNCHER_ //★★★动画图筐发射器 用来随机产生很多相同动画图筐的类 效果类似粒子系统
{
  public:
    virtual void zCreateUnit(unsigned int id); //创建筐单元(单元ID)
    virtual void zPlayFrame2(unsigned int updatetime, unsigned int color); //播放附加效果的筐动画(更新时间, 色调)

    uint8 *zDataSave(uint8 *wpos, GN::sLIST<cFRAME *> &frames_collect, GN::sLIST<P<char>> &keep_obj_collect); //保存该对象的2进制数据(当前写入位置, 收集到的图框列表, 收集得到的保留鉴别对象)返回最后的写完位置
    uint8 *zDataLoad(uint8 *rpos, cFRAME **frame_refs, P<char> *keep_obj_refs); //读取该对象的2进制数据(当前读取位置, 图框引用组, 保留鉴别对象引用组)返回最后的读完位置
  public:
    float aGenRngW; //发射点区划范围宽
    float aGenRngH; //发射点区划范围高
    float aGenRngX; //发射点区划范围X
    float aGenRngY; //发射点区划范围Y

    cFRAME_LAUNCHER();
    cFRAME_LAUNCHER(const cFRAME_LAUNCHER &src);
    cFRAME_LAUNCHER &operator=(const cFRAME_LAUNCHER &src);

    void PerfectCopy(const cFRAME_LAUNCHER &src); //完美的复制 这个连里面每个筐的位置时间数据都会复制过来(要复制的源对象)

    void SetRange(const sRECT2 *_range); //设置发射点区划范围(范围矩形) !一旦调用这个 属性 aGen_ts_flags 会重置为0
    void GetRange(sRECT2 *range_out); //获得发射点区划范围(范围矩形输出)

    virtual sVEC2 GetCentre(); //获得发射点区划范围中心
};
class GN_API cFRAME_LAUNCHER2D : public cFRAME_LAUNCHER //★★★随机产生很多相同的动画筐的超级类,答配了复合效果
{
  public:
    virtual void zCreateUnit(unsigned int id); //创建筐单元(单元ID)
    virtual void zPlayFrames(unsigned int updatetime); //播放发射筐动画(更新时间)

    uint8 *zDataSave(uint8 *wpos, GN::sLIST<cFRAME *> &frames_collect, GN::sLIST<P<char>> &keep_obj_collect); //保存该对象的2进制数据(当前写入位置, 收集到的图框列表, 收集得到的保留鉴别对象)返回最后的写完位置
    uint8 *zDataLoad(uint8 *rpos, cFRAME **frame_refs, P<char> *keep_obj_refs); //读取该对象的2进制数据(当前读取位置, 图框引用组, 保留鉴别对象引用组)返回最后的读完位置

    float zGenRngSclX; //发射点区划缩放比列X
    float zGenRngSclY; //发射点区划缩放比列Y
    float zGenRngRot; //发射点区划旋转角度
    sVEC2 zGenRngAx; //发射点范围原轴方向X
    sVEC2 zGenRngAy; //发射点范围原轴方向Y

    void zCopyData2(const cFRAME_LAUNCHER2D &src, bool cpy_state); //复制该类独有的内部数据(复制源, 是否复制当前状态 否则重置为初始状态)
    void zSetPos(const cFRAME *f); //设置该对象摆放位置为指定动画筐的摆放位置(动画筐) !若属性 aGen_ts_flags 包含eMODULATE_COLOR 也使动画筐的色调传给该对象
  public:
    typedef cLAUNCHER_SEED<cFRAME_LAUNCHER2D, cFRAME> cSEED; //2D动画图框发射器种子

    P<P<cSEED>> aAddEffectSeeds; //筐单元附着子效果种子

    cFRAME_LAUNCHER2D();
    cFRAME_LAUNCHER2D(const cFRAME_LAUNCHER2D &src);
    cFRAME_LAUNCHER2D &operator=(const cFRAME_LAUNCHER2D &src);

    void PerfectCopy(const cFRAME_LAUNCHER2D &src); //完美的复制 这个连里面每个筐的位置时间数据都会复制过来(要复制的源对象)
    void ReSetPlaytime(); //重新设置发射器的动画时间 使到它恢复到初始状态

    void SetPos(const GNg::sTRANSF2D *ts); //设置该对象摆放信息(变换信息)
    void GetPos(GNg::sTRANSF2D *ts_out); //获得该对象摆放信息(变换信息输出)

    void GetRange(sRECT2 *range_out); //获得发射点区划范围(范围矩形输出)

    virtual sVEC2 GetCentre(); //获得发射点区划范围中心
};
template <class T, class T2>
class cLAUNCHER_SEED //★发射器种子
{
  public:
    T *zChilds; //子发射器数组
    unsigned int zMaxNum; //子发射器内存缓冲个数 也就是在同一时刻存在的最大数
    unsigned int zGenCount; //子发射器当前已生成个数
    unsigned int zBirthTime; //相对单元射出后的引发时间

    void zCreateChild(T2 *unit, unsigned int gid); //创建一个子发射器(变换位置跟随的射出单元, 要设置的是第几个子发射器)

    inline void zSetChildPos(T2 *unit, unsigned int gid) { //设置指定子发射器变换位置(变换位置跟随的射出单元, 要设置的是第几个子发射器)
        zChilds[gid % zMaxNum].zSetPos(unit);
    }
    void zCopyToChilds(T *style, unsigned int act_time, unsigned int st = 1); //复制参照式样到各个子发射器( 参照式样 , 子发射器活动发射时间 , 除首个子反射器外其它的复制起始子反射器ID)
    void zPerfectCopy(cLAUNCHER_SEED<T, T2> &src); //完美的复制 这会连当前状态都复制过来( 要复制的源对象 )
  public:
    bool aFollow; //引发后是否一直跟随射出单元变换位置

    cLAUNCHER_SEED() : zChilds(0) {}
    ~cLAUNCHER_SEED() { GN_DeleteArray(zChilds); }

    //初始化(父发射器, 参照式样发射器, 相对单元射出后的引发时间 默认为自动设为射出单元动画结束时间, 引发后是否一直跟随射出单元变换位置, 除首个子反射器外其它也要设置的起始子反射器ID)
    void InitToData(cFRAME_LAUNCHER_ *father, T *style, unsigned int birth_time = 0xffffffff, bool follow = false, unsigned int st = 1);

    //更新播放所有活动子发射器动画(更新时间)
    void UpdatePlay(unsigned int updatetime) {
        unsigned int i = 0;
        for(; i < zMaxNum; ++i) zChilds[i].PlayFrame(updatetime);
    }
    //使所有子发射器执行指定处理(处理函数(当前检索到的子发射器, 传回您的用户参数), 用户参数)
    void ChildsExec(void (*proc)(T *chld, void *us_arg), void *_us_arg) {
        T *_chd = zChilds, *chd_end = zChilds + zMaxNum;
        for(; _chd < chd_end; ++_chd) proc(_chd, _us_arg);
    }
    //重置所有子发射器的动画时间 使到它恢复到初始状态
    void ResetChildsTime() {
        unsigned int i = 0;
        for(; i < zMaxNum; ++i) zChilds[i].zObjNowtime = 0xffffffff;
    }
    //设置指定范围的子发射器为参照式样( 参照式样, 要设置的起始子发射器ID, 结束的子发射器ID)
    void SetRangeChilds(T *style, unsigned int st, unsigned int end);

    //获得源参照式样发射器
    T *GetSourceStyle() { return zChilds; }

    //获得相对单元射出后的引发时间
    unsigned int &GetBirthTime() { return zBirthTime; }
};
template <class T, class T2>
void cLAUNCHER_SEED<T, T2>::zCreateChild(T2 *unit, unsigned int gid) {
    unsigned int id = gid % zMaxNum;
    ++zGenCount;
    zChilds[id].ReSetPlaytime();
    zChilds[id].zSetPos(unit);
}
template <class T, class T2>
void cLAUNCHER_SEED<T, T2>::SetRangeChilds(T *style, unsigned int st, unsigned int end) {
    if(end > zMaxNum) end = zMaxNum;
    for(; st < end; ++st) {
        zChilds[st] = *style;
        zChilds[st].zObjNowtime = 0xffffffff;
    }
}
template <class T, class T2>
void cLAUNCHER_SEED<T, T2>::zCopyToChilds(T *style, unsigned int act_time, unsigned int st) {
    zChilds = new T[zMaxNum];
    *zChilds = *style;
    zChilds->zActivity_time = act_time; //不让无限时产生
    zChilds->zObjNowtime = 0xffffffff;
    for(; st < zMaxNum; ++st) {
        zChilds[st] = zChilds[0];
        zChilds[st].zObjNowtime = 0xffffffff;
    }
}
template <class T, class T2>
void cLAUNCHER_SEED<T, T2>::zPerfectCopy(cLAUNCHER_SEED<T, T2> &src) {
    *this = src;
    if(zChilds) {
        zChilds = new T[zMaxNum];
        unsigned int ci = 0;
        for(; ci < zMaxNum; ++ci) zChilds[ci].PerfectCopy(src.zChilds[ci]);
    }
}
template <class T, class T2>
void cLAUNCHER_SEED<T, T2>::InitToData(cFRAME_LAUNCHER_ *father, T *style, unsigned int birth_time, bool follow, unsigned int st) {
    unsigned int uint_end_time = father->zEnd_time - 1;
    zBirthTime = (birth_time > uint_end_time ? uint_end_time : birth_time);
    aFollow = follow;

    unsigned int act_time = (style->zActivity_time ? style->zActivity_time : father->zEnd_time - zBirthTime);
    unsigned int rng_time = act_time + style->zEnd_time;
    if(father->zActivity_time && father->zActivity_time < rng_time) rng_time = father->zActivity_time;
    unsigned int gen_count_max = rng_time / father->zMax_gen_speed;
    if(gen_count_max * father->zMax_gen_speed < rng_time) ++gen_count_max;
    zMaxNum = gen_count_max * father->zMax_once_num;
    zGenCount = 0;

    GN_DeleteArray(zChilds);
    zCopyToChilds(style, act_time, st);
}
/********************************************************* cFRAME_LAUNCHER_FONT *****************************************************************************************************************************/

class GN_API cFRAME_LAUNCHER_FONT : public cFRAME_LAUNCHER, public sDRAW_ARRANGE //★★★动画艺术字筐发射器 该类功能很广 可以显示漂亮文字还可以按编号有序排列显示图片 而且带有鼠标触摸检测 也很适合做漂亮的UI
{
  public:
    void zCopyFontsData(const cFRAME_LAUNCHER_FONT &src, bool cpy_state); //复制该类专属内部数据(复制源, 是否复制已发射的字框当前状态 否则重置为初始状态)
    void zRelease(); //释放内部一些资源
    static GNg::sDRAW_AREA *zDrawRangeInfo; //这个通常由外部的类分配好足够空间后 再传个该类对象把显示位置信息都记录多份进这里去 这里的数据个数一定要和 zMax_num 一致 !对于还未够时生成的字筐位置信息将不会记录进去

    cFRAME *zFontFrame; //艺术字体式样动画筐
    float ztCU; //临时记录旋转中心点U
    float ztCV; //临时记录旋转中心点V

    uint8 *zActState_s; //各个字筐的演变状态 值0:未启动 1:增加运动时间 2:减少运动时间
    unsigned int *zActTime_s; //各个字筐的动态控制演变相对时间
    cFRAME *zActFrame; //动态控制演变参照动画筐
    bool zActMouseAuto; //是否要根据鼠标自动控制演变
    int8 zActMouseKey; //要根据鼠标那个键位来控制演变

    bool zUsScissor; //是否启用剪裁
    uint8 zTrackingState; //当前是否启用字筐范围跟踪 启用这个会损耗一些性能
    float zPointX; //指定追查点的坐标X
    float zPointY; //指定追查点的坐标Y
    unsigned int zSelectedId; //当前选中的字筐ID

    bool zIsFontMap; //是否为字体图
    bool zIsGenEndStop; //是否生成到文本结束就停止生成
    unsigned short zSrcAmMode; //记录原本的弹出动画播放模式
    unsigned short zGen_once_numB; //初始化时 zGen_once_num 的备份

    bool zToActFramePrco(unsigned int id, unsigned int updatetime); //筐动态演变处理(可视字筐ID, 更新时间)返回指定字筐是否可忽略
    void zPlayFontFrame(unsigned int id); //传输数据到字筐并播放显示(可视字筐ID)
    void zPlayFontFrameAndRangeCollect(unsigned int id); //传输数据到字筐并播放显示并收集该字筐范围(可视字筐ID)

    uint8 *zTextSave(uint8 *wpos); //保存该对象里的文本或图序数据(当前写入位置)返回最后的写完位置
    uint8 *zDataSave(uint8 *wpos, GN::sLIST<cFRAME *> &frames_collect, GN::sLIST<P<char>> &keep_obj_collect, bool inc_text = true); //保存该对象的2进制数据(当前写入位置, 收集到的图框列表, 收集得到的保留鉴别对象, 保存是否含文本内容)返回最后的写完位置
    uint8 *zDataLoad(uint8 *rpos, cFRAME **frame_refs, P<char> *keep_obj_refs, bool set_text = true); //读取该对象的2进制数据(当前读取位置, 图框引用组, 保留鉴别对象引用组, 是否一并读取设置文本内容)返回最后的读完位置

  public: //★用户接口 ---------------------------------------------------------------------------------------------------------------------------------------
    static char aPicIndex[128]; //按由0到127顺序的图片索引编号 方便初始化时用

    cFRAME_LAUNCHER_FONT();
    cFRAME_LAUNCHER_FONT(const cFRAME_LAUNCHER_FONT &src);
    cFRAME_LAUNCHER_FONT &operator=(const cFRAME_LAUNCHER_FONT &sr);
    ~cFRAME_LAUNCHER_FONT();

    void Release(); //释放内部资源

    //初始化
    void InitToData(
        cFRAME *_font_frame, //字体式样动画筐或普通图片动画筐 !调用后这个筐里一些信息将受到改变 如果发射器重新再次初始化时最好这个动画筐也一并初始化一下
        char *_text, //要显示的文本
        unsigned short _textlen, //显示的文本长度 0表示自动输出全部
        sRECT2 *_gen_range, //筐发射点范围
        sARRANGE_FMT *_parame = &ArrangeFmt, //文本排列格式
        float _gen_angle = 0.0f, //筐发射转向角度范围
        unsigned int _gen_speed = 1000, //筐生产间隔时间
        unsigned short _gen_once_num = 0, //一次发射筐数 0表示一次过显示全部
        int8 _pose_range = 127, //位置生成幅度
        int8 _angle_range = 0, //角度生成幅度
        int8 _size_range = 127, //大小生成幅度
        int8 _color_range = 127, //生成颜色变化幅度
        unsigned int _activity_count = 0, //生成多少个字停止 0表示生成到文本的结尾就停止
        cFRAME *_frame = 0, //附加在表面的动画筐
        cFRAME *_frame2 = 0, //附加效果的动画筐
        cFRAME *act_change_frame = 0, //动态控制演变参照动画筐
        bool is_pic = false //标明是否用于排列图片而不是文本 如果为ture那么第二个参数_text就应该传入一个图片索引数组进去:uint8 index[5]={0,1,0xff,2,1};并且第三个参数必需指定索引数组元素的个数 图片的索引值最大为254 索引值:0xff为提示换行
    );

    //重设文本显示内容(文本, 文本字数:0为显示全部, 文本在筐内的X偏移量, 文本在筐内的Y偏移量, 是否要重设每个显示字筐的动画状态）!!设置之后字筐的生成数将重新计算 一般使它能重设置性能更高 如果在只是单纯偏移拖动区划而要重设置文本时又不想内容的动画重新播放造成闪动则把参数reset_am设为:false
    void SetText(char *text, unsigned short tlen, int offset_x = 0, int offset_y = 0, bool reset_am = true);
    //设置子文本段(从该系统的第几个开始重设替换, 子文本内容）
    void SetSubText(unsigned int id, char *text);

    //更新播放(更新时间)返回发射器所经历的时间 全播放完返回0  !如要设置分段播放一般都是到一定的时间将它重新初始化一偏来实现 动画筐的节点分段不能用在这里
    unsigned int PlayFrame(unsigned int updatetime);
    //更新播放(更新时间, 实时色调)返回发射器所经历的时间 全播放完返回0  !如要设置分段播放一般都是到一定的时间将它重新初始化一偏来实现 动画筐的节点分段不能用在这里
    unsigned int PlayFrame(unsigned int updatetime, unsigned int color);

    //完美的复制(要复制的源对象)这个连里面每个筐的位置时间数据都会复制过来
    void PerfectCopy(const cFRAME_LAUNCHER_FONT &src);

    //设置选择点(选择点坐标X, 选择点坐标Y) 这函数用来告诉PlayFrame()下次播放时要顺便帮它找出指定点选中了那个字筐 !这一般放在PlayFrame()之前使用 经过调用PlayFrame()处理后再用GetSelectFontFrame()接收结果
    void SetSelectPoint(int x, int y);
    //获得选择中的字筐(返回被选中的字筐ID)返回是否找到点选目标 没找到返回0
    bool GetSelectFontFrame(unsigned int *id_out);

    //从指定筐选择获取多个字筐(指定筐, 选中的字筐个数输出) 返回字筐的ID数组 该数组会自动帮你分配好足够的空间 用完记得delete [] 这函数为提高性能只是大概的检索一下摆放位置 对于设置了动画弹出或经过旋转偏移改变了的字筐有一定偏差
    unsigned int *GetFontFrameID_Data(const sRECT *rect, unsigned int *id_num_out);

    void SelectActChange(unsigned int id); //选择设置指定字筐为动态演变目标(可视字筐ID)
    void CancelActChange(unsigned int id); //取消指定字筐动态演变目标(可视字筐ID)
    void ControlActFromMouseAuto(bool yes, short onDown = -1); //按鼠标来自动控制动态演变(是否启用, 需要按下鼠标那个键才起作用 默认值-1:不需要按下键) !!如果开了这个就没必要使用SetSelectPoint(int x,int y)设置选择点了

    void Move(int x, int y); //便捷移动整个筐里的内容 如果您只想平移整个范围 调用这个比 SetText 块很多
    void ToHide(); //要求现在生成的所有字筐立即反向播放到播完一次消失为止
    void StopGenerate(); //停止生成字筐
    void ContinueGenerate(unsigned int count = 0); //继续生成字筐(继续生成多少个字筐 值0:永远不停生成)
    void ReSetPlaytime(); //重新设置发射器的动画时间 使到它恢复到初始状态

    void GetTextRange(sRECT *_range_out, bool is_stark = true); //获取确定排好的所有字筐整个范围(范围输出, 是否绝对范围)
    void GetTextRange(sRECT2 *_range_out, bool is_stark = true); //获取确定排好的所有字筐整个范围(范围输出, 是否绝对范围)
    bool GetFontFrameRange(unsigned int id, sQUADS2 *out); //获取现时指定字筐的判断范围四边形(可视字筐ID, 四边型输出) 返回1表示成功 返回0表示失败或字筐已经播放完消失
    inline cFRAME *const GetFontFrame() { return zFontFrame; } //获得艺术字体式样动画筐
    inline sIMAGE *GetFontMap() { return zFontFrame->zPlay_obj; } //获得该对象正在使用的字体图()

    unsigned int GetEndtime(); //获得播放结束时间
    inline bool PlayIsEnd() { return zObjNowtime > GetEndtime(); } //判断播放是否已经停止
};
/********************************************************* cLENS_FLARE *****************************************************************************************************************************/

class GN_API cLENS_FLARE //★★镜头光环效果
{
  protected:
    sIMAGE zImg; //光环式样图
    sVEC2 zCp; //中心对齐点
    unsigned int zNowtime; //镜头光动画现在时间
    unsigned int zEndtime; //镜头光到结束的间隔时间
    unsigned int zFlareColor; //光环色调和亮度颜色
    bool zAm_s_e; //标记到结束还是开始

  public:
    //初始化镜头光环 (光环式样图路径文件名,   大小比率              闪动间隔时间,               闪动强度                       屏幕宽,                  屏幕高)
    bool InitToData(char *imgfile_name = 0, float size = 1.0f, unsigned int flash_intvl = 500, unsigned char flash_ins = 50, short vw = ViewCur->zBackScreenW, short vh = ViewCur->zBackScreenH);

    //显示镜头光环效果(帧间隔时间,              光源坐标X,        光源坐标Y,                    光强颜色,   是否显示更多的光环)
    void Display(unsigned int updatetime, float light_src_x, float light_src_y, unsigned int color = 0xffffffff, bool boost_up = true);

    uint8 *zDataSave(uint8 *wpos, char *imgexp_path = 0); //保存该对象的2进制数据(当前写入位置, 式样图导出路径 默认0不导出图文件)返回最后的写完位置
    uint8 *zDataLoad(uint8 *rpos, char *load_path_); //读取该对象的2进制数据(当前读取位置, 当前读取路径 当有读取到图片时会在尾部续上图片名)返回最后的读完位置
};
struct GN_API cLENS_FLARE_OBJ //★★镜头光环效果对象
{
    P<cLENS_FLARE> aStyle; //光环效果式样
    float aSrcX; //光源坐标X
    float aSrcY; //光源坐标Y
    unsigned int aColor; //光强颜色
    uint8 aShowFlags; //额外显示标记 1:更多的光环 2:可被遮挡  可用|组合多个标记 !使用遮挡会增加一些性能开销

    void PosDraw(); //光环位置绘制 如需遮挡处理需要在显示光环效果前调用一下这个
    void Display(unsigned int updatetime); //显示镜头光环效果(帧间隔时间)

    uint8 *zDataSave(uint8 *wpos, GN::sLIST<P<cLENS_FLARE>> &flare_collect); //保存该对象的2进制数据(当前写入位置, 收集到的光环效果式样列表)返回最后的写完位置
    uint8 *zDataLoad(uint8 *rpos, P<P<cLENS_FLARE>> &flare_refs); //读取该对象的2进制数据(当前读取位置, 光环效果式样引用组)返回最后的读完位置
};
}; // namespace GNg
#endif //endif _GN_FRAME_H_
