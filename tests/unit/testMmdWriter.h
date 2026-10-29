#pragma once
#ifndef GN_TEST_MMD_WRITER_H
#define GN_TEST_MMD_WRITER_H
// 单测辅助：小端字节写入器，用于按文件格式构造合成 PMD / PMX 输入。
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace mmdTest {

class ByteWriter {
public:
    void u8(uint32_t value) { mData.push_back(static_cast<uint8_t>(value & 0xFF)); }

    void u16(uint32_t value) {
        mData.push_back(static_cast<uint8_t>(value & 0xFF));
        mData.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    }

    void u32(uint32_t value) {
        for (int i = 0; i < 4; ++i) {
            mData.push_back(static_cast<uint8_t>((value >> (8 * i)) & 0xFF));
        }
    }

    void i32(int32_t value) { u32(static_cast<uint32_t>(value)); }

    void f32(float value) {
        uint32_t bits = 0;
        std::memcpy(&bits, &value, sizeof(bits));
        u32(bits);
    }

    void f3(float a, float b, float c) {
        f32(a);
        f32(b);
        f32(c);
    }

    void f4(float a, float b, float c, float d) {
        f32(a);
        f32(b);
        f32(c);
        f32(d);
    }

    void bytes(const void *data, size_t size) {
        const uint8_t *first = static_cast<const uint8_t *>(data);
        mData.insert(mData.end(), first, first + size);
    }

    void text(const std::string &value) { bytes(value.data(), value.size()); }

    // PMD 定长字符串：写入内容并补 NUL；内容超长时只保留前 width 字节。
    void fixedText(const std::string &value, size_t width) {
        const size_t copy = value.size() < width ? value.size() : width;
        bytes(value.data(), copy);
        mData.insert(mData.end(), width - copy, 0);
    }

    // PMX 字符串：int32 字节长度 + 原始字节（encoding=1 UTF-8）。
    void prefixedText(const std::string &value) {
        i32(static_cast<int32_t>(value.size()));
        text(value);
    }

    // PMX 字符串：int32 字节长度 + UTF-16LE 码元（encoding=0）。
    void utf16Text(const std::u16string &value) {
        i32(static_cast<int32_t>(value.size() * 2));
        for (size_t i = 0; i < value.size(); ++i) {
            u16(value[i]);
        }
    }

    void zeros(size_t count) { mData.insert(mData.end(), count, 0); }

    const std::vector<uint8_t> &data() const { return mData; }
    size_t size() const { return mData.size(); }

private:
    std::vector<uint8_t> mData;
};

} // namespace mmdTest
#endif // GN_TEST_MMD_WRITER_H
