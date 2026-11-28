# B7 static 收敛账本（c4）

> **状态**：✅ 已完成 — B7 全批（B7-1…B7-7）；B7-6 非渲染模块、B7-7 L2 图形 API。口径、批次与审计记录见下。

## 口径

- 旧口径「136」含 static 函数定义等，非纯状态量；本账本用精确扫描（2026-11-26）：
  **文件/命名空间级非常量 static 变量 33** + **函数内 static 30** + 非 static 文件级全局（粗扫 ~52，含少量误报）。
- 扫描器：brace 深度 + 命名空间感知，排除 const/constexpr/static 函数；函数内 static 单列（同为全局状态）。
- DA-M2 批已把部分 static 收敛进「每文件 context struct」（gXxxContext）——B7 再统一进 sEngineContext。

## 批次计划（冻结顺序：门面 → 渲染状态 → 资源缓存 → 输入/UI → 审计）

| 批 | 范围 | 状态 |
|---|---|---|
| B7-1 | sEngineContext 门面 + vkFrame2d.cpp 2D 帧状态（16 变量 → GN_Vk.s2DRes） | ✅ 2026-11-26，945/0，ABI 登记 3 移除 3 新增 |
| B7-2 | 渲染状态余量：vk3dPasses/vk3dShutdown/vkRenderTarget/vkImageData 等 file static + 3D pass extern | ✅ 2026-11-26，24 变量 → GN_Vk（s3DPassRes/sRtRes/sSwapchainRes/s2DRes + 3 顶层命名成员），948/0，ABI 登记 1 移除；范围与留后续批见文末登记 |
| B7-3 | 资源缓存：core（GN_Init/GN_Format*）+ file（GN_File） | ✅ 2026-11-26，6 变量 → GNg::GN_Core / GN_FileState（新品 2 头），950/0，ABI 登记 2 强新增 + 弱变更 2/2；公共配置变量保留见文末登记 |
| B7-4 | 输入/UI/平台：GN_Input、GN_UiFBrowser、GN_Main（9 static）、GN_Sound | ✅ 2026-11-27，14 变量 → GN_InputState/GN_UiBrowserState/GN_MainState/GN_SoundState（新品 4 头），952/0，ABI 登记 4 强新增 + 弱变更 4/4；预览上下文与公共变量保留见文末登记 |
| B7-5 | 函数内 static 审计 + 全仓复审 + 多实例测试 | ✅ 2026-11-27，30 项函数内 static 审计（迁 10 / 留 20，逐类登记）；全仓复审补 2 项漏计（sysProcClipboard.inc）；多实例契约测试 +3 例；955/0，ABI 零变化；留后续批清单见文末 |
| B7-6 | 非渲染模块 static 收敛：parallel/network/userthread/clipboard | ✅ 2026-11-28，4 上下文 → sEngineContext 门面（前向声明头 + 非 static 全局实例）；955/0，ABI 4 强新增 + 弱 4/4；L2 API/纹理环集/X11 仍留后续 |
| B7-7 | L2 图形 API static 收敛：scissor 栈/imageload | ✅ 2026-11-28，2 上下文 → 门面（L2 同层头，无需跨层白名单，不塞 L3 GN_Vk）；955/0，ABI 2 强新增、弱零变化；纹理环集/X11/预览仍留后续 |

## 文件级 static 变量清单（33）


### src/audio/_Sound/GN_Sound.cpp（1，✅ B7-4 迁 GNg::GN_SoundState）

- L856 `gSoundAlContext`

### src/core/GN_FormatAudio.cpp（1，✅ B7-3 迁 GNg::GN_Core.aAudioExtNames）

- L11 `GT_SupportExtNamesAudio`

### src/core/GN_FormatPlugin.cpp（1，✅ B7-3 迁 GNg::GN_Core.aImageExtNames）

- L10 `GT_SupportExtNamesImage`

### src/core/GN_FormatScale.cpp（1，✅ B7-3 迁 GNg::GN_Core.aImgRowEndID）

- L8 `gFormatScaleContext`

### src/core/GN_Init.cpp（2，✅ B7-3 迁 GNg::GN_Core：aModuleFileName/aModule、aSysDir/aAppDir 等）

- L62 `gUserModuleContext`
- L76 `gPathDirContext`

### src/core/GN_Parallel.cpp（1，✅ B7-6 迁 GNg::GN_ParallelState）

- L34 `gParallelContext`

### src/file/GN_File.cpp（1，✅ B7-3 迁 GNg::GN_FileState.aRunLogOpenMode）

- L894 `gFileLogContext`

### src/graphics/GN_2D.cpp（1，✅ B7-7 迁 GNg::GN_ScissorState，L2 子系统上下文）

- L76 `gScissorContext`

### src/graphics/GN_2Dtype.cpp（1，✅ B7-7 迁 GNg::GN_ImageLoadState，L2 子系统上下文）

- L68 `gImageLoadContext`

### src/graphics/_Vulkan/vk3dShutdown.cpp（2，✅ B7-2 迁 GN_Vk.s3DPassRes）

- L11 `s_vtxCap3D`
- L12 `s_idxCap3D`

### src/graphics/_Vulkan/vkDraw2d.cpp（1，✅ B7-2 迁 GN_Vk.s2DRes.alphaColorKey）

- L229 `s_alphaColorKey`

### src/graphics/_Vulkan/vkFrame2d.cpp（3，✅ B7-1 已迁 GN_Vk.s2DRes）

- L265 `s_pendingStagingBufs`
- L266 `s_pendingStagingMems`
- L267 `s_pendingStagingCnt`

### src/graphics/_Vulkan/vkRenderTarget.cpp（2，✅ B7-2 迁 GN_Vk.sRtRes）

- L15 `s_rtRenderPass`
- L16 `s_rtFramebuffer`

### src/graphics/_Vulkan/vkSkinnedDispatch.cpp（1，✅ B7-2 迁 GN_Vk.s3DPassRes.lastSkinnedPath）

- L18 `s_lastSkinnedPath`

### src/input/GN_Input.cpp（1，✅ B7-4 迁 GNg::GN_InputState）

- L125 `gInputContext`

### src/network/GN_NetTrans.cpp（1，✅ B7-6 迁 GNg::GN_NetTransState）

- L1215 `gNetTransContext`

### src/platform/_Main/GN_Main.cpp（9，✅ B7-4 迁 GNg::GN_MainState）

- L16 `GT_PENDING_RESIZE_W`
- L17 `GT_PENDING_RESIZE_H`
- L28 `GT_LastCursorX`
- L29 `GT_LastCursorY`
- L30 `GT_AccumDX`
- L31 `GT_AccumDY`
- L32 `GT_FirstCursor`
- L37 `GT_ImeBuffer`
- L38 `GT_ImeBufLen`

### src/platform/_Sys/sysProcess.inc（1，✅ B7-6 迁 GNg::GN_UserThreadState）

- L207 `gUserThreadContext`

### src/ui/GN_UiFBrowser.cpp（2，✅ B7-4：gFbContext → GN_UiBrowserState；gFbPreviewContext 留（宏关闭不可验证）；另补迁账本漏计的 GT_FileBrowserLast → aLastFileBrowser）

- L59 `gFbContext`
- L206 `gFbPreviewContext`


## 函数内 static 清单（30，B7-5 审计结果见文末）


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

vkFrame2d.cpp 的 11 个已随 B7-1 迁移。B7-2 已迁渲染状态簇：vk3dPasses/vk3dShutdown/vk3dInit 3D pass 状态、
vkRenderTarget RT 状态（s_activeRT/s_rtInProgress）、vkSwapchain 2D 叠层（s_renderPass2DOverlay/s_framebuffers2DOverlay）。
余量重点：GN_Init.cpp 默认配置 9 个（GT_AppDefault_*）、GN_Input.cpp 计时 5 个、GN_NetBase/GN_NetTrans 网络状态、
sysEncoding.inc 应用目录（GT_CurAppDir*）。vkImageTypes.inc 的 7 个为结构体定义误报，不计。

## B7-2 范围登记（2026-11-26）

**已迁（24 变量 → GN_VkContext；NSDMI 初值与原一致；30 文件约 280 处引用改 GN_Vk.*）**：

- s3DPassRes（新组，14）：in3DPass、inShadowPass、inCubeShadowPass、cubeShadowLightIndex、cubeShadowLightPos、
  shadowPassPipeline、framebuffers3D、vtxOfs3D、idxOfs3D、vtxCap3D、idxCap3D、hasDeferredViewProj、retireTick3D、lastSkinnedPath
- sRtRes（新组，4）：rtRenderPass、rtFramebuffer、activeRT、rtInProgress
- sSwapchainRes（+2）：renderPass2DOverlay、framebuffers2DOverlay
- s2DRes（+1）：alphaColorKey
- 顶层命名非平凡成员（+3，aPipelineCache 先例）：aRetired3DBuffers（std::vector<sRETIRED_3D_BUFFER>，
  条目类型迁 vkImageTypes.inc）、aSkyParams（VkSkyParams，#ifdef GN_USING_3D）、aDeferredViewProj（sMAT44）
- vkImageData.cpp 核实：无文件级状态余量（其引用已随 B7-1 收敛），本批无需迁移。
- vkInternal.h 删 9 extern；GN_Vk.h 补 include GN_3DSkin.h（L3→L2 向下，lastSkinnedPath 枚举可见性）。
- 门禁基线：context_groups.sh 8→10 组、顶层 1→4 命名非平凡成员（字段数 250≥200）。
- ABI：唯一强符号变更 = 移除 GNg::s_shadowPassPipeline；基线按宿主 GCC 16.2.1 重生成，
  弱符号名漂移（libstdc++ 版本）另行登记，见 abi/changes.md。
- 顺手修（环境阻塞，存量缺陷 61dd182 引入）：testOutfitDefault/testOutfitWiring 硬编码 /work/.sbx/tmp
  （仅 sbxd 容器存在，宿主 fopen 必败）→ 相对路径 build/。
- 技术债登记：vk3dInit.cpp 有效代码 1038 行（多功能上限 1000，历史超限）；本批净 −5 行、未顺手拆，
  按技能逃生条款登记为后续拆分项（拆点：Init 编排 / 阴影 / 大气各阶段分文件）。
- 命名存量登记：tests/lint/context_groups.sh 为 snake_case（与同目录 headers/input/layers/output.sh 同族，
  Makefile lint/lint-strict 按名调用）；本批仅改内容、未改名（改名需动 Makefile 与文档，超出本批预算），
  登记为存量命名债务，后续与 tests/lint 脚本一并迁移 camelCase。

**明确留后续批（不在本批写域，原因登记）**：

- gScissorContext（GN_2D.cpp）、gImageLoadContext（GN_2Dtype.cpp）：L2 API 层状态，收敛进 GN_Vk 需跨层白名单，
  留 B7-5 审计或专门 L2 批。
- 三套纹理环集缓存与诊断计数：s_pbrTextureSets/s_forwardTextureSets/s_pbrSyncedGen/s_forwardSyncedGen/
  s_ringResetRequested/s_last3DIndexCount（vk3dForward.cpp）、s_gbufferTextureSets（vk3dPasses.cpp）——
  描述符资源缓存（非平凡类型），属资源缓存类，留后续批。
- 各子系统自有 GPU 资源（vkBloom/vkCloudCube/vkParticle*/vkCloth*/vkAtmosphereLut/vkSkinSsbo 等）：
  vkContext.inc 设计注释明确保留文件内静态，不在 B7 收敛范围（B7-5 审计复核该决定）。
- GN_Parallel/GN_NetTrans/sysProcess.inc 等非渲染模块 static：未分批，B7-5 审计盘点。

## B7-5 审计记录（2026-11-27）

### 函数内 static（30 项）逐项裁决

**迁入上下文（10 项）**：
- GN_File.cpp `run_filename`/`cached_target` → `GN_FileState.aRunLogFileName[256]`/`aRunLogCachedTarget[128]`；
  `pluginsDir` → `aPluginsDir[128]`（初值 "Plugins/"）。
- GN_Input.cpp `GN::sIME_CONTEXT ctx`（GN::GetImeContext 的存储）→ `GN_InputState.aImeContext`；
  `lastMouseTime`/`lastTouchTime` → `aLastMouseTime`/`aLastTouchTime`；`outPutTestIime`/`change` →
  `aOutPutTestTime`/`aOutPutTestChange`（初值 10000）；`Rx`/`Rz` → `aSensorRx`/`aSensorRz`。
- vkDraw2d.cpp `s_lastBlendMode` → `GN_Vk.s2DRes.lastBlendMode`（初值 1）；vkProbe.cpp `cursor` →
  `GN_Vk.sDeferredRes.probeCursor`。

**保留（20 项，逐类理由）**：
- 警告一次标记（2）：vk3dForward/vk3dPasses 的 `warned`——只影响日志频次，无状态语义。
- 计算常量缓存（6）：GN_NetTrans `bs/ds/fs/ts`（strlen 常量）、`max_send_size`；GN_Input `sensorCaps`（惰性平台查询，
  静态初始化期取会早于平台初始化）；vkInit `s_layerNames/s_extNames`（实例创建期临时向量）。
- 临时缓冲/线程临时（3）：GN_Character `aSweepScratch`（thread_local）、GN_UiFBrowser `cloneName`、
  GN_Media `avVersionStr`（返回值指向的格式化缓冲，契约要求持久）。
- 平台事实/进程级语义（5）：sysMutexIo `sysInfo`（三平台分支各一份，仅一个编译）、sysProcess `start_run_time`、
  GN_Color `Tr/Tg/Tb`（色彩动画宏内状态）、GN_Base `seed`（进程级 RNG 序列，公共语义）、GN_Media `zAV_Register`
  （类静态成员，原账本误记为函数内）。
- 其余（4）：GN_UiPopDlgMenu `sPopDlgContext`（访问器持有，ui）、GN_NetBase `sNetAddrContext`（访问器持有，网络）、
  GN_NetTrans `en_addr`（网络通知地址）、GN_Main 分支内 `static int`（随 sys* 批走）。

### 全仓复审（B7 收口盘点）

- 文件级/命名空间级 static：33 项清单中已迁 28、余 5；另补迁账本漏计的 1 项（GN_UiFBrowser GT_FileBrowserLast）。
  函数内 static：30 项中迁 10、留 20。
- 剩余文件级 static（5 + 2 新发现）：
  * `GN_2D.cpp gScissorContext`、`GN_2Dtype.cpp gImageLoadContext`：L2 图形 API 层，收敛需跨层白名单 → 留后续「L2 图形 API 上下文」批。
  * `GN_Parallel.cpp gParallelContext`（并行任务上下文）、`GN_NetTrans.cpp gNetTransContext`、`sysProcess.inc gUserThreadContext`。
  * **新发现（原扫描未覆盖 .inc）**：`sysProcClipboard.inc gClipboardContext`、`GT_X11Clip`（X11 剪贴板）。
- 非渲染状态清单（未批次化，后续按需）：网络（NetBase/NetTrans 上下文与函数内 static）、媒体（AVI/ffmpeg 状态）、
  平台（sysEncoding.inc `GT_CurAppDir*` Android 分支、sysProc* 各 .inc）、ui 弹窗（GN_UiPopDlgMenu 上下文）。
- 按设计保留：纹理环集缓存（vk3dForward/vk3dPasses 描述符缓存，非平凡类型）、各子系统自有 GPU 资源
  （vkContext.inc 注释）、公共 API 变量（GT_MainWindow/GT_AppDefault_*/GT_pFunc*/GT_SYS_*/GN_RunError*/
  GN_ErrorLast/List/GN_Words/GT_RunTotal*/GT_IdleTime/GT_MouseCkN）。
- 口径订正：GN_Media `zAV_Register` 实为类静态成员（非函数内）；账号清单以本记录为准。

### 多实例测试（+3 例）

- `engineContextMultiInstanceContract`：栈上第二门面与单例指向同一批默认子系统上下文（多实例语义变更必须改测试）。
- `coreContextSecondInstanceIndependent`：GN_CoreContext/GN_MainContext/GN_FileContext 本地实例默认值正确且与全局互不影响。
- `migratedFunctionStaticDefaults`：迁入的 4 个可安全断言的初值（输出测试端口/感应器基准/IME 状态）。


## B7-3 范围登记（2026-11-26）

**已迁（6 变量 → GNg::GN_Core / GNg::GN_FileState；原均为文件内 static，引用全在定义文件内）**：

- GN_Core（新头 core/GN_CoreContext.h）：用户程序模块句柄/文件名（原 sUserModuleContext；GN_cMODULE 类声明随迁，方法定义留 GN_Init.cpp）、
  系统/用户资源目录与写入游标（原 sPathDirContext：aSysDir/aSysFile/aAppDir/aAppFile）、格式扩展名缓存（原 GT_SupportExtNamesAudio/Image）、
  缩放临时边界（原 gFormatScaleContext.aImgRowEndID）。
- GN_FileState（新头 file/GN_FileContext.h）：运行日志打开模式（原 gFileLogContext.aRunLogOpenMode）。
- 门面：sEngineContext 增加 core/file 指针（默认指向上述全局实例）；两个新内部头自包含通过（headers.sh 0/2 失败），
  abi/headers-baseline.txt 基线未增（66 条不变）。
- ABI：新增导出 GNg::GN_Core/ GN_FileState；弱符号 2 移除（sUserModuleContext 析构）/2 新增（GN_CoreContext 析构）；见 abi/changes.md。

**保留为公共 API（不迁，理由）**：GN_Init.h 的 GT_AppDefault_*（8）与 GT_pFunc*/GT_SYS_* 回调表、GN_File.cpp 的
GN_RunErrorFileName/GN_RunErrorOutputOn/GN_ErrorLast/GN_ErrorList/GN_Words 等——均为 extern GN_API 导出、仓外应用可直接读写（U1），
与 DA-M2 保留 GT_MainWindow 同一判据；多实例化需先给出兼容层（后续批次）。

