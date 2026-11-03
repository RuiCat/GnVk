# Cornell Box 独立测试程序规划（自由相机 + 全局光照）

> 性质：**规划文档**，本轮不写源码。基线 `HEAD = aaf73ef`；本机 `make unit` = 396 cases / 0 failed。
> 关联：[GI_RT_TYNDALL_PLAN.md](GI_RT_TYNDALL_PLAN.md) §3.7（验证基准）/§5（路线图）、
> [GI_PLAN.md](GI_PLAN.md)（G-Buffer/SSAO/SSGI/探针契约）、[CAMERA_PLAN.md](CAMERA_PLAN.md)（cCAMERA C0–C5）、
> [AXIS_CONVENTIONS.md](AXIS_CONVENTIONS.md)（坐标约定）。
> 本机无 GPU：程序可编译，但运行/截图/误差验收**只能在桌面机执行**，文中标「未验证」。

---

## 0. 结论先行

1. **形态**：新建独立可执行 `tests/cornellBox/`，构建目标 `make cornellBox`，链接 `libGN_mini.so`。
   **「独立」= 独立程序 / 独立场景 / 独立相机 / 独立构建目标，不依赖 mmdViewer 的模型与资产**；
   仍复用引擎的 Vulkan 后端与 GI API（否则测不到引擎 GI）。
2. **三个目标**（都从同一份纯 CPU 几何/位姿出发）：
   - `cornellBox`：引擎实时路径（**延迟路径**，这样 G-Buffer/SSAO/SSGI/探针才可见），自由相机 + GI 开关 + HUD；
   - `cornellPt`：CPU 路径追踪参考（真面光/方向光、余弦采样、NEE、ACES→PNG），不需要 GPU/窗口；
   - `cornellCompare`：两张 PNG 的 RMSE/MAE/最大误差/逐通道偏差 + 误差图。
   共享相机位姿（`CORNELL_POSES`）、几何/材质（`cornellGeometry`），才能做同机位对比。
3. **为什么用延迟路径**：当前唯一应用走前向（`mmdViewer.cpp:479`），SSAO/SSGI/探针只挂延迟，
   且默认关（`gpuCheck3D.cpp:312-316`）。Cornell 程序是第一个**真正消费延迟 GI** 的应用，正好补上「做了看不到」。
4. **场景**：经典 Cornell box（5 面墙 + 1 个漫反射球 + 1 个金属球 + 顶部面光），全部程序内生成，无外部资产；
   PT 的面光放在天花板 1×1 开口上方；引擎 `point` 模式用点光近似面光，`sun` 模式用方向光。
5. **已实现范围（C0/C1/C2）**：几何 + 自由相机 + 直接光 + SSAO/SSGI 开关 + HUD；CPU PT + 对比工具。
   **不**做云、大气、Tyndall、玻璃、纹理；C3 接 DDGI；C4（可选）在盒内加雾测丁达尔光柱。
6. **验收核心不是「像」而是可判定**：canonical 视角可复现、PT 同种子稳定、engine vs PT 的误差指标、
   性能基线。所有截图/参考帧落 `.local/cornellbox/`（过程产物，不入库）。

---

## 1. 定位、假设与边界

### 1.1 「独立」的含义（假设）

- **是**：独立可执行、独立 `main`、独立场景/相机/HUD/截图；不 `#include` mmdViewer 的源文件；不读 MMD/glTF 资产。
- **不是**：脱离引擎的裸 Vulkan 程序。裸 Vulkan 需要重写 device/swapchain/G-Buffer/光照，成本高且**测的是另一套渲染器**，
  与「验证 GnVk 的 GI」目标相反。
- **推翻条件**：若目标是「脱离引擎的参考渲染器」，则应改为独立 CPU/GPU 路径追踪器（不链接 `libGN_mini.so`），
  本文的 C2 可直接演进为那个程序，但 C0/C1 的引擎验证部分不再适用。

### 1.2 目标

| # | 目标 | 判据 |
|---|---|---|
| G1 | 可复现的 Cornell box 场景 | 同一相机位姿、同一参数，两次运行截图逐像素一致（引擎确定性部分） |
| G2 | 自由相机 | 键鼠自由飞行 + 轨道 + 4 个 canonical 位姿；位姿可保存/回放 |
| G3 | 引擎 GI 可见 | SSAO/SSGI/探针开关在画面上有可判定差异，不依赖 gpuCheck |
| G4 | 真实 GI 参考 | CPU 路径追踪输出可作为 ground truth；误差可量化 |
| G5 | 可自动化 | PT/对比可脚本化（无窗口）；引擎 headless/F12 截图未实现，桌面截图人工执行 |

### 1.3 非目标（防范围蔓延）

- 不做材质编辑器、模型导入、动画、粒子、后处理大而全。
- 不做玻璃/次表面/体积云（C4 只做盒内均匀雾，验证丁达尔）。
- 不做多场景管理；Cornell box + 一个 sun 变体足够。
- 不改引擎公开 API（除 DDGI 阶段确实需要的新入口，另行冻结）。

---

## 2. 场景规格

### 2.1 几何与材质（程序内生成）

坐标/朝向按 [AXIS_CONVENTIONS.md](AXIS_CONVENTIONS.md)；本文不另立约定。建议房间 5.5 × 5.5 × 5.5 m，
前面开口，相机在开口外看向盒内。

| 物体 | 几何 | 材质（albedo / roughness / metallic） | 说明 |
|---|---|---|---|
| 地板 | 1 quad | 0.73 / 1.0 / 0 | 白 |
| 天花板 | 1 quad | 0.73 / 1.0 / 0 | 白 |
| 后墙 | 1 quad | 0.73 / 1.0 / 0 | 白 |
| 左墙 | 1 quad | 0.65,0.05,0.05 / 1.0 / 0 | 红 |
| 右墙 | 1 quad | 0.12,0.45,0.12 / 1.0 / 0 | 绿 |
| 漫反射球 | UV sphere 32×16 | 0.73 / 1.0 / 0 | 半径 0.55，偏左后 |
| 金属球 | UV sphere 32×16 | 0.95 / 0.08 / 1.0 | 半径 0.45，偏右前 |
| 面光 | 1 quad（仅 PT 参与发光） | L = 15–20（线性） | 1×1 m，贴天花板中心；引擎无面光/自发光材质，实时用点光近似（见 §2.2） |

- **无纹理、无 alpha、无法线贴图**：去掉所有与 GI 无关的变量；顶点格式用引擎 `VkVertex3D`（位置/法线/UV），UV 可置 0。
- 三角形量级：5 墙 + 2 球 ≈ 5×2 + 2×(32×16×2) ≈ 2k 三角形，CPU 暴力路径追踪完全够用。
- 生成函数与 PT 共享同一份 `sCORNELL_TRIANGLE`（位置/法线/albedo/材质 id），避免两套几何漂移。

### 2.2 两种光照变体

| 变体 | 光源 | 用途 | 注意 |
|---|---|---|---|
| `--scene=classic` | 顶部 1×1 面光，L=15–20 | 经典 Cornell box；PT 真面光 | 引擎没有面光，实时用「点光/聚光 + 自发光 quad」近似，**直接光不完全一致**，误差里要标注 |
| `--scene=sun` | 开口方向光（+ 弱环境） | 引擎直接光可精确匹配；只差 GI | 推荐作为 C1/C2 的**默认对照**，避免面光近似污染结论 |

- 引擎侧 `cLIGHT` 最多 8 光源（`GN_3Dlight.h:22`）；classic 用一个点光放在面光中心作为近似，
  可选画一个白色 quad 作位置提示（引擎无自发光材质，它不参与光照）。
- 环境：**背景与 IBL 都置黑**（不重烘环境；必要时用黑 cubemap 或跳过天空绘制），关闭云/大气/天气，
  确保「间接光只来自 Cornell box 内部」。
- PT 的太阳/面光颜色用中性白（1,1,1），避免色温变量。

### 2.3 相机

- 用 `cCAMERA`（`src/graphics/GN_3Dcamera.h`）：支持 FREE / ORBIT / FOLLOW，`sCAMERA_INPUT` 解耦输入，
  `BuildViewProj` 出矩阵（`GN_3Dcamera.h:59-125`）。
- 自由视角：WASD 前后左右、Q/E 升降、鼠标看、Shift 加速、滚轮 FOV/前后；`O` 切轨道绕盒心。
- Canonical 位姿（代码常量，保证可复现）：
  1. 正面（开口外，略高于中心，看后墙）
  2. 左前 3/4（看红墙 + 两球）
  3. 右前 3/4（看绿墙 + 金属球）
  4. 顶视/俯视（看面光与地板）
- 位姿结构 `{position, yaw, pitch, fovY}`；截图旁写 `.txt` sidecar（位姿 + 模式 + GI 开关 + 曝光），供复算。
- FOV 45°，near 0.05，far 100；aspect 1280×800（可命令行覆盖）。

---

## 3. 程序结构

### 3.1 文件划分（实际落地；单功能 <500 有效行）

| 文件 | 职责 | 实际物理行 |
|---|---|---|
| `tests/cornellBox/main.cpp` | 窗口/回调接线、延迟 pass 顺序、GI 快捷键 | 119 |
| `tests/cornellBox/cornellShared.h` | 跨模块声明（场景/相机/HUD/网格类型） | 57 |
| `tests/cornellBox/cornellGeometry.h/.cpp` | **纯 CPU 唯一几何源**：四边形/球/材质/4 位姿/太阳方向 | 77 + 162 |
| `tests/cornellBox/cornellScene.cpp` | 几何三角化 → G-Buffer 网格；材质/光源；平面法线贴图 | 199 |
| `tests/cornellBox/cornellCamera.cpp` | 包装 `cCAMERA`：输入、canonical 位姿、自由/轨道 | 124 |
| `tests/cornellBox/cornellHud.cpp` | 左上角叠层：位姿/光照/GI 开关/快捷键 | 60 |
| `tests/cornellBox/cornellPathTracer.h/.cpp` | CPU PT：解析求交、NEE、余弦采样、RR、多线程、ACES+PNG | 33 + 389 |
| `tests/cornellBox/cornellPtMain.cpp` | PT 入口/CLI（spp/尺寸/位姿/area|sun/--metal/输出） | 117 |
| `tests/cornellBox/cornellCompare.cpp` | 两张 PNG 的 RMSE/MAE/最大误差/逐通道偏差 + 误差图 | 191 |
| `tests/cornellBox/cornellCapture.h/.cpp` | F12：swapchain 读回 → BGRA 交换 → PNG（不含 HUD） | 17 + 63 |
| `tests/cornellBox/cornellCpuRt.h/.cpp` | CPU 软光追预览：解析求交 + 3 bounce 路径追踪（8 spp） | 16 + 291 |
| `tests/cornellBox/cornellCpuRtProbe.cpp` | CPU RT 自检：均值/nonBlack/PNG | 79 |
| `tests/cornellBox/cornellGpuRt.h/.cpp` | GPU compute 光追预览：注入式 Vulkan 上下文、场景 SSBO、dispatch/回读、逐帧累积 | 43 + 498 |
| `tests/cornellBox/cornellGpuRtProbe.cpp` | GPU 自检：与 CPU 同 seed 对比（MAE）+ 累积收敛 + 640×400 耗时 | 277 |
| `tests/cornellBox/cornellPng.h` | RGBA8→RGB PNG（CPU/GPU 探针共用） | 58 |
| `assets/shaders/gn_cornell_rt.comp` | GPU 路径追踪着色器（与 cornellCpuRt.cpp 同一套公式） | 263 |

> 命名遵循 `code-modularity`：文件名/函数 `camelCase`（`GN_*` 契约名、`main` 除外），类型 `PascalCase`；
> 新增文件不得进 `LEGACY_NAMING`。构建后跑 lint（实测 20 文件 0 违规）。

### 3.2 构建

```make
# 实际规则（Makefile:269-283）：引擎程序 + GPU RT 着色器依赖
CORNELL_SRC   := tests/cornellBox/main.cpp ... tests/cornellBox/cornellGpuRt.cpp
CORNELL_SHADERS := assets/shaders/gn_cornell_rt.comp.spv
cornellBox: $(TARGET) $(CORNELL_SHADERS)
	$(CXX) $(CORNELL_FLAGS) $(CORNELL_SRC) -L. -l$(LIB_STEM) $(LIBS) -o build/cornellBox

assets/shaders/gn_cornell_rt.comp.spv: assets/shaders/gn_cornell_rt.comp
	glslangValidator -V $< -o $@
```

- `make cornellBox` 编译引擎程序并确保 GPU RT 着色器已编译；`LD_LIBRARY_PATH=. ./build/cornellBox` 运行。
- `make cornellRtProbe`（CPU 自检）/ `make cornellGpuRtProbe`（GPU 自检，无设备 SKIP）为纯 CPU/无窗口目标。
- `make cornellPt` 编译纯 CPU 参考；`./build/cornellPt --spp=64 --out=...` 运行（不需要 GPU/窗口）。
- `make cornellCompare` 编译误差对比；`./build/cornellCompare --ref=... --test=...` 运行。
- 不把 Cornell 场景塞进 `tests/gpu`（那是验收程序）；也不塞进 mmdViewer（资产耦合）。

### 3.3 命令行（实际实现）

引擎程序 `cornellBox` 没有命令行参数（交互式，快捷键见 §4.2）。PT/对比程序参数：

| 参数 | 含义 | 默认 |
|---|---|---|
| `--spp=N` | PT 每像素样本 | 64 |
| `--width=W --height=H` | 输出分辨率 | 640×400 |
| `--pose=0..3` | canonical 位姿（与引擎共用 `CORNELL_POSES`） | 0 |
| `--scene=area\|sun` | 面光 / 方向光 | `area` |
| `--metal` | 金属球用完美镜面（默认漫反射近似，避免焦散 firefly） | 关 |
| `--out=FILE` | PT 输出 PNG | `cornell-pt.png` |
| `--ref=FILE --test=FILE [--out=FILE]` | `cornellCompare` 的参考/测试/误差图 | 无 |

> 原计划的 `--headless` / `--frames` / `--assert` 未实现；PT 本身就是无窗口 CPU 程序，可直接脚本化。
> 引擎截图暂用系统截图（隐藏 HUD），F12 读回未实现。

---

## 4. 引擎模式（实时）

### 4.1 Pass 顺序（复用现有 API）

```text
GN_VkBegin3DPassDeferred(viewProj, camera, lightUBO)
  GN_VkDraw3D_GBuffer(...) × N        // 墙/球/面光
  GN_VkSSAOPass()                     // 需要 G-Buffer；indirect.w=1 才跑
  GN_VkSSGIPass()                     // envParams.w>0 才跑
  GN_VkUpdateProbes()                 // 探针 weight>0 才更新
GN_VkDeferredLighting(viewProj, camera, lightUBO)  // 内部自动 composite
```

- 证据：`vk3dPasses.cpp:313-406`（G-Buffer/SSAO/SSGI）、`:489-562`（延迟 + 自动合成）、
  `vk3dPasses.cpp:380-391`（SSAO 默认返回）、`vk3dPasses.cpp:395-406`（SSGI 默认返回）。
- Cornell 程序在 `AppInit` 显式开 `GN_VkSetSsaoEnabled(true)` / `GN_VkSetSsgiStrength(1.0)` /
  `GN_VkEnableGI(true)`；引擎默认关，不显式开就会是「只有直接光」。

### 4.2 GI 开关与 HUD

| 键 | 功能 | 备注 |
|---|---|---|
| `O` | SSAO（`GN_VkSetSsaoEnabled`） | 默认开 |
| `P` | SSGI 强度（`GN_VkSetSsgiStrength` 0/0.5/1 循环） | 默认 1.0；命中点含直接光 |
| `I` | IBL 开关（`GN_VkEnableGI`） | 默认开 |
| `K` | 光照变体（sun / point） | `L` 留给相机右转，不再复用 |
| `T` | CPU 软件光追预览 | 真 ray-scene；路径追踪 8 spp / 3 bounce + 镜面；`sun` 无阴影、`point` 有阴影射线；320×200 约 30 ms |
| `Y` | GPU compute 光追预览 | 同一套 ray-scene 搬到 compute；640×400 每帧 16 spp（约 10 ms），相机静止时逐帧累积收敛、移动时重置；无 GPU 自动回退 CPU |
| `H` | HUD 开关 | 位姿、GI 状态、模式 |
| `1..4` | canonical 位姿 | 与 `CORNELL_POSES` 一致 |
| `F12` | 截图 | 引擎模式 `.local/cornellbox/engine-<epoch>.png`；预览模式 `.local/cornellbox/rt-<epoch>.png`（均为 tonemap 后、不含 HUD） |

- **键位冲突修复**：`A/S` 是相机移动、`L` 是相机右转，因此 GI 开关改为 `O/P/I/K`。
- **T = CPU 软件光追预览**：`cornellCpuRt.cpp` 对 7 个解析体做 ray-scene 求交，
  镜面球真实反射墙面、漫反射面 3 次弹射（每像素 8 条路径）；输出 640×400 RGBA 上传为 `sIMAGE` 后全屏绘制。
  它不走引擎延迟管线，切回引擎按 `T`。
- **Y = GPU compute 光追预览**：`cornellGpuRt.cpp` + `assets/shaders/gn_cornell_rt.comp`，同一套公式/同一份
  `buildCornellGeometry(false)` 场景（SSBO 上传，避免两套几何漂移）；每帧 16 spp、相机静止时把样本累加到
  accum SSBO（噪声按 1/√N 收敛，累积上限 32768 spp 后回放缓存），相机/光照变化时清零重来。
  与 `T` 互斥；设备不可用或建管线失败会打印原因并回退 `T`（CPU）。
- HUD 用引擎 2D 叠层（`GN_VkBegin2DPass` + 文本），不引入 ImGui。
- 叠层显示：位姿、光照变体、IBL/SSAO/SSGI 状态、快捷键。

### 4.3 Cornell 特有设置

- **GI 默认开**：`gEngineGiOn=true`、`gSsgiStrength=1.0`、`gSsaoOn=true`；否则默认画面只有直接光，
  看起来就是「漫反射/镜面都没有」。
- **SSGI 命中直接光**：`gn3d_ssgi.frag` 绑定 5 = LightUBO，命中处加
  `albedo × Σ(lightColor·intensity·NdotL)`；方向光直接算，点光带 attenuation；v1 不做阴影。
- **SSR 镜面**：`gn3d_deferred_light.frag` 的 `traceSsr()` 屏幕空间 64 步、0.08 m 步长；金属/低粗糙度命中墙面时
  用 `gAlbedoMetalTex + evalSH9(hitNormal)` 替代预滤波环境；未命中回退 IBL。
- **探针网格**：8×4×8、间距 2 m（引擎默认 origin=0）；当前仍是环境 SH9，C3 接 DDGI 后才承载场景 GI。
- **SSAO 半径**：盒尺度 0.2–0.5 m；页面 `aoParams` 可调。

> 注：SSGI 的命中直接光 v1 不做阴影；金属球的 SSR 只反射屏幕内可见的表面，屏幕外回退 IBL。
> 这两项都应在桌面与 PT 参考帧对比后再收紧阈值。

### 4.4 截图与复现

- 已实现 `F12`：在 `GN_VkDeferredLighting` 之后、`GN_VkBegin2DPass` 之前读回 swapchain
  （**不含 HUD**），存 `.local/cornellbox/engine-<epoch>.png`；实现见 `cornellCapture.cpp`。
- 读回走 `GN_VkReadBack`；PNG 由 `cornellCapture.cpp` 内 libpng 直接写 RGB（不再走 `GN_SavePngFile`，
  后者曾产出 0 字节文件）；BGRA 时先交换 R/B，行按读回顺序写（未翻转）。
- **未在桌面验证**：通道顺序与上下翻转仅按实现推断；若截图颜色/方向不对，先调 BGRA 分支或行序。
- 没有 sidecar、没有 headless `--frames`；对拍时人工隐藏 HUD（`H`）并用 F12。

---

## 5. 路径追踪参考模式（真实 GI）

### 5.1 算法

- **求交**：7 个物体（5 quad + 2 sphere）解析求交即可；不需要 BVH；若后续加物体再上 `cBVH`/三角形 BVH。
- **积分**：余弦加权半球采样 + 面光 NEE（next-event estimation）+ 俄罗斯轮盘（第 2 次弹射后，存活率 0.85）；
  漫反射 Lambert；金属球默认按漫反射近似（见下），`--metal` 用完美镜面。
- **光源**：面光按面积采样（均匀）；`scene=sun` 用方向光（delta 光，直接乘 BRDF，不采样、不投阴影）。
- **输出**：线性 HDR → ACES + sRGB → PNG（RGB8）；固定每像素种子，可复现；多线程按行切分。
- **实际落地（2026-11-03）**：
  - `cornellPathTracer.cpp`：`trace()` 最近命中、`directLight()` 面光 NEE、`cosineSample()`、RR、`std::thread`；
  - 面光放在天花板 1×1 开口**上方**（`y=H+0.02`），天花板本身留孔，避免近场照爆天花板产生 firefly；
  - 金属球默认 `aMetallic=0`（漫反射近似）：完美镜面 + 小面光的焦散方差极大，且金属反射不属于漫反射 GI 验收；
  - 实测：320×200×64 spp = 0.32 s；640×400×64 spp = 1.30 s；`linear max=0.323, nan=0, inf=0`；
  - 未做：EXR/sidecar、渐进累积、GGX 金属、BVH（当前 7 个物体不需要）；渐进累积只在 GPU 预览里做（见 §4.2）。

### 5.2 与引擎对比

- 同一相机位姿/投影/FOV/分辨率；同一 tonemap（引擎 composite 的 ACES/曝光参数）。
- 已实现工具 `cornellCompare`：
  - `RMSE` / `MAE` / `max`（0..255，tonemap 后 PNG）；
  - 逐通道有符号均值（`ref - test`，看整体偏亮/偏色）；
  - 可选绝对误差热力图 PNG（`--out`）。
- 原计划的「探针点相对误差 / 亮度加权误差」尚未实现；当前先看全图 RMSE/MAE 与误差图定位。
- 初始阈值（C2 起，按实际标定调整）：全图 RMSE < 40（0..255，tonemap 后）、无大面积偏色。
  **不要**在未标定前写死更严的阈值；金属球区域应掩掉（PT 默认漫反射近似）。
- 引擎截图要求：隐藏 HUD（`H`）、固定 `1280×800`、`pose=0`、`sun` 或 `area` 与 PT 一致；
  用系统截图或后续 F12 读回（当前未实现 F12）。

### 5.3 用途

- 作为 `GI_RT_TYNDALL_PLAN.md §3.7 S5` 的落地程序：先 furnace test，再 Cornell PT 参考帧。
- 引擎 GI 每改一步（SSGI 直接光、DDGI、探针），用同一组位姿/种子重跑，误差必须下降；否则回退。
- 参考帧与误差图是过程产物，落 `.local/`；只把**结论/阈值/命令**写进文档与记忆。

---

## 6. 分阶段与验收

| 阶段 | 内容 | 本机可验 | 桌面可验 |
|---|---|---|---|
| **C0 骨架** | 窗口 + 自由相机 + Cornell 几何 + 直接光（sun 变体） | `make cornellBox` exit 0；PT 侧几何可独立运行 | 4 个 canonical 位姿正确；键鼠自由视角；画面无破面 |
| **C1 引擎 GI** | 延迟路径 + SSAO/SSGI 开关 + HUD + 探针占位 | 编译；`make unit` 396/0 | SSAO 角落变暗；SSGI 开关有可见差异；关闭回退到基线 |
| **C2 PT 参考** | CPU 路径追踪 + 对比工具 + 误差图 | `make cornellPt`/`cornellCompare` exit 0；PT 实跑 640×400×64=1.3s；自比较 RMSE=0；红/绿渗色统计通过 | 引擎截图 vs PT 的 RMSE/MAE 落盘；阈值判定；金属球区域掩掉 |
| **C3 DDGI** | 探针网格接 DDGI；`P` 权重；与 SSGI 混合 | 探针 UBO/采样单测 | 屏幕外遮挡变暗；探针边界无接缝；误差低于 C2 |
| **C4 可选：盒内雾** | 均匀雾 + 顶灯光柱（丁达尔 T1/T2 的盒内版本） | 相位/透射率单测 | 光柱方向/遮挡正确；与 `GI_RT_TYNDALL_PLAN §4` 共用实现 |

**每阶段 DoD**（沿用 `LIGHTING_PLAN §8.2`）：`make` + `make unit` 0 失败；shader 编译；lint 0 违规；
新能力默认关/可回退；至少 1 条可判定断言；桌面截图/像素值落 `.local/`；文档更新一行状态。

---

## 7. 资源与性能预算（粗算，必须实测）

| 项 | 实测/估算 | 说明 |
|---|---|---|
| 几何 | 5 quad + 2 sphere（解析求交） | 不需要 BVH；引擎侧三角化约 3k 三角形 |
| 引擎帧 | 1280×800 延迟 + SSAO + SSGI | 与 gpuCheck 同量级；780M 基线待测 |
| PT CPU | 320×200×64 spp = 0.32 s；640×400×64 spp = 1.30 s | 本机实测（多线程，`--scene=area`，默认金属漫反射） |
| PT 高 spp | 320×200×1024 spp ≈ 4.3 s | 测过；1024 spp 仍有轻微金属焦散噪声（`--metal` 时） |
| 显存 | 引擎现有 G-Buffer + SSGI/SSAO 目标 | 无新增（C3 DDGI 另算） |
| 资产 | 0（全程序内） | 确定性、无外部依赖 |

> 不写死毫秒数；C1 引擎帧时间待桌面实测，C2 PT 时间随分辨率/spp 线性增长。

---

## 8. 风险与未决

1. **headless 是否可行**：`GN_StartRun` 依赖 GLFW 窗口/交换链；无窗口渲染需要引擎支持，当前**未验证**。
   退路：headless = 隐藏窗口 + 固定帧数 + 读回，不做真正的无 surface。
2. **面光近似**：引擎无面光，classic 场景的直接光与 PT 不一致；默认用 `sun` 变体做严格对照，
   classic 只做「观感/趋势」验收。
3. **引擎延迟路径只在 gpuCheck 用过**：Cornell 程序会暴露 G-Buffer/描述符/resize/槽位问题；
   这不是坏事，但要把「修引擎 bug」与「Cornell 功能」分开登记。
4. **色空间/tonemap**：引擎输出线性 HDR → composite tonemap；PT 必须用同一 tonemap 才能比；
   保存线性 HDR 供指标，PNG 仅供看。
5. **确定性**：PT 固定种子 + 固定线程切分（按像素，不按时间片）；引擎的 SSAO/SSGI 有像素哈希抖动，截图需固定帧/固定相机。
6. **范围蔓延**：不加纹理/玻璃/云/模型导入；C4 只做均匀雾。
7. **GI 默认关（引擎）**：Cornell 程序已在 `AppInit` 显式开启；其它应用/后续改动不要依赖引擎默认值。
8. **代码行数**：PT 容易写成大文件；按 §3.1 拆分，交付前跑 `lintCodeModularity`。

---

## 9. 与主路线的关系

- 本程序 = `GI_RT_TYNDALL_PLAN.md` 的 **S5 验证基准** + **第一个延迟 GI 应用**：
  - 解决「SSAO/SSGI/探针只在 gpuCheck 里、画面看不到」；
  - 给 S1（SSGI 直接光/材质）、S3（DDGI）、S4（SSR）提供固定场景/固定相机的回归基线；
  - C4 的盒内雾可复用 T1/T2 的光束实现（均匀介质比大气/云更易验证）。
- 建议顺序：**C0 → C1 → C2（可与 S1 并行）→ C3（依赖 S2/S3）→ C4（可选）**。
- 若只做一件事：先做 C0+C1（引擎 GI 可见），再做 C2（参考帧）；没有 C2，C1 的「好看」无法证明是「对」。

---

## 10. 待拍板决策（≤5）

| # | 决策 | 推荐 | 备选 / 推翻条件 |
|---|---|---|---|
| 1 | 独立程序是否链接引擎 | 链接 `libGN_mini.so`（测引擎 GI） | 若要「脱离引擎的参考渲染器」→ 只保留 C2，改独立 PT |
| 2 | 单模式还是双模式 | 双模式（engine + PT） | 工期紧 → 先 engine，PT 后置；但验证会缺 ground truth |
| 3 | 默认光源 | `sun`（直接光精确） | 若必须经典 Cornell 面光 → 接受直接光近似，误差里标注 |
| 4 | PT 实现载体 | CPU 多线程（简单、可复现） | 需要更快 → compute PT；但确定性与调试成本上升 |
| 5 | 是否纳入 headless/CI | 是（固定帧 + 阈值 + 退出码） | 引擎无无窗口模式 → 隐藏窗口 + 固定帧，先手动跑 |

---

## 11. 复算与锚点

```bash
# 现状：GI 只挂延迟、默认关
grep -n 'GN_VkSSAOPass\|GN_VkSSGIPass\|GN_VkUpdateProbes' src/graphics/_Vulkan/vk3dPasses.cpp
grep -n 'GN_VkEnableGI\|GN_VkSetSsaoEnabled\|GN_VkSetSsgiStrength' src/graphics/_Vulkan/GN_Vk.h

# 相机契约
sed -n '59,125p' src/graphics/GN_3Dcamera.h

# 延迟调用点（gpuCheck 是唯一现有调用者）
grep -rn 'GN_VkBegin3DPassDeferred\|GN_VkDeferredLighting' tests src --include=*.cpp

# 读回/截图可复用
sed -n '1,40p' tests/gpu/gpuPixelRead.h
grep -n 'SavePngFile' src/core/GN_Format.h   # GN_API，公开可复用

# 构建目标
grep -n 'cornellBox\|cornellPt\|cornellCompare' Makefile

# 本机验收（引擎需 GPU；PT/对比纯 CPU）
make cornellBox
make cornellPt
make cornellCompare
make unit
./build/cornellPt --width=320 --height=200 --spp=64 --pose=0 --scene=area \
  --out=.local/cornellbox/pt-pose0.png
./build/cornellCompare --ref=.local/cornellbox/pt-pose0.png \
  --test=.local/cornellbox/pt-pose0.png
```

> 行号以 `HEAD = aaf73ef` 为准；实现后按锚点复核。

---

## 12. 实施状态（2026-11-03）

| 阶段 | 状态 | 证据 |
|---|---|---|
| C0 骨架 | ✅ 代码级（编译通过） | `tests/cornellBox/` 资源 + `make cornellBox` exit 0；自由相机 + 5 面墙/2 球 + 方向光 |
| C1 引擎 GI | ✅ 代码级（接线） | 延迟路径 + `GN_VkSSAOPass/SSGIPass/UpdateProbes`；O/P/I/K/T/H/1-4/R/F12 键；HUD |
| C2 PT 参考 | ✅ 代码级（CPU 本机实测） | `make cornellPt` / `make cornellCompare` exit 0；PNG 产出与红/绿渗色统计通过；引擎对拍待桌面截图 |
| C2.5 CPU 软件光追预览 | ✅ 代码级 + CPU 实测 | `T` 切换；`make cornellRtProbe` mean≈(163,204,174)、nonBlack=100%；320×200≈30 ms |
| C2.6 GPU compute 光追预览 | ✅ 真机实测 | `Y` 切换；`make cornellGpuRtProbe` PASS：与 CPU 同 seed 逐像素 MAE=0.000、累积 32 帧噪声降 5.72×；640×400 8/32/64/128 spp = 7.3/12.9/14.9/24.7 ms |
| C3 DDGI | ⬜ 未开始 | 依赖 S2/S3 |
| C4 盒内雾 | ⬜ 未开始 | 依赖 T1/T2 |

- 构建/静态检查：`make cornellBox`、`make cornellPt`、`make cornellCompare`、`make cornellRtProbe`、
  `make cornellGpuRtProbe` 均 exit 0；`make cornellCheck` PASS（同 seed 输出逐像素一致 `RMSE=0.000`；
  不同 seed/位姿可区分）；`make unit` = 396 / 0；lint 见交付时实测。
- **PT 本机实测**（CPU，无需 GPU）：320×200×64 spp = 0.32 s；640×400×64 spp = 1.30 s；
  `linear max=0.323, nan=0, inf=0`；区域均值（0..255）背墙左 `(128.6,111.5,107.8)`、背墙右
  `(118.0,122.5,111.2)`、左墙 `(116.0,19.6,18.5)`、右墙 `(37.4,89.0,33.3)` —— 红/绿渗色方向正确。
- **GPU 环境（2026-11-03 更正）**：此前「本机无 Vulkan 设备」是受限沙箱看不到 `/dev/dri` 造成的误判。
  在可访问设备的会话里实测 `vulkaninfo`：AMD Radeon 780M（RADV PHOENIX / Mesa 26.2.6，Vulkan 1.4.354），
  支持 `VK_KHR_ray_query`、`VK_KHR_ray_tracing_pipeline`、`VK_KHR_acceleration_structure`。
  因此 GPU compute 预览可在本机真机验证；`VK_KHR_ray_query` 后端仍未接（见 C2.6 / 下一步）。
- **引擎模式视觉**：F12 截图已实现（composite 后、HUD 前读回）；预览模式 F12 保存光追原图。
  通道顺序/翻转仍需目视确认。
- 已知实现细节：
  - G-Buffer 默认白图会被当成法线贴图，Cornell 侧显式传 1×1 平面法线 `(128,128,255)`；
  - PT 面光放在天花板 1×1 开口上方（避免近场照爆天花板产生 firefly）；
  - PT 默认把金属球按漫反射近似（完美镜面 + 小面光的焦散方差大）；`--metal` 可开真实镜面；
  - PT `sun` 模式不投阴影（与引擎 `sun` 的「无 shadow pass」对齐）。
- **2026-11-03 用户反馈修复**（漫反射/镜面都没有、快捷键冲突）：
  - 引擎默认改为 GI 开、SSGI=1、SSAO 开；
  - SSGI 命中点增加直接光（`gn3d_ssgi.frag` binding 5 = LightUBO，方向光 + 点光衰减，v1 无阴影）；
  - 延迟着色器增加 SSR（`traceSsr()`，64 步 / 0.08 m，金属/低粗糙度，未命中回退 IBL）；
  - 键位改为 `O` SSAO / `P` SSGI / `I` IBL / `K` light（`A/S` 移动、`L` 右转不再冲突）；
  - F12 改由 `cornellCapture.cpp` 内 libpng 直接写 PNG（原 `GN_SavePngFile` 产出 0 字节）。
  - 以上均为编译级；桌面视觉/F12 仍待验证。
- **2026-11-03 CPU 软件光追预览**（3+2 的第一阶段）：
  - `T` 切换 `cornellCpuRt.cpp`：7 个解析体 ray-scene；镜面反射 + 3 次弹射路径追踪（8 spp）；`sun` 无阴影（与引擎一致），`point` 有阴影射线；背光面用天空朝向的 fill，避免纯黑；
  - 320×200 自检约 30 ms（4 线程），640×400 预览约 120 ms；输出经 `sIMAGE` 全屏绘制；
  - 纯 CPU 自检 `make cornellRtProbe`：mean≈(163,204,174)、nonBlack=100%；
  - GPU compute 后端已接（C2.6）：`Y` 键，640×400 每帧 16 spp；相机静止时逐帧累积，32 帧后平坦墙面噪声
    降 5.72×（自检实测），移动时重置。
  - 硬件 ray query 后端未接（设备支持 `VK_KHR_ray_query`，但需要 BLAS/TLAS 构建与光追管线）。
- 下一步：目视确认 `Y` 的累积收敛与移动重置；之后评估 `VK_KHR_ray_query` 后端；F12 与 PT 对拍记录 RMSE/MAE。

### 12.1 C2 复算命令

```bash
make cornellPt
./build/cornellPt --width=640 --height=400 --spp=64 --pose=0 --scene=area \
  --out=.local/cornellbox/pt-pose0.png

# 与引擎截图（桌面机）对比；隐藏 HUD（H）后按 F12，得到 .local/cornellbox/engine-<epoch>.png
make cornellCompare
./build/cornellCompare --ref=.local/cornellbox/pt-pose0.png \
  --test=.local/cornellbox/engine-pose0.png --out=.local/cornellbox/diff-pose0.png

# 纯 CPU 自检：同 seed 一致、不同 seed/位姿可区分（可进 CI）
make cornellCheck

# CPU 软件光追预览自检（真 ray-scene；输出 .local/cornellbox/cpu-rt-probe.png）
make cornellRtProbe
./build/cornellRtProbe

# GPU compute 光追自检：与 CPU 同 seed 对比 + 累积收敛 + 640×400 耗时（无 GPU 时 SKIP）
make cornellGpuRtProbe
./build/cornellGpuRtProbe
# 期望：CPU/GPU mean 相同、MAE<2、ratio≥3×、PASS

# 桌面：Y 切换 GPU 光追（静止 1–2 s 后收敛）；CORNELL_GPU_RT=1 启动即进入，便于无人值守截图
CORNELL_GPU_RT=1 LD_LIBRARY_PATH=. ./build/cornellBox
```
