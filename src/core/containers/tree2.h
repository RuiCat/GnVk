// GN_mini 容器拆分片段：段比较函数 + GN::cTREE2/cVOLUME_MAG/cMEM_POOL。
// 2026-11-26 自 GN_Array.h 第 2812-3064 行 move-only 拆出（REFACTOR_PLAN §13 B3）。
// 由 GN_Array.h 按原顺序转发包含；单独包含需自行补齐前置依赖。
#ifndef GN_CONTAINERS_TREE2_H
#define GN_CONTAINERS_TREE2_H

// 自包含依赖（B3 拆分时补齐；正文仍为 move-only）
#include "baseTypes.h"
#include "list.h"
#include "bufferMapping.h"
#include <memory>
#include <cstring>
#include "GN_Base.h"

/*-------------------------------------------------------------------------------------------------------------*/
template <class T>
inline int GT_CompareNodeNumber(T &cur_v, T &v) {
    return cur_v == v ? 0 : (v < cur_v ? -1 : 1);
}
template <class T>
inline int GT_CompareImgHeightMin(T cur_v, T v) {
    return cur_v->h == v->h ? 0 : (v->h < cur_v->h ? -1 : 1);
}
template <class T>
inline int GT_CompareImgHeightMin1(T cur_v, T v) {
    return v->h > cur_v->h ? 1 : -1;
}
template <class T>
int GT_CompareSegPosMin(T &cur_v, T &v) {
    return cur_v.pos == v.pos ? 0 : (v.pos < cur_v.pos ? -1 : 1);
}
template <class T>
int GT_CompareSegLenMin(T &cur_v, T &v) {
    return cur_v.len == v.len ? 0 : (v.len < cur_v.len ? -1 : 1);
}
template <class T>
int GT_CompareSegLenMin1(T &cur_v, T &v) {
    return v.len > cur_v.len ? 1 : -1;
}
template <class T>
int GT_CompareSegLenMin2(T &cur_v, T &v) {
    return cur_v.len == v.len ? (cur_v.pos == v.pos ? 0 : -1) : (v.len < cur_v.len ? -1 : 1);
}
namespace GN {
template <class T, class A = std::allocator<T>>
class cTREE2 //二分查找树
{
    template <class T_, class A_>
    struct s_TREE2_NODE //二分查找树节点
    {
        typedef typename A_::template rebind<s_TREE2_NODE>::other::pointer TNP;

        T_ zData; //数据
        TNP zLeft; //左子节点
        TNP zRight; //右子节点

        s_TREE2_NODE(T_ &v) : zData(v), zLeft(0), zRight(0) {}
    };
    typedef typename A::template rebind<s_TREE2_NODE<T, A>>::other ALLOC;

  public:
    typedef typename A::template rebind<s_TREE2_NODE<T, A>>::other::pointer TP;

  private:
    TP zRoot; //跟节点
    ALLOC zAlloc; //节点空间分配器
  public:
    cTREE2() : zRoot(0) {}
    ~cTREE2() { this->clear(); }

    //清除指定节点(节点)
    void zClear(TP &p);

    //清除树所有节点
    inline void clear() {
        zClear(zRoot);
        zRoot = 0;
    }
    //获得根节点指针
    inline TP *GetRoot() { return &zRoot; }

    //采用二分查找内容符合条件的节点(比较条件(当前节点内容,比较内容)要求返回查找方向值, 传入的比较内容, 返回符合条件的节点或比较接近条件的终止节点指针)返回最后的查找方向值 值0:找到符合条件的节点  -1:表示在接近条件的左侧终结了 1:表示在接近条件的右侧终结了
    int search2_if(int (*compare)(T &cur_v, T &v), T &cmp_v, TP **node_out);

    //采用二分查找内容符合条件的节点(比较条件(当前节点内容,比较内容)要求返回查找方向值, 传入的比较内容, 返回符合条件的节点或比较接近条件的终止节点指针, 返回比node_out大最小的节点指针 如果无返回0)返回最后的查找方向值 值0:找到符合条件的节点  -1:表示在接近条件的左侧终结了 1:表示在接近条件的右侧终结了
    int search2_if(int (*compare)(T &cur_v, T &v), T &cmp_v, TP **node_out, TP **up_node_out);

    //采用二分查找内容符合条件的节点(比较条件(当前节点内容,比较内容)要求返回查找方向值, 传入的比较内容, 返回符合条件的节点或比较接近条件的终止节点指针, 返回比node_out大最小的节点指针 如果无返回0, 返回比node_out小最小的节点指针 如果无返回0)返回最后的查找方向值 值0:找到符合条件的节点  -1:表示在接近条件的左侧终结了 1:表示在接近条件的右侧终结了
    int search2ff_if(int (*compare)(T &cur_v, T &v), T &cmp_v, TP **node_out, TP **up_node_out, TP **down_node_out);

    //在指定节点里插入一个内容(节点指针, 内容)
    inline void insert(TP *node, T &v) {
        (*node) = zAlloc.allocate(1, &node);
        *(*node) = s_TREE2_NODE<T, A>(v);
    }
    //插入一个内容值(内容, 插入排位条件(当前节点内容, 插入内容)要求返回排位查找方向值)返回插入值存放节点指针 !排位查找方向值 值0:查找方向终止直接覆盖当前节点内容 -1:再往当前节点左侧排 1:再往当前节点右侧排
    TP *insert(T &v, int (*compare)(T &cur_v, T &v) = GT_CompareNodeNumber) {
        TP *find_node;
        if(search2_if(compare, v, &find_node) == 0) {
            (*find_node)->zData = v;
        } else insert(find_node, v);
        return find_node;
    }
    //查找删除符合条件内容的节点(条件内容, 比较条件(当前节点内容, 条件内容)要求返回查找方向值) !查找方向值 值0:查找方向终止删除当前节点内容 -1:再往当前节点左侧查 1:再往当前节点右侧查
    bool erase(T &v, int (*compare)(T &cur_v, T &v) = GT_CompareNodeNumber);

    //删除指定节点(节点指针)
    void erase(TP *node);

    //获得指定节点最靠左的结尾节点(指定节点, 返回结尾节点指针)返回该路径有多少个有效节点
    int zGetLeftEndNode(TP p, TP **node_out);

    //获得指定节点最靠右的结尾节点(指定节点, 返回结尾节点指针)返回该路径有多少个有效节点
    int zGetRightEndNode(TP p, TP **node_out);
};
template <class T, class A>
void cTREE2<T, A>::zClear(TP &p) {
    if(!p) return;
    zClear(p->zLeft);
    zClear(p->zRight);
    zAlloc.destroy(p);
    zAlloc.deallocate(p, 1);
}
template <class T, class A>
int cTREE2<T, A>::search2_if(int (*compare)(T &cur_v, T &v), T &cmp_v, TP **node_out) {
#define GT_TREE2_SEARCH_IF(ADD_PROC, ADD_PROC2)            \
    int ret = -1;                                          \
    TP *find_node = &zRoot;                                \
    for(;;) {                                              \
        if(!(*find_node)) {                                \
            *node_out = find_node;                         \
            return ret;                                    \
        }                                                  \
        ret = compare((*find_node)->zData, cmp_v);         \
        if(!ret) {                                         \
            *node_out = find_node;                         \
            return 0;                                      \
        } else if(ret < 0) {                               \
            ADD_PROC find_node = &((*find_node)->zLeft);   \
        } else {                                           \
            ADD_PROC2 find_node = &((*find_node)->zRight); \
        }                                                  \
    }
    GT_TREE2_SEARCH_IF(, )
}
template <class T, class A>
int cTREE2<T, A>::search2_if(int (*compare)(T &cur_v, T &v), T &cmp_v, TP **node_out, TP **up_node_out) {
    *up_node_out = 0;
    GT_TREE2_SEARCH_IF(*up_node_out = find_node;, )
}
template <class T, class A>
int cTREE2<T, A>::search2ff_if(int (*compare)(T &cur_v, T &v), T &cmp_v, TP **node_out, TP **up_node_out, TP **down_node_out) {
    *up_node_out = 0;
    *down_node_out = 0;
    GT_TREE2_SEARCH_IF(*up_node_out = find_node;, *down_node_out = find_node;)
}
template <class T, class A>
bool cTREE2<T, A>::erase(T &v, int (*compare)(T &cur_v, T &v)) {
    TP *find_node;
    if(search2_if(compare, v, &find_node) == 0) {
        erase(find_node);
        return true;
    }
    return false;
}
template <class T, class A>
void cTREE2<T, A>::erase(TP *node) {
    TP *l_node, *r_node;
    TP del_node = (*node);
    if(!(del_node->zRight)) *node = del_node->zLeft;
    else if(!(del_node->zLeft)) *node = del_node->zRight;
    else {
        if(zGetLeftEndNode(del_node->zRight, &l_node) < zGetRightEndNode(del_node->zLeft, &r_node)) {
            *l_node = del_node->zLeft;
            *node = del_node->zRight;
        } else {
            *r_node = del_node->zRight;
            *node = del_node->zLeft;
        }
    }
    zAlloc.destroy(del_node);
    zAlloc.deallocate(del_node, 1);
}
template <class T, class A>
int cTREE2<T, A>::zGetLeftEndNode(TP p, TP **node_out) {
#define GT_TMP_TREE2_GET_END_NODE(L) \
    int i = 0;                       \
    for(;; ++i) {                    \
        *node_out = &(p->L);         \
        p = p->L;                    \
        if(!p) return i;             \
    }
    GT_TMP_TREE2_GET_END_NODE(zLeft)
}
template <class T, class A>
int cTREE2<T, A>::zGetRightEndNode(TP p, TP **node_out) {
    GT_TMP_TREE2_GET_END_NODE(zRight)
}
/* ------------------------------------------------------------------------------------------------------------------*/
class GN_API cVOLUME_MAG //用量管理器
{
    typedef GN::cTREE2<sSEGMENT<unsigned int>, GN::cALLOC_POOL<sSEGMENT<unsigned int>>> cTREE2_UINTx2;
    unsigned int (*zpfOnFull)(unsigned int tot_size, unsigned int need_size, void *us_arg); //当容量满时的回调
    void *zUsArg; //用户参数
    unsigned int zTotalSize; //总空间大小个数
    cTREE2_UINTx2 zSizeTable; //按大小的2叉查找表
    cTREE2_UINTx2 zPosTable; //按存储位置的2叉查找表

  public:
    cVOLUME_MAG() : zTotalSize(0) {}
    ~cVOLUME_MAG() { this->Release(); }

    //释放内部资源
    void Release();

    //初始化(容量总大小个数, 当容量满时的回调(当前总大小个数, 当前需要分配的最小大小个数, 传回给您用户参数)要求返回新分配大小个数, 用户参数)
    void InitToData(unsigned int total_size, unsigned int (*full_proc)(unsigned int tot_size, unsigned int need_size, void *us_arg), void *_us_arg);

    //用量空间申请(空间大小个数)返回空间的位置
    unsigned int Alloc(unsigned int size);

    //归还空间(空间位置, 空间大小个数, 归还的空间可否与前面剩余空间合并, 归还的空间可否与后面剩余空间合并)
    void Free(unsigned int pos, unsigned int size, bool merge_front = true, bool merge_back = true);

    //获得当前总容量大小
    unsigned int GetTotalSize() { return zTotalSize; }

    //获得最尾空段范围
    sSEGMENT<unsigned int> GetEndSegRange();
};
/*----------------------------------------------------------------------------------------------------------------------------*/
class GN_API cMEM_POOL : public cVOLUME_MAG, public GT_sBUF_MAPPING //内存池
{
  public:
    GT_TMP_MEM_POOL_BASE_FUNC_DEF()

    //初始化(容量总大小个数, 当容量满时的回调(当前总大小个数, 当前需要分配的最小大小个数, 传回给您用户参数)要求返回新分配大小个数, 用户参数) !!后两个参数在该类中无效
    void InitToData(unsigned int total_size, unsigned int (*full_proc)(unsigned int tot_size, unsigned int need_size, void *us_arg) = 0, void *_us_arg = 0);

    //空间申请(多少个数据空间)返回空间指针
    uint8 *Alloc(unsigned int size = 1) {
        return GetPtr(cVOLUME_MAG::Alloc(size));
    }
    //释放归还空间(空间指针, 释放多少个数据空间)
    void Free(const uint8 *p, unsigned int size = 1) {
        unsigned int pos = GetPos(p);
        cVOLUME_MAG::Free(pos, size, (pos % zBufElmNum) != 0, ((pos + size) % zBufElmNum) != 0);
    }
    // ------- 以下两个函数用于模拟常规的内存分配 malloc 和 free  数据单位视为字节 使用这两个函数 该对象构造时单个数据大小必须为1 --------------------------

    //通用字节空间申请(多少个字节空间)返回空间指针
    uint8 *AllocByte(unsigned int size = 1) {
#ifdef GN_DEBUG
        if(zUnitSize > 1) GN_Log("调用 cMEM_POOL::AllocByte 存在问题!! 该对象目前管理的数据单个大小为:%d 超过1字节!\n", zUnitSize);
#endif
        size += 4;
        unsigned int *p = (unsigned int *)GetPtr(cVOLUME_MAG::Alloc(size));
        *p = size;
        return (uint8 *)(p + 1);
    }
    //释放归还通用字节空间(空间指针)
    inline void FreeByte(const uint8 *p) {
        unsigned int *p1 = ((unsigned int *)p) - 1;
        Free((uint8 *)p1, *p1);
    }
};
}; // namespace GN

#endif // GN_CONTAINERS_TREE2_H
