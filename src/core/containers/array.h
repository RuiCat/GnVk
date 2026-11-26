// GN_mini 容器拆分片段：GN::sARRAY 家族（sARRAY/POSE/BUF/VEC）。
// 2026-11-26 自 GN_Array.h 第 791-1051 行 move-only 拆出（REFACTOR_PLAN §13 B3）。
// 由 GN_Array.h 按原顺序转发包含；单独包含需自行补齐前置依赖。
#ifndef GN_CONTAINERS_ARRAY_H
#define GN_CONTAINERS_ARRAY_H

// 自包含依赖（B3 拆分时补齐；正文仍为 move-only）
#include "baseTypes.h"
#include "iterator.h"

namespace GN {
/*------------------------------------------------------ sARRAY -------------------------------------------------------------------*/
template <class T, class T2 = unsigned int>
struct sARRAY //通用简单的数组
{
    T *data; //数据指针
    T2 num; //数据数
};
template <class T, class T2 = unsigned int>
struct sARRAY_POSE : public sARRAY<T, T2> //通用简单的数组与处理点结构
{
    T2 pos; //当前位置
};
template <class T, class T2 = unsigned int>
struct sARRAY_BUF : public sARRAY<T, T2> //数组缓冲 可不断随存储增长的数组
{
    T2 buf_len; //数据空间可容纳数据个数

    inline sARRAY_BUF() : buf_len(0) {
        this->data = 0;
        this->num = 0;
    }
    inline sARRAY_BUF(T2 _len) {
        this->data = 0;
        this->New(_len);
    } //分配数据(预分配多少个数据空间)
    inline ~sARRAY_BUF() { this->Delete(); }

    //删除数据
    void Delete() {
        GN_DeleteArray(this->data);
        this->buf_len = 0;
        this->num = 0;
    }
    //分配数据(预分配多少个数据空间)
    void New(T2 _len) {
#define GT_TMP_ARRAY_VEC_NEW  \
    this->Delete();           \
    this->data = new T[_len]; \
    this->buf_len = _len

        GT_TMP_ARRAY_VEC_NEW;
        this->num = 0;
    }
    //在缓冲末端存入一组数据(数据, 数据数)
    void Push(T *_array, T2 _num);

    T2 BufSize() const { return sizeof(T) * buf_len; } //获得数据空间所占大小字节
    T2 Count() const { return this->num; } //获得实存数据个数
};
template <class T, class T2>
void sARRAY_BUF<T, T2>::Push(T *_array, T2 _num) {
    if(this->num + _num > buf_len) {
        buf_len = this->num + _num;
        buf_len += buf_len >> 1;
        T *data1 = new T[buf_len];
        memcpy(data1, this->data, sizeof(T) * this->num);
        delete[] this->data;
        this->data = data1;
    }
    memcpy(this->data + this->num, _array, sizeof(T) * _num);
    this->num += _num;
}
template <class T, class T2 = unsigned int>
struct sARRAY_VEC : public sARRAY<T, T2> //用普通数组来模拟向量容器 std::vector 并且数据空间会自动增长
{
    T2 buf_len; //数据空间可容纳数据个数
    void (*on_del_proc)(T *data, T2 num); //当内部数据要清理前的处理

#define GT_TMP_ARRAY_ELEMENT_GET_DEF     \
    inline T *zpElement(T2 id) {         \
        return this->data + id;          \
    } /*获得指定存取位置的数据元素指针*/ \
    inline T &zElement(T2 id) {          \
        return this->data[id];           \
    } /*获得指定存取位置的数据元素*/

    GT_TMP_ARRAY_ELEMENT_GET_DEF

    inline sARRAY_VEC() : buf_len(0), on_del_proc(0) {
        this->data = 0;
        this->num = 0;
    }
    inline sARRAY_VEC(T2 _len) : on_del_proc(0) {
        this->data = 0;
        this->New(_len);
    } //分配数据(预分配多少个数据空间)
    inline sARRAY_VEC(T2 _len, T2 _num, T &em_all) : on_del_proc(0) {
        this->data = 0;
        this->New(_len, _num, em_all);
    } //分配数据并设置一些初值(预分配多少个数据空间, 从首部起设置多少个初值,  默认初始值)
    inline sARRAY_VEC(T2 _len, T2 _num, T *p_data) : on_del_proc(0) {
        this->data = 0;
        this->New(_len, _num, p_data);
    } //分配数据并从复制普通指针数据来设置一段初始值( 预分配多少个数据空间, 设置数据个数, 普通指针)
    //	inline  sARRAY_VEC(T2 _len, sLIST< T > &list):on_del_proc(0){ data = 0; this->New(_len, list); }			//分配数据并从复制连表里的数据来设置一段初始值( 预分配多少个数据空间, 连表 )
    inline ~sARRAY_VEC() { this->Delete(); }

    void Delete(); //删除数据

    //分配数据(预分配多少个数据空间)
    void New(T2 _len);

    //分配数据并设置一些初值(预分配多少个数据空间, 从首部起设置多少个初值,  默认初始值)
    void New(T2 _len, T2 _num, T &em_all);

    //分配数据并从复制普通指针数据来设置一段初始值( 预分配多少个数据空间, 设置数据个数, 普通指针)
    void New(T2 _len, T2 _num, T *p_data);

    //分配数据并从复制连表里的数据来设置一段初始值( 预分配多少个数据空间, 连表 )
    //	void New(T2 _len, GN::sLIST< T > &list);

    T2 BufNum() const { return buf_len; } //获得数据空间个数
    T2 BufSize() const; //获得数据空间所占大小字节
    bool Has() const { return this->num ? 1 : 0; } //是否有实存数据
    bool empty() const { return this->num == 0; } //是否无实存数据
    T2 size() const { return this->num; } //获得实存数据个数
    T2 Count() const { return this->num; } //获得实存数据个数

    void reserve(T2 _len); //预留新的数据空间 原有数据保持不变 新多出的空间数据可能为未知值
    inline void reset() { this->num = 0; } //重设数据存取位置为初始状态, 以便覆盖原有数据 并保留数据空间
    inline void clear() { Delete(); } //删除数据

    T &front() { return *(this->data); } //获得首个数据
    T &back() { return this->data[this->num - 1]; } //获得尾个数据

    void push_back(const T &v); //在尾部存入一个数据( 值 )
    T2 push_back_neq(const T &v); //在尾部存入一个不同于现存数据的值( 值 )返回插入值的位置ID 如已有相同的值将取消存入那么返回首个相同值的位置ID
    void pop_back(T &v) {
        --(this->num);
        v = this->data[this->num];
    } //在尾部取出一个数据( 值输出 )

    T &operator[](T2 id); //安全引用数组元数(索引号)

#define GT_TMP_ITERATOR_OPT_DEF(A, TYPE, B, E, RB, RE)                                         \
    typedef GT_sIterator##A<TYPE<T, T2>, T, T2> iterator; /*定义一个属于该类内的顺向迭代器*/   \
    typedef GT_sReverseIterator<iterator> reverse_iterator; /*定义一个属于该类内的逆向迭代器*/ \
                                                                                               \
    iterator begin() {                                                                         \
        return iterator(this, B);                                                              \
    } /*获得开始端搜索迭代器*/                                                                 \
    iterator end() {                                                                           \
        return iterator(this, E);                                                              \
    } /*获得结尾端搜索迭代器*/                                                                 \
    iterator rbegin() {                                                                        \
        return iterator(this, RB);                                                             \
    } /*获得逆向开始端搜索迭代器*/                                                             \
    iterator rend() {                                                                          \
        return iterator(this, RE);                                                             \
    } /*获得逆向结尾端搜索迭代器*/
    GT_TMP_ITERATOR_OPT_DEF(, sARRAY_VEC, 0, this->num, this->num - 1, 0 - 1) //定义迭代器操作

    void insert(iterator &pos, const T &v); //在指定位置插入一个值(位置, 值)
    void insert(iterator &pos, T2 count, const T &v); //在指定位置插入多个相同值(位置, 插入个数, 值)
    void insert(iterator &pos, const T *data2, T2 count); //在指定位置插入一段数据(位置, 数据, 插入个数)
};
template <class T, class T2>
void sARRAY_VEC<T, T2>::Delete() {
    if(this->data) {
        if(on_del_proc) on_del_proc(this->data, buf_len);
        delete[](this->data);
        this->data = 0;
    }
    buf_len = 0;
    reset();
}
template <class T, class T2>
void sARRAY_VEC<T, T2>::New(T2 _len) {
    GT_TMP_ARRAY_VEC_NEW;
    this->reset();
}
template <class T, class T2>
void sARRAY_VEC<T, T2>::New(T2 _len, T2 _num, T &em_all) {
    GT_TMP_ARRAY_VEC_NEW;
    this->num = _num;
    GN_SetArray((this->data), _num, em_all);
}
template <class T, class T2>
void sARRAY_VEC<T, T2>::New(T2 _len, T2 _num, T *p_data) {
    GT_TMP_ARRAY_VEC_NEW;
    this->num = _num;
    GN_SetArrayFromArray((this->data), p_data, _num);
}
/*
template<class T, class T2>
void sARRAY_VEC<T, T2>::New(T2 _len, GN::sLIST< T > &list){
    num = (T2)list.size();
	GT_TMP_ARRAY_VEC_NEW;
	GT_LIST_iterator( T ) it_t = list.begin();
	T2 i = 0;
	for(; i < num; ++i, ++it_t)data[i] = (*it_t);
}*/
template <class T, class T2>
T2 sARRAY_VEC<T, T2>::BufSize() const {
    return sizeof(T) * buf_len;
}

template <class T, class T2>
void sARRAY_VEC<T, T2>::reserve(T2 _len) {
    if(_len == buf_len) return;
    T *data2 = new T[_len];
    if(_len < this->num) this->num = _len;
    if(this->data) {
        T2 ci = 0;
        for(; ci < this->num; ++ci) data2[ci] = this->data[ci];
        if(on_del_proc) on_del_proc(this->data, buf_len);
        delete[](this->data);
    }
    this->data = data2;
    buf_len = _len;
}
template <class T, class T2>
void sARRAY_VEC<T, T2>::push_back(const T &v) {
    if(this->num >= buf_len) reserve(this->num < 4 ? 4 : (this->num + (this->num >> 1)));
    this->data[this->num++] = v;
}
template <class T, class T2>
T2 sARRAY_VEC<T, T2>::push_back_neq(const T &v) {
    if(this->num >= buf_len) reserve(this->num < 4 ? 4 : (this->num + (this->num >> 1)));
    T2 i = 0;
    for(; i < this->num; ++i)
        if(v == this->data[i]) return i;

    this->data[this->num++] = v;
    return i;
}
template <class T, class T2>
T &sARRAY_VEC<T, T2>::operator[](T2 id) {
    if(id >= buf_len) reserve(id + 1);
    return this->data[id];
}
template <class T, class T2>
void sARRAY_VEC<T, T2>::insert(iterator &pos, const T &v) {
    if(this->num >= buf_len) reserve(this->num < 4 ? 4 : (this->num + (this->num >> 1)));
    T2 j = this->num;
    ++(this->num);
    T2 id = pos.id;
    for(; j > id; --j) this->data[j] = this->data[j - 1];
    this->data[id] = v;
}
template <class T, class T2>
void sARRAY_VEC<T, T2>::insert(iterator &pos, T2 count, const T &v) {
#define GT_TMP_ARRAY_VEC_INSERT                                              \
    T2 count2 = this->num + count;                                           \
    if(count2 > buf_len) reserve(count2 < 4 ? 4 : (count2 + (count2 >> 1))); \
    this->num += count;                                                      \
    T2 id = pos.id;                                                          \
    T2 j = this->num - 1, id2 = id + count - 1;                              \
    for(; j > id2; --j) this->data[j] = this->data[j - count];

    GT_TMP_ARRAY_VEC_INSERT
    for(; id <= id2; ++id) this->data[id] = v;
}
template <class T, class T2>
void sARRAY_VEC<T, T2>::insert(iterator &pos, const T *data2, T2 count) {
    GT_TMP_ARRAY_VEC_INSERT
    T2 i = 0;
    for(; id <= id2; ++id, ++i) this->data[id] = data2[i];
}
}; // namespace GN

#endif // GN_CONTAINERS_ARRAY_H
