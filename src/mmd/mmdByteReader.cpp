// MMD 字节读取器实现：所有读操作先做剩余长度校验，越界置失败位。
#include "mmdByteReader.h"

#include <cstring>

namespace mmd {

bool ByteReader::failRead() {
    mFailed = true;
    return false;
}

bool ByteReader::readU8(uint8_t &value) {
    if (!has(1)) {
        return failRead();
    }
    value = mData[mOffset++];
    return true;
}

bool ByteReader::readI8(int8_t &value) {
    uint8_t raw = 0;
    if (!readU8(raw)) {
        return false;
    }
    value = static_cast<int8_t>(raw);
    return true;
}

bool ByteReader::readU16(uint16_t &value) {
    if (!has(2)) {
        return failRead();
    }
    value = static_cast<uint16_t>(mData[mOffset] | (mData[mOffset + 1] << 8));
    mOffset += 2;
    return true;
}

bool ByteReader::readI16(int16_t &value) {
    uint16_t raw = 0;
    if (!readU16(raw)) {
        return false;
    }
    value = static_cast<int16_t>(raw);
    return true;
}

bool ByteReader::readU32(uint32_t &value) {
    if (!has(4)) {
        return failRead();
    }
    value = static_cast<uint32_t>(mData[mOffset]) | (static_cast<uint32_t>(mData[mOffset + 1]) << 8) |
            (static_cast<uint32_t>(mData[mOffset + 2]) << 16) |
            (static_cast<uint32_t>(mData[mOffset + 3]) << 24);
    mOffset += 4;
    return true;
}

bool ByteReader::readI32(int32_t &value) {
    uint32_t raw = 0;
    if (!readU32(raw)) {
        return false;
    }
    value = static_cast<int32_t>(raw);
    return true;
}

bool ByteReader::readF32(float &value) {
    uint32_t bits = 0;
    if (!readU32(bits)) {
        return false;
    }
    std::memcpy(&value, &bits, sizeof(value));
    return true;
}

bool ByteReader::readBool(bool &value) {
    uint8_t raw = 0;
    if (!readU8(raw)) {
        return false;
    }
    value = raw != 0;
    return true;
}

bool ByteReader::readBytes(size_t count, std::string &value) {
    if (!has(count)) {
        return failRead();
    }
    value.assign(reinterpret_cast<const char *>(mData + mOffset), count);
    mOffset += count;
    return true;
}

bool ByteReader::readBytes(size_t count, std::vector<uint8_t> &value) {
    if (!has(count)) {
        return failRead();
    }
    value.assign(mData + mOffset, mData + mOffset + count);
    mOffset += count;
    return true;
}

bool ByteReader::skip(size_t count) {
    if (!has(count)) {
        return failRead();
    }
    mOffset += count;
    return true;
}

} // namespace mmd
