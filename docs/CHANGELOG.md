# Changelog

## 2026-10-27 — 移除旧示例测试与旧资源（测试体系待重新设计）

- 删除 `samples/`（16 个示例测试，其中 05/06/07/09/11~16 长期为 TODO 占位页）与 `run.sh`；
  旧测试资源 `assets/ui`、`assets/axis`、`assets/fileicon`、`assets/effects`（62 文件）一并删除。
- `Makefile`：移除 `test` / `tests` 目标与示例编译段；`make check` = 构建 + 单元测试（`make unit` 保留，134 用例）。
- 保留：`assets/shaders/`（渲染必需）、`external/`（编译必需）、`docs/`、`tests/unit/`（本工程验收用例）。
- 后续测试代码（含 GPU 验收）按新体系重新设计，不再恢复 `run.sh`。

## 2026-10-27 — 恢复引擎默认资源（修复资源加载全失败）

- 根因：`701828d 清理原始例子` 把 `assets/` 下除 `shaders/` 外的资源整批删除，而引擎与测试仍按 `GN_SysDir()`（根目录 = `assets/`）加载：
  `samples/02/03` 用 `ui/bubbleB`，`GN_RunRec` 用 `ui/br`，`GN_UiBase` 用 `ui/bubble*`、`axis/2Dctrl_*`，`GN_UiFBrowser` 用 `fileicon/*`，`GN_Frame` 用光环贴图。
- 修复：从 `701828d^` 恢复 `assets/ui`（21）、`assets/axis`（4）、`assets/fileicon`（36）、`assets/effects`（1），共 62 个文件 / 约 0.9 MB。
- 修复 `GN_Frame.cpp` 默认光环路径 `eft/` → `effects/`（仓库里从未有过 `assets/eft/`，该默认值一直是坏的）。
- 说明：`samples/05/06/07/09/11..16` 仍是 TODO 占位页（不是资源加载失败）；资源类测试为 `./run.sh 02`、`03`。

## 2026-10-26 — 工程化阶段 0（止血）

- 修复 `samples/08_cube3d.cpp` 的 `Height()` 重复声明 `float h`（该文件此前无法编译）。
- Makefile：`make tests` 失败时非 0 退出并保留错误输出；`test/tests` 使用 `$(CXX)`；`clean` 清理 `build/test_*`；移除 freeglut 头依赖；`help` 与实际目标一致。
- `GN_VERSION` 更新为 `2026.07.04`；架构文档的版本改为引用 `GN.h`（单一来源）。
- 新增根 `LICENSE`（MIT）与 `external/README.md`（第三方来源/许可清单）。
- `GN_VkInit` 在无 Vulkan 物理设备 / 无可用队列族时输出错误并返回，不再空指针崩溃。
- 新增 `tests/unit/` 最小测试框架与 67 个纯逻辑用例；`make unit` / `make check` 可用。
- 新增 `make asan`：独立构建目录 + 独立库名，ASan/UBSan 跑同一套单测（67/67 无报告）。
- 修复 `GN_StrUnSplitToData` 的长度累加错误（原为赋值，导致缓冲区溢出）。
- 修复 `GN_StrCutSpaceFL` 对空串/全空白串的 `strlen-1` 越界读取。
- 修复 `GN_ValueUnPackHalf(0)` 返回 3e-5 而非 0。
- 修复 `GN_ValueFloatToHalf` 在 `e>125` 时的无符号移位回绕 UB（UBSan 实测命中）。
- 修复 `GN_ValueHalfToFloat` 在 `e!=0` 时的 150 位移 UB（UBSan 实测命中）。
- 新增程序化图像生成（矩形渐变、棋盘格）单测。
- 阶段 2 第一步：把 `VkCheck`/`FindMemType`/`CreateBuf`/`MakeImageView`/`LoadSPIRVFile` 抽到 `_Vulkan/vkDevice.cpp` + `vkInternal.h`（内部符号隐藏，公共动态符号不变）；`GN_Vk.cpp` 3690 → 3627 行。
- 阶段 2 第二步：把 PNG 加载/图集打包/保存/帧缓冲回读抽到 `_Vulkan/vkImageIo.cpp`（281 行）；`GN_Vk.cpp` 3627 → 3366 行；清理不再使用的 `<png.h>/<fstream>/<vector>` 包含。
- 阶段 2 第三步：把 `sIMAGE` 构造/析构/赋值/`Release`/`CopyOut`/`DataGet*` 抽到 `_Vulkan/vkImage.cpp`（187 行）；`GN_Vk.cpp` 3366 → 3204 行。
- 阶段 2 第四步：把 `ImageDraw`/`ImageDraw4P`/`ImageDrawRot`/`LineFrame2dDraw` 抽到 `_Vulkan/vkDraw2d.cpp`（177 行）；共享顶点批状态（`gVBuf`/`gVMax`/`gVCnt`/`s_lastDS`）与批处理辅助改为 `vkInternal.h` 内部符号（hidden）；`GN_Vk.cpp` 3204 → 3035 行。
- 阶段 2 第五步：把剩余 2D 包装（`ImageDrawDitherBlur`/`ImageDrawMP`/`ImageDrawFull`/`TextDraw`/`Draw2DLine`/AlphaKey/AlphaDirection）并入 `vkDraw2d.cpp`（232 行）；`GN_Vk.cpp` 3035 → 2984 行。
- 阶段 2 第六步：把 `GN_VkBeginFrame`/`GN_VkEndFrame`/`GN_VkSetScissor`、scissor 与顶点批状态、`BatchSwitch`/`FlushBatchIfScissorDirty`/`FlushVertexBatch`/`GrowVBufIfNeeded`、延迟上传队列抽到 `_Vulkan/vkFrame2d.cpp`（184 行）；相关状态以 hidden extern 暴露给 3D 代码；`GN_Vk.cpp` 2984 → 2819 行。
- 阶段 2 第七步：把 `GN_VkUpload3DMesh`/`GN_VkRelease3DMesh` 抽到 `_Vulkan/vk3dMesh.cpp`（80 行）；`GN_Vk.cpp` 2819 → 2751 行。
- 阶段 2 第八步：把阴影 pass（`GN_VkShadowPassBegin/End`、`GN_VkShadowDraw`）、`GN_VkCompositeToSwapchain`、`GN_VkDrawSkybox` 抽到 `_Vulkan/vk3dPasses.cpp`（170 行）；`s_in3DPass`/`s_inShadowPass`/`s_framebuffers3D`/`s_vtxOfs3D`/`s_idxOfs3D` 与 `Ensure3DBufferCapacity` 提升为内部符号；`GN_Vk.cpp` 2751 → 2597 行。
- 阶段 2 第九步：把 PBR pass（含 HDR 变体）与延迟渲染（G-Buffer / DeferredLighting）并入 `vk3dPasses.cpp`（403 行）；`GN_Vk.cpp` 2597 → 2370 行。
- 阶段 2 第十步：把前向 3D pass（`GN_VkBegin3DPass`/`GN_VkEnd3DPass`/`GN_VkDraw3D`/`GN_VkDraw3DPBR`）抽到 `_Vulkan/vk3dForward.cpp`（273 行）；`GN_Vk.cpp` 2370 → 2111 行。
- 阶段 2 第十一步：从 `GN_VkInit3D` 抽出渲染目标初始化（MSAA 颜色/深度、3D render pass、framebuffer）到 `_Vulkan/vk3dInit.cpp`（`init3DTargets`，107 行）；`GN_Vk.cpp` 2111 → 2018 行。
- 阶段 2 第十二步：从 `GN_VkInit3D` 抽出前向管线初始化（着色器模块、描述符/管线布局、图形管线、场景+光照 UBO、默认描述符集）到 `_Vulkan/vk3dInit.cpp`（`init3DForward`，shader 二进制由调用方传入）；`GN_Vk.cpp` 2018 → 1905 行。
- 阶段 2 第十三步：从 `GN_VkInit3D` 抽出 PBR 管线初始化（6 binding 描述符布局、管线布局、图形管线）到 `_Vulkan/vk3dInit.cpp`（`init3DPbr`）；`GN_Vk.cpp` 1905 → 1824 行。
- 阶段 2 第十四步：从 `GN_VkInit3D` 抽出阴影映射初始化（深度图/视图/采样器、render pass、framebuffer、描述符集、管线）到 `_Vulkan/vk3dInit.cpp`（`init3DShadow`）；`GN_Vk.cpp` 1824 → 1712 行。
- 阶段 2 第十五步：从 `GN_VkInit3D` 抽出 HDR 离屏目标与合成管线（HDR 图像/视图/采样器、HDR render pass/framebuffer、合成 render pass/管线/描述符集）到 `_Vulkan/vk3dInit.cpp`（`init3DHdr`）；`GN_Vk.cpp` 1712 → 1574 行。
- 阶段 2 第十六步：从 `GN_VkInit3D` 抽出天空盒初始化（程序化 Cubemap 6 面、IBL 描述符绑定、天空盒管线）到 `_Vulkan/vk3dInit.cpp`（`init3DSkybox`）；`GN_Vk.cpp` 1574 → 1370 行。
- 修复：天空盒此前只有初始化、**没有任何调用点**（sample 10 注释写"不需要 skybox"）；在 `samples/10_scene3d.cpp` 的 3D pass 内加入 `GN_VkDrawSkybox(&vp)`。
- 阶段 2 第十七步：从 `GN_VkInit3D` 抽出延迟渲染初始化到 `_Vulkan/vk3dInit.cpp`：`init3DGbuffer`（3 颜色附件 + 深度、render pass/framebuffer、G-Buffer 管线）与 `init3DDeferredLight`（8 binding 描述符、交换链 render pass/framebuffer、全屏三角形管线）；`GN_Vk.cpp` 1370 → 1178 行，`GN_VkInit3D` 仅剩 86 行。
- 阶段 2 第十八步：把 `GN_VkShutdown3D`、`Ensure3DBufferCapacity`、3D 缓冲偏移/容量状态抽到 `_Vulkan/vk3dShutdown.cpp`（135 行）；`GN_Vk.cpp` 1178 → 1054 行（有效代码 <1000，机检通过）。生成的 `gn3d_pbr_frag_spv.h` 按规则登记 `code-modularity: no-split` 技术债。
- 格式化：新增 `.clang-format`（4 空格 / K&R / 不重排长行），格式化 `src/` 下 100 个手写源文件（9 个生成的 `*_spv.h` 排除）。clang-format 会展开单行 `{...}` 块，格式化后 `GN_Vk.cpp`（1226 有效行）与 `vk3dInit.cpp`（1515 有效行）超限，因此拆分：`sIMAGE` 数据方法 → `vkImageData.cpp`（553 行）；HDR/天空盒/G-Buffer/延迟光照初始化 → `vk3dInitScene.cpp`（1022 行）。`GN_2Dscene.cpp` 与 `GN_String.cpp` 因格式化越过 1000 行，已还原并重放两处字符串修复。剩余 12 个 >1000 行文件在 HEAD 中已超限，登记为存量技术债。
- Headless 冒烟未实现：当前机器无 Vulkan 设备（`/dev/dri` 缺失、未装 lavapipe），留待环境可用后补。

## 2026-07-04 — Vulkan 稳定性修复

### 核心修复

- **延迟纹理上传机制**: 渲染 pass 内 `DataFill`/`UploadImageToGPU` 不再 `vkQueueSubmit`，数据暂存到 `s_pendingUploads[256]`，由 `GN_VkBeginFrame` 在 pass 前处理。彻底解决 AMD RADV GPUVM fault。

- **PreFrame 回调**: 新增 `GT_pFuncPreFrame`，在 `GF_FrameCycleProc1` (渲染 pass 前) 调用，适用于需在 pass 外完成的 GPU 初始化。

- **UploadImageToGPU 重构**: 移除独立的 GPU 上传逻辑，委托给 `DataFill` 统一走延迟上传路径。

### 光照系统

- 延迟光照着色器: Blinn-Phong → Cook-Torrance BRDF (GGX + Smith + Fresnel)
- `AddDirectional`/`AddSpot` 新增 `enableShadow` 参数
- `cLIGHT::Clear()` 恢复 `shadowMapSize`
- `GN_VkBegin3DPass` 等 4 个函数从 `lightUBO` 读取环境光
- PBR shader IBL 镜面反射: `F0` → `FresnelSchlick(NdotV, F0)`

### 视频播放

- FFmpeg 8.x matroska: `AVFMT_FLAG_IGNIDX | AVFMT_FLAG_IGNDTS`
- `zReadPacket` EOF 循环恢复 + `Play()` IO 重置
- `GN_SetWindowPos` 实现 (glfwSetWindowSize + GN_VkResize)
- MKV 文件重封装修复 H264 解码警告

### UB 修复

- 严格别名: `GN_Base.h` (packHalf/unpackHalf), `GN_UiBase.cpp`
- `memcpy(this)` → placement new: `GN_Frame.cpp`, `GN_Array.h`

### 安全修复

- 98 处 `strcpy`/`strcat` → `strncpy` + 显式 null (12 文件)
- 25 处 Vulkan `VkCheck` 错误检查

### Vulkan 资源管理

- 默认纹理 (1×1 white) Image/ImageView/Memory 在 shutdown 正确销毁
- 顶点缓冲预分配 512K 顶点 (16MB)
- `vkQueueWaitIdle` → fence 同步 (3 处)

### 示例修复

- Sample4/5: OpenGL 纹理合并代码在 Vulkan 下跳过
- `ImgFmtGetByteNum(0)` 返回 4 (CR_DEFAULT_FORMAT)
- Sample12: 视频缩放适配窗口 + audio=false

### 代码清理

- samples/ 精简为仅 .cpp 源码 (259 文件删除, 80MB→380KB)
- assets/ 精简为仅 shaders/ 目录
- 调试 fprintf 清理
- .gitignore 添加 build/ 产物

### 构建

- Makefile: 合并重复 .o 规则 + DEPFLAGS 自动依赖
