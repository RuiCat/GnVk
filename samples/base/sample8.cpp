#include<GN.h>
   
GNg::sIMAGE  Img, Img2, StarImg;   //定义图象结构
GNg::cFRAME  Am1, Am2,  StarAm;    //定义动画筐对象
GNg::sAM_EVENT *Ev=0;              //定义命令动画筐怎样运动的事件结构 

extern "C"{
GN_CALL void AppInit(){     //▲初始化应用必须数据 程序开始准备好引擎系统后会调用它一次 
	StarImg.InitToData("samples/base/image/star4_", 1, GNg::CR_ALPHA);	  //该图为白和透明图所以用ALPHA这格式保存图片可以省很多显存空间
	Img.InitToData    ("samples/base/image/s40",    1, GNg::CR_LUMINANCE); //该图为黑白图 所以用LUMINANCE这格式保存图片可以省很多显存空间
	Img2.InitToData   ("samples/base/image/s41");                          //读入图象

	Ev = new GNg::sAM_EVENT[3];
	//设置动画事件( 事件类型,                     开始时间,结束时间,     开始值,  结束值,  缓动值,   缓动类型)
	Ev[0].Init(GNg::eEVENT_MOVE_X,                      0,     3000,       0.0f,    500.0f,  0.0f,  GNg::L_DOUBLE); //这里最后个参数是缓模式来 这里设置为首尾起步慢
	Ev[1].Init(GNg::eEVENT_TEXMOV_X,                    0,     3000,       0.0f,    12.0f); //挪动纹理事件
    Ev[2].Init(GNg::eEVENT_ROTATION,                 0000,     3000,       0.0f,   360.0f);
	
	Am2.Init(             &Img2,      1,      &Ev[0],GNg::sTRANSF2D(100,120),0.0f,0.0f, GNg::eBLEND_NORMAL, 0xffffffff, GNg::eAM_LOOP,     4,			 3);

	//为动画筐Am1装填图象(指定图象,动画事件个数,动画事件数组, 变换信息(x,y),旋转定点中心对左上角, 混合模式,		 颜色,       没有帧动画,   水平平铺7格,   垂直平铺5格) !!最后两个参数:负数为重复平铺数 正数为切格数
	Am1.Init(             &Img,       0,       0,    GNg::sTRANSF2D(0,0),    0.0f,0.0f, GNg::eBLEND_NORMAL, 0xff726555, GNg::eAM_NOAM,    -7,            -5);
	Am1.TextureMove(0.4f, 0.2f); //设置背景拖动速度
    
	//为动画筐星星装填图象(星图象, 动画事件个数, 动画事件数组, 变换信息(x,y), 旋转顶点向中心点下拉2倍位置, 发光混合模式,	  颜色橙色,   播放一次消失 ) 
	StarAm.Init(         &StarImg,       1,       &Ev[2], GNg::sTRANSF2D(0,0),  0.5f,  2.0f,			 GNg::eBLEND_BRIGHT, 0xffff8800, GNg::eAM_ONCE_HIDE); 
}
GN_CALL void AppUnInit(){   //▲释放应用必须数据 程序运行结束前会调用它一次 
	GN_DeleteArray(Ev);		//安全释放内存
	Img.Release();			//释放图象数据
	Img2.Release();
	StarImg.Release();
}
GN_CALL void AppDisplay2D(unsigned int nTime){ //▲显示2D画面 程序运行每帧都调用它 
	if(Am2.GetTextureSquareWN() > 0){ //当有分格时并这按下键盘这些键时
		if(GN::Key1[GK_1]){ Am2.SetTextureSquare(0,0); } //设置选择分格的第几格
		if(GN::Key1[GK_2]){ Am2.SetTextureSquare(1,0); } //设置选择分格的第几格
		if(GN::Key1[GK_3]){ Am2.SetTextureSquare(2,0); } //设置选择分格的第几格
		if(GN::Key1[GK_4]){ Am2.SetTextureSquare(3,0); } //设置选择分格的第几格
		if(GN::Key1[GK_8]){ Am2.SetTextureSquare(3,1); } //设置选择分格的第几格
	}
	if(GN::Key1[GK_A]){
		Am2.Init(&Img2, 2, &Ev[0], GNg::sTRANSF2D(100,120), 0.0f,0.0f, 1, 0xffffffff, GNg::eAM_LOOP, 4, 3); //动画事件加入纹理拖动
	    Am2.InitSubFrame( &StarAm, 1); //Am2加入子筐对象 使星动画筐能跟随Am2筐 该功能也可以做关节动画用
	}
	if(GN::Key1[GK_F]){
		Am2.Init(&Img2, 2, &Ev[0], GNg::sTRANSF2D(100,120), 0.0f,0.0f, 1, 0xffffffff, GNg::eAM_LOOP); //没指定格数 当有使用拖动纹理事件会就这样拖动在筐内的纹理
	    Am2.InitSubFrame( &StarAm, 1); //Am2加入子筐对象,使星动画筐能跟随Am2筐 这功能也可以做关节动画用
	}
    if(GN::Key1[GK_S]){
		static bool ks = false;
		ks = !ks;
		if(ks)Am1.TextureStopMove();    //停止拖动背景
	    else Am1.TextureMove(0.8f,0.4f);//再拖动  背景
	}
	if(GN::Key1[GK_R]){
		if(Am2.GetSubFrameNum())Am2.SubFrame(0).ReSetPlaytime(); //判断Am2有子对象你时 重设置Am2的首个子筐对象动画 使能从新播放
	}
	Am1.PlayFrame(nTime);   //播放背景动画 这里没有画矩形 因为背景足够大
    Am2.PlayFrame(15);      //播放Am2动画 后面的参数你可以试试调为其他值或0,灵活运用这个可以控制动画播放进度和速度
}
GN_CALL void AppDisplay3D(unsigned int nTime){} //▲显示3D画面 程序运行每帧都调用它 

GN_CALL void AppRun(unsigned int nTime){}      //▲运行程序后台的处理 程序运行每帧都调用它
}