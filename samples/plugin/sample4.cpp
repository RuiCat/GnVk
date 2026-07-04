#include<Media/GN_Media.h>		//使用到GN视频播放功能必需包含GN_Media.h头文件 里面已经包含 GN.h了
#include<Player/GN_EdPlugin.h>  //要导出参数设置菜单给编辑器打开 需包含编辑器插件接口头文件
#define eUI_VIDEO_VIEW 65534    //视频播放视图类型标记

namespace GNu{
struct sVIDEO_VIEW_STYLE	//视频播放视图界面式样
{
	unsigned int			back_color;		//底颜色
	unsigned int			focus_color;	//高亮颜色
	int						flags;			//界面式样标记  0:无 1:阴影
	
	sVIDEO_VIEW_STYLE():back_color(GN::RunRec.aBackColor),focus_color(GN::RunRec.aWarningColor),flags(0){} //使视图界面式样初始颜色和运行信息输出器一致
};
class cVIDEO_VIEW : public cUI_TEXT_ART, public GNm::cVIDEO	//视频播放视图  !!继承的UI组件必须放在前
{public:
	P<sVIDEO_VIEW_STYLE>	style;			//视图界面式样
	P<GNg::sIMAGE>			play_icon;	    //视图播放与暂停图标 tex_id 0:播放 1:暂停

	cVIDEO_VIEW():style(1){ 
		InitDefault();	fw = fh = 2;		//初始化静态文本基类的属性
		aType = eUI_VIDEO_VIEW;				/*初始化时设置类型标记*/
	}
	//重写以下这几个方法以对视频显示和数据保存与加载
	virtual P<iUI_OBJECT> Clone();												//复制出该UI组件的一个新副本
    virtual void UpdateRun(unsigned int updatetime, sUI_LEVEL *parent);			//更新运行(帧间隔时间, 父层级信息链)
	virtual unsigned int DataSave(cSHARE_DATA *share_objs, uint8 *data_out=0);  //把内部数据储存为一整段数据时的处理
	virtual unsigned int DataLoad(cSHARE_DATA *share_objs, const uint8 *data);  //从文件数据段里读入数据时的处理
};
P<iUI_OBJECT> cVIDEO_VIEW::Clone(){
	P<cVIDEO_VIEW> new_uobj(1);										  //创建一个本类对象
	cVIDEO_VIEW  *_new_uobj = new_uobj;								  //挖出裸指针以提高访问速度
	*((GNu::cUI_TEXT_ART*)_new_uobj) = *((cUI_TEXT_ART*)this);		  //复制静态文本组件相关数据给新对象
	_new_uobj->style = this->style;									  //复制界面式样给新对象
	_new_uobj->play_icon = this->play_icon;							  //复制播放图标给新对象
	if(*GetFileName())new_uobj->InitToData(GetFileName(), 0, false);  //当源视频播放已经装载文件时 就获取出它源文件名并传给新对象来装载它
	return new_uobj;			 //返回新对象
}
void cVIDEO_VIEW::UpdateRun(unsigned int updatetime, sUI_LEVEL *parent){
	short dx, dy;
	unsigned int dc = DrawBeginPosGet(dx, dy, parent);  //按设定的靠位对齐方式计算到最终显示坐标
	UpdataPlay(updatetime, dx, dy, w, h, parent->c);	//在指定位置和宽高下播放显示视频
	
	if(blendmode){ //当混合模式非0时 就显示界面和处理操作
		unsigned int bkc = GNg_MultiplyColorRatio(style->back_color,parent->c); //计算到受父层影响后的底颜色
		int bottom = dy + h;							//计算到视图底Y坐标
		GNg::sIMAGE *_icon = play_icon;					//临时拿出图标裸指针以加快访问速度
		if(_icon){
			int dy1 = bottom - _icon->height;			//计算到播放图标绘制Y坐标
			bool mouse_in_icon = this == RecvInputUiObject && GNg::ViewCur->HitRange(dx,dy1,_icon->width, _icon->height); //本对象是当前可接收输入UI组件和鼠标落到图标绘制范围里吗
			unsigned int dc1 = mouse_in_icon? GNg_MultiplyColorRatio(style->focus_color,parent->c) : parent->c;    //如鼠标接触到图标就用受父层影响后的高亮颜色否则直接用父层颜色
			unsigned short tid = _icon->texnum>1 && (!IsPlaying()||IsPause())? 1: 0;		 //当图标含有两个图以上且视频播完了或正在暂停时就用图1来显示否则用图0
			if(style->flags == 1)GNg::ImageDraw(_icon, dx+2, dy1+1, 0,0, bkc, tid); //当界面式样含有阴影时就用底颜色再向右偏移2像素绘制图标阴影
								 GNg::ImageDraw(_icon, dx  , dy1  , 0,0, dc1, tid); //绘制图标
			if(GN::MouseI[GM_LEFT] && mouse_in_icon){ //当鼠标左键单击了图标时
				if(!IsPlaying())Play();					//如停播了就播放视频
				else Pause();						    //否则切换视频暂停与播放
				UiActionCall(this, eUI_ACT_SELECT_ID);  //发出 点到首个选项 动作给UI组件交互回调
			}
		}
		unsigned int now_time = GetNowTime(), etime = GetEndTime();	//获得视频当前播放到的时间和总时长
		if(now_time > etime)now_time = etime;					//使最大已播时间不能大于总时长
		char time_str[32] = "00:00:00";							//视频播放时长默认显示字符信息
		if(now_time){			//已经播放了一些
			unsigned int sec  = now_time / 1000;				//计算到已播放总秒数
			unsigned int hour = sec / 3600;						//计算到已播放总小时数
			sprintf(time_str, "%02d:%02d:%02d", hour, (sec - hour*3600)/60, sec%60);  //按 时:分:秒 格式打印出播放时长字符信息
		}
		fh = textstyle->GetFontRangeY();							//获得文本显示行距作为时长信息显示筐高
		fw = textstyle->GetFontRangeX() * strlen(time_str) + 10;	//计算到时长信息显示筐宽
		if(style->flags == 1)textstyle->OutText(time_str,0, dx + w - fw+2, bottom - fh+1, fw, fh, bkc);			 //当界面式样含有阴影时就用底颜色在视图右下角再向右偏移2像素显示时长信息阴影
							 textstyle->OutText(time_str,0, dx + w - fw  , bottom - fh  , fw, fh, dc , blendmode); //在视图右下角显示时长信息

	    const int tlh = 4;												//播放进度条高
		int iconw = play_icon ? play_icon->width : 0;					//获得图标占位宽度
		int dx2   = dx + iconw + 10;									//计算到播放进度条绘制X坐标
		int dy2   = bottom - fh + (textstyle->GetFontHeight()-tlh)/2;	//计算到播放进度条绘制Y坐标
		int tlw   = w - fw - iconw - 10*2;								//计算到播放进度条宽
		bool mouse_inline = this == RecvInputUiObject && GNg::ViewCur->HitRange(dx2,dy2,tlw, tlh+2); //本对象是当前可接收输入UI组件和鼠标落到进度条绘制范围里吗 进度条高扩宽2像素来做判定 以避免难点中
		GNg::ImageDraw(0, dx2, dy2, tlw, tlh, bkc);						//绘制进度条底
		if(now_time){			//已经播放了一些
			int tlw1  = (int)((double)tlw * ((double)now_time / (double)etime)); //计算到已播放进度条宽
			unsigned int dc2 = mouse_inline? GNg_MultiplyColorRatio(style->focus_color,parent->c): dc; //如鼠标接触到进度条就用受父层影响后的高亮颜色否则用显示时长信息的颜色
			GNg::ImageDraw(0, dx2, dy2, tlw1, tlh, dc2, 0, blendmode);	//绘制已播放进度条
		}
		if(GN::MouseI[GM_LEFT] && mouse_inline){	  //当鼠标左键单击了进度条时
			float progr = (float)(GNg::ViewCur->mouseMX-dx2) / (float)tlw; //计算到点击位置对应的播放进度
			SetStage(progr);											   //设置视频当前播放进度
			UiActionCall(this, progr * (float)eUI_ACT_RANGE_MAX);		   //发出 选到进度 动作给UI组件交互回调
		}
	}
}
unsigned int cVIDEO_VIEW::DataSave(cSHARE_DATA *share_objs, uint8 *data_out){
	unsigned int size = cUI_TEXT_ART::DataSave(share_objs, data_out);  //调用继承类的保存数据
	char *video_filename = GetFileName(); //获得视频文件路径
	if(*video_filename){				  //有路径
		char *video_filename1 = GN_StrGetInAppDirPath(video_filename); //获得路径在当前运行应用程序工作目录里的那段相对路径
		if(video_filename1 > video_filename)video_filename = video_filename1; //如路径在应用程序工作目录里就用这个相对路径
	}
	unsigned int fname_size = strlen(video_filename) + 1;  //计算到储存路径需要的空间大小
	if(data_out){						  //有指定写缓冲
		uint8 *wpos = data_out + size;	  //计算到本类数据的写入位置
		unsigned int ptr_id;
		if( style ){					  //有界面式样时
			unsigned int eid = share_objs->sobj_list.size();	  //获得列表里将可存入的末尾序号ID
			ptr_id = share_objs->sobj_list.add_no_eq2( style );   //在其它自定义共享对象列表里查找界面式样所在序号ID 如没找到就把界面式样存入列表并返回它所在序号ID
			if(ptr_id == eid)share_objs->sobj_type_list.push_back(eUI_VIDEO_VIEW); //当刚才存入的界面式样是最末一个时就一并存入它的类型到其它自定义共享对象类型列表里 以便之后写自定义共享对象时能识别出它
		}else ptr_id = 0xffffffff;		  /*没界面式样时 以该值作标记*/
		*((unsigned int*)wpos) = ptr_id; wpos += 4;				  //写入界面式样所在序号ID 并使写入位置后移4字节

		if( play_icon ){				  //有播放图标时
			ptr_id = share_objs->pimage_list->add_no_eq2( play_icon );  //在共享图像列表里查找播放图标所在序号ID 如没找到就把播放图标存入列表并返回它所在序号ID
		}else ptr_id = 0xffffffff;		  /*没播放图标时 以该值作标记*/
		*((unsigned int*)wpos) = ptr_id; wpos += 4;				  //写入播放图标所在序号ID 并使写入位置后移4字节
		
		*((unsigned int*)wpos) = fname_size; wpos += 4;			  //写入视频路径占空间大小 并使写入位置后移4字节
		memcpy(wpos, video_filename, fname_size); //wpos += fname_size;  //写入视频路径 并使写入位置后移到路径末端
	}
	return size + 4+4+4 + fname_size;	  //返回数据段总大小字节
}
unsigned int cVIDEO_VIEW::DataLoad(cSHARE_DATA *share_objs, const uint8 *data){
	unsigned int size = cUI_TEXT_ART::DataLoad(share_objs, data);   //调用继承类的读取数据
	if(!size)return 0; //遇到继承类数据读取失败 就放弃读取返回失败标记0
	uint8 *rpos = (uint8*)data + size;	  //拿到当前数据读取位置
	unsigned int ptr_id = *((unsigned int*)rpos);	rpos += 4;		//读取界面式样序号ID 并使读取位置后移4字节
	if(ptr_id != 0xffffffff)style = share_objs->sobj_refs[ptr_id];  //当ID不是特殊标记时 就以该序号ID在其它自定义共享对象引用数组里拿到属于该视频视图的界面式样
	ptr_id = *((unsigned int*)rpos);	rpos += 4;					//读取播放图标序号ID 并使读取位置后移4字节
	if(ptr_id != 0xffffffff)play_icon = (*(share_objs->pimage_refs))[ptr_id]; //当ID不是特殊标记时 就以该序号ID在共享图像引用数组里拿到属于该视频视图的播放图标
	unsigned int fname_size = *((unsigned int*)rpos);	rpos += 4;  //读取视频路径占空间大小 并使读取位置后移4字节
	if(fname_size > 1)InitToData((char*)rpos, 0, false);			//当路径有效就装载该路径的视频
	return size + 4+4+4 + fname_size;	  //返回已读取总字节数
}
};
#if 1 /* 导出为引擎插件 ******************************************************************************************************************************************************************************************************************************/
//定义3个函数指针用于备份之前UI的相关3个函数
P<GNu::iUI_OBJECT> (*LastUiObjCreateFunc)(unsigned int);
unsigned int (*LastUiShareObjSaveFunc)(unsigned int, P<uint8>, uint8*);
unsigned int (*LastUiShareObjLoadFunc)(P<uint8>&, const uint8*);

//本例子新增的UI组件创建方法(UI组件类型)
P<GNu::iUI_OBJECT> SampleUiObjectCreate(unsigned int _type){
	if(_type == eUI_VIDEO_VIEW)return P<GNu::cVIDEO_VIEW>(1);  //当UI组件类型是视频播放视图时就创建视频播放视图并返回它的安全指针
	else return LastUiObjCreateFunc(_type);					   //其它类型就调用之前的创建方法
}
//本例子新增的自定义共享对象储存方法(共享对象类型, 共享对象, 返回整段数据 首个字节为版本号之后4个字节为参数type 当该值为0时仅返回数据段需要大小)返回数据段大小字节 !用户自定义的版本号应由1开始
unsigned int SampleUiShareObjSave(unsigned int type, P<uint8>  obj, uint8 *data_out){
	if(type == eUI_VIDEO_VIEW){  //当类型为视频播放视图时按以下方式来存储数据
		uint8 loc_buf[32];					//定义32字节临时缓冲
		if(!data_out)data_out = loc_buf;    //当外面调用本函数没指定data_out时就使它指向临时缓冲 以便以同一做法来统计需要的数据空间	
		uint8 *wpos = data_out;				//拿到当前数据写入位置
		*wpos = 1; ++wpos;					//写入版本 并使写入位置后移1字节
		*((unsigned int*)wpos) = type; wpos += 4;  //写入类型 并使写入位置后移4字节
		GNu::sVIDEO_VIEW_STYLE *_vvs = CVP(GNu::sVIDEO_VIEW_STYLE)obj;  //把共享对象转换回视频视图界面式样 之前存到其它自定义共享对象列表里的界面式样就会通过该回调来续个储存它的数据
		*((unsigned int*)wpos) = _vvs->back_color;  wpos += 4;	//写入底颜色   并使写入位置后移4字节
		*((unsigned int*)wpos) = _vvs->focus_color; wpos += 4;  //写入高亮颜色 并使写入位置后移4字节
		*wpos				   = _vvs->flags;		wpos += 1;  //写入界面式样标记 并使写入位置后移1字节
		return wpos - data_out;				//当前写入位置减数据起始位置就是数据段大小 返回大小

	}else return LastUiShareObjSaveFunc(type, obj, data_out);  //其它类型就调用之前的储存方法
}
//本例子新增的自定义共享对象读取方法(返回自定义共享对象, 数据段地址 首个字节为版本号之后4个字节为type)返回已读取字节数 失败返回0 !用户自定义的版本号应由1开始
unsigned int SampleUiShareObjLoad(					 P<uint8> &obj, const uint8 *data){
	if(*((unsigned int*)(data+1)) == eUI_VIDEO_VIEW){ //检测数据段首个字节之后的类型是否为视频播放视图 如是就按以下方式来读取数据
		uint8 *rpos = (uint8*)data;					  //拿到当前数据读取位置
		uint8 version = *rpos; ++rpos; if(version > 1)return 0; //读取版本 并使读取位置后移1字节 检测当读到版本 > 本插件编写时版本1时 可能会遇到未知格式就放弃读取返回失败标记0
		unsigned int type = *((unsigned int*)rpos);  rpos += 4; //读取类型 并使读取位置后移4字节
		P<GNu::sVIDEO_VIEW_STYLE> vvs(1);			  //创建一个视频视图界面式样
		obj = vvs;									  //把该界面式样传出去
		GNu::sVIDEO_VIEW_STYLE *_vvs = vvs;			  //挖出裸指针以提高访问速度
		_vvs->back_color  = *((unsigned int*)rpos);  rpos += 4; //读取底颜色   并使读取位置后移4字节
		_vvs->focus_color = *((unsigned int*)rpos);  rpos += 4; //读取高亮颜色 并使读取位置后移4字节
		_vvs->flags		  = *rpos;					 rpos += 1; //读取界面式样标记 并使读取位置后移1字节
		return rpos - data;					//当前读取位置减数据起始位置就是数据段大小 返回大小

	}else return LastUiShareObjLoadFunc(obj, data);		//其它类型就调用之前的读取方法
}
//连接本例子的3个回调给UI系统 以便UI系统可以对本例子的自定义UI组件作处理
void SampleCallBackComment(){
	LastUiObjCreateFunc	   = GNu::UiObjectCreate;  //备份之前UI的相关3个函数
	LastUiShareObjSaveFunc = GNu::UiShareObjSave;
	LastUiShareObjLoadFunc = GNu::UiShareObjLoad;
	GNu::UiObjectCreate = SampleUiObjectCreate;	   //使UI系统的相关3个回调函数指针指向本例子的3个回调函数 以便优先回调到本例的处理方法
	GNu::UiShareObjSave = SampleUiShareObjSave;
	GNu::UiShareObjLoad = SampleUiShareObjLoad;
}
#ifdef _GN_ED_PLUGIN_H_ /* 增加导出编辑器回调选单 **************************************************************************************************************************************************************************************/
#define MULTI_OBJ_ADJUST						   //支持多对象一起调整
P<GNu::sVIDEO_VIEW_STYLE> ViewStyleBk;			   //视图界面式样复制副本

class cVIDEO_STYLE_MENU :public GNu::iSETTING_MENU  //视频式样设置选单
{  
	char					file_buf[GN_MAXFILEPATH];	//文件路径修改缓冲
   public:
   #ifdef MULTI_OBJ_ADJUST
	unsigned int			back_color2;				//界面式样底颜色备份
	unsigned int			focus_color2;				//界面式样高亮颜色备份
	int						flags2;						//界面式样标记备份

	P< P_ >					other_objects;				//记录其它附带选到的对象  如您无需对多个选定对象一起进行参数调整 可以忽略这
   #endif
	GNu::iSETTING_MENU	   *icon_load_menu;			    //图标读取选单

   ~cVIDEO_STYLE_MENU(){ GN_Delete(icon_load_menu); }	//视频式样选单销毁时一定销毁图标读取选单
	virtual void Open();								//打开菜单时的处理
	virtual void Change();								//菜单值更改时的处理
	virtual void ParameCopy(GNu::eCOPY_OPT op);			//参数复制时的处理(复制副本操作方式)

	//获取复制栏显示状态(返回预览小图标色调, 返回是否显示粘贴图标, 返回是否需要显示粘贴实列图标, 返回首个复制属性起始ID 返回0xff不显示复制栏, 返回最末复制属性的ID)返回预览小图标 返回0用纯色显示
	virtual GNg::sIMAGE *GetCopyBarState(unsigned int &icon_color, bool &show_paste, bool &show_paste_inst, uint8 &first, uint8 &end){
		show_paste = ViewStyleBk?true:false; show_paste_inst = false; first = 0; end = 2;
		return 0;
	}
};
void cVIDEO_STYLE_MENU::Open(){
	GNu::sVIDEO_VIEW_STYLE *_vs = ((GNu::cVIDEO_VIEW*)set_obj)->style;
   #ifdef MULTI_OBJ_ADJUST
	back_color2  = _vs->back_color;		//备份原式样的3个值
	focus_color2 = _vs->focus_color;
	flags2		 = _vs->flags;
   #endif
	InitValueToData(&(_vs->back_color),  " 底颜色         :  ", 255, 0);				//插入底颜色修改项
	InitValueToData(&(_vs->focus_color), " 高亮颜色       :  ", 255, 0);				//插入高亮颜色修改项
	InitValueToData(&(_vs->flags),	     " 界面式样       :  ", 1, 0, 1, " 无| 阴影");  //插入界面式样修改项

	strcpy(file_buf, ((GNu::cVIDEO_VIEW*)set_obj)->GetFileName());  //复制原视频文件路径
	InitValueToData(file_buf  ,			 "+视频路径       :  ", GN_MAXFILEPATH, 26);	//插入路径修改项 路径筐宽最小为26个字符宽
	InitSubMenuToData(icon_load_menu, "	 播放与暂停图标", 3);							//插入图标读取子选单
}
void cVIDEO_STYLE_MENU::Change(){
   #ifdef MULTI_OBJ_ADJUST
	GNu::sVIDEO_VIEW_STYLE *_vs = ((GNu::cVIDEO_VIEW*)set_obj)->style;  //临时拿出界面式样裸指针以加快访问速度
	unsigned int i;
	if(back_color2 != _vs->back_color){		//当底颜色有更变了
		back_color2 = _vs->back_color;		//再次备份底颜色
		for(i = 0; i < other_objects.Num(); ++i)
			(CVP(GNu::cVIDEO_VIEW)other_objects[i])->style->back_color = _vs->back_color;   //把底颜色一起设置给其它附带选到的对象
	}
	if(focus_color2 != _vs->focus_color){	//当高亮颜色有更变了
		focus_color2 = _vs->focus_color;	//再次备份高亮颜色
		for(i = 0; i < other_objects.Num(); ++i)
			(CVP(GNu::cVIDEO_VIEW)other_objects[i])->style->focus_color = _vs->focus_color; //把高亮颜色一起设置给其它附带选到的对象
	}
	if(flags2 != _vs->flags){				//当式样标记有更变了
		flags2 = _vs->flags;				//再次备份式样标记
		for(i = 0; i < other_objects.Num(); ++i)
			(CVP(GNu::cVIDEO_VIEW)other_objects[i])->style->flags = _vs->flags;			  //把式样标记一起设置给其它附带选到的对象
	}
   #endif
	if(strcmp(((GNu::cVIDEO_VIEW*)set_obj)->GetFileName(), file_buf)){  //当视频文件路径有更变了
		((GNu::cVIDEO_VIEW*)set_obj)->InitToData(file_buf, 0, false);		//装载新选到的视频文件
	   #ifdef MULTI_OBJ_ADJUST
		for(i = 0; i < other_objects.Num(); ++i)
			(CVP(GNu::cVIDEO_VIEW)other_objects[i])->InitToData(file_buf, 0, false);		  //把新选到的文件路径传给其它附带选到的对象一起装载
	   #endif
	}
}
void cVIDEO_STYLE_MENU::ParameCopy(GNu::eCOPY_OPT op){
	if(op){
		if(op == GNu::eCOPY_COPY){ //当提示复制时
			ViewStyleBk.New(1);									   //分配副本数据空间
		   *ViewStyleBk = *(((GNu::cVIDEO_VIEW*)set_obj)->style);  //复制视图界面式样到副本
		}else{					   //其它情况就是粘贴了
			*(((GNu::cVIDEO_VIEW*)set_obj)->style) = *ViewStyleBk; //复制回界面式样到视图
		   #ifdef MULTI_OBJ_ADJUST
			Change();											   //值更改了执行一下 Change 以便通知其它附带选到的对象也一起更变
		   #endif
		}
	}else ViewStyleBk.Delete(); //当提示要删除副本就删除它
}
void (*LastSettingMenuInsertFunc)(GP_eSETTING_CLASS, void *, P< P_ > &, GNu::sSIM_MENU*);  //备份原菜单插入函数用的函数指针

P<GNg::sIMAGE>&	VideoViewIconGet(void *uobj){			 //获取视图播放图标的回调函数
	return ((GNu::cVIDEO_VIEW*)uobj)->play_icon;
}
void VideoViewIconSet(void *uobj, P<GNg::sIMAGE> &img){  //设置视图播放图标的回调函数
	((GNu::cVIDEO_VIEW*)uobj)->play_icon = img;
}
//本例子新增的UI参数设置菜单插入方法(设置对象的类型, 设置对象, 其它一起选到的对象, 父菜单)
void SampleSettingMenuInsert(GP_eSETTING_CLASS set_class, void *_set_obj, P< P_ > &other_objs, GNu::sSIM_MENU *pmenu){
	if(set_class == GP_eSETTING_OBJUI && ((GNu::iUI_OBJECT*)_set_obj)->aType == eUI_VIDEO_VIEW){ //仅关心本例子的UI组件类型
		cVIDEO_STYLE_MENU *style_menu = new cVIDEO_STYLE_MENU;	 //创建视频式样设置选单
		style_menu->set_obj = _set_obj;							 //为选单指定将要设置的UI组件对象 
	   #ifdef MULTI_OBJ_ADJUST
		style_menu->other_objects = other_objs;					 //为选单指定将要设置的其它一起选到对象
	   #endif
		GP_sMENU_ARGS_IMAGE menu_args(GP_eSETTING_OBJUI, VideoViewIconGet, VideoViewIconSet);  //构建图标读取选单请求信息
		style_menu->icon_load_menu = GP_BaseSetMenuCreate(GP_eMENU_IMAGE, &menu_args);		   //向编辑器请求创建图标读取选单
		pmenu->InitSubMenuToData(style_menu, "	 视频源与式样", 3);     //插入该视频式样设置选单到编辑器里的UI组件主选单里 !该选单插入后就会被编辑器接管 您无需再手动销毁它 !!注意选单名首字符必须为'Tab'而不是空格
	}
	LastSettingMenuInsertFunc(set_class, _set_obj, other_objs, pmenu);  //继续执行之前的其它设置菜单插入方法
}
extern "C"{
//获得插件支持的UI组件类型信息并添加到相应链表里(装填UI组件类型名的链表, 装填UI组件类型的链表, 装填UI组件继承那个基类型UI组件参数设置的链表)
GN_CALL void GetSupportTypes(GN::sLIST<char*> &uobj_type_names, GN::sLIST<unsigned int> &uobj_types, GN::sLIST<unsigned int> &base_types){
	LastSettingMenuInsertFunc = GP_SettingMenuInsert;  //备份之前插入UI参数设置菜单的函数
	GP_SettingMenuInsert = SampleSettingMenuInsert;	   //使UI参数设置菜单回调函数指针指向本例子的相关回调函数 以便优先回调到本例的处理方法

	uobj_type_names.push_back("视频播放视图");		//加入支持的组件名称
	uobj_types.push_back(eUI_VIDEO_VIEW);			//加入支持的组件类型	  !这个选值必须 > GNu::eUI_TYPE_NUM 若要与其他UI组件扩展插件兼容 您最好通过登陆引擎官网申请到一个全网唯一的值
	base_types.push_back(GNu::eUI_TEXT_ART);		//加入该组件继承的基类型  值0xffff表示没有继承 其它选值必须是 >= GNu::eUI_BACK_FRAME && < GNu::eUI_TYPE_NUM 的值
} }
#endif /********************************************************************************************************************************************************************************************************************************/
extern "C"{
//必须导出一个版本函数给引擎调用 并返回接口版本的宏  !之后把编译好的插件动态库和它依赖到的资源放到一个与插件动态库同名的文件夹下 并将该文件夹放到引擎目录 Plugins/UIObject/ 下就可以被编辑器加载到 但您用SDK开发的应用需要手动调用 GN_PluginsLoad 系列函数来选择加载那些插件
GN_CALL char*const Version(){  
	SampleCallBackComment();		//连接本例子的3个回调给UI系统
	return GN_INTERFACE_VERSION;	//返回接口版本
}		 
#else /* 插件使用例子 *******************************************************************************************************************************************************************************************************************************/
P<GNu::cUI_SCENE>  UiScene(1);		//定义一个UI场景 以便来显示视频播放视图
extern "C"{
GN_CALL void AppInit(){     //▲初始化应用必须数据 程序开始准备好引擎系统后会调用它一次 
	P<GNu::cVIDEO_VIEW> video_view(1);			  //创建一个视频播放视图
	GNu::cVIDEO_VIEW  *_video_view = video_view;  //挖出裸指针以提高访问速度
	_video_view->x = 32;  _video_view->y = 32;	  //指定视频播放视图基于上层的相对坐标
	_video_view->w = 640; _video_view->h = 360;	  //指定视频播放视图的显示宽高
	_video_view->InitToData("samples/base/sound/summer.mkv", 0, false);  //初始化装载视频文件

	UiScene->aSubs.push_back(video_view);		  //把视频播放视图加入到UI场景的子对象列表里
}
GN_CALL void AppUnInit(){   //▲释放应用必须数据 程序运行结束前会调用它一次 
	UiScene.Delete();			//删除UI场景及它的所有子对象
}
GN_CALL void AppDisplay2D(unsigned int nTime){ //▲显示2D画面 程序运行每帧都调用它 
	GNg::cVIEW2D *view = GNg::ViewCur;
	GNg::ImageDraw(0, view->ortho_left,view->ortho_top,view->orthoW,view->orthoH,0xff000000); //绘制一个黑色屏幕大小的矩形以清屏
	UiScene->Resize(GNg::ViewCur);			//重设UI场景显示范围为当前视口大小
	UiScene->UpdatePlay(nTime);				//播放显示UI场景
}
GN_CALL void AppDisplay3D(unsigned int nTime){}//▲显示3D画面 程序运行每帧都调用它 

GN_CALL void AppRun(unsigned int nTime){}      //▲运行程序后台的处理 程序运行每帧都调用它
#endif
}