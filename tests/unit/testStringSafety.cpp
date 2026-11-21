// testStringSafety.cpp — 带容量上限的字符串 API（B5f）：截断边界、空输入、UTF-8 字节语义。
#include "testFramework.h"

#include <GN.h>
#include <cstring>

TEST_CASE(stringCopyToLimitBoundaries) {
    char buf[8];
    std::memset(buf, 'x', sizeof(buf));
    // 精确容纳：cap = len + 1
    CHECK_EQ(GN_StrCopyToLimit(buf, 6, "abcde"), 5);
    CHECK_STREQ(buf, "abcde");
    // 少一个字节：截断到 cap-1
    CHECK_EQ(GN_StrCopyToLimit(buf, 5, "abcde"), 4);
    CHECK_STREQ(buf, "abcd");
    // cap=1 只写 NUL
    CHECK_EQ(GN_StrCopyToLimit(buf, 1, "abc"), 0);
    CHECK_STREQ(buf, "");
    // 非法参数不写也不崩
    CHECK_EQ(GN_StrCopyToLimit(buf, -1, "abc"), 0);
    CHECK_EQ(GN_StrCopyToLimit(0, 5, "abc"), 0);
    // src 为空指针按空串处理
    CHECK_EQ(GN_StrCopyToLimit(buf, 8, 0), 0);
    CHECK_STREQ(buf, "");
}

TEST_CASE(stringCopyToLimitUtf8Bytes) {
    // 接口是字节语义：按 cap 截断可能把多字节字符切断，但必须保证 NUL 结尾。
    char buf[8];
    const char *zh = "\xE4\xB8\xAD\xE6\x96\x87"; // 中文（6 字节）
    CHECK_EQ(GN_StrCopyToLimit(buf, 8, zh), 6);
    CHECK_EQ(std::strlen(buf), 6u);
    CHECK_EQ(GN_StrCopyToLimit(buf, 2, zh), 1); // 只保留第一个字节
    CHECK_EQ(std::strlen(buf), 1u);
    CHECK_EQ(buf[1], '\0');
}

TEST_CASE(stringUnSplitCappedEdgeCases) {
    char buf[16];
    // 空数组 / 空 strn
    CHECK_EQ(GN_StrUnSplit_s(buf, (int)sizeof(buf), 0, 0, '|'), 0u);
    CHECK_STREQ(buf, "");
    // 单个元素
    const char *one[1] = {"abc"};
    CHECK_EQ(GN_StrUnSplit_s(buf, (int)sizeof(buf), one, 1, '|'), 3u);
    CHECK_STREQ(buf, "abc");
    // 空元素参与拼接：只保留分隔符
    const char *empty[2] = {"", ""};
    CHECK_EQ(GN_StrUnSplit_s(buf, (int)sizeof(buf), empty, 2, '|'), 1u);
    CHECK_STREQ(buf, "|");
    // 分隔符正好落在末尾：不能把半个分隔符写出去
    const char *two[2] = {"abc", "de"};
    CHECK_EQ(GN_StrUnSplit_s(buf, 4, two, 2, '|'), 3u);
    CHECK_STREQ(buf, "abc");
    // cap=1
    CHECK_EQ(GN_StrUnSplit_s(buf, 1, two, 2, '|'), 0u);
    CHECK_STREQ(buf, "");
    // 空指针元素按空串处理
    const char *withNull[2] = {"a", 0};
    CHECK_EQ(GN_StrUnSplit_s(buf, (int)sizeof(buf), withNull, 2, '|'), 2u);
    CHECK_STREQ(buf, "a|");
}

TEST_CASE(stringExtractCappedEdgeCases) {
    char buf[8];
    // 第 0 段 / 中间段 / 末段（无尾部分隔符）
    CHECK_EQ(GN_StrExtractFromMultiItem_s(buf, (int)sizeof(buf), "aa,bb,cc", 0, ','), 2);
    CHECK_STREQ(buf, "aa");
    CHECK_EQ(GN_StrExtractFromMultiItem_s(buf, (int)sizeof(buf), "aa,bb,cc", 2, ','), 2);
    CHECK_STREQ(buf, "cc");
    // 越界 / 非法参数
    CHECK_EQ(GN_StrExtractFromMultiItem_s(buf, (int)sizeof(buf), "aa,bb", 5, ','), -1);
    CHECK_EQ(GN_StrExtractFromMultiItem_s(buf, (int)sizeof(buf), "aa,bb", -1, ','), -1);
    CHECK_EQ(GN_StrExtractFromMultiItem_s(0, 8, "aa", 0, ','), -1);
    CHECK_EQ(GN_StrExtractFromMultiItem_s(buf, 0, "aa", 0, ','), -1);
    // 截断：cap=3 最多保留 2 字节
    CHECK_EQ(GN_StrExtractFromMultiItem_s(buf, 3, "aa,bbbb", 1, ','), 2);
    CHECK_STREQ(buf, "bb");
}