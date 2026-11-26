// GN_MainContext.h — 平台主循环上下文（B7-4，c4 状态收敛，2026-11-27）。
//
// resize 挂起值、光标位置与逐帧累积、IME 文字输入缓冲；原 GN_Main.cpp 的 9 个文件级 static。
// 由 sEngineContext 持有（GN_GetEngineContext().platform）；老 API 经全局 GN_MainState 工作。
// 只用平凡类型（不包含任何头），平台层自包含。
#ifndef _GN_MAIN_CONTEXT_H_
#define _GN_MAIN_CONTEXT_H_

namespace GNg {

struct GN_MainContext {
    int aPendingResizeW = 0; // framebuffer resize 回调记录的挂起值（主循环统一处理）
    int aPendingResizeH = 0;
    double aLastCursorX = 0.0; // 上次光标位置
    double aLastCursorY = 0.0;
    double aAccumDX = 0.0; // 本帧累积的水平增量
    double aAccumDY = 0.0; // 本帧累积的垂直增量
    bool aFirstCursor = true; // 首帧初始化标记
    char aImeBuffer[256] = {0}; // IME 文字输入缓冲（GF_ReSetInputState 取走）
    unsigned char aImeBufLen = 0; // 缓冲有效字节数（原 uint8 GT_ImeBufLen）
};

extern GN_MainContext GN_MainState; // 默认上下文（仿 GN_Vk / GN_Core）

} // namespace GNg

#endif // _GN_MAIN_CONTEXT_H_
