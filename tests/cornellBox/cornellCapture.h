// tests/cornellBox/cornellCapture.h — 引擎画面读回为 PNG（C2 对拍用）
#ifndef CORNELL_CAPTURE_H
#define CORNELL_CAPTURE_H

namespace Cornell {

// 创建 .local/cornellbox/（幂等）。
void ensureCaptureDir();

// 两阶段截图（引擎模式）：在「当前帧命令缓冲」里录制 swapchain→staging 的拷贝，
// 下一帧帧首（引擎已等待 inFlightFence）再把 staging 写成 PNG。
//   * 不能在帧中直接提交独立命令缓冲读 swapchain：镜像还被本帧命令缓冲引用，
//     读到的是上一帧/未定义内容（实测为品红条纹）。
// request 在 composite 之后、HUD overlay 之前调用；finish 在下一帧 AppRun 调用。
bool requestSwapchainCapture(const char *path);
// 诊断：直接抓 HDR 目标（R16G16B16A16_SFLOAT），CPU 端做 ACES+gamma 后写 PNG，
// 用于区分「3D pass 没写进 HDR」还是「composite 采样 HDR 出错」。
bool requestHdrCapture(const char *path);
bool finishPendingCapture();
bool hasPendingCapture();

} // namespace Cornell
#endif // CORNELL_CAPTURE_H
