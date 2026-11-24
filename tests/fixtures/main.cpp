// tests/fixtures/main.cpp — 把程序化资产落盘（make fixtures → .local/fixtures/）。
#include "fixtures.h"

#include <cstdio>

int main(int argc, char **argv) {
    const char *dir = (argc > 1) ? argv[1] : ".local/fixtures";
    if (!fixture::writeFixtures(dir)) {
        std::fprintf(stderr, "[fixtures] 写入失败: %s\n", dir);
        return 1;
    }
    std::printf("[fixtures] 已写入 %s（bone3/bone129/cube_skin）\n", dir);
    return 0;
}
