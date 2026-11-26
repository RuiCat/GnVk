// GN_CoreContext.h — core 子系统上下文（B7-3，c4 状态收敛，2026-11-26）。
//
// 目标：core 模块的进程级状态收敛进一个上下文结构，由 sEngineContext 持有（GN_GetEngineContext().core）；
// 老 API 继续经全局 GN_Core 实例工作，行为不变（REFACTOR_PLAN §13 B7 第三批）。
// 本批迁入：用户程序模块句柄/文件名、系统与用户资源目录（含写入游标）、格式支持扩展名缓存、
// 缩放内核临时边界。均为原 GN_Init/Format* 文件内 static（多实例化前提），不涉及公共 API。
#ifndef _GN_CORE_CONTEXT_H_
#define _GN_CORE_CONTEXT_H_

#include <GN.h> // P<> 智能指针（GN_Array.h）与基础类型；派生的核心头，不依赖包含顺序

// 动态库模块句柄（B7-3：自 GN_Init.cpp 文件内类迁入，供 GN_CoreContext 持有）。
// 方法定义仍在 GN_Init.cpp。
class GN_cMODULE {
public:
    inline GN_cMODULE() : zModule(0) {}
    inline ~GN_cMODULE() {
        if(zModule) GT_ModuleClose(zModule);
    }

    //初始化装载一个程序模块(模块名)返回是否成功
    bool InitToData(const char *mod_name);

    //从模块里获取一个函数( 函数名 ) 失败返回0, 成功返回该函数指针
    GT_PFUNC_VOID GetFunction(const char *func_name);

private:
    int *zModule; // 模块句柄（平台模块 API 的不透明指针，见 GN_Init.cpp 的 GT_ModuleOpen/Close）
};

namespace GNg {

struct GN_CoreContext {
    // ── sUserModuleRes：用户程序模块（原 GN_Init.cpp sUserModuleContext）
    P<char> aModuleFileName; // 用户程序模块路径文件名 不包含.dll等
    P<GN_cMODULE> aModule; // 用户程序模块

    // ── sPathDirRes：系统与用户资源目录（原 sPathDirContext）
    // 写入游标指向各自缓冲区；初值与旧 static 一致（sys="assets/"、app 空）。
    char aSysDir[384] = "assets/";
    char *aSysFile;
    char aAppDir[384] = {};
    char *aAppFile;

    // ── sFormatExtRes：格式支持扩展名缓存（原 GT_SupportExtNamesAudio/Image，惰性填充）
    char aAudioExtNames[128] = {};
    char aImageExtNames[160] = {};

    // ── sFormatScaleRes：缩放内核临时边界（原 GN_FormatScale.cpp gFormatScaleContext）
    unsigned int aImgRowEndID = 0;

    GN_CoreContext() : aSysFile(aSysDir + 7), aAppFile(aAppDir) {}
};

extern GN_CoreContext GN_Core; // 默认上下文（仿 GN_Vk；多实例随 c4 逐步开放）

} // namespace GNg

#endif // _GN_CORE_CONTEXT_H_
