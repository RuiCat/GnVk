// GN_mini 容器拆分片段：临时高速缓冲 GT_sTBUF + GN::sTBUF 家族。
// 2026-11-26 自 GN_Array.h 第 2529-2811 行 move-only 拆出（REFACTOR_PLAN §13 B3）。
// 由 GN_Array.h 按原顺序转发包含；单独包含需自行补齐前置依赖。
#ifndef GN_CONTAINERS_TEMPBUF_H
#define GN_CONTAINERS_TEMPBUF_H

// 自包含依赖（B3 拆分时补齐；正文仍为 move-only）
#include "baseTypes.h"
#include "memMacros.h"
#include "list.h"
#include <cstring>
#include "_SysDef/GN_SysDef.h"

/***************************************************************** 内存管理 ************************************************************/
template <class T>
struct GT_sTBUF {
    T _size; //存量字节数
    uint8 *p__; //空间首地址

    void *GetSpace(T _size_); //获取指定大小的数据空间(大小字节) 返回数据空间指针 失败返回0
};
template <class T>
void *GT_sTBUF<T>::GetSpace(T _size_) {
    if(_size_ > _size) return 0;
    else {
        _size -= _size_;
        return p__ + _size;
    }
}
namespace GN {
template <class T>
struct sTBUF : public GT_sTBUF<T> //★局部临时高速处理缓冲 适用于在一个功能函数处理时需要临时分配大量(不定量)的临时空间时用 !不适合比较看重构造函数分配的对象数据
{
    T size; //缓冲大小
    GN::sLIST<GT_sTBUF<T>> add_bufs; //在局部缓冲不够用时来分配的堆缓冲

    sTBUF() : size(131072 /*128k*/) {
        this->p__ = 0;
        this->_size = 0;
    }
    sTBUF(uint8 *loc_buf, T buf_size); //创建(指定一个局部缓冲 0为不指定那么之后所获取的空间总是在内存堆上的, 缓冲大小字节) 局部缓冲例如 uint8 loc_buf[100000]; !局部缓冲的大小您应该适当设为稍大于当前需要处理数据的评估最大临时空间, 如果实际使用大于这个设定大小 系统会自动new出适当的其它空间, 但使用到new出来的空间会比较慢
    ~sTBUF() { this->Clear(); }

    void Clear(); //清除一下额外增长多分配出来的内存空间
    void *GetSpace(T _size_); //获取指定大小的数据空间(大小字节) 返回数据空间指针
};
template <class T>
sTBUF<T>::sTBUF(uint8 *loc_buf, T buf_size) {
#define GT_TMP_INIT_TBUF(BUF) \
    this->p__ = BUF;          \
    size = buf_size;          \
    this->_size = BUF ? buf_size : 0;

    GT_TMP_INIT_TBUF(loc_buf)
}
template <class T>
void sTBUF<T>::Clear() {
    if(add_bufs.size()) {
        GT_LIST_iterator(GT_sTBUF<T>) it_b = add_bufs.begin(), et_b = add_bufs.end();
        for(; it_b != et_b; ++it_b) free(it_b->p__);
        add_bufs.clear();
    }
}
template <class T>
void *sTBUF<T>::GetSpace(T _size_) {
    T _size4 = _size_ & (~3);
    if(_size_ > _size4) _size_ = _size4 + 4;
    void *rp = GT_sTBUF<T>::GetSpace(_size_);
    if(rp) return rp;
    if(add_bufs.size()) {
        GT_LIST_iterator(GT_sTBUF<T>) it_b = add_bufs.begin(), et_b = add_bufs.end();
        for(; it_b != et_b; ++it_b) {
            rp = it_b->GetSpace(_size_);
            if(rp) return rp;
        }
    }
    GT_sTBUF<T> *tbf = add_bufs.add(GT_sTBUF<T>());
    if(_size_ > size) {
        tbf->p__ = (uint8 *)malloc(_size_);
        tbf->_size = -_size_;
        if(tbf->_size > 0) tbf->_size = 0;
        return tbf->p__;
    } else {
        tbf->p__ = (uint8 *)malloc(size);
        tbf->_size = size - _size_;
        return tbf->p__ + tbf->_size;
    }
}
typedef sTBUF<int> sTBUF_INT; //定义整形计数的局部缓冲类型

struct GN_API sTBUF_MT : public sTBUF<unsigned int> //★局部临时高速处理缓冲 能在各线程了安全分配空间 适用于在一个功能函数处理时需要临时分配大量(不定量)的临时空间时用 !不适合比较看重构造函数分配的对象数据
{
    GT_sATOMIC is_busy; //标记是否正在忙

    sTBUF_MT() : is_busy(0) {}
    sTBUF_MT(uint8 *loc_buf, unsigned int buf_size) : sTBUF<unsigned int>(loc_buf, buf_size), is_busy(0) {} //创建(指定一个局部缓冲 0为不指定那么之后所获取的空间总是在内存堆上的, 缓冲大小字节) 局部缓冲例如 uint8 loc_buf[100000]; !局部缓冲的大小您应该适当设为稍大于当前需要处理数据的评估最大临时空间, 如果实际使用大于这个设定大小 系统会自动new出适当的其它空间, 但使用到new出来的空间会比较慢
    ~sTBUF_MT() { this->Clear(); }

    long IsBusy(); //检测是否正在忙
    void Clear(); //清除一下额外增长多分配出来的内存空间
    void *GetSpace(unsigned int _size_); //获取指定大小的数据空间(大小字节) 返回数据空间指针
};
struct GN_API sTBUF_STATE //★高速处理缓冲使用状态
{
    GT_sTBUF<int> *alloc_buf; //记录当分配区块
    int m_size; //记录当时可用空间大小
};
struct GN_API sTBUF_MAG : public sTBUF_INT //★高速处理缓冲管理器 适用于处理需要临时分配大量(不定量)的临时空间时用 这个比内存池更有效率 !但不适合比较看重构造函数分配的对象数据 !这个不局限于在一个功能函数内 配合 sTBUF_STATE 和 SaveUsState 可以快速腾出不再需要的空间给下个处理反复用
{
    GN::sLIST<unsigned int> buf_idle_times; //各个附加缓冲的空闲时间
    int last_reset_max_bid; //记录上次重设或恢复空间用量状况时最大使用到的附加缓冲ID号
    unsigned int remove_time; //移除空闲附加缓冲超时时间

    sTBUF_MAG() {}
    //构造  (借用共享缓冲 0为不指定那么之后所获取的空间总是在内存堆上的, 缓冲大小字节, 使用到内存堆上的空闲空间移除超时时间) !该管理器不负责借用的共享缓冲的空间创建和删除
    sTBUF_MAG(uint8 *share_buf, int buf_size, unsigned int out_time) : sTBUF_INT(share_buf, buf_size), remove_time(out_time) {}

    //初始化(借用共享缓冲 0为不指定那么之后所获取的空间总是在内存堆上的, 缓冲大小字节, 使用到内存堆上的空闲空间移除超时时间) !该管理器不负责借用的共享缓冲的空间创建和删除
    inline void Init(uint8 *share_buf, int buf_size, unsigned int out_time) {
        GT_TMP_INIT_TBUF(share_buf)
        remove_time = out_time;
    }
    void Clear(); //清除一下额外增长多分配出来的内存空间

    //重设使各个空间区块存量恢复为之前指定的缓冲大小 !调用这个你需确保之前分配的空间数据可丢去失效的
    void ResetSpace();

    //重设数据缓冲(缓冲, 首部以使用大小) !调用这个你需确保之前分配的空间数据可丢去失效的
    void ResetSpace(uint8 *buf, int used_size);

    //获得当前空间用量状况(状况输出)
    void SaveUsState(sTBUF_STATE &tst);

    //恢复之前调用 SaveUsState 时的空间用量状况(用量状况)
    void RestoreUsState(const sTBUF_STATE &tst);

    //检测并清除空闲未被使用的空间(更新时间) !这个应该放在每帧都被调用的地方
    void CheckAndRemoveNoUseMem(unsigned int up_time);
};
class GN_API cBUF_WHEEL //缓冲轮 首尾相通的缓冲空间 !适合边写数据边读数据的应用场合 在尾写入在头取出 数据写到末尾之后如前段数据以被取出就轮回到开头段继续写入 这样就可以用小缓冲来处理大数据流 !当写满时将会自动扩充内部空间以保证所有数据都能成功写入
{
    uint8 *zBuf; //数据缓冲
    unsigned int zBufSize; //缓冲大小字节
    unsigned int zReadPos; //当前取出位置
    unsigned int zWritePos; //当前写入位置
  public:
    cBUF_WHEEL() : zBuf(0) {}
    cBUF_WHEEL(unsigned int size) : zBufSize(size) {
        zBuf = (uint8 *)malloc(size);
        zReadPos = 0;
        zWritePos = 0;
    } //分配缓冲空间(初始缓冲大小字节)
    ~cBUF_WHEEL() { GN_MemFree(zBuf); }

    void Delete() { GN_MemFree(zBuf); } //删除缓冲数据
    void Clear() {
        zReadPos = 0;
        zWritePos = 0;
    } //清空以存数据

    //分配缓冲空间(初始缓冲大小字节)
    void New(unsigned int size) {
        this->Delete();
        zBufSize = size;
        zBuf = (uint8 *)malloc(size);
        zReadPos = 0;
        zWritePos = 0;
    }

    //写入数据(数据, 数据大小字节)返回内部是否有扩充过空间
    bool Write(const uint8 *data, unsigned int size);

    //读取数据(装填数据的空间, 需读取大小字节)返回取出数据大小字节 返回0表示里面没数据了
    unsigned int Read(uint8 *data_out, unsigned int size);

    //取出内部数据(输入需取的大小字节返回取出数据大小字节)返回内部数据指针 !如果内部数据在指定大小不连续时将会自动整理为连续一段 调用后被取出的数据地址段 会被 Write 写入的数据覆盖
    uint8 *GetOut(unsigned int &size_in_out);

    //获得以存数据大小字节
    inline unsigned int DataSize() { return (zWritePos < zReadPos ? zBufSize - zReadPos + zWritePos : zWritePos - zReadPos); }

    //数据缓冲大小字节
    inline unsigned int BufSize() { return zBufSize; }
};
/*--------------------------------------------------------------------------------------------------------------------------------------*/
template <class T, class T2>
struct cDEPOT //数据仓库  是一种封闭的能定点快速移除特定数据 并且容量可自动扩充的数组 访问里面数据可像数组一样快速历遍 但放入数据最好用 insert 拿到数据的存放位置 !注意不适合比较看重构造函数分配的对象数据
{
    T2 **opps; //对方联系号码缓冲
    T *data; //数据缓冲
    T2 num; //实存数据个数
    T2 buf_len; //数据空间个数
    GN::sTBUF<T2> *tbuf; //临时缓冲分配器
  public:
    GT_TMP_ARRAY_ELEMENT_GET_DEF

    ~cDEPOT() {
        this->clear();
        tbuf = 0;
    }
    cDEPOT() : data(0), num(0), buf_len(0), tbuf(0) {}

    //构造(预留多少个数据空间, 临时缓冲分配器 如果放到里面的数据只是临时处理用给定这个性能更好)
    cDEPOT(T2 count, GN::sTBUF<T2> *_tbuf = 0) : data(0), num(0), buf_len(0), tbuf(0) {
        this->reserve(count, _tbuf);
    }
    T *Data() { return data; } //获得数据首地址  !!注意返回的地址您最好只用在临时处理的时候 因为如果您有新的数据放入后 该地址可能会变为无效
    T2 BufNum() { return buf_len; } //获得数据空间个数
    T2 BufSize() { return sizeof(T) * buf_len; } //获得数据空间所占大小字节
    bool Has() { return num ? true : false; } //是否有实存数据
    bool empty() { return num ? false : true; } //是否无实存数据
    T2 size() { return num; } //获得实存数据个数

    //清除内部所有数据
    void clear();

    //重分配预留一定的空间(预留多少个数据空间, 临时缓冲分配器 如果放到里面的数据只是临时处理用给定这个性能更好)
    void reserve(T2 count, GN::sTBUF<T2> *_tbuf = 0);

    //在尾部插入一个数据(返回装数据的位置ID, 数据)               !!! id_out 收到ID之后需确保 该ID的数据空间和数据仓库并存 因为之后如有移除过数据为了压缩储存空间 数据仓库会搬动数据从而更新这个ID
    void insert(T2 *id_out, const T &v);

    //在尾部插入空数据位置(返回装数据的位置ID)返回该数据位置地址 !!! id_out 收到ID之后需确保 该ID的数据空间和数据仓库并存 因为之后如有移除过数据为了压缩储存空间 数据仓库会搬动数据从而更新这个ID
    T *insert(T2 *id_out);

    //移除指定数据(数据位置ID)
    void remove(T2 id);

    //重设数据存取位置为初始状态, 以便覆盖原有数据 并保留数据空间
    void reset() { num = 0; }

    //向所有拥有数据者通告数据无效(无效值) !调用后拥有者的数据位置ID变为指定值
    void notify_inv(T2 inv);

    GT_TMP_ITERATOR_OPT_DEF(Ext, cDEPOT, 0, num, num - 1, 0 - 1)
};
template <class T, class T2>
void cDEPOT<T, T2>::clear() {
    if(data) {
        if(!tbuf) {
            free(opps);
            free(data);
        }
        opps = 0;
        data = 0;
    }
    num = 0;
    buf_len = 0;
}
template <class T, class T2>
void cDEPOT<T, T2>::reserve(T2 count, GN::sTBUF<T2> *_tbuf) {
    T *data2 = (T *)(_tbuf ? _tbuf->GetSpace(sizeof(T) * count) : malloc(sizeof(T) * count));
    T2 **opps2 = (T2 **)(_tbuf ? _tbuf->GetSpace(sizeof(T2 *) * count) : malloc(sizeof(T2 *) * count));
    if(data) { //原本有数据
        if(num) {
            T2 cpy_n = (num < count ? num : count);
            memcpy(data2, data, sizeof(T) * cpy_n);
            memcpy(opps2, opps, sizeof(T2 *) * cpy_n);
        }
        if(!tbuf) {
            free(opps);
            free(data);
        }
    }
    data = data2;
    opps = opps2;
    buf_len = count;
    tbuf = _tbuf;
}
template <class T, class T2>
void cDEPOT<T, T2>::insert(T2 *id_out, const T &v) {
    if(num >= buf_len) reserve(buf_len < 8 ? 8 : buf_len + (buf_len >> 1), tbuf);
    data[num] = v;
    opps[num] = id_out;
    *id_out = num++;
}
template <class T, class T2>
T *cDEPOT<T, T2>::insert(T2 *id_out) {
    if(num >= buf_len) reserve(buf_len < 8 ? 8 : buf_len + (buf_len >> 1), tbuf);
    //  data[num] = v;
    opps[num] = id_out;
    *id_out = num;
    return data + num++;
}
template <class T, class T2>
void cDEPOT<T, T2>::remove(T2 id) {
    --num;
    if(id >= num) return;
    data[id] = data[num];
    opps[id] = opps[num];
    *(opps[id]) = id;
}
template <class T, class T2>
void cDEPOT<T, T2>::notify_inv(T2 inv) {
    T2 i = 0;
    for(; i < num; ++i) *(opps[i]) = inv;
}
}; // namespace GN

#endif // GN_CONTAINERS_TEMPBUF_H
