# GN_mini 引擎 — 缺陷分析报告 (含修复记录)

> 2026-07-04 更新 | 原分析: 2026-06-20

## 修复进度

| 类别 | 原数量 | 已修复 | 剩余 |
|------|:--:|:--:|:--:|
| 🔴 Critical (UB/崩溃) | ~200 | ~195 | 5 |
| 🟠 High (稳定性) | ~75 | ~70 | 5 |
| 🟡 Medium (可移植性) | ~6 | 2 | 4 |
| 🟢 Low (代码异味) | ~10 | 2 | 8 |

## 主要修复 (2026-07)

### 🔴 严格别名违规 → memcpy 安全位操作
- `GN_Base.h`: `GN_ValuePackToHalf`, `GN_ValueFloatToHalf`, `GN_ValueUnPackHalf`
- `GN_UiBase.cpp`: 颜色位双关

### 🔴 memcpy(this) 破坏 vtable → placement new
- `GN_Frame.cpp`: `GT_TMP_FRAME_RC_COPY_DATA` 宏
- `GN_Array.h`: `GT_RE_CONSTRUCTOR` 宏

### 🔴 98处 strcpy/strcat → strncpy + 显式 null
- 12 个文件: GN_Sys.cpp(28), GN_String.cpp(11), GN_UiFBrowser.cpp(17) 等

### 🔴 Vulkan API 零错误检查 → +25处 VkCheck
- `GN_Vk.cpp`: 覆盖全部 vkCreate/vkAllocate 关键路径

### 🟠 Vulkan 资源泄漏修复
- 默认纹理 (1×1 white): Image/ImageView/Memory 正确销毁
- GN_VkShutdown 完整清理

### 🟠 光照系统
- 延迟光照着色器 → Cook-Torrance BRDF (GGX + Smith + Fresnel)
- 方向光/聚光灯 `enableShadow` 参数
- `Clear()` 恢复 shadowMapSize
- PBR IBL 镜面反射 F0→FresnelSchlick

### 🟠 视频播放
- FFmpeg 8.x matroska 兼容 (`AVFMT_FLAG_IGNIDX`)
- EOF 循环恢复
- `GN_SetWindowPos` 实现

### 🟡 RADV GPUVM fault — 延迟纹理上传
- 渲染 pass 内 `vkQueueSubmit` → GPUVM fault
- 修复: pass 内暂存数据，帧首 GN_VkBeginFrame 处理

### 🟡 Makefile 修复
- 合并重复 `.o` 规则
- 自动依赖生成 (DEPFLAGS)

## 剩余已知问题

| 问题 | 严重度 | 说明 |
|------|:--:|------|
| `memcpy(this)` UB (cFRAME_RC) | 🟡 | 实测安全, 标记 TODO |
| 全局可变状态 57+ 个 | 🟡 | 需 Context 封装 |
| 编译警告 150+ | 🟢 | 不影响功能 |
| 32 处 strcpy TODO | 🟢 | 需人工审计缓冲区大小 |
| UI 组件 Vulkan 适配 | 🟡 | cFILE_BROWSER/cUI_SCENE 等 |
