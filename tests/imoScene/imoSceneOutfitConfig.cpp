// tests/imoScene/imoSceneOutfitConfig.cpp — 默认服装组合名单与整数环境变量的解析。
//
// 单独成一个文件的原因：这两个函数**只吃字符串、只吐字符串/整数**，
// 不依赖 cMODEL、不依赖输入映射，因此可以直接被单测链接进去验证。
// 它们各自曾经有一个静默错误（见 docs/CHANGELOG.md 与 tests/unit/testOutfitDefault.cpp）：
//   * `"hair"` 全文件搜索会误命中 hidden 名单里的同名槽；
//   * `atoi("abc")` 等于 0，把"全部显示"悄悄变成"选第一套"。

#include "imoSceneOutfit.h"

#include <cstdio>
#include <cstdlib>
#include <string>

namespace imoScene {

namespace {

// 读整个文件；失败返回 false（不抛异常、不打日志，调用方决定说什么）。
bool readWholeFile(const char *path, std::string &out) {
    std::FILE *file = std::fopen(path, "rb");
    if (file == 0) {
        return false;
    }
    char buffer[4096];
    bool ok = true;
    while (true) {
        const size_t read = std::fread(buffer, 1, sizeof(buffer), file);
        if (read == 0) {
            ok = (std::ferror(file) == 0);
            break;
        }
        out.append(buffer, read);
    }
    std::fclose(file);
    return ok;
}

// 从 [begin, end) 区间里取出所有双引号字符串。
void collectQuotedStrings(const std::string &text, size_t begin, size_t end,
                          std::vector<std::string> &out) {
    size_t cursor = begin;
    while (cursor < end) {
        const size_t quote = text.find('"', cursor);
        if (quote == std::string::npos || quote >= end) {
            break;
        }
        const size_t close = text.find('"', quote + 1);
        if (close == std::string::npos || close >= end) {
            break;
        }
        out.push_back(text.substr(quote + 1, close - quote - 1));
        cursor = close + 1;
    }
}

} // namespace

sOUTFIT_DEFAULT::sOUTFIT_DEFAULT() : aHair(-1) {}

// ═══════════════════════════════════════════════════════════
//  JSON 名单
// ═══════════════════════════════════════════════════════════
// 只需 `hidden` 数组与 `hair` 两个字段，所以不引入 JSON 库；
// 但解析要守住两件事（旧实现都没守）：
//   * `"hair"` 只在 `"hidden"` 数组**之后**找，避免名单里恰好有叫 hair 的槽被误命中；
//   * 数值用 strtol 并校验整串，读不出数字就保持 -1（未指定），不给个看似合理的 0。
bool parseOutfitDefaultText(const std::string &text, sOUTFIT_DEFAULT &out) {
    out.aHidden.clear();
    out.aHair = -1;
    const size_t key = text.find("\"hidden\"");
    if (key == std::string::npos) {
        return false;
    }
    const size_t begin = text.find('[', key);
    if (begin == std::string::npos) {
        return false;
    }
    const size_t end = text.find(']', begin);
    if (end == std::string::npos) {
        return false; // '[' 没有配对的 ']'：不猜，直接失败
    }
    collectQuotedStrings(text, begin, end, out.aHidden);

    // "hair" 只在 hidden 数组之后找：名单里若真有叫 hair 的槽，不会被当成字段名。
    const size_t hairKey = text.find("\"hair\"", end);
    if (hairKey != std::string::npos) {
        const size_t colon = text.find(':', hairKey);
        if (colon != std::string::npos) {
            const char *start = text.c_str() + colon + 1;
            char *stop = 0;
            const long value = std::strtol(start, &stop, 10);
            // 校验：必须真的解析出数字，且后面只剩空白或逗号/花括号。
            bool tailOk = (stop != start);
            for (const char *p = stop; tailOk && *p != '\0'; ++p) {
                if (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n' || *p == ',' ||
                    *p == '}') {
                    continue;
                }
                tailOk = false;
            }
            if (tailOk && value >= 0 && value <= 100000) {
                out.aHair = static_cast<int32_t>(value);
            }
        }
    }
    return true;
}

bool parseOutfitDefaultFile(const char *path, sOUTFIT_DEFAULT &out) {
    std::string text;
    if (path == 0 || !readWholeFile(path, text)) {
        out.aHidden.clear();
        out.aHair = -1;
        return false;
    }
    return parseOutfitDefaultText(text, out);
}

// ═══════════════════════════════════════════════════════════
//  严格整数（环境变量）
// ═══════════════════════════════════════════════════════════
// 旧实现用 std::atoi：`IMO_SCENE_HAIR=abc` 得到 0（选第一套），
// 而用户想表达的显然是"全部显示"（-1），并且没有任何提示。
bool parseIntegerStrict(const char *text, int32_t &out) {
    if (text == 0) {
        return false;
    }
    const char *begin = text;
    while (*begin == ' ' || *begin == '\t') {
        ++begin;
    }
    char *stop = 0;
    const long value = std::strtol(begin, &stop, 10);
    if (stop == begin) {
        return false; // 一个数字都没读到
    }
    while (*stop == ' ' || *stop == '\t' || *stop == '\r' || *stop == '\n') {
        ++stop;
    }
    if (*stop != '\0') {
        return false; // 后面还有垃圾（"12abc"、"1.5"）
    }
    if (value < -1 || value > 1000000) {
        return false; // 越界：-1 是"全部"的下界
    }
    out = static_cast<int32_t>(value);
    return true;
}

bool parseIntegerEnv(const char *name, int32_t &out) {
    const char *text = std::getenv(name);
    if (text == 0 || text[0] == '\0') {
        return false;
    }
    if (!parseIntegerStrict(text, out)) {
        std::printf("[imoScene] 环境变量 %s=\"%s\" 不是合法整数，忽略（按“全部显示”处理）\n", name,
                    text);
        return false;
    }
    return true;
}

} // namespace imoScene
