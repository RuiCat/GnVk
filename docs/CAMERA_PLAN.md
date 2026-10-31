# 摄像机移动（GN3d::cCAMERA）实现规划

> 状态：**仅规划，未改任何源码**（2026-10-29）。
> 基线：`git rev-parse --short HEAD` = `a73b572`；工作区另有 119 个未提交改动（既有 MMD / 天气等工作，与本规划无关）。
> 参考库：**GN 引擎 3D 相机类 `GN3d::cCAMERA`**（GN SDK 1e），取证副本放在 `.local/vendor/`（已 gitignore，不入库）。
> 定级：**L1**（单交付物、单人顺序、无并行执行体、无需要冻结的跨模块契约）。

**结论先行**：把相机从 `mmdViewer` 抽成引擎层纯 CPU 模块 `GN3d::cCAMERA`，按 GN SDK 1e 的操作契约实现「移动原语 + 自由/轨道/跟随三模式 + 自动取景」；先 C0–C4 用单测锁定语义，最后 C5 才接查看器。渲染接口（`GN_VkBegin3DPass`/`cMODEL`）零改动，风险集中在 C5 的桌面 GPU 回归。

---

## 0. 范围与参考

### 0.1 做什么 / 不做什么

**做**（本规划覆盖、后续按阶段实现）：

- CPU 侧相机核心：位置/朝向、镜头参数（FOV / near / far / 正交高度）、视图矩阵与投影矩阵。
- 与 GN `cCAMERA`/`cOBJ3D` 对齐的移动原语：`SelfMove`、`SelfRotLR/TB/CW/Axis`、`ToMove`、`SetWorldPos`/`GetWorldPos`。
- 三种移动模式：**自由飞行**、**轨道**、**目标跟随**（含平滑与自动对准）。
- 自动取景：`SetCameraLookDirection/Range/Obj` 的等价实现（按对象/范围自动摆位与 FOV 适配）。
- 输入适配层（输入 → 移动原语），以及 `mmdViewer` 接线（替换现有散落相机代码）。
- 纯 CPU 单元测试（不依赖 GPU / 窗口）。

**不做**（明确延后，理由见 §2.5 / §6）：

- 相机动画关键点（`sAM_CAMERA_KEYS`，只有 FOV 关键点；位置/旋转动画走 `cOBJ3D` 的 `sAM_TRANSF_KEYS`）。
- 景深（引擎 3D 管线无 DOF 渲染语义）。
- 鼠标拾取射线 / 多视口 / 贴图映射（`cVIEW3D` 的其它职责）。
- 遮挡碰撞（spring arm）——列为 v2（§4.8）。
- 焦距（mm）换算——传感器幅面未定（§2.2 表）。

### 0.2 参考库取证

| 项 | 值 |
|---|---|
| 类 | `GN3d::cCAMERA`（3D 摄像机，`GN_3D.h:725`） |
| 来源 | <http://www.img-power.com/load/Release/GN_SDK1e.zip> |
| 大小 / 服务器时间 | 100,275,623 B / `Last-Modified: Wed, 10 Dec 2025 05:32:17 GMT` |
| SHA-256 | `fe0468d63622f9740bd87b801982084d54bd8c15ae3df7d4f34de6830020e1a1` |
| 本地路径 | `.local/vendor/GN_SDK1e.zip`；解包 `.local/vendor/gnsdk/GN_SDK1e/Include/`；UTF-8 转写 `*.utf8.h`（行号与原文一致） |
| 包内容 | 头文件 + 二进制（`App.dll`/`App.lib` 等）+ `Sample/**/*.cpp`；**无引擎实现源码** |
| 许可边界 | SDK 为上游闭源分发（同站 GN_mini 2D 源码才是 MIT，且 `GN_mini1.0.zip` 的 `project/src` 只有 2D 头、无 `GN_3D*.h`）。本仓库保持 MIT 重实现：**只参考 API 语义与行为，不复制实现代码，不把 SDK 头文件/二进制入库**。 |

### 0.3 复算命令

```bash
# 1) 校验参考包
sha256sum .local/vendor/GN_SDK1e.zip

# 2) 相机类与移动原语（行号与 UTF-8 转写一致）
grep -an "class GN_API cCAMERA\|struct GT_sFOCUS_TARGET\|void SelfMove(float" \
  .local/vendor/gnsdk/GN_SDK1e/Include/GN_3D.h

# 3) 自动取景与观看枚举
grep -an "SetCameraLookDirection\|SetCameraLookRange\|SetCameraLookObj" \
  .local/vendor/gnsdk/GN_SDK1e/Include/GN_3D.h
grep -an "enum eCMA_LOOK_MODE\|enum eCAMERA_FOV_TYPE" \
  .local/vendor/gnsdk/GN_SDK1e/Include/GN_3D.h \
  .local/vendor/gnsdk/GN_SDK1e/Include/GN_3Dtype.h

# 4) 样例中的真实用法
grep -an "SetCameraLookRangeV\|SetFollowOBJ\|SetTarget" \
  .local/vendor/gnsdk/samples/GN_SDK1e/Sample/3d/*.utf8.cpp
```

---

## 1. 现状（源码证据）

| 能力 | 现状 | 证据 |
|---|---|---|
| 引擎相机类 | **没有**。`src/graphics/` 无相机文件，`GN.h` 的 3D 头清单里也没有相机头 | `src/core/GN.h:143-158`；`ls src/graphics | grep -i camera` 为空 |
| 查看器相机 | 两个全局量 `gCameraPosition`（`sVEC3`）+ `gCameraRotation`（`sQUAT`），注释明确「只有一套实现……没有轨道/欧拉角分支」 | `tests/mmdViewer/mmdViewer.cpp:34-39, 51-52` |
| 自由飞行 | W/S 沿视线、A/D 沿右向、Q/E 世界竖直、Shift×3；速度 = `max(1, 模型高) × 0.9 m/s` | 同文件 `884-919` |
| 转视角 | 鼠标拖拽：水平绕世界 +Y、垂直绕相机右轴，左乘四元数并归一化；俯仰硬限 ±1.45 rad（≈83°） | 同文件 `176-205`、`232-256` |
| 初始视角 | 模型高度 ×1.9，位于 -Z 侧，yaw=π，俯角 0.12 rad；注释说明 MMD 模型面向 -Z（统一坐标系 v2 的迁移目标：资产正面 +Z、相机在 +Z 侧，见 `AXIS_UNIFICATION_PLAN` §6 U5） | 同文件 `206-222` |
| 渲染接线 | 查看器自建 `Mat44LookAtRH` + `Mat44PerspectiveVK`，`Mat44Mul(vp, proj, view)` 后传 `GN_VkBegin3DPass` | 同文件 `817-828`；`GN_Vk.h:400` |
| 数学地基 | 矩阵乘法/求逆/TRS 分解、四元数（轴角/欧拉/矩阵/slerp/旋转向量）、透视/正交/LookAt 齐全 | `GN_3Dmath.h:201-239` |
| 场景对象 | `cMODEL` 有 `aPosition/aRotation/aScale/aTransform/aWorldAABB`；**没有** `cOBJ3D` 基类 | `GN_3Dmodel.h:73-83` |
| 构建 | `src/graphics/*.cpp` 与 `tests/unit/*.cpp` 都是通配自动纳入，新增文件无需改构建清单 | `Makefile:170-188`、`228-238` |
| 测试基线 | `make unit` = **275 cases**；`make gpuCheck` = **95/95** | `docs/CHANGELOG.md:3` |
| GPU 环境 | DSH 沙箱无 `/dev/dri`，GPU 验收只能在桌面执行 | `ls /dev/dri`（不存在） |

**差距清单**

1. 相机逻辑全部散落在查看器，引擎层不可复用，也无法做纯 CPU 单测。
2. 没有「模式」概念：没有轨道、没有目标跟随、没有平滑、没有自动取景。
3. 没有正交投影，也没有 FOV 参考方向换算（横向/纵向/对角）。
4. 视图/投影矩阵由每个消费者自己拼，新增消费者就得复制一遍。

---

## 2. 参考库契约（只列与「移动 / 取景」相关的部分）

### 2.1 类型关系

`cCAMERA : public GT_sFOCUS_TARGET, public cOBJ3D`（`GN_3D.h:725`）

- `GT_sFOCUS_TARGET`（`GN_3D.h:104-113`）：`zTargetOBJ`（自动对准的目标）、`zAutoBalanced`（自动平衡于地平线）、`zAutoCorrectsRot`（自动校正自身旋转）、`GetTargetOBJ()`。
- `cOBJ3D`（`GN_3D.h:342-497`）：世界矩阵（`aMatWorld`）、跟随（`zFollowOBJ/zF_Ratio`）、动画关键点（`zAm`）、移动原语（§2.3）。
- 视口 `cVIEW3D`（`GN_3D.h:19-51`）持有 `P<cCAMERA> Camera`（内置默认）与 `CameraCurOn`（当前启用），并提供 `SetCameraFov / SetCameraOrthoH / SetCameraNearFar`。

### 2.2 `cCAMERA` 操作面（`GN_3D.h:725-794`）

| GN 方法 | 语义 | 本规划 |
|---|---|---|
| `InitToData(name, p_this, ts)` | 初始化（名字 / 安全指针 / 初始变换） | 纳入：`Init(desc)`（无 `P_`，用值语义） |
| `SetTarget(target, f_ratio=1, auto_balanced=1, auto_Corrects_rot=0)` | 自动对准目标；`P_()` 取消，变回自由镜头 | 纳入：`SetTarget(snapshot, ...)` |
| `SetAmCma(keys, optimize)` | 镜头 FOV 动画关键点 | **延后**（C6，与移动解耦） |
| `On(view)` / `Off()` | 绑定/解绑视口相机 | **不纳入**：GnVk 无 `cVIEW3D`，由调用方取矩阵；等价于「当前启用的相机」由调用方持有 |
| `ProjectionModeChange()` | 透视↔正交 | 纳入：`SetFov`/`SetOrthoH` 的副作用 |
| `SetDofDefocus / SetDofFocusDist / GetDof*` | 景深参数 | **延后**（无 DOF 渲染） |
| `SetFov / SetOrthoH / SetNear / SetFar / SetNF` + `Get*` | 镜头参数 | 纳入 |
| `IsOrthoMode()` | 正交判定 | 纳入 |
| `GetRelationView()` | 关联视口 | 不纳入（无视口对象） |
| `FovToFocalLen / FocalLenToFov` | 视角↔焦距（mm） | **延后**（传感器幅面未定，§2.6） |
| `FovToOrthoH / FovConvert` | 正交高度换算 / 视角参考方向换算 | 纳入 |
| `zUpdateCma()` | 重算观察矩阵（框架自动调用） | 纳入：`Update(dt)` + `BuildViewProj(aspect, ...)` |
| `aMatWorldCma` | 自动对焦处理后的最终镜头世界矩阵 | 纳入：`BuildViewProj` 内部使用，不对外暴露额外状态 |

### 2.3 `cOBJ3D` 移动原语（`GN_3D.h:450-491`，跟随在 `:411`）

| GN 方法 | 语义 | 本规划 |
|---|---|---|
| `SelfMove(front, left, up)` | 按**自身**轴移动（前 / 左 / 上） | 纳入 |
| `SelfRotLR(angle)` | 按自身左右方向旋转（偏航） | 纳入 |
| `SelfRotTB(angle)` | 按自身上下方向旋转（俯仰） | 纳入 |
| `SelfRotCW(angle)` | 按自身前后方向旋转（滚转） | 纳入（自由模式用） |
| `SelfRotAxis / SelfRotAxisNz` | 按任意轴（原始 / 已归一化）旋转 | 纳入 `SelfRotAxis`（内部归一化，不再分两版） |
| `SelfScale` | 自身缩放 | 不纳入（相机不缩放） |
| `SelfMatrix / SelfPos` | 自身矩阵 / 位置引用 | 不纳入（改为值语义 `Get/Set`，避免外部持有内部引用） |
| `ToMove(x, y, z)` | 世界坐标位移 | 纳入 |
| `SetWorldPos(ts)` / `SetWorldMatrix(m)` | 手动绝对世界变换（调用后不再受父级/跟随/动画影响） | 纳入 `SetWorldPos(pos)`；矩阵版不纳入（相机无父级/动画） |
| `SetFollowOBJ(obj, f_ratio)` | 跟随目标（GN 注释：摄像机 `0.1~0.05` 效果较好） | 纳入：`SetTarget` 的跟随部分（§4.4） |
| `GetWorldPos / GetWorldMatrix / GetLocalMatrix / GetCoordV / GetCoordXYZ` | 读回世界/局部变换 | 纳入 `GetWorldPos / GetCoordV`；局部矩阵不需要 |
| `ToRectMouseCtrl(...)` | 鼠标区划内控制移动/旋转/缩放（编辑器用途） | 不纳入（查看器用自己的输入映射） |

样例实证（`Sample/3d/*.utf8.cpp`）：

- `sample01:14-16`：`SetCameraLookRangeV(View.CameraCurOn, rng, 1.0f)` → `SelfRotTB(-15.0f)` → `SelfMove(12.0f, 0.0f, -9.0f)`；说明「自动取景 + 原语微调」是官方推荐用法。
- `sample04:97-99`：`View.Camera->SetFollowOBJ(Mod)` → `SelfMove(-1.06f, 0.0f, 0.1f)`；注释写明「这两行相当于设置一个第三人称视觉」。
- `sample03:22,42-46,51-53`：`SetTarget(Mod)` 自动对准；`SetOrthoH(7.0f)` 切正交；手动 `SetWorldMatrix` 后不再受跟随影响。

### 2.4 自动取景 / 观看（`GN_3D.h:285-294`、`920-942`）

| 名字 | 语义 |
|---|---|
| `eCMA_LOOK_MODE` | `TOP / FRONT / LEFT / BOTTOM / BACK / RIGHT / DOWN / SWAP` 八种观看方位 |
| `SetCameraLookDirection(camera, mode)` | 把相机放到坐标原点并按该方位看 |
| `SetCameraLookRange(camera, l, w, h, m=1, d=0, mode)` | 对准原点处范围；`m`=填满镜头区的参考长度，`d`=最大调整距离；正交时同步改高度截取范围 |
| `SetCameraLookObj(camera, obj, obj_to_origin, m, d, mode)` | 同上，范围取对象尺寸；`obj_to_origin` 把对象移到原点中心 |
| `AdjustFocusPosCameraLookRange(focus, camera, rect3, near_ratio)` | 按当前方向调整焦点/相机位置来观察范围 |
| `SetCameraLookRangeV` / `SetCameraLookObjV` | 上面两个的便捷形式（查看器最常用） |
| `GetCameraLookFocus` | 反算观察某物体所需的焦点/距离 |

### 2.5 本规划边界

- **动画**：`sAM_CAMERA_KEYS`（`GN_3DAmKey.h:319-359`）只有 FOV 关键点（`fov_keys`、`TimeGetFov`）；相机位置/旋转动画走 `cOBJ3D::zAm`（`sAM_TRANSF_KEYS`）。两者都与「移动」正交，延后到 C6。
- **景深**：只有参数与 `sRENDER_HINT::dofLv`，本仓库 3D 管线无 DOF，延后。
- **视口**：`cVIEW3D` 的屏幕映射、鼠标射线、多视口不在本规划；本规划只产出 `view / proj / viewProj` 三个矩阵。

### 2.6 头文件没写死的语义（必须显式定义）

| 项 | 参考库信息 | 本规划的定义（**deviation**，需用户拍板） |
|---|---|---|
| 坐标系/手性 | 头文件未声明；样例注释只提到 `Z:上 Y:左 X:前`（`sample07:64`） | 本仓库沿用查看器已验收的约定：**+Y 上、RH `Mat44LookAtRH`、Vulkan 深度 0..1**（`mmdViewer.cpp:817-828`）。GN 的轴约定不移植 |
| `f_ratio` | 注释「跟随紧密度 0..1；摄像机 0.1~0.05 效果较好」，**无公式** | 定义 `smoothTime = clamp((1 - ratio) × 0.25, 0.02, 0.5)` 秒（ratio=1 近瞬时；ratio=0.1 → 0.225 s）。单测锁定 |
| `autoBalanced` | 未写实现 | 是 = 视线朝向由「世界 +Y 为上」重建（无 roll）；否 = 保留累积旋转 |
| `autoCorrectsRot` | 未写实现 | 是 = 每帧直接重建朝向；否 = 朝向按同 `smoothTime` 做 slerp |
| `FRONT` 轴向 | 未给向量 | 定义 FRONT = 相机位于焦点 **+Z 侧、朝 -Z 看**（资产正面 +Z，与 `AXIS_UNIFICATION_PLAN` v2 一致）；其余方位见 §4.5 |
| `SelfRotLR/TB` 符号 | 样例 `SelfRotLR(-2)` 对应「左转」、`SelfRotTB(-15)` 对应「抬头」（`sample03:76-79`、`sample01:15`） | 本仓库采用查看器已验收的符号：**正角 = 右转 / 抬头**（`mmdViewer.cpp:180-205`）。与 GN 样例的符号差异是坐标系差异，按本仓库约定实现并单测 |
| `FovToOrthoH` / `FovConvert` | 只给签名，未给公式 | 按标准透视几何定义（§4.5），并可互逆 |
| 正交切换 | `SetOrthoH(0)` 回透视；`SetFov` 也回透视（`GN_3D.h:768` 注释） | 同语义 |

---

## 3. 目标架构

### 3.1 设计原则

1. **纯 CPU**：相机不读 `GN::Key`/`GN::Mouse`、不碰 Vulkan；输入由控制层翻译（可单测）。
2. **不移植 `cOBJ3D` 继承体系**：本仓库没有 3D 对象基类，强行移植会把相机、模型、场景耦合在一起；相机只持有自己的变换状态。
3. **目标是快照，不是指针**：调用方每帧填 `sCAMERA_TARGET`（世界位置 + 世界 AABB）。避免悬垂指针、避免相机依赖场景/模型生命周期。
4. **命名对齐 GN 外部契约**：`cCAMERA`、`SelfMove`、`SetFov`、`SetTarget`、`eCAMERA_*` 保持 GN 名字（这是引擎对外 API 的一部分）；内部辅助函数用 camelCase。文件沿用项目既有 3D 模块命名 `GN_3Dcamera.*`（同 `GN_3Dcloth.h`、`GN_3Dlod.h`；`lintCodeModularity` 的命名检查只覆盖 py/sh/js/ts/go/rs 等扩展名，C++ 不参与，保持项目内一致优先）。
5. **渲染接口零改动**：`GN_VkBegin3DPass` / `GN_VkDraw*` / `cMODEL` 一个不改；只换调用方如何算矩阵。

### 3.2 GN → 本规划映射

| GN（SDK 1e） | 本规划 | 落点 | 差异 |
|---|---|---|---|
| `cCAMERA`（继承 `cOBJ3D`） | `GN3d::cCAMERA`（独立类） | `GN_3Dcamera.h/.cpp` | 无父级/动画/物理，矩阵由自身状态推导 |
| `GT_sFOCUS_TARGET` | `sCAMERA_TARGET`（快照） | 同头文件 | 不持有对象指针 |
| `SelfMove/SelfRot*/ToMove` | 同名方法 | `GN_3DcameraMove.cpp` | 值语义、弧度制 |
| `SetWorldPos/GetWorldPos/GetCoordV` | 同名方法（`sVEC3`） | 同上 | 不暴露矩阵引用 |
| `SetFollowOBJ` + `SetTarget` | 统一到 `SetTarget(target, ratio, balanced, correctsRot)` | `GN_3DcameraFollow.cpp` | 位置跟随 + 朝向对准可同时设置 |
| `SetCameraLookDirection/Range/Obj` | 同名自由函数 | `GN_3DcameraFit.cpp` | 接受 `sVEC3`/`sCAMERA_TARGET`，不接 `P<cOBJ3D>` |
| `SetFov/SetOrthoH/SetNF/FovToOrthoH/FovConvert` | 同名 | `GN_3Dcamera.cpp` / `GN_3DcameraFit.cpp` | 返回 `bool` 以便校验失败可测（GN 为 `void`） |
| `cVIEW3D::SetCamera*` | 不实现 | — | 调用方直接调相机；矩阵按需构建 |
| `sAM_CAMERA_KEYS` | 不实现（C6） | — | 与移动解耦 |
| `ToRectMouseCtrl` | 不实现 | — | 查看器输入层自己映射 |

### 3.3 文件与职责（单文件 < 500 有效行）

| 文件 | 职责（单一） | 预计有效行 |
|---|---|---|
| `src/graphics/GN_3Dcamera.h` | 契约：枚举、`sCAMERA_DESC`、`sCAMERA_TARGET`、`sCAMERA_INPUT`、`cCAMERA` 声明、取景函数声明 | ~130 |
| `src/graphics/GN_3Dcamera.cpp` | 生命周期、镜头参数校验、状态读写、`Forward/Right/Up`、`BuildViewProj` | ~240 |
| `src/graphics/GN_3DcameraMove.cpp` | `SelfMove / SelfRot* / ToMove / SetWorldPos / GetWorldPos / 朝向构造` | ~170 |
| `src/graphics/GN_3DcameraOrbit.cpp` | 轨道状态、旋转/缩放/平移、模式切换（自由↔轨道↔跟随） | ~190 |
| `src/graphics/GN_3DcameraFollow.cpp` | `SetTarget/ClearTarget`、临界阻尼平滑、自动对准、`Update(dt)` | ~210 |
| `src/graphics/GN_3DcameraFit.cpp` | 观看方位、`SetCameraLookDirection/Range/Obj`、`FovToOrthoH/FovConvert` | ~230 |
| `src/graphics/GN_3DcameraControl.cpp` | `sCAMERA_INPUT` → 移动/转视角（自由飞行 + 轨道），不读全局输入 | ~150 |
| `tests/unit/testCamera.cpp` | 22 条 CPU 用例（§5.2） | ~430 |
| 改动：`src/core/GN.h` | +1 行 include | +1 |
| 改动：`tests/mmdViewer/mmdViewer.cpp` | 删除散落相机段，改调 `cCAMERA` + 控制层（净减 ~60 行） | ~-60 |

新增约 **1750 行**（含测试），每个 .cpp 单功能、有效行 < 500。`GN_3Dcamera.cpp` 只放「状态 + 矩阵」，移动/轨道/跟随/取景各自独立文件，避免长成 `GN_Physics.cpp` 那种 498 行的贴顶文件。

### 3.4 数据结构契约草案

```cpp
namespace GN3d {

enum eCAMERA_MODE { eCAMERA_MODE_FREE, eCAMERA_MODE_ORBIT, eCAMERA_MODE_FOLLOW };

// 对齐 GN 的 eCMA_LOOK_MODE 八值
enum eCAMERA_LOOK_MODE {
    eCAMERA_LOOK_TOP, eCAMERA_LOOK_FRONT, eCAMERA_LOOK_LEFT, eCAMERA_LOOK_BOTTOM,
    eCAMERA_LOOK_BACK, eCAMERA_LOOK_RIGHT, eCAMERA_LOOK_DOWN, eCAMERA_LOOK_SWAP
};

// 对齐 GN 的 eCAMERA_FOV_TYPE
enum eCAMERA_FOV_TYPE { eCAMERA_FOV_X = 1, eCAMERA_FOV_Y, eCAMERA_FOV_XY };

// 目标快照：调用方每帧刷新；相机不持有对象指针
struct sCAMERA_TARGET {
    bool aValid;            // false = 无目标
    GNg::sVEC3 aPosition;   // 世界位置（跟随/对准中心）
    GNg::sAABB aBounds;     // 世界 AABB（自动取景用；空盒按点处理）
};

struct sCAMERA_DESC {
    GNg::sVEC3 aPosition;
    GNg::sQUAT aRotation;
    float aFovYDeg;         // 默认 45
    float aNear;            // 默认 0.1
    float aFar;             // 默认 1000
    float aOrthoH;          // 0 = 透视
    eCAMERA_MODE aMode;     // 默认 FREE
    float aFollowRatio;     // 0..1，默认 0.15
};

// 控制层输入（与 GN::Key / GN::Mouse 解耦）
struct sCAMERA_INPUT {
    float aMoveFront, aMoveLeft, aMoveUp;  // -1..1
    float aLookYaw, aLookPitch;            // 弧度增量
    float aZoom;                           // 滚轮增量（+ 拉近）
    float aPanRight, aPanUp;               // 轨道平移
    bool  aBoost;                          // Shift
};
```

> `sAABB` 来自 `GN_Spatial.h`（CPU 结构，已在 `GN_3Dmodel.h:10` 引入）。

### 3.5 公共 API 草案

```cpp
class cCAMERA {
public:
    // 状态（沿用 GN 的 a 前缀用户属性风格）
    GNg::sVEC3 aPosition;
    GNg::sQUAT aRotation;
    float      aFovY;      // 角度
    float      aNear, aFar;
    float      aOrthoH;    // 0 = 透视
    eCAMERA_MODE aMode;

    cCAMERA();
    void Init(const sCAMERA_DESC *desc = 0);
    void Reset();                                  // 回默认透视 + 自由

    // 镜头（返回 false = 参数非法，状态不变）
    bool SetFov(float deg);                        // 同时退出正交（GN 语义）
    bool SetOrthoH(float h = 0.0f);                // 0 = 回透视
    bool SetNear(float n);
    bool SetFar(float f);
    bool SetNF(float n, float f);
    bool IsOrthoMode() const;

    // GN 移动原语
    void SelfMove(float front, float left, float up);
    void SelfRotLR(float angleRad);                // 正 = 右转（§2.6）
    void SelfRotTB(float angleRad);                // 正 = 抬头（§2.6）
    void SelfRotCW(float angleRad);
    void SelfRotAxis(float angleRad, const GNg::sVEC3 *axis);
    void ToMove(float x, float y, float z);
    void SetWorldPos(const GNg::sVEC3 *pos);
    void GetWorldPos(GNg::sVEC3 *out) const;
    GNg::sVEC3 GetCoordV() const;
    GNg::sVEC3 Forward() const;
    GNg::sVEC3 Right() const;
    GNg::sVEC3 Up() const;

    // 轨道
    void SetOrbit(const GNg::sVEC3 &focus, float distance, float yawRad, float pitchRad);
    void OrbitRotate(float yawDelta, float pitchDelta);
    void OrbitZoom(float delta);
    void OrbitPan(float right, float up);

    // 目标/跟随
    void SetTarget(const sCAMERA_TARGET &target, float followRatio = -1.0f,
                   bool autoBalanced = true, bool autoCorrectsRot = false);
    void ClearTarget();
    bool HasTarget() const;

    // 控制层推进（不读全局输入）
    void ApplyInput(const sCAMERA_INPUT &input, float dt, float moveSpeed, float lookSpeed);
    void Update(float dt);                         // 轨道解算 / 跟随平滑 / 数值钳制

    // 视图（aspect 由调用方给；不改渲染接口）
    bool BuildViewProj(float aspect,
                       GNg::sMAT44 *viewOut, GNg::sMAT44 *projOut, GNg::sMAT44 *viewProjOut) const;

    // GN 静态换算
    static float FovToOrthoH(float fovDeg, float targetDistance);
    static float FovConvert(eCAMERA_FOV_TYPE dst, eCAMERA_FOV_TYPE src, float fovDeg, float whRatio);
};

// 自动取景（对齐 GN 的自由函数名）
void  SetCameraLookDirection(cCAMERA &camera, eCAMERA_LOOK_MODE mode,
                             const GNg::sVEC3 *focus = 0);
float SetCameraLookRange(cCAMERA &camera, const GNg::sVEC3 &center,
                         const GNg::sVEC3 &halfExtent, float margin = 1.0f,
                         eCAMERA_LOOK_MODE mode = eCAMERA_LOOK_FRONT);
float SetCameraLookObj(cCAMERA &camera, const sCAMERA_TARGET &target,
                       float margin = 1.0f,
                       eCAMERA_LOOK_MODE mode = eCAMERA_LOOK_FRONT);

} // namespace GN3d
```

### 3.6 不变量

- `aRotation` 始终单位化；`Forward/Right/Up` 由四元数旋转局部基得到（与 `mmdViewer.cpp:137-145` 同法）。
- `aNear < aFar`、`0 < aFovY < 180`、`aOrthoH >= 0`；非法 setter 返回 `false` 且不改状态。
- 俯仰限制 ±83°（与查看器 ±1.45 rad 对齐），越界丢弃该帧垂直分量。
- `Update(dt)` 把 dt 钳制到 `[0, 0.1]`；输入含 NaN/Inf 时直接返回，不污染状态。
- 轨道：`distance ∈ [0.05, 1e5]`；`position` 与 `focus + offset` 始终一致（单一事实来源是 focus+角度+距离）。
- 跟随：目标快照 `aValid=false` 时等价于无目标，不产生平滑残影。

---

## 4. 移动与平滑算法

### 4.1 朝向

四元数累积，与查看器现有实现一致：水平绕世界 +Y（`QuatFromAxisAngle`，右转取负角）、垂直绕**当前相机右轴**（正角抬头），左乘后归一化（`mmdViewer.cpp:180-205`）。俯仰用 `asin(-forward.y)` 计算当前角并夹到 ±83°，越界则丢弃垂直分量。不使用欧拉角累加，避免万向锁与 roll 漂移。

### 4.2 自由飞行

- `SelfMove(front, left, up)`：`pos += Front*front + Right*(-left) + Up*up`（GN 参数是「前 / 左 / 上」）。
- 输入映射（`GN_3DcameraControl.cpp`，查看器只负责填 `sCAMERA_INPUT`）：
  - `W/S → aMoveFront ±1`，`A/D → aMoveLeft +1/-1`，`Q/E → aMoveUp -1/+1`；
  - 速度 = `max(1, 参考高度) × 0.9 m/s`（与查看器 `892` 的观感一致），`Shift` ×3；
  - 位移 = 方向 × 速度 × dt；转视角 = 鼠标像素 ×0.005 rad 或键盘 1.6 rad/s（同查看器 `36-39`）。
- `ApplyInput` 只调 `SelfMove/SelfRot*`，不直接改 `aPosition`，保证原语被单测覆盖。

### 4.3 轨道

- 状态：`aFocus`、`aDistance`、`aYaw`、`aPitch`（与 `aPosition/aRotation` 同步求解）。
- 位置：`offset = (sin(yaw)·cos(pitch), sin(pitch), cos(yaw)·cos(pitch)) × distance`；`position = focus + offset`；朝向 = 朝 `focus` 的 look rotation。
- 默认 `FRONT`：`yaw = 0`、`pitch = 0.12`、`distance = 参考高度 × 1.9`（统一坐标系 v2：相机在 **+Z 侧**、资产正面 +Z；旧查看器在 -Z 侧，迁移见 `AXIS_UNIFICATION_PLAN` §6 U5）。
- `OrbitRotate(dyaw, dpitch)`：角度累加，pitch 夹到 ±83°；`OrbitZoom(delta)`：`distance *= exp(-delta × 0.15)`，夹到 `[0.05, 1e5]`；`OrbitPan(right, up)`：`focus += Right×right + Up×up` 后重解位置。
- 模式切换：
  - **进轨道**：`focus = position + Forward × distance`（distance 取当前到目标/参考点的距离，缺省 `1.9×高度`），再由 `position - focus` 反算 yaw/pitch。
  - **出轨道（回自由）**：保留当前 `position/aRotation`，清空轨道参数。

### 4.4 目标跟随与自动对准

- `SetTarget(snapshot, ratio, balanced, correctsRot)`：保存快照与 `ratio`；捕获 `offset = position - target.aPosition`（保持当前相对位置）；设置跟随标志。
- `Update(dt)`：
  1. 对 `target.aPosition` 做**临界阻尼平滑**（逐轴；Thomas Lowe, *Critically Damped Ease-In/Ease-Out Smoothing*, Game Programming Gems 4, pp.95-101）：
     ```
     omega = 2 / smoothTime
     x     = omega * dt
     exp   = 1 / (1 + x + 0.48x² + 0.235x³)
     change = current - target
     temp   = (velocity + omega * change) * dt
     velocity = (velocity - omega * temp) * exp
     current  = target + (change + temp) * exp
     ```
  2. `position = smoothedTarget + offset`。
  3. 朝向：看向 `smoothedTarget`；`autoBalanced=true` 时用世界 +Y 重建（无 roll）；`autoCorrectsRot=false` 时对朝向做同 `smoothTime` 的 slerp，`true` 时直接重建。
  4. 收敛判据：`|position-target| < 1e-4` 且 `|velocity| < 1e-4` → 速度清零（防尾抖）。
- `smoothTime` 映射：`clamp((1 - ratio) × 0.25, 0.02, 0.5)` 秒（§2.6，待拍板）。
- 模式与目标的组合：`FOLLOW` 模式跟随目标并保持 `offset`；`ORBIT` 模式下 `SetTarget` 只把 `aFocus` 绑到目标位置（是否平滑同样由 `ratio` 决定），轨道角度/距离不变；`FREE` 模式下目标只被记录、不驱动相机。三种组合都走同一个 `Update`，单测覆盖。
- `Update` 不读输入；跟随与自由输入可叠加（例如跟随中仍可 `SelfMove` 调偏移，等价 GN 的 `SetFollowOBJ` + `SelfMove` 用法）。

### 4.5 自动取景与 FOV 换算

- `FovToOrthoH(fovDeg, d) = 2 · d · tan(fovDeg/2)`。
- 横向视角：`tan(fovX/2) = tan(fovY/2) × aspect`。
- `FovConvert`：`tanX = tanY × aspect`；`tanDiag = sqrt(tanX² + tanY²)`；按 `dst` 返回 `2·atan(tan)`（角度制）。
- `SetCameraLookRange(camera, center, halfExtent, margin, mode)`：
  1. 先按 `mode` 定相机方位（`SetCameraLookDirection` 的方向表，**方向 = 从相机指向 center**）：`FRONT=(0,0,-1)`（相机在 +Z 侧）、`BACK=(0,0,+1)`、`LEFT=(+1,0,0)`（相机在 -X 侧）、`RIGHT=(-1,0,0)`、`TOP=(0,-1,0)`、`BOTTOM=(0,+1,0)`；`DOWN` = TOP 且 up=+Z、`SWAP` = FRONT 且 up=-Y；
  2. 所需距离 `dY = (hy) / tan(fovY/2)`、`dX = (hx) / tan(fovX/2)`，`d = max(dX, dY) × margin + 沿视线的半深度`；
  3. 相机位置 = `center - dir × d`；正交模式下同步 `aOrthoH = 2·d·tan(fovY/2)`。
  GN 的 `d` 参数是「最大调整距离」，v1 省略（不做上限）；这是与 GN 签名的差异，记入 §6。
- `SetCameraLookObj`：用 `target.aBounds` 的半径当 `halfExtent`；空盒按点处理（`d = 1`）。
- `FovToFocalLen/FocalLenToFov` **不实现**：需要胶片幅面（36 mm 全幅？）与画幅比假设，GN 头文件未声明，留给用户拍板（§6）。

### 4.6 视图与投影

- View：`Mat44LookAtRH(pos, pos + Forward, Up)`（`GN_3Dmath.h:221`），与 `mmdViewer.cpp:817-819` 同法；自由模式允许 roll，跟随/轨道模式无 roll。
- Perspective：`Mat44PerspectiveVK(fovY, aspect, near, far)`（`GN_3Dmath.h:217`）。
- Ortho：`Mat44OrthoVK(-halfW, halfW, -halfH, halfH, near, far)`，`halfH = aOrthoH/2`，`halfW = halfH × aspect`（`GN_3Dmath.h:219`）。
- `viewProj = proj × view`（`Mat44Mul(&vp, &proj, &view)`，与 `mmdViewer.cpp:828` 同序）。
- **相机不内置场景知识**：云层所需的远平面下限仍由调用方算（`mmdViewer.cpp:820-827`），相机只接受 `SetNF`。

### 4.7 数值安全

- dt：`if (!(dt > 0)) return;` 后钳到 `0.1`。
- 输入：`isfinite` 检查（`std::isfinite`，C++11）；任一分量非有限则整帧忽略。
- 归一化：所有轴/四元数在写入前归一化；零长度轴返回 false。
- 退化：`focus == position` 时轨道反算失败 → 保持上一帧朝向，不产生 NaN。
- 投影：`aspect <= 0` 时 `BuildViewProj` 返回 false，调用方沿用旧矩阵。

### 4.8 遮挡碰撞（v2，不在本规划实现）

- 方案：从 `focus` 向期望相机位置做射线（复用 `GN_Physics.h:219` 的 `Raycast`），命中则把相机拉到命中点前 0.2 m；无物理世界时跳过。
- 参考：Unity Cinemachine Collider / Unreal SpringArm。因引入场景/物理依赖，列入 C6，不进 v1。

---

## 5. 分阶段落地与验收

### 5.1 阶段表

| 阶段 | 内容 | 文件 | 验收命令 | 通过判据 | 失败怎么办 |
|---|---|---|---|---|---|
| **C0** 契约 | ✅ 2026-10-30：头文件 + 实现骨架 + `GN.h` include + `testCamera` | `GN_3Dcamera.h`、`GN_3Dcamera*.cpp`、`GN.h`、`testCamera.cpp` | `make unit` | 通过（实测见下） | — |
| **C1** 核心 | ✅ 2026-10-30：状态/镜头参数校验/`BuildViewProj`（透视+正交）/`Forward/Right/Up` + 6 用例 | `GN_3Dcamera.cpp` | `make unit` | 6/6 PASS | — |
| **C2** 移动 | ✅ 2026-10-30：`SelfMove/SelfRot*/ToMove/SetWorldPos` + `sCAMERA_INPUT` 控制层 + 5 用例 | `GN_3DcameraMove.cpp`、`GN_3DcameraControl.cpp` | `make unit` | 5/5 PASS；方向/角度与查看器符号一致 | — |
| **C3** 轨道/跟随 | ✅ 2026-10-30：轨道状态机、跟随平滑、自动对准（含地平线平衡）、`Update(dt)` + 7 用例 | `GN_3DcameraOrbit.cpp`、`GN_3DcameraFollow.cpp` | `make unit` | 7/7 PASS；收敛/无过冲/dt=0/非法目标/无 roll | — |
| **C4** 取景 | ✅ 2026-10-30：`SetCameraLookDirection/Range/Obj`、`FovToOrthoH/FovConvert` + 4 用例 | `GN_3DcameraFit.cpp` | `make unit` | 4/4 PASS；互逆/拟合断言 | — |
| **C5** 接线 | ✅ 2026-10-30（编译级）：查看器相机段已换成 `GN3d::cCAMERA`（唯一实例 `gCamera`），输入只填 `sCAMERA_INPUT`→`ApplyInput`，视图/投影由 `BuildViewProj` 产出；新增 F 自由↔轨道、轨道滚轮缩放、中键平移；R 重置仍自由模式且默认视角与旧实现等价 | `mmdViewerCamera.cpp`、`mmdViewerShared.h`、`mmdViewer.cpp` | `make unit`、`make mmdViewer`、桌面 `make gpuCheck` | 编译通过 + 单测 371/0 + lint 0 违规；**运行期手感待桌面** | 任一回归 → 只回退 C5（相机文件保留） |

> **C0–C4 实测（2026-10-30；2026-10-31 更新）**：22 个 `TEST_CASE` 全 PASS；套件总量随其它模块增长，当前 `make unit` = **371 cases / 0 failed**
> （= 292 基线 + 本规划 22 + 并行 GI CPU 16）；`make` 构建 exit 0；`lintCodeModularity src/graphics tests/unit --naming --comments`
> 对新增 8 文件 0 违规（全仓仅存量 `GN_2Dmath.cpp` 超长违规与 spv 头 no-split 告警）。
> **运行期未验证**（本机无 `/dev/dri`）；另发现既有 `GN_3Dmath.cpp:381` `Mat44OrthoVK` 的 `m[10] = -fn` 与 VK [0,1] 深度符号不符
> （near→-2e-4、far→-1.0002），正交相机深度不可用；属既有数学库缺陷，未在本规划内修改（见风险）。

> 用例数是**目标值**；实测：camera 22 用例全 PASS，套件总量 371/0（2026-10-31）。C0–C5 全部代码级完成。

### 5.2 `testCamera` 用例清单（22 条）

| # | 用例 | 断言要点 |
|---|---|---|
| 1 | `camera.initDefaults` | FOV=45、near=0.1、far=1000、正交=0、mode=FREE、四元数单位化 |
| 2 | `camera.lensValidate` | `SetFov(0/-1/200)` 返回 false 且状态不变；`SetNF(10,1)` 拒绝；`SetOrthoH(7)` 切正交，`SetFov(45)` 回透视 |
| 3 | `camera.buildViewProjCenter` | 相机在 (0,0,5) 看原点，原点投影到 NDC 中心（x≈0,y≈0，z∈(0,1)） |
| 4 | `camera.buildViewProjDepth` | 近/远平面点深度落在 VK 区间两端（容差 1e-4） |
| 5 | `camera.buildOrtho` | `aOrthoH=10`、aspect=2 → 右侧点 x_ndc 与半宽关系正确 |
| 6 | `camera.fovToOrthoH` | `FovToOrthoH(90, 5) ≈ 10`；往返一致 |
| 7 | `camera.selfMoveLocal` | 默认朝向（-Z）下 `SelfMove(1,0,0)` → z-1；`SelfMove(0,1,0)` → x-1；`SelfMove(0,0,1)` → y+1 |
| 8 | `camera.selfRotYawPitch` | `SelfRotLR(+90°)` 后前向指向 +X 侧；`SelfRotTB(+30°)` 前向 y>0 |
| 9 | `camera.pitchClamp` | 连续抬头 10 次，俯仰 ≤83° 且四元数仍单位化 |
| 10 | `camera.toMoveWorld` | 先转 90° 再 `ToMove(1,0,0)`，位移沿世界 +X（不受朝向影响） |
| 11 | `camera.selfRotAxis` | 绕任意轴旋转后正交基仍正交、行列式≈1 |
| 12 | `camera.orbitPosition` | `SetOrbit(焦点, 10, 0, 0)` → 位置 = 焦点 + (0,0,+10)，朝向 -Z |
| 13 | `camera.orbitZoomClamp` | 连续 zoom 后 distance ∈ [0.05,1e5]，且 focus 不变 |
| 14 | `camera.orbitPan` | pan 后 focus 沿相机 right/up 平移，distance 不变 |
| 15 | `camera.followConverge` | 目标从 0 瞬移到 (10,0,0)，1.5 s 内 |pos-target-offset| < 1e-3 |
| 16 | `camera.followNoOvershoot` | 收敛过程中距离单调不增（临界阻尼无过冲），末速度≈0 |
| 17 | `camera.followInvalidTarget` | `aValid=false` 后 `Update` 不改位置/朝向 |
| 18 | `camera.followAutoBalance` | 目标斜向移动后相机 up.y > 0.99（无 roll） |
| 19 | `camera.lookDirectionModes` | 8 个方位的前向/up 与定义表一致（点积断言） |
| 20 | `camera.lookRangeFit` | 半范围 (1,1,1)、fov=90、aspect=1、margin=1 → 距离≈2，投影后范围恰好填满 |
| 21 | `camera.lookObjFit` | 给 AABB 的 `sCAMERA_TARGET`，取景后四角投影落在 NDC [-1,1] 内 |
| 22 | `camera.fovConvert` | X↔Y 互逆（aspect=2）、XY 对角关系 `tanD²=tanX²+tanY²` |

### 5.3 查看器接线清单（C5，已完成 ✅）

| 位置 | 现状 | 改法 |
|---|---|---|
| `mmdViewer.cpp:51-52` | `gCameraPosition/gCameraRotation` 全局 | 换成 `GN3d::cCAMERA gCamera;` |
| `:132-176` | `cameraPosition/cameraBasisVectors/cameraTarget/cameraDistance/cameraBasis` | 改为 `gCamera.GetCoordV()/Forward()/Right()/Up()`；`cameraTarget()` = `GetCoordV() + Forward()`（保留同名薄封装，减少改动面） |
| `:180-205` | `applyLookDelta` | 改为 `gCamera.SelfRotLR/SelfRotTB` |
| `:206-222` | `resetCamera` | 改为 `gCamera.SetOrbit(...)`（默认 `yaw=0`，相机在 +Z 侧）或 `Init` + `SetTarget` |
| `:232-256` | `updateMouseLook` | 只负责把像素差填进 `sCAMERA_INPUT.aLookYaw/aLookPitch` |
| `:817-828` | 自建 lookAt + perspective + 乘法 | `gCamera.BuildViewProj(aspect, &view, &proj, &vp)`；near/far 仍由查看器 `SetNF` |
| `:884-944` | WASD/QE/方向键直接改位置 | 填 `sCAMERA_INPUT`，调 `gCamera.ApplyInput(input, dt, 速度, 1.6f)` |
| 新增按键 | — | `F` 切自由/轨道（不与天气 `1-6`、`G`、`R` 冲突）；轨道下滚轮缩放（`GN::Mouse1[GM_TOP/BOTTOM]`）、中键拖动平移（`GM_MIDDLE`） |
| 操作提示 | `:763` 的 `printf` | 补充 F/滚轮/中键说明 |

### 5.4 全量验收命令（C5，已完成 ✅）

```bash
make unit                       # camera 22 用例全 PASS；套件总量 371/0（2026-10-31）
make check                      # 构建 + 单测
make mmdViewer                  # 查看器编译（不要求 GPU）
make gpuCheck                   # 仅桌面 GPU；期望 95/95 PASS
bash ~/.dsh/skills/code-modularity/scripts/lintCodeModularity.sh \
  src/graphics tests/unit --naming --comments   # 新增文件 0 违规
```

手动清单（桌面）：迁移 U5 后默认正面视角在 +Z 侧（与资产正面 +Z 一致）；WASD/QE 自由飞行；鼠标拖拽与方向键转视角；俯仰到顶不翻转；F 进轨道后绕角色旋转、滚轮缩放、中键平移、再按 F 回自由且画面不跳变；R 重置；天气 1–6 与雨雪/云/雾无回归；G 坐标轴正常。

---

## 6. 风险与待拍板

| # | 风险 / 待决 | 影响 | 处置 |
|---|---|---|---|
| 1 | GN 头文件未给 `f_ratio` 公式、`autoBalanced/autoCorrectsRot` 实现、FovConvert 公式；FRONT 轴向已由 `AXIS_UNIFICATION_PLAN` v2 定为「资产正面 +Z、相机在 +Z 侧」 | 移植语义可能与原作者意图不同 | §2.6 显式定义并单测；**其余语义请用户确认**；后续只改映射常量，不动接口 |
| 2 | 本仓库是 +Y 上 RH，GN 样例是 Z 上 / 不同手性（`sample07:64`） | 直接照搬 GN 的旋转符号会导致上下/左右反向 | 只移植操作**语义**，符号按查看器已验收的行为（§2.6），不移植矩阵 |
| 3 | 查看器使用负高度视口，投影/剔除路径对矩阵变化敏感 | 相机改动可能让 `ground.placement` / `decal` 等 GPU 检查回归 | C5 必须跑桌面 `make gpuCheck` 95/95；任一回归只回退 C5 |
| 4 | 目标快照过期/悬垂 | 相机跟到旧位置或读越界 | 值语义 + `aValid` 标志；每帧刷新写进接线说明 |
| 5 | dt 尖峰 / 暂停 / 窗口拖动 | 跟随跳变、轨道飞出去 | dt 钳到 0.1；非有限输入整帧忽略（§4.7） |
| 6 | 模式互斥与切换跳变 | 自由↔轨道切换画面突变 | 切换时按 §4.3 反算状态，单测断言切换前后 `position/rotation` 连续 |
| 7 | 输入键位冲突 | 与天气 1–6/`G`/`R` 冲突 | `F` + 滚轮 + 中键均未被占用（已核对 `mmdViewer.cpp` 与 `GN_Input.h:259-260`） |
| 8 | 范围蔓延到相机动画/景深/拾取 | 工期与耦合失控 | 明确列入 §0.1「不做」；C6 另立规划 |
| 9 | 用例数与实测不符 | 验收判据形同虚设 | 目标值是下限，实测回填；只增不减 |
| 10 | 相机与 `cMODEL` 的依赖方向 | 相机反向依赖场景会破坏可测性 | 相机只认 `sCAMERA_TARGET`；`cMODEL → 快照` 的转换写在调用方 |
| 11 | `SetCameraLookRange/Obj` 省略 GN 的 `maxDistance` 参数 | 与原库签名不完全一致 | v1 明确不做上限（§4.5）；若后续需要，只加默认参数，不改现有调用 |

---

## 7. 参考锚点

### 7.1 参考库（本地取证，不入库）

| 锚点 | 内容 |
|---|---|
| `GN_3D.h:725` | `class GN_API cCAMERA : public GT_sFOCUS_TARGET, public cOBJ3D` |
| `GN_3D.h:104-113` | `GT_sFOCUS_TARGET`（目标/平衡/旋转校正） |
| `GN_3D.h:342-497` | `cOBJ3D`（世界矩阵、跟随、移动原语；类声明 342 行起） |
| `GN_3D.h:411` | `SetFollowOBJ(obj, f_ratio=1.0f)` |
| `GN_3D.h:450-491` | `SelfMove/SelfRot*/ToMove/SetWorldPos/GetWorldPos/GetCoordV` |
| `GN_3D.h:745-794` | `zUpdateCma/SetTarget/SetAmCma/SetFov/SetOrthoH/SetNear/SetFar/SetNF/FovToOrthoH/FovConvert` |
| `GN_3D.h:19-51` | `cVIEW3D` 相机槽与镜头控制 |
| `GN_3D.h:285-294` | `eCMA_LOOK_MODE` |
| `GN_3D.h:920-942` | 自动取景自由函数族 |
| `GN_3Dtype.h:146,153,182,207,213` | `eCAMERA_AT_TYPE`、`eCAMERA_FOV_TYPE`、`GN3d_sCAMERA_INFO`、`InitCmaDefault/InitCma` |
| `GN_3DAmKey.h:319-359` | `sAM_CAMERA_KEYS`（FOV 关键点，延后） |
| `Sample/3d/sample01.utf8.cpp:14-16` | 自动取景 + 原语微调 |
| `Sample/3d/sample03.utf8.cpp:22,42-46,51-53` | `SetTarget` / 正交切换 / 手动世界矩阵 |
| `Sample/3d/sample04.utf8.cpp:97-99` | 第三人称跟随（`SetFollowOBJ` + `SelfMove`） |
| `Sample/3d/sample07.utf8.cpp:64` | GN 默认轴 `Z:上 Y:左 X:前`（样例注释） |

### 7.2 本仓库

| 锚点 | 内容 |
|---|---|
| `src/core/GN.h:143-158` | 3D 头清单（无相机；C0 加一行） |
| `src/graphics/GN_3Dmath.h:201-239` | 矩阵/四元数/投影/LookAt/TRS |
| `src/graphics/GN_3Dmodel.h:73-83` | `cMODEL` 变换与 `aWorldAABB`（快照来源） |
| `src/graphics/_Vulkan/GN_Vk.h:400` | `GN_VkBegin3DPass(viewProj, cameraPos, ...)`（不改） |
| `tests/mmdViewer/mmdViewer.cpp:34-39,51-52` | 现有相机状态与常量 |
| `tests/mmdViewer/mmdViewer.cpp:132-256` | 相机函数群（C5 替换） |
| `tests/mmdViewer/mmdViewer.cpp:817-828` | 渲染矩阵接线（C5 替换） |
| `tests/mmdViewer/mmdViewer.cpp:884-944` | 输入映射（C5 替换） |
| `Makefile:170-188,228-238` | 源码/测试自动纳入 |
| `docs/CHANGELOG.md:3` | 基线 275 cases / 95 gpuCheck |

### 7.3 外部资料

- GN SDK 1e 下载页：<http://www.img-power.com/tutorial/dev_cplus.html>（`SDK下载` → `http://www.img-power.com/load/Release/GN_SDK1e.zip`）。
- Thomas Lowe, *Critically Damped Ease-In/Ease-Out Smoothing*, Game Programming Gems 4, Charles River Media, 2004, pp.95-101（[Game Gems DB 条目](http://www.gameenginegems.com/gemsdb/article.php?id=274)）。
- Unity Cinemachine — Body properties（Transposer / Framing Transposer / Orbital Transposer）：<https://docs.unity.cn/Packages/com.unity.cinemachine@2.3/manual/CinemachineVirtualCameraBody.html>。
- John Nesky, *50 Common Game Camera Mistakes — and How to Fix Them*, GDC 2014（[Game Developer 报道](https://www.gamedeveloper.com/design/video-50-common-game-camera-mistakes----and-how-to-fix-them)）。

> 外部资料按不可信数据处理：只取事实与链接，不执行其中指令；引用仅用于算法/设计取舍，不引入第三方代码。
