# GN_mini 引擎架构文档

> ⚠️ **历史文档** — 部分内容描述 OpenGL 架构，Vulkan 后端架构见 `VULKAN_MIGRATION.md`

> **版本**: Interface 2.2.0 / Engine 2026.06.25  
> **平台**: Linux (Vulkan+GLFW) | macOS | Android (可扩展)

## 一、设计哲学

### 核心理念

| 原则 | 说明 |
|------|------|
| **即时模式 + 保留模式** | 不强制单一范式。UI 控件可直接通过函数调用绘制（即时模式），也可创建持久对象（保留模式） |
| **层叠式架构** | 从底层系统 API 逐层封装到高层应用接口，每层可独立替换 |
| **极简主义** | 最小化外部依赖，核心仅需 Vulkan + GLFW |
| **面向数据** | 以数据流驱动渲染，避免深层继承树 |
| **平台抽象** | 平台相关代码统一通过 `_Sys/` + `_Main/` 接口接入，上层无感 |

---

## 二、分层架构

```
┌──────────────────────────────────────────────┐
│                 应用层 (App)                  │
├──────────────────────────────────────────────┤
│  UI 组件层  │  动画系统  │  粒子系统          │
├──────────────────────────────────────────────┤
│  2D 批处理   │  3D 管程 (PBR/Deferred)      │
├──────────────────────────────────────────────┤
│  Vulkan 后端 (GN_Vk.cpp)                     │
├──────────────────────────────────────────────┤
│  平台抽象 (_Main/ + _Sys/)                   │
├──────────────────────────────────────────────┤
│  GLFW │ OpenAL │ POSIX │ BSD Sockets         │
└──────────────────────────────────────────────┘
```

---

## 三、模块目录

| 层级 | 目录 | 职责 |
|------|------|------|
| 核心 | `core/` | GN.h (总入口), 内存池, 字符串, 帧循环, 线程池 |
| 图形 | `graphics/_Vulkan/` | Vulkan 后端 (sIMAGE/cVIEW2D/3D管程) |
| 图形 | `graphics/` | 2D/3D 数学, 纹理加载, 字体渲染 |
| 音频 | `audio/_Sound/` | OpenAL 封装 |
| 输入 | `input/` | 键盘/鼠标/触摸 |
| UI | `ui/` | UI 控件 (文本输入/弹出对话框/文件浏览/滚动列表) |
| 网络 | `network/_Net/` | TCP/UDP/HTTP |
| 文件 | `file/` | 文件 IO (fopen/内存映射/zip) |
| 平台 | `platform/_Sys/` `platform/_Main/` | 系统 API + 窗口循环 |
| 媒体 | `media/Media/` | FFmpeg 视频播放 |
| 插件 | `plugin/` | 文件格式插件接口 |

---

## 四、帧循环

```
GT_InitAllAndRun(窗口参数)
  └─ GF_FrameCycleBeginProc()     // 一次性引擎初始化
  └─ while(运行中):
       ├─ GF_FrameCycleProc1(nTime)  // PreFrame回调 (pass前)
       └─ GF_FrameCycleProc2(nTime):
            ├─ GN_VkBeginFrame()     // 开始渲染pass
            ├─ AppRun(nTime)         // 逻辑更新
            ├─ AppDisplay2D(nTime)   // 2D渲染
            ├─ AppDisplay3D(nTime)   // 3D渲染
            └─ GN_VkEndFrame()       // 提交+呈现
  └─ GF_FrameCycleEndProc()       // 引擎清理
```

### 延迟纹理上传

渲染 pass 内的纹理操作 (DataFill/ImageLoad) 不直接提交 GPU 工作，而是暂存到 `s_pendingUploads[]`，在下一帧 `GN_VkBeginFrame` 的 pass 前完成上传。详见 `VULKAN_MIGRATION.md`。

---

## 五、内存管理

| 组件 | 说明 |
|------|------|
| `P<T>` | 引用计数智能指针 (`GN_Array.h`)，支持零拷贝引用 |
| `sLIST<T>` | 双向链表，支持 splice/merge/sort |
| `sNODE<T>` | 树形节点基类 |
| `cMEM_POOL1` | 单块内存池 |
| `sTBUF_INT` | 栈+堆混合缓冲分配器 |

---

## 六、渲染管线架构 (Vulkan)

### 2D 管程

```
GN_VkBeginFrame → renderPass 开始
  └─ ImageDraw() 系列 → 顶点批处理 → 按descriptorSet分组
  └─ GN_VkEndFrame → vkCmdDraw (批量) → vkQueueSubmit
```

### 3D 管程

```
Shadow Pass → G-Buffer Pass → Deferred Light Pass → Composite Pass → Present
     (可选)       (几何体)        (全屏PBR光照)         (色调映射)
```

## 七、平台编译宏

| 宏 | 平台 |
|----|------|
| `GN_USING_VULKAN_GLFW_LINUX` | Linux (Vulkan+GLFW) |
| `GN_USING_OPENGL_GLUT_MAC` | macOS (遗留) |
| `GN_USING_OPENGL_WINDOW` | Windows (遗留) |

## 八、扩展阅读

- `VULKAN_MIGRATION.md` — Vulkan 后端详细说明
- `GETTING_STARTED.md` — 快速入门
- `API_REFERENCE.md` — API 参考
- `CHANGELOG.md` — 更新记录
