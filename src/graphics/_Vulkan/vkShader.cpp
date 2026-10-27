// 着色器模块缓存：按 SPIR-V 内容哈希去重，模块生命周期归缓存独占（Release/析构统一销毁）。
#include "GN_Shader.h"

#include <cstdint>
#include <cstring>

namespace GN3d {

cSHADER_CACHE::cSHADER_CACHE() : aDevice(VK_NULL_HANDLE), aHits(0), aMisses(0) {}

// 析构即 Release：调用方必须在销毁 VkDevice 之前销毁缓存（或提前显式 Release）。
cSHADER_CACHE::~cSHADER_CACHE() { Release(); }

void cSHADER_CACHE::Init(VkDevice device) {
    Release();
    aDevice = device;
    aHits = 0;
    aMisses = 0;
}

void cSHADER_CACHE::Release() {
    if (aDevice != VK_NULL_HANDLE) {
        for (size_t i = 0; i < aModules.size(); ++i) {
            if (aModules[i].aModule != VK_NULL_HANDLE) {
                vkDestroyShaderModule(aDevice, aModules[i].aModule, 0);
            }
        }
    }
    aModules.clear();
    aDevice = VK_NULL_HANDLE;
    aHits = 0;
    aMisses = 0;
}

VkShaderModule cSHADER_CACHE::GetOrCreate(const void *spirv, size_t bytes, bool *created) {
    if (created) *created = false;
    if (aDevice == VK_NULL_HANDLE) return VK_NULL_HANDLE;
    if (!IsSpirv(spirv, bytes)) return VK_NULL_HANDLE;
    if (bytes % sizeof(uint32_t) != 0) return VK_NULL_HANDLE; // SPIR-V 以 32 位字为单位

    const uint64_t key = HashSpirv(spirv, bytes);
    for (size_t i = 0; i < aModules.size(); ++i) {
        if (aModules[i].aKey == key) {
            ++aHits;
            if (created) *created = false;
            return aModules[i].aModule;
        }
    }

    ++aMisses;
    // pCode 要求 4 字节对齐；调用方指针不满足时复制到对齐缓冲再创建。
    const uint32_t *code = 0;
    std::vector<uint32_t> alignedCopy;
    const bool aligned = (reinterpret_cast<std::uintptr_t>(spirv) % sizeof(uint32_t)) == 0;
    if (aligned) {
        code = static_cast<const uint32_t *>(spirv);
    } else {
        alignedCopy.resize(bytes / sizeof(uint32_t));
        std::memcpy(&alignedCopy[0], spirv, bytes);
        code = &alignedCopy[0];
    }

    VkShaderModuleCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.codeSize = bytes;
    info.pCode = code;

    VkShaderModule module = VK_NULL_HANDLE;
    if (vkCreateShaderModule(aDevice, &info, 0, &module) != VK_SUCCESS) {
        return VK_NULL_HANDLE;
    }

    sMODULE entry;
    entry.aKey = key;
    entry.aModule = module;
    aModules.push_back(entry);
    if (created) *created = true;
    return module;
}

} // namespace GN3d
