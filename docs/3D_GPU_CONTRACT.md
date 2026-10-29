# P1c GPU / P2 GPU 蒙皮 / P8 渲染接入 — 冻结契约与写域（2026-10-29）

> 本轮目标：把 3D 规划的剩余 GPU 部分落地，**每片都能独立验收**。P9（GPU 粒子/布料/SSBO/批处理）
> 已全部由 Lead 实现（见 §3 P9 三段），CPU 回退保留。
>
> 基线（本机实测，AMD Radeon 780M / RADV，Wayland）：
> `make unit` = 256 cases 0 failed；`make gpuCheck` = 73/73 PASS（P9 之后）。

## 0. 环境事实（所有执行体共用）

- 沙箱**有真实 GPU 和桌面显示**：`/dev/dri/renderD128`、`DISPLAY=:1`、`WAYLAND_DISPLAY=wayland-1`。
  `make gpuCheck` 可直接跑（会开可见窗口）。不需要请人代跑。
- 构建：`make -j8`（引擎库）、`make unit`（单测）、`make gpuCheck`（GPU 验收）。
- 着色器：`glslangValidator -V <file>.vert -o <file>.vert.spv`；**只编译自己新增的着色器**，
  不要跑 `make shaders`（它会重写全仓 `.spv`，把无关改动带进 diff）。
- 禁止：`git checkout/clean/stash/reset`、`sudo`、改共享文件、改别人写域、动 `.local/`。
- 工作区已有其它工作流（sky/weather、P10 LOD、MMD）的未提交改动，**不要动、也不要提交**。

## 1. 共享文件（已由 Lead 改好，执行体只读）

- `src/graphics/_Vulkan/GN_Vk.h`：已声明本轮公共 API（见下）。
- `src/graphics/_Vulkan/vkInternal.h`：已声明 `init3DMaterial/destroyMaterial`、
  `init3DSkinned/destroySkinned`、`init3DDebugLines/destroyDebugLines`。
- `src/graphics/_Vulkan/GN_Vk.cpp`：`GN_VkInit3D()` 末尾按顺序调用三个 init（失败只告警）。
- `src/graphics/_Vulkan/vk3dShutdown.cpp`：`GN_VkShutdown3D()` 开头调用三个 destroy。
- `tests/gpu/gpuCheckMain.cpp`：已 include 并调用 `RunMaterialChecks/RunSkinChecks/RunCullChecks`。
- 三个骨架实现文件（`vkMaterial.cpp` / `vkSkinDraw.cpp` / `vkDebugDraw.cpp`）与三个验收骨架
  （`tests/gpu/gpuCheck{Material,Skin,Cull}.cpp/h`）已创建，**已归属到各执行体，骨架可直接替换**。

## 2. 公共 API（冻结，不得改动签名）

```cpp
// P1c：材质绑定的 GPU 资源（sMATERIAL_BINDING::aResource 按 aKind 指向其一）
struct sMATERIAL_UBO_RESOURCE { VkBuffer aBuffer; VkDeviceSize aOffset; VkDeviceSize aRange; };
struct sMATERIAL_TEX_RESOURCE { VkImageView aImageView; VkSampler aSampler; VkImageLayout aLayout; };

bool GN_VkMaterialCreate(GN3d::sMATERIAL_RES *material);   // 成功写 material->aDescriptorSet
void GN_VkMaterialDestroy(GN3d::sMATERIAL_RES *material);
void GN_VkDraw3DMaterial(const void *vertices, uint32_t vertexCount,
                         const uint32_t *indices, uint32_t indexCount,
                         const sMAT44 *modelMatrix, const GN3d::sMATERIAL_RES *material);

// P2：GPU 蒙皮（vertices = GN3d::sSKINNED_VERTEX 数组，68 字节/顶点）
bool GN_VkDraw3DSkinned(const void *vertices, uint32_t vertexCount,
                        const uint32_t *indices, uint32_t indexCount,
                        const sMAT44 *palette, uint32_t paletteCount,
                        const sMAT44 *modelMatrix, float r, float g, float b);

// P8：3D pass 内调试线框（lines = GN3d::sDEBUG_LINE 数组，世界坐标；颜色 0xAARRGGBB）
void GN_VkDrawDebugLines(const void *lines, uint32_t lineCount, unsigned int color);
```

## 3. 验收口径（必须真实 GPU 跑通，写进 `make gpuCheck` 输出）

三条共同要求：

1. 新增检查在 `make gpuCheck` 里输出 `[PASS]`，失败要有可定位的 detail（实际值/期望值）。
2. 管线一律经 `GN_Vk.aPipelineCache.GetOrCreate()` 创建并计数，不直接 `vkCreateGraphicsPipelines`
   （缓存是 P1b 的既有契约）。
3. 资源在 `destroy*` 里释放干净；`gpuCheck` 结尾的 `gpu.shutdown` 仍须 PASS。

### P1c（`p1c-gpu-material`）

- 固定 set 0 布局：binding 0 = UNIFORM_BUFFER（`sMATERIAL_UBO_RESOURCE`），
  binding 1 = COMBINED_IMAGE_SAMPLER（`sMATERIAL_TEX_RESOURCE`）。
- 描述符集从 `GN_Vk.descriptorPool` 分配；`cMATERIAL_CACHE` 的 CPU 去重逻辑不变。
- 材质管线：1 条 graphics 管线（VS/FS 新增 `assets/shaders/gn3d_material.*`），
  renderPass = `GN_Vk.renderPass3D`，push constant = 64B model 矩阵。
- GPU 验收（`gpuCheckMaterial.cpp`）：
  1. 建 UBO（颜色 0.25/0.5/0.75）+ 8×8 纯色纹理（白色），`sMATERIAL_DESC` 合法且键稳定；
  2. `GN_VkMaterialCreate` 成功、`aDescriptorSet != VK_NULL_HANDLE`；重复 Destroy 安全；
  3. 材质管线非空、两次绘制命中缓存（miss 不增加）；
  4. 在全屏四边形上绘制材质，回读中心像素 ≈ UBO 颜色（±0.02），证明 UBO+采样器都绑对了；
  5. 非法描述（绑定 0 不是 UBO / 缺 binding 1）返回 false 且不改材料。

### P2（`p2-gpu-skin`）

- 顶点布局 = `GN3d::sSKINNED_VERTEX`（stride 68）：
  loc0 pos vec3(0)、loc1 normal vec3(12)、loc2 uv vec2(24)、loc3 tangent vec3(32)、
  loc4 joints u16vec4(44, `VK_FORMAT_R16G16B16A16_UINT`)、loc5 weights vec4(52)。
- set 0：binding 0 = 场景 UBO（`GN_Vk.uniformBuffer3D`，`VkSceneUBO`），binding 1 = 关节调色板 8KB UBO。
  调色板按 `GN_Vk.currentFrame` 选环形槽位（建议 3 槽，避免覆盖在飞帧）。
- 着色器：`assets/shaders/gn3d_skinned.vert/.frag`；VS 先做 LBS（`sum w_i * palette[j_i] * pos`），
  再乘 model 与 viewProj；法线用 palette 的 3×3。
- GPU 验收（`gpuCheckSkin.cpp`）：
  1. 管线非空、经缓存创建；调色板 UBO 大小 = 128×16×4；
  2. 单关节 palette = 平移 +0.5x，蒙皮后立方体中心像素按位移变化（与 CPU `skinVertices`
     的参考位置一致，容差 ±0.03）；
  3. 双关节 0.5/0.5 混合的参考点与 CPU LBS 一致；
  4. `paletteCount > 128` 截断不越界（用 129 个关节，仅前 128 生效）；
  5. 未初始化/空顶点返回 false 不崩溃（可用 `GN_VkDraw3DSkinned` 参数校验覆盖）。

### P8（`p8-cull-debug`）

- `GN3d::cullDrawModelsPBR(const sFRUSTUM&, cMODEL *const *, uint32_t count, uint32_t *outVisible)`
  （声明加在 `GN_3Dmodel.h`，实现加在 `GN_3Dcull.cpp`；返回绘制的模型数，`outVisible` 可空）。
  它复用 `cullModels` 的可见列表，只对可见模型调 `DrawPBR`。
- `GN_VkDrawDebugLines`：LINE_LIST 管线经缓存创建，renderPass = `renderPass3D`，
  set 0 = 场景 UBO（binding 0），深度测试开/写关；输入 `sDEBUG_LINE` 世界坐标。
- GPU 验收（`gpuCheckCull.cpp`）：
  1. 三个立方体（原点、+2x、-2x），视锥只覆盖原点与原 +2x → `cullDrawModelsPBR` 返回 2，
     `outVisible == 2`；像素上原点可见、-2x 位置是背景色；
  2. 空指针/空列表安全：count=0 返回 0；
  3. 画一条过屏幕中心的调试线 → 中心像素 ≈ 请求的颜色（±0.05），线外仍是背景；
  4. 单测补充（`tests/unit/testCull.cpp`）：`cullDrawModelsPBR` 的计数/空指针/满 outVisible 上限
     （无 GPU 时绘制是 no-op，只验证计数）。

### P1c 纹理（2026-10-29 追加，Lead 直接实现）

- `GN_3Dtexture.h/.cpp`（CPU）：PNG/JPEG 内存解码 → RGBA8（top-down）、data URI base64、
  文件头格式识别；损坏输入返回 false 且不改 out。
- `_Vulkan/vkTexture.cpp`：RGBA8 → `R8G8B8A8_SRGB` VkImage（staging + 布局转换），
  glTF sampler 枚举 → VkSampler；`GN_VkCreateTextureRGBA8` / `GN_VkDestroyTexture`。
- `sASSET_TEXTURE::aImageBytes` 保存 bufferView（.glb/内嵌）与相对 `.gltf` 的外部图片字节；
  data URI 仍由解码器按 `aUri` 解。
- 验收：`testTexture` 7 用例 + `gpuCheck` 5 条（`texture.decode/glTF/upload/sample/destroy`）。

### P9 GPU 粒子（2026-10-29，Lead 直接实现）

- 契约（`src/graphics/GN_3DparticleGPU.h`）：`sGPU_PARTICLE` = 64B（四个 vec4：
  pos+age / vel+life / color / size+seed+alive+pad），CPU、`gn3d_particle.comp`、
  `gn3d_particle.vert` 三方共用，改字段必须三处同步；`sPARTICLE_SIM_DESC{gravity, drag}`；
  `sPARTICLE_DRAW_PARAMS` = 144B std140 镜像（mat4 + 5×vec4）；
  `stepParticlesCpu()` = 同一公式的 CPU 回退与 GPU 对拍参考。
- API（`GN_Vk.h`）：`GN_VkParticleCreate/Upload/Simulate/Draw/Data/Destroy`。
- 实现：`_Vulkan/vkParticleCompute.cpp`（host-visible SSBO 持久映射 + compute 管线经
  `aPipelineCache` 的 `ePIPE_COMPUTE` 分支 + 独立命令缓冲/fence 同步）；
  `_Vulkan/vkParticleDraw.cpp`（顶点 pulling 的 billboard 管线：无顶点/索引缓冲，
  `gl_InstanceIndex` 直读 SSBO；深度测试开、写关、双面、标准 alpha 混合）。
- 着色器：`gn3d_particle.comp`（local_size 64）、`gn3d_particle.vert/.frag`。
- 描述符池补 `STORAGE_BUFFER` 256 个（`GN_Vk.cpp`）。
- 验收：`testParticleGPU` 6 用例（布局/参数校验/CPU 步进）+ `gpuCheck` 7 条
  （`particle.create/compute/cpuMatch/death/draw/invalid/perf`）。
- 边界：v1 不自动重生（寿命到只清 `aAlive`）；每步 fence 等待、不跨帧流水化，
  65k 粒子实测 GPU 0.26 ms/步 vs CPU 0.10 ms/步（提交+等待开销占主导），收益需大批量或流水化；
  按 SSBO 顺序绘制、无半透明排序；无 GPU 图元裁剪。

### P9-a GPU 布料（2026-10-29，Lead 直接实现）

- 契约（`src/graphics/GN_3DclothGPU.h`）：`sCLOTH_GPU_VERTEX` = 32B（pos+pinned / prev+pad）；
  `sCLOTH_GPU_DESC{width, height, spacing, gravity, damping, iterations}`；
  `stepClothCpu()` = 同一套 Verlet 积分 + Jacobi 距离约束的 CPU 回退与 GPU 对拍参考（容差 1e-3）。
- API：`GN_VkClothCreate/Upload/Simulate/Data/Draw/Destroy`。
- 实现：`_Vulkan/vkClothCompute.cpp`（A/B 双 host-visible SSBO 乒乓：积分 A→B、每轮约束翻转；
  迭代结束若结果在 B 则 `vkCmdCopyBuffer` 回 A；单条命令缓冲 + fence 等待）；
  `_Vulkan/vkClothDraw.cpp`（顶点 pulling 的网格管线 + Create 时生成的网格索引缓冲）。
- 着色器：`gn3d_cloth.comp`（一个着色器两种模式：积分 / 约束）、`gn3d_cloth.vert/.frag`。
- 验收：`testClothGPU` 5 用例（布局/参数校验/重力+pinned/决定性/守卫）+ `gpuCheck` 5 条
  （`cloth.create/compute/cpuMatch/draw/invalid`）；20 步后 meanY -0.2153、角点不动、与 CPU 差 0。
- 边界：只有结构距离约束（无剪切/弯曲/碰撞）；v1 每步 fence 等待、不跨帧流水化；
  网格拓扑在 Create 时固定（改尺寸必须重新 Create）。

### P9-b SSBO 调色板蒙皮（2026-10-29，Lead 直接实现）

- API：`GN_VkDraw3DSkinnedSsbo(...)`（顶点仍是 `GN3d::sSKINNED_VERTEX` 68B）。
- 实现：`_Vulkan/vkSkinSsbo.cpp`：set0 = 场景 UBO + `mat4 palette[]` SSBO（capacity 4096，
  着色器索引钳到 4095）；3 个环形调色板槽位（帧号 + 本帧绘制计数轮转），每槽一个描述符集；
  管线经 `aPipelineCache`；`init3DSkinnedSsbo/destroySkinnedSsbo`。
- 着色器：`gn3d_skinned_ssbo.vert/.frag`（与 P2 UBO 路径同一套 LBS / Lambert）。
- 验收：`gpuCheck` 4 条（`skin.ssbo.pipeline/joint200/paletteRing/invalid`）——200 关节时
  关节 150 平移生效（像素 `0.572,0.254,0.127`），同帧两块不同调色板各自生效。
- 边界：旧 128 关节 UBO 路径原样保留（`GN_VkDraw3DSkinned` 未改）；无 GPU 动画/morph；
  上限 4096 关节（超出返回 false）。

### P9-c 绘制项批处理（2026-10-29，Lead 直接实现）

- 契约（`src/graphics/GN_3Dbatch.h`）：`sBATCH_DRAW_ITEM{model, pipelineKey, materialKey, depth, order}`；
  `sortBatchItems` 稳定排序（pipelineKey↑ → materialKey↑ → depth↑ → order↑）；
  `countBatchSwitches` 统计相邻项管线/材质键变化次数。纯 CPU，无 Vulkan 依赖，
  **不重构既有绘制路径**（只是给调用方的排序工具）。
- 实现/验收：`GN_3Dbatch.cpp` + `testBatch` 5 用例 + `gpuCheck` 3 条
  （`batch.sort/batch.switches/batch.draw`：乱序 3 次切换 → 排序后 1 次；排序前后两帧采样颜色一致）。
- 边界：只排序、不合并几何/不做实例化；调用方负责按排序结果提交。

## 4. 写域（互不重叠；越界只报告不处理）

| 执行体 | 只写这些 |
|---|---|
| p1c-gpu-material | `src/graphics/_Vulkan/vkMaterial.cpp`、`assets/shaders/gn3d_material.vert|frag`（+`.spv`）、`tests/gpu/gpuCheckMaterial.cpp/.h` |
| p2-gpu-skin | `src/graphics/_Vulkan/vkSkin.cpp`、`src/graphics/_Vulkan/vkSkinDraw.cpp`、`assets/shaders/gn3d_skinned.vert|frag`（+`.spv`）、`tests/gpu/gpuCheckSkin.cpp/.h` |
| p8-cull-debug | `src/graphics/_Vulkan/vkDebugDraw.cpp`、`src/graphics/GN_3Dcull.cpp`、`src/graphics/GN_3Dmodel.h`、`assets/shaders/gn3d_debug.*`（+`.spv`）、`tests/unit/testCull.cpp`、`tests/gpu/gpuCheckCull.cpp/.h` |
| Lead（P9） | `src/graphics/GN_3DparticleGPU.h/.cpp`、`_Vulkan/vkParticleCompute.cpp`、`_Vulkan/vkParticleDraw.cpp`、`assets/shaders/gn3d_particle.*`（+`.spv`）、`tests/unit/testParticleGPU.cpp`、`tests/gpu/gpuCheckParticle.cpp/.h`，以及共享接线（`GN_Vk.h`/`vkInternal.h`/`GN_Vk.cpp`/`vk3dShutdown.cpp`/`gpuCheckMain.cpp`/`Makefile`/`docs`） |

其余路径一律只读。需要改共享文件/签名 ⇒ 停下报告，由 Lead 改。

## 5. 交付格式

每片结束时报告：改了哪些文件（路径 + 关键行）、`make -j8` / `make unit` / `make gpuCheck` 的实际输出
（贴 `GPU CHECK SUMMARY` 与新增 PASS 行）、lint 结果、遗留风险。**不要**自称"已验证可用"——
可用性由 Lead 独立复核。新增/修改文件先跑：

```bash
bash ~/.dsh/skills/code-modularity/scripts/lintCodeModularity.sh <你的写域> --naming --comments
```

确认退出码 0（存量告警不计，但新增文件不得有命名/模块头违规）。
