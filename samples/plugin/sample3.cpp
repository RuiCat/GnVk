#include<GN.h> //使用GN必需包含GN.h头文件
#include<Player/GN_EdPlugin.h>	   //要导出参数设置菜单给编辑器打开 需包含编辑器插件接口头文件
#define e2D_PHYSICAL_TYPE 65534    //体能类型标记

struct sPHYSICAL			//体能结构
{
	int						aHP;			//体力
	float					aST;			//力气
};
namespace GNg{
class c2D_PHYSICAL : public iOBJ2D_EXT, public sPHYSICAL //体能扩展属性 !!继承的2D对象扩展必须放在前
{public:
	c2D_PHYSICAL(){ aHP = 100; aST = 5.0f; type = e2D_PHYSICAL_TYPE; }	//初始化

	//重写以下这几个方法以对视频显示和数据保存与加载
	virtual P<GNg::iOBJ2D_EXT> OnCloneOut(){ //当要复制或创建对象数据副本时的处理
		P<c2D_PHYSICAL> new_clone(1);		//创建新类的对象
		new_clone->aHP = aHP;				//传递体力属性
		new_clone->aST = aST;				//传递力气属性
		return new_clone;
	}
	virtual unsigned int OnSave(uint8 *data_out=0);  //当要把内部数据储存为一整段数据时的处理
	virtual unsigned int OnLoad(const uint8 *data);  //当要从文件数据段里读入数据时的处理
};
unsigned int c2D_PHYSICAL::OnSave(uint8 *data_out){
	uint8 loc_buf[64];						//定义足够大的局部缓冲
	if(!data_out)data_out = loc_buf;		//如传入的参数为0即仅统计所需大小 为一份代码通吃 这里就使它指向局部缓冲来装填不用的数据
	uint8 *wpos = data_out;					//写入起始位值
	*wpos = 1; ++wpos;						//写入版本 并使写入位置后移1字节
	*((unsigned int*)wpos) = type;	wpos += 4; //写入类型标记 并使写入位置后移4字节
	*((int  *)wpos) = aHP;	 wpos += 4;		   //写入体力 并使写入位置后移4字节
	*((float*)wpos) = aST;   wpos += 4;		   //写入力气 并使写入位置后移4字节
	return wpos - data_out;					   //返回 当前写入位置减写起始位值就是数据段占空间大小
}
unsigned int c2D_PHYSICAL::OnLoad(const uint8 *data){
	uint8 *rpos = (uint8*)data;				//读起始位值
	uint8 version = *rpos; ++rpos;  //读取版本 并使读入位置后移1字节
	type = *((unsigned int*)rpos);	rpos += 4; //读入类型标记 并使读入位置后移4字节
	if(version > 1){
		GN_Log("读取到的体能扩展属性版本:%d 大于当前相关插件版本1 ! 未能识别新版本数据格式! 读取该扩展属性失败\n", (int)version);
		return 0; //返回失败标记
	}
	aHP = *(  (int*)rpos);	rpos += 4;		//读入体力 并使读入位置后移4字节
	aST = *((float*)rpos);	rpos += 4;		//读入力气 并使读入位置后移4字节
	return rpos - data;						//返回 已读字节数
}
};
#if 1 /* 导出为引擎插件 ******************************************************************************************************************************************************************************************************************************/
P<GNg::iOBJ2D_EXT> (*Last2DExtCreateFunc)(unsigned int); //用于备份之前2D扩展属性创建的函数

//本例子新增的2D扩展创建方法(扩展类型)
P<GNg::iOBJ2D_EXT> Sample2DExtCreate(unsigned int _type){
	if(_type == e2D_PHYSICAL_TYPE)return P<GNg::c2D_PHYSICAL>(1);  //当扩展类型是体能类型时就创建体能扩展属性并返回它的安全指针
	else return Last2DExtCreateFunc(_type);						   //其它类型就调用之前的创建方法
}
//连接本例子回调给引擎的2D扩展默认创建方法
void SampleCallBackComment(){
	Last2DExtCreateFunc	= GNg::iOBJ2D_EXT::aOnCreate;  //备份之前的创建函数
	GNg::iOBJ2D_EXT::aOnCreate = Sample2DExtCreate;	   //使默认创建方法指向本例子的回调函数 以便优先回调到本例的处理方法
}
#ifdef _GN_ED_PLUGIN_H_  /* 增加导出编辑器回调选单 *************************************************************************************************************************************************************************************/
#define MULTI_OBJ_ADJUST						   //支持多对象一起调整
P<sPHYSICAL> PhysicalBk;						   //体能属性复制副本

class cPHYSICAL_MENU :public GNu::iSETTING_MENU, public sPHYSICAL  //体能设置选单
{  public:
   #ifdef MULTI_OBJ_ADJUST
	P< P_ >					other_objects;				//记录其它附带选到的对象  如您无需对多个选定对象一起进行参数调整 可以忽略这
   #endif
	virtual void Open();								//打开菜单时的处理
	virtual void Change();								//菜单值更改时的处理
	virtual void ParameCopy(GNu::eCOPY_OPT op);			//参数复制时的处理(复制副本操作方式)

	//获取复制栏显示状态(返回预览小图标色调, 返回是否显示粘贴图标, 返回是否需要显示粘贴实列图标, 返回首个复制属性起始ID 返回0xff不显示复制栏, 返回最末复制属性的ID)返回预览小图标 返回0用纯色显示
	virtual GNg::sIMAGE *GetCopyBarState(unsigned int &icon_color, bool &show_paste, bool &show_paste_inst, uint8 &first, uint8 &end){
		show_paste = PhysicalBk?true:false; show_paste_inst = false; first = 0; end = 1;
		return 0;
	}
};
void cPHYSICAL_MENU::Open(){
	sPHYSICAL *_ph = (sPHYSICAL*)((GNg::c2D_PHYSICAL*)set_obj);
   #ifdef MULTI_OBJ_ADJUST
	*((sPHYSICAL*)this) = *(_ph);		//备份原属性值
   #endif
	InitValueToData(&(_ph->aHP),  " 体力           :  ", 100, 0, 1);			//插入体力修改项
	InitValueToData(&(_ph->aST),  " 力气           :  ", 30.0f, 0.0f, -0.25f);	//插入力气修改项 !每次增加量 值为负时表示每次增加量是当前值乘该值的量
}
void cPHYSICAL_MENU::Change(){
   #ifdef MULTI_OBJ_ADJUST
	sPHYSICAL *_ph = (sPHYSICAL*)((GNg::c2D_PHYSICAL*)set_obj);  //临时拿出2D扩展里的体能结构裸指针以加快访问速度
	GNg::c2D_PHYSICAL *_ph2;
	unsigned int i;
	if(aHP != _ph->aHP){	//体力有更变了
		aHP = _ph->aHP;		//再次备份体力
		for(i = 0; i < other_objects.Num(); ++i){
			_ph2 = CVP(GNg::c2D_PHYSICAL)other_objects[i];		 //强制转换无类型安全指针为体能扩展属性指针
			if(_ph2->type == e2D_PHYSICAL_TYPE)_ph2->aHP = aHP;  //当其它选到对象扩展类型也是体能类型时就把体力一起设置给它
	}	}
	if(aST != _ph->aST){	//力气有更变了
		aST = _ph->aST;		//再次备份力气
		for(i = 0; i < other_objects.Num(); ++i){
			_ph2 = CVP(GNg::c2D_PHYSICAL)other_objects[i];		 //强制转换无类型安全指针为体能扩展属性指针
			if(_ph2->type == e2D_PHYSICAL_TYPE)_ph2->aST = aST;  //当其它选到对象扩展类型也是体能类型时就把力气一起设置给它
	}	}
   #endif
}
void cPHYSICAL_MENU::ParameCopy(GNu::eCOPY_OPT op){
	if(op){
		if(op == GNu::eCOPY_COPY){ //当提示复制时
			PhysicalBk.New(1);											//分配副本数据空间
		   *PhysicalBk = *((sPHYSICAL*)((GNg::c2D_PHYSICAL*)set_obj));  //复制体能结构属性到副本
		}else{					   //其它情况就是粘贴了
			*((sPHYSICAL*)((GNg::c2D_PHYSICAL*)set_obj)) = *PhysicalBk; //复制回体能结构属性到对象扩展
		   #ifdef MULTI_OBJ_ADJUST
			Change();													//值更改了执行一下 Change 以便通知其它附带选到的对象扩展也一起更变
		   #endif
		}
	}else PhysicalBk.Delete(); //当提示要删除副本就删除它
}
void (*LastSetMenuInsertFunc)(GP_eSETTING_CLASS, void *, P< P_ > &, GNu::sSIM_MENU*);  //备份原菜单插入函数用的函数指针

//本例子新增的2D扩展属性设置菜单插入方法(设置对象的类型, 设置对象, 其它一起选到的对象, 父菜单)
void SampleSetMenuInsert(GP_eSETTING_CLASS set_class, void *_set_obj, P< P_ > &other_objs, GNu::sSIM_MENU *pmenu){
	if(set_class == GP_eSETTING_OBJ2D && ((GNg::c2D_PHYSICAL*)_set_obj)->type == e2D_PHYSICAL_TYPE){ //仅关心本例子的2D扩展类型
		cPHYSICAL_MENU *phy_menu = new cPHYSICAL_MENU;	 //创建体能设置选单
		phy_menu->set_obj = _set_obj;						//为选单指定将要设置的2D对象扩展 !当设置类型为 GP_eSETTING_OBJ2D 时 _set_obj 和 other_objs里指向的对象都为2D对象扩展 iOBJ2D_EXT 的派生类
	   #ifdef MULTI_OBJ_ADJUST
		phy_menu->other_objects = other_objs;				//为选单指定将要设置的其它一起选到的2D对象扩展
	   #endif
		pmenu->InitSubMenuToData(phy_menu, "	 体能", 3); //插入体能设置选单到编辑器里的2D对象主选单里 !该选单插入后就会被编辑器接管 您无需再手动销毁它 !!注意选单名首字符必须为'Tab'而不是空格
	}
	LastSetMenuInsertFunc(set_class, _set_obj, other_objs, pmenu);  //继续执行之前的其它设置菜单插入方法
}
extern "C"{
//获得插件支持的2D扩展类型信息并添加到相应链表里(装填2D扩展类型名的链表, 装填2D扩展类型的链表)
GN_CALL void GetSupportTypes(GN::sLIST<char*> &uobj_type_names, GN::sLIST<unsigned int> &uobj_types){
	LastSetMenuInsertFunc = GP_SettingMenuInsert;  //备份之前插入参数设置菜单的函数
	GP_SettingMenuInsert = SampleSetMenuInsert;	   //使插入参数设置菜单回调函数指针指向本例子的相关回调函数 以便优先回调到本例的处理方法

	uobj_type_names.push_back("体能");			   //加入支持的2D扩展名称
	uobj_types.push_back(e2D_PHYSICAL_TYPE);	   //加入支持的2D扩展类型	  !若要与其他2D扩展插件兼容 您最好通过登陆引擎官网申请到一个全网唯一的值
} }
#endif /********************************************************************************************************************************************************************************************************************************/
extern "C"{
//必须导出一个版本函数给引擎调用 并返回接口版本的宏  !之后把编译好的插件动态库和它依赖到的资源放到一个与插件动态库同名的文件夹下 并将该文件夹放到引擎目录 Plugins/2DObject/ 下就可以被编辑器加载到 但您用SDK开发的应用需要手动调用 GN_PluginsLoad 系列函数来选择加载那些插件
GN_CALL char*const Version(){  
	SampleCallBackComment();		//连接本例子的2D扩展创建方法给引擎
	return GN_INTERFACE_VERSION;	//返回接口版本
}
#else /* 插件使用例子 *******************************************************************************************************************************************************************************************************************************/
P<GNg::cSCENE2D>   Scene2D(1);		//定义一个2D场景 以便来显示里面的内容
GNg::c2D_PHYSICAL *ObjPhys;			//指向2D对象扩展的指针
extern "C"{
GN_CALL void AppInit(){     //▲初始化应用必须数据 程序开始准备好引擎系统后会调用它一次 
	GNg::sIMAGE_OBJ *image = new GNg::sIMAGE_OBJ;		//创建一个图像对象
	image->img.New(1);									//为图像对象里的图像分配内存空间
	image->img->InitToData("samples/base/image/combo");  //为图像加载图片
	image->bmode = GNg::eBLEND_NORMAL;					//指定图像对象混合模式为一般效果
	image->Init(120, 80, 0, 0, 0xffffffff);				//指定图像对象显示位置坐标 原大小 原色来显示
	GNg::cOBJ2D img_obj(GNg::eOBJ_IMAGE, image);		//把图像对啊封包成2D对象
	img_obj.BindExt(P<GNg::c2D_PHYSICAL>(1));			//为2D对象绑定体能扩展属性
	ObjPhys = CVP(GNg::c2D_PHYSICAL)img_obj.aUsExt;		//拿到体能扩展的裸指针以便之后更方便地修改它的值
	GN::sLIST<GNg::cOBJ2D> *sub_objs = new GN::sLIST<GNg::cOBJ2D>;  //创建2D对象列表
	sub_objs->push_back(img_obj); //把刚才的2D对象加入到该列表里	  
	Scene2D->aObj = sub_objs;	  //把该列表接到2D场景里 作为它的子对象
}
GN_CALL void AppUnInit(){   //▲释放应用必须数据 程序运行结束前会调用它一次 
	Scene2D.Delete();			  //删除2D场景及它的所有子对象
}
GN_CALL void AppDisplay2D(unsigned int nTime){ //▲显示2D画面 程序运行每帧都调用它 
	GNg::cVIEW2D *view = GNg::ViewCur;
	GNg::ImageDraw(0, view->ortho_left,view->ortho_top,view->orthoW,view->orthoH,0xff000000); //绘制一个黑色屏幕大小的矩形以清屏
	
	if	   (GN::Key[GK_1])ObjPhys->aHP--;			//当按住键盘1建 使体力值 - 1
	else if(GN::Key[GK_2])ObjPhys->aHP++;			//当按住键盘2建 使体力值 + 1
	if	   (GN::Key[GK_3])ObjPhys->aST *= 0.95f;	//当按住键盘3建 使力量值 缩小一点
	else if(GN::Key[GK_4])ObjPhys->aST *= 1.05f;	//当按住键盘4建 使力量值 放大一点
	GN_RunValueCheck2( ObjPhys->aHP , 10 );			//显示体力值到屏幕上 排在第10号位置
	GN_RunValueCheck2( ObjPhys->aST , 11 );			//显示力量值到屏幕上 排在第11号位置

	Scene2D->UpdatePlay(nTime);						//播放显示2D场景
}
GN_CALL void AppDisplay3D(unsigned int nTime){}//▲显示3D画面 程序运行每帧都调用它 

GN_CALL void AppRun(unsigned int nTime){}      //▲运行程序后台的处理 程序运行每帧都调用它
#endif
}