# 全局光照（GI）实现分析

> 性质：**源码实现分析 + 落地方案**，本文不改任何源码。
> 基线：当前工作区（`src/graphics/_Vulkan` + `assets/shaders`），C++11、Vulkan、`make` 单一构建入口；
> 天空/天气按 [SKY_WEATHER_PLAN.md](SKY_WEATHER_PLAN.md) 的 S1/S2a 已落地。
> 结论先行：现状**不是「没有 GI」**，而是「有一个基于静态 cubemap 的伪 IBL + 一个未接线的 SSAO 着色器」，且两者都不正确：
> IBL 没做 split-sum（漫反射直接采法线方向的天空、镜面不随粗糙度取 mip、没有 BRDF LUT、cubemap 只有 1 级 mip），
> SSAO 着色器存在但没有任何 C++ 调用点，且它把世界空间法线当成视空间法线在用。
> 更前置的问题是 **G-Buffer 只够「延迟光照」用，不够「GI」用**：没有 AO 目标、没有显式 subpass 依赖、resize 不重建、
> 附件命名与内容错位、格式浪费 2/3 带宽（详见 §5）。
> 推荐顺序：**G0 G-Buffer 补齐 → v1 正确的 IBL + SSAO（仅延迟路径）→ v2 SSGI（半分辨率、单帧）→ v3 探针（SH9 / DDGI）→ v4 光追（远期不做）**。
> G0/v1 不新增 pass 依赖，可在本机编译 + CPU 单测；GPU 视觉仍需你在桌面验证。

## 0. 范围

本规划把「全局光照」定义为**间接光照**：

- 环境/天空提供的**漫反射 irradiance**；
- 环境提供的**镜面反射**（随粗糙度模糊）；
- 间接光的**可见性**（AO / 遮挡）；
- 以及（v2/v3）来自场景几何的**一次弹射**。

直接光照（8 光源 + 单张方向光阴影）不在本规划内，见 `GN_3Dlight.h`。烘焙 lightmap 也不做：本引擎的场景模型是运行时加载的（MMD / glTF），且天气是动态的。

## 1. 现状（源码证据）

| 能力 | 现状 | 证据 |
|---|---|---|
| 可见天空 | 解析式渐变 + FBM 云（S2a），随天气/时间变化 | `assets/shaders/gn3d_skybox.frag` |
| 环境贴图（IBL 源） | 启动时 CPU 生成的 **256²×6 静态渐变 cubemap**，`mipLevels=1`、`R8G8B8A8_UNORM` | `vk3dInitScene.cpp:219,225,227` |
| 该 cubemap 的生成方式 | 每个面都按**图像 y** 线性插值，不按方向重建 → 不是真实天空（面与面之间也不连续） | `vk3dInitScene.cpp:274` |
| IBL 漫反射 | `texture(skyboxCubemap, N)`，没做 irradiance 卷积 | `gn3d_pbr.frag:254`、`gn3d_deferred_light.frag:247` |
| IBL 镜面 | 采 `R` 方向 **mip0**，只乘 `1-0.7*roughness` 与 `0.5` | `gn3d_pbr.frag:255-256,267-268`、`gn3d_deferred_light.frag:248-249,259-260` |
| 环境 BRDF | 无 BRDF LUT、无 split-sum，镜面直接乘 Fresnel | `gn3d_pbr.frag:259-268` |
| AO | 材质 AO 写进 G-buffer；只乘 diffuse IBL，镜面没有遮挡 | `gn3d_gbuffer.frag:52,64`、`gn3d_deferred_light.frag:256,259-260` |
| SSAO | 有片元着色器（71 行），**无管线 / 描述符 / 调用点**；法线取自 G-buffer 却是世界空间，位置却按视空间比较 | `gn3d_ssao.frag:23-34`、`gn3d_gbuffer.vert:41-42`；`grep -r gn3d_ssao` 只命中该文件 |
| 环境光 | scene × light 两次相乘；延迟再 ×3；IBL 再 `mix` 0.5 | `gn3d_deferred_light.frag:244,256`、`gn3d_pbr.frag:264` |
| 直接光 | 8 光源 + 单张 2D 方向光阴影（点光 cube 阴影仅预留） | `GN_3Dlight.h:22`、`gn3d_pbr.frag:243` |
| G-Buffer（格式/同步/生命周期） | 3×RGBA16F、1× 采样、**无任何 `VkSubpassDependency`**、resize 不重建、附件命名与内容错位 | §5；`vk3dInitScene.cpp:491-495,507-538,608` |
| 延迟 pass | G-buffer pass 由 `GN_VkDeferredLighting` **内部**结束 | `vk3dPasses.cpp:512-520` |
| HDR | 有 HDR 目标与合成；PBR 内已做一次 Reinhard，合成再乘一次 | `gn3d_pbr.frag:274-275`、`gn3d_composite.frag:42-43` |
| 计算管线 | 有 `ePIPE_COMPUTE` 与 `CreateComputePipeline`，**全仓无 `vkCmdDispatch`** | `GN_Pipeline.h:25`、`vkPipeline.cpp:191` |
| 光追 | 无 `VK_KHR_ray*`、无加速结构 | `grep -ri "ray_query\|accelerationstructure"` 无命中 |
| 天气联动 | 只有 `WeatherOutput.ambientScale` 影响环境光强度；cubemap 不随天气重烘 | `weather.h:63-68`、`weather.cpp:206-210` |

补充事实：

- S2a 之后，**可见天空与 IBL 已经不是同一张图**：`GN_VkDrawSkybox` 画解析式天空，而 PBR / deferred 仍采样启动时的静态渐变 cubemap。光滑金属表面能直接看出这个矛盾——反射里没有云、没有太阳。
- `GN_VkDeferredLighting` 自己结束 G-buffer pass（`vk3dPasses.cpp:512-520`），所以 SSAO 必须插在它之前：要么把「结束 pass」的逻辑前移到 SSAO API，要么让 SSAO 内部结束它。
- 前向 PBR 直接渲染到 swapchain（`GN_VkBegin3DPassPBR`），**没有深度/法线 MRT** → SSAO 不能用于前向路径，除非给前向再加 G-buffer 预写（大改，不建议 v1 做）。
- 描述符槽位：延迟 0..7 已满（`vk3dInitScene.cpp:735`）、PBR 0..5、前向 0..2。
  天气计划已把 Weather UBO 预留在 3/6/8，GI 必须另选槽位（见 §4.3），两份计划要用同一张表。
- 引擎当前**单帧 in flight**（只有一个 `inFlightFence`，`vkFrame2d.cpp:118`），所以单份 G-Buffer framebuffer 目前安全；这是约束不是缺陷，改成双帧并发时要 ×N（§5.3）。

## 2. 差距与判断

GI 的三个必要条件，现状分别缺什么：

| 条件 | 现状 | 缺什么 |
|---|---|---|
| 能量来源 | 静态天空 cubemap | 与天气同源、irradiance 卷积、随粗糙度的预滤波 |
| 可见性 | 材质 AO 常量 | 屏幕空间遮蔽（SSAO）、镜面遮挡 |
| 空间传输 | 无 | 一次弹射（SSGI）、屏幕外信息（探针）、多次弹射（光追） |

结论：**G0 先补 G-Buffer**，**v1 补「正确性」**（split-sum + SSAO），**v2 补「一次弹射」**（SSGI），**v3 补「屏幕外」**（探针）。
顺序不能反：SSGI 的 miss 回退依赖正确的环境贴图；而 SSAO/SSGI 的输入都来自 G-Buffer，G-Buffer 不对，后面全是白做。

## 3. 方案对比

| 方案 | 内容 | 成本 | 结论 |
|---|---|---|---|
| A. IBL 正确化 + SSAO | split-sum 环境贴图 + 屏幕空间 AO | 低（无新 pass 依赖，可 CPU 预计算） | ✅ v1 |
| B. SSGI | 屏幕空间 ray march 间接光 + 单帧去噪 | 中（半分辨率 pass + 新纹理） | ✅ v2 |
| C. 探针 | SH9 网格 / DDGI | 中–高（DDGI 需先补 compute dispatch） | ✅ v3（先 SH9，DDGI 可选） |
| D. 硬件光追 | ray query + 加速结构 | 高（扩展、AS、降噪、驱动要求） | 远期，不做 |
| E. 烘焙 lightmap | 离线烘焙 | 与动态模型/天气冲突 | 不采用 |

## 4. v1：正确的 IBL + SSAO

### 4.1 环境贴图拆分（split-sum）

目标是把「一张全频天空」拆成三件预计算数据：

| 数据 | 用途 | 形式 |
|---|---|---|
| irradiance SH9 | 漫反射 | 9 个 `vec4`，直接进 GI UBO（省一张纹理） |
| prefiltered specular | 镜面 | cubemap + mip 链（mip 对应粗糙度） |
| BRDF LUT | 镜面积分 | 128² `RG16F`，横轴 `NdotV`、纵轴 `roughness` |

**源的选择**（这是 v1 唯一的架构分叉）：

| 方案 | 做法 | 代价 | 结论 |
|---|---|---|---|
| a | 启动时从现有 256²×6 cubemap 生成上述三件 | 最小，可 CPU 单测；但源本身不是真实天空 | ✅ v1（只修「滤波方式」，不修「来源」） |
| b | 新增 GPU 6 面 pass，用解析天空烘 128²×6 | 与可见天空同源；需 6 面 render pass + mip 生成 | v1.5 / G3 |
| c | CPU 复制一份天空函数 | 与 GLSL 必然漂移 | 不采用 |

推荐 v1 先用 a：它把「采样方式」修对，且不改动任何 pass 结构；「来源」问题在 G3 用 b 解决。
a 的预计算都在初始化时做一次，参数与成本：

| 项 | 建议值 | 估算 |
|---|---|---|
| 预滤波 cubemap | 128²×6，5~6 级 mip，GGX 重要性采样 64 | ≈ 31M 次采样（约 0.3–0.8 s 启动）；太慢就降到 64²（≈ 8M） |
| irradiance SH9 | 从 64²×6 降采样后积分 | ≈ 25k 样本，可忽略 |
| BRDF LUT | 128²，每 texel 64 样本 | ≈ 1M 样本 |
| 存储 | 预滤波 RGBA16F（或 RGBA8 + 显式 gamma），LUT RG16F | 约 1.5 MB |

采样公式（两个片元着色器共用同一段）：

```glsl
// irradiance：SH9 求值，已含余弦积分
vec3 irradiance = evalSH9(gi.sh, N);

// 镜面：mip 级 = roughness × maxMip，LUT 给出积分项
vec3 prefiltered = textureLod(envCube, R, roughness * gi.envParams.y);
vec2 ab = texture(brdfLUT, vec2(NdotV, roughness)).rg;
vec3 specularIBL = prefiltered * (F0 * ab.x + ab.y);

vec3 diffuseIBL = kD * albedo * irradiance;
vec3 ambient = diffuseIBL + specularIBL * gi.indirect.y;  // x 给漫反射倍率
```

修改点：`gn3d_pbr.frag:247-270`、`gn3d_deferred_light.frag:237-262`；
删掉 `0.5` / `0.7` / `×3.0` 这些魔数（`gn3d_deferred_light.frag:244,256`），改由 GI UBO 控制。

**AO 复合**：`ao = materialAO * ssaoAO`；`diffuseIBL *= ao`；`specularIBL *= mix(1.0, ao, 1.0)` 或用 horizon 近似
（`ao` 的 `1-(1-ao)^2`）——先做线性版，观感不够再换。

### 4.2 SSAO（只做延迟路径）

现有 `gn3d_ssao.frag` **不能直接接线**，先修两处：

1. **空间不一致**：G-buffer 写的是世界空间法线（`gn3d_gbuffer.vert:41-42`），SSAO 却拿它和视空间位置/投影比较（`gn3d_ssao.frag:23-34`）。修法按 §5.4 的约定统一到**世界空间**：UBO 里带 `viewProj` + `invViewProj`，从深度重建世界位置，核与比较都在世界空间。
2. **无模糊**：16 样本的原始 AO 噪点明显，补一个 4×4 双边模糊 pass（`gn3d_ssao_blur.frag`，按深度/法线拒绝跨边界的样本）。

接线顺序与 API：

```text
GN_VkBegin3DPassDeferred(...)
  GN_VkDraw3D_GBuffer(...) × N
  GN_VkSSAOPass();        // 新增：结束 G-buffer pass → SSAO → blur → 转 SHADER_READ
GN_VkDeferredLighting(...) // 读 AO 纹理
```

- 需要把 `GN_VkDeferredLighting` 里「如果还在 pass 就结束」的分支保留（`vk3dPasses.cpp:512-520`），SSAO 先结束时它什么都不做。
- 输出全分辨率 `R8`；AO 只乘间接项，不乘直接光。
- 描述符：SSAO 独立 layout（`binding 0`=SSAO UBO〔`viewProj` 64 + `invViewProj` 64 + 16×`vec4` 256 + 参数 16 = 400 B〕、`1`=normal、`2`=depth）；
  G-buffer 的 finalLayout 已经是 `SHADER_READ_ONLY` / `DEPTH_STENCIL_READ_ONLY`（`vk3dInitScene.cpp:575-593`），
  但这**只是布局转换，不是可见性保证**——依赖要在 G-buffer render pass 里显式声明（§5.3）。
- 采样核：CPU 生成 16 点半球（Halton + 随机切向），半径按场景尺度（默认 0.5 m）。
- 关闭时 `ao = materialAO`，画面与现在一致（可回退）。

#### 4.2.1 G2b 实施细化（2026-10-30 重新规划；每步是独立验收门）

**冻结契约**

| 项 | 值 |
|---|---|
| `sSSAO_UBO`（giParams，CPU 单测） | 400 B：`viewProj` mat4 @0、`invViewProj` mat4 @64、`samples[16]` vec4 @128（256 B）、`params` vec4 @384（x=radius, y=bias, z=power, w=启用位） |
| AO 目标 | 全分辨率 `R8_UNORM`，两张：`rawAO` / `blurAO`；usage COLOR_ATTACHMENT\|SAMPLED |
| SSAO 描述符 set | 0 = SSAO UBO、1 = G-Buffer normal（RGBA16F）、2 = G-Buffer depth |
| 模糊描述符 set | 0 = SSAO UBO、1 = rawAO、2 = depth、3 = normal（双边拒绝用） |
| 延迟 layout | 12 → 13：binding 12 = blurAO（`sampler2D`）；GI UBO `indirect.w` = SSAO 开关（0=关） |
| invViewProj | GI UBO 偏移 192；`GN_VkSSAOPass` 从 `GN_VkBegin3DPassDeferred` 记录的 viewProj 求逆后写入 |
| Pass 顺序 | `Begin3DPassDeferred → Draw3D_GBuffer×N → GN_VkSSAOPass()（先 end pass → SSAO → blur → SHADER_READ）→ DeferredLighting` |
| 默认 | `indirect.w=0`：延迟 shader 用 `materialAO`，不采样 AO 图；`GN_VkSSAOPass` 也直接返回（不跑 pass），现有观感与开销不变 |

**分步（每步跑 make + make unit + glslangValidator/lint，失败只回退该步）**

1. **G2b-1 CPU 契约**：`giParams` 新增 `sSSAO_UBO`（偏移 static_assert）、`makeDefaultSsaoUbo`、`packSsaoUbo`；**修复 `GNg::Mat44Inverse`**（旧实现对纯平移矩阵返回错误，`testSkeleton.cpp:46` 与 `vkCloud.cpp:11` 已登记）为正确列主序 4×4 逆，`testMath3D` 加平移逆/TRS/透视 roundtrip/奇异用例；`testGi` 加 UBO 用例（大小/偏移、默认值/打包/非法输入）。验收：`make unit` 335→≥341（实测 **345**）。
2. **G2b-2 着色器**：重写 `gn3d_ssao.frag` 为世界空间（invViewProj 重建位置、世界法线、16 点核、range check、power）；新增 `gn3d_ssao_blur.frag`（4×4 双边，深度/法线拒绝）。验收：glslangValidator + `.spv`。
3. **G2b-3 `vkSsao.cpp` 资源**：两张 R8 目标/视图/采样器、两张 render pass（final=SHADER_READ_ONLY，显式 EXTERNAL↔subpass 依赖）、两条全屏管线（复用 `gn3d_deferred_light.vert`）、两套描述符、400 B host-visible UBO；`init3DSsao/destroy3DSsao/recreate3DSsaoTargets`。验收：`make` + lint。
4. **G2b-4 Pass 接线**：`GN_VkSSAOPass()`（先 end 3D pass，再 SSAO→blur；`indirect.w<=0` 直接返回）；`GN_VkBegin3DPassDeferred` 记录 viewProj；每帧写 GI UBO 偏移 192。验收：`make` + `gpuCheck` 编译。
5. **G2b-5 延迟合成**：`gn3d_deferred_light.frag` layout 12→13、binding 12 = blurAO、`ao = materialAO * mix(1, aoTex, indirect.w)`、`wpFromDepth` 改用 `gi.invViewProj`；重生成 `.spv`。验收：shader 编译 + `make unit` 不回归。
6. **G2b-6 resize/shutdown**：`GN_VkResize` 在 G-Buffer 重建后重建 SSAO 目标/framebuffer/管线并重写 normal/depth 描述符；shutdown 释放；`make gpuCheck` 编译。验收：`make`/lint。

**桌面验收（非本机）**：开 `GN_VkSetGI`（indirect.w=1）后角落变暗、无带状噪点、关掉回到现状；同步验证层 resize 无告警。

### 4.3 GI 参数 UBO 与描述符槽位

```cpp
// 与 assets/shaders 里的 GIUBO 逐字段一致；sizeof = 256 B
struct sGI_UBO {
    float indirect[4];    // x=漫反射倍率 y=镜面倍率 z=AO 强度 w=SSAO 开关(0/1)
    float aoParams[4];    // x=半径 y=偏移 z=幂 w=模糊半径
    float envParams[4];   // x=环境旋转 y=预滤波 mip 数 z=曝光补偿 w=保留
    float sh[36];         // SH9，每系数 4 float（144 B）
    float invViewProj[16]; // 深度 → 世界位置（64 B），替代每像素 inverse(vp)
};
```

槽位分配（**与天气计划共同冻结**，两份文档写同一张表）：

| 管线 | 现有占用 | Weather UBO（天气计划） | GI（本计划，G1b 已按此接线） |
|---|---|---|---|
| 前向 `gn3d` | 0=Scene, 1=tex, 2=Light | 3 | 4 = GI UBO（layout 4→5；`gn3d.frag` 暂未使用，槽位预留） |
| PBR | 0..5（5=skybox） | 6 | 7 = GI UBO、8 = 预滤波环境 cubemap、9 = BRDF LUT（layout 7→10） |
| 延迟 | 0..7（7=Light） | 8 | 9 = GI UBO、10 = 预滤波 cubemap、11 = BRDF LUT（layout 9→12） |
| SSAO | — | — | 独立 set |

- 新增 `GN_VkSetGI(const sGI_UBO&)`、`GN_VkSSAOPass()`（声明进 `GN_Vk.h`）。
- 三个描述符 layout（前向、PBR、延迟）各加 GI UBO（fragment 阶段），写入同一块 256 B 缓冲；
  PBR/延迟另加预滤波 cubemap 与 BRDF LUT 两个采样器槽位（G1b 实际接线，见 `vkGi.cpp`）。
  池 `maxSets=1024`、UBO/采样器各 1024，配额充足（`GN_Vk.cpp:361`）。
- 默认值：SSAO 关、GI 倍率 **0**（`disableGiByDefault`）⇒ 着色器走旧 Skybox IBL 分支，现有观感不变；
  调用方用 `GN_VkSetGI` 把 indirect.x/y 调到 >0 才启用 split-sum + SH9。

## 5. G-Buffer 实现缺失与补齐（GI 前置）

现有 G-Buffer 是「能跑延迟光照」的最小实现，但作为 GI 的输入还缺 12 项。下面按**对 GI 的阻断程度**排序，
每条都给证据、影响与补齐动作，验收在 §9 的 G0/G2。

### 5.1 缺失总览

| # | 缺失 | 现状与证据 | 对 GI 的影响 | 补齐动作 | 阶段 |
|---|---|---|---|---|---|
| 1 | 附件命名与内容错位 | `gAlbedoImage` 实际装 normalRough、`gNormalImage` 装 albedoMetal、`gPBRImage` 装 AO（`vk3dInitScene.cpp:493` vs `gn3d_gbuffer.frag:34-36`） | 加/改附件时极易接错，GI 的 AO、albedo 会串位 | 改名 `gNormalRoughImage` / `gAlbedoMetalImage` / `gAoImage`，并加 `enum eGBufferAttachment` 统一索引（含 shutdown 侧，`vk3dShutdown.cpp:281`） | G0 |
| 2 | 无显式 subpass 依赖 | 全仓 `VkSubpassDependency` **零命中**；G-buffer 写完靠 finalLayout 转 `SHADER_READ`，下一 pass 立即采样（`vk3dInitScene.cpp:575-593`） | SSAO/SSGI 读取无可见性保证；同步验证层会报 | 给 G-buffer pass 加 external→subpass0、subpass0→external 两条依赖（§5.3 给字段） | G0 |
| 3 | resize 不重建 | `GN_VkResize` 只重建 swapchain 与 2D framebuffer（`GN_Vk.cpp:664-728`），G-buffer/HDR/deferred framebuffer 仍是旧尺寸 | 改窗口后 GI/延迟全部错位，新 target 同样失效 | 抽出 `vk3dRecreateTargets()`；注意池只创建一次，不能反复 Shutdown/Init 3D（§5.3） | G0 |
| 4 | 无 alpha cutout | 所有 3D 着色器无 `discard`；G-buffer 也没有 alpha test 参数（`gn3d_gbuffer.frag` 全文）；`pc.albedo.w` 恒为 1（`vk3dPasses.cpp:481`） | MMD 头发/睫毛的 alpha 贴图在延迟路径变实心片，GI 的 albedo 也跟着错 | 复用 `pc.albedo.w` 作 `alphaCutoff`（0=不裁），片元 `if (a < cutoff) discard;`；材质 alpha 模式由导入侧提供 | G0 |
| 5 | 无调试视图 | 无法看 normal/roughness/AO/albedo | GI 调参只能靠猜，回归无法定位 | `GN_VkDebugGBuffer(int channel)` + 全屏调试着色器（伪彩输出 4 张图之一） | G0 |
| 6 | 法线空间未约定 | 存世界法线（`gn3d_gbuffer.vert:41-42`），消费侧却用 `inverse(proj)` 在视空间比较（`gn3d_ssao.frag:23-27`）；deferred 每像素 `inverse(vp)`（`gn3d_deferred_light.frag:97-101`） | SSAO 方向错误；SSGI 光线空间混乱 | 冻结约定：**法线与位置都用世界空间**，`invViewProj` 由 GI UBO 提供（§4.3），删掉每像素求逆 | G0（约定）/G2（接线） |
| 7 | 无 AO 目标通道 | AO 只有材质常量（`gn3d_gbuffer.frag:64`），SSAO 无处可写 | SSAO 无法落地；镜面没有遮挡 | 新增 `R8` AO 目标图（SSAO pass 输出，blur 后供 deferred 采样） | G2 |
| 8 | 无速度/运动矢量 | 无 velocity 附件；引擎无 TAA | SSGI 只能单帧 + 双边；MMD 动态物体无法 reproject | v2 先不做时域；要 TAA 再加 `RG16F` velocity（静态用上帧 VP + depth reprojection 近似，动态需上帧模型矩阵，push constant 已无空间，得进 SSBO/UBO） | G4（可选） |
| 9 | 无 specular/F0 通道 | 介电体 F0 固定 0.04（`gn3d_pbr.frag:238`） | split-sum 后镜面颜色仍不可按材质调 | AO 附件扩成 `RGBA8`：R=AO、G=反射率、B=材质 ID、A=保留 | G2/G4 |
| 10 | 格式浪费 2/3 带宽 | 3×RGBA16F = 24 B/px（`vk3dInitScene.cpp:492`；注释还写 R8G8B8A8，`491`） | 加 AO/velocity 后显存与带宽进一步放大 | 保守方案 normal 保留 RGBA16F、albedo/材质打包到 `RGBA8` → 16 B/px；激进方案 normal 用 octahedral `RG16F` → 12 B/px（§5.2） | G2 |
| 11 | 无 MSAA | G-buffer 固定 1 sample（`vk3dInitScene.cpp:507,710`），而 HDR/PBR/天空用 `msaaSamples=4`（`vk3dInitScene.cpp:471`） | 延迟路径边缘锯齿；SSAO/SSGI 的采样噪点更明显 | 二选一：4× MSAA G-buffer（显存 ≈ 200 MB@1080p，需 resolve）或后处理 FXAA/SMAA（§5.2） | G0/G2 |
| 12 | 每 draw 更新描述符 | `GN_VkDraw3D_GBuffer` 每个 draw 写 binding 1/3 两张纹理（`vk3dPasses.cpp:463-470`） | GI 再加 binding 后 CPU 开销叠加 | 按纹理组合缓存 descriptor set（复用材质缓存思路），或至少只在纹理变化时写 | G4（性能） |

补充约束：G-buffer 只有一份 framebuffer（`vk3dInitScene.cpp:608`），引擎单帧 in flight（`vkFrame2d.cpp:118`）→ 目前安全；
若以后上双帧并发，G-Buffer 与延迟目标都要 ×N。这条只登记，不在本轮动作里。

### 5.2 格式、带宽与 AA

**格式打包**（推荐保守方案，先降 1/3 带宽，风险低）：

| 附件 | 现在 | 建议 | 说明 |
|---|---|---|---|
| 0 normal+rough | RGBA16F（8 B） | RGBA16F（8 B） | 法线精度最关键，先不动；以后可换 octahedral `RG16F`（4 B） |
| 1 albedo+metal | RGBA16F（8 B） | **RGBA8**（4 B） | albedo 用 sRGB 编码存、片元里解码，避免 8-bit 线性暗部带状 |
| 2 AO+材质 | RGBA16F（8 B） | **RGBA8**（4 B） | R=AO、G=反射率、B=材质 ID、A=alpha/保留（为 §5.1-9 预留） |
| 3 depth | D32 | D32 | 不变；`SAMPLED` 已开（`vk3dInitScene.cpp:536`） |

结果：24 → 16 B/px（1080p 约 50 → 33 MB），且 AO/材质有了正式通道。**激进方案**（normal 换 octahedral `RG16F`）可到 12 B/px，
但法线在光滑金属上可能出现可见量化，作为 G2 之后的优化项，不在首轮。

> **G2a 已落地（2026-10-30，编译级）**：`vkGbuffer.cpp` 颜色格式表 = `{RGBA16F, RGBA8_SRGB, RGBA8_UNORM}`（深度 D32 不变），
> 颜色 24→16 B/px；`gn3d_gbuffer.frag` 附件 1 写线性 albedo（硬件 sRGB 编码）+ metallic，附件 2 写
> `(AO, reflectance=0.04 占位, materialId=0 占位, alpha)`，alpha cutoff 预留 `pc.specular.w`（C++ 恒 0）；
> `gn3d_deferred_light.frag` 仅注释更新（SRGB 采样自动 linear）；`gpuCheck 3d.deferred.gbuffer` 改 RGBA8 + `srgbDecode`。
> 未验证：格式兼容/验证层、8-bit 精度、带宽、运行时断言。

**MSAA 取舍**：

| 方案 | 代价 | 结论 |
|---|---|---|
| A. 4× MSAA G-buffer | 显存 ×4（≈ 200 MB@1080p），需要 resolve 后再给 SSAO/SSGI 采样 | 与前向路径观感一致，但重 |
| B. 1× G-buffer + 后处理 AA（FXAA/SMAA） | 一个全屏 pass，显存几乎不变 | ✅ 推荐（延迟路径先跑通，观感问题留给后处理） |

### 5.3 同步与生命周期

**subpass 依赖**（G-buffer 读上一帧的 sampling、写本帧 attachment；延迟/SSAO 在下一个 pass 采样）：

```cpp
VkSubpassDependency deps[2] = {};
// 上一帧的采样（deferred 读 G-buffer）→ 本帧写 attachment
deps[0].srcSubpass = VK_SUBPASS_EXTERNAL;
deps[0].dstSubpass = 0;
deps[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
deps[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                       VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
deps[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
deps[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
// 本帧写完 → 下一 pass 采样
deps[1].srcSubpass = 0;
deps[1].dstSubpass = VK_SUBPASS_EXTERNAL;
deps[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                       VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
deps[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
deps[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
deps[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
```

**resize**：`GN_VkResize`（`GN_Vk.cpp:664`）在重建 swapchain 后必须重建所有依赖尺寸的 3D 资源：
G-buffer 三色 + 深度、HDR 目标、deferred light framebuffer、合成 framebuffer。注意：

- 描述符池只创建一次（`GN_Vk.cpp:329`，`maxSets=1024`，无 `FREE`/`RESET`）→ **不能**简单反复 `GN_VkShutdown3D()/GN_VkInit3D()`，否则每 resize 泄漏一批 set；
  推荐把 3D 的池独立出来（重建时销毁重建），或只重建 image/framebuffer 并 `vkUpdateDescriptorSets` 指向新 view。
- 重建前 `vkDeviceWaitIdle`（现有 resize 已做）。
- 新加的 GI/SSAO 目标同样挂在 `vk3dRecreateTargets()` 下，不要各写一份。

**G-Buffer 帧数**：单份 framebuffer 与单 `inFlightFence` 匹配；引入双帧并发时必须 ×2 并同步更新 descriptor。

### 5.4 通道语义与 G-Buffer 契约（冻结）

附件映射（改名前 → 内容 → 延迟 binding；改名后同名替换）：

| 附件 | image 变量（现在） | 内容 | 延迟 binding |
|---|---|---|---|
| 0 | `gAlbedoImage` | normal(RGB, N*0.5+0.5) + roughness(A) | 1 |
| 1 | `gNormalImage` | albedo(RGB) + metallic(A) | 2 |
| 2 | `gPBRImage` | AO(R)，GBA 未用 | 3 |
| 3 | `gDepthImage` | D32 | 4 |

冻结口径（SSAO / SSGI / deferred 都按这张表）：

- **空间**：normal 与重建位置都用**世界空间**；`invViewProj` 放 GI UBO（§4.3），不再每像素 `inverse()`。
- **法线编码**：`N*0.5+0.5` 存 RGBA16F；若改 octahedral，编码/解码函数进 `giParams`（CPU 单测 roundtrip）。
- **AO**：附件 2 的 R 是**材质 AO**；SSAO 结果另存 `R8` 目标图，合成 `ao = materialAO * ssaoAO`，只作用间接项。
- **alpha**：`pc.albedo.w = alphaCutoff`（0=不裁），片元 `if (albedo.a < cutoff) discard;`。
- **坐标系**：Vulkan NDC z∈[0,1]，投影带 Y 翻转（`Mat44PerspectiveVK`）——重建函数必须和现有着色器一致（`gn3d_deferred_light.frag:97-101` 是唯一参考实现）。

### 5.5 调试与可验证性

- `GN_VkDebugGBuffer(int channel)`：0=normal、1=albedo、2=AO/roughness、3=depth；全屏输出伪彩。调 GI 前先确认这四张图正确。
- 开关方式：环境变量或查看器按键（不新增公开 API 面）。
- 验证：开同步验证层跑一次「resize → 延迟帧」，应无 layout/依赖告警；这是 G0 的硬验收。
- CPU 可测部分：打包/解包（albedo sRGB 编解码、AO 位域）、octahedral 编解码 roundtrip、alpha cutoff 判定。

## 6. v2：SSGI（屏幕空间一次弹射）

Pass 顺序变成：

```text
G-buffer → SSAO → SSGI(半分辨率) → 双边去噪 → 延迟光照
```

- 输出 `RGBA16F` 半分辨率；视空间 ray march：8 rays × 8 steps，命中点用 depth 反投影、拿 G-buffer 的 albedo 累加。
- miss 回退到环境 irradiance（§4.1 的 SH9），这样屏幕外不会变黑。
- 去噪：单帧双边 5×5（G-Buffer 没有 velocity、引擎没有 TAA，不做时域累积，避免拖影；见 §5.1-8）。
- 合成：`diffuseIBL = mix(envDiffuse, ssgiDiffuse, valid * strength)`；镜面 v2 仍走预滤波环境，v2.5 再评估。
- 预算：1080p 半分辨率 960×540 × 8 rays × 8 steps ≈ 33M 深度采样/帧；给质量档（4×12 / 8×8 / 关闭）。
- 绑定：延迟 set 再增加 SSGI 颜色 + 采样器（binding 10），与 GI UBO 一起扩容 layout / pool。
- HDR：GI 缓冲必须是线性空间、色调映射之前；存量「双 Reinhard」要先清理，否则 SSGI 亮度无法校准。

> **G4 落地状态（2026-10-30）**：上述 v1 版本已实现——半分辨率 1/2、8 rays×8 steps（`params.y` 仅契约占位，步数固定）、
> 5×5 单帧双边、延迟 binding 13 合成、`envParams.w` 为强度（0=关）。未做：时域累积/velocity、全分辨率、镜面 SSGI、
> 亮度标定；`tests/gpu` 延迟用例有调用点，但尚未有「开启 SSAO/SSGI 后断言」的用例。

## 7. v3：探针（屏幕外信息）

- 前置：补 compute dispatch 封装。`ePIPE_COMPUTE` 与 `CreateComputePipeline` 已存在（`GN_Pipeline.h:25`、`vkPipeline.cpp:191`），
  但全仓没有 `vkCmdDispatch`，DDGI 的更新通道目前不存在。
- 方案 A（轻，推荐先做）：相机周围 SH9 探针网格（8×4×8，间距 2 m），CPU 每帧更新 1–2 个探针
  （从天空 + 太阳可见性积分），采样用三线性 + 时间滞后；不需要 compute。
- 方案 B（重）：DDGI（irradiance + depth moments，3D 纹理，compute 更新）。只有在需要动态遮挡时才值得。
- 与 SSGI 混合：屏幕内有效用 SSGI，miss / 屏幕外用探针。

### 7.1 G5b 设计冻结（2026-10-30；实现前先按此表，禁止自行改契约）

| 项 | 冻结值 |
|---|---|
| 探针数据 | CPU `cPROBE_GRID`（G5a，8×4×8、间距 2 m、SH9=36 float/探针）→ 上传为 **9 张 3D 纹理** `RGBA16F` 8×4×8（每张 = 1 个 SH 系数 vec4，RGB+pad；9×4=36 float/探针，无损；256×36=9216 floats = 9×1024 texels×4） |
| 描述符（延迟 set） | 13→**23**：13=SSGI（G4）、**14=probe UBO（48 B）**、**15..23=9 张探针 3D 纹理**（每张对应一个 SH 系数） |
| probe UBO（新 `sPROBE_UBO`，放 giParams） | 48 B：`origin` xyz+pad @0、`spacing` + `counts` xyz @16、`weight`（0=关）@32、`reserved` @36；static_assert + 单测 |
| 采样 | 片元：`probeSh9[k] = texture(Tk, wp)`（9 次三线性采样后组合）；`envDiffuse = mix(evalSH9(N), probeSH9(N), clamp(weight,0,1))`；`weight=0` 逐像素等同现状 |
| 更新预算 | CPU 每帧最多更新 **2 个探针**（从当前环境 SH9 起，v1 不做遮挡 raycast；预留 occlusion 回调），`BlendProbeSh(alpha=0.15)` 时间滞后；任何探针变化才整网格上传（9 张 × 8 KB ≈ 72 KB staging） |
| 开关 | 新增 `GN_VkSetProbeWeight(float)`（写 probe UBO.weight）；默认 0=关，不改变现有画面 |
| 生命周期 | 网格固定尺寸，不随 resize 重建；shutdown 释放 3 纹理/UTexture/UBO；相机移动由 `Recenter` 决定是否整网格搬移 |
| 不做 | DDGI/compute、时域累积、屏幕外遮挡 raycast（v1 只从天空 SH9 初始化） |
| 验收 | 本机：`make unit` + `sPROBE_UBO` 用例 + 编译；桌面：屏幕外遮挡场景间接光变化、关掉 weight 回到现状 |

**实现顺序（G5b 分两步）**：**G5b-1 已完成（2026-10-30）**——`sPROBE_UBO` 48 B 契约 + `GetProbeSh` + `packProbeTextures`（按裁决 A：9 张 RGBA16F 纹理 float 层，9216 floats 无损；testGi +5、testProbe +4，`make unit` = 369/0）。**G5b-2 已完成（2026-10-30，编译级）**：`_Vulkan/vkProbe.cpp`（272 有效行）——9 张 3D RGBA16F 8×4×8 + LINEAR/CLAMP sampler、探针 UBO 48 B、启动用环境 SH9 填满 256 探针并整网格上传（18 KB staging，9 次 copy）；延迟 layout 14→23；`GN_VkSetProbeWeight`（默认 0）与 `GN_VkUpdateProbes`（weight=0 返回；否则每帧最多 2 探针 Blend 0.15）；shader `sampleProbeSh` 三线性 + SH9，weight=0 不采样、逐像素等同现状。尺寸固定不随 resize 重建。**限制**：无应用侧调用者（gpuCheck 延迟用例已插 `GN_VkUpdateProbes`，weight 需应用设）；v1 未接 `Recenter`/天气重烘刷新探针。
>
> **MMD 查看器接线（2026-10-30，范围 A）**：模型绘制从 `cMODEL::Draw()`（Blinn-Phong）改为 `DrawPBR()`，并在 AppInit 调
> `GN_VkEnableGI(true)` 开启 GI IBL 倍率（`indirect[0]/[1]`，SH9/envParams 保留）；天气切换仍走 G3 重烘。
> **SSAO/SSGI/探针不适用于查看器**（前向无 G-Buffer，均只存在于延迟路径）；若要在 MMD 里看到它们，需先实现 cMODEL 的 G-Buffer 绘制（大改）。

## 8. 文件与接口规划（单功能 <500 有效行）

| 文件 | 职责 | 预估有效行 |
|---|---|---|
| `src/graphics/giParams.h/.cpp`（新） | GI UBO 默认值/校验、SSAO 核（Halton）、SH9 积分与求值、BRDF 解析近似、打包/解包与 alpha cutoff | ~260 |
| `src/graphics/_Vulkan/vkGbuffer.cpp`（新，从 `vk3dInitScene` 迁出） | G-Buffer 创建/重建、附件枚举、subpass 依赖、调试视图、alpha cutout 接线、resize 重建 | ~340 |
| `src/graphics/_Vulkan/vk3dInitScene.cpp`（改） | **只留** HDR 目标/合成/延迟光照；G-Buffer 与天空盒迁出后应降到 500 有效行以内 | ~-300 |
| `src/graphics/_Vulkan/vkGi.cpp`（新） | GI UBO 创建/上传、`GN_VkSetGI`、三个描述符集的 binding 写入 | ~140 |
| `src/graphics/_Vulkan/vkSsao.cpp`（新） | SSAO 图像/模糊图/描述符/两条管线/`GN_VkSSAOPass` | ~260 |
| `src/graphics/_Vulkan/vkEnvironment.cpp`（新） | 预滤波 cubemap、mip 生成、SH9、BRDF LUT（CPU 生成 + 上传） | ~300 |
| `assets/shaders/gn3d_gbuffer.frag`（改） | alpha cutout、AO/材质打包（按 §5.2）、世界空间约定 | ~+30 |
| `assets/shaders/gn3d_gbuffer_debug.frag`（新） | 四通道伪彩调试 | ~40 |
| `assets/shaders/gn3d_ssao.frag`（改） | 统一世界空间 + 读 GI UBO | ~80 |
| `assets/shaders/gn3d_ssao_blur.frag`（新） | 4×4 双边模糊 | ~40 |
| `assets/shaders/gn3d_pbr.frag` / `gn3d_deferred_light.frag`（改） | split-sum + AO 复合 + `invViewProj` 重建 | 各 ~+30 |
| `assets/shaders/gn3d.frag`（改，可选） | 前向 Blinn-Phong 补环境漫反射 | ~+15 |
| `assets/shaders/gn3d_ssgi.frag` / `gn3d_ssgi_denoise.frag`（v2 新） | ray march + 双边 | ~180 / ~80 |
| `src/graphics/_Vulkan/GN_Vk.h`（改） | 新 API / UBO 声明 / SSAO 与 G-Buffer 句柄（含改名） | ~+60 |
| `src/graphics/_Vulkan/vk3dShutdown.cpp`（改） | 改名同步 + GI/SSAO 资源释放 | ~+30 |
| `tests/unit/testGi.cpp`（新） | 默认值、核分布、SH9 能量守恒、BRDF 单调、打包 roundtrip、alpha cutoff、确定性 | ~200 |

约束：`vk3dInitScene.cpp` 机检 813 有效行，**本轮必须减负**（G-Buffer + 天空盒迁出），不是继续加；
新增文件名/函数名 camelCase、类型 PascalCase，`GN_*` 是外部契约名保留。

## 9. 分阶段与验收

| 阶段 | 内容 | 验收（本机可做） |
|---|---|---|
| G0 | G-Buffer 结构修复：附件改名 + 枚举、显式 subpass 依赖、resize 重建、调试视图、alpha cutout、注释纠错 | 开同步验证层 resize 无告警；调试视图四通道正确；MMD 头发不再实心；`make check` |
| G1 | IBL 正确化：预滤波 cubemap + SH9 + BRDF LUT + GI UBO + 两个片元着色器 | `make check`；CPU 单测（SH9 常数环境 = 常数 irradiance、LUT 单调）；桌面看「粗糙度越大反射越糊」 |
| G2 | G-Buffer 格式打包（16 B/px）+ AO 目标 + `invViewProj` 重建 + SSAO 接线/模糊/合成 | `make check`；桌面看角落变暗、无带状噪点；关掉 SSAO 画面回到现状 |
| G3 | 环境贴图与解析天空同源（GPU 6 面烘焙，天气切换时重烘） | 桌面：切 1..6 天气，光滑球反射的天空与背景一致 |
| G4 | SSGI（半分辨率 + 单帧双边；velocity 可选） | 桌面：颜色渗色可见；帧时间在预算内；关闭可回退 |
| G5 | 探针（SH9 网格；DDGI 可选） | 桌面：屏幕外遮挡有间接光变化 |

> **状态（2026-10-31，代码级全部完成；运行时均未验证）**：

| 阶段 | 状态 | 主要产物 / 证据 | 剩余（桌面） |
|---|---|---|---|
| G0a | ✅ 代码级 | `_Vulkan/vkGbuffer.cpp`：附件枚举 `eGBUFFER_ATTACHMENT`、字段改名、两条显式 subpass 依赖、resize 重建、统一 destroy | 验证层 resize 无告警 |
| G0b | ✅ 代码级 | `_Vulkan/vkGbufferDebug.cpp` + `gn3d_gbuffer_debug.frag`（四通道）；`GN_VkDraw3D_GBuffer` 的 `alphaCutoff` 参数 | 调试视图四通道 |
| G1-CPU | ✅ 单测 | `giParams.h/.cpp` + `testGi` 21 用例（UBO/SH9/SSAO 核/BRDF LUT/打包/cubemap 预滤波） | — |
| G1a | ✅ 代码级 | `_Vulkan/vkEnvironment.cpp`：GPU 6 面烘焙解析天空 → 回读 → CPU 预滤波 → cubemap/LUT/GI UBO | 面朝向/启动耗时 |
| G1b | ✅ 代码级 | `_Vulkan/vkGi.cpp` + 槽位 4/7/9（+8/9、10/11）+ split-sum/SH9 着色器；**默认 GI 关** | 亮度/槽位兼容 |
| G2a | ✅ 代码级 | G-Buffer 颜色 24→16 B/px（`{RGBA16F, RGBA8_SRGB, RGBA8_UNORM}`） | 8-bit 精度/带宽 |
| G2b | ✅ 代码级 | `_Vulkan/vkSsao.cpp`：R8 raw/blur、世界空间 SSAO、4×4 双边、binding 12 合成、`Mat44Inverse` 修复；**默认 SSAO 关** | AO 视觉/模糊 |
| G3 | ✅ 代码级 | `GN_VkGetSky` + `GN_VkRebakeEnvironment`；天气目标变化+过渡结束后重烘一次（启动首帧一次） | 同源视觉/切换卡顿 |
| G4 | ✅ 代码级 | `_Vulkan/vkSsgi.cpp` + `gn3d_ssgi*.frag`：半分辨率 8×8 march + 5×5 双边；`envParams.w` 开关，**默认 0** | 渗色/性能 |
| G5a | ✅ 单测 | `GN_3Dprobe.h/.cpp` + `testProbe` 10 用例（8×4×8 网格/三线性/Recenter） | — |
| G5b | ✅ 代码级 | `_Vulkan/vkProbe.cpp`：9 张 3D 纹理 + 延迟 layout 23 + shader 采样；`GN_VkSetProbeWeight` 默认 0 | weight>0 视觉 |
| MMD 接线 | ✅ 编译级 + 目视通过（2026-10-31） | 模型 `DrawPBR` + `GN_VkEnableGI(true)`；SSAO/SSGI/探针仅延迟路径（查看器前向无 G-Buffer） | 默认 GI IBL 由用户目视通过；SSAO/SSGI/探针默认关闭 |

> 当前 `make unit` = **371 cases / 0 failed**（含 camera 22、GI/SSAO/SSGI/探针 CPU 用例）。
> 真 GPU（AMD Radeon 780M / RADV PHOENIX）`make gpuCheck` = **99/99 PASS / 0 skip**（2026-10-31）；
> 默认 GI IBL 画面由用户目视验收通过；SSAO/SSGI/探针仍默认关闭。
> 已知限制：GI/SSAO/SSGI/探针默认关闭（需 `GN_VkSetGI`/`GN_VkSetProbeWeight` 开启）；BLEND 只做前向 PBR（网格级排序）；SSGI 步数固定 8。

每阶段收尾：`make check`、`bash ~/.dsh/skills/code-modularity/scripts/lintCodeModularity.sh <写域> --naming --comments`、
GLSL 用 `glslangValidator -V` 编译通过。`workspace-write` 沙箱隐藏 `/dev/dri`（`make gpuCheck` SKIP），
真机验收用 `danger-full-access` 或桌面终端跑同一命令。

## 10. 风险与待决

1. ✅ 已解决（G1a/G3）：环境由 GPU 6 面烘焙解析天空，天气切换重烘。
2. ✅ 已解决（G2b/G4）：`GN_VkSSAOPass`/`GN_VkSSGIPass` 在延迟前调用，keep 原有 end-pass 分支。
3. ✅ 已解决（task-8/task-26）：resize 只重建 image/framebuffer/管线并重写描述符，池不拆；2D/3D 全部固定视口管线已纳入重建。
4. ✅ 已解决（G0a）：`eGBUFFER_ATTACHMENT` 映射统一创建/销毁/绑定，旧名全仓 0 处。
5. **打包精度**：albedo 存 `RGBA8` 若用线性编码，暗部会带状；用 sRGB 编码或先保留 16F（见 §5.2）。
6. **4× MSAA G-buffer**：显存 ≈ 200 MB@1080p，且 SSAO/SSGI 必须在 resolve 后采样；推荐先走后处理 AA。
7. ✅ 已解决（2026-10-31）：`sMATERIAL`/`sASSET_MATERIAL` alphaMode/cutoff，glTF `alpha_mode`，MMD MASK+0.5；BLEND 独立管线 + 网格级排序。
8. ✅ 已解决（G1b）：槽位表已冻结并落地（前向 4、PBR 7/8/9、延迟 9/10/11；延迟 layout 12）。
9. ✅ 已解决（G2b）：`gn3d_ssao.frag` 重写为世界空间 + 4×4 双边。
10. **前向路径没有 G-buffer**：SSAO 不可用；给前向加 MRT 会牵动所有前向绘制，v1 不做。
11. **HDR 双 Reinhard**：`gn3d_pbr.frag:275` 与 `gn3d_composite.frag:43` 各做一次。GI 必须在线性空间注入，建议 G1 前先统一到合成阶段，否则 SSGI 亮度无法校准。
12. **无 compute dispatch**：DDGI 的前置工作；SH9 探针不需要。
13. **启动成本**：CPU 预滤波约 31M 采样，若启动明显变慢就降到 64²，或挪到 G3 的 GPU 烘焙。
14. ✅ 已解决（2026-10-31）：真 GPU `make gpuCheck` = 99/99 PASS；默认 GI IBL 画面用户目视通过。此前沙箱无 `/dev/dri` 的 SKIP 已查明是 `workspace-write` 隐藏设备节点。

## 11. 待你确认（≤5）

1. **v1 环境贴图来源**（**已拍板 2026-10-30：选 B**）：(A) 启动时 CPU 从现有静态 cubemap 预滤波（最省事，与 S2a 天空不同源）；
   (B) 直接做 GPU 6 面烘焙（同源，但要新增 pass 与 mip 生成，工作量大）；
   (C) v1 先不做环境贴图，只接 SSAO + 修采样公式（最轻，但镜面仍然不正确）。
   用户选择 **B**：G1 先用「GPU 6 面烘焙解析天空 → 回读 → `giParams::prefilterCubemap` CPU 预滤波 → 上传 mip 链」，
   G3 再把预滤波也搬到 GPU 并接天气切换重烘。B 使 G1 与 G3 的边界前移，G3 只剩「天气切换重烘 + GPU 端预滤波」。
2. **G-Buffer 抗锯齿**：(A) 4× MSAA G-buffer（与前向一致，显存 ×4、需 resolve）；
   (B) 1× + 后处理 FXAA/SMAA（推荐，轻）；(C) 先不管锯齿。
3. **SSAO 只做延迟路径**（推荐），前向保持现状？还是接受给前向加 MRT 的大改？
4. **v2 SSGI 起步档**：半分辨率 + 8 rays + 单帧双边（推荐），还是全分辨率低 ray 数？
5. **v3 探针**：先 SH9 CPU 探针（推荐，不依赖 compute），还是直接 DDGI（要先补 compute dispatch）？

## 12. 参考锚点（复算用）

```bash
# IBL 现状
sed -n '247,270p' assets/shaders/gn3d_pbr.frag
sed -n '237,262p' assets/shaders/gn3d_deferred_light.frag
sed -n '215,300p' src/graphics/_Vulkan/vk3dInitScene.cpp

# G-Buffer 创建 / 附件 / 同步 / 生命周期
sed -n '487,610p' src/graphics/_Vulkan/vk3dInitScene.cpp
sed -n '30,70p' assets/shaders/gn3d_gbuffer.frag
grep -rn "VkSubpassDependency" src
sed -n '664,728p' src/graphics/_Vulkan/GN_Vk.cpp
sed -n '325,335p' src/graphics/_Vulkan/GN_Vk.cpp

# 附件映射与描述符占用
sed -n '490,500p' src/graphics/_Vulkan/vk3dInitScene.cpp
sed -n '726,800p' src/graphics/_Vulkan/vk3dInitScene.cpp

# SSAO 现状（无 C++ 引用）
cat assets/shaders/gn3d_ssao.frag
grep -rn "gn3d_ssao" src

# 延迟 pass 结束点
sed -n '505,525p' src/graphics/_Vulkan/vk3dPasses.cpp

# compute / 光追
grep -rn "ePIPE_COMPUTE\|vkCmdDispatch" src
grep -rni "ray_query\|accelerationstructure" src
```
