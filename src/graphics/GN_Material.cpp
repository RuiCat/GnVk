// GN_Material.cpp — GPU 材质实例的 CPU 侧：描述校验、64 位内容键、逐字段比较与去重表。
//
// 键算法：FNV-1a 64 位，依次吃入管线键、绑定数、每个绑定的 binding/kind/stageFlags/资源地址。
// 去重规则：键相同 **且** 逐字段相等才算同一实例（与管线缓存"只比哈希"的已知缺口不同）。
#include <GN.h>
#include "GN_Material.h"

#include <cstdint>
#include <cstring>

namespace GN3d {

namespace {

const uint32_t kMaterialMaxBindings = 8;

void hashBytes(uint64_t &hash, const void *data, size_t size) {
    const unsigned char *bytes = (const unsigned char *)data;
    for(size_t i = 0; i < size; ++i) {
        hash ^= (uint64_t)bytes[i];
        hash *= 1099511628211ull;
    }
}

bool validKind(eMATERIAL_BINDING_KIND kind) {
    return kind == eMAT_BIND_UNIFORM_BUFFER || kind == eMAT_BIND_COMBINED_IMAGE_SAMPLER;
}

} // namespace

bool ValidateMaterialDesc(const sMATERIAL_DESC &desc) {
    if(desc.aPipelineKey == 0) return false;
    if(desc.aBindingCount > kMaterialMaxBindings) return false;
    uint32_t previousBinding = 0;
    bool hasPrevious = false;
    for(uint32_t i = 0; i < desc.aBindingCount; ++i) {
        const sMATERIAL_BINDING &binding = desc.aBindings[i];
        if(binding.aResource == 0) return false;
        if(!validKind(binding.aKind)) return false;
        if(binding.aStageFlags == 0) return false;
        if(hasPrevious && binding.aBinding <= previousBinding) return false;
        previousBinding = binding.aBinding;
        hasPrevious = true;
    }
    return true;
}

uint64_t ComputeMaterialKey(const sMATERIAL_DESC &desc) {
    uint64_t hash = 1469598103934665603ull;
    hashBytes(hash, &desc.aPipelineKey, sizeof(desc.aPipelineKey));
    hashBytes(hash, &desc.aBindingCount, sizeof(desc.aBindingCount));
    for(uint32_t i = 0; i < desc.aBindingCount && i < kMaterialMaxBindings; ++i) {
        const sMATERIAL_BINDING &binding = desc.aBindings[i];
        hashBytes(hash, &binding.aBinding, sizeof(binding.aBinding));
        const uint32_t kind = (uint32_t)binding.aKind;
        hashBytes(hash, &kind, sizeof(kind));
        hashBytes(hash, &binding.aStageFlags, sizeof(binding.aStageFlags));
        const uintptr_t resource = (uintptr_t)binding.aResource;
        hashBytes(hash, &resource, sizeof(resource));
    }
    return hash;
}

bool MaterialDescEqual(const sMATERIAL_DESC &a, const sMATERIAL_DESC &b) {
    if(!ValidateMaterialDesc(a) || !ValidateMaterialDesc(b)) return false;
    if(a.aPipelineKey != b.aPipelineKey) return false;
    if(a.aBindingCount != b.aBindingCount) return false;
    for(uint32_t i = 0; i < a.aBindingCount; ++i) {
        const sMATERIAL_BINDING &left = a.aBindings[i];
        const sMATERIAL_BINDING &right = b.aBindings[i];
        if(left.aBinding != right.aBinding) return false;
        if(left.aKind != right.aKind) return false;
        if(left.aStageFlags != right.aStageFlags) return false;
        if(left.aResource != right.aResource) return false;
    }
    return true;
}

uint32_t cMATERIAL_CACHE::GetOrCreate(const sMATERIAL_DESC &desc) {
    if(!ValidateMaterialDesc(desc)) return INVALID_HANDLE;
    const uint64_t key = ComputeMaterialKey(desc);
    for(size_t i = 0; i < aMaterials.size(); ++i) {
        if(aMaterials[i].aKey != key) continue;
        if(!MaterialDescEqual(aMaterials[i].aDesc, desc)) continue;
        return (uint32_t)i;
    }
    sMATERIAL_RES result;
    result.aKey = key;
    result.aDesc = desc;
    result.aDescriptorSet = 0;
    aMaterials.push_back(result);
    return (uint32_t)(aMaterials.size() - 1u);
}

const sMATERIAL_RES *cMATERIAL_CACHE::Get(uint32_t handle) const {
    if(handle >= aMaterials.size()) return 0;
    return &aMaterials[handle];
}

void cMATERIAL_CACHE::Release() {
    aMaterials.clear();
}

} // namespace GN3d
