#include<GN.h>		 //使用GN必需包含GN.h头文件

P<GNu::sBUBBLE>		 Bubble(1);  //定义一个对话汽泡

extern "C"{
GN_CALL void AppInit(){     //▲初始化应用必须数据 程序开始准备好引擎系统后会调用它一次 
	P<GNg::sIMAGE> image9, arrow, image9_frame, arrow_frame;
	GNu::sBUBBLE::LoadDefStyle(image9, arrow, image9_frame, arrow_frame); //读取系统默认的对话汽泡式样的几个图 底和底筐为9宫格图
	   
	Bubble->InitToData(image9, arrow); //用系统默认的来初始化汽泡式样和汽泡颜色等
	Bubble->SetFrameStyle(image9_frame, arrow_frame, 0xcc555555, GNg::eBLEND_NORMAL, 2); //用系统默认的来设定边筐式样和边筐颜色等 最后个参数:2是表示边线粗度 单位象素
}
GN_CALL void AppUnInit(){   //▲释放应用必须数据 程序运行结束前会调用它一次 
	Bubble.Delete(); //删除对话汽泡
}
GN_CALL void AppDisplay2D(unsigned int nTime){ //▲显示2D画面 程序运行每帧都调用它 
	//绘制一块屏幕大小的灰色来作底色清屏处理
	GNg::ImageDraw(0,GNg::ViewCur->ortho_left,GNg::ViewCur->ortho_top,GNg::ViewCur->orthoW,GNg::ViewCur->orthoH,0xffaaaaaa);
	
	GNg::Draw2DAlphaDirectionSet(GNu::eNEAR_BOTTOM); //使接下来的2D绘制都为向下渐淡的
	GNg::ImageDraw(0,GNg::ViewCur->ortho_left,GNg::ViewCur->ortho_top,GNg::ViewCur->orthoW,GNg::ViewCur->orthoH/2, 0xff556fb5); //绘制一个半个屏大渐变普蓝色块
	GNg::Draw2DAlphaDirectionSet(GNu::eNEAR_NULL);   //取消渐变绘制
	
	//控制   整形值  X  默认值 每次递增2 D键加 A键减 值放到排位0处显示
	GN_RunKCV( int , x , 50 ,     2 ,    GK_D , GK_A   , 0 ); 
	GN_RunKCV( int , y , 280,     2 ,    GK_S , GK_W   , 1 ); //S  W  键控制汽泡上下移动
	GN_RunKCV( int , w , 100,	  2 , GK_RIGHT, GK_LEFT, 2 ); //左 右 键控制汽泡宽度变化
	GN_RunKCV( int , h , 64,	  2 ,  GK_DOWN, GK_UP  , 3 ); //上 下 键控制汽泡高度变化
    
	GN_RunKCOF( frame , GK_R , 4 );	 //用R键控制布尔值 frame 交替变化 值放到排位4处显示
	if(frame){
		Bubble->aColor		= 0x88000000;
		Bubble->aFw			= 0; //无边透明黑色式样
	}else{
		Bubble->aColor		= 0xffffffff;
		Bubble->aFw			= 2; //灰边白色式样
	}
	//显示汽泡(汽泡箭头指向坐标X,  坐标Y, 汽泡范围左上角坐标X, Y, 汽泡显示宽度, 高度)
	Bubble->Show(            400,    300,                   x, y,      w,        h );
	Bubble->Show(            400,    300,				  500, 80,     80,      48 );
}
GN_CALL void AppDisplay3D(unsigned int nTime){}//▲显示3D画面 程序运行每帧都调用它 

GN_CALL void AppRun(unsigned int nTime){}      //▲运行程序后台的处理 程序运行每帧都调用它
}