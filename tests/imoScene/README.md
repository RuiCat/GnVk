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
- **alpha 重判**：Blender 导出的 glTF 把 26 个材质里的 25 个标成 `BLEND`；
  按贴图 alpha 分布重判为 `OPAQUE`（无透明像素）/ `MASK 0.5`（镂空）/ `BLEND`（大量半透明，如 `Toumei`）。
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
