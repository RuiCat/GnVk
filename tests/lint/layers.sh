#!/bin/bash
# tests/lint/layers.sh — 分层依赖检查
#
# 为什么需要它：架构文档里的「只能向下依赖」如果只写在文档里，三个月后必然失效。
# 这个脚本用 include 路径判断模块层级，把越级依赖列出来。
#
# 两条豁免（是设计决定，不是漏洞）：
#   1. src/core/GN.h —— 单头文件聚合器，设计上就要 include 所有模块。它的存在
#      本身就是"最容易越级"的地方，但拆掉它要改 135 个 .cpp，风险不划算。
#      只对它的**被依赖**不收，对它的**内容**不做检查。
#   2. external/ 下的第三方头 —— 层级概念不适用。
#
# 退出码：
#   0 = 无新增越级（或全部在基线白名单内）
#   1 = 有越级且未列入白名单（仅 --strict）
#   2 = SKIP（不在预期目录）
#
# 用法：
#   tests/lint/layers.sh              # 报告全部越级
#   tests/lint/layers.sh --strict     # 白名单外有任何越级就失败
set -u

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT" || exit 2
[ -d src ] || { echo "[SKIP] 不在仓库根（找不到 src/）"; exit 2; }

STRICT=0
[ "${1:-}" = "--strict" ] && STRICT=1

# ── 层级定义（数字越小越底层）─────────────────────────────
#  L0 platform   系统调用 / 窗口 / GLFW
#  L1 基础设施   core（容器/字符串/日志）/ file
#  L2 资源       graphics 的数学 + 资源（非 _Vulkan）
#  L3 后端       graphics/_Vulkan
#  L4 仿真       graphics 的 3D 仿真（物理/动画/骨骼/空间）
#  L5 框架       ui / input / audio / network / media / mmd
level_of_dir() {
    case "$1" in
        external/*) echo 99 ;;                       # 第三方，不参与判层
        src/platform*) echo 0 ;;
        src/core*|src/file*) echo 1 ;;
        src/graphics/_Vulkan*) echo 3 ;;
        src/graphics*) echo 2 ;;
        src/ui*|src/input*|src/audio*|src/network*|src/media*|src/mmd*) echo 5 ;;
        *) echo 9 ;;
    esac
}

# ── 已知越级（基线白名单）───────────────────────────────
# 格式：<源文件>:<头文件名>  —— 每条都要写清原因，否则不许加。
# 原因一览（按行）：
#   GN_3Dmodel.h/.cpp:GN_Vk.h      渲染实体需要后端类型（B2 前既有）
#   GN_EngineContext.cpp:GN_Vk.h   引擎门面按设计持有渲染上下文（B7-1）
#   GN_EngineContext.cpp:GN_InputContext.h / GN_UiBrowserContext.h / GN_SoundContext.h
#                                  引擎门面按设计持有全部子系统上下文指针（B7-4/B7-6；只取全局实例地址，不调 L5 逻辑）
#   GN_3Dasset.cpp:cgltf.h         资产导入直接解析 glTF（既有）
#   GN_UiPopDlgIO.cpp:GN_EdPlugin.h 编辑器插件接口（既有）
#   GN_RunRec.cpp:GN_FontFT.h      运行记录写字形（既有）
KNOWN_VIOLATIONS='
src/graphics/GN_3Dmodel.h:GN_Vk.h
src/graphics/GN_3Dmodel.cpp:GN_Vk.h
src/graphics/GN_EngineContext.cpp:GN_Vk.h
src/graphics/GN_EngineContext.cpp:GN_InputContext.h
src/graphics/GN_EngineContext.cpp:GN_UiBrowserContext.h
src/graphics/GN_EngineContext.cpp:GN_SoundContext.h
src/graphics/GN_EngineContext.cpp:GN_NetTransContext.h
src/graphics/GN_3Dasset.cpp:cgltf.h
src/ui/GN_UiPopDlgIO.cpp:GN_EdPlugin.h
src/core/GN_RunRec.cpp:GN_FontFT.h
'

is_known() {
    echo "$KNOWN_VIOLATIONS" | grep -qx "$1:$2"
}

echo "==== 分层依赖检查 ===="
> /tmp/layers-new.txt

for src in $(find src -name '*.cpp' -o -name '*.h' | sort); do
    # 聚合器豁免：它按设计就 include 所有模块
    [ "$src" = "src/core/GN.h" ] && continue
    from_level=$(level_of_dir "$src")
    grep -oE '#include "[A-Za-z0-9_/.]+\.h"' "$src" 2>/dev/null | sed 's/#include "//; s/"//' |
    while read -r inc; do
        target=$(find src external/include -name "$(basename "$inc")" 2>/dev/null | head -1)
        [ -z "$target" ] && continue
        to_level=$(level_of_dir "$target")
        [ "$to_level" -eq 99 ] && continue               # 第三方跳过
        if [ "$to_level" -gt "$from_level" ]; then
            if is_known "$src" "$(basename "$inc")"; then
                echo "  [已知] $src (L$from_level) → $(basename "$inc") (L$to_level)"
            else
                echo "  [越级] $src (L$from_level) → $(basename "$inc") (L$to_level)"
                echo "$src:$inc" >> /tmp/layers-new.txt
            fi
        fi
    done
done

new_count=$(wc -l < /tmp/layers-new.txt 2>/dev/null | tr -d ' ')
rm -f /tmp/layers-new.txt

echo "---- 白名单外越级: $new_count ----"
if [ "$new_count" -gt 0 ]; then
    echo "（每条都要么修掉，要么在 KNOWN_VIOLATIONS 里写清原因）"
    [ "$STRICT" -eq 1 ] && exit 1
else
    echo "（尚未纳入阻断；A5 重构完成后改 --strict 进 make ci）"
fi
exit 0
