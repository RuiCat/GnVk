# Changelog

## 2026-07-04 — Vulkan 稳定性修复

### 核心修复

- **延迟纹理上传机制**: 渲染 pass 内 `DataFill`/`UploadImageToGPU` 不再 `vkQueueSubmit`，数据暂存到 `s_pendingUploads[256]`，由 `GN_VkBeginFrame` 在 pass 前处理。彻底解决 AMD RADV GPUVM fault。

- **PreFrame 回调**: 新增 `GT_pFuncPreFrame`，在 `GF_FrameCycleProc1` (渲染 pass 前) 调用，适用于需在 pass 外完成的 GPU 初始化。

- **UploadImageToGPU 重构**: 移除独立的 GPU 上传逻辑，委托给 `DataFill` 统一走延迟上传路径。

### 光照系统

- 延迟光照着色器: Blinn-Phong → Cook-Torrance BRDF (GGX + Smith + Fresnel)
- `AddDirectional`/`AddSpot` 新增 `enableShadow` 参数
- `cLIGHT::Clear()` 恢复 `shadowMapSize`
- `GN_VkBegin3DPass` 等 4 个函数从 `lightUBO` 读取环境光
- PBR shader IBL 镜面反射: `F0` → `FresnelSchlick(NdotV, F0)`

### 视频播放

- FFmpeg 8.x matroska: `AVFMT_FLAG_IGNIDX | AVFMT_FLAG_IGNDTS`
- `zReadPacket` EOF 循环恢复 + `Play()` IO 重置
- `GN_SetWindowPos` 实现 (glfwSetWindowSize + GN_VkResize)
- MKV 文件重封装修复 H264 解码警告

### UB 修复

- 严格别名: `GN_Base.h` (packHalf/unpackHalf), `GN_UiBase.cpp`
- `memcpy(this)` → placement new: `GN_Frame.cpp`, `GN_Array.h`

### 安全修复

- 98 处 `strcpy`/`strcat` → `strncpy` + 显式 null (12 文件)
- 25 处 Vulkan `VkCheck` 错误检查

### Vulkan 资源管理

- 默认纹理 (1×1 white) Image/ImageView/Memory 在 shutdown 正确销毁
- 顶点缓冲预分配 512K 顶点 (16MB)
- `vkQueueWaitIdle` → fence 同步 (3 处)

### 示例修复

- Sample4/5: OpenGL 纹理合并代码在 Vulkan 下跳过
- `ImgFmtGetByteNum(0)` 返回 4 (CR_DEFAULT_FORMAT)
- Sample12: 视频缩放适配窗口 + audio=false

### 代码清理

- samples/ 精简为仅 .cpp 源码 (259 文件删除, 80MB→380KB)
- assets/ 精简为仅 shaders/ 目录
- 调试 fprintf 清理
- .gitignore 添加 build/ 产物

### 构建

- Makefile: 合并重复 .o 规则 + DEPFLAGS 自动依赖
