#include<GN.h>		   //使用GN库必需包含这个头文件

P<GNu::cUI_SCENE>			  SceneUi;		 //定义一个UI场景
P<GNu::cUI_GROUP>			  DlgLogin;		 //登录对话筐组
GNu::cUI_SELECT_BUTTON		 *BtLogin;		 //登陆与注册按钮联
P<GNu::sUI_IMAGE>			  Picture;		 //用户头像		 该类只默认以左上靠位
P<GNu::cUI_IMAGE>			  ScanCode;		 //扫码图		 该类支持多种方式靠位
GNu::cUI_PTR				  ScanCodePtr;	 //扫码图UI地址 用于方便把它拆出来
GNu::cUI_PTR				  PicturePtr;	 //用户头像UI地址 用于移动图层
GNu::cUI_PTR				  PicturePtr2;	 //最近向前复制出的用户头像UI地址
GNu::cUI_PTR				  PicturePtr3;	 //最近向后复制出的用户头像UI地址

extern "C"{
GN_CALL void AppInit(){     //▲初始化应用必须数据 程序开始准备好引擎系统后会调用它一次 
	GN_AppDirSet("samples/ui/LoginUI");		 //设置用户应用资源目录
	SceneUi.New(1);							 //创建UI场景空间
	
	P<GNu::cUI_IMAGE>  bg(1);				 //创建背景
	GNu::cUI_IMAGE *_uimg = bg;				 //拿出它的安全指针以访问得更快
	_uimg->img.New(1);						 //为它里面的图像分配空间
	_uimg->img->InitToData(GN_AppDir("bg")); //加载应用资源目录里的 bg0000.png 图片为背景图
	_uimg->bmode = GNg::eBLEND_NORMAL;		 //使用标准混合模式
	_uimg->ntype_x = GNu::eNEAR_SCALE;		 //横向靠边方式为按比列拉伸
	_uimg->x = 0; _uimg->nmin_x = 0;		 //横向两端的边距都为0
	_uimg->ntype_y = GNu::eNEAR_SCALE;		 //竖向靠边方式为按比列拉伸
	_uimg->y = 0; _uimg->nmin_y = 0;		 //竖向两端的边距都为0		!这样就能使背景图拉伸占满视口
	SceneUi->aSubs.push_back(bg);			 //把背景图加入到UI场景里

	DlgLogin.New(1);						 //创建登录对话筐UI组
	GNu::cUI_GROUP *_login = DlgLogin;		 //拿出它的安全指针以访问得更快
	_login->x  = 232;   _login->y  = 132;	 //位置坐标为离视口左边 232像素  离视口上边 132像素 !所有UI组件默认靠边方式都为靠左上
	_login->lw = 128;   _login->lh = 128;	 //指定UI组最小大小的宽高 如组里的UI组件总占范围大于这个宽高 UI组就会被撑大 !另外UI组您无需设置w h 因为这个会被撑大改写
	SceneUi->aSubs.push_back(DlgLogin);		 //把登录对话筐UI组加入到UI场景里

	P<GNu::cUI_BACK_FRAME>  bf(1);			 //常见底框
	GNu::sUI_IMAGE *_img1 = bf;				 //拿出它的安全指针以访问得更快
	_img1->img.New(1);						 //为它里面的图像分配空间
	_img1->img->InitToData(GN_AppDir("[9]dlgframe"), 9); //加载圆角底框9宫格图
	_img1->bmode = GNg::eBLEND_NORMAL;		 //使用标准混合模式
	_img1->Init(0, 0, 340, 314,  0xa5fff7dd);//设置底框左上边距为0 0 宽度为340 高度为314  奶黄色
	bf->aDragBarH = 26;						 //设置底框顶端拖动栏高为26像素 这样就可以拖动包含它的上层UI组
	bf->aZoom = true;						 //设置可拖拉边框缩放上层UI组
	_login->aSubs.push_back(bf);			 //把底框加入到UI组

	P<GNu::cUI_IMAGE> angle_left(1);		 //创建标题栏左边角图
	_uimg = angle_left;						 //拿出它的安全指针以访问得更快
	_uimg->img = bf->img;					 //使它共用底框9宫格图的首个图像
	_uimg->bmode = GNg::eBLEND_NORMAL;		 //使用标准混合模式
	_uimg->Init(2, 2, 0, 0, 0xff866934);	 //设置它左上边距为2 2 宽度为原图宽 高度为原图高  咖啡色
	_login->aSubs.push_back(angle_left);	 //把它加入到UI组

	P<GNu::cUI_IMAGE> angle_right(1);		 //创建标题栏右边角图
	_uimg = angle_right;					 //拿出它的安全指针以访问得更快
	_uimg->img = bf->img;					 //使它共用底框9宫格图的首个图像
	_uimg->bmode = GNg::eBLEND_NORMAL;		 //使用标准混合模式
	_uimg->ntype_x = GNu::eNEAR_RIGHT_BOTTOM;//横向排位设为靠右方式
	int angle_w = bf->img->width;			 //圆角底框图宽
	_uimg->Init(2+angle_w, 2, -angle_w, 0, 0xff866934);	//设置靠左边距为2 再加那个宽度是因为图要翻转绘制用来修正位置的  靠上边距为2 宽度为负图宽就可以翻转绘制 高度为原图高  咖啡色
	_login->aSubs.push_back(angle_right);	 //把它加入到UI组

	P<GNu::cUI_IMAGE> bar1(1);				 //创建标题栏顶边图
	_uimg = bar1;
	_uimg->bmode = GNg::eBLEND_NORMAL;		 //使用标准混合模式
	_uimg->ntype_x = GNu::eNEAR_SCALE;		 //横向排位设为拉伸方式   !这个方式您可不设置宽度 w
	_uimg->nmin_x = 2+angle_w;				 //横向末端边距为2+圆角底框图宽
	_uimg->Init(2+angle_w, 2, 0, angle_w, 0xff866934);	//设置靠左边距为2+圆角底框图宽 靠上2 宽度为0 高度为圆角底框图宽  咖啡色
	_login->aSubs.push_back(bar1);			 //把它加入到UI组

	P<GNu::cUI_IMAGE> bar2(1);				 //创建标题栏下边图
	_uimg = bar2;
	_uimg->bmode = GNg::eBLEND_NORMAL;		 //使用标准混合模式
	_uimg->ntype_x = GNu::eNEAR_SCALE;		 //横向排位设为拉伸方式  
	_uimg->nmin_x = 2;						 //横向末端边距为2
	_uimg->Init(2, 2+angle_w, 0, 12, 0xff866934);	    //设置靠左边距为2 靠上2+圆角底框图宽 宽度为0 高度为12  咖啡色 
	_login->aSubs.push_back(bar2);			 //把它加入到UI组

	Picture.New(1);							 //创建用户头像图
	_img1 = Picture;						 //拿出它的安全指针以访问得更快
	_img1->img.New(1);						 //为它里面的图像分配空间
	_img1->img->InitToData(GN_AppDir("photo.png")); //加载头像图
	_img1->bmode = GNg::eBLEND_NORMAL;		 //使用标准混合模式
	_img1->Init(39, 44, 0, 0,  0xe6283c20);  //设置靠左边距为39 靠上44 宽度为原图宽 高度为原图高  鸭屎绿色 
	_login->Comment(Picture, &PicturePtr);	 //把它加入到UI组 并获取到它在组里的地址
	PicturePtr2 = PicturePtr3 = PicturePtr;	 //使前后复制源UI地址都先等于首个用户头像UI地址

	ScanCode.New(1);						 //创建扫码图
	_uimg = ScanCode;						 //拿出它的安全指针以访问得更快
	_uimg->img.New(1);						 //为它里面的图像分配空间
	_uimg->img->InitToData(GN_AppDir("code.png")); //加载扫码图
	_uimg->bmode = GNg::eBLEND_MODULATE;	 //使用调和混合模式
	_uimg->Init(0, 42, 122, 120, 0xfffff6de); //设置靠上边距为42 宽度为122 高度为120  梨黄色 
	_uimg->ntype_x = GNu::eNEAR_NONE;		 //设置横向不靠边 就按所在UI组里的百分比而定坐标
	*((unsigned short*)&_uimg->x) = (unsigned short)(65535.0f * 0.72f); //用这样强制转换数值来指定X坐标落在UI组里72%处 65535是 unsigned short 的最大值即百分百
	_login->Comment(ScanCode, &ScanCodePtr); //把它加入到UI组 并获取到它在组里的地址

	P<GNu::cUI_TEXT_ART> caption(1);		 //创建标题静态文本
	GNu::cUI_TEXT_ART *_caption = caption;
	_caption->Init(0, 11, 0, 0, 0xbbffffff); //仅指定离顶11像素 和颜色 !该类无需您手动指定宽高 w h
	_caption->ntype_x = GNu::eNEAR_NONE;	 //设置横向不靠边
	*((unsigned short*)&_caption->x) = (unsigned short)(65535.0f * 0.5f);  //在UI组里50%处 即居中显示
	GN_StrcpyData(_caption->text, "扫码登录"); //复制扫码登录字符串给文本内容
	_caption->InitDefault();				 //初始化静态文本其它值为默认
	_login->aSubs.push_back(caption);		 //把它加入到UI组

	P<GNg::sIMAGE>	font_map(1);			 //创建字体图像
	font_map->InitToData(GN_AppDir("fontmap_"));	//加载字体图
	SceneUi->aFontMapList.push_back(font_map);		//把字体图交给UI场景保管 !因为静态文本式样是裸指针引用 自己不保管文本式样数据
	GNg::sARRANGE_FMT afmt(false);					//非填满筐的排列格式
	afmt.width = 9;  afmt.height = 12;				//指定排列的字宽和高
	P<GNg::cTEXT_ART> text_style(1);		 //创建1个新文本式样
	text_style->Init(font_map, &afmt);				//为文本式样指定字体图和排列格式
	SceneUi->aTextStyleList.push_back(text_style);  //把文本式样交给UI场景保管

	P<GNu::cUI_TEXT_ART> user_id(1);		//创建用户名静态文本
	GNu::cUI_TEXT_ART *_user_id = user_id;  //拿出它的安全指针以访问得更快
	_user_id->x = 40;						//离左40像素
	_user_id->ntype_y = GNu::eNEAR_NONE;	//竖向不靠边 就按所在UI组里的百分比而定坐标
	*((unsigned short*)&_user_id->y) = (unsigned short)(65535.0f * 0.6f); //用这样强制转换数值来指定Y坐标落在UI组里60%处 65535是 unsigned short 的最大值即百分百
	GN_StrcpyData(_user_id->text, "USER ID  :");  //复制用户名字符串给文本内容
	_user_id->InitDefault();					  //初始化静态文本其它值为默认
	_user_id->textstyle = text_style;		//指定文本式样
	_login->aSubs.push_back(user_id);		//把这个UI组件加入到UI组

	P<GNu::cUI_TEXT_ART> password = user_id->Clone();	//复制出另一个静态文本
	GNu::cUI_TEXT_ART  *_password = password;			//拿出它的安全指针以访问得更快
	*((unsigned short*)&_user_id->y) = (unsigned short)(65535.0f * 0.735f);   //指定Y坐标落在UI组里73%处
	GN_StrcpyData(_user_id->text, "PASSWORD :");  //复制密码名字符串给文本内容
	_login->aSubs.push_back(password);		//把这个UI组件加入到UI组

	P<GNu::cUI_BACK_FRAME>  inframe1(1);	//创建输入筐的圆角边筐
	_img1 = inframe1;
	_img1->img.New(1);						//为它里面的图像分配空间
	_img1->img->InitToData(GN_AppDir("[9]bubbleBF"), 9); //加载圆角边筐9宫格图
	_img1->bmode = GNg::eBLEND_NORMAL;		//使用标准混合模式
	_img1->Init(138, 168, 160, 36,  0x42ffffff);	//设置靠左边距为18 靠上168 宽度为160 高度为36  淡白色 
	_login->aSubs.push_back(inframe1);		//把这个UI组件加入到UI组

	P<GNu::cUI_BACK_FRAME>  inframe2 = inframe1->Clone();  //复制出另一个底筐
	inframe2->y += 42;						//另一个底筐显示位置往下移42像素
	_login->aSubs.push_back(inframe2);		//把这个UI组件加入到UI组

	P<GNu::cUI_TEXT_INPUT> input_text1(1);				//创建文本输入筐
	GNu::cUI_TEXT_INPUT  *_input_text1 = input_text1;
	_input_text1->InitStyle();							//初始化为默认式样
	_input_text1->Init(145, 177, 146, 30, 0xca63390b);  //设置靠左边距为145 靠上177 宽度为146 高度为30  咖啡色 
	_input_text1->aTextColor = 0xca63390b;				//设置文本色为咖啡色 
	_input_text1->InitInputFmtToData(GNu::eTX_IN_TEXT_LOCK, "input user id", 22); //设置输入格式为锁定文本筐内容不能滚动 且预设内容和设置最大可输入字符数
	_login->aSubs.push_back(input_text1);		//把这个UI组件加入到UI组

	P<GNu::cUI_TEXT_INPUT> input_text2 = input_text1->Clone();  //复制出另一个输入筐
	input_text2->y += 44;						//另一个输入筐显示位置往下移44像素
	input_text2->InitInputFmtToData(GNu::eTX_IN_PASSWORD, 0, 22);				  //设置输入格式为密码输入
	_login->aSubs.push_back(input_text2);		//把这个UI组件加入到UI组

	P<GNg::sIMAGE> sb_img(1);					//创建按钮图像
	sb_img->InitToData(GN_AppDir("[2]Login"),2); //读取两个按钮图像
	P<GNu::cUI_SELECT_BUTTON> sb(1);			//创建选择按钮联
	BtLogin = sb;								//拿出它的安全指针以访问得更快
	BtLogin->GNu::cSELECT_BUTTON::Init(sb_img); //指定按钮图像
	BtLogin->ntype_y = GNu::eNEAR_RIGHT_BOTTOM; //指定为竖向靠下方式
	BtLogin->x = 46;  BtLogin->y = 22;			//离左46像素 离下22像素
	BtLogin->aStride = 96;						//每选项跨距为96像素
	_login->aSubs.push_back(sb);				//把这个UI组件加入到UI组
}
GN_CALL void AppUnInit(){   //▲释放应用必须数据 程序运行结束前会调用它一次 
	SceneUi.Delete();   //删除场景和它包含的所有数据
	DlgLogin.Delete();  //删除登录对话筐组
	Picture.Delete();   //删除用户头像		
	ScanCode.Delete();	//删除扫码图	
}
GN_CALL void AppDisplay2D(unsigned int nTime){ //▲显示2D画面 程序运行每帧都调用它 
	if(GN::ViewReSized)SceneUi->Resize(GNg::ViewCur);		//当视口大小变更时就重设一下UI场景区域大小 以便各UI组件能适配停靠
	SceneUi->UpdatePlay(nTime);		//更新播放UI场景
	GN_RunValueCheck2(BtLogin->aSelect, 0);		//显示登录按钮联的当前选择ID
	GN_RunValueCheck2(ScanCode.Us(),	1);		//显示扫码图被引用数

	if(GN::Key1[GK_1]){ //当按下键盘 1 键
		if(ScanCodePtr.aPtr)ScanCodePtr.UnComment();	  //如扫码图在UI组里就把它解除出来
		else DlgLogin->Comment(ScanCode, &ScanCodePtr);   //否则把扫码图接入到UI组里 并获取回它的新地址
	}
	if(PicturePtr.aPtr != PicturePtr2.aPtr && PicturePtr.aPtr != PicturePtr3.aPtr){  //当前后都复制有头像图时
		if	   (GN::Key1[GK_W])PicturePtr.OrderMove( 1);	//当按下键盘 W 键使原头像图层向前移动一层
		else if(GN::Key1[GK_S])PicturePtr.OrderMove(-1);	//当按下键盘 S 键使原头像图层向后移动一层
	}
	if(GN::Key1[GK_2]){ //当按下键盘 2 键
		P<GNu::sUI_IMAGE> pic = PicturePtr2.aPtr[0]->Clone();  //用最前的头像图复制一个副本
		pic->c = GNg::GetColorChange(51);	//给副本一个幻彩变化颜色
		pic->y += 15;						//使副本下移15像素
		PicturePtr2.InsertFront(pic, &PicturePtr2);		//把副本插入到它复制源的前面 并获取回副本所在地址 成为当前最前的头像图
	}else if(GN::Key1[GK_3]){ //当按下键盘 3 键
		P<GNu::sUI_IMAGE> pic = PicturePtr3.aPtr[0]->Clone();  //用最后的头像图复制一个副本
		pic->c = GNg::GetColorChange(51);	//给副本一个幻彩变化颜色
		pic->y -= 15;						//使副本上移15像素
		PicturePtr3.InsertBack (pic, &PicturePtr3);		//把副本插入到它复制源的后面 并获取回副本所在地址 成为当前最后的头像图
	}
}
GN_CALL void AppDisplay3D(unsigned int nTime){} //▲显示3D画面 程序运行每帧都调用它 

GN_CALL void AppRun(unsigned int nTime){}       //▲运行程序后台的处理 程序运行每帧都调用它
}