#ifndef _GN_SYSDEF_H_
#define _GN_SYSDEF_H_

/* 系统平台: 单项选择 */
//#define GN_USING_OPENGL_WINDOW             //★适用于普通配置电脑 Window系统 推荐选这个（通过 Makefile PLATFORM= 选择）
//#define GN_USING_OPENGL_SDL_WINDOWMODE     //适用于一般应用兼容性好 Window系统 显示2D用
//#define GN_USING_OPENGL_GLUT               //适用于普通配置电脑 Window系统
//#define GN_USING_OPENGLes_GLUT             //适用于低配置嵌入式系统        !!该编译路径暂未完成
//#define GN_USING_DIRECTX9_WINDOW           //适用于Window系统 !!该编译路径3D摸块暂未完成
//#define GN_USING_DIRECTDRAW_WINDOW         //适用于Window系统纯2D 不具硬件加速 平台具体差异参考上面的 DIRECTDRAW 图形模式的注意事项
//#define GN_USING_DIRECTDRAW_WINCE		     //适用于WinCE 系统纯2D 不具硬件加速 平台具体差异参考上面的 DIRECTDRAW 图形模式的注意事项
//#define GN_USING_OPENGL_SDL_LINUX          //★适用于一般应用兼容性好 Linux系统 暂不能动态控制窗口状态 且还没适配好中文字符输入
//#define GN_USING_OPENGL_GLUT_LINUX         //适用于普通配置电脑 Linux系统 暂未能开启全屏抗锯齿 且还没适配好中文字符输入
//#define GN_USING_OPENGL_GLUT_MAC           //★适用于普通配置电脑 Mac系统
//#define GN_USING_OPENGL_SDL_MAC            //这个Mac下还不完善 性能不够佳和暂不能动态控制窗口状态 且还没适配好中文字符输入
//#define GN_USING_OPENGLes_IOS              //★适用于苹果移动产品系列系统 如:iphone ipad itouch 最好系统版本为3.0以上 该平台默认为全幕模式 程序在最少化时会被暂停运行
//#define GN_USING_OPENGLes_ANDROID_JAVA     //★适用于Android系统经java层调用 可兼容更多系统层面的交互
//#define GN_USING_OPENGLes_ANDROID          //Android系统全C++原生程序应用
//#define GN_USING_VULKAN_GLFW_LINUX      //★Vulkan+GLFW Linux (替代OpenGL+GLUT)

/*--------------------------------- !! 关于使用 DIRECTDRAW 图形模式注意事项 !! ------------------------------------*\
|                                                                                                                   |
|		一般情况下推荐用OpenGL作显示,不过有时可能遇到不支持3D硬件加速的平台环境,无法之下再考虑使用DIRECTDRAW        |
|		使用DIRECTDRAW是不具备图形旋转,反转,半透明混合,颜色调节的(包括图象变色,文字的颜色,想要有色的文字必需        |
|		在字库里制作好中途不能动态改变),应用程序应该尽量不要用以上效果,用了虽然也可编译顺利运行,但显示出来的        |
|		效果可能和你预期有些差别,缩放效果也应该尽量少用,用也不要用在大图之上,因为DIRECTDRAW那些效果都没硬件         |
|		加速, 用了会拖慢运行速度!!                                                                                  |
|	                                                                                                                |
|		如想在DIRECTDRAW环境下使用3D模块的功能,只能把场景内容放在循环显示2D内容的地方显示,摄像机,灯光,材质,         |
|		渲染效果等,那些3D的概念一律忽略掉,至于在建模工具里拉平面之前要得把平面分段的参数宽高都设置为1,否则某        |
|		些部分可能会忽略显示,要改图片大小尽量不要用外面的比例变换工具变换,应进入里面修改参数来得到精确实际大        |
|		小,以免缩放了图片. 要保持图形的4个角都为90度,否侧位置显示可能会乱!                                          |
|	                                                                                                                |
|		如果在WINCE环境,因声效的接口亦没有3D加速,不同音效对象是不支持独立控制音量的,要改音量只能在音频文件里        |
|		预先做好,音调不支持动态调节,BGM设置的音量变化延时过渡不产生作用!                                            |
|	                                                                                                                |
\*-----------------------------------------------------------------------------------------------------------------*/
#ifdef GN_USING_OPENGL_WINDOW
#define GT_USING_OPENGL 1 //该宏可以调大为3以使用更高版本 OpenGL
#define _WIN32_
#elif defined GN_USING_OPENGL_SDL_WINDOWMODE
#define GT_USING_OPENGL 1
#define GT_USING_GLsdl
#define GT_USING_WINDOW_MODE //捆绑 Windows 窗口模式
#define _WIN32_
#elif defined GN_USING_OPENGL_GLUT
#define GT_USING_OPENGL 1
#define GT_USING_GLUT
#define _WIN32_
#elif defined GN_USING_OPENGLes_GLUT
#define GT_USING_OPENGL 1
#define GT_USING_GLes
#define GT_USING_GLUT
#define _WIN32_
#elif defined GN_USING_DIRECTX9_WINDOW
#define GT_USING_DIRECT3D9
#define _WIN32_
#elif defined GN_USING_DIRECTDRAW_WINDOW
#define GT_USING_DDRAW
#define _WIN32_
#elif defined GN_USING_DIRECTDRAW_WINCE
#define GT_USING_DDRAW
#ifndef _WIN32_WCE
#define _WIN32_WCE
#endif
#elif defined GN_USING_VULKAN_GLFW_LINUX
#define GT_USING_VULKAN 1
#define _LINUX_
#elif defined GN_USING_OPENGL_SDL_LINUX
#define GT_USING_OPENGL 1
#define GT_USING_GLsdl
#define _LINUX_
#elif defined GN_USING_OPENGL_GLUT_LINUX
#define GT_USING_OPENGL 1
#define GT_USING_GLUT
#define _LINUX_
#elif defined GN_USING_OPENGL_GLUT_MAC
#define GT_USING_OPENGL 1
#define GT_USING_GLUT
#define _MAC_
#define HAVE_NANOSLEEP 1
#elif defined GN_USING_OPENGL_SDL_MAC
#define GT_USING_OPENGL 1
#define GT_USING_GLsdl
#define _MAC_
#elif defined GN_USING_OPENGLes_IOS
#define GT_USING_OPENGL 1
#define GT_USING_GLes
#define GT_USING_IOS
#elif defined GN_USING_OPENGLes_ANDROID_JAVA
#define GT_USING_OPENGL 1
#define GT_USING_GLes
#define GT_USING_ANDROID_JNI
#define _LINUX_
#ifndef GN_USING_UNZIP
#define GN_USING_UNZIP
#endif
#endif
#if defined _MAC_ || defined GT_USING_IOS
#ifndef _APPLE_
#define _APPLE_
#endif
#endif

//预包含标准头文件
#ifndef _INC_MATH
#include <math.h>
#endif
#ifndef _INC_STDIO
#include <stdio.h>
#ifndef _IOEOF
#define _IOEOF 0x0010
#endif
#endif
#ifndef _INC_STDLIB
#include <stdlib.h>
#endif
#ifndef _INC_STRING
#include <string.h>
#endif
#if !defined(_ALGORITHM_) && !defined(_GLIBCXX_ALGORITHM) && !defined(_LIBCPP_ALGORITHM)
#include <algorithm>
#endif
#ifndef _VECTOR_
//#include<vector>
#endif
#ifndef _LIST_
#include <list>
#endif
#if defined(_INC_STDIO) || defined(_INC_IO) || defined(_ALGORITHM_) || defined(_INC_STDARG) || defined(_INC_STRING)
#pragma warning(disable : 4996)
#endif
#if defined _WIN32_ || defined _WIN32_WCE
//#define GT_TMP_COMPATI_DX_DDRAW  //在WINDOW时 同时兼容动态库里可用 DX 和 DDRAW API
#ifdef _MSC_VER
#if !defined(_GN_H_) || defined _WIN64
#ifdef _WIN64
#define __LP64__ 1
#endif
#include <windows.h>
#endif
#else
#include <windows.h>
#include <emmintrin.h>
#endif
typedef volatile long GT_sATOMIC;
#else
#include <atomic>
typedef std::atomic<int> GT_sATOMIC;
#endif

#ifdef GN_USING_SOURCE_CODE //源代码编辑
#define GN_API
#define GN_CALL
#elif defined GN_USING_LIB_EXPORTS //动态库导出
#if defined _WIN32_ || defined _WIN32_WCE
#define GN_API __declspec(dllexport)
#define GN_CALL __declspec(dllimport)
#else
#define GN_API extern
#define GN_CALL extern
#endif
#else //---------------------------- //SDK方式使用
#if defined _WIN32_ || defined _WIN32_WCE
#define GN_API __declspec(dllimport)
#define GN_CALL __declspec(dllexport)
#else
#define GN_API
#define GN_CALL
#endif
#endif
#ifdef _MSC_VER
#define TMPVAR(TYPEV)
#else
#define TMPVAR(TYPEV) TYPEV
#endif
#endif //endif _GN_SYSDEF_H_
