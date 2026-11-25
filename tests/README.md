# tests/ — 测试资产

## 单元测试（无需 GPU / 窗口）

- 位置：`tests/unit/`，最小框架见 `testFramework.h`，入口 `testMain.cpp`。
- 运行：`make unit`（编译并执行）；`make asan`（ASan/UBSan 构建并执行，独立构建目录与库名）；
  `make check`（构建 + 单测 + 16 个示例编译）。
- 覆盖：容器（`sLIST` / `sARRAY_VEC` / `cTREE2` / `cMEM_POOL1`）、字符串、2D/3D 数学、颜色与像素格式、程序化图像生成。
- 当前：67 个用例，0 失败。

### 新增用例

1. 在 `tests/unit/` 新建 `testXxx.cpp`，包含 `testFramework.h`；
2. 用 `TEST_CASE(小驼峰名) { CHECK(...); }` 声明，注册自动完成；
3. 用例不得依赖 GPU、窗口、网络或外部文件；必须可重复、无随机；
4. 断言优先用 `CHECK_EQ` / `CHECK_NEAR` / `CHECK_STREQ`，失败会打印文件行号。

## GPU 管线验收（P1b，需要 Vulkan 设备）

- 位置：`tests/gpu/`，入口 `make gpuCheck`（构建 `build/gpuCheck` 并运行）。
- 覆盖：管线缓存计数（实测 35 条，断言 ≥11）、2D 四混合像素数学、
  3D 前向/PBR/阴影/G-Buffer+延迟/合成/天空盒，以及 `GN_VkShutdown` 的缓存释放；
  另含**光照接入**（`gpuCheckLight.cpp`：改光源类型/强度/距离/环境光 → 像素跟着变，7 条断言）。
- 真实 GPU 桌面验收：在能看到 `/dev/dri` 的桌面终端执行 `make gpuCheck`；
  退出码 `0` = 全通过，`1` = 有 FAIL，`2` = 无 Vulkan 设备（SKIP，不算通过）。
- DSH 沙箱内没有 `/dev/dri`；可用 flatpak runtime 自带的 lavapipe 做**软件预验**：
  `VK_ICD_FILENAMES=.local/probes/lvp_icd.json make gpuCheck`。
  软件预验只证明验收程序本身可运行，**不能替代真实 GPU 桌面验收**。
- 细节、期望值与已知引擎问题见 `tests/gpu/README.md`。

## Headless 无窗口渲染（已实现，无需窗口系统）

- 引擎入口：`GN_StartRunHeadless(w, h, frames, frame_proc)`（`src/core/GN_Init.h`）。
  不创建 GLFW 窗口、不建 `VkSurface`/`VkSwapchain`，渲染到引擎自建的离屏图；
  在 `frame_proc` 里用 `GN_GrabFrame` 取 RGBA8 像素（`GN_GrabFrameFree` 释放）。
- **通用开关**：`GN_HEADLESS_FRAMES=N`。设了它，所有用 `GN_MAIN`/`GN_EXIT` 的程序都会跳过
  `glfwInit` 并在无窗口下跑 N 帧后退出；现有示例只需在 `GN_StartRun(...)` 前加一行
  `if(GN_TryRunHeadlessFromEnv(w, h)) return;`。
- 验收：`make headlessCheck`（`tests/headless/`）。画四个色块→回读→断言象限颜色、
  清屏 alpha 与非全黑。退出码 `0` 通过、`1` 有失败、`2` 无 Vulkan 设备（SKIP）。
- 示例冒烟：`make smoke`（`tests/smoke/runSamples.sh`）—— headlessCheck / cornellBox /
  mmdViewer / imoScene / gpuCheck 各跑 8 帧（`SMOKE_FRAMES=N` 可调）。缺模型资源的示例
  只画背景，判据是「跑完不崩」。
- 一条命令：`make ci` = `check`（构建 + 单测）+ `smoke`。
- `make gpuCheck` 也支持无窗口：`GN_HEADLESS=1` 或 `GN_HEADLESS_FRAMES>0` 时跑同一批 109 项检查。
- `tests/cornellBox` 支持 `CORNELL_HEADLESS=1`（或 `GN_HEADLESS_FRAMES`）：34 帧后把引擎画面写到
  `.local/cornellbox/headless-engine.png`（与 `CORNELL_CAPTURE_ENGINE=1` 的交换链截图逐像素一致）。
- 需要 Vulkan 设备；无 `/dev/dri` 的容器可配 lavapipe（见 `tests/gpu/README.md` 的
  `VK_ICD_FILENAMES`）。
- **平台范围**：纯 Vulkan 1.1（不启用 `VK_KHR_swapchain`），Linux / Windows / macOS
  共用同一份实现；GLFW 负责给出各平台 surface 扩展，引擎侧无分支。
  实测仅 Linux；Windows 已过 `make crosscheck` 的编译检查，macOS（MoltenVK）未实测。

### `cull.debugLine`（已修，2026-11-24）

- 曾长期失败（窗口与无窗口模式都一样）：测试把线段放在世界 `z = +0.5`，相机在原点看向 `-z`，
  `Mat44OrthoVK` 的 `z_ndc = -z_world`，`z_ndc = -0.5` 落在 near 平面外被裁剪，
  中心像素只剩前向 pass 的 clear 色（0.05,0.05,0.1）。改成 `z = -0.5` 后全过（当时 101/101，现 109/109）。

## 实测统计（自动生成）

<!-- AUTO-STATS:BEGIN -->
<!-- 本段由 tests/tools/gen_stats.sh 生成，不要手改；改数字请跑 make stats -->

| 项 | 数值 |
|----|------|
| 单元测试用例 | **815**（79 个文件）|
| gpuCheck 文件 | 18 |
| gpuCheck 实跑 | `pass=126 fail=0 skip=0` |
| 单测实跑 | `815 cases, 0 failed, 0 assertions failed` |
| src 源文件 | 182 个 .cpp |
| src 总行数 | 104902 |
| 编译产物着色器 | 47 个 .spv |

<!-- AUTO-STATS:END -->
