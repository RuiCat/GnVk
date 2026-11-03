# 引擎 GI 的硬件光追（Ray Query）落地规划

> 状态：规划（2026-11-03）。前置条件已具备：引擎实例升 Vulkan 1.1，设备在支持时已启用
> `VK_KHR_acceleration_structure` + `VK_KHR_ray_query`（`GN_VkRayQuerySupported()` 实测 on），
> `tests/cornellBox` 已验证 ray query 管线与对拍（MAE 0.015）。

## 1. 目标

把引擎的屏幕空间 GI（当前 `gn3d_ssgi.frag` 的屏幕空间 raymarch）升级为**基于场景 BVH 的 ray query**：

- 次级射线（漫反射 GI / 反射）走 `rayQueryEXT` 打 TLAS，不再受屏幕边界限制（当前屏幕外回退 IBL）；
- 命中后直接取**真实几何**的 albedo/法线/材质，不再依赖 G-Buffer 反投影；
- 保留屏幕空间路径作为**回退**（设备不支持 ray query / AS 未就绪时）。

## 2. 架构

```
场景三角形采集（draw 时累积）
  → BLAS（三角形，world space）+ 单实例 TLAS
  → SSGI fragment shader: binding 6 = TLAS
      rayQueryEXT(origin=G-Buffer 重建世界位置, dir=半球/反射方向)
      命中 → shadeDirect(命中点) + SH9/环境；miss → sky/IBL
  → 输出半分辨率 SSGI → 去噪 → 延迟合成（现有链路不变）
```

### 2.1 场景 AS 生命周期

| 场景类型 | 策略 |
|---|---|
| 静态（Cornell / mmdViewer 静止模型） | 首帧构建 BLAS/TLAS，场景变更（模型增删/变换）重建 |
| 动态（粒子/布/动画） | 不进入 GI AS（用屏幕空间回退或仅静态代理）；v1 只建静态场景 |
| resize | 不受影响（AS 与分辨率无关） |

### 2.2 冻结契约（v1）

| 项 | 契约 |
|---|---|
| SSGI set0 binding 6 | `accelerationStructureEXT`（TLAS），仅 fragment 阶段 |
| 场景三角形 | world-space，`float[3]` 位置 + `uint32` 索引；每三角形材质 id → 单独 SSBO（binding 7） |
| 法线 | 三角形顶点法线 SSBO（binding 8），命中后重心插值（与 cornellBox 的 HW RT 同法） |
| UBO | 复用 GIUBO（binding 4）；新增字段只在尾部追加，不改现有偏移 |
| 回退 | `GN_VkRayQuerySupported()==false` 或 AS 未构建 ⇒ 走现有屏幕空间 shader（编译期/运行期二选一，v1 运行期） |

### 2.3 引擎 API（新增，`src/graphics/_Vulkan/vkRayQuery.cpp`）

```cpp
bool GN_VkRayQuerySupported();                       // 已有
bool GN_VkRayQueryBeginScene();                      // 开始收集本场景三角形
void GN_VkRayQueryAddMesh(const void* verts, uint32_t vCount, const uint32_t* idx,
                          uint32_t iCount, uint32_t vertexStride); // world-space
bool GN_VkRayQueryEndScene();                        // 建 BLAS/TLAS（异步 fence，下一帧可用）
VkAccelerationStructureKHR GN_VkRayQuerySceneTlas();
bool GN_VkRayQuerySceneReady();
void GN_VkRayQueryDestroyScene();
```

## 3. 实施顺序（每步可独立验收）

| 步 | 内容 | 验收 |
|---|---|---|
| S1 | `vkRayQuery.cpp`：收集 + BLAS/TLAS 构建（复用 cornellBox 已验证逻辑）；设备函数指针 + fence | 用 Cornell 场景建 AS 成功；`GN_VkRayQuerySceneReady()`=true |
| S2 | SSGI 描述符加 binding 6/7/8（TLAS/材质/法线），未就绪时不绑定、走回退 | 引擎不崩；截图与现状一致（回退路径） |
| S3 | `gn3d_ssgi.frag` 加 ray query 分支（`#extension GL_EXT_ray_query`，GLSL 460 + vulkan1.2） | 开启 RT SSGI：命中真实墙面；Cornell vs PT RMSE 下降 |
| S4 | 关闭 RT 时逐像素与旧 SSGI 一致（回退正确性） | A/B 截图 RMSE≈0（关闭分支） |
| S5 | `gpuCheck` 加 ray-query SSGI 用例（命中/未命中/回退） | `make gpuCheck` 全 PASS |

## 4. 验收与风险

- **数值**：Cornell `engine(RT SSGI) vs PT` 的 RMSE/MAE 必须 ≤ 当前屏幕空间版（当前 20.68/17.94）；否则回退。
- **性能**：1280×800 下延迟增量 ≤ 3 ms（RADV 780M）；超了降半分辨率或减少射线数。
- **风险**：① BLAS 每帧重建开销（静态缓存规避）；② 动态物体不参与（v1 明确）；③ 无验证层，布局/描述符错误只能靠 A/B 截图与崩溃暴露；④ shader 需 GLSL 460 + `--target-env vulkan1.2`（Makefile 已支持该特例）。

## 5. 不在本轮

- DDGI 探针的 RT 更新（S3 之后）、SSR 的 RT 化、硬件 ray tracing pipeline（SBT）—— 均复用本轮的 TLAS/材质契约，后续增量做。
