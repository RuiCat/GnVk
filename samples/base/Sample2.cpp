#include<GN.h>		   //使用GN库必需包含这个头文件

GNg::sIMAGE	    Img;   //定义一个图象
GNg::sIMAGE	    Img1;  //另一个图象

extern "C"{
GN_CALL void AppInit(){     //▲初始化应用必须数据 程序开始准备好引擎系统后会调用它一次 
	Img.InitToData("samples/base/image/s11_"); //读入图象文件
	GNg::ImageLoad("samples/base/image/red_bright_", 1, &Img1, GNg::CR_LUMINANCE_ALPHA); //读入图象文件 并仅用灰度和透明的格式装载图像数据 !这能更节省空间
}
GN_CALL void AppUnInit(){   //▲释放应用必须数据 程序运行结束前会调用它一次 
	Img.Release();  //释放图象数据
	Img1.Release(); 
}
GN_CALL void AppDisplay2D(unsigned int nTime){ //▲显示2D画面 程序运行每帧都调用它 
	
	GN_RUNTSTART(1); //这个调试信息宏用来计算一段代码运行的时间( 重复执行1次 )

		//画一个窗口大小的矩型(图象,  坐标X, Y,  宽,  高,    颜色     )  !如果背景已经有覆盖全屏的图 您可省掉这个矩形绘制以节省开销
		GNg::ImageDraw(			0,      0,	 0, 800, 600,  0xff110077 );
		
		//抖动模糊绘制图象(	图象,   坐标x, y,   宽度,    高度, 一个透明的颜色, 绘制图序中的首张, 标准的混合模式,	抖动重复绘制6次 该值越大开销越高,   抖动比率) 
		GNg::ImageDrawDitherBlur(&Img,10,  10, 800-20,  600-20, 0x5affffff,    0,			   GNg::eBLEND_NORMAL,    6,							   0.01f);
		
		//再绘制多次图象(图象, 居中坐标x,			居中的坐标y,    宽 0表示原大小 , 高 同理)
		GNg::ImageDraw(&Img,  400-Img.width/2,    300-Img.height/2,   0,				0); 

		//用原大小绘制图象(图象, 坐标x,  y,    宽 0表示原大小 , 高 同理) !您可看到用 CR_LUMINANCE_ALPHA 格式装载的图像会无彩色 但您可以在绘制时给一个颜色参数
		GNg::ImageDraw(&Img1,      0,    0,        0,			 0);

		//画一个线筐         (x,  y,   宽,    高, 线宽,  颜色,		混合模式);
		GNg::LineFrame2dDraw(10, 10, 800-20,600-20, 3, 0xac00ff00, GNg::eBLEND_BRIGHT);
	
	GN_RUNTEND();   //这个调试信息宏是和GN_RUNTSTART()配对的 两个宏必需放在同一作用域内 配对以后运行统计时间将自动显示出来
}
GN_CALL void AppDisplay3D(unsigned int nTime){} //▲显示3D画面 程序运行每帧都调用它 

GN_CALL void AppRun(unsigned int nTime){}       //▲运行程序后台的处理 程序运行每帧都调用它
}