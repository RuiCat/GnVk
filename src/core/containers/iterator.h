// GN_mini 容器拆分片段：全局搜索迭代器 GT_sIterator/GT_sReverseIterator/GT_sIteratorExt。
// 2026-11-26 自 GN_Array.h 第 717-790 行 move-only 拆出（REFACTOR_PLAN §13 B3）。
// 由 GN_Array.h 按原顺序转发包含；单独包含需自行补齐前置依赖。
#ifndef GN_CONTAINERS_ITERATOR_H
#define GN_CONTAINERS_ITERATOR_H

/*  历遍搜索方法 模拟普通连表那样操作 ------------------------------------------------------------------------------------------------ */
template <class TW, class T, class T2>
struct GT_sIterator //顺向搜索迭代器
{
    TW *lw; //指向关联列表轮
    T2 id; //当前位置

    GT_sIterator() {}
    GT_sIterator(TW *_lw, T2 _id) : lw(_lw), id(_id) {}

    inline bool operator==(const GT_sIterator &it) const { return (this->lw == it.lw && this->id == it.id); }
    inline bool operator!=(const GT_sIterator &it) const { return (this->lw != it.lw || this->id != it.id); }
    inline T &operator*() { return lw->zElement(id); }
    inline T *operator->() { return lw->zpElement(id); }

    GT_sIterator &operator++() {
        ++id;
        return (*this);
    } /*模板代码:迭代器增减*/
    GT_sIterator operator++(int) {
        GT_sIterator tmp = (*this);
        ++id;
        return tmp;
    }
    GT_sIterator &operator--() {
        --id;
        return (*this);
    }
    GT_sIterator operator--(int) {
        GT_sIterator tmp = (*this);
        --id;
        return tmp;
    }
};
template <class ITER>
struct GT_sReverseIterator : public ITER //逆向搜索迭代器
{
    ITER &operator++() {
        --(this->id);
        return (*this);
    } /*模板代码:迭代器增减*/
    ITER operator++(int) {
        ITER tmp = (*this);
        --(this->id);
        return tmp;
    }
    ITER &operator--() {
        ++(this->id);
        return (*this);
    }
    ITER operator--(int) {
        ITER tmp = (*this);
        ++(this->id);
        return tmp;
    }
};
template <class TW, class T, class T2>
struct GT_sIteratorExt : public GT_sIterator<TW, T, T2> //顺向搜索迭代器扩展功能
{
    GT_sIteratorExt(TW *_lw) { this->lw = _lw; }

    //下标引用数据
    inline T &operator[](T2 _id) { return this->lw->zElement(this->id + _id); }

    //转换为普通指针 这个一般在参数传递和赋值给普通指针会自动调用 连 if( P ) 也会调用的
    inline operator const T *() const {
        if(this->lw) return (T *)this->lw->zpElement(this->id);
        else return 0;
    }
    inline operator T *() {
        if(this->lw) return (T *)this->lw->zpElement(this->id);
        else return 0;
    }
};

#endif // GN_CONTAINERS_ITERATOR_H
