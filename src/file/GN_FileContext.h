// GN_FileContext.h — file 子系统上下文（B7-3，c4 状态收敛，2026-11-26）。
//
// 原 GN_File.cpp 文件内 static（gFileLogContext）：运行日志打开模式——首次写 'w'，
// 之后追加 'a'，换日志文件时重置。由 sEngineContext 持有（GN_GetEngineContext().file）；
// 老 API（GN_Log 等）继续经全局 GN_FileState 工作，行为不变。
#ifndef _GN_FILE_CONTEXT_H_
#define _GN_FILE_CONTEXT_H_

namespace GNg {

struct GN_FileContext {
    char aRunLogOpenMode[2] = "w"; // 'w' 首次写 / 'a' 追加；换日志文件名时 GN_Log 内重置为 'w'
};

extern GN_FileContext GN_FileState; // 默认上下文（仿 GN_Vk / GN_Core）

} // namespace GNg

#endif // _GN_FILE_CONTEXT_H_
