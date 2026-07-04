#include<GN.h>

/***************************************** 以下是"文本输入"复制过来的例子代码 *****************************************************/

GNg::sIMAGE			BgImg;		 //背景图
GNg::sIMAGE			SquareImg;	 //9宫格图		
GNg::sIMAGE			Gb2312Img;   //字库图象对象
GNg::cTEXT_ART		Gb2312Text;  //文本式样对象
GNu::cTEXT_INPUT    TextIn;      //文本输入对象
 
void TextinInit(){
	BgImg.InitToData("samples/base/image/bg");		    //读入背景图
	SquareImg.InitToData(GN_SysDir("ui/bubbleB"),9, GNg::CR_ALPHA); //读入9宫格图 包含9个图序

	// FreeType UTF-8 中文 (替代 GB2312 PNG 字库)
	Gb2312Text.Init(nullptr);
	Gb2312Text.SetFreeTypeFont(16);

	//初始终化文本输出式样(字库图)
	// Gb2312Text.Init(&Gb2312Img);  // 由 FreeType 替代

	//初始化文本输入式样(文体式样, 文本筐X坐标, 文本筐Y坐标, 文本筐宽, 文本筐高, 文本颜色,   焦点颜色,   文本混合模式, 焦点混合模式)
	TextIn.InitStyle( &Gb2312Text,    220,         200,        360,       300 ,  0xff5566aa, 0xff771133);

	//初始化文本输入格式(        输入形式,      筐内开始预设的文本内容,  输入筐最大输入数)
	TextIn.InitInputFmtToData(GNu::eTX_IN_TEXT,      0               /*, ...*/);

	//在当前焦点位置输入文本(文本内容, 字节数:0为全部输入)
	TextIn.InputText("Please input your text!");   
}
void TextinUnInit(){
	TextIn.Release();     //释放文本筐的内容
	Gb2312Text.Release(); //释放文本式样内容
    Gb2312Img.Release();  //释放字库图内容
	SquareImg.Release();  //释放9宫格图
	BgImg.Release();	  //释放背景图
}
void TextinDisplay2D(unsigned int nTime){
	GNg::ImageDraw(&BgImg, 0, 0, 800, 600, 0xffffffff);  //绘制背景以清屏
	
	short ix,iy,iw,ih; //获取文本筐位置(返回坐标X, 返回坐标Y, 返回宽, 返回高)
	TextIn.GetPos(						  &ix,       &iy,      &iw,    &ih);	
	
	GNg::ImageDraw9Square(&SquareImg, ix-24, iy-24, iw+48, ih+48, 0x66ffffff); //绘制一个比文本筐稍大的圆角矩形
	GNg::LineFrame2dDraw(ix, iy, iw, ih, 1, 0x60ffffff);  //绘制文本边筐
	
	TextIn.UpdateRun(nTime);  //挂起更新运行文本输入筐
}
/********************************************** 以下是"弹出对话筐"复制过来的例子代码 ************************************************/

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
GNg::sIMAGE		 BgImg2;	//底纹用的图片
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

GN_CALL void PopDlgInit(){     //▲初始化应用必须数据 程序开始准备好引擎系统后会调用它一次 
	KeySo.InitToData("samples/base/sound/sel.wav");				  //读入声音文件（文件路径）
	BgImg2.InitToData("samples/base/image/s40", 1, GNg::CR_LUMINANCE);  //读入图片并用灰度单色格式保存
	AsciiImg.InitToData("samples/base/image/ascii_system2");

	//设置动画筐(指定所用图片, 帧事件数, 帧事件数组, 坐标变换结构, 缩放定点在左上角, 缩放定点在左上角, 混合模式,		 颜色,     播放模式,       水平分格,  垂直分格)
	Bg.Init(      &BgImg2,       0,         0,         GNg::Ts2D,      0.0f,         0.0f,			GNg::eBLEND_NORMAL, 0xff485b6a, GNg::eAM_LOOP,    -7,        -6);   
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
GN_CALL void PopDlgUnInit(){   //▲释放应用必须数据 程序运行结束前会调用它一次 
	AsciiImg.Release();		//释放字图
	BgImg2.Release();		//释放图片
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
GN_CALL void PopDlgDisplay2D(unsigned int nTime){ //▲显示2D画面 程序运行每帧都调用它 
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
/*###################################################### 以下才是本例子的关键代码 ########################################################*/

GNg::cVIEW2D     SubView;	//定义一个子视口
GNg::sIMAGE		 ViewImg;	//定义一个图象对象 用于保存截图
GNg::sIMAGE		 RenderImg;	//定义一个图象对象 用于动态渲染内容到这上面

extern "C"{
GN_CALL void AppInit(){     //▲初始化应用必须数据 程序开始准备好引擎系统后会调用它一次 
	//初始化子视口
	SubView.Init(      
		         10,   //左上角坐标X
				 10,   //左上角坐标Y
				 400,  //视口宽度
				 300,  //视口高度
				 0,    //视口里显示内容的取景范围左
				 0,    //视口里显示内容的取景范围上
				 800,  //视口里显示内容的取景范围右 !注意这里为什么不是400 因为原例子用到800 这里想显示全部范围得是800 如果为400 它将截取到400的范围无缩放地渲染到子视口上
				 600   //视口里显示内容的取景范围下
				 );

	RenderImg.CreateEmptyToData(400, 300, 1, GNg::CR_RGB); //初始化空的图象为400宽300高大小 RGB格式

    TextinInit();    //加载文本输入的例子内容
    PopDlgInit();    //加载弹出对话筐的例子内容
}
GN_CALL void AppUnInit(){   //▲释放应用必须数据 程序运行结束前会调用它一次 
	PopDlgUnInit();			//释放弹出对话筐的例子内容
	TextinUnInit();		    //释放文本输入的例子内容
	ViewImg.Release();		//释放图象对象
	RenderImg.Release();    //释放动态渲染图
}
GN_CALL void AppDisplay2D(unsigned int nTime){ //▲显示2D画面 程序运行每帧都调用它 
	TextinDisplay2D(nTime);   //在默认视口显示文本输入例子的内容
	
	if(GN::Key[GK_F8]){ //当按紧键 F8 时使用渲染到图象方式

		if(GNg::sIMAGE::RenderTargetSupport()){ //检测硬件是否支持渲染到图象

		    RenderImg.BeginRender(); //设置之后渲染的内容到该图象上

             PopDlgDisplay2D(nTime);   //在这里显示弹出对话筐例子的内容

		    RenderImg.EndRender();   //结束渲染内容到该图象 使渲染恢复为主窗口渲染
        
		    //在屏幕上绘制刚才渲染的图象( 图象,        位置X ,		 位置Y)
            GNg::ImageDraw(         &RenderImg, GNu::NearLx(20), GNu::NearTy(20));
			
		    //显示一些文字来区分当前是渲染到图象( 输出文本的内容,              字数,  位置X , 位置Y)
		      GN::RunRec.OutText                ( "Render to image...",          0,    30,     30);
		}else GN::RunRec.OutText                ( "Render to image not support", 0,    30,     30);  //输出硬件不支持渲染到图像

	}else{ //没按紧键 F8 时使用渲染到子视图方式 子视图能自动映射鼠标与触摸输入
        SubView.BeginRender2D();    //架设视口为2D渲染模式 此后的渲染绘制命令都显示到该视口区内 直到调用EndRender2Dand3D()为止

	     PopDlgDisplay2D(nTime);    //在这里显示弹出对话筐例子的内容
	
        SubView.EndRender2Dand3D(); //结束该视口2D或3D渲染模式 使渲染恢复为主窗口渲染
	}
	//按下F3或F4键      保存主视口内容到文件( 文件名,    截取相对视口里的坐标X,   Y,  宽 0为子视口大小, 高 0为子视口大小)
	if(GN::Key1[GK_F3])GNg::View.SaveToFile("Screen_save.png",               0,   0,  0,				 0);
	if(GN::Key1[GK_F4])GNg::View.SaveToFile("Screen_save2.png",              100, 5,  300,				200);

    //当按下空格键并有把视口内容保存到图象时就显示图象内容到(截屏图象, 靠住窗口左边显示, 靠住窗口上边显示)
	if(GN::Key[GK_SPACE] && ViewImg.texture)GNg::ImageDraw(&ViewImg,GNu::NearLx(),GNu::NearTy());

	//当按下F5             保存主视口内容到图象( 图象,   截取相对视口里的坐标X,   Y,  宽 0为子视口大小,  高 0为子视口大小)
	if(GN::Key1[GK_F5])GNg::View.SaveToImageData(&ViewImg,                   0,   0,  0,				 0);

    //当按下F6             保存子视口内容到图象( 图象,   获取相对视口里的坐标X,   Y,  宽,  高)
	if(GN::Key1[GK_F6])SubView.SaveToImageData(&ViewImg,                     100, 5,  300, 200);
}
GN_CALL void AppDisplay3D(unsigned int nTime){}//▲显示3D画面 程序运行每帧都调用它 

GN_CALL void AppRun(unsigned int nTime){}      //▲运行程序后台的处理 程序运行每帧都调用它
}