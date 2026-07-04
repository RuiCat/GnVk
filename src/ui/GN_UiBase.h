#ifndef _GN_UIBASE_H_
#define _GN_UIBASE_H_

void GT_ScrollListGuiDrawCall (int,int,int,int,unsigned int ,unsigned int ,unsigned int ,bool,void*);
void GT_SelectListIconDrawCall(int,int,int,int,unsigned int ,unsigned int ,unsigned int ,bool,void*);
void GT_SelectListTextDrawCall(int,int,int,int,unsigned int ,unsigned int ,unsigned int ,bool,void*);

#define GT_DEF_FOCUS_COLOR 0xffff3434   //滚动筐初始化默认焦点颜色

namespace GNu{
class GN_API cSCROLL_CTRL       //滚动控制器
{  protected:
	bool                        zIsMouseFocus;   //当前鼠标是否在筐内
    bool                        zKeyOnDown;      //当前是否在筐内鼠右键或滚上滚下有按紧
    int8                        zRepeatNum;      //重复铺盖移动栏的图案数
    float                       zTextOffset;     //当前筐内区域偏移位置
	float                       zTextOffsettg;   //目标筐内区域偏移位置
    float                       zOffsetSheep;    //区域在筐内偏移速度 0.05f至1.0f

	GNg::cFRAME					zArrowAm;        //拖动箭头动画筐
	P<GNg::sIMAGE>			    zOffsetArrow;    //拖动箭头图
	GNg::sAM_EVENT				zFev;            //闪动动画事件	
   public:
	unsigned int                aFocusColor;     //滚条焦点颜色
	unsigned short              aFocusBlend;     //滚条焦点混合模式

    cSCROLL_CTRL();

	//设置筐内容滚动速度(速度:0.05f至1.0f) !如果该值为0.0f 它将不能滚动
	inline void SetOffsetEF(float sheep=0.1f){  zOffsetSheep = sheep; }

	//设置滚动拉边效果式样( 滚条闪动间隔时间 0为不闪动,  遍移拖动箭头拦式样图,  拖动拦图案重复铺盖数 0:就拉伸到边沿) !最好能在调用InitStyleToData之前调用这个
	void SetOffsetBorderEF(short focus_flash_time, P<GNg::sIMAGE> offset_arrow = P_(), int8 repeat_num = 0);

	//获得焦点经过动画变化影响后的颜色
	inline unsigned int  GetFocusChangeColor(){ return zArrowAm.aLevel_color; }

	inline P<GNg::sIMAGE> &OffsetArrow(){ return zOffsetArrow; }		 //引用拖动箭头图

	inline void RePos(){ zTextOffset = 0.0f; zTextOffsettg = 0.0f; }     //使滚动筐归位

	inline bool IsMouseFocus(){ return zIsMouseFocus; }           //返回鼠标是否瞄到该对象

	void   zSpecClone();													 //分离克隆副本不能共享的属性
	uint8* zDataSave(uint8 *wpos, GN::sLIST< P<GNg::sIMAGE> > &img_collect); //保存该对象的2进制数据(当前写入位置, 收集到的图像列表)返回最后的写完位置
    uint8* zDataLoad(uint8 *rpos, P< P<GNg::sIMAGE>	>		  &img_refs);	 //读取该对象的2进制数据(当前读取位置, 图像引用组)返回最后的读完位置
};
class GN_API cSCROLL_RECT  :public cUI_GROUP, public cSCROLL_CTRL  //★滚动筐 只能上下滚动的筐
{  protected:
	unsigned int                zMaxOffset;      //最大向下滚动偏移量
	void zResetOffsetAndAm();					 //重置框内偏移和动画
   public:
	cSCROLL_RECT(){ aType = eUI_SCROLL_RECT; }

	//初始化设置滚动区域大小(  滚区左上坐标X,               坐标Y,                    宽度,                     高度,                 可滚动总高度,                               滚条焦点颜色,                 滚条焦点混合模式) !必需
	void InitStyleToData(short x = NearLx(0), short y = NearTy(0), short w = GNg::ViewCur->orthoW, short h = GNg::ViewCur->orthoH, short total_h=GNg::ViewCur->orthoH<<1, unsigned int  focus_color=GT_DEF_FOCUS_COLOR, unsigned short focus_blend=GNg::eBLEND_BRIGHT);

	//初始化设置滚动区域大小(UI组,						可滚动总高度,                               滚条焦点颜色,						 滚条焦点混合模式) !必需
	void InitStyleToData(const cUI_GROUP *ui_group, short total_h=GNg::ViewCur->orthoH<<1, unsigned int  focus_color=GT_DEF_FOCUS_COLOR, unsigned short focus_blend=GNg::eBLEND_BRIGHT);

    //更新运行滚动筐(帧间隔时间) 返回0表示有错  返回1表示正在运行中
	unsigned int UpdateRun(unsigned int updatetime);

	void  SetPos(short  x,short  y,short  w,short  h);			   //设置滚动筐位置大小(左上角坐标X, 左上角坐标Y, 筐宽, 筐高)
	void  GetPos(short *x,short *y,short *w,short *h);			   //获得滚动筐位置大小(左上角坐标X输出, 左上角坐标Y输出, 宽度输出, 高度输出)
	void  SetOffsetRatio(float value);							   //手动设置筐内偏移比率(比率0.0f至1.0f)
	inline short GetColOffset(){ return (short)zTextOffset; }      //获得筐内的滚动垂直偏移量()返回偏移量 一般使用时用原显示坐标Y加这个返回值就行了
	void  StatisticFitMaxOffset();								   //统计滚动筐里包含的内容适合的滚动偏移量

	virtual P<iUI_OBJECT> Clone();								   //复制出该UI组件的一个新副本

	//更新运行滚动筐和它包含的所有子UI组件(帧间隔时间, 父层级信息链)
	virtual void UpdateRun(unsigned int updatetime, sUI_LEVEL *parent);

	//把内部数据储存为一整段数据时的处理(收集共享对象集, 返回整段数据 首个字节为版本号之后4个字节为aType 当该值为0时仅返回数据段需要大小)返回数据段大小字节 !用户自定义UI组件的版本号应由1开始
	virtual unsigned int DataSave(cSHARE_DATA *share_objs, uint8 *data_out=0);

	//从文件数据段里读入数据时的处理(引用共享对象集, 数据段地址 首个字节为版本号之后4个字节为aType)返回已读取字节数 失败返回0 !用户自定义UI组件的版本号应由1开始
	virtual unsigned int DataLoad(cSHARE_DATA *share_objs, const uint8 *data);
};
class GN_API cSCROLL_LIST :public cUI_NEAR_OBJECT, public cSCROLL_CTRL //★滚动列表  能上下或水平滚动并可显示多项子分块内容的筐
{
	friend class cSIM_DIALOG;
   protected:
	static unsigned short       zCreateTotalNum; //该对象的总创建数
	static GNg::sIMAGE		   *zInternalImg;    //内部调用的传值图象指针
	static P<GNg::cFRAME>		zDefaultFrameAct;//默认点中的演变效果筐

	P<GNg::cFRAME_LAUNCHER_FONT> zFFrames;       //用于显示内容的字筐
    P<GNg::sDRAW_AREA>          zDrawRangeData;  //每帧绘制区范围数据
    unsigned int                zZoningNum;      //绘制区域数
	bool                        zReArrange;      //是否需要重新排列图标
	bool                        zIsErectArrange; //是否竖方向排列
	int8						zState;		     //最近运行状态 0:表示区域内容已经播放结束消失了 1滚筐正在拖动中 2待操作显示了
	unsigned short              zLastStartPos;   //当zZoningNum超出排版限制数量时 记录上次的排列起始区域号

	P<GNg::sIMAGE>				zFocusImg;       //选中焦点底图
	P<GNg::cFRAME>				zFrameIcon;      //默认弹出 动画效果筐
	P<GNg::sAM_EVENT>			zFev2;           //默认弹出 动画帧事件

	void  *zUsArg;								 //保存用户参数
	void (*zDrawCall)(int _x, int _y, int _w, int _h, unsigned int color, unsigned int zid, unsigned int vid, bool has_focus); //保存绘制回调

	//当滚动筐被重排列后的处理(当前排列起始区域号, 上次的排列起始区域号, 排列是否一起重设了所有区域的弹出效果)
	virtual void zOnReArranged(unsigned short sp, unsigned short sp_last, bool ret_am){ }
	//当滚动筐要添加滚动偏移的处理(偏移量)返回是否添加并处理了这个偏移
	virtual bool zOnAddScroolOffset(int move_dist){ return false; }
	
	//检测是否要重设绘制区域分块个数(各区域索引ID, 之前用于显示内容的字筐)
	void   zCheckResetZoningCount(char *pic_id, void *last_fframe);
	void   zSpecClone();									//分离克隆副本不能共享的属性
	uint8* zDataSave(uint8 *wpos, cSHARE_DATA *share_objs); //保存该对象的2进制数据(当前写入位置, 收集共享对象集)返回最后的写完位置
    uint8* zDataLoad(uint8 *rpos, cSHARE_DATA *share_objs);	//读取该对象的2进制数据(当前读取位置, 引用共享对象集)返回最后的读完位置
   public:
	static GNg::cFRAME *CheckInitDefaultFrameAct();			//检测默认点中的演变效果筐如没创建 就创建它并返回它

	bool                        aShowBG;         //是否要显示各格的背景 默认false
	bool                        aShowInFrame;    //内容是否只在滚动筐内显示 而不能显示出界 默认true

    cSCROLL_LIST();
   ~cSCROLL_LIST();

    //设置区块弹出的动画效果 区域分块排列方向按 effect 参数里给定 所以调用这个就可不用调用 SetArrangeIsErect了 !要设置效果最好能在调用InitStyleToData之前调用它
	void SetEffect(
		short						 focus_flash_time    ,   //滚条闪动间隔时间 0为不闪动
		P<GNg::cFRAME_LAUNCHER_FONT> effect        = P_(),   //区域弹出动画效果 该参数在运行后将会受到改变
        P<GNg::sIMAGE>				 focus_style   = P_(),   //选中焦点底图
		P<GNg::sIMAGE>				 offset_arrow  = P_(),   //拖动箭头拦式样图
		int8						 repeat_num    = 0       //拖动拦图案重复铺盖数 0:就拉伸到边界
	);
	//设置滚动列表默认动画区域分块是否按竖方向排列(是否) 默认是按竖方向排列的 设置它最好能在调用InitStyleToData之前调用 该调用和 SetEffect 是二选一的
	inline void SetArrangeIsErect(bool yes){ zIsErectArrange = yes; }

    //初始化设置显示式样 !必需
    bool InitStyleToData( //各可视区域的绘制处理回调(区域的位置X,Y,区域的宽度,高度, 区域底色, 当前要描绘的是第几个区域, 描绘区域是第几个显示区域, 该区域是否选择焦点)
	            void (*draw_func)(int _x, int _y, int _w, int _h, unsigned int  color, unsigned int  zid, unsigned int  vid, bool has_focus),
	            short _x             = NearLx(0),            //滚动列表左上坐标X
				short _y             = NearTy(0),            //滚动列表左上坐标Y
				short _w             = GNg::ViewCur->orthoW, //滚动列表宽度
				short _h             = GNg::ViewCur->orthoH, //滚动列表高度
				bool  default_am    = true,                  //滚动列表默认动画效果 在没调用SetEffect指定生成效果筐或第一次调用该函数时才生这个默认效果 默认效果的区域底色是幻彩变化的
				short dw            = 0,                     //绘制区域宽度 默认0:跟筐的宽度一样
				short dh            = 0,                     //绘制区域高度 默认0:跟筐的高度度一半大小
	   unsigned int   zoning_num    = 1,                     //绘制区域分块个数
	   unsigned int   focus_color   = GT_DEF_FOCUS_COLOR,    //焦点颜色
	   unsigned short focus_blend   = GNg::eBLEND_BRIGHT     //焦点混合模式
	); //返回是否成功

	//设置各可视区域的绘制处理回调( 绘制处理回调(区域的位置X,Y,区域的宽度,高度, 区域底色, 当前要描绘的是第几个区域, 描绘区域是第几个显示区域, 该区域是否选择焦点, 回传的用户参数), 传给回调用的用户参数) !这个可以设置带用户参数的回调 当您要设置这个时 必需先调用InitStyleToData 且首个参数填0即可
	void SetZoningDrawCall(void (*draw_func)(int _x, int _y, int _w, int _h, unsigned int color, unsigned int zid, unsigned int vid, bool has_focus, void *us_arg), void *usarg);
	
	//设置绘制区域分块个数
	void SetZoningCount(unsigned int zoning_num);	

	//更新运行滚动列表(帧间隔时间, 当自动重排筐区域时背景图标是否都引用相同的, 实时总体色调)返回滚动筐里的区域被重设之后的运行经历时间 如果返回0表示区域内容已经播放结束消失了 返回1表示滚筐正在拖动中
	unsigned int UpdateRun(unsigned int updatetime, bool bg_eq=true, unsigned int color=0xffffffff);

	void  OkHide();                                         //隐藏滚动列表 当有设置动画效果时 确定好后调用这个就可隐藏
	void  RePop(){ zReArrange = true; }                     //重弹出滚动列表内容 通常用来恢复 OkHide 所隐藏的内容
	void  RePos();                                          //滚动归位
	void  Transpose();                                      //使区域分块横竖排列摆向更变

	void  SetPos(short  x,short  y,short  w,short  h);      //设置显示位置大小(左上位置X, 左上位置Y, 筐宽, 筐高)
	void  GetPos(short *x,short *y,short *w,short *h);      //获得显示位置大小(左上位置X输出, 左上位置Y输出, 宽输出, 高输出)
	void  SetPos(GNg::sRECT2 &range);					    //设置显示位置范围(范围矩型)
	void  GetPos(GNg::sRECT2 &range_out);					//获得显示位置范围(范围矩型输出)
	GNg::sVEC2 GetPosXY();									//获得左上角显示位置坐标()返回坐标
	inline int8 GetState(){ return zState; }				//返回最近运行状态 0:表示区域内容已经播放结束消失了 1滚筐正在拖动中 2待操作显示了 !当调用无返回值的UpdateRun时就会用这个来缓返回值
//	bool  IsMouseFocus();									//返回鼠标是否瞄到该对象 !已在基类有定义
	short GetParalleShowCount();                            //获取滚动筐内的最大并排显示区域数
	void  SetOffsetRatio(float value, float flow_r=0.0f);   //手动设置筐内偏移的比率(值0.0f至1.0f, 推进度0.0f至1.0f 默认调用后会有滚动动画 在经过一段时间后才会达到偏移比率位置 该参数可以让你把这个过程推进一下)
	int   GetTotalOffset();									//获取滚动筐整个总偏移() 返回值亦可能为负数

	inline float GetOffsetRatio(){ if(!zZoningNum)return 0.0f; return zTextOffset / (float)GetTotalOffset(); } //获得滚动筐内偏移比率 值0.0f至1.0f
	inline unsigned int GetTotalLength(){ int ofs=GetTotalOffset(); return ofs<0?-ofs:ofs; }				   //获得滚动筐整个滚动总长度
	inline unsigned int GetVisualCount(){ if(zFFrames)return (unsigned int)zFFrames->zMax_num;else return 0; } //获得最大可见区域分块个数
	inline unsigned int GetZoningCount(){ return zZoningNum; }												   //获得总区域分块个数
	inline void GetZoningSize(short &w, short &h){ w =zFFrames->zParame.width; h =zFFrames->zParame.height; }  //获得区域分块的显示宽度与高度(返回宽度, 返回高度)
	inline short GetZoningWidth() { return zFFrames->zParame.width;  }	 //获得区域分块的显示宽度
	inline short GetZoningHeight(){ return zFFrames->zParame.height; }	 //获得区域分块的显示高度
	inline bool  IsOverturnArrange(){ return zFFrames->zParame.overturn_print; } //判断区域分块是否反向排列

	inline P<GNg::sIMAGE> &FocusImg(){ return zFocusImg; }				 //引用选中焦点底图

	virtual P<iUI_OBJECT> Clone();										 //复制出该UI组件的一个新副本
	virtual void OnRePos(sUI_LEVEL *parent);							 //当父层UI组大小变更了要求组件成员重置位置时的处理(父层级信息链)
	virtual void OnMove(short mx, short my);							 //当父层UI组平移拖动了要求组件成员偏移位置时的处理(X移动量, Y移动量)
	virtual void UpdateRun(unsigned int updatetime, sUI_LEVEL *parent);  //更新运行(帧间隔时间, 父层级信息链)
	virtual void GetDrawArea(GNg::sDRAW_AREA2 &out, cUI_GROUP *parent);  //获得UI组件的实际绘制区域(返回区域, 父层组 您可以不指定它) !需在 UpdateRun UpdatePlay 执行后再调用该方法才有正确值输出
};
typedef cSCROLL_LIST cSCROLL_RECTEX;	//增强滚动筐

/*****************************************************************************************************************************************************************/
enum eRECT_DRAW_TYPE {		  //矩型区域绘制类型
     eRECT_DRAW_NULL,			//不绘制
     eRECT_DRAW_FILL,			//填充
     eRECT_DRAW_WIRE			//线筐
};
class GN_API cSELECT_LIST_ICON :public cSCROLL_LIST  //★选择图标列表 式样较丰富多样灵活
{  														   //最多可以显示128个不同的图标 如果在多选模式下最多只能显示32个图标
    friend void ::GT_SelectListIconDrawCall(int,int,int,int,unsigned int ,unsigned int ,unsigned int ,bool,void*);
   #ifdef _MSC_VER
    cSCROLL_LIST::InitStyleToData;
   #endif
   #ifndef _APPLE_
	cSCROLL_LIST::SetZoningDrawCall;			   //使基类该函数不能用
   #endif
   protected:
    void   zReSelectState();								//重置选择状态
    bool   zSelectProc(unsigned int zid, bool has_focus);   //选择项处理(选项ID, 选项是否当前选择焦点)返回该项是否要绘制高亮焦点
	void   zSpecClone();									//分离克隆副本不能共享的属性
	uint8* zDataSave(uint8 *wpos, cSHARE_DATA *share_objs); //保存该对象的2进制数据(当前写入位置, 收集共享对象集)返回最后的写完位置
    uint8* zDataLoad(uint8 *rpos, cSHARE_DATA *share_objs);	//读取该对象的2进制数据(当前读取位置, 引用共享对象集)返回最后的读完位置

	P<GNg::sIMAGE>				zIcons;			   //图标图象
	uint8                       zMsLastState;      //在多选模式时 上次点选的状态 0:未选, 1:加选, 2:减选
    bool                        zMultSelect;       //标记是否可多选
	bool                        zIsFocusFrameShow; //当前焦点是否需要高亮显示
   public:
	unsigned int                zMsLastID;         //在多选模式时 上次点到的图标ID
    unsigned int                zSelectedLast;     //上次的选择状态
	unsigned int                zFocusLastID;      //上次焦点的图标ID
   public:
	unsigned int                aSelected;         //当前已选上了的项 也可以在代码里显式主动设置它来选择 在多选模式下 该值一般用位划检测选中组合 aSelected & (1|2|4|8...) 如在单选模式下 该值为选中的是第几个选项 值由1开始 返回0表示没有选到东西
    eRECT_DRAW_TYPE             aSelectFrameStyle; //选中选筐式样
    unsigned short              aLineWidth;        //当选筐色样用线筐显示时的宽度

	inline cSELECT_LIST_ICON():aSelectFrameStyle(eRECT_DRAW_FILL),aLineWidth(2){ aType = GNu::eUI_SELECT_LIST_ICON; }

	//初始化设置显示式样 !!必需
    bool InitStyleToData(
				 P<GNg::sIMAGE>		icons,					 //图标图象 !如果有先调用 SetEffect 图标是提前放入参数 effect 里的 该值可以忽略填0  要是还填了该值那该值会覆盖原来图标
		         bool  mult_select  = false,                 //是否可多选
		         short x            = NearLx(0),             //图标列表范围左上位置X
				 short y            = NearTy(0),             //图标列表范围左上位置Y
				 short w            = 0,                     //图标列表范围宽度 默认0为:和图标宽度一致
				 short h            = GNg::ViewCur->orthoH,  //图标列表范围高度
				 bool  default_am   = true,                  //图标列表默认动画效果 !在没调用SetEffect指定生成效果筐或第一次调用这个时该值才会生效
				 short dw           = 0,                     //单个图标宽度 默认0:跟列表范围宽度一致
				 short dh           = 0,                     //单个图标高度 默认0:跟列表范围宽度一致
		unsigned int   focus_color  = GT_DEF_FOCUS_COLOR,    //焦点颜色
		unsigned short focus_blend  = GNg::eBLEND_BRIGHT     //焦点混合模式
	);  //返回是否成功

	//更新运行图标列表(帧间隔时间, 实时总体色调) 返回图标列表被重设之后的运行经历时间 如果返回0表示图标列表已经播放消失结束了 返回1表示图标列表正在拖动中
	unsigned int UpdateRun(unsigned int updatetime, unsigned int color=0xffffffff);

	inline void OkHide(){ cSCROLL_LIST::OkHide(); zSelectedLast = aSelected = 0; } //隐藏图标列表 当有设置动画效果时 确定好后调用这个就可隐藏 且一并清除选择
	inline void RePos() { cSCROLL_LIST::RePos();  zSelectedLast = aSelected = 0; } //滚动复位 一并清除选择
	inline bool IsMultSelect(){ return zMultSelect; }							   //判断是否可多选
	void		SetMultSelect(bool yes);										   //设置是否可多选

	virtual P<iUI_OBJECT> Clone();	//复制出该UI组件的一个新副本

	//更新运行滚动筐(帧间隔时间, 父层级信息链)
	virtual void UpdateRun(unsigned int updatetime, sUI_LEVEL *parent);

	//把内部数据储存为一整段数据时的处理(收集共享对象集, 返回整段数据 首个字节为版本号之后4个字节为aType 当该值为0时仅返回数据段需要大小)返回数据段大小字节 !用户自定义UI组件的版本号应由1开始
	virtual unsigned int DataSave(cSHARE_DATA *share_objs, uint8 *data_out=0);

	//从文件数据段里读入数据时的处理(引用共享对象集, 数据段地址 首个字节为版本号之后4个字节为aType)返回已读取字节数 失败返回0 !用户自定义UI组件的版本号应由1开始
	virtual unsigned int DataLoad(cSHARE_DATA *share_objs, const uint8 *data);
};
class GN_API cSELECT_LIST_TEXT :public cSELECT_LIST_ICON  //★选择文本列表 式样较少 使用简单
{															//如果在多选模式下最多只能显示32个选项
    friend void ::GT_SelectListTextDrawCall(int,int,int,int,unsigned int ,unsigned int ,unsigned int ,bool,void*);
   #ifdef _MSC_VER
    cSELECT_LIST_ICON::InitStyleToData;			   //使基类的该函数不能用
   #endif
   protected:
	void   zSpecClone();									//分离克隆副本不能共享的属性
	uint8* zDataSave(uint8 *wpos, cSHARE_DATA *share_objs); //保存该对象的2进制数据(当前写入位置, 收集共享对象集)返回最后的写完位置
    uint8* zDataLoad(uint8 *rpos, cSHARE_DATA *share_objs); //读取该对象的2进制数据(当前读取位置, 引用共享对象集)返回最后的读完位置
	virtual void zOnReArranged(unsigned short sp, unsigned short sp_last, bool ret_am); //当滚动筐被重排列后的处理(当前排列起始区域号, 上次的排列起始区域号, 排列是否一起重设了所有区域的弹出效果)

    GNg::cTEXT_ART             *zTextOut;		   //文本输出器
    P< P<char> >                zLabels;		   //各个标签字符串
   public:
    unsigned int                aTextColor;		   //文本颜色
	unsigned int                aBackColor;		   //底框颜色
	eRECT_DRAW_TYPE             aBackStyle;		   //底框式样 当值为 eRECT_DRAW_NULL 时但有指定 aBackColor 时将以下划线形式显示 非下划线模式 aLineWidth 来控制各标签底框边距
	bool                        aAffectColor;	   //效果渐变色是否影响内容颜色

	cSELECT_LIST_TEXT():zTextOut(0),aBackColor(0),aBackStyle(eRECT_DRAW_NULL),aAffectColor(false){ aType = GNu::eUI_SELECT_LIST_TEXT; }

    //初始化设置显示式样 !!必需
    bool InitStyleToData(
		 P< P<char> >   labels,								 //可供选择的标签组
         const GNg::cTEXT_ART *text_style = 0,				 //显示标签的文本输出 该类内部不托管这个 您得确保在使用周期内这个还未被释放
	     unsigned int   text_color  = 0,                     //标签文字颜色
				   bool mult_select = false,                 //是否可多选
				  short x           = NearLx(0),             //文本列表范围左上坐标X
				  short y           = NearTy(0),             //文本列表范围左上坐标Y
				  short w           = 0,                     //文本列表范围宽度  默认0可以显示全部标签文字的宽度
				  short h           = 0,                     //文本列表范围高度  默认0可以显示全部标签文字的高度
				  bool  default_am  = true,                  //文本列表默认动画效果 !在没调用SetEffect指定生成效果筐或第一次调用该函数时该值才会生效
				  short dw          = 0,                     //单个标签宽度  默认0:自动适应文字大小
				  short dh          = 0,                     //单个标签高度  默认0:自动适应文字大小
		 unsigned int   focus_color = GT_DEF_FOCUS_COLOR,    //焦点颜色
		 unsigned short focus_blend = GNg::eBLEND_BRIGHT     //焦点混合模式
	);   //返回是否成功

	//更新运行文本列表(帧间隔时间, 实时总体色调) 与继承类的一致
	//unsigned int  UpdateRun(unsigned int updatetime, unsigned int color=0xffffffff);

	//设置选择文本列表内容
	void  SetLables(P< P<char> > labels);
	//获得选择文本列表内容
	inline P< P<char> >/*const*/ GetLables(){ return zLabels; }
	//获得文本输出式样 
	inline GNg::cTEXT_ART *const GetTextStyle(){ return zTextOut; }

	virtual P<iUI_OBJECT> Clone();	//复制出该UI组件的一个新副本

	//把内部数据储存为一整段数据时的处理(收集共享对象集, 返回整段数据 首个字节为版本号之后4个字节为aType 当该值为0时仅返回数据段需要大小)返回数据段大小字节 !用户自定义UI组件的版本号应由1开始
	virtual unsigned int DataSave(cSHARE_DATA *share_objs, uint8 *data_out=0);

	//从文件数据段里读入数据时的处理(引用共享对象集, 数据段地址 首个字节为版本号之后4个字节为aType)返回已读取字节数 失败返回0 !用户自定义UI组件的版本号应由1开始
	virtual unsigned int DataLoad(cSHARE_DATA *share_objs, const uint8 *data);
};
/**********************************************************************************************************************************************************************************************/

class GN_API cSELECT_CALL	    //选项回调处理
{  protected:
	uint8* zDataSave(uint8 *wpos);				   //保存该对象的2进制数据(当前写入位置)返回最后的写完位置
    uint8* zDataLoad(uint8 *rpos);				   //读取该对象的2进制数据(当前读取位置)返回最后的读完位置

	P<float>				    zHots;			   //对应每个选项的关注度
   public:
	short						aSelect;		   //记录当前选择ID

	inline cSELECT_CALL():aSelect(0){}

	//初始化(选项数)
	void Init(short count);

	//更新运行(每个选择项的回调处理(选项绘制坐标X, 坐标Y, ID, 关注度, 传回您的用户参数), 用户参数, 当前是否有点击事件, 整排选项的绘制坐标X, Y, 单个选项大小, 每个选项跨距, 是否竖排列,  动画速度比率)
	void UpdateRun(void (*proc)(short, short, short, float, void*), void *us_arg, bool cp, short dx, short dy, short size, short dist, bool erect=false, float time_raito=GN::TimeRatioL);

	short Count(){ return zHots.Num(); }	//返回选项数
};
class GN_API cPAGE_BAR :public cSELECT_CALL  //分页栏
{
   #ifndef _APPLE_
    cSELECT_CALL::Init;
	cSELECT_CALL::UpdateRun;
   #endif
   public:
	P< P<char> >			    zNames;			   //各个分页栏名
	GNg::cTEXT_ART			   *zTextOut;		   //文本式样
	short						zL, zH;			   //单个页的长和厚

    //初始化(各个分页栏名, 文字式样 当式样为竖向输出时 那么分页也竖排列, 每分页长度 默认0为自动适配, 每分页厚度 默认0为自动适配)
	void Init(P< P<char> > names, const GNg::cTEXT_ART *text_style = 0, short l=0, short h=0);

	//设置各个分页栏名(各个分页栏名)
	inline void SetNames(P< P<char> > names){ Init(names, zTextOut); }

	//获得各个分页栏名
	inline P< P<char> > &GetNames(){ return zNames; }
	
	//获得文本式样
	inline GNg::cTEXT_ART *const GetTextStyle(){ return zTextOut; }			    

	//获得单个页宽度和高度(返回宽度, 返回高度)
	void GetOnecWH(short &w, short &h);

	//更新运行(当前是否有点击事件, 整排分页栏的绘制坐标X, Y, 每个页跨距 0为自动匹配, 文本线条粗, 文本颜色, 底色, 选中焦点颜色 默认0镂空, 边线颜色 默认0不显示边线, 动画速度比率)
	void  UpdateRun(bool cp, short dx, short dy, short dist, int tw, unsigned int  text_color, unsigned int  back_color, unsigned int  focus_color=0, unsigned int  line_color=0, float time_raito=GN::TimeRatioL);
   protected:
	//更新运行(当前是否有点击事件, 整排分页栏的绘制坐标X, Y, 每个页跨距 0为自动匹配, 文本线条粗, 文本颜色, 底色, 选中焦点颜色 默认0镂空, 边线颜色 默认0不显示边线, 动画速度比率) 这个为不作排版适配的版本
	void zUpdateRun(bool cp, short dx, short dy, short dist, int tw, unsigned int  text_color, unsigned int  back_color, unsigned int  focus_color=0, unsigned int  line_color=0, float time_raito=GN::TimeRatioL);
};
class GN_API cSELECT_BUTTON :public cSELECT_CALL  //选择按钮联 !这个暂不支持多选
{
   #ifndef _APPLE_
    cSELECT_CALL::Init;
	cSELECT_CALL::UpdateRun;
   #endif
   public:
	P<GNg::sIMAGE>				zImg;			   //按钮图序

    //初始化(按钮图标组)
	void Init(P<GNg::sIMAGE> button_style);

	//获得按钮图序
	inline P<GNg::sIMAGE> &GetButtonImg(){ return zImg; }

	//获得单个按钮宽度和高度(返回宽度, 返回高度)
	void GetOnecWH(short &w, short &h);

	//更新运行(当前是否有点击事件, 整排按钮的绘制坐标X, 坐标Y, 每个按钮的跨距 0为自动匹配, 颜色, 选中颜色, 是否竖排列, 动画速度比率) !颜色和选中颜色一致时动画效果也不显现
	void UpdateRun(bool cp, short dx, short dy, short dist, unsigned int color, unsigned int focus_color, bool erect=false, float time_raito=GN::TimeRatioL);
};
};
/*---------------------------------------------------------------------------------------------------------------------------------------------------------------*/
struct GT_sUI_STYLE_BASE		//UI基本式样
{
	float                       aAmSheep;		   //动画速度比率
	unsigned int                aFocusColor;       //选中颜色
	short						aStride;		   //每选项跨距
	
	GT_sUI_STYLE_BASE():aAmSheep(1.0f),aFocusColor(GT_DEF_FOCUS_COLOR),aStride(48){}

	uint8* zDataSave(uint8 *wpos);   //保存该对象的2进制数据(当前写入位置)返回最后的写完位置
    uint8* zDataLoad(uint8 *rpos);   //读取该对象的2进制数据(当前读取位置)返回最后的读完位置
};
namespace GNu {
class GN_API cUI_PAGE_BAR : public cUI_NEAR_OBJECT, public GT_sUI_STYLE_BASE, public cPAGE_BAR  //★分页栏
{  public:
	unsigned int                aBackColor;		   //底色
	unsigned int                aLineColor;		   //边线颜色
	uint8						aFontLineW;		   //文本线条粗
	bool						aAutoLayerObjShow; //影响与它同层群组可见与隐藏 !即自动管理与它在同群组层里且在它之后的多个UI组显示与隐藏 在它之后的UI组显示顺序与各分页卡一一对应 !中间夹杂的其它类型UI组件将忽略

	cUI_PAGE_BAR():aBackColor(0x88888888),aLineColor(0),aFontLineW(1),aAutoLayerObjShow(false){ aType = eUI_PAGE_BAR; }

	void GetSize(short &_w, short &_h); //获得整个分页栏的宽度和高度(返回宽度, 返回高度)

	virtual P<iUI_OBJECT> Clone();   	//复制出该UI组件的一个新副本
	
	virtual void UpdateRun(unsigned int updatetime, sUI_LEVEL *parent);  //更新运行(帧间隔时间, 父层级信息链)
	virtual void GetDrawArea(GNg::sDRAW_AREA2 &out, cUI_GROUP *parent);  //获得UI组  的实际绘制区域(返回区域, 父层组) !需在 UpdateRun UpdatePlay 执行后再调用该方法才有正确值输出

	//把内部数据储存为一整段数据时的处理(收集共享对象集, 返回整段数据 首个字节为版本号之后4个字节为aType 当该值为0时仅返回数据段需要大小)返回数据段大小字节 !用户自定义UI组件的版本号应由1开始
	virtual unsigned int DataSave(cSHARE_DATA *share_objs, uint8 *data_out=0);

	//从文件数据段里读入数据时的处理(引用共享对象集, 数据段地址 首个字节为版本号之后4个字节为aType)返回已读取字节数 失败返回0 !用户自定义UI组件的版本号应由1开始
	virtual unsigned int DataLoad(cSHARE_DATA *share_objs, const uint8 *data);
};
class GN_API cUI_SELECT_BUTTON : public cUI_NEAR_OBJECT, public GT_sUI_STYLE_BASE, public cSELECT_BUTTON  //★选择按钮联
{  public:
	bool                        aIsErectArrange;   //是否竖方向排列

	cUI_SELECT_BUTTON():aIsErectArrange(false){ aType = eUI_SELECT_BUTTON; }
	
	void GetSize(short &_w, short &_h); //获得整个按钮联的宽度和高度(返回宽度, 返回高度)

	virtual P<iUI_OBJECT> Clone();   	//复制出该UI组件的一个新副本
	
	virtual void UpdateRun(unsigned int updatetime, sUI_LEVEL *parent);  //更新运行(帧间隔时间, 父层级信息链) !当颜色和选中颜色一致时动画效果也不显现
	virtual void GetDrawArea(GNg::sDRAW_AREA2 &out, cUI_GROUP *parent);  //获得UI组  的实际绘制区域(返回区域, 父层组) !需在 UpdateRun UpdatePlay 执行后再调用该方法才有正确值输出

	//把内部数据储存为一整段数据时的处理(收集共享对象集, 返回整段数据 首个字节为版本号之后4个字节为aType 当该值为0时仅返回数据段需要大小)返回数据段大小字节 !用户自定义UI组件的版本号应由1开始
	virtual unsigned int DataSave(cSHARE_DATA *share_objs, uint8 *data_out=0);

	//从文件数据段里读入数据时的处理(引用共享对象集, 数据段地址 首个字节为版本号之后4个字节为aType)返回已读取字节数 失败返回0 !用户自定义UI组件的版本号应由1开始
	virtual unsigned int DataLoad(cSHARE_DATA *share_objs, const uint8 *data);
};
/*****************************************************************************************************************************************************************/

class GN_API cSELECT_RECT		//选择筐类  !如在DirectDraw模式下 想不怕把后面的内容挡住了 可设置显示颜色为0x00ffffff 或把底图处理成网点形式以摸以半透明效果
{
	int							zHideTime;		   //距离隐藏的时间
	int							zHideSleep;		   //选筐消失延迟时间
	GNg::sRECT					zP2;			   //当前鼠标拖动的两粒点
	GNg::sRECT					zRect;			   //选筐
	P<GNg::sIMAGE>			    zBg;			   //底图 该图支持可用特殊的9宫格重复铺盖图
   public:
	cSELECT_RECT();

    //初始化(底图, 选筐消失延迟时间 单位毫秒);
    void Init(P<GNg::sIMAGE> bg_img=P_(), int hide_sleep=1);

	//更新运行选筐(帧间隔时间, 底颜色, 底混合模式, 边筐颜色, 边筐混合模式, 边筐线宽) 返回内部选筐矩形 返回:0表示未出现选筐
	GNg::sRECT *UpdateRun(unsigned int updatetime, unsigned int  color=0xffffffff, unsigned short blendmode=GNg::eBLEND_NORMAL, unsigned int  line_color=0xffffffff, unsigned short line_blendmode=1, int line_w=0);

	void Hide();                                   //立即使筐隐藏

	//获得选筐范围(返回范围矩形)
	inline void GetRect(GNg::sRECT &rect){ rect = zRect; }
	//获得选筐范围各分量(左上位置X输出, 左上位置Y输出, 宽度输出, 高度输出)
	inline void GetRectXYWH(int &x,int &y,int &w, int &h){ x = zRect.left, y = zRect.top, w = zRect.right-zRect.left, h = zRect.bottom-zRect.top; }
};
/*****************************************************************************************************************************************************************/
class GN_API cSLIDER   :public cUI_IMAGE	   // ★滑动条
{  protected:
	uint8                       zState;			   //当前状态
	short						zOfs;			   //图标局部偏移
	float						zFocusRatio;	   //焦点颜色占比
   public:
    float                       aAmSheep;		   //动画速度比率
	unsigned int                aFocusColor;	   //焦点颜色
	unsigned int                aPressColor;	   //按紧颜色
//	P<GNg::sIMAGE>				img;			   //滑竿图标		!该属性在基类已定义
	unsigned int                aIconColor;		   //滑竿图颜色
	unsigned short				aIconBlend;		   //滑竿图混合模式
	short						aIconW;			   //滑竿图宽或边筐宽
	short						aIconH;			   //滑竿图高

	cSLIDER():zState(0),zOfs(0),zFocusRatio(0.0f),aAmSheep(1.0f),aFocusColor(GT_DEF_FOCUS_COLOR),aPressColor(GT_DEF_FOCUS_COLOR|0x00006000),aIconColor(0x88888888),aIconBlend(1),aIconW(0),aIconH(0){ aType = eUI_SLIDER; bmode = 1; }

	inline uint8 GetState(){ return zState; }      //返回当前状态 0:无操作 1:鼠标落在滑动条上 2:按紧中
	
	inline float GetVolume(){ return (float)zOfs/(float)(w>h?w:h);  }    //获得当前滑满量 0.0f至1.0f
	inline void  SetVolume(float vol){ zOfs = (float)(w>h?w:h)*vol; }    //设置当前滑满量(滑满量 0.0f至1.0f)

	virtual P<iUI_OBJECT> Clone();										 //复制出该UI组件的一个新副本
    virtual void UpdateRun(unsigned int updatetime, sUI_LEVEL *parent);  //更新运行(帧间隔时间, 父层级信息链)
	virtual void GetDrawArea(GNg::sDRAW_AREA2 &out, cUI_GROUP *parent);  //获得UI组  的实际绘制区域(返回区域, 父层组) !需在 UpdateRun UpdatePlay 执行后再调用该方法才有正确值输出

	//把内部数据储存为一整段数据时的处理(收集共享对象集, 返回整段数据 首个字节为版本号之后4个字节为aType 当该值为0时仅返回数据段需要大小)返回数据段大小字节
	virtual unsigned int DataSave(cSHARE_DATA *share_objs, uint8 *data_out=0);

	//从文件数据段里读入数据时的处理(引用共享对象集, 数据段地址 首个字节为版本号之后4个字节为aType)返回已读取字节数 失败返回0 
	virtual unsigned int DataLoad(cSHARE_DATA *share_objs, const uint8 *data);
};
class GN_API cPALETTE  :public cUI_NEAR_OBJECT // ★颜色版
{  protected:
	GNg::sCOLOR					zColor;            //记录当前选择颜色
	GNg::sCOLOR					zColorBase;        //当前色谱的基色
    GNg::sCOLOR					zColorArrow;       //箭头游标颜色

	uint8						zSetTpye;          //当前鼠标操作正在设置的值类型
	bool						zIsMouseFocus;     //当前是否有鼠标箭头点重叠
	float						zAlphaBgOffset;    //透明背景条当前选取遍移
	float						zAlphaBgUV;        //透明背景移动UV的记量
	float						zBarOffset;        //色调条当前选取遍移
	float						zColorOffsetX;     //色谱当前选取遍移X
	float						zColorOffsetY;     //色谱当前选取遍移Y
	P<GNg::sIMAGE>				zColorImg;         //色谱图
	P<GNg::sCOLOR>				zColorClone;       //当前要复制的颜色副本
	float						zColorCloneMX;     //复制出颜色时飞过去的当前坐标中心X
	float						zColorCloneMY;     //复制出颜色时飞过去的当前坐标中心Y
	float						zColorCloneMW;     //复制出颜色时飞过去的当前宽度
	float						zColorCloneMH;     //复制出颜色时飞过去的当前高度
	float						zOffsetSheep;      //动画偏移效果速率
	unsigned int				zLastRunTime;      //最后一次调用 UpdateRun 后程序的运行时间

	static P<GNg::sIMAGE>		zColorBarImg;      //色调条图
	static P<GNg::sIMAGE>	    zAlphaBgImg;       //透明区底图
	static unsigned short		zCreateTotalNum;   //总颜色版创建数

	//计算内部色谱颜色数据(基色:通常是比较鲜艳没有杂色的一个颜色)
	void zComputeColorImgData(GNg::sCOLOR base);
	//计算色调条颜色数据(返回颜色数据, 色调条宽度, 色调条高度, 从色调条的第几个象素开始计算, 第几个象素结束)
    void zComputeColorBarData(uint8 *p_data, int bar_w,int bar_h, int start,int end);
	//按设定好的各个偏移状态来计算更新颜色
	void zUpdateColorFromOffsetState();
   public:
	#define 					aDlgw w            //显示宽度
	#define						aDlgh h            //显示高度
	#define						aDlgx x            //显示左上位置X
	#define						aDlgy y            //显示左上位置Y
	bool						aAlphaSet;         //是否可设置不透明度
	bool						aArrowShadow;      //指向色调箭头是否有显示阴影
	bool						aAlphaBgShow;      //是否显示透明移动背景 以凸显出透明颜色的效果
	bool						aDisableControl;   //是否禁用操作控制 该值为true 就只能显示不能对它操作
	uint8						aColorTablePosType;//颜色表布局类型 0:颜色表靠内 1:颜色表靠外 默认值为0
    P<GNg::sCOLOR>				aTable;            //现成的颜色表 有指定这个属性才会附加显示颜色表 要是把它的内容删掉则不会显示

	static GN::sLIST<GNg::sCOLOR> aColorPasteTable; //颜色粘贴版色表 当有指定这个属性和现成颜色表才会附加显示这个色表 要是把它的内容删掉则不会显示

    cPALETTE();
   ~cPALETTE();

	//初始化颜色版( 左上坐标X, 左上坐标Y, 宽度, 高度,		初始颜色,                箭头颜色,                   箭头是否有阴影效果,    是否可设置不透明度,  是否显示透明移动背景)
    void InitToData(short _x, short _y, short _w, short _h, GNg::sCOLOR cr=0xffff0000, GNg::sCOLOR arrow=0xffffffff, bool arrow_shadow=false, bool alpha_set=true, bool alpha_bg_show=false);

	//设置动画偏移效果(偏移速率:0.05f至1.0f) 在复制颜色时将产生效果 值 >= 1.0f 时将禁止动画 初始值为1.0f
	inline void SetOffsetEF(float sheep = 0.22f){ zOffsetSheep = sheep; }

	//追加初始化标准默认颜色表
	void InitDefaultTable();

	//更新运行显示颜色板(帧间隔时间, 实时总体色调) 返回颜色选择是否改变了 !这个要在每帧都行的地方挂起
	bool UpdateRun(unsigned int updatetime, unsigned int color=0xffffffff);

	//手动设置颜色值并一起计算各个选择偏移点的新位置(颜色)
	void SetColor(GNg::sCOLOR cr);

	//设置显示位置大小(左上坐标X, 左上坐标Y, 宽度, 高度)
	inline void SetPos(short _x, short _y, short _w, short _h){ x = _x, y = _y, w = _w, h = _h; }

	//获得显示位置大小(左上坐标X输出, 左上坐标Y输出, 宽度输出, 高度输出)
	inline void GetPos(short *_x, short *_y, short *_w, short *_h){ *_x = x, *_y = y, *_w = w, *_h = h; }

	inline GNg::sCOLOR GetColor(){ return zColor; }							  //获取当前选择颜色
	inline short GetBarHeight(bool alpha_set=true){ return alpha_set?72:36; } //返回色调条和箭头额外内容的高度(是否含有透明设置条)
	inline bool  IsMouseFocus(){ return zIsMouseFocus || zSetTpye; }          //返回鼠标是否瞄到该对象

	virtual P<iUI_OBJECT> Clone();											  //复制出该UI组件的一个新副本
    virtual void UpdateRun(unsigned int updatetime, sUI_LEVEL *parent);		  //更新运行(帧间隔时间, 父层级信息链)

	//把内部数据储存为一整段数据时的处理(收集共享对象集, 返回整段数据 首个字节为版本号之后4个字节为aType 当该值为0时仅返回数据段需要大小)返回数据段大小字节
	virtual unsigned int DataSave(cSHARE_DATA *share_objs, uint8 *data_out=0);

	//从文件数据段里读入数据时的处理(引用共享对象集, 数据段地址 首个字节为版本号之后4个字节为aType)返回已读取字节数 失败返回0 
	virtual unsigned int DataLoad(cSHARE_DATA *share_objs, const uint8 *data);
};
struct GN_API sCTRL_AXIS  //2D控制轴 !该类在DirectDraw模式时没有旋转轴
{
	static  P<GNg::sIMAGE>      zCtrlAxisImg;      //控制轴式样图
	static  unsigned short      zCreateTotalNum;   //该对象总创建数

	GNg::cFRAME				   *zSelectAxis;       //当前选到的轴
    P<GNg::cFRAME>			    zGroup;            //控制轴组
	P<GNg::cFRAME>			    zAxis;             //保存各个子控制轴 0:移动 1:旋转 2:缩放

	GNg::sVEC2				    zCp;               //上次点选到轴的定点
	GNg::sVEC2				    zPos;              //记录上次设置的初始位置
	float                       zRot;              //记录上次设置的初始旋转角度

	uint8                       zAxisType;         //上次点选到控制轴的类型
	bool                        zHideMove;         //是否隐藏移动控制轴
	bool                        zHideRot;          //是否隐藏旋转控制轴
	bool                        zHideScl;          //是否隐藏缩放控制轴
	bool                        zIsLocalMode;      //当前是否始终按局部坐标轴方向来变换
	bool                        zScaleIsFront;     //点击缩放轴是否由正面开始
	float                       zCurTsTotalSize;   //当前总缩放比
	GNg::sMAT33				    zTs;			   //记录累计变换操作
	GNg::sMAT33				    zTsBak;			   //记录累计变换操作的副本

    //用户调用接口-----------------------------------------------------------------------------------------------------------------------------------------
    float                       aUnitCtrlMov;      //单位移动量   即控制每步移动的量
	float                       aUnitCtrlRot;      //单位旋转角度 即控制每步旋转的角度
    float                       aUnitCtrlScl;      //单位缩放量   即控制每步缩放的量
	bool                        aDisableControl;   //是否禁用操作控制 该值为true 就只能显示不能对它操作
	bool                        aIsVisual;         //当前是否可视可操作
	short                       aShowSize;         //在屏幕上显示的初始大小

	sCTRL_AXIS();
   ~sCTRL_AXIS();

    //初始化(            移动轴颜色,                  旋转轴颜色,                缩放轴颜色)  !在DirectDraw模式下不支持颜色设置 控制盘只能以原图色显示
    void InitToData(GNg::sCOLOR move=0xffff0000, GNg::sCOLOR rot=0xff00ff00, GNg::sCOLOR scl=0xff0000ff);

	//设置初始位置(位置X, 位置Y, 旋转角度, 是否使用局部操纵模式)
	void SetPos(int x, int y, float rot = 0.0f, bool local_mode = 0);

	//挂起2D显示(返回本次操作的移动或旋转或缩放X量, Y量) 返回现在正在被设置的操作标记 1:X方向移动 2:Y方向移动 3:XY方向移动 4:旋转 5:X缩放 6:Y缩放 7:XY缩放 如果返回0xff表示刚点中 中心点控制盘转向归位 !当返回的是旋转操作时就通过首个参数返回相对旋转角度
	uint8 Show(float *rx=0, float *ry=0);

	inline void CtrlHide (bool move, bool rot, bool scl){ zHideMove=move, zHideRot=rot, zHideScl=scl; }    //设置控制盘某些部分隐藏不能被操作(平移控制隐藏, 旋转控制隐藏, 缩放控制隐藏)
	inline void CtrlNoHideAll(){ zHideMove = false; zHideRot = false; zHideScl = false; }				   //设置控制盘的所有部件可视
	inline bool IsMouseFocus(){ return zSelectAxis ? 1 : 0; }											   //当前是否有鼠标瞄到任意控制轴
	inline void GetCoord(float &cx, float &cy){ cx=zGroup->aLevel_world.m31,cy=zGroup->aLevel_world.m32; } //获得整个控制轴的控制点中心位置(返回位置X, 返回位置Y)

	void GetCoordForSXY(float &scx, float &scy);					//获取缩放XY的控制点中心位置(返回位置X, 返回位置Y)
	void GetLastTranslate(GNg::sMAT33 *m_out, bool clear=true);		//获得自上次调用SetPos 或 GetLastTranslate 之后经过操作累计的变换矩阵(矩阵输出, 获得后是否清除之前的累计记录)
	GNg::sMAT33 *GetWorldMatrix();									//获得当前整个控制轴的绝对世界变换矩阵()返回世界变换矩阵
};
struct GN_API sBUBBLE			// ★对话汽泡 像漫画里的对话筐那样
{
	P<GNg::sIMAGE>				zArrow;            //底箭头图
	P<GNg::sIMAGE>				zArrowFrame;	   //边筐箭头图
	P<GNg::sIMAGE>				zImage9;           //底9宫格图
    P<GNg::sIMAGE>				zImage9Frame;      //边筐9宫格图

    int							aFw;               //边筐图边粗度
    unsigned int 				aColor;            //底色
    unsigned int 				aColorFrame;       //边筐色
	unsigned short				aBlend;            //底混合模式
    unsigned short				aBlendFrame;       //边筐混合模式

	inline sBUBBLE():aFw(0){}

   //初始化(底9宫格图, 底箭头图 该图尖部分应该向右的, 底色, 底混合模式)
	void InitToData(P<GNg::sIMAGE> image9, P<GNg::sIMAGE> arrow, unsigned int  color=0xffffffff, unsigned short blend=GNg::eBLEND_NORMAL);

   //设置边筐色样(边筐9宫格图, 边筐箭头图, 边筐色, 边筐混合模式, 边筐图边粗度) !要特别注意的是当底色有透明或使用别的混合模式时 边筐将不显示
   void SetFrameStyle(P<GNg::sIMAGE> image9_frame, P<GNg::sIMAGE> arrow_frame, unsigned int  color_frame=0xff666666, unsigned short blend_frame=GNg::eBLEND_NORMAL, int fw=2);

   //展现对话汽泡(箭头指向坐标X, Y, 显示左上坐标X, Y, 显示宽度, 显示高度) !这个大小是不包括箭头的
   void Show(int ax, int ay, int x, int y, int w, int h);

   //展现对话汽泡(箭头方向, 显示左上坐标X, Y, 显示宽度, 显示高度) !这个大小是不包括箭头的
   void Show(GNg::sVEC2 &v, int x, int y, int w, int h);

   //加载默认对话汽泡式样( 返回底9宫格图, 返回底箭头图,  返回边筐9宫格图,  返回边筐箭头图) 返回是否都加载成功
   static bool LoadDefStyle(P<GNg::sIMAGE> &image9, P<GNg::sIMAGE> &arrow, P<GNg::sIMAGE> &image9_frame, P<GNg::sIMAGE> &arrow_frame);
};
/***************************************************************************************************************************************/
class GN_API cBUTTON_T		    // 触屏虚拟按钮  !!该类不支持编辑器编辑
{
    uint8                       zTid;              //当前关联的触点ID
   public:
    uint8                       aTouchZ;           //当前计算出的按钮下压力 值意义与 GN::TouchZ 一致
    uint8                       aMappingKey;       //映射到键盘的键位 直接指定一个键位 如 GK_SPACE 值0时禁用映射功能
    short                       aRngL;             //按钮范围左
    short                       aRngT;             //按钮范围上
    short                       aRngR;             //按钮范围右
    short                       aRngB;             //按钮范围下
    unsigned short              aBlend;            //混合模式
    unsigned int                aColor;            //颜色
    P<GNg::sIMAGE>              aIcon;             //按钮图标
    
    cBUTTON_T():zTid(255),aTouchZ(0),aMappingKey(0),aColor(0xffffffff),aBlend(1){}
    
    //初始化(按钮放置范围左, 上, 右, 下)
    inline void Init(short left, short top, short right, short bottom){ aRngL = left; aRngT = top; aRngR = right; aRngB = bottom; }

    void UpdateRun(unsigned int updatetime); //更新运行(帧间隔时间)
};
class GN_API cBUTTON_TOUCH	: public cUI_IMAGE	// ★★★触屏虚拟按钮
{  
	uint8                       zTid;              //当前关联的触点ID
   public:
    uint8                       aTouchZ;           //当前计算出的按钮下压力 值意义与 GN::TouchZ 一致
    uint8                       aMappingKey;       //映射到键盘的键位 直接指定一个键位 如 GK_SPACE 值0时禁用映射功能
  //P<GNg::sIMAGE>				img;			   //按钮图标 !已在基类定义了

	cBUTTON_TOUCH():zTid(255),aTouchZ(0),aMappingKey(0){ bmode = 1; aType = eUI_BUTTON_TOUCH; }

	//初始化(按钮放置范围左, 上, 右, 下)
    inline void Init(short left, short top, short right, short bottom){ x = left; y = top; w = right-left; h = bottom-top; }

	virtual P<iUI_OBJECT> Clone();										 //复制出该UI组件的一个新副本
	virtual void UpdateRun(unsigned int updatetime, sUI_LEVEL *parent);  //更新运行(帧间隔时间, 父层级信息链)

	//把内部数据储存为一整段数据时的处理(收集共享对象集, 返回整段数据 首个字节为版本号之后4个字节为aType 当该值为0时仅返回数据段需要大小)返回数据段大小字节
	virtual unsigned int DataSave(cSHARE_DATA *share_objs, uint8 *data_out=0);

	//从文件数据段里读入数据时的处理(引用共享对象集, 数据段地址 首个字节为版本号之后4个字节为aType)返回已读取字节数 失败返回0 
	virtual unsigned int DataLoad(cSHARE_DATA *share_objs, const uint8 *data);
};
class GN_API cJOYSTICK  : public iUI_OBJECT		// ★★★触屏虚拟操控杆
{  protected:
    short                       zCpX;              //上次点下的定点位置X
    short                       zCpY;              //上次点下的定点位置Y
    short                       zRngL;             //放置管控范围左
    short                       zRngT;             //放置管控范围上
    short                       zRngR;             //放置管控范围右
    short                       zRngB;             //放置管控范围下
    GNg::sVEC2                  zVpUV;             //自动贴近视口边角的范围对角点UV
    float                       zVectorScl;        //视口方向变换到控杆底盘局部空间的缩放比率
    uint8                       zTid;              //当前关联的触点ID
    
    void zInit(float back_disk_r);                 //初始化内部属性(控杆底盘大小像素半径)
    void zReCptNearRange();                        //重计算靠位范围
   public:
    P<GNg::sIMAGE>              aBackDiskImg;      //控杆底盘式样图
    P<GNg::sIMAGE>              aArrowImg;         //控杆方向箭头图
    unsigned int                aBackDiskColor;    //控杆底盘颜色
    unsigned int                aArrowColor;       //控杆方向箭头颜色
    GNg::sVEC2                  aVector;           //当前计算出的控杆方向  !该值当按压摞动出底盘半径距离时为1.0f
    float                       aPressure;         //当前计算出的控杆下压力
    float                       aVectorMaxLen;     //控杆方向可拖拉长度 超出这个底盘会被拉动
    
    float                       aVectorMappingRef; //方向映射触发值 aVector分量绝对值大于该值时映射键会被设置为1
    float                       aPressMappingRef;  //压下映射触发值 aPressure大于该值时映射键会被设置为1
    uint8                       aVecMappingKeyType;//方向映射键类型 0:禁用 1:映射到AWDS键 2:映射到方向键
    uint8                       aPressMappingKey;  //压下映射键位   直接指定一个键位 如 GK_SPACE
    
    cJOYSTICK():zTid(255),aBackDiskColor(0xffffffff),aArrowColor(0xffffffff),aVector(0.0f,0.0f),aPressure(0.0f),aVectorMaxLen(100.0f),aVectorMappingRef(0.5f),aPressMappingRef(0.2f),aVecMappingKeyType(0),aPressMappingKey(0){ aType = eUI_JOYSTICK; }
    
    //初始化(自动贴近视口边角的范围对角点UV, 控杆底盘大小像素半径 默认自动指定半径大小) !用这个来初始化之后控杆底盘放置管控范围会跟随视口改变而自动设置 用起来较方便
    inline void Init(GNg::sVEC2 &vp_uv, float back_disk_r=0.0f){ zVpUV = vp_uv; zReCptNearRange(); zInit(back_disk_r); }

    //初始化(放置管控范围左, 上, 右, 下, 控杆底盘大小像素半径 默认自动指定半径大小)
    inline void Init(short left, short top, short right, short bottom, float back_disk_r=0.0f){ zRngL = left; zRngT = top; zRngR = right; zRngB = bottom; zVpUV.x = -1.0f; zInit(back_disk_r); }
    
    //放置管控范围设置(自动贴近视口边角的范围对角点UV)
    inline void SetRange(GNg::sVEC2 &vp_uv){ Init(vp_uv, -1.0f); }
    
    //放置管控范围设置(范围左, 上, 右, 下)
    inline void SetRange(short left, short top, short right, short bottom){ Init(left,top,right,bottom,-1.0f); }
    
	//设置控杆底盘大小像素半径(半径)
	inline void SetBackDiskR(float back_disk_r){ zVectorScl = 1.0f / back_disk_r; }

    //获取管控范围(范围左输出, 上输出, 右输出, 下输出)
    inline void GetRange(short &left, short &top, short &right, short &bottom){ left = zRngL; top = zRngT; right = zRngR; bottom = zRngB; }
    
    //获取管控范围()返回范围
    inline GNg::sRECT GetRange(){ return GNg::sRECT(zRngL,zRngT,zRngR,zRngB); }
    
    //获取管控范围(返回范围)
    inline void GetRange(GNg::sRECT &rc){ rc.left = zRngL; rc.top = zRngT; rc.right = zRngR; rc.bottom = zRngB; }
    
	//获取控杆底盘大小像素半径
	inline float GetBackDiskR(){ return 1.0f / zVectorScl; }

	//判断当前放置管控范围是否自动靠位
	inline bool AutoNearRange(){ return zVpUV.x > 0.0f; }

    void UpdateRun(unsigned int updatetime, unsigned int color=0xffffffff); //更新运行(帧间隔时间, 实时总体色调)

	virtual P<iUI_OBJECT> Clone();										 //复制出该UI组件的一个新副本
    virtual void UpdateRun(unsigned int updatetime, sUI_LEVEL *parent);  //更新运行(帧间隔时间, 父层级信息链)
	virtual void GetDrawArea(GNg::sDRAW_AREA2 &out, cUI_GROUP *parent);  //获得UI组  的实际绘制区域(返回区域, 父层组 您可以不指定它) !需在 UpdateRun UpdatePlay 执行后再调用该方法才有正确值输出

	//把内部数据储存为一整段数据时的处理(收集共享对象集, 返回整段数据 首个字节为版本号之后4个字节为aType 当该值为0时仅返回数据段需要大小)返回数据段大小字节
	virtual unsigned int DataSave(cSHARE_DATA *share_objs, uint8 *data_out=0);

	//从文件数据段里读入数据时的处理(引用共享对象集, 数据段地址 首个字节为版本号之后4个字节为aType)返回已读取字节数 失败返回0 
	virtual unsigned int DataLoad(cSHARE_DATA *share_objs, const uint8 *data);
};
};
#endif //_GN_UIBASE_H_
