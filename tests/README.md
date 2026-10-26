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

## Headless / GPU 冒烟（未实现，环境阻塞）

- 需要可用的 Vulkan 物理设备。当前机器在 DSH 沙箱内没有 `/dev/dri`，
  且未安装 lavapipe（`/usr/share/vulkan/icd.d` 只有 `radeon_icd.json`）。
- 计划：新增 `GN_StartRunHeadless`（`VK_EXT_headless_surface` 或离屏渲染），
  或在有 `mesa-vulkan-drivers`（lvp）的环境用 Xvfb 跑 16 个示例各 N 帧。
- 在此之前，`make check` 不含渲染冒烟；运行示例仍需要真实桌面环境。
