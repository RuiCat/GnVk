// GN_mini 容器拆分片段：数组算法：置零/过滤/去重/排序/查找（模板与宏）。
// 2026-11-26 自 GN_Array.h 第 151-716 行 move-only 拆出（REFACTOR_PLAN §13 B3）。
// 由 GN_Array.h 按原顺序转发包含；单独包含需自行补齐前置依赖。
#ifndef GN_CONTAINERS_ARRAYALGORITHMS_H
#define GN_CONTAINERS_ARRAYALGORITHMS_H

// 自包含依赖（B3 拆分时补齐；正文仍为 move-only）
#include <cstring>
#include "baseTypes.h"

template <class T, class T2>
//设置数组里所有元素所有属性值均为0(数组 , 元素个数)
inline void GN_SetArray0(T *data, T2 num) {
    memset(data, 0, sizeof(T) * num);
}

//以数组里前段值来填满整个数组( 数组 , 数组总大小字节 , 前段值大小字节 )
inline void GN_SetArraySelf(uint8 *data, unsigned int tot_size, unsigned int value_size) {
    for(; (value_size << 1) <= tot_size; value_size <<= 1) memcpy(data + value_size, data, value_size);
    if(tot_size > value_size) memcpy(data + value_size, data, tot_size - value_size);
}
//交换两数组里的数据并保持原顺序不变( 数组1 , 数组2 , 交换个数 , 数据类型 )
#define GN_SwapArrayKeepOrder(data, data2, num, DATA_TYPE) \
    DATA_TYPE _tmp;                                        \
    unsigned int _di = 0, _dn = (unsigned int)(num);       \
    for(; _di < _dn; ++_di) {                              \
        _tmp = data2[_di];                                 \
        data2[_di] = data[_di];                            \
        data[_di] = _tmp;                                  \
    }

template <class T, class T2>
//抽取源数组里特定间距的元素数据来构建另一数组(另一数组 需先分配好足够空间, 源数组, 源数组结束位置, 抽取间距, 抽取元素放到另一数组里的复制副本数)
void GN_MakeArray(T *dst, T *src, T *end, T2 span, T2 cn) {
    switch(cn) {
        case 1:
            for(; src < end; src += span, dst += cn) dst[0] = *src;
            break;
        case 2:
            for(; src < end; src += span, dst += cn) {
                dst[0] = *src;
                dst[1] = *src;
            }
            break;
        case 3:
            for(; src < end; src += span, dst += cn) {
                dst[0] = *src;
                dst[1] = *src;
                dst[2] = *src;
            }
            break;
        case 4:
            for(; src < end; src += span, dst += cn) {
                dst[0] = *src;
                dst[1] = *src;
                dst[2] = *src;
                dst[3] = *src;
            }
            break;
        default: {
            T2 ci;
            for(; src < end; src += span, dst += cn) {
                for(ci = 0; ci < cn; ++ci) dst[ci] = *src;
            }
        } break;
    }
}
template <class T, class T2>
//移除数组里指定范围特定值的元素并保持原顺序不变( 数组, 值 , 范围起点索引, 范围结束索引) !如果有元素被移除了 参数end会变小
void GN_RemoveArrayElementKeepOrder(T *data, T em, T2 st, T2 &end) {
    if(st >= end) return;
    T2 e = end - 1;
    while(st < e && data[st] != em) ++st;
    if(st == e) {
        if(data[st] == em) end = st;
        return;
    }
    T2 i_ = st + 1;
    for(;;) {
        while(data[i_] == em) {
            if(i_ < e) ++i_;
            else {
                end = st;
                return;
            }
        }
        data[st] = data[i_];
        if(i_ < e) {
            ++i_;
            ++st;
        } else {
            end = st + 1;
            return;
        }
    }
}
template <class T, class T2>
//逆向移除数组里指定范围特定值的元素并保持原顺序不变( 数组, 值 , 范围起点索引, 范围结束索引) !如果有元素被移除了 参数st会变大
void GN_RemoveArrayElementKeepOrderReverse(T *data, T em, T2 &st, T2 end) {
    if(st >= end) return;
    T2 i = end - 1, s = st;
    while(i > s && data[i] != em) --i;
    if(i == s) {
        if(data[i] == em) st = i + 1;
        return;
    }
    T2 i_ = i - 1;
    for(;;) {
        while(data[i_] == em) {
            if(i_ > s) --i_;
            else {
                st = i + 1;
                return;
            }
        }
        data[i] = data[i_];
        if(i_ > s) {
            --i_;
            --i;
        } else {
            st = i;
            return;
        }
    }
}
//过滤保留数组里指定范围达到指定条件的所有元素并保持原顺序不变( 数组 , 比较前处理 , 条件 如 data[di]->x > 0.3f , 范围起点索引, 范围结束索引 ) !如果有元素被过滤掉 参数end会变小  !适合保留数比移除数小的处理
#define GN_FilterArrayElementIfKeepOrder(data, add_proc, cmp, st, end) \
    {                                                                  \
        unsigned int di = (st), di2 = (end);                           \
        if(di < di2) {                                                 \
            for(; di < di2; ++di) {                                    \
                add_proc;                                              \
                if(!(cmp)) break;                                      \
            }                                                          \
            if(di < di2) {                                             \
                unsigned int di1 = di;                                 \
                ++di;                                                  \
                for(; di < di2; ++di) {                                \
                    add_proc;                                          \
                    if(cmp) (data)[di1++] = (data)[di];                \
                }                                                      \
                (end) = di1;                                           \
            }                                                          \
        }                                                              \
    }
//逆向过滤保留数组里指定范围达到指定条件的所有元素并保持原顺序不变( 数组 , 比较前处理 , 条件 如 data[di]->x > 0.3f , 范围起点索引, 范围结束索引 ) !如果有元素被过滤掉 参数st会变大 !适合保留数比移除数小的处理
#define GN_FilterArrayElementReverseIfKeepOrder(data, add_proc, cmp, st, end) \
    {                                                                         \
        if((st) < (end)) {                                                    \
            unsigned int di2 = (st), di = (end) - 1;                          \
            for(; di > di2; --di) {                                           \
                add_proc;                                                     \
                if(!(cmp)) break;                                             \
            }                                                                 \
            unsigned int di1 = di;                                            \
            if(di > di2) {                                                    \
                --di;                                                         \
                for(; di > di2; --di) {                                       \
                    add_proc;                                                 \
                    if(cmp) (data)[di1--] = (data)[di];                       \
                }                                                             \
            }                                                                 \
            add_proc;                                                         \
            if(cmp) (data)[di1] = (data)[di]; /*最后一个*/                    \
            else ++di1;                                                       \
            (st) = di1;                                                       \
        }                                                                     \
    }
//快速移除数组里指定范围达到指定条件的所有元素( 数组 , 比较前处理 , 条件 如 data[di]->x > 0.3f , 范围起点索引, 范围结束索引 ) !如果有元素被移除了 参数end会变小 !适合移除数比保留数小的处理
#define GN_RemoveArrayElementIf(data, add_proc, cmp, st, end) \
    {                                                         \
        if((st) < (end)) {                                    \
            unsigned int di = (st), di2 = (end) - 1;          \
            for(; di < di2;) {                                \
                add_proc;                                     \
                if(cmp) (data)[di] = (data)[di2--];           \
                else ++di;                                    \
            }                                                 \
            add_proc;                                         \
            if(cmp) (end) = di; /*最后一个*/                  \
            else (end) = di + 1;                              \
        }                                                     \
    }
//快速逆向移除数组里指定范围达到指定条件的所有元素( 数组 , 比较前处理 , 条件 如 data[di]->x > 0.3f , 范围起点索引, 范围结束索引 ) !如果有元素被移除了 参数st会变大 !适合移除数比保留数小的处理
#define GN_RemoveArrayElementReverseIf(data, add_proc, cmp, st, end) \
    {                                                                \
        if((st) < (end)) {                                           \
            unsigned int di2 = (st), di = (end) - 1;                 \
            for(; di > di2;) {                                       \
                add_proc;                                            \
                if(cmp) (data)[di] = (data)[di2++];                  \
                else --di;                                           \
            }                                                        \
            add_proc;                                                \
            if(cmp) (st) = di + 1; /*最后一个*/                      \
            else (st) = di;                                          \
        }                                                            \
    }
template <class T, class T2>
//加入一个在数组里不相同的元素到尾部( 数组 , 元素个数 如成功加入该值会+1, 加入元素 )返回数组里与加入元素值相同的首个ID 如果成功加入返回加入后元素个数
T2 GN_ArrayAddNotEqual(T *data, T2 &num, T elm) {
    T2 di = 0;
    for(; di < num; ++di) {
        if(data[di] == elm) return di;
    }
    data[di] = elm;
    ++num;
    return num;
}
template <class T, class T2>
//合并数组2里不同于数组1里的元素到数组1的尾部( 数组1 , 数组1元素个数 如成功加入该值会增大, 数组2 , 数组2元素个数 )返回共加入了多少个元素 !数组一需先分配好足够多的空间
T2 GN_ArrayMergeNotEqual(T *data, T2 &num, T *data2, T2 num2) {
    T2 num1 = num;
    T2 i, i2 = 0;
    for(; i2 < num2; ++i2) {
        for(i = 0; i < num; ++i) {
            if(data2[i2] == data[i]) break;
        }
        if(i >= num) data[num++] = data2[i2];
    }
    return num - num1;
}
template <class T, class T2>
//合并数组2里对应分量比数组1大的值到数组1里(数组1, 数组2, 数组元素个数)
void GN_ArrayMagerMax(T *data, T *data2, T2 num) {
    T2 i = 0;
    for(; i < num; ++i) {
        if(data2[i] > data[i]) data[i] = data2[i];
    }
}
template <class T, class T2>
//合并数组2里对应分量比数组1小的值到数组1里(数组1, 数组2, 数组元素个数)
void GN_ArrayMagerMin(T *data, T *data2, T2 num) {
    T2 i = 0;
    for(; i < num; ++i) {
        if(data2[i] < data[i]) data[i] = data2[i];
    }
}
//去掉数组里值相同的元素使数据内每个数据都是不同的( 数组 ,  元素个数 ) !!如果有相同内容去掉后数组会自动缩紧,元素数将会减少
#define GN_UniqueArray(data, num)                                 \
    {                                                             \
        if(num) {                                                 \
            unsigned int di, dj, dn = (unsigned int)(num) - 1;    \
            for(di = 0; di < dn; ++di) {                          \
                for(dj = di + 1; dj <= dn; ++dj) {                \
                    while((data)[di] == (data)[dj] && dj <= dn) { \
                        (data)[dj] = (data)[dn];                  \
                        dn--;                                     \
                    }                                             \
                }                                                 \
            }                                                     \
            (num) = dn + 1;                                       \
        }                                                         \
    }
//交换反转数组里的元素次序(  数组 ,  元素个数 , 数据类型)
#define GN_SwapArray(data, num, DATA_TYPE)                   \
    {                                                        \
        DATA_TYPE tmp;                                       \
        unsigned int di1 = 0, di2 = (unsigned int)(num) - 1; \
        for(; di1 < di2; ++di1, --di2) {                     \
            tmp = (data)[di1];                               \
            (data)[di1] = (data)[di2];                       \
            (data)[di2] = tmp;                               \
        }                                                    \
    }

//交换三角型相关数据的绕序 即点a - c值交换( 三角点数据类型 , 三角型相关数据 , 三角型数 )
#define GN_SwapTriDataCCW(_DATA_TYPE, tri_data, tri_num) \
    {                                                    \
        _DATA_TYPE _v_;                                  \
        int _fi_ = 0, _fn_ = (int)(tri_num);             \
        for(; _fi_ < _fn_; ++_fi_) {                     \
            _v_ = (tri_data)[_fi_].a;                    \
            (tri_data)[_fi_].a = (tri_data)[_fi_].c;     \
            (tri_data)[_fi_].c = _v_;                    \
        }                                                \
    }
//用快速2分查找法搜索有序数组里最接近条件的一个元素索引( 比较条件 , 查找首元素, 查找末元素 , 元素索引结果输出)  !!注意用2分快速查找法的数组必须是按从小到大或从大到小地有序排列好, 数据从小到大排列比较条件符号必需为 (特定量 < 数据[ID_OUT]), 数据从大到小排列比较条件符号比需为 (特定量 > 数据[ID_OUT]) !!处理完之后参数START_ID和END_ID也将受到改变
#define GN_FindNearDataID(COMPARE, START_ID, END_ID, ID_OUT) \
    for(;;) {                                                \
        (ID_OUT) = ((END_ID) - (START_ID)) >> 1;             \
        if(ID_OUT) {                                         \
            (ID_OUT) += (START_ID);                          \
            if(COMPARE) (END_ID) = (ID_OUT);                 \
            else (START_ID) = (ID_OUT);                      \
        } else { /*已经不能再分了*/                          \
            (ID_OUT) = (END_ID);                             \
            if(COMPARE) (ID_OUT) = (START_ID);               \
            break;                                           \
        }                                                    \
    }

//用2分查找法搜索有序数组里最接近条件的临近两个元素索引( 比较条件 , 查找首元素, 查找末元素, 临时元素索引缓冲变量)  !!注意用2分快速查找法的数组必须是按从小到大或从大到小地有序排列好, 数据从小到大排列比较条件符号必需为 (特定量 < 数据[ID]), 数据从大到小排列比较条件符号比需为 (特定量 > 数据[ID]) !!处理完之后 临近两个元素索引也由参数START_ID和END_ID来返回
#define GN_FindNearDataID2(COMPARE, START_ID, END_ID, ID) \
    for(;;) {                                             \
        (ID) = ((END_ID) - (START_ID)) >> 1;              \
        if(ID) {                                          \
            (ID) += (START_ID);                           \
            if(COMPARE) (END_ID) = (ID);                  \
            else (START_ID) = (ID);                       \
        } else break; /*已经不能再分了*/                  \
    }

#if !defined(_ALGORITHM_) && !defined(_GLIBCXX_ALGORITHM) && !defined(_LIBCPP_ALGORITHM)
//数组排序(数据类型,数组指针,元数个数,比较方法,比较的成员) 这个比较合适简单数据的数组
#define GN_Sort(class_type, p_array, count, compare, member)             \
    {                                                                    \
        unsigned int i, j, n = (count) - 1;                              \
        bool exc;                                                        \
        class_type tmp;                                                  \
        for(i = 0; i < n; ++i) {                                         \
            exc = true;                                                  \
            for(j = n; j > i; --j) {                                     \
                if((p_array)[j] member compare(p_array)[j - 1] member) { \
                    tmp = (p_array)[j];                                  \
                    (p_array)[j] = (p_array)[j - 1];                     \
                    (p_array)[j - 1] = tmp;                              \
                    exc = false;                                         \
                }                                                        \
            }                                                            \
            if(exc) break;                                               \
        }                                                                \
    }
//数组排序复制(数据类型,储存排好数据的指针,要整理的数组指针,元数个数,比较方法,比较的指向成员)这个比较合适结构体数据的数组
#define GN_SortP(class_type, dp_array, p_array, count, compare, member) \
    {                                                                   \
        unsigned int i, j, n = (count) - 1;                             \
        bool exc;                                                       \
        class_type **pdata = 0;                                         \
        pdata = new class_type *[count];                                \
        for(i = 0; i <= n; ++i) pdata[i] = &(p_array)[i];               \
        class_type *tmp;                                                \
        for(i = 0; i < n; ++i) {                                        \
            exc = true;                                                 \
            for(j = n; j > i; --j) {                                    \
                if(pdata[j] member compare pdata[j - 1] member) {       \
                    tmp = pdata[j];                                     \
                    pdata[j] = pdata[j - 1];                            \
                    pdata[j - 1] = tmp;                                 \
                    exc = false;                                        \
                }                                                       \
            }                                                           \
            if(exc) break;                                              \
        }                                                               \
        for(i = 0; i <= n; ++i) (dp_array)[i] = *(pdata[i]);            \
        if(pdata) delete[] pdata;                                       \
    }
#else
//数组排序(数据类型,数组指针,元数个数,比较方法函数)
#define GN_Sort(class_type, p_array, count, comparefunc) std::sort<class_type *>((p_array), (p_array) + (count), comparefunc)
#endif
//采用二分数组排序(数据类型,数组指针,元数个数,比较方法,比较的成员) 这个比较合适简单数据的数组 !速度较快 但仅适合判断同一个值的线性大小排序, 和 std::sort 性质差不多
#define GN_Sort2(class_type, p_array, count, compare, emg)                                  \
    {                                                                                       \
        if((count) > 1) {                                                                   \
            unsigned int j, j_, n = (count) - 1;                                            \
            unsigned int oid = n, type_size = sizeof(class_type);                           \
            class_type tmp;                                                                 \
            j = n;                                                                          \
            j_ = j - 1;                                                                     \
            if(compare(emg((p_array)[j]), emg((p_array)[j_]))) {                            \
                GN_Swap(class_type, (p_array)[j], (p_array)[j_]);                           \
            }                                                                               \
            for(--j; j > 0; --j) {                                                          \
                j_ = j - 1;                                                                 \
                if(compare(emg((p_array)[j]), emg((p_array)[j_]))) {                        \
                    tmp = (p_array)[j_];                                                    \
                    unsigned int sid = j, eid = n;                                          \
                    GN_FindNearDataID(compare(emg tmp, emg((p_array)[oid])), sid, eid, oid) \
                        memmove((p_array) + j_, (p_array) + j, (oid - j + 1) * type_size);  \
                    (p_array)[oid] = tmp;                                                   \
                }                                                                           \
            }                                                                               \
        }                                                                                   \
    }
template <class T>
//默认排序比较方法 由小到大( 比较的左值 , 比较的右值 )返回是否符合条件 符合条件的值排在前
inline bool GN_SortFuncDef(T left, T right) {
    return left < right; //!您可以复制这个模板函数 修改这里自定义自己的排序方式
}
template <class T>
//    排序比较方法 由大到小( 比较的左值 , 比较的右值 )返回是否符合条件 符合条件的值排在前
inline bool GN_SortFuncBigToSmall(T left, T right) {
    return left > right;
}
//    排序比较方法 由小到大
#define GN_SortFuncSmallToBig GN_SortFuncDef

/*-----------------------------------------------------------------------------------------------------------------------*/
template <class T, class T2, class T3, class F>
//按指定方法分类排列数据为达成条件|和未达成条件的两边(数据, 数据个数, 比较方法参考参数, 比较方法) 返回达成条件的数据个数
T2 GN_ArrayClassify(T *p_array, T2 count, T3 user_arg, F cmp_func) {
    if(count) {
        T tmp;
        T2 st = 0, end = count - 1;
        for(; st < end;) {
            if(cmp_func(p_array[st], user_arg)) ++st;
            else {
                tmp = p_array[st];
                p_array[st] = p_array[end];
                p_array[end] = tmp;
                --end;
            }
        }
        if(cmp_func(p_array[st], user_arg)) ++st;
        return st;
    } else return 0;
}
/*////////////////////////////////////////////////////////////////
比较方法函数参考(要分类数据里的其中一个元素, 传回给您的用户参数)
inline int Compare(T em, T3 user_arg){
    return em.member < user_arg.member;
}
*/
////////////////////////////////////////////////////////////////

template <class T, class T2>
//获得两数组值的总差距(数组1, 数组2, 比较数组个数) 返回总差距量
T GN_ArrayTotalGapGet(T *data, T *data2, T2 dn) {
    T2 di = 0, n = dn;
    T gt = 0;
    for(; di < n; ++di) gt += (data)[di] > (data2)[di] ? (data)[di] - (data2)[di] : (data2)[di] - (data)[di];
    return gt;
}

template <class T, class T2>
//获得两数组里不同值的平均差距(数组1, 数组2, 比较数组个数) 返回差距量
T GN_ArrayAverageGapGet(T *data, T *data2, T2 dn) {
    T2 di = 0, n = dn, gn = 0;
    T g, gt = 0;
    for(; di < n; ++di) {
        g = (data)[di] > (data2)[di] ? (data)[di] - (data2)[di] : (data2)[di] - (data)[di];
        if(g) {
            gt += g;
            ++gn;
        }
    }
    if(gn) return gt / (T)gn;
    else return 0;
}
//使浮点类型数组里每个元素归一化 即>0为1 <0为-1 0不变(数组, 数据个数)
inline void GN_ArrayNormalizeFloat(float *p_array, int count) {
    int i = 0;
    for(; i < count; ++i) {
        if(p_array[i] > 0.0f) p_array[i] = 1.0f;
        else if(p_array[i] < 0.0f) p_array[i] = -1.0f;
    }
}
//整片内存数据乘比率值(逻辑数据类型,内存片地址,内存片大小,相乘的比值)
#define GN_MemRatio(class_type, p_memory, uint_len, ratio)                       \
    {                                                                            \
        class_type *pTarget;                                                     \
        uint8 *pmm = (uint8 *)(p_memory);                                        \
        unsigned int TargetSize = sizeof(class_type), leni = 0, ulen = uint_len; \
        for(; leni < ulen; leni += TargetSize) {                                 \
            pTarget = (class_type *)(&pmm[leni]);                                \
            *pTarget = *pTarget * (ratio);                                       \
        }                                                                        \
    }
//比较两内存片里的数值是否相同(内存地址1, 内存地址2, 大小字节 )
GN_API inline bool GN_MemCompare(void *p_memory1, void *p_memory2, unsigned int size) {
    unsigned char *pm1 = (unsigned char *)p_memory1, *pm2 = (unsigned char *)p_memory2;
    unsigned char *pme = pm1 + size;

    while(pm1 < pme) {
        if(*pm1 != *pm2) return false;
        pm1++;
        pm2++;
    }
    return true;
}
template <class T, class T2>
//比较两个数组里的数值是否近似(数组指针1, 数组指针2, 元数个数 , 容许误差值:不能为负数)
bool GN_MemCompareSimilar(T *fd1, T *fd2, T2 count, T v) {
    T *fe = fd1 + count;
    for(; fd1 < fe; ++fd1, ++fd2) {
        if((*fd1 > *fd2 ? *fd1 - *fd2 : *fd2 - *fd1) > v) return false;
    }
    return true;
}
//比较两个浮点数组里的数值是否近似(数组指针1, 数组指针2, 元数个数 , 容许误差值:不能为负数)
#define GN_MemCompareFloat GN_MemCompareSimilar

template <class T, class T2>
//在数组里查找与指定值相近的值位置指针(数组指针 , 元数个数 , 值 , 容许误差值:不能为负数)返回位置指针 返回0表示数组里无相近的值
T *GN_ArrayGetSimilarElementId(T *fd, T2 count, T f, T v) {
    T *fe = fd + count;
    for(; fd < fe; ++fd) {
        if((*fd > f ? *fd - f : f - *fd) < v) return fd;
    }
    return 0;
}
/********************************************************* 使数组整理为数组与索引 *************************************************************/

template <class T, class T2, class T3>
//按现有数据来创建它对应的索引和数据(整里好不相同的数据输出, 整理好的对应索引输出, 要整理数据输入, 数据数)返回整理后不相同的数据输出有多少个 !参数 data_out 和 index_out 需预先分配好空间 元素个数不能少于 data_num
T3 GN_IndexAndDataCreate(T *data_out, T2 *index_out, const T *data_in, T3 data_num) {
    //数据生成索引
#define GT_TMP_DATA_GEN_INDEXS(CMP)    \
    T3 i, i2, n2 = 0;                  \
    for(i = 0; i < data_num; ++i) {    \
        for(i2 = 0; i2 < n2; ++i2) {   \
            if(CMP) {                  \
                index_out[i] = (T2)i2; \
                break;                 \
            }                          \
        }                              \
        if(i2 >= n2) {                 \
            data_out[n2] = data_in[i]; \
            index_out[i] = (T2)n2;     \
            ++n2;                      \
        }                              \
    }                                  \
    return n2;

    GT_TMP_DATA_GEN_INDEXS(data_in[i] == data_out[i2])
}

template <class T, class T2, class T3>
//按现有数据来创建它对应的索引和数据-小数类型数据(整里好不相同的数据输出, 整理好的对应索引输出, 要整理数据输入, 数据数)返回整理后不相同的数据输出有多少个 !参数 data_out 和 index_out 需预先分配好空间 元素个数不能少于 data_num
T3 GN_IndexAndDataCreateFloat(T *data_out, T2 *index_out, const T *data_in, T3 data_num) {
    unsigned int t_size = sizeof(T);
    GT_TMP_DATA_GEN_INDEXS(GN_MemCompare((void *)(&data_in[i]), (void *)(&data_out[i2]), t_size))
}

template <class T, class T2, class T3>
//集中紧揍整理好数据和对应的数据索引 使数据不存在相同的值(整里好不相同的数据输出, 整理好的对应索引输出, 要整理数据输入, 数据数, 要整理索引数据输入, 索引数)返回整理后不相同的数据输出有多少个 !参数 data_out 和 index_out 需预先分配好空间 元素个数分别不能少于 data_num 和 index_num
T3 GN_IndexAndDataConcentrate(T *data_out, T2 *index_out, const T *data_in, T3 data_num, const T2 *index_in, T3 index_num) {
#define GT_TMP_INDEX_AND_DATA_CONCENTRATE(PROC)                         \
    T2 *em_refs = new T2[data_num];                                     \
    T3 i = 0, n = PROC(data_out, em_refs, data_in, data_num);           \
    for(i = 0; i < index_num; ++i) index_out[i] = em_refs[index_in[i]]; \
    delete[] em_refs;                                                   \
    return n;

    GT_TMP_INDEX_AND_DATA_CONCENTRATE(GN_IndexAndDataCreate)
}

template <class T, class T2, class T3>
//集中紧揍整理好数据和对应的数据索引 使数据不存在相同的值-小数类型数据(整里好不相同的数据输出, 整理好的对应索引输出, 要整理数据输入, 数据数, 要整理索引数据输入, 索引数)返回整理后不相同的数据输出有多少个 !参数 data_out 和 index_out 需预先分配好空间 元素个数分别不能少于 data_num 和 index_num
T3 GN_IndexAndDataConcentrateFloat(T *data_out, T2 *index_out, const T *data_in, T3 data_num, const T2 *index_in, T3 index_num) {
    GT_TMP_INDEX_AND_DATA_CONCENTRATE(GN_IndexAndDataCreateFloat)
}

template <class T, class T2, class T3>
//整理移去特定段落未被引用的数据(数据首地址, 数据整理段落开始位置, 段落结束位置, 索引数据, 索引数)返回整理后数据段落新结束位置 !!适合用于简单数据类型
T3 GN_IndexAndDataRemoveNoUsed(T *data, T3 st, T3 end, T2 *index_s, T3 index_num) {
#define GT_TMP_INDEX_AND_DATA_REMOVE_NO_USED(COPY_PROC)                \
    T2 idx_buf[65536];                                                 \
    T2 *idxs = end > 65536 ? new T2[end] : idx_buf;                    \
    memset(idxs + st, 0xff, sizeof(T2) * (end - st));                  \
    T3 ii;                                                             \
    for(ii = 0; ii < index_num; ++ii) idxs[index_s[ii]] = index_s[ii]; \
    T3 st2 = end - 1;                                                  \
    for(; st < st2;) {                                                 \
        if(idxs[st] == 0xffff) { /*没有引用到的索引*/                  \
            if(idxs[st2] != 0xffff) {                                  \
                COPY_PROC;                                             \
                idxs[st2] = st;                                        \
            }                                                          \
            idxs[st] = idxs[st2--];                                    \
        } else ++st;                                                   \
    }                                                                  \
    if(idxs[st] != 0xffff) ++st;                                       \
    for(ii = 0; ii < index_num; ++ii) index_s[ii] = idxs[index_s[ii]]; \
    if(idxs != idx_buf) delete[] idxs;                                 \
    return st;

    GT_TMP_INDEX_AND_DATA_REMOVE_NO_USED(data[st] = data[st2])
}

template <class T, class T2, class T3>
//整理移去特定段落未被引用的数据(数据移动复制处理回调(传回给您的数据首地址, 当前要复制到的目标数据位置, 当前复制数据源位置), 数据首地址, 数据整理段落开始位置, 段落结束位置, 索引数据, 索引数)返回整理后数据段落新结束位置 !!适合用于复杂数据类型 或类对象
T3 GN_IndexAndDataRemoveNoUsed(void (*on_data_cpy_proc)(T *data, T3 dst_i, T3 src_i), T *data, T3 st, T3 end, T2 *index_s, T3 index_num) {
    GT_TMP_INDEX_AND_DATA_REMOVE_NO_USED(on_data_cpy_proc(data, st, st2))
}

#endif // GN_CONTAINERS_ARRAYALGORITHMS_H
