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

## 5. 2026-11-26 扩展警告审查批（DA-1126-*）

方法：GCC 13 扩展警告（`-Wall -Wextra -Wshadow -Wduplicated-cond -Wduplicated-branches -Wlogical-op
-Wuse-after-free=2 -Woverloaded-virtual`）全量语法检查（19385 条，多为头文件风格噪声）
+ `-fanalyzer` 对 13 个高风险模块的跨过程分析（**0 报告**）+ 近两周新增模块人工审查。
修复明细与回归测试见 `docs/CHANGELOG.md` 2026-11-26 条目；本节只留锚点与复算入口。

### 5.1 已修复（13）

| ID | 锚点 | 复算 |
|---|---|---|
| DA-1126-01 提取闭合符分支恒不进入 | `src/core/GN_String.cpp:356` | `make unit` → `stringExtractToData*` 2 例 |
| DA-1126-02 HDR 保存格式判定恒真 | `src/core/GN_FormatImgFile.cpp:839` | `make unit` → `hdrSave*` 2 例 |
| DA-1126-03 软键盘哨兵 short 溢出变正 | `src/input/GN_Input.cpp:956,958` | `grep -n 'softKeyboard[XY] = ' src/input/GN_Input.cpp` → -32768 |
| DA-1126-04 声音宏实参自初始化 UB | `src/core/GN_FrameRc.cpp:512` | 扩展警告 `-Wsequence-point` → 0 |
| DA-1126-05 函数地址当条件恒真 | `src/ui/GN_UI.cpp:735,740` | `grep -n 'RenderTargetSupport()' src/ui/GN_UI.cpp` → 2 处调用 |
| DA-1126-06 基类析构非虚 | `src/core/GN_Frame.h:140,315`、`src/core/GN_Linear.h:41` | `grep -n 'virtual ~' src/core/GN_Frame.h src/core/GN_Linear.h` |
| DA-1126-07 胶囊距离采样高估（斜交漏算） | `src/graphics/GN_CapsuleSweep.cpp`（capsuleMinDistance） | `make unit` → `capsuleMinDistance*` 2 例 |
| DA-1126-08 近接触等距探针探穿薄墙 | `src/graphics/GN_CapsuleSweep.cpp`（近接触分支） | `make unit` → `sweepSlideAlongFloorIntoThinWall` |
| DA-1126-09 边框缝隙优先级 >>3 | `src/ui/GN_UiPopDlgSetup.cpp:707` | `grep -n 'zBorderW =' src/ui/GN_UiPopDlgSetup.cpp` |
| DA-1126-10 鼠标滚动步进减半 | `src/ui/GN_UiTextIn.cpp:694,696` | `grep -n 'zOffsetN2) + 1' src/ui/GN_UiTextIn.cpp` → 2 处 |
| DA-1126-11 焊接量化键 int32 回绕 | `src/graphics/GN_MeshTools.cpp`（weldNormalsByPosition 校验） | `make unit` → `meshWeldRejectsUnquantizablePosition` |
| DA-1126-12 布料网格 int 乘法溢出 | `src/graphics/GN_3Dcloth.cpp`（create） | `make unit` → `clothCreateRejectsOversizedGrid` |
| DA-1126-13 GBK 尾字节下限检查恒假 | `src/ui/GN_UiTextIn.cpp:270` | `grep -n "text_code\[1\] < " src/ui/GN_UiTextIn.cpp` → `< 0xa0`（无 (char) 转换） |

### 5.2 已核安全 / 刻意保留（有警告但不是缺陷）

- `src/graphics/GN_2Dscene.cpp:147-149`、`src/core/GN_FormatAudio.cpp:250,254`（`delete this`）、
  `src/core/GN_Parallel.h:122,136,139`：`-Wdelete-non-virtual-dtor` 警告，但均为**按精确类型删除**，
  且无更深层派生类；今后若新增派生类需补虚析构。
- `src/ui/GN_UiTextIn.cpp:600,630` `w << (3 - bit)`：意图存疑（也可能是 `(w<<3)-bit`），
  已加显式括号**保持现行为**，待真机文本框验证后再定口径。
- `src/ui/GN_UiBase.cpp:1030` `(rng.h - ih2) >> 1`：语义是"箭头区域居中"，现行为正确，仅加显式括号。
- 7 处 `memset`/`memcpy` 非平凡类型（`GN_ShadowCascade.cpp:131`、`GN_UI.cpp:767`、
  `GN_UiPopDlgIO.cpp:250,262`、`GN_UiBase.cpp:2498,2500,2515`）：已核全 POD 布局 / 无虚表，
  `(void*)` 转换明示意图。

### 5.3 批量风格警告（本批不动，DA-L1 口径更新）

扩展警告扫描 19385 条（2026-11-26，语法检查口径）：`-Wreorder` 7860、`-Wignored-qualifiers` 5316、
`-Wmissing-field-initializers` 2734、`-Wdeprecated` 1440（access declarations）、`-Wunknown-pragmas` 641
（`#pragma comment`）、`-Wshadow` 496、`-Wclass-conversion` 320、`-Wwrite-strings` 180、
`-Wsign-compare` 167、`-Wswitch` 61（多有 default 兜底）。
复算：`find src -name '*.cpp' | xargs -P2 -I{} g++ <扩展flags> -fsyntax-only {}`（flags 见 CHANGELOG 本轮条目）。

### 5.4 审查批新发现（2026-11-26 分析报告；P0 已全部修复，2026-11-26）

| ID | 锚点 | 判定 |
|---|---|---|
| DA-1126-14 全角转 ASCII 死代码 | `src/core/GN_String.cpp:906`（GN_StrSymbolToSingleChar） | **已修复**：改 `uint8` 比较 + 尾字节缺失安全终止；`stringSymbolToSingleChar*` 2 例 |
| DA-1126-15 回调签名双重转型 | `src/ui/GN_UiBase.cpp`（SetZoningDrawCall / InitStyleToData / 分派点）、`src/ui/GN_UiBase.h`（zDrawCallHasUsArg） | **已修复**：分派改看注册签名标志 `zDrawCallHasUsArg`；`InitStyleToData` 清 `zUsArg`；`SetZoningDrawCall(fn9, 0)` 也按 9 参分派；`scrollListDrawCallSignatureFlag` 1 例。存储 cast 保留（往返转型），登记不修 |
| DA-1126-16 角色头注释过期 | `src/graphics/GN_Character.h:14`、`src/graphics/GN_Entity.h:342` | **已修复**：改为现行口径（可经 SetPhysicsWorld 单向推动态刚体） |
| DA-1126-17 缺口账本 D4 过期 | `docs/REMAINING_GAPS.md` D4 行 | **已修正**（2026-11-26）：deploy key 可用，容器内直接 push |
| DA-1126-18 SetZoningDrawCall 空指针 | `src/ui/GN_UiBase.cpp`（SetZoningDrawCall 尾行） | **已修复**（P0.2 测试暴露）：注册先于帧初始化时 `zFFrames->zMax_num` 解引用空指针；加空守卫。复现：构造后直接 SetZoningDrawCall |
| DA-1126-19 cornell 光追自检规则陈旧 | `Makefile` cornellRtProbe/GpuRtProbe/HwRtProbe | **已修复**（B8 parity 暴露）：三个目标含 cornellCpuRt.cpp（`#include <GN.h>` 且调用 GN3d:: 引擎函数），旧规则缺 `$(DEFINES) $(INCLUDES)` 与 `-lGN_mini`，编不过（不在 ci 内故长期未暴露）；补头路径/宏/链接与 $(TARGET) 依赖，CMake 侧同步 |

复算：`make unit` → `stringSymbolToSingleChar*` 2 例 + `scrollListDrawCallSignatureFlag` 1 例（943/0）。

## 6. 相关台账

- `docs/DEFECT_AUDIT_2026-11-21.md` — B5e 逐条复核（判定、锚点、17 条差集、越界发现）。
- `docs/STRCPY_AUDIT_2026-11-21.md` — 71 处裸 `strcpy`/`strcat` 逐处登记（加固 9 / 契约 22 / 已核 39 / 非代码 1）。
- `docs/BUFFER_SIZE_AUDIT.md` — B5f 的 36 处 buffer-size TODO 收口记录。
- `docs/BACKLOG.md` — 唯一待办账本（DA-M2/M3 的 planned/blocked 行在此）。
