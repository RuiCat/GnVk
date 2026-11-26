// GN_mini 容器拆分片段：列表轮附加操作 GT_sLIST_WHEEL_AT 等。
// 2026-11-26 自 GN_Array.h 第 2362-2528 行 move-only 拆出（REFACTOR_PLAN §13 B3）。
// 由 GN_Array.h 按原顺序转发包含；单独包含需自行补齐前置依赖。
#ifndef GN_CONTAINERS_LISTWHEEL_H
#define GN_CONTAINERS_LISTWHEEL_H

// 自包含依赖（B3 拆分时补齐；正文仍为 move-only）
#include "baseTypes.h"
#include "list.h"

/* ----------------------------------------------------- GT_sLIST_WHEEL_AT -----------------------------------------------------------------*/
template <class T>
void GT_EmptyFunc(void) {}

template <class T>
void GT_EmptyFunc(T arg) {}

template <class T, class T2>
bool GT_Add1Func(T, void *arg) {
    ++(*(((T2 *)arg)));
    return true;
}

template <class T, class T2>
class GT_sLIST_WHEEL_AT : public GN::sLIST_WHEEL<T, T2> //列表轮 首尾相连的数据结构 附加操作版本
{
  protected:
    virtual void zInit() {
        GN::sLIST_WHEEL<T, T2>::zInit();
        zpAddOperate = GT_EmptyFunc<void>;
    }
    void (*zpAddOperate)(void); //附加动作函数指针
  public:
    void reset(); //重设数据存取位置为初始状态  以便覆盖原有数据  并保留数据空间

    bool push_front(const T &v, unsigned int wt); //允许等待在首部存入一个数据( 值 , 等待时间) 返回是否成功 !由于空间可能会满 就等待能存入时存入
    bool push_back(const T &v, unsigned int wt); //允许等待在尾部存入一个数据( 值 , 等待时间) 返回是否成功 !由于空间可能会满 就等待能存入时存入
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
};
#ifndef _MSC_VER
extern unsigned int GT_RunTotalTimeApp;
void GN_TimeSleep(unsigned int);
#endif
#if !defined _WIN32_ && !defined _WIN32_WCE
char *itoa(int, char *, int);
#endif
template <class T, class T2>
void GT_sLIST_WHEEL_AT<T, T2>::reset() {
    this->bid = this->sid;
    this->fid = this->bid;
    zpAddOperate();
}

template <class T, class T2>
bool GT_sLIST_WHEEL_AT<T, T2>::push_front(const T &v, unsigned int wt) {
    if(push_front(v)) return true;
    unsigned int t = GT_RunTotalTimeApp;
    for(;;) {
        GN_TimeSleep(1);
        if(push_front(v)) return true;
        else if((volatile unsigned int)GT_RunTotalTimeApp - t > wt) return false;
    }
    return false;
}
template <class T, class T2>
bool GT_sLIST_WHEEL_AT<T, T2>::push_back(const T &v, unsigned int wt) {
    if(push_back(v)) return true;
    unsigned int t = GT_RunTotalTimeApp;
    for(;;) {
        GN_TimeSleep(1);
        if(push_back(v)) return true;
        else if((volatile unsigned int)GT_RunTotalTimeApp - t > wt) return false;
    }
    return false;
}
template <class T, class T2>
bool GT_sLIST_WHEEL_AT<T, T2>::push_front(const T &v) {
    if(this->bid > this->fid + this->end_id) return false;
    T2 _fid = (this->bid < this->fid ? this->bid : this->fid) - 1;
    this->data[_fid & this->end_id] = v;
    this->fid = _fid;
    zpAddOperate();
    return true;
}
template <class T, class T2>
bool GT_sLIST_WHEEL_AT<T, T2>::push_back(const T &v) {
    if(this->bid > this->fid + this->end_id) return false;
    if(this->bid < this->fid) this->bid = this->fid;
    this->data[this->bid & this->end_id] = v;
    ++(this->bid);
    zpAddOperate();
    return true;
}
template <class T, class T2>
bool GT_sLIST_WHEEL_AT<T, T2>::pop_front(T &v) {
    if(this->bid > this->fid) {
        ++(this->fid);
        zpAddOperate();
        v = this->data[(this->fid - 1) & this->end_id];
        return true;
    } else return false;
}
template <class T, class T2>
bool GT_sLIST_WHEEL_AT<T, T2>::pop_back(T &v) {
    if(this->bid > this->fid) {
        --(this->bid);
        zpAddOperate();
        v = this->data[this->bid & this->end_id];
        return true;
    } else return false;
}
template <class T, class T2>
T *GT_sLIST_WHEEL_AT<T, T2>::pop_begin() {
    if(this->bid > this->fid) {
        ++(this->fid);
        zpAddOperate();
        return this->data + ((this->fid - 1) & this->end_id);
    } else return 0;
}
template <class T, class T2>
T *GT_sLIST_WHEEL_AT<T, T2>::pop_rbegin() {
    if(this->bid > this->fid) {
        --(this->bid);
        zpAddOperate();
        return this->data + (this->bid & this->end_id);
    } else return 0;
}
template <class T, class T2>
void GT_sLIST_WHEEL_AT<T, T2>::pop_front__(T &v) {
    ++(this->fid);
    zpAddOperate();
    v = this->data[(this->fid - 1) & this->end_id];
}
template <class T, class T2>
void GT_sLIST_WHEEL_AT<T, T2>::pop_back__(T &v) {
    --(this->bid);
    zpAddOperate();
    v = this->data[this->bid & this->end_id];
}
template <class T, class T2>
void GT_sLIST_WHEEL_AT<T, T2>::discd_front() {
    ++(this->fid);
    zpAddOperate();
}

template <class T, class T2>
void GT_sLIST_WHEEL_AT<T, T2>::discd_back() {
    --(this->bid);
    zpAddOperate();
}

template <class T, class T2>
void GT_sLIST_WHEEL_AT<T, T2>::discd_front(T2 c) {
    this->fid += c;
    zpAddOperate();
}

template <class T, class T2>
void GT_sLIST_WHEEL_AT<T, T2>::discd_back(T2 c) {
    this->bid -= c;
    zpAddOperate();
}


#endif // GN_CONTAINERS_LISTWHEEL_H
