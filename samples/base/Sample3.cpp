#include<GN.h>		   //使用GN库必需包含这个头文件

GNg::sIMAGE		    Img;        //定义一个图象
GNg::sIMAGE		    Img1;       //线式样图
GNg::sIMAGE			BgImg;	    //这个为背景图
GNg::sDRAW_RANGE   *DrawRngs=0; //一组绘制范围
extern "C"{
GN_CALL void AppInit(){     //▲初始化应用必须数据 程序开始准备好引擎系统后会调用它一次 
	Img.InitToData("samples/base/image/chong", 7); //读入图象的7个图序文件
	GNg::ImageLoad("samples/base/image/arrow_", 1, &Img1); //读入线式样图 图的内容可以仅有线的一小节不重复的即可 以节省空间
	GNg::ImageLoad("samples/base/image/bg", 1, &BgImg);

	DrawRngs = new GNg::sDRAW_RANGE[4]; //分配4个绘制范围
	//预先设置好各个绘制的位置
	DrawRngs[0].x  = 120;
	DrawRngs[0].y  = 100;
	DrawRngs[1].x  = 570;
	DrawRngs[1].y  = 128;
	DrawRngs[2].x  = 100;
	DrawRngs[2].y  = 460;
	DrawRngs[3].x  = 430;
	DrawRngs[3].y  = 450;
	int i = 0;
	for(; i < 4; ++i){ //其它属性的初始化
		DrawRngs[i].w  = Img.width;  //采用原图片大小
		DrawRngs[i].h  = Img.height;
		DrawRngs[i].u1 = 0.0f;   //默认截取范围 即整个图片 您可以修改一下这些看看会有什么变化!
		DrawRngs[i].v1 = 0.0f;
		DrawRngs[i].u2 = 1.0f;
		DrawRngs[i].v2 = 1.0f;
		DrawRngs[i].tid = i;	 //引用到的图序ID
		DrawRngs[i].strid = 0;   //非文字显示用时这个为0即可
	}
}
GN_CALL void AppUnInit(){   //▲释放应用必须数据 程序运行结束前会调用它一次 
	GN_DeleteArray(DrawRngs);    //检测DrawRngs是否有数据 如有就释放它的空间
	BgImg.Release();			 //释放背景图象数据
	Img.Release();				 //释放图象数据
	Img1.Release();				 //释放线式样图数据
}
GN_CALL void AppDisplay2D(unsigned int nTime){ //▲显示2D画面 程序运行每帧都调用它 
	//绘制背景(背景图象,  坐标X, Y,  宽,   高)  !如果指定的宽高比原图像大时即会缩放图像到指定大小
	GNg::ImageDraw(	&BgImg, 0,	 0,  800, 600);
	
	GNg::sVEC2 v;
	int i = 0, i2;
	for(; i < 3; ++i){
		i2 = i + 1;
		//各绘制位置间画一条线(线式样图,  位置1x,        位置1y,		位置2x,      位置2y,			线宽,     线中心点落在相对线宽的比率)
		GNg::Draw2DLine(      &Img1,    DrawRngs[i].x, DrawRngs[i].y, DrawRngs[i2].x, DrawRngs[i2].y, Img1.height, 0.5f); //这里注意连线指向到图的左上角 而不是图中心
		
		if((GN::TimeGetRunFpsCount() % 16) == 0){ //每隔16帧
			DrawRngs[i].tid++; //递增一下引用的图序ID 以实现播放序列动画
			if(DrawRngs[i].tid >= (int)Img.texnum)DrawRngs[i].tid = 0; //如果图序ID引用超出序列的数量范围就设置从0开始
		}
	} //一口气绘制图像多次(图像, 各个绘制范围数据, 范围数,  所有图颜色, 所有图混合模式) !如果同一个图像要绘制很多次的话调用这个性能更好  引擎里在显示文字时也会在内部调用到这个
	GNg::ImageDrawMP(	&Img,	  DrawRngs,			 4,	    0xffffffff, GNg::eBLEND_NORMAL);  
	
	//旋转绘制图像 (图像 , 坐标X, Y,  图宽度,    高度,		 旋转角度 这里按各帧给个不同旋转角度, 旋转中心定点处于图的横向比率, 竖向比率,    颜色,	图序ID)		
	GNg::ImageDrawRot(&Img, 350, 270, Img.width, Img.height, (float)(GN::TimeGetRunFpsCount()%360),	  0.5f,						 0.41f,   0xffffffff,  4);
}
GN_CALL void AppDisplay3D(unsigned int nTime){} //▲显示3D画面 程序运行每帧都调用它 

GN_CALL void AppRun(unsigned int nTime){}       //▲运行程序后台的处理 程序运行每帧都调用它
}