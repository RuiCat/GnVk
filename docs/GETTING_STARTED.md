# GN_mini 快速入门指南 (Linux)

> Vulkan 后端 | 最后更新: 2026-07-04

## 环境要求

- **Linux** (推荐 Ubuntu 22.04+)
- **GPU** 支持 Vulkan 1.0+ (含 RADV/AMDVLK/NVIDIA 驱动)
- **编译工具**: g++ 11+ / clang 14+, GNU Make
- **依赖库**: 见 [README.md](../README.md#依赖)

### 安装依赖 (Ubuntu/Debian)

```bash
sudo apt install build-essential libglfw3-dev libvulkan-dev \
  libpng-dev libfreetype-dev libopenal-dev libz-dev \
  libx11-dev libasound2-dev \
  libavcodec-dev libavformat-dev libswscale-dev libavutil-dev \
  glslang-tools
```

## 编译引擎

```bash
git clone <repo-url> GnVk
cd GnVk
make              # 编译 libGN_mini.so
make shaders      # 编译 GLSL → SPIR-V (如有着色器修改)
```

## 创建应用

应用入口是 `main.cpp` + 回调函数：

```cpp
#include <GN.h>

extern "C" {
GN_CALL void AppInit() {}
GN_CALL void AppUnInit() {}

GN_CALL void AppDisplay2D(unsigned int nTime) {
    // 2D 绘制代码
    GN_RunTextOut("Hello Vulkan!", 0, 150, 100, 0, 0, 0xffff3434, GNg::eBLEND_NORMAL);
}

GN_CALL void AppDisplay3D(unsigned int nTime) {}
GN_CALL void AppRun(unsigned int nTime) {}
}

GN_MAIN;
GN_InitLoadCallBackFunc(AppInit, AppUnInit);
GN_InitLoopCallBackFunc(AppDisplay2D, AppDisplay3D, AppRun);
GN_StartRun("My App", 800, 600, 0, false, true, true, 0,0,0,0);
GN_EXIT;
```

编译运行:
```bash
g++ -O2 -std=c++11 -fpermissive \
  -DGN_USING_VULKAN_GLFW_LINUX \
  -DGN_USING_SOURCE_CODE \
  -Isrc -Isrc/core -Isrc/graphics -Isrc/graphics/_Vulkan \
  -Isrc/audio -Isrc/input -Isrc/ui -Isrc/network \
  -Isrc/file -Isrc/platform -Isrc/media -Isrc/plugin \
  -Iexternal/include \
  main.cpp \
  -L. -lGN_mini -lglfw -lvulkan -lpng -lfreetype \
  -lopenal -lz -lX11 -lasound \
  -lavformat -lavcodec -lswscale -lavutil \
  -o myapp

export LD_LIBRARY_PATH=.:$LD_LIBRARY_PATH
./myapp
```

## 回调时序

```
GT_InitAllAndRun:
  └─ AppInit()                    // 一次性初始化
  └─ GF_FrameCycleProc1           // PreFrame回调 (pass前)
  └─ GF_FrameCycleProc2:
       ├─ GN_VkBeginFrame()       // 渲染pass开始
       ├─ AppRun(nTime)           // 逻辑更新
       ├─ AppDisplay2D(nTime)     // 2D绘制
       ├─ AppDisplay3D(nTime)     // 3D绘制
       └─ GN_VkEndFrame()         // 渲染pass结束+提交
  └─ AppUnInit()                  // 清理
```

## 常用 API

| 函数 | 用途 |
|------|------|
| `GNg::ImageDraw(img, x, y, w, h, color, texID, blend)` | 2D 纹理绘制 |
| `GNg::ImageDraw9Square(img, x, y, w, h, color)` | 9宫格绘制 |
| `GNg::LineFrame2dDraw(x, y, w, h, lw, color)` | 矩形边框 |
| `GNg::ImageLoad(path, num, &img, fmt)` | 加载纹理 |
| `GN_RunTextOut(text, count, x, y, w, h, color, blend)` | 文本输出 |
| `GNg::ViewCur->ortho_left/top/right/bottom` | 视口范围 |

## 调试

```bash
# 调试版本 (含符号)
make debug

# 启用 Vulkan Validation Layer
export VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation
./myapp
```
