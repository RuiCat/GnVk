#pragma once
#ifndef GN_MMD_BYTE_READER_H
#define GN_MMD_BYTE_READER_H
// ╔══════════════════════════════════════════════════════════╗
// ║  GN_mini — MMD 字节读取器（小端 / 全量边界检查）          ║
// ╚══════════════════════════════════════════════════════════╝
//
// 为什么不直接按指针强转：PMD/PMX 的段长度、索引宽度、字符串长度都来自文件自身，
// 截断或损坏的文件会让裸指针读取越界。本读取器的每个读操作都先校验剩余字节，
// 越界只置失败位并返回 false，调用方据此中止解析——不做任何越界访问。
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace mmd {

class ByteReader {
public:
    ByteReader(const uint8_t *data, size_t size) : mData(data), mSize(size) {}

    bool ok() const { return !mFailed; }
    size_t offset() const { return mOffset; }
    size_t remaining() const { return mSize - mOffset; }
    bool has(size_t count) const { return count <= remaining(); }

    bool readU8(uint8_t &value);
    bool readI8(int8_t &value);
    bool readU16(uint16_t &value);
    bool readI16(int16_t &value);
    bool readU32(uint32_t &value);
    bool readI32(int32_t &value);
    bool readF32(float &value);
    bool readBool(bool &value);

    bool readBytes(size_t count, std::string &value);
    bool readBytes(size_t count, std::vector<uint8_t> &value);
    bool skip(size_t count);

private:
    bool failRead();

    const uint8_t *mData;
    size_t mSize;
    size_t mOffset = 0;
    bool mFailed = false;
};

} // namespace mmd
#endif // GN_MMD_BYTE_READER_H
