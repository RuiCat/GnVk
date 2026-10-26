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
//#include"GND.h"
#ifdef _GN_PARALLEL_H_
/*--------------------------------------------------------- 多线程任务管理 ---------------------------------------------------------------------------------------------------------*/
GN_sTASK_LIST_NODE *GT_IdleTasks[2] = {0, 0};

GN_API void GN_TaskPushToIdleExec(P<GN_sTASK_BASE> task, bool thread_b) {
    GN_ListWheelListPushBack(GT_IdleTasks[(uint8)thread_b], task);
}
GN_API void GN_TaskIdleProc(unsigned int nTime, bool thread_b) {
    GN_ListWheelListProc<P<GN_sTASK_BASE>, GN_sTASK_LIST_NODE>(GT_IdleTasks[(uint8)thread_b], nTime);
}
GN_API void GN_TaskIdleClearAll() {
    GN_ListWheelListClear(GT_IdleTasks[0]);
    GN_ListWheelListClear(GT_IdleTasks[1]);
}
void GN_sDATA_SGM::Reset() {
    st = end = 0;
}
void GN_sDATA_SGM::SetSE(unsigned int _st, unsigned int _end) {
    st = _st;
    end = _end;
}
void GN_sDATA_SGM::SetSt(unsigned int _st) {
    st = _st;
}
void GN_sDATA_SGM::SetEnd(unsigned int _end) {
    end = _end;
}
unsigned int GN_sDATA_SGM::GetSt() {
    return st;
}
unsigned int GN_sDATA_SGM::GetEnd() {
    return end;
}
unsigned int GN_sDATA_SGM::Count() {
    return st < end ? end - st : 0;
}

bool GN_sTIME_EXEC_TASK::Run() {
    if(GN::TimeGetRunCount() >= exec_time) {
        proc(usarg);
        return true;
    }
    return false;
}
void GN_sRUN_TASK::Run() {
    proc(arg_1, arg_2);
}

#define GT_e_CWPC_WAIT 1 //等待
#define GT_e_CWPC_PROC 2 //执行并行处理
#define GT_e_CWPC_TIME 32 //催眠线程

#define GT_e_CWPC_WAIT_ 254 //取消等待掩码
#define GT_e_CWPC_PROC_ 253 //取消执行并行处理掩码
#define GT_e_CWPC_TIME_ 223 //取消催眠线程掩码

void GT_ParallelWaitProc(P<GN_sTHREAD> this_thread, void *arg, void *arg2) {
    GN_cPARALLEL *cp = (GN_cPARALLEL *)arg;
    unsigned int tid = (uintptr_t)arg2;
    GN_sDATA_SGM *_dsg = cp->zDataSeg + tid;
    GN::sLIST_WHEEL_MT<GN_sRUN_TASK, unsigned int> *_task_list = cp->zTaskLists + tid;
    GN_sTHREAD *td = this_thread;
    //td->us_state = GT_e_CWPC_WAIT;   //默认状态已经是 1
    while(td->us_state) {
        if(td->us_state & GT_e_CWPC_PROC) {
            cp->zUserProc(cp->zUserArg, _dsg);
            td->us_state &= GT_e_CWPC_PROC_;
        }
        while(_task_list->Has()) {
            _task_list->Begin()->Run();
            _task_list->discd_front();
        }
        if(td->us_state & GT_e_CWPC_TIME) {
            GN_TimeSleep(cp->zGetSleepTime());
            td->us_state &= GT_e_CWPC_TIME_;
        }
        //GN_TimeSleep(1);
        //if(tid==1)GN_Log("k1\n");
        GN_ThreadCondWait((volatile void *)(cp->zEvent));
    }
}
#define GT_TASK_LIST_MAX_SIZE 32 //8
unsigned int GN_cPARALLEL::zTn = 0;
unsigned int GN_cPARALLEL::zTn_ = 0;
unsigned int GN_cPARALLEL::zSortMinNum = 28;

static unsigned int GT_GetParallelProcMaxNum() {
    unsigned int tn = GT_GetSysCoreCount() - 1; //分发处理数, 留回一线程给系统, 不然会更加慢
    if(tn < 1) tn = 1;
    else if(tn > 3) tn = 3; /*存在问题:不使用超过3个核 有时间再找个性能最优解!*/
    return tn;
}
void GN_cPARALLEL::StartUp() {
    if(zDataSeg) return;
#ifdef GT_USING_GLes
    zTn = 1;
    zTn_ = 0;
#else
    if(!GN_ThreadSupport) {
        zTn = 1;
        zTn_ = 0;
        return;
    }
    if(!zTn) {
        zTn = GT_GetParallelProcMaxNum();
        zTn_ = zTn - 1; //减主线程
        if(zSortMinNum < zTn) zSortMinNum = zTn;
        if(!zTn_) return;
    }
    zEvent = GN_ThreadCondCreate();
    zTid = 0;
    unsigned int i, n = zTn_;
    zDataSeg = new GN_sDATA_SGM[zTn];
    zTaskLists = new GN::sLIST_WHEEL_MT<GN_sRUN_TASK, unsigned int>[n];
    zTrs_ = new GN_sTHREAD *[n];
    zTrs.New(n);
    P<GN_sTHREAD> *_Trs = zTrs;

    for(i = 0; i < n; ++i) {
        zTaskLists[i].New(GT_TASK_LIST_MAX_SIZE);
        _Trs[i] = GN_ThreadStart(GT_ParallelWaitProc, this, (void *)i);
        zTrs_[i] = _Trs[i];
        zDataSeg[i].pid = i;
    }
    zDataSeg[i].pid = i;
#endif
}
void GN_cPARALLEL::Shutdown() {
    if(zDataSeg) {
        GN_ThreadCondCtrl(zEvent, zTn_);
        unsigned int i, n = zTn_; //减主线程1个
        for(i = 0; i < n; ++i) zTrs_[i]->us_state = 0;
        for(i = 0; i < n; ++i) GN_ThreadEndWait(zTrs[i]);
        GN_DeleteArray(zDataSeg);
        GN_DeleteArray(zTrs_);
        zTrs.Delete();
        GN_DeleteArray(zTaskLists);
        GN_ThreadCondRemove(zEvent);
    }
}
int GN_cPARALLEL::AddProcThread(int tn) {
    int last_tn = (int)zTn;
    if(tn && zDataSeg && GN_ThreadSupport) {
        int ti = last_tn + tn;
        if(tn > 0) {
            int t_max = GT_GetParallelProcMaxNum();
            tn = (ti < t_max ? ti : t_max) - 1;
            if(tn > (int)zTrs.Num()) {
                Shutdown();
                zTn_ = tn;
                zTn = tn + 1;
                StartUp();
            } else {
                ti = last_tn - 1;
                for(; ti < tn; ++ti) {
                    zTrs[ti] = GN_ThreadStart(GT_ParallelWaitProc, this, (void *)ti);
                    zTrs_[ti] = zTrs[ti];
                }
                zTn_ = tn;
                zTn = tn + 1;
            }
        } else if(last_tn > 1) {
            GN_ThreadCondCtrl(zEvent, zTn_);
            zTn = ti > 1 ? ti : 1;
            zTn_ = zTn - 1; //减主线程
            for(ti = zTn; ti < last_tn; ++ti) {
                zTrs_[ti - 1]->us_state = 0;
                //zTaskLists[ti-1].reset();
            }
        }
    }
    return (int)zTn - last_tn;
}
void GN_cPARALLEL::On() {
    if(zDataSeg) GN_ThreadCondCtrl(zEvent, zTn_); //GN_Log("on ========================\n");
}
void GN_cPARALLEL::Off() {
    if(zDataSeg) GN_ThreadCondCtrl(zEvent, 0); //GN_Log("off ***********************\n");
}
void GN_cPARALLEL::TaskListClear(bool safe_check) {
    if(zDataSeg) {
        unsigned int i = 0, n = zTn_;
        if(safe_check) {
            for(; i < n; ++i) {
                if(zTaskLists[i].Bid() > 4000000000) break;
            }
            if(i < n) {
                TaskHelpEnd();
                i = 0;
            } else return;
        }
        for(; i < n; ++i) {
            zTaskLists[i].sLIST_WHEEL<GN_sRUN_TASK, unsigned int>::reset();
        }
    }
}
void GN_cPARALLEL::Sleep(unsigned int ms) {
    if(zDataSeg) {
        zSetSleepTime(ms);
        unsigned int i = 0, n = zTn_;
        for(; i < n; ++i) zTrs_[i]->us_state |= GT_e_CWPC_TIME;
    }
}
void GN_cPARALLEL::zRunProc(GN_sDATA_SGM *dsg) {
    zUserProc(zUserArg, dsg);
}
void GN_cPARALLEL::zSetSleepTime(unsigned int ms) {
    zSleepTime = ms;
}
unsigned int GN_cPARALLEL::zGetSleepTime() {
    return zSleepTime;
}

void GN_cPARALLEL::TaskPush(void (*proc)(void *arg, void *arg2), void *_arg, void *_arg2) {
#ifdef _MSC_VER
    TaskPush(GN_sRUN_TASK(proc, _arg, _arg2));
#else
    GN_sRUN_TASK tk(proc, _arg, _arg2);
    TaskPush(tk);
#endif
}
void GN_cPARALLEL::TaskPush(GN_sRUN_TASK &task) {
    if(zTn_ && zDataSeg) {
        unsigned int tid = zGetIdleTaskListID();
        if(zTaskLists[tid].sLIST_WHEEL<GN_sRUN_TASK, unsigned int>::push_back(task)) {
        } else task.Run();
    } else task.Run();
}
void GN_cPARALLEL::TaskPushUrgent(void (*proc)(void *arg, void *arg2), void *_arg, void *_arg2) {
#ifdef _MSC_VER
    TaskPushUrgent(GN_sRUN_TASK(proc, _arg, _arg2));
#else
    GN_sRUN_TASK tk(proc, _arg, _arg2);
    TaskPushUrgent(tk);
#endif
}
void GN_cPARALLEL::TaskPushUrgent(GN_sRUN_TASK &task) {
    if(zTn_ && zDataSeg) {
        if(zTaskLists[zTid].sLIST_WHEEL<GN_sRUN_TASK, unsigned int>::push_back(task)) {
            zTid += 1;
            if(zTid >= zTn_) zTid = 0;
        } else task.Run();
    } else task.Run();
}
void GN_cPARALLEL::zTaskHelp() {
    for(;;) {
        GN_sRUN_TASK *_tk = (zTaskLists + zGetBusyTaskListID())->try_pop_rbegin();
        if(_tk) _tk->Run();
        else break;
    }
}
void GN_cPARALLEL::zTaskExit() {
    unsigned int tid = 0, n = zTn_;
    for(; tid < n; ++tid) {
        for(;;) {
            GN_sRUN_TASK *_tk = (zTaskLists + tid)->try_pop_rbegin();
            if(_tk)
                ;
            else break;
        }
    }
}
bool GN_cPARALLEL::zTaskListIsRuning(unsigned int tid) {
    return zTaskLists[tid].Has();
}
bool GN_cPARALLEL::zProcIsRuning(unsigned int tid) {
    return (zTrs_[tid]->us_state & GT_e_CWPC_PROC);
}
bool GN_cPARALLEL::zThreadIsBusy(unsigned int tid) {
    return (zTrs_[tid]->us_state != GT_e_CWPC_WAIT);
}
unsigned int GN_cPARALLEL::ProcArrayStartPointsGetTry(unsigned int *sts_out, unsigned int st, unsigned int end) {
    if(zDataSeg) {
        unsigned int proc_num = end - st;
        /*模板代码:并行处理数组数据段分发*/
        unsigned int _pn = proc_num / zTn; /*倍数*/
        unsigned int _mn = proc_num - (_pn * zTn); /*余数*/
        unsigned int i = 0, n = zTn_;
        if(_mn) {
            ++_pn;
            for(; i < n; ++i) {
                if(i >= _mn) {
                    --_pn;
                    _mn = 0xffffffff;
                }
                sts_out[i] = st;
                st += _pn;
            }
            sts_out[i] = st;
        } else {
            for(; i < n; ++i) {
                sts_out[i] = st;
                st += _pn;
            }
            sts_out[i] = st;
        }
        return zTn;
    } else {
        sts_out[0] = st;
        return 1;
    }
}
void GN_cPARALLEL::ProcArray(void (*proc)(void *user_arg, const GN_sDATA_SGM *ds), void *_user_arg, unsigned int st, unsigned int end, bool wait) {
    if(zDataSeg) {
#ifdef GN_DEBUG
        GT_TMP_PARALLEL_PROC_CHECK;
#endif
        zUserProc = proc;
        zUserArg = _user_arg;
        unsigned int proc_num = end - st;
        /*模板代码:并行处理数组数据段分发*/
#define GT_TMP_PARALLEL_DATA_SGM_DISPATCH_(N, N_, ADD_PROC, ADD_PROC2) \
    unsigned int _pn = proc_num / N; /*倍数*/                          \
    unsigned int _mn = proc_num - (_pn * N); /*余数*/                  \
    unsigned int i = 0, n = N_;                                        \
    if(_mn) {                                                          \
        ++_pn;                                                         \
        for(; i < n; ++i) {                                            \
            if(i >= _mn) {                                             \
                --_pn;                                                 \
                _mn = 0xffffffff;                                      \
            }                                                          \
            ADD_PROC                                                   \
            zDataSeg[i].st = st;                                       \
            st += _pn;                                                 \
            zDataSeg[i].end = st;                                      \
            ADD_PROC2                                                  \
        }                                                              \
        if(i >= _mn) --_pn;                                            \
        zDataSeg[i].st = st;                                           \
        zDataSeg[i].end = st + _pn;                                    \
    } else {                                                           \
        for(; i < n; ++i) {                                            \
            zDataSeg[i].st = st;                                       \
            st += _pn;                                                 \
            zDataSeg[i].end = st;                                      \
            ADD_PROC2                                                  \
        }                                                              \
        zDataSeg[i].st = st;                                           \
        zDataSeg[i].end = st + _pn;                                    \
    }
#ifdef GT_USING_GLes
#define MEM_BARRIER GN_MemBarrier();
#else
#define MEM_BARRIER
#endif
#define GT_TMP_PARALLEL_DATA_SGM_DISPATCH(N, N_, ADD_PROC) GT_TMP_PARALLEL_DATA_SGM_DISPATCH_(N, N_, ADD_PROC, MEM_BARRIER zTrs_[i]->us_state |= GT_e_CWPC_PROC;)

        GT_TMP_PARALLEL_DATA_SGM_DISPATCH(zTn, zTn_, )
        proc(_user_arg, zDataSeg + i);

#define GT_TMP_PARALLEL_WAIT_PROC_END \
    for(i = 0; i < n; ++i)            \
        while((volatile uint8)(zTrs_[i]->us_state & GT_e_CWPC_PROC));
        if(wait) { GT_TMP_PARALLEL_WAIT_PROC_END }
    } else {
        GN_sDATA_SGM dsg0(st, end, 0);
        proc(_user_arg, &dsg0);
    }
}
void GN_cPARALLEL::ProcArrayTry(unsigned int st, unsigned int end) {
    if(zDataSeg) {
        unsigned int proc_num = end - st;
        GT_TMP_PARALLEL_DATA_SGM_DISPATCH_(zTn, zTn_, , )
    }
}
void GN_cPARALLEL::Proc(void (*proc)(void *user_arg, GN_sDATA_SGM *ds), void *_user_arg, unsigned int st, unsigned int end, unsigned int fr) {
    if(st >= end) return;
    if(zDataSeg) {
#ifdef GN_DEBUG
#define GT_TMP_PARALLEL_PROC_CHECK \
    if(ProcHas()) { GN_Log("并行处理 GN_cPARALLEL::Proc ... 只能等上次的处理完成后才能再次调用 或者您可以先调用 GN_cPARALLEL::ProcHas() 来检测是否有未完成的处理\n"); }

        GT_TMP_PARALLEL_PROC_CHECK;
#endif
        if(!zTn_) {
            *zDataSeg = GN_sDATA_SGM(st, end, 0);
            proc(_user_arg, zDataSeg);
            return;
        }
        zUserProc = (void (*)(void *, const GN_sDATA_SGM *))proc;
        zUserArg = _user_arg;
        unsigned int proc_num = end - st;

        if(proc_num < (zTn << 1)) { //太少数量就用类似 ProcArray 的处理方式
            GT_TMP_PARALLEL_DATA_SGM_DISPATCH(zTn, zTn_, if(!_pn) break;)
            if(_pn) proc(_user_arg, zDataSeg + i);
            GT_TMP_PARALLEL_WAIT_PROC_END
            return;
        } //主线程处理小一点计算, 其他平均分发
        unsigned int mn = proc_num / (zTn_ * fr + 1) + 1;
        proc_num -= mn;
        GT_TMP_PARALLEL_DATA_SGM_DISPATCH(zTn_, zTn_ - 1, )
        n = zTn_;
        GN_sDATA_SGM *_data_seg = zDataSeg;
        GN_sDATA_SGM *spl_seg, *_cur_seg, *end_seg = _data_seg + n;
        end_seg->st = st + _pn;
        end_seg->end = end_seg->st + mn;
        zTrs_[i]->us_state |= GT_e_CWPC_PROC;
        proc(_user_arg, end_seg);

        //找出处理余量最大的, 切尾补到空闲线程来处理
        unsigned int wpids[GN_USING_PARALLEL_MAX];
        unsigned int fpids[GN_USING_PARALLEL_MAX];
        unsigned int m_n, _m_n, _m_n2, _ed, _ed_, fn = 0;
        for(i = 0; i < n; ++i) wpids[i] = 0;
        for(;;) {
            for(i = 0; i < n; ++i) {
                if(!(zTrs_[i]->us_state & GT_e_CWPC_PROC)) { /*处理完的线程*/
                    if(!_data_seg[i].end) goto GT_GOTO_WaitProcEnd3;
                    if(_data_seg[i].st < wpids[i]) {
                        fpids[fn++] = wpids[i];
                        wpids[i] = 0;
                    }
#define GT_TMP_PARALLEL_FIND_LONG_PROC(ADD_PROC)                                                                         \
    _m_n = 0;                                                                                                            \
    for(_cur_seg = _data_seg; _cur_seg < end_seg; ++_cur_seg) {                                                          \
        /*unsigned int st0 = _cur_seg->GetSt();*/                                                                        \
        m_n = _cur_seg->Count();                                                                                         \
        if(m_n > 3) {                                                                                                    \
            /*if(_cur_seg->GetSt() > _cur_seg->GetEnd()){  GN_RunReportError(); }*/                                      \
            /*存在问题,st会加过隆*/                                                                                      \
            /*if(_cur_seg->GetSt() >= _cur_seg->GetEnd()){ GN_Log("-%d %d\n",_cur_seg->GetSt(), _cur_seg->GetEnd()); }*/ \
            if(m_n > _m_n) { /*找出余量最大*/                                                                            \
                _m_n = m_n;                                                                                              \
                spl_seg = _cur_seg;                                                                                      \
            }                                                                                                            \
            /*if(_cur_seg->GetSt() > st0){ GN_Log(">%d %d\n",st0, _cur_seg->GetSt()); }*/                                \
        } /*else if(!_cur_seg->end)goto GT_GOTO_WaitProcEnd3;*/                                                          \
        ADD_PROC                                                                                                         \
    }
                GT_GOTO_FindLongProc:
                    GT_TMP_PARALLEL_FIND_LONG_PROC()

#define GT_TMP_PARALLEL_SPLIT_NUM_CPT(GT)   \
    if(_m_n > 3) { /*还可分割*/             \
        _ed = spl_seg->GetEnd();            \
        if(spl_seg->GetSt() + 3 >= _ed) {   \
            if(_ed) goto GT;                \
            else goto GT_GOTO_WaitProcEnd3; \
        }                                   \
        _m_n = _ed - spl_seg->GetSt();      \
        if(_m_n <= 3) goto GT;              \
        _m_n = ((_m_n - 2) >> 1); /*最小为1*/

                    /*GN_Log("%d\n", _m_n);*/
#define GT_TMP_PARALLEL_SPLIT_PROC(GT)                                                                        \
    _ed_ = _ed - _m_n;                                                                                        \
    if(spl_seg->GetSt() + 2 < _ed_) { /*GN_Log("%d %d\n", _data_seg[_j].GetSt() , _data_seg[_j].GetEnd()); */ \
        spl_seg->SetEnd(_ed_);                                                                                \
        GN_MemBarrier();                                                                                      \
        _data_seg[i].SetEnd(_ed); /*GN_Log("%d %d\n", _data_seg[_j].GetSt(), _data_seg[_j].GetEnd());*/       \
        unsigned int _st = spl_seg->GetSt();                                                                  \
        unsigned int _st1 = _st + 1;                                                                          \
        if(_st1 >= _ed) goto GT;                                                                              \
        else if(_st1 < _ed_) _data_seg[i].SetSt(_ed_);                                                        \
        else /* if(_st >= _ed_)*/ {                                                                           \
            /*GN_Log(">=%d %d %d\n", _st , _ed_, _ed);*/                                                      \
            wpids[spl_seg - _data_seg] = _st1;                                                                \
            if(_st1 + 1 >= _ed) goto GT;                                                                      \
            _data_seg[i].SetSt(_st1 + 1);                                                                     \
        }
                    GT_TMP_PARALLEL_SPLIT_NUM_CPT(GT_GOTO_WaitProcEnd)
                    GT_TMP_PARALLEL_SPLIT_PROC(GT_GOTO_WaitProcEnd)
                    zTrs_[i]->us_state |= GT_e_CWPC_PROC;
                    GN_MemBarrier();

                } //else goto GT_GOTO_FindLongProc;
            }
            else goto GT_GOTO_WaitProcEnd; /*处理分发结束*/
        } //end if us_state
    } //end for i
    if(fn) {
#define GT_TMP_PARALLEL_PROC_INTV \
    for(i = 0; i < fn; ++i) {     \
        end_seg->st = fpids[i];   \
        end_seg->end = fpids[i];  \
        proc(_user_arg, end_seg); \
    }
        GT_TMP_PARALLEL_PROC_INTV
        fn = 0;
    } else {
    GT_GOTO_FindLongProc1:
        _m_n2 = 0xffffffff;
        GT_TMP_PARALLEL_FIND_LONG_PROC(if(m_n < _m_n2) _m_n2 = m_n;)
        GT_TMP_PARALLEL_SPLIT_NUM_CPT(GT_GOTO_WaitProcEnd1)
        _m_n2 = _m_n2 < 2 ? 1 : _m_n2 / fr + (fr > 1 ? 1 : 0);
        _m_n = _m_n2 < _m_n ? _m_n2 : _m_n;
        GT_TMP_PARALLEL_SPLIT_PROC(GT_GOTO_WaitProcEnd1)
        //GN_Log("%d %d\n", _data_seg[i].GetSt(), _data_seg[i].GetEnd());
        proc(_user_arg, end_seg); //GN_Log("%d\n",i);
        if(!end_seg->end) goto GT_GOTO_WaitProcEnd3;
    } //else goto GT_GOTO_FindLongProc1;
}
else goto GT_GOTO_WaitProcEnd1; //处理分发结束
}
}
GT_GOTO_WaitProcEnd : //GN_Log("-\n");
                      if(fn){
                          GT_TMP_PARALLEL_PROC_INTV} GT_GOTO_WaitProcEnd1 : for(i = 0; i < n; ++i) {
    while((volatile uint8)(zTrs_[i]->us_state & GT_e_CWPC_PROC));
    if(_data_seg[i].st < wpids[i]) {
        end_seg->st = wpids[i]; //GT_TMP_PARALLEL_PROC_INTV
        end_seg->end = wpids[i];
        proc(_user_arg, end_seg);
    }
}
return;
GT_GOTO_WaitProcEnd3 : GT_TMP_PARALLEL_WAIT_PROC_END
}
else {
    GN_sDATA_SGM dsg0(st, end, 0);
    proc(_user_arg, &dsg0);
}
}
GN_API unsigned int GN_ArrayArrange(void **p_array, const GN_sDATA_SGM *const dsg, unsigned int dsg_count) {
    GT_TMP_ARRAY_ARRANGE(memcpy(p_array + end, p_array + end2, en << GT_PTR_SIZE_OFS);, memcpy(p_array + end, p_array + st2, sn << GT_PTR_SIZE_OFS);)
}
GN_API unsigned int GN_ArrayArrange(unsigned int *p_array, const GN_sDATA_SGM *const dsg, unsigned int dsg_count) {
    GT_TMP_ARRAY_ARRANGE(memcpy(p_array + end, p_array + end2, en << 2);, memcpy(p_array + end, p_array + st2, sn << 2);)
}
GN_API unsigned int GN_ArrayArrange(uint8 *p_array, unsigned int tsize, const GN_sDATA_SGM *const dsg, unsigned int dsg_count) {
    GT_TMP_ARRAY_ARRANGE(memcpy(p_array + end * tsize, p_array + end2 * tsize, en * tsize);, memcpy(p_array + end * tsize, p_array + st2 * tsize, sn * tsize);)
}
GN_API unsigned int GN_ArrayArrange(void (*proc)(void *user_arg, unsigned int dst_i, unsigned int src_i, unsigned int count), void *_user_arg, const GN_sDATA_SGM *const dsg, unsigned int dsg_count) {
    GT_TMP_ARRAY_ARRANGE(proc(_user_arg, end, end2, en);, proc(_user_arg, end, st2, sn);)
}
GN_API unsigned int GN_ArrayArrangeKeepOrder(void **p_array, const GN_sDATA_SGM *const dsg, unsigned int dsg_count) {
    GT_TMP_ARRAY_ARRANGE_KEEP_ORDER(memcpy(p_array + end, p_array + set, cn << GT_PTR_SIZE_OFS);)
}
GN_API unsigned int GN_ArrayArrangeKeepOrder(unsigned int *p_array, const GN_sDATA_SGM *const dsg, unsigned int dsg_count) {
    GT_TMP_ARRAY_ARRANGE_KEEP_ORDER(memcpy(p_array + end, p_array + set, cn << 2);)
}
GN_API unsigned int GN_ArrayArrangeKeepOrder(uint8 *p_array, unsigned int tsize, const GN_sDATA_SGM *const dsg, unsigned int dsg_count) {
    GT_TMP_ARRAY_ARRANGE_KEEP_ORDER(memcpy(p_array + end * tsize, p_array + set * tsize, cn * tsize);)
}
GN_API unsigned int GN_ArrayArrangeKeepOrder(void (*proc)(void *user_arg, unsigned int dst_i, unsigned int src_i, unsigned int count), void *_user_arg, const GN_sDATA_SGM *const dsg, unsigned int dsg_count) {
    GT_TMP_ARRAY_ARRANGE_KEEP_ORDER(proc(_user_arg, end, set, cn);)
}
namespace GN {
GN_API GN_cPARALLEL Parallel;
};
#endif //endif _GN_PARALLEL_H_
