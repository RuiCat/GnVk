// vkBindingLayout.cpp — canonical 绑定表的实现与自检（P2a 骨架：只描述、不创建 Vulkan 对象）。
#include <GN.h>

#include "GN_RenderPipeline.h"
#include "vkBindingLayout.h"

#include <cstdio>
#include <cstring>

namespace GN3d {
namespace {

// set0：场景/帧（13 槽，§3.4.2）。binding 1/3 是材质纹理迁走后保留的白纹理槽。
const sVK_BINDING kSceneBindings[] = {
    {eVK_SET_SCENE, 0, eVK_DESC_UNIFORM_BUFFER, 1, eVK_STAGE_VERTEX | eVK_STAGE_FRAGMENT},
    {eVK_SET_SCENE, 1, eVK_DESC_COMBINED_IMAGE_SAMPLER, 1, eVK_STAGE_FRAGMENT},
    {eVK_SET_SCENE, 2, eVK_DESC_UNIFORM_BUFFER, 1, eVK_STAGE_FRAGMENT},
    {eVK_SET_SCENE, 3, eVK_DESC_COMBINED_IMAGE_SAMPLER, 1, eVK_STAGE_FRAGMENT},
    {eVK_SET_SCENE, 4, eVK_DESC_COMBINED_IMAGE_SAMPLER, 4, eVK_STAGE_FRAGMENT},
    {eVK_SET_SCENE, 5, eVK_DESC_COMBINED_IMAGE_SAMPLER, 1, eVK_STAGE_FRAGMENT},
    {eVK_SET_SCENE, 6, eVK_DESC_UNIFORM_BUFFER, 1, eVK_STAGE_FRAGMENT},
    {eVK_SET_SCENE, 7, eVK_DESC_UNIFORM_BUFFER, 1, eVK_STAGE_FRAGMENT},
    {eVK_SET_SCENE, 8, eVK_DESC_COMBINED_IMAGE_SAMPLER, 1, eVK_STAGE_FRAGMENT},
    {eVK_SET_SCENE, 9, eVK_DESC_COMBINED_IMAGE_SAMPLER, 1, eVK_STAGE_FRAGMENT},
    {eVK_SET_SCENE, 10, eVK_DESC_COMBINED_IMAGE_SAMPLER, 1, eVK_STAGE_FRAGMENT},
    {eVK_SET_SCENE, 11, eVK_DESC_COMBINED_IMAGE_SAMPLER, 1, eVK_STAGE_FRAGMENT},
    {eVK_SET_SCENE, 12, eVK_DESC_UNIFORM_BUFFER, 1, eVK_STAGE_VERTEX},
};

// set1：材质（8 槽：0=256B UBO，1..7=纹理，§3.4.3）。
const sVK_BINDING kMaterialBindings[] = {
    {eVK_SET_MATERIAL, 0, eVK_DESC_UNIFORM_BUFFER, 1, eVK_STAGE_VERTEX | eVK_STAGE_FRAGMENT},
    {eVK_SET_MATERIAL, 1, eVK_DESC_COMBINED_IMAGE_SAMPLER, 1, eVK_STAGE_FRAGMENT},
    {eVK_SET_MATERIAL, 2, eVK_DESC_COMBINED_IMAGE_SAMPLER, 1, eVK_STAGE_FRAGMENT},
    {eVK_SET_MATERIAL, 3, eVK_DESC_COMBINED_IMAGE_SAMPLER, 1, eVK_STAGE_FRAGMENT},
    {eVK_SET_MATERIAL, 4, eVK_DESC_COMBINED_IMAGE_SAMPLER, 1, eVK_STAGE_FRAGMENT},
    {eVK_SET_MATERIAL, 5, eVK_DESC_COMBINED_IMAGE_SAMPLER, 1, eVK_STAGE_FRAGMENT},
    {eVK_SET_MATERIAL, 6, eVK_DESC_COMBINED_IMAGE_SAMPLER, 1, eVK_STAGE_FRAGMENT},
    {eVK_SET_MATERIAL, 7, eVK_DESC_COMBINED_IMAGE_SAMPLER, 1, eVK_STAGE_FRAGMENT},
};

// set2：蒙皮二选一（UBO 8KB / SSBO，§3.4.4）。
const sVK_BINDING kSkinUboBinding[] = {
    {eVK_SET_SKIN, 0, eVK_DESC_UNIFORM_BUFFER, 1, eVK_STAGE_VERTEX},
};
const sVK_BINDING kSkinSsboBinding[] = {
    {eVK_SET_SKIN, 0, eVK_DESC_STORAGE_BUFFER, 1, eVK_STAGE_VERTEX},
};

// 阴影层例外：set0 只含 binding12（§3.4.4；canonical 13 槽 set 不能绑到子集布局上）。
const sVK_BINDING kShadowBindings[] = {
    {eVK_SET_SCENE, 12, eVK_DESC_UNIFORM_BUFFER, 1, eVK_STAGE_VERTEX},
};

template <typename T, size_t N> constexpr uint32_t countOf(const T (&)[N]) { return (uint32_t)N; }

void setReason(char *reason, size_t reasonSize, const char *message) {
    if(reason && reasonSize) snprintf(reason, reasonSize, "%s", message);
}

bool checkTable(const sVK_BINDING *table, uint32_t count, uint32_t set, const char *name,
                bool contiguous, char *reason, size_t reasonSize) {
    char detail[160];
    for(uint32_t i = 0; i < count; ++i) {
        const sVK_BINDING &b = table[i];
        if(b.aSet != set) {
            snprintf(detail, sizeof(detail), "%s entry %u belongs to set %u", name, i, b.aSet);
            setReason(reason, reasonSize, detail);
            return false;
        }
        if(contiguous && b.aBinding != i) {
            snprintf(detail, sizeof(detail), "%s bindings are not contiguous at index %u", name, i);
            setReason(reason, reasonSize, detail);
            return false;
        }
        if(b.aDescriptorType != eVK_DESC_UNIFORM_BUFFER && b.aDescriptorType != eVK_DESC_STORAGE_BUFFER &&
           b.aDescriptorType != eVK_DESC_COMBINED_IMAGE_SAMPLER) {
            snprintf(detail, sizeof(detail), "%s entry %u has unknown descriptor type %u", name, i, b.aDescriptorType);
            setReason(reason, reasonSize, detail);
            return false;
        }
        if(b.aArrayCount == 0u) {
            snprintf(detail, sizeof(detail), "%s entry %u has zero array count", name, i);
            setReason(reason, reasonSize, detail);
            return false;
        }
    }
    return true;
}

} // namespace

const sVK_BINDING *CanonicalSceneBindings(uint32_t *countOut) {
    if(countOut) *countOut = countOf(kSceneBindings);
    return kSceneBindings;
}

const sVK_BINDING *CanonicalMaterialBindings(uint32_t *countOut) {
    if(countOut) *countOut = countOf(kMaterialBindings);
    return kMaterialBindings;
}

const sVK_BINDING *CanonicalSkinBinding(uint32_t skinData) {
    if(skinData == (uint32_t)eSKIN_UBO) return kSkinUboBinding;
    if(skinData == (uint32_t)eSKIN_SSBO) return kSkinSsboBinding;
    return 0;
}

const sVK_BINDING *ShadowBindings(uint32_t *countOut) {
    if(countOut) *countOut = countOf(kShadowBindings);
    return kShadowBindings;
}

uint32_t CanonicalSetBindingCount(eVK_BINDING_SET set, uint32_t skinData) {
    if(set == eVK_SET_SCENE) return countOf(kSceneBindings);
    if(set == eVK_SET_MATERIAL) return countOf(kMaterialBindings);
    if(set == eVK_SET_SKIN) return CanonicalSkinBinding(skinData) ? 1u : 0u;
    return 0u;
}

const char *BindingSetName(eVK_BINDING_SET set) {
    switch(set) {
        case eVK_SET_SCENE: return "scene";
        case eVK_SET_MATERIAL: return "material";
        case eVK_SET_SKIN: return "skin";
        default: return "invalid";
    }
}

bool ValidateCanonicalBindingTable(char *reason, size_t reasonSize) {
    if(reason && reasonSize) reason[0] = 0;
    static_assert(kVkBindingLayoutVersion == kBindingLayoutVersion, "binding layout version drift");
    static_assert(countOf(kShadowBindings) == 1u, "shadow layout must have exactly one binding");
    if(countOf(kSceneBindings) != 13u) { setReason(reason, reasonSize, "canonical set0 must have 13 bindings"); return false; }
    if(countOf(kMaterialBindings) != 8u) { setReason(reason, reasonSize, "canonical set1 must have 8 bindings"); return false; }
    if(!checkTable(kSceneBindings, countOf(kSceneBindings), eVK_SET_SCENE, "set0", true, reason, reasonSize)) return false;
    if(!checkTable(kMaterialBindings, countOf(kMaterialBindings), eVK_SET_MATERIAL, "set1", true, reason, reasonSize)) return false;
    if(!checkTable(kSkinUboBinding, 1u, eVK_SET_SKIN, "set2/ubo", true, reason, reasonSize)) return false;
    if(!checkTable(kSkinSsboBinding, 1u, eVK_SET_SKIN, "set2/ssbo", true, reason, reasonSize)) return false;
    if(!checkTable(kShadowBindings, 1u, eVK_SET_SCENE, "shadow", false, reason, reasonSize)) return false;
    const sVK_BINDING &shadow = kShadowBindings[0];
    const sVK_BINDING &scene12 = kSceneBindings[kVkShadowBinding];
    if(shadow.aBinding != scene12.aBinding || shadow.aDescriptorType != scene12.aDescriptorType ||
       shadow.aArrayCount != scene12.aArrayCount || shadow.aStageFlags != scene12.aStageFlags) {
        setReason(reason, reasonSize, "shadow binding 12 disagrees with canonical set0 binding 12");
        return false;
    }
    return true;
}

} // namespace GN3d
