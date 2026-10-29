# 统一坐标系规划（按主流引擎 / glTF 惯例修订）

> 状态：**仅规划，未改任何源码**（2026-10-29，v2 修订）。
> 依据：[docs/AXIS_CONVENTIONS.md](</home/ruicat/下载/GnVk/GnVk/docs/AXIS_CONVENTIONS.md>) 的现状盘点与 §8 漂移清单。
> 基线：`a73b572`；工作区有未提交改动（PBR 视口已在工作区改为正高度）。
> 定级：**L1**（单交付物、单人顺序）。本规划只冻结「约定 + 转换点 + 迁移步骤」；真正跨模块改代码时按 L3 走 `task-planning`。

**结论先行**：按主流 RH 引擎 / glTF 惯例，运行时统一到 **右手系、+Y 上、-Z 为相机前向、+Z 为资产正面、米、世界 CCW 为正面**；2D 屏幕单独保留 **左上原点 / +Y 下 / CW 屏幕绕序**；Vulkan 的 NDC/深度/视口差异只允许出现在投影与管线描述两个收口点；资产在导入边界对齐到「+Z 正面 + CCW 绕序」。这版与上一版（世界 CW、资产 -Z 正面）相反，**迁移范围更大**，见 §6/§8。

## 实施进度（2026-10-29）

| 阶段 | 状态 | 证据 |
|---|---|---|
| U0 契约 | ✅ 已落地 | `src/graphics/GN_Space.h/.cpp`、`tests/unit/testSpace.cpp`；`make unit` = **290 cases, 0 failed** |
| U1 投影/视口 | ◐ 工作区已具备正高度视口（未提交） | 见 §1；未做新的代码改动 |
| U2a 几何绕序 | ✅ 代码已改 | `CreatePlaneModel` 反绕序为 CCW；立方体/球体/地面实测已是 CCW，无需改；程序化四边形均 `cullBack=false` |
| U2b pass 正面 | ✅ **已修正并实测（CPU Vulkan）** | 前向/PBR/G-Buffer/材质 = **CCW**；阴影 = **CW**；llvmpipe `make gpuCheck` = **96/96 PASS**（旧 CW 规则为 78/18） |
| U3 UV（OBJ 部分） | ◐ 代码已改、未做视觉验收 | `GN_3Dmodel.cpp` 的 `vt` 改用 `flipTextureV` |
| U4 光照符号 | ◐ 辅助函数已加、未接线 | `GN_Space` 的 `sunDirectionToLightDirection` / 反向函数 + 单测 |
| U5 资产/相机 | ✅ 代码已改、**视觉待验** | MMD 两个 builder 镜像 X **并反转绕序**（恢复「CCW + 外向法线」）；`resetCamera` 移到 +Z 侧；探针 mode.pmd 验证绕序/法线一致 |
| 漂移注释 | ✅ 已修 | `mmdMeshBuilder.*`、`mmdViewer.cpp` 地面/绕序注释 |
| GPU 验收 | ◐ CPU 侧已验，真实 GPU 待主人 | 本沙箱无 `/dev/dri`；用 llvmpipe（CPU Vulkan）跑通 96/96，真机 RADV 仍需复跑同一命令 |

---

## 0. 目标 / 非目标

**目标**

1. 给 3D 世界、相机、物理、天空/光照、资源导入、渲染 pass 一份**单一事实来源**，且与 glTF/主流引擎的「+Y 上、+Z 资产正面、CCW 正面」对齐。
2. 把「Y 翻转、视口高度符号、正面绕序、UV 上下、sun/light 符号」从散落的注释/字面量收口成**可测的常量与函数**。
3. 消除 [AXIS_CONVENTIONS §8](</home/ruicat/下载/GnVk/GnVk/docs/AXIS_CONVENTIONS.md>) 的 7 处漂移，并加机检门禁防止回潮。

**非目标**

- 不把 2D/UI 改成 +Y 向上：屏幕坐标与 3D 世界是**两个空间**，合并会让所有 UI 与输入跟着翻。
- 不重写渲染器、不换 Vulkan 约定、不引入 SPIRV-Cross/数学库。
- v1 不做 MMD 的单位换算（模型高度当前直接当米用；改 scale 会连带相机距离/云层/降水，另立任务）。
- 不要求所有资产「手性一致」；只要求**每个导入器输出运行时坐标**，并在元数据里记录来源空间。

---

## 1. 现状与目标差异（关键）

| 项 | 现状 | 目标（本规划） |
|---|---|---|
| 世界正面绕序 | **混合**：MMD 原始绕序与顶点法线一致（探针 agree=6189/disagree=40，并非旧文说的 CW）；内置 cube/sphere/ground 实测 CCW；`CreatePlaneModel` 是 CW | **CCW**（主流/glTF 惯例）统一 |
| 3D pass `frontFace` | **旧版 U2b 误改为 CW**（前提写反）；2026-10-29 用 llvmpipe 跑 gpuCheck 实测后改回 **CCW** | 含 Y 翻转投影的 3D pass = **CCW**；identity / 未翻转正交 = **CW** |
| 阴影 pass | 旧版 U2b 误改为 CCW；现为 **CW**（无 Y 翻转，世界 CCW 被镜像成帧缓冲 CW） | **CW**（无 Y 翻转） |
| 2D 屏幕 | 左上原点、+Y 下；CW | **保持**（独立屏幕空间，不参与 3D 绕序统一） |
| 资产正面 | MMD/glTF 原样（+Z 前）；OBJ 无 | **+Z**（glTF 原生）；相机前视在 **+Z 侧**（原查看器在 -Z 侧，见 §6 U5） |
| glTF 绕序 | 原生 CCW | **CCW**（无需翻绕序、无需旋转） |
| OBJ 绕序/UV | 原始 CCW；v 未翻 | CCW（无需翻绕序）；**v = 1-v** |
| MMD 手性/绕序 | LH；原始绕序与顶点法线一致（CCW 口径） | RH + CCW（镜像 X 后**再反转绕序**一次，恢复与法线一致；见 §4.2） |
| 现有几何 | **混合**：MMD 镜像后需反绕序；cube/sphere/ground CCW；plane CW | 统一为 CCW（MMD 镜像+反绕序、plane 反绕序；cube/sphere/ground 已合规） |
| 各 pass 的 `frontFace` | 不统一（见 AXIS_CONVENTIONS §4/§8） | 由 `frontFaceForWorldPass()` 单点生成 |

**为什么这次是「多数 pass 要改」**：旧文把「MMD 是 CW 绕序」当成前提，但 2026-10-29 的 CPU 探针（真实 `mode.pmd`）显示
原始 MMD 的三角形绕序与顶点法线一致（`agree=6189 / disagree=40`）；镜像 X 后变成 `agree=40 / disagree=6189`，
即模型内外翻转。所以 MMD 转换必须「镜像 X + 反转绕序」两步，世界内容才是 CCW 且法线朝外。

**2026-10-29 实测修正（推翻 v2 旧前提）**：旧文写「CCW 内容被投影 Y 翻转后变 CW」，方向是反的。
判据在 **帧缓冲坐标**（Vulkan NDC 的 +Y 向下）：正高度视口下，投影的 Y 翻转只把画面摆正，
不改变视觉绕序 ⇒ 世界 CCW 内容仍是帧缓冲 CCW，`frontFace` 必须 **CCW**；只有 `identity` /
未翻转正交（画面被垂直镜像）才需要 **CW**。
证据：`VK_ICD_FILENAMES=$PWD/.local/probes/lvp_icd.json make gpuCheck`（llvmpipe 26.2.5，CPU Vulkan）：
旧规则（CW）**pass=78 fail=18**，其中 `ground.placement`/`ground.pbr`（真实透视 + 世界 CCW 四边形）被判背面剔除、
`3d.forward`/`3d.pbr`/`material.draw`/`texture.sample`/`cull.draw.*` 同因失败；改回 CCW（阴影 CW）后 **96/96 PASS**。

**证据**：`vk3dInit.cpp:232,385,560`；`vk3dInitScene.cpp:468,707,885`；`tests/gpu/gpuCheck3D.cpp:305-307`（G-Buffer 用 identity viewProj）；`mmdMeshBuilder.cpp` 的 `reverseTriangleWinding`（镜像后反绕序）；`mmdViewer.cpp:477-485`（地面 0,2,1 绕序）。

---

## 2. 规范（Canonical v2，建议冻结）

### 2.1 五个空间

| 空间 | 原点/轴 | 用途 | 是否允许业务代码直接用 |
|---|---|---|---|
| **World（世界）** | 右手；**+X 右、+Y 上、-Z 相机前向、+Z 资产正面**；米；正面 **CCW** | 3D 位置/朝向/物理/天空/资产落点 | ✅ 唯一 3D 权威空间 |
| **View（视图）** | 右手；+X 右、+Y 上、**-Z 前** | 相机矩阵 | ✅ 只由相机/矩阵函数产生 |
| **Clip/NDC** | Vulkan：x 右、**y 下**、**z ∈ [0,1]** | 投影输出 | ❌ 禁止手写投影 |
| **Screen（屏幕）** | **左上原点、+Y 下**、像素；屏幕绕序 **CW** | 2D/UI/输入/HUD | ✅ 2D 唯一权威空间 |
| **Texture/UV** | **(0,0)=左上**、**v 向下**、图像 top-down | 所有贴图采样 | ✅ 导入边界转换 |

> 「+Z 资产正面」与「-Z 相机前向」可以同时成立：相机在资产 +Z 侧、朝 -Z 看，看到的就是资产正面。查看器当前默认相机在 -Z 侧（朝 +Z），迁移后需挪到 +Z 侧（§6 U5）。

### 2.2 常量与符号（新头 `GN_Space.h`）

```cpp
namespace GN3d {

extern const GNg::sVEC3 SPACE_WORLD_RIGHT;   // ( 1, 0, 0)
extern const GNg::sVEC3 SPACE_WORLD_UP;      // ( 0, 1, 0)
extern const GNg::sVEC3 SPACE_CAMERA_FORWARD;// ( 0, 0,-1)
extern const GNg::sVEC3 SPACE_ASSET_FRONT;   // ( 0, 0, 1)

extern const float SPACE_MMD_UNIT_SCALE;     // v1 = 1.0f

enum eSPACE_HANDEDNESS { eSPACE_RH, eSPACE_LH };
enum eSPACE_FACE { eSPACE_FACE_CCW, eSPACE_FACE_CW }; // 世界正面 = CCW

} // namespace GN3d
```

**规则**

- 世界正面统一为 **CCW**（从物体外侧看）；所有网格导入器必须输出 CCW 绕序。
- 投影 Y 翻转**不改变**视觉绕序（Vulkan 在 +Y 向下的帧缓冲坐标里判定）：**含 Y 翻转的 3D pass → `frontFace = CCW`；不含翻转的阴影 / identity 用例 → `frontFace = CW`**。
- 2D 屏幕是独立空间：屏幕内容绕序 CW、`frontFace = CW`，不参与 3D 规则。
- 相机/视口的 Y 翻转只由 `Mat44PerspectiveVK` / `Mat44OrthoVK` 产生；业务代码禁止自拼投影。
- 视口**只允许正高度**；负高度视口视为违规（它等价于再翻一次 Y，是漂移根源）。
- 资产正面统一为 **+Z**；导入器负责把来源资产正面转到 +Z，并把绕序归一化为 CCW。

### 2.3 正面绕序规则（单点生成）

```cpp
// 世界内容统一为 CCW（从外侧看）。
// projectionFlipsY：VK 透视/正交 = true；2D 屏幕投影 / 阴影正交 = false。
// 正高度视口下 Y 翻转只把画面摆正，CCW 内容仍是帧缓冲 CCW ⇒ 3D 用 CCW；
// 不翻 Y（identity / 未翻转正交）会把 CCW 镜像成 CW ⇒ 用 CW。
eSPACE_FACE frontFaceForWorldPass(bool projectionFlipsY) {
    return projectionFlipsY ? eSPACE_FACE_CCW : eSPACE_FACE_CW;
}
eSPACE_FACE frontFaceForScreenPass() { return eSPACE_FACE_CW; } // 2D 独立
```

用这条规则对照目标：

| pass | projectionFlipsY | 目标 frontFace | 现状 | 动作 |
|---|---|---|---|---|
| 3D 前向 / PBR / 延迟 / 云 / 贴花 / 降水 / 布料 / 蒙皮 / 粒子 / 材质 | true | **CCW** | CCW（剔除开启的：前向/PBR/材质；其余 cullBack=false） | 已完成（2026-10-29 实测修正） |
| G-Buffer | true | **CCW** | CCW | 已完成；测试改用透视/`BA` 索引 |
| 阴影 | false | **CW** | CW | 已完成 |
| 2D | false（屏幕空间） | **CW** | CW | 保持 |
| 天空 / 合成 | 剔除关闭 | 不适用 | — | 保持 |

**G-Buffer 的特殊问题**：它用真实透视相机（含 Y 翻转），所以 `frontFace=CCW`；但 `gpuCheck3D.cpp` 的裸绘制用
`identity viewProj`（无 Y 翻转），identity 下必须用 `kQuadIndexBA` 才落在帧缓冲 CCW 侧。
「值对、验证口径错」会掩盖真实剔除问题，故测试里保留两条口径（identity 索引 + 真实相机 ground.placement）。

**G-Buffer 的特殊问题**：它的 `frontFace=CW` 在新规则下是对的，但 `gpuCheck3D.cpp:305-307` 传的是 `identity viewProj`（无 Y 翻转），在 identity 下 CW 只对 CW 内容成立。**必须补一条真实相机 viewProj 的 G-Buffer 回归**，否则「值对、验证口径错」会掩盖真实剔除问题。

---

## 3. 边界与转换矩阵

| 边界 | 输入空间 | 转换 | 落点 | 验收 |
|---|---|---|---|---|
| 2D 绘制/UI/输入 | Screen | 不做空间转换；走 `CoordWindowToView*` | 现有 `GNg::cVIEW2D` | UI 位置/鼠标回归 |
| 3D 相机 → 渲染 | World → Clip | `cCAMERA::BuildViewProj`（见 CAMERA_PLAN） | `GN_3Dcamera.*` | NDC 中心/深度单测 |
| 3D 管线描述 | — | `makeViewport()` 只产正高度；`frontFaceForWorldPass()` | `GN_Space.*` + `vkPipeline` | 管线描述单测 + gpuCheck |
| glTF 导入 | RH、+Y、**+Z 前**、CCW | **恒等**（轴/绕序都不动） | `GN_3Dasset.*` | 正面 marker + 绕序单测 |
| OBJ 导入 | RH、+Y、通常 CCW | **恒等绕序**；**UV v = 1-v** | `GN_3Dmodel.*` | UV/绕序单测 + 贴图截图 |
| MMD 导入 | LH、+Y、+Z 前；原始绕序与顶点法线一致 | 镜像 X + **反绕序一次**，恢复 CCW 与法线一致（见 §4.2） | `mmdMeshBuilder.*` | fixture 单测 + 视觉验收（已加 `mmdPmdMirrorKeepsWindingConsistentWithNormals`） |
| 内置几何 | 目前按 CW 构建 | **反绕序 → CCW**（立方体/球体/地面 quad/程序化四边形） | `GN_3Dmodel.cpp`、`mmdViewer.cpp`、各程序化 pass | 单测 + gpuCheck |
| 图像/纹理上传 | top-down | 统一 top-down；TGA 原点左下时翻行 | 现有解码/上传 | 行序单测 |
| 天空/方向光 | — | `towardSunToLightDir()` / `lightDirToTowardSun()` | `GN_Space.*` | 点积符号单测 |
| 物理 | World | 无转换；重力/胶囊继续 +Y | `GN_Physics.*` | 现有物理用例 |
| 调试/HUD | Screen | 屏幕坐标直接画 | `mmdViewer` | 目视 |

---

## 4. 资产空间策略

### 4.1 glTF / OBJ（低风险，且这版最省）

- **glTF**：规范为 RH、+Y 上、**+Z 为正面**、三角形正面 **CCW**；与运行时（RH、+Y 上、+Z 资产正面、CCW）**完全一致 ⇒ 导入零转换**。现有 glTF 只到 CPU 描述（`GN_3Dasset.h`），还没有渲染用例，风险低。
- **OBJ**：RH、+Y 上；OBJ 没有「正面」标准（通常按 CCW）⇒ **绕序保持**；UV 的 `v` 原点在左下，而图像 top-down ⇒ **导入时 `v = 1 - v`**（`GN_3Dmodel.cpp:282-286` 目前未翻）。需补一个带方向标记的贴图测试。
- **内置几何**：当前按 CW 构建（地面 quad `0,2,1 / 0,3,2`、部分程序化四边形），迁移到 CCW 需要**逐个反绕序**；这是 U2 的主要工作量。

### 4.2 MMD（必须先做 fixture，再决定镜像轴）

已知事实互相矛盾，**不能凭推理定转换**：

| 来源 | 说法 |
|---|---|
| PMX 资料 | 左手系、+Y 上、**+Z 向前**、三角形绕序为**顺时针**（DirectX 风格） |
| 查看器注释 | 「MMD 模型面向 **-Z**，只有从 -Z 侧看才是正面」（`mmdViewer.cpp:206-222`） |
| 当前渲染 | 原始坐标不做转换直入（`mmdMeshBuilder.cpp:175-177`），能出画面（用户目视验收过） |

**fixture 判定**：用 `tests/うさうさ/うさうさ.pmx`（已在 `.gitignore`，本机存在），在模型上找已知不对称特征或注入一个 +Z 方向 marker，回答：

1. 原始 LH 坐标里，模型正面到底在 **+Z** 还是 **-Z**？
2. 当前「原样直入」的画面看到的是正面、背面还是镜像？

判定后按下表选转换（目标：RH、正面 **+Z**、CCW）：

| 观测 | 转换 | 代价 / 结果 |
|---|---|---|
| 原始正面 = **+Z** | `x' = -x`（镜像 X）+ 法线 x 取反 | 正面保持 +Z；CW 绕序在镜像后自然变 **CCW**（无需再手工反绕序）；左右互换是手性转换的固有代价，需 fixture 确认 |
| 原始正面 = **-Z** | `z' = -z`（镜像 Z）+ 法线 z 取反 | 正面变 +Z；CW 绕序自然变 CCW；前后语义随镜像改变 |
| 当前画面已依赖「原样直入」 | 方案 B：**不转几何**，元数据标 `eSPACE_LH`，只在 MMD 绘制入口套显式镜像矩阵 | 不是真统一；零视觉回归，作为 v1 兜底 |

**推荐**：先按 fixture 结果做转换；若转换后你目视验收不通过（正反/左右不对），退回方案 B 并把 MMD 登记为**来源空间例外**。**不接受**「不验证就转换」。

### 4.3 单位

- 运行时单位 = 米；`SPACE_MMD_UNIT_SCALE = 1.0f`（v1 不换算）。
- 资产元数据增加 `aSourceSpace`（`eSPACE_RH / eSPACE_LH`）与 `aUnitScale`；导入器负责填，渲染层不感知。

---

## 5. 代码落点与接口

| 文件 | 职责 | 预计有效行 |
|---|---|---|
| `src/graphics/GN_Space.h` | 常量、`eSPACE_*`、转换描述、`frontFaceForWorldPass`/`frontFaceForScreenPass`/`makeViewport` 声明 | ~85 |
| `src/graphics/GN_Space.cpp` | 常量定义、绕序规则、视口构造、`sSPACE_CONVERSION::apply`、sun/light 符号转换 | ~180 |
| `src/graphics/GN_SpaceCheck.cpp` | 运行时校验：负高度视口拒绝、`frontFace` 与投影翻转一致性、非法 `aSourceSpace` | ~90 |
| `tests/unit/testSpace.cpp` | §7 的 CPU 用例 | ~240 |
| 改动：`GN.h` | +2 行 include | +2 |
| 改动：导入器/内置几何 | glTF 恒等；OBJ v 翻转；MMD 按 §4.2；内置四边形反绕序 | 各 20-60 行 |

**结构约定**

```cpp
struct sSPACE_CONVERSION {
    bool aMirrorX, aMirrorY, aMirrorZ; // 至多一个为 true（手性转换）
    bool aReverseWinding;              // 显式反绕序（无镜像的归一化用）
    float aScale;
    GNg::sQUAT aRotation;              // 纯旋转，不改手性
};

bool applySpaceConversion(const sSPACE_CONVERSION &conv,
                          float *vertices, uint32_t vertexCount, uint32_t strideFloats,
                          uint32_t *indices, uint32_t indexCount);
```

**校验规则**

- `aMirrorX/Y/Z` 至多一个为 true。
- **镜像本身反转一次绕序**：MMD 原始数据绕序与法线一致，镜像 X 后必须 `aReverseWinding = true` 才能恢复
  「CCW + 外向法线」（2026-10-29 探针实测；旧文写 false 是错的，会导致模型内外翻转）。
- 无镜像时，`aReverseWinding=true` 用于把 CW 内置几何/OBJ 归一化为 CCW。
- 管线描述：`height <= 0` 拒绝；`frontFace` 必须等于 `frontFaceForWorldPass(projectionFlipsY)`（2D 用 `frontFaceForScreenPass()`）；不一致时告警并给出期望值。
- 资产：`aSourceSpace == eSPACE_LH` 时必须带非零转换（否则拒绝导入）。

---

## 6. 迁移阶段（每阶段可独立验收/回退）

| 阶段 | 内容 | 文件 | 验收 | 失败处置 |
|---|---|---|---|---|
| **U0** 契约 | `GN_Space.h/.cpp` + `testSpace` 骨架；不改变行为 | `GN_Space.*`、`GN.h`、`testSpace.cpp` | `make unit` = 275 cases 全绿 | 编译错误就地修 |
| **U1** 投影/视口 | 所有 pass 正高度；PBR 以工作区版本为准；`makeViewport` 收口 | `GN_Space.cpp`、`vk3dInit*`、各 pass | `make unit` ≥275；桌面 `make gpuCheck` **95/95** | 只回退 U1 |
| **U2a** 几何绕序 | 内置/程序化几何统一为 CCW：`CreatePlaneModel` 反绕序；cube/sphere/ground 实测已 CCW 保留；程序化四边形（云/贴花/降水/粒子/蒙皮）均 `cullBack=false` 不参与；MMD 归 U5 | `GN_3Dmodel.cpp`、`mmdViewer.cpp` | 反绕序单测；地面/图元不消失 | 只回退 U2a |
| **U2b** pass 正面 | 3D 前向/PBR/G-Buffer/材质 `frontFace = CCW`；阴影 = CW（**旧文写成 CW/CCW，2026-10-29 实测修正**）；同步 gpuCheck 的 identity 口径索引 | `vk3dInit*`、`vk3dInitScene.cpp`、`vkMaterial.cpp` | llvmpipe `make gpuCheck` = **96/96 PASS**；真机复跑同一命令 | 只回退 U2b |
| **U3** UV | OBJ `v=1-v`；纹理/图像行序契约；补 `space.uvFlip` | `GN_3Dmodel.cpp`、`testSpace.cpp` | 单测；有纹理的 OBJ 截图（桌面） | 回退 U3 |
| **U4** 光照符号 | `towardSun/lightDir` 转换函数；查看器/天空/方向光改用函数；补 `space.sunSign` | `GN_Space.*`、`mmdViewer.cpp` | 点积符号单测 + 视觉“太阳与阴影同侧” | 回退 U4 |
| **U5** 资产/相机 | MMD 两个 builder 镜像 X + **反转绕序**（恢复 CCW 与法线一致）；查看器默认相机挪到 +Z 侧；glTF 恒等 | `mmdMeshBuilder.*`、`mmdViewer.cpp`、`docs/CAMERA_PLAN.md` | 绕序/法线一致性探针 + 新单测；视觉待主人验：正面、左右、地面/天空 | 回退 mirrorX + 反绕序 / 恢复 -Z 相机 |
| **U6** 清理 | 删除/改写漂移注释；CHANGELOG；grep 门禁 | 文档 + 脚本 | 门禁全绿；`AXIS_CONVENTIONS.md` 更新 | — |

**依赖与验证顺序**：代码已按 **U0 → U2a → U5 → U2b** 一次性落地；**桌面验证**必须按同一顺序：先 `make gpuCheck` 看几何是否还在（U2a/U5），再看各 pass 是否整片剔除（U2b）。U2b 必须在 MMD 与内置几何都已 CCW 之后才能翻 `frontFace`；若 MMD 视觉不通过退回方案 B，则 U2b 需要给 MMD 单独一条 pass/绘制路径，不能与统一 pass 共用 `frontFace`。

---

## 7. 测试与门禁

### 7.1 `testSpace` 用例（CPU，目标 12 条）

| # | 用例 | 断言 |
|---|---|---|
| 1 | `space.constants` | `UP=(0,1,0)`、`CAMERA_FORWARD=(0,0,-1)`、`ASSET_FRONT=(0,0,1)`、`RIGHT=(1,0,0)`；`RIGHT×UP=CAMERA_FORWARD` |
| 2 | `space.conversionRules` | 多于一个镜像轴 ⇒ false；镜像（MMD）+ 不反绕序合法；无镜像 + 反绕序（内置归一化）合法；非法组合拒绝 |
| 3 | `space.conversionWinding` | 反绕序后三角形正面法线与绕序约定一致（叉积断言） |
| 4 | `space.conversionRoundTrip` | 镜像两次 + 反绕序两次 = 原数据（1e-6 容差） |
| 5 | `space.mmdMirrorX` | 输入 +Z 正面 → 输出 +Z；x 取反；镜像后 CW→CCW |
| 6 | `space.gltfIdentity` | glTF 输入（+Z 前、CCW）→ 输出不变；行列式 +1 |
| 7 | `space.uvFlip` | OBJ `v=0.25` → `0.75`；`0/1` 端点不变 |
| 8 | `space.frontFaceRule` | `frontFaceForWorldPass(true)=CCW`、`(false)=CW`；`frontFaceForScreenPass()=CW`；与目标 pass 表一致 |
| 9 | `space.viewportRejectNegative` | `makeViewport(w,-h)` 无效；合法值 `height>0` |
| 10 | `space.cameraNdc` | 相机在 (0,0,5) 看原点：原点 NDC≈(0,0)、深度∈[0,1]；世界 +Y 点 NDC y<0 |
| 11 | `space.frustumY` | 世界 +Y 点越过视锥索引 2 面（与 `testSpatial` 口径一致） |
| 12 | `space.sunLightSign` | `sunDirection` 与 `light.direction` 互为相反数；转换函数往返一致 |

### 7.2 GPU 门禁（桌面，不能在本沙箱跑）

新增：`space.groundPlacement`（沿用现有 ground.placement）、`space.gbufferRealCamera`（真实 viewProj 下 G-Buffer 几何不被剔）、`space.pbrWinding`（PBR 正面可见）、`space.uvOrientation`（带方向标记的贴图读回）、`space.sunSide`。目标：从 95/95 提升到 **≥99/99**（具体以实测为准）。

**U2b 已同步的既有 GPU 用例（2026-10-29）**：

- `tests/gpu/gpuCheck3D.cpp`：forward/PBR/composite 用 `kQuadIndexBA`（identity + CCW 管线）；阴影保留 `kQuadIndexAB`（CW）；G-Buffer 裸绘制改 `BA`。
- `tests/gpu/gpuCheckCull.cpp`、`gpuCheckBatch.cpp`：`Mat44OrthoVK` 的 bottom/top 交换（m[5] 为负 = 等价 Y 翻转），立方体 z 从 0.7 挪到 0.4 避免 +Z 面被远平面裁掉；调试线的世界 y 取反以落在中心像素行中心。
- `tests/gpu/gpuCheckPrecipitation.cpp`：`precipitation.pixels`/`overGeometry` 改用真实透视 VP（identity 的 `w=1` 会被片元 `smoothstep(1.5,6,w)` 近相机淡出判成 alpha=0，这是测试口径问题，不是管线缺陷）。

复算命令（CPU Vulkan，非真 GPU）：`VK_ICD_FILENAMES=$PWD/.local/probes/lvp_icd.json make gpuCheck` → `pass=96 fail=0`。

### 7.3 grep 门禁（U6）

```bash
# 负高度视口：应为空
grep -rn "height\s*=\s*-" src/graphics/_Vulkan/*.cpp

# 投影/视口/正面：只允许出现在收口文件与 pass 表
grep -rn "Mat44PerspectiveVK\|Mat44OrthoVK" src/graphics --include=*.cpp
grep -rn "aFrontFace" src/graphics/_Vulkan/*.cpp

# UV 翻转只允许出现在导入器
grep -rn "1.0f - .*v\|1 - .*v" src/graphics/GN_3Dmodel.cpp src/graphics/GN_3Dasset.cpp
```

预期：迁移后 `Mat44PerspectiveVK/OrthoVK` 只出现在 `GN_3Dcamera.cpp`/`GN_Space.cpp`/测试；`aFrontFace` 只出现在 `vkPipeline` 的规则应用处或 `GN_Space.cpp`。

---

## 8. 风险与待拍板

| # | 事项 | 影响 | 建议 |
|---|---|---|---|
| 1 | 旧文把 MMD 当 CW 源；实测原始绕序与法线一致，镜像后必须再反绕序 | 不反绕序 ⇒ 模型内外翻转、光照像法线反了（已发生并由主人反馈） | 已在 `mmdMeshBuilder.cpp` 修；探针 `mode.pmd` 前后 agree 从 40 → 6189；新增单测锁定 |
| 2 | 3D pass 的 `frontFace` = **CCW**（含 Y 翻转）、阴影 = **CW**（2026-10-29 实测修正；旧文方向写反） | 任一 pass 改错会整片剔除 | llvmpipe gpuCheck 96/96；真机复跑确认 |
| 3 | MMD 代码已按「+Z 正面」假设做 mirrorX，但 fixture/视觉未验 | 角色左右/正反可能不对 | 用 `tests/うさうさ` 目视；不通过改 mirrorZ 或退方案 B |
| 4 | 默认相机要从 -Z 侧挪到 +Z 侧（资产正面 +Z） | 查看器默认画面、CAMERA_PLAN 的 FRONT/yaw/测试都要同步 | U5 一起改，避免两套默认视角 |
| 5 | G-Buffer 用真实透视相机时 `frontFace=CCW`；identity 用例必须换 `BA` 绕序 | 口径写错会「值对、验证错」，掩盖真实剔除 | 已用 llvmpipe 实测；identity 与真实相机两条口径都保留 |
| 6 | PBR 工作区改动（正高度 + CCW）尚未提交 | 以哪版为基线 | 以工作区为正高度基线；若回退 HEAD，U1/U2 重做 |
| 7 | 2D 保持 +Y 下/CW，是否算「统一」 | 认知分歧 | 明确写进规范：Screen 是独立空间，转换点固定 |
| 8 | OBJ 无正式正面约定 | 转向语义不确定 | v1 只做绕序/UV 归一化，不强行定义 OBJ 正面 |
| 9 | 世界单位与 MMD scale | 换 scale 连带相机/云/降水 | v1 取 1.0；单独立项 |
| 10 | 未来 macOS/MoltenVK 的 Y/深度差异 | 平台回归 | 投影只从 `BuildViewProj` 出 |

**需要你拍板的 3 件事**：

1. MMD 已按「PMX +Z 正面」实现 mirrorX；请在桌面用 `tests/うさうさ` 目视验收正面/左右。不通过时选：改 `mirrorZ`，或退方案 B（不转几何、单独绘制路径）。
2. 默认相机已挪到 +Z 侧（`resetCamera`），请确认默认视角看到的是正面。
3. OBJ 的 `v` 翻转已做；有 OBJ 资源时补一张带方向标记的贴图截图。

**GPU 验收状态**：本沙箱无 `/dev/dri`，但可用 llvmpipe（CPU Vulkan）；
`VK_ICD_FILENAMES=$PWD/.local/probes/lvp_icd.json make gpuCheck` 已跑到 **96/96 PASS**。
真机（AMD RADV）仍需用同一命令复跑一次确认；重点看 `ground.placement`、`decal`、`precipitation`、
`cloud`、`lod` 与 G-Buffer 真实相机剔除，以及 MMD 模型是否完整可见。

---

## 9. 参考锚点

| 锚点 | 内容 |
|---|---|
| `docs/AXIS_CONVENTIONS.md` | 现状总表 + 7 处漂移（本规划的直接输入） |
| `src/graphics/GN_3Dmath.cpp:342-355,405-433` | 投影 Y 翻转 / RH LookAt |
| `tests/unit/testSpatial.cpp:277-283` | 世界 +Y 与视锥索引 2 的回归口径 |
| `src/graphics/_Vulkan/vk3dInit.cpp:232-233,387,563` | 前向/PBR = CCW、阴影 = CW（2026-10-29 实测修正） |
| `src/graphics/_Vulkan/vk3dInitScene.cpp:707`；`src/graphics/_Vulkan/vkMaterial.cpp:92` | G-Buffer / 材质 = CCW |
| `src/graphics/GN_Space.cpp:17-24` | 正面规则单一事实来源（含实测依据注释） |
| `tests/gpu/gpuCheckPrecipitation.cpp` `makePerspectiveViewProj` | 降水的 identity 口径已改为透视（近相机淡出依赖真实视深） |
| `src/graphics/GN_3Dmodel.cpp:287` | OBJ UV 已用 `flipTextureV` |
| `src/mmd/mmdMeshBuilder.cpp:187-188,235-236` | MMD 两个 builder 的 mirrorX |
| `src/graphics/GN_3Dasset.cpp:142-149` | glTF 原样读入（与目标一致，零转换） |
| `src/mmd/mmdMeshBuilder.cpp:175-177,126-131` | MMD 原样坐标/CW 绕序 |
| `src/graphics/weather.cpp:20`；`assets/shaders/gn3d_pbr.frag:180` | sun/light 符号相反 |
| glTF 规范 | RH、+Y 上、**+Z 前**、正面 CCW（<https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html> §3.4） |
| PMX 资料 | 左手、+Y 上、+Z 前、三角形顺时针绕序（[PMX 2.1 规范](https://github.com/dskjal/OpenMMD/blob/main/docs/specification/pmx2.1-specification.md)） |
| Vulkan 规范 | NDC y 向下、深度 [0,1]、视口变换（<https://registry.khronos.org/vulkan/specs/latest/html/vkspec.html>） |
