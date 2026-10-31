# 3D 能力扩展规划：空间划分 / 物理 / 粒子 / 动画 / 骨骼 / 布料 / 通用管线 / 天空天气 / 全局光照

> **2026-10-31 最新**：待办与已知缺口统一登记在 **§0 未解决项**；当前证据 `make unit` = **371/0**。
> 本机无 GPU（`/dev/dri` 不存在、无 lavapipe）：`make gpuCheck` 只能编译、运行 SKIP，所有 Vulkan 运行时视觉待桌面验收。
> 2026-10-30/31 接管（代码级/CPU 级完成）：cCAMERA C0–C5、GI G0–G5 + G0b、mmdViewer 拆分与 PBR/GI、
> 材质 alpha + BLEND、窗口 resize 接线与全部固定视口管线重建、task-8 resize 链。详见 §0 #10–#12 与 §3.1。

> 状态：P0 契约冻结 + P1a（管线/着色器缓存库）+ P1b（**11 条既有管线全部接入缓存，GPU 31/31**）+ P4（空间划分）+
> P5 v1（球/盒刚体物理）+ P5b 全部六项（胶囊、warm starting、CCD v1、DebugDraw、关节/约束、静态三角网）已落地；
> P6 布料 XPBD、P7 3D 粒子、P3 动画、P2 骨骼（世界矩阵/调色板）、P4→P8 的 CPU 侧（cMODEL 世界 AABB + 视锥剔除）
> 与 P1c 材质实例 CPU 侧（描述/校验/去重键）已落地；**P2 的 CPU 部分全部落地**（glTF 导入、材质/纹理描述、
> 骨骼、动画、场景装配、CPU 线性混合蒙皮、8KB 调色板 UBO 骨架）；
> **2026-10-29：P2 GPU 蒙皮、P1c GPU 材质实例与纹理上传、P8 渲染接入（剔除绘制 + 调试线框）已落地并在真实 GPU
> 验收通过；P9 全部落地（GPU 粒子 compute+SSBO+实例化 billboard、GPU 布料 compute+网格绘制、
> SSBO 调色板蒙皮 >128 关节、绘制项批处理），CPU 回退全部保留**
> （`make gpuCheck` = 94/94 PASS，`make unit` = 275 cases 0 failed）；
> **P10 动态 LOD 已落地（2026-10-29）**：`GN_3Dlod`（屏幕像素误差 + 滞后选级）、`sMESH` 追加
> `aLods/aLodCount`、`sMESH::DrawPBRLevel` / `cMODEL::DrawPBRWithLod`（§3.4）。
> **2026-10-29 追加：天空 / 云 / 天气与全局光照（GI）纳入本规划**（P11 / P12，路线图见 §3.6 / §3.7）；
> 其中 S1 天气状态机、S2a 解析式天空 + FBM 云、**S3 雾（独立 Fog UBO，前向 3 / PBR 6 / 延迟 8）**
> 已落地；**S2b 三层云盘、S4 粒子池扩展、S5 降水管线、S6 查看器全天气集成也已落地**
> （`gpuCheck` 新增 cloud.* 4 条 + precipitation.* 6 条）；GI 规划见 §3.7（2026-10-30/31 已全部代码级落地，见 §0 #10）。
> 详细证据、代码锚点与复算命令见子计划 `docs/SKY_WEATHER_PLAN.md`、`docs/GI_PLAN.md`。
> 基线：C++11、Vulkan 1.4、Makefile 单一构建入口；现有 3D 为静态网格 + Blinn-Phong/PBR + 阴影/延迟/天空盒。
> 约束：不破坏现有 `GN_VkDraw3D`/`GN_VkDrawPBR`/`cMODEL` API；新文件 camelCase，单功能 <500 有效行、多功能 <1000 有效行。
> 2D 侧现有的 `GN_2DArea` 碰撞与 `cFRAME_LAUNCHER` 动画图筐发射器**保持不变**，新系统只服务 3D。
>
> **本轮验收证据**（2026-10-27，3D 规划口径隔离构建）：单元测试 **213 cases, 0 failed**（基线 67 + P1a/P1b 22 +
> P4 11 + P5 70 + P6 6 + P7 6 + P3 6 + P2 骨骼 6 + P2 glTF 2 + P2 场景 5 + P2 蒙皮 6 + P8-CPU 3 + P1c-CPU 3）。
> **全量 `make check` 当前被并行 MMD 工作流阻断**（`src/mmd/mmdMeshBuilder.h` 先 include `GN_3Dmodel.h` 导致
> `GN_API` 未定义；Makefile 已由该工作流加入 `src/mmd`，不是本规划改动）——本喵用排除 `testMmd*.cpp` 的隔离构建
> 验证 3D 口径，未改动对方文件。
> `lintCodeModularity src/graphics tests/unit --naming --comments`
> 仅剩既有 `GN_2Dmath.cpp` 超限与既有 `gn3d_pbr_frag_spv.h` 告警，新增/改动文件 0 违规 0 告警。
> 物理文件有效行：`GN_Physics.cpp` 498（只剩 2 行余量，再加逻辑必须迁出）、`GN_PhysicsSolver.cpp` 265、
> `GN_PhysicsJoint.cpp` 380、`GN_PhysicsMesh.cpp` 212、`GN_PhysicsCcd.cpp` 112、`GN_PhysicsDebug.cpp` 108、
> `GN_PhysicsBody.cpp` 196、`GN_PhysicsCollision.cpp` 376、`GN_PhysicsCollisionCapsule.cpp` 319；
> 布料 `GN_3Dcloth.cpp` 234、粒子 `GN_3Dparticle.cpp` 170、动画 `GN_3Danim.cpp` 190、骨骼 `GN_3Dskeleton.cpp` 43、
> 剔除 `GN_3Dcull.cpp` 61、材质 `GN_Material.cpp` 82、资产 `GN_3Dasset.cpp` 286、场景 `GN_3Dscene.cpp` 105、
> 蒙皮 `GN_3DSkin.cpp` 82、`_Vulkan/vkSkin.cpp` 41（均 <500）。
> **重要边界**：DSH 沙箱内无 GPU（`/dev/dri` 不存在，`vulkaninfo` 报 no valid GPUs）；P1b 的 11 条管线已在
> 桌面 GPU（AMD Radeon 780M / RADV PHOENIX，RGBA16F swapchain）用 `make gpuCheck` 验收通过：
> **31/31 PASS**（2026-10-29）。验收程序与证据位置见 `tests/gpu/`（`make gpuCheck`）。

## 0. 未解决项（2026-10-30 接管时登记）

> 当前证据：`make unit` = **371 cases / 0 failed**（292 基线 + cCAMERA 22 + GI/探针/alpha 等新增 57）；
> `make gpuCheck` = **99/99 PASS** 是此前 llvmpipe（CPU Vulkan）结果；**本机（2026-10-30 接管会话）无 `/dev/dri`、
> 未装 lavapipe，gpuCheck 无法运行**，GPU 视觉与 RADV 复验一律待桌面机。下列条目按「影响 → 现状 → 下一步/验收」登记，
> 完成一条划掉一条。

| # | 问题 | 现状与根因 | 下一步 / 验收 |
|---|---|---|---|
| 1 | **S2b 中景 billboard 云层不可用** | `drawClouds` 之前因拿到单位矩阵从未真正显示（等于未验证）；矩阵修对后暴露为大片 256 m 方形贴片、接缝明显、「糊一片」。查看器已**默认关闭**，C 键开关；天空盒 S2a 云仍正常 | 重做贴片尺度/接缝/透明度（或环形分布 + 距离淡出）后再默认开；验收：真机目视 + `cloud.*` 4 条仍 PASS |
| 2 | ✅ **降水柱硬边（"一条雪带"）**（2026-10-30 代码级） | 柱高 24→64 m（`PRECIP_COLUMN_HEIGHT`，发射面/柱心/柱半高/回收盒/叠层快照全链由它推导，`mmdViewerWeather.cpp:37`）；雨/雪寿命收紧为「单趟落地 + 余量」（雨 2.3–2.6 s、雪 17.5–18.0 s），粒子落地 wrap 回柱顶后才寿终（寿终离地约 49–63 m / 60–62 m，预期在雾/视距外）。容量 16384/24576 未动 ⇒ 64 m 柱密度约 2.67× 稀释 | 真机目视上下过渡连续、地面密度可接受；若仍见寿终边界或过稀，再调容量/发射率（入口 `mmdViewerWeather.cpp`） |
| 3 | **粒子分层 / 空白间隔（待真机确认）** | 已修两处：满池时积压不再钳到 1（否则锯齿抽干）；新增 `aSpreadSpawnPhase` 把同帧批次沿速度打散。单测 `particleFullPoolStaysSteady`（min=max=容量）锁定 | 真机目视确认无横条；若仍有，按 #2 处理柱硬边 |
| 4 | **降水外观调参（待真机确认）** | 雨线 `0.05–0.09 m` + `RAIN_STREAK=20`（长度 1–1.8 m）；密度池 `16384/24576`、rate `60000/30000`；V 键包围盒雨雪都画；柱高已改 64 m（§0 #2） | 真机目视；调参入口 = `mmdViewerWeather.cpp` 的 `RAIN_SIZE_*` / `RAIN_STREAK` / `RAIN_CAPACITY` / `SNOW_CAPACITY` / `aLife*` |
| 5 | **贴花可见性（待真机确认）** | `drawDecals(&viewProjection)` 传指针被 `sMAT44(bool)` 吃成单位矩阵，已修；`decal.pixels` 在 gpuCheck 通过 | 真机目视雨天地面水渍/溅射环；若仍不可见，查 `gatherDecalRenderItems` 返回值与地面 y 偏移（+0.02） |
| 6 | **真实 GPU 验收缺口** | 本机无 `/dev/dri`、无 lavapipe ⇒ `make gpuCheck` 只能编译、运行 SKIP；99/99 是此前 llvmpipe 结果。窗口 resize 已接线并重建全部固定视口管线（task-22/26），仍待桌面拖拽验收 | 桌面执行 `VK_ICD_FILENAMES= make gpuCheck` 并贴回结果；拖拽窗口 + 验证层 |
| 7 | **MMD 正反/左右未目视验收** | 绕序/法线一致性已用探针 + 单测锁定（`mode.pmd` agree=6189/disagree=40），但「正面是否朝 +Z、左右是否镜像」仍靠目视 | 真机看 `tests/うさうさ`；不通过则改 `mirrorZ` 或退方案 B（见 `AXIS_UNIFICATION_PLAN.md` §4.2） |
| 8 | ✅ **`mmdViewer.cpp` 超行数上限**（2026-10-30 完成） | 原 1385 物理 / 1164 有效行；已拆成 `mmdViewerCamera.cpp`（182）/`mmdViewerWeather.cpp`（251）/`mmdViewerDecal.cpp`（174）/`mmdViewerOverlay.cpp`（262）/`mmdViewer.cpp`（319）+ `mmdViewerShared.h`（71），全部 <500 有效行 | 已验收：`lintCodeModularity tests/mmdViewer --naming --comments` = 0 违规 0 告警；`make mmdViewer` 编译通过；原 MULTI_TOO_LONG 消除。**运行期画面未验证**（无 GPU），桌面目视复核 |
| 9 | ✅ **异盘备份已完成**（2026-10-31） | 外部盘 `/run/media/ruicat/3ce0adc2-…`（btrfs，`/dev/nvme1n1p1`，设备号 78 ≠ 项目 45）经 `udisksctl unmount && mount -o rw` 重挂可写；DSH 沙箱内该挂载点默认只读，备份命令需 `danger-full-access`（已由用户批准执行） | `GnVk-20261031T100430Z.tar.zst`（154 MB，1557 条目/1254 文件；排除 `.local/`、`build/`、`libGN_mini*.so`）；`zstd -t` OK、解包后 `rsync --checksum` 无差异、sha256 `5e18ef51…d43c59`（旁证 `*.sha256`） |
| 10 | ✅ **T3 GI / T4 cCAMERA**（2026-10-31，代码级） | **cCAMERA C0–C5 ✅**（22 用例）。**GI G0a/G0b/G1a/G1b/G2a/G2b/G3/G4/G5a/G5b ✅**：G-Buffer 结构+四通道调试视图、CPU 基座、GPU 6 面烘焙→预滤波→cubemap/LUT/GI UBO、槽位 4/7/9（+8/9、10/11）、16 B/px、SSAO/双边/合成、天气切换重烘、半分辨率 SSGI、9 张 3D 探针纹理（layout 23）；**GI/SSAO/SSGI/探针默认关闭**。MMD 查看器已开前向 PBR + GI IBL。 | 运行时视觉待桌面（GI 亮度/SSAO/SSGI/探针/AO/resize） |
| 11 | ✅ **窗口 resize 接线 + 完整重建**（2026-10-31，代码级） | `GN_Main.cpp` 的 `glfwSetFramebufferSizeCallback` → pending → 主循环 `GN_VkResize`；`GN_VkResize` 重建 swapchain/MSAA/depth/3D forward/HDR/G-Buffer/延迟/SSAO/SSGI/探针相关/composite/G-Buffer 调试 + **全部固定视口管线**（前向/PBR/天空盒/调试线框/材质/蒙皮/粒子/布料/贴花/降水/云/2D 叠层）。修复 resize 后调试 AABB 错位/反向随相机。 | 拖拽窗口 + 验证层验收 |
| 12 | ✅ **材质 alpha + BLEND**（2026-10-31，代码级） | `sMATERIAL`/`sASSET_MATERIAL` alphaMode/cutoff；glTF `alpha_mode`；MMD 一律 MASK+0.5；前向 PBR discard；BLEND 独立管线（alpha 混合/深度写关/双面）+ `cMODEL::DrawPBRSorted`（网格级远→近）+ 查看器 PNG 半透明像素 >5% 自动 BLEND。限制：BLEND 仅前向、无逐三角形排序。 | 桌面目视（头发/裙边） |

## 1. 现状与差距

| 能力 | 现状 | 差距 |
|---|---|---|
| 空间划分 | 无 3D 空间索引；3D 逐模型线性绘制 | 无 BVH/AABB 树、无视锥剔除、无 raycast |
| 物理 | 无物理引擎；只有 2D 相交测试 | 无刚体、积分、接触求解、3D 碰撞 |
| 粒子 | 仅 2D `cFRAME_LAUNCHER`（`GN_Frame.h:238`） | 无 3D 粒子发射器/实例化渲染 |
| 动画 | 仅 2D `cFRAME`/`sAM_CTRL`（`GN_Frame.h:64`、`GN_AmCtrl.h:51`） | 无 3D clip/混合/状态机/骨骼 |
| 骨骼 | 无 | 无关节层级、蒙皮、glTF |
| 布料 | 无 | 无 XPBD/动态网格 |
| 管线 | 每种 pass 手写 `VkPipeline`（`GN_Vk.h:197-278`） | 无统一描述/缓存/材质变体 |
| 细节层次 | 无几何 LOD：`sMESH` 只有单套顶点/索引与一对 buffer（`GN_3Dmodel.h:20-48`） | 无多级网格、无屏幕误差选级、无切级过渡；`cMODEL` 已有世界 AABB/视锥剔除但未接绘制 |
| 天空 / 天气 | 解析式天空 + FBM 云（S2a）、天气状态机、雾（S3）、三层中景云（S2b）、降水（S5）与查看器全天气接线已落地（`assets/shaders/gn3d_skybox.frag`、`assets/shaders/gn3d_cloud.*`、`assets/shaders/precipitation.*`、`src/graphics/weather.h`、`_Vulkan/vkCloud.cpp`、`_Vulkan/vkPrecipitation.cpp`） | HDR 天空/降水变体、动态 IBL 未做（P12，§3.7） |
| 全局光照 | 静态 cubemap 伪 IBL（无 split-sum/无 LUT/mip 仅 1 级）；SSAO 着色器存在但未接线；G-Buffer 仅够延迟光照 | split-sum IBL、SSAO、SSGI、探针；G-Buffer 12 项缺失：无显式 subpass 依赖、resize 不重建、无 AO 目标/velocity/alpha cutout、附件命名错位等（§3.7） |

## 2. 目标架构

```
src/graphics/
  GN_Shader.h/.cpp           SPIR-V 模块加载/缓存/变体
  GN_Pipeline.h/.cpp         管线描述、布局、缓存（图形/计算）
  GN_Material.h/.cpp         材质实例：管线 + 描述符集 + UBO/纹理
  GN_Spatial.h/.cpp          AABB、BVH、动态 AABB 树、视锥、raycast
  GN_Physics.h/.cpp          物理世界/刚体/碰撞体/查询
  GN_PhysicsCollision.cpp    形状相交与接触流形
  GN_PhysicsSolver.cpp       顺序冲量求解、积分、休眠
  GN_3Dasset.h/.cpp          glTF 2.0（节点/网格/皮肤/动画）
  GN_3Dskeleton.h/.cpp       关节层级/绑定姿势/逆绑定/调色板
  GN_3Danim.h/.cpp           clip、采样、混合、状态机、事件
  GN_3Dskin.h/.cpp           蒙皮网格资源与绘制
  GN_3Dlod.h/.cpp           多级网格、屏幕误差、选级与滞后（P10）
  _Vulkan/vkLod.cpp         LOD 级别索引缓冲上传与绘制接线（P10b，需 GPU）
  GN_3Dcloth.h/.cpp          XPBD 布料求解器
  GN_3Dparticle.h/.cpp       3D 粒子发射器/粒子池/实例数据
  _Vulkan/vkPipeline.cpp     管线缓存实现
  _Vulkan/vkSkin.cpp         蒙皮描述符/关节调色板上传/绘制
  _Vulkan/vkCloth.cpp        布料动态缓冲上传/绘制；后续 compute
  _Vulkan/vkParticle.cpp     粒子实例缓冲/绘制
  _Vulkan/vkDebugDraw.cpp    物理/空间划分线框调试
  weather.h/.cpp             天气状态机/预设/派生参数（P11-S1，已落地）
  giParams.h/.cpp            GI 参数校验、SSAO 核、SH9、打包/解包、alpha cutoff（P12-G1）
  _Vulkan/vkGbuffer.cpp      G-Buffer 创建/重建/附件枚举/subpass 依赖/调试视图（P12-G0）
  _Vulkan/vkEnvironment.cpp  预滤波 cubemap、SH9、BRDF LUT（P12-G1/G3）
  _Vulkan/vkSsao.cpp         SSAO 与双边模糊 pass（P12-G2）
  _Vulkan/vkGi.cpp           GI UBO 与三个描述符集的 binding 写入（P12-G1）
  _Vulkan/vkAtmosphere.cpp   独立 Weather UBO（P11-S3）
  _Vulkan/vkCloud.cpp        中景 billboard 云层（P11-S2b）
  _Vulkan/vkPrecipitation.cpp 降水管线（P11-S5）
（测试程序待重新设计；原 samples/ 已移除）
  计划中的示例：skinned / cloth / physics / particles
tests/unit/
  testSkeleton.cpp / testAnimation.cpp / testCloth.cpp
  testSpatial.cpp / testPhysics.cpp / testParticle.cpp / testPipeline.cpp
  testWeather.cpp / testGi.cpp
assets/models/*.glb
assets/shaders/
  gn3d_skinned.vert/frag
  gn3d_cloth.vert/frag
  gn3d_particle.vert/frag
  gn3d_debug.vert/frag
  gn3d_cloud.vert/frag
  precipitation.vert/frag
  gn3d_ssao.frag / gn3d_ssao_blur.frag
  gn3d_ssgi.frag / gn3d_ssgi_denoise.frag
  gn3d_gbuffer_debug.frag
```

### 2.1 通用着色器管线（所有新系统的地基）

不引入 SPIRV-Cross；先用**显式管线描述**：

```cpp
enum ePIPELINE_KIND { ePIPE_GRAPHICS, ePIPE_COMPUTE };

struct sVERTEX_ATTR { uint32_t location, binding, offset; VkFormat format; };
struct sVERTEX_LAYOUT { sVERTEX_ATTR attrs[8]; uint32_t count, stride; };

struct sPIPELINE_DESC {
    ePIPELINE_KIND kind;
    const char *vsPath, *fsPath, *csPath;
    sVERTEX_LAYOUT vertexLayout;
    VkRenderPass renderPass; uint32_t subpass;
    VkPipelineLayout layout;
    bool depthTest, depthWrite, blend, cullBack;
    VkPrimitiveTopology topology;
};

class cPIPELINE_CACHE {
public:
    VkPipeline GetOrCreate(const sPIPELINE_DESC &desc); // 哈希命中复用
    void Release();
};
```

- 哈希键 = 描述字段 + SPIR-V 内容哈希；同 pass 只创建一次。
- `sMATERIAL` = 管线句柄 + 描述符集 + UBO/纹理；蒙皮/布料/粒子/调试线框全部走它。
- 阶段 1 先把现有 PBR/前向管线包进缓存，旧 `GN_Vk.pipeline*` 保留到迁移完成。

**P1a 已落地（2026-10-27）**：`GN_Shader.h/.cpp`（SPIR-V 读取/内容哈希/魔数 + `cSHADER_CACHE` 模块去重）、
`GN_Pipeline.h/.cpp`（描述/顶点布局/`ValidatePipelineDesc`/`ComputePipelineKey`/`PipelineDescEqual`）、
`_Vulkan/vkShader.cpp`、`_Vulkan/vkPipeline.cpp`（图形/计算创建 + `cPIPELINE_CACHE`）、`tests/unit/testPipeline.cpp`（13 用例）。
相对草案的描述字段做了必要扩充：`aSamples`（MSAA）、`aDepthCompare`、`aFrontFace`、`aColorAttachmentCount`、
`ePIPELINE_BLEND`（既有 2D 的 additive/multiply 变体需要）、`aDynamicViewportScissor` —— 草案只写
`depthTest/depthWrite/blend` 布尔，无法表达既有管线的混合/采样/附件差异。
**尚未做**：P1b（把既有 `pipeline3D`/`pipelinePBR`/阴影/延迟/天空盒/2D 创建点改为走缓存）需要 GPU 桌面验证；
P1c（GPU 材质实例 `sMATERIAL_RES`）未开始；现有 `GN3d::sMATERIAL` 是不含 GPU 句柄的 CPU 材质，两者不冲突。

### 2.2 空间划分

```cpp
struct sAABB { GNg::sVEC3 min, max; };

class cBVH {                       // 静态几何：地形/建筑/合并网格
public:
    void Build(const sAABB *boxes, uint32_t count);
    void QueryFrustum(const sFRUSTUM &f, std::vector<uint32_t> &out) const;
    bool Raycast(const sVEC3 &origin, const sVEC3 &dir, float maxT, sRAY_HIT &hit) const;
};

class cDYNAMIC_AABB_TREE {         // 动态刚体/粒子发射器 broadphase
public:
    uint32_t Insert(const sAABB &box, void *user);
    void Update(uint32_t handle, const sAABB &box);
    void Remove(uint32_t handle);
    void Query(const sAABB &box, std::vector<void*> &out) const;
};

struct sFRUSTUM { GNg::sVEC4 planes[6]; }; // 从 viewProj 提取
```

- 渲染：`cMODEL::Draw` 前先做视锥剔除，可见列表交给渲染器；静态大网格用 BVH，动态对象用 AABB 树。
- 物理：动态 AABB 树做 broadphase；BVH 做静态场景查询/raycast。
- 拾取/编辑器：统一 `Raycast` 接口；2D 的 `GN_2DArea` 不动。
- 不引入 octree；AABB 树对动态物体更友好，BVH 对静态场景更高效。

### 2.3 物理模拟

**后端决策：自研最小刚体物理（推荐）**，接口允许后续换 Bullet/Jolt；不直接引 Jolt（要求 C++17，与工程 C++11 冲突）。

```cpp
enum eBODY_TYPE { eBODY_STATIC, eBODY_DYNAMIC, eBODY_KINEMATIC };
enum eSHAPE_TYPE { eSHAPE_SPHERE, eSHAPE_BOX, eSHAPE_CAPSULE };

struct sRIGID_BODY {
    eBODY_TYPE type; eSHAPE_TYPE shape;
    float mass, invMass;
    GNg::sVEC3 pos; GNg::sQUAT rot;
    GNg::sVEC3 linVel, angVel;
    GNg::sMAT44 invInertiaLocal, invInertiaWorld;
    float friction, restitution;
    bool sleeping;
    sAABB worldAABB;
};

class cPHYSICS_WORLD {
public:
    void SetGravity(const GNg::sVEC3 &g);
    uint32_t AddBody(const sRIGID_BODY &body);
    void AddStaticMesh(const sMESH &mesh);      // 静态三角网格/BVH
    void Step(float dt);                        // 固定 1/60，内部子步
    bool Raycast(const GNg::sVEC3 &o, const GNg::sVEC3 &d, float maxT, sRAY_HIT &hit);
    void DebugDraw(cDEBUG_DRAW &draw) const;
};
```

- 形状 v1：球/盒/胶囊；凸包 v2。
- 碰撞：球-球、球-盒、盒-盒（SAT）、胶囊-盒/球；接触流形 + 顺序冲量（warm start、摩擦、恢复系数）。
- 积分：半隐式 Euler；固定步长 + 累加器；休眠/唤醒。
- 约束 v2：距离、铰链、固定；关节驱动可选。
- 碰撞查询与空间划分共用 AABB 树；静态网格用 BVH 三角查询。
- 现有 2D 碰撞不并入 3D 物理。

### 2.4 布料（物理子模块，XPBD）

```cpp
struct sCLOTH_DESC {
    int nx, ny; float spacing, mass;
    float compliance; int substeps;
    GNg::sVEC3 gravity, wind;
    bool pinCorners;
};

class cCLOTH {
public:
    bool Create(const sCLOTH_DESC &d);
    void Step(float dt);
    void Upload();
    void Draw(const sMATERIAL &m);
    void AddCollider(const sSPHERE3D &s); // 接刚体/骨骼碰撞体
};
```

- 距离约束：结构/剪切/弯曲 + 固定点；XPBD 子步求解，位置修正后更新速度。
- 碰撞：球/胶囊；阶段 6 接 `cPHYSICS_WORLD` 的刚体与骨骼碰撞体。
- 渲染：双面、无背面剔除；动态顶点/法线缓冲；GPU compute 放 P9。

### 2.5 粒子模拟

```cpp
struct sPARTICLE {
    GNg::sVEC3 pos, vel;
    float age, life, size;
    GNg::sVEC4 color;
};

struct sEMITTER_DESC {
    float rate; int maxParticles;
    float lifeMin, lifeMax;
    GNg::sVEC3 dir, spread; float speedMin, speedMax;
    GNg::sVEC3 gravity; float drag;
    bool additive; const sIMAGE *texture;
};

class cPARTICLE_SYSTEM {
public:
    void Create(const sEMITTER_DESC &d);
    void Update(float dt);       // CPU 池 + 环形复用
    void Upload();               // 实例缓冲
    void Draw(const sMATERIAL &m);
};
```

- 渲染：billboard 实例化，深度测试开、写关；加法/Alpha 混合；按深度排序可选。
- 阶段 7 CPU 粒子（~10k）；P9 可换 compute + SSBO。
- 2D `cFRAME_LAUNCHER` 继续用于 UI/2D 特效，不迁移。

### 2.6 骨骼与蒙皮

```cpp
struct sJOINT {
    int32_t parent;
    GNg::sMAT44 inverseBind;
    GNg::sVEC3 bindT; GNg::sQUAT bindR; GNg::sVEC3 bindS;
};

struct sSKELETON {
    std::vector<sJOINT> joints;
    void ComputePalette(const std::vector<GNg::sMAT44> &world,
                        GNg::sMAT44 *out) const; // world * inverseBind
};

struct sSKIN { uint16_t joint[4]; float weight[4]; };
```

- 关节调色板走 UBO（128 关节 × 64B = 8KB），兼容现有仅 UBO 的描述符；超出分批 draw。
- 新顶点 `VkVertex3DSkinned`（11 float + 4×uint16 + 4×float）；旧 `VkVertex3D` 不变。
- glTF：关节=节点，`inverseBindMatrices` 直接读；按 primitive 拆 `sMESH`。

### 2.7 动画系统

```cpp
struct sANIM_CHANNEL {
    uint32_t node; enum ePATH { T, R, S } path;
    std::vector<float> times;
    std::vector<GNg::sVEC3> vecValues;
    std::vector<GNg::sQUAT> quatValues;
};

struct sANIM_CLIP {
    std::vector<sANIM_CHANNEL> channels;
    float duration;
    void Sample(float t, /* out T/R/S */ ...) const;
};

struct sANIM_STATE {
    const sANIM_CLIP *clip; float time, speed; bool loop;
    const sANIM_CLIP *next; float blend; // CrossFade
};
```

- 插值：glTF `STEP/LINEAR/CUBICSPLINE`；旋转 slerp。
- 混合：节点级 T/R/S；后续可加 1D/2D blend space。
- 状态机：`Play/Stop/CrossFade` + 事件（脚步/攻击判定）。
- 2D `sAM_CTRL` 保留；3D 动画独立，不强行统一两套时间轴。

## 3. 阶段与验收

| 阶段 | 内容 | 产物 | 验收 |
|---|---|---|---|
| P0（2d） | 契约冻结：结构体/API/文件；测试骨架 | 本规划 + 头文件草案 | 评审通过；旧 API 不变 |
| P1（5d） | 通用管线/材质/SPIR-V 缓存 | `GN_Shader`/`GN_Pipeline`/`GN_Material`、`vkPipeline.cpp` | `make check`；PBR/前向改走缓存；`testPipeline` |
| P2（10d） | glTF + 骨骼 + 蒙皮 | `GN_3Dasset`/`GN_3Dskeleton`/`GN_3Dskin`、`11_skinned` | 静态/带骨骼 glTF 可见；palette 单测；sample 11 |
| P3（7d） | 动画 clip/采样/混合/状态机 | `GN_3Danim`、蒙皮动画 | 循环/CrossFade；`testAnimation` |
| P4（5d） | 空间划分：BVH/AABB 树/视锥/raycast | `GN_Spatial`、`testSpatial` | 剔除结果与暴力测试一致；raycast 命中 |
| P5（15d） | 刚体物理：形状/接触/冲量/休眠 | `GN_Physics*`、`13_physics` | 下落/堆叠/碰撞稳定；`testPhysics` |
| P6（8d） | 布料 XPBD + 刚体/骨骼碰撞 | `GN_3Dcloth`、`12_cloth` | 下落/pinned/碰撞稳定；`testCloth` |
| P7（6d） | 3D 粒子：发射器/实例化/混合 | `GN_3Dparticle`、`14_particles` | 10k 粒子可交互；`testParticle` |
| P8（7d） | 集成：视锥剔除接入渲染、调试绘制、示例/文档/ASan | 可见列表、`vkDebugDraw` | `make check/asan`；4 个示例桌面运行 |
| P9（7d，可选） | GPU 蒙皮/GPU 布料/GPU 粒子、SSBO、批处理 | compute 路径 | 性能对比；CPU 回退可用 |
| P10（5d，2026-10-29 新增） | 动态 LOD：多级网格 + 屏幕像素误差选级 + 滞后防抖 | `GN_3Dlod`、`sMESH` 多级字段、`testLod` | 三档距离选不同级；阈值往返微动切级 ≤2 次；单级模型行为不变；`make check` |
| P11（2026-10-29 新增） | 天空/云/天气剩余：中景 billboard 云、独立雾 UBO、粒子池扩展、降水管线、HDR 变体与查看器集成 | `weather.*`、`vkAtmosphere`/`vkCloud`/`vkPrecipitation`、`gn3d_skybox.frag`、`precipitation.*` | S2b–S6 逐项验收；详见 §3.6 |
| P12（2026-10-29 新增） | 全局光照 + G-Buffer 补齐：split-sum IBL、SSAO、同源环境烘焙、SSGI、探针 | `vkGbuffer`/`vkEnvironment`/`vkSsao`/`vkGi`、`gn3d_ssao*`/`gn3d_ssgi*` | G0–G5 逐项验收；详见 §3.7 |

依赖：P1 → P2/P4/P5/P7；P4 ↔ P5（broadphase）；P5 → P6；P2 → P3；P8 在 P2–P7 后。总约 **72 人日**（约 15 周单人），不含资产制作，建议预留 20% 缓冲。
P10（2026-10-29 新增，5 人日）依赖 P8 的剔除接入，可与 P9 并行；**不计入上述 72 人日基线**。
P11/P12（2026-10-29 新增，工期待排）依赖 P1b 的管线缓存与 P8 的渲染接入；两者改同一批描述符 layout，
必须先冻结槽位表（Weather 3/6/8、GI 4/7/9，见 §3.7），**不计入上述 72 人日基线**。
建议推进顺序：G0（G-Buffer 修复）与 S3（冻结槽位 + 雾 UBO）先做，其余可并行；G1 与 S2b 互不依赖。

### 3.1 进度（2026-10-27）

| 阶段 | 状态 | 产出 / 证据 |
|---|---|---|
| P0 | ✅ 完成 | 冻结 `GN_Spatial.h`、`GN_Shader.h`、`GN_Pipeline.h`（契约注释含语义与边界），接入 `GN.h` |
| P1a | ✅ 完成（库+单测） | `GN_Shader.*`、`GN_Pipeline.*`、`_Vulkan/vkShader.cpp`、`_Vulkan/vkPipeline.cpp`、`testPipeline`（17 用例）；有效行 39/303/62/262/497 |
| P1b | ✅ 完成（11 条管线全部接入，GPU 桌面验收通过） | 2D 四条混合 + 3D 前向/PBR/阴影 + 合成 + 天空盒 + G-Buffer(3 附件) + 延迟光照；`GN_VkContext::aPipelineCache` 统一持有，`GN_VkShutdown` 先 Release 再销毁 renderPass。**GPU 桌面验收 31/31 PASS**（2026-10-29，AMD 780M/RADV，`make gpuCheck`） |
| P1b 契约补充 | ✅ | `aDepthBiasEnable/Constant/Slope`（阴影 1.25/1.75）；`aColorAttachmentCount==0` = 深度专用（无 FS、subpass 0、颜色混合 0 附件） |
| P1c（CPU 侧） | ✅ 描述/校验/去重键（Vulkan 描述符集留 P8） | `GN_Material.h`（35）/`GN_Material.cpp`（82）：`sMATERIAL_DESC`/`sMATERIAL_RES`、`ValidateMaterialDesc`、FNV-1a 64 位内容键、`MaterialDescEqual`、`cMATERIAL_CACHE` 去重表（句柄槽 `aDescriptorSet` 留 P8）；`testMaterial` 3 用例 |
| P1c（GPU 侧） | ✅ 完成（2026-10-29，真实 GPU 验收） | `_Vulkan/vkMaterial.cpp`（241）：固定 set0（binding0 UBO / binding1 组合采样器）描述符集创建/写入/归还、材质管线经 `aPipelineCache`；`gn3d_material.vert/.frag`；`GN_VkMaterialCreate/Destroy/Draw3DMaterial`；`gpuCheck` 6 条 |
| P4 | ✅ 完成 | `GN_Spatial.cpp`（AABB/射线/视锥）、`GN_SpatialBVH.cpp`、`GN_SpatialTree.cpp`、`testSpatial`（11 用例）；有效行 219/167/241/499 |
| P5 | ✅ v1 + 胶囊 + warm start + CCD + DebugDraw + 关节 + 三角网 | `GN_Physics.h` 契约；`GN_Physics.cpp`（世界/宽相/休眠/射线/warm start 缓存，迭代 30 次，498）、`GN_PhysicsCollision.cpp`（dispatch，376）、`GN_PhysicsCollisionCapsule.cpp`（球/胶囊/盒-胶囊，319）、`GN_PhysicsBody.cpp`（构造/惯量/AABB，196）、`GN_PhysicsJoint.cpp`（距离/铰链/固定，380）、`GN_PhysicsMesh.cpp`（三角网+BVH，212）、`GN_PhysicsCcd.cpp`（扫掠 TOI，112）、`GN_PhysicsDebug.cpp`（线框/接触标记，108）、`GN_PhysicsSolver.cpp`（265）；`testPhysics` 19 + `testPhysicsCollision` 10 + `testPhysicsSolver` 14 + `testPhysicsCapsule` 12 + `testPhysicsDebug` 5 + `testPhysicsJoint` 6 + `testPhysicsMesh` 4 = 70 用例 |
| P5 剩余 | ✅ 完成 | P5b 六项全部落地。**已知限制**：7 层及以上堆叠坍塌（30/40 次速度迭代都复现）；盒对静态三角网 v1 未实现（球/胶囊可用）；三角网接触不参与 warm start |
| P6 | ✅ CPU 模拟（GPU 上传/绘制留 P8/P9） | `GN_3Dcloth.h`（58）/`GN_3Dcloth.cpp`（234）：规则网格、XPBD 距离约束（结构/剪切/弯曲）、球体碰撞、法线重建；`testCloth` 6 用例（参数/拓扑、固定角+下垂+侧风、球碰撞、确定性、Reset、法线） |
| P7 | ✅ CPU 模拟（实例上传/绘制留 P8） | `GN_3Dparticle.h`（59）/`GN_3Dparticle.cpp`（170）：固定池、按速率发射、圆锥随机方向（固定 xorshift 种子）、重力/阻尼、寿命回收、颜色插值；`testParticle` 6 用例（参数、发射/寿终、容量、重力、确定性、Reset） |
| P3 | ✅ CPU（glTF 导入留 P2，GPU 蒙皮留 P8/P9） | `GN_3Danim.h`（56）/`GN_3Danim.cpp`（190）：通道采样 STEP/LINEAR + 旋转 slerp、clip duration、`cANIM_PLAYER` 的 loop/crossFade/非循环停住；`testAnimation` 6 用例（采样/STEP/slerp、未覆盖节点、loop、crossFade、非循环停住） |
| P2（骨骼部分） | ✅ CPU（glTF 导入待第三方头） | `GN_3Dskeleton.h`（21）/`GN_3Dskeleton.cpp`（43）：关节父链世界矩阵、palette = world*inverseBind、坏父索引按根降级；`testSkeleton` 6 用例（空、单根、父链、绑定姿势恒等、坏父、长度不符） |
| P8（CPU 侧） | ✅ cMODEL 世界 AABB + 视锥剔除 + 剔除绘制 | `GN_3Dcull.cpp`：顶点×变换求世界 AABB、`isVisibleIn`、`cullModels` 可见列表（上限/空指针安全）、`cullDrawModelsPBR`（可见列表 → `DrawPBR`，返回绘制数）；`testCull` 5 用例 + `gpuCheck` 6 条 |
| P2（glTF 导入 + 材质/纹理描述） | ✅ CPU | `GN_3Dasset.h/.cpp`（有效 286）：cgltf v1.15（已静态审计/登记）、节点/网格/皮肤/动画、材质与纹理 CPU 描述（`sASSET_MATERIAL` / `sASSET_TEXTURE` / primitive.aMaterial；不读图片字节）；`testAsset` 2 用例（全字段 fixture + 非法输入，含默认值断言） |
| P2（场景装配 + 蒙皮 CPU） | ✅ CPU（GPU 蒙皮绘制留 P8） | `GN_3Dscene.h/.cpp`（105）：父链世界矩阵、每节点姿势采样、关节父映射、palette；`GN_3DSkin.h/.cpp`（82）：CPU LBS（权重归一化/越界钳制）与 `sSKIN_UBO` 打包；`testScene` 5 + `testSkin` 6 用例 |
| P2（GPU 蒙皮绘制） | ✅ 完成（2026-10-29，真实 GPU 验收） | `_Vulkan/vkSkin.cpp`（123）：3 槽 8KB 调色板环形 UBO；`_Vulkan/vkSkinDraw.cpp`（300）：`sSKINNED_VERTEX` 68B 六属性布局、set0（场景 UBO + 调色板 UBO）、蒙皮管线经 `aPipelineCache`、`GN_VkDraw3DSkinned`；`gn3d_skinned.vert/.frag`；`gpuCheck` 5 条 |
| P1c GPU 纹理上传/glTF 纹理解码 | ✅ 完成（2026-10-29，真实 GPU 验收） | `GN_3Dtexture.h/.cpp`：PNG（libpng）/JPEG（libjpeg，可选）内存解码、data URI、格式嗅探；`_Vulkan/vkTexture.cpp`：RGBA8 → `R8G8B8A8_SRGB` VkImage（staging + 布局转换）+ glTF sampler 映射；`GN_VkCreateTextureRGBA8/DestroyTexture`；`sASSET_TEXTURE::aImageBytes` 在导入期保存 bufferView/外部文件字节；`testTexture` 7 用例 + `gpuCheck` 5 条 |
| P8（调试线框） | ✅ 完成（2026-10-29，真实 GPU 验收） | `_Vulkan/vkDebugDraw.cpp`（200）：LINE_LIST 管线经 `aPipelineCache`、set0 场景 UBO、深度测试开/写关、`GN_VkDrawDebugLines`；`gn3d_debug.vert/.frag`；`gpuCheck` 2 条 |
| P9（GPU 粒子部分） | ✅ 完成（2026-10-29，真实 GPU 验收） | `GN_3DparticleGPU.h/.cpp`（64B std430 布局 + CPU 回退/对拍）；`_Vulkan/vkParticleCompute.cpp`（host-visible SSBO + compute 管线经 `aPipelineCache` + fence 同步）、`_Vulkan/vkParticleDraw.cpp`（顶点 pulling 的 billboard 管线）；`gn3d_particle.comp/vert/frag`；`GN_VkParticleCreate/Upload/Simulate/Draw/Data/Destroy`；`testParticleGPU` 6 用例 + `gpuCheck` 7 条 |
| P9（剩余：GPU 布料/SSBO 蒙皮/批处理） | ✅ 完成（2026-10-29，真实 GPU 验收） | `GN_3DclothGPU.h/.cpp` + `_Vulkan/vkClothCompute.cpp`（A/B SSBO 乒乓 + Jacobi）+ `_Vulkan/vkClothDraw.cpp` + `gn3d_cloth.comp/vert/frag`（`cloth.*` 5 条）；`_Vulkan/vkSkinSsbo.cpp` + `gn3d_skinned_ssbo.vert/frag`（4096 关节 SSBO + 3 槽环形调色板，`skin.ssbo.*` 4 条）；`GN_3Dbatch.h/.cpp`（稳定排序 + 切换统计，`batch.*` 3 条）；`testClothGPU` 5 + `testBatch` 5 用例 |
| P10（动态 LOD） | ✅ 完成（2026-10-29，CPU 单测 + GPU 验收） | `GN_3Dlod.h/.cpp`：误差公式 + 带滞后选级（纯 CPU）；`sMESH_LOD` + `sMESH::aLods/aLodCount`、`DrawPBRLevel`、`cMODEL::DrawPBRWithLod`；`testLod` 6 用例；`gpuCheck` 4 条（`lod.select/indexCount/singleLevel/pixels`） |
| P11（天空 / 天气） | ✅ 完成（2026-10-29；视觉 1..6 由用户验收） | S1 天气状态机 `weather.h/.cpp`（8 用例）+ S2a 解析式天空/FBM 云 + S3 雾（`_Vulkan/vkAtmosphere.cpp`、`GN_VkSetFog`、前向 3 / PBR 6 / 延迟 8）+ **S2b 三层云盘**（`_Vulkan/vkCloud.cpp`、`gn3d_cloud.vert/frag`，`gpuCheck` 4 条）+ **S4 粒子池扩展**（BOX / 回收盒 / 尺寸范围 / 风，`testParticle` 12 用例）+ **S5 降水管线**（`_Vulkan/vkPrecipitation.cpp`、`precipitation.vert/frag`，`gpuCheck` 4 条）+ **S6 查看器集成**（雾/云/雨雪接线、远平面容云层）；HDR 变体留 P12，路线图 §3.6 |
| P12（全局光照） | ✅ 代码级完成（2026-10-31） | `docs/GI_PLAN.md`：G0a/G0b/G1a/G1b/G2a/G2b/G3/G4/G5a/G5b 全部落地；GI/SSAO/SSGI/探针**默认关闭**；MMD 查看器前向 PBR + GI IBL；运行时视觉待桌面 |
| T4 cCAMERA（`docs/CAMERA_PLAN.md`） | ✅ C0–C5 代码级完成（2026-10-30） | `GN_3Dcamera.h` + 6 个 .cpp（Move/Orbit/Follow/Fit/Control）；`testCamera` 22 用例全 PASS；查看器已接线（唯一 `cCAMERA`、F 轨道/滚轮/中键）；运行期手感待桌面 |

P4 的"剔除结果与暴力测试一致"验收已通过：BVH 512 盒 40 次 AABB 查询与暴力集合逐元素相等；射线 20 条与暴力最小 t 对照；
视锥 300 点/300 盒分别与"投影后 NDC 判定"和"8 角点全在外侧"暴力判据对照；动态树 350 步随机 Insert/Update/Remove
每 20 步与活跃槽位暴力重叠集合相等。**尚未接入渲染**（`cMODEL::Draw` 前的视锥剔除属 P8）。

P5 v1 的验收：球从 2m 下落停在地面（y=r±0.05，静止后入睡）、三层 1kg 盒堆叠 5s 不塌且全部入睡、
两球对撞分离、运动学盒推动态球、射线命中最近刚体、休眠/唤醒、确定性（两次运行位差 bitwise 相等）、
非法输入不崩溃。**修复记录**：位置修正原先按"接触点数 × 迭代次数"重复施加固定穿透（4×4≈3.2 倍），
三层堆叠会被顶散；改为按 `pointCount × iterations` 均摊后稳定（`GN_PhysicsSolver.cpp:296-301`）。

P5b 已验收部分（2026-10-27）：
- **胶囊**：球/盒/胶囊两两窄相（`CollideBoxCapsule` 用线段-盒三分法 + 面流形 4 采样）、胶囊惯量/世界 AABB、
  射线近似（沿轴 17 球采样，`GN_Physics.cpp:121` 注明误差）；`testPhysicsCapsule` 12 用例 + `testPhysics` 3 例。
- **warm starting**：`sCONTACT_CONSTRAINT::aAccNormal/aAccTangent` 由世界按"刚体对 + 接触点 5cm 就近匹配"
  从上一步缓存预填（`GN_Physics.cpp` `seedWarmStart`/`cacheContacts`），求解器先重放这些冲量再迭代；
  速度迭代由 20 提到 **30** 后，3/4/5/6 层堆叠入睡步数 **29/29/29/29**；同配置关闭 warm start 为
  **60/29/125/塌**。6 层堆叠 5s 内稳定入睡（`physicsTallStackStable`）。
  `RemoveBody` 清空缓存（句柄复用）。缓存为空时与冷启动逐位一致（`testWarmStartEmptyCacheMatchesColdStart`）。
- **CCD v1**：`setContinuousCollision(true)`（默认关闭）时按「上一位置 → 目标位置」线段扫掠静态/运动学刚体的
  世界 AABB（按本刚体包围半径扩展），在最早 TOI 处钳住并清零进入方向的法向速度（`GN_PhysicsCcd.cpp`，112 行）。
  `testPhysics` 3 例：120 m/s 小球不穿 0.1m 薄地面、90 m/s 胶囊被拦、常速下落仍正常入睡；
  对照组（不开 CCD）同速小球一步穿到地面顶面以下。近似边界见 §3.3。
- **DebugDraw（CPU 线段）**：`DebugDraw(sDEBUG_DRAW&)` 输出所有刚体形状线框 + 上一步接触点/法线；
  线数口径：球 3×24、盒 12、胶囊 2×24+4、每接触点 4（十字 + 0.2 长法线）；`GN_PhysicsDebug.cpp`（108）。
  `testPhysicsDebug` 5 用例：空世界清空、盒包围盒=半尺寸、球端点距心=半径、胶囊端圆位置/半径、接触标记与法线长度。
  v1 边界：胶囊线框只画圆柱端圆与侧线（半球顶不在线框里）；渲染器消费在 P8。
- **关节/约束（距离/铰链/固定）**：`AddJoint(sJOINT_DESC)` / `RemoveJoint` / `GetJointCount`；世界每子步构建
  运行时关节，速度层消去被约束方向的相对速度 + Baumgarte 误差项，位置层修正锚点/轴向误差
  （`GN_PhysicsJoint.cpp`，380）。静态/运动学/睡着的动态刚体在关节内按无限质量；`RemoveBody` 连带删除
  引用它的关节；v1 无关节 warm start。`testPhysicsJoint` 6 用例：非法输入/连带删除、距离保持、
  固定关节局部系相对位姿、铰链绕轴自由、铰链消去垂直角速度、确定性。
- **静态三角网（v1）**：`AddStaticMesh(const sMESH&)` 追加世界坐标三角形（按 VkVertex3D 布局取前 3 个 float），
  `cBVH` 宽相 + 球/胶囊窄相（球心/胶囊线段到三角形最近点），`GN_PhysicsMesh.cpp`（212）。
  边界：盒对三角网 v1 未实现（跳过）、三角网接触不参与 warm start、A 端为内部静态代理（不占刚体句柄）。
  `testPhysicsMesh` 4 用例：输入校验/退化剔除、球静止、横放胶囊静止、200 三角形 BVH 不漏。
- **仍不稳**：7 层及以上堆叠坍塌（30/40 次迭代都复现）。定位：每步强制唤醒的 6 层对照仍坍塌 ⇒ 排除休眠路径，
  是速度迭代预算/收敛不足；6 层在 20 次迭代下会下沉、30 次迭代后稳定（本项已把上限由 5 层提到 6 层）。

P6 v1 的验收（CPU）：5×5 软布四角固定后重力下垂、侧风把中间顶点吹离 z=0 平面、球体碰撞把顶点推到球面外、
两次模拟逐位一致、`reset()` 回到初始网格、法线有限且单位长度。**边界**：只固定四角（不支持任意点集/整行固定）、
风按加速度处理（不做面积/法线投影）、无自碰撞/摩擦、无 GPU 上传与绘制（P8/P9）。

P7 v1 的验收（CPU）：参数非法拒绝；`rate=100, dt=0.1` 发射 10 个；停发后按寿命全部回收；发射速率远大于容量时
存活数钳在 `maxParticles`；初速 0 的粒子在重力下 `v.y = g*dt` 且位置向下；同参数两次模拟池内容逐位一致；
`reset()` 清空存活粒子并保留容量/参数。**边界**：无纹理/混合模式/GPU 实例缓冲（`Upload`/`Draw` 留 P8）。

P3 v1 的验收（CPU）：LINEAR 平移中点/STEP 取前一帧、旋转最短弧 slerp（identity→90° 在 t=0.5 为 45°）、
未覆盖节点返回 false 且不改 out、loop 按 duration 回绕、crossFade 权重线性过渡并在结束后切到下一段、
非循环到末尾停住且采样停在最后一帧。**边界**：CUBICSPLINE 未实现；glTF clip 导入依赖 P2；无骨骼/蒙皮
（GPU 蒙皮留 P8/P9）。

P2 骨骼部分 v1 的验收（CPU）：单根世界平移 = 局部平移、两节父链叠加、绑定姿势下 palette ≈ identity、
坏父索引（父 >= 自身）按根降级不崩溃、局部姿势数量不匹配返回 false。**边界**：glTF 导入（节点/网格/皮肤/
动画）需引入 `cgltf` 并登记 `external/README.md`；蒙皮顶点混合与关节 UBO 留 P8/P9。

P8 CPU 侧 v1 的验收：`cMODEL::computeWorldAABB` 用顶点 × `aTransform` 得到世界包围盒（缩放/平移纳入）；
`sFRUSTUM::FromViewProj` 下原点/平面内模型可见、远处/相机后方不可见、`aVisible=false` 不可见；
`cullModels` 只写可见下标并尊重 `maxOut`/空指针。**边界**：尚未接入 `GN_VkDraw3D` 的绘制循环
（draw 次数下降与逐像素一致需要 GPU 桌面验证）。

P1c CPU 侧 v1 的验收：合法描述通过校验；管线键为 0、绑定数 >8、binding 重复/非递增、资源为空、种类非法、
阶段位为 0 都被拒绝；内容相同（含资源地址）键相同且逐字段相等，任一字段变化键与相等性都变化；
去重表对同描述返回同句柄、不同资源新建、非法描述拒绝、`Release` 清空。**边界**：VkDescriptorSet 创建/写入与
UBO/纹理实际绑定留 P8（需要 GPU）。

P2 glTF 导入 v1 的验收（CPU）：内存 fixture（1 三角形 + 2 节点 + 1 关节 skin + 1 条平移动画）导入后
节点/网格/皮肤/动画字段正确（位置、索引、逆绑定矩阵、动画时长与 0.5s 采样值）；非 JSON/空指针/不存在文件
返回 false 且不残留数据。**边界**：材质/纹理/相机/灯光不导入；CUBICSPLINE 与 morph 权重通道跳过；
GPU 蒙皮、关节 UBO 与绘制留 P8/P9。

P2 材质/纹理 CPU 描述 v1 的验收：材质因子/贴图索引与 glTF 默认值正确（baseColor 缺省 1,1,1,1、metallic/
roughness 缺省 1、emissive 缺省 0、贴图缺省 -1）；纹理保留 uri/mime/bufferView 与 sampler 的 wrap/filter
原始枚举（无 sampler 时 wrap=10497、filter=0）。**边界**：不读取/解码图片字节，纹理上传与采样留 P8。

P2 场景装配与蒙皮 CPU v1 的验收：节点父链世界矩阵与坏父降级、动画通道覆盖局部姿势、绑定姿势 palette≈identity、
CPU LBS（identity 不变、单关节平移、0.5/0.5 混合、越界关节/零权重安全）、`packPalette` 128 截断与未用槽清零。
**边界**：场景装配不渲染；`vkSkin.cpp` 只有 UBO 创建/销毁骨架（无 GPU 运行验证），描述符集/调色板上传与
绘制留 P8。

### 3.2 P1b 的契约演进与剩余缺口（2026-10-27 更新）

**已解决**（契约扩展，`GN_Pipeline.h` 已冻结）：
1. ~~没有固定 viewport/scissor~~ ⇒ 新增 `aDynamicViewport` / `aDynamicScissor` + `VkViewport aViewport` / `VkRect2D aScissor`
   （PBR 的 y=H、height=-H 负高度视口可精确表达）。
2. ~~无法表达"无深度状态"~~ ⇒ 新增 `aDepthStencil`（2D renderPass 无深度附件，必须 false）。
3. ~~2D alpha 因子无法复刻~~ ⇒ 新增 `aCustomBlend` + 四个 `VkBlendFactor`；既有 2D 用
   color=(SRC_ALPHA, ONE_MINUS_SRC_ALPHA)、alpha=(ONE, ZERO)，bright 只把 dst 改 ONE，multiply 用 (DST_COLOR, ZERO)。
4. ~~缓存生命周期~~ ⇒ `GN_VkContext::aPipelineCache`，`GN_VkShutdown` 先 `Release()` 再销毁 renderPass / device。

**仍存在的缺口**（只记录，未处理）：
1. 缓存键只有 64 位内容哈希，`cSHADER_CACHE`/`cPIPELINE_CACHE` 均无二次内容比较 ⇒ 哈希碰撞会把不同 SPIR-V/描述认成同一项
   （理论风险，实测无碰撞）。若要求"零风险"，需在条目里存字节/描述并二次比较，属头文件变更。
2. 深度专用（0 颜色附件）不强制 `aDepthStencil=true`（Vulkan 允许不提供深度状态）；当前只有阴影管线用这条路径，测试只覆盖 `true` 形态。
3. 深度专用描述若误设 `aCustomBlend=true`，因子仍参与键与校验（虽然用不到）。
4. 动态开关为 true 时 viewport/scissor 数值仍参与缓存键（站点统一留默认值；差异只多一次 miss，不影响正确性）。
5. `Raycast` 要求 `dir` 已归一化；平行轴判定依赖 IEEE `1/0 = ±inf`（构建不得开 `-ffast-math`）。

### 3.3 P5b 的契约演进（胶囊 / warm starting / CCD，2026-10-27）

**头文件变更**（`GN_Physics.h`，均为新增，不破坏既有 4 参数调用）：
1. `SolveVelocities(..., uint32_t iterations, bool warmStart = false)`；默认 false 时求解器仍清零后迭代。
2. `cPHYSICS_WORLD` 私有区新增 `sWARM_CONTACT` 缓存、刚体对索引与 `seedWarmStart`/`cacheContacts`；
   缓存不跨 `RemoveBody` / `Clear` 保值（句柄复用会清空）。
3. `sCONTACT_CONSTRAINT::aAccNormal/aAccTangent` 语义改为"世界可预填的累计冲量"；字段与布局未变。
4. 新增 `cPHYSICS_WORLD::setContinuousCollision(bool)`（默认 false）+ 私有 `resolveContinuousCollisions`；
   CCD 状态不进入 `sRIGID_BODY` 布局，旧代码（含手工构造刚体的单测）无需改字段初始化。
5. 新增 `sDEBUG_LINE` / `sDEBUG_DRAW`（纯 CPU 线段收集器）与 `cPHYSICS_WORLD::DebugDraw(sDEBUG_DRAW&)`；
   本层不含颜色/线宽/句柄，渲染器消费在 P8（`_Vulkan/vkDebugDraw.cpp`）。
6. 新增 `eJOINT_TYPE` / `sJOINT_DESC` / `sJOINT_CONSTRAINT`、`AddJoint` / `RemoveJoint` / `GetJointCount`
   与 `solveJointVelocities` / `solveJointPositions`；关节持久数据存世界私有区，`RemoveBody` 连带删除。
7. 新增 `AddStaticMesh(const sMESH&)`（头文件只前向声明 `sMESH`，不把 GN_3Dmodel.h 拉进物理头）与
   `GetStaticTriangleCount()`；三角网数据 + BVH 存世界私有区，接触 A 端为内部静态代理（不入 `aBodies`），
   `Clear()` 一并清理。
8. 新增 `GN_3Dcloth.h`（`sCLOTH_DESC` / `sCLOTH_COLLIDER` / `cCLOTH`）并接入 `GN.h`；纯 CPU 模拟，
   GPU 上传/绘制未做（P8/P9）。
9. 新增 `GN_3Dparticle.h`（`sPARTICLE` / `sPARTICLE_EMITTER_DESC` / `cPARTICLE_SYSTEM`）并接入 `GN.h`；
   纯 CPU 池模拟，实例上传/绘制未做（P8）。
10. 新增 `GN_3Danim.h`（`sANIM_CHANNEL` / `sANIM_TRANSFORM` / `sANIM_CLIP` / `cANIM_PLAYER`）并接入 `GN.h`；
    采样/混合/状态机纯 CPU，glTF 导入留 P2，GPU 蒙皮留 P8/P9。
11. 新增 `GN_3Dskeleton.h`（`sJOINT` / `sSKIN` / `sSKELETON`）并接入 `GN.h`；世界矩阵与调色板纯 CPU，
    glTF 导入与 GPU 关节 UBO/蒙皮留 P2 后续/P8。
12. `cMODEL` 新增 `aWorldAABB` / `computeWorldAABB()` / `isVisibleIn()`；新增 `cullModels()` 可见列表
    （`GN_3Dcull.cpp`）。渲染循环接入仍待 P8 的 GPU 验证。
13. 新增 `GN_Material.h`（`sMATERIAL_BINDING` / `sMATERIAL_DESC` / `sMATERIAL_RES` / `cMATERIAL_CACHE`）并接入
    `GN.h`；CPU 侧校验/内容键/去重，VkDescriptorSet 创建与 UBO/纹理绑定留 P8。
14. 新增 `external/include/cgltf.h`（jkuhlmann/cgltf **v1.15**，MIT，SHA-256 `e378a21c…11c6ec`；静态审查
    无网络/子进程/凭据读取/写文件，仅 `fopen(path,"rb")` 读）与 `GN_3Dasset.h`（`sASSET` 等）并接入 `GN.h`；
    导入节点/网格/皮肤/动画到 CPU 结构，材质/纹理与 GPU 蒙皮留 P2 后续/P8。
15. `sASSET` 扩展 `sASSET_MATERIAL` / `sASSET_TEXTURE` 与 `sASSET_PRIMITIVE::aMaterial`（-1=无）；
    只导 CPU 描述（因子/索引/uri/mime/bufferView/sampler 枚举），不读图片字节、不接渲染。
16. 新增 `GN_3Dscene.h`（`sASSET_SCENE`、`buildWorld`、`sampleLocalPose`、`buildSkinParents`、
    `computeSkinPalette`）并接入 `GN.h`；父链/坏父降级/动画采样/调色板纯 CPU。
17. 新增 `GN_3DSkin.h`（`sSKINNED_VERTEX`、`sSKIN_UBO`、`skinVertices`、`packPalette`）并接入 `GN.h`；
    `_Vulkan/vkSkin.cpp` 提供 8KB 调色板 UBO 创建/销毁骨架（声明入 `vkInternal.h`），描述符集与绘制留 P8。

**已知边界**：warm start 只重放**标量**冲量，切线方向按本步相对速度重算 ⇒ 高速滑动/旋转接触不保留历史切向
（v1 近似）；接触点匹配半径 5cm，超出视为新接触（冷启动）；休眠体的接触不入缓存。

**CCD 的近似**：目标用世界 AABB 按本刚体包围半径扩展 ⇒ 盒/胶囊的棱角会**提前**停下（保守，不漏判）；
运动学目标只按步末位置参与扫掠（其步内自身运动不扫掠）；命中后本步剩余位移丢弃、不施加恢复系数；
只处理动态且醒着的刚体；起点已在扩展盒内时不在此处理（交给接触求解）。

**关节的边界**：v1 不做关节 warm starting；固定/铰链要求添加时两锚点重合（否则第一步会被立即拉齐）；
铰链轴为零向量时退回 (0,1,0)；睡着的动态刚体在关节内按无限质量，不会被关节唤醒（与接触的唤醒规则不同）。

**三角网的边界**：只支持球/胶囊（盒 v1 跳过）；网格必须已是世界坐标（不读 `sMESH` 之外的变换）；
三角网接触不参与 warm start；`AddStaticMesh` 每次追加后立即重建 BVH（大网格应合并后一次添加）。

### 3.4 P10 动态 LOD（2026-10-29 落地；契约草案 → 实现偏差见下）

**实现状态（2026-10-29）**：`GN_3Dlod.h/.cpp` + `sMESH_LOD`/`sMESH::aLods` + `DrawPBRLevel` +
`cMODEL::DrawPBRWithLod` + `testLod` 6 用例 + `gpuCheck` 4 条（77/77 PASS）。
与草案的三处偏差（实现时判定，均有理由）：
1. 选级函数**显式传入世界 AABB**（`const sAABB&`）：草案签名没有它但公式需要，隐式假设模型在原点会算错距离。
2. **不建每级 VkBuffer**：现有 PBR 路径每次绘制把顶点动态上传（`GN_VkDraw3DPBR`），`aLods` 只需 CPU 数据；
   `UploadToGPU` 不处理 `aLods`。
3. 草案验收里"镜头 2/20/200 m 选 0/1/2 级"的具体距离与默认 1.0 px 阈值不自洽（按公式 0.1 m 误差需 ≥93 m 才降一级），
   测试改为按公式推导距离（10/100/1000 m）并保留滞后与单级回归用例。

**目标**：同一模型按屏幕像素误差选择几何精度级别，砍掉远距离几何开销。**不做**运行时网格简化（级别在烘焙期生成），
不改 `GN_VkDraw3D` / `GN_VkDrawPBR` / `cMODEL` 既有签名（新增字段与函数只追加）。

**立项依据（实测）**：`sMESH` 只有一组 `aVertices`/`aIndices` 与一对 `VkBuffer`（`GN_3Dmodel.h:20-48`），
`src docs tests assets` 全量检索 `lod` / `level of detail` / `细节层次` 零命中；`cMODEL` 已有
`aWorldAABB`/`computeWorldAABB`/`isVisibleIn` 与 `cullModels`（`GN_3Dmodel.h:63-71`、`GN_3Dcull.cpp`），
但绘制循环尚未接入（属 P8）。因此 P10 是**新增能力**，不是补全既有缺口。

**数据结构（`GN_3Dmodel.h` 追加，不改既有字段语义）**：
1. `sMESH_LOD`：`float *aVertices; uint32_t *aIndices; uint32_t aVertexCount/aIndexCount; float aMaxError;`
   —— `aMaxError` 为该级相对 LOD0 的世界空间最大几何误差（米），由烘焙器算出并随资产存盘。
2. `sMESH` 追加 `sMESH_LOD *aLods; uint32_t aLodCount; float aBoundRadius;`；
   **`aLodCount == 0` 退化为现有单级行为**（向后兼容，所有既有测试/调用不变）。
3. `aLods[0]` 即 LOD0，与现有 `aVertices`/`aIndices` 同源；`UploadToGPU` 按级各建一对 `VkBuffer`，
   `ReleaseGPU` 一并释放；各级 `aWorldAABB` 必须一致（细级不得超出粗级包围盒）。

**选级（`GN_3Dlod.h/.cpp`，纯 CPU、可单测）**：
- `sLOD_VIEW { float aScreenHeight; float aFovYRad; GNg::sMAT44 aViewProj; GNg::sVEC3 aCamPos; }`
- `float lodScreenError(const sMESH_LOD &lod, const sMESH &mesh, const sLOD_VIEW &view);`
  公式 `S = aMaxError * aScreenHeight / (2 * d * tan(aFovYRad / 2))`，`d` 取相机到世界 AABB 的最近距离（保守）；
  `d <= 近平面` 时返回 `+INF`（此情形不得降级）。
- `uint32_t selectLod(const sMESH &mesh, const sLOD_VIEW &view, float aMaxScreenErrorPx,
  uint32_t aPrevLevel);` —— 从粗到细找**第一个** `S > aMaxScreenErrorPx` 的级；
  用双阈值滞后防抖（升档阈值 `aMaxScreenErrorPx`、降档阈值 `0.8 * aMaxScreenErrorPx`），
  `aPrevLevel` 由调用方跨帧持有（无状态函数 + 显式输入，便于单测）。
- 默认阈值 **1.0 px**；分辨率变化只改 `aScreenHeight`，阈值语义与 DPI 无关。

**绘制接线（P10b，需 GPU 桌面验证）**：`sMESH::DrawPBR` 增加带选级结果的版本；`cMODEL::DrawPBR` 传入上一帧缓存级别，
避免每网格每帧重算；**阴影/深度 pass 与主 pass 必须使用同一级**，否则影体错位。选级结果在 pass 外算一次、各 pass 复用。

**验收（CPU，无 GPU 可做）**：新增 `testLod`：
1. 三级合成网格（LOD0 1000 三角 / LOD1 200 / LOD2 20，误差 0 / 0.1 / 1.0 m），镜头距离 2 m / 20 m / 200 m 分别选 0 / 1 / 2 级；
2. 距离在阈值附近往返微动 100 次，切级次数 ≤ 2（滞后生效）；
3. 相机落在世界 AABB 内或近平面内时返回最细级，且不除零、不产生 NaN；
4. `aLodCount == 0` 时选级返回 0，绘制路径与现有单级逐位一致（回归保护）；
5. 桌面 GPU 验收（P10b）：同一场景两倍距离下 `vkCmdDrawIndexed` 的 `indexCount` 按级下降，截图无裂缝、无闪档。

**边界 / 不做**：不做运行时 QEM 简化；不做 cluster/Nanite 类 GPU-driven LOD（量级远超本规划）；v1 不做几何 morph、
不做 alpha 淡入淡出——**要求各级同拓扑烘焙**，跨拓扑切级若出现裂缝则加 skirt（P10b 再评估）；
不处理 LOD 与蒙皮变形（本仓库 GPU 蒙皮未开始）；离线烘焙工具按项目规则进 `.tools` 载体，不写一次性脚本。

### 3.5 P1c GPU / P2 GPU 蒙皮 / P8 渲染接入 / P9 全部（2026-10-29 落地）

**公共 API（`GN_Vk.h`，新增、不改既有签名）**：
`GN_VkMaterialCreate/Destroy/Draw3DMaterial`（固定 set0：binding0 UBO + binding1 组合采样器）、
`GN_VkDraw3DSkinned`（`sSKINNED_VERTEX` 68B，set0 = 场景 UBO + 调色板 UBO）、
`GN_VkDrawDebugLines`（世界坐标线段，LINE_LIST）、
`GN_VkParticleCreate/Upload/Simulate/Draw/Data/Destroy`（P9：SSBO + compute + billboard）。

**验收证据**（本机真实 GPU，AMD Radeon 780M / RADV PHOENIX，Wayland）：
`make gpuCheck` = **73/73 PASS、0 FAIL、0 SKIP**；`make unit` = **256 cases 0 failed**；
新增检查覆盖：

- P1c：`material.desc/create/cache/draw/destroy/invalid` —— UBO(0.25,0.5,0.75) × 纯白纹理回读
  `0.250,0.500,0.750`；两次绘制管线 miss 不增；非法描述（binding 种类/缺失）被拒且不改句柄。
- P1c 纹理：`texture.decode`（2×2 PNG 逐字节）、`texture.glTF`（image data URI + sampler 经
  `loadFromMemory` 解码）、`texture.upload`（RGBA8 → SRGB VkImage）、`texture.sample`
  （(200,40,40) 采样 = sRGB→linear `0.578,0.021,0.021`）、`texture.destroy`（重复销毁 + 重建）。
- P2：`skin.pipeline/invalidArgs/singleJoint/dualJoint/jointLimit` —— 单关节 +0.5x 与 CPU LBS 左边界差
  `0.0016`（容差 0.03）；双关节 0.5/0.5 差 `0.0016`；129 关节截断到 128。
- P8：`cull.draw.count/origin/plusX/minusX/empty`（3 个立方体剔掉 1 个，返回 2）、
  `cull.debugLine/outside`（中心线颜色误差 0、线外仍是背景）。
- P9：`particle.create`（容量 64 的 SSBO + 映射回读）、`particle.compute`（dt=0.1、重力 (0,-10,0)
  ⇒ v.y=-1.000000、p.y=-0.100000）、`particle.cpuMatch`（64 粒子 GPU 与 `stepParticlesCpu`
  逐粒子差 0）、`particle.death`（life=0.05 后 `aAlive=0`）、`particle.draw`（单个 billboard
  中心像素 `0.200,0.800,0.300`）、`particle.invalid`（dt<=0/越界上传/pass 外/重复销毁）、
  `particle.perf`（65536×4 步：GPU 0.26 ms/步、CPU 0.10 ms/步，末粒子结果一致）。
- P9-a 布料：`cloth.create/compute`（20 步后 meanY 0 → -0.2153、四角位置逐位不变、全有限）、
  `cloth.cpuMatch`（同初始数据 1 步后 GPU 与 `stepClothCpu` 差 0）、`cloth.draw`（中心像素
  `0.700,0.300,0.200`）、`cloth.invalid`。
- P9-b SSBO 蒙皮：`skin.ssbo.pipeline`（init 预建，绘制不再新增缓存项）、`skin.ssbo.joint200`
  （200 关节、关节 150 平移 +0.5x 后像素 `0.572,0.254,0.127`）、`skin.ssbo.paletteRing`
  （同帧两块不同调色板各自生效）、`skin.ssbo.invalid`（0/4097 关节、空指针、pass 外）。
- P9-c 批处理：`batch.sort`（键单调 + aOrder 集合不变）、`batch.switches`（3 → 1）、
  `batch.draw`（乱序/排序两帧同点颜色一致 `±0.02`）。

**边界 / 已知限制**：
1. 材质 v1 只支持"固定两个绑定"（UBO + 单张纹理）；纹理 v1 无 mipmap/各向异性，JPEG 依赖
   `libjpeg`（pkg-config 缺失时只有 PNG 可用）；`loadFromMemory` 不解析外部图片文件（无 base 目录），
   外部 uri 需走 `loadFromFile`。
2. 蒙皮 v1 只做位置/法线 LBS；无 morph、无 GPU 动画、无 SSBO（128 关节 UBO 上限）。
3. 蒙皮/调试线框复用 3D 动态顶点缓冲；`Ensure3DBufferCapacity` 在同一 pass 内扩容会销毁旧缓冲，
   已记录为既有缺陷（未在本次改动中修）。
4. `GN_VkInit3D()` 在 `GN_Vk.initialized = true` 之前调用各 `init3D*`，新增 init 一律以 `device/renderPass`
   判空，不得检查 `initialized`。
5. `CreateCubeModel` 的 `aVertexCount=36` 但只写 24 个顶点（`GN_3Dmodel.cpp:428/440`），
   会导致 `computeWorldAABB()` 读到未初始化顶点；属既有缺陷、不在本轮写域，GPU 验收用测试内自建立方体规避。
6. GPU 粒子 v1：SSBO 为 host-visible（不是 device-local + staging）；每个 `GN_VkParticleSimulate`
   单独提交并等待 fence，不跨帧流水化——65536 粒子实测 GPU 0.26 ms/步 vs CPU 0.10 ms/步，
   收益要等批量更大或改为跨帧流水化；寿命到只清 `aAlive`（不自动重生，重生由调用方重新 Upload）；
   按 SSBO 顺序绘制，无半透明排序。
7. P9-a 布料 v1：只有结构距离约束（无剪切/弯曲/球碰撞/自碰撞）、每步 fence 等待不跨帧流水化、
   拓扑在 Create 时固定；CPU `cCLOTH`（XPBD，含剪切/弯曲/碰撞）保持独立，两者不互相调用。
8. P9-b 蒙皮：SSBO 调色板上限 4096 关节，旧 128 关节 UBO 路径原样保留（未删、未改）；
   无 GPU 动画/morph。P9-c 批处理只提供排序与切换统计，不合并几何、不做实例化，
   调用方自行按排序结果提交。

### 3.6 P11 天空 / 云 / 天气（2026-10-29 新增，S1/S2a 已落地）

**范围**：天气状态机（CPU）→ 解析式天空 + FBM 云 → 雾 → CPU 降水模拟 + 透明管线 → HDR 变体与查看器集成。
**已确认决策（2026-10-28）**：云 = 混合模式（远景 FBM + 中景 billboard，不做 raymarch）；雾 = 独立 Weather UBO（不扩 SceneUBO）；
雨雪 v1 = CPU 展开 billboard；HDR 首版纳入；动态 IBL 放 v2。
**证据与锚点**：`docs/SKY_WEATHER_PLAN.md` §1（现状表）与 §11（复算命令）。

剩余子阶段：

| 子阶段 | 内容 | 写域（新文件 camelCase） | 验收 |
|---|---|---|---|
| S2b | 中景 billboard 云层（视差、程序化 alpha、随风漂移） | `_Vulkan/vkCloud.cpp`、`gn3d_cloud.vert/frag` | ✅ 已完成（2026-10-29）：**3 层云盘**（高度 300/260/220 m，tile 与风速按层放大）、无顶点缓冲 tile 四边形、160 B UBO、alpha 混合/深度写关；`gpuCheck` 4 条（`cloud.outsidePass/draw/pixels/destroy`） |
| S3 | 雾：独立 Weather UBO（binding 前向 3 / PBR 6 / 延迟 8）+ 3 个片元着色器 | `_Vulkan/vkAtmosphere.cpp`、`gn3d.frag`/`gn3d_pbr.frag`/`gn3d_deferred_light.frag` | ✅ 已完成（2026-10-29）：`GN_VkSetFog` + 3 条管线接线；`gpuCheck` 3 条（`fog.pbr/pbrReset/forward`） |
| S4 | 粒子池扩展：BOX 形状、相机跟随循环回收、尺寸范围、风 | `GN_3Dparticle.h/.cpp`、`testParticle.cpp` | ✅ 已完成（2026-10-29）：`ParticleShape{CONE,BOX}`、`setRecycleBox`（逐轴 wrap）、`aSizeMin/aSizeMax`（独立随机流，老描述逐位不变）、`aWind/aWindResponse`；`testParticle` 6→12 用例 |
| S5 | 降水管线：CPU billboard 展开、alpha 混合、深度不写、HDR 变体 | `_Vulkan/vkPrecipitation.cpp`、`precipitation.vert/frag` | ✅ 已完成（2026-10-29）：CPU 展开 6 顶点/粒子（stride 28B）、单 64 B push constant、alpha 混合/深度写关；`gpuCheck` 4 条（`precipitation.outsidePass/empty/draw/pixels`）；HDR 变体未做（P12） |
| S6 | 查看器全天气集成、HDR 天空/降水变体、文档与 CHANGELOG | `tests/mmdViewer/mmdViewer.cpp`、`docs/` | ✅ 已完成（2026-10-29，视觉验收由用户执行）：`applyWeather` 接 `GN_VkSetFog`；3D pass 内 天空盒 → 云 → 地面 → 模型 → 贴花 → 降水；雨/雪 BOX 粒子跟随相机；远平面下限提到云层高度+2×tile（否则小模型裁掉云层）；HDR 变体未做（P12） |
| S6b | 地面 + 地面贴花（雨滴溅射水渍/环，软边/环状 alpha） | `GN_3Ddecal.h/.cpp`、`_Vulkan/vkDecal.cpp`、`gn3d_decal.vert/frag` | ✅ 已完成（2026-10-29）：CPU 环形池 + 寿命渐隐；世界 XZ 平面四边形、alpha 混合/深度写关；`gpuCheck` 4 条（`decal.*`）+ `testDecal` 7 用例；降水形状 v2（细芯雨丝沿速度 / 径向雪团） |

约束与风险：

- 描述符槽位必须与 P12 共同冻结（§3.7 的表）；漏写一条管线 = 那条 pass 没雾。
- 天空盒管线只绑 `renderPass3D`（`vk3dInitScene.cpp`），HDR 要再建一条 `hdrRenderPass` 管线，否则 HDR 下天空缺失。
- 降水 v1 每帧上传约 8000×6 顶点 × 28 B ≈ 1.3 MB；上万粒子要等 `sVERTEX_ATTR` 支持 input rate 或走 GPU 粒子（P9）。
- **降水 v1 只有一块动态顶点缓冲**：同一次 3D pass 内多次调用 `GN_VkDrawPrecipitation` 会互相覆盖 ⇒ 查看器每帧只提交一次（雨雪并存时合并粒子、streak 取折中值）。
- 降水与云层管线用**固定 viewport**（创建时的 swapchainExtent），窗口 resize 后不跟随（与 vkClothDraw/vkParticleDraw 一致）。
- 云层 3 层是同一 draw 内的程序化分层（高度/tile/风速按层缩放），不是 3 次 draw，因此只占一个 UBO。
- IBL 与解析天空不同源：v1 接受，G3 用 GPU 6 面烘焙修（§3.7）。

### 3.7 P12 全局光照（GI）+ G-Buffer 补齐（2026-10-29 新增）

**范围**：间接光照（环境漫反射 + 镜面 + 可见性 + 一次弹射）；直接光照不在内。
**现状**：静态 cubemap 伪 IBL（无 split-sum、无 BRDF LUT、cubemap 仅 1 级 mip）；SSAO 着色器存在但**无任何 C++ 引用**；
G-Buffer 仅够延迟光照，GI 所需通道/同步/生命周期共 **12 项缺失**。
**证据与锚点**：`docs/GI_PLAN.md` §1（现状）、§5（G-Buffer 缺失）、§12（复算命令）。

剩余子阶段：

| 子阶段 | 内容 | 写域（新文件 camelCase） | 验收 |
|---|---|---|---|
| G0 | G-Buffer 补齐：附件改名 + 枚举、显式 subpass 依赖、resize 重建、调试视图、alpha cutout | `_Vulkan/vkGbuffer.cpp`、`gn3d_gbuffer.frag`、`gn3d_gbuffer_debug.frag` | 同步验证层下 resize 无告警；调试视图四通道正确；MMD 头发不再实心 |
| G1 | IBL split-sum：预滤波 cubemap + SH9 + BRDF LUT + GI UBO（256 B） | `_Vulkan/vkEnvironment.cpp`、`vkGi.cpp`、`giParams.*`、`testGi.cpp` | CPU 单测（SH9 常数环境、LUT 单调）；桌面粗糙度反射 |
| G2 | G-Buffer 打包（24→16 B/px）+ AO 目标 + `invViewProj` + SSAO/模糊/合成 | `_Vulkan/vkSsao.cpp`、`gn3d_ssao*.frag` | `make check`；桌面 AO 无带状；关 SSAO 回退现状 |
| G3 | 环境与解析天空同源（GPU 6 面烘焙，天气切换重烘） | `_Vulkan/vkEnvironment.cpp` | 桌面切天气后反射与背景一致 |
| G4 | SSGI：半分辨率 ray march + 单帧双边（velocity 可选） | `gn3d_ssgi.frag`、`gn3d_ssgi_denoise.frag` | 桌面颜色渗色；帧时间在预算内 |
| G5 | 探针：SH9 网格（DDGI 可选，需先补 compute dispatch） | 按 G5 开始时冻结（`GN_3Dprobe.*` 或 `vkProbe.cpp`） | 桌面屏幕外遮挡有间接光 |

> **2026-10-31 状态**：G0–G5 全部代码级完成（G0a/G0b/G1a/G1b/G2a/G2b/G3/G4/G5a/G5b，见 §0 #10 与 GI_PLAN §9）；
> GI/SSAO/SSGI/探针默认关闭，运行时视觉待桌面。

**G-Buffer 12 项缺失**（G0/G2 的验收清单）：附件命名错位、无显式 subpass 依赖、resize 不重建、无 alpha cutout、无调试视图、
法线空间未约定、无 AO 目标、无 velocity、无 F0 通道、RGBA16F 浪费带宽、MSAA 不一致、每 draw 更新描述符；
逐项证据与补齐动作见 `docs/GI_PLAN.md` §5.1。

**待拍板（详见 §4 第 8/9 条）**：环境贴图来源（CPU 预滤波 / GPU 6 面烘焙 / 先不做）、G-Buffer AA（4× MSAA / 后处理 / 不管）、
SSAO 是否只做延迟路径、SSGI 起步档、探针 SH9 vs DDGI。

**槽位表（P11/P12 共同冻结）**：

| 管线 | 现有 | Weather（P11-S3） | GI（P12-G1，G1b 已接线） |
|---|---|---|---|
| 前向 `gn3d` | 0,1,2 | 3 | 4 = GI UBO（layout 4→5；shader 暂未用，预留） |
| PBR | 0..5 | 6 | 7 = GI UBO、8 = 预滤波 cubemap、9 = BRDF LUT（layout 7→10） |
| 延迟 | 0..7 | 8 | 9 = GI UBO、10 = 预滤波 cubemap、11 = BRDF LUT（layout 9→12） |
| SSAO | — | — | 独立 set |

## 4. 关键决策（需拍板）

1. **物理后端**：自研最小刚体（推荐，C++11、无新依赖）vs Bullet（大依赖、构建变重）；Jolt 需 C++17，暂不选。
2. **空间划分**：动态 AABB 树 + 静态 BVH（推荐）；octree 不选。
3. **资产格式**：glTF 2.0 + `cgltf`（推荐）。
4. **布料/粒子**：CPU 先行（推荐），GPU compute 放 P9。
5. **管线描述**：显式 `sPIPELINE_DESC`（推荐），不引 SPIRV-Cross。
6. **骨骼上限**：128 关节 UBO（推荐），超出分批；P9 评估 SSBO。
7. **LOD 生成与阈值**：离线烘焙 2–3 级 + `sMESH` 追加字段（推荐：复用现有绘制路径、零新依赖）vs 运行时 QEM（主线程不可控）
   vs cluster/Nanite 类（需新管线，量级远超本规划）；选级用屏幕像素误差（与分辨率/DPI 无关），默认阈值 **1.0 px**，
   降档阈值 `0.8×` 做滞后；级别同拓扑烘焙（避免裂缝处理进 v1）。
8. **天空 / 天气**（2026-10-28 已确认）：云混合（远景 FBM + 中景 billboard）、雾独立 Weather UBO、
   雨雪 v1 CPU billboard、HDR 首版、动态 IBL 放 v2。
9. **全局光照**（2026-10-29 规划；**2026-10-30 拍板：环境贴图来源 = B（GPU 6 面烘焙解析天空）**）：
   v1 = split-sum IBL + SSAO（仅延迟路径），IBL 源 = GPU 6 面烘焙 → 回读 → `giParams::prefilterCubemap` CPU 预滤波 → 上传 mip 链，
   G3 只补「天气切换重烘 + GPU 端预滤波」；v2 = SSGI 半分辨率 + 单帧双边；
   v3 = 探针先 SH9（DDGI 需先补 compute dispatch）。**G-Buffer AA** 推荐 1× + 后处理 FXAA/SMAA；G-Buffer 必须先补齐（G0）再叠 GI。

## 5. 风险

- 自研物理工作量最大（P5），先冻结 v1 形状/求解器范围，不要一开始做凸包/CCD/关节。
- 描述符池按旧管线尺寸配置，新增材质/蒙皮/粒子需扩容；P1 一并调整 `descriptorPool`。
- 关节 UBO 每 draw 更新会阻塞；P2 用 per-frame 环形 UBO，P9 改 SSBO。
- 布料/粒子/蒙皮都依赖动态顶点/实例上传；复用 `Ensure3DBufferCapacity` 思路，避免每帧销毁重建。
- 无 GPU 的 CI 只能验证 CPU 数学/构建；渲染正确性由桌面 GPU 测试确认（旧 `run.sh` 已移除，测试待重新设计）。
- cgltf 为第三方头文件，需登记 `external/README.md` 并在 `make` 中只编译一次实现。
- LOD 显存随级数线性增长（每级一对 `VkBuffer`）；静态大场景先估容量，且 `LoadOBJ`（只吃单文件）需扩展多级加载约定。
- 选级必须在 pass 外算一次并跨 pass 复用：阴影/深度与主 pass 各自算一遍会出现影体错档。
- 各级世界 AABB 必须一致（细级不得超出粗级包围盒），否则 BVH/视锥剔除与选级互相打架。
- 跨拓扑切级会裂缝：v1 只允许同拓扑烘焙 + 滞后，不做 morph/淡入淡出。
- 天气与 GI 抢描述符槽位：Weather 3/6/8、GI 4/7/9（§3.7 表）；两份子计划必须引用同一张表，改一处同步另一处。
- 天空盒管线只绑 `renderPass3D`，HDR 路径要单独建管线；否则 HDR 下天空缺失。
- 降水 v1 是 CPU 展开（≈1.9 MB/帧 @8000 雨滴）；2 万+ 需要 `sVERTEX_ATTR` 的 input rate 或 GPU 粒子。
- ✅ 已解决（G0a/task-8/task-26）：G-Buffer 显式 subpass 依赖、resize 完整重建、固定视口管线重建。
- ✅ 已解决（G0a）：G-Buffer 附件改名 + 枚举（`eGBUFFER_ATTACHMENT`）。
- ✅ 已解决（G2b）：SSAO 重写为世界空间 + 4×4 双边。
- HDR 双 Reinhard（`gn3d_pbr.frag` 与 `gn3d_composite.frag` 各一次）：GI 必须在线性空间注入，SSGI 前要统一到合成阶段。
- GI/天气的 GPU 视觉在沙箱内无法验证（无 `/dev/dri`）：只能编译 + CPU 单测，以桌面截图与帧时间为准。

## 6. 建议的首个可交付

先做 **P1 + P4**：通用管线缓存 + 空间划分/视锥剔除。两者都不依赖资产格式，能立即改善渲染并给物理 broadphase 打基础；随后 P2/P3（骨骼/动画）与 P5/P6（物理/布料）可并行推进。

**执行结果（2026-10-27）**：P1 拆成 P1a/P1b/P1c 后，P1a + P1b + P4 已完成并可无 GPU 验证；P1b 覆盖全部 11 条既有管线
（2D 四条混合、3D 前向、PBR、阴影、合成、天空盒、G-Buffer(3 附件)、延迟光照），描述字段按站点逐项对齐旧 `VkPipelineCreateInfo`。
1. ✅ **桌面验收已完成**（2026-10-29，AMD Radeon 780M / RADV，31/31 PASS）：验收程序 `tests/gpu/`，
   `make gpuCheck` 逐项对照（2D 四混合、前向、PBR/场景、阴影/延迟/天空盒/G-Buffer）；旧 `run.sh` 已移除；
   失败时按站点回滚（每站点内注释给出 desc 与旧状态的对应关系）。GPU 不可用的环境无法代替这一步。
2. ✅ **P1c GPU 已完成**（2026-10-29）：材质实例描述符集 + 固定材质管线，`gpuCheck` 6 条 PASS；
   glTF 纹理解码/上传仍留后续（见 §3.5 边界）。
3. ✅ **P4 接入渲染已完成**（2026-10-29）：`cullDrawModelsPBR` 按视锥剔除后绘制，`GN_VkDrawDebugLines`
   调试线框；验收 = 3 立方体剔掉 1 个 + 未绘制位置为背景色（§3.5）。
4. ✅ **P2 GPU 蒙皮已完成**（2026-10-29）：`GN_VkDraw3DSkinned` + 调色板环形 UBO，`gpuCheck` 5 条 PASS。
5. ✅ **P9 全部完成**（2026-10-29）：GPU 粒子（compute+SSBO+实例化 billboard，7 条）、
   GPU 布料（A/B SSBO 乒乓 + Jacobi + 网格绘制，5 条）、SSBO 调色板蒙皮（>128 关节，4 条）、
   绘制项批处理（稳定排序 + 切换统计，3 条）；`gpuCheck` 73/73，CPU 回退
   （`stepParticlesCpu` / `stepClothCpu`）保留。
   下一步：P10（LOD，契约见 §3.4）与 P11（天空/天气）。
   **2026-10-29 追加的下一步**：P11 从 S3（雾 UBO，先冻结槽位表）与 S2b（云）继续；
   P12 先做 G0（G-Buffer 修复）→ G1（IBL）→ G2（SSAO），详见 §3.6 / §3.7。
6. ✅ **P10 / P11 已完成**（2026-10-29）：P10 动态 LOD（4 条 GPU 检查）+ P11 S2b 三层云盘 / S4 粒子池扩展 /
   S5 降水管线（形状 v2：细芯雨丝 / 径向雪团）/ S6 查看器全天气集成 / **S6b 地面 + 贴花**
   （cloud.* 4 条 + precipitation.* 6 条 + decal.* 4 条）；`make unit` = 275 cases 0 failed，
   `make gpuCheck` = 94/94 PASS（AMD 780M / RADV）。剩余：P12 GI（G0–G5 未开始，§3.7）。
