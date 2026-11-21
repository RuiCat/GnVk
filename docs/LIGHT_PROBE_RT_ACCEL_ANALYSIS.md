# 光照探针能否加速 Cornell Box 光追预览 — 可行性分析

> 日期：2026-11-20
> 基线：`HEAD = ffdf0bb` + 工作区未提交修改（着色器审计修复）
> 场景：`tests/cornellBox` 的 T/Y/U 光追预览（CPU / GPU compute / HW ray query）
> 性质：**静态分析 + 精确计数**。本会话无 GPU（`/dev/dri` 不存在），所有毫秒数都是估算，
> 射线/采样次数是可复算的精确值。
>
> **实施状态（2026-11-20）**：Stage 0 + Stage 1 已落地（FPS/分阶段计时、移动 4 / 静止 16 spp、
> 收敛即停、collect → 上传 → submit 单帧重叠、无新帧跳过 DataFill）；Stage 2 探针未做。
> 见 `docs/CHANGELOG.md`。仍无真机 FPS 数据。

---

## 0. 结论先行

1. **能实现，而且 Cornell 是最适合探针的场景**：把探针当 irradiance cache（辐照度缓存）用，
   替代路径追踪的第 2/3 次漫反射弹射，每样本 trace 次数从 3（太阳）或 6（点光）降到 1 或 2，
   理论射线工作量约 **1/3**；金属球保留 1 次镜面反射。
2. **但探针只省「间接弹射」**，不省以下三项，而它们和弹射是叠加的：
   - 每帧固定 16 spp，即使相机静止、画面已收敛也照跑 16 spp；
   - compute 之后 `vkWaitForFences` 每帧阻塞 CPU 等 GPU；
   - 每帧 1 MB 回读 + `sIMAGE::DataFill` 的第二次 submit/wait。
   只做探针，乐观估计 1.5–2×；把同步和采样一起改，才可能 3× 以上。
3. **现有 G5 探针不能直接拿来用**：8×4×8、间距 2 m，对 5.5×5.0×5.5 的 Cornell 只有约
   3×3×3 个有效格；数据是启动时天空 SH9 填满的常量，不是场景辐照度；而且只接在引擎延迟
   路径的描述符上，不接 RT compute 管线。
4. **建议顺序**：先量（FPS + 分阶段计时）→ 再修管线（自适应 spp / 收敛即停 / 异步回读）
   → 最后烘探针。探针是「画质换速度」里最贵的一步，不该第一个做。

---

## 1. 现状取证

### 1.1 光追预览的固定开销

| 事实 | 证据 |
|---|---|
| 预览分辨率固定 640×400 | `tests/cornellBox/main.cpp:22-23` |
| 每帧固定 16 spp（GPU/HW），CPU 8 spp | `tests/cornellBox/main.cpp:24-25`；`cornellCpuRt.cpp:22` |
| 每样本 3 次弹射 | `assets/shaders/gn_cornell_rt.comp:37,186`；`gn_cornell_rq.comp:43,179`；`cornellCpuRt.cpp:23` |
| 每帧 dispatch 后立即等 fence | `tests/cornellBox/cornellGpuRt.cpp:454,462` |
| 等完立刻 memcpy 1 MB 回读 | `cornellGpuRt.cpp:468`（640×400×4 B） |
| 相机静止仍每帧跑 16 spp，直到 32768 样本 | `cornellGpuRt.cpp:81,438`（32768/16 = 2048 帧） |
| 回读后每帧 `DataFill` 上传纹理 | `tests/cornellBox/main.cpp:227` |
| `DataFill` 对已有图像走 staging + submit + **阻塞等 fence** | `src/graphics/_Vulkan/vkImageData.cpp:304-305` |
| 全仓没有 GPU timestamp / FPS 计数 | `grep -rn "vkCmdWriteTimestamp\|VK_QUERY_TYPE_TIMESTAMP" src` 无命中 |

所以「目测卡」至少由四段组成：**路径追踪 compute + 两次全同步 + 1 MB 回读/上传 + 16 spp 不随画面状态变化**。
探针只作用于第一段里的「弹射部分」。

### 1.2 现有探针实现的状态

- G5a/G5b 已完成（编译级）：`src/graphics/GN_3Dprobe.h/.cpp`、`src/graphics/_Vulkan/vkProbe.cpp`。
- 数据布局：8×4×8 = 256 探针，9 张 3D `RGBA16F` 纹理，每张存 1 个 SH 系数，硬件三线性。
- 默认 `weight=0`，shader 不采样；没有任何应用调用 `GN_VkSetProbeWeight`。
- 初始化时 256 个探针全部写成**同一份环境 SH9**；`GN_VkUpdateProbes` 每帧最多把 2 个探针
  向同一份环境 SH9 混合 0.15。也就是说：**探针里没有场景的局部反弹光**。
- 只在引擎延迟光照描述符（binding 14..23）里；RT 预览是独立 compute 管线，看不到它。

证据：`src/graphics/_Vulkan/vkProbe.cpp:1-20`（文件头注释与初始化）、
`assets/shaders/gn3d_deferred_light.frag:118-156`（`sampleProbeSh` 9 次 fetch）、
`docs/GI_PLAN.md:342-369`（G5 设计与限制）。

---

## 2. 成本模型（精确计数，非实测毫秒）

Cornell 场景：5 个四边形 + 2 个球 = **7 个图元**（`cornellGeometry.cpp:118-151`，
`areaLight=false`）。每个 `trace()` 线性测试这 7 个图元（`gn_cornell_rt.comp:122-150`）。

| 量 | 当前（太阳模式） | 当前（点光模式） | 探针版（太阳） | 探针版（点光） |
|---|---:|---:|---:|---:|
| 像素 | 256,000 | 256,000 | 256,000 | 256,000 |
| 每帧路径（16 spp） | 4,096,000 | 4,096,000 | 4,096,000 | 4,096,000 |
| trace / path | 3 | 3 + ≤3 阴影 = ≤6 | 1 | 1 + 1 阴影 = 2 |
| trace / frame | 12,288,000 | ≤24,576,000 | 4,096,000 | ≤8,192,000 |
| 图元测试 / frame | 86,016,000 | ≤172,032,000 | 28,672,000 | ≤57,344,000 |
| 探针 3D fetch / frame | 0 | 0 | 36,864,000（9×16spp×256k） | 同左 |

- 太阳模式：trace 数 **3×** 下降，图元测试同比例。
- 点光模式：trace 数 **≤3×** 下降（阴影射线保留，但弹射射线全砍）。
- 代价：每样本 9 次 3D 纹理 fetch。硬件三线性、网格只有几十 KB，命中率会很高，但不是免费；
  必须实测，不能拿「少 2/3 射线」直接当帧率结论。

> 这些是计数，不是帧时间。本会话没有 GPU，任何「能到多少 FPS」都必须真机量。

---

## 3. 探针方案设计（针对 Cornell）

### 3.1 网格

- 房间 5.5×5.0×5.5 m（`cornellGeometry.h:14-16`）。建议 10×9×10 = **900 探针**，
  轴间距约 0.55 m，边界留 0.25 m 余量（探针不要落在墙面上）。
- 900 ≤ 现有 `PROBE_GRID_MAX_PROBES=1024`，可以先不改上限；若要 12×11×12 再放宽到 2048。
- 数据量：900 × 36 float × 2 B ≈ 65 KB（9 张 10×9×10 `RGBA16F`），可忽略。

### 3.2 烘焙

两条路，推荐第二条：

1. **方向采样直接投 SH9**：每探针 256–512 条余弦分布射线，路径追踪 3 次弹射，
   按 SH 基函数累加。实现简单，但要自己保证 SH 系数约定（A_l=π, 2π/3, π/4）。
2. **6 面 cubemap + 现成投影**：每探针渲染 6 面 32×32（或 16×16），64 spp，3 次弹射，
   再用已经在用的 `giParams::shProjectCubemap` 投影（`src/graphics/giParams.h`，有单测）。
   复用已验证的 SH 约定，风险低。

烘焙量按选项不同（900 探针 × 6 面 × 3 弹射）：

| 面分辨率 | 每面 spp | trace 数 | 备注 |
|---|---:|---:|---|
| 16×16 | 32 | ≈ 133M | SH9 只需要低频，推荐先试这档 |
| 16×16 | 64 | ≈ 265M | 更平滑 |
| 32×32 | 32 | ≈ 531M | 细节更稳 |
| 32×32 | 64 | ≈ 1.06B | CPU 烘会到分钟级 |

GPU compute 一次（预计秒级到十几秒；本会话无法验证），CPU 需要分钟级。烘焙只在启动/切换光照模式时做；
太阳与点光各缓存一份，按 `K` 键切换时换用，避免每次重烘。

### 3.3 运行时着色

在 `radiance()` 里（`gn_cornell_rt.comp:186` 的循环）：

- 第 1 次命中：保留 `shadeDirect()`（太阳解析 / 点光 NEE + 1 条阴影射线）。
- 漫反射间接：不再发射第 2/3 次余弦射线，改为按命中点世界坐标三线性采样探针 SH9，
  得到 irradiance，乘 albedo/π。
- 金属（`metallic > 0.5`）：保留 1 次镜面反射射线（这是 Cornell 金属球的观感来源），
  反射命中后再按同一规则取直接光 + 探针间接；不再继续多次镜面弹射。
- 天空 miss：保留 `skyColor()`。

多 bounce 的能量已经烘进探针，所以运行时 1 次弹射在能量上是近似正确的；误差主要来自
SH9 的低频截断和 0.55 m 网格的空间插值。

### 3.4 与现有代码的复用

- CPU 端 `cPROBE_GRID` / `packProbeTextures`（`GN_3Dprobe.h/.cpp`）可直接复用，只需把
  counts 换成 10×9×10。
- 纹理创建/上传可参考 `vkProbe.cpp` 的 9 张 3D 纹理写法；RT 预览需要在自己独立的
  descriptor set 里加 9 个 `COMBINED_IMAGE_SAMPLER`（binding 4..12），不要复用引擎的延迟 set。
- 引擎延迟路径将来也可以吃同一份烘焙数据（现有 `sampleProbeSh` 已接好），
  所以烘焙管线不是只为预览服务。

### 3.5 质量边界

- Cornell 是**封闭凸盒**：盒内任意两点之间除两个球外没有遮挡，所以 irradiance volume 的
  「穿墙漏光」问题在这里最轻；主要误差是球体周围和接触阴影（SH9 + 0.55 m 网格会糊）。
- 金属球靠 1 次镜面射线保留，不依赖探针。
- 探针版预览不再是严格 reference。保留原 PT 模式（或 `CORNELL_PROBES=0`）用于对拍，
  用现有 MAE 工具比较探针版与原 PT 的误差，再决定能否接受。

---

## 4. 风险

| 风险 | 影响 | 处置 |
|---|---|---|
| 只做探针、不修同步 | 可能只快 1.5–2×，仍被 fence/回读卡住 | 先做 Stage 0/1 |
| 9 次 3D fetch × 16 spp | 36.9M fetch/帧，可能吃掉一部分收益 | 实测；必要时降到 4–8 spp 或改成 2–3 张打包纹理 |
| 探针网格太粗 / 球附近漏光 | 接触阴影、球底部间接光偏差 | 加密网格；必要时用 DDGI 的深度矩拒绝跨面探针 |
| 光照模式切换 | 探针烘焙与 lightMode 绑定 | 两份缓存，K 键切换；或切换时重烘 |
| 场景动态化 | 烘焙失效 | Cornell 是静态场景；真要动态走 DDGI |
| 预览失去 reference 身份 | 无法再用它验证引擎 | 保留 `CORNELL_PROBES=0` 原路径 |

DDGI（动态版本）的资料与适用边界：
[Majercik et al., "Dynamic Diffuse Global Illumination with Ray-Traced Irradiance Fields", JCGT 2019](https://jcgt.org/published/0008/02/01/)
——它解决的是动态场景/动态光照下的探针更新与可见性插值；Cornell 静态场景用不上这套复杂度，
但若以后要动态物体，这是标准路线。经典 irradiance caching 可参考 Ward 等人的工作，
本质就是「在稀疏点缓存漫反射辐照度，查询时插值」。

---

## 5. 建议的实施顺序

### Stage 0：先量化（半天以内）

- HUD 增加 FPS / 帧时间（当前没有任何 FPS 显示）。
- 可选 `CORNELL_PROFILE=1`：在 dispatch 前后、回读、`DataFill` 各插一次 CPU 计时；
  有 GPU 后换成 `vkCmdWriteTimestamp`（2 个时间戳即可）。
- 验收：能打印「compute / 等待 / 回读 / 上传 / 总计」五项，连续 120 帧取中位数。

### Stage 1：管线修复（1–2 天，收益最确定）

- **自适应 spp**：相机移动时 2–4 spp，静止时 16 spp 累积。
- **收敛即停**：达到目标样本数后不再 dispatch、不再回读，直接复用上一张纹理。
- **异步回读**：2–3 帧 in-flight，提交 N 帧、回读 N-1 帧，去掉每帧 fence 阻塞。
- **持久 staging**：`DataFill` 每帧重新 `CreateBuf`/`vkAllocateMemory`/`vkWaitForFences`
  是纯开销；改成复用一个 staging buffer + 每帧一次 copy。
- 验收：移动时 FPS 相对基线 ≥2×；静止收敛后 GPU 空闲、画面不闪；截图与基线逐像素接近。

### Stage 2：探针烘焙 + 着色（2–4 天）

- 实现 3.2 的烘焙 compute（或先用 CPU 烘一次验证数值），输出 9 张 3D 纹理。
- 改 `gn_cornell_rt.comp` / `gn_cornell_rq.comp`：第 2/3 次弹射换成探针采样；
  金属保留 1 次镜面反射。
- 加 `CORNELL_PROBES=1` 开关；关掉时逐像素回到原路径。
- 验收：trace 计数从 3/6 降到 1/2（可在 shader 里计数或看 SPIR-V）；探针版与原 PT 的
  MAE 在可接受阈值内；移动 FPS 相对 Stage 1 再提升（实测，不预设倍数）。

### Stage 3（可选）：DDGI

只有需要动态物体/动态光照时才做；compute dispatch 基础设施已有，但工作量远大于 Stage 2。

---

## 6. 复算命令

```bash
# 射线计数：640*400*16*(3 或 1) ；图元测试再乘 7
python3 - <<'PY'
px = 640*400; spp = 16; prims = 7
print("paths/frame       =", px*spp)
print("sun traces/frame  =", px*spp*3, "-> prim tests", px*spp*3*prims)
print("point traces/frame=", px*spp*6, "-> prim tests", px*spp*6*prims)
print("probe sun traces  =", px*spp*1, "-> prim tests", px*spp*1*prims)
print("probe point traces=", px*spp*2, "-> prim tests", px*spp*2*prims)
PY

# 现状取证
grep -n "kRtWidth\|kRtHeight\|kGpuRtSamples" tests/cornellBox/main.cpp
grep -n "vkCmdDispatch\|vkWaitForFences\|memcpy(outRgba" tests/cornellBox/cornellGpuRt.cpp
grep -n "DataFill" tests/cornellBox/main.cpp
grep -n "vkWaitForFences" src/graphics/_Vulkan/vkImageData.cpp

# 确认没有 GPU 计时
grep -rn "vkCmdWriteTimestamp\|VK_QUERY_TYPE_TIMESTAMP" src || echo "no timestamp queries"
```

---

## 7. 未验证声明

- 本会话 `/dev/dri` 不存在，未能运行 `cornellBox`、`vulkaninfo` 或任何 GPU 计时。
- 文中所有「毫秒/倍数」都是基于射线计数的估算，不是实测帧时间；必须按 Stage 0 在真机
  （上次记录为 AMD 780M / RADV）上量过再下结论。
- 探针烘焙的 SH 约定必须复用 `giParams::shProjectCubemap`，不要另写一套；否则会重演
  「irradiance 已含余弦卷积、Lambert 还要 /π」的尺度错误（见 `.agent-memory/topics/gi-params.md`
  的 M-gi-params-3）。
