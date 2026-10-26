// 单元测试入口：按注册顺序执行用例并汇总失败数。
#include "testFramework.h"

std::vector<TestCase>& testRegistry() {
    static std::vector<TestCase> registry;
    return registry;
}

int g_assertionFailures = 0;

int main() {
    const std::vector<TestCase> &cases = testRegistry();
    int failedCases = 0;
    for (size_t i = 0; i < cases.size(); ++i) {
        const int before = g_assertionFailures;
        cases[i].fn();
        const bool ok = (g_assertionFailures == before);
        std::printf("%s %s\n", ok ? "PASS" : "FAIL", cases[i].name);
        if (!ok) ++failedCases;
    }
    std::printf("\n%d cases, %d failed, %d assertions failed\n",
                (int)cases.size(), failedCases, g_assertionFailures);
    return failedCases == 0 ? 0 : 1;
}
