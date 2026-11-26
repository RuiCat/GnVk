# 符号变更登记（REFACTOR_PLAN C1 修订版：破坏性变更必须登记）

基线：`abi/baseline.txt`（nm -D --defined-only，每批次更新）。
比较口径：按符号名（地址随代码布局变化，不算变更）。

| 日期 | 批次 | 变更 | 原因 | 消费者同步 |
|---|---|---|---|---|
| 2026-11-26 | B2 第一刀（vkRenderTarget 拆出） | 移除导出：`GNg::s_activeRT`、`GNg::s_rtInProgress`（改 GN_VK_INTERNAL 隐藏可见性） | 内部全局变量，历史上泄漏进动态符号表；仅后端内部使用，无仓内/公开消费者 | 无需（仓内引用全部在 _Vulkan 模块内，经 vkInternal.h extern） |
| 2026-11-26 | B7-1（2D 帧状态收敛 + 引擎上下文门面） | 移除导出：`GNg::s_currentScissor`、`GNg::s_scissorEnabled`、`GNg::s_scissorDirty`（迁入 GN_VkContext.s2DRes）；新增导出：`GN_GetEngineContext`、`sEngineContext` 构造 | c4 状态收敛：scissor 状态进渲染上下文；门面为多实例铺路（D2 允许破坏式重命名） | 仓内引用已全部改 GN_Vk.scissor*；仓外消费者见 U1（未确认，风险已知） |
| 2026-11-27 | B7-4（输入/UI/平台/音频上下文） | 新增导出：`GNg::GN_InputState`（B）、`GNg::GN_UiBrowserState`（B）、`GNg::GN_MainState`（D）、`GNg::GN_SoundState`（B）；弱符号变更：移除 4 个（旧 sINPUT_CONTEXT/sUI_FB_CONTEXT 析构 D1/D2）、新增 4 个（GN_InputContext/GN_UiBrowserContext 析构 D1/D2） | c4 状态收敛：四个子系统状态进上下文，门面统一持有（D2 允许破坏式重命名） | 仓内引用已改 `GN_InputState.*`/`GN_UiBrowserState.*`/`GN_MainState.*`/`GN_SoundState.*`；导出计时/配置变量按 API 保留 |
| 2026-11-26 | B7-3（资源缓存上下文：core/file） | 新增导出：`GNg::GN_Core`（B）、`GNg::GN_FileState`（D）——core/file 子系统上下文单例，供门面默认指向；弱符号变更：移除 2 个（旧 `sUserModuleContext` 析构 D1/D2 不再生成）、新增 2 个（`GN_CoreContext` 析构 D1/D2）。无其他强符号变更（6 个原 static 均无导出） | c4 状态收敛：资源缓存（模块句柄/资源目录/格式扩展名缓存/日志模式）进子系统上下文，门面统一持有（D2 允许破坏式重命名） | 仓内引用已改 `GN_Core.*`/`GN_FileState.*`；`GT_AppDefault_*` 等公共配置变量按 API 保留未迁（见账本 B7-3 范围登记） |
| 2026-11-26 | B7-2（渲染状态余量收敛） | 移除导出：`GNg::s_shadowPassPipeline`（迁入 GN_VkContext.s3DPassRes.shadowPassPipeline）；强符号零新增。基线按宿主 GCC 16.2.1 重生成：另有 149 个弱/unique/typeinfo 符号移除、42 个新增（libstdc++ 版本漂移，如 `_M_realloc_insert`→`_M_realloc_append`；旧基线含 GCC≤14 命名，与本批代码改动无关） | c4 状态收敛：3D pass/RT/2D 叠层状态进渲染上下文（D2 允许破坏式重命名） | 仓内引用已全部改 GN_Vk.*；仓外消费者见 U1（未确认，风险已知） |
