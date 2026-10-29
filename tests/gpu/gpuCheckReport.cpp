// GPU 验收结果收集的实现：计数与统一输出格式。
#include "gpuCheckReport.h"

#include <cstdio>

namespace GpuCheck {
namespace {
int sPass = 0;
int sFail = 0;
int sSkip = 0;
} // namespace

void ReportPass(const char *name, const char *detail) {
    ++sPass;
    std::printf("[PASS] %-28s %s\n", name ? name : "-", detail ? detail : "");
}

void ReportFail(const char *name, const char *detail) {
    ++sFail;
    std::printf("[FAIL] %-28s %s\n", name ? name : "-", detail ? detail : "");
}

void ReportSkip(const char *name, const char *detail) {
    ++sSkip;
    std::printf("[SKIP] %-28s %s\n", name ? name : "-", detail ? detail : "");
}

int GetPassCount() { return sPass; }
int GetFailCount() { return sFail; }
int GetSkipCount() { return sSkip; }

void PrintSummary() {
    std::printf("---- GPU CHECK SUMMARY: pass=%d fail=%d skip=%d ----\n", sPass, sFail, sSkip);
}

} // namespace GpuCheck
