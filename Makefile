# GN_mini Engine — Cross-Platform Makefile
#
# 用法:
#   make                     自动检测平台，构建引擎库
#   make PLATFORM=linux      Linux (Vulkan + GLFW)
#   make PLATFORM=macos      macOS（未验证，OpenGL 遗留）
#   make PLATFORM=windows    MinGW-w64 交叉编译 → .dll（未验证）
#   make PLATFORM=android    Android NDK 交叉编译 → .so（未验证）
#   make CROSS=<prefix>      自定义交叉编译前缀
#
# 目标: all / debug / unit / asan / check / shaders / clean / help / info

# ═══════════════════════════════════════════════════════════
#  0. 平台自动检测
# ═══════════════════════════════════════════════════════════
ifeq ($(PLATFORM),)
  UNAME_S := $(shell uname -s)
  ifeq ($(UNAME_S),Linux)
    PLATFORM := linux
  else ifeq ($(UNAME_S),Darwin)
    PLATFORM := macos
  else ifneq (,$(findstring MINGW,$(UNAME_S)))
    PLATFORM := windows
  else ifneq (,$(findstring MSYS,$(UNAME_S)))
    PLATFORM := windows
  else
    $(warning ⚠ 未知平台 '$(UNAME_S)', 默认使用 linux)
    PLATFORM := linux
  endif
endif

# ═══════════════════════════════════════════════════════════
#  1. 交叉编译前缀 (可选)
#     make PLATFORM=windows CROSS=x86_64-w64-mingw32-
#     make PLATFORM=android CROSS=aarch64-linux-android-
# ═══════════════════════════════════════════════════════════
CROSS ?=

# ═══════════════════════════════════════════════════════════
#  2. 每平台配置矩阵
# ═══════════════════════════════════════════════════════════

# --- Linux (Vulkan + GLFW) ---
ifeq ($(PLATFORM),linux)
  TARGET_EXT     := so
  DEFINES        := -DGN_USING_VULKAN_GLFW_LINUX
  LIBS           := -lglfw -lvulkan -lpng -lopenal -lz
  CXX             = g++
  CXXFLAGS_BASE  := -O3 -std=c++11 -fpermissive -fPIC -Wall -Wextra
  CXXFLAGS_EXTRA := -msse2 -include cctype
  LDFLAGS        := -shared
  RUN_PREFIX     := LD_LIBRARY_PATH=.
endif

# --- macOS (GLUT + OpenGL.framework) ---
ifeq ($(PLATFORM),macos)
  TARGET_EXT     := dylib
  DEFINES        := -DGN_USING_OPENGL_GLUT_MAC -DHAVE_NANOSLEEP
  LIBS           := -framework OpenGL -framework GLUT -lpng -lopenal -lz
  CXX             = clang++
  CXXFLAGS_BASE  := -O3 -std=c++11 -fpermissive -fPIC -Wall -Wextra
  CXXFLAGS_EXTRA :=
  LDFLAGS        := -dynamiclib -install_name @rpath/libGN_mini.dylib
  RUN_PREFIX     := DYLD_LIBRARY_PATH=.
endif

# --- Windows (MinGW-w64 — CROSS 默认 x86_64-w64-mingw32-) ---
ifeq ($(PLATFORM),windows)
  TARGET_EXT     := dll
  DEFINES        := -DGN_USING_OPENGL_WINDOW
  LIBS           := -lopengl32 -lglut -lpng -lopenal -lz -lws2_32
  ifeq ($(CROSS),)
    CROSS := x86_64-w64-mingw32-
  endif
  CXX            = g++
  CXXFLAGS_BASE  := -O3 -std=c++11 -fpermissive -Wall -Wextra
  CXXFLAGS_EXTRA := -msse2 -DGL_GLEXT_PROTOTYPES -DGWL_USERDATA=GWLP_USERDATA
  LDFLAGS        := -shared -static-libgcc -static-libstdc++
  RUN_PREFIX     := wine
endif

# --- Android (NDK — CROSS 需手动指定) ---
ifeq ($(PLATFORM),android)
  TARGET_EXT     := so
  DEFINES        := -DGN_USING_OPENGLes_ANDROID_JAVA
  LIBS           := -lEGL -lGLESv1_CM -lopenal -lz
  CXX             = clang++
  CXXFLAGS_BASE  := -O3 -std=c++11 -fpermissive -fPIC -Wall
  CXXFLAGS_EXTRA :=
  LDFLAGS        := -shared
  ifndef ANDROID_NDK
    $(warning ⚠ ANDROID_NDK 未设置, 请 export ANDROID_NDK=/path/to/ndk)
  endif
  ifeq ($(CROSS),)
    $(warning ⚠ 请指定 CROSS, 例如: make PLATFORM=android CROSS=aarch64-linux-android21-)
  endif
endif

# ═══════════════════════════════════════════════════════════
#  2b. 图形后端（BACKEND=vulkan | opengl，缺省 vulkan）
#      Vulkan 后端是唯一带离屏/无窗口能力的后端，且完全不依赖窗口系统；
#      Windows 用 VK_KHR_win32_surface、macOS 用 MoltenVK 的 VK_EXT_metal_surface
#      （都由 glfwGetRequiredInstanceExtensions 自动给出，引擎侧无需分支）。
#      BACKEND=opengl 保留原有的 GLUT/GLES 老路径（platform/_Main 目前是 GLFW 层，
#      选 opengl 需自行提供对应的窗口层）。
#      注：这段必须放在所有 per-platform 块之后——那些块会重新定义 DEFINES/LIBS。
# ══════════════════════════════════════════════════════════
BACKEND ?= vulkan

ifeq ($(BACKEND),vulkan)
  ifeq ($(PLATFORM),macos)
    DEFINES        := -DGN_USING_VULKAN_GLFW_MAC -DHAVE_NANOSLEEP
    LIBS           := -lglfw -lvulkan -lpng -lopenal -lz
    CXXFLAGS_EXTRA :=
  endif
  ifeq ($(PLATFORM),windows)
    DEFINES        := -DGN_USING_VULKAN_GLFW_WINDOWS
    LIBS           := -lglfw -lvulkan -lpng -lopenal -lz -lws2_32
    CXXFLAGS_EXTRA := -msse2
  endif
endif

# ── 模块级宏（每模块独立控制源码/DLL/SDK模式）───────────
# 定义 _USING_SOURCE_CODE 使所有模块的 __declspec 为空
MODULE_DEFINES := -DGN_USING_SOURCE_CODE \
                  -DGN_INIT_USING_SOURCE_CODE \
                  -DGN_MAIN_USING_SOURCE_CODE \
                  -DGN_SYS_USING_SOURCE_CODE \
                  -DGN_G_USING_SOURCE_CODE \
                  -DGN_SOUND_USING_SOURCE_CODE \
                  -DGN_NET_USING_SOURCE_CODE \
                  -DGNm_USING_SOURCE_CODE

# ── Linux 可选特性检测 (pkg-config) ─────────────────────
ifeq ($(PLATFORM),linux)
  # X11 剪贴板支持
  ifeq ($(shell pkg-config --exists x11 && echo yes),yes)
    X11_CFLAGS := $(shell pkg-config --cflags x11)
    X11_LIBS   := $(shell pkg-config --libs x11)
    DEFINES    += -DGN_HAS_X11_CLIPBOARD
    CXXFLAGS_EXTRA += $(X11_CFLAGS)
    LIBS       += $(X11_LIBS)
  endif
  # ALSA 音量控制支持
  ifeq ($(shell pkg-config --exists alsa && echo yes),yes)
    ALSA_CFLAGS := $(shell pkg-config --cflags alsa)
    ALSA_LIBS   := $(shell pkg-config --libs alsa)
    DEFINES    += -DGN_HAS_ALSA_VOLUME
    CXXFLAGS_EXTRA += $(ALSA_CFLAGS)
    LIBS       += $(ALSA_LIBS)
  endif
  # FFmpeg 媒体支持 (系统最新版)
  ifeq ($(shell pkg-config --exists libavcodec libavformat libavutil libswscale && echo yes),yes)
    FFMPEG_CFLAGS := $(shell pkg-config --cflags libavcodec libavformat libavutil libswscale)
    FFMPEG_LIBS   := $(shell pkg-config --libs libavcodec libavformat libavutil libswscale)
    DEFINES       += -DGN_HAS_FFMPEG
    CXXFLAGS_EXTRA += $(FFMPEG_CFLAGS)
    LIBS          += $(FFMPEG_LIBS)
  endif
  # FreeType 字体渲染支持
  ifeq ($(shell pkg-config --exists freetype2 && echo yes),yes)
    FT_CFLAGS := $(shell pkg-config --cflags freetype2)
    FT_LIBS   := $(shell pkg-config --libs freetype2)
    DEFINES   += -DGN_HAS_FREETYPE
    CXXFLAGS_EXTRA += $(FT_CFLAGS)
    LIBS      += $(FT_LIBS)
  endif
  # libjpeg（P1c glTF 纹理：JPEG 解码；缺失时只支持 PNG）
  ifeq ($(shell pkg-config --exists libjpeg && echo yes),yes)
    JPEG_CFLAGS := $(shell pkg-config --cflags libjpeg)
    JPEG_LIBS   := $(shell pkg-config --libs libjpeg)
    DEFINES   += -DGN_HAS_JPEG
    CXXFLAGS_EXTRA += $(JPEG_CFLAGS)
    LIBS      += $(JPEG_LIBS)
  endif
endif

# ═══════════════════════════════════════════════════════════
#  3. 通用设置
# ═══════════════════════════════════════════════════════════
LIB_STEM   ?= GN_mini
TARGET     := lib$(LIB_STEM).$(TARGET_EXT)
BUILD_DIR  ?= build/obj
CXX        := $(CROSS)$(CXX)
CXXFLAGS   := $(CXXFLAGS_BASE) $(CXXFLAGS_EXTRA)
DEFINES    := $(DEFINES) $(MODULE_DEFINES)

INCLUDES   := -Isrc -Isrc/core -Isrc/graphics -Isrc/graphics/_Vulkan -Isrc/audio -Isrc/input -Isrc/ui \
              -Isrc/network -Isrc/file -Isrc/platform \
              -Isrc/media -Isrc/plugin \
              -Iexternal/include

# ── 源文件列表 ──────────────────────────────────────────
CORE_SRC   := $(wildcard src/core/*.cpp)
GFX_SRC    := $(wildcard src/graphics/*.cpp) \
              $(wildcard src/graphics/_Vulkan/*.cpp)
AUDIO_SRC  := $(wildcard src/audio/*.cpp) \
              $(wildcard src/audio/_Sound/*.cpp)
INPUT_SRC  := $(wildcard src/input/*.cpp)
UI_SRC     := $(wildcard src/ui/*.cpp)
NET_SRC    := $(wildcard src/network/*.cpp) \
              $(wildcard src/network/_Net/*.cpp)
FILE_SRC   := $(wildcard src/file/*.cpp)
PLAT_SRC   := $(wildcard src/platform/_Main/*.cpp) \
              $(wildcard src/platform/_Sys/*.cpp)
MEDIA_SRC  := $(wildcard src/media/Media/*.cpp)
MMD_SRC    := $(wildcard src/mmd/*.cpp)

ENGINE_SRC := $(CORE_SRC) $(GFX_SRC) $(AUDIO_SRC) $(INPUT_SRC) \
              $(UI_SRC) $(NET_SRC) $(FILE_SRC) $(PLAT_SRC) $(MEDIA_SRC) \
              $(MMD_SRC)
ENGINE_OBJ := $(patsubst src/%.cpp, $(BUILD_DIR)/%.o, $(ENGINE_SRC))

# ── 颜色 ────────────────────────────────────────────────
GREEN   := \033[0;32m
YELLOW  := \033[0;33m
CYAN    := \033[0;36m
BLUE    := \033[0;34m
RESET   := \033[0m

# ═══════════════════════════════════════════════════════════
#  4. 目标
# ═══════════════════════════════════════════════════════════
.PHONY: all
all: banner $(TARGET)
	@echo "$(GREEN)✅ 编译完成: $(TARGET)  [平台: $(PLATFORM)]$(RESET)"

.PHONY: banner
banner:
	@echo "$(BLUE)╔════════════════════════════════════════╗$(RESET)"
	@echo "$(BLUE)║  GN_mini  $(shell printf '%-12s' $(PLATFORM)) → $(TARGET)$(BLUE) ║$(RESET)"
	@echo "$(BLUE)╚════════════════════════════════════════╝$(RESET)"

# ── 链接 ──────────────────────────────────────────────
$(TARGET): $(ENGINE_OBJ)
	@echo "$(YELLOW)🔗 链接 $@ ...$(RESET)"
	$(CXX) $(ENGINE_OBJ) $(LDFLAGS) $(LIBS) -o $@
	@echo "$(GREEN)   → $(shell ls -lh $@ 2>/dev/null | awk '{print $$5}')$(RESET)"

# ── 编译 ──────────────────────────────────────────────
$(BUILD_DIR)/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	@echo "$(CYAN)  CC  $(notdir $<)$(RESET)"
	$(COMPILE.cc)

# ═══════════════════════════════════════════════════════════
#  5. 单元测试（示例测试已移除，待重新设计）
# ═══════════════════════════════════════════════════════════
TEST_LIBS  := -L. -l$(LIB_STEM) $(LIBS)

# ── 单元测试（纯逻辑，无 GPU/窗口依赖）──────────────────
UNIT_DIR   := tests/unit
# 示例的键位绑定表（P0）也要被单测直接验：把这两个 TU 一起编进单测二进制，
# 保证"改绑定表"与"跑绑定用例"永远是同一份代码。
UNIT_DEMO_SRC := tests/mmdViewer/mmdViewerActions.cpp tests/imoScene/imoSceneActions.cpp
# 程序化测试资产（P1）：单测与 `make fixtures` 共用同一份生成器。
UNIT_FIXTURE_SRC := tests/fixtures/fixtures.cpp
UNIT_SRC   := $(wildcard $(UNIT_DIR)/*.cpp) $(UNIT_DEMO_SRC) $(UNIT_FIXTURE_SRC)
UNIT_BIN   := build/unit_tests
# 单测要复用两个示例的键位绑定表（P0）与程序化资产生成器（P1）：把它们所在目录加进 include 路径
UNIT_FLAGS := -O2 -std=c++11 -fpermissive $(DEFINES) -I$(UNIT_DIR) -Itests/mmdViewer -Itests/imoScene -Itests/fixtures $(INCLUDES)

# ── 程序化测试资产（P1）：仓库不含 PMX/GLB，所以骨架/蒙皮资产由代码生成 ──
FIXTURE_SRC   := $(wildcard tests/fixtures/*.cpp)
FIXTURE_BIN   := build/fixtures
FIXTURE_FLAGS := -O2 -std=c++11 -fpermissive $(DEFINES) -Itests/fixtures $(INCLUDES)

.PHONY: fixtures
fixtures: $(FIXTURE_BIN)
	@echo "$(YELLOW)🧱 生成程序化测试资产 → .local/fixtures/...$(RESET)"
	@LD_LIBRARY_PATH=.:$$LD_LIBRARY_PATH ./$(FIXTURE_BIN) .local/fixtures

$(FIXTURE_BIN): $(FIXTURE_SRC) $(TARGET)
	@echo "$(YELLOW)🧱 编译测试资产生成器...$(RESET)"
	@mkdir -p build
	$(CXX) $(FIXTURE_FLAGS) $(FIXTURE_SRC) $(TEST_LIBS) -o $(FIXTURE_BIN)

.PHONY: unit
unit: $(TARGET)
	@echo "$(YELLOW)🧪 编译并运行单元测试...$(RESET)"
	@mkdir -p build
	$(CXX) $(UNIT_FLAGS) $(UNIT_SRC) $(TEST_LIBS) -o $(UNIT_BIN)
	@LD_LIBRARY_PATH=.:$$LD_LIBRARY_PATH ./$(UNIT_BIN)

.PHONY: check
check: all unit physicsBoxes characterDemo
	@echo "$(GREEN)✅ check 通过: 构建 + 单元测试 + 物理场景 + 角色场景$(RESET)"

# ── P1b GPU 验收（需要 Vulkan 设备与可见窗口；无 GPU 时退出码 2 = SKIP）──
GPU_CHECK_SRC   := $(wildcard tests/gpu/*.cpp)
GPU_CHECK_BIN   := build/gpuCheck
# gpuCheck 复用 tests/fixtures（P1 程序化资产）验证实体层蒙皮接线（P2）
GPU_CHECK_FLAGS := -O2 -std=c++11 -fpermissive $(DEFINES) -Itests/fixtures $(INCLUDES)

.PHONY: gpuCheck
# 编译与运行拆开：smoke 只该依赖二进制，不该顺带以窗口模式跑一次
# （无显示时会 SKIP 并让 make 直接失败）。
gpuCheck: $(GPU_CHECK_BIN)
	@echo "$(YELLOW)▶ 运行 GPU 验收（无 /dev/dri 时会 SKIP）...$(RESET)"
	@LD_LIBRARY_PATH=.:$$LD_LIBRARY_PATH ./$(GPU_CHECK_BIN)

$(GPU_CHECK_BIN): $(GPU_CHECK_SRC) $(TARGET)
	@echo "$(YELLOW)🖥  编译 GPU 管线验收程序...$(RESET)"
	@mkdir -p build
	$(CXX) $(GPU_CHECK_FLAGS) $(GPU_CHECK_SRC) $(UNIT_FIXTURE_SRC) $(TEST_LIBS) -o $(GPU_CHECK_BIN)

# ── 无窗口渲染验收（不需要窗口/X11，只要 Vulkan 设备；可跑 lavapipe）──
HEADLESS_SRC   := $(wildcard tests/headless/*.cpp)
HEADLESS_BIN   := build/headlessCheck
HEADLESS_FLAGS := -O2 -std=c++11 -fpermissive $(DEFINES) $(INCLUDES)

.PHONY: headlessCheck
headlessCheck: $(HEADLESS_BIN)
	@echo "$(YELLOW)▶ 运行无窗口渲染验收（不需要 DISPLAY）...$(RESET)"
	@LD_LIBRARY_PATH=.:$$LD_LIBRARY_PATH ./$(HEADLESS_BIN)
	@mkdir -p .local/tmp
	@echo "$(YELLOW)▶ 帧编排等价性（cRENDER_FRAME vs 裸 sFRAME_3D_SCOPE 逐字节比对）...$(RESET)"
	@LD_LIBRARY_PATH=.:$$LD_LIBRARY_PATH ./$(HEADLESS_BIN) 640 400 4 .local/tmp/frame_parity.png frame; \
		rc=$$?; if [ $$rc -eq 2 ]; then echo "  [SKIP] 无 Vulkan 设备"; elif [ $$rc -ne 0 ]; then exit $$rc; fi

$(HEADLESS_BIN): $(HEADLESS_SRC) $(TARGET)
	@echo "$(YELLOW)🪟  编译无窗口渲染验收程序...$(RESET)"
	@mkdir -p build
	$(CXX) $(HEADLESS_FLAGS) $(HEADLESS_SRC) $(TEST_LIBS) -o $(HEADLESS_BIN)

# ── 物理场景验收（P4：20 个箱子堆叠 + 球撞倒；纯 CPU，不需要 GPU/窗口）──
PHYSICS_BOXES_SRC   := $(wildcard tests/physicsBoxes/*.cpp)
PHYSICS_BOXES_BIN   := build/physicsBoxes
PHYSICS_BOXES_FLAGS := -O2 -std=c++11 -fpermissive $(DEFINES) $(INCLUDES)

.PHONY: physicsBoxes
physicsBoxes: $(PHYSICS_BOXES_BIN)
	@echo "$(YELLOW)📦 物理场景验收（20 箱堆叠 + 球撞倒，120 帧）...$(RESET)"
	@LD_LIBRARY_PATH=.:$$LD_LIBRARY_PATH ./$(PHYSICS_BOXES_BIN)

$(PHYSICS_BOXES_BIN): $(PHYSICS_BOXES_SRC) $(TARGET)
	@echo "$(YELLOW)📦 编译物理场景验收程序...$(RESET)"
	@mkdir -p build
	$(CXX) $(PHYSICS_BOXES_FLAGS) $(PHYSICS_BOXES_SRC) $(TEST_LIBS) -o $(PHYSICS_BOXES_BIN)

# ── 角色场景验收（P5：台阶 + 斜坡 + 跳跃；纯 CPU，不需要 GPU/窗口）──
CHARACTER_DEMO_SRC   := $(wildcard tests/characterDemo/*.cpp)
CHARACTER_DEMO_BIN   := build/characterDemo
CHARACTER_DEMO_FLAGS := -O2 -std=c++11 -fpermissive $(DEFINES) $(INCLUDES)

.PHONY: characterDemo
characterDemo: $(CHARACTER_DEMO_BIN)
	@echo "$(YELLOW)🚶 角色场景验收（台阶 + 斜坡 + 跳跃，240 帧）...$(RESET)"
	@LD_LIBRARY_PATH=.:$$LD_LIBRARY_PATH ./$(CHARACTER_DEMO_BIN)

$(CHARACTER_DEMO_BIN): $(CHARACTER_DEMO_SRC) $(TARGET)
	@echo "$(YELLOW)🚶 编译角色场景验收程序...$(RESET)"
	@mkdir -p build
	$(CXX) $(CHARACTER_DEMO_FLAGS) $(CHARACTER_DEMO_SRC) $(TEST_LIBS) -o $(CHARACTER_DEMO_BIN)

# ── 物理场景视频（P0–P6 综合演示）：逐帧离屏渲染 → ffmpeg 编码 ──
# 需要 Vulkan 设备（容器里用 lavapipe）与 ffmpeg（不在 PATH 时可用 FFMPEG=... 指定）。
PHYSICS_VIDEO_SRC   := $(wildcard tests/physicsVideo/*.cpp)
PHYSICS_VIDEO_BIN   := build/physicsVideo
PHYSICS_VIDEO_FLAGS := -O2 -std=c++11 -fpermissive $(DEFINES) $(INCLUDES)
PHYSICS_VIDEO_OUT   ?= .local/video
PHYSICS_VIDEO_FRAMES ?= 240
PHYSICS_VIDEO_SIZE  ?= 1280 720
FFMPEG              ?= $(shell command -v ffmpeg 2>/dev/null || echo /work/.sbx/bin/ffmpeg)

.PHONY: physicsVideo
physicsVideo: $(PHYSICS_VIDEO_BIN)
	@echo "$(YELLOW)🎬 渲染物理场景（$(PHYSICS_VIDEO_FRAMES) 帧 @ $(PHYSICS_VIDEO_SIZE)）...$(RESET)"
	@LD_LIBRARY_PATH=.:$$LD_LIBRARY_PATH ./$(PHYSICS_VIDEO_BIN) --out $(PHYSICS_VIDEO_OUT)/frames \
		--frames $(PHYSICS_VIDEO_FRAMES) --size $(PHYSICS_VIDEO_SIZE)
	@echo "$(YELLOW)🎞  编码 mp4...$(RESET)"
	@$(FFMPEG) -y -loglevel error -framerate 30 -i $(PHYSICS_VIDEO_OUT)/frames/frame_%05d.png \
		-c:v libx264 -preset slow -crf 20 -pix_fmt yuv420p -movflags +faststart \
		$(PHYSICS_VIDEO_OUT)/physics-collision.mp4
	@ls -lh $(PHYSICS_VIDEO_OUT)/physics-collision.mp4

$(PHYSICS_VIDEO_BIN): $(PHYSICS_VIDEO_SRC) $(TARGET)
	@echo "$(YELLOW)🎬 编译物理场景视频渲染器...$(RESET)"
	@mkdir -p build
	$(CXX) $(PHYSICS_VIDEO_FLAGS) $(PHYSICS_VIDEO_SRC) $(TEST_LIBS) -o $(PHYSICS_VIDEO_BIN)

# ── 灯光 / 火焰场景视频（光照对比 + GPU 粒子）──
FIRE_VIDEO_SRC   := $(wildcard tests/fireLighting/*.cpp)
FIRE_VIDEO_BIN   := build/fireLighting
FIRE_VIDEO_FLAGS := -O2 -std=c++11 -fpermissive $(DEFINES) $(INCLUDES)
FIRE_VIDEO_OUT   ?= .local/video
FIRE_VIDEO_FRAMES ?= 315
FIRE_VIDEO_SIZE  ?= 1280 720

.PHONY: fireVideo
fireVideo: $(FIRE_VIDEO_BIN)
	@echo "$(YELLOW)🔥 渲染灯光/火焰场景（$(FIRE_VIDEO_FRAMES) 帧 @ $(FIRE_VIDEO_SIZE)）...$(RESET)"
	@LD_LIBRARY_PATH=.:$$LD_LIBRARY_PATH ./$(FIRE_VIDEO_BIN) --out $(FIRE_VIDEO_OUT)/fire-frames \
		--frames $(FIRE_VIDEO_FRAMES) --size $(FIRE_VIDEO_SIZE)
	@echo "$(YELLOW)🎞  编码 mp4...$(RESET)"
	@$(FFMPEG) -y -loglevel error -framerate 30 -i $(FIRE_VIDEO_OUT)/fire-frames/frame_%05d.png \
		-c:v libx264 -preset slow -crf 20 -pix_fmt yuv420p -movflags +faststart \
		$(FIRE_VIDEO_OUT)/fire-lighting.mp4
	@ls -lh $(FIRE_VIDEO_OUT)/fire-lighting.mp4

$(FIRE_VIDEO_BIN): $(FIRE_VIDEO_SRC) $(TARGET)
	@echo "$(YELLOW)🔥 编译灯光/火焰渲染器...$(RESET)"
	@mkdir -p build
	$(CXX) $(FIRE_VIDEO_FLAGS) $(FIRE_VIDEO_SRC) $(TEST_LIBS) -o $(FIRE_VIDEO_BIN)

# ── 编辑器控件演示视频（网格 / 坐标轴 / Gizmo / 包围盒 / 线框 / 样条）──
GIZMO_VIDEO_SRC   := $(wildcard tests/editorGizmo/*.cpp)
GIZMO_VIDEO_BIN   := build/editorGizmo
GIZMO_VIDEO_FLAGS := -O2 -std=c++11 -fpermissive $(DEFINES) $(INCLUDES)
GIZMO_VIDEO_OUT   ?= .local/video
GIZMO_VIDEO_FRAMES ?= 360
GIZMO_VIDEO_SIZE  ?= 1280 720

.PHONY: gizmoVideo
gizmoVideo: $(GIZMO_VIDEO_BIN)
	@echo "$(YELLOW)🧩 渲染编辑器控件演示（$(GIZMO_VIDEO_FRAMES) 帧 @ $(GIZMO_VIDEO_SIZE)）...$(RESET)"
	@LD_LIBRARY_PATH=.:$$LD_LIBRARY_PATH ./$(GIZMO_VIDEO_BIN) --out $(GIZMO_VIDEO_OUT)/gizmo-frames \
		--frames $(GIZMO_VIDEO_FRAMES) --size $(GIZMO_VIDEO_SIZE)
	@echo "$(YELLOW)🎞  编码 mp4...$(RESET)"
	@$(FFMPEG) -y -loglevel error -framerate 30 -i $(GIZMO_VIDEO_OUT)/gizmo-frames/frame_%05d.png \
		-c:v libx264 -preset slow -crf 20 -pix_fmt yuv420p -movflags +faststart \
		$(GIZMO_VIDEO_OUT)/editor-gizmo.mp4
	@ls -lh $(GIZMO_VIDEO_OUT)/editor-gizmo.mp4

$(GIZMO_VIDEO_BIN): $(GIZMO_VIDEO_SRC) $(TARGET)
	@echo "$(YELLOW)🧩 编译编辑器控件演示...$(RESET)"
	@mkdir -p build
	$(CXX) $(GIZMO_VIDEO_FLAGS) $(GIZMO_VIDEO_SRC) $(TEST_LIBS) -o $(GIZMO_VIDEO_BIN)

# ── 控件交互演示（真实鼠标路径 + 吸附 + 相机跟随）──
GIZMO_INPUT_SRC   := $(wildcard tests/gizmoInput/*.cpp)
GIZMO_INPUT_BIN   := build/gizmoInput
GIZMO_INPUT_FLAGS := -O2 -std=c++11 -fpermissive $(DEFINES) $(INCLUDES)

.PHONY: gizmoInput
gizmoInput: $(GIZMO_INPUT_BIN)
	@echo "$(YELLOW)🖱  渲染控件交互演示（$(GIZMO_VIDEO_OUT)/gizmo-input.mp4）...$(RESET)"
	@LD_LIBRARY_PATH=.:$$LD_LIBRARY_PATH ./$(GIZMO_INPUT_BIN) --out $(GIZMO_VIDEO_OUT)/gizmo-input-frames \
		--frames 300 --size $(GIZMO_VIDEO_SIZE)
	@$(FFMPEG) -y -loglevel error -framerate 30 -i $(GIZMO_VIDEO_OUT)/gizmo-input-frames/frame_%05d.png \
		-c:v libx264 -preset slow -crf 20 -pix_fmt yuv420p -movflags +faststart \
		$(GIZMO_VIDEO_OUT)/gizmo-input.mp4
	@ls -lh $(GIZMO_VIDEO_OUT)/gizmo-input.mp4

$(GIZMO_INPUT_BIN): $(GIZMO_INPUT_SRC) $(TARGET)
	@echo "$(YELLOW)🖱  编译控件交互演示...$(RESET)"
	@mkdir -p build
	$(CXX) $(GIZMO_INPUT_FLAGS) $(GIZMO_INPUT_SRC) $(TEST_LIBS) -o $(GIZMO_INPUT_BIN)

# ── 多选 + 撤销演示 ──
UNDO_DEMO_SRC   := $(wildcard tests/undoDemo/*.cpp)
UNDO_DEMO_BIN   := build/undoDemo
UNDO_DEMO_FLAGS := -O2 -std=c++11 -fpermissive $(DEFINES) $(INCLUDES)

.PHONY: undoDemo
undoDemo: $(UNDO_DEMO_BIN)
	@echo "$(YELLOW)↩️  渲染多选/撤销演示...$(RESET)"
	@LD_LIBRARY_PATH=.:$$LD_LIBRARY_PATH ./$(UNDO_DEMO_BIN) --out $(GIZMO_VIDEO_OUT)/undo-frames \
		--frames 360 --size $(GIZMO_VIDEO_SIZE)
	@$(FFMPEG) -y -loglevel error -framerate 30 -i $(GIZMO_VIDEO_OUT)/undo-frames/frame_%05d.png \
		-c:v libx264 -preset slow -crf 20 -pix_fmt yuv420p -movflags +faststart \
		$(GIZMO_VIDEO_OUT)/undo-multiselect.mp4
	@ls -lh $(GIZMO_VIDEO_OUT)/undo-multiselect.mp4

$(UNDO_DEMO_BIN): $(UNDO_DEMO_SRC) $(TARGET)
	@echo "$(YELLOW)↩️  编译多选/撤销演示...$(RESET)"
	@mkdir -p build
	$(CXX) $(UNDO_DEMO_FLAGS) $(UNDO_DEMO_SRC) $(TEST_LIBS) -o $(UNDO_DEMO_BIN)

# ── 两骨 IK 演示（C3b）──
IKDEMO_SRC   := $(wildcard tests/ikDemo/*.cpp)
IKDEMO_BIN   := build/ikDemo
IKDEMO_FLAGS := -O2 -std=c++11 -fpermissive $(DEFINES) $(INCLUDES) -Itests/common

.PHONY: ikDemo
ikDemo: $(IKDEMO_BIN)
	@echo "$(YELLOW)🦵 渲染两骨 IK 演示...$(RESET)"
	@LD_LIBRARY_PATH=.:$$LD_LIBRARY_PATH ./$(IKDEMO_BIN) --out $(GIZMO_VIDEO_OUT)/ik-frames \
		--frames 360 --size $(GIZMO_VIDEO_SIZE)
	@$(FFMPEG) -y -loglevel error -framerate 30 -i $(GIZMO_VIDEO_OUT)/ik-frames/frame_%05d.png \
		-c:v libx264 -preset slow -crf 20 -pix_fmt yuv420p -movflags +faststart \
		$(GIZMO_VIDEO_OUT)/two-bone-ik.mp4
	@ls -lh $(GIZMO_VIDEO_OUT)/two-bone-ik.mp4

$(IKDEMO_BIN): $(IKDEMO_SRC) $(wildcard tests/common/*.h) $(TARGET)
	@echo "$(YELLOW)🦵 编译两骨 IK 演示...$(RESET)"
	@mkdir -p build
	$(CXX) $(IKDEMO_FLAGS) $(IKDEMO_SRC) $(TEST_LIBS) -o $(IKDEMO_BIN)

# ── Root Motion 演示（C3）──
ROOTMOTION_SRC   := $(wildcard tests/rootMotion/*.cpp)
ROOTMOTION_BIN   := build/rootMotion
ROOTMOTION_FLAGS := -O2 -std=c++11 -fpermissive $(DEFINES) $(INCLUDES) -Itests/common

.PHONY: rootMotion
rootMotion: $(ROOTMOTION_BIN)
	@echo "$(YELLOW)🦶 渲染 Root Motion 演示...$(RESET)"
	@LD_LIBRARY_PATH=.:$$LD_LIBRARY_PATH ./$(ROOTMOTION_BIN) --out $(GIZMO_VIDEO_OUT)/root-frames \
		--frames 360 --size $(GIZMO_VIDEO_SIZE)
	@$(FFMPEG) -y -loglevel error -framerate 30 -i $(GIZMO_VIDEO_OUT)/root-frames/frame_%05d.png \
		-c:v libx264 -preset slow -crf 20 -pix_fmt yuv420p -movflags +faststart \
		$(GIZMO_VIDEO_OUT)/root-motion.mp4
	@ls -lh $(GIZMO_VIDEO_OUT)/root-motion.mp4

$(ROOTMOTION_BIN): $(ROOTMOTION_SRC) $(wildcard tests/common/*.h) $(TARGET)
	@echo "$(YELLOW)🦶 编译 Root Motion 演示...$(RESET)"
	@mkdir -p build
	$(CXX) $(ROOTMOTION_FLAGS) $(ROOTMOTION_SRC) $(TEST_LIBS) -o $(ROOTMOTION_BIN)

# ── 混合树 / 动画层演示（D3）──
ANIMBLEND_SRC   := $(wildcard tests/animBlend/*.cpp)
ANIMBLEND_BIN   := build/animBlend
ANIMBLEND_FLAGS := -O2 -std=c++11 -fpermissive $(DEFINES) $(INCLUDES) -Itests/common

.PHONY: animBlend
animBlend: $(ANIMBLEND_BIN)
	@echo "$(YELLOW)🎭 渲染混合树/动画层演示...$(RESET)"
	@LD_LIBRARY_PATH=.:$$LD_LIBRARY_PATH ./$(ANIMBLEND_BIN) --out $(GIZMO_VIDEO_OUT)/blend-frames \
		--frames 300 --size $(GIZMO_VIDEO_SIZE)
	@$(FFMPEG) -y -loglevel error -framerate 30 -i $(GIZMO_VIDEO_OUT)/blend-frames/frame_%05d.png \
		-c:v libx264 -preset slow -crf 20 -pix_fmt yuv420p -movflags +faststart \
		$(GIZMO_VIDEO_OUT)/anim-blend.mp4
	@ls -lh $(GIZMO_VIDEO_OUT)/anim-blend.mp4

$(ANIMBLEND_BIN): $(ANIMBLEND_SRC) $(wildcard tests/common/*.h) $(TARGET)
	@echo "$(YELLOW)🎭 编译混合树/动画层演示...$(RESET)"
	@mkdir -p build
	$(CXX) $(ANIMBLEND_FLAGS) $(ANIMBLEND_SRC) $(TEST_LIBS) -o $(ANIMBLEND_BIN)

# ── 角色推体演示（C2）──
CHAR_PUSH_SRC   := $(wildcard tests/characterPush/*.cpp)
CHAR_PUSH_BIN   := build/characterPush
CHAR_PUSH_FLAGS := -O2 -std=c++11 -fpermissive $(DEFINES) $(INCLUDES)

.PHONY: characterPush
characterPush: $(CHAR_PUSH_BIN)
	@echo "$(YELLOW)🏃 渲染角色推体演示...$(RESET)"
	@LD_LIBRARY_PATH=.:$$LD_LIBRARY_PATH ./$(CHAR_PUSH_BIN) --out $(GIZMO_VIDEO_OUT)/push-frames \
		--frames 300 --size $(GIZMO_VIDEO_SIZE)
	@$(FFMPEG) -y -loglevel error -framerate 30 -i $(GIZMO_VIDEO_OUT)/push-frames/frame_%05d.png \
		-c:v libx264 -preset slow -crf 20 -pix_fmt yuv420p -movflags +faststart \
		$(GIZMO_VIDEO_OUT)/character-push.mp4
	@ls -lh $(GIZMO_VIDEO_OUT)/character-push.mp4

$(CHAR_PUSH_BIN): $(CHAR_PUSH_SRC) $(TARGET)
	@echo "$(YELLOW)🏃 编译角色推体演示...$(RESET)"
	@mkdir -p build
	$(CXX) $(CHAR_PUSH_FLAGS) $(CHAR_PUSH_SRC) $(TEST_LIBS) -o $(CHAR_PUSH_BIN)

# ── 示例无窗口冒烟（所有示例在 GN_HEADLESS_FRAMES 下跑固定帧数）──
# 依赖只取「编译」产物：不能依赖 headlessCheck/gpuCheck 这些 .PHONY 运行目标，
# 否则它们会先以窗口模式跑一次（无显示时 SKIP → make 直接失败）。
# 没有 Vulkan 设备时整体 SKIP（退出码 2）。
.PHONY: smoke
smoke: $(TARGET) $(HEADLESS_BIN) $(GPU_CHECK_BIN) cornellBox mmdViewer imoScene physicsBoxes characterDemo $(ROOTMOTION_BIN) $(ANIMBLEND_BIN) $(IKDEMO_BIN)
	@echo "$(YELLOW)🚦 示例无窗口冒烟...$(RESET)"
	@GN_HEADLESS_FRAMES=$(if $(SMOKE_FRAMES),$(SMOKE_FRAMES),8) \
		LD_LIBRARY_PATH=.:$$LD_LIBRARY_PATH bash tests/smoke/runSamples.sh

# ── CI 一条命令：构建 + 单测 + 无窗口渲染验收 + 示例冒烟 ──
# 与 check 的区别：check 只跑纯逻辑单测（不需要 GPU）；ci 额外验真实渲染。
.PHONY: ci
ci: check smoke doc-check spv-check
	@echo "$(GREEN)✅ ci 通过: 构建 + 单测 + 物理场景 + 无窗口渲染 + 示例冒烟 + 文档数字 + 内嵌 SPIR-V 一致$(RESET)"

# ── 分层依赖检查（只报告；架构重组完成后改 --strict 进 make ci）──
.PHONY: lint
lint:
	@echo "$(YELLOW)🧭 分层依赖检查...$(RESET)"
	@bash tests/lint/layers.sh $(LINT_FLAGS)
	@echo "$(YELLOW)📢 输出通道检查...$(RESET)"
	@bash tests/lint/output.sh
	@echo "$(YELLOW)🗂  GN_VkContext 分组检查...$(RESET)"
	@bash tests/lint/context_groups.sh
	@echo "$(YELLOW)⌨  示例输入访问面检查（P0：只准走动作映射）...$(RESET)"
	@bash tests/lint/input.sh

# ── 文档数字：唯一来源是实测。改数字不要手改，跑 make stats ──
.PHONY: stats doc-check
# **stats 必须先构建**：gen_stats.sh 会**实际运行** unit 与 gpuCheck 来取数字。
# 用旧二进制跑出来的数字与源码不符，doc-check 就会失败，而"失败原因"看起来
# 像是文档没更新 —— 实际上是构建产物过期。这里显式依赖，把这个坑堵掉。
stats: $(TARGET) build/gpuCheck
	@echo "$(YELLOW)📊 生成文档统计数字...$(RESET)"
	@bash tests/tools/gen_stats.sh --write

doc-check:
	@echo "$(YELLOW)🔍 校验文档统计数字是否过时...$(RESET)"
	@bash tests/tools/gen_stats.sh

# ── 交叉平台编译检查（只编译不链接）──
# 用 MinGW-w64 把 src/ 全量按 Windows 目标编一遍，确保 Vulkan 后端不只"在 Linux 上能编"。
# 没有交叉工具链时退出码 2 = SKIP（没装 g++-mingw-w64-x86-64 就跳过）。
.PHONY: crosscheck
crosscheck:
	@echo "$(YELLOW)🌐 交叉编译检查（$(if $(CROSS),$(CROSS),x86_64-w64-mingw32-))$(RESET)"
	@JOBS=$(if $(JOBS),$(JOBS),2) bash tests/crosscompile/checkCross.sh

# ── MMD 查看器（编译不依赖 GPU；运行需要 Vulkan 设备与窗口）────────
MMD_VIEWER_SRC := $(wildcard tests/mmdViewer/*.cpp)
MMD_VIEWER_BIN := build/mmdViewer
MMD_VIEWER_FLAGS := -O2 -std=c++11 -fpermissive $(DEFINES) $(INCLUDES)

.PHONY: mmdViewer
mmdViewer: $(TARGET)
	@echo "$(YELLOW)🖼  编译 MMD 查看器...$(RESET)"
	@mkdir -p build
	$(CXX) $(MMD_VIEWER_FLAGS) $(MMD_VIEWER_SRC) -L. -l$(LIB_STEM) $(LIBS) -o $(MMD_VIEWER_BIN)
	@echo "$(GREEN)✅ 运行: LD_LIBRARY_PATH=. ./$(MMD_VIEWER_BIN)$(RESET)"

# ── imo_chan 场景（glTF 角色 + 地面/天空/相机；编译不依赖 GPU）──────────
IMO_SCENE_SRC   := $(wildcard tests/imoScene/*.cpp)
IMO_SCENE_BIN   := build/imoScene
IMO_SCENE_FLAGS := -O2 -std=c++11 -fpermissive $(DEFINES) $(INCLUDES)
# rpath=$ORIGIN/..：从 build/ 运行时直接找到仓库根的 libGN_mini.so，不必再设 LD_LIBRARY_PATH。
IMO_SCENE_RPATH := -Wl,-rpath,'$$ORIGIN/..'
# 转换输入/输出：源 FBX 在提取资源盘，GLB 落在本机状态目录（.local/ 不入库）。
# 默认转换“穿好衣服”的 Clothes_imo_chan（imo_chan 根带的是 inactive 的 kemo 裸体变体）。
IMO_SCENE_FBX   ?= /run/media/ruicat/9294E1A394E18A55/游戏/unity-extracted/01_新生活は妹ちゃんと_v1.8/models/Clothes_imo_chan/Clothes_imo_chan.fbx
IMO_SCENE_ASSET ?= .local/imoScene/clothes_imo_chan.glb

.PHONY: imoScene
imoScene: $(TARGET)
	@echo "$(YELLOW)🎬 编译 imo_chan 场景...$(RESET)"
	@mkdir -p build
	$(CXX) $(IMO_SCENE_FLAGS) $(IMO_SCENE_SRC) -L. -l$(LIB_STEM) $(LIBS) $(IMO_SCENE_RPATH) \
		-o $(IMO_SCENE_BIN)
	@echo "$(GREEN)✅ 运行: ./$(IMO_SCENE_BIN)$(RESET)"

.PHONY: imoSceneAsset
imoSceneAsset:
	@command -v blender >/dev/null 2>&1 || (echo "$(YELLOW)缺少 Blender，无法转换 FBX → GLB$(RESET)"; exit 1)
	@echo "$(YELLOW)🧩 FBX → GLB: $(IMO_SCENE_FBX)$(RESET)"
	blender -b --factory-startup --python tools/assetconvert/fbx2glb.py -- \
		"$(IMO_SCENE_FBX)" "$(IMO_SCENE_ASSET)"
	@echo "$(GREEN)✅ 资产: $(IMO_SCENE_ASSET)$(RESET)"

# 场景（房间背景）：从游戏数据里把 level1 的可见静态网格按材质合并成 GLB。
# 依赖外部提取管线（UnityPy venv + Blender），数据在提取资源盘、产物仍落 .local/。
IMO_UNITY_EXTRACT ?= /run/media/ruicat/9294E1A394E18A55/游戏/.unity-extract
IMO_ROOM_DATA     ?= /run/media/ruicat/9294E1A394E18A55/游戏/新生活は妹ちゃんと!ver1.8/新生活は妹ちゃんと!_Data
IMO_ROOM_PAYLOAD  ?= .local/imoScene/room
IMO_ROOM_ASSET    ?= .local/imoScene/room.glb

.PHONY: imoRoomAsset
imoRoomAsset:
	@command -v blender >/dev/null 2>&1 || (echo "$(YELLOW)缺少 Blender，无法装配场景 GLB$(RESET)"; exit 1)
	@test -x "$(IMO_UNITY_EXTRACT)/venv/bin/python" || (echo "$(YELLOW)缺少提取管线 venv: $(IMO_UNITY_EXTRACT)/venv$(RESET)"; exit 1)
	@echo "$(YELLOW)🧩 场景 → 中间产物: $(IMO_ROOM_DATA)$(RESET)"
	"$(IMO_UNITY_EXTRACT)/venv/bin/python" "$(IMO_UNITY_EXTRACT)/pipeline/sceneExport.py" \
		"$(IMO_ROOM_DATA)" "$(IMO_ROOM_PAYLOAD)"
	@echo "$(YELLOW)🧩 中间产物 → GLB$(RESET)"
	blender -b --factory-startup --python "$(IMO_UNITY_EXTRACT)/pipeline/sceneBlender.py" -- \
		"$(IMO_ROOM_PAYLOAD)" "$(IMO_ROOM_ASSET)"
	@echo "$(GREEN)✅ 场景资产: $(IMO_ROOM_ASSET)$(RESET)"

# 一次生成两份资产（角色 + 房间）。
.PHONY: imoSceneAssets
imoSceneAssets: imoSceneAsset imoRoomAsset

# ── Cornell Box 测试程序（独立场景/相机；编译不依赖 GPU）──────────
CORNELL_SRC   := tests/cornellBox/main.cpp tests/cornellBox/cornellScene.cpp \
                 tests/cornellBox/cornellCamera.cpp tests/cornellBox/cornellHud.cpp \
                 tests/cornellBox/cornellGeometry.cpp tests/cornellBox/cornellCapture.cpp \
                 tests/cornellBox/cornellCpuRt.cpp tests/cornellBox/cornellProbeBake.cpp \
                 tests/cornellBox/cornellGpuRt.cpp \
                 tests/cornellBox/cornellRtAs.cpp tests/cornellBox/cornellRtDevice.cpp
CORNELL_BIN   := build/cornellBox
CORNELL_FLAGS := -O2 -std=c++11 -pthread -fpermissive $(DEFINES) $(INCLUDES)
CORNELL_SHADERS := assets/shaders/gn_cornell_rt.comp.spv assets/shaders/gn_cornell_rq.comp.spv

assets/shaders/gn_cornell_rt.comp.spv: assets/shaders/gn_cornell_rt.comp assets/shaders/cornellRayLib.glsl
	@echo "$(YELLOW)🔧 编译 Cornell compute RT 着色器...$(RESET)"
	glslangValidator -V -Iassets/shaders $< -o $@

assets/shaders/gn_cornell_rq.comp.spv: assets/shaders/gn_cornell_rq.comp
	@echo "$(YELLOW)🔧 编译 Cornell ray-query 着色器...$(RESET)"
	glslangValidator -V --target-env vulkan1.2 $< -o $@

.PHONY: cornellBox
cornellBox: $(TARGET) $(CORNELL_SHADERS)
	@echo "$(YELLOW)🧪 编译 Cornell Box 测试程序...$(RESET)"
	@mkdir -p build
	$(CXX) $(CORNELL_FLAGS) $(CORNELL_SRC) -L. -l$(LIB_STEM) $(LIBS) -o $(CORNELL_BIN)
	@echo "$(GREEN)✅ 运行: LD_LIBRARY_PATH=. ./$(CORNELL_BIN)$(RESET)"

# ── Cornell Box CPU 路径追踪参考（纯 CPU，不需要 Vulkan 设备）──────
CORNELL_PT_SRC := tests/cornellBox/cornellGeometry.cpp \
                  tests/cornellBox/cornellPathTracer.cpp \
                  tests/cornellBox/cornellPtMain.cpp
CORNELL_PT_BIN := build/cornellPt
CORNELL_PT_FLAGS := -O3 -std=c++11 -pthread -Itests/cornellBox

.PHONY: cornellPt
cornellPt:
	@echo "$(YELLOW)🧪 编译 Cornell Box 路径追踪参考...$(RESET)"
	@mkdir -p build
	$(CXX) $(CORNELL_PT_FLAGS) $(CORNELL_PT_SRC) -lpng -lz -o $(CORNELL_PT_BIN)
	@echo "$(GREEN)✅ 运行: ./$(CORNELL_PT_BIN) --spp=64 --out=/tmp/cornell-pt.png$(RESET)"

# ── Cornell Box 图像误差对比（PT vs 引擎截图）────────────────────
CORNELL_COMPARE_SRC := tests/cornellBox/cornellCompare.cpp
CORNELL_COMPARE_BIN := build/cornellCompare

.PHONY: cornellCompare
cornellCompare:
	@echo "$(YELLOW)🧪 编译 Cornell Box 误差对比工具...$(RESET)"
	@mkdir -p build
	$(CXX) -O2 -std=c++11 $(CORNELL_COMPARE_SRC) -lpng -lz -o $(CORNELL_COMPARE_BIN)
	@echo "$(GREEN)✅ 运行: ./$(CORNELL_COMPARE_BIN) --ref=pt.png --test=engine.png$(RESET)"

# ── Cornell CPU 软件光追预览自检（纯 CPU）──────────────────────────
CORNELL_RT_PROBE_SRC := tests/cornellBox/cornellGeometry.cpp \
                        tests/cornellBox/cornellCpuRt.cpp \
                        tests/cornellBox/cornellCpuRtProbe.cpp
CORNELL_RT_PROBE_BIN := build/cornellRtProbe

.PHONY: cornellRtProbe
cornellRtProbe:
	@echo "$(YELLOW)🧪 编译 Cornell CPU 光追自检...$(RESET)"
	@mkdir -p build
	$(CXX) -O2 -std=c++11 -pthread $(CORNELL_RT_PROBE_SRC) -lpng -lz -o $(CORNELL_RT_PROBE_BIN)
	@echo "$(GREEN)✅ 运行: ./$(CORNELL_RT_PROBE_BIN)$(RESET)"

# ── Cornell GPU compute 光追自检（无窗口 Vulkan；无设备时 SKIP）──────
CORNELL_GPU_RT_PROBE_SRC := tests/cornellBox/cornellGeometry.cpp \
                            tests/cornellBox/cornellCpuRt.cpp \
                            tests/cornellBox/cornellGpuRt.cpp \
                            tests/cornellBox/cornellRtAs.cpp \
                            tests/cornellBox/cornellGpuRtProbe.cpp
CORNELL_GPU_RT_PROBE_BIN := build/cornellGpuRtProbe

.PHONY: cornellGpuRtProbe
cornellGpuRtProbe: $(CORNELL_SHADERS)
	@echo "$(YELLOW)🧪 编译 Cornell GPU 光追自检...$(RESET)"
	@mkdir -p build
	$(CXX) -O2 -std=c++11 -pthread $(CORNELL_GPU_RT_PROBE_SRC) -lvulkan -lpng -lz -o $(CORNELL_GPU_RT_PROBE_BIN)
	@echo "$(GREEN)✅ 运行: ./$(CORNELL_GPU_RT_PROBE_BIN)$(RESET)"

# ── Cornell 硬件 ray-query 自检（无窗口 Vulkan；无设备时 SKIP）──────
CORNELL_HW_RT_PROBE_SRC := tests/cornellBox/cornellGeometry.cpp \
                           tests/cornellBox/cornellCpuRt.cpp \
                           tests/cornellBox/cornellGpuRt.cpp \
                           tests/cornellBox/cornellRtAs.cpp \
                           tests/cornellBox/cornellRtDevice.cpp \
                           tests/cornellBox/cornellHwRtProbe.cpp
CORNELL_HW_RT_PROBE_BIN := build/cornellHwRtProbe

.PHONY: cornellHwRtProbe
cornellHwRtProbe: $(CORNELL_SHADERS)
	@echo "$(YELLOW)🧪 编译 Cornell 硬件光追自检...$(RESET)"
	@mkdir -p build
	$(CXX) -O2 -std=c++11 -pthread $(CORNELL_HW_RT_PROBE_SRC) -lvulkan -lpng -lz -o $(CORNELL_HW_RT_PROBE_BIN)
	@echo "$(GREEN)✅ 运行: ./$(CORNELL_HW_RT_PROBE_BIN)$(RESET)"

# ── Cornell Box 自检：确定性 + 位姿/种子敏感性（纯 CPU，可进 CI）────
.PHONY: cornellCheck
cornellCheck: cornellPt cornellCompare
	@echo "$(YELLOW)🧪 Cornell PT 自检...$(RESET)"
	@mkdir -p .local/cornellbox
	./build/cornellPt --width=160 --height=100 --spp=8 --pose=0 --seed=7 \
		--out=.local/cornellbox/check-a.png
	./build/cornellPt --width=160 --height=100 --spp=8 --pose=0 --seed=7 \
		--out=.local/cornellbox/check-b.png
	./build/cornellPt --width=160 --height=100 --spp=8 --pose=0 --seed=8 \
		--out=.local/cornellbox/check-c.png
	./build/cornellPt --width=160 --height=100 --spp=8 --pose=1 --seed=7 \
		--out=.local/cornellbox/check-p1.png
	@./build/cornellCompare --ref=.local/cornellbox/check-a.png \
		--test=.local/cornellbox/check-b.png > /tmp/cornell-self.txt
	@grep -q 'RMSE=0.000' /tmp/cornell-self.txt || { cat /tmp/cornell-self.txt; exit 1; }
	@./build/cornellCompare --ref=.local/cornellbox/check-a.png \
		--test=.local/cornellbox/check-c.png > /tmp/cornell-seed.txt
	@if grep -q 'RMSE=0.000' /tmp/cornell-seed.txt; then \
		echo "FAIL: 不同 seed 输出相同"; cat /tmp/cornell-seed.txt; exit 1; fi
	@./build/cornellCompare --ref=.local/cornellbox/check-a.png \
		--test=.local/cornellbox/check-p1.png > /tmp/cornell-pose.txt
	@if grep -q 'RMSE=0.000' /tmp/cornell-pose.txt; then \
		echo "FAIL: 不同位姿输出相同"; cat /tmp/cornell-pose.txt; exit 1; fi
	@echo "$(GREEN)✅ cornellCheck PASS（同 seed 一致；不同 seed/位姿可区分）$(RESET)"

# ── ASan/UBSan 单元测试（独立构建目录与库名，不污染 release 产物）──
.PHONY: asan
asan:
	@echo "$(YELLOW)🧪 ASan/UBSan 构建 + 单元测试...$(RESET)"
	$(MAKE) BUILD_DIR=build/obj-asan LIB_STEM=GN_mini_asan \
		CXXFLAGS_BASE="-g -O1 -std=c++11 -fpermissive -fPIC -Wall -Wextra -fsanitize=address,undefined -fno-omit-frame-pointer" \
		LDFLAGS="-shared -fsanitize=address,undefined" \
		libGN_mini_asan.$(TARGET_EXT)
	@mkdir -p build
	$(CXX) $(UNIT_FLAGS) -g -fsanitize=address,undefined -fno-omit-frame-pointer \
		$(UNIT_SRC) -L. -lGN_mini_asan $(LIBS) -o build/unit_tests_asan
	@LD_LIBRARY_PATH=. ./build/unit_tests_asan

# ═══════════════════════════════════════════════════════════
#  6. 调试版本
# ═══════════════════════════════════════════════════════════
.PHONY: debug
debug: CXXFLAGS_BASE := -g -O0 -std=c++11 -fpermissive -fPIC -Wall -Wextra
debug: DEFINES += -DGN_DEBUG
debug: clean all
	@echo "$(YELLOW)🐛 调试版本完成 [$(PLATFORM)]$(RESET)"

# ═══════════════════════════════════════════════════════════
#  7. 清理
# ═══════════════════════════════════════════════════════════
.PHONY: clean
clean:
	@echo "$(YELLOW)🧹 清理编译产物...$(RESET)"
	rm -rf $(BUILD_DIR) build/obj-asan build/cross-* libGN_mini*.* build/test_* build/unit_tests* build/gpuCheck build/headlessCheck build/mmdViewer build/imoScene build/cornellBox build/cornellPt build/cornellCompare build/cornellRtProbe build/cornellGpuRtProbe build/cornellHwRtProbe
	@echo "$(GREEN)✅ 清理完成$(RESET)"

# ═══════════════════════════════════════════════════════════
#  8. 着色器 (GLSL → SPIR-V)
# ═══════════════════════════════════════════════════════════
SHADER_DIR  := assets/shaders
SHADER_SRC  := $(filter-out $(SHADER_DIR)/gn_cornell_rq.comp,$(wildcard $(SHADER_DIR)/*.vert) $(wildcard $(SHADER_DIR)/*.frag) $(wildcard $(SHADER_DIR)/*.comp))
SHADER_OUT  := $(SHADER_SRC:=.spv)

.PHONY: shaders
shaders:
	@echo "$(YELLOW)🔧 编译 GLSL → SPIR-V...$(RESET)"
	@c=0; for f in $(SHADER_SRC); do \
		glslangValidator -V -I$(SHADER_DIR) "$$f" -o "$$f.spv" 2>/dev/null && c=$$((c+1)); \
	done; \
	glslangValidator -V --target-env vulkan1.2 $(SHADER_DIR)/gn_cornell_rq.comp -o $(SHADER_DIR)/gn_cornell_rq.comp.spv 2>/dev/null && c=$$((c+1)); \
	echo "$(GREEN)✅ $$c 个着色器$(RESET)"
	@$(MAKE) --no-print-directory embed-spv

# ── 内嵌 SPIR-V 数组同步（必须跟 make shaders 一起跑）──
#
# 引擎运行时**优先**读磁盘 assets/shaders/*.spv，读不到才退回内嵌数组（src/graphics/_Vulkan/ 下的 *_spv.h）。
# 内嵌数组曾经是"一次性手工生成"的：改了 .glsl → make shaders 更新了 .spv →
# 程序却仍在跑旧的内嵌代码。表现是"改了完全没效果"，排查了整整一轮
# （docs/CHANGELOG.md 的 cube shadow 条目有完整记录）。
#
# 现在：make shaders 末尾自动同步；make spv-check 在 CI 里当门禁。
.PHONY: embed-spv spv-check
embed-spv:
	@echo "$(YELLOW)📦 同步内嵌 SPIR-V 数组...$(RESET)"
	@python3 tools/embedSpv.py --write

spv-check:
	@python3 tools/embedSpv.py --check

# ═══════════════════════════════════════════════════════════
#  9. 帮助
# ═══════════════════════════════════════════════════════════
.PHONY: help
help:
	@echo "$(BLUE)╔══════════════════════════════════════════════════╗"
	@echo "║   GN_mini — Cross-Platform Makefile             ║"
	@echo "╠══════════════════════════════════════════════════╣"
	@echo "║                                                  ║"
	@echo "║  🎯 目标平台选择                                 ║"
	@echo "║    make                     自动检测 → $(PLATFORM)$(shell printf '%*s' $$((24 - $(shell printf '%s' $(PLATFORM) | wc -c))) '')║"
	@echo "║    make PLATFORM=linux      Linux (Vulkan+GLFW)  ║"
	@echo "║    make PLATFORM=macos      macOS (GLUT+GL)      ║"
	@echo "║    make PLATFORM=windows    Win (MinGW-w64)      ║"
	@echo "║    make PLATFORM=android    Android (NDK)        ║"
	@echo "║                                                  ║"
	@echo "║  🔧 交叉编译                                     ║"
	@echo "║    make PLATFORM=windows CROSS=i686-w64-mingw32- ║"
	@echo "║    make PLATFORM=android CROSS=armv7a-linux-     ║"
	@echo "║                                                  ║"
	@echo "║  📦 构建目标                                     ║"
	@echo "║    make / make all          编译引擎库           ║"
	@echo "║    make debug               调试版本 (-g -O0)    ║"
	@echo "║    make unit                编译并运行单元测试   ║"
	@echo "║    make asan                ASan/UBSan 单测      ║"
	@echo "║    make check               构建+单元测试        ║"
	@echo "║    make headlessCheck       无窗口渲染验收       ║"
	@echo "║    make smoke               示例无窗口冒烟       ║"
	@echo "║    make ci                  check + smoke 全跑   ║"
	@echo "║    make gpuCheck            P1b GPU 管线验收     ║"
	@echo "║    make crosscheck          Windows 交叉编译检查 ║"
	@echo "║    make lint                分层依赖检查           ║"
	@echo "║    make stats               生成文档统计数字       ║"
	@echo "║    make doc-check           校验文档数字是否过时   ║"
	@echo "║    make cornellBox          Cornell Box 测试程序 ║"
	@echo "║    make cornellPt           Cornell CPU 路径追踪 ║"
	@echo "║    make cornellCompare      Cornell 误差对比     ║"
	@echo "║    make cornellRtProbe      CPU 光追预览自检     ║"
	@echo "║    make cornellCheck        Cornell PT 自检      ║"
	@echo "║    make imoScene            imo_chan 角色场景    ║"
	@echo "║    make imoSceneAsset       FBX → GLB 资产转换   ║"
	@echo "║    make shaders             编译 GLSL → SPIR-V   ║"
	@echo "║    make clean               清理                 ║"
	@echo "║    make help                此帮助               ║"
	@echo "║                                                  ║"
	@echo "╚══════════════════════════════════════════════════╝$(RESET)"

# ═══════════════════════════════════════════════════════════
#  9. 平台信息
# ═══════════════════════════════════════════════════════════
.PHONY: info
info:
	@echo "$(BLUE)当前平台配置:$(RESET)"
	@echo "  PLATFORM    = $(PLATFORM)"
	@echo "  CROSS       = $(CROSS)"
	@echo "  CXX         = $(CXX)"
	@echo "  TARGET      = $(TARGET)"
	@echo "  DEFINES     = $(DEFINES)"
	@echo "  LIBS        = $(LIBS)"
	@echo "  LDFLAGS     = $(LDFLAGS)"

# ═══════════════════════════════════════════════════════════
#  10. 依赖自动生成 (合并入上方编译规则)
# ═══════════════════════════════════════════════════════════
DEPFLAGS   = -MT $@ -MMD -MP -MF $(BUILD_DIR)/$*.d
COMPILE.cc  = $(CXX) $(DEPFLAGS) $(CXXFLAGS) $(DEFINES) $(INCLUDES) -c $< -o $@

-include $(patsubst src/%.cpp, $(BUILD_DIR)/%.d, $(ENGINE_SRC))
