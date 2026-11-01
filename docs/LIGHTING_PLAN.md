# 光照与效果实现规划（论文算法落地）

> 性质：**规划文档**。本轮只落这份文档，不改任何源码；实现按阶段另起任务。
> 基线：`git HEAD = 8c77d7d` + 工作区未提交改动（P12 GI G0–G5 代码级）。文档记录
> `make unit` = 369 cases / 0 failed（[3D_FEATURE_PLAN.md](3D_FEATURE_PLAN.md) §0）。
> 本机（2026-10-31 实测）无 `/dev/dri`：`make gpuCheck` 与一切视觉验收**只能在桌面机**执行。
> 关联：[GI_PLAN.md](GI_PLAN.md)（GI 实现细节与冻结契约，本文只引用不复制）、
> [papers/README.md](papers/README.md)（论文库与校验口径）、[SKY_WEATHER_PLAN.md](SKY_WEATHER_PLAN.md)（P11 天空天气）、
> [3D_FEATURE_PLAN.md](3D_FEATURE_PLAN.md) §3.6/§3.7（P11/P12）。

**结论先行**：引擎现在「零件齐、成片缺」。

- GI 的 G0–G5 已全部代码级落地，但**默认关闭**，且 SSAO/SSGI/探针**只挂在延迟路径**；唯一的应用（MMD 查看器）走前向 PBR，
  因此这些 GI 效果在实际画面里看不到（`tests/mmdViewer/mmdViewer.cpp:306`；延迟路径全仓调用点只有
  `tests/gpu/gpuCheck3D.cpp:307,314`）。
- 后处理只有「5 抽样假 Bloom + shader 内 Reinhard」：PBR 与延迟着色器各自做一次 Reinhard
  （`assets/shaders/gn3d_pbr.frag:337`、`gn3d_deferred_light.frag:382`），HDR 变体再经合成着色器做第二次
  （`gn3d_composite.frag:42-43`）；延迟路径直接写 swapchain（`vk3dInitScene.cpp:736-741`），无法参与 Bloom/曝光。
- 图像质量缺口：延迟 1× 无 AA（`docs/GI_PLAN.md:227`）、阴影单张 2048² 无级联（`GN_Vk.h:253`）、
  天空是解析渐变 + 指数雾（无大气散射）、云是 2D FBM + billboard（无体积感）。

本规划把「光照与效果」拆成 **L0–L8** 九个阶段（`L` 是 Lighting 阶段编号，**不是**任务编排级别 L0–L3），
每阶段独立可验收、默认关闭、可回退；每项能力都对应可核对的论文与代码锚点（§3）。推荐首个可交付：
**L0（收口与标定）+ L1（线性 HDR / 曝光 / 色调映射 / Bloom）**，之后 L3（阴影）观感收益最大，其余按 §4 依赖推进。

## 0. 范围、边界与既有计划的关系

### 0.1 本文的「光照与效果」

- **光照**：直接光与阴影、环境光（IBL/SH9）、间接光（SSAO/SSGI/探针/DDGI）、反射（SSR）。
- **效果**：HDR 曝光与色调映射、Bloom、抗锯齿、天空/大气散射、云、雾/空气透视，以及据此派生的可选后处理。
- **不在本文**：2D UI、粒子/降水/贴花外观（P11 已覆盖）、物理/LOD（P5/P10）、离线烘焙工具链。

### 0.2 不做（写给未来的自己，防范围蔓延）

| 不做 | 理由 |
|---|---|
| 硬件光追（`VK_KHR_ray_query` / 加速结构） | 驱动与硬件门槛，远期再评估（GI_PLAN §3 方案 D 已判远期） |
| DLSS / FSR / 超分 | 不是「论文算法落地」的核心缺口；先做原生分辨率质量 |
| 烘焙 lightmap | 模型运行时加载且天气动态（GI_PLAN §0） |
| 路径追踪离线渲染 | 与本引擎实时目标无关 |
| 运动模糊 / 景深 / 镜头耀斑 | 放 L8「可选效果」，L1–L7 收口前不做 |
| 推翻既有 GI 契约（UBO 布局/槽位） | GI 契约已冻结（GI_PLAN §7.1 §5.4）；新能力用**新 pass 独立描述符集**，不抢 0..23 |

### 0.3 与既有计划的关系（避免两套口径）

| 文档 | 管什么 | 本文与其关系 |
|---|---|---|
| [GI_PLAN.md](GI_PLAN.md) | G0–G5 的实现细节、G-Buffer 契约、槽位表 | 本文**只引用**；L0 负责把它的成果接到画面上 |
| [SKY_WEATHER_PLAN.md](SKY_WEATHER_PLAN.md) | S1–S6 天空/云/雾/降水与天气状态机 | L4/L5 **取代其中的天空与云**，需先解决 §5 决策 3 的冲突 |
| [3D_FEATURE_PLAN.md](3D_FEATURE_PLAN.md) §3.6/§3.7 | P11/P12 阶段与验收 | 本文是 P12 之后的「光照与效果」总入口 |
| [papers/README.md](papers/README.md) | 12 篇论文、校验与摄取口径 | 新文献按同一约定补采（L0e），不散落 `docs/` 根目录 |

## 1. 现状（源码证据）

| # | 能力 | 现状 | 证据锚点 |
|---|---|---|---|
| 1 | 直接光 | 8 光源，GGX/Cook-Torrance，前向 + 延迟两套 | `GN_3Dlight.h:22`；`gn3d_pbr.frag:213-266`；`gn3d_deferred_light.frag:231-290` |
| 2 | 阴影 | 单张 2048² 2D 方向光阴影，16 抽样 Poisson PCF；无级联/无 PCSS/无稳定化 | `GN_Vk.h:253`；`gn3d_pbr.frag:9,186-203`；`gn3d_deferred_light.frag:161-184`；`vk3dPasses.cpp:24-87` |
| 3 | 环境光 IBL | split-sum：预滤波 cubemap（128²，5 mip）+ BRDF LUT + SH9；**默认关** | `vkEnvironment.cpp:16-18,635`；`giParams.h:31-39`；`vkGi.cpp:20-40,83-95` |
| 4 | SSAO | 420 物理行实现 + 双边模糊；只用于延迟路径；`indirect.w=1` 才开 | `vkSsao.cpp`（420 物理行）；`gn3d_deferred_light.frag:316-317` |
| 5 | SSGI | 半分辨率 8 rays×8 steps + 5×5 双边；只用于延迟路径；`envParams.w>0` 才开 | `vkSsgi.cpp`（445 物理行）；`giParams.h:84-101`；`gn3d_deferred_light.frag:369-370` |
| 6 | 探针 | SH9 8×4×8 网格，9 张 3D 纹理；CPU 每帧最多更新 2 个、无遮挡；`weight` 默认 0 | `vkProbe.cpp`（308 物理行，272 有效行）；`gn3d_deferred_light.frag:78-86`；`giParams.h:107` |
| 7 | 天空 | 解析渐变 + 太阳 + FBM 云（混合模式） | `gn3d_skybox.frag:12-19,30-45` |
| 8 | 雾 | 距离 × 高度指数雾（无大气散射/无空气透视） | `vkAtmosphere.cpp:1-10`；`gn3d_deferred_light.frag:137-140` |
| 9 | 云 | 天空盒 2D FBM；三层 billboard 云盘（查看器默认关，未解决项 #1） | `gn3d_cloud.frag:1-10`；`3D_FEATURE_PLAN.md:53` |
| 10 | HDR | 有 `R16G16B16A16_SFLOAT` 离屏目标 | `vk3dInitScene.cpp:13-21,55-101` |
| 11 | 色调映射 | Reinhard 在 PBR/延迟 shader 内各一次；HDR 合成再一次（双重） | `gn3d_pbr.frag:337`；`gn3d_deferred_light.frag:382`；`gn3d_composite.frag:42-43`；`3D_FEATURE_PLAN.md:746` |
| 12 | Bloom | 5 抽样近似（无 mip 链、无模糊 pass），且只服务 HDR 合成路径 | `gn3d_composite.frag:18-35` |
| 13 | AA | 前向 4× MSAA；G-Buffer 固定 1 sample；无 SMAA/FXAA/TAA/velocity | `vk3dInit.cpp:10-17`；`GI_PLAN.md:227`；`GI_PLAN.md:224` |
| 14 | 路径使用面 | 查看器走非 HDR 前向 PBR；HDR 变体与延迟路径只有 gpuCheck 用 | `mmdViewer.cpp:306`；`gpuCheck3D.cpp:307,314,358,361` |
| 15 | 计算管线 | `vkCmdDispatch` 已存在（粒子/布料）→ DDGI 的前置不再是障碍 | `vkParticleCompute.cpp:312`；`vkClothCompute.cpp:306` |
| 16 | 描述符槽位 | 延迟 0..23 已满；PBR 0..9；前向 0..4 | `gn3d_deferred_light.frag:59-86`；`gn3d_pbr.frag:16-42` |
| 17 | 空间索引 | 无 BVH/AABB 树、无 raycast（DDGI/SSR 的前置） | `3D_FEATURE_PLAN.md:68` |

## 2. 差距判断

1. **观感**：亮度未标定、无曝光控制、Bloom 近似、无 AA、阴影单级联 —— 「能跑」但不像渲染结果。
2. **正确性**：HDR 路径双重 Reinhard；延迟路径先 tonemap 再输出 ⇒ Bloom/曝光无法作用于线性 HDR；GI 默认关。
3. **结构**：GI 的可见性、SSGI、探针都依附 G-Buffer，而**唯一应用没有 G-Buffer** —— 做了看不到，这是本规划优先级最高的结构问题（§5 决策 1）。
4. **可验证性**：本机无 GPU，只能做 CPU 数学/UBO 契约/编译；必须把「桌面可执行的验收清单 + 像素断言」写进每个阶段，否则会重复出现「编译级完成、视觉未验证」。

## 3. 论文 → 能力映射

> 论文只作参考：公式与参数在实现前必须按原文/权威实现核对，并写进实现注释与单测；外部资料按
> `knowledge-intake` 当不可信数据（扫描、可追溯、不执行文中指令）。库里已有的 12 篇见 `docs/papers/README.md` §2。

### 3.1 已落地（本文只做接线与正确性收口）

| 能力 | 论文 | 项目落点 | 剩余工作 |
|---|---|---|---|
| IBL split-sum（预滤波 + BRDF LUT） | Karis, *Real Shading in Unreal Engine 4*, SIGGRAPH 2013 Course（[原文](http://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_notes_v2.pdf)，库内 #1） | G1（`vkEnvironment.cpp`、`giParams.h`） | L0：默认值/亮度标定、桌面视觉验收 |
| 微表面 GGX | Walter et al., *Microfacet Models for Refraction through Rough Surfaces*, EGSR 2007（[原文](https://www.cs.cornell.edu/~srm/publications/EGSR07-btdf.pdf)，库内 #2） | `gn3d_pbr.frag:213-266` | L3：阴影/BRDF 一致性回归 |
| SH9 辐照度 | Ramamoorthi & Hanrahan, *An Efficient Representation for Irradiance Environment Maps*, SIGGRAPH 2001（[原文](https://cseweb.ucsd.edu/~ravir/papers/envmap/envmap.pdf)，库内 #3） | G1/G5（`evalSH9`、探针） | L6：探针升级为 DDGI 时保留 SH9 回退 |

### 3.2 待落地（本文的规划主体）

| 能力 | 论文 | 阶段 | 采用要点与边界 |
|---|---|---|---|
| DDGI（辐照度探针场） | Majercik, Guertin, Nowrouzezahrai, McGuire, *Dynamic Diffuse Global Illumination with Ray-Traced Irradiance Fields*, JCGT 8(2), 2019（[原文](https://jcgt.org/published/0008/02/01/)，库内 #4） | L6 | 八面体辐照度 + 深度矩、compute 更新、时间滞后；v1 不做探针重定位；需要场景求交（§5 决策 5） |
| 大气散射 | Hillaire, *A Scalable and Production Ready Sky and Atmosphere Rendering Technique*, CGF 39(4)/EGSR 2020（[原文](https://sebh.github.io/publications/egsr2020.pdf)，库内 #5） | L4 | 透射率 LUT + 多次散射近似 LUT + sky-view LUT + 空气透视；单次散射为主 |
| 晴空模型选型 | Bruneton, *A Qualitative and Quantitative Evaluation of 8 Clear Sky Models*, IEEE TVCG 23(12), 2017（[arXiv:1612.04336](https://arxiv.org/abs/1612.04336)，库内 #6） | L4 | 用于论证「不选 Preetham（低太阳角偏差大）」与 Hillaire 2020 的折中定位 |
| 体积云 | Schneider, *The Real-Time Volumetric Cloudscapes of Horizon Zero Dawn*, SIGGRAPH 2015（[原文](https://www.advances.realtimerendering.com/s2015/The%20Real-time%20Volumetric%20Cloudscapes%20of%20Horizon%20-%20Zero%20Dawn%20-%20ARTR.pdf)，库内 #7） | L5 | weather map + 3D 噪声 + 低分辨率 raymarch + 时域重投影；与 §0.2「不做全分辨率」一致 |
| 梯度噪声 | Perlin, *Improving Noise*, ACM TOG 21(3), 2002（[原文](https://mrl.cs.nyu.edu/~perlin/paper445.pdf)，库内 #8） | L5 | 用梯度噪声替换现有 value noise 作为云的基础噪声 |

### 3.3 本次补查的文献（库外，题录已核对）

> 取原文状态（2026-10-31 更新）：Reinhard / SMAA / PCSS / SDSM / Wronski 已用 `curl` 补采落盘（见 §3.4 与
> `papers/README.md` §2b）；ACES 规范、Jimenez 2014（426 MB PPTX）、Karis TAA（PPTX）、Sikachev SSR（PPTX）仍未取，
> 按出版题录引用，**实现前必须取到原文或权威实现再落参数**。

| 能力 | 论文 | 阶段 | 采用要点与边界 |
|---|---|---|---|
| 色调映射（基线） | Reinhard, Stark, Shirley, Ferwerda, *Photographic Tone Reproduction for Digital Images*, ACM TOG 21(3):267–276, 2002（[DOI 10.1145/566654.566640](https://doi.org/10.1145/566654.566640)） | L1 | 保留为基线/对比开关；全局算子改为**只在合成阶段做一次** |
| 色调映射（默认候选） | ACES 2.0 输出变换 / `aces-core`（[仓库](https://github.com/ampas/aces-dev)，已核实可访问） | L1 | 先用单曲线 fit（per-channel），在 UI 暴露；注意 per-channel 会让高光去饱和 |
| Bloom + 阴影采样 + AA 工程实践 | Jimenez, *Next Generation Post Processing in Call of Duty: Advanced Warfare*, SIGGRAPH 2014 Advances in Real-Time Rendering（[课程页](https://www.advances.realtimerendering.com/s2014/index.html)，已核实） | L1/L2/L3 | Bloom 用 13-tap 下采样 + tent 上采样链；阴影采样稳定性；AA 的工程取舍 |
| TAA | Karis, *High Quality Temporal Supersampling*, SIGGRAPH 2014 Advances in Real-Time Rendering（同[课程页](https://www.advances.realtimerendering.com/s2014/index.html)，已核实） | L2 | jitter + 历史重投影 + 邻域钳制；必须在线性 HDR、双缓冲历史下做 |
| SMAA | Jimenez, Masia, Echevarria, Navarro, Gutierrez, *SMAA: Enhanced Subpixel Morphological Antialiasing*, CGF 31(2):355–364, 2012（[DOI 10.1111/j.1467-8659.2012.03080.x](https://doi.org/10.1111/j.1467-8659.2012.03080.x)） | L2 | 先做 1×（无时域），LDR 后处理；需要深度边缘 |
| PCSS | Fernando, *Percentage-Closer Soft Shadows*, SIGGRAPH 2005 Sketch（[DOI 10.1145/1187112.1187153](https://doi.org/10.1145/1187112.1187153)） | L3 | blocker search + penumbra 估计，接触硬化；采样预算受 780M 限制 |
| 级联/稳定化阴影 | Lauritzen & Salvi, *Sample Distribution Shadow Maps*, I3D 2011（课程版[PDF](https://www.advances.realtimerendering.com/s2010/Lauritzen-SDSM(SIGGRAPH%202010%20Advanced%20RealTime%20Rendering%20Course).pdf)） | L3 | 先做固定分割 2–4 级 CSM，SDSM 作为可选升级 |
| 体积雾 | Wronski, *Volumetric Fog: Unified Compute Shader-Based Solution to Atmospheric Scattering*, SIGGRAPH 2014（同[课程页](https://www.advances.realtimerendering.com/s2014/index.html)，已核实） | L4/L8 | froxel 体积雾与大气散射叠加；L4 先做空气透视，体积雾放 L8 |
| SSR + IBL 混合 | Sikachev & Longchamps, *Reflection System in Thief*, SIGGRAPH 2014（同[课程页](https://www.advances.realtimerendering.com/s2014/index.html)，已核实） | L7 | 屏幕空间反射 + IBL/局部 cubemap 混合、粗糙度感知 |
| FXAA（L2a v1） | Lottes, *FXAA*（NVIDIA 白皮书, 2009；题录：[FXAA WhitePaper](https://developer.download.nvidia.com/assets/gamedev/files/sdk/11/FXAA_WhitePaper.pdf)） | L2 | tonemap 后单 pass 边缘混合；本机未取原文，公式按 FXAA 3.11 公开版本实现并标注来源 |
| SMAA（L2a 目标） | 同 §3.3 SMAA 行（CGF 2012） | L2 | 需 Area/Search LUT 纹理；资产未入库前以 FXAA v1 代替 |

### 3.4 文献入库计划（L0e）

| # | 文件命名（`docs/papers/pdf/`） | 来源 | 状态 |
|---|---|---|---|
| 1 | `reinhard2002-photographic-tonemap.pdf` | web.archive 的 ACM PDF | ✅ 已入库（2026-10-31，10 页） |
| 2 | `aces2-output-transform.pdf` 或官方规范 | ACES / aces-core 文档 | ⬜ 待取（仓库可访问，AB） |
| 3 | `jimenez2014-cod-postprocessing.pdf` | advances 课程页只有 426 MB PPTX | ⬜ 未取（体积过大，暂缓） |
| 4 | `karis2014-taa.pdf` | advances 课程页只有 27 MB PPTX | ⬜ 未取（非 PDF，暂缓） |
| 5 | `jimenez2012-smaa.pdf` | 作者页 PDF | ✅ 已入库（15 页） |
| 6 | `fernando2005-pcss.pdf` | NVIDIA shadowlibrary PDF | ✅ 已入库（8 页） |
| 7 | `lauritzen2011-sdsm.pdf` | advances SIGGRAPH 2010 课程 PDF | ✅ 已入库（33 页） |
| 8 | `wronski2014-volumetric-fog.pdf` | advances 课程 PDF | ✅ 已入库（67 页） |
| 9 | `sikachev2014-ssr.pdf` | advances 课程页只有 46 MB PPTX | ⬜ 未取（暂缓） |

补采流程沿用 `docs/papers/README.md` §5–§6：落盘 → `knowledgeAudit.sh` 扫描 → 更新 §2 表与 `MANIFEST.sha256`；
取不到 PDF 的条目写进 §3「未取到」，**不要**用二手博客替代原始出处。

## 4. 阶段路线图

### 4.1 总表

| 阶段 | 目标 | 依赖 | 主要改动 | 工作量 | 主要论文 |
|---|---|---|---|---|---|
| **L0** | 收口与标定：GI 可见/可开关、修双重 Reinhard、亮度基准、验收扩展、论文补采 | 无 | 接线 + 合成路径 + 测试 | S–M | 已有 G1 三篇 |
| **L1** | 线性 HDR 后处理：统一到线性、单次 tonemap、曝光、ACES、真 Bloom | L0 | 新后处理 pass + 合成重构 | M | Reinhard 2002 / ACES / Jimenez 2014 |
| **L2** | 抗锯齿：SMAA 先行，TAA 随需求 | L1 | 新 pass +（TAA）velocity/历史 | M | SMAA / Karis 2014 |
| **L3** | 阴影：稳定化 → PCSS → CSM | L0 | 阴影 pass 重构 + shader | M | PCSS / SDSM / Jimenez 2014 |
| **L4** | 大气散射：LUT 天空 + 空气透视，替换解析天空/指数雾 | L1 | 新 compute LUT + 天空/雾 shader | L | Hillaire 2020 / Bruneton 2017 |
| **L5** | 体积云：weather map + raymarch + 时域重投影 | L4（TAA 可选） | 新云 pass + 3D 噪声 | L | Schneider 2015 / Perlin 2002 |
| **L6** | DDGI 探针 + 时域降噪 | L1/L2（可选 L3）、场景 BVH | 新 compute + 探针纹理升级 | L | Majercik 2019 |
| **L7** | SSR：屏幕空间反射与 IBL 混合 | L1/L2、G-Buffer | 新半分辨率 pass | M | Sikachev 2014 |
| **L8** | 可选效果：体积雾、DoF、运动模糊等 | L1 | 按需 | S/项 | Wronski 2014 等 |

依赖图（`→` 表示必须先于）：

```text
L0 → L1 → L2（SMAA；TAA）
        → L3（可与 L2 并行）
        → L4 → L5
        → L7
L1 + L2（+L3）→ L6（还依赖场景 BVH）
L1 → L8
```

### 4.2 L0：收口与标定（先做，否则后面全是盲调）

**目标**：把「代码级已完成」变成「画面可见、参数可解释、回归可测」。

- **L0a 开关语义统一**：`sGI_UBO.indirect = (diffuse, specular, aoStrength, ssaoOn)`、`envParams.w`（SSGI 强度）、
  `sPROBE_UBO.weight` 三者写进一份开关表；应用侧提供 `GN_VkSetGI / GN_VkEnableGI / GN_VkSetProbeWeight` 的组合调用；
  **默认全部关闭**（现状），打开顺序与推荐值写进文档与查看器按键。
- **L0b 修双重 Reinhard 与路径归属**：三条渲染路径（前向 / 前向 HDR / 延迟）统一为「着色器输出线性 HDR，
  合成阶段做唯一一次 tonemap」；延迟路径的 render pass 目标从 swapchain 改到 HDR 目标
  （`vk3dInitScene.cpp:736-741`），否则 Bloom/曝光对它无效。此步与 L1a 合并实施，L0 只出契约与回归基线。
- **L0c 亮度基准**：固定场景（同一模型、相机、天气、分辨率 1280×800）+ 三个测点（白炉、0.18 灰卡、金属球），
  记录 `exposure=1`、GI 关/开、SSAO 关/开、探针权重 0/0.5 的像素值；后续每阶段与这份基线对比。
  **不预设目标值**（当前亮度未标定），基线本身就是后续判据。
- **L0d 验收扩展**：`tests/gpu/` 增加「开关前后像素差」断言（SSAO 差异 > 阈值、探针权重差异、IBL 粗糙度
  mip 采样单调、tonemap 单调性），避免每次靠肉眼。
- **L0e 论文补采**：按 §3.4 入库 9 篇（可失败，失败按题录登记）；`knowledgeAudit.sh` 全绿后更新 `MANIFEST.sha256`。

**验收**：`make` / `make unit` 不回归；桌面 `make gpuCheck` 全 PASS 且新增断言 PASS；L0c 基线截图与像素值落 `.local/`；
`cd docs/papers && sha256sum -c MANIFEST.sha256` 全 OK。

**风险**：延迟路径改 HDR 目标会动 `GN_VkDeferredLighting` 的 pass 结束逻辑（`vk3dPasses.cpp:552-560` 已自行结束
G-Buffer pass），必须一次改完并跑验证层；否则 SSAO/SSGI 的读写可见性会回退。

### 4.3 L1：线性 HDR、曝光、色调映射、Bloom

**目标**：建立真正的 HDR 后处理链，后续所有效果（AA/云/DDGI/SSR）都在同一链上。

- **L1a 线性管线**：所有 3D pass 输出线性 HDR（`R16G16B16A16_SFLOAT`）；天空/云/雾/粒子/贴花/降水统一在线性空间混合；
  sRGB 编码只由 swapchain 格式完成；删除 PBR/延迟 shader 内的 Reinhard。
- **L1b 曝光**：手动 EV 参数（`exposure = 2^EV`）+ 可选自动曝光（64×64 亮度下采样 + log-average + 0.1–0.5 s 时间滞后）；
  v1 不做直方图。
- **L1c 色调映射**：Reinhard（基线/对比）+ ACES 单曲线 fit（默认候选），运行期可切换；参数进 UBO（不写死）。
- **L1d Bloom**：mip 链（6 级）13-tap 下采样 + 9-tap tent 上采样（Jimenez 2014），阈值软膝盖、强度/半径可调；
  起点为半分辨率，性能不足再降级。

**验收**：CPU 单测（tonemap 单调、EV 倍率关系、UBO 布局）；gpuCheck 数值断言（线性输入经 tonemap 后仍在 [0,1]；
Bloom 使亮区邻域像素提升且不改变远处暗区）；桌面固定场景对比 Reinhard/ACES 截图 + 高光不剪裁。

**风险**：ACES per-channel 去饱和会改变 MMD 角色观感，必须保留切换开关；自动曝光在天气突变时会「呼吸」，
用时间滞后与上下限钳制。

### 4.4 L2：抗锯齿（SMAA 先行，TAA 后置）

- **L2a AA v1 = FXAA（Lottes 2009，编译级已落地）**：在 `gn3d_composite.frag` 的 tonemap 之后做单 pass
  FXAA 3.11 风格边缘混合（对角 luma 定方向 + 3/4 采样混合），邻域样本只含 HDR×曝光、不含 Bloom；
  `sPOST_UBO.fxaa` 开关（默认 0，查看器 `T` 键切换）。
  **SMAA 1× 未落地**：它需要 SMAA 的 Area/Search LUT 纹理（`docs/papers/` 尚未收录、本机也取不到该资产），
  待 LUT 入库后替换 FXAA（接口仍走 composite 单 pass，便于对照）。
- **L2b TAA（Karis 2014）**：投影 jitter（Halton 2,3）+ `RG16F` velocity 附件（G-Buffer 扩到 5 附件）+ 双缓冲历史 +
  邻域钳制；HDR 域做混合，tonemap 之前。**这是结构性改动**：需要第二份历史资源与 resize 重建，且必须与
  SSGI/云的时域重投影共用同一套重投影矩阵。
- 顺序建议：L2a 先交付；L2b 与 L6 的时域降噪一起做（同一套 velocity/历史机制），避免做两遍。

**验收**：SMAA 边缘像素覆盖率与静态场景差异度量；TAA 动态场景 ghosting/闪烁清单（固定相机移动、天气切换、快速平移）；
桌面逐项目视；`make gpuCheck` 增加开关前后差异断言（能断言的部分）。

**风险**：`GI_PLAN.md:224` 已指出 velocity 缺失且 push constant 无空间 —— 速度附件与模型矩阵上传需要新的 SSBO/UBO
方案；MMD 查看器当前没有 G-Buffer，TAA 只能先在延迟路径验证。

### 4.5 L3：阴影（稳定化 → PCSS → CSM）

- **L3a 稳定化**：光空间 texel snapping、normal offset / slope-scaled bias、阴影图分辨率策略（2048 保留）；
  先解决 acne/peter-panning，再谈软阴影。
- **L3b PCSS（Fernando 2005）**：blocker search（16 taps）+ penumbra 估计；接触处锐、远处软；采样预算先按 16+16 评估。
- **L3c 级联（Lauritzen 2011）**：2–4 级 CSM，固定分割（对数/均匀混合）先行；SDSM（按深度分布自适应）作为可选升级；
  光空间 AABB 由场景/模型 AABB 求（`cMODEL` 已有世界 AABB）。

**验收**：阴影 acne/peter-panning 消失（固定场景近/远两档）；PCSS penumbra 宽度随遮挡距离变化；级联接缝不可见；
`gpuCheck` 阴影用例从「灰阶在 0.3–0.7」升级为「近/远接触硬度差异」断言；桌面目视。

**风险**：级联会增加阴影 pass 次数与描述符/缓冲；阴影 UBO 的更新频率与单帧 in-flight 约束要一起改。

### 4.6 L4：大气散射（Hillaire 2020）

- **L4a LUT**：透射率 LUT（约 256×64）、多次散射近似 LUT（约 32×32）、sky-view LUT（约 192×108），用 compute 生成
  （`vkCmdDispatch` 已可用，`vkParticleCompute.cpp:312`）。
- **L4b 天空**：天空盒 shader 改为采样 sky-view LUT；太阳/月亮与曝光联动；天气参数映射到大气参数。
- **L4c 空气透视**：用大气散射结果替换现有指数雾（`vkAtmosphere.cpp`），保留指数雾作为 fallback 开关。
- **L4d 与 IBL 同源**：环境烘焙（`vkEnvironment.cpp`）改为从新天空 LUT 采样，保持「反射与背景一致」（G3 已建立重烘链路）。

**验收**：CPU 单测（透射率随高度/距离单调、LUT 边界无 NaN、能量守恒近似）；桌面与 Hillaire 2020 参考图/参考实现
对比（色度/亮度趋势，不做绝对数值）；低太阳角不再出现 Preetham 式偏色；切换天气后反射与天空仍然一致。

**风险**：LUT 分辨率与精度（RGB16F）在 780M 上的带宽；大气参数与现有 `SkyParams`（horizon/zenith 颜色）是两套语义，
需要一个映射层并处理旧预设；重烘 128² CPU 预滤波已有 25M 采样开销（`vkEnvironment.cpp:635`），不要每次天气过渡都重烘。

### 4.7 L5：体积云（Schneider 2015）

- **L5a 数据**：weather map（2D，覆盖率/云型）+ 3D Perlin-Worley 噪声（Perlin 2002 的梯度噪声为主）+ 高度梯度。
- **L5b 渲染**：半分辨率 raymarch（64 步起点）+ 6 步光照 march + 透射率/散射；蓝噪声抖动 + 时域重投影（复用 L2b 机制）。
- **L5c 集成**：与 L4 大气散射耦合（太阳/天空光照）；云影可选；远处云与天空盒的接缝处理。

**决策 8 已被推翻（2026-10-31 用户拍板）**：`SKY_WEATHER_PLAN.md` 决策 8「云 = 混合模式，不做 raymarch」不再生效；
体积云为**唯一云路径**：旧天空 FBM 云已从 `gn3d_skybox.frag` 删除，查看器不再提交 billboard 中景云（`X` 键整体开关，默认开）。
本轮实际实现（`docs/L5_CLOUD_CONTRACT.md` 冻结契约）：
- **L5a 数据（已落地）**：CPU 生成 64³ RGBA8 3D Perlin-Worley 噪声（R=基础、G/B=低频/高频 Worley，周期可平铺）
  + 128² RG8 weather map；`src/graphics/cloudNoise.h/.cpp` 纯 CPU、带单测。
- **L5b 渲染（已落地，与上文规划有偏差）**：**全分辨率**逐像素 raymarch（默认 32 步）+ 4 步光照 march +
  静态像素抖动 + Beer/Lambert/HG 相位；**默认单层薄平面**（1600–2200 m，覆盖率/锐度/太阳/风逐帧来自天气），
  多层参数保留但默认关闭；**未做**半分辨率 pass 与**未做**时域重投影（L2b TAA 尚未实现），两者登记为后续项。
- **L5c 集成（已落地）**：在 3D pass 内、天空盒之后绘制（预乘 alpha 混合，前景几何随后自然覆盖）；
  太阳方向/强度与 L4 的天空/大气同源；查看器 `X` 键（默认开）、叠层显示层数/覆盖率；旧云已停用。
- **位置口径**：云层为**水平无限层**（v1），不接球形大气壳层；云影未做。

**验收**：覆盖率与 weather map 一致；云层透光/暗部有体积感；性能目标（780M/1080p 待实测标定，先记录基线）；
桌面固定相机截图对比；关闭开关回到现有 FBM/billboard 表现。

**风险**：全分辨率 raymarch 在 780M/1080p 可能不达标（需实测后决定降半分辨率）；无时域重投影会有噪点/拖影；
噪声纹理显存（1 MiB + weather 32 KiB）；与雾/降水的透明排序；多层边界接缝需桌面观察。

### 4.8 L6：DDGI 探针（Majercik 2019）

- **L6a 场景求交**：当前无空间索引（`3D_FEATURE_PLAN.md:68`）。建议 CPU 静态 BVH 上传 SSBO，compute 遍历；
  动态物体用代理（AABB/简化网格）更新，v1 不做逐三角形动态重建。
- **L6b 探针更新**：八面体辐照度 + 深度矩 3D 纹理；compute 每帧更新有限探针；时间滞后（hysteresis）；天空/太阳作为 miss 回退。
- **L6c 合成**：替换/并存现有 SH9 探针（`vkProbe.cpp`），与 SSGI/SSAO 按可见性混合；保留 `weight=0` 回退。

**验收**：屏幕外遮挡场景中间接光变暗/染色正确；探针边界无接缝；关闭后回到 SH9 探针现状；性能基线（780M）；
桌面固定场景对比。

**风险**：BVH 构建/上传成本；compute 与图形队列同步；探针漏光；与 L2b 时域历史共享重投影。

### 4.9 L7：SSR（屏幕空间反射）

- 半分辨率 ray march（深度 + 法线）+ 粗糙度感知（mip/随机）+ 双边升采样；与 IBL/探针/局部 cubemap 混合；
  屏幕边缘淡出与 miss 回退（Sikachev 2014）。
- **验收**：平面/金属球反射与场景一致；屏幕外正确回退到 IBL；无「反射拖尾」；性能预算。
- **风险**：反射与 G-Buffer 精度；多层透明（MMD 头发/衣服）；与 SSR 的时域复用。

### 4.10 L8：可选效果（按需）

体积雾（Wronski 2014 froxel）、景深、运动模糊、接触阴影（SSAO 增强）、镜头耀斑。**每项独立开关、独立验收**，
不阻塞 L0–L7；在 L1 完成后才允许开工。

## 5. 需拍板的决策（≤5）

| # | 决策 | 推荐 | 备选与推翻条件 |
|---|---|---|---|
| 1 | **渲染路径归属**：GI/后处理挂在延迟路径，但查看器走前向 | 中期把查看器主路径转**延迟不透明 + 前向透明**（复用已有 G-Buffer/SSAO/SSGI/探针/SSR）；短期先做屏幕空间后处理（深度可用时） | 若 MMD 材质（描边/透明/卡通）在延迟下丢失且短期无法补 → 保留前向，GI 只做「前向可用」的子集（IBL/AO 屏幕空间），延后 SSGI/探针 |
| 2 | **色调映射** | ACES 单曲线 fit 为默认，Reinhard 保留对照 | 若角色肤色/高光不可接受 → 换 AgX 或调曲线；必须保留 UI 切换 |
| 3 | **云**：是否推翻 SKY_WEATHER 的「不做 raymarch」 | 渐进：远景 raymarch + 中景 billboard/天空盒 FBM 保留 | 若 780M 性能不达标或工期紧 → 只做远景低分辨率 raymarch，其余保持现状 |
| 4 | **AA 顺序** | SMAA 1× 先交付；TAA 与 L6 的时域降噪一起做 | 若先做 TAA，则需先补 velocity/双历史，属于结构改动，风险前移 |
| 5 | **DDGI 求交** | CPU 静态 BVH 上传 SSBO + compute 遍历 | 若 BVH 维护成本过高 → 先用「天空 + 太阳可见性」的近似探针（不做遮挡），把 DDGI 推后 |

> 推荐项在未获答复前**不实施**；开工前按「用户本轮要求 > 已核实证据 > 旧计划」定胜负（见 §0.3 冲突）。

## 6. 资源、描述符与生命周期约束

1. **槽位不抢**：延迟 0..23 已满（`gn3d_deferred_light.frag:59-86`）。新能力一律**新 pass + 独立 descriptor set/layout**；
   GI/天气的既有槽位表（`3D_FEATURE_PLAN.md` §3.7）继续冻结。
2. **单帧 in-flight**：当前只有一个 `inFlightFence`；时域效果（TAA/云/DDGI）需要**双缓冲历史**，并保证不在 GPU 未完成时覆盖历史。
3. **描述符池无 FREE/RESET**（GI_PLAN §10 风险 3）：resize 只重建 image/view/framebuffer 并更新 image info，不重建池。
4. **新增资源预算（估算，实现时以实测为准）**：HDR mip 链（6 级）、Bloom 临时链、SMAA 3 张 LDR 中间图、
   `RG16F` velocity、2×历史 RGBA16F、大气 LUT（约 256×64×2 + 32×32 + 192×108）、云 3D 噪声（64³/128³ 视预算）、
   DDGI 探针 3D 纹理（8×4×8 × 八面体分辨率）。
5. **命名与拆分**：新文件 camelCase、单功能 <500 有效行、多功能 <1000；交付前跑
   `bash ~/.dsh/skills/code-modularity/scripts/lintCodeModularity.sh <写域> --naming --comments`。

## 7. 文件与接口规划（预估，开工时按实际冻结）

| 阶段 | 新文件（建议） | 职责 | 预估有效行 |
|---|---|---|---|
| L0 | `_Vulkan/vkLightingSwitches.cpp`（或并入 `vkGi.cpp`） | GI/SSAO/SSGI/探针开关表与组合 API | ~120 |
| L1 | `_Vulkan/vkPostProcess.cpp`、`vkBloom.cpp` | HDR 合成链、曝光、tonemap、Bloom mip 链 | ~250 + ~260 |
| L1 | `assets/shaders/gn3d_tonemap.frag`、`gn3d_bloom_down.frag`、`gn3d_bloom_up.frag` | tonemap/曝光、双滤波 Bloom | 各 ~60–100 |
| L2 | `_Vulkan/vkSmaa.cpp`、`gn3d_smaa_edge.frag`、`gn3d_smaa_weight.frag`、`gn3d_smaa_blend.frag` | SMAA 1× 三 pass | ~240 + 各 ~60 |
| L2 | `_Vulkan/vkTaa.cpp`、`gn3d_taa.frag` | jitter/历史/邻域钳制（与 L6 共用） | ~200 + ~120 |
| L3 | `_Vulkan/vkShadow.cpp`、`gn3d_shadow_pcss.frag` | 级联/PCSS/稳定化（替换现有阴影站点） | ~300 + ~120 |
| L4 | `_Vulkan/vkAtmosphereLut.cpp`、`gn3d_atmosphere.comp`、`gn3d_sky_lut.frag` | 大气 LUT + 天空采样 + 空气透视 | ~280 + 各 ~100 |
| L5 | `_Vulkan/vkCloudVolume.cpp`、`gn3d_cloud_raymarch.frag` | weather map/raymarch/重投影 | ~300 + ~200 |
| L6 | `_Vulkan/vkDdgi.cpp`、`gn3d_ddgi_probe.comp`、`gn3d_ddgi_blend.frag` | 场景 BVH SSBO、探针更新、合成 | ~400 + 各 ~150 |
| L7 | `_Vulkan/vkSsr.cpp`、`gn3d_ssr.frag` | SSR + IBL 混合 | ~260 + ~120 |

接口原则：CPU 侧只暴露「开关 + 参数 + pass 调用点」，不把算法参数散进各 pass；UBO 结构一律 `static_assert` 偏移/大小，
并配单测（沿用 `giParams` 的做法）。

## 8. 验收体系

### 8.1 两层验收

| 层 | 环境 | 能验什么 | 命令 |
|---|---|---|---|
| 本机（沙箱） | 无 `/dev/dri` | 编译、CPU 数学/LUT/UBO 契约、shader 编译、lint | `make`、`make unit`、`glslangValidator -V`、`lintCodeModularity.sh` |
| 桌面（有 GPU） | RADV/AMD 780M 或等价 | 视觉、像素断言、性能、验证层 | `make gpuCheck`、`make mmdViewer`、截图对比 |

**桌面验收脚本化**：每阶段提供一份清单（固定相机/天气/分辨率/曝光 + 预期现象 + 失败处置），截图存
`.local/lighting-shots/<phase>/`（过程产物，不入库）；`gpuCheck` 能断言的部分必须写成用例，而不是只写「目视」。

### 8.2 每阶段完成定义（DoD）

1. `make` + `make unit` 0 失败；shader 编译通过；lint 0 违规；
2. 新能力**默认关闭**，开关有文档、有组合；
3. `gpuCheck` 至少新增 1 条可判定的像素/数值断言（无法断言的视觉项写进桌面清单）；
4. 桌面验收通过（人或用户执行），截图/像素值落盘；
5. 文档同步：本文件阶段状态 + `docs/CHANGELOG.md` 一条 + 必要时更新 `GI_PLAN.md` 的引用（不复制结论）。

### 8.3 性能预算（目标，非承诺）

在 780M/1080p 下先测基线帧时间，再给每阶段分配预算；**未实测前不写死毫秒数**。每阶段交付时必须记录：
场景、分辨率、天气、开关组合、帧时间（min/avg/max）。若某阶段超预算，优先降级（半分辨率/步数/采样数），不许静默超支。

## 9. 风险与未决

1. **无 GPU 验证**：本机无法跑 `gpuCheck`，所有视觉结论依赖桌面机；跨阶段回归必须靠固定基线截图 + 像素断言。
2. **路径归属未定（§5 决策 1）**：不先解决，L2/L5/L6/L7 都可能「做了看不到」。
3. **TAA/云的时域历史**与单帧 in-flight 冲突：需要双缓冲 + resize 重建，改动面比后处理本身大。
4. **论文原文缺**（Reinhard/PCSS/SMAA/SDSM/ACES 细节）：实现前必须取原文或权威实现核对公式与参数；未取到就标 `claimed`，
   不许按记忆写。
5. **MMD 材质兼容**：延迟化可能丢失描边/透明/卡通效果（MMD 查看器当前走 `DrawPBR`，`mmdViewer.cpp:314`），
   转延迟前先核对 `cMODEL` 的材质能力与 alpha 需求。
6. **亮度标定主观性**：L0c 基线只解决「可比较」，不解决「正确」；ACES/曝光参数最终需要用户目视拍板。
7. **范围蔓延**：L0–L8 是路线图不是承诺；每阶段独立可交付，未验收不进入下一阶段（除明确并行的 L2/L3）。

## 10. 复算与核对命令

```bash
# 基线
git rev-parse HEAD && git status --short | head

# 本机可做的验收
make
make unit
# shader 编译（严格模式：任一条失败即非零退出；make shaders 只重生成且不传播失败）
fail=0
for f in assets/shaders/*.vert assets/shaders/*.frag assets/shaders/*.comp; do
  glslangValidator -V "$f" -o /tmp/gnv.spv || fail=1
done
[ "$fail" -eq 0 ]

# 实现阶段（写源码后）：写域 = 本次改动的路径
bash ~/.dsh/skills/code-modularity/scripts/lintCodeModularity.sh <写域> --naming --comments

# 桌面（有 GPU 时）
make gpuCheck
make mmdViewer

# 论文库完整性
cd docs/papers && sha256sum -c MANIFEST.sha256
```

## 11. 参考锚点（复算用）

```bash
# 现状：双重 Reinhard / Bloom 近似
grep -n 'Reinhard' assets/shaders/gn3d_pbr.frag assets/shaders/gn3d_deferred_light.frag assets/shaders/gn3d_composite.frag
grep -n 'SimpleBloom\|threshold\|intensity' assets/shaders/gn3d_composite.frag

# 现状：延迟路径目标与调用点
sed -n '736,741p' src/graphics/_Vulkan/vk3dInitScene.cpp
grep -rn 'GN_VkBegin3DPassDeferred\|GN_VkDeferredLighting' src tests --include=*.cpp

# 现状：AA / velocity / 空间索引
grep -n 'msaaSamples' src/graphics/_Vulkan/vk3dInit.cpp
grep -n 'velocity\|无 MSAA' docs/GI_PLAN.md
grep -n '空间划分' docs/3D_FEATURE_PLAN.md

# 现状：GI 默认关与开关
sed -n '14,24p' src/graphics/_Vulkan/vkGi.cpp
grep -n 'GN_VkEnableGI\|GN_VkSetProbeWeight' src/graphics/_Vulkan/GN_Vk.h
```

## 12. 实施进度（2026-10-31）

| 阶段项 | 状态 | 证据 |
|---|---|---|
| L1a 线性 HDR 统一 | ✅ 编译级 | `vk3dInit.cpp`（MSAA/resolve RGBA16F、resolve→`hdrView`）；`vk3dInitScene.cpp`（deferred pass→HDR）；`GN_VkEnd3DPass`/`GN_VkDeferredLighting` 自动合成 |
| L1b 曝光 | ✅ 编译级 | `postParams.*`（EV 换算/CPU 参考/单测）；`GN_VkSetExposure` |
| L1c 色调映射 | ✅ 编译级 | `gn3d_composite.frag`（NONE/Reinhard/ACES 单次）；PBR/延迟 shader 内 Reinhard 已删 |
| L0b 双重 Reinhard | ✅ 编译级 | 同上；`GN_VkBegin3DPassPBR_HDR` 归并为 PBR 别名 |
| L0d 验收扩展（部分） | ✅ 编译级 | `gpuCheck3D.cpp` 新增 `3d.post.aces/reinhard/exposure`；旧色值站点由 `makeLegacyPostUbo()` 保持 |
| L2a AA v1（FXAA） | ✅ 编译级 | `gn3d_composite.frag`（tonemap 后 FXAA 3.11 风格单 pass）；`sPOST_UBO.fxaa` + `GN_VkSetFxaa`；查看器 `T` 键；SMAA 待 LUT 资产 |
| L3 阴影接线（查看器） | ✅ 编译级 | `sMESH/cMODEL::DrawShadow`（仅 OPAQUE）+ 查看器方向光 `enableShadow=true` + AABB 拟合正交光矩阵 + 每帧 shadow pass；`K` 键开关 |
| L3a 稳定化（texel snapping） | ✅ 编译级 | `_Vulkan/vkShadowCascade.cpp` 光空间平移按 texel 世界尺寸取整；正交矩阵 `Mat44OrthoVK` 的 VK [0,1] 深度符号缺陷本轮修（`GN_3Dmath.cpp:336`，桌面需复验） |
| L3b PCSS | ✅ 编译级 | 原始深度手动比较（NEAREST/非 compare）+ 16-tap blocker search + 半影 PCF（12/400/1..24 texel 经验值）；常量仍待桌面标定 |
| L3c CSM | ✅ 编译级 | 4 级固定分割（对数/均匀混合 lambda=0.5）+ 相机视锥角点/AABB 拟合 + `sampler2DArray` 4 层阴影图（2048²×4）+ 着色器包含测试选级；`cascadeCount=1` 回退单级 |
| L4a 大气 CPU 基座 | ✅ 编译级 | `atmosphereParams.h/.cpp`（Hillaire 参数/光学深度/透射率 LUT）+ `testAtmosphere` 7 用例 |
| L4b 天空 CPU 参考 | ✅ 编译级 | `computeSkyRadiance`（单次散射+透射率查表）、`computeSkyViewLut` 192×108、`evalSkyViewLut`；+2 用例 |
| L4c LUT 上传 + GPU 重建 | ✅ 编译级 | `vkAtmosphereLut.cpp`：透射率静态上传 + sky-view 改为逐帧 compute（`gn3d_skyview.comp`，dirty 时 `recordAtmosphereLutUpdate`）；`GN_VkGetAtmosphereLutViews`；`gpuCheck atmo.lut/atmo.skyview` |
| L4d 天空/空气透视接线 | ✅ 编译级 | 天空 shader 采样 sky-view LUT（`atmosphereMix` 开关，旧解析路径保留）；PBR/延迟按 8-step 单次散射替换指数雾（`aAtmosphere2.w` 开关，指数雾 fallback 保留）；环境烘焙同源（同一天空管线 + LUT 绑定）；天气→大气映射由查看器每帧调用 |
| L5a 云数据 | ✅ 编译级 | `src/graphics/cloudNoise.cpp`（64³ RGBA8 Perlin-Worley 周期噪声 + 128² RG8 weather map）+ `testCloudNoise` 单测 |
| L5b 体积云 raymarch | ✅ 编译级（偏离规划） | `gn3d_cloud_vol.vert/frag`：**全分辨率** raymarch（默认 32 步 + 4 光照步 + 静态像素抖动 + Beer/粉末/HG + 环境项）；**球壳求交（曲面）** + 默认单层 1600–2200 m（多层参数保留、默认关闭）；25 km 距离钳制 + 12 km 远景/地平线淡出（修放射线）；光照 march 1.5×厚度（修黑云）；覆盖率/锐度/太阳/风逐帧来自天气；**未做**半分辨率与时域重投影（L2b 未实现） |
| L5c 集成 | ✅ 编译级 | `_Vulkan/vkVolumetricCloud.cpp`（3D 噪声/weather 纹理 + 192 B UBO + 预乘 alpha 全屏管线）；3D pass 内天空盒之后绘制；**旧云已停用**（skybox FBM 云删除、billboard 不再提交）；查看器 `X` 开关（默认开）、叠层显示；`gpuCheck 3d.volcloud` |
| L1d Bloom mip 链 | ✅ 编译级 | `_Vulkan/vkBloom.cpp`（5 级 13-tap↓ + 9-tap tent↑，首级曝光+软阈值）、`gn3d_bloom_down/up.frag`、composite binding3；默认 0.15/1.0/0.5 |
| L0a GI 子开关 | ✅ 编译级 | `GN_VkEnableGI` / `GN_VkSetSsaoEnabled` / `GN_VkSetSsgiStrength` / `GN_VkSetProbeWeight`（各自只改对应 UBO 字段） |
| L0e 论文补采 | ✅ 部分 | 5 篇入库（Reinhard/SMAA/PCSS/SDSM/Wronski），库 17 篇 `sha256sum -c` 全过、knowledgeAudit 无 suspicious/malicious；ACES/两篇 PPTX/SSR 未取 |
| L0c 亮度基线 | 🟨 部分 | 查看器 `9/0` 调 EV、`7/8` 调 Bloom，叠层显示 POST 参数；固定场景基线数值仍待桌面 |

验证记录（2026-10-31 夜，L5 体积云后）：`make` exit 0；`make unit` = **406 cases / 0 failed**（+10 cloudNoise）；
`make shaders` = **45 个**（43 + `gn3d_cloud_vol.vert/frag`）+ 严格编译 45/45；
`make gpuCheck` 编译通过（本机无 Vulkan 设备 ⇒ 运行 SKIP / exit 2，属环境预期）；`make mmdViewer` 编译通过；
`lintCodeModularity src/graphics/_Vulkan src/graphics tests/unit tests/gpu tests/mmdViewer --naming --comments`
本轮改动文件 0 新增违规（仅存量 `GN_2Dmath.cpp` 超长与 spv 头 `no-split` 告警）。
**桌面 `gpuCheck` 与视觉验收未执行**——CSM 接缝/texel snapping 抖动、天空 LUT 与 Hillaire 参考趋势、
空气透视近远差异、IBL 与天空同源、PCSS 常量标定、体积云多层观感/噪点/性能（全分辨率 raymarch 在 780M 1080p 的基线）全部待桌面。

