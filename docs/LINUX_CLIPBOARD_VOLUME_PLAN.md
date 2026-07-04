# Linux 剪贴板 & 音量控制 — 实现路径规划

> 基于 `GN_mini` 引擎源码分析 | 2026-06-20

---

## 一、现状总结

| 功能 | 文件 | 当前 Linux 实现 | 问题 |
|------|------|----------------|------|
| 📋 剪贴板 GET | `src/platform/_Sys/GN_Sys.cpp:728` | 返回内部变量 `GT_StrPasteBuffer` | 不与系统剪贴板交互 |
| 📋 剪贴板 SET | `src/platform/_Sys/GN_Sys.cpp:731` | 写入内部变量 `GT_StrPasteBuffer` | 不与系统剪贴板交互 |
| 🔊 系统音量 | `src/audio/_Sound/GN_Sound.cpp:732` | OSS `ioctl(/dev/mixer)` | API 已废弃，fd=0 无效 |
| 🔊 源音量 | `src/audio/_Sound/GN_Sound.cpp:247` | OpenAL `alSourcef(AL_GAIN)` | ✅ 正常 |

---

## 二、剪贴板实现路径

### 2.1 架构选型

```
请求来源（UI 模块 / 应用层）
        │
        ▼
┌──────────────────────────────────┐
│  GN_StrGetPasteText()            │  ← 保持现有 API 不变
│  GN_StrSetPasteText()            │
└──────────────┬───────────────────┘
               │
    ┌──────────┼──────────┐
    ▼          ▼          ▼
┌──────┐ ┌──────┐ ┌──────────────┐
│ X11  │ │ SDL  │ │ 内部缓冲区    │
│原生  │ │ 后端 │ │ (fallback)   │
└──────┘ └──────┘ └──────────────┘
```

### 2.2 第一优先级：X11 原生实现

**文件**: `src/platform/_Sys/GN_Sys.cpp` — 新增 `#elif defined(_LINUX_)` 分支

**API 调用链**:
```
读取剪贴板:
  XOpenDisplay(NULL)
  → XGetSelectionOwner(disp, XA_CLIPBOARD)   // 检查是否有所有者
  → 创建窗口 (XCreateSimpleWindow)
  → XConvertSelection(disp, XA_CLIPBOARD, XA_STRING, ...)
  → 本地事件循环等待 SelectionNotify
  → XGetWindowProperty() 获取数据
  → XDestroyWindow() 清理

写入剪贴板:
  XOpenDisplay(NULL)
  → XSetSelectionOwner(disp, XA_CLIPBOARD, win, CurrentTime)
  → 保存文本到内部缓冲区
  → 响应 SelectionRequest 事件（发送数据给请求者）
```

**关键设计决策**:

| 决策点 | 选择 | 理由 |
|--------|------|------|
| 同时支持 PRIMARY + CLIPBOARD? | 是 | 中键粘贴(PRIMARY) 和 Ctrl+V(CLIPBOARD) 都要支持 |
| X11 Display 生命周期 | 惰性打开，缓存连接 | 避免每次操作都 `XOpenDisplay` |
| 事件循环冲突 | 独立 X11 连接 | 不与 GLUT 主循环共享 Display 连接 |
| 超时处理 | 100ms 超时 + 返回空 | 防止目标应用不响应时永久阻塞 |

**新增头文件依赖**: `<X11/Xlib.h>`, `<X11/Xatom.h>`
**新增链接依赖**: `-lX11`

**伪代码**:

```cpp
#ifdef _LINUX_
#include <X11/Xlib.h>
#include <X11/Xatom.h>

static Display *GT_X11Display = 0;
static Window   GT_X11Win = 0;
static Atom     GT_X11_Clipboard;
static Atom     GT_X11_UTF8;
static Atom     GT_X11_Targets;

static void GT_ClipboardInit() {
    if (!GT_X11Display) {
        GT_X11Display = XOpenDisplay(NULL);
        GT_X11Win = XCreateSimpleWindow(GT_X11Display, 
            DefaultRootWindow(GT_X11Display), 0,0,1,1,0,0,0);
        GT_X11_Clipboard = XInternAtom(GT_X11Display, "CLIPBOARD", False);
        GT_X11_UTF8 = XInternAtom(GT_X11Display, "UTF8_STRING", False);
        GT_X11_Targets = XInternAtom(GT_X11Display, "TARGETS", False);
    }
}

GN_SYS_API char const* GN_StrGetPasteText() {
    GT_ClipboardInit();
    // 1. 请求剪贴板内容
    XConvertSelection(GT_X11Display, GT_X11_Clipboard, 
                      GT_X11_UTF8, GT_X11_Clipboard, GT_X11Win, CurrentTime);
    // 2. 等待 SelectionNotify（最多 100ms）
    XEvent ev;
    unsigned long start = GT_TimeMs();
    while (GT_TimeMs() - start < 100) {
        if (XCheckTypedWindowEvent(GT_X11Display, GT_X11Win, 
                                    SelectionNotify, &ev)) {
            // 3. 读取属性
            if (ev.xselection.property != None) {
                Atom type; int format; unsigned long nitems, after;
                unsigned char *data = 0;
                XGetWindowProperty(GT_X11Display, GT_X11Win,
                    GT_X11_Clipboard, 0, 65536, False,
                    GT_X11_UTF8, &type, &format, &nitems, &after, &data);
                if (data) {
                    GN_StrcpyData(GT_StrPasteBuffer, (char*)data);
                    XFree(data);
                }
            }
            break;
        }
    }
    return GT_StrPasteBuffer;
}

GN_SYS_API void GN_StrSetPasteText(const char *text) {
    GT_ClipboardInit();
    // 1. 保存文本
    GN_StrcpyData(GT_StrPasteBuffer, text);
    // 2. 声明剪贴板所有权
    XSetSelectionOwner(GT_X11Display, GT_X11_Clipboard, GT_X11Win, CurrentTime);
    // 3. 处理后续 SelectionRequest（在下一帧事件循环中）
    //    GLUT 主循环的 X11 事件处理需要响应这些请求
}
#endif
```

### 2.3 第二优先级：Wayland fallback

当 X11 不可用时（纯 Wayland 环境），fallback 到命令行工具：

```cpp
// Wayland fallback (X11 连接失败时)
FILE *p = popen("wl-paste -t text 2>/dev/null", "r");
if (p) {
    fgets(GT_StrPasteBuffer, 384, p);
    pclose(p);
}
```

### 2.4 编译集成

在 `Makefile` 中：

```makefile
# 自动检测 X11
X11_CFLAGS := $(shell pkg-config --cflags x11 2>/dev/null)
X11_LIBS   := $(shell pkg-config --libs x11 2>/dev/null)
ifneq ($(X11_LIBS),)
  DEFINES  += -DGN_HAS_X11_CLIPBOARD
  LIBS     += $(X11_LIBS)
endif
```

---

## 三、系统音量控制实现路径

### 3.1 架构选型

```
应用层: GNa::SetSystemVolume(0.5f)
              │
    ┌─────────┼─────────┐
    ▼         ▼         ▼
┌──────┐ ┌──────┐ ┌──────────┐
│ ALSA │ │Pulse │ │  OSS     │
│原生  │ │Audio │ │ (fallback)│
└──────┘ └──────┘ └──────────┘
```

### 3.2 第一优先级：ALSA 原生实现

**文件**: `src/audio/_Sound/GN_Sound.cpp` — 重写 `#else //LINUX` 分支

**新增头文件**: `<alsa/asoundlib.h>`
**新增链接**: `-lasound`

**API 调用链**:
```
GNa::SetSystemVolume(vol):
  snd_mixer_open(&handle, 0)
  → snd_mixer_attach(handle, "default")
  → snd_mixer_selem_register(handle, NULL, NULL)
  → snd_mixer_load(handle)
  → 遍历寻找 Master/PCM 元素:
      snd_mixer_first_elem(handle)
      snd_mixer_selem_get_name(elem) == "Master"
  → snd_mixer_selem_set_playback_volume_all(handle, elem, vol * max)
  → snd_mixer_close(handle)

GNa::GetSystemVolume():        // 🆕 新增函数
  同上，但调用 snd_mixer_selem_get_playback_volume()
```

**伪代码**:

```cpp
#ifdef _LINUX_
#include <alsa/asoundlib.h>

GN_SOUND_API void GNa::SetSystemVolume(float vol) {
    snd_mixer_t *mixer;
    snd_mixer_selem_id_t *sid;
    
    if (snd_mixer_open(&mixer, 0) < 0) return;
    snd_mixer_attach(mixer, "default");
    snd_mixer_selem_register(mixer, NULL, NULL);
    snd_mixer_load(mixer);
    
    // 查找 Master 控件
    snd_mixer_selem_id_alloca(&sid);
    snd_mixer_selem_id_set_index(sid, 0);
    snd_mixer_selem_id_set_name(sid, "Master");
    
    snd_mixer_elem_t *elem = snd_mixer_find_selem(mixer, sid);
    if (!elem) {
        // fallback: 尝试 "PCM"
        snd_mixer_selem_id_set_name(sid, "PCM");
        elem = snd_mixer_find_selem(mixer, sid);
    }
    
    if (elem) {
        long minv, maxv;
        snd_mixer_selem_get_playback_volume_range(elem, &minv, &maxv);
        long target = (long)(minv + (maxv - minv) * vol);
        snd_mixer_selem_set_playback_volume_all(elem, target);
    }
    
    snd_mixer_close(mixer);
}

GN_SOUND_API float GNa::GetSystemVolume() {
    // ... 类似逻辑，调用 snd_mixer_selem_get_playback_volume()
    return (float)(current - minv) / (float)(maxv - minv);
}
#endif
```

### 3.3 补充：新增 `GetSystemVolume()` API

当前只有 `SetSystemVolume()`，没有对应的 `Get`。需要在 `GN_Sound.h` 中声明：

```cpp
GN_SOUND_API void  SetSystemVolume(float volume);   // 已有
GN_SOUND_API float GetSystemVolume();               // 🆕 新增
```

### 3.4 编译集成

```makefile
# 自动检测 ALSA
ALSA_CFLAGS := $(shell pkg-config --cflags alsa 2>/dev/null)
ALSA_LIBS   := $(shell pkg-config --libs alsa 2>/dev/null)
ifneq ($(ALSA_LIBS),)
  DEFINES  += -DGN_HAS_ALSA_VOLUME
  LIBS     += $(ALSA_LIBS)
endif
```

---

## 四、实施步骤

### Phase 1 — 基础设施 (1 小时)

| 步骤 | 文件 | 操作 |
|------|------|------|
| 1.1 | `Makefile` | 添加 `pkg-config` 检测 X11 和 ALSA |
| 1.2 | `src/platform/_SysDef/GN_SysDef.h` | 添加 `GN_HAS_X11_CLIPBOARD` / `GN_HAS_ALSA_VOLUME` 宏 |
| 1.3 | `src/platform/_Sys/GN_Sys.h` | 确认 API 声明不需要修改 |

### Phase 2 — 音量控制 (1.5 小时)

| 步骤 | 文件 | 操作 |
|------|------|------|
| 2.1 | `src/audio/_Sound/GN_Sound.cpp:700-738` | 重写 Linux 分支，用 ALSA 替换 OSS |
| 2.2 | `src/audio/_Sound/GN_Sound.h` | 新增 `GetSystemVolume()` 声明 |
| 2.3 | `src/audio/_Sound/GN_Sound.cpp` | 实现 `GetSystemVolume()` |

### Phase 3 — 剪贴板 (3 小时)

| 步骤 | 文件 | 操作 |
|------|------|------|
| 3.1 | `src/platform/_Sys/GN_Sys.cpp:683-734` | 重写 Linux 分支，实现 X11 剪贴板 |
| 3.2 | `src/platform/_Sys/GN_Sys.cpp` | 添加 Wayland fallback (`wl-paste`/`wl-copy`) |
| 3.3 | `src/platform/_Sys/GN_Sys.h` | 如有必要，添加辅助声明 |

### Phase 4 — 测试验证 (1 小时)

| 步骤 | 内容 |
|------|------|
| 4.1 | 编译验证：`make PLATFORM=linux` 无错误 |
| 4.2 | 功能测试：剪贴板 GET/SET 与系统应用互通 |
| 4.3 | 功能测试：音量调节与系统混音器同步 |
| 4.4 | 边界测试：X11 不可用时的 Wayland fallback |

---

## 五、风险与注意事项

| 风险 | 缓解措施 |
|------|---------|
| X11 事件循环冲突 | 使用独立 `Display*` 连接，不共享 GLUT 的 X11 Display |
| `SelectionRequest` 未响应 | 在 GLUT 空闲回调中处理待处理的剪贴板请求 |
| ALSA 设备名不一致 | 先尝试 "Master"，fallback 到 "PCM"，再 fallback 到遍历第一个有 playback volume 的元素 |
| 纯 Wayland 环境无 X11 | 检测 `$WAYLAND_DISPLAY`，自动 fallback 到 `wl-paste`/`wl-copy` |
| PipeWire 替代 PulseAudio | ALSA API 在 PipeWire 下通过 `pipewire-alsa` 兼容层工作 |

---

## 六、预计改动文件

```
修改:
  Makefile                          (+20 行) X11/ALSA 检测
  src/platform/_Sys/GN_Sys.cpp      (+120 行) X11 剪贴板实现
  src/audio/_Sound/GN_Sound.cpp     (+60 行)  ALSA 音量实现
  src/audio/_Sound/GN_Sound.h       (+5 行)   GetSystemVolume 声明
  src/platform/_SysDef/GN_SysDef.h  (+4 行)   新宏定义

总计: ~210 行新增代码，4 个文件修改
```
