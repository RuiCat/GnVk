#pragma once
#ifndef _GN_AMCTRL_H_
#define _GN_AMCTRL_H_

namespace GNg {
extern GN_API const char *AmModeNames[]; //动画播放模式名 -- 对应下面的每一个举枚

enum eAM_MODE { //动画播放模式
    eAM_NOAM, //无动画
    eAM_LOOP, //循环播放
    eAM_REPEAT, //来回播放
    eAM_ONCE_HIDE, //播放一次消失
    eAM_ONCE_KEEP //播放一次停住
};
enum eAM_TIME_RESET_OP { //动画时间重设选项
    eAM_TIME_RESET_SELF, //仅重设自身动画时间
    eAM_TIME_RESET_ALL //重设自身与所有子对象动画时间
};
enum eAM_TRS_OP { //时间运行段处理选项
    eAM_TRS_FLUSH, //立即执行
    eAM_TRS_PAUSE //先暂停住
};
enum eAM_TCR_TYPE { //动画时间特殊常量值

    eAM_TCR_START = 0xfffffffa, //动画段开始时间
    eAM_TCR_LAST, //上一帧时间
    eAM_TCR_CUR, //当前时间
    eAM_TCR_NEXT, //下一帧时间
    eAM_TCR_END, //动画段结束时间
};
struct GN_API sAM_NODE //动画段落节点
{
    unsigned int node_time; //节点时间
    unsigned int seek_time; //节点目标跳转时间 值0xffffffff或超出动画结束时间范围为在当前段落里继续播放 不会作跳转
    unsigned short am_mode; //动画段落间的播放模式 取值参考 eAM_MODE

    inline sAM_NODE() {}

    //构造  (			 节点时间,				  播放到节点时跳转到的时间位置,           该节点前的段落播放模式)
    inline sAM_NODE(unsigned int _node_time, unsigned int _seek_time = 0xffffffff, unsigned short _am_mode = eAM_REPEAT) : node_time(_node_time), seek_time(_seek_time), am_mode(_am_mode) {}

    //初始化(			 节点时间,				  播放到节点时跳转到的时间位置,           该节点前的段落播放模式)
    inline void Init(unsigned int _node_time = 1000, unsigned int _seek_time = 0xffffffff, unsigned short _am_mode = eAM_REPEAT) {
        node_time = _node_time;
        seek_time = _seek_time;
        am_mode = _am_mode;
    }
};
extern GN_API sAM_NODE AmNode; //默认动画段落节点 AmNode(1000);

struct GN_API sAM_CTRL //动画时间控制
{
    unsigned int zRuntime; //动画存活运行时间
    unsigned int zNowtime; //动画自身现在时间
    unsigned int zEndtime; //动画结束时间
    unsigned int zStarttime; //动画开始时间
    unsigned short zAm_mode; //播放模式 值参考 eAM_MODE
    bool zAm_s_e; //播放为开始还是结束 0为开始播放 到了结束时间了就为1 如果再回到播放开头又会变回0
    bool zHasTRS; //自更新动画时间是否有运行段控制
    bool zPlaying; //自更新动画时间是否在播放中

    P<sAM_NODE> zNodes; //所包含的动画段落节点数组
    unsigned short zSegmentID; //当前动画段落号
    unsigned short ztAm_mode; //临时保存原本动画播放模式
    unsigned int ztEndtime; //临时保存原本动画结束时间
    unsigned int zTargetTime; //自更新动画运行目标时间
    unsigned int zStartRunTime; //预估开始存活运行时间

    void zSegmentSet(); //设置段落时间范围
    uint8 *zDataSave(uint8 *wpos); //保存该对象的2进制数据(当前写入位置)返回最后的写完位置
    uint8 *zDataLoad(uint8 *rpos); //读取该对象的2进制数据(当前读取位置)返回最后的读完位置

    inline sAM_CTRL() : zRuntime(0), zEndtime(0), zStarttime(0), zNowtime(0), zAm_mode(0), zAm_s_e(false), zPlaying(true), zHasTRS(false) {}

    //动画时间处理控制(更新多少时间) 返回值 0:正在播放 1:从新循环一次到开始 2:正在倒播 3:正在播放消失 4:正在播放到停住
    unsigned short zAmTimeProce(unsigned int updatetime);

    //初始化动画段落(动画段落节点数组 输入空指针可以清除动画段落节点)
    void InitSegment(const P<sAM_NODE> &node_array);

    //跳转到指定动画段落号的播放位置( 动画段落号 )
    void SeekSegment(unsigned short segmentID);

    //设置时间控制(        当前时间,                     开始时间,             结束时间,                  播放模式  )
    void SetTimeControl(unsigned int now_time = 0, unsigned int start_time = 0, unsigned int end_time = 1, unsigned short am_mode = eAM_REPEAT);

    //时间控制运行段设置(跳转时间, 运行目标时间, 处理选项) 时间参数也可以是 eAM_TCR_TYPE 里的值之一  !这个带有即时性 通常调用zAmTimeProce后再调用 作用就是立即从跳转时间运行到目标时间后自动暂停! 一但调用这个后自身的动画时间更新将会强行被这里设置的改变 自动循环播放和来回播放将被取代
    void TimeRunSegmentSet(unsigned int st, unsigned int tt, eAM_TRS_OP op = eAM_TRS_FLUSH);

    //把字符串表示的时间运行段转成实际时间段(时间段表示字符串 调用后该值将受到改变, 返回跳转时间, 返回运行目标时间)成功返回0 返回1:格式不正确 2:引用了未知符号
    static int8 TimeRunSegmentAtoL(char *ts_str, unsigned int *st, unsigned int *tt);
    /*
		以上函数的时间段表示字符串 通常这样描述: 0,1000 或 380,20 或 0,| 或 -,> 时间可以用特殊符号表示 如:
		. 动画段的开始时间				    内部转义为: eAM_TCR_START
		< 相对于当前播放时间的上一帧时间	内部转义为: eAM_TCR_LAST
		- 当前播放时间						内部转义为: eAM_TCR_CUR
		> 相对于当前播放时间的下一帧时间	内部转义为: eAM_TCR_NEXT
		| 动画段的结束时间					内部转义为: eAM_TCR_END
    */
    //时间控制运行段取消
    inline void TimeRunSegmentCancel() {
        zHasTRS = false;
        zPlaying = true;
    }

    //获得动画结束时间()返回结束时间
    inline unsigned int GetEndtime() { return zEndtime; }
    //获得动画自身现在时间()返回自身现在时间
    inline unsigned int GetNowtime() { return zNowtime; }
    //设置动画自身现在时间(时间)
    inline void SetNowtime(unsigned int _time) { zNowtime = _time; }
    //设置动画存活运行时间(时间)
    inline void SetRuntime(unsigned int *_time) { zRuntime = *_time; }
    //获得动画当前播放模式
    inline eAM_MODE GetAmMode() { return (eAM_MODE)zAm_mode; }
    //判断动画当前是否为结束阶段  来回播放模式回流时也返回true
    inline bool AmIsEnd() { return zAm_s_e; }
};
}; // namespace GNg
#endif //_GN_AMCTRL_H_