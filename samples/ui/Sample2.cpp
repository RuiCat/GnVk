#include<GN.h>		   //使用GN库必需包含这个头文件

GNg::sAM_EVENT *FontEv = 0;				//多个动画图框都共享的动画帧事件
GNg::cFRAME FontAm, FontAct;			//字体式样动画筐 和动态演变参照动画筐
GNg::cFRAME_LAUNCHER_FONT FLauncher;	//悬挂动画艺术字筐
GNg::cFRAME FontAm2, FontAct2;			//字体式样动画筐2 和动态演变参照动画筐2
GNg::cFRAME_LAUNCHER_FONT FLauncher2;	//十二生肖动画艺术字筐
unsigned int FTime = 0xffffffff;		//当前选到的十二生肖字筐当前时间
unsigned int FID   = 0;					//当前选到的十二生肖字筐ID
char *TextZodiac = "鼠牛虎兔龙蛇马羊猴鸡狗猪"; //十二生肖文本

extern "C"{
GN_CALL void AppInit(){     //▲初始化应用必须数据 程序开始准备好引擎系统后会调用它一次 
	/*这些初始化式样设置都可以通过编辑器调参数设置的 没耐心看可以跳过*/
	FontEv = new GNg::sAM_EVENT[6];		//分配帧事件空间
	FontEv[0].Init(GNg::eEVENT_ROTATION,0,1000,50.0f*GN_ANGLE_UNIT,0.0f);	//设为旋转帧事件 1秒内完成50度角到0度角的旋转
    FontEv[1].Init(GNg::eEVENT_SIZE_X|GNg::eEVENT_SIZE_Y,0,600,1.0f,1.5f);	//设为整体缩放帧事件 600毫秒内完成原大小到1.5倍大小的变化
	FontEv[2].Init(GNg::eEVENT_SIZE_X|GNg::eEVENT_SIZE_Y,0,500,3.0f,1.0f);	//设为整体缩放帧事件 500毫秒内完成3倍大小到原大小的变化
	FontEv[3].Init(GNg::eEVENT_MOVE_Y,0,500,0.0f,-120.0f);					//设为Y坐标移动帧事件 500毫秒内完成向上移动120像素
	FontEv[4].Init(GNg::eEVENT_ROTATION,0,500,0.0f, 180.0f);				//设为旋转帧事件 500毫秒内完成0度角到180度角的旋转
	FontEv[5].Init(GNg::eEVENT_SIZE_X|GNg::eEVENT_SIZE_Y,0,500,1.0f,0.3f);  //设为整体缩放帧事件 500毫秒内完成原大小到0.3倍大小的变化

	//初始化动画筐 (使用到字体图或普通图序, 动画事件个数,动画事件数组, 变换信息(x,y), 图心点对位x,y,   混合模式发光,	     颜色,      循环播放)
	FontAm .Init(GN::RunRec.aFont.GetFontMap(), 1,			FontEv,      GNg::Ts2D,		 0.0f,0.0f,  GNg::eBLEND_NORMAL,  0xffffffff,  GNg::eAM_REPEAT);	//为字筐指定字体图为系统默认的 并指定旋转动画帧事件给它来回播放
	FontAct.Init(0,							    1,		    FontEv+1,   GNg::Ts2D,		 0.0f,0.0f,  GNg::eBLEND_NORMAL,  0xffffdd00,  GNg::eAM_ONCE_KEEP); //指定整体缩放帧事件给动态演变参照筐 要求它播放完就定住
	GNg::sARRANGE_FMT afmt(0,0.0f,0.0f,1,1,24,23);		//定义一个 行列间距都为1 字宽为24 字高为23的排列格式
	GNg::sRECT2 range(50.0f, 50.0f, 350.0f, 150.0f);	//动画艺术字筐显示范围
	FLauncher.InitToData(&FontAm,"点击变星星⑴⑵⑶⑷⑸",0,&range,&afmt,0.0f,200,1,127,0,127,127,8,0,0,&FontAct); //设置动画艺术字筐用到的字筐和动态演变参照筐 范围 排版格式, 并预设显示的文本内容且要求先显示前8个字符

	FontAm2 .Init(GN::RunRec.aFont.GetFontMap(), 2,			FontEv+2,   GNg::Ts2D,		 0.5f,0.5f,  GNg::eBLEND_NORMAL,  0xffffffff,  GNg::eAM_ONCE_KEEP);	//为字筐指定字体图为系统默认的 并指定整体缩放和上移动画帧事件给它播完就定住
	FontAct2.Init(0,							 2,		    FontEv+4,   GNg::Ts2D,		 0.0f,0.0f,  GNg::eBLEND_NORMAL,  0xffffdd00,  GNg::eAM_ONCE_KEEP); //指定整体缩放和旋转180度帧事件给动态演变参照筐 要求它播放完就定住
					afmt.Init(0,0.5f,0.5f,-12,1,24,23);	//定义一个以字中心为旋转 行间距-12 列间距1 字宽为24 字高为23的排列格式
				range.Init(400.0f, 380.0f, 0.0f, 0.0f);	//十二生肖动画艺术字筐显示范围
	FLauncher2.InitToData(&FontAm2,TextZodiac,0,&range,&afmt,360.0f,70,1,127,22,127,127,0,0,0,&FontAct2); //设置十二生肖动画艺术字筐用到的字筐和动态演变参照筐 范围 排版格式, 360度向外全方位发射 并预设十二生肖文本内容且要求全部显示 
	FLauncher2.ControlActFromMouseAuto(true);  //启用鼠标自动控制十二生肖动画艺术字筐动态演变
}
GN_CALL void AppUnInit(){   //▲释放应用必须数据 程序运行结束前会调用它一次
	GN_DeleteArray(FontEv);				//安全删除动画帧事件 !由于使用它的动画筐不保管它 所以需手动删除
}
GN_CALL void AppDisplay2D(unsigned int nTime){ //▲显示2D画面 程序运行每帧都调用它 
	GNg::ImageDraw(0, GNg::ViewCur->ortho_left, GNg::ViewCur->ortho_top, GNg::ViewCur->orthoW, GNg::ViewCur->orthoH,0xff000000); //绘制一个视口大小的黑色矩形用以清屏

	if	   (GN::Key[GK_W])FLauncher.Move( 0,-2);		//当按下键盘W键 上移一下动画艺术字筐
	else if(GN::Key[GK_S])FLauncher.Move( 0, 2);		//当按下键盘S键 下移一下动画艺术字筐
    if	   (GN::Key[GK_A])FLauncher.Move(-2, 0);		//当按下键盘A键 左移一下动画艺术字筐
	else if(GN::Key[GK_D])FLauncher.Move( 2, 0);		//当按下键盘D键 右移一下动画艺术字筐
	
	if(GN::Mouse1[GM_LEFT])FLauncher.SetSelectPoint(GNg::ViewCur->mouseMX,GNg::ViewCur->mouseMY); //当点下鼠标左键 设置选择点 以便 PlayFrame()下次播放时要顺便帮找出这个点 点选中了那个字筐 
	FLauncher.PlayFrame(nTime);		//更新播放悬挂动画艺术字筐
	unsigned int fid;
	if(FLauncher.GetSelectFontFrame(&fid))FLauncher.SetSubText(fid,"★");	//当刚才设置的选点有点到字筐 就收取字筐ID 并设置该ID对应的字筐变为★

	GNg::LineFrame2dDraw(FLauncher.aGenRngX,FLauncher.aGenRngY,FLauncher.aGenRngW,FLauncher.aGenRngH,1,0x77aa5500); //绘制显示动画艺术字筐范围
	GNg::sRECT rng;
	FLauncher.GetTextRange(&rng);	//获取确定排好的所有字筐所占范围
	GNg::LineFrame2dDraw(rng.left,rng.top,rng.GetWidth(),rng.GetHeight(),1,0x33aa5500); //绘制显示排好的所有字筐所占范围
	GNg::sQUADS2 qu4;
	if(FLauncher.GetFontFrameRange(5,&qu4)){	//获取第5个字筐的判断范围四边形
		GNg::sVEC2 p4[4];
		GNg::RangeQuadsGetAnglePoint(qu4, p4);	//计算出四边形的4个角点
		GNg::ImageDraw4P(0, (float*)p4, 0x50dda000, 0, GNg::eBLEND_NORMAL, 0.0f, 0.0f, 1.0f, 1.0f); //绘制该字筐的判断范围
	}
	if(GN::Key1[GK_E])FLauncher.SetSubText(5,"〓〓〓");	//当按下键盘E键 使由第5个字筐起之后的三个字筐设为〓
	if(GN::Key1[GK_R])FLauncher.SetText("点击变星星",0);//当按下键盘R键 重设动画艺术字筐显示文本
	if(GN::Key1[GK_1])FLauncher.SelectActChange(1);		//当按下键盘1键 使第1个字筐向参照动画筐演变
	if(GN::Key1[GK_2])FLauncher.SelectActChange(2);		//当按下键盘2键 使第2个字筐向参照动画筐演变
	if(GN::Key1[GK_3])FLauncher.CancelActChange(1);		//当按下键盘3键 取消第1个字筐动态演变 复位
	if(GN::Key1[GK_4])FLauncher.CancelActChange(2);		//当按下键盘4键 取消第2个字筐动态演变 复位

	if(GN::Key1[GK_LEFT] || GN::Key1[GK_RIGHT]){	//当按下键盘左键或右键 ----------------------- 以下是十二生肖动画艺术字筐的处理
		FLauncher2.SetSubColor(FID, 0xffffffff);		//设置之前选到的字筐颜色为白色
		if(GN::Key1[GK_LEFT]){	//按下的是左键
			if(FID)--FID;			//当前选到字筐ID不为0时就使它减1
			else FID = 11;			//如是0时就使它轮回为最大ID
		}else{					//按下的是右键
			if(FID < 11)++FID;		//当前选到字筐ID不为最大时就使它加1
			else FID = 0;			//否则让它回到0
		}
		FLauncher2.SetSubColor(FID, 0xff56aaff);		//设置当前选到的字筐颜色为天蓝色
	}
	if(FTime == 0xffffffff){	//当前选到的字筐当前时间无效时
		if(GN::Key1[GK_RETURN])FTime = FLauncher2.GetFontFrame()->GetEndtime(); //当按下键盘回车键 使当前选到的字筐当前时间跳到它的结束处
	}else{						//当为有效时 字筐就渐渐跑回原处
		if(FTime > nTime)FTime -= nTime;				//当字筐当前时间大于要减去的时间就减去它
		else FTime = 0;									//否则设当前时间为0
		FLauncher2.SetSubNowtime(FID, FTime);			//设置当前选到的字筐播放时间为刚才递减过的时间
		if(GN::Key1[GK_RETURN])FTime = 0xffffffff;		//当按下键盘回车键 使当前选到的字筐当前时间变为无效
	}
	if(GN::Key1[GK_H])FLauncher2.ToHide();				//当按下键盘H键 就动效隐藏艺术字筐
	else if(GN::Key1[GK_P]){							//当按下键盘P键
		FLauncher2.aColor_range -= 9;					//使颜色变化幅度减少9
		FLauncher2.SetText(TextZodiac,0);				//重动效弹出艺术字筐
	}
	FLauncher2.PlayFrame(nTime);						//更新播放十二生肖动画艺术字筐
	GN_RunValueCheck2(FLauncher2.aColor_range, 5);		//显示十二生肖艺术字筐颜色变化幅度
}
GN_CALL void AppDisplay3D(unsigned int nTime){} //▲显示3D画面 程序运行每帧都调用它 

GN_CALL void AppRun(unsigned int nTime){}       //▲运行程序后台的处理 程序运行每帧都调用它
}