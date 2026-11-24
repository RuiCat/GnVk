#!/bin/bash
# tests/lint/input.sh — 示例键位访问面检查（P0 门禁）
#
# 为什么需要它：P0 之前两个示例里有 42 处直接读 GN::Key/GN::Key1/GN::Mouse/GN::Mouse1，
# 于是"改一次键位要翻遍游戏逻辑"。P0 把这些读取收进 GN::cINPUT_MAP 的动作编号后，
# 这条门禁负责防止旧写法回流：示例里再出现全局输入数组的下标访问就报红。
#
# 合法位置：src/input/（输入模块本身）与两张绑定表（只用 GK_*/GM_* 常量，不摸数组）。
#
# 退出码：0 = 干净；1 = 有直读；2 = SKIP（不在仓库根）
set -u

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT" || exit 2
[ -d tests ] || { echo "[SKIP] 不在仓库根（找不到 tests/）"; exit 2; }

# 匹配 GN::Key1[ / GN::Mouse[ 这类"全局输入数组下标访问"
PATTERN='GN::(Key|Key0|Key1|Key11|Mouse|Mouse0|Mouse1|Mouse2|MouseQ|MouseI|MouseT)[[:space:]]*\['
hits=$(grep -rnE "$PATTERN" tests/mmdViewer tests/imoScene 2>/dev/null)

echo "==== 示例输入访问面检查 ===="
if [ -n "$hits" ]; then
    echo "$hits" | sed 's/^/  [直读] /'
    echo "---- 违规 $(echo "$hits" | wc -l | tr -d ' ') 处 ----"
    echo "（键位请写进 mmdViewerActions.cpp / imoSceneActions.cpp，逻辑侧用 GN::cINPUT_MAP 的动作查询）"
    exit 1
fi
echo "  OK：示例里没有全局输入数组直读（键位只在两张绑定表里）"
echo "---- 违规 0 处 ----"
exit 0
