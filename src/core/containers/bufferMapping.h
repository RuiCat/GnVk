// GN_mini 容器拆分片段：缓冲映射 GT_sBUF_MAPPING + GN 用量管理/内存池1/分配器。
// 2026-11-26 自 GN_Array.h 第 1052-1204 行 move-only 拆出（REFACTOR_PLAN §13 B3）。
// 由 GN_Array.h 按原顺序转发包含；单独包含需自行补齐前置依赖。
#ifndef GN_CONTAINERS_BUFFERMAPPING_H
#define GN_CONTAINERS_BUFFERMAPPING_H

// 自包含依赖（B3 拆分时补齐；正文仍为 move-only）
#include "baseTypes.h"
#include "memMacros.h"
#include "array.h"
#include <cstdlib>
#include <cstring>
#include "GN_Base.h"

/*--------------------------------------------------- 模拟STL的内存块分配器 -----------------------------------------------------------*/
struct GN_API GT_sBUF_MAPPING //自动增长的缓冲映射
{
    GN::sARRAY_VEC<uint8 *, unsigned int> zBufs; //一组缓冲
    unsigned int zBufLen; //单个缓冲大小字节
    unsigned int zUnitSize; //单个数据大小字节
    unsigned int zBufElmNum; //单个缓冲可存多少个数据

    inline GT_sBUF_MAPPING(unsigned int unit_size = 1) : zBufLen(0), zUnitSize(unit_size) {}
    inline ~GT_sBUF_MAPPING() { this->Release(); }

    //释放内部数据
    void Release();

    //初始化(预分配多少个缓冲, 单个缓冲可存多少个数据)
    inline void InitToData(unsigned int count, unsigned int em_num) {
        zBufElmNum = em_num;
        zBufLen = zBufElmNum * zUnitSize;
        zBufs.New(count);
    }
    //加入一块缓冲
    inline void PushBuf() {
        zBufs.push_back((uint8 *)malloc(zBufLen));
    }
    //获得数据存放空间指针(数据存放位置)
    inline uint8 *GetPtr(unsigned int pos) {
        unsigned int bufid = pos ? pos / zBufElmNum : 0;
        return zBufs.data[bufid] + ((pos - (bufid * zBufElmNum)) * zUnitSize);
    }
    //获得数据存放位置(数据存放空间指针)
    unsigned int GetPos(const uint8 *p);
};
/* ----------------------------------------------------------------------------------------------------------------------------*/
namespace GN {
class GN_API cVOLUME_MAG1 //用量管理器 - 仅能分配每份相等的用量 分配速度快
{
    unsigned int (*zpfOnFull)(unsigned int tot_size, void *us_arg); //当容量满时的回调
    void *zUsArg; //用户参数
    unsigned int zTotalSize; //总空间大小个数
    unsigned int zUsedSize; //已使用大小个数
    unsigned int *zPosData; //可用位置数据
    unsigned int zPosNum; //可用位置数
  public:
    cVOLUME_MAG1() : zTotalSize(0), zPosData(0) {}
    ~cVOLUME_MAG1() { this->Release(); }

    //释放内部资源
    void Release();

    //初始化(容量总大小个数, 当容量满时的回调(当前总大小个数, 传回给您用户参数)要求返回新分配大小个数, 用户参数)
    void InitToData(unsigned int total_size, unsigned int (*full_proc)(unsigned int tot_size, void *us_arg), void *_us_arg);

    //空间申请()返回空间的位置
    unsigned int Alloc();

    //归还空间(空间位置)
    inline void Free(unsigned int pos) {
        zPosData[zPosNum++] = pos;
    }
    //获得当前总容量大小
    unsigned int GetTotalSize() { return zTotalSize; }

    //获得最尾空段范围(是否精确检测 这稍慢一点)
    sSEGMENT<unsigned int> GetEndSegRange(bool accur = true);
};
/* ------------------------------------------------------------------------------------------------------------------*/
class GN_API cMEM_POOL1 : public cVOLUME_MAG1, public GT_sBUF_MAPPING //内存池 - 仅能分配每份相等的空间 分配速度快
{
  public:
#define GT_TMP_MEM_POOL_BASE_FUNC_DEF(N)                                     \
    /*构造对象(单个数据大小)*/                                               \
    cMEM_POOL##N(unsigned int unit_size = 1) : GT_sBUF_MAPPING(unit_size) {} \
    ~cMEM_POOL##N() {}                                                       \
                                                                             \
    /*释放内部资源*/                                                         \
    void Release() {                                                         \
        GT_sBUF_MAPPING::Release();                                          \
        cVOLUME_MAG##N::Release();                                           \
    }
    GT_TMP_MEM_POOL_BASE_FUNC_DEF(1)

    //初始化(容量总大小个数, 当容量满时的回调(当前总大小个数, 传回给您用户参数)要求返回新分配大小个数, 用户参数) !!后两个参数在该类中无效
    void InitToData(unsigned int total_size, unsigned int (*full_proc)(unsigned int tot_size, void *us_arg) = 0, void *_us_arg = 0);

    //空间申请(参数无效为兼容其它接口而设)返回空间指针
    uint8 *Alloc(unsigned int size = 1) {
        return GetPtr(cVOLUME_MAG1::Alloc());
    }
    //释放归还空间(空间指针, 此参数无效为兼容其它接口而设)
    void Free(const uint8 *p, unsigned int size = 1) {
        cVOLUME_MAG1::Free(GetPos(p));
    }
};
template <class T, class T2 = size_t, class T3 = cMEM_POOL1>
class cALLOC_POOL //空间分配器 使用内存池 并兼容STD标准
{
    T3 zVmem; //模拟内存

  public: //为兼容STD标准
    typedef T value_type;
    typedef T2 size_type;
    typedef std::ptrdiff_t difference_type;
    typedef value_type &reference;
    typedef const value_type &const_reference;
    typedef value_type *pointer;
    typedef const value_type *const_pointer;

    template <typename U>
    struct rebind {
        typedef cALLOC_POOL<U, T2, T3> other;
    };

    inline cALLOC_POOL() {}

    template <class U>
    cALLOC_POOL(const cALLOC_POOL<U, T2, T3> &al) {}
    /*
    cALLOC_POOL(const cALLOC_POOL<T>&){ }

	template<class _Other>
	cALLOC_POOL<T>& operator=(const cALLOC_POOL<_Other>&){
		return (*this);
	}
*/
    //初始化
    void InitToData() {
        zVmem.zUnitSize = sizeof(value_type);
        zVmem.InitToData(256);
    }
    //分配空间(多少个数据空间, 接收数据空间的指针)返回空间指针
    inline pointer allocate(size_type n, pointer **p = 0) {
        if(!zVmem.zBufLen) InitToData();
        return (pointer)zVmem.Alloc(n);
    }
    //释放空间(空间指针, 释放多少个数据空间)
    inline void deallocate(pointer p, size_type n = 1) {
        zVmem.Free((uint8 *)p, n);
    }
    //为指定的空间指针调用构造函数(空间指针, 初始值)
    inline void construct(pointer p, const value_type &v) {
        new(p) value_type(v);
    }
    //销毁空间指针指向的对象(空间指针) 即调用对象的析构函数
    inline void destroy(pointer p) {
        p->~value_type();
    }
    //可分配最大大小个空间
    size_type max_size() const {
        size_type size = (size_type)(-1) / sizeof(value_type);
        return (0 < size ? size : 1);
    }
};
}; // namespace GN

#endif // GN_CONTAINERS_BUFFERMAPPING_H
