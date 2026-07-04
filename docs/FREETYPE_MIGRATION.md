# GN_mini FreeType 字体迁移报告

> 日期: 2026-06-22 | 状态: ✅ 完成 | 替代: 预生成 PNG 字库 → FreeType 实时渲染

---

## 一、背景

GN_mini 原始字体系统依赖预生成的 GB2312 PNG 字库（30 帧 × 256×256 图集），存在以下问题：

| 问题 | 影响 |
|------|------|
| 仅支持 GB2312 编码 | 源码必须是 GBK 编码，UTF-8 中文乱码 |
| 字体不可更换 | 需用专用工具预生成 PNG |
| 字号固定 | 无法运行时缩放 |
| 磁盘占用大 | 30 个 PNG 文件 ≈ 数 MB |

## 二、方案

使用 **FreeType** 库实时渲染字形到位图缓存，替代预生成 PNG 字库：

```
文本(UTF-8) → FT_ComputeArrangeData
  → 逐字符 UTF-8→Unicode 解码
  → cFREETYPE_FONT::GetGlyph(cp)
    → 缓存命中? 直接返回 UV
    → 缓存未命中:
      → FT_Load_Char + FT_Render_Glyph
      → PackIntoAtlas (动态图集打包)
      → 返回图集 UV
  → 生成 sDRAW_RANGE[]
→ ImageDrawMP(atlas, ranges) → Vulkan 批量绘制
```

## 三、新增文件

| 文件 | 行数 | 职责 |
|------|:--:|------|
| `src/graphics/GN_FontFT.h` | 82 | FreeType 字形缓存类 + 排版函数声明 |
| `src/graphics/GN_FontFT.cpp` | 375 | 完整实现（FT 初始化/光栅化/图集打包/缓存/UTF-8 排版） |

## 四、修改文件

| 文件 | 改动 |
|------|------|
| `Makefile` | +7 行: `pkg-config freetype2` 自动检测 → `-DGN_HAS_FREETYPE -lfreetype` |
| `src/graphics/GN_Font.h` | `eFONT_FREETYPE = 3` 枚举 + `cTEXT_ART::SetFreeTypeFont()` 方法 |
| `src/graphics/GN_Font.cpp` | `SetText()`/`Show()` 添加 FT 分支; `QueryFontMapFormat`/`zClassifyFontMapFormat` FT 检测 |
| `src/core/GN_RunRec.cpp` | `InitStyleToData()` 中 FT 覆盖默认字体 (`aFont.SetFreeTypeFont(16)`) |

## 五、系统字体搜索路径

```
Linux:   /usr/share/fonts/noto-cjk/NotoSansCJK-Regular.ttc
         /usr/share/fonts/TTF/uming.ttc
         /usr/share/fonts/wenquanyi/wqy-microhei-lite/wqy-microhei-lite.ttc
         ... (共 14 个路径)
macOS:   /System/Library/Fonts/PingFang.ttc
Windows: C:/Windows/Fonts/msyh.ttc
```

## 六、GB2312 PNG 字库 — 已完全移除

| 项目 | 状态 |
|------|:--:|
| `assets/gb2312/` (30 PNG 文件) | 🗑️ 已删除 |
| `GN_RunRec.cpp` PNG 加载代码 | 🗑️ 已移除 |
| `GT_SYS_FontDefName` 常量 | 🗑️ 已清空 |
| `eFONT_GB2312` 枚举值 | 🗑️ 已删除（`eFONT_FREETYPE` 改为值 2） |
| GB2312 字库引用 | ✏️ 已改为 FreeType |
| `GN_Sys.cpp` UTF-8↔GB2312 转换 | ⚠️ 保留（系统层路径编码需要） |

**FreeType 是唯一的中文渲染方案。** 不再需要预生成 PNG 字库。

## 七、使用方法

```cpp
// 自动模式 — 引擎启动时自动加载系统字体（无需代码改动）
GN_InitSys(); // 内部调用 FT_LoadSystemFont(16)

// 手动模式 — 指定字体路径和大小
cTEXT_ART myText;
myText.SetFreeTypeFont(24);  // 24px 系统字体

// 或指定 TTF 文件
GNg::FT_FontSys->Load("/path/to/font.ttf", 32);
myText.Init(GNg::FT_FontSys->GetAtlas(), &fmt);
```

## 八、待改进

| 项目 | 说明 |
|------|------|
| 字号切换 | 当前仅支持单一字号，换字号需重建图集 |
| 子像素渲染 | 当前使用灰度抗锯齿，可升级为 LCD 子像素 |
| 竖排文本 | `FT_ComputeArrangeData` 中竖排逻辑为占位 |
| 缓存持久化 | 当前仅内存缓存，重启后需重新光栅化 |
