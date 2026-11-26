// GN_mini 容器拆分片段：安全指针 P_/P（引用计数、防重复删除）。
// 2026-11-26 自 GN_Array.h 第 3065-3671 行 move-only 拆出（REFACTOR_PLAN §13 B3）。
// 由 GN_Array.h 按原顺序转发包含；单独包含需自行补齐前置依赖。
#ifndef GN_CONTAINERS_SMARTPTR_H
#define GN_CONTAINERS_SMARTPTR_H

// 自包含依赖（B3 拆分时补齐；正文仍为 move-only）
#include "baseTypes.h"
#include "memMacros.h"
#include "list.h"
#include <new>
#include <cstring>
#include "_SysDef/GN_SysDef.h"
#include "GN_Base.h"

/*********************************************** 安全指针 **************************************************************************/
//#define GT_CUSTODY_PTR
#ifdef GT_CUSTODY_PTR
#define GT_P_ACTUAL(DNUM) (DNUM == 0xffffffff ? 1 : DNUM)
#else
#define GT_P_ACTUAL(DNUM) DNUM
#endif
#ifdef GN_DEBUG
#define GT_PMEM_WORNING
#endif
#ifndef GT_PMEM_WORNING
#define P_SetName(pointer, name)
#define P_PrintName()
#define P_PrintUsRange()
#else
//设置安全指针的名称(安全指针, 名称)以方便跟踪识别
#define P_SetName(pointer, name) \
    strcpy(pointer.zName, name); \
    if(pointer.zData) GN_Log("@@: %08x - %08x %s\n", pointer.zData->pmem, (pointer.zData->pmem) + pointer.Size(), zName);

//打印安全指针的名称
#define P_PrintName() \
    if(zName[0]) GN_Log("@@: %08x - %08x %s\n", 0x00000000, 0x00000000, zName);

//打印安全指针的内存范围
#define P_PrintUsRange() \
    if(zName[0]) GN_Log("@@: %08x - %08x %s\n", zData->pmem, ((T *)zData->pmem) + zData->num, zName);
#endif

//强制转换安全指针( 目标类型 )
#define CVP(CType) *(P<CType> *)&

//强制转换表示类型 而不转换它在内存的值( 目标类型 )
#define CV(CType) *(CType *)&

struct GT_sMEMORY { //内存块结构
    void *pmem; //内存地址
    volatile unsigned int us; //使用者数
    unsigned int num; //数据数

    inline bool operator==(const GT_sMEMORY &mem) const { return this->pmem == mem.pmem; }
    inline bool operator<(const GT_sMEMORY &mem) const { return this->pmem < mem.pmem; }
};
struct GN_API GT_sMEMORY_TABLE { //内存表结构
    GN::sLIST<GT_sMEMORY> memList; //内存使用表 记下那些内存正在被使用
    GT_sATOMIC isBusy; //内存使用表是否被占用

    GT_sMEMORY_TABLE();
    ~GT_sMEMORY_TABLE();

    volatile GT_sMEMORY *Add(GT_sMEMORY &em); //记录一个内存块(内存块) 返回内存块在这里的位置
    void Del(volatile GT_sMEMORY *mem); //移除一个内存块记录(内存块位置)
};
extern GN_API GT_sMEMORY_TABLE GT_UsMemTable; //定义一个全局内存表

//inline bool CompareRemoveifMemory(GT_sMEMORY &mem){ return (mem.us == 0); } //释放内存的比较条件(当前检索到的内存块)返回是否可移除

extern GN_API bool P_CopyModeAll; //用于设置所有类型的安全指针遇到 " = " 和 " P<T> pointer(P) " 时是否为复制副本数据模式

struct P_ //★所有安全指针基类 支持线程安全 一般用作不同类型安全指针的便捷传值 !注意它不能拥有和操作数据
{
    volatile GT_sMEMORY *zData; //指向内存使用表的元素位置

#ifdef GT_PMEM_WORNING
    char zName[64];

    inline P_() : zData(0) { zName[0] = 0; }
#else
    inline P_() : zData(0) {}
#endif
    inline ~P_() { /*zData = 0;*/ }
    inline void Delete() { zData = 0; }

    //比较是否指向相同数据内存
    inline bool operator==(const P_ &p_) const { return (this->zData == p_.zData); }
    //比较是否指向不同数据内存
    inline bool operator!=(const P_ &p_) const { return (this->zData != p_.zData); }

    //返回指针是否含有数据
    inline bool Has() { return zData ? true : false; }

    //逆向从普通指针找回它的安全指针( 普通指针 )  !这需要一些处理时间 这普通指针必需是用安全方式创建来的 被安全指针包含 否侧该调用被忽略
    void Re(void *p);
};
/*------------------------------------------- 安全指针原理 ----------------------------------------------------*\

	安全指针主要用引用计数来管理内存片的生命存活周期 刚分配空间时使用者数为1 把它传给其它安全指针使用者数加1
	当删除安全指针时 仅在被使用者数为1的才会切底删除数据空间 否则记录使用者数减1 这样就可确保数据安全有效

	一般在对象里或局部函数范围内的安全指针都无需手动删除释放 引擎会在它没用时自动为您清除那些内存空间
	只有静态类型的或在全局区域的安全指针 如您不想一些数据内存被长期闲置占用空间即可手动清除一下

\*-------------------------------------------------------------------------------------------------------------*/
template <class T>
struct P : public P_ //★★★安全指针  支持线程安全 预防内存涉漏 防重复删除造成崩溃 完美解决多对象同时引用同一指针删除问题 和提供更多数据操作方法
{
    static bool copyMode; //用于设置该类型指针遇到 " = " 和 " P<T> pointer(P) " 时是否为复制副本数据模式

    inline P() {}
    inline ~P() { this->Delete(); }

    //删除数据 ------------------------------------------------------------------------------------------------
    inline void Delete() {
        if(zData) {
            if(zData->us > 1) { zData->us--; } //如果这快内存被多个指针共享的话就记下它的共享数少1
            else if(zData->us == 1) { //仅有一个指针共享它时就切底删除它
#ifdef GT_CUSTODY_PTR //删除托管的普通指针数据
                if(zData->num == 0xffffffff) delete(T *)zData->pmem;
                else
#endif
                    delete[](T *)zData->pmem;
                GT_UsMemTable.Del(zData);
            }
            zData = 0;
            P_PrintName();
        }
    }
    //分配数据(多少个元素) ------------------------------------------------------------------------------------
    inline void New(unsigned int count) {
        this->Delete();
#ifdef GT_PMEM_WORNING
        if(!count) GN_Log("P %s 分配数据空间为: 0 !!\n", zName[0] ? zName : "");
#endif
        GT_sMEMORY em;
        em.us = 1;
        em.num = count;
        em.pmem = new T[count]; //分配一个内存块
        zData = GT_UsMemTable.Add(em);
        P_PrintUsRange();
    }
    inline P(unsigned int count) { this->New(count); }

    inline void operator()(unsigned int count) { this->New(count); }

    //分配数据(多少个元素, 全部的默认初始值) ------------------------------------------------------------------
    inline void New(unsigned int count, T em_all) {
        this->New(count);
        T *it = (T *)(zData->pmem), *et = ((T *)zData->pmem) + zData->num;
        for(; it < et; ++it) *it = em_all;
    }
    inline P(unsigned int count, T em_all) { this->New(count, em_all); }

    inline void operator()(unsigned int count, T em_all) { this->New(count, em_all); }

    //从复制普通指针数据来初始分配数据( 普通指针, 数据个数) 如数据个数为0就当它是字符串来处理 -----------------
    inline void New(const T *p_data, unsigned int count) {
#ifdef GT_PMEM_WORNING
        if(!p_data) GN_Log("P %s 分配空间时复制数据源为无效的?!!\n", zName[0] ? zName : "");
#endif
        if(p_data) {
            if(count) {
                this->New(count);
                T *it = (T *)(zData->pmem), *et = ((T *)zData->pmem) + count, *id = (T *)(p_data);
                for(; it < et; ++it, ++id) *it = *id;
            } else if(sizeof(T) == sizeof(char)) { //是字符类型时
                this->New((unsigned int)strlen((char *)p_data) + 1);
                strcpy((char *)(this->zData->pmem), (char *)p_data);
                return;
            }
            return;
        }
        this->Delete();
    }
    inline P(const T *p_data, unsigned int count) { this->New(p_data, count); }

    inline void operator()(const T *p_data, unsigned int count) { this->New(p_data, count); }

    //从复制连表里的数据来初始化分配数据( 连表 ) --------------------------------------------------------------
    inline void New(GN::sLIST<T> &list) {
        if(list.empty()) {
#ifdef GT_PMEM_WORNING
            GN_Log("P %s 分配空间时复制链表源为空的?!!\n", zName[0] ? zName : "");
#endif
            this->Delete();
            return;
        }
        this->New((unsigned int)list.size());
        GT_LIST_iterator(T) it_t = list.begin(), et_t = list.end();
        T *it = (T *)(zData->pmem);
        for(; it_t != et_t; ++it_t, ++it) *it = (*it_t);
    }
    inline P(GN::sLIST<T> &list) { this->New(list); }

    inline void operator()(GN::sLIST<T> &list) { this->New(list); }
#ifdef GT_CUSTODY_PTR
    //一般在分配数据并一起构造对象时用这个函数比较有效率一点 使用示范: P<int> pointer(new int(123));  new出来的不能是数组 如 new int[3]
    //托管新分配的普通指针来初始化(新 new 出来的数据指针) 这不会另行分配空间来复制数据  托管后new出来的指针不必释放调用C++的 delete 了
    inline void New(const T *new_p) {
#ifdef GT_PMEM_WORNING
        if(!new_p) GN_Log("P %s 初始化时托管的指针为无效的?!!\n", zName[0] ? zName : "");
#endif
        this->Delete();
        GT_sMEMORY em;
        em.us = 1;
        em.num = 0xffffffff; //这个标记它是用单独的一个数据分配的
        em.pmem = (void *)new_p;
        zData = GT_UsMemTable.Add(em);
        P_PrintUsRange();
    }
    inline P(const T *new_p) { this->New(new_p); }

    inline void operator()(const T *new_p) { this->New(new_p); }
#endif
    //赋值复制副本 --------------------------------------------------------------------------------------------
    inline P(const P &pm) {
        if(copyMode || P_CopyModeAll) {
            if(pm.zData) {
                unsigned int data_num = GT_P_ACTUAL(pm.zData->num);
                this->New(data_num); //另外创建一份新内存
                unsigned int ci = 0;
                for(; ci < data_num; ++ci) ((T *)(zData->pmem))[ci] = ((T *)(pm.zData->pmem))[ci];
            } else this->zData = 0;
        } else {
            if(pm.zData) {
                this->zData = pm.zData;
                ++(this->zData->us);
            } else this->zData = 0;
        }
    }
    //赋值复制副本
    inline P(const P_ &p_) { *this = *(P *)&p_; }

    //等号复制副本
    inline P &operator=(const P &pm) {
        if(copyMode || P_CopyModeAll) {
            if(pm.zData) {
                unsigned int data_num = GT_P_ACTUAL(pm.zData->num);
                this->New(data_num); //另外创建一份新内存
                unsigned int ci = 0;
                for(; ci < data_num; ++ci) ((T *)(zData->pmem))[ci] = ((T *)(pm.zData->pmem))[ci];
            } else this->Delete();
        } else {
            this->Delete();
            if(pm.zData) {
                this->zData = pm.zData;
                ++(this->zData->us);
                P_PrintUsRange();
            }
        }
        return *this;
    }
    //等号复制副本
    inline P &operator=(const P_ &p_) {
        *this = *(P *)&p_;
        return (*this);
    }

    //从其它安全指针复制一份相同的数据( 其它安全指针 )
    inline void Copy(const P &pm) {
        if(pm.zData) {
            unsigned int data_num = GT_P_ACTUAL(pm.zData->num);
            this->New(data_num); //另外创建一份新内存
            unsigned int ci = 0;
            for(; ci < data_num; ++ci) ((T *)(zData->pmem))[ci] = ((T *)(pm.zData->pmem))[ci];
        } else this->Delete();
    }
    //复制出内部数据到新的安全指针( 增补元素个数, 新增的全部元数的默认值 ) 返回装有复制数据的新安全指针 首个参数为负数可以裁短数据
    inline P CopyOut(int add_count, T em_all) {
        P pd;
        unsigned int data_num = GT_P_ACTUAL(zData->num);
        int ci, new_num = (int)data_num + add_count;
        if(new_num > 0) {
            pd.New(new_num);
            if(new_num >= data_num) {
                for(ci = 0; ci < data_num; ++ci) ((T *)(pd.zData->pmem))[ci] = ((T *)(zData->pmem))[ci];
                for(; ci < new_num; ++ci) ((T *)(pd.zData->pmem))[ci] = em_all;
            } else {
                for(ci = 0; ci < new_num; ++ci) ((T *)(pd.zData->pmem))[ci] = ((T *)(zData->pmem))[ci];
            }
        }
        return pd;
    }
    //与指定安全指针交换指向的数据内容(其它安全指针) !!如果有其它安全指针与两者共享同一片数据,这修改将会影响其它共享的安全指针
    inline void Swap(const P &pm) {
        GN_Swap(void *, zData->pmem, pm.zData->pmem);
        GN_Swap(unsigned int, zData->num, pm.zData->num);
    }
    //替换内部一组元素值(开始替换的元素索引号, 共替换几个元素, 这些元素的替换值)
    inline void Replace(unsigned int id, unsigned int count, T em_all) {
#ifdef GT_PMEM_WORNING
        if(!count) {
            GN_Log("P %s Replace count = 0? !!\n", zName[0] ? zName : "");
            return;
        }
#endif
        unsigned int data_num = GT_P_ACTUAL(zData->num);
        if(id >= data_num) return; //判断处理可能用户乱填的参数范围
        count += id;
        if(count > data_num) count = data_num;
        T *it = (T *)(zData->pmem);
        for(; id < count; ++id) it[id] = em_all;
    }
    //在内部从新分配指定大小的空间( 多少个元素 ) !!如果有其它安全指针和它共享同一片数据 这修改将会影响其它共享的安全指针
    inline void ReSize(unsigned int new_num) {
#ifdef GT_PMEM_WORNING
        if(!new_num) {
            GN_Log("P %s ReSize new_num = 0? !!\n", zName[0] ? zName : "");
            return;
        }
#endif
        unsigned int data_num = GT_P_ACTUAL(zData->num);
        if(new_num == data_num) return;
        unsigned int ci, min = data_num < new_num ? data_num : new_num;
        T *new_data = new T[new_num];
        for(ci = 0; ci < min; ++ci) new_data[ci] = ((T *)(zData->pmem))[ci];
#ifdef GT_CUSTODY_PTR
#define GT_P_MEM_RESET                                   \
    if(zData->num == 0xffffffff) delete(T *)zData->pmem; \
    else delete[](T *)zData->pmem;                       \
    zData->pmem = new_data;                              \
    zData->num = new_num
#else
#define GT_P_MEM_RESET        \
    delete[](T *)zData->pmem; \
    zData->pmem = new_data;   \
    zData->num = new_num
#endif
        GT_P_MEM_RESET;
        P_PrintUsRange();
    }
    //在内部从新分配足够空间插入元素( 增补元素个数, 新增的全部元数的默认值) !!如果有其它安全指针和它共享同一片数据 这修改将会影响其它共享的安全指针
    inline void Insert(unsigned int add_count, T em_all) {
#ifdef GT_PMEM_WORNING
        if(!add_count) {
            GN_Log("P %s Insert add_count = 0? !!!\n", zName[0] ? zName : "");
            return;
        }
#endif
        unsigned int data_num = GT_P_ACTUAL(zData->num);
        unsigned int ci, new_num = data_num + add_count;
        T *new_data = new T[new_num];
        for(ci = 0; ci < data_num; ++ci) new_data[ci] = ((T *)(zData->pmem))[ci];
        for(; ci < new_num; ++ci) new_data[ci] = em_all;
        GT_P_MEM_RESET;
        P_PrintUsRange();
    }
    //在内部从新分配足够空间插入另一个同类型安全指针里的所有元素( 另一个安全指针 )
    inline void Insert(const P &pm) {
        if(!pm.zData) {
#ifdef GT_PMEM_WORNING
            GN_Log("P %s Insert 安全指针pm为空的!\n", zName[0] ? zName : "");
#endif
            return;
        }
        unsigned int data_num = GT_P_ACTUAL(zData->num), add_count = GT_P_ACTUAL(pm.zData->num);
        if(!add_count) return;
        unsigned int ci, ci2, new_num = data_num + add_count;
        T *new_data = new T[new_num], *pdata1 = (T *)(zData->pmem), *pdata2 = (T *)(pm.zData->pmem);
        for(ci = 0; ci < data_num; ++ci) new_data[ci] = pdata1[ci];
        for(ci2 = 0; ci < new_num; ++ci, ++ci2) new_data[ci] = pdata2[ci2];
        GT_P_MEM_RESET;
        P_PrintUsRange();
    }
    //在内部从新分配足够空间插入另一个同类型安全指针里不同于当前指针里的所有元素( 另一个安全指针 ) 返回另一个安全指针里的数据对应放到当前指针里的第几个位置 该返回值的数据个数和另一安全指针的数据数对应
    inline P<unsigned int> InsertNEQ(const P &pm) {
        if(!pm.zData) {
#ifdef GT_PMEM_WORNING
            GN_Log("P %s InsertNEQ 安全指针pm为空的!\n", zName[0] ? zName : "");
#endif
            return P<unsigned int>();
        }
        unsigned int ci, mi1, mi2, me1 = GT_P_ACTUAL(zData->num), me2 = GT_P_ACTUAL(pm.zData->num);
        unsigned int add_em_num = 0, new_num = me1;
        T *pdata1 = (T *)(zData->pmem), *pdata2 = (T *)(pm.zData->pmem);
        T *add_em = new T[me2];
        P<unsigned int> em_indexs(me2);
        for(mi2 = 0; mi2 < me2; ++mi2) {
            for(mi1 = 0; mi1 < me1; ++mi1) {
                if(pdata2[mi2] == pdata1[mi1]) { //原元素已有另一个安全指针的这个元素
                    em_indexs[mi2] = mi1;
                    break;
                }
            }
            if(mi1 >= me1) {
                add_em[add_em_num] = pdata2[mi2]; //原元素没有另一个安全指针的这个元素就加入它
                em_indexs[mi2] = new_num;
                add_em_num++;
                new_num++;
            }
        }
        if(new_num > me1) { //有不同的元素加入
            T *new_data = new T[new_num];
            for(ci = 0; ci < me1; ++ci) new_data[ci] = pdata1[ci];
            for(add_em_num = 0; ci < new_num; ++ci, ++add_em_num) new_data[ci] = add_em[add_em_num];
            GT_P_MEM_RESET;
        }
        delete[] add_em;
        P_PrintUsRange();
        return em_indexs;
    }
    //在内部移除指定范围内的元素( 开始移除的元素索引号 , 共移除几个元素) !!如果有其它安全指针和它共享同一片数据 这修改将会影响其它共享的安全指针 如果指针指向的数据只有一个元素则忽略该处理
    inline void Remove(unsigned int id, unsigned int count) {
#ifdef GT_PMEM_WORNING
        if(!count) {
            GN_Log("P %s Remove count = 0? !!\n", zName[0] ? zName : "");
            return;
        }
#endif
        unsigned int data_num = GT_P_ACTUAL(zData->num);
        if(data_num < 2 || id >= data_num) return; //判断处理可能用户乱填的参数范围
        if(id) {
            if(id + count >= data_num) count = data_num - id;
        } else {
            if(/*id + */ count >= data_num) count = data_num - 1;
        }
        unsigned int ci, ci2, new_num = data_num - count;
        T *new_data = new T[new_num];
        for(ci2 = ci = 0; ci < id; ++ci, ++ci2) new_data[ci2] = ((T *)(zData->pmem))[ci];
        for(ci = id + count; ci < data_num; ++ci, ++ci2) new_data[ci2] = ((T *)(zData->pmem))[ci];
        GT_P_MEM_RESET;
        P_PrintUsRange();
    }
    //在内部移除指定值的元素( 指定值 , 按顺序共移除几个指定值的元素)返回共移除了多少个元素 !!如果有其它安全指针和它共享同一片数据 这修改将会影响其它共享的安全指针 如果指针指向的数据只有一个元素则忽略该处理
    inline int RemoveV(T em, unsigned int count) {
#ifdef GT_PMEM_WORNING
        if(!count) {
            GN_Log("P %s RemoveV count = 0? !!\n", zName[0] ? zName : "");
            return 0;
        }
#endif
        unsigned int data_num = GT_P_ACTUAL(zData->num);
        if(data_num < 2) return 0;
        T *pdata = (T *)(zData->pmem);
        unsigned int ci = 0, cn = 0;
        for(; ci < data_num && cn < count; ++ci) {
            if(pdata[ci] == em) ++cn; //检查需要移除的个数
        }
        if(!cn) return 0;
        unsigned int ci2, new_num = data_num - cn;
        T *new_data = new T[new_num];
        for(ci = 0, ci2 = 0, cn = 0; ci < data_num; ++ci) {
            if(cn < count && pdata[ci] == em) {
                ++cn;
            } else {
                new_data[ci2] = pdata[ci];
                ++ci2;
            }
        }
        GT_P_MEM_RESET;
        P_PrintUsRange();
        return cn;
    }
    //逆向从普通指针找回它的安全指针( 普通指针 ) !!这需要一些处理时间 这普通指针必需是用安全方式创建的 被安全指针包含 否侧该指针会变为空的
    inline void Re(T *p) {
        this->Delete();
        GN::sLIST<GT_sMEMORY>::iterator ir = GT_UsMemTable.memList.begin(), er = GT_UsMemTable.memList.end();
        for(; ir != er; ++ir) {
            if(p == (*ir).pmem) {
                zData = &(*ir);
                ++(zData->us);
                P_PrintUsRange();
                return;
            }
        }
    }
    //比较是否指向相同的内存
    inline bool operator==(const P &pm) const { return this->zData == pm.zData; }

    //比较是否指向不同的内存
    inline bool operator!=(const P &pm) const { return this->zData != pm.zData; }

    //获得这快内存的数据个数
    inline unsigned int Num() { return zData ? GT_P_ACTUAL(zData->num) : 0; }

    //获得这快内存的大小字节
    inline unsigned int Size() { return zData ? sizeof(T) * (GT_P_ACTUAL(zData->num)) : 0; }

    //获得这快内存的共享者数
    inline unsigned int Us() { return zData ? zData->us : 0; }
#if defined _WIN32_ && defined __LP64__
#define GT_PMEM_SIZE_T size_t
#else
#define GT_PMEM_SIZE_T uint64
#endif
#define GT_PMEM_INDEX_FUNC_DEF(RET)           \
    /*引用内存数据元数( 索引号 ) */           \
    inline T &operator[](unsigned int id) {   \
        RET                                   \
    }                                         \
    inline T &operator[](int id) {            \
        RET                                   \
    }                                         \
    inline T &operator[](unsigned long id) {  \
        RET                                   \
    }                                         \
    inline T &operator[](long id) {           \
        RET                                   \
    }                                         \
    inline T &operator[](unsigned short id) { \
        RET                                   \
    }                                         \
    inline T &operator[](short id) {          \
        RET                                   \
    }                                         \
    inline T &operator[](unsigned char id) {  \
        RET                                   \
    }                                         \
    inline T &operator[](char id) {           \
        RET                                   \
    }                                         \
    inline T &operator[](int8 id) {           \
        RET                                   \
    }                                         \
    inline T &operator[](GT_PMEM_SIZE_T id) { \
        RET                                   \
    }

#ifdef GT_PMEM_WORNING
    //引用内存数据元数( 索引号 )
    inline T &operator[](unsigned int id) {
        GT_P_MEM_NOINIT_TIPS1( "[]" ）
        unsigned int data_num = GT_P_ACTUAL(zData->num);
		if(id >= data_num){
            GN_Log("P %s 内存访问出轨!!\n", zName[0] ? zName : "");
            id = id % data_num; //防止索引出轨
		}
		return ((T*)(zData->pmem))[id];
    }
    GT_PMEM_INDEX_FUNC_DEF(return (*this)[(unsigned int)id];)
#else
    GT_PMEM_INDEX_FUNC_DEF(return ((T *)(zData->pmem))[id];)
#endif
    //从指针抽取数据
    inline T &operator*() {
#ifdef GT_PMEM_WORNING
#define GT_P_MEM_NOINIT_TIPS(F, PROC)                                           \
    if(!zData) {                                                                \
        GN_Log("P %s ::%s 使用了未初始化的指针!\n", zName[0] ? zName : ""， F); \
        PROC                                                                    \
    }
#define GT_P_MEM_NOINIT_TIPS1(F) GT_P_MEM_NOINIT_TIPS(F, this->New(1);)

        GT_P_MEM_NOINIT_TIPS1("*()")
#endif
        return *((T *)zData->pmem);
    }
    //指向成员
    inline T *operator->() const {
#ifdef GT_PMEM_WORNING
        GT_P_MEM_NOINIT_TIPS( "->" ,  ）
#endif
		return (T*)(zData->pmem);
    }
    //指向成员
    inline T *operator->() {
#ifdef GT_PMEM_WORNING
        GT_P_MEM_NOINIT_TIPS( "->" ,  ）
#endif
		return (T*)(zData->pmem);
    }
    /*!以下这些把它的实际内存地址挖回出来 提供更好的性能 但不安全 最好在局部函数里用 尽量不要超出单个函数范围内!*/

    //转换为普通指针 这个一般在参数传递和赋值给普通指针时会自动调用 连 if( P ) 也会被调用的
    inline operator const T *() const {
        if(zData) return (T *)(zData->pmem);
        else return 0;
    }
    inline operator T *() {
        if(zData) return (T *)(zData->pmem);
        else return 0;
    }
    //获得内存首地址
    inline T *Begin() {
#ifdef GT_PMEM_WORNING
        GT_P_MEM_NOINIT_TIPS1("Begin")
#endif
        return (T *)(zData->pmem);
    }
    //获得内存尾地址
    inline T *End() {
#ifdef GT_PMEM_WORNING
        GT_P_MEM_NOINIT_TIPS1("End")
#endif
        unsigned int data_num = GT_P_ACTUAL(zData->num);
        return ((T *)zData->pmem) + data_num;
    }
};
template <class T>
bool P<T>::copyMode = false;

/* ----------------------------------------------------------- sNODE ---------------------------------------------------------------------------------------------------------------------- */
template <class T>
bool GT_NodeCollect(T &node, void *arg) {
    ((GN::sLIST<T> *)arg)->push_back(node);
    return true;
}
//- 对象数据重构造 ------------------------------------
#ifdef _MSC_VER
#define GT_TMP_OBJ_RE_CONSTRUCTOR(OBJ_TYPE) \
    if(aName) {                             \
        this->OBJ_TYPE ::~OBJ_TYPE();       \
        this->OBJ_TYPE ::OBJ_TYPE();        \
    }
#define GT_RE_CONSTRUCTOR(CTYPE) \
    this->CTYPE ::~CTYPE();      \
    this->CTYPE ::CTYPE();
#else
#define GT_TMP_OBJ_RE_CONSTRUCTOR(OBJ_TYPE) \
    if(aName) {                             \
        this->OBJ_TYPE ::~OBJ_TYPE();       \
        new(this) OBJ_TYPE();               \
    }
#define GT_RE_CONSTRUCTOR(CTYPE) \
    this->CTYPE ::~CTYPE();      \
    new(this) CTYPE();
#endif //----------------------------------------------


#endif // GN_CONTAINERS_SMARTPTR_H
