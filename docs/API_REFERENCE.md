# GN_mini API 参考手册

> 按命名空间分类，标注实用性等级：★ = 常用  ★★ = 很常用  ★★★ = 核心必备

---

## 1. `GN::` — 核心工具命名空间

### 1.1 时间与帧率

| API | 说明 | 等级 |
|-----|------|------|
| `float GN::TimeRatio` | 时间比例（帧无关），用于精确插值 | ★★★ |
| `float GN::TimeRatioL` | 时间比例（限制 ≤3.0），用于平滑动画 | ★★★ |
| `unsigned int GN::TimeGet()` | 获取运行时间（毫秒） | ★★★ |
| `unsigned int GN::TimeGetFps()` | 获取当前 FPS | ★★ |
| `GN_AverageFrameTime(var, count)` | 帧时间平滑宏 | ★ |

### 1.2 输入状态

| API | 说明 | 等级 |
|-----|------|------|
| `bool GN::Key[GK_*]` | 键盘按键持续按下状态 | ★★★ |
| `bool GN::Key1[GK_*]` | 键盘按键首次按下（单帧） | ★★★ |
| `bool GN::Mouse[GM_*]` | 鼠标按键状态 | ★★★ |
| `bool GN::Mouse1[GM_*]` | 鼠标首次按下 | ★★★ |
| `bool GN::MouseM` | 鼠标是否移动中 | ★★ |
| `bool GN::Touch[GT_*]` | 触摸点状态 | ★★ |
| `bool GN::TouchM` | 触摸拖动中 | ★★ |

**按键码常量**:
```
GK_A ~ GK_Z, GK_0 ~ GK_9,  GK_SPACE, GK_ENTER, GK_ESC,
GK_UP, GK_DOWN, GK_LEFT, GK_RIGHT, GK_SHIFT, GK_CTRL,
GK_F1 ~ GK_F12, GK_BACK, GK_TAB
```

**鼠标码**: `GM_LEFT`, `GM_MIDDLE`, `GM_RIGHT`, `GM_TOP`(滚轮上), `GM_BOTTOM`(滚轮下)

### 1.3 运行记录与调试

| API | 说明 | 等级 |
|-----|------|------|
| `GN::RunRec` | 全局运行记录器实例 | ★★★ |
| `GN_RUNTSTART(nc)` | 开始计时 | ★ |
| `GN_RUNTEND(proc)` | 结束计时并输出 | ★ |
| `GN_RunValueCheck(val, name, id)` | 运行时变量监控 | ★★ |
| `GN_RunProgressDraw(...)` | 绘制进度条 | ★ |
| `GN_Log(fmt, ...)` | 日志输出 | ★★ |

### 1.4 数据结构

| API | 说明 | 等级 |
|-----|------|------|
| `P<T>` | 智能指针模板，自动引用计数 | ★★★ |
| `P<T>::New(n)` | 分配 n 个元素的数组 | ★★★ |
| `P<T>::Num()` | 获取数组元素数 | ★★★ |
| `GN::sLIST<T>` | 动态列表 | ★★★ |
| `sLIST::add(item)` | 追加元素 | ★★★ |
| `sLIST::add_no_eq2(item)` | 去重追加（指针比较） | ★★ |
| `sLIST::size()` | 元素数量 | ★★★ |
| `sLIST::operator[]` | 索引访问 | ★★★ |
| `GN::sARRAY<T>` | 固定数组 | ★ |

---

## 2. `GNg::` — 2D 图形命名空间

### 2.1 视口 — cVIEW2D ★★★

```cpp
class cVIEW2D : public sWINDOW {
    // 鼠标状态
    int  mouseMX, mouseMY;       // 鼠标在视口中的位置
    int  mouseMSX, mouseMSY;     // 鼠标移动速度
    short *touchX, *touchY;      // 触摸坐标

    // 生命周期
    void Init(short x, short y, short w, short h,
              short back_left, short back_top,
              short back_right=0, short back_bottom=0);
    void ReSize(short x, short y, short w, short h,
                bool auto_scale2D=true, bool auto_scale3D=true);
    void BeginRender2D();         // 开始2D渲染
    void EndRender2Dand3D();      // 结束渲染

    // 裁剪
    void ScissorRectSetup(int x, int y, int w, int h, bool nest=true);
    void ScissorRectRemove();

    // 碰撞检测
    bool HitRange(int x, int y, int w, int h);

    // 截屏
    uint8* SaveToData(int *size_out, ...);
    void SaveToFile(const char *filename, ...);

    static cVIEW2D *ViewCur;      // 当前活跃视口
};
```

### 2.2 图像 — sIMAGE ★★★

```cpp
struct sIMAGE {
    unsigned int width, height;    // 图像尺寸
    float uRange, vRange;          // UV范围（图集子区域）
    VkImage vkImage;               // Vulkan 纹理句柄

    // 从文件加载（支持 PNG）
    bool InitToData(const char *filename);
    // 释放纹理
    void Release();
};
```

**全局图像实例**:
- `GNg::Image` — 默认空白图像
- `GNg::BackScreen128` — 128x128 临时渲染目标
- `GNg::cVIEW2D::screenImg` — 屏幕截图缓存

### 2.3 绘制函数

| API | 说明 | 等级 |
|-----|------|------|
| `GNg::ImageDraw(sIMAGE*, x, y, w, h, color, angle, blend)` | 绘制图像 | ★★★ |
| `GNg::ImageDrawEx(sIMAGE*, ...)` | 绘制图像（扩展参数） | ★★ |
| `GNg::LineFrame2dDraw(x, y, w, h, width, color, blend)` | 绘制线框 | ★★ |
| `GNg::TextDraw(cTEXT_ART*, text, x, y, w, h)` | 快速文本绘制 | ★★ |

### 2.4 文本渲染 — cTEXT_ART ★★★

```cpp
class cTEXT_ART {
    // 设置文本
    void SetText(const char *text, int id, int x, int y, int w, int h);
    // 显示
    void Show(unsigned int color=0xffffffff);
    // 设置字体样式
    void SetFontStyle(const char *font_style_name);
    // 获取文本区域
    void GetTextRange(sRECT *out, int text_id=0);
};
```

### 2.5 数学类型

| 类型 | 说明 | 等级 |
|------|------|------|
| `GNg::sVEC2` | 2D向量 `{float x, y}` | ★★★ |
| `GNg::sVEC3` | 3D向量 `{float x, y, z}` | ★★ |
| `GNg::sVEC4` | 4D向量 | ★ |
| `GNg::sRECT` | 矩形 `{float left, top, right, bottom}` | ★★★ |
| `GNg::sRECT2` | 矩形（构造函数重载） | ★★★ |
| `GNg::sDRAW_AREA2` | 绘制区域 `{short x, y, w, h}` | ★★ |
| `GNg::sCOLOR` | 颜色 `{uint8 r, g, b, a}` | ★★ |
| `GNg::sMAT4` | 4×4 矩阵 | ★ |

### 2.6 动画引擎 — cFRAME / cFRAME_RC ★★★

```cpp
class cFRAME {
    // Flash 风格关键帧动画
    float aGenRngX, aGenRngY;    // 当前位置
    float aGenRngW, aGenRngH;    // 当前尺寸
    sIMAGE *zImage;              // 当前帧图像

    // 设置动画参数
    void SetParame(sARRANGE_FMT *fmt);
    // 播放
    void PlayFrame(unsigned int updatetime);
    // 移动到指定帧
    void MoveToID(int frame_id);
};

enum eAM_LOOP {  // 播放模式
    eAM_NOAM,        // 不播放
    eAM_ONCE,        // 播放一次
    eAM_ONCE_KEEP,   // 播放一次并保持最后一帧
    eAM_REPEAT,      // 循环播放
    eAM_UPDOWN,      // 来回播放
};
```

### 2.7 碰撞检测

| API | 说明 | 等级 |
|-----|------|------|
| `GNg::TestPointIntRect(mx, my, x, y, w, h)` | 点与矩形 | ★★★ |
| `GNg::TestRectIntersect(r1, r2)` | 矩形与矩形 | ★★ |
| `GNg::TestCircleIntersect(c1, c2)` | 圆与圆 | ★ |

---

## 3. `GNu::` — UI 系统命名空间

### 3.1 UI 对象基类 — iUI_OBJECT ★★★

```cpp
class iUI_OBJECT {
    unsigned short aType;   // UI 类型 (eUI_TYPE)

    virtual P<iUI_OBJECT> Clone();
    virtual void StatisticSize(cUI_GROUP *parent);
    virtual void OnRePos(sUI_LEVEL *parent);
    virtual void OnMove(short mx, short my);
    virtual void UpdateRun(unsigned int updatetime, sUI_LEVEL *parent);
    virtual void GetDrawArea(sDRAW_AREA2 &out, cUI_GROUP *parent);

    // 序列化
    virtual unsigned int DataSave(cSHARE_DATA *share, uint8 *out=0);
    virtual unsigned int DataLoad(cSHARE_DATA *share, const uint8 *data);
};
```

### 3.2 UI 类型枚举

| 类型 | 类 | 说明 | 等级 |
|------|-----|------|------|
| `eUI_BACK_FRAME` | 背景框 | 基础背景/边框 | ★★ |
| `eUI_IMAGE` | 静态图片 | 显示单张图像 | ★★ |
| `eUI_FRAME_RC` | 动画精灵 | 播放帧动画 | ★★ |
| `eUI_TEXT_ART` | 静态文本 | 显示文字 | ★★★ |
| `eUI_TEXT_IN` | `cTEXT_INPUT` | 文本输入框 | ★★★ |
| `eUI_PALETTE` | `cPALETTE` | 颜色选择器 | ★ |
| `eUI_SCROLL_RECT` | `cSCROLL_RECTEX` | 滚动面板 | ★★★ |
| `eUI_SCROLL_LIST` | `cSCROLL_LIST` | 滚动列表 | ★★ |
| `eUI_SCROLL_LIST_GUI` | `cSCROLL_LIST_GUI` | 内嵌UI滚动列表 | ★★ |
| `eUI_SELECT_LIST_ICON` | `cSELECT_LIST_ICON` | 图标选择列表 | ★★ |
| `eUI_SELECT_LIST_TEXT` | `cSELECT_LIST_TEXT` | 文本选择列表 | ★★★ |
| `eUI_TREE_VIEW` | `cTREE_VIEW` | 树形视图 | ★★ |
| `eUI_FILE_BROWSER` | `cFILE_BROWSER` | 文件浏览器 | ★★ |
| `eUI_SIM_DIALOG` | `cSIM_DIALOG` | 简单对话框 | ★★★ |
| `eUI_SELECT_BUTTON` | 选择按钮 | 单选/多选按钮 | ★★ |
| `eUI_PAGE_BAR` | 分页栏 | 选项卡切换 | ★★ |
| `eUI_SLIDER` | `cSLIDER` | 滑块控件 | ★★ |
| `eUI_BUTTON_TOUCH` | 触摸按钮 | 触控响应按钮 | ★★ |
| `eUI_JOYSTICK` | `cJOYSTICK` | 虚拟摇杆 | ★ |
| `eUI_VALUE_EXTRACT` | 数值提取器 | 调试用 | ★ |

### 3.3 核心 UI 类详解

#### cSCROLL_RECTEX ★★★ — 滚动面板

```cpp
class cSCROLL_RECTEX {
    // 设置内容
    void SetText(const char *text, int id, ...);
    void AddImage(sIMAGE *img, int id);
    void AddFrame(cFRAME *fm, int id);

    // 滚动控制
    float zTextOffset;          // 当前滚动偏移
    float zTextOffsettg;        // 目标滚动偏移
    float zOffsetSheep;         // 惯性系数（默认 0.15）

    // 更新与绘制
    unsigned int UpdatePlay(unsigned int updatetime);
    void Show();

    // 序列化
    uint8* zDataSave(uint8 *wpos, sLIST<sIMAGE*> &imgs);
    uint8* zDataLoad(const uint8 *rpos, P<sIMAGE*> &imgs);
};
```

#### cTEXT_INPUT ★★★ — 文本输入框

```cpp
class cTEXT_INPUT {
    char *zText;               // 输入文本缓冲区
    int   zCursorPos;          // 光标位置
    bool  zIsFocus;            // 是否获得焦点

    void SetText(const char *text);
    const char* GetText();
    void Show();
    unsigned int UpdatePlay(unsigned int updatetime);
};
```

#### cSELECT_LIST_TEXT ★★★ — 文本选择列表

```cpp
class cSELECT_LIST_TEXT {
    int  zSelectId;            // 当前选中项 ID（-1 = 无选择）
    bool zIsMouseFocus;        // 鼠标悬停

    void SetText(const char *text, int id, ...);
    void ClearText();
    unsigned int UpdatePlay(unsigned int updatetime);
    void Show();
};
```

#### cSIM_DIALOG ★★★ — 简单对话框

```cpp
class cSIM_DIALOG {
    void SetText(const char *text);
    void AddButton(const char *label, int action_id);
    unsigned int UpdatePlay(unsigned int updatetime);
    void Show();
};
```

### 3.4 UI 事件系统

| 回调指针 | 类型 | 说明 |
|----------|------|------|
| `GNu::UiObjectCreate` | `P<iUI_OBJECT>(*)(unsigned int)` | 创建自定义UI对象 |
| `GNu::UiActionCall` | `void(*)(iUI_OBJECT*, int)` | UI事件回调 |
| `GNu::RecvInputUiObject` | `iUI_OBJECT*` | 当前接收输入的对象 |
| `GNu::RequestInputUiObject` | `iUI_OBJECT*` | 请求输入焦点的对象 |

---

## 4. `GNf::` — 文件系统命名空间

### 4.1 sFILE ★★★ — 统一文件抽象

```cpp
struct sFILE {
    // 打开文件（支持本地/zip包内/网络路径）
    bool Open(const char *filename);
    // 关闭
    void Close();
    // 读取
    unsigned int Read(void *buf, unsigned int size);
    // 写入
    unsigned int Write(const void *buf, unsigned int size);
    // 获取文件大小
    unsigned int Size();
    // 判断是否存在
    bool Exist(const char *filename);
    // 获取完整路径
    const char* FullPath();
};
```

**路径格式支持**:
- 本地: `"image/icon.png"`
- ZIP内: `"data.pak/image/icon.png"`
- 网络: `"http://host/file.dat"`

---

## 5. `GNa::` — 音频命名空间

### 5.1 sSOUND ★★ — 音效

```cpp
struct sSOUND {
    unsigned int time;          // 播放时长(ms)
    char *fileName;             // 源文件路径

    bool InitToData(char *filename, bool point_sound=true);
    void Release();
};

// 加载音效文件
bool GNa::SoundLoad(char *filename, sSOUND *sound, bool point_sound=true);
```

### 5.2 cBGM_MUSIC ★★★ — 背景音乐

```cpp
class cBGM_MUSIC {
    bool InitToData(char *filename, int loop=-1);
    void Play();
    void Stop();
    void Pause();
    void SetVolume(float vol=1.0f, int fade_ms=0);
    void SetPitch(float pitch=1.0f, int fade_ms=0);
    void SetProgress(float progress);    // 0.0 ~ 1.0
    float GetProgress();
    eBGM_PLAY_STATE UpdataPlay(unsigned int updatetime=15);
    void Release();
};
```

---

## 6. `GNn::` — 网络命名空间

| 类/函数 | 说明 | 等级 |
|----------|------|------|
| `cSOC_TCP` | TCP 客户端/服务端 | ★★ |
| `cSOC_UDP` | UDP 通信 | ★★ |
| `cCLIENT_HTTP` | HTTP 客户端 | ★★★ |
| `cDTP` | 自定义数据传输协议 | ★ |

---

## 7. `GNm::` — 媒体命名空间

| 类 | 说明 | 等级 |
|-----|------|------|
| `cVIDEO` | 视频播放器（依赖 FFmpeg） | ★★ |

---

## 8. 全局面板变量

| 变量 | 命名空间 | 说明 |
|------|----------|------|
| `ViewCur` | `GNg::` | 当前活跃视口 |
| `View` | `GNg::` | 默认视口 |
| `Image` | `GNg::` | 默认空白图像 |
| `RunRec` | `GN::` | 运行记录器 |
| `Sound` | `GNa::` | 默认音效 |
| `TimeRatio` | `GN::` | 时间比例 |
| `GT_SystemInited` | 全局 | 系统初始化标志位掩码 |

---

## 9. 编码规范速查

| 前缀 | 含义 | 示例 |
|------|------|------|
| `s` | 结构体 | `sIMAGE`, `sVEC2` |
| `c` | 类 | `cVIEW2D`, `cFRAME` |
| `e` | 枚举 | `eAM_LOOP`, `eUI_TYPE` |
| `z` | 内部成员（勿用） | `zState`, `zData` |
| `a` | 可设置属性 | `aColor`, `aBlendmode` |
| `_` | 裸指针（局部） | `char *_pstr` |
| `i` | 接口基类 | `iUI_OBJECT` |
| `GT_` | 引擎内部工具 | `GT_SysDir`, `GT_SystemInited` |
| `GF_` | 平台框架接口 | `GF_FrameCycleProc1()` |
| `GN_` | 公共 API | `GN_InitSys()`, `GN_Log()` |

---

*此文档基于 GN_mini 头文件注释提取，完整 API 细节请参阅源码头文件*
