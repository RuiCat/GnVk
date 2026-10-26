#ifndef _GN_2D_H_
#define _GN_2D_H_

namespace GNg {
class GN_API cVIEW2D : public sWINDOW // ★2D视口 提供实现多个子视图 并管理好图像缩放比例和坐标映射转换
{
  public:
    //以下几个属性均是按后台坐标计算处理和图像背景制作大小来度量 并不是前台所看到的屏幕坐标

    int mouseMX; //鼠标点击或移动后的相对于该视口的新位置x
    int mouseMY; //鼠标点击或移动后的相对于该视口的新位置y
    int mouseMSX; //鼠标移动速度x 使用if(GN::MouseM) 检测当前时刻是否发生移动 当有移动该值才有意义
    int mouseMSY; //鼠标移动速度y 使用if(GN::MouseM) 检测当前时刻是否发生移动 当有移动该值才有意义

    short *touchX; //点触摸时的相对于该视口的新位置x
    short *touchY; //点触摸时的相对于该视口的新位置y
    short *touchSX; //触摸拖动速度x 使用if(GN::TouchM) 检测当前时刻是否发生拖动 当有拖动该值才有意义
    short *touchSY; //触摸拖动速度y 使用if(GN::TouchM) 检测当前时刻是否发生拖动 当有拖动该值才有意义

    short orthoOffsetX; //后台取景范围x方向的被偏移量 这个通常是由于系统视图遇到一些突发情况需要对整个视区内容进行平移时(如系统屏幕软键盘弹出遮挡了视图) 用以来通知你当前的偏移量
    short orthoOffsetY; //后台取景范围y方向的被偏移量 ...同上
    short softKeyboardX; //当有软键盘打开时的位置X
    short softKeyboardY; //当有软键盘打开时的位置Y
    short softKeyboardW; //当有软键盘打开时的宽度  !软键盘还没打开时该值通常为0
    short softKeyboardH; //当有软键盘打开时的高度  !软键盘还没打开时该值通常为0

    short zOrthoLeftTg; //后台取景范围左当要偏移时的目标位置
    short zOrthoLeftSrc; //后台取景范围当有要偏移时 记录取景范围左还没偏移时的原位置
    short zOrthoTopTg; //后台取景范围上当要偏移时的目标位置
    short zOrthoTopSrc; //后台取景范围当有要偏移时 记录取景范围上还没偏移时的原位置
    bool zHasOrthoOfsTg; //后台取景范围是否有偏移目标
    bool zHasOrthoOfsIng; //后台取景范围是否正在偏移中

    static GN::sLIST<cVIEW2D *> zTotalList; //记录应用程序运行时的所有视口对象
    void zScissorRectSet(int x, int y, int w, int h); //设置显示剪裁区划范围(区划坐标x, 坐标y, 宽度, 高度)

    static P<GNg::sIMAGE> screenImg; //屏幕截图			!截取屏幕开销较大 如系统内部已经更新过这个图 您可以直接使用
    static unsigned int screenImgUpdateFps; //屏幕截图更新帧号 !您可以通过这个判断截图是否最新的 如不是最新您可以手动去更新它

    ~cVIEW2D();
    cVIEW2D();

    //架设视口为2D渲染模式 !子视口内渲染最好在主视口渲完后再进行 避免频繁切换视口带来额外性能开销
    void BeginRender2D();
    //结束该视口2D与3D渲染 如拥有子视口 子视图区渲染完后最好调用一下该函数 以免多视口时容易引起混乱
    void EndRender2Dand3D();

    //初始化视口(视口左上角位置X, 位置Y, 视口W度, 视口H度, 2D后台取景范围左, 2D后台取景范围上, 2D后台取景范围右:默认0为自动, 2D后台取景范围下:默认0为自动)
    void Init(short view_x, short view_y, short view_w, short view_h, short back_rng_left, short back_rng_top, short back_rng_right = 0, short back_rng_bottom = 0);

    //重设置视口位置大小(新位置X, 新位置Y, 新宽度, 新高度, 是否自动调整2D缩放比例, 是否自动调整3D缩放比例) 当游戏窗口改变了一定要更新游戏视口 不然的话图形会变形
    void ReSize(short new_x, short new_y, short new_w, short new_h, bool auto_scale2D = true, bool auto_scale3D = true);

    //加入安装一个指定范围为当前剪裁范围(范围坐标x, 坐标y, 范围宽度, 范围高度, 新加的剪裁范围是否嵌套于上一层使用的范围内) !!这个范围是相对后台画面并随子视图影响的 而并不是绝对屏幕窗口坐标 这个支持多层嵌套 但必需和 ScissorRectRemove 配对使用 剪裁范围嵌套最大可以支持32层
    void ScissorRectSetup(int x, int y, int w, int h, bool is_include = true);

    //移去当前最面层的一个剪裁范围 并恢复上一层的剪裁范围有效 如果移去的是最后一个剪裁范围那将会禁用剪裁功能()
    void ScissorRectRemove();

    //返回当前正在使用的剪裁区划范围(范围输出) 返回 0:没使用剪裁  其它值:当前正在使用的剪裁区划层数号 层数号从1开始
    uint8 ScissorRectGet(GNg::sRECT *rc);

    //将视口的图象内容保存到内存缓冲(返回缓冲数据大小, 相对视口里的坐标X, 坐标Y, 宽度, 高度, 是否翻转列象素)返回数据缓冲 用完记得要delete [] !!调用这个会比较慢 可能会带来卡停一下的感觉 缓冲数据通常为 RGBA 格式
    uint8 *SaveToData(int *size_out, short vx = 0, short vy = 0, short vw = 0, short vh = 0, bool col_turn = false);

    //将视口的图象内容保存到PNG图像文件(保存路径文件名, 相对视口里的坐标X, 坐标Y, 宽度, 高度) !!调用这个会比较慢 可能会带来卡停一下的感觉
    void SaveToFile(const char *filename, short vx = 0, short vy = 0, short vw = 0, short vh = 0);

    //将视口的图象内容保存到图像(图象, 相对视口里的坐标X, 坐标Y, 宽度, 高度, 是否使用多级渐进纹理, 是否翻转列象素) !!如果图象大小和视口大小不匹配将自动删除原有图象从新创建一个新的和视口大小匹配的图象 !!在OpenGL模式下 位色深为16位时调用 或参数col_turn为false时调用 需要增加比较大的额外象素转换开销值得注意
    void SaveToImageData(sIMAGE *img_out, short vx = 0, short vy = 0, short vw = 0, short vh = 0, bool us_mipmap = false, bool col_turn = false);

    //判断鼠标位置是否与矩形范围相交(范围坐标X, 坐标Y, 范围宽, 高) 返回是否相交
    inline bool HitRange(int x, int y, int w, int h) { return !(mouseMX < x || mouseMX > x + w || mouseMY < y || mouseMY > y + h); }
    //判断鼠标位置是否与矩形范围相交(矩形范围) 返回是否相交
    inline bool HitRange(const GNg::sRECT *rc) { return !(mouseMX < rc->left || mouseMX > rc->right || mouseMY < rc->top || mouseMY > rc->bottom); }
    //判断鼠标位置是否与绘制区域相交(绘制区域) 返回是否相交
    inline bool HitRange(const GNg::sDRAW_AREA2 *area) { return !(mouseMX < area->x || mouseMX > area->x + area->w || mouseMY < area->y || mouseMY > area->y + area->h); }

    //获取指定范围里首个刚按下的触摸点ID(范围左, 上, 右, 下)返回触摸点ID 返回255表示范围里没有刚按下的触摸点
    uint8 GetRangeTouchDownId(short left, short top, short right, short bottom);
    //获取指定范围里首个刚按下的触摸点ID(矩形范围)返回触摸点ID 返回255表示范围里没有刚按下的触摸点
    uint8 GetRangeTouchDownId(const GNg::sRECT *rc);
};
extern GN_API cVIEW2D *ViewCur; //当前正在渲染的视口
extern GN_API cVIEW2D View; //主默认视口
extern GN_API sIMAGE Image; //默认空图像 - 未初始化分配空间
extern GN_API sIMAGE BackScreen128; //临时后台渲染图用 一般渲染到它里面的内容即时拿来用 因为可能有很多处理都共享到它
extern bool RenderRotSupport; //标记图形系统渲染是否支持旋转变换

struct sIMAGE_GROUP //图像组
{
    P<sIMAGE> imgs; //图像数组
    P<unsigned int> ids; //对应每个图像ID
};
struct GN_API sIMAGE_OBJ : public sDRAW_AREA2 //图像对象
{
    P<sIMAGE> img; //图像
    unsigned short bmode; //混合模式

    inline void Show() { ImageDraw(img, x, y, w, h, c, 0, bmode); } //显示图像

    //判断图像是否与矩形范围相交(范围坐标X, 坐标Y, 范围宽, 高) 返回是否相交
    inline bool HitRange(int _x, int _y, int _w, int _h) { return !(x > _x + _w || y > _y + _h || _x > x + w || _y > y + h); }
    //判断图像是否与矩形范围相交(矩形范围) 返回是否相交
    inline bool HitRange(const GNg::sRECT *rc) { return !(x > (short)rc->right || y > (short)rc->bottom || (short)rc->left > x + w || (short)rc->top > y + h); }
    //判断图像是否与绘制区域相交(绘制区域) 返回是否相交
    inline bool HitRange(const GNg::sDRAW_AREA2 *area) { return !(x > area->x + area->w || y > area->y + area->h || area->x > x + w || area->y > y + h); }
    //判断图像是否与视口相交(视口) 返回是否相交
    inline bool HitView(cVIEW2D *view) { return !(x > view->ortho_right || y > view->ortho_bottom || view->ortho_left > x + w || view->ortho_top > y + h); }

    uint8 *zDataSave(uint8 *wpos, GN::sLIST<P<sIMAGE>> &imgs_collect); //保存图像对象的2进制数据(当前写入位置, 收集到的图像列表)返回最后的写完位置
    uint8 *zDataLoad(uint8 *rpos, P<P<sIMAGE>> &imgs_ref); //读取图像对象的2进制数据(当前读取位置, 图像引用列表)返回最后的读完位置
};
//按图像里无扩展名的文件路径来检测存在且支持的图象类型(图像, 保留扩展参数)返回图象类型 返回 eIMG_FILE_UNKNOWN 表示未知
GN_API eIMG_FILE_TYPE CheckHasImgFmtTypeFromImgPath(const sIMAGE *img, void *ph = GNf::sFILE::DefaultH);

//复制出图像的透明通道信息到一个新的图像里(图像, 抗剧齿平滑过渡质量等级)返回仅有透明通道信息的新图像 !!这个调用不支持序列帧 而且必须要在绘制2D时段用 且需要图形硬件支持渲染目标
GN_API P<sIMAGE> CopyOutImageAlpha(sIMAGE *img, unsigned char linearlv = 1);

//导出指定图象到文件(导出路径, 图象)成功返回导出的文件完整路径 失败返回空指针 !路径不能包含文件名  !!该功能只能导出通过加载进来的图象 不能导出程序动态生成的图像
GN_API P<char> ExportImageFile(const char *exp_path, GNg::sIMAGE *img);

//将图象内容保存为PNG图像文件(文件名, 截取相对图象里的坐标X, 坐标Y, 宽度, 高度)返回是否成功  !!调用这个会比较慢 可能会带来卡停一下的感觉 !如果图象为一组动画序列图象 那么会在你指定的文件名后面自动接上0000.png 等序号来产生多个图片  !该功能在OpenGL模式下需要图像能作为渲染目标支持 调用 sIMAGE::RenderTargetSupport() 来检测是否支持渲染目标
GN_API bool SaveImageToFile(const char *filename, sIMAGE *img, short x = 0, short y = 0, short w = 0, short h = 0);

//绘制2D图象的部分(	 图象,		  显示坐标X,显示坐标Y, 显示的宽, 显示的高,      图的底色,                 图ID,                  混合模式,  部分比列 1.0f为整个图像, 部分图像的靠边方向)
GN_API void ImageDrawRng(sIMAGE *img, int x, int y, int w, int h, unsigned int color, unsigned short tex_id, unsigned short blendmode, float scl, GNu::eNEAR_TYPE ntype);

//绘制2D图象的部分(	 图象,		  显示坐标X,显示坐标Y, 显示的宽, 显示的高,      图的底色,                 图ID,                  混合模式,   左上角的U坐标, 左上角的V坐标,   右下角的U坐标,      右下角的V坐标)
inline void ImageDrawRng(sIMAGE *img, int x, int y, int w, int h, unsigned int color, unsigned short tex_id, unsigned short blendmode, float u1, float v1, float u2, float v2) {
    ImageDraw(img, x, y, w, h, color, tex_id, blendmode, u1 * img->uRange, v1 * img->vRange, u2 * img->uRange, v2 * img->vRange);
}
//	按指定的旋转角度来绘制2D图象(要显示的图象对象, 显示坐标X,显示坐标Y,显示的宽, 显示的高, 旋转角度, 旋转中心点落在相对图横向的比率,旋转中心点落在相对图竖向的比率,  图的底色, 图ID, 混合模式)
GN_API void ImageDrawRot(sIMAGE *img, int x, int y, int w = 0, int h = 0, float r = 0.0f, float cu = 0.0f, float cv = 0.0f, unsigned int color = 0xffffffff, unsigned short texID = 0, unsigned short blendmode = 1);

//	绘制9格关联组成的边筐图(要显示的图象对象, 显示坐标X,显示坐标Y,显示的宽, 显示的高,           图的底色,            混合模式,       是否重复平铺填满中间内容 值并入|2:漏空中间) 这个比较适合来绘制能拉伸的按钮 窗口等 !!注意9格图的长和宽必须是2的陪数
GN_API void ImageDraw9Square(sIMAGE *img9, int x, int y, int w = 0, int h = 0, unsigned int color = 0xffffffff, unsigned short blendmode = 1, uint8 repeat = 0);

//	避开指定的一组矩型区划绘制2D图象(要显示的图象对象, 显示坐标X,显示坐标Y,显示的宽, 显示的高,  图的底色,                 图ID,                  混合模式,             矩形区划数组,     矩型数)
GN_API void ImageDrawMaskRect(sIMAGE *img, int x, int y, int w, int h, unsigned int color, unsigned short texID, unsigned short blendmode, const sRECT *rects, unsigned short rect_n = 1);

//	避开指定的一组矩型区划绘制2D图象(要显示的图象对象, 显示坐标X,显示坐标Y,显示的宽, 显示的高,  图的底色,                 图ID,                  混合模式,      左上角的U坐标, 左上角的V坐标,   右下角的U坐标,      右下角的V坐标,    矩形区划数组,     矩型数) !这里的UV参数均未匹配不同平台的 为保正各平台显示一样 应将所填参数都乘上图象的UV占用范围 U * img->uRange V * img->vRange
GN_API void ImageDrawMaskRect(sIMAGE *img, int x, int y, int w, int h, unsigned int color, unsigned short texID, unsigned short blendmode, float left_top_u, float left_top_v, float right_bottom_u, float right_bottom_v, const sRECT *rects, unsigned short rect_n = 1);
//	避开指定的一组矩型区划绘制2D图象的部分(要显示的图象对象, 显示坐标X,显示坐标Y,显示的宽, 显示的高, 图的底色,            图ID,                  混合模式,      左上角的U坐标, 左上角的V坐标,   右下角的U坐标,      右下角的V坐标,    矩形区划数组,     矩型数)
inline void ImageDrawMaskRectRng(sIMAGE *img, int x, int y, int w, int h, unsigned int color, unsigned short texID, unsigned short blendmode, float u1, float v1, float u2, float v2, const sRECT *rects, unsigned short rect_n = 1) {
    ImageDrawMaskRect(img, x, y, w, h, color, texID, blendmode, u1 * img->uRange, v1 * img->vRange, u2 * img->uRange, v2 * img->vRange, rects, rect_n);
}
//	抖动模糊绘制2D图象(要显示的图象对象, 显示坐标X,显示坐标Y,显示的宽, 显示的高,          图的底色,                 图ID,              混合模式               ,  重复绘制的次数,       重复绘制多个图之间的抖动比率)
GN_API void ImageDrawDitherBlur(sIMAGE *img, int x, int y, int w = 0, int h = 0, unsigned int color = 0xffffffff, unsigned short texID = 0, unsigned short blendmode = 1, unsigned short count = 4, float dither = 0.03f);

//	抖动模糊绘制2D图象(要显示的图象对象, 显示坐标X,显示坐标Y,显示的宽, 显示的高,          图的底色,                 图ID,              混合模式               ,  左上角的U坐标, 左上角的V坐标,   右下角的U坐标,      右下角的V坐标     重复绘制的次数,       重复绘制多个图之间的抖动比率) !这里的UV参数均未匹配不同平台的 为保正各平台显示一样 应将所填参数都乘上图象的UV占用范围 U * img->uRange V * img->vRange
GN_API void ImageDrawDitherBlur(sIMAGE *img, int x, int y, int w, int h, unsigned int color, unsigned short texID, unsigned short blendmode, float left_top_u, float left_top_v, float right_bottom_u, float right_bottom_v, unsigned short count = 4, float dither = 0.03f);
//	抖动模糊绘制2D图象的部分(要显示的图象对象,显示坐标X,显示坐标Y,显示的宽,显示的高,      图的底色,                 图ID,              混合模式               ,  左上角的U坐标, 左上角的V坐标,   右下角的U坐标,      右下角的V坐标     重复绘制的次数,       重复绘制多个图之间的抖动比率)
inline void ImageDrawDitherBlurRng(sIMAGE *img, int x, int y, int w, int h, unsigned int color, unsigned short texID, unsigned short blendmode, float u1, float v1, float u2, float v2, unsigned short count = 4, float dither = 0.03f) {
    ImageDrawDitherBlur(img, x, y, w, h, color, texID, blendmode, u1 * img->uRange, v1 * img->vRange, u2 * img->uRange, v2 * img->vRange, count, dither);
}
//	抖动模糊绘制2D图象投影到视口大小在指定位置范围里截取部分(要显示的图象对象, 显示坐标X,显示坐标Y,显示的宽, 显示的高, 图的底色, 图ID,  混合模式               ,  重复绘制的次数,       重复绘制多个图之间的抖动比率, 图像是否列象素翻转的) !这个配合屏幕截图 非常适合用来绘制毛玻璃UI效果的底图
GN_API void ImageDrawDitherBlurProjToViewRng(sIMAGE *img, int x, int y, int w, int h, unsigned int color = 0xffffffff, unsigned short texID = 0, unsigned short blendmode = 1, cVIEW2D *view = ViewCur, unsigned short count = 4, float dither = 0.03f, bool is_col_turn = false);

//	绘制一条2D线(线式样图象对象, 线点1坐标X,线点1坐标Y, 线点2坐标X,线点2坐标Y, 线宽, 线中心点落在相对线宽的比率,  图的底色, 图ID, 混合模式) !为了能兼容老旧设备 线式样图宽度最好为2的陪数
GN_API void Draw2DLine(sIMAGE *img, int x, int y, int x2, int y2, int line_w = 1, float cv = 0.5f, unsigned int color = 0xffffffff, unsigned short texID = 0, unsigned short blendmode = 1);

//	绘制一条2D线(线式样图象对象,         线点1,           线点2,               线宽, 线中心点落在相对线宽的比率,  图的底色, 图ID, 混合模式) !为了能兼容老旧设备 线式样图宽度最好为2的陪数
GN_API void Draw2DLine(sIMAGE *img, const sVEC2 *p1, const sVEC2 *p2, int line_w = 1, float cv = 0.5f, unsigned int color = 0xffffffff, unsigned short texID = 0, unsigned short blendmode = 1);

//!!该功能还未实现 直接绘制线段到内存(目标内存地址:2维数组, 2维数组宽, 2维数组高, 2维数组每象素的颜色位数, 坐标X数组, 坐标Y数组, 点数, 线宽            线颜色,                 是否循环闭合线,  裁剪筐) 当有设置裁剪筐时象素的填充绘制被限制在筐内
GN_API void Draw2DLinesToMem(unsigned char *p_memory, int mw, int mh, short color_bit, const int *px, const int *py, int count, int line_w = 1, unsigned int color = 0xffffffff, bool is_loop = true, const sRECT *scissor = 0);
}; // namespace GNg
namespace GNu {

//返回靠取景范围左边偏移x量的坐标(偏移量)返回坐标
inline int NearLx(int offset_x = 0) {
    return GNg::ViewCur->ortho_left + offset_x;
}
//返回靠取景范围右边偏移x量的坐标(偏移量)返回坐标
inline int NearRx(int offset_x = 0) {
    return GNg::ViewCur->ortho_right + offset_x;
}
//返回靠取景范围上边偏移y量的坐标(偏移量)返回坐标
inline int NearTy(int offset_y = 0) {
    return GNg::ViewCur->ortho_top + offset_y;
}
//返回靠取景范围下边偏移y量的坐标(偏移量)返回坐标
inline int NearBy(int offset_y = 0) {
    return GNg::ViewCur->ortho_bottom + offset_y;
}
//返回靠取景范围中心偏移x量的坐标(偏移量)返回坐标
inline int NearCx(int offset_x = 0) {
    return (GNg::ViewCur->ortho_left + (GNg::ViewCur->orthoW >> 1)) + offset_x;
}
//返回靠取景范围中心偏移y量的坐标(偏移量)返回坐标
inline int NearCy(int offset_y = 0) {
    return (GNg::ViewCur->ortho_top + (GNg::ViewCur->orthoH >> 1)) + offset_y;
}

//返回取景范围水平边筐靠近量U再偏移x量的坐标(U偏移左0.0-右1.0, 偏移量)返回坐标
inline int NearUx(float near_pointU, int offset_x = 0) {
    return (int)(near_pointU * (float)GNg::ViewCur->orthoW) + GNg::ViewCur->ortho_left + offset_x;
}
//返回取景范围垂直边筐靠近量V再偏移y量的坐标(V偏移上0.0-下1.0, 偏移量)返回坐标
inline int NearVy(float near_pointV, int offset_y = 0) {
    return (int)(near_pointV * (float)GNg::ViewCur->orthoH) + GNg::ViewCur->ortho_top + offset_y;
}

//返回取景范围边筐内靠近量UV再偏移  xy  量的坐标(U偏移左0.0-右1.0, V偏移上0.0-下1.0, x偏移量, y偏移量)返回坐标
GN_API GNg::sVEC2 Near(float near_pointU, float near_pointV, int offset_x = 0, int offset_y = 0);
//返回取景范围边筐内靠近量UV再偏移offset量的坐标(U偏移左0.0-右1.0, V偏移上0.0-下1.0, 偏移量)返回坐标
GN_API GNg::sVEC2 Near(float near_pointU, float near_pointV, GNg::sVEC2 offset);

}; //end namespace GNu

//准备为2D渲染模式()
GN_API void GN_RenderBegin2D();

//初始化图形相关的内容(窗口标识, 窗口分辨率W, 窗口分辨率H, 位色深, 后台取景范围坐标左, 后台取景范围坐标上, 后台取景范围坐标右:0为自动, 后台取景范围坐标下:0为自动, 是否全屏, 窗口可否缩放)
GN_API void GF_InitGraphics(const int *window, short view_w, short view_h, short color_bit, short back_rng_left, short back_rng_top, short back_rng_right, short back_rng_bottom, bool full_screen, bool window_zoom);

//释放图形相关的内容()
GN_API void GF_UnInitGraphics();

#endif //endif _GN_2D_H_
