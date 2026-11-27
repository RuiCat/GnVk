# 文档索引

`docs/` 下有 30 多份文档，写于不同阶段：有的是**必须遵守的现行口径**，有的是
**某一次工作的记录**，还有的是**被后来的决定推翻的设计**。只看文件名看不出区别 ——
按「最新修改时间」判断更危险，因为这个仓库每次批量改动都会 touch 全部文档。

所以这张表是唯一的入口：**每份文档开头都有一行 `> **状态**：`，内容与本表一致**
（由 `tests/tools/check_docs_index.sh` 双向校验，见文末）。

---

## 从哪读起

| 你想做什么 | 读这些 |
|---|---|
| 第一次上手、跑起来 | [GETTING_STARTED.md](GETTING_STARTED.md) → [API_REFERENCE.md](API_REFERENCE.md) §10 |
| 写 3D 游戏逻辑（实体/场景/角色/物理） | [API_REFERENCE.md](API_REFERENCE.md) §10 → [ENTITY_CONTROLLERS_PLAN.md](ENTITY_CONTROLLERS_PLAN.md) |
| 改渲染/光照/阴影 | [L4_L3_CONTRACT.md](L4_L3_CONTRACT.md)（冻结契约）→ [AXIS_CONVENTIONS.md](AXIS_CONVENTIONS.md)（坐标） |
| 不知道现在还剩什么没做 | [REMAINING_GAPS.md](REMAINING_GAPS.md) |
| 想知道某个改动为什么这么写 | [CHANGELOG.md](CHANGELOG.md)（按时间倒序，带实测数字） |
| 想知道某个旧方案为什么被推翻 | 本表「历史」组，每份开头都写了被谁取代 |

---

## 状态图例

| 标记 | 含义 | 可以照着做吗 |
|---|---|---|
| ✅ 现行 | 仍是权威口径，改代码前要看 | 可以 |
| 📋 已完成 | 某轮工作的完整记录，正文即背景资料 | 参考，不必遵守 |
| 🚧 进行中 | 尚未收口，正文含未完成项 | 看已完成部分 |
| 📦 历史 | 已被取代，或只描述当时的快照 | **不要照着做** |
| 📊 快照 | 某一天的量化审计结果，数字固定于当日 | 数字可复算，结论可能已变 |

---

## 一、入门与参考（✅ 现行）

| 文档 | 状态 | 一句话 |
|---|---|---|
| [GETTING_STARTED.md](GETTING_STARTED.md) | ✅ 现行 | 编译、依赖、最小应用骨架；含无 GPU（lavapipe）跑法 |
| [API_REFERENCE.md](API_REFERENCE.md) | ✅ 现行 | API 手册：2D/UI/音频/网络（§1–§9）+ **3D 实体层导航（§10）** |
| [COMMENT_STYLE.md](COMMENT_STYLE.md) | ✅ 现行 | 注释与文档写法：头文件三段式、契约写声明处、数字要有出处、TODO 不进代码 |
| [AXIS_CONVENTIONS.md](AXIS_CONVENTIONS.md) | ✅ 现行 | 四套空间的约定与转换收口点；改任何几何/投影前先看 |
| [CHANGELOG.md](CHANGELOG.md) | ✅ 现行 | 按日期倒序的变更记录，每轮带实测数字与边界披露 |

## 二、架构与契约（✅ 现行）

| 文档 | 状态 | 一句话 |
|---|---|---|
| [ARCHITECTURE_UNIFICATION.md](ARCHITECTURE_UNIFICATION.md) | ✅ 现行 | 架构统一 A1–A7 的问题清单与落地结果（分层 lint、错误入口、数学层、GPU 释放、上下文拆分） |
| [L4_L3_CONTRACT.md](L4_L3_CONTRACT.md) | ✅ 现行 | L4 大气散射 + L3 阴影的**冻结契约**（UBO 布局/绑定槽/公式/验收） |
| [DEFECT_ANALYSIS.md](DEFECT_ANALYSIS.md) | ✅ 现行 | 缺陷台账：每条带「文件:行」锚点与复算命令 |
| [ENTITY_CONTROLLERS_PLAN.md](ENTITY_CONTROLLERS_PLAN.md) | ✅ 现行 | 实体层设计：四控制器 + 固定步长时钟 + 场景层，P0–P6 的设计依据 |

## 三、进度与待办（✅ 现行）

| 文档 | 状态 | 一句话 |
|---|---|---|
| [REMAINING_GAPS.md](REMAINING_GAPS.md) | ✅ 现行 | **当前缺口清单**（唯一待办账本）：A/B/C/D 四组，标「已完成 / 已交付未启用 / 需资产 / 需桌面」 |

## 四、已完成的工作（📋 / 🚧）

| 文档 | 状态 | 一句话 |
|---|---|---|
| [CAMERA_PLAN.md](CAMERA_PLAN.md) | 📋 已完成 | C0–C5：`cCAMERA` 移动原语 + 自由/轨道/跟随 + 自动取景 |
| [AXIS_UNIFICATION_PLAN.md](AXIS_UNIFICATION_PLAN.md) | 📋 已完成 | 统一坐标系迁移（U0–U5）；残余项是真机视觉复验 |
| [GI_PLAN.md](GI_PLAN.md) | 📋 已完成 | G0–G5：G-Buffer 补齐、IBL、SSAO、SSGI、探针 |
| [LIGHTING_PLAN.md](LIGHTING_PLAN.md) | 📋 已完成 | L0–L8 路线图（阴影/大气/云/后处理）；未做项见 REMAINING_GAPS |
| [SKY_WEATHER_PLAN.md](SKY_WEATHER_PLAN.md) | 📋 已完成 | S1/S2a 天空与天气；L5 体积云**已被取代**（见 L5_CLOUD_CONTRACT） |
| [PROBE_BAKE_PLAN.md](PROBE_BAKE_PLAN.md) | 📋 已完成 | 探针烘焙 Stage 2；§10 列的 DDGI 异步更新等未做 |
| [CORNELLBOX_PLAN.md](CORNELLBOX_PLAN.md) | 📋 已完成 | 独立 Cornell Box（`tests/cornellBox`），自由相机 + 光追预览 |
| [MULTILAYER_RENDER_PIPELINE_PLAN.md](MULTILAYER_RENDER_PIPELINE_PLAN.md) | 📋 已完成 | 多层渲染设计；CPU 侧（`GN_RenderPipeline` / `GN_RenderQueue`）已落地，GPU 全路径分期推进 |
| [LINUX_CLIPBOARD_VOLUME_PLAN.md](LINUX_CLIPBOARD_VOLUME_PLAN.md) | 📋 已完成 | X11 剪贴板 + ALSA 音量（OSS `/dev/mixer` 已废弃） |
| [REFACTOR_PLAN.md](REFACTOR_PLAN.md) | 🚧 进行中 | 工程化重构：C 彻底粒度已裁决（2026-11-26），G2 复核完成，冻结任务图 §13 |
| [B7_STATIC_LEDGER.md](B7_STATIC_LEDGER.md) | ✅ 已完成 | B7 static 收敛账本（c4）：精确口径、批次计划、逐变量清单、B7-5 审计记录 |
| [B9_QUALITY_GATE_PLAN.md](B9_QUALITY_GATE_PLAN.md) | 🚧 进行中 | B9 质量门全量（c7）执行账本：去 -fpermissive、C++17、告警预算、字符串安全、解析器 fuzz |
| [ENGINE_RT_GI_PLAN.md](ENGINE_RT_GI_PLAN.md) | 🚧 规划 | 把屏幕空间 GI 升级为 ray query；目前只在 `tests/cornellBox` 里验证过，引擎侧未接 |
| [GI_RT_TYNDALL_PLAN.md](GI_RT_TYNDALL_PLAN.md) | 🚧 规划 | 光追 GI + 丁达尔；**无对应着色器，未实现** |
| [NEXT_PHASE_PLAN.md](NEXT_PHASE_PLAN.md) | 🚧 规划 | 审查批（620bd0e）后的执行顺序：P0 遗留小修 → P1 容器内功能 → P2 真机/资产 → P3 设计批次 |

## 五、历史（📦 不要照着做）

| 文档 | 状态 | 取代者 / 保留原因 |
|---|---|---|
| [ENGINE_ARCHITECTURE.md](ENGINE_ARCHITECTURE.md) | 📦 历史 | 取代者：代码 + [API_REFERENCE.md](API_REFERENCE.md)；保留原因：描述 OpenGL 时代的模块分层 |
| [VULKAN_MIGRATION.md](VULKAN_MIGRATION.md) | 📦 历史 | 取代者：同上；保留原因：Vulkan 迁移的完成记录（2026-07） |
| [FREETYPE_MIGRATION.md](FREETYPE_MIGRATION.md) | 📦 历史 | 取代者：同上；保留原因：字体从预生成字库迁到 FreeType |
| [3D_FEATURE_PLAN.md](3D_FEATURE_PLAN.md) | 📦 历史 | 取代者：[REMAINING_GAPS.md](REMAINING_GAPS.md)；保留原因：2026-10-31 的 3D 扩展快照（P0–P12），大部分已落地 |
| [3D_GPU_CONTRACT.md](3D_GPU_CONTRACT.md) | 📦 历史 | 取代者：代码里的 `static_assert` + [L4_L3_CONTRACT.md](L4_L3_CONTRACT.md)；保留原因：P1c/P2/P8 的 GPU 契约快照 |
| [L5_CLOUD_CONTRACT.md](L5_CLOUD_CONTRACT.md) | 📦 历史 | 取代者：`GN_Vk.h` 的 `sCLOUD_CUBE_*`；保留原因：体积云整层实现已删除，改为独立云块 |
| [BACKLOG.md](BACKLOG.md) | 📦 历史 | 取代者：[REMAINING_GAPS.md](REMAINING_GAPS.md)；保留原因：2026-11-21 的待办账本，条目可追溯 |
| [ENGINE_COORDINATION_2026-11-21.md](ENGINE_COORDINATION_2026-11-21.md) | 📦 历史 | 保留原因：两个并发会话的协作底稿，一次性用途 |

## 六、审计快照（📊 数字固定于当日）

| 文档 | 状态 | 日期 | 收口情况 |
|---|---|---|
| [SHADER_AUDIT_2026-11-03.md](SHADER_AUDIT_2026-11-03.md) | 📊 快照 | 2026-11-03 | Phase 0/1 + 低风险 P2 已落地；Phase 3 ray-query 未做 |
| [DEFECT_AUDIT_2026-11-21.md](DEFECT_AUDIT_2026-11-21.md) | 📊 快照 | 2026-11-21 | 对 DEFECT_ANALYSIS 的逐条复核 |
| [STRCPY_AUDIT_2026-11-21.md](STRCPY_AUDIT_2026-11-21.md) | 📊 快照 | 2026-11-21 | 71 → 65 处裸调用，逐条判定 |
| [BUFFER_SIZE_AUDIT.md](BUFFER_SIZE_AUDIT.md) | 📊 快照 | 2026-11-21 | 36 处 TODO → 0 |
| [REMAINING_WORK_SUMMARY_2026-11-21.md](REMAINING_WORK_SUMMARY_2026-11-21.md) | 📊 快照 | 2026-11-21 | 那一轮的收敛单页 |
| [CORNELL_RT_EXPOSURE_FIX.md](CORNELL_RT_EXPOSURE_FIX.md) | 📊 快照 | 2026-11-20 | 过曝/二次 sRGB 编码，已修复 |
| [REFACTOR_REDTEAM_2026-11-26.md](REFACTOR_REDTEAM_2026-11-26.md) | 📊 快照 | 2026-11-26 | C 粒度裁决后六攻击面复核；发现项回写 REFACTOR_PLAN §13/§4 |
| [LIGHT_PROBE_RT_ACCEL_ANALYSIS.md](LIGHT_PROBE_RT_ACCEL_ANALYSIS.md) | 📊 快照 | — | 光追探针加速结构分析 |

## 七、外部资料

| 文档 | 说明 |
|---|---|
| [papers/README.md](papers/README.md) | 论文库索引；按**不可信外部数据**对待，仅供算法参考 |

---

## 维护规则

1. **每份文档的开头有一行 `> **状态**：<标记> 现行/已完成/… — 一句话`**，与上表对应。
   新增文档必须同时加状态行并在上表登记；删除文档要同步删表项。
2. 状态变了（例如规划落地）**先改文档里的状态行和正文口径，再改本表** ——
   正文说「未实现」而实际已实现，比没有状态更糟。
3. 待办只记在 [REMAINING_GAPS.md](REMAINING_GAPS.md)。计划文档里的「后续步骤」只是当时
   的想法汇总，不是账本；完成与否以 REMAINING_GAPS 为准。
4. `make doc-check` 会校验：上表列出的文件都存在、`docs/*.md` 都出现在表里、
   文档里的状态行与本表一致、AUTO-STATS 数字未漂移。
