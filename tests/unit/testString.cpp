// 字符串工具单测：文件名/扩展名、分割与合并、替换、裁剪。
#include "testFramework.h"
#include <cstring>
#include <GN.h>

TEST_CASE(stringCharNum) {
    CHECK_EQ(GN_StrGetCharNum("a,b,c", ','), 2);
    CHECK_EQ(GN_StrGetCharNum("abc", ','), 0);
}

TEST_CASE(stringExtName) {
    CHECK_STREQ(GN_StrGetExtName("dir/file.cpp"), "cpp");
    CHECK_STREQ(GN_StrGetExtName("a.b"), "b");
}

TEST_CASE(stringExtNameMissing) {
    CHECK(GN_StrGetExtName("noext") == 0);
}

TEST_CASE(stringFileName) {
    CHECK_STREQ(GN_StrGetFileName("/a/b/c.txt"), "c.txt");
    CHECK_STREQ(GN_StrGetFileName("plain.txt"), "plain.txt");
}

TEST_CASE(stringHasNameTrue) {
    CHECK(GN_StrHasName("beta", "alpha beta gamma", ' '));
    CHECK(GN_StrHasName("alpha", "alpha beta gamma", ' '));
}

TEST_CASE(stringHasNameFalse) {
    CHECK(!GN_StrHasName("delta", "alpha beta gamma", ' '));
}

TEST_CASE(stringReplace) {
    char buffer[] = "a-b-c";
    GN_StrReplace(buffer, '-', '+');
    CHECK_STREQ(buffer, "a+b+c");
}

TEST_CASE(stringCutSpace) {
    char buffer[] = "  hi  ";
    char *trimmed = GN_StrCutSpaceFL(buffer);
    CHECK_STREQ(trimmed, "hi");
}

TEST_CASE(stringCutSpaceEmpty) {
    char buffer[] = "";
    char *trimmed = GN_StrCutSpaceFL(buffer);
    CHECK_STREQ(trimmed, "");
}

TEST_CASE(stringCutSpaceWhitespaceOnly) {
    char buffer[] = "   ";
    char *trimmed = GN_StrCutSpaceFL(buffer);
    CHECK_STREQ(trimmed, "");
}

TEST_CASE(stringVisualNum) {
    CHECK_EQ(GN_StrGetVisualNum("abc"), 3);
    CHECK_EQ(GN_StrGetVisualNum("a\nb"), 2);
    CHECK_EQ(GN_StrGetVisualNum("ab\ncd"), 4);
}

TEST_CASE(stringSplitCount) {
    unsigned int count = 0;
    char **parts = GN_StrSplitToData(count, "alpha,beta,gamma", ',', 0);
    CHECK_EQ(count, 3u);
    for (unsigned int i = 0; i < count; ++i) delete[] parts[i];
    delete[] parts;
}

TEST_CASE(stringSplitContent) {
    unsigned int count = 0;
    char **parts = GN_StrSplitToData(count, "alpha,beta,gamma", ',', 0);
    CHECK_EQ(count, 3u);
    if (count == 3) {
        CHECK_STREQ(parts[0], "alpha");
        CHECK_STREQ(parts[1], "beta");
        CHECK_STREQ(parts[2], "gamma");
    }
    for (unsigned int i = 0; i < count; ++i) delete[] parts[i];
    delete[] parts;
}

TEST_CASE(stringUnSplitRoundTrip) {
    const char *parts[3] = {"alpha", "beta", "gamma"};
    char *joined = GN_StrUnSplitToData(parts, 3, '|');
    CHECK_STREQ(joined, "alpha|beta|gamma");
    delete[] joined;
}


// B5f：GN_StrRemoveSegment 的段删除语义（含同缓冲区重叠拷贝，memmove 路径）。
TEST_CASE(stringRemoveSegmentBasic) {
    char buf[64] = "a,b,c,d";
    CHECK_EQ(GN_StrRemoveSegment(buf, ',', "b"), 1);
    CHECK_STREQ(buf, "a,c,d");
}

TEST_CASE(stringRemoveSegmentSubstringMatch) {
    char buf[64] = "a,bb,c";
    CHECK_EQ(GN_StrRemoveSegment(buf, ',', "b"), 1);
    CHECK_STREQ(buf, "a,c");
}

TEST_CASE(stringRemoveSegmentTailAndWhole) {
    char tail[64] = "a,b,c";
    CHECK_EQ(GN_StrRemoveSegment(tail, ',', "c"), 1);
    CHECK_STREQ(tail, "a,b,"); // 现有语义：尾段删除后保留前一个分隔符
    char whole[64] = "b";
    CHECK_EQ(GN_StrRemoveSegment(whole, ',', "b"), -1);
    CHECK_STREQ(whole, "");
}

TEST_CASE(stringRemoveSegmentTailCopyWithinBuffer) {
    // 触发 pos0/pos1 同缓冲区重叠拷贝（旧实现是 strcpy，UB；现为 memmove）。
    char buf[64] = "b,a,b";
    CHECK_EQ(GN_StrRemoveSegment(buf, ',', "b"), 1);
    CHECK_STREQ(buf, "a,b");
}


// B5f：带容量上限的拷贝（UI 固定缓冲的截断语义）。
TEST_CASE(stringCopyToLimit) {
    char buf[8];
    std::memset(buf, 'x', sizeof(buf));
    CHECK_EQ(GN_StrCopyToLimit(buf, 8, "abc"), 3);
    CHECK_STREQ(buf, "abc");
    CHECK_EQ(GN_StrCopyToLimit(buf, 8, "1234567890"), 7);
    CHECK_STREQ(buf, "1234567");
    CHECK_EQ(GN_StrCopyToLimit(buf, 0, "a"), 0);
    CHECK_EQ(GN_StrCopyToLimit(0, 8, "a"), 0);
    CHECK_EQ(GN_StrCopyToLimit(buf, 1, "abc"), 0);
    CHECK_STREQ(buf, "");
    CHECK_EQ(GN_StrCopyToLimit(buf, 8, 0), 0);
    CHECK_STREQ(buf, "");
}


// B5f 收尾：带容量上限的 _s 变体。
TEST_CASE(stringUnSplitCapped) {
    char buf[8];
    const char *parts[3] = {"alpha", "beta", "gamma"};
    const unsigned int n = GN_StrUnSplit_s(buf, (int)sizeof(buf), parts, 3, '|');
    CHECK_EQ(n, 7u);
    CHECK_STREQ(buf, "alpha|b");
    const unsigned int n0 = GN_StrUnSplit_s(buf, 0, parts, 3, '|');
    CHECK_EQ(n0, 0u);
}

TEST_CASE(stringExtractCapped) {
    char buf[4];
    CHECK_EQ(GN_StrExtractFromMultiItem_s(buf, 4, "aa,bb,cc", 1, ','), 2);
    CHECK_STREQ(buf, "bb");
    CHECK_EQ(GN_StrExtractFromMultiItem_s(buf, 4, "aa,bbbb,cc", 1, ','), 3);
    CHECK_STREQ(buf, "bbb");
    CHECK_EQ(GN_StrExtractFromMultiItem_s(buf, 4, "aa", 3, ','), -1);
}

TEST_CASE(stringDirName) {
    char dir[384] = {0};
    GN_StrGetDirName("/a/b/c.txt", dir);
    CHECK_STREQ(dir, "/a/b");
}

TEST_CASE(stringToKey) {
    CHECK_STREQ(GN_StrToKey("key=value", '='), "value");
}

// ── 2026-11-26 缺陷修复回归：GN_StrExtractToData 闭合符分支旧写法重复判 key，
// 永远进不来，提取结果全是空串。──
TEST_CASE(stringExtractToDataClosingKey) {
    char buf[] = "{a} {bc} {d}";
    P<P<char>> r = GN_StrExtractToData(buf, '{', '}');
    CHECK_EQ(r.Num(), 3u);
    CHECK_STREQ(r[0], "a");
    CHECK_STREQ(r[1], "bc");
    CHECK_STREQ(r[2], "d");
}

TEST_CASE(stringExtractToDataUnclosed) {
    char buf[] = "{a";
    P<P<char>> r = GN_StrExtractToData(buf, '{', '}');
    CHECK_EQ(r.Num(), 0u); // 未闭合：计数循环提前返回，不产出半个结果
}

// ── P0.1（DA-1126-14）：全角→半角转换旧实现是死代码（signed char 与 0xa1/0xa3 比较恒假）──
TEST_CASE(stringSymbolToSingleCharFullwidth) {
    char out[64];
    // ＡＢＣ（GBK A3C1 A3C2 A3C3）→ ABC
    GN_StrSymbolToSingleChar("\xa3\xc1\xa3\xc2\xa3\xc3", out);
    CHECK_STREQ(out, "ABC");
    // ０１２（A3B0 A3B1 A3B2）→ 012
    GN_StrSymbolToSingleChar("\xa3\xb0\xa3\xb1\xa3\xb2", out);
    CHECK_STREQ(out, "012");
    // 标点：“”‘’《》＄。～
    GN_StrSymbolToSingleChar("\xa1\xb0\xa1\xb1\xa1\xae\xa1\xaf\xa1\xb6\xa1\xb7\xa1\xe7\xa1\xa3\xa1\xab", out);
    CHECK_STREQ(out, "\"\"''<>$.~");
    // 混合：ASCII 与普通中文原样通过
    GN_StrSymbolToSingleChar("a\xd6\xd0z", out);
    CHECK_STREQ(out, "a\xd6\xd0z");
}

TEST_CASE(stringSymbolToSingleCharTruncated) {
    char out[8];
    // 高字节领头但没有尾字节：安全终止，不越过终结符读
    const int n = GN_StrSymbolToSingleChar("\xa3", out);
    CHECK_EQ(n, 0);
    CHECK_STREQ(out, "");
}
