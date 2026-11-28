#!/bin/bash
# tests/lint/output.sh — 输出通道纪律检查
#
# 背景（实测后修正的认知，见 docs/ARCHITECTURE_UNIFICATION.md §2.6）：
#   引擎里的输出本来就是三层合理分工：
#     GN_Log    → RunLog.txt（持久日志）
#     stderr    → 控制台诊断
#     GN_LogErr → 两者都写（必须让人立刻知道且事后能查）
#   所以本脚本【不是】去禁止 fprintf(stderr)，而是守住三条底线：
#     1. src/ 里不允许裸 printf 到 stdout（会把 CLI 程序的机器可读输出搅乱）
#     2. src/ 里不允许 sprintf（用 snprintf，防溢出）
#     3. GN_LogErr 存在且被 libGN_mini 导出
#
# 退出码：0 = 通过；1 = 有违规
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT" || exit 2
[ -d src ] || { echo "[SKIP] 不在仓库根"; exit 2; }

fail=0
echo "==== 输出通道检查 ===="

# 1) 裸 printf 到 stdout（排除 fprintf/vprintf/snprintf/sprintf）
n=$(grep -rnE '(^|[^fvsn_])printf\(' src/ --include=*.cpp --include=*.h 2>/dev/null \
    | grep -vE 'snprintf|vprintf|vfprintf|fprintf|//' | wc -l | tr -d ' ')
if [ "$n" -eq 0 ]; then
    echo "  [PASS] src/ 无裸 printf（不会污染 CLI 的 stdout）"
else
    echo "  [FAIL] src/ 有 $n 处裸 printf："
    grep -rnE '(^|[^fvsn_])printf\(' src/ --include=*.cpp --include=*.h 2>/dev/null \
        | grep -vE 'snprintf|vprintf|vfprintf|fprintf|//' | head -5 | sed 's/^/         /'
    fail=1
fi

# 2) 字符串安全（B9-4）：strcpy/strcat 清零；sprintf 仅余登记契约站点
# strcpy/strcat 已全部改 memcpy（精确长度，见 STRCPY_AUDIT / B9_QUALITY_GATE_PLAN），零容忍。
sc=$(grep -rn '\bstrcpy(\|\bstrcat(' src/ --include=*.cpp --include=*.h --include=*.inc 2>/dev/null | wc -l | tr -d ' ')
if [ "$sc" -eq 0 ]; then
    echo "  [PASS] src/ 无 strcpy/strcat（已改 memcpy 精确长度拷贝）"
else
    echo "  [FAIL] src/ 有 $sc 处 strcpy/strcat（请用 memcpy/snprintf/GN_StrCopyToLimit）："
    grep -rn '\bstrcpy(\|\bstrcat(' src/ --include=*.cpp --include=*.h --include=*.inc 2>/dev/null | head -5 | sed 's/^/         /'
    fail=1
fi
# sprintf → snprintf。存量基线 = 9 处登记契约/动态指针站点（容量由调用方/预计算保证，无法本地 snprintf）：
#   GN_NetTrans.cpp 406/423/541/787/895/897/898（HTTP body 预计算缓冲，w_pos/zContent 指针写入）
#   sysEncoding.inc itoa(_DstBuf)（调用方缓冲契约）、sysMutexIo.inc GN_GetDeviceInfoRam(info)（调用方缓冲契约）
n=$(grep -rnE '(^|[^nv_])sprintf\(' src/ --include=*.cpp --include=*.h --include=*.inc 2>/dev/null | grep -v 'snprintf' | grep -vE ':\s*//|:\s*\*|:\s*/\*' | wc -l | tr -d ' ')
SPRINTF_BASELINE=9
if [ "$n" -le "$SPRINTF_BASELINE" ]; then
    echo "  [PASS] sprintf 存量 $n 处（≤ 登记契约基线 $SPRINTF_BASELINE，只防新增）"
else
    echo "  [FAIL] sprintf 从 $SPRINTF_BASELINE 增加到 $n 处（新代码请用 snprintf）"
    fail=1
fi

# 3) GN_LogErr 已实现
if grep -q "GN_API void GN_LogErr" src/file/GN_File.h 2>/dev/null &&
   grep -q "GN_API void GN_LogErr" src/file/GN_File.cpp 2>/dev/null; then
    echo "  [PASS] GN_LogErr 已声明并实现（RunLog + stderr 双写）"
else
    echo "  [FAIL] GN_LogErr 缺失"
    fail=1
fi

echo "---- $( [ $fail -eq 0 ] && echo '全部通过' || echo '有违规' ) ----"
exit $fail
