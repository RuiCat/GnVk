# 天空 + 云 + 天气（雨 / 雾 / 雪）实现分析

> 性质：**源码实现分析 + 落地方案**，本文不改任何源码。
> 基线：当前工作区（`src/graphics/_Vulkan` + `assets/shaders`），C++11、Vulkan、`make` 单一构建入口。
> 结论先行：现状只有「一张启动时生成的静态天空 cubemap + 一条只用于 IBL 的天空盒管线」；
> 云、雾、雨、雪、天气状态**全部没有**。推荐按「天气状态机(CPU) → 解析式天空/云(着色器) →
> 雾(3 个片元着色器) → CPU 降水模拟 + 新透明管线」四层落地，CPU 部分可单测，GPU 视觉需你在桌面验证。
>
> **已确认决策（2026-10-28）**
> 1. 云 = **混合模式**：远景在天空着色器里做 FBM 云，中景再加一层 billboard 云片（视差）；不做体素/raymarch。
>
> ⚠️ **决策 1（云）已于 2026-10-31 被用户推翻**：L5 体积云（多层 raymarch）作为新路径落地，
> 见 `docs/L5_CLOUD_CONTRACT.md` 与 `docs/LIGHTING_PLAN.md` §4.7；旧 FBM 天空云 + billboard 中景云
> 保留为回退（查看器 `X` 键开关，默认关）。本文件其余决策（2–5）不变。
> 2. 雾 = **独立 Weather UBO**（不扩 SceneUBO，见 §5.2）。
> 3. 雨雪 v1 = **CPU 展开 billboard**（接受每帧上传带宽）。
> 4. **HDR 路径纳入首版**（天空/降水各建 HDR 变体管线）。
> 5. **IBL 动态更新放 v2**（v1 继续用静态 cubemap）。

---

## 1. 现状（源码证据）

| 能力 | 现状 | 证据 |
|---|---|---|
| 天空盒 | 启动时 CPU 生成 **静态** 256×256×6 渐变 cubemap | `vk3dInitScene.cpp:215`（`init3DSkybox`）、:219（`smSize=256`）、:266 起生成渐变 |
| 天空绘制 | 单位立方体、`z=w` 远平面、深度 LEQUAL 不写、无剔除 | `gn3d_skybox.vert:30-35`、`vk3dInitScene.cpp:462-470`、`vk3dPasses.cpp:169-188` |
| 天空是否默认画 | **没有调用点**（`grep "GN_VkDrawSkybox("` 只找到定义） | 默认场景背景只是 3D pass 的清屏色 |
| IBL | PBR / 延迟光照都采样同一张静态天空 cubemap | `gn3d_pbr.frag:25,254`、`gn3d_deferred_light.frag:22,247` |
| 云 | 无 | `grep cloud` 无命中 |
| 雾 | 无（所有 3D 着色器都没有 fog） | `grep fog assets/shaders` 无命中 |
| 雨/雪 | 只有 CPU 粒子池，**无 GPU 上传/绘制** | `GN_3Dparticle.h:13-14`（明写“实例上传/绘制留 P8”）、`GN_3Dparticle.cpp`（173 行） |
| 天气状态 | 无 | 无 weather/环境状态模块 |
| 场景 UBO | 96 B：`viewProj` + `cameraPos` + `ambientLight` | `GN_Vk.h:101-105`；缓冲按 `sizeof(VkSceneUBO)` 分配（`vk3dInit.cpp:245`）；描述符范围同（`GN_Vk.cpp:798`） |
| 光照 UBO | 缓冲固定 1024 B，结构 752 B | `GN_3Dlight.h:40-49`、`vk3dInit.cpp:252-254` |
| 管线能力 | `sPIPELINE_DESC` 支持 alpha/add/multiply、深度测试/写、剔除 | `GN_Pipeline.h:28-33, 61-100` |
| 顶点布局 | 仅单 binding 交错属性，**无 per-instance rate** | `GN_Pipeline.h:44-56` |
| Pass 结构 | 前向/PBR → `renderPass3D`；HDR → `hdrRenderPass`；延迟 → G-Buffer + deferredLight | `vk3dPasses.cpp:260,344,409,547` |
| 天空盒与 HDR 兼容 | 天空盒管线只绑定 `renderPass3D`，**不能在 HDR pass 复用** | `vk3dInitScene.cpp`（`descSky.aRenderPass = GN_Vk.renderPass3D`） |
| 着色器构建 | `make shaders` 批量 `glslangValidator`；运行时优先读 `assets/shaders/*.spv` | `Makefile:287-291`、`GN_Vk.cpp:760-780` |

补充事实：

- `gn3d_skybox.vert` 里 `fragWorldDir = pos`（立方体顶点就是世界方向），**解析式天空可以直接用这个方向**，不需要 cubemap。
- 前向 `gn3d.frag` 输出前没有色调映射；PBR `gn3d_pbr.frag` 内部做了 Reinhard；HDR 路径 `gn3d_composite.frag` 又做一次 Reinhard——HDR 路径本身已有“重复色调映射”的存量问题，加雾时必须明确在**线性空间、色调映射之前**做。
- `init3DSkybox` 所在文件 `vk3dInitScene.cpp` 机检为 **813 有效行**（空行/注释不计），目前同时承载 HDR、天空盒、G-Buffer、延迟四块；新天空参数/绘制建议独立成 `vkSky.cpp`（并把天空盒初始化迁出），保持“一个源码文件一个功能”的边界，而不是继续在 813 行里叠加。

---

## 2. 差距清单

| 目标 | 缺什么 |
|---|---|
| 天空 | 太阳方向/强度、时段、地平线颜色、可动画参数；当前只有固定渐变 |
| 云 | 无任何云层；cubemap 是静态的，无法滚动/变覆盖度 |
| 雾 | 无距离/高度雾；三个受光着色器都要加 |
| 雨/雪 | 有 CPU 池，但发射形状是“点+圆锥”，没有相机跟随体积、没有循环回收、没有 GPU 绘制 |
| 天气 | 无状态机、无过渡、无风雨强度驱动 |
| 一致性 | 雾色/天空色/环境光需要同源，否则会出现“雾是灰的、天是蓝的” |

---

## 3. 总体设计

```text
                 ┌──────────────────────────────┐
                 │ cWEATHER (CPU, 纯逻辑, 可单测)│
                 │  preset → 目标 → 平滑过渡     │
                 └───────┬───────────┬──────────┘
                         │           │
        sSKY_PARAMS      │           │  sWEATHER_UBO
   (太阳/云/曝光/时间)    │           │  (雾/风/雨雪强度)
                         ▼           ▼
              GN_VkSetSky()      GN_VkSetWeather()
                         │           │
                         ▼           ▼
        ┌───────────────────────────────────────┐
        │ 3D pass                                │
        │  1. 天空（解析式 + FBM 云）            │
        │  2. 不透明模型（片元着色器内加雾）     │
        │  3. 降水（alpha 混合，深度测试不写）   │
        └───────────────────────────────────────┘
                         ▲
       GN_VkDrawPrecipitation(verts,count)
                         │
              ┌──────────┴───────────┐
              │ cPRECIPITATION (CPU) │  ← 相机跟随体积、循环回收
              └──────────────────────┘
```

渲染顺序固定：天空（最远、深度不写）→ 不透明 → 半透明降水 → 结束 pass。
天气状态每帧只更新一次（`AppRun`），渲染层只消费派生参数。

---

## 4. 天空与云

### 4.1 方案对比

| 方案 | 代价 | 结论 |
|---|---|---|
| A. 继续用 cubemap，天气变化时 CPU 重生成 6 面 | 6×256² 生成 + 上传；云无法平滑动画 | 只适合作为 IBL 缓存 |
| B. **解析式天空写在 skybox 片元着色器**（推荐） | 全屏一次，无额外 pass；方向已有 | ✅ v1 采用 |
| C. 外部全景贴图 | 资产依赖、无法随天气变 | 不采用 |

推荐 B：保留现有天空盒管线与立方体顶点，**只替换片元着色器**与 UBO 内容。

### 4.2 UBO 扩展

现有天空盒 UBO 只有 64 B（`viewProj`，`vk3dInitScene.cpp:426-428`）。扩为 128 B：

```glsl
layout(binding = 0, set = 0) uniform SkyboxUBO {
    mat4 viewProj;        // 64 B
    vec4 sunDirection;    // xyz=太阳方向, w=太阳强度
    vec4 skyParams;       // x=浊度/曝光 y=时段 z=天空亮度 w=保留
    vec4 cloudParams;     // x=覆盖度 y=锐度 z=云层风速 w=云层高度
    vec4 timeParams;      // x=时间(秒) y=风角 z=风力 w=保留
} skybox;
```

C++ 侧新增 `sSKY_PARAMS` 与 `GN_VkSetSkyParams(const sSKY_PARAMS&)`；
`GN_VkDrawSkybox` 上传 128 B（`vk3dPasses.cpp:173-176`）。
未设置时用默认值，保持现有观感。

### 4.3 天空着色器

- 方向：`fragWorldDir` 已是世界方向（`gn3d_skybox.vert:33`），直接算 `dir.y` 做地平线/天顶渐变。
- 太阳：`sunAmount = smoothstep(cosSunRadius, 1, dot(dir, sunDir))`，叠加暖色；可选简单 Rayleigh 近似（`pow(1-dir.y, k)` 控制地平线暖色）。
- 云（FBM）：
  - 把方向投影到云平面：`p = dir.xz / max(dir.y, 0.06)`；
  - 两层 `fbm(p * scale + wind * time)`（4~5 octaves，值噪声哈希即可）；
  - `cloud = smoothstep(1 - coverage, 1 - coverage + softness, fbm)`；
  - 云颜色 = 被太阳照亮的白 + 背阴灰，按 `dot(dir, sunDir)` 混合；
  - 地平线附近用 `smoothstep` 淡出，避免接缝。
- 云（**混合模式**，分两步）：
  - **S2a 远景**：天空着色器内 FBM（4~5 octaves）云层，按方向投影到云平面并随风滚动；
  - **S2b 中景**：一层 billboard 云片（少量大四边形，半透明、程序化 alpha、随风漂移），提供视差；
  - 不做体素/raymarch；云影暂不做。
- 无 raymarch / 体积云（留后续）。

### 4.4 天空与 IBL / HDR

- **IBL**：v1 仍用启动时的静态 cubemap（PBR/deferred）；天气变化只改 `scene.ambientLight` 与天空视觉，接受“IBL 与天空不完全一致”。
  v2 可选：天气/时段变化时以 64×64×6 重生成 cubemap（新增一个写 cubemap 的 render pass），再更新 PBR/deferred 的 binding 5——这是最贵的部分，放最后。
- **HDR**：天空盒管线绑的是 `renderPass3D`，HDR 用 `hdrRenderPass`（`vk3dPasses.cpp:344`）。
  若要在 HDR 路径画天空，需要**为 `hdrRenderPass` 再建一条同 shader 的天空管线**（管线缓存键已含 renderPass，直接再 `GetOrCreate` 一份即可）；否则 HDR 路径下天空会缺失。
- **延迟路径**：背景天空是在 `gn3d_deferred_light.frag:210-211` 从 cubemap 采样得到的；解析式天空要么复制同一段 GLSL 函数，要么等 v2 的 cubemap 重生成。建议 v1 只保证前向/PBR。

---

## 5. 雾

### 5.1 模型

采用「距离 × 高度指数雾」，参数少、与现有场景 UBO 契合：

```text
heightDensity = fogDensity * exp(-heightFalloff * max(worldY - baseHeight, 0))
distance      = length(worldPos - cameraPos)
fogFactor     = 1 - exp(-distance * heightDensity)
color         = mix(fogColor, litColor, fogFactor)
```

可选 `fogStart/fogEnd` 做艺术控制；`fogColor` 由天气模块从天空地平线颜色推导，保证天/雾同源。

### 5.2 UBO 方案（独立 Weather UBO）

**决策：独立 UBO，不扩 `VkSceneUBO`。** 新增 64 B：

```cpp
// C++ 与 GLSL 必须逐字段一致
struct sWEATHER_UBO {
    float fogColor[4];   // rgb=雾色, a=密度
    float fogParams[4];  // x=高度衰减, y=基准高度, z=起始距离, w=结束距离(0=纯指数)
    float windTime[4];   // xyz=风(m/s), w=时间(秒)
    float precip[4];     // x=雨强度, y=雪强度, z=太阳强度, w=云覆盖度
};
```

- 缓冲：host-visible 64 B，由新文件 `vkAtmosphere.cpp` 持有；API `GN_VkSetWeather(const sWEATHER_UBO&)`。
- 绑定槽（各管线现有占用之后插入一个）：
  - 前向：binding **3**（现有 0=Scene,1=tex,2=Light）
  - PBR：binding **6**（现有 0..5）
  - 延迟光照：binding **8**（现有 0=Scene,1..4=G-Buffer,5=shadow,6=skybox,7=Light）
- 三个 `VkDescriptorSetLayout` 各加一个 `UNIFORM_BUFFER`（fragment 阶段），三个描述符集写入同一缓冲；
  需确认 `descriptorPool` 的 `UNIFORM_BUFFER` 配额（`vk3dInit` 创建描述符池处）是否还有余量。
- 片元着色器 `gn3d.frag` / `gn3d_pbr.frag` / `gn3d_deferred_light.frag` 各加对应 binding 的 UBO 声明与雾混合；
  **顶点着色器不动**。
- 好处：雾参数只有一处数据源，不会出现“5 处 `VkSceneUBO` 漏填一处”的问题。
- API：`GN_VkSetWeather`（天气层把 `WeatherOutput` 填进来）。

### 5.3 天空/降水的雾

- 天空：地平线颜色直接与雾色混合（天空着色器内）。
- 降水：片元按距离衰减 alpha（或直接套同公式）。

API：`GN_VkSetWeather(const sWEATHER_UBO&)`。

---

## 6. 雨 / 雪

### 6.1 模拟层：扩展现有粒子池（不打第二份池）

`cPARTICLE_SYSTEM`（`GN_3Dparticle.h:35`）已有：固定池、按速率发射、生命周期、重力/阻尼、颜色插值、确定性随机。
降水与它的差异只有三点，建议**原位扩展**而不是复制一份池逻辑（C10）：

1. **发射形状**：增加 `ePARTICLE_SHAPE { CONE, BOX }`；`BOX` 用半尺寸 `aBoxHalf`，在体积内均匀采样。
2. **循环回收**：`aWrapHeight`（>0 时，粒子低于 `origin.y - halfY` 回到顶部并重置寿命），配合每帧
   `setOrigin(cameraPos)` 实现“永远围绕相机的雨柱”。
3. **尺寸与风**：`aSizeMin/aSizeMax`（雨 0.02~0.05、雪 0.03~0.08）、`aWind`（水平速度，云层与降水共用）。
   现有 `setOrigin/setRate/setGravity` 已够用。

保持 CONE 模式行为逐位不变，现有 `testParticle` 6 个用例继续守回归。

参数建议：

| 参数 | 雨 | 雪 |
|---|---|---|
| 体积（相机跟随） | 40×30×40 | 50×35×50 |
| 速度 | 25~35 m/s（向下） | 1~3 m/s |
| 重力 | 9.81 | 1.5 |
| 阻尼 | 0 | 0.6 |
| 寿命 | 1~1.5 s | 8~12 s |
| 风影响 | 0.3× | 1.0×（带正弦漂移） |
| 最大粒子数 | 4000~8000 | 3000~6000 |

### 6.2 渲染层：CPU 展开 billboard + 新透明管线（v1）

现有 `sVERTEX_LAYOUT` **没有 per-instance rate**（`GN_Pipeline.h:44-56`），v1 不对它做扩展：

- 每粒子 6 顶点（2 三角形），属性 `pos3 + uv2 + color4 + kind`（stride 40 B）；
- 8000 雨滴 ≈ 1.9 MB/帧上传，可接受；后续再评估实例化（需要给 `sVERTEX_ATTR` 加 `aInputRate`）。

新管线（`_Vulkan/vkPrecipitation.cpp`）：

| 字段 | 值 |
|---|---|
| renderPass | `renderPass3D`（HDR 另建一份 `hdrRenderPass` 变体） |
| blend | `eBLEND_ALPHA` |
| depthTest / depthWrite | true / **false** |
| cull | 不剔除 |
| 顶点输入 | 上述 40 B 交错布局 |
| push constant | `viewProj(64) + camRight/camUp(32) + tint/fog(16)` = 112 B（≤128 保证值） |

- 雨：四边形沿屏幕空间速度方向拉长，边缘 alpha 衰减；颜色偏冷灰。
- 雪：方块精灵，片元内圆形衰减；白色。
- 绘制时机：不透明模型之后、`GN_VkEnd3DPass()` 之前；不做排序（alpha 混合 + 不写深度，观感可接受）。
- 专用 host-visible 动态顶点缓冲（按需扩容），不复用 `vertexBuffer3D`（stride 44，耦合）。
- 与雾一致：按距离/高度衰减 alpha。

### 6.3 与天气状态联动

- `rain/snow` 强度 → 目标活跃粒子数（预热：切换时直接按当前寿命随机填充，避免“从零慢慢下起来”）；
- `wind` → 发射速度水平分量与雪漂移；
- 雾强度 → 降水可见距离同步。

---

## 7. 天气状态机（CPU）

### 7.1 数据结构

```cpp
enum class WeatherPreset { CLEAR, CLOUDY, RAIN, SNOW, FOG, STORM };

struct WeatherState {        // 目标值与当前值同构，current 每帧向 target 平滑
    float rain;              // 0..1
    float snow;              // 0..1
    float fog;               // 0..1
    float cloudCoverage;     // 0..1
    float windX, windZ;      // m/s
    float sunIntensity;      // 0..1
};

struct WeatherOutput {       // 渲染层消费的派生参数（只读快照）
    SkyParams sky;
    FogParams fog;
    PrecipitationParams precipitation;
    float ambientScale;
};
```

### 7.2 API 与过渡

- `cWEATHER::setPreset(WeatherPreset, float transitionSeconds = 3.0f)`
- `cWEATHER::setState(const WeatherState&, float transitionSeconds)`
- `cWEATHER::update(float dt)`：线性/平滑插值 + 限幅；`dt` 钳制（与粒子一致，≤0.1 s）
- `cWEATHER::output()`：由 current 推导 `SkyParams/FogParams/PrecipitationParams`；
  雾色 = 天空地平线色 × (1 - 0.3×rain)（保证同源）。

预设建议：`CLEAR`（云 0.1）、`CLOUDY`（0.7）、`RAIN`（云 0.9/雨 0.8/雾 0.35/风 6）、`SNOW`（云 0.8/雪 0.8/雾 0.3/风 2）、`FOG`（雾 0.9/云 0.5/风 0）、`STORM`（雨 1.0/云 1.0/雾 0.5/风 14/太阳 0.2）。

### 7.3 引擎接线

- 应用每帧：`GN_WeatherUpdate(dt)` → 内部调用 `GN_VkSetSky` / `GN_VkSetWeather` / 更新降水发射器。
- 光照：`WeatherOutput.ambientScale` 乘到 `cLIGHT` 环境光/太阳强度（渲染层读同一份）。
- 可选：引擎在 `AppRun` 前自动 update（不建议隐式，保持应用可控）。

---

## 8. 文件与接口规划（遵守单功能 <500 有效行）

| 文件 | 职责 | 预估有效行 |
|---|---|---|
| `src/graphics/weather.h/.cpp`（已完成 S1） | 状态机、预设、派生参数（纯 CPU） | ~260 |
| `src/graphics/GN_3Dparticle.h/.cpp`（改） | 增加 BOX 形状 / 循环回收 / 尺寸+风 | +~120（文件仍 <500） |
| `src/graphics/_Vulkan/vkSky.cpp`（新） | 天空 UBO/参数/绘制，**从 vk3dInitScene 迁出天空盒初始化**（该文件 813 有效行，迁出后只保留 HDR/G-Buffer/延迟） | ~180 |
| `src/graphics/_Vulkan/vkAtmosphere.cpp`（新，必需） | 独立 Weather UBO（64 B）、`GN_VkSetWeather`、3 个描述符集的 binding 写入 | ~120 |
| `src/graphics/_Vulkan/vkPrecipitation.cpp`（新） | 降水管线、动态缓冲、绘制 | ~220 |
| `src/graphics/_Vulkan/vkCloud.cpp`（新，S2b） | billboard 云层管线/缓冲/绘制（与降水共用 alpha 管线设施） | ~140 |
| `assets/shaders/gn3d_skybox.frag`（重写） | 解析式天空 + FBM 云 | ~130 |
| `assets/shaders/precipitation.vert/.frag`（新） | billboard 展开 + 雨/雪外观 | ~90 / ~60 |
| `assets/shaders/gn3d.frag` / `gn3d_pbr.frag` / `gn3d_deferred_light.frag`（改） | 雾混合 | 各 ~10 |
| `src/graphics/_Vulkan/GN_Vk.h` | `sWEATHER_UBO` / 新 API 声明、降水句柄 | +~40 |
| `tests/unit/testWeather.cpp`（新） | 预设/过渡/限幅/派生参数/确定性 | ~180 |
| `tests/unit/testParticle.cpp`（改） | BOX 体积、循环回收、尺寸范围、既有用例回归 | +~70 |

命名：新文件名 camelCase（`weather.h`、`vkSky.cpp`…），类型 PascalCase；`GN_*` 函数是外部契约名，保留。

---

## 9. 分阶段落地与验收

| 阶段 | 内容 | 验收（本机可做） |
|---|---|---|
| S1 ✅ | 天气状态机 `weather.h/.cpp` + 8 个用例 | `make unit` → 231 cases 全绿 |
| S2a ✅ | 天空 UBO 64→128、解析式天空 + FBM 云、`GN_VkSetSky`、查看器画天空 + 1..6 切天气 | `make mmdViewer` / `make check` 通过；GLSL 编译通过；桌面视觉待你验证 |
| S2b | billboard 云层（中景视差） | 同上 |
| S3 | 雾：独立 Weather UBO（binding 3/6/8）+ 3 个片元着色器 | 编译 + 单测；你桌面看远近过渡 |
| S4 | 粒子池 BOX/回收/风 + 单测 | `make check`（既有 6 用例不回归） |
| S5 | 降水管线 + 着色器 + 绘制接线 | `make shaders` / 编译；你桌面看雨雪外观与性能 |
| S6 | MMD 查看器集成（天空 + 天气 + 降水）、HDR 变体、文档/CHANGELOG | 你桌面全天气截图；CPU 单测全绿 |

本机（沙箱）**无 `/dev/dri`**，S2/S3/S5 的 GPU 视觉无法自测；只能做到“编译 + GLSL 编译 + CPU 单测”，
这一点会如实写进交付说明。

---

## 10. 风险与待决

1. **独立 UBO 的描述符改动**：前向/PBR/延迟三条管线都要加 binding 与描述符写入；漏一条就是“某条 pass 没雾”。
   同时确认描述符池容量，避免 `vkAllocateDescriptorSets` 失败。
2. **HDR 兼容**：天空盒管线只对 `renderPass3D`；HDR 需要第二条管线；HDR 路径还有存量“重复 Reinhard”问题，雾应在线性空间、色调映射前加。
3. **IBL 与天空不一致**：v1 接受；v2 再评估动态 cubemap 的生成成本与描述符更新。
4. **降水透明排序**：alpha 混合 + 不写深度，粒子间无排序；雨丝/雪花足够小，可接受；如出现穿帮再加按深度排序或软粒子。
5. **实例化缺失**：v1 CPU 展开 billboard；若雨雪数量上到 2 万+，需要给 `sVERTEX_ATTR` 增加 input rate 或走 GPU 粒子（P9）。
6. **延迟路径**：背景天空目前在 deferred light shader 里采样 cubemap（`gn3d_deferred_light.frag:210`），解析式天空需要同步修改，否则两条路径观感不一致。
7. **无 GPU 验证**：所有 Vulkan 运行时路径以你桌面实测为准。

---

## 11. 参考锚点（复算用）

```bash
# 天空
sed -n '215,300p' src/graphics/_Vulkan/vk3dInitScene.cpp
sed -n '169,188p' src/graphics/_Vulkan/vk3dPasses.cpp
cat assets/shaders/gn3d_skybox.vert assets/shaders/gn3d_skybox.frag

# IBL
grep -n "skyboxCubemap" assets/shaders/gn3d_pbr.frag assets/shaders/gn3d_deferred_light.frag

# 场景 UBO
sed -n '101,105p' src/graphics/_Vulkan/GN_Vk.h
grep -n "uniformBuffer3D" src/graphics/_Vulkan/vk3dInit.cpp

# 粒子
sed -n '1,80p' src/graphics/GN_3Dparticle.h

# 管线能力
sed -n '28,56p' src/graphics/GN_Pipeline.h
```
