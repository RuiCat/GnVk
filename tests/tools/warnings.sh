#!/bin/bash
# tests/tools/warnings.sh — B9 告警预算门禁（REFACTOR_PLAN c7）
#
# 口径（与 docs/DEFECT_ANALYSIS.md §5.3 台账一致）：
#   GCC 扩展警告全量 -fsyntax-only 扫描 src/ 全部 .cpp；
#   只计主位置在 src/ 下的警告（external/ 与系统头不计）；
#   -Wno-unused-parameter：兼容桩层保留参数名作 API 文档（台账口径本不含此类）。
# 预算：<500（B9 目标）；任何 error 即失败。
# 用法：tests/tools/warnings.sh [预算数]   （默认 500）
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT" || exit 2
BUDGET="${1:-500}"
[ -f /work/.sbx/gnvk-env.sh ] && . /work/.sbx/gnvk-env.sh

CXX="${CXX:-g++}"
DEFINES="-DGN_USING_VULKAN_GLFW_LINUX -DGN_USING_SOURCE_CODE -DGN_INIT_USING_SOURCE_CODE \
 -DGN_MAIN_USING_SOURCE_CODE -DGN_SYS_USING_SOURCE_CODE -DGN_G_USING_SOURCE_CODE \
 -DGN_SOUND_USING_SOURCE_CODE -DGN_NET_USING_SOURCE_CODE -DGNm_USING_SOURCE_CODE"
# 特性宏按 pkg-config 探测（与 CMakeLists 同一组）
for p in "x11:GN_HAS_X11_CLIPBOARD" "alsa:GN_HAS_ALSA_VOLUME" "libavcodec libavformat libavutil libswscale:GN_HAS_FFMPEG" "freetype2:GN_HAS_FREETYPE" "libjpeg:GN_HAS_JPEG"; do
    mods="${p%%:*}"; def="${p##*:}"
    pkg-config --exists $mods 2>/dev/null && DEFINES="$DEFINES -D$def"
done
FT_INC=$(pkg-config --cflags-only-I freetype2 2>/dev/null | tr ' ' '\n' | grep 'freetype2$' | head -1)
INCLUDES="-Isrc -Isrc/core -Isrc/graphics -Isrc/graphics/_Vulkan -Isrc/audio -Isrc/input -Isrc/ui \
 -Isrc/network -Isrc/file -Isrc/platform -Isrc/media -Isrc/plugin -Iexternal/include ${FT_INC}"
FLAGS="-std=c++17 -fPIC -Wall -Wextra -Wshadow -Wduplicated-cond -Wduplicated-branches \
 -Wlogical-op -Wuse-after-free=2 -Woverloaded-virtual -Wno-unused-parameter \
 -msse2 -include cctype -fsyntax-only"

LOG=$(mktemp -d)
trap 'rm -rf "$LOG"' EXIT
find src -name '*.cpp' | sort | xargs -P"${JOBS:-2}" -I{} bash -c \
    '"$0" "$1" '"$FLAGS $DEFINES $INCLUDES"' 2>"$2/$(echo "$1" | tr / _).log"; true' \
    "$CXX" {} "$LOG"

ERR=$(grep -h 'error:' "$LOG"/*.log 2>/dev/null | wc -l)
WARN=$(grep -h 'warning:' "$LOG"/*.log 2>/dev/null | grep -c '^src/')
echo "==== 告警预算门禁（扩展警告口径，src/ 主位置）===="
echo "  error: $ERR / warning: $WARN / 预算: < $BUDGET"
grep -h 'warning:' "$LOG"/*.log 2>/dev/null | grep '^src/' | grep -oE '\[-W[a-z-]+\]' | sort | uniq -c | sort -rn | sed 's/^/    /'
if [ "$ERR" -gt 0 ]; then
    echo "  [FAIL] 有编译错误："
    grep -h 'error:' "$LOG"/*.log | sort -u | head -10 | sed 's/^/    /'
    exit 1
fi
if [ "$WARN" -ge "$BUDGET" ]; then
    echo "  [FAIL] 警告 $WARN ≥ 预算 $BUDGET"
    exit 1
fi
echo "  [PASS] 警告 $WARN < $BUDGET"
exit 0
