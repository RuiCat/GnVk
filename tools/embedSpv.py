#!/usr/bin/env python3
# embedSpv.py — 把 .spv 二进制嵌入成 C++ 头文件（生成物，勿手改）。
#
# 为什么需要这个工具（而不是手工复制）：
#   引擎在运行时会**优先**读磁盘上的 assets/shaders/*.spv；读不到才退回内嵌数组。
#   曾经这些内嵌头是"一次性手工生成"的，`make shaders` 只更新 .spv 而不更新它 ⇒
#   改了 .glsl、编译出新的 .spv，程序却仍在跑旧的内嵌代码，表现是"改了完全没效果"。
#   这浪费了整整一轮排查（详见 docs/CHANGELOG.md 的 cube shadow 条目）。
#
# 用法：
#   tools/embedSpv.py --write        # 重新生成所有 *_spv.h
#   tools/embedSpv.py --check        # 只校验一致性（CI 用），不一致返回 1
#
# 约定：
#   * 每行 12 字节，逗号后带空格（与仓库既有文件一致，diff 稳定）；
#   * 首行是 code-modularity 豁免注释（生成物不参与模块度检查）；
#   * 输出与输入**严格幂等** —— 内容没变时重跑不产生 diff。

import os
import sys

SHADER_DIR = os.path.join("assets", "shaders")
HEADER_DIR = os.path.join("src", "graphics", "_Vulkan")

BANNER = "// code-modularity: no-split（机器生成的 SPIR-V 字节数组，拆分无法独立验证）"
BYTES_PER_LINE = 12


def header_path_for(spv_path):
    """assets/shaders/gn3d_pbr.frag.spv → src/graphics/_Vulkan/gn3d_pbr_frag_spv.h

    symbol 名规则：把路径里的 . 和 / 都换成 _（assets_shaders_gn3d_pbr_frag_spv）。
    """
    rel = spv_path.replace(os.sep, "/")
    base = os.path.basename(rel)
    stem = base[: -len(".spv")]                      # gn3d_pbr.frag
    stem = stem.replace(".", "_")                    # gn3d_pbr_frag
    return os.path.join(HEADER_DIR, stem + "_spv.h")


def symbol_for(spv_path):
    rel = spv_path.replace(os.sep, "/")
    return rel.replace("/", "_").replace(".", "_")


def render(spv_path):
    with open(spv_path, "rb") as f:
        data = f.read()
    name = symbol_for(spv_path)
    lines = [BANNER, "unsigned char %s[] = {" % name]
    for i in range(0, len(data), BYTES_PER_LINE):
        chunk = data[i : i + BYTES_PER_LINE]
        lines.append("  " + ", ".join("0x%02x" % b for b in chunk) + ",")
    lines.append("};")
    lines.append("unsigned int %s_len = %d;" % (name, len(data)))
    return "\n".join(lines) + "\n"


def all_spv():
    if not os.path.isdir(SHADER_DIR):
        return []
    out = []
    for name in sorted(os.listdir(SHADER_DIR)):
        if name.endswith(".spv"):
            out.append(os.path.join(SHADER_DIR, name))
    return out


def main(argv):
    mode = "--write" if "--write" in argv else ("--check" if "--check" in argv else None)
    if mode is None:
        sys.stderr.write("用法: tools/embedSpv.py --write | --check\n")
        return 2

    spvs = all_spv()
    if not spvs:
        sys.stderr.write("没有找到任何 .spv（先跑 make shaders）\n")
        return 1

    # **只处理已经有内嵌头的 shader**。绝大多数 .spv 没有内嵌备份（只走磁盘加载），
    # 这里不该要求为它们新建 —— 那会把仓库塞进几 MB 的字节数组。
    changed, stale = [], []
    processed = 0
    for spv in spvs:
        hdr = header_path_for(spv)
        if not os.path.exists(hdr):
            continue
        with open(hdr, "r") as f:
            probe = f.read()
        # 跳过"空数组占位"的头（gn2d 那两个就是这样：明显写着必须从磁盘加载）。
        # 它们不是过期的字节数组，而是**刻意的空实现**，不该被本工具覆盖。
        if "[] = {};" in probe:
            continue
        processed += 1
        expect = render(spv)
        actual = probe
        if actual != expect:
            stale.append(hdr)
            if mode == "--write":
                with open(hdr, "w") as f:
                    f.write(expect)
                changed.append(hdr)

    if processed == 0:
        sys.stderr.write("没有找到任何 *_spv.h（仓库结构变了？）\n")
        return 1

    if mode == "--write":
        if changed:
            print("已更新 %d 个头文件：" % len(changed))
            for h in changed:
                print("  " + h)
        else:
            print("所有内嵌数组都是最新的（%d 个）" % processed)
        return 0

    # --check
    if stale:
        print("[FAIL] %d 个内嵌 spv 头与磁盘 .spv 不一致：" % len(stale))
        for h in stale:
            print("  过期: " + h)
        print("  修法：tools/embedSpv.py --write")
        return 1
    print("[PASS] 内嵌 spv 头与磁盘 .spv 一致（%d 个）" % processed)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
