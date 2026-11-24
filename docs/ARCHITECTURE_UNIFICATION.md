# 引擎底层架构综合分析 + 统一方案

> 2026-11-24。基于对 164 个 .cpp / 91,802 行的实测扫描，不是印象。
> 关联：`ENTITY_CONTROLLERS_PLAN.md`（上层的控制器规划）、`ENGINE_ARCHITECTURE.md`（原描述）。

## 1. 规模实测

| 模块 | .cpp | 行数 | 占比 |
|------|------|------|------|
| graphics | 106 | 42,346 | 46% |
| core | 23 | 19,657 | 21% |
| ui | 14 | 16,268 | 18% |
| platform | 3 | 2,895 | 3% |
| mmd | 9 | 2,662 | 3% |
| network | 3 | 2,378 | 3% |
| input | 2 | 1,604 | 2% |
| audio | 2 | 1,423 | 2% |
| file | 1 | 1,280 | 1% |
| media | 1 | 1,212 | 1% |
| **合计** | **164** | **91,802** | |

单文件 top：`GN_Array.h` 4,016 行、`GN_UiBase.cpp` 2,665、`GN_UiFBrowser.cpp` 2,157、
`GN_Sys.cpp` 1,721、`GN_2Dmath.cpp` 1,501、`GN_Vk.cpp` 1,402。

**最要紧的一个数字**：`GN.h` 只有 229 行，但预处理展开后 **97,193 行**；
**164 个 .cpp 里有 135 个 include 它**（82%），也就是说绝大多数编译单元都要吃掉
近 10 万行头文件。这不是风格问题，是编译时间和耦合度的硬成本。

## 2. 九处架构断层（附证据）

### 2.1 四套命名前缀并存

| 前缀 | 不同的函数名数 | 来源 |
|------|--------------|------|
| `GN_*` | 444 | 主 API |
| `GT_*` | 164 | 平台/系统层旧式 |
| `GF_*` | 19 | 循环框架回调 |
| `GNg::GN_Vk*` | 89 | Vulkan 后端 |

外加 **10 个命名空间**（修正：正整数按全字母匹配会漏掉 `GN3d`，以下为正确统计）：

| 命名空间 | 声明处数 | 说明 |
|---------|--------|------|
| `GN3d` | **92** | 3D 仿真：物理/动画/骨骼/空间/数学（是真实主战场之一）|
| `GNg` | 67 | 图形基础设施 + Vulkan 后端 |
| `GN` | 21 | 全局输入/容器/少量旧 API |
| `mmd` | 19 | MMD 导入 |
| `GNu` | 11 | UI / 布局 |
| `GNa`(4) / `GNn`(3) / `GNf`(2) / `GNio`(2) / `GNm`(1) / `GNd`(1) | — | 音频/网络/文件/IO/媒体/格式 |

**真正的问题不是"命名空间太少"，而是 `GNg` 与 `GN3d` 的切分线没按"领域"划**：
数学归 `GNg`、仿真归 `GN3d`（详见下表）。而且 `sVEC3` 还落在了 2D 数学头里（§2.2），
三处错位叠加。

**同名类型跨命名空间重复：实测 0 组。**
（前期用 `grep -oE "[A-Za-z_]+"` 统计时，`sVEC2/sVEC3/sVEC4` 因数字被截断而误报成 "sVEC"，
前向声明也被当成重复；用带数字的正则 + 按命名空间归属重跑后，真实重名为 0。）

**真正的问题是 `GNg` 与 `GN3d` 的切分线画在了错位置**：

| 类型 | 归属 | 直觉预期 |
|------|------|---------|
| `sVEC3` / `sVEC4` / `sMAT44` / `sQUAT`（3D 数学） | **`GNg`** | `GN3d` |
| `sMESH` / `sMATERIAL` / `cCAMERA` / `sSKELETON` / `sANIM_CLIP` / `sFRUSTUM`（3D 仿真）| **`GN3d`** | `GN3d` ✓ |

结果是写一个 3D 功能要在两个命名空间间来回跳：`GNg::sMAT44` + `GN3d::sMESH`。
而且 `sVEC3`（3D 向量）定义在 `GN_2Dmath.h`（§2.2），三个错位叠在一起。

代价：不是"编译不过"，而是**每次读代码都要先确认命名空间**；IDE 的 go-to-definition
在 `sIMAGE` 这类同名前向声明多处的类型上经常跳错位置。

### 2.2 2D / 3D 数学的物理位置颠倒（A3 已修）

> **状态：已修复（2026-11-24 A3）**。`sVEC3` 已从 `GN_2Dmath.h:978` 迁到 `GN_3Dmath.h`，
> 改动 +11/−6 行。搬家前先确认了使用量：2D 层实际使用为 0（`GN_Format.h` 里只是注释），
> 且直接 include `GN_2Dmath.h` 的只有 `GN.h` 与 `GN_3Dmath.h`，不产生新越级。

原始问题记录：

- `sVEC2` 在 `GN_2Dmath.h`（合理）
- **`sVEC3` 也在 `GN_2Dmath.h`，第 978 行**（不合理）
- `sVEC4` / `sMAT44` / `sQUAT` 在 `GN_3Dmath.h`

`GN_3Dmath.h` 开头写着"依赖 sVEC2, sVEC3 基础定义"——所以 3D 数学模块依赖 2D 数学模块，
而 2D 数学模块里装着 3D 向量。真实依赖方向是反的。

### 2.3 全局可变状态：151 处 `extern`

最重的单个：

| 全局 | 规模 |
|------|------|
| `GNg::GN_Vk`（`GN_VkContext`） | **265 行 struct，199 个 Vk 句柄字段** |
| `GT_SystemInited` / `GT_UsMemTable` / `Parallel` / `RunRec` / `Sound` / `AmNode` | 各 1 |
| `GN::Key0/Key1/Mouse[]` | 输入全局数组 |

`GN_VkContext` 一个 struct 装下了：设备、交换链、2D 管线、前向、PBR、阴影 CSM、HDR、
Bloom、Composite、Skybox、环境烘焙、G-Buffer、延迟光照、SSAO、SSGI、探针、大气 LUT、
材质、蒙皮、粒子、布料、贴花、降水、云……

**没有任何子系统边界**。想知道"SSAO 需要哪些资源"只能通读 265 行。

### 2.4 生命周期是"函数式 + 手工配对"

- **57 个** `init3D* / create3D* / destroy3D* / recreate3D*` 自由函数
- **89 个** `GN_Vk*` 导出函数

`GN_VkInit3D()` 是一段 130 行的顺序调用清单，正确性完全依赖注释里写明的顺序约束
（"renderPass3D 的 resolve 附件是 HDR 目标，必须先于 init3DTargets 创建 hdrView"）。
没有任何东西在编译期或运行期强制这个顺序。

### 2.5 资源释放三套模型

| 模型 | 例子 | 计数 |
|------|------|------|
| 析构 + `Release(bool immed=false)` | `sIMAGE` | — |
| **两个** Release：`Release()` + `ReleaseGPU()` | `sMESH` / `cMODEL` | 4 处声明 |
| 引用计数智能指针 `P<T>` | 到处 | **487 处** |

同一进程里 `new` 249 次、`delete[]` 139 次、`malloc` 14 次、`std::vector` 173 处、
`sLIST<` 214 处。容器也是两套（`sLIST` 自研 vs `std::vector`）。

### 2.6 日志与错误处理：三层分工（不是五套混乱）

> **修正（实测后）**：本节初稿把这里写成“五套并存”的断层，是错的。
> 实际是三层**合理**分工，只缺一个“双写”入口。详见文末附录第 4 条。

| 层 | 实际行为 | 调用量 | 用途 |
|----|---------|-------|------|
| `GN_Log(...)` | **写 `RunLog.txt` 文件**（`GN_File.cpp:895`，不是 stdout） | 309 | 持久日志，事后排查 |
| `fprintf(stderr, ...)` | 控制台 | 127 | 当场可见的诊断 |
| `bool` 返回 / `VkCheck` | 错误传播 | 157 / 31 | 调用方处理 |

已实测确认：`src/` 里裸 `printf`（不带 f/v/s 前缀）**0 处**；
`headlessCheck` / `gpuCheck` 的 **stdout 只有 `[PASS]` / `[FAIL]` / `DEVICE:` 等机器可读输出**，
`[GN]` / `[GN_Vk]` 诊断全在 stderr。**CLI 输出污染问题不存在。**

真正缺的只有一个：**“既要持久化（RunLog）又要当场可见（stderr）”时没有现成 API**。
要这么做只能在两处各写一行。影响有限，但值得补一个入口（见 §3.5 修正）。

### 2.7 测试直接耦合 Vulkan 内部

| 指标 | 数量 |
|------|------|
| tests 里 `GNg::GN_Vk*` 调用 | **189** |
| tests 里直接 `vkCmd*` | 36 |
| tests 里 `vkCreate*` | 24 |
| tests 直读 `GN_Vk.swapchainExtent` | 67 |
| tests 直读 `GN_Vk.device` | 60 |
| tests 直读 `GN_Vk.aPipelineCache` | 10 |

后果：任何底层重组都会同时打断 17 个 gpuCheck 文件。这就是为什么"架构调整"在这套代码里
显得很贵——**测试是耦合放大器**。

### 2.8 每帧编排没有归属

- **17 个文件**直接调 `GN_VkBegin3DPass`（其中 `gpuCheck3D.cpp` 一个文件 17 次）
- CSM 拟合在 **4 处**各自实现：`mmdViewer.cpp`(8 处引用)、`imoSceneShadow.cpp`、
  `GN_3Dlight.cpp`、`testLight.cpp`

渲染顺序（近远平面 → CSM → 4 级 shadow pass → 3D pass → skybox → 云 → 地面 → 模型 →
贴花 → 降水 → gizmo → 2D 叠层）只存在于 mmdViewer 的 66 行里，没有任何抽象。

### 2.9 文档与代码有漂移风险

`docs/` 有 **34 份 md**，多份自称"契约（冻结）"；而 `src/` 里引用 `docs/` 的注释只有 **26 行**。
文档靠人工维持同步——这轮工作里我就发现过两处（`tests/gpu/README.md` 写"99 条"实为 109、
`vk3dForward.cpp` 注释说"每个 3D pass 开始时同步"实为帧首+代际号）。

## 3. 目标架构

### 3.1 分层与依赖规则（唯一硬规则）

```
L6 框架     UI / 输入 / 音频 / 网络 / 媒体
L5 渲染     渲染编排（pass 序列、cRENDER_FRAME） + 后端（Vulkan）
L4 仿真     物理 / 动画 / 骨骼 / 布料 / 粒子 / 空间划分
L3 资源     图像 / 纹理 / 着色器 / 管线 / 材质
L2 数学     向量 / 矩阵 / 四元数 / 颜色
L1 基础设施 容器 / 字符串 / 日志与错误 / 时间 / 并行 / 文件
L0 平台     系统调用 / 窗口 / GLFW / 线程原语
```

**规则：只能向下依赖；同层之间不互相依赖。**
`ui` 不许 include `graphics/_Vulkan`；`graphics` 不许 include `ui`；`core` 不许 include 任何上层。

**可自动化检查**（这是让规则活下去的关键）：

```bash
# tests/lint/layers.sh —— 用 include 路径判层，越级即失败
# 现状基线：graphics 有 45 处 include GN_Vk.h（同层，合法）
#           但 core 有 1 处、platform 有 1 处（越级，需修）
```

### 3.2 命名规则（新代码强制，老代码按需迁移）

| 类别 | 规则 | 例 |
|------|------|-----|
| 值类型 | `s` + 大驼峰 | `sVEC3`、`sMESH` |
| 有身份对象 | `c` + 大驼峰 | `cCAMERA`、`cPHYSICS_WORLD` |
| 枚举与枚举值 | `e` + 大驼峰 / `e` + 大写 | `eLIGHT_TYPE`、`eLIGHT_POINT` |
| 智能指针 | `P<T>` | 保持 |
| 成员 | `a` + 大驼峰 | `aJoints`（现有 3D 层已统一） |
| 自由函数 | **单一**`GN_`前缀；模块内 `static` 不加前缀 | `GN_VkInit` |
| 命名空间 | 见 §3.3 |

**明确废除**：新增 `GT_*` / `GF_*` / `GNg_*` 前缀。已存在的保留（改动面太大），
但**不再新增**，并在 §5 的清理阶段逐步收敛。

### 3.3 命名空间收敛表

| 命名空间 | 归属 | 处理 |
|---------|------|------|
| `GN` | 兼容 C API（`GN::Key0` 等全局输入） | 保留，冻结 |
| `GNg` | 图形基础设施（含 Vulkan 后端） | 保留，**主战场** |
| `GN3d` | 3D 仿真（物理/动画/骨骼/空间）——目前是空壳 | **充实**，把 3D 类型从 `GNg` 迁进来 |
| `GNu` | UI / 布局 | 保留 |
| `GNf` / `GNio` / `GNa` / `GNn` / `GNd` | 文件/IO/音频/网络/格式 | 保留 |
| `GNm` / `mmd` | 媒体 / MMD 导入 | 保留 |

同名类型（§2.1）实测为 0 组，无需消歧；真正要动的是 **`GNg` / `GN3d` 的职责线**：
把 3D 数学（`sVEC3/sVEC4/sMAT44/sQUAT`）一并归入 `GN3d`，让 "写 3D 就在 `GN3d` 里"
成立。迁移涉及 308 处 `GN3d::` 与大量 `GNg::sMAT44` 引用，用 `namespace GN3d { using GNg::sMAT44; }`
别名过渡。

### 3.4 生命周期规则（A4 已实施，方案已修正）

> **修正**：原计划引入 `iGPU_RES` 虚函数基类，**没有采用**。
> `sIMAGE` 到处按值拷贝（有拷贝构造/赋值、装在 `P<T>` 里），加 vptr 会让每个纹理多 8 字节、
> 破坏 POD 语义。为一个命名问题引入虚函数基类，代价远大于收益。

实际采用的方案：

**规则 1：CPU 侧资源一律 RAII**（`sMESH::Release()` 释放顶点/索引/LOD）。
**规则 2：GPU 侧资源用 `ReleaseGPU()` 这个统一名字**，且**入口自查设备存活**：

```cpp
// GN_Vk.h
bool GN_VkDeviceAlive();   // initialized && device != VK_NULL_HANDLE

// sIMAGE：ReleaseGPU() 为规范名，Release() 是等价别名（56 处现有调用不必改）
void sIMAGE::ReleaseGPU() {
    if(!GN_VkDeviceAlive()) { /* 只清句柄，绝不调 vkDestroy* */ }
    ...
}

// sMESH：Release()=CPU、ReleaseGPU()=GPU（原本就对）
```

修掉的真 bug：`sIMAGE::Release()` 原本**不判设备存活**，而它被析构函数无条件调用。
对象比设备活得长时（静态/全局对象、`AppUnInit` 较晚释放的模型）就是拿销毁后的 device
调 `vkDestroyImageView`。已由 `tests/unit/testResourceLifecycle.cpp` 与门禁自测证实：
注入旧行为后立即 `vkDestroyImageView: Invalid device` + core dump。

### 3.5 日志规则（修正后）

```
规则：库内禁止裸 printf 到 stdout（实测已为 0 处，靠 lint 保持）。
      ● 只需持久化          → GN_Log(...)        → RunLog.txt
      ● 只需当场可见        → fprintf(stderr,…)   → 控制台
      ● 两者都要（代码路径失败、设备初始化失败这类“必须知道”的）→ GN_LogErr(...)  ← 新增
      ● Vulkan 调用         → VkCheck（保留，内部走 stderr）
```

`GN_LogErr` 是本轮唯一新增的错误处理 API：同时写 RunLog 与 stderr。
它解决的是“只写 RunLog 的话用户当场看不到”这个真实缺口，而不是“统一格式”这种表面收益。

### 3.6 全局状态拆分（A5 已完成）

> **已完成（2026-11-24 A5）**：265 行 / 200 字段的 `GN_VkContext` 已按子系统切成 8 个
> 匿名 struct 组，**零调用点改动、sizeof 仍为 2008 字节**。下面是最终方案，与初稿不同。

**初稿方案（未采用）**：把 context 传进每个函数 + 访问器 `Device().device`。
否决理由：那要改 3155 处调用，风险与收益完全不成比例。

**实际方案：匿名 struct**

```cpp
struct GN_VkContext {
    GN3d::cPIPELINE_CACHE aPipelineCache;   // 非平凡成员，不能进匿名 struct

    // ── sDeviceRes：设备与队列
    struct {
        VkInstance instance;
        VkDevice device;
        ...
    };
    // ── sSwapchainRes / sFrameRes / s2DRes / sForwardRes …
};
```

`GN_Vk.device` 照旧能写，但物理上已按子系统切开。

**前提（已逐项实测为 0）**：`offsetof` 0 处、整体 `memcpy` 0 处、
`sizeof(GN_VkContext)` 0 处、按值传参 0 处 → 重排字段顺序安全。

**两条硬约束**：
1. 有构造/析构的非平凡类型不能放匿名 struct（`cPIPELINE_CACHE` 只能留在顶层）。
2. 匿名 struct 是 GNU/MS 扩展（ISO C++ 不允许），本项目不用 `-Wpedantic`。

分组结果与守卫脚本见 `tests/lint/context_groups.sh`（已并入 `make lint`）。

当初的目标——「“SSAO 需要哪些 GPU 资源”从 265 行里一眼可见」——已达成：
现在看 `sDeferredRes` 那 67 行就够。

### 3.7 测试解耦

引入**测试钩子**而不是让测试直接摸内部：

```cpp
namespace GNg {
struct sTEST_HOOKS {
    VkDevice device; VkPhysicalDevice physicalDevice; VkQueue queue;
    VkCommandPool commandPool; VkDescriptorPool descriptorPool;
    VkExtent2D extent; VkFormat format;
    uint32_t (*pipelineCacheCount)();
    bool (*readSwapchain)(int x, int y, int w, int h, std::vector<uint8_t> &out);
};
const sTEST_HOOKS *GetTestHooks();   // 非测试构建返回 nullptr
}
```

`tests/gpu` 从"直读 `GN_Vk.swapchainExtent` 67 次"变成"读 `hooks->extent`"。
底层重组时只需维持 hooks 不变。

**这一步是解耦的前提**——不然 §3.6 的分组一改，17 个测试文件全红。

## 4. 统一决策表（哪些做、哪些不做）

| # | 断层 | 决定 | 理由 |
|---|------|------|------|
| 1 | 四套前缀 | **冻结旧、规范新** | 全量重命名 600+ 符号，收益不抵风险 |
| 2 | 2D/3D 数学颠倒 | **已做（A3）**：`sVEC3` 迁入 `GN_3Dmath.h` | +11/−6 行；2D 层实际使用为 0，搬家零风险 |
| 3 | 全局状态 265 行 | **已做（A5）**：8 个匿名 struct 组，零调用点改动 | `sizeof` 仍 2008；前提检查（offsetof/memcpy/sizeof/按值传参）全为 0 |
| 4 | 生命周期函数式 | **不做全量重构**；新增统一 `ReleaseGPU()` 契约 | 57 个 init 函数重写=重写 graphics |
| 5 | 三套释放模型 | **已做（A4）**：`ReleaseGPU()` 统一 + `GN_VkDeviceAlive()` 守护 | 推翻原 `iGPU_RES` 基类方案（`sIMAGE` 按值拷贝，vptr 代价大）；修掉设备销毁后释放的 UB |
| 6 | 五套错误处理 | **做**：库内禁 printf，统一 `GN_Logf` | 改动机械、可脚本化、立刻见效 |
| 7 | 测试耦合内部 | **做**：`sTEST_HOOKS` | §3.6 的前置条件 |
| 8 | 编排无归属 | **已做（A6）**：CSM 策略 + shadow 循环 + RAII pass 守卫全上台 | 完整 `cRENDER_FRAME` 不做（drawable 差异太大），但 pass 两端配对用 RAII 解决了 |
| 9 | 文档漂移 | **已做（A7）**：关键数字改为实测生成，`make doc-check` 校验 | 已修 3 处历史漂移（99→109、463→491、3D pass 同步描述）|

## 5. 分阶段实施

排序原则：**先解耦（让后面所有改动变便宜）→ 再重组（收益大但风险高）→ 最后清理**。

### A1 — 测试钩子 + 分层 lint（3–4 天）

- `GNg::GetTestHooks()`；17 个 gpuCheck 文件改走 hooks（189 处 `GN_Vk*` 调用逐步替换）。
- `tests/lint/layers.sh`：按 include 判层，输出越级清单；先记为"基线"，不阻断。
- **验收**：`make ci` 全绿；`GN_Vk.` 在 tests 里的直接出现从 ~200 降到 <20。

### A2 — 错误处理统一（2 天）

- 新增 `GN_Logf`（`GN_Log` 的 printf 变体，写 stderr + RunLog）。
- 库内（`src/`）的 `fprintf(stderr` 123 处 + 库内 `printf` 收敛到 `GN_Logf`。
- 注意区分：demo 的进度输出保留 `printf`（用户可见）。
- **验收**：`grep -rc "fprintf(stderr" src/` ≈ 0；`make ci` 全绿；产出内容不变。

### A3 — 数学层归位（**已完成**，2026-11-24）

`sVEC3` 从 `GN_2Dmath.h` 迁到 `GN_3Dmath.h`（+11/−6 行）；同步修正三处注释。
搬家前先确认：2D 层实际使用 0 处，直接 include `GN_2Dmath.h` 的只有 `GN.h` 与 `GN_3Dmath.h`。

**教训**：该仓库换行符是混合的（部分文件 CRLF、部分 LF）。用 Python 文本模式读写会把
CRLF 静默转成 LF，导致整文件变“已修改”。必须用 `open(..., 'rb')` / `'wb'` 二进制模式，
改完逐一比对换行符与 HEAD 是否一致。

### A4 — GPU 资源释放契约（**已完成**，2026-11-24）

- 新增 `GN_VkDeviceAlive()`；`sIMAGE::ReleaseGPU()` 成为规范名（`Release()` 为别名），入口先判设备存活。
- 修掉真 bug：析构函数无条件调 `Release()`，设备销毁后即 UB（已由门禁自测证实可触发）。
- 新增 `tests/unit/testResourceLifecycle.cpp` 6 用例。
- **推翻原方案**：不做 `iGPU_RES` 虚函数基类（`sIMAGE` 按值拷贝，vptr 代价大于收益）。
- **验收**：`make unit` 483/0；门禁自测注入旧行为后立即 `Invalid device` + core dump。

### A5 — `GN_VkContext` 分组（**已完成**，2026-11-24）

8 个匿名 struct 组、200 字段全覆盖、**零调用点改动**、`sizeof` 仍 2008。
非平凡成员 `cPIPELINE_CACHE` 单独留顶层（匿名 struct 不允许有构造/析构的成员）。
新增 `tests/lint/context_groups.sh` 守卫（已并入 `make lint`），
门禁自测：往顶层插一个散字段立即报错并点名。

**过程中踩到的坑**：首版脚本的单条正则静默丢了 10 个指针字段；
现改为「行尾抽名字列表」解析 + 两道硬校验（无法解析即退出、字段数低于原文 95% 即退出）。

### A6 — CSM 拟合策略 + 3D pass 守卫（**已完成**，2026-11-24）

分两轮完成。第一轮抽 CSM 策略（`computeShadowFar` / `fitShadowCascades`），
第二轮补上真正需要抽象的那部分。

**第一轮**：两个 demo 的 farZ 计算有 22 行逐字相同；抽出 `computeShadowFar()` +
`fitShadowCascades()`（五步钳在一起：外扩/拍地 → farZ → 级联数钳制 → 拟合 → 写 sLIGHT_UBO）。
同时把 `vkShadowCascade` 从 `_Vulkan/` 移到 `graphics/`（纯 CPU，原先迫使 demo 越级 include L3 头）。

**第二轮**（上轮我误判为“不值得”）：`Begin3DPass`/`End3DPass` 背后是个状态机，
漏 End 会让 `s_in3DPass` 永久为真（之后所有 Begin 静默失效）+ 不调 `GN_VkCompositeToSwapchain`
（HDR 从不合成 ⇒ 纯黑）。两个都是静默黑屏。
所以做了：
- `sFRAME_3D_SCOPE`（RAII，保证 End3DPass + Begin2DPass）
- `GN_VkIsIn3DPass()`（Begin 是 void 且静默失败，守卫必须能回查）
- `GN_VkDrawShadowCascades()`（消除两边逐字相同的 for 循环）

**完整 `cRENDER_FRAME` 仍不做**：drawable 列表差异太大，抽出来只有空壳。
但“pass 两端配对”这个共性是真实的，已用 RAII 解决。

### A7 — 文档数字自动化（**已完成**，2026-11-24）

`tests/tools/gen_stats.sh` + `make stats`（生成）/ `make doc-check`（校验）。
文档里用 `<!-- AUTO-STATS:BEGIN/END -->` 标记区，内容由实测生成：
单测用例数（静态统计 `TEST_CASE`）、gpuCheck 实跑结果（必须跑）、src 规模、着色器数。

已接入 `README.md` / `tests/README.md` / `tests/gpu/README.md`，并顺手修掉
`tests/README.md:44` 那处过时的「463 单测」。

门禁自测：篡改数字、新增一个用例（不改文档）都能让 `make doc-check` 报红并点名文件。

**局限**：只有放进标记区的数字受保护。散文里随口写的数字（如“第 3 级”）仍靠人。
这是有意的——把所有数字都自动生成会把文档写得很机械。

总工期 **约 17–21 天**，且 A1–A3（约 6–7 天）就能拿到大部分"读起来顺"的收益。

## 6. 明确不做

- **不重写 graphics**（42,346 行）。只做分组与统一契约，不动算法与 pass 实现。
- **不全量重命名** 600+ 旧前缀符号。新代码守规则即可。
- **不引入 ECS / 反射 / 完整资源系统**。`iGPU_RES` 只解决三套模型的歧义。
- **不做"完全去全局化"**。保持 `GNg::Device()` 这类访问器，不把 context 参数传遍全栈。
- **不动 UI 模块**（16,268 行）。它与 graphics 的耦合是"调 `ImageDraw`"（85 处），
  属于 L6→L3 的合法向下依赖，无需拆分。

## 7. 风险

| 风险 | 对策 |
|------|------|
| A5 搬迁期间 `GN_Vk` 指向不明（部分字段搬了、部分没搬） | 严格一组一搬，搬完立刻删兼容宏；每组结束打快照 |
| 测试在搬迁期间大面积变红 | A1 先行（hooks），让测试只依赖稳定面 |
| 分层 lint 一开始就几百条违规，被无视 | 先只输出基线不阻断，A5 后逐步收紧到阻断 |
| 改动影响已验证画面 | 每阶段用 `headlessCheck` + `cornellBox` 逐像素对比兜底 |
| 文档又漂移 | A7 把关键数字改成生成；契约文档加"对应代码文件+行号"便于核对 |

## 8. 一句话结论

这套代码**算法层很扎实**（17 个模块有单测、gpuCheck 109 项、101 个管线断言），
问题全在**组织层**：命名四套、全局状态一个 265 行的 god struct、生命周期靠注释维序、
测试直接摸内部。

统一的收益不在"代码变短"，而在**"改一处不再需要读 265 行才知道有没有漏"**。
按 §5 的顺序，前 7 天（A1–A3）就能把这个门槛降一半，而风险几乎为零。

## 附录：本文的测量误差与修正

写这份分析时我自己犯了五处错误（四处统计 + 一处判断），均已修正并保留记录：

| 错误结论 | 原因 | 修正 |
|---------|------|------|
| "`GN3d` 几乎是空壳，只有 2 处声明" | 正则 `[A-Za-z_]+` 漏了数字，`namespace GN3d` 被计入 `GN` | `GN3d` 实为 **92 处**，与 `GNg`(67) 并列主战场 |
| "同名类型跨命名空间重复 10 组" | 同上（`sVEC2/3/4` 被截成 `sVEC`）+ 前向声明被当成重复 | 真实重名 **0 组** |
| "`src/` 内裸 `printf` 128 处，污染 CLI 输出" | 把 `fprintf(` 里的 `printf(` 子串数成了裸 printf | 裸 `printf` **0 处**；`fprintf(stderr)` **123 处**（正确用法） |
| "`GN_Log` 写 stdout" | **没看实现就假设** | 实为写 `RunLog.txt`（`GN_File.cpp:895`）；`GN_Base.cpp` 里的 vprintf 版被 `#ifndef _GN_FILE_H_` 屏蔽 |
| **A2 整节基于错误前提**（“五套错误处理”） | 上面两条叠加 | 实际是三层合理分工；A2 从 2 天缩水到 2 小时 |

教训两条：
1. **统计类结论必须用能处理完整标识符的正则重跑一次**（`printf` 的坑就在前缀 `f`）；
2. **判断“某 API 行为如何”必须看实现，不能从名字推**——`GN_Log` 听起来像写控制台，
   实际写文件；如果我开工前先去看一眼实现，A2 这 2 天就不会写进计划。

数字写进文档就会被当成事实引用，错了比没写更糟。

这套代码**算法层很扎实**（17 个模块有单测、gpuCheck 109 项、101 个管线断言），
问题全在**组织层**：命名四套、全局状态一个 265 行的 god struct、生命周期靠注释维序、
测试直接摸内部。

统一的收益不在"代码变短"，而在**"改一处不再需要读 265 行才知道有没有漏"**。
按 §5 的顺序，前 7 天（A1–A3）就能把这个门槛降一半，而风险几乎为零。
