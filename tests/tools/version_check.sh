#!/bin/bash
# tests/tools/version_check.sh — 版本单一来源一致性门禁（B10-1，REFACTOR_PLAN C14）
#
# 单一来源：src/core/GN.h 的 GN_VERSION（引擎主版本，YYYY.MM.DD）与
# GN_INTERFACE_VERSION（接口版本，semver）。GN.h 内 `#ifndef GN_USING_UTILITY_LIB`
# 的 "0.0" 回退是 utility-lib 构建分支，不算第二来源。
#
# 检查：
#   1. GN.h 两个版本宏存在且格式合法；
#   2. 架构文档版本行引用 GN.h（不写死冲突版本）；
#   3. README 不写死与 GN.h 冲突的引擎主版本。
# 退出码：0 通过；1 不一致。
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT" || exit 2
GNH="src/core/GN.h"
fail=0
echo "==== 版本单一来源检查（C14）===="

# 取首个 #define（主版本；utility-lib 回退在其后）
VER=$(grep -m1 -E '^#define GN_VERSION "' "$GNH" | sed -E 's/^#define GN_VERSION "([^"]+)".*/\1/')
IVER=$(grep -m1 -E '^#define GN_INTERFACE_VERSION "' "$GNH" | sed -E 's/^#define GN_INTERFACE_VERSION "([^"]+)".*/\1/')

if echo "$VER" | grep -qE '^[0-9]{4}\.[0-9]{2}\.[0-9]{2}$'; then
    echo "  [PASS] GN_VERSION = $VER（YYYY.MM.DD）"
else
    echo "  [FAIL] GN_VERSION 缺失或格式非法：'$VER'"; fail=1
fi
if echo "$IVER" | grep -qE '^[0-9]+\.[0-9]+\.[0-9]+$'; then
    echo "  [PASS] GN_INTERFACE_VERSION = $IVER（semver）"
else
    echo "  [FAIL] GN_INTERFACE_VERSION 缺失或格式非法：'$IVER'"; fail=1
fi

# 架构文档须引用 GN.h 作版本来源
if grep -q 'GN.h' docs/ENGINE_ARCHITECTURE.md && grep -qE 'GN_VERSION|GN_INTERFACE_VERSION' docs/ENGINE_ARCHITECTURE.md; then
    echo "  [PASS] 架构文档版本引用 GN.h（单一来源）"
else
    echo "  [FAIL] 架构文档版本未引用 GN.h"; fail=1
fi

# README 若写死引擎主版本（YYYY.MM.DD），须与 GN.h 一致
if [ -n "$VER" ]; then
    bad=$(grep -oE '[0-9]{4}\.[0-9]{2}\.[0-9]{2}' README.md | grep -v "^$VER$" | sort -u)
    if [ -z "$bad" ]; then
        echo "  [PASS] README 无冲突的引擎主版本"
    else
        echo "  [FAIL] README 写死了与 GN.h($VER) 冲突的版本：$bad"; fail=1
    fi
fi

if [ "$fail" -eq 0 ]; then echo "---- 版本一致 ----"; else echo "---- 版本不一致 ----"; fi
exit $fail