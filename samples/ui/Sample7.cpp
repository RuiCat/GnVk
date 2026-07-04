#include<GN.h>		   //使用GN库必需包含这个头文件

P<GNu::cUI_SCENE>			  SceneUi;		 //定义一个UI场景
GNu::sUI_FRAME_RC			 *DlgLoginAmin;	 //登陆对话筐里作为动效的图筐
GNu::cUI_GROUP				 *DlgProps;		 //道具面板对话筐
GNu::cUI_GROUP				 *DlgCombine;	 //道具合并对话筐
GNu::cUI_GROUP				 *PageClause;	 //条款页面
GNu::cUI_TEXT_INPUT			 *InUser;		 //用户名文本输入筐
GNu::cUI_TEXT_ART			 *UserID;		 //用户名静态文本
GNu::cUI_TEXT_ART			 *Sortie;		 //出击数静态文本
GNu::cUI_TEXT_ART			 *Props;		 //道具数静态文本
GNu::cUI_TEXT_ART			 *Info;			 //道具合并信息静态文本
GNu::cUI_FRAME_LAUNCHER		 *FLauncher;	 //散开粒子特效
GNu::cSELECT_LIST_ICON		 *Icons;		 //道具图标选择列表
GNu::cSELECT_LIST_TEXT		 *Names;		 //道具名文本选择列表
GNu::cUI_SELECT_BUTTON		 *BtLogin;		 //登陆与注册按钮联
GNu::cUI_SELECT_BUTTON		 *BtCombine;	 //合并按钮
P<GNu::cUI_SELECT_BUTTON>	  BtYes;		 //Yes按钮
GNu::cUI_PTR				  BtYesPtr;		 //Yes按钮所在地址
GNu::cUI_SELECT_BUTTON		 *BtClose1;		 //道具面板的[X]关闭按钮
GNu::cUI_SELECT_BUTTON		 *BtClose2;		 //道具合并的[X]关闭按钮
GNu::cSELECT_LIST_ICON		 *Agree;		 //同意勾选图标选择列表  !虽然只有1个图标 但是用这个功能更方便
GNu::cUI_PAGE_BAR			 *BtOk;			 //确定同意按钮 !这里用到的是分页栏功能来做 因为它支持文字式点选
GNu::cUI_FRAME_LAUNCHER_FONT *Tips;			 //需同意条款的提示
GNu::cUI_FRAME_LAUNCHER_FONT *Selected;		 //列出已选道具的动画艺术图筐
int							  SortieNum = 0; //出击数
namespace GN {
struct sVALUE_LINEAR_TASK : public GN_sTASK_BASE, public GNg::sAM_EVENT   //继承任务基类定义自己的 来回线性插值任务
{
	short			*pvalue;				 //指向需插值的变量指针

	virtual bool Run(){ //自定义任务的处理方法
		unsigned int am_time = GN::TimeGetRunCount() - start_time;  //计算到动画经历时间
		*pvalue = GNg::SLinear(start_value, end_value, am_time, segment_time, 0.83f, GNg::L_START|GNg::L_DOUBLE); //根据经历时间用缓动来回线性插值计算得到子对话筐的动画Y坐标
		return am_time >= segment_time;	//经历时间到尾了就通知丢弃该任务
	}
};
//变量来回更变插值(要修改的变量指针, 插值的开始值, 结束值, 插值所需时间)
void ValueChangeRepeat(short *_pv, float _start_value=100.0f, float _end_value=0.0f, unsigned int _seg_time=600){
	P<sVALUE_LINEAR_TASK> task(1);       //新建1个来回线性插值任务
	sVALUE_LINEAR_TASK *_task = task;    //临时拿出裸指针可访问得更快
	_task->pvalue = _pv;			     //指定要插值的变量指针
	_task->start_value = _start_value;  _task->end_value = _end_value; //指定插值的开始值与结束值
	_task->segment_time = _seg_time; _task->start_time = GN::TimeGetRunCount(); //指定插值所需时间 和记录任务开始时间
	GN_TaskPushToIdleExec(task, false);  //存入这个任务给每帧闲时执行
}
};
void ColorToHide(void *color){
	*((unsigned int*)color) &= 0x00ffffff; //去掉颜色不透明值 使相关的UI组隐藏起来 这样隐藏的UI组也不怎么消耗性能 
}
void UiActionProc(GNu::iUI_OBJECT *uobj, int action){  //自定义的UI组件交互操作处理
	if(uobj == BtLogin){ //由登陆按钮联触发的回调
		if(action == 1 || action == 2){  //点击的是第一或第二个按钮
		  if((PageClause->c & 0xff000000) == 0){	   //当条款页面UI组的不透明度为0 即正在隐藏中
			DlgLoginAmin->SwapAmStartEnd();			   //交换登陆对话筐动画事件起始结束值 这样就可以使动画显现变隐藏
			DlgLoginAmin->ReSetPlaytime();			   //重播放动画
			if(action == 1){			 //点击的是第一个按钮 LOGIN
				GN::ValueChangeColor(&(DlgProps->c), 0xffffffff);			//插值道具面板对话筐UI组的不透明度为最大 使它渐渐显现出来
				GN::ValueChange(&(DlgProps->y), (short)(DlgProps->y-100));	//插值道具面板对话筐UI组的Y坐标 使它渐渐往上移动100个像素
				char *us_name = InUser->GetTextData(); //从文本输入筐里获得用户输入名
				GN_DeleteArray(UserID->text);		   //删除原有用户名文本内容
				UserID->text = us_name;				   //引用从输入筐里获得的用户名
				char *num_pos = strchr(Sortie->text,':') + 2;  //扫描出击数静态文本 找到 : 号再进两个字符的位置 即数字区的起始位置
				itoa(++SortieNum, num_pos, 10);		   //按十进制形式转换出击数为字符串 并存入改写刚才数字区位置的内容 之后出击数增加1
					  num_pos = strchr(Props->text, ':') + 2;  //扫描道具数静态文本 找到 : 号再进两个字符的位置
				sprintf(num_pos,"%03d",Icons->GetZoningCount()); //以三位数不够补0的形式打印道具数为字符串 并存入改写刚才数字区位置的内容
			}else{						 //点击的是第二个按钮 REGISTER
				GN::ValueChange(&(PageClause->x), (short)0);   //插值条款页面的X坐标使它渐渐从右边移到坐标0处
				PageClause->c |= 0xffffffff;				   //使条款页面的总颜色变为不透明的
				Agree->aSelected = 0;						   //同意勾选图标设为未勾选
				Tips->SetNowtime(1000000);					   //使同意条款提示的动画时间跳到最末 让它先隐藏起来
			}
		  }
		}
	}else if(action == 1){		//其它点击到首个按钮的情况
		if(uobj == BtClose1){				//点击的是道具面板的[X]关闭按钮
			GN::ValueChangeColor(&(DlgProps->c), 0x00ffffff);			//插值道具面板对话筐UI组的不透明度为0 使它渐渐隐藏起来 这样隐藏的UI组也不怎么消耗性能 您无需要把它解除出来
			GN::ValueChange(&(DlgProps->y), (short)(DlgProps->y+100));	//插值道具面板对话筐UI组的Y坐标 使它渐渐往下移动100个像素
			DlgLoginAmin->SwapAmStartEnd();								//交换登陆对话筐动画事件起始结束值 使动画隐藏变显现
			DlgLoginAmin->ReSetPlaytime();								//重播放动画
		
		}else if(uobj == BtClose2){			//点击的是道具合并的[X]关闭按钮
			GN::ValueChangeColor(&(DlgProps->c), 0xffffffff);			//使道具面板对话筐渐渐变回原貌
			GN::ValueChangeColor(&(DlgCombine->c), 0x00ffffff);			//使道具合并对话筐渐渐隐藏起来
			GN::ValueChangeRepeat(&DlgCombine->y, 270, DlgCombine->y-100); //升高再下降来回插值道具合并对话筐的Y坐标 插值过程时间为270毫秒 升高高度为100像素
		
		}else if(uobj == BtCombine){		//点击的是合并按钮
		  if(DlgProps->c == 0xffffffff){			//当道具面板对话筐颜色为原貌时
			GN::ValueChangeColor(&(DlgProps->c), 0xff777777);			   //使道具面板对话筐渐渐变暗
			GN::ValueChangeColor(&(DlgCombine->c), 0xffffffff);			   //使道具合并对话筐渐渐显现出来
			GN::ValueChangeRepeat(&DlgCombine->y, 270, DlgCombine->y-100); //使道具合并对话筐动画弹跳出来
			
			P<char> *_names = Names->GetLables();	//从道具名文本选择列表里获得各个道具名
			char sids[32], name_buf[32*12] = "确定要合成:";				//定义两个临时字符串缓冲 并指定好其中一个的前段内容 用来作合并清单信息显示
			char *pstr = name_buf + strlen(name_buf);					//定位到合并清单信息的末尾位置
			unsigned int si = 0, sn = 0;
			for(; si < 32; ++si){										//循环检索道具选择列表最大能支持的32个选定
				if(Icons->aSelected & (1<<si)){							//比较 aSelected 特定位域来判断出对应序号的图标是否被选上
					sids[sn++] = si;									//有选上就记录该图标序号
					pstr += sprintf(pstr, "%s,", (char*)_names[si]);	//打印对应序号的道具名到合并清单信息末尾 然后再使写入位置再移到信息新的末尾
				}
			}
			if(sn){					//当有图标被选上
				Selected->SetText(sids, sn);				//设置刚才记录到的图标序号给道具动画艺术图筐来显示
				--pstr; *pstr = 0;							//去掉合并清单信息末尾的,号
			}else{					//当无图标被选上
				Selected->ToHide();							//使道具动画艺术图筐隐藏起来
				strcpy(name_buf, "您还没选到任何物品!");	//使合并清单信息变为这个字符串
			}
			GN_StrcpyData(Info->text, name_buf);			//复制合并清单信息给对应的静态文本来显示
			if(sn > 1){				//当有大于1个图标被选上
				if(!BtYesPtr.aPtr)DlgCombine->Comment(BtYes, &BtYesPtr);  //当Yes按钮所在地址为0 表示它不在UI场景里 那就连入它 并获得新的地址
			}else{					//当有1个或0个图标被选上
				if( BtYesPtr.aPtr)BtYesPtr.UnComment((P<GNu::iUI_OBJECT>*)&BtYes);  //当Yes按钮所在地址不为0 表示它在UI场景里 那就解除它出来
			}
		  }
		}else if(uobj == BtYes){	//点击的是Yes按钮
			BtYesPtr.UnComment((P<GNu::iUI_OBJECT>*)&BtYes);			  //把Yes按钮解除出来
			Selected->GetFontFrame()->aBlendmode = GNg::eBLEND_BRIGHT;	  //使已选到要合并的道具图标变为发光混合模式
			GNg::sRECT rect;
			Selected->GetTextRange(&rect);								  //获得已选道具图标占位范围
			Selected->ToHide();											  //使已选道具图标动画地隐藏起来
			FLauncher->aGenRngX = rect.left - SceneUi->x;				  //设置散开粒子特效发射范围的坐标X为道具图标占位范围X !注意这里要减去UI场景当前所在位置坐标 因为粒子特效在UI场景里
			FLauncher->aGenRngY = rect.top  - SceneUi->y;				  //设置散开粒子特效的坐标Y为道具图标占位范围Y		   !您需要把全局坐标转为在UI场景表层的相对坐标 UI组件在那个群组就要减去那个群组的坐标就转为相对坐标
			FLauncher->aGenRngW = rect.GetWidth();						  //散开粒子特效的宽度也设置一下
			FLauncher->aGenRngH = rect.GetHeight();						  //散开粒子特效的高度也设置一下
			FLauncher->ReSetPlaytime();									  //重新播放散开粒子特效
		}else if(uobj == BtOk){		//点击的是条款页的确定同意按钮
			if(Agree->aSelected){		//当同意有勾选到
				GN::ValueChange(&(PageClause->x), (short)(-GNg::ViewCur->orthoW), ColorToHide, &(PageClause->c)); //插值条款页面的X坐标使它渐渐移到视口右边 完成插值时执行 ColorToHide 函数 使页面变透明隐藏起来
				DlgLoginAmin->SwapAmStartEnd();							  //交换登陆对话筐动画事件起始结束值 使动画隐藏变显现
				DlgLoginAmin->ReSetPlaytime();							  //重播放动画
			}else{						//当同意没勾选到
				Agree->aSelected = 2;									  //设置该值为2	使它在下次再点击同意按钮时能通过认证
				Tips->ReSetPlaytime();									  //重播同意条款提示动画
			}
		}
	}
}
extern "C"{
GN_CALL void AppInit(){     //▲初始化应用必须数据 程序开始准备好引擎系统后会调用它一次 
	float progr;
	SceneUi.New(1);			//创建UI场景空间
	GNu::cUI_SCENE *_us = SceneUi;  //临时拿出UI场景的裸指针
	_us->InitLoadToData("samples/ui/LoginUI/LoginUI.gui", progr);	  //读取UI场景
	_us->PrintAllNodeClassName();	//打印出所有子节点UI组件代码类名和名称 以便您知道每个UI组件对应代码里是什么类型
	DlgProps	= CUP(GNu::cUI_GROUP,_us->Search("dlg_props"));			  //在UI场景里查找名为 dlg_props 的道具面板UI组 并挖出它的裸指针 对于在UI场景里由场景管理的UI组件 您可无需担心他的内存涉漏问题 可以放心拿裸指针用
	DlgCombine	= CUP(GNu::cUI_GROUP,_us->Search("dlg_combine"));		  //以同样的方法获取到道具合并UI组的裸指针
	PageClause	= CUP(GNu::cUI_GROUP,_us->Search("page_clause"));		  //获取到条款页面UI组的裸指针
	DlgLoginAmin = CUP(GNu::sUI_FRAME_RC,_us->Search("dlg_amin"));		  //获取到登陆对话筐里作为动效的图筐裸指针 以便控制登陆对话筐的显现和隐藏
	InUser		= CUP(GNu::cUI_TEXT_INPUT,	 _us->Search("in_user"));	  //获取到用户名文本输入筐的裸指针
	UserID		= CUP(GNu::cUI_TEXT_ART,	 _us->Search("user_id"));	  //获取到用户名静态文本的裸指针
	Sortie		= CUP(GNu::cUI_TEXT_ART,	 _us->Search("sortie"));	  //获取到出击数静态文本的裸指针
	Props		= CUP(GNu::cUI_TEXT_ART,	 _us->Search("props"));		  //获取到道具数静态文本的裸指针
	Info		= CUP(GNu::cUI_TEXT_ART,	 _us->Search("info"));		  //获取到道具合并信息静态文本的裸指针
	FLauncher	= CUP(GNu::cUI_FRAME_LAUNCHER,_us->Search("FLauncher"));  //获取到散开粒子特效的裸指针
	Icons		= CUP(GNu::cSELECT_LIST_ICON,_us->Search("icons"));		  //获取到道具图标选择列表的裸指针
	Names		= CUP(GNu::cSELECT_LIST_TEXT,_us->Search("names"));		  //获取到道具名文本选择列表的裸指针
	BtLogin		= CUP(GNu::cUI_SELECT_BUTTON,_us->Search("bt_login"));	  //获取到登陆与注册按钮联的裸指针
	BtCombine	= CUP(GNu::cUI_SELECT_BUTTON,_us->Search("bt_combine"));  //获取到合并按钮的裸指针
	BtClose1	= CUP(GNu::cUI_SELECT_BUTTON,_us->Search("bt_close1"));	  //获取到道具面板的[X]关闭按钮的裸指针
	BtClose2	= CUP(GNu::cUI_SELECT_BUTTON,_us->Search("bt_close2"));	  //获取到道具合并的[X]关闭按钮的裸指针
	Agree		= CUP(GNu::cSELECT_LIST_ICON,_us->Search("agree"));		  //获取到同意勾选图标选择列表的裸指针
	BtOk		= CUP(GNu::cUI_PAGE_BAR,	 _us->Search("PageOk"));	  //获取到确定同意按钮的裸指针
	Tips		= CUP(GNu::cUI_FRAME_LAUNCHER_FONT,	_us->Search("tips")); //获取到需同意条款提示的裸指针
	Selected	= CUP(GNu::cUI_FRAME_LAUNCHER_FONT,	_us->Search("selected")); //获取到道具动画艺术图筐的裸指针
	BtYesPtr    = _us->Search("bt_yes");					//获取到Yes按钮所在地址
	BtYes		= CUP(GNu::cUI_SELECT_BUTTON,BtYesPtr);		//根据Yes按钮地址获取出Yes按钮UI组件 !如要解除出来的UI组件就不能用裸指针了 !所有以上这些获取UI组件操作必须在未添加或解除过东西前调用 否则查询顺序会被打乱
	int fsize;
	char *code_content = GNf::FileFullLoadToMemoryText("samples/ui/Sample7.cpp", &fsize); //读取本例子的源码文件内容
	if(code_content){	//有读取到内容
		GNu::cUI_TEXT_ART *text2 = CUP(GNu::cUI_TEXT_ART,	_us->Search("text2"));  //获取到第二页条款里名为 text2 的静态文本的裸指针
		GN_DeleteArray(text2->text);		//删除原有条款文本内容
		text2->text = code_content;			//把刚读进来的源码内容传给它保管显示
		text2->h = text2->textstyle->GetFontRangeY() * (GN_StrGetCharNum(code_content, '\n') + 1); //统计代码行数并计算出全代码显示的总高度
		(CUP(GNu::cSCROLL_RECT,_us->Search("scrool_rect2")))->StatisticFitMaxOffset();	//使包含第二页条款的滚动筐统计它里面包含的内容适合的滚动偏移量 以适配代码页的高度
	}
	DlgProps->y += 100;						//先把道具面板对话筐下移100像素
	PageClause->x -= GNg::ViewCur->orthoW;	//先把条款页面移到视口右边
	FLauncher->SetNowtime(100);				//使散发特效动画时间跳到结尾 它就先隐藏起来

	GNu::UiActionCall = UiActionProc;		//指定UI组件交互操作处理方法
}
GN_CALL void AppUnInit(){   //▲释放应用必须数据 程序运行结束前会调用它一次 
	BtYes.Delete();	   //删除被拿出来的Yes按钮
	SceneUi.Delete();  //删除场景和它包含的所有数据
}
GN_CALL void AppDisplay2D(unsigned int nTime){ //▲显示2D画面 程序运行每帧都调用它 
	if(GN::ViewReSized)SceneUi->Resize(GNg::ViewCur);		//当视口大小变更时就重设一下UI场景区域大小 以便各UI组件能适配停靠
	GNg::cFRAME *font_frame = Selected->GetFontFrame();		//拿出道具动画艺术图筐式样
	if(font_frame->aBlendmode == GNg::eBLEND_BRIGHT && FLauncher->PlayIsEnd()){ //判断图筐式样当前如是发光混合模式且散开粒子特效已经播放完毕
		font_frame->aBlendmode = GNg::eBLEND_NORMAL;		//就设置回图筐式样混合模式为标准
		char pids[1];
		Selected->GetText(pids, 1);							//获取出首个已选道具图标序号
		Selected->SetText(pids, 1);							//把该图标序号再设置给道具动画艺术图筐 使它只显示首个已选图标
		strchr(Info->text,',')[0] = 0;						//找到合并清单信息里的首个,号出现处 设成结束符 相当于把它截断
	}
	SceneUi->UpdatePlay(nTime);		//更新播放UI场景
}
GN_CALL void AppDisplay3D(unsigned int nTime){} //▲显示3D画面 程序运行每帧都调用它 

GN_CALL void AppRun(unsigned int nTime){}       //▲运行程序后台的处理 程序运行每帧都调用它
}