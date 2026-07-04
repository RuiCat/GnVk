# GN_mini 引擎架构文档

> **版本**: Interface 2.2.0 / Engine 2026.06.25  
> **后端**: Vulkan + GLFW (完整迁移)  
> **许可**: MIT 开源协议  
> **平台**: Linux (Vulkan+GLFW) | Windows | macOS | Android | iOS

---

## 1. 引擎概述

GN_mini 是一个跨平台的轻量级实时 GUI 与 2D/3D 游戏运行时框架。设计目标为极致精简、高性能、可嵌入——核心 DLL 体积仅数百 KB，依赖极少但功能丰富。

### 核心设计原则

| 原则 | 说明 |
|------|------|
| **极简主义** | 最小化外部依赖，核心仅需 OpenGL + GLUT/SDL |
| **模块可选** | 通过编译宏按需启用功能模块，未启用模块零开销 |
| **平台抽象** | `_` 前缀的平台层将系统 API 差异封装，上层代码完全跨平台 |
| **实用优先** | 提供大量"开箱即用"的实用类（UI控件、动画引擎、文件浏览器等） |

---

## 2. 系统架构

### 2.1 分层架构图

```
┌─────────────────────────────────────────────────────────┐
│                    应用层 (Application)                   │
│  ┌──────────┬──────────┬──────────┬──────────────────┐  │
│  │ 2D 场景  │ UI 系统  │ 音频播放 │ 网络通信/文件IO  │  │
│  │ GN_2Dscene│ GN_UI   │ GN_Audio │ GN_NetTrans      │  │
│  │ GN_Frame  │ GN_Ui*  │ cBGM_MUSIC│ GN_File         │  │
│  └──────────┴──────────┴──────────┴──────────────────┘  │
├─────────────────────────────────────────────────────────┤
│                   引擎核心层 (Core)                       │
│  ┌──────────┬──────────┬──────────┬──────────────────┐  │
│  │ 内存管理 │ 数据结构 │ 数学库   │ 字符串/格式化     │  │
│  │ GN_Base  │ GN_Array │ GN_Linear│ GN_String        │  │
│  │ P<T>     │ sLIST    │ GN_2Dmath│ GN_Format        │  │
│  └──────────┴──────────┴──────────┴──────────────────┘  │
├─────────────────────────────────────────────────────────┤
│                 平台抽象层 (Platform)                     │
│  ┌──────────┬──────────┬──────────┬──────────────────┐  │
│  │ 图形后端 │ 音频后端 │ 系统接口 │ 网络后端          │  │
│  │_Graphics │ _Sound   │ _Sys     │ _Net              │  │
│  │ OpenGL   │ OpenAL   │ POSIX/Win│ BSD Sockets       │  │
│  └──────────┴──────────┴──────────┴──────────────────┘  │
├─────────────────────────────────────────────────────────┤
│                 主框架层 (Main Framework)                 │
│                _Main (GF_* 帧循环接口)                    │
│             负责窗口创建、消息循环、帧同步                 │
├─────────────────────────────────────────────────────────┤
│                    操作系统 (OS)                          │
└─────────────────────────────────────────────────────────┘
```

### 2.2 源码目录映射

```
src/
├── core/          核心运行时
│   ├── GN.h           主头文件(模块索引+依赖图)
│   ├── GN.cpp         主实现
│   ├── GN_Base.h/cpp  基础定义(类型、数学常量、内存宏)
│   ├── GN_Def.h       综合宏定义(G2D/GUI数据序列化宏)
│   ├── GN_Init.h/cpp  系统初始化、帧循环、资源目录管理
│   ├── GN_Frame.h/cpp 2D动画引擎(Flash式关键帧+补间)
│   ├── GN_RunRec.h/cpp 运行记录/调试工具
│   ├── GN_Array.h/cpp 动态数组、安全指针P<T>、内存池
│   ├── GN_String.h/cpp 字符串处理(UTF-8/GB2312转换)
│   ├── GN_Linear.h/cpp 线性/曲线插值(Ease函数)
│   ├── GN_Color.h/cpp  32位RGBA颜色运算
│   ├── GN_Format.h/cpp 打包格式读写(.g2d/.gui二进制)
│   ├── GN_Parallel.h/cpp 多线程并行计算
│   └── GN_AmCtrl.h/cpp 时间轴动画控制器
│
├── graphics/      图形系统
│   ├── _Graphics/      底层图形接口
│   │   ├── GN_G2D.h/cpp     OpenGL 2D渲染命令封装
│   │   └── GN_G2D_Def.h     图形数据结构定义
│   ├── GN_2D.h/cpp      cVIEW2D视口、sIMAGE图像、绘制API
│   ├── GN_2DArea.h/cpp  2D空间区域计算、碰撞检测
│   ├── GN_2Dmath.h/cpp  2D数学(向量/矩阵/四元数)
│   ├── GN_2Dscene.h/cpp  2D场景管理(图层/对象序列化)
│   ├── GN_2Dtype.h/cpp  2D图形数据结构(顶点/UV/颜色)
│   └── GN_Font.h/cpp    位图字体渲染(GB2312/ASCII)
│
├── ui/            UI系统 (GNu 命名空间)
│   ├── GN_UI.h/cpp          UI对象基类、类型枚举、事件回调
│   ├── GN_UiBase.h/cpp      基础UI元件(背景框/图片/文本)
│   ├── GN_UiListGui.h/cpp   内嵌自定义UI的滚动列表
│   ├── GN_UiListTree.h/cpp  树形列表(展开/折叠/拖拽)
│   ├── GN_UiTextIn.h/cpp    文本输入框(支持光标/选择/IME)
│   ├── GN_UiFBrowser.h/cpp  文件浏览器
│   ├── GN_UiPopDlg.h/cpp    弹出对话框/菜单
│   └── GN_Layout.h/cpp      自动布局引擎
│
├── audio/         音频系统 (GNa 命名空间)
│   ├── _Sound/           底层音频接口(OpenAL封装)
│   └── GN_Audio.h/cpp    高层音频API(sSOUND/cBGM_MUSIC)
│
├── input/         输入系统 (GN 命名空间)
│   ├── GN_Input.h/cpp       键盘/鼠标/触摸状态查询
│   └── GN_InputAct.h/cpp    输入动作映射(手势识别)
│
├── network/       网络系统 (GNn 命名空间)
│   ├── _Net/              底层网络接口(BSD Sockets)
│   ├── GN_NetBase.h/cpp   网络基础类
│   └── GN_NetTrans.h/cpp  HTTP/DTP/文件传输高级API
│
├── file/          文件系统 (GNf 命名空间)
│   └── GN_File.h/cpp      sFILE: 统一文件抽象(本地/zip/网络)
│
├── platform/      平台层
│   ├── _Main/            主框架(GF_FrameCycle* 帧循环)
│   ├── _Sys/             系统接口(文件IO/线程/时间/路径)
│   └── _SysDef/          平台选择宏(GN_SysDef.h)
│
├── media/         媒体播放 (GNm 命名空间)
│   └── Media/GN_Media.h/cpp  cVIDEO视频播放器(FFmpeg)
│
└── plugin/        编辑器插件
    └── Player/GN_EdPlugin.h  编辑器扩展接口
```

---

## 3. 命名空间与命名规范

### 3.1 命名空间体系

| 命名空间 | 职责 | 关键类/函数 |
|----------|------|------------|
| `GN::` | 通用工具 | `Key[]`, `Mouse[]`, `RunRec`, `TimeGet*()`, `sLIST<T>`, `P<T>` |
| `GNg::` | 2D图形+通用 | `sIMAGE`, `cVIEW2D`, `cTEXT_ART`, `cFRAME`, `sVEC2`, `sRECT` |
| `GN3d::` | 3D图形(可选) | `cSCENE`, `cMODEL`, `cLIGHT`, `cCAMERA` |
| `GNu::` | UI界面 | `cSCROLL_RECTEX`, `cSELECT_LIST_TEXT`, `cTEXT_INPUT`, `cSIM_DIALOG` |
| `GNf::` | 文件操作 | `sFILE`, `Open()`, `Close()`, `Read()`, `Write()` |
| `GNn::` | 网络通信 | `cCLIENT_HTTP`, `cDTP`, `cSOC_TCP`, `cSOC_UDP` |
| `GNa::` | 音频处理 | `sSOUND`, `cSOUND_SRC`, `cBGM_MUSIC` |
| `GNm::` | 视频播放 | `cVIDEO` |
| `GNd::` | 数据加密 | 编码转换、引擎专属格式 |

### 3.2 代码命名规范

| 元素 | 规则 | 示例 |
|------|------|------|
| 全局函数/变量 | 大写驼峰 | `GlobalValue`, `GlobalFunction()` |
| 结构体 | `s` + 全大写 | `sCOLOR`, `sIMAGE`, `sVEC2` |
| 类 | `c` + 全大写 | `cMODEL`, `cVIEW2D`, `cTEXT_ART` |
| 常量 | 全大写 | `GN_VERSION`, `GN_MAXPATH` |
| 枚举标志 | `e` + 全大写 | `eAM_LOOP`, `eUI_TYPE` |
| 局部变量/参数 | 全小写+下划线 | `char *_pstr`, `int max_count` |
| 内部成员 | `z` 前缀 | `zState`, `zVolume` (不建议外部使用) |
| 可设置属性 | `a` 前缀 | `aColor`, `aBlendmode` |

---

## 4. 模块依赖关系

### 4.1 编译期模块开关

```cpp
// 在 GN.h 中定义，按需注释/启用
#define GN_USING_2D          2    // 2D模块
//#define GN_USING_3D        4    // 3D模块(可选)
#define GN_USING_AUDIO       8    // 音频模块
#define GN_USING_NET        16    // 网络模块
#define GN_USING_UNZIP      32    // ZIP解压
#define GN_USING_UTILITY_LIB 1   // 实用库(★标记的类)
#define GN_USING_ADDITIONAL_LIB 256 // 附加库(序列化等)
```

### 4.2 依赖关系图

```
                    ┌──────────────┐
                    │   GN_Base    │  (基础类型/数学常量)
                    └──────┬───────┘
                           │
                    ┌──────▼───────┐
                    │   GN_Array   │  (动态数组/P<T>/内存池)
                    └──────┬───────┘
                           │
              ┌────────────┼────────────┐
              │            │            │
     ┌───────▼──────┐ ┌───▼────┐ ┌────▼─────┐
     │  GN_String   │ │ _Sys   │ │ GN_Color  │
     └───────┬──────┘ └───┬────┘ └────┬─────┘
             │            │           │
      ┌──────▼──────┐ ┌───▼────┐ ┌───▼──────┐
      │ GN_Parallel │ │GN_File │ │GN_Layout  │
      └──────┬──────┘ └───┬────┘ └────┬─────┘
             │            │           │
      ┌──────▼──────┐     │    ┌──────▼──────┐
      │ GN_Format   │     │    │ GN_2Dmath   │
      └─────────────┘     │    └──────┬──────┘
                          │           │
              ┌───────────┼───────────┼───────────┐
              │           │           │           │
     ┌───────▼──────┐ ┌──▼───┐  ┌───▼────┐ ┌───▼──────┐
     │ _Graphics    │ │_Sound│  │_Net    │ │GN_Linear  │
     │ (OpenGL)     │ │(OpenAL)│(Socket)│ │(Ease函数) │
     └───────┬──────┘ └──┬───┘  └───┬────┘ └────┬─────┘
             │           │          │           │
     ┌───────▼──────┐ ┌──▼───┐  ┌───▼────┐ ┌───▼──────┐
     │ GN_2D        │ │GN_Audio│ │GN_NetBase│GN_2DArea │
     │ (绘制API)    │ └───────┘ └────┬────┘ └────┬─────┘
     └───────┬──────┘               │           │
             │               ┌──────▼────┐ ┌───▼──────┐
     ┌───────▼──────┐        │GN_NetTrans│ │GN_2Dtype │
     │ GN_Input     │        └───────────┘ └────┬─────┘
     └───────┬──────┘                           │
             │                          ┌───────▼──────┐
     ┌───────▼──────┐                   │   GN_2D      │
     │ GN_Init      │                   │ (高层2D API) │
     └───────┬──────┘                   └───────┬──────┘
             │                                  │
     ┌───────▼──────┐                   ┌───────▼──────┐
     │ _Main        │                   │  GN_Font     │
     │ (帧循环框架) │                   └───────┬──────┘
     └───────┬──────┘                          │
             │               ┌─────────────────┼─────────────────┐
             │               │                 │                 │
     ┌───────▼──────┐ ┌──────▼──────┐  ┌──────▼──────┐  ┌──────▼──────┐
     │  GN_Frame    │ │ GN_2Dscene  │  │   GN_UI     │  │ GN_UiBase   │
     │  (动画引擎)  │ │ (2D场景)    │  │  (UI框架)   │  │ (基础UI)    │
     └──────────────┘ └─────────────┘  └──────┬──────┘  └──────┬──────┘
                                              │                │
                              ┌───┬───┬───────┼───────┬───┬────┤
                              │   │   │       │       │   │    │
                         ┌────▼─┐┌─▼──┐┌──▼──┐┌──▼──┐┌─▼──┐┌─▼───┐
                         │UiList││Tree││TextIn││FBrow││PopD││SIM_ │
                         │Gui   ││View││      ││ser  ││lg  ││DIALOG│
                         └──────┘└────┘└──────┘└─────┘└────┘└─────┘
```

---

## 5. 核心系统详解

### 5.1 平台抽象层 (`_*` 目录)

平台层是引擎跨平台的关键。每个 `_` 前缀的目录对应一个平台相关子系统：

| 模块 | 目录 | 职责 | Linux 后端 |
|------|------|------|-----------|
| 图形 | `_Graphics/` | 2D渲染命令 → GPU API | OpenGL 2.1+ (GLUT) |
| 音频 | `_Sound/` | 音频播放/采集 | OpenAL |
| 网络 | `_Net/` | Socket 通信 | BSD Sockets |
| 系统 | `_Sys/` | 文件IO/线程/时间/路径 | POSIX |
| 主框架 | `_Main/` | 窗口创建/消息循环 | GLUT |

**平台选择** (在 `_SysDef/GN_SysDef.h`):

```cpp
// Linux 平台 (二选一)
#define GN_USING_OPENGL_GLUT_LINUX   // GLUT 模式 (推荐)
//#define GN_USING_OPENGL_SDL_LINUX  // SDL 模式

// 其他平台
//#define GN_USING_OPENGL_WINDOW      // Windows 原生
//#define GN_USING_OPENGL_GLUT_MAC    // macOS
//#define GN_USING_OPENGLes_ANDROID_JAVA  // Android
```

### 5.2 帧循环系统

引擎采用经典的 **游戏循环 (Game Loop)** 模式。主框架 (`_Main`) 负责驱动：

```
                     ┌──────────────┐
                     │  程序入口     │
                     │ main()       │
                     └──────┬───────┘
                            │
                     ┌──────▼───────┐
                     │ GN_InitSys() │  初始化系统
                     └──────┬───────┘
                            │
              ┌─────────────┼─────────────┐
              │             │             │
     ┌───────▼──────┐ ┌───▼────┐ ┌─────▼──────┐
     │ GN_InitWords │ │GN_Init │ │GN_InitNet  │  初始化各模块
     │ (字体)       │ │Graphics│ │work()      │
     └───────┬──────┘ └───┬────┘ └─────┬──────┘
              │             │             │
              └─────────────┼─────────────┘
                            │
              ┌─────────────▼─────────────┐
              │  GF_FrameCycleBeginProc() │  进入帧循环
              └─────────────┬─────────────┘
                            │
              ┌─────────────▼─────────────┐
              │     while(运行中) {        │
              │   ┌─────────────────────┐ │
              │   │ GF_FrameCycleProc1()│ │  帧开始: 处理输入/逻辑
              │   │ GF_FrameCycleProc2()│ │  帧结束: 渲染/提交
              │   │ GN_ScreenUpData()   │ │  交换缓冲区
              │   │ GF_Sleep(ms)        │ │  帧率控制
              │   └─────────────────────┘ │
              │     }                     │
              └─────────────┬─────────────┘
                            │
              ┌─────────────▼─────────────┐
              │  GF_FrameCycleEndProc()   │  退出帧循环
              │  GN_UnInitSys()           │  释放资源
              └───────────────────────────┘
```

**GF_* 接口函数** (平台开发者需实现):

| 函数 | 调用时机 | 职责 |
|------|---------|------|
| `GF_AppArgsSet()` | 程序入口 | 设置命令行参数 |
| `GF_InitGraphics()` | 初始化 | 创建窗口/GL上下文 |
| `GF_FrameCycleBeginProc()` | 进入循环前 | 分配帧循环资源 |
| `GF_FrameCycleProc1()` | 每帧 | 处理逻辑更新 |
| `GF_FrameCycleProc2()` | 每帧 | 处理渲染提交 |
| `GF_FrameCycleEndProc()` | 退出循环后 | 释放帧循环资源 |
| `GF_Sleep()` | 每帧末尾 | CPU 空闲等待 |
| `GF_ScreenUpData()` | 每帧渲染后 | SwapBuffers |
| `GF_UnInitGraphics()` | 退出前 | 销毁窗口/GL上下文 |

### 5.3 内存管理

引擎提供轻量级内存管理：

- **`P<T>`**: 智能指针模板，自动引用计数，`New(n)` 分配数组
- **`sLIST<T>`**: 动态列表，支持 `add()` / `add_no_eq2()` 去重添加
- **`sARRAY<T>`**: 固定/动态数组，带边界检查
- **内存池**: `GN_Array` 提供 Pool 分配器，减少碎片

### 5.4 资源目录体系

```
assets/                     ← 引擎系统资源 (运行时)
├── ui/         UI组件素材
├── axis/       2D/3D控制轴图标
├── effects/    特效贴图
├── fileicon/   默认文件图标
└── fonts/      GB2312位图字体

应用资源目录 (运行时指定):
GN_AppDir("app/source")     ← 用户资源
└── *.png / *.g2d / *.gui   ...
```

---

## 6. 构建系统

### 6.1 Linux 构建

```bash
# 编译器: g++ ≥ 7
# 依赖: libgl-dev freeglut3-dev libpng-dev libopenal-dev

g++ -O3 -std=c++11 -fPIC -shared \
    -DGN_USING_OPENGL_GLUT_LINUX \
    -Isrc -Iexternal/include \
    src/core/*.cpp \
    src/graphics/*.cpp src/graphics/_Graphics/*.cpp \
    src/audio/*.cpp src/audio/_Sound/*.cpp \
    src/input/*.cpp src/ui/*.cpp src/network/*.cpp \
    src/network/_Net/*.cpp src/file/*.cpp \
    src/platform/_Main/*.cpp src/platform/_Sys/*.cpp \
    src/media/Media/*.cpp \
    -lGL -lglut -lpng -lopenal \
    -o libGN_mini.so
```

### 6.2 VSCode 集成

`build/.vscode/tasks.json` 提供一键编译任务，`c_cpp_properties.json` 配置 IntelliSense。

---

## 7. 第三方依赖

| 依赖 | 用途 | Linux 包 |
|------|------|---------|
| **OpenGL** | 2D/3D 图形渲染 | `libgl-dev` |
| **GLUT** | 窗口管理/输入 | `freeglut3-dev` |
| **libpng** | PNG 图像加载 | `libpng-dev` |
| **OpenAL** | 音频播放 | `libopenal-dev` |
| **libzip** | ZIP 压缩包读取 | `libzip-dev` (可选) |
| **FFmpeg** | 视频播放(可选) | `libavcodec-dev` 等 |

---

## 8. 数据流总览

```
用户输入 → GN::Key[] / Mouse[]
    │
    ▼
cVIEW2D (坐标变换) → UI 系统 (GNu::) → UI 事件回调
    │
    ▼
场景系统 (GN_2Dscene) + 动画引擎 (GN_Frame)
    │
    ▼
图形绘制 (GN_2D::ImageDraw → _Graphics/GN_G2D → OpenGL)
    │
    ▼
cVIEW2D::EndRender2Dand3D() → GF_ScreenUpData() → SwapBuffers
```

---

## 9. Vulkan 3D 渲染管线 (2026.06 完整版)

### 9.1 RenderPass 架构 (7 个)

| RenderPass | 附件 | 格式 | 用途 |
|-----------|------|------|------|
| `renderPass` | 1 Color | swapchain | 2D UI/Sprite (4混合模式) |
| `renderPass3D` | MSAA Color + MSAA Depth + Resolve | swapchain+D32 | Forward 3D (MSAA 4×) |
| `hdrRenderPass` | MSAA Color + MSAA Depth + HDR Resolve | R16G16B16A16 | Forward→HDR (Bloom前) |
| `shadowRenderPass` | 1 Depth | D32 (2048²) | 阴影预Pass (depthBias 1.25/1.75) |
| `compositeRenderPass` | 1 Color | swapchain | HDR→Swapchain合成 |
| `gBufferRenderPass` | 3 Color + 1 Depth | R16G16B16A16×3 + D32 | 延迟渲染 G-Buffer |
| `deferredLightRenderPass` | 1 Color | swapchain | 延迟光照→Swapchain |

### 9.2 渲染管线 (12 个 Pipeline)

| Pipeline | 着色器 | 功能 |
|----------|--------|------|
| `pipeline` | gn2d.vert/frag | 2D Alpha混合 |
| `pipelineBright` | gn2d.vert/frag | 2D 加法混合 |
| `pipelineMultiply` | gn2d.vert/frag | 2D 乘算混合 |
| `pipelineNull` | gn2d.vert/frag | 2D 无混合 |
| `pipeline3D` | gn3d.vert/frag | Blinn-Phong (纹理+镜面高光+8光源, MSAA) |
| `pipelinePBR` | gn3d_pbr.vert/frag | Cook-Torrance GGX PBR (MSAA) |
| `shadowPipeline` | gn3d_shadow.vert | 深度Only (depthBias) |
| `compositePipeline` | gn3d_composite.vert/frag | SimpleBloom + Reinhard |
| `skyboxPipeline` | gn3d_skybox.vert/frag | 天空盒立方体 (LEQUAL, depthWrite OFF) |
| `gBufferPipeline` | gn3d_gbuffer.vert/frag | G-Buffer MRT (3附件, 非MSAA) |
| `deferredLightPipeline` | gn3d_deferred_light.vert/frag | 延迟光照 (全屏三角形) |
| `ssaoPipeline` | gn3d_ssao.frag | 屏幕空间AO (预留) |

### 9.3 G-Buffer 布局 (延迟渲染)

```
Attachment 0 (R16G16B16A16_SFLOAT): Normal(RGB, N*0.5+0.5) + Roughness(A)
Attachment 1 (R16G16B16A16_SFLOAT): Albedo(RGB) + Metallic(A)
Attachment 2 (R16G16B16A16_SFLOAT): AO(R) + unused
Attachment 3 (D32_SFLOAT):         Depth
```

### 9.4 光照特性

| 特性 | Blinn-Phong | Forward PBR | Deferred |
|------|:---:|:---:|:---:|
| 光源类型 | 方向/点/聚光 | 方向/点/聚光 | 方向/点/聚光 |
| 最大光源 | 8 | 8 | 8 (独立于几何) |
| 阴影 | ✗ | Poisson 16点 PCF | Poisson 16点 PCF |
| Skybox IBL | ✗ | Cubemap采样 | Cubemap 半球采样 |
| 镜面模型 | Blinn-Phong | Cook-Torrance GGX | Blinn-Phong |
| 色调映射 | ✗ | Reinhard | Reinhard |
| 包裹光照 | ✗ | ✗ | ✓ (N·L+0.3)/1.3 |
| MSAA | 4× (自动) | 4× (自动) | ✗ (非MSAA, 计划TAA) |

### 9.5 帧循环 (示例: Deferred + HDR)

```
GN_VkBeginFrame()                          → 2D Pass (swapchain)
  [2D UI 批处理绘制]
GN_VkShadowPassBegin(lightVP)              → Shadow Pass (D32 2048²)
  GN_VkShadowDraw × N                       → 深度写入
GN_VkShadowPassEnd()
GN_VkBegin3DPassDeferred(vp, cam, lights)  → G-Buffer Pass (3×R16G16B16A16 + D32)
  GN_VkDrawSkybox(vp)                       → 天空盒 (depth≈1.0)
  GN_VkDraw3D_GBuffer × N                   → 写入albedo/normal/roughness/metallic/ao
GN_VkDeferredLighting(vp, cam, lights)     → Lighting Pass (swapchain)
  [全屏三角形: 逐光源PBR + Skybox IBL + Reinhard]
GN_VkEndFrame()                            → Submit + Present
```

### 9.6 新增 API

```cpp
// Forward PBR HDR (Bloom)
void GN_VkBegin3DPassPBR_HDR(const sMAT44 *vp, const sVEC3 *cam,
                              const void *lightUBO, uint32_t size);
void GN_VkCompositeToSwapchain();

// Deferred Rendering
void GN_VkBegin3DPassDeferred(const sMAT44 *vp, const sVEC3 *cam,
                               const void *lightUBO, uint32_t size);
void GN_VkDraw3D_GBuffer(const void *vertices, uint32_t vc,
                          const uint32_t *indices, uint32_t ic,
                          const sMAT44 *model,
                          float ar, float ag, float ab,
                          float roughness, float metallic, float ao,
                          const sIMAGE *albedoTex, const sIMAGE *normalTex);
void GN_VkDeferredLighting(const sMAT44 *vp, const sVEC3 *cam,
                            const void *lightUBO, uint32_t size);

// Skybox
void GN_VkDrawSkybox(const sMAT44 *viewProj);

// Texture support (Blinn-Phong / PBR)
void GN_VkDraw3D(..., const sIMAGE *texture=nullptr);
void GN_VkDraw3DPBR(..., const sIMAGE *albedoTex=nullptr,
                         const sIMAGE *normalTex=nullptr);
```

### 9.7 Push Constant 类型安全

```cpp
struct alignas(16) VkModelPushConstBP {   // Blinn-Phong
    float model[16];      // 64B
    float diffuse[4];     // 16B: RGB=diffuse, A=pad
    float specular[4];    // 16B: RGB=specular, A=shininess
}; // 96B total

struct alignas(16) VkModelPushConstPBR {  // PBR
    float model[16];      // 64B
    float albedo[4];      // 16B: RGB=albedo, A=pad
    float pbrParams[4];   // 16B: x=roughness, y=metallic, z=ao
}; // 96B total
```

---

*此文档基于 GN_mini 源码分析生成，版本 2026.06.25*
