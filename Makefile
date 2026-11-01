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
UNIT_SRC   := $(wildcard $(UNIT_DIR)/*.cpp)
UNIT_BIN   := build/unit_tests
UNIT_FLAGS := -O2 -std=c++11 -fpermissive $(DEFINES) -I$(UNIT_DIR) $(INCLUDES)

.PHONY: unit
unit: $(TARGET)
	@echo "$(YELLOW)🧪 编译并运行单元测试...$(RESET)"
	@mkdir -p build
	$(CXX) $(UNIT_FLAGS) $(UNIT_SRC) $(TEST_LIBS) -o $(UNIT_BIN)
	@LD_LIBRARY_PATH=. ./$(UNIT_BIN)

.PHONY: check
check: all unit
	@echo "$(GREEN)✅ check 通过: 构建 + 单元测试$(RESET)"

# ── P1b GPU 验收（需要 Vulkan 设备与可见窗口；无 GPU 时退出码 2 = SKIP）──
GPU_CHECK_SRC   := $(wildcard tests/gpu/*.cpp)
GPU_CHECK_BIN   := build/gpuCheck
GPU_CHECK_FLAGS := -O2 -std=c++11 -fpermissive $(DEFINES) $(INCLUDES)

.PHONY: gpuCheck
gpuCheck: $(TARGET)
	@echo "$(YELLOW)🖥  编译 GPU 管线验收程序...$(RESET)"
	@mkdir -p build
	$(CXX) $(GPU_CHECK_FLAGS) $(GPU_CHECK_SRC) $(TEST_LIBS) -o $(GPU_CHECK_BIN)
	@echo "$(YELLOW)▶ 运行 GPU 验收（无 /dev/dri 时会 SKIP）...$(RESET)"
	@LD_LIBRARY_PATH=. ./$(GPU_CHECK_BIN)

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
	rm -rf $(BUILD_DIR) build/obj-asan libGN_mini*.* build/test_* build/unit_tests* build/gpuCheck
	@echo "$(GREEN)✅ 清理完成$(RESET)"

# ═══════════════════════════════════════════════════════════
#  8. 着色器 (GLSL → SPIR-V)
# ═══════════════════════════════════════════════════════════
SHADER_DIR  := assets/shaders
SHADER_SRC  := $(wildcard $(SHADER_DIR)/*.vert) $(wildcard $(SHADER_DIR)/*.frag) $(wildcard $(SHADER_DIR)/*.comp)
SHADER_OUT  := $(SHADER_SRC:=.spv)

.PHONY: shaders
shaders:
	@echo "$(YELLOW)🔧 编译 GLSL → SPIR-V...$(RESET)"
	@c=0; for f in $(SHADER_SRC); do \
		glslangValidator -V "$$f" -o "$$f.spv" 2>/dev/null && c=$$((c+1)); \
	done; echo "$(GREEN)✅ $$c 个着色器$(RESET)"

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
	@echo "║    make gpuCheck            P1b GPU 管线验收     ║"
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
