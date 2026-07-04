#include<GN.h>        //使用GN库必需包含这个头文件

GNg::sIMAGE					 BgImg;				   //背景图像
GNg::sIMAGE					 StartImg;			   //开始按钮图像
GNg::sIMAGE					 DragonflyImg;		   //蜻蜓图像
GNg::sIMAGE					 FnumImg;			   //数字号码图像
GNg::sIMAGE					 GrassLImg;			   //左边草图像
GNg::sIMAGE					 GrassRImg;			   //右边草图像
GNg::sIMAGE					 LeafImg;			   //荷叶图像
GNg::sIMAGE					 ActImg;			   //青蛙动作图序
P<GNg::sIMAGE>				 FgImg;				   //一系列前景图像

GNg::cTEXT_ART				 Fnum;				   //数字文本输出器
GNg::cTEXT_ART				 Fnum2;				   //数字文本输出器2

GNa::cSOUND_SRC				 SoundDown;			   //落地发声点对象
GNa::cSOUND_SRC				 SoundJump;			   //起跳发声点对象
GNa::cSOUND_SRC				 SoundWater;		   //落水发声点对象
GNa::cSOUND_SRC				 SoundOver;			   //结束弹出发声点对象
GNa::cSOUND_SRC				 SoundCombo;		   //连跳奖励发声点对象
P<GNa::sSOUND>				 SoundOs;			   //一系列声音对象
GNa::cBGM_MUSIC				 Bgm;				   //音频流播放器 该对象以流式播放不会加载整个文件到内存

GNg::cFRAME					 Act;				   //青蛙动作动画筐
GNg::cFRAME					 InsectAm;			   //虫图动画筐
GNg::cFRAME_RC				 ComboAm;			   //连跳奖励动画筐
GNg::cFRAME					 Wave;				   //水波动画筐
GNg::cFRAME					 Water;				   //水浪动画筐
GNg::cFRAME					 GameOver;			   //游戏结束动画筐
GNg::cFRAME_LAUNCHER		 Stars4;			   //闪烁星星粒子发射器
GNg::cFRAME_LAUNCHER		 StarsUp;			   //升起星星粒子发射器
float						 FrogSpeed;			   //青蛙跳跃当前速度
float						 FrogY;				   //青蛙当前位置Y
float						 CameraY;			   //当前镜头位置Y
int							 LeafMinShowId;		   //当前最小显示的叶ID
int							 LeafMaxShowNum;	   //叶最大显示个数
int							 LeafStandR;		   //荷叶可站半径
int							 PassNum;			   //通过叶数
int							 LastStatistNum;	   //最近已统计通过叶数
int							 LifeNum;			   //剩余命数
int							 Score;				   //得分
unsigned int				 RebirthTime;		   //重生时间
uint8						 PageState = 0;		   //当前游戏页面状态

int							 LeafMinDist	  = 80;   //荷叶最小间距
int							 LeafMoveRngL	  = 140;  //荷叶移动范围左
int							 LeafMoveRngR	  = 410;  //荷叶移动范围右

struct sLEAF_INFO //荷叶信息
{
	static int y_total; //累计所有创建的位置Y总量

	int		   x;		//位置X
	int		   y;		//位置Y
	int		   s;		//速度

	//创建一个随机荷叶信息(荷叶编号)
	void Create(int id){
		x =  GN_Random_n( LeafMoveRngL, LeafMoveRngR); //在荷叶移动范围左与右之间随机选一个位置X
		y =  y_total - GN_Random_n(LeafMinDist+(id>>2), LeafMinDist*2+(id>>1)); //以荷叶最小距离加上荷叶编号除4 和这个值的两倍之间随机选一个位置Y 然后再退到当前Y总量之上 得到荷叶的Y位置 之后每生成新荷叶都在上次生成荷叶之上
		s = ( GN_Random_n( id>>5, (id>>5)+3) - 1); //以荷叶编号除32 和这个值的再加3之间随机选一个值再减1作为该荷叶的移动速度
		if(s < 1)s = 1;		 //确保速度不为0
		y_total = y;		 //记下当前最上的荷叶位置Y
	}
};
int sLEAF_INFO::y_total;

sLEAF_INFO	     LeafInfoBuf[16];	//荷叶信息缓冲 用于记录当前屏幕看到的和屏外1个预备荷叶信息 16个就够了 因为离开了屏幕的荷叶信息又可以重新安排给将要出现的荷叶用 以此轮循

uint8 Act1Tids[] = { 0,1,2,2,3 };	//青蛙第一套动作所引用到的图ID
uint8 Act2Tids[] = { 4 };			//青蛙第二套动作所引用到的图ID
uint8 Act3Tids[] = { 5,6 };			//青蛙第三套动作所引用到的图ID

uint8 InsectIndex[] = { 2,1,0,0,1,2, 3,4,3,4,3,4,3,4 }; //虫子引用到的图ID

//初始化游戏数据(通用参数)
void GameInit(void *arg=0){
	LeafMinShowId = 0;
	FrogSpeed = 0.0f;
	CameraY = 0.0f;
	PassNum = 0;
	LastStatistNum = 0;
	LifeNum = 6;						  //初始6条命
	Score   = 0;
	RebirthTime = 0;
	LeafStandR = LeafImg.height / 5 * 3;  //判断半径是荷叶高度的5分3
	srand(13);							  //使用固定随机数生成 每次游戏生成的随机位置都是一样的
	sLEAF_INFO::y_total = GNg::View.ortho_bottom;   //荷叶的位置Y从取景范围下开始
	int pi = 0, pn = LeafMaxShowNum;
	for(; pi < pn; ++pi)LeafInfoBuf[pi].Create(pi); //生成当前屏幕有可能看到的前几个荷叶信息
	FrogY = LeafInfoBuf[0].y;			  //青蛙初始位置Y等于首个荷叶置Y
}
//视口重置处理
void ViewReSizeProc(){
	LeafMaxShowNum = GNg::ViewCur->orthoH / LeafMinDist; //用屏幕取景范围高除荷叶最小间距得到叶最大显示个数
	if((short)(LeafMaxShowNum * LeafMinDist) < GNg::ViewCur->orthoH)++LeafMaxShowNum; //还有一个可能将要出现的荷叶
}
//开始按钮处理(显示位置X, 位置X, 颜色, 是否为首界面)
void StartButtonProc(int x, int y, unsigned int c, bool first){
	bool in_rng = GNg::ViewCur->HitRange(x, y, StartImg.width, StartImg.height); //判断鼠标是否落在开始按钮图范围里
	if(in_rng && GN::Mouse0[GM_LEFT]){ //在范围里且鼠标左键松开
		if(first){ //首界面的处理
			PageState = 1;	//标记接下来将要显示正式游戏内容
			BgImg.InitToData("samples/base/jump/bg"); //更换读取另一个背景图片
		}else{     //游戏时的处理
			GN::ValueChange(&GameOver.aFrame_ts.sx, 4.0f, GameInit, 0, 0.5f, 0.07f); //要求让游戏结束动画筐的水平缩放在一定时间里插值到4倍形成放大动画 动画完结后就调用初始化游戏数据
			SoundJump.SetPitch(0, 0.7f);  //起跳声音降调
			SoundJump.Play(0);			  //播放起跳声音
		}
	}else if(in_rng && !GN::Mouse[GM_LEFT]){ //在范围里且鼠标没有按下
		  GNg::ImageDraw(&StartImg, x-3, y-3, StartImg.width+6, StartImg.height+6, c); //放大一点绘制开始按钮
	}else GNg::ImageDraw(&StartImg, x,   y, 0, 0, c); //绘制开始按钮
}
//初始化指定星星粒子发射器(粒子发射器,					粒子动画事件,			事件数,			筐发射间隔时间,              一次的发射筐量)
void StarLauncherInit(GNg::cFRAME_LAUNCHER &lanucher, GNg::sAM_EVENT *evs, unsigned int evn, unsigned int gen_speed, unsigned short gen_once_num){
	GNg::sIMAGE *star_img = new GNg::sIMAGE;	   //分配图片空间
	star_img->InitToData("samples/base/image/star4_", 1, GNg::CR_ALPHA); //星星图用透明格式保存就够了
	
	GNg::cFRAME *star_up = new GNg::cFRAME;
	//为图框装填图象(指定图象,动画事件个数,动画事件数组, 变换信息(x,y),图心点对位x,y,发光混合模式,   颜色,      播放完就消失)
	star_up->Init(   star_img,    evn,         evs,		  GNg::Ts2D,	 0.5f,0.5f,       2,      0xffffffff,GNg::eAM_ONCE_HIDE);
	star_up->KeepExternObjs();	   //保管外部连接对象 调用后 star_img ev_up 等外部对象数据不必手动释放了

	lanucher.InitToData(
		star_up,                                      //装入星星闪现动画筐
		0.0f,                                         //发射角度 修改这个可以向其他方向散发出去
		gen_speed,                                    //发射间隔时间
		gen_once_num,                                 //一次生成数
		0,                                            //位置变化幅度 0随机
		0,                                            //角度变化幅度 0随机
		0,                                            //大小变化幅度 0随机
		127,										  //颜色变化幅度 0随机
		0                                             //该发射器的活动时间 发射超过活动时间后会停止发射 0为无限发射
	);
	lanucher.KeepExternObjs();	//保管外部连接对象 调用后 star_up 等外部对象数据不必手动释放了
}
int		 MaxScore    = 0; //	最大得分

extern "C"{
GN_CALL void AppInit(){      //▲初始化应用必须数据 程序开始准备好引擎系统后会调用它一次
    SoundOs.New(5); //读入声音文件（ 文件路径,  是否转换为定点声音) 定点声音即是转为单声道
	SoundOs[0].InitToData("samples/base/jump/wa.wav",   false);  
	SoundOs[1].InitToData("samples/base/jump/jump.wav", false);
	SoundOs[2].InitToData("samples/base/jump/water.wav",false);
	SoundOs[3].InitToData("samples/base/jump/over.wav", false);
	SoundOs[4].InitToData("samples/base/jump/combo.wav",false);
	//为发声点装入声音(指定声音对象, 是否不断循环发声)
	SoundDown.InitToData (SoundOs,			  false);		   //装入落地声
	SoundJump.InitToData (SoundOs+1,		  false);		   //装入起跳声
	SoundWater.InitToData(SoundOs+2,		  false);		   //装入落水声
	SoundOver.InitToData (SoundOs+3,		  false);		   //装入结束弹出声
	SoundCombo.InitToData(SoundOs+4,		  false);		   //装入连跳奖励声
	Bgm.InitToData("samples/base/jump/bgm.mp3");				   //为音频流指定播放文件(文件路径)

	BgImg.InitToData("samples/base/jump/start_bg");			   //读取初始背景图片
	StartImg.InitToData("samples/base/jump/start");			   //读取开始按钮图片
	DragonflyImg.InitToData("samples/base/jump/dragonfly");	   //读取蜻蜓图片
	FnumImg.InitToData("samples/base/jump/number");			   //读取数字号码图片
	GrassLImg.InitToData ("samples/base/jump/grassL");		   //读取左边草图片
	GrassRImg.InitToData ("samples/base/jump/grassR");		   //读取右边草图片
	LeafImg.InitToData ("samples/base/jump/leaf");			   //读取荷叶图片
	ActImg.InitToData ("samples/base/jump/act", 7);			   //读取青蛙动作图序 共7张图
	FgImg.New(6);  //为前景图分配6个图像空间
	char filename[] = "samples/base/jump/fg0_";
	char *_id = strstr(filename, "0_"); //指向路径里编号的位置
	int i = 0;
	for(; i < 6; ++i){ //循环读取6个图片
		*_id = '0' + i;				    //为每次读取修改路径的编号
		FgImg[i].InitToData(filename);  //读取特定编号的前景图片
	}
	//定义排列格式(	  是否填满文本筐,   文本水平靠位,  文本垂直靠位, 字间距)
	GNg::sARRANGE_FMT fmt2(false,       1.0f/*靠右*/,	  0.0f,       17);
	GNg::sARRANGE_FMT fmt (false,       0.5f/*居中*/,     0.0f);
	Fnum2.Init(&FnumImg, &fmt2);	  //初始化靠右数字输出器
	Fnum .Init(&FnumImg, &fmt);		  //初始化居中数字输出器

	//为图框装填图象(指定图象,动画事件个数,动画事件数组, 变换信息(x,y), 图心点对位x,y)
	Act.Init(		 &ActImg,     0,			0,		   GNg::Ts2D,	0.05f,0.25f);
	
	GNa::sSOUND *insect_so = new GNa::sSOUND;
	insect_so->InitToData("samples/base/sound/hhhh.wav");  //读入虫子笑声音频文件

	GNg::sIMAGE *insect_img = new GNg::sIMAGE;   //分配图片空间
	insect_img->InitToData("samples/base/image/chong",7); //读入虫子动画图组
	
	GNg::sAM_EVENT *ev_insect = new GNg::sAM_EVENT[1];   //虫子动画事件
	ev_insect[0].Init(GNg::eEVENT_SOUND, 750);	 //声音事件可以只设置开始时间

	//InsectAm装填图象(指定图象,动画事件个数, 动画事件数组,  变换信息(x,y),      图心点对位x,y,   混合模式,			颜色,       播放完就停住)
	InsectAm.Init(insect_img,     1,           ev_insect, GNg::sTRANSF2D(85,383), 0.5f,0.5f, GNg::eBLEND_NORMAL, 0xffffffff, GNg::eAM_REPEAT);
	//设置动画索引图数据(有几个索引数据,   索引数组,   每个索引停留的时间,	播放完就停住)
	InsectAm.InitTID(sizeof(InsectIndex), InsectIndex,       120,         GNg::eAM_REPEAT); 
	InsectAm.InitSoundToData(insect_so); //插入声音
	InsectAm.KeepExternObjs();	   //保管外部连接对象 调用后 insect_img ev_insect insect_so 等外部对象数据不必手动释放了 !但这样您需确保一个动画筐引用一个图像和一组事件 不能和其它对象共享
	
	P<GNg::sAM_NODE> am_node(2);   //创建两个动画段落节点
	//设置节点数据 (  节点所在时间,播到该节点以后跳转到那个时间0xffffffff:为在原段内继续播放,该节点段的播放模式 )
 	am_node[0].Init(700,                   0xffffffff,                                       GNg::eAM_REPEAT);
	am_node[1].Init(InsectAm.GetEndtime(), 0,												 GNg::eAM_REPEAT);
	InsectAm.InitSegment(am_node); //插入段落节点 使动画分为两段


	GNg::sIMAGE *gameover_img = new GNg::sIMAGE;   //分配图片空间
	gameover_img->InitToData("samples/base/jump/gameover"); 
	
	GNg::sAM_EVENT *ev_gover = new GNg::sAM_EVENT[1]; //游戏结束筐动画事件
	ev_gover[0].Init(GNg::eEVENT_SIZE_X|GNg::eEVENT_SIZE_Y, 0, 400,   0.2f,     1.0f,     0.72f, GNg::L_DOUBLE); //XY放大又缩小动画

	//为图框装填图象(指定图象,动画事件个数,动画事件数组, 变换信息(x,y),		  图心点对位x,y,    混合模式,		  颜色,       播放完就定住)
	GameOver.Init(gameover_img,     1,       ev_gover, GNg::sTRANSF2D(284,230), 0.5f,0.5f, GNg::eBLEND_NORMAL, 0xffffffff, GNg::eAM_ONCE_KEEP);
	GameOver.KeepExternObjs();	   //保管外部连接对象 调用后 gameover_img ev_gover 等外部对象数据不必手动释放了


	GNg::sIMAGE *wave_img = new GNg::sIMAGE;	   //分配图片空间
	wave_img->InitToData("samples/base/jump/wave");
	
	GNg::sAM_EVENT *ev_wave = new GNg::sAM_EVENT[2]; //水波筐动画事件
	ev_wave[0].Init(GNg::eEVENT_SIZE_X|GNg::eEVENT_SIZE_Y, 0, 2000,   0.45f,   3.0f);	   //XY放大动画
	ev_wave[1].InitColor(							       0, 2000,0xffffffff,0x00ffffff); //渐淡动画

	//为图框装填图象(指定图象,动画事件个数,动画事件数组, 变换信息(x,y),	图心点对位x,y,    混合模式,		   颜色,     播放完就消失)
	Wave.Init(		wave_img,     2,        ev_wave,      GNg::Ts2D,	  0.5f,0.5f, GNg::eBLEND_NORMAL, 0xffffffff, GNg::eAM_ONCE_HIDE);
	Wave.KeepExternObjs();	   //保管外部连接对象 调用后 wave_img ev_wave 等外部对象数据不必手动释放了


	GNg::sIMAGE *water_img = new GNg::sIMAGE;	   //分配图片空间
	water_img->InitToData("samples/base/jump/water",5);
	
	GNg::sAM_EVENT *ev_water = new GNg::sAM_EVENT[1]; //水浪筐动画事件
	ev_water[0].Init(GNg::eEVENT_TEXID, 0, 450,   0,   4);			    //图序变化动画

	//为图框装填图象(指定图象,动画事件个数,动画事件数组, 变换信息(x,y),	图心点对位x,y,    混合模式,		   颜色,     播放完就消失)
	Water.Init(	   water_img,     1,       ev_water,      GNg::Ts2D,	 0.5f,0.5f, GNg::eBLEND_NORMAL, 0xffffffff, GNg::eAM_ONCE_HIDE);
	Water.KeepExternObjs();	   //保管外部连接对象 调用后 water_img ev_water 等外部对象数据不必手动释放了


	GNg::sIMAGE *combo_img = new GNg::sIMAGE;	   //分配图片空间
	combo_img->InitToData("samples/base/image/combo",2);
	
	GNg::sAM_EVENT *ev_combo = new GNg::sAM_EVENT[4]; //连跳奖励动画事件
	ev_combo[0].Init(GNg::eEVENT_MOVE_X, 0,    1800,   0.0f,   -100.0f);  //向左移动动画
	ev_combo[1].Init(GNg::eEVENT_MOVE_X, 1800, 3600,  -100.0f,   0.0f);	  //向右移动动画
	ev_combo[2].Init(GNg::eEVENT_MOVE_Y, 0,    3600,   0.0f,    -80.0f,    0.8f, GNg::L_DOUBLE); //向下上来回移动动画
	ev_combo[3].InitColor(				 0,    3600,0xffffffff,0x00ffffff, 1.0f, GNg::L_END);	 //渐淡动画
	
	//为图框装填图象(指定图象,动画事件个数,动画事件数组, 变换信息(x,y),	  图心点对位到显示数字的位置, 混合模式,	   颜色,     播放完就消失)
	ComboAm.Init(   combo_img,     4,       ev_combo, GNg::sTRANSF2D(480,180), 0.48f,0.59f, GNg::eBLEND_NORMAL, 0xffffffff, GNg::eAM_ONCE_HIDE);
	ComboAm.KeepExternObjs();	   //保管外部连接对象 调用后 combo_img ev_combo 等外部对象数据不必手动释放了
	ComboAm.SetNowtime(3600);

	
	GNg::sAM_EVENT *ev1 =	 new GNg::sAM_EVENT[2]; //闪烁星星闪现动画事件
	ev1[0].Init(GNg::eEVENT_SIZE_X|GNg::eEVENT_SIZE_Y, 0, 1500,   0.2f,     1.0f,     0.8f, GNg::L_DOUBLE); //XY放大又缩小动画
	ev1[1].InitColor(								   0, 1500,0x18ffffff,0x66ffffff, 0.8f, GNg::L_DOUBLE); //高透明->小透明->高透明动画
	//初始化闪烁星星(粒子发射器, 粒子动画事件, 事件数, 筐发射间隔时间, 一次的发射筐量)
	StarLauncherInit(Stars4,		ev1,		 2,			700,			 2);
	GNg::sRECT2 rng4(130.0f,0.0f,354.0f,180.0f);
	Stars4.SetRange(&rng4);     //设置星星的出生范围 用矩形描述
	
	GNg::sAM_EVENT *ev_up = new GNg::sAM_EVENT[4]; //升起星星闪现动画事件
	ev_up[0].Init(GNg::eEVENT_SIZE_X|GNg::eEVENT_SIZE_Y, 0, 9000,  0.2f, 0.1f);			   //XY缩小动画
	ev_up[1].Init(GNg::eEVENT_MOVE_X, 0, 9000,	 0.0f,     130.0f, 0.1f, GNg::L_DOUBLE);   //向右移动动画
	ev_up[2].Init(GNg::eEVENT_MOVE_Y, 0, 9000,	 0.0f,    -300.0f);						   //向上移动动画
	ev_up[3].InitColor(				  0, 9000,0xffffffff,0x00ffffff);					   //渐淡动画
	//初始化升起星星(粒子发射器, 粒子动画事件, 事件数, 筐发射间隔时间, 一次的发射筐量)
	StarLauncherInit(StarsUp,	   ev_up,		 4,			800,			 6);
	GNg::sRECT2 rng1(-20.0f,300.0f,530.0f,480.0f);
	StarsUp.SetRange(&rng1);     //设置星星的出生范围 用矩形描述

    if(GN::GetTouchMaxNum()>1)GN_SetMouseDoubleClick(10000000, 32); //当系统支持触摸屏时 设置触摸按紧映射鼠标双击间隔时间(间隔时间 值大一点相当于禁用双击映射, XY的位置误差)
	GNg::View.Init(0, 0, GNg::View.viewW, GNg::View.viewH, 0, 0, 550, 460); //保持主视口大小并修改2D取景范围为550x460以放大显示来匹配原设计的大小
	ViewReSizeProc();  //视口大小变更后重置一些状态
	GameInit();		   //游戏数据初始化
}
GN_CALL void AppUnInit(){    //▲释放应用必须数据 程序运行结束前会调用它一次
	SoundDown.Release();	 //释放落地发声点对象
	SoundJump.Release();	 //释放起跳发声点对象
	SoundWater.Release();	 //释放落水发声点对象
	SoundOver.Release();	 //释放结束弹出发声点对象
	SoundCombo.Release();	 //释放连跳奖励发声点对象
	SoundOs.Delete();		 //释放所有声音对象 这里要记得要先释放了装有它的发声点对象先再释放它 顺序调乱了可能会有问题
	Bgm.Release();			 //释放音频流对象
	GameOver.Release();		 //释放游戏结束动画筐空间
	Wave.Release();			 //释放水波动画筐空间
	Water.Release();		 //释放水浪动画筐空间
	Act.Release();			 //释放青蛙动作动画筐空间
	InsectAm.Release();		 //释放虫子动画筐空间
	ComboAm.Release();		 //释放连跳奖励动画筐空间
	Stars4.Release();		 //释放闪烁星星发射器空间
	StarsUp.Release();		 //释放升起星星发射器空间
	StartImg.Release();		 //释放开始按钮图片空间
	DragonflyImg.Release();	 //释放蜻蜓图片空间
	Fnum.Release();			 //释放数字文本输出器空间
	Fnum2.Release();		 //释放数字文本输出器2空间
	FnumImg.Release();		 //释放数字号码图片空间
	GrassLImg.Release();	 //释放左边草图片空间
	GrassRImg.Release();	 //释放右边草图片空间
	LeafImg.Release();		 //释放荷叶图片空间
	ActImg.Release();		 //释放青蛙动作图序空间
	FgImg.Delete();			 //删除前景相关图片空间
	BgImg.Release();		 //释放背景图片空间
}
GN_CALL void AppDisplay2D(unsigned int nTime){ //▲显示2D画面 程序运行每帧都调用它
	Bgm.UpdataPlay(nTime);			//更新播放背景音乐
	GNg::ImageDraw(&BgImg, 0, 0);	//绘制背景
	if(PageState == 1){				//已进入游戏
		float scrool_ofs = CameraY / (float)GrassLImg.height * 0.22f; //按镜头位置Y 除左边草图高来得到纹理内的偏移量 再乘0.22使图滚动得慢一点以产生伪透视效果 !将该值传给纹理截取UV即可实现控制图内滚动
		//绘制左右边草图(图像 显示坐标X,坐标Y,   显示宽度,	   显示高度,        图的底色, 图序ID,   混合模式, 截取纹理左上角的U坐标, 左上角的V坐标,   右下角的U坐标,      右下角的V坐标)
		GNg::ImageDraw(&GrassLImg,   0,  0, GrassLImg.width, GrassLImg.height, 0xffffffff,  0, GNg::eBLEND_NORMAL,   0.0f,			scrool_ofs,		GrassLImg.uRange,  GrassLImg.vRange+scrool_ofs);
		GNg::ImageDraw(&GrassRImg, 420,  0, GrassRImg.width, GrassRImg.height, 0xffffffff,  0, GNg::eBLEND_NORMAL,   0.0f,			scrool_ofs,		GrassRImg.uRange,  GrassRImg.vRange+scrool_ofs);

		Wave.PlayFrame(nTime); //更新播放水波动画筐 当动画是播放一次后消失了 调用会立即返回不会多消算力

		static float downOfs = 0.0f;
		int v_top = GNg::ViewCur->ortho_top, v_bottom = GNg::ViewCur->ortho_bottom; //临时获得视口取景范围上和下
		int pi = LeafMinShowId, pn = LeafMinShowId + LeafMaxShowNum, i, dy, cy = (int)CameraY; //从最下边一个开始 到之后可能被看见的荷叶
		for(; pi < pn; ++pi){ //循环历遍可能看得见的荷叶 
			i = pi % LeafMaxShowNum;					   //把实际荷叶ID 映射为在缓冲里的ID
			dy = LeafInfoBuf[i].y - cy;					   //索取到ID对应荷叶的当前位置Y 在减去镜头位置Y 得到视口位置Y
			if(dy > v_bottom){							   //视口位置Y大于取景范围下时 表示这个荷叶已经过期
				LeafInfoBuf[i].Create(pi);				   //用它的缓冲空间创建一个新的荷叶信息位置 使缓冲循环利用
				++LeafMinShowId;						   //使当前最小显示荷叶ID递增1
				continue;								   //跳转处理下个荷叶
			}else if(dy + LeafImg.height < v_top)continue; //荷叶底部小于取景范围上时 表示这个荷叶还没在视口中出现就跳转处理下个荷叶

			LeafInfoBuf[i].x += LeafInfoBuf[i].s;		   //使荷叶以它的速度水平移动一下 
			if(LeafInfoBuf[i].x <= LeafMoveRngL || LeafInfoBuf[i].x >= LeafMoveRngR)LeafInfoBuf[i].s = -LeafInfoBuf[i].s; //当荷叶移到左边或右边草时改变一下它的速度正负 使得移到边处就能反向移动来回往返

			GNg::ImageDraw(&LeafImg, LeafInfoBuf[i].x, dy+5, 0, 0, 0xaa500077); //以半透明深蓝色绘制荷叶作为它的阴影
			GNg::ImageDraw(&LeafImg, LeafInfoBuf[i].x, pi == PassNum/*如是当前青蛙所在荷叶*/ ? dy+(int)downOfs/*使它位置Y偏移一下来模拟青蛙下落时的压感*/ : dy); //绘制荷叶
		}
		Water.PlayFrame(nTime); //更新播放水浪动画筐
		
		static float genOnceNum;
		if(GN::TimeGetRunCount() >= RebirthTime){ //程序当前运行时间到了青蛙重生时间
			i = PassNum % LeafMaxShowNum; //把青蛙所在实际荷叶ID 映射为在缓冲里的ID
			if(Act.GetEndtime() < 240){   //青蛙当前为落水动画 落水动画时间短不会超过240毫秒
				genOnceNum = 0.0f;		  //使升起星星粒子发射器每次生成粒子数为0 可起到停止生成的功效
				--LifeNum;				  //减少一个生命数
				if(LifeNum >= 0){		  //还有命
					//初始化加入索引动画（索引数据个数,   索引数组, 单格的停留时间, 播放模式)
					Act.InitTID(	  sizeof(Act1Tids),	  Act1Tids,	    120,	 GNg::eAM_LOOP); //设置青蛙动作为待机
					Act.aFrame_ts.x = (float)LeafInfoBuf[i].x;			//使青蛙位置对齐荷叶
					FrogY			= (float)LeafInfoBuf[i].y - 15.0f;  //青蛙位置Y在荷叶往上15个像素位置出现 !在游戏中 Act.aFrame_ts.y 存的是青蛙视口位置Y
					SoundDown.Play(0);   //播放落地声音
				}else{  //没命了
					SoundOver.Play(0);   //播放结束弹出声音
					GameOver.ReSetPlaytime();	   //通知重新播放游戏结束动画筐
					GameOver.aFrame_ts.sx = 1.0f;  //还原动画筐水平无缩放
					GameOver.aFrame_ts.sy = 1.0f;  //还原动画筐垂直无缩放
					RebirthTime = 0xffffffff;	   //重生时间拉到最大 使程序当前运行时间总不能经过这里
				}
			}else{	  //青蛙刚重生后
				if((int)FrogY < LeafInfoBuf[i].y)FrogY += 2.25f; //当青蛙位置还比荷叶高时使它再下落一点
				else{ //青蛙重生并落地了
						FrogY = LeafInfoBuf[i].y;  //使青蛙位置Y对齐荷叶
						downOfs = 5.0f;			   //使荷叶绘制Y向下偏移5个像素以模拟压感
						RebirthTime = 0xffffffff;  //重生时间拉到最大 使程序当前运行时间总不能经过这里
				} 
			}
		}
		if(FrogSpeed){ //青蛙有速度
			FrogY	  -= FrogSpeed;   //使青蛙以它的速度往上移动一下
			FrogSpeed -= 0.28f;		  //速度递减一些
			if(FrogSpeed < -3.0f){    //速度小于-3表示青蛙下落完成
				FrogSpeed = 0.0f;	  //清除速度使青蛙停下来
				Wave.ReSetPlaytime(); //通知重新播放水波动画筐
				Wave.aFrame_ts.x = Act.aFrame_ts.x + LeafImg.width  / 2; //水波的位置X对齐荷叶中心 !当青蛙站在荷叶中时 青蛙图心点是在荷叶图左上角的
				Wave.aFrame_ts.y = Act.aFrame_ts.y + LeafImg.height / 2; //水波的位置Y对齐荷叶中心
				int last_pass_num = PassNum;  //临时记录之前的青蛙通过叶数
				for(PassNum = LeafMinShowId; (int)FrogY < LeafInfoBuf[PassNum % LeafMaxShowNum].y - LeafStandR; ++PassNum); //从最下边一个荷叶开始续个往上历遍 初步判断青蛙不会冲过那个荷叶
				i = PassNum % LeafMaxShowNum; //把当前没冲过的荷叶实际ID 映射为在缓冲里的ID
				int crs_y = (int)FrogY - LeafInfoBuf[i].y, crs_x = (int)Act.aFrame_ts.x - LeafInfoBuf[i].x; //计算出当前青蛙与这个荷叶的水平和垂直位置差距

				if(crs_y > LeafStandR || crs_x > LeafStandR || crs_x < -LeafStandR){ //差距超出荷叶可站半径表明青蛙没落在荷叶里
					RebirthTime = GN::TimeGetRunCount() + 1000; //设置它的重生时间为当前程序运行时间往后1秒
					LastStatistNum = PassNum;				//记录一下当前通过叶数 以便重新计算最大连跳数
					Water.ReSetPlaytime();					//通知重新播放水浪动画筐
					Water.aFrame_ts.x = Wave.aFrame_ts.x;	//水浪的位置X对齐水波的位置X
					Water.aFrame_ts.y = Wave.aFrame_ts.y;	//水浪的位置Y对齐水波的位置Y
					InsectAm.SeekSegment(1);				//左下边虫子跳转去播放第二段动画
					Act.SetTimeControl(); //用这个来重设时间可以连动画结束时间一起清掉 以便重新统计新动画的结束时间
					Act.InitTID(sizeof(Act3Tids), Act3Tids, 50, GNg::eAM_ONCE_HIDE); //设置青蛙动作为落水
					SoundWater.Play(0);   //播放落水声音
				}else{ //青蛙落在荷叶上
					if(PassNum > last_pass_num){ //落在荷叶不是之前那个
						if(genOnceNum < 6.0f)genOnceNum += 1.0f; //升起星星粒子每次生成粒子数小于6时使它增加1 使得升起星星更多
						else genOnceNum = 6.0f;					 //每次生成粒子数最大不能大于6
						static unsigned int lastPassTime = 0;
						if(PassNum - LastStatistNum > 1){	//当前通过叶数比上次统计数大于1时
							if(GN::TimeGetRunCount() - lastPassTime < 3200){ //当前程序运行时间没超过上次落地时间3秒多时
								ComboAm.ReSetPlaytime(); //通知重新播放连跳奖励动画筐
								SoundCombo.Play(0);		 //播放连跳奖励声音
							}else LastStatistNum = PassNum - 1;  //当超时时 记录一下当前通过叶数减1 以便重新计算最大连跳数
						}
						lastPassTime = GN::TimeGetRunCount();	 //记录一下落地时间
						int base_score = (crs_y>crs_x?crs_y:crs_x) < LeafImg.width/6 ? 10 : 5;  //按落在离叶中差距来确定本次基础得分 更接近叶中心的获10分 其它以外范围获得5分
						Score += (PassNum - last_pass_num/*本次跳跃越过叶数*/) * (PassNum - LastStatistNum/*当前连跳数*/) * base_score; //计算得分
						if(Score > MaxScore)MaxScore = Score;	 //大于当前最大分数时就刷新它
					}
					Act.InitTID(sizeof(Act1Tids), Act1Tids,	120, GNg::eAM_LOOP);  //设置青蛙动作为待机
					downOfs = 5.0f;		 //使荷叶绘制Y向下偏移5个像素以模拟压感
					SoundDown.Play(0);   //播放落地声音
				}
			}
		}else if(Act.GetEndtime() > 240){ //青蛙没有速度且动画结束时间大于240表明是待机动画
			static unsigned int mouse1_time = 0xffffffff;
			if(GN::GetTouchMaxNum()>1 ? GN::TouchZ[0]==255 : GN::Mouse1[GM_LEFT])mouse1_time = GN::TimeGetRunCount(); //当是触屏 记录首个触点刚按下时间 否则 记录鼠标左键刚按下时间   !因为触屏设备映射的鼠标左键按下消息会有一点涉后
			else if(GN::Mouse1[GM_RIGHT])mouse1_time = 0xffffffff;		//当按下鼠标右键取消这个时间 也就取消了青蛙跳的操作
			else if(GN::Mouse0[GM_LEFT] && mouse1_time != 0xffffffff){  //当鼠标左键刚松开且按下时间还有效时
				FrogSpeed = sqrtf((float)(GN::TimeGetRunCount() - mouse1_time) * 0.001f) * 15.0f; //以按下时长来确定青蛙跳速度 按住越久冲力越大
				if(FrogSpeed > 18.0f)FrogSpeed = 18.0f;  //最大速度不能超过18
				mouse1_time = 0xffffffff;				 //取消这个时间 
				Act.InitTID(sizeof(Act2Tids), Act2Tids,	120, GNg::eAM_ONCE_KEEP); //设置青蛙动作为跳跃
				SoundJump.SetPitch(0, 1.25f - FrogSpeed / 18.0f); //按青蛙速度来调整起跳声音音调
				SoundJump.Play(0);   //播放起跳声音
			}
			Act.aFrame_ts.x += ((float)LeafInfoBuf[PassNum % LeafMaxShowNum].x - Act.aFrame_ts.x) * 0.5f; //使青蛙的位置X带点涉后缓动地跟随荷叶移动 使看起来没那么生硬
		}
		Act.aFrame_ts.y = FrogY - CameraY;    //青蛙位置Y减去镜头位置Y即可得到视口位置Y
		Act.PlayFrame(nTime);				  //更新播放青蛙动画筐
		float cma_move = (FrogY - 350.0f/*青蛙待机静止时离视口顶部高度*/ - CameraY) * 0.02f; //计算镜头要追随青蛙时的目标移动量的一点
		CameraY += cma_move;  //推一点移动量 使镜头追随涉后一点产生缓动感 这样更能显出青蛙起跳那股冲力 而不是死死锁定青蛙在固定位置上
		Wave.aFrame_ts.y -= cma_move;		  //水波位置Y以反方向拖动一下 !这样就能在纯2D模式下模拟摄像机拖动
		Water.aFrame_ts.y = Wave.aFrame_ts.y; //水浪位置Y以反方向拖动一下
		downOfs *= 0.925f;					  //使被压下一点的荷叶Y偏移慢慢恢复到0

		GNg::sIMAGE *_fg_img = FgImg;				  //临时从安全指针里取出原指针 这样在循环处理里性能更优
		int fgxs[] = {   0, 426, 346, 0, 430, 351 };  //各个前景图的位置X
		int fgys[] = { 343, 323,   0, 0, 223, 400 };  //各个前景图的位置Y
		for(i = 0; i < 6; ++i)GNg::ImageDraw(_fg_img + i, fgxs[i], fgys[i]); //绘制各个前景图

		for(i = 0; i < LifeNum; ++i)GNg::ImageDraw(&DragonflyImg, 491 - i * 35, 396); //显示命数个蜻蜓从右到左 每个间距为35像素
		InsectAm.PlayFrame(nTime);					  //更新播放虫子动画筐

		//更新播放连跳奖励动画筐(当青蛙跌落水时以3倍速度播放否则原速度播放)
		if(ComboAm.PlayFrame(Act.GetEndtime() < 240?nTime*3:nTime)){ //当连跳奖励动画正在播放时
			ComboAm.SetTextureID((GN::TimeGetRunFpsCount()&4)?1:0);  //按程序运行帧数 每4帧来轮替图序ID 使蜜蜂翅膀扇起来
			Fnum.TextOutNumber(PassNum - LastStatistNum, (int)ComboAm.aLevel_ts.x ,  (int)ComboAm.aLevel_ts.y,  0,  0,  ComboAm.aLevel_color); //跟随连跳奖励动画位置和颜色来显示连跳数
		}
		unsigned int max_score_cc = 0xffff00b0;
		Fnum .TextOutNumber(MaxScore, 277 ,   14,      0,         0,      max_score_cc); //显示最高得分
		Fnum .TextOutNumber(MaxScore, 275 ,   13,      0,         0,      0xffffffff); //显示最高得分
		Fnum .TextOutNumber(MaxScore, 274 ,   13,      0,         0,      0xffffffff); //显示最高得分
		
		unsigned int life_num = LifeNum > 0 ? LifeNum : 0; //使生命显示数不能为负
		//输出数字(				数字, 坐标x, 坐标y, 文本筐宽,  文本筐高,    颜色)
		Fnum .TextOutNumber(life_num, 462 ,  294,      0,         0,      0xffd9d0c3); //显示生命数
		Fnum .TextOutNumber(Score,	  487 ,   36,      0,         0,      0xffd9d0c3); //显示得分
		Fnum2.TextOutNumber(PassNum,  125,   197,      0,         0,      0xff000000); //显示当前通过荷叶数

		if(genOnceNum > 0.5f)genOnceNum -= 0.005f * GN::TimeRatio; //使粒子一次生成数每帧减少一些
		StarsUp.SetGenerateOnceNum((unsigned short)genOnceNum);	   //设置升起星星粒子发射器一次生成量
		StarsUp.PlayFrame(nTime); //播放升起星星粒子动画 以烘托氛围

		if(LifeNum < 0){	//当生命数小于0时
			GameOver.PlayFrame(nTime);  //更新播放游戏结束动画筐
			if(GameOver.aFrame_ts.sx > 1.0f)GameOver.aFrame_ts.sy = GameOver.aFrame_ts.sx; //当动画筐正在放大时 使它的垂直比例和水平比例一致
			else{ //正在弹出时 即播放自身的动画
				unsigned int alpha = 255 * GameOver.GetNowtime() / GameOver.GetEndtime(); //计算出以当前动画进度来代表的不透明度
				if(alpha > 255)alpha = 255;								//使不透明度不能超出最大值
				StartButtonProc(233, 240,(alpha<<24)|0x00ffffff/*把不透明度推到它所属位置并与白色颜色值结合*/,false); //按这个不透明度来显示开始游戏标题并处理它的点击
			}
		}
	}else{
		StartButtonProc(239, 182, 0xffffffff, true); //显示开始游戏标题并处理它的点击
		Stars4.PlayFrame(nTime); //播放星星闪现动画
	}
}
GN_CALL void AppDisplay3D(unsigned int nTime){} //▲显示3D画面 程序运行每帧都调用它

GN_CALL void AppRun(unsigned int nTime){}       //▲运行程序后台的处理 程序运行每帧都调用它
}