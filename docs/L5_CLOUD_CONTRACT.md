# L5 体积云（多层 raymarch）实现契约（2026-10-31 冻结）

> ⚠️ **已被取代（2026-11-01）**：用户拍板把云改成「世界固定的一组独立立方体云块 + 每块云一个降水发射器」，
> 体积云整层实现（`vkVolumetricCloud.cpp`、`gn3d_cloud_vol.*`、`cloudNoise.*`、208 B `sCLOUD_UBO`）
> 及对应 gpuCheck/单测已**删除**。现行契约 = `GN_Vk.h` 的 `sCLOUD_CUBE_PARAMS` / `sCLOUD_CUBE_INSTANCE`
> 与 `assets/shaders/gn3d_cloud_cube.vert|frag`。以下正文仅作历史记录。

> 性质：**冻结契约**。来源规划 `docs/LIGHTING_PLAN.md` §4.7（L5a–L5c）。
> **决策变更**：本轮经用户拍板**推翻** `docs/SKY_WEATHER_PLAN.md` 决策 8「云 = 混合模式，不做 raymarch」；
> 体积云作为新路径，旧 FBM 天空云与 billboard 中景云**保留为回退**（`C` 开关）。
> 本文件只写接口口径（格式 / UBO / 绑定 / 公式 / 验收），不重复论文背景。

## 0. 口径与 v1 取舍（必须随实现写入文档）

- 世界 **y-up**，地面 y=0；云层按**水平无限平面**层（v1 不做球形壳层）。
- **观感修正（2026-10-31 用户反馈后）**：默认改为**单层薄平面**（1600–2200 m，`aLayer0` 生效、
  `aLayer1/2` 覆盖率 0）；云漂移时间 ×0.25（`CLOUD_DRIFT_TIME_SCALE`）；抖动改为**只与像素有关**
  （不逐帧换图，消除"沸腾"）；`CLOUD_SIGMA=0.01`、覆盖率 remap 用 `smoothstep` 并以天气云锐度控制软硬，
  使 CLEAR(≈0.15) 稀疏、STORM(≈1.0) 满云。默认**开启**（`aRender.w=1` 由查看器每帧填）。
- **旧云已停用**：`gn3d_skybox.frag` 删除 FBM 云块（天空只剩 LUT/解析基色 + 太阳）；查看器不再提交
  billboard 中景云（`drawClouds` 保留但无调用点，C 键停用）。体积云是唯一云路径，X 键可整体关掉云。
- 开关默认**开**（查看器每帧填 `aRender.w=1`）；`X` 关闭时既不画云、也不改天空。
- **v1 是逐像素全分辨率 raymarch**，不是规划里的半分辨率 + upsample：
  半分辨率需要独立 pass 与深度感知升采样，本机无 GPU 无法验证，v1 先保证排序正确（云先于不透明几何绘制）。
  性能基线待桌面实测后再决定是否降半分辨率——**不得**把"全分辨率"写成"已达性能目标"。
- **v1 无时域重投影**（L2b TAA 未实现）：用逐像素静态 hash 抖动 + 固定步数，记录边界锯齿为桌面待验项。
- **v1 无云影**（规划 L5c 可选项）；不写深度（`depthWrite=false`）、不参与阴影投射。
- weather map 为**启动时 CPU 生成**的小尺寸纹理；覆盖率/锐度/太阳/风逐帧来自天气状态，weather map 只做空间起伏。

## 1. 数据（执行体 A：`src/graphics/cloudNoise.h/.cpp` + `tests/unit/testCloudNoise.cpp`）

```cpp
namespace GN3d {
constexpr int CLOUD_NOISE_3D_SIZE = 64;      // 3D 噪声边长（RGBA8，64³ = 1 MiB）
constexpr int CLOUD_WEATHER_MAP_SIZE = 128;  // weather map 边长（RG8）

struct sCLOUD_WEATHER_INPUT {
    float coverage;   // 0..1，越界钳制
    float sharpness;  // 0..1，越界钳制
    float cloudType;  // 0..1（0=层云/1=积云；线性映射到 weather.G）
};

// 确定性 3D Perlin-Worley（同 seed 同结果；三轴周期可平铺；构造依据 Schneider 2015 参考实现）：
//   R = Perlin-Worley：Perlin 用 **Worley fBm**（4/8/16 胞，权重 0.625/0.25/0.125）膨胀，
//       即 remap(perlin,0,1,worleyFbm,1) = worleyFbm + perlin*(1-worleyFbm)；
//       注意不能用单频 Worley——单频 Voronoi 的规则胞格会带进云形，表现为规则纹理/接缝感。
//   G = 低频 Worley（约 4 胞/边，单频）
//   B = 高频 Worley（约 8 胞/边，单频）
//   A = 255
// rgba 长度 = size³×4，值域 0..255；size<=0 / 非 2 的幂实现可返回 false（64 必须支持）。
bool generateCloudNoiseTexture(uint32_t size, uint32_t seed, std::vector<uint8_t>& rgba);

// 2D weather map（RG8）：R = 覆盖率扰动（值域 0..255，0.5 对应输入 coverage），G = cloudType。
// 用周期 2D value noise；同 seed+输入同结果。
bool generateCloudWeatherMap(uint32_t size, const sCLOUD_WEATHER_INPUT& weather, uint32_t seed,
                             std::vector<uint8_t>& rg);
} // namespace GN3d
```

- 单测至少覆盖：尺寸/值域、同 seed 确定性、不同 seed 有差异、非退化（标准差 > 阈值）、
  周期可平铺（对边差值小）、weather map 随 coverage 单调、非法输入返回 false。
- 纯 CPU，不碰 Vulkan；不得 include Vulkan 头。

## 2. UBO 与绑定（冻结；C++ `sCLOUD_UBO` 在 `GN_Vk.h`，**208 B**）

| 偏移 | 字段 | 语义 |
|---|---|---|
| 0 | `mat4 invViewProj` | 当前帧 viewProj 逆（绘制时由 C++ 覆盖） |
| 64 | `vec4 aCamera` | xyz=相机世界位置, w=时间(秒) |
| 80 | `vec4 aSun` | xyz=指向太阳单位向量, w=太阳强度 |
| 96 | `vec4 aRender` | x=raymarch 步数(查看器默认 32), y=光照步数(默认 4), z=天气云锐度 0..1, w=开关 |
| 112 | `vec4 aLayer0` | x=云底(m), y=云顶(m), z=覆盖率 0..1, w=密度倍率（默认唯一启用的薄层 1600–2200 m） |
| 128 | `vec4 aLayer1` | 同上（可选第二层；覆盖率 0 = 关闭，默认 0） |
| 144 | `vec4 aLayer2` | 同上（可选第三层；覆盖率 0 = 关闭，默认 0） |
| 160 | `vec4 aNoise` | x=weather 平铺尺度, y=3D 噪声世界尺度, z=风 x, w=风 z |
| 176 | `vec4 aScreen` | x=像素宽, y=像素高, z=1/宽, w=1/高（C++ 绘制时填充） |
| 192 | `vec4 aPlanet` | x=地面半径(m)（**球壳求交**用，与 L4 大气同源）；y/z/w 保留 |

- 管线 set0：binding 0 = UBO（208 B）、binding 1 = `sampler3D`（3D 噪声）、binding 2 = `sampler2D`（weather map）。
- 着色器 push constant：**不用**（屏幕尺寸走 `aScreen`）。
- 纹理创建（Lead）：3D = `R8G8B8A8_UNORM` 64³、sampler LINEAR + REPEAT（三轴）；
  2D = `R8G8_UNORM` 128²、sampler LINEAR + REPEAT。
  **必须带 mip 链**（3D 64³→1、2D 128²→1；mipmapMode=LINEAR、maxLod=mip 数−1）：
  远处云层对 3D 噪声的采样频率极高，无 mip 会在掠射角出现规则走样条纹（被用户当作“噪声接缝”）。
- 混合：**预乘 alpha**——`outColor.rgb = 散射辐亮度`（已预乘），`outColor.a = **不透明度 (1 - 透射率)**`；
  管线混合 `src=ONE, dst=ONE_MINUS_SRC_ALPHA`（颜色与 alpha 通道都如此），
  结果 `dst' = C + dst×(1-a) = C + dst×T`。
  **alpha 是"不透明度"不是"透射率"**：无云处 a=0（透明）、全遮挡 a=1；写反会把天空整片涂黑
  （2026-11-01 用户截图"天空全黑"的根因，两轮才定位）。

## 3. 着色器（执行体 B：`assets/shaders/gn3d_cloud_vol.vert` + `gn3d_cloud_vol.frag`）

### 3.1 顶点

无顶点缓冲：`gl_VertexIndex` 生成覆盖屏幕的大三角形；输出 `uv`（0..1，Vulkan NDC y 向下）。
`gl_Position = vec4(pos.xy, 0, 1)`。管线 `aCullBack=false`、`aTopology=TRIANGLE_LIST`。

### 3.2 片元主流程（`gn3d_cloud_vol.frag`）

1. `if (aRender.w < 0.5 || aRender.x < 1) { outColor = vec4(0); return; }`
   **所有提前返回必须写 `vec4(0)`（alpha=不透明度=0 = 完全透明）**：混合是
   `dst' = C + dst×(1-a)`，alpha=1 且颜色 0 会得到 `dst'=0` ⇒ 天空整片涂黑
   （2026-11-01 用户截图"天空全黑"的根因）。
2. 反投影视线：`ndc = uv*2-1`；`ws = invViewProj * vec4(ndc, 1, 1)`；`dir = normalize(ws.xyz/ws.w - aCamera.xyz)`；
   `dir.y <= 1e-4` ⇒ 输出透明 `vec4(0)`（地平线以下不画）。
3. **球壳区间**：相机在地面附近（低于云底）⇒ 内球（`Rg+baseY`）与外球（`Rg+topY`）求交；
   `t0 = max(inner.y, 0)`、`t1 = min(outer.y, 120000)`；无交或 `t1<=t0` ⇒ 透明。云层是**曲面**（随行星弯曲）。
4. 步进：`steps=aRender.x`（查看器默认 64），`dt=(t1-t0)/steps`，**无逐像素抖动**（抖动会在薄云上留下
   可见网格/麻点）；远处乘 `farFade=1-smoothstep(20000,120000,t)`。
5. 逐样本密度（方向空间噪声；参考用户给定着色器的 `cloud()` 写法）：
   ```text
   altitude = length(p - planetCenter) - Rg
   h = (altitude - base)/(top-base)；越界返回 0
   profile = smoothstep(0,0.30,h) * smoothstep(1,0.55,h)
   n  = normalize(p - planetCenter)
   uv = n*2.0 + windOffset                 // 方向空间采样：无世界坐标平铺接缝
   c0 = texture(noise, uv).r               // R = Perlin-Worley（Worley fBm 膨胀，带 mip）
   low  = mix(0.62, 0.28, coverage)        // coverage = aLayer0.z（天气云量）
   high = low + 0.20
   c  = smoothstep(low, high, c0)          // 覆盖率阈值：边界清晰，不退化成雾
   m  = c0 + 0.35*texture(noise, n*6.2 + windOffset*3.1).r
   d  = smoothstep(0.85 - 0.5*c, 0.95, m)  // 高频侵蚀：团块/窟窿
   density = d * c * profile * aLayer0.w
   ```
6. 消光：`sigmaStep = density * CLOUD_SIGMA * dt`（`CLOUD_SIGMA=0.06`，随参考的密度尺度归一化），
   `k = exp(-sigmaStep)`。
7. 光照（参考的朝向太阳透射率，比多步 light march 便宜且不会把云压黑）：
   `sunDensity = cloudDensity(p + sunDir*900m)`；`tl = exp(-3.5 * sunDensity * densityScale)`；
   `day = smoothstep(-0.10, 0.25, dot(n, sunDir))`；
   `lit = (vec3(1.45,1.20,1.05)*tl + vec3(0.36,0.44,0.58)*(0.35+0.65*tl)) * day + 天顶色*环境系数`，
   再乘 `(sunIntensity*2 + 0.25)`（天气太阳强度）。
8. 累加：`scatter += transmittance * (1-k) * lit`；`transmittance *= k`；`transmittance<1e-3` 提前结束。
9. 输出 `outColor = vec4(scatter, 1.0 - transmittance)`（预乘颜色 + 不透明度，见 §2 混合）。
10. 常量必须集中为 `const` 并注释来源/量纲；不得从 `atmosphereParams.cpp` 复制大气参数（云是独立介质）。
11. **天空亮度标定**：查看器 `sky.atmosphereIntensity = 1.8`；该值与云散射同一线性域，桌面可继续微调。

### 3.3 性能约束

- 默认步数 64（`aRender.x`）；无独立光照 march（朝太阳单次采样式透射率）；桌面可下调。
- v1 不做半分辨率、不做时域；报告里必须写明这两点未做。

## 4. C++ 绘制（Lead：`src/graphics/_Vulkan/vkVolumetricCloud.cpp` + `GN_Vk.h`）

- `init3DVolumetricCloud()`：CPU 生成 3D 噪声 + weather map（`GN3d::generateCloudNoiseTexture/…WeatherMap`，固定 seed 1337）
  → staging 上传（复用 `CreateBuf`/`beginOnceCommand`）；建 sampler、UBO（208 B host-visible）、
  描述符布局/集、管线（`renderPass3D`、`msaaSamples`、`aCustomBlend=true`、`aBlendSrcColor/DstColor=ONE/ONE_MINUS_SRC_ALPHA`、
  同样设置 alpha 因子、`aDepthTest=false`、`aDepthWrite=false`、`aCullBack=false`、动态 viewport/scissor）。
- `GN_VkSetVolumetricClouds` 写映射内存；`GN_VkDrawVolumetricClouds(invViewProj)`：
  仅 `s_in3DPass` 时生效；`aRender.w<0.5` 直接返回；写 `invViewProj` 与 `aScreen`（swapchainExtent），
  设 viewport/scissor，`draw(3,1,0,0)`。
- 初始化顺序：`GN_VkInit3D` 中 `init3DSkybox()` 之后调用（renderPass3D/msaaSamples 已就绪）；
  `vk3dShutdown` 释放（描述符集随池释放）。
- 资源失败只告警（`GN_VkVolumetricCloudReady()=false`），不得影响既有渲染。

## 5. 查看器与回退（Lead：`tests/mmdViewer/`）

- 每帧（`applyWeather` 后、`GN_VkDrawSkybox` 之后）：由 `WeatherOutput` 填 UBO：
  `aCamera=camera`、`aSun=sunDir/sunIntensity`、`aRender={32,4,cloudSharpness,1}`、
  `aLayer0`（薄平面 1600–2200 m，coverage=weather.cloudCoverage，density=0.55+0.55*coverage）、
  `aLayer1/aLayer2` 覆盖率 0（多层参数保留，默认只启用一层）、
  `aNoise={1/20000, 1/1800, windX, windZ}`；`GN_VkSetVolumetricClouds` + `GN_VkDrawVolumetricClouds(&invViewProj)`。
- **替换旧云**：`gn3d_skybox.frag` 已删 FBM 云块；查看器不再提交 billboard 中景云（`drawClouds` 保留但无调用点、C 键停用）。
  `X` 键整体开关体积云（默认**开**）；叠层显示开关与层数/覆盖率。

## 6. 验收

```bash
make                          # 编译通过
make shaders                  # 含 gn3d_cloud_vol.vert/frag；数量 +2，严格编译全过
make unit                     # 0 failed（新增 cloudNoise 用例）
make gpuCheck                 # 编译通过（本机无 /dev/dri：运行 SKIP 属预期）
make mmdViewer                # 编译通过
bash ~/.dsh/skills/code-modularity/scripts/lintCodeModularity.sh <写域> --naming --comments
```

- gpuCheck 新增 `3d.volcloud`：identity VP 下天空区域画云（coverage=1、density=1、步数 16），
  读回中心上方像素：开关前后至少一个采样点差异 > 1e-4 且值有限。
- **桌面待验**（写进 LIGHTING_PLAN §12，不得写成已完成）：多层观感/体积感、边界接缝、噪点/拖影、
  覆盖率与天气一致、780M 1080p 性能基线、与 L4 大气天空的颜色一致性。
