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
- 覆盖：11 条管线创建 + 缓存计数（11 miss / 0 hit）、2D 四混合像素数学、
  3D 前向/PBR/阴影/G-Buffer+延迟/合成/天空盒，以及 `GN_VkShutdown` 的缓存释放。
- 真实 GPU 桌面验收：在能看到 `/dev/dri` 的桌面终端执行 `make gpuCheck`；
  退出码 `0` = 全通过，`1` = 有 FAIL，`2` = 无 Vulkan 设备（SKIP，不算通过）。
- DSH 沙箱内没有 `/dev/dri`；可用 flatpak runtime 自带的 lavapipe 做**软件预验**：
  `VK_ICD_FILENAMES=.local/probes/lvp_icd.json make gpuCheck`。
  软件预验只证明验收程序本身可运行，**不能替代真实 GPU 桌面验收**。
- 细节、期望值与已知引擎问题见 `tests/gpu/README.md`。

## Headless 冒烟（未实现，环境阻塞）

- 需要可用的 Vulkan 物理设备；`GN_StartRunHeadless`（`VK_EXT_headless_surface` 或离屏渲染）尚未实现。
- `make check` 不含渲染冒烟；运行示例仍需要真实桌面环境。
