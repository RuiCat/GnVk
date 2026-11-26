#!/bin/bash
# tests/lint/context_groups.sh — GN_VkContext 分组守卫（A5）
#
# 背景：A5 把 265 行的 GN_VkContext 按子系统切成 8 个匿名 struct 组。
# 匿名 struct 的价值全在"新字段要进对应组"这个约定上——一旦有人图省事往顶层
# 直接加字段，分组就会重新退化成扁平表，白做。
#
# 本脚本检查三件事：
#   1. 顶层只允许 4 个命名非平凡成员（aPipelineCache + B7-2 的 aRetired3DBuffers/
#      aSkyParams/aDeferredViewProj）+ 10 个匿名 struct 组；
#   2. 每组都有「── sXxxRes：」标题注释；
#   3. 字段数不少于基线 200（防静默丢失；B7-1/B7-2 收敛只增不减）。
# 基线随批次登记更新（同 abi/baseline.txt 机制）：B7-2 新增 s3DPassRes/sRtRes 两组。
#
# 退出码：0 = 通过；1 = 有违规
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT" || exit 2
# 2026-11-26 B4：GN_Vk.h 拆成转发头 + .inc 片段，结构体随片段迁移；按内容定位而非写死路径
HDR=$(grep -rl '^struct GN_VkContext {' src/graphics/_Vulkan/ 2>/dev/null | head -1)
[ -n "$HDR" ] && [ -f "$HDR" ] || { echo "[SKIP] 找不到 GN_VkContext 定义"; exit 2; }

# 提取结构体正文（到构造函数为止）
BODY=$(awk '/^struct GN_VkContext \{/,/^    GN_VkContext\(\)/' "$HDR")
fail=0
echo "==== GN_VkContext 分组检查 ===="

# 1) 组数
groups=$(echo "$BODY" | grep -cE '^    // ── s[A-Za-z0-9]+Res：')
if [ "$groups" -eq 10 ]; then
    echo "  [PASS] 10 个命名分组（sDeviceRes … sDeferredRes + B7-2 s3DPassRes/sRtRes）"
else
    echo "  [FAIL] 分组数为 $groups，预期 10"
    echo "$BODY" | grep -oE '^    // ── s[A-Za-z0-9]+Res：' | sed 's/^/         /'
    fail=1
fi

# 2) 顶层字段：按“组标题行号”切分。aPipelineCache 必须位于第一个组标题之前。
top_block=$(echo "$BODY" | sed -n '/── s[A-Za-z0-9]*Res：/q;p')
top_level=$(echo "$top_block" | grep -cE '^\s+[A-Za-z_][^;]*;' | tr -d ' ')
if [ "$top_level" -eq 4 ] && echo "$top_block" | grep -q 'aPipelineCache' \
   && echo "$top_block" | grep -q 'aRetired3DBuffers' && echo "$top_block" | grep -q 'aSkyParams' \
   && echo "$top_block" | grep -q 'aDeferredViewProj'; then
    echo "  [PASS] 顶层仅 4 个命名非平凡成员（aPipelineCache/aRetired3DBuffers/aSkyParams/aDeferredViewProj）"
else
    echo "  [FAIL] 顶层有 $top_level 个字段，预期 4（命名非平凡成员；平凡新字段请放进对应组）"
    echo "$top_block" | grep -E '^\s+[A-Za-z_][^;]*;' | sed 's/^/         /'
    fail=1
fi

# 3) 匿名 struct 块数应与组数一致
blocks=$(echo "$BODY" | grep -cE '^    struct \{$')
if [ "$blocks" -eq 10 ]; then
    echo "  [PASS] 10 个匿名 struct 块"
else
    echo "  [FAIL] 匿名 struct 块数为 $blocks，预期 10"
    fail=1
fi

# 4) 字段总数（防静默丢失）。基线 200，允许增（加新资源）不允许减。
total=$(echo "$BODY" | grep -cE '^\s+[A-Za-z_][^;]*;\s*(//.*)?$' | tr -d ' ')
BASELINE=200
if [ "$total" -ge "$BASELINE" ]; then
    echo "  [PASS] 字段数 $total（≥ 基线 $BASELINE）"
else
    echo "  [FAIL] 字段数从 $BASELINE 减少到 $total（有字段被误删）"
    fail=1
fi

echo "---- $( [ $fail -eq 0 ] && echo '全部通过' || echo '有违规' ) ----"
exit $fail
