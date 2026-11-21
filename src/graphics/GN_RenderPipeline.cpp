// GN_RenderPipeline.cpp — 多层渲染 CPU 契约的实现：层名、掩码/入口解析、
// 程序与渲染项校验、层内稳定排序。不依赖 Vulkan，可无 GPU 单测。
#include <GN.h>
#include "GN_RenderPipeline.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace GN3d {
namespace {

void setReason(char *reason, size_t size, const char *text) {
    if (!reason || size == 0 || !text) return;
    std::snprintf(reason, size, "%s", text);
}

bool hasVert(const sSHADER_ENTRY &entry) { return entry.aVertSpv != 0 && entry.aVertSize > 0; }
bool hasFrag(const sSHADER_ENTRY &entry) { return entry.aFragSpv != 0 && entry.aFragSize > 0; }

bool isKnownVertexFormat(eVERTEX_FORMAT format) {
    return format == eVF_STATIC || format == eVF_SKINNED;
}

bool isKnownLayer(eRENDER_LAYER layer) {
    return layer >= eLAYER_SHADOW && layer < eLAYER_COUNT;
}

bool validMask(uint32_t mask) {
    const uint32_t all = (1u << (uint32_t)eLAYER_COUNT) - 1u;
    return (mask & ~all) == 0;
}

bool validStateOverride(int32_t value, int32_t minValue, int32_t maxValue) {
    return value >= minValue && value <= maxValue;
}

bool finiteMatrix(const GNg::sMAT44 &matrix) {
    for (int i = 0; i < 16; ++i) {
        if (!std::isfinite(matrix.m[i])) return false;
    }
    return true;
}

uint32_t programIdFor(const sRENDER_ITEM &item, eRENDER_LAYER layer) {
    const sSHADER_PROGRAM *program = item.aLayerProgram[layer];
    if (!program && item.aMaterial) program = item.aMaterial->aProgram;
    return program ? program->aId : 0xffffffffu;
}

float viewDepth(const GNg::sMAT44 &view, const sRENDER_ITEM &item) {
    const float x = item.aModelMatrix.m[12];
    const float y = item.aModelMatrix.m[13];
    const float z = item.aModelMatrix.m[14];
    return view.m[2] * x + view.m[6] * y + view.m[10] * z + view.m[14];
}

bool entryValidForLayer(const sSHADER_ENTRY &entry, eRENDER_LAYER layer, char *reason,
                        size_t reasonSize) {
    if (!hasVert(entry)) {
        setReason(reason, reasonSize, "layer entry missing vertex SPIR-V");
        return false;
    }
    if (entry.aVertSize % 4u != 0u) {
        setReason(reason, reasonSize, "vertex SPIR-V size must be multiple of 4");
        return false;
    }
    if (layer != eLAYER_SHADOW) {
        if (!hasFrag(entry)) {
            setReason(reason, reasonSize, "non-shadow layer entry missing fragment SPIR-V");
            return false;
        }
        if (entry.aFragSize % 4u != 0u) {
            setReason(reason, reasonSize, "fragment SPIR-V size must be multiple of 4");
            return false;
        }
    } else if (hasFrag(entry) && entry.aFragSize % 4u != 0u) {
        setReason(reason, reasonSize, "fragment SPIR-V size must be multiple of 4");
        return false;
    }
    if (entry.aPushConstantSize > kMaxPushConstantBytes ||
        entry.aPushConstantSize % 4u != 0u) {
        setReason(reason, reasonSize, "push constant size must be <=128 and multiple of 4");
        return false;
    }
    if (entry.aSpecCount > kMaxSpecConstants) {
        setReason(reason, reasonSize, "too many specialization constants");
        return false;
    }
    if (!validStateOverride(entry.aBlend, -1, 3) ||
        !validStateOverride(entry.aDepthTest, -1, 1) ||
        !validStateOverride(entry.aDepthWrite, -1, 1) ||
        !validStateOverride(entry.aCullBack, -1, 1) ||
        !validStateOverride(entry.aTopology, -1, 4)) {
        setReason(reason, reasonSize, "state override out of range");
        return false;
    }
    return true;
}

} // namespace

const char *RenderLayerName(eRENDER_LAYER layer) {
    switch (layer) {
    case eLAYER_SHADOW: return "shadow";
    case eLAYER_GBUFFER: return "gbuffer";
    case eLAYER_FORWARD_OPAQUE: return "forward-opaque";
    case eLAYER_OUTLINE: return "outline";
    case eLAYER_FORWARD_ALPHA: return "forward-alpha";
    case eLAYER_EFFECT: return "effect";
    case eLAYER_OVERLAY: return "overlay";
    default: return "invalid";
    }
}

uint32_t ResolveLayerMask(const sRENDER_ITEM &item) {
    if (!item.aMaterial || !item.aMaterial->aProgram) return 0;
    if (item.aLayerMask != 0) return item.aLayerMask;
    if (item.aMaterial->aLayerMask != 0) return item.aMaterial->aLayerMask;
    return item.aMaterial->aProgram->aDesc.aLayerMask;
}

const sSHADER_ENTRY *ResolveEntry(const sRENDER_ITEM &item, eRENDER_LAYER layer,
                                  const sSHADER_PROGRAM *implicitShadowProgram) {
    if (!isKnownLayer(layer) || !item.aMaterial) return 0;
    const sSHADER_PROGRAM *program = item.aLayerProgram[layer];
    if (!program) program = item.aMaterial->aProgram;
    if (!program) return 0;

    const bool implicitShadow =
        layer == eLAYER_SHADOW && (program->aDesc.aFlags & ePROG_IMPLICIT_SHADOW) != 0u;
    if ((program->aDesc.aLayerMask & (1u << (uint32_t)layer)) == 0u && !implicitShadow) {
        return 0;
    }

    const sSHADER_ENTRY &entry = program->aDesc.aEntries[layer];
    if (hasVert(entry)) return &entry;

    if (implicitShadow && implicitShadowProgram) {
        if ((implicitShadowProgram->aDesc.aLayerMask & (1u << (uint32_t)eLAYER_SHADOW)) == 0u) {
            return 0;
        }
        const sSHADER_ENTRY &fallback = implicitShadowProgram->aDesc.aEntries[eLAYER_SHADOW];
        return hasVert(fallback) ? &fallback : 0;
    }
    return 0;
}

bool ValidateShaderProgramDesc(const sSHADER_PROGRAM_DESC &desc, char *reason, size_t reasonSize) {
    if (!desc.aName || desc.aName[0] == '\0') {
        setReason(reason, reasonSize, "program name is empty");
        return false;
    }
    if (std::strlen(desc.aName) > 63u) {
        setReason(reason, reasonSize, "program name exceeds 63 chars");
        return false;
    }
    if (desc.aLayerMask == 0u || !validMask(desc.aLayerMask)) {
        setReason(reason, reasonSize, "layer mask is empty or has unknown bits");
        return false;
    }
    if (desc.aLayoutVersion != kBindingLayoutVersion) {
        setReason(reason, reasonSize, "binding layout version mismatch");
        return false;
    }
    if (!isKnownVertexFormat(desc.aVertexFormat)) {
        setReason(reason, reasonSize, "unknown vertex format");
        return false;
    }
    if (desc.aVertexFormat == eVF_SKINNED) {
        if (desc.aSkinData != eSKIN_UBO && desc.aSkinData != eSKIN_SSBO) {
            setReason(reason, reasonSize, "skinned program requires UBO or SSBO skin data");
            return false;
        }
    } else if (desc.aSkinData != eSKIN_NONE) {
        setReason(reason, reasonSize, "static program must not carry skin data");
        return false;
    }
    if ((desc.aFlags & ~(uint32_t)ePROG_IMPLICIT_SHADOW) != 0u) {
        setReason(reason, reasonSize, "unknown program flags");
        return false;
    }
    if ((desc.aFlags & ePROG_IMPLICIT_SHADOW) != 0u &&
        (desc.aLayerMask & (1u << (uint32_t)eLAYER_SHADOW)) != 0u) {
        setReason(reason, reasonSize, "implicit shadow flag conflicts with shadow entry");
        return false;
    }

    for (uint32_t i = 0; i < (uint32_t)eLAYER_COUNT; ++i) {
        const bool bit = (desc.aLayerMask & (1u << i)) != 0u;
        const bool present = hasVert(desc.aEntries[i]);
        if (bit != present) {
            setReason(reason, reasonSize, "layer mask and entries disagree");
            return false;
        }
        if (!present) continue;
        if (!entryValidForLayer(desc.aEntries[i], (eRENDER_LAYER)i, reason, reasonSize)) {
            return false;
        }
    }
    return true;
}

bool ValidateRenderItem(const sRENDER_ITEM &item, char *reason, size_t reasonSize) {
    if (!item.aMaterial || !item.aMaterial->aProgram) {
        setReason(reason, reasonSize, "render item material/program is null");
        return false;
    }
    if (item.aVertexCount == 0u || !item.aVertices) {
        setReason(reason, reasonSize, "render item has no vertices");
        return false;
    }
    if (item.aIndexCount > 0u && !item.aIndices) {
        setReason(reason, reasonSize, "render item has indices but null index pointer");
        return false;
    }
    if (!finiteMatrix(item.aModelMatrix)) {
        setReason(reason, reasonSize, "model matrix has non-finite values");
        return false;
    }
    if (item.aLayerMask != 0u && !validMask(item.aLayerMask)) {
        setReason(reason, reasonSize, "render item layer mask has unknown bits");
        return false;
    }
    if (!ValidateShaderProgramDesc(item.aMaterial->aProgram->aDesc, reason, reasonSize)) {
        return false;
    }
    for (uint32_t i = 0; i < (uint32_t)eLAYER_COUNT; ++i) {
        const sSHADER_PROGRAM *program = item.aLayerProgram[i];
        if (program && !ValidateShaderProgramDesc(program->aDesc, reason, reasonSize)) {
            return false;
        }
    }
    return true;
}

void SortRenderItems(sRENDER_ITEM *items, uint32_t count, eRENDER_LAYER layer,
                     const GNg::sMAT44 &view) {
    if (!items || count < 2u || !isKnownLayer(layer)) return;
    const bool backToFront = (layer == eLAYER_FORWARD_ALPHA);
    std::stable_sort(items, items + count,
                     [layer, &view, backToFront](const sRENDER_ITEM &a, const sRENDER_ITEM &b) {
                         const uint32_t idA = programIdFor(a, layer);
                         const uint32_t idB = programIdFor(b, layer);
                         if (idA != idB) return idA < idB;
                         const float zA = viewDepth(view, a);
                         const float zB = viewDepth(view, b);
                         if (std::fabs(zA - zB) > 1.0e-6f) {
                             return backToFront ? (zA > zB) : (zA < zB);
                         }
                         return a.aSortKey < b.aSortKey;
                     });
}

} // namespace GN3d
