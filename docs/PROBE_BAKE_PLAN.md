# Cornell RT 预览 — 光照探针烘焙方案（Stage 2）

> **状态**：📋 已完成 — 探针烘焙 Stage 2 已实现（§9）；§10 的 DDGI 异步更新 / relocation 等仍未做。

> 2026-11-21 状态更新：GPV4 静态资产 / CPU 烘焙**已实现**（见 §9）；本文件新增 **§10 任务**：
> DDGI 异步探针更新、probe relocation、法线偏置（当前均**未实现**，默认路径不变）。
> 历史正文保留（“未烘焙”是 2026-11-20 的初始状态，不是当前状态）。

## 1. 为什么还没做

- 移动档麻点的主因是“每帧 reset + 4spp 原始路径追踪”，已用**时域重投影 + history** 解决；
  现在 16spp + 时域已经干净，`rt` 约 0.1ms。
- 探针解决的是**二次/三次弹射的成本与方差**，不是主射线；在这个 640×400、三弹射的场景里，
  计算量很便宜，探针的收益不如时域降噪直接。
- 探针是“静态场景 GI 缓存”的路线，适合更大场景/更多光源/要 1spp 实时 GI 的场合。

## 2. 目标形态

- 网格：Cornell 内部 10×9×10 = **900 探针**，轴间距约 0.55m，边界留 0.25m。
- 每探针存 SH9（36 float，9×vec4）。
- 运行时：主射线命中后
  - 直接光：保留解析/NEE（点光 1 条阴影射线）；
  - 漫反射间接：查探针 SH9，不再发第 2/3 次余弦射线；
  - 金属：保留 1 次镜面反射射线；
  - 天空 miss：保留 skyColor。
- 效果：每样本 trace 从 3（太阳）/6（点光）降到 1/2；二次弹射方差消失；
  可把移动档降到 1–2spp + 时域。

## 3. 烘焙实现

### 3.1 GPU compute（推荐）

- 一个 dispatch 覆盖 `probeCount × faceCount × faceRes²` 个方向；
  每个线程从探针位置沿方向发射路径，累计 radiance。
- 6 面 16×16、32spp、3 弹射：
  `900 × 6 × 256 × 32 × 3 ≈ 133M trace`（GPU 秒级；llvmpipe 分钟级）。
- 投影到 SH9：复用 `giParams::shProjectCubemap`（已验证 A_l 约定），或方向采样直接累加。
- 输出：900×36 float（约 130KB），可上传为 9 张 3D RGBA16F（10×9×10），
  或 SSBO + 手写三线性。

### 3.2 CPU 备选

- 用现有 `cornellCpuRt` 逐探针/逐面渲染；成本高（亿级 trace），只适合离线一次性/低分辨率验证。

## 4. 运行时接线

- 独立于引擎延迟路径：RT compute 管线需要自己的 descriptor：
  - 9 个 `COMBINED_IMAGE_SAMPLER`（3D 纹理）或 1 个 SSBO；
  - 1 个 probe UBO（origin/spacing/counts/weight）。
- 采样：世界坐标 → 三线性 → SH9 求辐照度；乘 albedo/π（注意 irradiance 已含余弦卷积）。
- 开关：`CORNELL_PROBES=1`；关闭时逐像素回到当前路径。
- 光照切换：太阳/点光各缓存一份，K 键切换；或切换时重烘（GPU 秒级）。

## 5. 质量与风险

- Cornell 是封闭凸盒，穿墙漏光最轻；主要误差在球体接触处和 SH9 低频截断。
- 金属反射不烘焙，保留 1 次射线。
- 探针是近似，预览不再是严格 reference；保留 `CORNELL_PROBES=0` 对拍。
- 需要 MAE 对比（现有 `cornellCompare`/CPU PT）确认可接受。

## 6. 分阶段

1. **Stage 2a**：GPU bake compute + 数据输出（SSBO/3D 纹理），先只验证数值（CPU 对拍）。
2. **Stage 2b**：RT shader 增加探针采样，`CORNELL_PROBES=1` 开关；metal 保留反射。
3. **Stage 2c**：太阳/点光两份缓存、MAE/画质对比、帧时间对比；再决定是否默认开。
4. **Stage 2d（新增，2026-11-21）**：DDGI 异步探针更新——GPU compute 逐帧分摊更新、双缓冲、
   temporal hysteresis、预算调度；不再依赖启动时静态烘焙。
5. **Stage 2e（新增）**：probe relocation——检测墙内/球内探针，存 per-probe offset，
   查询时按 offset 修正探针位置，改善球体边缘和墙角。
6. **Stage 2f（新增）**：法线偏置 / 自阴影偏置——查询点沿法线（可选沿视线反向）偏移，
   配合可见性权重，减少轮廓漏光与墙角串色。
7. **Stage 2g（可选）**：把同一套 DDGI 数据/更新接到引擎 G5 延迟路径（当前 G5 是 8×4×8 SH9，
   与本 Cornell DDGI 是两套实现，不混用）。

## 7. 现有可复用代码

- `src/graphics/GN_3Dprobe.h/.cpp`：`cPROBE_GRID`、`SampleSh`、`packProbeTextures`。
- `src/graphics/_Vulkan/vkProbe.cpp`：9 张 3D 纹理上传/描述符写法（引擎延迟路径）。
- `src/graphics/giParams.h/.cpp`：`shProjectCubemap`、SH9 约定。
- `tests/cornellBox/cornellCpuRt.cpp`、`gn_cornell_rt.comp`：路径追踪/shading 公式。

## 8. 结论

- 探针烘焙**可以做，且现有基础设施能复用**；但它和时域降噪是两条正交路线：
  时域解决“低 spp 噪声”，探针解决“二次弹射成本/静态 GI 缓存”。
- 在 Cornell 这个小场景，探针的收益主要是**把 3 弹射降到 1 弹射**；
  如果目标是更大场景/更多光源的实时 GI，值得做。

## 9. 实施状态（2026-11-20）

- **2a 已实现**：`cornellProbeBake.cpp` 用 CPU 路径追踪逐探针渲染 6 面 cubemap，
  `GN3d::shProjectCubemap` 投到 SH9；初始网格 20×18×20（0.275m），8×8 面、16spp，
  7200 探针约 **11s**（llvmpipe/CPU）；2026-11-21 已提升到 24×22×24@0.228m（见本节末尾）。
- **2b 已接**：`gpuRtUploadProbes` 把 SH9 上传为 SSBO + 48B probe UBO；
  RT shader 在首个漫反射命中处查探针并跳过第 2/3 次弹射，金属保留 1 次反射。
  开关：`CORNELL_PROBES=1`；默认关闭。
- 验证：llvmpipe 下 bake/upload 成功，画面可出；默认路径不受影响。
- **2026-11-20 追加**：已改为**探针只烘间接光**（bake 首个命中跳过直接光），运行时
  保留直接光，再加探针间接；并对 SH9 做 3×3×3 盒式平滑。块状色斑消失，画面变软但
  干净；直接光保留。
- **2026-11-20 DDGI-lite**：烘焙时记录每探针 6 面平均主命中距离（faceAvgDepth），
  运行时按“探针→表面”方向选面，距离超过 `faceDepth*1.25+0.05` 时丢弃该角点探针；缓存升级 GPV2 含 depth。
  2026-11-21 升级为**每面 8×8 逐 texel depth moments**（mean/mean²，GPV4 缓存约 23MB），
  shader 用 Chebyshev 可见性权重；仍没有 DDGI 的异步探针更新/relocation。
- **质量限制**：仍是 SH9 低频 + 0.275m 网格 + 全量平滑，细节不如 DDGI；要接近游戏效果，
  需要逐 texel 深度矩/DDGI 更新、离线缓存。
- 启动烘焙 11s 只适合实验；现已把 sun/point 两份 GPV4 缓存落到
  `assets/probes/`（各 ~23MB），启动直接 asset hit；缺失时回落 `.local`/重烘。
- 2026-11-21 已支持 **K 切光照热切换**：按 lightMode 缓存两份探针，切光时上传对应
  asset 并重置时域历史；未做 DDGI 异步更新/relocation。
- 2026-11-21 网格提升到 **24×22×24@0.228m**，depth 4×4/面；资产 12MB/份，
  `assets/probes/cornell_probe_<0|1>_24x22x24_4x16.bin`。

---

## 10. DDGI 异步更新 / Probe Relocation / 法线偏置（新增任务，2026-11-21）

> 性质：**任务登记 + 设计**。当前全部未实现；默认路径（`CORNELL_PROBES` 未设置）不受影响。
> 本节的“DDGI”指 Cornell RT 预览里的探针 GI，不是引擎延迟路径的 G5（8×4×8 SH9）那套。

### 10.0 现状与差距（必须先说清）

当前实现是 **DDGI-lite 的静态版本**：

| 能力 | 现状 | 证据 |
|---|---|---|
| 探针数据 | 24×22×24 = 12672 探针，间距 0.228m；每探针 SH9（36 float）+ 6 面 × 4×4 depth moments（mean/mean²） | `assets/probes/README.md:1-15`；`docs/CHANGELOG.md:13-20` |
| 数据来源 | CPU 逐探针 6 面路径追踪，**启动时一次性烘焙**；或直接加载 GPV4 资产（12MB/份） | `tests/cornellBox/cornellProbeBake.h:12-22`；`cornellProbeBake.cpp:1-80` |
| 运行时 | 只读 SSBO；`gpuRtUploadProbes` 启动/切光时上传；切光 `vkDeviceWaitIdle` | `tests/cornellBox/cornellGpuRt.cpp:671-760` |
| 查询 | 三线性插值 8 个角点；`probePos` = 网格基点，**没有 relocation offset**；`n` 只用于 SH 求值，**没有 normal bias** | `assets/shaders/gn_cornell_rt.comp:30-58,96-123`；`gn_cornell_rq.comp:49-77,115-142` |
| 更新 | 无逐帧更新、无 temporal 累积、无预算调度 | §9 实施状态自述“未做 DDGI 异步更新/relocation” |

**差距**：静态烘焙意味着光照/物体变化必须重烘或换资产；探针落在球体/墙内时会产生黑探针或跨墙串色；
查询点就在表面上，自阴影可见性在球体轮廓/墙角方差最大，容易漏光。用户要求把这三件事补上。

### 10.1 目标与非目标

**目标**

| 编号 | 目标 | 可观察结果 |
|---|---|---|
| G1 | 异步探针更新 | 每帧固定预算更新一部分探针，CPU 不等待；若干帧后探针收敛；光照/物体变化不需要重烘整个网格 |
| G2 | Probe relocation | 落在球体/墙内的探针得到 per-probe offset 或被标记 inactive；球体边缘/墙角的黑斑、穿墙串色减少 |
| G3 | 法线偏置 | 查询点沿法线（可选沿视线反向）偏移；球体轮廓/墙角的漏光减少，平面不过暗 |
| G4 | 兼容/回退 | GPV4 静态资产仍可直接加载；`CORNELL_PROBE_ASYNC=0` 时行为与现在一致；无 GPU RT 时回退静态 |

**非目标（v1 不做）**

- 不做多卷/级联 DDGI、不做动态物体 TLAS refit（Cornell 场景静态；动态物体另立任务）。
- 不做引擎 G5 的 DDGI 化（G5 是 8×4×8 SH9，数据/绑定完全不同；见 Stage 2g 可选）。
- 不做 GPU 驱动级优化（异步 compute 队列/时间线信号量）；v1 用同队列 + 双缓冲 + 帧预算实现“不阻塞 CPU”。
- 不做离线 GPV5 资产（relocation offset 是运行时状态；v1 不持久化）。

### 10.2 数据契约（GPV4 静态资产保持兼容）

#### 10.2.1 UBO：48 B → 96 B（前 48 B 字段位置不变）

| 偏移 | 字段 | 语义 |
|---:|---|---|
| 0 | `vec4 origin` | 网格原点 xyz |
| 16 | `vec4 spacingCounts` | x=spacing，yzw=counts |
| 32 | `vec4 weight` | x=强度（0=关），y=depthRes（现有）；**z=normalBias（世界单位），w=viewBias** |
| 48 | `vec4 relocation` | x=maxOffset，y=insideThreshold，z=iterations，w=enable(0/1) |
| 64 | `vec4 update` | **实现口径**：x=rangeStart，y=rangeCount（本帧 dispatch 的探针数），z=hysteresis(0..1)，w=flags（host 每帧置 1） |
| 80 | `vec4 state` | **实现口径**：x=spp（每 texel radiance 样本数），y=maxAge，z=epsilon，w=reserved |

- 前 48 B 与现有 shader 的 `origin/spacingCounts/weight` **逐字段一致**；现有静态路径继续读前三个 vec4，
  不因扩容改变行为。`gpuRtUploadProbes` 的 `float ubo[12]` 需要扩到 24 个 float、descriptor range 48→96。
- `weight.y` 仍是 depthRes；现有 `gn_cornell_rt.comp:96` / `rq:115` 不需要改。

#### 10.2.2 SSBO：在现有 binding 12/13 上做双缓冲，新增 14/15/16

| binding | 名称 | 布局 | 用途 |
|---:|---|---|---|
| 11 | `ProbeUBO` | 96 B | 上表 |
| 12 | `ProbeSh` | `vec4[2 * N * 9]` | SH9 双缓冲：`parity*N*9 + probeIndex*9 + k` |
| 13 | `ProbeDepth` | `vec2[2 * N * 6 * R * R]` | depth moments 双缓冲；`parity` 同上 |
| 14 | `ProbeOffset` | `vec4[N]` | xyz=relocation 世界偏移；w=0 保留（flags 放 state） |
| 15 | `ProbeState` | `uvec4[N]` | x=age，y=class(0=active/1=relocated/2=inactive)，z=lastScore(×1000)，w=flags |
| 16 | `ProbeWork` | `uint[budget]` | 可选：脏探针索引表；v1 可不用，直接 round-robin |

- `parity = update.y & 1`：渲染读 parity，更新写 `1-parity`；下一帧翻转。这样 compute 与 fragment 不碰同一份数据，
  不需要同一帧 compute→fragment 屏障；只需帧边界屏障。
- **实现偏差（2026-11-21，B3c-2）**：v1 **不做 12/13 双缓冲**，改为单缓冲原地混合 +
  compute→compute barrier。原因：每帧只更新预算内的一小段探针，若按 `parity=N&1` 逐帧翻转
  读写缓冲，同一探针相邻两次更新落在不同缓冲、且渲染逐帧在两代数据间切换，混合基准与可见值
  都会错位（闪烁 + 无法收敛）。原地写入 + 屏障保持“CPU 不等待”，且一次全量 vs 预算分批可逐位对拍。
- 显存：SH9 ≈ 3.5 MB，depth ≈ 18.6 MB（N=12672、R=4），offset 0.2 MB，state 0.2 MB；
  用于 llvmpipe 校验时全部为 host-visible（便于回读），真机可改 device-local。
- `ProbeOffset`/`ProbeState` 单缓冲：更新写、渲染同帧读（屏障保证顺序）。

#### 10.2.3 查询契约

对 8 个角点探针：
1. `base = origin + spacing * (px,py,pz)`；
2. `probePos = base + offset[probeIndex].xyz`（relocation）；
3. `wpB = wp + n * normalBias - viewDir * viewBias`（法线/反向偏置，`viewBias` 默认 0）；
4. `toSurface = wpB - probePos`；`dist = length(toSurface)`；
5. `class == inactive` → 权重 0；`dot(probePos - wpB, n) <= 0` → 权重乘 0.5（背面探针降权）；
6. depth moments 的 Chebyshev 权重保持现有形式，`diff = max(0, dist - mean - epsilon)`；
7. SH9 权重与角点三线性权重相乘，归一化；`wsum` 太小返回 0。

### 10.3 异步更新流水线（Stage 2d）

#### 10.3.1 帧结构

Frame N:
  render RT shader (读 parity = N&1)
  dispatch probe update (写 1-parity；预算 budget 个探针)
  barrier (compute write -> compute read)
  dispatch relocation (读 1-parity 的 depth；写 offset/state)
  barrier (compute -> next frame fragment)
Frame N+1:
  parity 翻转；render 读刚更新的数据（depth/SH9/offset）
```

- **异步定义**：CPU 不在探针更新上 `vkWaitForFences`/`vkDeviceWaitIdle`；每帧只提交固定预算的 compute，
  渲染继续用同一份已更新的探针数据。这样光照/物体变化会在一段时间内渐进生效，而不是阻塞启动。
- v1 **同队列**：update dispatch → `VK_MEMORY_BARRIER`（compute write → compute read/write）→ render dispatch，
  同一 command buffer 内串行；真正的多队列并行留到 v2（timeline semaphore / 队列族选择，Cornell 单队列即可）。
- relocation 的输入是 depth moments：v1 的 relocation 仍是上传时 CPU 计算（B3b-lite），
  下一阶段改为 compute 读更新后的 depth 再写 offset；顺序同样是 update → barrier → relocation → barrier。
- 切光/重载资产时：`aHistoryValid=false`、所有 age=0、offset 归零、`frameIndex=0`；静态资产仍可先上传作 warm start。
- **开关关系**：`CORNELL_PROBE_ASYNC` / `CORNELL_PROBE_RELOCATION` / `CORNELL_PROBE_NORMAL_BIAS`
  任一新开关非默认值时，应自动置 `gProbesEnabled=true`（否则探针采样本身被 `CORNELL_PROBES` 关掉，新逻辑等于空转）；
  `CORNELL_PROBES` 未设置且新开关全默认时，走原路径。

#### 10.3.2 更新 compute

- 新增 `assets/shaders/gn_cornell_probe_update.comp`（软件路径）与 `gn_cornell_probe_update_rq.comp`（HW ray query 路径）；
  复用 `gn_cornell_rt.comp` / `gn_cornell_rq.comp` 的场景遍历与材质/shading 代码（抽公共 include 或复制最小函数）。
- 每个 workgroup 负责 1 个探针：`gl_WorkGroupID.x` 从 `update.x` 起算；`local_size_x = 96`，
  `gl_LocalInvocationIndex` 覆盖 6 面 × 4×4 texel（R=4，与 GPV4 资产一致）；
  每个 texel 追 `state.x` 条射线（默认 4，`CORNELL_PROBE_SPP` 可调），texel 中心方向与 CPU 烘焙一致。
- 每个 texel 另有 4 条 ±0.5 texel 抖动射线写 depth moments；SH9 由 invocation 0 在 `barrier()` 后按
  solid angle 归约，seed 只依赖 (probe, face, texel, sample)，与帧号/预算无关。
- 累计：radiance → SH9（`shProject` 的离散方向权重）；命中距离 → mean/mean²（miss 用大值，与现有 bake 一致）。
- 首命中只累计间接光（与现有 `indirectOnly` 一致），运行时保留直接光 + 探针间接。
- 输出写 `1-parity` 的 SH9/depth；不写 offset/state（relocation 单独一步）。

#### 10.3.3 预算与调度

- 默认 `probesPerFrame = 64`（`CORNELL_PROBE_UPDATE_BUDGET` 可调，钳制 1..512）；
  12672 / 64 ≈ 198 帧 ≈ 3.3 s @60fps 完成一轮；512/帧 ≈ 25 帧。
- v1 round-robin：`start = (frameIndex * budget) % N`；跳过 inactive 探针（其 age 不增）。
- v2（可选）：`ProbeWork` 脏表——相机附近、offset 刚变、光照刚切的探针优先；CPU 每帧填表（≤budget 个），
  compute 按表 dispatch。
- 每帧最多一个 update dispatch；不因预算不足阻塞渲染。

#### 10.3.4 temporal hysteresis

- 每探针维护 `age`（`ProbeState.x`）：
  `alpha = max(1/(age+1), 1-hysteresis)`，`hysteresis` 默认 0.9（即 floor 0.1）；
  `newSh = mix(oldSh, sampleSh, alpha)`，depth 同理；`age = min(age+1, maxAge)`。
- `relocation` 改变 offset > epsilon、切光、资产重传时：`age=0`（重新收敛），避免旧位置/旧光照残留。
- 可选“perceptual encoding”加速大变化收敛（JCGT 2021 的 transitions 思路），v1 不做，先保证稳定。

### 10.4 Probe relocation（Stage 2e）

#### 10.4.1 检测

- 用当前 depth moments（`1-parity`）计算 inside score：
  `score = (# texels with mean < insideThreshold * spacing) / (6*R*R)`。
- 默认 `insideThreshold = 0.5`；`score > 0.5` 视为“探针在几何内/贴墙太近”。
- 只对 active 探针做；inactive 探针周期性（每 K 帧）复查，避免永久死锁。

#### 10.4.2 候选位置

- v1：6 个轴向候选 `base ± axis*maxOffset`（`maxOffset = 0.5*spacing`），对每个候选从候选点追 6 条轴向短射线，
  用 `mean < 0.5*spacing` 计数作为候选分数；选分数最低者。
- 若最低分数 < 当前分数 - `margin`（默认 0.1），把 offset 向候选方向移动，单帧步长 ≤ `0.25*spacing`；
  否则**仅在当前分数 > 0.5 时**标记 `inactive`（避免探针留在墙里持续发黑）。
  > 实现备注：原句「否则标记 inactive」若字面执行，会把所有健康探针（当前分数 0、无更优候选）
  > 一次性判死；实现收紧为“困在几何内且无更优候选”才停用。
- 逐轴 clamp 同时受网格边界约束：偏移后位置不得超出 `[origin, origin+spacing*(counts-1)]`。
- offset 变化 > epsilon 时 `age=0`（重新收敛）；`class=1` 表示已移动，`2` 表示 inactive。
- v2（可选）：64 个邻近候选/迭代搜索（Flax/RTXGI 的 relocation 思路），代价更高；v1 先用 6 轴。

#### 10.4.3 状态与查询

- `ProbeOffset` 存 xyz 偏移（世界单位，逐轴 clamp 到 ±`maxOffset`）；`ProbeState.y` 存 class。
- `ProbeState` 的 `lastScore` 用于滞回：分数改善 < `margin` 不移动，防抖。
- 查询按 §10.2.3：offset 加到角点探针位置；inactive 权重 0；背面探针降权。
- offset 变化 > epsilon 时 `age=0`；旧 SH9 保留但重新收敛。
- 边界：offset 不得把探针推出网格外（clamp 到 `[origin, origin+spacing*(counts-1)]`）；
  相邻探针可重叠，但 v1 不做去重。

### 10.5 法线偏置 / 自阴影偏置（Stage 2f）

- 查询点：
  `wpB = wp + n * normalBias`；`normalBias` 默认 `0.1*spacing`，钳制 `[0, 0.5*spacing]`。
- 可选反向偏置：`wpB -= normalize(viewDir) * viewBias`，`viewBias` 默认 0；`viewDir` 是相机→表面方向。
  这是 JCGT 2021 讨论的 “self-shadow bias” 思路：沿法线偏移可能把查询点推进薄几何，沿视线反向偏移更稳；
  v1 默认只开法线偏置，`viewBias` 作为 A/B 开关。
- 用 `wpB` 做网格坐标、`toSurface`、depth 比较；`wpB` 夹到网格范围。
- 可见性：`diff = max(0, dist - mean - biasEps)`（`biasEps` 默认 0.01*spacing）；
  `dot(probePos - wpB, n) <= 0` 的探针权重乘 0.5（或 0，按 A/B）。
- 目标：球体轮廓与墙角的漏光/黑边减少；平面墙面不被过度压暗。调参用 `normalBias` 扫描 0/0.05/0.1/0.2×spacing。

### 10.6 任务分解与验收

#### Stage 2d — 异步更新（2026-11-21 v1 已实现并验收，软件解析路径）

| 项 | 内容 |
|---|---|
| 状态 | **done（软件路径）**：增量更新、budget 环绕、temporal alpha、inactive 跳过、切光重置均已实测；HW ray-query 变体未做（本机无可验证硬件），脏表/压实未做 |
| 改 | `tests/cornellBox/cornellGpuRt.cpp/.h`（binding 15 ProbeState、96 B UBO、update 管线、dispatch+barrier、回读）；`tests/cornellBox/main.cpp`（env/帧调度/切光重传）；`assets/shaders/cornellRayLib.glsl`（新增，渲染与更新共用的场景/着色/探针采样）；`assets/shaders/gn_cornell_rt.comp`（改为 include 公共库，输出逐像素不变） |
| 新 shader | `gn_cornell_probe_update.comp`（软件解析路径）；`_rq` 变体未做 |
| 开关 | **`CORNELL_PROBE_ASYNC`：设了 `CORNELL_PROBES` 时默认开启**（未显式给该变量）；`=0` 退回静态烘焙。`CORNELL_PROBE_UPDATE_BUDGET`（默认 64，0=每帧全量）；`CORNELL_PROBE_SPP`（默认 4）；`CORNELL_PROBE_HYSTERESIS`（默认 0.9） |
| 校验开关 | `CORNELL_PROBE_ASYNC_VERIFY=1` + `CORNELL_PROBE_ASYNC_FRAMES=N` + `CORNELL_PROBE_ASYNC_DUMP=<prefix>`：跑 N 帧后回读 SH9/state，打印 hash/dcMean/ages 并落盘 |
| 验收证据 | see §10.7 表下的「2d 实测」 |

#### Stage 2e — Probe relocation（2026-11-21 v1-lite 已接线，视觉待定）

| 项 | 内容 |
|---|---|
| 改 | `cornellGpuRt.cpp/.h`（offset SSBO binding 14）；`gn_cornell_rt.comp`/`_rq.comp`（角点 offset + 背面权重）；`main.cpp`（开关） |
| 算法 | **v1-lite**：`src/graphics/GN_3DprobeRelocation.*` 从 depth moments 统计“短命中”方向，反向推开并逐轴 clamp（`0.5×spacing`）；**不是**原规划的 compute 6 轴候选/RT 重测 |
| 开关 | `CORNELL_PROBE_RELOCATION=1`（默认 0）；`CORNELL_PROBE_RELOCATION_MAX` 尚未接（固定 0.5×spacing） |
| 验收 | CPU 单测：`tests/unit/testProbeRelocation.cpp` 4 例（非法/空/贴墙/clamp）；GPU：llvmpipe 出图，日志 `moved=693/12672 max=0.114`；A/B 9 例 mean|Δ|≈0.0017/255、max=1 → **视觉收益未判定** |

> **结论**：v1-lite 只证明 offset 数据链路（CPU 计算 → SSBO → shader 角点偏移）可用；
> 693 个被移动探针在当前 Cornell 画面里影响很小。
>
> **GPU 6 轴候选（B3b-full，2026-11-21 已实现）**：新增 `assets/shaders/gn_cornell_probe_relocation.comp`
> （local_size 6，每 workgroup 1 探针；候选 = base ± axis*maxOffset，分数 = 6 条轴向短射线命中率；
> 改善 > margin 才走单帧步长，逐轴 clamp 到 ±0.5×spacing 与网格边界；offset 变化即 age=0）。
> 宿主 `CORNELL_PROBE_RELOCATION_GPU=1` 接管上传期 CPU v1-lite，并把 relocation 排在 update 之后、
> 用刚写入的 depth。实测（lavapipe，3 帧 BUDGET=0）：`moved=674/12672`、`maxLen=0.11400`（=0.5×spacing）、
> `clamp_violations=0`、`nan=0`、`class{active:11998, relocated:674}`、两次运行 offset 文件**逐位一致**；
> 与 CPU v1-lite 的 `moved=693 max=0.1140` 同量级。**视觉收益仍未判定**（A/B 差异 ~0.0017/255 量级）。

#### Stage 2f — 法线偏置（2026-11-21 已接线，视觉待定）

| 项 | 内容 |
|---|---|
| 改 | `gn_cornell_rt.comp`/`_rq.comp`（`wpB`、normalBias、背面降权）；`main.cpp`（env/调参）；`cornellGpuRt`（UBO `weight.z`） |
| 开关 | `CORNELL_PROBE_NORMAL_BIAS`（×spacing，**默认 0=关**；建议 0.1，需 A/B）；`CORNELL_PROBE_VIEW_BIAS` 预留未接 |
| 验收 | `probe.bias.corner`（墙角/球体轮廓像素漏光减少，A/B 可判定）；`probe.bias.flat`（平面墙面亮度变化 ≤ 阈值）；`probe.bias.clamp`（偏置钳制；NaN/越界安全） |

> **已接线证据（2026-11-21）**：`make shaders` = 45/45；`make cornellBox` 0 error；
> llvmpipe + `CORNELL_PROBES=1 CORNELL_RT_CAPTURE=1` 成功出图，日志 `probe upload ok normalBias=0.0228/0.1140`。
> A/B（bias 0 vs 0.5×spacing）全图 mean |Δ|≈0.010/255、max=1、pct>2=0 —— **当前 Cornell 资产下法线偏置的
> 视觉收益不可判定**。结论：代码通路正确，但“改善球体边缘/墙角”需要 2e relocation 或更极端的暗角样本；
> 不得仅凭本 A/B 声称已改善。

**依赖**：2d → 2e/2f；2f 可在 2d 后与 2e 并行（只改查询，不改更新）。2e 的 relocation 输入依赖 2d 的 depth 双缓冲。
**可回退**：每个阶段独立 env 开关；关闭即回退静态资产 + 原查询，不改默认路径。

### 10.7 验收矩阵（可执行）

| 检查 | 命令/观察 | 通过标准 | 阶段 |
|---|---|---|---|
| GPV4 兼容 | 现有 asset 加载 + `CORNELL_PROBES=1` | 画面/MAE 与改动前一致；无新崩溃 | 2d–2f |
| 更新预算 | 日志/SSBO 回读 age | 每帧 budget 个探针 age+1；总数不超 budget | 2d |
| 非阻塞 | 代码检查 + 帧时间 | 更新路径无 `vkWaitForFences`/`vkDeviceWaitIdle`；帧时间记录 | 2d |
| 收敛 | A/B 与静态资产 | 120 帧后中心区域 MAE ≤ 阈值（按实际测） | 2d |
| 球内探针 | 合成场景/CPU 单测 | relocation offset >0，class 正确，无 NaN | 2e |
| GPU 6 轴 relocation | `CORNELL_PROBE_RELOCATION_GPU=1 ..._ASYNC_VERIFY=1 ..._FRAMES=3` | 实测：`moved=674/12672`、逐轴 `max=0.11400`（=0.5×spacing）、`clamp_violations=0`、`nan=0`、两次运行 offset `.reloc.bin` 逐位一致；默认路径截图仍 `ebe115fd…` | 2e |
| 球体边缘 | Cornell 截图 A/B | 轮廓黑边/漏光减少；用户目视 | 2e/2f |
| 墙角 | Cornell 截图 A/B | 跨墙串色减少；平面不过暗 | 2e/2f |
| 稳定性 | 连续 100–300 帧 | offset/SH9 不振荡；切光后 age 归零 | 2e |
| 帧时间 | llvmpipe + 真机 | 只记录实测；预算可调至不超预算 | 2d–2f |
| 默认路径 | `CORNELL_PROBES` 未设置 | 全部新逻辑不执行，画面不变 | 2d–2f |

**CPU 单测**（不依赖 GPU）：relocation 的 clamp/候选选择、法线偏置的钳制、UBO 字段打包、
`inactive` 权重为 0、NaN 输入安全。已落地：`tests/unit/testProbeUpdate.cpp` 增 `probeUpdateUboLayout`、
`probeUpdateStateAgeAndReset`（`make unit` = 415/0）。
**2d 实测（2026-11-21，lavapipe / llvmpipe，640×400，spp=4）**：

| 检查 | 命令 | 实测结果 |
|---|---|---|
| parity（预算分批 vs 一次全量） | `CORNELL_PROBE_ASYNC=1 CORNELL_PROBE_UPDATE_BUDGET=0|64 CORNELL_PROBE_ASYNC_VERIFY=1 CORNELL_PROBE_ASYNC_FRAMES=1|198` | 两者 SH9 回读 hash 均 = `f2447cc68304ca6f`（**逐位一致**）；198 帧后 ages 全 1 |
| age 递增 | `..._ASYNC_FRAMES=3 BUDGET=0` | ages{zero:0,one:0,max:3}，3 轮后 hash 变化、dcMean 仍 0.979943 |
| 收敛（对静态资产） | 同上 + 对 `assets/probes/cornell_probe_0_24x22x24_4x16.bin` | DC 均值 `0.979943` vs 烘焙 `0.979817`（相对差 1.3e-4）；逐探针 DC MAE `0.063`（6.4%，来自 4 spp + 资产 3×3×3 平滑，更新路径未平滑） |
| 非阻塞 | `grep vkWaitForFences/vkDeviceWaitIdle cornellGpuRt.cpp` | dispatch 区间（`nextProbeUpdateRange` … barrier）内 0 处；仅 shutdown/drain/上传处有 |
| 默认路径 | `CORNELL_GPU_RT=1 CORNELL_PROBES=1 CORNELL_RT_CAPTURE=1`（async 未设） | 截图 sha256 `ebe115fd…` 与改动前**逐字节一致** |
| inactive 跳过 | `..._ASYNC_FRAMES=1 BUDGET=0 CORNELL_PROBE_TEST_INACTIVE=100` | 前 100 探针 `class=2`、`age=0`、SH 与资产**逐值相等**；其余 12572 个 age=1 且 SH 改变；hash 与无 inactive 运行不同 |
| 切光 age 归零 | `CORNELL_AUTO_LIGHT_SWITCH=1 ..._ASYNC_FRAMES=5 BUDGET=0` vs 同参数不切光 | 切光（第 3 帧 `probe upload[1]`）后最终 ages max=**3**；不切光同 6 帧为 max=**6**；dcMean 0.897095（point 光照）≠ 0.979943（sun） |
| 着色器重构 | `make shaders` + 同一条截图命令 | 46/46 编译；`gn_cornell_rt.comp` 改 include 前后截图逐字节一致 |

### 10.8 风险与回退

| 风险 | 影响 | 缓解/回退 |
|---|---|---|
| 软件 RT/llvmpipe 更新太慢 | 帧时间上升 | 预算可调；默认关；无 GPU 时回退静态资产 |
| 双缓冲/屏障写错 | 画面撕裂/校验层报错 | 双缓冲 + 帧边界 barrier；校验层跑 2d 验收 |
| relocation 振荡 | 探针抖动/闪烁 | maxOffset、单帧步长、score margin、age 重置、100 帧稳定性检查 |
| relocation 把探针推出网格/重叠 | 采样空洞 | 逐轴 clamp；inactive 兜底；v2 再做去重 |
| normal bias 推穿薄几何 | 漏光/暗边 | 钳制到 0.5×spacing；`viewBias` 默认 0；A/B 平面不过暗 |
| 切光/资产切换残留 | 旧 GI 鬼影 | 切光即 age=0、offset=0、frame=0；静态资产 warm start |
| 内存增加 ~22.5 MB | 低端设备压力 | 启动打印预算；`CORNELL_PROBE_ASYNC=0` 不分配双缓冲 |
| 动态物体 | Cornell 静态场景无 TLAS refit | 明确非目标；动态场景另立任务 |
| 语义偏差 | 预览不再是严格 reference | 保留 `CORNELL_PROBES=0` 对拍；文档写明近似 |

### 10.9 参考资料与适用边界

| 资料 | 用途 | 关键结论 | 适用边界 |
|---|---|---|---|
| [Majercik et al., DDGI, JCGT 8(2), 2019](https://jcgt.org/published/0008/02/01/) | DDGI 原始论文 | 动态辐照度场 + 可见性感知的 moment 插值；探针可运行时更新 | 论文假设 GPU RT；本项目的 Cornell 软件 RT 是降级实现 |
| [Majercik et al., Scaling Probe-Based Real-Time Dynamic Global Illumination for Production, JCGT 10(2), 2021](https://www.jcgt.org/published/0010/02/01/)（[arXiv](https://arxiv.org/abs/2009.10796v3)） | 生产化扩展 | “self-shadow” bias 单参数、GI 过渡加速、探针状态机、级联卷 | 面向 RTXGI/UE4/Unity；参数需按本项目调 |
| [NVIDIA RTXGI DDGI](https://github.com/NVIDIAGameWorks/RTXGI-DDGI) | 权威实现 | 探针 relocation / classification / 更新分帧；offset 存在 probe data 中，查询时加到世界位置 | SDK 实现；纹理/卷布局与 Cornell SSBO 不同，只借算法 |
| [NVIDIA RTXGI 产品页](https://developer.nvidia.com/rtxgi) | 特性概览 | Probe Relocation、Probe State Classification、perceptual encoding | 产品/概览页，不含完整公式 |
| [Roháček, Improving probes in DDGI, 2022](https://cescg.org/wp-content/uploads/2022/04/Rohacek-Improving-Probes-in-Dynamic-Diffuse-Global-Illumination.pdf) | 实现参考 | relocation 后的三线性权重/offset 处理；深度采样 | 硕士论文/二手实现，优先级低于论文与 SDK |

**边界**：以上资料解决“怎么更新/怎么移/怎么偏置”，不解决本项目的 llvmpipe 性能、Cornell 单卷适配、
GPV4 资产兼容；这些必须由 §10.7 的实测验收决定，不能只靠引用。

---

## 11. 任务登记（2026-11-21）

| 任务 | 状态 | 依赖 | 验收入口 |
|---|---|---|---|
| Stage 2d DDGI 异步更新 | **计划（未实现）** | 2a/2b/2c 已完成 | `CORNELL_PROBE_ASYNC=1` + §10.7 |
| Stage 2e Probe relocation | **计划（未实现）** | 2d（depth 双缓冲） | `CORNELL_PROBE_RELOCATION=1` + §10.7 |
| Stage 2f 法线偏置 | **计划（未实现）** | 2d（可与 2e 并行） | `CORNELL_PROBE_NORMAL_BIAS=0.1` + §10.7 |
| Stage 2g 引擎 G5 推广 | 可选/未排期 | 2d–2f | 另立规划，不与本 Cornell 任务混用 |

**建议顺序**：2d（先让数据动起来）→ 2f（低风险、直接改善轮廓）→ 2e（relocation 需要更多稳定性验证）。
若球体边缘/墙角是首要目标，2f 可先于 2e；relocation 的收益主要在“探针落在几何内”的场景。

