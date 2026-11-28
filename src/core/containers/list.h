// GN_mini 容器拆分片段：链表节点/迭代器1 + GN::sLIST + GN::sLIST_WHEEL。
// 2026-11-26 自 GN_Array.h 第 1205-2361 行 move-only 拆出（REFACTOR_PLAN §13 B3）。
// 由 GN_Array.h 按原顺序转发包含；单独包含需自行补齐前置依赖。
#ifndef GN_CONTAINERS_LIST_H
#define GN_CONTAINERS_LIST_H

// 自包含依赖（B3 拆分时补齐；正文仍为 move-only）
#include "array.h"
#include "baseTypes.h"
#include <memory>
#include <list>

/*********************************************************** 单向链表 ***********************************************************************/

//简单单向连表加入一个节点到队列的首部( 队列 , 要加进去的节点 )
#define GT_MX_PushFront(FLIST, F) \
    {                             \
        (F)->next_node = (FLIST); \
        (FLIST) = (F);            \
    }

//简单单向连表历遍开始( 连表类型 , 连表 )
#define GT_MX_FindBegin(TYPE, LIST)    \
    {                                  \
        GT_s_LITMK_FACE *_last = LIST; \
        GT_s_LITMK_FACE *_cur = _last; \
        for(; _cur;) {

//简单单向连表历遍时移除当前检索节点( 连表 )
#define GT_MX_FindRemoveCur(LIST)    \
    {                                \
        if(_cur == _last) {          \
            _cur = _cur->next_node;  \
            LIST = _last = _cur;     \
        } else {                     \
            _cur = _cur->next_node;  \
            _last->next_node = _cur; \
        }                            \
        continue;                    \
    }

//简单单向连表历遍时跳转继续检索下个节点( 连表 )
#define GT_MX_FindContinue()    \
    {                           \
        _last = _cur;           \
        _cur = _cur->next_node; \
        continue;               \
    }

//简单单向连表历遍结束
#define GT_MX_FindEnd()     \
    _last = _cur;           \
    _cur = _cur->next_node; \
    }                       \
    }

template <class T>
//连接合并两个简单单向连表(首个连表, 尾个连表)返回合并后的连表首部
T GT_MX_Merge(T frist, T last) {
    if(frist) {
        if(last) {
            T _fmk = frist;
            for(; _fmk->next_node; _fmk = _fmk->next_node);
            _fmk->next_node = last;
        }
        return frist;
    } else return last;
}

template <class T>
struct GT_sNODE_BASE //基础简单单向连表节点
{
    GT_sNODE_BASE<T> *next_node; //下一节点
    T v; //值
};
/*********************************************************** 双向链表相关 *****************************************************************/
template <class T>
struct GT_sLNODE;

template <class T>
struct GT_sLNODE_ //链表端点
{
    GT_sLNODE<T> *last; //上一个数据节点
    GT_sLNODE<T> *next; //下一个数据节点
};
template <class T>
struct GT_sLNODE : public GT_sLNODE_<T> //链表数据节点
{
    T em; //值元素
};
/* 链表 历遍搜索方法  -------------------------------------------------------------------------------------------------------- */
template <class TW, class T>
struct GT_sIterator1 //顺向搜索迭代器
{
    TW *lw; //指向关联节点

    inline GT_sIterator1() {}
    inline GT_sIterator1(TW *_lw) : lw(_lw) {}

    inline bool operator==(const GT_sIterator1 &it) const { return (this->lw == it.lw); }
    inline bool operator!=(const GT_sIterator1 &it) const { return (this->lw != it.lw); }
    inline T &operator*() { return lw->em; }
    inline T *operator->() { return &(lw->em); }

    inline GT_sIterator1 &operator++() {
        lw = lw->next;
        return (*this);
    } /*模板代码:迭代器增减*/
    inline GT_sIterator1 operator++(int) {
        GT_sIterator1 tmp = (*this);
        lw = lw->next;
        return tmp;
    }
    inline GT_sIterator1 &operator--() {
        lw = lw->last;
        return (*this);
    }
    inline GT_sIterator1 operator--(int) {
        GT_sIterator1 tmp = (*this);
        lw = lw->last;
        return tmp;
    }
};
template <class ITER>
struct GT_sReverseIterator1 : public ITER //逆向搜索迭代器
{
    inline GT_sReverseIterator1 &operator++() {
        this->lw = this->lw->last;
        return (*this);
    } /*模板代码:迭代器增减*/
    inline GT_sReverseIterator1 operator++(int) {
        GT_sReverseIterator1 tmp = (*this);
        this->lw = this->lw->last;
        return tmp;
    }
    inline GT_sReverseIterator1 &operator--() {
        this->lw = this->lw->next;
        return (*this);
    }
    inline GT_sReverseIterator1 operator--(int) {
        GT_sReverseIterator1 tmp = (*this);
        this->lw = this->lw->next;
        return tmp;
    }
};
/* --------------------------------------------------------- sLIST ----------------------------------------------------------- */
#define GT_US_STDLIST //使继承自stl的链表
#ifdef GT_US_STDLIST
#ifdef _MSC_VER
#if _MSC_VER < 1600 || _MSC_VER >= 1900
#if _MSC_VER >= 1900
#define GT_TMP_ITORID 0
#else
#define GT_TMP_ITORID 1
#endif
/* !以下这个方法比较硬核 如果其它平台的C标准库有些差异可能会影响到这里导致出轨 可以用以下方法来输出 历遍游标和数据地址看看它们的关系再作更改
	  GN_Log("%08X, %08X, %08X\n",((unsigned int **)(&itor_))[0],((unsigned int **)(&itor_))[1], &(*itor_) );
	*/
//把连表里指定数据的地址指针转回历遍迭代器(数据类型, 分配器类型, 对应连表, 连表里指定数据的地址指针)
#define GT_ReListIterator(list_type, alloc_type, list_, p_tg)                           \
    GN::sLIST<list_type alloc_type>::iterator itor_ = (list_).begin();                  \
    void **pptr = (void **)(&itor_);                                                    \
    pptr[GT_TMP_ITORID] = ((void **)(p_tg)) - 2; /*用数据指针计算回连表内相对应的位置*/ \
    return itor_;
#else
#define GT_ReListIterator(list_type, alloc_type, list_, p_tg)                                \
    GN::sLIST<list_type alloc_type>::iterator itor_ = (list_).begin(), end_ = (list_).end(); \
    for(; itor_ != end_; ++itor_) {                                                          \
        if(&(*itor_) == (p_tg)) return itor_;                                                \
    }
#endif
#else
//把连表里指定数据的地址指针转回历遍迭代器(数据类型, 分配器类型, 对应连表, 连表里指定数据的地址指针)
#ifdef __LP64__
#define GT_ITER_PTR_OFS 16
#else
#define GT_ITER_PTR_OFS 8
#endif
#ifdef _LINUX_
#define GT_ReListIterator(list_type, alloc_type, list_, p_tg)         \
    unsigned char *pdr = ((unsigned char *)(p_tg)) - GT_ITER_PTR_OFS; \
    return *((std::_List_iterator<list_type> *)&pdr);
#else
#define GT_ReListIterator(list_type, alloc_type, list_, p_tg)         \
    unsigned char *pdr = ((unsigned char *)(p_tg)) - GT_ITER_PTR_OFS; \
    return *((typename std::list<list_type>::iterator *)&pdr);
#endif
#endif
#endif
namespace GN {
template <class T, class A = std::allocator<T>>
#ifdef GT_US_STDLIST
struct sLIST : public std::list<T, A>
#else
struct sLIST //★双向连表数据结构 实现高效动态增加删减数据元素 扩展了标准C的一些功能
#endif
{
//以下是模拟STL list 的一些定义 --------------------------------------------------
#ifdef _MSC_VER
#define GT_LIST_iterator(_T_) GN::sLIST<_T_>::iterator
#else
#ifdef _LINUX_
#define GT_LIST_iterator(_T_) std::_List_iterator<_T_>
#ifdef GT_US_STDLIST
    typedef std::_List_iterator<T> iterator;
#endif
#else
#define GT_LIST_iterator(_T_) typename GN::sLIST<_T_>::iterator
#ifdef GT_US_STDLIST
    typedef typename std::list<T, A>::iterator iterator;
#endif
#endif
#endif
#ifndef GT_US_STDLIST
    typedef typename A::template rebind<GT_sLNODE<T>>::other zAltype;

    GT_sLNODE_<T> z_Start; //头端点
    GT_sLNODE_<T> z_End; //末端点
    unsigned int zSize; //连表节点数
    zAltype zAlnod; //节点分配器

    //#define GT_TMP_ITERATOR_OPT_DEF( A , TYPE , B , E , RB , RE )

    typedef GT_sIterator1<GT_sLNODE<T>, T> iterator; /*该类内的顺向迭代器*/
    typedef GT_sReverseIterator1<iterator> reverse_iterator; /*该类内的逆向迭代器*/
    friend struct iterator;
    friend struct reverse_iterator;

    //获得开始端搜索迭代器
    inline iterator begin() { return iterator(z_Start.next); }

    //获得结尾端搜索迭代器
    inline iterator end() {
        return iterator((GT_sLNODE<T> *)&z_End);
    }
    //获得逆向开始端搜索迭代器
    inline reverse_iterator rbegin() {
        reverse_iterator rit;
        rit.lw = z_End.last;
        return rit;
    }

    //获得逆向结尾端搜索迭代器
    inline reverse_iterator rend() {
        reverse_iterator rit;
        rit.lw = (GT_sLNODE<T> *)&z_Start;
        return rit;
    }
    //重设内部状态
    inline void zReset() {
        zSize = 0;
        z_Start.next = (GT_sLNODE<T> *)&z_End;
        z_End.last = (GT_sLNODE<T> *)&z_Start;
    }

    //在指定位置前插入一个数据元素(位置, 值)
    void zInsert(iterator pos, const T &v) {
        GT_sLNODE<T> *new_node = zAlnod.allocate(1);
        new(&(new_node->em)) T(v);
        new(&(new_node->last)) GT_sLNODE<T> *();
        new(&(new_node->next)) GT_sLNODE<T> *();

        GT_sLNODE<T> *f = pos.lw->last;
        new_node->last = f;
        new_node->next = pos.lw;
        pos.lw->last = new_node;
        f->next = new_node;
        ++zSize;
    }
    //转移指定链表里一段范围的数据元素到该链表里(插入进该链表那个位置之前, 指定链表, 指定链表的范围段的首个位置, 范围段的尾个位置, 范围段的数据元素个数)
    void zSplice(iterator pos, sLIST &ls, iterator first, iterator last, unsigned int count) {
        if(&zAlnod == &(ls.zAlnod)) {
            if(this != &ls) {
                zSize += count;
                ls.zSize -= count;
            } //在之前的拿出
            GT_sLNODE<T> *__l = last.lw->last; //段的尾节点
            last.lw->last = first.lw->last; //断开就及时封口
            first.lw->last->next = last.lw;

            //放入到当前的
            GT_sLNODE<T> *l1 = pos.lw->last;
            l1->next = first.lw;
            first.lw->last = l1; //绑定段的首连
            pos.lw->last = __l;
            __l->next = pos.lw; //绑定段的尾连
        } else {
            insert(pos, first, last);
            ls.erase(first, last);
        }
    }
    //默认构造
    inline sLIST() { zReset(); }

    //构造指定数量的默认值元素(数量)
    inline sLIST(unsigned int count) {
        zReset();
        T v;
        unsigned int i = 0;
        for(; i < count; ++i) push_back(v);
    }
    //构造指定数量的指定值元素(数量, 指定值)
    inline sLIST(unsigned int count, const T &v) {
        zReset();
        unsigned int i = 0;
        for(; i < count; ++i) push_back(v);
    }
    //构造并插入一段范围的数据元素(范围段的首个位置, 范围段的尾个位置)
    inline sLIST(iterator frist, iterator last) {
        zReset();
        insert(begin(), frist, last);
    }
    //构造与指定链表拥有相同数据的链表(指定链表)
    inline sLIST(const sLIST &ls) {
        zReset();
        insert(begin(), ((sLIST &)ls).begin(), ((sLIST &)ls).end());
    }
    //析构
    inline ~sLIST() { this->clear(); }

    //使内部数据等同于指定链表(指定链表)
    inline sLIST &operator=(const sLIST &ls) {
        this->clear();
        insert(begin(), ((sLIST &)ls).begin(), ((sLIST &)ls).end());
        return *this;
    }
    //清除原有数据并用指定链表的值来重新赋值(链表)
    inline void assign(sLIST ls) {
        this->assign(ls.begin(), ls.end());
    }
    //清除原有数据重新赋值(范围段的首个位置, 范围段的尾个位置)
    inline void assign(iterator frist, iterator last) {
        this->clear();
        for(; frist != last; ++frist) push_back(*frist);
    }
    //清除原有数据重新赋值(数据个数, 值)
    inline void assign(unsigned int count, const T &v) {
        this->clear();
        for(; count; --count) push_back(v);
    }
    //反转链表中的数据元素顺序
    void reverse() {
        if(zSize > 1) {
            iterator last = end();
            for(iterator cur = ++begin(); cur != last;) {
                iterator before = cur;
                zSplice(begin(), *this, before, ++cur, 1);
            }
        }
    }
    //重设链表当前容纳数据个数(个数 这如小于之前的会自动移除尾部的一些数据元素)
    inline void resize(unsigned int new_size) {
        resize(new_size, T());
    }
    //重设链表当前容纳数据个数(个数 这如小于之前的会自动移除尾部的一些数据元素, 新增数据的值)
    inline void resize(unsigned int new_size, T v) {
        if(zSize < new_size) insert(end(), new_size - zSize, v);
        else {
            while(new_size < zSize) pop_back();
        }
    }
    //在指定位置前插入一个数据元素(位置, 值)返回元素插入的位置
    inline iterator insert(iterator pos, const T &v) {
        zInsert(pos, v);
        return (--pos);
    }
    //在指定位置前插入数据元素多次(位置, 次数, 值)
    inline void insert(iterator pos, unsigned int count, const T &v) {
        for(; count; --count) zInsert(pos, v);
    }
    //在指定位置前插入一段范围的数据元素(位置, 范围段的首个位置, 范围段的尾个位置)
    inline void insert(iterator pos, iterator frist, iterator last) {
        for(; frist != last; ++frist) zInsert(pos, *frist);
    }
    //在指定位置前插入另一个链表的所有数据元素 (位置, 另一个链表)
    inline void insert(iterator pos, sLIST &ls) {
        insert(pos, ls.begin(), ls.end());
    }
    //在尾部添加一个数据元素
    void push_back(const T &val) {
        zInsert(end(), val);
    }
    //在首部添加一个数据元素
    void push_front(const T &val) {
        zInsert(begin(), val);
    }
    //删除最头的一个数据元素
    void pop_front() {
        erase(begin());
    }
    //删除最尾的一个数据元素
    void pop_back() {
        erase(--end());
    }
    //删除指定位置指向的数据元素(位置) 返回迭代器的下个
    inline iterator erase(iterator _it) {
        GT_sLNODE<T> *tmp = _it.lw;
        if(tmp == (GT_sLNODE<T> *)&z_End || tmp == (GT_sLNODE<T> *)&z_Start) return _it;
        --zSize;
        _it.lw = tmp->next;
        tmp->last->next = _it.lw;
        _it.lw->last = tmp->last;

        tmp->~GT_sLNODE<T>();
        zAlnod.deallocate(tmp, 1);
        return _it;
    }
    //删除指定范围段的数据元素(范围段的首个位置, 范围段的尾个位置) 返回迭代器的下个
    inline iterator erase(iterator frist, iterator last) {
        for(; frist != last;) frist = erase(frist);
        return last;
    }
    //移除与指定值相等的所有数据元素(值)
    inline void remove(const T &v) {
        iterator last = end();
        for(iterator first = begin(); first != last;) {
            if(*first == v) first = erase(first);
            else ++first;
        }
    }
    template <class F>
    //移除所有符合条件的数据元素(条件方法)
    void remove_if(F cmpfunc) {
        iterator last = end();
        for(iterator first = begin(); first != last;) {
            if(cmpfunc(*first)) first = erase(first);
            else ++first;
        }
    }
    //删除连表中邻近相同值的数据元素
    void unique() {
#define GT_TMP_LIST_UNIQUE(CMP)                        \
    if(zSize > 1) {                                    \
        iterator first = begin(), last = end();        \
        iterator after = first;                        \
        for(++after; after != last;)                   \
            if(*first == *after) after = erase(after); \
            else first = after++;                      \
    }
        GT_TMP_LIST_UNIQUE(*first == *after)
    }
    template <class F>
    //删除连表中邻近且符合条件的数据元素(条件方法)
    void unique(F cmpfunc) {
        GT_TMP_LIST_UNIQUE(cmpfunc(*_First, *_After))
    }
    //清除全部数据元素
    void clear() {
        if(zSize) {
            /*GT_sLNODE<T> *_cur1, *_cur = z_Start.next, *_end = (GT_sLNODE<T>*)&(z_End);
			zReset();
			for(; _cur != _end; _cur = _cur1){
				_cur1 = _cur->next;
				_cur->~GT_sLNODE<T>();
				zAlnod.deallocate(_cur,1);
			}*/
            erase(begin(), end());
        }
    }
    //交换两链表所有内容(另一链表)
    void swap(sLIST &ls) {
        z_End.last->next = (GT_sLNODE<T> *)&(ls.z_End);
        ls.z_End.last->next = (GT_sLNODE<T> *)&(z_End);
        z_Start.next->last = (GT_sLNODE<T> *)&(ls.z_Start);
        ls.z_Start.next->last = (GT_sLNODE<T> *)&(z_Start);

        GN_Swap(GT_sLNODE_<T>, z_Start, ls.z_Start);
        GN_Swap(GT_sLNODE_<T>, z_End, ls.z_End);
        GN_Swap(unsigned int, zSize, ls.zSize);
        GN_Swap(zAltype, zAlnod, ls.zAlnod);
        /*
		sLIST tmp;
		unsigned int tsize = sizeof(tmp);
		memcpy(&tmp, this, tsize);
		memcpy(this, &ls, tsize);
		memcpy(&ls, &tmp, tsize);*/
    }
    //转移指定链表里的所有数据元素到该链表里(插入进该链表那个位置之前, 指定链表)
    inline void splice(iterator pos, sLIST &ls) {
        if(this != &ls && ls.zSize)
            zSplice(pos, ls, ls.begin(), ls.end(), ls.zSize);
    }
    //转移指定链表里一个数据元素到该链表里(插入进该链表那个位置之前, 指定链表, 指定链表的数据源位置)
    void splice(iterator pos, sLIST &ls, iterator first) {
        if(first != ls.end()) {
            iterator last = first;
            ++last;
            if(this != &ls || (pos != first && pos != last))
                zSplice(pos, ls, first, last, 1);
        }
    }
    //转移指定链表里一段范围的数据元素到该链表里(插入进该链表那个位置之前, 指定链表, 指定链表的范围段的首个位置, 范围段的尾个位置)
    void splice(iterator pos, sLIST &ls, iterator first, iterator last) {
        if(first != last && (this != &ls || pos != last)) {
            unsigned int count = 0;
            if(this == &ls)
                ;
            else if(first == ls.begin() && last == ls.end()) count = ls.zSize;
            else {
                iterator cur = first;
                for(; cur != last; ++cur) ++count; //统计范围段长度
            }
            zSplice(pos, ls, first, last, count);
        }
    }
    //合并两个链表 并使值小的排在前(另一链表) !这个排序不是完全排序 它仅考虑相邻的值
    void merge(sLIST &ls) {
#define GT_TMP_LIST_MERGE(CMP)                                           \
    if(&ls != this) {                                                    \
        iterator first1 = begin(), last1 = end();                        \
        iterator first2 = ls.begin(), last2 = ls.end();                  \
                                                                         \
        while(first1 != last1 && first2 != last2)                        \
            if(CMP) {                                                    \
                iterator last = first2;                                  \
                zSplice(first1, ls, first2, ++last, 1);                  \
                first2 = last;                                           \
            } else ++first1;                                             \
                                                                         \
        if(first2 != last2) zSplice(last1, ls, first2, last2, ls.zSize); \
    }
        GT_TMP_LIST_MERGE((*first2) < (*first1))
    }
    template <class F>
    //合并两个链表 并按指定方法来定那边的值排在前(另一链表, 判定方法) !这个排序不是完全排序 它仅考虑相邻的值
    void merge(sLIST &ls, F cmpfunc) {
        GT_TMP_LIST_MERGE(cmpfunc(*first2, *first1))
    }
    //排序数据
    inline void sort() {
        sort(GN_SortFuncDef<T>);
    }
    template <class F>
    //按指定方法排序数据(方法)
    void sort(F cmpfunc) {
        if(zSize > 1) {
            uint8 buf[131072]; //128KB
            sTBUF_INT tbuf(buf, 131072);
            T **ptrs = (T **)tbuf.GetSpace(sizeof(T *) * zSize);
            get_parray(ptrs);
            GN_Sort2(T *, ptrs, zSize, cmpfunc, *); //排序指向值的指针

            T **ptr_end = ptrs + zSize;
            unsigned int node_shell_size = ((uint8 *)&(((GT_sLNODE<T> *)buf)->em) - buf);
            GT_sLNODE<T> *cur1 = (GT_sLNODE<T> *)(((uint8 *)(*ptrs)) - node_shell_size);
            GT_sLNODE<T> *cur;
            cur1->last = (GT_sLNODE<T> *)&z_Start;
            z_Start.next = cur1;
            for(++ptrs; ptrs < ptr_end; ++ptrs) { //重链路各个节点
                cur = (GT_sLNODE<T> *)(((uint8 *)(*ptrs)) - node_shell_size);
                cur1->next = cur;
                cur->last = cur1;
                cur1 = cur;
            }
            cur1->next = (GT_sLNODE<T> *)&z_End;
            z_End.last = cur1;
        }
    }
    //获得链表首个数据元素
    inline T &front() { return z_Start.next->em; }
    //获得链表首个数据元素
    inline T &front() const { return z_Start.next->em; }

    //获得链表尾个数据元素
    inline T &back() { return z_End.last->em; }
    //获得链表尾个数据元素
    inline T &back() const { return z_End.last->em; }

    //连表是否空的
    inline bool empty() const { return (zSize == 0); }

    //获得连表数据元素个数
    inline unsigned int size() const { return zSize; }

    //获得链表能容纳数据的最大个数
    inline unsigned int max_size() const { return zAlnod.max_size(); }
#endif
    //以下是引擎扩展的一些定义 --------------------------------------------------

    //添加一个数据元素( 数据元素 )返回该元素被添加后的位置指针
    inline T *add(const T &v) {
        this->push_back(v);
#ifdef GT_US_STDLIST
        return &(*(--(this->end())));
#else
        return &(z_End.last->em);
#endif
    }
    //在指定位置添加一个数据元素(指定位置, 数据元素)返回该元素被添加后的位置指针
    inline T *add(iterator _it, const T &v) {
        return &(*(this->insert(_it, v)));
    }
    //添加数组里的所有元素(数组, 数组元素个数)
    inline void add(const T *data, unsigned int num) {
        unsigned int i = 0;
        for(; i < num; ++i) this->push_back(data[i]);
    }
    //在该连表首部添加一个数据元素(数据元素)返回该元素被添加后的位置指针
    inline T *add_front(const T &v) {
        this->push_front(v);
#ifdef GT_US_STDLIST
        return &(*(this->begin()));
#else
        return &(z_Start.next->em);
#endif
    }
    //添加一个在连表里没有的数据元素(数据元素) 如果成功添加返回该元素被添加后的位置指针 不成功返回0
    inline T *add_no_eq(const T &v) {
        if(this->size()) {
            iterator _et = this->end();
            for(iterator _it = this->begin(); _it != _et; ++_it) {
                if((*_it) == v) return 0;
            }
        }
        this->push_back(v);
#ifdef GT_US_STDLIST
        return &(*(--(this->end())));
#else
        return &(z_End.last->em);
#endif
    }
    //添加一个在连表里没有的数据元素(数据元素) 返回添加后元素在连表里的序号 如连表已有该数据元素值则返回这个值的序号
    inline unsigned int add_no_eq2(const T &v) {
        unsigned int id = 0, ln = this->size();
        if(ln) {
            for(iterator _it = this->begin(); id < ln; ++id, ++_it) {
                if((*_it) == v) return id;
            }
        }
        this->push_back(v);
        return id;
    }
    //添加数组里在连表里没有的数据元素(数组, 数组元素个数)
    inline void add_no_eq(const T *data, unsigned int num) {
        unsigned int i = 0;
        for(; i < num; ++i) {
            if(this->size()) {
                iterator _et = this->end();
                for(iterator _it = this->begin(); _it != _et;) {
                    if((*_it) == data[i]) {
                        ++i;
                        if(i == num) return;
                        _it = this->begin();
                    } else ++_it;
                }
            }
            this->push_back(data[i]);
        }
    }
    //在连表中删除(指定位置的数据元素)
    inline void del(T *p_em) {
        if(!p_em) {
#ifdef GT_PMEM_WORNING
            GN_Log("连表删除的数据元素 无效!!\n");
#endif
            return;
        }
        this->erase(to_iterator(p_em));
    }
    //把指定数据元素位置转回历遍迭代器(数据元素位置)
    inline iterator to_iterator(T *p_em) {
#ifdef GT_US_STDLIST
#define GT_TMP_AT , A
        GT_ReListIterator(T, GT_TMP_AT, (*this), p_em)
#else
        uint8 buf[32];
        return iterator((GT_sLNODE<T> *)((uint8 *)p_em - ((uint8 *)&(((GT_sLNODE<T> *)buf)->em) - buf)));
#endif
    }
    //查找指定数据元素的位置指针(数据元素)返回该数据元素的位置指针 返回0表示没找到
    inline T *search(const T &v) {
        iterator _et = this->end();
        for(iterator _it = this->begin(); _it != _et; ++_it)
            if((*_it) == v) return &(*_it);
        return 0;
    }
    //查找指定位置序号的数据元素位置指针(位置序号)返回该数据元素的位置指针 返回0表示没找到
    inline T *search(unsigned int id) {
        if(id >= this->size()) return 0;
        iterator _it = this->begin();
        unsigned int i_ = 0;
        for(; i_ < id; ++i_) ++_it;
        return &(*_it);
    }
    //查找指定数据元素的位置序号(数据元素)返回该数据元素的位置序号 返回0xffffffff表示没找到
    inline unsigned int search_id(const T &v) {
        unsigned int id = 0, num = this->size();
        for(iterator _it = this->begin(); id < num; ++id, ++_it)
            if((*_it) == v) return id;
        return id;
    }
    //按条件查找得到一个数据元素的指针( 返回是否 比较条件函数(告诉你当前检索到的元素) )返回满足条件的一个元素的指针 返回0表示没找到
    inline T *search_if(bool (*compare)(T &cur)) {
        iterator _et = this->end();
        for(iterator _it = this->begin(); _it != _et; ++_it)
            if(compare((*_it))) return &(*_it);
        return 0;
    }
    //使连表里的所有数据元素都执行指定函数( 返回是否需继续查找 执行函数(告诉你当前检索到的元素) ) 返回查找中途是否有中断
    inline bool search_exec(bool (*proc)(T &cur)) {
        iterator _et = this->end();
        for(iterator _it = this->begin(); _it != _et; ++_it) {
            if(!proc((*_it))) return true;
        }
        return false;
    }
    //删除连表中全部相同值的数据元素  系统默认的unique只删除隔离值相同的数据元素
    inline void unique_all() {
#define GT_LIST_unique_all(CMP)                                \
    if(2 <= this->size()) {                                    \
        iterator _it = this->begin(), _et = this->end(), _cur; \
        for(; _it != _et; ++_it) {                             \
            _cur = _it;                                        \
            ++_cur;                                            \
            for(; _cur != _et;) {                              \
                if(CMP) _cur = this->erase(_cur);              \
                else ++_cur;                                   \
            }                                                  \
        }                                                      \
    }

        GT_LIST_unique_all(*_it == *_cur)
    }
    //按指定比较条件来删除连表中全部值相同的数据元素(要求返回是否要删除 比较条件(告诉你被比较的数据元素, 告诉你正在比较的数据元素)要求返回是否删除正在比较的元素)  系统默认的unique只删除隔离值相同的数据元素
    inline void unique_all(bool (*compaer)(T &left, T &right)) {
        GT_LIST_unique_all(compaer(*_it, *_cur))
    }
    //生成一个和连表数据一一对应的数组() 返回数组指针 如果连表里没有数据返回0 这会帮您自动分配好数据空间 用完记得 delete[] 释放指针内容
    inline T *to_array() {
        unsigned int n = this->size();
        if(!n) return 0;
        T *data = new T[n];
        iterator _it = this->begin(), _et = this->end();
        for(n = 0; _it != _et; ++_it) data[n++] = (*_it);
        return data;
    }
    //获得连表的所有数据并装填到指定缓冲里(缓冲 需预先分配足够空间)
    inline void get_array(T *data) {
        if(this->empty()) return;
        unsigned int i = 0;
        iterator _it = this->begin(), _et = this->end();
        for(; _it != _et; ++_it, ++i) data[i] = (*_it);
    }
    //获得连表的所有数据指针并装填到指定缓冲里(缓冲 需预先分配足够空间) !如果数据元素为大型结构或类对象 这个更有效率
    inline void get_parray(T **ptr_data) {
#ifdef GT_US_STDLIST

#else
        if(zSize) {
            GT_sLNODE<T> *cur = z_Start.next, *_end1 = z_End.last;
            for(;; cur = cur->next, ++ptr_data) {
                *ptr_data = &(cur->em);
                if(cur == _end1) break;
            }
        }
#endif
    }
    //移动数据元素位置顺序(数据元素位置的指针, 向后移动次数 负值就向前移动)返回实际移动了的次数 当移到最前或最后时会提前返回
    int order(T **pp_em, int fn) {
        int i = 0;
        iterator ito = to_iterator(*pp_em), eto;
        if(fn > 0) {
            iterator ito1 = ito;
            ++ito1;
            eto = this->end();
            if(ito1 == eto) return 0;

        } else {
            eto = this->begin();
            if(ito == eto) return 0;
        }
        T tv = (*(*pp_em));
        ito = this->erase(ito);
        if(fn > 0) {
            for(; i != fn && ito != eto; ++i) ++ito;
        } else {
            for(; i != fn && ito != eto; --i) --ito;
        }
        *pp_em = this->add(ito, tv);
        return i;
    }
};
template <class T, class T2>
//转移源连表的所有元素到目标连表里(目标连表, 源连表)
void ListSpliceList(sLIST<T> &tg, sLIST<T2> &src) {
    GT_LIST_iterator(T2) _it = src.begin(), _et = src.end();
    for(; _it != _et; ++_it) tg.push_back(*_it);
}
template <class T, class ITER>
//获得连表指定历遍迭代器的下个位置迭代器(连表, 指定迭代器)返回下个位置迭代器 !当迭代器已到结尾时就轮回到开始
ITER *ListGetNextIter(T &list, ITER *iter) {
    if(list.size()) {
        if(!iter) {
            iter = new ITER;
            *iter = list.begin();
        } else {
            if(*iter == list.end()) *iter = list.begin();
            else {
                ++(*iter);
                if(*iter == list.end()) *iter = list.begin();
            }
        }
    } else {
        GN_Delete(iter);
    }
    return iter;
}
/* ------------------------------------------------------ sLIST_WHEEL --------------------------------------------------------------------- */
template <class T, class T2>
class sLIST_WHEEL //★列表轮 首尾相连的数据结构  !用固定的容器空间轮寻来模拟连表操作 优点:与普通数组操作效率相当 缺点:没真正连表的动态增长删减
{
  protected:
    T *data; //内部数据
    T2 end_id; //内部数据最大索引ID
    T2 fid; //前向存取位置
    T2 bid; //后向存取位置

    static T2 sid; //起点位置
    static void zComputeSP(); //计算起点位置 为该类型最大值的一半

    virtual void zInit() {
        if(!sid) zComputeSP();
    }

  public:
    inline T *zpElement(T2 id) { return data + (id & end_id); } //获得指定存取位置的数据元素指针
    inline T &zElement(T2 id) { return data[id & end_id]; } //获得指定存取位置的数据元素

    inline sLIST_WHEEL() : data(0) { this->reset(); }
    inline sLIST_WHEEL(T2 _len) : data(0) { this->New(_len); } //分配数据(预分配多少个数据空间 为提高处理速度实际分配可能大于该数)
    inline sLIST_WHEEL(T2 _len, T2 _num, T &em_all) : data(0) { this->New(_len, _num, em_all); } //分配数据并设置一些初值(预分配多少个数据空间 为提高处理速度实际分配可能大于该数, 从首部起设置多少个初值,  默认初始值)
    inline sLIST_WHEEL(T2 _len, T2 _num, T *p_data) : data(0) { this->New(_len, _num, p_data); } //分配数据并从复制普通指针数据来设置一段初始值( 预分配多少个数据空间 为提高处理速度实际分配可能大于该数, 设置数据个数, 普通指针)
    inline sLIST_WHEEL(T2 _len, GN::sLIST<T> &list) : data(0) { this->New(_len, list); } //分配数据并从复制连表里的数据来设置一段初始值( 预分配多少个数据空间 为提高处理速度实际分配可能大于该数, 连表 )
    inline ~sLIST_WHEEL() { this->Delete(); }

    void Delete(); //删除数据

    //分配数据(预分配多少个数据空间 为提高处理速度实际分配可能大于该数)
    void New(T2 _len);

    //分配数据并设置一些初值(预分配多少个数据空间 为提高处理速度实际分配可能大于该数, 从首部起设置多少个初值, 默认初始值)
    void New(T2 _len, T2 _num, T &em_all);

    //分配数据并从复制普通指针数据来设置一段初始值( 预分配多少个数据空间 为提高处理速度实际分配可能大于该数, 设置数据个数, 普通指针)
    void New(T2 _len, T2 _num, T *p_data);

    //分配数据并从复制连表里的数据来设置一段初始值( 预分配多少个数据空间 为提高处理速度实际分配可能大于该数, 连表 )
    void New(T2 _len, GN::sLIST<T> &list);

    T2 BufNum(); //获得数据空间个数
    T2 BufSize(); //获得数据空间所占大小字节
    bool Has(); //是否有实存数据
    bool empty(); //是否无实存数据
    T2 size(); //获得实存数据个数
    T2 Count(); //获得实存数据个数
    T2 GetArrays(T *a[2][2]); //获得内部实存数据段的首尾指针(返回数据段的首尾指针) 返回共有多少段数据 !调用这后您就可以像普通数组那样操作里面的数据了 这个的用法您可以参考 Search 的实现
    T *ToArray(T2 &n); //生成一个数据一一对应的数组(返回数据个数) 返回数组指针 返回0表示内部没有数据 这会帮您自动分配好空间 用完记得释放指针内容
    T *Search(const T &v); //查找指定数据元素的位置指针 返回该数据元素的位置指针 返回0表示没找到
    T *Search(const T &v, T2 &idout); //查找指定数据元素的位置指针(元素值, 返回该数据元素的位置ID)返回该数据元素的位置指针 返回0表示没找到

    T *Begin(); //快速直接获得开始端数据指针 不作范围安全检测
    T *End(); //快速直接获得结尾端数据指针 不作范围安全检测
    T *RBegin(); //快速直接获得逆向开始端数据指针 不作范围安全检测
    T *REnd(); //快速直接获得逆向结尾端数据指针 不作范围安全检测

    bool Front(T &v); //获得首个数据( 值输出 ) 返回是否成功
    bool Back(T &v); //获得尾个数据( 值输出 ) 返回是否成功
    T &front(); //快速直接获得首个数据 不作范围安全检测
    T &back(); //快速直接获得尾个数据 不作范围安全检测

    void reset(); //重设数据存取位置为初始状态, 以便覆盖原有数据  并保留数据空间

    bool can_push(); //检测是否还可以存入数据

    bool push_front(const T &v); //在首部存入一个数据( 值 ) 返回是否成功
    bool push_back(const T &v); //在尾部存入一个数据( 值 ) 返回是否成功
    bool pop_front(T &v); //在首部取出一个数据( 值输出 ) 返回是否成功
    bool pop_back(T &v); //在尾部取出一个数据( 值输出 ) 返回是否成功
    T *pop_begin(); //在首部取出一个数据 成功返回它的指针 失败返回0
    T *pop_rbegin(); //在尾部取出一个数据 成功返回它的指针 失败返回0

    void pop_front__(T &v); //在首部直接盲取一个数据( 值输出 ) 不作范围安全检测
    void pop_back__(T &v); //在尾部直接盲取一个数据( 值输出 ) 不作范围安全检测
    void discd_front(); //快速直接丢弃首个数据 不作范围安全检测
    void discd_back(); //快速直接丢弃尾个数据 不作范围安全检测
    void discd_front(T2 c); //快速直接丢弃首部几个数据(丢弃个数) 不作范围安全检测
    void discd_back(T2 c); //快速直接丢弃尾部几个数据(丢弃个数) 不作范围安全检测

    GT_TMP_ITERATOR_OPT_DEF(, sLIST_WHEEL, fid, bid, bid - 1, fid - 1)

    //快速历遍查找列表轮里的所有实存数据开始( 数据类型 , 计数类型 )  得到 (*p) 为当前数据元素
#define GN_LIST_WHEEL_SEARCH_BEGIN(T, T2) \
    {                                     \
        T *as[2][2], *p;                  \
        T2 i = 0, n_arr = GetArrays(as);  \
        for(; i < n_arr; ++i)             \
            for(p = as[i][0]; p < as[i][1]; ++p) {

    //快速历遍查找列表轮里的所有实存数据结束
#define GN_LIST_WHEEL_SEARCH_END \
    }                            \
    }
};
template <class T, class T2>
T2 sLIST_WHEEL<T, T2>::sid = 0;

template <class T, class T2>
void sLIST_WHEEL<T, T2>::zComputeSP() {
    T2 sp = 1;
    T2 i = 1, n = (T2)sizeof(T2);
    for(; i < n; ++i) sp *= 256;
    sid = sp * 128;
}
template <class T, class T2>
void sLIST_WHEEL<T, T2>::Delete() {
    GN_DeleteArray(data);
    reset();
}

template <class T, class T2>
void sLIST_WHEEL<T, T2>::New(T2 _len) {
#define GT_TMP_LIST_WHEEL_NEW \
    this->Delete();           \
    zInit();                  \
    _len = GN_NumGet2n(_len); \
    data = new T[_len];       \
    end_id = _len - 1

#define GT_TMP_LIST_WHEEL_SET_RANGE \
    if(_num >= _len) _num = _len;   \
    fid = sid;                      \
    bid = sid + _num;

    GT_TMP_LIST_WHEEL_NEW;
    this->reset();
}
template <class T, class T2>
void sLIST_WHEEL<T, T2>::New(T2 _len, T2 _num, T &em_all) {
    GT_TMP_LIST_WHEEL_NEW;
    GT_TMP_LIST_WHEEL_SET_RANGE
    GN_SetArray(data, _num, em_all);
}
template <class T, class T2>
void sLIST_WHEEL<T, T2>::New(T2 _len, T2 _num, T *p_data) {
    GT_TMP_LIST_WHEEL_NEW;
    GT_TMP_LIST_WHEEL_SET_RANGE
    GN_SetArrayFromArray(data, p_data, _num);
}
template <class T, class T2>
void sLIST_WHEEL<T, T2>::New(T2 _len, GN::sLIST<T> &list) {
    T2 _num = (T2)list.size();
    GT_TMP_LIST_WHEEL_NEW;
    GT_TMP_LIST_WHEEL_SET_RANGE
    GT_LIST_iterator(T) it_t = list.begin();
    T2 i = 0;
    for(; i < _num; ++i, ++it_t) data[i] = (*it_t);
}
template <class T, class T2>
T2 sLIST_WHEEL<T, T2>::BufNum() {
    return data ? end_id + 1 : 0;
}

template <class T, class T2>
T2 sLIST_WHEEL<T, T2>::BufSize() {
    return data ? (end_id + 1) * sizeof(T) : 0;
}

template <class T, class T2>
bool sLIST_WHEEL<T, T2>::Has() {
    return fid < bid;
}

template <class T, class T2>
bool sLIST_WHEEL<T, T2>::empty() {
    return fid >= bid;
}

template <class T, class T2>
T2 sLIST_WHEEL<T, T2>::size() {
    return fid < bid ? bid - fid : 0;
}

template <class T, class T2>
T2 sLIST_WHEEL<T, T2>::Count() {
    return bid - fid;
}

template <class T, class T2>
T2 sLIST_WHEEL<T, T2>::GetArrays(T *a[2][2]) {
    if(bid > fid) {
        T2 _fid = (fid & end_id), _bid = (bid & end_id);
        if(_fid < _bid) {
            a[0][0] = data + _fid;
            a[0][1] = data + _bid;
            return 1;
        } else {
            a[0][0] = data + _fid;
            a[0][1] = data + (end_id + 1);
            if(_bid) {
                a[1][0] = data;
                a[1][1] = data + _bid;
                return 2;
            }
            return 1;
        }
    }
    return 0;
}
template <class T, class T2>
T *sLIST_WHEEL<T, T2>::ToArray(T2 &n) {
    if(bid > fid) {
        T *r_data = new T[bid - fid];
        T2 j = 0;
        GN_LIST_WHEEL_SEARCH_BEGIN(T, T2)
        r_data[j++] = *p;
        GN_LIST_WHEEL_SEARCH_END
        return r_data;
    }
    return 0;
}
template <class T, class T2>
T *sLIST_WHEEL<T, T2>::Search(const T &v) {
    GN_LIST_WHEEL_SEARCH_BEGIN(T, T2)
    if((*p) == v) return p;
    GN_LIST_WHEEL_SEARCH_END
    return 0;
}
template <class T, class T2>
T *sLIST_WHEEL<T, T2>::Search(const T &v, T2 &idout) {
    T2 did = 0;
    GN_LIST_WHEEL_SEARCH_BEGIN(T, T2)
    if((*p) == v) {
        idout = did;
        return p;
    }
    ++did;
    GN_LIST_WHEEL_SEARCH_END
    return 0;
}
template <class T, class T2>
T *sLIST_WHEEL<T, T2>::Begin() {
    return data + (fid & end_id);
}

template <class T, class T2>
T *sLIST_WHEEL<T, T2>::End() {
    return data + (bid & end_id);
}

template <class T, class T2>
T *sLIST_WHEEL<T, T2>::RBegin() {
    return data + ((bid - 1) & end_id);
}

template <class T, class T2>
T *sLIST_WHEEL<T, T2>::REnd() {
    return data + ((fid - 1) & end_id);
}

template <class T, class T2>
bool sLIST_WHEEL<T, T2>::Front(T &v) {
    if(bid > fid) {
        v = data[fid & end_id];
        return true;
    } else return false;
}
template <class T, class T2>
bool sLIST_WHEEL<T, T2>::Back(T &v) {
    if(bid > fid) {
        v = data[(bid - 1) & end_id];
        return true;
    } else return false;
}
template <class T, class T2>
T &sLIST_WHEEL<T, T2>::front() {
    return data[fid & end_id];
}

template <class T, class T2>
T &sLIST_WHEEL<T, T2>::back() {
    return data[(bid - 1) & end_id];
}

template <class T, class T2>
void sLIST_WHEEL<T, T2>::reset() {
    bid = sid;
    fid = bid;
}

template <class T, class T2>
bool sLIST_WHEEL<T, T2>::can_push() {
    return (bid <= fid + end_id);
}
template <class T, class T2>
bool sLIST_WHEEL<T, T2>::push_front(const T &v) {
    if(bid > fid + end_id) return false;
    T2 _fid = (bid < fid ? bid : fid) - 1;
    data[_fid & end_id] = v;
    fid = _fid;
    return true;
}
template <class T, class T2>
bool sLIST_WHEEL<T, T2>::push_back(const T &v) {
    if(bid > fid + end_id) return false;
    if(bid < fid) bid = fid;
    data[bid & end_id] = v;
    ++bid;
    return true;
}
template <class T, class T2>
bool sLIST_WHEEL<T, T2>::pop_front(T &v) {
    if(bid > fid) {
        ++fid;
        v = data[(fid - 1) & end_id];
        return true;
    } else return false;
}
template <class T, class T2>
bool sLIST_WHEEL<T, T2>::pop_back(T &v) {
    if(bid > fid) {
        --bid;
        v = data[bid & end_id];
        return true;
    } else return false;
}
template <class T, class T2>
T *sLIST_WHEEL<T, T2>::pop_begin() {
    if(bid > fid) {
        ++fid;
        return data + ((fid - 1) & end_id);
    } else return 0;
}
template <class T, class T2>
T *sLIST_WHEEL<T, T2>::pop_rbegin() {
    if(bid > fid) {
        --bid;
        return data + (bid & end_id);
    } else return 0;
}
template <class T, class T2>
void sLIST_WHEEL<T, T2>::pop_front__(T &v) {
    ++fid;
    v = data[(fid - 1) & end_id];
}
template <class T, class T2>
void sLIST_WHEEL<T, T2>::pop_back__(T &v) {
    --bid;
    v = data[bid & end_id];
}
template <class T, class T2>
void sLIST_WHEEL<T, T2>::discd_front() {
    ++fid;
}

template <class T, class T2>
void sLIST_WHEEL<T, T2>::discd_back() {
    --bid;
}

template <class T, class T2>
void sLIST_WHEEL<T, T2>::discd_front(T2 c) {
    fid += c;
}

template <class T, class T2>
void sLIST_WHEEL<T, T2>::discd_back(T2 c) {
    bid -= c;
}
}; //end namespace GN

#endif // GN_CONTAINERS_LIST_H
