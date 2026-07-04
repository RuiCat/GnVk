#include<GN.h>
#if 1
extern "C" {
	void AppInit();
	void AppUnInit();
	void AppDisplay2D(unsigned int nTime);
	void AppDisplay3D(unsigned int nTime);
	void AppRun(unsigned int nTime);
}
GN_MAIN;
#else
GN_MAIN;
GN_InitCallBackFuncFromModule("App"); //使用 App 动态库的应用回调接口
#endif
#if 1
GN_InitLoadCallBackFunc( //设置读取回调函数
	/*--读取资源的回调函数*/ AppInit,
	/*--释放资源的回调函数*/ AppUnInit
);
GN_InitLoopCallBackFunc( //设置循环回调函数
	/*显示2D画面的回调函数*/ AppDisplay2D,
	/*显示3D画面的回调函数*/ AppDisplay3D,
	/*--运行处理的回调函数*/ AppRun
);
#endif
GN_InitParameDefaultStyle(3);		  //设置系统默认字体样式 1:细英文 2:粗英文 3:正常中文
GN_StartRun( //启动应用程序
	/*         窗口标题*/ "GN Sample",
	/*     屏幕窗口宽度*/  800,
	/*     屏幕窗口高度*/  600,
	/*       颜色深度位*/  0,		  //移动设备需要使用高级3D渲染指定32位色 否则默认0很可能只给分配16位色深
	/*         是否全屏*/  false,
	/*     窗口可缩放否*/  true,
	/* 系统菜单是否可见*/  true,
	/*       取景范围左*/  0,
	/*       取景范围上*/  0,
	/*       取景范围右*/  0,
	/*       取景范围下*/  0
);
#if 0
GN_CallBackFuncModuleRelease(); //退出程序 释放 App 动态库占空间
#endif
GN_EXIT;
