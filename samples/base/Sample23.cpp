#include<GN.h>		   //使用GN库必需包含这个头文件
GNa::cSOUND_SRC				 SoundSelect;		//按键选择发声点对象
GNa::cSOUND_SRC				 SoundGameOver;		//游戏结束发声点对象
GNa::cSOUND_SRC				 SoundDamage;		//南瓜受损发声点对象
GNa::cSOUND_SRC				 SoundBurst;		//大招爆发声点对象
GNa::cSOUND_SRC				 SoundDestroy;		//火灭发声点对象
P<GNa::sSOUND>				 SoundOs;			//一系列声音对象
GNa::cBGM_MUSIC				 Bgm;				//音频流播放器 该对象以流式播放不会加载整个文件到内存

P<GNu::cUI_SCENE>			 SceneUi;		    //Ui场景
GNu::cUI_PTR				 OptionMenu;		//游戏设置菜单在UI场景里的地址 有地址您就可控制它的层级关系和移除插入操作等
P<GNu::cSELECT_LIST_TEXT>	 Ranking;			//排名选择列表

P<GNg::cSCENE2D>			 Scene2D;			//2D场景
GNg::cOBJPTR				 MainMenu;			//游戏主界面在场景里的地址 有地址您就可控制它的层级关系和移除插入操作等
GNg::cFRAME_RC			    *MainBg;			//游戏主界面背景	不需要拿出来的2D对象 控制它用裸指针更快
GNg::cFRAME_LAUNCHER_FONT	*MainButton;		//游戏主界面选项按钮
GNg::cFRAME_LAUNCHER_FONT   *Mission;			//Mission 动画艺术字筐 不需要拿出来的2D对象 控制它用裸指针更快
GNg::cFRAME_LAUNCHER		*GameOverEf;		//游戏结束爆炸效果
GNg::sIMAGE_OBJ				*ImgLife;			//显示剩余命数的图标
GNg::sIMAGE_OBJ				*PumpkinBack;		//南瓜眼后的火图序
GNg::sIMAGE_OBJ				*PumpkinFront;		//南瓜眼前亮起的火图序
GNg::cFRAME_RC			    *Head1;				//骨头动画筐1
GNg::cFRAME_RC			    *Head2;				//骨头动画筐2
GNg::cOBJ2D					 TextRectTime;		//时间文本图筐 !从场景里拆出的对象必须用 GNg::cOBJ2D 类型来保管才可留存
GNg::cOBJ2D					 TextRectLevel;		//等级文本图筐
GNg::cOBJ2D					 Fire;				//火动画筐
GNg::cOBJ2D					 Destroy;			//火灭动画筐
GNg::cFRAME_LAUNCHER		*Water;				//喷水发射器
GNg::cOBJPTR				 Stage;				//游戏舞台在场景里的地址
GNg::cOBJ2D					 Stage2d;			//游戏舞台

GN::sLIST<GNg::cOBJ2D>		*FireList;			//游戏运行中动态生成的火挂接链表
GNg::cTEXT_ART				 NumberOut;			//数字输出器
float						 DamageLevel;		//南瓜损坏等级
unsigned int				 ChangeLevel;		//南瓜前后火图序切换的损坏等级
float						 BurstHitDist;		//大招爆发碰撞检测距离
int							 FireR;				//火的碰撞半径

//下面这几个值是给游戏设置菜单调节的
unsigned int				 FastFireColor  = 0xffff6666;	//快速火颜色
unsigned int				 BurstColor	    = 0xffffffff;	//大招颜色
int							 BurstMaxNum    = 3;			//起始大招爆发数
int							 LevelUpTime    = 180;			//难度等级晋升时间
int							 ShowFireNum = 0;				//是否显示目前火数

//插入分数排名从大到小排列(排名选择列表, 分数转字符串的打印格式, 大分数, 小分数)
void InsterRankingFromMaxToMin(GNu::cSELECT_LIST_TEXT *ranking, const char *pfmt, unsigned int maxv, unsigned int minv){
	P< P<char> > ranking_strs = ranking->GetLables();  //获得排名列表内容
	P<char> *_ranking_strs = ranking_strs;			   //拿出裸指针 已使访问更快
	unsigned int ri = 0, ri2, rn = ranking_strs.Num(), level1, time1;
	for(; ri < rn; ++ri){
		sscanf(_ranking_strs[ri], "%d %d", &level1, &time1);  //历遍各个排名 并读出排名字符串对应的整数值
		if(maxv > level1 || (maxv == level1 && minv > time1))break; //与当前游戏得分作比较 找出当前得分对应的排名位置:ri
	}
	if(rn > 29)rn = 29;						 //如已有排名大于29 就忽略掉29之后的排名项
	P< P<char> > ranking2_strs(rn + 1);		 //构建比原排名列表多一个排位的排名列表
	P<char> *_ranking2_strs = ranking2_strs; //拿出裸指针 已使访问更快
	for(ri2 = 0; ri2 < ri; ++ri2)_ranking2_strs[ri2] = _ranking_strs[ri2];   //复制排名插入位置前段的各个排名
	_ranking2_strs[ri2].New(24);  //分配新排名字符串空间
	sprintf(_ranking2_strs[ri2], pfmt, maxv, minv);			 //打印当前游戏得分成排名字符串
	for(++ri2; ri < rn; ++ri, ++ri2)_ranking2_strs[ri2] = _ranking_strs[ri]; //复制排名插入位置后段的各个排名
	ranking->SetLables(ranking2_strs);		 //把新排名列表内容指定给排名列表
}
static void HideRanking(void *usarg){ ((GNu::cSELECT_LIST_TEXT*)usarg)->OkHide(); } //隐藏排名列表

//获得指定火到鼠标位置的距离(火)
inline float GetFireToMouseDist(GNg::cFRAME_RC *fire){
	GNg::sVEC2 v = ((GNg::sVEC2*)&(fire->aFrame_ts.x))[0] - GNg::sVEC2((float)GNg::ViewCur->mouseMX, (float)GNg::ViewCur->mouseMY);
	return v.Length();  //返回 鼠标位置到火的一段方向线的长度 这就是距离
}
extern "C"{
GN_CALL void AppInit(){     //▲初始化应用必须数据 程序开始准备好引擎系统后会调用它一次 
	SoundOs.New(4); //读入声音文件（ 文件路径,  是否转换为定点声音) 定点声音即是转为单声道
	SoundOs[0].InitToData("samples/base/halloween/destroy.wav",  false);
	SoundOs[1].InitToData("samples/base/halloween/burst.mp3",  false);
	SoundOs[2].InitToData("samples/base/halloween/damage.wav",  false);
	SoundOs[3].InitToData("samples/base/halloween/gameover.wav",  false);
	//为发声点装入声音(指定声音对象, 是否不断循环发声)
	SoundDestroy.InitToData (SoundOs  ,		false);	   //装入火灭声
	SoundBurst.InitToData (SoundOs+1,		false);	   //装入大招爆发声
	SoundDamage.InitToData(SoundOs+2,		false);	   //装入受损声
	SoundGameOver.InitToData(SoundOs+3,		false);	   //装入游戏结束声
	SoundSelect.InitToData(SoundOs,		false);		   //装入按键选择声与火灭声共用
	Bgm.InitToData("samples/base/halloween/bg.mp3");	   //为音频流指定播放文件(文件路径)
	Bgm.SetPitch(0.3f);								   //设置背景音乐播放音调要低些

	float load_progr;
	SceneUi.New(1);									   //为UI场景分配空间
	SceneUi->InitLoadToData("samples/base/halloween/Halloween2D.gui", load_progr); //读取UI场景文件
	Ranking = *(SceneUi->Search("TextSelectList#00001").aPtr);  //在UI场景里查找名为 TextSelectList#00001 的排名列表
	OptionMenu = SceneUi->Search("SimDialog#00001");			//在UI场景里查找名为 SimDialog#00001 的游戏设置菜单UI组件地址
	GNu::cUI_SIM_DIALOG *_menu = CUP(GNu::cUI_SIM_DIALOG,OptionMenu); //拿出它额裸指针以便访问得更快

	_menu->BeginInitToData("samples/base/halloween/game_setting");			//在绑定修改值前为设置菜单指定数值保存文件 它会自动维护该文件的数据读写保存
	_menu->InitValueToData(&FastFireColor,	" Fast fire color", 255, 255);			//为设置菜单绑定颜色修改值 - 半透明不可调节
	_menu->InitValueToData(&BurstColor,		" Burst color", 255, 0);				//为设置菜单绑定颜色修改值 - 半透明  可调节
	_menu->InitTextToData (" ");													//插入一个空行
	_menu->InitValueToData(&BurstMaxNum,	" Burst max num", 10, 0, 1);			//为设置菜单绑定修改值 - 起始大招爆发数
	_menu->InitValueToData(&LevelUpTime,	" Level up time", 3600, 10, 10);		//为设置菜单绑定修改值 - 难度等级晋升时间
	_menu->InitValueToData(&ShowFireNum,	" Show fire num", 1, 0, 1, " No| Yes"); //为设置菜单绑定修改值 - 是否显示目前火数
	_menu->EndInitToData();													//指明结束修改值绑定 这要与 BeginInitToData 配对调用
	_menu->SetKeyToData(SoundOs, GK_RETURN, GK_DOWN, GK_UP, 0, 1, true);	//为设置菜单指定按键音和键盘操作键和启用鼠标操作
	int flen;
	char *file_content = GNf::FileFullLoadToMemoryText("samples/base/halloween/Ranking.txt", &flen); //读取排名文本文件
	if(file_content){			 //成功读到文件内容
		Ranking->SetLables(GN_StrSplitToData(file_content, '\n'));  //把文件内容按换行符来分割出多个字符串 并指定给排名列表
		delete [] file_content;  //删除文件内容
	}
	P<GN_sTIME_EXEC_TASK> time_task(1);							//分配一个定时执行任务
	time_task->Init(GN::TimeGetRunCount()+30, HideRanking, (GNu::cSELECT_LIST_TEXT*)Ranking); //设置任务执行时间为当前后30毫秒 回调 参数为排名列表
	GN_TaskPushToIdleExec(time_task, false);					//提交这个任务到闲时执行 不使用后台执行
	
	Scene2D.New(1);									 //为2D场景分配空间
	GNg::cSCENE2D *s2d = Scene2D;					 //拿出它额裸指针以便访问得更快
	s2d->InitLoadToData("samples/base/halloween/Halloween2D.g2d", load_progr);   //读取2D场景文件
	MainBg   = CPP(GNg::cFRAME_RC, s2d->Search("image#00001"));					//在2D场景里查找名为 image#00001 的游戏主界面背景 并挖出它的裸指针
	MainMenu = s2d->Search("main_menu");										//在2D场景里查找名为 main_menu 的游戏主界面 并获得它的所在地址
	MainButton = CPP(GNg::cFRAME_LAUNCHER_FONT,s2d->Search("FontFrame#00001")); //在2D场景里查找名为 FontFrame#00001 的游戏主界面选项按钮 并挖出它的裸指针
	Mission = CPP(GNg::cFRAME_LAUNCHER_FONT, s2d->Search("FontFrame#00002"));   //在2D场景里查找名为 FontFrame#00001 的Mission动画艺术字筐 并挖出它的裸指针
	GameOverEf = CPP(GNg::cFRAME_LAUNCHER, s2d->Search("FLauncher#00002"));		//在2D场景里查找名为 FLauncher#00002 的游戏结束爆炸效果 并挖出它的裸指针
	ImgLife = CPP(GNg::sIMAGE_OBJ, s2d->Search("life"));						//在2D场景里查找名为 life 的剩余命数图标 并挖出它的裸指针
	PumpkinBack  = CPP(GNg::sIMAGE_OBJ, s2d->Search("pumpkin_b"));				//在2D场景里查找名为 pumpkin_b 的南瓜眼后的火图序 并挖出它的裸指针
	PumpkinFront = CPP(GNg::sIMAGE_OBJ, s2d->Search("pumpkin_f"));				//在2D场景里查找名为 pumpkin_f 的南瓜眼前亮起的火图序 并挖出它的裸指针
	Head1  = CPP(GNg::cFRAME_RC, s2d->Search("head1"));							//在2D场景里查找名为 head1 的骨头动画筐1 并挖出它的裸指针
	Head2  = CPP(GNg::cFRAME_RC, s2d->Search("head2"));							//在2D场景里查找名为 head2 的骨头动画筐2 并挖出它的裸指针
	Water  = CPP(GNg::cFRAME_LAUNCHER, s2d->Search("WaterLaunch"));				//在2D场景里查找名为 WaterLaunch 的喷水发射器 并挖出它的裸指针
	GNg::cOBJPTR text_rc_time  = s2d->Search("text_rect01");					//在2D场景里查找名为 text_rect01 的时间文本图筐 并获得它的所在地址 
	GNg::cOBJPTR text_rc_level = s2d->Search("text_rect02");					//在2D场景里查找名为 text_rect02 的等级文本图筐 并获得它的所在地址 
	GNg::cOBJPTR fire = s2d->Search("fire");									//在2D场景里查找名为 fire 的火动画筐 并获得它的所在地址 
	GNg::cOBJPTR destroy = s2d->Search("destroy");								//在2D场景里查找名为 destroy 的火灭动画筐 并获得它的所在地址 
	Stage = s2d->Search("stage");												//在2D场景里查找名为 stage 的游戏舞台 并获得它的所在地址 
																//--------------当要用的对象都查找获取后再拆出对象 否则会找错
	FireList = new GN::sLIST<GNg::cOBJ2D>;						//创建火挂接链表
	{ GNg::cOBJ2D _tmp(GNg::eOBJ_GROUP, FireList); fire.InsertFront(_tmp); }	//把它插入到火动画筐图层前面
	text_rc_time .UnComment(&TextRectTime);						//拆出时间文本图筐给 TextRectTime 接管
	text_rc_level.UnComment(&TextRectLevel);					//拆出等级文本图筐给 TextRectLevel 接管
	fire.UnComment(&Fire);										//拆出火动画筐给 Fire 接管
	destroy.UnComment(&Destroy);								//拆出火灭动画筐给 Destroy 接管
	Stage.UnComment(&Stage2d);									//先把游戏舞台整个组在场景中拆出来 不显示它
	
	NumberOut.Init(Mission->GetFontMap());				//用 Mission 动画艺术字筐里的字体图来给 数字输出器 初始化
	
	GameOverEf->SetNowtime(2000);						//使游戏结束爆炸效果动画时间跳到结尾 它就先隐藏起来
	ChangeLevel = PumpkinBack->img->texnum / 2;			//计算得到南瓜眼后的火图序有多少个亮度等级 每级别两个图
	DamageLevel = (float)((PumpkinBack->img->texnum + PumpkinFront->img->texnum) / 2); //计算得到南瓜总损坏(亮度)等级
	int burst_r;
	Water->aGenColor = BurstColor;						//指定水发射器的色调为游戏设置的大招颜色
	Water->GetFrame()->GetFrameSize(&burst_r,0);		//获得水发射器的水花直径大小
	CP(GNg::cFRAME_RC, Fire)->GetFrameSize(&FireR,0);	//获得火动画筐直径大小
	FireR >>= 1;										//直径除2得到半径   >> 1 对等于除2 但它执行得更快
	BurstHitDist = (burst_r>>1) + FireR;				//将两个半径加起来即为大招爆发的碰撞检测距离 水发射器与火的距离小于这个视为碰到
}
GN_CALL void AppUnInit(){   //▲释放应用必须数据 程序运行结束前会调用它一次 
	GNf::sFILE *fp = GNf::Open("samples/base/halloween/Ranking.txt", "w");  //以文本读写方式打开排名文本文件
	if(fp){     //成功打开
		P< P<char> > ranking_strs = Ranking->GetLables();  //获得排名列表内容
		P<char> *_ranking_strs = ranking_strs;			   //拿出裸指针 已使访问更快
		unsigned int ri = 0, rn = ranking_strs.Num();
		for(; ri < rn; ++ri)GNf::Printf(fp, "%s\n", (char*)_ranking_strs[ri]); //历遍各个排名 并把它打印成文本行到文件
		GNf::Close(fp); //写完关闭文件
	}
	NumberOut.Release();	 //释放数字输出器内容
	TextRectTime.Delete();   //删除时间文本图筐数据
	TextRectLevel.Delete();  //删除等级文本图筐数据
	Fire.Delete();			 //删除火动画筐数据
	Destroy.Delete();		 //删除火灭动画筐数据
	Stage2d.Delete();		 //删除游戏舞台数据
	Scene2D.Delete();		 //删除2D场景数据
							 //..至于那些裸指针数据不用删除 因为那些数据在场景里 随场景一起删除
	Ranking.Delete();		 //删除排名列表
	SceneUi.Delete();		 //删除UI场景数据
	Bgm.Release();			 //释放音频流对象
	SoundSelect.Release();	 //释放按键选择发声点对象
	SoundGameOver.Release(); //释放游戏结束发声点对象		 
	SoundDamage.Release();	 //释放南瓜受损发声点对象	
	SoundBurst.Release();	 //释放大招爆发发声点对象
	SoundDestroy.Release();	 //释放火灭发声点对象	
	SoundOs.Delete();		 //释放所有声音对象 这里要记得要先释放了装有它的发声点对象先再释放它 顺序调乱了可能会有问题
}
GN_CALL void AppDisplay2D(unsigned int nTime){ //▲显示2D画面 程序运行每帧都调用它 
	//定义这些静态变量来临时存储游戏状态
	static unsigned int missionNum;				//漏失计数
	static unsigned int gameTime;				//当前等级游戏时间
	static unsigned int	gameLevel;				//当前游戏难度等级
	static unsigned int	burstNum;				//大招爆发数
	static unsigned int	fireGenFps;				//火出生间隔帧数
	static float		fireSpeed;				//火移动速度
	static float		pumpkinDamage;			//南瓜损坏度
	static GNg::sIMAGE  *pumpkinShowImg;		//当前南瓜损坏度要显示的火图序
	static unsigned int	*pumpkinTexBk;			//用于备份 pumpkinShowImg 的原纹理 texture 指针
	static short		textTimeX,  textTimeY;	//当前游戏时间显示坐标X和坐标Y
	static short		textLevelX, textLevelY; //当前游戏难度等级显示坐标X和坐标Y
	static short		viewLastRngTop;			//视口范围最近变更的顶部坐标
	if(GN::ViewReSized){	//当视口大小变更时
		viewLastRngTop = GNg::ViewCur->ortho_top;   //更新一下它的顶部坐标
		GNg::View.Init(0, 0, GNg::View.viewW, GNg::View.viewH, 0, 0, 550, 450); //保持原视口大小仅调节视口的后台取景范围为 0 0 550, 450 以适配游戏原设计的分辨率 这就会将 550x450 分辨率拉伸到视口的大小
	}
	Bgm.UpdataPlay(nTime);										  //更新播放背景音乐
	Scene2D->UpdatePlay(nTime);									  //更新播放2D场景
	SceneUi->UpdatePlay(nTime);									  //更新播放UI场景
	if((MainBg->aLevel_color & 0xff000000) > 0x55000000){  //当游戏主界面背景色比较不透明时 表示正运行在主界面 ----------------------------------------------------------------------------------------------
		static uint8 optionState = 0;
		uint8 option_state = CUP(GNu::cUI_SIM_DIALOG,OptionMenu)->GetState();	//获得游戏设置菜单当前状态
		if(option_state == 3 && optionState == 2)SoundSelect.Play(0);			//当本帧菜单状态为开始隐藏且上一帧为待操作 就播放选择音
		optionState = option_state;			//记录菜单状态留下一帧用
		if(GN::MouseI[GM_LEFT] && option_state == 0){ //当鼠标左键原位点击松开时且游戏设置菜单正在隐藏中
			SoundSelect.Play(0);			//播放选择音
			unsigned int sl_id = 100000;	//初始指定一个怎么都不可能选到的ID值
			if(MainButton->GetSelectFontFrame(&sl_id)){ //当有点中主界面选项按钮时 获取出选到的ID值
				switch(sl_id){	//判断选到的ID
				case 2: Ranking->RePos(); break;							//是第二个就呼叫出排名列表
				case 1: CUP(GNu::cUI_SIM_DIALOG,OptionMenu)->Pop(); break;  //是第一个就弹出游戏设置菜单
				case 0:{		//是第0个时就进入游戏
				    MainMenu.InsertBack(Stage2d, &Stage);	//插入游戏舞台在主界面群组之后
					Mission->SetText("0", 1);				//设置初始漏失显示数为0
					missionNum = 0;							//设置初始漏失数为0
					textTimeX  = CP(GNg::sIMAGE_OBJ,TextRectTime)->x;   textTimeY  = CP(GNg::sIMAGE_OBJ,TextRectTime)->y;	//设置初始游戏时间显示坐标X和坐标Y
					textLevelX = CP(GNg::sIMAGE_OBJ,TextRectLevel)->x;  textLevelY = CP(GNg::sIMAGE_OBJ,TextRectLevel)->y;  //设置初始游戏难度等级显示坐标X和坐标Y
					Water->SetNowtime(Water->GetEndtime());	//使大招水喷发效果动画时间跳到结尾 让它先隐藏起来
					burstNum = BurstMaxNum;					//充满大招数
					gameLevel = 1;							//设置初始游戏难度等级为1
					gameTime = 0;							//重置当前等级游戏时间
					fireGenFps = 60;						//初始火出生间隔为60帧一个
					fireSpeed = 1.0f;						//初始火移动速度为1
					pumpkinDamage = 0.0f;					//初始南瓜损坏度为0
					PumpkinFront->y -= 10000;				//使眼前的火图序向上移开视口
					pumpkinShowImg = PumpkinBack->img;		//设置南瓜初始显示火图序为眼后的图序
					pumpkinTexBk = pumpkinShowImg->texture; //备份它的原纹理指针
					PumpkinBack->c = 0xff000000;			//南瓜眼后的火图显示颜色初始为黑色
					unsigned int *_start_color = (unsigned int*)&(MainBg->GetFEvent()->start_value); //挖出游戏主界面背景动画事件的起始颜色值指针
					*_start_color = 0x00ffffff;				//设置动画起始颜色值为全透明的 以便背景动画可以渐渐变透明
					MainMenu.aPtr->AnimationIn(false);		//通知游戏主界面退场 退场后的主界面含动画的对象会自动隐藏起来 隐藏后到的对象也不怎么吃性能 如果您想达到极致性能 可以在退场动画播放完后整组拆出
				}break;
				}
			}
			if(sl_id != 2){ //不是点到排名按钮
				short tlx, tly, tlw, tlh;
				Ranking->GetPos(&tlx, &tly, &tlw, &tlh); //获取到排名列表显示范围
			    if(!GNg::ViewCur->HitRange(tlx, tly, tlw, tlh))Ranking->OkHide(); //鼠标点到范围外就隐藏排名列表
		    }
		}
	}else{	//正运行在游戏舞台中 ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
	  unsigned int game_sec = gameTime/1000; //计算得到游戏运行到秒数
	  unsigned int text_color = 0xff6969ff;  //游戏结束得分显示颜色 湖南色
	  if(pumpkinDamage < 1.0f){ //正在游戏中 南瓜还没损坏掉 
	    bool bursting = Water->GetNowtime() < Water->GetEndtime(); //喷水动画当前时间小于它的结束时间 表示正在喷发中
		if(bursting){ //大招水正在喷发中
			Water->aGenRngX = (float)GNg::ViewCur->mouseMX - Water->aGenRngW * 0.5f; //使喷水范围位置X跟随鼠标位置
			Water->aGenRngY = (float)GNg::ViewCur->mouseMY - Water->aGenRngH * 0.5f; //使喷水范围位置Y跟随鼠标位置
		}else if(burstNum && GN::Mouse2[GM_LEFT]){ //非喷发中且剩余有大招和鼠标左键双击了
			--burstNum;					 //减少一个大招数
			Water->ReSetPlaytime();		 //重新播放大招水喷发效果动画
			SoundBurst.Play(0);			 //播放大招音
		}																//构建一个比视口范围宽一个火球半径的矩形
		GNg::sRECT2 view_rc(GNg::ViewCur->ortho_left - FireR, GNg::ViewCur->ortho_top - FireR, GNg::ViewCur->ortho_right + FireR, GNg::ViewCur->ortho_bottom + FireR);
		GNg::sVEC2 view_centre;
		GNg::RectGetCentre(view_rc, view_centre);  //获取到这个矩形的中心位置
		if(GN::TimeGetRunFpsCount() % fireGenFps == 0){   //当到了火出生间隔帧时
			GNg::sVEC2 ray(5000.0f, 0.0f);	//构建一个向右长5000的向量
			GNg::Vec2Rot(&ray, GN_RandomF0_1() * 360.0f); //按固定随机角度来旋转该向量
			ray += view_centre;				//把向量接到中心位置处
			GNg::RectCutLine(&view_rc, &view_centre, &ray); //裁剪掉向量在矩形以外的部分 那么向量的末端就是与矩形的交点
			GNg::cOBJ2D new_fire;
			new_fire.CopyData(Fire,true);	//复制火动画筐数据给新火对象
			GNg::cFRAME_RC *_new_fire = CP(GNg::cFRAME_RC,new_fire);
			GNg::sTRANSF2D &ts = _new_fire->aFrame_ts; //引用新火对象的变换描述
			ts.x = ray.x;  ts.y = ray.y;			   //设置它的诞生点为刚才向量与矩形的交点
			ts.sx = ts.sy = 0.9f + GN_Random(0.2f);	   //设置它的诞生大小比列为0.9至1.1之间的随机大小
			if(ts.sy > 1.08f)_new_fire->aColor = FastFireColor; //比例大于一定值的火设为快速火
			else if(gameLevel >= 3){	//普通大小的游戏难度等级达到3以后
				unsigned int remain_sec = LevelUpTime - game_sec;  //计算到游戏剩余秒数
				if(game_sec > remain_sec){  //该等级难度坚持到一半时间后
					if(remain_sec < 2 || (game_sec % (remain_sec >> 1) == 0))_new_fire->aBlendmode = GNg::eBLEND_BRIGHT; //时间越到末尾就越密间隔使新生火变成浮游火 用发光混合模式来标记它
			}	}
			if(ts.sy > 0.9f + 0.003f * (float)gameLevel)_new_fire->GetEventcount() = 0; //比例大于游戏难度等级越大就越大的值时 就忽略这个新生火透明变化动画
			FireList->push_back(new_fire);	//把新火对象挂接到火链表里 这样它就可以随 Scene2D->UpdatePlay 一起显示出来
		}
		if(ShowFireNum)GN_RunValueCheck((int)FireList->size(), "FireNum", 2);  //如有要显示目前火数就显示它
		if(FireList->size()){				//当有动态生成的火
			view_centre.y -= 15.0f;			//使矩形中心位置往上移15个像素 作为火移动的终点
			float dist;
			GNg::cFRAME_RC *_fire;			
			GNg::sVEC2 v;
			GN::sLIST<GNg::cOBJ2D>::iterator itf = FireList->begin(), etf = FireList->end(); //获得历遍火的首尾检索位置
			for( ; itf != etf; ){	//循环历遍各个火
				_fire = CP(GNg::cFRAME_RC,*itf);    //拿出当前检索到火的裸指针以加快访问速度
				if(_fire->GetAmMode() == GNg::eAM_LOOP){ //当火的动画模式还是循环播放时
					if((_fire->aColor & 0xff000000) == 0xff000000){	 //火的不透明为满 表示还在移动中
						if(bursting? GetFireToMouseDist(_fire) < BurstHitDist /*当大招正在喷发中就判断火到鼠标位置的距离是否小于它们碰撞检测距离*/\
							: GN::Mouse1[GM_LEFT] && _fire->HitPointTest(GNg::ViewCur->mouseMX, GNg::ViewCur->mouseMY)) //否则判断鼠标左键有单击了且火接触到鼠标
						{   //火被打中
							GNg::cOBJ2D new_destroy;
							new_destroy.CopyData(Destroy,false);  //复制火灭动画筐数据给新火灭对象
							CP(GNg::cFRAME_RC,new_destroy)->aFrame_ts = _fire->aFrame_ts;  //同步火动画筐的变换给火灭动画筐
							*itf = new_destroy;					  //使当前检索位置的对象替换成新火灭对象
							Head1->ReSetPlaytime();				  //重新播放骨头动画1
							Head2->ReSetPlaytime();				  //重新播放骨头动画2
							SoundDestroy.Play(0);				  //播放火灭音
						}else{ //火没打中
							GNg::sVEC2 &pos = *((GNg::sVEC2*)&(_fire->aFrame_ts.x));  //定位引用火的变换位置
							v = view_centre - pos;				//计算得火位置到中心位置的一段方向线
							dist = v.Length();					//这段方向线的长度就是火到中心的距离
							if(dist > 100.0f * (1.5f - _fire->aFrame_ts.sy)){ //当距离大于离中心60至40像素之间的范围按火的大小而定 视为未碰到南瓜
								float speed = _fire->aColor == FastFireColor? fireSpeed * 1.5f: fireSpeed;  //颜色是快火的就用1.5倍速度 否则用正常速度
								v /= dist;						//归一化这段方向 使它长度为1 也就是移动方向
								pos += v * speed;				//那就使火往方向按火速度移动一下
								if(_fire->aBlendmode == GNg::eBLEND_BRIGHT){ //当是发光混合模式的火 属于浮游火
									GNg::sVEC2 v1;
									GNg::Vec2Right(&v1, &v);    //获得移动方向的向右方向
									pos += v1 * ((GNg::GetWaveLineHeight((unsigned int)dist, 160)-0.5f) * 3.5f); //按火离中心距离在上落波形线的偏移量来使火左右偏移一下 以实现浮游效果
								}
							}else{  //火碰到南瓜
								++missionNum;					//增加一个漏失数
								char mn_buf[16];
								Mission->SetText(itoa(missionNum, mn_buf, 10), 0); //把漏失数转为10进制字符串传给 Mission 动画艺术字筐更新显示
								pumpkinDamage += 0.025f;		//增加一些南瓜损坏度
								PumpkinBack->c = 0xffffffff;	//恢复南瓜眼后的火图原颜色
								_fire->aColor -= 0x0100020a;	//使火颜色减去一定量起到迷彩若隐若现的变化
								SoundDamage.Play(0);			//播放南瓜受损音

								GNg::ViewCur->ortho_top += 2; GNg::ViewCur->ortho_bottom += 2;	//使视口截取范围向上平移2像素
								GN::ValueChange(&GNg::ViewCur->ortho_top,    viewLastRngTop,    0,0,0.5f, 0.12f);  //然后再插值回原位置 起到2D震屏效果
								GN::ValueChange(&GNg::ViewCur->ortho_bottom, (short)(viewLastRngTop+GNg::ViewCur->orthoH), 0,0,0.5f, 0.12f); //视口截取范围下也一起插值回原位置 
							}
						}
					}else{  //火在南瓜上燃烧
						if(_fire->aFrame_ts.sy < 0.8f){ //火大小比例小于0.8时
							itf = FireList->erase(itf);	  //从链表中移除这个火对象 一并跳到下个检索位置
							continue;					  //跳去继续历遍下个火
						}
						_fire->aColor -= 0x0100020a;	  //使火颜色减去一定量起到迷彩若隐若现的变化
						_fire->aFrame_ts.sx -= 0.001f;	  //使火比例X 缩小一点
						_fire->aFrame_ts.sy -= 0.0005f;	  //使火比例Y 缩小一点点
					}
				}else if(_fire->AmIsEnd()){    //非循环播放且动画已到结束时
					itf = FireList->erase(itf);  //从链表中移除这个火灭对象 一并跳到下个检索位置
					continue;					 //跳去继续历遍下个火
				}
				++itf;	//跳到下个检索位置
			}
		}
		pumpkinDamage -= 0.0001f * GN::TimeRatio; //每帧减少一点损坏度 乘 GN::TimeRatio 可避免受帧率高低影响恢复速度变快或变慢
		if(pumpkinDamage < 0.0f){		  //当南瓜损坏度达到下限
			pumpkinDamage = 0.0f;			//使损坏度最小为0
			PumpkinBack->c = 0xff000000;	//使南瓜眼后的火图显示颜色为黑色
		}else if(pumpkinDamage >= 1.0f){  //当南瓜损坏度达到上限
			GameOverEf->ReSetPlaytime();	//重新播放游戏结束爆炸效果动画
			SoundGameOver.Play(0);			//播放游戏结束音
			short dx = GNu::NearCx(), dy = (short)GNu::NearCy() - NumberOut.GetFontHeight()/2;  //计算到得分在视口中间显示的位置
			GN::ValueChange(&textTimeX,  dx, 0,0,0.5f, 0.12f);	//插值游戏时间显示坐标X到视口中间处 以实现得分飞到中间的动画效果
			GN::ValueChange(&textTimeY,  dy, 0,0,0.5f, 0.12f);	//插值游戏时间显示坐标Y到视口中间处
			GN::ValueChange(&textLevelX, (short)(dx - NumberOut.GetFontRangeX() * 3), 0,0,0.5f, 0.12f);  //插值游戏难度等级显示坐标X到视口中间往左3个字段处
			GN::ValueChange(&textLevelY, dy, 0,0,0.5f, 0.12f);  //插值游戏难度等级显示坐标Y到视口中间处
		}else{
			unsigned int level = pumpkinDamage * DamageLevel;  //计算出当前损坏等级
			if((level < ChangeLevel) != (pumpkinShowImg == (GNg::sIMAGE*)(PumpkinBack->img))){  //按当前损坏等级判断出南瓜前后火图序需切换时
				pumpkinShowImg->texture = pumpkinTexBk;  //恢复 pumpkinShowImg 的原纹理 texture 指针
				if(level < ChangeLevel){				 //当前损坏等级小于火图序切换等级
					pumpkinShowImg = PumpkinBack->img;	 //使用眼后的火图序来显示
					PumpkinBack ->y += 10000;			 //使眼后的火图序移回视口中
					PumpkinFront->y -= 10000;			 //使眼前的火图序向上移开视口
				}else{
					pumpkinShowImg = PumpkinFront->img;  //使用眼前的火图序来显示
					PumpkinFront->y += 10000;			 //使眼前的火图序移回视口中
					PumpkinBack ->y -= 10000;			 //使眼后的火图序向上移开视口
				}
				pumpkinTexBk = pumpkinShowImg->texture;	 //备份它的原纹理指针
			}
			unsigned int tid = (level < ChangeLevel? level: level-ChangeLevel) * 2 + (GN::TimeGetRunFpsCount()/12 & 1);   //计算出当前损坏等级对应的图序ID 在同亮度级别的图序中每12帧轮换一次
			pumpkinShowImg->texture = pumpkinTexBk + tid;   //使纹理指针定位到它当前要显示的图序ID处 以实现程序根据损坏等级控制火图序强弱动画播放的效果
		}
		unsigned int li = 0, life_num = (0.999f - pumpkinDamage) * 3.0f;  //按剩余损坏度计算出剩余命数
		short lw = ImgLife->w + 2;	//每个命图标的显示跨距
		short lx = ImgLife->x + lw; //下一个命图标显示的X坐标
		for(; li < life_num; ++li, lx += lw)GNg::ImageDraw(ImgLife->img, lx, ImgLife->y);  //向右续个绘制出剩余命图标
		
		GNg::sIMAGE *burst_img = GameOverEf->GetFrame()->GetImage();	//挖出游戏结束爆炸效果的单个火焰图像
		lx = ImgLife->x + lw * 4;	//在右边第4个命图标的显示位置X开始
		for(li = 0; li < burstNum; ++li, lx += lw)GNg::ImageDraw(burst_img, lx, ImgLife->y - 10, 42, 42);  //向右续个以42像素大小绘制出剩余大招数
		gameTime += nTime;			//累计当前游戏时间
		text_color = 0xffffffff;	//还在游戏中时得分显示颜色为白色

	  }else if(PumpkinBack->y < -5000 && GameOverEf->GetNowtime() > 1800){ //游戏结束
		FireList->clear();						 //清除所有动态生成的火
		MainMenu.aPtr->AnimationIn(true);		 //通知游戏主界面进场
		pumpkinShowImg->texture = pumpkinTexBk;  //恢复 pumpkinShowImg 的原纹理 texture 指针
		PumpkinBack->y += 10000;				 //使眼后的火图序移回视口中
		Stage.UnComment(&Stage2d);				 //把游戏舞台整个组在场景中拆出来 不显示它
		InsterRankingFromMaxToMin(Ranking, "% 3d % 6d", gameLevel, game_sec); //插入的得分到排名列表
	  }
	  if(game_sec >= LevelUpTime){    //当游戏时间大于等级晋升时间时 
		  ++gameLevel;							 //就晋升等级
		  fireGenFps = (float)fireGenFps*0.875f; //减少一点火出生间隔帧数
		  fireSpeed += 0.03f;					 //增加一点移动速度 
		  burstNum = BurstMaxNum;				 //充满大招数 
		  gameTime = 0;							 //使游戏时间重设为0
	  }
	  NumberOut.TextOutNumber(game_sec,  textTimeX,  textTimeY, 0,0, text_color);		//输出显示当前游戏时间 单位为秒
	  NumberOut.TextOutNumber(gameLevel, textLevelX, textLevelY,0,0, text_color);		//输出显示当前游戏难度等级
	}
}
GN_CALL void AppDisplay3D(unsigned int nTime){} //▲显示3D画面 程序运行每帧都调用它 

GN_CALL void AppRun(unsigned int nTime){}       //▲运行程序后台的处理 程序运行每帧都调用它
}