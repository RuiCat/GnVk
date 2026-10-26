// 最小单元测试框架：用例注册、断言计数与失败汇总（无第三方依赖）。
#ifndef GN_TEST_FRAMEWORK_H
#define GN_TEST_FRAMEWORK_H

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

struct TestCase {
    const char *name;
    void (*fn)();
};

std::vector<TestCase>& testRegistry();
extern int g_assertionFailures;

#define TEST_CASE(caseName)                                                     \
    static void caseName();                                                     \
    static const bool caseName##Registered __attribute__((unused)) =            \
        (testRegistry().push_back(TestCase{#caseName, caseName}), true);        \
    static void caseName()

#define CHECK(cond)                                                             \
    do {                                                                        \
        if (!(cond)) {                                                          \
            std::fprintf(stderr, "  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            ++g_assertionFailures;                                              \
        }                                                                       \
    } while (0)

#define CHECK_EQ(actual, expected)                                              \
    do {                                                                        \
        const auto checkEqA = (actual);                                         \
        const auto checkEqB = (expected);                                       \
        if (!(checkEqA == checkEqB)) {                                          \
            std::fprintf(stderr, "  FAIL %s:%d: %s == %s\n",                    \
                         __FILE__, __LINE__, #actual, #expected);               \
            ++g_assertionFailures;                                              \
        }                                                                       \
    } while (0)

#define CHECK_NEAR(actual, expected, eps)                                       \
    do {                                                                        \
        const double checkNearA = (double)(actual);                             \
        const double checkNearB = (double)(expected);                           \
        const double checkNearE = (double)(eps);                                \
        if (std::fabs(checkNearA - checkNearB) > checkNearE) {                  \
            std::fprintf(stderr, "  FAIL %s:%d: %s = %f, 期望 %f (eps %f)\n",   \
                         __FILE__, __LINE__, #actual, checkNearA, checkNearB,   \
                         checkNearE);                                           \
            ++g_assertionFailures;                                              \
        }                                                                       \
    } while (0)

#define CHECK_STREQ(actual, expected)                                           \
    do {                                                                        \
        const char *checkStrA = (actual);                                       \
        const char *checkStrB = (expected);                                     \
        if (!checkStrA || !checkStrB || std::strcmp(checkStrA, checkStrB) != 0) { \
            std::fprintf(stderr, "  FAIL %s:%d: \"%s\" != \"%s\"\n",            \
                         __FILE__, __LINE__, checkStrA ? checkStrA : "(null)",  \
                         checkStrB ? checkStrB : "(null)");                     \
            ++g_assertionFailures;                                              \
        }                                                                       \
    } while (0)

#endif // GN_TEST_FRAMEWORK_H
