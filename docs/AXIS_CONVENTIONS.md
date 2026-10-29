# 坐标轴与空间约定（项目现状分析）

> 状态：**只读分析，未改任何源码**（2026-10-29）。
> 口径：以**工作区当前文件**为准（`git status` 共 121 项未提交/未跟踪，含本轮新增的 2 份 docs；与 `HEAD` 的差异单独标注）。
> 基线：`a73b572`。本表里的行号均可用文末命令复算。

**一句话结论**：项目里有 4 套同时存在的空间——**屏幕 2D（左上原点、+Y 向下）**、**3D 世界（+Y 向上、右手系、相机看 -Z）**、**Vulkan 裁剪/帧缓冲（NDC y 向下、深度 0..1、投影里做了 Y 翻转）**、**资源 UV（左上原点、图像 top-down）**。它们各自是自洽的，但**跨层没有单一事实来源**：多处注释与当前代码已经漂移，详见 §8。

---

## 0. 总表（先看这个）

| # | 空间 | 原点 | +X | +Y | +Z / 前向 | 手性 | 单位 | 定义位置（证据） |
|---|---|---|---|---|---|---|---|---|
| 1 | 窗口/屏幕像素 | 左上角 | 右 | **下** | — | 2D | 像素 | `GN_2Dtype.h:58-59`；`GN_2D.h:48` |
| 2 | 2D 视口后台坐标（ortho） | 左上角（`ortho_left/top`） | 右 | **下** | — | 2D | 像素 | `GN_2Dtype.h:83-93`；`GN_2D.h:48` |
| 3 | UI/输入 | 左上角 | 右 | **下** | — | 2D | 像素 | `GN_UiBase.h:60,69-70`；`GN_Input.cpp:452-453` |
| 4 | 3D 世界 | 场景任意 | 右 | **上** | 相机默认看 **-Z** | **右手** | 无全局常量（物理按米） | `mmdViewer.cpp:817-828`；`GN_3Dmath.cpp:405-433` |
| 5 | 相机/视图空间 | 相机 | 右 | 上 | **-Z 前向** | 右手 | 同世界 | `GN_3Dmath.cpp:405-433`；`testSpatial.cpp:48-58` |
| 6 | 裁剪/NDC | — | 右 | **Vulkan NDC y 向下** | z ∈ **[0,1]** | — | — | `GN_3Dmath.cpp:342-355`；`GI_PLAN.md:279` |
| 7 | 帧缓冲/视口 | 左上角 | 右 | **下** | 深度 0..1、LESS | — | 像素 | `vkFrame2d.cpp:153-156`（2D）；`vk3dInit.cpp:392-393`（3D） |
| 8 | 纹理/UV | **左上角 (0,0)** | u 右 | **v 向下** | — | — | 归一化 | `GN_2D.h:129`；`vkTexture.cpp:4` |
| 9 | MMD 模型空间 | 模型原点 | 原始数据 | **上**（左手系） | 原始数据；查看器按 **-Z 为正面** | **左手** | 原始单位 | `mmdMeshBuilder.cpp:175-177`；`mmdViewer.cpp:206-222` |
| 10 | glTF 资产空间 | 资产原点 | 右 | **上** | -Z 前 | **右手**（glTF 约定） | 米 | `GN_3Dasset.cpp:142-149`（原样读入） |
| 11 | OBJ 资产空间 | 资产原点 | 右 | 上 | 原始数据 | 右手（OBJ 约定） | 任意 | `GN_3Dmodel.cpp:282-286,445-451` |
| 12 | 物理世界 | 世界原点 | 右 | **上** | 右手 | 右手 | 米（9.81） | `GN_Physics.cpp:174`；`GN_Physics.h:45` |
| 13 | 天空/光照方向 | — | 右 | 上 | 见 §7 | — | 单位向量 | `weather.cpp:20`；`gn3d_skybox.frag:14`；`gn3d_pbr.frag:180` |
| 14 | 云/雾/降水/贴花 | — | 右 | **+Y 为上** | 云层/雾按 +Y 高度；贴花在 XZ 平面 | — | 米 | `vkCloud.cpp:201-202`；`weather.cpp:195-196`；`vkDecal.cpp:4` |
| 15 | 调试坐标轴 | 世界原点 | **+X 红** | **+Y 绿** | **+Z 蓝** | — | 随模型缩放 | `mmdViewer.cpp:575-586` |
| 16 | 数学基座 | — | — | — | 列主序 `sMAT44`、四元数 `(x,y,z,w)`、右手旋转 | — | — | `GN_3Dmath.h:78-140`；`GN_3Dmath.cpp:262-336` |

> 说明：GN 原引擎（外部参考）的默认轴是 **Z 上 / Y 左 / X 前**（SDK 样例 `Sample/3d/sample07.utf8.cpp:64` 注释）。本仓库**没有沿用**这套轴，见 `docs/CAMERA_PLAN.md:167`。

---

## 1. 数学基座（所有坐标轴的底层约定）

| 约定 | 定义 | 证据 |
|---|---|---|
| 矩阵存储 | 列主序 `m[16]`；`Elem(col,row) = m[col*4+row]`；平移在 `m[12..14]` | `GN_3Dmath.h:78-117` |
| 乘法 | `Mat44Mul(out,a,b)` 计算 `a × b`（列主序） | `GN_3Dmath.cpp:79-91` |
| 四元数 | 内存顺序 `(x,y,z,w)`，`w` 在最后；四元数乘为标准 Hamilton 积 | `GN_3Dmath.h:118-140` |
| 旋转方向 | `Mat44RotateX/Y/Z` 均为右手定则（`R_x(+90°)` 把 +Y 转到 +Z） | `GN_3Dmath.cpp:262-287` |
| 欧拉顺序 | `Mat44RotateYawPitchRoll` = `Yaw(Y) × Pitch(X) × Roll(Z)` | `GN_3Dmath.cpp:313-336` |
| LookAt | `Mat44LookAtRH`：右手系，`f = eye - center`，`r = up × f`，`u = f × r`，矩阵行 = `r/u/f` | `GN_3Dmath.cpp:405-433` |
| 2D 角度 | `Vec2Rot` 参数是**角度**（`GT_ANGLE_UNIT=π/180`），数学上逆时针为正；因屏幕 y 向下，**视觉上是顺时针** | `GN_Base.h:81`；`GN_2Dmath.cpp:84-89` |
| 2D 矩阵 | `sMAT33` 为 2D 变换；`sTRANSF2D` 位置/缩放/旋转 | `GN_2Dmath.h:241-259` |

---

## 2. 屏幕 2D：左上原点，+Y 向下

1. **窗口坐标**：`sWINDOW::viewX/viewY` 是视口**左上角**；`CoordWindowToViewX/Y` 直接把窗口 y 加偏移，不做翻转（`GN_2Dtype.h:58-59,83-87`）。
2. **2D 投影**：`GN_Vk` 每帧写 `proj`，取 `L=0, R=w, T=h, B=0`：
   ```
   m[0]=2/w, m[5]=2/h, m[10]=-1, m[12]=-1, m[13]=-1
   ```
   顶点 `(x,y)` → `NDC=(2x/w-1, 2y/h-1)`；`y=0` 得 NDC `-1`，在正高度视口里映射到帧缓冲顶部 ⇒ **屏幕 y 向下**（`vkFrame2d.cpp:153-156`；视口 `GN_Vk.cpp:260-261` 为正高度）。
3. **管线**：2D 管线 `frontFace = CLOCKWISE`、`cullBack = true`、深度关闭（`GN_Vk.cpp:240-261`）——与「y 向下」的屏幕绕序匹配。
4. **输入**：鼠标/触摸窗口坐标 → `CoordWindowToViewY` 直接换算，`mouseMY` 向下增大（`GN_Input.cpp:452-453,499-500,671-672`）；UI（滚动区/对话框/列表）全部按**左上角 + 向下**定义（`GN_UiBase.h:60,69-70,143-144`）。
5. **图像/UV**：2D 图像绘制 API 的 UV 明确是**左上角 U/V**（`GN_2D.h:129`）；`ImageDraw` 的 `v` 不做翻转；图像内存按 **top-down** 行序（`GN_Format.cpp:1710-1721`；`vkImageIo.cpp:48-52`）。2D 的「y 向下」与「v 向下」是同一套坐标，贴图不会上下翻。
6. **2D 旋转**：`Vec2Rot` 正角在数学坐标逆时针；屏幕 y 向下 ⇒ 视觉顺时针（`GN_2Dmath.cpp:84-89`）。`ImageDrawRot` 的旋转角走同一函数。

---

## 3. 3D 世界与相机：+Y 向上，右手系，看 -Z

| 项 | 约定 | 证据 |
|---|---|---|
| 上方向 | **+Y**（地面在 XZ 平面、云层在 +Y 高度、雾按 +Y 高度衰减） | `mmdViewer.cpp:454-459,469-470`；`vkCloud.cpp:201-202`；`weather.cpp:195-196` |
| 前方 | 相机默认看 **-Z**（视图空间 forward = -Z） | `testSpatial.cpp:48-58` 注释「前视 -Z」 |
| 右手系 | `Mat44LookAtRH` + `Mat44PerspectiveVK` 注释「右手坐标系」 | `GN_3Dmath.cpp:342-343,405-407` |
| 相机摆放 | 查看器默认：模型高度×1.9，位于模型 **-Z 侧**，yaw=π、俯角 0.12 | `mmdViewer.cpp:206-222` |
| 鼠标转视角 | 水平绕**世界 +Y**、垂直绕**相机右轴**；正 yaw=右转、正 pitch=抬头 | `mmdViewer.cpp:176-205,232-256` |
| 移动 | W/S 沿视线、A/D 沿右向、Q/E 沿世界竖直 | `mmdViewer.cpp:884-919` |
| 物理 | 重力 `(0,-9.81,0)`；胶囊沿**局部 +Y**；铰链默认轴 `(0,1,0)` | `GN_Physics.cpp:174`；`GN_Physics.h:45,152-165` |
| 世界单位 | 无全局单位常量；物理按米（9.81），查看器把模型高度直接当米用 | 同上 + `mmdViewer.cpp:892` |

**投影与 NDC**（`GN_3Dmath.cpp:342-355`）：

```
m[0]=f/aspect, m[5]=-f, m[10]=far/(near-far), m[11]=-1, m[14]=near*far/(near-far)
```

- 深度映射到 **Vulkan `[0,1]`**，`depthCompare = LESS`（`vk3dInit.cpp:381-383`）。
- `m[5] = -f` 是**为 Vulkan NDC 的 y 向下做的 Y 翻转**：世界 +Y → 视图 +Y → NDC **负 y** → 正高度视口映射到**屏幕上方**。所以「世界 +Y 在上」在当前工作区成立。
- **视锥平面**顺序固定为「左/右/下/上/近/远」（`GN_Spatial.h:73-85`）；由于上面的 Y 翻转，**世界 +Y 越过的是索引 2（NDC 的 y=-1 面）**，世界 -Y 越过索引 3（`testSpatial.cpp:277-283` 把这条写成了回归用例）。
- **绕序**：投影的 Y 翻转会反转 NDC 绕序，因此每个 3D 管线必须各自选定 `frontFace`（见 §4），不能假设与 2D 相同。

---

## 4. 渲染管线级：视口、正面、剔除

> 当前工作区**所有管线的视口都是正高度**（`y=0, height=+H`）。`HEAD` 里 PBR 是负高度，工作区有未提交修改（§8-1）。

| 管线 | 视口高度 | frontFace | cull | 证据 |
|---|---|---|---|---|
| 2D 批处理 | + | CW | back | `GN_Vk.cpp:254-261` |
| 3D 前向 | + | CCW | back | `vk3dInit.cpp:231-240` |
| 3D PBR | + | CCW | back | `vk3dInit.cpp:385-387`（HEAD 是负高度 + CW；工作区 2026-10-29 实测修正为 CCW） |
| 阴影 | + | CW | back | `vk3dInit.cpp:556-567`；光源 VP 由 `SetShadowLightVP` 从外部给，当前无调用者 |
| 天空盒 | + | CW（`cull=false`，绕序无实际影响） | none | `vk3dInitScene.cpp:455-475` |
| 合成/色调映射 | + | CCW（`cull=false`） | none | `vk3dInitScene.cpp:179-187` |
| G-Buffer | + | **CCW**（2026-10-29 修正，与 PBR 同规则） | back | `vk3dInitScene.cpp:706-714` |
| 延迟光照 | + | CCW | — | `vk3dInitScene.cpp:885-892` |
| 调试线 | + | CW（LINE_LIST） | — | `vkDebugDraw.cpp:131-138` |
| 布料 / 云 / 贴花 / 降水 / 粒子 / 蒙皮 / 材质 | + | CCW | back | `vkClothDraw.cpp:57-65`；`vkCloud.cpp:74-82`；`vkDecal.cpp:110-118`；`vkPrecipitation.cpp:142-150`；`vkParticleDraw.cpp:118-125`；`vkSkinDraw.cpp:73-80`；`vkSkinSsbo.cpp:72-79`；`vkMaterial.cpp:92-99` |

**要点（2026-10-29 已解决）**：G-Buffer 与 PBR 使用**同一个相机 `viewProj`**。旧盘点看到 `frontFace` 一个 CW、一个 CCW，
是因为旧版 U2b 把世界 pass 的方向写反了。实测（llvmpipe `make gpuCheck`：旧规则 78/18，修正后 96/96）证明：
正高度视口 + 投影 Y 翻转下，世界 CCW 内容的帧缓冲绕序仍是 CCW，故两条 pass 都必须 `frontFace=CCW`；
只有不含 Y 翻转的阴影 / identity 用例才用 CW。详见 `AXIS_UNIFICATION_PLAN.md` §1/§2.3。

---

## 5. 资源/文件空间

### 5.1 MMD（左手系；已做镜像 X 手性转换）

- 顶点/法线/UV 读入后**镜像 X**（`mmdMeshBuilder.cpp:188-189,236-237` → 现值以现文件为准），正面保持 +Z。
- 绕序：镜像会把三角形绕序反转，构建时**再反绕序一次**（`reverseTriangleWinding`），
  恢复「CCW 绕序 + 外向法线」一致；不反会导致模型内外翻转、光照像「法线反了」。
  实测（真实 `mode.pmd`）：原始 agree=6189/disagree=40；镜像未反绕序 agree=40/disagree=6189；
  镜像+反绕序后 agree=6189/disagree=40。
- 回归：`tests/unit/testMmdPmd.cpp` 的 `mmdPmdMirrorKeepsWindingConsistentWithNormals`。
- **注释漂移**：文件里 `:99` 的历史注释「按 0,2,1 顺写抵消 Y 翻转」不要再当依据（§8-4）。
- 纹理：TGA 若原点在左下则翻成 top-down（`mmdTextureDecode.cpp:28-38,116-120`）；PNG/BMP 解码也按 top-down；UV 不改。
- 查看器默认相机在 **+Z 侧**（U5 已改），看到的是模型正面。
### 5.2 glTF（右手、Y 上、UV 左上）

- 位置/UV **原样读入**（`GN_3Dasset.cpp:142-149`），不做轴转换。
- glTF 规范：UV `(0,0)` = 图像**左上角**（Khronos 规范原文；见 §9 外部资料）。本仓库图像/纹理按 top-down 上传（`vkTexture.cpp:4`）⇒ **一致**。
- 动画/关节/蒙皮的四元数与矩阵同样原样使用（`GN_3Dasset.cpp` 的 accessor 读取）。

### 5.3 OBJ（UV 原点在左下，未翻转）

- 位置原样读入（`GN_3Dmodel.cpp:409-445`）。
- `vt` 的 `v` 原样保存（`GN_3Dmodel.cpp:282-286`），没有 `1-v`；而图像内存是 top-down（§2-5）。
- OBJ 的 UV 原点在**左下**（格式约定）⇒ **理论上有上下翻转风险**。当前没有 OBJ 的渲染用例/截图证据，列为待验证项（§8-5），不要直接判定为 bug。

### 5.4 图像内存与纹理

| 项 | 约定 | 证据 |
|---|---|---|
| PNG 行序 | 默认 top-down（`col_turn=false` 时 `row_ptrs` 从 `tmp_data` 递增） | `GN_Format.cpp:1710-1721` |
| PNG→GPU（3D 纹理） | 逐行 top-down 上传 | `vkImageIo.cpp:48-52`；`vkTexture.cpp:4` |
| TGA | 若文件标志「原点左下」则 `flipRows` 成 top-down | `mmdTextureDecode.cpp:28-38,116-120` |
| Cubemap 面序 | `+X, -X, +Y, -Y, +Z, -Z`（Vulkan 标准顺序） | `vk3dInitScene.cpp:266-270` |
| 天空盒顶点 | 单位立方体世界方向，注释 `+X=右, +Y=上, +Z=前`；`gl_Position = clip.xyww` | `gn3d_skybox.vert:12-45` |

---

## 6. 物理与空间划分

| 项 | 约定 | 证据 |
|---|---|---|
| 重力 | 默认 `(0,-9.81,0)` | `GN_Physics.cpp:174` |
| 胶囊 | 沿**局部 +Y**；总半高 = `aHalfExtent.y + aRadius`；质心在中点 | `GN_Physics.h:45,73` |
| 铰链 | 局部轴默认 `(0,1,0)`，内部归一化 | `GN_Physics.h:152-165` |
| AABB | `aMin/aMax` 逐分量；空盒 `min=+FLT_MAX,max=-FLT_MAX` | `GN_Spatial.h:34-48` |
| 视锥 | 6 平面「左/右/下/上/近/远」，`n·p+d>=0` 为内；由 `viewProj` 提取，Vulkan z∈[0,1] | `GN_Spatial.h:73-85` |
| 视锥 Y 语义 | 世界 +Y 对应 NDC 的 y=-1 面（索引 2），世界 -Y 对应索引 3 | `testSpatial.cpp:277-283` |

---

## 7. 天空 / 天气 / 特效的方向约定

| 项 | 约定 | 证据 |
|---|---|---|
| 天空太阳方向 | `sunDirection` 是**指向太阳**的单位向量，默认 `(0.45, 0.78, 0.44)`（+Y 向上） | `weather.cpp:20`；`gn3d_skybox.frag:14,50-63` |
| 场景方向光 | `direction` 是**光的传播方向**（从光源指向场景）；着色器取 `Ldir = -direction` 得到「指向光源」 | `gn3d_pbr.frag:180`；`mmdViewer.cpp:811-813` 用 `(-0.35,-0.85,0.4)`（y 负 = 向下照） |
| 注意 | 天空的 `sunDirection`（指向太阳、y 正）与方向光的 `direction`（传播方向、y 负）**符号约定相反**；把两者直接互传会得到相反光照 | 同上 |
| 云层 | 层高在 **+Y**；`aCameraUp = (0,1,0)` 作为云层法线；tile 在 XZ 平面按相机 xz 取模 | `vkCloud.cpp:201-202`；`mmdViewer.cpp:60-63` |
| 雾 | `baseHeight` 与 `heightFalloff` 都按 **世界 +Y**；片元用 `max(worldY-baseHeight,0)` | `weather.cpp:195-196`；`gn3d_pbr.frag:35-40` |
| 降水 | 雨/雪速度沿**世界 -Y**（重力），billboard 的两轴 = 相机 right/up；静止粒子回退屏幕竖直 | `vkPrecipitation.cpp:197-220`；`mmdViewer.cpp` 的雨/雪描述 |
| 贴花 | 世界 **XZ 平面**四边形，y 由调用方给（贴地 +0.02） | `vkDecal.cpp:4`；`GN_3Ddecal.h:26` |
| 粒子 BOX | 以 `aOrigin` 为中心、`±aBoxHalfExtent` 的轴对齐盒；跟随相机时逐轴 wrap | `GN_3Dparticle.h:12,29-30,57` |
| 调试坐标轴 | 世界原点：**+X 红、+Y 绿、+Z 蓝**，长度随模型高度缩放 | `mmdViewer.cpp:575-586` |

---

## 8. 冲突、漂移与待核对（重点）

> 这些不是「轴定义」，而是**同一轴语义在不同文件里被写成了不同版本**。按影响排序。

| # | 发现 | 证据 | 影响 | 状态 |
|---|---|---|---|---|
| 1 | **PBR 视口/正面已被实测确认**：HEAD 是 `y=H,height=-H` + `frontFace=CW`；工作区是正高度 + `frontFace=CCW`，llvmpipe gpuCheck 96/96 | `vk3dInit.cpp:385-387`；`git show HEAD:src/graphics/_Vulkan/vk3dInit.cpp:366,373-375` | 以工作区为准（2026-10-29 修正） | **已确认：工作区为目标** |
| 2 | `CHANGELOG` 仍写「统一为负高度视口」，与工作区正高度相反 | `docs/CHANGELOG.md:3-6` vs `vk3dInit.cpp:361-362` | 按文档调试会得到错误结论 | 文档漂移 |
| 3 | 查看器地面注释说「走前向 3D 管线」，代码实际调用 `GN_VkDraw3DPBR`；注释又说「PBR frontFace=CW」，代码是 CCW | `mmdViewer.cpp:454-459` vs `:477-485`；`vk3dInit.cpp:385` | 地面法线/绕序的取舍依据已过期 | 代码/注释漂移（待随 T2 修） |
| 4 | MMD 构建注释说「按 0,2,1 写索引抵消 Y 翻转」，实现保持原始绕序 | `mmdMeshBuilder.cpp:99` vs `:126-131` | 后续改绕序的人会被注释误导 | 注释漂移 |
| 5 | OBJ 的 UV `v` 未翻转，而图像是 top-down；OBJ 约定 v 原点在左下 | `GN_3Dmodel.cpp:282-286,445-451`；`GN_Format.cpp:1710-1721` | OBJ 贴图可能上下翻转；当前无 OBJ 渲染回归 | 待验证（低优先） |
| 6 | G-Buffer 与 PBR 用同一 `viewProj`，旧版 `frontFace` 分别是 CW / CCW | `vk3dInitScene.cpp:707`；`vk3dInit.cpp:385` | 2026-10-29 实测：正高度 + Y 翻转下两者都必须是 CCW；G-Buffer 已改 CCW，identity 测试索引改 `BA` | **已解决** |
| 7 | 「本引擎竖直方向是镜像的」+ 地面法线传 `(0,-1,0)` | `mmdViewer.cpp:469-470`；`CHANGELOG.md:9` | 若按当前工作区（正高度 + 投影 Y 翻转）推导，世界 +Y 映射到屏幕上方，**不再镜像**；该取值可能是旧 PBR 负高度时期的补丁 | **需在 PBR 视口改动后实测复核** |
| 8 | `SetShadowLightVP` 在 `src/`、`tests/` 中无调用者，阴影光源矩阵保持默认 | `GN_3Dlight.h:86`；`GN_3Dlight.cpp:175` | 阴影 pass 的光源空间当前未被定义/使用（查看器方向光 `enableShadow=false`） | 已确认未接线 |
| 9 | 2D 与 3D 的 `frontFace` 相反（CW vs CCW） | §4 表 | 不是错误：2D 没有投影 Y 翻转、y 向下；3D 有 Y 翻转。**不要统一** | 设计差异 |

**建议的单一事实来源（若要收敛）**：

> 2026-10-29 已出统一规划 [docs/AXIS_UNIFICATION_PLAN.md](</home/ruicat/下载/GnVk/GnVk/docs/AXIS_UNIFICATION_PLAN.md>)（v2 按主流/glTF 惯例：RH、+Y 上、-Z 相机前、**+Z 资产正面**、**世界 CCW 正面**）。本节建议以该规划为准。

1. 世界：**+Y 上、右手系、相机看 -Z、米**——写进 `docs/` 并在 `GN_3Dmath.h` 顶部加一段契约注释。
2. 投影：**Vulkan NDC z∈[0,1]、投影内做 Y 翻转、正高度视口**——把「负高度视口」方案标记为已废弃。
3. 屏幕/UV：**左上原点、+Y/v 向下**——2D/UI/纹理统一。
4. 资源：MMD 原样（左手）、glTF 原样（右手）、OBJ 需确认 v 翻转——在各自 loader 头注释里写清，不要求统一手性。
5. pass 级 `frontFace` 写进 `sPIPELINE_DESC` 的契约注释，禁止从别的 pass 复制。

---

## 9. 复算命令与参考

### 9.1 本仓库锚点

```bash
# 2D 投影 / 视口 / 正面
sed -n '148,160p' src/graphics/_Vulkan/vkFrame2d.cpp
sed -n '240,262p' src/graphics/_Vulkan/GN_Vk.cpp

# 3D 投影（m[5] 翻转）与 LookAt
sed -n '342,355p;405,433p' src/graphics/GN_3Dmath.cpp

# 视锥的 Y 语义（回归用例）
sed -n '270,285p' tests/unit/testSpatial.cpp

# 各 pass 的 frontFace / 视口
grep -rn "aFrontFace\|aViewport" src/graphics/_Vulkan/*.cpp

# HEAD 与工作区的 PBR 差异
git diff -- src/graphics/_Vulkan/vk3dInit.cpp
git show HEAD:src/graphics/_Vulkan/vk3dInit.cpp | sed -n '355,390p'

# MMD / OBJ / glTF 的原始坐标处理
sed -n '95,135p;170,180p' src/mmd/mmdMeshBuilder.cpp
sed -n '282,290p;440,455p' src/graphics/GN_3Dmodel.cpp
sed -n '138,150p' src/graphics/GN_3Dasset.cpp

# 图像行序
sed -n '1705,1722p' src/core/GN_Format.cpp
sed -n '40,55p' src/graphics/_Vulkan/vkImageIo.cpp

# 物理轴
grep -n "aGravity\|eSHAPE_CAPSULE\|aLocalAxisA" src/graphics/GN_Physics.cpp src/graphics/GN_Physics.h

# 天空/光照符号
grep -n "SUN_DIRECTION" src/graphics/weather.cpp
grep -n "Ldir = normalize" assets/shaders/gn3d_pbr.frag
grep -n "sunDir\|sunDirection" assets/shaders/gn3d_skybox.frag
```

### 9.2 外部资料

- Khronos glTF 2.0 规范：UV 原点 `(0,0)` 对应图像**左上角**（<https://github.com/KhronosGroup/glTF/blob/main/specification/2.0/README.adoc>，§3.8 Texture Data）。
- Vulkan 规范：NDC 的 y 轴方向、视口变换与深度范围 `[0,1]`（<https://registry.khronos.org/vulkan/specs/latest/html/vkspec.html>，§Basic Polygon Rasterization / Controlling Viewports）。
- GN 原引擎默认轴 `Z:上 Y:左 X:前`：GN SDK 1e 样例 `Sample/3d/sample07.utf8.cpp:64`（本地 `.local/vendor/gnsdk/`，不入库）。
