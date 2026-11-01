# L4 大气散射 + L3 阴影收尾：实现契约（2026-10-31 冻结）

> 性质：**冻结契约**。来源规划 `docs/LIGHTING_PLAN.md` §4.5（L3）/ §4.6（L4）。
> 本文件只写各执行体之间的接口口径（UBO 布局 / 绑定槽 / API 语义 / 公式 / 验收），
> 不重复规划文档里的背景与论文说明。改动本文件需同步 `GN_Vk.h` / `GN_3Dlight.h` 里的
> `static_assert`，并让三个 shader 的 UBO 声明与之一致。

## 0. 全局口径

- 世界空间 **y-up**；地面平面 `y=0`，行星中心 `(0, -Rg, 0)`。
- `sunDir` = 世界空间**单位**向量，**指向太阳**（与 `VkSkyParams.sunDirection` 同向）。
- 所有辐亮度都在**线性 HDR** 域；曝光由 `gn3d_composite.frag` 统一施加，天空/空气透视着色器不再乘曝光。
- 默认全部可回退：`VkSkyParams.atmosphereMix=0` ⇒ 旧解析天空；`sFOG_UBO.aAtmosphere2.w=0` ⇒ 旧指数雾；
  `sLIGHT_UBO.shadowParams[1]=1` ⇒ 单级阴影（旧行为）。
- 本机无 `/dev/dri`：所有 GPU 行为只能做到**编译级**（`make` / `make shaders` / `make gpuCheck` 编译），
  视觉验收只能在桌面机执行；不得把编译通过写成"已验证可用"。

## 1. 大气 LUT（执行体 A：`src/graphics/_Vulkan/vkAtmosphereLut.cpp` + `assets/shaders/gn3d_skyview.comp`）

- 透射率 LUT：256×64 RGBA16F，**静态**，启动时 CPU 生成上传（现有实现保留）。
  参数化：`u = mu*0.5 + 0.5`，`v = height01`（0=地面、1=大气顶）；与 `evalTransmittanceLut` 一致。
- sky-view LUT：192×108 RGBA16F，**GPU compute 按需重建**（太阳方向或大气参数变化时）。
  - 参数化必须与 CPU `computeSkyViewLut`（`src/graphics/atmosphereParams.cpp:344-371`）逐 texel 一致：
    `u=(x+0.5)/W`、`v=(y+0.5)/H`；`mu = 1 - v`（`dir.y`）；`delta = (u*2-1)*PI`；
    `azimuth = atan2(sun.x, sun.z) + delta`；`dir = (sinθ*sin(azimuth), mu, sinθ*cos(azimuth))`。
  - 辐亮度模型：单次散射（Rayleigh + Mie 相位、太阳透射率查表、地面遮挡返回 0），直接移植
    `computeSkyRadiance`（`atmosphereParams.cpp`）。
  - compute 布局：`local_size_x=8, local_size_y=8`，dispatch 覆盖 192×108；
    set0 binding 0 = `sampler2D` 透射率 LUT，binding 1 = `image2D`（`r16g16b16a16f`、write）sky-view LUT，
    binding 2 = UBO（太阳方向 + 大气参数，std140，`vec4` 对齐）。
  - 图像 usage 需含 `STORAGE_BIT`；dispatch 前 barrier 到 `GENERAL`，之后 barrier 回 `SHADER_READ_ONLY_OPTIMAL`。
- `recordAtmosphereLutUpdate(cb)`：仅在 dirty 时录制 dispatch；**必须在 render pass 之外调用**。
  dirty 判据：`GN_VkSetAtmosphere` 收到的太阳方向与上次点积 `< 0.9995` 或参数逐字段不等。
- 数值验收：gpuCheck `atmo.skyview` 回读 GPU LUT 与 CPU `computeSkyViewLut` 同尺寸参考采样比对（容差 2%）。

## 2. API（声明已由 Lead 写入 `GN_Vk.h`）

```cpp
void GN_VkSetAtmosphere(const GN3d::sATMOSPHERE_PARAMS &params, const float sunDir[3]);
bool GN_VkGetAtmosphereLutViews(VkImageView *transmittanceView, VkImageView *skyView,
                                VkSampler *transmittanceSampler, VkSampler *skySampler);
void GN_VkShadowPassBegin(const sMAT44 *lightViewProj, uint32_t cascadeIndex = 0);
```

- `GN_VkSetAtmosphere`：归一化 sunDir、`validateAtmosphereParams`、太阳强度钳到 `>=0`；未初始化安全忽略（不写越界）。
- `GN_VkGetAtmosphereLutViews`：LUT 未就绪返回 false，不写输出。
- `GN_VkShadowPassBegin`：`cascadeIndex` 越界钳到 3；每级一个 2048² 深度 framebuffer，绘制同一批投影网格。

## 3. UBO 布局（冻结；C++ 与 GLSL 逐字段一致，`std140`）

### 3.1 `sFOG_UBO`（64 B，`GN_Vk.h`）

| 偏移 | 字段 | 语义 |
|---|---|---|
| 0 | `float aColor[4]` | rgb=雾色，a=保留 |
| 16 | `float aParams[4]` | x=density(1/m) y=heightFalloff(1/m) z=baseHeight(m) w=maxOpacity（指数雾 fallback） |
| 32 | `float aAtmosphere[4]` | xyz=太阳方向（单位、指向太阳），w=太阳强度 |
| 48 | `float aAtmosphere2[4]` | x=地面半径(m) y=大气顶半径(m) z=相机高度(m) w=空气透视开关(0/1) |

GLSL 声明：`gn3d_pbr.frag` binding 6、`gn3d_deferred_light.frag` binding 8 扩为 4 个 `vec4`；
`gn3d.frag` binding 3 保持 32 B 声明（不读新字段，前向路径仍用指数雾）。

### 3.2 `sLIGHT_UBO`（976 B，`GN_3Dlight.h`）

| 偏移 | 字段 | 语义 |
|---|---|---|
| 0 | `sLIGHT_GPU lights[8]` | 640 B，不变 |
| 640 | `float ambientLight[4]` | 16 B |
| 656 | `float lightCount; float _pad[3]` | 16 B |
| 672 | `float shadowLightVP[4][16]` | 256 B（4×mat4，每级 64 B），每级光源 VP（列主序） |
| 928 | `float shadowParams[4]` | x=阴影图分辨率(2048)，y=级数(1..4)，z/w=0 |
| 944 | `float cascadeSplits[4]` | 各级视距分割（米，升序） |
| 960 | `float shadowTexelWorld[4]` | 各级 texel 世界尺寸（米/texel） |

GLSL：三个着色器的 `LightUBO` 末尾改为 `mat4 shadowLightVP[4]; vec4 shadowParams; vec4 cascadeSplits; vec4 shadowTexelWorld;`（std140 下 `mat4[4]` = 256 B，与 C++ `float[4][16]` 一致）。

### 3.3 天空 UBO（144 B，`VkSkyParams` 80 B）

`mat4 viewProj` + `vec4 sunDirection` + `vec4 horizonColor` + `vec4 zenithColor` + `vec4 timeParams`
+ `vec4 atmosphereParams`（x=atmosphereMix 0/1，y=辐亮度倍率，z/w=0）。

## 4. 绑定槽（冻结）

| 管线 | 槽位 | 内容 |
|---|---|---|
| 天空盒 set0 | 0 / 1 | UBO(144 B) / 静态 cubemap（片元未用，保留） |
| 天空盒 set0 | **2 / 3** | 透射率 LUT `sampler2D` / sky-view LUT `sampler2D` |
| PBR set0 | **10 / 11** | 透射率 LUT / sky-view LUT `sampler2D`（0..9 已占） |
| PBR set0 | 4 | 阴影：`sampler2DArray`（0..3 层；类型由 `sampler2D` 改为数组） |
| 延迟 set0 | **24 / 25** | 透射率 LUT / sky-view LUT `sampler2D`（0..23 已占，GI 槽位不动） |
| 延迟 set0 | 5 | 阴影：`sampler2DArray` |

描述符池/布局/写入由 Lead（接线）负责；着色器只按上表声明。

## 5. 天空着色器（执行体 B：`assets/shaders/gn3d_skybox.frag`）

- `atmosphereParams.x >= 0.5` 时基色改为采样 sky-view LUT：
  `azSun = atan2(sky.sunDirection.x, sky.sunDirection.z)`，`az = atan2(dir.x, dir.z)`，
  `delta = wrap(az - azSun, -PI, PI)`，`u = delta/(2*PI) + 0.5`，`v = 1 - dir.y`；
  采样值乘 `atmosphereParams.y`。云、太阳盘、haze/below 处理沿用现有顺序（LUT 基色 → 云 → 太阳盘 → haze/below），
  地平线以下不得出现纯黑（回落 `horizonColor`）。
- `atmosphereParams.x < 0.5` ⇒ 保持现有解析渐变（旧路径逐行不动）。
- 太阳圆盘/光晕保留解析式，不额外乘太阳透射率（v1 记录为简化项）。

## 6. 空气透视（执行体 B：`gn3d_pbr.frag` / `gn3d_deferred_light.frag`）

- 开关：`fog.aAtmosphere2.w > 0.5` 且太阳强度 > 0；否则走原指数雾公式（fallback 原样保留）。
- 大气路径（**8 步**单次散射积分，步数用 `const int AERIAL_STEPS = 8;` 常量，便于后续调）：
  - 相机位置 `camera = scene.cameraPos.xyz`，片元距离 `tMax = length(fragWorldPos - camera)`；
    延迟路径用深度 + `gi.invViewProj` 还原世界位置。
  - 行星中心 `C = (0, -Rg, 0)`；`Rg/ Rt` 取 `fog.aAtmosphere2.x/.y`；相机高度取 `fog.aAtmosphere2.z`（用于 Rg 的等价性，
    即 `C.y = -Rg + 0`，相机实际世界 y 由 `camera.y` 决定）。
  - 每步：`h = max(length(p - C) - Rg, 0)`；密度 `dR = exp(-h/8000)`、`dM = exp(-h/1200)`；
    光学深度增量 `dτ = (σR*dR + σM_ext*dM + σO*hOzone(h)) * ds`，**σM_ext = mieScattering(3.996e-6) + mieAbsorption(4.4e-6) = 8.396e-6**（与 `atmosphereParams.cpp` 的 `mieExtinction` 口径一致；散积只用散射系数）。
  - 太阳透过率：用透射率 LUT（`u = mu*0.5+0.5`、`v = h/(Rt-Rg)`）查表，`mu = dot(normalize(p-C), sunDir)`。
  - 单次散射累加：`S += T_view * (σR*dR*phaseR + σM*dM*phaseM) * sunT * stepLen`；
    相位：Rayleigh `(3/(16π))(1+mu²)`，Mie HG(`mu`，g=0.8)。
  - 结果：`color = litColor * T_view + S * sunIntensity`（`sunIntensity = fog.aAtmosphere.w`）；`T_view` 钳到 `[0,1]`。
- 保持 `fog.aParams[3]`（maxOpacity）对空气透视结果的作用：最终混合上限仍受 maxOpacity 约束，便于与旧路径对照。

## 7. 大气常量（shader 内硬编码，必须与 `atmosphereParams.cpp::makeDefaultAtmosphereParams` 逐值一致）

执行体 B 直接从该函数抄数值（Rayleigh 5.802/13.558/33.1e-6、Hr=8000；Mie 散射 3.996e-6 + 吸收 4.4e-6、
Hm=1200、g=0.8；Ozone 0.65/1.881/0.085e-6、中心 25 km、半宽 15 km；太阳强度 20），不得凭记忆填写。

## 8. CSM（执行体 C：`src/graphics/_Vulkan/vkShadowCascade.cpp/.h` + 查看器）

- 级数默认 4（`cascadeCount` 可设 1 回退单级）；阴影图 2048²×4 层 D32。
- 分割：对数 + 均匀混合（Lauritzen 2011 固定分割），`split[i] = lerp(near*(far/near)^t, near+(far-near)*t, 0.5)`，
  `t=(i+1)/count`；`near=0.1`，`far = max(场景 AABB 对角, 相机到 AABB 最远角距离 + 0.5)`（必须覆盖相机→场景最远距离，否则级联落不到模型上）。
- 每级：相机视锥 8 角点 → 以场景 AABB 求交/包裹 → 光源空间正交矩阵（`Mat44LookAtRH × Mat44OrthoVK`），
  再做 **texel snapping**（L3a：平移取整到该级 texel 世界尺寸的整数倍），输出 `lightViewProj[i]` 与 `texelWorld[i]`。
- 选级（着色器）：对 `i=0..cascadeCount-1` 依近到远算 `p = shadowLightVP[i] * vec4(worldPos,1)`，
  要求 `p.w>0`，`ndc = p.xyz/p.w`，`uv = ndc.xy*0.5+0.5`，`uv` 在 [0,1] 且 `ndc.z` 在 [0,1] ⇒ 第一个命中级；都不命中 ⇒ 无阴影。
- 每帧查看器：`for i in 0..count-1: GN_VkShadowPassBegin(&vp[i], i); DrawShadow(); GN_VkShadowPassEnd();`
- 阴影采样：PCSS 逻辑保留，采样器改 `sampler2DArray`，层 = 选中级；法线偏移用 `shadowTexelWorld[cascade]`。
- 验收：`gpuCheck 3d.shadow` 改为断言"4 层深度都被写入（每层非全清）"；`3d.shadow.cascade` 断言近级覆盖中心、
  远级覆盖更大范围（具体数值由执行体 C 定并写进测试注释）。

## 9. 环境烘焙同源（Lead 接线）

`vkEnvironment` 使用同一 `gn3d_skybox` 管线烘焙：烘焙描述符集补 2/3 槽（LUT）与 144 B UBO；
`init3DAtmosphereLut`（`GN_Vk.cpp:1085`）先于 `init3DEnvironment`（:1089）执行，LUT 未就绪时烘焙回退解析天空并告警。

## 10. 验收（本机能做的部分）

```bash
make                      # 编译通过
make shaders              # 含新 gn3d_skyview.comp；数量与严格编译计数一致
make unit                 # 0 failed（新增 cascade 单测计入）
make gpuCheck             # 仅编译（本机无 /dev/dri，运行时 SKIP 属预期）
bash ~/.dsh/skills/code-modularity/scripts/lintCodeModularity.sh <写域> --naming --comments
```

桌面待验（写进 LIGHTING_PLAN §12 的"桌面待验"列，不得写成已完成）：K 阴影观感/CSM 接缝、天空 LUT 与 Hillaire
参考图趋势、空气透视近远差异、切换天气后 IBL 与天空一致。
