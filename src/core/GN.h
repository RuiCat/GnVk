#pragma once
#ifndef _GN_H_
#define _GN_H_

#define GN_INTERFACE_VERSION "2.2.0" //★★★☆ 接口版本

#define GN_VERSION "2026.07.04" //★★★☆ 引擎主版本（版本号单一来源：只在此处修改）

/*------------------------------------------------ !! GN_mini引擎使用声明 !! ---------------------------------------------------------\
|                                                                                                                                     |
|	本引擎精简版源码是个采用MIT协议开源免费,超轻量,依赖少,配置要求极低,优化到位,可定制式样,动效丰富且跨平台的实时GUI和2D引擎运行库	  |
|	下面的一些模块定义列举了当前版本所依赖到的一些开源库, 如果您不需要特定功能您可以剪裁它以使它更轻量                                |
|                                                                                                                                     |
|	本引擎主要依赖到的其它库有: OpenGL, GLUT, SDL, libpng, libzip, OpengAL(如果您使用声音播放的话), FFmpeg.(播放视频用)               |
|	如在使用过程中遇到问题, 或有何好建议欢迎加技术QQ群 152670375 交流, 如想了解更多使用例子或更详细的文档可以登陆:                    |
|	www.gncreate.com 或 www.img-power.com 查阅!                                                                                       |
|	                                                                                                                                  |
\------------------------------------------------------------------------------------------------------------------------------------*/

/* 请在编译环境里的预处理定义添加以下其中一个宏来确定代码的应用模式 */
//#define GN_USING_SDK                       //SDK方式使用
//#define GN_USING_SDK_INTEGRATION           //集成一体化SDK方式使用
//#define GN_USING_LIB_EXPORTS               //动态库导出
//#define GN_USING_SOURCE_CODE               //源代码编辑

/* 处于DEBUG模式     这个一般在项目工程那里设比较方便 */
//#define GN_DEBUG

// #内嵌功能模块编译配置信息 ----------------------------------------------------------------------------------------------------------

/* 内嵌使用2D模块: - 前台应用必需 */
#define GN_USING_2D 2

/* 内嵌使用3D模块 */
#define GN_USING_3D 4

/* 内嵌使用音频模块 */
#define GN_USING_AUDIO 8

/* 内嵌使用网络模块: - 后台服务必需 */
#define GN_USING_NET 16

/* 内嵌使用zip解压缩模块 */
#define GN_USING_UNZIP 32

/* 内嵌使用更新模块 */
//#define GN_USING_UPDATE 64

/* 内嵌实用功能模块 */
#define GN_USING_UTILITY_LIB 1

/* 内嵌额外附加功能模块 */
#define GN_USING_ADDITIONAL_LIB 256

/* 内嵌支持更多外部3D文件格式 这个选项目前仅支持Windows系统 */
//#define GN_USING_EXT_3DFILE_FORMAT 512

/* 内嵌支持ASE和RTX 3D文件格式 老版本3DMAX的一种导出格式 */
//#define GN_USING_ASE_3DFILE_FORMAT 1024

/* 内嵌专属格式g3d文件保存功能模块 */
//#define GN_USING_G3D_FORMAT_SAVE 2048

//#功能使用缓冲的大小配置 ---------------------------------------------------------------------------------------------------------

/* 使用 文件 缓冲大小  2MB */
#define GN_USING_FILE_BUF 2097152

/* 最大支持线程并行数核数 */
#define GN_USING_PARALLEL_MAX 32

/* 3D模型网格三角面使用长索引 以支持更多三角型 */
//#define GN_USING_TRI_LONG_INDEX 1024

//---------------------------------------------------------------------------------------------------------------------------------
#ifndef GN_USING_UTILITY_LIB
#undef GN_VERSION
#define GN_VERSION "0.0"
#endif
#if defined GN_USING_UPDATE && !defined GN_USING_UNZIP
#define GN_USING_UNZIP 32 //升级模块需用到解压模块
#endif
#define GN_RUNTSTART(nc) 0
#define GN_RUNTEND(time_proc) 0
#define GN_RUNTENDMF() 0
#define GN_RunReportError() 0
#define GN_RunTextOut(text, count, x, y, w, h, color, blendmode) 0
#define GN_RunDrawFrame(x, y, w, h, color) 0
#define GN_RunValueCheck(value, name, ID_order) 0
#define GN_RunValueCheck2(value, ID_order) 0
#define GN_RunProgressSizeGet(out_w, out_h) 0
#define GN_RunProgressDraw(progress, style, re_draw, cx, cy, icon_color, text_color, frame_color, bar_color, blendmode) 0
#define GN_RunKCOF(vname, KCODE, ID_order) 0
#define GN_RunKCV (type, vname, vdef, vadd, KADD, KSUB, ID_order)0
#define GN_RunKCV1(type, vname, vdef, vadd, KADD, KSUB, ID_order) 0

#include "_SysDef/GN_SysDef.h" //编译目标系统平台定义, 里面包含了所编译目标系统所用到的一些头文件和定义一些在不同系统下所用到或用来标识系统的宏

#ifndef _WIN32_
#undef GN_USING_EXT_3DFILE_FORMAT
#endif
typedef void (*GT_VOID_PFUNC_UINT)(unsigned int);
typedef void (*GT_PFUNC_VOID)(void);
typedef void (*GT_PFUNC_PVOID)(void *);
typedef void (*GT_PFUNC_PVOID_PVOID)(void *, void *);
typedef void (*GT_PFUNC_PVOID_UINT3)(void *, unsigned int, unsigned int, unsigned int);

typedef bool (*GT_BOOL_PFUNC_PVOID)(void *);

typedef signed char int8;
typedef signed long long int64;
typedef unsigned char uint8;
typedef unsigned long long uint64;

extern GN_API unsigned int GT_SystemInited; //标记引擎系统各部分是否已经初始化了

//引擎头文件模块功能和依赖关系结构 每个功能依赖到的头文件都会放在该文件上面更靠左的地方
//_另外包含在目录内的头文件均为接口转换模块 它的实现文件对应不同系统平台有不同的处理 但总确保对外的接口统一 使得引擎能够运行在不同平台环境下

// 基础通用功能所需包含
#include "GN_Base.h" //一些比较基本常用的功能 比如随机数 快速线性插值 数值关系比较等
#include "GN_Array.h" //各种容器数据结构 树型数据结构 安全指针 内存池和数据整理比较等功能
#include "GN_String.h" //字符串与文件路径处理等功能
#include "_Sys/GN_Sys.h" //系统文件IO 系统剪贴版 系统工作目录 字符转码 多线程 时间 小外设部件IO 等相关转换操作
#include "GN_Parallel.h" //多线程运行任务 并行计算等功能
#include "GN_File.h" //文件操作与管理 语言词条读取 运行日志输出到文件 插件加载
#include "GN_Format.h" //常用格式文件加载与保存 颜色格式转换 压缩解压文件等处理

// 声音功能所需包含
#ifdef GN_USING_AUDIO
#include "_Sound/GN_Sound.h" //播放采集音频数据的底层转换操作
#ifdef GN_USING_UTILITY_LIB
#include "GN_Audio.h" //音乐控制 播放等功能 是引擎比较实用的一个模块
#endif
#endif

// 2D功能所需包含
#ifdef GN_USING_2D
#include "GN_Color.h" //颜色运算 颜色数据类型转换等
#include "GN_Layout.h" //平面的位置布局 按格式排版 计算绘制坐标等功能
#include "GN_2Dmath.h" //2D图形处理用到的几何数学等功能
#ifdef GN_USING_3D
#include "GN_3Dmath.h" //3D图形处理用到的几何数学等功能
#include "GN_3Dmodel.h" //3D模型加载与绘制等功能
#include "GN_3Dlight.h" //3D光源系统
#include "GN_3Dmaterial.h" //3D材质系统
#include "GN_Shader.h" //SPIR-V 加载与着色器模块缓存
#include "GN_Pipeline.h" //通用图形/计算管线描述与缓存
#include "GN_Material.h" //GPU 材质实例（描述/校验/去重键；Vulkan 创建留 P8）
#include "GN_Spatial.h" //3D空间划分 AABB/BVH/动态AABB树/视锥/射线
#include "GN_Physics.h" //3D刚体物理 球/盒/胶囊/关节/三角网
#include "GN_3Dcloth.h" //布料 XPBD（CPU 模拟；GPU 上传留 P8/P9）
#include "GN_3Dparticle.h" //3D 粒子池/发射器（CPU 模拟；实例上传留 P8）
#include "GN_3Danim.h" //3D 动画 clip 采样/混合/状态机（CPU）
#include "GN_3Dskeleton.h" //3D 骨骼 世界矩阵/调色板（CPU；蒙皮留 P8）
#endif
#include "GN_Linear.h" //线性插值与曲线插值等功能
#include "GN_2DArea.h" //2D空间区域构造与2D碰撞检测等功能
#include "GN_2Dtype.h" //2D图形处理用到的一些其它数据结构 功能标志定义等
#include "_Vulkan/GN_Vk.h" //2D图形的绘制和帧缓冲数据读取等转换操作
#include "GN_2D.h" //2D复合图形绘制 线绘制 屏幕截图 绘图屏幕比例控制 画中画子视图功能
#include "GN_Input.h" //集合各种键盘 鼠标 触摸屏等输入的数据 记录和回放输入操作 便捷计时器等 是引擎比较实用的一个模块
#include "GN_Init.h" //控制引擎整体运行流程 自动更新 运行错误修复和一些方便的引擎系统初始化方法 包括处理加载ini文件 简化用户使用引擎程序框架
#include "_Main/GN_Main.h" //系统平台程序入口调度转换 绘图窗口的创建 管理主循环使程序能不停地运行等 该模块主要为运行引擎与操作系统平台之间的一个桥梁
#ifdef GN_USING_UTILITY_LIB
#include "GN_InputAct.h" //输入动作组合响应功能
#include "GN_Font.h" //在图形模式下高速显示漂亮文字的功能
#include "GN_RunRec.h" //引擎运行记录信息输出 如监察运行分支情况 运行中的变量值 统计代码运行时间 进度条等
#include "GN_AmCtrl.h" //动画时间播放控制功能
#include "GN_Frame.h" //2D插值动画像flash那样 2D粒子效果 动态文字动画等 是引擎比较实用的一个模块
#include "GN_2Dscene.h" //2D对象管理控制的许多功能 和2D场景文件的2进制读写运行显示 是引擎比较实用的一个模块
#include "GN_UI.h" //用户交互界面组件接口 UI场景文件的2进制读写显示与组件对象管理 是引擎比较实用的一个模块等
#include "GN_UiBase.h" //用户交互界面的一些简单组件单元
#include "GN_UiListGui.h" //用户交互界面的嵌套自定义子区块UI滚动列表组件 能上下或水平滚动并可显示多个子分块自定义UI内容的滚动筐
#include "GN_UiListTree.h" //用户交互界面的滚动列表树组件 有关列表树的显示折叠增删重排操作等功能
#include "GN_UiTextIn.h" //用户交互界面的文本输入框组件
#include "GN_UiFBrowser.h" //用户交互界面的文件浏览组件
#include "GN_UiPopDlg.h" //用户交互界面的弹出选单组件 编辑器选单插件接口
#endif
#endif

// 网络功能所需包含
#ifdef GN_USING_NET
#include "GN_NetBase.h" //网络通讯的一些基础运算
#include "_Net/GN_Net.h" //网络通讯的底层转换功能
#ifdef GN_USING_UTILITY_LIB
#include "GN_NetTrans.h" //网络传输一些实用功能 如基本通讯 http协议通讯 文件下载 异步多任务服务器框架 是引擎比较实用的一个模块
#endif
#else
#ifndef GN_USING_ADDITIONAL_LIB
//云渲染需要依赖网络模块
#define NET_USING_INDEPENDENT
#include <GN_NetBase.h>
#endif
#endif

#if !defined(GN_USING_SOURCE_CODE) && !defined(GN_USING_LIB_EXPORTS)
#pragma comment(lib, "GN_mini.LIB")
#endif

GN_API char *const GN_GetInterfaceVersion(); //获得引擎接口版本号

GN_API char *const GN_GetVersion(); //获得引擎主版本号

GN_API unsigned int GN_GetModuleConstitute(); //获得引擎里的所有构成模块 返回一个模块构成标记位集
//你可这样来判断包含的模块 if(GN_GetModuleConstitute() & GN_USING_UTILITY_LIB)...; 表示包含实用功能库

#if 0 /*********************************************** 用本引擎作后台服务开发时的框架程序示例 ********************************************************/
#include <GN.h>
int main(int argc, char **argv){
    GN_InitSys();
	GN_InitWords();     //初始化语言单词
	GN_InitNetwork();   //只初始化有用的功能模块就可以了

    //...▲▲▲在这编写您服务程序的代码

	GN_UnInitNetwork(); //释放引擎网络模块
	GN_UnInitSys();
}
#endif
//#pragma GCC visibility pop
#endif //end _GN_H_
