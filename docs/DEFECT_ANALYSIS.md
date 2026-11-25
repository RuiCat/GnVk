# GN_mini 引擎 — 缺陷台账（2026-11-21 复核后重写）

> **状态**：✅ 现行 — 缺陷台账：每条带「文件:行」锚点与可复算命令；只有带锚点的条目才进表。

> 本文件在 B5e 复核（`docs/DEFECT_AUDIT_2026-11-21.md`）之后重写。原版有两个结构性问题：
> ① 进度表「剩余」列相加 22 条，但「剩余已知问题」表只枚举 5 条；② 另外 17 条只有计数、没有条目文本，
> 无法逐条复核。因此本台账不再保留「剩余 22 条」这一不可核口径：**只有带锚点的条目才进本表**。
>
> 台账口径：每条 = 判定 + 「文件:行」锚点 + 一条可复算命令。判定取值：已修复 / 仍成立 / 阻塞 / 已退役。

## 1. 现状（原版枚举的 5 条）

| ID | 问题 | 原严重度 | 判定（2026-11-21） | 锚点 | 复算入口 |
|---|---|:--:|---|---|---|
| DA-M1 | `memcpy(this)` / 逐字节覆写（`cFRAME_RC` 拷贝） | 🟡 | **已修复（B5g，2026-11-21）**：改为逐字段复制，`zSound` 不再共享（原字节拷贝会让两个对象共用一个 OpenAL 发声源 → 双重释放）；自赋值早退 | `src/core/GN_FrameCopy.cpp:9`（`zCopyFields`）、`src/core/GN_Frame.cpp:101`（`operator=`）、`src/core/GN_Frame.cpp:113`（`CopyData`）、`src/core/GN_Frame.h:154` | `make unit` → `frameCopy*` 4 例（438/0），见 §4 |
| DA-M2 | 全局可变状态 57+ 个（需 Context 封装） | 🟡 | **仍成立（planned，设计批次）**：代理计数 61 个文件级 `GT_`/`z` 定义；`extern` 123 条。本批不实现 Context 封装 | 原文无锚点；复算：`rg -n '^[A-Za-z_][A-Za-z0-9_:<>]*[ \*]+(GT_|z)[A-Za-z0-9_]+(\[[^]]*\])?\s*(=|;)' src -g '*.cpp' \| wc -l` → 61 | 同上命令；入口 `docs/BACKLOG.md` DA-M2 行 |
| DA-M3 | UI 组件 Vulkan 适配（`cFILE_BROWSER`/`cUI_SCENE`） | 🟡 | **仍成立（blocked）**：图标纹理合并被显式跳过，`texture` 数组未用；当前 checkout 缺 `assets/fileicon/` 与 `samples/`，且需 GPU 才能验 | `src/ui/GN_UiFBrowser.cpp:586-587`（跳过合并）、`:1459`（`icon.texture[]` 交换）、`docs/VULKAN_MIGRATION.md:54-55` | `ls assets/fileicon` → 不存在；`rg -n '\bgl[A-Z][A-Za-z]*\s*\(' src/ \| grep -vi glfw \| wc -l` → 0 |
| DA-L1 | 编译警告 150+ | 🟢 | **仍成立；已定量（B5g，2026-11-21）**：全量重建 **24600** 条 `warning:`（139 个 `.cpp`，`-Wall -Wextra`） | `build/full_build_warn_20261121.log`（4.9 MB，本次全量 `make -B`） | `grep -c 'warning:' build/full_build_warn_20261121.log` → 24600；分布见 `docs/DEFECT_AUDIT_2026-11-21.md` §1 |
| DA-L2 | 32 处 `strcpy` TODO | 🟢 | **已修复（B5f，2026-11-21）**：`TODO: verify buffer size` 36→0；但当时未纳入台账的 71 处裸 `strcpy`/`strcat` 已在 B5g/B5h/B5i 逐处登记并加固：现存裸调用 65、加固 22 行（含 6 处改形）、契约残留 3（仅旧 ABI）、非代码 2 | `docs/BUFFER_SIZE_AUDIT.md`、`docs/STRCPY_AUDIT_2026-11-21.md` | `rg -n 'TODO: verify buffer size' src \| wc -l` → 0；`rg -n '\bstrcpy\s*\(|\bstrcat\s*\(' src \| wc -l` → 69 |

## 2. 已退役的计数（原版 17 条：5 critical / 5 high / 1 medium / 6 low）

原文只给计数、无任何条目文本（无文件、无行号、无复现），按「不可复核」退役：

| 级别 | 原声称剩余 | 有条目文本 | 处置 |
|:--:|:--:|:--:|---|
| 🔴 Critical | 5 | 0 | **已退役**：不经任何当前代码复核，不得再作为「剩余缺陷」引用 |
| 🟠 High | 5 | 0 | **已退役**：同上 |
| 🟡 Medium | 4 | 3（DA-M1/M2/M3） | 1 条无文本 → **已退役** |
| 🟢 Low | 8 | 2（DA-L1/L2） | 6 条无文本 → **已退役** |
| 合计 | 22 | 5 | 17 条退役；重建条目须按当前代码重新取证 |

## 3. 历史修复记录（2026-07，原文保留；均为当时口径，未经本次复核）

> 这些是「声称已修复」的历史记录，本次未逐条验证。其中 VkCheck 一条的量化措辞偏强：
> 当前树 `VkCheck` 使用 32 处、`vkCreate*/vkAllocate*` 调用点 213 处，同行带 `VkCheck` 的仅 7 处 —— 「覆盖全部关键路径」不可据此复现。

| 类别 | 原记录 |
|---|---|
| 🔴 严格别名 | `GN_ValuePackToHalf` / `GN_ValueFloatToHalf` / `GN_ValueUnPackHalf`、`GN_UiBase.cpp` 颜色位双关 |
| 🔴 `memcpy(this)` | `GN_Frame.cpp` 宏、`GN_Array.h` 的 `GT_RE_CONSTRUCTOR`（后者已改为 placement new；前者由 B5g 改为逐字段复制） |
| 🔴 strcpy/strcat | 98 处 → `strncpy` + 显式 NUL（12 个文件；口径见 B5f/B5g 台账） |
| 🔴 Vulkan 错误检查 | +25 处 `VkCheck`（见上方措辞保留） |
| 🟠 Vulkan 资源泄漏 | 默认 1×1 白纹理销毁、`GN_VkShutdown` 清理 |
| 🟠 光照系统 | 延迟光照 Cook-Torrance BRDF、方向光/聚光灯 `enableShadow`、`Clear()` 恢复 `shadowMapSize`、PBR IBL F0→Fresnel-Schlick |
| 🟠 视频播放 | FFmpeg 8.x matroska（`AVFMT_FLAG_IGNIDX`）、EOF 循环、`GN_SetWindowPos` |
| 🟡 RADV GPUVM fault | 渲染 pass 内 `vkQueueSubmit` → 帧首暂存上传（代码现状见 `vkFrame2d.cpp:115-128`） |
| 🟡 Makefile | 合并重复 `.o` 规则、自动依赖生成（`DEPFLAGS`） |

## 4. 复算入口

```bash
cd /home/ruicat/下载/GnVk/GnVk
# 台账本身：逐条复核报告（DA-M1..DA-L2 的锚点与差集）
sed -n '1,60p' docs/DEFECT_AUDIT_2026-11-21.md
# 全量警告定量（约 3–6 分钟；本文件引用的 24600 来自该命令）
make -B -j$(nproc) > build/full_build_warn_20261121.log 2>&1; grep -c 'warning:' build/full_build_warn_20261121.log
# DA-M1 回归
make unit    # 438 cases / 0 failed，含 frameCopy* 4 例
# DA-M2 代理计数
rg -n '^[A-Za-z_][A-Za-z0-9_:<>]*[ \*]+(GT_|z)[A-Za-z0-9_]+(\[[^]]*\])?\s*(=|;)' src -g '*.cpp' | wc -l   # 61
```

## 5. 相关台账

- `docs/DEFECT_AUDIT_2026-11-21.md` — B5e 逐条复核（判定、锚点、17 条差集、越界发现）。
- `docs/STRCPY_AUDIT_2026-11-21.md` — 71 处裸 `strcpy`/`strcat` 逐处登记（加固 9 / 契约 22 / 已核 39 / 非代码 1）。
- `docs/BUFFER_SIZE_AUDIT.md` — B5f 的 36 处 buffer-size TODO 收口记录。
- `docs/BACKLOG.md` — 唯一待办账本（DA-M2/M3 的 planned/blocked 行在此）。
