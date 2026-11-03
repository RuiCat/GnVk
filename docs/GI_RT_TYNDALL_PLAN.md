# 真实全局光照 / 光线追踪实现分析 + 丁达尔光照规划

> 性质：**源码实现分析 + 落地方案**。本轮只新增本文档（并在 `LIGHTING_PLAN.md` §0.3 加一行引用），**不改任何源码**。
> 基线：`HEAD = aaf73ef`（2026-11-01）；本机实测 `make unit` = **396 cases / 0 failed**。
> 本机（沙箱）无 `/dev/dri`：`make gpuCheck` 与一切视觉/性能结论**未执行**，文中运行时判断均标「未验证」。
> 关联：[GI_PLAN.md](GI_PLAN.md)（G0–G5 冻结契约）、[LIGHTING_PLAN.md](LIGHTING_PLAN.md)（L0–L8 路线图）、
> [papers/README.md](papers/README.md)（论文库与校验口径）、[L5_CLOUD_CONTRACT.md](L5_CLOUD_CONTRACT.md)（云块契约）。

---

## 0. 结论先行

1. **现状不是「真实 GI / 光线追踪」**：全仓无 `VK_KHR_ray*`、无加速结构；GI 链是
   split-sum IBL + SSAO + 屏幕空间 SSGI + 无遮挡 SH9 探针。按本文 §1 的评级约 **L2（屏幕空间一次弹射近似）**。
2. **SSGI 不是「一次弹射 GI」而是「环境色屏幕空间渗色」**：命中点的辐射写成
   `albedo(hit) × evalSH9(normal(hit))`（`gn3d_ssgi.frag:109`），**不含直接光、自发光、环境遮挡与多次弹射**；
   未命中直接回退环境 SH9（`gn3d_ssgi.frag:92`）。它搬运的是「环境辐照度」，不是场景光。
3. **探针不生产 GI**：256 个探针初值全部来自环境 SH9，逐帧只向环境 SH9 混合（`vkProbe.cpp:197-201,260-269`），
   没有遮挡 raycast，因此它只是「把环境光放大到屏幕外」，不构成空间传输。
4. **软件光线路线可行，但缺三件基础设施**：①场景求交（现有 `cBVH` 只有 AABB 盒、只服务剔除，未接 GI）；
   ②时域累积（无 velocity/TAA、单帧历史，只有 5×5 单帧双边）；③真正消费 GI 的应用（唯一查看器走前向，SSAO/SSGI/探针只在
   `gpuCheck` 里有调用点，`gpuCheck3D.cpp:312-316`）。
5. **丁达尔（云隙光）当前不存在**：现有空气透视（`gn3d_deferred_light.frag:199-249`）只积分行星大气的单次散射，
   太阳可见性只查透射率 LUT，**不查云、不查地形**；云块是独立的盒内 raymarch（`gn3d_cloud_cube.frag`），
   不写深度、不投影云影（`vkCloudCube.cpp:67-86`）。所以没有「云缝里漏下来的光柱」。
6. **丁达尔的两个结构阻塞必须先解决**：
   - 前向路径的深度**不可采样**：深度附件 usage 只有 `DEPTH_STENCIL_ATTACHMENT`（`vk3dInit.cpp:164`），
     `storeOp = DONT_CARE`（`:52`）、`finalLayout = DEPTH_STENCIL_ATTACHMENT_OPTIMAL`（`:54`），
     而唯一应用走前向（`mmdViewer.cpp:479`）。光束要做世界空间 raymarch，就必须先补深度 resolve / 线性深度输出。
   - `GN_VkEnd3DPass` 在结束 3D pass 后**自动合成**（`vk3dForward.cpp:113-123`），光束 pass 必须插在它之前
     （或在 `End3DPass` 内部、合成调用之前）。
7. **太阳方向有两套来源且数值不一致**：查看器方向光用 `kSunDirection = (-0.35,-0.85,0.4)`（`mmdViewer.cpp:52,259`），
   天气/云/大气用 `SUN_DIRECTION = (0.45,0.78,0.44)`（`weather.cpp:20,183`）。二者即使按方向光取反也不相等
   （`0.35,0.85,-0.4` ≠ `0.45,0.78,0.44`）。**做丁达尔之前必须先统一太阳方向**，否则光束、太阳盘、直接光会错位。
8. **推荐顺序**：先做「结构收口 + 丁达尔 T0/T1」（改动小、观感收益直接、复用现有 CSM/云数据），
   再做「SSGI 正确化 → 场景求交 → DDGI → 参考路径追踪验证」。硬件 RT 维持 `GI_PLAN §3-D` 的「远期不做」。

---

## 1. 范围与「真实」判定口径

### 1.1 本文的 GI / RT 定义

- **GI（全局光照）**：解渲染方程中的**间接项**
  `L_o(x,ω_o) = L_e + ∫_Ω f_r(x,ω_i,ω_o) L_i(x,ω_i) cosθ_i dω_i`，
  其中 `L_i` 至少包含一次以上的场景弹射，而不是常数环境光。
- **光线追踪（本文口径）**：任意 **ray–scene 求交**（硬件加速结构，或软件 BVH / SDF / voxel），
  用于可见性、探针更新、反射或多弹射。**屏幕空间 ray march 不算本文的「光线追踪」**，它是屏幕空间近似。
- **真实度评级**（用于本仓库，不追求学术严格）：

| 级别 | 判据 | 本仓库对应 |
|---|---|---|
| L0 | 常数环境光，无方向性、无遮挡 | 接线前的 `ambientLight` |
| L1 | 环境 IBL：单弹射、有方向性，但无场景可见性 | 当前 split-sum IBL |
| L2 | 屏幕空间可见性/一次弹射（SSAO/SSGI） | **当前整体水平** |
| L3 | 带遮挡的探针场 / 多弹射（DDGI）、或软件 ray 求交 | 本文目标 |
| L4 | 路径追踪参考帧 / 硬件 RT | 仅作验证基准 |

### 1.2 本文不做什么

- 不改任何源码；不新增 API；不冻结新契约（所有 UBO/资源布局在实现阶段另行冻结）。
- 不做硬件光追落地（`LIGHTING_PLAN §0.2` 已判远期），只保留「以后可替换的 ray 后端」接口设计。
- 不做离线烘焙 lightmap（场景运行时加载、天气动态，`GI_PLAN §0`）。

---

## 2. 现状（源码证据）

### 2.1 能力总表

| # | 能力 | 现状 | 证据锚点 | 真实度 |
|---|---|---|---|---|
| 1 | 环境 IBL | GPU 6 面烘焙 128² → 回读 → CPU GGX 预滤波（5 mip）+ SH9 + BRDF LUT；默认关（`indirect=0`） | `vkEnvironment.cpp:18,389,394,539`；`vkGi.cpp:14-24` | L1 |
| 2 | SSAO | 延迟路径专用；R8 raw + 4×4 双边；`indirect.w=1` 才跑；默认关 | `vkSsao.cpp`（420 行）；`vk3dPasses.cpp:380-391` | 可见性近似 |
| 3 | SSGI | 半分辨率 8 rays × 8 steps + 5×5 单帧双边；命中=albedo×SH9；未命中=SH9；默认 `envParams.w=0` | `gn3d_ssgi.frag:80-116`；`gn3d_ssgi_denoise.frag:20-34`；`vkSsgi.cpp:445`；`vk3dPasses.cpp:395-406` | L2 |
| 4 | SH9 探针 | 9 张 3D 纹理 8×4×8；初值=环境 SH9；每帧最多 2 个向环境 SH9 混合 0.15；无遮挡；默认 weight=0 | `vkProbe.cpp:197-201,256-269` | 不构成 GI |
| 5 | 延迟合成 | env/SSGI/probe 三选一混合；SSGI 与 SH9 同尺度（除以 π） | `gn3d_deferred_light.frag:504-542` | — |
| 6 | 阴影 | 4 级 CSM（2048²×4）+ PCSS；方向光 | `GN_3Dlight.h:45-56`；`gn3d_deferred_light.frag:266-342`；`vk3dPasses.cpp:24-129` | 直接光可见性 |
| 7 | 大气 | 透射率 LUT + sky-view LUT（compute）+ 8 步单次散射空气透视 | `vkAtmosphereLut.cpp:524`；`gn3d_deferred_light.frag:199-249` | 单次散射 |
| 8 | 云 | 盒内 raymarch（20 步，云盖 16 步）；太阳方向 1 次密度采样；预乘 alpha、深度写关 | `gn3d_cloud_cube.frag:29-33,124-161`；`vkCloudCube.cpp:67-86` | 自阴影近似 |
| 9 | 时域 | 无 TAA、无 velocity 附件；只有单帧双边 | `gn3d_ssgi_denoise.frag`；全仓无 TAA/velocity 资源 | 缺失 |
| 10 | 计算管线 | `vkCmdDispatch` 已有 4 处调用（粒子/布料/大气 LUT） | `vkParticleCompute.cpp:312`；`vkClothCompute.cpp:306,319`；`vkAtmosphereLut.cpp:524` | 可用 |
| 11 | 硬件光追 | 无 `VK_KHR_ray*`、无加速结构 | `grep -rniE 'ray_query|accelerationstructure|raytracing' src` → 无输出 | 无 |
| 12 | 场景求交 | CPU 静态 AABB BVH，支持射线查询，但**只有盒**、未接 GI | `GN_SpatialBVH.cpp:161`；`GN_Spatial.cpp:272` | 未接线 |
| 13 | 应用消费 | 唯一查看器走前向 PBR，只开 GI IBL；SSAO/SSGI/探针只在 gpuCheck 调用且默认关 | `mmdViewer.cpp:409-411,479-489`；`gpuCheck3D.cpp:312-316` | 「做了看不到」 |
| 14 | 帧并发 | 只有一条 `inFlightFence`；时域历史必须双缓冲 | `GN_Vk.cpp:478` | 约束 |

### 2.2 SSGI 的真实度拆解（核心）

`assets/shaders/gn3d_ssgi.frag`：

- **采样**：余弦加权半球，8 rays × 8 steps；固定像素哈希抖动，无蓝噪声、无时域（`:69-93`）。
- **命中**：`hit = hitAlbedo * evalSH9(hitNormal)`（`:109`）。这里 `evalSH9` 返回的是**辐照度**
  （`A_l = π, 2π/3, π/4`，见 `:46-52`），不是入射辐亮度，也没有直接光/阴影。
- **未命中**：`hit = environment = evalSH9(N)`（`:92,83`）。
- **合成**：延迟端 `ssgiDiffuse = kD * albedo * ssgiRadiance / π * indirect.x * ao`（`gn3d_deferred_light.frag:531-534`）。

由此有三个可判定的问题：

1. **没有直接光**：命中面只乘环境 SH9，太阳/阴影像素对渗色无贡献；因此「红墙把光染到白墙」只能来自环境色，不是场景光。
2. **量纲一致（已核对）**：`hit = albedo_h × E_h` 存的是「辐照度等价值」（= π × 辐亮度）；
   8 条余弦加权射线的 `1/N Σ` 估计的正是 `∫ L_h cos dω`，延迟端再乘 `kD·albedo/π` 还原 Lambert BRDF
   （`gn3d_deferred_light.frag:529-534`）。所以这里**不是**量纲错误；真正缺的是命中辐射里没有直接光/自发光。
3. **不是多弹射**：命中后不再继续追踪；屏幕外只有环境 SH9 回退，没有探针/场景信息。

### 2.3 探针的真实度拆解

- 初始化：`SetProbeSh(i, sh9)`，256 个探针全部等于当前环境 SH9（`vkProbe.cpp:197-201`）。
- 更新：`BlendProbeSh(index, sh9, 0.15)`，每帧最多 2 个，目标仍是环境 SH9（`:260-269`）。
- 结论：探针场在数值上 ≈ 环境光场；没有遮挡、没有场景辐射、没有重定位。**开 weight 只会让屏幕外变亮/变平**，
  不会出现「角落变暗、彩色渗色、遮挡处变暗」的真实 GI 行为。

### 2.4 为什么「做了看不到」

- 查看器：`GN_VkEnableGI(true)`（`mmdViewer.cpp:411`）只开 IBL 倍率；SSAO/SSGI/探针依赖 G-Buffer，
  查看器没有 G-Buffer（`mmdViewer.cpp:409-410` 注释明确）。
- 延迟路径只有 `gpuCheck3D.cpp:307-317` 使用；`GN_VkSSAOPass()`/`GN_VkSSGIPass()` 默认直接返回
  （`vk3dPasses.cpp:382,397`），所以「编译级完成」≠「画面可见」≠「效果正确」。

---

## 3. 真实 GI 的软件光线路线

### 3.1 总原则

1. **先让 GI 可见、可标定，再加算法**：否则每加一个 pass 都无法判断是变好还是变亮。
2. **先屏幕空间正确化，再场景求交**：SSGI 的命中着色修对（直接光/材质）成本低；DDGI 需要 BVH/探针/时域，是第二步。
3. **验证优先于观感**：用「furnace test + 离线路径追踪参考帧」判定能量守恒与误差，不靠「看起来像」。
4. **所有新能力默认关、可回退**：沿用 `LIGHTING_PLAN §8.2` 的 DoD。

### 3.2 S0：结构收口（前置，必须先做）

| 项 | 动作 | 验收 |
|---|---|---|
| S0-1 路径归属 | 二选一：①把查看器不透明部分转延迟（复用现有 G-Buffer/SSAO/SSGI/探针）；②给前向补最小 G-Buffer（深度+法线+albedo）。**推荐 ①**，但需先核对 MMD 材质（描边/透明/卡通） | 延迟帧在查看器可见；`gpuCheck` 有像素断言 |
| S0-2 开关与标定 | 用 `GN_VkEnableGI/SetSsaoEnabled/SetSsgiStrength/SetProbeWeight` 组成「关/开」基线；固定相机、天气、分辨率记录像素值 | 开关前后差异可复算（截图 + 像素值落 `.local/`） |
| S0-3 能量校验 | 用 furnace test 校验 IBL/SSGI/探针的能量守恒；若偏亮，先查多路径叠加，而不是量纲 | 均匀环境 + 白 Lambert 闭合场景，像素值 = 环境辐亮度（±1e-3） |
| S0-4 时域前置 | 加 `RG16F` velocity + 历史双缓冲（或先只做 2–3 帧 SSGI 累积）；注意单 `inFlightFence` | 静止场景噪点下降且无拖影；动态场景清单 |
| S0-5 断言 | `gpuCheck` 增加「SSAO/SSGI 开关前后中心像素差」用例 | 断言可判定，不靠目视 |

> S0-1 是最大的结构决策：不解决它，后面所有 GI 都只是 `gpuCheck` 里的数字。

### 3.3 S1：SSGI 正确化（屏幕空间，成本最低）

目标：把「环境色渗色」变成「屏幕空间一次弹射」。

| 改动 | 做法 | 代价 |
|---|---|---|
| 命中直接光 | 命中点用 `LightUBO` 算一次太阳/主光贡献 + CSM 1 tap 阴影；或先渲一张「直接光缓冲」再给 SSGI 采样 | 8 rays × 1 shadow tap；或 1 张全屏 RGBA16F |
| 命中间接 | 命中点再取一次探针 SH9（多弹射近似），没有探针则退回环境 SH9 | 9 次 3D 采样/命中 |
| 命中材质 | 只取 albedo 不够；至少用 `kD*albedo/π` 与法线做 Lambert，金属/高光交给 SSR | 低 |
| 去噪 | 蓝噪声（IGN/R2）+ 2–3 帧累积 + 5×5 双边；无 velocity 时先做「相机静止才累积」 | 中 |
| 回退 | miss 仍回退环境 SH9；屏幕边缘淡出到探针/环境 | 低 |
| 质量档 | 4×8 / 8×8 / 8×12，半分辨率 | 预算可调 |

**验收**：固定场景（红墙/白墙/绿地板）颜色渗色方向正确；关闭 SSGI 回到 S0 基线；
furnace test 误差 < 1%；与离线参考帧的相对误差比当前下降（指标见 S5）。

### 3.4 S2：场景求交（软件 ray 的基础设施）

**推荐 A：AABB 代理 BVH → GPU SSBO → compute 遍历。**

- 复用现有 `cBVH`（中位数切分、叶子 4 盒、`Raycast`，`GN_SpatialBVH.cpp`），把节点/实例上传 SSBO；
  compute 里做 ray-AABB slab 测试 + 栈遍历，返回最近实例与 `t`。
- 动态物体每帧 refit 或重建（256–1024 盒量级，CPU 成本可接受）；静态物体可预先构建。
- 精度：AABB 代理会让探针「漏光/过暗」，但对漫反射 GI 可接受；需要更准时再换三角形 BVH。

**备选 B：三角形 BVH**（静态场景精确，动态物体仍用 AABB 代理）；**备选 C：SDF clipmap**
（动态友好、显存高；未取原文，暂不推荐）。

**数据布局（建议，实现时冻结）**：

```text
节点 32 B：vec4 aabbMin(w=leftOrOffset) + vec4 aabbMax(w=count/leafFlag)
实例 48 B：mat4? 不，用 vec3 aabbMin + vec3 aabbMax + uint materialId + uint pad
```

**验收**：CPU/GPU 对同一组随机射线命中结果一致（实例 id + t 误差 < 1e-4）；单元测试覆盖空场景/单盒/重叠盒/射线起点在盒内。

### 3.5 S3：DDGI（带遮挡的探针场）

在现有 8×4×8、间距 2 m 的探针网格上升级（`GN_3Dprobe` 已有网格/三线性/Recenter，`vkProbe.cpp` 已有 9 张 3D 纹理）：

| 项 | 建议 |
|---|---|
| 数据 | 辐照度：八面体 8×8 RGBA16F；深度矩：16×16 RG16F（或 8×8 先跑通） |
| 更新 | compute，每帧 2–4 个探针；每探针 64–128 rays、最多 2 次弹射；太阳/天空作为 miss |
| 时域 | hysteresis 0.9（可调）；探针逐帧轮转；无 TAA 时靠 hysteresis 稳定 |
| 重定位 | v1 不做（`GI_PLAN §7.1` 已冻结）；后续按深度/几何偏移 |
| 合成 | 屏幕内 SSGI 有效 → SSGI；miss/屏幕外 → 探针；`weight` 平滑过渡 |
| 回退 | weight=0 时逐像素等同现状（已有保证） |

**验收**：屏幕外遮挡场景（墙后/箱内）间接光变暗且染色正确；探针边界无接缝；关闭回退到 SH9 探针；
与离线参考帧的相对误差低于 S1；780M 上帧时间有基线（未实测前不写死毫秒）。

### 3.6 S4：多弹射与镜面

- 多弹射：把探针辐照度喂给 SSGI 命中点（S1 已预留），形成「屏幕内 1 次 + 探针 1 次」的近似两次弹射。
- 镜面 GI：SSR（半分辨率 + 粗糙度感知 + IBL 回退，`LIGHTING_PLAN §4.9`）；
  反射 miss 回退预滤波 cubemap + 探针，不做「屏幕外真实反射」。
- 能量：每加一条路径都要过 furnace test；不允许「多一条路径就整体变亮」。

### 3.7 S5：验证基准（这一步决定「真实」是否可证）

> 载体规划见 [CORNELLBOX_PLAN.md](CORNELLBOX_PLAN.md)：独立 `make cornellBox`，双模式（engine 实时 / PT 参考），
> 自由相机 + GI 开关 + 固定 canonical 位姿 + headless 断言；本文只定义「要验什么」，该文定义「怎么搭」。

1. **Furnace test（CPU/GPU 均可）**：均匀环境 + 白 Lambert 闭合几何，出射辐亮度应等于环境辐亮度；
   验证 IBL/SSGI/探针/多弹射的能量守恒。
2. **离线路径追踪参考帧**：简单 Cornell box（红/绿墙 + 白地板 + 面光），CPU 暴力三角形求交 + 64–256 spp；
   输出 EXR/PNG 到 `.local/`（过程产物，不入库）；与实时 GI 比相对误差/结构相似度。
3. **A/B 断言**：`gpuCheck` 读回中心像素，比较「GI 关/开」「SSAO 关/开」「探针 0/0.5」，阈值写进用例。
4. **不入库**：参考帧与截图是过程产物，按 `data-placement` 放 `.local/`。

### 3.8 S6：硬件光追（远期，不在本轮）

- 保留 `rayScene` 抽象：DDGI/SSGI/SSR 只依赖「trace(origin, dir) → hit」接口；
  以后可把 AABB BVH 换成 `VK_KHR_acceleration_structure` + ray query，而不改上层。
- 硬件 RT 的额外工作：AS 构建/更新、BLAS/TLAS、descriptor、驱动门槛、降噪；`GI_PLAN §3-D` 已判远期。

### 3.9 成本与风险（GI）

| 项 | 风险 |
|---|---|
| 场景求交 | AABB 代理漏光；三角形 BVH 构建/更新成本；动态物体 |
| DDGI | compute 与图形队列同步；探针漏光/接缝；无 TAA 时闪烁 |
| 时域 | 单 `inFlightFence` 下历史覆盖；resize 重建历史；动态物体鬼影 |
| 能量 | 多路径叠加变亮（SSGI + 探针 + IBL 同时开）；每条路径都要过 furnace test |
| 应用 | 查看器前向 vs 延迟的路径归属（S0-1） |

---

## 4. 丁达尔光照（Tyndall / 云隙光）规划

### 4.1 物理模型与术语

「丁达尔效应」在图形学里通常指**参与介质中的单次散射可见光柱**（god rays / crepuscular rays），
工程上按 Mie 散射 + Henyey-Greenstein 相位近似，不逐波长做胶体散射。沿视线积分：

```text
L_s(x, ω) = ∫_0^{t_max} T_view(0,s) · T_sun(s) · σ_s(s) · p_HG(θ) · L_sun ds
T_view(0,s) = exp(-∫_0^s σ_t(u) du)
p_HG(θ) = (1 - g²) / (4π (1 + g² - 2g·cosθ)^{3/2})
```

- `σ_s/σ_t`：散射/消光系数，来自高度雾 + 天气（雨/雪/阴天可调）。
- `T_sun(s)`：该点沿太阳方向的透射率，**必须包含云与地形遮挡**；这是「云隙光」的关键。
- `p_HG`：气溶胶 `g≈0.6–0.8`；现有大气已用 `g=0.8`（`gn3d_deferred_light.frag:164`）。
- 与现有空气透视的区别：现有 AP 只对**行星大气**做 8 步单次散射，太阳可见性只查透射率 LUT
  （`gn3d_deferred_light.frag:188-238`），**不含云/地形阴影、不含近场雾**；它解决「远景发蓝」，
  不解决「云缝光柱」。

### 4.2 现状与缺口（证据）

| 缺口 | 证据 | 影响 |
|---|---|---|
| 没有光束 pass | 全仓无 `god ray`/`light shaft`/`tyndall` 命中 | 无效果 |
| 云不投影 | 查看器阴影 pass 只提交 `gModel.DrawShadow()`（`mmdViewer.cpp:471-477`）；云块不在其中 | 地面/空中没有云影 |
| 云不写深度 | `desc.aDepthWrite = false`（`vkCloudCube.cpp:68`） | 后处理拿不到云的距离 |
| 云只做一次太阳采样 | `gn3d_cloud_cube.frag:137-138`（`SUN_STEP=160 m`） | 只有云体自阴影近似，没有「云影贴图」 |
| 前向深度不可采样 | `vk3dInit.cpp:164`（usage 仅 attachment）、`:52`（DONT_CARE）、`:54`（finalLayout 非 SAMPLED） | 世界空间光束无法从深度重建 |
| 合成点固定 | `GN_VkEnd3DPass` 自动 `GN_VkCompositeToSwapchain`（`vk3dForward.cpp:113-123`） | 光束 pass 没有插入点 |
| 太阳方向不一致 | `mmdViewer.cpp:52` vs `weather.cpp:20` | 光束/太阳盘/直接光错位 |
| 描述符已满 | 延迟 0..25（`gn3d_deferred_light.frag:65-96`） | 新 pass 必须独立 descriptor set |

### 4.3 方案对比

| 方案 | 做法 | 优点 | 缺点 | 结论 |
|---|---|---|---|---|
| **A. 屏幕空间径向模糊** | 太阳屏幕位置 + 遮挡 mask + 径向 blur（Mitchell 2007, GPU Gems 3 ch13） | 最便宜；无深度也能做；适合太阳在屏内 | 无世界空间遮挡；太阳出屏失效；光柱不随雾/云物理变化 | 低配备选/首版 |
| **B. 半分辨率光束 raymarch** | 从相机沿视线 16–32 步，采高度雾密度 × `T_sun` × HG，累加 `(S,T)`，深度感知升采样 | 物理正确、可控质量 | 需要深度；需要 `T_sun`；半分辨率伪影 | **推荐主干** |
| **C. 云透射率阴影图** | 从太阳正交投影渲染云块，输出 `T_cloud`（R16F，512²/1024²） | 真正「云隙」；可复用云密度/实例；还能给地面云影 | 需要额外 pass/实例数据；与云 shader 共享密度函数 | **推荐与 B 配套** |
| **D. Froxel 体积雾** | Wronski 2014，3D 体积雾 + 光束统一 | 最统一、可做动态光 | 成本/复杂度最高；需要 3D 纹理与时域 | 放 L8，后续 |

**推荐：B + C**（半分辨率光束 + 云透射率图）；A 作为「无深度/低配」回退；D 后续统一。

### 4.4 推荐架构

#### 4.4.1 Pass 顺序

```text
[cloud shadow pass]        // 太阳正交，R16F T_cloud；可在 3D pass 之前（或上一帧缓存）
[3D pass]                  // 天空 → 云块 → 不透明 → 透明；现有顺序不变
[depth resolve]            // 仅当前向路径需要：MSAA D32 → 单采样 D32（或 R32F 线性深度）
[light shaft pass]         // 半分辨率 RGBA16F：输出 (inscatter S, transmittance T)
[composite]                // hdr = hdr * T + S → 曝光/tonemap/Bloom/FXAA
```

- 合成必须在**线性 HDR、tonemap 之前**：`hdr = hdr * T + S`（预乘），不能加在 tonemap 之后。
- 云已经在 3D pass 里预乘合成；光束的 `T` 是「相机到场景表面之间介质」的透射率，
  对云像素（不写深度）会有近似：建议用云 mask 或 `T_cloud` 限制 `t_max`，并在文档里登记为近似。

#### 4.4.2 资源与 UBO（建议，实现时冻结）

| 资源 | 格式/尺寸 | 说明 |
|---|---|---|
| `cloudShadow` | R16F 512²/1024² | 太阳方向云透射率；`1` = 全透、`0` = 全遮 |
| `shaftColor` | RGBA16F 半分辨率 | `rgb=S`、`a=T` |
| `sceneDepth` | D32 单采样 或 R32F 线性深度 | 前向路径需 resolve/额外输出 |
| `blueNoise` | RG8 64²（可选） | 抖动，避免带状 |
| `sSHAFT_UBO` | 建议 128–160 B，`static_assert` 偏移 | viewProj/invViewProj、相机、太阳方向/强度、HG `g`、雾密度/高度衰减、`tMax`、步数、云影矩阵、开关 |

#### 4.4.3 云透射率图（C）的两种实现

1. **复用云片元 shader（推荐先试）**：用太阳正交 `viewProj` 渲染云块，片元沿太阳方向积分同一密度函数，
   输出 `T`；混合用**乘法**（`srcFactor=DST_COLOR, dstFactor=ZERO`），与绘制顺序无关。
   需要给云 shader 加「正交模式」：射线起点由 `invViewProj` 重建，而不是 `u.aCamera`。
2. **compute 版**：512² 线程，每 texel 沿太阳方向 8–16 步采样云密度（盒/云盖），输出 `T`。
   需要把密度函数共享给 compute；当前 `make shaders` 逐文件调用 `glslangValidator -V`、**未显式传 `-I`**
   （`Makefile:304-313`）。glslang 支持 `#include` 且优先搜索 includer 目录，同目录 include 理论可用，
   但**未实测**；不成立时用构建期拼接或复制并加一致性测试。

云影图更新频率：云移动慢，可每 2–4 帧更新；或只在太阳/云参数变化时更新（dirty 标记，类似 `recordAtmosphereLutUpdate`）。

#### 4.4.4 光束 shader 伪代码

```glsl
vec3 P = worldFromDepth(depth, uv);
float tMax = min(length(P - cam), SHAFT_MAX_DIST);
float stepLen = tMax / float(STEPS);
vec3 S = vec3(0); float T = 1.0;
for (int i = 0; i < STEPS; ++i) {
    vec3 q = cam + viewDir * ((i + 0.5) * stepLen);
    float density = heightFogDensity(q.y) * weatherScale;
    float sunT = texture(cloudShadow, projectSun(q)).r * csmShadow(q); // 1 tap + 可选 PCSS
    float phase = hg(dot(viewDir, sunDir), g);
    vec3 inscatter = sunColor * sunT * phase * density * stepLen;
    S += T * inscatter;
    T *= exp(-extinction * density * stepLen);
}
outColor = vec4(S, T);
```

- `csmShadow` 可选：地形遮挡会形成「山影光柱」；成本 1 tap，建议开。
- 相位：太阳在视线方向时最强；`g` 可调（0.6–0.8）。
- 抖动：蓝噪声偏移首步，半分辨率 + 深度感知升采样；无 TAA 时用 4×4 固定模式，避免逐帧沸腾。

#### 4.4.5 质量档

| 档 | 分辨率 | 步数 | 云影 | 说明 |
|---|---|---|---|---|
| 低 | 1/4 | 16 | 512²，每 4 帧 | 780M 兜底 |
| 中 | 1/2 | 24 | 512²，每 2 帧 | 默认 |
| 高 | 1/2 | 32 | 1024²，每帧 | 桌面 |

> 毫秒数未实测，禁止在文档里写死；实现后按 `LIGHTING_PLAN §8.3` 记录基线。

### 4.5 与现有代码的接点 / 阻塞

| 接点 | 现状 | 需要的改动 | 风险 |
|---|---|---|---|
| 前向深度 | 不可采样（`vk3dInit.cpp:164`） | 加 depth resolve（Vulkan 1.2 `depthStencilResolve`）或额外 R32F 线性深度输出 | 驱动/扩展；MSAA 解析 |
| 合成点 | `End3DPass` 自动合成（`vk3dForward.cpp:113-123`） | 在 `vkCmdEndRenderPass` 之后、`GN_VkCompositeToSwapchain` 之前插 `recordLightShaftPass`（开关控制） | 2D 叠层顺序 |
| 云实例 | 应用侧构建，`GN_VkDrawCloudCubes` 在 3D pass 内调用（`mmdViewerWeather.cpp:708-720`） | 新增 `GN_VkCloudShadowPass(params)`（3D pass 之前）或引擎缓存实例、上一帧生成 | 实例数据重复/生命周期 |
| 太阳方向 | 两套常量不一致（§0-7） | 以天气 `sunDirection` 为唯一真源，`cLIGHT` 方向光由它派生（或反之），并加一致性断言 | 影响现有光照观感 |
| 描述符 | 延迟 0..25 满 | 光束独立 descriptor set/layout | 池配额（现 1024） |
| 云 shader 复用 | 逐文件编译、无 include | 构建期拼接或复制 + 一致性测试 | 密度函数漂移 |
| 时域 | 单 fence、无历史 | 光束可先不做时域（固定抖动），后续与 TAA 一起 | 噪点/闪烁 |

### 4.6 分阶段 T0–T3 与验收

| 阶段 | 内容 | 验收（本机可做） | 验收（桌面） |
|---|---|---|---|
| **T0 结构** | 统一太阳方向；前向深度 resolve/线性深度；`End3DPass` 插入点；云影图（先输出调试视图，默认关） | `make`/`make unit` 0 失败；CPU UBO `static_assert` + 打包单测；shader 编译 | 深度/云影调试图正确；关掉开关画面与现状逐像素一致（diff < 1e-3） |
| **T1 屏幕空间光束（A）** | 太阳屏幕位置 + 云 mask + 径向模糊；HDR 线性合成 | UBO/参数单测；`gpuCheck`：阴天光束≈0、云隙>0、开关差异可判定 | 太阳附近可见光束；出屏/遮挡不穿帮；性能基线 |
| **T2 世界空间光束（B+C）** | 半分辨率 raymarch + 云影图 + CSM + HG；深度感知升采样；质量档 | 同上 + CPU 单次散射参考对比（合成雾板，误差 < 2%）；`gpuCheck` 像素断言 | 云隙光柱方向与太阳一致；地形/云遮挡正确；无带状噪点；780M 帧时间记录 |
| **T3 体积雾统一（D，可选）** | Froxel 体积雾 + 光束统一（Wronski 2014） | 按 L8 单独验收 | 与大气/云/光束一致；性能可接受 |

**T1/T2 的「关掉不变」是硬性验收**：默认关时不得改变任何现有像素（沿用 `indirect.w=0` 的回退思路）。

### 4.7 成本估算（粗算，必须实测）

- 1080p 半分辨率 960×540 = 518k 像素；24 步 ≈ 12.4M 次采样/帧；每步 1 次云影 + 1 次 CSM + 1 次雾密度。
- 云影图 512² × 16 步 ≈ 4.2M 次密度评估/帧；每 2 帧更新则摊半。
- 对比现有：SSGI 半分辨率 8×8 ≈ 33M 深度采样/帧（`GI_PLAN §6`），光束更轻。
- 结论：T2 在 780M 上「大概率可跑」，但**未实测**；按质量档降级。

### 4.8 风险

1. **前向深度 resolve 的驱动支持**：RADV 应支持 Vulkan 1.2 depth resolve；若不行，退回「额外线性深度输出」或只在延迟路径做光束。
2. **云 shader 复用导致密度漂移**：正交模式与相机模式必须共享同一密度函数；复制两份必须有对比测试。
3. **合成顺序**：光束必须在云之后、tonemap 之前；加在 tonemap 后会出现「亮部不衰减」的假光。
4. **太阳方向不一致**：不先统一，光束会与太阳盘/阴影方向错位，调参无法收敛。
5. **屏幕空间伪影**：半分辨率升采样在深度边缘会出现光晕；需要深度感知 + 边缘淡出。
6. **天气联动**：阴天/雨天应减弱光束（云更厚、太阳更弱）；参数必须来自 `WeatherOutput`，不能写死。

---

## 5. 总路线图与优先级

| 顺序 | 阶段 | 依赖 | 为什么这个顺序 |
|---|---|---|---|
| 1 | **T0 结构收口 + 太阳统一** | 无 | 同时服务 GI 与丁达尔；不改算法先解决「看不到/插不进去」 |
| 2 | **T1 屏幕空间光束（A）** | T0 | 成本最低、观感收益直接；可作为 T2 的回退 |
| 3 | **T2 世界空间光束（B+C）** | T0/T1、云影图 | 真正「云隙光」；复用现有 CSM/云数据 |
| 4 | **S0 GI 收口 + S1 SSGI 正确化** | T0（深度/路径归属） | 先修命中直接光/材质，再谈 DDGI |
| 5 | **S2 场景求交 + S3 DDGI** | S0/S1、compute | 真实 GI 的核心；需要 BVH/探针/时域 |
| 6 | **S4 多弹射 + SSR** | S3 | 在探针基础上加镜面/多次弹射 |
| 7 | **S5 参考路径追踪** | S1 起持续 | 验证「真实」；越早越好，可与 S2 并行 |
| 8 | **T3 Froxel / S6 硬件 RT** | 全部 | 远期，不在本轮承诺 |

**一句话优先级**：先让画面「有光柱、有正确环境色」，再让 GI「有遮挡、有多次弹射」；
两者共享同一套深度/时域/合成基础设施，先做基础设施。

---

## 6. 验收与复算命令

### 6.1 本轮（只加文档）

```bash
git rev-parse HEAD                 # aaf73ef…
make unit                          # 实测 396 cases, 0 failed
bash ~/.dsh/skills/code-modularity/scripts/lintCodeModularity.sh docs --naming --comments
                                   # 仅文档，预期 0 违规（脚本不存在时跳过并说明）
```

### 6.2 当前状态的复算锚点

```bash
# 无硬件光追
grep -rniE 'ray_query|rayquery|accelerationstructure|raytracing' src

# compute dispatch 已存在
grep -rn 'vkCmdDispatch' src

# SSGI 命中不含直接光（关键行）
grep -n 'hitAlbedo \* evalSH9' assets/shaders/gn3d_ssgi.frag

# 探针初值/更新都是环境 SH9
grep -n 'SetProbeSh\|BlendProbeSh' src/graphics/_Vulkan/vkProbe.cpp

# 前向深度不可采样
grep -n 'VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT\|DONT_CARE\|DEPTH_STENCIL_ATTACHMENT_OPTIMAL' src/graphics/_Vulkan/vk3dInit.cpp

# 自动合成点
grep -n 'GN_VkCompositeToSwapchain' src/graphics/_Vulkan/vk3dForward.cpp

# 太阳方向两套来源
grep -n 'kSunDirection' tests/mmdViewer/mmdViewer.cpp
grep -n 'SUN_DIRECTION' src/graphics/weather.cpp
```

### 6.3 实现阶段（写源码后）

```bash
make
make unit
# shader 严格编译
fail=0
for f in assets/shaders/*.vert assets/shaders/*.frag assets/shaders/*.comp; do
  glslangValidator -V "$f" -o /tmp/gnv.spv || fail=1
done
[ "$fail" -eq 0 ]
# 写域 lint
bash ~/.dsh/skills/code-modularity/scripts/lintCodeModularity.sh <写域> --naming --comments
# 桌面（有 GPU）
make gpuCheck
make mmdViewer
```

---

## 7. 参考

### 7.1 论文 / 资料（库内篇号见 `papers/README.md`）

| 用途 | 出处 |
|---|---|
| IBL split-sum | Karis, *Real Shading in Unreal Engine 4*, SIGGRAPH 2013 Course（库内 #1） |
| SH9 辐照度 | Ramamoorthi & Hanrahan, *An Efficient Representation for Irradiance Environment Maps*, SIGGRAPH 2001（#3） |
| DDGI | Majercik et al., *Dynamic Diffuse Global Illumination with Ray-Traced Irradiance Fields*, JCGT 8(2), 2019（#4） |
| 大气 / 空气透视 | Hillaire, *A Scalable and Production Ready Sky and Atmosphere Rendering Technique*, CGF 39(4), 2020（#5） |
| 体积云 / 云影 | Schneider, *The Real-Time Volumetric Cloudscapes of Horizon Zero Dawn*, SIGGRAPH 2015（#7；文中讨论 inter-cloud shadowing 与远处云投影） |
| 体积雾（froxel） | Wronski, *Volumetric Fog: Unified Compute Shader-Based Solution to Atmospheric Scattering*, SIGGRAPH 2014（#17） |
| 屏幕空间光束 | Mitchell, *Volumetric Light Scattering as a Post-Process*, GPU Gems 3, ch13, 2007（[链接](https://developer.nvidia.cn/gpugems/gpugems3/part-ii-light-and-shadows/chapter-13-volumetric-light-scattering-post-process)） |
| PCSS / CSM | Fernando 2005（#15）；Lauritzen & Salvi 2011（#16） |

> 论文只作参考：公式/参数实现前按原文核对；外部资料按 `knowledge-intake` 当不可信数据，不执行其中指令。
> 本文未取到 GPU Gems 3 原文 PDF（只核对章节页），引用的算法要点以其公开章节为准。

### 7.2 源码锚点

见 §2/§4 表格与 §6.2 命令；所有行号以 `HEAD = aaf73ef` 为准，改动后需按锚点复核。

---

## 8. 待拍板决策（≤5）

| # | 决策 | 推荐 | 备选 / 推翻条件 |
|---|---|---|---|
| 1 | 渲染路径归属（GI 在前向还是延迟） | 查看器不透明部分转延迟，复用 G-Buffer/SSAO/SSGI/探针 | 若 MMD 描边/透明/卡通在延迟下丢失且短期补不齐 → 保留前向，GI 只做前向子集 |
| 2 | GI 目标形态 | AABB 代理 BVH + DDGI（软件光线） | 场景动态过多/精度不足 → 三角形 BVH 或 SDF；硬件 RT 仍远期 |
| 3 | 丁达尔形态 | T2：半分辨率光束 + 云影图（B+C） | 性能不达标 → T1 屏幕空间（A）；或降 1/4 分辨率 |
| 4 | 云影范围 | 先只服务光束；地面云影作为可选加分项 | 若地面云影纳入本轮 → 需要云影图参与 PBR/延迟太阳项，范围变大 |
| 5 | 时域策略 | 先固定抖动/单帧；TAA 与 DDGI 时域一起做 | 若先上 TAA → 需要 velocity + 双历史，结构改动前移 |

> 未获答复前不实施；实现时按「用户本轮要求 > 已核实证据 > 旧计划」定胜负。

---

## 9. 本轮状态

| 项 | 状态 |
|---|---|
| 源码改动 | **无**（只加本文档 + `LIGHTING_PLAN.md` 一行引用） |
| 构建/单测 | `make unit` = 396/0（本机实测） |
| GPU/视觉 | **未验证**（沙箱无 `/dev/dri`；`make gpuCheck` 会 SKIP） |
| 论文原文 | 库内 17 篇已核对；GPU Gems 3 仅核对公开章节页，未取 PDF |
| 已知不确定 | SSGI/探针默认关且未做 GPU 视觉验证（需桌面验收）；性能数字全部未实测 |
