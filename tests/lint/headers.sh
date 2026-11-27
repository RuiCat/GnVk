#!/bin/bash
# tests/lint/headers.sh — 公共头自包含检查（REFACTOR_PLAN C10，G3-0 新增）。
#
# 每个头必须能单独 -fsyntax-only 编译，不依赖 GN.h 伞头的包含顺序。
# 默认报告模式：存量头有基线（abi/headers-baseline.txt），只许减不许增；
# --strict：任何失败即阻断（用于新增/重写目录）。
#
# 用法：tests/lint/headers.sh [--strict] [dir...]
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT" || exit 1
STRICT=0
if [ "${1:-}" = "--strict" ]; then STRICT=1; shift; fi
DIRS=("$@"); [ ${#DIRS[@]} -eq 0 ] && DIRS=(src)
CXX="${CXX:-g++}"
FLAGS="-std=c++17 -msse2 -include cctype -DGN_USING_VULKAN_GLFW_LINUX -fsyntax-only" # B9-2：与构建同标准，无 -fpermissive
INC="-Isrc -Isrc/core -Isrc/graphics -Isrc/graphics/_Vulkan -Isrc/audio -Isrc/input -Isrc/ui -Isrc/network -Isrc/file -Isrc/platform -Isrc/media -Isrc/plugin -Iexternal/include"
total=0; fail=0; failed=()
while IFS= read -r h; do
    total=$((total + 1))
    if ! $CXX $FLAGS $INC -x c++ "$h" >/dev/null 2>&1; then
        fail=$((fail + 1)); failed+=("$h")
    fi
done < <(find "${DIRS[@]}" -name '*.h' | sort)
echo "---- 头自包含: $fail/$total 不能单独编译 ----"
if [ "$fail" -gt 0 ]; then printf '%s\n' "${failed[@]}"; fi
if [ "$STRICT" -eq 1 ] && [ "$fail" -gt 0 ]; then exit 1; fi
# 基线比对（只许减不许增）：与 abi/headers-baseline.txt 逐行对比
BASE="$(cd "$(dirname "$0")/../.." && pwd)/abi/headers-baseline.txt"
if [ -f "$BASE" ]; then
    added=$(comm -13 <(sort "$BASE") <(printf '%s\n' "${failed[@]}" | sort) | grep -c . || true)
    removed=$(comm -23 <(sort "$BASE") <(printf '%s\n' "${failed[@]}" | sort) | grep -c . || true)
    if [ "$added" -gt 0 ]; then
        echo "[FAIL] 头自包含失败比基线新增 $added 个（只许减不许增）："
        comm -13 <(sort "$BASE") <(printf '%s\n' "${failed[@]}" | sort)
        exit 1
    fi
    echo "[PASS] 头自包含失败 ≤ 基线（新增 0 / 减少 $removed）"
fi
exit 0
