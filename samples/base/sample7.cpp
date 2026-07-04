#include<GN.h>

GNg::sIMAGE  BgImg;				  //背景图
GNg::sIMAGE  InsectImg;			  //虫子图
GNg::cFRAME  Am1, Am2, InsectAm;  //动画筐对象
P<GNg::sAM_EVENT> Ev;             //命令动画筐怎样运动的事件结构 

extern "C"{
GN_CALL void AppInit(){     //▲初始化应用必须数据 程序开始准备好引擎系统后会调用它一次 
	BgImg.InitToData("samples/base/image/bg");         //读入背景图象文件
	InsectImg.InitToData("samples/base/image/chong",7);//读入虫子动画图组
	
	//为动画筐Am1装填图象(指定图象,动画事件个数,动画事件数组, 变换信息(x, y,  计算出背景图放大至屏幕的缩放比 使得填满整个屏幕)
	Am1.Init(            &BgImg,      0,           0,    GNg::sTRANSF2D(0,0, (float)GNg::ViewCur->orthoW/(float)BgImg.width, (float)GNg::ViewCur->orthoH/(float)BgImg.height));            
	Am2 = Am1; //相同动画的方便赋值方法

	Ev.New(5);
	//设置动画事件( 事件类型,                      开始时间,  结束时间,     开始值,  结束值, 缓动值,   缓动类型)
	Ev[0].Init(GNg::eEVENT_MOVE_X,                       0,     4000,       0.0f,    200.0f,  0.4f,  GNg::L_DOUBLE);
	Ev[1].Init(GNg::eEVENT_SIZE_X|GNg::eEVENT_SIZE_Y, 1800,     4000,       1.0f,     0.65f);
	Ev[2].Init(GNg::eEVENT_ROTATION,                  2000,     4000,       0.0f, 180.0f);
	Ev[3].InitColor(                                     0,     4000, 0xffffffff, 0x55ffffff);
	Ev[4].Init(GNg::eEVENT_TEXID,                        0,      800,       0.0f,    6.0f);
	
	//InsectAm装填图象(指定图象,动画事件个数,动画事件数组, 变换信息(x,y),  以图中心为旋转定点, 旋转定点,	混合模式,		  颜色,		   播放模式)
	InsectAm.Init(&InsectImg,    Ev.Num(),         Ev, GNg::sTRANSF2D(100,430),		0.5f,		0.5f,    GNg::eBLEND_NORMAL, 0xffffffff,  GNg::eAM_REPEAT);
}
GN_CALL void AppUnInit(){   //▲释放应用必须数据 程序运行结束前会调用它一次 
	Ev.Delete();		   //安全释放 这个不释放也可以  程序退出也会自动释放
	InsectImg.Release();   //释放图组数据
	BgImg.Release();       //释放图象数据
}
GN_CALL void AppDisplay2D(unsigned int nTime){ //▲显示2D画面 程序运行每帧都调用它 
	//画一个屏幕大小的矩型(图象对象, x,y,   宽,  高,   颜色    ） 作为清屏用
	GNg::ImageDraw(0,               0,  0, 800,  600,0xff110077);
   
	Am2.PlayFrame(15); //播放背景动画 参数为每帧得间隔 现在默认写死为15毫秒

	//键盘控制状态变量:键盘压下A,D,W,S键时为1 松开时为0
	if(GN::Key[GK_A])Am1.aFrame_ts.x -= 3.0f;  //用 .aFrame_ts 可以动态控制动画筐 动画筐事件动画与这里的动态控制可以一起互相使用不相影响
	if(GN::Key[GK_D])Am1.aFrame_ts.x += 3.0f;
	if(GN::Key[GK_W])Am1.aFrame_ts.y -= 3.0f;
	if(GN::Key[GK_S])Am1.aFrame_ts.y += 3.0f;    
	
	//键状态 Key1 为只按一下的状态  哪怕您按紧C键该值也只会在刚按下时为true
	if(GN::Key1[GK_C]){ 

		//重新指定新动画(装图, 1个动作 ,动作数据位置,   变换信息(x, y,  放大x,放大y), 以图中心为旋转定点, 旋转定点)
		Am1.Init( &InsectImg,  1,          Ev + 2,      GNg::sTRANSF2D(0,0,2.0f,2.0f),    0.5f,				 0.5f); 
	}
	Am1.PlayFrame(nTime);		//播放Am1动画
	InsectAm.PlayFrame(nTime);  //播放InsectAm动画
}
GN_CALL void AppDisplay3D(unsigned int nTime){} //▲显示3D画面 程序运行每帧都调用它 

GN_CALL void AppRun(unsigned int nTime){}       //▲运行程序后台的处理 程序运行每帧都调用它
}