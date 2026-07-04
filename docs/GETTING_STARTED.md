# GN_mini 快速入门指南 (Linux)

> 从零开始，10 分钟内运行你的第一个 GN_mini 应用  
> **版本 2026.06.25** — Vulkan 完整后端 | 2D/3D/延迟渲染/PBR/阴影

---

## 1. 环境准备

### 1.1 安装系统依赖

```bash
# Ubuntu/Debian
sudo apt update
sudo apt install -y \
    build-essential g++ \
    libglfw3-dev libvulkan-dev \
    libpng-dev libopenal-dev \
    libzip-dev libfreetype-dev

# Fedora
sudo dnf install -y \
    gcc-c++ glfw-devel vulkan-devel \
    libpng-devel openal-soft-devel libzip-devel freetype-devel

# Arch
sudo pacman -S --needed \
    gcc glfw vulkan-headers vulkan-icd-loader \
    libpng openal libzip freetype2
```

### 1.2 验证安装

```bash
g++ --version          # ≥ 7.0
dpkg -l | grep glut    # 确认 freeglut 已安装
dpkg -l | grep libpng  # 确认 libpng 已安装
```

---

## 2. 编译引擎

### 2.1 使用 VSCode（推荐）

```bash
# 在项目根目录打开 VSCode
cd GN_mini/
code build/   # 打开 build 目录（含 .vscode 配置）

# 按 Ctrl+Shift+B 编译
# 生成 libGN_mini.so 在项目根目录
```

### 2.2 手动命令行编译

```bash
cd GN_mini/

g++ -O3 -std=c++11 -fPIC -shared \
    -DGN_USING_OPENGL_GLUT_LINUX \
    -Isrc -Iexternal/include \
    src/core/*.cpp \
    src/graphics/*.cpp src/graphics/_Graphics/*.cpp \
    src/audio/*.cpp src/audio/_Sound/*.cpp \
    src/input/*.cpp src/ui/*.cpp \
    src/network/*.cpp src/network/_Net/*.cpp \
    src/file/*.cpp \
    src/platform/_Main/*.cpp src/platform/_Sys/*.cpp \
    src/media/Media/*.cpp \
    -lGL -lglut -lpng -lopenal \
    -o libGN_mini.so

echo "✅ 编译完成: libGN_mini.so"
```

---

## 3. 你的第一个 GN_mini 程序

### 3.1 最简示例 — Hello World

```cpp
// hello.cpp
#include <GN.h>

int main(int argc, char **argv) {
    // 1. 初始化引擎
    GN_InitSys();
    GN_InitWords();       // 初始化字体
    GN_InitNetwork();     // 初始化网络（如需要）

    // 2. 主循环由引擎驱动
    //    你的游戏逻辑写在帧回调中

    // 3. 清理
    GN_UnInitNetwork();
    GN_UnInitSys();
    return 0;
}
```

### 3.2 显示图像

```cpp
#include <GN.h>

// 全局图像对象
GNg::sIMAGE myImage;

void OnInit() {
    // 加载 PNG 图像
    myImage.InitToData("assets/my_sprite.png");
}

void OnFrame() {
    // 获取当前视口
    GNg::cVIEW2D *view = GNg::ViewCur;

    // 开始 2D 渲染
    view->BeginRender2D();

    // 在 (100, 100) 位置绘制 200×200 的图像
    GNg::ImageDraw(&myImage, 100, 100, 200, 200,
                   0xffffffff,  // 白色（无色调修改）
                   0.0f,        // 旋转角度
                   0);          // 混合模式（0=正常）

    // 结束渲染
    view->EndRender2Dand3D();
}

void OnExit() {
    myImage.Release();
}
```

### 3.3 处理输入

```cpp
void OnFrame() {
    // 键盘检测
    if (GN::Key1[GK_SPACE]) {
        GN_Log("空格键被按下！\n");
    }
    if (GN::Key[GK_UP]) {
        GN_Log("上箭头持续按住中...\n");
    }

    // 鼠标检测
    if (GN::Mouse1[GM_LEFT]) {
        int mx = GNg::ViewCur->mouseMX;
        int my = GNg::ViewCur->mouseMY;
        GN_Log("鼠标点击位置: %d, %d\n", mx, my);
    }

    // 触摸检测（移动端）
    if (GN::Touch[0]) {
        short tx = GNg::ViewCur->touchX[0];
        short ty = GNg::ViewCur->touchY[0];
    }
}
```

### 3.4 使用 UI 控件

```cpp
#include <GN.h>

GNu::cSELECT_LIST_TEXT myList;
GNu::cTEXT_INPUT       myInput;
GNg::cTEXT_ART         myLabel;

void OnInit() {
    // 创建选择列表
    myList.SetText("选项 A", 0, 50, 50, 200, 30);
    myList.SetText("选项 B", 1, 50, 85, 200, 30);
    myList.SetText("选项 C", 2, 50, 120, 200, 30);

    // 创建文本输入框
    myInput.SetText("在这里输入...");
}

void OnFrame() {
    unsigned int t = GN::TimeGet();

    // 更新 UI
    myList.UpdatePlay(t);
    myInput.UpdatePlay(t);

    // 渲染 UI
    GNg::ViewCur->BeginRender2D();
    myList.Show();
    myInput.Show();
    GNg::ViewCur->EndRender2Dand3D();

    // 处理选择事件
    if (myList.zSelectId >= 0) {
        GN_Log("用户选择了: %d\n", myList.zSelectId);
        myList.zSelectId = -1; // 清除选择状态
    }
}
```

### 3.5 播放音频

```cpp
#include <GN.h>

GNa::sSOUND     mySfx;
GNa::cBGM_MUSIC myBgm;

void OnInit() {
    // 加载音效
    GNa::SoundLoad("assets/click.wav", &mySfx);
    // 加载并播放背景音乐（-1 = 循环）
    myBgm.InitToData("assets/bgm.ogg", -1);
    myBgm.Play();
}

void OnFrame() {
    // 更新 BGM 播放状态（必须每帧调用）
    myBgm.UpdataPlay(15);

    // 按空格播放音效
    if (GN::Key1[GK_SPACE]) {
        // sSOUND 的播放由引擎内部管理
    }
}

void OnExit() {
    myBgm.Stop();
    myBgm.Release();
}
```

---

## 4. 项目目录结构

```
my_game/
├── main.cpp            # 程序入口
├── assets/             # 游戏资源（引擎自带）
├── app/
│   └── source/         # 你的应用资源
│       ├── images/
│       ├── sounds/
│       └── ui/
├── external/
│   └── include/        # 第三方头文件
├── src/                # 引擎源码
└── build/
    └── .vscode/        # VSCode 构建配置
```

---

## 5. 常用工作流

### 5.1 加载并显示精灵动画

```cpp
GNg::cFRAME_RC sprite;
sprite.Init("assets/character.g2d", GNg::Ts2D, 0.5f, 0.5f,
            1, 0xffffffff, GNg::eAM_REPEAT, 0, -1);

void OnFrame() {
    sprite.PlayFrame(GN::TimeGet());
    GNg::ViewCur->BeginRender2D();
    sprite.Show();
    GNg::ViewCur->EndRender2Dand3D();
}
```

### 5.2 绘制文本

```cpp
GNg::cTEXT_ART label;
label.SetFontStyle("default");
label.SetText("Hello GN_mini!", 0, 10, 10, 300, 40);

void OnFrame() {
    GNg::ViewCur->BeginRender2D();
    label.Show(0xff00ff00);  // 绿色文字
    GNg::ViewCur->EndRender2Dand3D();
}
```

### 5.3 读取文件

```cpp
GNf::sFILE file;
if (file.Open("app/source/data.txt")) {
    char buf[1024];
    unsigned int n = file.Read(buf, 1024);
    buf[n] = '\0';
    GN_Log("文件内容: %s\n", buf);
    file.Close();
}
```

### 5.4 多视口（分屏）

```cpp
GNg::cVIEW2D viewTop, viewBottom;

void OnInit() {
    int hw = 800, hh = 450;
    viewTop.Init(0, 0, hw, hh, 0, 0);           // 上半屏
    viewBottom.Init(0, hh, hw, hh, 0, 0);       // 下半屏
}

void OnFrame() {
    // 渲染到上半屏
    viewTop.BeginRender2D();
    // ... 绘制内容 ...
    viewTop.EndRender2Dand3D();

    // 渲染到下半屏
    viewBottom.BeginRender2D();
    // ... 绘制不同内容 ...
    viewBottom.EndRender2Dand3D();
}
```

---

## 6. 调试技巧

### 6.1 启用调试输出

```cpp
// 在 GN.h 中取消注释:
#define GN_DEBUG
```

### 6.2 使用运行记录器

```cpp
// 监控变量值
int myValue = 42;
GN_RunValueCheck(myValue, "myValue", 0);

// 性能计时
GN_RUNTSTART(0);
// ... 被测代码 ...
GN_RUNTEND(RenderTime);
```

### 6.3 日志输出

```cpp
GN_Log("调试信息: value=%d, ptr=%p\n", val, ptr);
GN_RunTextOut("状态文本", 8, 100, 200, 300, 20, 0xffff0000, 0);
```

---

## 7. 常见问题

### Q: 编译时报找不到 GL/glut.h
```bash
sudo apt install freeglut3-dev
```

### Q: 运行时段错误 "cannot open GN_system/"
引擎已迁移为 `assets/` 目录，确认 `assets/` 在可执行文件同级目录下。

### Q: 如何关闭不需要的模块以减小体积？
编辑 `src/core/GN.h`，注释掉不需要的宏：
```cpp
//#define GN_USING_AUDIO  8   // 不需要音频
//#define GN_USING_NET   16   // 不需要网络
```

### Q: FPS 太低怎么办？
1. 减少 `GF_Sleep()` 的等待时间
2. 检查是否每帧都在加载资源（资源加载应只在初始化时进行）
3. 使用 `GN_RUNTSTART/END` 定位性能瓶颈

---

## 8. 3D 渲染示例

### 8.1 Sample24 — 延迟渲染 + 阴影 + 天空盒

```bash
# 构建并运行 3D 示例
make -j4                              # 编译引擎
make sample N=24 2>/dev/null || \
  g++ -std=c++11 -DGN_USING_3D -DGN_USING_VULKAN_GLFW_LINUX \
    -Isrc -Isrc/core -Isrc/graphics -Isrc/graphics/_Vulkan \
    -Isrc/audio -Isrc/input -Isrc/ui -Isrc/platform \
    -Isrc/platform/_Main -Isrc/platform/_Sys -Isrc/platform/_SysDef \
    -Iexternal/include \
    samples/main.cpp samples/base/Sample24.cpp \
    libGN_mini.so -lglfw -lvulkan -lpng -lopenal -lz -lfreetype \
    -o build/sample_sample24

cd /path/to/GnVk
LD_LIBRARY_PATH=. ./build/sample_sample24
```

**Sample24 渲染管线**: Shadow Pass → G-Buffer Pass (MRT 3附件) → Skybox → Deferred Lighting (Lambert+Blinn-Phong+Skybox IBL+Reinhard)

### 8.2 可用 3D API 速览

```cpp
// Forward PBR (直接到 swapchain, 含 MSAA)
GN_VkBegin3DPassPBR(&vp, &cam, lightUBO, size);
model->DrawPBR();  // Cook-Torrance GGX
GN_VkEnd3DPass();

// Forward PBR + HDR + Bloom
GN_VkBegin3DPassPBR_HDR(&vp, &cam, lightUBO, size);
model->DrawPBR();
GN_VkEnd3DPass();
GN_VkCompositeToSwapchain();  // Bloom + Tonemap

// Deferred PBR + Shadow + Skybox
GN_VkShadowPassBegin(&shadowVP);
model->Draw();                      // 深度写入
GN_VkShadowPassEnd();

GN_VkBegin3DPassDeferred(&vp, &cam, lightUBO, size);
GN_VkDrawSkybox(&vp);               // 天空盒
GN_VkDraw3D_GBuffer(/* mesh data */); // G-Buffer 写入
GN_VkDeferredLighting(&vp, &cam, lightUBO, size); // 光照合成

// 纹理支持
model->aMeshes[0].aAlbedoTex = loadedTexture;  // albedo贴图
model->aMeshes[0].aNormalTex = loadedNormal;    // 法线贴图
```

---

## 9. 进阶参考

- **引擎架构**: `docs/ENGINE_ARCHITECTURE.md`
- **Vulkan 迁移**: `docs/VULKAN_MIGRATION.md`
- **API 手册**: `docs/API_REFERENCE.md`
- **编码规范**: `docs/代码功能简述与规范.txt`
- **完整示例**: `samples/` 目录（24 个官方示例）

---

*快速入门文档 — 基于 GN_mini v2026.06.25*
