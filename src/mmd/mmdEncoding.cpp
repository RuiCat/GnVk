// MMD 文本编码转换实现：Shift-JIS → UTF-8 依赖平台的编码转换能力。
//   * Windows: MultiByteToWideChar(CP932…) + 本文件的 UTF-16→UTF-8（无外部依赖，不需要 iconv）
//   * Linux/macOS: 系统 iconv（与 GN_Sys 既有的 GB2312 转换同一依赖）
#include "mmdEncoding.h"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <iconv.h>
#endif

#include <vector>

namespace mmd {

namespace {

// 追加一个 Unicode 码点到 UTF-8 字符串。
void appendUtf8(std::string &out, uint32_t codePoint) {
    if (codePoint <= 0x7F) {
        out.push_back(static_cast<char>(codePoint));
    } else if (codePoint <= 0x7FF) {
        out.push_back(static_cast<char>(0xC0 | (codePoint >> 6)));
        out.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    } else if (codePoint <= 0xFFFF) {
        out.push_back(static_cast<char>(0xE0 | (codePoint >> 12)));
        out.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | (codePoint >> 18)));
        out.push_back(static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    }
}

// 用指定源编码调用 iconv；成功返回 true。
#if !defined(_WIN32)
bool convertWithIconv(const char *fromCode, const uint8_t *bytes, size_t size, std::string &out) {
    iconv_t handle = iconv_open("UTF-8", fromCode);
    if (handle == reinterpret_cast<iconv_t>(-1)) {
        return false;
    }
    std::vector<char> input(bytes, bytes + size);
    std::string result(size * 4 + 16, '\0');
    char *inPointer = input.empty() ? nullptr : &input[0];
    size_t inLeft = input.size();
    char *outPointer = &result[0];
    size_t outLeft = result.size();
    const size_t converted = iconv(handle, input.empty() ? nullptr : &inPointer, &inLeft, &outPointer, &outLeft);
    if (converted == static_cast<size_t>(-1)) {
        iconv_close(handle);
        return false;
    }
    iconv_close(handle);
    result.resize(result.size() - outLeft);
    out.swap(result);
    return true;
}
#else // _WIN32：用系统代码页转换（Windows 没有 iconv）
bool convertWithCodePage(unsigned int codePage, const uint8_t *bytes, size_t size, std::string &out) {
    if (size > 0x7FFFFFFFu) {
        return false;
    }
    const int wideLen = ::MultiByteToWideChar(codePage, 0, reinterpret_cast<const char *>(bytes),
                                             static_cast<int>(size), nullptr, 0);
    if (wideLen <= 0) {
        return false;
    }
    std::vector<wchar_t> wide(static_cast<size_t>(wideLen));
    if (::MultiByteToWideChar(codePage, 0, reinterpret_cast<const char *>(bytes),
                              static_cast<int>(size), &wide[0], wideLen) <= 0) {
        return false;
    }
    // Windows 的 wchar_t 就是 UTF-16LE，直接复用本文件的 UTF-16LE → UTF-8。
    out = utf16LeToUtf8(reinterpret_cast<const uint8_t *>(&wide[0]),
                        static_cast<size_t>(wideLen) * sizeof(wchar_t));
    return true;
}
#endif

} // namespace

std::string shiftJisToUtf8(const uint8_t *bytes, size_t size) {
    if (!bytes || size == 0) {
        return std::string();
    }
#if defined(_WIN32)
    // 932=CP932（Windows 的 Shift-JIS 实现）；余下两个是更严格/更老的变体，作兜底。
    static const unsigned int CODEPAGES[] = {932u, 943u, 20932u};
    for (size_t i = 0; i < sizeof(CODEPAGES) / sizeof(CODEPAGES[0]); ++i) {
        std::string converted;
        if (convertWithCodePage(CODEPAGES[i], bytes, size, converted)) {
            return converted;
        }
    }
#else
    static const char *const CODECS[] = {"SHIFT-JIS", "CP932", "SHIFT_JIS"};
    for (size_t i = 0; i < sizeof(CODECS) / sizeof(CODECS[0]); ++i) {
        std::string converted;
        if (convertWithIconv(CODECS[i], bytes, size, converted)) {
            return converted;
        }
    }
#endif
    return std::string(reinterpret_cast<const char *>(bytes), size);
}

std::string utf16LeToUtf8(const uint8_t *bytes, size_t size) {
    std::string out;
    if (!bytes) {
        return out;
    }
    size_t i = 0;
    while (i + 1 < size) {
        uint32_t unit = static_cast<uint32_t>(bytes[i]) | (static_cast<uint32_t>(bytes[i + 1]) << 8);
        i += 2;
        uint32_t codePoint = unit;
        if (unit >= 0xD800 && unit <= 0xDBFF) {
            if (i + 1 < size) {
                const uint32_t low = static_cast<uint32_t>(bytes[i]) | (static_cast<uint32_t>(bytes[i + 1]) << 8);
                if (low >= 0xDC00 && low <= 0xDFFF) {
                    codePoint = 0x10000 + ((unit - 0xD800) << 10) + (low - 0xDC00);
                    i += 2;
                } else {
                    codePoint = 0xFFFD;
                }
            } else {
                codePoint = 0xFFFD;
            }
        } else if (unit >= 0xDC00 && unit <= 0xDFFF) {
            codePoint = 0xFFFD;
        }
        appendUtf8(out, codePoint);
    }
    return out;
}

} // namespace mmd
