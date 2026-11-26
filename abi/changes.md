# 符号变更登记（REFACTOR_PLAN C1 修订版：破坏性变更必须登记）

基线：`abi/baseline.txt`（nm -D --defined-only，每批次更新）。
比较口径：按符号名（地址随代码布局变化，不算变更）。

| 日期 | 批次 | 变更 | 原因 | 消费者同步 |
|---|---|---|---|---|
| 2026-11-26 | B2 第一刀（vkRenderTarget 拆出） | 移除导出：`GNg::s_activeRT`、`GNg::s_rtInProgress`（改 GN_VK_INTERNAL 隐藏可见性） | 内部全局变量，历史上泄漏进动态符号表；仅后端内部使用，无仓内/公开消费者 | 无需（仓内引用全部在 _Vulkan 模块内，经 vkInternal.h extern） |
