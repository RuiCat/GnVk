#ifndef _GN_INIT_H
#define _GN_INIT_H
#ifndef _GN_H_
#ifndef GN_INIT_API
#define GN_INIT_API GN_API
#endif
#endif
/*************************************************************** 供系统接口开发用的主程序调用 ***********************************************************************************************************/

//指定应用程序运行参数(应用程序标识, 控制台调用参数个数, 控制台调用参数数组, Windows系统调用命令 ) 参数argv 和 cmdstr 通常是二选一的 !做系统接口开发的应该在程序入口的开头就调用这个来指定程序运行参数
GN_INIT_API void GF_AppArgsSet(const int *app, int argc=0, char **argv=0, const char *cmdstr=0);

#ifdef _GN_H_
//使当前程序暂停挂起一定时间(指定时间:单位毫秒)
GN_INIT_API void GF_Sleep(unsigned int ms);

//将要开始帧循环时的初始化和加载资源任务处理 !!做系统接口开发的 在准备进入帧循环前需要调用这个来初始化系统各个模块和加载用户资源
GN_INIT_API void GF_FrameCycleBeginProc();

//帧循环任务处理1(帧间隔时间) !!做系统接口开发的需把这个函数挂在帧循环的地方每帧都调用 以确保能正确完成内部所需处理的事情 - 这个一般多处理3D渲染任务
GN_INIT_API void GF_FrameCycleProc1(unsigned int nTime);

//帧循环任务处理2(帧间隔时间) !!做系统接口开发的需把这个函数挂在帧循环的地方每帧都调用 以确保能正确完成内部所需处理的事情
GN_INIT_API void GF_FrameCycleProc2(unsigned int nTime);

//退出帧循环时的清理任务处理  !!做系统接口开发的 在退出帧循环时程序将要结束前需要调用这个来正确清理引擎内部资源 和 用户资源
GN_INIT_API void GF_FrameCycleEndProc();

/***************************************************************** 特殊目录和文件 ***************************************************************************************************************************/

extern GN_INIT_API const char *GN_RunPauseFileName;  //运行暂停通告文件名
extern GN_INIT_API       char *GN_RunStateFileName;  //运行状态记录文件名 你可以把该指针指向另一个名 来更变状态记录文件名 如果作为服务器开发时 一般都把该名更变一下加上一个日期时间 以保留每次运行结果待管理查询 而且又可以防止引擎检测到相同的文件而弹出错误修复窗口卡住服务程序运行

//得到一个用引擎核心系统目录续上指定路径文件名的路径文件名(路径文件名)返回路径文件名 这个便于用户读取引擎核心共享资源定位用的 因为有时版本不同这个目录也会不同
GN_INIT_API char *const GN_SysDir(const char *path_file);

/**************************************************************** 用户应用资源目录 **************************************************************************************************************************/

//得到一个用引擎当前用户资源目录续上指定路径文件名的路径文件名()返回路径文件名 这个便于用户读取自身资源定位用的 因为用户资源可能是固定的单机模式存在于本地 也可能是在网上更新下载回来的 这时它的存放相对目录可能会有差异! 该函数通常在引擎回调 读取用户资源函数如"AppInit"时才开始生效
GN_INIT_API char *const GN_AppDir(const char *path_file);
//设置引擎当前用户资源目录(目录路径) 示范: GN_AppDirSet("app/source");
GN_INIT_API void        GN_AppDirSet(const char *dir);
//获得引擎当前用户资源目录(目录路径输出)
GN_INIT_API void        GN_AppDirGet(char *dir_out/*[384]*/);

/**************************************************************** 引擎内部一些默认值 *************************************************************************************************************************/

extern GN_INIT_API unsigned short GT_AppDefault_ExtModule;      //引擎用到的扩展模块
extern GN_INIT_API unsigned char  GT_AppDefault_MsgCode;        //与外部通讯的消息编码 0:使用Unicode 1:使用Ansi 2:Unicode换为Ansi
extern GN_INIT_API unsigned char  GT_AppDefaultFontStyle;		//应用默认使用字体式样
extern GN_INIT_API unsigned int   GT_AppDefaultColorFont;		//应用默认字体颜色
extern GN_INIT_API unsigned int   GT_AppDefaultColorBack;		//应用默认底框颜色
extern GN_INIT_API unsigned int   GT_AppDefaultColorFocus;		//应用默认高亮颜色

#ifdef GN_USING_UPDATE
/************************************************************************ 自动更新 **************************************************************************************************************************/
#endif
extern GN_INIT_API P<char>		  GT_AppDefaultSrcDownloadURL;  //默认应用资源下载目录连接
extern GN_INIT_API P<char>		  GT_AppDefaultSrcDownloadDir;  //默认应用资源下载目录

#ifdef GN_USING_UPDATE
/*------------------------------------------------------------ 关于更新包制作方法 ------------------------------------------------------------------------------------------------------

推荐用引擎配套软件 FileManage 通过它的资源更新储备目录为您自动打包出更新包
引擎会自动下载的更新包分为3种, 每种里再分成两类, 由于 FileManage 打包出来的包文件名都相同, 所以引擎会以以下的目录结构来索取对应的功能更新包

1a)核心系统常规依赖资源包 AppUpdate程序也应放到该包里		引擎公司网址/load/lib/
1b)核心系统所包含的执行程序模块包							引擎公司网址/load/lib/Bin_win32/
2a01)扩展系统1常规依赖资源包								引擎公司网址/load/lib/GN_Ext01/
2b01)扩展系统1所包含的执行程序模块包						引擎公司网址/load/lib/GN_Ext01/Bin_win32/
3a)应用程序常规依赖资源包									应用公司网址/load/lib/MyAppName/
3b)应用程序所包含的执行程序模块包							应用公司网址/load/lib/MyAppName/Bin_win32/

!第1和第2种引擎内部会自动完成 用户可不必理会
!这里的 Bin_win32 为对应运行平台的程序模块目录名 如果模块为64位 则对应为 Bin_win64 如果为linux平台的模块则对应为 Bin_linux 具体后续平台名可用 GN_StrGetSysName 获得
!这里的 GN_Ext01 为对应扩展模块的下载位置 若有多个扩展模块分别多个目录 GN_Ext02 GN_Ext03 GN_Ext04 ...等
!这个 MyAppName 可由用户自指定义 主要是通过 GN_InitParameExtSrc 参数src 来指定 如果无指定不会下载应用相关更新包
在 FileManage 里 您需要分别为每项创建关联的储备目录 然后将每项相关的文件通过 FileManage 复制到关联储备目录里 程序模块文件在包里尽量不要再用目录包含着
然后再把打包出的更新包和版本文件 UpdateSV.txt 放到每项对应功能的网上目录里

!如果您所开发的应用仍然希望使用引擎的旧版本 不想引擎核心库更新会影响到应用代码模块的兼容性 您可以在调用 GN_InitParameExtSrc 给参数 ext_module | 结合1024 即可!
!此后如您又升级了引擎版本 但可能不是最新的 希望手动选择引擎版本来更新! 您可以把相关版本的引擎程序模块文件和您的应用程序模块文件放一起打包 核心依赖资源文件也和应用依赖资源文件一起打包

--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/

enum GN_eUPDATE_STATE_TYPE{                   //更新包下载状态 对应 GN_UpdateCheck 返回的线程成员->us_state
     GN_eUPDATE_STATE_DOWNLOADING        = 1,   //正在下载中 如果无该位 表示下载任务已经完成了, 如果中途调用了 GN_UpdateCancel 取消下载更新, 该位也会变为空的

     GN_eUPDATE_STATE_SRC_ERR_NET        = 2,   //下载一般资源时存在网络错误
	 GN_eUPDATE_STATE_SRC_ERR_FILE       = 4,   //下载一般资源时存在本地文件访问错误
     GN_eUPDATE_STATE_SRC_ERR_REPLACE    = 8,   //更新覆盖替换一般资源时存在错误
     GN_eUPDATE_STATE_LOAD_MODULE        = 16,  //有该位表示呈有程序模块下载
     GN_eUPDATE_STATE_MODULE_ERR_NET     = 32,  //下载程序模块时存在网络错误
	 GN_eUPDATE_STATE_MODULE_ERR_FILE    = 64,  //下载程序模块时存在本地文件访问错误
	 GN_eUPDATE_STATE_MODULE_ERR_REPLACE = 128, //更新覆盖替换程序模块时存在错误
};
//更新检测(更新下载的目录连接, 下载到本地的目录, 下载进度输出 在有下载任务完成前必须保证该变量是生效的, 下载好的更新包是否全部都要解压 默认值为自动适应) 如检测有更新返回下载任务线程 你可以通过该成员->us_state来监测下载状态 如没有更新返回空的
GN_INIT_API P<GN_sTHREAD> GN_UpdateCheck(const char *src_dir_url,const char *tg_dir, float *prog_out, bool unpack_all=false);

//强制取消更新任务并清除它相关的资源( 下载任务线程 )
GN_INIT_API void GN_UpdateCancel(P<GN_sTHREAD> task);

//获得引擎内部更新任务() 如有更新任务就返回该任务 否则返回空的 由于程序刚开始执行时引擎都会自动检测更新 你可以用这个函数得知引擎是否正在更新中
GN_INIT_API P<GN_sTHREAD> & GN_UpdateInternalTaskGet();

//用系统默认方式显示更新下载进度并显示出错原因(下载任务线程, 进度0.0f-1.0f, 显示的信息, 是否主要显示 这会用整个窗口来显示进度画面)
GN_INIT_API void GN_UpdateProgressDefaultShow(P<GN_sTHREAD> &task , float prog, const char *info, bool primary);

GN_INIT_API void GT_UpdateAllTaskSet(bool sys_up_ignore=false);  //检测所有更新任务并设置它(是否忽略系统核心库的更新) 如果有检测到更新将会暂停所有用户应用回调处理 等更新完后再继续处理用户应用回调
GN_INIT_API void GT_UpdateAppSetNextRequired(const char *tg_dir);//设置指定应用目录下的所有模块和资源在下次运行时都必需要更新
GN_INIT_API bool GT_UpdateAppModuleReCheck(char *mfn);           //用户应用模块强行重新检测更新(程序模块文件名)返回是否成功 !调用这个一般有次数限制以免造成递归死了 超出了总是会不成功的 !这里模块名不需要加扩展名 !!这个常用于在加载程序模块文件时遇到失败时调用的 以检测下载到完整的程序模块 再去尝试加载它

GN_INIT_API bool GN_CallBackFuncToReRun();  //通知引擎移除所有应用处理回调 并重新按正确顺序再运行它 返回这些回调是否在动态库里装载的 如果是在动态库里装载调用的就一并更新用户程序模块
#endif
GN_INIT_API bool GN_CallBackFuncIsNull();   //检测引擎是否还未设置应用处理回调这些回调对应 GN_InitLoadCallBackFunc和GN_InitLoopCallBackFunc设置的
GN_INIT_API void GN_CallBackLoadFuncIsNull(bool *nulls/*[2]*/); //检测引擎对应GN_InitLoadCallBackFunc里设置的两个回调是否还未设置(返回两个回调是否还未设置)
GN_INIT_API void GN_CallBackLoopFuncIsNull(bool *nulls/*[3]*/); //检测引擎对应GN_InitLoopCallBackFunc里设置的三个回调是否还未设置(返回三个回调是否还未设置)

extern GN_INIT_API void (*GT_RunBreakLoopProc)();  //如果有指定该函数指针 在程序刚退出循环运行时便会调用它指向的处理函数 系统默认为空的 !!在该函数里最好不要调用引用到须加载的用户资源
extern GN_INIT_API void (*GT_RunEndProc)();        //如果有指定该函数指针 在程序运行到最后一刻时便会调用它指向的处理函数 系统默认为空的 !!在该函数里最好不要调用引用到须加载的用户资源和系统资源设备被清理后不能用的东西

/******************************************************************** 用户应用程序处理的回调方法 ******************************************************************************************************/

#define GN_APP_MODULE_NAME "App" //引擎默认加载的应用程序模块名 !!这里模块名不需要加扩展名 因为系统不同有所差异 具体的实物: App.dll 或 App.so 或 App.dylib

//读取程序模块文件里的5个特定处理回调 (程序模块文件名, 返回读取用户资源的函数, 返回清理资源的函数, 返回显示2D画面的函数, 返回显示3D画面的函数, 返回后台不断运行处理的函数)返回是否成功  !!这里模块名不需要加扩展名 因为系统不同有所差异 模块里面的5个调用函数名必须用以下的命名 详细参看下面的: 引擎前台开发完整框架程序示例 里的5个用户接口函数名
GN_INIT_API bool GN_LoadCallBackFuncFromModule(const char *module_name, GT_PFUNC_VOID &load_func, GT_PFUNC_VOID &exit_func, GT_VOID_PFUNC_UINT &display2D_func, GT_VOID_PFUNC_UINT &display3D_func, GT_VOID_PFUNC_UINT &run_func);

//加载程序模块文件来设置应用程序的5个处理回调 (程序模块文件名)返回是否成功 !!这里模块名不需要加扩展名 因为系统不同有所差异 模块里面的5个调用函数名必须用以下的命名 详细参看下面的: 引擎前台开发完整框架程序示例 里的5个用户接口函数名
GN_INIT_API bool GN_InitCallBackFuncFromModule(const char *module_name);

//移除从程序模块文件里加载的一些处理回调 在此之后就不能再用那个程序模块文件里的函数了
GN_INIT_API void GN_CallBackFuncModuleRelease();

//设置程序开始和退出后清理资源回调函数(读取用户资源的回调, 清理资源的回调) !!因为有时可能会遇到程序非法关闭,设置这函数能更安全释放用户资源
GN_INIT_API void GN_InitLoadCallBackFunc(void (*load_func)(void), void (*exit_func)(void));

//设置程序循环回调函数(显示2D画面的回调:常用于处理主逻辑和2D显示, 显示3D画面的回调, 后台不断运行处理的回调:常用于处理文件下载或运算处理极大量数据) !!这个是作为前台程序必须要设置的
GN_INIT_API void GN_InitLoopCallBackFunc(void (*display2D_func)(unsigned int), void (*display3D_func)(unsigned int)=0, void (*run_func)(unsigned int)=0);

#endif // _GN_H_

/************************************************************************ 应用程序初始化方法 ******************************************************************************************************/

struct GN_INIT_API GN_sINIT //初始化参数结构 基本的初始化参数 对应初始化配置ini里所描的
{
    short            vieww;			/*窗口宽度:0表示用当前桌面分辨率大小 在一些不支持窗口显示模式的系统下将忽略该值 但在移动端设置为锁屏不能旋转时可用来确定初始为横屏 宽>高*/
	short            viewh;			/*窗口高度:0表示用当前桌面分辨率大小 在一些不支持窗口显示模式的系统下将忽略该值 但在移动端设置为锁屏不能旋转时可用来确定初始为竖屏 高>宽*/
	short            colorbit;		/*颜色位 一般取值0为自动 移动端如要使用高级3D渲染指定32为好 该值一般在全屏模式下才有效*/
    short            rangeleft;		/*取景范围左*/
	short            rangetop;		/*取景范围上*/
	short            rangeright;	/*取景范围右 取值0为自动*/
	short            rangebottom;	/*取景范围下 取值0为自动*/
	bool			 depthbuffer;	/*是否额外分配3D原尺寸渲染用的深度缓冲 如为2D应用或确定使用缩放渲染时可不用 以节省空间*/
	unsigned char	 msbuffer;		/*是否额外分配3D原尺寸msaa抗锯齿用的多重采样缓冲 指定大于1的值分辨率小时才分配大则不分配 如为2D应用或确定使用缩放渲染或其它抗锯齿时可不用 以节省空间*/
    bool             zoom;			/*窗口可否拉伸 在一些不支持窗口显示模式的系统下该值用来标记视图是否能自动适应旋屏*/
	bool             full;			/*是否全屏 Android系统不支持这里设置全屏 只能在xml配置文件里设置*/
	bool             showmouse;		/*系统默认指针是否可见 在只有触摸屏的系统下将忽略该值*/
	unsigned short   extmodule;		/*依赖到的扩展模块*/
	unsigned char    msgcode;		/*外部通讯消息编码 0:使用Unicode 1:使用Ansi 2:Unicode换为Ansi */
    unsigned char    fontstyle;		//应用默认使用字体式样
    unsigned int     fontcolor;		//应用默认字体颜色
	unsigned int     backcolor;		//应用默认底框颜色
	unsigned int     focuscolor;	//应用默认高亮颜色
    char             src[384];		//应用资源目录
	char             caption[128];  //窗口标题名

	//使用默认值来初始化()
    void InitDefault();
	//从读取对应ini文件来初始化(ini路径文件名)返回是否成功
	bool InitLoadFromFile(const char * ini_filename);
};
#ifdef _GN_H_

//按ini文件描述设置来完整运行应用程序(ini路径文件名) !!这个函数功能集成了 GN_InitParameSystemStyle 和 GN_InitParameExtSrc 和 GN_StartRun 只是参数由ini文件里获得
GN_INIT_API void GN_StartRunFromFile(const char *ini_filename);

/*-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/

//按现有窗口来完整运行应用程序(初始化参数结构, 用户应用资源下载到本地的保存目录 仅支持局部路径, 现有窗口标识) !!这个函数功能集成了 GN_InitParameSystemStyle 和 GN_InitParameExtSrc 和 GN_StartRun  只是使用现有的窗口而已 如果没提供窗口也会在内创建一个主窗口
GN_INIT_API void GN_StartRunFromWindow(const GN_sINIT* _init_arg, const char *tg_dir, const int *hwnd=0);

/*-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
#ifdef GN_USING_UTILITY_LIB
//设置应用程序初始默认式样(系统默认字体式样, 系统默认字体颜色, 系统默认显示底色, 系统默认高亮显示颜色)
GN_INIT_API void GN_InitParameDefaultStyle(unsigned char fontstyle, unsigned int  fontcolor=0xffffffff, unsigned int  backcolor=0x88000000, unsigned int  focuscolor=0xffaf4f00);
#endif
//设置应用程序资源下载目录与扩展依赖(用户应用资源下载目录连接:如果在连接末尾有">>>"时将自动开启自动更新功能, 用户应用资源下载到本地的保存目录 仅支持局部路径, 应用端使用到的扩展模块, 外部通讯的消息编码 0:使用Unicode 1:使用Ansi 2:Unicode转为Ansi) !!如果有指定了源 引擎核心库和扩展模块也会一并检测更新  扩展模块参数取值 1;视频媒体库 1024:不检测核心库 多项可用 | 结合一起
GN_INIT_API void GN_InitParameExtSrc(const char * src, const char *tg_dir=0, unsigned short ext_module=0, unsigned char msg_code=0);

//使用完整功能配置与参数指定的设置来运行应用程序 这会一并帮你初始化引擎各个系统功能模块 如自动调用 GN_InitWindow(...); GN_InitGraphics(...); GN_InitNetwork(...); GN_InitSound(...)...和自动检查更新等 常规应用程序一般都用这个来启动运行 这时你可以省去手动调用GN_InitWindow(...)等模块配置的麻烦!
GN_INIT_API void GN_StartRun(
						 const char *caption=0,          /*窗口标题名:0表示使用一个引擎默认窗口标题名 !!Android系统不支持这里设置标题 只能在xml配置文件里设置*/
						 short       view_w=800,         /*窗口宽度:0表示用当前桌面分辨率大小 在一些不支持窗口显示模式的系统下将忽略该值 但在移动端设置为锁屏不能旋转时可用来确定初始为横屏 宽>高*/
						 short       view_h=600,         /*窗口高度:0表示用当前桌面分辨率大小 在一些不支持窗口显示模式的系统下将忽略该值 但在移动端设置为锁屏不能旋转时可用来确定初始为竖屏 高>宽*/
						 short       color_bit=0,        /*颜色位 一般取值0为自动 移动端如要使用高级3D渲染指定32为好 因为默认0很可能帮您创建的是16位色深 该值一般在全屏模式下才有效*/
						 bool        full_screen=0,      /*是否全屏 Android系统不支持这里设置全屏 只能在xml配置文件里设置*/
						 bool        window_zoom=1,      /*窗口可否拉伸 在一些不支持窗口显示模式的系统下该值用来标记视图是否能自动适应旋屏*/
						 bool        cursor_show=1,      /*系统默认指针是否可见 在只有触摸屏的系统下将忽略该值*/
						 short       back_rng_left=0,    /*取景范围左*/
						 short       back_rng_top=0,     /*取景范围上*/
						 short       back_rng_right=0,   /*取景范围右 取值0为自动*/
						 short       back_rng_bottom=0,  /*取景范围下 取值0为自动*/
						 bool		 depth_buf=true,	 /*是否额外分配3D原尺寸渲染用的深度缓冲 如为2D应用或确定使用缩放渲染时可不用 以节省空间*/
						 uint8		 ms_buf=1			 /*是否额外分配3D原尺寸msaa抗锯齿用的多重采样缓冲 指定大于1的值分辨率小时才分配大则不分配 如为2D应用或确定使用缩放渲染或其它抗锯齿时可不用 以节省空间*/
						 );

/* ------------------------------------------------------------------- 退出运行 ---------------------------------------------------------------------------------------------------------------------*/

//退出应用程序运行 !!这个和 GN_GameBreakLoop 的区别在于 GN_GameBreakLoop 一调用整个程序都会强制性退出来 而 GN_BreakRun 会受 GN_BreakRunProcChange 影响可以只退出到上一层 一般做应用开发的都适合用这个 GN_GameBreakLoop适合做系统平台框架那端的开发
GN_INIT_API void GN_BreakRun();

//设置应用程序关闭前的处理(函数指针) 有指定这个函数当用户点了窗口的X按钮或其它正常关闭程序时便会先调用它 该函数通常是做为结束提示用户用 !!这个和设置 GN_GameBreakLoopProc 的区别在于 这个同上可以受到 GN_BreakRunProcChange 的影响 比较适合做应用开发 而 GN_GameBreakLoopProc 适合做系统平台框架那端的开发
GN_INIT_API void GN_BreakRunBeforeProcSet( void (*proc)());

//退出应用程序的处理更改(更改为的函数指针:0为恢复原来的默认处理方式)返回是否成功 该函数不能嵌套调用 只能一进一出地调用 否则会失败! 这个是用来改变调用 GN_BreakRun 的行为的 该函数一般是给开发系统平台框架用的
GN_INIT_API bool GN_BreakRunProcChange(void (*other_proc)());

extern GN_INIT_API P< P<char> > GN_ExecArgs;  //主程序被执行时的入口参数

#endif //_GN_H_

#if 0/****************************** 引擎前台开发完整框架程序示例: 以下带'▲'符号的为用户接口 ************************************/
#include<GN.h>
extern "C"{
GN_CALL void AppInit(){     //▲初始化应用必须数据 程序开始准备好引擎系统后会调用它一次

}
GN_CALL void AppUnInit(){   //▲释放应用必须数据 程序运行结束前会调用它一次

}
GN_CALL void AppDisplay2D(unsigned int nTime){ //▲显示2D画面 程序运行每帧都调用它

}
GN_CALL void AppDisplay3D(unsigned int nTime){ //▲显示3D画面 程序运行每帧都调用它

}
GN_CALL void AppRun(unsigned int nTime){       //▲运行程序后台的处理 程序运行每帧都调用它

}
}
/*-------------------------------------------- ★程序入口点★ --------------------------------------------------------------------*/
GN_MAIN;
    GN_InitLoadCallBackFunc( //设置读取回调函数
	    /*--读取资源的回调▲*/ AppInit,
        /*--释放资源的回调▲*/ AppUnInit
	);
	GN_InitLoopCallBackFunc( //设置循环回调函数
		/*显示2D画面的回调▲*/ AppDisplay2D,
		/*显示3D画面的回调▲*/ AppDisplay3D,
		/*--运行处理的回调▲*/ AppRun
	);
	GN_StartRun(       //运行应用程序
		/*         窗口标题*/ "(GN)APP",
		/*     屏幕或窗口宽*/  1920,
		/*     屏幕或窗口高*/  1080,
        /*       颜色深度位*/  0,
        /*         是否全屏*/  false,
		/*     窗口可否缩放*/  true,
		/* 鼠标指针是否可见*/  true,
		/*       取景范围左*/  0,
		/*       取景范围上*/  0,
		/*       取景范围右*/  1920,
		/*       取景范围下*/  1080
	);
GN_EXIT;
/*********************************** 引擎前台开发订制框架程序示例: 以下带'▲'符号的为用户接口 ***********************************/
#include<GN.h>

void CustomInit(){
	//初始化系统模块
	GN_InitSys();
    GN_InitWords();
    int *hwnd;
	GN_InitWindow(&hwnd, "(GN)APP", 1920, 1080, false, true, true);
	GN_InitGraphics(hwnd, 1920, 1080, 0, 0, 0, 1920, 1080, false, 1);
	GN_InitInput();

    //▲初始化应用必须数据 程序开始准备好引擎系统后会调用它一次
    //...
}
void CustomUnInit(){
	//▲释放应用必须数据 程序运行结束前会调用它一次
    //...

    //释放系统模块
    GN_UnInitInput();
	GN_UnInitGraphics();
	GN_UnInitWindow(GNg::sWINDOW::Hwnd);
	GN_UnInitSys();
}
void CustomDisplay2D(unsigned int nTime){ //▲显示2D画面 程序运行每帧都调用它

}
void CustomDisplay3D(unsigned int nTime){ //▲显示3D画面 程序运行每帧都调用它

}
void CustomRun(unsigned int nTime){       //▲运行程序后台的处理 程序运行每帧都调用它

}
/*-------------------------------------------- ★程序入口点★ --------------------------------------------------------------------*/
GN_MAIN;
    GN_InitLoadCallBackFunc( //设置读取回调函数
	    /*--读取资源的函数▲*/ CustomInit,
        /*--释放资源的函数▲*/ CustomUnInit
	);
	GN_InitLoopCallBackFunc( //设置循环回调函数
		/*显示2D画面的函数▲*/ CustomDisplay2D,
		/*显示3D画面的函数▲*/ CustomDisplay3D,
		/*--运行处理的函数▲*/ CustomRun
	);
    GN_GameLoop(true);
GN_EXIT;
#endif
#endif //_GN_INIT_H
