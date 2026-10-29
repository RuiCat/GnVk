# tests/gpu — GPU 管线/渲染验收

对应 `docs/3D_FEATURE_PLAN.md`：P1b（11 条既有管线走 `GN3d::cPIPELINE_CACHE`）、P1c GPU 材质实例、
P2 GPU 蒙皮、P8 渲染接入（视锥剔除绘制 + 调试线框）都在本机真实 GPU 上做运行时验收。
旧的 `samples/` + `run.sh` 已移除，这里是重新设计的入口。

## 运行

真实 GPU 桌面（必须能看到 `/dev/dri`，并在桌面会话的终端里）：

```bash
make gpuCheck
```

- 退出码 `0`：全部检查 PASS。
- 退出码 `1`：有 FAIL，输出里逐条给出实际/期望值。
- 退出码 `2`：没有可用 Vulkan 设备（SKIP）；**SKIP 不算验收通过**。

没有 `/dev/dri` 的环境只能用 flatpak runtime 自带的 lavapipe 做软件预验：

```bash
VK_ICD_FILENAMES="$PWD/.local/probes/lvp_icd.json" make gpuCheck
```

软件预验（llvmpipe）只证明验收程序可运行、断言可判定；**不能替代真实 GPU 验收**。

## 检查项（94 条）

| 组 | 检查 | 判据 |
|---|---|---|
| 缓存 | `cache.entries` | 管线缓存 ≥11 条（P1b 11 + P1c/P2/P8 新增），且无命中 |
| 创建 | `pipe.2d.normal/bright/multiply/null` | 4 个 2D 混合管线非空 |
| 创建 | `pipe.3d.forward/pbr/shadow` | 前向/PBR/阴影管线非空 |
| 创建 | `pipe.composite/skybox/gbuffer/deferredLight` | 合成/天空盒/G-Buffer/延迟光照管线非空 |
| 2D | `2d.alpha/bright/multiply/null` | 半亮红前景 + 半亮蓝背景，线性 RGBA 期望值 ±0.012 |
| 3D | `3d.shadow` | 阴影深度图中心 ≈ 0.5（identity 光源 VP + z=0.5 四边形） |
| 3D | `3d.forward` / `.cull` / `.depth` | 可视绕序为绿；反绕序被剔除成 clear 色；深度 LESS 近平面覆盖远平面 |
| 3D | `3d.pbr` / `.cull` | PBR 可见（非黑）/反绕序被剔除（黑） |
| 3D | `3d.deferred.light` | 延迟光照输出非黑 |
| 3D | `3d.deferred.gbuffer` | 直接读 G-Buffer 附件 1（引擎命名为 `gNormal*`）的 albedo ≈ 0.9/0.1/0.2 |
| 3D | `3d.composite` | PBR HDR + 合成后输出非黑 |
| 3D | `3d.skybox` | 天空盒输出不等于前向 clear 色 |
| P1c | `material.desc/create/cache` | 合法描述键稳定；描述符集创建成功；两次绘制管线 miss 不增 |
| P1c | `material.draw` | UBO(0.25/0.5/0.75) × 纯白纹理 → 中心像素 `0.250,0.500,0.750`（±0.02） |
| P1c | `material.destroy/invalid` | 重复 Destroy 安全；binding 种类错/缺 binding 1 被拒且不改句柄 |
| P1c 纹理 | `texture.decode` / `texture.glTF` | 2×2 PNG 逐字节；最小 glTF（image data URI + sampler）经 `loadFromMemory` 解出同图 |
| P1c 纹理 | `texture.upload` / `texture.sample` | RGBA8 (200,40,40) 上传为 SRGB，材质采样 = sRGB→linear `0.578,0.021,0.021`（±0.03） |
| P1c 纹理 | `texture.destroy` | 重复释放安全，销毁后可再次创建 |
| P2 | `skin.pipeline/invalidArgs` | 蒙皮管线非空且缓存命中、调色板 UBO=8192；6 种非法输入全拒 |
| P2 | `skin.singleJoint/dualJoint/jointLimit` | 单关节 +0.5x、双关节 0.5/0.5 与 CPU `skinVertices` 左边界差 ≤0.03；129 关节截断到 128 |
| P8 | `cull.draw.count/origin/plusX/minusX` | 3 个立方体剔掉 -2x 那个（返回 2/可见 2），未绘制位置为 PBR clear 黑 |
| P8 | `cull.empty` | 空指针/空列表返回 0，不崩溃 |
| P8 | `cull.debugLine/outside` | 过屏幕中心的调试线中心像素 ≈ 请求颜色（±0.05），线外仍是背景 |
| P9 | `particle.create/compute` | 容量 64 的 SSBO 创建成功；dt=0.1、重力 (0,-10,0) ⇒ v.y=-1.000000、p.y=-0.100000 |
| P9 | `particle.cpuMatch` | 64 粒子 GPU 结果与 `stepParticlesCpu` 逐粒子差 = 0（容差 1e-4） |
| P9 | `particle.death` | life=0.05 经 dt=0.1 后 compute 把 `aAlive` 清 0 |
| P9 | `particle.draw` | 单个 billboard 覆盖屏幕中心，回读 `0.200,0.800,0.300`（±0.03） |
| P9 | `particle.invalid` | dt<=0、越界上传、pass 外绘制、重复销毁全部安全返回 false |
| P9 | `particle.perf` | 65536×4 步：GPU 0.26 ms/步 vs CPU 0.10 ms/步，末粒子结果一致（耗时仅信息性） |
| P9-a | `cloth.create/compute` | 8×8 布料 SSBO 创建成功；20 步后 meanY 0 → -0.2153、四角位置逐位不变、全有限 |
| P9-a | `cloth.cpuMatch` | 同初始数据 1 步后 GPU 与 `stepClothCpu` 最大差 = 0（容差 1e-3） |
| P9-a | `cloth.draw` | 平面布料按 identity VP 绘制，中心像素 `0.700,0.300,0.200`（±0.05） |
| P9-a | `cloth.invalid` | dt<=0、count 不符上传、pass 外绘制、重复销毁全部安全返回 false |
| P9-b | `skin.ssbo.pipeline/joint200` | 管线 init 预建（绘制不再新增缓存项）；200 关节、关节 150 平移 +0.5x 后像素 `0.572,0.254,0.127` |
| P9-b | `skin.ssbo.paletteRing` | 同一帧两次绘制用不同调色板（3 槽轮转），两块各自出现在预期位置 |
| P9-b | `skin.ssbo.invalid` | paletteCount 0/4097、空指针、3D pass 外调用全部返回 false |
| P9-c | `batch.sort/switches` | 稳定排序后键单调、aOrder 集合不变；状态切换 3 → 1 |
| P9-c | `batch.draw` | 乱序与排序各画一帧，两点采样颜色一致（±0.02） |
| P10 | `lod.select` | 同一两级网格：相机 10 m 选 0 级、1000 m 选 1 级 |
| P10 | `lod.indexCount` | 按选级绘制时 `GN_VkLast3DIndexCount()` = 36（LOD0）/ 6（LOD1） |
| P10 | `lod.singleLevel` | levels 为空时仍提交 LOD0 的 36 个索引（单级回归） |
| P10 | `lod.pixels` | LOD0 与 LOD1 都含同一前面，中心像素一致（±0.05） |
| P11-S3 | `fog.pbr` | 红色雾（density=5）下 PBR 四边形中心像素红通道 > 0.4 且明显高于无雾值 |
| P11-S3 | `fog.pbrReset` | density 归零后中心像素回到无雾值（±0.03） |
| P11-S3 | `fog.forward` | Blinn-Phong 路径（binding 3）同样被蓝色雾影响 |
| P11-S2b | `cloud.outsidePass` | pass 外返回 false；pass 内 tiles=0/65、tileSize=0 也被拒绝 |
| P11-S2b | `cloud.draw` | 3D pass 内合法参数返回 true（三层云盘共 96 个四边形） |
| P11-S2b | `cloud.pixels` | 云帧 4 个采样点亮度 − 清屏亮度 > 0.02（alpha 混合生效） |
| P11-S2b | `cloud.destroy` | 重复 Destroy 安全，之后 pass 外绘制仍返回 false |
| P11-S5 | `precipitation.outsidePass` | 3D pass 外调用返回 false |
| P11-S5 | `precipitation.empty` | `aCount=0` / `aParticles=nullptr` 各跑一帧均返回 false |
| P11-S5 | `precipitation.draw` | 512 粒子的 3D pass 内提交成功（覆盖 2× 顶点缓冲扩容） |
| P11-S5 | `precipitation.pixels` | 中心粒子（size=0.5、alpha=1、纯红）使中心像素 r > 0.25 |
| P11-S5 | `precipitation.perspective` | 真实透视相机（lookAt + 60° fov）下原点粒子落在画面中心且为红（验证真实 viewProj 路径） |
| P11-S5 | `precipitation.overGeometry` | 先画不透明 PBR 四边形（z=0.5）、再画更近的粒子（z=0.2）⇒ 中心变红（证明半透明降水不会被深度测试剔掉） |
| P11-S6b | `decal.outsidePass` | 3D pass 外调用 `GN_VkDrawDecals` 返回 false |
| P11-S6b | `decal.empty` | `aCount=0` / `aItems=nullptr` 各跑一帧均返回 false |
| P11-S6b | `decal.draw` | 64 条贴花（水渍/溅射环混合）在 3D pass 内提交成功 |
| P11-S6b | `decal.pixels` | 俯视相机下原点 2 m 红色水渍使中心像素转红（r > 0.4） |
| 关闭 | `gpu.shutdown` | `GN_VkShutdown` 后 device 已释放且管线缓存为空 |

## swapchain 格式（回读口径）

- 引擎直接取 surface 的第一个格式：lavapipe 是 `B8G8R8A8_SRGB`，RADV 780M 是
  `R16G16B16A16_SFLOAT`（`format=97`）。
- 验收程序按实际格式解码：8-bit sRGB 先解码到线性，RGBA16F 直接读 half；之后统一按**线性 RGBA**
  比较。2D 期望值是线性混合结果（如半亮红 over 半亮蓝 = 0.251/0/0.251/0.502），不再依赖 8-bit/sRGB 编码。

## 验收前置：回读所需的 image usage（不改变渲染结果）

为了让像素/深度回读合法，本巡检补了 `VK_IMAGE_USAGE_TRANSFER_SRC_BIT`：

- `src/graphics/_Vulkan/GN_Vk.cpp`：swapchain image usage（`GN_VkReadBack`/截图路径也需要）。
- `src/graphics/_Vulkan/vk3dInit.cpp`：shadow map image。
- `src/graphics/_Vulkan/vk3dInitScene.cpp`：G-Buffer 三张颜色附件 + 深度附件。

这些只影响 `vkCmdCopyImageToBuffer` 是否合法，不改变渲染输出。

## 验收中发现的引擎问题（只报告，未处理）

1. `GN_VkDraw3D_GBuffer` 在命令缓冲录制中更新已绑定的 `gBufferDescriptorSet`；
   Validation 报 `VUID-vkCmd*-commandBuffer-recording`（描述符集已更新）。
   本验收改用测试侧直接绑定 G-Buffer 管线绘制，验证的是**缓存管线**本身；
   引擎这条 draw 路径需要单独修（更新描述符放在绑定之前，或改为 push descriptor）。
2. PBR 管线使用负高度视口，但实例请求 `VK_API_VERSION_1_0`；
   Validation 报 `VUID-VkViewport-apiVersion-07917`。真实驱动可能拒绝创建 PBR 管线，属 P1b 桌面验收风险。
3. HDR render pass 的 resolve 附件格式与颜色附件不一致
   （`VUID-VkSubpassDescription-pResolveAttachments-00850`）。
4. swapchain `minImageCount` 固定为 2，未按 surface caps 取值
   （`VUID-VkSwapchainCreateInfoKHR-presentMode-02839`；lavapipe 报 minImageCount=4）。
5. 命令池未带 `VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT`，但每帧 `vkResetCommandBuffer`
   （`VUID-vkResetCommandBuffer-commandBuffer-00046`）。
6. 2D 的 `static s_lastBlendMode` 不随帧重置，且顶点批处理直到 `GN_VkEndFrame` 才提交；
   同一帧内混用多种混合模式时，整帧会使用最后绑定的那条管线。测试按“每帧只用一种目标混合”规避。
7. `GN_VkReadBack` 固定按 4 字节/像素分配与返回（`vkImageIo.cpp`），RGBA16F swapchain 会被截断一半；
   `cVIEW2D::SaveToFile` 等截图路径同样受影响。验收程序改为自己按实际格式回读，未改引擎这条路径。
8. `CreateCubeModel` 声明 `aVertexCount=36`，但 `GN_3Dmodel.cpp:440` 只写 24 个顶点（`base = fi*44` 六个面）；
   `computeWorldAABB()` 会读到未初始化顶点（实测 min.x=-2.7e36）。P8 的 GPU 验收因此用测试内自建的
   24 顶点立方体，未改该文件。
9. `Ensure3DBufferCapacity` 在同一 3D pass 内扩容时会 `vkDestroyBuffer` 旧缓冲；
   已记录的 draw 仍引用旧句柄（Validation 会报 buffer in use）。P1c/P2/P8 的绘制复用这条路径，
   本次未修（属既有缺陷），大场景多 draw 时应改为延迟销毁。
10. `GN_VkInit3D()` 在 `GN_Vk.initialized = true` 之前调用各 `init3D*`；
    新增 init 一律以 `device/renderPass` 判空，不得检查 `initialized`（否则静默失败）。
11. 纹理 v1 无 mipmap/各向异性；JPEG 依赖 `libjpeg`（pkg-config 缺失时只有 PNG）；
    `sASSET::loadFromMemory` 不解析外部图片文件（无 base 目录），外部 uri 需 `loadFromFile`。
12. GPU 布料 v1 只有结构距离约束、每步 fence 等待；SSBO 蒙皮上限 4096 关节（旧 128 关节 UBO
    路径保留）；批处理只做排序不合并几何。
13. `batch.draw` 验收前先画一帧预热：`Ensure3DBufferCapacity` 同 pass 内扩容会销毁旧缓冲
    （见第 9 条），首帧较早的绘制会丢失，属既有缺陷、本轮未修。

## 当前状态

- 真实 GPU（AMD Radeon 780M，RADV PHOENIX，RGBA16F swapchain）：**94/94 PASS**
  （2026-10-29：P1b 11 条 + 2D/3D + P1c 材质 6 + 纹理 5 + P2 5 + P8 6 + P9 粒子 7、
  布料 5、SSBO 蒙皮 4、批处理 3 + P10 LOD 4 + P11-S3 雾 3 + P11-S2b 云 4 + P11-S5 降水 6 + P11-S6b 贴花 4）。
- 历史：P1b 首跑 20/31 → 二跑 21/31 → 自建 swapchain 回读后 31/31；随后新增 23 条到 54/54，
  P9 粒子 7 条到 61/61，P9 剩余 12 条到 73/73，P10 4 条到 77/77，P11-S3 3 条到 80/80，
  P11-S2b/S5 8 条到 88/88，降水透视/覆盖几何 2 条到 90/90，贴花 4 条到 94/94。
