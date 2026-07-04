#ifndef _GN_SYS_H_
#define _GN_SYS_H_

/* 请在编译环境里的预处理定义添加以下其中一个宏来确定代码的应用模式 */
//#define GN_SYS_USING_SDK                       //SDK方式使用
//#define GN_SYS_USING_LIB_EXPORTS               //动态库导出
//#define GN_SYS_USING_SOURCE_CODE               //源代码编辑

#ifdef GN_SYS_USING_SOURCE_CODE          //源代码编辑
#define GN_SYS_API
#define GN_SYS_CALL
#elif defined GN_SYS_USING_LIB_EXPORTS   //动态库导出
 #if defined _WIN32_ || defined _WIN32_WCE
  #define GN_SYS_API      __declspec(dllexport)
  #define GN_SYS_CALL     __declspec(dllimport)
 #else
  #define GN_SYS_API      extern
  #define GN_SYS_CALL     extern
 #endif
#else //------------------------------   //SDK方式使用
 #if defined _WIN32_ || defined _WIN32_WCE
  #define GN_SYS_API      __declspec(dllimport)
  #define GN_SYS_CALL     __declspec(dllexport)
 #else
  #define GN_SYS_API
  #define GN_SYS_CALL
 #endif
#endif
/*-----------------------------------------------------------------------------------------------------------------------------------*/
#if !defined(GN_SYS_USING_SOURCE_CODE) && !defined(GN_SYS_USING_LIB_EXPORTS) && !defined(GN_USING_SDK_INTEGRATION)
#pragma comment(lib,"GN_Sys_win32.lib")
#endif

/* 引擎所依赖标准库的一些内容 如果相关平台没有这些标准库函数则在这里有它的相关处理实现以确保应用程序总能顺利调用到这些功能 ----------*/
#if defined GT_USING_ANDROID || defined GT_USING_ANDROID_JNI
 //static __inline__ void _Exit(int){}
 //static __inline__ float strtold(const char *nptr, char **endptr){ return 0.0f; }
 #include<jni.h>
 #include<unistd.h>
 #include<thread>
 #include<pthread.h>
 #include<tr1/stdarg.h>
 #include<android/sensor.h>
 //#include<sys/atomics.h>
 GN_SYS_API int GT_vsprintf(char*, const char*, va_list ap);
 GN_SYS_API int GT_vfprintf(FILE*, const char*, va_list ap);
 GN_SYS_API int GT_vprintf(const char*, va_list ap);
 GN_SYS_API int GT_sprintf(char*, const char*,...);
 GN_SYS_API int GT_fprintf(FILE*, const char*,...);
 GN_SYS_API int GT_printf(const char*,...);
 #define vsprintf GT_vsprintf
 #define vfprintf GT_vfprintf
 #define  vprintf GT_vprintf
 #define  sprintf GT_sprintf
 #define  fprintf GT_fprintf
 #define   printf GT_printf
#else
 #ifdef _LINUX_
  #include<sys/dir.h>
  #include<sys/io.h>
  //#include<sys/types.h>
  //#include<sys/stat.h>
  //#include<dirent.h>
  #include<iconv.h>
  #include<pthread.h>

 #elif defined _MAC_ || defined GT_USING_IOS
  #ifdef _MAC_
   #include<architecture/i386/io.h>
   #include<sys/dir.h>
   #include<iconv.h>
  #else
   #include "../../ios/Classes/GNiosFrameWork.h"
  #endif
  #include<sys/sysctl.h>
  #include<unistd.h>
  #include<pthread.h>

 #else //Windows or Wince
  #if !defined _INC_IO && !defined _WIN32_WCE
   #include<io.h>
  #endif
  #ifndef _INC_DIRECT
   #include<direct.h>
  #endif
  #ifndef _INC_PROCESS
   #include<process.h>
  #endif
 #endif //endif

 #ifndef _INC_STDARG
  #include<stdarg.h>
 #endif
#endif
#if !defined _INC_STAT && !defined _WIN32_WCE
#include<sys/stat.h>
#endif
#if !defined _WIN32_ && !defined _WIN32_WCE
#include <dirent.h>
#include<fcntl.h>
#include<sys/ioctl.h>
#include<sys/time.h>
#include<errno.h>
#endif

#define GN_SYS_SPT_UTF8  1  //系统是否支持 UTF-8 字符编码转换

// 标准文件操作 -----------------------------------------------------------------------------------------------------------
#ifndef SEEK_SET
#define REDEF_SEEK_SET

#define SEEK_CUR   1
#define SEEK_END   2
#define SEEK_SET   0

struct GN_SYS_API _iobuf
{
	char		 *_ptr;
	int			  _cnt;
	char		 *_base;
	int			  _flag;
	int			  _file;
	int			  _charbuf;
	int			  _bufsiz;
	char		 *_tmpfname;
};
typedef struct _iobuf FILE;

GN_SYS_API FILE * fopen(const char * _Filename, const char * _Mode);
GN_SYS_API int    fclose(FILE * _File);
GN_SYS_API int    fseek(FILE * _File, long _Offset, int _Origin);
GN_SYS_API long   ftell(FILE * _File);
GN_SYS_API int    feof (FILE * _File);
GN_SYS_API size_t fread(void * _DstBuf, size_t _ElementSize, size_t _Count, FILE * _File);
GN_SYS_API size_t fwrite(const void * _Str,  size_t _Size,  size_t _Count, FILE * _File);
GN_SYS_API int    fscanf(FILE * _File, const char * _Format, ...);
GN_SYS_API int    fprintf(FILE * _File,const char * _Format, ...);
GN_SYS_API int    fgetc(FILE * _File);
GN_SYS_API char * fgets(char * _Buf,int _MaxCount, FILE * _File);
#endif

// 标准查找目录操作 -------------------------------------------------------------------------------------------------------
#ifndef _A_SUBDIR
#define REDEF__A_SUBDIR

#define _A_NORMAL  0x00  /* Normal file - No read/write restrictions */
#define _A_RDONLY  0x01  /* Read only file */
#define _A_HIDDEN  0x02  /* Hidden file */
#define _A_SYSTEM  0x04  /* System file */
#define _A_SUBDIR  0x10  /* Subdirectory */
#define _A_ARCH    0x20  /* Archive file */

typedef int64_t   __time64_t;
typedef size_t    _fsize_t;

struct GN_SYS_API _finddata_t
{
   unsigned		   attrib;
   __time64_t	   time_create;   /* -1 for FAT file systems */
   __time64_t	   time_access;   /* -1 for FAT file systems */
   __time64_t	   time_write;
   _fsize_t		   size;
   char            name[260];
    
   _finddata_t():attrib(0){}
};
GN_SYS_API intptr_t _findfirst(const char * _Filename, _finddata_t * _FindData, char *proc_buf=0/*[384]*/); //最后一个参数临时处理缓冲有指定可以处理得更快
GN_SYS_API int      _findnext (intptr_t _FindHandle,   _finddata_t * _FindData, char *proc_buf=0/*[384]*/);
GN_SYS_API int      _findclose(intptr_t _FindHandle, char *proc_buf=0/*[384]*/);

#define GT_FFPROCBUF ,tmp_buf
#else
#define GT_FFPROCBUF
#endif

// 其它一些文件管理 -----------------------------------------------------------------------------------------------------------
#if !defined _WIN32_ && !defined _WIN32_WCE
GN_SYS_API int      mkdir(const char * _Path);
#endif

#ifdef _WIN32_WCE
struct GN_SYS_API stat
{
    time_t st_mtime;
};
GN_SYS_API int stat(const char *filename, stat *_stat_out);
GN_SYS_API int access(const char * _Filename, int _AccessMode);
#endif

#if !defined _WIN32_ && !defined _WIN32_WCE
GN_SYS_API char *itoa(int _Val, char * _DstBuf, int _Radix);
GN_SYS_API char *strupr(char *pstr);
#endif
/****************************************************** Sys模块必须实现的功能 *************************************************************/
struct GT_sPROC_INFO   //进程信息
{
	P<char>			 name; //名
	unsigned int 	 pid;  //进程ID
};
enum GN_eMODULE_TYPE{  //程序模块类型
     GN_eMODULE_EXE,       //可执行程序文件
	 GN_eMODULE_DLL        //动态连接库文件
};
//获得对应系统正确的程序模块扩展名 包含分格符'.'(程序模块类型) !因为各个系统使用的程序模块扩展名是有些不同
GN_SYS_API char *const GN_StrGetModuleExtName(GN_eMODULE_TYPE type);

//获得对应系统的简名 如: "win32", "win64", "wince", "linux", "android", "mac", "ios"
GN_SYS_API char *const GN_StrGetSysName();

//把指定的字符串转为文件系统所用的编码字符串(用来装输出字符串的数组指针, 字符串)
GN_SYS_API void GN_StrToSysCode(char *out, const char *in);

//把字符串表示的文件路径按打开模式来转为文件系统适用的文件路径(用来装输出文件路径的数组[size:384+], 原文件路径, 打开模式, 原文件路径是否已经是全局路径) !!因为某些操作系统下有磁盘访问限制或要求必须使用全局路径访问等问题存在 该函数用于正确的把原文件路径跳转为对应的能访问的文件路径
GN_SYS_API void GN_StrToSysFilePath(char des[], const char *src, const char *open_mode, bool is_g_dir);

//把指定的字符串转为文件系统所用的编码字符串(临时处理缓冲, 字符串)返回转好的字符串指针 当系统无需转换直接返回原字符串指针 否则返回的是临时处理缓冲
GN_SYS_API char* GN_StrToSysCode2(char *buf/*[384]*/, const char *in);

//把字符串表示的文件路径按打开模式来转为文件系统适用的文件路径(临时处理缓冲, 原文件路径, 打开模式, 原文件路径是否已经是全局路径)返回转好的路径指针 当系统无需转换直接返回原路径指针 否则返回的是临时处理缓冲 !!因为某些操作系统下有磁盘访问限制或要求必须使用全局路径访问等问题存在 该函数用于正确的把原文件路径跳转为对应的能访问的文件路径
GN_SYS_API char* GN_StrToSysFilePath2(char *buf/*[384]*/, const char *src, const char *open_mode, bool is_g_dir);

//把宽字符串转为单字节字符串(宽字符串数据地址, 单字节字符串缓冲地址, 单字节字符串缓冲可装多少个字符)返回已转换好的单字符数 这会包含结束符号\0 !参数  char_buf_len 为0可先不作转换而返回装填转换字符所需字符个数
GN_SYS_API int GN_StrWCharToChar(const void *p_wchar, void *p_char, int char_buf_len);

//把单字节字符串转为宽字符串(单字节字符串数据地址, 宽字符串缓冲地址, 宽字符串缓冲可装多少个宽字符)返回已转换好的宽字符数 这会包含结束符号\0   !参数 wchar_buf_len 为0可先不作转换而返回装填转换字符所需宽字符个数
GN_SYS_API int GN_StrCharToWChar(const void *p_char, void *p_wchar, int wchar_buf_len);

//把UTF-8编码的字符串转为GB2312编码的字符串(字符串数据地址, 输出字符串地址, 输出字符串缓冲地址可装多少个字符)返回已转换好的字符数 这会包含结束符号\0  如果您不确定输出字符占用长度 您可以把 char_buf_len 设为0 且参数 gb 为地址的指针 char** 那么系统会帮您自动分配好合适的空间并指派给gb   这时数据用完后记得手动delete [] 掉
GN_SYS_API int GN_StrUTF8ToGB2312(const void *utf8, void *gb, int char_buf_len);

//把GB2312编码的字符串转为UTF-8编码的字符串(字符串数据地址, 输出字符串地址, 输出字符串缓冲地址可装多少个字符)返回已转换好的字符数 这会包含结束符号\0  如果您不确定输出字符占用长度 您可以把 char_buf_len 设为0 且参数utf8为地址的指针 char** 那么系统会帮您自动分配好合适的空间并指派给utf8 这时数据用完后记得手动delete [] 掉
GN_SYS_API int GN_StrGB2312ToUTF8(const void *gb, void *utf8, int char_buf_len);

//将文本内容复制放入粘贴板里(文本内容) 参数值:0 释放删除粘贴板内的内容 一但放入新的内容 旧得将被替换
GN_SYS_API void        GN_StrSetPasteText(const char *text);

//获取粘贴板里的文本内容() 返回0表示粘贴板里没有内容
GN_SYS_API char const* GN_StrGetPasteText();

//执行另外一个子程序(另一个程序文件名)返回0表示成功 失败返回-1 !这可能需要终端用户授予权限才能调用成功
GN_SYS_API int GT_ExeclSub(const char *exec_filename);

//结束一个程序进程(程序进程斋文件名)返回0表示成功 失败返回-1   !这可能需要终端用户授予权限才能调用成功
GN_SYS_API int GT_ExitProc(const char *proc_filename);

//获得系统所有正在运行的进程信息 成功返回各个进程信息 失败返回空指针 !这可能需要终端用户授予权限才能调用成功
GN_SYS_API P<GT_sPROC_INFO> GT_GetSysRunProcAll();

//获得系统CPU核心数
GN_SYS_API unsigned int GT_GetSysCoreCount();

//应用程序当前工作目录更变通知(当前工作目录, 存档目录:常规为0即可 但有些系统只能限制在某个目录里才可以有权限写文件的就需要指定该值) !!这个通常用来如在外部改变了工作目录后必须调用这个函数以便通告Sys模块作出最新的改变 使能正确的处理事务 !!如果你是开发系统接口的要是应用程序运行在特殊目录下你必须调用这个来通知Sys模块
GN_SYS_API void GF_AppCurPathChange(char *app_path, char *doc_path=0);

//获得正在运行应用程序当前工作目录(目录输出) 返回目录字符串长度
GN_SYS_API int GT_GetAppCurPath(char dir_out[]/*[256]*/);

//获得正在运行的应用程序全局路径文件名(全局路径文件名输出) 返回目录字符串长度
GN_SYS_API void GT_GetAppFileName(char name_out[]/*[256]*/);

//列举系统的几个常用目录() 返回目录指针列表
GN_SYS_API P< P<char> > GT_EnumSystemDirectory();

//调用让系统调用适合的程序来打开展现一个指定路径的文件或目录(全局路径文件名)返回是否成功 !!参数可以是一个网络连接 这样即可用系统默认浏览器来打开它
GN_SYS_API bool GN_SysShowFile(const char *filename);

//得到一个用 用户存档目录续上指定路径文件名的路径文件名()返回路径文件名 这个便于用户写入存档定位用的 当返回路径里的文件或目录不能访问 这可能是还未获得终端用户授权 要授权后才能访问 该函数通常在引擎回调 读取用户资源函数如"AppInit"时才开始生效
GN_SYS_API char *const GN_DocDir(const char *path_file);
/*************************************************************** 时间API ***********************************************************************/

//使当前执行线程暂停挂起一定时间(指定时间 单位毫秒)
GN_SYS_API void GN_TimeSleep(unsigned int sleep_time);

//获取系统时间值() 返回系统时间值 单位毫妙 这个是常规精度调用 返回速度应该比较快
GN_SYS_API double GT_TimeGetCountN(void);

typedef double (*GT_P_DOUBLE_VOID_FUNC)(void);
//指向一个获取系统时间值的函数
extern GN_SYS_API GT_P_DOUBLE_VOID_FUNC GN_TimeGetCount; //--如果在该系统下能够提供高精度的系统时间 请把这个指针指向获取高精度时间的方法 否则就把这个指针指向获取常规精度时间的方法 GT_TimeGetCountN

/*************************************************************** 多线程 ***********************************************************************/

struct GN_SYS_API GN_sTHREAD	//线程结构
{
	void			 *z__attrb;    //私有附加属性
    void			 *id;          //线程系统ID
	volatile uint8	  us_state;    //用户状态 你可以用这个状态变量来和主进行沟通 一般常使用时开始把该值设为1

	GN_sTHREAD(); //:id(0), us_state(1){}
   ~GN_sTHREAD();

    // -------------------------- 如果涉及多线程同时读写 us_state 您最好用以下这组函数来读写值 而尽量避免直接对成员的访问 !因为成员在多CPU的寄存器都有副本存在 可能会读到不是最新的数据

    void   UsFlagsAdd(uint8 fg);   //{ us_state |= fg; }        //用户标记加入
	void   UsFlagsDel(uint8 fg);   //{ us_state &=~fg; }        //用户标记删除
	bool   UsFlagsHas(uint8 fg);   //{ return us_state & fg; }  //检测用户状态是否持有指定标记
    void   UsStateAnd(uint8 mk);   //{ us_state &= mk; }        //用户状态与掩码
	void   UsStateSet(uint8 st);   //{ us_state = st; }         //用户状态设置
	uint8  UsStateGet();           //{ return us_state; }       //返回当前用户状态
};
extern GN_SYS_API bool GN_ThreadSupport; //是否支持多线程处理

//开始执行线程( 执行函数(告诉你函数所分配到的对应线程, 传回给你的参数, 传回给你的参数2), 传给执行函数用的参数, 用的参数2) 返回新创建的线程 这个和指定执行函数的第一个参数是一样的 用户可以通过这个来使线程内和线程外沟通  你可以用 UsFlag... , UsState... 等函数来设置访问成员 us_state 来以外界协商处理  若返回为空表示有错 !!应该注意的是调用一般先返回 接着过一会儿线程函数才开始执行 因为新线程创建需要点时间 而且调用开销也挺大 尽量不要在每帧循环里调用
GN_SYS_API P<GN_sTHREAD> GN_ThreadStart( void (*proc)(P<GN_sTHREAD> this_thread, void *arg, void *arg2), void *_arg=0, void *_arg2=0);

//堵塞等待线程执行完成到返回为止(线程) 返回是否成功等到回来了 !!线程处理完后会自动清理开辟线程的相关资源  无须手动去关闭
GN_SYS_API bool          GN_ThreadEndWait(const P<GN_sTHREAD> &pt);

//强制终止线程执行() 在那个线程执行区内部调用就终止该线程执行  !这个非必要的情况下尽量少用
GN_SYS_API void          GT_ThreadExit();

//线程通行证创建
GN_SYS_API void *        GN_ThreadCondCreate();
//线程通行证删除(通行证)
GN_SYS_API void          GN_ThreadCondRemove(void *cond);
//线程堵塞等待通行证变为可通行为止(通行证)
GN_SYS_API void          GN_ThreadCondWait(volatile void *cond);
//线程通行控制(通行证, 放行线程数 值0关闭所有相关线程通行)
GN_SYS_API void          GN_ThreadCondCtrl(void *cond, uint8 pass_count);

/************************************************************* 原子与同步操作 *********************************************************************/
#if defined _WIN32_ || defined _WIN32_WCE
#ifdef __LP64__
GN_SYS_API void	GN_MemBarrier();				//多线程内存栅栏
#else
#define GN_MemBarrier MemoryBarrier             //多线程内存栅栏
#endif
#else
#if (defined _LINUX_ && defined GT_USING_GLes) || defined(_MAC_)
GN_SYS_API void GN_MemBarrier();
#else
inline void smp_mb(){ __sync_synchronize(); }  // 用户态内存栅栏
#define GN_MemBarrier smp_mb					//多线程内存栅栏
#endif
inline bool InterlockedCompareExchange(GT_sATOMIC* PV , int WV , int CV ){ return !(PV->compare_exchange_strong(CV,WV)); }
#define InterlockedExchange( PV , WV ) (PV)->store(WV)
#endif
typedef  GT_sATOMIC      GN_sMUTEX;			//互斥锁  !这个同步线程虽然方便 但是堵塞等待浪费性能 最好不要滥用

GN_SYS_API void GN_MutexLock(  GN_sMUTEX *mut);	//访问锁定(互斥锁)
GN_SYS_API void GN_MutexUnLock(GN_sMUTEX *mut); //访问解锁(互斥锁)

/***************************************************************** 动态库 ***********************************************************************/

//装载打开一个程序模块( 模块名 )返回模块引用号 失败将返回0
GN_SYS_API int *GT_ModuleOpen(const char *module_name);

//关闭及清除一个程序模块( 模块 )
GN_SYS_API void GT_ModuleClose(int *module);

//从模块里获取一个函数( 模块, 函数名 ) 失败返回0 成功返回该函数指针
GN_SYS_API GT_PFUNC_VOID GT_ModuleGetFunction(int *module, const char *func_name);

/*************************************************************** 特殊外设IO *********************************************************************/
namespace GNio {

    extern GN_SYS_API float  InFv[];     //一搬记录外部传感器输入值 或 一些外部设备传入来的浮点类型数据 使用示范: InFv[GI_WX] 引用重力X轴分量 关于GI_WX具体在 GN_Input.h 传感器索引表里有详细定义 在不同的系统对应不同的设备下该数组拥有的元素个数都不一样的 但最少应有3个元素 元素的个数为传感器索引表里最大索引值加1

	extern GN_SYS_API bool   InPort[];   //记录外部IO各个输入状态 这个通常是一一对应其它外部输入硬件设备的 在不同的系统对应不同的外部输入设备 该数组拥有的元素个数都不一样的 但最少应有一个元素

	//命令输出（端口, 状态, 转变数, 转变时间）
    GN_SYS_API void OutPut(int IO_port, int status, int turn_num=0, int turn_time=0);
	//在ios和andriod下可用这个来输出震机
	//震机输出 按以下这样调用(震动IO号, 关/开, 重复多少次, 震动毫秒数)
	//			 GNio::OutPut(0, 1, 1, 1000); !!在IOS系统里只有iphone 能发出震动 而者不能快速连续轻震 由于它震动的最小单位也要 300ms

	//获得所有外部传感器输入值个数
	GN_SYS_API uint8 GetInFvNum();
	//获得外部输入IO设备数
	GN_SYS_API uint8 GetInPortNum();
	//获得外部输出IO设备数
	GN_SYS_API uint8 GetOutPutNum();

	//以下的两个函数所操作到的感应器组合位具体描述在 GN_Input.h 感应器标记位里有详细定义
    //开启有位标记的感应器和禁用没标记位的感应器( 感应器组合位 ) 由于这些信号数据比较多 接收需要些性能开销 特别是如开启了输入纪录时 内存消托也大 在默认情况下不开启感应器输入
    GN_SYS_API void EnableSensors(unsigned int use_flags = 1 /*GI_HAS_WEIGHT*/);
	//获得设备所拥有的传感器信息  使用示范: if(GetSensorCaps() & GI_HAS_WEIGHT){ 有重力感应器时的处理 }
    GN_SYS_API unsigned int GetSensorCaps();

	//命令出票(今次出多少张)
	GN_SYS_API void TicketsNumOut(int count);
	//获得未出的彩票数
    GN_SYS_API int  GetOweTickets();
	//判断内部是否没彩票了
    GN_SYS_API bool IsNoTickets();
	//得到版子上记下的投币存储数(投币端口, 币数输出)
    GN_SYS_API void GetInCount(int IO_port, int *nCounter);

}; //end GNio namespace
namespace GN {
	//开始检查密匙是否通过( 游戏ID )
    GN_SYS_API bool StartRockeyPass(const char *gameID);
	//再检查密匙是否通过
    GN_SYS_API bool RockeyPass();
};
/*******************************************************************************************************************************************************************************************************************/

GN_SYS_API void GN_InitSys();                   //初始化系统相关的内容 !如果你是开发接口的 需要一并设置引擎系统初始化状态变量 GT_SystemInited & (1<<5) 以便按该值位标记来判断使得重复调用该函数时而不会再次被初始化
GN_SYS_API void GN_UnInitSys();                 //释放系统相关的内容   !如果你是开发接口的 需要一并移除引擎系统初始化状态变量 GT_SystemInited & (1<<5) 的位标记
GN_SYS_API void GN_SysProc(unsigned int nTime); //挂起处理系统相关的一些杂务(帧间隔时间) 该函数用于在帧循环里不断调用的

GN_SYS_API char *const GN_GetInerfaceInfoSys(); //获得系统模块信息 如: "Windows 7" , "Mac OS X 10.5" , "Linux" , "WinCE 6" , "Android 2.3" , "IOS 4" , ...等
GN_SYS_API void GN_GetDeviceInfoCpu(char *info/*[64]*/); //获得硬件CPU厂商型号主频信息(信息输出缓冲 需先分配好空间)
GN_SYS_API void GN_GetDeviceInfoRam(char *info/*[64]*/); //获得硬件内存大小信息(信息输出缓冲 需先分配好空间)

#endif //endif _GN_SYS_H_
