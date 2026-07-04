#ifndef _GN_UITEXTIN_H_
#define _GN_UITEXTIN_H_

struct GT_sINPUT_SAVE     //文本输入操作记录
{
	char				 *str;			//更改内容
	unsigned short		  bid;			//更改点 在文本缓冲里的第几个字节
	unsigned short		  strNum;		//更改内容字数
	bool				  isAdd;		//是增加还是减

	inline GT_sINPUT_SAVE():str(0),strNum(0){}
    inline ~GT_sINPUT_SAVE(){ GN_DeleteArray(str); }

    inline bool operator == ( const GT_sINPUT_SAVE &is) const{ return this == &is; }
	inline bool operator <  ( const GT_sINPUT_SAVE &is) const{ return this  < &is; }
};
namespace GNu{

enum eTX_IN_TYPE {      //文本输入类型
	 eTX_IN_TEXT,         //文本输入
	 eTX_IN_PASSWORD,     //密码输入
	 eTX_IN_FLOAT,        //浮点小数输入
	 eTX_IN_INT,		  //有负号整数输入
	 eTX_IN_UINT,         //无负号整数输入
	 eTX_IN_HEX,		  //无负号16进制整数输入
     eTX_IN_TEXT_LOCK     //文本输入锁定文本筐内容使不能滚动
};
class GN_API cTEXT_INPUT  // ★★文本输入筐
{  protected:
	//增加一条记录(是否插入字符, 触发点文本缓冲位置, 记录字符数, 记录内容, 触发点跳转位置)
	void zAddSave(bool is_add,unsigned short _bid,unsigned short strcount,char *_str,unsigned short seekbid);
	
	void zSetBid(unsigned short bufid, bool save_lastxy=true);    //设置输入点(落在缓冲区的编号位置, 是否记下刚才焦点的局部坐标)这个内部用 性能高点但不作太多安全检查
	void zArrangeText();                                          //整理文本显示内容
	void zDelText(unsigned short bufid1,unsigned short bufid2);   //删除文本段(缓冲位置编号1, 至缓冲位置编号2)的区间
	
	void zBackSpaceDelTextProc();								  //退格符删除处理
	void zBackSpaceDelTextNumberProc();							  //数字的退格符删除处理
	bool zFuntionKeyProc(char key);								  //其他功能键的处理(功能键码)返回输入键码是否功能键
	void zOffsetBidForErectPrint(bool is_enter, bool is_back);	  //为竖方向输出偏移一下位置(是否有经过换行, 是否为退一位偏移操作)	
	void zOffsetCtrlNextForErectPrint(bool is_enter);			  //竖方向输出的下一位偏移控制(是否有经过换行)
	void zOffsetCtrlNext(bool is_enter, short ead, short ad);	  //下一位偏移控制(是否有经过换行, 有经过换行时偏移增量, 偏移增量)
	void zOffsetCtrlNext4(bool is_enter);						  //四种方向输出的下一位偏移控制(是否有经过换行)
	void zOffsetCtrlBackForErectPrint(bool is_enter);			  //竖方向输出的退一位偏移控制(是否有经过换行)
	void zOffsetCtrlBack(bool is_enter, short ead, short ad);	  //退一位偏移控制(是否有经过换行, 有经过换行时偏移增量, 偏移增量)
	void zOffsetCtrlBack4(bool is_enter);						  //四种方向输出的退一位偏移控制(是否有经过换行)
	void zOffsetCtrlMouseSelectErectPrint(short mx, short my);	  //竖方向输出的鼠标选择偏移控制(鼠标位置X, 位置Y)
	void zOffsetCtrlMouseSelect			 (short mx, short my);	  //鼠标选择偏移控制(鼠标位置X, 位置Y)

	void zCheckAddSave();										  //检测可否并增加一条记录
	void zEraseSaveNode();										  //剪断撤消位置之前的记录

	uint8* zDataSave(uint8 *wpos, GN::sLIST<GNg::sIMAGE*> &img_collect); //保存该对象的2进制数据(当前写入位置, 收集到的字图式样列表)返回最后的写完位置
    uint8* zDataLoad(uint8 *rpos, P<GNg::sIMAGE*>		  &img_refs);	 //读取该对象的2进制数据(当前读取位置, 字图式样引用组)返回最后的读完位置

	char                 *zBuf;              //文本内容缓冲
    unsigned short        zBid;              //当前文本缓冲处理位置
	unsigned short        zBid2;             //当拖动鼠标选择了范围段的另一个文本缓冲处理位置
    unsigned short        zEid;              //当前输入到的最尾插入符位置
    unsigned short        zVid;              //当前输入焦点落在第几个可视字符里
	unsigned char         zVrow_id;          //当前输入焦点对应的可视字符里在屏幕显示为第几行

	short                 zDlgx;             //文本筐位置X
	short                 zDlgy;             //文本筐位置Y
	short                 zDlgw;             //文本筐宽
	short                 zDlgh;             //文本筐高
    short                 zOffsetx;          //文本筐内偏移量X
    short                 zOffsety;          //文本筐内偏移量Y
    short                 zOffsetxtg;        //文本筐内偏移量目标X
    short                 zOffsetytg;        //文本筐内偏移量目标Y
	short                 zOffsetxad;        //文本筐内偏移递增量X
    short                 zOffsetyad;        //文本筐内偏移递增量Y
	short                 zOffsetLastx;      //刚才焦点在文本筐内的X局部坐标偏移量
    short                 zOffsetLasty;      //刚才焦点在文本筐内的Y局部坐标偏移量
	unsigned char         zOffsetN1;         //下一段的偏移阻力 1至16 值越大筐内容偏移越少
	unsigned char         zOffsetN2;         //下一行的偏移阻力 1至16 值越大筐内容偏移越少
	float                 zOffsetSheep;      //文本筐偏移速度 0.05至1.0
	
	GNg::cVIEW2D		 *zShowView;         //显示文本筐用的视口
	GNg::cTEXT_ART		  zTextOut;          //文字输出器

	bool                  zImeIn;            //输入是否激活
	bool                  zMousePass;        //虚拟一个鼠标点击
	bool                  zArgText;          //当前帧是否需要重排列文本
	bool                  zShowReady;        //标记是否准备好可运行了
	bool                  zLastMK;           //标记刚才鼠标是否按下
    bool                  zReOffsetLast;     //是否正在恢复上次记下的偏移坐标
	bool                  zMoveTextFrame;    //文本筐是否可移动
	bool                  zKeyOnDown;        //标记是否在筐内有鼠右键按紧
						
	short                 zVmx;              //虚拟鼠标判断位置X
	short                 zVmy;              //虚拟鼠标判断位置Y
	unsigned short        zMaxInNum;         //最大可输入数
					
	int                   zfNowtime;         //焦点闪动现在时间
	bool                  zfAm_s_e;          //焦点来回状态
	char                  zIn_;              //输入过滤临时用
	bool                  zInputSaveOn;      //输入操作步骤记录是否开启 在撤消时的摸以输入操作将不开启动它
	bool                  zCancelIng;        //当前是否在撤消到记录当中
	eTX_IN_TYPE           zInType;           //输入类型
	unsigned short        zSaveStartBid;	 //跟踪要记录的触发点的位置
	unsigned short        zLastEndBid;		 //跟踪最近结束输入的位置 用以判别是否有跳到其他位置来输入
	
	GN::sLIST<GT_sINPUT_SAVE>::iterator zIt; //当前的记录指针
	GN::sLIST<GT_sINPUT_SAVE> zSaveList;     //撤消记录列表
	
	P<GNg::sIMAGE>		  zRetArrow;         //返回主要显示模式的按钮图标

	static unsigned char  zBackAlpha;		 //底色不透明度

	static cTEXT_INPUT   *zCurIn;			 //当前正在操作的文本输入对象 
   public:
   #ifndef GN_USING_ADDITIONAL_LIB
	bool                  aTabKeyHandle;     // Tab键处理:true - 内部处理;false - 外部处理
   #endif
    static unsigned short aMaxCancelNum;     //最大撤消数 记录超过这个数时将忽略
	unsigned int          aTextColor;        //文本颜色
	unsigned int          aFocusColor;       //焦点颜色
	unsigned short        aTextBlend;        //文本混合模式
	unsigned short        aFocusBlend;       //焦点混合模式
	bool                  aLock;             //是否锁定它不能被修改
	bool                  aDisableControl;   //是否禁用控制操作 如果是就只能显示不能对它操作

    cTEXT_INPUT();
   ~cTEXT_INPUT();

    //释放内部资源
    void Release();   

	//初始化显示式样
    void InitStyle(
	  const GNg::cTEXT_ART *text_style  = 0,            //文字式样
			          short x           = NearCx(-100), //文本筐位置X
					  short y           = NearCy(-30),  //文本筐位置Y
					  short w           = 200,          //文本筐宽
					  short h           = 60 ,          //文本筐高
		     unsigned int   text_color  = 0,            //文本颜色
			 unsigned int   focus_color = 0xffff3434,   //焦点颜色
			 unsigned short text_blend  = 1,            //文本混合模式
			 unsigned short focus_blend = 2,            //焦点混合模式
             P<GNg::sIMAGE> ret_arrow   = P_()          //在主要显示模式下的返回图标式样
		 );

	//初始化设置输入格式(接受输入的类型, 默认值, 最大可输入数) !该类在初始化后 最好不要在运行中途动态改变排列格式这样可能会带来混乱
	void  InitInputFmtToData(eTX_IN_TYPE type = eTX_IN_TEXT, const char *default_v=0, unsigned short max_input_num=65535);

    //初始化加载文本文件(文件名) 返回是否成功 !该函数和上个函数一般不会混用 都是二选一的
    bool  InitLoadToData(const char *file_name);

	//设置筐内容偏移滑动效果(偏移倍速:0.05f至1.0f, 下一段偏移阻力:1至16, 下一行偏移阻力:1至16)
	void  SetOffsetEF(float sheep = 0.1f, unsigned char n1 = 2, unsigned char n2 = 4);

	//更新运行文本输入筐(帧间隔时间) 这个要在每帧都行的地方挂起
	void  UpdateRun(unsigned int updatetime);

    //设为突出主要显示(横向靠位0.0f至1.0f, 纵向靠位0.0f至1.0f,横向是否参考输入焦点, 纵向是否参考输入焦点)
	void  SetPrimary(float nearU, float nearV, bool refer_focusU=false,bool refer_focusV=false);

	//取消突出主要显示()
    void  SetPrimaryCancel();

	//设置文本缓冲输入点并一起移动显示焦点(文本内指定的字节位置 值65535:为移动到末尾 0:为移动到开头)
	void  SetPointer(unsigned short buf_id=0);

	//输入文本(文本, 输入长度)
	void  InputText(const char *text, unsigned short len=0);

	//获取输入的数据(是否仅获取选中的内容)返回数据 这会帮你创建一份数据拷贝 用完记得 delete [] data 如果想取得数字数据 用atoi atof atol 转换返回的数据指针
	char *GetTextData(bool select_only=false);

	//设置显示位置与大小(左上角位置X,左上角位置Y,文本筐宽, 文本筐高)
	void  SetPos(short x, short y, short w, short h);
	
	//获得显示位置与大小(左上角X输出, 左上角Y输出, 文本筐宽输出, 文本筐高输出)
	inline void GetPos(short *x ,short *y, short *w, short *h){  *x = zDlgx, *y = zDlgy, *w = zDlgw, *h = zDlgh; }
	
	//获得文本在筐内的偏移拖动量(偏移X输出, 偏移Y输出)
	inline void GetInternalOffset(short *ox, short *oy){ *ox = zOffsetx; *oy = zOffsety; }

	inline GNg::cTEXT_ART *const GetTextStyle(){ return &zTextOut; }			   //获得文本输出式样
	inline GNg::sARRANGE_FMT const* GetFontFormat(){ return &(zTextOut.zParame); } //获得文本显示的排列格式信息
	inline unsigned char GetFontMapFormat(){ return zTextOut.zFontMapFormat; }     //获得文本显示用的字库图格式
	inline eTX_IN_TYPE   GetInputType(){ return zInType; }						   //获得文本输入类型
	inline bool			 IsInputing(){ return zImeIn; }							   //返回当前是否正在输入中
	bool				 IsMouseFocus();										   //返回鼠标是否指到该对象
	
   #ifndef GN_USING_ADDITIONAL_LIB //GN_ADD_CONTEN_PRIVATE
	void SetFocus(void);									//设置焦点
	void SetText(const char *text,unsigned short len=0);
   #endif
	void CopyData(const cTEXT_INPUT &src);					//复制数据(复制源)
};
class GN_API cUI_TEXT_INPUT :public cUI_NEAR_OBJECT, public cTEXT_INPUT  //★输入筐
{public:
	cUI_TEXT_INPUT(){ aType = eUI_TEXT_IN; }

	virtual P<iUI_OBJECT> Clone();										 //复制出该UI组件的一个新副本
	virtual void UpdateRun(unsigned int updatetime, sUI_LEVEL *parent);  //更新运行(帧间隔时间, 父层级信息链)

	//把内部数据储存为一整段数据时的处理(收集共享对象集, 返回整段数据 首个字节为版本号之后4个字节为aType 当该值为0时仅返回数据段需要大小)返回数据段大小字节
	virtual unsigned int DataSave(cSHARE_DATA *share_objs, uint8 *data_out=0);

	//从文件数据段里读入数据时的处理(引用共享对象集, 数据段地址 首个字节为版本号之后4个字节为aType)返回已读取字节数 失败返回0 
	virtual unsigned int DataLoad(cSHARE_DATA *share_objs, const uint8 *data);
};
};
#endif //_GN_UITEXTIN_H_