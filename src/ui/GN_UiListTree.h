#ifndef _GN_UILISTTREE_H_
#define _GN_UILISTTREE_H_
namespace GNu{
//构建单色简单箭头图标(装填图标, 图标宽, 图标高, 式样0:圆头1:尖头 , 生成为空心边框的边宽度 默认0为实心的)
GN_API void MakeSimpleArrowIcon(GNg::sIMAGE *_img, int _w, int _h, bool style=0, int frame_w=0);

class cTREE_NODE_OPTR				//★树节点操作器  继承该接口类以便对自定义树形数据结构传给滚动列表树进行操作
{ public:
	virtual void OnUpdateBegin(unsigned int updatatime){}				  //当列表框更新前的准备处理(帧间隔时间)
	virtual void OnReArranged(int id, float st_nid,unsigned int max_vnn){}//当框滚动或被重排列后的处理(当前排列起始节点ID, 树列表框可见起始节点ID, 列表框当前最大可见节点数)
	virtual bool HasUnfold(P_ g){ return 0; }							  //判断指定节点是否含展开标记(节点)返回是否
	virtual void UnfoldTag(P_ g, uint8 on){}							  //为指定节点设置展开标记(节点, 是否展开)
	virtual bool HasSelect(P_ node){ return 0; }						  //判断指定节点是否含选中标记(节点)返回是否
	virtual void SelectTag(P_ node, uint8 on, int8 mode){}				  //为指定节点设置选中标记(节点, 是否选中, 选择模式 0:单选 1:段选起点 -1:段选)
	virtual int  CanDrawLastNodeNum(int num){ return 0; }				  //当绘制完可视节点后检测当前向上是否还有指定个数可绘制节点 以便测量可滚动量(指定节点数) 返回指定节点数 若可绘制节点数小于指定节点数就返回可绘制节点数
	virtual int  CanDrawNextNodeNum(int num){ return 0; }				  //当绘制完可视节点后检测当前向下是否还有指定个数可绘制节点 以便测量可滚动量(指定节点数) 返回指定节点数 若可绘制节点数小于指定节点数就返回可绘制节点数
	virtual void OnUpdateEnd(){}										  //当列表框更新结束后的处理
	//以上这几个虚函数定义顺序与被滚动列表树调用顺序一致

	virtual ~cTREE_NODE_OPTR(){}
	virtual P<cTREE_NODE_OPTR> OnCloneOut(){return P<cTREE_NODE_OPTR>(1);}//复制出该对象的一个新副本
	virtual bool Search_exec(P_ g, bool unfold, bool (*proc)(P_ &node, void *us_arg), void *_us_arg){ return false; } //使指定节点里的所有子节点和它都执行指定函数(指定节点 传入空指针表示跟节点, 是否仅检索展开的节点, 返回是否要继续查找 执行函数(当前检索到的节点, 传回给你的参数), 传给执行函数用的参数) 返回查找中途是否有被中断
	virtual void MoveNodes(P_ pos, bool pos_back, GN::sLIST<P_> *nodes){} //移动一组节点到指定节点旁边(指定节点, 是否放在指定节点之后 否则在前, 一组节点)
	virtual char*GetNodeName(P_ &node){ return ""; }					  //获得指定节点名称
	virtual P_	 GetCurFristNode() { return P_(); }						  //获得当前首个检索节点
	virtual int	 GetVisualNodeNum(){ return 0; }						  //获得当前展开的可显示节点总数
	virtual void *GetDrawNodeFunc(){ return 0; }						  //获得节点信息绘制的方
};
//绘制滚动列表树的节点项(滚动列表树, 显示信息的文本输出 , 当前绘制节点 , 节点名)  !该方法是给 GetDrawNodeFunc 里的绘制方法调用的 以方便使用到滚动列表树预设式样
#define GNu_TREE_VIEW_DrawNodeItem( tree_view , text_style , cur_node , node_name ) \
	GNu::cTREE_VIEW *sl = (GNu::cTREE_VIEW*)tree_view;\
	GT_SELECT_LIST_DrawText( text_style , node_name );\
	GT_SELECT_LIST_ProcAndDrawFocus( zSelectProc(cur_node,zid,has_focus) , GetFocusChangeColor() , FocusImg() )

/************************************************************************************************************************************************************************/

class GN_API cTREE_VIEW :public cSELECT_LIST_TEXT //★滚动列表树 支持动态增长缩短双向链表树 和增删重排操作等功能
{  
   #ifndef _APPLE_
    cSELECT_LIST_TEXT::InitStyleToData;			    //使基类的该函数不能用
	cSCROLL_LIST::SetZoningCount;
   #endif
   protected:
	P<cTREE_NODE_OPTR>			    zNodeOptr;		//树节点操作器
	P_								zFocusLastNode; //上次焦点的节点
	int								zFocusLastVid;  //上次焦点对应的可视ID
	float							zTextOffsetBk;	//zTextOffset 的备份
	uint8							zDraging;		//是否正在拖动节点中

	uint8* zDataSave(uint8 *wpos, cSHARE_DATA *share_objs); //保存该对象的2进制数据(当前写入位置, 收集共享对象集)返回最后的写完位置
    uint8* zDataLoad(uint8 *rpos, cSHARE_DATA *share_objs); //读取该对象的2进制数据(当前读取位置, 引用共享对象集)返回最后的读完位置

	//当滚动筐被重排列后的处理(当前排列起始区域号, 上次的排列起始区域号, 排列是否一起重设了所有区域的弹出效果)
	virtual void zOnReArranged(unsigned short sp, unsigned short sp_last, bool ret_am);
	//当滚动筐要添加滚动偏移的处理(偏移量)返回是否忽略默认的偏移量处理
	virtual bool zOnAddScroolOffset(int move_dist);
   public:
	bool zSelectProc(P_ &node, unsigned int zid, bool has_focus);    //选择项处理(选项对应的节点, 选项ID, 选项是否当前选择焦点)返回该项是否要绘制高亮焦点
	void						   *zProcArg;			//中转处理传参
//	unsigned int					aSelected;          //当前已选上了的项 !在本类中未展开的项不计入其中 多选模式下最多只能统筹前32个已展开的项

	static P<GNg::sIMAGE>			aDefaultUnfoldImg;	//默认展开图标

	P<GNg::sIMAGE>					aUnfoldIcon;	    //展开图标 当里面纹理数大于1时 会自动给展开的节点引用另一个纹理 否则用原图旋转90度来给展开的节点
//	unsigned int					c;					//展开图标色调  !已在基类有定义
	short							aSpacing;			//层缩进间距
	bool							aMergeIcon;			//是否合并图标显示
	bool							aEnableImeSelect;   //是否启用字符输入选择

	cTREE_VIEW():aSpacing(GN::RunRec.aFont.GetFontWidth()<<1),aMergeIcon(false),aEnableImeSelect(false){ aType = eUI_TREE_VIEW; aBackColor = 0x00888888; }

	//初始化设置显示式样 !!必需
    bool InitStyleToData(
		 P<cTREE_NODE_OPTR>   node_optr,					 //树节点操作器 如指定空指针则依旧使用原树节点操作器
         const GNg::cTEXT_ART *text_style = 0,				 //显示信息的文本输出 该类内部不托管这个 您得确保在使用周期内这个还未被释放
	     unsigned int   text_color  = 0,                     //文本信息颜色
				  bool  mult_select = false,                 //是否可多选
				  short x           = NearLx(0),             //滚动列表范围左上坐标X
				  short y           = NearTy(0),             //滚动列表范围左上坐标Y
				  short w           = GNg::ViewCur->orthoW,  //滚动列表范围宽度
				  short h           = GNg::ViewCur->orthoH,  //滚动列表范围高度
				  bool  default_am  = true,                  //列表默认动画效果 !在没调用SetEffect指定生成效果筐或第一次调用该函数时该值才会生效
		 unsigned int   focus_color = GT_DEF_FOCUS_COLOR,    //焦点颜色
		 unsigned short focus_blend = GNg::eBLEND_BRIGHT     //焦点混合模式
	);   //返回是否成功

	void SetNodeOptr(P<cTREE_NODE_OPTR> node_optr);				//设置树节点操作器
	inline cTREE_NODE_OPTR*	GetNodeOptr(){ return zNodeOptr; }	//获得内部树节点操作器 
	inline P<GNg::sIMAGE> &GetIcons(){ return zIcons; }			//引用图标图像
	inline bool MouseKeyIsLongDwon(){ return zDraging&&GN::MouseQ[GM_LEFT]; } //判断鼠标左键是否长按点击 这个通常放在节点绘制回调里来检测这个来确定怎么操作 例如查看节点详细属性或修改节点名

	void RePos();												//滚动复位
	void SetPos(short _x,short _y,short _w,short _h);			//设置显示位置大小(左上位置X, 左上位置Y, 筐宽, 筐高)
	void SetPos(GNg::sRECT2 &range);							//设置显示位置范围(范围矩型)
	void SetMultSelect(bool yes);								//设置是否可多选
	
	//获得当前选到的节点(指向装填返回节点的链表) 返回值相当于 aSelected 值的意义 !在本类中多选模式下 aSelected 不会随操作立即更新 需要调用该函数来刷新
	unsigned int GetSelectNodes(GN::sLIST< P_ > *_node_list_out);
	
	//检测UI交互回调触发动作类型如是点选ID就转换为不论展开与否绝对顺序的点选ID(触发动作类型)返回转换后的动作类型 非点选ID就直接返回
	int  CheckConvertActionSelectID(int action);

	void AllSelect(bool yes, P_ g = P_());   //选上指定节点里的全部节点(是否, 指定节点 默认为跟节点)
	void AllUnfold(bool yes, P_ g = P_());   //展开指定节点里的全部节点(是否, 指定节点 默认为跟节点)
	void AllCancelAndClose(P_ g = P_());	 //选择取消并关闭指定节点里的全部节点(指定节点 默认为跟节点)

	unsigned int SearchNodeID(P_ node, bool unfold=true);  //查找指定节点序号ID(节点, 是否仅检索展开的节点)返回序号从1开始 返回0表示没找到
	P_ SearchNode(unsigned int  id, bool unfold=true);	   //查找指定序号ID的节点(节点序号ID 从1开始, 是否仅检索展开的节点)返回节点 返回空指针表示没找到
	P_ SearchNode(unsigned int &id, const char *name, uint8 cmpn=0, bool ign_sl=false, bool unfold=true);  //查找与指定名匹配的节点(返回节点序号ID 从1开始, 名, 比较名前段字符个数 默认0表示比较完整名, 是否忽略已选上的节点, 是否仅检索展开的节点)返回节点 返回空指针表示没找到
	
	inline char *GetNodeName(P_ node){ return zNodeOptr->GetNodeName(node); }  //获得指定节点名称

	void ScrollToNodeID(unsigned int id);				   //滚动到指定序号ID的节点处(节点序号ID 从1开始) !这个执行比 ScrollToNode 快
	void ScrollToNode(P_ node);							   //滚动到指定节点处(节点)
	void ScrollToNode(const char *name, uint8 cmpn=0);	   //滚动到与指定名匹配的节点处(名, 比较名前段字符个数 默认0表示比较完整名)

	//手动设置筐内偏移的比率(值0.0f至1.0f, 推进度0.0f至1.0f 默认调用后会有滚动动画 在经过一段时间后才会达到偏移比率位置 该参数可以让你把这个过程推进一下)
	inline void SetOffsetRatio(float value, float flow_r=0.0f){
		zZoningNum = zNodeOptr->GetVisualNodeNum();
		cSELECT_LIST_TEXT::SetOffsetRatio(value, flow_r);
	}
	//获得滚动筐内偏移比率 值0.0f至1.0f
	inline float GetOffsetRatio(){
		zZoningNum = zNodeOptr->GetVisualNodeNum();
		return cSELECT_LIST_TEXT::GetOffsetRatio();
	}
	//通知刷新一组可视节点(首个可视节点ID, 刷新个数) !这个通常用来刷新弹出动画
	void RefreshVisualNode(unsigned int vid, unsigned int count);

	//更新运行滚动列表树(帧间隔时间, 实时总体色调) 返回滚动列表树被重设之后的运行经历时间 如果返回0表示滚动列表树已经播放消失结束了 返回1表示滚动列表树正在拖动中
	unsigned int UpdateRun(unsigned int updatetime, unsigned int color=0xffffffff);

	virtual P<iUI_OBJECT> Clone();	//复制出该UI组件的一个新副本

	//更新运行滚动列表树(帧间隔时间, 父层级信息链)
	virtual void UpdateRun(unsigned int updatetime, sUI_LEVEL *parent);

	//把内部数据储存为一整段数据时的处理(收集共享对象集, 返回整段数据 首个字节为版本号之后4个字节为aType 当该值为0时仅返回数据段需要大小)返回数据段大小字节 !用户自定义UI组件的版本号应由1开始
	virtual unsigned int DataSave(cSHARE_DATA *share_objs, uint8 *data_out=0);

	//从文件数据段里读入数据时的处理(引用共享对象集, 数据段地址 首个字节为版本号之后4个字节为aType)返回已读取字节数 失败返回0 !用户自定义UI组件的版本号应由1开始
	virtual unsigned int DataLoad(cSHARE_DATA *share_objs, const uint8 *data);
};
/******************************************************** 自定义树形数据结构传给滚动列表树进行操作的功能 ************************************************************************************/

class GN_API cTREE_NODE__OPTR  :public cTREE_NODE_OPTR			//★定制树节点操作器  !适合操作高度定制的节点类型 操作小型树较快 大型树适中 树节点有变动需要手动提示刷新
{ 
	void zDrawNodesCollectAndStatist(P_ g, short lv);
	int								zStid;			   //当前排列起始节点ID
	int								zEndid;			   //当前排列结尾节点ID
	int								zVisNodeTotalNum;  //当前展开的可见节点总数
	P<short>						zNodeLevels;	   //绘制节点缓冲中每个节点所在层级数
	P< P_ >							zDrawNodes;		   //绘制节点缓冲
	P_ *							zDrawNodeCur;	   //当前绘制节点
	P_								zMsLastNode;	   //上次点选到的节点
	bool							zNodeNeedRefresh;  //节点是否需要刷新
   public:
	P_								aRoot;			   //跟节点

	inline void RefreshNodes(){ zNodeNeedRefresh = true; }				  //通知节点树需要刷新 !通常派生类自定义 UnfoldTag 函数里的节点展开变更后 用于通知刷新的
	P_ * SearchParentNodes(P_ &g, P_ &node, P_ *parent_out);			  //查找指定节点所在的一连串父节点(起始查找组节点, 指定节点, 装父节点链的缓冲)返回在父节点链缓冲里最末端最近指定节点的父节点指针 没找到返回0
	P_	 GetNodeAndSeekNext(short &level_n);							  //获得当前检索节点 并使内部跳转到下个检索节点的位置(返回所在层级数)
	virtual GN::sLIST<P_> *GetNodeSubs(P_ node){ return 0; }			  //获得指定节点的子对象链表指针(指定节点)返回子对象链表指针

	virtual void OnUpdateBegin(unsigned int updatatime);				  //当列表框更新前的准备处理(帧间隔时间)
	virtual void OnReArranged(int id, float st_nid,unsigned int max_vnn); //当框滚动或被重排列后的处理(当前排列起始节点ID, 列表框可见起始节点ID, 树列表框当前最大可见节点数)
	virtual void SelectTag1(P_ node, uint8 on){}						  //为指定节点设置选中标记(节点, 是否选中)
	virtual void SelectTag (P_ node, uint8 on, int8 mode);				  //为指定节点设置选中标记(节点, 是否选中, 选择模式 0:单选 1:段选起点 -1:段选)
	virtual int  CanDrawLastNodeNum(int num);							  //当绘制完可视节点后检测当前向上是否还有指定个数可绘制节点 以便测量可滚动量(指定节点数) 返回指定节点数 若可绘制节点数小于指定节点数就返回可绘制节点数
	virtual int  CanDrawNextNodeNum(int num);							  //当绘制完可视节点后检测当前向下是否还有指定个数可绘制节点 以便测量可滚动量(指定节点数) 返回指定节点数 若可绘制节点数小于指定节点数就返回可绘制节点数

	cTREE_NODE__OPTR():aNodeIconIdAddrOffset(0),aSelectAddProc(0),aDrawNodeAddProc(0){}
	virtual ~cTREE_NODE__OPTR(){}
	virtual P<cTREE_NODE_OPTR> OnCloneOut();							  //复制出该对象的一个新副本
	virtual bool Search_exec(P_ g, bool unfold, bool (*proc)(P_ &node, void *us_arg), void *_us_arg); //使指定节点里的所有子节点和它都执行指定函数(指定节点 传入空指针表示跟节点, 是否仅检索展开的节点, 返回是否要继续查找 执行函数(当前检索到的节点, 传回给你的参数), 传给执行函数用的参数) 返回查找中途是否有被中断
	virtual void MoveNodes(P_ pos, bool pos_back, GN::sLIST<P_> *nodes);  //移动一组节点到指定节点旁边(指定节点, 是否放在指定节点之后 否则在前, 一组节点)
	virtual P_	 GetCurFristNode() { return zDrawNodes[0]; }			  //获得当前首个检索节点
	virtual int	 GetVisualNodeNum(){ return zVisNodeTotalNum; }			  //获得当前展开的可显示节点总数
	virtual void *GetDrawNodeFunc();									  //获得节点信息绘制的方
	
	int	   aNodeIconIdAddrOffset;										  //绘制节点图标索引ID所在节点指针偏移量 !假如节点里的图标索引ID属性为icid 那么该值应该为 (uint8*)&(node->icid) - (uint8*)node

	void (*aSelectAddProc)(P_ &node, uint8 on);							  //节点设置选中标记后的额外处理(节点, 是否选中)
	int  (*aDrawNodeAddProc)(P_ &node,int x, int y, int w, int h, unsigned int color, bool &has_focus, void *arg); //加插在绘制节点图标与信息之间的处理(节点, 安插位置X, 位置Y, 节点区块的宽度,高度, 区块底色, 该区块是否选择焦点, 挂载到的滚动列表树)返回处理占用到的区间长度

	void Init(P_ root, unsigned int max_vnn);							  //初始化(树根节点, 树列表框最大可见节点数)
	void Clear();														  //清除树节点和相关中转辅助数据
};
/******************************************************** 基于 GN::sNODE<T> 派生的树形数据结构传给滚动列表树进行操作的功能 ******************************************************************/

//树展开可见节点总数统计(树跟节点)返回可见节点总数
GN_API int TreeVisualNodeNumStatist(P<GN::sNODE_> &node);

template<class T>
//树子节点链接(链接到的目标节点 指定0则表示移除子节点,子节点)
void TreeNodeComment(GN::sNODE_ *tg, P<GN::sNODE_> node){
	if(tg)((T*)tg)->Comment(CVP(T)node);
	else (CVP(T)node)->UnComment();
}
template<class T, class LTER = GT_LIST_iterator(T) >
struct sTREE_LEVEL					//树节点层级信息
{
	sTREE_LEVEL<T,LTER>			   *parent; //上一层级
	P< sTREE_LEVEL<T,LTER> >		next;	//下一层级
	T								sobj;   //层的节点
	LTER						    iter;   //层的检索位置
	short							lid;	//层ID
};
typedef sTREE_LEVEL< P<GN::sNODE_> > sTREE_LEVEL_;	//基础树节点层级信息

class GN_API cTREE_NODE_P_OPTR :public cTREE_NODE_OPTR, public sTREE_LEVEL_  //★安全树节点操作器  !适合操作继承自 GN::sNODE<T> 的节点类型 操作小型树适中 大型树较快 树节点有变动会自动同步无需要手动提示刷新
{																						   //!所有基于继承自 GN::sNODE<T> 的树形连锁数据结构 都可以用
	sTREE_LEVEL_							*zCurLevel;		   //当前检索层级
	sTREE_LEVEL_							*zCurLevelBk;	   //最近备份的检索层级
	P<sTREE_LEVEL_>							 zNextBk;		   //下一层级备份
	P<sTREE_LEVEL_>							 zNextBk2;		   //下一层级备份2
	P<GN::sNODE_>							 zMsLastNode;	   //上次点选到的节点
	int										 zCurShowStartNid; //列表框可见起始节点ID

	sTREE_LEVEL_ *zBackupLevels(P<sTREE_LEVEL_> &next2);       //备份层级链里的内容(备份到的目标层首)返回指向备份目标里对应 zCurLevel 的层级
	bool zSeekLastNode();									   //跳转到上个检索节点 可反复循环历遍树所有节点 返回调用后检索节点是否为子节点
	bool zSeekNextNode();									   //跳转到下个检索节点 可反复循环历遍树所有节点 返回调用后检索节点是否为子节点
	void zSeekLastNode(int count);							   //向上跳转多个检索节点或到终点为止(跳转个数)
	void zSeekNextNode(int count);							   //向下跳转多个检索节点或到终点为止(跳转个数)
   public:
    P<GN::sNODE_> GetNodeAndSeekNext(short &level_n);		   //获得当前检索节点 并使内部跳转到下个检索节点的位置(返回所在层级数)
	short		  GetNodeOnLevel();							   //获得当前检索节点所在层级数

	virtual void OnUpdateBegin(unsigned int updatatime);				 //当列表框更新前的准备处理(帧间隔时间)
	virtual void OnReArranged(int id, float st_nid,unsigned int max_vnn);//当框滚动或被重排列后的处理(当前排列起始节点ID, 列表框可见起始节点ID, 树列表框当前最大可见节点数)
	virtual bool HasUnfold(P_ g);										 //判断指定节点是否含展开标记(节点)返回是否
	virtual void UnfoldTag(P_ g, uint8 on);								 //为指定节点设置展开标记(节点, 是否展开)
	virtual bool HasSelect(P_ node);									 //判断指定节点是否含选中标记(节点)返回是否
	virtual void SelectTag(P_ node, uint8 on, int8 mode);				 //为指定节点设置选中标记(节点, 是否选中, 选择模式 0:单选 1:段选起点 -1:段选)
	virtual int  CanDrawLastNodeNum(int num);							 //当绘制完可视节点后检测当前向上是否还有指定个数可绘制节点 以便测量可滚动量(指定节点数) 返回指定节点数 若可绘制节点数小于指定节点数就返回可绘制节点数
	virtual int  CanDrawNextNodeNum(int num);							 //当绘制完可视节点后检测当前向下是否还有指定个数可绘制节点 以便测量可滚动量(指定节点数) 返回指定节点数 若可绘制节点数小于指定节点数就返回可绘制节点数
	virtual void OnUpdateEnd();											 //当列表框更新结束后的处理
	//以上这几个虚函数定义顺序与被滚动列表树调用顺序一致

	cTREE_NODE_P_OPTR():aNodeIconIdAddrOffset(0),aCommentFunc(0),aSelectAddProc(0),aDrawNodeAddProc(0){}
	virtual ~cTREE_NODE_P_OPTR(){}
	virtual P<cTREE_NODE_OPTR> OnCloneOut();							 //复制出该对象的一个新副本
	virtual bool Search_exec(P_ g, bool unfold, bool (*proc)(P_ &node, void *us_arg), void *_us_arg); //使指定节点里的所有子节点和它都执行指定函数(指定节点 传入空指针表示跟节点, 是否仅检索展开的节点, 返回是否要继续查找 执行函数(当前检索到的节点, 传回给你的参数), 传给执行函数用的参数) 返回查找中途是否有被中断
	virtual void MoveNodes(P_ pos, bool pos_back, GN::sLIST<P_> *nodes); //移动一组节点到指定节点旁边(指定节点, 是否放在指定节点之后 否则在前, 一组节点)
	virtual char*GetNodeName(P_ &node);									 //获得指定节点名称
	virtual P_	 GetCurFristNode();										 //获得当前首个检索节点
	virtual int	 GetVisualNodeNum();									 //获得当前展开的可显示节点总数
	virtual void *GetDrawNodeFunc();									 //获得节点信息绘制的方法

	int	   aNodeIconIdAddrOffset;										 //绘制节点图标索引ID所在节点指针偏移量 !假如节点里的图标索引ID属性为icid 那么该值应该为 (uint8*)&(node->icid) - (uint8*)node

	void (*aCommentFunc)(GN::sNODE_ *tg, P<GN::sNODE_> node);			 //树子节点链接方法 !如您操作的派生类树节点修改过 GN::sNODE::Comment UnComment 等方法需要指定它为 TreeNodeComment<您的派生类>
	void (*aSelectAddProc)(P_ &node, uint8 on);							 //节点设置选中标记后的额外处理(节点, 是否选中)
	int  (*aDrawNodeAddProc)(GN::sNODE_ *node,int x, int y, int w, int h, unsigned int color, bool &has_focus, void *arg); //加插在绘制节点图标与信息之间的处理(节点, 安插位置X, 位置Y, 节点区块的宽度,高度, 区块底色, 该区块是否选择焦点, 挂载到的滚动列表树)返回处理占用到的区间长度

	void Init(P<GN::sNODE_> root, void (*comment_func)(GN::sNODE_*, P<GN::sNODE_>)=TreeNodeComment<GN::sNODE_>); //初始化(树根节点, 树节点连接方法)
	void Clear();																								 //清除树节点和相关中转辅助数据
};
template<class T>
//构建安全树节点操作器(树根节点)返回树节点操作器 !通常用于封装树根节点传给 cTREE_VIEW::InitStyleToData 首个参数用
P<cTREE_NODE_OPTR> MakeTreeNodeP_Operator(P<GN::sNODE_> root){
	P<cTREE_NODE_P_OPTR> optr(1);
	optr->Init(root, TreeNodeComment<T>);
	return optr;
}
};
#endif //_GN_UILISTTREE_H_