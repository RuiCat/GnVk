// GN_mini 容器拆分片段：普通指针内存操作宏（GN_Delete/GN_Release/数组复制等）。
// 2026-11-26 自 GN_Array.h 第 3-150 行 move-only 拆出（REFACTOR_PLAN §13 B3）。
// 由 GN_Array.h 按原顺序转发包含；单独包含需自行补齐前置依赖。
#ifndef GN_CONTAINERS_MEMMACROS_H
#define GN_CONTAINERS_MEMMACROS_H

/************************************************普通指针内存操作**************************************************************************/
//安全释放指针
#define GN_MemFree(p) \
    {                 \
        if(p) {       \
            free(p);  \
            (p) = 0;  \
        }             \
    }
//安全删除指针
#define GN_Delete(p)   \
    {                  \
        if(p) {        \
            delete(p); \
            (p) = 0;   \
        }              \
    }
//安全删除指针数组
#define GN_DeleteArray(p) \
    {                     \
        if(p) {           \
            delete[](p);  \
            (p) = 0;      \
        }                 \
    }

//安全释放指针表所指的内存(指针表数组地址,指针表指针的个数)
#define GN_FreePPMem(pp, n)                              \
    {                                                    \
        if(pp) {                                         \
            unsigned int di = 0, dn = (unsigned int)(n); \
            for(; di < dn; ++di) {                       \
                if((pp)[di]) free((pp)[di]);             \
            }                                            \
            free(pp);                                    \
            (pp) = 0;                                    \
        }                                                \
    }
//安全删除指针表所指的数据(指针表数组地址,指针表指针的个数)
#define GN_DeletePPData(pp, n)                           \
    {                                                    \
        if(pp) {                                         \
            unsigned int di = 0, dn = (unsigned int)(n); \
            for(; di < dn; ++di) {                       \
                if((pp)[di]) delete((pp)[di]);           \
            }                                            \
            delete[](pp);                                \
            (pp) = 0;                                    \
        }                                                \
    }
//安全删除指针表所指的数组数据(指针表数组地址,指针表指针的个数)
#define GN_DeletePPArray(pp, n)                          \
    {                                                    \
        if(pp) {                                         \
            unsigned int di = 0, dn = (unsigned int)(n); \
            for(; di < dn; ++di) {                       \
                if((pp)[di]) delete[]((pp)[di]);         \
            }                                            \
            delete[](pp);                                \
            (pp) = 0;                                    \
        }                                                \
    }
//安全删除安全指针表所指的数据(安全指针表)
#define GN_DelPPData(pp)                          \
    {                                             \
        if(pp) {                                  \
            unsigned int di = 0, dn = (pp).Num(); \
            for(; di < dn; ++di) {                \
                if((pp)[di]) delete((pp)[di]);    \
            }                                     \
            (pp).Delete();                        \
        }                                         \
    }
//安全释放指针资源
#define GN_Release(p)       \
    {                       \
        if(p) {             \
            (p)->Release(); \
            (p) = 0;        \
        }                   \
    }

//清除连表指针内容(指针数据类型 , 连表)
#define GN_DeleteListP(p_class_type, p_list)                                            \
    if(!(p_list).empty()) {                                                             \
        GN::sLIST<p_class_type *>::iterator it = (p_list).begin(), et = (p_list).end(); \
        for(; it != et; ++it) delete(*it);                                              \
        (p_list).clear();                                                               \
    }
//清除连表指针数组内容(指针数据类型 , 连表)
#define GN_DeleteListPArray(p_class_type, p_list)                                       \
    if(!(p_list).empty()) {                                                             \
        GN::sLIST<p_class_type *>::iterator it = (p_list).begin(), et = (p_list).end(); \
        for(; it != et; ++it) delete[](*it);                                            \
        (p_list).clear();                                                               \
    }
//安全复制指针数据一个( 接收目标指针-未分配的 , 复制源指针  , 数据类型)
#define GN_CopyOnce(desc, src, DATA_TYPE) \
    if(desc) delete(desc);                \
    if(src) {                             \
        (desc) = new DATA_TYPE;           \
        *(desc) = *(src);                 \
    } else (desc) = 0

//安全复制指针数组( 接收目标指针-未分配的 , 复制源指针 , 数据数 , 数据类型)
#define GN_CopyArray(desc, src, num, DATA_TYPE)        \
    if(desc) delete[](desc);                           \
    if(src) {                                          \
        (desc) = new DATA_TYPE[num];                   \
        unsigned int ci = 0, ce = (unsigned int)(num); \
        for(; ci < ce; ci++) (desc)[ci] = (src)[ci];   \
    } else (desc) = 0

//重分配数组大小(数组, 元素个数, 新元素个数, 数据类型)
#define GN_ReSizeArray(data, num, num_, DATA_TYPE)                               \
    if(data) {                                                                   \
        if(num_) {                                                               \
            DATA_TYPE *data_1 = data;                                            \
            data = new DATA_TYPE[num_];                                          \
            memcpy(data, data_1, sizeof(DATA_TYPE) * (num_ < num ? num_ : num)); \
            delete[] data_1;                                                     \
        } else {                                                                 \
            delete[] data;                                                       \
            data = 0;                                                            \
        }                                                                        \
    }                                                                            \
    num = num_

//保留数组前指定个数的数据 裁掉后面的(数组 , 保留个数 , 数据类型 )
#define GN_KeepArray(data, num, DATA_TYPE)             \
    {                                                  \
        DATA_TYPE *data_1 = data;                      \
        data = new DATA_TYPE[num];                     \
        memcpy(data, data_1, sizeof(DATA_TYPE) * num); \
        delete[] data_1;                               \
    }
//设置数组里所有元素为指定值( 数组 , 元素个数 , 值 )
#define GN_SetArray(data, num, value)                    \
    {                                                    \
        unsigned int _di = 0, _dn = (unsigned int)(num); \
        for(; _di < _dn; ++_di) (data)[_di] = (value);   \
    }
//设置数组里所有元素与源数组里相等( 数组 , 源数组 , 数组元素个数 )
#define GN_SetArrayFromArray(dst, src, num)              \
    {                                                    \
        unsigned int _di = 0, _dn = (unsigned int)(num); \
        for(; _di < _dn; ++_di) (dst)[_di] = (src)[_di]; \
    }

#endif // GN_CONTAINERS_MEMMACROS_H
