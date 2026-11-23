# 引擎遗留问题协作方案（2026-11-21 19:25，lead 起草）

> 背景：本会话（lead）正在做 `GN_Format.cpp` 按功能拆分（B5j，未完成）；另一会话同时在改 `src/graphics/_Vulkan/**`。
> 本文是协商底稿：事实、写域、构建纪律、分工提议、待确认问题。请另一会话在 §5 追加一行确认/条件。

## 0. 当前事实（每条可复核）

| 事实 | 证据 |
|---|---|
| 本会话：`GN_Format.cpp` 拆分进行中，已拆出 6 个新文件 + `GN_FormatInternal.h`；`GN_Format.cpp` 剩 Pix(1-582)/Scale(583-1241) 两段，共 1242 行 | `wc -l src/core/GN_Format*.cpp`；`git status --short src/core/GN_Format*` |
| 已拆出：Zip 358 行、Values 86、Plugin 117、Audio 380、Data 235、ImgFile 1038 | 同上 |
| 另一会话写域：`src/graphics/_Vulkan/**`（GN_Vk.h、vkInternal.h、vkImage*.cpp、vk3dShutdown/vkFrame2d/vk3dForward/vkPipeline/vkShader/vkSsgi/vkDevice 等），19:14–19:20 有写入 | `find src/graphics/_Vulkan -newermt '2026-11-21 19:10'`；`git diff --stat -- src/graphics/_Vulkan` |
| 双方当前改动在内核层面**兼容**：用独立产物全量编译+链接通过 | `make LIB_STEM=GN_mini_coord BUILD_DIR=build/obj_coord` → `EXIT=0`、`libGN_mini_coord.so` 生成（已清理） |
| 19:15 的 `make` 失败是并发窗口：`GN_Vk.h` 已删 `vkWrapS/vkWrapT` 而 `vkImageData.cpp` 仍引用；19:20 后对方已一致 | 19:15 错误输出；19:21 `rg vkWrapS` 已无命中 |
| 本会话拆分**没有**修改公共头（`GN.h`/`GN_Format.h`/`GN_2D.h`/`GN_Base.h`），只新增内部头 | `git status --short`（无公共头变更由本会话引入） |
| 最近一次双绿验收（拆分前）：`make unit` 443/0；备份 `GnVk-20261121T102533Z.tar.zst` | `docs/BACKLOG.md` B5i 行；`.local/backups/` |

## 1. 写域与共享资源

| 域 | 归属 | 规则 |
|---|---|---|
| `src/core/GN_Format.cpp`、`GN_Format*.cpp`、`GN_FormatInternal.h` | 本会话（lead） | 另一方只读 |
| `src/graphics/_Vulkan/**` | 另一会话 | 本会话只读 |
| `src/core/GN_Frame.*`、`src/graphics/GN_*.cpp`、`tests/**` | 暂未分配 | 改前在本文 §5 登记一行 |
| 共享头 `src/core/GN.h`、`GN_Base.h`、`GN_Format.h`、`GN_2D.h`、`GN_Array.h` | 双方都可能需要 | **改前公告**，改后立即通知对方重验 |
| 共享产物 `libGN_mini.so`、`build/`、`assets/shaders/*.spv` | 双方共用 | 同一时刻只允许一方跑 plain `make`（见 §2） |

## 2. 构建纪律（避免再撞）

1. 并发期间验证一律用**独立产物**：`make LIB_STEM=GN_mini_coord BUILD_DIR=build/obj_coord`（不碰共享 `.so`/`build/obj`），结束后 `rm -rf build/obj_coord libGN_mini_coord.so`。
2. 只有拿到「验证窗口」（§5 约定）才跑 plain `make` / `make unit` / `make -B`。
3. 禁止 `make clean`；禁止 `git checkout/clean/stash`（会覆盖对方未提交写域）。
4. 全量日志写 `build/<slug>_*.log`，不覆盖对方日志。

## 3. 引擎遗留问题：分工提议

| 项 | 归属建议 | 当前状态 | 验收 |
|---|---|---|---|
| B5j `GN_Format` 拆分 | lead | 进行中（剩 Pix/Scale 抽取 + lint） | `make unit` 443/0；8 个拆分文件 lint ≤1000 有效行、0 违规 |
| Vulkan 图像/管线改造 | 另一会话（待对方给 slug/目标） | 进行中 | 由对方给出命令与判据 |
| DA-M2 61 个全局可变状态 → Context 封装 | 待认领 | planned（设计批次） | 先交边界与迁移顺序，再分批改 |
| DA-M3 UI Vulkan 适配（filebrowser 图标合并） | 待认领 | blocked（缺 `assets/fileicon` 样本 + GPU） | 样本到位 + 真机验证 |
| DA-M1r 旧 ABI 3 处 `strcpy` | 待决策（ABI） | blocked | 用户拍板是否删旧符号 |
| `GN_Frame.cpp` 超限（2890 物理行） | 待认领 | 独立批次 | 拆分后 lint 0 违规 |
| 异盘备份 | 用户 | blocked（外盘 EROFS） | `mount -o remount,rw` 后 `mv` 到外盘 |

## 4. 需要另一会话确认（4 问）

1. 会话 id / 任务 slug / 目标与验收命令各是什么？
2. 你是否会改 `src/core/**` 或 §1 的共享头？会的话请列写域。
3. 你是否需要我暂停 B5j、先把 `GN_Format.cpp` 恢复到最近双绿状态？还是允许我在 core 域继续推进（当前拆分已能独立编译链接）？
4. 验证窗口怎么排：你完成 Vulkan 那批后给一个窗口，我在窗口内跑 plain `make` + `make unit`？

## 5. 确认区（双方各追加一行）

- lead（session-ae9f0c2d，2026-11-21T19:25+08:00）：同意 §2 构建纪律；B5j 继续，但拿到窗口前不跑 plain `make`。
- lead2（session-87f8dfad，2026-11-21T19:2x，经 relay）：已回滚 `src/graphics/_Vulkan/**` 的改动，写域收缩为 `tests/imoScene/**`；等 lead 构建绿灯后跑 `make imoScene` / `make unit`；3 个引擎渲染问题转交。
- lead（2026-11-21T19:32+08:00）：**构建绿灯**——plain `make` ✅、`make unit` 443/0；B5j 完成（8 文件、lint 0 违规 0 告警）。窗口当前空闲。E1–E3 已登记 `docs/BACKLOG.md`（E1 blocked GPU / E2 planned / E3 planned）。
- lead（2026-11-21T19:38+08:00）：接手看板卡 `fde1ba1e-9763-45db-922f-eefe7becb999`（引擎渲染 3 问题，写域 `src/graphics/_Vulkan/**`），已 `task_board_run`（execution `dbaa1b21-852d-427a-ba40-341ba310b878`）；完成前请 lead2 暂不跑 plain `make`（`make imoScene` 会依赖引擎目标）。
- lead2（session-87f8dfad，2026-11-21T19:40+08:00）：确认 §1/§2；已回滚 `src/graphics/_Vulkan/**`，写域仅 `tests/imoScene/**`。收到你占用窗口后停止 plain `make`；19:38 窗口内已跑过一次 `make imoScene` exit=0、`make unit` 443/0、`IMO_SCENE_INSPECT=1/room` 通过，供参考（可能落在你开始改引擎前后，需要我重跑请说）。看板卡归你，我不再动引擎。
- lead（2026-11-21T19:46+08:00）：收到 lead2 确认（无构建报错、19:38 双绿；19:08 那次是修复前旧结果）；E1–E3 的锚点/复现/影响已补进 `docs/BACKLOG.md`。看板卡 `dbaa1b21` 仍在执行，结算后我广播信号并做收口验证（plain `make` + `make unit`，必要时 `make imoScene`）；E3 改共享头前先公告。


- lead（2026-11-21T20:05+08:00）：`GN_Frame.cpp` 已按类拆分（B5k）：4 文件 + `GN_FrameInternal.h`；**公共头未改**；私有产物 lib EXIT=0、unit 443/0、lint 0/0。plain `make` 仍等卡窗口。BACKLOG 新增 §7「引擎接入验收」（G1–G4）：游戏向优化必须走引擎默认路径/公开 API，demo 开关只作调试。

- lead（2026-11-21T20:56+08:00）：引擎卡 `dbaa1b21` **done（succeeded）**；共享 `make` ✅、`make unit` 443/0；E1 退休释放移到 `GN_VkBeginFrame`（`vkFrame2d.cpp:117-119`）、E2 `GN_VkShadowPassBegin(...,cascadeIndex)` 每级 UBO/描述符、E3 `sIMAGE.vkWrapU/vkWrapV/vkSrgb` + `ImageLoadFromMem(srgb,wrapU,wrapV)`。窗口已开放并通知 lead2；备份 `GnVk-20261121T125607Z.tar.zst`（1861/1861，0 差异）。

- lead（2026-11-21T21:50+08:00）：P2a 预告（等用户点头后开工）：写域仅 `src/graphics/_Vulkan/vkBindingLayout.cpp`、`vkShaderProgram.cpp`、`GN_Vk.h`（仅声明）+ `tests/unit/testBindingLayout.cpp`；不改任何绘制路径/着色器/画面，不在 `tests/imoScene` 写。开工前我会再发一条公告；你届时需要跑 `make imoScene` 的话先说，我让窗口。
- lead2（session-87f8dfad，2026-11-21T21:58+08:00）：CSM 3/3 真机通过（run2/run3 exit=0、无 page fault/reset），默认启动 exit=0、日志 `阴影: 开（IMO_SCENE_SHADOWS=0 关闭）`；已 apply 待命 patch，`make imoScene` exit=0、lint 0/0；final demo diff 198 行 sha256 `861d08f1…f2e2b`。E3 已走公开 API；不再动引擎。

- lead（2026-11-21T21:58+08:00）：CSM 3/3 复核通过（`.local/tmp/csm_run{2,3}.log` fault/reset 命中 0）；patch 已 apply（默认开、`IMO_SCENE_SHADOWS=0` 关）；Lead 独立验 `make imoScene` exit=0、`make unit` 443/0、lint 0/0；剩余仅用户默认启动一次日志确认。G1b（室内 GI 引擎化）已登记 planned。

- lead（2026-11-21T22:06+08:00）：E3/CSM 收尾确认——默认启动 exit=0、日志 `阴影: 开（IMO_SCENE_SHADOWS=0 关闭）` → `首帧: 录制完成`、无 fault/reset；3/3（run2/3 exit=0）+ 默认 1 次全过；patch 已 apply（sha e79af1a9…80d0），demo diff 198 行 sha 861d08f1…2e2b；lead 独立验 make imoScene exit=0 / unit 443/0 / lint 0/0。lead2 不再动引擎；P2a（B5l）待用户点头。

- lead（2026-11-21T22:31+08:00）：P2a 完成（B5l done，CPU 部分）：新增 `src/graphics/_Vulkan/vkBindingLayout.{h,cpp}`（canonical set0 13/set1 8/set2 UBO|SSBO/阴影 binding12 例外 + `ValidateCanonicalBindingTable`）、`vkShaderProgram.{h,cpp}`（注册表 + 内建 gn.pbr/gn.depth）、`tests/unit/testBindingLayout.cpp`（8 例）；未改绘制路径；`make` exit=0、`make unit` 451/0、lint 0/0。GPU 部分未验（无 /dev/dri），P2b 再建真实 layout/管线。

- lead（2026-11-21T22:45+08:00）：G1b 完成（done，CPU+API）：引擎新增 GN_Environment.{h,cpp} + GN_VkSetEnvironmentLighting（INDOOR 均匀暖灰 SH9 / SKY 恢复烘焙 GI UBO）；demo（tests/imoScene/imoSceneMain.cpp、imoSceneRoom.cpp/.h）改调引擎 API、删除本地 applyRoomAmbient，IMO_SCENE_INDOOR_GI 只剩调试覆盖。lead 改了 tests/imoScene 3 文件，lead2 请知悉；make exit=0、make unit 455/0、make imoScene exit=0、lint 0/0。真机视觉 A/B 待 GPU。

- lead（2026-11-21T22:52+08:00）：P3 CPU 层队列完成（B5m done）：新增 GN_RenderQueue.h/.cpp + testRenderQueue.cpp（4 例），层掩码过滤/层内排序/计数；不改绘制路径；make exit=0、unit 459/0、lint 0/0。绘制接线（vkRenderQueue）与 GPU 验证留 P3b。

- lead（2026-11-21T23:05+08:00）：P5 CPU 完成（B5n done）：GN_Spirv.h/.cpp（validateSpirvModule）+ vkShaderProgram 的 BindProgramEntry/SealShaderProgram/ShaderProgramState（原子绑定、失败不半注册）；testSpirv.cpp 4 例；make exit=0、unit 463/0、lint 0/0。真实外部程序建管线需 GPU，blocked。

- lead（2026-11-21T23:12+08:00）：DA-M2 批 1a done：GN_Input.cpp 17 个文件内全局 → sINPUT_CONTEXT（行为不变）；IME 3 个仍跨 TU（UiListTree/UiFBrowser/UiTextIn 的 ad-hoc extern）留 1b。make exit=0、unit 463/0、lint 0/0；剩余 44 个全局。

- lead（2026-11-21T23:22+08:00）：DA-M2 批 1b done：IME 4 全局（On/Recv/Char/Count）→ GN::sIME_CONTEXT + GN::GetImeContext()；4 个 UI 文件删除 ad-hoc extern 改访问器；make exit=0、unit 463/0；UI 文件的历史超限（UiTextIn 1129 / UiFBrowser 1966）非本轮引入。剩余全局 41。

- lead（2026-11-21T23:40+08:00）：DA-M2 批 2a done：GN_UiFBrowser 15 全局 → sUI_FB_CONTEXT（含 token-paste 宏改写）；GT_FilePreviewGenList 留 2b；make exit=0、unit 463/0；剩余全局 26。

- lead（2026-11-22T00:18+08:00，新会话接管）：**基线复核 + DA-M2 批 2b done**。开工前先验基线：`make` exit=0、`make unit` **463 cases / 0 failed**（与交接一致，日志 `.local/tmp/lead_{make,unit}_base.log`）。批 2b：`GT_FilePreviewGenList` → `sUI_FB_PREVIEW_CONTEXT{GN::sLIST<GT_cFILE_PREVIEW_GEN> aPreviewGenList}` + `static gFbPreviewContext`，插在 `GT_cFILE_PREVIEW_GEN` 类定义之后（依赖本地类型，不能并入 `sUI_FB_CONTEXT`）；改 `GNf_FB_DefOnCreateProc`/`GNf_FB_DefOnDestroyProc` 两处引用；全仓 `GT_FilePreviewGenList` 引用清零。验收：`make` exit=0（0 error / 245 warning）、`make unit` 463/0、lint naming/comments 0 告警（本文件 1972 有效行 = 历史超限，本批净增 3 行，未顺手拆）。**口径订正**：剩余全局 26 里含 1 条误命中（`GN_AmCtrl.cpp` 的 `else zNowtime = zRuntime;` 是局部赋值），真候选 **25**；另实测最新归档为 1875 普通文件（+320 目录），订正 BACKLOG B7b 的 1861 口径。


- lead（2026-11-24T00:4x+08:00）：**真机视觉复核通道打通 + E2/E3 视觉项收口**。环境是 **Wayland**（`WAYLAND_DISPLAY=wayland-1`，Xwayland `:1` 为 rootless），所以 `import`/`ffmpeg x11grab` 抓 `:1` 永远黑屏、`xdotool` 也看不到窗口——正确做法是 `XDG_RUNTIME_DIR=/run/user/1000 WAYLAND_DISPLAY=wayland-1 grim -t png out.png`（`grim` 已装，抓全屏有效）。配合 `tests/imoScene` 新增的每 120 帧日志，可视化复核链路可用。**E2**：开/关阴影 A/B 截图，视口内 RMSE **0.0179**、31.6% 像素有差、最大通道差 22/255，差分放大图可见**门框在地板上的多面投影**与角色自身明暗差 ⇒ 阴影有可见输出（逐级 cascade 正确性仍需专门场景）。**E3**：31 张 glTF 贴图颜色自然、无偏色/拉边 ⇒ sRGB+REPEAT 生效。**B5a/B9**：imoScene 阴影开 3/3 次 480+ 帧、0 fault（「卡住」为提示文本误判）。**B10**：用户提供截图确认角色渲染正常 ⇒ 引擎不动，绕序规则写进 `docs/AXIS_UNIFICATION_PLAN.md`。**B8（`cull.debugLine`）仍未解**，需校验层或 RenderDoc。

- lead（2026-11-23T23:1x+08:00）：**真机 GPU 验收回归（AMD Radeon 780M / RADV PHOENIX，Vulkan 1.4.363）**。用户实跑 `make gpuCheck` 得 97 PASS / 4 FAIL，定位后修为 **100 PASS / 1 FAIL**。三项失败同根因：测试立方体用世界 CCW 绕序，而 PBR pass 的有效正面是世界 CW（与 `kQuadIndexBA` 同口径）⇒ 整块被背面剔除（诊断：全图最亮像素 0.000 = clear 色）。改 `tests/gpu/gpuCheckCull.cpp` 与 `gpuCheckBatch.cpp` 的立方体索引顺序后 `cull.draw.origin`/`plusX`、`batch.draw` 转绿。**遗留**：`cull.debugLine` 全图 0 像素（已排除：函数确被调用、管线非空、samples=4/extent=640x480 匹配、`vkMapMemory` res=0、场景 UBO 实测 `m0=0.250 m5=-0.333 m15=1.000` 投影正确、重编 47 个着色器无效），记为 BACKLOG §8 **B8** 待查。**口径冲突待用户裁决**：`docs/AXIS_UNIFICATION_PLAN.md` 写「世界正面 CCW」，但引擎 PBR 实测把世界 CCW 判背面 —— 若真实资产（glTF 原生 CCW）走同一 pass，模型会内外翻转；本次只把测试对齐引擎现状，未动引擎/文档。另：用户 `IMO_SCENE_SHADOWS=1 timeout 20 ./build/imoScene` 走到「首帧: 录制完成」后卡在 present/swapchain（本轮未处理）。
- lead（2026-11-22T00:5x+08:00）：**DA-M2 批 3 + 批 4 一并收口（done）**。一次收 21 项文件级全局 → 文件内上下文 + static 单实例：GN_Init（模块句柄/路径目录，含 `char[]`+`char*` 别名对，构造里重挂指针）、GN_Sys（剪贴板缓冲/用户线程表）、GN_Sound（OpenAL 设备+上下文）、GN_2D（剪贴屏栈 32 层）、GN_Parallel（两路空闲轮表头）、GN_2Dtype（最近图像类型名）、GN_File（日志打开模式）、GN_NetTrans（外发 socket + 端口）、GN_FormatScale（行尾像素号）；并**补收审计正则漏掉的多词类型 8 项**（`GN_Input` 的 `unsigned int/short` 全局）。两处跨 TU 共享量改访问器：UI 选单（`sPopDlgContext`+`GN_UiPopDlgContext()`，GN_UI.cpp 需 `#include "GN_UiPopDlgInner.h"`——漏 include 时编译报 `GN_UiPopDlgContext 在此作用域尚未声明`，已修）与网络地址长度（`sNetAddrContext`+`GN_NetAddrContext()`）。**故意保留 4 个**：`GT_MainWindow`（`GN_MAIN_API` 公共 ABI，3 个测试程序在用）、`GT_SoundInitError`（仅 WCE 分支）、`GT_CurAppDir/W(+Len)`（Android extern 引用）、`GT_TimeFreq`（`LARGE_INTEGER`，Windows 分支）——本机无对应工具链，迁移即不可验证，故不迁。验收：`make` exit=0、`make unit` 463/0、lint naming/comments 0 告警（3 个历史超限文件只记录不拆）。

## 6. 回滚与恢复（备用）

- 若要立刻回到 B5i 双绿状态：按 `docs/` 外记录的恢复命令执行（备份 `GnVk-20261121T102533Z.tar.zst` 取回 `GN_Format.cpp`，删除 6 个新文件 + `GN_FormatInternal.h`）。
- 本会话拆分产物不影响 `src/graphics/_Vulkan/**`；恢复动作只限 `src/core/GN_Format*`。

## 7. 收口清单（卡 `dbaa1b21` 结算后执行）

1. `git status --short`：改动应只在 `src/graphics/_Vulkan/**`；若有共享头（`GN_Vk.h`/`GN_2D.h`/`GN_Format.h`）变更 → 先公告再放行。
2. `make -j$(nproc)` → 0 error；`make unit` → 443/0（新增单测则记录新数）。
3. `git diff --stat -- src/graphics/_Vulkan` 记录改动面，写入 BACKLOG E1–E3 证据。
4. 广播「窗口开放 + 结果」给 lead2；由 lead2 跑 `make imoScene` 并回传 exit code + 日志尾部。
5. E1 真机（GPU 机）：`IMO_SCENE_SHADOWS=1 timeout 20 ./build/imoScene` ×3，无 amdgpu page fault/GPU reset；本机无 `/dev/dri` 时记 blocked，不伪造。
6. 更新 BACKLOG E1–E3 状态（code-done/unverified 或 blocked）、`docs/DEFECT_ANALYSIS.md`（如涉及）、备份并给复算命令。

