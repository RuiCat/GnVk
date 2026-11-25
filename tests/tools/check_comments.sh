#!/bin/bash
# tests/tools/check_comments.sh — 头文件注释覆盖率检查
#
# 为什么需要它：docs/COMMENT_STYLE.md 约定「每个 .h 在 include guard 之后必须有一段
# 文件头注释」，说明它负责哪一块、为什么需要、边界在哪。这条约定在 2026-11-25 补齐到
# 118/118，但它只靠自觉就会退化 —— 新加一个头文件忘了写，没人会发现。
#
# 判据刻意宽松：只看**前 14 行有没有注释行**，不检查内容质量（质量只能靠人看）。
# 宽判据的好处是零误报，能长期挂在 make doc-check 上。
#
# 用法：bash tests/tools/check_comments.sh
# 退出码：0 = 全部有；1 = 有缺失。
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT" || exit 2
[ -d src ] || { echo "[SKIP] 不在仓库根"; exit 2; }

total=0
missing=0
while IFS= read -r f; do
    total=$((total + 1))
    # 自动生成的内嵌 SPIR-V 数组不要求手写头注释
    case "$f" in
        *_spv.h) continue ;;
    esac
    if ! head -14 "$f" | grep -qE '^[[:space:]]*(//|/\*)'; then
        echo "[FAIL] 缺少文件头注释：$f"
        missing=$((missing + 1))
    fi
done < <(find src -name '*.h' | sort)

if [ "$missing" -ne 0 ]; then
    echo "[comments] $missing / $total 个头文件缺少文件头注释"
    echo "           约定见 docs/COMMENT_STYLE.md §1"
    exit 1
fi
echo "[comments] PASS —— $total 个头文件均有文件头注释"
exit 0
