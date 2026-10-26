#ifndef _GN_PARALLEL_H_
#define _GN_PARALLEL_H_
/*--------------------------------------------------------- 多线程任务管理 ---------------------------------------------------------------------------------------------------------*/
struct GN_sTASK_BASE;
namespace GN {
template <class T, class T2>
struct sLIST_WHEEL_MT : public GT_sLIST_WHEEL_AT<T, T2> //★列表轮 首尾相连的数据结构 兼容多线程同时操作版本 !用固定的容器空间轮寻来模拟连表操作 优点:与普通数组操作效率相当, 缺点:没真正连表的动态增长, 删减
{
    virtual void zInit() {
        GT_sLIST_WHEEL_AT<T, T2>::zInit();
        this->zpAddOperate = GN_MemBarrier;
    } //多线程版本附加操作为线程间内存同步

    T2 Count(); //获得实存数据个数
    T2 Bid() { return this->bid; } //获得后向存取位置

    bool try_pop_front(T &v); //尝试取出正被其它线程逆向处理时的首个数据( 值输出 ) 返回是否成功          逆向处理操作包括: pop_back,  pop_back__,  pop_rbegin, discd_back
    bool try_pop_back(T &v); //尝试取出正被其它线程顺向处理时的尾个数据( 值输出 ) 返回是否成功          顺向处理操作包括: pop_front, pop_front__, pop_begin,  discd_front
    T *try_pop_begin(); //尝试取出正被其它线程逆向处理时的首个数据     成功返回它的指针 失败返回0  逆向处理操作包括: pop_back,  pop_back__,  pop_rbegin, discd_back
    T *try_pop_rbegin(); //尝试取出正被其它线程顺向处理时的倒数首个数据 成功返回它的指针 失败返回0  顺向处理操作包括: pop_front, pop_front__, pop_begin,  discd_front
};
template <class T, class T2>
T2 sLIST_WHEEL_MT<T, T2>::Count() {
    return this->fid < this->bid ? this->bid - this->fid : 0;
}

template <class T, class T2>
bool sLIST_WHEEL_MT<T, T2>::try_pop_front(T &v) {
    if(Count() < 3) return false;
    if(this->Has()) this->discd_front();
    else return false;
    if(this->fid > this->bid) return false;
    if(this->Has()) {
        v = *(this->REnd());
        return true;
    } else --(this->fid);
    return false;
}
template <class T, class T2>
bool sLIST_WHEEL_MT<T, T2>::try_pop_back(T &v) {
    if(Count() < 3) return false;
    if(this->Has()) this->discd_back();
    else return false;
    if(this->fid > this->bid) return false;
    if(this->Has()) {
        v = *(this->End());
        return true;
    } else ++(this->bid);
    return false;
}
template <class T, class T2>
T *sLIST_WHEEL_MT<T, T2>::try_pop_begin() {
    if(Count() < 3) return 0;
    if(this->Has()) this->discd_front();
    else return 0;
    if(this->fid > this->bid) return 0;
    if(this->Has()) return this->REnd();
    else --(this->fid);
    return 0;
}
template <class T, class T2>
T *sLIST_WHEEL_MT<T, T2>::try_pop_rbegin() {
    if(Count() < 3) return 0;
    if(this->Has()) this->discd_back();
    else return 0;
    if(this->fid > this->bid) return 0;
    if(this->Has()) return this->End();
    else ++(this->bid);
    return 0;
}
};
template <class T, class T2>
struct GT_sLIST_WHEEL_MT_LIST : public GN::sLIST_WHEEL_MT<T, T2> //列表轮列表 提供存取各在不同线程高速操作且不限容量空间
{
    GT_sLIST_WHEEL_MT_LIST *next_node; //指向下个节点

    //初始化
    GT_sLIST_WHEEL_MT_LIST() : next_node(0) {}

    //初始化(为列表轮预分配多少个数据空间
    GT_sLIST_WHEEL_MT_LIST(T2 _len) : next_node(0) {
        this->data = 0;
        GN::sLIST_WHEEL_MT<T, T2>::New(_len);
    }
};

typedef GT_sLIST_WHEEL_MT_LIST<P<GN_sTASK_BASE>, unsigned int> GN_sTASK_LIST_NODE; //任务列表节点

template <class T, class LWL>
//在列表轮列表尾部存入一个数据(列表轮列表指针, 值)
void GN_ListWheelListPushBack(LWL *&plwl, T &v) {
    LWL *_task_list = plwl;
    if(!_task_list) {
        LWL *new_list = new LWL(32);
        GT_MX_PushFront(_task_list, new_list);
        plwl = _task_list;
    }
    for(; _task_list->next_node; _task_list = _task_list->next_node); //找到末尾节点

    if(!_task_list->push_back(v)) {
        LWL *new_list = new LWL(32);
        new_list->push_back(v);
        _task_list->next_node = new_list;
    }
    GN_MemBarrier();
}
template <class T, class LWL>
//执行处理掉列表轮列表首部的一个数据(列表轮列表指针, 帧间隔时间)
void GN_ListWheelListProc(LWL *&plwl, unsigned int nTime) {
GT_GOTO_ListWheelListProc:
    LWL *_task_list = plwl;
    if(_task_list) {
        if(_task_list->Has()) {
            T *_task = _task_list->Begin();
            if((*_task)->Run()) {
                (*_task).Delete();
                _task_list->discd_front();
            }
        } else {
            plwl = _task_list->next_node;
            GN_MemBarrier();
            delete _task_list;
            goto GT_GOTO_ListWheelListProc;
        }
    }
}
template <class LWL>
//删除整串列表轮列表(列表轮列表指针)
void GN_ListWheelListClear(LWL *&plwl) {
    if(plwl) {
        for(;;) {
            LWL *_vm_list = plwl;
            if(_vm_list->next_node) {
                plwl = _vm_list->next_node;
                GN_MemBarrier();
                delete _vm_list;
            } else break;
        }
        delete plwl;
        plwl = 0;
    }
}

//加入一个任务到闲时执行(任务, 是否使用副线程执行) !用这个可以发送任务到其它线程执行 但不能在多个线程竞争调用
GN_API void GN_TaskPushToIdleExec(P<GN_sTASK_BASE> task, bool thread_b);
//闲时执行处理(帧间隔时间, 是否副线程执行)
GN_API void GN_TaskIdleProc(unsigned int nTime, bool thread_b);
//清除所有闲时任务记录
GN_API void GN_TaskIdleClearAll();

struct GN_API GN_sDATA_SGM //数据段标记
{
    unsigned int st; //数据起始点
    unsigned int end; //数据结束点
    unsigned int pid; //处理号 - 如果系统支持多核多线程 这个可用来区分数据是属于哪个处理线程的

    GN_sDATA_SGM() {}
    GN_sDATA_SGM(unsigned int _st, unsigned int _end, unsigned int _pid) : st(_st), end(_end), pid(_pid) {}

    void Reset(); //重设 st = end = 0;
    void SetSE(unsigned int _st, unsigned int _end); //设置数据起始和结束点
    void SetSt(unsigned int _st); //设置数据起始点
    void SetEnd(unsigned int _end); //设置数据结束点
    unsigned int GetSt(); //获得数据起始点
    unsigned int GetEnd(); //获得数据结束点
    unsigned int Count(); //获得段数据个数
};
struct GN_API GN_sTASK_BASE //任务基类
{
    virtual ~GN_sTASK_BASE() {}
    virtual bool Run() { return true; } //运行任务()返回是否可以丢弃该任务
};
struct GN_API GN_sTIME_EXEC_TASK : public GN_sTASK_BASE //★定时执行任务
{
    unsigned int exec_time; //任务执行的运行时间
    void (*proc)(void *arg); //任务的处理函数
    void *usarg; //处理参数

    //初始化(任务执行的运行时间, 任务的处理函数, 处理参数)
    inline void Init(unsigned int _exec_time, void (*_proc)(void *), void *_usarg) {
        exec_time = _exec_time;
        proc = _proc;
        usarg = _usarg;
    }
    virtual bool Run(); //运行任务
};
struct GN_API GN_sRUN_TASK //运行任务
{
    void (*proc)(void *arg, void *arg2); //任务的处理函数
    void *arg_1; //处理参数1
    void *arg_2; //处理参数2

    GN_sRUN_TASK() {}
    GN_sRUN_TASK(void (*_proc)(void *arg, void *arg2), void *_arg_1, void *_arg_2) : proc(_proc), arg_1(_arg_1), arg_2(_arg_2) {}
    bool operator==(const GN_sRUN_TASK &src) const { return (proc == src.proc && arg_1 == src.arg_1 && arg_2 == src.arg_2); }

    void Run(); //运行任务
};
/* ----------------------------------------------------------------------------------------------------------------------------------------------------- */
template <class T, class T2, class T3>
struct GT_s_PROC_ARG //处理参数
{
    T3 (T::*func)(T2);
    T *arg;

    GT_s_PROC_ARG() {}
    GT_s_PROC_ARG(T3 (T::*_func)(T2), T *_arg) : func(_func), arg(_arg) {}
};
template <class T, class F>
//数据段排序(排序任务, 分发处理数据段标记)
void GT_DataSegmentSort(void *sort_arg, const GN_sDATA_SGM *ds) {
    GN_sRUN_TASK *tk = (GN_sRUN_TASK *)sort_arg;
    T *first = (T *)(tk->arg_1) + ds->st;
    F cmp_func = (F)(tk->proc);
    GN_Sort(T, first, ds->end - ds->st, cmp_func);
}
void GT_ParallelWaitProc(P<GN_sTHREAD> this_thread, void *arg, void *arg2); //内部自调用的等候任务处理
/* ----------------------------------------------------------------------------------------------------------------------------------------------------- */

class GN_API GN_cPARALLEL //★并行处理  提供高性能并行处理计算 和 高效的简短子任务运行机制 !!该类的实例一般一个就够了, 定义太多处理性能不升反降 !!!该类的注意事项: 1.加入处理的子任务最好不带死循环或长期处于等待的 这样会严重拖慢整个并行性能  2.子任务也不能太过简单或处理量太少 这样的计算倒不如直接单核计算快 因为该类调度任务和分割处理也需要些小性能开销
{
    friend void ::GT_ParallelWaitProc(P<GN_sTHREAD> this_thread, void *arg, void *arg2);

    static unsigned int zTn; //处理线程数
    static unsigned int zTn_; //处理副线程数
    static unsigned int zSortMinNum; //启用并行排序的最小参考数

    unsigned int zTid; //当前插入任务列表号
    P<P<GN_sTHREAD>> zTrs; //处理副线程组
    GN_sTHREAD **zTrs_; //深入指向处理副线程组的指针, 用以提高处理速度
    GN::sLIST_WHEEL_MT<GN_sRUN_TASK, unsigned int> *zTaskLists; //对应每个副线程的处理任务列表
    GN_sDATA_SGM *zDataSeg; //对应每个线程的处理数据段标记

    void *zEvent; //通知副线如何处理

    void *zUserArg; //用于传递用户处理参数
    void (*zUserProc)(void *user_arg, const GN_sDATA_SGM *ds); //用于传递用户处理函数

    unsigned int zSleepTime; //催眠时间

    void zRunProc(GN_sDATA_SGM *dsg); //运行用户处理(数据段)
    void zSetSleepTime(unsigned int ms); //设置催眠时间
    unsigned int zGetSleepTime(); //获得最近设置的催眠时间

    //获得比较空闲的任务队列号
    unsigned int zGetIdleTaskListID() {
#define GT_TMP_PARALLEL_FIND_TASK_LIST(CMP)             \
    unsigned int ti = 1, tn = zTn_, id = 0;             \
    unsigned int _count = zTaskLists->Count(), _count1; \
    for(; ti < tn; ++ti) {                              \
        _count1 = zTaskLists[ti].Count();               \
        if(_count1 CMP _count) {                        \
            _count = _count1;                           \
            id = ti;                                    \
        }                                               \
    }                                                   \
    return id

        GT_TMP_PARALLEL_FIND_TASK_LIST(<);
    }
    //获得比较忙的任务队列号
    unsigned int zGetBusyTaskListID() {
        GT_TMP_PARALLEL_FIND_TASK_LIST(>);
    }
    //检测指定任务队列是否正在运行(任务队列号)
    bool zTaskListIsRuning(unsigned int tid);

    //检测指定并行分处理是否正在运行(任务标记号)
    bool zProcIsRuning(unsigned int tid);

    //检测指定副线程是否在忙(副线程号)
    bool zThreadIsBusy(unsigned int tid);

    //帮助处理副线程未完成的任务
    void zTaskHelp();

    //强制终止副线程未完成的任务
    void zTaskExit();

  public:
    void *aUserArgL; //保留给用户用的, 最常用的就是保存一定周期内不变的参数

    GN_cPARALLEL() : zTrs_(0), zTaskLists(0), zDataSeg(0), zEvent(0) {}
    ~GN_cPARALLEL() { this->Shutdown(); }

    void StartUp(); //启动 - 启动以后系统开销会增加
    void Shutdown(); //停止关闭

    void On(); //临时开并行处理
    void Off(); //临时关并行处理

    void Sleep(unsigned int ms); //让内部所有副线程暂时休息一会(休息毫秒数)

    void TaskListClear(bool safe_check = true); //清除一下任务队列残余的内容以便还有空间来调用 TaskPush ...(是否检测将近满才清楚 否侧直接清除)  通常调用了 TaskPush 20亿次后 临时缓冲已满就需要调用这个 或者您可以在大量调用 TaskPush 之前调用一下这个

    //加入一个运行任务用副线程来执行( 任务执行函数(传回给你的参数, 传回给你的参数2) , 传给执行函数用的参数, 用的参数2) !调用后通常马上返回, 但如果内部任务队列已满或当前系统不支持多线程, 会在此执行完才返回  !适合一些逻辑互不相同的大量处理 调用较方便
    void TaskPush(void (*proc)(void *arg, void *arg2), void *_arg, void *_arg2 = 0);

    //加入一个运行任务用副线程来执行( 运行任务 ) !调用后通常马上返回, 但如果内部任务队列已满或当前系统不支持多线程, 会在此执行完才返回  !适合一些逻辑互不相同的大量处理 调用较方便
    void TaskPush(GN_sRUN_TASK &task);

    //加入一个较急运行任务用副线程来执行( 任务执行函数(传回给你的参数, 传回给你的参数2) , 传给执行函数用的参数, 用的参数2) !调用后通常立即返回, 但如果内部任务队列已满或当前系统不支持多线程, 会在此执行完才返回  !适合一些逻辑互不相同的小量处理 调用较方便 由于它不会检测加入到空闲任务队列, 所以不适合连续加入大量任务
    void TaskPushUrgent(void (*proc)(void *arg, void *arg2), void *_arg, void *_arg2 = 0);

    //加入一个较急的运行任务用副线程来执行( 运行任务 ) !调用后通常立即返回, 但如果内部任务队列已满或当前系统不支持多线程, 会在此执行完才返回  !适合一些逻辑互不相同的小量处理 调用较方便 由于它不会检测加入到空闲任务队列, 所以不适合连续加入大量任务
    void TaskPushUrgent(GN_sRUN_TASK &task);

    //帮助处理副线程未完成的任务     配合 TaskPush 用
    void TaskHelp() {
        if(zDataSeg) zTaskHelp();
    }

    //强制终止副线程未完成的任务
    void TaskExit() {
        if(zDataSeg) zTaskExit();
    }

    //检测还有没有未运行完的任务   配合 TaskPush 用
    bool TaskHas() {
        if(zDataSeg) {
            unsigned int i = 0, n = zTn_;
            for(; i < n; ++i) {
                if(zTaskListIsRuning(i)) return true;
            }
        }
        return false;
    }

    //等待之前加入的任务运行完成   配合 TaskPush 用
    void TaskEndWait() {
        if(zDataSeg) {
            unsigned int i = 0, n = zTn_;
            for(; i < n; ++i)
                while((volatile bool)zTaskListIsRuning(i));
        }
    }

    //帮忙处理之前加入的任务并等到所有运行完成   配合 TaskPush 用
    void TaskHelpEnd() {
        if(zDataSeg) {
            zTaskHelp();
            unsigned int i = 0, n = zTn_;
            for(; i < n; ++i)
                while((volatile bool)zTaskListIsRuning(i));
        }
    }

    //帮忙处理之前加入的任务并等到所有运行完成   配合 TaskPush 用
    void TaskExitEnd() {
        if(zDataSeg) {
            zTaskExit();
            unsigned int i = 0, n = zTn_;
            for(; i < n; ++i)
                while((volatile bool)zTaskListIsRuning(i));
        }
    }

    //并行处理( 每个分发的处理函数( 传回给你的参数, 分发处理数据段标记) , 传给执行函数用的参数, 处理数据起始索引, 处理数据结束索引, 数据加判断估算量是完整计算量的多少份之一)  !这会运用所有CPU一起计算处理 适合大量逻辑大概相同计算并且某些达到一定条件会提前结束的处理 数量大和单个代码量大速度效能好 但如果量很少反而不及 TaskPushUrgent 效能好 但如果单个代码量少也不及 ProcArray  !如果调用前还有其他副线程运行任务未完成可能会影响到这里的速度
    void Proc(void (*proc)(void *user_arg, GN_sDATA_SGM *ds), void *_user_arg, unsigned int st, unsigned int end, unsigned int fr = 3);

#define GN_PROC_BEGIN /*处理数据循环准备 配合 Proc 用*/ for(;;) {
#define GN_PROC_END /*处理数据循环结束 配合 Proc 用*/ \
    register unsigned int st1 = ds->st + 1;           \
    if(st1 < ds->GetEnd()) {                          \
        ds->SetSt(st1);                               \
        GN_MemBarrier();                              \
    } else break;                                     \
    }
#define GN_PROC_BREAK /*处理数据循环退出 配合 Proc 用*/ \
    {                                                   \
        ds->end = 0;                                    \
        GN_MemBarrier();                                \
        break;                                          \
    }

    /*   Proc 回调自定义处理函数的用法示例:
    void MyProc(void *user_arg, GN_sDATA_SGM *ds){
	    MY_s_STRUCT *us = (MY_s_STRUCT*)user_arg;
	    GN_PROC_BEGIN
		    unsigned int i = ds->st;
            us->data[i] = us->data2[i] * us->ratio;
			if(us->data[i] < 1.0f)GN_PROC_BREAK
		GN_PROC_END
    }                                                                */
    //并行处理数组( 每个分发的处理函数( 传回给你的参数, 分发处理数据段标记) , 传给执行函数用的参数, 处理数据起始索引, 处理数据结束索引, 是否等齐所有处理)  !这会运用所有CPU一起计算处理 适合大量计算完全相同 计算比较简短的处理 数量大速度效能好 但如果量很少反而不及 TaskPushUrgent 效能好  !如果调用前还有其他副线程运行任务未完成可能会影响到这里的速度
    void ProcArray(void (*proc)(void *user_arg, const GN_sDATA_SGM *ds), void *_user_arg, unsigned int st, unsigned int end, bool wait = true);

/*   ProcArray 回调自定义处理函数的用法示例:
    void MyProc(void *user_arg, const GN_sDATA_SGM *ds){
	    MY_s_STRUCT *us = (MY_s_STRUCT*)user_arg;
	    unsigned int i = ds->st;
	    for( ; i < ds->end; ++i){ //for是必须的 因为有时回调给你的是空段 ds->st == ds->end 如果这时直接用 ds->st 或 do{...}while() 显然会出错
            us->data[i] = us->data2[i] * us->ratio;
	    }
    }                                                                */
#define GN_PROC_ARRAY_RESET_ST(ST) ((GN_sDATA_SGM *)ds)->st = ST; /*强行重设置处理数据段起始标记( 起始ID ) 重设后您可以通过 GetLastProcDataSegmentInfo 获得修改后的信息*/
#define GN_PROC_ARRAY_RESET_END(END) ((GN_sDATA_SGM *)ds)->end = END; /*强行重设置处理数据段结束标记( 结束ID ) 重设后您可以通过 GetLastProcDataSegmentInfo 获得修改后的信息*/

    //试探并行处理数组( 处理数据起始索引, 处理数据结束索引 ) !这个实际不会执行处理 您可以调用这个来预先得知数据段分发状况 配合 GetLastProcDataSegmentInfo 用
    void ProcArrayTry(unsigned int st, unsigned int end);

    //试探获得并行处理数组给定范围的各个分发数据段开始索引(返回各个分发数据段开始索引, 处理数据起始索引, 处理数据结束索引)返回分段数 !参数sts_out需预先分配好空间 您可以调用 GetProcThreadNum 获得准确数量 或 [GN_USING_PARALLEL_MAX] 设置足够的常数空间
    unsigned int ProcArrayStartPointsGetTry(unsigned int *sts_out, unsigned int st, unsigned int end);

    //获得上次调用 ProcArray 或 ProcArrayTry 后的各个数据段分割信息  !!如果系统不支持多线程返回0 返回值是一个数组, 数量为 GetProcThreadNum 可获得
    inline GN_sDATA_SGM *const GetLastProcDataSegmentInfo() { return zDataSeg; }

    //获得上次调用 ProcArray 或 ProcArrayTry 后的各个数据段数据总数
    inline unsigned int GetLastProcDataSegmentNum() {
        unsigned int dn = 0;
        const GN_sDATA_SGM *_dg = zDataSeg, *dg_end = zDataSeg + zTn;
        for(; _dg < dg_end; ++_dg) dn += _dg->end - _dg->st;
        return dn;
    }

    //获得上次调用 ProcArray 或 ProcArrayTry 后的各个数据段分量end值的总和
    inline unsigned int GetLastProcSegmentEndTotalNum() {
        unsigned int dn = 0;
        const GN_sDATA_SGM *_dg = zDataSeg, *dg_end = zDataSeg + zTn;
        for(; _dg < dg_end; ++_dg) dn += _dg->end;
        return dn;
    }

    //获得上次调用 ProcArray 或 ProcArrayTry 后的各个数据段分量 st值的最小
    inline unsigned int GetLastProcSegmentStMinNum() {
        unsigned int dn = 0xffffffff;
        const GN_sDATA_SGM *_dg = zDataSeg, *dg_end = zDataSeg + zTn;
        for(; _dg < dg_end; ++_dg)
            if(_dg->st < dn) dn = _dg->st;
        return dn;
    }

    //获得上次调用 ProcArray 或 ProcArrayTry 后的各个数据段分量end值的最大
    inline unsigned int GetLastProcSegmentEndMaxNum() {
        unsigned int dn = 0;
        const GN_sDATA_SGM *_dg = zDataSeg, *dg_end = zDataSeg + zTn;
        for(; _dg < dg_end; ++_dg)
            if(_dg->end > dn) dn = _dg->end;
        return dn;
    }

    //获得上次调用 ProcArray 或 ProcArrayTry 后的各个数据段分量 st值的总和
    inline unsigned int GetLastProcSegmentStTotalNum() {
        unsigned int dn = 0;
        const GN_sDATA_SGM *_dg = zDataSeg, *dg_end = zDataSeg + zTn;
        for(; _dg < dg_end; ++_dg) dn += _dg->st;
        return dn;
    }

    template <class T>
    //获得指定输入值数据对应处理线程数个值的总和(输入值数据)
    inline T GetProcValueTotalNum(T *values) {
        T total = 0;
        unsigned int di = 0;
        for(; di < zTn; ++di) total += values[di];
        return total;
    }

    //获得并行处理线程数 - 含主线程
    inline unsigned int GetProcThreadNum() { return zTn; }

    //在已启动中临时增加并行处理线程(增加线程数 负值表示减少数)返回成功增加或减少的线程数 !可能由于系统所有处理器被占用时或系统不支持多线程会导致不能成功加入指定的线程数
    int AddProcThread(int tn);

    //检测还有没有未完成的并行处理   配合 Proc... 用
    bool ProcHas() {
        if(zDataSeg) {
            unsigned int i = 0, n = zTn_;
            for(; i < n; ++i) {
                if(zProcIsRuning(i)) return true;
            }
        }
        return false;
    }

    //等齐所有并行处理完成    配合 Proc... 用
    void ProcEndWait() {
        if(zDataSeg) {
            unsigned int i = 0, n = zTn_;
            for(; i < n; ++i) { while((volatile bool)zProcIsRuning(i)); }
        }
    }

    //检测还有没有忙 包括忙着休息
    bool BusyHas() {
        if(zDataSeg) {
            unsigned int i = 0, n = zTn_;
            for(; i < n; ++i) {
                if(zThreadIsBusy(i)) return true;
            }
        }
        return false;
    }

    //等待所有任务和处理和休息结束为止
    void EndWait() {
        if(zDataSeg) {
            unsigned int i = 0, n = zTn_;
            for(; i < n; ++i) {
                while((volatile bool)zThreadIsBusy(i));
                zTaskLists[i].reset();
            }
        }
    }

    template <class T, class T2, class T3>
    //数据段处理(处理参数, 分发处理数据段标记)
    static void GT_DataSegmentProc(void *user_arg, GN_sDATA_SGM *ds) {
        GT_s_PROC_ARG<T, T2, T3> *pa = (GT_s_PROC_ARG<T, T2, T3> *)user_arg;
        T *c = pa->arg;
        T3 (T::*func0)(T2) = pa->func;
        GN_PROC_BEGIN
        if((c->*func0)(ds->st)) GN_PROC_BREAK
        GN_PROC_END
    }
    template <class T>
    //并行处理( 数据封装类的成员处理函数( 告诉你当前处理的是第几个数据)要求返回是否要中断处理 , 数据封装类对象, 处理数据起始索引, 处理数据结束索引, 数据加判断估算量是完整计算量的多少份之一)  !这会运用所有CPU一起计算处理 但调用起来或许会方便一点 适合大量逻辑大概相同的类实例计算并且某些达到一定条件会提前结束的处理 数量大速度效能好 但稍狲Proc 如果量很少反而不及 TaskPushUrgent 效能好
    inline void Proc1(bool (T::*mb_func)(unsigned int id), T *obj, unsigned int st, unsigned int end, unsigned int fr = 3) {
        GT_s_PROC_ARG<T, unsigned int, bool> pa(mb_func, obj);
        Proc(GT_DataSegmentProc<T, unsigned int, bool>, &pa, st, end, fr);
    }

    template <class T, class T2, class F>
    //并行排序数据( 排好的数据输出 , 要排的数据输入 !注意这里的数据顺序也可能受改变, 数据总个数 , 排序比较方法)  !支持多线程系统这个比较快
    void Sort(T *out_array, T *p_array, T2 count, F cmp_func) {
        if(zDataSeg && (unsigned int)count >= zSortMinNum) {
            GN_sRUN_TASK tk((GT_PFUNC_PVOID_PVOID)cmp_func, p_array, 0);
            ProcArray(GT_DataSegmentSort<T, F>, &tk, 0, count, true);
            GN_OrderIntegration(out_array, p_array, count, cmp_func, zDataSeg, zTn);
        } else {
            GN_Sort(T, p_array, count, cmp_func);
            T2 i = 0;
            for(; i < count; ++i) out_array[i] = p_array[i];
        }
    }
    template <class T, class T2, class F>
    //并行排序数据(要排的数据, 数据总个数 , 排序比较方法)   !不支持多线程系统这个比较快
    void Sort(T *p_array, T2 count, F cmp_func) {
        if(zDataSeg && (unsigned int)count >= zSortMinNum) {
            GN_sRUN_TASK tk((GT_PFUNC_PVOID_PVOID)cmp_func, p_array, 0);
            ProcArray(GT_DataSegmentSort<T, F>, &tk, 0, count, false);
            T *out_array = new T[count];
            ProcEndWait();
            T2 i = 0;
            for(; i < count; ++i) out_array[i] = p_array[i];
            GN_OrderIntegration(p_array, out_array, count, cmp_func, zDataSeg, zTn);
            delete[] out_array;
        } else {
            GN_Sort(T, p_array, count, cmp_func);
        }
    }
};
namespace GN {
extern GN_API GN_cPARALLEL Parallel; //定义一个全局并行计算来处理密集型计算
};
//快速整理间中含有空缺的数组使数组成为紧密排列的一整段( 数组 , 数组里实存数据段标记 , 数据段数) 返回整理后该段数据个数 !!适合用于指针数据
GN_API unsigned int GN_ArrayArrange(void **p_array, const GN_sDATA_SGM *const dsg, unsigned int dsg_count);

//快速整理间中含有空缺的数组使数组成为紧密排列的一整段( 数组 , 数组里实存数据段标记 , 数据段数) 返回整理后该段数据个数 !!适合用于占4字节的简单类型
GN_API unsigned int GN_ArrayArrange(unsigned int *p_array, const GN_sDATA_SGM *const dsg, unsigned int dsg_count);

//快速整理间中含有空缺的数组使数组成为紧密排列的一整段( 数组 , 单个数据占字节数 , 数组里实存数据段标记 , 数据段数) 返回整理后该段数据个数 !!适合用于占其它字节的简单类型数据
GN_API unsigned int GN_ArrayArrange(uint8 *p_array, unsigned int tsize, const GN_sDATA_SGM *const dsg, unsigned int dsg_count);

//快速整理间中含有空缺的数组使数组成为紧密排列的一整段( 当需要移动数据段的处理函数(传回给你的参数, 数据段移动到的目标偏移位置, 移动采集源偏移位置, 共移动几个数据), 传给执行函数用的参数 , 数组里实存数据段标记 , 数据段数) 返回整理后该段数据个数 !!适合用于更灵活更复杂的数据类型 !如果觉得自定回调麻烦 默认提供了 GN_ArraySeg...一系列模板函数满足常规处理 指定这些回调时可以用(GT_PFUNC_PVOID_UINT3)来强制转换到适配类型
GN_API unsigned int GN_ArrayArrange(void (*proc)(void *user_arg, unsigned int dst_i, unsigned int src_i, unsigned int count), void *_user_arg, const GN_sDATA_SGM *const dsg, unsigned int dsg_count);

template <class T>
//快速整理间中含有空缺的数组使数组成为紧密排列的一整段( 数组 , 数组里实存数据段标记 , 数据段数) 返回整理后该段数据个数 !!适合于带复制赋值重载运算符的结构体或类数据
unsigned int GN_ArrayArrange(T *p_array, const GN_sDATA_SGM *const dsg, unsigned int dsg_count) {
#define GT_TMP_ARRAY_ARRANGE(COPY_PROC1, COPY_PROC2)                        \
    if(dsg_count < 2) return dsg->end - dsg->st;                            \
    unsigned int di = 0, di2 = dsg_count - 1;                               \
    unsigned int end = dsg[di].end, st2 = dsg[di2].st, end2 = dsg[di2].end; \
    unsigned int en = dsg[di + 1].st - end, sn = end2 - st2;                \
    for(;;) {                                                               \
        if(en < sn) {                                                       \
            if(en) {                                                        \
                end2 -= en;                                                 \
                COPY_PROC1                                                  \
            }                                                               \
            ++di;                                                           \
            if(di == di2) return end2 - dsg->st;                            \
            sn -= en;                                                       \
            end = dsg[di].end;                                              \
            en = dsg[di + 1].st - end;                                      \
        } else {                                                            \
            if(sn) {                                                        \
                COPY_PROC2                                                  \
            }                                                               \
            --di2;                                                          \
            if(en == sn) {                                                  \
                if(di == di2) return st2 - dsg->st;                         \
                ++di;                                                       \
                if(di == di2) return dsg[di2].end - dsg->st;                \
                end = dsg[di].end;                                          \
                en = dsg[di + 1].st - end;                                  \
            } else {                                                        \
                end += sn;                                                  \
                if(di == di2) return end - dsg->st;                         \
                en -= sn;                                                   \
            }                                                               \
            st2 = dsg[di2].st;                                              \
            end2 = dsg[di2].end;                                            \
            sn = end2 - st2;                                                \
        }                                                                   \
    }
    T *p_array1, *p_array2;
    GT_TMP_ARRAY_ARRANGE(p_array1 = p_array + end; p_array2 = p_array + end2; GN_SetArrayFromArray(p_array1, p_array2, en);, p_array1 = p_array + end; p_array2 = p_array + st2; GN_SetArrayFromArray(p_array1, p_array2, sn);)
}
//整理间中含有空缺的数组使数组成为紧密排列的一整段并保持原顺序不变( 数组 , 数组里实存数据段标记 , 数据段数) 返回整理后该段数据个数 !!适合用于指针数据
GN_API unsigned int GN_ArrayArrangeKeepOrder(void **p_array, const GN_sDATA_SGM *const dsg, unsigned int dsg_count);

//整理间中含有空缺的数组使数组成为紧密排列的一整段并保持原顺序不变( 数组 , 数组里实存数据段标记 , 数据段数) 返回整理后该段数据个数 !!适合用于占4字节的简单类型
GN_API unsigned int GN_ArrayArrangeKeepOrder(unsigned int *p_array, const GN_sDATA_SGM *const dsg, unsigned int dsg_count);

//整理间中含有空缺的数组使数组成为紧密排列的一整段并保持原顺序不变( 数组 , 单个数据占字节数 , 数组里实存数据段标记 , 数据段数) 返回整理后该段数据个数 !!适合用于占其它字节的简单类型数据
GN_API unsigned int GN_ArrayArrangeKeepOrder(uint8 *p_array, unsigned int tsize, const GN_sDATA_SGM *const dsg, unsigned int dsg_count);

//整理间中含有空缺的数组使数组成为紧密排列的一整段并保持原顺序不变( 当需要移动数据段的处理函数(传回给你的参数, 数据段移动到的目标偏移位置, 移动采集源偏移位置, 共移动几个数据), 传给执行函数用的参数 , 数组里实存数据段标记 , 数据段数) 返回整理后该段数据个数 !!适合用于更灵活更负复杂的数据类型 !如果觉得自定回调麻烦 默认提供了 GN_ArraySeg...一系列模板函数满足常规处理 指定这些回调时可以用(GT_PFUNC_PVOID_UINT3)来强制转换到适配类型
GN_API unsigned int GN_ArrayArrangeKeepOrder(void (*proc)(void *user_arg, unsigned int dst_i, unsigned int src_i, unsigned int count), void *_user_arg, const GN_sDATA_SGM *const dsg, unsigned int dsg_count);

template <class T>
//整理间中含有空缺的数组使数组成为紧密排列的一整段并保持原顺序不变( 数组 , 数组里实存数据段标记 , 数据段数) 返回整理后该段数据个数 !!适合于带复制赋值重载运算符的结构体或类数据
unsigned int GN_ArrayArrangeKeepOrder(T *p_array, const GN_sDATA_SGM *const dsg, unsigned int dsg_count) {
#define GT_TMP_ARRAY_ARRANGE_KEEP_ORDER(COPY_PROC)                        \
    unsigned int end = dsg->end, di = 1;                                  \
    for(; di < dsg_count && end == dsg[di].st; ++di) end = dsg[di].end;   \
    if(di >= dsg_count) return end - dsg->st;                             \
    unsigned int set, end2, cn;                                           \
    GT_GOTO_ArrayArrangeKeepOrderStart:                                   \
    set = dsg[di].st;                                                     \
    GT_GOTO_ArrayArrangeKeepOrderNextSgm:                                 \
    end2 = dsg[di++].end;                                                 \
    if(di < dsg_count) {                                                  \
        if(end2 == dsg[di].st) goto GT_GOTO_ArrayArrangeKeepOrderNextSgm; \
        else {                                                            \
            cn = end2 - set;                                              \
            if(cn) {                                                      \
                COPY_PROC                                                 \
                end += cn;                                                \
            }                                                             \
            goto GT_GOTO_ArrayArrangeKeepOrderStart;                      \
        }                                                                 \
    }                                                                     \
    cn = end2 - set;                                                      \
    if(cn) {                                                              \
        COPY_PROC                                                         \
        end += cn;                                                        \
    }                                                                     \
    return end - dsg->st;

    T *p_array1, *p_array2;
    GT_TMP_ARRAY_ARRANGE_KEEP_ORDER(p_array1 = p_array + end; p_array2 = p_array + set; GN_SetArrayFromArray(p_array1, p_array2, cn);)
}
//--------------------------------------------- 以下这几个模板函数是方便传给GN_ArrayArrange回调的 -------------------------------------------------
template <class T2>
//调转从目标位置复制一段数据到原位置(数据, 目标起始位置, 源起始位置, 复制个数) 一整段复制, 速度快 !!适合用于指针类型
void GN_ArraySeg_CopyTurnP(void *_data, T2 dst_i, T2 src_i, T2 count) {
#ifdef __LP64__
#define GT_PTR_SIZE << 1
#define GT_PTR_SIZE_OFS 3
#else
#define GT_PTR_SIZE
#define GT_PTR_SIZE_OFS 2
#endif
    void **data = (void **)_data;
    memcpy(data + src_i, data + dst_i, count << GT_PTR_SIZE_OFS);
}
template <class T2>
//调转从目标位置复制一段数据到原位置(数据, 目标起始位置, 源起始位置, 复制个数) 一整段复制, 速度快 !!适合用于占4字节的简单类型
void GN_ArraySeg_CopyTurn4(void *_data, T2 dst_i, T2 src_i, T2 count) {
    unsigned int *data = (unsigned int *)_data;
    memcpy(data + src_i, data + dst_i, count << 2);
}
template <class T, class T2>
//调转从目标位置复制一段数据到原位置(数据, 目标起始位置, 源起始位置, 复制个数) 一整段复制, 速度快 !!适合用于占其它字节的简单类型数据或指针数据
void GN_ArraySeg_CopyTurn_(void *_data, T2 dst_i, T2 src_i, T2 count) {
    T *data = (T *)_data;
    memcpy(data + src_i, data + dst_i, count * sizeof(T));
}
template <class T, class T2>
//调转从目标位置复制一段数据到原位置(数据, 目标起始位置, 源起始位置, 复制个数) 一个个地复制 可完美兼容带复制赋值重载运算符的结构体或类数据
void GN_ArraySeg_CopyTurn(void *data, T2 dst_i, T2 src_i, T2 count) {
    T *p_array1 = ((T *)data) + src_i, *p_array2 = ((T *)data) + dst_i;
    GN_SetArrayFromArray(p_array1, p_array2, count);
}
template <class T, class T2>
//从原位置复制一段数据到目标位置(数据, 目标起始位置, 源起始位置, 复制个数) 一个个地复制 可完美兼容带复制赋值重载运算符的结构体或类数据
void GN_ArraySeg_Copy(void *data, T2 dst_i, T2 src_i, T2 count) {
    T *p_array1 = ((T *)data) + dst_i, *p_array2 = ((T *)data) + src_i;
    GN_SetArrayFromArray(p_array1, p_array2, count);
}
template <class T, class T2>
//交换指定的两段数据并保存原顺序不变(数据, 数据段1起始位置, 数据段2起始位置, 交换个数) 一个个地交换 可完美兼容带复制赋值重载运算符的结构体或类数据
void GN_ArraySeg_SwapKeepOrder(void *data, T2 i1, T2 i2, T2 count) {
    T *p_array1 = ((T *)data) + i1, *p_array2 = ((T *)data) + i2;
    GN_SwapArrayKeepOrder(p_array1, p_array2, count, T)
}
//--------------------------------------------------------------------------------------------------------------------------------------------------
template <class T2>
//调转从目标位置复制一段数据到原位置(数据, 目标起始位置, 源起始位置, 复制个数) 一整段复制, 速度快 !!适合用于指针类型
void GN_ArraySegCopyTurnP(void **data, T2 dst_i, T2 src_i, T2 count) {
    memcpy(data + src_i, data + dst_i, count << GT_PTR_SIZE_OFS);
}
template <class T2>
//调转从目标位置复制一段数据到原位置(数据, 目标起始位置, 源起始位置, 复制个数) 一整段复制, 速度快 !!适合用于占4字节的简单类型
void GN_ArraySegCopyTurn4(unsigned int *data, T2 dst_i, T2 src_i, T2 count) {
    memcpy(data + src_i, data + dst_i, count << 2);
}
template <class T, class T2>
//调转从目标位置复制一段数据到原位置(数据, 目标起始位置, 源起始位置, 复制个数) 一整段复制, 速度快 !!适合用于占其它字节的简单类型数据或指针数据
void GN_ArraySegCopyTurn_(T *data, T2 dst_i, T2 src_i, T2 count) {
    memcpy(data + src_i, data + dst_i, count * sizeof(T));
}
template <class T, class T2>
//调转从目标位置复制一段数据到原位置(数据, 目标起始位置, 源起始位置, 复制个数) 一个个地复制 可完美兼容带复制赋值重载运算符的结构体或类数据
void GN_ArraySegCopyTurn(T *data, T2 dst_i, T2 src_i, T2 count) {
    T *p_array1 = data + src_i, *p_array2 = data + dst_i;
    GN_SetArrayFromArray(p_array1, p_array2, count);
}
template <class T, class T2>
//从原位置复制一段数据到目标位置(数据, 目标起始位置, 源起始位置, 复制个数) 一个个地复制 可完美兼容带复制赋值重载运算符的结构体或类数据
void GN_ArraySegCopy(T *data, T2 dst_i, T2 src_i, T2 count) {
    T *p_array1 = data + dst_i, *p_array2 = data + src_i;
    GN_SetArrayFromArray(p_array1, p_array2, count);
}
template <class T, class T2>
//交换指定的两段数据并保存原顺序不变(数据, 数据段1起始位置, 数据段2起始位置, 交换个数) 一个个地交换 可完美兼容带复制赋值重载运算符的结构体或类数据
void GN_ArraySegSwapKeepOrder(T *data, T2 i1, T2 i2, T2 count) {
    T *p_array1 = data + i1, *p_array2 = data + i2;
    GN_SwapArrayKeepOrder(p_array1, p_array2, count, T)
}
//--------------------------------------------------------------------------------------------------------------------------------------------------
template <class T, class T2, class F>
//有序的数据段整合( 完整顺序的数据输出 , 分段有序的数据输入 , 数据总个数 , 排序比较方法 , 数据段标记 , 数据段数 )  !该函数通常用于整合通过用并行处理来分段排序后的数据 如输入:{ 4,8,11,15, 16,17,18,43, 6,7,12,56, 1,9,88,97 } 输出 { 1,4,6,7, 8,9,11,12, 15,16,17,18, 43,56,88,97 }
void GN_OrderIntegration(T *out_array, const T *p_array, T2 count, F cmp_func, const GN_sDATA_SGM *const dsg, T2 dsg_count) {
    GN_sDATA_SGM *_dsg = (GN_sDATA_SGM *)dsg;
    T *_p_array = (T *)p_array;
    T *pP_array[GN_USING_PARALLEL_MAX + GN_USING_PARALLEL_MAX];
    T **pp_array;
    T **end_array;
    if(dsg_count > GN_USING_PARALLEL_MAX) {
        pp_array = new T *[dsg_count * 2];
        end_array = pp_array + dsg_count;
    } else {
        pp_array = pP_array;
        end_array = pp_array + GN_USING_PARALLEL_MAX;
    }
    T2 di;
    for(di = 0; di < dsg_count; ++di) {
        pp_array[di] = _p_array + _dsg[di].st;
        end_array[di] = _p_array + _dsg[di].end;
    }
    T2 i = 0, n = (T2)(count), mi;
    for(; i < n; ++i) {
        mi = 0;
        out_array[i] = *(pp_array[0]);
        for(di = 1; di < dsg_count; ++di) {
            if(cmp_func(*(pp_array[di]), out_array[i])) {
                out_array[i] = *(pp_array[di]);
                mi = di;
            }
        }
        ++(pp_array[mi]);
        if(pp_array[mi] >= end_array[mi]) { /*一行检测完*/
            T2 dn_ = dsg_count - 1;
            if(mi != dn_) {
                pp_array[mi] = pp_array[dn_];
                end_array[mi] = end_array[dn_];
            }
            --dsg_count;
            if(!dsg_count) {
#ifdef GN_DEBUG
                if(i < n - 1) { GN_Log("并行排序有问题!\n"); }
#endif
                break;
            }
        }
    }
    if(pp_array != pP_array) delete[] pp_array;
}
#define GT_PROC_ARG_DEF2(T1, ARG1, T2, ARG2) \
    T1 ARG1;                                 \
    T2 ARG2;
#define GT_PROC_ARG_SND2(T1, ARG1, T2, ARG2) T1 ARG1##_, T2 ARG2##_
#define GT_PROC_ARG_SET2(T1, ARG1, T2, ARG2) ARG1(ARG1##_), ARG2(ARG2##_)
#define GT_PROC_ARG_RE2(T1, ARG1, T2, ARG2) \
    T1 ARG1 = args->ARG1;                   \
    T2 ARG2 = args->ARG2;
#define GT_PROC_ARG_DEF3(T1, ARG1, T2, ARG2, T3, ARG3) \
    T1 ARG1;                                           \
    T2 ARG2;                                           \
    T3 ARG3;
#define GT_PROC_ARG_SND3(T1, ARG1, T2, ARG2, T3, ARG3) T1 ARG1##_, T2 ARG2##_, T3 ARG3##_
#define GT_PROC_ARG_SET3(T1, ARG1, T2, ARG2, T3, ARG3) ARG1(ARG1##_), ARG2(ARG2##_), ARG3(ARG3##_)
#define GT_PROC_ARG_RE3(T1, ARG1, T2, ARG2, T3, ARG3) \
    T1 ARG1 = args->ARG1;                             \
    T2 ARG2 = args->ARG2;                             \
    T3 ARG3 = args->ARG3;
#define GT_PROC_ARG_DEF4(T1, ARG1, T2, ARG2, T3, ARG3, T4, ARG4) \
    T1 ARG1;                                                     \
    T2 ARG2;                                                     \
    T3 ARG3;                                                     \
    T4 ARG4;
#define GT_PROC_ARG_SND4(T1, ARG1, T2, ARG2, T3, ARG3, T4, ARG4) T1 ARG1##_, T2 ARG2##_, T3 ARG3##_, T4 ARG4##_
#define GT_PROC_ARG_SET4(T1, ARG1, T2, ARG2, T3, ARG3, T4, ARG4) ARG1(ARG1##_), ARG2(ARG2##_), ARG3(ARG3##_), ARG4(ARG4##_)
#define GT_PROC_ARG_RE4(T1, ARG1, T2, ARG2, T3, ARG3, T4, ARG4) \
    T1 ARG1 = args->ARG1;                                       \
    T2 ARG2 = args->ARG2;                                       \
    T3 ARG3 = args->ARG3;                                       \
    ;                                                           \
    T4 ARG4 = args->ARG4;
#define GT_PROC_ARG_DEF5(T1, ARG1, T2, ARG2, T3, ARG3, T4, ARG4, T5, ARG5) \
    T1 ARG1;                                                               \
    T2 ARG2;                                                               \
    T3 ARG3;                                                               \
    T4 ARG4;                                                               \
    T5 ARG5;
#define GT_PROC_ARG_SND5(T1, ARG1, T2, ARG2, T3, ARG3, T4, ARG4, T5, ARG5) T1 ARG1##_, T2 ARG2##_, T3 ARG3##_, T4 ARG4##_, T5 ARG5##_
#define GT_PROC_ARG_SET5(T1, ARG1, T2, ARG2, T3, ARG3, T4, ARG4, T5, ARG5) ARG1(ARG1##_), ARG2(ARG2##_), ARG3(ARG3##_), ARG4(ARG4##_), ARG5(ARG5##_)
#define GT_PROC_ARG_RE5(T1, ARG1, T2, ARG2, T3, ARG3, T4, ARG4, T5, ARG5) \
    T1 ARG1 = args->ARG1;                                                 \
    T2 ARG2 = args->ARG2;                                                 \
    T3 ARG3 = args->ARG3;                                                 \
    T4 ARG4 = args->ARG4;                                                 \
    T5 ARG5 = args->ARG5;
#define GT_PROC_ARG_DEF6(T1, ARG1, T2, ARG2, T3, ARG3, T4, ARG4, T5, ARG5, T6, ARG6) \
    T1 ARG1;                                                                         \
    T2 ARG2;                                                                         \
    T3 ARG3;                                                                         \
    T4 ARG4;                                                                         \
    T5 ARG5;                                                                         \
    T6 ARG6;
#define GT_PROC_ARG_SND6(T1, ARG1, T2, ARG2, T3, ARG3, T4, ARG4, T5, ARG5, T6, ARG6) T1 ARG1##_, T2 ARG2##_, T3 ARG3##_, T4 ARG4##_, T5 ARG5##_, T6 ARG6##_
#define GT_PROC_ARG_SET6(T1, ARG1, T2, ARG2, T3, ARG3, T4, ARG4, T5, ARG5, T6, ARG6) ARG1(ARG1##_), ARG2(ARG2##_), ARG3(ARG3##_), ARG4(ARG4##_), ARG5(ARG5##_), ARG6(ARG6##_)
#define GT_PROC_ARG_RE6(T1, ARG1, T2, ARG2, T3, ARG3, T4, ARG4, T5, ARG5, T6, ARG6) \
    T1 ARG1 = args->ARG1;                                                           \
    T2 ARG2 = args->ARG2;                                                           \
    T3 ARG3 = args->ARG3;                                                           \
    T4 ARG4 = args->ARG4;                                                           \
    T5 ARG5 = args->ARG5;                                                           \
    T6 ARG6 = args->ARG6;
#define GT_PROC_ARG_DEF7(T1, ARG1, T2, ARG2, T3, ARG3, T4, ARG4, T5, ARG5, T6, ARG6, T7, ARG7) \
    T1 ARG1;                                                                                   \
    T2 ARG2;                                                                                   \
    T3 ARG3;                                                                                   \
    T4 ARG4;                                                                                   \
    T5 ARG5;                                                                                   \
    T6 ARG6;                                                                                   \
    T7 ARG7;
#define GT_PROC_ARG_SND7(T1, ARG1, T2, ARG2, T3, ARG3, T4, ARG4, T5, ARG5, T6, ARG6, T7, ARG7) T1 ARG1##_, T2 ARG2##_, T3 ARG3##_, T4 ARG4##_, T5 ARG5##_, T6 ARG6##_, T7 ARG7##_
#define GT_PROC_ARG_SET7(T1, ARG1, T2, ARG2, T3, ARG3, T4, ARG4, T5, ARG5, T6, ARG6, T7, ARG7) ARG1(ARG1##_), ARG2(ARG2##_), ARG3(ARG3##_), ARG4(ARG4##_), ARG5(ARG5##_), ARG6(ARG6##_), ARG7(ARG7##_)
#define GT_PROC_ARG_RE7(T1, ARG1, T2, ARG2, T3, ARG3, T4, ARG4, T5, ARG5, T6, ARG6, T7, ARG7) \
    T1 ARG1 = args->ARG1;                                                                     \
    T2 ARG2 = args->ARG2;                                                                     \
    T3 ARG3 = args->ARG3;                                                                     \
    T4 ARG4 = args->ARG4;                                                                     \
    T5 ARG5 = args->ARG5;                                                                     \
    T6 ARG6 = args->ARG6;                                                                     \
    T7 ARG7 = args->ARG7;
#define GT_PROC_ARG_DEF8(T1, ARG1, T2, ARG2, T3, ARG3, T4, ARG4, T5, ARG5, T6, ARG6, T7, ARG7, T8, ARG8) \
    T1 ARG1;                                                                                             \
    T2 ARG2;                                                                                             \
    T3 ARG3;                                                                                             \
    T4 ARG4;                                                                                             \
    T5 ARG5;                                                                                             \
    T6 ARG6;                                                                                             \
    T7 ARG7;                                                                                             \
    T8 ARG8;
#define GT_PROC_ARG_SND8(T1, ARG1, T2, ARG2, T3, ARG3, T4, ARG4, T5, ARG5, T6, ARG6, T7, ARG7, T8, ARG8) T1 ARG1##_, T2 ARG2##_, T3 ARG3##_, T4 ARG4##_, T5 ARG5##_, T6 ARG6##_, T7 ARG7##_, T8 ARG8##_
#define GT_PROC_ARG_SET8(T1, ARG1, T2, ARG2, T3, ARG3, T4, ARG4, T5, ARG5, T6, ARG6, T7, ARG7, T8, ARG8) ARG1(ARG1##_), ARG2(ARG2##_), ARG3(ARG3##_), ARG4(ARG4##_), ARG5(ARG5##_), ARG6(ARG6##_), ARG7(ARG7##_), ARG8(ARG8##_)
#define GT_PROC_ARG_RE8(T1, ARG1, T2, ARG2, T3, ARG3, T4, ARG4, T5, ARG5, T6, ARG6, T7, ARG7, T8, ARG8) \
    T1 ARG1 = args->ARG1;                                                                               \
    T2 ARG2 = args->ARG2;                                                                               \
    T3 ARG3 = args->ARG3;                                                                               \
    T4 ARG4 = args->ARG4;                                                                               \
    T5 ARG5 = args->ARG5;                                                                               \
    T6 ARG6 = args->ARG6;                                                                               \
    T7 ARG7 = args->ARG7;                                                                               \
    T8 ARG8 = args->ARG8;
#define GT_PROC_ARG_DEF9(T1, ARG1, T2, ARG2, T3, ARG3, T4, ARG4, T5, ARG5, T6, ARG6, T7, ARG7, T8, ARG8, T9, ARG9) \
    T1 ARG1;                                                                                                       \
    T2 ARG2;                                                                                                       \
    T3 ARG3;                                                                                                       \
    T4 ARG4;                                                                                                       \
    T5 ARG5;                                                                                                       \
    T6 ARG6;                                                                                                       \
    T7 ARG7;                                                                                                       \
    T8 ARG8;                                                                                                       \
    T9 ARG9;
#define GT_PROC_ARG_SND9(T1, ARG1, T2, ARG2, T3, ARG3, T4, ARG4, T5, ARG5, T6, ARG6, T7, ARG7, T8, ARG8, T9, ARG9) T1 ARG1##_, T2 ARG2##_, T3 ARG3##_, T4 ARG4##_, T5 ARG5##_, T6 ARG6##_, T7 ARG7##_, T8 ARG8##_, T9 ARG9##_
#define GT_PROC_ARG_SET9(T1, ARG1, T2, ARG2, T3, ARG3, T4, ARG4, T5, ARG5, T6, ARG6, T7, ARG7, T8, ARG8, T9, ARG9) ARG1(ARG1##_), ARG2(ARG2##_), ARG3(ARG3##_), ARG4(ARG4##_), ARG5(ARG5##_), ARG6(ARG6##_), ARG7(ARG7##_), ARG8(ARG8##_), ARG9(ARG9##_)
#define GT_PROC_ARG_RE9(T1, ARG1, T2, ARG2, T3, ARG3, T4, ARG4, T5, ARG5, T6, ARG6, T7, ARG7, T8, ARG8, T9, ARG9) \
    T1 ARG1 = args->ARG1;                                                                                         \
    T2 ARG2 = args->ARG2;                                                                                         \
    T3 ARG3 = args->ARG3;                                                                                         \
    T4 ARG4 = args->ARG4;                                                                                         \
    T5 ARG5 = args->ARG5;                                                                                         \
    T6 ARG6 = args->ARG6;                                                                                         \
    T7 ARG7 = args->ARG7;                                                                                         \
    T8 ARG8 = args->ARG8;                                                                                         \
    T9 ARG9 = args->ARG9;

#define GT_PROC_ARRAY_CHECK_USE_MT(NAME, ARG_PTR, ADD_PROC_MT, ADD_PROC, BEGIN_PROC, END_PROC, CMP) \
    BEGIN_PROC                                                                                      \
    if(_pl && CMP) {                                                                                \
        _pl->ProcArray(NAME##_, ARG_PTR, 0, num, true);                                             \
        ADD_PROC_MT;                                                                                \
    } else {                                                                                        \
        GN_sDATA_SGM dsg(0, num, 0);                                                                \
        NAME##_(ARG_PTR, &dsg);                                                                     \
        ADD_PROC;                                                                                   \
    }                                                                                               \
    END_PROC

#define GT_PROC_ARG_STRUCT_DEF(NAME, ARG_TABLE, ADD_PROC_MT, ADD_PROC, BEGIN_PROC, END_PROC, CMP, ADD_DEF)                                                                                                           \
    void NAME##_(void *user_arg, const GN_sDATA_SGM *ds);                                                                                                                                                            \
    struct NAME {                                                                                                                                                                                                    \
        GT_PROC_ARG_DEF##ARG_TABLE inline NAME() {} /*建立一个并行处理(参数表.... , 处理数据数, 比较数 处理数大于该值才采用并行, 并行处理对象 默认0不采用并行)*/                                                     \
        inline NAME(GT_PROC_ARG_SND##ARG_TABLE, unsigned int &num, unsigned int cmpn, GN_cPARALLEL *_pl = 0) : GT_PROC_ARG_SET##ARG_TABLE{                                                                           \
                                                                                                                   GT_PROC_ARRAY_CHECK_USE_MT(NAME, this, ADD_PROC_MT, ADD_PROC, BEGIN_PROC, END_PROC, CMP)} ADD_DEF \
    };

#define GT_PROC_ARRAY_FUNC_DEF(NAME, ARGT, ARG, ADD_PROC_MT, ADD_PROC, BEGIN_PROC, END_PROC, CMP)  \
    void NAME##_(void *user_arg, const GN_sDATA_SGM *ds);                                          \
    /*并行处理(参数 , 处理数据数, 比较数 处理数大于该值才采用并行, 并行处理对象 默认0不采用并行)*/ \
    void NAME(ARGT ARG, unsigned int &num, unsigned int cmpn, GN_cPARALLEL *_pl = 0) {             \
        GT_PROC_ARRAY_CHECK_USE_MT(NAME, ARG, ADD_PROC_MT, ADD_PROC, BEGIN_PROC, END_PROC, CMP)    \
    }                                                                                              \
    void NAME##_(void *user_arg, const GN_sDATA_SGM *ds) {                                         \
        ARGT ARG = (ARGT)user_arg;                                                                 \
        unsigned int st = ds->st, end = ds->end;

#define GT_PROC_ARRAY_FUNC_DEF_(NAME, ARG_TABLE, ADD_PROC_MT, ADD_PROC, BEGIN_PROC, END_PROC, CMP) \
    GT_PROC_ARG_STRUCT_DEF(NAME, ARG_TABLE, ADD_PROC_MT, ADD_PROC, BEGIN_PROC, END_PROC, CMP, )    \
    void NAME##_(void *user_arg, const GN_sDATA_SGM *ds) {                                         \
        NAME *args = (NAME *)user_arg;                                                             \
        GT_PROC_ARG_RE##ARG_TABLE unsigned int st = ds->st, end = ds->end;

#define GT_PROC_ARRAY_FUNC_DEF__(NAME, ARG_TABLE, ADD_PROC_MT, ADD_PROC, BEGIN_PROC, END_PROC, CMP)                               \
    GT_PROC_ARG_STRUCT_DEF(NAME, ARG_TABLE, ADD_PROC_MT, ADD_PROC, BEGIN_PROC, END_PROC, CMP, void Proc(const GN_sDATA_SGM *ds);) \
    void NAME##_(void *user_arg, const GN_sDATA_SGM *ds) {                                                                        \
        ((NAME *)user_arg)->Proc(ds);                                                                                             \
    }                                                                                                                             \
    void NAME::Proc(const GN_sDATA_SGM *ds) {                                                                                     \
        unsigned int st = ds->st, end = ds->end;

/* -------------------------------------- 并行处理数组 ProcArray 调用相关的一些便捷宏定义 -----------------------------------------------------*/

//准备传给并行处理用的额外返回值( 返回值名 , 类型 )
#define GN_PROC_ARRAY_RETURN_VALUE_BEGIN(NAME, TYPE)      \
    unsigned int ptn__ = GN::Parallel.GetProcThreadNum(); \
    TYPE NAME##buf[512];                                  \
    TYPE *NAME = ptn__ > 512 ? new TYPE[ptn__] : NAME##buf

//继续准备传给并行处理用的额外返回值( 返回值名 , 类型 )
#define GN_PROC_ARRAY_RETURN_VALUE_BEGIN_(NAME, TYPE) \
    TYPE NAME##buf[512];                              \
    TYPE *NAME = ptn__ > 512 ? new TYPE[ptn__] : NAME##buf

//结束传给并行处理用的额外返回值( 返回值名 )
#define GN_PROC_ARRAY_RETURN_VALUE_END(NAME) \
    if(NAME != NAME##buf) delete[] NAME

//上次并行处理数组的每个分发数据段再历遍开始( 并行处理实列指针 ) !当前检索到的数据ID为变量 [di]
#define GN_PROC_ARRAY_LAST_DATA_SGM_FOR_BEGIN(PL)               \
    {                                                           \
        GN_sDATA_SGM *dss = PL->GetLastProcDataSegmentInfo();   \
        unsigned int _pti = 0, _ptn = PL->GetProcThreadNum();   \
        for(; _pti < _ptn; ++_pti) {                            \
            unsigned int di = dss[_pti].st, de = dss[_pti].end; \
            for(; di < de; ++di) {

//上次并行处理数组的每个分发数据段再历遍结束
#define GN_PROC_ARRAY_LAST_DATA_SGM_FOR_END \
    }                                       \
    }                                       \
    }

//按条件选择并行处理数组还是单核普通处理( 并行处理 , 分发的处理函数 , 传给执行函数用的参数 , 处理数据起始索引, 处理数据结束索引 , 条件 )
#define GN_PROC_ARRAY_CHECK_USE_MT(PL_, PROC, USER_ARG, ST, END, CMP) \
    if(CMP && PL_ GetProcThreadNum() > 1) {                           \
        PL_ ProcArray(PROC, USER_ARG, ST, END, true);                 \
    } else {                                                          \
        GN_sDATA_SGM dsg(ST, END, 0);                                 \
        PROC(USER_ARG, &dsg);                                         \
    }

//试探条件选择并行处理数组还是单核普通处理( 并行处理 , 分发的处理函数 , 传给执行函数用的参数 , 处理数据起始索引, 处理数据结束索引 , 试探条件的键控码 , 数量显示排位号 ) !这个主要用于帮助 GN_PROC_ARRAY_CHECK_USE_MT 找出适合的条件 例如处理数量达到什么程度才开启并行处理, 对于小少量处理单核运行可能会更快哟!
#define GN_PROC_ARRAY_CHECK_USE_MT_TRY(PL_, PROC, USER_ARG, ST, END, KCODE, SHOWID) \
    GN_RunValueCheck2(END, SHOWID);                                                 \
    GN_RUNTSTART(1);                                                                \
    if(GN::Key[KCODE] && PL_ GetProcThreadNum() > 1) {                              \
        PL_ ProcArray(PROC, USER_ARG, ST, END, true);                               \
    } else {                                                                        \
        GN_sDATA_SGM dsg(ST, END, 0);                                               \
        PROC(USER_ARG, &dsg);                                                       \
    }                                                                               \
    GN_RUNTEND;

//并行处理数据处理方法定义( 方法名 , 参数类型 参数名 , 并行善后处理 , 非并行善后处理 )
#define GN_PROC_ARRAY_FUNC_DEF(NAME, ARGT, ARG, ADD_PROC_MT, ADD_PROC) GT_PROC_ARRAY_FUNC_DEF(NAME, ARGT, ARG, ADD_PROC_MT, ADD_PROC, , , num > cmpn)
//并行处理数据处理方法定义( 方法名 , 参数个数(类型参数表...) , 并行善后处理 , 非并行善后处理 )
#define GN_PROC_ARRAY_FUNC_DEF_(NAME, ARG_TABLE, ADD_PROC_MT, ADD_PROC) GT_PROC_ARRAY_FUNC_DEF_(NAME, ARG_TABLE, ADD_PROC_MT, ADD_PROC, , , num > cmpn)
//并行处理数据处理方法定义( 方法名 , 参数个数(类型参数表...) , 并行善后处理 , 非并行善后处理 ) !这个适合多参数处理更快 比如6个以上 但传值参数您最好当成const 在实现内部不要另行再赋值
#define GN_PROC_ARRAY_FUNC_DEF__(NAME, ARG_TABLE, ADD_PROC_MT, ADD_PROC) GT_PROC_ARRAY_FUNC_DEF__(NAME, ARG_TABLE, ADD_PROC_MT, ADD_PROC, , , num > cmpn)

//试探并行处理数据处理方法定义( 方法名 , 参数类型 参数名 , 并行善后处理 , 非并行善后处理 , 试探条件的键控码 , 数量显示排位号)
#define GN_PROC_ARRAY_FUNC_TRY_DEF(NAME, ARGT, ARG, ADD_PROC_MT, ADD_PROC, KCODE, SHOWID) GT_PROC_ARRAY_FUNC_DEF(NAME, ARGT, ARG, ADD_PROC_MT, ADD_PROC, GN_RunValueCheck2(num, SHOWID); GN_RUNTSTART(1);, GN_RUNTEND;, GN::Key[KCODE])
//试探并行处理数据处理方法定义( 方法名 , 参数个数(类型参数表...) , 并行善后处理 , 非并行善后处理 , 试探条件的键控码 , 数量显示排位号)
#define GN_PROC_ARRAY_FUNC_TRY_DEF_(NAME, ARG_TABLE, ADD_PROC_MT, ADD_PROC, KCODE, SHOWID) GT_PROC_ARRAY_FUNC_DEF_(NAME, ARG_TABLE, ADD_PROC_MT, ADD_PROC, GN_RunValueCheck2(num, SHOWID); GN_RUNTSTART(1);, GN_RUNTEND;, GN::Key[KCODE])
//试探并行处理数据处理方法定义( 方法名 , 参数个数(类型参数表...) , 并行善后处理 , 非并行善后处理 , 试探条件的键控码 , 数量显示排位号) !这个适合多参数处理更快 比如6个以上  但传值参数您最好当成const 在实现内部不要另行再赋值
#define GN_PROC_ARRAY_FUNC_TRY_DEF__(NAME, ARG_TABLE, ADD_PROC_MT, ADD_PROC, KCODE, SHOWID) GT_PROC_ARRAY_FUNC_DEF__(NAME, ARG_TABLE, ADD_PROC_MT, ADD_PROC, GN_RunValueCheck2(num, SHOWID); GN_RUNTSTART(1);, GN_RUNTEND;, GN::Key[KCODE])

/*------------------------------------------------------ 用法示例 ------------------------------------------------------------------------------------------------------------------------------------
//定义处理方法
GN_PROC_ARRAY_FUNC_DEF_( MyProc  , 2(char**, texts  , char *, text ) , num = GN_ArrayArrange((unsigned int*)texts, _pl->GetLastProcDataSegmentInfo(), _pl->GetProcThreadNum())  , num = dsg.end )
	unsigned int end2 = st; // st 和 end 为处理数据的起始索引和结束索引
    for( ; st < end; ++st){ //for是必须的 因为有时回调给你的是空段 st == end 如果这时直接用 st 或 do{...}while() 显然会出错
        if(strstr(texts[st], text))texts[end2++] = texts[st]; //选出包含参数2的字串
    }
    GN_PROC_ARRAY_RESET_END( end2 ) //!如果需要返回处理后的数量就有必要调用这个, 并配合善后处理把新数量传回给 num  !如不需要返回新数量可不调用这个和善后处理...
}
//调用处理方法(参数1, 参数2, 处理数据个数, 大于该值才采用并行, 并行处理对象)
MyProc(      words,    "o",     sn,             1,                    pl);
*/
#endif //endif _GN_PARALLEL_H_
