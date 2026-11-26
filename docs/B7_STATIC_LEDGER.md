# B7 static 收敛账本（c4）

> **状态**：🚧 进行中 — B7-1 完成（门面 + 2D 帧状态）；余渲染/资源/输入UI 批次

## 口径

- 旧口径「136」含 static 函数定义等，非纯状态量；本账本用精确扫描（2026-11-26）：
  **文件/命名空间级非常量 static 变量 33** + **函数内 static 30** + 非 static 文件级全局（粗扫 ~52，含少量误报）。
- 扫描器：brace 深度 + 命名空间感知，排除 const/constexpr/static 函数；函数内 static 单列（同为全局状态）。
- DA-M2 批已把部分 static 收敛进「每文件 context struct」（gXxxContext）——B7 再统一进 sEngineContext。

## 批次计划（冻结顺序：门面 → 渲染状态 → 资源缓存 → 输入/UI → 审计）

| 批 | 范围 | 状态 |
|---|---|---|
| B7-1 | sEngineContext 门面 + vkFrame2d.cpp 2D 帧状态（16 变量 → GN_Vk.s2DRes） | ✅ 2026-11-26，945/0，ABI 登记 3 移除 3 新增 |
| B7-2 | 渲染状态余量：vk3dPasses/vk3dShutdown/vkRenderTarget/vkImageData 等 file static + 3D pass extern | 待做 |
| B7-3 | 资源缓存：core（GN_Init/GN_Format*）+ file（GN_File） | 待做 |
| B7-4 | 输入/UI/平台：GN_Input、GN_UiFBrowser、GN_Main（9 static）、GN_Sound | 待做 |
| B7-5 | 函数内 static 审计 + 全仓复审 + 多实例测试 | 待做 |

## 文件级 static 变量清单（33）


### src/audio/_Sound/GN_Sound.cpp（1，待迁）

- L856 `gSoundAlContext`

### src/core/GN_FormatAudio.cpp（1，待迁）

- L11 `GT_SupportExtNamesAudio`

### src/core/GN_FormatPlugin.cpp（1，待迁）

- L10 `GT_SupportExtNamesImage`

### src/core/GN_FormatScale.cpp（1，待迁）

- L8 `gFormatScaleContext`

### src/core/GN_Init.cpp（2，待迁）

- L62 `gUserModuleContext`
- L76 `gPathDirContext`

### src/core/GN_Parallel.cpp（1，待迁）

- L34 `gParallelContext`

### src/file/GN_File.cpp（1，待迁）

- L894 `gFileLogContext`

### src/graphics/GN_2D.cpp（1，待迁）

- L76 `gScissorContext`

### src/graphics/GN_2Dtype.cpp（1，待迁）

- L68 `gImageLoadContext`

### src/graphics/_Vulkan/vk3dShutdown.cpp（2，待迁）

- L11 `s_vtxCap3D`
- L12 `s_idxCap3D`

### src/graphics/_Vulkan/vkDraw2d.cpp（1，待迁）

- L229 `s_alphaColorKey`

### src/graphics/_Vulkan/vkFrame2d.cpp（3，✅ B7-1 已迁 GN_Vk.s2DRes）

- L265 `s_pendingStagingBufs`
- L266 `s_pendingStagingMems`
- L267 `s_pendingStagingCnt`

### src/graphics/_Vulkan/vkRenderTarget.cpp（2，待迁）

- L15 `s_rtRenderPass`
- L16 `s_rtFramebuffer`

### src/graphics/_Vulkan/vkSkinnedDispatch.cpp（1，待迁）

- L18 `s_lastSkinnedPath`

### src/input/GN_Input.cpp（1，待迁）

- L125 `gInputContext`

### src/network/GN_NetTrans.cpp（1，待迁）

- L1215 `gNetTransContext`

### src/platform/_Main/GN_Main.cpp（9，待迁）

- L16 `GT_PENDING_RESIZE_W`
- L17 `GT_PENDING_RESIZE_H`
- L28 `GT_LastCursorX`
- L29 `GT_LastCursorY`
- L30 `GT_AccumDX`
- L31 `GT_AccumDY`
- L32 `GT_FirstCursor`
- L37 `GT_ImeBuffer`
- L38 `GT_ImeBufLen`

### src/platform/_Sys/sysProcess.inc（1，待迁）

- L207 `gUserThreadContext`

### src/ui/GN_UiFBrowser.cpp（2，待迁）

- L59 `gFbContext`
- L206 `gFbPreviewContext`


## 函数内 static 清单（30，B7-5 审计）


### src/core/GN_Base.cpp（1）

- L71 `int`

### src/core/GN_Color.cpp（1）

- L69 `Tr`

### src/file/GN_File.cpp（3）

- L902 `run_filename`
- L903 `cached_target`
- L976 `pluginsDir`

### src/graphics/GN_Character.cpp（1）

- L284 `std`

### src/graphics/_Vulkan/vk3dForward.cpp（1）

- L93 `warned`

### src/graphics/_Vulkan/vk3dPasses.cpp（1）

- L178 `warned`

### src/graphics/_Vulkan/vkDraw2d.cpp（1）

- L28 `short`

### src/graphics/_Vulkan/vkInit.cpp（2）

- L42 `char`
- L43 `char`

### src/graphics/_Vulkan/vkProbe.cpp（1）

- L259 `cursor`

### src/input/GN_Input.cpp（6）

- L131 `ctx`
- L501 `int`
- L681 `int`
- L800 `int`
- L889 `int`
- L897 `Rx`

### src/media/Media/GN_Media.cpp（2）

- L331 `zAV_Register`
- L527 `avVersionStr`

### src/network/GN_NetBase.cpp（1）

- L33 `ctx`

### src/network/GN_NetTrans.cpp（3）

- L387 `bs`
- L388 `ds`
- L719 `max_send_size`

### src/platform/_Main/GN_Main.cpp（1）

- L339 `int`

### src/platform/_Sys/sysMutexIo.inc（2）

- L141 `sysInfo`
- L212 `sysInfo`

### src/platform/_Sys/sysProcess.inc（1）

- L187 `start_run_time`

### src/ui/GN_UiFBrowser.cpp（1）

- L2028 `cloneName`

### src/ui/GN_UiPopDlgMenu.cpp（1）

- L274 `ctx`


## 非 static 文件级全局（粗扫 ~52）

vkFrame2d.cpp 的 11 个已随 B7-1 迁移。余量重点：GN_Init.cpp 默认配置 9 个（GT_AppDefault_*）、
GN_Input.cpp 计时 5 个、vk3dPasses/vk3dShutdown 3D pass 状态、GN_NetBase/GN_NetTrans 网络状态、
sysEncoding.inc 应用目录（GT_CurAppDir*）。vkImageTypes.inc 的 7 个为结构体定义误报，不计。
