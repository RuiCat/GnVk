#ifndef _GN_UIPOPDLG_H_
#define _GN_UIPOPDLG_H_

struct GT_sSP_CHILD				//修改值子项
{
	char                       *name;          //名称
	void                       *pointer;       //值指针
	float                       maxValue;      //最大值
	float                       minValue;      //最小值
    float                       addValue;      //每次增加值
    unsigned int                srcValue;      //保存原变量值
	char                       *enumName;      //列举名
	short                      *enumIds;       //在列举选择变量时就记录列举名分割符的位置数组 在文本变量时就记录文本缩短的显示文本
	unsigned char               enumIdnum;     //列举名分割符个数+2
	unsigned char               type;          //类型
	P<GNg::sIMAGE>              icon;          //图标

	inline  GT_sSP_CHILD():pointer(0),name(0),enumName(0),enumIds(0),enumIdnum(0){}
    inline ~GT_sSP_CHILD(){ /*this->Release();*/ }

	void Release(); //释放数据
};
struct GT_sVALUE_SET_TOOL		//变量设置工具集
{
	unsigned int                fc;            //当前修改范围焦点颜色
    unsigned int                ic;            //出错提示当前显示底颜色
	unsigned int                it;            //出错提示显示累计时间
	short                       is;            //出错提示所属起始子项
	short                       fx,fy,fw,fh;   //当前修改范围位置和大小
	bool                        ts_transpose;  //选择表是否转置了
	bool                        linear_ing;    //位置大小是否正在动画插值

	P<GNu::cPALETTE>            palette;       //颜色修改器
	P<GNu::cTEXT_INPUT>         text_in;       //文本输入修改器
	P<GNu::cSELECT_LIST_TEXT>	text_select;   //选择修改器

	P<GNu::cSELECT_LIST_TEXT>   fb_select;     //子文件浏览用到的选择列表
	P< P<char> >                dir_strs;      //存放文件路径常规目录
	P< P<char> >                ftf_strs;      //存放文件类型过滤字符串组
	P< P<char> >                fts_strs;      //存放文件排序方式字符串组
	P<GNu::cFILE_BROWSER>       file_browser;  //文件浏览路径修改器
	P<GNg::sAM_EVENT>			fb_ev_icon;    //文件浏览里的图标动画事件
    GNg::sAM_EVENT				fb_ev[3];      //文件浏览用的其它动画事件帧
	GNg::cFRAME					fb_am[3];      //文件浏览用的动画筐
	uint8						fb_open_did;   //子文件浏览接下来要打开的目录号 值:0xff表示无需设置
	bool                        fb_show;       //标记是否显示子文件浏览筐

	char                        info[64];      //记录出错提示

	//检测指定点是否在修改范围内(点坐标X, 坐标Y)返回是否
	inline bool HitTest(short px, short py){ return px >= fx && px <= fx+fw && py >= fy && py <= fy+fh; }

	inline GT_sVALUE_SET_TOOL():text_in(1),text_select(1),palette(1),/*file_browser(1),*/ts_transpose(true),fb_open_did(0xff){}
};
/******************************************************************************************************************************************************/
namespace GNu{

enum eDETAIL_SET_TYPE {         //详细设置类型
     eDETAIL_SET_NULL,            //没有详细设置
	 eDETAIL_SET_VALUE_INPUT,     //变量值输入
	 eDETAIL_SET_VALUE_SELECT,    //变量值选择
     eDETAIL_SET_TEXT_INPUT,      //文本详细输入
     eDETAIL_SET_COLOR_SELECT,    //颜色详细选择
	 eDETAIL_SET_PATH_SELECT,     //文件路径选择
	 eDETAIL_SET_PATH_SELECTF     //文件路径选择 全屏幕方式
};
enum eCHILD_CONTENT_TYPE {      //子选项内容类型
     eCHILD_CONTENT_ALL,		  //整个子选项
     eCHILD_CONTENT_ICON,         //子选项图标
	 eCHILD_CONTENT_NAME,         //子选项名
	 eCHILD_CONTENT_ARWL,         //子选项左箭头
	 eCHILD_CONTENT_VALUE,        //子选项变量
	 eCHILD_CONTENT_ARWR,         //子选项右箭头
     eCHILD_CONTENT_LABLE         //子选项注释标记
};
enum eSET_PATH_TYPE {           //文件路径变量修改方法类型
     eSET_PATH_INPUT,			  //手工输入
     eSET_PATH_SELECT,            //在对话筐内的文件浏览里选择
	 eSET_PATH_SELECTF            //在整个屏幕内的文件浏览里选择
};
struct GN_API sSIM_MENU			//★★简单选单
{  protected:
	GN::sARRAY_VEC<GT_sSP_CHILD> zChild;        //子项组
    char                        *zCaption;      //选单标题
	sSIM_MENU				    *zFather;       //父选单
	int8                         zType;         //确定等按钮表示形式
	P<GNg::sIMAGE>               zIcon;         //标题图标

	friend class cSIM_DIALOG;
	void zReDefault(bool re_sub);                         //为该选单或它的子选单内的所有变量还原为默认值(是否牵连子选单)
	void zReadData( GNf::sFILE *fp, int &max_read_num);	  //为该选单和它的子选单内的所有变量赋值(数据来自的文件对象, 本次最多可以读取的数据个数)
	void zWriteData(GNf::sFILE *fp);					  //写入该选单和它的子选单内的所有变量数据(目标文件对象)
	void zReadText( GNf::sFILE *fp, int &max_read_byte);  //为该选单和它的子选单内的所有文本赋值(数据来自的文件对象, 本次最多可以读取的字节数)
	void zWriteText(GNf::sFILE *fp);					  //写入该选单和它的子选单内的所有文本数据(目标文件对象)

	//计算目标选单的变量在文件变量数据保存的所在偏移量(偏移量统计输出, 目标选单) !调用前一般先把offset_out设为0 以方便统计
	bool zComputeMenuForDataOffset(int &offset_out, sSIM_MENU *menu);

   public://★用户调用接口 --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
    inline sSIM_MENU():zCaption(0),zFather(0){}
   ~sSIM_MENU();
	void Release();        //只释放它的内容
    void ReleaseAll();     //全部释放 这包含所有子选单所包含的空间
    void RemoveValue();    //只移除它的变量内容
    void RemoveValueAll(); //移除它和它的所有子选单变量内容 这包含连子选单对象一起移除变量内容

    //插入修改值(       要修改的值,               值名称,          最大值,				最小值,				每次增加量,    列举名:"Yes|No"用这格式可以填入变量筐里面, 该项的图标)
	void InitValueToData(int    *value,        const char *name, int   max_value, int   min_value=0,    int   add_value=1, const char *enum_name=0, P<GNg::sIMAGE> icon=P_());

    //插入修改颜色(    要修改的颜色,              值名称,          最大值,				最小值,				每次增加量,  该项的图标)  !!颜色最大值和最小值增加量都是表示不透明度的 值范围:0至255
	void InitValueToData(unsigned int  *value, const char *name, int   max_value, int   min_value=0,    int   add_value=1, P<GNg::sIMAGE> icon=P_());

	//插入修改浮点值(要修改的浮点值,              值名称,          最大值,				最小值,				每次增加量 值为负时表示每次增加量是当前值乘该值的量,  该项的图标)
	void InitValueToData(float  *value,        const char *name, float max_value, float min_value=0.0f, float add_value=1.0f, P<GNg::sIMAGE> icon=P_());

	//插入修改文本(要修改的文本:需先分配好空间,   对应名称,	    要修改的文本空间字节大小, 修改筐最大字符显示数,  该项的图标) !参数name 如果第一个字符为'*'这样就会按密码保护方式来显示文本 如果为'+','=','>' 而且 text_buf_size 为 GN_MAXFILEPATH 时就会当这是一个文件路径 将可提供用户选择文件的处理方式来修改它的值 你可预先指定它的值包含文件扩展名 那么文件浏览打开时就可以限制只显示该类型文件 列如:"*.png,tga,bmp" 多个用逗号隔开 若为:"*.*"则不再限制 '+','=','>'表示使用的浏览模式 '+'标准浏览模式 '='仅显示目录的浏览模式 '>'可新建文档的浏览模式 这模式最好配合限制显示文件类型描述"*.png,bmp,jpg"用 这样新建的文件均为首个列出的类型
	void InitValueToData(  char *text,		   const char *name, int text_buf_size, int text_max_show_num=15, P<GNg::sIMAGE> icon=P_());

    //您可以用修改整型值的方法来作列举项挑选 列如用"Yes|No"这样的列举名并设置add_value为1 这样第一项列举名"Yes"代表的值为min_value 之后max_value会自动按列举个数来改变适应 使变动不会超出列举名所列举的范围 使用时你需保证每个列举的代表值按跨距为1 这样的顺序定义 如: 0 1 2 3 4 5 6 ...等
	//您可以用修改整型值的方法来作列举项多选 列如用"Yes|No"这样的列举名并设置add_value为0 这样所填的 max_value 和 min_value 都会自动被忽略 这模式最多只支持32个列举 要很好的操作多选必须调用EnableDetailSetting 参数:detail_set=true开启详细设置 否则一般操作也会变为只选到一个 使用多选时你还需保证每个可插入的列举代表值按这样的规律顺序定义 列如: 1 2 4 8 16 32 64 128 ... 等

	//插入文本提示(		文本内容,		附加跟综显示的可变值,  该项的图标) !如果加了跟踪值 那么首个参数里要加1个'%d'表明显示格式 第2个参数不能为局部临时变量 !另外在文本内容内没必要用'\n'换行 如您想在下一行显示内容 应该把内容再次调用该方法
	void InitTextToData (const char *text, int   *value = 0, P<GNg::sIMAGE> icon=P_());

	//插入文本提示(	    文本内容,		附加跟综显示的可变值,  该项的图标) !如果加了跟踪值 那么首个参数里要加1个'%f'表明显示格式 第2个参数不能为局部临时变量
	void InitTextToDataf(const char *text, float *value = 0, P<GNg::sIMAGE> icon=P_());

	//插入子选单(			子选单,             子选单名称,     子选单确定等按钮表示形式:<0. > <1.cancel> <2.cancel|save> <3.cancel|save|default>, 该项的图标)
	void InitSubMenuToData(sSIM_MENU *menu, const char *name, char menu_type=2, P<GNg::sIMAGE> icon=P_());

	//替换指定子项名称(     子项的原名,        子项的新名) 返回true表示替换了 返回false表示找不到指定名称项
	bool ReplaceChildName(const char *name, const char *new_name);

	//输入子项字符内容( 子项的名称,          要输入的字符内容) 返回true表示成功输入 返回false表示找不到指定名称项
	bool InputChildContent(const char *name, const char *content);

    //获得已插入了的子项数() 返回子项数
	inline unsigned int GetInitedChildNum(){ return zChild.size(); }

	//获得特定子项(第几个子项) 返回子项指针
	inline GT_sSP_CHILD *const GetChild(unsigned int id){ return &zChild[id]; }

	//设置特定子项名称(第几个子项, 新名称)
	void SetChildName(unsigned int id, const char * name);
};
/*-----------------------------------------------------------------------------------------------------------------------------------------------*/

class GN_API cSIM_DIALOG :public sSIM_MENU //★★★简单对话筐
{  protected:
	/* 用于方便实现变量值的交互修改和显示并保存文件自动读取等 也可用来做弹出式菜单  就算单独用键盘也能很好地控制 */

	GNg::cTEXT_ART				zTextOut;        //文本输出器
	GNg::cFRAME_RC			   *zEffect;         //弹出动画效果筐

	sSIM_MENU				   *zTargetMenu;     //当前处理的选单
	GNg::sIMAGE				   *zTargetIcon;     //当前选单图标
	char					   *zTargetCaption;  //当前选单标题名
	int8						zTargetType;     //当前选单功能按钮表示形式

	uint8						zDetailSetState; //当前正在进行详细设置值的状态类型
    bool						zShowReady;      //标记是否准备好了
	bool						zMouseLeft;      //当前鼠标左键使用状态
	short						zMouseCPx;       //当前鼠标左键刚按下时的x位置
	short						zMouseCPy;       //当前鼠标左键刚按下时的x位置
	short						zSelect;         //当前选到子选项ID

    short					    zDlgx;           //对话筐位置X
	short						zDlgy;           //对话筐位置Y
	short						zDlgw;           //对话筐宽度
	short						zDlgh;           //对话筐高度
	short						zChildNum;       //当前正在处理选单的子项数
	short						zIconW;          //选单图标显示宽度
    short						zIconH;          //选单图标显示高度
    short						zIconW2;         //选单主题图标显示宽度
    short						zIconH2;         //选单主题图标显示高度

	P<GT_sVALUE_SET_TOOL>		zSetTool;		 //详细设置值的工具
	P<GNg::sRECT>				zTmpRangeRect;	 //更新显示时计算的临时范围
	eSET_PATH_TYPE				zSetPathType;    //文件路径修改设置类型

	short						zKeyOK;          //"OK确定"的键位
    short						zKeyDOWN;        //"下一个"的键位
	short						zKeyUP;          //"上一个"的键位
	short						zKeyLEFT;        //"左选"的键位
	short						zKeyRIGHT;       //"右选"的键位
	bool						zUsMouse;        //是否使用鼠标控制
	bool						zIsMouseFocus;   //当前鼠标是否指到对话筐或正在设置对话筐
	bool						zIsMoveDlg;      //是否要移动对话筐
	bool						zIsDetailSet;    //是否启用详细设置

	GNg::sIMAGE				   *zFocusMap;       //焦点式样图
    GNg::sIMAGE				   *zArrowMap;       //小箭头式样图
	GNg::sIMAGE				   *zBackFrame;      //底筐9宫格图
	GNg::sIMAGE				   *zBackCaption;    //底筐标题图
	bool					    zBackFrameRepeat;//底筐9宫格图是否重复平铺填满中间内容
    bool					    zUsGenArrowMap;  //是否使用自动生成的小箭头式样图
    int8						zState;          //运行状态
    bool						zfAm_s_e;        //焦点来回状态
	int							zfNowtime;       //焦点闪动现在时间
    unsigned int 				zNowtime;        //动画效果现在时间
	short					    zFocusFlash;     //焦点闪动间隔时间

	short						zSubValueW;      //子修改值项文本的总显示宽度
	short						zNameW;          //修改值名文本显示宽度
	short						zValueW;         //修改值文本显示宽度
	short						zBorderW;        //内边筐宽度
    short						zEnumNameW;      //列举项文本显示宽度
    short						zFuncButtonW;    //功能按纽文本显示宽度

	short					    zIconNW;         //选单最大图标宽度
    short						zIconNH;         //选单最大图标高度
	short						zCaptionNum;     //标题文字数
	short						zNameNum;        //修改值名文字数
	short						zValueNum;       //修改值文字数
	short						zSubMenuNum;     //子选单文字数
	short						zSubValueNum;    //子修改值项文本的总文字数
    short						zEnumNameNum;    //列举项文字数
    short						zFuncButtonNum;  //功能按纽文字数
	short						zTotalLenNum;    //内容最长的总文字数
	short						zDlaOffsetX;     //对话筐经过鼠标拖动的X偏移量
	short						zDlaOffsetY;     //对话筐经过鼠标拖动的Y偏移量
	short						zTi_compartment; //原文本字横向间距
    short						zTj_compartment; //原文本字竖向间距
	short						zTwidth;         //原文本字宽
	short						zTheight;        //原文本字高
	float						zT_ef_x;         //原效果筐的原点坐标X
	float						zT_ef_y;         //原效果筐的原点坐标Y
	GNg::sRECT2					zT_effect_rect;  //原效果筐的原点范围
	GNg::sRECT					zDrawBackFrame;  //底筐放在底下的绘制范围 这个是弥补WINCE不支持透明 记录焦点放在底下显示判断用的

    short						zSelectLast;     //上次选到子选项ID
	short						zSelectLockID;   //当前被锁定的修改值所在选单的行号ID

	char					   *zButtonName;     //功能按钮名
	char					   *zFileName;       //存档维护文件名
   #ifdef _GN_SOUND_H_
	GNa::sSOUND				   *zSouObj;         //按键声音
	GNa::cSOUND_SRC				zSound;          //按键音发声点
   #endif
	void (*zBackDrawProc)(short,short,short,short,unsigned int ,unsigned short,GNg::sIMAGE*); //自定义的底筐绘制方法
	void  *zValueChangeArg;				  //传给 zValueChangeProc 用的参数
	void (*zValueChangeProc)(void*);      //当值被修改后的用户回调
	void (*zFun1)();                      //点中功能按钮1的用户回调
    void (*zFun2)();                      //点中功能按钮2的用户回调
	void (*zFun3)();                      //点中功能按钮3的用户回调

	//详细设置返回选项长度插值(变量设置工具集, 范围)
	void zDetailSetReturnLongLinear(GT_sVALUE_SET_TOOL *_set_tool, const GNg::sRECT *rng);
	//详细设置返回范围插值(变量设置工具集, 范围, 目标颜色)
	void zDetailSetReturnRangeLinear(GT_sVALUE_SET_TOOL *_set_tool, const GNg::sRECT *rng, unsigned int tg_color);
	//设置当前激活的选单(选单 默认为再次激活原选单)
	void zSetActiveMenu(const sSIM_MENU *menu=0);
	//统计对话筐内的各子项字符个数(需要计算的选单, 选单的标题名)
	void zStatisticsDlgTextNum(const sSIM_MENU *menu, const char *caption);
	//统计对话筐尺寸和其他宽度等(宽放大比率, 高放大比率)
	void zStatisticsDlgSize(float ratio_w, float ratio_h);
    //保存当前层选单的变量数据到文件(数据写入偏移)
	void zSave(int offset_num);
	//隐藏对话筐
	void zHide();
	//安全删除小箭头式样图
	void zDeleteArrowMap();
	//释放内部资源(是否连子选单一起释放)
	void zRelease_(bool all);
  public:
	int zGetLastDetailSetCY();	 //获得最近详细设置值时的显示中心y位置 !如果不存在详细设置返回-100000

	static char  aFuntionButtonName[2][3][9]; //记录功能按钮名

	unsigned int                aCaptextColor;   //标题颜色
    unsigned int                aTextColor;      //文本颜色
	unsigned int                aBackColor;      //底筐颜色
	unsigned int                aLineColor;      //线筐颜色
	unsigned int                aFocusColor;     //焦点颜色
	unsigned int                aInputBackColor; //详细设置的背景颜色
	unsigned int                aInputBackBlend; //详细设置的背景混合模式
    unsigned short              aTextBlend;      //文本混合模式
	unsigned short              aBackBlend;      //底筐混合模式
	unsigned short              aLineBlend;      //线筐混合模式
	unsigned short              aFocusBlend;     //焦点混合模式
    unsigned short              aLineWidth;      //线筐宽度
	short						aCaptextOfsY;	 //标题显示高度偏移
	bool                        aAffectColor;    //弹出动画筐色是否影内容颜色
	bool                        aValueLoop;      //修改变量值超出最大时是否循环从最小开始
	bool                        aSkipPos;        //是否锁定对话筐的位置使鼠标不能拖动

	cSIM_DIALOG();
   ~cSIM_DIALOG();
    inline void Release(){ zRelease_(false); }     //只释放它的内容
	inline void ReleaseAll(){ zRelease_(true); }   //全部释放 这包含所有子选单所包含的空间

	//准备值数据保存的文件(文件名) 本类会自动维护该文件的数据读写保存 !!文件名无需带后续名 引擎会自动加上专用的后续名
	void BeginInitToData(const char *filename);

	//结束插入修改值 !以上这两个函数只有想要安全自动保存设置的值才需调用 要自动保存 值必需是占4个字节的数据类型 只想修改变量值可不调用这两个
    void EndInitToData();

	/*-----------------------------------------------------------------------------------------------------------*\
	|	当你用 InitValueToData 加入修改值条目时 想要该类为你自动管理安全保存变量值到文件时                        |
	|	你必需先确定好所有要修改的值条目后 才在条目代码的首尾加上 BeginInitToData ...EndInitToData() 函数对       |
	|	来自动为您生成文件 当文件生成后你再更改过值条目的数目或顺序 造成不匹配 引擎会自动重设置所有值项为默认值   |
	\------------------------------------------------------------------------------------------------------------*/

	//设置对话筐显示式样
	bool SetStyleToData(
		const char           *caption="(GN)Option",   //对话筐标题
		const GNg::cTEXT_ART *text_style=0,           //文字输出式样 !这个不支持竖方向排列方式
		                int8  type=2,                 //确定按钮表示形式:<0.   > <1.cancel> <2.cancel|save> <3.cancel|save|default>
			   unsigned int   captext_color=0,        //标题文本颜色
		       unsigned int   text_color=0,           //文本颜色
		       unsigned int   back_color=0,           //底筐色
               unsigned int   line_color=0,           //线筐色
		       unsigned int   focus_color=0xffff3434, //焦点颜色
		      unsigned short  text_blend=1,           //文本混合模式
		      unsigned short  back_blend=1,           //底筐混合模式
              unsigned short  line_blend=1,           //线筐混合模式 值0:隐藏线筐
		      unsigned short  focus_blend=2,          //焦点混合模式
			  unsigned short  line_width =2,          //线筐宽度
			  P<GNg::sIMAGE>  icon =P_()			  //标题左边图标
	); //返回是否成功

	//设置控制按键
	void SetKeyToData(
	   #ifdef _GN_SOUND_H_
		  const GNa::sSOUND	 *key_sound,			  //按键声音对象
       #else
                 const void  *key_sound,			  //没有按键声音 保留占位填0即可
       #endif
	                  short   ok = GK_RETURN,		  //确定键位
		              short   down = GK_DOWN,		  //下一个键位
		              short   up = GK_UP,			  //上一个键位
		              short   left = 0,				  //左选键位
		              short   right = 1,			  //右选键位
					   bool   usMouse = 0			  //是否使用鼠标控制
	);

	//设置底筐(底筐图:必须是9格关联组成的边筐图, 标题底图, 是否重复平铺填满中间内容, 自定义底筐图绘制方法(筐位置X, 筐位置Y, 筐宽, 筐高, 颜色, 混合模式, 传回给您的参数)) !如有设置底筐图绘制方法 那么参数 frame_img 将传回给您处理
	void SetBackFrame(const GNg::sIMAGE * frame_img,  const GNg::sIMAGE * caption_img, bool repeat=false, void (*draw_call)(short,short,short,short,unsigned int ,unsigned short,GNg::sIMAGE*)=0);
	//仅设置底筐标题图(标题底图)
	void SetBackCaption(const GNg::sIMAGE * caption_img){ zBackCaption = (GNg::sIMAGE *)caption_img; }

	//设置动画效果
	void SetEffect(
		             GNg::cFRAME_RC  *effect,                   //动画效果
		             short			  focus_flash_time=0,       //焦点闪动间隔时间,0为不闪动
			   const GNg::sIMAGE     *focus_style=0,            //选择焦点式样图
			   const GNg::sIMAGE     *arrow_style=0,            //小箭头式样图
			   bool                   affect_color=true         //动画筐色是否影内容颜色
			   );

    //设置是否开启详细设置(是否开启详细设置, 在详细修改文本内容时的输入背景颜色, 输入背景混合模式, 文件路径值修改设置类型)
	void EnableDetailSetting(bool detail_set, unsigned int  input_bgcolor=0x55000000, unsigned short input_bgblend=1, eSET_PATH_TYPE set_path_type=eSET_PATH_INPUT);

	//详细设置里的文件路径选择目录添加(一组目录 成功调用后里面的条目会组合了原选择目录组一起) !新添加上的目录会在选择列表的前面 !调用这个前必须确保先调用 EnableDetailSetting 且参数 set_path_type 为eSET_PATH_SELECT或eSET_PATH_SELECT下时才生效
	void PathSelectDirectoryAdd(P< P<char> > dirs);

	//设置在详细设置下的首选打开文件路径所在选择目录ID 对应用户目录选择列表里的项(目录路径ID ) !调用这个前必须确保先调用 EnableDetailSetting 且参数 set_path_type 为eSET_PATH_SELECT或eSET_PATH_SELECT下时才生效
	void SetFileSelectOpenDirectoryID(uint8 did);

	//设置修改值是否可以由小到大重复循环(是否启用)
	void SetValueChange(bool value_loop);

	//设置当值被修改后的处理回调(处理函数(传回给您的用户参数)默认0:没有额外处理, 用户参数) !这个无论变量值已经是最大或最小值时 实质数据即使没有变化 但只要对话筐有去修改过变量乃然会调用处理函数
    void OnValueChange(void (*proc)(void*)=0, void *us_arg=0);

	//替换功能按钮的处理方法
	void ReplaceFuntion(
		        char button_name[2][3][9], //两种语言3个功能按钮名 每个按钮名不能超出8个字节 一般格式为char button_name[2][3][9]={{"取 消","保 存","默 认"},{"Cancel","Save","Default"}};
                void (*fun1)() = 0,  //当点中按钮1的处理回调  默认0:使用默认处理
				void (*fun2)() = 0,  //当点中按钮2的处理回调  默认0:使用默认处理
				void (*fun3)() = 0   //当点中按钮3的处理回调  默认0:使用默认处理
	);

	//更新运行对话筐(帧间隔时间) 返回正在处理的子选单对象指针 返回0表示已经确定结束对话筐了 !这个要在每帧都行的地方挂起
	sSIM_MENU *UpdateRun(unsigned int updatatime);

	/*- 本类一般基本使用顺序: ------------------------------------------------------------------------------------\
	|	GNu::cSIM_DIALOG mydlg;                                                                                   |
	|                                                                                                             |
	|	mydlg.BeginInitToData(...);  //启用自动保存参数值功能  !这个比较适合游戏里的-选项设置-简单菜单的存档      |
	|	mydlg.InitValueToData(...);  //这里设置的修改值不能为局部临时变量										  |
	|	mydlg.InitValueToData(...);                                                                               |
	|	mydlg.InitTextToData(...);   //插入一行文本仅显示                                                         |
	|	mydlg.EndInitToData();                                                                                    |
	|	mydlg.SetStyleToData(...);                                                                                |
	|	mydlg.Pop();                 //弹出对话筐                                                                 |
	|                                                                                                             |
	|	mydlg.UpdateRun(...);		 //这个要放在主循环里挂起                                                     |
	|	                                                                                                          |
	|	mydlg.ReleaseAll();          //用完后释放内部空间                                                         |
	\*-----------------------------------------------------------------------------------------------------------*/

	void Pop();                                            //弹出对话筐
    void Pop(const char *new_caption);                     //重设置标题再弹出对话筐(标题)
    void RePop();										   //重新弹出当前层的对话筐
	void Hide();                                           //隐藏对话筐
	void OffsetPos(int ofsx, int ofsy);					   //偏移对话框位置(移动X, 移动Y)
	void Default(bool this_also=false);                    //把当前层菜单的值还原为默认值(若当前层菜单不是它本身是否也把它的层变量值还原为默认值) 默认值就是用InitValueToData设置时该修改值的初值 这个跟在对话筐内按下默认按钮效果一样
	void Cancel();                                         //取消当前的一层菜单   这个跟在对话筐内按下对应的按钮效果一样 如果是跟层对话筐即隐藏它 如果是子菜单即返回到上一层对话筐或菜单
	void Save();                                           //保存当前层菜单的值 若您调用过 BeginInitToData对 也将会把数据保存到文件 这个跟在对话筐内按下对应的按钮效果一样

	inline int8  GetState(){ return zState; }							   //获得当前对话筐状态( ) 返回状态 0:隐藏中 1:正在打开 2:已经显示中待操作了 3:正在关闭
	inline char *GetCaption(){ return zCaption; }						   //获得对话筐标题()标题
	inline char *GetCurProcCaption(){ return zTargetCaption; }			   //获得当前正在处理的子选单标题()返回子选单标题
	inline sSIM_MENU *GetCurProcMenu(){ return zTargetMenu; }			   //获得当前正在处理的子选单()返回子选单指针

	inline GT_PFUNC_PVOID GetValueChangeProc(){ return zValueChangeProc; } //获得值被修改后的处理函数回调
	inline GNg::cFRAME_RC *const GetEffect(){ return zEffect; }			   //获得弹出动画效果筐
	inline GNg::sIMAGE    *const GetFocusStyle(){ return zFocusMap; }	   //获得选择焦点式样图
	inline GNg::cTEXT_ART *const GetTextStyle(){ return &zTextOut; }	   //获得文本输出式样
	unsigned int  GetEffectLevelColor();								   //获得当前动画效果层影响后的最终颜色 如果无设置动画效果返回 0xffffffff
	void  GetDlgPos(int &_x, int &_y, int &_w, int &_h);				   //获得对话筐的显示位置范围(返回位置X, 返回位置Y, 宽度, 高度) 这个的大小只有调用了Pop()弹出以后才有正确大小返回
	void  GetDlgRect(GNg::sRECT *rect_out);								   //获得对话筐的显示范围矩型(矩型输出) 这个的大小只有调用了Pop()弹出以后才有正确大小返回

	void  GetDetailSetPos(int &_x, int &_y, int &_w, int &_h);                              //获得当前详细设置值的显示位置范围(返回位置X输出, 返回位置Y, 返回宽度, 返回高度) 这个的大小只有值正在进行详细设置时才有正确大小返回
	void  GetDetailSetPos(GNg::sRECT *rect_out);                                            //获得当前详细设置值的显示位置范围(范围矩型输出) 这个的大小只有值正在进行详细设置时才有正确大小返回
	eDETAIL_SET_TYPE GetCurDetailSetState(){ return (eDETAIL_SET_TYPE)zDetailSetState; }    //获得当前正在进行详细设置值的类型
	inline eSET_PATH_TYPE GetSetPathType(){ return zSetPathType; }						    //获得在详细设置里文件路径修改设置类型
	inline bool  GetDetailSetEnable(){ return zIsDetailSet; }								//获得对话筐是否有启用详细设置()返回是否
	inline GNu::cFILE_BROWSER *GetDetailSetFileBrowser(){ if(zSetTool)return zSetTool->file_browser; else return 0; } //获得详细设置的文件浏览器 !当文件浏览没打开过时 调用这个将返回0

	void  GetChildPos(int &_x,int &_y,int &_w,int &_h,short child_id,eCHILD_CONTENT_TYPE tpye); //获取对话筐内子选项的内容位置范围(返回位置X, 返回位置Y, 返回宽度, 返回高度, 子选项ID, 内容类型) 这个的大小只有调用了Pop()弹出以后才有正确大小返回 !这个调用是需要计算位置的而不是直接有位置信息返回
	void  GetChildRect(GNg::sRECT *rect_out,      short child_id,eCHILD_CONTENT_TYPE tpye);		//获取对话筐内子选项的内容范围矩型(范围输出, 子选项ID, 内容类型) 这个的大小只有调用了Pop()弹出以后才有正确大小返回, !这个调用是需要计算位置的而不是直接有位置信息返回
	inline short GetChildNum(){ return zChildNum; }												//获得对话筐内当前的子选项数 !这个获取到的是运行时的数目 如果正在加入条目时要获取的话请调用 GetInitedChildNum()
	inline short GetChildIdCur(){ return zSelect; }												//获得对话筐内当前选到子选项ID 如果选到的是功能按钮 返回值就等于当前子选项数

	bool IsMouseFocus(){ bool ret=zIsMouseFocus; zIsMouseFocus=false; return ret; }				//返回鼠标是否指到该对象
}; //end cSIM_DIALOG

class GN_API cSIM_DIALOGEX :public cSIM_DIALOG //★★★简单对话筐扩展
{
	void (*zValueChangeProc2)(void*);				//当变量被更改后的用户回调指针
	void  *zValueChangeArg2;						//当变量被更改后的用户回调用到的参数
   protected:
	void			   *zTsItems;					//类型转换条目

	void  zRemoveValueRelatTsItems();   //移除它的变量关联条目内容
   public:
    void  zOnValueChangeProc(unsigned short *chg_id_out);	//当变量值被修改后的固定处理(如果有指定返回第几个通过InitValueToData1设置的变量被修改了 如果多个变量同时被修改 该值为最后一个被修改变量的位置 如果都没该值为0xffff)

	cSIM_DIALOGEX();
   ~cSIM_DIALOGEX();
    void RemoveValue();				    //只移除它的变量内容
    void RemoveValueAll();			    //移除它的变量内容 这包含连子选单对象一起移除变量内容
    void Release();						//只释放它的内容
	void ReleaseAll();					//全部释放 这包含所有子选单所包含的空间

	//设置当变量值被修改后的处理函数(处理函数(传回给您的用户参数)默认0:为没有处理, 用户参数) !这个无论变量值已经是最大或最小值时 实质数据即使没有变化 但只要对话筐有去修改过变量乃然会调用处理函数
	void OnValueChange(void (*proc)(void*)=0, void *us_arg=0);

	//初始化插入变量元素( 插入到的目标菜单 也可以为本对话框, 要修改的变量,           对应名称,             最大值,         最小值,         每次增加量,    列举名:"YES|NO"用这格式可以填入变量筐里面,  代表变量的图标)
	void InitValueToData1(sSIM_MENU *tg, uint8  *value,        const char *name,   uint8 max_value,   uint8 min_value=0,    uint8 add_value=1, const char *enum_name=0, P<GNg::sIMAGE> icon=P_());

	//更新对话筐显示值 !有时要修改的变量在该对话筐以外修改了 对话筐里显示可能未能及时刷新 调用这个来通知刷新
	void UpValues();

	inline GT_PFUNC_PVOID GetValueChangeProc(){ return zValueChangeProc2; } //获得当前变量值被修改后的处理函数回调
}; //end cSIM_DIALOGEX
/********************************************************************************************************************************************************************************************************************************/

class GN_API cUI_SIM_DIALOG :public cUI_NEAR_OBJECT, public cSIM_DIALOGEX //★靠边简单对话筐
{  protected:
	P<GNg::sIMAGE>				zFocusMap1;      //焦点式样图真身
	P<GNg::sIMAGE>				zArrowMap1;      //小箭头式样图真身
	P<GNg::sIMAGE>				zBackFrame1;     //底筐9宫格图真身
	P<GNg::sIMAGE>				zBackCaption1;   //底筐标题图真身
   #ifdef _GN_SOUND_H_
	P<GNa::sSOUND>			    zSouObj1;		 //按键声音
   #endif
	P<char>						zObjsKeep;		 //保留来鉴别有多少个复制副本
   public:
	uint8						aBackBlurLv;	 //底筐模糊级别 1-4  0表示禁用

	cUI_SIM_DIALOG():aBackBlurLv(0){ aType = eUI_SIM_DIALOG; w = 0; h = 0; }
	virtual ~cUI_SIM_DIALOG();
	
	virtual P<iUI_OBJECT> Clone();										 //复制出该UI组件的一个新副本
	virtual void UpdateRun(unsigned int updatetime, sUI_LEVEL *parent);  //更新运行(帧间隔时间, 父层级信息链)
	virtual void GetDrawArea(GNg::sDRAW_AREA2 &out, cUI_GROUP *parent);  //获得UI组件的实际绘制区域(返回区域, 父层组 您可以不指定它) !需在 UpdateRun UpdatePlay 执行后再调用该方法才有正确值输出

	//把内部数据储存为一整段数据时的处理(收集共享对象集, 返回整段数据 首个字节为版本号之后4个字节为aType 当该值为0时仅返回数据段需要大小)返回数据段大小字节
	virtual unsigned int DataSave(cSHARE_DATA *share_objs, uint8 *data_out=0);

	//从文件数据段里读入数据时的处理(引用共享对象集, 数据段地址 首个字节为版本号之后4个字节为aType)返回已读取字节数 失败返回0 
	virtual unsigned int DataLoad(cSHARE_DATA *share_objs, const uint8 *data);

	/* ------------------------------------------ 以下是本类用安全指针代替基类裸指针的方法 ------------------------------------------------------------------------------------------------------------------------------------- */
   #ifdef _GN_SOUND_H_
	//设置控制按键
	void SetKeyToData(
		  /*const*/ P<GNa::sSOUND> &key_sound,		  //按键声音对象
	                  short   ok = GK_RETURN,		  //确定键位
		              short   down = GK_DOWN,		  //下一个键位
		              short   up = GK_UP,			  //上一个键位
		              short   left = 0,				  //左选键位
		              short   right = 1,			  //右选键位
					   bool   usMouse = 0			  //是否使用鼠标控制
	);
   #endif
	//设置底筐(底筐图:必须是9格关联组成的边筐图, 标题底图, 是否重复平铺填满中间内容, 自定义底筐图绘制方法(筐位置X, 筐位置Y, 筐宽, 筐高, 颜色, 混合模式, 传回给您的参数)) !如有设置底筐图绘制方法 那么参数 frame_img 将传回给您处理
	void SetBackFrame(/*const*/ P<GNg::sIMAGE> &frame_img, /*const*/ P<GNg::sIMAGE> &caption_img, bool repeat=false, void (*draw_call)(short,short,short,short,unsigned int ,unsigned short,GNg::sIMAGE*)=0);
	//仅设置底筐标题图(标题底图)
	void SetBackCaption(/*const*/ P<GNg::sIMAGE> &caption_img){ zBackCaption = (GNg::sIMAGE *)caption_img; zBackCaption1 = caption_img; }

	//设置动画效果
	void SetEffect(
		             GNg::cFRAME_RC  *effect,						//动画效果
		             short			  focus_flash_time=0,			//焦点闪动间隔时间,0为不闪动
		   /*const*/ P<GNg::sIMAGE>   focus_style=P<GNg::sIMAGE>(), //选择焦点式样图
		   /*const*/ P<GNg::sIMAGE>   arrow_style=P<GNg::sIMAGE>(), //小箭头式样图
			   bool                   affect_color=true			    //动画筐色是否影内容颜色
			   );

	//转移小箭头式样图到安全指针的真身里
	void ArrowMapTransfer();
	//保管外部连接对象 调用后 zEffect 等外部对象数据不必在外释放了
	void KeepExternObjs(){ zObjsKeep.New(1); }
};
};  //end namespace GNu
#endif //_GN_UIPOPDLG_H_
