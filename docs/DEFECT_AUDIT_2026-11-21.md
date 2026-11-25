# DEFECT_ANALYSIS 残余条目复核（B5e，2026-11-21）

> **状态**：📊 快照 — 2026-11-21 对 `DEFECT_ANALYSIS.md` 的逐条复核，行号固定于当日。

> 复核对象：`docs/DEFECT_ANALYSIS.md`（61 行，最后提交 `fdd4af3` 2026-07-04 16:51:58 +0800）
> 复核口径：只读（rg / sed / cat / wc / git log / 读代码）。本任务**禁跑** `make`、`make shaders`、`make unit`
> （`build/`、`assets/shaders/*.spv` 归并发子任务 B5d），故凡需重编才能核的数字一律标注「不可在当前树复算」。
> 锚点纪律：每条判定给「文件:行」；原文只给计数、不给条目的，写「原文无锚点」，不补造行号。

## 0. 条目数与计数核对（先对账，再复核）

| 口径 | 来源 | 条数 |
|---|---|---|
| 进度表「剩余」列相加 | `DEFECT_ANALYSIS.md:9-12`（5 + 5 + 4 + 8） | **22** |
| 「剩余已知问题」表实际枚举 | `DEFECT_ANALYSIS.md:53-61`（表头 53-56，条目 57-61） | **5** |
| 差集（只有计数、无条目） | 5 critical + 5 high + 1 medium + 6 low | **17** |

枚举的 5 条按原文严重级标记：🟡 Medium 3 条（第 57、58、61 行），🟢 Low 2 条（第 59、60 行）。
原文声称的残余 5 critical / 5 high **一条都没有列出**，因此这 10 条无法逐条复核（见 §2）。

## 1. 逐条复核（枚举的 5 条）

### DA-M1 · `memcpy(this)` UB (cFRAME_RC)
- **严重级**：🟡 Medium（`DEFECT_ANALYSIS.md:57`）
- **原文摘要**：「`memcpy(this)` UB (cFRAME_RC)｜实测安全, 标记 TODO」
- **当前锚点**：
  - `src/core/GN_Frame.cpp:102-105` — 免责注释 + `TODO: refactor to field-by-field assignment for C++ compliance.` **仍在**
  - `src/core/GN_Frame.cpp:106-116` — 宏 `GT_TMP_FRAME_RC_COPY_DATA`：`memcpy` 已换成 `110-113` 的 `char*` 逐字节循环（`for(_i...) dst[_i]=s[_i];`），UB 形态未消除，只是换了写法
  - `src/core/GN_Array.h:1663-1668` — 旧的三行 `memcpy(&tmp, this, ...)/memcpy(this, &ls, ...)` **已整块注释掉**（`1667` 的 `memcpy(this, &ls, tsize);` 只存在于注释内），实际走 `1659-1662` 的 `GN_Swap`
  - `src/core/GN_Array.h:3658-3671` — `GT_RE_CONSTRUCTOR` 在非 MSVC 分支已是 `new(this) CTYPE()`（即原文第 20-22 行那条「已修复」确实落地）
- **判定**：**仍成立**（形态已变：`memcpy(this)` → 逐字节覆写；`GT_RE_CONSTRUCTOR` 那半已修复，`cFRAME_RC` 那半未修）
- **复算读码依据**：`sed -n '95,130p' src/core/GN_Frame.cpp`、`sed -n '1655,1680p' src/core/GN_Array.h`、`rg -n 'GT_TMP_FRAME_RC_COPY_DATA|GT_RE_CONSTRUCTOR' src/`
- **最小复现路径**：静态可证（无需运行）。运行层复现需 `-fsanitize=undefined` 构建后走 `cFRAME_RC::operator=`；本任务禁构建、仓内无 UBSan 产物 ⇒ **运行时未复现，仅静态形态成立**。

### DA-M2 · 全局可变状态 57+ 个
- **严重级**：🟡 Medium（`DEFECT_ANALYSIS.md:58`）
- **原文摘要**：「全局可变状态 57+ 个｜需 Context 封装」
- **当前锚点**：**原文无锚点**（只给数字，未给文件/行/清单）
- **本次代理计数**（规则：`.cpp` 中第 0 列定义、变量名带 `GT_`/`z` 前缀的文件级定义）：**61**
  - 旁证：头文件中 `extern` 声明 **123** 条（`rg -n '^\s*extern\s' src -g '*.h' | wc -l`）；文件级 `static` 变量/函数启发式计数 22
- **判定**：**仍成立**（量级与「57+」相符；因原文无清单，精确复现不可能，61 是代理口径而非原作者口径）
- **复算命令**：`rg -n '^[A-Za-z_][A-Za-z0-9_:<>]*[ \*]+(GT_|z)[A-Za-z0-9_]+(\[[^]]*\])?\s*(=|;)' src -g '*.cpp' | wc -l` → `61`
- **最小复现路径**：上面的 rg 计数即复现；「是否可去全局化」需设计评审，非本审计范围。

### DA-M3 · UI 组件 Vulkan 适配（cFILE_BROWSER / cUI_SCENE）
- **严重级**：🟡 Medium（`DEFECT_ANALYSIS.md:61`）
- **原文摘要**：「UI 组件 Vulkan 适配｜cFILE_BROWSER/cUI_SCENE 等」
- **当前锚点**：
  - `src/ui/GN_UiFBrowser.cpp:586-587` — `// Vulkan: 跳过OpenGL纹理合并(texture数组未使用, 避免heap corruption)` + `(void)_img;` ⇒ 图标图集合并**被显式跳过**（功能退化，不是修好）
  - `src/ui/GN_UiFBrowser.cpp:1459` — 仍按 `ad->icon.texture[...]` 与 `((GNg::sIMAGE *)icon_data)->texture[0]` 做交换，说明 `texture` 数组语义仍在用
  - `src/ui/GN_UiFBrowser.cpp:576`、`:585` — 依赖 `fileicon/file_icon.ini`、`fileicon/file_icon`
  - 全库 OpenGL 调用残留：`rg -n '\bgl[A-Z][A-Za-z]*\s*\(' src/ | grep -v -i glfw | wc -l` → **0**（GL 代码已清除，「GL 代码不兼容」这一描述已不适用）
  - `docs/VULKAN_MIGRATION.md:54-55` — 同一状态仍标 ⚠️（`cFILE_BROWSER` / `cUI_SCENE`）
- **判定**：**仍成立**（描述形态已改：从「GL 纹理合并代码不兼容」变为「合并被跳过、texture 数组未使用」；适配缺口仍在）
- **为何无法运行时复现**：① 本任务禁构建、禁跑 GUI；② 本 checkout **缺样本资源**——`ls assets/fileicon` → `没有那个文件或目录`，`assets/` 下只有 `imoScene/probes/shaders`，`samples/` 也不存在，`GN_UiFBrowser.cpp:576/585` 需要的 `file_icon(.ini)` 无从加载 ⇒ 文件浏览器图标路径连初始化都跑不到。

### DA-L1 · 编译警告 150+
- **严重级**：🟢 Low（`DEFECT_ANALYSIS.md:59`）
- **原文摘要**：「编译警告 150+｜不影响功能」
- **当前锚点**：**原文无锚点**（只给数字）。旁证取自仓内既有构建日志 `build/lead_make_check35.log`（2026-10-28 00:55，4.47 MB，73 个 `CC` 编译单元）：
  - `grep -c 'warning:' build/lead_make_check35.log` → **14636**
  - `-W` 分布（同日志，`grep -o '\[-W[a-z-]*\]' | sort | uniq -c | sort -rn`）：`-Wunused-parameter` 5446、`-Wreorder` 3693、`-Wignored-qualifiers` 2447、`-Wmissing-field-initializers` 1293、`-Wdeprecated` 846、`-Wclass-conversion` 188、`-Wstrict-aliasing` 171
  - 样例行：`build/lead_make_check35.log:23` → `src/core/GN_Base.h:270:12: warning: comparison of integer expressions of different signedness … [-Wsign-compare]`
- **判定**：**仍成立**（量级远超 150；但「当前树的确切条数」**不可复算**——本任务禁跑 make，且该日志早于当前工作树 24 天、只编译了 73/139 个 `.cpp`，属增量构建）
- **最小复现路径**：`make 2>&1 | grep -c 'warning:'`（**本任务禁跑**，留给父体/后续批次；`src` 下共 139 个 `.cpp`，全量数应更大）

### DA-L2 · 32 处 strcpy TODO
- **严重级**：🟢 Low（`DEFECT_ANALYSIS.md:60`）
- **原文摘要**：「32 处 strcpy TODO｜需人工审计缓冲区大小」
- **当前锚点**：**原文无锚点**（只给数字、无位置）。现状：
  - TODO 口径已清零：`rg -n 'TODO: verify buffer size' src | wc -l` → **0**（B5f 已收口，起点实为 36 处，见 `docs/BUFFER_SIZE_AUDIT.md:3`）
  - 但裸 `strcpy(` 仍有 **65** 处：`rg -n '\bstrcpy\s*\(' src | wc -l` → `65`（原文「32」与实际起点「36」均与当前计数不符）
  - 对照：`strncpy(` **109** 处、`strcat(` **6** 处
  - 计数前 3 的文件：`src/core/GN_Format.cpp` 12、`src/graphics/GN_2D.cpp` 9、`src/ui/GN_UiPopDlg.cpp` 6
  - 样例：`src/core/GN_Format.cpp:2869` `strcpy(out, upper?"ogg,OGG,":"ogg,");`、`:2979-2980` `strcpy(out, …)` + `strcpy(out + 19, …)`，`src/ui/GN_UiFBrowser.cpp:1223` `strcpy(gpath0 + opendir_len, "/..");`
- **判定**：**已修复**（就本条「32 处 strcpy TODO」而言：TODO 悬挂标记已 36→0 清零，`GN_StrCopyToLimit` 已落地于 `src/ui/GN_UiDataIO.cpp:302`、`src/ui/GN_UiPopDlgMenu.cpp:300/448`、`src/ui/GN_UiPopDlgRun.cpp:575/579/589`、`src/ui/GN_UiPopDlgInner.h:92` 等调用方；行号已随 2026-11-21 UI 拆分（`GN_UI.cpp`→`GN_UiDataIO.cpp`、`GN_UiPopDlg.cpp`→`Menu/Setup/Run/IO/Inner.h`）同步刷新）——**残余风险**：65 处裸 `strcpy` 未挂 TODO，其缓冲区判定见 `docs/BUFFER_SIZE_AUDIT.md`，不在本条原文的 TODO 口径内
- **最小复现路径**：两条 rg 计数即复现（上列命令）。

## 2. 未枚举的 17 条（原文只有计数）

`DEFECT_ANALYSIS.md:9-12` 声称残余 5 critical / 5 high / 4 medium / 8 low，但 `:53-61` 只列了 3 medium + 2 low。差集 17 条**没有任何条目文本**，逐条复核不可能：

| 严重级 | 原文声称残余 | 已枚举 | 缺条目 | 可复核性 |
|---|:--:|:--:|:--:|---|
| 🔴 Critical | 5 | 0 | 5 | **原文无锚点**，无条目文本 ⇒ 无法复核 |
| 🟠 High | 5 | 0 | 5 | **原文无锚点**，无条目文本 ⇒ 无法复核 |
| 🟡 Medium | 4 | 3（:57/:58/:61） | 1 | **原文无锚点**，无条目文本 ⇒ 无法复核 |
| 🟢 Low | 8 | 2（:59/:60） | 6 | **原文无锚点**，无条目文本 ⇒ 无法复核 |
| 合计 | 22 | 5 | **17** | — |

对这 17 条只做了一处**限定范围的旁证抽查**（不构成判定，仅供父体判断是否需要补清单）：
原文 `:27-28` 声称「+25 处 VkCheck 覆盖全部 vkCreate/vkAllocate 关键路径」，而当前树 `rg -n 'VkCheck' src/ | wc -l` → **32** 处使用，`vkCreate*/vkAllocate*` 调用点 **213** 处（其中同行带 `VkCheck` 的仅 7 处，其余多为多行写法或经返回值转发）。是否每个调用点都经统一错误检查需逐函数读码，**本审计未展开**，只指出「覆盖全部…关键路径」的量化措辞偏强。

## 3. 汇总表

| 判定 | 条数（枚举的 5 条内） | 条目 |
|---|---:|---|
| 仍成立 | 4 | DA-M1、DA-M2、DA-M3、DA-L1 |
| 已修复 | 1 | DA-L2 |
| 无法复现 | 0 | — |
| 描述已过时 | 0 | — |
| 原文无条目（仅计数，无法判定） | 17 | 5 Critical + 5 High + 1 Medium + 6 Low |
| **合计** | **22** | = `:9-12` 剩余列 5+5+4+8 ✅ |

按严重级看已判定的 5 条：🟡 Medium 3 条（3 条全部「仍成立」）、🟢 Low 2 条（1 条仍成立、1 条已修复）。

**条目数核对**：枚举条目 5 = `:57-61` 的行数 ✅；计数口径 22 = `:9-12` 的 5+5+4+8 ✅；两者差 17 ✅。原文两处口径**互相矛盾**，是本次复核最大的结构性发现。

## 4. 复算命令（全部只读，工作目录 `/home/ruicat/下载/GnVk/GnVk`）

```bash
# 0 计数对账
sed -n '5,12p' docs/DEFECT_ANALYSIS.md; sed -n '53,61p' docs/DEFECT_ANALYSIS.md
git log -1 --format='%h %ad %s' --date=iso -- docs/DEFECT_ANALYSIS.md   # fdd4af3 2026-07-04 16:51:58 +0800

# DA-M1
rg -n 'GT_TMP_FRAME_RC_COPY_DATA|GT_RE_CONSTRUCTOR' src/   # GN_Frame.cpp:106/118/124, GN_Array.h:3658/3667
sed -n '95,130p' src/core/GN_Frame.cpp                     # 102-105 注释+TODO, 106-116 宏(110-113 逐字节循环)
sed -n '1655,1680p' src/core/GN_Array.h                    # 1659-1662 GN_Swap；1663-1668 memcpy(this) 整块注释

# DA-M2
rg -n '^[A-Za-z_][A-Za-z0-9_:<>]*[ \*]+(GT_|z)[A-Za-z0-9_]+(\[[^]]*\])?\s*(=|;)' src -g '*.cpp' | wc -l   # 61
rg -n '^\s*extern\s' src -g '*.h' | wc -l                                                              # 123

# DA-M3
sed -n '575,595p' src/ui/GN_UiFBrowser.cpp        # 586-587 跳过纹理合并
sed -n '1450,1462p' src/ui/GN_UiFBrowser.cpp      # 1459 icon.texture[] 交换
rg -n '\bgl[A-Z][A-Za-z]*\s*\(' src/ | grep -v -i glfw | wc -l   # 0
ls -d assets/fileicon                             # 无此目录（缺样本）

# DA-L1（日志旁证，非构建）
grep -c 'warning:' build/lead_make_check35.log    # 14636
grep -o '\[-W[a-z-]*\]' build/lead_make_check35.log | sort | uniq -c | sort -rn | head

# DA-L2
rg -n 'TODO: verify buffer size' src | wc -l      # 0
rg -n '\bstrcpy\s*\(' src | wc -l                # 65
rg -n '\bstrncpy\s*\(' src | wc -l               # 109
rg -n '\bstrcat\s*\(' src | wc -l                # 6

# 未枚举 17 条的旁证抽查
rg -n 'VkCheck' src/ | wc -l                                              # 32
rg -n 'vkCreate[A-Za-z]*\s*\(|vkAllocate[A-Za-z]*\s*\(' src/ | wc -l   # 213
```

## 5. 越界发现（只提交，未处理）

| # | 位置 | 发现 | 影响 |
|---|---|---|---|
| 1 | `DEFECT_ANALYSIS.md:9-12` vs `:53-61` | 剩余总数 22 与枚举 5 条自相矛盾，17 条无文本 | 该文档无法作为缺陷台账使用；任何「剩余 22 条」的引用都是不可核的 |
| 2 | `DEFECT_ANALYSIS.md` 全文 | 最后提交 2026-07-04，早于当前树 4 个多月 | `:60` 的 strcpy 计数（32）已被 B5f 证伪（实际起点 36，现 0）；`:27-28` 的 VkCheck 量化偏强 |
| 3 | `src/ui/GN_UiFBrowser.cpp:586-587` | 图标纹理合并被「跳过」而非实现（注释自述 `避免heap corruption`） | 文件浏览器图标功能在 Vulkan 下是缺省关闭的，属功能缺口而非已修 |
| 4 | 本 checkout | `assets/fileicon/` 与 `samples/` 不存在 | `GN_UiFBrowser.cpp:576/585` 的资源加载路径在此 checkout 无法跑通；是否有意裁剪需父体判断 |
| 5 | 全库 | 裸 `strcpy` 65 处、`strcat` 6 处无 TODO 标记 | B5f 只覆盖了「TODO: verify buffer size」口径（36→0），这 71 处未纳入任何台账 |

## 6. 本次复核的边界声明

- 写域：仅新建本文件 `docs/DEFECT_AUDIT_2026-11-21.md`；源码、`docs/DEFECT_ANALYSIS.md`、`docs/BACKLOG.md`、`docs/BUFFER_SIZE_AUDIT.md`、`build/`、`assets/shaders/*.spv` **全部只读**。
- 未执行 `make` / `make shaders` / `make unit`，未 `git commit` / `stash` / `revert` / `checkout` / `clean`。
- 判定「仍成立」但注明「不可在当前树复算」的（DA-L1）需要父体安排一次全量构建才能定量。

## 7. 后续处置（B5g，2026-11-21）

| 本报告的判定 | 后续处置 | 证据 |
|---|---|---|
| DA-M1 仍成立 | **已修复**：`operator=`/`CopyData` 改逐字段复制；`zSound` 不再共享（消除双重释放） | `src/core/GN_FrameCopy.cpp:9`、`GN_Frame.cpp:101/:113`、`tests/unit/testFrameCopy.cpp`；`make unit` 438/0 |
| DA-L1 不可在当前树复算 | **已定量**：全量 `make -B` 24600 条 `warning:`（139 个 `.cpp`） | `build/full_build_warn_20261121.log` |
| DA-L2 已修复 | 维持；其「残余 65 处裸 strcpy」升级为 **71 处逐处台账**（含 6 处 `strcat`）：B5g/B5h/B5i 加固 22 行，现存裸调用 65 | `docs/STRCPY_AUDIT_2026-11-21.md` |
| §5-1 文档自相矛盾 | **已处置**：`docs/DEFECT_ANALYSIS.md` 重写为可复核台账（5 条逐条 + 17 条计数退役） | `docs/DEFECT_ANALYSIS.md` §1–§2 |
| §5-2 原文过期 | 同上：旧口径不再作为台账引用；历史修复记录标注「未经本次复核」 | `docs/DEFECT_ANALYSIS.md` §3 |
| §5-3 图标合并被跳过 | 维持：DA-M3 登记 blocked（缺 `assets/fileicon` 样本 + GPU） | `docs/BACKLOG.md` DA-M3 行 |
| §5-4 缺样本 | 维持：`assets/fileicon/`、`samples/` 仍不存在；未猜补 | `ls assets/fileicon` |
| §5-5 71 处无台账 | **已处置**：逐处登记（B5i 后：加固 22 / 契约 3 / 已核 45 / 非代码 2），契约残留 3 处（旧 ABI）登记为 DA-M1r | `docs/STRCPY_AUDIT_2026-11-21.md`、`docs/BACKLOG.md` DA-M1r |
| §2 未枚举 17 条 | **已退役**：无条目文本、无法复核；不再作为「剩余缺陷」引用 | `docs/DEFECT_ANALYSIS.md` §2 |

