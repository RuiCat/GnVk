// tests/cornellBox/cornellCapture.h — 引擎画面读回为 PNG（C2 对拍用）
#ifndef CORNELL_CAPTURE_H
#define CORNELL_CAPTURE_H

namespace Cornell {

// 创建 .local/cornellbox/（幂等）。
void ensureCaptureDir();

// 读回当前 swapchain 并写 PNG。成功后 pathOut 为实际文件路径（由调用方提供缓冲）。
// 注意：swapchain 若为 BGRA，会先交换 R/B；翻交由 SavePngFile 的 col_turn 处理。
bool captureSwapchainPng(const char *path);

} // namespace Cornell
#endif // CORNELL_CAPTURE_H
