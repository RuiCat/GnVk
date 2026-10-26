/****************************************************************************
Copyright (c) 2018-2025 gncreate Technologies Inc.

 http://www.gncreate.com
 http://www.img-power.com

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 ****************************************************************************/

#include "GN.h"
#ifdef _GN_ARRAY_H_
/*----------------------------------------------------------------------------------------------------------------*/
volatile long GN::sTBUF_MT::IsBusy() {
    return is_busy;
}

//屏蔽其它线程锁定处理
#define GT_TMP_TBUF_WAIT_PROC(LC, PROC)           \
    while(InterlockedCompareExchange(&LC, 1, 0)); \
    PROC                                          \
        InterlockedExchange(&LC, 0);

void GN::sTBUF_MT::Clear() {
    GT_TMP_TBUF_WAIT_PROC(is_busy, sTBUF<unsigned int>::Clear();)
}
void *GN::sTBUF_MT::GetSpace(unsigned int _size_) {
    GT_TMP_TBUF_WAIT_PROC(is_busy, void *p = sTBUF<unsigned int>::GetSpace(_size_);)
    return p;
}
/*----------------------------------------------------------------------------------------------------------------*/
void GN::sTBUF_MAG::Clear() {
    sTBUF_INT::Clear();
    buf_idle_times.clear();
}
void GN::sTBUF_MAG::ResetSpace() {
    if(p__) _size = size;
    if(size) {
        if(add_bufs.size()) {
            int bid = -1;
            GN::sLIST<GT_sTBUF<int>>::iterator it_b = add_bufs.begin(), et_b = add_bufs.end();
            for(; it_b != et_b; ++it_b, ++bid) {
                int &size1 = it_b->_size;
                if(size1 >= size) break;
                if(size1 >= 0) size1 = size;
            }
            if(last_reset_max_bid < bid) last_reset_max_bid = bid;
        }
    } else this->Clear();
}
void GN::sTBUF_MAG::ResetSpace(uint8 *buf, int used_size) {
    p__ = buf + used_size;
    ResetSpace();
    _size -= used_size;
}
void GN::sTBUF_MAG::SaveUsState(sTBUF_STATE &tst) {
    if(add_bufs.size()) {
        GN::sLIST<GT_sTBUF<int>>::reverse_iterator it_b = add_bufs.rbegin(), et_b = add_bufs.rend();
        for(; it_b != et_b; ++it_b) {
            if(it_b->_size < size) {
                tst.alloc_buf = &(*it_b);
                tst.m_size = tst.alloc_buf->_size;
                return;
            }
        }
    }
    tst.alloc_buf = this;
    tst.m_size = _size;
}
void GN::sTBUF_MAG::RestoreUsState(const sTBUF_STATE &tst) {
    if(add_bufs.size()) {
        int bid = -1;
        GN::sLIST<GT_sTBUF<int>>::iterator it_b = add_bufs.begin(), et_b = add_bufs.end();
        if(tst.alloc_buf != this) {
            for(; it_b != et_b; ++it_b) {
                ++bid;
                if(tst.alloc_buf == &(*it_b)) break;
            }
            if(last_reset_max_bid < bid) last_reset_max_bid = bid;
            ++it_b;
        }
        for(; it_b != et_b; ++it_b) {
            if(it_b->_size >= 0) it_b->_size = size;
        }
    }
    tst.alloc_buf->_size = tst.m_size;
}
void GN::sTBUF_MAG::CheckAndRemoveNoUseMem(unsigned int up_time) {
    unsigned int buf_count = add_bufs.size();
    if(buf_count) {
        GN::sLIST<unsigned int>::iterator it_t = buf_idle_times.begin();
        if(buf_count > buf_idle_times.size()) {
            GN::sLIST<unsigned int>::iterator et_t = buf_idle_times.end();
            for(; it_t != et_t; ++it_t) *it_t = 0;
            buf_count -= buf_idle_times.size();
            unsigned int i = 0;
            for(; i < buf_count; ++i) buf_idle_times.push_back(0); //同步数据长度
        } else {
            GN::sLIST<GT_sTBUF<int>>::iterator it_b = add_bufs.begin(), et_b = add_bufs.end();
            int bid = -1; //使本回有用过的缓冲超时时间为0
            for(; it_b != et_b; ++it_b, ++it_t, ++bid) {
                if(it_b->_size == size) break;
                *it_t = 0;
            }
            for(; bid < last_reset_max_bid; ++it_b, ++it_t, ++bid) {
                *it_t = 0;
            } //本回未使用过的缓冲检测超时时间
            for(; it_b != et_b; ++it_b, ++it_t) {
                *it_t += up_time;
                if(*it_t > remove_time) break;
            } //超出时间之后的缓冲移除
            while(it_b != et_b) {
#define GT_TMP_TBUF_DEL_ADD_BUF  \
    free(it_b->p__);             \
    it_b = add_bufs.erase(it_b); \
    it_t = buf_idle_times.erase(it_t);

                GT_TMP_TBUF_DEL_ADD_BUF
            }
        }
        it_t = buf_idle_times.begin();
        GN::sLIST<GT_sTBUF<int>>::iterator it_b = add_bufs.begin(), et_b = add_bufs.end();
        for(; it_b != et_b;) {
            if(it_b->_size < 0) {
                GT_TMP_TBUF_DEL_ADD_BUF
            } else {
                ++it_b;
                ++it_t;
            }
        }
        last_reset_max_bid = -1;
    }
}
/*----------------------------------------------------------------------------------------------------------------*/
bool GN::cBUF_WHEEL::Write(const uint8 *data, unsigned int size) {
    unsigned int seg_size;
    if(zWritePos >= zReadPos) {
        seg_size = zBufSize - zWritePos;
        if(size < seg_size) {
            memcpy(zBuf + zWritePos, data, size);
            zWritePos += size;
            size = 0;
        } else {
            memcpy(zBuf + zWritePos, data, seg_size);
            zWritePos = 0;
            size -= seg_size;
        }
    } else seg_size = 0;
    if(size) {
        if(zReadPos - zWritePos > size) {
            memcpy(zBuf + zWritePos, data + seg_size, size);
            zWritePos += size;
        } else {
            unsigned int last_size = zBufSize;
            zBufSize += (last_size >> 1) > size ? (last_size >> 1) : size;
            uint8 *buf2 = (uint8 *)malloc(zBufSize);
            last_size -= zReadPos;
            memcpy(buf2, zBuf + zReadPos, last_size);
            memcpy(buf2 + last_size, zBuf, zWritePos);
            zWritePos += last_size;
            memcpy(buf2 + zWritePos, data + seg_size, size);
            zWritePos += size;
            zReadPos = 0;
            free(zBuf);
            zBuf = buf2;
            return true;
        }
    }
    return false;
}
unsigned int GN::cBUF_WHEEL::Read(uint8 *data_out, unsigned int size) {
    if(zWritePos == zReadPos) return 0;
    unsigned int seg_size = (zWritePos > zReadPos ? zWritePos : zBufSize) - zReadPos;
    if(seg_size > size) seg_size = size;
    memcpy(data_out, zBuf + zReadPos, seg_size);
    zReadPos += seg_size;
    if(seg_size < size && zWritePos < zReadPos) {
        unsigned int seg_size1 = size - seg_size;
        if(zWritePos < seg_size1) seg_size1 = zWritePos;
        memcpy(data_out + seg_size, zBuf, seg_size1);
        zReadPos = seg_size1;
        seg_size += seg_size1;
    } //GN_Log("seg_size=%d\n", seg_size);
    return seg_size;
}
uint8 *GN::cBUF_WHEEL::GetOut(unsigned int &size) {
    if(zWritePos == zReadPos) {
        size = 0;
        return zBuf;
    }
    unsigned int seg_size = (zWritePos > zReadPos ? zWritePos : zBufSize) - zReadPos;
    if(size > seg_size) {
        if(zReadPos > zWritePos) { //不连续段
            unsigned int _size_ = zReadPos - zWritePos;
            if(_size_ < seg_size) {
                uint8 buf[131072]; //128K
                sTBUF_INT tbuf(buf, 131072);
                unsigned int __size_ = seg_size - _size_;
                uint8 *data1 = (uint8 *)tbuf.GetSpace(__size_);
                memcpy(data1, zBuf + zReadPos, __size_);
                zReadPos += __size_;
                memmove(zBuf + seg_size, zBuf, zWritePos);
                memcpy(zBuf, data1, __size_);
                memcpy(zBuf + __size_, zBuf + zReadPos, seg_size - __size_);
            } else {
                memmove(zBuf + seg_size, zBuf, zWritePos);
                memcpy(zBuf, zBuf + zReadPos, seg_size);
            }
            zReadPos = 0;
            zWritePos += seg_size;
        } else size = seg_size;
    }
    uint8 *ret = zBuf + zReadPos;
    zReadPos += size;
    return ret;
}
/*----------------------------------------------------------------------------------------------------------------*/
void GT_sBUF_MAPPING::Release() {
    uint8 **_datas = zBufs.data;
    unsigned int di = 0, dn = zBufs.num;
    for(; di < dn; ++di) {
        if(_datas[di]) free(_datas[di]);
    }
    zBufs.Delete();
}
unsigned int GT_sBUF_MAPPING::GetPos(const uint8 *p) {
    uint8 **_datas = zBufs.data;
    unsigned int di = 0, dn = zBufs.num;
    for(; di < dn; ++di) {
        if(p >= _datas[di] && p < _datas[di] + zBufLen) {
            unsigned int len_ = p - _datas[di];
            return (len_ ? len_ / zUnitSize : 0) + di * zBufElmNum;
        }
    }
    return 0xffffffff;
}
/*----------------------------------------------------------------------------------------------------------------*/
namespace GN {
void cVOLUME_MAG1::Release() {
    GN_MemFree(zPosData);
    zPosNum = 0;
    zTotalSize = 0;
    zUsedSize = 0;
}
void cVOLUME_MAG1::InitToData(unsigned int total_size, unsigned int (*full_proc)(unsigned int tot_size, void *us_arg), void *_us_arg) {
    if(zPosData) Release();
    zPosData = (unsigned int *)malloc(sizeof(unsigned int) * total_size);
    zPosNum = 0;
    zTotalSize = total_size;
    zUsedSize = 0;
    zUsArg = _us_arg;
    zpfOnFull = full_proc;
}
unsigned int cVOLUME_MAG1::Alloc() {
    if(zPosNum) return zPosData[--zPosNum];
    else {
        if(zUsedSize >= zTotalSize) {
            zTotalSize += zpfOnFull(zTotalSize, zUsArg);
            unsigned int *pos_data = (unsigned int *)malloc(sizeof(unsigned int) * zTotalSize);
            memcpy(pos_data, zPosData, sizeof(unsigned int) * zPosNum);
            free(zPosData);
            zPosData = pos_data;
        }
        return zUsedSize++;
    }
}
inline int GT_CompareUint(unsigned int left, unsigned int right) {
    return left < right;
}
GN::sSEGMENT<unsigned int> cVOLUME_MAG1::GetEndSegRange(bool accur) {
    unsigned int pos = zUsedSize;
    if(accur && zPosNum) {
        GN_Sort(unsigned int, zPosData, zPosNum, GT_CompareUint);
        int i = (int)zPosNum - 1;
        for(; i >= 0; --i, --pos) {
            if(pos - zPosData[i] > 1) break;
        }
    }
    return sSEGMENT<unsigned int>(pos, zTotalSize - pos);
}
/*----------------------------------------------------------------------------------------------------------------*/
void cVOLUME_MAG::Release() {
    zSizeTable.clear();
    zPosTable.clear();
    zTotalSize = 0;
}
void cVOLUME_MAG::InitToData(unsigned int total_size, unsigned int (*full_proc)(unsigned int tot_size, unsigned int need_size, void *us_arg), void *_us_arg) {
    if(zTotalSize) Release();
    zTotalSize = total_size;
    zUsArg = _us_arg;
    zpfOnFull = full_proc;
    sSEGMENT<unsigned int> v(0, total_size);
    zSizeTable.insert(v, GT_CompareSegLenMin);
    zPosTable.insert(v, GT_CompareSegPosMin);
}
unsigned int cVOLUME_MAG::Alloc(unsigned int size) {
    sSEGMENT<unsigned int> v(0, size);
    cTREE2_UINTx2::TP *find_node, *up_node;
    unsigned int pos;
    if(zSizeTable.search2_if(GT_CompareSegLenMin, v, &find_node, &up_node) == 0) {
        pos = (*find_node)->zData.pos;
        zSizeTable.erase(find_node);
        v.pos = pos;
        zPosTable.erase(v, GT_CompareSegPosMin);
        return pos;
    } else {
        if(up_node) {
            pos = (*up_node)->zData.pos;
            zSizeTable.erase(up_node);
            v.pos = pos;
            zPosTable.search2_if(GT_CompareSegPosMin, v, &find_node);
            unsigned int &len_ = (*find_node)->zData.len;
            len_ -= v.len;
            v.len = len_;
            zSizeTable.insert(v, GT_CompareSegLenMin1);
            return pos + v.len;
        } else {
            unsigned int total_size = zTotalSize;
            zTotalSize += zpfOnFull(zTotalSize, v.len, zUsArg);
            v.pos = total_size + v.len;
            v.len = zTotalSize - v.pos;
            if(v.len) {
                zSizeTable.insert(v, GT_CompareSegLenMin1);
                zPosTable.insert(v, GT_CompareSegPosMin);
            }
            return total_size;
        }
    }
}
void cVOLUME_MAG::Free(unsigned int pos, unsigned int size, bool merge_front, bool merge_back) {
    sSEGMENT<unsigned int> v(pos, size);
    cTREE2_UINTx2::TP *find_node, *up_node, *front_node;
    zPosTable.search2ff_if(GT_CompareSegPosMin, v, &find_node, &up_node, &front_node);
    //GN_Log("PS=%d %d %d %d %x %x\n", pos, size, merge_front?1:0, merge_back?1:0, front_node, up_node);
    if(merge_back && up_node) { //续后
        sSEGMENT<unsigned int> seg0 = (*up_node)->zData; //GN_Log("seg0:%d %d\n",  seg0.pos, seg0.len);
        if(v.pos + v.len == seg0.pos) { //GN_Log("cc\n");
            zPosTable.erase(up_node);
            find_node = 0;
            zSizeTable.erase(seg0, GT_CompareSegLenMin2);
            v.len += seg0.len;
        }
    }
    if(merge_front && front_node) { //续前
        sSEGMENT<unsigned int> &seg = (*front_node)->zData; //GN_Log("seg:%d %d\n", seg.pos, seg.len);
        if(seg.pos + seg.len == pos) {
            zSizeTable.erase(seg, GT_CompareSegLenMin2); //GN_Log("CC\n");
            seg.len += v.len;
            zSizeTable.insert(seg, GT_CompareSegLenMin1);
            //GN_Log("NC:%d %d\n", zPosTable.zNodeCount, zSizeTable.zNodeCount);
            return;
        }
    }
    if(find_node) zPosTable.insert(find_node, v);
    else zPosTable.insert(v, GT_CompareSegPosMin);
    zSizeTable.insert(v, GT_CompareSegLenMin1);
    //GN_Log("NC--:%d %d\n\n", zPosTable.zNodeCount, zSizeTable.zNodeCount);
}
GN::sSEGMENT<unsigned int> cVOLUME_MAG::GetEndSegRange() {
    sSEGMENT<unsigned int> v(0xffffffff, 0xffffffff);
    cTREE2_UINTx2::TP *find_node, *up_node, *front_node;
    zPosTable.search2ff_if(GT_CompareSegPosMin, v, &find_node, &up_node, &front_node);
    if(front_node) {
        v.pos = (*front_node)->zData.pos + (*front_node)->zData.len;
        v.len = zTotalSize - v.pos;
        return v;
    } else {
        return sSEGMENT<unsigned int>(0, zTotalSize);
    }
}
}; // namespace GN
/*----------------------------------------------------------------------------------------------------------------*/
//重分配等份用量(当前总大小个数, 用户参数)返回新分配大小个数
static unsigned int GT_ReallocVolume1(unsigned int tot_size, void *us_arg) {
    ((GN::cMEM_POOL1 *)us_arg)->PushBuf();
    return ((GN::cMEM_POOL1 *)us_arg)->zBufElmNum;
}
//重分配用量(当前总大小个数, 当前需要分配的最小大小个数, 用户参数)返回新分配大小个数
static unsigned int GT_ReallocVolume(unsigned int tot_size, unsigned int need_size, void *us_arg) {
    GN::cMEM_POOL *_mp = (GN::cMEM_POOL *)us_arg;
    unsigned int bn = need_size / _mp->zBufElmNum;
    unsigned int new_size = bn * _mp->zBufElmNum;
    if(new_size < need_size) {
        new_size += _mp->zBufElmNum;
        ++bn;
    }
    for(; bn; --bn) _mp->PushBuf();
    return new_size;
}
namespace GN {
void cMEM_POOL1::InitToData(unsigned int total_size, unsigned int (*full_proc)(unsigned int tot_size, void *us_arg), void *_us_arg) {
#define GT_TMP_MEM_POOL_INIT_DEF(N)                                    \
    GT_sBUF_MAPPING::Release();                                        \
    cVOLUME_MAG##N::InitToData(total_size, GT_ReallocVolume##N, this); \
    GT_sBUF_MAPPING::InitToData(1, total_size);                        \
    PushBuf();

    GT_TMP_MEM_POOL_INIT_DEF(1)
}
void cMEM_POOL::InitToData(unsigned int total_size, unsigned int (*full_proc)(unsigned int tot_size, unsigned int need_size, void *us_arg), void *_us_arg) {
    GT_TMP_MEM_POOL_INIT_DEF()
}
}; // namespace GN
/*----------------------------------------------------------------------------------------------------------------*/
GT_sMEMORY_TABLE::GT_sMEMORY_TABLE() : isBusy(0) {
    if(!memList.empty()) memList.clear();
}
GT_sMEMORY_TABLE::~GT_sMEMORY_TABLE() {
#if defined _GN_FORMAT_H_ && defined GN_USING_UNZIP
    if(!GT_ZipIDs.zFreeIDs.empty()) GT_ZipIDs.zFreeIDs.clear();
    GT_ZipIDs.zMaxID = 0;
#endif
    if(memList.empty()) return;

#ifdef GN_DEBUG
    GN_Log("has memory do not free!!!\n");
#endif
    GN::sLIST<GT_sMEMORY>::iterator it = memList.begin(), et = memList.end();
    for(; it != et; ++it) {
#ifdef GN_DEBUG
        GN_Log("%08X    using %d    data count %d\n", it->pmem, it->us, it->num);
#endif
        delete[](unsigned char *)it->pmem; //清除所有沉余内存
    }
    memList.clear();
}
volatile GT_sMEMORY *GT_sMEMORY_TABLE::Add(GT_sMEMORY &em) {
    GT_TMP_TBUF_WAIT_PROC(isBusy,
                          volatile GT_sMEMORY *ret = memList.add(em);)
    return ret;
}
void GT_sMEMORY_TABLE::Del(volatile GT_sMEMORY *mem) {
    GT_TMP_TBUF_WAIT_PROC(isBusy,
                          memList.del((GT_sMEMORY *)mem);)
}
GN_API bool P_CopyModeAll = false;

void P_::Re(void *p) {
    this->Delete();
    GN::sLIST<GT_sMEMORY>::iterator ir = GT_UsMemTable.memList.begin(), er = GT_UsMemTable.memList.end();
    for(; ir != er; ++ir) {
        if(p == ir->pmem) {
            zData = &(*ir);
            (zData->us)++;
            return;
        }
    }
}
#endif //_GN_ARRAY_H_