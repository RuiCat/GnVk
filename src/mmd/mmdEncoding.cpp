// MMD 文本编码转换实现：Shift-JIS 走系统 iconv（与 GN_Sys 既有 GB2312 转换同一依赖）。
#include "mmdEncoding.h"

#include <iconv.h>

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

} // namespace

std::string shiftJisToUtf8(const uint8_t *bytes, size_t size) {
    if (!bytes || size == 0) {
        return std::string();
    }
    static const char *const CODECS[] = {"SHIFT-JIS", "CP932", "SHIFT_JIS"};
    for (size_t i = 0; i < sizeof(CODECS) / sizeof(CODECS[0]); ++i) {
        std::string converted;
        if (convertWithIconv(CODECS[i], bytes, size, converted)) {
            return converted;
        }
    }
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
