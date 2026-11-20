# imoScene — 新生活は妹ちゃんと! / imo_chan 单角色场景

从 Unity 提取资源建立的一个 GnVk 场景程序：一个角色（导入姿势）站在游戏原场景的房间里
（`room.glb`：level1 的可见静态网格，按材质合并）+ 天空盒 + 方向光/吸顶灯补光 + 轨道相机。
房间缺失时自动降级为“角色 + 虚拟地面”。

## 构建与运行

```bash
# 1. 资产（需要 Blender；输出落在 .local/，不入库）
make imoSceneAsset   # 角色：FBX → GLB
make imoRoomAsset    # 房间：游戏数据 level1 → GLB（走外部提取管线的 UnityPy venv）
make imoSceneAssets  # 一次生成两份

# 2. 编译场景程序（依赖 libGN_mini.so）
make imoScene

# 3. 运行（需要 Vulkan 设备与窗口；本仓库开发机可能没有 /dev/dri）
./build/imoScene                     # 已带 rpath=$ORIGIN/..，不必设 LD_LIBRARY_PATH
# 换模型 / 换房间 / 不要房间：
IMO_SCENE_MODEL=/path/to/other.glb ./build/imoScene
IMO_SCENE_ROOM=/path/to/room.glb IMO_SCENE_NO_ROOM=1 ./build/imoScene
```

`make imoSceneAsset` 的默认输入/输出可用变量覆盖：

```bash
make imoSceneAsset \
  IMO_SCENE_FBX="/path/to/imo_chan.fbx" \
  IMO_SCENE_ASSET=".local/imoScene/imo_chan.glb"

# 房间导出：数据目录 / 管线位置 / 中间产物 / 产物都可覆盖
make imoRoomAsset \
  IMO_UNITY_EXTRACT="/path/to/.unity-extract" \
  IMO_ROOM_DATA="/path/to/<游戏>_Data" \
  IMO_ROOM_PAYLOAD=".local/imoScene/room" \
  IMO_ROOM_ASSET=".local/imoScene/room.glb"
```

## 环境变量

| 变量 | 默认 | 作用 |
|---|---|---|
| `IMO_SCENE_MODEL` | `.local/imoScene/clothes_imo_chan.glb` | 角色 GLB |
| `IMO_SCENE_ROOM` | `.local/imoScene/room.glb` | 房间 GLB（游戏场景静态网格） |
| `IMO_SCENE_NO_ROOM` | 未设 | `=1` 关闭房间，退回“角色 + 虚拟地面” |
| `IMO_SCENE_INSPECT` | 未设 | `=1` 只自检角色；`=room` 只自检房间（不建窗口/不初始化 Vulkan） |
| `IMO_SCENE_HIDE` / `IMO_SCENE_SHOW` | `Kemo_Body,Jirai` / 空 | 网格名子串过滤（只作用于角色） |
| `IMO_SCENE_NO_TEXTURES` / `IMO_SCENE_MAX_MESHES` | 关 / 0 | 贴图与网格数的最小化二分开关 |
| `IMO_SCENE_AMBIENT` | 0.35 | 房间模式的均匀环境辐亮度（0.2–0.6） |
| `IMO_SCENE_BLOB` | 开 | `=0` 关掉贴地软阴影（假接触阴影） |
| `IMO_SCENE_BLOB_SHADOW` / `IMO_SCENE_BLOB_RADIUS` | `assets/imoScene/blob_shadow.png` / 0.46 | 贴地阴影贴图与半径（米） |
| `IMO_SCENE_SHADOWS` | 关 | **实验**：`=1` 开启方向光 CSM（真机首帧会触发 GPU 页错误，勿在日常会话用） |
| `IMO_SCENE_INDOOR_GI` | 开 | `=0` 退回天空 IBL（室内暖灰 SH9 的回退开关） |
| `IMO_SCENE_SHADOW_INTENSITY` | 0.45 | 阴影方向光的强度 |
| `IMO_SCENE_LAMPS` | 1.0 | 房间模式三盏吸顶点光的强度倍率 |
| `IMO_SCENE_EV` | -0.40（房间）/ -0.5（角色） | 合成曝光（EV） |
| `IMO_SCENE_FORCE_OPAQUE` / `IMO_SCENE_ALBEDO` | 关 / 关 | 材质与光照诊断 |

## 操作

| 输入 | 行为 |
|---|---|
| 左/右键拖动 | 绕焦点旋转（yaw=0 时相机在 +Z 侧看角色正面） |
| 滚轮 / 上滚 | 拉近（每格距离 ×exp(-0.09)） |
| 中键拖动 | 平移焦点 |
| 方向键 / J L | 键盘旋转（无鼠标时兜底） |
| R | 回到初始取景 |
| Esc | 退出（引擎默认） |

## 安全诊断（真机首次运行前）

本程序会创建 Vulkan 窗口与 3D 管线；**在确认稳定前，不要在正使用的桌面会话里直接跑**。
先用不碰 GPU 的自检，再从最小负载往上加：

```bash
# 1) 只做 CPU 转换与统计：不建窗口、不初始化 Vulkan
IMO_SCENE_INSPECT=1 ./build/imoScene

# 2) 保守运行（建议在 TTY 里、用 timeout 兜底；先跑已知可用的 mmdViewer 对照）
GN_VSYNC=1 IMO_SCENE_NO_TEXTURES=1 IMO_SCENE_MAX_MESHES=1 \
  timeout 20 ./build/imoScene 2> /tmp/imoScene.log

# 3) 逐步放开：MAX_MESHES=1 → 4 → 全部；再去掉 NO_TEXTURES
```

启动进度全部打到 stderr（无缓冲）并同步 stdout，卡住时看 `/tmp/imoScene.log` 停在哪一行：

| 日志 | 含义 |
|---|---|
| `AppInit: 开始读取/转换 glTF` | 进入 CPU 转换（此前是引擎窗口/交换链初始化） |
| `AppInit: 开始 GPU 上传` | `ImageLoadFromMem` 每张 PNG 一次上传 |
| `首帧: 开始 3D pass` | 第一次录制绘制命令 |
| `首帧: 录制完成（此后卡住 = present/swapchain）` | 命令已录完，问题在提交/呈现 |

若出现整机死机/桌面退出，请保留（只读命令，不需要再启动本程序）：

```bash
journalctl -b -1 -p err --no-pager | tail -50     # 上一次启动的错误
dmesg | grep -iE "amdgpu|gpu reset|ring|hang|oom" # 驱动/显存/OOM
coredumpctl list | tail                            # 崩溃转储
free -h; lspci -nn | grep -i vga                   # 内存与 GPU
```

## 管线与模块

```text
imo_chan.fbx ──(Blender, tools/assetconvert/fbx2glb.py)──▶ .local/imoScene/clothes_imo_chan.glb
        │
游戏数据 level1 ──(.unity-extract/pipeline/sceneExport.py → sceneBlender.py)──▶ .local/imoScene/room.glb
        │
        ├─ imoSceneConvert   glTF 导入 → 蒙皮 CPU 烘焙 → 重定心（角色；房间不重定心）
        │                    贴图 alpha 统计 → 材质 OPAQUE / MASK / BLEND
        ├─ imoSceneUpload    sModelData → cMODEL / sIMAGE（ImageLoadFromMem，需引擎已初始化）
        ├─ imoSceneRoom      房间的路径配置 / 转换 / 上传（可缺省）
        ├─ imoSceneCamera    轨道相机与输入
        └─ imoSceneMain      引擎回调、光照、地面、3D pass 主循环（房间先画、角色后画）
```

| 文件 | 一句话职责 |
|---|---|
| `imoSceneConvert.h/.cpp` | glTF → 烘焙网格/材质/贴图（CPU，无 Vulkan） |
| `imoSceneUpload.h/.cpp` | CPU 数据 → 引擎对象与贴图上传 |
| `imoSceneCamera.h/.cpp` | 轨道相机与输入映射 |
| `imoSceneRoom.h/.cpp` | 房间背景：路径配置、转换选项、加载上传 |
| `imoSceneMain.cpp` | 入口、光照、地面、绘制（房间 → 角色） |
| `tools/assetconvert/fbx2glb.py` | Blender 无头 FBX → GLB（保留 skin/贴图，不导动画） |
| `.unity-extract/pipeline/sceneExport.py` | Unity 场景静态网格 → 按材质合并的 npz + 贴图（仓库外） |
| `.unity-extract/pipeline/sceneBlender.py` | 中间产物 → `room.glb`（仓库外） |

## 关键决策

- **蒙皮在 CPU 烘焙**：GLB 的 skin 有 1797 个关节，超过 GPU 调色板 UBO 上限（128）；
  场景只显示导入姿势，因此用 `computeSkinPalette` + 四权重混合一次性烘焙进顶点，
  之后走普通 `cMODEL`/PBR 路径（不占 SSBO 蒙皮管线，也不受关节数限制）。
- **alpha 混合策略**：本资产 26 个材质里 25 个 glTF 标 `BLEND`，但贴图 alpha 实际是二值的
  （衣料外侧 0、本体 1）：全走 `BLEND` 会因为“深度写关 + 网格级排序”看起来像半透明鬼影；
  因此按贴图统计分流：无 alpha → 源模式（`OPAQUE`）、半透明占比 >30%（`Toumei`）→ `BLEND`、
  其余 → `MASK 0.5`。Blender 用 `CLIP` 渲染同一份 GLB 与 `BLEND` 结果一致（`render_clip.png` / `render_gltf.png`）。
- **重定心**：网格整体平移到脚底 y=0、xz 中心为原点，相机取景只依赖模型高度。

## 默认模型：Clothes_imo_chan（穿好衣服的版本）

游戏数据里 `imo_chan` 根下的 `kemo_Body` 是 **inactive** 的裸体变体（Unity 里 `m_IsActive=False`），
提取管线原先没有过滤它，导致“胸穿透衣服/身体实体混乱”。真正穿好衣服的角色在 `Clothes_imo_chan` 根下
（材质 `Clo_T_Shatu`/`Clo_Jazi_Uwagi`/`Clo_Zubon_Mat`/`Clo_Sera_top`/`Clo_Sukart_Mat`，皮肤 `Clo_Body_00_SKIN`）。

- 默认模型：`.local/imoScene/clothes_imo_chan.glb`（`make imoSceneAsset` 生成，源 FBX 指向 `models/Clothes_imo_chan`）；
- 默认过滤：`Kemo_Body,Jirai`（游戏里同为 inactive 的裸体/镜片变体），可用 `IMO_SCENE_HIDE=` 覆盖；
- **`imo_chan` 根导出的 GLB 已废弃（不要再用）**：那套资源里的身体是 inactive 的 kemo 变体，
  衣物与身体不匹配（胸穿模、缺腿），属于提取源本身的问题；真机验证 `clothes_imo_chan` 才是正确显示。

## 资产修正（提取管线的贴图错绑）

`N00_000_00_Body_00_SKIN` 在原 FBX 里被绑到内衣贴图 `pan_ichigo_Pink_P_524.png`（97.6% 透明背景、无皮肤），
按 MASK 渲染时人体几乎整片被裁掉。`tools/assetconvert/fbx2glb.py` 现在会把它换成同目录
`textures/ImoBody_NL_*.png`（人体皮肤图集）；用 Body_SKIN 网格的 UV 采样该图，alpha 覆盖率 100%、
RGB 为正常肤色，确认是正确的贴图。转换日志会打印 `FBX2GLB remap: Body_00_SKIN -> ...`。

## 房间背景（游戏场景 level1）

`make imoRoomAsset` 从游戏数据里取 level1 场景的**可见**静态网格（MeshRenderer + MeshFilter）：

- 680 个 MeshRenderer → 509 个 `m_IsActive`，再排除父链未激活（302）、`m_Enabled=false`（165）、
  无 MeshFilter（2）与角色子树（1）后，实际导出 **211 个渲染器**；
- 按材质合并成 **66 个网格 / 45,595 顶点 / 47,602 三角形 / 64 材质 / 59 贴图**，`room.glb` 约 10 MB；
- 坐标：Unity 世界坐标（`matrixFromTrs` 累积到场景根）→ `(x, -z, y)` 转 Blender → **减去角色根世界位置**
  （`imo_chan` ≈ `(0.006, 0, 1.512)`），于是角色根就是原点、角色与房间天然对齐；
- **两处坐标陷阱**（都在导出脚本里处理）：
  1. 静态合并网格（名字 `Combined Mesh (root: scene) N`）的顶点已经烘进场景根世界坐标，
     渲染器自身却挂着建库时的单位换算节点（scale 80–120）——必须按单位矩阵处理，否则整栋房子放大百倍；
  2. 房间与角色共用世界坐标，因此房间转换时 `aRecenter=false`；角色仍按 AABB 重定心（脚底 y=0）。
- alpha：Blender 导出的场景材质全是 `OPAQUE`，玻璃/窗纱/软垫的透明从贴图 alpha 反推
  （`aClassifyAlpha=true`，沿用角色那套规则），本场景实际分布 `OPAQUE=43 / MASK=15 / BLEND=6`；
- 房间在 3D pass 里先画、角色后画；房间加载成功时**不再画**那块 60×60 虚拟地面（会与木地板 z-fighting）；
  光照额外加两盏吸顶灯点光，方向光保留。

室内布光（房间模式）：

- **不用天空 IBL**：户外天空是亮蓝色，直接当环境光会把房间洗成一片淡蓝、丢掉明暗。
  `applyRoomAmbient()` 用自定义 GI UBO 覆盖成**均匀暖灰 SH9**（`sh00 = L·√(4π)`，其余系数清零），
  镜面天空压到 1/4；`IMO_SCENE_AMBIENT` 调亮度（默认 0.35），`IMO_SCENE_EV` 调曝光（默认 -0.40）。
- **不加太阳**：方向光没有阴影时只会把室内照成一片平；改为三盏暖色吸顶点光
  （角色上方 2.6 / 厨房 1.8 / 玄关 1.1，`IMO_SCENE_LAMPS` 倍率），点光衰减给远处墙面自然明暗。
- 角色模式（`IMO_SCENE_NO_ROOM=1`）保持原来的 GI/IBL + 方向光参数不变。
- **贴地软阴影（默认）**：`imoSceneBlobShadow` 把 `assets/imoScene/blob_shadow.png`（128² 径向渐变）
  铺在脚下（半径 0.46 m，`IMO_SCENE_BLOB_RADIUS`），走 PBR 混合管线，只压暗脚下、不引入新光照。
  这是 CSM 不可用时的接地感替代。
- **CSM（实验、默认关闭）**：`imoSceneShadow` 每帧拟合 4 级级联（2048²、λ=0.5、拟合盒=角色 AABB 外扩 2.5 m），
  3D pass 前画深度图、只投角色。**真机实测开启后首帧触发 `amdgpu page fault` → GPU reset → 合成器崩溃**
  （见 CHANGELOG 2026-11-21），因此默认关闭，仅在独立 TTY/可恢复环境里用 `IMO_SCENE_SHADOWS=1` 复现。
  已知边界：`cMODEL::DrawShadow` 只画 OPAQUE 网格（本角色 22/32 OPAQUE、10 MASK），
  MASK 部件不投影，阴影轮廓偏瘦。

Blender CPU 预览：`.local/imoScene/render_room_default2.png`（明亮版）与
`.local/imoScene/render_room_engine_light.png`（近似室内布光）。

## 边界

- 不做动画播放：humanoid muscle-clip 没有离线关键帧（见提取报告），本场景是静态导入姿势；
- 不做阴影、后处理调节、HUD 叠层；
- 只有“角色 + 房间”两份 GLB 的手工装配，未接入场景文件格式；房间只含可见静态网格（无动态物体）；
- 本次开发机无 Vulkan 设备（`/dev/dri` 缺失），已完成的验证是：`make imoScene` 编译通过 +
  CPU 探针（`buildModelFromGltf` 的真实代码路径）统计；**画面效果需在有 GPU 的机器上运行确认**。

## 验证记录

```text
make imoScene                                     # 编译通过（0 error）
make imoRoomAsset                                 # SCENE level1 renderers 211 groups 66 verts 45595 tris 47602
                                                  # SCENEBLENDER objects=66 verts=45595 tris=47602 bytes=10147660
IMO_SCENE_INSPECT=1    ./build/imoScene           # 角色 meshes=43 verts=95085 height=1.624 alpha OPAQUE=22 MASK=10
IMO_SCENE_INSPECT=room ./build/imoScene           # 房间 meshes=66 verts=76694 tris=47586 textures=59
                                                  # AABB x=[-13.06,8.37] y=[-2.17,3.35] z=[-8.49,5.62]
                                                  # alpha OPAQUE=43 MASK=15 BLEND=6
lintCodeModularity tests/imoScene --naming --comments   # 0 违规 / 0 告警
```
