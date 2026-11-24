// testLog.cpp — 日志通道分层：GN_Log 只写文件、GN_LogErr 双写文件与 stderr。
//
// 为什么值得测：这两条通道的分工是「用户当场看不看得见」的关键。一旦有人在实现里把
// GN_Log 改成 printf（或反过来让 GN_LogErr 只写文件），CLI 程序的输出纪律就被破坏，
// 而这种错不会让编译或功能测试失败，只能靠这里盯住。
#include "testFramework.h"

#include <GN.h>

#include <cstdio>
#include <cstring>
#include <string>

namespace {

// 读整个文件到字符串；不存在返回空串。
std::string readAll(const char *path) {
    FILE *f = std::fopen(path, "rb");
    if (!f) return std::string();
    std::fseek(f, 0, SEEK_END);
    const long size = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    std::string out;
    if (size > 0) {
        out.resize((size_t)size);
        const size_t read = std::fread(&out[0], 1, (size_t)size, f);
        out.resize(read);
    }
    std::fclose(f);
    return out;
}

// 重定向 fd 到文件，执行 fn，恢复。返回文件内容。
template <typename Fn> std::string captureFd(int fd, const char *path, Fn fn) {
    std::fflush(nullptr);
    const int saved = dup(fd);
    FILE *f = std::fopen(path, "wb");
    dup2(fileno(f), fd);
    fn();
    std::fflush(nullptr);
    dup2(saved, fd);
    close(saved);
    std::fclose(f);
    return readAll(path);
}

} // namespace

// GN_Log 只写 RunLog.txt，不碰 stdout（否则 CLI 的机器可读输出会被搅乱）。
TEST_CASE(logWritesFileNotStdout) {
    GN_RunErrorOutputOn = true;
    const char *kName = "test-log-plain.txt";
    GN_RunErrorFileName = (char *)kName;
    std::remove(kName);

    const std::string out = captureFd(1, "test-log-stdout.txt", []() {
        GN_Log("PLAIN_%d\n", 1);
    });
    const std::string fileText = readAll(kName);

    CHECK(out.find("PLAIN_1") == std::string::npos);      // 不进 stdout
    CHECK(fileText.find("PLAIN_1") != std::string::npos); // 进文件
    std::remove("test-log-stdout.txt");
    std::remove(kName);
}

// GN_LogErr 两边都写：文件里有、stderr 里也有，stdout 里没有。
TEST_CASE(logErrWritesFileAndStderr) {
    GN_RunErrorOutputOn = true;
    const char *kName = "test-log-err.txt";
    GN_RunErrorFileName = (char *)kName;
    std::remove(kName);

    const std::string errText = captureFd(2, "test-log-stderr.txt", []() {
        GN_LogErr("ERR_%d_%s\n", 7, "x");
    });
    const std::string fileText = readAll(kName);
    const std::string outText = captureFd(1, "test-log-stdout2.txt", []() {
        GN_LogErr("ERR_SECOND\n");
    });

    CHECK(errText.find("ERR_7_x") != std::string::npos);   // stderr 有
    CHECK(fileText.find("ERR_7_x") != std::string::npos);  // 文件有
    CHECK(outText.find("ERR_SECOND") == std::string::npos); // stdout 没有
    std::remove("test-log-stderr.txt");
    std::remove("test-log-stdout2.txt");
    std::remove(kName);
}

// 超长消息（超过内部栈缓冲）也要完整进文件，不能被截断。
TEST_CASE(logErrLongMessageNotTruncated) {
    GN_RunErrorOutputOn = true;
    const char *kName = "test-log-long.txt";
    GN_RunErrorFileName = (char *)kName;
    std::remove(kName);

    std::string big(3000, 'A');
    captureFd(2, "test-log-stderr-long.txt", [&big]() {
        GN_LogErr("LONG:%s:END\n", big.c_str());
    });
    const std::string fileText = readAll(kName);
    CHECK(fileText.find("LONG:") != std::string::npos);
    CHECK(fileText.find(big) != std::string::npos); // 3000 个 A 完整保留
    CHECK(fileText.find(":END") != std::string::npos);
    std::remove("test-log-stderr-long.txt");
    std::remove(kName);
}

// GN_RunErrorOutputOn=false 时两条通道都不应写文件（保持既有语义）。
TEST_CASE(logDisabledWritesNothing) {
    GN_RunErrorOutputOn = false;
    const char *kName = "test-log-off.txt";
    GN_RunErrorFileName = (char *)kName;
    std::remove(kName);

    captureFd(2, "test-log-stderr-off.txt", []() {
        GN_Log("OFF_PLAIN\n");
    });
    const std::string fileText = readAll(kName);
    CHECK(fileText.empty()); // 关闭时文件不产生

    GN_RunErrorOutputOn = true; // 恢复，避免影响后续用例
    std::remove("test-log-stderr-off.txt");
    std::remove(kName);
}
