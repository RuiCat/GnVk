#ifndef _GN_UI_H_
#define _GN_UI_H_

namespace GNu{
enum eUI_TYPE {				//UI组件类型
	 eUI_BACK_FRAME = GNg::eOBJ_FRAME_LAUNCHER_FONT +1,	//底筐
	 eUI_IMAGE,					//靠边图像
	 eUI_FRAME_RC,				//靠边动画图筐
	 eUI_TEXT_ART,				//静态文本
	 eUI_TEXT_IN,				//输入筐
	 eUI_PALETTE,				//颜色版
	 eUI_SCROLL_RECT,			//滚动筐
	 eUI_SCROLL_LIST,			//滚动列表
	 eUI_SCROLL_LIST_GUI,		//滚动嵌套UI列表
	 eUI_SELECT_LIST_ICON,		//图标选择列表
	 eUI_SELECT_LIST_TEXT,		//文本选择列表
	 eUI_TREE_VIEW,				//滚动列表树
	 eUI_FILE_BROWSER,			//文件浏缆
	 eUI_SIM_DIALOG,			//简单对话筐
	 eUI_SELECT_BUTTON,			//选择按钮联
	 eUI_PAGE_BAR,				//分页栏
	 eUI_SLIDER,				//滑动条
	 eUI_BUTTON_TOUCH,			//触屏虚拟按钮
	 eUI_JOYSTICK,				//触屏虚拟控杆
	 eUI_VALUE_EXTRACT,			//数值提取描述
	 eUI_TYPE_NUM				//UI组件类型数
};
extern GN_API const char *UiObjTypeNames[eUI_TYPE_NUM];			//中文UI组件类型名 一一对应以上各个类型
extern GN_API const char *UiObjCodeClassNames[eUI_TYPE_NUM];	//UI组件对应代码里的类名 一一对应以上各个类型

enum eUI_ACTION_TYPE {	//UI组件交互触发动作类型
	 //适用类型 eUI_SELECT_BUTTON eUI_PAGE_BAR cSELECT_LIST_ICON cSELECT_LIST_TEXT cTREE_VIEW cUI_SIM_DIALOG
	 eUI_ACT_SELECT_ID	  = 1,		//首个选项点选ID  负数时表示取消点选
	 eUI_ACT_SELECT_IDMAX = 65535,	//最大选项点选ID

	 //适用类型 cFILE_BROWSER
	 eUI_ACT_SELECT_FILE  = 1,		//选到文件
	 eUI_ACT_SELECT_DIR	  =-1,		//选到目录

	 //适用类型 cSLIDER
	 eUI_ACT_RANGE_MIN	  = 0,		//值范围最小 通常表示浮点数0.0f 范围里其它值可以 /(float)65535 来获得
	 eUI_ACT_RANGE_MAX	  = 65535,	//值范围最大 通常表示浮点数1.0f

	 //适用类型 cSCROLL_LIST cSCROLL_LIST_GUI  cSELECT_LIST_ICON cSELECT_LIST_TEXT cTREE_VIEW cUI_SIM_DIALOG cFILE_BROWSER cUI_FRAME_LAUNCHER_FONT
	 eUI_ACT_VANISH		  = 65536,	//消失了

	//对于类型 cPALETTE 均为颜色变化值 您可以这样 *((unsigned int*)&action) 得到正确的颜色值
};
class GN_API cSHARE_DATA        //共享数据集 !通常在保存文件时用于收集所有共享对象以便一次过写入数据 或加载文件时用于一次性集中读取所有共享对象数据以便之后给其它对象引用
{ public:
	GN::sLIST< P<GNg::sIMAGE> > *pimage_list;		//指向图像列表
	P< P<GNg::sIMAGE> >			*pimage_refs;		//指向图像引用数组
	GN::sLIST<GNg::sIMAGE*>     image_list;		    //字体图列表
	P<GNg::sIMAGE*>				image_refs;			//字体图引用数组
	GN::sLIST< P<char> >		text_list;		    //文本列表
	P< P<char> >				text_refs;			//文本引用数组
	GN::sLIST<GNg::cTEXT_ART*>  text_style_list;	//文本式样列表
	P<GNg::cTEXT_ART*>			text_style_refs;	//文本式样引用数组
	
	GN::sLIST< unsigned int >   sobj_type_list;		//其它自定义共享对象类型列表
	GN::sLIST< P<uint8> >		sobj_list;		    //其它自定义共享对象列表
	P< P<uint8> >				sobj_refs;			//其它自定义共享对象引用数组

    virtual ~cSHARE_DATA(){}
};
class cUI_PTR;
class cUI_GROUP;

struct sUI_LEVEL :public GNg::sDRAW_AREA2	//UI层级信息
{
	cUI_GROUP				*node;		//层组节点
	sUI_LEVEL				*parent;	//父层级信息
};
class GN_API  iUI_OBJECT  //★UI组件基本类
{public:
	unsigned short			 aType;	  //类型 内置类型开头几个对应 GNg::eOBJ_TYPE 里的 然后接着为 GNu::eUI_TYPE 里的 再往后就是用户自定义类型

	virtual ~iUI_OBJECT(){}
	virtual P<iUI_OBJECT> Clone(){ P<iUI_OBJECT> ui(1); ui->aType = aType; return ui; }	//复制出该UI组件的一个新副本
	
	virtual void StatisticSize(cUI_GROUP *parent){}						  //统计UI占位大小并调整父层组的宽高使能包含整个UI占位区(父层组) !UI占位大小包含在组里的四方向边距 若想UI能靠边对齐 可以配合用sDRAW_IN_AREA::EnsureInclude来调整父层组宽高 !如您不介意父层UI组未能包含全UI组件可以不重写该函数  另外您也可以设置父层组的颜色为0 以使父层包含的所有子对象之后不再显示以节省性能开消
	virtual void OnRePos(sUI_LEVEL *parent){}							  //当父层UI组大小变更了要求组件成员重置位置时的处理(父层级信息链)
	virtual void OnMove(short mx, short my){}							  //当父层UI组平移拖动了要求组件成员偏移位置时的处理(X移动量, Y移动量)
    virtual void UpdateRun(unsigned int updatetime, sUI_LEVEL *parent){}  //更新运行(帧间隔时间, 父层级信息链) !绘制UI时为了使UI能跟随在组里 把UI坐标加上父层绘制位置坐标即可 如(ui->x + parent->x, ui->y + parent->y) 您也可以通过 parent->parent->parent->w 这样来获取到 上上上层的UI组绘制区域
	virtual void GetDrawArea(GNg::sDRAW_AREA2 &out, cUI_GROUP *parent){}  //获得UI组件的实际绘制区域(返回区域, 父层组) !需在 UpdateRun UpdatePlay 执行后再调用该方法才有正确值输出

	//把内部数据储存为一整段数据时的处理(收集共享对象集, 返回整段数据 首个字节为版本号之后4个字节为aType 当该值为0时仅返回数据段需要大小)返回数据段大小字节 !用户自定义UI组件的版本号应由1开始
	virtual unsigned int DataSave(cSHARE_DATA *share_objs, uint8 *data_out=0){ if(data_out)*data_out = 0; return 1; }

	//从文件数据段里读入数据时的处理(引用共享对象集, 数据段地址 首个字节为版本号之后4个字节为aType)返回已读取字节数 失败返回0 !用户自定义UI组件的版本号应由1开始
	virtual unsigned int DataLoad(cSHARE_DATA *share_objs, const uint8 *data){ return 1; }
};
extern GN_API P<iUI_OBJECT> (*UiObjectCreate)(unsigned int _type);  //创建UI组件的方法(UI组件类型)返回对应类型的UI组件 !把该函数指针指向一个能创建派生自定义UI组件的方法 便可以创建更多的自定义UI组件
extern GN_API unsigned int  (*UiShareObjSave)(unsigned int type, P<uint8>  obj, uint8 *data_out);   //储存自定义共享对象为一整段数据(共享对象类型, 共享对象, 返回整段数据 首个字节为版本号之后4个字节为参数type 当该值为0时仅返回数据段需要大小)返回数据段大小字节 !用户自定义的版本号应由1开始
extern GN_API unsigned int  (*UiShareObjLoad)(					 P<uint8> &obj, const uint8 *data); //从文件数据段里读出对应的自定义共享对象(返回自定义共享对象, 数据段地址 首个字节为版本号之后4个字节为type)返回已读取字节数 失败返回0 !用户自定义的版本号应由1开始

extern GN_API void (*UiActionCall)(iUI_OBJECT *uobj, int action);   //UI组件交互操作回调(触发动作的UI组件, 触发动作类型) !动作类型1至65535为点选ID 负数时表示取消点选 其他值参考 eUI_ACTION_TYPE !把该函数指针指向您的自定义触发动作处理方法 便可以收到UI交互操作的消息

extern GN_API iUI_OBJECT* RecvInputUiObject;		//当前可接收输入操作的UI组件对象 !UI组件通过检测是否为这个才做处理 就不影响在后面不关心的UI组件
extern GN_API iUI_OBJECT* RequestInputUiObject;		//请求输入操作的UI组件对象 您可以在UI场景调用UpdatePlay之后 使它指向某个UI组件 这样就只专注该UI组件输入

class GN_API cUI_NEAR_OBJECT : public iUI_OBJECT, public GNg::sDRAW_IN_AREA	//★UI靠边组件
{ public:
	uint8* zNearAreaDataSave(uint8 *wpos);								  //保存绘制里区的2进制数据(当前写入位置)返回最后的写完位置
	uint8* zNearAreaDataLoad(uint8 *rpos);								  //读取绘制里区的2进制数据(当前读取位置)返回最后的读完位置
	
	virtual void StatisticSize(cUI_GROUP *parent);						  //统计UI占位大小并调整父层组的宽高使能包含整个UI占位区(父层组)
	virtual void GetDrawArea(GNg::sDRAW_AREA2 &out, cUI_GROUP *parent);   //获得UI组件的实际绘制区域(返回区域, 父层组) !需在 UpdateRun UpdatePlay 执行后再调用该方法才有正确值输出
};
class GN_API cUI_GROUP : public iUI_OBJECT, public GNg::sDRAW_IN_AREA	//★UI组
{  public:
	short							lw, lh;		 //组最小大小的宽高
	short							gx, gy;		 //经过更新计算后得到的组绝对位置坐标
	short							gw, gh;		 //经过更新计算后得到的组绝对范围宽高
	unsigned int					gc;			 //经过更新计算后得到的组绝对绘制颜色
	bool							aUsScissor;  //是否剪裁掉组区域外的内容显示

	GN::sLIST< P<iUI_OBJECT> >		aSubs;		 //接入到它的所有子对象

	cUI_GROUP():gw(-1),lw(0),lh(0),aUsScissor(false){ aType = GNg::eOBJ_GROUP; }
//	virtual ~cUI_GROUP();
	virtual P<iUI_OBJECT> Clone();										 //复制出该UI组件的一个新副本
	
	virtual void StatisticSize(cUI_GROUP *parent);						 //统计UI占位大小并调整父层组的宽高使能包含整个UI占位区(父层组)
    virtual void UpdateRun(unsigned int updatetime, sUI_LEVEL *parent);  //更新运行(帧间隔时间, 父层级信息链)
	virtual void GetDrawArea(GNg::sDRAW_AREA2 &out, cUI_GROUP *parent);  //获得UI组  的实际绘制区域(返回区域, 父层组 您可以不指定它) !需在 UpdateRun UpdatePlay 执行后再调用该方法才有正确值输出

	//把内部数据储存为一整段数据时的处理(收集共享对象集, 返回整段数据 首个字节为版本号之后4个字节为aType 当该值为0时仅返回数据段需要大小)返回数据段大小字节 !用户自定义UI组件的版本号应由1开始
	virtual unsigned int DataSave(cSHARE_DATA *share_objs, uint8 *data_out=0);

	//从文件数据段里读入数据时的处理(引用共享对象集, 数据段地址 首个字节为版本号之后4个字节为aType)返回已读取字节数 失败返回0 !用户自定义UI组件的版本号应由1开始
	virtual unsigned int DataLoad(cSHARE_DATA *share_objs, const uint8 *data);

	void Resize(GNg::cVIEW2D *view=0);			//重设UI组区域默认大小(如有指定就设为与视口相同区域大小)

	void Comment(P<iUI_OBJECT> obj, cUI_PTR *ptr_out=0); //接入其它UI组件或UI组(其它UI组件或UI组, 如有指定返回插入后的组件地址) !组件包含的子对象 图片等资源也一并牵连进来 !只有接入到调用UpdatePlay的UI场景中才作显示 相对来说它才生存于这个场景世界中 连接应尽量避免连接该组件之上的任何父对象以免引起混乱
	cUI_PTR Search(P<iUI_OBJECT> obj);					 //在UI组里查找指定的UI组件( UI组件 ) 返回在UI组里的组件地址
};
struct GN_API sUI_IMAGE : public iUI_OBJECT, public GNg::sIMAGE_OBJ		//★静态图像
{
	sUI_IMAGE(){ aType = GNg::eOBJ_IMAGE; }

	virtual P<iUI_OBJECT> Clone();										 //复制出该UI组件的一个新副本
    virtual void UpdateRun(unsigned int updatetime, sUI_LEVEL *parent);  //更新运行(帧间隔时间, 父层级信息链)
	virtual void GetDrawArea(GNg::sDRAW_AREA2 &out, cUI_GROUP *parent);  //获得UI组  的实际绘制区域(返回区域, 父层组) !需在 UpdateRun UpdatePlay 执行后再调用该方法才有正确值输出

	//把内部数据储存为一整段数据时的处理(收集共享对象集, 返回整段数据 首个字节为版本号之后4个字节为aType 当该值为0时仅返回数据段需要大小)返回数据段大小字节 !用户自定义UI组件的版本号应由1开始
	virtual unsigned int DataSave(cSHARE_DATA *share_objs, uint8 *data_out=0);

	//从文件数据段里读入数据时的处理(引用共享对象集, 数据段地址 首个字节为版本号之后4个字节为aType)返回已读取字节数 失败返回0 !用户自定义UI组件的版本号应由1开始
	virtual unsigned int DataLoad(cSHARE_DATA *share_objs, const uint8 *data);
};
class GN_API cUI_IMAGE : public cUI_NEAR_OBJECT	//★靠边图像
{  public:
	P<GNg::sIMAGE>					img;		//图像
	unsigned short					bmode;		//混合模式

	cUI_IMAGE(){ aType = eUI_IMAGE; }
	void DrawRangeGet(GNg::sDRAW_AREA2 &out, GNg::sDRAW_AREA2 *rng);  //获得里区在外区的绘制范围和颜色(返回受影响后的里区范, 外区)

	virtual P<iUI_OBJECT> Clone();										 //复制出该UI组件的一个新副本
    virtual void StatisticSize(cUI_GROUP *parent);						 //统计UI占位大小并调整父层组的宽高使能包含整个UI占位区(父层组)
	virtual void UpdateRun(unsigned int updatetime, sUI_LEVEL *parent);  //更新运行(帧间隔时间, 父层级信息链)
	virtual void GetDrawArea(GNg::sDRAW_AREA2 &out, cUI_GROUP *parent);  //获得UI组  的实际绘制区域(返回区域, 父层组) !需在 UpdateRun UpdatePlay 执行后再调用该方法才有正确值输出

	//把内部数据储存为一整段数据时的处理(收集共享对象集, 返回整段数据 首个字节为版本号之后4个字节为aType 当该值为0时仅返回数据段需要大小)返回数据段大小字节
	virtual unsigned int DataSave(cSHARE_DATA *share_objs, uint8 *data_out=0);

	//从文件数据段里读入数据时的处理(引用共享对象集, 数据段地址 首个字节为版本号之后4个字节为aType)返回已读取字节数 失败返回0 
	virtual unsigned int DataLoad(cSHARE_DATA *share_objs, const uint8 *data);
};
class GN_API cUI_BACK_FRAME : public sUI_IMAGE 							//★底筐
{																		  //支持圆角边筐和位置大小拖拉操作
	uint8							zDragMask;   //拖动标记位 1:顶栏 2:顶边 4:下边 8:左边 16:右边
   public:
	bool						    aBackRepeat; //底框9宫格图是否重复平铺填满中间内容
	uint8							aBackBlurLv; //底框模糊级别 1-4  0表示禁用
	uint8							aZoom;		 //可否拖拉边框缩放
	uint8							aDragBarH;	 //顶端拖动栏高 当该值非0时 筐顶可以拖动并影响到上级UI组和里面的内容
	
	cUI_BACK_FRAME():aBackBlurLv(0),aBackRepeat(false),aZoom(0),aDragBarH(0){ aType = eUI_BACK_FRAME; }
	
	virtual P<iUI_OBJECT> Clone();										 //复制出该UI组件的一个新副本
	virtual void StatisticSize(cUI_GROUP *parent);						 //统计UI占位大小并调整父层组的宽高使能包含整个UI占位区(父层组)
	virtual void UpdateRun(unsigned int updatetime, sUI_LEVEL *parent);  //更新运行(帧间隔时间, 父层级信息链)
	virtual void GetDrawArea(GNg::sDRAW_AREA2 &out, cUI_GROUP *parent);  //获得UI组  的实际绘制区域(返回区域, 父层组) !需在 UpdateRun UpdatePlay 执行后再调用该方法才有正确值输出

	//把内部数据储存为一整段数据时的处理(收集共享对象集, 返回整段数据 首个字节为版本号之后4个字节为aType 当该值为0时仅返回数据段需要大小)返回数据段大小字节 !用户自定义UI组件的版本号应由1开始
	virtual unsigned int DataSave(cSHARE_DATA *share_objs, uint8 *data_out=0);

	//从文件数据段里读入数据时的处理(引用共享对象集, 数据段地址 首个字节为版本号之后4个字节为aType)返回已读取字节数 失败返回0 !用户自定义UI组件的版本号应由1开始
	virtual unsigned int DataLoad(cSHARE_DATA *share_objs, const uint8 *data);
};
struct GN_API sUI_FRAME_RC : public iUI_OBJECT, public GNg::cFRAME_RC	//★正规动画图筐
{																		 //您可通过设置它的动画播放模式为 GNg::eAM_ONCE_KEEP + 1 以使包含它的上一级UI组范围和颜色受它影响 而该动画图筐就可作底筐来显示 !如您不想显示它而有其它底筐 那么您可以设置它的混合模式 aBlendmode 为0 这样就只拿它的自身动画事件来影响上级UI组 而忽略它外面的位置与比例影响
	sUI_FRAME_RC(){ aType = GNg::eOBJ_FRAME_RC; }
	
	virtual P<iUI_OBJECT> Clone();										 //复制出该UI组件的一个新副本
    virtual void UpdateRun(unsigned int updatetime, sUI_LEVEL *parent);  //更新运行(帧间隔时间, 父层级信息链)
	virtual void GetDrawArea(GNg::sDRAW_AREA2 &out, cUI_GROUP *parent);  //获得UI组  的实际绘制区域(返回区域, 父层组) !需在 UpdateRun UpdatePlay 执行后再调用该方法才有正确值输出 该类不会返回区域的宽和高

	//把内部数据储存为一整段数据时的处理(收集共享对象集, 返回整段数据 首个字节为版本号之后4个字节为aType 当该值为0时仅返回数据段需要大小)返回数据段大小字节
	virtual unsigned int DataSave(cSHARE_DATA *share_objs, uint8 *data_out=0);

	//从文件数据段里读入数据时的处理(引用共享对象集, 数据段地址 首个字节为版本号之后4个字节为aType)返回已读取字节数 失败返回0 
	virtual unsigned int DataLoad(cSHARE_DATA *share_objs, const uint8 *data);
};
class GN_API cUI_FRAME_RC : public cUI_NEAR_OBJECT, public GNg::cFRAME_RC	//★靠边动画图筐
{public:
	cUI_FRAME_RC(){ aType = eUI_FRAME_RC; }
	void zSetDrawPos(short dx, short dy);								 //设置绘制的绝对坐标(坐标X, 坐标Y)

	virtual P<iUI_OBJECT> Clone();										 //复制出该UI组件的一个新副本
    virtual void UpdateRun(unsigned int updatetime, sUI_LEVEL *parent);  //更新运行(帧间隔时间, 父层级信息链)
	virtual void GetDrawArea(GNg::sDRAW_AREA2 &out, cUI_GROUP *parent);  //获得UI组件的实际绘制区域(返回区域, 父层组) !需在 UpdateRun UpdatePlay 执行后再调用该方法才有正确值输出

	//把内部数据储存为一整段数据时的处理(收集共享对象集, 返回整段数据 首个字节为版本号之后4个字节为aType 当该值为0时仅返回数据段需要大小)返回数据段大小字节
	virtual unsigned int DataSave(cSHARE_DATA *share_objs, uint8 *data_out=0);

	//从文件数据段里读入数据时的处理(引用共享对象集, 数据段地址 首个字节为版本号之后4个字节为aType)返回已读取字节数 失败返回0 
	virtual unsigned int DataLoad(cSHARE_DATA *share_objs, const uint8 *data);
};
class GN_API cUI_FRAME : public iUI_OBJECT, public GNg::cFRAME	   //★动画图筐
{public:
	cUI_FRAME(){ aType = GNg::eOBJ_FRAME; }

	virtual P<iUI_OBJECT> Clone();										 //复制出该UI组件的一个新副本
    virtual void UpdateRun(unsigned int updatetime, sUI_LEVEL *parent);  //更新运行(帧间隔时间, 父层级信息链)
	virtual void GetDrawArea(GNg::sDRAW_AREA2 &out, cUI_GROUP *parent);  //获得UI组  的实际绘制区域(返回区域, 父层组) !需在 UpdateRun UpdatePlay 执行后再调用该方法才有正确值输出

	//把内部数据储存为一整段数据时的处理(收集共享对象集, 返回整段数据 首个字节为版本号之后4个字节为aType 当该值为0时仅返回数据段需要大小)返回数据段大小字节
	virtual unsigned int DataSave(cSHARE_DATA *share_objs, uint8 *data_out=0);

	//从文件数据段里读入数据时的处理(引用共享对象集, 数据段地址 首个字节为版本号之后4个字节为aType)返回已读取字节数 失败返回0 
	virtual unsigned int DataLoad(cSHARE_DATA *share_objs, const uint8 *data);
};
class GN_API cUI_FRAME_LAUNCHER : public iUI_OBJECT, public GNg::cFRAME_LAUNCHER	 //★图筐发射器
{public:
	cUI_FRAME_LAUNCHER(){ aType = GNg::eOBJ_FRAME_LAUNCHER; }

	virtual P<iUI_OBJECT> Clone();										 //复制出该UI组件的一个新副本
    virtual void UpdateRun(unsigned int updatetime, sUI_LEVEL *parent);  //更新运行(帧间隔时间, 父层级信息链)
	virtual void GetDrawArea(GNg::sDRAW_AREA2 &out, cUI_GROUP *parent);  //获得UI组  的实际绘制区域(返回区域, 父层组) !需在 UpdateRun UpdatePlay 执行后再调用该方法才有正确值输出

	//把内部数据储存为一整段数据时的处理(收集共享对象集, 返回整段数据 首个字节为版本号之后4个字节为aType 当该值为0时仅返回数据段需要大小)返回数据段大小字节
	virtual unsigned int DataSave(cSHARE_DATA *share_objs, uint8 *data_out=0);

	//从文件数据段里读入数据时的处理(引用共享对象集, 数据段地址 首个字节为版本号之后4个字节为aType)返回已读取字节数 失败返回0 
	virtual unsigned int DataLoad(cSHARE_DATA *share_objs, const uint8 *data);
};
class GN_API cUI_FRAME_LAUNCHER2D : public iUI_OBJECT, public GNg::cFRAME_LAUNCHER2D //★复合发射器
{public:
	cUI_FRAME_LAUNCHER2D(){ aType = GNg::eOBJ_FRAME_LAUNCHER2D; }

	virtual P<iUI_OBJECT> Clone();										 //复制出该UI组件的一个新副本
    virtual void UpdateRun(unsigned int updatetime, sUI_LEVEL *parent);  //更新运行(帧间隔时间, 父层级信息链)
	virtual void GetDrawArea(GNg::sDRAW_AREA2 &out, cUI_GROUP *parent);  //获得UI组  的实际绘制区域(返回区域, 父层组) !需在 UpdateRun UpdatePlay 执行后再调用该方法才有正确值输出

	//把内部数据储存为一整段数据时的处理(收集共享对象集, 返回整段数据 首个字节为版本号之后4个字节为aType 当该值为0时仅返回数据段需要大小)返回数据段大小字节
	virtual unsigned int DataSave(cSHARE_DATA *share_objs, uint8 *data_out=0);

	//从文件数据段里读入数据时的处理(引用共享对象集, 数据段地址 首个字节为版本号之后4个字节为aType)返回已读取字节数 失败返回0 
	virtual unsigned int DataLoad(cSHARE_DATA *share_objs, const uint8 *data);
};
class GN_API cUI_FRAME_LAUNCHER_FONT : public cUI_NEAR_OBJECT, public GNg::cFRAME_LAUNCHER_FONT  //★字筐发射器
{public:
	cUI_FRAME_LAUNCHER_FONT(){ aType = GNg::eOBJ_FRAME_LAUNCHER_FONT; c = 0xffffffff/*该类忽略该值*/; }

	virtual P<iUI_OBJECT> Clone();										 //复制出该UI组件的一个新副本
    virtual void OnRePos(sUI_LEVEL *parent);							 //当父层UI组大小变更了要求组件成员重置位置时的处理(父层级信息链)
	virtual void OnMove(short mx, short my);							 //当父层UI组平移拖动了要求组件成员偏移位置时的处理(X移动量, Y移动量)
	virtual void UpdateRun(unsigned int updatetime, sUI_LEVEL *parent);  //更新运行(帧间隔时间, 父层级信息链)
	virtual void GetDrawArea(GNg::sDRAW_AREA2 &out, cUI_GROUP *parent);  //获得UI组件的实际绘制区域(返回区域, 父层组) !需在 UpdateRun UpdatePlay 执行后再调用该方法才有正确值输出

	//把内部数据储存为一整段数据时的处理(收集共享对象集, 返回整段数据 首个字节为版本号之后4个字节为aType 当该值为0时仅返回数据段需要大小)返回数据段大小字节
	virtual unsigned int DataSave(cSHARE_DATA *share_objs, uint8 *data_out=0);

	//从文件数据段里读入数据时的处理(引用共享对象集, 数据段地址 首个字节为版本号之后4个字节为aType)返回已读取字节数 失败返回0 
	virtual unsigned int DataLoad(cSHARE_DATA *share_objs, const uint8 *data);
};
class GN_API cUI_TEXT_ART : public cUI_NEAR_OBJECT	//★静态文本
{public:
	GNg::cTEXT_ART				   *textstyle; //文本式样

	char			               *text;		//文本内容
	unsigned short				    blendmode;	//混合模式
	short							offsetx;	//在筐内的偏移X量
	short							offsety;	//在筐内的偏移Y量
	short							fw;			//文本筐宽
	short							fh;			//文本筐高

	cUI_TEXT_ART():textstyle(&GN::RunRec.aFont), text(0){ aType = eUI_TEXT_ART; }
    virtual ~cUI_TEXT_ART(){ GN_DeleteArray(text); }

    void InitDefault(){ blendmode = 1; offsetx = 0; offsety = 0; fw = 0; fh = 0; } //初始化其它值为默认
	virtual P<iUI_OBJECT> Clone();										 //复制出该UI组件的一个新副本
    virtual void UpdateRun(unsigned int updatetime, sUI_LEVEL *parent);  //更新运行(帧间隔时间, 父层级信息链)
	virtual void GetDrawArea(GNg::sDRAW_AREA2 &out, cUI_GROUP *parent);  //获得UI组件的实际绘制区域(返回区域, 父层组) !需在 UpdateRun UpdatePlay 执行后再调用该方法才有正确值输出

	//把内部数据储存为一整段数据时的处理(收集共享对象集, 返回整段数据 首个字节为版本号之后4个字节为aType 当该值为0时仅返回数据段需要大小)返回数据段大小字节
	virtual unsigned int DataSave(cSHARE_DATA *share_objs, uint8 *data_out=0);

	//从文件数据段里读入数据时的处理(引用共享对象集, 数据段地址 首个字节为版本号之后4个字节为aType)返回已读取字节数 失败返回0 
	virtual unsigned int DataLoad(cSHARE_DATA *share_objs, const uint8 *data);
};
//便捷拉出UI组件地址里的UI组件指针( UI组件类型 , UI组件地址 )
#define CUP( CLASS , PTR ) (*((P<CLASS>*)(PTR).aPtr))

class GN_API cUI_PTR //★★★ UI组件地址 用于控制组件的层级关系
{  public:		 
	cUI_GROUP				      *zParent; //父层组指针
	
	P<iUI_OBJECT>				  *aPtr;	//指向它所在父层组链表里的地址

	cUI_PTR():aPtr(0),zParent(0){}

	void UnComment(P<iUI_OBJECT> *obj=0);					  //解除UI组件与父层组的连接(如有指定 返回该UI组件) !解除后对象UI组件地址便会失效
	void InsertFront(P<iUI_OBJECT> obj, cUI_PTR *ptr_out=0);  //插入其它UI组件或UI组在它前面(其它UI组件或UI组, 如有指定返回插入后的组件地址) !组件包含的子对象 图片等资源也一并牵连进来 其它注意项同上
	void InsertBack (P<iUI_OBJECT> obj, cUI_PTR *ptr_out=0);  //插入其它UI组件或UI组在它后面(其它UI组件或UI组, 如有指定返回插入后的组件地址) !组件包含的子对象 图片等资源也一并牵连进来 其它注意项同上
	int  OrderMove(int fn);									  //移动UI图层顺序(向前移动层数 负值就向后移动)返回实际移动了的层数 当移到最前或最后时会提前返回

	cUI_GROUP *GetParent(){ return zParent; } //获得UI组件的父层组指针
	cUI_PTR    Clone();						  //复制出该UI组件的副本到一个新地址里
};
class GN_API cUI_SCENE : public cUI_GROUP		//★★★ UI场景
{  public:
	static bool						zNeedScreenImg;	    //所有绘制UI场景是否需要屏幕截图
	static unsigned int				zNeedScreenImgObjN;	//所有需要屏幕截图的UI组件数
	static void zDrawFinishProc();						//所有UI场景绘制完毕的善后处理
   
	P<char*>						zObjNameTable;      //UI场景里所有组件对象名的查找表 里面每个指针指向对象名缓冲里名称的开始位置 存放顺序与递归检索场景树的所有对象访问顺序一致 通常用于找到指定对象的指针用
	char						   *zObjNameBuf;	    //UI场景里所有组件对象名的存放缓冲 中间以\0结束符隔开 如你直接访问只访问到首个名

	GN::sLIST< P<GNg::sIMAGE> >		aFontMapList;	    //所引用到的字体图列表
	GN::sLIST< P<GNg::cTEXT_ART> >  aTextStyleList;	    //所引用到的文本式样列表

	cUI_SCENE():zObjNameBuf(0){}
	virtual ~cUI_SCENE(){ this->Release(); }
	
	void Release();		  //释放内部资源

	//在UI场景里查找指定名的UI组件( UI组件名 ) 返回在场景里的组件地址 !这个必须在场景刚加载后还没插入或删除过里面的对象前调用
	cUI_PTR Search(const char *name);

	//初始化加载UI场景文件(文件名 仅支持gui专属文件, 读取进度输出) 返回是否成功
	bool InitLoadToData(const char *file_name, float &progress);

	//保存UI场景内容到文件(文件名 仅支持gui专属文件, 文件版本, 是否导出图片文件, 如有指定则仅保存该场景或群组节点下的内容) 返回是否成功
	bool SaveToFile(const char *filename, uint8 version=0, bool exp_img=false, cUI_GROUP *node=0);

	void PrintAllNodeClassName();										  //打印出所有子节点UI组件代码类名和名称

	void UpdatePlay(unsigned int updatetime);							  //更新播放UI场景(更新时间)

    virtual void UpdateRun(unsigned int updatetime, sUI_LEVEL *parent);	  //更新运行(帧间隔时间, 父层级信息链)
};
#define GNu_SAVE_GUI_VERSION  0	//当前最新UI专用文件版本

//打印出UI组件对应的代码类名(UI组件)
inline void UiObjectClassNamePrint(iUI_OBJECT *ug){ GN_Log("%s\n", ug->aType >= eUI_TYPE_NUM? "?UNKNOWN" : UiObjCodeClassNames[ug->aType]); }

//在UI组树里查找指定序号顺序的UI组件(UI组, 序号 0:为UI组自身)返回UI组件 返回空指针表示序号超出组件总数范围
GN_API P<iUI_OBJECT> UiObjectSearch(P<iUI_OBJECT> &ug, unsigned int &id);

//在UI组树里查找指定UI组件所在顺序序号(UI组, 指定UI组件)返回顺序序号
GN_API unsigned int  UiObjectIdSearch(iUI_OBJECT *ug, iUI_OBJECT *tg);

//收集UI组里的所有子对象类型(UI组, 存放各个类型的数组)返回数组中类型存放到的最后位置指针
GN_API unsigned short *UiObjectsTypeCollect(iUI_OBJECT *ug, unsigned short *type);

};//end namespace GNu
#endif  //endif _GN_UI_H_
