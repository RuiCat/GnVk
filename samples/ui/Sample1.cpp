#include<GN.h>		   //使用GN库必需包含这个头文件

P<GNu::cPALETTE>			Palette;	//颜色版  !注意这个在全局区必须用指针 不能直接定义实例
GNu::cSLIDER				Slider;		//滑动条
//
extern "C"{
GN_CALL void AppInit(){     //▲初始化应用必须数据 程序开始准备好引擎系统后会调用它一次 
	/*这些初始化式样设置都可以通过编辑器调参数设置的 没耐心看可以跳过*/
	Palette.New(1);			//为颜色版分配空间
	Palette->InitToData(100, 50, 350, 350);	  //设置颜色版位置XY 宽度 高度
	Palette->SetOffsetEF(0.2f);				  //设置颜色版动效偏移速率

	Slider.img.New(1);		//为滑动条滑竿图分配空间
	Slider.img->InitToData("samples/ui/LoginUI/close");  //加载滑竿图
	Slider.Init(550, 60, 8, 160, 0xff777777);			//设置滑动底条位置XY 宽度 高度 颜色为灰色
	Slider.aIconColor = 0xff33aaff;						//滑竿图色调 天蓝色
	Slider.aIconH = 16;									//滑竿图显示高度
	Slider.aAmSheep	= 0.1f;								//滑动条动效速度比率 
}
GN_CALL void AppUnInit(){   //▲释放应用必须数据 程序运行结束前会调用它一次
	Palette.Delete();		//删除颜色版
	Slider.img.Delete();	//删除滑竿图
}
GN_CALL void AppDisplay2D(unsigned int nTime){ //▲显示2D画面 程序运行每帧都调用它 
	GNg::ImageDraw(0, GNg::ViewCur->ortho_left, GNg::ViewCur->ortho_top, GNg::ViewCur->orthoW, GNg::ViewCur->orthoH,0xff000000); //绘制一个视口大小的黑色矩形用以清屏
	GNu::cPALETTE *ple = Palette;			   //临时拿出颜色版裸指针以便访问更快
	ple->UpdateRun(nTime);					   //更新显示颜色版
	if(GN::Key1[GK_1]){						   //当按下键盘 1 键
		if(ple->aTable)ple->aTable.Delete();   //如已存在颜色表就删除它
		else ple->InitDefaultTable();		   //否则追加标准默认颜色表
	}
	if(GN::Key1[GK_2])ple->aAlphaSet = !ple->aAlphaSet;		//当按下键盘 2 键 颜色版透明调节条显示或隐藏
	if(GN::Key1[GK_3])ple->aColorTablePosType = !ple->aColorTablePosType;  //当按下键盘 3 键 颜色表靠内靠外切换
	if(GN::Key1[GK_R])ple->SetColor(0xffff0000);			//当按下键盘 R 键 设置颜色版为红色
	if(GN::Key1[GK_G])ple->SetColor(0xff00ff00);			//当按下键盘 G 键 设置颜色版为绿色
	if(GN::Key1[GK_B])ple->SetColor(0xff0000ff);			//当按下键盘 B 键 设置颜色版为蓝色
	short dx, dy, dw, dh;
	ple->GetPos(&dx, &dy, &dw, &dh);	//获得颜色版当前位置范围
	if(GN::Mouse1[GM_LEFT] && GNg::ViewCur->HitRange(dx, dy, dw, dh)) //当点击了鼠标左键且鼠标落在颜色版范围里
		Slider.c = ple->GetColor();		//就使滑条底色变为在颜色版里点中的颜色

	GNu::sUI_LEVEL uilv;	//定义首个父层级信息
	uilv.Init(GNg::ViewCur->ortho_left, GNg::ViewCur->ortho_top, GNg::ViewCur->orthoW, GNg::ViewCur->orthoH, 0xffffffff); //指定父层级的范围为视口大小 和它的颜色
	uilv.parent = 0;		//首个父层级没有父对象 就指定0
	GNu::RecvInputUiObject = &Slider;	//把滑动条设为当前可接收输入操作的UI组件 !但凡您手动调用需要指定父层级信息链参数的 UpdateRun 想接收输入必须手动设置 RecvInputUiObject
	Slider.UpdateRun(nTime, &uilv);		//更新显示滑动条
	float vol = Slider.GetVolume();		//获得当前滑动量
	static float vol1 = vol;			//备份滑动量 用于下一帧比较用
	if(GN_ValueNotEqualFloat(vol, vol1)){	//当滑动量有变更时  !浮点数要 GN_ValueNotEqualFloat 来判断  直接用 == 对于浮点数有时不准确
		vol1 = vol;								//再备份滑动量
		ple->h = 350 + (float)Slider.h * vol;	//使颜色版高度 等于 原颜色版高度加滑动高度
	}
}
GN_CALL void AppDisplay3D(unsigned int nTime){} //▲显示3D画面 程序运行每帧都调用它 

GN_CALL void AppRun(unsigned int nTime){}       //▲运行程序后台的处理 程序运行每帧都调用它
}