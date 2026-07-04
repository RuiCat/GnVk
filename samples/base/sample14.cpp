#include<GN.h>

#define USPOP_EFFECT	    //应用动画效果弹出 把这个注释掉就没弹出和特殊效果

/*------------------ 定义对话筐要维护修改的变量 只能为int 或 float 或 unsigned int 颜色类型 ------------------------*/
int PassScoreRange = 50000;

int   GameVolume = 100;    //在这里指定的值为该变量的默认值 在对话筐里点击“DEFAULT”就是还原这些值的
int   GameMode   = 1;
int   GamePayout = 80;
float GameSheep  = 1.0f;
 
int LineW		 = 2;
int LineBlend	 = 0;

float ValueF	 = 6.566224f;
int   ValueI	 = 0;
/*-------------------------------------------------------------------------------------------------------------------*/
GNu::sSIM_MENU   Sub1;		//子选单1
GNu::sSIM_MENU   Sub2;		//子选单2
GNu::sSIM_MENU   Sub3;		//子选单3
GNu::cSIM_DIALOG Sdlg;		//定义一个简单对话筐
GNg::sIMAGE		 AsciiImg;	//粗英文字图
GNg::sIMAGE		 BgImg;		//底纹用的图片
GNg::cFRAME		 Bg;		//控制底纹的动画筐
GNa::sSOUND		 KeySo;		//按键音

#ifdef USPOP_EFFECT
GNg::sIMAGE      ArrowImg;  //小箭头图
GNg::sIMAGE      FocusImg;  //选择焦点图
GNg::sIMAGE      DlgImg;    //对话筐用的底图
GNg::cFRAME      PopEF;     //弹出动画效果参照筐
GNg::sAM_EVENT  *PopEv;     //动画弹出帧关键点事件数组指针
GNa::sSOUND      PopSo;     //弹出音
#endif

extern "C"{
GN_CALL void AppInit(){     //▲初始化应用必须数据 程序开始准备好引擎系统后会调用它一次 
	KeySo.InitToData("samples/base/sound/sel.wav");				  //读入声音文件（文件路径）
	BgImg.InitToData("samples/base/image/s40", 1, GNg::CR_LUMINANCE);  //读入图片并用灰度单色格式保存
	AsciiImg.InitToData("samples/base/image/ascii_system2");

	//设置动画筐(指定所用图片, 帧事件数, 帧事件数组, 坐标变换结构, 缩放定点在左上角, 缩放定点在左上角, 混合模式,		 颜色,     播放模式,       水平分格,  垂直分格)
	Bg.Init(      &BgImg,       0,         0,         GNg::Ts2D,      0.0f,         0.0f,			GNg::eBLEND_NORMAL, 0xff485b6a, GNg::eAM_LOOP,    -7,        -6);   
	Bg.TextureMove(-0.2f,-0.15f); //设置底纹移动x和y方向速度
	/*-------------------------------------------------------------------------------------------------------------------------*/
	 
	//Sdlg.BeginInitToData("DlgValueSave.ygs"); //准备插入变量数据(数据保存的文件) 该类会自动维护该文件的数据读写保存 该函数可以忽略 如果没指定的话引擎也会自动创建一个维护文件
	
	//插入整型变量元素(  变量指针,       用于显示的名称,  最大值,   最小值,  一次递增值,  附加注息内容)
	Sub3.InitValueToData(&PassScoreRange,"PassRange:",	100000000,  50000,	    1000,       "$$$");
	/*-------------------------------------------------------------------------------------------------------------------------*/
	//      插入文本   (     内容			  )
	Sub2.InitTextToData(" 1. Dragon    9988745");
	Sub2.InitTextToData(" 2. Mary      8459495");
	Sub2.InitTextToData(" 3. Justice   6887743");
	Sub2.InitTextToData(" 4. Yumi        94456");
	Sub2.InitTextToData(" 5. Orange     534354");
    Sub2.InitTextToData(" 6. Value     % 7d",&ValueI);
	//插入子菜单         (子菜单指针, 子菜单标题名, 功能按钮个数) 最多3个功能按钮分别为 1.Cancel 2.Save 3.Default
	Sub2.InitSubMenuToData(&Sub3,  " Pass Score Range",  2);
    /*-------------------------------------------------------------------------------------------------------------------------*/
	//  插入整形变量元素( 变量指针, 用于显示的名称, 最大值,  最小值,  一次递增值, 附加注息) 
	Sub1.InitValueToData(&GameVolume,"Volume:",		 100,		0,		5);
	Sub1.InitValueToData(&GameMode,  "Mode:",		 2,			0,		1,		"EASY|NORMAL|HARD"); //在插入整型变量的附加注息里用'|'符号可以分隔文本作为标志放入变量值内显示
    Sub1.InitValueToData(&GamePayout,"Payout:",		 100,		60,		1,	    "%");
    //  插入浮点变量元素
	Sub1.InitValueToData(&GameSheep, "Sheep:",		 2.0f,		0.0f,   0.1f);
    /*-------------------------------------------------------------------------------------------------------------------------*/
	
	Sdlg.InitTextToData(" DLG-BG-STYLE:");
	Sdlg.InitValueToData(&Bg.aFrame_ts.sx, "BG Size x:",		2.0f, 0.0f, 0.2f);
	Sdlg.InitValueToData(&Bg.aFrame_ts.sy, "BG Size y:",		2.0f, 0.0f, 0.1f);
	Sdlg.InitValueToData(&LineW,		   "Line Width:",		10,		1,	  1,   "set width!");
	Sdlg.InitValueToData(&LineBlend,	   "Frame Blend:",		2,		0,	  1,   "HIDE|NORMAL|BRIGHT");
	Sdlg.InitSubMenuToData(&Sub1,		   " Game Setting",		3);
    Sdlg.InitSubMenuToData(&Sub2,		   " Game Score Table", 1);
	/*-------------------------------------------------------------------------------------------------------------------------*/
	Sdlg.EndInitToData(); //结束插入变量数据 调用这个通知系统帮您维护这些变量的保存文件操作 当用户按保存按钮时即会自动保存到文件 并采用安全方式 即使中途断电也不会使文件记录损坏

	GNg::cTEXT_ART text_style;
	text_style.Init(&AsciiImg); //初始化粗英文式样

	//设置对话筐式样和文字(主对话筐标题,         文字式样,  功能按钮数,  标题颜色,  正文颜色,  底筐颜色,  线筐颜色...) 值0的为默认值 后面的参数也可不填选默认值
	Sdlg.SetStyleToData(" GN GAME OPTION :",  &text_style,     3,      0xffffffff,     0,         0,      0xabdd66aa); 
	 
	//设置控制键   ( 键声音,    确定键,  下选键,  上选键,  左选键,  右选键, 是否启用鼠标控制)
	Sdlg.SetKeyToData(&KeySo, GK_RETURN, GK_DOWN, GK_UP,  GK_LEFT, GK_RIGHT,  true);  //这函数可不指定 均有默认值
	Sdlg.EnableDetailSetting(true); //开启详细设置修改变量

   #ifdef USPOP_EFFECT
	ArrowImg.InitToData("samples/base/image/arrow_");		//读入箭头图片文件
	FocusImg.InitToData("samples/base/image/focus_");		//读入选择焦点图片文件
	DlgImg.InitToData  ("samples/base/image/alphawindow0_"); //读入窗口背景图片文件
	PopSo.InitToData   ("samples/base/sound/pop.wav");       //读入弹出声音文件
	//设置弹出效果动画关键点数据
	PopEv = new GNg::sAM_EVENT[4];
	PopEv[0].InitColor(0, 400, 0x00ffffff, 0xffffffff);
	PopEv[1].Init(GNg::eEVENT_SOUND, 100, 400);
	PopEv[2].Init(GNg::eEVENT_SIZE_X|GNg::eEVENT_SIZE_Y, 0,   400, 0.2f, 1.0f);
    PopEv[3].Init(GNg::eEVENT_SIZE_X|GNg::eEVENT_SIZE_Y, 400, 430, 1.0f, 1.0f);
	
	//初始化设置弹出动画筐对象(底图对象, 关键点数据个数, 关键点事件数组,  摆放坐标结构,      水平中心对位, 垂直中心对位, 混合模式,			颜色,       播放模式)
	PopEF.Init(                &DlgImg,      4,              PopEv,   GNg::sTRANSF2D(130,110),  -0.2f,       -0.1f,   GNg::eBLEND_NORMAL, 0xffffffff, GNg::eAM_ONCE_KEEP);
	
	//插入声音到动画筐( 声音对象 )
	PopEF.InitSoundToData(&PopSo);
	
	//设置动画附加效果(参照的动画筐对象, 选择焦点闪动间隔时间, 选择焦点图, 小箭头图)
	Sdlg.SetEffect(    &PopEF,				   700,             &FocusImg, &ArrowImg);   //该函数可不指定
	
	Sdlg.aFocusColor   = 0xccffffff;  //更改选择焦点颜色
	Sdlg.aTextColor    = 0xff275e85;  //更改文本显示颜色
   #endif
	Sdlg.aCaptextOfsY  = 4;			  //使标题显示位置向下偏移一点
	Sdlg.Pop();  //弹出对话筐
}
GN_CALL void AppUnInit(){   //▲释放应用必须数据 程序运行结束前会调用它一次 
	AsciiImg.Release();		//释放字图
	BgImg.Release();		//释放图片
	Sdlg.ReleaseAll();		//释放对话筐,并连它之内包含的子选单也一起释放
	KeySo.Release();		//释放按键声音
   #ifdef USPOP_EFFECT
	PopEF.Release();        //释放效果筐
	GN_DeleteArray(PopEv);  //删除帧关键点事件数据
	PopSo.Release();        //释放弹出声音
	DlgImg.Release();       //释放对话筐底图
	ArrowImg.Release();     //释放小箭头图
	FocusImg.Release();     //释放选择焦点图
   #endif
}
GN_CALL void AppDisplay2D(unsigned int nTime){ //▲显示2D画面 程序运行每帧都调用它 
	GNg::ImageDraw(0, 0,0, 800, 600, 0xffdd66aa);  //绘制矩形以清屏
	Bg.PlayFrame(15);   //播放显示底纹动画
	
	GN_RunValueCheck(LineW,"LineW",10);  //用调试信息显示查看该变量经修改后的值
	GN_RunValueCheck(LineBlend,"LineBlend",11); 
	Sdlg.aLineWidth = LineW;             //把这些变量值传给对话筐线宽属性以即时响应修改
	Sdlg.aLineBlend = LineBlend;
	
	Sdlg.UpdateRun(nTime); //挂起运行对话筐
	
	if(GN::Key1[GK_P])Sdlg.Pop();		   //按 P 键就弹出对话筐
	else if(GN::Key1[GK_C])Sdlg.Cancel();  //按 C 键就退出对话筐
	ValueI += 100;
	ValueF += 50.0f;
}
GN_CALL void AppDisplay3D(unsigned int nTime){}//▲显示3D画面 程序运行每帧都调用它 

GN_CALL void AppRun(unsigned int nTime){}      //▲运行程序后台的处理 程序运行每帧都调用它
}