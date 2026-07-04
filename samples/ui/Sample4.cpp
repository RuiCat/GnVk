#include<GN.h>		   //使用GN库必需包含这个头文件

extern void (*GT_pFuncPreFrame)(unsigned int);  // pass前回调(GF_FrameCycleProc1)

P< P<char> >				 FBDirs;                 //文件浏览目录表 - 用于显示
P< P<char> >				 FBDirPaths;             //文件浏览目录表 - 用于查找
P<GNu::cSELECT_RECT>		 SelectRect;			 //选择筐
P<GNu::cPAGE_BAR>			 DirPageBar;			 //目录分页栏
P<GNu::cFILE_BROWSER>		 FileBrowser;            //文件浏览筐
P<GNu::cTEXT_INPUT>			 TextInput;				 //文本输入筐

//绘制勾选框(位置X, 位置Y, 名称, 修改值) 返回名称末尾的X坐标
short DrawCheckBox(short dx,  short dy, char *name, bool &value){
	GNg::LineFrame2dDraw(dx, dy, 14, 14, 1, 0xff777777);			  //绘制选框
	if(GN::Mouse1[GM_LEFT] && GNg::ViewCur->HitRange(dx, dy, 14, 14))value = !value; //当鼠标落在选框里且点击左键 就使值是与否切换
	if(value)GNg::ImageDraw(0, dx+3, dy+3, 8, 8, 0xff777777);		  //当值为是 就绘制勾选点
	dx += 20;														  //绘制位置右移20像素
	GN::RunRec.aFont.OutText(name, 0, dx, dy, 0, 0, 0xff777777);	  //在选框右侧输出 名称
	return dx + GN::RunRec.aFont.GetFontRangeX() * strlen(name);	  //返回绘制位置加名称占位宽度的位置
}
extern "C"{
static bool s_preFrameInitDone = false;
static void Sample4PreFrameInit(unsigned int nTime){
    if(s_preFrameInitDone) return;
    s_preFrameInitDone = true;
    (void)nTime;
    GNu::cFILE_BROWSER *fb = FileBrowser;
    fb->SetEffect(300);
    fb->InitStyleToData(0, 0, GNg::ViewCur->ortho_left, GNg::ViewCur->ortho_top+30, GNg::ViewCur->orthoW, GNg::ViewCur->orthoH - 60);
    fb->aFocusColor = 0x66000000; fb->aFocusBlend = GNg::eBLEND_NORMAL;
    fb->aShowOffsetX = 16; fb->aShowOffsetY = 4;
    fb->UsShowPreview(true);
}

GN_CALL void AppInit(){
	FBDirPaths = GNf::EnumSystemDirectory();
	FBDirs = GN_StrGetFileNames(FBDirPaths);
	DirPageBar.New(1);
	DirPageBar->Init(FBDirs, 0, 0, 30);
	FileBrowser.New(1);
	TextInput.New(1);
	TextInput->InitStyle();
	TextInput->InitInputFmtToData(GNu::eTX_IN_TEXT_LOCK, ".png,.txt", 32);
	TextInput->aTextColor = 0xbbffffff;
	SelectRect.New(1);
	SelectRect->Init();
	GT_pFuncPreFrame = Sample4PreFrameInit;  // pass前完成GPU init
}
GN_CALL void AppUnInit(){   //▲释放应用必须数据 程序运行结束前会调用它一次
	FileBrowser->WaitProcFlushReturn(); //通知文件浏览结束检索任务并等待执行结束
	SelectRect.Delete();		//删除选择筐
	TextInput.Delete();			//删除文本输入筐
	FileBrowser.Delete();		//删除文件浏览与它的数据
	DirPageBar.Delete();		//删除目录分页栏
	FBDirs.Delete();			//删除文件名空间
	FBDirPaths.Delete();		//删除路径空间
}
GN_CALL void AppDisplay2D(unsigned int nTime){ //▲显示2D画面 程序运行每帧都调用它 
    (void)nTime;
	GNg::cVIEW2D *view = GNg::ViewCur;
    GNu::cFILE_BROWSER *fb = FileBrowser;
	GNg::ImageDraw(0,view->ortho_left,view->ortho_top,view->orthoW,view->orthoH,0xff777777);
	return; // DEBUG: 简化渲染
	GNg::Draw2DAlphaDirectionSet(GNu::eNEAR_BOTTOM);			//使接下来的2D绘制都为向下渐淡的
	GNg::ImageDraw(0,view->ortho_left,view->ortho_top,view->orthoW,320, 0x66aa8822);		  //绘制一个半个屏大渐变土黄色块
	GNg::Draw2DAlphaDirectionSet(GNu::eNEAR_NULL);				//取消渐变绘制
	
	GNu::cPAGE_BAR *page_bar = DirPageBar;		//临时拿出分页栏的裸指针以访问更快
	short pw, ph;
	page_bar->GetOnecWH(pw, ph);				//获得单个页宽度和高度
	short px = GNu::NearLx(page_bar->GetNames().Num() * pw + 1); //计算到分页栏末右移1个像素的位置
	GNg::ImageDraw(0, px, view->ortho_top, view->ortho_right - px, 30,0xaa000000); //绘制右上角一块黑底
	page_bar->UpdateRun(GN::Mouse1[GM_LEFT], view->ortho_left, view->ortho_top, 0, 1, 0xffb3b3b3, 0xaa000000, 0xff807350); //更新显示分页栏
	px += 5;	//使绘制位置右移5像素
	GNg::ImageDraw(0, px, view->ortho_top+5, view->ortho_right-40-px, 20, 0x55000000); //绘制右上角的输入筐底
	if(GN::ViewReSized)TextInput->SetPos(px+5, view->ortho_top+7, view->ortho_right-50-px, 20);		//当视口变更了 就重设一下输入筐位置
	GN::RunRec.aFont.OutText("查找", 0, view->ortho_right-33, view->ortho_top+8, 0, 0, 0xffa08850);	//在右上角输出土黄色 查找 文字
	TextInput->UpdateRun(nTime);				//更新显示输入筐

	if(GN::Mouse1[GM_LEFT] && GNg::ViewCur->HitRange(view->ortho_right-35, view->ortho_top, 35, 30)){ //当鼠标落在[查找]里且点击左键
		if(!fb->IsBusy()){	 //文件浏览处于空闲时
			char *input_text = TextInput->GetTextData();  //获取输入筐的内容
			if(input_text){								  //有内容
				fb->aSearchTotalSizeB = 0;				  //清除上次查找统计到的大小
				fb->Search(false, 0, input_text, true);	  //按输入筐内容扩展名的格式来查找当前打开文件夹下匹配到的文件
				delete [] input_text;					  //删除获取出的内容
		}	}
	}
	static bool showFileBrowser = true;			//文件浏览是否展现
	static short lastSelectId = 0;				//分页栏最近选到项ID
	if(lastSelectId != page_bar->aSelect){		//分页栏选到的项变更了
		lastSelectId = page_bar->aSelect;		//记下变更后的项ID
		fb->SetOpenDirectory(FBDirPaths[lastSelectId]);  //用文件浏览打开项ID对应的目录路径
		showFileBrowser = true;					//确保文件浏览展现
	}
	if(GN::ViewReSized)fb->SetPos(view->ortho_left,view->ortho_top+30,view->orthoW,view->orthoH - 60);  //当视口变更了 就重设一下文件浏览筐位置
	if(GN::Mouse1[GM_RIGHT])fb->SelectClear();  //鼠标点右键取消所有选择
	if(showFileBrowser){	//文件浏览要展现时
		uint8 state = fb->UpdateRun(nTime);		//更新显示文件浏览 并获得它返回状态
		GN_RunValueCheck2(state, 2);			//显示返回状态值
		switch(state){							//检测返回状态 !文件浏览在单选模式下才有该值返回的
		case  1:{	//有选到文件通知时
			P< P<char> > click_files = fb->GetSelectFileNameData(); //获取当前选择到的路径文件名列表
			if(click_files.Num()){   //有有选到文件
				if(GN_SysShowFile(click_files[0]))showFileBrowser = false;  //调用系统适合的程序打开首个文件路径 来显示它 如打开成功就立即隐藏文件浏览
				//fb->OkHide();	 	 //带动画地隐藏文件浏览
			}
			}break;
		//case  2: showFileBrowser = false; break;  //隐藏动画播放完毕 就立即隐藏文件浏览
		}
		static float wait_ing = 0.0f;
		if(fb->IsBusy()){			//当文件浏览正在忙
			wait_ing += 0.005f;		//增加一点进度 以拖动 动画重复效果
			if(wait_ing > 0.06f){	//进度达到一定时
				GNg::ImageDraw(0,view->ortho_left,view->ortho_top,view->orthoW,view->orthoH,0x88000000);  //绘制一个淡黑色的矩型填满视口
				GN_RunProgressDraw(wait_ing, 2, true, GNu::NearCx(), GNu::NearCy(), 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 1); //在视口中间用白色显示引擎默认执行进度LOGO动画
			}
		}else wait_ing = 0.0f;		//不忙时就重置等待进度为0
	}
	short dx = view->ortho_left + 8, dy = view->ortho_bottom - 30;			//计算底栏内容起始绘制坐标
	GNg::ImageDraw(0, view->ortho_left, dy, view->orthoW, 30,0xaa000000);	//绘制底栏
	dy += 8;	//绘制坐标Y下移8像素
	if(fb->aUsMultiSelect){			 //文件浏览处于多选模式时
		if(GN::Key[GK_LCONTROL]){			//当按紧左Ctrl键时
			static GNg::sRECT *Rect = 0;
			GNg::sRECT *rect = SelectRect->UpdateRun(nTime, 0x66000000, GNg::eBLEND_NORMAL, 0xff888888);  //选框更新显示 并指定底色和边框色 再就获得选框范围矩形 
			if(Rect && !rect)fb->SelectIconFromRect(Rect);		//当上一帧有范围本帧没表示框选完松开了 就把选框范围传给文件浏览来激活选上文件
			Rect = rect;					//记下选框范围在下一帧用
			fb->aDisableControl = true;		//使文件浏览暂时不能操作
		}else if(GN::Key0[GK_LCONTROL])fb->aDisableControl = false;  //当左Ctrl键松开时 使文件浏览恢复可以操作

		GNg::ImageDraw(0, view->ortho_right - 55, dy-4, 47, 22, 0xff807350);				//绘制右下角的土黄色按钮底图
		GN::RunRec.aFont.OutText("打开", 0, view->ortho_right - 45, dy, 0, 0, 0xaa000000);	//在按钮中间输出淡黑色 打开 文字
		if(GN::Mouse1[GM_LEFT] && GNg::ViewCur->HitRange(view->ortho_right - 55, dy-4, 47, 22)){ //当鼠标落在按钮里且点击左键
			P< P<GNf::sFILE_INFO> > file_info = fb->GetSelectFileAndDirInfoData();	//获得当前选到的文件信息
			unsigned int fi = 0, fn = file_info.Num();
			for( ; fi < fn; ++fi)GN::RunRec.Print("%s\n", file_info[fi]->global_filename);  //历遍所有选到的文件 打印文件路径到消息提示里
			if(!GN::RunRec.PrintViewIsOn())GN::RunRec.PrintViewOnOff();				//当消息提示没打开就打开它
			fb->SelectClear();  //取消所有选择
		}
	}
	dx = DrawCheckBox(dx, dy, "多选",			fb->aUsMultiSelect) + 16;	//绘制 多选 勾选框 并使绘制坐标移到勾选框右边16像素处
	dx = DrawCheckBox(dx, dy, "禁止操作",		fb->aDisableControl)+ 16;	//绘制 禁止操作
	dx = DrawCheckBox(dx, dy, "查找忽略文件夹", fb->aIgnoreSED)		+ 16;	//绘制 查找忽略文件夹 勾选框
	char info_buf[32];
	int len = sprintf(info_buf, "总文件数:%d", fb->GetCurIconCount());		//按格式打印出当前目录下的文件夹和文件总数 一并获得这个字符串长度
	GN::RunRec.aFont.OutText(info_buf, len, dx, dy, 0, 0, 0xff777777);		//接在后面输出 总文件数
	dx += GN::RunRec.aFont.GetFontRangeX() * len + 16;						//使绘制坐标移到 总文件数 右边16像素处
	if(!strcmp(GN_StrGetFileName(fb->aOpenDir),"search_result")){			//当前正在浏览目录末尾为 search_result 时表示在虚构的目录下列出查找结果
		sprintf(info_buf, "查找文件总大小: %.02fKB", (float)fb->aSearchTotalSizeB / 1024.0f);	//按仅留两位小数的格式打印出查找得到文件总大小
		  GN::RunRec.aFont.OutText(info_buf, 0,     dx, dy, 0, 0, 0xff777777);	//接在后面输出 文件总大小
	}else GN::RunRec.aFont.OutText(fb->aOpenDir, 0, dx, dy, 0, 0, 0xff777777);	//接在后面输出 当前正在浏览的目录
}
GN_CALL void AppDisplay3D(unsigned int nTime){} //▲显示3D画面 程序运行每帧都调用它 

GN_CALL void AppRun(unsigned int nTime){}       //▲运行程序后台的处理 程序运行每帧都调用它
}