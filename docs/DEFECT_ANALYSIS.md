# GN_mini 引擎 — 缺陷分析报告

> 基于源码静态分析 | 2026-06-20 | 分析范围: 38 个 .cpp 文件, ~20,000 行

---

## 一、严重度分类

| 级别 | 标志 | 说明 |
|:--:|:--:|------|
| 🔴 | Critical | 必然导致崩溃/安全漏洞/UB |
| 🟠 | High | 特定条件下崩溃/数据损坏 |
| 🟡 | Medium | 潜在风险/可移植性/维护性 |
| 🟢 | Low | 代码异味/风格问题 |

---

## 二、🔴 Critical — 必须修复

### 2.1 `memcpy` 到含虚函数/非平凡类型的对象

**影响**: 未定义行为，vtable 损坏，对象切片

| 文件 | 行 | 代码 | 问题 |
|------|:--:|------|------|
| `GN_G2D.cpp` | 99,104,252 | `memcpy(this, &src, sizeof(sIMAGE))` | `sIMAGE` 有虚析构函数 (`virtual ~sIMAGE`)，`memcpy` 破坏 vtable |
| `GN_Frame.cpp` | 106 | `memcpy(this, &src, sizeof(cFRAME_RC))` | `cFRAME_RC` 含虚函数 + 继承链 |
| `GN_Frame.cpp` | 148 | `memcpy(&zSound, &ssrc, sizeof(cSOUND_SRC))` | `cSOUND_SRC` 有虚函数 |
| `GN_UI.cpp` | 464 | `memcpy` 到 `cUI_NEAR_OBJECT` | 含 UI 继承链 |

**修复建议**: 用拷贝构造/赋值运算符替代 `memcpy`，或将类型改为 POD。

### 2.2 严格别名违规 (Strict Aliasing)

**影响**: 编译器优化导致错误代码生成

| 文件 | 行 | 代码 |
|------|:--:|------|
| `GN_Base.h` | 126 | `*(unsigned int*)&float_val` — 通过 `unsigned int*` 访问 `float` |
| `GN_Base.h` | 148 | `*(unsigned int*)&mf` — 同上 |
| `GN_UI.cpp` | 219-343 | 大量 `*(unsigned short*)&float_var` 类型双关 |
| `GN_Layout.cpp` | 63 | `*(unsigned short*)&x` |

**修复建议**: 用 `memcpy` 或 `std::bit_cast`(C++20) 或 `union` 进行类型双关。

### 2.3 183 处 `strcpy` / `strcat` — 缓冲区溢出

**影响**: 栈破坏，任意代码执行

| 文件 | 典型高危点 |
|------|------|
| `GN_Init.cpp:60` | `strcpy(GT_SysFile, pathfile)` — `pathfile` 长度无上限，缓冲区仅 377 字节 |
| `GN_Init.cpp:68` | `strcpy(GT_AppFile, pathfile)` — 同上 |
| `GN_Init.cpp:155` | `strcpy(md_name, mod_name)` — 变量尺寸不明 |
| `GN_String.cpp` | 13 处 `strcpy` — 字符串处理核心，大量外部输入 |

**修复建议**: 全局替换为 `strncpy` + 显式 null 终止，或使用 `GN_StrcpyData`(引擎自带安全版本)。

---

## 三、🟠 High — 建议修复

### 3.1 全局可变状态 (57 个全局变量)

**影响**: 多线程不安全，无法支持多实例

```
全局变量分布:
  GN_G2D_Def.cpp:  14 个 (GL 状态缓存: GT_RS_UsScissor, GT_RS_Texture, ...)
  GN_Init.cpp:      8 个 (应用默认配置)
  GN_Input.cpp:     6 个 (按键/鼠标状态数组)
  GN_Format.cpp:    5 个 (文件格式状态)
  GN_Sys.cpp:       4 个 (剪贴板缓冲区, 系统路径)
  其他:            20 个
```

**修复建议**: 将全局状态封装到 `Context` / `Instance` 结构体中，通过指针传递。

### 3.2 `delete` 非虚析构函数对象

**影响**: 派生类析构函数不被调用，资源泄漏

| 文件 | 代码 | 问题 |
|------|------|------|
| `GN_G2D.cpp:156` | `delete ((GT_PBUFFER*)z__attrb)` | `GT_PBUFFER` 无虚析构 → 派生资源泄漏 |
| `GN_G2D_Def.cpp:320` | `delete _vm_list` | `GT_sLIST_WHEEL_MT2` 无虚析构 |
| `GN_Sound.cpp:341` | `delete sr` | `GT_sSSRC` 无虚析构 |

> 编译器已发出警告: `-Wdelete-non-virtual-dtor`

**修复建议**: 添加 `virtual ~ClassName() = default;` 到基类。

### 3.3 `#include` .cpp 文件

**影响**: 违反 ODR，链接时可能重复符号

| 文件 | 被 Include 的 .cpp |
|------|------|
| `GN_G2D.cpp:29` | `#include "GN_G2D_Def.cpp"` |

**修复建议**: 拆分为 `.h` + `.cpp`，或重命名为 `.inc` / `.inl`。

### 3.4 固定大小缓冲区

| 文件 | 缓冲区 | 风险 |
|------|------|------|
| `GN_Init.cpp:56` | `char GT_SysDir[384]` | 路径 > 384 字节时溢出 |
| `GN_Init.cpp:64` | `char GT_AppDir[384]` | 同上 |
| `GN_Sys.cpp` | `GT_StrPasteBuffer` | 动态分配但无上限 |

---

## 四、🟡 Medium — 可移植性与维护性

### 4.1 平台不一致

| 功能 | Windows | macOS | Linux |
|------|:--:|:--:|:--:|
| 剪贴板 | ✅ Win32 API | ✅ Carbon/Cocoa | ⚠️ (刚修复) |
| 系统音量 | ✅ mixer API | ✅ GLUT ext | ⚠️ (刚修复) |
| 线程本地存储 | `__declspec(thread)` | `__thread` | `thread_local` |
| `#pragma comment(lib)` | ✅ | ❌ 已注释 | ❌ 已注释 |

### 4.2 硬编码路径

```cpp
// GN_G2D_Def.h:42-44 — 绝对路径
#include</usr/include/GL/gl.h>    // Linux 安装位置不同会编译失败
#include</usr/include/GL/glext.h>
```

### 4.3 异常处理缺失

整个引擎**不使用 C++ 异常**（无 `try`/`catch`），错误处理依赖：
- 返回 `0`/`NULL`/`false` 表示失败
- 无 RAII 资源管理（手动 `Release()` / `delete`）
- 构造函数可能失败但无法报告（无异常）

### 4.4 GBK 编码源码

所有源文件和注释使用 **GBK/GB2312 编码**，在 UTF-8 环境中显示为乱码，开发者体验差。

---

## 五、🟢 Low — 代码异味

### 5.1 宏过度使用

```cpp
// GN_Def.h — 1300+ 行几乎全是宏
#define GT_TMP_GET_IMAGE_WIDTH_AND_UV ...
#define GT_DrawTextSelectRange( CMP_IsCn1, CMP_IsCn2, PROC ) ... // 74 行宏
#define GT_TMP_UI_SCROLL_VIEW( MR, END_RANGE, NUM, LIMIT ) ...    // 105 行宏
```

- 调试困难（断点无法步入宏展开）
- 错误信息指向宏定义位置而非调用位置
- 多个 100+ 行的函数式宏 — 应改为 `inline` 函数或模板

### 5.2 命名不一致

| 应有 | 实有 |
|------|------|
| `sample12.cpp` | 全小写 |
| `Sample15.cpp` | 首字母大写 |
| `sample5.cpp` | 全小写 + 数字前无 0 |

### 5.3 魔法数字

```cpp
#define GN_SIZE_1GB   1073741824
#define GN_MAXPATH     260       // Windows 遗留, Linux 通常 4096
#define GN_MAXFILEPATH 384       // 不够 PATH_MAX(4096)
```

---

## 六、统计总览

| 类别 | 数量 | 严重度 |
|------|:--:|:--:|
| `memcpy` 到非平凡类型 | 9 处 | 🔴 Critical |
| 严格别名违规 | 15+ 处 | 🔴 Critical |
| `strcpy`/`strcat` 高危调用 | 183 处 | 🔴 Critical |
| 全局可变状态 | 57 个 | 🟠 High |
| `delete` 非虚析构 | 10 处 | 🟠 High |
| 固定大小缓冲区 | 8 处 | 🟠 High |
| `#include` .cpp | 1 处 | 🟡 Medium |
| 平台不一致 | 4 项 | 🟡 Medium |
| 100+ 行宏 | 8 个 | 🟢 Low |
| 编译警告 (GCC 16) | 150+ | 🟢 Low |

---

## 七、修复优先级路线图

```
Phase 1 (安全):   strcpy → strncpy 替换           ~2h
Phase 2 (UB):     memcpy 非平凡类型 → 赋值运算符   ~3h
Phase 3 (UB):     严格别名 → memcpy/union           ~2h
Phase 4 (稳定性): 非虚析构 → virtual                 ~1h
Phase 5 (架构):   全局状态 → Context 封装            ~8h
```

---

*分析完成 — 基于 GN_mini v2025.12.7 源码*
