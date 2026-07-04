#ifndef _GN_2DTYPE_H_
#define _GN_2DTYPE_H_
/****************************************************** 引擎在不确定类型方式读取文件时的一些跳转处理 ******************************************************************/

//正确设置IMAGE图象文件名和IMAGE的一些属性(原读取的文件名字符串输入, 当输入文件名含扩展名返回0 否则返回文件名的长度, 返回输入文件名是否含有后序编号, 返回输入文件名转换成适合在IMAGE里面储存方式的文件名:这会帮你自动分配好适合的内存空间) 输入文件名里无扩展名就返回0 否则返回能读取该类型文件的方法 如引擎不支持该类型文件格式返回1
GN_API GNg::PF_LOAD_IMG_FILE_DATA GT_ImageFileNameSet(const char *filename, int *fname_len_out, bool *Img_add_0000_out, char **Img_fileName_out);
//获得符合引擎读取图片规则的序列号位置指针(完整路径文件名)返回列号位置指针 返回0:表示没有序列号
GN_API char *GT_ImageFileNameNumberPosGet(const char *filename);

//按引擎跳转处理方式来读取IMAGE图象文件到内存缓冲
GN_API uint8 *GT_ImageFileLoadToData(
	const char *filename,             //文件名 当不包含扩展名时 内部会自动在该名后面接上四位图序编号 如0000 且还尝试用引擎支持的各种图像文件格式来读取
	int		    fname_len,			  //无扩展名时的文件名长度 如有扩展名应填0
 GNg::PF_LOAD_IMG_FILE_DATA load_func,//有扩展名时对应格式的读取方法
	int  id,                          //要读取的图序编号 要是没有序列的图像文件填0即可
	GNg::COLOR_FORMAT *format_out,    //图像颜色格式输出
	int *w_out,                       //图像宽输出
	int *h_out                        //图像高输出
	); //返回图象数据 返回0表示读取失败  返回数据用完后记得delete []data;

namespace GNg{

//读取不确定类型的图象文件到内存缓冲(无扩展名的文件名,要接上读取的图序编号 值-1表示无图序编号,图像颜色格式输出,图像宽输出,图像高输出) 返回图象数据 返回0表示读取失败  返回数据用完后记得delete []data;
GN_API uint8 *ImageLoadFileToData(const char *filename, int id, COLOR_FORMAT *format_out, int *w_out, int *h_out);

//获取最近读取图像没指定文件扩展名时内部帮您加载到的图片文件扩展名() !返回的字符串可能还含有一连串扩展名 如:"png,bmp,tga" 若要做比较拿前3个比较即可
GN_API char *const ImageLoadLastTypeNameGet();

enum eBLEND_MODE			   //混合模式
{
    eBLEND_NULL              = 0,     //不混合直接覆盖
    eBLEND_NORMAL            = 1,     //一般效果
	eBLEND_BRIGHT            = 2,     //发光效果
	eBLEND_SOFT_LIGHT        = 3,     //映射柔光
	eBLEND_BLACK_MASK_BRIGHT = 4,     //黑色蒙版中间发光
	eBLEND_BLACK_MASK        = 5,     //黑色蒙版
	eBLEND_SHADOW            = 6,     //黑影
	eBLEND_PHANTOM           = 7,     //黑影带些幻象
	eBLEND_DEEPEN            = 8,     //颜色叠加深 最适合用来混合灰色的浮雕效果图
	eBLEND_REVERSE           = 9,     //反差乘
    eBLEND_REVERSE_HALF      = 10,    //反差乘半
    eBLEND_MODULATE          = 11,    //调和 在Photoshop里叫正片叠底
	eBLEND_BRIGHT_MAX        = 12,    //强光
	eBLEND_BRIGHT_MODU       = 13,    //调和发光
	eBLEND_DST_ALPHA_MASK	 = 14	  //底色A为蒙版
};
struct GN_API sWINDOW          // ★图形显示窗口 窗口为能适应不同大小而不影响最终于显示内容 使用了后台取景坐标和实际屏幕窗口坐标两个概念 当应用程序绘图或判断时所有的坐标都以后台为准 这样无论实际窗口大小怎样发生变化也可以确保内容全部能正确显示
{
	static short     FBufferColorBit; //帧缓存颜色位数 即位色深
    static bool      FullScreen;      //是否全屏显示
    static bool      WindowZoom;      //窗口可否缩放
	static bool      CursorShow;      //窗口是否有指针显示 
	
	static int      *Hinst;           //应用程序的系统标识
    static int      *Hwnd;            //显示用窗口的系统标识
	static int      *HwndRoot;        //宿主窗口的系统标识  这个通常作为插件形式运行时 显示窗口由外部其他程序创建的  常规应用程序方式运行时该值为0
    
	short            viewX;           //视口左上角X坐标
    short            viewY;           //视口左上角Y坐标
	short            viewW;           //视口宽度
    short            viewH;           //视口高度

	short            zSrcScreenW;     //起始屏幕分辨率W  !窗口首次创建时的宽度
    short            zSrcScreenH;     //起始屏幕分辨率H  !窗口首次创建时的高度
				  
					 //以下几个属性均是按后台坐标计算处理和图像背景制作大小来度量 并不是前台所看到的屏幕坐标

	short            ortho_left;      //后台取景范围左坐标
    short            ortho_top;       //后台取景范围上坐标
    short            ortho_right;     //后台取景范围右坐标
    short            ortho_bottom;    //后台取景范围下坐标
    short            orthoW;          //后台取景范围宽度 相当于 ortho_right  - ortho_left
    short            orthoH;          //后台取景范围高度 相当于 ortho_bottom - ortho_top
    
	short            zBackScreenW;    //起始后台取景范围宽度
    short            zBackScreenH;    //起始后台取景范围高度
				
    float            zScreenRatioW;   //起始后台前台屏幕宽度比 zBackScreenW / zSrcScreenW
    float            zScreenRatioH;   //起始后台前台屏幕高度比 zBackScreenH / zSrcScreenH
				
	float            zCameraFar;      //内部可显示的最远距离 通常在3D模式时才有意义

	inline short CoordWindowToViewX(short wx){ return (short)((wx - viewX) * zScreenRatioW) + ortho_left; } //把水平方向的窗口坐标转为后台坐标
    inline short CoordWindowToViewY(short wy){ return (short)((wy - viewY) * zScreenRatioH) + ortho_top;  } //把垂直方向的窗口坐标转为后台坐标
    
	inline short CoordViewToWindowX(short vx){ return (short)((vx - ortho_left) / zScreenRatioW) + viewX; } //把水平方向的后台坐标转为窗口坐标
    inline short CoordViewToWindowY(short vy){ return (short)((vy - ortho_top ) / zScreenRatioH) + viewY; } //把垂直方向的后台坐标转为窗口坐标
   
	inline short GetViewCX(){ return viewX + (viewW >> 1); }         //获取当前视口中心X坐标
	inline short GetViewCY(){ return viewY + (viewH >> 1); }         //获取当前视口中心Y坐标
	
	inline short GetOrthoCX(){ return ortho_left + (orthoW >> 1); }  //获取当前后台取景范围中心X坐标
	inline short GetOrthoCY(){ return ortho_top  + (orthoH >> 1); }  //获取当前后台取景范围中心Y坐标

	inline float GetCameraFar(){ return zCameraFar; }     //获得当前窗口内部视区可显示的最远距离 通常在3D模式时才有意义
};
}; //end namespace GNg
#endif //endif _GN_2DTYPE_H_