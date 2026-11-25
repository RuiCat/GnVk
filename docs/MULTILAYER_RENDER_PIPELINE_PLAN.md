# 多层渲染流水线规划（场景物体多着色器 / 逐物体多 pass）

> **状态**：📋 已完成 — 多层渲染设计；CPU 侧 `GN_RenderPipeline` / `GN_RenderQueue` 已落地，GPU 全路径按 §5 分期。

> **本文件写于 2026-11-20**：当时只有设计、不改代码。之后 P0（CPU 契约 + 单测）、P1a/P1b（管线缓存 v2）、
> P2a/P3-CPU（统一绑定表 + 层队列，CPU 侧）陆续落地，进度已逐条标在 §5；**P4 起（逐物体多 pass、
> 外部 SPIR-V 热重载、延迟/前向混排）仍未实现**。分期推进，每期独立验收。
>
> **需求确认（2026-11-20，用户答复）**
> - 层含义：**D+C**——既要有「物体 → 不同着色器」的选择，也要有「渲染通道分层」，并且同一物体可以被多套着色器分多趟渲染（阴影 / G-Buffer / 前向 / 描边等）。
> - 交付范围：**只要设计规划文档**。
> - 着色器来源：**内置变体 + 运行时外部 SPIR-V，两者都要**。
>
> **证据基线**：git HEAD `a48523d` + 工作树未提交改动（2026-11-20 23:10 快照；`git status --short` 约 67 项）。
> 本文所有「现状」断言都指向该快照的文件:行；工作树仍在被其它会话修改，行号会漂，**回看时以代码为准并重新定位**。
> 文中 `make unit` / `make gpuCheck` 的历史数字只作区间参考；本规划**没有**重跑它们，不把旧数字当本轮验收。

---

## 0. 结论摘要（TL;DR）

1. **模型**：把现在「一个 pass 一套手写 `VkPipeline` + 一个固定 Draw API」改成三层：
   - **Layer（层）**：面向物体的渲染槽位（阴影 / G-Buffer / 前向不透明 / 描边 / 前向透明 / 效果 / 叠层），固定顺序与固定状态策略；
   - **ShaderProgram（着色器程序）**：具名、可注册、按层提供入口（VS/FS + 顶点格式 + 状态覆盖 + 特殊化常量）；
   - **RenderItem（渲染项）**：网格 + 材质 + 层掩码 + 逐层着色器覆盖，提交后在层内排序、逐项解析管线。
2. **绑定契约**：因为是显式描述、**不引入 SPIRV-Cross 反射**，必须冻结一套所有着色器共用的绑定表：
   - `set 0` = 场景/帧数据（PBR 的 12 槽 + 阴影层专用的 per-cascade lightVP 槽，共 13 槽）；
   - `set 1` = 材质（UBO + 最多 7 张纹理）；
   - `set 2` = 蒙皮（可选；UBO 或 SSBO 两种变体）；
   - push constant = **128 B**（模型矩阵 64 B + 每 draw 覆盖参数 64 B）。
   外部 SPIR-V 只有遵守这张表才被接受；用反射自动推导布局不在 v1。
3. **逐物体多 pass** = 层掩码 + 逐层程序覆盖。一个角色可以是：阴影层用内置 depth、G-Buffer 层用 PBR、前向层用卡通、描边层用 outline——**同一物体、四趟、不同着色器**。
4. **管线缓存**：现有 `GN3d::cPIPELINE_DESC` 已经是内容键（描述字段 + SPIR-V 哈希，`src/graphics/GN_Pipeline.h:61,121`），
   扩展「特殊化常量」后可直接承载变体；新增按帧回收以支持热重载。
5. **分期**：P0 契约与 CPU 测试 → P1 管线缓存 v2 → P2 统一绑定表迁移 + 程序注册 → P3 层队列 + 前向多着色器 → P4 逐物体多 pass（阴影/G-Buffer/描边/透明）→ P5 外部 SPIR-V + 热重载 → P6 延迟/前向混排 + 蒙皮 canonical + 收尾。
6. **最大风险**：P2 统一绑定表要改 PBR/前向/G-Buffer/阴影四套着色器与描述符写入，**是唯一会大面积影响既有画面的阶段**；
   必须用现有 gpuCheck 像素断言逐条兜住，并在真机桌面目视验收后才允许进入 P4。
7. **明确不做**（v1）：2D/UI 迁移、粒子/云/布料/贴花等效果 pass 的通用化、完整 Render Graph、OIT、运行时 GLSL 编译、SPIRV-Cross、`VK_EXT_shader_object`、多线程管线编译。

---

## 1. 需求、术语与边界

### 1.1 需求拆成三件事

| 编号 | 需求 | 验收口径（本规划定义） |
|---|---|---|
| R1 | 场景物体可选不同着色器 | 两个物体在同一帧、同一层内使用不同程序绘制，像素颜色/行为各自正确 |
| R2 | 渲染分层（通道顺序与状态） | 影子层 → G-Buffer → 延迟光照 → 前向不透明 → 描边 → 前向透明 → 效果 → 叠层；每层状态表固定 |
| R3 | 同一物体多趟渲染 | 同一物体在**同一渲染路径内**的阴影、G-Buffer、前向、描边组合被绘制，各趟可用不同程序；跨「延迟+前向」混排属 P6 的扩展验收（见 §6.2 Q1） |
| R4 | 内置着色器变体 | 内置程序可在无外部文件时注册并绘制（P1：PBR / depth；P3/P4：unlit / toon / outline） |
| R5 | 运行时外部 SPIR-V | 从文件加载 `.spv`，校验格式/描述/版本，建管线、可绘制；格式或描述非法被原子拒绝并可回退。**语义越界（表外 binding、SPIR-V 输出接口不匹配）无法静态拒绝**，按 ADR-1 的边界交给管线创建/校验层/gpuCheck |
| R6 | 不破坏既有行为 | 既有 `GN_VkDraw3D*` / `GN_VkDraw3DPBR` / `cMODEL::DrawPBRSorted` 调用方不改代码，现有 gpuCheck 仍 PASS |

### 1.2 术语（本文严格区分）

| 术语 | 定义 | 不是什么 |
|---|---|---|
| **Pass（通道）** | 帧级阶段，拥有一个 render pass / framebuffer / 目标，例如 Shadow、G-Buffer、DeferredLight、Forward、Composite | 不是「层」；一个 Web 页式的「layer」不是 Pass |
| **Layer（层）** | 面向物体的槽位：决定排序、深度/混合状态、可用的程序入口；一个物体可进入多个层 | 不是独立 render pass（描边层和前向层共用同一个 forward render pass） |
| **ShaderProgram（程序）** | 具名的「按层入口表」+ 顶点格式 + 状态覆盖 + 特殊化常量；是着色器的逻辑组合 | 不是单个 `VkShaderModule`，也不是单个 `VkPipeline` |
| **Entry（入口）** | 程序在某一层的 VS/FS 对 + 该层允许的状态覆盖 | 不是 SPIR-V 的 `main` 入口名（v1 固定 `main`） |
| **Material（材质）** | 程序句柄 + 材质 UBO 参数 + 纹理绑定 + 默认层掩码 | 不是 `GN3d::sMATERIAL`（后者是旧 CPU 光照属性结构） |
| **RenderItem（渲染项）** | 一次网格提交：顶点/索引（CPU 指针或 GPU 缓冲）、模型矩阵、材质、层掩码、逐层程序覆盖 | 不是一次 `vkCmdDraw`（同一项可能产生多次 draw） |
| **Pipeline variant（管线变体）** | 入口 × 顶点格式 × render pass × 状态 × 特殊化常量的具体 `VkPipeline` | 不是程序（程序是变体的来源） |

### 1.3 范围边界

**在范围内**：静态网格、蒙皮网格（后续）、场景物体的材质与着色器选择、层顺序、逐物体多 pass、内置与外部 SPIR-V、管线缓存与回收。

**不在 v1 范围内**（登记为后续项，不得偷偷混入）：
- 2D 批处理、字体、UI、视频层；
- 粒子、云块、降水、贴花、布料、天空盒、调试线框、G-Buffer 调试视图等**效果/工具 pass** 的通用化（它们继续使用现有专用管线，只在帧时间线上占位）；
- 完整 Render Graph（自动依赖、别名、内存屏障推导）；v1 用**固定层序**；
- 透明逐三角形排序 / OIT；
- 运行时 GLSL 编译与 SPIRV-Cross 反射；
- `VK_EXT_shader_object`（见 ADR-4）；
- 多线程管线编译、管线缓存跨设备共享。

---

## 2. 现状与证据

### 2.1 帧级通道现状

现有 3D 路径按调用方手动编排（不是引擎自动编排）：

```
GN_VkShadowPassBegin/End        → 阴影（D32×4 CSM，深度专用管线）
GN_VkBegin3DPassDeferred        → G-Buffer pass（3 MRT + 深度）
GN_VkDraw3D_GBuffer             → 写 G-Buffer
GN_VkSSAOPass / GN_VkSSGIPass   → 全屏 pass（在 G-Buffer 后、延迟光照前）
GN_VkDeferredLighting           → 延迟光照（全屏）
GN_VkBegin3DPass                → 前向 3D pass（HDR MSAA + resolve）
GN_VkDraw3DPBR / GN_VkDraw3D    → 前向绘制（每 draw 立刻上传顶点）
GN_VkEnd3DPass                  → 结束 + 合成到 swapchain
GN_VkBegin2DPass / 2D 绘制      → 叠层
```

对应实现锚点：`src/graphics/_Vulkan/vk3dPasses.cpp:318`（Begin3DPassDeferred）、`:384`（SSAO）、`:399`（SSGI）、
`:420`（Draw3D_GBuffer）、`:495`（DeferredLighting）；前向 pass 在 `src/graphics/_Vulkan/vk3dForward.cpp:120` 起。

**事实**：当前没有「层」对象，也没有把物体交给引擎排序；顺序完全由每个测试程序手写调用决定
（例如 `tests/mmdViewer/mmdViewer.cpp:479-484`、`tests/imoScene/imoSceneMain.cpp:232-235`）。

### 2.2 管线与着色器现状

| 事实 | 证据 |
|---|---|
| 管线描述 `sPIPELINE_DESC` 是唯一描述源，缓存键 = 描述字段 + SPIR-V 内容哈希，同键只建一次 | `src/graphics/GN_Pipeline.h:10-13,61,121,140` |
| 图形/计算管线缓存 `cPIPELINE_CACHE` 在 `GN_VkContext::aPipelineCache` 中集中持有；既有 11 条管线已接入（P1b） | `src/graphics/_Vulkan/GN_Vk.h:183`；`docs/3D_FEATURE_PLAN.md:373` |
| 管线缓存条目只增不删，`Release()` 才统一销毁；没有按帧/按程序回收 | `src/graphics/_Vulkan/vkPipeline.cpp:244-310` |
| 每个 pass 的管线在 init/resize 时手写创建（前向/PBR/阴影/G-Buffer/延迟/合成/天空盒/…） | `src/graphics/_Vulkan/vk3dInit.cpp:262,306,319`；`vkGbuffer.cpp:298` |
| 固定视口管线在 resize 时重建（已有完整重建链） | `docs/3D_FEATURE_PLAN.md:65`（§0 #11） |
| 着色器以 `.spv` 文件在运行时加载，**部分** shader 还有嵌入式 SPIR-V 回退；SPIR-V 校验只有「魔数 + 4 字节对齐」 | 加载：`src/graphics/_Vulkan/GN_Vk.cpp:1068-1095`；回退数组：同文件 `:1044-1048,1072-1095`；校验：`GN_Shader.h:25-31`、`vkShader.cpp:38-39`、`GN_Shader.cpp:49-53` |
| 45 个 GLSL 通过 `make shaders`（glslangValidator）离线编译成 `.spv`；该目标**不生成** `*_spv.h` | `Makefile:472-482`；`assets/shaders/` |
| 明确「不引 SPIRV-Cross」，所以没有自动反射描述符布局 | `src/graphics/GN_Pipeline.h:5` |

### 2.3 绘制 API 现状（关键耦合点）

`GN_VkDraw3DPBR` 是一个「一体化」绘制函数，内部固定做了四件事：

1. 为**每个纹理组合**分配独立描述符集并绑定（`src/graphics/_Vulkan/vk3dForward.cpp:30-60,343-347`）；
2. 用固定 `VkModelPushConstPBR` 写 push constant（`GN_Vk.h:127-131`）；
3. 按 `alphaBlend` 在两条硬编码管线之间二选一（`vk3dForward.cpp:376-381`，管线字段见 `GN_Vk.h:246-247`）；
4. 把顶点/索引立刻 memcpy 进动态缓冲再 `vkCmdDraw*`（`vk3dForward.cpp:383-420`）。

**结论**：物体、材质、着色器、管线四者被写死在一个函数调用签名里。
要支持「同一物体不同层用不同程序」，仅靠加参数会迅速膨胀；必须把「选择」从 draw 函数中抽出来。

### 2.4 材质现状

| 层 | 结构 | 能力 | 缺口 |
|---|---|---|---|
| CPU 光照属性 | `GN3d::sMATERIAL`（`GN_3Dmaterial.h:28-45`） | diffuse/specular/shininess/emissive/roughness/metallic/alpha mode/cutoff/两张纹理 | 没有程序/层/自定义参数 |
| 资产描述 | `GN3d::sASSET_MATERIAL`（`GN_3Dasset.h:85-95`） | glTF PBR 因子 + alpha + 纹理索引 | 同上；`alphaMode BLEND` 已被 glTF 导入侧采用 |
| GPU 材质实例 | `GN3d::sMATERIAL_DESC/RES` + `cMATERIAL_CACHE`（`GN_Material.h:33-70`） | 管线键 + 最多 8 个绑定（UBO / 组合采样器），CPU 去重 | 固定 set 0 两槽位（`vkMaterial.cpp`）；与 PBR 的 12 槽位不是同一套布局 |
| 网格 | `sMESH::aMaterial`（`GN_3Dmodel.h:44`） | 一个网格一个材质 | 绘制路径仍走 PBR 固定函数 |

### 2.5 描述符布局现状：canonical 之外至少五套（含 legacy）

> 标题原写「三套」不准确：除前向/PBR/G-Buffer 外，旧材质实例与蒙皮也各有一套布局。
> 这些 legacy 布局在 v1 **保留**（§3.8），canonical 只服务新程序路径。

| 路径 | set 0 槽位 | 材质纹理在哪 | 证据 |
|---|---|---|---|
| 前向 Blinn-Phong | 0 场景 UBO、1 纹理、2 光照 UBO、3 雾 UBO、4 GI UBO | set 0 binding 1 | `src/graphics/_Vulkan/vk3dInit.cpp:360-403` |
| PBR | 0 场景 UBO、**1 albedo**、2 光照、**3 normal**、4 阴影、5 天空、6 雾、7 GI、8 环境、9 BRDF LUT、10/11 大气 LUT | set 0 binding 1/3 | `vk3dInit.cpp:471-524`；`assets/shaders/gn3d_pbr.frag:21-50` |
| G-Buffer | 0 场景 UBO、**1 albedo**、**3 normal**（槽 2 空） | set 0 binding 1/3 | `src/graphics/_Vulkan/vkGbuffer.cpp:227-250` |
| legacy 材质实例 | 0 材质 UBO、1 组合采样器（固定两槽） | set 0 binding 1 | `src/graphics/_Vulkan/vkMaterial.cpp:39-47` |
| legacy 蒙皮（UBO/SSBO） | 0 场景 UBO、1 palette（UBO 或 SSBO 二选一） | — | `src/graphics/_Vulkan/vkSkinDraw.cpp:91`；`vkSkinSsbo.cpp:87` |

**冲突**：前向把 binding 3 当雾，PBR 把 binding 3 当法线贴图——同一套「场景绑定」不可能直接共享。
这是本规划必须先解决的结构性问题（§3.4）。

### 2.6 差距表

| 能力 | 现状 | 目标 |
|---|---|---|
| 物体 → 着色器选择 | 无；由调用哪个 `GN_VkDraw3D*` 决定 | 材质持有程序句柄，逐层可覆盖 |
| 逐物体多 pass | 只有 PBR 的「不透明一趟 + 透明排序第二趟」；阴影由 `cMODEL::DrawShadow` 单独调用 | 层掩码驱动，引擎/调用方按层提交 |
| 层顺序 | 调用方手写 | 固定层序 + 每层状态表 |
| 统一绑定表 | 五套（前向 5 / PBR 12 / G-Buffer 3 / legacy 材质 2 / legacy 蒙皮 2） | set0/set1/set2 一套 + 版本号 |
| 管线变体 | 按 pass 手写；透明靠第二条 `pipelinePBRBlend` | 程序入口 × 状态 × 特殊化常量，缓存键统一 |
| 外部 SPIR-V | 只能加载引擎内置 `.spv`，无程序级描述 | 显式程序描述 + 校验 + 原子拒绝 + 回退 |
| 热重载 | 无 | 帧边界换程序 + 旧管线延迟回收 |
| 管线回收 | 只增不删 | 按帧/按程序回收 |

---

## 3. 目标架构

### 3.1 三层模型总览

```
                 ┌──────────────────────────────────────────────┐
                 │ 场景/调用方                                   │
                 │  for each mesh: RenderItem{mesh, material,   │
                 │      layerMask, layerProgram[LAYER_COUNT]}   │
                 └───────────────────┬──────────────────────────┘
                                     │ 提交
                 ┌───────────────────▼──────────────────────────┐
                 │ cRENDER_QUEUE（每层一个队列，CPU）            │
                 │  - 按 layerMask 入队                          │
                 │  - 层内按 (program, material, depth) 稳定排序  │
                 └───────────────────┬──────────────────────────┘
                                     │ 层执行
       ┌─────────────────────────────▼────────────────────────────────┐
       │ 固定层序（v1）                                                 │
       │  Shadow → GBuffer → [SSAO/SSGI/DeferredLight] → ForwardOpaque │
       │  → Outline → ForwardAlpha → Effect → Overlay                   │
       └─────────────────────────────┬────────────────────────────────┘
                                     │ 每个 RenderItem × 层
                 ┌───────────────────▼──────────────────────────┐
                 │ 解析：层掩码 → 程序 → 入口 → 状态 → 管线变体  │
                 │  cSHADER_PROGRAM_REGISTRY::Resolve()          │
                 │  cPIPELINE_CACHE::GetOrCreate()               │
                 └───────────────────┬──────────────────────────┘
                                     │
                 ┌───────────────────▼──────────────────────────┐
                 │ 绑定：set0 场景 / set1 材质 / set2 蒙皮       │
                 │ push constant 128B：模型 + 每 draw 覆盖参数    │
                 └──────────────────────────────────────────────┘
```

**分层原则**：CPU 侧（层/程序/材质/渲染项/解析/排序）与 Vulkan 侧（管线/描述符/命令录制）分离。
CPU 侧只用 `std::vector`、纯函数，可无 GPU 单测；Vulkan 侧只消费冻结的契约。
这与项目现有 `GN_Shader` / `GN_Pipeline` / `GN_Material` 的「CPU 描述可单测」路线一致。

### 3.2 层定义（v1 固定顺序与状态）

| 序号 | 层 | 目标 / render pass | 深度 | 混合 | 剔除 | 排序 | 典型程序入口 |
|---:|---|---|---|---|---|---|---|
| 0 | `eLAYER_SHADOW` | 阴影 CSM（D32，无颜色） | test+write，bias | 无 | 背面（可配置双面） | 级内按物体 | depth（内置） |
| 1 | `eLAYER_GBUFFER` | G-Buffer 3 MRT + 深度 | test+write | 无 | 背面 | 前→后 | pbr 的 gbuffer 入口 |
| — | （SSAO / SSGI / 延迟光照） | 全屏 pass，不是物体层 | — | — | — | — | 现有专用管线 |
| 2 | `eLAYER_FORWARD_OPAQUE` | 前向 HDR MSAA + resolve | test+write | 无 | 背面 | 前→后 | pbr / unlit / toon 的前向入口 |
| 3 | `eLAYER_OUTLINE` | 同一前向 pass | test，**write off** | alpha | **正面**（反向外扩壳） | 逐物体 | outline |
| 4 | `eLAYER_FORWARD_ALPHA` | 同一前向 pass | test，**write off** | alpha（标准） | 不剔除 | 后→前 | pbr 的 blend 入口 / toon 的 blend 入口 |
| 5 | `eLAYER_EFFECT` | 同一前向 pass | test，write off | alpha（可预乘） | 不剔除 | 效果自定义 | 现有粒子/云/贴花/降水专用管线（v1 不通用化） |
| 6 | `eLAYER_OVERLAY` | 合成后的 swapchain（2D/UI） | 无 | 2D 混合 | — | 提交序 | 现有 2D 批处理（v1 不通用化） |

**关于描边层**：当前 3D 深度格式默认是 `VK_FORMAT_D32_SFLOAT`（字段见 `GN_Vk.h:224`；初始化时按设备能力选择，
见 `src/graphics/_Vulkan/vk3dInit.cpp:21-29`，不支持时可能落到 `D24_UNORM_S8_UINT`），**不保证有模板位**。
因此 v1 的描边只能走「反向外扩壳 + 深度测试 + 不写深度」，不能依赖模板；若后续要模板描边，
必须把深度格式改为 `D24_UNORM_S8_UINT` 并重建全部相关 target/管线——登记为未决项 Q3，不在 v1 偷偷做。

**关于延迟/前向混排**：`eLAYER_GBUFFER` 与 `eLAYER_FORWARD_*` 可以同时启用：
标准 PBR 物体进 GBuffer，卡通/自定义程序只能走前向，于是延迟光照 pass 之后、前向层里再画。
这是「多层」真正的价值，但 v1 先保证**单一渲染路径**（前向或延迟）可用，混排放 P6。

### 3.3 ShaderProgram 模型

一个程序 = 一个名字 + 一个层掩码 + 每层可选入口 + 共享的顶点格式/状态/特殊化常量。

```cpp
// 概念草案（最终以 P0 冻结的头文件为准；这里只表达契约）
namespace GN3d {

enum eRENDER_LAYER {
    eLAYER_SHADOW = 0,
    eLAYER_GBUFFER,
    eLAYER_FORWARD_OPAQUE,
    eLAYER_OUTLINE,
    eLAYER_FORWARD_ALPHA,
    eLAYER_EFFECT,
    eLAYER_OVERLAY,
    eLAYER_COUNT
};

enum eVERTEX_FORMAT {
    eVF_STATIC = 0,   // VkVertex3D：位置/法线/切线/UV（11 float，44 B）
    eVF_SKINNED,      // sSKINNED_VERTEX：68 B（P2/P9 已有）
    eVF_CUSTOM        // v1 拒绝；保留桩
};

enum eSKIN_DATA {
    eSKIN_NONE = 0,
    eSKIN_UBO,        // set2 binding0 = 8 KB palette UBO（≤128 关节）
    eSKIN_SSBO        // set2 binding0 = storage buffer（≤4096 关节）
};

enum ePROGRAM_FLAGS {
    ePROG_IMPLICIT_SHADOW = 1u << 0 // 无 SHADOW 入口时，允许引擎用内置 depth 代打阴影
};

struct sSHADER_ENTRY {
    const void *aVertSpv; uint32_t aVertSize;
    const void *aFragSpv; uint32_t aFragSize;
    // -1 = 层默认；0/1 = 覆盖
    int32_t aBlend;            // ePIPELINE_BLEND；-1 = 层默认
    int32_t aDepthTest;        // -1 默认 / 0 / 1
    int32_t aDepthWrite;       // -1 默认 / 0 / 1
    int32_t aCullBack;         // -1 默认 / 0 / 1
    int32_t aTopology;         // -1 默认 = TRIANGLE_LIST
    // 特殊化常量（v1 最多 8 个）
    uint32_t aSpecCount;
    sSPEC_CONST aSpec[8];
};

struct sSHADER_PROGRAM_DESC {
    const char *aName;          // 唯一；区分大小写
    uint32_t aLayerMask;        // 该程序支持的层（1u << eLAYER_*）
    uint32_t aFlags;            // ePROGRAM_FLAGS
    eVERTEX_FORMAT aVertexFormat;
    eSKIN_DATA aSkinData;
    sSHADER_ENTRY aEntries[eLAYER_COUNT]; // 未用层留空
    uint32_t aLayoutVersion;    // 必须等于引擎当前 kBindingLayoutVersion
};

} // namespace GN3d
```

**契约要点**

1. 程序必须至少有一个非空入口；`aLayerMask` 与入口必须一致（第 `i` 层有入口 ⇔ 掩码第 `i` 位为 1）。
2. 所有入口共用同一个顶点格式；v1 不允许一个程序在不同层用不同顶点格式。
3. `ePROG_IMPLICIT_SHADOW` 只允许在「没有 SHADOW 入口」时置位；置位后阴影层由内置 `gn.depth` 代打。
4. 入口的 VS 输出 / FS 输入接口必须与目标层一致；引擎无反射，**只能靠**：
   - 内置程序由引擎作者保证；
   - 外部程序由管线创建成功 + 校验层 + gpuCheck 像素断言保证（见 ADR-1 的边界）。
5. 特殊化常量进管线键；同一程序不同常量值 = 不同管线变体（Vulkan 规范：特殊化常量在管线创建时指定，
   [Pipelines · Specialization Constants](https://docs.vulkan.org/spec/latest/chapters/pipelines.html)）。
6. 状态覆盖只能**收紧**层策略，不能违反层不变式：例如 G-Buffer 层不允许关深度写、不允许混合；
   校验失败 = 程序整体拒绝（不是画错一帧）。

### 3.4 绑定契约（本规划最重要的冻结项）

#### 3.4.1 为什么必须冻结

项目明确不引 SPIRV-Cross（`GN_Pipeline.h:5`），即**不能从 SPIR-V 反射出描述符布局**。
若每个程序自带一套 `VkDescriptorSetLayout`，则：
- 外部程序无法可靠对齐引擎提供的场景/光照/阴影数据；
- 每换一个程序就要换 set layout，描述符集无法复用，绘制状态切换爆炸；
- 管线创建失败的根因难定位（布局不匹配在驱动层只给一个笼统错误）。

因此 v1 采用**固定绑定表 + 版本号**：所有程序共享同一套 set 布局；程序用不到的 binding 留默认值。
这是「显式描述」路线的必然结果，不是可选项。

#### 3.4.2 set 0：场景/帧（版本 `kBindingLayoutVersion = 1`）

沿用现有 PBR 槽位编号（减少迁移面），把材质纹理从 set 0 移走后，1/3 成为保留槽：

| binding | 类型 | 内容 | 谁写 | 备注 |
|---:|---|---|---|---|
| 0 | UNIFORM_BUFFER | `VkSceneUBO`（VP / 相机 / 环境光，96 B） | 引擎每帧 | 现有 |
| 1 | COMBINED_IMAGE_SAMPLER | 保留（默认白纹理） | 引擎 | 原 PBR albedo；迁移后不再由程序采样 |
| 2 | UNIFORM_BUFFER | `sLIGHT_UBO`（8 光源） | 引擎每帧 | 现有 |
| 3 | COMBINED_IMAGE_SAMPLER | 保留（默认白纹理） | 引擎 | 原 PBR normal |
| 4 | COMBINED_IMAGE_SAMPLER | 阴影 `sampler2DArray`（4 级 CSM） | 引擎每帧 | 现有 |
| 5 | COMBINED_IMAGE_SAMPLER | 天空 cubemap | 引擎每帧 | 现有 |
| 6 | UNIFORM_BUFFER | 雾 / 天气 UBO（64 B） | 引擎每帧 | 现有 |
| 7 | UNIFORM_BUFFER | GI UBO（256 B） | 引擎每帧 | 现有 |
| 8 | COMBINED_IMAGE_SAMPLER | 预滤波环境 cubemap | 引擎每帧 | 现有 |
| 9 | COMBINED_IMAGE_SAMPLER | BRDF LUT（RG16F） | 引擎每帧 | 现有 |
| 10 | COMBINED_IMAGE_SAMPLER | 大气透射率 LUT | 引擎每帧 | 现有 |
| 11 | COMBINED_IMAGE_SAMPLER | sky-view LUT | 引擎每帧 | 现有 |
| 12 | UNIFORM_BUFFER（64 B） | **SHADOW 层专用**：当前 cascade 的 lightVP 矩阵 | 引擎每级阴影 pass | 新增；见 §3.4.4 的布局例外 |

**迁移动作**：前向 Blinn-Phong 的雾从 binding 3 改到 6（`gn3d.frag` 未使用 GI，不需要改 GI 槽）；
G-Buffer 的 albedo/normal 从 1/3 移到 set 1 的 1/2；阴影 shader（`gn3d_shadow.vert`）从 binding 0 改读 binding 12。
这正是 `gn3d_pbr.frag` / `gn3d.frag` / `gn3d_gbuffer.frag` / `gn3d_shadow.vert` 需要一起改的原因。

#### 3.4.3 set 1：材质（所有程序共享）

| binding | 类型 | 内容 |
|---:|---|---|
| 0 | UNIFORM_BUFFER（256 B） | `sMATERIAL_UBO`：albedo vec4（rgb + **alphaCutoff**）、pbr vec4（rough/metal/ao/reserved）、emissive vec4、保留 vec4、自定义参数 12×vec4 |
| 1 | COMBINED_IMAGE_SAMPLER | albedo / baseColor |
| 2 | COMBINED_IMAGE_SAMPLER | normal |
| 3 | COMBINED_IMAGE_SAMPLER | metallic-roughness |
| 4 | COMBINED_IMAGE_SAMPLER | emissive |
| 5 | COMBINED_IMAGE_SAMPLER | occlusion |
| 6 | COMBINED_IMAGE_SAMPLER | 自定义纹理 0 |
| 7 | COMBINED_IMAGE_SAMPLER | 自定义纹理 1 |

- 未绑定的槽位一律绑 1×1 白纹理 + 默认采样器（与现有 `defaultImageView/defaultSampler` 一致）。
- 材质 UBO 固定 256 B：给后续 toon/water 等程序留参数空间；**不按程序变化**，因此一套布局通吃。
- 材质描述符集**按材质实例**缓存（每个 `sMATERIAL_INSTANCE` 一个 set），不是按 UBO 内容做键：
  材质参数可变，内容键会导致每帧新建 set 或命中已过期的 set。材质 UBO 用双缓冲（当前帧/上一帧各一份）
  或只在帧边界更新，避免写正在被 GPU 读取的 UBO。材质销毁时归还 set。

#### 3.4.4 set 2：蒙皮（可选，只在 `aSkinData != eSKIN_NONE` 时进 pipeline layout）

| binding | 类型 | 内容 |
|---:|---|---|
| 0 | UNIFORM_BUFFER（8 KB） | 128 关节 palette（`eSKIN_UBO`） |
| 0 | STORAGE_BUFFER | 4096 关节 palette（`eSKIN_SSBO`） |

两种变体二选一，不能同时出现在一个 pipeline layout。静态程序不创建 set 2，避免无谓绑定。

**pipeline layout 按层分三种（不是所有层都三套 set）**：

| 层 | 使用的 set | 说明 |
|---|---|---|
| `eLAYER_SHADOW` | 仅 set 0（**shadow 专用 layout**：只含 binding 12 = per-cascade lightVP） | 深度专用，不需要材质/蒙皮；**每 cascade 一个描述符集**；不绑定 canonical 场景 set（两套 layout 不兼容，见下）；push constant 仅模型 64 B |
| `eLAYER_GBUFFER` / `eLAYER_FORWARD_*` / `eLAYER_OUTLINE` | set 0 + set 1 | 静态程序；set 0 用 canonical 13 槽 layout |
| 同上，且 `aSkinData != eSKIN_NONE` | set 0 + set 1 + set 2 | 蒙皮程序 |

**阴影 layout 是明确例外**：canonical set0 有 13 个 binding，而 shadow 只需要 binding12。Vulkan 的
descriptor set layout 兼容性不接受「子集」——把 canonical 场景 set 绑到只含 binding12 的管线布局上是非法用法。
因此 SHADOW 层单独创建 `VkDescriptorSetLayout`（只含 binding12）与对应 pipeline layout，每级 cascade 一个 set；
它的 binding 编号仍与 canonical 表一致（12），外部 shadow 程序按这个例外写。这样阴影层不必伪造材质描述符集，
也不会因为「set 0 binding 0 是场景 UBO」而拿不到每级 lightVP。

set 2 的 palette 用**环形槽**（例如 4 个 set/UBO 槽轮转），不是每个蒙皮 draw 一个 set；
调用方在同一帧提交超过槽数的蒙皮项时，按槽回收/等待，并在调试计数里暴露。

#### 3.4.5 push constant（128 B，Vulkan 最小保证值）

| 偏移 | 大小 | 内容 |
|---:|---:|---|
| 0 | 64 B | 模型矩阵（列主序 `mat4`） |
| 64 | 64 B | 每 draw 覆盖参数：16 个 float（按程序解释，例如透明淡出、描边宽度、色调） |

- 128 B 正好是 Vulkan 规范保证的最小 `maxPushConstantsSize`；**没有余量**。
  需要更多每物体参数时，必须走 set 1 材质 UBO 的 `aCustom[12×vec4]`，不得扩 push constant。
- 现有 `VkModelPushConstPBR` 是 96 B（`GN_Vk.h:127-131`），落在 128 B 内；迁移后材质参数移到 set 1，
  push constant 只留模型矩阵 + 覆盖参数，语义更清晰。

#### 3.4.6 描述符池与扩容

现有 `descriptorPool` 尺寸是按旧管线估算的（`docs/3D_FEATURE_PLAN.md:738` 已登记要调；实际值见
`src/graphics/_Vulkan/GN_Vk.cpp:432-443`：maxSets 1024 / UBO 1024 / sampler 1024 / storage 256）。
新方案会新增：
- 每个**材质实例**一个 set 1（不是每个纹理组合；随材质销毁归还）；
- set 2 的环形槽（固定 4 个 set），不是每 draw 一个；
- 现有 PBR 纹理 set 缓存（`vk3dForward.cpp:25`）迁移后删除。

**容量账（P2 必须写进实现与验收）**：

| 用途 | 上限来源 | 估算 |
|---|---|---|
| 材质 set 1 | `kMaxMaterialInstances`（建议 1024） | 1024 sets ×（1 UBO + 7 sampler） |
| 蒙皮 set 2 | 环形 4 槽 | 4 sets ×（1 UBO 或 1 storage） |
| 场景 set 0 | 每 pass 一份（前向/PBR/GBuffer/阴影×4） | < 16 |
| legacy 材质/蒙皮 | 旧路径保留 | 按现有 gpuCheck 需求 |

P2 必须同时调整池尺寸并加「池耗尽」的可观测计数（`descriptorPoolExhausted`）；
`GN_VkSubmitRenderItem` 在 set 分配失败时返回 false 并计数，不允许静默回退到共享 set（那正是
commit `6dd2e66` 修掉的「多网格采样同一贴图」缺陷的成因）。

### 3.5 解析、排序与执行

#### 3.5.1 层掩码解析（伪代码，冻结语义）

```text
resolve(item, layer):
    // 三层回退：item 显式 → 材质默认 → 程序默认；任何一层为 0 都继续往下取
    mask = item.layerMask ? item.layerMask
         : item.material->aLayerMask ? item.material->aLayerMask
         : item.material->aProgram->aLayerMask
    if (mask & (1<<layer)) == 0: return SKIP

    prog = item.layerProgram[layer] ?: item.material->aProgram
    entry = prog.entry[layer]
    if entry == null:
        if layer == eLAYER_SHADOW and (prog.aFlags & ePROG_IMPLICIT_SHADOW):
            entry = builtinDepth.entry[eLAYER_SHADOW]   // 显式选择退让，不是每层隐式回退
        else:
            warnOnce(prog.name, "layer not supported", layer)
            return SKIP

    pipeline = pipelineCache.GetOrCreate(key(prog, entry, layer, vertexFormat, pass, spec))
    if pipeline == null:
        warnOnce(prog.name, "pipeline create failed")
        return SKIP          // 不崩、不画半截

    draw(item, pipeline, bindMaterialSet(item.material, layer), pushConstants(item))
```

- **默认层掩码**优先用材质；材质为 0 时用程序默认。`item.layerMask != 0` 时完全覆盖（不是取交集）。
- **`aMaterial` 必须非空**（v1 由 `ValidateRenderItem` 拒绝空指针）：阴影/描边这类无光照程序也给一个
  默认材质实例（程序 = 对应程序，UBO 用默认值），避免在解析里做空指针分支。
- **逐层覆盖**是「同一物体多套着色器」的唯一入口；没有覆盖时同一程序服务所有层。
- 程序不支持某层时默认**跳过**并只警告一次；阴影层例外：程序可置 `ePROG_IMPLICIT_SHADOW`，
  让引擎用内置 depth 程序代打（例如卡通角色仍要投影），且该标志只允许在「没有 SHADOW 入口」时置位。
- 管线创建失败不回退到「另一个程序」——因为顶点格式/层接口可能不兼容；
  正确回退发生在**程序注册/加载阶段**（见 §3.7），此时整项改用内置 PBR 程序。
- SHADOW 层的 `bindMaterialSet` 不绑定 set 1（该层 pipeline layout 没有 set 1，见 §3.4.4）。

#### 3.5.2 排序键

层内稳定排序，键依次为：
1. `programId`（程序注册序号，越小越先）；
2. `materialId`（材质实例 id）；
3. 视空间深度（用当前 view 矩阵算 `viewZ`，不是到相机的欧氏距离：
   `eLAYER_FORWARD_OPAQUE` / `eLAYER_GBUFFER` 前→后；`eLAYER_FORWARD_ALPHA` 后→前；阴影层按级内物体）；
4. `itemId`（提交序号，保证稳定）。

排序目标：减少 `vkCmdBindPipeline` / `vkCmdBindDescriptorSets` 切换，其次才是正确性。
现有 `GN_3Dbatch` 已有稳定排序与切换统计（`docs/3D_FEATURE_PLAN.md:391` 的 P9-c），可复用其比较器思路。
`SortRenderItems` 必须接收 view 矩阵或已算好的 `viewZ`（不能用相机距离代替，广角/大物体时顺序会错）。

#### 3.5.3 提交/执行 API（v1 与现有 API 共存）

v1 **不推翻**现有 `GN_VkBegin3DPass` / `GN_VkDraw3DPBR`，而是在旁边加一组显式层 API：

```cpp
// CPU 侧：构造渲染项（不产生 GPU 工作）
struct sRENDER_ITEM {
    const void *aVertices; uint32_t aVertexCount;   // 借用：必须活到队列 reset
    const uint32_t *aIndices; uint32_t aIndexCount; // 借用：必须活到队列 reset
    GNg::sMAT44 aModelMatrix;                        // 按值复制进队列
    const sMATERIAL_INSTANCE *aMaterial;             // 非空；必须活到队列 reset
    uint32_t aLayerMask;                             // 0 = 材质/程序默认
    const sSHADER_PROGRAM *aLayerProgram[eLAYER_COUNT]; // 可空；指针按值复制
    uint32_t aSortKey;
};
bool GN_VkSubmitRenderItem(const sRENDER_ITEM &item); // 满队/非法返回 false（不静默丢）
bool GN_VkFlushLayer(GN3d::eRENDER_LAYER layer);      // 在当前打开的 pass 内执行该层
// 按固定顺序执行「与当前 pass 兼容」的所有已提交层；返回实际执行的层数。
// 不兼容的层（例如在 forward pass 里调它时的 SHADOW）原样留在队列，不静默丢弃。
uint32_t GN_VkFlushAllLayers();
void GN_VkResetRenderQueue();   // 清空队列；由 GN_VkBeginFrame 在帧首调用，也可显式调用
```

**生命周期与消费语义（冻结）**：
- `GN_VkSubmitRenderItem` 只复制 `sRENDER_ITEM` 结构与模型矩阵；**顶点/索引/材质/程序指针是借用的**，
  必须活到下一次 `GN_VkResetRenderQueue()`。调用方不得提交栈上的临时顶点数组后立刻离开作用域。
- `GN_VkFlushLayer` **不消费**队列：同一批 item 可以在多个 pass 里重复 flush（CSM 4 级正是这样用：
  每级阴影 pass 各 flush 一次 SHADOW）。同一 pass 内对同一层重复 flush 会重复绘制，调用方负责只调一次。
- 队列在 `GN_VkBeginFrame` 开头清空（实现放在既有帧开始路径），避免跨帧累积；显式 `Reset` 用于
  中途重提交（例如 resize/场景切换）。

调用方形态（前向路径；先提交、再按 pass 分段 flush）：

```cpp
// 帧首：GN_VkBeginFrame 内部已 ResetRenderQueue
for (obj : scene) GN_VkSubmitRenderItem(makeItem(obj));

for (cascade = 0; cascade < GN_VkShadowCascadeCount(); ++cascade) {
    GN_VkShadowPassBegin(lightVP[cascade], cascade);
        GN_VkFlushLayer(eLAYER_SHADOW);   // 同一批 item 每级各画一次（不消费）
    GN_VkShadowPassEnd();
}

GN_VkBegin3DPass(&vp, &cam, light, size);
    GN_VkFlushAllLayers();               // 执行 FORWARD_OPAQUE / OUTLINE / FORWARD_ALPHA
GN_VkEnd3DPass();
```

**约束**：层与当前打开的 pass 必须匹配；`GN_VkFlushLayer` 在错误 pass 内调用返回 false 并只警告一次。
阴影层跨越独立 render pass，因此**必须**在阴影 pass 内单独 flush；`GN_VkFlushAllLayers` 只执行
`SHADOW` 之外的、与当前 pass 兼容的层（实现上按层表的 pass 归属过滤），不会把阴影项丢掉或画错目标。
P4 验收必须断言「4 个 cascade 的阴影图各自有深度」，防止只画了第 0 级。

v2（P6 之后）再提供 `GN_VkSceneExecute()` 统一编排 shadow/gbuffer/deferred/forward，调用方只提交一次。
**v1 不做**自动编排，避免把现有测试程序（mmdViewer/imoScene/cornell）一次性重写。

### 3.6 管线缓存 v2

现有 `sPIPELINE_DESC`（`GN_Pipeline.h:61-108`）已经覆盖：kind、VS/FS/CS、顶点布局、render pass/subpass、layout、
深度/混合/剔除/拓扑/采样数、颜色附件数、viewport/scissor、自定义混合。需要补的只有：

| 新增字段 | 原因 | 进缓存键 |
|---|---|---|
| `aSpecCount` + `aSpec[8]` | 特殊化常量是管线创建期常量，不同值必须不同管线 | 是 |
| `aKeyVersion`（如 `2`） | 绑定契约/描述语义变更时让旧缓存整体失效，避免旧键误命中 | 是 |
| `aLastUsedFrame`（非键） | 热重载后按帧回收旧管线 | 否 |

**回收**：`cPIPELINE_CACHE` 增加 `CollectGarbage(currentFrame, keepFrames = 2)`：
- 只在帧边界、且已 `vkWaitForFences` 后调用；
- 保留最近 2 帧用过的入口（单 in-flight fence 下足够覆盖在飞命令）；
- `Release()` 仍负责最终销毁。
- 着色器模块缓存 `cSHADER_CACHE`（`GN_Shader.h:46-70`）同样只增不删，热重载会不断新增模块；
  给它加同样的 `aLastUsedFrame` 与 `CollectGarbage`，与管线回收同批执行，避免显存/驱动对象缓慢泄漏。
**不在 v1 做驱动级 `VkPipelineCache` 磁盘缓存**（可跨进程复用，但数据与设备/驱动强相关，
[VkPipelineCache](https://github.khronos.org/Vulkan-Site/refpages/latest/refpages/source/VkPipelineCache.html) 明确内容由实现管理）；
登记为 P6 可选项，并必须校验设备 UUID/驱动版本后才复用。

### 3.7 内置程序与外部 SPIR-V

#### 3.7.1 内置程序（v1 至少六个；P2 先 `gn.pbr`/`gn.depth`，P3/P4 再补 unlit/toon/outline）

| 程序名 | 层掩码 | 顶点格式 | 说明 |
|---|---|---|---|
| `gn.pbr` | SHADOW + GBUFFER + FORWARD_OPAQUE + FORWARD_ALPHA | STATIC | 现有 PBR 的四个入口；阴影入口复用 depth（读 set0 binding12 的 lightVP），GBuffer 入口复用 `gn3d_gbuffer.*`，前向入口复用 `gn3d_pbr.*`，透明入口复用 `gn3d_pbr.*` + blend 状态 |
| `gn.unlit` | FORWARD_OPAQUE + FORWARD_ALPHA | STATIC | P3 新建：albedo 纹理 × 材质色，无光照；证明「不同着色器」最小闭环 |
| `gn.toon` | FORWARD_OPAQUE + FORWARD_ALPHA | STATIC | P4 新建：2–3 段硬边光照；证明同一物体可换程序，且可只走前向 |
| `gn.depth` | SHADOW | STATIC | 现有 `gn3d_shadow.vert`（深度专用，迁移到 binding12） |
| `gn.outline` | OUTLINE | STATIC | P4 新建：反向外扩壳 + 纯色；不依赖模板 |
| `gn.skinned.pbr`（P6） | 同 `gn.pbr` | SKINNED | P6：蒙皮对象走同一套层/材质；set2 蒙皮数据。P4 之前 legacy 蒙皮路径保持不变 |

内置程序的 SPIR-V 来源：v1 仍从 `assets/shaders/*.spv` 加载（`GN_Vk.cpp:1068-1095`），
但**现状已有内嵌回退数组**（`GN_Vk.cpp:1044-1048` 的 `*_spv.h`，用于文件缺失时回退），且
`make shaders` **不生成**这些头文件（`Makefile:472-482`）。因此 P2 必须二选一并写进验收：
- **方案 A（推荐）**：给 `make shaders` 加生成步骤，保证 `*_spv.h` 与 `.spv` 内容同步；
  gpuCheck 加 `spv.header.sync`（比对头文件数组哈希与 `.spv` 文件哈希）。
- **方案 B**：删除内嵌回退，部署必须带 `assets/shaders/`；在 README/启动日志写明。
无论选哪个，**P2 改绑定后绝不能让旧内嵌数组继续生效**——那会用旧 binding 配新布局（H1 的真实风险）。

#### 3.7.2 外部 SPIR-V 的加载契约

```cpp
struct sSHADER_FILE_ENTRY {
    GN3d::eRENDER_LAYER aLayer;
    const char *aVertPath;   // .spv（相对工作目录或绝对路径；由调用方决定）
    const char *aFragPath;   // 可空 = 深度专用层（仅 SHADOW）
    uint32_t aPushConstantSize; // ≤128；SHADOW 层只用模型 64 B
    int32_t aBlend;            // -1 = 层默认
    int32_t aDepthTest;        // -1 = 层默认
    int32_t aDepthWrite;       // -1 = 层默认
    int32_t aCullBack;         // -1 = 层默认
    uint32_t aSpecCount;
    GN3d::sSPEC_CONST aSpec[8];
};

struct sSHADER_LOAD_DESC {
    const char *aName;
    GN3d::eVERTEX_FORMAT aVertexFormat;
    GN3d::eSKIN_DATA aSkinData;
    uint32_t aEntryCount;
    sSHADER_FILE_ENTRY aEntries[GN3d::eLAYER_COUNT];
    uint32_t aLayoutVersion;   // 必须 = 引擎当前版本
    const uint8_t *aSha256;    // 可空；非空时校验每个 .spv 的 SHA-256（信任边界，P5）
};

// 成功返回 true；失败返回 false 并把原因写入 outError（不注册半成品）。
bool GN_VkShaderProgramLoad(const sSHADER_LOAD_DESC &desc, char *outError, size_t errorSize);
bool GN_VkShaderProgramReload(const char *name, char *outError, size_t errorSize);
void GN_VkShaderProgramUnload(const char *name);
const GN3d::sSHADER_PROGRAM *GN_VkShaderProgramFind(const char *name);
```

> **所有权**：注册表在注册时深拷贝 `aName` 与所有 SPIR-V 字节到自有缓冲，并让 `aDesc` 内的指针指向这些缓冲；
> 调用方传入的 `sSHADER_LOAD_DESC` 与文件读取缓冲在返回后即可释放。这是热重载/延迟建管线不悬空的前提。

**校验顺序（任一失败即整体拒绝，不留半注册状态）**

1. 名字非空、长度 ≤63、不与已有程序重名；
2. `aLayoutVersion == kBindingLayoutVersion`；
3. `aVertexFormat` 是已知值（v1 只接受 STATIC / SKINNED）；
4. `aSkinData` 与顶点格式一致（SKINNED 必须 UBO 或 SSBO；STATIC 必须 NONE）；
5. 入口数与层掩码一致；层号合法；每层 VS 必须存在；非 SHADOW 层 FS 必须存在；
6. 每个 `.spv`：读文件成功、非空、≤ 8 MiB、SPIR-V 魔数、4 字节对齐（复用现有 `IsSpirv`/`LoadSpirvFile`，
   `GN_Shader.h:25-31`）；
7. push constant 需求 ≤128 B（v1 由程序声明 `aPushConstantSize`，见 §7 草案）；
8. 状态覆盖满足层不变式（§3.2/§3.3：GBuffer 不许混合/关深度写；Outline 不许写深度；等）；
   同时检查层布局：SHADOW 入口按 set0（binding12=lightVP、无 set1）；其它层按 set0+set1；
9. 逐层 `GetOrCreate` 管线：先在**临时程序对象**上创建全部入口，全部成功后才注册；
   任一失败则不注册任何东西。注意：失败的变体已经进了 `cPIPELINE_CACHE`（归缓存所有，不能当场销毁），
   实现上把它们标成 `aLastUsedFrame = 0`，由下一次 `CollectGarbage` 回收；不得留下「已注册但没有管线」的半成品；
10. 注册成功后再允许 `RenderItem` 引用。

**回退语义**：
- 程序加载失败：调用方拿到 false + 原因；引用该名字的材质在解析时回退到 `gn.pbr`，并**只警告一次**；
- 程序热重载失败：保留旧程序继续用（旧程序不换），错误只记录；
- 管线创建失败：该 RenderItem 本层跳过并警告一次；不回退到其它程序（接口可能不兼容）。

**热重载**：
- 监听文件内容哈希（不是 mtime：mtime 可能被 touch 改变而内容不变）；
- 只在帧边界执行：帧内请求 → 置 pending，下一帧开头 `vkWaitForFences` 后重建；
- **程序对象地址稳定**：重载在原有 `sSHADER_PROGRAM` 上原地替换 `aRuntime`（新管线句柄），
  不替换对象指针；这样材质/队列里缓存的 `const sSHADER_PROGRAM *` 不会变成野指针；
- 新管线全部建成功后一次切换 `aRuntime`；失败则保留旧 `aRuntime`（旧程序继续用）；
- 旧管线由 `CollectGarbage` 按帧回收；**不**在命令缓冲可能仍在飞时销毁任何 VkPipeline。

#### 3.7.3 信任边界（必须写进使用者文档）

SPIR-V 是**可执行内容**，不是数据。加载外部 `.spv` 等于允许它进入 GPU 驱动：

- 引擎只能保证「文件格式/描述契约」的校验，**不能**保证驱动不会因畸形但合法的 SPIR-V 崩溃或挂起；
- 因此外部着色器必须来自**可信来源**；
- v1 提供的缓解手段（不是沙箱）：
  - 大小上限 8 MiB、魔数/对齐校验；
  - 可选 SHA-256 白名单（`sSHADER_LOAD_DESC::aSha256`，P5 加）；
  - 调试构建建议开启 Vulkan 校验层（`VK_LAYER_KHRONOS_validation`）捕获布局/状态错误；
  - 离线用 `spirv-val`（SPIRV-Tools）预检；**不把 spirv-val 做成运行时依赖**（本机不一定安装，
    `make shaders` 只用 glslangValidator）；
  - 加载失败/管线失败不崩、不画半截。
- 运行时**不编译 GLSL**：引擎只接受 SPIR-V；GLSL 由构建期 `make shaders` 或使用者自己的
  glslangValidator 编译。这样避免引入 glslang 运行时依赖，也避免「引擎里带一个编译器」的攻击面。

### 3.8 向后兼容

| 既有接口 | v1 处理 |
|---|---|
| `GN_VkDraw3D` / `GN_VkDraw3DPBR` / `GN_VkDraw3D_GBuffer` | 保留；内部继续**立即绘制**（不进队列），签名不变；canonical 绑定表在 P2 迁移后由这些函数内部适配。行为逐像素保持 |
| `cMODEL::DrawPBR` / `DrawPBRSorted` / `DrawPBRWithLod` | 保留旧路径（立即绘制）；P4 新增 `DrawLayered(...)` 作为新入口，不强制迁移 |
| `sMATERIAL` | 追加 `aShaderProgram`（默认 `gn.pbr`）与 `aLayerMask`（默认 PBR 默认掩码），旧字段不动 |
| `sMATERIAL_RES` / `GN_VkMaterialCreate` / `GN_VkDraw3DMaterial` | **保留独立 legacy 布局**：set0 只有 binding0 UBO + binding1 组合采样器（`vkMaterial.cpp:39-47`），配 `gn3d_material.*` 专用管线；**不参与** canonical set0。P6 再决定是否迁移或标记弃用 |
| `GN_VkDraw3DSkinned` / `GN_VkDraw3DSkinnedSsbo` | **保留 legacy 布局**：set0 = 场景 UBO + palette（UBO 见 `vkSkinDraw.cpp:91`；SSBO 见 `vkSkinSsbo.cpp:87`）；P4 的新蒙皮程序改为 canonical set0 + set2，二者在切换完成前并存 |
| 现有 gpuCheck 99 条 | 全部必须继续 PASS；新增检查单独分组 |
| 2D / UI | 不动 |

> **并存规则**：legacy 布局与 canonical 布局各自持有独立的 `VkPipelineLayout` / `VkDescriptorSetLayout`。
> 同一帧可以先后使用两者，但**不能把 legacy 描述符集绑到 canonical 管线布局上**；引擎在调试构建下对
> 「管线布局 / 描述符集布局」做断言（校验层也会报错）。两条路径的 descriptor pool 需求必须一起估算。

**默认层掩码**（仅描述**新队列路径** `GN_VkSubmitRenderItem` 的默认值；旧 API 立即绘制、不读掩码）：

| 新路径默认 | 层掩码 |
|---|---|
| 前向 PBR 材质 | `SHADOW \| FORWARD_OPAQUE`；`alphaBlend=true` 时 `FORWARD_ALPHA` |
| G-Buffer 材质 | `SHADOW \| GBUFFER` |
| 前向 Blinn-Phong 材质 | `FORWARD_OPAQUE`（不默认投影，保持现状） |
| 显式投影提交 | 调用方写 `SHADOW`，并在阴影 pass 内 flush |

> **旧 API 不进队列**：`GN_VkDraw3DPBR` 等立即函数不产生层掩码、不参与排序，也不会自动进阴影 pass；
> 这保证既有调用方的帧结构（`mmdViewer.cpp:479-484` 等）完全不改。

---

## 4. 关键决策（ADR）

### ADR-1 固定绑定表，不做 SPIR-V 反射

- **选择**：set0/set1/set2 + 128 B push constant 的固定表，程序声明兼容版本。
- **理由**：项目明确不引 SPIRV-Cross（`GN_Pipeline.h:5`）；自研反射等于重写一个 SPIR-V 解析器，
  且要覆盖所有扩展指令，风险远高于收益。固定表让外部程序「按文档写绑定」即可，行为确定、可单测。
- **代价**：外部程序不能自由定义绑定；引擎不能自动检查「程序是否真的只用了已有 binding」。
- **边界**：SPIR-V 里声明了表外的 binding，只会在管线创建/校验层报错或采样未定义数据；
  v1 用「管线创建 + gpuCheck + 校验层」兜住，不假装能静态证明。

### ADR-2 显式程序描述，不做运行时 GLSL 编译

- **选择**：程序描述是 C++ 结构体（P5 可选加 key=value 文本 sidecar，不引 JSON 库）。
- **理由**：引擎只依赖 Vulkan/GLFW，没有 JSON 解析器（cgltf 是 glTF 专用，不复用）；
  运行时 GLSL 需要链 glslang 或调用外部进程，增加体积、权限与攻击面。
- **替代**：`glslangValidator` 在构建期跑（现有 `make shaders`）；使用者自带 `.spv`。

### ADR-3 固定层序，不做完整 Render Graph

- **选择**：层序是编译期常量表，v1 不自动推导依赖/屏障/资源别名。
- **理由**：现有 pass 数量固定（阴影/G-Buffer/SSAO/SSGI/延迟/前向/合成），
  每个 pass 的 render pass 与 framebuffer 已手工管理（`vk3dPasses.cpp`、`vkGbuffer.cpp`）。
  引入 Render Graph 会同时改资源生命周期、屏障、resize 链，属另一个量级的重构。
- **边界**：新增 pass 仍需手写 render pass/framebuffer；层表只能排物体，不能自动生成 pass。

### ADR-4 用 VkPipeline，不用 VK_EXT_shader_object

- **选择**：v1 仍以 `VkPipeline` 为绑定单位。
- **理由**：`VK_EXT_shader_object` 是可选扩展，需要应用自己管理全部动态状态与跨阶段接口兼容，
  且当前设备/驱动未验证支持（[VK_EXT_shader_object](https://docs.vulkan.org/features/latest/features/proposals/VK_EXT_shader_object.html)
  明确把兼容性责任交给应用）。引入它会扩散到所有 pass 的状态管理，收益（减少管线对象）在本项目规模不成立。
- **替代触发条件**：若外部程序组合导致管线数量/编译时间成为实测瓶颈，再评估。

### ADR-5 v1 保留立即模式 + 层内排序，不做全场景队列

- **选择**：`GN_VkFlushLayer` 在调用方打开的 pass 内执行；队列只负责层内排序。
- **理由**：现有测试程序手写 pass 顺序；一次性改成「引擎自动编排」会同时改 3 个查看器与 gpuCheck，
  风险/h收益不成比例。
- **演进**：P6 再提供 `GN_VkSceneExecute()`，把 pass 编排也收进引擎。

### ADR-6 内置程序：文件优先 + 内嵌回退同步（或删除回退）

- **选择**：内置程序与外部程序走同一个注册表；内置程序 v1 从 `assets/shaders/*.spv` 加载。
- **现状**：前向/PBR/阴影三条已有内嵌 SPIR-V 回退（`GN_Vk.cpp:1044-1048,1072-1095`），
  但 `make shaders` 只生成 `.spv`、不生成 `*_spv.h`（`Makefile:472-482`）。P2 改绑定后，
  若继续保留旧回退数组，会在「无 assets」路径下用旧 binding 配新布局。
- **决定（P2 二选一，不允许默认忽略）**：
  - A：给 `make shaders` 加 `*_spv.h` 生成步骤，并加 `spv.header.sync` 校验；
  - B：删除内嵌回退，文档写明部署必须带 `assets/shaders/`。
- **代价**：A 需要改构建与生成脚本；B 减少无资源部署的容错。

---

## 5. 分阶段任务图

> **写域互斥**：每个阶段只动自己列出的文件；跨阶段共享文件（`GN_Vk.h`、`gn3d_pbr.frag` 等）按阶段串行，
> 不允许两个执行体同时改。所有新文件遵守 `code-modularity`：单功能 <500 有效行、多功能 <1000 有效行，
> camelCase 命名，交付前跑 lint。

```
P0 ──> P1 ──> P2 ──> P3 ──> P4 ──> P6
                        └──> P5（可与 P4 并行，但 P5 依赖 P3）
```

### P0 契约 + CPU 侧（无 GPU 行为变化）— ✅ 已完成（2026-11-21）

> 证据：`src/graphics/GN_RenderPipeline.h/.cpp`、`tests/unit/testRenderPipeline.cpp`；`make unit` = 404/0；
> lint = 0 违规 0 告警；`GN.h` 已接入。台账见 `docs/BACKLOG.md` B1。

**目标**：把 §3.2/§3.3/§3.5 的枚举、结构体、校验、层解析、排序写成可单测的纯 CPU 层。

| 项 | 内容 |
|---|---|
| 新文件 | `src/graphics/GN_RenderPipeline.h` / `.cpp`（层/程序/材质/渲染项/解析/排序，纯 CPU） |
| 新测试 | `tests/unit/testRenderPipeline.cpp` |
| 接入 | `src/core/GN.h` 增加 include（umbrella 头） |
| 不做 | 不碰 Vulkan、不改任何现有绘制路径 |

**验收**：
- `make unit` 新增用例全过；总数 ≥ 旧基线。
- 用例覆盖：层掩码**三层回退**（item=0/material=0/程序有入口仍能画）、item 覆盖、材质覆盖、
  不支持层、`ePROG_IMPLICIT_SHADOW` 退让、空 material/空 program 被 `ValidateRenderItem` 拒绝、
  排序稳定性（同一 viewZ 保持提交序）、程序描述合法性（层入口/顶点格式/状态不变式）、
  布局版本不匹配拒绝、空/非法输入不崩。
- `bash ~/.dsh/skills/code-modularity/scripts/lintCodeModularity.sh src/graphics tests/unit --naming --comments` 新文件 0 违规。

**失败怎么办**：纯 CPU，失败即改测试/契约；不影响渲染，无需回滚。

### P1 管线缓存 v2（特殊化常量 / 键版本 / 回收；仍不切换绘制路径）

> **P1a 已完成（2026-11-21）**：`aSpecCount/aSpec[8]/aKeyVersion` 进描述、校验、键与相等性；
> `vkPipeline.cpp` 图形/计算创建注入 `VkSpecializationInfo`；`make unit` = 405/0；lint 0。
> **P1b API 已完成（2026-11-21）**：`cSHADER_CACHE`/`cPIPELINE_CACHE` 的 `SetFrame/CollectGarbage`
> 已实现，无设备 no-op 单测通过；**真实 GPU 销毁时机未验证**（需帧边界 + fence）。

**目标**：`sPIPELINE_DESC` 加特殊化常量/键版本/按帧回收；`cSHADER_CACHE` 加同批回收。
**不做**：本轮不创建「程序入口 → 管线」的注册表（放 P2，避免先按旧布局建一遍再全拆）。

| 项 | 内容 |
|---|---|
| 改 | `src/graphics/GN_Pipeline.h/.cpp`、`src/graphics/_Vulkan/vkPipeline.cpp`、`src/graphics/GN_Shader.h/.cpp`、`src/graphics/_Vulkan/vkShader.cpp` |
| 测试 | `testPipeline.cpp` 加特殊化常量键/相等性/键版本失效；`gpuCheck` 加「管线数不因复用增长」 |

**验收**：
- 同一描述不同 `aSpec` 值 → 不同键、不同管线；同值 → 命中不新建。
- `aKeyVersion` 变化 → 旧缓存不命中。
- 现有 11+ 管线仍由缓存创建；gpuCheck 旧检查全绿。
- `CollectGarbage` 在帧边界回收未用条目；`Release` 前不销毁在飞管线（单元测试用假 device 只能测键/计数，销毁路径靠人工审查 + GPU 冒烟）。
- 新文件 <500 有效行；lint 0。

**失败怎么办**：管线键/特殊化常量是纯 CPU 可复算；失败只改描述与测试，不碰绘制。

### P2 统一绑定表迁移 + 程序注册（唯一高风险阶段）

**目标**：把 PBR / 前向 / G-Buffer / 阴影的 set 0 收敛到 §3.4 的 canonical 表；材质纹理搬到 set 1；
同时建立「程序入口 → 管线」注册表（先只注册 `gn.pbr` / `gn.depth`）。

| 项 | 内容 |
|---|---|
| 改（Vulkan） | `GN_Vk.h`（材质 UBO/布局版本/新访问器）、`vk3dInit.cpp`（PBR/前向/阴影 layout）、`vk3dInitScene.cpp`（PBR/前向 set 的绑定写入）、`vk3dPasses.cpp`（G-Buffer draw/延迟绑定）、`vk3dForward.cpp`（PBR/前向 draw 的 set 绑定）、`vkGbuffer.cpp`（G-Buffer 描述符/管线/布局）、`GN_Vk.cpp`（`GN_VkInit3D` 编排 + 内嵌回退）、`vk3dShutdown.cpp`（销毁新布局） |
| 改（GLSL） | `gn3d_pbr.frag`（材质参数改读 set1 UBO、albedo/normal → set1；雾/GI 编号保持 6/7）、`gn3d_gbuffer.frag`（材质参数/纹理 → set1）、`gn3d.frag`（雾 3→6、纹理 → set1；**它没有 GI binding，不用改 GI**）、`gn3d_shadow.vert`（lightVP 0→12） |
| 新 | `src/graphics/_Vulkan/vkBindingLayout.cpp`（set0/set1/set2 的 `VkDescriptorSetLayout` 创建与绑定写入）、`src/graphics/_Vulkan/vkShaderProgram.cpp`（注册表 + 建管线） |
| 构建 | 若选 ADR-6 方案 A：`Makefile` 加 `*_spv.h` 生成步骤；方案 B：删除 `GN_Vk.cpp` 的回退分支与头 include |
| 删除/替换 | `vk3dForward.cpp` 的 per-texture PBR set 缓存（由材质 set 缓存替代） |
| 回退开关 | **只在本阶段存在**：编译宏 `GN_BINDING_LAYOUT_LEGACY` 选择旧布局/旧 shader 文件（旧 shader 以 `*_legacy` 后缀保留，`make shaders` 一并编译），验收通过后 P6 删除 |
| 测试 | 现有 `gpuCheck` 3D/material/fog/gi 像素断言全部保留；新增 `binding.pbr` / `binding.forward` / `binding.gbuffer` / `spv.header.sync` / `descriptor.pool` |

**验收**：
- `make unit` 全过（CPU 部分不应受影响）。
- `make gpuCheck` 旧 99 条全 PASS（真实 GPU；SKIP 不算）。
- 新增像素断言：PBR 四边形颜色与旧值 ±0.012；前向雾像素与旧值一致；G-Buffer albedo 读取一致。
- `spv.header.sync`：内嵌数组哈希 == 对应 `.spv` 哈希（方案 A）；或确认回退已删除（方案 B）。
- 程序注册：`gn.pbr` / `gn.depth` 的入口全部建出管线；注册失败不产生半注册。
- `descriptor.pool`：压力场景（512 材质 + 4 级阴影）池耗尽计数 = 0。
- 真机桌面目视：mmdViewer 默认画面、imoScene 角色无黑块/无贴图错位。
- lint 0；新文件 <500 有效行。

**失败怎么办**：
- 用 `GN_BINDING_LAYOUT_LEGACY` 回到旧布局/旧 shader，先保证旧画面；A/B 只允许存在于本阶段。
- 单条像素断言失败 → 只修对应绑定/着色器，不重跑整套视觉；
- 若真机出现驱动相关差异，记录 GPU/驱动版本与像素实际值，回退该绑定改动，不把「软件渲染通过」当验收。

### P3 层队列 + 前向多着色器（最小闭环）

**目标**：两个物体、同一前向 pass、不同程序，像素各自正确；层内排序生效。

| 项 | 内容 |
|---|---|
| 新 | `src/graphics/_Vulkan/vkRenderQueue.cpp`（入队/排序/执行/层校验） |
| 改 | `GN_Vk.h`（`sRENDER_ITEM`/`sMATERIAL_INSTANCE`/提交与 flush API）、`vk3dForward.cpp`（从队列执行而非每次固定 PBR） |
| 新 shader | `gn3d_unlit.vert/frag`（或复用 `gn3d_pbr.vert` 的接口）、`gn3d_toon.frag` |
| 新测试 | `tests/gpu/gpuCheckLayers.cpp`：`layer.multiShader.pixels`、`layer.order`、`layer.mask`；`tests/gpu/gpuCheckMain.cpp` 加 include + 调用 |

**验收**：
- 两个四边形：一个 `gn.pbr`（绿）、一个 `gn.unlit`（红），同一帧同一层；
  中心像素分别 ≈ 绿/红（±0.03），互不串色。
- 排序：同层两个不同程序，绘制顺序改变但结果一致（不透明）；透明层后→前排序结果正确。
- 层外/错误 pass 调用 flush 返回 false 且只警告一次。
- 旧 `GN_VkDraw3DPBR` 调用路径仍走默认程序，画面不变；旧 gpuCheck 全绿。
- 真机目视：imoScene 增加一个 unlit 物体，与角色同帧可见。

**失败怎么办**：队列/解析是 CPU 逻辑优先单测；像素失败先查绑定与程序选择，再查队列顺序。

### P4 逐物体多 pass（阴影 / G-Buffer / 描边 / 透明）

**目标**：同一物体按层掩码进入多趟，逐层可用不同程序；默认 PBR 行为不变。

| 项 | 内容 |
|---|---|
| 改 | `GN_3Dmodel.*`（`DrawLayered`、网格级层掩码/程序覆盖）、`GN_3Dmaterial.*`（追加程序/层字段） |
| 改（Vulkan） | `vk3dPasses.cpp`（阴影/G-Buffer/前向层执行）、`vk3dForward.cpp`（透明层排序） |
| 新 shader | `gn3d_outline.vert/frag`（反向外扩壳） |
| 新测试 | `gpuCheckLayers.cpp`：`layer.shadow.pixels`、`layer.shadow.cascades`（4 级各有深度）、`layer.multiPass.order`、`layer.outline.pixels`、`layer.blend.order`、`layer.pbrSorted.regression` |
| 不在本阶段 | `gn.skinned.pbr` 与 `vkSkin*` 的 canonical 迁移 → P6；P4 只保证静态网格多 pass |

**验收**：
- 同一四边形同时进阴影层 + 前向层：**4 个 cascade 的阴影图各自有深度**（`layer.shadow.cascades`）；颜色 pass 像素正确。
- 卡通物体：前向层用 `gn.toon`、描边层用 `gn.outline`，画面可见描边；阴影层由 `ePROG_IMPLICIT_SHADOW` 走内置 depth。
- 透明层：两个半透明四边形，后→前排序后混合结果与 CPU 期望一致（±0.03）。
- 旧 `DrawPBRSorted` 行为逐像素不变：`layer.pbrSorted.regression` 用两个 BLEND 网格做像素基线（不是只看截图）。
- 真机目视：角色描边/透明衣物无新缺陷。

**失败怎么办**：分层是显式 API，可逐层开关定位；某层失败先禁用该层掩码，不阻塞其它层。

### P5 外部 SPIR-V + 热重载

**目标**：从磁盘加载 `.spv` 程序，校验、绘制、回退、热重载。
**前置**：P3 完成（需要 `gn3d_unlit.*.spv` 作为外部程序样本；P3 之前它不存在）。

| 项 | 内容 |
|---|---|
| 改 | `vkShaderProgram.cpp`（加载/校验/临时对象/原子注册/重载）、`GN_RenderPipeline.h`（加载描述/错误串） |
| 新测试 | `gpuCheckShader.cpp`：`shader.external.load`、`shader.external.invalid`、`shader.external.fallback`、`shader.reload`；`gpuCheckMain.cpp` 加 include + 调用 |
| 示例 | `tests/imoScene` 或独立小程序加载一个自定义 `.spv`（例如把 P3 产出的 `gn3d_unlit` 换成品红） |

**验收**：
- 用 **P3 产出的** `assets/shaders/gn3d_unlit.*.spv` 作为外部程序加载，注册成功、可绘制，像素正确。
- 格式/描述非法输入（不存在/0 字节/非 SPIR-V/超 8 MiB/层掩码不符/布局版本不符）逐条被拒，
  返回 false + 原因，不崩、不注册半成品。
- **无法静态拒绝的语义问题**（表外 binding、SPIR-V 输出接口不匹配、超长 push constant 被 SPIR-V 绕过）：
  用校验层 + 故意错配的 gpuCheck 用例验证「管线创建失败 → 程序拒绝/回退」，不把它写成 R5 的格式校验项。
- 管线创建失败（例如故意错配 render pass）→ 程序拒绝，引用项回退 `gn.pbr`，只警告一次。
- 热重载：改一个 `.spv`（重新编译）后调用 reload，下一帧生效；旧管线在 2 帧后回收，无 UAF/崩溃。
- 文档写明信任边界（§3.7.3）。

**失败怎么办**：加载器支持「只注册不绘制」诊断模式；像素失败先确认程序选择，再确认绑定表。

### P6 延迟/前向混排 + 蒙皮 canonical + 收尾

**目标**：标准 PBR 走 G-Buffer、自定义程序走前向，同帧混排；清理双轨；性能与文档收尾。

| 项 | 内容 |
|---|---|
| 改 | `vk3dPasses.cpp`（GBuffer 程序入口）、`vkRenderQueue.cpp`（路径选择）、`GN_Vk.h`（路径枚举）、`vkSkinDraw.cpp`/`vkSkinSsbo.cpp`（蒙皮改 canonical set0 + set2）、`gn3d_skinned*.vert/frag`（绑定迁移） |
| 清理 | 删除 P2 的 A/B 开关与旧布局常量；`GN_VkMaterialCreate` 旧材质路径先保留公开 API，若迁移则走独立阶段，不在 P6 顺手删 |
| 测试 | `gpuCheck`：`path.deferred.mixed`（PBR 在 GBuffer、unlit 在前向，同帧都可见）；`skin.canonical`（蒙皮 200 关节可见） |
| 性能 | 记录基线：1000 个 RenderItem × 4 程序的 CPU 帧时间、管线数、状态切换数；**只记录实测值，不预设百分比目标** |

**验收**：
- 混排场景像素正确：GBuffer 物体经延迟光照可见，前向物体覆盖在其上；两者深度关系正确。
- 蒙皮对象走 canonical 布局 + set2 后画面与 legacy 一致；旧 `GN_VkDraw3DSkinned*` 仍可用。
- 旧 API 仍全绿；文档/CHANGELOG/记忆更新。
- 真实 GPU 全量 `make gpuCheck` PASS；用户桌面目视通过。

**失败怎么办**：混排是**扩展**能力，失败可回退到「单一前向路径」；此时 R3 只按「同一路径内多 pass」验收，
不得声称跨延迟/前向混排已完成。

---

## 6. 风险、取舍与未决问题

### 6.1 风险表

| 编号 | 风险 | 触发条件 | 缓解 | 残留影响 |
|---|---|---|---|---|
| R1 | 绑定表迁移打错像素/贴图 | P2 改 3 套 shader + 描述符写入 | 旧 gpuCheck 全保留；新增像素对照；A/B 开关仅本阶段 | 若真机驱动差异，需按 GPU/驱动记录并回退该绑定 |
| R2 | 管线数量爆炸 | 程序 × 层 × 状态 × 特殊化常量组合 | 缓存键去重；上报管线数；>256 警告；特殊化常量只用于真正的常量分支 | 首次加载卡顿；需离线预热（P6 可选） |
| R3 | 外部 SPIR-V 崩溃/挂起 | 畸形但合法的 SPIR-V | 大小上限、白名单、校验层、离线 spirv-val、失败回退 | 无法完全沙箱化；信任边界必须写清 |
| R4 | 热重载 UAF | 销毁仍被在飞命令引用的管线/描述符 | 帧边界 + `vkWaitForFences` + 2 帧延迟回收 | 极端情况下仍需 `vkDeviceWaitIdle` 兜底 |
| R5 | 透明排序/深度写错 | 多层透明物体 | 层状态表固定（透明层 write off）；网格级后→前；逐三角形排序不在 v1 | MMD 头发/裙边类穿插仍可能有排序瑕疵（既有已知限制） |
| R6 | push constant 128 B 不够 | 新程序需要 >16 个每 draw float | 强制走 set1 材质 UBO 的自定义参数；push constant 不扩 | 参数特别多的程序需要一次 UBO 写入 |
| R7 | 描述符池耗尽 | 材质实例/蒙皮环槽多 | 池尺寸按 §3.4.6 容量账估算；材质 set 随实例归还；耗尽计数与日志 | 极端场景需限制同时存活材质数 |
| R8 | 阴影层退让语义被滥用 | 程序置 `ePROG_IMPLICIT_SHADOW` 但几何不适合 | 默认不置位；校验器只允许「无 SHADOW 入口」时置位；文档写清前提 | 可能出现阴影与本体不一致 |
| R9 | 新文件超行数/命名违规 | 实现期赶工 | 每个文件单一职责；lint 门禁 | 需拆分文件，工期增加 |

### 6.2 未决问题（实现前需用户拍板）

| 编号 | 问题 | 选项 | 推荐 |
|---|---|---|---|
| Q1 | 默认渲染路径 | (a) 前向（现状） (b) 延迟 (c) 按场景切换 | **(a) 前向**：现有查看器与 gpuCheck 都用前向；延迟混排放 P6 |
| Q2 | 外部程序描述格式 | (a) 仅 C++ 结构体（v1） (b) 加 key=value 文本 sidecar | **(a) v1**；文本 sidecar 等有第二个使用者再加 |
| Q3 | 描边是否用模板 | (a) v1 反向外扩壳，无模板 (b) 改 D24S8 深度格式 | **(a)**：改深度格式会动所有 target/管线，代价远大于收益 |
| Q4 | 效果 pass（粒子/云/贴花/布料）是否进层系统 | (a) v1 只在层时间线上占位，继续专用管线 (b) 全部通用化 | **(a)**：它们不是「场景物体」，通用化收益低 |
| Q5 | 材质 UBO 自定义参数布局 | (a) 固定 12×vec4，按程序解释 (b) 每程序自定义偏移表 | **(a) v1**：无反射，自定义偏移表等于自造反射；冻结结构见 §7（256 B） |
| Q6 | 是否支持实例化 | (a) v1 不支持 (b) v1 支持 | **(a)**：先把多程序/多层闭环；实例化另立规划 |

### 6.3 独立对抗审查结论（2026-11-20）

审查者 `pipeline-reviewer`：只读、未参与写作、按六攻击面（假假设/隐藏耦合/循环依赖/边界失败/资源权限/交付可验证性）
审查本文件。以下是**会改变交付结果**的发现与处置；未列入的条目为「未击穿」或已在正文声明。

| 编号 | 发现 | 处置（本文件位置） |
|---|---|---|
| B1 | CSM 4 级阴影的 flush 消费语义未定义，可能只画一级或跨帧累积 | §3.5.3：flush **不消费**、帧首 `ResetRenderQueue`、CSM 每级各 flush；P4 加 `layer.shadow.cascades` |
| B2 | `hasImplicitShadow` 在冻结契约里不存在 | §3.3/§7：`sSHADER_PROGRAM_DESC::aFlags` + `ePROG_IMPLICIT_SHADOW`；校验器只允许无 SHADOW 入口时置位 |
| B3 | `sSHADER_PROGRAM` 未定义，P0 纯函数无法实现 | §7：补 `sSHADER_PROGRAM`（desc/id/runtime）；P0 只依赖 desc |
| B4 | P2 漏 `vkGbuffer.cpp`/`GN_Vk.cpp`/`vk3dShutdown.cpp`/`vk3dInitScene.cpp` | §5 P2 文件清单补齐 |
| H1 | 内嵌 SPIR-V 回退已存在且 `make shaders` 不生成头文件；P2 后会旧绑定配新布局 | ADR-6 + §3.7.1：P2 二选一（同步生成/删除回退），加 `spv.header.sync` |
| H2 | 层掩码解析漏「程序默认」，默认材质会被 SKIP | §3.5.1：`item → material → program` 三层回退；P0 加用例 |
| H3 | 阴影每级 lightVP 在 canonical set0 无位置 | §3.4.2 binding12 + §3.4.4 按层三种 pipeline layout；P2 改 `gn3d_shadow.vert` |
| H4 | `sRENDER_ITEM` 裸指针生命周期未定义，flush 时可能悬空 | §3.5.3/§7：模型矩阵按值、顶点/索引/材质借用至 reset、满队返回 false |
| H5 | 描述符池容量与 set1/set2 规模没有可验收的账 | §3.4.6 容量表；set1 按材质实例、set2 环形 4 槽；P2 加 `descriptor.pool` |
| H6 | P2 写 `gn3d.frag` 的 GI 4→7，但该 shader 没有 GI binding | §3.4.2/§5 P2：只改雾 3→6；前向 layout 的 GI 槽单独说明 |
| H7 | `aTextures[8]` 与 set1 的 7 个纹理槽不一致 | §7：改为 `aTextures[7]` |
| H8 | Q5 推荐 4×vec4 与冻结的 12×vec4 冲突 | §6.2 Q5：统一为 12×vec4（256 B） |
| H9 | P5 写「现有 gn3d_unlit」不存在 | §5 P5：改为「P3 产出」并写前置 P3 |
| H10 | P4 声称新蒙皮程序但没有任务项 | §3.7.1/§5：`gn.skinned.pbr` 移到 P6，P4 只做静态多 pass |
| M1 | 基线 `9d27dbd` 与实际 `a48523d`/脏树不符，5 处行号偏移 | §0 更新基线；§10 改为按工作树复算；修正 `GN_3Dmodel.h:44`、`3D_FEATURE_PLAN.md:373/391`、`Makefile:472-482` 等 |
| M2 | `DrawPBRSorted` 逐像素回归没有可执行用例 | §5 P4：加 `layer.pbrSorted.regression` |
| M3 | P2 的 A/B 开关没有落地方式 | §5 P2：`GN_BINDING_LAYOUT_LEGACY` + `*_legacy` shader 文件 |
| M4 | `sMATERIAL_UBO` 的 alphaCutoff 位置矛盾 | §3.4.3/§7：统一到 albedo.w |
| M5 | 队列没有帧开始/清空 API | §3.5.3：`GN_VkResetRenderQueue`，帧首清空 |
| M6 | P1 程序注册数量/验收目标不清 | §5 P1：P1 只做缓存 v2；注册表移到 P2，目标 `gn.pbr`/`gn.depth` |
| M7 | P2 只写 PBR，漏前向 Blinn-Phong 的 set1 | §5 P2：明确 `GN_VkDraw3D` 也要绑 set1 |
| M8 | R3「任意组合」与 P6 可选混排矛盾 | §1.1 R3 收窄为「同一路径内」；跨路径混排是 P6 扩展 |
| M9 | R5「原子拒绝」与 ADR-1「不能检查表外 binding」矛盾 | §1.1 R5/§5 P5：拆成「格式/描述可拒」与「语义越界交给管线创建/校验层」 |
| M10 | `ValidateRenderItem` 没要求 material 非空 | §3.5.1/§7：明确非空；P0 用例覆盖 |
| L1–L4 | 排序用相机距离、满队丢项、`aKey` 生命周期、§2.5「三套」不准确 | 分别改为 view 深度、返回 false + 计数、删除 `aKey`、§2.5 标题与五套布局表 |

**审查未覆盖/未独立验证**：gpuCheck 99 条未重跑；真实 GPU 行为、驱动差异、表外 binding 的实际后果未验证；
这些在 §8 的验收矩阵里要求实现阶段补证据，不能拿本文件当运行时证据。

---

## 7. 冻结契约草案（P0 落地时以此为准）

```cpp
// GN_RenderPipeline.h —— 纯 CPU，不 include Vulkan
namespace GNg { struct sIMAGE; } // 前向声明；sIMAGE 的 Vulkan 句柄只在 .cpp/_Vulkan 侧使用
namespace GN3d {

const uint32_t kBindingLayoutVersion = 1;
const uint32_t kMaxRenderItems = 4096;       // 每帧每层上限；满队 Submit 返回 false 并计数（不静默丢弃）
const uint32_t kMaxShaderPrograms = 64;
const uint32_t kMaxMaterialInstances = 1024; // set1 分配上限（见 §3.4.6）
const uint32_t kMaxPushConstantBytes = 128;

enum eRENDER_LAYER { /* §3.2 的 7 层，顺序固定 */ };
enum eVERTEX_FORMAT { eVF_STATIC = 0, eVF_SKINNED, eVF_CUSTOM };
enum eSKIN_DATA { eSKIN_NONE = 0, eSKIN_UBO, eSKIN_SSBO };
enum ePROGRAM_FLAGS { ePROG_IMPLICIT_SHADOW = 1u << 0 };

struct sSPEC_CONST {
    uint32_t aId;
    uint32_t aValue;      // v1 只支持 32 位标量；float 用位模式
};

struct sSHADER_ENTRY {
    const void *aVertSpv; uint32_t aVertSize;
    const void *aFragSpv; uint32_t aFragSize;
    uint32_t aPushConstantSize; // ≤128
    int32_t aBlend, aDepthTest, aDepthWrite, aCullBack, aTopology; // -1 = 层默认
    uint32_t aSpecCount; sSPEC_CONST aSpec[8];
};

struct sSHADER_PROGRAM_DESC {
    const char *aName;
    uint32_t aLayerMask;
    uint32_t aFlags;          // ePROGRAM_FLAGS（ePROG_IMPLICIT_SHADOW）
    eVERTEX_FORMAT aVertexFormat;
    eSKIN_DATA aSkinData;
    uint32_t aLayoutVersion;
    sSHADER_ENTRY aEntries[eLAYER_COUNT];
};

// 注册后的程序对象（由注册表拥有；CPU 层可读 aDesc，aRuntime 不透明）
// aDesc.aName 由注册表深拷贝到自有缓冲；调用方传入的字符串不必长期存活。
struct sSHADER_PROGRAM {
    sSHADER_PROGRAM_DESC aDesc;
    uint32_t aId;       // 注册序号（层内排序键）
    void *aRuntime;     // Vulkan 侧私有：入口→VkPipeline、SPIR-V 所有权等
};

// set1 binding0 的 CPU 侧镜像（256 B，std140；与 §3.4.3 一致）
struct sMATERIAL_UBO {
    float aAlbedo[4];    // rgb + alphaCutoff
    float aPbr[4];       // roughness, metallic, ao, reserved
    float aEmissive[4];  // rgb + reserved
    float aReserved[4];
    float aCustom[12][4];// 程序自定义参数（48 float，按程序解释）
};
static_assert(sizeof(sMATERIAL_UBO) == 256, "sMATERIAL_UBO 必须为 256 字节");

struct sMATERIAL_INSTANCE {
    const sSHADER_PROGRAM *aProgram; // 非空
    uint32_t aLayerMask;             // 0 = 用程序默认
    sMATERIAL_UBO aUbo;
    const GNg::sIMAGE *aTextures[7]; // 对应 set1 binding 1..7（不是 8）；空 = 默认白纹理
};

struct sRENDER_ITEM {
    const void *aVertices; uint32_t aVertexCount;   // 借用，活到 ResetRenderQueue
    const uint32_t *aIndices; uint32_t aIndexCount; // 借用，活到 ResetRenderQueue
    GNg::sMAT44 aModelMatrix;                        // 按值复制
    const sMATERIAL_INSTANCE *aMaterial;             // 非空
    uint32_t aLayerMask;                             // 0 = 材质/程序默认
    const sSHADER_PROGRAM *aLayerProgram[eLAYER_COUNT]; // 可空
    uint32_t aSortKey;
};

// 纯函数：不碰 Vulkan，可单测
// 解析顺序：item.layerMask → material->aLayerMask → material->aProgram->aLayerMask
uint32_t ResolveLayerMask(const sRENDER_ITEM &item);
const sSHADER_ENTRY *ResolveEntry(const sRENDER_ITEM &item, eRENDER_LAYER layer);
bool ValidateShaderProgramDesc(const sSHADER_PROGRAM_DESC &desc, char *reason, size_t reasonSize);
// 拒绝：空顶点/索引指针但计数>0、空 material、空 program、层掩码非法、模型矩阵非有限
bool ValidateRenderItem(const sRENDER_ITEM &item, char *reason, size_t reasonSize);
// view 用于算视空间深度（不能只用相机位置/距离）
void SortRenderItems(sRENDER_ITEM *items, uint32_t count, eRENDER_LAYER layer, const GNg::sMAT44 &view);

} // namespace GN3d
```

**层状态不变式（校验器必须实现）**：

| 层 | 允许的覆盖 | 禁止 |
|---|---|---|
| SHADOW | depthTest/Write、cull、bias | 颜色附件、混合、FS（除非显式 alpha-test 变体） |
| GBUFFER | cull | 关深度写、混合、非 3 个颜色附件 |
| FORWARD_OPAQUE | cull、topology | 混合、关深度写 |
| OUTLINE | cull、blend、depthTest | 写深度（v1）、非 alpha 混合 |
| FORWARD_ALPHA | cull、blend | 写深度 |
| EFFECT | 效果自定义 | 由现有专用管线负责，v1 不接受外部程序 |
| OVERLAY | 2D 规则 | 3D 程序 |

**兼容性规则**：`sSHADER_PROGRAM_DESC::aLayoutVersion != kBindingLayoutVersion` 一律拒绝；
外部程序不得绕过该检查；内置程序在编译期用同一常量。

---

## 8. 验收矩阵（每期都要能复算）

| 检查 | 命令/观察 | 通过标准 | 适用阶段 |
|---|---|---|---|
| 单元测试 | `make unit` | 新增用例 0 failed；旧用例不回归 | P0–P6 |
| 真实 GPU | `make gpuCheck`（桌面，有 `/dev/dri`） | 全 PASS；SKIP 不算 | P1–P6 |
| 像素对照 | gpuCheck 内 `layer.*` / `binding.*` / `shader.*` | 断言值 ±0.012～0.03（按颜色空间） | P2–P6 |
| 旧画面回归 | 运行 `build/mmdViewer`、`build/imoScene` | 与改动前截图对比无可解释差异；用户目视 | P2、P3、P4、P6 |
| 代码规范 | `bash ~/.dsh/skills/code-modularity/scripts/lintCodeModularity.sh src/graphics tests/unit tests/gpu --naming --comments` | 新文件 0 违规；既有告警不新增 | P0–P6 |
| SPIR-V 头同步 | gpuCheck `spv.header.sync` | 内嵌数组哈希 == `.spv` 哈希（方案 A）或回退已删除（方案 B） | P2 |
| 描述符池 | gpuCheck `descriptor.pool`（512 材质 + 4 级阴影压力） | 耗尽计数 = 0；分配失败有日志/返回值 | P2 |
| 层掩码 | gpuCheck `layer.mask` + unit `ResolveLayerMask` | item→material→program 三层回退正确 | P0/P3 |
| 多着色器 | gpuCheck `layer.multiShader.pixels` | 同帧同层两个程序颜色各自正确、不串色 | P3 |
| 4 级阴影 | gpuCheck `layer.shadow.cascades` | 4 个 cascade 阴影图各有深度 | P4 |
| 多 pass | gpuCheck `layer.multiPass.order` | 同物体多层各趟像素正确 | P4 |
| 旧排序回归 | gpuCheck `layer.pbrSorted.regression` | 与基线像素一致（±0.03） | P4 |
| 混排 | gpuCheck `path.deferred.mixed` | GBuffer 物体 + 前向物体同帧可见、深度正确 | P6 |
| 蒙皮 canonical | gpuCheck `skin.canonical` | 200 关节蒙皮可见，结果与 legacy 一致 | P6 |
| 外部加载 | 手工用非法 `.spv`/不存在路径/布局版本不符 | 返回 false + 原因；不注册；不崩 | P5 |
| 热重载 | 重新编译一个 `.spv` 后 reload，连续 100 次 | 无 UAF/崩溃；管线数有界 | P5 |
| 性能基线 | 1000 RenderItem × 4 程序 | **只记录** CPU 帧时间/管线数/状态切换；不预设通过阈值 | P6 |

---

## 9. 参考资料与适用边界

| 资料 | 用途 | 关键结论 | 适用边界 |
|---|---|---|---|
| [Vulkan Specification · Pipelines](https://docs.vulkan.org/spec/latest/chapters/pipelines.html) | 管线对象、特殊化常量、多管线创建 | 管线把着色器与固定功能状态绑定为一个对象；特殊化常量在**管线创建时**指定，不同值 = 不同管线 | 规范本身；不讨论引擎架构 |
| [VkPipelineCache(3)](https://github.khronos.org/Vulkan-Site/refpages/latest/refpages/source/VkPipelineCache.html) | 驱动级管线缓存 | 缓存内容由实现管理，可跨管线/跨运行复用；应用只负责提供缓存对象 | 缓存数据与设备/驱动相关，不可假设跨设备可移植 |
| [VK_EXT_shader_object 提案](https://docs.vulkan.org/features/latest/features/proposals/VK_EXT_shader_object.html) | 评估「无管线」路线 | 着色器对象 + 大量动态状态；跨阶段接口与描述符兼容由应用保证；扩展可选 | 本机未验证支持；不进入 v1 |
| [SPIR-V 规范](https://registry.khronos.org/SPIR-V/specs/unified1/SPIRV.html) | 外部程序格式 | SPIR-V 是二进制中间表示；格式正确不等于语义安全 | 引擎只做魔数/对齐/大小校验；完整校验交给离线工具与校验层 |
| [glslang](https://github.com/KhronosGroup/glslang) | 构建期 GLSL→SPIR-V | 现有 `make shaders` 使用 glslangValidator | 仅构建期，不链进引擎 |
| [SPIRV-Tools](https://github.com/KhronosGroup/SPIRV-Tools) | 离线校验 | `spirv-val` 可检查模块合法性 | 不保证 GPU 驱动不崩；不作为运行时依赖 |
| [Vulkan Validation Layers](https://github.com/KhronosGroup/Vulkan-ValidationLayers) | 调试期捕获布局/状态错误 | 可在开发期发现描述符/管线不匹配 | 不进入发布路径；不替代测试 |

---

## 10. 复算命令（核本文的现状断言）

```bash
cd /home/ruicat/下载/GnVk/GnVk

# 基线（会随其它会话变动；行号以当前工作树为准）
git rev-parse HEAD
git status --short | wc -l

# 现状：五套 set0 布局（前向/PBR/G-Buffer + legacy 材质/蒙皮）
sed -n 360,410p  src/graphics/_Vulkan/vk3dInit.cpp     # 前向 5 槽
sed -n 468,545p  src/graphics/_Vulkan/vk3dInit.cpp     # PBR 12 槽
sed -n 227,260p  src/graphics/_Vulkan/vkGbuffer.cpp    # G-Buffer 3 槽（槽 2 空）
sed -n 35,50p    src/graphics/_Vulkan/vkMaterial.cpp   # legacy 材质 2 槽
sed -n 85,105p   src/graphics/_Vulkan/vkSkinDraw.cpp   # legacy 蒙皮 set0
sed -n 81,100p   src/graphics/_Vulkan/vkSkinSsbo.cpp   # legacy 蒙皮 SSBO

# 现状：绘制函数写死管线 + 每纹理描述符集
sed -n 319,420p  src/graphics/_Vulkan/vk3dForward.cpp  # 376 选管线、378 bind

# 现状：管线缓存只增不删
sed -n 244,310p  src/graphics/_Vulkan/vkPipeline.cpp

# 现状：SPIR-V 校验只有魔数/对齐
sed -n 30,45p    src/graphics/GN_Shader.h
sed -n 49,54p    src/graphics/GN_Shader.cpp
sed -n 35,42p    src/graphics/_Vulkan/vkShader.cpp

# 现状：内嵌 SPIR-V 回退（P2 必须同步或删除）
sed -n 1044,1048p src/graphics/_Vulkan/GN_Vk.cpp
sed -n 1068,1096p src/graphics/_Vulkan/GN_Vk.cpp

# 现状：描述符池容量
sed -n 432,443p  src/graphics/_Vulkan/GN_Vk.cpp

# 现状：4 级 CSM 由查看器循环调用
sed -n 468,478p  tests/mmdViewer/mmdViewer.cpp

# 现状：pass 编排由调用方手写
sed -n 318,500p  src/graphics/_Vulkan/vk3dPasses.cpp

# 现状：阴影 shader 仍读 binding 0
sed -n 1,20p     assets/shaders/gn3d_shadow.vert

# 管线/材质 CPU 契约
sed -n 55,145p   src/graphics/GN_Pipeline.h
sed -n 25,75p    src/graphics/GN_Material.h
```

---

## 11. 下一步（需要用户确认后才动手）

> 本文件已过一轮**独立只读对抗审查**（`pipeline-reviewer`，六攻击面）；阻断/高优先级发现已并入正文（§6.3）。
> 审查没有重跑 GPU 检查，也没有验证真实驱动行为——这些是实现阶段的验收项，不是本文件的结论。
>
> **相关独立任务**：DDGI 异步探针更新 / probe relocation / 法线偏置已登记在
> [`docs/PROBE_BAKE_PLAN.md`](PROBE_BAKE_PLAN.md) §10（Stage 2d–2f），与本文的多层材质管线无直接依赖。
> 全仓未完成项与批次进度统一记在 [`docs/BACKLOG.md`](BACKLOG.md)；P0 已在该台账标 done。

1. 确认 §6.2 的 Q1–Q6（推荐项已给；Q5 已统一为 12×vec4）。
2. 确认 P0 的契约草案（§7）是否按当前形态冻结（含 `sSHADER_PROGRAM`、`aFlags`、128 B push constant、set0 13 槽）。
3. 确认 P2 的迁移策略：
   - 是否接受「一次性迁移 PBR / 前向 / G-Buffer / 阴影四套 shader + `GN_BINDING_LAYOUT_LEGACY` A/B 开关只在 P2 存在」；
   - ADR-6 选方案 A（生成并同步 `*_spv.h`）还是方案 B（删除内嵌回退）。
4. 确认 `gn.skinned.pbr` 放 P6（P4 只做静态网格多 pass），以及是否接受跨延迟/前向混排作为 P6 扩展而非 P0–P5 的阻塞项。
5. 确认后再开 P0（纯 CPU，不改现有渲染）；P0 以 `make unit` + lint 验收后，才进入 P1；P1 只做缓存 v2，P2 才动画面。
