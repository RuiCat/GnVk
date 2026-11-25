# GnVk — GN_mini Vulkan 游戏引擎

轻量级跨平台 2D/3D 游戏引擎，Vulkan 渲染后端。

## 特性

- **Vulkan 渲染** — 2D 批处理绘制 + 3D PBR/延迟光照管线
- **跨平台** — Linux (Vulkan/GLFW)，macOS/Windows/Android 可扩展
- **模块化** — 图形、音频、输入、UI、网络、文件系统独立模块
- **FFmpeg 视频播放** — 流式解码，支持 MKV/MP4/H264
- **FreeType 字体** — 实时 TTF/OTF 渲染，替代预生成字库
- **延迟 PBR 光照** — Cook-Torrance BRDF + GGX + IBL + 阴影映射

## 依赖

| 库 | 用途 |
|----|------|
| Vulkan + GLFW | 图形渲染 |
| libpng | PNG 纹理加载 / glTF PNG 纹理解码 |
| libjpeg（可选，pkg-config） | glTF JPEG 纹理解码；缺失时只支持 PNG |
| FreeType | TTF 字体渲染 |
| OpenAL | 音频播放 |
| FFmpeg (libavcodec/format/swscale) | 视频播放 |
| zlib, X11, ALSA | 系统支持 |

## 项目规模（自动生成）

<!-- AUTO-STATS:BEGIN -->
<!-- 本段由 tests/tools/gen_stats.sh 生成，不要手改；改数字请跑 make stats -->

| 项 | 数值 |
|----|------|
| 单元测试用例 | **815**（79 个文件）|
| gpuCheck 文件 | 18 |
| gpuCheck 实跑 | `pass=126 fail=0 skip=0` |
| 单测实跑 | `815 cases, 0 failed, 0 assertions failed` |
| src 源文件 | 182 个 .cpp |
| src 总行数 | 105013 |
| 编译产物着色器 | 47 个 .spv |

<!-- AUTO-STATS:END -->

## 文档

完整索引见 [`docs/README.md`](docs/README.md) —— 每份文档开头都标了状态（✅ 现行 / 📋 已完成 / 🚧 进行中 / 📦 历史 / 📊 快照），
由 `make doc-check` 双向校验，不会出现「文档说未实现、代码已实现」。

| 想做什么 | 读这个 |
|---|---|
| 编译、跑第一个应用 | [`docs/GETTING_STARTED.md`](docs/GETTING_STARTED.md) |
| 查 API（含 3D 实体层导航） | [`docs/API_REFERENCE.md`](docs/API_REFERENCE.md) |
| 写代码前看注释约定 | [`docs/COMMENT_STYLE.md`](docs/COMMENT_STYLE.md) |
| 看还剩什么没做 | [`docs/REMAINING_GAPS.md`](docs/REMAINING_GAPS.md) |
| 查某个改动为什么这么写 | [`docs/CHANGELOG.md`](docs/CHANGELOG.md) |

## 编译

```bash
# Linux (自动检测)
make

# 调试版本
make debug

# 单元测试（纯逻辑，无需 GPU）
make unit

# ASan/UBSan 单元测试（独立构建，不覆盖 release 产物）
make asan

# 构建 + 单元测试 + 全部示例编译
make check

# 无窗口渲染验收（离屏，不需要 X11/Wayland）
# 含两条：2D 四象限像素断言 + 帧编排等价性（cRENDER_FRAME vs 裸 sFRAME_3D_SCOPE 逐字节比对）
make headlessCheck

# 示例无窗口冒烟（五个二进制各 8 帧；无 Vulkan 设备时 SKIP）
make smoke

# 一条命令全跑：check + smoke
make ci

# 交叉平台编译检查（MinGW-w64 把 src/ 全量按 Windows 目标编一遍；没工具链时 SKIP）
make crosscheck

# 着色器编译
make shaders

# 清理
make clean
```

## 平台与图形后端

`PLATFORM`（操作系统）与 `BACKEND`（图形后端）分开选择，`BACKEND` 缺省 `vulkan`：

| 平台 | 默认 | 说明 |
|------|------|------|
| Linux | `Vulkan + GLFW` | 已实测（RADV / lavapipe） |
| Windows | `Vulkan + GLFW` | MinGW-w64 交叉编译已验证能编；链接/运行需在 Windows 上做 |
| macOS | `Vulkan + GLFW` | 需自行安装 MoltenVK；**未实测** |
| Android | `GLES` | 无 Vulkan 后端 |

```bash
make PLATFORM=windows          # Vulkan 后端（默认）
make PLATFORM=windows BACKEND=opengl   # 旧的 GLUT/OpenGL 路径（需自行提供窗口层）
```

Vulkan 后端是唯一带**无窗口（离屏）渲染**能力的后端，且不依赖任何窗口系统
（`make headlessCheck` / `make smoke` / `GN_HEADLESS_FRAMES=N`）。

## 目录结构

```
GnVk/
├── assets/shaders/     # GLSL 着色器 + SPIR-V 编译产物
├── src/
│   ├── core/           # 引擎核心 (初始化/帧循环/内存/字符串)
│   ├── graphics/       # 2D/3D 图形 (Vulkan 后端)
│   │   └── _Vulkan/    # Vulkan 实现
│   ├── audio/          # 音频系统 (OpenAL)
│   ├── input/          # 输入处理
│   ├── ui/             # UI 组件
│   ├── network/        # 网络模块
│   ├── file/           # 文件系统
│   ├── media/          # 视频播放 (FFmpeg)
│   ├── platform/       # 平台抽象层
│   └── plugin/         # 插件接口
├── external/include/   # 第三方头文件
├── docs/               # 文档
├── Makefile            # 构建系统
└── libGN_mini.so       # 编译产物 (动态库)
```

## 运行

```bash
# 设置库路径
export LD_LIBRARY_PATH=.:$LD_LIBRARY_PATH

# 运行示例 (需在 AppInit/AppDisplay2D 等回调中实现逻辑)
./your_app
```

## 应用入口

引擎通过回调函数驱动应用：

```cpp
#include <GN.h>

extern "C" {
GN_CALL void AppInit()           { /* 初始化 */ }
GN_CALL void AppUnInit()         { /* 清理 */ }
GN_CALL void AppDisplay2D(uint nTime) { /* 2D 渲染 */ }
GN_CALL void AppDisplay3D(uint nTime) { /* 3D 渲染 */ }
GN_CALL void AppRun(uint nTime)  { /* 逻辑更新 */ }
}

GN_MAIN;
GN_InitLoadCallBackFunc(AppInit, AppUnInit);
GN_InitLoopCallBackFunc(AppDisplay2D, AppDisplay3D, AppRun);
GN_StartRun("My App", 800, 600, 0, false, true, true, 0,0,0,0);
GN_EXIT;
```

## Vulkan 渲染管线

| 管程 | 用途 |
|------|------|
| 2D 批处理 | GUI / 精灵 / 文本 (单 pass, 动态 scissor) |
| 3D Forward PBR | Cook-Torrance + GGX + IBL |
| 延迟光照 | G-Buffer → 逐光源 PBR + PCF 阴影 |
| 阴影映射 | Poisson PCF 软阴影 |
| HDR + Composite | 色调映射 + Bloom |

## 关键技术细节

- **延迟纹理上传**: 渲染 pass 内不提交 GPU 工作，延迟到帧首 `GN_VkBeginFrame` 处理，避免 RADV GPUVM fault
- **PreFrame 回调**: `GF_FrameCycleProc1` 中调用，在渲染 pass 前安全初始化 GPU 资源
- **顶点批处理**: 2D 绘制按 descriptor set 分组，单次提交

## 许可证

MIT License — 详见源文件头部声明
