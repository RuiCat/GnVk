// MMD 文件读取实现。
#include "mmdFile.h"

#include <fstream>

namespace mmd {

bool readFileBytes(const std::string &path, std::vector<uint8_t> &bytes, std::string *error) {
    std::ifstream stream(path.c_str(), std::ios::binary);
    if (!stream) {
        if (error) {
            *error = "无法打开文件: " + path;
        }
        return false;
    }
    stream.seekg(0, std::ios::end);
    const std::streamoff length = stream.tellg();
    if (length < 0) {
        if (error) {
            *error = "无法获取文件长度: " + path;
        }
        return false;
    }
    stream.seekg(0, std::ios::beg);
    bytes.assign(static_cast<size_t>(length), 0);
    if (length > 0) {
        stream.read(reinterpret_cast<char *>(&bytes[0]), static_cast<std::streamsize>(length));
        if (!stream) {
            bytes.clear();
            if (error) {
                *error = "读取文件失败: " + path;
            }
            return false;
        }
    }
    return true;
}

} // namespace mmd
