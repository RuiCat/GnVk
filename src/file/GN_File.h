#ifndef _GN_FILE_H_
#define _GN_FILE_H_

/****************************************************** 文件操作 ***************************************************************/
namespace GNf {
	
enum ePATH_TYPE {				//路径目标类型
     ePATH_UNKNOWN,				   //不确定
     ePATH_FILE,                   //文件
     ePATH_DIR,                    //目录
};
enum eFILE_TYPE{				//文件类型常量
     eFILE_SAFE = 0,			   //表示使用安全校验方式操作标准IO文件
	 eFILE_STD  = 1,			   //表示使用标准IO文件
	 eFILE_MEM  = 3				   //大于等于该值的就表示使用内存镜像文件 可以是任何大小 !!关于压缩包文件使用也会自动转为eFILE_MEM方式使用
};
enum eSEEK_TYPE{				//文件偏移常量
     eSEEK_START,				   //靠文件开始
	 eSEEK_CUR,				       //靠文件当前
	 eSEEK_END  				   //靠文件结尾
};
struct GN_API sFILE             //★★文件结构体
{
	static GN_sMUTEX	  Mutex;			   //文件方位互斥锁 方便应用程序管理以下几个静态属性状态的
	static void			 *DefaultH;            //保留日后扩展用 默认为 0 
	static int			  DefaultType;         //默认使用类型  默认为 eFILE_STD
//	static bool			  EnableDedicatedOnly; //是否限制只使用专用的文件格式 一概不接受其它格式 这个常用来保护程序资源如图片等... 不被修改用 默认值为 false
	static bool			  EnableGlobalPath;    //视为全局路径来读取文件 默认值为 false
	
	char				 *handle_buf;          //文件句柄或数据内存指针
	int					  buf_size;            //文件大小
	int					  offset;              //正值时记录内存或包文件内存文件指针当前偏移位置 负值时标记着远端文件
	int					  special_mark;        //特殊标记用 ...
	int					  type;                //文件使用的类型

    sFILE();
   ~sFILE();
};
extern GN_API sFILE File;

struct GN_API sFILE_INFO :public _finddata_t
{
    char global_filename[384];  //包含全局路径的文件名
};
/*--------------------------------- !以下用其它方式打开文件时的注意事项! -------------------------------------------*\
|                                                                                                                    |
|		  eFILE_MEM:  对于内存镜像文件不作添加处理 写操作文件也只限制在buf_size大小内                                |
|                                                                                                                    |
|		  eFILE_SAFE: 对于安全模式打开文件 用“r+”或“a+”等可局部增加修改部分数据的写操作有时可能会有数据不同步    |
|                     这应该在写完后再重复打开写多一次可避免这个发生                                                 |
|                                                                                                                    |
\*------------------------------------------------------------------------------------------------------------------*/

/*以下这5个函数的最尾两个参数 usTYPE_size 和 ph 是只能单独指定一个 另一个应该保留原默认值 如果同时改变了两个参数 那么ph会较为优先*/

//将文件读取一小节到内存(          文件名或内存地址, 存放读出数据的内存, 要读取的大小, 文件读取偏移位置, 使用类型或内存大小, 保留扩展参数) 返回读取了个数 失败返回-1  !!这个对于在包文件里读取效率不高 !!调用这个最好先确认好指定文件存在可访问并知道大小来指定读取量 以免容易出错
GN_API int FileSegmentLoadToMemory(char *filename,   char *buf_out,  int buf_size, int offset, int us_type_size=sFILE::DefaultType, void *ph=sFILE::DefaultH);
//将文件完整读到内存(              文件名或内存地址, 文件大小输出, 使用类型或内存大小, 保留扩展参数) 返回新内存数据地址 用完记得要delete []; 返回0表示文件不存在或被其它因素锁住或权限问题不能打开
GN_API char* FileFullLoadToMemory    (char *filename, int *size_out, int us_type_size=sFILE::DefaultType, void  *ph=sFILE::DefaultH);
//将文本文件完整读到内存(          文件名或内存地址, 文本长度输出, 使用类型或内存大小, 保留扩展参数) 返回新内存数据地址 用完记得要delete []; 返回0表示文件不存在或被其它因素锁住或权限问题不能打开
GN_API char* FileFullLoadToMemoryText(char *filename, int *len_out,  int us_type_size=sFILE::DefaultType, void  *ph=sFILE::DefaultH);

//创建打开文件待准备用来写操作(文件名,      打开方式 例"w" ,     保留扩展参数) 成功返回文件句柄  失败返回0表示可能被其它因素锁住或权限问题不能创建打开 !!如果指定文件所在目录不存在将会自动创建它以确保尽量能成功打开文件
GN_API sFILE *OpenCreate(char *filename, const char *mode, void  *ph=sFILE::DefaultH);
//打开文件待以下的函数来操作（文件名或内存地址, 打开方式 例"r+",  使用类型或内存大小, 保留扩展参数) 返回文件句柄 返回0表示文件不存在或被其它因素锁住或权限问题不能打开
GN_API sFILE *Open(      char *filename, const char *mode, int us_type_size=sFILE::DefaultType, void  *ph=sFILE::DefaultH);

//关闭文件(文件句柄) 成功返回0 失败返回-1
GN_API int   Close(sFILE *handle);
//返回文件打开时的长度(文件句柄) 失败返回-1
GN_API int   GetSize(sFILE *handle);
//设置文件指针偏移位置(文件句柄, 偏移量, 靠位）成功返回0 失败返回-1
GN_API int   Seek(sFILE *handle, int offset, int origin);
//返回指定文件的当前指针位置(文件句柄) 失败返回-1 
GN_API int   Tell(sFILE *handle);
//检查文件是否到结尾(文件句柄) 是返回1 否则返回0 返回-1表示存在错误
GN_API int   Eof(sFILE *handle);
//读文件(存放数据的内存, 单个数据大小, 数据个数, 文件句柄) 返回读取了个数 失败返回-1
GN_API int   Read (char       *buf,int unit_size,int count,sFILE *handle);
//写文件(待写数据的内存, 单个数据大小, 数据个数, 文件句柄) 返回写入了个数 失败返回-1  该会受文件权限影响导致操作失败
GN_API int   Write(const char *buf,int unit_size,int count,sFILE *handle);
//按格式读取文本文件(文件句柄, 格式字符号描述, 参数队列)返回赋值成功的变量数 失败返回-1
GN_API int   Scanf( sFILE *handle, const char *format, ...);
//按格式写入文本文件(文件句柄, 格式字符号描述, 参数队列)返回已打印字符的个数 失败返回-1 该会受文件权限影响导致操作失败
GN_API int   Printf(sFILE *handle, char *format, ...);
//从文件现在流的位置读出一个字符(文件句柄) 返回当前位置读出的字符
GN_API int   Getc(sFILE *handle);
//从文件现在流的位置读出一行字符串(接收字符串用的缓冲, 缓冲大小字节, 文件句柄) 返回读到的字符串即第一个参数的指针 返回0表示读取失败
GN_API char *Gets(char *buf, int buf_len, sFILE *handle);

/*--------------------------------------------- 文件管理 !!这个不能操作包文件 -------------------------------------------------------------------------------*/

//检查目录或文件是否存在(目录名, 查找类型设0即可, 保留扩展参数) 是返回0 否返回-1
GN_API int   Access  (const char *path_filename,  int amode,                  void  *ph=sFILE::DefaultH);
//更改目录名或文件名(全局原文件或目录名, 全局新文件或目录名, 保留扩展参数) 成功返回0 失败返回-1  该会受文件权限影响导致操作失败 该函数也可以用于剪切文件和文件夹只要换另外一个路径就可以了 !!注意 名不能更改到不存在的地方去 也不能改为与现有名相同的名
GN_API int   Rename  (const char *src_g_pathname, const char *new_g_pathname, void  *ph=sFILE::DefaultH);
//创建指定名称新目录  (目录名, 保留扩展参数) 成功返回0 失败返回-1  该会受文件权限影响导致操作失败
GN_API int   Mkdir   (const char *path,                                       void  *ph=sFILE::DefaultH);
//删除指定名称的空目录(目录名, 保留扩展参数) 成功返回0 失败返回-1  该会受文件权限影响导致操作失败
GN_API int   Rmdir   (const char *path,                                       void  *ph=sFILE::DefaultH);
//删除指定名称文件(文件名, 保留扩展参数) 成功返回0 失败返回-1  该会受文件权限影响导致操作失败
GN_API int   Del     (const char *path_filename,                              void  *ph=sFILE::DefaultH);

//立即跳转到执行另外一个程序(另一个程序文件名, 保留扩展参数) 成功返回0 失败返回-1  该会受文件权限影响导致操作失败
GN_API int   Execl   (const char *exec_filename,                              void  *ph=sFILE::DefaultH);
//执行另一个子程序(另一个程序文件名, 保留扩展参数)成功返回0 失败返回-1  该会受文件权限影响导致操作失败
GN_API int   ExeclSub(const char *exec_filename,                              void  *ph=sFILE::DefaultH);
//结束一个程序进程(程序进程斋文件名, 保留扩展参数)成功返回0 失败返回-1  该会受文件权限影响导致操作失败
GN_API int   ExitProc(const char *proc_filename,                              void  *ph=sFILE::DefaultH);

//获得系统当前运行有指定名的进程数
GN_API int  GetRunProcNum(const char *proc_name);

//获得正在运行应用程序当前工作目录(目录输出, 保留扩展参数) 返回是否成功
GN_API bool GetAppCurPath(char *dir_out/*[256]*/, void  *ph=sFILE::DefaultH);

//搜索指定目录里的所有文件信息和目录信息( 搜索目录+搜索模式字符串 如E:/temp/*.*, 是否组建全局路经名, 接收文件信息的链表 , 接收目录信息的链表, 保留扩展参数) 成功返回0 失败返回-1
GN_API int  FindFileDirInfoData(const char *path, bool g_dir, GN::sLIST< P<sFILE_INFO> > *files_out, GN::sLIST< P<sFILE_INFO> > *dirs_out, void  *ph=sFILE::DefaultH);
//搜索指定目录里有多少个与指定文件名前段匹配的文件数(搜索目录+文件名前段, 保留扩展参数)返回匹配的文件数 返回-1表示搜索失败
GN_API int  FindFile_Num(const char *filename_, void  *ph=sFILE::DefaultH);
//列举系统的几个常用目录( 保留扩展参数 ) 返回目录指针列表
GN_API P< P<char> > EnumSystemDirectory(void *ph=sFILE::DefaultH);

/*----------------------------------------------------------------------------------------------------------------------------------------------------------------*/
//GN_API void PACK_BEGIN();
//GN_API void PACK_END();

//当打开一个文件成功时的处理函数指针(成功打开文件的大小, 该文件路径, 文件的打开模式, 文件使用类型或内存大小, 文件打开的扩展参数)
extern GN_API void (*OnFileOpenOKCallBack)(int filesize, char *filename, const char *mode, int us_type_size, void *ph);

}; //end GNf namespace

/*********************************************************** 临时更改文件默认读取状态 *******************************************************************/

//临时记忆并更改目录状态( 是否使用全局路径来读取文件 )
#define GNf_DirStatePush( global_dir ) \
    bool g_dir = GNf::sFILE::EnableGlobalPath;\
	GNf::sFILE::EnableGlobalPath = global_dir

//复原之前调用 GNf_DirStatePush 更改之前的目录状态  这两个必须在同一区划里配对调用
#define GNf_DirStatePop() \
    GNf::sFILE::EnableGlobalPath = g_dir

//临时记忆并更改文件读取状态(文件所在主机)
#define GNf_LoadFileStatePush1( _host_ ) \
    void *src_host = GNf::sFILE::DefaultH;\
	GNf::sFILE::DefaultH = _host_

//复原之前调用 GNf_LoadFileStatePush1 更改之前的文件读取状态 这两个必须在同一区划里配对调用
#define GNf_LoadFileStatePop1() \
    GNf::sFILE::DefaultH = src_host

//临时记忆并更改文件读取状态(文件所在主机, 使用文件类型)
#define GNf_LoadFileStatePush2( _host_ , _us_type_ ) \
    void *src_host        = GNf::sFILE::DefaultH;\
    int   src_us_type     = GNf::sFILE::DefaultType;\
	GNf::sFILE::DefaultH    = _host_ ;\
	GNf::sFILE::DefaultType = _us_type_

//复原之前调用 GNf_LoadFileStatePush2 更改之前的文件读取状态 这两个必须在同一区划里配对调用
#define GNf_LoadFileStatePop2() \
    GNf::sFILE::DefaultH    = src_host;\
	GNf::sFILE::DefaultType = src_us_type

//临时记忆并更改文件读取状态(文件所在主机, 使用文件类型, 是否使保密形式读取)
#define GNf_LoadFileStatePush3( _host_ , _us_type_ , _dedicated_ ) \
    void *src_host         = GNf::sFILE::DefaultH;\
    int   src_us_type      = GNf::sFILE::DefaultType;\
	bool  src_dedicated    = GNf::sFILE::EnableDedicatedOnly;\
	GNf::sFILE::DefaultH            = _host_ ;\
	GNf::sFILE::DefaultType         = _us_type_ ;\
	GNf::sFILE::EnableDedicatedOnly = _dedicated_
	
//复原之前调用 GNf_LoadFileStatePush3 更改之前的文件读取状态 这两个必须在同一区划里配对调用
#define GNf_LoadFileStatePop3() \
    GNf::sFILE::DefaultH            = src_host;\
	GNf::sFILE::DefaultType         = src_us_type;\
	GNf::sFILE::EnableDedicatedOnly = src_dedicated

/********************************************************** 对应文件的字符串处理 *****************************************************************/

//把打开文件方式字符串转为整型标记( 打开文件方式字符串 )返回整型标记 参数是对应上面的GNf::Open函数里用到的打开文件方式
GN_API int GNf_OpenModeCharToInt(const char *open_mode);

//把文件指针移到指定字符首次出现处之后(文件句柄, 指定字符) 返回是否找到匹配字符
GN_API bool GN_StrFileToKey(GNf::sFILE *text,char key);

//比较字符串和文本文件流当前字段(字符串1, 文本文件流句柄) 返回是否相同  比较完后文件流的指针会移到下一字串处或结束处
GN_API bool GN_StrcmpFileSN(const char *str1, GNf::sFILE *text);

//比较不对齐字符串和文本文件流当前字段(字符串1,文本文件流句柄) 返回是否相同  这容许字符串里的首尾有空格符不对齐的
GN_API bool GN_StrcmpFileEX(const char *str1, GNf::sFILE *text);

//从文件流中复制指定段落到新缓存(文本文件流句柄, 开始标识符, 结束标识符, 返回新字符段长度) 返回新分配的字符串指针 返回0表示失败
GN_API char* GN_StrCopyFileSegmentToData(GNf::sFILE *load, char start_key, char end_key, int *size_out);

//把文本文件指针跳过指定段落后(文本文件流句柄, 开始标识符 ,结束标识符) 返回0表示失败
GN_API bool GN_StrFileSeekToSegmentBack(GNf::sFILE *text, char start_key, char end_key);

//从ASCII格式文件中删除多段含指定内容的段落(路径文件名, 段分隔符, 删除段中包含的字符串, 最多删除多少段, 是否只扫描每段的首部作比较) 返回共删除了多少段 返回0表示没找到符合条件的段 返回-1表示内容已经全清了 返回-2表示指定文件不存在与错 !!注意这个文件不能在包文件里
GN_API int GN_StrRemoveFlieSegment(char *filename, char split_key, const char *include_str, int sn = 1, bool check_first = true);

//判断两个文件路径是否为同一个文件或目录(路径1, 路径2) 返回是否相同 !!用路径比较 '/' '\\' 被视为相等 这个支持全局路径和非全局路径比较
GN_API bool GN_StrPathIsEqual(const char *str1, const char *str2);

//判断并获得指定文件路径在当前运行应用程序工作目录里的那段相对路径(文件路径)如果在就返回它的相对路径 不在就返回0  如果文件路径本是相对路径就直接返回它
GN_API char* GN_StrGetInAppDirPath(const char *str);

//读取按特定格式书写的文件里的语言句子(文件名)
GN_API P< P<char> > GN_StrLoadWordFromFile(const char *filename);

/* ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- */

GN_API bool GN_InitWords(); //读取引擎系统内部词条() 返回是否读取成功

// 1.GN_Base.h GN_Words 信息判断输出  2.本引擎服务器模式时错误输出和更新的不同处理

extern GN_API char				   *GN_Words_No_Yes[2];  //[0]"否" , [1]"是"
extern GN_API P< P<char> >          GN_Words;			 //信息单词 用于支持多语言


extern GN_API GN::sLIST< P<char> >  GN_ErrorList;  //记录内部一系列错误信息     - 在运行中途比较常发 但不会导致运行中断的小错误
extern GN_API P<char>               GN_ErrorLast;  //记录最近一次的内部错误信息 - 在运行中途比较常发 但不会导致运行中断的小错误 如果是问题大的错误一般都是用 GN_Log 记录到文件里去

GN_API void GN_ErrorInfoClear();				   //清除记录到的错误信息


//向运行日志输出信息 即记录到 RunLog.txt 里 (输出格式, 参数队列 ) !用法跟<printf()>差不多
GN_API void GN_Log(const char *fmt, ...);

extern GN_API bool  GN_RunErrorOutputOn; // GN_Log 时是否开启错误信息输出 默认为:true
extern GN_API char *GN_RunErrorFileName; //运行日志记录文件名 默认为:"RunLog.txt" 你可以把该指针指向另一个名 来更变日志记录文件名 如果作为服务器开发时 一般都把该名更变一下加上一个日期时间 以保留每次运行结果待管理查询 而且又可以防止引擎检测到相同的文件而弹出错误修复窗口卡住服务程序运行


//接入图像文件支持插件到引擎(插件所在程序模块, 插件创建方法)
GN_API void GN_PluginFileImageAdd(int *module, GT_PFUNC_VOID create_func);
//接入音频文件支持插件到引擎(插件所在程序模块, 插件创建方法)
GN_API void GN_PluginFileAudioAdd(int *module, GT_PFUNC_VOID create_func);
//接入UI组件支持插件到引擎(插件所在程序模块, 插件创建方法)
GN_API void GN_PluginUIObjectAdd(int *module, GT_PFUNC_VOID create_func);
//接入2D组件支持插件到引擎(插件所在程序模块, 插件创建方法)
GN_API void GN_Plugin2DObjectAdd(int *module, GT_PFUNC_VOID create_func);

//加载插件子分类目录里的所有插件(子分类名, 接入插件到引擎的方法(插件所在程序模块, 插件创建方法))返回成功加载插件数
GN_API int	GN_PluginsLoad_(const char *class_name, void (*add_plugin_func)(int *module, GT_PFUNC_VOID create_func));
//加载插件子分类目录里的所有插件(子分类名)返回成功加载插件数
#define		GN_PluginsLoad( class_name ) GN_PluginsLoad_(#class_name,GN_Plugin##class_name##Add)
//加载所有插件()返回成功加载插件数
GN_API int	GN_PluginsLoadAll();

//移除所有图像文件支持插件()返回本次移除插件数
GN_API int	GN_PluginsFileImageRemove();
//移除所有音频文件支持插件()返回本次移除插件数
GN_API int	GN_PluginsFileAudioRemove();
//移除所有UI组件支持插件()返回本次移除插件数
GN_API int	GN_PluginsUIObjectRemove();
//移除所有2D组件支持插件()返回本次移除插件数
GN_API int	GN_Plugins2DObjectRemove();
//移除所有插件()返回本次移除插件数
GN_API int	GN_PluginsRemoveAll();

//获得所有指定类型对象支持插件(类型 0:UI组件 1:2D对象, 2:3D对象)返回这组插件 返回0表示没有支持的
GN_API GN::sLIST<int*> *const GN_PluginsClassifyGet(uint8 obj_class);

//移除插件列表里的所有插件(插件列表)返回本次移除插件数
int GT_RemoveListPlugins(GN::sLIST<int*> &plugins);
#endif //endif _GN_FILE_H_