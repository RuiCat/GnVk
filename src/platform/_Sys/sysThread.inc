// GN_mini 平台实现片段：GN_sTHREAD 线程实现。
// 2026-11-26 自 GN_Sys.cpp 第 1172-1335 行 move-only 拆出（REFACTOR_PLAN §13 B6）。
// 由 GN_Sys.cpp 按原顺序包含（单编译单元，#ifdef 平台分支语义不变）；不是独立头文件。
static void GT_CheckMultiThreadCaps(P<GN_sTHREAD> this_thread, void *arg, void *arg2) {
    GN_TimeSleep(10);
    GN_ThreadSupport = true;
}
GN_sTHREAD::GN_sTHREAD() : id(0), us_state(1) {}

void GN_sTHREAD::UsFlagsAdd(uint8 fg) {
    us_state |= fg;
    GN_MemBarrier();
}
void GN_sTHREAD::UsFlagsDel(uint8 fg) {
    us_state &= ~fg;
    GN_MemBarrier();
}
bool GN_sTHREAD::UsFlagsHas(uint8 fg) {
    return us_state & fg;
}
void GN_sTHREAD::UsStateAnd(uint8 mk) {
    us_state &= mk;
    GN_MemBarrier();
}
void GN_sTHREAD::UsStateSet(uint8 st) {
    us_state = st;
    GN_MemBarrier();
}
uint8 GN_sTHREAD::UsStateGet() {
    return us_state;
}

#if defined _WIN32_ || defined _WIN32_WCE
GN_sTHREAD::~GN_sTHREAD() {
    if(id) {
        CloseHandle((HANDLE)id);
        id = 0;
    }
}
unsigned WINAPI GT_ThreadStartPoint(void *arg) {
    GT_sTHREAD_ARG *del_tr = (GT_sTHREAD_ARG *)arg;
    del_tr->fun(del_tr->tr, del_tr->arg, del_tr->arg2);
    GNg::GN_UserThreadState.aUsThreadTable.del(del_tr);
    return 0;
}
GN_SYS_API P<GN_sTHREAD> GN_ThreadStart(void (*proc)(P<GN_sTHREAD> this_thread, void *arg, void *arg2), void *_arg, void *_arg2) {
    P<GN_sTHREAD> p_tr(1);
    GT_sTHREAD_ARG *del_tr = GNg::GN_UserThreadState.aUsThreadTable.add(GT_sTHREAD_ARG(p_tr, _arg, _arg2, proc));
    unsigned pid;
    HANDLE porc = (HANDLE)::_beginthreadex(0, 0, GT_ThreadStartPoint, del_tr, 0 /*CREATE_SUSPENDED*/, &pid);
    if(porc) {
        //SetThreadPriority(porc, THREAD_PRIORITY_TIME_CRITICAL);	//设为高优先级
        //ResumeThread(porc);
        p_tr->id = porc;
        return p_tr;
    } else {
        GNg::GN_UserThreadState.aUsThreadTable.del(del_tr);
        return P_();
    }
}
GN_SYS_API bool GN_ThreadEndWait(const P<GN_sTHREAD> &pt) {
#ifdef GN_DEBUG
    if(!pt) { GN_Log("GN_ThreadEndWait parame is empty!\n"); }
#endif
    if(::WaitForSingleObject((HANDLE)(pt->id), INFINITE) == WAIT_FAILED) return false;
    else return true;
}
GN_SYS_API void GT_ThreadExit() {
    ::ExitThread(0);
}

GN_SYS_API void *GN_ThreadCondCreate() {
    return CreateEvent(0, 1, 1, "prl");
}
GN_SYS_API void GN_ThreadCondRemove(void *cond) {
    CloseHandle(cond);
}
GN_SYS_API void GN_ThreadCondWait(volatile void *cond) {
    WaitForSingleObject((HANDLE)cond, INFINITE);
}
GN_SYS_API void GN_ThreadCondCtrl(void *cond, uint8 state) {
    if(state) SetEvent(cond);
    else ResetEvent(cond);
}
#ifdef __LP64__
GN_SYS_API void GN_MemBarrier() {
    MemoryBarrier();
}
#endif
#else
GN_sTHREAD::~GN_sTHREAD() {
    id = 0;
}

void *GT_ThreadStartPoint(void *arg) {
    GT_sTHREAD_ARG *del_thread = (GT_sTHREAD_ARG *)arg;
    del_thread->fun(del_thread->tr, del_thread->arg, del_thread->arg2);
#ifdef GT_USING_ANDROID_JNI
    if(GT_JEnv) {
        GT_Jvm->DetachCurrentThread();
        GT_JEnv = 0;
    }
#endif
    GNg::GN_UserThreadState.aUsThreadTable.del(del_thread);
    return 0;
}
GN_SYS_API P<GN_sTHREAD> GN_ThreadStart(void (*proc)(P<GN_sTHREAD> this_thread, void *arg, void *arg2), void *_arg, void *_arg2) {
    void *proc_id;
    P<GN_sTHREAD> p_thread(1);
    GT_sTHREAD_ARG *del_thread = GNg::GN_UserThreadState.aUsThreadTable.add(GT_sTHREAD_ARG(p_thread, _arg, _arg2, proc));
    int ret = pthread_create((pthread_t *)&proc_id, 0, GT_ThreadStartPoint, del_thread);
    if(ret != -1) {
        /*存在问题:还未设为高优先级*/
        p_thread->id = proc_id;
        return p_thread;
    } else {
        GNg::GN_UserThreadState.aUsThreadTable.del(del_thread);
        return P_();
    }
}
GN_SYS_API bool GN_ThreadEndWait(const P<GN_sTHREAD> &pt) {
#ifdef GN_DEBUG
    if(!pt) { GN_Log("GN_ThreadEndWait parame is empty!\n"); }
#endif
    if(pthread_join(*((pthread_t *)&(pt->id)), 0)) return false;
    else return true;
}
GN_SYS_API void GT_ThreadExit() {
    pthread_exit(0);
}

struct GT_sTRCOND {
    pthread_mutex_t mutex;
    pthread_cond_t condine;
    volatile uint8 state;

    GT_sTRCOND() : mutex(PTHREAD_MUTEX_INITIALIZER), condine(PTHREAD_COND_INITIALIZER), state(0) {}
};
GN_SYS_API void *GN_ThreadCondCreate() {
    return new GT_sTRCOND;
}
GN_SYS_API void GN_ThreadCondRemove(void *cond) {
    delete((GT_sTRCOND *)cond);
}
GN_SYS_API void GN_ThreadCondWait(volatile void *cond) {
    GT_sTRCOND *_cond = (GT_sTRCOND *)cond;
    pthread_mutex_lock(&(_cond->mutex));
    while(!_cond->state) {
        pthread_cond_wait(&(_cond->condine), &(_cond->mutex));
    }
    pthread_mutex_unlock(&(_cond->mutex));
}
GN_SYS_API void GN_ThreadCondCtrl(void *cond, uint8 state) {
    ((GT_sTRCOND *)cond)->state = state;
    GN_MemBarrier();
    pthread_cond_broadcast(&(((GT_sTRCOND *)cond)->condine));
}
#if (defined _LINUX_ && defined GT_USING_GLes) || defined(_MAC_)
GN_SYS_API void GN_MemBarrier() {
#if __cplusplus >= 201103L
    std::atomic_thread_fence(std::memory_order_seq_cst);
#else
    __sync_synchronize();
#endif
}
#endif
#endif
