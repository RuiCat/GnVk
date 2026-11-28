# 符号变更登记（REFACTOR_PLAN C1 修订版：破坏性变更必须登记）

基线：`abi/baseline.txt`（nm -D --defined-only，每批次更新）。
比较口径：按符号名（地址随代码布局变化，不算变更）。

| 日期 | 批次 | 变更 | 原因 | 消费者同步 |
|---|---|---|---|---|
| 2026-11-28 | B7-7（L2 图形 API static 收敛） | 新增导出 2：`GNg::GN_ScissorState`/`GN_ImageLoadState`（B）；弱符号零变化（sRECT 数组+uint8、char* 均平凡可析构，无析构符号）；无移除 | c4：L2 2D 剪裁嵌套栈/图像加载类型名作为 L2 子系统上下文纳入门面（前向声明头同层包含，无需跨层白名单；不塞进 L3 GN_Vk） | 仓内引用改 GNg::GN_ScissorState/GN_ImageLoadState（仅本文件）；make ci 955/0 |
| 2026-11-28 | B7-6（非渲染模块 static 收敛） | 新增导出 4：`GNg::GN_ParallelState`/`GN_NetTransState`/`GN_UserThreadState`/`GN_ClipboardState`（B）；弱符号 4 移除（旧 static sNetTransContext/sUserThreadContext 析构 D1/D2）/4 新增（GNg::GN_NetTransContext/GN_UserThreadContext 析构 D1/D2）；sParallelContext/sClipboardContext 平凡类型无析构符号 | c4 状态收敛：parallel/network/userthread/clipboard 文件级 static 纳入 sEngineContext 门面（前向声明头 + 非 static 全局实例，结构体定义留实现文件） | 仓内引用改 GNg::GN_XxxState（含跨 .inc：sysThread/sysMutexIo）；make ci 955/0 |
| 2026-11-28 | B9-4（字符串安全） | 强/弱符号零变化（nm -D diff=0）。strcpy/strcat→memcpy、sprintf→snprintf 均不改签名/符号 | 字符串安全清零（9 处契约登记） | 无需（make ci 955/0 + smoke 13/0） |
| 2026-11-28 | B9-3（告警大扫除） | 强符号/弱符号均零变化（同工具链 nm -D diff=0）。构造初始化列表重排、Vulkan {sType}→{}+赋值、顶层限定符去除、字面量 cast 等均不改签名/符号 | 质量门：告警 19385→7 | 无需（make ci 955/0 + smoke 13/0 验证） |
| 2026-11-28 | B9-1/B9-2（去 -fpermissive + C++17） | 强符号（T/D/B）零变化。弱符号：对远程基线 191 处差异为 GCC 13.3（容器）vs 16.2.1（基线宿主）工具链漂移（B7-2 已登记同类）；B9-1→B9-2 同工具链另有 7 个弱模板符号实例化漂移（GN_ListWheelListPushBack/cLAUNCHER_SEED/P<sAM_EVENT>::Delete 等，register 清除与 std 切换所致），无强符号/签名变更 | 质量门：非法转换显式化 + 标准升级，均不改公共签名 | 无需（仓内消费者经 make ci 955/0 验证） |
| 2026-11-26 | B2 第一刀（vkRenderTarget 拆出） | 移除导出：`GNg::s_activeRT`、`GNg::s_rtInProgress`（改 GN_VK_INTERNAL 隐藏可见性） | 内部全局变量，历史上泄漏进动态符号表；仅后端内部使用，无仓内/公开消费者 | 无需（仓内引用全部在 _Vulkan 模块内，经 vkInternal.h extern） |
| 2026-11-26 | B7-1（2D 帧状态收敛 + 引擎上下文门面） | 移除导出：`GNg::s_currentScissor`、`GNg::s_scissorEnabled`、`GNg::s_scissorDirty`（迁入 GN_VkContext.s2DRes）；新增导出：`GN_GetEngineContext`、`sEngineContext` 构造 | c4 状态收敛：scissor 状态进渲染上下文；门面为多实例铺路（D2 允许破坏式重命名） | 仓内引用已全部改 GN_Vk.scissor*；仓外消费者见 U1（未确认，风险已知） |
| 2026-11-27 | B7-4（输入/UI/平台/音频上下文） | 新增导出：`GNg::GN_InputState`（B）、`GNg::GN_UiBrowserState`（B）、`GNg::GN_MainState`（D）、`GNg::GN_SoundState`（B）；弱符号变更：移除 4 个（旧 sINPUT_CONTEXT/sUI_FB_CONTEXT 析构 D1/D2）、新增 4 个（GN_InputContext/GN_UiBrowserContext 析构 D1/D2） | c4 状态收敛：四个子系统状态进上下文，门面统一持有（D2 允许破坏式重命名） | 仓内引用已改 `GN_InputState.*`/`GN_UiBrowserState.*`/`GN_MainState.*`/`GN_SoundState.*`；导出计时/配置变量按 API 保留 |
| 2026-11-26 | B7-3（资源缓存上下文：core/file） | 新增导出：`GNg::GN_Core`（B）、`GNg::GN_FileState`（D）——core/file 子系统上下文单例，供门面默认指向；弱符号变更：移除 2 个（旧 `sUserModuleContext` 析构 D1/D2 不再生成）、新增 2 个（`GN_CoreContext` 析构 D1/D2）。无其他强符号变更（6 个原 static 均无导出） | c4 状态收敛：资源缓存（模块句柄/资源目录/格式扩展名缓存/日志模式）进子系统上下文，门面统一持有（D2 允许破坏式重命名） | 仓内引用已改 `GN_Core.*`/`GN_FileState.*`；`GT_AppDefault_*` 等公共配置变量按 API 保留未迁（见账本 B7-3 范围登记） |
| 2026-11-26 | B7-2（渲染状态余量收敛） | 移除导出：`GNg::s_shadowPassPipeline`（迁入 GN_VkContext.s3DPassRes.shadowPassPipeline）；强符号零新增。基线按宿主 GCC 16.2.1 重生成：另有 149 个弱/unique/typeinfo 符号移除、42 个新增（libstdc++ 版本漂移，如 `_M_realloc_insert`→`_M_realloc_append`；旧基线含 GCC≤14 命名，与本批代码改动无关） | c4 状态收敛：3D pass/RT/2D 叠层状态进渲染上下文（D2 允许破坏式重命名） | 仓内引用已全部改 GN_Vk.*；仓外消费者见 U1（未确认，风险已知） |
