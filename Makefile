# GN_mini — make 薄封装（B8，2026-11-27）
#
# 构建定义唯一来源：CMakeLists.txt + CMakePresets.json。本文件只保留「目标名 → CMake 目标」
# 的薄映射与运行/门禁脚本（unit/smoke/ci/lint/doc-check/stats/shaders），不再手写编译规则。
#
# 官方平台：Linux/Vulkan（D3）。experimental 交叉平台（Windows/Android/macOS 未验证）仍用
# 旧规则文件：make -f Makefile.legacy PLATFORM=windows CROSS=...（能力保留、不静默删除）。
#
# 常用：make / make unit / make check / make ci / make parity / make help

BUILD   ?= build-cmake
PRESET  ?= default
JOBS    ?= 2
CMAKE   ?= cmake
SMOKE_FRAMES ?=

GREEN   := \033[0;32m
YELLOW  := \033[0;33m
BLUE    := \033[0;34m
RESET   := \033[0m

# 每个目标：确保已配置 → 只构建所需 CMake 目标（增量）→ 运行。
define cmakeBuild
	@test -f $(BUILD)/CMakeCache.txt || $(CMAKE) --preset $(PRESET)
	@$(CMAKE) --build --preset $(PRESET) -j$(JOBS) --target $(1)
endef

.PHONY: all configure
# 默认目标：配置 + 构建全部产物（引擎库 + 单测/验收/示例），并把库软链到仓库根，
# 兼容老工作流（nm -D libGN_mini.so / LD_LIBRARY_PATH=.）。
all:
	@test -f $(BUILD)/CMakeCache.txt || $(CMAKE) --preset $(PRESET)
	@$(CMAKE) --build --preset $(PRESET) -j$(JOBS)
	@ln -sf $(BUILD)/libGN_mini.so libGN_mini.so
	@echo "$(GREEN)✅ 构建完成: $(BUILD)/libGN_mini.so（根目录软链 libGN_mini.so）$(RESET)"

configure:
	@$(CMAKE) --preset $(PRESET)

# ── 单测 / 验收 ──────────────────────────────────────────
.PHONY: unit fixtures
unit:
	$(call cmakeBuild,unit_tests)
	@echo "$(YELLOW)🧪 运行单元测试...$(RESET)"
	@./$(BUILD)/unit_tests

fixtures:
	$(call cmakeBuild,fixtures)
	@echo "$(YELLOW)🧱 生成程序化测试资产 → .local/fixtures/...$(RESET)"
	@./$(BUILD)/fixtures .local/fixtures

.PHONY: gpuCheck headlessCheck physicsBoxes characterDemo
gpuCheck:
	$(call cmakeBuild,gpuCheck)
	@echo "$(YELLOW)▶ 运行 GPU 验收（无 /dev/dri 时会 SKIP）...$(RESET)"
	@./$(BUILD)/gpuCheck

headlessCheck:
	$(call cmakeBuild,headlessCheck)
	@echo "$(YELLOW)▶ 运行无窗口渲染验收（不需要 DISPLAY）...$(RESET)"
	@./$(BUILD)/headlessCheck
	@mkdir -p .local/tmp
	@echo "$(YELLOW)▶ 帧编排等价性（cRENDER_FRAME vs 裸 sFRAME_3D_SCOPE 逐字节比对）...$(RESET)"
	@./$(BUILD)/headlessCheck 640 400 4 .local/tmp/frame_parity.png frame; \
		rc=$$?; if [ $$rc -eq 2 ]; then echo "  [SKIP] 无 Vulkan 设备"; elif [ $$rc -ne 0 ]; then exit $$rc; fi

physicsBoxes:
	$(call cmakeBuild,physicsBoxes)
	@echo "$(YELLOW)📦 物理场景验收（20 箱堆叠 + 球撞倒，120 帧）...$(RESET)"
	@./$(BUILD)/physicsBoxes

characterDemo:
	$(call cmakeBuild,characterDemo)
	@echo "$(YELLOW)🚶 角色场景验收（台阶 + 斜坡 + 跳跃，240 帧）...$(RESET)"
	@./$(BUILD)/characterDemo

.PHONY: check smoke
check: unit physicsBoxes characterDemo
	@echo "$(GREEN)✅ check 通过: 构建 + 单元测试 + 物理场景 + 角色场景$(RESET)"

# 闪烟：构建全部示例后由 runSamples.sh 统一跑（GN_BUILD_DIR 指向 CMake 产物目录）。
smoke:
	$(call cmakeBuild,all)
	@echo "$(YELLOW)🚦 示例无窗口冒烟...$(RESET)"
	@GN_HEADLESS_FRAMES=$(if $(SMOKE_FRAMES),$(SMOKE_FRAMES),8) GN_BUILD_DIR=$(BUILD) \
		bash tests/smoke/runSamples.sh

# ── 门禁 ─────────────────────────────────────────────────
.PHONY: ci parity lint lint-strict
ci: check smoke doc-check spv-check lint-strict
	@echo "$(GREEN)✅ ci 通过: CMake 构建 + 单测 + 物理/角色场景 + 无窗口渲染 + 示例冒烟 + 文档 + SPIR-V$(RESET)"

# B8 完成后默认构建即 CMake：parity 与 ci 同源（保留目标名兼容旧脚本/文档）。
parity: ci

lint:
	@echo "$(YELLOW)🧭 分层依赖检查...$(RESET)"
	@bash tests/lint/layers.sh $(LINT_FLAGS)
	@bash tests/lint/output.sh
	@bash tests/lint/context_groups.sh
	@bash tests/lint/input.sh

lint-strict:
	@echo "$(YELLOW)🧭 分层依赖检查（strict）...$(RESET)"
	@bash tests/lint/layers.sh --strict
	@bash tests/lint/output.sh
	@bash tests/lint/context_groups.sh
	@bash tests/lint/input.sh

.PHONY: stats doc-check
stats:
	$(call cmakeBuild,unit_tests)
	$(call cmakeBuild,gpuCheck)
	@echo "$(YELLOW)📊 生成文档统计数字...$(RESET)"
	@GN_BUILD_DIR=$(BUILD) bash tests/tools/gen_stats.sh --write

doc-check:
	@echo "$(YELLOW)🔍 校验文档统计数字是否过时...$(RESET)"
	@GN_BUILD_DIR=$(BUILD) bash tests/tools/gen_stats.sh
	@echo "$(YELLOW)🗂  校验文档索引与状态行...$(RESET)"
	@python3 tests/tools/check_docs_index.py
	@echo "$(YELLOW)💬 校验头文件注释覆盖...$(RESET)"
	@bash tests/tools/check_comments.sh

# ── 着色器（与构建系统无关）──────────────────────────────
SHADER_DIR := assets/shaders
SHADER_SRC := $(filter-out $(SHADER_DIR)/gn_cornell_rq.comp,$(wildcard $(SHADER_DIR)/*.vert) $(wildcard $(SHADER_DIR)/*.frag) $(wildcard $(SHADER_DIR)/*.comp))

.PHONY: shaders embed-spv spv-check
shaders:
	@echo "$(YELLOW)🔧 编译 GLSL → SPIR-V...$(RESET)"
	@c=0; for f in $(SHADER_SRC); do \
		glslangValidator -V -I$(SHADER_DIR) "$$f" -o "$$f.spv" 2>/dev/null && c=$$((c+1)); \
	done; \
	glslangValidator -V --target-env vulkan1.2 $(SHADER_DIR)/gn_cornell_rq.comp -o $(SHADER_DIR)/gn_cornell_rq.comp.spv 2>/dev/null && c=$$((c+1)); \
	echo "$(GREEN)✅ $$c 个着色器$(RESET)"
	@$(MAKE) --no-print-directory embed-spv

embed-spv:
	@echo "$(YELLOW)📦 同步内嵌 SPIR-V 数组...$(RESET)"
	@python3 tools/embedSpv.py --write

spv-check:
	@python3 tools/embedSpv.py --check

# ── 示例 / 演示（构建走 CMake，运行脚本保留）────────────────
VIDEO_OUT ?= .local/video
FFMPEG ?= $(shell command -v ffmpeg 2>/dev/null || echo ffmpeg)

define runVideo
	$(call cmakeBuild,$(1))
	@./$(BUILD)/$(1) --out $(VIDEO_OUT)/$(2) --frames $(3) --size $(4)
	@$(FFMPEG) -y -loglevel error -framerate 30 -i $(VIDEO_OUT)/$(2)/frame_%05d.png \
		-c:v libx264 -preset slow -crf 20 -pix_fmt yuv420p -movflags +faststart $(VIDEO_OUT)/$(5)
	@ls -lh $(VIDEO_OUT)/$(5)
endef

.PHONY: physicsVideo clothVideo fireVideo gizmoVideo gizmoInput undoDemo shadowProbe multiShadowDemo ragdollDemo ikDemo rootMotion animBlend characterPush
physicsVideo:
	$(call runVideo,physicsVideo,physics-frames,240,1280 720,physics-collision.mp4)
clothVideo:
	$(call runVideo,clothDemo,cloth-frames,300,1280 720,cloth-collision.mp4)
fireVideo:
	$(call runVideo,fireLighting,fire-frames,315,1280 720,fire-lighting.mp4)
gizmoVideo:
	$(call runVideo,editorGizmo,gizmo-frames,360,1280 720,editor-gizmo.mp4)
gizmoInput:
	$(call runVideo,gizmoInput,gizmo-input-frames,300,1280 720,gizmo-input.mp4)
undoDemo:
	$(call runVideo,undoDemo,undo-frames,360,1280 720,undo-multiselect.mp4)
multiShadowDemo:
	$(call runVideo,multiShadowDemo,multiShadow-frames,300,1280 720,multi-cube-shadow.mp4)
ragdollDemo:
	$(call runVideo,ragdollDemo,ragdoll-frames,360,1280 720,ragdoll.mp4)
ikDemo:
	$(call runVideo,ikDemo,ik-frames,360,1280 720,two-bone-ik.mp4)
rootMotion:
	$(call runVideo,rootMotion,root-frames,360,1280 720,root-motion.mp4)
animBlend:
	$(call runVideo,animBlend,blend-frames,300,1280 720,anim-blend.mp4)
characterPush:
	$(call runVideo,characterPush,push-frames,300,1280 720,character-push.mp4)

shadowProbe:
	$(call cmakeBuild,shadowProbe)
	@./$(BUILD)/shadowProbe --out $(VIDEO_OUT)/shadow-probe --verbose

.PHONY: mmdViewer imoScene
mmdViewer:
	$(call cmakeBuild,mmdViewer)
	@echo "$(GREEN)✅ 运行: ./$(BUILD)/mmdViewer$(RESET)"

imoScene:
	$(call cmakeBuild,imoScene)
	@echo "$(GREEN)✅ 运行: ./$(BUILD)/imoScene$(RESET)"

# ── Cornell 家族 ────────────────────────────────────────
CORNELL_SHADERS := assets/shaders/gn_cornell_rt.comp.spv assets/shaders/gn_cornell_rq.comp.spv

assets/shaders/gn_cornell_rt.comp.spv: assets/shaders/gn_cornell_rt.comp assets/shaders/cornellRayLib.glsl
	@glslangValidator -V -Iassets/shaders $< -o $@

assets/shaders/gn_cornell_rq.comp.spv: assets/shaders/gn_cornell_rq.comp
	@glslangValidator -V --target-env vulkan1.2 $< -o $@

.PHONY: cornellBox cornellPt cornellCompare cornellRtProbe cornellGpuRtProbe cornellHwRtProbe cornellCheck
cornellBox: $(CORNELL_SHADERS)
	$(call cmakeBuild,cornellBox)
	@echo "$(GREEN)✅ 运行: ./$(BUILD)/cornellBox$(RESET)"

cornellPt:
	$(call cmakeBuild,cornellPt)
	@echo "$(GREEN)✅ 运行: ./$(BUILD)/cornellPt --spp=64 --out=/tmp/cornell-pt.png$(RESET)"

cornellCompare:
	$(call cmakeBuild,cornellCompare)
	@echo "$(GREEN)✅ 运行: ./$(BUILD)/cornellCompare --ref=pt.png --test=engine.png$(RESET)"

cornellRtProbe:
	$(call cmakeBuild,cornellRtProbe)
	@echo "$(GREEN)✅ 运行: ./$(BUILD)/cornellRtProbe$(RESET)"

cornellGpuRtProbe: $(CORNELL_SHADERS)
	$(call cmakeBuild,cornellGpuRtProbe)
	@echo "$(GREEN)✅ 运行: ./$(BUILD)/cornellGpuRtProbe$(RESET)"

cornellHwRtProbe: $(CORNELL_SHADERS)
	$(call cmakeBuild,cornellHwRtProbe)
	@echo "$(GREEN)✅ 运行: ./$(BUILD)/cornellHwRtProbe$(RESET)"

cornellCheck: cornellPt cornellCompare
	@echo "$(YELLOW)🧪 Cornell PT 自检...$(RESET)"
	@mkdir -p .local/cornellbox
	@./$(BUILD)/cornellPt --width=160 --height=100 --spp=8 --pose=0 --seed=7 --out=.local/cornellbox/check-a.png
	@./$(BUILD)/cornellPt --width=160 --height=100 --spp=8 --pose=0 --seed=7 --out=.local/cornellbox/check-b.png
	@./$(BUILD)/cornellPt --width=160 --height=100 --spp=8 --pose=0 --seed=8 --out=.local/cornellbox/check-c.png
	@./$(BUILD)/cornellPt --width=160 --height=100 --spp=8 --pose=1 --seed=7 --out=.local/cornellbox/check-p1.png
	@./$(BUILD)/cornellCompare --ref=.local/cornellbox/check-a.png --test=.local/cornellbox/check-b.png > /tmp/cornell-self.txt
	@grep -q 'RMSE=0.000' /tmp/cornell-self.txt || { cat /tmp/cornell-self.txt; exit 1; }
	@./$(BUILD)/cornellCompare --ref=.local/cornellbox/check-a.png --test=.local/cornellbox/check-c.png > /tmp/cornell-seed.txt
	@if grep -q 'RMSE=0.000' /tmp/cornell-seed.txt; then echo "FAIL: 不同 seed 输出相同"; cat /tmp/cornell-seed.txt; exit 1; fi
	@./$(BUILD)/cornellCompare --ref=.local/cornellbox/check-a.png --test=.local/cornellbox/check-p1.png > /tmp/cornell-pose.txt
	@if grep -q 'RMSE=0.000' /tmp/cornell-pose.txt; then echo "FAIL: 不同位姿输出相同"; cat /tmp/cornell-pose.txt; exit 1; fi
	@echo "$(GREEN)✅ cornellCheck PASS（同 seed 一致；不同 seed/位姿可区分）$(RESET)"

# ── 资产转换（与构建系统无关）──────────────────────────────
IMO_SCENE_FBX   ?= /run/media/ruicat/9294E1A394E18A55/游戏/unity-extracted/01_新生活は妹ちゃんと_v1.8/models/Clothes_imo_chan/Clothes_imo_chan.fbx
IMO_SCENE_ASSET ?= .local/imoScene/clothes_imo_chan.glb
IMO_UNITY_EXTRACT ?= /run/media/ruicat/9294E1A394E18A55/游戏/.unity-extract
IMO_ROOM_DATA     ?= /run/media/ruicat/9294E1A394E18A55/游戏/新生活は妹ちゃんと!ver1.8/新生活は妹ちゃんと!_Data
IMO_ROOM_PAYLOAD  ?= .local/imoScene/room
IMO_ROOM_ASSET    ?= .local/imoScene/room.glb

.PHONY: imoSceneAsset imoRoomAsset imoSceneAssets
imoSceneAsset:
	@command -v blender >/dev/null 2>&1 || (echo "$(YELLOW)缺少 Blender，无法转换 FBX → GLB$(RESET)"; exit 1)
	@blender -b --factory-startup --python tools/assetconvert/fbx2glb.py -- "$(IMO_SCENE_FBX)" "$(IMO_SCENE_ASSET)"

imoRoomAsset:
	@command -v blender >/dev/null 2>&1 || (echo "$(YELLOW)缺少 Blender，无法装配场景 GLB$(RESET)"; exit 1)
	@test -x "$(IMO_UNITY_EXTRACT)/venv/bin/python" || (echo "$(YELLOW)缺少提取管线 venv: $(IMO_UNITY_EXTRACT)/venv$(RESET)"; exit 1)
	"$(IMO_UNITY_EXTRACT)/venv/bin/python" "$(IMO_UNITY_EXTRACT)/pipeline/sceneExport.py" "$(IMO_ROOM_DATA)" "$(IMO_ROOM_PAYLOAD)"
	blender -b --factory-startup --python "$(IMO_UNITY_EXTRACT)/pipeline/sceneBlender.py" -- "$(IMO_ROOM_PAYLOAD)" "$(IMO_ROOM_ASSET)"

imoSceneAssets: imoSceneAsset imoRoomAsset

# ── 调试 / 清理 / 实验 ───────────────────────────────────
.PHONY: debug asan crosscheck clean
debug:
	@$(CMAKE) --preset debug
	@$(CMAKE) --build --preset debug -j$(JOBS)
	@echo "$(GREEN)✅ 调试版本: build-cmake-debug/$(RESET)"

# ASan/UBSan 单测：独立构建目录，不污染 release 产物（等价旧 asan 目标）。
asan:
	@echo "$(YELLOW)🧪 ASan/UBSan 构建 + 单元测试...$(RESET)"
	@$(CMAKE) -S . -B build-asan -DCMAKE_BUILD_TYPE=RelWithDebInfo \
		-DCMAKE_CXX_FLAGS="-g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer" \
		-DCMAKE_SHARED_LINKER_FLAGS="-fsanitize=address,undefined" \
		-DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined" > /dev/null
	@$(CMAKE) --build build-asan -j$(JOBS) --target unit_tests
	@ASAN_OPTIONS=detect_leaks=0 ./build-asan/unit_tests

# 交叉平台编译检查（experimental，D3）：独立脚本，不经构建系统。
crosscheck:
	@echo "$(YELLOW)🌐 交叉编译检查（$(if $(CROSS),$(CROSS),x86_64-w64-mingw32-))$(RESET)"
	@JOBS=$(JOBS) bash tests/crosscompile/checkCross.sh

clean:
	@echo "$(YELLOW)🧹 清理编译产物...$(RESET)"
	rm -rf $(BUILD) build-cmake-debug build-asan build/obj build/obj-asan libGN_mini.so
	@echo "$(GREEN)✅ 清理完成$(RESET)"

# ── 帮助 ────────────────────────────────────────────────
.PHONY: help info
help:
	@echo "$(BLUE)GN_mini — make 薄封装（CMake 驱动，B8）$(RESET)"
	@echo ""
	@echo "  make / make all        配置 + 构建全部产物（$(BUILD)/）"
	@echo "  make unit              编译并运行单元测试"
	@echo "  make gpuCheck          GPU 管线验收（无设备 SKIP）"
	@echo "  make headlessCheck     无窗口渲染验收"
	@echo "  make physicsBoxes      物理场景验收"
	@echo "  make characterDemo     角色场景验收"
	@echo "  make check             unit + 物理 + 角色"
	@echo "  make smoke             示例无窗口冒烟（13 个示例）"
	@echo "  make ci                check + smoke + doc-check + spv-check + lint-strict"
	@echo "  make parity            ci 同源（B8 后默认构建即 CMake）"
	@echo "  make stats             重新生成文档统计数字"
	@echo "  make doc-check         校验文档数字/索引/头注释"
	@echo "  make lint / lint-strict 分层/输出/分组/输入面检查"
	@echo "  make shaders           GLSL → SPIR-V + 同步内嵌数组"
	@echo "  make spv-check         校验内嵌 SPIR-V 与磁盘 .spv 一致"
	@echo "  make cornellBox/cornellPt/cornellCompare/cornellCheck    Cornell 家族"
	@echo "  make mmdViewer / imoScene / physicsVideo / clothVideo / ...  示例"
	@echo "  make asan              ASan/UBSan 单测（build-asan/）"
	@echo "  make debug             Debug 构建（build-cmake-debug/）"
	@echo "  make crosscheck        Windows 交叉编译检查（experimental）"
	@echo "  make clean             清理构建产物"
	@echo "  make help              本帮助"
	@echo ""
	@echo "  experimental 平台（不静默删除能力）：make -f Makefile.legacy PLATFORM=windows CROSS=..."

info:
	@echo "  BUILD   = $(BUILD)"
	@echo "  PRESET  = $(PRESET)"
	@echo "  CMAKE   = $(CMAKE)"
	@echo "  JOBS    = $(JOBS)"

DEFAULT_GOAL := all
