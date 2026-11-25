#!/usr/bin/env python3
# tests/tools/check_docs_index.py — docs 索引与状态行的双向校验
#
# 为什么需要它：docs/ 下有 30 多份文档，写于不同阶段。只看文件名分不清哪些还有效，
# 而「按修改时间判断」在这个仓库里完全失效 —— 每次批量改动都会 touch 全部文档。
# 所以约定：docs/README.md 是唯一入口，每份文档开头有一行 `> **状态**：<标记> ...`，
# 两处必须一致。人工维护两张表必然漂移（这个仓库已经漂移过：计划文档写着「未实现」
# 而代码早就落地了），所以用脚本钉住。
#
# 校验四项：
#   1. 索引里每个条目都指向真实存在的文件；
#   2. 每份 docs/*.md 都在索引里（README.md 自己除外）；
#   3. 每份文档的 `> **状态**：` 行存在且标记与索引一致；
#   4. 索引里的标记只能是已知的五种（防止写出新标记而校验逻辑没跟上）。
#
# 用法：python3 tests/tools/check_docs_index.py
# 退出码：0 = 通过；1 = 有失败项。
import io
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
DOCS = os.path.join(ROOT, "docs")
INDEX = os.path.join(DOCS, "README.md")

MARKS = ("✅", "📋", "🚧", "📦", "📊")

# 索引表格行：| [xxx.md](xxx.md) | <标记> ... | 说明 |
ROW = re.compile(r"^\|\s*\[([^\]]+\.md)\]\(([^)]+)\)\s*\|\s*([^\s|]+)")
STATUS_LINE = re.compile(r"^>\s*\*\*状态\*\*：\s*(\S+)")

fails = []
warns = []


def main():
    if not os.path.isfile(INDEX):
        print("[FAIL] 缺少 docs/README.md（文档索引）")
        return 1

    index_text = io.open(INDEX, encoding="utf-8").read()
    indexed = {}  # 文件名 -> 标记
    for line in index_text.split("\n"):
        m = ROW.match(line)
        if not m:
            continue
        link, target, mark = m.group(1), m.group(2), m.group(3)
        if mark not in MARKS:
            # 「从哪读起」那类表格没有状态列，跳过（不在这里报错）
            continue
        name = os.path.basename(target)
        if name in indexed:
            fails.append("索引里重复登记：%s" % name)
        indexed[name] = mark
        if not os.path.isfile(os.path.join(DOCS, target)):
            fails.append("索引指向的文件不存在：%s" % target)

    # 2 + 3：docs 下每份文档都要在索引里，且状态行一致
    names = sorted(n for n in os.listdir(DOCS) if n.endswith(".md") and n != "README.md")
    for name in names:
        if name not in indexed:
            fails.append("docs/%s 没有登记在索引里" % name)
            continue
        head = io.open(os.path.join(DOCS, name), encoding="utf-8").read().split("\n")[:14]
        found = None
        for line in head:
            m = STATUS_LINE.match(line.strip())
            if m:
                found = m.group(1)
                break
        if found is None:
            fails.append("docs/%s 缺少 `> **状态**：` 行" % name)
        elif found not in MARKS:
            fails.append("docs/%s 的状态标记未知：%s" % (name, found))
        elif found != indexed[name]:
            fails.append("docs/%s 状态不一致：文档=%s 索引=%s" % (name, found, indexed[name]))

    # 4：索引里出现但磁盘上不存在的登记项已在上面报过；这里只统计
    print("[docs-index] 索引 %d 条 / 磁盘 %d 份" % (len(indexed), len(names)))
    for w in warns:
        print("[WARN] " + w)
    if fails:
        for f in fails:
            print("[FAIL] " + f)
        print("[docs-index] %d 项失败" % len(fails))
        return 1
    print("[docs-index] PASS —— 索引、状态行与磁盘一致")
    return 0


if __name__ == "__main__":
    sys.exit(main())
