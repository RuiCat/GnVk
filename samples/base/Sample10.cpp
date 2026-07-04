#include<GN.h> //使用GN必需包含GN.h头文件

GNg::sIMAGE  StarImg, RedImg, Bg;				//定义图象
GNg::cFRAME  StarAm, StarAm2, StarAm3, EFAm;	//定义动画筐对象
GNg::sAM_EVENT *Ev=0;						    //定义命令动画筐怎样运动的事件
GNg::cFRAME_LAUNCHER    Stars1,Stars2,Stars3;   //定义动画筐发射器

GNg::cFRAME_LAUNCHER2D  Firework;			    //定义烟花复合筐发射器
GNg::cFRAME_LAUNCHER2D  Burst;				    //定义爆炸复合筐发射器
GNg::cFRAME_LAUNCHER2D  Tail;				    //定义甩尾复合筐发射器

GNg::cLENS_FLARE		CB;						//定义镜头炫光
unsigned int			BrightColor;			//镜头光色调

//设置镜头炫光位置(当前检索到的子效果发射器, 传回您的用户参数)
void SetCameraBrightPose(GNg::cFRAME_LAUNCHER2D *chld, void *us_arg){
	if(chld->GetNowtime() < 512){ //在子发射器播放前段时
		GNg::sRECT2 range;
		chld->GetRange(&range);
		GNg::RectGetCentre(range, *((GNg::sVEC2*)us_arg)); //获取到子发射器生成范围中心通过用户参数传给坐标 brightPose
		BrightColor = ((512-chld->GetNowtime())*256/512<<24)|(chld->aGenColor&0x00ffffff); //计算到炫光色调为子发射器的色调慢慢变淡
	}
}
extern "C"{
GN_CALL void AppInit(){     //▲初始化应用必须数据 程序开始准备好引擎系统后会调用它一次
	StarImg.InitToData("samples/base/image/star4_",1,GNg::CR_ALPHA); //星星图用透明格式保存就够了节省显存空间
	RedImg.InitToData("samples/base/image/red_bright_");
	Bg.InitToData("samples/base/image/louyu");						//读取背景图片

	Ev = new GNg::sAM_EVENT[7];
	Ev[0].Init(GNg::eEVENT_MOVE_Y,0,1500,0.0f,-120.0f); //!!温馨提示! 设置动画的良好习惯 一般在事件里设置的坐标都为相对自身世界的偏移量坐标 在使用.Init成员函数时才设置对象的摆放坐标 这样就没那么容易混淆而使对象放不到想要的地方
	Ev[1].InitColor(0,300,0x00ffac88,0xffffffac);		//设置0到300毫秒的颜色变化
	Ev[2].InitColor(300,1000, 0xffffffac,0xffffffac);	//设置300到1000毫秒的颜色变化
	Ev[3].InitColor(1000,1500,0xffffffac,0x00ff6800);	//设置1000到1500毫秒的颜色变化

	Ev[4].Init(GNg::eEVENT_SIZE_X|GNg::eEVENT_SIZE_Y,  0, 500,0.4f,1.0f,0.0f,GNg::L_START); //减速的缓动
	Ev[5].Init(GNg::eEVENT_SIZE_X|GNg::eEVENT_SIZE_Y,500,1000,1.0f,0.7f,0.0f,GNg::L_END);   //加速的缓动
	Ev[6].InitColor(0,1000,0xffffffff,0x00ffffff);

	//StarAm装填图象(指定图象,动画事件个数,动画事件数组, 变换信息(x,y), 图心点对位x,y,	混合模式发光,	   颜色,      循环播放)
	StarAm.Init(   &StarImg,     4,         Ev,   GNg::sTRANSF2D(80,430),0.5f,0.5f,  GNg::eBLEND_BRIGHT, 0xffffffff, GNg::eAM_LOOP);
	StarAm2 = StarAm; //使动画筐2的效果和1一样
	StarAm3.Init(&RedImg,3,Ev+4,GNg::Ts2D,0.4f,0.6f,2); //设置这个的中心点在中间移开一点

	//为附加效果筐装填图象
	EFAm.Init(&StarImg,0,0,GNg::Ts2D,0.5f,0.5f);
	EFAm.aFrame_ts.sx=2.4f; //使动画筐放大2.4倍
	EFAm.aFrame_ts.sy=2.4f;

    //设置动画筐发射器的发射方式
	Stars1.InitToData(
		&StarAm,                                  //装入参照动画筐
		0.0f,                                     //发射角度 修改这个可以向其他方向散发出去
		70,                                       //发射间隔时间
		3,                                        //一次生成数
		0,                                        //位置变化幅度 0随机
		0,                                        //角度变化幅度 0随机
		0,                                        //大小变化幅度 0随机
		0,                                        //颜色变化幅度 0随机
		5000,                                     //该发射器的活动时间 发射超过活动时间后会停止发射 0为无限发射
		&EFAm                                     //附加效果 这个可以方便在表面加多一个闪动效果 使它看起来更漂亮
	);
	GNg::sRECT2 rng1(100.0f,150.0f,150.0f,200.0f);
	Stars1.SetRange(&rng1); //设置发射点的出生范围 用矩形描述

	Stars2.InitToData(
		&StarAm,                                  //装入参照动画筐
		360.0f,                                   //发射角度 修改这个可以向其他方向散发出去
		500,                                      //发射间隔时间
		3,                                        //一次生成数
		0,                                        //位置变化幅度 0随机
		3,                                        //角度变化幅度 3顺时针慢慢转动发射
		0,                                        //大小变化幅度 0随机
		127,                                      //颜色变化幅度 127不变
		0,                                        //该发射器的活动时间 发射超过活动时间后会停止发射 0为无限发射
		&EFAm                                     //附加效果 这个可以方便在表面加多一个闪动效果 使它看起来更漂亮
	);
	GNg::sRECT2 rng2(700.0f,320.0f,710.0f,330.0f);
	Stars2.SetRange(&rng2); //设置发射点的出生范围 用矩形描述

	Stars3.InitToData(
		&StarAm3,                                 //装入参照动画筐
		360.0f,                                   //发射角度 修改这个可以向其他方向散发出去
		100,                                      //发射间隔时间
		4,                                        //一次生成数
		0,                                        //位置变化幅度 0随机
		0,                                        //角度变化幅度 0随机
		0,                                        //大小变化幅度 0随机
		126,                                      //颜色变化幅度 126慢慢变色
		0                                         //该发射器的活动时间 发射超过活动时间后会停止发射 0为无限发射
	);
	GNg::sRECT2 rng3(300,560,500.0f,610.0f);
	Stars3.SetRange(&rng3);       //设置发射点的出生范围 用矩形描述

	//参数的详细设置资料请参考定义该类的头文件
	//记住 设置粒子效果的方法是先设置一个参照的动画筐对象
	//并设置它在自身世界坐标内的运动方式 然后把它装进"cFRAME_LAUNCHER"类的对象 再设置发射方式
	//如果你的原参照筐也可以带子父对象关系 这样效果可能会更棒哦
	//如果您觉得在代码手动设置这些参数很麻烦 您可以通过引擎编辑器来编辑好想要的效果动画并保存到文件 之后通过简单加载文件即可

	//以下是一个烟花的复合筐发射器设置 ---------------------------------------------------------------------------------------------
	GNg::sIMAGE *star_img = new GNg::sIMAGE;
	star_img->InitToData("samples/base/image/star4_",1,GNg::CR_ALPHA);//星星图用透明格式保存就够了

	GNg::sAM_EVENT *ev_up = new GNg::sAM_EVENT[3]; //烟花上升动画事件
	ev_up[0].Init(GNg::eEVENT_MOVE_Y,0,1350,0.0f,-510.0f,0.0f,GNg::L_START); //向上缓动移动
	ev_up[1].InitColor(0,250,0x00ffac88,0xfffffffe);
	ev_up[2].InitColor(250,1350, 0xfffffffe,0xfffffffe);

	GNg::cFRAME *star_up = new GNg::cFRAME;
	//上升图框装填图象(指定图象,动画事件个数,动画事件数组, 变换信息(x,y),图心点对位x,y,发光混合模式,   颜色,      播放完就消失)
	star_up->Init(   star_img,     3,         ev_up,   GNg::Ts2D,		   0.5f,0.5f,       2,      0xffffffff,GNg::eAM_ONCE_HIDE);
	star_up->KeepExternObjs();	   //保管外部连接对象 调用后 star_img ev_up 等外部对象数据不必手动释放了 !但这样您需确保一个动画筐引用一个图像和一组事件 不能和其它对象共享

	Firework.InitToData(
		star_up,                                      //装入烟花上升动画筐
		25.0f,                                        //发射角度 修改这个可以向其他方向散发出去
		500,                                          //发射间隔时间
		1,                                            //一次生成数
		0,                                            //位置变化幅度 0随机
		0,                                            //角度变化幅度 0随机
		0,                                            //大小变化幅度 0随机
		0,											  //颜色变化幅度 0随机
		0                                             //该发射器的活动时间 发射超过活动时间后会停止发射 0为无限发射
	);
	GNg::sRECT2 rng4(300,530,500.0f,560.0f);
	Firework.SetRange(&rng4);     //设置烟花发射点的出生范围 用矩形描述
	Firework.KeepExternObjs();	   //保管外部连接对象 调用后 star_up 等外部对象数据不必手动释放了

	GNg::sIMAGE *star2_img = new GNg::sIMAGE, *star3_img = new GNg::sIMAGE;
	*star2_img = *star_img;  //复制一个相同的星图
	*star3_img = *star_img;  //复制一个相同的星图

	GNg::sAM_EVENT *ev_t = new GNg::sAM_EVENT[3];	//烟花甩尾动画事件
	ev_t[0].Init(GNg::eEVENT_MOVE_Y,0,450,0.0f,-100.0f);
	ev_t[1].Init(GNg::eEVENT_SIZE_X|GNg::eEVENT_SIZE_Y,0,450,0.5f, 0.35f);
	ev_t[2].InitColor(0,450, 0xffffffff,0xffff8000);

	GNg::cFRAME *star_t = new GNg::cFRAME;
	//甩尾图框装填图象(指定图象,动画事件个数,动画事件数组, 变换信息(x,y),图心点对位x,y,发光混合模式,   颜色,      播放完就消失)
	star_t->Init(  star2_img,     3,         ev_t,		   GNg::Ts2D,     0.5f,0.5f,       2,      0xffffffff,GNg::eAM_ONCE_HIDE);
	star_t->KeepExternObjs();	   //保管外部连接对象 调用后 star2_img ev_t 等外部对象数据不必手动释放了

	Tail.InitToData(
		star_t,                                       //装入烟花甩尾动画筐
		0.0f,                                         //发射角度 修改这个可以向其他方向散发出去
		40,											  //发射间隔时间
		1,                                            //一次生成数
		0,                                            //位置变化幅度 0随机
		0,                                            //角度变化幅度 0随机
		0,                                            //大小变化幅度 0随机
		127,										  //颜色变化幅度 127颜色不变
		0                                             //当该发射器作为种子参照式样时 指定0 即可跟随射出单元一起完结
	);
	Tail.aGenRngW = 2.0f;	//仅指定产生甩尾筐的范围宽度 因为产生甩尾的位置是跟随射出筐单元的 所以没必要指定位置
	Tail.aGenRngH = 2.0f;	//仅指定产生甩尾筐的范围高度
	Tail.aGen_ts_flags = GNg::eTRANSF_SCL|eMODULATE_COLOR; //继承跟随单元的缩放比例与色调
	Tail.KeepExternObjs();	   //保管外部连接对象 调用后 star_t 等外部对象数据不必手动释放了 !但这样您需确保一个发射器引用一次动画筐 不能和其它对象共享

	GNg::sAM_EVENT *ev_b = new GNg::sAM_EVENT[4];	//烟花爆炸动画事件
	ev_b[0].Init(GNg::eEVENT_MOVE_Y,0,1350,0.0f,-250.0f,0.5f,GNg::L_START);
	ev_b[1].Init(GNg::eEVENT_ROTATION,0,1350,0.0f,90.0f,0.5f,GNg::L_START);
	ev_b[2].Init(GNg::eEVENT_SIZE_X|GNg::eEVENT_SIZE_Y,0,1350,0.5f, 0.5f);
	ev_b[3].InitColor(0,1350, 0xffffffff,0x00ffffff,0.75f,GNg::L_START);

	GNg::cFRAME *star_b = new GNg::cFRAME;
	//爆炸图框装填图象(指定图象,动画事件个数,动画事件数组, 变换信息(x,y),图心点对位x,y,发光混合模式,   颜色,      播放完就消失)
	star_b->Init(  star3_img,     4,         ev_b,		   GNg::Ts2D,     0.5f,0.5f,       2,      0xffffffff,GNg::eAM_ONCE_HIDE);
	star_b->KeepExternObjs();	   //保管外部连接对象 调用后 star3_img ev_b 等外部对象数据不必手动释放了

	Burst.InitToData(
		star_b,                                       //装入烟花爆炸动画筐
		360.0f,                                       //发射角度 修改这个可以向其他方向散发出去
		30,											  //发射间隔时间
		32,                                           //一次生成数
		0,                                            //位置变化幅度 0随机
		0,                                            //角度变化幅度 0随机
		0,                                            //大小变化幅度 0随机
		127,										  //颜色变化幅度 127颜色不变
		60                                            //该发射器的活动时间
	);
	Burst.aGenRngW = 28.0f;	   //仅指定产生爆炸筐的范围宽度
	Burst.aGenRngH = 28.0f;	   //仅指定产生爆炸筐的范围高度
	Burst.aGen_ts_flags = GNg::eTRANSF_SCL|eMODULATE_COLOR; //继承跟随单元的缩放比例与色调
	Burst.KeepExternObjs();	   //保管外部连接对象 调用后 star_b 等外部对象数据不必手动释放了

	//定义附着子效果种子			 爆炸		上升甩尾	爆开甩尾
	P<GNg::cFRAME_LAUNCHER2D::cSEED> seed_b(1), seed_t(1), seed_bt(1);

	//初始化附着效果种子(父发射器, 参照式样, 相对单元射出后的引发时间 默认为自动设为射出单元动画结束时间, 引发后是否一直跟随射出单元变换位置)
	seed_bt->InitToData(&Burst,	    &Tail,						30,												true);
	Burst.aAddEffectSeeds.New(1);
	Burst.aAddEffectSeeds[0] = seed_bt;   //为爆开的筐单元附着甩尾子效果

	seed_b->InitToData(&Firework, &Burst,	0xffffffff, false);
	seed_t->InitToData(&Firework, &Tail,  30, true);
	Firework.aAddEffectSeeds.New(2);
	Firework.aAddEffectSeeds[0] = seed_b; //为上升后的筐单元附着爆开子效果
	Firework.aAddEffectSeeds[1] = seed_t; //为上升途中的筐单元附着甩尾子效果

	//-------------------------------------------------------------------------------------------------------------------------------
	CB.InitToData(GN_SysDir("eft/camerabright_")); //在引擎系统目录里加载光晕式样图
}
GN_CALL void AppUnInit(){   //▲释放应用必须数据 程序运行结束前会调用它一次
	Firework.Release();			//释放烟花发射器
	Burst.Release();			//释放爆炸发射器
	Tail.Release();				//释放甩尾发射器
	Stars1.Release();           //释放发射器
	Stars2.Release();           //释放发射器
	Stars3.Release();           //释放发射器
	GN_DeleteArray(Ev);         //安全释放内存
	StarImg.Release();          //释放图片
	RedImg.Release();           //释放图片
	Bg.Release();				//释放背景图片
}
GN_CALL void AppDisplay2D(unsigned int nTime){ //▲显示2D画面 程序运行每帧都调用它
	GNg::ImageDraw(&Bg, GNu::NearLx(), GNu::NearTy(), GNg::ViewCur->orthoW, GNg::ViewCur->orthoH); //填满全屏绘制背景

	if(GN::Key1[GK_D])
	{
		StarAm2.ReSetPlaytime();	//重新播发动画
		static int count = 0;
		++count;
		GN_Log("ReSetPlaytime:%d\n",count); //利用调试信息打印一些文本出来 当屏幕出现“!”号按F11查看 F10和F12滚屏 用此函数输出信息如果不能在屏幕上显示 即会在应用程序同一目录下生成"RunLog.txt"记录刚才的信息
	}
	// 移动筐发射器1
	if(GN::Key[GK_LEFT]) { Stars1.aGenRngX -= 3.0f; }
	if(GN::Key[GK_RIGHT]){ Stars1.aGenRngX += 3.0f; }
	if(GN::Key[GK_UP])   { Stars1.aGenRngY -= 3.0f; }
	if(GN::Key[GK_DOWN]) { Stars1.aGenRngY += 3.0f; }

	if(GN::Key1[GK_A])Stars1.ContinueGenerate(2000);	 //发射多两秒
    if(GN::Key1[GK_S])Stars1.StopGenerate();			 //停止生成
	if(GN::Key1[GK_P])Stars1.Pause();					 //暂停生成
    if(GN::Key1[GK_1])Stars1.SetGenerateOnceNum(1);      //重设置一次发射数
	if(GN::Key1[GK_2])Stars1.SetGenerateOnceNum(2);      //重设置一次发射数
	if(GN::Key1[GK_3])Stars1.SetGenerateOnceNum(3);      //重设置一次发射数
    if(GN::Key1[GK_4])Stars1.SetGenerateSpeed(200);      //重设置发射间隔
    if(GN::Key1[GK_5])Stars1.SetGenerateSpeed(500);      //重设置发射间隔
	if(GN::Key1[GK_6])Stars1.SetGenerateSpeed(70);       //重设置发射间隔

	StarAm2.PlayFrame(15);  //播放StarAm2动画 给你看看原参照对象的效果
	Firework.PlayFrame(15); //播放烟花动画
	Stars1.PlayFrame(15);   //播放发射动画筐1
	Stars2.PlayFrame(15);   //播放发射动画筐2
	Stars3.PlayFrame(15);   //播放发射动画筐3

	static GNg::sVEC2 brightPose; //用于存储镜头炫光位置
	Firework.aAddEffectSeeds[0]->ChildsExec(SetCameraBrightPose, &brightPose); //检索烟花的爆开子效果里的所有子发射器执行(设置镜头炫光位置函数, 通过用户参数来接收刚爆开的位置)
	CB.Display(15, brightPose.x, brightPose.y, BrightColor); //显示镜头炫光
}
GN_CALL void AppDisplay3D(unsigned int nTime){} //▲显示3D画面 程序运行每帧都调用它

GN_CALL void AppRun(unsigned int nTime){}       //▲运行程序后台的处理 程序运行每帧都调用它
}
