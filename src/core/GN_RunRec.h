#pragma once
#ifndef _GN_RUNINFO_H_
#define _GN_RUNINFO_H_

GN_API void GN_ExitInfoPrint();         //打印一些退出时还未作处理的信息
GN_API void GN_ShowMemoryUsingState();  //显示内存使用状态

class GN_API GT_cRUN_REC	//运行信息记录器
{
	GN::sARRAY_VEC<GNg::sDRAW_AREA,unsigned short> zDrawRects; //矩形框显示记录
    void zPushShowValueInfo(int ID_order, char *info);
    
	int                         zNowtime[4];      //分别记录几种不同信息显示动画的现在时间
	int                         zEndtime[4];      //分别记录几种不同信息显示动画的结束时间
    unsigned char               zAm_s_e[4];       //分别记录几种不同信息显示动画是否结束了
    int                         ztValue[3];       //临时值 用于校验是否需要重设文本

	unsigned int                zFPS_AddTime;     //帧累加时间
    unsigned short              zFPS_AddCount;    //帧累加次数
	unsigned short              zFPS_Value;       //帧间隔值

	short                       zTextOutID;       //标记当前处理到的文本显示记录
	short                       zTextOutNum;      //文本显示记录个数
	GNg::sTEXT_SET			   *zTextOutDatas;    //文本显示记录数据

	int                         zFocus;           //添加焦点 内部字符记录了的个数
	char                       *zpErrorInfo;      //储存错误输出信息
	int                         zMaxInfoStrNum;   //储存错误信息的缓冲空间大小
	int                         zZoningNum;       //信息筐最大向下滚动高度
    float                       zUpdownValue;     //当前筐内信息文本的上落偏移量
    float                       zUpdownValueTg;   //当前筐内信息文本的上落目标偏移量
	bool                        zInfoOpenChange;  //信息筐更改打开状况
	bool                        zInfoIsOpen;      //信息筐是否打开
	bool                        zKeyOnDown;       //当前是否在信息筐内鼠右键 或滚上滚下有按紧

	GNg::sIMAGE                 zFontMap;         //字体图
	GNg::sIMAGE                 zIconLoad;        //读取进度时的显示图标即公司LOGO
	GNg::sIMAGE                 zBarLoad;         //读取进度条时的式样图
   public:
    int                         zRuntimeStrNum;   //    运行时间信息字符总个数
    bool                        zGWPrintReady;    //是否可以开启图形窗口输出
	char                        zpRunTime[128];   //储存运行时间信息的缓冲

	unsigned int                aFontColor;       //字体颜色
    unsigned int                aBackColor;       //底框颜色
	unsigned int                aWarningColor;    //报警!号与高亮的颜色

	GNg::cTEXT_ART              aFont;            //用户信息文本输出器
    GNg::cTEXT_ART              aErrorFont;       //错误信息文本输出器
	P<GNg::sIMAGE>              aGV_OffsetArrow;  //内部一个偏移方向条
    P<GNg::sIMAGE>              aGV_Point32;      //内部一个扩散的点图案 32*32
	
	char					   *aComHostName;	  //公司网站主机名 这个其实是指向 aComWeb "http://" 往后的内容
    P<char>                     aComWeb;          //公司网址
	P<char>                     aComName;         //公司名称
	P<char>                     aComTel;          //公司电话号码

    GT_cRUN_REC(void);
   ~GT_cRUN_REC(void);
    
    //初始化显示式样和加载所需资源
	bool InitStyleToData(uint8 fontstyle  = GT_AppDefaultFontStyle,   /*字体式样*/
		         unsigned int  fontcolor  = GT_AppDefaultColorFont,   /*字体颜色*/
				 unsigned int  backcolor  = GT_AppDefaultColorBack,   /*显示的底色*/
				 unsigned int  focuscolor = GT_AppDefaultColorFocus); /*高亮显示的颜色*/ //返回读取外部的资源是否成功 !这个能够重入而不会造成内存涉漏

	//系统显示式样获取(返回当前系统字体式样值, 返回当前系统字体颜色, 返回当前系统背景底色, 返回当前系统焦点高亮颜色) !相当于获取 InitStyleToData 的输入参数
	void ShowStyleGet(unsigned char &fontstyle, unsigned int  &fontcolor, unsigned int  &backcolor, unsigned int  &focuscolor);

	//初始化 这个在里面会自动调用 InitStyleToData 返回读取外部资源是否成功 !这个能够重入而不会造成内存涉漏
	bool InitToData();

	//释放内部资源()
    void Release();

	//更新数据显示(参数是每帧所经过的时间) !放在主循环里挂起
	void Show(unsigned int updatatime);


	//发出“!"号报错()
	void ReportError();

	//显示实时变量值(变量输入,			   显示名称,  排列顺序ID） !这个可以不在绘制2D的地方调用都能正确显示
	void ValueCheck(float		   value, char *name, int ID_order);
    void ValueCheck(double		   value, char *name, int ID_order);
	void ValueCheck(int			   value, char *name, int ID_order);
	void ValueCheck(unsigned int   value, char *name, int ID_order);
    void ValueCheck(long		   value, char *name, int ID_order);
	void ValueCheck(unsigned long  value, char *name, int ID_order);
    void ValueCheck(short		   value, char *name, int ID_order);
    void ValueCheck(unsigned short value, char *name, int ID_order);
    void ValueCheck(unsigned char  value, char *name, int ID_order);
    void ValueCheck(char		   value, char *name, int ID_order);
    void ValueCheck(char		  *value, char *name, int ID_order);
    void ValueCheck(bool		   value, char *name, int ID_order);
	//显示实时指针值(指针输入,			   显示名称,  排列顺序ID） !这个可以不在绘制2D的地方调用都能正确显示
    void ValueCheck(void		  *value, char *name, int ID_order);

	//输出文本(要显示的文本, 输出个数 值0自动输出全部, 坐标X, 坐标Y, 文本筐宽度, 文本筐高度,   文字颜色,            混合模式,      文本在筐内的偏移X量, 文本在筐内的偏移Y量) !这个可以不在绘制2D的地方调用都能正确显示
	void OutText(char *text, unsigned short count=0, int x=0, int y=0, int w=0, int h=0, unsigned int  color=0xffffffff, unsigned short blendmode=1, int text_offset_x=0, int text_offset_y=0);

	//绘制图框(坐标X, 坐标Y, 筐宽度, 筐高度, 颜色) !这个可以不在绘制2D的地方调用都能正确显示
	void DrawFrame(int x, int y, int w, int h, unsigned int  color=0xffffffff);


	//打印输出信息(格式, 参数队列) !用法跟 printf 差不多 这里打印的信息总量不能超过 PrintMaxNumGet() 否则会出轨 最多打印显示行数为255行! 调用这个之后会在视口上边角出现一个浮动'!'号 点击它可以打开查看打印出来的内容
	void  Print(const char *fmt, ...);
	//设置打印输出信息的内容(内容文本) 如果内容文本长度超过255行或超过PrintMaxNumGet() 会自动在末尾截断并加上"..."来结束 !!调用这个之后会在视口上边角出现一个浮动'!'号 点击它可以打开查看打印出来的内容
	void  PrintInfoSet(const char *text);
	//获得已经打印的信息内容(信息内容字符数输出) 返回内部信息内容
	char *PrintInfoGet(int *num_out=0);              
	//打印输出信息清除()
	void  PrintInfoClear();      
	//打开或关闭输出信息视图() !如果当前是关闭则打开 反之就关闭
	void  PrintViewOnOff();          
	//获得输出信息视图是否正在打开()返回是否
	inline bool PrintViewIsOn(){ return zInfoIsOpen; }     
	//获得使用 Print 可打印信息的最大字符个数()返回个数
	inline int  PrintMaxNumGet(){ return zMaxInfoStrNum; } 
    

	//版本绘制(左上角显示坐标X, 坐标Y,         LOGO图标颜色,                 文字颜色,									 号码颜色）
    void VersionDraw(int x, int y, unsigned int  icon_color=0xffffffff, unsigned int  text_color=0xffffffff, unsigned int  num_color=0xffffffff);
	//获得显示版本信息占用区划大小(区划宽度输出, 高度输出)
	void VersionSizeGet(int &w, int &h);
    //获得版本图标的图像()
	inline GNg::sIMAGE /*const*/* VersionIconGet(){ return &zIconLoad; }

	//读取进度绘制(进度:0.0f-1.0f, 进度条式样, 是否重绘图标字样, 心中位置X, Y,       LOGO图标颜色,                      LOADING文字颜色,                     进度筐颜色,                             进度条颜色 ,                     混合模式) !参数进度条式样:0不明确方式LOADING 1明确方式LOADING 2不明确方式EXECING 3明确方式EXECING
    void ProgressDraw(float progress, uint8 style, bool re_draw, int cx, int cy, unsigned int  icon_color=0xffffffff, unsigned int  text_color=0xffffffff, unsigned int  frame_color=0xffffffff, unsigned int  bar_color=0xffffffff, unsigned short blendmode=1);
	//获得显示进度条占用区划大小(区划宽度输出, 高度输出)
	void ProgressSizeGet(int &w, int &h);

	//获得帧速率()
	inline unsigned short GetFPS(){ return zFPS_Value; }

	//显示感叹号(坐标X, 坐标Y, 大小比例, 颜色)  这个对齐中心是"!"号的中心
	static void DrawWarning(int x, int y, float size_ratio, unsigned int  color);

	/*------------------------------ 使用以下这些定义可以在没这个头文件时在其它地方的调用自动失效忽略 -------------------------------------------------------------------------*/
	#undef GN_RUNTSTART
	#undef GN_RUNTEND
	#undef GN_RUNTENDMF
	#undef GN_RunReportError
	#undef GN_RunTextOut
	#undef GN_RunDrawFrame
	#undef GN_RunValueCheck
	#undef GN_RunValueCheck2
	#undef GN_RunKCV
	#undef GN_RunKCV1
	#undef GN_RunKCOF
	#undef GN_RunProgressDraw
	#undef GN_RunProgressSizeGet

    //纪录运行开始时间(重复执行代码次数)
	#define GN_RUNTSTART(nc) \
		unsigned int fcnt = nc;\
		double startTime = GN_TimeGetCount();\
		for(unsigned int irun = 0; irun < fcnt; ++irun){

    //记录运行结束时间() ! GN_RUNTSTART 和 GN_RUNTEND 需在同一作用域范围内成对使用
	#define GN_RUNTEND( time_proc ) } double s_e_Time=GN_TimeGetCount()-startTime;\
        time_proc \
        GN::RunRec.zRuntimeStrNum = sprintf( GN::RunRec.zpRunTime,/*"ALLRUN-TIME:%16.3fms  N:%10d\nEVERY--TIME:%16.10fms"*/"总运行时间   :%16.6f 毫秒  运行次数:%10d\n单次运行时间 :%16.10f 毫秒", s_e_Time, fcnt, s_e_Time/fcnt)

    //用来记录结束时间() !这个会收集多帧时间取个平均值
	#define GN_RUNTENDMF() GN_RUNTEND( GN_AverageFrameTimeT( double , s_e_Time , 80.0 ); )

	//发出 ! 号报错
	#define GN_RunReportError() GN::RunRec.ReportError()

    //在图形区任意输出文本(文本字符串,字符个数,坐标X,坐标X,文本筐宽度,文本筐高度,字颜色,混合模式)
	#define GN_RunTextOut(text,count,x,y,w,h,color,blendmode) GN::RunRec.OutText(text,count,x,y,w,h,color,blendmode)
    //在图形区任意绘制图框(坐标X,坐标X,筐宽度,筐高度, 颜色)
	#define GN_RunDrawFrame(x, y, w, h, color) GN::RunRec.DrawFrame(x, y, w, h, color)
	
	//显示实时变量值(变量输入,显示名称,排列顺序ID）
	#define GN_RunValueCheck(value,name,ID_order) GN::RunRec.ValueCheck(value,name,ID_order)
    //显示实时变量值(变量输入,排列顺序ID）!GN_RunValueCheck 的简化版
	#define GN_RunValueCheck2(value, ID_order) GN::RunRec.ValueCheck(value,#value,ID_order)
	
    //定义一个键盘按紧键控制变量 一并显示它当前值在屏幕上( 变量类型 , 变量名 , 初始值 , 控制增减量 , 增量控制键控码 , 减量控制键控码 , 屏幕显示排列顺序ID ) 
	#define GN_RunKCV( type , vname , vdef , vadd , KADD , KSUB , ID_order ) \
        static type vname = vdef;\
		if     (GN::Key[KSUB])vname -= vadd;\
		else if(GN::Key[KADD])vname += vadd;\
        GN_RunValueCheck2( vname , ID_order )

    //定义一个键盘一次键控制变量 一并显示它当前值在屏幕上( 变量类型 , 变量名 , 初始值 , 控制增减量 , 增量控制键控码 , 减量控制键控码 , 屏幕显示排列顺序ID ) 
	#define GN_RunKCV1( type , vname , vdef , vadd , KADD , KSUB , ID_order ) \
        static type vname = vdef;\
		if     (GN::Key1[KSUB])vname -= vadd;\
		else if(GN::Key1[KADD])vname += vadd;\
        GN_RunValueCheck2( vname , ID_order )

    //定义一个键盘控制开光变量 一并显示它当前值在屏幕上( 变量名 , 控制键控码 , 屏幕显示排列顺序ID ) 
	#define GN_RunKCOF( vname , KCODE , ID_order ) \
        static bool vname = false;\
		if(GN::Key1[KCODE])vname = !vname;\
        GN_RunValueCheck2( vname , ID_order )

    //读取进度绘制(进度:0.0f-1.0f,进度条式样,是否重绘图标字样,心中位置X,Y,LOGO图标颜色,LOADING文字颜色,进度筐颜色,进度条颜色,混合模式) !参数进度条式样:0不明确方式LOADING 1明确方式LOADING 2不明确方式EXECING 3明确方式EXECING
	#define GN_RunProgressDraw(progress,style,re_draw,cx,cy,icon_color,text_color,frame_color,bar_color,blendmode) GN::RunRec.ProgressDraw(progress,style,re_draw,cx,cy,icon_color,text_color,frame_color,bar_color,blendmode)  
    //读取进度的显示大小获得(宽度输出,高度输出)
	#define GN_RunProgressSizeGet(out_w,out_h) GN::RunRec.ProgressSizeGet(out_w,out_h)
};
namespace GN {
	extern GN_API GT_cRUN_REC RunRec; //系统运行信息输出测试用
};
#endif //endif _GN_RUNINFO_H_
