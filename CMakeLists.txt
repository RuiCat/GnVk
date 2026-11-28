# GnVk — CMake 构建（B8，2026-11-26）
#
# 与 Makefile 的 Linux/Vulkan 配置做 parity（同一套源码/宏/特性检测/优化级别）：
#   cmake --preset default && cmake --build --preset default -j2
#   LD_LIBRARY_PATH=build-cmake ./build-cmake/unit_tests
#   GN_BUILD_DIR=build-cmake tests/smoke/runSamples.sh
#
# D3（2026-11-26）：官方仅支持 Linux。本 CMake 构建只实现 Linux/Vulkan；
# Windows/macOS/Android 的 experimental 目标仍走 Makefile（PLATFORM=...）。
# parity 验证完成前，Makefile 仍是 ci 的驱动（make ci 不变）。
cmake_minimum_required(VERSION 3.16)
project(GnVk LANGUAGES CXX)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "GnVk CMake 构建仅支持 Linux（D3）。experimental 平台请用 Makefile PLATFORM=...")
endif()

set(CMAKE_CXX_STANDARD 17) # B9-2：C++17（Linux-only，D3；全仓扫描 0 error）
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF) # -std=c++17（不是 gnu++17）；Makefile.legacy 交叉平台仍 c++11（experimental）
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()
# parity：Makefile 恒用 -O3 且不带 -DNDEBUG（assert 保持启用），这里对齐
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

option(GN_BUILD_TESTS "编译单测与示例可执行文件" ON)

# 产物输出到源码目录 build-cmake/（runSamples.sh 用 GN_BUILD_DIR 指向这里做 parity）
set(GN_OUT_DIR "${CMAKE_SOURCE_DIR}/build-cmake")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${GN_OUT_DIR}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${GN_OUT_DIR}")

# ── 可选特性检测（与 Makefile 同一组 pkg-config 项）──────────────
find_package(PkgConfig REQUIRED)
set(GN_FEATURE_DEFS "")
set(GN_FEATURE_LIBS "")

pkg_check_modules(GN_X11 QUIET x11)
if(GN_X11_FOUND)
    list(APPEND GN_FEATURE_DEFS GN_HAS_X11_CLIPBOARD)
    list(APPEND GN_FEATURE_LIBS ${GN_X11_LINK_LIBRARIES})
endif()
pkg_check_modules(GN_ALSA QUIET alsa)
if(GN_ALSA_FOUND)
    list(APPEND GN_FEATURE_DEFS GN_HAS_ALSA_VOLUME)
    list(APPEND GN_FEATURE_LIBS ${GN_ALSA_LINK_LIBRARIES})
endif()
pkg_check_modules(GN_FFMPEG QUIET libavcodec libavformat libavutil libswscale)
if(GN_FFMPEG_FOUND)
    list(APPEND GN_FEATURE_DEFS GN_HAS_FFMPEG)
    list(APPEND GN_FEATURE_LIBS ${GN_FFMPEG_LINK_LIBRARIES})
endif()
pkg_check_modules(GN_FT QUIET freetype2)
if(GN_FT_FOUND)
    list(APPEND GN_FEATURE_DEFS GN_HAS_FREETYPE)
    list(APPEND GN_FEATURE_LIBS ${GN_FT_LINK_LIBRARIES})
    # parity 注意：pkg_check_modules 的 INCLUDE_DIRS 是传递闭包（会带出 libpng16 的头，
    # 抢在 external/include 的 libpng 1.2.4 之前导致 png_struct 变 opaque）。
    # Makefile 只用 freetype2 自身的 Cflags，这里只保留 freetype2 目录。
    set(GN_FT_OWN_INCS ${GN_FT_INCLUDE_DIRS})
    list(FILTER GN_FT_OWN_INCS INCLUDE REGEX "freetype2$")
    include_directories(${GN_FT_OWN_INCS})
endif()
pkg_check_modules(GN_JPEG QUIET libjpeg)
if(GN_JPEG_FOUND)
    list(APPEND GN_FEATURE_DEFS GN_HAS_JPEG)
    list(APPEND GN_FEATURE_LIBS ${GN_JPEG_LINK_LIBRARIES})
endif()

# ── 引擎共享库（源文件集合与 Makefile 的 ENGINE_SRC 一致）────────
file(GLOB GN_ENGINE_SRC CONFIGURE_DEPENDS
    src/core/*.cpp
    src/graphics/*.cpp src/graphics/_Vulkan/*.cpp
    src/audio/*.cpp src/audio/_Sound/*.cpp
    src/input/*.cpp
    src/ui/*.cpp
    src/network/*.cpp src/network/_Net/*.cpp
    src/file/*.cpp
    src/platform/_Main/*.cpp src/platform/_Sys/*.cpp
    src/media/Media/*.cpp
    src/mmd/*.cpp)
add_library(GN_mini SHARED ${GN_ENGINE_SRC})
target_include_directories(GN_mini PUBLIC
    src src/core src/graphics src/graphics/_Vulkan src/audio src/input src/ui
    src/network src/file src/platform src/media src/plugin external/include)
# 宏与 Makefile 的 DEFINES + MODULE_DEFINES 一致；PUBLIC 让测试目标同样拿到
target_compile_definitions(GN_mini PUBLIC
    GN_USING_VULKAN_GLFW_LINUX
    GN_USING_SOURCE_CODE GN_INIT_USING_SOURCE_CODE GN_MAIN_USING_SOURCE_CODE
    GN_SYS_USING_SOURCE_CODE GN_G_USING_SOURCE_CODE GN_SOUND_USING_SOURCE_CODE
    GN_NET_USING_SOURCE_CODE GNm_USING_SOURCE_CODE
    ${GN_FEATURE_DEFS})
# parity：-O3 来自 CMAKE_CXX_FLAGS_RELEASE；其余与 CXXFLAGS_BASE+EXTRA 一致
target_compile_options(GN_mini PRIVATE -Wall -Wextra -msse2 -include cctype)
target_link_libraries(GN_mini PUBLIC glfw vulkan png openal z ${GN_FEATURE_LIBS})

# ── 测试与示例（-O2；B9-1 起无 -fpermissive）───
if(GN_BUILD_TESTS)
    function(gn_add_exe name)
        cmake_parse_arguments(ARG "" "" "SRCS;INCS" ${ARGN})
        add_executable(${name} ${ARG_SRCS})
        target_compile_options(${name} PRIVATE -O2)
        if(ARG_INCS)
            target_include_directories(${name} PRIVATE ${ARG_INCS})
        endif()
        target_link_libraries(${name} PRIVATE GN_mini)
        # 产物同目录即可找到 libGN_mini.so（Makefile 靠 LD_LIBRARY_PATH=.）
        set_target_properties(${name} PROPERTIES BUILD_RPATH "$ORIGIN")
    endfunction()

    # 单测：tests/unit + 两个示例的键位绑定表/换装规则 + 程序化资产生成器
    file(GLOB GN_UNIT_SRC CONFIGURE_DEPENDS tests/unit/*.cpp)
    gn_add_exe(unit_tests SRCS ${GN_UNIT_SRC}
        tests/mmdViewer/mmdViewerActions.cpp
        tests/imoScene/imoSceneActions.cpp
        tests/imoScene/imoSceneOutfitConfig.cpp
        tests/imoScene/imoSceneOutfit.cpp
        tests/fixtures/fixtures.cpp
        INCS tests/unit tests/mmdViewer tests/imoScene tests/fixtures)

    file(GLOB GN_FIXTURE_SRC CONFIGURE_DEPENDS tests/fixtures/*.cpp)
    gn_add_exe(fixtures SRCS ${GN_FIXTURE_SRC} INCS tests/fixtures)

    file(GLOB GN_GPU_SRC CONFIGURE_DEPENDS tests/gpu/*.cpp)
    gn_add_exe(gpuCheck SRCS ${GN_GPU_SRC} tests/fixtures/fixtures.cpp INCS tests/fixtures)

    file(GLOB GN_HEADLESS_SRC CONFIGURE_DEPENDS tests/headless/*.cpp)
    gn_add_exe(headlessCheck SRCS ${GN_HEADLESS_SRC})

    # B9-5 解析器模糊测试（纯 CPU 解析器；ASan 构建见 make fuzz）
    gn_add_exe(fuzzParsers SRCS tests/fuzz/fuzzParsers.cpp INCS src/mmd)

    # 普通示例：tests/<dir>/*.cpp → <name>
    foreach(name physicsBoxes characterDemo physicsVideo clothDemo fireLighting
                 editorGizmo gizmoInput undoDemo characterPush mmdViewer imoScene)
        file(GLOB GN_SRC_${name} CONFIGURE_DEPENDS tests/${name}/*.cpp)
        gn_add_exe(${name} SRCS ${GN_SRC_${name}})
    endforeach()

    # 带 tests/common 的示例
    foreach(name shadowProbe multiShadowDemo ragdollDemo ikDemo rootMotion animBlend)
        file(GLOB GN_SRC_${name} CONFIGURE_DEPENDS tests/${name}/*.cpp)
        gn_add_exe(${name} SRCS ${GN_SRC_${name}} INCS tests/common)
    endforeach()

    # ── Cornell 家族（Makefile 用显式源列表，不是通配）────────────
    # cornellBox：链引擎，-O2 -pthread（其余同普通示例）
    gn_add_exe(cornellBox SRCS
        tests/cornellBox/main.cpp tests/cornellBox/cornellScene.cpp
        tests/cornellBox/cornellCamera.cpp tests/cornellBox/cornellHud.cpp
        tests/cornellBox/cornellGeometry.cpp tests/cornellBox/cornellCapture.cpp
        tests/cornellBox/cornellCpuRt.cpp tests/cornellBox/cornellProbeBake.cpp
        tests/cornellBox/cornellGpuRt.cpp
        tests/cornellBox/cornellRtAs.cpp tests/cornellBox/cornellRtDevice.cpp)
    target_compile_options(cornellBox PRIVATE -pthread)

    # 独立工具（不链引擎库，与 Makefile 一致）
    function(gn_add_standalone name)
        cmake_parse_arguments(ARG "" "OPT" "SRCS;INCS;LIBS" ${ARGN})
        add_executable(${name} ${ARG_SRCS})
        target_compile_options(${name} PRIVATE ${ARG_OPT})
        if(ARG_INCS)
            target_include_directories(${name} PRIVATE ${ARG_INCS})
        endif()
        target_link_libraries(${name} PRIVATE ${ARG_LIBS})
        set_target_properties(${name} PROPERTIES BUILD_RPATH "$ORIGIN")
    endfunction()
    gn_add_standalone(cornellPt OPT -O3 -pthread INCS tests/cornellBox LIBS png z
        SRCS tests/cornellBox/cornellGeometry.cpp tests/cornellBox/cornellPathTracer.cpp
             tests/cornellBox/cornellPtMain.cpp)
    gn_add_standalone(cornellCompare OPT -O2 LIBS png z
        SRCS tests/cornellBox/cornellCompare.cpp)
    gn_add_standalone(cornellRtProbe OPT -O2 -pthread LIBS png z
        SRCS tests/cornellBox/cornellGeometry.cpp tests/cornellBox/cornellCpuRt.cpp
             tests/cornellBox/cornellCpuRtProbe.cpp)
    gn_add_standalone(cornellGpuRtProbe OPT -O2 -pthread LIBS vulkan png z
        SRCS tests/cornellBox/cornellGeometry.cpp tests/cornellBox/cornellCpuRt.cpp
             tests/cornellBox/cornellGpuRt.cpp tests/cornellBox/cornellRtAs.cpp
             tests/cornellBox/cornellGpuRtProbe.cpp)
    gn_add_standalone(cornellHwRtProbe OPT -O2 -pthread LIBS vulkan png z
        SRCS tests/cornellBox/cornellGeometry.cpp tests/cornellBox/cornellCpuRt.cpp
             tests/cornellBox/cornellGpuRt.cpp tests/cornellBox/cornellRtAs.cpp
             tests/cornellBox/cornellRtDevice.cpp tests/cornellBox/cornellHwRtProbe.cpp)

    # 三个光追自检含 cornellCpuRt.cpp（#include <GN.h>）且调用 GN3d:: 引擎函数：
    # 需要引擎头路径、宏并链接引擎库。Makefile 旧规则漏了 -I 与 -lGN_mini 导致编不过，
    # 2026-11-26 两边同步修复（DEFECT_ANALYSIS 登记）。
    foreach(probe cornellRtProbe cornellGpuRtProbe cornellHwRtProbe)
        target_link_libraries(${probe} PRIVATE GN_mini)
    endforeach()
endif()