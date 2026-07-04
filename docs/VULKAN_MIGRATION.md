# GN_mini Vulkan 后端迁移报告

> 日期: 2026-06-25 (最终更新) | 状态: ✅ 完整实现
> 原始引擎: OpenGL+GLUT → 目标: Vulkan+GLFW

---

## 一、架构变更

```
原始:  samples → GN引擎 → _Graphics (OpenGL) → GLUT → OS
现在:  samples → GN引擎 → _Vulkan (GN_Vk.cpp) → GLFW → OS
                              └── SPIR-V着色器 (glslangValidator编译)
```

## 二、3D 渲染功能总览

### 2.1 着色器管线 (8 个着色器程序)

| 着色器 | 类型 | 行数 | 功能 |
|--------|------|:---:|------|
| `gn2d.vert/frag` | 2D | 30 | 正交投影 + 纹理×顶点色 + Alpha混合 |
| `gn3d.vert/frag` | Blinn-Phong | 135 | 纹理采样 + 镜面高光 + 8光源(方向/点/聚光) + 衰减 |
| `gn3d_pbr.vert/frag` | PBR | 323 | Cook-Torrance GGX + Fresnel-Schlick + Smith遮蔽 + 法线贴图(TBN) + Poisson 16点PCF阴影 + Skybox IBL |
| `gn3d_shadow.vert` | Shadow | 19 | 深度Only (2048² D32, depthBias 1.25/1.75) |
| `gn3d_gbuffer.vert/frag` | G-Buffer | 80 | MRT 3附件 (Normal+Roughness, Albedo+Metallic, AO) |
| `gn3d_deferred_light.vert/frag` | Deferred | 260 | 全屏三角形延迟光照 (逐光源PBR + Skybox IBL + Reinhard) |
| `gn3d_composite.vert/frag` | PostFX | 43 | SimpleBloom + Reinhard 色调映射 |
| `gn3d_skybox.vert/frag` | Skybox | 50 | 单位立方体 + Cubemap采样 + depth LEQUAL |
| `gn3d_ssao.frag` | SSAO | 52 | 屏幕空间环境光遮蔽 (预留集成) |

### 2.2 RenderPass 与 Pipeline (7+12)

```
RenderPass (7个): 2D | 3D-Forward | HDR | Shadow | Composite | G-Buffer | DeferredLight
Pipeline (12个):  gn2d×4 | gn3d | gn3d_pbr | shadow | composite | skybox | gBuffer | deferredLight | ssao(预留)
```

### 2.3 核心渲染特性

| 特性 | 状态 |
|------|:---:|
| **MSAA 4× 抗锯齿** | ✅ 硬件自适应 (4→2→1回退), 3D Pass自动启用 |
| **HDR 线性工作流** | ✅ swapchain SRGB自动编码, 移除手动Gamma |
| **延迟渲染** | ✅ 3附件 R16G16B16A16 G-Buffer, 深度重建世界位置 |
| **Poisson PCF 软阴影** | ✅ 16点圆盘采样 + 随机旋转, 2048² shadow map |
| **Skybox IBL** | ✅ 程序化256²×6面 Cubemap, 半球采样环境光 |
| **包裹光照** | ✅ (N·L+0.3)/1.3 柔化明暗交界 |
| **Bloom 泛光** | ✅ 邻域亮度提取 + 叠加 + Reinhard |
| **Push Constant 类型安全** | ✅ VkModelPushConstBP / VkModelPushConstPBR |
| **法线贴图** | ✅ TBN矩阵 + Gram-Schmidt正交化 |
| **纹理逐模型绑定** | ✅ vkUpdateDescriptors per-draw |
| **VkCheck 错误报告** | ✅ 20种Vulkan错误 → stderr |
| **SPIR-V 双重回退** | ✅ 外部.spv → 嵌入式C数组 |
| **深度格式回退** | ✅ D32 → D24_S8 → D16 |

### 2.4 光照模型

| 模型 | 镜面 | 阴影 | IBL | 色调映射 |
|------|:---:|:---:|:---:|:---:|
| Blinn-Phong (Forward) | Blinn-Phong N·H | ✗ | ✗ | ✗ |
| PBR (Forward) | Cook-Torrance GGX | Poisson 16点 | Skybox Cubemap | Reinhard |
| PBR (Deferred) | Blinn-Phong | Poisson 16点 | Skybox Cubemap + Analytic | Reinhard |

### 2.5 光源系统

- **方向光**: 平行光 + 2D shadow map
- **点光源**: 二次衰减 (c + l·d + q·d²), 无阴影 (cube map预留)
- **聚光灯**: 内外锥角软化 + 2D shadow map
- **shadowType** (direction.w): 0=无阴影, 1=2D, 2=Cube(预留)
- **最大光源**: 8 (UBO 752B)
- **预置**: 三点光照 / 户外光照

## 三、2D 渲染功能总览

| 功能 | 状态 | 说明 |
|------|:--:|------|
| ImageDraw / MP / Rot / 4P | ✅ | 完整 |
| ImageDraw9Square / MaskRect | ✅ | 完整 |
| LineFrame2dDraw | ✅ | 完整 |
| 多帧图集 (ImageLoad num>1) | ✅ | 图集打包 + 网格 UV 映射 |
| Scissor 裁剪 | ✅ | VK_DYNAMIC_STATE_SCISSOR |
| RenderTarget 离屏渲染 | ✅ | RenderPass 切换 |
| GPU→CPU 回读 (DataGet) | ✅ | Staging buffer |
| 截图保存 (SaveToFile) | ✅ | GN_VkReadBack |
| 混合模式 Normal/Bright/Multiply/Null | ✅ | 4/15 |
| 键盘输入 | ✅ | GLFW→GN 映射 |
| FreeType 实时字形渲染 | ✅ | 替代 GB2312 |

## 四、修改文件清单

| 文件 | 变更 |
|------|------|
| `src/graphics/_Vulkan/GN_Vk.h` | +MSAA字段 +HDR字段 +Composite字段 +Skybox字段 +G-Buffer字段 +Deferred字段 +Push类型安全 +纹理API |
| `src/graphics/_Vulkan/GN_Vk.cpp` | +VkCheck +Ensure3DBufferCapacity +MSAA创建 +HDR/Composite管线 +天空盒Cubemap +G-Buffer管线 +Deferred管线 +纹理DS更新 |
| `src/graphics/GN_3Dlight.cpp` | +shadowType (direction.w) |
| `src/graphics/GN_3Dmaterial.h/cpp` | +aAlbedoTex +aNormalTex 纹理引用 |
| `src/graphics/GN_3Dmodel.h/cpp` | +aAlbedoTex +aNormalTex +纹理传递 |
| `assets/shaders/gn3d.frag` | +纹理采样 +镜面高光 +环境光修正 |
| `assets/shaders/gn3d.vert` | +世界空间法线变换 |
| `assets/shaders/gn3d_pbr.frag` | +Poisson PCF +IBL split-sum +HDR工作流 |
| `assets/shaders/gn3d_gbuffer.*` | **新增** G-Buffer MRT着色器 |
| `assets/shaders/gn3d_deferred_light.*` | **新增** 延迟光照着色器 |
| `assets/shaders/gn3d_composite.*` | **新增** Bloom合成着色器 |
| `assets/shaders/gn3d_skybox.*` | **新增** 天空盒着色器 |
| `assets/shaders/gn3d_ssao.frag` | **新增** SSAO着色器 (预留) |
| `samples/base/Sample24.cpp` | 增强: 阴影+延迟渲染+天空盒+多光源 |
| `docs/ENGINE_ARCHITECTURE.md` | 更新至 2026.06.25 |
| `docs/VULKAN_MIGRATION.md` | 更新至 2026.06.25 |

## 五、测试状态 (2026-06-25)

| 测试项 | 状态 | 说明 |
|--------|:--:|------|
| libGN_mini.so 编译 | ✅ | 1.9MB, 零错误 |
| Sample24 编译 | ✅ | 60KB, 零错误 |
| Sample24 运行 | ✅ | 窗口800×600, 无Vulkan错误 |
| Forward PBR + Shadow + HDR + Bloom | ✅ | 完整管线 |
| Deferred PBR + Shadow + Skybox IBL | ✅ | 3附件G-Buffer |
| Wrap lighting | ✅ | 柔化明暗交界 |
| Poisson PCF 16点软阴影 | ✅ | 2048² |
| 天空盒 (程序化渐变) | ✅ | 256²×6面 Cubemap |
| 多光源 (方向/点/聚光) | ✅ | 8光源上限 |
| 纹理逐模型绑定 | ✅ | albedo + normal |

---

*此文档基于 GN_mini 源码分析生成，版本 2026.06.25*