#pragma once
#ifndef _GN_2DSCENE_H_
#define _GN_2DSCENE_H_
namespace GNg{
/********************************************************** 2D场景对象 *****************************************************************************************************************************/
enum eOBJ_TYPE{					//2D对象类型
	 eOBJ_GROUP,					//群组
	 eOBJ_IMAGE,					//静态图像
	 eOBJ_LENS_FLARE,			    //镜头光环
	 eOBJ_FRAME_RC,					//正规动画图筐
	 eOBJ_FRAME,					//动画图筐
	 eOBJ_FRAME_LAUNCHER,			//图筐发射器
	 eOBJ_FRAME_LAUNCHER2D,			//复合发射器
	 eOBJ_FRAME_LAUNCHER_FONT,		//字筐发射器
	 eOBJ_TYPE_NUM					//2D对象类型数
};
extern GN_API const char *ObjTypeNames[eOBJ_TYPE_NUM];    //2D中文对象类型名 一一对应以上各个类型

class cOBJ2D;
class cOBJPTR;
//便捷拉出2D对象指针( 对象类型 , 2D对象 )
#define CP( CLASS , OBJ ) ((CLASS*)(OBJ).aObj)

//便捷拉出2D对象地址里的对象指针( 对象类型 , 2D对象地址 )
#define CPP( CLASS , PTR ) ((CLASS*)((PTR).aPtr->aObj))

class GN_API iOBJ2D_EXT   //★★★★★ 2D对象扩展功能基类  可继承该类来扩展2D对象的功能
{public:
	static P<iOBJ2D_EXT> (*aOnCreate)(unsigned int _type); //当要创建该类的继承类对象时的处理(告诉您扩展类型)要求返回对应类型的继承对象 !把该函数指针指向一个能创建派生自定义2D扩展的方法 便可以创建更多的自定义2D扩展

    cOBJ2D				 *obj;			  //扩展所属2D对象
    unsigned short        type;			  //扩展类型 用来标识新继承的类用

	inline iOBJ2D_EXT():obj(0),type(0){}
   public:
	//当该对象被删时怎样对自定属性进行释放处理  !这个~iOBJ2D_EXT 要改为自定义类名如: MY_c2DOBJ
	virtual ~iOBJ2D_EXT(){}

	//当要复制或创建对象数据副本时怎样对自定属性赋值给新复制出来的对象() 要求返回新复制出来的扩展对象安全指针
	virtual P<GNg::iOBJ2D_EXT> OnCloneOut(){
		//示例:
		P<iOBJ2D_EXT> new_clone(1); //!这里的 iOBJ2D_EXT 要改为自定义类名如: MY_c2DOBJ

		//...在这里加上自定以属性的传值代码
		//new_clone->my_attrb = my_attrb;

		return new_clone;
	}
	//当要把内部数据储存为一整段数据时的处理(返回整段数据 首个字节为版本号之后4个字节为type 当该值为0时仅返回数据段需要大小)返回数据段大小字节 !用户扩展的版本号应由1开始
	virtual unsigned int OnSave(uint8 *data_out=0){ if(data_out)*data_out = 0; return 1; }

	//当要从文件数据段里读入数据时的处理(数据段地址 首个字节为版本号之后4个字节为type)返回已读取字节数 !用户扩展的版本号应由1开始
	virtual unsigned int OnLoad(const uint8 *data){ return 1; }
};
class GN_API cOBJ2D		  //★★★2D对象
{public:
	int8                  aOBJtype;		   //对象类型 参考 GNg::eOBJ_TYPE 定义
	void				 *aObj;			   //具体的对象内容
	P<iOBJ2D_EXT>         aUsExt;          //用户扩展属性 绑定该属性来扩展对象功能 调用:cOBJ2D::BindExt(...) 来绑定

	cOBJ2D(int8 obj_type=-1, void *_obj=0):aOBJtype(obj_type),aObj(_obj),aUsExt(1){}
   ~cOBJ2D(){ this->Delete(); }
	cOBJ2D& operator =(const cOBJ2D &src);

	void Delete();	//删除对象数据

	//复制内部数据(复制源, 是否一并复制里面对象的当前状态 否则重置为初始状态)
	void CopyData(const cOBJ2D &src, bool cpy_state);

	//更新播放(更新时间)返回当前播放到的时间 播放完结束返回0
	unsigned int UpdatePlay(unsigned int updatetime);

	//自动管控动画入场(是 否侧出场, 动画过程完成后的处理, 传给回调用的参数)返回动画过程总时间
	unsigned int AnimationIn(bool yes, void (*end_proc)(void*)=0, void *usarg=0);

	//设置混合模式(混合模式)
	void SetBlendMode(unsigned char blend);

	//设置颜色(颜色)
	void SetColor(unsigned int color);

	//混合指定颜色到对象颜色(指定颜色, 自定义混合方法(传回指定颜色, 对象颜色))  !混合方法也可以是 AddColor 以下的几个函数
	void ColorBlend(unsigned int color, unsigned int (*func)(unsigned int _color, unsigned int obj_color));

	//绑定对象扩展属性(对象扩展) !要扩展2D对象的功能要先定义一个iOBJ2D_EXT的继承类 用该新类定义自己的扩展属性和相关控制方法
	void BindExt(P<iOBJ2D_EXT> ext){ ext->obj = this; aUsExt = ext; }

	//使对象移动(X移动量, Y移动量)
	void ToMove(float x, float y);

	//获取当前对象和它所包含子对象的总体范围判定盒(返回范围判定盒)
	void GetWorldBox(GNg::sRECT2 *box_out);

	//使范围判定盒扩大以能包含该对象和它所包含的子对象(范围判定盒)
	void ExpandWorldBox(GNg::sRECT2 *box_out);

	//接入其它对象或群组(其它对象或群组, 如有指定返回接入后的对象地址) !对象包含的子对象 图片等资源也一并牵连进来 !只有接入到调用UpdatePlay的场景中才作显示 相对来说它才生存于这个场景世界中 连接应尽量避免连接该对象之上的任何父对象以免引起混乱
	void Comment(cOBJ2D &obj2d, cOBJPTR *ptr_out=0);

	//在群组里查找指定具体内容的2D对象( 具体对象内容 ) 返回它在群组里的对象地址
	cOBJPTR Search(const void *_obj);

	unsigned int   GetIncludeTypeObjNum(int8 obj_type);				   //获取对象所包含指定类型的对象数(对象类型)返回包含的对象数 !如果对象是指定类型也包含它在内
	unsigned int   GetIncludeTypeObjs(cOBJ2D *mds_out, int8 obj_type); //获取对象所包含指定类型的所有对象(装填对象的数组 需预先分配好足够的空间, 对象类型)返回包含的对象数 !空间数量可由 cOBJ2D::GetIncludeTypeObjNum 获得 !如果对象是指定类型也包含它在内
	P< cOBJ2D >	   GetIncludeTypeObjs(int8 obj_type);				   //获取对象所包含指定类型的所有对象(对象类型) 返回包含的所有对象 !如果对象是指定类型也包含它在内 如果没有指定类型对象返回空的
};
class GN_API cOBJPTR //★★★ 2D对象地址 用于控制对象的层级关系
{  public:		 
	cOBJ2D				 *zParent;		   //父层组指针
	
	cOBJ2D				 *aPtr;			   //指向它所在父层组链表里的地址

	cOBJPTR():aPtr(0),zParent(0){}

	void UnComment(cOBJ2D *obj=0);						//解除对象与父层组的连接(如有指定 返回该对象) !解除后对象地址便会失效
	void InsertFront(cOBJ2D &obj, cOBJPTR *ptr_out=0);	//插入其它对象或群组在它前面(其它对象或群组, 如有指定返回插入后的对象地址) !对象包含的子对象 图片等资源也一并牵连进来 其它注意项同上
	void InsertBack (cOBJ2D &obj, cOBJPTR *ptr_out=0);	//插入其它对象或群组在它后面(其它对象或群组, 如有指定返回插入后的对象地址) !对象包含的子对象 图片等资源也一并牵连进来 其它注意项同上
	int  OrderMove(int fn);								//移动图层顺序(向前移动层数 负值就向后移动)返回实际移动了的层数 当移到最前或最后时会提前返回

	cOBJ2D  *GetParent(){ return zParent; } //获得对象的父层组指针
	cOBJPTR  Clone();						//复制出该2D对象的副本到一个新地址里
};
class GN_API cSCENE2D :public cOBJ2D  //★★★ 2D场景 游戏可以定义多个场景 不过通常同一时间使用两个就够了 一个处理2D 一个作界面显示或文件流加载
{  public:
	static cSCENE2D      *zCurProcScene;   //当前正在显示处理的2D场景
	P<char*>			  zObjNameTable;   //场景里所有对象名的查找表 里面每个指针指向对象名缓冲里名称的开始位置 存放顺序与递归检索场景树的所有对象访问顺序一致 通常用于找到指定对象的指针用
	char				 *zObjNameBuf;	   //场景里所有对象名的存放缓冲 中间以\0结束符隔开 如你直接访问只访问到首个名
	
	GN::sARRAY_VEC<cLENS_FLARE_OBJ*> zProcLensFlares; //待显示的镜头光环数组
   public:
	cSCENE2D();
   ~cSCENE2D();

	//在场景里查找指定名的2D对象( 对象名 ) 返回在场景里的对象地址 !这个必须在场景刚加载后还没插入或删除过里面的对象前调用
	cOBJPTR Search(const char *name);

	//初始化加载2D场景文件(文件名 仅支持g2d专属文件, 读取进度输出) 返回是否成功
	bool InitLoadToData(const char *file_name, float &progress);

	//保存2D场景内容到文件(文件名 仅支持g2d专属文件, 文件版本, 是否导出图片文件, 如有指定则仅保存该场景或群组节点下的内容) 返回是否成功
	bool SaveToFile(const char *filename, uint8 version=0, bool exp_img=false, cOBJ2D *node=0);

	//更新播放场景(更新时间)返回当前播放到的时间 播放完结束返回0
	unsigned int UpdatePlay(unsigned int updatetime);
};
#define GNg_SAVE_G2D_VERSION  0	//当前最新2D专用文件版本

};  //end GNg namespace
#endif //_GN_2DSCENE_H_