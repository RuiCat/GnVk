# GN_mini Vulkan 后端迁移报告

> 最终更新: 2026-07-04 | 状态: ✅ 核心完成

## 渲染管线

| 管程 | 状态 | 说明 |
|------|:--:|------|
| 2D 批处理渲染 | ✅ | 顶点批处理 + 动态 scissor + 4 种混合模式 |
| 3D Forward PBR | ✅ | Cook-Torrance BRDF + GGX + 法线贴图 + IBL |
| 延迟光照 (Deferred) | ✅ | G-Buffer + 逐光源 PBR + Poisson PCF 阴影 |
| 阴影映射 | ✅ | 2D Shadow Map + Poisson 16点软阴影 |
| HDR + 色调映射 | ✅ | Reinhard 色调映射 + Skybox IBL |
| MSAA | ✅ | 4x MSAA (3D 管程) |
| RenderTarget | ✅ | 离屏渲染到纹理 |

## 资源加载

| 功能 | 状态 | 说明 |
|------|:--:|------|
| PNG 纹理 (libpng) | ✅ | 单帧 + 多帧图集 |
| FreeType 字体 | ✅ | TTF/OTF 实时渲染 |
| FFmpeg 视频 | ✅ | 流式解码 H264/AAC |

## 关键修复 (2026-07)

### 延迟纹理上传机制

**问题**: 渲染 pass 内调用 `DataFill` 会创建独立 `VkCommandBuffer` 并 `vkQueueSubmit`，在 AMD RADV 驱动上触发 GPUVM fault。

**修复**: pass 内检测 `s_inRenderPass`，将纹理数据暂存到 `s_pendingUploads[]`，在下一帧 `GN_VkBeginFrame` 中（`vkWaitForFences` 后、`vkBeginRenderPass` 前）完成上传。

**影响范围**:
- `sIMAGE::DataFill` — 子区域更新 + 新纹理创建
- `UploadImageToGPU` — 委托给 `DataFill` 统一处理
- pending list 容量: 256

### PreFrame 回调

新增 `GT_pFuncPreFrame` 函数指针，在 `GF_FrameCycleProc1` 中调用（渲染 pass 前），适用于需要在 pass 外完成 GPU 初始化的场景。

### OpenGL 兼容字段

| 字段 | Vulkan 状态 |
|------|:--:|
| `sIMAGE::texture` | 未使用 (OpenGL 纹理 ID 数组) |
| `sIMAGE::texnum` | 由 DataFill 设置 (图集帧数) |
| `sIMAGE::cFormat` | CR_DEFAULT_FORMAT(0) → 映射为 RGBA |

## 已知局限

| 项目 | 状态 | 说明 |
|------|:--:|------|
| 文件浏览器 (cFILE_BROWSER) | ⚠️ | OpenGL 纹理合并代码在 Vulkan 下不兼容 |
| UI 场景加载 (cUI_SCENE) | ⚠️ | 同上，需适配 descriptor set 管理 |
| 弹窗对话框 | ⚠️ | UI 组件需逐组件适配 |
| OpenGL 旧版路径 | ❌ | 已移除，仅 Vulkan |

## 性能优化记录

- 顶点缓冲预分配 512K 顶点 (16MB)，避免运行时 GrowVBufIfNeeded
- 2D 批处理按 descriptor set 分组，减少 bind 调用
- 纹理上传合并到单次 vkQueueSubmit

## FFmpeg 兼容性

引擎代码兼容 FFmpeg 4.x ~ 8.x，通过 `US_FFMPEG4` 宏切换新旧 API。FFmpeg 8.x 的 matroska 解复用器对段边界检测更严格，`AVFMT_FLAG_IGNIDX | AVFMT_FLAG_IGNDTS` 标志可放宽限制。
