/****************************************************************************
Copyright (c) 2018-2025 gncreate Technologies Inc.

 http://www.gncreate.com
 http://www.img-power.com

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 ****************************************************************************/

#include"GN.h"
#include "GN_Def.h"
#ifdef _GN_UIFBROWSER_H_

#define GN_USING_FILE_PREVIEW

//填充空图标图数据( 参考大小的图像 )
#define GT_TMP_FILL_EMPTY_IMG_DATA( IMG ) \
	uint8 locbuf[32768];\
	GN::sTBUF_INT tbuf(locbuf,32768);\
	unsigned int dsize = IMG ->width * IMG ->height * GNg::ImgFmtGetByteNum(IMG ->cFormat);\
	uint8 *buf0 = (uint8*)tbuf.GetSpace(dsize);\
	memset(buf0, 0, dsize);\
	icon.DataFill(buf0, IMG ->cFormat); //第0层贴图为空

P<GNg::sIMAGE>	   GT_FB_DefaultIconImg;    //默认图标图

#ifdef GN_USING_FILE_PREVIEW /********************************************** 文件缩图预览功能 ****************************************************************************/
uint8             *GT_FbIconBackData  = 0;	//图标底图数据
uint8             *GT_FbIconFrameData = 0;	//图标外筐数据
GNg::COLOR_FORMAT  GT_FbIconBackCfmt;		//图标底图颜色格式
GNg::COLOR_FORMAT  GT_FbIconFrameCfmt;		//图标外筐颜色格式
int                GT_FbIconBackW = -1;		//图标底图宽
int                GT_FbIconBackH;			//图标底图高
int                GT_FbIconFrameX;			//图标外筐相对显示位置X
int                GT_FbIconFrameY;			//图标外筐相对显示位置Y
int                GT_FbIconFrameW;			//图标外筐宽
int                GT_FbIconFrameH;			//图标外筐高
GNg::sRECT         GT_FbPicInIconRange;		//缩图贴在图标底图的相对范围

//图标加框操作初始化
void GT_FbIconOptInitToData(){
	GNf_DirStatePush(false);
	//读取文件加载图标和筐和信息的内容用于生成预览图用 ------------------------------------------------------------------------------------
    bool _is_bmp_, _add_0000_;
    char *file_name__, *file_name;
	int ifname_len;

   #define GT_TMP_FB_LOAD_DEF_EMPTY_ICON( PATH_ , V ) \
	file_name__ = 0, file_name = GN_SysDir( PATH_ );\
    GT_ImageFileNameSet(file_name, &ifname_len, &_add_0000_, &file_name__);\
	GT_FbIcon##V##Data = GT_ImageFileLoadToData(file_name__, ifname_len, 0, 0, &GT_FbIcon##V##Cfmt, &GT_FbIcon##V##W, &GT_FbIcon##V##H);\
	if(file_name__)delete [] file_name__;\
	if(!GT_FbIcon##V##Data){ GN_Log("读取图象文件失败! 找不到文件 : %s\n" /*"Load Image file faile! do not file : %s\n"*/,file_name); }

	GT_TMP_FB_LOAD_DEF_EMPTY_ICON( "fileicon/load_icon"  , Back  )
	GT_TMP_FB_LOAD_DEF_EMPTY_ICON( "fileicon/load_iconF" , Frame )

	int fsize;
	file_name = GN_SysDir("fileicon/load_icon.ini");
    char *ini_file_buf = GNf::FileFullLoadToMemoryText(file_name,&fsize);
	if(ini_file_buf){
		char arg_name1[64], arg_name2[64];
		if(sscanf(ini_file_buf, "%s %d %d %d %d %s %d %d",arg_name1 ,&GT_FbPicInIconRange.left,&GT_FbPicInIconRange.top,&GT_FbPicInIconRange.right,&GT_FbPicInIconRange.bottom, arg_name2, &GT_FbIconFrameX, &GT_FbIconFrameY) != 8){
			GN_Log("%s 文件图标筐信息设置不正确! 应按这样的格式来设置\nrange 预览图填充区相对图标的偏移位置X Y 预览图填充宽度 高度\nframeOffset 图标筐显示相对图标的偏移位置X Y\n示范:\nrange 3 5 48 48\nframeOffset 2 2\n\n" , file_name);
		    fsize = 0;
		}
	    delete [] ini_file_buf;
	}else{
		GN_Log("读取文件图标筐信息失败! 找不到文件: %s\n" /*"load file icon frame info faile! do not file: %s\n"*/, file_name);
	}
	int def_icon_w = GT_FB_DefaultIconImg->width, def_icon_h = GT_FB_DefaultIconImg->height;
	if(!fsize){
		GT_FbPicInIconRange.Init(0,0,def_icon_w,def_icon_h);
	    GT_FbIconFrameX = GT_FbIconFrameY = 0;
	}
    //检查这些信息有没指定正确 -------------------------------------------------------------
	if(GT_FbIconBackData){
		uint8 *tem_img_data = 0;
		if(GT_FbIconBackW != def_icon_w || GT_FbIconBackH != def_icon_h){
	        int pd = GNg::ImgFmtGetByteNum(GT_FbIconBackCfmt);
			tem_img_data = new uint8[def_icon_w * def_icon_h * pd]; //图大小不匹配就缩放到一致
			GNg::ImgScale(tem_img_data, def_icon_w, def_icon_h, GT_FbIconBackData, GT_FbIconBackW, GT_FbIconBackH, pd);
            delete [] GT_FbIconBackData;
            GT_FbIconBackData = tem_img_data;
			GT_FbIconBackW = def_icon_w;
			GT_FbIconBackH = def_icon_h;
		}
		if(GT_FbIconBackCfmt != GNg::CR_RGBA){ //图不是RGBA个格式
            tem_img_data = new uint8[GT_FbIconBackW * GT_FbIconBackH * GNg::ImgFmtGetByteNum(GNg::CR_RGBA)];
			GNg::ImgFmtConvertTs(tem_img_data,GNg::CR_RGBA,GT_FbIconBackData,GT_FbIconBackCfmt,GT_FbIconBackW * GT_FbIconBackH,false);
            delete [] GT_FbIconBackData;
            GT_FbIconBackData = tem_img_data;
		}
		bool in_pic_out_range = false;
		if(GT_FbPicInIconRange.left + GT_FbPicInIconRange.right > GT_FbIconBackW ){
		    GT_FbPicInIconRange.left = 0;
            GT_FbPicInIconRange.right = GT_FbIconBackW;
			in_pic_out_range = true;
		}
		if(GT_FbPicInIconRange.top + GT_FbPicInIconRange.bottom > GT_FbIconBackH){
		    GT_FbPicInIconRange.top = 0;
            GT_FbPicInIconRange.bottom = GT_FbIconBackH;
			in_pic_out_range = true;
		}
		if(in_pic_out_range){ GN_Log("指定的预览图填充区超出图标范围!\n"); }
	}else GT_FbIconBackW = 0;

	if(GT_FbIconFrameData){
		if(GT_FbIconFrameCfmt != GNg::CR_RGBA){
		    GN_DeleteArray(GT_FbIconFrameData);
            GN_Log("文件图标加筐的图不是RGBA 32位格式的! 将不能正确加在图标上面, 该图标筐将被忽略\n");
		}else if(GT_FbIconFrameX + GT_FbIconFrameW > GT_FbIconBackW || GT_FbIconFrameY + GT_FbIconFrameH > GT_FbIconBackH){
		    GN_DeleteArray(GT_FbIconFrameData);
            GN_Log("文件图标加筐的范围超出图标本身! 图标筐将被忽略\n");
	}	}
	GNf_DirStatePop();
}
//释放图标加框操作内部资源
void GT_FbIconOptRelease(){
	GN_DeleteArray(GT_FbIconBackData);
	GN_DeleteArray(GT_FbIconFrameData);
	GT_FbIconBackW = -1;
}
/*********************************************************** GT_cFILE_PREVIEW_GEN ************************************************************/
//#define GT_FB_ICON_FILL_FULL //如图片比较小时也填满图标筐
//#define GT_FB_3DSCENE_KEEP   //当场景提交渲染后还保持住场景不删除 直到更换浏览别的目录或浏览关闭后才切底删除

static void GT_FB_IconDataLoadAndGen(P<GN_sTHREAD>, void*, void*);

enum GT_eFB_ICON_PROC_STATE{  //文件图标处理状态
	 GT_eFB_ICON_PROC_NONE,			//无需处理
     GT_eFB_ICON_PROC_LOAD,			//需要加载
	 GT_eFB_ICON_PROC_RENDER,		//需要渲染
	 GT_eFB_ICON_PROC_RENDER_LOAD	//渲染前加载
};
class GT_cFILE_PREVIEW_GEN	  //文件预览缩图生成器
{
	friend void ::GT_FB_IconDataLoadAndGen(P<GN_sTHREAD>, void*, void*);
	char             **zPaths;     //文件浏览里的全局路径
	uint8            **zDatas;     //收集对应每个图标的图标数据
	uint8             *zStates;    //标记对应每个图标当前状态
	int                zLoadId;    //通知线程读取对应的文件编号
	int                zFid;       //线程正在读取处理的文件编号
	int                zIconNum;   //要显示的总文件和文件夹图标数
	int                zIconW;	   //图标宽
	int				   zIconH;     //图标高
	P<GN_sTHREAD>      zLoadTask;  //读取任务线程
   public:
	GT_cFILE_PREVIEW_GEN(void *fb_hinstance=0);
   ~GT_cFILE_PREVIEW_GEN(){
	   OnIconClear();
    }
    //当文件浏览有文件或目录图标要生成时的处理(当前要准备显示的文件夹和文件总数, 所包含的文件夹和文件全局路径数据, 对应每个文件夹和文件所分配到的图标号数据, 是否查找结果, 显示图标宽度, 高度) !这个通常在打开一个目录显示目录里的内容前和查找文件显示结果前被调用到 这里分配到的图标号数据你可修改它来实现文件类型图标更变
	void  OnIconGen(int fn, const char **paths, char *icon_ids, bool is_search_result, int icon_w, int icon_h);
	//当文件浏览的文件或目录图标要清除时的处理() !这个通常在新一批图标要准备生成时或该类对象销毁前被调用到
	void  OnIconClear();
	//当文件浏览图标更变数据要检测是否准备好时的处理(当前图标属于第几个文件夹或文件, 是否被选中, 显示图标宽度, 高度, 当前图标对应文件或目录的全局路径)返回一个通知状态值 0:该图标没有数据无需更变图标 4:数据准备好了 建议原图标消失 新图标带弹出动画出现 并提示接下来不用再检测该图标数据了
	int   OnIconDataOkCheck(int fi, int is_sed, int icon_w, int icon_h, const char *path);
	//当文件浏览图标更变需要获取数据时的处理(当前图标属于第几个文件夹或文件, 是否被选中, 当前图标对应文件或目录的全局路径)返回图标的图象数据 !RGBA格式数据, 这个通常是 OnIconDataOkCheck 被调用后得知图标更变数据准备好了才被调用的 用以来更新图标数据内容
	char* OnIconDataGet(int fi, int is_sed, const char *path);
	//当文件浏览图标获取到数据后更变了时的处理(当前图标属于第几个文件夹或文件, 是否被选中, 当前图标对应文件或目录的全局路径) 这个通常是 OnIconDataGet 被调用后获得数据更改了图标后就被调用 由于数据被传达后文件浏览会另外拷贝副本 所以这里就把数据删掉以节省空间
	void  OnIconDataChanged(int fi, int is_sed, const char *path);
	
	void  IconDataLoadAndGenProc();	 //图标数据读取和生成处理
};
GN::sLIST<GT_cFILE_PREVIEW_GEN>  GT_FilePreviewGenList; //文件预览缩图生成器列表 保存所有该类对象

GT_cFILE_PREVIEW_GEN::GT_cFILE_PREVIEW_GEN(void *fb_hinstance):zDatas(0),zStates(0),zIconNum(0){
	if(GT_FbIconBackW == -1){
		GT_FbIconOptInitToData(); //-1表示还未初始化
	}
}
void GT_cFILE_PREVIEW_GEN::OnIconGen(int fn, const char **paths, char *icon_ids, bool is_search_result, int icon_w, int icon_h){
    OnIconClear();
    zIconNum = fn;
	if(!fn)return;

    //预生成一些处理图标用到的信息数据
    zPaths  = (char**)paths;
    zDatas  = new uint8*[fn];
	zStates = new uint8[fn];
	zIconW = icon_w, zIconH = icon_h;
	unsigned int fpos = GN_StrGetFileName(zPaths[0]) - zPaths[0];
	char *filename, *ext_name;
	uint8 need_load = 0;
	int i = 0;
	for(; i < fn; ++i){
		zDatas[i] = 0;
		filename = zPaths[i] + fpos;
		if(GNg::CheckImgFmtSupportFromFileName(filename)){ need_load |= 1; zStates[i] = GT_eFB_ICON_PROC_LOAD; } //标记为要加载的文件
		else{
			ext_name = GN_StrGetExtName(filename);
			if(ext_name && (!strcmp(ext_name, ext_name[0]<'a'?"IES":"ies"))){ need_load |= 1; zStates[i] = GT_eFB_ICON_PROC_LOAD; } //标记为要加载的文件
			else zStates[i] = GT_eFB_ICON_PROC_NONE;	//其它类型文件标记为不处理
		}
	}
	if(need_load){
		zFid = -1; zLoadId = -1;
		zLoadTask = GN_ThreadStart(GT_FB_IconDataLoadAndGen, this);
	}
}
void GT_cFILE_PREVIEW_GEN::OnIconClear(){
	if(zLoadTask && zLoadTask->us_state != 255){
	    zLoadTask->us_state = 254; zLoadId = 0;
		GN_ThreadEndWait(zLoadTask); //等待处理线程结束
		zLoadTask.Delete();
	}
	//删除处理图标用的信息数据
	if(zDatas){
	    int di = 0;
		for(; di < zIconNum ; ++di){
			if(zDatas[di]){
				delete [] zDatas[di];
		}	}
        delete [] zDatas;
        zDatas = 0;
	}
	GN_DeleteArray(zStates);
    zIconNum = 0;
}
int GT_cFILE_PREVIEW_GEN::OnIconDataOkCheck(int fi, int is_sed, int icon_w, int icon_h, const char *path){
	if(!zStates || zStates[fi] == GT_eFB_ICON_PROC_NONE)return 0;

	if(zDatas[fi])return 3;  //对应数据准备好了
	else{
		if(zLoadId < 0 && fi != zFid)zLoadId = fi; //通知线程读取对应编号的文件
		return -1;
	}
}
char* GT_cFILE_PREVIEW_GEN::OnIconDataGet(int fi, int is_sed, const char *path){
	if(zDatas[fi])return (char*)(zDatas[fi]);	   //返回提交对应图标数据
	else          return 0;
}
void GT_cFILE_PREVIEW_GEN::OnIconDataChanged(int fi, int is_sed, const char *path){
	if(zDatas[fi]){ //提交完的数据就删除它以节省空间
		delete [] zDatas[fi];  
		zDatas[fi] = 0;	
	}
}
void GT_cFILE_PREVIEW_GEN::IconDataLoadAndGenProc(){
	GN_sTHREAD *_task = zLoadTask;
	if(!_task)return;
	bool egp, erout;
	GNg::COLOR_FORMAT cr_format;
	int fi, tw, th, px_byte, icon_dsize = zIconW * zIconH * GNg::ImgFmtGetByteNum(GNg::CR_RGBA);
   #ifndef GT_FB_ICON_FILL_FULL
    int _tw, _th;
   #endif
	uint8 *tmp_buf = new uint8[icon_dsize], *t_data, *icon_data;
	char *ext_name;
	while(_task->us_state < 254){
		zFid = -1;
		while(zLoadId < 0)GN_TimeSleep(10);
		zFid = fi = zLoadId; zLoadId = -1;		//传递读取编号
        if(_task->us_state == 254)break;
		erout = GN_RunErrorOutputOn;
		GN_RunErrorOutputOn = false;
		egp = GNf::sFILE::EnableGlobalPath;
        GNf::sFILE::EnableGlobalPath = true; //使用求全局路径来读取数据
			ext_name = GN_StrGetExtName(zPaths[fi]);
			cr_format = GNg::CR_DEFAULT_FORMAT;
			GNg::PF_LOAD_IMG_FILE_DATA pf_load = GNg::FileImage->CheckFmtSupportFromExtName(ext_name);
			if(pf_load){
				  t_data = GT_ImageFileLoadToData(zPaths[fi], 0, pf_load, 0, &cr_format, &tw, &th);
			}else t_data = 0;
			GNf::sFILE::EnableGlobalPath = egp;
			GN_RunErrorOutputOn = erout;
			if(t_data){
				if(_task->us_state < 254){ //缩放并结合图标数据
					if(cr_format != GNg::CR_DEFAULT_FORMAT && cr_format != GNg::CR_PALETTE){
						px_byte = GNg::ImgFmtGetByteNum(cr_format);
                        GN_DeleteArray(zDatas[fi]);
						if(tw == zIconW && th == zIconH && cr_format == GNg::CR_RGBA){
							icon_data = t_data; t_data = 0; //图片数据与图标所需的数据匹配
						}else{
							icon_data = new uint8[icon_dsize];
							if(!GT_FbIconBackData || cr_format == GNg::CR_RGBA || cr_format == GNg::CR_LUMINANCE_ALPHA || cr_format == GNg::CR_ALPHA){ //含透明的图标处理
							   #ifndef GT_FB_ICON_FILL_FULL
								_tw = tw < zIconW ? tw : zIconW;  _th = th < zIconH ? th : zIconH;
                               #endif
								if(cr_format == GNg::CR_RGBA){
                                   #ifndef GT_FB_ICON_FILL_FULL
									if(_tw == zIconW && _th == zIconH)
										GNg::ImgScale(icon_data, zIconW, zIconH, t_data, tw, th, px_byte);
									else{
										memset(icon_data, 0, icon_dsize);
										GNg::ImgScaleBlt(t_data, tw, 0, 0, tw, th, icon_data, zIconW, _tw == zIconW ? 0 : ((zIconW-_tw)>>1), _th == zIconH ? 0 : ((zIconH-_th)>>1), _tw, _th, cr_format);
									}
                                   #else
									GNg::ImgScale(icon_data, zIconW, zIconH, t_data, tw, th, px_byte);
                                   #endif
								}else{
									if(tw == zIconW && th == zIconH)
										GNg::ImgFmtConvertTs(icon_data,GNg::CR_RGBA, t_data,cr_format, zIconW * zIconH);
									else{
                                       #ifndef GT_FB_ICON_FILL_FULL
                                        if(_tw == zIconW && _th == zIconH)
											GNg::ImgScale(tmp_buf, zIconW, zIconH, t_data, tw, th, px_byte);
										else{
                                            memset(tmp_buf, 0, px_byte * zIconW * zIconH);
										    GNg::ImgScaleBlt(t_data, tw, 0, 0, tw, th, tmp_buf, zIconW, _tw == zIconW ? 0 : ((zIconW-_tw)>>1), _th == zIconH ? 0 : ((zIconH-_th)>>1), _tw, _th, cr_format);
										}
                                       #else
                                        GNg::ImgScale(tmp_buf, zIconW, zIconH, t_data, tw, th, px_byte);
                                       #endif
										GNg::ImgFmtConvertTs(icon_data,GNg::CR_RGBA, tmp_buf,cr_format, zIconW * zIconH);
							    } 	}
							}else{	   //不含透明的图标处理 必须转换颜色格式
								memcpy(icon_data, GT_FbIconBackData, icon_dsize);
								if(tw == GT_FbPicInIconRange.right && th == GT_FbPicInIconRange.bottom) //图片尺寸与想要的尺寸大小匹配
									GNg::ImgTsBlt(t_data, tw, 0, 0, tw, th, icon_data, zIconW, GT_FbPicInIconRange.left, GT_FbPicInIconRange.top, cr_format, GNg::CR_RGBA);
								else{  //把读出的图象填充到中间
									GNg::ImgScale(tmp_buf, GT_FbPicInIconRange.right, GT_FbPicInIconRange.bottom, t_data, tw, th, px_byte);
									GNg::ImgTsBlt(tmp_buf, GT_FbPicInIconRange.right, 0, 0, GT_FbPicInIconRange.right, GT_FbPicInIconRange.bottom, icon_data, zIconW, GT_FbPicInIconRange.left, GT_FbPicInIconRange.top, cr_format, GNg::CR_RGBA);
								}      //加入反光膜
								if(GT_FbIconFrameData)
									GNg::ImgBlendRGBAtoRGBAusNormal(GT_FbIconFrameData, GT_FbIconFrameW, 0, 0, GT_FbIconFrameW, GT_FbIconFrameH, icon_data, zIconW, GT_FbIconFrameX, GT_FbIconFrameY);
							}
				        }
                           zDatas[fi] = icon_data;
					}else zStates[fi] = GT_eFB_ICON_PROC_NONE; //不支持的颜色格式也标记为下次不读取
				}
			    if(t_data)delete [] t_data;
			}else zStates[fi] = GT_eFB_ICON_PROC_NONE; //读取文件失败也标记为下次不读取
	}
	delete [] tmp_buf;
	_task->us_state = 255;
}
static void GT_FB_IconDataLoadAndGen(P<GN_sTHREAD> this_thread, void *arg, void *arg2){
   ((GT_cFILE_PREVIEW_GEN*)arg)->IconDataLoadAndGenProc();
}
/*--------------------------------------------------------------- 导出函数 -------------------------------------------------------------------------*/

//默认当文件浏览对象创建时的处理(文件浏览对象)返回一个和文件浏览对象的关联对象
void* GNf_FB_DefOnCreateProc(void* fb){
	if(!GN_ThreadSupport)return 0;
	return GT_FilePreviewGenList.add(GT_cFILE_PREVIEW_GEN(fb));
}
//默认当文件浏览对象被销毁时的处理(关联对象)
void  GNf_FB_DefOnDestroyProc(void* relate_id){
	GT_FilePreviewGenList.del((GT_cFILE_PREVIEW_GEN*)relate_id);
}
//默认当文件浏览有文件或目录图标要生成时的处理(关联对象, 当前要准备显示的文件夹和文件总数, 所包含的文件夹和文件全局路径数据, 对应每个文件夹和文件所分配到的图标号数据, 是否查找结果, 显示图标宽度, 高度)
void  GNf_FB_DefOnIconGenProc(void* relate_id, int fn, const char **paths, char *icon_ids, bool is_search_result, int icon_w, int icon_h){
	((GT_cFILE_PREVIEW_GEN*)relate_id)->OnIconGen(fn, paths, icon_ids, is_search_result, icon_w, icon_h);
}
//默认当文件浏览的文件或目录图标要清除时的处理(关联对象) !这个通常在新一批图标要准备生成时或该类对象销毁前被调用到
void  GNf_FB_DefOnIconClearProc(void* relate_id){
	((GT_cFILE_PREVIEW_GEN*)relate_id)->OnIconClear();
}
//默认当图标被重新排列时的处理(关联对象, 当前打开目录里包含的文件夹和文件总数, 首个图标相当于文件夹和文件总数的第几个, 末尾图标相当于文件夹和文件总数的第几个)
void GNf_FB_DefOnIconsReArrangeProc(void *relate, int fn, int first, int last){
    
}
//默认当该类对象更新显示时的处理(关联对象, 每帧间隔时间)
void GNf_FB_DefOnUpdateProc(void *relate, unsigned int updatetime){

}
//默认当文件浏览图标更变数据要检测是否准备好时的处理(关联对象, 当前图标属于第几个文件夹或文件, 是否被选中, 显示图标宽度, 高度, 当前图标对应文件或目录的全局路径)返回一个通知状态值
int   GNf_FB_DefOnIconDataOkCheckProc(void* relate_id, int fi, int is_sed, int icon_w, int icon_h, const char *path){
	return ((GT_cFILE_PREVIEW_GEN*)relate_id)->OnIconDataOkCheck(fi, is_sed, icon_w, icon_h, path);
}
//默认当文件浏览图标更变需要获取数据时的处理(关联对象, 当前图标属于第几个文件夹或文件, 是否被选中, 当前图标对应文件或目录的全局路径)返回图标的图象数据
char* GNf_FB_DefOnIconDataGetProc(void* relate_id, int fi, int is_sed, const char *path){
	return ((GT_cFILE_PREVIEW_GEN*)relate_id)->OnIconDataGet(fi, is_sed, path);
}
//默认当文件浏览图标获取到数据后更变了时的处理(关联对象, 当前图标属于第几个文件夹或文件, 是否被选中, 当前图标对应文件或目录的全局路径)
void  GNf_FB_DefOnIconDataChangedProc(void* relate_id, int fi, int is_sed, const char *path){
    ((GT_cFILE_PREVIEW_GEN*)relate_id)->OnIconDataChanged(fi, is_sed, path);
}
//默认当图标正在绘制时的处理(关联对象, 当前图标属于第几个文件夹或文件, 是否被选中, 图标显示左上角坐标X, Y, 图标显示宽度,高度, 图标显示色调, 图标图层ID, 当前图标对应文件或目录的全局路径)返回是否要隐藏对应的原图标
bool GNf_FB_DefOnIconDrawProc(void *relate, int fi, int is_sed, int x, int y, int w, int h, unsigned int  color, unsigned short tid, const char *path){
	return false;
}
/******************************************************** cFILE_BROWSER *************************************************************/

#define GT_FB_ACT_ICON_MAX_NUM 512  //动态预览图标最大缓冲个数 估计消耗 8M显存

struct GT_sFB_ACT_ICON_DATA //文件浏览处理动态图标的临时数据
{
    GNg::sIMAGE                icon;       //文件动态图标组
	P<uint8>                   states;     //跟踪记录对应每个文件或目录的动态图标数据状态 0:没有数据不再需要数据 1:没有数据需要数据 2:已有数据还继续需要数据 3:已有数据不再需要数据
	P<unsigned short>          icon_ids;   //记录对应每个文件或目录使用的是第几个动态图标
	unsigned short             ir_min;     //记录有引用到图标对应的最细位置号
	unsigned short             ir_max;     //记录有引用到图标对应的最大位置号
	unsigned short             can_us_id;  //当前可使用动态图标号
	unsigned short             sr_last;    //记录上次的可见图标起始位置号
	bool                       gen_swap;   //标记是否反向产生动态图标
	bool                       full;       //图标是否占满了, 这个通常是图标缓冲小而造成的

	//初始化文件浏览的动态图标( 显示图标的缓冲数 )
	void InitToData(unsigned short img_num = GT_FB_ACT_ICON_MAX_NUM);

	//重设一些状态(要显示的文件或目录总数)
	void ReSet(unsigned int fn);
};
void GT_sFB_ACT_ICON_DATA::InitToData(unsigned short img_num){
    GNg::sIMAGE *_icon_img = GNu::cFILE_BROWSER::GetIcon();
	icon.CreateEmptyToData(_icon_img->width, _icon_img->height, img_num, GNg::CR_RGBA);
	GT_TMP_FILL_EMPTY_IMG_DATA( _icon_img )
}
void GT_sFB_ACT_ICON_DATA::ReSet(unsigned int fn){
    icon_ids.New(fn, (unsigned short)0);
    states.New(fn, (uint8)1);
	ir_min = 0xffff;
    ir_max = 0;
	can_us_id = 1;
	sr_last = 0;
	gen_swap = false;
	full = false; //GN_RunReportError();
}
/********************************************************************** cFILE_BROWSER *******************************************************************************************************************/
void* (*GNu::cFILE_BROWSER::aOnCreateCallBack)(void* fb_hinstance) = GNf_FB_DefOnCreateProc;
void  (*GNu::cFILE_BROWSER::aOnDestroyCallBack)(void* relate_id) = GNf_FB_DefOnDestroyProc;
void  (*GNu::cFILE_BROWSER::aOnIconGenCallBack)(void* relate_id, int fn, const char **paths, char *icon_ids, bool is_search_result, int icon_w, int icon_h) = GNf_FB_DefOnIconGenProc;
void  (*GNu::cFILE_BROWSER::aOnIconClearCallBack)(void* relate_id) = GNf_FB_DefOnIconClearProc;
void  (*GNu::cFILE_BROWSER::aOnIconsReArrangeCallBack)(void* relate_id, int fn, int first, int last) = GNf_FB_DefOnIconsReArrangeProc;
void  (*GNu::cFILE_BROWSER::aOnUpdateCallBack)(void* relate_id, unsigned int updatetime) = GNf_FB_DefOnUpdateProc;
int   (*GNu::cFILE_BROWSER::aOnIconDataOkCheckCallBack)(void* relate_id, int fi, int is_sed, int icon_w, int icon_h, const char *path) = GNf_FB_DefOnIconDataOkCheckProc;
char* (*GNu::cFILE_BROWSER::aOnIconDataGetCallBack)(void* relate_id, int fi, int is_sed, const char *path) = GNf_FB_DefOnIconDataGetProc;
void  (*GNu::cFILE_BROWSER::aOnIconDataChangedCallBack)(void* relate_id, int fi, int is_sed, const char *path) = GNf_FB_DefOnIconDataChangedProc;
bool  (*GNu::cFILE_BROWSER::aOnIconDrawCallBack)(void* relate_id, int fi, int is_sed, int x, int y, int w, int h, unsigned int  color, unsigned short tid, const char *path) = GNf_FB_DefOnIconDrawProc;

bool GT_FbShowPreview = false;
void GNu::cFILE_BROWSER::SetShowPreview(bool yes){
	GT_FbShowPreview = yes;
}
void GNu::cFILE_BROWSER::UsShowPreview(bool yes){
	if(yes){
		if(!zRelate)zRelate = aOnCreateCallBack(this);
	}else{
		if( zRelate){ aOnDestroyCallBack(zRelate); zRelate = 0; }
	}
}
#else
void* (*GNu::cFILE_BROWSER::aOnCreateCallBack)(void* fb_hinstance) = 0;
void  (*GNu::cFILE_BROWSER::aOnDestroyCallBack)(void* relate_id) = 0;
void  (*GNu::cFILE_BROWSER::aOnIconGenCallBack)(void* relate_id, int fn, const char **paths, char *icon_ids, bool is_search_result, int icon_w, int icon_h) = 0;
void  (*GNu::cFILE_BROWSER::aOnIconClearCallBack)(void* relate_id) = 0;
void  (*GNu::cFILE_BROWSER::aOnIconsReArrangeCallBack)(void* relate_id, int fn, int first, int last) = 0;
void  (*GNu::cFILE_BROWSER::aOnUpdateCallBack)(void* relate_id, unsigned int updatetime) = 0;
int   (*GNu::cFILE_BROWSER::aOnIconDataOkCheckCallBack)(void* relate_id, int fi, int is_sed, int icon_w, int icon_h, const char *path) = 0;
char* (*GNu::cFILE_BROWSER::aOnIconDataGetCallBack)(void* relate_id, int fi, int is_sed, const char *path) = 0;
void  (*GNu::cFILE_BROWSER::aOnIconDataChangedCallBack)(void* relate_id, int fi, int is_sed, const char *path) = 0;
bool  (*GNu::cFILE_BROWSER::aOnIconDrawCallBack)(void* relate_id, int fi, int is_sed, int x, int y, int w, int h, unsigned int  color, unsigned short tid, const char *path) = 0;

void GNu::cFILE_BROWSER::SetShowPreview(bool yes){}
void GNu::cFILE_BROWSER::UsShowPreview(bool yes){}
#endif
extern bool    GT_ImeOnRecv;
extern uint8   GT_ImeCount;
extern char    GT_ImeChar[];
P<GNg::cFRAME> GT_FB_DefaultFrameAct;  //默认点中变化效果筐
P< P<char> >   GT_FB_ExtNameTable;	   //保存扩展名列表

float          GNu::cFILE_BROWSER::aCurProcFileProgress = 1.0f;
char          *GNu::cFILE_BROWSER::aCurProcFileName = "\0";
unsigned short GNu::cFILE_BROWSER::zCreateTotalNum = 0;
GNf::sFILE*  (*GNu::cFILE_BROWSER::aOnFileCopyBeginCallBack)(const char*, const char*, void*, void*) = 0;
void         (*GNu::cFILE_BROWSER::aOnFileCopyEndCallBack)  (const char*, const char*, void*, void*) = 0;

GNu::cFILE_BROWSER::cFILE_BROWSER():zRelate(0),zActIconTmpData(0),zClickLastFile(0),zShowReady(0),zReArrange(0),zReLoadIcon(1), zIsFindIng(0), zReGenIcon(0),zReArrangeParame(0),zSortType(eFILE_SORT_NAME),
aDisableControl(0),aIgnoreSED(0), aShowOffsetX(0),aShowOffsetY(0),aSearchTotalSizeB(0),aSearchTotalSizeGB(0),aSearchTotalFileN(0),aSearchTotalDirN(0)
{
    aType = GNu::eUI_FILE_BROWSER; c = 0xffffffff /*该类忽略该值*/;
	++zCreateTotalNum;
}
GNu::cFILE_BROWSER::~cFILE_BROWSER(){
	zShowReady = false;
	if(zTkThread)GN_ThreadEndWait(zTkThread);
   #ifdef GN_USING_FILE_PREVIEW
	if(zRelate){
		aOnDestroyCallBack(zRelate); 
		zRelate = 0;
	}
	if(zActIconTmpData){ delete (GT_sFB_ACT_ICON_DATA*)zActIconTmpData; zActIconTmpData = 0; }
   #endif
	--zCreateTotalNum;
	if(!zCreateTotalNum){ //当已经没该类对象时就删除共享内容
	   #ifdef GN_USING_FILE_PREVIE
        GT_FbIconOptRelease();
	   #endif
		GT_FB_ExtNameTable.Delete();
	    GT_FB_DefaultIconImg.Delete();
		if(GT_FB_DefaultFrameAct){
			GNg::sAM_EVENT *_fev = GT_FB_DefaultFrameAct->zFEvent;
		    GN_DeleteArray(_fev);
		    GT_FB_DefaultFrameAct.Delete();
		}
	}
}
void GNu::cFILE_BROWSER::InitNoUiMode(){
	zShowReady = true;
}
bool GNu::cFILE_BROWSER::InitStyleToData(const GNg::cTEXT_ART *text_style,const char * open_dir,
short x,short y,short w,short h, short tw, short th, eNEAR_TYPE near_pos,
unsigned int text_color,unsigned int focus_color,unsigned short text_blend,unsigned short focus_blend){
    zTextOut.Release();    //释放之前的内部资源
	if(text_style){
		((GNg::cTEXT_ART*)text_style)->Release();
		zTextOut = (*text_style);
	}else if(GN::RunRec.zGWPrintReady){
		GN::RunRec.aFont.Release(); 
		zTextOut = GN::RunRec.aFont; //没有指示文本式样的时候 尝试使用调试信息的文本式样来显示
	}else{
		zShowReady = false;
	    return false;
	}
	aTextColor = text_color ? text_color : GT_AppDefaultColorFont;
	aFocusColor   = focus_color;
    aTextBlend    = text_blend;
	aFocusBlend   = focus_blend;
	zShowReady = true;
	zInitInternalObj(x,y,w,h,tw);
	zTextW = tw ? tw : zFFrames->zParame.width;
	zTextH = th ? th : zFFrames->zParame.height;
	zNearPos = near_pos != eNEAR_NULL? near_pos : eNEAR_BOTTOM;

    GNf::GetAppCurPath(zLastDir); //获取保能打开的应用程序当前目录
	strncpy(aOpenDir, open_dir?open_dir:zLastDir, sizeof(aOpenDir)-1); aOpenDir[sizeof(aOpenDir)-1] = 0;

	zReArrange       = true;
	zReArrangeParame = true;
	zReLoadIcon      = true;
	zOffsetSheep	 = 0.1f;
	oW = GN_INT_MAX;
	aUsMultiSelect   = false;
	return zShowReady;
}
void GNu::cFILE_BROWSER::SetEffect(short focus_flash_time, P<GNg::cFRAME_LAUNCHER_FONT> effect, P<GNg::sIMAGE> focus_style, P<GNg::sIMAGE> offset_arrow,int8 repeat_num){
	GT_TMP_UI_SET_EFFECT;
	oW = GN_INT_MAX;
	zReArrangeParame = true;
	zReLoadIcon      = true;
}
GNg::sIMAGE * GNu::cFILE_BROWSER::GetIcon(){
    if(!GT_FB_DefaultIconImg){
		GNf_DirStatePush(false);
		int fsize; // 读取默认能识别的文件扩展名列表 -------------------------------------------------------------------------
		char *ini_filename = GN_SysDir("fileicon/file_icon.ini");
		char *ini_file_buf = GNf::FileFullLoadToMemoryText(ini_filename, &fsize);
		if(ini_file_buf){
            GT_FB_ExtNameTable = GN_StrSplitToData(ini_file_buf,'\n'); //分割出各个扩展名放到字符表里
			delete [] ini_file_buf;

			if(GT_FB_ExtNameTable.Num()){
				GT_FB_DefaultIconImg.New(1);
				GNg::sIMAGE *_img = GT_FB_DefaultIconImg; // 读取默认文件图标 -----------------------------------------
				if(GNg::ImageLoad(GN_SysDir("fileicon/file_icon"),(unsigned short)GT_FB_ExtNameTable.Num()+2, _img)){
					GNg::sIMAGE icon;
					icon.CreateEmptyToData(_img->width, _img->height, 1, _img->cFormat);   //创建空图
					GT_TMP_FILL_EMPTY_IMG_DATA( _img )
					unsigned int *texture1 = new unsigned int[_img->texnum + 1];
					memcpy(texture1, _img->texture, sizeof(unsigned int) * _img->texnum);  //合并空图到文件图标图末尾
					texture1[_img->texnum] = icon.texture[0];
					delete [] _img->texture; _img->texture = texture1;
					delete [] icon.texture;	  icon.texture = 0;
				}else GN_Log("读取文件浏览默认文件图标失败!\n");
			}
		}else GN_Log("读取文件浏览默认能识别的文件扩展名列表失败! 找不到文件: %s\n",ini_filename);
        GNf_DirStatePop();
	}
	if(GT_FB_DefaultIconImg)return (GNg::sIMAGE*)GT_FB_DefaultIconImg;
	else return 0;
}
void GNu::cFILE_BROWSER::zInitInternalObj(short fx, short fy, short fw, short fh, short tw){
	GT_TMP_UI_OffsetArrowInit();
	if(!zFFrames){
		if(!GT_FB_DefaultFrameAct){
			GNg::sAM_EVENT *fev = new GNg::sAM_EVENT[1];
			fev->Init(GNg::eEVENT_SIZE_X|GNg::eEVENT_SIZE_Y, 0,120,1.0f,1.3f);
		    GT_FB_DefaultFrameAct.New(1);
			GT_FB_DefaultFrameAct->Init(0, 1,fev GT_TMP_DEF_FRAME_INIT_ARGS_ONCE_KEEP);
		}
		if(!zFrameIcon){
			GNg::sIMAGE *p_img = GetIcon();
			if(!p_img){ zShowReady = false; return; }
			zFrameIcon.New(1);
			zFrameIcon->Init(p_img, 0,0 GT_TMP_DEF_FRAME_INIT_ARGS_ONCE_KEEP);
		}
		zFFrames.New(1);
		GNg::cFRAME *_frame_icon = zFrameIcon;
		short w2 = _frame_icon->zPlay_obj->width;
		if(zNearPos == eNEAR_TOP || zNearPos == eNEAR_BOTTOM)
		     w2 = ( w2>>1);
		else w2 = ((w2>>1)+(tw?tw:w2));

		GNg::sARRANGE_FMT afmt(0,0.0f,0.0f,w2,0,0,0,GNg::eENTER_LEFT_RIGHT, 0,0);
		GNg::sRECT2 arc2d((float)fx,(float)fy,(float)(fx+fw),(float)(fy+fh));
		zFFrames->InitToData(_frame_icon, GNg::cFRAME_LAUNCHER_FONT::aPicIndex,1, &arc2d, &afmt, 0.0f,1000,0,127,0,127,127,0,0,0,GT_FB_DefaultFrameAct,true);
	
	}else SetPos(fx,fy,fw,fh);
}
void GNu::cFILE_BROWSER::zSetIconChange(unsigned int fid,unsigned int pid){
	GNg::cFRAME_LAUNCHER_FONT *_fframe = zFFrames;
	if(zUsMultiSelect){ //多选
	    if(zSelected[pid]){
			_fframe->CancelActChange(fid); zSelected[pid] = 0;
		}else{
			_fframe->SelectActChange(fid); zSelected[pid] = 1;
            if(zPicID[pid] != 1 && fid < zLastSelectFID)zLastSelectFID = fid;
		}
	}else{				//单选
			_fframe->SelectActChange(fid); zSelected[pid] = 1;
			zCurSelectID = pid;
			if(zPicID[pid] == 1)zLastSelectFID = 0xffffffff;
			else				zLastSelectFID = fid;
	}
}
int8 GNu::cFILE_BROWSER::zDoubleClickOpen(unsigned int fid){
    unsigned int pid = (unsigned int)zFFrames->zDrawRngData[fid].strid;
	if(zPicID[pid] == 1){		//检测当前点到的是文件夹吗
		if      (!strcmp((char*)zFileNames[pid],"<-LAST")){   //返回上一层特殊文件夹
			GN_StrRemoveFileName(aOpenDir);
		}else if(!strcmp((char*)zFileNames[pid],"<-RETURN")){ //返回上次可打开的文件夹
		    strncpy(aOpenDir, zLastDir, sizeof(aOpenDir)-1); aOpenDir[sizeof(aOpenDir)-1] = 0;
		}else{					//进入选中的子文件夹
		    int remaining = sizeof(aOpenDir) - strlen(aOpenDir) - 1;
		    strncat(aOpenDir, "/", remaining);
		    remaining = sizeof(aOpenDir) - strlen(aOpenDir) - 1;
		    strncat(aOpenDir, zFileNames[pid], remaining); //把子文件夹续在当前目录下 以便待会对这子文件夹打开
		    aOpenDir[sizeof(aOpenDir)-1] = 0;
		}
        zReArrange = 1; zReLoadIcon = 1; //标记需要重新排列图标
		return -1;
	}
    return 1;
}
inline int GT_FileSortFormName(GNf::sFILE_INFO * left, GNf::sFILE_INFO * right){
    char chr1 = left->name[0], chr2 = right->name[0];
	if(!(chr1&128) && !(chr2&128)){
	    if(chr1 <= 'Z' && chr1 >= 'A')chr1 += 'a' - 'A'; //把大写转为小写
	    if(chr2 <= 'Z' && chr2 >= 'A')chr2 += 'a' - 'A';
	    if     (chr1 < chr2)return 1;
	    else if(chr1 > chr2)return 0;
	}
	return (strcmp(left->name, right->name) < 0);
}
inline int GT_FileSortFormClass(GNf::sFILE_INFO *left, GNf::sFILE_INFO *right){
	char *ext1 = GN_StrGetExtName(left->name), *ext2 = GN_StrGetExtName(right->name);
	if(ext1 && ext2)return (strcmp(ext1 , ext2) < 0 );
	else return (ext1? 0 : 1);
}
inline int GT_FileSortFormSize(GNf::sFILE_INFO *left, GNf::sFILE_INFO *right){
	return (left->size < right->size);
}
inline int GT_FileSortFormData(GNf::sFILE_INFO *left, GNf::sFILE_INFO *right){
	return (left->time_write < right->time_write);
}
void GNu::cFILE_BROWSER::RePop(){
    if(zTkThread)return;
	if(zFFrames)zFFrames->zObjNowtime = 0;
    zReLoadIcon = true; zReArrange = true;
}
void GNu::cFILE_BROWSER::SetOpenDirectory(const char *open_dir){
	if(zTkThread)return;
	if(open_dir){ strncpy(aOpenDir, open_dir, sizeof(aOpenDir)-1); aOpenDir[sizeof(aOpenDir)-1] = 0; }
	else GNf::GetAppCurPath(aOpenDir); //取保能打开的应用程序当前目录
    zReLoadIcon = true; zReArrange = true;
}
void GNu::cFILE_BROWSER::SetSortMode(eFILE_SORT_TYPE sort_type){
	if(zTkThread || zSortType == sort_type)return;
    zSortType  = sort_type;
	zReGenIcon = true; zReArrange = true;
}
void GNu::cFILE_BROWSER::ClearCurIconData(){
    if(zTkThread)return;
   #ifdef GN_USING_FILE_PREVIEW
    if(zActIconTmpData){ delete (GT_sFB_ACT_ICON_DATA*)zActIconTmpData; zActIconTmpData = 0; }
    if(zRelate)aOnIconClearCallBack(zRelate);
    zGlobalPaths_.Delete(); zGlobalPaths.Delete();
   #endif
    zReLoadIcon = true; zReArrange = true;
}
void GNu::cFILE_BROWSER::zSearch(P< P<char> > &sm, const char *cur_dir, bool search_sub_dir, void *ph){
    GN::sLIST< P<GNf::sFILE_INFO> >::iterator idir;
	bool has_dir;
	if(search_sub_dir){
		if(zpSearchDirList->size()){
			idir = --(zpSearchDirList->end());
			  has_dir = true;
		}else has_dir = false;
	}
	unsigned int dir_num = 0, si, smn, cn;
	char open_dir[GN_MAXFILEPATH], *_fname, *com1, *com2;
	sprintf(open_dir,"%s/*.*",cur_dir);
	GN::sLIST< P<GNf::sFILE_INFO> > file_list, dir_list;
	if(GNf::FindFileDirInfoData(open_dir,true, &file_list, &dir_list,ph) != -1){
		if(!zShowReady){ //收到对象销毁结束通知
			dir_list.clear(); file_list.clear();
			return;
		}
		GN::sLIST< P<GNf::sFILE_INFO> >::iterator itfo, etfo;
        GNf::sFILE_INFO *_finfo;
		if(dir_list.size()){
			itfo = dir_list.begin(), etfo = dir_list.end();
			for( ; itfo != etfo; ++itfo){
			    _finfo = (*itfo);
				_fname = _finfo->name;
                if(_fname[0] == '.' && ((_fname[1]=='.' && !_fname[2]) || !_fname[1]));
				else{
				    zpSearchDirList->push_back(*itfo);
					++aSearchTotalDirN;
				    ++dir_num;
			}	}
			dir_list.clear();
		}
        if(file_list.size()){
			etfo = file_list.end();
			for(itfo = file_list.begin(); itfo != etfo; ++itfo){
			    _finfo = (*itfo);
				_fname = _finfo->name;
				smn = sm.Num();
			    if(smn){  //按分组来核对需要保留的文件
					for(si = 0; si < smn; ++si){
						com1 = (char*)sm[si]; com2 = _fname;
                        cn = strlen(com1);
						while(*com2){
							if(!strncmp(com1, com2, cn)){ //检索到有相对应的名称
					            zpSearchFileList->push_back(*itfo);
                                TotalFileSizeAdd(_finfo->size);
								break;
							}
						    ++com2;
						}
                        if(*com2)break;
					}
				}else{	  //收集所有文件
					zpSearchFileList->push_back(*itfo);
					TotalFileSizeAdd(_finfo->size);
			}	}
			file_list.clear();
		}
	}
	if(!zShowReady)return; //收到对象销毁结束通知
	if(search_sub_dir && dir_num){
		if(has_dir)++idir;
		else idir = zpSearchDirList->begin();
		for(si = 0; si < dir_num; ++si,++idir){
			if(!zShowReady)return; //收到对象销毁结束通知
		    zSearch(sm, (*idir)->global_filename, search_sub_dir, ph); //递归进入查找里面的所有子目录
	}	}
}
void GNu::cFILE_BROWSER::TotalFileSizeAdd(size_t f_size){
	++aSearchTotalFileN;
	size_t _size = GN_SIZE_1GB - aSearchTotalSizeB;
	if(f_size < _size)aSearchTotalSizeB += f_size;
	else if(f_size > _size){
	    ++aSearchTotalSizeGB;
        f_size -= _size;
		size_t gb_num = f_size / GN_SIZE_1GB;
        aSearchTotalSizeGB += gb_num;
        aSearchTotalSizeB = f_size - (gb_num * GN_SIZE_1GB);
	}else{
		++aSearchTotalSizeGB;
        aSearchTotalSizeB = 0;
	}
}
void GNu::cFILE_BROWSER::zSearch__(const char *dir, const char *search_mode, bool search_sub_dir,void (*proc)(GNf::sFILE_INFO * file_info), void *ph){
	P< P<char> > sm;
	if(search_mode){ //..分析这字串所代表有多少个查找用意
        sm = GN_StrSplitToData(search_mode,',');
		unsigned int si = 0, i = 0, smn = sm.Num();
		P<char> *_sm = sm;
		P< P<char> > sm_t(smn);
		char *pstr;
		for(; si < smn; ++si){
		    pstr = GN_StrToKey(_sm[si], '.');
			if(pstr){
				if(*pstr == '*' && *(pstr-2) == '*'); //忽略*.* 
				else if(*pstr      == '*'){
					*pstr = 0;
					sm_t[i++]((char*)_sm[si], 0);	  //将 pic.* 变为 pic. 来查找
				}else if(*(pstr-2) == '*'){
					sm_t[i++](pstr, 0);				  //将 *.jpg 变为 jpg 来查找
				}else{
					sm_t[i++]((char*)_sm[si], 0);	  //   pic.jpg 可不变来查找
				}
			}else{
					sm_t[i++]((char*)_sm[si], 0);	  //   pic	   可不变来查找
		}	}
		if(i)sm = sm_t.CopyOut((int)i - (int)smn, P_()); //得到需要查找核对的分组
		else sm.Delete();
	}
	if(!zShowReady)return; //收到对象销毁结束通知
	if(proc){
        GN::sLIST< P<GNf::sFILE_INFO> >  proc_dir_list, proc_file_list;
        zpSearchDirList = &proc_dir_list; zpSearchFileList = &proc_file_list;
		zSearch(sm, dir, search_sub_dir, ph); //开始递归进入查找里面的所有子目录
		if(!zShowReady){ proc_dir_list.clear(); proc_file_list.clear(); return; } //收到对象销毁结束通知
       
		GN::sLIST< P<GNf::sFILE_INFO> >::iterator itf, etf;
		if(aIgnoreSED && !zIsDirDelOrCopy)proc_dir_list.clear();
		else{
            itf = proc_dir_list.begin(), etf = proc_dir_list.end();
			for(; zShowReady && itf != etf; ++itf)proc((*itf));
		}
		itf = proc_file_list.begin(), etf = proc_file_list.end();
		for(; zShowReady && itf != etf; ++itf)proc((*itf));

		if(zShowReady)OnSearchProcFinish((unsigned int)(proc_dir_list.size() + proc_file_list.size()));
        proc_dir_list.clear(); proc_file_list.clear();
	}else{
		zDirList.clear(); zFileList.clear(); zInfoP.Delete();
        zpSearchDirList = &zDirList; zpSearchFileList = &zFileList;
		zSearch(sm, dir, search_sub_dir, ph); //开始递归进入查找里面的所有子目录
        zDirList.clear(); //这种查找不显示目录内容就把它删掉
        
		//加入一个返回目录按钮
		P<GNf::sFILE_INFO> pfile(1);
		GNf::sFILE_INFO *_finfo = pfile;
		strncpy(_finfo->name, "..", sizeof(_finfo->name)-1); _finfo->name[sizeof(_finfo->name)-1] = 0;
        int remaining = sizeof(aOpenDir) - strlen(aOpenDir) - 1;
        strncat(aOpenDir, "/search_result", remaining); aOpenDir[sizeof(aOpenDir)-1] = 0;
		strncpy(_finfo->global_filename, aOpenDir, sizeof(_finfo->global_filename)-1);
		_finfo->global_filename[sizeof(_finfo->global_filename)-1] = 0;
		char *_pstr = _finfo->global_filename + strlen(_finfo->global_filename);
		*_pstr = '/'; ++_pstr;
		remaining = sizeof(_finfo->global_filename) - (int)(_pstr - _finfo->global_filename) - 1;
		strncpy(_pstr, _finfo->name, remaining);
		_finfo->global_filename[sizeof(_finfo->global_filename)-1] = 0;
		_finfo->attrib = _A_SUBDIR;
		_finfo->size = 0;
		_finfo->time_access = 0;
		_finfo->time_create = 0;
		_finfo->time_write  = 0;
		zDirList.push_back(pfile);
        zReGenIcon = 2;
	}
}
void GT_FILE_BROWSER_FindProc(P<GN_sTHREAD> this_thread, void *arg, void *arg2){
    GNu::cFILE_BROWSER *fb = (GNu::cFILE_BROWSER*)arg;
    fb->zSearch__(fb->zTkArgSDir, fb->zTkArgSMode, fb->zTkArgSSubDir, fb->zTkArgFunc, fb->zTkArgPH);
	if(this_thread)this_thread->us_state = 255;
}
void GNu::cFILE_BROWSER::Search(bool wait, char *dir, const char *search_mode, bool search_sub_dir, void (*proc)(GNf::sFILE_INFO * file_info)){
	if(GN_ThreadSupport){
		if(zTkThread)return;
	    if(!wait){ 
           #define GT_TMP_FB_SearchResultDirRemove() \
			if(!dir){\
                dir = aOpenDir;\
                char *endpath = GN_StrRemoveFileName(dir); \
	            if(dir < endpath && strcmp(endpath,"search_result"))*(endpath-1) = '/';\
			}
			GT_TMP_FB_SearchResultDirRemove(); //移除查找路径
            zTkArgSMode(search_mode, 0);
			zTkArgSDir(dir, 0);
            zTkArgSSubDir = search_sub_dir;
			zTkArgPH = GNf::sFILE::DefaultH;
            zTkArgFunc = proc;
		    zTkThread = GN_ThreadStart(GT_FILE_BROWSER_FindProc, this, 0);
			if(!proc){ OkHide(); zIsFindIng = true; }
			return;
		}
	}
	GT_TMP_FB_SearchResultDirRemove();
	zIsDirDelOrCopy = false;
	zSearch__(dir, search_mode, search_sub_dir, proc, GNf::sFILE::DefaultH);
}
void GNu::cFILE_BROWSER::Search_exec(bool wait, char * dir, void (*proc)(GNf::sFILE_INFO * file_info)){
    if(!zTkThread)Search(wait, dir,"*.*", true, proc);
}
static GNu::cFILE_BROWSER *GT_FileBrowserLast = 0;

static void GT_FileBrowserDelFile(GNf::sFILE_INFO *file_info){
    if(!(file_info->attrib & _A_SUBDIR)){
		GNu::cFILE_BROWSER::aCurProcFileName = file_info->global_filename;
		if(GNf::Del  (file_info->global_filename, GT_FileBrowserLast->zTkArgPH))GT_FileBrowserLast->zTkArgSSubDir = 0; //标记有错
}	}
static void GT_FileBrowserDelDir(GNf::sFILE_INFO *file_info){
	if(file_info->attrib & _A_SUBDIR){
		GNu::cFILE_BROWSER::aCurProcFileName = file_info->global_filename;
	    if(GNf::Rmdir(file_info->global_filename, GT_FileBrowserLast->zTkArgPH))GT_FileBrowserLast->zTkArgSSubDir = 0; //标记有错
}	}
void GT_FILE_BROWSER_FindDel(P<GN_sTHREAD> this_thread, void *arg, void *arg2){
    GNu::cFILE_BROWSER *fb = (GNu::cFILE_BROWSER*)arg;
    void *ph  = fb->zTkArgPH;
	char *dir = fb->zTkArgSDir;
	GNf_DirStatePush(true);
	GT_FileBrowserLast = fb;
	fb->zIsDirDelOrCopy = true;
	fb->zTkArgSSubDir = 1; //临时用该值来标记是否无错
	fb->zSearch__(dir, "*.*", true, GT_FileBrowserDelFile, ph);
	if(fb->zTkArgSSubDir){    //无错
		uint8 i = 0;
		for(; i < 64; ++i){
			fb->zTkArgSSubDir = 1;
            fb->zSearch__(dir, "*.*", true, GT_FileBrowserDelDir, ph);
			if(fb->zTkArgSSubDir)break;
		}
        if(GNf::Rmdir(dir, ph))fb->zTkArgSSubDir = 0;
	}
    GNf_DirStatePop();
	if(!fb->zTkArgSSubDir){
		GN_ErrorLast.New(GN_MAXFILEPATH);
		sprintf(GN_ErrorLast,"删除目录: %s 途中有错!", dir);
	}
	if(this_thread)this_thread->us_state = 255;
}
void GNu::cFILE_BROWSER::DelDir(bool wait, const char *dir, void *ph){
    zTkArgPH = ph;
	zTkArgSDir(dir, 0);
	if(GN_ThreadSupport){
		if(zTkThread)return;
	    if(!wait){
			zTkThread = GN_ThreadStart(GT_FILE_BROWSER_FindDel, this, 0);
            return;
	}	}
    GT_FILE_BROWSER_FindDel(P_(), this, 0);
}
int GNu::cFILE_BROWSER::FileCopy(const char *dst, const char *src, void *ph_dst, void *ph_src){
	aCurProcFileName = (char*)src;
	GNf_DirStatePush(true);
	GNf::sFILE *fp_src;
	if(aOnFileCopyBeginCallBack){
		fp_src = aOnFileCopyBeginCallBack(dst, src, ph_dst, ph_src);
		if(!fp_src){ GNf_DirStatePop(); return 0; }
	}else   fp_src = GNf::Open((char*)src, "rb", GNf::eFILE_STD, ph_src);
	int ret = 0;
	if(fp_src){
		GNf::sFILE *fp_dst = GNf::Open((char*)dst, "wb", GNf::eFILE_STD, ph_dst);
		if(fp_dst){
			int fsize = GNf::GetSize(fp_src), max_rsize = GN_USING_FILE_BUF << 3;
			if((ph_src || ph_dst) && max_rsize > GN_SIZE_1MB)max_rsize = GN_SIZE_1MB; //通过网络复制时用少点缓存以监视到文件读写进度
			char *buf = new char[fsize > max_rsize? max_rsize : fsize];
			float inv_porg = 1.0f / (fsize * 0.00001f), *_porg = &aCurProcFileProgress;
			*_porg = 0.0f;
			int size_ = 0, rsize1, rsize, wsize;
			while(size_ < fsize){
                rsize1 = fsize - size_;
                if(rsize1 > max_rsize)rsize1 = max_rsize;
				rsize = GNf::Read(buf, 1, rsize1, fp_src);
				if(rsize > 0){
				    wsize = GNf::Write(buf, 1, rsize, fp_dst);
					if(wsize > 0){
						size_ += wsize;
						*_porg = (size_ * 0.00001f) * inv_porg;
						if(wsize != rsize)GNf::Seek(fp_src, size_, GNf::eSEEK_START);
					}else{ ret = -4; break; }
				}else{	   ret = -3; break; }
			}
            delete [] buf;
		    GNf::Close(fp_dst);
			if(ret < -2)GNf::Del(dst, ph_dst); //文件复制中途失败就删除该破碎文件
			else if(aOnFileCopyEndCallBack){
				aOnFileCopyEndCallBack(dst, src, ph_dst, ph_src);
			}
		}else ret = -2;
		GNf::Close(fp_src);
	}else ret = -1;
    GNf_DirStatePop();
	return ret;
}
inline void GT_FileBrowserCopyDirAndFile(GNf::sFILE_INFO *file_info){
    GNu::cFILE_BROWSER *fb = GT_FileBrowserLast;
	strncpy((char*)(fb->zTkArgSMode) + fb->zTkArgPathLen, file_info->global_filename + fb->zTkArgPathLen2, GN_MAXFILEPATH - fb->zTkArgPathLen - 1);
	((char*)(fb->zTkArgSMode))[GN_MAXFILEPATH - 1] = 0; //续上目标目录或文件名
	if(file_info->attrib & _A_SUBDIR){
		GNf_DirStatePush(true);
		GNf::Mkdir(fb->zTkArgSMode, fb->zTkArgPH);
		GNf_DirStatePop();
	
	}else if(GNu::cFILE_BROWSER::FileCopy(fb->zTkArgSMode, file_info->global_filename, fb->zTkArgPH , fb->zTkArgPH2))fb->zTkArgSSubDir = 0; //标记有错
}
void GT_FILE_BROWSER_FindCopy(P<GN_sTHREAD> this_thread, void *arg, void *arg2){
    GNu::cFILE_BROWSER * fb = (GNu::cFILE_BROWSER *)arg;
    char *dir = (char*)fb->zTkArgSDir;
    fb->zTkArgSSubDir = 1; //临时用该值来标记是否无错
	GT_FileBrowserLast = fb;
	GNf_DirStatePush(true);
    GNf::Mkdir(fb->zTkArgSMode, fb->zTkArgPH);
    GNf_DirStatePop();
	fb->zTkArgSMode[fb->zTkArgPathLen-1] = '/';
	fb->zTkArgSMode[fb->zTkArgPathLen  ] = 0;
    fb->zIsDirDelOrCopy = true;
	fb->zSearch__(fb->zTkArgSDir, "*.*", true, GT_FileBrowserCopyDirAndFile, fb->zTkArgPH2);
	if(!fb->zTkArgSSubDir){
		GN_ErrorLast.New(GN_MAXFILEPATH);
		sprintf(GN_ErrorLast,"复制目录: %s 途中有错!", (char*)fb->zTkArgSDir);
	}
	if(this_thread)this_thread->us_state = 255;
}
void GNu::cFILE_BROWSER::CopyDir(bool wait, const char *dir_dst, const char *dir_src, void *ph_src){
	if((char*)ph_src == (char*)GNf::sFILE::DefaultH && GN_StrcmpPath(dir_dst, dir_src)){
		GN_ErrorLast("在原位置复制目录无效", 0); return;
	}
	zTkArgPH  = GNf::sFILE::DefaultH;
    zTkArgPH2 = ph_src;
	zTkArgSMode.New(GN_MAXFILEPATH);
	strncpy((char*)zTkArgSMode, dir_dst, GN_MAXFILEPATH - 1);
	((char*)zTkArgSMode)[GN_MAXFILEPATH - 1] = 0;
	zTkArgSDir(dir_src, 0);
    zTkArgPathLen  = strlen(dir_dst) + 1;
	zTkArgPathLen2 = strlen(dir_src) + 1;

    if(GN_ThreadSupport){
		if(zTkThread)return;
	    if(!wait){
			zTkThread = GN_ThreadStart(GT_FILE_BROWSER_FindCopy, this, 0);
            return;
	}	}
    GT_FILE_BROWSER_FindCopy(P_(), this, 0);
}
void GNu::cFILE_BROWSER::WaitProcFlushReturn(){
	if(zTkThread){
		bool sry = zShowReady; zShowReady = false;
		GN_ThreadEndWait(zTkThread);
		zShowReady = sry;
	}
}
void GNu::cFILE_BROWSER::zGenerateFileIcons(){
	#ifdef GN_USING_FILE_PREVIEW
	if(zRelate)aOnIconClearCallBack(zRelate);
   #endif
	zTextOffsettg = 0.0f;
	zTextOffset   = 0.0f;
	zCurSelectID  = 0;
	zSeekSelectID  = 0xffffffff; //该值表示失效
	zLastSelectFID = 0xffffffff;
	unsigned int file_stid = 0xffffffff, file_n = zFileList.size(), dir_n = zDirList.size();

	if(file_n || dir_n){
		char *ext_name;
		GN::sLIST< P<GNf::sFILE_INFO> >::iterator itf, etf;
        GNf::sFILE_INFO *_finfo;
	
		//按类型字符串来过滤文件
		if(file_n && aFileTypeFilterStr && aFileTypeFilterStr[0]){
			char tmp_str[256];
			strncpy(tmp_str, aFileTypeFilterStr, sizeof(tmp_str)-1); tmp_str[sizeof(tmp_str)-1] = 0;
            P<char*> type_strs = GN_StrSplit(tmp_str, ',');
			unsigned int si, sn = type_strs.Num();
			if(sn){
				char **_type_strs = type_strs;
				for(itf = zFileList.begin(), etf = zFileList.end(); itf != etf; ){
					_finfo = (*itf);
					ext_name = GN_StrGetExtName(_finfo->name);
					if(ext_name){
						for(si = 0; si < sn; ++si){ if(!strcmp(ext_name, _type_strs[si]))break; }
                        if(si < sn)++itf;
						else itf = zFileList.erase(itf);
					}else    itf = zFileList.erase(itf);
				}
		        file_n = (unsigned int)zFileList.size();
		}	}
	    switch(zSortType){   //按类型来排列文件
		case eFILE_SORT_NAME:
			if(file_n > 1)zFileList.sort(GT_FileSortFormName);
            if(dir_n  > 1)zDirList.sort (GT_FileSortFormName);
			break;
		case eFILE_SORT_SIZE:
			if(file_n > 1)zFileList.sort(GT_FileSortFormSize);
            if(dir_n  > 1)zDirList.sort (GT_FileSortFormName);
			break;
		case eFILE_SORT_CLASS:
			if(file_n > 1)zFileList.sort(GT_FileSortFormClass);
            if(dir_n  > 1)zDirList.sort (GT_FileSortFormName);
			break;
		case eFILE_SORT_DATE:
			if(file_n > 1)zFileList.sort(GT_FileSortFormData);
            if(dir_n  > 1)zDirList.sort (GT_FileSortFormData);
			break;
		}
		unsigned int name_num = file_n + dir_n;
		zInfoP.New(name_num);		   //分配足够量的数据空间
		zFileNames.New(name_num);
		zPicID.New(name_num);
		zFileNum = 0;
		P<GNf::sFILE_INFO> **_infops = zInfoP;
        P<char> *_filenames = zFileNames;
        char    *_pic_ids   = zPicID;
		if(dir_n){
			char *_fname;
			for(itf = zDirList.begin(), etf = zDirList.end(); itf != etf; ++itf){
				_finfo = (*itf);
				_fname = _finfo->name;
                _pic_ids[zFileNum] = 1;	 //文件属性ID为目录
	            if(*_fname == '.'){
					if(_fname[1] == '.'&& !_fname[2]){		 //处理特殊文件夹 ..
				        (_filenames[zFileNum])("<-LAST", 0); //这个是返回上一层特殊目录
                        _infops[zFileNum++] = &(*itf);
					}else if(_fname[1]){
						(_filenames[zFileNum])(_fname,0);	 //复制文件名以便用来显示
						_infops[zFileNum++] = &(*itf);
					}
				}else{
						(_filenames[zFileNum])(_fname,0);	//复制文件名以便用来显示
						_infops[zFileNum++] = &(*itf);
				}
			}
		}
		if(file_n){
			file_stid = zFileNum;
			P<char> *ext_names = GT_FB_ExtNameTable;
			unsigned int exi, ext_num = GT_FB_ExtNameTable.Num();
			for(itf = zFileList.begin(), etf = zFileList.end(); itf != etf; ++itf){
                _finfo = (*itf);
				ext_name = GN_StrGetExtName(_finfo->name);
				if(ext_name){
					for(exi = 0; exi < ext_num; ++exi){ 
						if(!strcmp(ext_name, ext_names[exi])){  //其它文件属性ID就按文件扩展名来查找引擎支持扩展名表来得到图标ID
							_pic_ids[zFileNum] = (char)(exi + 2);
							break;
					}	}
                    if(exi >= ext_num)_pic_ids[zFileNum] = 0;	//如果所有找过都没找到就标记ID为0 表示未知类型
				}else                 _pic_ids[zFileNum] = 0;	//无有扩展名也标记为未知类型
			    (_filenames[zFileNum])(_finfo->name, 0);		//复制文件名以便用来显示
				_infops[zFileNum++] = &(*itf);
			}
		}
		if(zFileNum){
			strncpy(zLastDir, aOpenDir, sizeof(zLastDir)-1); zLastDir[sizeof(zLastDir)-1] = 0;
			  zIsNoFileAndDir = 0;
		}else zIsNoFileAndDir = 1;
	}else	  zIsNoFileAndDir = 2;

	if(zIsNoFileAndDir){
		zInfoP.Delete();
	    zFileNum = 1;
		zFileNames.New(1);
	   (zFileNames[0])("<-RETURN", 0);
		zPicID.New(1);
        zPicID[0] = 1;
	}
   #ifdef GN_USING_FILE_PREVIEW
	if(zRelate){ // -- 复制备份当前要显示图标对应的全局路径 ------------------------------
	   GT_GOTO_FB_CopyGlobalPaths:
		zGlobalPaths.New(zFileNum);  zGlobalPaths_.New(zFileNum);
		if(zIsNoFileAndDir){
			unsigned int opendir_len = strlen(aOpenDir);
			zGlobalPaths[0].New(opendir_len + 4);
			char *gpath0 = zGlobalPaths[0];
			strncpy(gpath0, aOpenDir, opendir_len);  strcpy(gpath0 + opendir_len, "/..");
			zGlobalPaths_[0] = gpath0;
		}else{
			P<char> *gpaths  = zGlobalPaths;
			char   **gpaths_ = zGlobalPaths_;
			P<GNf::sFILE_INFO> **_infops = zInfoP;
			unsigned int pfi = 0;
			for(; pfi < zFileNum; ++pfi){
				(gpaths[pfi])((*(_infops[pfi]))->global_filename, 0);
                 gpaths_[pfi] = gpaths[pfi];
		}	}
		aOnIconGenCallBack(zRelate, (int)zFileNum, (const char**)((char**)zGlobalPaths_), zPicID, zReGenIcon == 2, GT_FB_DefaultIconImg->width, GT_FB_DefaultIconImg->height);
	}else if(GT_FbShowPreview){
		zRelate = aOnCreateCallBack(this);
		goto GT_GOTO_FB_CopyGlobalPaths;
	}
   #endif
	//------------------------------------------------------------------------------------------------------------------------------------------------
	zUsedFileNameList.clear();
	zSelected.New(zFileNum);
    memset((char*)zSelected, 0, zSelected.Size()); //先设置所有文件为未选状态

	zLastStartPos = 0;
	GNg::cFRAME_LAUNCHER_FONT * _fframes = zFFrames;
	unsigned int set_count = zFileNum, arng_num = (unsigned int)GetParalleShowCount();
	if(set_count / arng_num > 253)set_count = arng_num * 254; //检测并裁短一下符合SetText要 的范围 以防超出最大256行限制
	_fframes->SetText(zPicID, set_count, aShowOffsetX, aShowOffsetY);
	zCurHasReArrange = 1;
   #ifdef GN_USING_FILE_PREVIEW
	if(zRelate)aOnIconsReArrangeCallBack(zRelate, zFileNum, _fframes->zDrawRngData->strid, _fframes->zDrawRngData[_fframes->zMax_num-1].strid);
	if(zActIconTmpData)((GT_sFB_ACT_ICON_DATA*)zActIconTmpData)->ReSet(zFileNum);
   #endif
	OnReArrangeIcon(zReGenIcon == 2, zFileNames, file_stid);
}
void GT_FILE_BROWSER_FindDir(P<GN_sTHREAD> this_thread, void *arg, void *arg2){
    GNu::cFILE_BROWSER *fb = (GNu::cFILE_BROWSER*)arg;
    char open_dir[GN_MAXFILEPATH];
	snprintf(open_dir, GN_MAXFILEPATH, "%s/*.*", fb->aOpenDir);
	fb->zInfoP.Delete();
	fb->zFileList.clear(); fb->zDirList.clear();
	int err = GNf::FindFileDirInfoData(open_dir, true, &(fb->zFileList), &(fb->zDirList));
   #ifdef GN_USING_OPENGLes_ANDROID_JAVA
    if(!err){
		bool no_retdir = true; //检测是否没有返回目录 ".."
		GN::sLIST< P<GNf::sFILE_INFO> > &dir_list = fb->zDirList;
		if(dir_list.size()){ 
			GN::sLIST< P<GNf::sFILE_INFO> >::iterator it_f = dir_list.begin(), et_f = dir_list.end();
            GNf::sFILE_INFO *p_file;
			for( ; it_f != et_f; ++it_f){
			    p_file = (*it_f);
                if(p_file->name[0] == '.'){
                    if(p_file->name[1]=='.' && p_file->name[2]==0){
                        no_retdir = false;
                        break;
                    }
				}else break;
			}
		}
		if(no_retdir){ //如没就增加一个返回目录
		    P<GNf::sFILE_INFO> rer_dir_info(1);
		    strncpy(rer_dir_info->name, "..", sizeof(rer_dir_info->name)-1); rer_dir_info->name[sizeof(rer_dir_info->name)-1] = 0;
		    rer_dir_info->attrib = _A_SUBDIR;
		    dir_list.push_front(rer_dir_info);
		}
    }
   #endif
	fb->zReGenIcon = 1;
	if(this_thread)this_thread->us_state = 255;
}
void GNu::cFILE_BROWSER::zArrangeFileIcons(){
    if(zReLoadIcon){	 //打开新的目录
        zReLoadIcon = false;
		if(GN_ThreadSupport){
            zTkThread = GN_ThreadStart(GT_FILE_BROWSER_FindDir, this, 0);
			OkHide(); zIsFindIng = true;
		}else GT_FILE_BROWSER_FindDir(P_(), this, 0);

	}else if(zSelected){ //重排当前的目录内容 ----------------------------------------------------------------------------------------------
		GNg::cFRAME_LAUNCHER_FONT *_fframe = zFFrames;
		bool ret_am = GN_ValueCompareFloat(zTextOffset, zTextOffsettg, 0.5f);
        GT_TMP_ScrollSetTextRangeBegin( zFileNum )
		if(parame->erect_print)_fframe->SetText(zPicID + pid_start, set_count, _icon_offset+aShowOffsetX, aShowOffsetY, ret_am);
		else				   _fframe->SetText(zPicID + pid_start, set_count, aShowOffsetX, _icon_offset+aShowOffsetY, ret_am);
		zCurHasReArrange = 1;

		unsigned int fni, fnn = _fframe->zMax_num;
		GNg::sDRAW_RANGE *rng_data = _fframe->zDrawRngData;
		unsigned short pid_start1 = (unsigned short)pid_start;
		if(pid_start1){
			for(fni = 0; fni < fnn; ++fni)rng_data[fni].strid += pid_start1;
		}
		if(_fframe->zActFrame){
			char *_selected = zSelected;
			for(fni = 0; fni < fnn; ++fni){		//需要重设被选中的图标为激活状态
				if(_selected[rng_data[fni].strid])_fframe->SelectActChange(fni);
			}
			if(!ret_am && zLastStartPos != pid_start1){   //该情况也可能要清一清之前的选定动态筐时间
                unsigned int  act_end_time = _fframe->zActFrame->zEndtime;
				unsigned int *act_times    = _fframe->zActTime_s;
				uint8 *act_states		   = _fframe->zActState_s;
			    for(fni = 0; fni < fnn; ++fni){
					if(_selected[rng_data[fni].strid])
						act_times[fni] = act_end_time;
					else{
						act_times[fni] = 0;
					    act_states[fni] = 0;
				}	}
			}
		}
		zLastStartPos = pid_start1;
	   #ifdef GN_USING_FILE_PREVIEW
		if(zRelate)aOnIconsReArrangeCallBack(zRelate, zFileNum, rng_data->strid, rng_data[fnn-1].strid);
	   #endif
	}
}
int8 GNu::cFILE_BROWSER::UpdateRun(unsigned int updatatime, GNg::sIMAGE **img_out, GNg::sDRAW_RANGE *click_icon_info, unsigned int dc){
	if(!zShowReady){
	   #ifdef GN_DEBUG
		GN_Log("调用cFILE_BROWSER::UpdateRun(...) 被终止! 请先调用cFILE_BROWSER::InitStyleToData 进行初始化\n");
       #endif
		return 0;
	}
	if(zTkThread && zTkThread->us_state == 255)zTkThread.Delete();
	GNg::cFRAME_LAUNCHER_FONT *_fframe = zFFrames;
    zCurHasReArrange = 0;

	if(zReArrangeParame){ //强行修改一下排版参数以适配显示文件图标用
		GNg::sIMAGE *def_fontmap = GN::RunRec.aFont.GetFontMap();
		if(zTextOut.GetFontMap() == def_fontmap){ //若使用了引擎默认字体侧需设置一下它 使得看起来更自然
			if(zNearPos == eNEAR_TOP || zNearPos == eNEAR_BOTTOM){
			    GNg::sARRANGE_FMT ffmt1(false, 0.5f,0.0f, 0,1,0,0, GNg::eENTER_LEFT_RIGHT);
				zTextOut.Init(def_fontmap, &ffmt1);
			}else{
			    GNg::sARRANGE_FMT ffmt2(false, 0.0f,0.5f, 0,1,0,0, GNg::eENTER_NULL);
			    zTextOut.Init(def_fontmap, &ffmt2);
		}	}
		_fframe->zParame.auto_enter = _fframe->zParame.erect_print ? GNg::eENTER_TOP_BOTTOM : GNg::eENTER_LEFT_RIGHT; //必需使用自动换行
        zReArrangeParame = 0;
	}
	if(zReArrange && !zIsFindIng){
        zArrangeFileIcons();
	    zReArrange = 0;
	}
	if(zReGenIcon){
		zGenerateFileIcons(); //重新生成一下图标
	    zReGenIcon = 0;
		zIsFindIng = 0;
	}
   #ifdef GN_USING_FILE_PREVIEW
	if(zRelate)aOnUpdateCallBack(zRelate, updatatime);
   #endif
	if(!zFileNames)return 0;

	GNg::cVIEW2D *view = GNg::ViewCur;
	if(!_fframe->zMax_num){
	    zIsMouseFocus = GNg::TestPointIntRect(view->mouseMX, view->mouseMY, (int)_fframe->aGenRngX, (int)_fframe->aGenRngY, (int)_fframe->aGenRngW, (int)_fframe->aGenRngH);
		zScrollProc(updatatime);
        return 0;
	}
   #ifdef GN_USING_FILE_PREVIEW
	unsigned short empty_tid = (unsigned short)GT_FB_ExtNameTable.Num() + 2;
	char **_global_paths = zGlobalPaths_;

	if(zRelate){ //-索取预览图标处理-------------------------------------------------------------------------------------------
		char * p_pic_id = zPicID;
		GNg::sDRAW_RANGE *_p_arrd = _fframe->zDrawRngData;
		unsigned short _first = _p_arrd[0].strid, _last = _p_arrd[_fframe->zMax_num - 1].strid;
		unsigned short _i = _first, _n = _last + 1;
		
        GT_sFB_ACT_ICON_DATA *ad;
		if(!zActIconTmpData){
			ad = new GT_sFB_ACT_ICON_DATA;
            zActIconTmpData = ad;
            ad->InitToData();
			ad->ReSet(zFileNum);
		}else ad = (GT_sFB_ACT_ICON_DATA *)zActIconTmpData;
		uint8          *_states       = ad->states;
		unsigned short *_icon_ids     = ad->icon_ids;

		if(ad->full){
			if(_first > ad->ir_max || _last < ad->ir_min)ad->ReSet(zFileNum);
		}else{
			unsigned int  *_ntime_data    = _fframe->zNowtime_s;
			char *icon_data, *_p_selected = zSelected;
			int icon_check_state;

			unsigned short _r_ = _n - _i, i_start = _i, i_end = _n;
			if(_r_ < ad->icon.texnum){ //扩充检索范围
			    unsigned short expand = (ad->icon.texnum - _r_) >> 1;
				if(i_start > expand)i_start -= (expand - 1);
				else i_start = 0;
                i_end = i_start + ad->icon.texnum - 2;
                if(i_end > (unsigned short)zFileNum)i_end = (unsigned short)zFileNum;
			}
			for(_r_ = 0; _r_ < 3; ++_r_){ //分3段来检索图标数据, 第一段为视图看到的, 第二,三段分别为看不到的两边段
				for(; _i < _n; ++_i){
					if(_states[_i] == 1 || _states[_i] == 2){ //对应图标需要数据 或 被选中时可能要更新数据时
						icon_check_state = aOnIconDataOkCheckCallBack(zRelate,(int)_i, _p_selected[_i] ? 1 : 0, ad->icon.width, ad->icon.height, _global_paths[_i]);
						if(icon_check_state){
							if(icon_check_state != -1){
								icon_data = aOnIconDataGetCallBack(zRelate,(int)_i, _p_selected[_i] ? 1 : 0, _global_paths[_i]);
								if(icon_data){ //填充数据
									if(_states[_i] == 1 && !_icon_ids[_i]){
										if( ad->can_us_id < ad->icon.texnum){ //还有新的图标号分配
									        _icon_ids[_i] = ad->can_us_id;
											++(ad->can_us_id);
                                            if(_i < ad->ir_min)ad->ir_min = _i;
											if(_i > ad->ir_max)ad->ir_max = _i;
									    }else{ //占用比较偏的图标号来作分配
											if(_fframe->zMax_num + 2 >= (unsigned int )ad->icon.texnum){
											    ad->full = true;
                                                goto GT_GOTO_End_ActIconDataFill;
											}
											if((int)_first - (int)ad->ir_min > (int)ad->ir_max - (int)_last){
											    _icon_ids[_i]  = _icon_ids[ad->ir_min];
												_icon_ids[ad->ir_min] = 0;
                                                _states[ad->ir_min]   = 1;
												if(_i > ad->ir_max)ad->ir_max = _i;
                                                ++(ad->ir_min);
                                                while(!_icon_ids[ad->ir_min])++(ad->ir_min);
											}else{
											    _icon_ids[_i]  = _icon_ids[ad->ir_max];
												_icon_ids[ad->ir_max] = 0;
                                                _states[ad->ir_max]   = 1;
												if(_i < ad->ir_min)ad->ir_min = _i;
                                                --(ad->ir_max);
                                                while(!_icon_ids[ad->ir_max])--(ad->ir_max);
										}	}
									}
									if(icon_check_state&8){ //直接使用图标图
										GN_Swap(unsigned int, ad->icon.texture[_icon_ids[_i]], ((GNg::sIMAGE*)icon_data)->texture[0]);
										icon_check_state -= 8;
									}else{				    //把数据填充到图标里
										ad->icon.DataFill((uint8*)icon_data, GNg::CR_RGBA, 0,0, 0,0, _icon_ids[_i], true);
									}
									_states[_i] = (icon_check_state > 2) ? 3 : 2;
									if((icon_check_state == 2 || icon_check_state == 4) && _i >= _first && _i <= _last)_ntime_data[_i - _first] = 0;
                                   GT_GOTO_End_ActIconDataFill:
									aOnIconDataChangedCallBack(zRelate,(int)_i, _p_selected[_i] ? 1 : 0, _global_paths[_i]);
								}else _states[_i] = 0;
							}
						}else _states[_i] = 0;
					}
				}
				if(ad->gen_swap){ //看看用户当前操作是反向查看
					if(!_r_){ _i = i_start;   _n = _first; }
					else    { _i = _last + 1; _n = i_end;  }
				}else{
					if( _r_){ _i = i_start;   _n = _first; }
					else    { _i = _last + 1; _n = i_end;  }
				}
			}
		}
        if     (_first > ad->sr_last)ad->gen_swap = false;
		else if(_first < ad->sr_last)ad->gen_swap = true;
        ad->sr_last = _first;

		if(!zTkThread || !zIsFindIng || _fframe->zFontFrame->zEventcount){
			for(_n = 0, _i = _first; _i <= _last; _i++, _p_arrd++){
				if(_states[_i] > 1){
                    _p_arrd->tid = _icon_ids[_i];
					_n++;
				}else _p_arrd->tid = 0;
			}
			if(_n){ //有活动图标才显示它
				GNg::sIMAGE *play_obj_src = _fframe->zFontFrame->zPlay_obj;
				_fframe->zFontFrame->zPlay_obj = &(ad->icon);
				_fframe->PlayFrame(updatatime, dc);
                _fframe->zFontFrame->zPlay_obj = play_obj_src;
                updatatime = 0;
			}
			for(_p_arrd = _fframe->zDrawRngData, _i = _first; _i <= _last; _i++, _p_arrd++)_p_arrd->tid = _states[_i] > 1 ? empty_tid : (unsigned short)p_pic_id[_i];
			  _n = 0; //标记已经恢复原来的图标引用ID了
		}else _n = _last + 1;

		if(!zCurHasReArrange && _n){ //可能要复原一下原来的图标引用ID
		    for(_p_arrd = _fframe->zDrawRngData, _i = _first; _i <= _last; _i++, _p_arrd++)_p_arrd->tid = (unsigned short)p_pic_id[_i];
		}
	}
   #endif //-----------------------------------------------------------------------------------------------------------------------------------------------------------------
	if(zTkThread){
		if(zIsFindIng && !(_fframe->zFontFrame->zEventcount))return 0;
		else{
            if(_fframe->PlayFrame(updatatime, dc))return 0;
			else return 2;
	}	}
	GNg::sDRAW_RANGE *rng_data = _fframe->zDrawRngData;
	if(oW >= GN_INT_MAX){
	    oW = (int)(_fframe->zTu_w / (rng_data->u2 - rng_data->u1) * (float)rng_data->w * _fframe->ztCU);
	    oH = (int)((float)rng_data->h * _fframe->ztCV);
	}
	bool ctrl_on = (GN::Key[GK_LCONTROL] || GN::Key[GK_RCONTROL]), shift_on = (GN::Key[GK_LSHIFT] || GN::Key[GK_RSHIFT]), alt_on = (GN::Key[GK_LMENU] || GN::Key[GK_RMENU]);
	zUsMultiSelect = aUsMultiSelect || ctrl_on;
   #if 1 //适合鼠标和触屏的操作
    bool mouse_keyi = GN::MouseI[GM_LEFT], mouse_key2 = false;
    if(!zUsMultiSelect)mouse_key2 = GN::MouseI[GM_LEFT];
   #else //上个版本操作适合鼠标
    bool mouse_keyi = GN::MouseI[GM_LEFT], mouse_key2 = GN::Mouse2[GM_LEFT];
   #endif
	int mouse_x = view->mouseMX, mouse_y = view->mouseMY;
	int frame_x  = (int) _fframe->aGenRngX,
		frame_y  = (int) _fframe->aGenRngY;
	int frame_x2 = frame_x + (int)(_fframe->aGenRngW),
		frame_y2 = frame_y + (int)(_fframe->aGenRngH);
	
    zIsMouseFocus = mouse_x > frame_x && mouse_x < frame_x2 && mouse_y > frame_y  && mouse_y < frame_y2;
	if(aDisableControl || !zIsMouseFocus){
		mouse_keyi = mouse_key2 = false;
		ctrl_on = false;
	}
	GNg::sDRAW_RANGE *_icon_rng;
	GNg::sIMAGE *_focus_img = zFocusImg;
	P<char> *_filenames = zFileNames;
    char    *_selected  = zSelected;
	const unsigned int show_text_num = _fframe->zMax_num;
	unsigned int pid, fni, sel_fid = 0xffffffff;

	if(!zUsMultiSelect){	//单项选择时
		if(mouse_keyi && zIsMouseFocus){	//有鼠标单击 就取消所有选中项
		    for(fni = 0; fni < show_text_num; ++fni)_fframe->CancelActChange(fni);
			memset(_selected, 0, zSelected.Size()); //设置所有为未选状态
	}	}
	//------------------------------------------------------------- 文件名显示 ---------------------------------------------------------------------------------------------------------------------------------
	bool has_am = _fframe->zFontFrame->zFEvent ? true: false;
	int tx,ty,tw,th, t_ofsx,t_ofsy;
	const int tw_8 = zTextW - 8, th_8 = zTextH - 8, tfw2 = zTextW>>1, tfh2 = zTextH>>1, fw2 = zTextOut.zParame.width>>1;
	unsigned int text_color, focus_color = zArrowAm.aLevel_color, focus_color2, st_color = 0xffffffff, ed_color = 0xffffffff, end_time = _fframe->zEnd_time;
	unsigned int *color_data = _fframe->zColor_s;
	unsigned int *ntime_data = _fframe->zNowtime_s;
	unsigned int *act_times  = _fframe->zActTime_s;
	if(_fframe->zSrcAmMode)ed_color = 0x00ffffff;
	else st_color = 0x00ffffff;

	for(fni = 0; fni < show_text_num; ++fni){
		_icon_rng = rng_data + fni;
		switch(zNearPos){
		case eNEAR_BOTTOM:
			tx = oW + _icon_rng->x + (_icon_rng->w>>1) - (int)tfw2;
			ty = oH + _icon_rng->y + _icon_rng->h + fw2; //距图标半个字高度
			break;
		case eNEAR_RIGHT:
            tx = oW + _icon_rng->x + _icon_rng->w + fw2; //距图标半个字宽度
            ty = oH + _icon_rng->y + (_icon_rng->h>>1) - (int)tfh2;
			break;
		case eNEAR_LEFT:
            tx = oW + _icon_rng->x - (int)zTextW - fw2;  //距图标半个字宽度
            ty = oH + _icon_rng->y + (_icon_rng->h>>1) - (int)tfh2;
			break;
		case eNEAR_TOP:
            tx = oW + _icon_rng->x + (_icon_rng->w>>1) - (int)tfw2;
			ty = oH + _icon_rng->y - (int)zTextH - fw2;  //距图标半个字高度
			break;
		}
		tx += 4,   ty += 4;	//文本筐与选中焦点边距4象素
		tw = tw_8; th = th_8;
        t_ofsx = 0;
		t_ofsy = 0;

		if(tx < frame_x){   /*模版代码:裁剪掉水平方向凸出筐的部分*/
			if(tx + tw <= frame_x)continue;
            t_ofsx = -(frame_x - tx);
			tw += t_ofsx;
			tx = frame_x;
		}else if(tx >= frame_x2)continue;
		 else if(tx + tw > frame_x2)tw = frame_x2 - tx;
		
		if(ty < frame_y){   /*模版代码:裁剪掉垂直方向凸出筐的部分*/
			if(ty + th <= frame_y)continue;
			t_ofsy = -(frame_y - ty);
			th += t_ofsy;
			ty = frame_y;
		}else if(ty >= frame_y2)continue;
		 else if(ty + th > frame_y2)th = frame_y2 - ty;

        pid = _icon_rng->strid;
        if(_fframe->zActFrame){
			if(act_times[fni] > 0){
			    focus_color2 = ((unsigned int)((float)act_times[fni] / (float)_fframe->zActFrame->zEndtime * (float)(focus_color>>24)) <<24) | (focus_color&0x00ffffff);
		        if(has_am)focus_color2 = GNg::LinearColorMultiplyRatio(st_color, ed_color, ntime_data[fni], end_time, focus_color2);
				GNg::ImageDraw(_focus_img, tx-4, ty-4, tw+8, th+8, focus_color2, 0, aFocusBlend); //有选中的就绘制一个底图
			}
		}else if(_selected[pid]){
				focus_color2 = (has_am? GNg::LinearColorMultiplyRatio(st_color, ed_color, (ntime_data[fni]==0xffffffff?0:ntime_data[fni]), end_time, focus_color) : focus_color);
				GNg::ImageDraw(_focus_img, tx-4, ty-4, tw+8, th+8, focus_color2, 0, aFocusBlend); //有选中的就绘制一个底图
		}
		if(has_am)text_color = (ntime_data[fni] <= end_time? GNg::LinearColorMultiplyRatio(st_color, ed_color, ntime_data[fni], end_time, GNg_MultiplyColorRatio(aTextColor, color_data[fni])) : 0x00ffffff);
		else	  text_color = GNg_MultiplyColorRatio(aTextColor,color_data[fni]);
		zTextOut.SetText(_filenames[pid], 0, tx,ty,tw,th, t_ofsx,t_ofsy); zTextOut.Show(text_color, aTextBlend); //显示文件名
		
	   #ifdef GN_USING_FILE_PREVIEW
		if(zRelate){
			if(aOnIconDrawCallBack(zRelate,(int)pid, _selected[pid]?1:0, _icon_rng->x+oW, _icon_rng->y+oH, _icon_rng->w, _icon_rng->h, text_color, _icon_rng->tid, _global_paths[pid]))_icon_rng->tid = empty_tid;
		}
	   #endif
		if(mouse_keyi && GNg::TestPointIntRect(mouse_x,mouse_y, tx,ty,tw,th)){ //使点到文件名都能选中图标
			if(shift_on && zCurSelectID!=pid)zSelectSegment(fni, pid);
			else zSetIconChange(fni, pid);
		}
        if(mouse_key2 && GNg::TestPointIntRect(mouse_x,mouse_y, tx,ty,tw,th))sel_fid = fni; //使双击点到文件名都能进入对应的目录
	}
	if(zIsNoFileAndDir){ //如打开目录没有东西就显示提示
		GNg::sARRANGE_FMT &tfmt = zTextOut.zParame;
		tx = (int) _fframe->aGenRngX + (int)tfmt.width;
		tw = (int)(_fframe->aGenRngX + _fframe->aGenRngW) - (int)tfmt.width - tx;
		ty = (int)(_fframe->aGenRngY + _fframe->aGenRngH) - (int)(tfmt.height<<1);
		th = (int)(tfmt.height<<1);
		text_color = (_fframe->zDisplay_num? GNg_MultiplyColorRatio(aTextColor,color_data[0]) : aTextColor);
		
		char err_info[GN_MAXFILEPATH];
		sprintf(err_info, zIsNoFileAndDir == 2?"打开目录: %s 失败 该目录不能访问!" : "%s 里找不到文件和目录!", aOpenDir);
		zTextOut.SetText(err_info,0, tx,ty,tw,th);
		zTextOut.Show(text_color, aTextBlend); //显示提示
	}
	_fframe->SetSelectPoint(mouse_x, mouse_y);
	if(!_fframe->PlayFrame(updatatime, dc))return 2; //显示图标

	if(mouse_keyi || mouse_key2){ // 鼠标有单击或双击 ------------------------
		unsigned int sel_id;
		if(_fframe->GetSelectFontFrame(&sel_id)){
			pid = rng_data[sel_id].strid;
			if(shift_on && zCurSelectID != pid)zSelectSegment(sel_id, pid);
			else zSetIconChange(sel_id, pid);
			if(mouse_key2)sel_fid = sel_id; //有鼠标双击
	}	}
    if(ctrl_on && GN::Key1[GK_A]){ // 全选 -----------------------------------
		for(fni = 0; fni < show_text_num; ++fni){
			if(!_selected[rng_data[fni].strid])_fframe->SelectActChange(fni);
		}
        memset(_selected, 1, zSelected.Size());
	}
	if(zClickLastFile){
	   zClickLastFile = 0;
        if(zLastSelectFID != 0xffffffff)sel_fid = zLastSelectFID;
	}
	if(sel_fid == 0xffffffff){
		if(!aDisableControl){
			if(!ctrl_on  && !alt_on){
				if(zIsMouseFocus){
					if(GN::Key1[GK_BACK])ReturnToLastDir(); //按下退格键就返回上一层
                    GT_ImeOnRecv = 1;
					if(GT_ImeCount){ //键盘有字符输入
                        unsigned int len, fni1 = zCurSelectID + 1;
						for(fni = 0; fni < zFileNum; ++fni, ++fni1){ //检索当前打开目录下文件名的头字符和键盘输入字符匹配的文件
							if(fni1 >= zFileNum)fni1 = 0;
							
							len = _filenames[fni1].Num() - 1;
							if(len > (unsigned int)GT_ImeCount)len = (unsigned int)GT_ImeCount;
							if(!strncmp(_filenames[fni1], GT_ImeChar , len)){ //找到匹配的文件名
								zScrollToSelectIcon(fni1);
								break;
						}	}
				}	}
			}
			if(zSeekSelectID == 0xffffffff)zOffsetAdd = 0;
			else{
				for(fni = 0; fni < show_text_num; ++fni){   //使滚屏找到要找的文件
					if(zSeekSelectID == (unsigned int)rng_data[fni].strid){ 
						zSetIconChange(fni, zSeekSelectID); //把找到的文件设为选中
                        zTextOffsettg = zTextOffset; zOffsetAdd = 0;  //停止滚屏
						
						if(_fframe->zParame.erect_print){   //如找到的图标有些在边角上被遮挡了就增加一点滚屏偏移来使图标可见
						    if(rng_data[fni].x < frame_x)
								zOffsetAdd = frame_x  - rng_data[fni].x;
							else if(rng_data[fni].x > frame_x2 - rng_data[fni].w)
								zOffsetAdd = frame_x2 - rng_data[fni].x - rng_data[fni].w;
						}else{
						    if(rng_data[fni].y < frame_y)
								zOffsetAdd = frame_y  - rng_data[fni].y;
							else if(rng_data[fni].y > frame_y2 - rng_data[fni].h)
								zOffsetAdd = frame_y2 - rng_data[fni].y - rng_data[fni].h;
						}
                        zSeekSelectID = 0xffffffff;
				}	}
			}
			zScrollProc(updatatime);  //处理滚屏
		}
	}else{
		if(img_out){
			*img_out = _fframe->zFontFrame->zPlay_obj;
		   #ifdef GN_USING_FILE_PREVIEW
            if(zActIconTmpData){
                GT_sFB_ACT_ICON_DATA *_ad = (GT_sFB_ACT_ICON_DATA *)zActIconTmpData;
				if(_ad->icon_ids[ rng_data[sel_fid].strid ])*img_out = &_ad->icon;
			}
		   #endif
		}
		if(click_icon_info)GetShowIconPosInfo(sel_fid, click_icon_info);
		return zDoubleClickOpen(sel_fid);
	}
	return 0;
}
void GNu::cFILE_BROWSER::UpdateRun(unsigned int updatetime, sUI_LEVEL *parent){
	if(RecvInputUiObject != this)GNg::ViewCur->mouseMX += 10000;
	zLastClickType = UpdateRun(updatetime, 0, 0, parent->c);
	if(RecvInputUiObject != this)GNg::ViewCur->mouseMX -= 10000;
	if(zLastClickType)UiActionCall(this, zLastClickType==2 ? eUI_ACT_VANISH : zLastClickType);
}
void GNu::cFILE_BROWSER::zScrollProc(unsigned int updatatime){
	GNg::cFRAME_LAUNCHER_FONT *_fframe = zFFrames;
	GNg::sARRANGE_FMT *pfmt = &(_fframe->zParame);
	GT_TMP_FFRAME_RANGE_GET;
	unsigned int focus_color = zArrowAm.aLevel_color;
    GT_TMP_UI_SCROLL_VIEW( zOffsetAdd, 1, zFileNum ,  );
}
void GNu::cFILE_BROWSER::GetShowIconPosInfo(unsigned int icon_sid, GNg::sDRAW_RANGE *info_out){
   #ifdef GN_DEBUG
	if(!info_out)GN_Log("调用cFILE_BROWSER::GetShowIconPosInfo 参数 info_out 不能为空的!\n");
    if (icon_sid >= zFFrames->zMax_num)GN_Log("调用cFILE_BROWSER::GetShowIconPosInfo 参数 icon_sid 值超出范围!\n");
   #endif
    GNg::sDRAW_RANGE *_dw_rng = zFFrames->zDrawRngData + icon_sid;
	*info_out = *_dw_rng;
	info_out->x += oW; info_out->y += oH;

   #ifdef GN_USING_FILE_PREVIEW
	if(zActIconTmpData){
	    unsigned short tid = ((GT_sFB_ACT_ICON_DATA *)zActIconTmpData)->icon_ids[_dw_rng->strid];
        if(tid)info_out->tid = tid;
	}
   #endif
}
void GNu::cFILE_BROWSER::zSelectSegment(unsigned int fframe_id, unsigned int pid){
	GNg::sDRAW_RANGE *rng_data = zFFrames->zDrawRngData;
	char *_pic_ids = zPicID;
	unsigned int id;
	if(zCurSelectID < pid){
		id = zCurSelectID;
		while(fframe_id && rng_data[fframe_id].strid >= id){	    //找到该段里最前一个文件
			if(_pic_ids[pid] != 1)zLastSelectFID = fframe_id;
            --fframe_id;
		}
	}else{
		id = pid; pid = zCurSelectID;
		unsigned int dn = zFFrames->GetDisplayCount();
        while(fframe_id < dn && rng_data[fframe_id].strid <= pid){  //找到该段里最末一个文件
			if(_pic_ids[pid] != 1)zLastSelectFID = fframe_id;
            ++fframe_id;
	}	}
    if(id)memset((char*)zSelected, 0, id - 1);
    memset((char*)zSelected + id,  1, pid - id + 1); //标记对应段落状态为选中
    ++pid;
	if(zFileNum > pid)memset((char*)zSelected + pid, 0, zFileNum - pid);
	zReArrange = true;
}
void GNu::cFILE_BROWSER::zScrollToSelectIcon(unsigned int pid){
	GNg::cFRAME_LAUNCHER_FONT *_fframe = zFFrames;
	GNg::sDRAW_RANGE *rng_data = _fframe->zDrawRngData;
	GNg::sARRANGE_FMT &ffmt = _fframe->zParame;
	unsigned int fni = 0, show_text_num = _fframe->zMax_num;
	for(; fni < show_text_num; ++fni)_fframe->CancelActChange(fni);
	
	memset((char*)zSelected, 0, zSelected.Size()); //清空所有选定状态
	zSeekSelectID = pid;
	if(pid < (unsigned int)rng_data->strid){					    //要找的东西在视图前
		if(ffmt.erect_print){
			if(ffmt.overturn_print)zOffsetAdd = -ffmt.i_compartment;
			else                   zOffsetAdd =  ffmt.i_compartment;
		}else                      zOffsetAdd =  ffmt.j_compartment;
	}else if(pid > (unsigned int)rng_data[show_text_num-1].strid){	//要找的东西在视图后
		if(ffmt.erect_print){
			if(ffmt.overturn_print)zOffsetAdd =  ffmt.i_compartment;
			else                   zOffsetAdd = -ffmt.i_compartment;
		}else                      zOffsetAdd = -ffmt.j_compartment;
	}
}
void GNu::cFILE_BROWSER::ReturnToLastDir(){
    if(zIsNoFileAndDir){
	    strncpy(aOpenDir, zLastDir, sizeof(aOpenDir)-1); aOpenDir[sizeof(aOpenDir)-1] = 0;
        zReArrange = 1; zReLoadIcon = 1;  //标记需重新排列图标
	}else{
	  if(GN_StrRemoveFileName(aOpenDir) != (char*)aOpenDir){
		zReArrange = 1; zReLoadIcon = 1;  //标记需重新排列图标
	  }
	}
}
bool GNu::cFILE_BROWSER::SelectIconFromName(const char *name){
	bool ret = false;
    P<char> *_filenames = zFileNames;
	unsigned int fi = 0;
	for( ; fi < zFileNum; ++fi){
		if(!strcmp(name, _filenames[fi])){
            zScrollToSelectIcon(fi);
		    ret = true; break;
	}	}
	return ret;
}
void GNu::cFILE_BROWSER::SelectIconFromRect(const GNg::sRECT *rect){
	if(!rect)return;
	GNg::cFRAME_LAUNCHER_FONT *_fframe = zFFrames;
	GNg::sDRAW_RANGE *rng_data = _fframe->zDrawRngData;
	char *_selected = zSelected;
	bool shift_off = !GN::Key[GK_LSHIFT] && !GN::Key[GK_RSHIFT];
	unsigned int  idn, i = 0, n = 0, pid, fdn = _fframe->zDisplay_num;
	unsigned int *ids = _fframe->GetFontFrameID_Data(rect, &idn);
	for(; i < fdn; ++i){
		pid = (unsigned int)rng_data[i].strid;
		if(n < idn && i == ids[n]){
			if(zUsMultiSelect){  //多选
			    if( _selected[pid]){ _fframe->CancelActChange(i); _selected[pid] = 0; }
			    else               { _fframe->SelectActChange(i); _selected[pid] = 1; }
			}else{				 //单选
				if(!_selected[pid]){ _fframe->SelectActChange(i); _selected[pid] = 1; }
			}
		    ++n;
		}else if(shift_off){
			if(!zUsMultiSelect){ //单选
			    if( _selected[pid]){ _fframe->CancelActChange(i); _selected[pid] = 0; }
		}	}
	}
	GN_DeleteArray(ids);
}
void GNu::cFILE_BROWSER::SelectClear(){
	if(zSelected){
		GNg::cFRAME_LAUNCHER_FONT *_fframe = zFFrames;
		unsigned int fni = 0, show_text_num = _fframe->zMax_num;
        for(; fni < show_text_num; ++fni)_fframe->CancelActChange(fni);
		memset((char*)zSelected, 0, zSelected.Size()); //清空所有选定状态
	}
}
void GNu::cFILE_BROWSER::OkHide(){
	if(zRelate)aOnIconClearCallBack(zRelate);
    zFFrames->ToHide();
}
GT_TMP_UI_GET_SET_POS_AND_OTHER_FUNC_DEF( cFILE_BROWSER )

P< P<char> > GNu::cFILE_BROWSER::GetSelectFileNameData(){
    #define GT_TMP_FILE_BROWSER_GetName( CMP ) \
	 P< P<char> > names;\
	 if(!zInfoP)return names;\
	 char *_selected = zSelected;\
	 char *_pic_ids  = zPicID;\
     unsigned int fi = 0, num = 0;\
	 for(; fi < zFileNum; ++fi){\
		 if(_selected[fi] && (_pic_ids[fi] CMP ))\
			 ++num;\
	 }\
	 if(!num)return names;\
     names.New(num);\
	 fi = 0, num = 0;\
	 for(; fi < zFileNum; ++fi){\
		 if(_selected[fi] && (_pic_ids[fi] CMP )){\
             (names[num])((*(zInfoP[fi]))->global_filename, 0);\
			 ++num;\
	 }	 }\
	 return names

	 GT_TMP_FILE_BROWSER_GetName( !=1 );
}
P< P<char> > GNu::cFILE_BROWSER::GetSelectDirNameData(){
	P<char> *_filenames = zFileNames;
    GT_TMP_FILE_BROWSER_GetName( ==1&&(strncmp(_filenames[fi],"<-LAST",6)&&strncmp(_filenames[fi],"<-RETURN",8)) );
}
P< P<GNf::sFILE_INFO> > GNu::cFILE_BROWSER::GetSelectFileAndDirInfoData(GNf::ePATH_TYPE get_op){
	 P< P<GNf::sFILE_INFO> > infos;
	 if(!zInfoP)return infos;
	 P<GNf::sFILE_INFO> **_infops = zInfoP;
	 char *_selected = zSelected;
	 char *_pic_ids  = zPicID;
	 unsigned int fi, num;
    #define GT_TMP_FB_SelectFileAndDirInfoData( PROC ) \
	 for(fi = 0, num = 0; fi < zFileNum; ++fi){\
		 if(_selected[fi]){\
			 switch(get_op){\
             case GNf::ePATH_FILE:\
			     if( _pic_ids[fi] != 1) PROC ;\
			     break;\
			 case GNf::ePATH_DIR:\
			     if((_pic_ids[fi] == 1) && (strcmp((*(_infops[fi]))->name,"..") && strcmp((*(_infops[fi]))->name,".")) ) PROC ;\
			     break;\
			 case GNf::ePATH_UNKNOWN:\
			     if((_pic_ids[fi] != 1) || (strcmp((*(_infops[fi]))->name,"..") && strcmp((*(_infops[fi]))->name,".")) ) PROC ;\
			     break;\
		 }	 }\
	 }
	 GT_TMP_FB_SelectFileAndDirInfoData( ++num );
	 if(!num)return infos;
     infos.New(num);
	 GT_TMP_FB_SelectFileAndDirInfoData( infos[num++]=(*(_infops[fi])) );
	 return infos;
}
P<unsigned int > GNu::cFILE_BROWSER::GetSelectVIDs(){
    P<unsigned int> sids;
	GNg::cFRAME_LAUNCHER_FONT *_fframe = zFFrames;
	GNg::sDRAW_RANGE *rng_data = _fframe->zDrawRngData;
    char *_selected = zSelected;
    unsigned int sn, fni, show_text_num = _fframe->zMax_num;
	for(sn = 0, fni = 0; fni < show_text_num; ++fni){
		if(_selected[rng_data[fni].strid])++sn;
	}
    if(!sn)return sids;
	sids.New(sn);
    unsigned int *_sids = sids;
    for(sn = 0, fni = 0; fni < show_text_num; ++fni){
		if(_selected[rng_data[fni].strid]){ _sids[sn++] = fni; }
	}
	return sids;
}
//检测参数指向的名有没副本编号 如有就使参数指向跳过副本编号(指向名的指针)并返回副本编号 如没就返回0
inline int GT_FileBrowserSeekCloneID(char **p_name){
    char *pstr = *p_name;
	if(	   pstr[5] == '#'
		&& pstr[0] >= '0' && pstr[0] <= '9'
	    && pstr[1] >= '0' && pstr[1] <= '9'
	    && pstr[2] >= '0' && pstr[2] <= '9'
	    && pstr[3] >= '0' && pstr[3] <= '9'
	    && pstr[4] >= '0' && pstr[4] <= '9'
	){  *p_name += 6; /*跳过副本编号段*/ return atoi(pstr); }
	return 0;
}
char const* GNu::cFILE_BROWSER::GetNoUsedCloneName(const char *name, bool name_set){
	char *pstr = (char*)name;
    GT_FileBrowserSeekCloneID(&pstr);

	P<char> *_filenames = zFileNames;
	char *_fname;
	GN::sLIST<int> ids;
	int id;
    unsigned int fi = 0;
    for(; fi < zFileNum; ++fi){ //统计当前打开目录里用到该名的所有编号
        _fname = _filenames[fi];
        id = GT_FileBrowserSeekCloneID(&_fname);
		if(!strcmp(pstr, _fname))ids.push_back(id);
	}
	if(zUsedFileNameList.size()){ //在这也找找看
		GN::sLIST< P<char> >::iterator itf = zUsedFileNameList.begin(), etf = zUsedFileNameList.end();
		for(; itf != etf; ++itf){
			_fname = (*itf);
            id = GT_FileBrowserSeekCloneID(&_fname);
		    if(!strcmp(pstr, _fname))ids.push_back(id);
	}	}
	char *ret_name;
	if(ids.size()){
	    ids.sort(); //排序一下ID
		int newid = -1;
		GN::sLIST<int>::iterator it = ids.begin(), et = ids.end();
		for(; it != et; ++it){
			if((*it) - newid > 1)break;
			else newid = (*it);
		}
		ids.clear();
		if(newid >= 0){
			  static char cloneName[128];
			  sprintf(cloneName,"%05d#%s", newid+1, pstr);
			  ret_name = cloneName;
		}else ret_name = pstr;
	}else	  ret_name = (char*)name;	//没同名的就直接返回参数指定的名
	
	if(name_set)zUsedFileNameList.push_back(P<char>(ret_name,0));
    return ret_name;
}
void GNu::cFILE_BROWSER::zSpecClone(){
	cSCROLL_CTRL::zSpecClone();
	zRelate = 0;
	zActIconTmpData = 0;
	zTkThread.Delete();
	zGlobalPaths.Delete(); 
	zGlobalPaths_.Delete();
	zFileNum   = 0;     
	zFileNames.Delete();   
	zInfoP.Delete();			
	zPicID.Delete();       
	zSelected.Delete();    
	zFileList.clear();
	zDirList.clear();
	zUsedFileNameList.clear();
	P<GNg::cFRAME_LAUNCHER_FONT> fframes2(1);
	fframes2->PerfectCopy(*((GNg::cFRAME_LAUNCHER_FONT*)zFFrames));
	zFFrames = fframes2;
	zReArrange  = true;
	zReLoadIcon = true;
}
P<GNu::iUI_OBJECT> GNu::cFILE_BROWSER::Clone(){
	P<cFILE_BROWSER> ui_fb(1);
	cFILE_BROWSER *_ui_fb = ui_fb;
	zTextOut.Release();
	*_ui_fb = *this;
	_ui_fb->zSpecClone();
	return ui_fb;
}
uint8* GNu::cFILE_BROWSER::zDataSave(uint8 *wpos, cSHARE_DATA *share_objs){
	GNg::cFRAME *_frame_icon = zFrameIcon;
	if(_frame_icon->zPlay_obj == (GNg::sIMAGE*)GT_FB_DefaultIconImg){
		_frame_icon->zPlay_obj = &GNg::Image;
		((GT_cUI_SHARE_DATA_W*)share_objs)->list_fileicon.push_back(_frame_icon);
	}
	GT_TMP_SCROLL_LIST_DATA_SAVE( GT_FB_DefaultFrameAct )
	*wpos = (_frame_icon->zPlay_obj == &GNg::Image)?1:0; wpos += 1;
	wpos = zTextOut.zDataSave(wpos, share_objs->image_list);
	memcpy(wpos, &aTextColor,	 4);   wpos += 4;
	memcpy(wpos, &aTextBlend,	 2);   wpos += 2;
	memcpy(wpos, &aShowOffsetX,  2);   wpos += 2;
	memcpy(wpos, &aShowOffsetY,  2);   wpos += 2;
	memcpy(wpos, &aUsMultiSelect,1);   wpos += 1;
	memcpy(wpos, &aIgnoreSED,	 1);   wpos += 1;
	GT_TMP_OBJECT_WRITE_INDEX( sdw->text_list , aFileTypeFilterStr );
	pointer_id = strlen(aOpenDir) + 1;
	memcpy(wpos, &pointer_id,	 2);   wpos += 2;
	memcpy(wpos, aOpenDir, pointer_id);wpos += pointer_id;
	memcpy(wpos, &zSortType,	 2);   wpos += 2;
	memcpy(wpos, &zNearPos,		 2);   wpos += 2;
	memcpy(wpos, &zTextW,		 2);   wpos += 2;
	memcpy(wpos, &zTextH,		 2);   wpos += 2;
	return wpos;
}
uint8* GNu::cFILE_BROWSER::zDataLoad(uint8 *rpos, cSHARE_DATA *share_objs){
	GT_TMP_SCROLL_LIST_DATA_LOAD
	GT_TMP_LOAD_FONT_FRAME_TRANSFER
	if(zFrameIcon && *rpos)zFrameIcon->zPlay_obj = GetIcon();
	rpos += 1;
	rpos = zTextOut.zDataLoad(rpos, share_objs->image_refs);
	memcpy(&aTextColor,	   rpos, 4);   rpos += 4;
	memcpy(&aTextBlend,	   rpos, 2);   rpos += 2;
	memcpy(&aShowOffsetX,  rpos, 2);   rpos += 2;
	memcpy(&aShowOffsetY,  rpos, 2);   rpos += 2;
	memcpy(&aUsMultiSelect,rpos, 1);   rpos += 1;
	memcpy(&aIgnoreSED,	   rpos, 1);   rpos += 1;
	memcpy(&pointer_id,    rpos, 4);   rpos += 4;
	if(pointer_id != 0xffffffff)aFileTypeFilterStr = share_objs->text_refs[pointer_id];
	unsigned short len = *((unsigned short*)rpos); rpos += 2;
	if(len){ memcpy(aOpenDir, rpos, len); rpos += len; memcpy(zLastDir, aOpenDir, len); }
	memcpy(&zSortType,	   rpos, 2);   rpos += 2;
	memcpy(&zNearPos,	   rpos, 2);   rpos += 2;
	memcpy(&zTextW,		   rpos, 2);   rpos += 2;
	memcpy(&zTextH,		   rpos, 2);   rpos += 2;

	zShowReady = zTextOut.GetFontMap() && zTextOut.IsInited();
	zReArrange       = true;
	zReArrangeParame = true;
	zReLoadIcon      = true;
	oW = GN_INT_MAX;
	return rpos;
}
unsigned int GNu::cFILE_BROWSER::DataSave(cSHARE_DATA *share_objs, uint8 *data_out){
	GT_TMP_PACK_DATA_SAVE( 1600 , cFILE_BROWSER )
	return wpos - data_out;
}
unsigned int GNu::cFILE_BROWSER::DataLoad(cSHARE_DATA *share_objs, const uint8 *data){
	GT_TMP_PACK_DATA_LOAD( cFILE_BROWSER )
	if(zFFrames)zFFrames->zMax_num = 0; 
	return rpos - data;
}
#endif //_GN_UIFBROWSER_H_
