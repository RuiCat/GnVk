// GN_SoundContext.h — audio 子系统上下文（B7-4，c4 状态收敛，2026-11-27）。
//
// OpenAL 设备/上下文句柄（原 GN_Sound.cpp 的 sSoundAlContext）。句柄用 void* 存储，
// 避免把分平台的 OpenAL 头（AL/OpenAL/windows 分支）拉进引擎上下文头；
// GN_Sound.cpp 在 AL 分支内做 ALCdevice*/ALCcontext* 转型。
#ifndef _GN_SOUND_CONTEXT_H_
#define _GN_SOUND_CONTEXT_H_

namespace GNg {

struct GN_SoundContext {
    void *aAlDevice = 0; // ALCdevice*
    void *aAlContext = 0; // ALCcontext*
};

extern GN_SoundContext GN_SoundState; // 默认上下文（仿 GN_Vk / GN_Core）

} // namespace GNg

#endif // _GN_SOUND_CONTEXT_H_
