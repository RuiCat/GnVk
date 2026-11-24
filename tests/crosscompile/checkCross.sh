#!/bin/bash
# tests/crosscompile/checkCross.sh — 交叉平台编译检查（只编译，不链接）
#
# 目的：验证 Vulkan 后端不只"在 Linux 上能编"。用 MinGW-w64 把 src/ 下全部源文件
# 按 Windows 目标编译成 .o：这能抓出平台专属头文件（iconv.h / unistd.h / X11 …）、
# 未加保护的 Linux API、以及只在某个平台成活的类型声明。
#
# 不链接：本机通常没有 Windows 版 glfw/vulkan/png/openal/zlib 的导入库。
# 链接与运行仍需在目标平台上做。
#
# 退出码：0 = 全部编译通过；1 = 有编译错误；2 = SKIP（没有交叉工具链）
#
# 用法：
#   tests/crosscompile/checkCross.sh                # 默认 x86_64-w64-mingw32-
#   CROSS=x86_64-w64-mingw32- tests/crosscompile/checkCross.sh
#   JOBS=4 tests/crosscompile/checkCross.sh
#
# 依赖（Ubuntu）：
#   apt install g++-mingw-w64-x86-64 libvulkan-dev libglfw3-dev libpng-dev \
#               libfreetype-dev libopenal-dev zlib1g-dev
#   头文件是三平台通用的 C 头，可以直接给交叉编译器用。

set -u

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT" || exit 1

CROSS="${CROSS:-x86_64-w64-mingw32-}"
CXX="${CROSS}g++"
JOBS="${JOBS:-2}"
OUT="${OUT:-build/cross-$CROSS}"

if ! command -v "$CXX" > /dev/null 2>&1; then
    echo "[SKIP] 找不到 $CXX（装 g++-mingw-w64-x86-64 或设 CROSS=）"
    exit 2
fi

# 头文件探测：Vulkan 与 GLFW 是必需项，其余缺失只影响对应特性的编译。
PROBE="$(mktemp -d)"
trap 'rm -rf "$PROBE"' EXIT
printf '#include <vulkan/vulkan.h>\n#include <GLFW/glfw3.h>\nint main(){return 0;}\n' > "$PROBE/probe.cpp"
if ! "$CXX" -fsyntax-only "$PROBE/probe.cpp" > /dev/null 2>&1; then
    echo "[SKIP] 交叉编译器找不到 vulkan/vulkan.h 或 GLFW/glfw3.h"
    exit 2
fi

SRC=$(ls src/core/*.cpp src/graphics/*.cpp src/graphics/_Vulkan/*.cpp \
         src/audio/*.cpp src/audio/_Sound/*.cpp src/input/*.cpp src/ui/*.cpp \
         src/network/*.cpp src/network/_Net/*.cpp src/file/*.cpp \
         src/platform/_Main/*.cpp src/platform/_Sys/*.cpp \
         src/media/Media/*.cpp src/mmd/*.cpp 2>/dev/null)

rm -rf "$OUT"
mkdir -p "$OUT"

FLAGS="-std=c++11 -fpermissive -w -c
 -DGN_USING_VULKAN_GLFW_WINDOWS
 -DGN_USING_SOURCE_CODE -DGN_INIT_USING_SOURCE_CODE -DGN_MAIN_USING_SOURCE_CODE
 -DGN_SYS_USING_SOURCE_CODE -DGN_G_USING_SOURCE_CODE -DGN_SOUND_USING_SOURCE_CODE
 -DGN_NET_USING_SOURCE_CODE -DGNm_USING_SOURCE_CODE
 -Isrc -Isrc/core -Isrc/graphics -Isrc/graphics/_Vulkan -Isrc/audio -Isrc/input -Isrc/ui
 -Isrc/network -Isrc/file -Isrc/platform -Isrc/media -Isrc/plugin -Iexternal/include"

echo "==== 交叉编译检查: ${CXX} (Vulkan + GLFW 后端) ===="
echo "源文件: $(printf '%s\n' $SRC | wc -l) 个 → $OUT"

export ROOT OUT CXX FLAGS
printf '%s\n' $SRC | xargs -P "$JOBS" -n 1 bash -c '
    f="$1"
    o="$OUT/$(echo "$f" | tr "/" "_").o"
    if ! (cd "$ROOT" && $CXX $FLAGS "$f" -o "$o") 2> "$o.err"; then
        echo "  [FAIL] $f"
    else
        rm -f "$o.err"
    fi
' _

fails=$(ls "$OUT"/*.err 2>/dev/null | wc -l)
objs=$(ls "$OUT"/*.o 2>/dev/null | wc -l)
echo "---- CROSS SUMMARY: ok=$objs fail=$fails ----"
for e in "$OUT"/*.err; do
    [ -f "$e" ] || continue
    echo "== $(basename "${e%.o.err}")"
    head -8 "$e"
done

[ "$fails" -eq 0 ] && exit 0
exit 1
