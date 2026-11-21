# 遗留项收敛总结（2026-11-21，goal-6fde2e21 轮次 8）

> 用途：给用户/后续会话一份「现在到哪了、还差什么、怎么复算」的单页总结。
> 权威台账仍是 `docs/BACKLOG.md`（§2 逐项状态、§8 收敛台账）与 `docs/ENGINE_COORDINATION_2026-11-21.md` §5。

## 0. 当前一句话

CPU 可验的遗留项已基本落地（多着色器 P2a/P3-CPU/P5-CPU、G1b、§8 台账、DA-M2 批 1a/1b/2a/2b）；剩余 CPU 工作 = **DA-M2 批 3 + 批 4（25 个全局；原口径 26 含 1 条误命中）**；GPU/真机项与 DA-M1r 已逐条标 blocked 并附复算命令。

## 1. Goal 与基线

| 项 | 值 |
|---|---|
| Goal | 旧 `goal-6fde2e21-…`（轮次 8 为止）；**2026-11-22 新会话 `get_goal` 为 null，已按续跑需要重建** |
| 构建基线 | `make` exit=0；`make unit` **463/0**；`make imoScene` exit=0 |
| lint | 新增/改动文件 0/0；`GN_UiTextIn`(1129)/`GN_UiFBrowser`(1966) 为历史超限（非本轮引入） |
| 最新备份 | `GnVk-20261121T153906Z.tar.zst`（213143276 B，sha256 `d11557cd…633e88`；**2026-11-22 实测 1875 普通文件 + 320 目录**，订正 BACKLOG B7b 的 1861 口径；同盘 staging，外盘 EROFS） |
| 工作树 | 含多会话未提交改动；**禁止** `make clean`/`git checkout/clean/stash`；恢复单文件不要用 `git show HEAD:`（HEAD 可能是未拆分旧版） |

## 2. 已完成（有实跑证据）

| 项 | 结论 | 证据 |
|---|---|---|
| E1 CSM 退休缓冲 UAF | done | 真机 `IMO_SCENE_SHADOWS=1` 3/3 + 默认启动无 page fault/reset（日志 `.local/tmp/csm_*.log`） |
| G1 CSM 默认开启 | done | demo 默认 `aEnabled(true)`、`IMO_SCENE_SHADOWS=0` 关闭；patch sha `e79af1a9…80d0` |
| E2/E3 | code-done；视觉 A/B blocked | 4 级 shadow UBO / sIMAGE sRGB+wrap 引擎 API；demo 已切 `ImageLoadFromMem` |
| F1 resize 警告 | confirmed 误报 | 惰性管线；`M-known-defects-34` |
| G1b 室内 GI 引擎化 | done（CPU+API） | `GN_Environment.{h,cpp}` + `GN_VkSetEnvironmentLighting`；demo 删本地 `applyRoomAmbient` |
| G2/G3/G4 | done | sIMAGE API 落地；21 个 `IMO_SCENE_*` 审计（§7.1）；收口命令 |
| B5b–B5n | done | 拆分/加固/审计/P2a/P3-CPU/P5-CPU（见 BACKLOG §2） |
| P1a/P1b | API done；GPU 销毁 blocked | 特殊化常量/键版本/GC API |
| B6 | obsolete | 云层已由 L5/立方体云接管 |
| §8 收敛台账 | done | `docs/BACKLOG.md` §8 |

## 3. blocked（需要硬件 / 决策 / 环境）

| 项 | 阻塞原因 | 复算命令 / 所需 |
|---|---|---|
| B2 P1b GC 销毁 | 真实 GPU 时序未验 | amdgpu 机：`make gpuCheck` + 帧边界回收冒烟 |
| B3a/B3b 视觉、B3c HW ray-query | 视觉 A/B / HW 变体 | GPU 机：`make gpuCheck` + cornell 同机位截图对比 |
| B4a/B4b | SSGI ray query / 探针动态化 | GPU 机：`make gpuCheck` + SSGI 场景 |
| B5a imoScene 阴影 fault | 需真机复现 | amdgpu：`IMO_SCENE_SHADOWS=1 timeout 20 ./build/imoScene` |
| E2/E3 视觉 A/B | 需真机截图 | 4 级阴影矩阵 / sRGB-warp 前后截图 |
| P2b 真实 layout/管线 + shader 槽位迁移 | 无 `/dev/dri`，改绘制高风险 | GPU 机：`make gpuCheck` 99 条 + 目视；保留 `GN_BINDING_LAYOUT_LEGACY` A/B |
| P3/P4 GPU 接线（vkRenderQueue/逐层提交/多 pass） | 需 GPU 验证 | GPU 机：分层渲染冒烟 + 像素断言 |
| DA-M1r 3 处旧 ABI | 需用户决策 | 三选一：保持 / 升 ABI / 逐处迁移 |
| B7b 异盘备份 | 外盘只读（EROFS） | 重挂 rw：`udisksctl unmount -b /dev/nvme1n1p1 && udisksctl mount -b /dev/nvme1n1p1 -o rw`；再 `bash .dsh/backupProject.sh` |
| DA-M3 UI Vulkan | 缺 `assets/fileicon/` 样本 + GPU | 补样本后在 GPU 机恢复合并 |

## 4. CPU 剩余：DA-M2（26 个全局，分批）

| 批 | 范围 | 状态 |
|---|---|---|
| 1a | `GN_Input.cpp` 17 个 → `sINPUT_CONTEXT` | done |
| 1b | IME 4 个（On/OnRecv/Count/Char）→ `GN::sIME_CONTEXT` + `GN::GetImeContext()` | done |
| 2a | `GN_UiFBrowser.cpp` 15 个 → `sUI_FB_CONTEXT`（含 token-paste 宏改写） | done |
| 2b | `GT_FilePreviewGenList` → `sUI_FB_PREVIEW_CONTEXT`（依赖本地类 `GT_cFILE_PREVIEW_GEN`） | **done（2026-11-22 00:18）**：make exit=0、unit 463/0、lint 0 告警；文件 1972 有效行属历史超限 |
| 3 | `GN_Init` 6 + `GN_Sys` 4 + `GN_Sound` 3 | pending（注意 Sys 为平台文件、部分可能被 extern 引用，先查 extern） |
| 4 | 其余约 12 个（每文件 1–2：`GN_2D`、`GN_2Dtype`、`GN_File`、`GN_NetBase`、`GN_NetTrans`、`GN_Main`、`GN_Parallel`、`GN_AmCtrl`、`GN_UiPopDlg*` 等） | pending |

**每批验收**：`make` exit=0、`make unit` 463/0、lint 0/0（历史超限文件只记录不顺手拆）、同盘备份 + B7b 更新 + BACKLOG/协作/记忆回写。

## 5. 下一轮顺序（自动目标会用）

1. ~~**DA-M2 2b**~~：**已完成 2026-11-22 00:18**（结构体插在类定义后，替换 2 处引用；make exit=0、unit 463/0）。
2. **批 3**：先 `rg` 这 13 个名字的 extern 引用；`GT_SysDir/GT_AppDir/GT_TimeFreq` 等若是跨 TU 公共量，走访问器或保留并在台账注明。
3. **批 4**：逐个 1–2 个全局收敛，按文件收尾。
4. 全部 done 后：`goal` 标 complete；终态备份；给用户「仍需你做的两件事」：
   - 真机 GPU：`make gpuCheck`、`IMO_SCENE_SHADOWS=1 timeout 20 ./build/imoScene` ×3、E2/E3 截图 A/B；
   - DA-M1r ABI 决策（保持 / 升 ABI / 逐处迁移）。

## 6. 关键锚点

| 内容 | 位置 |
|---|---|
| 逐项状态 + §8 收敛台账 | `docs/BACKLOG.md` |
| 协作记录（含 CSM/E3/G1b/P2a/P3/P5/DA-M2 各批） | `docs/ENGINE_COORDINATION_2026-11-21.md` §5 |
| 多着色器计划 | `docs/MULTILAYER_RENDER_PIPELINE_PLAN.md`（P0–P6） |
| 记忆 | `M-known-defects-34…43`（F1 误报、CSM 闭环、G1b、P3、P5、DA-M2 批与两个坑） |
| 备份 | `.local/backups/GnVk-20261121T153906Z.tar.zst`（+`.sha256` 记录在 B7b） |
