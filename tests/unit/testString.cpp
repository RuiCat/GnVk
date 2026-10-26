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

TEST_CASE(stringDirName) {
    char dir[384] = {0};
    GN_StrGetDirName("/a/b/c.txt", dir);
    CHECK_STREQ(dir, "/a/b");
}

TEST_CASE(stringToKey) {
    CHECK_STREQ(GN_StrToKey("key=value", '='), "value");
}
