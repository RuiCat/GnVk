// testImageNameConvert.cpp — 2D 场景图像名转换的容量语义（B5i）：
// GT_ConvertImageNameForLoad_s 按 cap 截断且不越界；旧 ABI 包装行为不变（0000 去尾 / 追加 .png）。
#include "testFramework.h"

#include <GN.h>

#include <cstring>

// GN_API 导出但无公共头声明（原型只在 GN_Frame.cpp 内部声明）。
int GT_ConvertImageNameForLoad_s(char *img_name_out, int cap, const char *img_name, unsigned int name_len);
void GT_ConvertImageNameForLoad(char *img_name_out, const char *img_name, unsigned int name_len);

TEST_CASE(imageNameConvertAppendsPng) {
    char out[32] = {0};
    CHECK_EQ(GT_ConvertImageNameForLoad_s(out, (int)sizeof(out), "abc", 3), 7);
    CHECK_STREQ(out, "abc.png");
}

TEST_CASE(imageNameConvertStrips0000) {
    char out[32] = {0};
    CHECK_EQ(GT_ConvertImageNameForLoad_s(out, (int)sizeof(out), "walk0000", 8), 4);
    CHECK_STREQ(out, "walk");
}

// 容量不足：只写 cap-1 字节 + NUL，缓冲外的哨兵必须保持不变。
TEST_CASE(imageNameConvertRespectsCap) {
    char buf[8];
    std::memset(buf, 'X', sizeof(buf));
    const int n = GT_ConvertImageNameForLoad_s(buf + 1, 4, "abcdefgh", 8);
    CHECK_EQ(n, 3);
    CHECK_STREQ(buf + 1, "abc");
    CHECK_EQ(buf[0], 'X');
    CHECK_EQ(buf[5], 'X');
    CHECK_EQ(buf[6], 'X');
}

// cap<=0 / 空指针不得写入。
TEST_CASE(imageNameConvertInvalidArgs) {
    char out[8] = {0};
    CHECK_EQ(GT_ConvertImageNameForLoad_s(out, 0, "abc", 3), 0);
    CHECK_EQ(out[0], 0);
    CHECK_EQ(GT_ConvertImageNameForLoad_s(0, 8, "abc", 3), 0);
}

// 旧 ABI 包装：正常输入与旧行为一致。
TEST_CASE(imageNameConvertLegacyWrapper) {
    char out[32] = {0};
    GT_ConvertImageNameForLoad(out, "abc", 3);
    CHECK_STREQ(out, "abc.png");
    GT_ConvertImageNameForLoad(out, "x0000", 5);
    CHECK_STREQ(out, "x");
}
