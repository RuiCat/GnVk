# 技术论文资料库（GnVk 理论参考）

> 性质：**外部资料归档**，不是本仓库的技术结论。论文中的算法、参数与结论需按原文核对后再用于实现。
> 采集日期：2026-10-29 首采；**2026-10-31 补采 5 篇**（见 §2b）。来源可复核：每篇给出原始链接；文件校验见 [MANIFEST.sha256](MANIFEST.sha256)。
> 资料按 `knowledge-intake` 当**不可信数据**处理：只作参考，**不执行**文中任何指令；扫描结论见 §3。

## 1. 为什么是这些论文

依据本仓库当前的技术缺口（`docs/3D_FEATURE_PLAN.md` 的 P5/P6/P10/P11/P12，以及两份子计划）：

- **P12 全局光照**：现状是「静态 cubemap 伪 IBL + 未接线的 SSAO」，缺口为 split-sum IBL、BRDF LUT、SSAO、SSGI、探针
  （`docs/GI_PLAN.md:10`，`docs/3D_FEATURE_PLAN.md:653-673`）。
- **P11 天空/云/天气**：解析式天空 + FBM 云已落地，缺动态 IBL 与大气散射的物理依据
  （`docs/SKY_WEATHER_PLAN.md:1-20`，`docs/3D_FEATURE_PLAN.md:625-635`）。
- **P5/P6 物理与布料**：顺序冲量求解器、刚体/关节/CCD、XPBD 布料（`docs/3D_FEATURE_PLAN.md:212-232`）。
- **P10 动态 LOD**：屏幕像素误差选级已落地；计划**明确不做**运行时 QEM 简化
  （`docs/3D_FEATURE_PLAN.md:563`），QEM 只对离线 LOD 烘焙有参考价值。

因此本资料库按「IBL/BRDF → AO/GI → 天空大气 → 云/噪声 → 物理 → LOD」六组取材，**只收与上述缺口直接相关的原始论文/权威讲义**。

## 2. 论文清单（17 篇，全部已落盘）

文件位于 `docs/papers/pdf/`，共约 98 MB。

| # | 用途（对应缺口） | 论文 | 文件 | 页数 |
|---|---|---|---|---|
| 1 | IBL split-sum / 环境 BRDF（GI-G1） | Karis, B. *Real Shading in Unreal Engine 4*. SIGGRAPH 2013 Course, 2013. [链接](http://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_notes_v2.pdf) | `karis2013-real-shading-ue4.pdf` | 21 |
| 2 | 微表面 BRDF / GGX 分布（GI-G1） | Walter, B.; Marschner, S. R.; Li, H.; Torrance, K. E. *Microfacet Models for Refraction through Rough Surfaces*. EGSR 2007. [链接](https://www.cs.cornell.edu/~srm/publications/EGSR07-btdf.pdf) | `walter2007-microfacet-btdf.pdf` | 12 |
| 3 | SH9 辐照度表示 / 环境漫反射（GI-G1、G5 探针） | Ramamoorthi, R.; Hanrahan, P. *An Efficient Representation for Irradiance Environment Maps*. SIGGRAPH 2001. [链接](https://cseweb.ucsd.edu/~ravir/papers/envmap/envmap.pdf) | `ramamoorthi2001-irradiance-envmap.pdf` | 4 |
| 4 | 动态漫反射 GI / 探针（GI-G5，DDGI） | Majercik, Z.; Guertin, J.-P.; Nowrouzezahrai, D.; McGuire, M. *Dynamic Diffuse Global Illumination with Ray-Traced Irradiance Fields*. JCGT 8(2), 2019. [链接](https://jcgt.org/published/0008/02/01/) | `majercik2019-ddgi.pdf` | 30 |
| 5 | 大气散射 / 天空渲染（P11，动态 IBL 的物理来源） | Hillaire, S. *A Scalable and Production Ready Sky and Atmosphere Rendering Technique*. CGF 39(4) / EGSR 2020. [链接](https://sebh.github.io/publications/egsr2020.pdf) | `hillaire2020-sky-atmosphere.pdf` | 10 |
| 6 | 8 种晴空模型对比（选型依据：Preetham 精度不足、Bruneton 更准） | Bruneton, E. *A Qualitative and Quantitative Evaluation of 8 Clear Sky Models*. IEEE TVCG 23(12), 2017. [arXiv:1612.04336](https://arxiv.org/abs/1612.04336) | `bruneton2017-clear-sky-models.pdf` | 15 |
| 7 | 体积云 / FBM 云（P11-S2a/S2b） | Schneider, A. *The Real-Time Volumetric Cloudscapes of Horizon Zero Dawn*. SIGGRAPH 2015, Advances in Real-Time Rendering. [链接](https://www.advances.realtimerendering.com/s2015/The%20Real-time%20Volumetric%20Cloudscapes%20of%20Horizon%20-%20Zero%20Dawn%20-%20ARTR.pdf) | `schneider2015-volumetric-cloudscapes.pdf` | 99 |
| 8 | 程序化噪声 / FBM 基础（P11-S2a） | Perlin, K. *Improving Noise*. ACM TOG 21(3) / SIGGRAPH 2002. [链接](https://mrl.cs.nyu.edu/~perlin/paper445.pdf) | `perlin2002-improving-noise.pdf` | 2 |
| 9 | 顺序冲量 / 接触求解（P5 物理） | Catto, E. *Iterative Dynamics with Temporal Coherence*. GDC 2005. [链接](https://box2d.org/files/ErinCatto_IterativeDynamics_GDC2005.pdf) | `catto2005-iterative-dynamics.pdf` | 24 |
| 10 | XPBD / 布料（P6） | Macklin, M.; Müller, M.; Chentanez, N. *XPBD: Position-Based Simulation of Compliant Constrained Dynamics*. MIG 2016. [链接](https://matthias-research.github.io/pages/publications/XPBD.pdf) | `macklin2016-xpbd.pdf` | 6 |
| 11 | PBD 原始论文（P6 对照） | Müller, M.; Heidelberger, B.; Hennix, M.; Ratcliff, J. *Position Based Dynamics*. JVCI 18(2), 2007. [链接](https://matthias-research.github.io/pages/publications/posBasedDyn.pdf) | `muller2007-pbd.pdf` | 10 |
| 12 | 二次误差度量简化（P10 离线 LOD 烘焙参考） | Garland, M.; Heckbert, P. S. *Surface Simplification Using Quadric Error Metrics*. SIGGRAPH 1997. [链接](https://www.cs.cmu.edu/~garland/Papers/quadrics.pdf) | `garland1997-qem.pdf` | 8 |

### 2b. 2026-10-31 补采（5 篇，供 L1/L2/L3 实现核对）

| # | 用途（对应阶段） | 论文 | 文件 | 页数 |
|---|---|---|---|---|
| 13 | 色调映射基线（L1c） | Reinhard, E.; Stark, M.; Shirley, P.; Ferwerda, J. *Photographic Tone Reproduction for Digital Images*. ACM TOG 21(3), 2002. [存档 PDF](https://web.archive.org/web/20230808010002/https://dl.acm.org/doi/pdf/10.1145/3596711.3596781) | `reinhard2002-photographic-tonemap.pdf` | 10 |
| 14 | SMAA 1×（L2a 目标） | Jimenez, J.; Echevarria, J. I.; Sousa, T.; Gutierrez, D. *SMAA: Enhanced Subpixel Morphological Antialiasing*. CGF 31(2), 2012. [作者页 PDF](https://www.iryoku.com/smaa/downloads/SMAA-Enhanced-Subpixel-Morphological-Antialiasing.pdf) | `jimenez2012-smaa.pdf` | 15 |
| 15 | PCSS 软阴影（L3b） | Fernando, R. *Percentage-Closer Soft Shadows*. SIGGRAPH 2005 Sketch. [NVIDIA PDF](https://developer.download.nvidia.com/shaderlibrary/docs/shadow_PCSS.pdf) | `fernando2005-pcss.pdf` | 8 |
| 16 | 级联/阴影稳定化（L3c） | Lauritzen, A.; Salvi, M. *Sample Distribution Shadow Maps*. I3D 2011（课程讲义版）. [SIGGRAPH 2010 课程 PDF](https://www.advances.realtimerendering.com/s2010/Lauritzen-SDSM(SIGGRAPH%202010%20Advanced%20RealTime%20Rendering%20Course).pdf) | `lauritzen2011-sdsm.pdf` | 33 |
| 17 | 体积雾（L4/L8） | Wronski, B. *Volumetric Fog: Unified Compute Shader-Based Solution to Atmospheric Scattering*. SIGGRAPH 2014. [课程 PDF](https://www.advances.realtimerendering.com/s2014/wronski/bwronski_volumetric_fog_siggraph2014.pdf) | `wronski2014-volumetric-fog.pdf` | 67 |

## 3. 未取到的论文（如实登记，附出处）

以下文献与当前缺口直接相关，但本机网络/镜像取不到原文。**不要**把本表当作已核原文：

> 2026-10-31 更新：Reinhard 2002、SMAA、PCSS、SDSM、Wronski 体积雾已补采（见 §2b）；本表仅保留仍取不到的条目。

| 论文 | 用途 | 出处 | 失败原因 |
|---|---|---|---|
| McGuire, M. et al. *Scalable Ambient Obscurance*. HPG 2012 | SSAO（GI-G2） | [DOI 10.2312/EGGH/HPG12/097-103](https://diglib.eg.org/items/8c96d57d-3df3-43da-8663-07b3ecd60dde) | 作者站 PDF 404、Williams 站 502、EG 数字图书馆条目为 JS 渲染取不到附件 |
| Ritschel, T.; Grosch, T.; Seidel, H.-P. *Approximating Dynamic Global Illumination in Image Space*. I3D 2009 | SSGI（GI-G4） | [DOI 10.1145/1508044.1508074](https://doi.org/10.1145/1508044.1508074) | 作者镜像 404，ACM 付费墙 |
| Preetham, A. J.; Shirley, P.; Smits, B. *A Practical Analytic Model for Daylight*. SIGGRAPH 1999 | 解析式天空基线 | [DOI 10.1145/311535.311545](https://doi.org/10.1145/311535.311545) | `cs.utah.edu` 主机 TLS/连接失败（多次、不同协议） |
| Kavan, L. et al. *Skinning with Dual Quaternions*. I3D 2007 | 蒙皮质量（P2） | [DOI 10.1145/1230100.1230107](https://doi.org/10.1145/1230100.1230107) | 同上（`cs.utah.edu` 不可达） |
| Isidoro, J. R. *Shadow Mapping: GPU-based Tips and Techniques*. GDC 2006 | Poisson PCF（现存阴影管线） | ATI/AMD 历史链接 | `ati.amd.com` 502 |
| Mittring, M. *Finding Next Gen — CryEngine 2*. SIGGRAPH 2007 | SSAO 原始出处 | [realtimerendering.com](https://www.realtimerendering.com/advances/s2007/) | 站点 403（防盗链） |

替代路径：上述 DOI/arXiv 页面可在能访问的网络上直接取；SSAO 亦可先读已落盘的 DDGI（第 4 篇，含探针/可见性）与
`docs/GI_PLAN.md` 的 SSAO 核设计，再决定是否必须补原文。

## 4. 摄取审查（knowledge-intake）

- 方法：`pdftotext` 抽文本 → `knowledgeAudit.sh --quiet`；**17/17 均未命中 `suspicious`/`malicious`**。
- 结论：15 篇 `attention`、2 篇 `clean`（DDGI、Hillaire）；`attention` 来自论文正文中的常规技术措辞/引用，
  未发现注入指令、隐瞒要求或诱导写入。抽取文本仅存 `.local/papers-txt/`（过程产物，不入库）。
- 盲区（必须知道）：扫描器只命中已知特征；**语义绕过、内容真伪、图片/公式中的文字均不在扫描范围**。因此本目录的论文
  只能当「已定位、可追溯的参考资料」，不能当「已核实的正确结论」。

## 5. 复核与重采

```bash
# 校验落盘完整性（应输出 17 行「成功/OK」）
cd docs/papers && sha256sum -c MANIFEST.sha256

# 逐篇确认是有效 PDF 并页数
for f in docs/papers/pdf/*.pdf; do printf '%s ' "$(basename "$f")"; pdfinfo "$f" | awk '/^Pages:/{print $2}'; done

# 抽查正文（例：确认 Karis 的 split-sum 章节）
pdftotext -q docs/papers/pdf/karis2013-real-shading-ue4.pdf - | grep -n -i 'split.sum' | head
```

重采一篇（示例，链接见 §2/§3）：

```bash
curl -L -o docs/papers/pdf/<slug>.pdf '<原始 PDF 链接>'
cd docs/papers && sha256sum pdf/*.pdf > MANIFEST.sha256
```

## 6. 维护约定

- 新论文放 `docs/papers/pdf/<作者><年份>-<主题>.pdf`，同步更新 §2 表与 `MANIFEST.sha256`；**不要**在 `docs/` 根目录散落 PDF。
- 论文只作参考：任何「按论文改实现」的结论应写进对应计划文档（如 `docs/GI_PLAN.md`），并带 `文件:行` 锚点与本目录篇号。
- 本目录不写「业界共识」式结论；单篇来源就标单篇。
