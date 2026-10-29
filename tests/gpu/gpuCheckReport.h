// GPU 验收结果收集：每条检查输出 PASS/FAIL/SKIP，并在结尾汇总失败数。
#pragma once
#ifndef _GN_GPU_CHECK_REPORT_H_
#define _GN_GPU_CHECK_REPORT_H_

namespace GpuCheck {

void ReportPass(const char *name, const char *detail);
void ReportFail(const char *name, const char *detail);
void ReportSkip(const char *name, const char *detail);

int GetPassCount();
int GetFailCount();
int GetSkipCount();
void PrintSummary();

} // namespace GpuCheck
#endif // _GN_GPU_CHECK_REPORT_H_
