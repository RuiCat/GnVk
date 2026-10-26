#ifndef _GN_LAYOUT_H_
#define _GN_LAYOUT_H_
namespace GNu {

enum eNEAR_TYPE { //靠边类型
    eNEAR_NULL, //不靠
    eNEAR_LEFT, //靠左
    eNEAR_RIGHT, //靠右
    eNEAR_TOP, //靠上
    eNEAR_BOTTOM //靠下
};
enum eNEAR_MODE { //靠边方式
    eNEAR_NONE, //不靠
    eNEAR_LEFT_TOP, //靠左或上
    eNEAR_RIGHT_BOTTOM, //靠右或下
    eNEAR_SCALE //按比列拉伸
};
//获得宽高指定范围在 靠在指定矩型里的正确位置(位置X输出, 位置Y输出, 宽度, 高度, 矩型范围位置X, 位置Y, 矩型宽度, 矩型高度,水平靠位, 垂直靠位)
inline void NearRectRangeGet(int &x_out, int &y_out, int w, int h, int rx, int ry, int rw, int rh, float near_pointU, float near_pointV) {
    x_out = rx + (int)((float)(rw - w) * near_pointU);
    y_out = ry + (int)((float)(rh - h) * near_pointV);
}
//获得宽高指定范围在 靠在指定矩型里的正确位置(位置X输出, 位置Y输出, 宽度, 高度, 矩型范围位置X, 位置Y, 矩型宽度, 矩型高度, 水平靠位 该值会以0~65535映射成0.0f~1.0f来计算, 垂直靠位 该值会以0~65535映射成0.0f~1.0f来计算)
inline void NearRectRangeGet(short &x_out, short &y_out, short w, short h, short rx, short ry, short rw, short rh, unsigned short near_pointU, unsigned short near_pointV) {
    x_out = rx + (short)((int)(rw - w) * near_pointU >> 16);
    y_out = ry + (short)((int)(rh - h) * near_pointV >> 16);
}
}; // namespace GNu
namespace GNg {

enum eENTER_TYPE { //自动换行类型
    eENTER_NULL, //不换行
    eENTER_LEFT_RIGHT, //超出左右边框会自动换行
    eENTER_TOP_BOTTOM //超出上下边框会自动换行
};
struct GN_API sARRANGE_FMT //排列格式 [][][][]
{
    bool rect_fill; //拉伸填满于矩形上
    float near_pointU; //水平靠位U偏移 比如 0.0f:左  0.5f:中  1.0f:右
    float near_pointV; //垂直靠位V偏移 比如 0.0f:上  0.5f:中  1.0f:下
    short i_compartment; //横排列间距 值0:自动合适的间距
    short j_compartment; //行排列间距 值0:自动合适的间距
    short width; //宽度 值0:使用原本的宽度 负值可以得到翻转的镜象
    short height; //高度 值0:使用原本的高度 负值可以得到翻转的镜象
    unsigned char auto_enter; //自动换行类型 值参考: eENTER_TYPE
    bool erect_print; //是否竖向排列
    bool overturn_print; //是否反向排列 如汉字古文从右到左

    sARRANGE_FMT() {}

    //构造排列格式(是否填满矩形筐, 水平靠位0.0f-1.0f, 垂直靠位0.0f-1.0f, 横间距, 行间距, 宽度, 高度, 自动换行类型, 是否竖向排列, 是否反向排列)
    sARRANGE_FMT(bool _rect_fill, float _near_pointU = 0.0f, float _near_pointV = 0.0f, short _i_compartment = 0, short _j_compartment = 0, short _width = 0, short _height = 0, unsigned char _auto_enter = eENTER_NULL, bool _erect_print = 0, bool _overturn_print = 0) : rect_fill(_rect_fill), near_pointU(_near_pointU), near_pointV(_near_pointV), i_compartment(_i_compartment), j_compartment(_j_compartment), width(_width), height(_height), auto_enter(_auto_enter), erect_print(_erect_print), overturn_print(_overturn_print) {}

    //初始化排列格式(是否填满文本筐,水平靠位0.0f-1.0f,垂直靠位0.0f-1.0f,水平字距离,垂直字距离,字宽,字高,自动换行模式,是否竖向输出,是否反向输出）
    void Init(bool _rect_fill, float _near_pointU = 0.0f, float _near_pointV = 0.0f, short _i_compartment = 0, short _j_compartment = 0, short _width = 0, short _height = 0, unsigned char _auto_enter = eENTER_NULL, bool _erect_print = 0, bool _overturn_print = 0);
};
extern GN_API sARRANGE_FMT ArrangeFmt; //默认排列格式 ArrangeFmt(false);

template <class T>
struct sDRAW_AREA_ //绘制区
{
    T x, y; //位置坐标
    T w, h; //绘制大小
    unsigned int c; //绘制颜色

    inline sDRAW_AREA_() {}

    //构造绘制区  (位置X, 位置Y, 宽度, 高度, 颜色)
    inline sDRAW_AREA_(T _x, T _y, T _w, T _h, unsigned int _c) : x(_x), y(_y), w(_w), h(_h), c(_c) {}

    //初始化绘制区(位置X, 位置Y, 宽度, 高度, 颜色)
    inline void Init(T _x, T _y, T _w, T _h, unsigned int _c) {
        x = _x;
        y = _y;
        w = _w;
        h = _h;
        c = _c;
    }
};
typedef sDRAW_AREA_<int> sDRAW_AREA; //绘制区
typedef sDRAW_AREA_<short> sDRAW_AREA2; //两字节坐标的绘制区

struct GN_API sDRAW_IN_AREA : public sDRAW_AREA2 //绘制里区
{
    short nmin_x; //横向靠位最小边距
    short nmin_y; //竖向靠位最小边距
    uint8 ntype_x; //横向靠边方式 取值参看 eNEAR_MODE
    uint8 ntype_y; //竖向靠边方式 取值参看 eNEAR_MODE

    sDRAW_IN_AREA();

    //调整外区的宽高使能确保包含整个里区(外区)
    void EnsureInclude(sDRAW_AREA2 *rng);

    //获得里区在外区的起始绘制位置(位置X输出, 位置Y输出, 外区)返回里区与外区调和后的颜色
    unsigned int DrawBeginPosGet(short &x_out, short &y_out, sDRAW_AREA2 *rng);

    //获得里区在外区的绘制范围和颜色(返回受影响后的里区范, 外区)
    void DrawRangeGet(sDRAW_AREA2 &out, sDRAW_AREA2 *rng);
};
struct GN_API sDRAW_RANGE //绘制范围
{
    int x, y; //位置坐标
    int w, h; //绘制大小
    float u1, v1; //贴图截取左上范围
    float u2, v2; //贴图截取右下范围
    unsigned short tid; //贴图索引
    unsigned short strid; //在字串里的字符索引号

    inline sDRAW_RANGE() {}

    //构造绘制范围  (位置X, 位置Y, 宽度, 高度, 贴图截取U坐标, 贴图截取V坐标, 贴图截取U2坐标, 贴图截取V2坐标, 贴图索引, 在字串里的字符索引号)
    inline sDRAW_RANGE(int _x, int _y, int _w = 32, int _h = 32, float _u1 = 0.0f, float _v1 = 0.0f, float _u2 = 1.0f, float _v2 = 1.0f, unsigned short _tid = 0, unsigned short _strid = 0) : x(_x), y(_y), w(_w), h(_h), u1(_u1), v1(_v1), u2(_u2), v2(_v2), tid(_tid), strid(_strid) {}

    //初始化绘制范围(位置X, 位置Y, 宽度, 高度, 贴图截取U坐标, 贴图截取V坐标, 贴图截取U2坐标, 贴图截取V2坐标, 贴图索引, 在字串里的字符索引号)
    inline void Init(int _x, int _y, int _w = 32, int _h = 32, float _u1 = 0.0f, float _v1 = 0.0f, float _u2 = 1.0f, float _v2 = 1.0f, unsigned short _tid = 0, unsigned short _strid = 0) {
        x = _x;
        y = _y;
        w = _w;
        h = _h;
        u1 = _u1;
        v1 = _v1;
        u2 = _u2;
        v2 = _v2;
        tid = _tid;
        strid = _strid;
    }
};
extern GN_API sDRAW_RANGE DrawRange; //默认绘制范围 DrawRange(0,0);

//按排列要求计算所需绘制单元范围数据
int ComputeArrangeRangeData(
    sDRAW_RANGE **p_ranges_buf, //指向排好的范围数据输出缓冲 这会帮您自动管理空间 用完记得要delete []p_ranges_buf
    int &buf_count, //输出缓冲当前可装填范围个数 这会帮您自动管理大小
    char *text, //格式描述文本 例如"hallo\nworld\n" 其中'\n'是要换行
    unsigned short count, //要排列的个数 0为自动按格式描述文本的字数
    int x, //排版矩形筐的坐标X
    int y, //排版矩形筐的坐标Y
    int w, //排版矩形筐的宽度
    int h, //排版矩形筐的高度
    int ofs_x, //筐内X偏移量
    int ofs_y, //筐内Y偏移量
    const sARRANGE_FMT *parame, //排列格式
    uint8 mode, //排列方式 0:适合常规图像或数字0-9排列 1:适合ASCII字符排列  2:适合含有汉字的GB2312字符排列
    float tu_w, //绘制单元截取纹理的U范围
    float tv_h, //绘制单元截取纹理的V范围
    unsigned short *row_sids_out /*[256]*/, //返回每行开头的起始可视ID数据
    unsigned char *row_num_out, //返回排好的行数
    short *tw_out = 0, //如有指定返回排好的内容实际所占宽度
    short *th_out = 0 //如有指定返回排好的内容实际所占高度

); //返回有多少个绘制单元范围 !!现在这功能暂时限制住一行最多排256个 最多排256行
}; //end namespace GNg

#endif //_GN_LAYOUT_H_