#include<Media/GN_Media.h>  // !这里面也包含了GN.h  这个为引擎的扩展库 并不是必须的
							//如果您不用视频播放 项目里也可以不存在 GN_Media_win32.dll 
							//FFmpeg avcodec-52.dll avformat-52.dll avutil-49.dll swscale-0.dll 这些动态库 以节省空间
char	*ScrollInfos = 
"GN Engine Sample\n\
Open Source Code\n\
--* Easy To Learn!! *--\n\
\n\
SCENE Transplant: Auspicious Studio\n\
\n\
PARTICLE Effects: Auspicious Studio\n\
\n\
POST-Processing: Auspicious Studio\n\
\n\
BGM From: Konami\n\
<Tokimeki Memorial2  Dancing Summer>\n\
\n\
SCENE From:\n\
www.unrealengine.com\n\
/marketplace/zh-CN/product\n\
/hand-painted-environment\n\
\n\
Technical Support:\n\
www.gncreate.com\n\
www.ffmpeg.org\n\
\n\
Code Complete by:2023.05.26";	 //滚屏显示的内容

GNg::cTEXT_ART   Fascii;		 //英文文本输出器
GNg::sIMAGE		 ImgAscii;		 //英文字图

GNa::cSOUND_SRC  SoundS,SoundS2; //发声点对象
GNa::sSOUND      SoundO;         //声音对象

GNa::cBGM_MUSIC  Bgm;            //音频流播放器 该对象以流式播放不会加载整个文件到内存
GNm::cVIDEO      AvPlay;         //视频流播放器 该对象以流式播放不会加载整个文件到内存

//类GNa::cSOUND_SRC支持3D音效 要使用3D音效定位时 指给它的 GNa::sSOUND 对象要转换为单声道格式
//几个不同的类GNa::cSOUND_SRC对象可以装入同一个GNa::sSOUND对象 这样节省内存 可分别调整播放音量和音调和空间定位实现不同的效果 而互相之间不会影响对方

extern "C"{
GN_CALL void AppInit(){     //▲初始化应用必须数据 程序开始准备好引擎系统后会调用它一次 
	ImgAscii.InitToData("samples/base/image/fontmap_");  //读入字图文件
	
	//定义排列格式(			是否填满文本筐, 文本水平居中靠位, 文本垂直靠位, 字间距, 行距, 单字宽, 单字高,  自动换行模式,     竖方向输出, 反转输出)
	GNg::sARRANGE_FMT asciifmt(false,              0.5f,         0.0f,        -3,    50,   24,     46);
	//设置文本输出器(字体式样图, 排列格式)
	Fascii.Init(&ImgAscii,   &asciifmt);

	AvPlay.InitToData("samples/base/sound/summer.mkv", 5, false, false); //视频流(文件, 循环次数, 不同步时间, 不启用音频) !FFmpeg 8.x AAC解码器初始化可能失败
	Bgm.InitToData("samples/base/sound/018.mp3",1);           //为音频流指定播放文件(文件路径, 重复播放次数 默认值为不断循环播放)
	SoundO.InitToData("samples/base/sound/click.wav",true);   //读入声音文件（文件路径, 是否转换为定点声音) 定点声音即是转为单声道
     
	//为发声点装入声音(指定声音对象, 是否不断循环发声, 该对象发声能重叠的数量 一般为1, 初始音量0.0f-1.0f,  初始音调0.003f-4.35f)
	SoundS.InitToData (&SoundO,       false,                   1,							 1.0f,              1.5f);
    SoundS2.InitToData(&SoundO,       true,                    1,						     1.0f,              1.0f);
	SoundS .Play(0);
	SoundS2.Play(0);  //播放(引用在对象内重叠发声点编号 由0开始)
	SoundS2.Pause(0); //先让它暂停一下

	/*--------------------------------------------------------------------------------------------\
	|	!!重叠对象的主要应用概念是:比如在有些场合同一种声音发出以后,                              |
	|	再这声音还未停止的时候可能会在发出多一次这样的声音 这情况下重叠对象就能运用了             |
	|	这经常在粒子系统里用上 实现好多相同声音发出来那样 互相不会打段对方 声音还会跟粒子运动的   |
	|	位置而改变发声方位 一般应用情况下指定1就够了 这也是默认值                                 |
	\--------------------------------------------------------------------------------------------*/
}
GN_CALL void AppUnInit(){   //▲释放应用必须数据 程序运行结束前会调用它一次 
	SoundS2.Release(); //释放发声点对象
	SoundS.Release();  //释放发声点对象
    SoundO.Release();  //释放声音对象 这里要记得要先释放了装有它的发声点对象先再释放它 顺序调乱了可能会有问题
	Bgm.Release();	   //释放音频流对象
	AvPlay.Release();  //释放视频流对象
	Fascii.Release();  //释放文本输出器
	ImgAscii.Release();//释放字图
}
GN_CALL void AppDisplay2D(unsigned int nTime){ //▲显示2D画面 程序运行每帧都调用它 
	int vw = AvPlay.GetImage()->width, vh = AvPlay.GetImage()->height; //获取视频宽度和高度
	int win_w = GNg::ViewCur->ortho_right - GNg::ViewCur->ortho_left;
	int win_h = GNg::ViewCur->ortho_bottom - GNg::ViewCur->ortho_top;
	int dx, dy, dw, dh;
	// 缩放视频以适配窗口 (保持宽高比)
	if(vw > 128 && vh > 128){ // 有效视频帧 (非默认 128x128)
		float scale = (float)win_w / (float)vw;
		if((int)(vh * scale) > win_h) scale = (float)win_h / (float)vh;
		dw = (int)(vw * scale); dh = (int)(vh * scale);
		dx = (win_w - dw) / 2; dy = (win_h - dh) / 2;
	}else{
		dw = win_w; dh = win_h; dx = 0; dy = 0;
	}
	unsigned int fpsn = GN::TimeGetRunFpsCount();
	// 视频已自动缩放适配窗口, 不再需要GN_SetWindowPos (GN_VkResize不能在渲染pass内调用)
	(void)fpsn;

	//更新播放视频流(帧间隔时间, 左上坐标X, 左上坐标Y, 宽,  高,     颜色,     混合模式)
	AvPlay.UpdataPlay(nTime, dx, dy, dw, dh, 0xffffffff, GNg::eBLEND_NORMAL);
	//更新播放音频流(帧间隔时间)
	Bgm.UpdataPlay(nTime);
 
	// 显示滚动字幕 -------------------------------------------------------------------------------------------------------------
	static int text_y_ofs = 600, round_num = 0;
	text_y_ofs -= 1;    //字幕坐标每帧上移一点
	if(text_y_ofs < Fascii.GetFontRangeY() * -23 /*字幕总共23行*/){
		++round_num;    //字幕坐标拉到尾时 滚动回数加1
		if(round_num < 2)text_y_ofs = 600; //回数小于2时使坐标重新回到底部
	}
	//显示输出字幕内容
	Fascii.OutText( 
		 ScrollInfos,		 //内容文本
		 0,					 //输出的字符个数 0表示全输出
		 0,					 //文本筐左上角坐标X
		 0,					 //文本筐左上角坐标Y
		 800,				 //文本筐宽度 值0即不使用文本筐
		 600,				 //文本筐高度 值0即不使用文本筐
		 0xffffffff,		 //文本颜色
		 GNg::eBLEND_NORMAL, //混合模式
		 0,					 //在文本筐内的水平偏移量 修改该值可以实现在筐内滚动文本效果 如果排列格式设置了填满文本筐这值不起作用
		 text_y_ofs			 //在文本筐内的垂直偏移量 
	);
	GNg::Draw2DAlphaDirectionSet(GNu::eNEAR_BOTTOM); //使接下来的2D绘制都为向下渐淡的
	//绘制图像的一部分(传入刚更新过的视频图, 左上坐标X, 坐标Y, 宽度, 高度, 颜色,	图序号, 混合模式,			  图像左边截取范围U,		 上边截取范围V,				右边截取范围U,				下边截取范围V)  !UV的概念相对于图片自身左上角为0,0  右下角为1,1的比率  图中心即为:0.5,0.5
	GNg::ImageDrawRng(AvPlay.GetImage(),	    0,		 0,	    800, 100 , 0xffffffff, 0,   GNg::eBLEND_NORMAL, (float)(-dx)/(float)vw,	(float)(  0-dy)/(float)vh,	(float)(800-dx)/(float)vw,	(float)(100-dy)/(float)vh);
	GNg::Draw2DAlphaDirectionSet(GNu::eNEAR_TOP);    //使接下来的2D绘制都为向上渐淡的
	GNg::ImageDrawRng(AvPlay.GetImage(),	    0,	   500,	    800, 100 , 0xffffffff, 0,	GNg::eBLEND_NORMAL, (float)(-dx)/(float)vw,	(float)(500-dy)/(float)vh,	(float)(800-dx)/(float)vw,	(float)(600-dy)/(float)vh);
	GNg::Draw2DAlphaDirectionSet(GNu::eNEAR_NULL);   //取消渐变绘制 -------------------------------------------------------------

	static float v_pitch = 1.0f, v_volume = 1.0f;
	//                                    设置音调(    音调值,  当前音调演变为目标音调所需的时间)
	if(GN::Key[GK_W]){ v_pitch += 0.005f; Bgm.SetPitch(v_pitch,    0); }
    if(GN::Key[GK_S]){ v_pitch -= 0.005f; Bgm.SetPitch(v_pitch,    0); }
    //                                    设置音量(    音调值,  当前音量演变为目标音量所需的时间)
	if(GN::Key1[GK_A]){ v_volume = 1.0f; Bgm.SetVolume(v_volume,    2000); }
    if(GN::Key1[GK_D]){ v_volume = 0.1f; Bgm.SetVolume(v_volume,    2000); }
    
	//            设置跳转进度状态(0.0f--1.0f)  设置跳转进度状态(0.0f--1.0f)
	if(GN::Key1[GK_1]){ Bgm.SetStage(0.0f );  AvPlay.SetStage(0.0f ); }
    if(GN::Key1[GK_2]){ Bgm.SetStage(0.25f);  AvPlay.SetStage(0.25f); }
	if(GN::Key1[GK_3]){ Bgm.SetStage(0.5f );  AvPlay.SetStage(0.5f ); }
	if(GN::Key1[GK_4]){ Bgm.SetStage(0.75f);  AvPlay.SetStage(0.75f); }

	static float px = 0.0f, py = 0.0f, pz = 0.0f;  //以下这几个控制实现3D音效 发声点可以根据坐标移动
	//								  设置发声点位置(选用那个编号, 坐标X   Y     Z)
	if(GN::Key[GK_LEFT]) { px -= 0.15f; SoundS2.SetPosi(0,            px,  py,  pz); }
	if(GN::Key[GK_RIGHT]){ px += 0.15f; SoundS2.SetPosi(0,            px,  py,  pz); }
	if(GN::Key[GK_UP])   { py -= 0.15f; SoundS2.SetPosi(0,            px,  py,  pz); }
	if(GN::Key[GK_DOWN]) { py += 0.15f; SoundS2.SetPosi(0,            px,  py,  pz); }
	if(GN::Key[GK_HOME]) { pz -= 0.15f; SoundS2.SetPosi(0,            px,  py,  pz); }
	if(GN::Key[GK_END])  { pz += 0.15f; SoundS2.SetPosi(0,            px,  py,  pz); }
	
	if(GN::Key1[GK_B])SoundS2.Play(0); 
	if(GN::Key1[GK_N])SoundS2.Pause(0); //按下N键控制发声点2继续播放或暂停
	if(GN::Key1[GK_M])SoundS.Play(0);   //按下M键控制发声点1播放

	/*-- 其它灵活提示 ---------------------------------------------------------------------------\
	|                                                                                            |
	|	对于流播放的文件可以不Release对象立即InitToData快速换文件播放                            |
	|                                                                                            |
	|	音频和视频流播放完了在UpDataPlay都会返回0给用户 以表示播放完毕                           |
	|                                                                                            |
	|	视频播放还可以和cFRAME动画筐类答配使用 以实现视频播放画面能象动画筐那样运动              |
	|	这时侯不用cVIDEO::UpdataPlay来显示画面 而是用cVIDEO::GetImage来为动画筐指定装入的图象    |
	|	用cVIDEO::UpdataToImage来更新文件流 再用cFRAME::PlayFrame来显示画面                      |
	|                                                                                            |
	\-------------------------------------------------------------------------------------------*/
}
GN_CALL void AppDisplay3D(unsigned int nTime){}//▲显示3D画面 程序运行每帧都调用它 

GN_CALL void AppRun(unsigned int nTime){}      //▲运行程序后台的处理 程序运行每帧都调用它
}
