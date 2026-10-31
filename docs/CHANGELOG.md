# Changelog

## 2026-10-31（四） — 真 GPU `gpuCheck` 99/99 PASS（RADV PHOENIX）

- **环境**：AMD Radeon 780M Graphics（RADV PHOENIX，Mesa 26.2.5，`renderD128`，Wayland `:1`）。
- **此前 SKIP 原因**：`workspace-write` 沙箱隐藏 `/dev/dri`，`vkCreateInstance` 报 `INCOMPATIBLE_DRIVER`（并非驱动缺失）；
  换 `danger-full-access` 后同一命令正常。
- **结果**：`make gpuCheck` = **99/99 PASS / 0 fail / 0 skip**。覆盖 2D 混合、3D forward/PBR/阴影/G-Buffer/延迟、
  GPU 蒙皮（SSBO >128 关节）、批处理、LOD、雾、云、降水、贴花、地面、`gpu.shutdown`。
- **仍未覆盖（待人工目视）**：GI/SSAO/SSGI/探针视觉（无 gpuCheck 断言）、拖拽 resize + 验证层、
  MMD 头发/裙边 BLEND、MMD 正反/左右（`tests/うさうさ`）。
- **验收**：`make unit` = 371/0 不变；本次仅改文档与 §0 状态。

## 2026-10-31（三） — resize 后全部固定视口管线重建（修调试 AABB 错位/反向随相机）+ 文档状态清理

- **现象**：窗口 resize 后模型 AABB/坐标轴错位，并随相机反向移动；根因是这些管线用创建时的固定 viewport/scissor，
  resize 后未重建（task-22 只覆盖前向/PBR/天空盒/G-Buffer/延迟/SSAO/SSGI/composite/调试视图）。
- **修复**：为调试线框、材质实例、蒙皮、粒子 billboard、布料、贴花、降水、云新增可重入 `recreate…Pipeline()`
  （按当前 extent、经 aPipelineCache，init 与 resize 共用），并在 `GN_VkResize` 中调用；另补 2D 叠层 4 条固定视口管线。
- **已补（task-27）**：`vkSkinSsbo.cpp` 的 SSBO 蒙皮管线（>128 关节）也已加 `recreateSkinnedSsboPipeline()` 并纳入 `GN_VkResize`；至此 `_Vulkan` 内所有固定视口管线均随 resize 重建（阴影 2048²、环境烘焙 128² 固定尺寸除外）。
- **文档状态清理**：GI_PLAN §9 改为状态表、§10 已解决风险标记；3D_FEATURE_PLAN 头部/§0（#10 完成 + 新增 #11 resize、#12 alpha/BLEND）/
  §3.1（P12、T4 标完成）/§3.7/§5；CAMERA_PLAN 的旧 case 计数（330/297 → 371）与 C5 状态。
- **验收**：`make` exit 0；`make unit` = **371 cases / 0 failed**；`make mmdViewer`、`make gpuCheck` 编译通过；lint 仅存量。
  **GPU 未运行验证**：resize 拖拽、各管线显示位置。

## 2026-10-31（二） — resize 回调接线 + BLEND 透明管线/排序（371 cases / 0 failed；无 GPU 运行验证）

### resize 接线（g0-dev）
- `src/platform/_Main/GN_Main.cpp`：注册 `glfwSetFramebufferSizeCallback`，回调只写 `static GT_PENDING_RESIZE_W/H`；
  主循环 `glfwPollEvents()` 后、`GF_FrameCycleProc1/2` 前处理 pending（w/h>0 且非 `GLFW_ICONIFIED` → 取最后值、清 pending、
  调 `GnVkResize`）。task-8 的 resize 重建从"无调用者"变成窗口拖拽即触发。
- 已知代价：拖动期间逐帧同步 `GN_VkResize`（waitIdle + 全量 3D 重建）可能卡顿/闪烁；后续可做"停止拖动后合并一次"节流。

### BLEND（alpha-dev）
- `GN_Vk.h` 新增 `pipelinePBRBlend`；`GN_VkDraw3DPBR(..., bool alphaBlend=false)`；`vk3dForward.cpp` 运行期选管线，
  混合管线缺失回退不透明；`vk3dInit.cpp` 的 `create3DPbrPipeline` 建混合变体（`eBLEND_ALPHA`、深度写关、`cullBack=false` 双面），
  resize 经同一函数自动重建。
- `cMODEL::DrawPBRSorted(cameraPos)`：OPAQUE/MASK 原顺序 → BLEND 按网格世界 AABB 中心到相机距离远到近（仅网格级排序）。
- `sMESH::DrawPBRLevel(..., bool forceBlend=false)`；glTF `alpha_mode=BLEND → eALPHA_BLEND`（不再折 MASK）。
- `gn3d_pbr.frag`：`outColor = vec4(mix(fog.rgb,color,...), texture(texSampler,fragUV).a * pc.diffuse.a)`（雾只混 rgb），`.spv` 重生成。
- 查看器：对 `.png` 贴图用 `decodePngMemory` 统计半透明像素（0<a<1）占比 >5% ⇒ 该网格材质改 BLEND（仅 PNG，其它保持 MASK）；
  绘制改 `gModel.DrawPBRSorted(camera)`。
- 限制：BLEND 不做 alpha test、双面渲染可能过度绘制、排序仅网格级（同一网格内/跨网格相交仍有顺序误差）。

- **验收**：`make` exit 0；`make unit` = **371 cases / 0 failed**；`make mmdViewer`、`make gpuCheck` 编译通过；lint 仅存量。
  **GPU 未运行验证**：拖拽 resize、alpha 混合观感、PNG 5% 阈值。

## 2026-10-31 — 材质 alpha 导入（PMX/PMD/glTF）+ 前向 PBR discard；task-8 前向/MSAA/depth/HDR resize 完整重建（371 cases / 0 failed；无 GPU 运行验证）

### 材质 alpha（alpha-dev）
- `sMATERIAL` 新增 `eALPHA_MODE`（OPAQUE/MASK/BLEND）、`aAlphaMode`、`aAlphaCutoff`（默认 0.5，`SetDefault` 初始化）；
  `sASSET_MATERIAL` 同步；glTF 导入读 `alpha_mode`/`alpha_cutoff`（BLEND 暂折叠为 MASK，`TODO(alpha-blend)`）。
- MMD 桥接在 `mmdMeshBuilder.cpp`：每材质建 `sMATERIAL`（MASK + 0.5）挂 `mesh.aMaterial`，材质池归 `cMODEL::aMaterials`
  （ctor 置空、`Release()` 在网格后释放）；PMX/PMD loader 仅产出 `Pmx/PmdMaterial`，未改。
- `GN_VkDraw3DPBR` 追加默认参数 `alphaCutoff` → `pc.pbrParams[3]`（GLSL `pc.specular.w`）；`DrawPBRLevel` 仅 MASK 传 cutoff；
  `gn3d_pbr.frag` 加 `if (alpha < cutoff) discard`（cutoff=0 行为不变），`.spv` 重生成。
- `testAsset` 加 glTF MASK/BLEND/缺省与 `sMATERIAL` 默认值用例（369→371）。
- 限制：BLEND 未实现（透明混合/排序另做，当前硬边裁剪）；MMD 一律 MASK+0.5；deferred G-Buffer 路径仍无生产调用方按材质传 cutoff。

### task-8：resize 完整重建（g0-dev）
- `vk3dInit.cpp`：可重入 `create/destroy3DMsaaTargets`、`create/destroy3DForwardFramebuffers`、`recreate3DTargets`、
  `create3DForwardPipeline`/`create3DPbrPipeline`/`create3DSkyboxPipeline`；init 与 resize 走同一路径，修掉
  `s_framebuffers3D` 引用旧 swapchainView 的悬垂。
- `vk3dInitScene.cpp`：`create3DHdrImages`/`create3DHdrFramebuffers`/`destroy/recreate3DHdrTargets`、`writeCompositeHdrBinding`、
  `create3DCompositePipeline`/`recreate3DCompositeTargets`；`init3DHdr` 改走 helper。
- `GN_VkResize`（GN_Vk.cpp）：waitIdle → 销毁引用旧 view/旧尺寸的 fb（overlay/forward/composite/deferred/HDR）+ HDR 目标 →
  swapchain → 重建 → MSAA/depth → 3D fb → HDR → G-Buffer/延迟/SSAO/SSGI → composite → 固定视口管线（前向/PBR/天空盒重载 SPIR-V）。
  `vk3dShutdown` 复用同一 destroy helper。
- 限制：`GN_VkResize` 仍无调用者（无 GLFW 回调接线）；resize 时 SPIR-V 缺失只告警并沿用旧管线；
  材质/蒙皮/粒子/布料等其它固定视口管线未纳入本次 resize。

- **验收**：`make` exit 0；`make unit` = **371 cases / 0 failed**；`make gpuCheck`、`make mmdViewer` 编译通过；lint 仅存量。
  **GPU 未运行验证**：alpha discard 观感、resize 运行时/验证层。

## 2026-10-30（十五） — 修复 MMD 地面「暗带随相机滑动」（IBL 面接缝 + 掠射角 kD 归零）

- 现象：MMD 查看器地面出现一块随相机位置滑动的暗带（像影子），关 GI 后仍隐约可见。
- 根因 1（环境图面接缝）：`vk3dInitScene.cpp` 的静态天空盒 cubemap 每面都用「面内局部 y」做渐变，
  相邻面颜色不连续；地面镜面 IBL 采样到接缝，接缝被投影成随视角滑动的暗带。
  修复：每个 texel 先算**世界方向**（Vulkan cube 面映射）再做同一套渐变；地平线以下取地平线色（下半球不再发黑）。
- 根因 2（掠射角能量项）：`gn3d_pbr.frag` 两个 IBL 分支都用 `kD = 1 - Fresnel(NdotV)`，相机越低/地面越远
  NdotV→0、F→1，`kD→0` ⇒ 那片地面环境漫反射整块变暗，暗区随相机滑动。
  修复：改为 `kD = (1 - F0) * (1 - metallic)`（常见 IBL 近似）；另加 `iblReflection`（朝上表面把反射抬到地平线以上）。
- 影响：MMD 查看器地面恢复均匀；GI 开/关两个分支都改。`.spv` 已重生成。
- 验证：`glslangValidator` exit 0、`make` exit 0、`make unit` = 369/0、`make mmdViewer` 编译通过；**用户桌面复测：已解决**。

## 2026-10-30（十四） — P12 G0b：G-Buffer 四通道调试视图 + alpha cutoff 参数（369 cases / 0 failed；无 GPU 运行验证）

- 新增 `assets/shaders/gn3d_gbuffer_debug.frag`（全屏三角形，复用 `gn3d_deferred_light.vert`；bindings 0=SceneUBO、
  1=NormalRough、2=AlbedoMetal、3=AO/材质、4=Depth、9=GI UBO；push constant `channel`）：
  0=法线、1=albedo、2=AO/材质 RGB、3=世界距离伪彩（深度→世界位置→相机距离，50 m 归一）；`.spv` 已生成。
- 新增 `_Vulkan/vkGbufferDebug.cpp`（102 有效行）：调试管线（复用 deferredLight 的 renderPass/描述符布局/描述符集/vert，
  经 aPipelineCache）、init/destroy、resize 按新 extent 重建；`GN_VkDebugGBuffer(int)` 在 3D pass 外 begin deferredLightRenderPass(clear)
  → bind → push channel → draw 3 → end；通道越界钳制 0..3；3D pass 内调用兜底返回。
- `GN_VkDraw3D_GBuffer` 追加默认参数 `float alphaCutoff = 0.0f`，写 `pc.pbrParams[3]`（原 0，现有调用行为不变）；
  `gn3d_gbuffer.frag` 已有 `if (alpha < cutoff) discard`，本次只接参数。
- **验收**：`make` exit 0；`make unit` = **369 cases / 0 failed**；debug shader `glslangValidator` exit 0；
  `make gpuCheck` 编译通过（运行 SKIP，无设备）；lint 写域 0 违规。**GPU 未运行验证**：调试视图渲染/四通道正确性、alpha discard。
- 限制：调试视图会覆盖当前 swapchain 画面（仅适合单帧调试/截图）；复用延迟描述符集，需当帧已跑过延迟路径；
  深度伪彩 50 m 硬编码；alphaCutoff 未接材质 alpha 导入（PMX/glTF）。

## 2026-10-30（十三） — MMD 查看器：模型改前向 PBR + 开启 GI IBL（369 cases / 0 failed；无 GPU 运行验证）

- 新增 `GN_VkEnableGI(bool)`（`vkGi.cpp`、`GN_Vk.h`）：map GI UBO 原地读改写 `indirect[0]/[1]`（漫反射/镜面倍率），
  保留 G1a/G3 写入的 SH9/envParams/invViewProj；GI UBO 不存在时静默返回。
- `tests/mmdViewer/mmdViewer.cpp`：模型绘制 `cMODEL::Draw()` → `DrawPBR()`（GGX/Cook-Torrance，材质 diffuse/roughness/metallic/贴图）；
  `AppInit()` 内 `loadModel()` 后调一次 `GN_VkEnableGI(true)`；绘制顺序/天气/相机/粒子/贴花未动，未加按键。
- **范围**：按用户裁决 A——只开前向 PBR + GI IBL；SSAO/SSGI/探针只存在于延迟路径，MMD 查看器无 G-Buffer，本轮不接。
- **验收**：`make` exit 0；`make unit` = **369 cases / 0 failed**；`make mmdViewer`、`make gpuCheck` 编译通过；lint 写域 0 违规。
  **GPU 未运行验证**：PBR 观感、GI 亮度/高光、天气切换后 IBL 与天空同源、验证层均待桌面。
- 风险：Blinn-Phong→PBR 观感变化（高光形状/强度、metallic 色调、整体亮度未标定）；GI 开启后环境项由 split-sum+SH9 提供，
  若 G1a 初始化失败仍强开可能异常（建议桌面先确认启动无 G1a 警告）；旧 Skybox IBL 回退分支在 GI 关时仍可用。

## 2026-10-30（十二） — P12 G5b-2：探针 9 张 3D 纹理 + 延迟 layout 23 + shader 采样（369 cases / 0 failed；无 GPU 运行验证）

- `_Vulkan/vkProbe.cpp`（272 有效行）：9 张 `VK_IMAGE_TYPE_3D` RGBA16F 8×4×8（SAMPLED|TRANSFER_DST）+ LINEAR/CLAMP sampler；
  探针 UBO 48 B host-visible；启动读 GI UBO 的 SH9（偏移 48、36 float）填满 256 探针 → `packProbeTextures` → half →
  单 staging 18 KB → 9 次 buffer→3D image copy（一次 fence）；init/destroy。
- 延迟 layout 14→23：14=probe UBO、15..23=9 张 3D 纹理；`gn3d_deferred_light.frag` 新增 probe UBO + 9×sampler3D、
  `sampleProbeSh(wp,N)`（世界→格坐标→`(g+0.5)/counts` CLAMP 三线性 + SH9 基函数）；仅 `probe.weight.x>0` 时与 `evalSH9(N)` mix，
  weight=0 不采样、逐像素等同现状；`.spv` 重生成。
- `GN_VkSetProbeWeight(float)`（clamp [0,1]，默认 0）+ `GN_VkUpdateProbes()`（weight=0 返回；否则每帧最多 2 探针 Blend 0.15，
  有变化才整网格重传）；`GN_VkInit3D` init、`vk3dShutdown` destroy；尺寸固定不随 resize 重建。
- `tests/gpu/gpuCheck3D.cpp` 延迟用例补 `GN_VkUpdateProbes()` 调用点（weight=0 no-op；weight 需应用侧设置）。
- **验收**：`make` exit 0；`make unit` = **369 cases / 0 failed**；`glslangValidator` deferred shader exit 0；
  `make gpuCheck` 编译通过（运行 SKIP，无设备）；lint 写域 0 违规。**GPU 未运行验证**：3D 纹理上传/采样、描述符/布局、
  验证层、视觉（weight>0）、性能均待桌面。**GI 规划 G0–G5 代码级实现至此全部落地。**

## 2026-10-30（十一） — P12 G5b-1：probe UBO 契约 + 9 张探针纹理打包（369 cases / 0 failed）

- `giParams`：`sPROBE_UBO` 48 B（origin@0、spacingCounts@16（spacing/countX/Y/Z）、weight@32（0=关），偏移全 static_assert）
  + `makeDefaultProbeUbo`（8×4×8、间距 2 m、origin 0、weight 0）+ `packProbeUbo`；`testGi` +5 用例。
- `GN_3Dprobe`：`GetProbeSh(index, out36)`（边界/空指针校验）+ `packProbeTextures(grid, out)`——按裁决 A 输出
  **9 张** RGBA16F 8×4×8 的 float 层布局 `[k][z][y][x][c]`（默认 9216 floats ≈72 KB，36 float/探针无损；索引公式注释）；
  `testProbe` +4 用例（常数网格、单探针 texel、打包→还原无损、非法输入）。
- **设计修正**：§7.1 原「3 张纹理」与「36 float/探针」算术矛盾（3072 vs 9216 floats），已改为 **9 张纹理**、延迟描述符 13→23
  （14=probe UBO、15..23=9 张纹理）；G5b-2 未开始（VkImage/描述符/shader 采样/每帧更新）。
- **验收**：`make` exit 0；`make unit` = **369 cases / 0 failed**；lint 写域 6 文件 0 违规；纯 CPU，无 GPU 依赖。

## 2026-10-30（十） — P12 G5a：SH9 探针网格 CPU 基座（GN_3Dprobe + testProbe；360 cases / 0 failed）

- 新增 `src/graphics/GN_3Dprobe.h/.cpp`（140 有效行，纯 CPU）：`sPROBE_GRID_DESC`（默认 8×4×8、间距 2 m、总数 ≤1024）；
  `cPROBE_GRID`：`Init`（counts/spacing 校验）、`Reset`、`Recenter(cameraPos)`、`SetProbeSh(index, sh9[9][4])`、
  `BlendProbeSh(index, target, alpha)`（时间滞后）、`SampleSh(worldPos, outSh9)`（世界→格坐标→夹紧→8 邻域三线性）。
  SH9 为 36 float（9×vec4），与 `giParams` 布局一致；未接 Vulkan（G5b）。
- **Recenter 语义**：位移 ≥ 2×spacing（默认 4 m）才按整数格搬移，重叠探针 SH9 保留、越界夹到最近旧值（不丢数据）；
  相机 3.9 m 不搬、4.1 m 搬（测试锁定）。若后续要「每格对齐」，改一处阈值常量。
- 新增 `tests/unit/testProbe.cpp`：10 用例（默认/非法 desc、采样常数/中点/角点/越界夹紧、Recenter 3.9/4.1 与 SH9 保留、
  Blend 单调、非法输入与确定性）。
- **验收**：`make` exit 0；`make unit` = **360 cases / 0 failed**；lint 写域 3 文件 0 违规。纯 CPU，无 GPU 依赖。
- 风险：网格对象约 147 KB（1024×36 float），栈上实例化需注意；探针默认全 0，未接遮挡 raycast 与 GPU 上传/采样；
  G5b 才决定 3D 纹理布局（9 系数分 3 张 RGBA 或 SSBO）。

## 2026-10-30（九） — P12 G4：半分辨率 SSGI + 5×5 单帧双边去噪 + 延迟合成（默认关；350 cases / 0 failed；无 GPU 运行验证）

- **CPU 契约**：`giParams` 新增 `sSSGI_UBO` 160 B（viewProj@0 / invViewProj@64 / params@128（强度/步数/厚度/距离）/
  screen@144（invW/invH/半分辨率宽高），static_assert 全锁）+ `makeDefaultSsgiUbo`（强度 0=关、8 步、厚度 0.5、距离 20）
  + `packSsgiUbo`；`testGi` +5 用例。
- **着色器**：新增 `gn3d_ssgi.frag`（半分辨率、depth+invViewProj 重建世界位置/法线、8 rays×8 steps 世界空间 march、
  hit=G-Buffer albedo×`evalSH9`、miss=`evalSH9`）与 `gn3d_ssgi_denoise.frag`（5×5 单帧双边，深度/法线/空间权重）；
  `gn3d_deferred_light.frag` layout 13→14（binding 13=denoised SSGI），`diffuseIBL = mix(envDiffuse, ssgi, gi.envParams.w)`，
  `envParams.w=0` 时逐像素等同现状。三个 `.spv` 已重生成。
- **资源/接线**：新增 `_Vulkan/vkSsgi.cpp`（414 有效行：两张半分辨率 RGBA16F raw/denoised、两条 render pass（final=SHADER_READ
  + 显式依赖）、两条全屏管线、SSGI/denoise 两套描述符、160 B host-visible UBO、init/destroy/recreate）；
  `GN_VkSSGIPass()`（`envParams.w<=0` 直接返回并先 end 3D pass 兜底），在 SSAOPass 之后、DeferredLighting 之前调用；
  init/resize/shutdown 接线；延迟 layout 14。
- **调用点补齐（Lead）**：`tests/gpu/gpuCheck3D.cpp` 的延迟用例在 `DeferredLighting` 前插入 `GN_VkSSAOPass()`/`GN_VkSSGIPass()`
  （默认关为 no-op），否则仓库内无人调用这两个 pass。
- **验收**：`make` exit 0；`make unit` = **350 cases / 0 failed**；三个 shader `glslangValidator` exit 0；
  `make gpuCheck`/`make mmdViewer` 编译通过；lint 写域 0 违规。**GPU 未运行验证**：SSGI 视觉（渗色/漏光）、去噪质量、
  验证层、性能（8×8 半分辨率）、resize 均待桌面；步数固定 8、镜面不做 SSGI、亮度尺度未标定。

## 2026-10-30（八） — P12 G3：环境与解析天空同源 + 天气切换重烘（345 cases / 0 failed；无 GPU 运行验证）

- **只读天空参数**：`GN_VkGetSky(VkSkyParams*)`（`vk3dPasses.cpp:177`、`GN_Vk.h:512`），供烘焙读取运行时天气/时间。
- **可重入烘焙**：`vkEnvironment.cpp` 拆为「一次性资源 + `bakeAndUploadEnvironment(sky)`」——逐面重烘 6 面 → 回读 →
  `prefilterCubemap`+`shProjectCubemap` → **新建**预滤波 cubemap 并上传（全部成功才换句柄、重写 PBR binding8 / deferred binding10）→
  只更新 GI UBO 的 SH9（保留 indirect/aoParams/invViewProj）；失败销毁新图、旧图/旧 SH9 不动。
- **公开入口**：`GN_VkRebakeEnvironment()`（`vkDeviceWaitIdle` + pass 内拒绝返回 false；资源缺失返回 false）。
- **查看器**：`mmdViewerWeather.cpp` 在天气目标变化后置 pending，等 `isTransitioning()` 结束、在 AppRun（3D pass 外）重烘一次；
  启动首帧一次；失败清 pending + 告警，绝不每帧重试。timeSeconds/风等连续变化不重烘（动态 IBL 后续项）。
- **验收**：`make` exit 0；`make unit` = **345 cases / 0 failed**；`make mmdViewer` 编译通过；
  `make gpuCheck` 编译通过、运行 SKIP（无设备）；lint 写域 6 文件 0 违规。
  **GPU 未运行验证**：重烘时序、切 1..6 天气后天空/反射同源、验证层、启动与切换耗时（128² CPU 预滤波约 25M 采样/次，
  切换时有一次 `vkDeviceWaitIdle` 卡顿风险）。

## 2026-10-30（七） — P12 G2b：SSAO（R8 目标 + 世界空间 SSAO + 4×4 双边 + 合成）+ Mat44Inverse 修复（345 cases / 0 failed；无 GPU 运行验证）

- **CPU 契约**：`giParams` 新增 `sSSAO_UBO`（400 B：viewProj@0 / invViewProj@64 / samples[16]@128 / params@384，全部 static_assert）、
  `makeDefaultSsaoUbo`（单位阵、16 点核、半径 0.5/偏置 0.025/幂 1.0/启用位 0）、`packSsaoUbo`；`testGi` +6 用例。
- **修复既有缺陷** `GNg::Mat44Inverse`：旧实现对纯平移矩阵返回错误（`testSkeleton.cpp:46`、`vkCloud.cpp:11` 早已注明）；
  重写为正确列主序 4×4 逆，`testMath3D` +4 用例（平移逆、TRS roundtrip、透视 roundtrip、奇异→单位阵）。
- **着色器**：`gn3d_ssao.frag` 重写为世界空间（invViewProj 重建位置、世界法线、16 点核、range check、power、固定哈希切向）；
  新增 `gn3d_ssao_blur.frag`（4×4 双边，深度差 + 法线夹角降权）；`gn3d_deferred_light.frag` layout 12→13
  （binding 12 = blurAO）、`ao = materialAO * mix(1, ssao, indirect.w)`、`wpFromDepth` 改用 `gi.invViewProj`；三个 `.spv` 重生成。
- **`_Vulkan/vkSsao.cpp`（新，381 有效行）**：两张全分辨率 R8（raw/blur）目标与视图、两条 render pass（final=SHADER_READ + 显式依赖）、
  两条全屏管线（复用 `gn3d_deferred_light.vert`）、SSAO/blur 两套描述符、400 B host-visible UBO、init/destroy/recreate。
- **接线**：`GN_VkSSAOPass()`（`indirect.w<=0` 直接返回；先 end 3D pass）；`GN_VkBegin3DPassDeferred` 记录 viewProj；
  `GN_VkDeferredLighting` 每帧用修复后的 `Mat44Inverse` 写 GI UBO offset 192；resize 重建、shutdown 释放。
- **验收**：`make` exit 0；`make unit` = **345 cases / 0 failed**；三个 shader `glslangValidator` exit 0；
  `make gpuCheck` 编译通过、运行 SKIP（无设备）；lint 写域 0 违规。**GPU 未运行验证**：SSAO 视觉/模糊质量/验证层/resize；
  默认 `indirect.w=0`（不跑 pass、观感不变），桌面用 `GN_VkSetGI` 设 `indirect.w=1` 验证。
- 风险：`invViewProj` 每帧覆盖写 GI UBO（帧内 `GN_VkSetGI` 会临时覆盖，下一帧恢复）；blur 深度权重为未标定经验值；
  两张 R8 常驻约 4 MB。

## 2026-10-30（六） — P12 G2a：G-Buffer 打包 24→16 B/px（albedo/Metal RGBA8_SRGB + AO/材质 RGBA8；335 cases / 0 failed；无 GPU 运行验证）

- `src/graphics/_Vulkan/vkGbuffer.cpp` 颜色格式表改为 `{RGBA16F, RGBA8_SRGB, RGBA8_UNORM}`（深度 D32 不变），
  颜色附件 24→16 B/px（1080p 约 50→33 MB）。
- `assets/shaders/gn3d_gbuffer.frag`：附件 1 写线性 albedo（RGBA8_SRGB 由硬件编码）+ metallic；
  附件 2 按 §5.4 写 `(AO, reflectance=0.04 占位, materialId=0 占位, alpha)`；alpha cutoff 预留 `pc.specular.w`
  （C++ 侧 `pbrParams[3]=0`，默认不 discard，材质 alpha 通路留 G0b）。
- `assets/shaders/gn3d_deferred_light.frag`：仅通道注释更新（RGBA8_SRGB 采样自动 linear；AO 仍取 .r）。
- `tests/gpu/gpuCheck3D.cpp` 的 `3d.deferred.gbuffer` 改按 RGBA8 读附件 1（4 B/px）、`GN3d::srgbDecode` 还原线性、容差 ±0.01。
- `.spv` 已重生成：`gn3d_gbuffer.frag.spv` 3240→3716 B（功能变化）；`gn3d_deferred_light.frag.spv` 仅注释/行号差异。
- **验收**：`make` exit 0；`make unit` **335 cases / 0 failed**；两个 shader `glslangValidator -V` exit 0；
  `make gpuCheck` 编译通过、运行 SKIP（无设备）；lint 写域 0 违规。**GPU 未运行验证**：RGBA8_SRGB 附件兼容/验证层、
  8-bit 精度、带宽收益、gpuCheck 运行时断言均待桌面。

## 2026-10-30（五） — P12 G1b：GI 槽位 4/7/9 + split-sum/SH9 着色器接线（默认 GI 关；335 cases / 0 failed；无 GPU 运行验证）

- **描述符槽位**（与天气 3/6/8 不冲突，两份计划的槽位表已同步）：前向 `4 = GI UBO`（layout 4→5，shader 暂未用）；
  PBR `7 = GI UBO`、`8 = 预滤波环境 cubemap`、`9 = BRDF LUT`（layout 7→10）；
  延迟 `9 = GI UBO`、`10 = prefiltered cubemap`、`11 = BRDF LUT`（layout 9→12）。
- 新增 `_Vulkan/vkGi.cpp`：`init3DGiBindings()` 写三套描述符；启动 `disableGiByDefault()` 把
  `indirect[0]/[1]` 置 0（保留 G1a 的 SH9/envParams），`GN_VkInit3D` 末尾接线（`GN_Vk.cpp:973`）。
- 着色器：`gn3d_pbr.frag` / `gn3d_deferred_light.frag` 增加 GI UBO + `evalSH9` + split-sum（预滤波 `textureLod` ×
  BRDF LUT `F0*ab.x+ab.y`）；`.spv` 已重生成（27004 B / 27680 B）。GI 关时走**逐行保留的旧 Skybox IBL 分支**
  （旧 0.7/0.5/×3.0 魔数保留在回退分支），默认观感不变；`GN_VkSetGI` 把 `indirect.x/y` 调到 >0 才启用新路径。
- **验收**：`make` exit 0；`make unit` **335 cases / 0 failed**；两个改动 shader `glslangValidator -V` exit 0；
  lint 写域 0 违规（全仓仅存量）。**GPU 未运行验证**：描述符兼容/验证层、GI 关回退画面、GI 开启亮度与方向、
  新 .spv 运行时加载均待桌面；嵌入式 fallback 头（`*_spv.h`）未同步（仅 assets 下 .spv 缺失时才用到旧版）。
- 风险：若 G1a 烘焙失败，bindings 8/9/10/11 写的是空视图（默认分支不采样）；GI 开启后亮度可能需调倍率。

## 2026-10-30（四） — P12 G1a：GPU 6 面烘焙环境 + CPU 预滤波 + 预滤波 cubemap/BRDF LUT/GI UBO（335 cases / 0 failed；无 GPU 运行验证）

- **环境来源拍板 B**（`docs/GI_PLAN.md` §11）：v1 IBL 源 = GPU 6 面烘焙解析天空 → 回读 → `giParams::prefilterCubemap`
  CPU 预滤波 → 上传 mip 链；G3 只补「天气切换重烘 + GPU 端预滤波」。
- 新增 `_Vulkan/vkEnvironment.cpp`（681 物理 / 630 有效行）：128²×6 RGBA16F cubemap 烘焙目标 + 6 face 视图/
  专用 render pass/6 framebuffer；复用 `gn3d_skybox.vert/frag` 逐面烘焙（6 个 128 B UBO 描述符集，256 B 对齐步长）；
  staging + fence 回读 → `prefilterCubemap`（5 mip）+ `shProjectCubemap` + `generateBrdfLut`；
  上传 CUBE 预滤波图（LINEAR/CLAMP、mip 线性）+ RG16F BRDF LUT + 256 B GI UBO（真实 SH9 + mip 数）。
- `GN_Vk.h` 新增环境/BRDF/GI 句柄与 `GN_VkSetGI(const GN3d::sGI_UBO&)`；`GN_VkInit3D`（`GN_Vk.cpp:958`）接线，
  `vk3dShutdown.cpp:58` 释放；创建/销毁成对，失败只告警不阻断 3D 初始化。
- **验收**：`make` exit 0；`make unit` = **335 cases / 0 failed**；`glslangValidator -V gn3d_skybox.vert/frag` exit 0；
  lint 写域 0 违规（全仓仅存量 `GN_2Dmath.cpp` 违规 + spv 头 no-split 告警）。**GPU 未运行验证**：面朝向（首要风险）、
  回读、上传内容、GI UBO 读取、同步/验证层均待桌面；bake 当前用默认天空参数（`s_skyParams` 未接入，G3 天气重烘解决）。
- 未接着色器/描述符槽位（G1b 另做）。

## 2026-10-30（三） — 3D 计划 §0 #2：降水柱硬边（柱高 64 m + 寿命单趟化；代码级，视觉未验证）

- **降水柱 24 m → 64 m**（`tests/mmdViewer/mmdViewerWeather.cpp`）：`PRECIP_COLUMN_HEIGHT` 是唯一来源，
  发射平面（地面+柱高）、柱心、柱半高、`setRecycleBox` 与叠层 `gPrecipBoxCenter/Half` 全链由它推导；
  注释写明 64 m 的理由（柱顶/柱底落在常见相机距离与雾外）与密度代价（同粒子数摊在更长柱里，64/24≈2.67× 稀释）。
- **雨/雪寿命收紧为「单趟落地 + 余量」，修复加高柱后粒子半空寿终的新瑕疵**：
  粒子回收只 wrap 位置、不重置 age（`GN_3Dparticle.cpp:179-197`）。雨：落地 2.01–2.22 s ⇒ 寿命 2.3–2.6 s；
  雪：落地 ≈17.07 s ⇒ 寿命 17.5–18.0 s。寿终点离地约 49–63 m（雨）/ 60–62 m（雪），落在柱顶附近（预期雾/视距外）。
- **未改**：容量 16384/24576、发射率 60000/30000、雨雪尺寸与 streak；密度/观感真机再调。
- **验收**：`make mmdViewer` exit 0；`make unit` **335 cases / 0 failed**；`lintCodeModularity tests/mmdViewer` 0 违规 0 告警。
  **运行期未验证**（无 `/dev/dri`）：上下过渡是否连续、地面密度是否可接受均待桌面目视。

## 2026-10-30（二） — P12 G0a + G1-CPU：G-Buffer 结构修复 / 附件改名 / cubemap 预滤波（335 cases / 0 failed；无 GPU 运行验证）

- **G0a：G-Buffer 结构修复**（`docs/GI_PLAN.md` §5）。新增 `_Vulkan/vkGbuffer.cpp`（从 `vk3dInitScene.cpp`
  迁出创建逻辑，372 行）：`eGBUFFER_ATTACHMENT` 枚举（0=NormalRough / 1=AlbedoMetal / 2=AO / 3=Depth），
  附件句柄按枚举映射；`gAlbedo*/gNormal*/gPBR*` 错位字段改名为 `gNormalRough*/gAlbedoMetal*/gAo*/gDepth*`
  （全仓旧名 0 处，`tests/gpu` 同步）；render pass 加两条显式 `VkSubpassDependency`
  （EXTERNAL→0 读→写；0→EXTERNAL 写→FRAGMENT_SHADER 读）。`GN_VkResize` 现在销毁并重建
  G-Buffer 图像/视图/内存/framebuffer、重写延迟光照 binding 1..4、按新尺寸重建 G-Buffer/延迟管线，
  并在销毁 swapchainView 前先释放 overlay/composite/deferred 三类 framebuffer；`vk3dShutdown` 走统一 `destroy3DGbuffer()`。
- **G1-CPU：cubemap 预滤波 + SH9**（`giParams.h/.cpp` 740 有效行，多功能 <1000 机检通过）。
  新增 `prefilterCubemap`（6 面输入、GGX 64 样本、mip 主序、mip0 原图、粗级 A=1）、
  `shProjectCubemap`（与 equirect 同系数约定）、`cubemapMaxMipCount` / `prefilterCubemapFloatCount` /
  `prefilterCubemapMipOffset` / `cubemapFaceDirection`；`testGi` 16→21 用例（常数环境 mip 恒定、梯度方差逐级下降、
  cubemap SH 与 equirect SH 一致、确定性、非法输入不写脏数据）。
- **验收**：`make unit` = **335 cases / 0 failed**；`make` 与 `make gpuCheck` 编译通过
  （gpuCheck 运行到 `gpu.init` 因无设备 `SKIP`，exit 2）；`lintCodeModularity src/graphics tests/unit tests/gpu`
  仅存量 1 违规 1 告警。**G0a 的 resize/同步路径与 G1-CPU 的 GPU 表现均未运行验证**（本机无 `/dev/dri`）。
- **越界发现（未修）**：① `s_framebuffers3D`（`vk3dInit.cpp` 前向路径）仍引用已销毁 swapchainView——
  resize 后悬垂，需连同 `msaaColorView/depthView` 与 HDR 目标一起重建；② `hdrFramebuffers` 仍是旧尺寸；
  ③ composite/deferred 之外的固定视口管线（前向/HDR）在 resize 后未重建。

## 2026-10-30 — 接管：cCAMERA C0–C5 + GI CPU 基座 + mmdViewer 拆分（330 cases / 0 failed；无 GPU 运行验证）

- **cCAMERA（`docs/CAMERA_PLAN.md` C0–C5）**：新增 `src/graphics/GN_3Dcamera.h`（144 有效行）+
  `GN_3Dcamera.cpp`（150）/`Move`（194）/`Orbit`（87）/`Follow`（138）/`Fit`（139）/`Control`（35），
  纯 CPU 的自由/轨道/跟随三模式、GN 移动原语、临界阻尼跟随、自动取景、FOV 换算；`testCamera` 22 用例全 PASS。
  查看器相机段改为唯一 `GN3d::cCAMERA gCamera`（输入只填 `sCAMERA_INPUT`→`ApplyInput`，视图/投影走 `BuildViewProj`），
  新增 F 自由↔轨道、轨道滚轮缩放、中键拖动平移；默认视角与旧实现数值等价（`SetOrbit(target, h*1.9, 0, 0.12)`）。
  已知行为差异：Q/E 从世界竖直改为沿相机 up；俯仰上限 1.45→83°（差 0.08°）。
- **GI CPU 基座（`docs/GI_PLAN.md` G1 前置）**：新增 `src/graphics/giParams.h/.cpp`（474 有效行）+
  `tests/unit/testGi.cpp`（16 用例）：256 B `sGI_UBO`（偏移/大小 static_assert）、默认/校验/打包、
  16 点半球 SSAO 核（Halton + 固定种子切向）、SH9 equirect 积分与 `evalSh9`、128²×64 样本 GGX BRDF LUT、
  albedo sRGB / 材质位域 / octahedral（float + snorm16）/ alpha cutoff。**未接 Vulkan**（G0/G1-GPU 未开始）。
- **`mmdViewer.cpp` 拆分（3D 计划 §0 #8）**：1385 物理 / 1164 有效行 → `mmdViewerCamera.cpp`（182）、
  `mmdViewerWeather.cpp`（251）、`mmdViewerDecal.cpp`（174）、`mmdViewerOverlay.cpp`（262）、
  `mmdViewer.cpp`（319）+ `mmdViewerShared.h`（71）；全部 <500 有效行，`lintCodeModularity tests/mmdViewer` = 0 违规 0 告警。
- **验收**：`make unit` = **330 cases / 0 failed**；`make` 与 `make mmdViewer` 编译通过；
  `lintCodeModularity src/graphics tests/unit` 新增文件 0 违规（仅存量 `GN_2Dmath.cpp` 超长与 spv 头 no-split）。
  **运行期未验证**：本机无 `/dev/dri` 且未装 lavapipe，`make gpuCheck` 无法运行，画面/手感/轨道切换待桌面验收。
- **越界发现（未修）**：`GN_3Dmath.cpp:381` `Mat44OrthoVK` 的 `m[10] = -fn` 与 VK [0,1] 深度符号不符
  （n=0.1/f=1000 时 near→-2e-4、far→-1.0002），正交相机深度不可用；正交取景/C5 收尾前需先修。
- **收尾备份未完成**：`/run/media/ruicat/*` 两个外部盘均为只读挂载，异盘备份无法落盘（详见 3D 计划 §0 #9）。

## 2026-10-30 — 正面绕序修正 + MMD 法线 + 降水/云/贴花拿到真矩阵（gpuCheck 99/99，291 cases）

- **世界 pass 的 `frontFace` 修正为 CCW**（`GN_Space.cpp`、`vk3dInit.cpp`、`vk3dInitScene.cpp`、`vkMaterial.cpp`）：
  旧 U2b 把「Y 翻转 ⇒ CW」写反了。正高度视口 + 投影 Y 翻转下，世界 CCW 内容在帧缓冲仍是 CCW，
  含 Y 翻转的 3D pass 必须 CCW；只有不翻 Y 的阴影 / identity 用例用 CW。
  证据：llvmpipe（CPU Vulkan）`make gpuCheck` 旧规则 **78 通过 / 18 失败**，修正后 **99/99**。
- **MMD 模型法线反**（`src/mmd/mmdMeshBuilder.cpp`）：镜像 X 会反转绕序，而原始 PMD/PMX 的绕序与顶点法线一致，
  只镜像不反绕序 ⇒ 模型内外翻转、光照像法线反了。两个 builder 现在镜像后再 `reverseTriangleWinding`。
  实测 `mode.pmd`：原始 agree=6189/disagree=40；修复前镜像后 40/6189；修复后 6189/40。
  新增单测 `mmdPmdMirrorKeepsWindingConsistentWithNormals`。
- **降水/云/贴花画不出来的根因**（`tests/mmdViewer/mmdViewer.cpp`）：调用点写成
  `drawPrecipitation(&viewProjection, …)` / `drawClouds(&viewProjection, …)` / `drawDecals(&viewProjection)`，
  而这三个函数收的是 `const sMAT44&`；指针被 `sMAT44(bool)` 隐式转成 `bool` 再构造成**单位矩阵**，
  于是世界坐标被当作裁剪空间坐标：绝大多数粒子被裁掉、剩下的画在错误位置（看起来「往上走」、不在包围盒里）。
  修正为传矩阵本身；并把 `sMAT44(bool)` 改成 `explicit`，让同类错误在编译期报错。
- **V 键粒子包围盒只画雨**（`mmdViewer.cpp`）：改为雨、雪两套系统各画最近的 16 个。
- **查看器启动天气开关**：`MMD_VIEWER_WEATHER=snow|rain|fog|storm|cloudy|clear`（便于脚本化复现）。
- **降水密度与分层**：池容量 61440/81920 → **16384/24576**、发射率 360000/150000 → **60000/30000**
  （池容量才是屏幕密度上限；原先过密）。同时修发射器满池逻辑（`GN_3Dparticle.cpp`）：
  旧实现池满时把待发射积压钳到 1，存活数被寿命抽干到近空再瞬间灌满，表现为**分层/空白间隔**；
  现在保留 0.1 s 额度，槽位一释放就补齐。单测 `particleFullPoolStaysSteady` 锁定
  （稳态 min=max=容量）。
- **gpuCheck 新增回归**：`precipitation.snowCurtain`（81920 颗、65 m 外）、
  `precipitation.orientation`（世界 -Y 必须落在屏幕下半）、`precipitation.screenAgreement`
  （CPU 投影与渲染像素质心一致，实测 expect=actual=(255,222)）。

## 2026-10-29 — 竖直方向一致性修正 + 四元数自由视角 + 包围盒/坐标轴（95/95，275 cases）

- **降水/贴花竖直方向**（`_Vulkan/vkPrecipitation.cpp`、`_Vulkan/vkDecal.cpp`）：两条管线的视口原来是
  正高度，与场景（PBR 用 `y=H, height=-H` 的负高度视口）相反 ⇒ 雨丝沿速度方向朝上、贴花投到视线上方。
  现在统一为负高度视口，降水与场景同向。
- **地面法线**（`tests/mmdViewer/mmdViewer.cpp`）：地面顶点法线改为 `(0,-1,0)`——本引擎竖直方向是镜像的
  （MMD 左手系模型恰好抵消），朝上的地面要按 -Y 传，否则光照把地面当背面而发黑。
- **自由视角改用四元数**（`tests/mmdViewer/mmdViewer.cpp`）：`gFreeRotation`（`GNg::sQUAT`）累积视角，
  鼠标水平位移绕**世界 +Y**、垂直位移绕**相机右轴**，`pitchQuat * yawQuat * 当前朝向` 左乘后归一化，
  前向/右向/上向由 `QuatRotateVec3` 旋转基向量得到（不做欧拉角累加，无万向锁/roll 漂移）；
  W/S/A/D 沿朝向平移、Q/E 沿世界竖直升降、Shift 加速、左/右键拖动转视角。
- **包围盒 + 坐标轴可视化**（新增，G 键切换）：`GN_VkDrawDebugLines` 画原点坐标轴
  （+X 红 / +Y 绿 / +Z 蓝，长度 0.6×模型高）与模型世界 AABB 的 12 条棱（黄色线框）。
- 验收：`make unit` = 275 cases 0 failed；`make gpuCheck` = 95/95 PASS；`build/mmdViewer` 编译通过。

## 2026-10-29 — 降水形状 v2 + 地面 + 贴花系统（94/94，275 cases）

- **降水形状 v2**（`_Vulkan/vkPrecipitation.cpp` + `precipitation.vert/frag`）：不再是实心长方形面片——
  顶点带 uv（横向 0..1、纵向 0=头/1=尾），片元按 `aStreakScale` 选形状：
  雨（>1.5）细芯 + 头部亮/尾部淡的雨丝，雪（<=1.5）径向软斑；雨丝取向改为**粒子速度方向**
  （有风会自然倾斜），静止粒子才回退屏幕竖直。顶点缓冲改为 4 槽（帧奇偶 × 帧内调用序号），
  同帧可分别提交雨和雪（各自的 streak），查看器不再需要合并粒子。
- **地面**（`tests/mmdViewer`）：模型世界 AABB 最低点铺一块 400 m 正方形，走既有 PBR 管线
  （不新增管线/着色器），先于模型绘制，作为贴花与降水的深度基准。
- **贴花系统**（新）：`src/graphics/GN_3Ddecal.h/.cpp` 纯 CPU 池——固定容量环形写（池满覆盖最老）、
  `aLife<=0` 为永久贴花、最后 30% 寿命线性淡出、`gatherDecalRenderItems` 转渲染条目；
  `_Vulkan/vkDecal.cpp` + `gn3d_decal.vert/frag`——世界 XZ 平面四边形，`SPLASH_RING` 画环、
  其它画软圆斑，alpha 混合 / 深度测试开（LESS_OR_EQUAL）/ 深度写关；`GN_VkDrawDecals` /
  `GN_VkDecalDestroy`（契约写入 `GN_Vk.h`，`gn3dShutdown` 接线）。
  查看器按雨强在角色周围地面生成水渍/环（220 条/s、单帧上限 64、寿命 0.5–1.1 s）。
- 测试：`testDecal` 7 用例（容量/参数校验、寿命回收、永久贴花、池满覆盖最老、末段渐隐、渲染条目转换）；
  `gpuCheck` 新增 4 条（`decal.outsidePass/empty/draw/pixels`，俯视相机 + 真实透视矩阵）。
- 验收（Lead 实跑）：`make unit` = **275 cases 0 failed**；`make gpuCheck` = **94/94 PASS**；
  lint 10 文件 0 违规 0 告警；`build/mmdViewer` 编译通过。
- 已知限制：贴花只贴水平地面（不做任意表面投影/法线朝向）；雪不生成地面水痕；地面是单色 quad，
  未做纹理/法线贴图；贴花池 512，超出覆盖最老。
- 修正（2026-10-29，用户目视反馈驱动）：
  ① **地面整片不可见**——地面四边形的绕序（0,2,1 / 0,3,2）与管线的背面剔除方向相反，被整块剔除，
  角色看起来浮在空中；已改为 0,1,2 / 0,2,3，并新增 `ground.placement` 回归检查（在 60° 透视相机下
  统计画面中列最亮行，断言落在下半屏，实测 63% → PASS）。
  ② 地面改走**前向 3D 管线**（同一条已验证路径），颜色提亮到 (0.42,0.43,0.40)。
  ③ 云层 `coverage×0.8`、`opacity=0.85`：暴风雨下三层云盘不再叠成一整块灰"天花板"（此前容易误看成地面翻到天上）。
  ④ 查看器新增**自由视角**（F 切换）：W/S/A/D 平移、Q/E 升降、Shift 加速、左键拖动转视角；轨道视角保持不变。
  ⑤ 查看器启动日志新增模型世界 AABB 与地面高度打印（`AABB y=[-0.081, 17.946]`，地面 y=-0.10）；
  地面高度取模型 AABB 最低点，实测与脚底一致。

## 2026-10-29 — P11-S2b/S4/S5/S6 集成：云层 / 粒子池扩展 / 降水 / 查看器全天气（90/90）

- **S2b 中景云层**：新增 `_Vulkan/vkCloud.cpp` 与 `gn3d_cloud.vert/frag`；`sCLOUD_DRAW_PARAMS`
  在 3D pass 内提交 `aTilesPerAxis² × 3 层` 面向相机的 billboard 四边形（tile 网格按相机 xz 取模对齐、
  按 `aWind × aTimeSeconds` 漂移、近处按 `aNearFade` 淡出），片元用 5 层高频 FBM 程序化 alpha
  （`aCoverage` 截断、`aSharpness` 控边、每层不同 coverage 偏置与明暗 ⇒ 云层分层不糊）；
  参数非法 / 3D pass 外调用返回 false。
- **S4 粒子池扩展**：`GN_3Dparticle.h/.cpp` 的 `sPARTICLE_EMITTER_DESC` 增加 BOX 形状
  （`aBoxHalfExtent`）、尺寸范围（`aSizeMin/aSizeMax`，独立随机流）、风场
  （`aWind/aWindResponse`，`v += (wind - v) * min(1, response*dt)`）与
  `setRecycleBox` 相机跟随循环回收；旧描述不设新字段时行为逐位不变（`particleLegacyDescUnchanged`）。
- **S5 降水管线**：新增 `_Vulkan/vkPrecipitation.cpp` 与 `precipitation.vert/frag`；CPU 把活粒子
  展开成世界空间 billboard（雨滴沿世界 -Y 按 `aStreakScale` 拉长，雪 = 1），alpha 混合、
  深度测试开 / 深度写关；`GN_VkDrawPrecipitation` 复用单块动态顶点缓冲。
- **S6 查看器集成**：`tests/mmdViewer/mmdViewer.cpp` 的 `applyWeather()` 增加雾
  （`GN_VkSetFog`：颜色 + density/heightFalloff/baseHeight，maxOpacity=0.9）；3D pass 内
  天空盒之后按 `weather.output()` 画云层（layerHeight 300 m、tileSize 256、8×8、nearFade 50）；
  创建雨/雪两套 `GN3d::cPARTICLE_SYSTEM`（BOX ±(16,10,16) 跟随相机，雨 1800/s、雪 900/s，
  风与尺寸随天气），每帧 `update(dt)` 后只在 3D pass 内提交一次 `GN_VkDrawPrecipitation`：
  雨雪强度同时 >0.05 时合并存活粒子（streak 取 6 折中），否则只提交强度更大的一类（雨 12、雪 1）。
  1..6 键语义不变，切换天气后云量/雾浓度/降水强度随过渡曲线变化。
- 验收（Lead 实跑）：`make gpuCheck` = **90/90 PASS**（AMD 780M / RADV，云与降水检查通过）；
  `tests/mmdViewer/mmdViewer.cpp` 带 `-DGN_USING_VULKAN_GLFW_LINUX` 的 `g++ -fsyntax-only` 通过；
  lint（写域）0 违规 0 告警。
- 已知限制：降水 v1 只有一块顶点缓冲，同一帧多次调用会互相覆盖 ⇒ 查看器每帧只提交一次；
  延迟光照路径的雾仍未做像素级验收（S3 遗留）；查看器全天气仅为桌面目视，未纳入自动化截图。
- 查看器后续修正（用户目视反馈驱动）：① 远平面下限提到 `CLOUD_LAYER_HEIGHT + 2*CLOUD_TILE_SIZE`
  （默认模型高度 18 m 时原 farZ≈232 m，300 m 云层被整片裁掉）；② 雨/雪初版粒径 0.05–0.10 m、
  alpha 0.55 在 15 m 外不足 2 px 且与天空对比过低（实测「看不见」）⇒ 雨加粗到 0.10–0.20 m / alpha 0.85，
  雪 0.16–0.32 m / alpha 0.95；③ 云层从单层改为 3 层云盘并把噪声频率提高约 3 倍（原单层低频 FBM 糊成一片）。
- 查看器降水体积修正（用户反馈「雨点不在角色身上」）：原回收盒以相机为心、半径 ±16 m，而查看器相机
  绕在角色 34 m 外 ⇒ 雨只存在于镜头附近、落不到角色上。现以**相机与角色中点为心**、xz 半径
  `max(26, 距离/2+8)`，生成盒固定 ±40×18×40，雨率 1800→3600/s、雪 900→1500/s（容量 6144/8192）。
- 新增两条 GPU 检查（88→90）：`precipitation.perspective`（真实 lookAt+透视矩阵下原点粒子落在画面中心）
  与 `precipitation.overGeometry`（先画不透明 PBR 四边形、再画更近的粒子 ⇒ 中心变红，证明半透明降水
  不会被深度测试剔掉）——两条都用于回答「到底有没有画、会不会被角色挡住」。

## 2026-10-29 — P11-S3 雾：独立 Fog UBO 接入三条 3D 片元管线（80/80）

- 新增 `_Vulkan/vkAtmosphere.cpp`：`sFOG_UBO`（color + density/heightFalloff/baseHeight/maxOpacity）
  的 host-visible UBO，`init3DAtmosphere` 在 `GN_VkInit3D` 最先创建，`GN_VkSetFog` 写入并钳制非法值；
  `density == 0`（默认）时雾量恒 0，既有画面不变。
- 描述符接线：前向 binding 3、PBR binding 6、延迟光照 binding 8（与 §3.6/§3.7 冻结槽位表一致）；
  `GN_VkShutdown3D` 增加 `destroyAtmosphere()`。
- 着色器：`gn3d.frag`（Blinn-Phong）、`gn3d_pbr.frag`、`gn3d_deferred_light.frag` 增加 FogUBO 与
  指数高度雾公式 `mix(fogColor, color, 1 - fogAmount)`，各自重新编译 `.spv`。
- 测试：`gpuCheck` 新增 3 条——`fog.pbr`（红色雾下中心像素转红）、`fog.pbrReset`（密度归零后
  恢复 ±0.03）、`fog.forward`（Blinn-Phong 路径同样受雾影响）。
- 验收：`make unit` = **262 cases 0 failed**；`make gpuCheck` = **80/80 PASS**（AMD 780M / RADV）。
- 边界：延迟路径的 binding 8 已接线并由描述符布局校验，但 G-Buffer 全链路的雾像素验收未做
  （v1 只验证前向与 PBR 两条路径）；雾颜色按线性空间混合，不做大气散射/太阳方向调制。

## 2026-10-29 — P10 动态 LOD：屏幕误差选级 + 分级绘制（77/77）

- 新增 `GN_3Dlod.h/.cpp`：`sLOD_VIEW`、`lodScreenError`（`S = err * H / (2*d*tan(fov/2))`，
  d = 相机到世界 AABB 最近距离，不可判定返回 +INF）、`selectLod`（由粗到细取最粗可接受级，
  变粗需 0.8× 阈值滞后）、`selectLodForModel`（按模型 AABB 批量选级）。纯 CPU、无 Vulkan 依赖。
- `GN_3Dmodel.h/.cpp`：新增 `sMESH_LOD`（顶点/索引/`aMaxError`）与 `sMESH::aLods/aLodCount/aBoundRadius`；
  `sMESH::DrawPBRLevel`（越界/无数据退回 LOD0）、`cMODEL::DrawPBRWithLod`；`DrawPBR` 改为
  `DrawPBRLevel(0)`（单级路径逐位一致）。
- `GN_Vk.h` 新增诊断钩子 `GN_VkLast3DIndexCount()`（`GN_VkDraw3DPBR` 记录最近一次 indexCount）。
- 测试：`testLod` 6 用例（误差公式、10/100/1000 m 选 0/1/2、滞后振荡 ≤2 次、相机盒内/非法视口、
  单级回归、模型批量选级）；`gpuCheck` 4 条（`lod.select` 0/1、`lod.indexCount` 36/6、
  `lod.singleLevel` 36、`lod.pixels` 两级中心像素一致）。
- 验收：`make unit` = **262 cases 0 failed**；`make gpuCheck` = **77/77 PASS**（AMD 780M / RADV）；
  lint 10 文件 0 违规 0 告警。
- 与草案偏差：选级显式接收世界 AABB；`aLods` 只存 CPU 数据（PBR 每次绘制动态上传，无需每级 VkBuffer）；
  验收距离改为按公式推导（原 2/20/200 m 与 1.0 px 阈值不自洽）。

## 2026-10-29 — P9 收尾：GPU 布料 / SSBO 蒙皮 / 绘制项批处理（73/73）

- **P9-a GPU 布料**：`GN_3DclothGPU.h/.cpp`（`sCLOTH_GPU_VERTEX` 32B、`sCLOTH_GPU_DESC`、
  `validateClothGpuDesc`、`stepClothCpu` 的 Verlet + Jacobi CPU 回退）；
  `_Vulkan/vkClothCompute.cpp`（A/B 双 host-visible SSBO 乒乓：积分 A→B、每轮约束翻转、
  结果不在 A 时 `vkCmdCopyBuffer` 回 A；单命令缓冲 + fence）；
  `_Vulkan/vkClothDraw.cpp`（顶点 pulling 网格 + Create 时生成索引缓冲）；
  `gn3d_cloth.comp/vert/frag`；`GN_VkClothCreate/Upload/Simulate/Data/Draw/Destroy`。
- **P9-b SSBO 蒙皮**：`_Vulkan/vkSkinSsbo.cpp` + `gn3d_skinned_ssbo.vert/frag`；
  `GN_VkDraw3DSkinnedSsbo`：调色板走 `mat4 palette[]` SSBO（4096 关节），3 个环形槽位，
  旧 128 关节 UBO 路径原样保留。
- **P9-c 批处理**：`GN_3Dbatch.h/.cpp`：`sortBatchItems`（pipelineKey→materialKey→depth→order
  稳定排序）+ `countBatchSwitches`；纯 CPU，不重构既有绘制路径。
- 接线：`GN_VkInit3D` 增加 `init3DSkinnedSsbo/init3DCloth`（失败只告警），
  `GN_VkShutdown3D` 增加 `destroySkinnedSsbo/destroyCloth`；描述符池 `STORAGE_BUFFER` 250→ 见 `GN_Vk.cpp`。
- 测试：`testClothGPU` 5 用例、`testBatch` 5 用例；`gpuCheck` 12 条新检查
  （`cloth.*` 5、`skin.ssbo.*` 4、`batch.*` 3）。
- 验收：`make unit` = **256 cases 0 failed**；`make gpuCheck` = **73/73 PASS**（AMD 780M / RADV，
  `cloth.compute` meanY 0 → -0.2153 且角点不动、`cloth.cpuMatch` 差 0、`cloth.draw` 中心
  `0.700,0.300,0.200`、`skin.ssbo.joint200` 像素 `0.572,0.254,0.127`、`batch.switches` 3 → 1）；
  新增文件 lint 0 违规 0 告警。
- 边界：布料只有结构距离约束（无剪切/弯曲/碰撞），每步 fence 等待；SSBO 蒙皮上限 4096 关节；
  批处理只排序不合并几何。CPU 回退（`stepParticlesCpu` / `stepClothCpu`）全部保留。

## 2026-10-29 — P9：GPU 粒子（compute 模拟 + SSBO + 实例化 billboard，61/61）

- 新增 `GN_3DparticleGPU.h/.cpp`：`sGPU_PARTICLE`（64B std430，四个 vec4）、`sPARTICLE_SIM_DESC`、
  144B `sPARTICLE_DRAW_PARAMS`（std140 镜像）、参数校验，以及 `stepParticlesCpu()` —— 与 compute
  同一公式的 CPU 回退与 GPU 对拍参考。
- 新增 `_Vulkan/vkParticleCompute.cpp`：host-visible SSBO 持久映射 + compute 管线经 `aPipelineCache`
  （`ePIPE_COMPUTE`）+ 独立命令缓冲/fence 同步；`GN_VkParticleCreate/Upload/Simulate/Data/Destroy`。
- 新增 `_Vulkan/vkParticleDraw.cpp`：顶点 pulling 的实例化 billboard 管线（无顶点/索引缓冲，
  `gl_InstanceIndex` 直读 SSBO；深度测试开/写关、双面、alpha 混合）；`GN_VkParticleDraw`。
- 新增着色器 `gn3d_particle.comp`（local_size 64）、`gn3d_particle.vert/.frag`。
- 描述符池补 `STORAGE_BUFFER`（256）；`GN_VkInit3D` 接 `init3DParticle/init3DParticleDraw`，
  `GN_VkShutdown3D` 先 `destroyParticle` 再 `destroyParticleDraw`。
- 新增 `tests/unit/testParticleGPU.cpp`（6 用例：布局、模拟/绘制参数校验、CPU 步进/寿命/守卫）与
  `tests/gpu/gpuCheckParticle.cpp`（7 条：create/compute/cpuMatch/death/draw/invalid/perf）。
- 验收：`make unit` = **246 cases 0 failed**；`make gpuCheck` = **61/61 PASS**（AMD 780M / RADV，
  `particle.compute` v.y=-1.000000 / p.y=-0.100000，`particle.cpuMatch` 差 0，`particle.draw`
  中心像素 0.200,0.800,0.300）。
- 边界：不自动重生（只清 `aAlive`）；每步 fence 等待、不跨帧流水化（65536 粒子 GPU 0.26 ms/步
  vs CPU 0.10 ms/步）；无半透明排序；GPU 布料 / SSBO 蒙皮调色板 / 批处理仍未做。

## 2026-10-29 — P1c 纹理：glTF PNG/JPEG 解码 + GPU 上传（54/54）

- 新增 `GN_3Dtexture.h/.cpp`：PNG（libpng）、JPEG（libjpeg，`GN_HAS_JPEG` 可选）内存解码，
  data URI 解 base64，按文件头识别格式；损坏输入走 setjmp 回收，不崩溃。
- 新增 `_Vulkan/vkTexture.cpp`：RGBA8 → `R8G8B8A8_SRGB` VkImage（staging + 布局转换）+
  glTF sampler 枚举映射（REPEAT/CLAMP/MIRROR + NEAREST/LINEAR）；`GN_VkCreateTextureRGBA8/DestroyTexture`。
- `sASSET_TEXTURE` 追加 `aImageBytes`：导入期保存 bufferView（.glb/内嵌）与相对 `.gltf` 的外部图片字节；
  `importTextures` 接收 base dir（`loadFromFile` 传入，`loadFromMemory` 为空）。
- `ImageLoadFromMem` 改为复用 `GN3d::decodePngMemory`，删除重复的 libpng 内存解码。
- 新增 `tests/unit/testTexture.cpp`（7 用例：PNG/JPEG/data URI/asset 来源/glTF data URI/sampler/损坏输入）
  与 `tests/gpu/gpuCheckTexture.cpp`（5 条：解码、glTF、上传、采样像素、销毁重建）。
- 验收：`make unit` = **240 cases 0 failed**；`make gpuCheck` = **54/54 PASS**（AMD 780M / RADV）。
- 边界：无 mipmap/各向异性；JPEG 依赖系统 `libjpeg`（缺失时仅 PNG）；`loadFromMemory` 不解析外部图片文件。

## 2026-10-29 — P1c GPU / P2 GPU 蒙皮 / P8 渲染接入（真实 GPU 49/49）

- **P1c GPU**：`_Vulkan/vkMaterial.cpp` + `gn3d_material.vert/.frag`；材质实例固定 set0
  （binding0 UBO / binding1 组合采样器）描述符集创建/写入/归还，材质管线经 `aPipelineCache`；
  `GN_VkMaterialCreate/Destroy/Draw3DMaterial`；描述符池补 `FREE_DESCRIPTOR_SET_BIT`（销毁可真正归还）。
- **P2 GPU 蒙皮**：`_Vulkan/vkSkin.cpp`（3 槽 8KB 调色板环形 UBO）+ `_Vulkan/vkSkinDraw.cpp`
  （`sSKINNED_VERTEX` 68B 六属性、set0 = 场景 UBO + 调色板 UBO、`GN_VkDraw3DSkinned`）+
  `gn3d_skinned.vert/.frag`；LBS 在模型空间完成后乘 model/viewProj。
- **P8 渲染接入**：`GN3d::cullDrawModelsPBR`（可见列表 → `DrawPBR`，返回绘制数）+
  `_Vulkan/vkDebugDraw.cpp`（LINE_LIST、深度测试开/写关、`GN_VkDrawDebugLines`）+ `gn3d_debug.vert/.frag`。
- **验收**：`make gpuCheck` = **49/49 PASS**（AMD Radeon 780M / RADV，新增 18 条）；`make unit` =
  **233 cases 0 failed**；新增文件 lint 0 违规。
- 只记录、未修（不在本轮写域）：`CreateCubeModel` 顶点数 36 但只写 24（世界 AABB 读脏）；
  `Ensure3DBufferCapacity` 同 pass 扩容会销毁在飞缓冲；`GN_VkInit3D` 在 `initialized=true` 之前调用 init。
  详见 `tests/gpu/README.md` 与 `docs/3D_FEATURE_PLAN.md` §3.5。
- P9（GPU 粒子/布料/SSBO）为可选阶段，本轮未启动；任务已排队。

## 2026-10-29 — P1b GPU 桌面验收通过（新的 tests/gpu 体系）

- 新增 `tests/gpu/` + `make gpuCheck`：11 条管线创建/缓存计数、2D 四混合、3D 前向/PBR/阴影/G-Buffer+延迟/合成/天空盒、缓存释放，共 31 条检查。
- 真实 GPU（AMD Radeon 780M / RADV PHOENIX，RGBA16F swapchain）**31/31 PASS**；lavapipe 软件预验同样 31/31。
- 回读路径：swapchain / shadow / G-Buffer 图像补 `VK_IMAGE_USAGE_TRANSFER_SRC_BIT`；验收程序按实际 swapchain 格式自建回读（不依赖 `GN_VkReadBack` 的固定 4 字节/像素）。
- 仅记录、未修（不属 P1b）：`GN_VkReadBack` 固定 4 字节/像素；`GN_VkDraw3D_GBuffer` 录制期更新已绑定描述符集；2D 批处理整帧单管线；PBR 负高度视口 vs API 1.0、HDR resolve 格式、swapchain `minImageCount` 等 Validation 项详见 `tests/gpu/README.md`。

## 2026-10-27 — 移除旧示例测试与旧资源（测试体系待重新设计）

- 删除 `samples/`（16 个示例测试，其中 05/06/07/09/11~16 长期为 TODO 占位页）与 `run.sh`；
  旧测试资源 `assets/ui`、`assets/axis`、`assets/fileicon`、`assets/effects`（62 文件）一并删除。
- `Makefile`：移除 `test` / `tests` 目标与示例编译段；`make check` = 构建 + 单元测试（`make unit` 保留，134 用例）。
- 保留：`assets/shaders/`（渲染必需）、`external/`（编译必需）、`docs/`、`tests/unit/`（本工程验收用例）。
- 后续测试代码（含 GPU 验收）按新体系重新设计，不再恢复 `run.sh`。

## 2026-10-27 — 恢复引擎默认资源（修复资源加载全失败）

- 根因：`701828d 清理原始例子` 把 `assets/` 下除 `shaders/` 外的资源整批删除，而引擎与测试仍按 `GN_SysDir()`（根目录 = `assets/`）加载：
  `samples/02/03` 用 `ui/bubbleB`，`GN_RunRec` 用 `ui/br`，`GN_UiBase` 用 `ui/bubble*`、`axis/2Dctrl_*`，`GN_UiFBrowser` 用 `fileicon/*`，`GN_Frame` 用光环贴图。
- 修复：从 `701828d^` 恢复 `assets/ui`（21）、`assets/axis`（4）、`assets/fileicon`（36）、`assets/effects`（1），共 62 个文件 / 约 0.9 MB。
- 修复 `GN_Frame.cpp` 默认光环路径 `eft/` → `effects/`（仓库里从未有过 `assets/eft/`，该默认值一直是坏的）。
- 说明：`samples/05/06/07/09/11..16` 仍是 TODO 占位页（不是资源加载失败）；资源类测试为 `./run.sh 02`、`03`。

## 2026-10-26 — 工程化阶段 0（止血）

- 修复 `samples/08_cube3d.cpp` 的 `Height()` 重复声明 `float h`（该文件此前无法编译）。
- Makefile：`make tests` 失败时非 0 退出并保留错误输出；`test/tests` 使用 `$(CXX)`；`clean` 清理 `build/test_*`；移除 freeglut 头依赖；`help` 与实际目标一致。
- `GN_VERSION` 更新为 `2026.07.04`；架构文档的版本改为引用 `GN.h`（单一来源）。
- 新增根 `LICENSE`（MIT）与 `external/README.md`（第三方来源/许可清单）。
- `GN_VkInit` 在无 Vulkan 物理设备 / 无可用队列族时输出错误并返回，不再空指针崩溃。
- 新增 `tests/unit/` 最小测试框架与 67 个纯逻辑用例；`make unit` / `make check` 可用。
- 新增 `make asan`：独立构建目录 + 独立库名，ASan/UBSan 跑同一套单测（67/67 无报告）。
- 修复 `GN_StrUnSplitToData` 的长度累加错误（原为赋值，导致缓冲区溢出）。
- 修复 `GN_StrCutSpaceFL` 对空串/全空白串的 `strlen-1` 越界读取。
- 修复 `GN_ValueUnPackHalf(0)` 返回 3e-5 而非 0。
- 修复 `GN_ValueFloatToHalf` 在 `e>125` 时的无符号移位回绕 UB（UBSan 实测命中）。
- 修复 `GN_ValueHalfToFloat` 在 `e!=0` 时的 150 位移 UB（UBSan 实测命中）。
- 新增程序化图像生成（矩形渐变、棋盘格）单测。
- 阶段 2 第一步：把 `VkCheck`/`FindMemType`/`CreateBuf`/`MakeImageView`/`LoadSPIRVFile` 抽到 `_Vulkan/vkDevice.cpp` + `vkInternal.h`（内部符号隐藏，公共动态符号不变）；`GN_Vk.cpp` 3690 → 3627 行。
- 阶段 2 第二步：把 PNG 加载/图集打包/保存/帧缓冲回读抽到 `_Vulkan/vkImageIo.cpp`（281 行）；`GN_Vk.cpp` 3627 → 3366 行；清理不再使用的 `<png.h>/<fstream>/<vector>` 包含。
- 阶段 2 第三步：把 `sIMAGE` 构造/析构/赋值/`Release`/`CopyOut`/`DataGet*` 抽到 `_Vulkan/vkImage.cpp`（187 行）；`GN_Vk.cpp` 3366 → 3204 行。
- 阶段 2 第四步：把 `ImageDraw`/`ImageDraw4P`/`ImageDrawRot`/`LineFrame2dDraw` 抽到 `_Vulkan/vkDraw2d.cpp`（177 行）；共享顶点批状态（`gVBuf`/`gVMax`/`gVCnt`/`s_lastDS`）与批处理辅助改为 `vkInternal.h` 内部符号（hidden）；`GN_Vk.cpp` 3204 → 3035 行。
- 阶段 2 第五步：把剩余 2D 包装（`ImageDrawDitherBlur`/`ImageDrawMP`/`ImageDrawFull`/`TextDraw`/`Draw2DLine`/AlphaKey/AlphaDirection）并入 `vkDraw2d.cpp`（232 行）；`GN_Vk.cpp` 3035 → 2984 行。
- 阶段 2 第六步：把 `GN_VkBeginFrame`/`GN_VkEndFrame`/`GN_VkSetScissor`、scissor 与顶点批状态、`BatchSwitch`/`FlushBatchIfScissorDirty`/`FlushVertexBatch`/`GrowVBufIfNeeded`、延迟上传队列抽到 `_Vulkan/vkFrame2d.cpp`（184 行）；相关状态以 hidden extern 暴露给 3D 代码；`GN_Vk.cpp` 2984 → 2819 行。
- 阶段 2 第七步：把 `GN_VkUpload3DMesh`/`GN_VkRelease3DMesh` 抽到 `_Vulkan/vk3dMesh.cpp`（80 行）；`GN_Vk.cpp` 2819 → 2751 行。
- 阶段 2 第八步：把阴影 pass（`GN_VkShadowPassBegin/End`、`GN_VkShadowDraw`）、`GN_VkCompositeToSwapchain`、`GN_VkDrawSkybox` 抽到 `_Vulkan/vk3dPasses.cpp`（170 行）；`s_in3DPass`/`s_inShadowPass`/`s_framebuffers3D`/`s_vtxOfs3D`/`s_idxOfs3D` 与 `Ensure3DBufferCapacity` 提升为内部符号；`GN_Vk.cpp` 2751 → 2597 行。
- 阶段 2 第九步：把 PBR pass（含 HDR 变体）与延迟渲染（G-Buffer / DeferredLighting）并入 `vk3dPasses.cpp`（403 行）；`GN_Vk.cpp` 2597 → 2370 行。
- 阶段 2 第十步：把前向 3D pass（`GN_VkBegin3DPass`/`GN_VkEnd3DPass`/`GN_VkDraw3D`/`GN_VkDraw3DPBR`）抽到 `_Vulkan/vk3dForward.cpp`（273 行）；`GN_Vk.cpp` 2370 → 2111 行。
- 阶段 2 第十一步：从 `GN_VkInit3D` 抽出渲染目标初始化（MSAA 颜色/深度、3D render pass、framebuffer）到 `_Vulkan/vk3dInit.cpp`（`init3DTargets`，107 行）；`GN_Vk.cpp` 2111 → 2018 行。
- 阶段 2 第十二步：从 `GN_VkInit3D` 抽出前向管线初始化（着色器模块、描述符/管线布局、图形管线、场景+光照 UBO、默认描述符集）到 `_Vulkan/vk3dInit.cpp`（`init3DForward`，shader 二进制由调用方传入）；`GN_Vk.cpp` 2018 → 1905 行。
- 阶段 2 第十三步：从 `GN_VkInit3D` 抽出 PBR 管线初始化（6 binding 描述符布局、管线布局、图形管线）到 `_Vulkan/vk3dInit.cpp`（`init3DPbr`）；`GN_Vk.cpp` 1905 → 1824 行。
- 阶段 2 第十四步：从 `GN_VkInit3D` 抽出阴影映射初始化（深度图/视图/采样器、render pass、framebuffer、描述符集、管线）到 `_Vulkan/vk3dInit.cpp`（`init3DShadow`）；`GN_Vk.cpp` 1824 → 1712 行。
- 阶段 2 第十五步：从 `GN_VkInit3D` 抽出 HDR 离屏目标与合成管线（HDR 图像/视图/采样器、HDR render pass/framebuffer、合成 render pass/管线/描述符集）到 `_Vulkan/vk3dInit.cpp`（`init3DHdr`）；`GN_Vk.cpp` 1712 → 1574 行。
- 阶段 2 第十六步：从 `GN_VkInit3D` 抽出天空盒初始化（程序化 Cubemap 6 面、IBL 描述符绑定、天空盒管线）到 `_Vulkan/vk3dInit.cpp`（`init3DSkybox`）；`GN_Vk.cpp` 1574 → 1370 行。
- 修复：天空盒此前只有初始化、**没有任何调用点**（sample 10 注释写"不需要 skybox"）；在 `samples/10_scene3d.cpp` 的 3D pass 内加入 `GN_VkDrawSkybox(&vp)`。
- 阶段 2 第十七步：从 `GN_VkInit3D` 抽出延迟渲染初始化到 `_Vulkan/vk3dInit.cpp`：`init3DGbuffer`（3 颜色附件 + 深度、render pass/framebuffer、G-Buffer 管线）与 `init3DDeferredLight`（8 binding 描述符、交换链 render pass/framebuffer、全屏三角形管线）；`GN_Vk.cpp` 1370 → 1178 行，`GN_VkInit3D` 仅剩 86 行。
- 阶段 2 第十八步：把 `GN_VkShutdown3D`、`Ensure3DBufferCapacity`、3D 缓冲偏移/容量状态抽到 `_Vulkan/vk3dShutdown.cpp`（135 行）；`GN_Vk.cpp` 1178 → 1054 行（有效代码 <1000，机检通过）。生成的 `gn3d_pbr_frag_spv.h` 按规则登记 `code-modularity: no-split` 技术债。
- 格式化：新增 `.clang-format`（4 空格 / K&R / 不重排长行），格式化 `src/` 下 100 个手写源文件（9 个生成的 `*_spv.h` 排除）。clang-format 会展开单行 `{...}` 块，格式化后 `GN_Vk.cpp`（1226 有效行）与 `vk3dInit.cpp`（1515 有效行）超限，因此拆分：`sIMAGE` 数据方法 → `vkImageData.cpp`（553 行）；HDR/天空盒/G-Buffer/延迟光照初始化 → `vk3dInitScene.cpp`（1022 行）。`GN_2Dscene.cpp` 与 `GN_String.cpp` 因格式化越过 1000 行，已还原并重放两处字符串修复。剩余 12 个 >1000 行文件在 HEAD 中已超限，登记为存量技术债。
- Headless 冒烟未实现：当前机器无 Vulkan 设备（`/dev/dri` 缺失、未装 lavapipe），留待环境可用后补。

## 2026-07-04 — Vulkan 稳定性修复

### 核心修复

- **延迟纹理上传机制**: 渲染 pass 内 `DataFill`/`UploadImageToGPU` 不再 `vkQueueSubmit`，数据暂存到 `s_pendingUploads[256]`，由 `GN_VkBeginFrame` 在 pass 前处理。彻底解决 AMD RADV GPUVM fault。

- **PreFrame 回调**: 新增 `GT_pFuncPreFrame`，在 `GF_FrameCycleProc1` (渲染 pass 前) 调用，适用于需在 pass 外完成的 GPU 初始化。

- **UploadImageToGPU 重构**: 移除独立的 GPU 上传逻辑，委托给 `DataFill` 统一走延迟上传路径。

### 光照系统

- 延迟光照着色器: Blinn-Phong → Cook-Torrance BRDF (GGX + Smith + Fresnel)
- `AddDirectional`/`AddSpot` 新增 `enableShadow` 参数
- `cLIGHT::Clear()` 恢复 `shadowMapSize`
- `GN_VkBegin3DPass` 等 4 个函数从 `lightUBO` 读取环境光
- PBR shader IBL 镜面反射: `F0` → `FresnelSchlick(NdotV, F0)`

### 视频播放

- FFmpeg 8.x matroska: `AVFMT_FLAG_IGNIDX | AVFMT_FLAG_IGNDTS`
- `zReadPacket` EOF 循环恢复 + `Play()` IO 重置
- `GN_SetWindowPos` 实现 (glfwSetWindowSize + GN_VkResize)
- MKV 文件重封装修复 H264 解码警告

### UB 修复

- 严格别名: `GN_Base.h` (packHalf/unpackHalf), `GN_UiBase.cpp`
- `memcpy(this)` → placement new: `GN_Frame.cpp`, `GN_Array.h`

### 安全修复

- 98 处 `strcpy`/`strcat` → `strncpy` + 显式 null (12 文件)
- 25 处 Vulkan `VkCheck` 错误检查

### Vulkan 资源管理

- 默认纹理 (1×1 white) Image/ImageView/Memory 在 shutdown 正确销毁
- 顶点缓冲预分配 512K 顶点 (16MB)
- `vkQueueWaitIdle` → fence 同步 (3 处)

### 示例修复

- Sample4/5: OpenGL 纹理合并代码在 Vulkan 下跳过
- `ImgFmtGetByteNum(0)` 返回 4 (CR_DEFAULT_FORMAT)
- Sample12: 视频缩放适配窗口 + audio=false

### 代码清理

- samples/ 精简为仅 .cpp 源码 (259 文件删除, 80MB→380KB)
- assets/ 精简为仅 shaders/ 目录
- 调试 fprintf 清理
- .gitignore 添加 build/ 产物

### 构建

- Makefile: 合并重复 .o 规则 + DEPFLAGS 自动依赖
