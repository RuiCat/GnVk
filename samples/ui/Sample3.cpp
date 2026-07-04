#include<GN.h>		   //使用GN库必需包含这个头文件

P<GNg::sIMAGE>				NodeIcons;		//节点类型图标
/********************************************************************************************************************************************************************/
struct sLNODE				//简单定制树节点
{
	uint8					selected;		//是否选中标记
	uint8					unfold;			//是否展开标记
	char					name[16];		//节点名
	GN::sLIST< P<sLNODE> >	subs;			//各个子节点

	static int				zTotalNodeNum;  //当前该类总节点数

	void Init(const char *name_){ //初始化(节点基名)
		selected = 0; unfold = 0;			//这两项标记初始都为0
		sprintf(name, "%s%02d", name_, zTotalNodeNum++); //按基名在前再续2位整数编号不够补0的格式打印出节点名 !编号为当前该类总节点数
	}
};
int	sLNODE::zTotalNodeNum = 0;	  //总节点数初始为0

class cTREE_LNODE_OPTR :public GNu::cTREE_NODE__OPTR				//简单定制树节点操作器  !高度自定义的树节点 需要再套这么一层胶水层
{ public:   //重写以下这些函数来匹配访问 sLNODE 里的数据
	virtual GN::sLIST<P_> *GetNodeSubs(P_ node){ return (GN::sLIST<P_>*)&((CVP(sLNODE)node)->subs); }  //获得指定节点的子对象链表指针(指定节点)返回子对象链表指针
	virtual bool HasUnfold(P_ g){ return (CVP(sLNODE)g)->unfold; }							//判断指定节点是否含展开标记(节点)返回是否
	virtual void UnfoldTag(P_ g, uint8 on){ (CVP(sLNODE)g)->unfold = on; RefreshNodes(); }	//为指定节点设置展开标记(节点, 是否展开)
	virtual bool HasSelect(P_ node){ return (CVP(sLNODE)node)->selected; }					//判断指定节点是否含选中标记(节点)返回是否
	virtual void SelectTag1(P_ node, uint8 on){ (CVP(sLNODE)node)->selected = on; }			//为指定节点设置选中标记(节点, 是否选中)
	virtual char*GetNodeName(P_ &node){ return (CVP(sLNODE)node)->name; }					//获得指定节点名称
};
P<sLNODE>					TestTreeL;		//测试节点树 - 定制的节点树
P<GNu::cTREE_VIEW>			TreeViewL;		//滚动列表树 - 定制树的列表
cTREE_LNODE_OPTR		   *NodeOptr;		//树节点操作器

//加插在绘制节点图标与信息之间的处理(节点, 安插位置X, 位置Y, 节点区块的宽度,高度, 区块底色, 该区块是否选择焦点, 挂载到的滚动列表树)返回处理占用到的区间长度
int LNodeDrawCall(P_ &node,int x, int y, int w, int h, unsigned int color, bool &has_focus, void *arg){
    GNu::cTREE_VIEW *tree = (GNu::cTREE_VIEW*)arg;	   //转换出滚动列表树
	GNg::cTEXT_ART *text_style = tree->GetTextStyle(); //获得它里面的文本输出式样
	char us_num_str[4];
	int len = sprintf(us_num_str, "%02d", (CVP(sLNODE)node).Us());     //把当前绘制节点被引用数 打印为2位整数编号不够补0的格式字符串
	text_style->OutText(us_num_str ,0, x, y, w, h, tree->aTextColor);  //用文本颜色显示输出编号字符串
	return len * text_style->GetFontRangeX();	//返回编号字符串占位大小
}
/********************************************************************************************************************************************************************/
struct sTNODE	:public GN::sNODE<sTNODE>	//测试节点
{
	unsigned short			node_type;		//节点类型
	sTNODE(){ node_type = GN_Random_n(2, NodeIcons->texnum); }	//初始化 给节点类型指定一个随机图标ID
};
P<sTNODE>					TestGroup;		//测试节点组
P<sTNODE>					TestTree;		//测试节点树
P<GNu::cTREE_VIEW>			TreeView;		//滚动列表树

//加插在绘制节点图标与信息之间的处理(节点, 安插位置X, 位置Y, 节点区块的宽度,高度, 区块底色, 该区块是否选择焦点, 挂载到的滚动列表树)返回处理占用到的区间长度
int TNodeDrawCall(GN::sNODE_ *node,int x, int y, int w, int h, unsigned int color, bool &has_focus, void *arg){
    GNu::cTREE_VIEW *tree = ( GNu::cTREE_VIEW*)arg;	  //转换出滚动列表树
	if(has_focus && GN::MouseI[GM_LEFT]){			  //点选到节点项
		int lx = GNg::ViewCur->mouseMX - x;			  //计算得鼠标落在图标的局部水平位置
		if(lx > 0 && lx < 16){						  //鼠标点到图标
			((sTNODE*)node)->node_type = 0;			  //使节点类型变为0
			has_focus = false;						  //使不能再选到其它内容
	}	}
	unsigned short tid = node->SubNum(false)? 1 : ((sTNODE*)node)->node_type; //如节点有子对象就用文件夹图标索引1 否则用节点类型来索引
	GNg::ImageDraw(NodeIcons, x, y, 16,16, color, tid); //绘制节点类型对应的图标 16x16大小
	return 16;	//返回图标占位大小
}
//插入一组节点(插入到的目标节点, 节点基名, 插入节点个数)
void InsertNodes(sTNODE *tg, char *name, int count){
	char name_buf[16];
	int i = 0;
	for(; i < count; ++i){	//循环数次来出入节点
		sprintf(name_buf, "%s%02d", name, i);	  //按基名在前再续2位整数编号不够补0的格式打印出节点名
		P<sTNODE> node(1);						  //创建新节点
		node->InitToData(name_buf,node);		  //初始化指定节点名
		tg->Comment(node);						  //把新节点连到目标节点里
	}
}
/********************************************************************************************************************************************************************/
extern "C"{
GN_CALL void AppInit(){     //▲初始化应用必须数据 程序开始准备好引擎系统后会调用它一次 
	NodeIcons.New(1);			//分配图标空间
	NodeIcons->InitToData("samples/ui/LoginUI/[14]fileicon", 0);  //加载图标
	//构建节点树右
	TestTree.New(1);							  //分配测试节点树空间
	TestTree->InitToData("Root",TestTree);		  //初始化指定跟节点名
	InsertNodes(TestTree, "node0_", 10);		  //插入10个节点到跟节点
	InsertNodes(TestTree, "node1_", 6);			  //再插入6个节点到跟节点
	TestGroup.New(1);
	TestGroup->InitToData("Group",TestGroup);	  //初始化指定组节点名
	TestTree->Comment(TestGroup);				  //把组节点连到跟节点里
	InsertNodes(TestGroup, "node__", 3);		  //插入3个节点到组节点
	InsertNodes(TestGroup->Search("node__01"), "node_-_", 5);	//在组节点里查找到名为 node__01 的节点 并插入5个子节点到它里面
	//滚动列表树右
	TreeView.New(1);					//分配滚动列表树空间
	GNu::cTREE_VIEW *tree = TreeView;	//临时拿出它的安全指针使访问更快
	tree->SetEffect(360);				//设置滚条与选框闪动间隔时间
	P<GNu::cTREE_NODE_OPTR> optr = GNu::MakeTreeNodeP_Operator<sTNODE>(TestTree);    //构建树节点操作器 来操作节点树
	tree->InitStyleToData(optr, &GN::RunRec.aFont,0xffffffff,true, 430,80, 300,450); //初始化指定树节点操作器和式样和显示范围
	tree->aUnfoldIcon.New(1);			//分配展开图标空间
	tree->aUnfoldIcon->InitToData("samples/base/image/arrow_"); //加载展开图标
	tree->aFocusColor = 0x50ff7722;		//指定选框焦点颜色
	((GNu::cTREE_NODE_P_OPTR*)TreeView->GetNodeOptr())->aDrawNodeAddProc = TNodeDrawCall;	//在滚动列表树里获出节点操作器 并指定它的加插绘制回调函数
	//((GNu::cTREE_NODE_P_OPTR*)TreeView->GetNodeOptr())->aNodeIconIdAddrOffset = (uint8*)&(TestGroup->node_type) - (uint8*)((sTNODE*)TestGroup);  //在滚动列表树里获出节点操作器 并指定它的图标索引ID所在节点指针偏移量
	//tree->GetIcons() = NodeIcons;		//指定节点类型图标
	
	//构建节点树左
	TestTreeL.New(1);
	TestTreeL->Init("Root");			//初始化指定跟节点名
	sLNODE *tree1 = TestTreeL;			//临时拿出它的安全指针使访问更快
	sLNODE *group = tree1->subs.add(P<sLNODE>(1))[0]; //在跟节点处加入一个新增群组节点 并获得它在里面的裸指针
	group->Init("Group");				//初始化指定组节点名
	int ti;
	for(ti = 0; ti <  5; ++ti)group->subs.add(P<sLNODE>(1))[0]->Init("node"); //在组节点处加入 5个新增节点 一并指定它的节点名
	for(ti = 0; ti < 15; ++ti)tree1->subs.add(P<sLNODE>(1))[0]->Init("node"); //在跟节点处加入15个新增节点 一并指定它的节点名
	//滚动列表树左
	TreeViewL.New(1);					//分配滚动列表树空间
	tree = TreeViewL;					//临时拿出它的安全指针使访问更快
	P<cTREE_LNODE_OPTR> optr1(1);		//创建树节点操作器
	tree->InitStyleToData(CVP(GNu::cTREE_NODE_OPTR)optr1, &GN::RunRec.aFont,0xffffffff,true, 80,80, 300,450); //初始化指定树节点操作器和式样和显示范围
	optr1->Init(TestTreeL, tree->GetVisualCount());		//初始化 为树节点操作器指定节点树 和滚动筐最大可见节点数
	NodeOptr = (cTREE_LNODE_OPTR*)tree->GetNodeOptr();  //在滚动列表树里获出节点操作器 !等效于 NodeOptr = optr1;
	NodeOptr->aDrawNodeAddProc = LNodeDrawCall;			//指定它的加插绘制回调函数
}
GN_CALL void AppUnInit(){   //▲释放应用必须数据 程序运行结束前会调用它一次
	TreeViewL.Delete();			//删除滚动列表树
	TestTreeL.Delete();			//删除测试节点树
	TreeView.Delete();			//删除滚动列表树
	TestGroup.Delete();			//删除组节点
	TestTree.Delete();			//删除测试节点树
	NodeIcons.Delete();			//删除节点类型图标
}
GN_CALL void AppDisplay2D(unsigned int nTime){ //▲显示2D画面 程序运行每帧都调用它 
	GNg::cVIEW2D *view = GNg::ViewCur;
	GNg::ImageDraw(0,view->ortho_left,view->ortho_top,view->orthoW,view->orthoH,0xff777777);  //显示一个灰色的矩型作为背景色清屏
	GNg::Draw2DAlphaDirectionSet(GNu::eNEAR_RIGHT);			//使接下来的2D绘制都为向右渐淡的
	GNg::ImageDraw(0,view->ortho_left,view->ortho_top,430,view->orthoH, 0x66aa8822);		  //绘制一个半个屏大渐变土黄色块
	GNg::Draw2DAlphaDirectionSet(GNu::eNEAR_NULL);			//取消渐变绘制
	
	GNu::cTREE_VIEW *tree = TreeView;	//临时拿出滚动列表树的安全指针使访问更快
	tree->UpdateRun(nTime);				//更新显示滚动列表树
	if(GN::Key1[GK_P]){			//当按下键盘P键 
		GN::sLIST< P_ > nodes;
		tree->GetSelectNodes(&nodes);	//获取出当前选到的节点
		if(nodes.size()){		//有选到节点
			GN::sLIST< P_ >::iterator it = nodes.begin(), et = nodes.end(); //定义节点历遍器首尾
			for(; it != et; ++it)GN::RunRec.Print("%s\n", (char*)(CVP(sTNODE)(*it))->aName); //历遍所有获取到的节点 并打印节点名到提示信息里
	}	}
	if(tree->IsMouseFocus()){	//鼠标落在滚动列表筐里
		if(GN::Key1[GK_A] && GN::Key[GK_LCONTROL])tree->AllSelect(true); //当按下键盘Ctrl + A键就全选所有节点
		if(GN::Key1[GK_C])TestGroup->Clone(2);	//当按下键盘C键 复制多一层组节点 包含里面的所有子节点
	}
	short fx, fy, fw, fh;
    tree->GetPos(  &fx,    &fy,  &fw,   &fh);	 //获取滚动列表树显示范围
	GNg::LineFrame2dDraw((int)fx, (int)fy, (int)fw, (int)fh, 1, 0x77ffddbb); //绘制一个范围大小的线筐

	tree = TreeViewL;					//临时拿出另一滚动列表树的安全指针使访问更快
	tree->UpdateRun(nTime); 			//更新显示滚动列表树
	GNg::LineFrame2dDraw( 80,	  (int)fy, (int)fw, (int)fh, 1, 0x77ffddbb); //在坐标x80的位置绘制一个范围大小的线筐
	if(tree->IsMouseFocus()){	//鼠标落在滚动列表筐里
		if(GN::Key1[GK_C]){				//当按下键盘C 
			GN::sLIST< P_ > nodes;
			tree->GetSelectNodes(&nodes);	//获取出当前选到的节点
			if(nodes.size()){   //有选到节点
				P_ parent_nodes[64];											//定义用来装父节点链的临时数组
				GN::sLIST< P_ >::iterator it = nodes.begin(), et = nodes.end(); //定义节点历遍器首尾
				for(; it != et; ++it){											//历遍所有获取到的节点
					if(*it == NodeOptr->aRoot)continue; //跳过跟节点
					P<sLNODE> clone(1);					//分配1个副本节点
					clone.Copy(*it);					//复制节点内数据 !!注意节点里面的子节点数据并没相应复制出来 只是被引用多次而已 在节点名前两位数可看到节点被引用数
					strcat(clone->name, "_B");			//使副本节点名续上 _B
					P_ *p_end = NodeOptr->SearchParentNodes(NodeOptr->aRoot, *it, parent_nodes);  //查找包含节点的所有父节点
					(*((P<sLNODE>*)p_end))->subs.push_back(clone); //把副本节点加入到节点最近的父节点里
				}
				NodeOptr->RefreshNodes();	//通知节点树刷新
			}
		}else if(GN::Key1[GK_G] || GN::Key1[GK_DELETE]){		//当按下键盘G键或Delete键 
			GN::sLIST< P_ > nodes;
			tree->GetSelectNodes(&nodes);	//获取出当前选到的节点
			if(nodes.size()){   //有选到节点
				sLNODE *group;
				if(GN::Key1[GK_G]){  //按下的是G键
					group = TestTreeL->subs.add(P<sLNODE>(1))[0];				//在跟节点处加入一个新增群组节点 并获得它在里面的裸指针
					group->Init("Group");										//初始化指定群组名
				}
				P_ parent_nodes[64];											//定义用来装父节点链的临时数组
				GN::sLIST< P_ >::iterator it = nodes.begin(), et = nodes.end(); //定义节点历遍器首尾
				for(; it != et; ++it){											//历遍所有获取到的节点
					if(*it == NodeOptr->aRoot)continue; //跳过跟节点
					if(GN::Key1[GK_G])group->subs.push_back(*it);				//按下的是G键就把节点加入到刚新增的群组节点里
					P_ *p_end = NodeOptr->SearchParentNodes(NodeOptr->aRoot, *it, parent_nodes);  //查找包含节点的所有父节点
					(*((P<sLNODE>*)p_end))->subs.remove(*it); //把节点从它的父节点里移除
				}
				NodeOptr->RefreshNodes();	//通知节点树刷新
			}
		}
	}
	int visual_node_num = NodeOptr->GetVisualNodeNum(); //获得当前展开的可显示节点总数
	GN_RunValueCheck2(visual_node_num, 0);				//显示节点总数
	GN_RunValueCheck2(tree->aSelected, 21);				//显示当前已选上了的项或掩位组合
}
GN_CALL void AppDisplay3D(unsigned int nTime){} //▲显示3D画面 程序运行每帧都调用它 

GN_CALL void AppRun(unsigned int nTime){}       //▲运行程序后台的处理 程序运行每帧都调用它
}