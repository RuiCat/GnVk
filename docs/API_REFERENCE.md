# GN_mini API 参考手册

> **状态**：✅ 现行 — 2D/UI/音频/网络的 API 手册（§1–§9）+ 3D 实体层导航（§10）；签名以头文件为准。

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

## 10. `GN3d::` — 3D 实体层导航

> **这一章不是逐函数参考。** 3D 层有 50 多个头文件，接口每轮都在长；
> 把签名抄进文档，下一次改动就会让它变成错的 —— 错的参考手册比没有更糟。
> 所以这里只给四样东西：**分层、模块入口、最小骨架、实测踩过的坑**。
> **签名一律以头文件为准**：每个头文件顶部都写了它「解决什么问题、边界在哪、为什么这么设计」。

### 10.1 分层

```
游戏逻辑（输入 / 相机 / 玩法）
   │
   ├─ 场景层   cSCENE ───────── 这一帧画谁：剔除 → LOD → 排序
   │      │
   │      └─ 实体层  cENTITY ─── 四套控制器的接线盒
   │             ├─ cMESH_CONTROLLER     网格 + 世界 AABB + 视锥剔除
   │             ├─ cANIM_CONTROLLER     采样 → 混合 → 状态机 → IK → 调色板
   │             ├─ cPHYSICS_CONTROLLER  固定步长 + 快照插值 + 接触事件
   │             └─ cCHARACTER          胶囊移动（重力/坡/台阶/贴墙）
   │
   ├─ 数据与算法  cMODEL · sSKELETON · cANIM_PLAYER · cPHYSICS_WORLD · IK · 布娃娃 …
   └─ 契约与数学  GN_3Dmath · GN_Space · GN_Spatial · GN_3Dcamera
                 ↑ 以上全部纯 CPU，不含 Vulkan
渲染后端 GN_Vk*（仅 src/graphics/_Vulkan/，唯一与 GPU 打交道的层）
```

**这条分界是仓库最重要的约束**：除 `_Vulkan/` 外，3D 层不 `#include <vulkan/vulkan.h>`。
所以 `make unit` 能在没有 GPU 的机器上跑完全部逻辑测试（当前 815 例），
`make gpuCheck` 只验证「描述符/管线/UBO 布局」这类真正的 GPU 契约。
写新代码时如果发现自己在 CPU 层里摸 Vulkan 句柄，说明分层错了。

### 10.2 模块入口

**数学 / 空间 / 约定**

| 模块 | 头文件 | 一句话 |
|---|---|---|
| 数学 | `GN_3Dmath.h` | 向量/矩阵/四元数；矩阵**列主序**（直接喂 Vulkan） |
| 空间划分 | `GN_Spatial.h` | AABB / BVH / 动态 AABB 树 / `sFRUSTUM` / 射线 |
| 坐标约定 | `GN_Space.h` | +Y 上 / -Z 相机前向 / +Z 资产正面 / CCW 为正面；转换收口点 |
| 相机 | `GN_3Dcamera.h` | 镜头参数 + `SelfMove/SelfRot` 原语 + 自由/轨道/跟随 + 自动取景 |
| 级联阴影 | `GN_ShadowCascade.h` | CSM 分割、视锥角点、光源空间正交拟合、texel snapping |

**资产 / 网格 / 骨骼**

| 模块 | 头文件 | 一句话 |
|---|---|---|
| 模型 | `GN_3Dmodel.h` | `cMODEL`：OBJ 加载 + GPU 上传 + 绘制 + **世界 AABB（带缓存）** |
| glTF 导入 | `GN_3Dasset.h` | glTF 2.0 节点/网格/皮肤/动画；PMX 走 `src/mmd/` |
| 纹理 | `GN_3Dtexture.h` | glTF 纹理图片解码（PNG/JPEG）+ 采样参数 |
| 骨骼 | `GN_3Dskeleton.h` | 关节层级 + 绑定姿势 + 逆绑定矩阵 |
| 场景装配 | `GN_3Dscene.h` | 由节点树算世界矩阵、局部姿势、蒙皮调色板 |
| 蒙皮 | `GN_3DSkin.h` | 蒙皮顶点布局 + CPU 线性混合蒙皮（GPU 路径的对拍基准） |

**动画**

| 模块 | 头文件 | 一句话 |
|---|---|---|
| 采样与播放 | `GN_3Danim.h` | clip 采样（STEP / LINEAR）× 混合 × `cANIM_PLAYER` |
| 混合树 / 遮罩 | `GN_AnimBlend.h` | 1D 混合空间 + 相位同步 + 关节遮罩层栈 |
| 状态机 | `GN_AnimStateMachine.h` | 参数 → 状态转换 + 事件；并列时取先注册者（确定性） |
| IK | `GN_IK.h` | 两骨解析解（余弦定理 + 极向量），不可达时钳制并拉向目标 |
| Root Motion | `GN_RootMotion.h` | 从根关节位移提取移动/转角，交给 `cCHARACTER` |

**物理 / 角色**

| 模块 | 头文件 | 一句话 |
|---|---|---|
| 刚体 | `GN_Physics.h` | 球/盒/胶囊 + 静态三角网；顺序冲量 + 位置修正；**盒对三角网未实现** |
| 角色 | `GN_Character.h` | 胶囊移动：重力 → 水平 → 解穿透 → 斜坡 → 台阶 → 贴墙下滑 |
| 扫掠 | `GN_CapsuleSweep.h` | 胶囊对三角网 TOI（保守推进 + 二分），台阶抬升用它 |
| 布娃娃 | `GN_Ragdoll.h` | 骨架 → 球链 + distance 关节；**无关节角限制**（膝盖能反向弯） |

**实体 / 场景 / 编辑**

| 模块 | 头文件 | 一句话 |
|---|---|---|
| 实体 | `GN_Entity.h` | `cENTITY`：四控制器接线、`PrepareRender/Draw`、相机跟随目标 |
| 场景 | `GN_Scene.h` | `cSCENE`：剔除 → LOD → 排序 + 选择集 + 撤销钩子 |
| 编辑器控件 | `GN_Gizmo.h` | 网格/坐标轴/移动旋转缩放/包围盒/线框/样条 + 射线拖拽 + 吸附 |
| 编辑命令 | `GN_EditCommand.h` | 变换快照命令 + 撤销/重做栈 |

**渲染数据与管线**（仍是 CPU 侧；真正的 Vulkan 调用在 `_Vulkan/`）

| 模块 | 头文件 | 一句话 |
|---|---|---|
| 管线缓存 | `GN_Pipeline.h` | 图形/计算管线描述 + 内容哈希复用 |
| 着色器 | `GN_Shader.h` / `GN_Spirv.h` | SPIR-V 加载与缓存 / 模块结构校验 |
| 材质 | `GN_3Dmaterial.h` / `GN_Material.h` | CPU 光照属性 / GPU 材质实例（描述、校验、去重键） |
| 光源 | `GN_3Dlight.h` | 方向/点/聚光 + UBO 布局 + **层号按注册顺序分配**（与相机无关） |
| 多层渲染 | `GN_RenderPipeline.h` / `GN_RenderQueue.h` | 层枚举、程序、渲染项、掩码解析、层内排序、层队列 |
| LOD | `GN_3Dlod.h` | `sLOD_POLICY`：像素误差 + 屏幕占比，只会更精细 |
| 遮挡 / overdraw | `GN_HiZ.h` | 深度金字塔 + 遮挡测试 + overdraw 倍数测量 |

**效果（多数是 CPU 基座，GPU 后端另在 `_Vulkan/`）**

| 模块 | 头文件 | 一句话 |
|---|---|---|
| 粒子 | `GN_3Dparticle.h` / `GN_3DparticleGPU.h` | CPU 池与发射器 / SSBO 契约 + CPU 回退对拍 |
| 布料 | `GN_3Dcloth.h` / `GN_3DclothGPU.h` | XPBD CPU 模拟 / GPU compute 与 CPU 逐位等价（容差 1e-3） |
| 贴花 | `GN_3Ddecal.h` | 地面贴花池 |
| 探针 | `GN_3Dprobe.h` + `Relocation` + `Update` | SH9 网格 / relocation 偏移 / DDGI 异步更新调度 |
| 参数基座 | `giParams.h` · `postParams.h` · `atmosphereParams.h` · `weather.h` | UBO 默认值/校验/打包；纯函数，可单测 |
| 环境预设 | `GN_Environment.h` | 室内均匀 SH9 vs 天空 IBL 的选择 |

### 10.3 最小骨架

```cpp
#include <GN.h>
#include "GN_3Dcamera.h"
#include "GN_Entity.h"
#include "GN_Scene.h"

static GN3d::cSCENE  g_scene;
static GN3d::cENTITY g_ent;
static GN3d::cCAMERA g_cam;

extern "C" GN_CALL void AppInit() {
    g_ent.SetModel(myModel);            // 非拥有：myModel 必须活到实体析构
    g_scene.Add(&g_ent);
}

extern "C" GN_CALL void AppRun(unsigned int nTime) {
    // 固定步长 + 渲染插值：alpha 是「距离下一步还有多远」，不是帧时间。
    g_scene.Update(1.0f / 60.0f, 0.0f);
}

extern "C" GN_CALL void AppDisplay3D(unsigned int nTime) {
    GNg::sMAT44 view, proj, viewProj;
    g_cam.BuildViewProj(aspectRatio, &view, &proj, &viewProj);

    // ⚠ FromViewProj 是 static：写成 `f.FromViewProj(vp)` 会丢掉返回值，得到空视锥。
    GN3d::sFRUSTUM frustum = GN3d::sFRUSTUM::FromViewProj(viewProj);

    GN3d::sLOD_VIEW lodView;
    lodView.aScreenHeight = (float)screenHeight;
    lodView.aFovYRad      = g_cam.aFovY * GN3d::CAMERA_DEG_TO_RAD; // cCAMERA 里 aFovY 是**度**
    lodView.aViewProj     = viewProj;
    lodView.aCamPos       = g_cam.GetCoordV();

    g_scene.PrepareRender(lodView, &frustum);   // 剔除 + 选级 + 排序
    g_scene.Draw(g_cam.GetCoordV());
}
```

输入一律走 P0 的动作映射（`GN::IA::`），不要在引擎层直接读 `GN_IsKeyDown`：
`tests/lint/input.sh` 会拦。

### 10.4 实测踩过的坑

这些都不是理论风险，是本仓库调通过程中真实炸过的：

| 现象 | 原因 | 正确做法 |
|---|---|---|
| LOD 选级离谱（全部选最粗/最细） | `cCAMERA::aFovY` 是**度**，`sLOD_VIEW::aFovYRad` 要**弧度** | 乘 `CAMERA_DEG_TO_RAD` |
| 剔除一个都没剔 | `sFRUSTUM::FromViewProj` 是 **static**，调完没接返回值 | `sFRUSTUM f = sFRUSTUM::FromViewProj(vp);` |
| 模型动了几帧，AABB 不跟着变 | 世界 AABB 有缓存，粘贴顶点后没失效 | 改顶点调 `InvalidateLocalBounds()` |
| 布娃娃穿地 100 m | 球半径 < 单帧位移 | `aBoneRadius` 取略大于单帧位移（默认 0.09） |
| 落地瞬间动能爆到 2000+ | 胶囊绕长轴惯量太小 / 忘了 `BodyUpdateInertia` | 用球体模式；`AddBody` 后手动算一次惯量 |
| 影子随视角闪进闪出 | 把「选灯排名」当层号用 | 层号只走 `assignCubeShadowLayers`（按注册顺序） |
| 薄台阶「上台阶抖一下」 | 离散点查询漏掉「中途扫过」 | 用 `sweepCapsuleAgainstTriangles` |
| 角色推不动箱子 | 物理盒对胶囊无效（盒未实现接触） | 角色侧走 `cCHARACTER::SetPhysicsWorld` 的位置分离 |
| `sMESH` 析构崩 `invalid pointer` | 它用 `delete[]` 释放，不是 `free` | 传堆数组，别传栈数组 |
| demo 姿态「腿反了 / 左右摆」 | 位置偏移与旋转方向不同源 | 用 `tests/common/demoPoseCheck.h` 自查（含摆动平面判据） |

### 10.5 找不到接口时

1. 先看对应头文件的**顶部注释**：本仓库的文件头固定回答三件事 ——
   为什么需要它 / 边界在哪（什么没做）/ 关键取舍的理由；
2. 再看 `tests/unit/test*.cpp`：每个模块都有正反用例，**用例就是最准的用法说明**；
3. 最后看 `docs/CHANGELOG.md` 对应日期的条目：那里记了为什么这么定、量化数据、
   以及「哪些看着该做但实测后判定不做」。

---
