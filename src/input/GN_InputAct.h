#ifndef _GN_INPUTACT_H_
#define _GN_INPUTACT_H_

namespace GN {
namespace IA {
struct GN_INIT_API sOBJ //输入动作基本对象
{
    static GN::sLIST<sOBJ *> zTotalList; //记录应用程序一共有那些GN::IA::sOBJ对象
    sOBJ **zDelId; //记录它的删除号

    //更新(帧间隔时间)
    virtual void zUpDate(unsigned int nTime) = 0;
    //更新全部GN::IA::sOBJ对象 这个引擎系统会自动调用 用户不必关心
    static void zUpdateAll(unsigned int nTime);

    bool aOn; //当前动作是否有触发 检测该值来判断是否触发了动作

    sOBJ() : aOn(false) { zDelId = zTotalList.add(this); }
    virtual ~sOBJ() { zTotalList.del(zDelId); }
};
struct GN_INIT_API sMOUSE_DOWN_SKIP : public sOBJ //鼠标按紧不动
{
    uint8 zMsKey; //鼠标键位
    unsigned int zMsDownTime; //鼠标键按紧下的时间
    unsigned int zMsSkipTime; //鼠标键按紧下的目标触发时间

    virtual void zUpDate(unsigned int nTime); //更新(帧间隔时间)

    sMOUSE_DOWN_SKIP() : zMsDownTime(0) {}
    virtual ~sMOUSE_DOWN_SKIP() {}

    //初始化(鼠标键位, 按住触发时间)
    void Init(uint8 mk, unsigned int skip_time) {
        zMsKey = mk;
        zMsSkipTime = skip_time;
        zMsDownTime = 0;
    }
};
struct GN_INIT_API sMOUSE_DOWN_SHAKE : public sOBJ //鼠标按住摇晃动作
{
    uint8 zMsKey; //鼠标键位
    unsigned int zMsDownTime; //鼠标键按紧下的时间
    short zMsDownSX; //鼠标键按紧下的拖动速度X
    short zMsDownSY; //鼠标键按紧下的拖动速度Y
    short zMsMoveNum; //鼠标键按紧下的来回移动次数

    virtual void zUpDate(unsigned int nTime); //更新(帧间隔时间)

    sMOUSE_DOWN_SHAKE() : zMsDownTime(0) {}
    virtual ~sMOUSE_DOWN_SHAKE() {}

    //初始化(鼠标键位)
    void Init(uint8 mk) {
        zMsKey = mk;
        zMsDownTime = 0;
    }
};

}; //end namespace IA
}; //end namespace GN
#endif //_GN_INPUTACT_H_
