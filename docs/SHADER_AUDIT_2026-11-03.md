# GnVk 着色器缺陷审计与优化方案

> **状态**：📊 快照 — 2026-11-03 的着色器审计；Phase 0/1 与低风险 P2 已落地，Phase 3 ray-query 未做。

> 日期：2026-11-03  
> 基线：`HEAD = ffdf0bb`  
> 范围：`assets/shaders/` 全部 45 个 GLSL 源文件（`.vert/.frag/.comp`，共 3689 行）+ 对应 C++ 管线/描述符/UBO 契约  
> 性质：**静态审计 + 可复算验证**。本会话无 GPU（`/dev/dri` 不存在、`vulkaninfo` 失败），运行时画质结论标注为「静态确认」或「待真机复验」；上一会话的真机记录作为旁证引用。
>
> **实施状态（2026-11-03 晚）**：Phase 0 + Phase 1 + 低风险 P2 已落地；Phase 2 中 PCSS 降 tap、SSGI 步数参数、`GN_VkReadBack` 格式适配已落地；SSGI 时域、探针/AP 降采样、G-Buffer 打包与 Phase 3 ray-query 未做。修复记录见 `docs/CHANGELOG.md` 的 2026-11-03 条目。

---

## 0. 结论先行

| 编号 | 级别 | 一句话结论 | 主要证据 |
|---|---|---|---|
| S-01 | **P0** | **SSGI 实际上永远输出 0**；开启 SSGI 后延迟光照用黑色 SSGI 替换 SH9 漫反射，反而**抹掉间接漫反射** | `gn3d_ssgi.frag:97-100`、`vkSsgi.cpp:294-299/411-428`、`vkGi.cpp:108-119`、`gn3d_deferred_light.frag:567-568` |
| S-02 | **P0** | 默认 1×1 **白图**被当成法线贴图，所有未提供 normal map 的网格法线被旋转约 45°（前向 PBR 与延迟 G-Buffer 都中） | `GN_Vk.cpp:453`、`gn3d_gbuffer.frag:42-47`、`gn3d_pbr.frag:438-444`、`cornellScene.cpp:131-137` |
| S-03 | **P1** | TBN 在「零切线」或「切线与法线平行」时产生 NaN；`gn3d_gbuffer.vert` 的回退只处理了前者，`gn3d_pbr.vert` 完全没有回退 | `gn3d_pbr.vert:39-42`、`gn3d_gbuffer.vert:45-53`、`mmdMeshBuilder.cpp:146-148` |
| S-04 | **P1** | 交换链格式取 `fmts[0]`、色彩空间恒为 `SRGB_NONLINEAR`，但 composite 注释假设「硬件自动 sRGB 编码」；真机实测是 RGBA16F（格式 97），二者契约矛盾 | `GN_Vk.cpp:209/216`、`GN_Vk.cpp:898/905`、`gn3d_composite.frag:103-104`、`cornellbox.md` M-18 |
| S-05 | **P2** | SSGI 是半分辨率 pass，却用 **LINEAR** 采样全分辨率深度/法线；SSAO 用的是 NEAREST，两者不一致 | `vkSsgi.cpp:282-283`、`gn3d_ssgi.frag:101-107`、`gn3d_ssgi_denoise.frag:14-26`、`vkSsao.cpp:285-286` |
| S-06 | **P2** | SSR 在循环中用 `texture()`（隐式 LOD 未定义），且固定 64 步、只按 metallic 早退；反射色用 `albedo × SH9` 当 radiance，π 倍偏亮且忽略直接光 | `gn3d_deferred_light.frag:356-382/577-583`、`evalSH9` 99-114 |
| S-07 | **P2** | PCSS 在 `NdotL <= 0` 之前执行；每光源探针 32 次纹理采样（本轮已降为 12+8）；点/聚光的 `attenuation.w`（range）从未被着色器读取（本轮已接线） | `gn3d_deferred_light.frag:448-458/522`、`gn3d_pbr.frag:393-401`、`GN_3Dlight.cpp:84/116` |
| S-08 | **P2** | 法线用 `mat3(model)` 直接变换；非均匀缩放下错误（G-Buffer vert 注释还写着「正确处理非均匀缩放」） | `gn3d_gbuffer.vert:39-40`、`gn3d_pbr.vert:35`、`gn3d.vert:34`、`gn3d_skinned.vert:63` |
| S-09 | **P2** | 天空盒地平线雾 `smoothstep(0.18, -0.02, dir.y)` 的 `edge0 > edge1`，GLSL 规范明确「结果未定义」 | `gn3d_skybox.frag:89` |
| S-10 | **P2** | 延迟光照 LightUBO 的描述符 range 写死 1024 B，但实际 buffer 只有 976 B（`sLIGHT_UBO` static_assert），越界描述符 | `vk3dInitScene.cpp:690`、`GN_3Dlight.h:56` |
| S-11 | **P3** | 一批健壮性/维护性问题：Bloom 先下采样再阈值、云块 slab 求交的 `1/0` 边界、compute 里用 `texture()`、PBR 在 discard 后采样纹理、9 张探针纹理、大量死参数与重复光照代码 | 见 §6、§7 |

**建议处理顺序**：S-01 → S-02 → S-03 → S-04 → S-05/S-06/S-07 → S-08/S-09/S-10 → S-11。  
**最小可验收修复**：只做 S-01 + S-02 + S-03，就能消除「功能失效 + 大面积错误光照 + NaN」三类最严重问题；预计改动集中在 2 个 shader、2 个 C++ 文件和一个 1×1 纹理。

---

## 1. 审计方法与基线

### 1.1 已执行的验证

| 检查 | 命令 | 结果 |
|---|---|---|
| GLSL 编译 | 对 45 个源文件逐个 `glslangValidator -V [- -target-env vulkan1.2]` 到 `/tmp` | **45/45 通过，0 失败** |
| SPIR-V 校验 | 对 `assets/shaders/*.spv` 逐个 `spirv-val` | **45/45 通过，0 失败** |
| 单元测试 | `LD_LIBRARY_PATH=. ./build/unit_tests` | **396 cases / 0 failed / 0 assertions failed** |
| 真 GPU | `ls /dev/dri`、`vulkaninfo --summary` | **本会话不可用**（无 `/dev/dri`，枚举不到物理设备） |
| 数值复算 | 见 §3.1、§3.2 | 默认白图法线、TBN 平行退化两处已复现 |

> 编译基线命令（可复算）：
>
> ```bash
> cd /home/ruicat/下载/GnVk/GnVk
> for f in assets/shaders/*.vert assets/shaders/*.frag assets/shaders/*.comp; do
>   extra=""; case "$f" in *gn_cornell_rq.comp) extra="--target-env vulkan1.2";; esac
>   glslangValidator -V $extra "$f" -o "/tmp/$(basename "$f").spv" || echo "FAIL $f"
> done
> for f in assets/shaders/*.spv; do spirv-val "$f" || echo "BAD $f"; done
> ```

### 1.2 审计口径

- **代码优先于文档**：所有断言都指向 `文件:行`；docs 只作为意图旁证。
- **P0**：默认/常见路径下功能失效或大面积错误输出；**P1**：条件明确、影响明显但可绕过；**P2**：质量/性能/边界问题；**P3**：健壮性、可维护性、文档漂移。
- 性能数字只要不是实测，全部标为「估算」并给出计数依据，不伪造帧时间。
- 测试程序 `gn_cornell_rt.comp` / `gn_cornell_rq.comp` 属于 `tests/cornellBox`，不进入引擎优先级；已在 §8 单独说明。

---

## 2. P0-1：SSGI 实际永远输出 0，开启后反而抹掉间接漫反射

### 2.1 完整代码链（这是本报告最重要的一条）

1. `sSSGI_UBO` 默认值把强度设为 0：
   - `src/graphics/giParams.cpp:294-304`
   - `ubo.params[0] = 0.0f; // 强度：默认关`
2. 初始化时把这份默认值上传到 SSGI UBO，之后**再也没有写 `params`**：
   - `src/graphics/_Vulkan/vkSsgi.cpp:294-299`（`makeDefaultSsgiUbo()` → `memcpy`）
   - `src/graphics/_Vulkan/vkSsgi.cpp:411-428`（`updateSsgiUniforms` 只写 `viewProj`、`invViewProj`、`screen`；`params` 在偏移 128，未被写）
3. 唯一的「强度设置」API 写的是**另一个 UBO**：
   - `src/graphics/_Vulkan/vkGi.cpp:108-119`：`GN_VkSetSsgiStrength()` 写 `sGI_UBO::envParams[3]`，不是 `sSSGI_UBO::params[0]`。
4. 但「SSGI pass 是否开启」却读的是 GI UBO：
   - `src/graphics/_Vulkan/vkSsgi.cpp:399-408`：`isSsgiEnabled()` 从 GI UBO 偏移 44 读 `envParams[3]`，`>0` 就返回 true。
5. 于是出现**互相矛盾的状态**：
   - CPU 认为 SSGI 已开启（`envParams[3]=1`），会执行 SSGI pass；
   - GPU 端 SSGI shader 看到的 `ssgi.params.x` 永远是 0，第一行就直接返回黑色：
     - `assets/shaders/gn3d_ssgi.frag:96-100`
     ```glsl
     void main() {
         if (ssgi.params.x <= 0.0) {
             outColor = vec4(0.0);
             return;
         }
     ```
6. 去噪 pass 把黑色继续平均成黑色；延迟光照又用这个黑色结果**替换** SH9 漫反射：
   - `assets/shaders/gn3d_deferred_light.frag:563-568`
   ```glsl
   vec3 envDiffuse = kD * albedo * ambientEnv * 0.31830988618 * gi.indirect.x * ao;
   vec3 ssgiRadiance = texture(ssgiTex, fragUV).rgb;      // 恒为 0
   vec3 ssgiDiffuse  = kD * albedo * ssgiRadiance * 0.31830988618 * gi.indirect.x * ao;
   float ssgiWeight  = clamp(gi.envParams.w, 0.0, 1.0);
   vec3 diffuseIBL   = mix(envDiffuse, ssgiDiffuse, ssgiWeight); // weight=1 时 → 0
   ```
7. 因此：
   - **SSGI 从未真正工作**；
   - **一旦调用 `GN_VkSetSsgiStrength(1.0)`，间接漫反射反而被清零**。Cornell 程序默认 `gSsgiStrength = 1.0`（`tests/cornellBox/main.cpp:118`），正好命中该回归；这也解释了「engine vs PT 的 RMSE=20.678、PT 偏亮」的部分原因（`.agent-memory/topics/cornellbox.md` M-19 记录了该对拍结果）。

### 2.2 最小修复

**方案 A（推荐，改动最小）**：让 `GN_VkSetSsgiStrength` 同时写 SSGI UBO 的 `params[0]`，并让 `updateSsgiUniforms` 不再覆盖它：

```cpp
// vkSsgi.cpp
void setSsgiParams(float strength, float steps, float thickness, float maxDistance) {
    if (GN_Vk.ssgiUBOMemory == VK_NULL_HANDLE) return;
    float p[4] = { strength, steps, thickness, maxDistance };
    void* mapped = nullptr;
    vkMapMemory(GN_Vk.device, GN_Vk.ssgiUBOMemory, 128, sizeof(p), 0, &mapped);
    if (mapped) { std::memcpy(mapped, p, sizeof(p)); vkUnmapMemory(...); }
}
```

`GN_VkSetSsgiStrength(strength)` 里同时调用它（或直接在 `vkGi.cpp` 内 map 两个 buffer）。  
**方案 B**：shader 不再读 `ssgi.params.x`，改用 `gi.envParams.w` 作为开关和强度；`params[1..3]` 保留步数/厚度/距离。  
**方案 C（不推荐）**：把 `params.x` 在 shader 里当作强度乘子，同时在 composite 端把权重固定为 1；这会改变现有 API 语义，且容易双重乘。

> 无论选哪种，**必须加一条 gpuCheck 断言**：开启 SSGI 后 `ssgiRaw` 的均值 > 0，且 deferred 输出与关闭 SSGI 有可判定差异（见 §9.2）。

---

## 3. P0-2 / P1：默认白图与 TBN 退化，导致法线错误/NaN

### 3.1 P0-2：默认 1×1 白图被当成法线贴图

**代码链：**

1. 引擎默认纹理是 1×1 **白色**：
   - `src/graphics/_Vulkan/GN_Vk.cpp:453`：`*px = 0xFFFFFFFF; // 白色`
   - `src/graphics/_Vulkan/GN_Vk.h:202`：`defaultImageView; // 1×1 白色默认纹理视图`
2. 没有法线贴图时，两个绘制入口都把这张白图绑到 normal 槽位：
   - 前向 PBR：`src/graphics/_Vulkan/vk3dForward.cpp:228-238`（`normalView = normalTex ? ... : defaultImageView`）
   - 延迟 G-Buffer：`src/graphics/_Vulkan/vk3dPasses.cpp:428-429`（同样回退到 `defaultImageView`）
   - 初始化时也直接写默认白图：`GN_Vk.cpp:1078-1081`、`vkGbuffer.cpp:262-263`
3. 两个片元着色器只用「长度 > 0.01」判断有没有法线贴图：
   - `assets/shaders/gn3d_gbuffer.frag:42-47`
   - `assets/shaders/gn3d_pbr.frag:438-444`
   ```glsl
   vec3 texNormal = texture(normalSampler, fragUV).rgb;
   if (length(texNormal) > 0.01) {
       texNormal = normalize(texNormal * 2.0 - 1.0);
       N = normalize(fragTBN * texNormal);
   }
   ```
   白图 `(1,1,1)` 的长度是 1.732，**必然通过判断**；解码后是 `(1,1,1)`，再归一化成 `(0.5774, 0.5774, 0.5774)` —— 不是 `(0,0,1)`。
4. 数值复算（本会话已执行）：

```text
white default (1,1,1): decode=[1, 1, 1] normalize=[0.5774, 0.5774, 0.5774]
flat normal (0.5,0.5,1): decode=[0.0, 0.0, 1] normalize=[0.0, 0.0, 1.0]
```

5. 影响面：
   - **所有没有显式 normal map 的网格**（前向 PBR 与延迟 G-Buffer 都中）；
   - MMD 查看器只给网格挂 `aAlbedoTex`（`tests/mmdViewer/mmdViewer.cpp:240`），`aNormalTex` 一直为空（`src/mmd/mmdMeshBuilder.cpp` 未设置；`sMESH::aNormalTex` 默认 `nullptr`），因此 **MMD 模型整体受影响**；
   - G-Buffer 法线错 → SSAO/SSGI/SSR/延迟光照全部跟着错；
   - Cornell 测试之所以正常，是因为它在场景里**显式创建了平面法线**并注释了这个坑：
     - `tests/cornellBox/cornellScene.cpp:131-137`
     ```cpp
     // 1×1 平面法线（R=0.5,G=0.5,B=1）：G-Buffer 默认白图会被当成 (1,1,1) 法线贴图，
     // 从而改写顶点法线；显式给平面法线，保证墙面/球面法线正确。
     unsigned char px[4] = {128, 128, 255, 255};
     ```
     这是「测试程序绕过引擎缺陷」的现场证据，不是修复。

**最小修复（推荐）**：为 normal 槽位单独准备一张 1×1 平面法线纹理（RGBA8：`128,128,255,255`），在 `GN_VkDraw3DPBR` / `GN_VkDraw3D_GBuffer` / 初始化描述符时用它替换 `defaultImageView`；albedo 继续用白图。  
**备选**：在 push constant 里加 `hasNormalMap` 标志，但需要改两个 shader + 两个绘制入口，且未来每个材质都要传，维护面更大。  
**不推荐**：在 shader 里用 `length(texNormal) > 1.5` 拒绝白图——真实法线贴图也可能接近 1.6，会误伤。

### 3.2 P1-1：TBN 零切线/平行切线产生 NaN

**前向 PBR**（`assets/shaders/gn3d_pbr.vert:35-42`）：

```glsl
vec3 N = normalize(mat3(pc.model) * inNormal);
vec3 T = normalize(mat3(pc.model) * inTangent); // inTangent=0 → normalize(0)=NaN
T = normalize(T - dot(T, N) * N);                // T∥N → normalize(0)=NaN
vec3 B = cross(N, T);
fragTBN = mat3(T, B, N);
```

- 没有零切线回退；
- 没有「切线与法线平行」回退；
- `inTangent` 在顶点格式里是「预留」字段：`GN_Vk.h:105` `float tx, ty, tz; // 切线 (预留)`；程序化/烘焙网格不填时就是 0；
- 测试代码已经踩过：`tests/gpu/gpuCheck3D.cpp:41` 注释 `// G-Buffer 顶点着色器对零切线 normalize 会得到 NaN`，所以测试里强制 `tx=1.0`，但**只测了 G-Buffer，没有测前向 PBR**。

**G-Buffer**（`assets/shaders/gn3d_gbuffer.vert:45-53`）：

```glsl
vec3 T = mat3(pc.model) * inTangent;
if (dot(T, T) < 1.0e-8) {                       // 只处理“原始切线为零”
    T = abs(N.y) < 0.99 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
}
T = normalize(T - dot(T, N) * N);               // 原始 T∥N 时仍 normalize(0)=NaN
```

- 回退发生在投影**之前**；如果原始切线非零但与 N 平行，投影后为 0 → NaN；
- MMD 网格的切线被固定写成 `(1,0,0)`（`src/mmd/mmdMeshBuilder.cpp:146-148`），OBJ 路径也把 `vtx[6]=1.0`（`src/graphics/GN_3Dmodel.cpp` 的 OBJ 顶点填充段）；因此**法线接近 ±X 的面**正好命中「平行」分支；
- 数值复算：`T=(1,0,0), N=(1,0,0)` → 投影后长度 = 0（本会话已复现）。

**为什么后果比“只是切线错”严重**：`fragTBN` 是 `mat3(T,B,N)`，只要 T/B 是 NaN，即使法线贴图是 `(0,0,1)`，矩阵乘法 `mat3 * vec3(0,0,1)` 也会把 `NaN*0` 传播成 NaN；再叠加 S-02 的默认白图，几乎所有无 normal map 的网格都会走这条路径。

**最小修复（两个 vert 共用）**：

```glsl
vec3 N = mat3(pc.model) * inNormal;
if (dot(N, N) < 1e-12) N = vec3(0.0, 0.0, 1.0);   // 零法线兜底
N = normalize(N);

vec3 T = mat3(pc.model) * inTangent;
T -= N * dot(T, N);                               // 先投影
if (dot(T, T) < 1e-8) {                           // 再检查退化
    vec3 axis = abs(N.y) < 0.9 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
    T = cross(axis, N);                           // 必然与 N 垂直
}
T = normalize(T);
vec3 B = cross(N, T);
```

同时建议：
- 用 `inTangent.w` 或 `sign(dot(cross(N,T), bitangent))` 处理镜像 UV 的手性；当前顶点格式没有 `w`，若未来接 glTF 法线贴图需要补；
- 为「零切线 / 平行切线 / 零法线」加 gpuCheck 用例（§9.2）。

---

## 4. P1-2：交换链格式与 sRGB 编码契约矛盾

**代码事实：**

- 交换链格式取驱动返回的**第一个**格式，没有优先级：
  - `src/graphics/_Vulkan/GN_Vk.cpp:209`：`GN_Vk.swapchainFormat = fmts[0].format;`
  - resize 同样：`GN_Vk.cpp:898`
- 色彩空间永远写成 `SRGB_NONLINEAR`：
  - `GN_Vk.cpp:216`、`GN_Vk.cpp:905`
- composite 注释假设「swapchain 是 SRGB 格式，硬件自动编码 Gamma」：
  - `assets/shaders/gn3d_composite.frag:103-104`
- 但上一会话真机实测交换链是 **`VK_FORMAT_R16G16B16A16_SFLOAT`（格式 97）**：
  - `.local/context-summaries/session-bb023ebd-cd10-4be0-80f2-a0118e366f0a-2026-11-03T11-15-18Z-handoff.md:12-13,39`
  - `.agent-memory/topics/cornellbox.md` M-cornellbox-18 的 evidence：`swapchainFormat=97=R16G16B16A16_SFLOAT`
- 对浮点格式，硬件**没有** sRGB 编码步骤；composite 输出的是线性值。当前截图链也承认这一点：`cornellCapture` 需要手动做 linear→sRGB，而不是直接读。

**风险分级：P1（环境相关）**。在一种情况下可能“看起来正常”：
- 某些合成器/驱动把 `R16G16B16A16_SFLOAT + SRGB_NONLINEAR` 当作 scRGB/线性处理（社区有同类记录，例如 Wine 的 float16 swapchain 行为讨论）；这种情况下屏幕上可能是对的。
- 但按 Vulkan 规范，`imageColorSpace` 是「呈现引擎解释图像值的色彩空间」；声明 `SRGB_NONLINEAR` 却写线性值，是**契约不一致**，换驱动/合成器就可能变暗、发灰或过亮。

**最小修复（推荐）**：
1. **LDR 输出优先选 8-bit sRGB 交换链格式**（`B8G8R8A8_SRGB` / `R8G8B8A8_SRGB`）和对应 `SRGB_NONLINEAR`；HDR 中间目标仍保持 `RGBA16F`。这样 composite 的注释才成立。
2. 若必须用 `RGBA16F` 交换链：
   - 使用支持线性色彩空间的 `VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT`（需查询 surface 支持）；
   - 或在 composite 里对输出做显式 linear→sRGB 编码（并让 `GN_VkReadBack` / 截图链知道这一点）。
3. 把「是否 sRGB 格式」做成运行时分支，不要用固定注释假设；在初始化日志里打印 `swapchainFormat` 与 `colorSpace`。

> 注意：这不仅是 shader 问题，也是 `GN_Vk.cpp` 的格式选择问题；但 shader 的注释/输出语义是矛盾的一半，必须一起改。

---

## 5. P2：SSGI / SSR / PCSS 的正确性与性能

### 5.1 S-05：SSGI 半分辨率却用 LINEAR 采样全分辨率 G-Buffer

- SSGI sampler：
  - `src/graphics/_Vulkan/vkSsgi.cpp:280-286`：`magFilter/minFilter = VK_FILTER_LINEAR`
- SSGI 目标是半分辨率：`vkSsgi.cpp:16-21`（`swapchainExtent/2`）
- 片元里用同一个 `fragUV` 采样全分辨率深度/法线：
  - `assets/shaders/gn3d_ssgi.frag:101-107`（`texture(gDepthTex, fragUV)`、`texture(gNormalTex, fragUV)`）
  - `assets/shaders/gn3d_ssgi_denoise.frag:14-26`（去噪同样）
- 对比：SSAO 用的是 NEAREST（`src/graphics/_Vulkan/vkSsao.cpp:285-286`）。

**影响**：半分辨率像素落在全分辨率 texel 之间时，深度/法线被线性混合，遮挡/命中判断在物体边缘出现假命中、漏光、AO/SSGI 边缘发糊。  
**修复**：为 SSGI 的深度/法线（以及命中点 albedo 之外的结构数据）单独创建 NEAREST sampler；或直接 `texelFetch` 到全分辨率坐标（`ivec2(fragUV * textureSize(gDepthTex, 0))`）。色彩/albedo 可继续 LINEAR。

### 5.2 S-06：SSR 的 LOD、粗糙度早退与亮度尺度

1. **隐式 LOD 未定义**：`traceSsr` 在带 `break/continue` 的循环里用 `texture()`：
   - `assets/shaders/gn3d_deferred_light.frag:370`（深度）、`:375`（法线）、`:377`（albedo）
   - 循环边界/分支逐像素不同，隐式导数在非一致控制流中未定义；应全部改 `textureLod(..., 0.0)`（屏幕空间反射不需要 mip）。
2. **粗糙度早退缺失**：
   - `gn3d_deferred_light.frag:577` 只判 `metallic > 0.05`；
   - 之后固定 64 步（`:361`），粗糙金属（roughness 接近 1）也跑满 64 次全屏步进；
   - 建议：`if (metallic <= 0.05 || roughness > 0.6) skip`，或按粗糙度动态降步数/半分辨率。
3. **SSR 亮度尺度错误**：
   - `:379` `outColor = ha * evalSH9(hn);`
   - `evalSH9` 返回 irradiance（`:99-114` 已含余弦卷积）；对 Lambertian 表面，出射 radiance = `albedo/π * irradiance`。这里少了 `/π`，且完全没有直接光/IBL/阴影，只拿环境辐照度当作镜面反射色 → **π 倍偏亮、反射内容错误**。
   - 建议：至少 `outColor = ha * evalSH9(hn) / PI`；正确做法是用命中点的真实 shading（直接光 + IBL），或直接回退预滤波环境。

### 5.3 S-07：PCSS 的早退与 range

- 延迟光照：`CalcPBRLightDeferred` 先做阴影（`gn3d_deferred_light.frag:448-458`），再在 `:476` 算 `NdotL`；即使表面背对光源，也会付 32 次纹理采样。
- 前向 PBR：`gn3d_pbr.frag:393-401` 同样先阴影，`:418` 才 `NdotL`。
- 每光源 PCSS = 16 次 blocker search + 16 次 PCF = **32 次纹理采样**（`gn3d_deferred_light.frag:319-341`；`gn3d_pbr.frag:330-357`）。
- 点/聚光的 `attenuation.w` 在 CPU 端写入的是 `range`（`src/graphics/GN_3Dlight.cpp:84/116`），但所有 shader 都没有读它（`grep attenuation.w` 0 命中）；当前衰减在 `d=range` 时仍有 1/(1+2+1)=0.25，超出 range 继续有贡献。
- **估算**：8 光源场景下，最坏每像素 256 次阴影采样；即使只有 2 个带阴影光源，背光面也会白付 64 次。按 1280×800 全屏估算，约 0.65–2.6 亿次纹理采样/帧（未计 G-Buffer/IBL），这是当前延迟路径最大的固定开销之一。

**修复**：
- 在 `CalcPBRLight*` 开头 `float NdotL = max(dot(N,Ldir),0); if (NdotL <= 0) return 0;`（注意仍要保留 `lc` 为零的早退）；
- 点/聚光先做 `d > range` 早退（range 从 `attenuation.w` 读），或至少用 range 做平滑截止；
- PCSS 降到 8–12 tap 的 blocker + 8 tap PCF，或半分辨率生成 blocker；能用硬件 compare sampler 的部分改用 compare；
- 把「阴影」放到 BRDF/早退之后，避免无效采样。

### 5.4 S-08：法线的非均匀缩放

`mat3(model)` 直接变换法线，只在均匀缩放/旋转下正确。非均匀缩放下应使用 `transpose(inverse(mat3(model)))`（或 CPU 传入逆转置）。

受影响：
- `assets/shaders/gn3d_gbuffer.vert:40`（注释 `// 世界空间法线 (mat3 变换, 正确处理非均匀缩放)` 与实现矛盾）
- `assets/shaders/gn3d_pbr.vert:35`
- `assets/shaders/gn3d.vert:34`
- `assets/shaders/gn3d_skinned.vert:63`（外层模型矩阵；蒙皮调色板内的缩放同理）

**修复**：在 push constant 里追加 3×3 逆转置（会扩大 96 B 的 push 常量），或 CPU 端把 model 拆成 `model` 与 `normalMatrix` 两个 mat4；也可在 shader 里对均匀缩放用 `mat3(model) / scale` 的特殊路径。项目当前 96 B push 常量没有余量，建议先确认是否有非均匀缩放的实际资产，再决定是否扩 push 范围。

### 5.5 S-09：天空盒反向 smoothstep

`assets/shaders/gn3d_skybox.frag:89`：

```glsl
float haze = smoothstep(0.18, -0.02, dir.y) * 0.55;
```

GLSL 规范：`smoothstep` 在 `edge0 >= edge1` 时**结果未定义**（Khronos 官方页明确写了 "Results are undefined if edge0 ≥ edge1"）。  
**修复**：

```glsl
float haze = (1.0 - smoothstep(-0.02, 0.18, dir.y)) * 0.55;
```

### 5.6 S-10：延迟光照 LightUBO 描述符越界

- `src/graphics/_Vulkan/vk3dInitScene.cpp:690`：
  ```cpp
  VkDescriptorBufferInfo dbi7 = {GN_Vk.lightBuffer3D, 0, 1024};
  ```
- 实际 buffer 大小：`sizeof(GN3d::sLIGHT_UBO) = 976`（`src/graphics/GN_3Dlight.h:56` static_assert；`vk3dInit.cpp:421` 也按这个大小分配）。
- `range=1024 > bufferSize=976`，违反 `VkDescriptorBufferInfo` 的 range 约束；Validation Layer 会报 `VUID-VkDescriptorBufferInfo-range-...`，部分驱动可能读到相邻内存。
- 修复：改成 `sizeof(GN3d::sLIGHT_UBO)`；同时清理 `vk3dInit.cpp:419` 注释里的 `1744 B`（早已过时）。

---

## 6. P2/P3：其余值得登记的缺陷

| 编号 | 位置 | 问题 | 影响/建议 |
|---|---|---|---|
| S-11 | `assets/shaders/gn3d_bloom_down.frag:39-45` | 先做 13-tap 下采样、再对结果做阈值；亮斑在阈值前已经被邻域摊开 | 亮部光晕变宽、阈值不稳定；建议先对源做阈值/软膝盖，再下采样，或使用预设 prefilter |
| S-12 | `assets/shaders/gn3d_cloud_cube.frag:102-121` | slab 求交 `1.0 / ray` 在 `ray` 分量为 0 时产生 `inf`；若分子也为 0 则 `0*inf=NaN`，后续 `tf <= tn` 对 NaN 为 false | 相机正好在盒面平面上、且射线沿该轴时可能带着 NaN 步进；建议对 `abs(ray)<eps` 单独处理（用 `t = (plane - origin)/ray` 的极限分支，或把退化轴排除） |
| S-13 | `assets/shaders/gn3d_skyview.comp:103` | compute 里用 `texture(transmittanceLut, ...)`；非 fragment 阶段的隐式 LOD 没有保证，应该用 `textureLod(..., 0.0)` | 当前 LUT 只有 1 级 mip，实际影响小；但属于 portability/规范问题 |
| S-14 | `assets/shaders/gn3d_pbr.frag:431-434` 后接 `:438/451/512` | `discard` 之后才采样带 mip 的纹理；discard 造成非一致控制流，隐式导数未定义 | alpha cutout（MMD 头发/睫毛）边缘可能闪烁；建议把法线/albedo/alpha 采样全部提到 discard 之前，或对需要 mip 的采样用 `textureLod` |
| S-15 | `assets/shaders/gn3d_pbr.frag:432/451/512` | 同一张 albedo 纹理每像素采样 3 次（alpha test、albedo、outAlpha） | 中低端 GPU 上可测；缓存到局部变量即可 |
| S-16 | `assets/shaders/gn3d_deferred_light.frag:118-131/558-560` | 探针每像素 9 张 3D 纹理各采样一次；9 次 3D fetch 是延迟路径的固定成本 | 可评估改成每探针一次 3×3 texel 读取/预积分 irradiance cubemap；在 780M 上可能值得，但需要实测，不要盲目改格式 |
| S-17 | `assets/shaders/gn3d_deferred_light.frag:199-249/590-593`；`gn3d_pbr.frag:122-172` | 空气透视每像素 8 步 + 每步一次 LUT 采样；前后向各有一份 | 可用半分辨率 AP 或 2D LUT（相机高度/视线天顶角/距离）降成本；注意与 fog 的叠加顺序 |
| S-18 | `assets/shaders/gn3d_ssao.frag:16`；`gn3d_ssgi.frag:14-15`；`gn3d_deferred_light.frag:51/90-96` | 死参数/死字段：`ssao.params.w`、`ssgi.params[1]`（步数）、`ssgi.screen`、`shadowParams.z/w`、`cascadeSplits`、`skyViewLut`（延迟路径）、`gi.aoParams` | 文档/契约与实现漂移；要么接线，要么从 UBO/注释删除，避免后续按“参数存在”做功能 |
| S-19 | `assets/shaders/gn3d_gbuffer.frag:68` | 附件 2 的 G=0.04 F0、B=materialID 基本未消费；alpha 写入但延迟不支持 BLEND | 已知限制（`docs/GI_PLAN.md` §5.1 条目 4/9）；要么实现，要么在 README/契约里明确「延迟路径只支持 OPAQUE/MASK」 |
| S-20 | `assets/shaders/gn3d_deferred_light.frag:496-499` | 延迟天空背景采样的是**启动时烘焙的 skybox cubemap**，而可见天空是 `gn3d_skybox.frag` 的 LUT/解析天空；GI_PLAN §1 已记录“可见天空与 IBL 不是同一张图” | 背景与 IBL/天空不一致；Cornell 对拍若要黑背景，需要显式黑 env/跳过天空，否则 RMSE 被污染 |
| S-21 | `assets/shaders/gn3d_cloth.vert:29-30`；`gn3d_cloth.comp:33-35` | `width`/`height` 为 0 时 `%`/`/` 未定义/除零；`width*height` 还可能溢出 uint | C++ 若保证 >0 则只是防御性；建议 shader 里 `if (width==0u || height==0u) return;` |
| S-22 | `assets/shaders/gn3d_ssgi.frag:45-68` | 命中点直接光不处理 spot（type=2），也不做阴影/可见性 | 间接光会漏光；v1 可接受，但应在契约里写明“只处理方向光/点光、无阴影” |
| S-23 | `assets/shaders/gn3d_ssao_blur.frag:36`；`gn3d_ssgi_denoise.frag:26` | 深度权重用 NDC/原始深度差 ×1000；随相机 near/far、深度分布变化 | 不同场景/相机的去噪强度不一致；建议线性化深度或用世界距离 |
| S-24 | `assets/shaders/gn3d_gbuffer.vert:39`；`gn3d_composite.frag:103`；`vk3dInit.cpp:419`；`vkCloudCube.cpp:43` | 注释/static_assert 文案过时：非均匀缩放、sRGB、1744 B、1168 B | 会误导后续维护；建议本轮一并修正 |
| S-25 | `assets/shaders/gn3d_pbr.frag` 与 `gn3d_deferred_light.frag` | SH9/PCSS/BRDF/fog/AP 整套代码重复（522 行 vs 595 行），已经出现常量/行为漂移风险 | **2026-11-21 完成**：纯数学 7 函数抽到 `brdfLighting.glsl`；随后统一两文件的 UBO 实例/字段名（`scene.cp`→`cameraPos`、`ld`→`lightData`），把 `evalSH9`/`applyAerialPerspective`/`fogAmountAt`/`sampleSunTransmittance`/`shadowCompare`/`selectCascade` 抽到 `sceneLighting.glsl`（调用方须先声明 Scene/Fog/GI/Light UBO 与采样器）。行数：pbr 525→363、deferred 606→445、公共 192 行。验收：`make shaders` 47/47、引擎截图 `00550dcf…` 逐字节一致、`make unit` 420/0 |
| S-26 | `assets/shaders/gn3d_composite.frag:53-89` | FXAA 邻域样本只做 HDR×曝光+tonemap，不含 Bloom；bloom 亮边不参与抗锯齿 | 注释已承认；属于取舍，不是硬缺陷；若 bloom 强度大，边缘可能仍闪 |
| S-27 | `assets/shaders/gn3d_skybox.frag:71-75` | `overcast` 用 `sunIntensity` 作为阴天代理；夜间 `sunIntensity≈0` 会被判成“全阴天”，天空被压成灰色 | 夜晚/低太阳高度时可能过亮/过灰；建议改用独立的天气 overcast 参数 |
| S-28 | `docs/GI_PLAN.md:218`（该引用已过时） | **已核销**：`src/graphics/_Vulkan/vkGbuffer.cpp:167-190` 当前已显式创建 external→subpass0、subpass0→external 两条 `VkSubpassDependency` | 无需修改；审计时引用的 GI_PLAN 条目早于这段代码 |
| S-29 | `src/graphics/_Vulkan/vkImageIo.cpp:232` | **本轮已修复**：`GN_VkReadBack` 按 swapchain 格式读回（RGBA16F→half→8-bit 量化、B8G8R8A8→RGBA 重排），不再固定 4 B/px | 仍需真机验证 sRGB/浮点两种 swapchain 的截图颜色 |

---

## 7. 优化方案（按阶段，含预期收益与风险）

### 7.1 阶段 0：正确性止血（P0，建议先做）

| 步骤 | 改动 | 验收 |
|---|---|---|
| 0.1 | 修复 SSGI 参数链（S-01）：`GN_VkSetSsgiStrength` 写 `sSSGI_UBO::params[0]`；`updateSsgiUniforms` 不覆盖 params；在 shader 里保留 `params.x` 作为开关 | 开启 SSGI 后 `ssgiRaw` 非零；关闭时逐像素与旧路径一致；Cornell engine vs PT 的 RMSE 下降（预期，需真机） |
| 0.2 | 为 normal 槽位绑定 1×1 平面法线（S-02）：`flatNormalView` 在 PBR 与 G-Buffer 两个入口替换 defaultImageView | G-Buffer 法线附件在无 normal map 的平面上为 `(0.5,0.5,1)`；MMD 光照不再整体偏 45° |
| 0.3 | 两个 vert 的 TBN 退化回退（S-03），投影后检查 + 零法线兜底 | 零切线、切线∥法线、零法线三个用例都不产生 NaN/黑块 |

**预计**：0.1 是功能修复，不直接提升帧率；0.2/0.3 修正画质，几乎无性能成本（多几条 ALU）。  
**风险**：0.1 会改变 `GN_VkSetSsgiStrength(0)` 的默认路径；要确保关闭时 `ssgi.params[0]=0`，且 `isSsgiEnabled` 与 `params` 状态一致。

### 7.2 阶段 1：P1 修复

| 步骤 | 改动 | 验收 |
|---|---|---|
| 1.1 | 交换链格式选择（S-04）：优先 8-bit sRGB；或 float 时显式 linear→sRGB/线性色彩空间 | 启动日志打印 `swapchainFormat/colorSpace`；截图与 HDR 直抓颜色一致；换驱动不偏色 |
| 1.2 | SSGI 深度/法线改为 NEAREST/texelFetch（S-05） | 半分辨率 SSGI 在深度边缘无跨物体假命中；A/B 截图边缘差异下降 |
| 1.3 | SSR `textureLod` + 粗糙度早退 + `/π`（S-06） | 粗糙金属不再跑 64 步；反射亮度与 PT/参考值同量级 |
| 1.4 | PCSS 早退 + range（S-07） | 背光面不再做 32 tap；8 光源场景 GPU 时间下降（需实测） |
| 1.5 | 法线逆转置（S-08）或明确“非均匀缩放不支持” | 非均匀缩放 A/B 法线一致；若暂不支持，写进 API 契约 |
| 1.6 | 天空盒 smoothstep 反向（S-09） | 地平线雾不再依赖驱动未定义行为 |
| 1.7 | LightUBO range 1024→976（S-10） | Validation 0 error；descriptor 写入无越界 |

### 7.3 阶段 2：性能优化（P2，按收益/风险排序）

> 下列收益是**基于调用次数与像素数的估算**，不是本机实测；需要在真机上用 GPU timestamp/`VK_EXT_calibrated_timestamps` 或 `apitrace` 验证。

| 优化 | 现状成本（估算） | 建议 | 预期收益 | 风险 |
|---|---|---|---|---|
| PCSS 早退 + range | 每光源 32 tap；背光面全白付 | `NdotL<=0` 先返回；range 外跳过；减少到 8–12 tap | 典型 8 光源场景阴影采样可降 50% 以上（估算） | 软阴影质量下降，需要 A/B |
| SSGI 降步数/时域 | 8 rays×8 steps=64 tap（半分辨率） | 4 rays×8 steps + 4 帧时域累积；`params[1]` 驱动步数；蓝噪声 | 采样数约 -50%，时域后噪声更低 | 需要 velocity/重投影；动态物体有拖影 |
| SSR 早退/半分辨率 | 64 步全屏，roughness 无上限 | roughness>0.6 跳过；半分辨率 + 双边上采样 | 金属像素成本可降 2×（估算） | 半分辨率反射边缘需处理 |
| 探针 9×3D fetch | 每像素 9 次 | 预积分 irradiance cubemap 或每探针一次 3×3 block；按需更新 | 探针路径成本显著下降（需实测） | 需要改探针数据布局/更新 |
| AP 8 步 | 每像素 8 步 + 8 次 LUT | 半分辨率 AP 或 2D LUT（高度/天顶角/距离） | AP 成本约 -50%~-75%（估算） | 近处精度下降，需对比 |
| G-Buffer 打包 | 16 B/px（不含深度）+ 4 B 深度 | 法线 octahedral `RG16F` 4 B；albedo/AO 已 RGBA8 | G-Buffer 带宽/显存约 -20%（估算） | 法线精度、编解码 ALU；参考 `docs/GI_PLAN.md` §5.2 |
| TBN varying | `mat3` 占 3 个 varying | 只传 T/B，片元重建；或传四元数 | 顶点/插值带宽小降 | 片元 ALU 略增 |
| Bloom | 13-tap 下采样 | 阈值前置/预滤波 | 质量为主，性能中性 | 需要重调 bloom 参数 |
| 光照分簇 | 所有像素循环所有光源 | 先做 range 早退/屏幕空间光源列表；再考虑 Forward+/Cluster | 多光源场景成本线性下降 | 架构改动大，先不要做 |

### 7.4 阶段 3：架构与 RT（与现有规划对齐）

- **SSGI/SSR 的 ray query 化**：按 `docs/ENGINE_RT_GI_PLAN.md` 的 S2/S3，把屏幕空间 ray march 替换为 TLAS `rayQueryEXT`；保留屏幕空间回退。注意 shader 需要 GLSL 460 + `--target-env vulkan1.2`（Makefile 已有先例 `gn_cornell_rq.comp`）。
- **统一光照片段**：抽 `lighting.glsl`（SH9、PCSS、BRDF、fog、AP），由 Makefile 用 `-I`/`#include` 或构建期拼接；否则 pbr/deferred 两份 500+ 行代码会继续漂移。
- **TAA/velocity**：GI_PLAN §5.1 条目 8 已指出；没有 velocity，SSGI 时域和 SSR 降噪都受限。建议先在静态相机下做累积，动态物体用 depth reprojection 的近似版本。
- **G-Buffer 调试与验证**：把 `gn3d_gbuffer_debug` 的 4 通道扩展为 normal/albedo/AO/materialID + 深度；在 gpuCheck 中锁像素值。

---

## 8. 测试程序着色器说明

- `assets/shaders/gn_cornell_rt.comp`、`gn_cornell_rq.comp` 只被 `tests/cornellBox` 使用，不属于引擎管线。
- 两者都已编译/校验通过；实现里有 NaN/Inf 防护（`gn_cornell_rq.comp:128-134`、`gn_cornell_rt.comp:214`），并且对背面命中做了 miss 处理（M-cornellbox-15）。
- 本轮不把它们列入引擎 P0/P1；若后续把 Cornell 作为 GI 回归基准，建议把它们的 `pow(value, 1/2.2)` 输出 gamma、累积样本数和相机参数纳入 golden image 口径（当前是测试工具行为，不是引擎缺陷）。

---

## 9. 验证计划

### 9.1 已有命令

```bash
cd /home/ruicat/下载/GnVk/GnVk
make shaders                 # 45 个 GLSL → SPIR-V
make unit                    # 396 cases / 0 failed（当前基线）
make gpuCheck                # 需要真 GPU；本会话无 /dev/dri，未执行
```

### 9.2 建议新增的 gpuCheck 用例（按优先级）

1. **`3d.ssgi_on`（S-01，最高优先）**
   - 开启 GI + `GN_VkSetSsgiStrength(1.0)`；
   - 断言 `ssgiRaw` 图像均值 > 1e-5（当前必失败，因为恒 0）；
   - 断言开启/关闭 SSGI 的最终画面有可判定差异；
   - 断言关闭 SSGI 时与旧路径逐像素一致（回退正确性）。
2. **`3d.normal_default`（S-02）**
   - 画一个没有 normal map 的平面，法线 `(0,0,1)`；
   - 读 G-Buffer attachment 0，断言中心像素 ≈ `(0.5,0.5,1.0)`（允许量化误差）；
   - 当前实现会得到约 `(0.79,0.79,0.79)`，必失败。
3. **`3d.tbn_degenerate`（S-03）**
   - 三个子用例：`inTangent=0`、`inTangent=N`、`inNormal=0`；
   - 断言法线附件无 NaN、画面不是全黑/全白；
   - 建议同时跑前向 PBR 与 G-Buffer 两条管线（当前 gpuCheck 只测了 G-Buffer）。
4. **`3d.ssgi_halfres_edge`（S-05）**
   - 构造深度突变的两个面；开启 SSGI；
   - 断言边缘两侧 SSGI 值不互相渗透（nearest 采样）。
5. **`3d.ssr_roughness`（S-06）**
   - 金属球 roughness 0.1/0.9 两组；
   - 断言 roughness 0.9 时 SSR 被跳过（可用 GPU timestamp 或输出标记）。
6. **`3d.colorspace`（S-04）**
   - 打印 `swapchainFormat`/`colorSpace`；如果格式不是 `*_SRGB`，断言 composite 走了显式编码分支；
   - 用已知灰度图做屏幕读回，与 sRGB 参考比对。

### 9.3 验证纪律

- 先跑 `make shaders` + `make unit`（不需要 GPU）；
- 真机跑 `make gpuCheck` + 新增用例；没有 GPU 时如实标注「未执行」，不要用静态分析冒充运行验证；
- 每个修复只重跑受影响用例（重试 ≠ 重验）；输入指纹未变不重复审查。

---

## 10. 附录 A：契约核对表（本会话逐项核对）

| 契约 | C++ 定义 | Shader 声明 | 结论 |
|---|---|---|---|
| SceneUBO | `VkSceneUBO` 96 B | `gn3d_*.vert/frag` `mat4+vec4+vec4` | 一致 |
| LightUBO | `sLIGHT_UBO` 976 B（`GN_3Dlight.h:56`） | `gn3d_deferred_light.frag:43-53` 976 B；`gn3d_pbr.frag` 前缀 | 布局一致；**deferred 描述符 range 1024 越界（S-10）** |
| SSAO UBO | `sSSAO_UBO` 400 B | `gn3d_ssao.frag:12-17` | 一致 |
| SSGI UBO | `sSSGI_UBO` 160 B | `gn3d_ssgi.frag:11-16` | 布局一致；**params[0] 写入链断裂（S-01）** |
| GI UBO | `sGI_UBO` 256 B | `gn3d_deferred_light.frag:65-71` / `gn3d_pbr.frag:38-44` | 一致 |
| Fog UBO | `sFOG_UBO` 64 B | `gn3d_deferred_light.frag:57-62` / `gn3d_pbr.frag:30-35` | 一致 |
| Sky UBO | 144 B（viewProj 64 + VkSkyParams 80） | `gn3d_skybox.frag:15-22` 144 B | 一致 |
| Cloud Cube UBO | `sCLOUD_CUBE_UBO` 4240 B（`vkCloudCube.cpp:43`） | `gn3d_cloud_cube.*` 4240 B | 一致（static_assert 文案 1168 过时） |
| G-Buffer 附件 | `RGBA16F/RGBA8_SRGB/RGBA8_UNORM/D32` | `gn3d_gbuffer.frag:35-37` | 一致 |
| Swapchain | `fmts[0]` + `SRGB_NONLINEAR` | composite 假设 sRGB 硬件编码 | **不一致（S-04）** |

## 11. 附录 B：本次审计的边界

- **没有 GPU**：本会话 `/dev/dri` 不存在，无法跑 `gpuCheck`、无法看画面、无法测帧时间；所有“画面会更差/更好”的结论都是代码与规范推导，已标注。
- **未修改任何 shader/C++**：本报告是分析与方案，不是补丁；所有修复建议都需要后续单独实施 + 真机验收。
- **严重度依赖默认路径**：若某个应用总是显式提供 normal map/切线，S-02/S-03 的触发面会缩小；但引擎默认 API 仍应修正。
- **文档可能过时**：`docs/GI_PLAN.md`、`docs/CHANGELOG.md` 中的行号/数量与当前 HEAD 有漂移；本报告以 `HEAD=ffdf0bb` 的代码为准。

## 12. 参考

- Khronos GLSL `smoothstep` 规范（`edge0 >= edge1` 未定义）：<https://registry.khronos.org/OpenGL-Refpages/gl4/html/smoothstep.xhtml>
- GLSL 4.50 规范（隐式导数/非一致控制流）：<https://registry.khronos.org/OpenGL/specs/gl/GLSLangSpec.4.50.pdf>
- Vulkan 色彩空间与 swapchain（`imageColorSpace` 解释）：<https://stackoverflow.com/questions/65241749/what-is-vulkan-color-space>（二手资料，用于说明「float + SRGB_NONLINEAR」的歧义；最终以 Vulkan spec 为准）
- 项目内：`docs/GI_PLAN.md` §4.3/§5.2/§6、`docs/ENGINE_RT_GI_PLAN.md` S2/S3、`.agent-memory/topics/cornellbox.md` M-18/M-19/M-20。

---

> 结论一句话：**先把 SSGI 的参数链修通、把 normal 槽位的默认白图换成平面法线、把 TBN 的退化分支补齐**；这三件事不需要 GPU 也能确定是错的，且直接影响当前 Cornell/GI 验收的可信度。其余按 §7 的阶段推进。
