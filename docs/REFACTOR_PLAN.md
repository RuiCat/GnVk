# GN_mini 工程化重构方案（候选 v0.1）

> **状态：候选，未冻结。** 本文是 G0–G3 草案：三个裁决点见第 10 节，独立红队复核留到执行启动前。
> **进度**：阶段 0 完成；阶段 1 部分完成（67 用例 + `make check` + ASan/UBSan；headless 受环境阻塞）；阶段 2 进行中（`vkDevice.cpp` / `vkImageIo.cpp` / `vkImage.cpp` / `vkDraw2d.cpp` / `vkFrame2d.cpp` / `vk3dMesh.cpp` / `vk3dPasses.cpp` / `vk3dForward.cpp` / `vk3dInit.cpp` / `vk3dShutdown.cpp` 已抽出，`GN_Vk.cpp` 3690→1054 行，有效代码 <1000）。
> **依据**：2026-10-26 本仓库实测——干净构建 0 error / 10,083 warnings；工作区 `libGN_mini.so` 链接 FFmpeg 8 已无法加载；
> `samples/08_cube3d.cpp` 当前编译失败；未提交改动 9 文件 / +571 −68。
> **范围**：本仓库 98 个 C++ 文件 / 58,359 物理行。**不新增渲染特性、不改 UI 视觉、不重写语言。**

---

## 0. 目标 / 验收 / 约束 / 未知（G0）

### 0.1 目标

把"能跑的引擎原型"变成"可持续维护的工程"，具体五件事：

1. **可构建、可运行、可验证**：干净克隆一条命令构建；测试与示例在 CI 可自动判定。
2. **结构可控**：源码文件按功能拆分（单功能 <500 有效行、多功能 <1000），模块依赖单向。
3. **状态可管理**：消除"渲染器 + UI + 资源"共用的隐式全局状态，引入显式上下文，为多实例/线程安全铺路。
4. **平台与构建诚实**：只声明真实支持的目标；平台代码按路径拆分；构建入口统一为 `make`。
5. **工程纪律落地**：测试门禁、命名/注释约束、版本单一来源、依赖与许可可追溯。

### 0.2 验收（整体）

- 干净 Linux 上 `git clone && make && make check` 全绿；
- 16 个示例全部编译通过；其中不依赖外部图片的 14 个在 headless Vulkan（Lavapipe 或 `VK_EXT_headless_surface`）下各跑 60 帧无错误，02/03 用测试图或断言"缺资源时优雅失败"；
- 单元测试覆盖容器/字符串/数学/颜色转换，且 ASan + UBSan 无告警；
- 除登记为 `no-split` 的文件外，无源码文件超过 500 有效代码行；
- 公共符号基线（`nm -D`）不变；文档版本号与代码一致。

### 0.3 约束与假设（**假设需裁决，标 A 的见第 10 节**）

| 编号 | 内容 |
|---|---|
| A1 | 项目仍以 **Linux 为唯一已验证目标**；macOS/Windows/Android 要么明示"不支持"，要么单独立项，不在本方案内实现。 |
| A2 | **公共 API 保持源码兼容**（`GN_*`/`c*`/`s*`/命名空间不改名），不承诺 ABI；如需破坏性变更另立大版本。 |
| A3 | 执行力量 1–2 人，按阶段推进，每阶段可独立停止；重构期冻结新功能。 |
| A4 | 依赖策略维持"轻量"：不引入大型框架；测试框架用单头文件或自带断言。 |
| A5 | 现有 `P<T>` 引用计数、`sLIST` 等自研容器**保留**，只做拆分与安全修正，不换成 STL 重写。 |
| A6 | 不承诺性能提升；重构只要求"不回退"，帧时间基准留作可选。 |

### 0.4 未知（开始前必须确认）

- U1 是否有仓库外的下游应用依赖当前 API/行为？（决定 A2 的严格程度）
- U2 是否真的需要 macOS/Windows/Android？（决定平台阶段的规模）
- U3 维护者是否接受 CMake 作为构建定义（`make` 仅作入口）？
- U4 CI 平台（GitHub Actions / GitLab / 仅本地）？无 CI 时以 `make check` 作为门禁。

---

## 1. 总原则

1. **绿构建不破**：任何一步合入后 `make` + `make check` 必须通过；红灯即回滚该步。
2. **测试先于拆分**：先有能判定的测试，再动结构；没有测试的模块先补最小行为测试。
3. **move-only 提交**：拆文件只搬运代码，不改逻辑；逻辑修改单独提交。评审时可用 `git diff --color-moved` 核对。
4. **契约先冻结再并行**：跨模块接口（上下文、后端、公共头）先定签名，再分发写域。
5. **一个 PR 一个写域**：单 PR ≤400 行、只动一个目录；跨目录改动串行。
6. **硬约束机检**：文件长度/命名/注释用 `lintCodeModularity.sh`；构建只用 `make <target>`；平台代码按路径拆。
7. **文档与代码同源**：版本号、命令、平台支持表只有一处真源。

---

## 2. 目标架构

### 2.1 依赖方向（单向，允许的边）

```
platform  →  core
file      →  core
audio     →  core
network   →  core
media     →  core, platform
graphics  →  core, platform
ui        →  core, graphics
```

禁止：`core → graphics/ui`、`graphics → ui`、`ui → network/media`、任何反向边。
`GN.h` 作为兼容伞头保留，但**新代码不得再被它隐式聚合**；各模块头必须能单独编译（self-contained header 测试）。

### 2.2 目标目录（分阶段迁移，旧路径以转发头保留）

```
src/
  core/
    containers/      array.h list.h listWheel.h tree2.h memPool.h tempBuf.h ringBuffer.h depot.h
    text/            string.cpp format.cpp path.cpp valueSerialize.cpp
    engine/          init.cpp frame.cpp runRec.cpp
  platform/
    include/         sys.h window.h thread.h clipboard.h volume.h     # 冻结接口
    linux/           sys.cpp window.cpp thread.cpp clipboard.cpp volume.cpp
  graphics/
    include/         image.h font.h type2d.h type3d.h backend.h       # 公共类型与后端接口
    common/          math2d.cpp math3d.cpp model.cpp light.cpp material.cpp fontFt.cpp
    vulkan/          vkDevice.cpp vkSwapchain.cpp vkImage.cpp vkUpload.cpp
                     vkFrame2d.cpp vkPipeline2d.cpp vkMesh3d.cpp
                     vkPassShadow.cpp vkPassForward.cpp vkPassDeferred.cpp vkComposite.cpp
  ui/                scroll.cpp listGui.cpp listTree.cpp textIn.cpp fileBrowser.cpp menu.cpp menuDialog.cpp
  ...
tests/               unit/ headless/ smoke/
assets/shaders/      *.vert *.frag（+ 构建生成的 .spv / *_spv.h）
```

> 目录名沿用现有小写习惯；**新文件名一律 camelCase**（`code-modularity` C7）。

### 2.3 上下文对象（替换隐式全局状态）

```cpp
namespace GN {
struct sEngineContext {            // 一个进程可有 N 个上下文
    sPlatform*   platform;
    sRenderer*   renderer;          // 后端接口，Vulkan 实现
    sResourceCache* resources;      // 图片/模型/字体缓存
    sFrameState  frame;             // 当前帧、批处理、延迟上传队列
    sInputState  input;
    sLog*        log;
};

sEngineContext* GN_ContextCreate(const sEngineConfig&);
void            GN_ContextDestroy(sEngineContext*);
sEngineContext* GN_ContextGetDefault();   // 兼容层：老 API 走默认上下文
}
```

- 迁移顺序：先建默认上下文 → 渲染器内部状态迁入 → 资源缓存 → 输入/UI → 删除 `static` 全局。
- 老 API（无上下文参数）成为默认上下文的薄封装，**行为不变**，示例无需改写。
- 新内部函数一律接收 `sEngineContext*`；不再新增文件级 `static` 可变状态。

### 2.4 后端接口（渲染）

```cpp
struct sRenderer {
    virtual bool init(const sRenderConfig&) = 0;
    virtual void shutdown() = 0;
    virtual bool beginFrame(sFrameState&) = 0;
    virtual void draw2d(const sDrawList&) = 0;
    virtual void draw3d(const sDrawList&) = 0;
    virtual void endFrame() = 0;
};
```

Vulkan 实现放 `graphics/vulkan/`；窗口/表面创建放 `platform/linux/window.cpp`（C11：平台差异按路径）。
未来加 macOS/Windows 后端 = 新增 `platform/<os>/` + 后端目录，**不改共享代码**。

### 2.5 命名与契约

- 公共契约：`GN_*` 函数、`cXXX`/`sXXX` 类型、`GNg`/`GN3d`/`GNu` 命名空间、`GN_*` 宏——**冻结**，头文件头部标 `code-naming: contract-names`。
- 新内部代码：文件/函数/方法/参数/变量 `camelCase`；类 `PascalCase`；常量 `UPPER_SNAKE`。
- 存量违规登记为技术债，机检只对新增/重写目录开 `--naming`，**只许减不许增**。

### 2.6 构建入口

- **唯一对外入口仍是 `make`**（C9）：`make` / `make debug` / `make test` / `make check` / `make shaders` / `make clean` / `make install`。
- 构建定义迁移到 **CMake + presets**（linux-release / linux-debug / asan / headless），`Makefile` 变薄封装：
  ```make
  build: ; cmake --preset linux-release && cmake --build --preset linux-release -j
  check: build ; ctest --preset linux-release
  ```
- 迁移期两套并存（`make BUILD=legacy` 保留旧路径），达到 parity 后删除旧规则。
- 反对意见保留：若维护者拒绝 CMake，改为保持手写 Makefile，拆成 `mk/{common,linux,samples,tests}.mk` 并补齐目标；阶段计划不变，阶段 4 工期 −30%。

---

## 3. 能力谱系与粒度（G1）

| 能力单元 | 输入 | 输出 | 不变量 | 验收 | 边界（不管什么） |
|---|---|---|---|---|---|
| c1 止血与基线 | 当前工作区 | 可构建、可运行的基线 tag | 公共符号不变 | `make && make test` 全过 | 不修历史欠账 |
| c2 测试与 CI | 现有示例/纯逻辑模块 | 单测 + headless smoke + CI | 测试不依赖 GPU 可降级 | `make check` 一条命令定成败 | 不做覆盖率指标 |
| c3 文件拆分 | 超限源文件 | 按功能拆分的文件树 | 行为等价、符号不变 | 行数机检 + 符号 diff | 不改逻辑 |
| c4 状态收敛 | 全局/静态状态 | `sEngineContext` | 老 API 行为不变 | 多实例测试 + 单测 | 不改渲染算法 |
| c5 资源与内存安全 | `sIMAGE`/上传队列/字符串 | 有界 API、RAII、无悬垂 | 无泄漏/无 UAF | ASan/UBSan + 压力测试 | 不做性能优化 |
| c6 构建与平台 | Makefile | CMake + make 封装 + 平台目录 | 一条命令全平台可声明 | 干净机 build | 不实现新平台 |
| c7 质量门 | 告警/命名/许可 | 告警预算、clang-tidy、LICENSE | 门禁可机检 | CI 红灯即拒绝 | 不追求零告警 |
| c8 文档与发布 | 版本/文档 | 单一版本源 + 准确文档 + ABI 基线 | 文档不撒谎 | 文档核对清单 | 不做站点 |

### 粒度候选

| 候选 | 内容 | 工期（1 人） | 并行度 | 集成风险 | 适合 |
|---|---|---:|---|---|---|
| **A 保守** | c1 + c2 + c3 只拆 `GN_Vk.cpp`/`GN_Array.h` | 2–3 周 | 低 | 低 | 只想继续做 demo |
| **B 标准（推荐）** | c1–c3 + c5 + c7 骨架 + c8 | 8–12 周 | 中（2 批并行） | 中 | 长期维护引擎 |
| **C 彻底** | B + c4 上下文 + c6 CMake/平台 + c7 全量 | 14–20 周 | 高（3–4 批） | 高 | 要做产品/多人协作 |

推荐 **B**：先拿到"可验证 + 结构可控"，把成本最高的 c4/c6 留作可选项。若目标是继续快速加特性，选 A 并在拆分文件上设"只许减"的债。

---

## 4. 约束清单（自检版，待红队替换）

> 每条都能用命令判定；标"自检"的由本方案作者提出，冻结前须经独立红队复核。

| # | 约束 | 判定方法 | 级别 |
|---|---|---|---|
| C1 | 公共符号不减少、签名不变 | `nm -D --defined-only libGN_mini.so \| sort > abi/after.txt; diff abi/baseline.txt abi/after.txt` 为空（新增可加） | 硬 |
| C2 | 单功能文件 <500 有效行，多功能 <1000 | `bash .dsh/skills/code-modularity/scripts/lintCodeModularity.sh src --naming --comments` 退出 0（存量另行登记） | 硬 |
| C3 | 核心层不得反向依赖图形/UI | `grep -rn '#include *"GN_\(2D\|3D\|Vk\|Font\|UI\)' src/core` 只允许出现在 `GN.h` 伞头 | 硬 |
| C4 | 平台差异不进共享代码 | `grep -rn '#if.*_WIN32\|#ifdef.*__linux__' src/core src/graphics src/ui` 无命中 | 硬 |
| C5 | 新代码无 `strcpy/strcat/sprintf` | `git diff --name-only <base>...HEAD \| xargs grep -nE '\b(strcpy\|strcat\|sprintf)\('` 为空 | 硬 |
| C6 | 新增状态必须进上下文 | 评审 + `git diff` 中新增 `^static .*;` 为 0 | 硬 |
| C7 | 新文件/函数 camelCase | lint 脚本 `--naming`（只跑新增目录） | 硬 |
| C8 | 模块头一句话职责 + 导出注释 | lint 脚本 `--comments` | 硬 |
| C9 | 构建只经 `make` | README/文档中不出现裸 `g++ ...` 构建命令；`make` 可从零构建 | 硬 |
| C10 | 头文件自包含 | 每个公共头单独 `g++ -fsyntax-only -x c++ header.h` 通过 | 硬 |
| C11 | 渲染失败必须可见 | debug 构建下 `VkCheck` 失败即 `abort()`；headless 冒烟对 `stderr` 中 `[GN_Vk] ERROR` 判失败 | 硬 |
| C12 | 延迟上传不持裸指针 | ASan 压力测试：同帧 `DataFill` 后 `Release`，无 UAF；队列满时有明确降级路径 | 硬 |
| C13 | 测试不得依赖真实 GPU | `make check` 在无 GPU CI 容器（Lavapipe）通过 | 硬 |
| C14 | 版本号单一来源 | 构建生成的头 `GN_Version.h` 与文档一致；`make version-check` | 硬 |
| C15 | 依赖与许可可追溯 | `external/` 每个目录有 `VERSION`/`LICENSE` 记录；根 `LICENSE` 存在 | 硬 |

---

## 5. 分阶段计划

> 每阶段可独立停止；停止时保证"绿构建 + 文档同步"。

### 阶段 0：止血与基线（1–2 天）

| 任务 | 文件/位置 | 验收 |
|---|---|---|
| 修 `float h` 重复声明 | `samples/08_cube3d.cpp:86-88` | `make test N=08` 通过 |
| 重编库（修复 FFmpeg ABI） | 全仓 | `ldd libGN_mini.so \| grep 'not found'` 为空；`./run.sh 1` 能启动 |
| `make tests` 变真门禁 | `Makefile:236-243` | 失败时非 0 退出；不再 `2>/dev/null` |
| `clean` 清测试产物 | `Makefile:260` | `make clean` 后 `build/test_*` 不存在 |
| `make test` 用 `$(CXX)` | `Makefile:233` | `CXX=g++-13 make test N=01` 生效 |
| 去掉 GLUT 残留依赖 | `Makefile:52` | `make` 不再需要 freeglut 头 |
| 修不存在的目标或删掉声明 | `Makefile:300-301`、`help` | `make help` 列出的目标都存在 |
| 版本单一来源 | `src/core/GN.h:5-7`、`docs/*` | 版本号一处定义、文档引用它 |
| 加 `LICENSE`、第三方许可清单 | 根目录、`external/` | 文件存在且列出全部内嵌代码 |
| 打基线 tag | git | `git tag baseline-2026-10` |

**写域**：`Makefile`、`samples/08_cube3d.cpp`、`src/core/GN.h`、根目录、`docs/`。
**回滚**：逐条独立提交，失败单条 revert。

### 阶段 1：测试与 CI 安全网（1–2 周）

| 任务 | 说明 | 验收 |
|---|---|---|
| 测试骨架 | `tests/` + 最小断言宏 + `make test` 聚合；不引入重型框架（A4） | `make test` 跑通空测试集 |
| 纯逻辑单测 | 容器（`sLIST`/`sTBUF`/`cTREE2`）、字符串、2D/3D 数学、颜色格式转换 | ≥60 个用例、无 GPU 依赖 |
| Headless 冒烟 | 新增 `GN_StartRunHeadless(w,h,frames,fn)`：`VK_EXT_headless_surface` 或离屏渲染，跑 N 帧退出 | 一个最小场景跑 60 帧、读回像素非全黑 |
| 示例冒烟 | 16 个示例全部编译；无外部资源依赖的 14 个在 Lavapipe + `VK_EXT_headless_surface` 下各跑 60 帧；02/03 用 1×1 测试图（`tests/fixtures/`）或断言加载失败路径 | `tests/smoke/runSamples.sh` 全过 |
| CI | build（GCC/Clang）→ unit → headless smoke → shader drift → ASan（纯逻辑） | PR 红灯即阻断合入 |
| 告警基线 | 记录 10,083 条按类分布；新代码 0 新增告警 | `make warnings` 输出对比 |
| 误报处理 | 对遗留告警分类：修正 / 局部 `#pragma`+理由 / 暂记 | 每类有处置结论 |

**写域**：`tests/`、`Makefile`/CMake、`.github/`（或 CI 配置）、`src/platform/` 的 headless 入口。
**风险点**：headless 扩展在部分驱动不可用 → 兜底 Xvfb + Lavapipe；两者都不可用时只跑纯逻辑测试并显式标 SKIP（不得静默）。
**退出**：`make check` 一条命令能在本地与 CI 判定本次改动。

### 阶段 2：结构拆分（3–4 周，可并行）

拆分规则：按功能切、不按行数均分；先补该文件行为测试，再 move-only 拆分；**不改逻辑**。

#### 2.1 `src/graphics/_Vulkan/GN_Vk.cpp`（3690 行 → 12 个文件）

| 目标文件 | 内容（原物理行区间，供切分参考） |
|---|---|
| `vkDevice.cpp` | 内存/缓冲辅助、`VkCheck`、初始化/关闭、resize（17–346、347–622、876–931） |
| `vkSwapchain.cpp` | swapchain/framebuffer/renderPass 创建与重建（并入上区间，按职责拆出） |
| `vkRenderTarget.cpp` | 离屏 RT 的 pass 开始/结束、`CopyOut`（82–186、673–798） |
| `vkImage.cpp` | `sIMAGE` 生命周期、`CreateEmptyToData`、`DataFill`、`Release`（87–284、1097–1105、1261–1594） |
| `vkUpload.cpp` | 延迟上传队列（618–621、806–813、1395–1402、1495–1502），并修复悬垂指针 |
| `vkImageIo.cpp` | PNG 加载、`ImageLoad/Save`、`DataGet`、readback（1106–1260、1625–1795） |
| `vkFrame2d.cpp` | scissor、批处理、Begin/EndFrame、顶点缓冲（623–672、694–875、918–931） |
| `vkDraw2d.cpp` | `ImageDraw*`、线、dither、alpha key（932–1096、1230–1260、1609–1624、1702–1719） |
| `vk3dInit.cpp` | 3D 初始化/关闭、缓冲容量（1797–2884 的装配部分、2885–2963） |
| `vk3dPipelines.cpp` | Forward/PBR/Shadow/Deferred 管线与描述符（从 1866–2884 拆出） |
| `vk3dDraw.cpp` | BeginPass/Skinned/PBR/HDR/mesh upload（2964–3423） |
| `vkPassShadow.cpp` / `vkPassDeferred.cpp` / `vkComposite.cpp` | 阴影（3424–3519）、延迟与 G-Buffer（3581–3690）、合成与 Skybox（3520–3580） |

> 每个目标文件必须 ≤500 有效行；`GN_Vk.h` 只保留公共类型与冻结签名，内部共享声明进 `vkInternal.h`。

#### 2.2 `src/core/GN_Array.h`（3654 行 → 10 个头）

按单元切：`iterator.h`（625–671、1115–1186）、`array.h`（672–893）、`bufferMapping.h`（894–926）、`memPool.h`（927–985）、`allocPool.h`（986–1092）、`list.h`（1187–1785）、`listWheel.h`（1786–2230）、`tempBuf.h`（2231–2352）、`ringBuffer.h`（2353–2386）、`depot.h`（2387–2526）、`tree2.h`（2527–3654）。保留 `GN_Array.h` 作为兼容聚合头（转发 include）。

#### 2.3 其他超限文件

| 文件 | 现状 | 拆分方向 |
|---|---:|---|
| `src/core/GN_Format.cpp` | 3205 | 按域切：`format/colorFmt.cpp`（54–511）、`format/pixelConvert.cpp`（512–1284）、`format/imageCodec.cpp`（1285–2085，再按 TGA/PNG/HDR 分）、`format/genImage.cpp`（2086–2310）、`audio/wavFile.cpp`（2311–2737）、`format/fileRegistry.cpp`（2738–2811）、`core/valueSerialize.cpp`（2812–2874）、`format/zip.cpp`（2875–3205） |
| `src/ui/GN_UiPopDlg.cpp` | 2786 | 菜单模型 `menu.cpp`（29–552）、对话框初始化/样式（553–780）、布局几何（781–1103）、绘制与输入（1104–1357）、滚动/值编辑（1358–2386）、`cUI_SIM_DIALOG` 适配（1297–1357、2387–2786） |
| `src/core/GN_Frame.cpp` | 2624 | `frameEvent.cpp`（30–66）、`frameRc.cpp`（67–683）、`frame.cpp`（684–796）、`frameLauncher.cpp`（797–2624 再按 2D/粒子分） |
| `src/ui/GN_UiBase.cpp` | 2380 | 按控件类切：`scrollCtrl.cpp`（30–186）、`scrollList.cpp`（187–1200）、其余 `c*` 控件各自成文件 |
| `src/platform/_Sys/GN_Sys.cpp` | 1526 | **按能力 + 平台**：`platform/include/` 冻结接口；`platform/linux/{time,thread,clipboard,volume,path,encode}.cpp`；删除共享代码里的 `#ifdef`（C4） |
| `src/platform/_Main/GN_Main.cpp` | — | `platform/linux/window.cpp` + `appLoop.cpp`；GLFW 只出现在此目录 |

#### 2.4 拆分门禁

```bash
bash .dsh/skills/code-modularity/scripts/lintCodeModularity.sh src --naming --comments
nm -D --defined-only libGN_mini.so | sort > abi/after.txt
diff abi/baseline.txt abi/after.txt        # 只允许新增
make check
```

**退出**：超大文件清零（除登记 `no-split`）、符号基线不变、示例与冒烟全过。

### 阶段 3：状态收敛与资源安全（3–4 周）

| 任务 | 说明 | 验收 |
|---|---|---|
| 建 `sEngineContext` + 默认上下文 | 老 API 委托默认上下文 | 门面单测：创建/销毁/两次创建互不影响 |
| 渲染状态迁入上下文 | 逐文件迁移 `static`（先 `GN_Vk.cpp` 40 个） | `grep '^static ' src/graphics/vulkan` 只允许常量 |
| 资源缓存独立 | 图片/模型/字体缓存从全局拆出 | 多上下文加载同一资源互不干扰 |
| 修 `sIMAGE` 值语义 | `operator=` 深拷贝或禁止拷贝；`CopyOut` 回退路径 | 单测：拷贝后互不影响；ASan 无 double free |
| 修延迟上传悬垂 | 队列项持引用/句柄；`Release` 取消 pending；满队列显式 flush 或报错 | 压力测试 1000 张纹理 + 同帧释放，ASan 干净 |
| 错误与日志 | `GN_Log` 分级；debug 下 `VkCheck` 致命即 abort；校验层开关 | 注入失败：验证输出含错误、debug 中止、release 记录 |
| 线程模型 | 文档化"渲染单线程 + 资源上传线程"；核心容器加锁边界 | TSAN 跑并发单测无告警 |

**写域**：`src/core/`、`src/graphics/`、`src/ui/` 的调用点；**串行推进**（上下文签名会波及全仓），按模块逐个合入。

### 阶段 4：构建与平台（2–3 周）

| 任务 | 说明 | 验收 |
|---|---|---|
| CMake 迁移 | presets + FindVulkan/FFmpeg/FreeType/OpenAL；`make` 薄封装 | 干净机 `make` 成功；`make help` 列出真实目标 |
| 目标矩阵 | 只声明 Linux 已支持；其他平台从构建与文档中移除或标 experimental | `make info` 不撒谎 |
| 依赖清理 | `#include<libzip/zip.c>` 改为独立编译；`external/` 加版本与许可；删除未用头 | 许可清单完整；构建不再内联 C 源 |
| Shader 单一来源 | 构建生成 `.spv` 与 `*_spv.h`；CI 校验无漂移 | `make shaders && git diff --exit-code assets/shaders` |
| 安装与导出 | `make install`、pkg-config `.pc` 或 CMake config | 下游工程可用 `find_package` |

### 阶段 5：质量与安全（与 2–4 并行，持续）

| 任务 | 说明 | 验收 |
|---|---|---|
| 移除 `-fpermissive` | 先修 C++ 非法写法，再删标志 | 无该标志可编 |
| 升级 C++17 | 保留 ABI 兼容；用 `std::string_view` 等逐步替换 | CI 双标准（11 兼容期/17 新代码） |
| 告警预算 | 10,083 → 阶段末 <500；新代码 0 新增 | `make warnings` 门禁 |
| 字符串安全 | 147 处 `strcpy/strcat/sprintf` → 有界 API；逐文件修 | 修改文件 0 命中；ASan 模糊测试 |
| 解析器加固 | PNG/ZIP/WAV/模型加载的边界检查与 fuzz 种子 |  oss-fuzz 风格本地 fuzz 1 小时无崩溃 |
| clang-tidy | 只对新增文件开启，规则集固定 | CI 无新增告警 |

### 阶段 6：文档与发布（1 周）

- 版本单一来源（`GN_Version.h` 生成），README/文档/代码一致；
- 重写 README：真实依赖、平台支持、`make` 目标、目录结构；
- 架构文档与代码树同步；`docs/DEFECT_ANALYSIS.md` 标历史；
- `CONTRIBUTING.md`（命名、文件长度、提交粒度、测试要求）；
- 发布清单 + ABI 基线随版本提交；
- 退役过期文档（`代码功能简述与规范.txt` 中与现状冲突的段落标注）。

---

## 6. 并行编排与写域（按阶段）

| 批次 | 任务 | 写域（互斥） | 依赖 |
|---|---|---|---|
| B0 | 阶段 0 全部 | `Makefile`、`samples/08_cube3d.cpp`、根目录、`docs/` | 无 |
| B1 | 测试骨架 + headless 入口 | `tests/`、`src/platform/`（新增文件）、CI 配置 | B0 |
| B2 | `GN_Vk.cpp` 拆分 | `src/graphics/_Vulkan/` | B1 |
| B3 | `GN_Array.h` 拆分 | `src/core/containers/` | B1 |
| B4 | `GN_Format.cpp` / `GN_Frame.cpp` 拆分 | `src/core/format/`、`src/core/frame/` | B1 |
| B5 | UI 拆分 | `src/ui/` | B1 |
| B6 | 平台拆分 | `src/platform/` | B5 |
| B7 | 上下文迁移 | `src/core/`、`src/graphics/`、`src/ui/`（**串行**，一次一个模块） | B2–B6 |
| B8 | CMake + 依赖清理 | `CMakeLists.txt`、`cmake/`、`Makefile`、`external/` | B2–B6（可与 B7 并行） |
| B9 | 文档与发布 | `docs/`、`README.md`、`LICENSE` | 全部 |

并发上限 4；B2–B5 可同批，但同一文件只能一个执行体碰。上下文迁移（B7）不得与拆分并行。

---

## 7. 风险与攻击面自检

> 本节是作者自检，**不能替代独立红队**（task-planning 要求换上下文对抗）。冻结前须由独立子智能体按六面攻击。

| 攻击面 | 自检结论 | 处置 |
|---|---|---|
| 假假设 | "samples 是唯一消费者"未验证；"Lavapipe 可用"未预演；"FFmpeg 9 运行时兼容"只验证了编译 | 先确认 U1；CI 预演 headless；FFmpeg 用容器固定版本 |
| 隐藏耦合 | `GN.h` 伞头 + `-Isrc/*` 扁平包含路径让任意模块可包含任意头；37 个可变全局 + 134 个文件级 static | 先加自包含头测试与依赖 lint，再拆；上下文分模块迁移 |
| 循环依赖 | `core` 的 `GN.h` 包含 graphics/ui，core 的 cpp 也直接包含 graphics 头 | 拆分期只新增"允许边"；`GN.h` 仅兼容伞头，新代码逐个包含 |
| 边界/失败/回滚 | 拆分 PR 混入逻辑修改会失去可回滚性；CMake 迁移期双构建可能产物不一致 | move-only 提交 + 符号 diff；双构建比对 `.so` 行为后再删旧 |
| 资源/权限 | 无 CI 账号、无 GPU；写工作区外备份需授权 | 本地 `make check` 为最低门禁；CI 可选；备份需用户批准 |
| 交付可验证性 | "重构完成"没有客观判定 | 每阶段命令 + 行数/告警/符号三类预算 + 冒烟结果 |

补充风险：

| 风险 | 影响 | 对策 |
|---|---|---|
| 上下文迁移破坏老 API | 示例/下游不可用 | 默认上下文门面 + 每模块回归；一次性只迁一个模块 |
| FFmpeg/驱动版本漂移 | 构建或运行失败 | 容器固定依赖；媒体模块可编译期关闭 |
| 拆分引入 silent 语义变化 | 渲染差异 | 拆分前截图/像素基线；拆完对比 |
| 无人评审 | 质量失控 | 单 PR ≤400 行、机检门禁、必要时独立复核 |
| 范围蔓延 | 工期失控 | 重构期冻结新功能；候选 A/B/C 明确停止点 |

---

## 8. 度量基线与目标

| 指标 | 当前（2026-10-26 实测） | 目标 |
|---|---:|---:|
| 干净构建 | 0 error / 10,083 warnings | 0 error / <500 warnings |
| 最大源码文件 | 3,690 行（`GN_Vk.cpp`） | ≤500 有效行（`no-split` 除外） |
| 源文件总数 | 98 | 按拆分增加，单文件变小 |
| 不安全字符串函数 | 147 处 | 修改文件 0 处；全仓清零 |
| 可变全局（extern） | 37 | 0 新增；上下文覆盖 |
| 文件级 static | 134（`GN_Vk.cpp` 40） | 常量除外清零 |
| 单元测试 | 0 → **67 用例**（`make unit`） | ≥60 用例（已达成） |
| Sanitizer | 无 | 单测 ASan/UBSan 通过（`make asan`，当前无报告） |
| 冒烟测试 | 0（手工示例） | 16 个示例全编译；14 个 headless 60 帧 + 02/03 资源夹具（**当前阻塞：无 Vulkan 设备**） |
| CI | 无 | build+test+smoke+ASan 四作业 |
| 平台声明 | 4 个（仅 1 个可用） | 与实现一致 |
| 版本来源 | 2 处不一致 | 1 处生成 |
| 未提交 WIP | 9 文件 / 571 行 | 0（按阶段合入） |

---

## 9. 验收总门与完成定义

**每阶段门**（全过才进下一阶段）：

```bash
make -j16                      # 0 error，告警不超预算
make test                      # 单元测试全过
make check                     # 示例编译 + headless 冒烟
bash <code-modularity>/lintCodeModularity.sh src --naming --comments
nm -D --defined-only libGN_mini.so | sort | diff abi/baseline.txt -   # 只允许新增
```

**整体完成定义（B 候选）**：

1. 上述门在干净克隆与 CI 上通过；
2. 超大文件清零，模块依赖符合 2.1；
3. 无 `strcpy/strcat/sprintf` 新增，资源/上传路径 ASan 干净；
4. 文档与命令真实可用，版本单一来源，许可完整；
5. 未提交 WIP 清零，每阶段有 tag。

**明确的"不完成"信号**：只有"文件拆了"但测试仍缺、符号变了却没记录、文档仍写 4 平台——任一条出现即不算完成。

---

## 10. 三关裁决点（待用户）

| 编号 | 决策 | 选项 | 推荐 | 代价 |
|---|---|---|---|---|
| D1 | 重构粒度 | A 保守 / B 标准 / C 彻底 | **B** | A 2–3 周、B 8–12 周、C 14–20 周（单人） |
| D2 | 公共 API | 保持源码兼容 / 允许破坏式重命名 | **保持兼容** | 破坏式可少写兼容层，但示例与下游要同步改 |
| D3 | 平台范围 | Linux-only 并如实声明 / 保留 4 平台目标 | **Linux-only** | 保留 4 平台需 +4–6 周且无验证环境 |
| D4 | 构建系统 | CMake + make 封装 / 维持手写 Makefile | **CMake 封装** | 迁移 1–2 周；维护者需接受 CMake |

> 裁决后进入 G2 独立红队（六攻击面），再冻结约束与任务图（G3）。

---

## 11. 首两周可执行清单

1. 修 `08_cube3d.cpp` 重复 `h`，重编库，`make tests` 16/16（半天）。
2. 修 `Makefile` 三处：`tests` 失败退出、`clean` 清 `build/test_*`、去掉 freeglut 依赖（半天）。
3. 加 `LICENSE` 与 `external/` 许可清单；版本号收敛到 `GN_Version.h`（1 天）。
4. 建 `tests/` 骨架 + 20 个容器/字符串/数学用例，`make test` 可跑（2–3 天）。
5. 加 `GN_StartRunHeadless`，用 `VK_EXT_headless_surface` 跑通 1 个示例 60 帧（2–4 天，最大不确定项）。
6. CI：build + test + headless smoke 三作业（1–2 天）。
7. 记录告警基线（按类），确定"新代码 0 新增告警"门禁（半天）。

---

## 12. 不做什么

- 不新增渲染特性、不调 UI 视觉、不做性能优化；
- 不实现 macOS/Windows/Android 后端（除非 D3 选择保留并另立项）；
- 不重写自研容器/内存池为 STL，不换语言；
- 不重命名公共 API（除非 D2 选择破坏式）；
- 不追求零告警、不追求测试覆盖率数字；
- 不在本方案内处理"新增功能导致的架构变化"——功能开发在重构窗口外进行。
