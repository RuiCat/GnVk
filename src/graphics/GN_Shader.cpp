// SPIR-V 文件读取、内容哈希（FNV-1a 64）与魔数校验：纯 CPU，不触碰 Vulkan 设备。
#include "GN_Shader.h"

#include <cstdio>

namespace GN3d {
namespace {

const uint64_t FNV_OFFSET_BASIS = 14695981039346656037ULL;
const uint64_t FNV_PRIME = 1099511628211ULL;

} // namespace

bool LoadSpirvFile(const char *path, std::vector<char> &out) {
    out.clear();
    if (path == 0 || path[0] == '\0') return false;

    FILE *file = std::fopen(path, "rb");
    if (file == 0) return false;

    bool ok = false;
    // 目录在 Linux 上也能 fopen 成功，但 fseek/ftell 得到的长度与 fread 实际字节数对不上，
    // 因此一律以「读到的字节数 == 文件长度」作为成功判据。
    if (std::fseek(file, 0, SEEK_END) == 0) {
        const long length = std::ftell(file);
        if (length > 0 && std::fseek(file, 0, SEEK_SET) == 0) {
            out.resize(static_cast<size_t>(length));
            const size_t got = std::fread(&out[0], 1, out.size(), file);
            ok = (got == out.size());
        }
    }
    std::fclose(file);

    if (!ok) out.clear();
    return ok;
}

uint64_t HashSpirv(const void *data, size_t bytes) {
    uint64_t hash = FNV_OFFSET_BASIS;
    const unsigned char *cursor = static_cast<const unsigned char *>(data);
    if (cursor == 0) return hash; // 空输入（含空指针）稳定返回偏移基，不崩溃
    for (size_t i = 0; i < bytes; ++i) {
        hash ^= static_cast<uint64_t>(cursor[i]);
        hash *= FNV_PRIME;
    }
    return hash;
}

bool IsSpirv(const void *data, size_t bytes) {
    if (data == 0 || bytes < 4) return false;
    // 魔数 0x07230203 的小端字节序；按字节比较，避免依赖宿主端序。
    const unsigned char *magic = static_cast<const unsigned char *>(data);
    return magic[0] == 0x03 && magic[1] == 0x02 && magic[2] == 0x23 && magic[3] == 0x07;
}

} // namespace GN3d
