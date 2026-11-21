// GN_Spirv.cpp — SPIR-V 最小结构校验实现。
#include "GN_Spirv.h"

#include <cstdio>
#include <cstring>

namespace GN3d {
namespace {

void setReason(char *reason, size_t reasonSize, const char *message) {
    if(reason && reasonSize) snprintf(reason, reasonSize, "%s", message);
}

uint32_t readWord(const unsigned char *bytes, size_t index) {
    const unsigned char *p = bytes + index * 4u;
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

} // namespace

bool validateSpirvModule(const void *data, size_t size, char *reason, size_t reasonSize) {
    if(reason && reasonSize) reason[0] = 0;
    if(!data || size == 0) { setReason(reason, reasonSize, "spir-v module is empty"); return false; }
    if((size % 4u) != 0u) { setReason(reason, reasonSize, "spir-v size is not 4-byte aligned"); return false; }
    if(size < 20u) { setReason(reason, reasonSize, "spir-v module is shorter than the header"); return false; }
    const unsigned char *bytes = (const unsigned char *)data;
    if(readWord(bytes, 0) != kSpirvMagic) { setReason(reason, reasonSize, "spir-v magic mismatch"); return false; }
    const uint32_t version = readWord(bytes, 1);
    const uint32_t major = (version >> 16) & 0xffu;
    const uint32_t minor = (version >> 8) & 0xffu;
    if(major != 1u || minor > 6u) { setReason(reason, reasonSize, "unsupported spir-v version"); return false; }
    if(readWord(bytes, 3) == 0u) { setReason(reason, reasonSize, "spir-v bound must be positive"); return false; }
    return true;
}

} // namespace GN3d
