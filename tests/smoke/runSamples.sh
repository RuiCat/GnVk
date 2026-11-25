#!/bin/bash
# tests/smoke/runSamples.sh — 示例无窗口冒烟（CI / 容器无显示环境用）
#
# 判据（与 tests/gpu 的约定一致）：
#   0 = 通过    1 = 失败    2 = SKIP（没有 Vulkan 设备，不算通过）
#
# 每个示例都在 GN_HEADLESS_FRAMES 下跑固定帧数：不需要 X11/Wayland，也不需要
# vkAcquireNextImageKHR/present。缺模型/贴图资源的示例（mmdViewer、imoScene）在
# 资源缺失时本就只画背景不崩，因此这里只断言「跑完 N 帧且退出码 0」。
#
# 用法：
#   tests/smoke/runSamples.sh              # 用仓库根作为工作目录
#   GN_HEADLESS_FRAMES=16 tests/smoke/runSamples.sh
#
# 环境：
#   GN_HEADLESS_FRAMES  每个示例跑的帧数（默认 8）
#   VK_ICD_FILENAMES    软件渲染时指向 lavapipe 的 ICD JSON
#   SMOKE_FILTER        只跑名字里包含该串的示例

set -u

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT" || exit 1

FRAMES="${GN_HEADLESS_FRAMES:-8}"
FILTER="${SMOKE_FILTER:-}"
LIB="${LD_LIBRARY_PATH:-}"

pass=0
fail=0
skip=0
failed_names=()

# 先探一次设备：没有 Vulkan 设备就整体 SKIP（退出码 2），不逐项刷失败。
probe() {
    local out
    out="$(GN_HEADLESS_FRAMES=1 ./build/headlessCheck 2>&1)"
    local code=$?
    if [ $code -eq 2 ]; then
        return 2
    fi
    return 0
}

# 验收类：内部自己打 summary，按「基线」判：要求 pass 数不降且失败项不新增。
# 当前基线（2026-11-24）：pass=109 fail=0（含 7 条光照接入 light.* 与 1 条描述符纪律 desc.*）。
# known_fail 传空串表示任何 FAIL 都算新增失败。
# 历史：cull.debugLine 曾长期失败（测试把线段放在世界 z=+0.5，相机看向 -z 时 z_ndc=-0.5
# 落在 near 平面外、整条线被裁掉）；2026-11-24 改成 z=-0.5 后 101/101 全过。
# known_fail 传空串表示任何 FAIL 都算新增失败。
run_baseline() {
    local name="$1"
    local bin="$2"
    local min_pass="$3"
    local known_fail="$4"
    if [ -n "$FILTER" ] && [[ "$name" != *"$FILTER"* ]]; then
        return 0
    fi
    if [ ! -x "$bin" ]; then
        printf '  [SKIP] %-14s 未编译（%s 不存在）\n' "$name" "$bin"
        skip=$((skip + 1))
        return 0
    fi
    local log
    log="$(timeout 300 "$bin" 2>&1)"
    local code=$?
    local passes fails
    passes="$(printf '%s\n' "$log" | sed -n 's/.*pass=\([0-9]*\) fail=\([0-9]*\).*/\1/p' | tail -1)"
    fails="$(printf '%s\n' "$log" | sed -n 's/.*pass=\([0-9]*\) fail=\([0-9]*\).*/\2/p' | tail -1)"
    if [ -z "$passes" ]; then
        printf '  [FAIL] %-14s 无 summary（退出码 %s）\n' "$name" "$code"
        printf '%s\n' "$log" | tail -6 | sed 's/^/         /'
        fail=$((fail + 1))
        failed_names+=("$name")
        return 0
    fi
    local unexpected
    if [ -z "$known_fail" ]; then
        unexpected="$(printf '%s\n' "$log" | sed -n 's/^\[FAIL\] \([^ ]*\).*/\1/p' | tr '\n' ' ')"
    else
        unexpected="$(printf '%s\n' "$log" | sed -n 's/^\[FAIL\] \([^ ]*\).*/\1/p' | grep -vxF "$known_fail" | tr '\n' ' ')"
    fi
    if [ "$passes" -ge "$min_pass" ] && [ -z "$unexpected" ]; then
        printf '  [PASS] %-14s pass=%s fail=%s%s\n' "$name" "$passes" "$fails" \
            "$( [ "$fails" -gt 0 ] && printf ' (基线已知: %s)' "$known_fail" )"
        pass=$((pass + 1))
    else
        printf '  [FAIL] %-14s pass=%s (期望 >=%s) fail=%s 新增失败: %s\n' \
            "$name" "$passes" "$min_pass" "$fails" "${unexpected:-无(但 pass 数下降)}"
        fail=$((fail + 1))
        failed_names+=("$name")
    fi
}

run_one() {
    local name="$1"
    local bin="$2"
    shift 2
    if [ -n "$FILTER" ] && [[ "$name" != *"$FILTER"* ]]; then
        return 0
    fi
    if [ ! -x "$bin" ]; then
        printf '  [SKIP] %-14s 未编译（%s 不存在）\n' "$name" "$bin"
        skip=$((skip + 1))
        return 0
    fi
    local log
    log="$(GN_HEADLESS_FRAMES="$FRAMES" timeout 300 "$bin" "$@" 2>&1)"
    local code=$?
    if [ $code -eq 0 ]; then
        printf '  [PASS] %-14s %s 帧\n' "$name" "$FRAMES"
        pass=$((pass + 1))
    elif [ $code -eq 124 ]; then
        printf '  [FAIL] %-14s 超时（300s）\n' "$name"
        printf '%s\n' "$log" | tail -5 | sed 's/^/         /'
        fail=$((fail + 1))
        failed_names+=("$name")
    else
        printf '  [FAIL] %-14s 退出码 %s\n' "$name" "$code"
        printf '%s\n' "$log" | tail -8 | sed 's/^/         /'
        fail=$((fail + 1))
        failed_names+=("$name")
    fi
}

echo "==== 示例无窗口冒烟（${FRAMES} 帧/示例）===="

if [ ! -x ./build/headlessCheck ]; then
    echo "  [SKIP] build/headlessCheck 不存在，先 make headlessCheck"
    exit 2
fi

export LD_LIBRARY_PATH=".:$LIB"
export GN_HEADLESS_FRAMES="$FRAMES"

probe
if [ $? -eq 2 ]; then
    echo "  [SKIP] 没有可用的 Vulkan 设备（装 lavapipe 或指定 VK_ICD_FILENAMES）"
    exit 2
fi

# 画面回归（有像素断言，最严格）：自带 4 象限色块 + alpha + 非全黑。
run_one headlessCheck ./build/headlessCheck
# 场景示例：缺资源时只画背景，判据是「跑完不崩」。
run_one cornellBox    ./build/cornellBox
run_one mmdViewer     ./build/mmdViewer
run_one imoScene      ./build/imoScene
# P4 物理场景：纯 CPU（不需要 Vulkan），放在这里是为了和"整仓示例都能跑"一起验。
run_one physicsBoxes  ./build/physicsBoxes
run_one characterDemo ./build/characterDemo
# 管线验收：无窗口跑同一批检查；按基线判（不把已知的 cull.debugLine 当失败）。
# 基线 2026-11-24：P2 蒙皮 6 站 + P3 动画切换 1 站后为 116。
run_baseline gpuCheck      ./build/gpuCheck 124 ""

echo "---- SMOKE SUMMARY: pass=$pass fail=$fail skip=$skip ----"
if [ $fail -ne 0 ]; then
    printf '失败项: %s\n' "${failed_names[*]}"
    exit 1
fi
exit 0
