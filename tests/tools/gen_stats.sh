#!/bin/bash
# tests/tools/gen_stats.sh — 文档数字的唯一来源
#
# 为什么需要它：这套仓库的文档里到处写着「463 单测」「109 项检查」「35 条管线」这类数字。
# 它们靠人工维护，而这一路已经出现过至少三次漂移：
#   * tests/gpu/README.md 写 "99 条"（实为 109）
#   * vk3dForward.cpp 注释说「每个 3D pass 开始时同步」（实为帧首 + 代际号）
#   * tests/README.md 写 "463 单测"（实为 491）
# 人工维护必然漂移，所以改成生成。
#
# 用法：
#   tests/tools/gen_stats.sh            # 校验：标记区内内容与实测不符则退出码 1
#   tests/tools/gen_stats.sh --write    # 生成：把实测数字写回各文件的标记区
#
# 标记格式（HTML 注释，Markdown 渲染时不可见）：
#   <!-- AUTO-STATS:BEGIN -->
#   ... 由脚本覆盖的内容 ...
#   <!-- AUTO-STATS:END -->
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT" || exit 2
[ -d src ] || { echo "[SKIP] 不在仓库根"; exit 2; }

WRITE=0
[ "${1:-}" = "--write" ] && WRITE=1

# ── 收集实测数据 ────────────────────────────────────────
# 若工作区里有引擎环境脚本（容器/无 GPU 场景），先 source 它，
# 否则 libGN_mini.so 的依赖（freetype/ffmpeg/…）可能找不到，实跑会「失败」。
if [ -f /work/.sbx/gnvk-env.sh ]; then
    # 注意：必须临时关掉 set -u。环境脚本引用了未定义变量，
    # 在 set -u 下会直接终止（而不是返回非零），导致后面全静默失败。
    set +u
    # shellcheck disable=SC1091
    source /work/.sbx/gnvk-env.sh >/dev/null 2>&1 || true
    set -u
fi
if [ -z "${VK_ICD_FILENAMES:-}" ] && [ -f /work/.sbx/sysroot/usr/share/vulkan/icd.d/lvp_icd.json ]; then
    export VK_ICD_FILENAMES=/work/.sbx/sysroot/usr/share/vulkan/icd.d/lvp_icd.json
fi
unset DISPLAY 2>/dev/null || true

# 单测：静态统计 TEST_CASE 宏（与实际运行数一致，无需跑）
UNIT_CASES=$(grep -rho "TEST_CASE(" tests/unit/*.cpp | wc -l | tr -d ' ')

# 单测文件数、gpuCheck 文件数
UNIT_FILES=$(ls tests/unit/test*.cpp 2>/dev/null | wc -l | tr -d ' ')
GPU_FILES=$(ls tests/gpu/gpuCheck*.cpp 2>/dev/null | wc -l | tr -d ' ')

# src 规模
SRC_CPP=$(find src -name '*.cpp' | wc -l | tr -d ' ')
SRC_LINES=$(find src -name '*.cpp' -o -name '*.h' | xargs cat 2>/dev/null | wc -l | tr -d ' ')

# 着色器
SHADER_SPV=$(find assets/shaders -name '*.spv' 2>/dev/null | wc -l | tr -d ' ')

# gpuCheck 与 unit 的实跑结果（需要二进制已构建且有 Vulkan 设备）
GPU_SUMMARY="（未运行：build/gpuCheck 不存在）"
UNIT_SUMMARY="（未运行：build/unit_tests 不存在）"
if [ -x build/unit_tests ]; then
    UNIT_SUMMARY=$(LD_LIBRARY_PATH=.:${LD_LIBRARY_PATH:-} ./build/unit_tests 2>&1 | grep -E "cases," | tail -1)
    [ -z "$UNIT_SUMMARY" ] && UNIT_SUMMARY="（运行失败）"
fi
if [ -x build/gpuCheck ]; then
    GPU_SUMMARY=$(LD_LIBRARY_PATH=.:${LD_LIBRARY_PATH:-} GN_HEADLESS=1 ./build/gpuCheck 2>&1 \
        | grep -oE "pass=[0-9]+ fail=[0-9]+ skip=[0-9]+" | tail -1)
    [ -z "$GPU_SUMMARY" ] && GPU_SUMMARY="（无 Vulkan 设备或运行失败）"
fi

# ── 生成内容 ────────────────────────────────────────────
BLOCK="<!-- AUTO-STATS:BEGIN -->
<!-- 本段由 tests/tools/gen_stats.sh 生成，不要手改；改数字请跑 make stats -->

| 项 | 数值 |
|----|------|
| 单元测试用例 | **${UNIT_CASES}**（${UNIT_FILES} 个文件）|
| gpuCheck 文件 | ${GPU_FILES} |
| gpuCheck 实跑 | \`${GPU_SUMMARY}\` |
| 单测实跑 | \`${UNIT_SUMMARY}\` |
| src 源文件 | ${SRC_CPP} 个 .cpp |
| src 总行数 | ${SRC_LINES} |
| 编译产物着色器 | ${SHADER_SPV} 个 .spv |

<!-- AUTO-STATS:END -->"

# ── 目标文件（显式列表）───────────────────────────────
# 不用 grep 扫全文找标记：文档里"提及"标记语法的散文（如 CHANGELOG 解释本机制时）
# 也会被扫中，然后因为不成对而误报。显式列表确定性强。
TARGET_CANDIDATES="README.md tests/README.md tests/gpu/README.md"

TARGETS=""
for f in $TARGET_CANDIDATES; do
    if [ ! -f "$f" ]; then
        continue
    fi
    if grep -q "AUTO-STATS:BEGIN" "$f" && grep -q "AUTO-STATS:END" "$f"; then
        TARGETS="$TARGETS $f"
    else
        echo "  [WARN] $f 标记不成对（跳过）"
    fi
done
TARGETS=$(echo $TARGETS)

if [ -z "$TARGETS" ]; then
    echo "[SKIP] 没有任何目标文件含 AUTO-STATS 标记"
    exit 2
fi

fail=0
for f in $TARGETS; do
    if [ "$WRITE" -eq 1 ]; then
        python3 - "$f" <<PYEOF
import re, sys
path = sys.argv[1]
block = '''$BLOCK'''
text = open(path, encoding='utf-8').read()
pattern = re.compile(r'<!-- AUTO-STATS:BEGIN -->.*?<!-- AUTO-STATS:END -->', re.S)
if pattern.search(text):
    text = pattern.sub(block, text, count=1)
    open(path, 'w', encoding='utf-8').write(text)
    print(f"  已写入 {path}")
else:
    print(f"  !! {path} 里标记不成对")
    sys.exit(1)
PYEOF
        [ $? -ne 0 ] && fail=1
    else
        # 校验模式：比对标记区内内容
        if ! python3 - "$f" "$BLOCK" <<'PYEOF'
import re, sys
path, expected = sys.argv[1], sys.argv[2]
text = open(path, encoding='utf-8').read()
m = re.search(r'<!-- AUTO-STATS:BEGIN -->.*?<!-- AUTO-STATS:END -->', text, re.S)
if not m:
    print(f"  [FAIL] {path} 标记不成对")
    sys.exit(1)
if m.group(0).strip() != expected.strip():
    print(f"  [FAIL] {path} 的统计区已过时（跑 make stats 更新）")
    sys.exit(1)
print(f"  [PASS] {path}")
PYEOF
        then
            fail=1
        fi
    fi
done

echo "---- $( [ $fail -eq 0 ] && echo '一致' || echo '有过时数字' ) ----"
[ "$WRITE" -eq 1 ] && echo "（已生成；记得提交）"
exit $fail
