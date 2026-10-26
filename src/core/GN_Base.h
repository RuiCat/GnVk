#ifndef _GN_BASE_H_
#define _GN_BASE_H_

//向运行日志输出信息(输出格式, 参数队列 ) !如遇到可能会导致运行中断的严重错误 适合用此方法输出报告
GN_API void GN_Log(const char *fmt, ...);

//批打印输出数组的值( 元素类型 , 数组首地址, 一行打印几个 , 共几行 , 打印格式符号跟 printf() 差不多)
#define GN_PrintArray(class_type, p__array, row, col, print__str)                \
    {                                                                            \
        class_type *pp = (class_type *)(p__array);                               \
        unsigned int pj, pi, nj = (unsigned int)(col), ni = (unsigned int)(row); \
        for(pj = 0; pj < nj; pj++) {                                             \
            for(pi = 0; pi < ni; pi++) {                                         \
                GN_Log(print__str, pp[pi + pj * ni]);                            \
            }                                                                    \
            GN_Log("\n");                                                        \
        }                                                                        \
        GN_Log("\n");                                                            \
    }
//打印输出矩阵44各元素的值( 矩阵指针 )
#define GN_PrintMat44(pmat) GN_PrintArray(float, pmat, 4, 4, "%f, ")

//平均化帧时间(时间类型, 本次被记录的时间, 向前取多少个时间值作计算)
#define GN_AverageFrameTimeT(TTYPE, TIME, time_Count)       \
    static TTYPE allframe##TIME = 6 * (time_Count);         \
    TTYPE frame##TIME = allframe##TIME / (time_Count);      \
    allframe##TIME = allframe##TIME - frame##TIME + (TIME); \
    (TIME) = frame##TIME

//平均化帧时间(本次被记录的时间, 向前取多少个时间值作计算)
#define GN_AverageFrameTime(TIME, time_Count) GN_AverageFrameTimeT(unsigned int, TIME, time_Count)

namespace GN {
extern GN_API float TimeRatio; //时刻比率 通常在一些对时间要求准确动画方面的参数乘这个系数以确保单位时间的变化量一至 以免受到卡机的影响
extern GN_API float TimeRatioL; //时刻比率 这个是 TimeRatio 受限制值 TimeRatioL 通常不会大于 3.0f 通常界面的动画不要求太准确的用这个比较合适

/* ------------------------------------------------- 范围段结构定义 -----------------------------------------------------------------*/
template <class T>
struct sSEGMENT //段落结构
{
    T pos; //开始位置
    T len; //长度

    sSEGMENT() {}
    sSEGMENT(T _pos, T _len) : pos(_pos), len(_len) {}
};
}; // namespace GN
template <class T>
struct GT_sSEGMENT_T //记录开始和结束点用
{
    T start;
    T end;

    GT_sSEGMENT_T() {}
    GT_sSEGMENT_T(T _start, T _end) : start(_start), end(_end) {}
};
struct GN_API GT_sSEGMENT_S { //记录开始和结束点用
    unsigned short start;
    unsigned short end;

    GT_sSEGMENT_S() {}
    GT_sSEGMENT_S(unsigned short _start, unsigned short _end) : start(_start), end(_end) {}
};
/************************************************ 一些简便块捷的数学函数 *******************************************************/
#define GN_SIZE_1GB 1073741824
#define GN_SIZE_1MB 1048576
#define GN_SIZE_128K 131072
#define GN_SIZE_64K 65536
#define GN_SIZE_32K 32768
#define GN_SIZE_16K 16384

#define GN_INT_MAX 2147483647
#define GN_INT_MIN (-2147483648)
#define GN_FLOAT_MAX 600000000.0f
#define GN_FLOAT_MIN (-600000000.0f)
#define GN_FLOAT_MIN2 (-10000000.0f)
#define GN_NAMESIZE 64
#define GN_MAXPATH 260
#define GN_MAXFILEPATH 384

#define GT_ANGLE_UNIT 0.01745329252f //角度乘以该值等于弧度
#define GT_ANGLE_1 0.01745329252f
#define GT_ANGLE_5 0.0872664626f
#define GT_ANGLE_36 0.62831853072f
#define GT_ANGLE_45 0.7853981634f
#define GT_ANGLE_90 1.5707963268f
#define GT_ANGLE_175 3.054326191f
#define GT_ANGLE_179 3.1241393611f
#define GT_ANGLE_180 3.1415926536f
#define GT_ANGLE_185 3.2288591162f
#define GT_ANGLE_270 4.7123889804f
#define GT_ANGLE_360 6.2831853072f
#define GN_ANGLE_UNIT 1.0f //一度角
#define GN_CUBE_DGN_LEN 1.414214f //单位格对角线长
#define GN_CUBE_DGN_R 0.707107f //单位格对角线半径

#define GN_05_CUBE_DGN_R -0.206107f //0.5 - GN_CUBE_DGN_R

//点积夹角转为弧度
#define GN_DotAgToRadian(ag) (ag <= -1.0f ? GT_ANGLE_180 : (ag >= 1.0f ? 0.0f : acosf(ag)))

//获得给定周长的圆的半径
inline float GN_CircumToR(float clen) {
    return clen / GT_ANGLE_360;
}

//比较两个整数是否大致相等( 原比较值 , 目标比较值 )
#define GN_ValueSimilar (S, D)((S > D ? S - D : D - S) < (D >> 4))
//比较两个整数是否大致相等( 原比较值 , 目标比较值 ) !这个在值太小时会使差异2的视为相等
#define GN_ValueSimilar2(S, D) ((S > D ? S - D : D - S) < ((D >> 4) < 2 ? 2 : (D >> 4)))
//比较两个整数是否大致相等( 原比较值 , 目标比较值 , 参考差异值 ) !这个在值太小时会使差异2的视为相等
#define GN_ValueSimilar_(S, D, C) ((S > D ? S - D : D - S) < ((C) < 2 ? 2 : (C)))

//比较两个浮点值是否近似(值1, 值2 , 容许误差值 不能为负数)
inline bool GN_ValueCompareFloat(float f1, float f2, float v) {
    f1 -= f2;
    return (f1 < 0.0f ? -f1 : f1) < v;
}
//比较两个浮点值是否相等
#define GN_ValueEqualFloat(F_1, F_2) ((F_1) - (F_2) <= 0.000001f && (F_1) - (F_2) >= -0.000001f)
//比较两个浮点值是否不相等
#define GN_ValueNotEqualFloat(F_1, F_2) ((F_1) - (F_2) > 0.000001f || (F_2) - (F_1) > 0.000001f)
//比较两个双精浮点值是否相等
#define GN_ValueEqualDouble(F_1, F_2) ((F_1) - (F_2) <= 0.0000000001 && (F_1) - (F_2) >= -0.0000000001)
//比较两个双精浮点值是否不相等
#define GN_ValueNotEqualDouble(F_1, F_2) ((F_1) - (F_2) > 0.0000000001 || (F_2) - (F_1) > 0.0000000001)
//判断浮点值是否无效的 (安全类型双关，避免严格别名UB)
inline bool GN_ValueIsInvalidFloat(float F) {
    unsigned int v;
    memcpy(&v, &F, sizeof(v));
    return v == 0xffc00000 || v == 0x7f800000;
}

//使值不小于最小值也不大于最大值(值 , 最小值 , 最大值 )
#define GN_ValueClamp(V, MINV, MAXV) \
    if((V) < (MINV)) (V) = (MINV);   \
    else if((V) > (MAXV)) (V) = (MAXV)

//把浮点值打包为半精度浮点数(浮点值)返回半精度浮点数
inline short GN_ValuePackToHalf(float f) {
    if(!f) return 0;
    int i32;
    memcpy(&i32, &f, sizeof(i32));
    return (((i32 & 0x7fffffff) >> 13) - (0x38000000 >> 13)) | ((i32 & 0x80000000) >> 16);
}
//把半精度浮点数解包为浮点值(半精度浮点数)返回浮点值
inline float GN_ValueUnPackHalf(short hf) {
    if(!hf) return 0.0f;
    int i32 = (((hf & 0x8000) << 16) | ((hf & 0x7fff) << 13)) + 0x38000000;
    float result;
    memcpy(&result, &i32, sizeof(result));
    return result;
}

//把浮点值转换为半精度浮点值(浮点值)返回半精度浮点值 !这个兼容GPU里的数据格式
inline unsigned short GN_ValueFloatToHalf(const float x) {
    unsigned int b;
    memcpy(&b, &x, sizeof(b));
    b += 0x00001000;
    const unsigned int e = (b & 0x7f800000) >> 23;
    const unsigned int m = b & 0x007fffff;
    // e>125 时次正规分支的乘数为 0，但移位表达式仍会求值；先钳制位移量避免无符号回绕 UB
    const unsigned int subShift = (e > 125) ? 0u : (125u - e);
    return (b & 0x80000000) >> 16 | (e > 112) * ((((e - 112) << 10) & 0x7c00) | m >> 13) | ((e < 113) & (e > 101)) * ((((0x007ff000 + m) >> subShift) + 1) >> 1) | (e > 143) * 0x7fff;
}
//把半精度浮点值转换为浮点值(半精度浮点值)返回浮点值 !这个兼容GPU里的数据格式
inline float GN_ValueHalfToFloat(const unsigned short x) {
    const unsigned int e = (x & 0x7c00) >> 10;
    const unsigned int m = (x & 0x03ff) << 13;
    float mf = (float)m;
    unsigned int v;
    memcpy(&v, &mf, sizeof(v));
    v >>= 23;
    // 次正规分支仅在 e==0 && m!=0 时非零，此时 v 约为 140..149，位移 1..10 安全；
    // 其它分支虽乘以 0 但仍会求值，故位移量固定为 0，避免移位计数 >=32 的 UB
    const unsigned int subShift = (e == 0 && m != 0) ? (150u - v) : 0u;
    unsigned int rf = (x & 0x8000) << 16 | (e != 0) * ((e + 112) << 23 | m) | ((e == 0) & (m != 0)) * ((v - 37) << 23 | ((m << subShift) & 0x007fe000));
    float result;
    memcpy(&result, &rf, sizeof(result));
    return result;
}
//获得0.0f - 1.0f的对称均匀散布值() !通过输入上次两个取得值 获得新值 不断迭代 终能均匀地获得0.0f - 1.0f之间的所有值 但不包含0和1 参数通常首次输入为1.0f
GN_API void GN_GetSymmetDispersionValue0_1(float &v /*1.0f*/, float &s /*1.0f*/);

//按格ID来获得均匀散落到的坐标X格ID(格ID, 行格数)返回坐标X格ID
GN_API uint8 GN_GetDiscreteGridIdX(uint8 id, uint8 size);

//按格ID来获得均匀散落到的坐标Y格ID(格ID, 列格数)返回坐标Y格ID
GN_API uint8 GN_GetDiscreteGridIdY(uint8 id, uint8 size);

//返回指定值除2或3更接近整数(指定值)返回2或3
inline float GN_AlmostIntDivision2or3(float v) {
    float v2 = v * 0.5f, v3 = v * 0.3333333f;
    int iv2 = (int)v2, iv3 = (int)v3;
    v2 -= (float)iv2;
    v3 -= (float)iv3;
    return (v2 > 0.5f ? 1.0f - v2 : v2) < (v3 > 0.5f ? 1.0f - v3 : v3) ? 2.0f : 3.0f;
}
//返回指定值除4或9更接近整数(指定值)返回2或3倒数的平方
inline float GN_AlmostIntDivision4or9Inv2(float v) {
    float v2 = v * 0.25f, v3 = v * 0.1111111f;
    int iv2 = (int)v2, iv3 = (int)v3;
    v2 -= (float)iv2;
    v3 -= (float)iv3;
    return (v2 > 0.5f ? 1.0f - v2 : v2) < (v3 > 0.5f ? 1.0f - v3 : v3) ? 0.25f : 0.1111111f;
}
//返回指定值的四舍五入值
inline float GN_Round(float v) {
    int vi = (int)v;
    float vf = (float)vi;
    return (v - vf < 0.5f) ? vf : vf + 1.0f;
}
//返回指定值的四舍五入值
/*inline double GN_Round(double v){
	int64 vi = (int64)v;
	double vf = (double)vi;
	return (v - vf < 0.5) ? vf : vf + 1.0;
}*/
template <class T>
//一维大小转为宽高总量最贴近或大于它一点的二维表示(指定大小, 最大宽度限制, 返回二维宽度, 返回二维高度) 返回二维总量比原一维的大多小个
T GN_SizeToWH(T size, T max_w, T &w_out, T &h_out) {
    if(size <= max_w) {
        w_out = size;
        h_out = 1;
        return 0;
    }
    T h = size / max_w;
    T m = size - (h * max_w);
    if(m) {
        T tn = (max_w - m) / (h + 1);
        w_out = max_w - tn;
        h_out = h + 1;
        return w_out - (m + tn * h);
    } else {
        w_out = max_w;
        h_out = h;
        return 0;
    }
}
template <class T>
//限制宽高不能超过指定最大值并保持宽高比(宽, 高, 最大值)
void GN_LimitMaxWH(T &w, T &h, T max) {
    if(w > h) {
        if(w > max) {
            h = h * max / w;
            w = max;
        }
    } else {
        if(h > max) {
            w = w * max / h;
            h = max;
        }
    }
}
template <class T>
//计算像素梯总格数(梯级数)
inline T GN_PixelLadderAmount(T lv) {
    return (T)(((float)((lv) * (lv)) * 0.5f) + ((float)(lv) * 0.5f));
}
template <class T>
//判断指定数是否为2的N次方数
inline bool GN_NumIs2n(T num) {
    T i = 2;
    for(; i < num; i <<= 1);
    if(num == i) return true;
    else return false;
}
//判断指定数是否为2的N次方数
inline bool GN_NumIs2nL(unsigned int num) {
    if(num > (1 << 31)) return false;
    return GN_NumIs2n(num);
}
//判断指定数是否为2的N次方数
inline bool GN_NumIs2nS(unsigned short num) {
    if(num > (1 << 15)) return false;
    return GN_NumIs2n(num);
}
template <class T>
//获得一个大于或等于指定数的2N次方数
inline T GN_NumGet2n(T num) {
    T v = 2;
    while(v < num) v <<= 1;
    return v;
}
//获得一个大于或等于指定数的2N次方数
inline unsigned int GN_NumGet2nL(unsigned int num) {
    return GN_NumGet2n(num);
}
//获得一个大于或等于指定数的2N次方数
inline unsigned short GN_NumGet2nS(unsigned short num) {
    return GN_NumGet2n(num);
}
//计算值次序
#define GT_TMP_VALUE_SEQUENCE3(V0, V1, V2) \
    if(V0 > V1) {                          \
        if(V1 > V2) {                      \
            *l = 0;                        \
            *m = 1;                        \
            *s = 2;                        \
        } else if(V0 > V2) {               \
            *l = 0;                        \
            *m = 2;                        \
            *s = 1;                        \
        } else {                           \
            *l = 2;                        \
            *m = 0;                        \
            *s = 1;                        \
        }                                  \
    } else {                               \
        if(V0 > V2) {                      \
            *l = 1;                        \
            *m = 0;                        \
            *s = 2;                        \
        } else if(V1 > V2) {               \
            *l = 1;                        \
            *m = 2;                        \
            *s = 0;                        \
        } else {                           \
            *l = 2;                        \
            *m = 1;                        \
            *s = 0;                        \
        }                                  \
    }

template <class T>
//获得两元素数组里值的大小次序(数组, 返回最大值索引号, 返回最小值索引号)
void GN_Sequence2(T *v /*[2]*/, uint8 *l, uint8 *s) {
    if(v[0] > v[1]) {
        *l = 0;
        *s = 1;
    } else {
        *l = 1;
        *s = 0;
    }
}
template <class T>
//获得三元素数组里值的大小次序(数组, 返回最大值索引号, 返回中等值索引号, 返回最小值索引号)
void GN_Sequence3(T *v /*[3]*/, uint8 *l, uint8 *m, uint8 *s){
    GT_TMP_VALUE_SEQUENCE3(v[0], v[1], v[2])}
//交换两个变量的值(变量类型 , 变量1 , 变量2 )
#define GN_Swap(TYPE, em1, em2) \
    {                           \
        TYPE tmp_em = (em1);    \
        (em1) = (em2);          \
        (em2) = tmp_em;         \
    }
//顺时针转移一下三角形内容顺序
#define GN_ShiftTriItemCW(TYPE, A, B, C) \
    {                                    \
        TYPE tmpX = (A);                 \
        (A) = (C);                       \
        (C) = (B);                       \
        (B) = tmpX;                      \
    }
//逆时针转移一下三角形内容顺序
#define GN_ShiftTriItemCCW(TYPE, A, B, C) \
    {                                     \
        TYPE tmpX = (A);                  \
        (A) = (B);                        \
        (B) = (C);                        \
        (C) = tmpX;                       \
    }
/* -------------------------------------------------------- 随机数 -----------------------------------------------------------------*/
//初始化随机数分配器
#define GN_RandomInit() srand((unsigned int)time(0))

#if defined _WIN32_ || defined _WIN32_WCE
//产生0.0f至于参数范围以内的随机浮点数( 范围大 )
#define GN_Random(range) ((float)rand() / ((float)(0x7fff)) * (float)(range))
//产生2个指定参数范围内的随机整数(范围小, 范围大)
#define GN_Random_n(min_range, max_range) (int)((float)(min_range) + GN_Random(((float)(max_range) - (float)(min_range))))
#else
//产生0.0f至于参数范围以内的随机浮点数( 范围大 )
#define GN_Random(range) ((double)rand() / ((double)(0x7fffffff)) * (double)(range))
//产生2个指定参数范围内的随机整数(范围小, 范围大)
#define GN_Random_n(min_range, max_range) (int)((double)(min_range) + GN_Random(((double)(max_range) - (double)(min_range))))
#endif
//产生0.0f - 1.0f的固定随机浮点数
GN_API float GN_RandomF0_1();

#endif // _GN_BASE_H_
