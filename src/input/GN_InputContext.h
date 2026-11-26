// GN_InputContext.h — input 子系统上下文（B7-4，c4 状态收敛，2026-11-27）。
//
// 输入模块的进程级状态收敛进一个上下文结构，由 sEngineContext 持有（GN_GetEngineContext().input）；
// 老 API 继续经全局 GN_InputState 实例工作，行为不变。原 GN_Input.cpp 的 sINPUT_CONTEXT（DA-M2 批 1/批 3）
// 与 GT_sINPUT_PARAME（上下文成员的类型）一并迁出。导出计时变量（GT_RunTotalTime 等，listWheel.h 内联
// 模板直接读）属公共 API，按 D2 保留不迁。
#ifndef _GN_INPUT_CONTEXT_H_
#define _GN_INPUT_CONTEXT_H_

#include <GN.h> // GN::sLIST / GNg::sRECT / GNg::cVIEW2D / GM_LEFT

// 输入操作参数记录（原 GN_Input.cpp 文件内结构，随上下文迁出；方法实现仍在 GN_Input.cpp）
struct GT_sINPUT_PARAME {
    unsigned int time; //消息离开始记录的时间
    short parame[10]; //参数 !!当是文字输入消息时 该数组头两字节是unsigned short 类型存字符长度 如果长度<=18 那么之后的就存字符内容 否则unsigned short之后存的是内存堆里的字符串指针
    //		!!当时浮点数值输入消息时 该数组头两字节是起始索引 之后两字节是浮点值数 接下来的空间就是存浮点数据了
    uint8 type; //输入操作消息类型 取值自 GT_eINPUT_TYPE

    ~GT_sINPUT_PARAME();
    void ImeParameMake(void *str, unsigned short len); //构建文字输入类型的参数数据(输入内容, 内容长度)
};

namespace GNg {

struct GN_InputContext {
    GN::sLIST<GT_sINPUT_PARAME> aInputRecList[2];
    GN::sLIST<GT_sINPUT_PARAME>::iterator aInputIt;
    bool aInputNotIsMapping = true;
    bool aInputRecOn = false;
    bool aInputPlaying = false;
    uint8 aInputRecId = 0;
    uint8 aInputPlayId = 1;
    int aMousePosDeviation = 32;
    bool aMouseRePosX = false;
    bool aMouseRePosY = false;
    GNg::cVIEW2D *aMouseRePosView = 0;
    GN::sLIST<GNg::sRECT> aLastDelayedMouseStates;
    uint8 aTouchMappingMKey = GM_LEFT;
    float aLastZoomLen = 0.0f;
    int aInputLastPort = 0;
    int aInputTestTime = 10000000;
    bool aWsMappingToKey = false;
    unsigned int aInputParameSizes[9] = {0, 20, 4, 6, 8, 4, 8, 20, 4}; //各输入操作消息的参数数据总大小字节 !顺序一一对应 GT_eINPUT_TYPE
    unsigned int aInputRecStartTime = 0; //输入操作记录开始时间
    unsigned int aInputPlayStartTime = 0; //输入操作演示开始时间
    unsigned int aLastOnMouseTime[5] = {10000000, 10000000, 10000000, 10000000, 10000000}; //储存上次按过鼠标键到现在的时间
    unsigned int aMouseInterval = 600; //鼠标双击间隔时间
    unsigned short aDitherRange = 16; //触摸映射为鼠标按下的防斗容忍范围
    unsigned int aTouch0DownTime = 0; //首个触摸点原位按紧不动还未映射为其它鼠键时的累计时间
    unsigned short *aIoMappingKeys = 0; //IO输入出映射键数据
};

extern GN_InputContext GN_InputState; // 默认上下文（仿 GN_Vk / GN_Core）

} // namespace GNg

#endif // _GN_INPUT_CONTEXT_H_
