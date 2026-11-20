# imoScene — 新生活は妹ちゃんと! / imo_chan 单角色场景

从 Unity 提取资源（`models/imo_chan/imo_chan.fbx`）建立的一个 GnVk 场景程序：
一个角色（导入姿势）+ 地面 + 天空盒 + 方向光/补光 + 轨道相机。

## 构建与运行

```bash
# 1. 资产：FBX → GLB（需要 Blender；输出落在 .local/，不入库）
make imoSceneAsset

# 2. 编译场景程序（依赖 libGN_mini.so）
make imoScene

# 3. 运行（需要 Vulkan 设备与窗口；本仓库开发机可能没有 /dev/dri）
LD_LIBRARY_PATH=. ./build/imoScene
# 换模型：
IMO_SCENE_MODEL=/path/to/other.glb LD_LIBRARY_PATH=. ./build/imoScene
```

`make imoSceneAsset` 的默认输入/输出可用变量覆盖：

```bash
make imoSceneAsset \
  IMO_SCENE_FBX="/path/to/imo_chan.fbx" \
  IMO_SCENE_ASSET=".local/imoScene/imo_chan.glb"
```

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
IMO_SCENE_INSPECT=1 LD_LIBRARY_PATH=. ./build/imoScene

# 2) 保守运行（建议在 TTY 里、用 timeout 兜底；先跑已知可用的 mmdViewer 对照）
GN_VSYNC=1 IMO_SCENE_NO_TEXTURES=1 IMO_SCENE_MAX_MESHES=1 \
  timeout 20 env LD_LIBRARY_PATH=. ./build/imoScene 2> /tmp/imoScene.log

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
imo_chan.fbx ──(Blender, tools/assetconvert/fbx2glb.py)──▶ .local/imoScene/imo_chan.glb
        │
        ├─ imoSceneConvert   glTF 导入 → 蒙皮 CPU 烘焙 → 重定心（脚底 y=0、xz 居中）
        │                    贴图 alpha 统计 → 材质 OPAQUE / MASK / BLEND
        ├─ imoSceneUpload    sModelData → cMODEL / sIMAGE（ImageLoadFromMem，需引擎已初始化）
        ├─ imoSceneCamera    轨道相机与输入
        └─ imoSceneMain      引擎回调、光照、地面、3D pass 主循环
```

| 文件 | 一句话职责 |
|---|---|
| `imoSceneConvert.h/.cpp` | glTF → 烘焙网格/材质/贴图（CPU，无 Vulkan） |
| `imoSceneUpload.h/.cpp` | CPU 数据 → 引擎对象与贴图上传 |
| `imoSceneCamera.h/.cpp` | 轨道相机与输入映射 |
| `imoSceneMain.cpp` | 入口、光照、地面、绘制 |
| `tools/assetconvert/fbx2glb.py` | Blender 无头 FBX → GLB（保留 skin/贴图，不导动画） |

## 关键决策

- **蒙皮在 CPU 烘焙**：GLB 的 skin 有 1797 个关节，超过 GPU 调色板 UBO 上限（128）；
  场景只显示导入姿势，因此用 `computeSkinPalette` + 四权重混合一次性烘焙进顶点，
  之后走普通 `cMODEL`/PBR 路径（不占 SSBO 蒙皮管线，也不受关节数限制）。
- **alpha 混合策略**：本资产 26 个材质里 25 个 glTF 标 `BLEND`，但贴图 alpha 实际是二值的
  （衣料外侧 0、本体 1）：全走 `BLEND` 会因为“深度写关 + 网格级排序”看起来像半透明鬼影；
  因此按贴图统计分流：无 alpha → 源模式（`OPAQUE`）、半透明占比 >30%（`Toumei`）→ `BLEND`、
  其余 → `MASK 0.5`。Blender 用 `CLIP` 渲染同一份 GLB 与 `BLEND` 结果一致（`render_clip.png` / `render_gltf.png`）。
- **重定心**：网格整体平移到脚底 y=0、xz 中心为原点，相机取景只依赖模型高度。

## 边界

- 不做动画播放：humanoid muscle-clip 没有离线关键帧（见提取报告），本场景是静态导入姿势；
- 不做阴影、后处理调节、HUD 叠层；
- 只用于单角色验证，未接入场景文件格式/多模型摆放；
- 本次开发机无 Vulkan 设备（`/dev/dri` 缺失），已完成的验证是：`make imoScene` 编译通过 +
  CPU 探针（`buildModelFromGltf` 的真实代码路径）统计；**画面效果需在有 GPU 的机器上运行确认**。

## 验证记录

```text
make imoScene                                     # 编译通过（0 error）
probeConvert .local/imoScene/imo_chan.glb         # meshes=60 verts=129951 tris=193318
                                                  # height=1.851 zeroWeight=0 badIndex=0 nanVertex=0
lintCodeModularity tests/imoScene tools/assetconvert --naming --comments   # 0 违规 / 0 告警
```
